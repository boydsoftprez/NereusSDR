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
//   2026-10-08  J.J. Boyd (KG4VCF): Qt 6.11.0 Cocoa accessibility cache
//               repair on a live item view (QtCocoaAccessibleTableCache).
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09 - PA Values' first temperature text spells the degree sign
//                 as U+00B0: its UTF-8 bytes in a UTF-16 literal read as
//                 two characters. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-09 - D25: the gain toolbar stacks its profile combo over its
//                 buttons when narrow (SetupFit::FitRow), and the gain table,
//                 too wide for the window, scrolls sideways inside its own
//                 frame (SetupFit::SidewaysFrame). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-15  J.J. Boyd (KG4VCF): rows change after a real model reset
//               (resetItemsBeforeRowChange), so accessibility cannot read a
//               deleted cell. AI-assisted via Anthropic Claude Code.
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
#include "gui/setup/SetupFit.h"
#include "gui/QtCocoaAccessibleTableCache.h"

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
#include "core/mmio/ExternalVariableEngine.h"
#include "core/mmio/MmioEndpoint.h"
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
#include <QDialog>
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
#include <algorithm>

#include <QLoggingCategory>
#include <QElapsedTimer>
#include <QHideEvent>

Q_LOGGING_CATEGORY(lcPaAutoCal, "nereus.setup.pacal")

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
            QStringLiteral("Auto-Calibrate Sequence"), this);
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
    //
    // D25: the buttons go under the combo when the window is too narrow for
    // the row.
    auto* toolbar = new SetupFit::FitRow;
    toolbar->setObjectName(QStringLiteral("paGainToolbar"));
    toolbar->setSpacing(6);
    auto* lifecycleButtons = new QHBoxLayout;
    lifecycleButtons->setSpacing(6);

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
    lifecycleButtons->addWidget(m_btnNew);

    m_btnCopy = new QPushButton(QStringLiteral("Copy"), this);
    m_btnCopy->setMinimumWidth(kLifecycleButtonWidth);
    m_btnCopy->setProperty("nereusSetupId", "pa.gain.copy");
    m_btnCopy->setToolTip(QStringLiteral(
        "Duplicate the active profile under a new name."));
    lifecycleButtons->addWidget(m_btnCopy);

    m_btnDelete = new QPushButton(QStringLiteral("Delete"), this);
    m_btnDelete->setMinimumWidth(kLifecycleButtonWidth);
    m_btnDelete->setProperty("nereusSetupId", "pa.gain.delete");
    m_btnDelete->setToolTip(QStringLiteral(
        "Delete the active profile.  The last remaining profile cannot be "
        "deleted."));
    lifecycleButtons->addWidget(m_btnDelete);

    m_btnReset = new QPushButton(QStringLiteral("Reset Defaults"), this);
    m_btnReset->setMinimumWidth(kLifecycleButtonWidth);
    m_btnReset->setProperty("nereusSetupId", "pa.gain.reset");
    m_btnReset->setToolTip(QStringLiteral(
        "Re-seed the active profile from the canonical factory PA gain row "
        "for its model.  Drive-step adjusts and max-power columns are "
        "cleared."));
    lifecycleButtons->addWidget(m_btnReset);
    toolbar->addLayout(lifecycleButtons);

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
        // Thetis parity: with Use Max these are percentages of the band's
        // Max W (the cell tooltip shows the watts); without it they are points
        // on the 0-100 drive scale.
        lbl->setToolTip(QStringLiteral(
            "Trim point at %1% of the band's Max W when Use Max is on "
            "(otherwise %1 on the 0-100 drive scale).").arg(drivePct));
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

    // D25: fourteen bands by thirteen columns of fixed-width boxes cannot
    // fit the Settings window, so the table scrolls sideways inside its own
    // frame (the page itself never does).
    auto* gainFrame = new SetupFit::SidewaysFrame(this);
    gainFrame->setObjectName(QStringLiteral("paGainTableFrame"));
    gainFrame->setWidget(gainGroup);
    contentLayout()->insertWidget(contentLayout()->count() - 1, gainFrame);

    // ── Bottom: "Auto-Calibrate Sequence" + per-band panel (2026-11-17) ──
    // One band at a time. The checkbox only reveals the panel; Start keys
    // TUNE, measures FWD and trims that band's base Gain until it is within
    // -1/+2 W of Max W (capped at 100 W). The nine trim points are manual and
    // are not touched.
    m_autoCalibrateCheck = new QCheckBox(
        QStringLiteral("Auto-Calibrate Sequence"), this);
    m_autoCalibrateCheck->setToolTip(QStringLiteral(
        "Measure one band's PA Gain. You key TUNE into a dummy load; the "
        "panel reads the selected meter, suggests a Gain correction, and only "
        "Apply changes it. It does not key, change bands or touch the nine "
        "trim points."));
    contentLayout()->insertWidget(contentLayout()->count() - 1,
                                   m_autoCalibrateCheck);

    m_autoCalPanel = new QGroupBox(
        QStringLiteral("Auto-Calibrate Sequence"), this);
    m_autoCalPanel->setVisible(false);
    auto* autoCalForm = new QFormLayout(m_autoCalPanel);

    m_autoCalWarning = new QLabel(
        QStringLiteral("Dummy load only. Hold TUNE while measuring; nothing "
                       "keys automatically. Apply writes the Gain."),
        m_autoCalPanel);
    m_autoCalWarning->setWordWrap(true);
    m_autoCalWarning->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-weight: bold; font-size: 12px; }")
            .arg(Style::kRedBorder));
    autoCalForm->addRow(m_autoCalWarning);

    m_autoCalConfirm = new QCheckBox(
        QStringLiteral("Dummy load connected"), m_autoCalPanel);
    autoCalForm->addRow(m_autoCalConfirm);

    m_autoCalBandCombo = new QComboBox(m_autoCalPanel);
    {
        const std::array<Band, 11> bands = {
            Band::Band160m, Band::Band80m, Band::Band60m, Band::Band40m,
            Band::Band30m,  Band::Band20m, Band::Band17m, Band::Band15m,
            Band::Band12m,  Band::Band10m, Band::Band6m,
        };
        for (Band b : bands) {
            m_autoCalBandCombo->addItem(bandLabel(b), static_cast<int>(b));
        }
    }
    autoCalForm->addRow(QStringLiteral("Band:"), m_autoCalBandCombo);

    // The reading source: the radio's own meter or a WaveNode channel. The
    // helper sends fwdpwr1..4 / refpwr1..4 (WN_DemoDlg.cpp), so PWRn here is
    // the WaveNode's PWRn input, in watts.
    m_autoCalSourceCombo = new QComboBox(m_autoCalPanel);
    m_autoCalSourceCombo->addItem(QStringLiteral("Anvelina FWD (radio)"), 0);
    for (int n = 1; n <= 4; ++n) {
        m_autoCalSourceCombo->addItem(
            QStringLiteral("WaveNode PWR%1").arg(n), n);
    }
    m_autoCalSource = AppSettings::instance()
                          .value(QStringLiteral("PaAutoCal/PowerSource"), 0)
                          .toInt();
    {
        const int idx = m_autoCalSourceCombo->findData(m_autoCalSource);
        if (idx >= 0) { m_autoCalSourceCombo->setCurrentIndex(idx); }
    }
    m_autoCalSourceCombo->setToolTip(QStringLiteral(
        "Which forward-power reading the sequence trusts. WaveNode PWRn is "
        "the helper's fwdpwrN input, in watts."));
    autoCalForm->addRow(QStringLiteral("Read power:"), m_autoCalSourceCombo);
    connect(m_autoCalSourceCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
        m_autoCalSource = m_autoCalSourceCombo->currentData().toInt();
        AppSettings::instance().setValue(
            QStringLiteral("PaAutoCal/PowerSource"), m_autoCalSource);
        updateAutoCalTargets();
    });

    // Safety: the operator keys TUNE, but the panel watches it. A keyed
    // carrier beyond the selected time is unkeyed for them, and anything over
    // 100 W (or target + 10 W) is unkeyed at once.
    m_autoCalTimeoutCombo = new QComboBox(m_autoCalPanel);
    for (int seconds : { 5, 10, 15, 20, 30, 60 }) {
        m_autoCalTimeoutCombo->addItem(
            QStringLiteral("%1 s").arg(seconds), seconds);
    }
    {
        const int saved = AppSettings::instance()
                              .value(QStringLiteral("PaAutoCal/KeyLimitSec"), 10)
                              .toInt();
        const int idx = m_autoCalTimeoutCombo->findData(saved);
        if (idx >= 0) { m_autoCalTimeoutCombo->setCurrentIndex(idx); }
    }
    m_autoCalTimeoutCombo->setToolTip(QStringLiteral(
        "Safety: if TUNE/MOX stays keyed longer than this, the panel unkeys "
        "it. Also unkeys above 100 W or target + 10 W."));
    autoCalForm->addRow(QStringLiteral("Key limit:"), m_autoCalTimeoutCombo);
    connect(m_autoCalTimeoutCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
        AppSettings::instance().setValue(
            QStringLiteral("PaAutoCal/KeyLimitSec"),
            m_autoCalTimeoutCombo->currentData().toInt());
    });

    m_autoCalToleranceCombo = new QComboBox(m_autoCalPanel);
    m_autoCalToleranceCombo->addItem(QStringLiteral("-1 / +2 W"), 0);
    m_autoCalToleranceCombo->addItem(QStringLiteral("+/- 1 W"), 1);
    m_autoCalToleranceCombo->addItem(QStringLiteral("+/- 2 W"), 2);
    m_autoCalToleranceCombo->addItem(QStringLiteral("+/- 5 W"), 3);
    {
        const int saved = AppSettings::instance()
                              .value(QStringLiteral("PaAutoCal/Tolerance"), 0)
                              .toInt();
        const int idx = m_autoCalToleranceCombo->findData(saved);
        if (idx >= 0) { m_autoCalToleranceCombo->setCurrentIndex(idx); }
    }
    m_autoCalToleranceCombo->setToolTip(QStringLiteral(
        "How close to Max W counts as done. The default is -1 W / +2 W; the "
        "panel shows IN TOLERANCE when a measurement lands inside it."));
    autoCalForm->addRow(QStringLiteral("Tolerance:"),
                        m_autoCalToleranceCombo);
    connect(m_autoCalToleranceCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
        AppSettings::instance().setValue(
            QStringLiteral("PaAutoCal/Tolerance"),
            m_autoCalToleranceCombo->currentData().toInt());
        updateAutoCalTargets();
    });

    m_autoCalWatchdog = new QTimer(this);
    m_autoCalWatchdog->setInterval(250);
    connect(m_autoCalWatchdog, &QTimer::timeout,
            this, &PaGainByBandPage::onAutoCalWatchdog);

    m_autoCalStatusLabel = new QLabel(QStringLiteral("Idle"), m_autoCalPanel);
    m_autoCalStatusLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-style: italic; }").arg(Style::kTextScale));
    autoCalForm->addRow(QStringLiteral("Status:"), m_autoCalStatusLabel);

    m_autoCalProgressBar = new QProgressBar(m_autoCalPanel);
    m_autoCalProgressBar->setRange(0, 100);
    m_autoCalProgressBar->setValue(0);
    autoCalForm->addRow(QStringLiteral("Progress:"), m_autoCalProgressBar);

    // Read-only: the target is the band's Max W, capped at 100 W.
    m_autoCalTargetLabel = new QLabel(QStringLiteral("100 W (limit)"),
                                      m_autoCalPanel);
    m_autoCalTargetLabel->setToolTip(QStringLiteral(
        "Target = this band's Max W, or the 100 W limit when no Max W/Use Max "
        "is set."));
    m_autoCalTargetLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; font-weight: bold; }")
            .arg(Style::kTitleText));
    autoCalForm->addRow(QStringLiteral("Target watts:"),
                        m_autoCalTargetLabel);

    m_autoCalGainSpin = new QDoubleSpinBox(m_autoCalPanel);
    m_autoCalGainSpin->setRange(38.8, 100.0);
    m_autoCalGainSpin->setSingleStep(0.1);
    m_autoCalGainSpin->setDecimals(1);
    m_autoCalGainSpin->setToolTip(QStringLiteral(
        "Base PA Gain for the selected band: the Max W / 100 W setpoint "
        "adjustment. The nine trim points above are separate and are not "
        "changed here."));
    autoCalForm->addRow(QStringLiteral("PA Gain (dB):"), m_autoCalGainSpin);

    m_autoCalModeLabel = new QLabel(
        QStringLiteral("Mode: Base Gain (Max W)"), m_autoCalPanel);
    m_autoCalModeLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextScale));
    autoCalForm->addRow(QStringLiteral("Mode:"), m_autoCalModeLabel);
    connect(m_autoCalGainSpin,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double value) { setAutoCalGainFromSpin(value); });

    auto* steps = new QLabel(
        QStringLiteral("Steps: 1) dummy load  2) drive to maximum (Max W, "
                       "100 W limit)  3) key TUNE  4) Measure  5) Apply."),
        m_autoCalPanel);
    steps->setWordWrap(true);
    steps->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextScale));
    autoCalForm->addRow(steps);

    m_autoCalCountdownLabel = new QLabel(QStringLiteral("Key time: --"),
                                         m_autoCalPanel);
    m_autoCalCountdownLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextScale));
    autoCalForm->addRow(m_autoCalCountdownLabel);

    auto* autoCalButtons = new QHBoxLayout;
    m_autoCalStartButton = new QPushButton(QStringLiteral("Measure"),
                                           m_autoCalPanel);
    m_autoCalStartButton->setToolTip(QStringLiteral(
        "Read the selected meter while you hold TUNE. It does not key or "
        "change bands."));
    m_autoCalTuneButton = new QPushButton(QStringLiteral("TUNE"),
                                          m_autoCalPanel);
    m_autoCalTuneButton->setCheckable(true);
    m_autoCalTuneButton->setToolTip(QStringLiteral(
        "Key TUNE (dummy load only). The key-limit and 100 W watchdog unkey "
        "it automatically."));
    connect(m_autoCalTuneButton, &QPushButton::toggled, this,
            [this](bool on) {
        if (m_autoCalTuneUpdating) { return; }
        if (RadioModel* const radio = this->model()) {
            radio->setTune(on);
        }
    });
    m_autoCalApplyButton = new QPushButton(QStringLiteral("Apply"),
                                           m_autoCalPanel);
    m_autoCalApplyButton->setToolTip(QStringLiteral(
        "Write the suggested Gain correction for this band."));
    m_autoCalCancelButton = new QPushButton(QStringLiteral("Clear"),
                                            m_autoCalPanel);
    m_autoCalCancelButton->setToolTip(QStringLiteral(
        "Drop the suggestion without changing the Gain."));
    autoCalButtons->addWidget(m_autoCalStartButton);
    autoCalButtons->addWidget(m_autoCalTuneButton);
    autoCalButtons->addWidget(m_autoCalApplyButton);
    autoCalButtons->addWidget(m_autoCalCancelButton);
    m_autoCalLinearButton = new QPushButton(QStringLiteral("PA Bnd Ln"),
                                            m_autoCalPanel);
    m_autoCalLinearButton->setToolTip(QStringLiteral(
        "PA Band Linearization. Run Auto-Calibrate Sequence for this band "
        "first. Highlights this band's 10 % group; pick a point to set the "
        "tune slider there, then TUNE, Measure, Apply."));
    autoCalButtons->addWidget(m_autoCalLinearButton);
    autoCalButtons->addStretch();
    autoCalForm->addRow(autoCalButtons);
    connect(m_autoCalLinearButton, &QPushButton::clicked,
            this, &PaGainByBandPage::openLinearizationPopup);

    contentLayout()->insertWidget(contentLayout()->count() - 1, m_autoCalPanel);

    m_autoCalStartButton->setEnabled(false);
    m_autoCalApplyButton->setEnabled(false);
    m_autoCalCancelButton->setEnabled(false);

    connect(m_autoCalibrateCheck, &QCheckBox::toggled,
            this, &PaGainByBandPage::onAutoCalibrateToggled);
    connect(m_autoCalStartButton, &QPushButton::clicked,
            this, &PaGainByBandPage::startAutoCal);
    connect(m_autoCalApplyButton, &QPushButton::clicked,
            this, &PaGainByBandPage::applyAutoCalSuggestion);
    connect(m_autoCalCancelButton, &QPushButton::clicked,
            this, &PaGainByBandPage::stopAutoCal);
    connect(m_autoCalConfirm, &QCheckBox::toggled, this,
            [this](bool) { updateAutoCalTargets(); });
    connect(m_autoCalBandCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
        // A pending measurement belongs to the old band: put its drive and
        // limits back before the new band is examined.
        if (m_linearizeDialog) { m_linearizeDialog->close(); }
        stopAutoCal();
        updateAutoCalTargets();
    });

    // Follow the radio: moving to another band selects it here (and restores
    // a pending measurement first), so the target always matches the TX band.
    if (model) {
        connect(model, &RadioModel::transmitBandChanged, this, [this, model] {
            if (!m_autoCalPanel || !m_autoCalPanel->isVisible()
                || !m_autoCalBandCombo) {
                return;
            }
            SliceModel* const tx = model->txBoundSlice();
            if (!tx) { return; }
            const int idx =
                m_autoCalBandCombo->findData(static_cast<int>(tx->band()));
            if (idx < 0 || m_autoCalBandCombo->currentIndex() == idx) {
                return;
            }
            stopAutoCal();
            if (m_linearizeDialog) { m_linearizeDialog->close(); }
            const QSignalBlocker block(m_autoCalBandCombo);
            m_autoCalBandCombo->setCurrentIndex(idx);
            updateAutoCalTargets();
        });
    }

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
    resetItemsBeforeRowChange(m_profileCombo);
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

    // Per-band watt targets for the nine trim points (Use Max decides).
    refreshAdjustTargets();

    // Mirrors Thetis updatePAControls at setup.cs:22884-22905 [v2.10.3.13]:
    // group title rewrites to "<profile> - PA Gain By Band (dB)" so the user
    // sees which profile's values they're editing.
    if (m_gainByBandGroup) {
        m_gainByBandGroup->setTitle(
            tr("%1: PA Gain By Band (dB)").arg(profile.name()));
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

void PaGainByBandPage::applyCeilingEditOnAir(Band band)
{
    RadioModel* const radio = model();
    if (radio && radio->ownsLocalDsp() && radio->paOnAirNow()
        && static_cast<int>(band) == radio->paOnAirBandIndex()) {
        radio->applyPaEditOnAir(RadioModel::PaProfileAction::SetMaxPower, -1);
    }
}

void PaGainByBandPage::refreshAdjustTargets()
{
    const PaProfile* active =
        m_paProfileManager ? m_paProfileManager->activeProfile() : nullptr;
    if (!active) { return; }

    for (int n = 0; n < kPaBandCount; ++n) {
        const Band band = static_cast<Band>(n);
        const double maxW = static_cast<double>(active->getMaxPower(band));
        const bool useMax = active->getMaxPowerUse(band) && maxW > 0.5;
        for (int step = 0; step < kColAdjustCount; ++step) {
            QDoubleSpinBox* spin = m_adjustSpins[n][step];
            if (!spin) { continue; }
            const int pct = (step + 1) * 10;
            spin->setToolTip(
                useMax
                    ? tr("Trim at %1% of %2's %3 W max (%4 W). "
                         "Positive adds output, negative reduces it.")
                          .arg(pct)
                          .arg(bandLabel(band))
                          .arg(maxW, 0, 'f', 0)
                          .arg(pct * maxW / 100.0, 0, 'f', 1)
                    : tr("Trim at %1% of the 0-100 drive scale for %2. "
                         "Positive adds output, negative reduces it.")
                          .arg(pct)
                          .arg(bandLabel(band)));
        }
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
    applyCeilingEditOnAir(band);
    refreshAdjustTargets();
    updateAutoCalTargets();
}

void PaGainByBandPage::onUseMaxPowerToggled(Band band, bool use)
{
    if (m_updatingFromProfile || !m_paProfileManager) { return; }

    const PaProfile* active = m_paProfileManager->activeProfile();
    if (!active) { return; }

    PaProfile mutated = *active;
    mutated.setMaxPowerUse(band, use);
    m_paProfileManager->saveProfile(active->name(), mutated);
    applyCeilingEditOnAir(band);
    refreshAdjustTargets();
    updateAutoCalTargets();
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

// ---------------------------------------------------------------------------
// Per-band PA Gain auto-calibration (2026-11-17)
//
// One band at a time. Start keys TUNE into a dummy load, reads the radio's
// forward power, and trims that band's BASE Gain until the output sits within
// -1 W / +2 W of Max W (capped at 100 W), then unkeys. The nine 10 % trim
// cells stay manual and are not touched. Thetis's CalibratePAGain does the
// same base-gain correction per band (console.cs:10228-10387 [v2.10.3.13]);
// this port keeps the operator's dummy-load confirmation, uses the existing
// TUNE path and restores the drive state afterwards.
// ---------------------------------------------------------------------------

namespace {
constexpr int    kAutoCalMaxTries     = 8;      // gain corrections per run
constexpr int    kAutoCalRetuneWaitMs = 1500;   // after a band change
constexpr int    kAutoCalKeySettleMs  = 2000;   // after keying TUNE
constexpr int    kAutoCalStepWaitMs   = 1000;   // between corrections
constexpr double kAutoCalMaxTargetW   = 100.0;  // never calibrate above 100 W
constexpr double kAutoCalUnderW       = 1.0;    // -1 W
constexpr double kAutoCalOverW        = 2.0;    // +2 W
constexpr double kAutoCalGainMin      = 38.8;   // the Gain spin's floor
constexpr double kAutoCalGainMax      = 100.0;

// The WaveNode helper's endpoint: its four-char id first, then its name.
MmioEndpoint* waveNodeEndpointForCal()
{
    ExternalVariableEngine& engine = ExternalVariableEngine::instance();
    if (MmioEndpoint* endpoint = engine.endpointByFourChar(QStringLiteral("Zjjg"))) {
        return endpoint;
    }
    for (MmioEndpoint* endpoint : engine.endpoints()) {
        if (endpoint
            && endpoint->name().contains(QStringLiteral("WaveNode"),
                                         Qt::CaseInsensitive)) {
            return endpoint;
        }
    }
    return nullptr;
}
}  // namespace

double PaGainByBandPage::readAutoCalForwardWatts()
{
    if (m_autoCalSource <= 0) {
        return model() ? model()->radioStatus().forwardPowerWatts() : 0.0;
    }
    if (MmioEndpoint* endpoint = waveNodeEndpointForCal()) {
        const QVariant value = endpoint->valueForName(
            QStringLiteral("fwdpwr%1").arg(m_autoCalSource));
        if (value.isValid()) { return value.toDouble(); }
    }
    return 0.0;
}

double PaGainByBandPage::readAutoCalReflectedWatts()
{
    if (m_autoCalSource <= 0) {
        return model() ? model()->radioStatus().reflectedPowerWatts() : 0.0;
    }
    if (MmioEndpoint* endpoint = waveNodeEndpointForCal()) {
        const QVariant value = endpoint->valueForName(
            QStringLiteral("refpwr%1").arg(m_autoCalSource));
        if (value.isValid()) { return value.toDouble(); }
    }
    return 0.0;
}

Band PaGainByBandPage::selectedAutoCalBand() const
{
    if (m_autoCalBandCombo) {
        return static_cast<Band>(m_autoCalBandCombo->currentData().toInt());
    }
    return m_autoCalBand;
}

void PaGainByBandPage::updateAutoCalTargets()
{
    if (!m_autoCalBandCombo || !m_autoCalTargetLabel || !m_autoCalStatusLabel
        || !m_autoCalStartButton || !m_autoCalConfirm) {
        return;
    }

    const Band band = selectedAutoCalBand();
    m_autoCalBand = band;
    m_autoCalCurrentBand = band;

    double maxWatts = 0.0;
    bool useMax = false;
    if (m_paProfileManager) {
        if (const PaProfile* profile = m_paProfileManager->activeProfile()) {
            useMax = profile->getMaxPowerUse(band);
            maxWatts = static_cast<double>(profile->getMaxPower(band));
        }
    }
    const bool capped = maxWatts > kAutoCalMaxTargetW;
    if (capped) { maxWatts = kAutoCalMaxTargetW; }
    // Never blank: no Max W (or Use Max off) targets the 100 W limit.
    const bool haveBandMax = useMax && maxWatts > 0.5;
    const double bandTarget = haveBandMax ? maxWatts : kAutoCalMaxTargetW;
    const bool linear = m_autoCalLinearPoint >= 0;
    const int linearPercent = linear ? (m_autoCalLinearPoint + 1) * 10 : 100;
    const double shownTarget =
        linear ? linearPercent * bandTarget / 100.0 : bandTarget;
    m_autoCalTargetWatts = shownTarget;

    if (linear) {
        m_autoCalTargetLabel->setText(
            QStringLiteral("%1 W (%2 % of Max W)")
                .arg(shownTarget, 0, 'f', 1).arg(linearPercent));
    } else {
        m_autoCalTargetLabel->setText(
            haveBandMax
                ? QStringLiteral("%1 W (Max W%2)")
                      .arg(shownTarget, 0, 'f', 0)
                      .arg(capped ? QStringLiteral(", capped") : QString())
                : QStringLiteral("100 W (limit, no Max W)"));
    }

    // The panel's manual control follows the selected band.
    if (m_autoCalGainSpin && m_paProfileManager) {
        if (const PaProfile* profile = m_paProfileManager->activeProfile()) {
            m_autoCalUpdatingGain = true;
            m_autoCalGainSpin->setValue(
                profile->getGainForBand(band, /*driveValue=*/-1));
            m_autoCalUpdatingGain = false;
        }
    }

    const bool ready = m_autoCalConfirm->isChecked() && !m_autoCalRunning;
    m_autoCalStartButton->setEnabled(ready);
    if (m_autoCalRunning) { return; }

    QString text;
    if (!m_autoCalConfirm->isChecked()) {
        text = QStringLiteral("Confirm the dummy load to enable Measure.");
    } else {
        const QString source = m_autoCalSource <= 0
            ? QStringLiteral("radio FWD")
            : QStringLiteral("WaveNode PWR%1").arg(m_autoCalSource);
        double tolUnder = kAutoCalUnderW;
        double tolOver = kAutoCalOverW;
        autoCalTolerance(&tolUnder, &tolOver);
        const QString tolText = (tolUnder == tolOver)
            ? QStringLiteral("+/- %1 W").arg(tolUnder, 0, 'f', 1)
            : QStringLiteral("-%1/+%2 W")
                  .arg(tolUnder, 0, 'f', 0).arg(tolOver, 0, 'f', 0);
        text = QStringLiteral("Ready: %1, target %2 W%3 on %4, tol %5.")
                   .arg(bandLabel(band))
                   .arg(shownTarget, 0, 'f', 0)
                   .arg(haveBandMax ? QStringLiteral(" (Max W)")
                                    : QStringLiteral(" (limit)"))
                   .arg(source)
                   .arg(tolText);
    }
    m_autoCalStatusLabel->setText(text);
    m_autoCalStatusLabel->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-style: italic; }")
            .arg(Style::kTextScale));
}

void PaGainByBandPage::onAutoCalibrateToggled(bool checked)
{
    if (m_autoCalProgrammaticToggle) {
        if (m_autoCalPanel) { m_autoCalPanel->setVisible(false); }
        return;
    }

    if (m_autoCalPanel) { m_autoCalPanel->setVisible(checked); }
    if (m_autoCalWatchdog) {
        if (checked) {
            m_autoCalWatchdog->start();
        } else {
            // Closing the panel must not leave a TUNE it started on.
            if (m_autoCalTuneButton && m_autoCalTuneButton->isChecked()) {
                if (RadioModel* const radio = model()) {
                    radio->setTune(false);
                }
                m_autoCalTuneUpdating = true;
                m_autoCalTuneButton->setChecked(false);
                m_autoCalTuneUpdating = false;
            }
            m_autoCalWatchdog->stop();
            m_autoCalKeyClock.invalidate();
            restoreAutoCalDrive();
        }
    }
    if (checked) {
        // Open on the band the radio is transmitting on, so a Max W set on
        // that band is the target. The operator may change it afterwards.
        if (m_autoCalBandCombo && model()) {
            if (SliceModel* const tx = model()->txBoundSlice()) {
                const int idx = m_autoCalBandCombo->findData(
                    static_cast<int>(tx->band()));
                if (idx >= 0
                    && m_autoCalBandCombo->currentIndex() != idx) {
                    const QSignalBlocker block(m_autoCalBandCombo);
                    m_autoCalBandCombo->setCurrentIndex(idx);
                }
            }
        }
        if (!m_autoCalRunning) {
            m_autoCalState = AutoCalState::Idle;
            if (m_autoCalProgressBar) { m_autoCalProgressBar->setValue(0); }
            updateAutoCalTargets();
        }
    } else {
        // Hiding the panel also drops any suggestion; nothing is keyed.
        stopAutoCal();
    }
}

void PaGainByBandPage::startAutoCal()
{
    // MEASURE ONLY. The operator holds TUNE/MOX on a dummy load. This sets the
    // drive to 100 % so the reading is the Max W / 100 W setpoint, reads the
    // selected source, restores the operator's drive, and suggests a base-Gain
    // correction. Nothing keys, changes band or loops.
    RadioModel* const radio = model();
    const PaProfile* profile =
        m_paProfileManager ? m_paProfileManager->activeProfile() : nullptr;
    if (!radio || !profile || !m_autoCalConfirm
        || !m_autoCalConfirm->isChecked()) {
        return;
    }

    const Band band = selectedAutoCalBand();
    SliceModel* const txSlice = radio->txBoundSlice();
    if (!txSlice || txSlice->band() != band) {
        m_autoCalStatusLabel->setText(
            QStringLiteral("Tune the radio to %1 before measuring.")
                .arg(bandLabel(band)));
        m_autoCalStatusLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-style: italic; }")
                .arg(Style::kRedBorder));
        return;
    }
    if (!radio->isTune() && !radio->transmitModel().isMox()) {
        m_autoCalStatusLabel->setText(
            QStringLiteral("Key TUNE (dummy load), then press Measure."));
        m_autoCalStatusLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-style: italic; }")
                .arg(Style::kTextScale));
        return;
    }

    const bool useMax = profile->getMaxPowerUse(band);
    const double bandMax = static_cast<double>(profile->getMaxPower(band));
    m_autoCalBand = band;
    m_autoCalCurrentBand = band;
    m_autoCalCurrentDriveStep = m_autoCalLinearPoint >= 0 ? m_autoCalLinearPoint : 0;

    const bool linear = m_autoCalLinearPoint >= 0;
    const int percent = linear ? (m_autoCalLinearPoint + 1) * 10 : 100;
    const double maxW = (useMax && bandMax > 0.5)
        ? std::min(bandMax, kAutoCalMaxTargetW) : kAutoCalMaxTargetW;
    m_autoCalTargetWatts = linear
        ? percent * maxW / 100.0
        : maxW;
    m_autoCalHasSuggestion = false;
    m_autoCalInTolerance = false;
    m_autoCalApplyButton->setEnabled(false);

    // Snapshot the operator's drive once; repeated Measure keeps the 100 %
    // drive and the original snapshot until Apply/Clear/close restores it.
    TransmitModel& tx = radio->transmitModel();
    if (!m_autoCalSavedValid) {
        m_autoCalSavedPower = tx.power();
        m_autoCalSavedTunePower = tx.tunePowerForBand(band);
        m_autoCalSavedTuneSource = static_cast<int>(tx.tuneDrivePowerSource());
        m_autoCalSavedPowerLimit = tx.limitPowerForBand(band);
        m_autoCalSavedTuneLimit = tx.limitTunePowerForBand(band);
        m_autoCalSavedBand = band;
        m_autoCalSavedValid = true;
    }

    // A slider limit below 100 % would clamp the reading below Max W, so
    // lift both limits for the measurement and put them back afterwards.
    tx.setPowerLimit(100);
    tx.setTunePowerLimit(100);

    switch (tx.tuneDrivePowerSource()) {
    case DrivePowerSource::DriveSlider:
        tx.setPower(percent);
        break;
    case DrivePowerSource::TuneSlider:
        tx.setTunePowerForBand(band, percent);
        break;
    case DrivePowerSource::Fixed:
        // The fixed tune value cannot be driven through the slider, so use
        // the drive slider for this reading. The saved source and its fixed
        // value are restored afterwards.
        tx.setTuneDrivePowerSource(DrivePowerSource::DriveSlider);
        tx.setPower(percent);
        break;
    }
    radio->refreshDrivePower();
    // Re-assert after the drive change: a band/power path can put the stored
    // trackbar limits back, and they would clamp the reading below Max W.
    tx.setPowerLimit(100);
    tx.setTunePowerLimit(100);
    radio->refreshDrivePower();
    qCInfo(lcPaAutoCal) << "measure start: power" << tx.power()
                        << "tunePower" << tx.tunePowerForBand(band)
                        << "source" << static_cast<int>(tx.tuneDrivePowerSource())
                        << "limits" << tx.powerLimit() << tx.tunePowerLimit();

    if (!m_autoCalSettleTimer) {
        m_autoCalSettleTimer = new QTimer(this);
        m_autoCalSettleTimer->setSingleShot(true);
        connect(m_autoCalSettleTimer, &QTimer::timeout,
                this, &PaGainByBandPage::onAutoCalMeasureReady);
    }
    m_autoCalStatusLabel->setText(
        QStringLiteral("Drive at %1 %% (%2 W); settling...")
            .arg(percent)
            .arg(m_autoCalTargetWatts, 0, 'f', 1));
    m_autoCalStatusLabel->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-style: italic; }")
            .arg(Style::kTextScale));
    m_autoCalSettleTimer->start(1500);
}

void PaGainByBandPage::onAutoCalMeasureReady()
{
    const double fwd = readAutoCalForwardWatts();
    const double ref = readAutoCalReflectedWatts();
    const Band band = m_autoCalBand;
    const double target = m_autoCalTargetWatts;

    if (RadioModel* const radio = model()) {
        TransmitModel& tx = radio->transmitModel();
        qCInfo(lcPaAutoCal) << "measure drive: power" << tx.power()
                            << "tunePower" << tx.tunePowerForBand(band)
                            << "source" << static_cast<int>(tx.tuneDrivePowerSource())
                            << "limits" << tx.powerLimit() << tx.tunePowerLimit();
    }

    qCInfo(lcPaAutoCal) << "measure: band" << static_cast<int>(band)
                        << "source" << m_autoCalSource
                        << "fwd" << fwd << "ref" << ref << "target" << target;

    const auto warn = [this](const QString& text) {
        m_autoCalStatusLabel->setText(text);
        m_autoCalStatusLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-style: italic; }")
                .arg(Style::kRedBorder));
    };
    const auto info = [this](const QString& text) {
        m_autoCalStatusLabel->setText(text);
        m_autoCalStatusLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-style: italic; }")
                .arg(Style::kTextScale));
    };

    if (ref > std::max(2.0, target * 0.15) || fwd > target + 10.0) {
        warn(QStringLiteral("Refused: %1 W forward, %2 W reflected.")
                 .arg(fwd, 0, 'f', 1).arg(ref, 0, 'f', 1));
        return;
    }
    if (fwd <= 0.1) {
        warn(QStringLiteral("No forward power from the selected source."));
        return;
    }

    double tolUnder = kAutoCalUnderW;
    double tolOver = kAutoCalOverW;
    autoCalTolerance(&tolUnder, &tolOver);
    if (fwd >= target - tolUnder && fwd <= target + tolOver) {
        const QString tolText = (tolUnder == tolOver)
            ? QStringLiteral("+/- %1 W").arg(tolUnder, 0, 'f', 1)
            : QStringLiteral("-%1/+%2 W")
                  .arg(tolUnder, 0, 'f', 0).arg(tolOver, 0, 'f', 0);
        m_autoCalStatusLabel->setText(
            QStringLiteral("IN TOLERANCE: %1 W (target %2 W, %3)")
                .arg(fwd, 0, 'f', 1)
                .arg(target, 0, 'f', 0)
                .arg(tolText));
        m_autoCalStatusLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-weight: bold; "
                           "font-style: italic; }")
                .arg(Style::kGreenText));
        // Nothing to change, but Apply finishes the procedure and restores
        // the operator's drive; Clear discards it the same way.
        m_autoCalInTolerance = true;
        m_autoCalApplyButton->setEnabled(true);
        m_autoCalCancelButton->setEnabled(true);
        return;
    }

    // Adaptive step: assertive far from target, damped close to it.
    const double dbError = wattsToDbm(fwd) - wattsToDbm(target);
    const double absDb = std::fabs(dbError);
    double factor = 1.0;
    if (absDb <= 1.0) {
        factor = 0.45;
    } else if (absDb <= 3.0) {
        factor = 0.7;
    } else if (absDb <= 6.0) {
        factor = 0.9;
    }
    double stepDb = std::clamp(dbError * factor, -12.0, 12.0);
    if (std::fabs(stepDb) < 0.1) {
        stepDb = std::copysign(0.1, dbError == 0.0 ? 1.0 : dbError);
    }

    m_autoCalPendingDb = stepDb;
    m_autoCalPendingBand = band;
    m_autoCalHasSuggestion = true;
    m_autoCalInTolerance = false;
    m_autoCalApplyButton->setEnabled(true);
    m_autoCalCancelButton->setEnabled(true);

    const QString action = stepDb >= 0.0
        ? QStringLiteral("raise") : QStringLiteral("lower");
    info(QStringLiteral("%1 W at 100%%: %2 Gain by %3 dB, then Apply.")
             .arg(fwd, 0, 'f', 1)
             .arg(action)
             .arg(std::fabs(stepDb), 0, 'f', 2));
}

void PaGainByBandPage::restoreAutoCalDrive()
{
    if (!m_autoCalSavedValid) { return; }
    m_autoCalSavedValid = false;
    RadioModel* const radio = model();
    if (!radio) { return; }
    TransmitModel& tx = radio->transmitModel();
    tx.setTuneDrivePowerSource(
        static_cast<DrivePowerSource>(m_autoCalSavedTuneSource));
    tx.setTunePowerForBand(m_autoCalSavedBand, m_autoCalSavedTunePower);
    tx.setPowerForBand(m_autoCalSavedBand, m_autoCalSavedPower);
    const Band current =
        radio->txBoundSlice() ? radio->txBoundSlice()->band()
                              : m_autoCalSavedBand;
    if (current == m_autoCalSavedBand) {
        tx.setPower(m_autoCalSavedPower);
        tx.setPowerLimit(m_autoCalSavedPowerLimit);
        tx.setTunePowerLimit(m_autoCalSavedTuneLimit);
        radio->refreshDrivePower();
    }
    qCInfo(lcPaAutoCal) << "restore: band" << static_cast<int>(m_autoCalSavedBand)
                        << "power" << m_autoCalSavedPower
                        << "tune" << m_autoCalSavedTunePower
                        << "limits" << m_autoCalSavedPowerLimit
                        << m_autoCalSavedTuneLimit
                        << "readback power" << tx.power()
                        << "stored" << tx.powerForBand(m_autoCalSavedBand)
                        << "tune" << tx.tunePowerForBand(m_autoCalSavedBand)
                        << "limits" << tx.powerLimit() << tx.tunePowerLimit();
}

void PaGainByBandPage::refreshLinearHighlight()
{
    clearLinearHighlight();
    if (m_autoCalLinearPoint < 0) { return; }
    const int band = static_cast<int>(m_autoCalLinearBand);
    if (band < 0 || band >= kPaBandCount) { return; }
    const QString style = QStringLiteral(
        "QDoubleSpinBox { background: #102830; border: 1px solid #00b4d8; }");
    for (int step = 0; step < kColAdjustCount; ++step) {
        if (m_adjustSpins[band][step]) {
            m_adjustSpins[band][step]->setStyleSheet(style);
        }
    }
}

void PaGainByBandPage::clearLinearHighlight()
{
    for (int band = 0; band < kPaBandCount; ++band) {
        for (int step = 0; step < kColAdjustCount; ++step) {
            if (m_adjustSpins[band][step]) {
                m_adjustSpins[band][step]->setStyleSheet(QString());
            }
        }
    }
}

void PaGainByBandPage::refreshLinearPointButtons()
{
    if (m_linearPointButtons.isEmpty() || !m_paProfileManager) { return; }
    const PaProfile* profile = m_paProfileManager->activeProfile();
    if (!profile) { return; }
    const Band band = selectedAutoCalBand();
    double maxW = profile->getMaxPowerUse(band)
        ? static_cast<double>(profile->getMaxPower(band)) : kAutoCalMaxTargetW;
    if (maxW > kAutoCalMaxTargetW) { maxW = kAutoCalMaxTargetW; }
    if (maxW <= 0.5) { maxW = kAutoCalMaxTargetW; }
    const int count = std::min(static_cast<int>(m_linearPointButtons.size()),
                               static_cast<int>(kColAdjustCount));
    for (int step = 0; step < count; ++step) {
        const int percent = (step + 1) * 10;
        const double watts = percent * maxW / 100.0;
        if (QPushButton* button = m_linearPointButtons[step]) {
            button->setText(
                QStringLiteral("%1 %  (%2 W)")
                    .arg(percent)
                    .arg(watts, 0, 'f', 1));
        }
        if (step < m_linearAdjustSpins.size()) {
            if (QDoubleSpinBox* spin = m_linearAdjustSpins[step]) {
                m_updatingFromProfile = true;
                spin->setValue(profile->getAdjust(band, step));
                m_updatingFromProfile = false;
            }
        }
    }
    if (m_linearBaseGainSpin) {
        m_autoCalUpdatingGain = true;
        m_linearBaseGainSpin->setValue(
            profile->getGainForBand(band, /*driveValue=*/-1));
        m_autoCalUpdatingGain = false;
    }
}

void PaGainByBandPage::openLinearizationPopup()
{
    if (m_linearizeDialog) {
        m_linearizeDialog->show();
        m_linearizeDialog->raise();
        return;
    }
    if (!m_paProfileManager) { return; }
    const PaProfile* profile = m_paProfileManager->activeProfile();
    if (!profile) { return; }

    const Band band = selectedAutoCalBand();
    m_autoCalLinearBand = band;
    refreshLinearHighlight();

    auto* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(
        QStringLiteral("PA Band Linearization - %1").arg(bandLabel(band)));
    dialog->setMinimumWidth(460);
    auto* layout = new QVBoxLayout(dialog);

    auto* note = new QLabel(
        QStringLiteral("Run Auto-Calibrate Sequence for this band first.\n"
                       "Pick a 10 % point: the tune slider is set there. Then "
                       "TUNE, Measure and Apply here. Base Gain returns to the "
                       "Max W point. Clear restores the original drive."),
        dialog);
    note->setWordWrap(true);
    layout->addWidget(note);

    auto* baseRow = new QHBoxLayout;
    baseRow->addWidget(new QLabel(QStringLiteral("PA Gain (base):"), dialog));
    m_linearBaseGainSpin = new QDoubleSpinBox(dialog);
    m_linearBaseGainSpin->setRange(38.8, 100.0);
    m_linearBaseGainSpin->setSingleStep(0.1);
    m_linearBaseGainSpin->setDecimals(1);
    m_linearBaseGainSpin->setToolTip(QStringLiteral(
        "Base PA Gain for the selected band (Max W setpoint)."));
    connect(m_linearBaseGainSpin,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double value) { setAutoCalGainFromSpin(value); });
    baseRow->addWidget(m_linearBaseGainSpin);
    baseRow->addStretch();
    layout->addLayout(baseRow);

    auto* base = new QPushButton(QStringLiteral("Base Gain (Max W)"), dialog);
    connect(base, &QPushButton::clicked, this, [this] {
        selectLinearPoint(-1);
        refreshLinearPointButtons();
    });
    layout->addWidget(base);

    auto* grid = new QGridLayout;
    m_linearPointButtons.clear();
    m_linearAdjustSpins.clear();
    for (int step = 0; step < kColAdjustCount; ++step) {
        auto* box = new QWidget(dialog);
        auto* boxLayout = new QVBoxLayout(box);
        boxLayout->setContentsMargins(0, 0, 0, 0);
        boxLayout->setSpacing(2);

        auto* button = new QPushButton(box);
        connect(button, &QPushButton::clicked, this, [this, step] {
            selectLinearPoint(step);
            refreshLinearPointButtons();
        });
        boxLayout->addWidget(button);

        auto* spin = new QDoubleSpinBox(box);
        spin->setRange(-10.0, 10.0);
        spin->setSingleStep(0.1);
        spin->setDecimals(1);
        spin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
        spin->setToolTip(QStringLiteral(
            "Trim for this point, in dB. Positive adds output (the opposite "
            "of the base Gain)."));
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this, step](double value) {
            const Band band = selectedAutoCalBand();
            onAdjustChanged(band, step, value);
            // Mirror into the table cell; the dialog spin is a second view of
            // the same profile value.
            const int idx = static_cast<int>(band);
            if (idx >= 0 && idx < kPaBandCount && m_adjustSpins[idx][step]) {
                m_updatingFromProfile = true;
                m_adjustSpins[idx][step]->setValue(value);
                m_updatingFromProfile = false;
            }
        });
        boxLayout->addWidget(spin);

        m_linearPointButtons.append(button);
        m_linearAdjustSpins.append(spin);
        grid->addWidget(box, step / 3, step % 3);
    }
    layout->addLayout(grid);

    auto* status = new QLabel(QStringLiteral("Pick a point."), dialog);
    status->setWordWrap(true);
    layout->addWidget(status);

    auto* actions = new QHBoxLayout;
    auto* tune = new QPushButton(QStringLiteral("TUNE"), dialog);
    tune->setCheckable(true);
    auto* measure = new QPushButton(QStringLiteral("Measure"), dialog);
    auto* apply = new QPushButton(QStringLiteral("Apply"), dialog);
    auto* clear = new QPushButton(QStringLiteral("Clear"), dialog);
    actions->addWidget(tune);
    actions->addWidget(measure);
    actions->addWidget(apply);
    actions->addWidget(clear);
    layout->addLayout(actions);
    apply->setEnabled(false);
    clear->setEnabled(false);

    auto* close = new QPushButton(QStringLiteral("Close"), dialog);
    layout->addWidget(close);

    connect(tune, &QPushButton::toggled, this, [this](bool on) {
        if (RadioModel* const radio = model()) { radio->setTune(on); }
    });
    connect(measure, &QPushButton::clicked,
            this, &PaGainByBandPage::startAutoCal);
    connect(apply, &QPushButton::clicked, this, [this] {
        applyAutoCalSuggestion();
        refreshLinearPointButtons();
    });
    connect(clear, &QPushButton::clicked, this, [this] {
        stopAutoCal();
        refreshLinearPointButtons();
    });
    connect(close, &QPushButton::clicked, dialog, &QDialog::close);
    connect(dialog, &QDialog::finished, this, [this] {
        stopAutoCal();
        m_linearPointButtons.clear();
        m_linearAdjustSpins.clear();
        m_linearBaseGainSpin = nullptr;
        m_linearizeDialog = nullptr;
    });

    // Mirror the panel: TUNE state, status text and the Apply/Clear enables.
    auto* timer = new QTimer(dialog);
    timer->setInterval(250);
    connect(timer, &QTimer::timeout, dialog,
            [this, tune, status, apply, clear] {
        if (!m_autoCalStatusLabel) { return; }
        if (RadioModel* const radio = model()) {
            const bool on = radio->isTune();
            if (tune->isChecked() != on) {
                const QSignalBlocker block(tune);
                tune->setChecked(on);
            }
        }
        if (status->text() != m_autoCalStatusLabel->text()) {
            status->setText(m_autoCalStatusLabel->text());
        }
        status->setStyleSheet(m_autoCalStatusLabel->styleSheet());
        if (m_autoCalApplyButton) {
            apply->setEnabled(m_autoCalApplyButton->isEnabled());
        }
        if (m_autoCalCancelButton) {
            clear->setEnabled(m_autoCalCancelButton->isEnabled());
        }
        // Keep the arrow spins in step with the table (edits from either).
        if (const PaProfile* profile = m_paProfileManager
                ? m_paProfileManager->activeProfile() : nullptr) {
            const Band band = selectedAutoCalBand();
            if (m_linearBaseGainSpin && !m_linearBaseGainSpin->hasFocus()) {
                m_autoCalUpdatingGain = true;
                m_linearBaseGainSpin->setValue(
                    profile->getGainForBand(band, /*driveValue=*/-1));
                m_autoCalUpdatingGain = false;
            }
            const int count = std::min(
                static_cast<int>(m_linearAdjustSpins.size()),
                static_cast<int>(kColAdjustCount));
            for (int step = 0; step < count; ++step) {
                QDoubleSpinBox* spin = m_linearAdjustSpins[step];
                if (!spin || spin->hasFocus()) { continue; }
                m_updatingFromProfile = true;
                spin->setValue(profile->getAdjust(band, step));
                m_updatingFromProfile = false;
            }
        }
    });
    timer->start();

    m_linearizeDialog = dialog;
    refreshLinearPointButtons();
    dialog->show();
}


void PaGainByBandPage::selectLinearPoint(int step)
{
    if (!m_paProfileManager) { return; }
    const PaProfile* profile = m_paProfileManager->activeProfile();
    if (!profile) { return; }

    const Band band = selectedAutoCalBand();
    m_autoCalLinearBand = band;

    double maxW = profile->getMaxPowerUse(band)
        ? static_cast<double>(profile->getMaxPower(band)) : kAutoCalMaxTargetW;
    if (maxW > kAutoCalMaxTargetW) { maxW = kAutoCalMaxTargetW; }
    if (maxW <= 0.5) { maxW = kAutoCalMaxTargetW; }

    if (step < 0) {
        m_autoCalLinearPoint = -1;
        clearLinearHighlight();
        if (m_autoCalModeLabel) {
            m_autoCalModeLabel->setText(
                QStringLiteral("Mode: Base Gain (Max W)"));
        }
        m_autoCalTargetWatts = maxW;
        if (m_autoCalTargetLabel) {
            m_autoCalTargetLabel->setText(
                QStringLiteral("%1 W (Max W)").arg(maxW, 0, 'f', 0));
        }
        restoreAutoCalDrive();
        return;
    }

    const int percent = (step + 1) * 10;
    const double target = percent * maxW / 100.0;
    m_autoCalLinearPoint = step;
    m_autoCalTargetWatts = target;
    m_autoCalHasSuggestion = false;
    m_autoCalInTolerance = false;
    if (m_autoCalApplyButton) { m_autoCalApplyButton->setEnabled(false); }
    if (m_autoCalCancelButton) { m_autoCalCancelButton->setEnabled(false); }
    if (m_autoCalModeLabel) {
        m_autoCalModeLabel->setText(
            QStringLiteral("Mode: %1 % (%2 W) trim")
                .arg(percent).arg(target, 0, 'f', 1));
    }
    if (m_autoCalTargetLabel) {
        m_autoCalTargetLabel->setText(
            QStringLiteral("%1 W (%2 % of Max W)")
                .arg(target, 0, 'f', 1).arg(percent));
    }
    refreshLinearHighlight();

    RadioModel* const radio = model();
    if (!radio) { return; }
    TransmitModel& tx = radio->transmitModel();
    if (!m_autoCalSavedValid) {
        m_autoCalSavedPower = tx.power();
        m_autoCalSavedTunePower = tx.tunePowerForBand(band);
        m_autoCalSavedTuneSource = static_cast<int>(tx.tuneDrivePowerSource());
        m_autoCalSavedPowerLimit = tx.limitPowerForBand(band);
        m_autoCalSavedTuneLimit = tx.limitTunePowerForBand(band);
        m_autoCalSavedBand = band;
        m_autoCalSavedValid = true;
    }
    tx.setPowerLimit(100);
    tx.setTunePowerLimit(100);
    switch (tx.tuneDrivePowerSource()) {
    case DrivePowerSource::DriveSlider:
        tx.setPower(percent);
        break;
    case DrivePowerSource::TuneSlider:
        tx.setTunePowerForBand(band, percent);
        break;
    case DrivePowerSource::Fixed:
        tx.setTuneDrivePowerSource(DrivePowerSource::DriveSlider);
        tx.setPower(percent);
        break;
    }
    radio->refreshDrivePower();
    if (m_autoCalStatusLabel) {
        m_autoCalStatusLabel->setText(
            QStringLiteral("Point set: %1 % (%2 W). Key TUNE, then Measure.")
                .arg(percent).arg(target, 0, 'f', 1));
        m_autoCalStatusLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-style: italic; }")
                .arg(Style::kTextScale));
    }
}

void PaGainByBandPage::applyAutoCalGain(double dbError, Band band)
{
    if (!m_paProfileManager) { return; }
    const PaProfile* active = m_paProfileManager->activeProfile();
    if (!active) { return; }

    const double current = active->getGainForBand(band, /*driveValue=*/-1);
    const double next =
        std::clamp(current + dbError, kAutoCalGainMin, kAutoCalGainMax);

    PaProfile mutated = *active;
    mutated.setGainForBand(band, static_cast<float>(next));
    m_paProfileManager->saveProfile(active->name(), mutated);
    qCInfo(lcPaAutoCal) << "gain:" << current << "->" << next
                        << "dbErr" << dbError << "band" << static_cast<int>(band);

    if (m_autoCalGainSpin) {
        m_autoCalUpdatingGain = true;
        m_autoCalGainSpin->setValue(next);
        m_autoCalUpdatingGain = false;
    }

    const int idx = static_cast<int>(band);
    if (idx >= 0 && idx < kPaBandCount && m_gainSpins[idx]) {
        m_updatingFromProfile = true;
        m_gainSpins[idx]->setValue(next);
        m_updatingFromProfile = false;
    }
    if (RadioModel* const radio = model()) { radio->refreshDrivePower(); }
}

void PaGainByBandPage::autoCalTolerance(double* under, double* over) const
{
    const int mode = m_autoCalToleranceCombo
        ? m_autoCalToleranceCombo->currentData().toInt() : 0;
    switch (mode) {
    case 1:  *under = 1.0; *over = 1.0; break;
    case 2:  *under = 2.0; *over = 2.0; break;
    case 3:  *under = 5.0; *over = 5.0; break;
    default: *under = kAutoCalUnderW; *over = kAutoCalOverW; break;
    }
}

void PaGainByBandPage::setAutoCalGainFromSpin(double gainDb)
{
    if (m_autoCalUpdatingGain || !m_paProfileManager) { return; }
    const PaProfile* active = m_paProfileManager->activeProfile();
    if (!active) { return; }

    const Band band = selectedAutoCalBand();
    const double clamped =
        std::clamp(gainDb, kAutoCalGainMin, kAutoCalGainMax);

    PaProfile mutated = *active;
    mutated.setGainForBand(band, static_cast<float>(clamped));
    m_paProfileManager->saveProfile(active->name(), mutated);
    qCInfo(lcPaAutoCal) << "manual gain:" << clamped
                        << "band" << static_cast<int>(band);

    const int idx = static_cast<int>(band);
    if (idx >= 0 && idx < kPaBandCount && m_gainSpins[idx]) {
        m_updatingFromProfile = true;
        m_gainSpins[idx]->setValue(clamped);
        m_updatingFromProfile = false;
    }
    if (RadioModel* const radio = model()) { radio->refreshDrivePower(); }
    if (m_autoCalStatusLabel) {
        m_autoCalStatusLabel->setText(
            QStringLiteral("PA Gain %1 dB for %2 (Max W setpoint).")
                .arg(clamped, 0, 'f', 1)
                .arg(bandLabel(band)));
        m_autoCalStatusLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-style: italic; }")
                .arg(Style::kTextScale));
    }
}

void PaGainByBandPage::onAutoCalWatchdog()
{
    RadioModel* const radio = model();
    if (!radio) { return; }

    // Follow the radio's TX band even when a memory recall retunes the slice
    // without a band-button event. Restores a pending measurement first.
    if (m_autoCalPanel && m_autoCalPanel->isVisible() && m_autoCalBandCombo) {
        if (SliceModel* const tx = radio->txBoundSlice()) {
            const int idx = m_autoCalBandCombo->findData(
                static_cast<int>(tx->band()));
            if (idx >= 0 && m_autoCalBandCombo->currentIndex() != idx) {
                if (m_linearizeDialog) { m_linearizeDialog->close(); }
                stopAutoCal();
                const QSignalBlocker block(m_autoCalBandCombo);
                m_autoCalBandCombo->setCurrentIndex(idx);
                updateAutoCalTargets();
            }
        }
    }

    const bool tune = radio->isTune();
    const bool mox = radio->transmitModel().isMox();

    // Keep the panel's TUNE button in step with the real key state (the main
    // TUNE button and the watchdog's own unkey both change it).
    if (m_autoCalTuneButton && m_autoCalTuneButton->isChecked() != tune) {
        m_autoCalTuneUpdating = true;
        m_autoCalTuneButton->setChecked(tune);
        m_autoCalTuneUpdating = false;
    }

    if (!tune && !mox) {
        m_autoCalKeyClock.invalidate();
        if (m_autoCalCountdownLabel) {
            m_autoCalCountdownLabel->setText(QStringLiteral("Key time: --"));
        }
        return;
    }
    if (!m_autoCalKeyClock.isValid()) { m_autoCalKeyClock.start(); }

    const int limitSec = m_autoCalTimeoutCombo
        ? m_autoCalTimeoutCombo->currentData().toInt() : 10;
    if (m_autoCalCountdownLabel) {
        m_autoCalCountdownLabel->setText(
            QStringLiteral("Key time: %1 s / %2 s")
                .arg(m_autoCalKeyClock.elapsed() / 1000.0, 0, 'f', 1)
                .arg(limitSec));
    }
    const double fwd = readAutoCalForwardWatts();

    // Live ceiling for the band being transmitted, not the panel's last
    // target: a stale 25 W target must not cap a legitimate 50 W tune.
    double bandMax = 0.0;
    bool useMax = false;
    if (m_paProfileManager) {
        if (const PaProfile* profile = m_paProfileManager->activeProfile()) {
            Band band = m_autoCalBand;
            if (SliceModel* const tx = radio->txBoundSlice()) {
                band = tx->band();
            }
            useMax = profile->getMaxPowerUse(band);
            bandMax = static_cast<double>(profile->getMaxPower(band));
        }
    }
    const double cap = (useMax && bandMax > 0.5)
        ? std::min(100.0, bandMax + 10.0)
        : 100.0;

    QString reason;
    if (fwd > cap) {
        reason = QStringLiteral("Safety unkey: %1 W (cap %2 W).")
                     .arg(fwd, 0, 'f', 1).arg(cap, 0, 'f', 0);
    } else if (m_autoCalKeyClock.hasExpired(limitSec * 1000)) {
        reason = QStringLiteral("Safety unkey: keyed over %1 s.").arg(limitSec);
    }
    if (reason.isEmpty()) { return; }

    if (tune) { radio->setTune(false); }
    if (mox) { radio->transmitModel().setMox(false); }
    m_autoCalKeyClock.invalidate();
    restoreAutoCalDrive();
    if (m_autoCalStatusLabel) {
        m_autoCalStatusLabel->setText(reason);
        m_autoCalStatusLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-style: italic; }")
                .arg(Style::kRedBorder));
    }
    qCWarning(lcPaAutoCal) << reason;
}

void PaGainByBandPage::applyAutoCalSuggestion()
{
    const bool linear = m_autoCalLinearPoint >= 0;

    if (m_autoCalInTolerance) {
        m_autoCalInTolerance = false;
        m_autoCalHasSuggestion = false;
        if (m_autoCalApplyButton) { m_autoCalApplyButton->setEnabled(false); }
        if (m_autoCalCancelButton) { m_autoCalCancelButton->setEnabled(false); }
        if (linear) {
            // Keep the drive at the point so the next point or a re-measure
            // is one click away; Clear/Base Gain restores the original.
            if (m_autoCalStatusLabel) {
                m_autoCalStatusLabel->setText(QStringLiteral(
                    "In tolerance: trim unchanged. Pick another point or Clear."));
                m_autoCalStatusLabel->setStyleSheet(
                    QStringLiteral("QLabel { color: %1; font-style: italic; }")
                        .arg(Style::kGreenText));
            }
            return;
        }
        // Base Gain: nothing to change; restore the drive.
        restoreAutoCalDrive();
        if (m_autoCalStatusLabel) {
            m_autoCalStatusLabel->setText(
                QStringLiteral("In tolerance: Gain unchanged; drive restored."));
            m_autoCalStatusLabel->setStyleSheet(
                QStringLiteral("QLabel { color: %1; font-style: italic; }")
                    .arg(Style::kGreenText));
        }
        return;
    }
    if (!m_autoCalHasSuggestion) { return; }

    if (linear && m_paProfileManager) {
        // Adjust is subtracted from Gain, so the sign is the opposite of the
        // base-Gain step: too much output lowers the trim.
        const int step = m_autoCalLinearPoint;
        const Band band = m_autoCalPendingBand;
        if (const PaProfile* active = m_paProfileManager->activeProfile()) {
            const double current = active->getAdjust(band, step);
            const double next = std::clamp(
                current - m_autoCalPendingDb, -10.0, 10.0);
            PaProfile mutated = *active;
            mutated.setAdjust(band, step, static_cast<float>(next));
            m_paProfileManager->saveProfile(active->name(), mutated);
            const int bandIdx = static_cast<int>(band);
            if (bandIdx >= 0 && bandIdx < kPaBandCount
                && m_adjustSpins[bandIdx][step]) {
                m_updatingFromProfile = true;
                m_adjustSpins[bandIdx][step]->setValue(next);
                m_updatingFromProfile = false;
            }
            if (RadioModel* const radio = model()) {
                radio->refreshDrivePower();
            }
            if (m_autoCalStatusLabel) {
                m_autoCalStatusLabel->setText(
                    QStringLiteral("Applied %1 dB to the %2 % trim. "
                                   "Measure again or pick another point.")
                        .arg(next - current, 0, 'f', 2)
                        .arg((step + 1) * 10));
                m_autoCalStatusLabel->setStyleSheet(
                    QStringLiteral("QLabel { color: %1; font-style: italic; }")
                        .arg(Style::kTextScale));
            }
        }
        m_autoCalHasSuggestion = false;
        if (m_autoCalApplyButton) { m_autoCalApplyButton->setEnabled(false); }
        if (m_autoCalCancelButton) { m_autoCalCancelButton->setEnabled(false); }
        return;
    }

    applyAutoCalGain(m_autoCalPendingDb, m_autoCalPendingBand);
    restoreAutoCalDrive();
    m_autoCalHasSuggestion = false;
    if (m_autoCalApplyButton) { m_autoCalApplyButton->setEnabled(false); }
    if (m_autoCalCancelButton) { m_autoCalCancelButton->setEnabled(false); }
    if (m_autoCalStatusLabel) {
        m_autoCalStatusLabel->setText(
            QStringLiteral("Applied %1 dB to %2. Measure again.")
                .arg(m_autoCalPendingDb, 0, 'f', 2)
                .arg(bandLabel(m_autoCalPendingBand)));
        m_autoCalStatusLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-style: italic; }")
                .arg(Style::kTextScale));
    }
}

void PaGainByBandPage::stopAutoCal()
{
    // "Clear": drop the suggestion and leave linearization, restoring the
    // band's original drive/limits. Nothing is keyed, so nothing to unkey.
    restoreAutoCalDrive();
    m_autoCalHasSuggestion = false;
    m_autoCalInTolerance = false;
    m_autoCalPendingDb = 0.0;
    m_autoCalLinearPoint = -1;
    clearLinearHighlight();
    if (m_autoCalModeLabel) {
        m_autoCalModeLabel->setText(QStringLiteral("Mode: Base Gain (Max W)"));
    }
    if (m_autoCalApplyButton) { m_autoCalApplyButton->setEnabled(false); }
    if (m_autoCalCancelButton) { m_autoCalCancelButton->setEnabled(false); }
    updateAutoCalTargets();
}

void PaGainByBandPage::refreshAutoCalUi()
{
    // The sequence updates its own labels; this keeps the page's UI seam
    // coherent when called while idle.
    if (!m_autoCalStatusLabel) { return; }
    if (!m_autoCalRunning) { updateAutoCalTargets(); }
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

void PaGainByBandPage::hideEvent(QHideEvent* event)
{
    if (m_linearizeDialog) { m_linearizeDialog->close(); }
    // Setup closing (or this page hidden) must not leave a measurement's
    // 100 % drive or lifted limits behind.
    stopAutoCal();
    if (m_autoCalTuneButton && m_autoCalTuneButton->isChecked()) {
        if (RadioModel* const radio = model()) { radio->setTune(false); }
        m_autoCalTuneUpdating = true;
        m_autoCalTuneButton->setChecked(false);
        m_autoCalTuneUpdating = false;
    }
    QWidget::hideEvent(event);
}

PaGainByBandPage::~PaGainByBandPage()
{
    // Never leave a carrier on or a measurement drive behind.
    if (m_autoCalSettleTimer) { m_autoCalSettleTimer->stop(); }
    restoreAutoCalDrive();
    if (m_autoCalRunning) {
        if (RadioModel* const radio = model()) {
            radio->setTune(false);
        }
        m_autoCalRunning = false;
    }
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
                                         QStringLiteral("0.0 \u00B0C"), paGroup);
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
