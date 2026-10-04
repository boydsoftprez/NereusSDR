// =================================================================
// src/core/CalibrationController.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/setup.cs (udHPSDRFreqCorrectFactor,
//     chkUsing10MHzRef, udHPSDRFreqCorrectFactor10MHz, udTXDisplayCalOffset,
//     ud6mLNAGainOffset, ud6mRx2LNAGainOffset, udGeneralCalFreq1/2,
//     udGeneralCalLevel, btnResetLevelCal — lines 5137-5144; 14036-14050;
//     22690-22706; 14325-14333; 17243-17248; 18315-18317; 6470-6525),
//     original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs (CalibrateFreq, CalibrateLevel,
//     RXCalibrationOffset, RX6mGainOffset_RX1/RX2 — lines 9764-9844;
//     21022-21086), original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-25 - R-R3-46 / R-R3-49 (remote-window parity Task 13):
//                 loadTransmitCalibration. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-28 - The volt calibration (AmpVoff / AmpSens) takes Thetis's
//                 defaults, clamps and per-model default (setHardwareModel,
//                 restoreDefaultVoltCalibration, initVoltsAmpsCalibration's
//                 rule) now that the PA current reading applies it.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - 6 m LNA gain offsets default to Thetis's 13 dB where
//                 nothing is stored (JJ's ruling: match Thetis). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - R-R3-49 (found bug): Log Volts/Amps to VALog.txt works:
//                 the controller reads the box (logVoltsAmps), the station's
//                 RadioModel logs through VoltsAmpsLog (Thetis console.cs
//                 LogVA). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-09-29 - Level Cal: the unused level offset (cal/levelOffset) is
//                 removed; the meter and display offsets live on RadioModel.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

// --- From setup.cs ---

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

// --- From console.cs ---

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
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
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines.
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#pragma once

#include "core/NereusCoreExport.h"
#include "core/HpsdrModel.h"
#include "core/PaCalProfile.h"

#include <QObject>
#include <QString>

namespace NereusSDR {

// CalibrationController — calibration-time state for frequency, level, and PA current.
//
// Porting sources:
//   setup.cs:5137-5144  HPSDRFreqCorrectFactorViaAutoCalibration / udHPSDRFreqCorrectFactor [@501e3f5]
//   setup.cs:14036-14050 udHPSDRFreqCorrectFactor_ValueChanged (toggle 10 MHz vs normal factor) [@501e3f5]
//   setup.cs:22690-22706 chkUsing10MHzRef_CheckedChanged / btnHPSDRFreqCalReset10MHz [@501e3f5]
//   setup.cs:14325-14333 udTXDisplayCalOffset_ValueChanged → Display.TXDisplayCalOffset [@501e3f5]
//   setup.cs:17243-17248 ud6mLNAGainOffset_ValueChanged → console.RX6mGainOffset_RX1 [@501e3f5]
//   setup.cs:18315-18317 ud6mRx2LNAGainOffset_ValueChanged → console.RX6mGainOffset_RX2 [@501e3f5]
//   console.cs:9766-9839 CalibrateFreq (FreqCalibrationRunning + correction factor write-back) [@501e3f5]
//   console.cs:9844-10215 CalibrateLevel (calibrating flag + per-RX cal offset) [@501e3f5]
//   console.cs:21022-21086 RXCalibrationOffset / _rx1_display_cal_offset / _rx2_display_cal_offset [@501e3f5]
//
// Per-MAC persistence: hardware/<mac>/cal/{freqFactor, freqFactor10M, using10M,
//   rx1_6mLna, rx2_6mLna, txDisplayOffset, paSens, paOffset}
class NEREUS_CORE_EXPORT CalibrationController : public QObject {
    Q_OBJECT

public:
    explicit CalibrationController(QObject* parent = nullptr);

    // ── Frequency correction factor ───────────────────────────────────────────
    // Source: setup.cs:5137-5144 HPSDRFreqCorrectFactorViaAutoCalibration
    //   udHPSDRFreqCorrectFactor (normal, non-10MHz ref) [@501e3f5]
    double freqCorrectionFactor() const;
    void   setFreqCorrectionFactor(double factor);

    // Source: setup.cs:22704-22706 udHPSDRFreqCorrectFactor10MHz_ValueChanged
    //   (separate factor when using external 10 MHz reference) [@501e3f5]
    double freqCorrectionFactor10M() const;
    void   setFreqCorrectionFactor10M(double factor);

    // Source: setup.cs:22690-22696 chkUsing10MHzRef_CheckedChanged
    //   udHPSDRFreqCorrectFactor10MHz.Enabled = chkUsing10MHzRef.Checked [@501e3f5]
    bool   using10MHzRef() const;
    void   setUsing10MHzRef(bool on);

    // Effective frequency correction factor — picks based on using10MHzRef.
    // Source: setup.cs:14036-14050 udHPSDRFreqCorrectFactor_ValueChanged:
    //   if (!_freqCorrectFactorChangedViaAutoCalibration || !chkUsing10MHzRef.Checked)
    //     NetworkIO.FreqCorrectionFactor = udHPSDRFreqCorrectFactor.Value;
    //   else if (chkUsing10MHzRef.Checked)
    //     NetworkIO.FreqCorrectionFactor = udHPSDRFreqCorrectFactor10MHz.Value;
    //   [@501e3f5]
    double effectiveFreqCorrectionFactor() const;

    // ── RX1 / RX2 6m LNA gain offsets ────────────────────────────────────────
    // Source: setup.cs:17243-17248 ud6mLNAGainOffset → console.RX6mGainOffset_RX1 [@501e3f5]
    double rx1_6mLnaOffset() const;
    void   setRx1_6mLnaOffset(double db);

    // Source: setup.cs:18315-18317 ud6mRx2LNAGainOffset → console.RX6mGainOffset_RX2 [@501e3f5]
    double rx2_6mLnaOffset() const;
    void   setRx2_6mLnaOffset(double db);

    // ── TX display calibration offset ─────────────────────────────────────────
    // Source: setup.cs:14325-14328 udTXDisplayCalOffset → Display.TXDisplayCalOffset [@501e3f5]
    double txDisplayOffsetDb() const;
    void   setTxDisplayOffsetDb(double db);

    // ── PA current calculation parameters (Volts/Amps Calibration) ───────────
    // Thetis AmpSens (sensitivity, mV per amp) and AmpVoff (sensor voltage
    // offset, mV), which convertToAmps applies to the PA current reading
    // (console.cs:24937-24975 [v2.10.3.15]; PaTelemetryScaling convertToAmps).
    // The setters clamp as Thetis's: sensitivity >= 0.001, offset >= 0.
    double paCurrentSensitivity() const;
    void   setPaCurrentSensitivity(double sens);

    double paCurrentOffset() const;
    void   setPaCurrentOffset(double offset);

    // The radio's model, whose factory volt calibration
    // (defaultVoltCalibrationFor, Thetis GetDefaultVoltCalibration) applies
    // while the operator has not set one. A saved calibration is kept.
    void setHardwareModel(HPSDRModel model);
    HPSDRModel hardwareModel() const noexcept { return m_hardwareModel; }

    // Thetis btnAmpDefault_Click (setup.cs:24346-24352 [v2.10.3.15]): the
    // model's defaults, which then count as set and are saved.
    void restoreDefaultVoltCalibration();

    // ── PA forward-power calibration profile ──────────────────────────────────
    // Source: Thetis console.cs:6691-6724 CalibratedPAPower [v2.10.3.13] —
    //   per-board cal table + PowerKernel piecewise-linear interpolation.
    //   The table itself (boardClass + 11 watts entries) lives in
    //   `PaCalProfile`; this controller owns its lifecycle and persistence.
    //   `RadioModel::connectToRadio` seeds the profile from
    //   `paCalBoardClassFor(m_hardwareProfile.model)` on first connect to
    //   each MAC (when no persisted state exists); thereafter the persisted
    //   profile is restored on every connect.
    PaCalProfile paCalProfile() const { return m_paCalProfile; }
    void setPaCalProfile(const PaCalProfile& p);

    // Set a single user-editable cal point. `idx` is in [1..10]; idx 0 is
    // hard-coded to 0.0 in PaCalProfile (see PaCalProfile.h §"Index 0 is
    // always 0 W") so it's not exposed for editing here. Out-of-range
    // indices are silent no-ops (production-safe — UI bugs cannot crash the
    // calibration tab). Emits `paCalPointChanged(idx, watts)` then
    // `changed()` on a successful update.
    void setPaCalPoint(int idx, float watts);

    // Forwarder to `m_paCalProfile.interpolate(rawAdcWatts)`.
    // Source: Thetis console.cs:6691-6724 CalibratedPAPower [v2.10.3.13] —
    //   `RadioModel`'s `alex_fwd` reading is routed through this method
    //   (Task 3.5 of the P1 full-parity plan) so live FWD power follows the
    //   user-calibrated table.
    float calibratedFwdPowerWatts(float rawAdcWatts) const noexcept;

    // ── Persistence ───────────────────────────────────────────────────────────
    void setMacAddress(const QString& mac);
    void load();   // hydrate from AppSettings under hardware/<mac>/cal/...
    void save();   // persist current state to AppSettings
    // R-R3-46 / R-R3-49 (remote-window parity Task 13): re-read only TX
    // Display Cal and Volts/Amps Calibration (cal/txDisplayOffset, paSens,
    // paOffset), through their setters, so a window's change to them
    // applies on the Core at once, on the air too, without reloading the
    // PA forward-power table that waits for receive.
    void loadTransmitCalibration();

    // R-R3-49: Volts/Amps Calibration's "Log Volts/Amps to VALog.txt"
    // (Thetis chkLogVoltsAmps, setup.cs:27627 [v2.10.3.15]), as the
    // Calibration tab stores it (hardware/<mac>/paCalibration/cal/
    // logVoltsAmps, "true"/"false" or "True"/"False"). load() and
    // loadTransmitCalibration() read it; RadioModel writes the log.
    bool logVoltsAmps() const { return m_logVoltsAmps; }
    void setLogVoltsAmps(bool on);

signals:
    // Emitted after any setter changes state. P2RadioConnection listens so
    // it can reapply effectiveFreqCorrectionFactor() on the next frequency command.
    void changed();

    // Emitted when a single PA cal-point watts slot changes via
    // `setPaCalPoint`. The general-purpose `changed()` signal also fires
    // (so existing consumers re-react), but this carries the index +
    // watts payload for the per-spinbox GUI.
    void paCalPointChanged(int idx, float watts);

    // Emitted when the entire PA cal profile is replaced via
    // `setPaCalProfile` (board class change or wholesale table swap).
    // `changed()` also fires.
    void paCalProfileChanged();

    // R-R3-49: the Volts/Amps log was turned on or off.
    void logVoltsAmpsChanged(bool on);

private:
    void readLogVoltsAmps();
    bool m_logVoltsAmps{false};
    // Source: setup.cs:5137 udHPSDRFreqCorrectFactor default 1.0 [@501e3f5]
    double m_freqCorrectionFactor{1.0};
    // Source: setup.cs:22701 btnHPSDRFreqCalReset10MHz → value = 1.0 [@501e3f5]
    double m_freqCorrectionFactor10M{1.0};
    // Source: setup.cs:22690 chkUsing10MHzRef default unchecked [@501e3f5]
    bool   m_using10MHzRef{false};
    // From Thetis setup.designer.cs:12112-12116 [v2.10.3.15]:
    //   this.ud6mLNAGainOffset.Value = new decimal(new int[] { 13, 0, 0, 0});
    // (console.cs:11812 _rx_6m_gain_offset_rx1 = 13 agrees.) An earlier
    // comment here gave Thetis's default as 0; it is 13 dB.
    double m_rx1_6mLnaOffset{13.0};
    // From Thetis setup.designer.cs:12070-12074 [v2.10.3.15]: 13 dB
    // (console.cs:11825 rx_6m_gain_offset_rx2 = 13).
    // Upstream inline attribution preserved verbatim:
    //   setup.cs:6261  HardwareSpecific.Model == HPSDRModel.REDPITAYA))//DH1KLM
    double m_rx2_6mLnaOffset{13.0};
    // Source: setup.cs:14325 udTXDisplayCalOffset default 0 [@501e3f5]
    double m_txDisplayOffsetDb{0.0};
    // From Thetis console.cs:24937-24938 [v2.10.3.15]:
    //   private float _amp_voff = 360.0f;
    //   private float _amp_sens = 120.0f;
    double m_paCurrentSensitivity{120.0};
    double m_paCurrentOffset{360.0};
    // Thetis _bSensSet / _bVoffSet (setup.cs:24344-24345 [v2.10.3.15]):
    // whether the operator (or a saved value) set each one. Until both are
    // set, the model's defaults apply and are not saved.
    bool m_paCurrentSensitivitySet{false};
    bool m_paCurrentOffsetSet{false};
    HPSDRModel m_hardwareModel{HPSDRModel::FIRST};

    // initVoltsAmpsCalibration's rule for values read from the settings.
    void applyLoadedVoltCalibration(const QString& sensText, const QString& voffText);
    // The model's defaults, marked as not set.
    void applyModelVoltCalibration();

    // Source: console.cs:6691-6724 CalibratedPAPower — per-board cal table
    //   driving the FWD-power UI meter. Default-constructed `PaCalProfile`
    //   has `boardClass == None` and value-initialized watts (all zeros);
    //   `RadioModel::connectToRadio` swaps in `PaCalProfile::defaults(class)`
    //   on first connect to each MAC. [v2.10.3.13]
    PaCalProfile m_paCalProfile;

    QString m_mac;
};

} // namespace NereusSDR
