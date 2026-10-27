// =================================================================
// src/gui/AmpViewWindow.cpp  (NereusSDR)
// =================================================================
//
// Implementation of the AmpViewWindow modeless dialog.  See
// AmpViewWindow.h for the design rationale and Thetis cite map.
//
// Ported from Thetis sources:
//   Project Files/Source/Console/AmpView.cs
//   Project Files/Source/Console/AmpView.Designer.cs
// original licences from Thetis source are included below.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-06 — Phase 3M-4 Task 9: created by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-10-13 — Linked readout and session-local visibility;
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// =================================================================

/*  AmpView.cs

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


#include "AmpViewWindow.h"

#include "AmpViewChart.h"
#include "core/AppSettings.h"
#include "core/dsp/Ps3DisplayAdapter.h"
#include "core/session/PureSignalSessionFacade.h"
#include "models/RadioModel.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/PureSignalViewStyle.h"

#include <QByteArray>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDateTime>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHideEvent>
#include <QLabel>
#include <QScreen>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <utility>

namespace NereusSDR {
namespace {

constexpr auto kShowGain = "ampview/showGain";
constexpr auto kPhaseZoom = "ampview/phaseZoom";
constexpr auto kLowRes = "ampview/lowRes";
constexpr auto kOnTop = "ampview/onTop";
constexpr auto kGeometry = "ampview/geometry";
constexpr qint64 kStaleAfterMs = 2000;

bool preference(const char* key, bool fallback)
{
    const QString stored = AppSettings::instance()
                               .value(QLatin1String(key),
                                      fallback ? QStringLiteral("True")
                                               : QStringLiteral("False"))
                               .toString();
    return stored.compare(QStringLiteral("True"), Qt::CaseInsensitive) == 0
        || stored == QStringLiteral("1");
}

void persistPreference(const char* key, bool enabled)
{
    AppSettings::instance().setValue(
        QLatin1String(key), enabled ? QStringLiteral("True") : QStringLiteral("False"));
}

} // namespace

AmpViewWindow::AmpViewWindow(RadioModel* radioModel,
                             PureSignal* pureSignal,
                             QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("ampViewWindow"));
    setWindowTitle(QStringLiteral("AmpView 1.0")); // AmpView.Designer.cs:223
    setMinimumSize(440, 380);                       // AmpView.Designer.cs:221
    resize(564, 401);                               // AmpView.Designer.cs:214
    setModal(false);

    if (radioModel) {
        // A radio has one facade, which owns display identity/subscription.
        // Ignore the legacy coordinator argument in this production path.
        m_facade = radioModel->pureSignalFacade();
    } else if (pureSignal) {
        // Standalone coordinator tests have no RadioModel-owned facade.
        m_facade = new PureSignalSessionFacade(nullptr, pureSignal, this);
    }

    buildUi();
    applyPresentationStyle();
    restorePreferences();
    connectUi();
    restoreAndRepairGeometry();

    m_presentationTimer = new QTimer(this);
    m_presentationTimer->setInterval(1000);
    m_presentationTimer->setTimerType(Qt::CoarseTimer);
    connect(m_presentationTimer, &QTimer::timeout,
            this, &AmpViewWindow::updateFreshnessLabel);

    if (m_facade) {
        m_displayGeneration = m_facade->displayGeneration();
        connect(m_facade, &PureSignalSessionFacade::displaySnapshotReady,
                this, &AmpViewWindow::acceptSnapshot);
        connect(m_facade, &PureSignalSessionFacade::displayInvalidated,
                this, &AmpViewWindow::invalidateDisplay);
        connect(m_facade, &PureSignalSessionFacade::statusChanged,
                this, &AmpViewWindow::refreshAvailability);
    }
    refreshAvailability();
}

AmpViewWindow::~AmpViewWindow()
{
    setSubscribed(false);
}

void AmpViewWindow::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 4);
    root->setSpacing(3);

    m_chart = new AmpViewChart(this);
    root->addWidget(m_chart, 1);

    m_cursorReadout = new QLabel(this);
    m_cursorReadout->setObjectName(QStringLiteral("ampViewCursorReadout"));
    m_cursorReadout->setContentsMargins(8, 3, 8, 3);
    m_cursorReadout->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_cursorReadout->setFont(WorkspacePreview::readableFont(m_cursorReadout->font()));
    root->addWidget(m_cursorReadout);
    updateReadout();

    m_displayStatus = new QLabel(this);
    m_displayStatus->setObjectName(QStringLiteral("ampViewDisplayStatus"));
    m_displayStatus->setContentsMargins(7, 0, 7, 0);
    m_displayStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_displayStatus);

    auto* options = new QGridLayout;
    options->setContentsMargins(7, 0, 7, 0);
    options->setHorizontalSpacing(10);
    options->setVerticalSpacing(2);

    m_chkShowGain = new QCheckBox(tr("Show Gain"), this);
    m_chkShowGain->setObjectName(QStringLiteral("chkAVShowGain"));
    m_chkPhaseZoom = new QCheckBox(tr("Phase Zoom"), this);
    m_chkPhaseZoom->setObjectName(QStringLiteral("chkAVPhaseZoom"));
    m_chkLowRes = new QCheckBox(tr("Low Res"), this);
    m_chkLowRes->setObjectName(QStringLiteral("chkAVLowRes"));
    m_chkStayOnTop = new QCheckBox(tr("On Top"), this);
    m_chkStayOnTop->setObjectName(QStringLiteral("chkStayOnTop"));
    options->addWidget(m_chkShowGain, 0, 0);
    options->addWidget(m_chkPhaseZoom, 0, 1);
    options->addWidget(m_chkLowRes, 1, 0);
    options->addWidget(m_chkStayOnTop, 1, 1);

    m_chkReference = new QCheckBox(tr("- - Reference"), this);
    m_chkReference->setObjectName(QStringLiteral("chkAVReference"));
    m_chkMeasuredMagnitude = new QCheckBox(tr("● Amp mag/gain"), this);
    m_chkMeasuredMagnitude->setObjectName(QStringLiteral("chkAVMeasuredMagnitude"));
    m_chkMeasuredMagnitude->setToolTip(tr("Measured PA magnitude or gain"));
    m_chkMeasuredPhase = new QCheckBox(tr("◆ Amp phase"), this);
    m_chkMeasuredPhase->setObjectName(QStringLiteral("chkAVMeasuredPhase"));
    m_chkCorrectionMagnitude = new QCheckBox(tr("― Correction mag/gain"), this);
    m_chkCorrectionMagnitude->setObjectName(QStringLiteral("chkAVCorrectionMagnitude"));
    m_chkCorrectionMagnitude->setToolTip(tr("Correction magnitude or gain"));
    m_chkCorrectionPhase = new QCheckBox(tr("― Correction phase"), this);
    m_chkCorrectionPhase->setObjectName(QStringLiteral("chkAVCorrectionPhase"));
    options->addWidget(m_chkReference, 2, 0);
    options->addWidget(m_chkMeasuredMagnitude, 2, 1);
    options->addWidget(m_chkMeasuredPhase, 3, 0);
    options->addWidget(m_chkCorrectionMagnitude, 3, 1);
    options->addWidget(m_chkCorrectionPhase, 4, 0);
    root->addLayout(options);
}

void AmpViewWindow::applyPresentationStyle()
{
    if (!WorkspacePreview::enabled()) {
        setStyleSheet(QString());
        return;
    }
    setFont(PureSignalViewStyle::font(font()));
    const QString panel = PureSignalViewStyle::background(this).name();
    const QString text = PureSignalViewStyle::text().name();
    const QString muted = PureSignalViewStyle::muted().name();
    const QString border = PureSignalViewStyle::border().name();
    // Plot-series colors stay in the chart. Ordinary controls follow the
    // shared workspace text roles so their labels remain readable in every look.
    setStyleSheet(QStringLiteral(
        "QDialog#ampViewWindow { background: %1; color: %2; }"
        "QLabel#ampViewCursorReadout { background: %1; color: %2; border-top: 1px solid %4; border-bottom: 1px solid %4; }"
        "QLabel#ampViewDisplayStatus { color: %3; }"
        "QCheckBox { color: %2; }"
        "QCheckBox#chkAVReference { color: %3; }")
        .arg(panel, text, muted, border));
}

void AmpViewWindow::restorePreferences()
{
    const std::array<std::pair<QCheckBox*, bool>, 4> restored{{
        {m_chkShowGain, preference(kShowGain, false)},
        {m_chkPhaseZoom, preference(kPhaseZoom, false)},
        {m_chkLowRes, preference(kLowRes, true)},
        {m_chkStayOnTop, preference(kOnTop, false)},
    }};
    for (const auto& [checkBox, enabled] : restored) {
        const QSignalBlocker blocker(checkBox);
        checkBox->setChecked(enabled);
    }
    for (QCheckBox* checkBox : {m_chkReference, m_chkMeasuredMagnitude,
                                m_chkMeasuredPhase, m_chkCorrectionMagnitude,
                                m_chkCorrectionPhase}) {
        checkBox->setChecked(true);
    }

    m_chart->setShowGain(m_chkShowGain->isChecked());
    m_chart->setPhaseZoom(m_chkPhaseZoom->isChecked());
    m_chart->setLowRes(m_chkLowRes->isChecked());
    m_chart->setSeriesVisible(AmpViewChart::Series::Reference, m_chkReference->isChecked());
    m_chart->setSeriesVisible(AmpViewChart::Series::MeasuredMagnitude,
                              m_chkMeasuredMagnitude->isChecked());
    m_chart->setSeriesVisible(AmpViewChart::Series::MeasuredPhase,
                              m_chkMeasuredPhase->isChecked());
    m_chart->setSeriesVisible(AmpViewChart::Series::CorrectionMagnitude,
                              m_chkCorrectionMagnitude->isChecked());
    m_chart->setSeriesVisible(AmpViewChart::Series::CorrectionPhase,
                              m_chkCorrectionPhase->isChecked());
    setWindowFlag(Qt::WindowStaysOnTopHint, m_chkStayOnTop->isChecked());
    updateReadout();
}

void AmpViewWindow::connectUi()
{
    connect(m_chkShowGain, &QCheckBox::toggled, this, &AmpViewWindow::onShowGainToggled);
    connect(m_chkPhaseZoom, &QCheckBox::toggled, this, &AmpViewWindow::onPhaseZoomToggled);
    connect(m_chkLowRes, &QCheckBox::toggled, this, &AmpViewWindow::onLowResToggled);
    connect(m_chkStayOnTop, &QCheckBox::toggled, this, &AmpViewWindow::onStayOnTopToggled);

    connect(m_chart, &AmpViewChart::selectedInputChanged,
            this, &AmpViewWindow::updateReadout);
    const auto wireSeries = [this](QCheckBox* checkBox,
                                   AmpViewChart::Series series) {
        connect(checkBox, &QCheckBox::toggled, this,
                [this, series](bool visible) {
                    m_chart->setSeriesVisible(series, visible);
                    updateReadout();
                });
    };
    wireSeries(m_chkReference, AmpViewChart::Series::Reference);
    wireSeries(m_chkMeasuredMagnitude, AmpViewChart::Series::MeasuredMagnitude);
    wireSeries(m_chkMeasuredPhase, AmpViewChart::Series::MeasuredPhase);
    wireSeries(m_chkCorrectionMagnitude, AmpViewChart::Series::CorrectionMagnitude);
    wireSeries(m_chkCorrectionPhase, AmpViewChart::Series::CorrectionPhase);
}

void AmpViewWindow::restoreAndRepairGeometry()
{
    const QString encoded = AppSettings::instance()
                                .value(QLatin1String(kGeometry), QString())
                                .toString();
    if (!encoded.isEmpty()) {
        restoreGeometry(QByteArray::fromBase64(encoded.toLatin1()));
    }
    repairOffscreenPosition();
}

void AmpViewWindow::repairOffscreenPosition()
{
    const QRect current = geometry();
    bool visible = false;
    for (QScreen* screen : QGuiApplication::screens()) {
        if (screen && screen->availableGeometry().intersects(current)) {
            visible = true;
            break;
        }
    }
    if (visible) {
        return;
    }

    QScreen* screen = parentWidget() && parentWidget()->window()
        ? parentWidget()->window()->screen() : QGuiApplication::primaryScreen();
    const QRect available = screen ? screen->availableGeometry() : QRect(100, 100, 800, 600);
    const int width = std::min(std::max(current.width(), minimumWidth()), available.width());
    const int height = std::min(std::max(current.height(), minimumHeight()), available.height());
    setGeometry(available.x() + (available.width() - width) / 2,
                available.y() + (available.height() - height) / 2,
                width, height);
}

void AmpViewWindow::persistGeometry() const
{
    AppSettings::instance().setValue(
        QLatin1String(kGeometry), QString::fromLatin1(saveGeometry().toBase64()));
}

void AmpViewWindow::setStayOnTopFromParent(bool on)
{
    if (m_chkStayOnTop->isChecked() != on) {
        m_chkStayOnTop->setChecked(on);
    } else {
        onStayOnTopToggled(on);
    }
}

void AmpViewWindow::onShowGainToggled(bool on)
{
    m_chart->setShowGain(on);
    persistPreference(kShowGain, on);
    updateReadout();
}

void AmpViewWindow::onPhaseZoomToggled(bool on)
{
    m_chart->setPhaseZoom(on);
    persistPreference(kPhaseZoom, on);
}

void AmpViewWindow::onLowResToggled(bool on)
{
    m_chart->setLowRes(on);
    persistPreference(kLowRes, on);
}

void AmpViewWindow::onStayOnTopToggled(bool on)
{
    const bool wasVisible = isVisible();
    setWindowFlag(Qt::WindowStaysOnTopHint, on);
    if (wasVisible) {
        show();
    }
    persistPreference(kOnTop, on);
}

void AmpViewWindow::setSubscribed(bool subscribed)
{
    if (m_subscribed == subscribed) {
        return;
    }
    m_subscribed = subscribed;
    if (m_facade) {
        m_facade->setAmpViewSubscribed(subscribed);
    }
}

void AmpViewWindow::acceptSnapshot(const Ps3Snapshot& snapshot)
{
    if (!isVisible() || !m_subscribed || !m_facade) {
        return;
    }
    const std::uint64_t generation = m_facade->displayGeneration();
    if (snapshot.sessionGeneration != generation) {
        return;
    }
    if (generation != m_displayGeneration) {
        m_chart->clearData();
        updateReadout();
        m_displayGeneration = generation;
        m_lastSequence = 0;
        m_lastCaptureMs = 0;
        m_lastCaptureWhileMox = false;
    }
    if (snapshot.sequence <= m_lastSequence) {
        return;
    }

    const Ps3PlotData plot = Ps3DisplayAdapter::transform(snapshot);
    if (plot.measuredMagnitude.empty() && plot.correctionMagnitude.empty()
        && plot.correctionPhase.empty()) {
        return;
    }
    m_chart->setPlotData(plot);
    updateReadout();
    m_lastSequence = snapshot.sequence;
    m_lastCaptureMs = snapshot.capturedAtUnixMilliseconds;
    m_lastCaptureWhileMox = m_facade->statusSnapshot().mox;
    updateFreshnessLabel();
}

void AmpViewWindow::invalidateDisplay()
{
    m_chart->clearData();
    updateReadout();
    m_lastSequence = 0;
    m_lastCaptureMs = 0;
    m_lastCaptureWhileMox = false;
    m_displayGeneration = m_facade ? m_facade->displayGeneration() : 0;
    refreshAvailability();
}

void AmpViewWindow::refreshAvailability()
{
    if (!m_facade) {
        m_displayStatus->setText(tr("PureSignal display unsupported."));
        return;
    }
    if (m_facade->displayGeneration() != m_displayGeneration) {
        m_chart->clearData();
        updateReadout();
        m_displayGeneration = m_facade->displayGeneration();
        m_lastSequence = 0;
        m_lastCaptureMs = 0;
        m_lastCaptureWhileMox = false;
    }
    if (!m_facade->available()) {
        m_chart->clearData();
        updateReadout();
        m_lastCaptureMs = 0;
        m_lastCaptureWhileMox = false;
        m_displayStatus->setText(tr("PureSignal display unavailable."));
        return;
    }
    updateFreshnessLabel();
}

void AmpViewWindow::updateFreshnessLabel()
{
    if (!m_facade) {
        m_displayStatus->setText(tr("PureSignal display unsupported."));
        return;
    }
    if (!m_facade->available()) {
        m_displayStatus->setText(tr("PureSignal display unavailable."));
        return;
    }
    if (m_lastCaptureMs <= 0) {
        m_displayStatus->setText(tr("Waiting for PureSignal display data…"));
        return;
    }
    const qint64 age = std::max<qint64>(
        0, QDateTime::currentMSecsSinceEpoch() - m_lastCaptureMs);
    const bool current = age <= kStaleAfterMs && m_lastCaptureWhileMox
        && m_facade->statusSnapshot().mox;
    if (!current) {
        m_displayStatus->setText(tr("Retained PureSignal curve • %1 ms old").arg(age));
    } else {
        m_displayStatus->setText(tr("Current PureSignal curve • %1 ms old").arg(age));
    }
}

void AmpViewWindow::updateReadout()
{
    const AmpViewCursorReadout values = m_chart->readoutAt(m_chart->selectedInput());
    const auto formatted = [](const std::optional<double>& value, int decimals) {
        return value ? QString::number(*value, 'f', decimals) : QStringLiteral("–");
    };
    const QString ampLabel = m_chart->showGain() ? tr("AMP GAIN") : tr("AMP MAG");
    const QString correctionLabel = m_chart->showGain() ? tr("CORR GAIN") : tr("CORR MAG");
    m_cursorReadout->setText(
        tr("INPUT %1\n%2 %3  ·  %4 %5\nAMP PHASE %6°  ·  CORR PHASE %7°")
            .arg(QString::number(values.inputMagnitude, 'f', 2),
                 ampLabel, formatted(values.magnitudeAmp, 3),
                 correctionLabel, formatted(values.magnitudeCorrection, 3),
                 formatted(values.phaseAmp, 1), formatted(values.phaseCorrection, 1)));
}

void AmpViewWindow::closeEvent(QCloseEvent* event)
{
    persistGeometry(); // AmpView.cs:465-468 Common.SaveForm
    setSubscribed(false);
    event->ignore();
    hide();
}

void AmpViewWindow::hideEvent(QHideEvent* event)
{
    setSubscribed(false);
    if (m_presentationTimer) {
        m_presentationTimer->stop();
    }
    QDialog::hideEvent(event);
}

void AmpViewWindow::showEvent(QShowEvent* event)
{
    applyPresentationStyle();
    QDialog::showEvent(event);
    repairOffscreenPosition();
    setSubscribed(true);
    if (m_presentationTimer) {
        m_presentationTimer->start();
    }
    if (m_facade) {
        if (const auto snapshot = m_facade->displaySnapshot()) {
            acceptSnapshot(*snapshot);
        }
    }
    refreshAvailability();
}

} // namespace NereusSDR
