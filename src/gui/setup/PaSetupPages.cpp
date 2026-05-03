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

#include "core/CalibrationController.h"
#include "core/PaCalProfile.h"
#include "core/RadioConnection.h"
#include "core/RadioStatus.h"
#include "gui/setup/hardware/PaCalibrationGroup.h"
#include "gui/widgets/MetricLabel.h"
#include "models/RadioModel.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
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
// (PaCalibrationGroup); Phase 3+ adds the Show PA Values toggle.
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
        return;
    }

    auto* calCtrl = &model->calibrationControllerMutable();
    m_paCalGroup = new PaCalibrationGroup(this);

    // Insert before the trailing stretch SetupPage::init() appended so the
    // group renders flush with the page top — matches PaGainByBandPage above.
    contentLayout()->insertWidget(contentLayout()->count() - 1, m_paCalGroup);

    // Initial populate from the controller's current profile (None hides the
    // group; live profile change rebuilds via the lambda below).
    m_paCalGroup->populate(calCtrl, calCtrl->paCalProfile().boardClass);

    // Repopulate on radio swap / first connect — board class can flip from
    // None → Anan10/100/8000 once RadioModel seeds m_paCalProfile from the
    // hardware profile.  Mirrors the connect block previously living in
    // CalibrationTab::CalibrationTab (Section 3.3 of the P1 full-parity epic).
    connect(calCtrl, &CalibrationController::paCalProfileChanged,
            this, [this, calCtrl]() {
        if (m_paCalGroup) {
            m_paCalGroup->populate(calCtrl, calCtrl->paCalProfile().boardClass);
        }
    });
}

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
// Skipped for MVP (TODO):
//   * textFwdVoltage / textRevVoltage — per-stage RF voltage readouts.
//     Thetis derives these from raw ADC via per-board scale curves
//     (computeAlexFwdPower / computeAlexRevPower).  Wiring needs a public
//     scaleFwdPowerWatts utility — today private to RadioModel.cpp — so we
//     defer until that helper is exposed.
//   * btnResetPAValues — peak/min tracking reset.  Phase 4 only surfaces the
//     instantaneous values; peak tracking is queued for the future
//     PaPeakTracker model.
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
    // Calibrated FWD (post-CalibratedPAPower) + REV + SWR.
    // The "Forward (raw)" placeholder is omitted for MVP — see TODO above.
    auto* powerGroup  = new QGroupBox(QStringLiteral("Power"), this);
    auto* powerForm   = new QFormLayout(powerGroup);
    m_fwdCalibratedLabel = new MetricLabel(QStringLiteral("FWD (cal)"),
                                           QStringLiteral("0.00 W"), powerGroup);
    m_revPowerLabel      = new MetricLabel(QStringLiteral("REV"),
                                           QStringLiteral("0.00 W"), powerGroup);
    m_swrLabel           = new MetricLabel(QStringLiteral("SWR"),
                                           QStringLiteral("1.00"), powerGroup);
    powerForm->addRow(QStringLiteral("Forward (calibrated):"), m_fwdCalibratedLabel);
    powerForm->addRow(QStringLiteral("Reflected:"),            m_revPowerLabel);
    powerForm->addRow(QStringLiteral("SWR:"),                  m_swrLabel);
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
    m_adcOverloadLabel = new MetricLabel(QStringLiteral("ADC OVF"),
                                         QStringLiteral("No"), paGroup);
    paForm->addRow(QStringLiteral("PA Current:"),     m_paCurrentLabel);
    paForm->addRow(QStringLiteral("PA Temperature:"), m_paTempLabel);
    paForm->addRow(QStringLiteral("Supply Voltage:"), m_supplyVoltsLabel);
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

    // ── RadioStatus subscriptions ─────────────────────────────────────────
    // RadioStatus aggregates fwd/rev/swr into a single powerChanged signal,
    // so we subscribe once and update all three labels in the slot.
    RadioStatus& rs = model->radioStatus();

    connect(&rs, &RadioStatus::powerChanged, this,
            [this](double fwdW, double revW, double swr) {
                if (m_fwdCalibratedLabel) {
                    m_fwdCalibratedLabel->setValue(
                        QString::number(fwdW, 'f', 2) + QStringLiteral(" W"));
                }
                if (m_revPowerLabel) {
                    m_revPowerLabel->setValue(
                        QString::number(revW, 'f', 2) + QStringLiteral(" W"));
                }
                if (m_swrLabel) {
                    m_swrLabel->setValue(QString::number(swr, 'f', 2));
                }
            });

    connect(&rs, &RadioStatus::paCurrentChanged, this,
            [this](double amps) {
                if (m_paCurrentLabel) {
                    m_paCurrentLabel->setValue(
                        QString::number(amps, 'f', 2) + QStringLiteral(" A"));
                }
            });

    connect(&rs, &RadioStatus::paTemperatureChanged, this,
            [this](double celsius) {
                if (m_paTempLabel) {
                    m_paTempLabel->setValue(
                        QString::number(celsius, 'f', 1)
                        + QStringLiteral(" \xC2\xB0""C"));
                }
            });

    // Populate from current state so the page renders meaningful values
    // even when no signal has fired yet (e.g. opened mid-session after
    // telemetry has already settled).
    m_fwdCalibratedLabel->setValue(
        QString::number(rs.forwardPowerWatts(), 'f', 2) + QStringLiteral(" W"));
    m_revPowerLabel->setValue(
        QString::number(rs.reflectedPowerWatts(), 'f', 2) + QStringLiteral(" W"));
    m_swrLabel->setValue(QString::number(rs.swrRatio(), 'f', 2));
    m_paCurrentLabel->setValue(
        QString::number(rs.paCurrentAmps(), 'f', 2) + QStringLiteral(" A"));
    m_paTempLabel->setValue(
        QString::number(rs.paTemperatureCelsius(), 'f', 1)
        + QStringLiteral(" \xC2\xB0""C"));

    // ── RadioConnection subscriptions ─────────────────────────────────────
    // Connection may be null at construction time (no active radio); the
    // capture-by-this lambdas + Qt auto-disconnect will keep us safe even if
    // the connection gets replaced later (PaSetupPages reconstructs each
    // time SetupDialog opens, so a stale page will be destroyed first).
    auto* conn = model->connection();
    if (conn) {
        connect(conn, &RadioConnection::supplyVoltsChanged, this,
                [this](float volts) {
                    if (m_supplyVoltsLabel) {
                        m_supplyVoltsLabel->setValue(
                            QString::number(volts, 'f', 1) + QStringLiteral(" V"));
                    }
                });

        connect(conn, &RadioConnection::paTelemetryUpdated, this,
                [this](quint16 fwdRaw, quint16 revRaw,
                       quint16 /*exciterRaw*/, quint16 /*userAdc0*/,
                       quint16 /*userAdc1*/,   quint16 /*supply*/) {
                    if (m_fwdAdcLabel) {
                        m_fwdAdcLabel->setValue(QString::number(fwdRaw));
                    }
                    if (m_revAdcLabel) {
                        m_revAdcLabel->setValue(QString::number(revRaw));
                    }
                    // TODO(future): expose scaleFwdPowerWatts as a public
                    // utility so we can show "Forward (raw): N.NN W" alongside
                    // the calibrated value.  See Thetis console.cs
                    // computeAlexFwdPower for the per-board formulas
                    // (setup.designer.cs:51155-51177 [v2.10.3.13]).
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
#endif

} // namespace NereusSDR
