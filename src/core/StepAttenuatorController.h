// =================================================================
// src/core/StepAttenuatorController.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs,
//   Project Files/Source/Console/enums.cs [v2.10.3.15],
//   original licences from Thetis source are included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-05-03 — Added setAttOnTxValue / attOnTxValue setter pair (Phase 1
//                 Agent 1D of #167) — the ATT-on-TX-on-power-change safety
//                 gate used by TransmitModel::setPowerUsingTargetDbm
//                 (Thetis console.cs:46740-46748 [v2.10.3.13]).  Mirrors
//                 Thetis SetupForm.ATTOnTX (mi0bot setup.cs:3988-4017
//                 [v2.10.3.13]); range clamped to [m_minAttDb, 31].
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-23: R-R3-46 / R-R3-11 / R-R3-13: a change signal for every
//                 setting (auto-attenuate mode, undo, undo delay, hold, the
//                 attenuator range, the RX1 preamp), settingsReloaded() after
//                 loadSettings(), an opt-in debounced per-MAC save for the
//                 Core, and the RX1 preamp held here so a remote window can
//                 set it through the Core.  NereusSDR-original; no new
//                 Thetis logic.  J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-25: R-R3-49 (parity Task 5): attOnTxEnabledChanged and
//                 forceAttWhenPsOffChanged, so the Core's mirrored step
//                 attenuator follows every change of either, PureSignal's
//                 own included.  NereusSDR-original; no new Thetis logic.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25: R-R3-49 (group A fix wave, M6): ATT on TX, its value and
//                 Force ATT schedule the Core's debounced save.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27: A11 / R-R3-49 (parity Task 31): txAttenuatorOffsetDb(),
//                 Thetis Display.TXAttenuatorOffset, set with every TX step
//                 attenuation this controller applies. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28: R-R3-46 / R-R3-11: a step attenuator per receive ADC, as
//                 Thetis keeps RX1's and RX2's (console.cs:11027-11063,
//                 11213-11251 [v2.10.3.15]): slice A's value on slice A's
//                 ADC, the other ADC's own value following the band of the
//                 first slice on it, both equal while diversity links them.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Level Cal: PreampMode carries all ten Thetis modes
//                 (enums.cs:236-251 [v2.10.3.15], SA_MINUS10/20/30 added;
//                 that file's header is now carried below) and
//                 setBoardIdentity feeds the stored-mode move. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Level Cal: each preamp mode drives the step attenuator,
//                 the preamp bit and the Alex attenuator as Thetis does
//                 (console.cs:19218-19330 [v2.10.3.15]), and above 31 dB an
//                 Alex board switches in the Alex attenuator and sends the
//                 value + 2 (console.cs:11027-11065). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Level Cal fix wave: RX2's own preamp mode with its band
//                memory and drive (console.cs:19413-19520 [v2.10.3.15]),
//                and the HPSDR MOX path turns RX1's step attenuator off and
//                holds RX2's mode (console.cs:29598-29608, 29688-29692).
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Level Cal 2 review: RX2's own attenuator is held to the
//                second ADC's 0-31 dB field (kRx2StepAttMaxDb,
//                rx2MaxAttenuation), so no RX2 value wraps on the wire.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

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

// --- From enums.cs ---
/*  enums.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2000-2025 Original authors
Copyright (C) 2020-2026 Richard Samphire MW0LGE

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

#pragma once

#include "models/Band.h"
#include "core/WdspTypes.h"
#include "core/HpsdrModel.h"

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <array>
#include <optional>
#include <unordered_map>
#include <vector>

namespace NereusSDR {

class RadioConnection;
class ReceiverManager;

// --- Enums ---

// ADC overload severity level.
// From Thetis console.cs:21369 — yellow when level > 0, red when level > 3.
enum class OverloadLevel {
    None,
    Yellow,
    Red
};

// Auto-attenuate algorithm selection.
enum class AutoAttMode {
    Classic,    // Thetis 1:1 bump + stack undo
    Adaptive    // NereusSDR attack/hold/decay with per-band memory
};

// Preamp mode: the ten Thetis modes, in Thetis's order (the integer is
// what a band's stored mode and the link carry).
// From Thetis enums.cs:236-251 [v2.10.3.15]:
//   public enum PreampMode
//   {
//       FIRST = -1,
//       HPSDR_OFF,
//       HPSDR_ON,
//       HPSDR_MINUS10,
//       HPSDR_MINUS20,
//       HPSDR_MINUS30,
//       HPSDR_MINUS40,
//       HPSDR_MINUS50,
//       SA_MINUS10,
//       SA_MINUS20,  //MW0LGE_21d
//       SA_MINUS30,
//       // STEP_ATTEN,
//       LAST,
//   }
// Off..Minus50 are the HPSDR modes (the preamp switch and the Alex
// attenuator); SaMinus10..SaMinus30 put 10, 20 or 30 dB on the step
// attenuator. Values stored before the SA modes existed are moved once by
// loadSettings (BoardCapsTable::preampModeFromV1).
enum class PreampMode {
    Off,        // HPSDR_OFF
    On,         // HPSDR_ON
    Minus10,    // HPSDR_MINUS10
    Minus20,    // HPSDR_MINUS20
    Minus30,    // HPSDR_MINUS30
    Minus40,    // HPSDR_MINUS40
    Minus50,    // HPSDR_MINUS50
    SaMinus10,  // SA_MINUS10
    SaMinus20,  // SA_MINUS20  //MW0LGE_21d
    SaMinus30   // SA_MINUS30
};

// --- Controller ---

class StepAttenuatorController : public QObject {
    Q_OBJECT
public:
    explicit StepAttenuatorController(QObject* parent = nullptr);

    // --- Accessors ---

    // Per-ADC overload level (0-5). From Thetis console.cs:21212.
    int overloadCounter(int adc) const;

    // Derived severity for a given ADC.
    OverloadLevel overloadLevel(int adc) const;

    // Current step attenuator value (dB).
    int attenuatorDb() const noexcept { return m_attDb; }

    // Current preamp mode.
    PreampMode preampMode() const noexcept { return m_preampMode; }

    // Step-att enabled (S-ATT mode vs ATT preamp combo mode).
    bool stepAttEnabled() const noexcept { return m_stepAttEnabled; }

    // Auto-att state.
    AutoAttMode autoAttMode() const noexcept { return m_autoAttMode; }
    bool autoAttEnabled() const noexcept { return m_autoAttEnabled; }
    bool autoAttApplied() const noexcept { return m_autoAttApplied; }

    // Issue #259 (review fix): expose Undo / Hold / Decay state so the
    // GeneralOptionsPage cold-open init pulls every persisted auto-att
    // field, not just enable + mode.  Without these the chkAutoAttUndoRx1
    // and spnAutoAttHoldRx1 widgets stay at their constructor defaults
    // even after loadSettings() restores the controller — exactly the
    // partial-restore case the reviewer flagged on PR #260.
    bool autoAttUndo() const noexcept { return m_autoUndoEnabled; }
    // Adaptive-mode hold window expressed in whole seconds (Setup spinbox
    // is integer-valued).  Internal storage is ms; the page wraps the
    // setter pair setAutoAttHoldSeconds (Adaptive) / setAutoUndoDelaySec
    // (Classic) on the same spinbox.
    int adaptiveHoldSeconds() const noexcept { return m_adaptiveHoldMs / 1000; }
    int adaptiveHoldMs() const noexcept { return m_adaptiveHoldMs; }
    int autoUndoDelaySec() const noexcept { return m_autoUndoDelaySec; }

    // ADC-linked state (both RX share the same ADC).
    bool adcLinked() const noexcept { return m_adcLinked; }

    // RX1 (second ADC) preamp on dual-ADC P2 boards (OrionMKII family).
    // Held here so the Core can apply it for a remote window; the local RX
    // applet still drives P2RadioConnection::setRx1Preamp directly and
    // never calls this.  Not persisted, like the local toggle.
    bool rx1Preamp() const noexcept { return m_rx1Preamp; }
    void setRx1Preamp(bool on);

    // ── A step attenuator per receive ADC (R-R3-46 / R-R3-11) ─────────────
    //
    // Thetis keeps one step attenuator per receiver and sends each to the
    // ADC that receiver is using, RX1's here and RX2's alike:
    // From Thetis console.cs:11021-11028 [v2.10.3.15] (RX1AttenuatorData),
    // after the range check that ends
    //   HardwareSpecific.Model != HPSDRModel.REDPITAYA) //DH1KLM
    //   _rx1_attenuator_data = validateRX1StepAttData(_rx1_attenuator_data); //[2.10.3.9]MW0LGE validated
    //
    //   //MW0LGE_22b step atten
    //   int nRX1DDCinUse = -1, nRX2DDCinUse = -1, sync1 = -1, sync2 = -1, psrx = -1, pstx = -1;
    //   GetDDC(out nRX1DDCinUse, out nRX2DDCinUse, out sync1, out sync2, out psrx, out pstx);
    //
    //   int nRX1ADCinUse = GetADCInUse(nRX1DDCinUse); // (rx1)
    //   int nRX2ADCinUse = GetADCInUse(nRX2DDCinUse); // (rx2)
    // then NetworkIO.SetADC1/2/3StepAttenData(_rx1_attenuator_data) for
    // nRX1ADCinUse 0/1/2 (console.cs 11062-11064), and RX2AttenuatorData
    // the same with rx2_attenuator_data (console.cs 11228-11230). Two
    // receivers on one ADC, or in linked diversity, keep one value:
    // From Thetis console.cs:11080-11081 [v2.10.3.15]:
    //   bool bRX1RX2diversity = m_bDiversityAttLinkForRX1andRX2 && (diversityForm != null && Diversity2 && diversityForm.EXTDIVOutput == 2); // if using diversity, and both rx's are linked, then we need to attenuate both
    //   if (((nRX1ADCinUse == nRX2ADCinUse) || bRX1RX2diversity) && RX2AttenuatorData != _rx1_attenuator_data)
    //
    // NereusSDR has up to five slices on two receive ADCs. attenuatorDb()
    // is Thetis's RX1 value: slice A's, sent to slice A's ADC. The other
    // ADC in use has its own value, Thetis's RX2 value, with its own band
    // memory (rx2_step_attenuator_by_band) following the band of the
    // controlling slice on that ADC (RadioModel picks the lowest-numbered
    // slice there; Thetis has only two receivers and no rule for more).
    // Slices on one ADC share its value. While diversity links the two
    // ADCs (Thetis's link is on unless "No ATT link" is ticked; NereusSDR's
    // diversity always mixes both, EXTDIVOutput 2) both take RX1's value.
    //
    // rx1Adc: slice A's ADC. rx2Adc: the other ADC in use, or -1 when every
    // slice is on slice A's ADC (the same ADC as rx1Adc reads as -1).
    // rx2Band: the band the other ADC's attenuator follows (kept as it was
    // while rx2Adc is -1: nothing controls it then). linked: the
    // two ADCs share RX1's value (diversity). rx2SliceMask: bit n set when
    // slice n is on the other ADC (so reads rx2AttenuatorDb); 0 while linked.
    void setAdcRouting(int rx1Adc, int rx2Adc, Band rx2Band, bool linked,
                       quint32 rx2SliceMask = 0);
    quint32 rx2SliceMask() const noexcept { return m_rx2SliceMask; }
    int rx1Adc() const noexcept { return m_rx1Adc; }
    int rx2Adc() const noexcept { return m_rx2Adc; }
    Band rx2Band() const noexcept { return m_rx2Band; }
    bool adcAttenuatorsLinked() const noexcept { return m_adcAttLinked; }
    // Thetis RX2AttenuatorData: the other ADC's own value (RX1's while
    // linked). Its own value is clamped to rx2MaxAttenuation().
    int rx2AttenuatorDb() const noexcept { return m_rx2AttDb; }
    void setRx2Attenuation(int dB);
    // Deliberate divergence (operator decision 2026-09-30): RX2's own step
    // attenuator is the second ADC's 5-bit field, 0-31 dB:
    // TAPR-OpenHPSDR-Firmware @e7c6584 Angelia.v:2319 (C1[4:0] input
    // attenuator 2), Orion.v:2295 ("0-31 dB") and :2419. Thetis lets RX2
    // reach 61 on an Alex board (console.cs:11176-11189 [v2.10.3.15],
    // udRX2StepAttData.Maximum = 61) and sends the value + 2 above 31
    // (console.cs:11211-11222), but RX2's setter never switches the Alex
    // attenuator in (SetAlexAtten is only in RX1's, console.cs:11044-11056),
    // so the gateware keeps the low 5 bits and 40 dB lands as 10. NereusSDR
    // holds RX2's own value to the field instead, so nothing wraps.
    static constexpr int kRx2StepAttMaxDb = 31;
    // The top of RX2's own range: the radio's, no higher than the field's.
    int rx2MaxAttenuation() const noexcept { return std::min(m_maxAttDb, kRx2StepAttMaxDb); }
    // True when `adc` reads and is set through attenuatorDb() (slice A's
    // ADC, an ADC not in use, or any ADC while linked); false for the other
    // ADC in use, which reads rx2AttenuatorDb().
    bool adcUsesRx1Attenuator(int adc) const noexcept;
    // The receive attenuation on `adc`'s input.
    int attenuatorDbForAdc(int adc) const noexcept;
    // Set the attenuation of `adc`: RX1's or the other ADC's, as above.
    void setAttenuationForAdc(int adc, int dB);

    // RX2's own step attenuator enable and auto-attenuate settings, as
    // Thetis keeps them apart from RX1's (_rx2_step_att_enabled,
    // _auto_att_rx2, _auto_att_undo_rx2, _auto_att_hold_delay_rx2), saved
    // for the radio. On one ADC (or linked) the two enables are one, as
    // Thetis's Setup mirrors them. RX2's step attenuator off: its slices
    // read the second preamp's offset and RX2 auto-attenuate does nothing.
    bool rx2StepAttEnabled() const noexcept { return m_rx2StepAttEnabled; }
    void setRx2StepAttEnabled(bool on);
    bool rx2AutoAttEnabled() const noexcept { return m_rx2AutoAttEnabled; }
    void setRx2AutoAttEnabled(bool on);
    bool rx2AutoAttUndo() const noexcept { return m_rx2AutoUndoEnabled; }
    void setRx2AutoAttUndo(bool on);
    int rx2AutoUndoDelaySec() const noexcept { return m_rx2AutoUndoDelaySec; }
    void setRx2AutoUndoDelaySec(int sec);

    // Level Cal: RX2's own preamp mode (Thetis RX2PreampMode,
    // console.cs:19413-19520 [v2.10.3.15]) with its band memory
    // (rx2_preamp_by_band), saved for the radio. With RX2's step attenuator
    // off it puts the mode's attenuation on the other ADC on the boards in
    // Thetis's list; on the HPSDR it sends the second preamp bit
    // (NetworkIO.SetRX2Preamp). While RX2 shares slice A's ADC, or diversity
    // links them, the two modes are one, as Thetis links them.
    PreampMode rx2PreampMode() const noexcept { return m_rx2PreampMode; }
    void setRx2PreampMode(PreampMode mode);

    // R-R3-46 / R-R3-11: save this radio's settings a short while after an
    // operator change, not only at teardown.  Off by default; the Core
    // (DaemonApp) turns it on so a change made from a remote window is on
    // disk without waiting for the Core to stop.  Auto-attenuate's own
    // moves never schedule a save.
    void setDebouncedSaveEnabled(bool on);

    // R-R3-46: on a band change, send the band's restored attenuation and
    // preamp to the radio, as Thetis does (console.cs:17325 [v2.10.3.15],
    // see setBand).  Off by default: a local window keeps today's behaviour
    // (the restored value is shown, not sent); the Core turns it on so the
    // radio runs what every window shows.
    void setBandRestoreToRadio(bool on) noexcept { m_bandRestoreToRadio = on; }
    bool bandRestoreToRadio() const noexcept { return m_bandRestoreToRadio; }
    bool debouncedSaveEnabled() const noexcept { return m_debouncedSave; }
    bool savePending() const { return m_saveTimer.isActive(); }
    // Run a pending debounced save now (no-op when none is pending).
    void flushPendingSave();

    // --- Configuration setters ---

    void setAutoAttEnabled(bool on);
    void setAutoAttMode(AutoAttMode mode);

    // Gate AutoAttMode::Adaptive on board support — set on connect from
    // RadioModel using BoardCapabilities::hasStepAttenuatorCal.  Default
    // true so existing behaviour holds when not yet wired.
    //
    // When false:
    //   - setAutoAttMode(Adaptive) is silently coerced to Classic.
    //   - loadSettings() clamps a persisted "Adaptive" string to Classic
    //     (handles cross-radio reconnects with different capabilities).
    //   - The applyAdaptiveAutoAtt() invocation site in tick() is gated
    //     defence-in-depth (the mode-coerce above already prevents reach,
    //     but if a stale state slips through, this is a safety net).
    //
    // Source: NereusSDR-internal extension. AutoAttMode::Adaptive is a
    // NereusSDR feature, not Thetis-derived; the hasStepAttenuatorCal flag
    // was added to BoardCapabilities to gate per-step cal-table support,
    // and we use it here to gate the Adaptive cal mode that depends on
    // per-step calibration data.  See plan
    // docs/architecture/2026-05-02-p1-full-parity-plan.md §4.1.
    void setHasStepAttenuatorCal(bool on) noexcept { m_hasStepAttCal = on; }
    bool hasStepAttenuatorCal() const noexcept { return m_hasStepAttCal; }

    // Classic mode: enable timer-based undo of auto-applied attenuation.
    void setAutoAttUndo(bool on);
    void setAutoUndoDelaySec(int sec);

    // Adaptive mode tunables.
    void setAutoAttHoldSeconds(double sec);
    void setAdaptiveDecayMs(int ms);

    // Step-att vs preamp mode (Thetis _rx1_step_att_enabled).
    void setStepAttEnabled(bool on);

    // Current band — for per-band ATT/preamp storage.
    // R-R3-46: the receive band (Thetis rx1_band); it also sets the transmit
    // band below, which a caller with a separate transmit slice then moves
    // with setTxBand.
    void setBand(Band band);
    Band currentBand() const noexcept { return m_currentBand; }

    // R-R3-46: the transmit band (Thetis _tx_band), whose per-band ATT-on-TX
    // value is applied on MOX and edited by setAttOnTxValue. Thetis keeps it
    // apart from rx1_band: console.cs:17325 [v2.10.3.15] restores the
    // receive attenuator for rx1_band and ATTOnTX for _tx_band.
    void setTxBand(Band band) noexcept { m_txBand = band; }
    Band txBand() const noexcept { return m_txBand; }

    // Set ATT value (e.g. from UI or persistence restore).
    void setAttenuation(int dB, int rx = 0);

    // Set preamp mode (e.g. from UI).
    void setPreampMode(PreampMode mode);

    // Attenuator value bounds (hardware limits).
    //   Most boards: 0..31 dB unsigned.
    //   HL2: −28..+31 dB signed (#175 follow-up — mi0bot widens upper to
    //     +32 at console.cs:11043 [v2.10.3.13-beta2] but that is an
    //     off-by-one bug; chip-correct cap is +31 per InitConsole at
    //     console.cs:2111).  Wire byte derives via `wire = 31 - userDb`
    //     in P1CodecHl2.
    int maxAttenuation() const { return m_maxAttDb; }
    void setMaxAttenuation(int dB);
    int minAttenuation() const { return m_minAttDb; }
    void setMinAttenuation(int dB);

    // --- TX-path configuration (F.2) ---
    //
    // ATT-on-TX master enable (Thetis _m_bATTonTX, console.cs:19041 [v2.10.3.13]).
    // When false, TX ATT is cleared to 0 dB on MOX-on.
    // R-R3-49 (parity Task 5): emits attOnTxEnabledChanged on a change.
    // G-04: a change applies to the radio at once, keyed or not, as the
    // Thetis ATTOnTX setter does (console.cs:19071-19094 [v2.10.3.15]).
    void setAttOnTxEnabled(bool on);
    bool attOnTxEnabled() const noexcept { return m_attOnTxEnabled; }

    // Force-31-dB when PS-A is off (Thetis _forceATTwhenPSAoff,
    // console.cs:29285 [v2.10.3.13] //MW0LGE [2.9.0.7] added).
    // R-R3-49 (parity Task 5): emits forceAttWhenPsOffChanged on a change.
    void setForceAttWhenPsOff(bool on)
    {
        if (m_forceAttWhenPsOff == on) { return; }
        m_forceAttWhenPsOff = on;
        emit forceAttWhenPsOffChanged(on);
        scheduleSave();  // R-R3-49 (group A fix wave, M6): saved at once on the Core
    }
    bool forceAttWhenPsOff() const noexcept { return m_forceAttWhenPsOff; }

    // PS-A active state: true when PureSignal auto-cal is ON.
    // Set by RadioModel when the PS state changes.
    // shouldForce31Db uses !m_psActive as the "PS off" input
    // (equivalent to !chkFWCATUBypass.Checked in Thetis).
    void setPsActive(bool on) { m_psActive = on; }
    bool psActive() const noexcept { return m_psActive; }

    // Current TX DSP mode (set by RadioModel from SliceModel::dspMode).
    // Used by shouldForce31Db to detect CWL/CWU.
    void setCurrentDspMode(DSPMode mode) { m_currentDspMode = mode; }
    DSPMode currentDspMode() const noexcept { return m_currentDspMode; }

    // HPSDR-board flag: true ⟺ connected radio is HPSDRModel::HPSDR
    // (Atlas/Metis kit), which uses the preamp save/restore path instead
    // of per-band TX ATT (Thetis console.cs:29548-29558 [v2.10.3.13]).
    void setIsHpsdrBoard(bool on) { m_isHpsdrBoard = on; }
    bool isHpsdrBoard() const noexcept { return m_isHpsdrBoard; }

    // Per-band TX ATT storage (Thetis tx_step_attenuator_by_band,
    // console.cs:205/48012-48022 [v2.10.3.13]).
    // Default 0 dB for all bands.
    void setTxAttenuationForBand(Band band, int dB);
    int txAttenuationForBand(Band band) const;

    /// Set the step-attenuator value applied during TX (MOX-engaged) on
    /// the current TX band.  Mirrors Thetis SetupForm.ATTOnTX setter:
    ///   - mi0bot setup.cs:3988-4017 [v2.10.3.13] (HL2 fork: −28..31 range)
    ///   - ramdor setup.cs:3957-3977 [v2.10.3.13] (legacy: 0..31 range)
    /// chains into Thetis console.cs:10600-10625 TxAttenData setter
    /// [v2.10.3.13] (//[2.10.3.6]MW0LGE att_fixes), which:
    ///   1. validateTXStepAttData clamps to spinbox limits
    ///   2. setTXstepAttenuatorForBand(_tx_band, value) writes per-band slot
    ///   3. NetworkIO.SetTxAttenData(value) pushes to hardware when m_bATTonTX
    ///
    /// Used by TransmitModel::setPowerUsingTargetDbm to force max ATT (31)
    /// when PS-A is active and drive power changes
    /// (console.cs:46740-46748 [v2.10.3.13] //[2.10.3.5]MW0LGE).
    ///
    /// Range: clamped to [m_minAttDb, 31].  Default m_minAttDb=0 (legacy
    /// boards); HL2 widens to −28 via setMinAttenuation(-28) on connect.
    /// Thetis ATTOnTX always caps at 31 regardless of board.
    ///
    /// Persists per-MAC via the existing per-band TX ATT storage
    /// (saveSettings/loadSettings).  No-op when m_attOnTxEnabled=false:
    /// the per-band slot still updates, but no hardware push occurs (the
    /// MOX-on path in onMoxHardwareFlipped already short-circuits to 0
    /// when ATT-on-TX is disabled).
    void setAttOnTxValue(int dB);
    int  attOnTxValue() const;

    /// Parity Task 31 (A11): Thetis Display.TXAttenuatorOffset
    /// (display.cs:1365-1370 [v2.10.3.15]): the TX step attenuation last
    /// applied to the radio, set beside each NetworkIO.SetTxAttenData call
    /// (console.cs:10613-10622, 19078-19088, 29619 and 29706-29710
    /// [v2.10.3.15], each //[2.10.3.6]MW0LGE att_fixes): the value when ATT
    /// on TX is on, 0 when it is off or at the unkey. The display adds it to
    /// the receive trace while keyed with display duplex on (RX1Offset,
    /// display.cs:4836). Unchanged on the HPSDR board, whose transmit path
    /// switches the preamp instead, as Thetis's is.
    int txAttenuatorOffsetDb() const noexcept { return m_txAttOffsetDb; }

    // shouldForce31Db predicate.
    //
    // Returns true ⟺ the TX attenuator must be forced to 31 dB.
    // From Thetis console.cs:29563-29566 [v2.10.3.13] //MW0LGE [2.9.0.7] added:
    //   txAtt = 31 ⟺ (!chkFWCATUBypass.Checked && _forceATTwhenPSAoff)
    //                 || (CurrentDSPMode == CWL || CurrentDSPMode == CWU)
    bool shouldForce31Db(DSPMode dspMode, bool isPsOff) const;

    // Wire to a RadioConnection for adcOverflow signals.
    void setRadioConnection(RadioConnection* conn);

    // Wire to ReceiverManager for DDC mapping changes.
    void setReceiverManager(ReceiverManager* mgr);

    // Per-MAC persistence — save/load all ATT/preamp/auto-att settings.
    //
    // Issue #259: saveSettings is gated on m_loadedMac == mac so a tear-
    // down that fires before the matching loadSettings can't overwrite
    // the persisted state with constructor defaults. Call
    // markSettingsUnloaded() on disconnect (clears m_loadedMac) so the
    // next connect's tear-down doesn't reuse a stale load tag from a
    // previously-loaded different radio.
    void saveSettings(const QString& mac);
    void loadSettings(const QString& mac);

    // The connected board, its Thetis model and Alex presence. Set before
    // loadSettings: the preamp modes stored before the SA modes existed
    // are moved to the new numbering only once the board is known.
    void setBoardIdentity(HPSDRHW board, HPSDRModel model, bool alexPresent);
    // R-R3-46: also drops the band memory, which is the unloaded radio's.
    void markSettingsUnloaded()
    {
        m_loadedMac.clear();
        m_bandState.clear();
        m_rx2BandAttDb.clear();
        m_rx2BandPreamp.clear();
    }
    bool settingsLoaded() const { return !m_loadedMac.isEmpty(); }

    // --- Tick (public for testability) ---

    // Stop/start the internal tick timer. Tests call setTickTimerEnabled(false)
    // then drive tick() manually for deterministic cycle control.
    void setTickTimerEnabled(bool on);

    // Called on each poll cycle (~100ms). Updates per-ADC hysteresis
    // counters and emits overloadStatusChanged on level transitions.
    // In production, driven by an internal QTimer; exposed for tests.
    void tick();

public slots:
    // Receives adcOverflow(int adc) from RadioConnection.
    // Marks the ADC as overloaded for the current tick cycle.
    void onAdcOverflow(int adc);

    // TX-path activation slot — F.2.
    //
    // Porting from Thetis console.cs:29546-29576 [v2.10.3.13] §6.2-§6.4.
    // Called when MoxController::hardwareFlipped(bool isTx) fires.
    //
    // isTx=true (RX→TX):
    //   HPSDR board: save current preamp mode, then force PreampMode::Off
    //     (≡ Thetis PreampMode.HPSDR_OFF, −20 dB attenuation).
    //   Standard board: look up per-band TX ATT; apply force-31-dB override
    //     if shouldForce31Db() is true; call setTxStepAttenuation() on the
    //     RadioConnection.
    //   If m_attOnTxEnabled is false: push 0 dB TX ATT (no attenuation on TX).
    //
    // isTx=false (TX→RX):
    //   HPSDR board: restore the saved preamp mode.
    //   Standard board: re-apply the current band's RX ATT via setBand()
    //     (existing path restores the per-band RX state).
    //
    // NOTE: the connect() call wiring this slot to MoxController::hardwareFlipped
    // is deferred to Task G.1 (same pattern as F.1 / AlexController). F.2 only
    // adds the slot logic and supporting helpers.
    void onMoxHardwareFlipped(bool isTx);

signals:
    // Emitted when any ADC's overload level transitions between
    // None/Yellow/Red. UI surfaces connect here for badge updates.
    void overloadStatusChanged(int adc, NereusSDR::OverloadLevel level);

    // Emitted when auto-att changes the attenuator value.
    void attenuationChanged(int dB);

    // Emitted when auto-att changes the preamp mode.
    void preampModeChanged(NereusSDR::PreampMode mode);

    // Emitted when auto-att applied/cleared state changes.
    void autoAttActiveChanged(bool applied);

    // Emitted when step-att-enabled changes (ATT ↔ S-ATT mode switch).
    void stepAttEnabledChanged(bool enabled);

    // Emitted when the user toggles "Auto Attenuate RX1 Enable" in
    // Setup → General → Options.  RxApplet listens here to flip the
    // S-ATT label to A-ATT on HL2 boards (mi0bot console.cs:21342-21365
    // [v2.10.3.13-beta2] AutoAttRX1 property: lblPreamp.Text = "A-ATT").
    void autoAttEnabledChanged(bool enabled);

    // Emitted when ADC-linked state changes (both RX share same ADC).
    void adcLinkedChanged(bool linked);

    // R-R3-46: the remaining settings' change signals, so a mirrored
    // object can follow every one of them.
    void autoAttModeChanged(NereusSDR::AutoAttMode mode);
    void autoAttUndoChanged(bool on);
    void autoUndoDelayChanged(int seconds);
    void autoAttHoldChanged(int ms);
    void attenuationRangeChanged(int minDb, int maxDb);
    void rx1PreampChanged(bool on);
    // Parity Task 31: txAttenuatorOffsetDb() changed.
    void txAttenuatorOffsetChanged(int dB);

    // Emitted at the end of loadSettings(): every setting may have changed
    // without its own signal (loadSettings stays silent so local widgets
    // keep today's behaviour).
    void settingsReloaded();

    // Emitted when the per-band ATT-on-TX dB value changes (either via
    // setAttOnTxValue user-side, or via PureSignal::autoAttentionTick
    // writing the new value back).  Bound by the Setup → Transmit → Power
    // udATTOnTX spinbox so AutoAtt updates are reflected in the UI.  ANAN-G2E
    // bench-fix 2026-05-23 (JJ Boyd): added so the new spinbox can mirror
    // AutoAtt's adjustments without a polling timer.
    void attOnTxValueChanged(int dB);

    // R-R3-49 (parity Task 5): ATT on TX and Force ATT changed (by Setup,
    // by the Core's mirrored step attenuator, or by PureSignal).
    void attOnTxEnabledChanged(bool on);
    void forceAttWhenPsOffChanged(bool on);

    // R-R3-46 / R-R3-11: the other ADC's own attenuation changed, or which
    // ADC each value is on (setAdcRouting, the slice mask included) moved.
    void rx2AttenuationChanged(int dB);
    void adcRoutingChanged();
    void rx2StepAttEnabledChanged(bool on);
    void rx2AutoAttEnabledChanged(bool on);
    void rx2AutoAttUndoChanged(bool on);
    void rx2AutoUndoDelayChanged(int seconds);
    // Level Cal: rx2PreampMode() changed.
    void rx2PreampModeChanged(NereusSDR::PreampMode mode);

private:
    static constexpr int kMaxAdcs = 3;
    // From Thetis console.cs:21366 — counter caps at 5.
    static constexpr int kMaxOverloadLevel = 5;
    // From Thetis console.cs:21369 — red threshold.
    static constexpr int kRedThreshold = 3;
    // Default step attenuator bounds (dB).
    static constexpr int kDefaultMaxAttDb = 31;
    static constexpr int kDefaultMinAttDb = 0;
    // Tick interval (ms) — Thetis pollOverloadSyncSeqErr ~400ms,
    // NereusSDR uses 100ms for snappier response.
    static constexpr int kTickIntervalMs = 100;

    // Debounce for the opt-in save.  NereusSDR-native value.
    static constexpr int kSaveDebounceMs = 500;

    // Start (or restart) the debounced save when it is enabled, a radio's
    // settings are loaded, and the radio is not transmitting.
    void scheduleSave();

    // Push a new ATT value to hardware + emit signal.  Used by auto-att
    // paths that bypass setAttenuation() (which also stores per-band state).
    void applyAttToHardware(int dB);

    // Per-ADC state. From Thetis console.cs:21212-21214.
    struct AdcState {
        bool overflowed = false;    // set by onAdcOverflow, cleared by tick
        int level = 0;             // hysteresis counter (0-kMaxOverloadLevel)
        OverloadLevel lastEmitted = OverloadLevel::None;
    };
    std::array<AdcState, kMaxAdcs> m_adcState{};

    // Step attenuator / preamp state.
    int m_attDb = 0;
    PreampMode m_preampMode = PreampMode::Off;
    bool m_stepAttEnabled = true;
    int m_maxAttDb = kDefaultMaxAttDb;
    // The range the caller set; m_maxAttDb is it, held at 31 on a known
    // board outside Thetis's Alex list (recomputeMaxAtt).
    int m_rawMaxAttDb = kDefaultMaxAttDb;
    int m_minAttDb = kDefaultMinAttDb;

    // Issue #259 — guards saveSettings against pre-load clobber.
    //
    // Bug pattern: during connectToRadio() the existing m_connection (from
    // a previous attempt or an in-flight auto-reconnect) triggers a
    // teardownConnection() in RadioModel::connectToRadio. Our save call in
    // teardownConnection fires BEFORE the matching loadSettings runs for
    // this MAC, so it writes the constructor defaults (m_attDb=0,
    // m_stepAttEnabled=true) over the user's previously-persisted state.
    //
    // The gate: m_loadedMac is set to the MAC at loadSettings entry. A
    // saveSettings call where mac != m_loadedMac short-circuits and does
    // nothing, so the defaults never make it to disk before the real
    // values have been pulled in. (Cleared on disconnect via
    // markSettingsUnloaded() so a different-MAC connect doesn't reuse
    // a stale load tag from a prior radio.)
    QString m_loadedMac;

    // setBoardIdentity(); m_boardKnown stays false until it is called.
    HPSDRHW m_board{HPSDRHW::Unknown};
    HPSDRModel m_hpsdrModel{HPSDRModel::FIRST};
    bool m_alexPresent{false};
    bool m_boardKnown{false};

    // Auto-att configuration.
    bool m_autoAttEnabled = false;
    AutoAttMode m_autoAttMode = AutoAttMode::Classic;
    bool m_autoAttApplied = false;

    // Gates AutoAttMode::Adaptive selection.  Default true so existing
    // behaviour holds when not yet wired by RadioModel.  Set to
    // BoardCapabilities::hasStepAttenuatorCal on connect; see the
    // setHasStepAttenuatorCal() header comment for full semantics.  Plan
    // docs/architecture/2026-05-02-p1-full-parity-plan.md §4.1.
    bool m_hasStepAttCal{true};

    // Classic mode state — Thetis uses a stack of historic readings.
    // We simplify to tracking the pre-auto-att value.
    int m_classicSavedAttDb = -1;
    PreampMode m_classicSavedPreamp = PreampMode::Off;
    bool m_autoUndoEnabled = false;
    int m_autoUndoDelaySec = 5;  // From Thetis console.cs:21224
    qint64 m_lastAutoAttTimeMs = 0;

    // Adaptive mode state.
    int m_adaptiveHoldMs = 2000;
    int m_adaptiveDecayMs = 500;
    qint64 m_adaptiveLastAttackMs = 0;
    qint64 m_adaptiveLastDecayMs = 0;
    int m_adaptiveFloorDb = 0;

    // Per-band RX ATT/preamp storage.
    Band m_currentBand = Band::GEN;
    // R-R3-46: the transmit band for the per-band ATT-on-TX value.
    Band m_txBand = Band::GEN;
    struct BandAttState {
        int attDb = 0;
        PreampMode preamp = PreampMode::Off;
    };
    std::unordered_map<int, BandAttState> m_bandState;

    // --- TX-path state (F.2) ---

    // ATT-on-TX master enable. From Thetis console.cs:19041 [v2.10.3.13]
    //   private bool m_bATTonTX = true;
    bool m_attOnTxEnabled{true};
    // Parity Task 31: Display.TXAttenuatorOffset (setTxAttenuatorOffset).
    int m_txAttOffsetDb{0};
    void setTxAttenuatorOffset(int dB);

    // Force-31-dB when PS-A off. From Thetis console.cs:29285 [v2.10.3.13]
    //   private bool _forceATTwhenPSAoff = true; //MW0LGE [2.9.0.7] added
    bool m_forceAttWhenPsOff{true};

    // PureSignal-A active state (true = PS-A on = chkFWCATUBypass.Checked).
    // Set by RadioModel. shouldForce31Db uses !m_psActive as "PS off" input.
    bool m_psActive{false};

    // Current TX DSP mode for shouldForce31Db CW detection.
    DSPMode m_currentDspMode{DSPMode::LSB};

    // HPSDR-board (Atlas/Metis) flag.
    // From Thetis console.cs:29548 [v2.10.3.13]:
    //   if (HardwareSpecific.Model == HPSDRModel.HPSDR) { ... preamp save/restore ... }
    bool m_isHpsdrBoard{false};

    // Per-band TX step attenuator (0-31 dB, default 0).
    // From Thetis console.cs:205 [v2.10.3.13]:
    //   private int[] tx_step_attenuator_by_band;
    // Thetis default: 31 dB per band (console.cs:1810 [v2.10.3.13]):
    //   setTXstepAttenuatorForBand((Band)i, 31);
    // NereusSDR default: 0 (no TX ATT until user configures it).
    // Per-band TX ATT spans HF amateur + GEN/WWV/XVTR and 2 m.  SWL bands
    // (Band::SwlFirst..SwlLast, Phase 3L extension) inherit ham-band
    // values — the HL2 ATT chip is a single hardware register regardless
    // of the SWL slice you tune to.  Sized by the per-band state slots
    // (15: 160m .. XVTR and 2 m, Band.h).
    std::array<int, static_cast<size_t>(kPerBandStateCount)> m_txAttByBand{};  // per-band state slots, 2 m at 14

    // MOX state mirror — set by onMoxHardwareFlipped().  Auto-att (Classic +
    // Adaptive) reads this to skip overload-driven ATT bumps during TX, since
    // any ADC overflow during TX is own-TX leakage, not antenna signal.
    // Without this gate the auto-att would bump from 31 → 32 on the first
    // tick after MOX engages because own-TX leakage trips ADC overflow.
    bool m_isMox{false};

    // HPSDR-only preamp save/restore (F.2).
    // Distinct from m_classicSavedPreamp (which is for the auto-att
    // Classic mode undo path and serves a different purpose).
    PreampMode m_savedPreampMode{PreampMode::Off};

    // Saved RX att for restore on TX→RX (#175 follow-up bench, 2026-05-04).
    //
    // Stashed by onMoxHardwareFlipped(true) before swapping m_attDb to txAtt;
    // read back on onMoxHardwareFlipped(false).  Init to 0 (matches default
    // RX att); only meaningful between MOX flips.
    //
    // Why a separate field instead of overwriting
    // m_bandState[currentBand].attDb: the per-band slot is the user's
    // RX-time setting and must stay untouched.  If we wrote txAtt into it on
    // RX→TX, then a band-change while transmitting would corrupt the stored
    // RX value.  m_savedRxAttDbForTx is a transient TX-window stash.
    //
    // Mirrors Thetis behavior — mi0bot console.cs:29960-30002
    // [v2.10.3.13-beta2] updateAttNudsCombos() swaps a separate
    // udTXStepAttData spinbox over udRX1StepAttData during MOX, and restores
    // the RX-time spinbox on un-key.  NereusSDR has a single S-ATT spinbox
    // bound to attenuationChanged, so we emit the TX value into m_attDb +
    // attenuationChanged on RX→TX, then restore on TX→RX.  User-visible
    // result is identical: "the spinbox jumped to 31 during TX".
    int m_savedRxAttDbForTx{0};

    // Stash-valid flag for m_savedRxAttDbForTx (#175 PR #194 review fix,
    // 2026-05-04).  Only true between an RX→TX transition that actually
    // populated the stash (m_attOnTxEnabled path on a non-HPSDR board) and
    // the matching TX→RX restore.  Codex review found that without this
    // gate, the TX→RX restore ran unconditionally and clobbered the user's
    // RX att value with the default-zero stash whenever ATT-on-TX was OFF.
    bool m_savedRxAttDbValid{false};

    // Internal tick timer.
    QTimer m_tickTimer;

    // RX1 (second ADC) preamp; see rx1Preamp().
    bool m_rx1Preamp{false};

    // Opt-in debounced save; see setDebouncedSaveEnabled().
    bool m_debouncedSave{false};
    // Opt-in band-restore push; see setBandRestoreToRadio().
    bool m_bandRestoreToRadio{false};
    QTimer m_saveTimer;

    // RadioConnection for adcOverflow wiring.
    QPointer<RadioConnection> m_connection;
    QMetaObject::Connection m_adcOverflowConn;

    // ReceiverManager for DDC mapping.
    QPointer<ReceiverManager> m_receiverManager;

    // ADC-linked state (both RX0 and RX1 share the same ADC).
    bool m_adcLinked{false};

    // R-R3-46 / R-R3-11: the per-ADC receive attenuators (setAdcRouting).
    int m_rx1Adc{0};
    int m_rx2Adc{-1};
    bool m_adcAttLinked{false};
    quint32 m_rx2SliceMask{0};
    Band m_rx2Band{Band::GEN};
    int m_rx2AttDb{0};
    // A routing or band change while keyed, sent on the fall.
    bool m_adcSendsHeldForMox{false};
    // Thetis rx2_step_attenuator_by_band: the other ADC's band memory.
    std::unordered_map<int, int> m_rx2BandAttDb;
    // What each receive ADC holds: in use, and its attenuation.
    struct AdcAttSnapshot {
        std::array<bool, kMaxAdcs> inUse{};
        std::array<int, kMaxAdcs> dB{};
    };
    AdcAttSnapshot adcAttSnapshot() const;
    // Send each ADC in use whose value or use differs from `before`.
    void sendAdcAttenuatorChanges(const AdcAttSnapshot& before);
    // Send RX1's value to slice A's ADC, and to the other ADC while linked.
    // On a known board: nothing while the step attenuator is off, and the
    // Alex attenuator plus the value + 2 above 31 dB on an Alex board.
    void sendRx1Attenuation(int dB);
    // Level Cal: what one preamp mode sends (Thetis RX1PreampMode setter).
    struct PreampDrive {
        int attDb{0};
        bool mercPreamp{false};
        int alexAtten{0};
    };
    static PreampDrive preampDriveFor(PreampMode mode) noexcept;
    bool isHpsdrModel() const noexcept;
    // Thetis's Alex list for the step attenuator above 31 dB.
    bool stepAttAlexEligible() const noexcept;
    // Alex settings become Off on a known board without Alex.
    PreampMode clampPreampForBoard(PreampMode mode) const noexcept;
    void recomputeMaxAtt();
    // The step attenuator value RX1's ADC receives for dB.
    int rx1WireAttDbFor(int dB) const noexcept;
    // attenuatorDbForAdc as it goes on the wire.
    int wireAttDbForAdc(int adc) const noexcept;
    // Send the current preamp mode's drive.
    void applyPreampDrive();
    void sendAlexAtten(int bits);
    // Send the other ADC's own value to it (nothing while linked or unused).
    void sendRx2Attenuation();
    void sendAttenuatorToAdc(int adc, int dB);
    // The other ADC's band follows its controlling slice (setAdcRouting).
    void setRx2Band(Band band);
    // Thetis's RX2 auto-attenuate on the other ADC in use (tick()).
    void runRx2AutoAtt(bool overloaded);
    // Drop the other ADC's auto-attenuate state, restoring its value.
    // RX2's auto-attenuate history (Thetis _historic_attenuator_readings_rx2):
    // the value before each raise, unwound one per undo. Thetis's
    // HistoricAttenuatorReading: stepAttenuator -1 and preampMode FIRST
    // (empty here) when not taken.
    struct Rx2AttReading {
        int stepAttenuator{-1};
        std::optional<PreampMode> preampMode;
    };
    std::vector<Rx2AttReading> m_rx2AutoAttHistory;
    qint64 m_rx2LastAutoAttTimeMs{0};
    // RX2's own enable and auto-attenuate settings (Thetis
    // _rx2_step_att_enabled, _auto_att_rx2, _auto_att_undo_rx2,
    // _auto_att_hold_delay_rx2).
    bool m_rx2StepAttEnabled{false};  // Thetis console.cs:11109
    bool m_rx2AutoAttEnabled{false};
    bool m_rx2AutoUndoEnabled{false};
    int m_rx2AutoUndoDelaySec{5};
    // Whether RX2 is on an ADC of its own (not slice A's, not linked).
    bool rx2OnItsOwnAdc() const noexcept;

    // Level Cal: RX2's preamp mode (Thetis rx2_preamp_mode) and its band
    // memory (rx2_preamp_by_band, HPSDR_ON on every band at start,
    // console.cs:1814 [v2.10.3.15]).
    PreampMode m_rx2PreampMode{PreampMode::On};
    std::unordered_map<int, PreampMode> m_rx2BandPreamp;
    // Thetis temp_mode2: RX2's mode held over an HPSDR transmit.
    PreampMode m_savedRx2PreampMode{PreampMode::On};
    // Thetis _setFromOtherAttenuator: one mode setting the other.
    bool m_setFromOtherPreamp{false};
    // What one RX2 preamp mode sends (the RX2PreampMode setter's switch).
    struct Rx2PreampDrive {
        int attDb{0};
        bool preamp{false};
    };
    static Rx2PreampDrive rx2PreampDriveFor(PreampMode mode) noexcept;
    // The models whose RX2 mode drives the other ADC's step attenuator.
    bool rx2PreampDrivesAdc() const noexcept;
    // Thetis _rx2_preamp_present for the connected model.
    bool rx2PreampPresent() const noexcept;
    // Send RX2's mode drive (the RX2PreampMode setter's sends).
    void applyRx2PreampDrive();

    // --- Helpers ---
    void applyClassicAutoAtt(int adc);
    void applyClassicUndo();
    void applyAdaptiveAutoAtt(int adc);
    void setAutoAttApplied(bool applied);
    OverloadLevel levelToSeverity(int level) const;
    void checkAdcLinked();

    // TX-path helpers (F.2).

    // Look up the per-band TX ATT value. Returns m_txAttByBand[band] or 0
    // if band is out of range.
    // From Thetis console.cs:48012-48017 [v2.10.3.13] getTXstepAttenuatorForBand.
    int applyTxAttenuationForBand(Band band) const;

    // HPSDR-only: cache the current preamp mode before TX.
    // From Thetis console.cs:29550 [v2.10.3.13]: temp_mode = RX1PreampMode.
    void saveRxPreampMode();

    // HPSDR-only: restore the cached preamp mode after TX.
    // From Thetis console.cs:29550 [v2.10.3.13]: RX1PreampMode = temp_mode.
    void restoreRxPreampMode();

private slots:
    void onDdcMappingChanged();

#ifdef NEREUS_BUILD_TESTS
public:
    // Test seams — expose internal TX-path state for white-box unit tests.
    PreampMode savedPreampModeForTest() const noexcept { return m_savedPreampMode; }
    PreampMode savedRx2PreampModeForTest() const noexcept { return m_savedRx2PreampMode; }
    int txAttByBandForTest(Band band) const { return applyTxAttenuationForBand(band); }
    // Expose the last TX ATT value pushed to hardware (via m_lastTxStepAttDb).
    int lastTxStepAttForTest() const noexcept { return m_lastTxStepAttDb; }
private:
    int m_lastTxStepAttDb{-1};  // set by onMoxHardwareFlipped; -1 = never called
#endif
};

}  // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::OverloadLevel)
Q_DECLARE_METATYPE(NereusSDR::PreampMode)
Q_DECLARE_METATYPE(NereusSDR::AutoAttMode)
