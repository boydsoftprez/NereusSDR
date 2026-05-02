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

#include <QLabel>
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
    auto* lbl = buildPlaceholderLabel(QStringLiteral(
        "PA Gain by Band — coming in Phase 3M-3.\n"
        "\n"
        "This page will host:\n"
        "  \xE2\x80\xA2 Per-band gain spinboxes (11 HF + 14 VHF bands)\n"
        "  \xE2\x80\xA2 PA Profile selector + new/copy/delete/reset buttons\n"
        "  \xE2\x80\xA2 Auto-cal sweep panel (target watts, all-bands/selected-bands,\n"
        "    11 per-band include checkboxes, \"Use Advanced Calibration Routine\")\n"
        "  \xE2\x80\xA2 TX-Profile / PA-Profile recovery linkage warning\n"
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
// Phase 3 migrates PaCalibrationGroup + Show PA Values toggle into this
// page from the existing Hardware → Calibration tab.
PaWattMeterPage::PaWattMeterPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Watt Meter"), model, parent)
{
    auto* lbl = buildPlaceholderLabel(QStringLiteral(
        "Watt Meter — content lands in Phase 3 (cal spinboxes + Show PA Values panel).\n"
        "\n"
        "Source: Thetis setup.designer.cs:49304-49309 [v2.10.3.13]"));
    contentLayout()->insertWidget(contentLayout()->count() - 1, lbl);
}

// ── PaValuesPage ─────────────────────────────────────────────────────────────
//
// NereusSDR-spin: Thetis embeds panelPAValues (setup.designer.cs:51155-51177
// [v2.10.3.13]) inside its Watt Meter tab.  We promote it to a dedicated page
// so live telemetry can be enriched without crowding the cal-point editor.
// Phase 4 wires the labels to RadioStatus signals.
PaValuesPage::PaValuesPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("PA Values"), model, parent)
{
    auto* lbl = buildPlaceholderLabel(QStringLiteral(
        "PA Values (NereusSDR-spin) — Phase 4 wires the live telemetry display.\n"
        "\n"
        "Will surface FWD power (raw + calibrated), REV power, SWR, PA current,\n"
        "PA voltage, PA temperature, ADC overload state, drive byte — bound to\n"
        "RadioStatus signals.\n"
        "\n"
        "Source: Thetis panelPAValues setup.designer.cs:51155-51177 [v2.10.3.13]\n"
        "        (richer NereusSDR variant — extra fields beyond Thetis baseline)"));
    contentLayout()->insertWidget(contentLayout()->count() - 1, lbl);
}

} // namespace NereusSDR
