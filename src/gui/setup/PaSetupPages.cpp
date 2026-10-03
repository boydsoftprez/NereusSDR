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
//   2026-05-03 — Phase 7 of issue #167 PA-cal hotfix: PaGainByBandPage
//                 gains the auto-cal sweep state machine.  Sub-panel
//                 (status label, progress bar, cancel button, target
//                 watts spinbox) wired to chkAutoPACalibrate.  Mirrors
//                 Thetis CalibratePAGain at console.cs:10228-10387
//                 [v2.10.3.13]: HF-only band loop, engage TUNE,
//                 settle, read alex_fwd, compute diff_dBm, write delta
//                 into active PaProfile via setAdjust.  Halts on user
//                 cancel or safety abort (observed > band_max * 1.1).
//                 Authored by J.J. Boyd (KG4VCF) with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-05-03 — Phase 8 of issue #167 PA-cal hotfix: per-SKU visibility
//                 wiring.  Adds applyCapabilityVisibility(BoardCapabilities)
//                 to all three PA pages, plus four informational warning
//                 labels on PaGainByBandPage (no-PA-support banner,
//                 Ganymede 500W follow-up, PA/TX-Profile recovery
//                 linkage, ATT-on-TX gate).  When caps.hasPaProfile is
//                 false, the live editor controls (Phase 6+7) — combo,
//                 buttons, spinbox grid, adjust matrix, max-power column,
//                 auto-cal checkbox + sub-panel — are all disabled.
//                 Visibility logic mirrors Thetis
//                 comboRadioModel_SelectedIndexChanged
//                 (setup.cs:19812-20310 [v2.10.3.13]).
//                 Authored by J.J. Boyd (KG4VCF) with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-05-03 — v0.3.2 pre-tag cleanup pass (issue #167):
//                 Stream A — m_attOnTxWarning informational label dropped.
//                 The ATT-on-TX safety subsystem is scaffolded for Phase
//                 3M-4 PureSignal but has no observable behaviour in
//                 v0.3.2 (pureSignalActive() returns false unconditionally;
//                 the gate never fires). The user-facing UI surface
//                 lands with PureSignal in 3M-4.
//                 Stream B — E1-E10 source-first remediations from the
//                 parity audit: panelAutoPACalibrate disclaimer (E1),
//                 validateProfileName helper (E3), max-power 0.1
//                 step / 1 decimal precision (E4), repurposing comment
//                 on m_warningLabel (E5), chkPANewCal hidden by default
//                 (E6), block-header on warning row group (E7), Default-
//                 fallback after delete (E8), dynamic group title on
//                 loadProfileIntoUi (E9). Authored by J.J. Boyd (KG4VCF)
//                 with AI-assisted implementation via Anthropic Claude
//                 Code.
//   2026-05-03 — Phase 8.5 of issue #167 PA-cal hotfix: pre-tag styling
//                 alignment pass. Applies Style::applyDarkPageStyle(this)
//                 to all three PA page constructors so they inherit the
//                 canonical dark palette + QGroupBox / QPushButton /
//                 QCheckBox / QSpinBox / QDoubleSpinBox / QComboBox /
//                 QSlider / QLineEdit / QLabel rules already used by
//                 every other Setup page. Replaces inline color literals
//                 with named StyleConstants on the divergence warning
//                 label (kAmberWarn), abort error label (kRedBorder),
//                 auto-cal status label (kTextScale), placeholder labels
//                 (kTextScale via helper), and the 3 informational banner
//                 labels (warning/info banner helpers). Standardises
//                 spinbox column widths in the 14x9 adjust grid + matches
//                 the 4 profile-lifecycle button widths so columns / rows
//                 line up. Centers grid header labels. Per STYLEGUIDE.md.
//                 Behaviour unchanged; pure visual. Authored by J.J. Boyd
//                 (KG4VCF) with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-25 - R-R3-46 / R-R3-49 / R-R3-32 (remote-window parity
//                 Task 6): PA Gain's editor and the Watt Meter's PA table
//                 follow transmitSettingsVersion 6 in a remote window (the
//                 auto-calibrate sweep keeps the transmit permission); PA
//                 Values' PA current, temperature and supply volts come from
//                 RadioModel::paReadings() (the Core's in a remote window),
//                 unavailable when absent. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-27 - R-R3-49 / R-R3-32 (remote-window parity Task 33): PA
//                 Values in a remote window shows the Core's forward,
//                 reflected and SWR readings and its raw forward and
//                 reflected readings (scaled here with the Core's radio
//                 model, as the local page scales its own), with the same
//                 peak and minimum tracking, and ADC overload from the
//                 mirrored step attenuator; each says it is the Core's, and
//                 a Core below txReadingsVersion 1 shows each unavailable
//                 with the reason. The local page's two handlers became
//                 applyPowerReadings and applyRawAdc, unchanged. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - R-R3-49 / R-IOS-18: PA Values' temperature, ADC overload
//                 and Reset Peak/Min, and the Watt Meter's Show PA Values
//                 page and Reset PA Values carry their Setup description
//                 ids; Show PA Values page now shows or hides the PA Values
//                 page, as Thetis's chkPAValues does (found bug: nothing read
//                 it). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-09-29 - R-R3-49 / R-IOS-18: PA Gain's profiles for a remote client
//                 (paProfileActionForStation; the page's ids, plain tooltips,
//                 the adjust tooltip's stray %, and the Default profile found
//                 by its real name after a delete). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-29 - R-R3-49: PA Gain follows Thetis's on-the-air lock (found
//                 bug: a local window could switch, create, copy, delete or
//                 reset a profile, or change another band's values, while
//                 the radio transmitted, and a profile switch reaches the
//                 drive at its next recompute). While the Core is on the
//                 air the profile controls and every band's row but the
//                 transmitting band's are disabled, in a local and a remote
//                 window. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-29 - R-R3-49 / R-IOS-27 (JJ's ruling): the transmitting band's
//                 row opens on the air only while this window's device holds
//                 transmit, with plain-words reasons; an adjust taken on the
//                 air moves the drive to that step, as Thetis's
//                 nudAdjustGain_ValueChanged does. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-29 - PA on-air gate review: the open row follows the Core's
//                 transmit band change, which holds while keyed, not the
//                 slice's band. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
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

#include "PaSetupPages.h"

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/CalibrationController.h"
#include "core/HardwareProfile.h"
#include "core/HpsdrModel.h"
#include "core/PaCalProfile.h"
#include "core/PaGainProfile.h"
#include "core/PaProfile.h"
#include "core/PaTempUnit.h"
#include "core/PaProfileManager.h"
#include "core/PaTelemetryScaling.h"
#include "core/RadioConnection.h"
#include "core/RadioStatus.h"
#include "core/StepAttenuatorFacade.h"
#include "core/session/IStationLink.h"
#include "core/session/TransmitStateFacade.h"
#include "gui/StyleConstants.h"
#include "gui/setup/hardware/PaCalibrationGroup.h"
#include "gui/widgets/MetricLabel.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
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
#include <QProgressBar>
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

// Per STYLEGUIDE.md and StyleConstants.h conventions.
// Shared muted-italic style for placeholder labels — matches the
// "coming in Phase X" tone used by the existing Power & PA fake-PA-group
// placeholder this category eventually replaces. Color matches
// Style::kTextScale (#607080) — the canonical muted-text shade.
QString placeholderStyle()
{
    return QStringLiteral(
        "QLabel { color: %1; font-style: italic; padding: 8px; }")
        .arg(Style::kTextScale);
}

// Build a placeholder label, install the muted style, disable focus.
QLabel* buildPlaceholderLabel(const QString& body)
{
    auto* lbl = new QLabel(body);
    lbl->setStyleSheet(placeholderStyle());
    lbl->setWordWrap(true);
    lbl->setEnabled(false);                  // visual-only; never accepts focus
    lbl->setTextInteractionFlags(Qt::NoTextInteraction);
    return lbl;
}

// Per STYLEGUIDE.md / StyleConstants.h: information banners use a calmer
// blue-cyan; warning banners use amber-orange. These are the two banner
// roles called out in the styling brief — extracted into helpers so the
// per-SKU informational rows on PaGainByBandPage stay visually consistent
// and so the duplicated "model-less path" / "live editor path" build
// blocks don't drift.
constexpr auto kBannerWarningColor = "#d09060";  // amber, fits NereusSDR amber palette
constexpr auto kBannerInfoColor    = "#80b0d0";  // cyan-blue, fits accent palette

QString bannerWarningStyle()
{
    return QStringLiteral(
        "QLabel { color: %1; padding: 8px; font-style: italic; }")
        .arg(kBannerWarningColor);
}

QString bannerInfoStyle()
{
    return QStringLiteral(
        "QLabel { color: %1; padding: 8px; }")
        .arg(kBannerInfoColor);
}

// Per STYLEGUIDE.md: column / per-band spinbox widths are picked here so the
// 14-row × 9-column adjust matrix lines up cleanly. Without these, native
// QDoubleSpinBox sizing varies per-platform and column edges drift.
constexpr int kGainSpinWidth      = 64;   // 0..100 dB, 1 decimal
constexpr int kAdjustSpinWidth    = 56;   // -10..+10 dB, 1 decimal — narrower
constexpr int kMaxPowerSpinWidth  = 72;   // 0..1500 W, 1 decimal — wider for "1500.0"
constexpr int kBandLabelWidth     = 56;   // band names ("160m" .. "XVTR")

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

// ── Phase 8 (#167) helper: build the informational warning rows ──────────────
//
// Extracted out of PaGainByBandPage's constructor so it can run in both the
// model-less (test fixture / headless preview) and live editor paths. The
// labels are always created; applyCapabilityVisibility() toggles them per
// the BoardCapabilities flags.
//
// v0.3.2 cleanup: m_attOnTxWarning was dropped here. The ATT-on-TX safety
// subsystem is scaffolded for Phase 3M-4 PureSignal but has no observable
// behaviour in v0.3.2 (pureSignalActive() returns false unconditionally; the
// gate never fires). The user-facing UI surface lands with PureSignal in 3M-4.
namespace {
void buildPhase8WarningRows(
    QLabel*& noPaSupportBanner,
    QLabel*& ganymedeWarning,
    QLabel*& psWarning,
    QWidget* parent,
    QBoxLayout* contentLayout)
{
    // Per STYLEGUIDE.md and StyleConstants.h conventions.
    // Banner colors: warning (amber) / info (cyan-blue) — see helpers above.
    noPaSupportBanner = new QLabel(QStringLiteral(
        "This radio does not support per-band PA gain calibration.\n"
        "PA gain by band is unavailable for the connected board class."), parent);
    noPaSupportBanner->setStyleSheet(bannerWarningStyle());
    noPaSupportBanner->setWordWrap(true);
    noPaSupportBanner->setVisible(true);
    contentLayout->insertWidget(contentLayout->count() - 1, noPaSupportBanner);

    ganymedeWarning = new QLabel(QStringLiteral(
        "NereusSDR has no settings for the ANAN Ganymede 500 W amplifier. "
        "The PA Gain table below applies to the radio's internal PA."), parent);
    ganymedeWarning->setStyleSheet(bannerInfoStyle());
    ganymedeWarning->setWordWrap(true);
    ganymedeWarning->setVisible(false);
    contentLayout->insertWidget(contentLayout->count() - 1, ganymedeWarning);

    psWarning = new QLabel(QStringLiteral(
        "PA Profile / TX Profile recovery linkage active. PureSignal "
        "feedback can re-tune the PA gain table; see Setup -> Transmit "
        "-> PureSignal for details."), parent);
    psWarning->setStyleSheet(bannerInfoStyle());
    psWarning->setWordWrap(true);
    psWarning->setVisible(false);
    contentLayout->insertWidget(contentLayout->count() - 1, psWarning);
}
} // namespace

PaGainByBandPage::PaGainByBandPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("PA Gain"), model, parent)
{
    // Per STYLEGUIDE.md and StyleConstants.h conventions.
    // Apply the canonical dark page palette (background, group-box title,
    // QPushButton / QComboBox / QSpinBox / QDoubleSpinBox / QCheckBox /
    // QSlider / QLineEdit / QLabel rules). Mirrors the pattern used by every
    // other Setup page (Power, Display, Appearance, GeneralOptions, ...);
    // without it the page renders against the system theme.
    Style::applyDarkPageStyle(this);

    // Resolve PaProfileManager from the model. Without it (model-less preview /
    // headless test fixture) we render an inert placeholder for the editor
    // surface but still build the Phase 8 informational warning labels so
    // applyCapabilityVisibility() has something to toggle.
    if (!model || !model->paProfileManager()) {
        // Needs a RadioModel with a PaProfileManager.
        // Source: Thetis setup.designer.cs:47386-47525 [v2.10.3.13]
        auto* lbl = buildPlaceholderLabel(QStringLiteral(
            "PA Gain -- requires a connected radio."));
        contentLayout()->insertWidget(contentLayout()->count() - 1, lbl);
        // Phase 8 (#167): Track the placeholder so test seams have a proxy
        // for editor-enabled state. Tests construct PaGainByBandPage(nullptr)
        // and invoke applyCapabilityVisibility() without a live editor; the
        // placeholder's enabled flag rides the same editorEnabled boolean.
        m_placeholderLabel = lbl;
        // Build the Phase 8 informational warning rows so the test seams
        // have non-null targets to toggle visibility on. (m_attOnTxWarning
        // dropped in v0.3.2 cleanup; ATT-on-TX UI lands with PureSignal in
        // Phase 3M-4 — see CHANGELOG ATT-on-TX entry.)
        buildPhase8WarningRows(m_noPaSupportBanner, m_ganymedeWarning,
                               m_psWarning,
                               this, contentLayout());
        // D3 test seam: create m_autoCalibrateCheck so
        // applyCapabilityVisibility() / autoCalibrateCheckForTest() have a
        // non-null target in the model-less path.
        m_autoCalibrateCheck = new QCheckBox(
            QStringLiteral("Auto-Calibrate (sweep)"), this);
        m_autoCalibrateCheck->setVisible(true);
        // D4 test seam: create m_bypassPaSettingsCheck so
        // applyCapabilityVisibility() / bypassPaSettingsCheckForTest() have a
        // non-null target in the model-less path.  Defaults hidden (standard board).
        m_bypassPaSettingsCheck = new QCheckBox(
            QStringLiteral("Bypass ANAN PA Settings"), this);
        m_bypassPaSettingsCheck->setProperty("nereusSetupId", "pa.gain.bypassPaSettings");
        m_bypassPaSettingsCheck->setVisible(false);
        return;
    }
    m_paProfileManager = model->paProfileManager();
    m_connectedModel = model->hardwareProfile().model;

    // ── Top toolbar row: combo + lifecycle buttons ───────────────────────
    // From Thetis comboPAProfile + btnNewPAProfile / btnCopyPAProfile /
    // btnDeletePAProfile / btnResetPAProfile [v2.10.3.13].
    //
    // Per STYLEGUIDE.md layout polish: the combo stretches (factor 1) so it
    // fills the page width; the 4 lifecycle buttons get matched fixed widths
    // (90 px) so they line up cleanly regardless of label length.
    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(6);

    m_profileCombo = new QComboBox(this);
    m_profileCombo->setMinimumWidth(220);
    // R-R3-49: plain words (it named the audio-volume scalar).
    m_profileCombo->setToolTip(QStringLiteral(
        "Active PA gain profile. Its per-band PA gain keeps the drive right for "
        "this radio's power amplifier, so a high-gain amplifier is not overdriven."));
    m_profileCombo->setAccessibleName(QStringLiteral("PA profile"));
    // R-IOS-18: the Setup description's ids for the profile row.
    m_profileCombo->setProperty("nereusSetupId", "pa.gain.profile");
    toolbar->addWidget(m_profileCombo, 1);

    constexpr int kLifecycleButtonWidth = 110;  // wide enough for "Reset Defaults"

    m_btnNew = new QPushButton(QStringLiteral("New"), this);
    m_btnNew->setMinimumWidth(kLifecycleButtonWidth);
    m_btnNew->setProperty("nereusSetupId", "pa.gain.new");
    m_btnNew->setToolTip(QStringLiteral(
        "Create a new empty profile seeded from the connected radio's "
        "factory PA gain row."));
    toolbar->addWidget(m_btnNew);

    m_btnCopy = new QPushButton(QStringLiteral("Copy"), this);
    m_btnCopy->setMinimumWidth(kLifecycleButtonWidth);
    m_btnCopy->setProperty("nereusSetupId", "pa.gain.copy");
    m_btnCopy->setToolTip(QStringLiteral(
        "Duplicate the active profile under a new name."));
    toolbar->addWidget(m_btnCopy);

    m_btnDelete = new QPushButton(QStringLiteral("Delete"), this);
    m_btnDelete->setMinimumWidth(kLifecycleButtonWidth);
    m_btnDelete->setProperty("nereusSetupId", "pa.gain.delete");
    m_btnDelete->setToolTip(QStringLiteral(
        "Delete the active profile.  The last remaining profile cannot be "
        "deleted."));
    toolbar->addWidget(m_btnDelete);

    m_btnReset = new QPushButton(QStringLiteral("Reset Defaults"), this);
    m_btnReset->setMinimumWidth(kLifecycleButtonWidth);
    m_btnReset->setProperty("nereusSetupId", "pa.gain.reset");
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

    // NereusSDR-spin: m_warningLabel is repurposed as a per-band gain divergence
    // indicator. Thetis lblPAProfileWarning ships verbatim 5-line red text
    // describing TX-Profile / PA-Profile recovery linkage
    // (setup.designer.cs:47487-47499 [v2.10.3.13]) which is a 3M-4 PureSignal
    // follow-up.
    m_warningLabel = new QLabel(QStringLiteral(
        "PA profile has been modified from factory defaults."), this);
    // Per STYLEGUIDE.md: divergence indicator uses the amber-warning palette
    // shade. Style::kAmberWarn is the canonical "approaching limit" color
    // already used by gauges (gauge yellow zone) and the AGC/SWR warning UI.
    m_warningLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-style: italic; }").arg(Style::kAmberWarn));
    m_warningLabel->setVisible(false);
    warningRow->addWidget(m_warningLabel, 1);

    m_newCalCheck = new QCheckBox(QStringLiteral("New Cal"), this);
    // From Thetis chkPANewCal at setup.designer.cs:47417 [v2.10.3.13] —
    // a "new calibration" mode marker. Per feedback_no_cites_in_user_strings,
    // the user-visible tooltip stays plain English; the upstream cite is
    // kept in this source comment.
    m_newCalCheck->setToolTip(QStringLiteral(
        "New-calibration mode marker. Nothing in this app acts on it; "
        "it is kept for parity only."));
    // From Thetis chkPANewCal Visible=false default at setup.designer.cs:47417
    // [v2.10.3.13]; Thetis Ctrl+Alt+A keyhandler (setup.cs:12490-12498) unhides
    // it. NereusSDR has no live behaviour wired to NewCal mode (deferred to
    // 3M-4 PureSignal calibration overhaul); hide by default to avoid shipping
    // an inert toggle.
    m_newCalCheck->setVisible(false);
    warningRow->addWidget(m_newCalCheck);

    contentLayout()->insertLayout(contentLayout()->count() - 1, warningRow);

    // ── Main grid: PA Gain by Band (dB) ──────────────────────────────────
    // 14 bands x (band label + gain + 9 adjusts + max-power + use-max)
    m_gainByBandGroup = new QGroupBox(QStringLiteral("PA Gain by Band (dB)"), this);
    m_gainByBandGroup->setProperty("nereusSetupId", "pa.gain.table");
    auto* gainGroup = m_gainByBandGroup;
    auto* grid = new QGridLayout(gainGroup);
    grid->setHorizontalSpacing(4);
    grid->setVerticalSpacing(2);

    // Header row.  From Thetis grpGainByBandPA layout (setup.designer.cs:
    // 47420-47525 [v2.10.3.13]) — labels are NereusSDR-original (Thetis
    // pre-renders headers via lblDriveHeader + per-step lblPAAdjustNN labels;
    // we use a plain QGridLayout header row).
    //
    // Per STYLEGUIDE.md layout polish: header labels are center-aligned so
    // they line up with the spinboxes below (which render numeric content
    // right-aligned by default but are visually centered within their fixed
    // column width).
    int row = 0;
    auto* hdrBand = new QLabel(QStringLiteral("Band"), gainGroup);
    auto* hdrGain = new QLabel(QStringLiteral("Gain (dB)"), gainGroup);
    hdrGain->setAlignment(Qt::AlignCenter);
    grid->addWidget(hdrBand, row, kColBand);
    grid->addWidget(hdrGain, row, kColGain);
    for (int step = 0; step < kColAdjustCount; ++step) {
        const int drivePct = (step + 1) * 10;
        auto* lbl = new QLabel(QStringLiteral("%1%").arg(drivePct), gainGroup);
        lbl->setAlignment(Qt::AlignCenter);
        grid->addWidget(lbl, row, kAdjustColumnFirst + step);
    }
    auto* hdrMax = new QLabel(QStringLiteral("Max W"), gainGroup);
    hdrMax->setAlignment(Qt::AlignCenter);
    auto* hdrUse = new QLabel(QStringLiteral("Use Max"), gainGroup);
    hdrUse->setAlignment(Qt::AlignCenter);
    grid->addWidget(hdrMax, row, kColMaxPower);
    grid->addWidget(hdrUse, row, kColUseMaxPower);
    ++row;

    // NereusSDR-spin: 14 per-band columns expand chkUsePowerOnDrvTunPA +
    // nudMaxPowerForBandPA which Thetis renders as a single shared row pivoting
    // on _adjustingBand (setup.cs:24142-24173 + setup.designer.cs:47515-47568
    // [v2.10.3.13]). Step + decimals matched to Thetis precision.
    //
    // Per-band rows: 14 entries.
    // From Thetis nud160M..nudVHF13 setup.designer.cs:47386-47525 [v2.10.3.13].
    for (int n = 0; n < kPaBandCount; ++n) {
        const Band band = static_cast<Band>(n);
        if (!isPaEditableBand(band)) {
            continue;
        }

        // Band label. Fixed width so the column edge is consistent across
        // all 14 rows (per STYLEGUIDE.md layout polish).
        auto* bandLbl = new QLabel(bandLabel(band), gainGroup);
        bandLbl->setFixedWidth(kBandLabelWidth);
        grid->addWidget(bandLbl, row, kColBand);

        // Per-band gain spinbox (38.8..100 dB, 0.1 step, 1 decimal).
        // Issue #199: Thetis hard-clamps the minimum at 38.8 across all 25
        // PA-gain spinboxes (11 HF nud<Band>M + 14 nudVHF<n>) — values below
        // 38.8 saturate computeAudioVolume's drive byte at 1.0 with no
        // further effect on RF output (funsutton field report on ANAN-10E:
        // 30.8 produced same RF as 38.8 on 80m).
        // From Thetis setup.designer.cs:48537-48546 [v2.10.3.13] — nudVHF1
        // (`Maximum = 100`, `Minimum = 38.8`) and 24 sibling sites match.
        m_gainSpins[n] = buildSpin(38.8, 100.0, 0.1, 1, gainGroup);
        m_gainSpins[n]->setFixedWidth(kGainSpinWidth);
        // R-R3-49: plain words (it named the audio-volume scalar and the
        // TX FIFO the scalar drives).
        m_gainSpins[n]->setToolTip(QStringLiteral(
            "PA gain for %1 in dB. The Core subtracts it from the power you "
            "ask for to set the drive.").arg(bandLabel(band)));
        grid->addWidget(m_gainSpins[n], row, kColGain);
        wireGainSpin(m_gainSpins[n], band);

        // Per-band drive-step adjust matrix — NereusSDR-spin densification.
        // From Thetis panelAdjustGain [v2.10.3.13] (Thetis ships only the
        // row for the currently-selected band; NereusSDR exposes all 14 x 9
        // cells so the user does not have to scrub through bands to see
        // their full per-step compensation table).
        for (int step = 0; step < kColAdjustCount; ++step) {
            auto* spin = buildSpin(-10.0, 10.0, 0.1, 1, gainGroup);
            spin->setFixedWidth(kAdjustSpinWidth);
            spin->setToolTip(QStringLiteral(
                "Per-step adjust at %1% drive for %2.")  // R-R3-49: showed "10%%"
                .arg((step + 1) * 10).arg(bandLabel(band)));
            m_adjustSpins[n][step] = spin;
            grid->addWidget(spin, row, kAdjustColumnFirst + step);
            wireAdjustSpin(spin, band, step);
        }

        // Per-band max-power column.
        // From Thetis nudMaxPowerForBandPA + chkUsePowerOnDrvTunPA [v2.10.3.13].
        // 0..1500 W range, 0.1 W single-step (Thetis nudMaxPowerForBandPA.TinyStep=true),
        // 1 decimal (Thetis DecimalPlaces=1, setup.designer.cs:47541 [v2.10.3.13]).
        m_maxPowerSpins[n] = buildSpin(0.0, 1500.0, 0.1, 1, gainGroup);
        m_maxPowerSpins[n]->setFixedWidth(kMaxPowerSpinWidth);
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

    // ── Bottom: chkAutoPACalibrate + auto-cal sub-panel (Phase 7 of #167) ─
    // From Thetis chkAutoPACalibrate setup.designer.cs:49084 [v2.10.3.13]
    // and chkAutoPACalibrate_CheckedChanged setup.cs:15683-15698
    // [v2.10.3.13] (panel toggle).  Sub-panel mirrors panelAutoPACalibrate
    // setup.designer.cs:49094-49119 [v2.10.3.13]: target watts spinbox
    // (udPACalPower), Calibrate button (here: Cancel — Phase 7 starts the
    // sweep on checkbox toggle, not on button click, since Thetis runs
    // the state machine on a worker thread; NereusSDR drives it from the
    // GUI thread via QTimer).
    m_autoCalibrateCheck = new QCheckBox(
        QStringLiteral("Auto-Calibrate (sweep)"), this);
    m_autoCalibrateCheck->setToolTip(QStringLiteral(
        "Enable the per-band auto-calibration sweep.  Sweep iterates HF "
        "bands x drive levels (10/20/.../90), engages TUNE, reads FWD "
        "power, and writes the gain delta into the active profile."));
    contentLayout()->insertWidget(contentLayout()->count() - 1,
                                   m_autoCalibrateCheck);

    // NereusSDR-spin: m_autoCalPanel ships 4 of Thetis's 17 panelAutoPACalibrate
    // sub-controls. Per-band selectors (chkPA160..chkPA6), radPACalAllBands /
    // radPACalSelBands, chkBypassANANPASettings, and explicit btnPAGainCalibration
    // are deferred follow-ups (see pa-calibration-hotfix.md §6 + remediation
    // backlog F1). Auto-cal currently sweeps unconditional 11 HF bands.
    //
    // From Thetis panelAutoPACalibrate setup.designer.cs:49094-49299 [v2.10.3.13].
    //
    // Sub-panel: status / progress / cancel / target watts.  Hidden until
    // the user ticks chkAutoPACalibrate (Thetis panelAutoPACalibrate.Visible
    // = false at construction; setup.designer.cs:49119 [v2.10.3.13]).
    m_autoCalPanel = new QGroupBox(QStringLiteral("Auto-Cal Sweep"), this);
    m_autoCalPanel->setVisible(false);
    auto* autoCalForm = new QFormLayout(m_autoCalPanel);

    // Per STYLEGUIDE.md: status text in muted-italic uses the kTextScale
    // shade (matches placeholder bodies above and gauge tick labels in
    // their normal/disabled state).
    m_autoCalStatusLabel = new QLabel(QStringLiteral("Idle"), m_autoCalPanel);
    m_autoCalStatusLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-style: italic; }").arg(Style::kTextScale));
    autoCalForm->addRow(QStringLiteral("Status:"), m_autoCalStatusLabel);

    // TODO: add Style::kProgressBar once defined — for now the dark-page
    // QProgressBar inherits no rules from applyDarkPageStyle (no rule
    // exists for QProgressBar in StyleConstants.h yet). Native rendering
    // is acceptable as a fallback; revisit when a follow-up adds a
    // styled progress-bar block to StyleConstants.
    m_autoCalProgressBar = new QProgressBar(m_autoCalPanel);
    m_autoCalProgressBar->setRange(0, 100);
    m_autoCalProgressBar->setValue(0);
    autoCalForm->addRow(QStringLiteral("Progress:"), m_autoCalProgressBar);

    // Target watts spinbox (Thetis udPACalPower setup.designer.cs:49155-
    // 49186 [v2.10.3.13] — 1..100 W range, 1 W increment).
    m_autoCalTargetSpin = new QDoubleSpinBox(m_autoCalPanel);
    m_autoCalTargetSpin->setRange(1.0, 1500.0);
    m_autoCalTargetSpin->setSingleStep(1.0);
    m_autoCalTargetSpin->setDecimals(0);
    m_autoCalTargetSpin->setValue(m_autoCalTargetWatts);
    m_autoCalTargetSpin->setToolTip(QStringLiteral(
        "Target watts for the auto-cal sweep.  Each step drives the "
        "radio at this power and adjusts the per-step gain delta to "
        "hit it."));
    connect(m_autoCalTargetSpin,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double w) {
                m_autoCalTargetWatts = w;
            });
    autoCalForm->addRow(QStringLiteral("Target watts:"), m_autoCalTargetSpin);

    m_autoCalCancelButton = new QPushButton(QStringLiteral("Cancel"),
                                            m_autoCalPanel);
    m_autoCalCancelButton->setToolTip(QStringLiteral(
        "Cancel the in-progress auto-cal sweep.  In-flight band's drive-"
        "step adjusts written so far are kept; future bands are skipped."));
    connect(m_autoCalCancelButton, &QPushButton::clicked, this, [this]() {
        if (m_autoCalibrateCheck && m_autoCalibrateCheck->isChecked()) {
            // Toggling unchecks fires onAutoCalibrateToggled(false) which
            // dispatches into cancelAutoCal.
            m_autoCalibrateCheck->setChecked(false);
        }
    });
    autoCalForm->addRow(m_autoCalCancelButton);

    contentLayout()->insertWidget(contentLayout()->count() - 1, m_autoCalPanel);

    connect(m_autoCalibrateCheck, &QCheckBox::toggled,
            this, &PaGainByBandPage::onAutoCalibrateToggled);

    // ── chkBypassANANPASettings (D4: ANAN-G2E port) ──────────────────────
    // From Thetis setup.cs:19921 [v2.10.3.15] //N1GP G2E added:
    //   chkBypassANANPASettings.Visible = true;  (in ANAN_G2E case)
    // Thetis tooltip: "BP PA" (setup.designer.cs:49245 [v2.10.3.15]).
    // Shown only when BoardCapabilities::showsBypassPaSettingsUi is true;
    // defaults hidden (false) here, shown by applyCapabilityVisibility().
    m_bypassPaSettingsCheck = new QCheckBox(
        QStringLiteral("Bypass ANAN PA Settings"), this);
    m_bypassPaSettingsCheck->setProperty("nereusSetupId", "pa.gain.bypassPaSettings");
    m_bypassPaSettingsCheck->setToolTip(QStringLiteral(
        "Bypass the board-specific PA calibration table (BP PA). "
        "When checked, the generic Hermes gain row is used instead "
        "of the ANAN-G2E factory row. Useful when PA gain for this "
        "radio is not calibrated."));
    m_bypassPaSettingsCheck->setVisible(false);  // hidden until applyCapabilityVisibility
    contentLayout()->insertWidget(contentLayout()->count() - 1,
                                   m_bypassPaSettingsCheck);

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
                // #202 deep-fix: rebuild the combo whenever the active
                // profile changes so the Bypass-mode special case can take
                // effect (when active == "Bypass", combo shows only
                // Bypass per Thetis setup.cs:23335-23339 [v2.10.3.13]).
                rebuildProfileCombo();
                if (m_profileCombo && m_profileCombo->currentText() != name) {
                    QSignalBlocker blocker(m_profileCombo);
                    m_profileCombo->setCurrentText(name);
                }
                if (const PaProfile* p = m_paProfileManager->profileByName(name)) {
                    loadProfileIntoUi(*p);
                }
            });

    // Subscribe to RadioStatus::powerChanged so the auto-cal state machine
    // can capture live FWD readings while sweeping.  The slot short-circuits
    // when the state machine is Idle / not collecting, so connecting
    // unconditionally is safe (no spurious side effects on normal RX/TX).
    connect(&model->radioStatus(), &RadioStatus::powerChanged,
            this, &PaGainByBandPage::onFwdPowerSample);

    // ── chkBypassANANPASettings ↔ TransmitModel::paSettingsBypass (D4) ───
    // From Thetis setup.cs:19921 [v2.10.3.15] //N1GP G2E added.
    // Thetis has no CheckedChanged handler; NereusSDR wires the checkbox
    // state to TransmitModel::paSettingsBypass for persistence.
    TransmitModel& tx = model->transmitModel();
    if (m_bypassPaSettingsCheck) {
        m_bypassPaSettingsCheck->setChecked(tx.paSettingsBypass());
        connect(m_bypassPaSettingsCheck, &QCheckBox::toggled,
                &tx, &TransmitModel::setPaSettingsBypass);
        connect(&tx, &TransmitModel::paSettingsBypassChanged,
                m_bypassPaSettingsCheck, [this](bool bypass) {
                    QSignalBlocker b(m_bypassPaSettingsCheck);
                    m_bypassPaSettingsCheck->setChecked(bypass);
                });
    }

    // ── NereusSDR-original: per-SKU informational warning labels ─────────────
    //
    // 3 labels with no Thetis equivalent. Driven by BoardCapabilities
    // (NereusSDR-spin collapse of comboRadioModel_SelectedIndexChanged
    // setup.cs:19812-20310 [v2.10.3.13]). Visibility toggled by
    // applyCapabilityVisibility(...).
    //
    // - m_noPaSupportBanner   shown when !caps.hasPaProfile
    // - m_ganymedeWarning     shown when caps.canDriveGanymede
    // - m_psWarning           shown when caps.hasPureSignal
    //
    // (m_attOnTxWarning was originally part of this block but was dropped in
    //  v0.3.2 cleanup — see CHANGELOG ATT-on-TX entry. The ATT-on-TX UI lands
    //  with PureSignal in Phase 3M-4.)
    //
    // All labels are constructed visible by default for the safest first-paint;
    // applyCapabilityVisibility() resolves the actual state once a radio
    // connects. Plain QLabel (not buildPlaceholderLabel) so the warning copy
    // renders in the normal-state font, distinct from the disabled-italic
    // placeholder bodies elsewhere.

    // Per STYLEGUIDE.md and StyleConstants.h conventions.
    // Banner colors: warning (amber) / info (cyan-blue) — see helpers above.
    m_noPaSupportBanner = new QLabel(QStringLiteral(
        "This radio does not support per-band PA gain calibration.\n"
        "PA gain by band is unavailable for the connected board class."), this);
    m_noPaSupportBanner->setStyleSheet(bannerWarningStyle());
    m_noPaSupportBanner->setWordWrap(true);
    m_noPaSupportBanner->setVisible(true);  // safest default: shown until caps prove PA support
    contentLayout()->insertWidget(contentLayout()->count() - 1, m_noPaSupportBanner);

    m_ganymedeWarning = new QLabel(QStringLiteral(
        "NereusSDR has no settings for the ANAN Ganymede 500 W amplifier. "
        "The PA Gain table below applies to the radio's internal PA."), this);
    m_ganymedeWarning->setStyleSheet(bannerInfoStyle());
    m_ganymedeWarning->setWordWrap(true);
    m_ganymedeWarning->setVisible(false);
    contentLayout()->insertWidget(contentLayout()->count() - 1, m_ganymedeWarning);

    m_psWarning = new QLabel(QStringLiteral(
        "PA Profile / TX Profile recovery linkage active. PureSignal "
        "feedback can re-tune the PA gain table; see Setup -> Transmit "
        "-> PureSignal for details."), this);
    m_psWarning->setStyleSheet(bannerInfoStyle());
    m_psWarning->setWordWrap(true);
    m_psWarning->setVisible(false);
    contentLayout()->insertWidget(contentLayout()->count() - 1, m_psWarning);

    // Initial population.
    rebuildProfileCombo();
    if (const PaProfile* active = m_paProfileManager->activeProfile()) {
        loadProfileIntoUi(*active);
    }

    // R-R3-46 / R-R3-49 (remote-window parity Task 6): a profile's values
    // saved elsewhere (another window, the Core, or the Core handing back
    // its own after a refused change) reach the editor.
    connect(m_paProfileManager, &PaProfileManager::profileDataChanged, this, [this]() {
        if (const PaProfile* active = m_paProfileManager->activeProfile()) {
            loadProfileIntoUi(*active);
        }
    });
    // In a remote window the editor starts closed until SetupDialog pushes
    // the version 6 gate and the transmit permission.
    if (!model->ownsLocalDsp()) {
        setTransmitSettingsPermittedAt(6, false, QString());
        setTransmitPermitted(false, QString());
    }

    // R-R3-49: Thetis's on-the-air lock. isCoreOnAir follows the Core's MOX,
    // TUNE and two-tone in a local and a remote window alike.
    connect(model, &RadioModel::coreOnAirChanged, this,
            [this](bool onAir) { applyOnAirState(onAir); });
    // JJ's ruling (holder only, both ways): the transmitting band opens
    // only while this window's device holds transmit, so the lock follows
    // the holder too.
    connect(model, &RadioModel::transmitHolderChanged, this,
            [this]() { applyPaSettingsGate(); });
    // The open row is the Core's transmit band (paOnAirBandIndex), which
    // holds while keyed: Thetis OnTXBandChanged (setup.cs:23835-23839
    // [v2.10.3.15]) moves _adjustingBand only from the TXBand setter, and
    // that returns while MOX.
    //   lblTXattBand.Text = newBand.ToString(); //[2.3.10.6]MW0LGE added (also in ATTOnTX)
    // A local window follows its own transmit band. A remote window takes
    // the Core's held band from the paTransmitBand property, which also
    // raises transmitBandChanged. The slice followers below serve only a
    // remote window on an older Core that does not send paTransmitBand; its
    // row then follows the window's own transmit slice as before.
    connect(model, &RadioModel::transmitBandChanged, this,
            [this]() { refreshOnAirBand(); });
    const auto followSlice = [this](SliceModel* slice) {
        if (!slice) {
            return;
        }
        connect(slice, &SliceModel::bandChanged, this, [this](Band) { refreshOnAirBand(); });
        connect(slice, &SliceModel::txSliceChanged, this,
                [this](bool) { refreshOnAirBand(); });
    };
    for (SliceModel* slice : model->slices()) {
        followSlice(slice);
    }
    connect(model, &RadioModel::sliceAdded, this, [this, followSlice](int index) {
        if (RadioModel* const radio = this->model()) {
            followSlice(radio->slices().value(index, nullptr));
        }
        refreshOnAirBand();
    });
    if (model->isCoreOnAir()) {
        applyOnAirState(true);
    }
}

QList<QWidget*> PaGainByBandPage::paSettingsControls() const
{
    QList<QWidget*> controls{m_profileCombo, m_btnNew, m_btnCopy, m_btnDelete, m_btnReset,
                             m_newCalCheck, m_bypassPaSettingsCheck};
    for (int n = 0; n < kPaBandCount; ++n) {
        controls << m_gainSpins[n] << m_maxPowerSpins[n] << m_useMaxPowerChecks[n];
        for (int step = 0; step < kAutoCalDriveSteps; ++step) {
            controls << m_adjustSpins[n][step];
        }
    }
    return controls;
}

QList<QWidget*> PaGainByBandPage::paBandControls(int bandIndex) const
{
    QList<QWidget*> controls{m_gainSpins[bandIndex], m_maxPowerSpins[bandIndex],
                             m_useMaxPowerChecks[bandIndex]};
    for (int step = 0; step < kAutoCalDriveSteps; ++step) {
        controls << m_adjustSpins[bandIndex][step];
    }
    return controls;
}

// R-R3-49: what Thetis locks while the radio is on the air.
// From Thetis setup.cs:23826-23834 [v2.10.3.15] OnMoxChangeHandler:
//   PAProfileEnableControls(newMox);
//   if (newMox) enabledAllPAnuds(false); else enabledAllPAnuds(true);
//   //[2.3.10.6]MW0LGE added (also in ATTOnTX)  [original inline comment from
//   setup.cs:23838, OnTXBandChanged's TX attenuator label line, not ported]
// From Thetis setup.cs:23479-23496 [v2.10.3.15] PAProfileEnableControls:
//   //prevent profile switch during a tx
//   //user can only tweak the NUD's
//   comboPAProfile.Enabled = !tx; btnNewPAProfile.Enabled = !tx;
//   if (tx) { btnDeletePAProfile, btnResetPAProfile, btnCopyPAProfile
//             .Enabled = false; }
// From Thetis setup.cs:24169-24192 [v2.10.3.15] enabledAllPAnuds(false):
//   // ignore current band
//   if (b != _adjustingBand) c.Enabled = false;
// Thetis shows the adjust matrix, max power and use-max for _adjustingBand
// alone (panelAdjustGain, enabledPAAdjust), so under MOX only that band's
// values can change; NereusSDR shows every band's row and locks the others.
// New Cal and the G2E's bypass box are not in Thetis's lock.
QList<QWidget*> PaGainByBandPage::onAirLockedControls() const
{
    if (!m_onAir) {
        return {};
    }
    QList<QWidget*> controls{m_profileCombo, m_btnNew, m_btnCopy, m_btnDelete, m_btnReset};
    for (int n = 0; n < kPaBandCount; ++n) {
        if (n != m_onAirBandIndex) {
            controls << paBandControls(n);
        }
    }
    return controls;
}

void PaGainByBandPage::applyOnAirState(bool onAir)
{
    if (onAir == m_onAir) {
        return;
    }
    m_onAir = onAir;
    m_onAirBandIndex = currentOnAirBandIndex();
    applyPaSettingsGate();
}

int PaGainByBandPage::currentOnAirBandIndex()
{
    // Thetis _adjustingBand, read where the Core's own refusals read it.
    // From Thetis setup.cs:23836-23852 [v2.10.3.15] OnTXBandChanged / setAdjustingBand:
    //   lblTXattBand.Text = newBand.ToString(); //[2.3.10.6]MW0LGE added (also in ATTOnTX)
    //   _adjustingBand = Band.FIRST; // MW0LGE_[2.9.0.7] reset
    RadioModel* const radio = model();
    return (m_onAir && radio) ? radio->paOnAirBandIndex() : -1;
}

void PaGainByBandPage::refreshOnAirBand()
{
    const int band = currentOnAirBandIndex();
    if (band == m_onAirBandIndex) {
        return;
    }
    m_onAirBandIndex = band;
    applyPaSettingsGate();
}

void PaGainByBandPage::applyPaSettingsGate()
{
    const QList<QWidget*> all = paSettingsControls();
    if (!m_paSettingsPermitted) {
        gateTransmitControls(all, false, m_paSettingsReason);
        return;
    }
    const QList<QWidget*> locked = onAirLockedControls();
    // JJ's ruling: on the air only the device that holds transmit changes
    // the transmitting band's values; this window too, when another device
    // holds it (RadioModel::paOnAirEditRefusal gives the Core's refusal).
    QList<QWidget*> holderOnly;
    if (m_onAir && m_onAirBandIndex >= 0 && !holdsTransmitHere()) {
        holderOnly = paBandControls(m_onAirBandIndex);
    }
    QList<QWidget*> open;
    for (QWidget* control : all) {
        if (!locked.contains(control) && !holderOnly.contains(control)) {
            open << control;
        }
    }
    gateTransmitControls(open, true, QString());
    gateTransmitControls(locked, false, RadioModel::paOnAirLockedReason());
    gateTransmitControls(holderOnly, false, RadioModel::paHolderOnlyReason());
}

bool PaGainByBandPage::holdsTransmitHere()
{
    const RadioModel* const radio = model();
    if (!radio) {
        return false;
    }
    // The Core's own window holds it unless another device does; a remote
    // window only while the Core names this device the holder.
    if (radio->ownsLocalDsp()) {
        return radio->otherDeviceHoldsRefusal().isEmpty();
    }
    const IStationLink* const link = radio->stationLink();
    return link && link->holdsTransmitHere();
}

QList<QWidget*> PaGainByBandPage::paKeyingControls() const
{
    // The sweep engages TUNE on every band (Thetis chkAutoPACalibrate), so
    // it waits for remote transmit.
    return {m_autoCalibrateCheck, m_autoCalPanel};
}

void PaGainByBandPage::applyPaGates()
{
    applyPaSettingsGate();
    gateTransmitControls(paKeyingControls(), m_paKeyingPermitted, m_paKeyingReason);
}

void PaGainByBandPage::setTransmitPermitted(bool permitted, const QString& reason)
{
    m_paKeyingPermitted = permitted;
    m_paKeyingReason = reason.isEmpty()
        ? tr("Remote transmit controls are not available from this Core.")
        : reason;
    gateTransmitControls(paKeyingControls(), m_paKeyingPermitted, m_paKeyingReason);
}

void PaGainByBandPage::setTransmitSettingsPermittedAt(int version, bool permitted,
                                                      const QString& reason)
{
    if (version != 6) {
        return;
    }
    m_paSettingsPermitted = permitted;
    m_paSettingsReason = reason.isEmpty() ? IStationLink::transmitSettingsUnavailableReason()
                                          : reason;
    applyPaSettingsGate();
}

// ── Phase 8 of #167: per-SKU visibility wiring ────────────────────────────────
//
// applyCapabilityVisibility — collapses the per-SKU visibility decisions for
// the live editor surface (Phase 6+7) and the four informational warning
// rows. When caps.hasPaProfile=false, the entire editor is disabled (combo,
// buttons, spinbox grid, adjust matrix, max-power column, auto-cal toggle +
// sub-panel) and the no-PA-support banner is shown. Other warnings layer
// independently on top.
//
// From Thetis comboRadioModel_SelectedIndexChanged (setup.cs:19812-20310
// [v2.10.3.13+501e3f51]) — per-SKU PA tab visibility. Thetis swaps dozens
// of controls (chkApolloPresent / chkAlexPresent / labelATTOnTX /
// chkAutoPACalibrate / etc.) per HPSDRModel; NereusSDR collapses those
// decisions into BoardCapabilities flags and surfaces the equivalent
// per-page visibility decisions here.
void PaGainByBandPage::applyCapabilityVisibility(const BoardCapabilities& caps)
{
    // ── Editor surface enable gate ──────────────────────────────────────
    // Phase 6+7 ships the live editor (combo + 4 lifecycle buttons + 14
    // gain spinboxes + 14×9 adjust matrix + 14 max-power spinboxes + 14
    // use-max checkboxes + auto-cal checkbox + auto-cal sub-panel). When
    // !caps.hasPaProfile, every editor control is disabled to prevent
    // editing a profile that has no underlying PA hardware. In the
    // model-less path (test fixture / headless preview) the live editor
    // isn't built; m_placeholderLabel rides the same editorEnabled flag
    // so test seams can verify the toggle behavior.
    const bool editorEnabled = caps.hasPaProfile;

    // R-R3-46 (parity Task 6): the capability decides each control's own
    // enabled state; the transmit gates are laid over it again below.
    gateTransmitControls(paSettingsControls(), true, QString());
    gateTransmitControls(paKeyingControls(), true, QString());

    if (m_profileCombo)        { m_profileCombo->setEnabled(editorEnabled); }
    if (m_btnNew)              { m_btnNew->setEnabled(editorEnabled); }
    if (m_btnCopy)             { m_btnCopy->setEnabled(editorEnabled); }
    if (m_btnDelete)           { m_btnDelete->setEnabled(editorEnabled); }
    if (m_btnReset)            { m_btnReset->setEnabled(editorEnabled); }
    if (m_newCalCheck)         { m_newCalCheck->setEnabled(editorEnabled); }
    if (m_autoCalibrateCheck)  { m_autoCalibrateCheck->setEnabled(editorEnabled); }
    if (m_autoCalPanel)        { m_autoCalPanel->setEnabled(editorEnabled); }
    // Model-less proxy: placeholder rides the same gate as the live editor.
    if (m_placeholderLabel)    { m_placeholderLabel->setEnabled(editorEnabled); }

    for (int n = 0; n < kPaBandCount; ++n) {
        if (m_gainSpins[n])        { m_gainSpins[n]->setEnabled(editorEnabled); }
        if (m_maxPowerSpins[n])    { m_maxPowerSpins[n]->setEnabled(editorEnabled); }
        if (m_useMaxPowerChecks[n]) { m_useMaxPowerChecks[n]->setEnabled(editorEnabled); }
        for (int step = 0; step < kAutoCalDriveSteps; ++step) {
            if (m_adjustSpins[n][step]) {
                m_adjustSpins[n][step]->setEnabled(editorEnabled);
            }
        }
    }

    // ── No-PA-support banner ────────────────────────────────────────────
    // Shown when caps.hasPaProfile=false. Atlas / RX-only kits / RedPitaya
    // hit this branch.
    if (m_noPaSupportBanner) {
        m_noPaSupportBanner->setVisible(!caps.hasPaProfile);
    }

    // ── Ganymede 500W PA follow-up ──────────────────────────────────────
    // Andromeda console only. From Thetis Andromeda/Andromeda.cs:914-920
    // [v2.10.3.13] — the Ganymede-specific PA tab is a separate UI surface
    // queued as a follow-up to this hotfix.
    if (m_ganymedeWarning) {
        m_ganymedeWarning->setVisible(caps.canDriveGanymede);
    }

    // ── PA / TX-Profile recovery linkage warning ────────────────────────
    // PS-capable radios only. The warning describes future 3M-4 PS feedback
    // behaviour; the gate itself is dormant until the PS feedback DDC lands.
    if (m_psWarning) {
        m_psWarning->setVisible(caps.hasPureSignal);
    }

    // ── Auto-PA-Calibrate checkbox visibility (D3) ───────────────────────
    // From Thetis setup.cs:19920 [v2.10.3.15] //N1GP G2E added:
    //   chkAutoPACalibrate.Visible = false;  (in ANAN_G2E case)
    // G2E sets allowsAutoPaCalibrate=false; all other boards default true.
    // The auto-cal sub-panel inherits visibility from the checkbox.
    if (m_autoCalibrateCheck) {
        m_autoCalibrateCheck->setVisible(caps.allowsAutoPaCalibrate);
    }
    if (m_autoCalPanel) {
        // Sub-panel hides when auto-calibrate is not available. If the
        // checkbox is hidden, the panel must also be hidden regardless of
        // its internal state machine (which manages show/hide during a live
        // calibration sweep on supporting boards).
        if (!caps.allowsAutoPaCalibrate) {
            m_autoCalPanel->setVisible(false);
        }
    }

    // ── Bypass ANAN PA Settings checkbox visibility (D4) ────────────────
    // From Thetis setup.cs:19921 [v2.10.3.15] //N1GP G2E added:
    //   chkBypassANANPASettings.Visible = true;  (in ANAN_G2E case)
    // G2E sets showsBypassPaSettingsUi=true; all other boards default false.
    if (m_bypassPaSettingsCheck) {
        m_bypassPaSettingsCheck->setVisible(caps.showsBypassPaSettingsUi);
    }

    // ── ATT-on-TX informational row ─────────────────────────────────────
    // Dropped in v0.3.2 cleanup. The ATT-on-TX safety subsystem is scaffolded
    // for Phase 3M-4 PureSignal but has no observable behaviour in v0.3.2;
    // the user-facing UI surface lands with PureSignal in 3M-4.
    // (Field caps.hasStepAttenuatorCal is still consulted by other components.)

    applyPaGates();
}

#ifdef NEREUS_BUILD_TESTS
// Phase 8 (#167) test seams.
//
// isPaEditorEnabledForTest: returns whether the editor surface is enabled.
// Live editor mode: m_profileCombo is the canonical proxy (toggled in
// lockstep with every other editor control via the editorEnabled boolean).
// Model-less mode (test fixture / headless preview): m_placeholderLabel
// rides the same gate. Tests that construct PaGainByBandPage(nullptr) hit
// the model-less path so this fallback ensures isPaEditorEnabledForTest()
// reports the correct enable state regardless of construction mode.
bool PaGainByBandPage::isPaEditorEnabledForTest() const
{
    if (m_profileCombo) {
        return m_profileCombo->isEnabled();
    }
    if (m_placeholderLabel) {
        return m_placeholderLabel->isEnabled();
    }
    return false;
}

bool PaGainByBandPage::isNoPaSupportBannerVisibleForTest() const
{
    // isVisibleTo(this) checks the visibility relative to the ancestor
    // chain rooted at `this` so we don't false-negative when the page
    // is itself hidden (which happens during construction before Setup
    // is shown).
    return m_noPaSupportBanner && m_noPaSupportBanner->isVisibleTo(this);
}

bool PaGainByBandPage::isGanymedeWarningVisibleForTest() const
{
    return m_ganymedeWarning && m_ganymedeWarning->isVisibleTo(this);
}

bool PaGainByBandPage::isPsWarningVisibleForTest() const
{
    return m_psWarning && m_psWarning->isVisibleTo(this);
}
#endif

// ── Combo + profile-load plumbing ────────────────────────────────────────────

void PaGainByBandPage::rebuildProfileCombo()
{
    if (!m_profileCombo || !m_paProfileManager) { return; }

    QSignalBlocker blocker(m_profileCombo);

    const QString prevActive = m_paProfileManager->activeProfileName();
    m_profileCombo->clear();
    // #202 deep-fix: filter to `Default - <connectedModel>` + every
    // user-customised profile (hide all other `Default - *`).  Mirrors
    // Thetis setup.cs:23341-23344 [v2.10.3.13].  Bypass-mode special case
    // (when prevActive == "Bypass", show only "Bypass") is handled inside
    // userVisibleProfileNames per setup.cs:23335-23339 [v2.10.3.13].
    const QStringList names =
        m_paProfileManager->userVisibleProfileNames(m_connectedModel,
                                                     prevActive);
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

    // Mirrors Thetis updatePAControls at setup.cs:22884-22905 [v2.10.3.13]:
    // group title rewrites to "<profile> - PA Gain By Band (dB)" so the user
    // sees which profile's values they're editing.
    if (m_gainByBandGroup) {
        m_gainByBandGroup->setTitle(
            tr("%1 — PA Gain By Band (dB)").arg(profile.name()));
    }

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

bool PaGainByBandPage::validateProfileName(const QString& name) const
{
    // Mirrors Thetis validatePAProfileName at setup.cs:22944-22966
    // [v2.10.3.13]: reject reserved "Default" prefix and collisions with
    // existing profile names.
    if (name.startsWith(QStringLiteral("Default"), Qt::CaseInsensitive)) {
        QMessageBox::warning(const_cast<PaGainByBandPage*>(this),
            tr("Invalid profile name"),
            tr("Profile names starting with \"Default\" are reserved for "
               "factory entries. Choose a different name."));
        return false;
    }
    if (m_paProfileManager
        && m_paProfileManager->profileNames().contains(name, Qt::CaseInsensitive)) {
        QMessageBox::warning(const_cast<PaGainByBandPage*>(this),
            tr("Duplicate profile name"),
            tr("A profile named \"%1\" already exists. Choose a different name.")
                .arg(name));
        return false;
    }
    return true;
}

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
    if (!validateProfileName(name)) {
        return;  // user shown a message box; abort the op
    }

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
    if (!validateProfileName(name)) {
        return;  // user shown a message box; abort the op
    }

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
        // Test seam: skip the modal so headless tests don't block on
        // a dialog that requires a user click.
#ifdef NEREUS_BUILD_TESTS
        if (m_suppressLastProfileWarningForTest) { return; }
#endif
        QMessageBox::information(
            this, tr("Cannot Delete Profile"),
            tr("It is not possible to delete the last remaining PA profile."));
        return;
    }

    // Mirrors Thetis btnDeletePAProfile_Click at setup.cs:23015-23024 [v2.10.3.13]:
    // after delete, select Default - <connectedModel> if available, else first
    // remaining profile.
    // R-R3-49 (found bug): the factory profile is named after the model's
    // enum name ("Default - ANAN_G2"), not its display name, so this never
    // found it and fell back to the first profile.
    const QString defaultName = PaProfileManager::defaultProfileName(m_connectedModel);
    QString nextActive;
    if (m_paProfileManager->profileNames().contains(defaultName)) {
        nextActive = defaultName;
    } else if (!m_paProfileManager->profileNames().isEmpty()) {
        nextActive = m_paProfileManager->profileNames().first();
    }
    if (!nextActive.isEmpty()) {
        m_paProfileManager->setActiveProfile(nextActive);
        if (m_profileCombo) {
            m_profileCombo->setCurrentText(nextActive);
        }
        if (const PaProfile* p = m_paProfileManager->profileByName(nextActive)) {
            loadProfileIntoUi(*p);
        }
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
    applyEditOnAir(band, /*adjust=*/false, -1);
}

void PaGainByBandPage::onAdjustChanged(Band band, int step, double value)
{
    if (m_updatingFromProfile || !m_paProfileManager) { return; }

    const PaProfile* active = m_paProfileManager->activeProfile();
    if (!active) { return; }

    PaProfile mutated = *active;
    mutated.setAdjust(band, step, static_cast<float>(value));
    m_paProfileManager->saveProfile(active->name(), mutated);
    // From Thetis setup.cs:24210-24222 [v2.10.3.15] nudAdjustGain_ValueChanged:
    //   if (console.MOX) ... console.PWR = nNumber + 10; // set drive to the value we are adjusting
    applyEditOnAir(band, /*adjust=*/true, step);
}

void PaGainByBandPage::applyEditOnAir(Band band, bool adjust, int step)
{
    // The Core's own window drives the radio: an edit taken on the air
    // reaches the drive as Thetis's does. A remote window's edit reaches
    // it at the Core (RadioModel::applyPaSettingOnAir). Only an edit to
    // the band transmitting now moves the drive (Thetis _adjustingBand);
    // the band is read at the edit, never latched at key.
    RadioModel* const radio = model();
    if (radio && radio->ownsLocalDsp() && radio->paOnAirNow()
        && static_cast<int>(band) == radio->paOnAirBandIndex()) {
        radio->applyPaEditOnAir(adjust ? RadioModel::PaProfileAction::SetAdjust
                                       : RadioModel::PaProfileAction::SetGain,
                                step);
    }
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

// ── Phase 7 of #167: auto-cal sweep state machine ────────────────────────────
//
// Mirrors Thetis CalibratePAGain at console.cs:10228-10387 [v2.10.3.13]
// + chkAutoPACalibrate_CheckedChanged at setup.cs:15683-15698 [v2.10.3.13]
// (panel toggle) + btnPAGainCalibration_Click at setup.cs:9743-9809
// [v2.10.3.13] (sweep start).  Thetis runs the loop on a dedicated worker
// thread (Thread.Sleep blocks the main UI); NereusSDR drives the same
// logic from the GUI thread via QTimer dispatch + RadioStatus signal so
// the audio callback / WDSP threads are never blocked.
//
// Important: the underlying TUNE / MOX wire-up runs through TransmitModel.
// The state machine here does NOT directly manipulate the radio — it
// simply ticks the cursor across (band, drive-step) pairs as readings
// land via onFwdPowerSample.  Phase 7 leaves the actual TUNE engagement
// "passive" (the user is expected to be tuned into a dummy load via the
// existing TUNE button before checking chkAutoPACalibrate); a follow-up
// will add full Thetis-style automatic MOX engagement once the HL2
// ATT/filter audit closes (3M-2 prerequisite).

// ---------------------------------------------------------------------------
// Auto-cal: HF-only band list. Mirrors Thetis console.cs:10269 [v2.10.3.13]:
//   Band[] bands = { B160M, B80M, B60M, B40M, B30M, B20M, B17M, B15M, B12M,
//                    B10M, B6M };
// VHF / GEN / WWV / XVTR are NOT included — Thetis does not iterate them.
namespace {

constexpr std::array<Band, 11> kAutoCalHfBands = {
    Band::Band160m, Band::Band80m, Band::Band60m, Band::Band40m,
    Band::Band30m,  Band::Band20m, Band::Band17m, Band::Band15m,
    Band::Band12m,  Band::Band10m, Band::Band6m,
};

#ifdef NEREUS_BUILD_TESTS
// True if `b` is an HF band the auto-cal sweep iterates over. Only the
// test-only sweep driver below reads it.
bool isAutoCalBand(Band b) noexcept
{
    for (Band hf : kAutoCalHfBands) {
        if (hf == b) { return true; }
    }
    return false;
}
#endif

// Default per-band max-watts ceiling per HPSDRModel for the safety check.
// From Thetis console.cs:10270 [v2.10.3.13]:
//   int[] max_pwr = { 100, 100, 100, 100, 100, 100, 100, 100, 75, 75, 75 };
// NereusSDR uses 200 W on HF for ANAN8000D-class radios (matches the
// PaProfile factory max-power column from clsHardwareSpecific.cs), 100 W
// otherwise.  6m falls to 75 W (per Thetis, plus 8000DLE bench tests).
double defaultMaxWattsForModelBand(HPSDRModel model, Band band) noexcept
{
    const bool is6m = (band == Band::Band6m);
    switch (model) {
        case HPSDRModel::ANAN8000D:    return is6m ? 75.0 : 200.0;
        case HPSDRModel::ANAN_G2:       return is6m ? 75.0 : 100.0;
        case HPSDRModel::ANAN_G2_1K:    return is6m ? 75.0 : 100.0;
        default:                        return is6m ? 75.0 : 100.0;
    }
}

// Watts -> dBm (Thetis console.cs:46645 WattsTodBm() helper inline form).
// dBm = 10 * log10(watts * 1000)
double wattsToDbm(double watts) noexcept
{
    if (watts <= 0.0) { return -1000.0; }  // sentinel: "very small"
    return 10.0 * std::log10(watts * 1000.0);
}

// Settle delay between drive-step changes (ms).  Thetis on_time = 2500 ms;
// NereusSDR uses 200 ms because (a) the user has already TUNE'd into a
// dummy load, so the radio's settled before the sweep starts, and (b)
// the QTimer + GUI thread responsiveness makes a faster cadence feasible.
// Tests can drive readings synchronously via simulateBandFwdReadingForTest.
constexpr int kAutoCalSettleMs = 200;

}  // namespace

void PaGainByBandPage::onAutoCalibrateToggled(bool checked)
{
    // Mirrors Thetis chkAutoPACalibrate_CheckedChanged at setup.cs:15683-
    // 15698 [v2.10.3.13]: panel visibility tied to the checkbox, plus
    // captures the active-profile name so cancel can restore it.
    if (m_autoCalProgrammaticToggle) {
        // Programmatic uncheck (e.g. abortAutoCal / completeAutoCal flipping
        // the checkbox OFF) — already handled the cleanup, just hide the
        // panel and bail.
        if (m_autoCalPanel) { m_autoCalPanel->setVisible(false); }
        return;
    }

    if (checked) {
        if (m_autoCalPanel) { m_autoCalPanel->setVisible(true); }
        startAutoCal();
    } else {
        cancelAutoCal();
        if (m_autoCalPanel) { m_autoCalPanel->setVisible(false); }
    }
}

void PaGainByBandPage::onAutoCalSettle()
{
    // Settle timer fired without a real telemetry reading arriving.  In
    // production this would mean the radio isn't transmitting — bail
    // gracefully (no advance).  Tests use simulateBandFwdReadingForTest
    // to inject deterministic readings instead of waiting on the timer.
    if (m_autoCalState == AutoCalState::SettlingStep) {
        m_autoCalState = AutoCalState::ReadingPower;
        refreshAutoCalUi();
    }
}

void PaGainByBandPage::onFwdPowerSample(double fwdW, double /*revW*/,
                                        double /*swr*/)
{
    // Only consume samples when the state machine is actively waiting for
    // one.  All other RX/TX traffic is ignored — the state machine sees
    // exactly the readings tagged for it.
    if (m_autoCalState != AutoCalState::SettlingStep
        && m_autoCalState != AutoCalState::ReadingPower) {
        return;
    }

    // Safety check first: if observed exceeds band_max * 1.1, abort.
    if (!autoCalSafetyCheck(fwdW, m_autoCalCurrentBand)) {
        abortAutoCal(QStringLiteral(
            "Observed %1 W on %2 exceeded safety ceiling — aborted.")
            .arg(fwdW, 0, 'f', 1)
            .arg(bandLabel(m_autoCalCurrentBand)));
        return;
    }

    // Capture: write the gain delta + advance.
    writeAutoCalGainAdjust(m_autoCalCurrentBand,
                           m_autoCalCurrentDriveStep, fwdW);
    advanceAutoCalStep();
}

// ---------------------------------------------------------------------------
// State-machine helpers
// ---------------------------------------------------------------------------

void PaGainByBandPage::startAutoCal()
{
    if (!m_paProfileManager) {
        return;  // no profile manager → no sweep
    }

    // Initialise sweep cursor.
    m_autoCalCurrentBand = Band::Band160m;
    m_autoCalCurrentDriveStep = 0;
    m_autoCalState = AutoCalState::Initializing;

    // Lazy-instantiate settle timer.
    if (!m_autoCalSettleTimer) {
        m_autoCalSettleTimer = new QTimer(this);
        m_autoCalSettleTimer->setSingleShot(true);
        m_autoCalSettleTimer->setInterval(kAutoCalSettleMs);
        connect(m_autoCalSettleTimer, &QTimer::timeout,
                this, &PaGainByBandPage::onAutoCalSettle);
    }

    // Read current target watts from the spinbox (user may have edited).
    if (m_autoCalTargetSpin) {
        m_autoCalTargetWatts = m_autoCalTargetSpin->value();
    }

    // Transition into SettlingStep.  In production the settle timer fires
    // after kAutoCalSettleMs; tests use simulateBandFwdReadingForTest which
    // bypasses the timer and drives readings synchronously.
    m_autoCalState = AutoCalState::SettlingStep;
    if (m_autoCalSettleTimer) {
        m_autoCalSettleTimer->start();
    }
    refreshAutoCalUi();
}

void PaGainByBandPage::cancelAutoCal()
{
    // Mirrors Thetis CalibratePAGain end-label cleanup at console.cs:10352-
    // 10377 [v2.10.3.13]: stop MOX, restore exciter signal, close progress.
    // NereusSDR's variant is simpler since the GUI thread holds the state.
    if (m_autoCalSettleTimer && m_autoCalSettleTimer->isActive()) {
        m_autoCalSettleTimer->stop();
    }
    m_autoCalState = AutoCalState::Halting;
    refreshAutoCalUi();

    // Clear the cursor; no partial profile writes have happened on the
    // current (band, step) pair since writes only go in via
    // writeAutoCalGainAdjust *after* a reading lands.
    m_autoCalCurrentBand = Band::Band160m;
    m_autoCalCurrentDriveStep = 0;
    m_autoCalState = AutoCalState::Idle;
    refreshAutoCalUi();
}

void PaGainByBandPage::advanceAutoCalStep()
{
    ++m_autoCalCurrentDriveStep;
    if (m_autoCalCurrentDriveStep >= kAutoCalDriveSteps) {
        // All 9 steps for this band done — move to next band.
        advanceAutoCalBand();
        return;
    }
    // Re-arm settle timer for next step.
    m_autoCalState = AutoCalState::SettlingStep;
    if (m_autoCalSettleTimer) {
        m_autoCalSettleTimer->start();
    }
    refreshAutoCalUi();
}

void PaGainByBandPage::advanceAutoCalBand()
{
    // Find the index of the current band in the HF list.
    int currentIdx = -1;
    for (int i = 0; i < static_cast<int>(kAutoCalHfBands.size()); ++i) {
        if (kAutoCalHfBands[i] == m_autoCalCurrentBand) {
            currentIdx = i;
            break;
        }
    }
    if (currentIdx < 0
        || currentIdx + 1 >= static_cast<int>(kAutoCalHfBands.size())) {
        // No more bands — sweep complete.
        completeAutoCal();
        return;
    }
    m_autoCalCurrentBand = kAutoCalHfBands[currentIdx + 1];
    m_autoCalCurrentDriveStep = 0;
    m_autoCalState = AutoCalState::SettlingStep;
    if (m_autoCalSettleTimer) {
        m_autoCalSettleTimer->start();
    }
    refreshAutoCalUi();
}

void PaGainByBandPage::completeAutoCal()
{
    // Persist the final profile state through PaProfileManager.  The
    // per-step writeAutoCalGainAdjust calls have already been saving each
    // delta; this is the final-clear save for any pending state.
    if (m_paProfileManager) {
        const PaProfile* active = m_paProfileManager->activeProfile();
        if (active) {
            m_paProfileManager->saveProfile(active->name(), *active);
        }
    }

    if (m_autoCalSettleTimer && m_autoCalSettleTimer->isActive()) {
        m_autoCalSettleTimer->stop();
    }

    m_autoCalState = AutoCalState::Completed;
    refreshAutoCalUi();

    // Auto-uncheck the chkAutoPACalibrate so the user can re-launch a fresh
    // sweep.  Programmatic flag prevents the toggled slot from re-firing
    // cancelAutoCal.
    if (m_autoCalibrateCheck && m_autoCalibrateCheck->isChecked()) {
        m_autoCalProgrammaticToggle = true;
        m_autoCalibrateCheck->setChecked(false);
        m_autoCalProgrammaticToggle = false;
    }
}

void PaGainByBandPage::abortAutoCal(const QString& reason)
{
    if (m_autoCalSettleTimer && m_autoCalSettleTimer->isActive()) {
        m_autoCalSettleTimer->stop();
    }
    m_autoCalState = AutoCalState::Aborted;
    if (m_autoCalStatusLabel) {
        m_autoCalStatusLabel->setText(reason);
        // Per STYLEGUIDE.md: error text uses the danger/red palette shade.
        // Style::kRedBorder (#ff4444) is the canonical "danger / over-limit"
        // color shared with gauge red zones and TX-overload badges.
        m_autoCalStatusLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-weight: bold; }").arg(Style::kRedBorder));
    }
    // Auto-uncheck so user can re-attempt.
    if (m_autoCalibrateCheck && m_autoCalibrateCheck->isChecked()) {
        m_autoCalProgrammaticToggle = true;
        m_autoCalibrateCheck->setChecked(false);
        m_autoCalProgrammaticToggle = false;
    }
}

bool PaGainByBandPage::autoCalSafetyCheck(double observedWatts, Band band) const
{
    // Mirrors the safety-margin Thetis applies via the max_pwr[] check
    // implicitly in console.cs:10289 ptbPWR.Value = Math.Min(...).
    // NereusSDR makes the safety check explicit: observed > band_max * 1.1
    // aborts the sweep.
    const double ceiling = autoCalBandMaxWatts(band) * 1.1;
    return observedWatts <= ceiling;
}

void PaGainByBandPage::writeAutoCalGainAdjust(Band band, int driveStep,
                                              double observedWatts)
{
    if (!m_paProfileManager) { return; }
    const PaProfile* active = m_paProfileManager->activeProfile();
    if (!active) { return; }

    // Mirrors Thetis console.cs:10319-10336 [v2.10.3.13]:
    //   if (Math.Abs(watts - target) > 2)
    //   {
    //       float diff_dBm = (float)Math.Round(
    //           (WattsTodBm(watts) - WattsTodBm((double)target)), 3);
    //       SetupForm.SetBypassGain(bands[i], g + diff_dBm);
    //   }
    //
    // NereusSDR maps to PaProfile::setAdjust because the per-step adjust
    // matrix is the per-drive-level finetune (as opposed to setBypassGain
    // which writes the base row only).  This matches the Phase 7 brief's
    // intent: "iterates HF bands x drive levels, writes new gain values
    // into the active profile".
    const double diffDbm = wattsToDbm(observedWatts)
                           - wattsToDbm(m_autoCalTargetWatts);

    PaProfile mutated = *active;
    // Negative sign: too-much output -> increase the gain table (reduces
    // future audio_volume so the radio outputs less); too-little output
    // -> decrease the gain table.  This matches Thetis sign convention
    // (g + diff_dBm where diff > 0 when observed > target; the gain table
    // is then larger which gets subtracted from target_dBm yielding a
    // smaller audio_volume).
    mutated.setAdjust(band, driveStep, static_cast<float>(diffDbm));
    m_paProfileManager->saveProfile(active->name(), mutated);
}

void PaGainByBandPage::refreshAutoCalUi()
{
    if (!m_autoCalStatusLabel || !m_autoCalProgressBar) { return; }

    QString status;
    int progressPercent = 0;
    bool error = false;
    switch (m_autoCalState) {
        case AutoCalState::Idle:
            status = QStringLiteral("Idle");
            progressPercent = 0;
            break;
        case AutoCalState::Initializing:
            status = QStringLiteral("Initializing...");
            break;
        case AutoCalState::SweepingBand:
        case AutoCalState::SettlingStep:
        case AutoCalState::ReadingPower:
        case AutoCalState::AdvancingStep: {
            const int drivePct = (m_autoCalCurrentDriveStep + 1) * 10;
            status = QStringLiteral("Calibrating %1 at %2%% drive")
                         .arg(bandLabel(m_autoCalCurrentBand))
                         .arg(drivePct);
            // Total steps = 11 HF bands * 9 drive steps = 99.
            int currentIdx = 0;
            for (int i = 0; i < static_cast<int>(kAutoCalHfBands.size()); ++i) {
                if (kAutoCalHfBands[i] == m_autoCalCurrentBand) {
                    currentIdx = i;
                    break;
                }
            }
            const int stepCount = currentIdx * kAutoCalDriveSteps
                                  + m_autoCalCurrentDriveStep;
            progressPercent = (stepCount * 100)
                              / (static_cast<int>(kAutoCalHfBands.size())
                                 * kAutoCalDriveSteps);
            break;
        }
        case AutoCalState::AdvancingBand:
            status = QStringLiteral("Advancing to next band...");
            break;
        case AutoCalState::Halting:
            status = QStringLiteral("Halting...");
            break;
        case AutoCalState::Completed:
            status = QStringLiteral("Sweep complete.");
            progressPercent = 100;
            break;
        case AutoCalState::Aborted:
            // abortAutoCal already set the label to the reason string;
            // don't overwrite.
            return;
    }

    m_autoCalStatusLabel->setText(status);
    if (!error) {
        // Per STYLEGUIDE.md: muted-italic status restores the kTextScale
        // shade (matches the auto-cal panel's idle-state look).
        m_autoCalStatusLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-style: italic; }").arg(Style::kTextScale));
    }
    m_autoCalProgressBar->setValue(progressPercent);
}

double PaGainByBandPage::autoCalBandMaxWatts(Band band) const
{
    if (m_paProfileManager) {
        const PaProfile* active = m_paProfileManager->activeProfile();
        if (active) {
            const float profileMax = active->getMaxPower(band);
            if (profileMax > 0.5f) {
                return static_cast<double>(profileMax);
            }
        }
    }
    return defaultMaxWattsForModelBand(m_connectedModel, band);
}

#ifdef NEREUS_BUILD_TESTS

void PaGainByBandPage::simulateBandFwdReadingForTest(Band band, int driveStep,
                                                     double watts)
{
    // Test seam: bypass the live RadioStatus path + settle timer.  Drives
    // the state machine deterministically by:
    //   1) cursor must match (band, driveStep) — otherwise no-op (the
    //      state machine has already advanced past or hasn't reached it).
    //   2) state must be Settling/Reading — otherwise no-op (sweep idle).
    //   3) call into onFwdPowerSample directly.
    if (m_autoCalState != AutoCalState::SettlingStep
        && m_autoCalState != AutoCalState::ReadingPower) {
        return;
    }
    if (m_autoCalCurrentBand != band
        || m_autoCalCurrentDriveStep != driveStep) {
        return;
    }
    // Stop the settle timer so we don't race a real fire.
    if (m_autoCalSettleTimer && m_autoCalSettleTimer->isActive()) {
        m_autoCalSettleTimer->stop();
    }
    onFwdPowerSample(watts, /*revW=*/0.0, /*swr=*/1.0);
}

void PaGainByBandPage::completeAutoCalForTest()
{
    // Synchronous fast-forward: inject target-watts readings at every
    // (band, step) pair until the state machine reaches Completed.  Used
    // by the completion / HF-only test cases.
    int safetyTicks = 11 * kAutoCalDriveSteps + 5;  // a couple of slack
    while (m_autoCalState != AutoCalState::Completed
           && m_autoCalState != AutoCalState::Aborted
           && m_autoCalState != AutoCalState::Idle
           && safetyTicks-- > 0) {
        const Band band = m_autoCalCurrentBand;
        const int step = m_autoCalCurrentDriveStep;
        if (!isAutoCalBand(band)) {
            // Should never happen — sweep is HF-only by construction.
            break;
        }
        simulateBandFwdReadingForTest(band, step, m_autoCalTargetWatts);
    }
}

#endif  // NEREUS_BUILD_TESTS

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
    // Per STYLEGUIDE.md and StyleConstants.h conventions.
    // Apply the canonical dark page palette so QGroupBox / QPushButton /
    // QCheckBox / QLabel inherit the project look (matches every other
    // Setup page).
    Style::applyDarkPageStyle(this);

    if (!model) {
        // Without a model we have nothing to populate the cal group from.
        // Fall back to a brief placeholder so tests / model-less previews still
        // render something coherent.
        // ASCII em-dash (--) for the disabled-italic placeholder; see
        // PaGainByBandPage above for the rationale.
        // Source: Thetis setup.designer.cs:49304-49309 [v2.10.3.13]
        auto* lbl = buildPlaceholderLabel(QStringLiteral(
            "Watt Meter -- requires a connected radio."));
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
        // R-R3-46 (parity Task 6): in a remote window the table starts
        // closed until SetupDialog pushes the version 6 gate.
        if (!model->ownsLocalDsp()) {
            setTransmitSettingsPermittedAt(6, false, QString());
        }
    }

    // ── chkPAValues "Show PA Values page" toggle ────────────────────────────
    // From Thetis chkPAValues setup.designer.cs:51155-51177 [v2.10.3.13]
    // + setup.cs:16340-16343 toggle.  Thetis flips panelPAValues.Visible
    // inline; NereusSDR persists the visibility hint to AppSettings under
    // "display/showPaValuesPage" (default "True") so the dedicated
    // PaValuesPage / SetupDialog navigation can honor it.
    m_showPaValuesCheck = new QCheckBox(tr("Show PA Values page"), this);
    m_showPaValuesCheck->setObjectName(QStringLiteral("chkPAValues"));
    m_showPaValuesCheck->setProperty("nereusSetupId", "pa.wattMeter.showPaValues");
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
            [this](bool checked) {
        AppSettings::instance().setValue(
            QStringLiteral("display/showPaValuesPage"),
            checked ? QStringLiteral("True") : QStringLiteral("False"));
        // R-R3-49 (found bug): nothing read the setting, so the PA Values
        // page stayed whatever the box said. Thetis shows or hides the
        // panel at once:
        // From Thetis setup.cs:16381-16385 [v2.10.3.15] chkPAValues_CheckedChanged
        //   panelPAValues.Visible = chkPAValues.Checked;
        // SetupDialog shows or hides the PA Values page.
        emit showPaValuesPageChanged(checked);
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
    m_resetPaValuesButton->setProperty("nereusSetupId", "pa.wattMeter.resetPaValues");
    // Tooltip is user-visible plain English; Thetis cite kept in the
    // surrounding header comment per CLAUDE.md "No source cites in
    // user-visible strings".
    m_resetPaValuesButton->setToolTip(tr(
        "Reset peak/min tracking on the PA Values page."));
    connect(m_resetPaValuesButton, &QPushButton::clicked, this,
            &PaWattMeterPage::resetPaValuesRequested);
    contentLayout()->insertWidget(contentLayout()->count() - 1, m_resetPaValuesButton);
}

// Phase 8 (#167) — PaWattMeterPage capability gate.
//
// From Thetis comboRadioModel_SelectedIndexChanged (setup.cs:19812-20310
// [v2.10.3.13+501e3f51]) — per-SKU PA tab visibility. The Thetis Watt Meter
// tab visibility is page-level: hidden when the connected board lacks PA
// hardware. Implemented here by toggling the PaCalibrationGroup + the
// chkPAValues toggle + Reset PA Values button; the page itself is hidden by
// SetupDialog when the parent PA category goes hidden (caps.isRxOnlySku
// or !caps.hasPaProfile).
void PaWattMeterPage::setTransmitSettingsPermittedAt(int version, bool permitted,
                                                     const QString& reason)
{
    // R-R3-46 / R-R3-49 (parity Task 6): the PA forward-power table
    // (hardware/<mac>/paCalibration/...) is taken by the Core on or off the
    // air, as Thetis has no transmit rule for it; a point changed while the
    // radio transmits reaches the meter once it is back on receive. Show PA
    // Values and Reset PA Values are this window's own.
    if (version != 6 || !m_paCalGroup) {
        return;
    }
    gateTransmitControls({m_paCalGroup}, permitted,
                         reason.isEmpty() ? IStationLink::transmitSettingsUnavailableReason()
                                          : reason);
}

void PaWattMeterPage::applyCapabilityVisibility(const BoardCapabilities& caps)
{
    // The PaCalibrationGroup auto-rebuilds via paCalProfileChanged when the
    // controller swaps board classes. We additionally hide it when the
    // connected board reports no PA support — this is the live cal-spinbox
    // editor and there is nothing to calibrate without an integrated PA.
    if (m_paCalGroup) {
        m_paCalGroup->setVisible(caps.hasPaProfile);
    }
    if (m_showPaValuesCheck) {
        m_showPaValuesCheck->setVisible(caps.hasPaProfile);
    }
    if (m_resetPaValuesButton) {
        m_resetPaValuesButton->setVisible(caps.hasPaProfile);
    }
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
    // Per STYLEGUIDE.md and StyleConstants.h conventions.
    // Apply the canonical dark page palette. QGroupBox titles + the embedded
    // MetricLabel widgets inherit consistent typography from this rule.
    Style::applyDarkPageStyle(this);

    if (!model) {
        // Model-less preview path: render a brief hint label so the page
        // doesn't ship as an empty widget when Setup is opened before a
        // RadioModel is wired (mirrors the PaWattMeterPage fallback).
        // Source: Thetis panelPAValues setup.designer.cs:51155-51177 [v2.10.3.13]
        auto* lbl = buildPlaceholderLabel(QStringLiteral(
            "PA Values -- requires a connected radio."));
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
    m_fwdCalibratedLabel->setProperty("nereusSetupId", "pa.values.forwardCalibrated");
    // Phase 5B (#167) — Raw FWD power label, scaled from raw ADC counts via
    // PaTelemetryScaling helpers (Phase 1B).
    // From Thetis panelPAValues textPAFwdPower setup.designer.cs:51155-51177
    // [v2.10.3.13] — `alex_fwd.ToString("f1") + " W"` at console.cs:24670.
    m_fwdRawLabel        = new MetricLabel(QStringLiteral("FWD (raw)"),
                                           QStringLiteral("0.00 W"), powerGroup);
    m_fwdRawLabel->setProperty("nereusSetupId", "pa.values.forwardRawPower");
    m_revPowerLabel      = new MetricLabel(QStringLiteral("REV"),
                                           QStringLiteral("0.00 W"), powerGroup);
    m_revPowerLabel->setProperty("nereusSetupId", "pa.values.reflectedPower");
    m_swrLabel           = new MetricLabel(QStringLiteral("SWR"),
                                           QStringLiteral("1.00"), powerGroup);
    m_swrLabel->setProperty("nereusSetupId", "pa.values.swr");
    // Phase 5B (#167) — Drive label, populated from TransmitModel::power().
    // From Thetis panelPAValues textDrivePower setup.designer.cs:51155-51177
    // [v2.10.3.13] — Thetis renders averaged exciter drive in mW; NereusSDR
    // uses the slider position in W (it's already what the user just set
    // and the slider 0..100 maps directly to watts on most boards).
    m_driveLabel         = new MetricLabel(QStringLiteral("Drive"),
                                           QStringLiteral("0 W"), powerGroup);
    m_driveLabel->setProperty("nereusSetupId", "pa.values.drive");
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
    m_paCurrentLabel->setProperty("nereusSetupId", "pa.values.paCurrent");
    m_paTempLabel      = new MetricLabel(QStringLiteral("PA T"),
                                         QStringLiteral("0.0 \xC2\xB0""C"), paGroup);
    // R-IOS-18: the Setup description's id for the temperature readout.
    m_paTempLabel->setProperty("nereusSetupId", "pa.values.paTemperature");
    m_supplyVoltsLabel = new MetricLabel(QStringLiteral("V"),
                                         QStringLiteral("0.0 V"), paGroup);
    m_supplyVoltsLabel->setProperty("nereusSetupId", "pa.values.dcVoltage");
    // Phase 5B (#167) — FWD/REV RF voltage labels, derived from raw ADC
    // via PaTelemetryScaling::scaleFwdRevVoltage (Phase 1B).
    // From Thetis panelPAValues textFwdVoltage / textRevVoltage at
    // setup.designer.cs:51155-51177 [v2.10.3.13] — `volts.ToString("f2") + " V"`
    // at console.cs:25068 / :25002.
    m_fwdVoltageLabel  = new MetricLabel(QStringLiteral("FWD V"),
                                         QStringLiteral("0.00 V"), paGroup);
    m_fwdVoltageLabel->setProperty("nereusSetupId", "pa.values.forwardVoltage");
    m_revVoltageLabel  = new MetricLabel(QStringLiteral("REV V"),
                                         QStringLiteral("0.00 V"), paGroup);
    m_revVoltageLabel->setProperty("nereusSetupId", "pa.values.reflectedVoltage");
    m_adcOverloadLabel = new MetricLabel(QStringLiteral("ADC OVF"),
                                         QStringLiteral("No"), paGroup);
    m_adcOverloadLabel->setProperty("nereusSetupId", "pa.values.adcOverload");
    paForm->addRow(QStringLiteral("PA Current:"),     m_paCurrentLabel);
    paForm->addRow(QStringLiteral("PA Temperature:"), m_paTempLabel);
    // Group A follow-up (group B fix wave): the AIN6 reading carries
    // Thetis's name for it. On MkII-class boards (the G2) it is not the
    // 13.8 V supply (RadioConnection::handleSupplyRaw).
    // From Thetis setup.designer.cs:51365 [v2.10.3.15]
    //   this.labelTS254.Text = "DC Voltage";
    // That label ships hidden; the reading itself carries the name in
    // Thetis's own computation of it (parity mini-round):
    // From Thetis console.cs:24782 [v2.10.3.15]
    //   public float computeHermesDCVoltage()
    //   ... int adc = NetworkIO.getHermesDCVoltage();
    paForm->addRow(QStringLiteral("DC Voltage:"), m_supplyVoltsLabel);
    paForm->addRow(QStringLiteral("FWD Voltage:"),    m_fwdVoltageLabel);
    paForm->addRow(QStringLiteral("REV Voltage:"),    m_revVoltageLabel);
    paForm->addRow(QStringLiteral("ADC Overload:"),   m_adcOverloadLabel);
    contentLayout()->insertWidget(contentLayout()->count() - 1, paGroup);

    // ── Group: Raw ADC Values ─────────────────────────────────────────────
    auto* adcGroup = new QGroupBox(QStringLiteral("Raw ADC Values"), this);
    auto* adcForm  = new QFormLayout(adcGroup);
    m_fwdAdcLabel = new MetricLabel(QStringLiteral("FWD"),
                                    QStringLiteral("0"), adcGroup);
    m_fwdAdcLabel->setProperty("nereusSetupId", "pa.values.forwardAdc");
    m_revAdcLabel = new MetricLabel(QStringLiteral("REV"),
                                    QStringLiteral("0"), adcGroup);
    m_revAdcLabel->setProperty("nereusSetupId", "pa.values.reflectedAdc");
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
    m_resetButton->setProperty("nereusSetupId", "pa.values.resetPeakMin");
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
            [this, model](double fwdW, double revW, double swr) {
                if (model->paReadingsFromCore()) { return; }  // see the ctor's end
                applyPowerReadings(fwdW, revW, swr);
            });

    // R-R3-32 / R-R3-46 (parity Task 6): PA current, PA temperature and
    // supply volts come from the one PA reading source,
    // RadioModel::paReadings() (the Core's in a remote window), an absent
    // reading shown as unavailable.
    connect(model, &RadioModel::paReadingsChanged,
            this, &PaValuesPage::refreshPaReadings);

    // Live re-format on °C / °F toggle without waiting for the next
    // telemetry sample — the peak/min trackers stay in their canonical
    // °C representation; formatPaTempWithPeakMin converts at format time.
    connect(&PaTempUnitNotifier::instance(),
            &PaTempUnitNotifier::unitChanged, this,
            [this](PaTempUnit /*unit*/) {
        if (m_paTempLabel && m_paTempPresent) {
            m_paTempLabel->setValue(
                formatPaTempWithPeakMin(m_paTempCurrent, m_paTempPeakMin));
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
    m_fwdCalibratedLabel->setValue(formatWithPeakMin(
        m_fwdCurrent, m_fwdPeakMin, QStringLiteral(" W"), 2));
    m_revPowerLabel->setValue(formatWithPeakMin(
        m_revCurrent, m_revPeakMin, QStringLiteral(" W"), 2));
    m_swrLabel->setValue(formatWithPeakMin(
        m_swrCurrent, m_swrPeakMin, QString(), 2));
    refreshPaReadings();

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
        // Capture HPSDRModel by value so the lambda survives any later
        // hardware-profile swap (PaValuesPage reconstructs on Setup-open
        // anyway; this is just defensive against mid-page-life swaps).
        const HPSDRModel hpsdrModel = model->hardwareProfile().model;

        connect(conn, &RadioConnection::paTelemetryUpdated, this,
                [this, hpsdrModel](quint16 fwdRaw, quint16 revRaw,
                                   quint16 /*exciterRaw*/, quint16 /*userAdc0*/,
                                   quint16 /*userAdc1*/,   quint16 /*supply*/) {
                    applyRawAdc(hpsdrModel, fwdRaw, revRaw);
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

    // R-R3-32 / R-R3-46 (parity Task 6): in a remote window the power,
    // raw ADC and RF voltage readings are the Core's transmit readings.
    // R-R3-49 / R-R3-32 (parity Task 33): a Core at txReadingsVersion 1
    // sends them in `txState`; below it each says unavailable with the
    // reason rather than showing a 0 nothing measured.
    if (model->paReadingsFromCore()) {
        if (TransmitState* coreTx = model->stationTransmitState()) {
            // As the local page: power on each power reading, the raw
            // readings on each raw sample.
            connect(coreTx, &TransmitState::metersChanged, this, [this, coreTx]() {
                if (coreSendsTransmitReadings() && m_coreReadingsShown) {
                    applyPowerReadings(coreTx->forwardPowerWatts(),
                                       coreTx->reflectedPowerWatts(), coreTx->swr());
                }
            });
            connect(coreTx, &TransmitState::adcRawChanged, this, [this, model, coreTx]() {
                if (coreSendsTransmitReadings() && m_coreReadingsShown) {
                    applyRawAdc(model->hardwareProfile().model,
                                static_cast<quint16>(std::clamp<qint64>(coreTx->forwardAdcRaw(), 0, 65535)),
                                static_cast<quint16>(std::clamp<qint64>(coreTx->reflectedAdcRaw(), 0, 65535)));
                }
            });
        }
        connect(model, &RadioModel::stationTxReadingsVersionChanged, this,
                &PaValuesPage::refreshCoreTransmitReadings);
        // The Core's ADC overload, as its step attenuator reports it.
        if (StepAttenuatorFacade* stepAtt = model->stepAttFacade()) {
            connect(stepAtt, &StepAttenuatorFacade::overloadAdc0Changed, this,
                    [this](int) { refreshCoreAdcOverload(); });
            connect(stepAtt, &StepAttenuatorFacade::overloadAdc1Changed, this,
                    [this](int) { refreshCoreAdcOverload(); });
        }
        refreshCoreTransmitReadings();
    }
}

void PaValuesPage::applyPowerReadings(double fwdW, double revW, double swr)
{
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
}

void PaValuesPage::applyRawAdc(HPSDRModel hpsdrModel, quint16 fwdRaw, quint16 revRaw)
{
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
}

bool PaValuesPage::coreSendsTransmitReadings()
{
    RadioModel* const radio = model();
    return radio != nullptr && radio->paReadingsFromCore()
        && radio->stationTransmitState() != nullptr && radio->stationTxReadingsVersion() >= 1;
}

void PaValuesPage::refreshCoreTransmitReadings()
{
    RadioModel* const radio = model();
    if (radio == nullptr || !radio->paReadingsFromCore()) {
        return;
    }
    const QList<MetricLabel*> labels{m_fwdCalibratedLabel, m_fwdRawLabel, m_revPowerLabel,
                                     m_swrLabel, m_fwdVoltageLabel, m_revVoltageLabel,
                                     m_adcOverloadLabel, m_fwdAdcLabel, m_revAdcLabel};
    if (!coreSendsTransmitReadings()) {
        m_coreReadingsShown = false;
        const QString reason = TransmitState::txReadingNotSentText();
        for (MetricLabel* label : labels) {
            if (label) {
                label->setValue(tr("Unavailable"));
                label->setToolTip(reason);
            }
        }
        return;
    }
    const TransmitState* tx = radio->stationTransmitState();
    // As the local page does at construction, the readings the page opens
    // on are shown without starting the peak and minimum tracking; each
    // change after that is tracked (the handlers in the constructor).
    m_coreReadingsShown = true;
    m_fwdCurrent = tx->forwardPowerWatts();
    m_revCurrent = tx->reflectedPowerWatts();
    m_swrCurrent = tx->swr();
    m_fwdCalibratedLabel->setValue(formatWithPeakMin(
        m_fwdCurrent, m_fwdPeakMin, QStringLiteral(" W"), 2));
    m_revPowerLabel->setValue(formatWithPeakMin(
        m_revCurrent, m_revPeakMin, QStringLiteral(" W"), 2));
    m_swrLabel->setValue(formatWithPeakMin(
        m_swrCurrent, m_swrPeakMin, QString(), 2));
    const auto rawOf = [](qint64 v) {
        return static_cast<quint16>(std::clamp<qint64>(v, 0, 65535));
    };
    applyRawAdc(radio->hardwareProfile().model, rawOf(tx->forwardAdcRaw()),
                rawOf(tx->reflectedAdcRaw()));
    refreshCoreAdcOverload();
    // R-R3-32: each says it is the Core's.
    for (MetricLabel* label : labels) {
        if (label) {
            label->setToolTip(tr("From the Core"));
        }
    }
}

void PaValuesPage::refreshCoreAdcOverload()
{
    RadioModel* const radio = model();
    if (!m_adcOverloadLabel || !coreSendsTransmitReadings()) {
        return;
    }
    // The Core's step attenuator reports each ADC's overload (0 none,
    // 1 yellow, 2 red); either one overloaded reads as the local page's
    // "Yes (ADC n)" does, until it clears.
    const StepAttenuatorFacade* stepAtt = radio->stepAttFacade();
    if (stepAtt == nullptr) {
        m_adcOverloadLabel->setValue(tr("Unavailable"));
        return;
    }
    if (stepAtt->overloadAdc0() > 0) {
        m_adcOverloadLabel->setValue(QStringLiteral("Yes (ADC %1)").arg(0));
    } else if (stepAtt->overloadAdc1() > 0) {
        m_adcOverloadLabel->setValue(QStringLiteral("Yes (ADC %1)").arg(1));
    } else {
        m_adcOverloadLabel->setValue(QStringLiteral("No"));
    }
}

// ---------------------------------------------------------------------------
// resetPaValues — public slot (Phase 5B #167).
// Phase 8 (#167) — PaValuesPage capability gate.
//
// From Thetis comboRadioModel_SelectedIndexChanged (setup.cs:19812-20310
// [v2.10.3.13+501e3f51]) — per-SKU PA tab visibility. Thetis embeds
// panelPAValues inside the Watt Meter tab and gates it on chkPAValues
// (setup.cs:16342 [v2.10.3.13]). NereusSDR promotes the readout to a
// dedicated page; per-SKU visibility hides every MetricLabel + every
// QGroupBox child when caps.hasPaProfile=false. SetupDialog also hides
// the parent PA category when caps.isRxOnlySku, so this method only
// has to toggle the page's own children.
void PaValuesPage::applyCapabilityVisibility(const BoardCapabilities& caps)
{
    // Toggle every MetricLabel child in one pass — no PA telemetry to
    // display when the board has no integrated PA. findChildren walks
    // the entire descendant tree so child labels nested inside QGroupBoxes
    // are caught.
    const QList<MetricLabel*> rows = findChildren<MetricLabel*>();
    for (MetricLabel* row : rows) {
        if (row) {
            row->setVisible(caps.hasPaProfile);
        }
    }
    // Also hide the parent QGroupBox containers so the page is visually
    // quiet on PA-less boards (no empty group headings).
    const QList<QGroupBox*> groups = findChildren<QGroupBox*>();
    for (QGroupBox* g : groups) {
        if (g) {
            g->setVisible(caps.hasPaProfile);
        }
    }
    // Reset button is meaningless without telemetry; gate it too.
    if (m_resetButton) {
        m_resetButton->setVisible(caps.hasPaProfile);
    }

    // ── Per-label gating for volts/amps telemetry rows (D5) ──────────────
    // From Thetis clsHardwareSpecific.cs:245-264 [v2.10.3.15]:
    //   HasVolts: ANAN7000D / ANAN8000D / ANVELINAPRO3 / ANAN_G2E //N1GP G2E added
    //             / ANAN_G2 / ANAN_G2_1K / REDPITAYA
    //   HasAmps:  same set
    // Boards without on-board PA current/voltage sensors (e.g. ANAN100,
    // ORIONMKII) hide these specific rows even when hasPaProfile=true.
    // Applied after the hasPaProfile pass so the per-label gate narrows
    // further for boards that have a PA but no volts/amps sensors.
    if (caps.hasPaProfile) {
        if (m_paCurrentLabel) {
            // From Thetis clsHardwareSpecific.cs:255-264 [v2.10.3.15] HasAmps.
            // //N1GP G2E added (line 260)
            m_paCurrentLabel->setVisible(caps.hasPaAmpsTelemetry);
        }
        if (m_supplyVoltsLabel) {
            // From Thetis clsHardwareSpecific.cs:245-254 [v2.10.3.15] HasVolts.
            // //N1GP G2E added (line 250)
            m_supplyVoltsLabel->setVisible(caps.hasPaVoltsTelemetry);
        }
    }
}

// Mirrors Thetis btnResetPAValues_Click semantic at setup.cs:16346-16357
// [v2.10.3.13].  Thetis simply blanks the textbox text strings; NereusSDR's
// spin tracks running peak/min and resets each tracker to its current value
// (so the live readout stays meaningful while history clears).
// ---------------------------------------------------------------------------
void PaValuesPage::resetPaValues()
{
    // R-R3-32 (parity Task 6): take the latest PA readings first, so each
    // tracker restarts at the live value rather than a stale one.
    refreshPaReadings();
    m_fwdPeakMin.reset(m_fwdCurrent);
    m_revPeakMin.reset(m_revCurrent);
    m_swrPeakMin.reset(m_swrCurrent);
    m_paCurrentPeakMin.reset(m_paCurrentCurrent);
    m_paTempPeakMin.reset(m_paTempCurrent);
    m_supplyPeakMin.reset(m_supplyCurrent);

    // Re-render every tracked label so the (P/M) annotation collapses to
    // P==M==current — the reset is visible to the user immediately even
    // when telemetry is paused (e.g. radio disconnected).
    // R-R3-32 (parity Task 6): a remote window's power readings stay
    // unavailable (see the ctor's end).
    const bool powerFromCore = model() && model()->paReadingsFromCore()
        && !coreSendsTransmitReadings();
    if (m_fwdCalibratedLabel && !powerFromCore) {
        m_fwdCalibratedLabel->setValue(formatWithPeakMin(
            m_fwdCurrent, m_fwdPeakMin, QStringLiteral(" W"), 2));
    }
    if (m_revPowerLabel && !powerFromCore) {
        m_revPowerLabel->setValue(formatWithPeakMin(
            m_revCurrent, m_revPeakMin, QStringLiteral(" W"), 2));
    }
    if (m_swrLabel && !powerFromCore) {
        m_swrLabel->setValue(formatWithPeakMin(
            m_swrCurrent, m_swrPeakMin, QString(), 2));
    }
    // R-R3-32 (parity Task 6): an absent PA reading stays unavailable.
    refreshPaReadings();
}

void PaValuesPage::refreshPaReadings()
{
    RadioModel* const radio = model();
    if (!radio) { return; }
    const RadioModel::PaReadings readings = radio->paReadings();
    const QString unavailable = tr("Unavailable");
    m_paCurrentPresent = readings.paCurrentAmps.has_value();
    m_paTempPresent = readings.paTemperatureCelsius.has_value();
    m_supplyPresent = readings.supplyVolts.has_value();
    if (m_paCurrentPresent) {
        m_paCurrentCurrent = *readings.paCurrentAmps;
        m_paCurrentPeakMin.update(m_paCurrentCurrent);
    }
    if (m_paTempPresent) {
        m_paTempCurrent = *readings.paTemperatureCelsius;
        m_paTempPeakMin.update(m_paTempCurrent);
    }
    if (m_supplyPresent) {
        m_supplyCurrent = *readings.supplyVolts;
        m_supplyPeakMin.update(m_supplyCurrent);
    }
    if (m_paCurrentLabel) {
        m_paCurrentLabel->setValue(m_paCurrentPresent
            ? formatWithPeakMin(m_paCurrentCurrent, m_paCurrentPeakMin, QStringLiteral(" A"), 2)
            : unavailable);
    }
    if (m_paTempLabel) {
        m_paTempLabel->setValue(m_paTempPresent
            ? formatPaTempWithPeakMin(m_paTempCurrent, m_paTempPeakMin)
            : unavailable);
    }
    if (m_supplyVoltsLabel) {
        m_supplyVoltsLabel->setValue(m_supplyPresent
            ? formatWithPeakMin(m_supplyCurrent, m_supplyPeakMin, QStringLiteral(" V"), 1)
            : unavailable);
    }
    // R-R3-32: a remote window's readings are the Core's, and say so.
    const QString source = radio->paReadingsFromCore() ? tr("From the Core") : QString();
    for (MetricLabel* label : {m_paCurrentLabel, m_paTempLabel, m_supplyVoltsLabel}) {
        if (label) { label->setToolTip(source); }
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

QString PaValuesPage::formatPaTempWithPeakMin(double currentC,
                                              const PeakMin& pmCelsius)
{
    const PaTempUnit unit = PaTempUnitNotifier::currentUnit();
    auto convert = [unit](double c) -> double {
        return (unit == PaTempUnit::Fahrenheit)
                   ? PaTempUnitNotifier::celsiusToFahrenheit(c)
                   : c;
    };
    const QString unitSuffix = (unit == PaTempUnit::Fahrenheit)
                                   ? QString::fromUtf8(" \xC2\xB0""F")
                                   : QString::fromUtf8(" \xC2\xB0""C");
    PeakMin convertedPm;
    if (pmCelsius.valid()) {
        // PeakMin's update() seeds the trackers; we set the converted
        // values directly so we don't have to clone the class. valid()
        // returns true once both peak and min have been set to non-
        // sentinel values, so once-valid stays valid through conversion.
        convertedPm.peak = convert(pmCelsius.peak);
        convertedPm.min  = convert(pmCelsius.min);
    }
    return formatWithPeakMin(convert(currentC), convertedPm, unitSuffix, 1);
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

// D5 (ANAN-G2E port): visibility seams for amps/volts rows.
// From Thetis clsHardwareSpecific.cs:255-264 [v2.10.3.15] HasAmps.
// //N1GP G2E added (line 260 [v2.10.3.15])
bool PaValuesPage::isPaCurrentRowVisibleForTest() const
{
    return m_paCurrentLabel && m_paCurrentLabel->isVisibleTo(
               const_cast<PaValuesPage*>(this));
}
// From Thetis clsHardwareSpecific.cs:245-254 [v2.10.3.15] HasVolts.
// //N1GP G2E added (line 250 [v2.10.3.15])
bool PaValuesPage::isSupplyVoltsRowVisibleForTest() const
{
    return m_supplyVoltsLabel && m_supplyVoltsLabel->isVisibleTo(
               const_cast<PaValuesPage*>(this));
}
#endif

} // namespace NereusSDR
