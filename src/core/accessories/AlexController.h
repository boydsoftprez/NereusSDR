// =================================================================
// src/core/accessories/AlexController.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/HPSDR/Alex.cs:30-106
//   (TxAnt[]/RxAnt[]/RxOnlyAnt[] per-band antenna arrays,
//    LimitTXRXAntenna / SetAntennasTo1 external-ATU compat mode,
//    setRxAnt / setRxOnlyAnt / setTxAnt per-band setters)
//
// Ported from mi0bot/OpenHPSDR-Thetis sources [@c26a8a4]:
//   Project Files/Source/Console/HPSDR/Alex.cs:310-446
//   (UpdateAlexAntSelection's HERMESLITE branches: the I/O board aerials)
//   Project Files/Source/Console/console.cs:25616-25637
//   (SetIOBoardAerialPorts: IOBoardAerialMode / IOBoardAerialPorts)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Replaces Phase 3I Alex stubs. Per-MAC
//                persistence via AppSettings. NereusSDR spin: 14 bands
//                (Band160m–XVTR) vs Thetis's 12 (B160M–B6M); extra
//                GEN/WWV/XVTR slots default to Ant 1. Block-TX safety
//                (blockTxAnt2/3) added as NereusSDR-native UI contract
//                on top of the core Thetis model.
//   2026-09-24 : R-R3-46 / R-R3-21: bpfModeChanged, so a filter policy
//                change is saved for the radio at once. NereusSDR-original
//                (the per-ADC BPF policy has no Thetis equivalent). J.J.
//                Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 : Plan Task 14 re-review N4 (Phase 3F section 16.4):
//                SwitchBypass, so the per-chain effective state reports the
//                Alex tab's HPF Bypass (master) and Disable 6m LNA on RX
//                when they put the bypass on the wire. NereusSDR-original.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 : Task 14 follow-up 2 (Phase 3F section 16.4): the keyed
//                SwitchBypass causes (HPF Bypass on TX, on PureSignal
//                feedback, Disable 6m LNA on TX), reported while they put
//                the bypass on the wire. NereusSDR-original. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 2: the I/O board aerial values
//                (hl2IoBoardAerials, ioBoardAerialPorts), ported from the
//                mi0bot fork's Alex.cs HERMESLITE branches and console.cs
//                SetIOBoardAerialPorts. J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-30 - Shared-input filters (ruling (d)): AlexAdcState carries the
//                low-pass reason and the slice that forces it
//                (setLowPassHold). NereusSDR-original. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30 - SwitchBypass::NoFilterPins, with the slice it names: the
//                HL2's N2ADR pins sent are 0x00 (JJ's ruling).
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-30 - lowPassHoldChanged: setLowPassHold no longer emits
//                bpfStateChanged, so a hold alone sends a peer without
//                rxFilterLowPass no delta. NereusSDR-original. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
//
// === Verbatim Thetis Console/HPSDR/Alex.cs header (lines 1-23) ===
/*
*
* Copyright (C) 2008 Bill Tracey, KD5TFD, bill@ewjt.com
* Copyright (C) 2010-2020  Doug Wigley
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
//
// this module contains code to support the Alex Filter and Antenna Selection board
//
//
// =================================================================
//
// (mi0bot/OpenHPSDR-Thetis fork)
// --- From Console/console.cs ---
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
// =================================================================

#pragma once

#include <QObject>
#include <QString>
#include <array>
#include "models/Band.h"

namespace NereusSDR {

// Per-band antenna assignment (TX / RX1 / RX-only) + Block-TX safety.
//
// Source: HPSDR/Alex.cs:30-106 [@501e3f5]
//
// Thetis stores 12-band arrays indexed as (int)band - (int)Band.B160M.
// NereusSDR uses Band::Count=14 bands (adds GEN/WWV/XVTR); those slots
// default to Ant 1 like all bands.
//
// Block-TX toggles (blockTxAnt2 / blockTxAnt3) are a NereusSDR addition —
// safety guards for the Antenna Control UI: antenna ports wired RX-only
// should not accept TX assignments.
class AlexController : public QObject {
    Q_OBJECT

public:
    explicit AlexController(QObject* parent = nullptr);

    // ── Phase 3F: per-ADC BPF state types ────────────────────────────────────
    // NereusSDR-original; no Thetis port.
    // Per docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §4.

    /// Operator preference for BPF state on this ADC.
    enum class BpfMode {
        Auto,        ///< default: filter when single-band, bypass on multi-band
        ForceBand,   ///< always filter to TX-bound slice's band (warn OOB attenuation)
        ForceBypass  ///< always bypass BPF (operator wideband or noise hunting)
    };

    /// Effective BPF state (what's actually on the wire).
    enum class BpfEffective {
        Filtered,         ///< BPF engaged at currentBpfBand
        Bypass,           ///< BPF in bypass (no per-band rejection)
        WidebandLocked    ///< BPF bypassed due to wideband stream active on this ADC
    };

    /// An Alex tab switch that puts the bypass on this chain's wire over a
    /// band selection the policy left filtered (plan Task 14 re-review N4).
    /// RadioModel decides it, from the switches, the board and the chain's
    /// selection, exactly as the connection applies them.
    enum class SwitchBypass {
        None,
        HpfBypass,         ///< "HPF Bypass (master)": the bypass on any band
        Disable6mLnaOnRx,  ///< "Disable 6m LNA on RX": the bypass for the 6 m BPF/LNA
        // The keyed arms (Task 14 follow-up 2): on the wire only while keyed.
        HpfBypassOnTx,     ///< "HPF Bypass on TX": the bypass on any band while keyed
        PureSignalTx,      ///< "HPF Bypass on PureSignal feedback": keyed with PureSignal,
                           ///< on the band-pass boards
        Disable6mLnaOnTx,  ///< "Disable 6m LNA on TX": the bypass for the 6 m BPF/LNA
                           ///< while keyed
        // JJ's ruling of 2026-09-30: on the HL2 the N2ADR receive pins sent
        // are 0x00 because the band of the slice they follow has no pins
        // set, so the filter board is off (SharedInputLowPass::hl2ReceivePins,
        // the byte the connection sends).
        NoFilterPins       ///< the HL2's receive pins are all off
    };

    /// Per-ADC state computed by recomputeBpf().
    struct AlexAdcState {
        BpfMode      mode {BpfMode::Auto};
        BpfEffective effective {BpfEffective::Filtered};
        Band         currentBpfBand {Band::Band20m};
        QString      reasonText;  ///< for WIDE badge tooltip + bottom-bar status
        /// Not None when effective is Bypass only because of that switch:
        /// the policy's own answer was Filtered, the band's selection is
        /// still what RadioModel sends, and the connection turns it into
        /// the bypass on the wire.
        SwitchBypass bypassSwitch {SwitchBypass::None};
        /// Shared-input filters, ruling (d) 2026-09-30: when the slices
        /// counted on this chain's input need more than one receive
        /// low-pass, the one sent follows the slice on the highest band,
        /// and this says so in plain words. Empty otherwise. RadioModel
        /// decides it (republishAlexAdcSlices); recomputeBpf leaves it.
        QString lowPassReason;
        /// The slice whose band sets that low-pass (the slice the others on
        /// the input are held to), -1 when no slice forces it.
        int lowPassSlice {-1};
    };

    // ── Per-ADC BPF mode mutators + recompute ────────────────────────────────
    // NereusSDR-original; no Thetis port.

    BpfMode bpfMode(int adc) const;
    void    setBpfMode(int adc, BpfMode mode);
    const AlexAdcState& adcState(int adc) const;

    /// Recompute BPF state for the given ADC based on current slice list, wideband
    /// state, and operator mode. Emits bpfStateChanged when effective state changes.
    void recomputeBpf(int adc);

    /// Plan Task 14 re-review N4: an Alex tab switch bypasses this chain on
    /// the wire (see SwitchBypass). Recomputes; reported only where the
    /// policy's own answer is Filtered.
    /// `detail` names what the cause is about where the cause needs it:
    /// for NoFilterPins, the slice whose band has no pins ("slice B on WWV").
    void setSwitchBypass(int adc, SwitchBypass cause, const QString& detail = QString());

    /// Shared-input filters, ruling (d): the low-pass reason and the slice
    /// that forces it (AlexAdcState::lowPassReason, lowPassSlice). Emits
    /// lowPassHoldChanged, not bpfStateChanged, when either changes: the
    /// band-pass state did not move.
    void setLowPassHold(int adc, int sliceIndex, const QString& reason);

    /// Mark that a wideband stream is active on this ADC.
    /// Recomputes BPF (wideband forces effective=WidebandLocked).
    void setWidebandActive(int adc, bool on);

    /// Phase 3F: called by RadioModel when the slice list on this ADC changes
    /// (slice add/remove/retune-cross-band). Drives the auto-mode recompute.
    /// Use Band::Count as the sentinel for "no slice in this position".
    void notifySlicesOnAdc(int adc, const std::array<Band, 5>& slicesOnAdc);

    // ── Per-band antenna selection (1, 2, or 3) ──────────────────────────────
    // Source: HPSDR/Alex.cs:56-58 TxAnt/RxAnt/RxOnlyAnt fields [@501e3f5]
    int  txAnt(Band band) const;
    int  rxAnt(Band band) const;
    int  rxOnlyAnt(Band band) const;  // 1 = rx1, 2 = rx2, 3 = xv, 0 = none selected [Alex.cs:58]
    void setTxAnt(Band band, int ant);     // clamps to [1, 3]; rejected if blockTxAntN
    void setRxAnt(Band band, int ant);
    void setRxOnlyAnt(Band band, int ant);

    // ── Block-TX safety ──────────────────────────────────────────────────────
    // NereusSDR UI contract: when true, setTxAnt() rejects assignment to that port.
    bool blockTxAnt2() const;
    bool blockTxAnt3() const;
    void setBlockTxAnt2(bool on);
    void setBlockTxAnt3(bool on);

    // Forces every band's TX/RX antenna to port 1 (external-ATU compat).
    // Source: HPSDR/Alex.cs SetAntennasTo1(bool IsSetTo1) [@501e3f5]
    // "SetAntennasTo1 causes RX, TX antennas to be set to 1 — the various RX bypass unaffected"
    void setAntennasTo1(bool force);

    // ── TX-bypass routing flags ──────────────────────────────────────────────
    // Ported from Thetis HPSDR/Alex.cs:61-66 [v2.10.3.13 @501e3f5].
    // Thetis declares these as `public static bool` on the Alex class; NereusSDR
    // scopes them per-instance so per-radio state is preserved.
    //
    // Mutual-exclusion trio (rxOutOnTx / ext1OutOnTx / ext2OutOnTx): setting any
    // one true clears the other two. Matches Thetis setup.cs:15423-15427
    // handlers (chkRxOutOnTx_CheckedChanged / chkEXT1OutOnTx_CheckedChanged /
    // chkEXT2OutOnTx_CheckedChanged).
    //
    // rxOutOverride (Thetis: rx_out_override) — chkDisableRXOut.Checked.
    //   Source: setup.cs:17568-17570 chkDisableRXOut_CheckedChanged.
    // useTxAntForRx (Thetis: TRxAnt) — when true, RX path uses TxAnt[band]
    //   instead of RxAnt[band]. Source: Alex.cs:363-364.
    // xvtrActive — NereusSDR-native session flag. Not in Thetis (Thetis
    //   passes `xvtr` as a method parameter; NereusSDR stores it as state
    //   for signal-driven reapply). Not persisted.
    bool rxOutOnTx() const;
    bool ext1OutOnTx() const;
    bool ext2OutOnTx() const;
    bool rxOutOverride() const;
    bool useTxAntForRx() const;
    bool xvtrActive() const;

    // ── HL2 I/O board aerial values ──────────────────────────────────────────
    // The Hermes Lite 2 I/O board's aerial mode (REG_RF_INPUTS) and aerial
    // ports (REG_ANTENNA) bytes, which the I/O board poll writes
    // (P1RadioConnection::setIoBoardAerials). mode is IOBoardAerialMode,
    // ports IOBoardAerialPorts.
    struct IoBoardAerials {
        quint8 mode {0};
        quint8 ports {0};
        bool operator==(const IoBoardAerials&) const = default;
    };

    // From mi0bot console.cs:25616-25637 SetIOBoardAerialPorts [@c26a8a4]:
    // the mode from rx_only_ant, the ports from the RX and TX aerials
    // (0-based, as Alex.cs passes trx_ant - 1 and tx_ant - 1).
    // // MI0BOT: Control I/O Board's aerial capabilities
    // // MI0BOT: Control I/O Board's Alt Rx aerial facility
    static IoBoardAerials ioBoardAerialPorts(int rxOnlyAnt, int rxAnt, int txAnt);

    // From mi0bot HPSDR/Alex.cs:310-460 UpdateAlexAntSelection [@c26a8a4],
    // with its HERMESLITE branches: the rx_only_ant, trx_ant and tx_ant it
    // hands to SetIOBoardAerialPorts. `band` is the current band (the TX
    // aerial); `rxBand` the band whose receive aerials are in use (the kept
    // band, as RadioModel::applyAlexAntennaForBand reads them).
    // Upstream author tags in the range, carried at each step in the .cpp:
    // // MI0BOT: Alt RX has been requested or TX and Rx are not the same
    // // MI0BOT: Antenna not the same is valid
    // //G8NJJ (Aries clamp, deferred)
    // // MI0BOT: Transmit antenna is being used for reception in split aerial operation
    // // MI0BOT: Sets the aerial controls on the I/O board
    IoBoardAerials hl2IoBoardAerials(Band band, Band rxBand, bool isTx) const;

public slots:
    void setRxOutOnTx(bool on);
    void setExt1OutOnTx(bool on);
    void setExt2OutOnTx(bool on);
    void setRxOutOverride(bool on);
    void setUseTxAntForRx(bool on);
    void setXvtrActive(bool on);

public:
    // ── Persistence ──────────────────────────────────────────────────────────
    void setMacAddress(const QString& mac);
    void load();   // hydrate from AppSettings under hardware/<mac>/alex/antenna/...
    void save();   // persist current state to AppSettings

signals:
    void bpfStateChanged(int adc, const AlexController::AlexAdcState& state);
    /// setLowPassHold changed `adc`'s lowPassReason or lowPassSlice.
    void lowPassHoldChanged(int adc);
    /// R-R3-46 / R-R3-21: setBpfMode changed `adc`'s operator policy. Fires
    /// even when the effective state did not change (a wideband chain), so
    /// the policy is saved and published whatever it does to the filter.
    void bpfModeChanged(int adc);
    void antennaChanged(Band band);  // fires on any per-band assignment mutation
    void blockTxChanged();           // fires when blockTxAnt2 or blockTxAnt3 changes
    void rxOutOnTxChanged(bool on);
    void ext1OutOnTxChanged(bool on);
    void ext2OutOnTxChanged(bool on);
    void rxOutOverrideChanged(bool on);
    void useTxAntForRxChanged(bool on);
    void xvtrActiveChanged(bool on);
    void rxOnlyAntChanged(Band band);  // fires independently of antennaChanged() for finer RX-only UI refresh

private:
    // From Thetis HPSDR/Alex.cs:56-58 [@501e3f5] — Thetis uses 12 bands
    // (B160M..B6M); NereusSDR uses 14 (adds GEN/WWV/XVTR).
    //
    // Alex antenna routing applies to HF amateur + GEN/WWV/XVTR only.
    // The Phase 3L Band enum extension (Band::SwlFirst..SwlLast =
    // 13 SWL bands for HL2 N2ADR Filter visibility) does NOT participate
    // in Alex routing: SWL bands inherit ham-band antenna assignments
    // because the HL2 RJ45 antenna jack is the same regardless of which
    // SWL slice you tune to.  Iteration stops at the SWL boundary
    // (Band::SwlFirst == 14) to preserve existing per-band-array
    // semantics + signal emission counts.
    //
    // 2 m (R-IOS-26, R-R3-49) has its own antennas, as in Thetis, whose
    // TxAnt/RxAnt/RxOnlyAnt hold 12 bands, B160M .. B2M (Alex.cs:56-58,
    // idx = (int)band - (int)Band.B160M [v2.10.3.15]). The arrays hold the
    // per-band state slots (Band.h): 2 m is slot 14.
    static constexpr int kBandCount = kPerBandStateCount;  // 15

    std::array<int, kBandCount> m_txAnt{};      // TxAnt[12] in Thetis
    std::array<int, kBandCount> m_rxAnt{};      // RxAnt[12] in Thetis
    std::array<int, kBandCount> m_rxOnlyAnt{};  // RxOnlyAnt[12] in Thetis
    bool m_blockTxAnt2{false};
    bool m_blockTxAnt3{false};
    bool m_rxOutOnTx     {false};
    bool m_ext1OutOnTx   {false};
    bool m_ext2OutOnTx   {false};
    bool m_rxOutOverride {false};
    bool m_useTxAntForRx {false};
    bool m_xvtrActive    {false};

    std::array<AlexAdcState, 2> m_perAdcState{};
    std::array<bool, 2> m_widebandActive {false, false};
    std::array<SwitchBypass, 2> m_switchBypass {SwitchBypass::None, SwitchBypass::None};
    std::array<QString, 2> m_switchBypassDetail;
    // Phase 3F: slice band per ADC — sentinel Band::Count means "no slice in this slot".
    // Initialized in ctor so all slots start at Band::Count (not Band::Band160m = 0).
    std::array<std::array<Band, 5>, 2> m_slicesPerAdc;

    QString m_mac;

    QString persistenceKey() const;  // "hardware/<mac>/alex/antenna"
    static int clampAnt(int v) { return v < 1 ? 1 : (v > 3 ? 3 : v); }
    static int clampRxOnlyAnt(int v) { return v < 0 ? 0 : (v > 3 ? 3 : v); }  // allows 0 = none
};

} // namespace NereusSDR
