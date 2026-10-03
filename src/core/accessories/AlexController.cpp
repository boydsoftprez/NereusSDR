// =================================================================
// src/core/accessories/AlexController.cpp  (NereusSDR)
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
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 2: the I/O board aerial values
//                (hl2IoBoardAerials, ioBoardAerialPorts), ported from the
//                mi0bot fork's Alex.cs HERMESLITE branches and console.cs
//                SetIOBoardAerialPorts. J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-30 - Shared-input filters (ruling (d)): setLowPassHold.
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-30 - SwitchBypass::NoFilterPins: the HL2's N2ADR pins sent are
//                0x00 (JJ's ruling). NereusSDR-original. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - setLowPassHold emits lowPassHoldChanged in place of
//                bpfStateChanged. NereusSDR-original. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
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

#include "AlexController.h"
#include "core/AppSettings.h"
#include <set>
#include <QStringList>

namespace NereusSDR {

// Source: HPSDR/Alex.cs:Alex() ctor — "for(int i=0;i<12;i++) { TxAnt[i]=1; RxAnt[i]=1; RxOnlyAnt[i]=0; }" [v2.10.3.13 @501e3f5]
// NereusSDR: Tx/RxAnt default to 1 (the standard ant port); RxOnlyAnt defaults
// to 0 per Thetis ("none selected") so the RX-only bypass relay stays off
// until the user picks a port. Bands GEN/WWV/XVTR add 2 extra slots.
AlexController::AlexController(QObject* parent) : QObject(parent)
{
    m_txAnt.fill(1);
    m_rxAnt.fill(1);
    m_rxOnlyAnt.fill(0);  // 0 = none selected — matches Thetis Alex.cs:59 [v2.10.3.13 @501e3f5]
    // Phase 3F: initialize slice-per-ADC arrays to Band::Count sentinel ("no slice in slot")
    for (auto& perAdc : m_slicesPerAdc) { perAdc.fill(Band::Count); }
}

// ── Per-ADC BPF mode mutators + recompute ──────────────────────────────────
// NereusSDR-original; no Thetis port.

AlexController::BpfMode AlexController::bpfMode(int adc) const
{
    if (adc < 0 || adc >= 2) { return BpfMode::Auto; }
    return m_perAdcState[adc].mode;
}

void AlexController::setBpfMode(int adc, BpfMode mode)
{
    if (adc < 0 || adc >= 2) { return; }
    if (m_perAdcState[adc].mode == mode) { return; }
    m_perAdcState[adc].mode = mode;
    recomputeBpf(adc);  // emits bpfStateChanged when effective changes
    emit bpfModeChanged(adc);
}

const AlexController::AlexAdcState& AlexController::adcState(int adc) const
{
    static const AlexAdcState empty{};
    if (adc < 0 || adc >= 2) { return empty; }
    return m_perAdcState[adc];
}

void AlexController::setWidebandActive(int adc, bool on)
{
    if (adc < 0 || adc >= 2) { return; }
    if (m_widebandActive[adc] == on) { return; }
    m_widebandActive[adc] = on;
    recomputeBpf(adc);
}

// Plan Task 14 re-review N4 (Phase 3F design section 16.4.1): WIDE means
// the chain is bypassed on the wire. An Alex tab switch can put the bypass
// there over a filtered band, so the chain's state has to know about it.
void AlexController::setSwitchBypass(int adc, SwitchBypass cause, const QString& detail)
{
    if (adc < 0 || adc >= 2) { return; }
    if (m_switchBypass[adc] == cause && m_switchBypassDetail[adc] == detail) { return; }
    m_switchBypass[adc] = cause;
    m_switchBypassDetail[adc] = detail;
    recomputeBpf(adc);
}

// Shared-input filters, ruling (d) 2026-09-30. NereusSDR-original: Thetis
// has one low-pass rule and no way to report it.
void AlexController::setLowPassHold(int adc, int sliceIndex, const QString& reason)
{
    if (adc < 0 || adc >= 2) { return; }
    AlexAdcState& s = m_perAdcState[adc];
    const int slice = reason.isEmpty() ? -1 : sliceIndex;
    if (s.lowPassSlice == slice && s.lowPassReason == reason) { return; }
    s.lowPassSlice = slice;
    s.lowPassReason = reason;
    // Its own signal: the band-pass did not move, and RadioModel publishes
    // the hold under a notifier of its own (lowPassHoldChanged), so a peer
    // without rxFilterLowPass is sent nothing for it.
    emit lowPassHoldChanged(adc);
}

// Phase 3F: slice-aware recompute trigger.
// Band::Count is the sentinel for "no slice in this position".
void AlexController::notifySlicesOnAdc(int adc, const std::array<Band, 5>& slicesOnAdc)
{
    if (adc < 0 || adc >= 2) { return; }
    m_slicesPerAdc[adc] = slicesOnAdc;
    recomputeBpf(adc);
}

void AlexController::recomputeBpf(int adc)
{
    if (adc < 0 || adc >= 2) { return; }

    AlexAdcState& s = m_perAdcState[adc];
    AlexAdcState prev = s;

    // Priority order: wideband > operator force-bypass > operator force-band > auto.
    if (m_widebandActive[adc]) {
        s.effective = BpfEffective::WidebandLocked;
        s.reasonText = QStringLiteral("BYPASS (wideband active)");
    } else if (s.mode == BpfMode::ForceBypass) {
        s.effective = BpfEffective::Bypass;
        s.reasonText = QStringLiteral("BYPASS (operator override)");
    } else if (s.mode == BpfMode::ForceBand) {
        s.effective = BpfEffective::Filtered;
        s.reasonText = QStringLiteral("%1 (forced)").arg(bandLabel(s.currentBpfBand));
    } else {
        // Auto mode: inspect slice bands on this ADC.
        // 0 bands -> idle/filtered; 1 band -> Filtered at that band;
        // 2+ distinct bands -> BYPASS (multi-band attenuation trade-off).
        // Phase 3F Task 13 — NereusSDR-original; no Thetis port.
        std::set<Band> uniqueBands;
        for (Band b : m_slicesPerAdc[adc]) {
            if (b != Band::Count) { uniqueBands.insert(b); }
        }

        if (uniqueBands.empty()) {
            s.effective = BpfEffective::Filtered;
            s.reasonText = QStringLiteral("%1 (idle)").arg(bandLabel(s.currentBpfBand));
        } else if (uniqueBands.size() == 1) {
            s.currentBpfBand = *uniqueBands.begin();
            s.effective = BpfEffective::Filtered;
            s.reasonText = bandLabel(s.currentBpfBand);
        } else {
            // 2+ distinct bands on this ADC -> BYPASS
            s.effective = BpfEffective::Bypass;
            QStringList bandList;
            for (Band b : uniqueBands) { bandList << bandLabel(b); }
            s.reasonText = QStringLiteral("BYPASS (multi-band: %1)").arg(bandList.join(QStringLiteral(" + ")));
        }
    }

    // An Alex tab switch bypasses a chain the policy left filtered (plan
    // Task 14 re-review N4). A chain already bypassed or wide is on the
    // bypass anyway, and keeps its own reason.
    s.bypassSwitch = SwitchBypass::None;
    if (s.effective == BpfEffective::Filtered && m_switchBypass[adc] != SwitchBypass::None) {
        s.effective = BpfEffective::Bypass;
        s.bypassSwitch = m_switchBypass[adc];
        switch (m_switchBypass[adc]) {
        case SwitchBypass::HpfBypass:
            s.reasonText = QStringLiteral("BYPASS (HPF Bypass setting)");
            break;
        case SwitchBypass::Disable6mLnaOnRx:
            s.reasonText = QStringLiteral("BYPASS (6m LNA off on RX)");
            break;
        // Task 14 follow-up 2: the keyed arms, on the wire while keyed.
        case SwitchBypass::HpfBypassOnTx:
            s.reasonText = QStringLiteral("BYPASS (HPF Bypass on TX)");
            break;
        case SwitchBypass::PureSignalTx:
            s.reasonText = QStringLiteral("BYPASS (PureSignal TX)");
            break;
        case SwitchBypass::Disable6mLnaOnTx:
            s.reasonText = QStringLiteral("BYPASS (6m LNA off on TX)");
            break;
        case SwitchBypass::NoFilterPins:
            s.reasonText = m_switchBypassDetail[adc].isEmpty()
                ? QStringLiteral("BYPASS (no filter pins)")
                : QStringLiteral("BYPASS (%1 has no filter pins)").arg(m_switchBypassDetail[adc]);
            break;
        case SwitchBypass::None:
            break;
        }
    }

    if (s.effective != prev.effective || s.reasonText != prev.reasonText) {
        emit bpfStateChanged(adc, s);
    }
}

// Source: HPSDR/Alex.cs:setTxAnt / TxAnt[] accessor [@501e3f5]
int AlexController::txAnt(Band band) const
{
    const int b = perBandStateSlot(band);
    return (b >= 0 && b < kBandCount) ? m_txAnt[b] : 1;
}

// Source: HPSDR/Alex.cs:setRxAnt / RxAnt[] accessor [@501e3f5]
int AlexController::rxAnt(Band band) const
{
    const int b = perBandStateSlot(band);
    return (b >= 0 && b < kBandCount) ? m_rxAnt[b] : 1;
}

// Source: HPSDR/Alex.cs:setRxOnlyAnt / RxOnlyAnt[] accessor [@501e3f5]
// Out-of-bounds sentinel returns 0 ("none selected") to match the in-range
// default; returning 1 would silently activate the RX-bypass relay.
int AlexController::rxOnlyAnt(Band band) const
{
    const int b = perBandStateSlot(band);
    return (b >= 0 && b < kBandCount) ? m_rxOnlyAnt[b] : 0;
}

// Source: HPSDR/Alex.cs:setTxAnt(Band band, byte ant) [@501e3f5]
// Original: "if(ant>3){ant=1;} idx=(int)band-(int)Band.B160M; TxAnt[idx]=ant;"
// NereusSDR: clampAnt(v) handles both low (0→1) and high (>3→3) clamping.
//            Block-TX safety guards added for UI contract.
void AlexController::setTxAnt(Band band, int ant)
{
    const int b = perBandStateSlot(band);
    if (b < 0 || b >= kBandCount) { return; }
    const int newAnt = clampAnt(ant);
    // Block-TX safety: reject TX assignment to a blocked port.
    if ((newAnt == 2 && m_blockTxAnt2) || (newAnt == 3 && m_blockTxAnt3)) { return; }
    if (m_txAnt[b] == newAnt) { return; }
    m_txAnt[b] = newAnt;
    emit antennaChanged(band);
}

// Source: HPSDR/Alex.cs:setRxAnt(Band band, byte ant) [@501e3f5]
// Original: "if(ant>3){ant=1;} idx=(int)band-(int)Band.B160M; RxAnt[idx]=ant;"
void AlexController::setRxAnt(Band band, int ant)
{
    const int b = perBandStateSlot(band);
    if (b < 0 || b >= kBandCount) { return; }
    const int newAnt = clampAnt(ant);
    if (m_rxAnt[b] == newAnt) { return; }
    m_rxAnt[b] = newAnt;
    emit antennaChanged(band);
}

// Source: HPSDR/Alex.cs:setRxOnlyAnt(Band band, byte ant) [@501e3f5]
// Original: "if(ant>3){//ant=0;} idx=(int)band-(int)Band.B160M; RxOnlyAnt[idx]=ant;"
// 1 = rx1, 2 = rx2, 3 = xv, 0 = none selected  [Alex.cs:58]
// Uses clampRxOnlyAnt (allows 0) instead of clampAnt — fix for 3P-I-b T3.3.
void AlexController::setRxOnlyAnt(Band band, int ant)
{
    const int b = perBandStateSlot(band);
    if (b < 0 || b >= kBandCount) { return; }
    const int newAnt = clampRxOnlyAnt(ant);
    if (m_rxOnlyAnt[b] == newAnt) { return; }
    m_rxOnlyAnt[b] = newAnt;
    emit antennaChanged(band);
    emit rxOnlyAntChanged(band);
}

bool AlexController::blockTxAnt2() const { return m_blockTxAnt2; }
bool AlexController::blockTxAnt3() const { return m_blockTxAnt3; }

// Source: Thetis setup.cs:18745-18766 chkBlockTxAnt2_CheckedChanged +
// setup.cs:13237 radAlexR_160_CheckedChanged branch [v2.10.3.13 @501e3f5].
// When Block-TX-ANT2 flips ON, Thetis walks every band and clamps any
// radAlexT2_*.Checked band back to radAlexT1_*. Without the retroactive
// sweep the flag is a write-time guard only — existing per-band
// txAnt=2 values keep firing on transmit until the user re-picks.
// Flagged by Codex review on PR #116 (NereusSDR bench pass 2026-04-22).
void AlexController::setBlockTxAnt2(bool on)
{
    if (m_blockTxAnt2 == on) { return; }
    m_blockTxAnt2 = on;
    if (on) {
        for (int b = 0; b < kBandCount; ++b) {
            if (m_txAnt[b] == 2) {
                m_txAnt[b] = 1;
                emit antennaChanged(bandFromPerBandStateSlot(b));
            }
        }
    }
    emit blockTxChanged();
}

// Same rationale as setBlockTxAnt2 — Thetis sets radAlexT1_* checked
// when radAlexT3_* was checked and the block toggles on
// (setup.cs:13248). Mirror the retroactive clamp for ANT3.
void AlexController::setBlockTxAnt3(bool on)
{
    if (m_blockTxAnt3 == on) { return; }
    m_blockTxAnt3 = on;
    if (on) {
        for (int b = 0; b < kBandCount; ++b) {
            if (m_txAnt[b] == 3) {
                m_txAnt[b] = 1;
                emit antennaChanged(bandFromPerBandStateSlot(b));
            }
        }
    }
    emit blockTxChanged();
}

// Source: HPSDR/Alex.cs:SetAntennasTo1(bool IsSetTo1) [v2.10.3.13 @501e3f5]
// Thetis original: "LimitTXRXAntenna = IsSetTo1;" — a flag consulted by
// UpdateAlexAntSelection (Alex.cs:381-382) that clamps trx_ant to 1 at
// composition time. The method comment is explicit: "SetAntennasTo1 causes
// RX, TX antennas to be set to 1 — the various RX 'bypass' unaffected."
//
// NereusSDR: applies the TX/RX-antenna clamp immediately to in-memory
// storage and emits per-band signals. The RX-only array is intentionally
// left untouched to preserve Thetis semantics (the RX-bypass / XVTR port
// selection is independent of external-ATU compat mode). Phase 3M-1 will
// add the proper Aries/LimitTXRXAntenna flag-and-compose path; this
// immediate-set form is a 3P-F carry-over kept here for UI wiring.
void AlexController::setAntennasTo1(bool force)
{
    if (!force) { return; }
    for (int b = 0; b < kBandCount; ++b) {
        m_txAnt[b] = 1;
        m_rxAnt[b] = 1;
        emit antennaChanged(bandFromPerBandStateSlot(b));
    }
}

// ── TX-bypass routing flags ─────────────────────────────────────────────────
// Source: Thetis HPSDR/Alex.cs:61-66 + setup.cs:15420-16505 [v2.10.3.13 @501e3f5]

bool AlexController::rxOutOnTx() const       { return m_rxOutOnTx; }
bool AlexController::ext1OutOnTx() const     { return m_ext1OutOnTx; }
bool AlexController::ext2OutOnTx() const     { return m_ext2OutOnTx; }
bool AlexController::rxOutOverride() const   { return m_rxOutOverride; }
bool AlexController::useTxAntForRx() const   { return m_useTxAntForRx; }
bool AlexController::xvtrActive() const      { return m_xvtrActive; }

void AlexController::setRxOutOnTx(bool on)
{
    if (m_rxOutOnTx == on) { return; }
    m_rxOutOnTx = on;
    if (on) {
        // Mutual exclusion — From Thetis setup.cs:15425-15426 [v2.10.3.13 @501e3f5]
        if (m_ext1OutOnTx) { m_ext1OutOnTx = false; emit ext1OutOnTxChanged(false); }
        if (m_ext2OutOnTx) { m_ext2OutOnTx = false; emit ext2OutOnTxChanged(false); }
    }
    emit rxOutOnTxChanged(on);
}

void AlexController::setExt1OutOnTx(bool on)
{
    if (m_ext1OutOnTx == on) { return; }
    m_ext1OutOnTx = on;
    if (on) {
        // From Thetis setup.cs:16484-16485 [v2.10.3.13 @501e3f5]
        if (m_rxOutOnTx)   { m_rxOutOnTx   = false; emit rxOutOnTxChanged(false); }
        if (m_ext2OutOnTx) { m_ext2OutOnTx = false; emit ext2OutOnTxChanged(false); }
    }
    emit ext1OutOnTxChanged(on);
}

void AlexController::setExt2OutOnTx(bool on)
{
    if (m_ext2OutOnTx == on) { return; }
    m_ext2OutOnTx = on;
    if (on) {
        // From Thetis setup.cs:16497-16498 [v2.10.3.13 @501e3f5]
        if (m_rxOutOnTx)   { m_rxOutOnTx   = false; emit rxOutOnTxChanged(false); }
        if (m_ext1OutOnTx) { m_ext1OutOnTx = false; emit ext1OutOnTxChanged(false); }
    }
    emit ext2OutOnTxChanged(on);
}

void AlexController::setRxOutOverride(bool on)
{
    if (m_rxOutOverride == on) { return; }
    m_rxOutOverride = on;
    emit rxOutOverrideChanged(on);
}

void AlexController::setUseTxAntForRx(bool on)
{
    if (m_useTxAntForRx == on) { return; }
    m_useTxAntForRx = on;
    emit useTxAntForRxChanged(on);
}

// NereusSDR-native — not a Thetis port. Thetis passes xvtr as a method
// parameter to UpdateAlexAntSelection; NereusSDR stores it as session
// state so signal-driven composition can re-fire on toggle. Not persisted.
void AlexController::setXvtrActive(bool on)
{
    if (m_xvtrActive == on) { return; }
    m_xvtrActive = on;
    emit xvtrActiveChanged(on);
}

void AlexController::setMacAddress(const QString& mac) { m_mac = mac; }

QString AlexController::persistenceKey() const
{
    return QStringLiteral("hardware/%1/alex/antenna").arg(m_mac);
}

void AlexController::load()
{
    if (m_mac.isEmpty()) { return; }
    auto& s = AppSettings::instance();
    const QString base = persistenceKey();
    for (int b = 0; b < kBandCount; ++b) {
        const QString slug = bandKeyName(bandFromPerBandStateSlot(b));
        m_txAnt[b]     = s.value(QStringLiteral("%1/%2/tx").arg(base, slug),     QStringLiteral("1")).toInt();
        m_rxAnt[b]     = s.value(QStringLiteral("%1/%2/rx").arg(base, slug),     QStringLiteral("1")).toInt();
        m_rxOnlyAnt[b] = s.value(QStringLiteral("%1/%2/rxonly").arg(base, slug), QStringLiteral("0")).toInt();
        m_txAnt[b]     = clampAnt(m_txAnt[b]);
        m_rxAnt[b]     = clampAnt(m_rxAnt[b]);
        m_rxOnlyAnt[b] = clampRxOnlyAnt(m_rxOnlyAnt[b]);
    }
    m_blockTxAnt2 = (s.value(QStringLiteral("%1/blockTxAnt2").arg(base), QStringLiteral("False")).toString() == QStringLiteral("True"));
    m_blockTxAnt3 = (s.value(QStringLiteral("%1/blockTxAnt3").arg(base), QStringLiteral("False")).toString() == QStringLiteral("True"));
    m_rxOutOnTx     = (s.value(QStringLiteral("%1/rxOutOnTx").arg(base),     QStringLiteral("False")).toString() == QStringLiteral("True"));
    m_ext1OutOnTx   = (s.value(QStringLiteral("%1/ext1OutOnTx").arg(base),   QStringLiteral("False")).toString() == QStringLiteral("True"));
    m_ext2OutOnTx   = (s.value(QStringLiteral("%1/ext2OutOnTx").arg(base),   QStringLiteral("False")).toString() == QStringLiteral("True"));
    m_rxOutOverride = (s.value(QStringLiteral("%1/rxOutOverride").arg(base), QStringLiteral("False")).toString() == QStringLiteral("True"));
    m_useTxAntForRx = (s.value(QStringLiteral("%1/useTxAntForRx").arg(base), QStringLiteral("False")).toString() == QStringLiteral("True"));
    // Phase 3F: per-ADC BPF mode restore
    m_perAdcState[0].mode = static_cast<BpfMode>(
        s.value(QStringLiteral("%1/Alex0_BpfMode").arg(base), QStringLiteral("0")).toInt());
    m_perAdcState[1].mode = static_cast<BpfMode>(
        s.value(QStringLiteral("%1/Alex1_BpfMode").arg(base), QStringLiteral("0")).toInt());
    recomputeBpf(0);
    recomputeBpf(1);
    emit blockTxChanged();
    emit rxOutOnTxChanged(m_rxOutOnTx);
    emit ext1OutOnTxChanged(m_ext1OutOnTx);
    emit ext2OutOnTxChanged(m_ext2OutOnTx);
    emit rxOutOverrideChanged(m_rxOutOverride);
    emit useTxAntForRxChanged(m_useTxAntForRx);
    for (int b = 0; b < kBandCount; ++b) { emit antennaChanged(bandFromPerBandStateSlot(b)); }
}

void AlexController::save()
{
    if (m_mac.isEmpty()) { return; }
    auto& s = AppSettings::instance();
    const QString base = persistenceKey();
    for (int b = 0; b < kBandCount; ++b) {
        const QString slug = bandKeyName(bandFromPerBandStateSlot(b));
        s.setValue(QStringLiteral("%1/%2/tx").arg(base, slug),     QString::number(m_txAnt[b]));
        s.setValue(QStringLiteral("%1/%2/rx").arg(base, slug),     QString::number(m_rxAnt[b]));
        s.setValue(QStringLiteral("%1/%2/rxonly").arg(base, slug), QString::number(m_rxOnlyAnt[b]));
    }
    s.setValue(QStringLiteral("%1/blockTxAnt2").arg(base), m_blockTxAnt2 ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(QStringLiteral("%1/blockTxAnt3").arg(base), m_blockTxAnt3 ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(QStringLiteral("%1/rxOutOnTx").arg(base),     m_rxOutOnTx     ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(QStringLiteral("%1/ext1OutOnTx").arg(base),   m_ext1OutOnTx   ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(QStringLiteral("%1/ext2OutOnTx").arg(base),   m_ext2OutOnTx   ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(QStringLiteral("%1/rxOutOverride").arg(base), m_rxOutOverride ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(QStringLiteral("%1/useTxAntForRx").arg(base), m_useTxAntForRx ? QStringLiteral("True") : QStringLiteral("False"));
    // Phase 3F: per-ADC BPF mode persistence
    s.setValue(QStringLiteral("%1/Alex0_BpfMode").arg(base), QString::number(int(m_perAdcState[0].mode)));
    s.setValue(QStringLiteral("%1/Alex1_BpfMode").arg(base), QString::number(int(m_perAdcState[1].mode)));
}

// ── HL2 I/O board aerial values ─────────────────────────────────────────────

// From mi0bot console.cs:25616-25637 [@c26a8a4]
// public void SetIOBoardAerialPorts(int rx_only_ant, int rx_ant, int tx_ant, bool tx) // MI0BOT: Control I/O Board's aerial capabilities
// public void SetIOBoardAerialPorts(int rx_only_ant) // MI0BOT: Control I/O Board's Alt Rx aerial facility
// The `tx` argument is unused upstream too.
AlexController::IoBoardAerials AlexController::ioBoardAerialPorts(int rxOnlyAnt, int rxAnt, int txAnt)
{
    IoBoardAerials a;
    // switch (rx_only_ant) { case 1: IOBoardAerialMode = 2; ... default: 0 }
    a.mode = (rxOnlyAnt == 1) ? 2 : 0;
    // IOBoardAerialPorts = (byte) (rx_ant & 0x0f);
    // IOBoardAerialPorts |= (byte) (tx_ant << 4);
    a.ports = static_cast<quint8>((rxAnt & 0x0f) | (txAnt << 4));
    return a;
}

// The rx_only_ant, trx_ant and tx_ant mi0bot's UpdateAlexAntSelection hands
// to SetIOBoardAerialPorts on a HERMESLITE. The composition is the one
// RadioModel::applyAlexAntennaForBand already ports for the Alex wire, with
// mi0bot's two HERMESLITE branches added. Receive aerials are read at
// rxBand (NereusSDR's kept band, D61; with nothing kept it is `band`).
AlexController::IoBoardAerials AlexController::hl2IoBoardAerials(Band band, Band rxBand, bool isTx) const
{
    // tx_ant = TxAnt[idx];
    const int txAntNum = txAnt(band);

    int  rxOnly = 0;
    int  trx    = txAntNum;
    bool rxOut  = false;
    bool xvtr   = false;

    if (isTx) {
        // From mi0bot HPSDR/Alex.cs:348-354 [@c26a8a4]
        if (m_ext2OutOnTx)      { rxOnly = 1; }
        else if (m_ext1OutOnTx) { rxOnly = 2; }
        else                    { rxOnly = 0; }
        rxOut = m_rxOutOnTx || m_ext1OutOnTx || m_ext2OutOnTx;
        trx = txAntNum;
        xvtr = (band == Band::XVTR) || m_xvtrActive;
    } else {
        rxOnly = rxOnlyAnt(rxBand);
        xvtr = (rxBand == Band::XVTR) || m_xvtrActive;
        if (xvtr) {
            // From mi0bot HPSDR/Alex.cs:362-372 [@c26a8a4]
            // if (xvtrAnt == 4 ||		// MI0BOT: Alt RX has been requested or TX and Rx are not the same
            //    TRxAnt == false)
            //     rx_only_ant = 1;
            // else
            //     rx_only_ant = 0;
            // int xvtrAnt = c.XVTRForm.GetRXAntenna(c.RX1XVTRIndex);
            // NereusSDR has no transverter form, so the XVTR RX antenna is
            // xvtr.cs GetRXAntenna's default, 0 (never 4).
            constexpr int kXvtrAnt = 0;
            rxOnly = (kXvtrAnt == 4 || !m_useTxAntForRx) ? 1 : 0;
        } else if (rxOnly >= 3) {
            // "do not use XVTR ant port if not using transverter", Alex.cs:380
            rxOnly -= 3;
        }
        rxOut = (rxOnly != 0);
        trx = m_useTxAntForRx ? txAnt(rxBand) : rxAnt(rxBand);
    }

    // rx_out_override: receiving, trx_ant = 4 (Alex.cs:398-405).
    if (m_rxOutOverride && rxOut && !isTx) {
        trx = 4;
    }

    // Aries clamp (LimitTXRXAntenna) is deferred in NereusSDR, as it is
    // for the Alex wire in RadioModel::applyAlexAntennaForBand.
    //G8NJJ

    // From mi0bot HPSDR/Alex.cs:424-430 [@c26a8a4]
    if (m_useTxAntForRx && !xvtr) {
        // MI0BOT: Transmit antenna is being used for reception in split aerial operation
        //         so switch of the rx only aerial but not with transverter operation
        rxOnly = 0;
    }

    // From mi0bot HPSDR/Alex.cs:432-446 [@c26a8a4]
    //MW0LGE_21k9d only set bits if different
    // (the I/O board poll writes each register only when it changed)
    // c.SetIOBoardAerialPorts(rx_only_ant, trx_ant - 1, tx_ant - 1, tx);   // MI0BOT: Sets the aerial controls on the I/O board
    return ioBoardAerialPorts(rxOnly, trx - 1, txAntNum - 1);
}

} // namespace NereusSDR
