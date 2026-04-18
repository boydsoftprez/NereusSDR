// =================================================================
// src/gui/setup/hardware/BandwidthMonitorTab.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/ChannelMaster/bandwidth_monitor.h, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

/*  bandwidth_monitor.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2025 Richard Samphire, MW0LGE

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

// Developer note (NereusSDR Phase 3I): wires only the static controls
// (throttle threshold, auto-pause toggle); live feed from P1RadioConnection's
// bandwidth monitor is deferred to Phase 3L / Task 21.

#include "BandwidthMonitorTab.h"

#include "core/BoardCapabilities.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

namespace NereusSDR {

BandwidthMonitorTab::BandwidthMonitorTab(RadioModel* model, QWidget* parent)
    : QWidget(parent), m_model(model)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 8, 8, 8);
    outerLayout->setSpacing(8);

    // ── Live status group ─────────────────────────────────────────────────────
    // Source: bandwidth_monitor.h bandwidth_monitor_in/out (lines 48-49) —
    //   Phase 3I shows placeholder text; Task 21 / Phase 3L connects the feed.
    auto* statusGroup = new QGroupBox(tr("Live Status"), this);
    auto* statusForm  = new QFormLayout(statusGroup);
    statusForm->setLabelAlignment(Qt::AlignRight);

    m_currentRateLabel = new QLabel(QStringLiteral("— Mbps"), statusGroup);
    m_currentRateLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statusForm->addRow(tr("LAN PHY rate:"), m_currentRateLabel);

    m_throttleEventLabel = new QLabel(QStringLiteral("0 events"), statusGroup);
    m_throttleEventLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statusForm->addRow(tr("Throttle-triggered pauses:"), m_throttleEventLabel);

    outerLayout->addWidget(statusGroup);

    // ── Throttle settings group ───────────────────────────────────────────────
    auto* threshGroup = new QGroupBox(tr("Throttle Settings"), this);
    auto* threshForm  = new QFormLayout(threshGroup);
    threshForm->setLabelAlignment(Qt::AlignRight);

    // Throttle threshold spinbox 10..1000 Mbps, default 80 Mbps
    m_throttleThresholdSpin = new QSpinBox(threshGroup);
    m_throttleThresholdSpin->setRange(10, 1000);
    m_throttleThresholdSpin->setValue(80);
    m_throttleThresholdSpin->setSuffix(tr(" Mbps"));
    threshForm->addRow(tr("Throttle threshold:"), m_throttleThresholdSpin);

    // Auto-pause EP2 on throttle, default checked
    m_autoPauseCheck = new QCheckBox(tr("Auto-pause EP2 on throttle"), threshGroup);
    m_autoPauseCheck->setChecked(true);
    threshForm->addRow(m_autoPauseCheck);

    outerLayout->addWidget(threshGroup);
    outerLayout->addStretch();

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_throttleThresholdSpin,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
        emit settingChanged(QStringLiteral("bandwidthMonitor/throttleThresholdMbps"), value);
    });

    connect(m_autoPauseCheck, &QCheckBox::toggled, this, [this](bool checked) {
        emit settingChanged(QStringLiteral("bandwidthMonitor/autoPauseEp2"), checked);
    });
}

// ── populate ──────────────────────────────────────────────────────────────────

void BandwidthMonitorTab::populate(const RadioInfo& /*info*/, const BoardCapabilities& /*caps*/)
{
    // No board-specific gating required; live label updates come via
    // updateLiveStats() once Phase 3L / Task 21 wires the feed.
}

// ── updateLiveStats ───────────────────────────────────────────────────────────

void BandwidthMonitorTab::updateLiveStats(int currentMbps, int throttleEventCount)
{
    m_currentRateLabel->setText(
        QStringLiteral("%1 Mbps").arg(currentMbps));
    m_throttleEventLabel->setText(
        tr("%1 events").arg(throttleEventCount));
}

// ── restoreSettings ───────────────────────────────────────────────────────────

void BandwidthMonitorTab::restoreSettings(const QMap<QString, QVariant>& settings)
{
    auto it = settings.constFind(QStringLiteral("throttleThresholdMbps"));
    if (it != settings.constEnd()) {
        QSignalBlocker blocker(m_throttleThresholdSpin);
        m_throttleThresholdSpin->setValue(it.value().toInt());
    }

    it = settings.constFind(QStringLiteral("autoPauseEp2"));
    if (it != settings.constEnd()) {
        QSignalBlocker blocker(m_autoPauseCheck);
        m_autoPauseCheck->setChecked(it.value().toBool());
    }
}

} // namespace NereusSDR
