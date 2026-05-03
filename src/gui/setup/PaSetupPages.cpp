// =================================================================
// src/gui/setup/PaSetupPages.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// Setup IA reshape Phase 2 — top-level "PA" category placeholder pages.
// See PaSetupPages.h for category-level scope notes and the Thetis-source
// citations for each child page.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-02 — Original implementation.  PA top-level category for
//                 Setup IA reshape Phase 2.  AI-assisted transformation
//                 via Anthropic Claude Code.
//   2026-05-02 — Phase 4: live PA Values page wired to RadioStatus +
//                 RadioConnection signals (FWD calibrated / REV / SWR /
//                 PA current / PA temperature / supply volts / FWD-REV
//                 ADC raw / ADC overload). Skipped for MVP: per-stage
//                 RF voltage readouts (need public scaleFwdPowerWatts
//                 utility) and the Reset peak/min button.
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-05-03 — Phase 5 Agent 5A of #167: PaWattMeterPage gains the
//                 missing Thetis controls — chkPAValues "Show PA Values
//                 page" checkbox (persists visibility hint to
//                 AppSettings under "display/showPaValuesPage") and
//                 btnResetPAValues "Reset PA Values" button (emits
//                 resetPaValuesRequested for PaValuesPage / Phase 5B
//                 peak-min reset).  AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-05-03 — Phase 5 Agent 5B of issue #167 PA calibration safety
//                 hotfix.  PaValuesPage closes the panelPAValues 4-label
//                 gap (Raw FWD power, Drive, FWD voltage, REV voltage)
//                 via PaTelemetryScaling (Phase 1B), adds running
//                 peak/min tracking on six telemetry values, and
//                 surfaces a Reset button + resetPaValues() public slot
//                 for cross-page wiring to Phase 5A's PaWattMeterPage::
//                 resetPaValuesRequested signal.
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-05-03 — Phase 6 of issue #167 PA-cal hotfix: PaGainByBandPage
//                 promoted from Phase 2 placeholder to full live editor.
//                 Surface mirrors Thetis tpGainByBand
//                 (setup.designer.cs:47386-47525 [v2.10.3.13]) plus the
//                 chkAutoPACalibrate placeholder for Phase 7's sweep
//                 state machine. All edit flows route through
//                 PaProfileManager (Phase 2B) — auto-persist on every
//                 spinbox edit; profile lifecycle (New / Copy / Delete /
//                 Reset Defaults) driven by the manager. Test seams
//                 exposed via NEREUS_BUILD_TESTS so the dialog flows
//                 (QInputDialog / QMessageBox) can be bypassed in
//                 ctest. Authored by J.J. Boyd (KG4VCF) with
//                 AI-assisted implementation via Anthropic Claude Code.
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

#include "PaSetupPages.h"

#include "core/AppSettings.h"
#include "core/CalibrationController.h"
#include "core/HardwareProfile.h"
#include "core/HpsdrModel.h"
#include "core/PaCalProfile.h"
#include "core/PaGainProfile.h"
#include "core/PaProfile.h"
#include "core/PaProfileManager.h"
#include "core/PaTelemetryScaling.h"
#include "core/RadioConnection.h"
#include "core/RadioStatus.h"
#include "gui/setup/hardware/PaCalibrationGroup.h"
#include "gui/widgets/MetricLabel.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

namespace NereusSDR {

namespace {

// Shared muted-italic style for placeholder labels — matches the
// "coming in Phase X" tone used by the existing Power & PA fake-PA-group
// placeholder this category eventually replaces.
constexpr auto kPlaceholderStyle =
    "QLabel { color: #607080; font-style: italic; padding: 8px; }";

// Build a placeholder label, install the muted style, disable focus.
QLabel* buildPlaceholderLabel(const QString& body)
{
    auto* lbl = new QLabel(body);
    lbl->setStyleSheet(QString::fromLatin1(kPlaceholderStyle));
    lbl->setWordWrap(true);
    lbl->setEnabled(false);                  // visual-only; never accepts focus
    lbl->setTextInteractionFlags(Qt::NoTextInteraction);
    return lbl;
}

} // namespace

// ── PaGainByBandPage ─────────────────────────────────────────────────────────
//
// Mirrors Thetis tpGainByBand (setup.designer.cs:47386-47525 [v2.10.3.13])
// + chkAutoPACalibrate (setup.designer.cs:49084 [v2.10.3.13]).
//
// Phase 6 of issue #167 promotes this from placeholder to full live editor.
// All 14 controls in the parity matrix are wired through PaProfileManager
// (Phase 2B). Layout is a top-row toolbar (combo + lifecycle buttons +
// warning + chkPANewCal) over a 14-row gain grid (gain + 9 adjusts +
// max-power + use-max), with chkAutoPACalibrate at the bottom (Phase 7
// owns the actual sweep flow).
namespace {

// Per-row column ordering.  `kAdjustColumnFirst` is the column index of the
// first adjust spinbox (drive 10%); `kColAdjustCount` is how many follow
// before the per-band max-power column starts.
constexpr int kColBand          = 0;
constexpr int kColGain          = 1;
constexpr int kAdjustColumnFirst = 2;
constexpr int kColAdjustCount   = 9;
constexpr int kColMaxPower      = kAdjustColumnFirst + kColAdjustCount;     // 11
constexpr int kColUseMaxPower   = kColMaxPower + 1;                         // 12

// Build a styled QDoubleSpinBox configured for PA gain or adjust input
// (range 0..100 dB or -10..+10 dB).  `step` is the single-step increment.
QDoubleSpinBox* buildSpin(double minimum, double maximum, double step,
                          int decimals, QWidget* parent)
{
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(minimum, maximum);
    spin->setSingleStep(step);
    spin->setDecimals(decimals);
    spin->setKeyboardTracking(false);  // emit valueChanged on commit, not on every keystroke
    return spin;
}

// True if the given Band participates in the 14-band PA editor surface.
// SWL bands are silently no-op'd by PaProfile setters; we hide them from
// the editor entirely.
bool isPaEditableBand(Band b) noexcept {
    const int idx = static_cast<int>(b);
    return idx >= 0 && idx < PaGainByBandPage::kPaBandCount;
}

}  // namespace

PaGainByBandPage::PaGainByBandPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("PA Gain"), model, parent)
{
    // Resolve PaProfileManager from the model. Without it (model-less preview)
    // we render an inert placeholder — same fallback PaWattMeterPage uses.
    if (!model || !model->paProfileManager()) {
        auto* lbl = buildPlaceholderLabel(QStringLiteral(
            "PA Gain -- requires a connected RadioModel with PaProfileManager.\n"
            "\n"
            "Source: Thetis setup.designer.cs:47386-47525 [v2.10.3.13]"));
        contentLayout()->insertWidget(contentLayout()->count() - 1, lbl);
        return;
    }
    m_paProfileManager = model->paProfileManager();
    m_connectedModel = model->hardwareProfile().model;

    // ── Top toolbar row: combo + lifecycle buttons ───────────────────────
    // From Thetis comboPAProfile + btnNewPAProfile / btnCopyPAProfile /
    // btnDeletePAProfile / btnResetPAProfile [v2.10.3.13].
    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(6);

    m_profileCombo = new QComboBox(this);
    m_profileCombo->setMinimumWidth(220);
    m_profileCombo->setToolTip(QStringLiteral(
        "Active PA gain profile.  Per-band PA gain compensation drives the "
        "audio-volume scalar that prevents over-power on high-gain finals."));
    toolbar->addWidget(m_profileCombo, 1);

    m_btnNew = new QPushButton(QStringLiteral("New"), this);
    m_btnNew->setToolTip(QStringLiteral(
        "Create a new empty profile seeded from the connected radio's "
        "factory PA gain row."));
    toolbar->addWidget(m_btnNew);

    m_btnCopy = new QPushButton(QStringLiteral("Copy"), this);
    m_btnCopy->setToolTip(QStringLiteral(
        "Duplicate the active profile under a new name."));
    toolbar->addWidget(m_btnCopy);

    m_btnDelete = new QPushButton(QStringLiteral("Delete"), this);
    m_btnDelete->setToolTip(QStringLiteral(
        "Delete the active profile.  The last remaining profile cannot be "
        "deleted."));
    toolbar->addWidget(m_btnDelete);

    m_btnReset = new QPushButton(QStringLiteral("Reset Defaults"), this);
    m_btnReset->setToolTip(QStringLiteral(
        "Re-seed the active profile from the canonical factory PA gain row "
        "for its model.  Drive-step adjusts and max-power columns are "
        "cleared."));
    toolbar->addWidget(m_btnReset);

    contentLayout()->insertLayout(contentLayout()->count() - 1, toolbar);

    // ── Warning row: icon + label + chkPANewCal ──────────────────────────
    // From Thetis pbPAProfileWarning + lblPAProfileWarning + chkPANewCal
    // [v2.10.3.13].  Visible only when the active profile diverges from
    // canonical factory values.
    auto* warningRow = new QHBoxLayout;
    warningRow->setSpacing(6);

    m_warningIcon = new QLabel(this);
    m_warningIcon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning)
                                 .pixmap(20, 20));
    m_warningIcon->setVisible(false);
    warningRow->addWidget(m_warningIcon);

    m_warningLabel = new QLabel(QStringLiteral(
        "PA profile has been modified from factory defaults."), this);
    m_warningLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #d8a000; font-style: italic; }"));
    m_warningLabel->setVisible(false);
    warningRow->addWidget(m_warningLabel, 1);

    m_newCalCheck = new QCheckBox(QStringLiteral("New Cal"), this);
    m_newCalCheck->setToolTip(QStringLiteral(
        "Thetis-specific 'new calibration' mode marker.  No NereusSDR-side "
        "behavior is hooked to it yet — tracked for parity only."));
    warningRow->addWidget(m_newCalCheck);

    contentLayout()->insertLayout(contentLayout()->count() - 1, warningRow);

    // ── Main grid: PA Gain by Band (dB) ──────────────────────────────────
    // 14 bands x (band label + gain + 9 adjusts + max-power + use-max)
    auto* gainGroup = new QGroupBox(QStringLiteral("PA Gain by Band (dB)"), this);
    auto* grid = new QGridLayout(gainGroup);
    grid->setHorizontalSpacing(4);
    grid->setVerticalSpacing(2);

    // Header row.  From Thetis grpGainByBandPA layout (setup.designer.cs:
    // 47420-47525 [v2.10.3.13]) — labels are NereusSDR-original (Thetis
    // pre-renders headers via lblDriveHeader + per-step lblPAAdjustNN labels;
    // we use a plain QGridLayout header row).
    int row = 0;
    auto* hdrBand = new QLabel(QStringLiteral("Band"), gainGroup);
    auto* hdrGain = new QLabel(QStringLiteral("Gain (dB)"), gainGroup);
    grid->addWidget(hdrBand, row, kColBand);
    grid->addWidget(hdrGain, row, kColGain);
    for (int step = 0; step < kColAdjustCount; ++step) {
        const int drivePct = (step + 1) * 10;
        auto* lbl = new QLabel(QStringLiteral("%1%").arg(drivePct), gainGroup);
        grid->addWidget(lbl, row, kAdjustColumnFirst + step);
    }
    auto* hdrMax = new QLabel(QStringLiteral("Max W"), gainGroup);
    auto* hdrUse = new QLabel(QStringLiteral("Use Max"), gainGroup);
    grid->addWidget(hdrMax, row, kColMaxPower);
    grid->addWidget(hdrUse, row, kColUseMaxPower);
    ++row;

    // Per-band rows: 14 entries.
    // From Thetis nud160M..nudVHF13 setup.designer.cs:47386-47525 [v2.10.3.13].
    for (int n = 0; n < kPaBandCount; ++n) {
        const Band band = static_cast<Band>(n);
        if (!isPaEditableBand(band)) {
            continue;
        }

        // Band label.
        auto* bandLbl = new QLabel(bandLabel(band), gainGroup);
        bandLbl->setMinimumWidth(48);
        grid->addWidget(bandLbl, row, kColBand);

        // Per-band gain spinbox (0..100 dB, 0.1 step, 1 decimal).
        m_gainSpins[n] = buildSpin(0.0, 100.0, 0.1, 1, gainGroup);
        m_gainSpins[n]->setToolTip(QStringLiteral(
            "PA gain compensation for %1 in dB.  Subtracted from the "
            "target dBm to compute the audio-volume scalar that drives "
            "the radio's TX FIFO.").arg(bandLabel(band)));
        grid->addWidget(m_gainSpins[n], row, kColGain);
        wireGainSpin(m_gainSpins[n], band);

        // Per-band drive-step adjust matrix — NereusSDR-spin densification.
        // From Thetis panelAdjustGain [v2.10.3.13] (Thetis ships only the
        // row for the currently-selected band; NereusSDR exposes all 14 x 9
        // cells so the user does not have to scrub through bands to see
        // their full per-step compensation table).
        for (int step = 0; step < kColAdjustCount; ++step) {
            auto* spin = buildSpin(-10.0, 10.0, 0.1, 1, gainGroup);
            spin->setToolTip(QStringLiteral(
                "Per-step adjust at %1%% drive for %2.")
                .arg((step + 1) * 10).arg(bandLabel(band)));
            m_adjustSpins[n][step] = spin;
            grid->addWidget(spin, row, kAdjustColumnFirst + step);
            wireAdjustSpin(spin, band, step);
        }

        // Per-band max-power column.
        // From Thetis nudMaxPowerForBandPA + chkUsePowerOnDrvTunPA [v2.10.3.13].
        m_maxPowerSpins[n] = buildSpin(0.0, 1500.0, 1.0, 0, gainGroup);
        m_maxPowerSpins[n]->setToolTip(QStringLiteral(
            "Per-band max-power ceiling in watts for %1.")
            .arg(bandLabel(band)));
        grid->addWidget(m_maxPowerSpins[n], row, kColMaxPower);
        wireMaxPowerSpin(m_maxPowerSpins[n], band);

        m_useMaxPowerChecks[n] = new QCheckBox(gainGroup);
        m_useMaxPowerChecks[n]->setToolTip(QStringLiteral(
            "Apply the per-band max-power ceiling on %1.")
            .arg(bandLabel(band)));
        grid->addWidget(m_useMaxPowerChecks[n], row, kColUseMaxPower);
        wireUseMaxCheck(m_useMaxPowerChecks[n], band);

        ++row;
    }

    contentLayout()->insertWidget(contentLayout()->count() - 1, gainGroup);

    // ── Bottom: chkAutoPACalibrate ───────────────────────────────────────
    // From Thetis chkAutoPACalibrate setup.designer.cs:49084 [v2.10.3.13].
    // Phase 6 only renders the toggle; Phase 7 wires the actual sweep flow.
    m_autoCalibrateCheck = new QCheckBox(
        QStringLiteral("Auto-Calibrate (sweep) — coming in Phase 7"), this);
    m_autoCalibrateCheck->setToolTip(QStringLiteral(
        "Enable the per-band auto-calibration sweep.  The sweep state "
        "machine itself is not yet wired (Phase 7 of #167)."));
    contentLayout()->insertWidget(contentLayout()->count() - 1,
                                   m_autoCalibrateCheck);

    // ── Wire profile-management buttons + combo ──────────────────────────
    connect(m_profileCombo, &QComboBox::currentTextChanged,
            this, &PaGainByBandPage::onProfileSelected);
    connect(m_btnNew,    &QPushButton::clicked, this, &PaGainByBandPage::onNewProfile);
    connect(m_btnCopy,   &QPushButton::clicked, this, &PaGainByBandPage::onCopyProfile);
    connect(m_btnDelete, &QPushButton::clicked, this, &PaGainByBandPage::onDeleteProfile);
    connect(m_btnReset,  &QPushButton::clicked, this, &PaGainByBandPage::onResetDefaults);

    // Subscribe to PaProfileManager publishes so external mutations (e.g.
    // future auto-cal sweep) refresh the combo.
    connect(m_paProfileManager, &PaProfileManager::profileListChanged,
            this, &PaGainByBandPage::rebuildProfileCombo);
    connect(m_paProfileManager, &PaProfileManager::activeProfileChanged,
            this, [this](const QString& name) {
                if (m_profileCombo && m_profileCombo->currentText() != name) {
                    QSignalBlocker blocker(m_profileCombo);
                    m_profileCombo->setCurrentText(name);
                }
                if (const PaProfile* p = m_paProfileManager->profileByName(name)) {
                    loadProfileIntoUi(*p);
                }
            });

    // Initial population.
    rebuildProfileCombo();
    if (const PaProfile* active = m_paProfileManager->activeProfile()) {
        loadProfileIntoUi(*active);
    }
}

// ── Combo + profile-load plumbing ────────────────────────────────────────────

void PaGainByBandPage::rebuildProfileCombo()
{
    if (!m_profileCombo || !m_paProfileManager) { return; }

    QSignalBlocker blocker(m_profileCombo);

    const QString prevActive = m_paProfileManager->activeProfileName();
    m_profileCombo->clear();
    const QStringList names = m_paProfileManager->profileNames();
    for (const QString& n : names) {
        m_profileCombo->addItem(n);
    }
    if (!prevActive.isEmpty() && names.contains(prevActive)) {
        m_profileCombo->setCurrentText(prevActive);
    }
}

void PaGainByBandPage::loadProfileIntoUi(const PaProfile& profile)
{
    m_updatingFromProfile = true;

    for (int n = 0; n < kPaBandCount; ++n) {
        const Band band = static_cast<Band>(n);
        if (m_gainSpins[n]) {
            m_gainSpins[n]->setValue(profile.getGainForBand(band, /*driveValue=*/-1));
        }
        for (int step = 0; step < kColAdjustCount; ++step) {
            if (m_adjustSpins[n][step]) {
                m_adjustSpins[n][step]->setValue(profile.getAdjust(band, step));
            }
        }
        if (m_maxPowerSpins[n]) {
            m_maxPowerSpins[n]->setValue(profile.getMaxPower(band));
        }
        if (m_useMaxPowerChecks[n]) {
            m_useMaxPowerChecks[n]->setChecked(profile.getMaxPowerUse(band));
        }
    }

    m_updatingFromProfile = false;

    warnIfProfileDiverged();
}

void PaGainByBandPage::warnIfProfileDiverged()
{
    if (!m_warningIcon || !m_warningLabel || !m_paProfileManager) { return; }

    const PaProfile* active = m_paProfileManager->activeProfile();
    if (!active) {
        m_warningIcon->setVisible(false);
        m_warningLabel->setVisible(false);
        return;
    }

    // Compare against the canonical factory row for the active profile's
    // OWN model.  This is the "factory default for this profile" — not the
    // canonical for the connected radio.  A user-renamed profile that
    // started life as Default-ANAN8000D is always compared against the
    // 8000DLE row, regardless of what radio is connected now.
    bool diverged = false;
    for (int n = 0; n < kPaBandCount; ++n) {
        const Band band = static_cast<Band>(n);
        const float actual    = active->getGainForBand(band, /*driveValue=*/-1);
        const float canonical = defaultPaGainsForBand(active->model(), band);
        if (std::fabs(actual - canonical) > 0.001f) {
            diverged = true;
            break;
        }
    }

    m_warningIcon->setVisible(diverged);
    m_warningLabel->setVisible(diverged);
}

// ── User-edit handlers ───────────────────────────────────────────────────────

void PaGainByBandPage::onProfileSelected(const QString& name)
{
    if (!m_paProfileManager || name.isEmpty()) { return; }

    // Programmatic selection (rebuildProfileCombo etc.) is suppressed via
    // QSignalBlocker; this slot only fires for genuine user combo changes.
    m_paProfileManager->setActiveProfile(name);
    if (const PaProfile* p = m_paProfileManager->profileByName(name)) {
        loadProfileIntoUi(*p);
    }
}

void PaGainByBandPage::onNewProfile()
{
    if (!m_paProfileManager) { return; }

    // Mirrors Thetis btnNewPAProfile_Click (setup.cs:22984-22996 [v2.10.3.13]).
    QString name;
    bool accepted = false;

#ifdef NEREUS_BUILD_TESTS
    if (m_hasPendingProfileNameForTest) {
        name = m_pendingProfileNameForTest;
        accepted = !name.isEmpty();
        m_hasPendingProfileNameForTest = false;
    } else
#endif
    {
        name = QInputDialog::getText(this,
                                     tr("New PA Profile"),
                                     tr("Profile name:"),
                                     QLineEdit::Normal,
                                     QString(),
                                     &accepted);
    }
    if (!accepted) { return; }
    name = name.trimmed();
    if (name.isEmpty()) { return; }

    // Seed from the connected HPSDRModel — same Thetis precedent
    // (HardwareSpecific.Model is the connected model in setup.cs:22991).
    PaProfile p(name, m_connectedModel, /*isFactoryDefault=*/false);
    m_paProfileManager->saveProfile(name, p);
    m_paProfileManager->setActiveProfile(name);
}

void PaGainByBandPage::onCopyProfile()
{
    if (!m_paProfileManager) { return; }

    const PaProfile* active = m_paProfileManager->activeProfile();
    if (!active) { return; }

    // Mirrors Thetis btnCopyPAProfile_Click (setup.cs:22967-22983 [v2.10.3.13]).
    QString name;
    bool accepted = false;

#ifdef NEREUS_BUILD_TESTS
    if (m_hasPendingProfileNameForTest) {
        name = m_pendingProfileNameForTest;
        accepted = !name.isEmpty();
        m_hasPendingProfileNameForTest = false;
    } else
#endif
    {
        name = QInputDialog::getText(this,
                                     tr("Copy PA Profile"),
                                     tr("New profile name:"),
                                     QLineEdit::Normal,
                                     active->name() + tr(" (copy)"),
                                     &accepted);
    }
    if (!accepted) { return; }
    name = name.trimmed();
    if (name.isEmpty()) { return; }

    // Same Thetis pattern: build a new profile not associated with a model
    // (HPSDRModel::FIRST sentinel) then deep-copy the source data over.
    PaProfile copy(name, HPSDRModel::FIRST, /*isFactoryDefault=*/false);
    copy.copySettings(*active);
    m_paProfileManager->saveProfile(name, copy);
    m_paProfileManager->setActiveProfile(name);
}

void PaGainByBandPage::onDeleteProfile()
{
    if (!m_paProfileManager || !m_profileCombo) { return; }

    const QString target = m_profileCombo->currentText();
    if (target.isEmpty()) { return; }

    // Mirrors Thetis btnDeletePAProfile_Click (setup.cs:22998-23025 [v2.10.3.13]).
    bool confirmed = false;

#ifdef NEREUS_BUILD_TESTS
    if (m_hasDeleteConfirmForTest) {
        confirmed = m_deleteConfirmForTest;
        m_hasDeleteConfirmForTest = false;
    } else
#endif
    {
        const auto answer = QMessageBox::question(
            this,
            tr("Delete PA Profile"),
            tr("Delete profile \"%1\"?").arg(target),
            QMessageBox::Yes | QMessageBox::No);
        confirmed = (answer == QMessageBox::Yes);
    }

    if (!confirmed) { return; }

    const bool ok = m_paProfileManager->deleteProfile(target);
    if (!ok) {
        // Manager refused (last-remaining-profile guard).  Surface the
        // verbatim Thetis precedent string used by MicProfileManager.
        QMessageBox::information(
            this, tr("Cannot Delete Profile"),
            tr("It is not possible to delete the last remaining PA profile."));
    }
}

void PaGainByBandPage::onResetDefaults()
{
    if (!m_paProfileManager) { return; }

    // Mirrors Thetis btnResetPAProfile_Click (setup.cs:23161+ [v2.10.3.13]).
    bool confirmed = false;

#ifdef NEREUS_BUILD_TESTS
    if (m_hasResetConfirmForTest) {
        confirmed = m_resetConfirmForTest;
        m_hasResetConfirmForTest = false;
    } else
#endif
    {
        const auto answer = QMessageBox::question(
            this,
            tr("Reset PA Profile"),
            tr("Reset the active profile to factory defaults?"),
            QMessageBox::Yes | QMessageBox::No);
        confirmed = (answer == QMessageBox::Yes);
    }

    if (!confirmed) { return; }

    const PaProfile* active = m_paProfileManager->activeProfile();
    if (!active) { return; }

    // Reset the active profile in place (matches Thetis ResetGainDefaultsForModel
    // setup.cs:24027-24046 [v2.10.3.13]) then save back through the manager.
    PaProfile reset = *active;
    reset.resetGainDefaultsForModel(active->model());
    m_paProfileManager->saveProfile(active->name(), reset);
    loadProfileIntoUi(reset);
}

void PaGainByBandPage::onGainChanged(Band band, double value)
{
    if (m_updatingFromProfile || !m_paProfileManager) { return; }

    const PaProfile* active = m_paProfileManager->activeProfile();
    if (!active) { return; }

    PaProfile mutated = *active;
    mutated.setGainForBand(band, static_cast<float>(value));
    m_paProfileManager->saveProfile(active->name(), mutated);
    warnIfProfileDiverged();
}

void PaGainByBandPage::onAdjustChanged(Band band, int step, double value)
{
    if (m_updatingFromProfile || !m_paProfileManager) { return; }

    const PaProfile* active = m_paProfileManager->activeProfile();
    if (!active) { return; }

    PaProfile mutated = *active;
    mutated.setAdjust(band, step, static_cast<float>(value));
    m_paProfileManager->saveProfile(active->name(), mutated);
}

void PaGainByBandPage::onMaxPowerChanged(Band band, double watts)
{
    if (m_updatingFromProfile || !m_paProfileManager) { return; }

    const PaProfile* active = m_paProfileManager->activeProfile();
    if (!active) { return; }

    PaProfile mutated = *active;
    mutated.setMaxPower(band, static_cast<float>(watts));
    m_paProfileManager->saveProfile(active->name(), mutated);
}

void PaGainByBandPage::onUseMaxPowerToggled(Band band, bool use)
{
    if (m_updatingFromProfile || !m_paProfileManager) { return; }

    const PaProfile* active = m_paProfileManager->activeProfile();
    if (!active) { return; }

    PaProfile mutated = *active;
    mutated.setMaxPowerUse(band, use);
    m_paProfileManager->saveProfile(active->name(), mutated);
}

// ── Wiring helpers ───────────────────────────────────────────────────────────

void PaGainByBandPage::wireGainSpin(QDoubleSpinBox* spin, Band band)
{
    if (!spin) { return; }
    connect(spin,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this, band](double v) { onGainChanged(band, v); });
}

void PaGainByBandPage::wireAdjustSpin(QDoubleSpinBox* spin, Band band, int step)
{
    if (!spin) { return; }
    connect(spin,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this, band, step](double v) { onAdjustChanged(band, step, v); });
}

void PaGainByBandPage::wireMaxPowerSpin(QDoubleSpinBox* spin, Band band)
{
    if (!spin) { return; }
    connect(spin,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this, band](double v) { onMaxPowerChanged(band, v); });
}

void PaGainByBandPage::wireUseMaxCheck(QCheckBox* check, Band band)
{
    if (!check) { return; }
    connect(check, &QCheckBox::toggled,
            this, [this, band](bool checked) {
                onUseMaxPowerToggled(band, checked);
            });
}

#ifdef NEREUS_BUILD_TESTS
QDoubleSpinBox* PaGainByBandPage::gainSpinForTest(Band b) const
{
    const int idx = static_cast<int>(b);
    if (idx < 0 || idx >= kPaBandCount) { return nullptr; }
    return m_gainSpins[idx];
}

QDoubleSpinBox* PaGainByBandPage::adjustSpinForTest(Band b, int step) const
{
    const int idx = static_cast<int>(b);
    if (idx < 0 || idx >= kPaBandCount) { return nullptr; }
    if (step < 0 || step >= 9) { return nullptr; }
    return m_adjustSpins[idx][step];
}

QDoubleSpinBox* PaGainByBandPage::maxPowerSpinForTest(Band b) const
{
    const int idx = static_cast<int>(b);
    if (idx < 0 || idx >= kPaBandCount) { return nullptr; }
    return m_maxPowerSpins[idx];
}

QCheckBox* PaGainByBandPage::useMaxPowerCheckForTest(Band b) const
{
    const int idx = static_cast<int>(b);
    if (idx < 0 || idx >= kPaBandCount) { return nullptr; }
    return m_useMaxPowerChecks[idx];
}
#endif

// ── PaWattMeterPage ──────────────────────────────────────────────────────────
//
// Mirrors Thetis tpWattMeter (setup.designer.cs:49304-49309 [v2.10.3.13]).
// Phase 3A hosts the per-board PA forward-power cal-point spinbox group
// (PaCalibrationGroup).
//
// Phase 5 Agent 5A of #167 closes the gap to full Thetis parity by adding:
//   * chkPAValues        — "Show PA Values page" checkbox; toggles
//                           AppSettings "display/showPaValuesPage" so the
//                           SetupDialog navigation (or PaValuesPage itself)
//                           can hide the dedicated PA Values page when the
//                           user prefers a leaner Setup tree.  Maps to
//                           Thetis chkPAValues_CheckedChanged
//                           (setup.cs:16340-16343 [v2.10.3.13]) which
//                           toggles panelPAValues.Visible inline; NereusSDR
//                           replaces the inline panel with a separate page
//                           plus a settings hint.
//   * btnResetPAValues   — "Reset PA Values" button; emits
//                           resetPaValuesRequested() so PaValuesPage owns
//                           the peak/min reset slot.  Maps to Thetis
//                           btnResetPAValues_Click (setup.cs:16346-16357
//                           [v2.10.3.13]) which blanks readout text fields
//                           directly; NereusSDR splits controller (this
//                           page) from state owner (PaValuesPage) via a
//                           Qt signal.
//
// Wiring mirrors what previously lived in CalibrationTab.cpp:
//   * Construct PaCalibrationGroup parented to this page.
//   * Initial populate() from the controller's current PaCalProfile board class
//     (None on a fresh model — group will hide).
//   * Subscribe to paCalProfileChanged so the spinbox set rebuilds whenever the
//     bound radio's board class changes (radio swap or first connect).
//
// Source: Thetis console.cs:6691-6724 CalibratedPAPower [v2.10.3.13] — per-
// board cal-table family selection.
PaWattMeterPage::PaWattMeterPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Watt Meter"), model, parent)
{
    if (!model) {
        // Without a model we have nothing to populate the cal group from.
        // Fall back to a brief placeholder so tests / model-less previews still
        // render something coherent.
        // ASCII em-dash (--) for the disabled-italic placeholder; see
        // PaGainByBandPage above for the rationale.
        auto* lbl = buildPlaceholderLabel(QStringLiteral(
            "Watt Meter -- requires a connected radio model.\n"
            "\n"
            "Source: Thetis setup.designer.cs:49304-49309 [v2.10.3.13]"));
        contentLayout()->insertWidget(contentLayout()->count() - 1, lbl);
        // Note: the chkPAValues toggle and btnResetPAValues button are
        // model-independent (settings + signal only), so we fall through to
        // the shared block below to wire them in even in the model-less
        // fallback path.  This keeps the page's surface area consistent
        // across construction modes for tests and model-less previews.
    } else {
        auto* calCtrl = &model->calibrationControllerMutable();
        m_paCalGroup = new PaCalibrationGroup(this);

        // Insert before the trailing stretch SetupPage::init() appended so
        // the group renders flush with the page top — matches
        // PaGainByBandPage above.
        contentLayout()->insertWidget(contentLayout()->count() - 1, m_paCalGroup);

        // Initial populate from the controller's current profile (None hides
        // the group; live profile change rebuilds via the lambda below).
        m_paCalGroup->populate(calCtrl, calCtrl->paCalProfile().boardClass);

        // Repopulate on radio swap / first connect — board class can flip
        // from None → Anan10/100/8000 once RadioModel seeds m_paCalProfile
        // from the hardware profile.  Mirrors the connect block previously
        // living in CalibrationTab::CalibrationTab (Section 3.3 of the P1
        // full-parity epic).
        connect(calCtrl, &CalibrationController::paCalProfileChanged,
                this, [this, calCtrl]() {
            if (m_paCalGroup) {
                m_paCalGroup->populate(calCtrl, calCtrl->paCalProfile().boardClass);
            }
        });
    }

    // ── chkPAValues "Show PA Values page" toggle ────────────────────────────
    // From Thetis chkPAValues setup.designer.cs:51155-51177 [v2.10.3.13]
    // + setup.cs:16340-16343 toggle.  Thetis flips panelPAValues.Visible
    // inline; NereusSDR persists the visibility hint to AppSettings under
    // "display/showPaValuesPage" (default "True") so the dedicated
    // PaValuesPage / SetupDialog navigation can honor it.
    m_showPaValuesCheck = new QCheckBox(tr("Show PA Values page"), this);
    m_showPaValuesCheck->setObjectName(QStringLiteral("chkPAValues"));
    // Tooltip is user-visible plain English; Thetis cite kept in the
    // surrounding header comment (and the From Thetis cite block above)
    // per CLAUDE.md "No source cites in user-visible strings".
    m_showPaValuesCheck->setToolTip(tr(
        "Toggle visibility of the PA Values page in Setup navigation."));
    const bool showPaValues = AppSettings::instance().value(
        QStringLiteral("display/showPaValuesPage"),
        QStringLiteral("True")).toString() == QStringLiteral("True");
    m_showPaValuesCheck->setChecked(showPaValues);
    connect(m_showPaValuesCheck, &QCheckBox::toggled, this,
            [](bool checked) {
        AppSettings::instance().setValue(
            QStringLiteral("display/showPaValuesPage"),
            checked ? QStringLiteral("True") : QStringLiteral("False"));
    });
    contentLayout()->insertWidget(contentLayout()->count() - 1, m_showPaValuesCheck);

    // ── btnResetPAValues "Reset PA Values" button ───────────────────────────
    // From Thetis btnResetPAValues setup.designer.cs:51155-51177 [v2.10.3.13]
    // + setup.cs:16346-16357 click handler.  Thetis blanks readout text
    // directly on click; NereusSDR emits resetPaValuesRequested so the
    // separate PaValuesPage (Phase 5B) can subscribe and clear its
    // peak/min tracking.
    m_resetPaValuesButton = new QPushButton(tr("Reset PA Values"), this);
    m_resetPaValuesButton->setObjectName(QStringLiteral("btnResetPAValues"));
    // Tooltip is user-visible plain English; Thetis cite kept in the
    // surrounding header comment per CLAUDE.md "No source cites in
    // user-visible strings".
    m_resetPaValuesButton->setToolTip(tr(
        "Reset peak/min tracking on the PA Values page."));
    connect(m_resetPaValuesButton, &QPushButton::clicked, this,
            &PaWattMeterPage::resetPaValuesRequested);
    contentLayout()->insertWidget(contentLayout()->count() - 1, m_resetPaValuesButton);
}

#ifdef NEREUS_BUILD_TESTS
bool PaWattMeterPage::showPaValuesCheckedForTest() const
{
    return m_showPaValuesCheck && m_showPaValuesCheck->isChecked();
}

void PaWattMeterPage::clickResetPaValuesForTest()
{
    if (m_resetPaValuesButton) {
        m_resetPaValuesButton->click();
    }
}
#endif

// ── PaValuesPage ─────────────────────────────────────────────────────────────
//
// NereusSDR-spin: Thetis embeds panelPAValues (setup.designer.cs:51155-51177
// [v2.10.3.13]) inside its Watt Meter tab — a 11-field readout block (calibrated
// FWD power, raw FWD power, REV power, SWR, DC volts, drive power, drive-FWD
// ADC, FWD ADC, REV ADC, FWD voltage, REV voltage) plus a Reset button gated
// on chkPAValues.  We promote it to a dedicated page so live telemetry can be
// enriched without crowding the cal-point editor.
//
// Phase 4 binds the page to two upstream signal sources:
//
//   * RadioStatus  — calibrated FWD power, REV power, SWR, PA current,
//                    PA temperature.  RadioStatus aggregates the fwd/rev/swr
//                    triple into a single powerChanged(fwd, rev, swr) signal,
//                    so the FWD-calibrated, REV-power, and SWR labels all
//                    subscribe to the same signal.
//   * RadioConnection — supply volts, raw FWD/REV ADC counts (via the unified
//                    paTelemetryUpdated signal), and ADC overflow events.
//
// Phase 5B (#167) closes the four panelPAValues label gaps:
//
//   * Raw FWD power (W)  — scaled via PaTelemetryScaling::scaleFwdPowerWatts
//     (Phase 1 Agent 1B; Thetis textPAFwdPower).
//   * Drive (W)          — slider position from TransmitModel::power().
//                          Thetis textDrivePower shows averaged exciter
//                          drive (mW) — NereusSDR uses the slider in W
//                          since it's already what the user just set.
//   * FWD voltage (V)    — PaTelemetryScaling::scaleFwdRevVoltage
//                          (Thetis textFwdVoltage).
//   * REV voltage (V)    — same scaler reused per Phase 1B's combined-API
//                          design note (Thetis textRevVoltage).
//
// Plus running peak/min tracking on six telemetry values (FWD calibrated /
// REV / SWR / PA current / PA temperature / supply volts) and a Reset
// button.  Reset clears peak/min back to the current value, mirroring the
// semantic (not the literal text-clear) of Thetis btnResetPAValues_Click
// at setup.cs:16346-16357 [v2.10.3.13].  The public resetPaValues() slot
// is intended to be cross-wired to Phase 5A's PaWattMeterPage::
// resetPaValuesRequested signal at SetupDialog construction time.
//
// All connect() calls capture `this` and Qt auto-disconnects on destruction.
PaValuesPage::PaValuesPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("PA Values"), model, parent)
{
    if (!model) {
        // Model-less preview path: render a brief hint label so the page
        // doesn't ship as an empty widget when Setup is opened before a
        // RadioModel is wired (mirrors the PaWattMeterPage fallback).
        auto* lbl = buildPlaceholderLabel(QStringLiteral(
            "PA Values — requires a connected radio model.\n"
            "\n"
            "Source: Thetis panelPAValues setup.designer.cs:51155-51177 [v2.10.3.13]"));
        contentLayout()->insertWidget(contentLayout()->count() - 1, lbl);
        return;
    }

    // ── Group: Power ──────────────────────────────────────────────────────
    // Calibrated FWD (post-CalibratedPAPower) + raw FWD + REV + SWR + drive.
    // From Thetis panelPAValues setup.designer.cs:51155-51177 [v2.10.3.13]:
    //   textCaldFwdPower / textPAFwdPower / textPARevPower / textSWR /
    //   textDrivePower.
    auto* powerGroup  = new QGroupBox(QStringLiteral("Power"), this);
    auto* powerForm   = new QFormLayout(powerGroup);
    m_fwdCalibratedLabel = new MetricLabel(QStringLiteral("FWD (cal)"),
                                           QStringLiteral("0.00 W"), powerGroup);
    // Phase 5B (#167) — Raw FWD power label, scaled from raw ADC counts via
    // PaTelemetryScaling helpers (Phase 1B).
    // From Thetis panelPAValues textPAFwdPower setup.designer.cs:51155-51177
    // [v2.10.3.13] — `alex_fwd.ToString("f1") + " W"` at console.cs:24670.
    m_fwdRawLabel        = new MetricLabel(QStringLiteral("FWD (raw)"),
                                           QStringLiteral("0.00 W"), powerGroup);
    m_revPowerLabel      = new MetricLabel(QStringLiteral("REV"),
                                           QStringLiteral("0.00 W"), powerGroup);
    m_swrLabel           = new MetricLabel(QStringLiteral("SWR"),
                                           QStringLiteral("1.00"), powerGroup);
    // Phase 5B (#167) — Drive label, populated from TransmitModel::power().
    // From Thetis panelPAValues textDrivePower setup.designer.cs:51155-51177
    // [v2.10.3.13] — Thetis renders averaged exciter drive in mW; NereusSDR
    // uses the slider position in W (it's already what the user just set
    // and the slider 0..100 maps directly to watts on most boards).
    m_driveLabel         = new MetricLabel(QStringLiteral("Drive"),
                                           QStringLiteral("0 W"), powerGroup);
    powerForm->addRow(QStringLiteral("Forward (calibrated):"), m_fwdCalibratedLabel);
    powerForm->addRow(QStringLiteral("Forward (raw):"),        m_fwdRawLabel);
    powerForm->addRow(QStringLiteral("Reflected:"),            m_revPowerLabel);
    powerForm->addRow(QStringLiteral("SWR:"),                  m_swrLabel);
    powerForm->addRow(QStringLiteral("Drive:"),                m_driveLabel);
    contentLayout()->insertWidget(contentLayout()->count() - 1, powerGroup);

    // ── Group: PA Telemetry ───────────────────────────────────────────────
    auto* paGroup = new QGroupBox(QStringLiteral("PA Telemetry"), this);
    auto* paForm  = new QFormLayout(paGroup);
    m_paCurrentLabel   = new MetricLabel(QStringLiteral("PA I"),
                                         QStringLiteral("0.00 A"), paGroup);
    m_paTempLabel      = new MetricLabel(QStringLiteral("PA T"),
                                         QStringLiteral("0.0 \xC2\xB0""C"), paGroup);
    m_supplyVoltsLabel = new MetricLabel(QStringLiteral("V"),
                                         QStringLiteral("0.0 V"), paGroup);
    // Phase 5B (#167) — FWD/REV RF voltage labels, derived from raw ADC
    // via PaTelemetryScaling::scaleFwdRevVoltage (Phase 1B).
    // From Thetis panelPAValues textFwdVoltage / textRevVoltage at
    // setup.designer.cs:51155-51177 [v2.10.3.13] — `volts.ToString("f2") + " V"`
    // at console.cs:25068 / :25002.
    m_fwdVoltageLabel  = new MetricLabel(QStringLiteral("FWD V"),
                                         QStringLiteral("0.00 V"), paGroup);
    m_revVoltageLabel  = new MetricLabel(QStringLiteral("REV V"),
                                         QStringLiteral("0.00 V"), paGroup);
    m_adcOverloadLabel = new MetricLabel(QStringLiteral("ADC OVF"),
                                         QStringLiteral("No"), paGroup);
    paForm->addRow(QStringLiteral("PA Current:"),     m_paCurrentLabel);
    paForm->addRow(QStringLiteral("PA Temperature:"), m_paTempLabel);
    paForm->addRow(QStringLiteral("Supply Voltage:"), m_supplyVoltsLabel);
    paForm->addRow(QStringLiteral("FWD Voltage:"),    m_fwdVoltageLabel);
    paForm->addRow(QStringLiteral("REV Voltage:"),    m_revVoltageLabel);
    paForm->addRow(QStringLiteral("ADC Overload:"),   m_adcOverloadLabel);
    contentLayout()->insertWidget(contentLayout()->count() - 1, paGroup);

    // ── Group: Raw ADC Values ─────────────────────────────────────────────
    auto* adcGroup = new QGroupBox(QStringLiteral("Raw ADC Values"), this);
    auto* adcForm  = new QFormLayout(adcGroup);
    m_fwdAdcLabel = new MetricLabel(QStringLiteral("FWD"),
                                    QStringLiteral("0"), adcGroup);
    m_revAdcLabel = new MetricLabel(QStringLiteral("REV"),
                                    QStringLiteral("0"), adcGroup);
    adcForm->addRow(QStringLiteral("FWD ADC:"), m_fwdAdcLabel);
    adcForm->addRow(QStringLiteral("REV ADC:"), m_revAdcLabel);
    contentLayout()->insertWidget(contentLayout()->count() - 1, adcGroup);

    // ── Reset Peak/Min button ────────────────────────────────────────────
    // Mirrors Thetis btnResetPAValues setup.cs:16346-16357 [v2.10.3.13].
    // Thetis clears the textbox text strings; NereusSDR's spin tracks
    // running peak/min and resets those to the current value (more useful
    // than blanking the display since the live value is still meaningful).
    auto* resetRow    = new QWidget(this);
    auto* resetLayout = new QHBoxLayout(resetRow);
    resetLayout->setContentsMargins(0, 0, 0, 0);
    resetLayout->addStretch(1);
    m_resetButton = new QPushButton(tr("Reset Peak/Min"), resetRow);
    m_resetButton->setToolTip(tr("Reset running peak/min trackers to current values."));
    resetLayout->addWidget(m_resetButton);
    contentLayout()->insertWidget(contentLayout()->count() - 1, resetRow);

    connect(m_resetButton, &QPushButton::clicked,
            this, &PaValuesPage::resetPaValues);

    // ── RadioStatus subscriptions ─────────────────────────────────────────
    // RadioStatus aggregates fwd/rev/swr into a single powerChanged signal,
    // so we subscribe once and update all three labels in the slot.
    // Phase 5B (#167) — also feeds peak/min tracking and re-renders with
    // the (P / M) annotation.
    RadioStatus& rs = model->radioStatus();

    connect(&rs, &RadioStatus::powerChanged, this,
            [this](double fwdW, double revW, double swr) {
                m_fwdCurrent = fwdW;
                m_revCurrent = revW;
                m_swrCurrent = swr;
                m_fwdPeakMin.update(fwdW);
                m_revPeakMin.update(revW);
                m_swrPeakMin.update(swr);
                if (m_fwdCalibratedLabel) {
                    m_fwdCalibratedLabel->setValue(formatWithPeakMin(
                        fwdW, m_fwdPeakMin, QStringLiteral(" W"), 2));
                }
                if (m_revPowerLabel) {
                    m_revPowerLabel->setValue(formatWithPeakMin(
                        revW, m_revPeakMin, QStringLiteral(" W"), 2));
                }
                if (m_swrLabel) {
                    m_swrLabel->setValue(formatWithPeakMin(
                        swr, m_swrPeakMin, QString(), 2));
                }
            });

    connect(&rs, &RadioStatus::paCurrentChanged, this,
            [this](double amps) {
                m_paCurrentCurrent = amps;
                m_paCurrentPeakMin.update(amps);
                if (m_paCurrentLabel) {
                    m_paCurrentLabel->setValue(formatWithPeakMin(
                        amps, m_paCurrentPeakMin, QStringLiteral(" A"), 2));
                }
            });

    connect(&rs, &RadioStatus::paTemperatureChanged, this,
            [this](double celsius) {
                m_paTempCurrent = celsius;
                m_paTempPeakMin.update(celsius);
                if (m_paTempLabel) {
                    m_paTempLabel->setValue(formatWithPeakMin(
                        celsius, m_paTempPeakMin,
                        QStringLiteral(" \xC2\xB0""C"), 1));
                }
            });

    // Populate from current state so the page renders meaningful values
    // even when no signal has fired yet (e.g. opened mid-session after
    // telemetry has already settled).  Peak/min trackers stay at ±inf
    // sentinel values until a real signal lands; formatWithPeakMin omits
    // the (P/M) annotation in that case for a clean first-render.
    m_fwdCurrent       = rs.forwardPowerWatts();
    m_revCurrent       = rs.reflectedPowerWatts();
    m_swrCurrent       = rs.swrRatio();
    m_paCurrentCurrent = rs.paCurrentAmps();
    m_paTempCurrent    = rs.paTemperatureCelsius();
    m_fwdCalibratedLabel->setValue(formatWithPeakMin(
        m_fwdCurrent, m_fwdPeakMin, QStringLiteral(" W"), 2));
    m_revPowerLabel->setValue(formatWithPeakMin(
        m_revCurrent, m_revPeakMin, QStringLiteral(" W"), 2));
    m_swrLabel->setValue(formatWithPeakMin(
        m_swrCurrent, m_swrPeakMin, QString(), 2));
    m_paCurrentLabel->setValue(formatWithPeakMin(
        m_paCurrentCurrent, m_paCurrentPeakMin, QStringLiteral(" A"), 2));
    m_paTempLabel->setValue(formatWithPeakMin(
        m_paTempCurrent, m_paTempPeakMin,
        QStringLiteral(" \xC2\xB0""C"), 1));

    // ── TransmitModel drive subscription (Phase 5B #167) ─────────────────
    // The Drive label tracks the user's TX power slider position via
    // TransmitModel::powerChanged.  Render the current value at
    // construction time too so the label isn't empty for new pages.
    TransmitModel& tx = model->transmitModel();
    auto renderDrive = [this](int watts) {
        if (m_driveLabel) {
            m_driveLabel->setValue(QString::number(watts) + QStringLiteral(" W"));
        }
    };
    connect(&tx, &TransmitModel::powerChanged, this, renderDrive);
    renderDrive(tx.power());

    // ── RadioConnection subscriptions ─────────────────────────────────────
    // Connection may be null at construction time (no active radio); the
    // capture-by-this lambdas + Qt auto-disconnect will keep us safe even if
    // the connection gets replaced later (PaSetupPages reconstructs each
    // time SetupDialog opens, so a stale page will be destroyed first).
    auto* conn = model->connection();
    if (conn) {
        connect(conn, &RadioConnection::supplyVoltsChanged, this,
                [this](float volts) {
                    const double v = static_cast<double>(volts);
                    m_supplyCurrent = v;
                    m_supplyPeakMin.update(v);
                    if (m_supplyVoltsLabel) {
                        m_supplyVoltsLabel->setValue(formatWithPeakMin(
                            v, m_supplyPeakMin, QStringLiteral(" V"), 1));
                    }
                });

        // Capture HPSDRModel by value so the lambda survives any later
        // hardware-profile swap (PaValuesPage reconstructs on Setup-open
        // anyway; this is just defensive against mid-page-life swaps).
        const HPSDRModel hpsdrModel = model->hardwareProfile().model;

        connect(conn, &RadioConnection::paTelemetryUpdated, this,
                [this, hpsdrModel](quint16 fwdRaw, quint16 revRaw,
                                   quint16 /*exciterRaw*/, quint16 /*userAdc0*/,
                                   quint16 /*userAdc1*/,   quint16 /*supply*/) {
                    if (m_fwdAdcLabel) {
                        m_fwdAdcLabel->setValue(QString::number(fwdRaw));
                    }
                    if (m_revAdcLabel) {
                        m_revAdcLabel->setValue(QString::number(revRaw));
                    }
                    // Phase 5B (#167) — Raw FWD power label, scaled via
                    // PaTelemetryScaling::scaleFwdPowerWatts (Phase 1B).
                    // From Thetis console.cs computeAlexFwdPower
                    // [v2.10.3.13] — the raw alex_fwd shown on
                    // textPAFwdPower at console.cs:24670.
                    if (m_fwdRawLabel) {
                        const double watts = scaleFwdPowerWatts(hpsdrModel, fwdRaw);
                        m_fwdRawLabel->setValue(
                            QString::number(watts, 'f', 2)
                            + QStringLiteral(" W"));
                    }
                    // Phase 5B (#167) — FWD/REV voltage labels.  Both use
                    // the FWD-side scaler curve per Phase 1B's combined-API
                    // design (REV-side per-board offset difference is below
                    // the f2 UI display resolution).
                    // From Thetis console.cs:25068 / :25002 [v2.10.3.13].
                    if (m_fwdVoltageLabel) {
                        const double v = scaleFwdRevVoltage(hpsdrModel, fwdRaw);
                        m_fwdVoltageLabel->setValue(
                            QString::number(v, 'f', 2)
                            + QStringLiteral(" V"));
                    }
                    if (m_revVoltageLabel) {
                        const double v = scaleFwdRevVoltage(hpsdrModel, revRaw);
                        m_revVoltageLabel->setValue(
                            QString::number(v, 'f', 2)
                            + QStringLiteral(" V"));
                    }
                });

        connect(conn, &RadioConnection::adcOverflow, this,
                [this](int adc) {
                    if (m_adcOverloadLabel) {
                        m_adcOverloadLabel->setValue(
                            QStringLiteral("Yes (ADC %1)").arg(adc));
                    }
                    // Auto-clear after 2 s.  A real overload re-fires
                    // continuously while present, so the "No" state will
                    // only stick once the radio has settled.
                    QTimer::singleShot(2000, this, [this]() {
                        if (m_adcOverloadLabel) {
                            m_adcOverloadLabel->setValue(QStringLiteral("No"));
                        }
                    });
                });
    }
}

// ---------------------------------------------------------------------------
// resetPaValues — public slot (Phase 5B #167).
// Mirrors Thetis btnResetPAValues_Click semantic at setup.cs:16346-16357
// [v2.10.3.13].  Thetis simply blanks the textbox text strings; NereusSDR's
// spin tracks running peak/min and resets each tracker to its current value
// (so the live readout stays meaningful while history clears).
// ---------------------------------------------------------------------------
void PaValuesPage::resetPaValues()
{
    m_fwdPeakMin.reset(m_fwdCurrent);
    m_revPeakMin.reset(m_revCurrent);
    m_swrPeakMin.reset(m_swrCurrent);
    m_paCurrentPeakMin.reset(m_paCurrentCurrent);
    m_paTempPeakMin.reset(m_paTempCurrent);
    m_supplyPeakMin.reset(m_supplyCurrent);

    // Re-render every tracked label so the (P/M) annotation collapses to
    // P==M==current — the reset is visible to the user immediately even
    // when telemetry is paused (e.g. radio disconnected).
    if (m_fwdCalibratedLabel) {
        m_fwdCalibratedLabel->setValue(formatWithPeakMin(
            m_fwdCurrent, m_fwdPeakMin, QStringLiteral(" W"), 2));
    }
    if (m_revPowerLabel) {
        m_revPowerLabel->setValue(formatWithPeakMin(
            m_revCurrent, m_revPeakMin, QStringLiteral(" W"), 2));
    }
    if (m_swrLabel) {
        m_swrLabel->setValue(formatWithPeakMin(
            m_swrCurrent, m_swrPeakMin, QString(), 2));
    }
    if (m_paCurrentLabel) {
        m_paCurrentLabel->setValue(formatWithPeakMin(
            m_paCurrentCurrent, m_paCurrentPeakMin, QStringLiteral(" A"), 2));
    }
    if (m_paTempLabel) {
        m_paTempLabel->setValue(formatWithPeakMin(
            m_paTempCurrent, m_paTempPeakMin,
            QStringLiteral(" \xC2\xB0""C"), 1));
    }
    if (m_supplyVoltsLabel) {
        m_supplyVoltsLabel->setValue(formatWithPeakMin(
            m_supplyCurrent, m_supplyPeakMin, QStringLiteral(" V"), 1));
    }
}

// ---------------------------------------------------------------------------
// formatWithPeakMin — render "current unit  (P peak / M min)".
// Annotation is omitted (just "current unit") when:
//   * peak/min are still at sentinel ±infinity values (no signal yet), OR
//   * peak == min == current (single sample, range hasn't opened up yet).
// This keeps fresh / single-sample readings clean and only surfaces the
// (P/M) annotation once the value has actually moved.
// ---------------------------------------------------------------------------
QString PaValuesPage::formatWithPeakMin(double current,
                                        const PeakMin& pm,
                                        const QString& unit,
                                        int precision)
{
    QString base = QString::number(current, 'f', precision) + unit;
    if (!pm.valid()) {
        return base;
    }
    // Compare formatted strings so we collapse "0.001 vs 0.002 at f0
    // precision" into a single annotation-free reading — the user can't
    // see the difference anyway.
    const QString peakStr = QString::number(pm.peak, 'f', precision);
    const QString minStr  = QString::number(pm.min,  'f', precision);
    if (peakStr == minStr) {
        return base;
    }
    return base
        + QStringLiteral("  (P ") + peakStr
        + QStringLiteral(" / M ") + minStr
        + QStringLiteral(")");
}

#ifdef NEREUS_BUILD_TESTS
QString PaValuesPage::fwdCalibratedTextForTest() const
{
    return m_fwdCalibratedLabel ? m_fwdCalibratedLabel->value() : QString();
}

QString PaValuesPage::revPowerTextForTest() const
{
    return m_revPowerLabel ? m_revPowerLabel->value() : QString();
}

QString PaValuesPage::swrTextForTest() const
{
    return m_swrLabel ? m_swrLabel->value() : QString();
}

QString PaValuesPage::paCurrentTextForTest() const
{
    return m_paCurrentLabel ? m_paCurrentLabel->value() : QString();
}

QString PaValuesPage::paTempTextForTest() const
{
    return m_paTempLabel ? m_paTempLabel->value() : QString();
}

QString PaValuesPage::supplyVoltsTextForTest() const
{
    return m_supplyVoltsLabel ? m_supplyVoltsLabel->value() : QString();
}

QString PaValuesPage::fwdAdcTextForTest() const
{
    return m_fwdAdcLabel ? m_fwdAdcLabel->value() : QString();
}

QString PaValuesPage::revAdcTextForTest() const
{
    return m_revAdcLabel ? m_revAdcLabel->value() : QString();
}

QString PaValuesPage::adcOverloadTextForTest() const
{
    return m_adcOverloadLabel ? m_adcOverloadLabel->value() : QString();
}

// ── Phase 5B (#167) gap-fill test accessors ─────────────────────────────────
QString PaValuesPage::fwdRawTextForTest() const
{
    return m_fwdRawLabel ? m_fwdRawLabel->value() : QString();
}

QString PaValuesPage::driveTextForTest() const
{
    return m_driveLabel ? m_driveLabel->value() : QString();
}

QString PaValuesPage::fwdVoltageTextForTest() const
{
    return m_fwdVoltageLabel ? m_fwdVoltageLabel->value() : QString();
}

QString PaValuesPage::revVoltageTextForTest() const
{
    return m_revVoltageLabel ? m_revVoltageLabel->value() : QString();
}

QString PaValuesPage::fwdCalibratedPeakForTest() const
{
    if (!m_fwdPeakMin.valid()) {
        return QString();
    }
    return QString::number(m_fwdPeakMin.peak, 'f', 2) + QStringLiteral(" W");
}

QString PaValuesPage::fwdCalibratedMinForTest() const
{
    if (!m_fwdPeakMin.valid()) {
        return QString();
    }
    return QString::number(m_fwdPeakMin.min, 'f', 2) + QStringLiteral(" W");
}

void PaValuesPage::clickResetForTest()
{
    if (m_resetButton) {
        m_resetButton->click();
    } else {
        // Fall back to the slot directly — useful when the button-row
        // didn't construct (model-less preview path).
        resetPaValues();
    }
}
#endif

} // namespace NereusSDR
