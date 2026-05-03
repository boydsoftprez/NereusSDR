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
#include "core/PaTelemetryScaling.h"
#include "core/RadioConnection.h"
#include "core/RadioStatus.h"
#include "gui/setup/hardware/PaCalibrationGroup.h"
#include "gui/widgets/MetricLabel.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>

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
// + chkAutoPACalibrate (setup.designer.cs:49084 [v2.10.3.13]).  The full port
// (per-band gain spinboxes, profile lifecycle buttons, auto-cal sweep panel)
// lands in Phase 3M-3.
PaGainByBandPage::PaGainByBandPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("PA Gain"), model, parent)
{
    // Placeholder copy intentionally enumerates the future scope so a user
    // landing here pre-Phase-3M-3 understands what's coming.  Phrasing is
    // NereusSDR-original — the Thetis tab is widget-only with no narrative
    // text — and may be tweaked freely; the test only pins on the "3M-3"
    // future-phase tag.
    // Placeholder text deliberately uses ASCII-only punctuation: the
    // disabled-italic font fallback on some macOS system-font versions
    // renders Unicode bullets (U+2022) and em-dashes (U+2014) as tofu
    // boxes despite identical chars rendering correctly in normal-state
    // labels elsewhere in the app.  Verified at PR-time on JJ's bench.
    auto* lbl = buildPlaceholderLabel(QStringLiteral(
        "PA Gain by Band -- coming in Phase 3M-3.\n"
        "\n"
        "This page will host:\n"
        "  - Per-band gain spinboxes (11 HF + 14 VHF bands)\n"
        "  - PA Profile selector + new/copy/delete/reset buttons\n"
        "  - Auto-cal sweep panel (target watts, all-bands/selected-bands,\n"
        "    11 per-band include checkboxes, \"Use Advanced Calibration Routine\")\n"
        "  - TX-Profile / PA-Profile recovery linkage warning\n"
        "\n"
        "Source: Thetis setup.designer.cs:47386-47525 [v2.10.3.13]\n"
        "        + chkAutoPACalibrate at line 49084"));
    // SetupPage::init() already appended a trailing stretch; insert the
    // placeholder before it so the label renders flush with the page top.
    contentLayout()->insertWidget(contentLayout()->count() - 1, lbl);
}

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
