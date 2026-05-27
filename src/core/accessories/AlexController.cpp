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

    if (s.effective != prev.effective || s.reasonText != prev.reasonText) {
        emit bpfStateChanged(adc, s);
    }
}

// Source: HPSDR/Alex.cs:setTxAnt / TxAnt[] accessor [@501e3f5]
int AlexController::txAnt(Band band) const
{
    const int b = int(band);
    return (b >= 0 && b < kBandCount) ? m_txAnt[b] : 1;
}

// Source: HPSDR/Alex.cs:setRxAnt / RxAnt[] accessor [@501e3f5]
int AlexController::rxAnt(Band band) const
{
    const int b = int(band);
    return (b >= 0 && b < kBandCount) ? m_rxAnt[b] : 1;
}

// Source: HPSDR/Alex.cs:setRxOnlyAnt / RxOnlyAnt[] accessor [@501e3f5]
// Out-of-bounds sentinel returns 0 ("none selected") to match the in-range
// default; returning 1 would silently activate the RX-bypass relay.
int AlexController::rxOnlyAnt(Band band) const
{
    const int b = int(band);
    return (b >= 0 && b < kBandCount) ? m_rxOnlyAnt[b] : 0;
}

// Source: HPSDR/Alex.cs:setTxAnt(Band band, byte ant) [@501e3f5]
// Original: "if(ant>3){ant=1;} idx=(int)band-(int)Band.B160M; TxAnt[idx]=ant;"
// NereusSDR: clampAnt(v) handles both low (0→1) and high (>3→3) clamping.
//            Block-TX safety guards added for UI contract.
void AlexController::setTxAnt(Band band, int ant)
{
    const int b = int(band);
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
    const int b = int(band);
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
    const int b = int(band);
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
                emit antennaChanged(Band(b));
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
                emit antennaChanged(Band(b));
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
        emit antennaChanged(Band(b));
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
        const QString slug = bandKeyName(Band(b));
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
    for (int b = 0; b < kBandCount; ++b) { emit antennaChanged(Band(b)); }
}

void AlexController::save()
{
    if (m_mac.isEmpty()) { return; }
    auto& s = AppSettings::instance();
    const QString base = persistenceKey();
    for (int b = 0; b < kBandCount; ++b) {
        const QString slug = bandKeyName(Band(b));
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

} // namespace NereusSDR
