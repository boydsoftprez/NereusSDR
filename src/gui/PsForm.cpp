// =================================================================
// src/gui/PsForm.cpp  (NereusSDR)
// =================================================================
//
// Implementation of PsForm modeless dialog.  See PsForm.h for the
// design rationale and Thetis cite map.
//
// Ported from Thetis sources:
//   Project Files/Source/Console/PSForm.cs
//   Project Files/Source/Console/PSForm.designer.cs
// original licences from Thetis source are included below.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-13 — Bench instruments, Calibration/IMD workarea and diagnostics;
//                 J.J. Boyd (KG4VCF), assisted by OpenAI Codex.
//   2026-05-06 — Phase 3M-4 Task 8: created by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude
//                 Code.
//   2026-09-21 — Removed obsolete PS2-only controls for the PS3 ABI migration.
//   2026-09-25 - R-R3-49 (parity Task 7): Single Cal, Start Auto and Apply
//                 Current follow the facade's canArm (a remote window arms
//                 PureSignal on a Core at transmitSettingsVersion 7 off the
//                 air); Two-tone stays on canActuate; while the Core's
//                 radio is on the air the arming buttons and the settings
//                 grey with the reason. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-11 - A status update restyles the correction light only when
//                 its color changes. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-15 - The SetPk warning sign asks for text presentation
//                 (U+FE0E), so Windows draws it in the label's color instead
//                 of as a color emoji. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
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

#include "PsForm.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/PureSignalInstruments.h"
#include "gui/ImdMeasurementPanel.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QScreen>
#include <QScrollArea>
#include <QSpinBox>
#include <QTimer>
#include <QTabWidget>
#include <QResizeEvent>
#include <memory>
#include <QVBoxLayout>

#include "AmpViewWindow.h"
#include "DspAssetDialog.h"
#include "OperatorReasonText.h"
#include "StyleConstants.h"
#include "core/AppSettings.h"
#include "core/PureSignal.h"
#include "core/session/PureSignalSessionFacade.h"
#include "models/PureSignalSettings.h"
#include "models/RadioModel.h"

#include <algorithm>

namespace NereusSDR {

// ─────────────────────────────────────────────────────────────────────────
// Layout constants — derived from PSForm.designer.cs [v2.10.3.13].
// The Thetis dialog uses absolute positioning; NereusSDR uses
// QGridLayout in zoned bands that mirror the Thetis x/y zones.
// ─────────────────────────────────────────────────────────────────────────

// AppSettings keys.  PascalCase per CLAUDE.md → "Settings Persistence".
static const char* const kAdvancedCollapsedSettingsKey =
    "puresignal/advancedCollapsed";
static const char* const kGeometrySettingsKey =
    "puresignal/geometry";
static const char* const kAlwaysOnTopSettingsKey = "puresignal/alwaysOnTop";
static const char* const kLoopbackSettingsKey = "puresignal/displayLoopback";
static const char* const kShowTwoToneSettingsKey = "puresignal/showTwoToneMeasurements";

namespace {

// R-R3-49 (parity Task 7): grey `control` with `reason` as its tooltip,
// remembering its own tooltip and state, or put them back and apply
// `enabled`. An empty reason means no gate.
void gateWithReason(QWidget* control, bool enabled, const QString& reason)
{
    if (!control) {
        return;
    }
    static constexpr auto kSavedTooltip = "PsFormSavedTooltip";
    static constexpr auto kSavedEnabled = "PsFormSavedEnabled";
    if (!reason.isEmpty()) {
        if (!control->property(kSavedTooltip).isValid()) {
            control->setProperty(kSavedTooltip, control->toolTip());
            control->setProperty(kSavedEnabled, control->isEnabled());
        }
        control->setEnabled(false);
        control->setToolTip(reason);
        return;
    }
    if (control->property(kSavedTooltip).isValid()) {
        control->setToolTip(control->property(kSavedTooltip).toString());
        enabled = enabled && control->property(kSavedEnabled).toBool();
        control->setProperty(kSavedTooltip, QVariant());
        control->setProperty(kSavedEnabled, QVariant());
    }
    control->setEnabled(enabled);
}

} // namespace

// Bisque #FFE4C4 — colour Thetis uses for read-only data labels in the
// Calibration Information grid (PSForm.designer.cs:484-688 [v2.10.3.13]).
static QString bisqueLabelStyle()
{
    return QStringLiteral(
        "QLabel { background-color: #FFE4C4; color: black; "
        "border: 1px inset; padding: 1px 4px; min-width: 56px; }");
}

// Black status badge — initial colour for FB / CO labels per
// PSForm.designer.cs:299-307 / 311-319 [v2.10.3.13].
static QString blackBadgeStyle()
{
    return QStringLiteral(
        "QLabel { background-color: black; border: 1px inset; "
        "min-width: 12px; min-height: 12px; }");
}

// ─────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────

PsForm::PsForm(RadioModel* radioModel, PureSignal* pureSignal, QWidget* parent)
    : QDialog(parent)
    , m_radioModel(radioModel)
    , m_pureSignal(pureSignal)
{
    // From Thetis PSForm.designer.cs:898 [v2.10.3.13]:
    // PS3-native diagnostics and correction lifecycle.
    setWindowTitle(QStringLiteral("PureSignal Bench"));
    setObjectName(QStringLiteral("PsForm"));
    Style::applyDarkPageStyle(this);
    // Modeless — don't block other interaction.
    setModal(false);
    // Singleton lifecycle (matches TxEqDialog) — survive close/hide cycles.
    setAttribute(Qt::WA_DeleteOnClose, false);

    buildUi();
    if (m_radioModel) {
        m_facade = m_radioModel->pureSignalFacade();
    } else if (m_pureSignal) {
        // Standalone coordinator tests retain the historical constructor
        // shape while consuming the same session-neutral action boundary.
        m_facade = new PureSignalSessionFacade(nullptr, m_pureSignal, this);
    }
    m_settings = m_facade ? m_facade->settings()
                          : m_pureSignal ? m_pureSignal->settings() : nullptr;
    wireToPureSignal();
    syncFromPureSignal();
    restoreAdvancedMode();

    const QByteArray savedGeometry = QByteArray::fromBase64(
        AppSettings::instance().value(QLatin1String(kGeometrySettingsKey))
            .toString().toLatin1());
    if (!savedGeometry.isEmpty()) {
        restoreGeometry(savedGeometry);
    }
    QScreen* destination = nullptr;
    qint64 largestIntersection = 0;
    for (QScreen* screen : QGuiApplication::screens()) {
        if (!screen) {
            continue;
        }
        const QRect intersection = screen->availableGeometry().intersected(frameGeometry());
        const qint64 area = qint64(intersection.width()) * intersection.height();
        if (area > largestIntersection) {
            destination = screen;
            largestIntersection = area;
        }
    }
    if (!destination) {
        destination = parentWidget() && parentWidget()->screen()
            ? parentWidget()->screen() : QGuiApplication::primaryScreen();
    }
    if (destination) {
        const QRect available = destination->availableGeometry();
        const int safeWidth = qMin(qMax(minimumWidth(), width()), available.width());
        const int safeHeight = qMin(qMax(minimumHeight(), height()), available.height());
        resize(safeWidth, safeHeight);
        const int x = qBound(available.left(), frameGeometry().left(),
            qMax(available.left(), available.right() - frameGeometry().width() + 1));
        const int y = qBound(available.top(), frameGeometry().top(),
            qMax(available.top(), available.bottom() - frameGeometry().height() + 1));
        move(x + geometry().left() - frameGeometry().left(),
             y + geometry().top() - frameGeometry().top());
    }

    const bool stayOnTop = AppSettings::instance()
        .value(QLatin1String(kAlwaysOnTopSettingsKey), false).toBool();
    {
        const QSignalBlocker blocker(m_chkOnTop);
        m_chkOnTop->setChecked(stayOnTop);
    }
    setWindowFlag(Qt::WindowStaysOnTopHint, stayOnTop);

    // Workspace preview: the scoped stylesheet pass runs each time the
    // dialog is shown (preview scaffolding; nothing with the switch off).
    WorkspacePreview::themeOnShow(this);
}

PsForm::~PsForm() = default;

// ─────────────────────────────────────────────────────────────────────────
// UI construction
// ─────────────────────────────────────────────────────────────────────────

void PsForm::buildUi()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(12);
    setMinimumWidth(520);
    QFont readable = PureSignalViewStyle::font(font());
    readable.setPixelSize(12);
    setFont(readable);
    m_benchBody = new QWidget(this);
    m_benchBody->setObjectName(QStringLiteral("PureSignalBenchBody"));
    m_benchLayout = new QGridLayout(m_benchBody);
    m_benchLayout->setContentsMargins(0, 0, 0, 0);
    m_benchLayout->setSpacing(18);
    m_instruments = new PureSignalInstruments(QStringLiteral("PsBench"), false, m_benchBody);
    m_instruments->setMinimumWidth(240);
    auto* tabs = new QTabWidget(m_benchBody);
    tabs->setObjectName(QStringLiteral("PureSignalBenchTabs"));
    auto* calibration = new QWidget(tabs);
    auto* calibrationLayout = new QVBoxLayout(calibration);
    calibrationLayout->setContentsMargins(14, 16, 14, 14);
    calibrationLayout->setSpacing(16);
    auto section = [calibration](const QString& title) {
        auto* label = new QLabel(title, calibration);
        QFont sectionFont = PureSignalViewStyle::font(label->font());
        sectionFont.setPixelSize(12);
        sectionFont.setWeight(QFont::Medium);
        label->setFont(sectionFont);
        return label;
    };
    calibrationLayout->addWidget(section(tr("Feedback control")));
    auto* feedbackOptions = new QVBoxLayout;
    feedbackOptions->setSpacing(10);
    calibrationLayout->addLayout(feedbackOptions);
    calibrationLayout->addWidget(section(tr("Timing")));
    auto* processing = new QVBoxLayout;
    processing->setSpacing(10);
    auto* imd = new QWidget(tabs);
    auto* imdLayout = new QVBoxLayout(imd);
    imdLayout->setContentsMargins(0, 12, 0, 0);
    m_imdPanel = new ImdMeasurementPanel(imd);
    m_imdPanel->setObjectName(QStringLiteral("PureSignalBenchImd"));
    m_imdPanel->setPresentation(ImdMeasurementPanel::Presentation::Window);
    tabs->addTab(calibration, tr("Calibration"));
    tabs->addTab(imd, tr("IMD measurements"));
    m_benchLayout->addWidget(m_instruments, 0, 0);
    m_benchLayout->addWidget(tabs, 0, 1);
    m_benchLayout->setColumnStretch(1, 1);
    auto* diagnostics = new QWidget(this);
    diagnostics->setObjectName(QStringLiteral("PureSignalBenchDiagnostics"));
    auto* diagnosticsLayout = new QVBoxLayout(diagnostics);
    diagnosticsLayout->setContentsMargins(0, 0, 0, 0);
    diagnosticsLayout->setSpacing(8);
    auto* footer = new QWidget(this);
    footer->setObjectName(QStringLiteral("PureSignalBenchFooter"));
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(0, 0, 0, 0);
    footerLayout->setSpacing(8);

    // ── Top action row (always visible, even in collapsed Advanced mode) ──
    //
    // Per PSForm.designer.cs [v2.10.3.13] X-positions (left → right):
    //   btnPSTwoToneGen  X=14  W=71
    //   btnPSCalibrate   X=91  W=71
    //   btnPSAmpView     X=168 W=71
    //   btnPSAdvanced    X=245 W=71
    //   btnPSSave        X=322 W=71
    //   btnPSRestore     X=399 W=71
    //   btnPSReset       X=476 W=71
    auto* topRow = new QGridLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(6);
    // Workspace button styling can report a 24px sizeHint even when the
    // button minimum is 30px. Reserve the full control height in each row.
    topRow->setRowMinimumHeight(0, 36);
    topRow->setRowMinimumHeight(1, 36);

    m_btnTwoTone = new QPushButton(tr("Two-tone"), this);
    m_btnTwoTone->setObjectName(QStringLiteral("btnPSTwoToneGen"));
    m_btnTwoTone->setCheckable(true);
    m_btnTwoTone->setToolTip(tr("Generate and TX a Two Tone Signal"));
    topRow->addWidget(m_btnTwoTone, 0, 0);

    m_btnSingleCal = new QPushButton(tr("Single Cal"), this);
    m_btnSingleCal->setObjectName(QStringLiteral("btnPSCalibrate"));
    m_btnSingleCal->setToolTip(
        tr("Perform a singal calibration. This will happen up to 5 times in a row"));
    topRow->addWidget(m_btnSingleCal, 0, 1);

    m_btnAutomatic = new QPushButton(tr("Start Auto"), this);
    m_btnAutomatic->setObjectName(QStringLiteral("btnPSAutomatic"));
    m_btnAutomatic->setToolTip(tr("Start automatic PureSignal calibration."));
    topRow->addWidget(m_btnAutomatic, 0, 2);

    m_btnApplyCurrent = new QPushButton(tr("Apply Current"), this);
    m_btnApplyCurrent->setObjectName(QStringLiteral("btnPSApplyCurrent"));
    m_btnApplyCurrent->setToolTip(tr("Apply the correction currently held by PureSignal."));
    topRow->addWidget(m_btnApplyCurrent, 1, 0);

    m_btnAmpView = new QPushButton(tr("AmpView"), this);
    m_btnAmpView->setObjectName(QStringLiteral("btnPSAmpView"));
    topRow->addWidget(m_btnAmpView, 1, 1);

    m_btnAdvanced = new QPushButton(tr("Advanced"), this);
    m_btnAdvanced->setObjectName(QStringLiteral("btnPSAdvanced"));
    topRow->addWidget(m_btnAdvanced, 1, 2);

    m_btnSave = new QPushButton(tr("Save"), this);
    m_btnSave->setObjectName(QStringLiteral("btnPSSave"));
    // From Thetis PSForm.cs:574-590 [v2.10.3.13] — Save is gated on
    // CorrectionsBeingApplied; starts disabled until corrections land.
    m_btnSave->setEnabled(false);
    footerLayout->addWidget(m_btnSave);

    m_btnRestore = new QPushButton(tr("Restore"), this);
    m_btnRestore->setObjectName(QStringLiteral("btnPSRestore"));
    footerLayout->addWidget(m_btnRestore);

    m_btnReset = new QPushButton(tr("OFF"), this);
    m_btnReset->setObjectName(QStringLiteral("btnPSReset"));
    topRow->addWidget(m_btnReset, 0, 3);

    outer->addLayout(topRow);

    // ── Status row: badges + Show 2Tone measurements ──────────────────────
    //
    // Per PSForm.designer.cs:285-330, 846-857 [v2.10.3.13]:
    //   labelTS8 "Feedback Level" at X=32 Y=40
    //   lblPSInfoFB at X=14 Y=41 (12x12 badge)
    //   lblPSInfoCO at X=168 Y=41 (12x12 badge)
    //   labelTS9 "Correcting" at X=186 Y=40
    //   chkShow2ToneMeasurements at X=389 Y=38
    auto* statusRow = new QHBoxLayout();
    statusRow->setContentsMargins(0, 0, 0, 0);
    statusRow->setSpacing(6);

    m_lblFb = new QLabel(this);
    m_lblFb->setObjectName(QStringLiteral("lblPSInfoFB"));
    m_lblFb->setFixedSize(12, 12);
    m_lblFb->setStyleSheet(blackBadgeStyle());
    m_lblFb->setToolTip(
        tr("Indicates, by color, correct/incorrect RF feedback level"));
    statusRow->addWidget(m_lblFb);

    auto* lblFbText = new QLabel(tr("Feedback Level"), this);
    lblFbText->setObjectName(QStringLiteral("labelTS8"));
    lblFbText->setToolTip(
        tr("Indicates, by color, correct/incorrect RF feedback level"));
    statusRow->addWidget(lblFbText);

    statusRow->addSpacing(20);

    m_lblCo = new QLabel(this);
    m_lblCo->setObjectName(QStringLiteral("lblPSInfoCO"));
    m_lblCo->setFixedSize(12, 12);
    m_lblCo->setStyleSheet(blackBadgeStyle());
    m_lblCo->setToolTip(tr(
        "Green: correction applied and calibration feedback >90; yellow: applied with feedback ≤90"));
    statusRow->addWidget(m_lblCo);

    auto* lblCoText = new QLabel(tr("Correction applied / feedback >90"), this);
    lblCoText->setObjectName(QStringLiteral("labelTS9"));
    lblCoText->setToolTip(tr(
        "Green: correction applied and calibration feedback >90; yellow: applied with feedback ≤90"));
    statusRow->addWidget(lblCoText);

    statusRow->addStretch();

    // From PSForm.designer.cs:835-844 pbWarningSetPk [v2.10.3.13] — hidden
    // by default, shown when current SetPk drifts from psDefaultPeak.
    // NereusSDR uses a QLabel with a "!" glyph stand-in until artwork
    // import lands.
    // U+FE0E asks for the text form of the sign: without it Windows draws
    // U+26A0 from Segoe UI Emoji as a color picture.
    m_lblWarningSetPk = new QLabel(QStringLiteral("\u26A0\uFE0E"), this);
    m_lblWarningSetPk->setObjectName(QStringLiteral("pbWarningSetPk"));
    m_lblWarningSetPk->setToolTip(
        tr("SetPk has drifted from the per-board default."));
    m_lblWarningSetPk->setFixedSize(20, 20);
    m_lblWarningSetPk->setVisible(false);
    statusRow->addWidget(m_lblWarningSetPk);

    m_chkShow2ToneMeasurements =
        new QCheckBox(tr("Show 2Tone measurements"), this);
    m_chkShow2ToneMeasurements->setObjectName(
        QStringLiteral("chkShow2ToneMeasurements"));
    imdLayout->addWidget(m_chkShow2ToneMeasurements);
    imdLayout->addWidget(m_imdPanel);
    imdLayout->addStretch();

    diagnosticsLayout->addLayout(statusRow);

    m_lblActionStatus = new QLabel(tr("No PureSignal action pending."), this);
    m_lblActionStatus->setObjectName(QStringLiteral("lblPSActionStatus"));
    m_lblActionStatus->setWordWrap(true);
    diagnosticsLayout->addWidget(m_lblActionStatus);

    m_lblNativeStatus = new QLabel(this);
    m_lblNativeStatus->setObjectName(QStringLiteral("lblPSNativeStatus"));
    m_lblNativeStatus->setWordWrap(true);
    diagnosticsLayout->addWidget(m_lblNativeStatus);

    m_lblRoutingStatus = new QLabel(this);
    m_lblRoutingStatus->setObjectName(QStringLiteral("lblPSRoutingStatus"));
    m_lblRoutingStatus->setWordWrap(true);
    diagnosticsLayout->addWidget(m_lblRoutingStatus);

    // ── Body: 3-column grid ───────────────────────────────────────────────
    //
    // Column 0: timing labels       (MOX Wait / CAL Wait / AMP Delay)
    // Column 1: timing spinboxes
    // Column 2: retained calibration options (Auto-Attenuate / Quick)
    auto* body = new QGridLayout();
    body->setContentsMargins(0, 0, 0, 0);
    body->setHorizontalSpacing(8);
    body->setVerticalSpacing(4);

    // Row 0: MOX Wait + Auto-Attenuate
    auto* lblMox = new QLabel(tr("MOX Wait (sec)"), this);
    lblMox->setObjectName(QStringLiteral("labelTS4"));
    body->addWidget(lblMox, 0, 0);

    m_spinMoxDelay = new QDoubleSpinBox(this);
    m_spinMoxDelay->setObjectName(QStringLiteral("udPSMoxDelay"));
    // From Thetis PSForm.Designer.cs:346-372 [v2.10.3.15]:
    //   DecimalPlaces=1, Increment=0.1, Maximum=1.0, Minimum=0.1,
    //   Value=0.2.  (Decimal arrays {N,0,0,65536} = N / 10^1, so
    //   Maximum {10,..} is 1.0 and Value {2,..} is 0.2.)
    // Fix wave RD-I7: the earlier port read them as 10.0 and 2.0.
    m_spinMoxDelay->setDecimals(1);
    m_spinMoxDelay->setSingleStep(0.1);
    m_spinMoxDelay->setMinimum(0.1);
    m_spinMoxDelay->setMaximum(1.0);
    m_spinMoxDelay->setValue(0.2);
    m_spinMoxDelay->setToolTip(
        tr("Settling time between assertion of MOX and collection of feedback"));
    body->addWidget(m_spinMoxDelay, 0, 1);

    m_chkAutoAttenuate = new QCheckBox(tr("Auto-Attenuate"), this);
    m_chkAutoAttenuate->setObjectName(QStringLiteral("chkPSAutoAttenuate"));
    m_chkAutoAttenuate->setChecked(true);  // Designer default Checked
    m_chkAutoAttenuate->setToolTip(
        tr("Automatically adjust attenuator for optimum feedback level. (Recommended)"));
    feedbackOptions->addWidget(m_chkAutoAttenuate);

    m_chkAutoCalEnabled = new QCheckBox(tr("Automatic desired"), this);
    m_chkAutoCalEnabled->setObjectName(QStringLiteral("chkPSAutoCalEnabled"));
    m_chkAutoCalEnabled->setToolTip(
        tr("Remember whether automatic calibration should resume when the station is ready."));
    processing->addWidget(m_chkAutoCalEnabled);

    // Row 1: CAL Wait
    auto* lblCal = new QLabel(tr("CAL Wait (sec)"), this);
    lblCal->setObjectName(QStringLiteral("labelTS140"));
    body->addWidget(lblCal, 1, 0);

    m_spinCalDelay = new QDoubleSpinBox(this);
    m_spinCalDelay->setObjectName(QStringLiteral("udPSCalWait"));
    // From PSForm.designer.cs:778-805 [v2.10.3.13]:
    //   DecimalPlaces=1, Increment=0.1, Minimum=0, Maximum=100, Value=0.
    m_spinCalDelay->setDecimals(1);
    m_spinCalDelay->setSingleStep(0.1);
    m_spinCalDelay->setMinimum(0.0);
    m_spinCalDelay->setMaximum(100.0);
    m_spinCalDelay->setValue(0.0);
    m_spinCalDelay->setToolTip(tr(
        "Time to wait between calculating correction solutions.  (Zero for "
        "fastest response.)"));
    body->addWidget(m_spinCalDelay, 1, 1);

    m_chkRunCalibrationProcessing = new QCheckBox(tr("Run calibration processing"), this);
    m_chkRunCalibrationProcessing->setObjectName(
        QStringLiteral("chkPSRunCalibrationProcessing"));
    processing->addWidget(m_chkRunCalibrationProcessing);

    // Row 2: AMP Delay + Quick Attenuate Response
    auto* lblAmp = new QLabel(tr("AMP Delay (ns)"), this);
    lblAmp->setObjectName(QStringLiteral("labelTS2"));
    body->addWidget(lblAmp, 2, 0);

    m_spinAmpDelay = new QSpinBox(this);
    m_spinAmpDelay->setObjectName(QStringLiteral("udPSPhnum"));
    // From PSForm.designer.cs:388-413 [v2.10.3.13]:
    //   Increment=20, Minimum=0, Maximum=25_000_000, Value=150.
    m_spinAmpDelay->setSingleStep(20);
    m_spinAmpDelay->setMinimum(0);
    m_spinAmpDelay->setMaximum(25000000);
    m_spinAmpDelay->setValue(150);
    m_spinAmpDelay->setToolTip(tr("Compensation delay for the analog PA chain"));
    body->addWidget(m_spinAmpDelay, 2, 1);

    m_chkQuickAttenuate = new QCheckBox(tr("Quick Attenuate Response"), this);
    m_chkQuickAttenuate->setObjectName(QStringLiteral("chkQuickAttenuate"));
    m_chkQuickAttenuate->setToolTip(
        tr("Apply auto attenuation changes at a faster interval"));
    feedbackOptions->addWidget(m_chkQuickAttenuate);

    m_chkHardwarePeakOverride = new QCheckBox(tr("Override hardware peak"), this);
    m_chkHardwarePeakOverride->setObjectName(
        QStringLiteral("chkPSHardwarePeakOverride"));
    processing->addWidget(m_chkHardwarePeakOverride);

    calibrationLayout->addLayout(body);
    calibrationLayout->addWidget(section(tr("Calibration processing")));
    calibrationLayout->addLayout(processing);

    // ── Bottom row: Loopback (left) + Always On Top (right) ───────────────
    auto* bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 0, 0, 0);
    bottomRow->setSpacing(6);

    m_chkLoopback =
        new QCheckBox(tr("Display PS-RX and PS-TX spectra"), this);
    m_chkLoopback->setObjectName(QStringLiteral("checkLoopback"));
    m_chkLoopback->setToolTip(
        tr("Separate feedback spectra are unavailable in this build; "
           "use AmpView for PS3 diagnostics."));
    m_chkLoopback->setEnabled(false);
    bottomRow->addWidget(m_chkLoopback);

    bottomRow->addStretch();

    m_chkOnTop = new QCheckBox(tr("Always On Top"), this);
    m_chkOnTop->setObjectName(QStringLiteral("chkPSOnTop"));
    footerLayout->addStretch();
    footerLayout->addWidget(m_chkOnTop);

    diagnosticsLayout->addLayout(bottomRow);

    // ── Calibration Information group (advanced section) ──────────────────
    m_grpCalInfo = buildCalibrationInfoGroup(this);
    diagnosticsLayout->insertWidget(0, m_grpCalInfo);
    calibrationLayout->addWidget(m_peakReference);
    calibrationLayout->addStretch();
    auto* scrollContents = new QWidget(this);
    auto* scrollLayout = new QVBoxLayout(scrollContents);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(12);
    scrollLayout->addWidget(m_benchBody);
    scrollLayout->addWidget(diagnostics);
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("PureSignalBenchScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setWidget(scrollContents);
    scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    outer->addWidget(scroll, 1);
    outer->addWidget(footer);

    outer->addStretch();

    // ── Build advanced-section widget vector ──────────────────────────────
    //
    // Everything below the top action row hides when the Advanced toggle
    // collapses the form.  Mirrors PSForm.cs:894-902 [v2.10.3.13]:
    //   if (_advancedON) ClientSize = (560,60);  // top row only
    //   else             ClientSize = (560,300); // full form
    m_advancedSectionWidgets.append(scroll);
    m_advancedSectionWidgets.append(footer);
    for (QPushButton* button : findChildren<QPushButton*>()) {
        button->setAutoDefault(false);
        button->setMinimumHeight(30);
    }
    resize(920, 830);

    // ── Wire button signals to slots ──────────────────────────────────────
    connect(m_btnSingleCal,    &QPushButton::clicked,
            this, &PsForm::onSingleCalibrate);
    connect(m_btnAutomatic,    &QPushButton::clicked,
            this, &PsForm::onAutomaticCalibrate);
    connect(m_btnApplyCurrent, &QPushButton::clicked,
            this, &PsForm::onApplyCurrentCorrection);
    connect(m_btnAdvanced,     &QPushButton::clicked,
            this, &PsForm::onAdvancedClicked);
    connect(m_btnSave,         &QPushButton::clicked,
            this, &PsForm::onSavePressed);
    connect(m_btnRestore,      &QPushButton::clicked,
            this, &PsForm::onRestorePressed);
    connect(m_btnTwoTone,      &QPushButton::toggled,
            this, &PsForm::onTwoToneToggled);
    connect(m_btnReset,        &QPushButton::clicked,
            this, &PsForm::onResetClicked);
    connect(m_btnAmpView,      &QPushButton::clicked,
            this, &PsForm::onAmpViewClicked);
    connect(m_btnDefaultPeaks, &QPushButton::clicked,
            this, &PsForm::onDefaultPeaksClicked);
    // PR #212 follow-up bench fix: PSpeak text edit → PureSignal::setHwPeak.
    // Mirrors Thetis PSForm.cs:787-794 PSpeak_TextChanged [v2.10.3.13].
    // editingFinished fires on Enter or focus loss — matches WinForms
    // TextChanged-followed-by-Validating semantics.
    connect(m_txtPSpeak, &QLineEdit::editingFinished,
            this, &PsForm::onPSpeakEditingFinished);

    // ── Wire toggle signals ───────────────────────────────────────────────
    connect(m_chkOnTop,                &QCheckBox::toggled,
            this, &PsForm::onAlwaysOnTopToggled);
    connect(m_chkAutoAttenuate,        &QCheckBox::toggled,
            this, &PsForm::onAutoAttenuateToggled);
    connect(m_chkQuickAttenuate,       &QCheckBox::toggled,
            this, &PsForm::onQuickAttenuateToggled);
    connect(m_chkAutoCalEnabled,       &QCheckBox::toggled,
            this, &PsForm::onAutoCalEnabledToggled);
    connect(m_chkRunCalibrationProcessing, &QCheckBox::toggled,
            this, &PsForm::onRunCalibrationProcessingToggled);
    connect(m_chkHardwarePeakOverride, &QCheckBox::toggled,
            this, &PsForm::onHardwarePeakOverrideToggled);
    connect(m_chkLoopback,             &QCheckBox::toggled,
            this, &PsForm::onLoopbackToggled);
    connect(m_chkShow2ToneMeasurements, &QCheckBox::toggled,
            this, &PsForm::onShow2ToneMeasurementsToggled);

    // ── Wire spinbox / combo signals ──────────────────────────────────────
    connect(m_spinMoxDelay,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PsForm::onMoxDelayChanged);
    connect(m_spinCalDelay,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PsForm::onCalDelayChanged);
    connect(m_spinAmpDelay,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PsForm::onAmpDelayChanged);
}

// ─────────────────────────────────────────────────────────────────────────
// Calibration Information group (advanced section)
//
// Per PSForm.designer.cs:416-735 [v2.10.3.13], the grpPSInfo is a
// 358×148 group box at (14,145) holding:
//   Column 0: bldr.rx / bldr.cm / bldr.cc / bldr.cs labels + lblPSInfo0..3
//   Column 1: state / feedbk / sln.chk / dg.cnt / cor.cnt labels +
//             lblPSInfo15 / lblPSfb2 / lblPSInfo6 / lblPSInfo13 /
//             lblPSInfo5
//   Column 2: GetPk / SetPk labels + GetPSpeak / txtPSpeak text boxes
//   Bottom:   checkLoopback (left) + btnDefaultPeaks (right)
//
// NereusSDR uses QGridLayout to mirror the 3-column zone layout.
// ─────────────────────────────────────────────────────────────────────────

QGroupBox* PsForm::buildCalibrationInfoGroup(QWidget* parent)
{
    auto* grp = new QGroupBox(tr("Calibration details"), parent);
    grp->setObjectName(QStringLiteral("grpPSInfo"));

    auto* g = new QGridLayout(grp);
    g->setHorizontalSpacing(10);
    g->setVerticalSpacing(2);

    auto makeIndicatorLabel = [&](const QString& objectName) {
        auto* lbl = new QLabel(QStringLiteral("0"), grp);
        lbl->setObjectName(objectName);
        lbl->setStyleSheet(bisqueLabelStyle());
        lbl->setMinimumWidth(56);
        return lbl;
    };

    // Column 0: bldr.* row labels + lblPSInfo0..3
    auto* lblBldrRx = new QLabel(tr("bldr.rx"), grp);
    lblBldrRx->setToolTip(tr(
        "Indicator:  build of feedback magnitude curve.  (Normal = 0)"));
    g->addWidget(lblBldrRx, 0, 0);
    m_lblInfo0 = makeIndicatorLabel(QStringLiteral("lblPSInfo0"));
    m_lblInfo0->setToolTip(lblBldrRx->toolTip());
    g->addWidget(m_lblInfo0, 0, 1);

    auto* lblBldrCm = new QLabel(tr("bldr.cm"), grp);
    lblBldrCm->setToolTip(tr(
        "Indicator:  build of magnitude correction curve.  (Normal = 0)"));
    g->addWidget(lblBldrCm, 1, 0);
    m_lblInfo1 = makeIndicatorLabel(QStringLiteral("lblPSInfo1"));
    m_lblInfo1->setToolTip(lblBldrCm->toolTip());
    g->addWidget(m_lblInfo1, 1, 1);

    auto* lblBldrCc = new QLabel(tr("bldr.cc"), grp);
    lblBldrCc->setToolTip(tr(
        "Indicator:  build of cosine correction curve.  (Normal = 0)"));
    g->addWidget(lblBldrCc, 2, 0);
    m_lblInfo2 = makeIndicatorLabel(QStringLiteral("lblPSInfo2"));
    m_lblInfo2->setToolTip(lblBldrCc->toolTip());
    g->addWidget(m_lblInfo2, 2, 1);

    auto* lblBldrCs = new QLabel(tr("bldr.cs"), grp);
    lblBldrCs->setToolTip(tr(
        "Indicator:  build of sine correction curve.  (Normal = 0)"));
    g->addWidget(lblBldrCs, 3, 0);
    m_lblInfo3 = makeIndicatorLabel(QStringLiteral("lblPSInfo3"));
    m_lblInfo3->setToolTip(lblBldrCs->toolTip());
    g->addWidget(m_lblInfo3, 3, 1);

    // Column 1: state / feedbk / sln.chk / dg.cnt / cor.cnt labels
    auto* lblState = new QLabel(tr("state"), grp);
    lblState->setToolTip(tr(
        "Indicator:  indicates what PureSignal is doing at the present time."));
    g->addWidget(lblState, 0, 2);
    m_lblInfo15 = makeIndicatorLabel(QStringLiteral("lblPSInfo15"));
    m_lblInfo15->setToolTip(lblState->toolTip());
    g->addWidget(m_lblInfo15, 0, 3);

    auto* lblFeedbk = new QLabel(tr("feedbk"), grp);
    lblFeedbk->setToolTip(tr(
        "Latest calibration feedback level; may be retained between collections."));
    g->addWidget(lblFeedbk, 1, 2);
    m_lblFb2 = makeIndicatorLabel(QStringLiteral("lblPSfb2"));
    m_lblFb2->setToolTip(lblFeedbk->toolTip());
    g->addWidget(m_lblFb2, 1, 3);

    auto* lblSlnChk = new QLabel(tr("sln.chk"), grp);
    lblSlnChk->setToolTip(tr(
        "Indicator:  code indicating evaluation of correction solution. "
        "(Normal = 0)"));
    g->addWidget(lblSlnChk, 2, 2);
    m_lblInfo6 = makeIndicatorLabel(QStringLiteral("lblPSInfo6"));
    m_lblInfo6->setToolTip(lblSlnChk->toolTip());
    g->addWidget(m_lblInfo6, 2, 3);

    auto* lblDgCnt = new QLabel(tr("dg.cnt"), grp);
    lblDgCnt->setToolTip(tr(
        "Indicator:  number of rejected sample sets (Normal <= 2)"));
    g->addWidget(lblDgCnt, 3, 2);
    m_lblInfo13 = makeIndicatorLabel(QStringLiteral("lblPSInfo13"));
    m_lblInfo13->setToolTip(lblDgCnt->toolTip());
    g->addWidget(m_lblInfo13, 3, 3);

    auto* lblCorCnt = new QLabel(tr("Successful calibrations"), grp);
    lblCorCnt->setToolTip(tr(
        "Indicator: cumulative successful calibrations (native info[5])."));
    g->addWidget(lblCorCnt, 4, 2);
    m_lblInfo5 = makeIndicatorLabel(QStringLiteral("lblPSInfo5"));
    m_lblInfo5->setToolTip(lblCorCnt->toolTip());
    g->addWidget(m_lblInfo5, 4, 3);

    // Column 2: GetPk / SetPk labels + read-only mirror labels
    auto* lblGetPk = new QLabel(tr("GetPk"), grp);
    g->addWidget(lblGetPk, 1, 4);
    m_lblGetPSpeak = new QLabel(grp);
    m_lblGetPSpeak->setObjectName(QStringLiteral("GetPSpeak"));
    m_lblGetPSpeak->setStyleSheet(QStringLiteral("padding: 4px;"));
    m_lblGetPSpeak->setToolTip(
        tr("Indicator:  Peak level of measured digital TX feedback."));
    g->addWidget(m_lblGetPSpeak, 1, 5);

    auto* lblSetPk = new QLabel(tr("SetPk"), grp);
    g->addWidget(lblSetPk, 2, 4);
    // PR #212 follow-up bench fix (J.J. KG4VCF, 2026-05-07):
    // SetPk converted from QLabel (read-only mirror) to QLineEdit (user-
    // editable) — matches Thetis PSForm.designer.cs:573-582 txtPSpeak which
    // is a TextBox.  Wired to PureSignal::setHwPeak via editingFinished so
    // bench-tuned values stick (HL2 + N2ADR users typically need ~0.117 vs
    // the spec 0.233 default — the AutoAtt loop can't converge on hardware
    // where the feedback signal differs from spec without this).
    // From Thetis PSForm.cs:787-794 PSpeak_TextChanged [v2.10.3.13]:
    //   private void PSpeak_TextChanged(object sender, EventArgs e) {
    //       bool bOk = double.TryParse(txtPSpeak.Text, out double tmp);
    //       if (bOk) {
    //           _PShwpeak = tmp;
    //           puresignal.SetPSHWPeak(_txachannel, _PShwpeak);
    //           UpdateWarningSetPk();
    //       }
    //   }
    m_txtPSpeak = new QLineEdit(grp);
    m_txtPSpeak->setObjectName(QStringLiteral("txtPSpeak"));
    m_txtPSpeak->setStyleSheet(QStringLiteral("padding: 4px;"));
    m_txtPSpeak->setToolTip(tr(
        "PS hardware peak (editable).  Peak level of expected digital TX "
        "feedback for the current hardware.  Should be close to GetPk; can "
        "be tuned manually for non-recognized hardware/firmware or unusual "
        "feedback paths (e.g. HL2 + N2ADR + amp benches typically need "
        "~0.117 vs the 0.233 HL2 default).  Press Enter or click outside "
        "the field to apply."));
    m_txtPSpeak->setMaximumWidth(80);
    g->addWidget(m_txtPSpeak, 2, 5);

    // Default Peaks button
    m_btnDefaultPeaks = new QPushButton(tr("Default"), grp);
    m_btnDefaultPeaks->setObjectName(QStringLiteral("btnDefaultPeaks"));
    m_btnDefaultPeaks->setToolTip(tr(
        "Set the default peak level of expected digital TX feedback for the "
        "current hardware"));
    g->addWidget(m_btnDefaultPeaks, 4, 5);

    // PS3 exposes sixteen native status words. Keep the source-era named
    // indicators above and add the complete bounded readback for diagnosis.
    m_rawInfoLabels[0] = m_lblInfo0;
    m_rawInfoLabels[1] = m_lblInfo1;
    m_rawInfoLabels[2] = m_lblInfo2;
    m_rawInfoLabels[3] = m_lblInfo3;
    m_rawInfoLabels[5] = m_lblInfo5;
    m_rawInfoLabels[6] = m_lblInfo6;
    m_rawInfoLabels[13] = m_lblInfo13;
    m_rawInfoLabels[15] = m_lblInfo15;
    for (int i = 0; i < 16; ++i) {
        if (m_rawInfoLabels[static_cast<std::size_t>(i)]) {
            continue;
        }
        const int row = 5 + (i / 4);
        const int column = (i % 4) * 2;
        auto* name = new QLabel(tr("raw[%1]").arg(i), grp);
        m_rawInfoLabels[static_cast<std::size_t>(i)] =
            makeIndicatorLabel(QStringLiteral("lblPSInfo%1").arg(i));
        g->addWidget(name, row, column);
        g->addWidget(m_rawInfoLabels[static_cast<std::size_t>(i)], row, column + 1);
    }

    // Preserve the source-era controls, reorganizing their presentation only.
    // In PS3, info[5]/info[7] are successes/attempts; curve points are not solutions.
    m_peakReference = new QGroupBox(tr("Hardware peak reference"), parent);
    m_peakReference->setObjectName(QStringLiteral("PureSignalPeakReference"));
    auto* peaks = new QGridLayout(m_peakReference);
    peaks->setHorizontalSpacing(12);
    peaks->setVerticalSpacing(10);
    auto* explanation = new QLabel(tr("Digital feedback reference, not distortion improvement. "
        "GetPk: native HWPeak readback · SetPk: expected peak override."), m_peakReference);
    explanation->setWordWrap(true);
    peaks->addWidget(explanation, 0, 0, 1, 3);
    peaks->addWidget(lblGetPk, 1, 0);
    peaks->addWidget(m_lblGetPSpeak, 1, 1);
    peaks->addWidget(lblSetPk, 2, 0);
    peaks->addWidget(m_txtPSpeak, 2, 1);
    peaks->addWidget(m_btnDefaultPeaks, 2, 2);
    while (g->count() > 0) {
        std::unique_ptr<QLayoutItem> item(g->takeAt(0));
        if (item->widget()) { item->widget()->hide(); }
    }
    const QStringList names{tr("Feedback curve"), tr("Magnitude curve"), tr("Cosine curve"),
        tr("Sine curve"), tr("Calibration feedback"), tr("Successful calibrations"),
        tr("Solution check"), tr("Calibration attempts"), tr("Raw 8"), tr("Raw 9"),
        tr("Raw 10"), tr("Raw 11"), tr("File status"), tr("Raw 13"),
        tr("Applied code"), tr("Engine code")};
    for (int i = 0; i < 16; ++i) {
        auto* name = new QLabel(tr("%1 · [%2]").arg(names[i]).arg(i), grp);
        const int column = (i % 4) * 2;
        const int row = i / 4;
        g->addWidget(name, row, column);
        QLabel* value = m_rawInfoLabels[static_cast<std::size_t>(i)];
        value->setMinimumWidth(28);
        value->setStyleSheet(QStringLiteral("QLabel { background: %1; color: %2; padding: 3px 4px; border-radius: 3px; }")
            .arg(PureSignalViewStyle::background(value).name(), PureSignalViewStyle::text().name()));
        g->addWidget(value, row, column + 1);
        value->show();
    }
    m_lblFb2->setObjectName(QStringLiteral("lblPSfb2"));
    m_lblFb2->hide(); // Complete raw[4] above is the exposed calibration level.
    return grp;
}

// ─────────────────────────────────────────────────────────────────────────
// Wire to PureSignal coordinator
// ─────────────────────────────────────────────────────────────────────────

void PsForm::wireToPureSignal()
{
    if (m_facade) {
        connect(m_facade, &PureSignalSessionFacade::statusChanged,
                this, &PsForm::refreshFacadeStatus);
        connect(m_facade, &PureSignalSessionFacade::actionResult,
                this, &PsForm::onActionResult);
        connect(m_facade, &PureSignalSessionFacade::displayInvalidated,
                this, &PsForm::onSessionInvalidated);
    }
    if (m_settings) {
        const auto sync = [this]() { syncAcceptedSettings(); };
        connect(m_settings, &PureSignalSettings::autoCalEnabledChanged, this, sync);
        connect(m_settings, &PureSignalSettings::runCalibrationProcessingChanged, this, sync);
        connect(m_settings, &PureSignalSettings::autoAttenuateChanged, this, sync);
        connect(m_settings, &PureSignalSettings::quickAttenuateChanged, this, sync);
        connect(m_settings, &PureSignalSettings::moxDelaySecondsChanged, this, sync);
        connect(m_settings, &PureSignalSettings::loopDelaySecondsChanged, this, sync);
        connect(m_settings, &PureSignalSettings::requestedTxDelayNsChanged, this, sync);
        connect(m_settings, &PureSignalSettings::hardwarePeakOverrideEnabledChanged, this, sync);
        connect(m_settings, &PureSignalSettings::hardwarePeakOverrideChanged, this, sync);
        connect(m_settings, &PureSignalSettings::editRejected, this,
                [this](const QString& reason) {
            syncAcceptedSettings();
            if (m_lblActionStatus) {
                m_lblActionStatus->setText(tr("Setting rejected: %1")
                                                .arg(OperatorReasonText::forDisplay(reason)));
            }
        });
    }
}

void PsForm::syncFromPureSignal()
{
    syncAcceptedSettings();
    if (m_pureSignal && !m_radioModel) {
        const QSignalBlocker loopbackBlocker(m_chkLoopback);
        const QSignalBlocker measurementsBlocker(m_chkShow2ToneMeasurements);
        m_chkLoopback->setChecked(m_pureSignal->loopback());
        m_chkShow2ToneMeasurements->setChecked(m_pureSignal->show2ToneMeasurements());
    } else {
        const QSignalBlocker loopbackBlocker(m_chkLoopback);
        const QSignalBlocker measurementsBlocker(m_chkShow2ToneMeasurements);
        m_chkLoopback->setChecked(AppSettings::instance()
            .value(QLatin1String(kLoopbackSettingsKey), false).toBool());
        m_chkShow2ToneMeasurements->setChecked(AppSettings::instance()
            .value(QLatin1String(kShowTwoToneSettingsKey), false).toBool());
    }
    refreshFacadeStatus();
}

void PsForm::syncAcceptedSettings()
{
    if (!m_settings) {
        return;
    }
    m_updatingFromModel = true;
    m_chkAutoCalEnabled->setChecked(m_settings->autoCalEnabled());
    m_chkRunCalibrationProcessing->setChecked(m_settings->runCalibrationProcessing());
    m_chkAutoAttenuate->setChecked(m_settings->autoAttenuate());
    m_chkQuickAttenuate->setChecked(m_settings->quickAttenuate());
    m_spinMoxDelay->setValue(m_settings->moxDelaySeconds());
    m_spinCalDelay->setValue(m_settings->loopDelaySeconds());
    m_spinAmpDelay->setValue(static_cast<int>(m_settings->requestedTxDelayNs()));
    m_chkHardwarePeakOverride->setChecked(m_settings->hardwarePeakOverrideEnabled());
    m_txtPSpeak->setText(QString::number(m_settings->hardwarePeakOverride(), 'f', 4));
    m_txtPSpeak->setEnabled(true);
    m_updatingFromModel = false;
}

quint32 PsForm::requestAction(Ps3Action action, const QVariantMap& arguments)
{
    if (!m_facade) {
        m_lblActionStatus->setText(tr("PureSignal is unavailable."));
        return 0;
    }
    const quint32 operationId = m_facade->requestAction(action, arguments);
    if (!operationId) {
        const QString reason = m_facade->lastActionError();
        m_lblActionStatus->setText(reason.isEmpty()
            ? tr("PureSignal refused the action.") : OperatorReasonText::forDisplay(reason));
        syncFromPureSignal();
        return 0;
    }
    m_pendingActions.insert(operationId);
    m_lblActionStatus->setText(tr("Action %1 accepted.").arg(operationId));
    return operationId;
}

void PsForm::onActionResult(quint32 operationId, Ps3ActionPhase phase,
                            const QString& reason, const QVariantMap& values)
{
    if (!m_pendingActions.contains(operationId)) {
        return;
    }
    QString phaseText;
    switch (phase) {
    case Ps3ActionPhase::Accepted: phaseText = tr("accepted"); break;
    case Ps3ActionPhase::Pending: phaseText = tr("pending"); break;
    case Ps3ActionPhase::Completed: phaseText = tr("completed"); break;
    case Ps3ActionPhase::Failed: phaseText = tr("failed"); break;
    }
    QString text = tr("Action %1 %2.").arg(operationId).arg(phaseText);
    if (!reason.isEmpty()) {
        // The Core's reason, in user words; the raw text is logged.
        text += QStringLiteral(" ") + OperatorReasonText::forDisplay(reason);
    }
    const QString assetId = values.value(QStringLiteral("assetId")).toString();
    if (!assetId.isEmpty()) {
        text += tr(" Saved as asset %1.").arg(assetId);
    }
    m_lblActionStatus->setText(text);
    if (phase == Ps3ActionPhase::Completed || phase == Ps3ActionPhase::Failed) {
        m_pendingActions.remove(operationId);
    }
    refreshFacadeStatus();
}

void PsForm::onSessionInvalidated()
{
    m_pendingActions.clear();
    setImdMeasurementFrame({});
    if (m_lblActionStatus) {
        m_lblActionStatus->setText(tr("The connection to the Core changed; actions still waiting were canceled."));
    }
    refreshFacadeStatus();
}

void PsForm::refreshFacadeStatus()
{
    const Ps3StatusSnapshot status = m_facade
        ? m_facade->statusSnapshot() : Ps3StatusSnapshot{};
    for (std::size_t i = 0; i < m_rawInfoLabels.size(); ++i) {
        if (m_rawInfoLabels[i]) {
            m_rawInfoLabels[i]->setText(QString::number(status.raw[i]));
        }
    }
    if (m_lblFb2) {
        m_lblFb2->setText(QString::number(status.feedbackLevel));
    }
    if (m_lblGetPSpeak) {
        m_lblGetPSpeak->setText(QString::number(status.hardwarePeak, 'f', 4));
    }
    if (m_lblNativeStatus) {
        m_lblNativeStatus->setText(tr(
            "PS %1, MOX %2; RunCal %3; IQ correction run/busy %4/%5; "
            "delay requested/applied %6/%7 ns; peak/maxTX %8/%9; feedback %10 Hz; "
            "save gen %11 pending %12 result %13; restore gen %14 pending %15 result %16")
            .arg(status.psEnabled ? tr("on") : tr("off"))
            .arg(status.mox ? tr("on") : tr("off"))
            .arg(status.runCalibrationProcessing ? tr("on") : tr("off"))
            .arg(status.correctionRun).arg(status.correctionBusy)
            .arg(status.requestedTxDelayNs, 0, 'f', 1)
            .arg(status.appliedTxDelayNs, 0, 'f', 1)
            .arg(status.hardwarePeak, 0, 'f', 4)
            .arg(status.maxTx, 0, 'f', 4)
            .arg(status.feedbackRateHz)
            .arg(static_cast<qulonglong>(status.saveGeneration))
            .arg(status.savePending).arg(status.saveResult)
            .arg(static_cast<qulonglong>(status.restoreGeneration))
            .arg(status.restorePending).arg(status.restoreResult));
    }
    if (m_lblRoutingStatus) {
        m_lblRoutingStatus->setText(tr(
            "PSCC route TX monitor DDC %1, feedback DDC %2, feedback channel %3; "
            "pump %4; paired blocks %5")
            .arg(status.txMonitorDdc)
            .arg(status.feedbackDdc)
            .arg(status.feedbackChannelId)
            .arg(status.pumpActive ? tr("active") : tr("inactive"))
            .arg(static_cast<qulonglong>(status.pairedBlocks)));
    }

    const bool available = m_facade && m_facade->available();
    m_instruments->setStatus(status, available);
    const bool canActuate = m_facade && m_facade->canActuate();
    // R-R3-49 (parity Task 7): arming keys nothing, so it follows canArm;
    // the two-tone test below keys the radio and stays on canActuate.
    const bool canArm = m_facade && m_facade->canArm();
    const QString armingRefusal = m_facade ? m_facade->armingRefusal() : QString();
    gateWithReason(m_btnSingleCal, canArm, armingRefusal);
    gateWithReason(m_btnAutomatic, canArm, armingRefusal);
    gateWithReason(m_btnApplyCurrent, canArm, armingRefusal);
    // The settings a Core offering arming takes only off the air.
    const QString settingsRefusal = m_facade ? m_facade->settingsRefusal() : QString();
    for (QWidget* control : {static_cast<QWidget*>(m_chkAutoAttenuate),
                             static_cast<QWidget*>(m_chkQuickAttenuate),
                             static_cast<QWidget*>(m_chkAutoCalEnabled),
                             static_cast<QWidget*>(m_chkRunCalibrationProcessing),
                             static_cast<QWidget*>(m_chkHardwarePeakOverride),
                             static_cast<QWidget*>(m_txtPSpeak),
                             static_cast<QWidget*>(m_btnDefaultPeaks),
                             static_cast<QWidget*>(m_spinMoxDelay),
                             static_cast<QWidget*>(m_spinCalDelay),
                             static_cast<QWidget*>(m_spinAmpDelay)}) {
        if (control && (!settingsRefusal.isEmpty()
                        || control->property("PsFormSavedTooltip").isValid())) {
            gateWithReason(control, true, settingsRefusal);
        }
    }
    if (m_btnTwoTone) {
        const QSignalBlocker blocker(m_btnTwoTone);
        m_btnTwoTone->setChecked(m_facade && m_facade->twoToneOn());
        m_btnTwoTone->setEnabled(canActuate || m_btnTwoTone->isChecked());
    }
    if (m_btnSave) {
        m_btnSave->setEnabled(available && status.correctionsApplied);
    }
    // The manager remains useful on a remote non-TX station for listing,
    // import, and export. Restore selection itself crosses the facade and is
    // rejected when the station cannot actuate.
    if (m_btnRestore) {
        m_btnRestore->setEnabled(available && m_radioModel);
    }
    if (m_btnAmpView) {
        m_btnAmpView->setEnabled(available);
    }
    if (m_btnReset) {
        m_btnReset->setEnabled(true);
    }

    const bool correcting = status.feedbackLevel > 90;
    if (m_lblCo) {
        const QString colour = status.correctionsApplied
            ? correcting ? psFeedbackColour(150).name() : psFeedbackColour(100).name()
            : QStringLiteral("black");
        const QString style = QStringLiteral(
            "QLabel { background-color: %1; border: 1px inset; "
            "min-width: 12px; min-height: 12px; }").arg(colour);
        // Qt polishes and repaints a widget on every setStyleSheet(), the
        // same text or not, and this runs on every status update.
        if (m_lblCo->styleSheet() != style) {
            m_lblCo->setStyleSheet(style);
        }
    }
    refreshSaveRestoreButtons();
}

// ─────────────────────────────────────────────────────────────────────────
// Action slots — each forwards to PureSignal coordinator.  When
// m_pureSignal is null (test seam, dialog opened before connect), the
// slot is a no-op so the test can still verify control wiring without
// needing a full RadioModel stand-up.
// ─────────────────────────────────────────────────────────────────────────

void PsForm::onSingleCalibrate()
{
    // From Thetis PSForm.cs:466-478 btnPSCalibrate_Click [v2.10.3.13].
    // The PureSignal coordinator's singleCalibrate() also covers
    // PSForm.cs:481-484 SingleCalrun (//-W2PA Adds capability for CAT
    // control via console) — both routes share the same handler body.
    requestAction(Ps3Action::Single);
}

void PsForm::onAutomaticCalibrate()
{
    requestAction(Ps3Action::StartAutomatic);
}

void PsForm::onApplyCurrentCorrection()
{
    requestAction(Ps3Action::ApplyCurrentCorrection);
}

void PsForm::onAdvancedClicked()
{
    // From Thetis PSForm.cs:888-902 btnPSAdvanced_Click + setAdvancedView
    // [v2.10.3.13]:
    //   private bool _advancedON = false; //MW0LGE_[2.9.0.7]
    //   _advancedON = !_advancedON;
    //   if (_advancedON) ClientSize = (560, 60);
    //   else             ClientSize = (560, 300);
    // (//MW0LGE attribution preserved — author tag from upstream comment.)
    setAdvancedMode(!m_advancedCollapsed);
    persistAdvancedMode();
}

void PsForm::onSavePressed()
{
    bool accepted = false;
    const QString label = QInputDialog::getText(
        this, tr("Save PureSignal correction"), tr("Correction label:"),
        QLineEdit::Normal, {}, &accepted).trimmed();
    if (!accepted || label.isEmpty()) {
        return;
    }
    requestAction(Ps3Action::SaveCorrection, {{QStringLiteral("label"), label}});
}

void PsForm::onRestorePressed()
{
    if (!m_radioModel || !m_facade) {
        if (m_lblActionStatus) {
            m_lblActionStatus->setText(tr("The correction manager is unavailable."));
        }
        return;
    }
    if (!m_assetDialog) {
        m_assetDialog = new DspAssetDialog(
            m_radioModel, DspAssetKind::Ps3Correction, this);
        connect(m_assetDialog, &DspAssetDialog::restoreCorrectionRequested,
                this, [this](const QString& assetId) {
            requestAction(Ps3Action::RestoreCorrection,
                          {{QStringLiteral("assetId"), assetId}});
        });
    }
    m_assetDialog->show();
    m_assetDialog->raise();
    m_assetDialog->activateWindow();
}

void PsForm::onTwoToneToggled(bool checked)
{
    // From Thetis PSForm.cs:508-522 btnPSTwoToneGen_Click [v2.10.3.13].
    requestAction(Ps3Action::SetTwoTone,
                  {{QStringLiteral("enabled"), checked}});
}

void PsForm::onResetClicked()
{
    // From Thetis PSForm.cs:486-491 btnPSReset_Click [v2.10.3.13]:
    //   console.ForcePureSignalAutoCalDisable();
    //   if (!_OFF) _OFF = true;
    //   console.PSState = false;
    requestAction(Ps3Action::OffReset);
    // Also force the [Two-tone] toggle off — Thetis PSForm.cs:519-522
    // [v2.10.3.13] sets SetupForm.TTgenrun = false; mirroring that
    // toggle keeps the UI consistent with the engine state.
    if (m_btnTwoTone->isChecked()) {
        m_btnTwoTone->setChecked(false);
    }
}

void PsForm::onAmpViewClicked()
{
    // From Thetis PSForm.cs:454-464 btnPSAmpView_Click [v2.10.3.13]:
    //   if (ampv == null) { ampv = new AmpView(this); ampv.Show(); ... }
    //   else { ampv.WindowState = FormWindowState.Normal; ampv.Show(); }
    //   FixAmpViewOnTop();
    //
    // Lazy-construct on first click; reuse the singleton on subsequent
    // clicks.  The dialog is parented to PsForm so it survives PsForm
    // close/reopen.  AmpView.cs FormClosed [v2.10.3.13] sets PSForm.ampv
    // to null so a new instance is created next time; NereusSDR keeps
    // the dialog alive (hide-on-close) — matches the TxEqDialog +
    // PsForm singleton pattern.
    if (!m_ampView) {
        m_ampView = new AmpViewWindow(m_radioModel, m_pureSignal, this);
    }
    m_ampView->show();
    m_ampView->raise();
    m_ampView->activateWindow();

    // Mirror the parent's Always-On-Top state (PsForm FixAmpViewOnTop
    // equivalent).  Always pass the current chkPSOnTop state so the
    // child tracks any toggle that happened while AmpView was hidden.
    if (m_chkOnTop) {
        m_ampView->setStayOnTopFromParent(m_chkOnTop->isChecked());
    }
}

void PsForm::restoreAmpView()
{
    if (!m_ampView) {
        m_ampView = new AmpViewWindow(m_radioModel, m_pureSignal, this);
    }
    m_ampView->show();
    m_ampView->raise();
    m_ampView->activateWindow();
    if (m_chkOnTop) {
        m_ampView->setStayOnTopFromParent(m_chkOnTop->isChecked());
    }
}

void PsForm::onDefaultPeaksClicked()
{
    // From Thetis PSForm.cs:547-550 SetDefaultPeaks [v2.10.3.13]:
    //   psdefpeak(HardwareSpecific.PSDefaultPeak);
    if (m_settings) {
        m_settings->setHardwarePeakOverrideEnabled(false);
        syncAcceptedSettings();
    }
}

// PR #212 follow-up bench fix (J.J. KG4VCF, 2026-05-07).
// From Thetis PSForm.cs:787-794 PSpeak_TextChanged [v2.10.3.13]:
//   private void PSpeak_TextChanged(object sender, EventArgs e) {
//       bool bOk = double.TryParse(txtPSpeak.Text, out double tmp);
//       if (bOk) {
//           _PShwpeak = tmp;
//           puresignal.SetPSHWPeak(_txachannel, _PShwpeak);
//           UpdateWarningSetPk();
//       }
//   }
// Qt translation: editingFinished fires on Enter or focus loss (vs Thetis
// TextChanged which fires per keystroke).  Cleaner UX — user types the
// full number then commits, vs racing the parser per keystroke.  Invalid
// input is silently ignored; the displayed text snaps back to the
// model's hwPeak on the next updateFromModel() pass.  No persistence
// here — PureSignal::setHwPeak is non-persisted runtime state, matching
// Thetis (txtPSpeak's value is restored from `_PShwpeak` field which is
// itself rebuilt from per-board defaults on connect).
void PsForm::onPSpeakEditingFinished()
{
    if (m_updatingFromModel || !m_settings || !m_txtPSpeak) {
        return;
    }
    bool ok = false;
    const double value = m_txtPSpeak->text().toDouble(&ok);
    if (!ok) {
        // Reject invalid input; snap back to current model value.
        syncAcceptedSettings();
        return;
    }
    // Sanity clamp: psHWPeak must be positive.  Thetis allows any double
    // (including 0 or negative) but those crash calcc with
    // hw_scale = 1/peak.  We clamp to (0.001, 2.0] which covers all
    // realistic HL2/ANAN/Saturn values (HL2 default 0.233, Saturn default
    // 0.6121, ANAN-G2 0.4072 — all in this range).
    const double clamped = std::clamp(value, 0.001, 2.0);
    m_settings->setHardwarePeakOverride(clamped);
    syncAcceptedSettings();
}

void PsForm::onAlwaysOnTopToggled(bool on)
{
    // From Thetis PSForm.cs:903-908 chkPSOnTop_CheckedChanged [v2.10.3.13]:
    //   _topmost = chkPSOnTop.Checked;
    //   this.TopMost = _topmost; //MW0LGE
    Qt::WindowFlags flags = windowFlags();
    if (on) {
        flags |= Qt::WindowStaysOnTopHint;
    } else {
        flags &= ~Qt::WindowStaysOnTopHint;
    }
    const bool wasVisible = isVisible();
    setWindowFlags(flags);
    if (wasVisible) {
        show();
    }
    AppSettings::instance().setValue(
        QLatin1String(kAlwaysOnTopSettingsKey), on);

    // Propagate to AmpView (FixAmpViewOnTop equivalent — Thetis PSForm
    // helper that keeps the AmpView dialog tracking the parent's
    // Always-On-Top state).  Only fires if the AmpView dialog has been
    // lazily constructed already.
    if (m_ampView) {
        m_ampView->setStayOnTopFromParent(on);
    }
}

void PsForm::onAutoAttenuateToggled(bool on)
{
    if (m_updatingFromModel || !m_settings) { return; }
    m_settings->setAutoAttenuate(on);
    syncAcceptedSettings();
}

void PsForm::onQuickAttenuateToggled(bool on)
{
    if (m_updatingFromModel || !m_settings) { return; }
    m_settings->setQuickAttenuate(on);
    syncAcceptedSettings();
}

void PsForm::onAutoCalEnabledToggled(bool on)
{
    if (m_updatingFromModel || !m_settings) { return; }
    m_settings->setAutoCalEnabled(on);
    syncAcceptedSettings();
}

void PsForm::onRunCalibrationProcessingToggled(bool on)
{
    if (m_updatingFromModel || !m_settings) { return; }
    m_settings->setRunCalibrationProcessing(on);
    syncAcceptedSettings();
}

void PsForm::onHardwarePeakOverrideToggled(bool on)
{
    if (m_updatingFromModel || !m_settings) { return; }
    m_settings->setHardwarePeakOverrideEnabled(on);
    syncAcceptedSettings();
}

void PsForm::onLoopbackToggled(bool on)
{
    if (m_updatingFromModel) { return; }
    AppSettings::instance().setValue(QLatin1String(kLoopbackSettingsKey), on);
    if (m_pureSignal && !m_radioModel) {
        m_pureSignal->setLoopback(on);
    }
}

void PsForm::onShow2ToneMeasurementsToggled(bool on)
{
    if (m_updatingFromModel) { return; }
    AppSettings::instance().setValue(QLatin1String(kShowTwoToneSettingsKey),
                                     on ? QStringLiteral("True") : QStringLiteral("False"));
    emit showTwoToneMeasurementsChanged(on);
    if (m_pureSignal && !m_radioModel) {
        m_pureSignal->setShow2ToneMeasurements(on);
    }
}

void PsForm::onMoxDelayChanged(double v)
{
    if (m_updatingFromModel || !m_settings) { return; }
    m_settings->setMoxDelaySeconds(v);
    syncAcceptedSettings();
}

void PsForm::onCalDelayChanged(double v)
{
    if (m_updatingFromModel || !m_settings) { return; }
    m_settings->setLoopDelaySeconds(v);
    syncAcceptedSettings();
}

void PsForm::onAmpDelayChanged(int v)
{
    if (m_updatingFromModel || !m_settings) { return; }
    m_settings->setRequestedTxDelayNs(v);
    syncAcceptedSettings();
}

// ─────────────────────────────────────────────────────────────────────────
// PureSignal -> UI sync slots
// ─────────────────────────────────────────────────────────────────────────

void PsForm::onFeedbackLevelChanged(int level)
{
    // From Thetis PSForm.cs:567 timer1code [v2.10.3.13]:
    //   lblPSfb2.Text = puresignal.FeedbackLevel.ToString();
    if (m_lblFb2) {
        m_lblFb2->setText(QString::number(level));
    }
}

void PsForm::refreshCoBadge()
{
    // From Thetis PSForm.cs:574-593 [v2.10.3.13]:
    //   if (CorrectionsBeingApplied) {
    //       if (Correcting) lblPSInfoCO.BackColor = Color.Lime;
    //       else            lblPSInfoCO.BackColor = Color.Yellow;
    //   } else {            lblPSInfoCO.BackColor = Color.Black; }
    //
    // Codex Fix D: full Thetis 3-state logic.  Pre-fix this slot took a
    // single bool that conflated CorrectionsBeingApplied (info[14]==1) with
    // Correcting (FeedbackLevel > 90), and the Yellow case was unreachable.
    // Now driven by both correctionsBeingAppliedChanged AND correctingChanged
    // signals; reads both predicates from the coordinator's atomic getters.
    refreshFacadeStatus();
}

void PsForm::refreshSaveRestoreButtons()
{
    const Ps3StatusSnapshot status = m_facade
        ? m_facade->statusSnapshot() : Ps3StatusSnapshot{};
    if (m_btnSave) {
        m_btnSave->setEnabled(m_facade && m_facade->available()
                              && status.correctionsApplied);
    }
    if (m_btnRestore) {
        m_btnRestore->setEnabled(m_facade && m_facade->available()
                                 && m_radioModel);
    }
}

void PsForm::onCalibrationCountChanged(int count)
{
    // From Thetis PSForm.cs:561-571 timer1code [v2.10.3.13]:
    //   lblPSInfo0.Text  = puresignal.Info[0].ToString();   // bldr.rx
    //   lblPSInfo1.Text  = puresignal.Info[1].ToString();   // bldr.cm
    //   lblPSInfo2.Text  = puresignal.Info[2].ToString();   // bldr.cc
    //   lblPSInfo3.Text  = puresignal.Info[3].ToString();   // bldr.cs
    //   lblPSfb2.Text    = puresignal.FeedbackLevel.ToString();    // info[4]
    //   lblPSInfo5.Text  = puresignal.CalibrationCount.ToString(); // info[5]
    //   lblPSInfo6.Text  = puresignal.Info[6].ToString();   // sln.chk
    //   lblPSInfo13.Text = puresignal.Info[13].ToString();  // dg.cnt
    //   lblPSInfo15.Text = puresignal.Info[15].ToString();  // state
    //
    // ANAN-G2E bench-fix 2026-05-23 (JJ Boyd): pre-fix only lblPSInfo5
    // was updated here.  The other 8 labels were created in
    // buildCalibrationInfoGroup() but never bound to any data source —
    // they sat at their initial "0" placeholder text even while calcc
    // was actively cycling.  JJ noticed during a side-by-side Thetis vs
    // NereusSDR PS bench run that Thetis's labels updated while ours
    // were stuck.  Reading PureSignal::infoAt(i) here is safe because
    // calibrationCountChanged fires AFTER pollTimerTick's m_info update
    // (see PureSignal.cpp:1033 + 1287).
    if (m_lblInfo5) {
        m_lblInfo5->setText(QString::number(count));
    }
    if (!m_pureSignal) {
        return;
    }
    if (m_lblInfo0)  { m_lblInfo0->setText(QString::number(m_pureSignal->infoAt(0)));  }
    if (m_lblInfo1)  { m_lblInfo1->setText(QString::number(m_pureSignal->infoAt(1)));  }
    if (m_lblInfo2)  { m_lblInfo2->setText(QString::number(m_pureSignal->infoAt(2)));  }
    if (m_lblInfo3)  { m_lblInfo3->setText(QString::number(m_pureSignal->infoAt(3)));  }
    if (m_lblInfo6)  { m_lblInfo6->setText(QString::number(m_pureSignal->infoAt(6)));  }
    if (m_lblInfo13) { m_lblInfo13->setText(QString::number(m_pureSignal->infoAt(13))); }
    if (m_lblInfo15) { m_lblInfo15->setText(QString::number(m_pureSignal->infoAt(15))); }
    if (m_lblGetPSpeak) {
        // From Thetis PSForm.cs:614 [v2.10.3.13]:
        //   lblPSHWPeak.Text = puresignal.HWPeak.ToString("F4");
        const double peak = m_pureSignal->getHwPeak();
        m_lblGetPSpeak->setText(QString::number(peak, 'f', 4));
    }
}

void PsForm::onFeedbackColourChanged(const QColor& colour)
{
    // From Thetis PSForm.cs:597-611 [v2.10.3.13]:
    //   if (CalibrationAttemptsChanged)
    //       lblPSInfoFB.BackColor = puresignal.FeedbackColourLevel;
    if (m_lblFb) {
        const QString css = QStringLiteral(
            "QLabel { background-color: %1; border: 1px inset; "
            "min-width: 12px; min-height: 12px; }").arg(colour.name());
        m_lblFb->setStyleSheet(css);
    }
}

// ─────────────────────────────────────────────────────────────────────────
// Advanced collapse mode
// ─────────────────────────────────────────────────────────────────────────

void PsForm::setAdvancedMode(bool collapsed)
{
    m_advancedCollapsed = collapsed;
    for (QWidget* w : m_advancedSectionWidgets) {
        if (w) {
            w->setVisible(!collapsed);
        }
    }
    // Adjust the dialog size so the layout reflows.  Thetis hardcodes
    // ClientSize 560x60 (collapsed) / 560x300 (expanded); we let Qt
    // recompute from the visible widgets via adjustSize() so DPI scaling
    // works out of the box.
    adjustSize();
}

void PsForm::persistAdvancedMode() const
{
    AppSettings::instance().setValue(
        QLatin1String(kAdvancedCollapsedSettingsKey),
        m_advancedCollapsed ? QStringLiteral("True") : QStringLiteral("False"));
}

void PsForm::restoreAdvancedMode()
{
    const QString persisted =
        AppSettings::instance().value(
            QLatin1String(kAdvancedCollapsedSettingsKey),
            QStringLiteral("False")).toString();
    setAdvancedMode(persisted == QStringLiteral("True"));
}

// ─────────────────────────────────────────────────────────────────────────
// Close handling
// ─────────────────────────────────────────────────────────────────────────

void PsForm::setImdMeasurementFrame(const ImdMeasurementFrame& frame)
{
    m_imdMeasurementFrame = frame;
    if (m_imdPanel) { m_imdPanel->setFrame(frame); }
}

void PsForm::setShowTwoToneMeasurements(bool on)
{
    if (!m_chkShow2ToneMeasurements) { return; }
    const QSignalBlocker blocker(m_chkShow2ToneMeasurements);
    m_chkShow2ToneMeasurements->setChecked(on);
}

void PsForm::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    if (!m_benchLayout || !m_instruments) { return; }
    const bool narrow = width() < 760;
    QWidget* tabs = findChild<QWidget*>(QStringLiteral("PureSignalBenchTabs"));
    m_benchLayout->addWidget(tabs, narrow ? 1 : 0, narrow ? 0 : 1);
    m_benchLayout->setColumnStretch(0, narrow ? 1 : 0);
    m_benchLayout->setColumnStretch(1, narrow ? 0 : 1);
    // Reflow the exposed raw words independently from the control workarea.
    auto* grid = qobject_cast<QGridLayout*>(m_grpCalInfo->layout());
    const int pairs = narrow ? 2 : 4;
    QList<QWidget*> words;
    while (grid->count() > 0) {
        std::unique_ptr<QLayoutItem> item(grid->takeAt(0));
        if (item->widget()) { words.append(item->widget()); }
    }
    for (int i = 0; i < words.size(); ++i) {
        grid->addWidget(words[i], (i / 2) / pairs, ((i / 2) % pairs) * 2 + i % 2);
    }
}

void PsForm::closeEvent(QCloseEvent* event)
{
    // From Thetis PSForm.cs:418-422 PSForm_Closing [v2.10.3.13]:
    //   e.Cancel = true;
    //   Common.SaveForm(this, "PureSignal");
    // NereusSDR mirrors via hide() so the singleton survives across opens.
    AppSettings::instance().setValue(
        QLatin1String(kGeometrySettingsKey),
        QString::fromLatin1(saveGeometry().toBase64()));
    event->ignore();
    hide();
}

} // namespace NereusSDR
