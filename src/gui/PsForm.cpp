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
//   2026-05-06 — Phase 3M-4 Task 8: created by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude
//                 Code.
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

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

#include "AmpViewWindow.h"
#include "core/AppSettings.h"
#include "core/PureSignal.h"

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
    //   this.Text = "PureSignal 2.0";
    setWindowTitle(QStringLiteral("PureSignal 2.0"));
    setObjectName(QStringLiteral("PsForm"));
    // Modeless — don't block other interaction.
    setModal(false);
    // Singleton lifecycle (matches TxEqDialog) — survive close/hide cycles.
    setAttribute(Qt::WA_DeleteOnClose, false);

    buildUi();
    wireToPureSignal();
    syncFromPureSignal();
    restoreAdvancedMode();
}

PsForm::~PsForm() = default;

// ─────────────────────────────────────────────────────────────────────────
// UI construction
// ─────────────────────────────────────────────────────────────────────────

void PsForm::buildUi()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(6);

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
    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(6);

    m_btnTwoTone = new QPushButton(tr("Two-tone"), this);
    m_btnTwoTone->setObjectName(QStringLiteral("btnPSTwoToneGen"));
    m_btnTwoTone->setCheckable(true);
    m_btnTwoTone->setToolTip(tr("Generate and TX a Two Tone Signal"));
    topRow->addWidget(m_btnTwoTone);

    m_btnSingleCal = new QPushButton(tr("Single Cal"), this);
    m_btnSingleCal->setObjectName(QStringLiteral("btnPSCalibrate"));
    m_btnSingleCal->setToolTip(
        tr("Perform a singal calibration. This will happen up to 5 times in a row"));
    topRow->addWidget(m_btnSingleCal);

    m_btnAmpView = new QPushButton(tr("AmpView"), this);
    m_btnAmpView->setObjectName(QStringLiteral("btnPSAmpView"));
    topRow->addWidget(m_btnAmpView);

    m_btnAdvanced = new QPushButton(tr("Advanced"), this);
    m_btnAdvanced->setObjectName(QStringLiteral("btnPSAdvanced"));
    topRow->addWidget(m_btnAdvanced);

    m_btnSave = new QPushButton(tr("Save"), this);
    m_btnSave->setObjectName(QStringLiteral("btnPSSave"));
    // From Thetis PSForm.cs:574-590 [v2.10.3.13] — Save is gated on
    // CorrectionsBeingApplied; starts disabled until corrections land.
    m_btnSave->setEnabled(false);
    topRow->addWidget(m_btnSave);

    m_btnRestore = new QPushButton(tr("Restore"), this);
    m_btnRestore->setObjectName(QStringLiteral("btnPSRestore"));
    topRow->addWidget(m_btnRestore);

    m_btnReset = new QPushButton(tr("OFF"), this);
    m_btnReset->setObjectName(QStringLiteral("btnPSReset"));
    topRow->addWidget(m_btnReset);

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
        "If green, a correction solution is in place and PureSignal is correcting"));
    statusRow->addWidget(m_lblCo);

    auto* lblCoText = new QLabel(tr("Correcting"), this);
    lblCoText->setObjectName(QStringLiteral("labelTS9"));
    lblCoText->setToolTip(tr(
        "If green, a correction solution is in place and PureSignal is correcting"));
    statusRow->addWidget(lblCoText);

    statusRow->addStretch();

    // From PSForm.designer.cs:835-844 pbWarningSetPk [v2.10.3.13] — hidden
    // by default, shown when current SetPk drifts from psDefaultPeak.
    // NereusSDR uses a QLabel with a "!" glyph stand-in until artwork
    // import lands.
    m_lblWarningSetPk = new QLabel(QStringLiteral("⚠"), this);
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
    statusRow->addWidget(m_chkShow2ToneMeasurements);

    outer->addLayout(statusRow);

    // ── Body: 3-column grid ───────────────────────────────────────────────
    //
    // Column 0: timing labels       (MOX Wait / CAL Wait / AMP Delay)
    // Column 1: timing spinboxes
    // Column 2: cal options column 1 (Auto-Attenuate / Relax Tol / Quick)
    // Column 3: cal options column 2 (PIN / MAP / STBL / TINT)
    auto* body = new QGridLayout();
    body->setContentsMargins(0, 0, 0, 0);
    body->setHorizontalSpacing(8);
    body->setVerticalSpacing(4);

    // Row 0: MOX Wait + Auto-Attenuate + PIN
    auto* lblMox = new QLabel(tr("MOX Wait (sec)"), this);
    lblMox->setObjectName(QStringLiteral("labelTS4"));
    body->addWidget(lblMox, 0, 0);

    m_spinMoxDelay = new QDoubleSpinBox(this);
    m_spinMoxDelay->setObjectName(QStringLiteral("udPSMoxDelay"));
    // From PSForm.designer.cs:347-372 [v2.10.3.13]:
    //   DecimalPlaces=1, Increment=0.1, Minimum=0.1, Maximum=10.0,
    //   Value=2.0.  (Decimal arrays {1,0,0,65536} = 1 / 10^1 = 0.1.)
    m_spinMoxDelay->setDecimals(1);
    m_spinMoxDelay->setSingleStep(0.1);
    m_spinMoxDelay->setMinimum(0.1);
    m_spinMoxDelay->setMaximum(10.0);
    m_spinMoxDelay->setValue(2.0);
    m_spinMoxDelay->setToolTip(
        tr("Settling time between assertion of MOX and collection of feedback"));
    body->addWidget(m_spinMoxDelay, 0, 1);

    m_chkAutoAttenuate = new QCheckBox(tr("Auto-Attenuate"), this);
    m_chkAutoAttenuate->setObjectName(QStringLiteral("chkPSAutoAttenuate"));
    m_chkAutoAttenuate->setChecked(true);  // Designer default Checked
    m_chkAutoAttenuate->setToolTip(
        tr("Automatically adjust attenuator for optimum feedback level. (Recommended)"));
    body->addWidget(m_chkAutoAttenuate, 0, 2);

    m_chkPin = new QCheckBox(tr("PIN"), this);
    m_chkPin->setObjectName(QStringLiteral("chkPSPin"));
    m_chkPin->setChecked(true);  // Designer default Checked
    m_chkPin->setToolTip(tr(
        "Manually 'pin' the upper-end of the gain curve; compensates for "
        "overshoots, etc. (Recommended)"));
    body->addWidget(m_chkPin, 0, 3);

    // Row 1: CAL Wait + Relax Tolerance + MAP
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

    m_chkRelaxPtol = new QCheckBox(tr("Relax Tolerance"), this);
    m_chkRelaxPtol->setObjectName(QStringLiteral("chkPSRelaxPtol"));
    m_chkRelaxPtol->setToolTip(
        tr("Allow for more dynamic variation in feedback; e.g., for memory-effects"));
    body->addWidget(m_chkRelaxPtol, 1, 2);

    m_chkMap = new QCheckBox(tr("MAP"), this);
    m_chkMap->setObjectName(QStringLiteral("chkPSMap"));
    m_chkMap->setChecked(true);  // Designer default Checked
    m_chkMap->setToolTip(tr(
        "Optimally re-map the sample collection intervals based upon amplifier "
        "characteristic.  (Recommended)"));
    body->addWidget(m_chkMap, 1, 3);

    // Row 2: AMP Delay + Quick Attenuate Response + STBL
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
    body->addWidget(m_chkQuickAttenuate, 2, 2);

    m_chkStbl = new QCheckBox(tr("STBL"), this);
    m_chkStbl->setObjectName(QStringLiteral("chkPSStbl"));
    m_chkStbl->setToolTip(tr("Averages multiple collections of calibration samples."));
    body->addWidget(m_chkStbl, 2, 3);

    // Row 3: TINT (label + combo span the right two columns)
    m_lblTint = new QLabel(tr("TINT (dB)"), this);
    m_lblTint->setObjectName(QStringLiteral("lblPSTint"));
    body->addWidget(m_lblTint, 3, 2);

    m_comboTint = new QComboBox(this);
    m_comboTint->setObjectName(QStringLiteral("comboPSTint"));
    // From PSForm.designer.cs:164-173 [v2.10.3.13]:
    //   Items: "0.5", "1.1", "2.5"; Text="0.5"
    m_comboTint->addItem(QStringLiteral("0.5"));
    m_comboTint->addItem(QStringLiteral("1.1"));
    m_comboTint->addItem(QStringLiteral("2.5"));
    m_comboTint->setCurrentIndex(0);
    m_comboTint->setToolTip(tr("FOR EXPERIMENTATION. - LEAVE AT 0.5dB."));
    body->addWidget(m_comboTint, 3, 3);

    outer->addLayout(body);

    // ── Bottom row: Loopback (left) + Always On Top (right) ───────────────
    auto* bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 0, 0, 0);
    bottomRow->setSpacing(6);

    m_chkLoopback =
        new QCheckBox(tr("Display PS-RX and PS-TX spectra"), this);
    m_chkLoopback->setObjectName(QStringLiteral("checkLoopback"));
    m_chkLoopback->setToolTip(
        tr("Use top and bottom panadapters to display the two feedback streams."));
    bottomRow->addWidget(m_chkLoopback);

    bottomRow->addStretch();

    m_chkOnTop = new QCheckBox(tr("Always On Top"), this);
    m_chkOnTop->setObjectName(QStringLiteral("chkPSOnTop"));
    bottomRow->addWidget(m_chkOnTop);

    outer->addLayout(bottomRow);

    // ── Calibration Information group (advanced section) ──────────────────
    m_grpCalInfo = buildCalibrationInfoGroup(this);
    outer->addWidget(m_grpCalInfo);

    outer->addStretch();

    // ── Build advanced-section widget vector ──────────────────────────────
    //
    // Everything below the top action row hides when the Advanced toggle
    // collapses the form.  Mirrors PSForm.cs:894-902 [v2.10.3.13]:
    //   if (_advancedON) ClientSize = (560,60);  // top row only
    //   else             ClientSize = (560,300); // full form
    m_advancedSectionWidgets.append(m_lblFb);
    m_advancedSectionWidgets.append(lblFbText);
    m_advancedSectionWidgets.append(m_lblCo);
    m_advancedSectionWidgets.append(lblCoText);
    m_advancedSectionWidgets.append(m_lblWarningSetPk);
    m_advancedSectionWidgets.append(m_chkShow2ToneMeasurements);
    m_advancedSectionWidgets.append(lblMox);
    m_advancedSectionWidgets.append(m_spinMoxDelay);
    m_advancedSectionWidgets.append(m_chkAutoAttenuate);
    m_advancedSectionWidgets.append(m_chkPin);
    m_advancedSectionWidgets.append(lblCal);
    m_advancedSectionWidgets.append(m_spinCalDelay);
    m_advancedSectionWidgets.append(m_chkRelaxPtol);
    m_advancedSectionWidgets.append(m_chkMap);
    m_advancedSectionWidgets.append(lblAmp);
    m_advancedSectionWidgets.append(m_spinAmpDelay);
    m_advancedSectionWidgets.append(m_chkQuickAttenuate);
    m_advancedSectionWidgets.append(m_chkStbl);
    m_advancedSectionWidgets.append(m_lblTint);
    m_advancedSectionWidgets.append(m_comboTint);
    m_advancedSectionWidgets.append(m_chkLoopback);
    m_advancedSectionWidgets.append(m_chkOnTop);
    m_advancedSectionWidgets.append(m_grpCalInfo);

    // ── Wire button signals to slots ──────────────────────────────────────
    connect(m_btnSingleCal,    &QPushButton::clicked,
            this, &PsForm::onSingleCalibrate);
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
    connect(m_chkPin,                  &QCheckBox::toggled,
            this, &PsForm::onPinToggled);
    connect(m_chkMap,                  &QCheckBox::toggled,
            this, &PsForm::onMapToggled);
    connect(m_chkStbl,                 &QCheckBox::toggled,
            this, &PsForm::onStblToggled);
    connect(m_chkAutoAttenuate,        &QCheckBox::toggled,
            this, &PsForm::onAutoAttenuateToggled);
    connect(m_chkRelaxPtol,            &QCheckBox::toggled,
            this, &PsForm::onRelaxPtolToggled);
    connect(m_chkQuickAttenuate,       &QCheckBox::toggled,
            this, &PsForm::onQuickAttenuateToggled);
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
    connect(m_comboTint,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PsForm::onTintIndexChanged);
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
    auto* grp = new QGroupBox(tr("Calibration Information"), parent);
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
        "Indicator:  RF feedback level; drives red/yellow/green indicator."));
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

    auto* lblCorCnt = new QLabel(tr("cor.cnt"), grp);
    lblCorCnt->setToolTip(tr(
        "Indicator:  cumulative number of new correction solutions."));
    g->addWidget(lblCorCnt, 4, 2);
    m_lblInfo5 = makeIndicatorLabel(QStringLiteral("lblPSInfo5"));
    m_lblInfo5->setToolTip(lblCorCnt->toolTip());
    g->addWidget(m_lblInfo5, 4, 3);

    // Column 2: GetPk / SetPk labels + read-only mirror labels
    auto* lblGetPk = new QLabel(tr("GetPk"), grp);
    g->addWidget(lblGetPk, 1, 4);
    m_lblGetPSpeak = new QLabel(grp);
    m_lblGetPSpeak->setObjectName(QStringLiteral("GetPSpeak"));
    m_lblGetPSpeak->setStyleSheet(bisqueLabelStyle());
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
    m_txtPSpeak->setStyleSheet(bisqueLabelStyle());
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

    return grp;
}

// ─────────────────────────────────────────────────────────────────────────
// Wire to PureSignal coordinator
// ─────────────────────────────────────────────────────────────────────────

void PsForm::wireToPureSignal()
{
    if (!m_pureSignal) {
        return;
    }
    // Save / Restore button enabled state.  Two gates per Thetis:
    //   1. PSForm.cs:574-590 [v2.10.3.13] — Save tracks CorrectionsBeingApplied.
    //   2. PSForm.cs:865/871/877/883 [v2.10.3.13] — both Save+Restore gated by
    //      TINT combo index (only idx 0 / default keeps the file format
    //      compatible).
    // Codex Fix D: route from correctionsBeingAppliedChanged (info[14]==1
    // predicate), NOT correctingChanged (FeedbackLevel > 90 predicate).
    // Codex Fix F: combine both gates via refreshSaveRestoreButtons slot so
    // a flip in EITHER signal reruns the AND.
    connect(m_pureSignal, &PureSignal::correctionsBeingAppliedChanged,
            this, &PsForm::refreshSaveRestoreButtons);
    connect(m_pureSignal, &PureSignal::saveRestoreEnabledChanged,
            this, &PsForm::refreshSaveRestoreButtons);

    // PureSignal -> UI label updates
    connect(m_pureSignal, &PureSignal::feedbackLevelChanged,
            this, &PsForm::onFeedbackLevelChanged);
    // Codex Fix D: CO badge depends on BOTH predicates (Lime when both,
    // Yellow when applied-but-not-correcting, Black when neither).  Wire to
    // both signals so a flip in either retriggers the slot.
    connect(m_pureSignal, &PureSignal::correctingChanged,
            this, &PsForm::refreshCoBadge);
    connect(m_pureSignal, &PureSignal::correctionsBeingAppliedChanged,
            this, &PsForm::refreshCoBadge);
    connect(m_pureSignal, &PureSignal::calibrationCountChanged,
            this, &PsForm::onCalibrationCountChanged);
    connect(m_pureSignal, &PureSignal::feedbackColourChanged,
            this, &PsForm::onFeedbackColourChanged);
}

void PsForm::syncFromPureSignal()
{
    if (!m_pureSignal) {
        return;
    }
    m_updatingFromModel = true;

    m_chkPin->setChecked(m_pureSignal->pinMode());
    m_chkMap->setChecked(m_pureSignal->mapMode());
    m_chkStbl->setChecked(m_pureSignal->stabilize());
    m_chkAutoAttenuate->setChecked(m_pureSignal->autoAttenuate());
    m_chkRelaxPtol->setChecked(m_pureSignal->relaxTolerance());
    m_chkQuickAttenuate->setChecked(m_pureSignal->quickAttenuate());
    m_spinMoxDelay->setValue(m_pureSignal->moxDelay());
    m_spinCalDelay->setValue(m_pureSignal->calDelay());
    m_spinAmpDelay->setValue(m_pureSignal->ampDelay());
    m_chkLoopback->setChecked(m_pureSignal->loopback());
    m_chkShow2ToneMeasurements->setChecked(
        m_pureSignal->show2ToneMeasurements());

    // TINT combo: read directly from the tintIndex() accessor (Codex Fix F
    // post-fix API).  Mirrors PSForm.designer.cs:172 [v2.10.3.13]
    // initial Text="0.5" when tintIndex()==0.
    m_comboTint->setCurrentIndex(m_pureSignal->tintIndex());

    // SetPk readout (editable QLineEdit; user can override per-bench)
    m_txtPSpeak->setText(QString::number(m_pureSignal->hwPeak(), 'f', 4));

    // Save button initial gating: PSForm.cs:574-590 [v2.10.3.13] sets
    // btnPSSave.Enabled to CorrectionsBeingApplied, AND PSForm.cs:865/871/
    // 877 sets it false unconditionally for TINT indexes 1+2 (regardless
    // of corrections-applied state).  Mirror both gates here.
    m_btnSave->setEnabled(m_pureSignal->correctionsBeingApplied()
                          && m_pureSignal->saveRestoreEnabled());
    // Restore button gating: PSForm.cs:865/871/877/883 [v2.10.3.13] —
    // tracks the same Save/Restore enabled state.
    m_btnRestore->setEnabled(m_pureSignal->saveRestoreEnabled());

    m_updatingFromModel = false;
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
    if (m_pureSignal) {
        m_pureSignal->singleCalibrate();
    }
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
    // From Thetis PSForm.cs:524-532 btnPSSave_Click [v2.10.3.13]:
    //   System.IO.Directory.CreateDirectory(console.AppDataPath + "PureSignal\\");
    //   SaveFileDialog savefile1 = new SaveFileDialog();
    //   ...
    //   if (savefile1.ShowDialog() == DialogResult.OK)
    //       puresignal.PSSaveCorr(_txachannel, savefile1.FileName);
    const QString defaultDir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/PureSignal/");
    QDir().mkpath(defaultDir);

    const QString filename = QFileDialog::getSaveFileName(
        this,
        tr("Save PureSignal corrections"),
        defaultDir,
        tr("PureSignal corrections (*.psk *.dat);;All files (*)"));
    if (filename.isEmpty()) {
        return;
    }
    if (!m_pureSignal || !m_pureSignal->saveCorrections(filename)) {
        QMessageBox::warning(this, tr("Save failed"),
            tr("Could not save PureSignal corrections to %1").arg(filename));
    }
}

void PsForm::onRestorePressed()
{
    // From Thetis PSForm.cs:534-545 btnPSRestore_Click [v2.10.3.13]:
    //   OpenFileDialog openfile1 = new OpenFileDialog();
    //   ...
    //   if (openfile1.ShowDialog() == DialogResult.OK) {
    //       console.ForcePureSignalAutoCalDisable();
    //       _OFF = false;
    //       puresignal.PSRestoreCorr(_txachannel, openfile1.FileName);
    //       _restoreON = true;
    //   }
    const QString defaultDir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/PureSignal/");
    QDir().mkpath(defaultDir);

    const QString filename = QFileDialog::getOpenFileName(
        this,
        tr("Restore PureSignal corrections"),
        defaultDir,
        tr("PureSignal corrections (*.psk *.dat);;All files (*)"));
    if (filename.isEmpty()) {
        return;
    }
    if (!m_pureSignal || !m_pureSignal->restoreCorrections(filename)) {
        QMessageBox::warning(this, tr("Restore failed"),
            tr("Could not restore PureSignal corrections from %1").arg(filename));
    }
}

void PsForm::onTwoToneToggled(bool checked)
{
    // From Thetis PSForm.cs:508-522 btnPSTwoToneGen_Click [v2.10.3.13].
    if (m_pureSignal) {
        m_pureSignal->setTwoToneOn(checked);
    }
}

void PsForm::onResetClicked()
{
    // From Thetis PSForm.cs:486-491 btnPSReset_Click [v2.10.3.13]:
    //   console.ForcePureSignalAutoCalDisable();
    //   if (!_OFF) _OFF = true;
    //   console.PSState = false;
    if (m_pureSignal) {
        m_pureSignal->reset();
    }
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

void PsForm::onDefaultPeaksClicked()
{
    // From Thetis PSForm.cs:547-550 SetDefaultPeaks [v2.10.3.13]:
    //   psdefpeak(HardwareSpecific.PSDefaultPeak);
    if (m_pureSignal) {
        m_pureSignal->setDefaultPeaks();
        // Mirror the new SetPk back into the editable field.
        m_txtPSpeak->setText(QString::number(m_pureSignal->hwPeak(), 'f', 4));
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
    if (m_updatingFromModel || !m_pureSignal || !m_txtPSpeak) {
        return;
    }
    bool ok = false;
    const double value = m_txtPSpeak->text().toDouble(&ok);
    if (!ok) {
        // Reject invalid input; snap back to current model value.
        m_txtPSpeak->setText(
            QString::number(m_pureSignal->hwPeak(), 'f', 4));
        return;
    }
    // Sanity clamp: psHWPeak must be positive.  Thetis allows any double
    // (including 0 or negative) but those crash calcc with
    // hw_scale = 1/peak.  We clamp to (0.001, 2.0] which covers all
    // realistic HL2/ANAN/Saturn values (HL2 default 0.233, Saturn default
    // 0.6121, ANAN-G2 0.4072 — all in this range).
    const double clamped = std::clamp(value, 0.001, 2.0);
    m_pureSignal->setHwPeak(clamped);
    if (clamped != value) {
        // Show the clamped value back to the user.
        m_txtPSpeak->setText(QString::number(clamped, 'f', 4));
    }
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

    // Propagate to AmpView (FixAmpViewOnTop equivalent — Thetis PSForm
    // helper that keeps the AmpView dialog tracking the parent's
    // Always-On-Top state).  Only fires if the AmpView dialog has been
    // lazily constructed already.
    if (m_ampView) {
        m_ampView->setStayOnTopFromParent(on);
    }
}

void PsForm::onPinToggled(bool on)
{
    if (m_updatingFromModel || !m_pureSignal) { return; }
    m_pureSignal->setPinMode(on);
}

void PsForm::onMapToggled(bool on)
{
    if (m_updatingFromModel || !m_pureSignal) { return; }
    m_pureSignal->setMapMode(on);
}

void PsForm::onStblToggled(bool on)
{
    if (m_updatingFromModel || !m_pureSignal) { return; }
    m_pureSignal->setStabilize(on);
}

void PsForm::onAutoAttenuateToggled(bool on)
{
    if (m_updatingFromModel || !m_pureSignal) { return; }
    m_pureSignal->setAutoAttenuate(on);
}

void PsForm::onRelaxPtolToggled(bool on)
{
    if (m_updatingFromModel || !m_pureSignal) { return; }
    m_pureSignal->setRelaxTolerance(on);
}

void PsForm::onQuickAttenuateToggled(bool on)
{
    if (m_updatingFromModel || !m_pureSignal) { return; }
    m_pureSignal->setQuickAttenuate(on);
}

void PsForm::onLoopbackToggled(bool on)
{
    if (m_updatingFromModel || !m_pureSignal) { return; }
    m_pureSignal->setLoopback(on);
}

void PsForm::onShow2ToneMeasurementsToggled(bool on)
{
    if (m_updatingFromModel || !m_pureSignal) { return; }
    m_pureSignal->setShow2ToneMeasurements(on);
}

void PsForm::onMoxDelayChanged(double v)
{
    if (m_updatingFromModel || !m_pureSignal) { return; }
    m_pureSignal->setMoxDelay(v);
}

void PsForm::onCalDelayChanged(double v)
{
    if (m_updatingFromModel || !m_pureSignal) { return; }
    m_pureSignal->setCalDelay(v);
}

void PsForm::onAmpDelayChanged(int v)
{
    if (m_updatingFromModel || !m_pureSignal) { return; }
    m_pureSignal->setAmpDelay(v);
}

void PsForm::onTintIndexChanged(int idx)
{
    if (m_updatingFromModel || !m_pureSignal) { return; }
    // Codex Fix F: route through the new setTintIndex post-fix API which
    // pushes (ints, spi) to the calcc engine via TxChannel::setPSIntsAndSpi.
    // Mirrors the comboPSTint handler at PSForm.cs:857-885 [v2.10.3.13]
    // verbatim including the default-case fallback for out-of-range idx.
    m_pureSignal->setTintIndex(idx);
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
    if (!m_lblCo || !m_pureSignal) {
        return;
    }
    QString css;
    if (m_pureSignal->correctionsBeingApplied()) {
        if (m_pureSignal->isCorrecting()) {
            css = QStringLiteral(
                "QLabel { background-color: #00FF00; border: 1px inset; "
                "min-width: 12px; min-height: 12px; }");
        } else {
            // Yellow — corrections applied but feedback level not yet > 90.
            css = QStringLiteral(
                "QLabel { background-color: #FFFF00; border: 1px inset; "
                "min-width: 12px; min-height: 12px; }");
        }
    } else {
        css = blackBadgeStyle();
    }
    m_lblCo->setStyleSheet(css);
}

void PsForm::refreshSaveRestoreButtons()
{
    // Codex Fix F: combined gate for Save / Restore button enabled-state.
    //
    // Save button: PSForm.cs:574-590 [v2.10.3.13] sets btnPSSave.Enabled to
    // CorrectionsBeingApplied; PSForm.cs:865/871/877/883 [v2.10.3.13]
    // additionally forces it false when TINT index is 1 or 2 (saveRestoreEnabled
    // = false).  AND-combine both gates here.
    //
    // Restore button: PSForm.cs:865/871/877/883 [v2.10.3.13] gates it on
    // saveRestoreEnabled alone — Restore is always available so long as the
    // file format is compatible (idx 0 / default), regardless of whether
    // calibration is currently producing corrections.
    if (!m_pureSignal) {
        return;
    }
    if (m_btnSave) {
        m_btnSave->setEnabled(m_pureSignal->correctionsBeingApplied()
                              && m_pureSignal->saveRestoreEnabled());
    }
    if (m_btnRestore) {
        m_btnRestore->setEnabled(m_pureSignal->saveRestoreEnabled());
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
