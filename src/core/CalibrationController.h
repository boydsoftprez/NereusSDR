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
//   levelOffset, rx1_6mLna, rx2_6mLna, txDisplayOffset, paSens, paOffset}
class CalibrationController : public QObject {
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

    // ── Level calibration offset ──────────────────────────────────────────────
    // Source: console.cs:21074-21086 _rx1_display_cal_offset / _rx2_display_cal_offset
    //   RXCalibrationOffset(int rx) [@501e3f5]
    // Upstream inline attribution preserved verbatim:
    //   :21075  HardwareSpecific.Model == HPSDRModel.ANAN_G2_1K || HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
    //   :21090  // Added 6/11/05 BT to support CAT //[2.10.3.11]MW0LGE included setter
    double levelOffsetDb() const;
    void   setLevelOffsetDb(double db);

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

    // ── PA current calculation parameters ─────────────────────────────────────
    // Source: console.cs:6691-6724 CalibratedPAPower — sensitivity and offset
    //   are hardware constants that live in BoardCapabilities defaults; mirrored
    //   here for live per-radio user override. [@501e3f5]
    double paCurrentSensitivity() const;
    void   setPaCurrentSensitivity(double sens);

    double paCurrentOffset() const;
    void   setPaCurrentOffset(double offset);

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

private:
    // Source: setup.cs:5137 udHPSDRFreqCorrectFactor default 1.0 [@501e3f5]
    double m_freqCorrectionFactor{1.0};
    // Source: setup.cs:22701 btnHPSDRFreqCalReset10MHz → value = 1.0 [@501e3f5]
    double m_freqCorrectionFactor10M{1.0};
    // Source: setup.cs:22690 chkUsing10MHzRef default unchecked [@501e3f5]
    bool   m_using10MHzRef{false};
    // Source: console.cs:21074 _rx1_display_cal_offset default 0 [@501e3f5]
    // Upstream inline attribution preserved verbatim:
    //   console.cs:21075  HardwareSpecific.Model == HPSDRModel.ANAN_G2_1K || HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
    double m_levelOffsetDb{0.0};
    // Source: setup.cs:3866 ud6mLNAGainOffset default 0 [@501e3f5]
    double m_rx1_6mLnaOffset{0.0};
    // Source: setup.cs:6262 ud6mRx2LNAGainOffset default 0 [@501e3f5]
    // Upstream inline attribution preserved verbatim:
    //   setup.cs:6261  HardwareSpecific.Model == HPSDRModel.REDPITAYA))//DH1KLM
    double m_rx2_6mLnaOffset{0.0};
    // Source: setup.cs:14325 udTXDisplayCalOffset default 0 [@501e3f5]
    double m_txDisplayOffsetDb{0.0};
    // Source: console.cs:6691 CalibratedPAPower — default sensitivity/offset
    //   values from PA current sensing circuit constants [@501e3f5]
    double m_paCurrentSensitivity{1.0};
    double m_paCurrentOffset{0.0};

    // Source: console.cs:6691-6724 CalibratedPAPower — per-board cal table
    //   driving the FWD-power UI meter. Default-constructed `PaCalProfile`
    //   has `boardClass == None` and value-initialized watts (all zeros);
    //   `RadioModel::connectToRadio` swaps in `PaCalProfile::defaults(class)`
    //   on first connect to each MAC. [v2.10.3.13]
    PaCalProfile m_paCalProfile;

    QString m_mac;
};

} // namespace NereusSDR
