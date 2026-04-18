// =================================================================
// src/gui/setup/hardware/RadioInfoTab.cpp  (NereusSDR)
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

#include "RadioInfoTab.h"

#include "core/BoardCapabilities.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"

#include <QClipboard>
#include <QComboBox>
#include <QFormLayout>
#include <QGuiApplication>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace NereusSDR {

RadioInfoTab::RadioInfoTab(RadioModel* model, QWidget* parent)
    : QWidget(parent), m_model(model)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 8, 8, 8);

    // ── Identity group ────────────────────────────────────────────────────────
    auto* identGroup = new QGroupBox(tr("Board Identity"), this);
    auto* form = new QFormLayout(identGroup);
    form->setLabelAlignment(Qt::AlignRight);

    m_boardLabel    = new QLabel(QStringLiteral("—"), identGroup);
    m_protocolLabel = new QLabel(QStringLiteral("—"), identGroup);
    m_adcCountLabel = new QLabel(QStringLiteral("—"), identGroup);
    m_maxRxLabel    = new QLabel(QStringLiteral("—"), identGroup);
    m_firmwareLabel = new QLabel(QStringLiteral("—"), identGroup);
    m_macLabel      = new QLabel(QStringLiteral("—"), identGroup);
    m_ipLabel       = new QLabel(QStringLiteral("—"), identGroup);

    for (QLabel* lbl : {m_boardLabel, m_protocolLabel, m_adcCountLabel,
                        m_maxRxLabel, m_firmwareLabel, m_macLabel, m_ipLabel}) {
        lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }

    form->addRow(tr("Board:"),      m_boardLabel);
    form->addRow(tr("Protocol:"),   m_protocolLabel);
    form->addRow(tr("ADC count:"),  m_adcCountLabel);
    form->addRow(tr("Max RX:"),     m_maxRxLabel);
    form->addRow(tr("Firmware:"),   m_firmwareLabel);
    form->addRow(tr("MAC:"),        m_macLabel);
    form->addRow(tr("IP address:"), m_ipLabel);

    outerLayout->addWidget(identGroup);

    // ── Operating parameters group ────────────────────────────────────────────
    auto* paramGroup = new QGroupBox(tr("Operating Parameters"), this);
    auto* paramForm  = new QFormLayout(paramGroup);
    paramForm->setLabelAlignment(Qt::AlignRight);

    // Sample rate combo — entries populated in populate() from caps.sampleRates
    // Source: Thetis Setup.cs:847 — include_extra_p1_rate; sample rates filtered
    // per-model and listed in comboGeneralSampleRate.
    m_sampleRateCombo = new QComboBox(paramGroup);
    m_sampleRateCombo->setMinimumWidth(120);

    // Active RX count spinbox — capped at caps.maxReceivers in populate()
    // Source: Thetis Setup.cs numericUpDownNr, range 1..board_max_rx
    m_activeRxSpin = new QSpinBox(paramGroup);
    m_activeRxSpin->setRange(1, 1);
    m_activeRxSpin->setValue(1);
    m_activeRxSpin->setEnabled(false); // enabled when a radio is present

    paramForm->addRow(tr("Sample rate (Hz):"), m_sampleRateCombo);
    paramForm->addRow(tr("Active RX count:"),  m_activeRxSpin);

    outerLayout->addWidget(paramGroup);

    // ── Support info button ───────────────────────────────────────────────────
    m_copySupportInfoButton = new QPushButton(tr("Copy Support Info to Clipboard"), this);
    m_copySupportInfoButton->setToolTip(
        tr("Copies board identity and firmware version to the clipboard for bug reports."));
    outerLayout->addWidget(m_copySupportInfoButton);
    outerLayout->addStretch();

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_sampleRateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RadioInfoTab::onSampleRateChanged);

    connect(m_copySupportInfoButton, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(m_currentInfo);
    });
}

// ── populate ──────────────────────────────────────────────────────────────────

void RadioInfoTab::populate(const RadioInfo& info, const BoardCapabilities& caps)
{
    // Fill read-only identity labels
    m_boardLabel->setText(info.name.isEmpty()
        ? QString::fromLatin1(caps.displayName)
        : info.name);
    m_protocolLabel->setText(
        info.protocol == ProtocolVersion::Protocol2
            ? QStringLiteral("Protocol 2")
            : QStringLiteral("Protocol 1"));
    m_adcCountLabel->setText(QString::number(caps.adcCount));
    m_maxRxLabel->setText(QString::number(caps.maxReceivers));
    m_firmwareLabel->setText(
        info.firmwareVersion > 0
            ? QString::number(info.firmwareVersion)
            : QStringLiteral("—"));
    m_macLabel->setText(info.macAddress.isEmpty() ? QStringLiteral("—") : info.macAddress);
    m_ipLabel->setText(info.address.isNull()
        ? QStringLiteral("—")
        : info.address.toString());

    // Rebuild sample-rate combo from caps.sampleRates (zero = unused slot)
    // Source: Thetis Setup.cs:847-850 — rate list filtered per model
    {
        QSignalBlocker blocker(m_sampleRateCombo);
        m_sampleRateCombo->clear();
        for (int rate : caps.sampleRates) {
            if (rate > 0) {
                m_sampleRateCombo->addItem(
                    QStringLiteral("%1").arg(rate), rate);
            }
        }
        // Select the max-rate entry by default (last non-zero slot)
        if (m_sampleRateCombo->count() > 0) {
            m_sampleRateCombo->setCurrentIndex(m_sampleRateCombo->count() - 1);
        }
    }

    // Active-RX spinbox range 1..caps.maxReceivers
    // Source: Thetis Setup.cs numericUpDownNr
    {
        QSignalBlocker blocker(m_activeRxSpin);
        m_activeRxSpin->setRange(1, caps.maxReceivers);
        m_activeRxSpin->setValue(1);
        m_activeRxSpin->setEnabled(true);
    }

    // Build clipboard text for Copy Support Info button
    m_currentInfo = QStringLiteral(
        "Board: %1\n"
        "Protocol: %2\n"
        "ADC count: %3\n"
        "Max RX: %4\n"
        "Firmware: %5\n"
        "MAC: %6\n"
        "IP: %7\n"
        "Max sample rate: %8 Hz\n")
        .arg(m_boardLabel->text())
        .arg(m_protocolLabel->text())
        .arg(caps.adcCount)
        .arg(caps.maxReceivers)
        .arg(m_firmwareLabel->text())
        .arg(m_macLabel->text())
        .arg(m_ipLabel->text())
        .arg(caps.maxSampleRate);
}

// ── private slots ─────────────────────────────────────────────────────────────

void RadioInfoTab::onSampleRateChanged(int index)
{
    if (index < 0) { return; }
    int rate = m_sampleRateCombo->itemData(index).toInt();
    if (rate > 0) {
        emit settingChanged(QStringLiteral("radioInfo/sampleRate"), rate);
    }
}

// ── restoreSettings ───────────────────────────────────────────────────────────

void RadioInfoTab::restoreSettings(const QMap<QString, QVariant>& settings)
{
    // sampleRate — match combo item data
    auto srIt = settings.constFind(QStringLiteral("sampleRate"));
    if (srIt != settings.constEnd()) {
        const int rate = srIt.value().toInt();
        QSignalBlocker blocker(m_sampleRateCombo);
        for (int i = 0; i < m_sampleRateCombo->count(); ++i) {
            if (m_sampleRateCombo->itemData(i).toInt() == rate) {
                m_sampleRateCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    // activeRxCount
    auto rxIt = settings.constFind(QStringLiteral("activeRxCount"));
    if (rxIt != settings.constEnd()) {
        QSignalBlocker blocker(m_activeRxSpin);
        m_activeRxSpin->setValue(rxIt.value().toInt());
    }
}

} // namespace NereusSDR
