// =================================================================
// src/gui/setup/MultimeterPage.cpp  (NereusSDR)
// =================================================================
//
// Task 3.1 — Display → Multimeter setup page implementation.
//
// Ported from Thetis source:
//   Project Files/Source/Console/display.cs (multimeter group controls)
//   Project Files/Source/Console/console.cs (S-meter / dBm / µV unit-mode)
// Original licence from Thetis source is included below.
//
// NereusSDR-original page structure.  Default values and AppSettings key names
// derived from Thetis Display→General Multimeter group per design Section 3A.
//
// Source cites:
//   From Thetis display.cs (multimeter group controls) [v2.10.3.13]
//   From Thetis console.cs (S-meter / dBm / µV unit-mode) [v2.10.3.13]
//
// Default values:
//   MultimeterDelayMs          = 100   — Thetis udDisplayMeterDelay default [v2.10.3.13]
//   MultimeterPeakHoldMs       = 1500  — Thetis udDisplayMultiPeakHoldTime default [v2.10.3.13]
//   MultimeterTextHoldMs       = 1500  — Thetis udDisplayMultiTextHoldTime default [v2.10.3.13]
//   MultimeterAverageWindow    = 1     — Thetis udDisplayMeterAvg minimum (no averaging) [v2.10.3.13]
//   MultimeterDigitalDelayMs   = 100   — Thetis udMeterDigitalDelay default [v2.10.3.13]
//   MultimeterShowDecimal      = True  — Thetis chkDisplayMeterShowDecimal default [v2.10.3.13]
//   MultimeterUnitMode         = "dBm" — Thetis default display unit [v2.10.3.13]
//   MultimeterSignalHistoryEnabled    = False — off by default [v2.10.3.13]
//   MultimeterSignalHistoryDurationMs = 60000 — 60 s default [v2.10.3.13]
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-01 — Skeleton created in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-23 - R-R3-21 / R-R3-10: the S-meter sample interval is the Core's
//                 setting, disabled while a remote window does not have
//                 the Core's settings.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-23 - R-R3-21: applyPersistedSettings() carries the saved
//                 averaging window, update interval, unit, decimal and history
//                 duration into the meters at startup, not only when this
//                 page opens. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-24 - R-R3-49: peak hold, text hold, digital delay and the
//                 history enable are hidden (UnbuiltFeatures) until the
//                 meters use them. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-28 - R-IOS-18 (Display V12): decimal, units and history
//                 duration carry Setup description ids. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

//=================================================================
// display.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley (W5WC)
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
// Waterfall AGC Modifications Copyright (C) 2013 Phil Harman (VK6APH)
// Transitions to directX and continual modifications Copyright (C) 2020-2025 Richard Samphire (MW0LGE)
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

#include "MultimeterPage.h"
#include "core/AppSettings.h"
#include "models/RadioModel.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/HistoryGraphItem.h"
#include "gui/UnbuiltFeatures.h"
// Remote Daemon R2 Task 12: the delay spinbox also drives SliceMeterPump's
// interval, or the operator's delay slider silently stops changing the
// per-flag S-meter cadence while still changing MeterPoller's.
#include "core/meters/SliceMeterPump.h"
#include "gui/containers/ContainerManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>

namespace NereusSDR {

MultimeterPage::MultimeterPage(RadioModel* radioModel, QWidget* parent)
    : SetupPage(QStringLiteral("Multimeter"), radioModel, parent)
{
    buildUI();
    loadSettings();
    connectSignals();
}

void MultimeterPage::buildUI()
{
    // ── Multimeter polling / display group ───────────────────────────────────
    // From Thetis Display→General Multimeter group [v2.10.3.13]
    QGroupBox* multiGroup = addSection(tr("Multimeter"));

    m_delayMs = addLabeledSpinner(
        tr("Polling delay:"), 10, 2000,
        // From Thetis udDisplayMeterDelay default 100ms [v2.10.3.13]
        100);
    m_delayMs->setProperty("nereusSetupId", "display.multimeter.pollingDelay");
    m_delayMs->setSuffix(QStringLiteral(" ms"));
    // The meter values are read from WDSP at this interval.
    m_delayMs->setToolTip(tr("How often the meter values are updated (10–2000 ms). "
                              "Lower values give faster response; higher values reduce CPU load."));

    m_peakHoldMs = addLabeledSpinner(
        tr("Peak hold time:"), 0, 60000,
        // From Thetis udDisplayMultiPeakHoldTime default 1500ms [v2.10.3.13]
        1500);
    m_peakHoldMs->setSuffix(QStringLiteral(" ms"));
    m_peakHoldMs->setToolTip(tr("How long the peak indicator on bar meters stays at the highest reading."));

    m_textHoldMs = addLabeledSpinner(
        tr("Text hold time:"), 0, 60000,
        // From Thetis udDisplayMultiTextHoldTime default 1500ms [v2.10.3.13]
        1500);
    m_textHoldMs->setSuffix(QStringLiteral(" ms"));
    m_textHoldMs->setToolTip(tr("How long the text readout holds its peak value before updating."));

    m_avgWindow = addLabeledSpinner(
        tr("Averaging window:"), 1, 32,
        // From Thetis udDisplayMeterAvg minimum 1 (no averaging) [v2.10.3.13]
        1);
    m_avgWindow->setToolTip(tr("Number of samples averaged per meter update. "
                                "1 = no averaging (fastest response). Higher values smooth rapid fluctuations."));
    UnbuiltFeatures::hideRowUnlessBuilt(m_avgWindow, UnbuiltFeature::MultimeterAveraging);

    m_digitalDelayMs = addLabeledSpinner(
        tr("Digital delay:"), 10, 2000,
        // From Thetis udMeterDigitalDelay default 100ms [v2.10.3.13]
        100);
    m_digitalDelayMs->setSuffix(QStringLiteral(" ms"));
    m_digitalDelayMs->setToolTip(tr("Polling rate for digital readout items (separate from the bar meter rate)."));

    // QGroupBox* is captured in addSection but we add the checkbox manually
    // so it spans the full row without a label prefix.
    m_showDecimal = new QCheckBox(tr("Show decimal point in readouts"), this);
    m_showDecimal->setProperty("nereusSetupId", "display.multimeter.showDecimal");
    m_showDecimal->setToolTip(tr("Display a decimal digit in S-meter and dBm text readouts "
                                  "(e.g. S5.3 or -85.6 dBm)."));
    contentLayout()->addWidget(m_showDecimal);

    Q_UNUSED(multiGroup)

    // ── Signal Units group ───────────────────────────────────────────────────
    // Collapses Thetis radSReading / radDBM / radUV radio buttons into a combo.
    // From Thetis console.cs unit-mode radio group [v2.10.3.13]
    addSection(tr("Signal Units"));
    m_unitMode = addLabeledCombo(
        tr("Display units:"),
        {
            QStringLiteral("S"),    // S-units (e.g. S9, S5)
            QStringLiteral("dBm"),  // decibels relative to 1 mW
            QStringLiteral("uV")    // microvolts
        });
    m_unitMode->setProperty("nereusSetupId", "display.multimeter.unitMode");
    m_unitMode->setToolTip(tr("Sets the unit used for signal level readouts across all meter items. "
                               "S = IARU S-scale (S1–S9+dB), dBm = -130 to 0, uV = microvolts at 50Ω."));

    // ── Signal History group ─────────────────────────────────────────────────
    // From Thetis console.cs chkSignalHistory + udSignalHistoryDuration [v2.10.3.13]
    addSection(tr("Signal History"));

    m_signalHistoryEnable = new QCheckBox(tr("Enable signal history graph"), this);
    m_signalHistoryEnable->setToolTip(tr("Show a scrolling history of signal levels in HistoryGraph meter items."));
    contentLayout()->addWidget(m_signalHistoryEnable);

    m_signalHistoryDurationMs = addLabeledSpinner(
        tr("History duration:"), 1000, 600000,
        // From Thetis udSignalHistoryDuration default 60 s [v2.10.3.13]
        60000);
    m_signalHistoryDurationMs->setSuffix(QStringLiteral(" ms"));
    m_signalHistoryDurationMs->setProperty("nereusSetupId", "display.multimeter.historyDuration");
    m_signalHistoryDurationMs->setToolTip(tr("Total time span shown in the signal history graph (1–600 000 ms)."));

    // ── Cross-link ───────────────────────────────────────────────────────────
    // R-R3-49: saved, but no meter reads these four yet; hidden until one
    // does. Their saved values stay as they are.
    for (QWidget* control : std::initializer_list<QWidget*>{
             m_peakHoldMs, m_textHoldMs, m_digitalDelayMs, m_signalHistoryEnable}) {
        UnbuiltFeatures::hideRowUnlessBuilt(control, UnbuiltFeature::MultimeterHolds);
    }

    m_backBtn = new QPushButton(tr("← Spectrum defaults"), this);
    m_backBtn->setToolTip(tr("Navigate to the Spectrum Defaults setup page."));
    contentLayout()->addWidget(m_backBtn, 0, Qt::AlignLeft);
}

// R-R3-21: the values below reached the meters only when this page was
// opened, so a restart lost them until the operator visited Setup.
// MainWindow calls this once its containers are restored; the page's own
// loadSettings() and control handlers apply the same keys the same way.
// Peak hold, text hold, digital delay and the history enable flag are not
// applied here: nothing consumes them yet.
void MultimeterPage::applyPersistedSettings(RadioModel* model)
{
    if (model == nullptr) {
        return;
    }
    const auto& s = AppSettings::instance();

    // From Thetis udDisplayMeterAvg [v2.10.3.13]
    if (auto* p = model->meterPoller()) {
        p->setAverageWindow(s.value(QStringLiteral("MultimeterAverageWindow"), 1).toInt());
    }
    applyPersistedMeterInterval(model);

    auto* cm = model->containerManager();
    if (cm == nullptr) {
        return;
    }
    cm->forEachMeterItem([](MeterItem* item) { applyPersistedSettingsTo(item); });
}

void MultimeterPage::applyPersistedMeterInterval(RadioModel* model)
{
    auto* p = model ? model->meterPoller() : nullptr;
    if (p == nullptr) {
        return;
    }
    // From Thetis udDisplayMeterDelay [v2.10.3.13]: the meter update
    // interval (R-R3-21: it too applied only when this page opened). The key
    // is the Core's in a remote window, so MainWindow calls this again once
    // the Core's settings arrive.
    p->setIntervalMs(AppSettings::instance()
                         .value(QStringLiteral("MultimeterDelayMs"), 100).toInt());
}

void MultimeterPage::applyPersistedSettingsTo(MeterItem* item)
{
    if (item == nullptr) {
        return;
    }
    const auto& s = AppSettings::instance();
    // From Thetis radSReading/radDBM/radUV + chkDisplayMeterShowDecimal [v2.10.3.13]
    const QString unitStr =
        s.value(QStringLiteral("MultimeterUnitMode"), QStringLiteral("dBm")).toString();
    const MeterItem::MeterUnit unit = (unitStr == QStringLiteral("S"))
        ? MeterItem::MeterUnit::S
        : (unitStr == QStringLiteral("uV"))
            ? MeterItem::MeterUnit::uV
            : MeterItem::MeterUnit::dBm;
    const bool dec =
        s.value(QStringLiteral("MultimeterShowDecimal"), QStringLiteral("True")).toString()
            == QStringLiteral("True");
    // From Thetis udSignalHistoryDuration [v2.10.3.13]
    const int historyMs =
        s.value(QStringLiteral("MultimeterSignalHistoryDurationMs"), 60000).toInt();
    item->setUnitMode(unit);
    item->setShowDecimal(dec);
    if (auto* h = qobject_cast<HistoryGraphItem*>(item)) {
        h->setDurationMs(historyMs);
    }
}

void MultimeterPage::loadSettings()
{
    auto& s = AppSettings::instance();

    // Multimeter group
    // From Thetis udDisplayMeterDelay [v2.10.3.13]
    m_delayMs->setValue(s.value(QStringLiteral("MultimeterDelayMs"), 100).toInt());
    // From Thetis udDisplayMultiPeakHoldTime [v2.10.3.13]
    m_peakHoldMs->setValue(s.value(QStringLiteral("MultimeterPeakHoldMs"), 1500).toInt());
    // From Thetis udDisplayMultiTextHoldTime [v2.10.3.13]
    m_textHoldMs->setValue(s.value(QStringLiteral("MultimeterTextHoldMs"), 1500).toInt());
    // From Thetis udDisplayMeterAvg [v2.10.3.13]
    m_avgWindow->setValue(s.value(QStringLiteral("MultimeterAverageWindow"), 1).toInt());
    // From Thetis udMeterDigitalDelay [v2.10.3.13]
    m_digitalDelayMs->setValue(s.value(QStringLiteral("MultimeterDigitalDelayMs"), 100).toInt());
    // From Thetis chkDisplayMeterShowDecimal [v2.10.3.13]
    m_showDecimal->setChecked(
        s.value(QStringLiteral("MultimeterShowDecimal"), QStringLiteral("True")).toString()
            == QStringLiteral("True"));

    // Signal Units
    // From Thetis radSReading/radDBM/radUV [v2.10.3.13]
    m_unitMode->setCurrentText(
        s.value(QStringLiteral("MultimeterUnitMode"), QStringLiteral("dBm")).toString());

    // Signal History
    // From Thetis chkSignalHistory + udSignalHistoryDuration [v2.10.3.13]
    m_signalHistoryEnable->setChecked(
        s.value(QStringLiteral("MultimeterSignalHistoryEnabled"), QStringLiteral("False")).toString()
            == QStringLiteral("True"));
    m_signalHistoryDurationMs->setValue(
        s.value(QStringLiteral("MultimeterSignalHistoryDurationMs"), 60000).toInt());

    // Apply live to MeterPoller on load (carries persisted values into the
    // running poller without needing the user to touch a control).
    if (auto* p = model() ? model()->meterPoller() : nullptr) {
        p->setIntervalMs(m_delayMs->value());
        p->setAverageWindow(m_avgWindow->value());
    }
    // Remote Daemon R2 Task 12: same delay value, also applied to
    // SliceMeterPump (null on a Role::Remote model -- see
    // RadioModel::sliceMeterPump()'s doc comment).
    if (auto* pump = model() ? model()->sliceMeterPump() : nullptr) {
        pump->setIntervalMs(m_delayMs->value());
    }

    // Task 3.2: apply persisted unit-mode + show-decimal to all live
    // MeterItems at setup-page open time.  connectSignals() hasn't run yet
    // so we drive the broadcast directly here.  ContainerManager may not be
    // populated at first launch (items are created later), but on subsequent
    // opens of the Setup dialog this carries the persisted choice into items
    // that were created during the session.
    if (auto* cm = model() ? model()->containerManager() : nullptr) {
        const QString unitStr = m_unitMode->currentText();
        const MeterItem::MeterUnit unit = (unitStr == QStringLiteral("S"))
            ? MeterItem::MeterUnit::S
            : (unitStr == QStringLiteral("uV"))
                ? MeterItem::MeterUnit::uV
                : MeterItem::MeterUnit::dBm;
        const bool dec = m_showDecimal->isChecked();
        cm->forEachMeterItem([unit, dec](MeterItem* item) {
            item->setUnitMode(unit);
            item->setShowDecimal(dec);
        });
    }
}

void MultimeterPage::connectSignals()
{
    // Polling delay: persists + applies live to MeterPoller and (Remote
    // Daemon R2 Task 12) SliceMeterPump.
    connect(m_delayMs, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [this](int v) {
            AppSettings::instance().setValue(QStringLiteral("MultimeterDelayMs"), v);
            if (auto* p = model() ? model()->meterPoller() : nullptr) {
                p->setIntervalMs(v);
            }
            if (auto* pump = model() ? model()->sliceMeterPump() : nullptr) {
                pump->setIntervalMs(v);
            }
        });

    // Peak hold time — persists only (BarItem consumes in Task 3.2)
    connect(m_peakHoldMs, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [](int v) { AppSettings::instance().setValue(QStringLiteral("MultimeterPeakHoldMs"), v); });

    // Text hold time — persists only (TextItem / SignalTextItem consume in Task 3.2)
    connect(m_textHoldMs, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [](int v) { AppSettings::instance().setValue(QStringLiteral("MultimeterTextHoldMs"), v); });

    // Averaging window — persists + applies live to MeterPoller
    connect(m_avgWindow, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [this](int v) {
            AppSettings::instance().setValue(QStringLiteral("MultimeterAverageWindow"), v);
            if (auto* p = model() ? model()->meterPoller() : nullptr) {
                p->setAverageWindow(v);
            }
        });

    // Digital delay — persists only (digital readout items consume in Task 3.2)
    connect(m_digitalDelayMs, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [](int v) { AppSettings::instance().setValue(QStringLiteral("MultimeterDigitalDelayMs"), v); });

    // Show decimal — persists + broadcasts to all bound MeterItems (Task 3.2)
    connect(m_showDecimal, &QCheckBox::toggled, this,
        [this](bool v) {
            AppSettings::instance().setValue(
                QStringLiteral("MultimeterShowDecimal"),
                v ? QStringLiteral("True") : QStringLiteral("False"));
            // Fan-out to all live MeterItem instances
            if (auto* cm = model() ? model()->containerManager() : nullptr) {
                cm->forEachMeterItem([v](MeterItem* item) {
                    item->setShowDecimal(v);
                });
            }
        });

    // Unit mode — persists + broadcasts to all bound MeterItems (Task 3.2)
    // Fan-out: MultimeterPage combo → ContainerManager::forEachMeterItem →
    // MeterItem::setUnitMode() on every live item.  SignalTextItem overrides
    // setUnitMode() to also sync its own per-item Units enum.
    connect(m_unitMode, &QComboBox::currentTextChanged, this,
        [this](const QString& v) {
            AppSettings::instance().setValue(QStringLiteral("MultimeterUnitMode"), v);
            const MeterItem::MeterUnit unit = (v == QStringLiteral("S"))
                ? MeterItem::MeterUnit::S
                : (v == QStringLiteral("uV"))
                    ? MeterItem::MeterUnit::uV
                    : MeterItem::MeterUnit::dBm;
            if (auto* cm = model() ? model()->containerManager() : nullptr) {
                cm->forEachMeterItem([unit](MeterItem* item) {
                    item->setUnitMode(unit);
                });
            }
        });

    // Signal history enable — persists only (Task 3.3)
    // The enable flag is a UI setting; actual visibility is controlled by the
    // container that holds the HistoryGraphItem and its binding configuration.
    connect(m_signalHistoryEnable, &QCheckBox::toggled, this,
        [](bool v) {
            AppSettings::instance().setValue(
                QStringLiteral("MultimeterSignalHistoryEnabled"),
                v ? QStringLiteral("True") : QStringLiteral("False"));
        });

    // Signal history duration — persists + broadcasts to all HistoryGraphItem instances (Task 3.3)
    connect(m_signalHistoryDurationMs, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [this](int v) {
            AppSettings::instance().setValue(QStringLiteral("MultimeterSignalHistoryDurationMs"), v);
            // Task 3.3: broadcast duration to all HistoryGraphItem instances
            if (auto* cm = model() ? model()->containerManager() : nullptr) {
                cm->forEachMeterItem([v](MeterItem* item) {
                    if (auto* h = qobject_cast<HistoryGraphItem*>(item)) {
                        h->setDurationMs(v);
                    }
                });
            }
        });

    // Cross-link
    connect(m_backBtn, &QPushButton::clicked, this,
            &MultimeterPage::backToSpectrumDefaultsRequested);
}

void MultimeterPage::setStationSettingsAvailable(bool available, const QString& reason)
{
    gateStationControls({m_delayMs}, available, reason);
}

}  // namespace NereusSDR
