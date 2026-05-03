#pragma once

// =================================================================
// src/gui/setup/PaSetupPages.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// Setup IA reshape Phase 2 — top-level "PA" category placeholder pages.
// Mirrors Thetis tpPowerAmplifier (setup.designer.cs:47366-47371
// [v2.10.3.13]) which hosts a tab control with two sub-tabs:
//   - tpGainByBand → PaGainByBandPage (placeholder for Phase 3M-3)
//   - tpWattMeter  → PaWattMeterPage  (placeholder for Phase 3)
// NereusSDR adds a third NereusSDR-spin page (PaValuesPage) for a richer
// always-visible diagnostic readout — Thetis ships its panelPAValues block
// (setup.designer.cs:51155-51177 [v2.10.3.13]) inside the Watt Meter tab.
//
// Phase 2 ships these as placeholders only.  Phase 3+ migrates the live
// PaCalibrationGroup + Show PA Values panel into PaWattMeterPage; Phase
// 3M-3 wires the per-band gain table + auto-cal sweep into PaGainByBandPage;
// Phase 4 binds PaValuesPage to RadioStatus signals.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-02 — Original implementation.  PA top-level category for
//                 Setup IA reshape Phase 2.  AI-assisted transformation
//                 via Anthropic Claude Code.
//   2026-05-02 — Phase 4: PaValuesPage gains MetricLabel members + test
//                 accessors guarded by NEREUS_BUILD_TESTS.  AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-05-03 — Phase 5 Agent 5A of #167: PaWattMeterPage gains the
//                 missing Thetis controls — chkPAValues "Show PA Values
//                 page" checkbox (persists "display/showPaValuesPage"
//                 visibility hint to AppSettings) + btnResetPAValues
//                 "Reset PA Values" button (emits resetPaValuesRequested
//                 for PaValuesPage / Phase 5B peak-min reset).
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-05-03 — Phase 5 Agent 5B of issue #167 PA calibration safety
//                 hotfix.  PaValuesPage closes the panelPAValues 4-label
//                 gap (Raw FWD power, Drive, FWD voltage, REV voltage)
//                 via PaTelemetryScaling (Phase 1B), adds running
//                 peak/min tracking on six telemetry values (FWD
//                 calibrated / REV / SWR / PA current / PA temperature /
//                 supply volts), exposes a Reset button, and a public
//                 resetPaValues() slot intended to be cross-page-wired
//                 to PaWattMeterPage::resetPaValuesRequested (Phase 5A).
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

#include "gui/SetupPage.h"

#include <QString>
#include <limits>

class QPushButton;

class QCheckBox;
class QPushButton;

namespace NereusSDR {

class PaCalibrationGroup;
class MetricLabel;

// ---------------------------------------------------------------------------
// PA > PA Gain
// Mirrors Thetis tpGainByBand (setup.designer.cs:47386-47525 [v2.10.3.13]).
// Phase 2 placeholder — Phase 3M-3 wires per-band gain table + auto-cal sweep.
// ---------------------------------------------------------------------------
class PaGainByBandPage : public SetupPage {
    Q_OBJECT
public:
    explicit PaGainByBandPage(RadioModel* model, QWidget* parent = nullptr);
};

// ---------------------------------------------------------------------------
// PA > Watt Meter
// Mirrors Thetis tpWattMeter (setup.designer.cs:49304-49309 [v2.10.3.13]).
// Phase 3A migrates the per-board PA forward-power cal-point spinbox group
// (PaCalibrationGroup) here from the Hardware → Calibration tab; the group
// is rebuilt on `CalibrationController::paCalProfileChanged` (radio swap).
//
// Phase 5 Agent 5A of #167 adds the two missing Thetis controls from
// panelPAValues' surrounding chrome:
//   * chkPAValues  — "Show PA Values page" checkbox.  In Thetis this gates
//                    the embedded panelPAValues block's visibility
//                    (setup.cs:16340-16343 chkPAValues_CheckedChanged
//                    [v2.10.3.13]).  In NereusSDR the readout block is
//                    promoted to a dedicated PaValuesPage, so the toggle
//                    persists a visibility hint to AppSettings under
//                    "display/showPaValuesPage" (default "True").  The
//                    SetupDialog navigation (or PaValuesPage itself) honors
//                    the preference.
//   * btnResetPAValues — "Reset PA Values" button.  Emits
//                    resetPaValuesRequested() so PaValuesPage (Phase 5B)
//                    can subscribe and reset its peak/min tracking state.
//                    Thetis btnResetPAValues_Click at setup.cs:16346-16357
//                    [v2.10.3.13] blanks the readout text fields directly;
//                    NereusSDR splits the controller (this page) from the
//                    state owner (PaValuesPage) via a signal.
// ---------------------------------------------------------------------------
class PaWattMeterPage : public SetupPage {
    Q_OBJECT
public:
    explicit PaWattMeterPage(RadioModel* model, QWidget* parent = nullptr);

#ifdef NEREUS_BUILD_TESTS
    bool showPaValuesCheckedForTest() const;
    void clickResetPaValuesForTest();
#endif

signals:
    /// Emitted when the user clicks "Reset PA Values".  Phase 5B's
    /// PaValuesPage subscribes and resets its peak/min state.
    /// From Thetis btnResetPAValues_Click handler at setup.cs:16346-16357
    /// [v2.10.3.13].
    void resetPaValuesRequested();

private:
    PaCalibrationGroup* m_paCalGroup{nullptr};
    QCheckBox*          m_showPaValuesCheck{nullptr};
    QPushButton*        m_resetPaValuesButton{nullptr};
};

// ---------------------------------------------------------------------------
// PA > PA Values  (NereusSDR-spin)
// No direct Thetis page-level equivalent — Thetis ships panelPAValues
// (setup.designer.cs:51155-51177 [v2.10.3.13]) embedded inside the Watt
// Meter tab.  NereusSDR promotes this readout to a dedicated page so the
// live telemetry can be enriched (raw + calibrated FWD/REV, SWR, PA current,
// PA voltage, PA temperature, ADC overload state, drive byte) without
// crowding the cal-point editor.  Phase 4 binds this page to RadioStatus
// (forward / reflected / SWR / PA current / PA temperature) and to
// RadioConnection (supply volts / raw FWD-REV ADC counts / ADC overload).
//
// Phase 5B (#167 PA-cal hotfix) closes the four label-gaps Phase 4 deferred:
//   * m_fwdRawLabel      — raw FWD power, scaleFwdPowerWatts (Phase 1B)
//   * m_driveLabel       — drive power from TransmitModel slider
//   * m_fwdVoltageLabel  — FWD voltage,  scaleFwdRevVoltage  (Phase 1B)
//   * m_revVoltageLabel  — REV voltage,  scaleFwdRevVoltage  (Phase 1B,
//                          uses the FWD-side curve per the API design
//                          note in PaTelemetryScaling.h — REV-side curve
//                          difference is below the f2 UI display
//                          resolution).
// Plus running peak/min tracking on six values (FWD calibrated / REV / SWR /
// PA current / PA temperature / supply volts), an in-page Reset button, and
// a `resetPaValues()` public slot that SetupDialog (or the future Phase 5A
// PaWattMeterPage::resetPaValuesRequested signal) can connect to so the
// reset action is shared between sibling pages.
// ---------------------------------------------------------------------------
class PaValuesPage : public SetupPage {
    Q_OBJECT
public:
    explicit PaValuesPage(RadioModel* model, QWidget* parent = nullptr);

public slots:
    /// Reset peak/min tracking on all six tracked telemetry labels back to
    /// their current value.  Connected to the in-page Reset button and
    /// intended to be cross-wired to PaWattMeterPage's
    /// resetPaValuesRequested signal (Phase 5A) by SetupDialog so a single
    /// reset action clears tracked peaks on both sibling pages.
    ///
    /// From Thetis btnResetPAValues_Click setup.cs:16346-16357 [v2.10.3.13]
    /// — Thetis clears the textbox text strings; NereusSDR's spin tracks
    /// running peak/min and resets those to current.
    void resetPaValues();

#ifdef NEREUS_BUILD_TESTS
    QString fwdCalibratedTextForTest() const;
    QString revPowerTextForTest()      const;
    QString swrTextForTest()           const;
    QString paCurrentTextForTest()     const;
    QString paTempTextForTest()        const;
    QString supplyVoltsTextForTest()   const;
    QString fwdAdcTextForTest()        const;
    QString revAdcTextForTest()        const;
    QString adcOverloadTextForTest()   const;

    // Phase 5B (#167) gap-fill accessors.
    QString fwdRawTextForTest()        const;
    QString driveTextForTest()         const;
    QString fwdVoltageTextForTest()    const;
    QString revVoltageTextForTest()    const;
    QString fwdCalibratedPeakForTest() const;
    QString fwdCalibratedMinForTest()  const;
    void    clickResetForTest();
#endif

private:
    // ── Existing labels (Phase 4) ────────────────────────────────────────
    MetricLabel* m_fwdCalibratedLabel{nullptr};
    MetricLabel* m_revPowerLabel{nullptr};
    MetricLabel* m_swrLabel{nullptr};
    MetricLabel* m_paCurrentLabel{nullptr};
    MetricLabel* m_paTempLabel{nullptr};
    MetricLabel* m_supplyVoltsLabel{nullptr};
    MetricLabel* m_adcOverloadLabel{nullptr};
    MetricLabel* m_fwdAdcLabel{nullptr};
    MetricLabel* m_revAdcLabel{nullptr};

    // ── Phase 5B (#167) gap-fill labels + reset button ───────────────────
    MetricLabel* m_fwdRawLabel{nullptr};       // Raw FWD power (W)
    MetricLabel* m_driveLabel{nullptr};        // Drive power slider value
    MetricLabel* m_fwdVoltageLabel{nullptr};   // FWD RF voltage (V)
    MetricLabel* m_revVoltageLabel{nullptr};   // REV RF voltage (V)
    QPushButton* m_resetButton{nullptr};

    // ── Peak/min running tracking state ──────────────────────────────────
    // Six values benefit from peak/min — the calibrated power triplet, PA
    // current, PA temperature, and supply volts.  Raw ADC counts and
    // voltages are skipped (they're noisy and the user wants the live
    // value, not the historical extreme).  Drive is also skipped — it's
    // a slider position, not a measurement.
    struct PeakMin {
        double peak{-std::numeric_limits<double>::infinity()};
        double min { std::numeric_limits<double>::infinity()};

        void update(double v) noexcept {
            if (v > peak) { peak = v; }
            if (v < min)  { min  = v; }
        }
        void reset(double current) noexcept {
            peak = current;
            min  = current;
        }
        bool valid() const noexcept {
            return peak != -std::numeric_limits<double>::infinity()
                && min  !=  std::numeric_limits<double>::infinity();
        }
    };

    PeakMin m_fwdPeakMin;       ///< calibrated FWD watts
    PeakMin m_revPeakMin;       ///< REV watts
    PeakMin m_swrPeakMin;       ///< SWR ratio
    PeakMin m_paCurrentPeakMin; ///< amps
    PeakMin m_paTempPeakMin;    ///< Celsius
    PeakMin m_supplyPeakMin;    ///< volts

    // Last-known current values, retained so `resetPaValues()` can clamp
    // peak/min back to the live reading without needing a fresh telemetry
    // signal to land first.
    double m_fwdCurrent{0.0};
    double m_revCurrent{0.0};
    double m_swrCurrent{1.0};
    double m_paCurrentCurrent{0.0};
    double m_paTempCurrent{0.0};
    double m_supplyCurrent{0.0};

    // Helper: format a label with peak/min annotation.  Output shape:
    //   "12.34 W  (P 50.00 / M 5.00)"
    // The annotation is collapsed (just "current unit") when:
    //   * peak/min are still at sentinel ±infinity values (no signal yet), OR
    //   * formatted peak == formatted min (single-sample path; range hasn't
    //     opened up at the requested precision yet).
    // Both cases keep first-impression and steady-state readings clean.
    static QString formatWithPeakMin(double current,
                                     const PeakMin& pm,
                                     const QString& unit,
                                     int precision);
};

} // namespace NereusSDR
