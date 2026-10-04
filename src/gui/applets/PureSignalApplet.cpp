// =================================================================
// src/gui/applets/PureSignalApplet.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/PSForm.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-04 - Include the coordinator type for Qt 6.4 typed connections;
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-04-18 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Layout ports Thetis PSForm.cs (PureSignal feedback/correction controls). All controls NYI — wired in later phase (3M-4).
//   2026-05-06 — Phase 3M-4 Task 13: replaced NyiOverlay::markNyi calls with
//                 live wiring to the PureSignal coordinator (Task 7).  Save /
//                 Restore use QFileDialog with default folder
//                 ~/.config/NereusSDR/PureSignal/.  Right-click on every
//                 control emits openPureSignalDialogRequested.  J.J. Boyd
//                 (KG4VCF), with AI-assisted source-first protocol via
//                 Anthropic Claude Code.
//   2026-09-22 — Migrated actions, status, and correction assets to the
//                 shared PureSignalSessionFacade boundary.
//   2026-09-25 - R-R3-49 (parity Task 7): Calibrate and Auto-Cal follow the
//                 facade's canArm (a remote window arms PureSignal on a Core
//                 at transmitSettingsVersion 7 off the air), greyed with the
//                 reason while the Core's radio is on the air; 2-Tone stays
//                 on canActuate. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-30 - Fix wave GUI-I7: Calibrate, Auto-Cal and 2-Tone grey
//                 with the facade's reason whenever they cannot run.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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

#include "PureSignalApplet.h"

#include "core/PureSignal.h"
#include "core/session/PureSignalSessionFacade.h"
#include "gui/DspAssetDialog.h"
#include "gui/HGauge.h"
#include "gui/StyleConstants.h"
#include "models/PureSignalSettings.h"
#include "models/RadioModel.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

namespace NereusSDR {

namespace {

// LED active style — green-on-dark (mirrors PsaIndicatorWidget palette).
constexpr const char* kLedActiveStyle =
    "QLabel {"
    "  background: #20c060; border-radius: 4px;"
    "  color: #000000; font-size: 8px; font-weight: bold;"
    "  padding: 0px 2px;"
    "}";

// LED inactive style — desaturated dark grey (matches the original
// styling baked into PureSignalApplet's buildUI).
constexpr const char* kLedInactiveStyle =
    "QLabel {"
    "  background: #405060; border-radius: 4px;"
    "  color: #6080a0; font-size: 8px; font-weight: bold;"
    "  padding: 0px 2px;"
    "}";

// R-R3-49 (parity Task 7): grey `control` with `reason` as its tooltip,
// remembering its own tooltip, or put it back and apply `enabled`. An empty
// reason means no gate.
void gateWithReason(QWidget* control, bool enabled, const QString& reason)
{
    if (!control) {
        return;
    }
    static constexpr auto kSavedTooltip = "PureSignalAppletSavedTooltip";
    if (!reason.isEmpty()) {
        if (!control->property(kSavedTooltip).isValid()) {
            control->setProperty(kSavedTooltip, control->toolTip());
        }
        control->setEnabled(false);
        control->setToolTip(reason);
        return;
    }
    if (control->property(kSavedTooltip).isValid()) {
        control->setToolTip(control->property(kSavedTooltip).toString());
        control->setProperty(kSavedTooltip, QVariant());
    }
    control->setEnabled(enabled);
}

} // namespace

PureSignalApplet::PureSignalApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();
    wireRightClicks();

    if (m_model) {
        m_facade = m_model->pureSignalFacade();
        wireFacade();
        connect(m_model, &RadioModel::pureSignalCoordinatorReady, this,
                &PureSignalApplet::setPureSignal);
    }
    refreshFromFacade();
}

void PureSignalApplet::setPureSignal(PureSignal* coordinator)
{
    if (m_facade) {
        m_facade->setCoordinator(coordinator);
    }
    refreshFromFacade();
}

void PureSignalApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    // Phase 3M-4 bench-fix: AppletPanelWidget::wrapWithTitleBar (AppletPanelWidget.cpp:155)
    // already prepends a host-side title bar from appletTitle().  Adding our own
    // appletTitleBar() here resulted in a double header.  Same-shape bug exists
    // in DiversityApplet / DigitalApplet / TunerApplet / CwxApplet / CatApplet /
    // DvkApplet — fix here is PureSignal-scoped; flag for follow-up sweep.

    auto* body = new QWidget(this);
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(4, 2, 4, 4);
    vbox->setSpacing(2);

    // --- Control 1+2: Calibrate + Auto-cal row ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        // Control 1: calibrate (non-toggle push button)
        m_calibrateBtn = styledButton(QStringLiteral("Calibrate"));
        m_calibrateBtn->setObjectName(QStringLiteral("PsAppletCalibrateBtn"));
        m_calibrateBtn->setToolTip(tr(
            "Run a single PureSignal calibration sweep. "
            "Right-click to open PureSignal..."));
        row->addWidget(m_calibrateBtn);

        // Control 2: auto-cal (green toggle)
        m_autoCalBtn = greenToggle(QStringLiteral("Auto"));
        m_autoCalBtn->setObjectName(QStringLiteral("PsAppletAutoCalBtn"));
        m_autoCalBtn->setCheckable(true);
        m_autoCalBtn->setToolTip(tr(
            "Toggle continuous PureSignal auto-calibration. "
            "Right-click to open PureSignal..."));
        row->addWidget(m_autoCalBtn);
        row->addStretch();

        vbox->addLayout(row);
    }

    // --- Control 3: Feedback level gauge (0-100, yellow@70, red@90) ---
    m_feedbackGauge = new HGauge(this);
    m_feedbackGauge->setObjectName(QStringLiteral("PsAppletFeedbackGauge"));
    m_feedbackGauge->setRange(0.0, 100.0);
    m_feedbackGauge->setYellowStart(70.0);
    m_feedbackGauge->setRedStart(90.0);
    m_feedbackGauge->setTitle(QStringLiteral("FB Level"));
    m_feedbackGauge->setToolTip(tr(
        "PureSignal feedback level. Right-click to open PureSignal..."));
    vbox->addWidget(m_feedbackGauge);

    // --- Control 4: Correction magnitude gauge (0-100, yellow@80, red@95) ---
    m_correctionGauge = new HGauge(this);
    m_correctionGauge->setObjectName(QStringLiteral("PsAppletCorrectionGauge"));
    m_correctionGauge->setRange(0.0, 100.0);
    m_correctionGauge->setYellowStart(80.0);
    m_correctionGauge->setRedStart(95.0);
    m_correctionGauge->setTitle(QStringLiteral("Correction"));
    m_correctionGauge->setToolTip(tr(
        "PureSignal correction magnitude. Right-click to open PureSignal..."));
    vbox->addWidget(m_correctionGauge);

    // --- Control 5+6+7: Save / Restore / Two-tone row ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        // Control 5: save coefficients
        m_saveBtn = styledButton(QStringLiteral("Save"));
        m_saveBtn->setObjectName(QStringLiteral("PsAppletSaveBtn"));
        m_saveBtn->setEnabled(false);  // gated on correctingChanged
        m_saveBtn->setToolTip(tr(
            "Save the current PureSignal corrections to a file. "
            "Right-click to open PureSignal..."));
        row->addWidget(m_saveBtn);

        // Control 6: restore coefficients
        m_restoreBtn = styledButton(QStringLiteral("Restore"));
        m_restoreBtn->setObjectName(QStringLiteral("PsAppletRestoreBtn"));
        m_restoreBtn->setToolTip(tr(
            "Load a previously saved PureSignal corrections file. "
            "Right-click to open PureSignal..."));
        row->addWidget(m_restoreBtn);

        // Control 7: two-tone test (green toggle)
        m_twoToneBtn = greenToggle(QStringLiteral("2-Tone"));
        m_twoToneBtn->setObjectName(QStringLiteral("PsAppletTwoToneBtn"));
        m_twoToneBtn->setCheckable(true);
        m_twoToneBtn->setToolTip(tr(
            "Inject a two-tone test signal for PureSignal calibration. "
            "Right-click to open PureSignal..."));
        row->addWidget(m_twoToneBtn);
        row->addStretch();

        vbox->addLayout(row);
    }

    // --- Control 8: Status LEDs row — "Cal", "Run", "Fbk" ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);

        const QString ledNames[3] = {
            QStringLiteral("Cal"),
            QStringLiteral("Run"),
            QStringLiteral("Fbk")
        };
        const QString ledObjectNames[3] = {
            QStringLiteral("PsAppletCalLed"),
            QStringLiteral("PsAppletRunLed"),
            QStringLiteral("PsAppletFbkLed")
        };

        for (int i = 0; i < 3; ++i) {
            m_led[i] = new QLabel(ledNames[i], this);
            m_led[i]->setObjectName(ledObjectNames[i]);
            m_led[i]->setFixedSize(24, 14);
            m_led[i]->setAlignment(Qt::AlignCenter);
            m_led[i]->setStyleSheet(QString::fromLatin1(kLedInactiveStyle));
            row->addWidget(m_led[i]);
        }
        row->addStretch();
        vbox->addLayout(row);
    }

    // --- Info readout labels ---
    m_iterations   = new QLabel(QStringLiteral("Iterations: 0"), this);
    m_iterations->setObjectName(QStringLiteral("PsAppletIterationsLabel"));
    m_feedbackDb   = new QLabel(QStringLiteral("Feedback: — dB"), this);
    m_feedbackDb->setObjectName(QStringLiteral("PsAppletFeedbackDbLabel"));
    m_correctionDb = new QLabel(QStringLiteral("Correction: — dB"), this);
    m_correctionDb->setObjectName(QStringLiteral("PsAppletCorrectionDbLabel"));

    const QString infoStyle = QStringLiteral(
        "QLabel { font-size: 10px; color: %1; }").arg(Style::kTextSecondary);
    for (QLabel* lbl : {m_iterations, m_feedbackDb, m_correctionDb}) {
        lbl->setStyleSheet(infoStyle);
        vbox->addWidget(lbl);
    }

    vbox->addStretch();
    root->addWidget(body);
}

void PureSignalApplet::setLedActive(QLabel* led, bool active)
{
    if (!led) {
        return;
    }
    led->setStyleSheet(QString::fromLatin1(active ? kLedActiveStyle
                                                  : kLedInactiveStyle));
}

void PureSignalApplet::setupRightClick(QWidget* widget)
{
    if (!widget) {
        return;
    }
    widget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(widget, &QWidget::customContextMenuRequested, this,
            [this](const QPoint&) {
        emit openPureSignalDialogRequested();
    });
}

void PureSignalApplet::wireRightClicks()
{
    // Opening the full dialog is presentation-only and never submits an action.
    setupRightClick(m_calibrateBtn);
    setupRightClick(m_autoCalBtn);
    setupRightClick(m_saveBtn);
    setupRightClick(m_restoreBtn);
    setupRightClick(m_twoToneBtn);
    setupRightClick(m_feedbackGauge);
    setupRightClick(m_correctionGauge);

    connect(m_calibrateBtn, &QPushButton::clicked, this, [this]() {
        requestAction(Ps3Action::Single);
    });
    connect(m_autoCalBtn, &QPushButton::toggled, this, [this](bool on) {
        requestAction(on ? Ps3Action::StartAutomatic : Ps3Action::OffReset);
    });
    connect(m_twoToneBtn, &QPushButton::toggled, this, [this](bool on) {
        requestAction(Ps3Action::SetTwoTone,
                      {{QStringLiteral("enabled"), on}});
    });
    connect(m_saveBtn, &QPushButton::clicked, this, [this]() {
        if (!m_facade) {
            return;
        }
        bool accepted = false;
        const QString label = QInputDialog::getText(
            this, tr("Save PureSignal correction"),
            tr("Correction label:"), QLineEdit::Normal, {}, &accepted).trimmed();
        if (accepted && !label.isEmpty()) {
            requestAction(Ps3Action::SaveCorrection,
                          {{QStringLiteral("label"), label}});
        }
    });
    connect(m_restoreBtn, &QPushButton::clicked,
            this, &PureSignalApplet::showRestoreDialog);
}

void PureSignalApplet::wireFacade()
{
    if (!m_facade) {
        return;
    }
    connect(m_facade, &PureSignalSessionFacade::statusChanged,
            this, &PureSignalApplet::refreshFromFacade);
    connect(m_facade, &PureSignalSessionFacade::actionResult, this,
            [this](quint32, Ps3ActionPhase phase, const QString&, const QVariantMap&) {
        if (phase == Ps3ActionPhase::Completed || phase == Ps3ActionPhase::Failed) {
            refreshFromFacade();
        }
    });
    if (PureSignalSettings* settings = m_facade->settings()) {
        connect(settings, &PureSignalSettings::autoCalEnabledChanged,
                this, &PureSignalApplet::refreshFromFacade);
    }
}

void PureSignalApplet::requestAction(Ps3Action action,
                                     const QVariantMap& arguments)
{
    if (!m_facade || m_facade->requestAction(action, arguments) == 0) {
        refreshFromFacade();
    }
}

void PureSignalApplet::showRestoreDialog()
{
    if (!m_model || !m_facade) {
        return;
    }
    if (!m_restoreDialog) {
        m_restoreDialog = new DspAssetDialog(
            m_model, DspAssetKind::Ps3Correction, this);
        m_restoreDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_restoreDialog, &DspAssetDialog::restoreCorrectionRequested,
                this, [this](const QString& assetId) {
            requestAction(Ps3Action::RestoreCorrection,
                          {{QStringLiteral("assetId"), assetId}});
        });
    }
    m_restoreDialog->show();
    m_restoreDialog->raise();
    m_restoreDialog->activateWindow();
}

void PureSignalApplet::refreshFromFacade()
{
    const bool available = m_facade && m_facade->available();
    const bool canActuate = available && m_facade->canActuate();
    const Ps3StatusSnapshot status = m_facade
        ? m_facade->statusSnapshot() : Ps3StatusSnapshot{};
    const bool automaticIntent = m_facade && m_facade->settings()
        ? m_facade->settings()->autoCalEnabled() : false;

    {
        const QSignalBlocker blocker(m_autoCalBtn);
        m_autoCalBtn->setChecked(automaticIntent);
    }
    {
        const QSignalBlocker blocker(m_twoToneBtn);
        m_twoToneBtn->setChecked(m_facade && m_facade->twoToneOn());
    }

    // R-R3-49 (parity Task 7): arming keys nothing and follows canArm; the
    // two-tone test keys the radio and stays on canActuate.
    const bool canArm = available && m_facade->canArm();
    const QString armingRefusal = m_facade ? m_facade->armingRefusal()
                                           : PureSignalSessionFacade::needsRadioReason();
    gateWithReason(m_calibrateBtn, canArm, armingRefusal);
    gateWithReason(m_autoCalBtn, canArm, armingRefusal);
    // Fix wave GUI-I7: the two-tone test greys with its reason too.
    gateWithReason(m_twoToneBtn, canActuate,
                   m_facade ? m_facade->twoToneRefusal()
                            : PureSignalSessionFacade::needsRadioReason());
    // Opening the station asset manager is read-only. The dialog and facade
    // gate the actual restore action on transmit permission.
    m_restoreBtn->setEnabled(available && !status.restorePending && !status.savePending);
    m_saveBtn->setEnabled(available && status.correctionsApplied
                          && !status.savePending && !status.restorePending);

    const double feedback = std::clamp(status.feedbackLevel * 100.0 / 255.0,
                                       0.0, 100.0);
    m_feedbackGauge->setValue(feedback);
    m_correctionGauge->setValue(status.correctionsApplied ? 100.0 : 0.0);
    m_iterations->setText(
        tr("Iterations: %1").arg(status.successfulCalibrations));
    m_feedbackDb->setText(
        available ? tr("Feedback: %1").arg(status.feedbackLevel)
                  : tr("Feedback: —"));
    m_correctionDb->setText(
        status.correctionsApplied ? tr("Correction: Applied")
                                  : tr("Correction: Off"));

    const bool calibrating = status.engineState == 3
        || status.engineState == 4 || status.engineState == 6;
    setLedActive(m_led[0], calibrating);
    setLedActive(m_led[1], status.correctionsApplied);
    setLedActive(m_led[2], status.mox && status.feedbackLevel > 0);
}

void PureSignalApplet::syncFromModel()
{
    refreshFromFacade();
}

} // namespace NereusSDR
