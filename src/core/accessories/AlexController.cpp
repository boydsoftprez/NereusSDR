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

namespace NereusSDR {

// Source: HPSDR/Alex.cs:Alex() ctor — "for(int i=0;i<12;i++) { TxAnt[i]=1; RxAnt[i]=1; RxOnlyAnt[i]=0; }" [@501e3f5]
// NereusSDR: default RxOnlyAnt to 1 (not 0) so every slot has a valid ant; "none selected" (0)
// is Thetis's initial value but the UI presents 1-3. Bands GEN/WWV/XVTR add 2 extra slots.
AlexController::AlexController(QObject* parent) : QObject(parent)
{
    m_txAnt.fill(1);
    m_rxAnt.fill(1);
    m_rxOnlyAnt.fill(1);  // 1 = rx1, 2 = rx2, 3 = xv, 0 = none selected [Alex.cs:58]
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
int AlexController::rxOnlyAnt(Band band) const
{
    const int b = int(band);
    return (b >= 0 && b < kBandCount) ? m_rxOnlyAnt[b] : 1;
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
void AlexController::setRxOnlyAnt(Band band, int ant)
{
    const int b = int(band);
    if (b < 0 || b >= kBandCount) { return; }
    const int newAnt = clampAnt(ant);
    if (m_rxOnlyAnt[b] == newAnt) { return; }
    m_rxOnlyAnt[b] = newAnt;
    emit antennaChanged(band);
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

// Source: HPSDR/Alex.cs:SetAntennasTo1(bool IsSetTo1) [@501e3f5]
// "SetAntennasTo1 causes RX, TX antennas to be set to 1 — the various RX bypass unaffected"
// Original: "LimitTXRXAntenna = IsSetTo1;" (applies on next output frame via callers).
// NereusSDR: applies immediately to all in-memory values and emits signals for each band.
void AlexController::setAntennasTo1(bool force)
{
    if (!force) { return; }
    for (int b = 0; b < kBandCount; ++b) {
        m_txAnt[b] = 1;
        m_rxAnt[b] = 1;
        m_rxOnlyAnt[b] = 1;
        emit antennaChanged(Band(b));
    }
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
        m_rxOnlyAnt[b] = s.value(QStringLiteral("%1/%2/rxonly").arg(base, slug), QStringLiteral("1")).toInt();
        m_txAnt[b]     = clampAnt(m_txAnt[b]);
        m_rxAnt[b]     = clampAnt(m_rxAnt[b]);
        m_rxOnlyAnt[b] = clampAnt(m_rxOnlyAnt[b]);
    }
    m_blockTxAnt2 = (s.value(QStringLiteral("%1/blockTxAnt2").arg(base), QStringLiteral("False")).toString() == QStringLiteral("True"));
    m_blockTxAnt3 = (s.value(QStringLiteral("%1/blockTxAnt3").arg(base), QStringLiteral("False")).toString() == QStringLiteral("True"));
    emit blockTxChanged();
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
}

} // namespace NereusSDR
