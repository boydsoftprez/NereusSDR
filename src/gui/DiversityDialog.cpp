// src/gui/DiversityDialog.cpp
// =================================================================
// src/gui/DiversityDialog.cpp  (NereusSDR)
// =================================================================
//
//  Copyright (C) 2026 J.J. Boyd (KG4VCF)
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27 — Created for Phase 3F Sub-Epic G Task 4 (simplified,
//                bench-minimum). NereusSDR-original code. J.J. Boyd
//                (KG4VCF), with AI-assisted implementation via
//                Anthropic Claude Code.
//   2026-05-27 — Sub-Epic G Task 12: embed DiversityRadarWidget for
//                polar lobe display; dialog re-titled "Diversity".
// =================================================================

#include "gui/DiversityDialog.h"
#include "StyleConstants.h"
#include "gui/widgets/DiversityRadarWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include <cmath>

namespace NereusSDR {

DiversityDialog::DiversityDialog(RadioModel* radioModel, QWidget* parent)
    : QDialog(parent)
    , m_radioModel(radioModel)
{
    setWindowTitle(QStringLiteral("Diversity"));
    setStyleSheet(QStringLiteral("background: %1; color: %2;")
                      .arg(Style::kPanelBg, Style::kTextPrimary));
    setFixedWidth(460);

    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(14, 14, 14, 14);

    m_enableBox = new QCheckBox(
        QStringLiteral("Enable diversity on Slice A (DDC0 + DDC1 sync pair)"),
        this);
    connect(m_enableBox, &QCheckBox::toggled,
            this, &DiversityDialog::onEnableToggled);
    main->addWidget(m_enableBox);

    auto* phaseGroup = new QGroupBox(QStringLiteral("Phase (degrees)"), this);
    auto* phaseLayout = new QHBoxLayout(phaseGroup);
    m_phaseSlider = new QSlider(Qt::Horizontal, phaseGroup);
    // 0.1-degree precision: slider 0..3600 maps to 0..360 degrees.
    m_phaseSlider->setRange(0, 3600);
    connect(m_phaseSlider, &QSlider::valueChanged,
            this, &DiversityDialog::onPhaseChanged);
    m_phaseLabel = new QLabel(QStringLiteral("0.0"), phaseGroup);
    m_phaseLabel->setMinimumWidth(50);
    phaseLayout->addWidget(m_phaseSlider, 1);
    phaseLayout->addWidget(m_phaseLabel);
    main->addWidget(phaseGroup);

    auto* gainGroup = new QGroupBox(QStringLiteral("Gain (dB)"), this);
    auto* gainLayout = new QHBoxLayout(gainGroup);
    m_gainSlider = new QSlider(Qt::Horizontal, gainGroup);
    // 0.1-dB precision: slider -200..200 maps to -20.0..+20.0 dB.
    m_gainSlider->setRange(-200, 200);
    connect(m_gainSlider, &QSlider::valueChanged,
            this, &DiversityDialog::onGainChanged);
    m_gainLabel = new QLabel(QStringLiteral("0.0"), gainGroup);
    m_gainLabel->setMinimumWidth(50);
    gainLayout->addWidget(m_gainSlider, 1);
    gainLayout->addWidget(m_gainLabel);
    main->addWidget(gainGroup);

    // Phase 3F Sub-Epic G Task 12: polar sensitivity radar.
    auto* radarGroup = new QGroupBox(QStringLiteral("Sensitivity pattern"), this);
    auto* radarLayout = new QVBoxLayout(radarGroup);
    m_radar = new DiversityRadarWidget(radarGroup);
    m_radar->setMinimumSize(240, 240);
    radarLayout->addWidget(m_radar);
    main->addWidget(radarGroup);

    // Mouse drag on the radar emits phaseAdjusted(radians); convert to
    // degrees and write back to SliceModel.  refreshFromSlice() then
    // echoes the value into the slider (signal-blocked) and the lobe
    // re-renders.
    connect(m_radar, &DiversityRadarWidget::phaseAdjusted,
            this, [this](double rad) {
                if (auto* s = sliceA()) {
                    const double deg = rad * 180.0 / M_PI;
                    const double normDeg = std::fmod(deg + 360.0, 360.0);
                    s->setDiversityPhaseDeg(normDeg);
                }
            });

    m_statusLabel = new QLabel(QStringLiteral("Status: idle"), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: %1;")
                                     .arg(Style::kTextSecondary));
    main->addWidget(m_statusLabel);

    auto* footer = new QHBoxLayout();
    footer->addStretch(1);
    auto* closeBtn = new QPushButton(QStringLiteral("Close"), this);
    closeBtn->setStyleSheet(Style::buttonBaseStyle());
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    footer->addWidget(closeBtn);
    main->addLayout(footer);

    // Initial state from Slice A.
    refreshFromSlice();

    // React to external SliceModel changes (e.g. AppSettings load,
    // RadioModel T13 round-trip, future RadarWidget edits).
    if (auto* s = sliceA()) {
        connect(s, &SliceModel::diversityEnabledChanged,
                this, &DiversityDialog::refreshFromSlice);
        connect(s, &SliceModel::diversityPhaseDegChanged,
                this, &DiversityDialog::refreshFromSlice);
        connect(s, &SliceModel::diversityGainDbChanged,
                this, &DiversityDialog::refreshFromSlice);
        connect(s, &SliceModel::frequencyChanged,
                this, &DiversityDialog::refreshFromSlice);
    }
}

DiversityDialog::~DiversityDialog() = default;

SliceModel* DiversityDialog::sliceA() const
{
    if (!m_radioModel) { return nullptr; }
    const auto slices = m_radioModel->slices();
    return slices.isEmpty() ? nullptr : slices.first();
}

void DiversityDialog::onEnableToggled(bool on)
{
    if (auto* s = sliceA()) {
        s->setDiversityEnabled(on);
        m_statusLabel->setText(on
            ? QStringLiteral("Status: engaged (DDC0+DDC1 sync; BPF auto-bypass)")
            : QStringLiteral("Status: idle"));
    }
}

void DiversityDialog::onPhaseChanged(int sliderValue)
{
    const double deg = sliderValue / 10.0;
    m_phaseLabel->setText(QString::number(deg, 'f', 1));
    if (auto* s = sliceA()) {
        s->setDiversityPhaseDeg(deg);
    }
}

void DiversityDialog::onGainChanged(int sliderValue)
{
    const double db = sliderValue / 10.0;
    m_gainLabel->setText(QString::number(db, 'f', 1));
    if (auto* s = sliceA()) {
        s->setDiversityGainDb(db);
    }
}

void DiversityDialog::refreshFromSlice()
{
    auto* s = sliceA();
    if (!s) { return; }
    QSignalBlocker bEn(m_enableBox);
    QSignalBlocker bPh(m_phaseSlider);
    QSignalBlocker bGn(m_gainSlider);
    m_enableBox->setChecked(s->diversityEnabled());
    m_phaseSlider->setValue(static_cast<int>(s->diversityPhaseDeg() * 10.0));
    m_gainSlider->setValue(static_cast<int>(s->diversityGainDb() * 10.0));
    m_phaseLabel->setText(QString::number(s->diversityPhaseDeg(), 'f', 1));
    m_gainLabel->setText(QString::number(s->diversityGainDb(), 'f', 1));
    m_statusLabel->setText(s->diversityEnabled()
        ? QStringLiteral("Status: engaged (DDC0+DDC1 sync; BPF auto-bypass)")
        : QStringLiteral("Status: idle"));

    // Phase 3F Sub-Epic G Task 12: push slice phase/gain/VFO into radar
    // for live lobe redraw.  Gain is stored in dB on the slice; the
    // radar widget takes a linear ratio.
    if (m_radar) {
        m_radar->setPhase(s->diversityPhaseDeg() * M_PI / 180.0);
        const double gainLin = std::pow(10.0, s->diversityGainDb() / 20.0);
        m_radar->setGain(gainLin);
        m_radar->setVfoFreqMhz(s->frequency() / 1e6);
    }
}

} // namespace NereusSDR
