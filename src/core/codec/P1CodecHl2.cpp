// =================================================================
// src/core/codec/P1CodecHl2.cpp  (NereusSDR)
// =================================================================
//
// Ported from mi0bot-Thetis sources:
//   Project Files/Source/ChannelMaster/networkproto1.c:869-1201
//   (WriteMainLoop_HL2)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. HL2-only codec; mirrors mi0bot's
//                literal WriteMainLoop_HL2 vs WriteMainLoop split.
//                Fixes reported HL2 S-ATT bug at the wire layer.
// =================================================================
//
// === Verbatim mi0bot networkproto1.c header (lines 1-19) ===
// /*
//  * networkprot1.c
//  * Copyright (C) 2020 Doug Wigley (W5WC)
//  *
//  * This library is free software; you can redistribute it and/or
//  * modify it under the terms of the GNU Lesser General Public
//  * License as published by the Free Software Foundation; either
//  * version 2 of the License, or (at your option) any later version.
//  *
//  * This library is distributed in the hope that it will be useful,
//  * but WITHOUT ANY WARRANTY; without even the implied warranty of
//  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  * Lesser General Public License for more details.
//  *
//  * You should have received a copy of the GNU Lesser General Public
//  * License along with this library; if not, write to the Free Software
//  * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//  *
//  */
// =================================================================

#include "P1CodecHl2.h"
#include "core/DdcAssignment.h"
#include "core/IoBoardHl2.h"

namespace NereusSDR {

void P1CodecHl2::composeCcForBank(int bank, const CodecContext& ctx,
                                  quint8 out[5]) const
{
    // C0 low bit = XmitBit (MOX)
    // Source: mi0bot networkproto1.c:899 [@c26a8a4]
    const quint8 C0base = ctx.mox ? 0x01 : 0x00;
    for (int i = 0; i < 5; ++i) { out[i] = 0; }

    switch (bank) {
        // Bank 0 — General settings
        // Source: mi0bot networkproto1.c:938-978 [@c26a8a4 / matches @501e3f5]
        // C3 bits 5-6: RX-only mux — From Thetis netInterface.c:479-481 [v2.10.3.13 @501e3f5]
        // C3 bit 7: _Rx_1_Out relay — networkproto1.c:455 [v2.10.3.13 @501e3f5]
        case 0: {
            out[0] = C0base | 0x00;
            out[1] = quint8(ctx.sampleRateCode & 0x03);
            out[2] = quint8((ctx.ocByte << 1) & 0xFE);
            // C3: rxPreamp[0] + dither + random + RX-only mux + RX-bypass-out.
            // Source: networkproto1.c:453-468 [v2.10.3.13 @501e3f5]
            quint8 c3 = quint8((ctx.rxPreamp[0] ? 0x04 : 0)
                             | (ctx.dither[0]   ? 0x08 : 0)
                             | (ctx.random[0]   ? 0x10 : 0));
            switch (ctx.rxOnlyAnt) {
                case 1: c3 |= 0b0010'0000; break;  // _Rx_1_In
                case 2: c3 |= 0b0100'0000; break;  // _Rx_2_In
                case 3: c3 |= 0b0110'0000; break;  // _XVTR_Rx_In
                default: break;                     // 0 = no RX-only path selected
            }
            if (ctx.rxOut) { c3 |= 0b1000'0000; }  // _Rx_1_Out relay
            out[3] = c3;
            out[4] = quint8((ctx.antennaIdx & 0x03)
                          | (ctx.duplex ? 0x04 : 0)
                          | (((ctx.activeRxCount - 1) & 0x0F) << 3)
                          | (ctx.diversity ? 0x80 : 0));
            return;
        }

        // Bank 1 — TX VFO
        // Source: mi0bot networkproto1.c:980-984 [@c26a8a4 / matches @501e3f5]
        case 1:
            out[0] = C0base | 0x02;
            out[1] = quint8((ctx.txFreqHz >> 24) & 0xFF);
            out[2] = quint8((ctx.txFreqHz >> 16) & 0xFF);
            out[3] = quint8((ctx.txFreqHz >>  8) & 0xFF);
            out[4] = quint8( ctx.txFreqHz        & 0xFF);
            return;

        // Banks 2-9 — slot ordering MUST match mi0bot WriteMainLoop_HL2 exactly.
        // Source: mi0bot ChannelMaster/networkproto1.c:987-1079 [v2.10.3.14-beta1]
        //
        // Critical bug fix (2026-05-01): the prior emission collapsed cases 2-9
        // into a single RX-VFO formula `C0|0x04 + (bank-2)*2`, which produced:
        //   slot 4 → C0|0x08  (wrong — should be ADC-assign C0|0x1C)
        //   slot 5 → C0|0x0A  (wrong — should be DDC2 C0|0x08)
        //   slot 9 → C0|0x12  (wrong — collides with drive/Alex bank C0|0x12 in
        //                       slot 10!)
        // Result: the radio saw two consecutive drive-level frames per round-robin,
        // one with C1=0 (zero drive) and one with C1=50 (tune drive).  HL2 firmware
        // oscillated PA on/off ⇒ rapid T/R relay clicking on TUNE.
        //
        // Slot 2 = DDC0/RX1 freq        (C0|0x04)
        case 2: {
            out[0] = C0base | 0x04;
            // Phase 3M-4 Task 17 P1 follow-up: bank 2/3 PS freq override
            // for HermesII (nddc==2).  Source: mi0bot networkproto1.c:985
            // [v2.10.3.13-beta2].  Mirrors P1CodecStandard's identical
            // override; HL2 itself sets nDdc=4 in
            // P1CodecHl2::applyPureSignalDdcConfig (line 477) so this gate
            // is normally false on HL2 — the firmware handles freq routing
            // via cntrl1=4 ADC steering.  But HL2's codec is also used for
            // any P1 board whose physical wire dialect maps to HermesLite,
            // so the gate is kept honest by reading ctx.p1PsNDdc.
            quint64 freq;
            if (0 >= ctx.activeRxCount) {
                freq = ctx.txFreqHz;
            } else if (ctx.p1PsNDdc == 2 && ctx.mox && ctx.p1PuresignalRun) {
                freq = ctx.txFreqHz;  //MW0LGE PS DDC0 override (mirror of networkproto1.c:986)
            } else {
                freq = ctx.rxFreqHz[0];
            }
            out[1] = quint8((freq >> 24) & 0xFF);
            out[2] = quint8((freq >> 16) & 0xFF);
            out[3] = quint8((freq >>  8) & 0xFF);
            out[4] = quint8( freq        & 0xFF);
            return;
        }

        // Slot 3 = DDC1/RX2 freq        (C0|0x06)
        case 3: {
            out[0] = C0base | 0x06;
            // Phase 3M-4 Task 17 P1 follow-up: bank 2/3 PS freq override.
            // Source: mi0bot networkproto1.c:1000-1005 [v2.10.3.13-beta2].
            // Same gate as bank 2 above; on HL2 (nDdc=4 from
            // applyPureSignalDdcConfig) this falls through to the standard
            // ctx.rxFreqHz[1] path, so HL2 PS-MOX behaviour is unchanged
            // — the firmware handles DDC1 routing via cntrl1=4.
            quint64 freq;
            if (1 >= ctx.activeRxCount) {
                freq = ctx.txFreqHz;
            } else if (ctx.p1PsNDdc == 2 && ctx.mox && ctx.p1PuresignalRun) {
                freq = ctx.txFreqHz;  //MW0LGE PS DDC1 override (mirror of networkproto1.c:1001)
            } else if (ctx.p1PsNDdc == 5) {
                freq = ctx.rxFreqHz[0];  // Orion: DDC1 = RX1 freq (networkproto1.c:1003)
            } else {
                freq = ctx.rxFreqHz[1];  // Hermes default: RX2 VFO
            }
            out[1] = quint8((freq >> 24) & 0xFF);
            out[2] = quint8((freq >> 16) & 0xFF);
            out[3] = quint8((freq >>  8) & 0xFF);
            out[4] = quint8( freq        & 0xFF);
            return;
        }

        // Slot 4 = ADC assignments + TX step ATT  (C0|0x1C)
        // From mi0bot networkproto1.c:1020-1026 [v2.10.3.14-beta1]:
        //   C0 |= 0x1c;
        //   C1 = P1_adc_cntrl & 0xFF;
        //   C2 = (P1_adc_cntrl >> 8) & 0x3FF;
        //   C3 = adc[0].tx_step_attn & 0x1F;
        //   C4 = 0;
        //
        // `P1_adc_cntrl` is a separate Thetis variable from the
        // UpdateDDCs `cntrl1` — the latter maps to prn->rx[i].rx_adc
        // (P2 wire) only. Reading `ctx.p1AdcCntrl` here matches the
        // upstream mi0bot networkproto1.c byte source. See
        // CodecContext.h::p1AdcCntrl for the conflation history.
        case 4:
            out[0] = C0base | 0x1C;
            out[1] = quint8(ctx.p1AdcCntrl & 0xFF);
            out[2] = quint8((ctx.p1AdcCntrl >> 8) & 0x3F);
            out[3] = quint8(ctx.txStepAttn[0] & 0x1F);
            out[4] = 0;
            return;

        // Slots 5-9 = DDC2..DDC6 freqs.  For HL2 (nddc=4 always per
        // mi0bot console.cs:8421-8427 [v2.10.3.14-beta1]):
        //   DDC2 (slot 5) ⇒ tx[0].frequency  (PureSignal feedback when active)
        //   DDC3 (slot 6) ⇒ tx[0].frequency  (always tracks TX)
        //   DDC4 (slot 7) ⇒ tx[0].frequency  (always tracks TX)
        //   DDC5 (slot 8) ⇒ rx[0].frequency  (unused, defaults to RX1)
        //   DDC6 (slot 9) ⇒ rx[0].frequency  (unused, defaults to RX1)
        // Source: mi0bot networkproto1.c:1028-1078 [v2.10.3.14-beta1]
        case 5: case 6: case 7: case 8: case 9: {
            // C0 address: bank 5 → 0x08, 6 → 0x0A, 7 → 0x0C, 8 → 0x0E, 9 → 0x10
            static const quint8 kRxC0Addr[] = { 0x08, 0x0A, 0x0C, 0x0E, 0x10 };
            out[0] = C0base | kRxC0Addr[bank - 5];
            // DDC2/3/4 → TX freq; DDC5/6 → RX1 freq.
            const bool tracksTx = (bank >= 5 && bank <= 7);
            const quint64 freq = tracksTx ? ctx.txFreqHz : ctx.rxFreqHz[0];
            out[1] = quint8((freq >> 24) & 0xFF);
            out[2] = quint8((freq >> 16) & 0xFF);
            out[3] = quint8((freq >>  8) & 0xFF);
            out[4] = quint8( freq        & 0xFF);
            return;
        }

        // Bank 10 — TX drive, mic boost, Alex HPF/LPF, T/R relay
        // Source: mi0bot networkproto1.c:1081-1094 [v2.10.3.14-beta1]
        // HL2 note: deskhpsdr clears C2/C3/C4 entirely for HL2 PA-enable
        // (old_protocol.c:2964-2966 [@120188f]) — HL2 firmware (control.v:211-214)
        // does NOT decode C3 bit 7 (Alex T/R relay).  We still write trxRelay
        // here for correctness; HL2 FW ignores the bit.
        // T/R relay bit (C3 bit 7) is INVERTED: 0 = engaged, 1 = disabled.
        // Source: deskhpsdr/src/old_protocol.c:2909-2910 [@120188f]
        //
        // C2 bit 3 = HL2 PA-enable (repurposed `ApolloTuner` slot per mi0bot).
        // mi0bot routes DisablePA() on HL2 through EnableApolloTuner(!bit), so
        // the bit follows tx[0].pa polarity inverted: PA enabled ⇒ bit set,
        // PA disabled ⇒ bit cleared.  We emit bit set always — NereusSDR has
        // no user-facing "Disable PA" wiring for HL2 yet, and PA-enabled is
        // the only state in which TUNE / MOX produce RF.  Without this bit,
        // the HL2 FPGA sees MOX asserted with PA-not-enabled and the T/R
        // relay flutters because it cannot reconcile.  This was the root
        // cause of the "rapid relay clicking on TUNE" bench symptom.
        // From mi0bot ChannelMaster/networkproto1.c:1084-1085 [v2.10.3.14-beta1]:
        //   C2 = ((mic_boost & 1) | ((line_in & 1) << 1) | ApolloFilt |
        //         ApolloTuner | ApolloATU | ApolloFiltSelect | 0b01000000) & 0x7f;
        // From mi0bot ChannelMaster/netInterface.c:582-588 [v2.10.3.14-beta1]:
        //   EnableApolloTuner(bits): bits != 0 ⇒ ApolloTuner = 0x8 else 0
        // From mi0bot ChannelMaster/netInterface.c:629-635 [v2.10.3.14-beta1]:
        //   DisablePA on HL2 routes through EnableApolloTuner(!bit)
        // MI0BOT: This call used on HL2 to enable/disable PA  [original inline comment from netInterface.c:629]
        case 10:
            out[0] = C0base | 0x12;
            out[1] = quint8(ctx.txDrive & 0xFF);
            // C2: mic_boost (bit 0) | line_in (bit 1) | HL2 PA enable (bit 3)
            //     | always-on default (bit 6).
            out[2] = quint8(
                (ctx.p1MicBoost ? 0x01 : 0x00) |
                (ctx.p1LineIn   ? 0x02 : 0x00) |
                /*HL2 PA enable*/ 0x08 |
                /*always-on*/     0x40);
            out[3] = quint8(ctx.alexHpfBits | (ctx.trxRelay ? 0x00 : 0x80));  // T/R relay engaged (INVERTED: 1 = disabled)
            out[4] = quint8(ctx.alexLpfBits);
            return;

        // Bank 11 — Preamp + RX/TX step ATT ADC0 (HL2 6-bit encoding + MOX branch)
        // **THIS IS THE BUG FIX** — HL2 needs 6-bit mask (0x3F) + 0x40 enable,
        // and MOX branch selects txStepAttn[0] instead of rxStepAttn[0].
        // Standard codec keeps ramdor's 5-bit (0x1F) + 0x20 encoding.
        // Source: mi0bot networkproto1.c:1091-1104 [@c26a8a4]
        case 11: {
            out[0] = C0base | 0x14;
            // C1: preamp bits 0-3 (bit 3 = rx0 again, Thetis quirk) + mic_trs bit 4
            //     + mic_bias bit 5 + mic_ptt bit 6.
            // HL2 has no mic jack but the bits are written for correctness (FW ignores).
            // mic_trs polarity inversion: wire bit set when tip is BIAS/PTT (!tipHot).
            // mic_bias polarity: 1 = bias on (no inversion).
            // mic_ptt polarity: direct — bit set when PTT is disabled at firmware,
            // matching Thetis console.cs:19764 [v2.10.3.13+501e3f51]:
            //   NetworkIO.SetMicPTT(Convert.ToInt32(mic_ptt_disabled));
            // From mi0bot ChannelMaster/networkproto1.c:1101 [v2.10.3.14-beta1 @c26a8a4]:
            //   C1 = ... | ((mic.mic_ptt & 1) << 6);
            // Verified against a Thetis HL2 capture: bit 6 = 0 in steady state
            // (host_to_radio bank 11 C1 byte was 0x00 throughout TUNE).
            out[1] = quint8((ctx.rxPreamp[0] ? 0x01 : 0)
                          | (ctx.rxPreamp[1] ? 0x02 : 0)
                          | (ctx.rxPreamp[2] ? 0x04 : 0)
                          | (ctx.rxPreamp[0] ? 0x08 : 0)             // bit3 = rx0 again (Thetis quirk)
                          | (!ctx.p1MicTipRing      ? 0x10 : 0x00)   // mic_trs (inverted) — 3M-1b G.3
                          | (ctx.p1MicBias          ? 0x20 : 0x00)   // mic_bias (no inversion) — 3M-1b G.4
                          | (ctx.p1MicPTTDisabled   ? 0x40 : 0x00)); // mic_ptt (direct, issue #182)
            // C2: line_in_gain (low 5 bits) | puresignal_run (bit 6).
            // Source: mi0bot ChannelMaster/networkproto1.c:1097 [v2.10.3.13-beta2]
            //   C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
            //
            // PR #212 follow-up bench-fix (J.J. KG4VCF, 2026-05-07):
            // Pre-fix this was hardcoded `out[2] = 0` (cargo-culted scaffold from
            // an early HL2 codec stub).  Wire-pcap of working Thetis HL2 PS-MOX
            // session showed C2=0x57 (line_in_gain=0x17 + ps_run=1).  HL2 firmware
            // engages PureSignal feedback routing on this bit; without it,
            // calcc/pscc receive paired streams of identical RX1-audio data and
            // the calibration state machine can't compute corrections.
            out[2] = quint8((ctx.p1LineInGain & 0x1F)
                          | (ctx.p1PuresignalRun ? 0x40 : 0x00));
            out[3] = 0;
            // MI0BOT: Different read loop for HL2 — Larger range for the HL2 attenuator
            // [original inline comment from networkproto1.c:1100,1102]
            if (ctx.mox) {
                // HL2 TX path — issue #175 follow-up bench, 2026-05-04 (JJ).
                //
                // From mi0bot networkproto1.c:1099-1100 [v2.10.3.13-beta2]:
                //   if (XmitBit)
                //     C4 = (prn->adc[0].tx_step_attn & 0b00111111) | 0b01000000;
                // The codec writes the network-buffer value DIRECTLY (6-bit
                // mask + 0x40 enable bit).  The (31 - userDb) HL2-specific
                // inversion is applied upstream at the wire-send call site,
                // mi0bot console.cs:10658 [v2.10.3.13-beta2]:
                //   if (HardwareSpecific.Model == HPSDRModel.HERMESLITE)
                //       NetworkIO.SetTxAttenData(31 - _tx_attenuator_data);
                //         // MI0BOT: Greater range for HL2
                // The HL2 firmware reads the wire field with inverted polarity
                // — higher wire value = more attenuation — so mi0bot performs
                // the (31 - userDb) flip before storing into the buffer.
                //
                // NereusSDR architecture stores the user's raw signed dB in
                // m_txStepAttn (set via P1RadioConnection::setTxStepAttenuation
                // — no inversion).  We perform the (31 - userDb) inversion
                // at the codec — same pattern as the RX-path branch
                // immediately below, which inverts (31 - ctx.rxStepAttn[0])
                // at emission.  Architectural choice: keep the user-facing
                // value at the connection layer; do all wire-format inversion
                // at the codec.  Saves one layer of indirection and matches
                // the RX path.
                //
                // The pre-fix emission `quint8(0x40)` (forced wire = 0,
                // user 31 dB equivalent) was a 3M-1c bench overcorrection
                // motivated by a 0 W TUNE bug that turned out to be caused
                // by an unrelated drive-scaling issue.  With the hardcode in
                // place, m_txStepAttn was silently discarded — so when JJ
                // enabled "ATT on TX" and engaged MOX expecting 31 dB to
                // protect the HL2's RX ADC from own-TX leakage, the radio
                // saw wire = 0 (= 0 dB attenuation), which is the opposite
                // of the requested behavior.
                //
                // After this fix:
                //   user 0 dB  → wire = (31 - 0)  & 0x3F | 0x40 = 0x5F (no ATT)
                //   user 15 dB → wire = (31 - 15) & 0x3F | 0x40 = 0x50
                //   user 31 dB → wire = (31 - 31) & 0x3F | 0x40 = 0x40 (max ATT)
                //
                // Signed user-facing range -28..+31 dB applies on TX as well
                // (mi0bot setup.cs:16085-16086 [v2.10.3.13-beta2]).
                const int txUserDb = qBound(-28, static_cast<int>(ctx.txStepAttn[0]), 31);
                out[4] = quint8(((31 - txUserDb) & 0b00111111) | 0b01000000);  // Larger range for the HL2 attenuator
            } else {
                // HL2 RX path: same (31 - userDb) inversion as TX. mi0bot applies
                // the inversion at three console.cs callsites (RX1 ADC0
                // attenuator, RX2 ADC0 attenuator, and the SetADC1StepAttenData
                // bridge).  Without it, slider value 31 reaches HL2 firmware as
                // zero attenuation — RX ATT controls feel reversed and "max
                // attenuation" actually disables the attenuator.  Closes
                // 3M-1c desk-review follow-up B1.
                // From mi0bot-Thetis console.cs:11075, 11251, 19380 [@c26a8a4]
                // MI0BOT: Greater range for HL2
                //
                // Signed user-facing range −28..+31 dB applies on RX as well
                // (mi0bot setup.cs:16085-16086 [v2.10.3.13-beta2]).
                const int rxUserDb = qBound(-28, static_cast<int>(ctx.rxStepAttn[0]), 31);
                out[4] = quint8(((31 - rxUserDb) & 0b00111111) | 0b01000000);  // Larger range for the HL2 attenuator
            }
            return;
        }

        // Bank 12 — Step ATT ADC1/2 + CW keyer
        // HL2 behavior: identical to Standard (forces ADC1=0x1F under MOX).
        // No RedPitaya-special-case needed — HL2 is known hardware.
        // Source: mi0bot networkproto1.c:1106-1125 [@c26a8a4]
        case 12: {
            out[0] = C0base | 0x16;
            if (ctx.mox) {
                out[1] = 0x1F | 0x20;  // forced under MOX (same as Standard)
            } else {
                out[1] = quint8(ctx.rxStepAttn[1]) | 0x20;
            }
            out[2] = quint8((ctx.rxStepAttn[2] & 0x1F) | 0x20);
            out[3] = 0;
            out[4] = 0;
            return;
        }

        // Banks 13-15 — CW / EER (HL2 has no CW state surface yet — emit C0 only)
        // Source: mi0bot networkproto1.c:1127-1149 [v2.10.3.13-beta2]
        case 13: out[0] = C0base | 0x1E; return;
        case 14: out[0] = C0base | 0x20; return;
        case 15: out[0] = C0base | 0x22; return;

        // Bank 16 — BPF2 + xvtr_enable + puresignal_run
        // Source: mi0bot networkproto1.c:1151-1160 [v2.10.3.13-beta2]:
        //   case 16: // BPF2 0x12
        //       C0 |= 0x24;
        //       C1 = (BPF2 HPF/LPF/preamp bits + rx2_gnd<<7);
        //       C2 = (xvtr_enable & 1) | ((prn->puresignal_run & 1) << 6);
        //       C3 = 0;
        //       C4 = 0;
        //
        // PR #212 follow-up bench-fix (J.J. KG4VCF, 2026-05-07):
        // Pre-fix this was a stub (C0 only, C1-C4 = 0).  Wire-pcap of working
        // Thetis HL2 PS-MOX session showed bank 16 carrying ps_run=1 in C2 bit 6
        // (alongside bank 11 C2 — Thetis emits the bit on TWO banks).  HL2
        // firmware appears to engage PureSignal feedback routing only when
        // BOTH bank 11 and bank 16 carry the bit; without bank 16, calcc state
        // machine stalls at LCOLLECT with txEnvMax == rxEnvMax.
        //
        // BPF2 (HL2's secondary BPF board) state isn't tracked in NereusSDR
        // yet — emit C1 = 0 (no BPF2 routing) until BPF2 support lands.
        // xvtr_enable is also not yet plumbed — emit 0.  Only the ps_run bit
        // is wired; that's what HL2 firmware needs for PS to engage.
        case 16:
            out[0] = C0base | 0x24;
            out[1] = 0;     // BPF2 filter bits — not plumbed in NereusSDR yet
            out[2] = quint8(ctx.p1PuresignalRun ? 0x40 : 0x00);  // ps_run only
            out[3] = 0;
            out[4] = 0;
            return;

        // Bank 17 — TX latency + PTT hang (HL2-only, NOT AnvelinaPro3 extra OC)
        // Source: mi0bot networkproto1.c:1162-1168 [@c26a8a4]
        case 17:
            out[0] = C0base | 0x2E;
            out[1] = 0;
            out[2] = 0;
            out[3] = quint8(ctx.hl2PttHang & 0b00011111);
            out[4] = quint8(ctx.hl2TxLatency & 0b01111111);
            return;

        // Bank 18 — Reset on disconnect (HL2-only firmware feature)
        // Source: mi0bot networkproto1.c:1170-1176 [@c26a8a4]
        case 18:
            out[0] = C0base | 0x74;
            out[1] = 0;
            out[2] = 0;
            out[3] = 0;
            out[4] = quint8(ctx.hl2ResetOnDisconnect ? 0x01 : 0x00);
            return;

        default:
            out[0] = C0base;
            return;
    }
}

// ---------------------------------------------------------------------------
// tryComposeI2cFrame
//
// Overrides the normal C&C compose path when the IoBoardHl2 queue has pending
// I2C transactions. Pops the next txn from the front of the queue and encodes
// it into the 5-byte C&C frame per mi0bot WriteMainLoop_HL2.
//
// Wire layout (ported from mi0bot networkproto1.c:895-943 [@c26a8a4]):
//   C0 = XmitBit | (I2C chip addr << 1) | (ctrl_request << 7)
//        I2C chip addr: 0x3c for bus 0 (I2C1), 0x3d for bus 1 (I2C2)
//        ctrl_request comes from txn.needsResponse (was prn->i2c.ctrl_request
//        global in mi0bot — moved per-txn for back-to-back independence).
//   C1 = 0x07 (read) or 0x06 (write) — driven by txn.isRead (was
//        prn->i2c.ctrl_read global in mi0bot).
//   C2 = 0x80 | address              — device 7-bit address with stop bit.
//        If address > 0x7F, right-shift by 1 to strip the r/w bit.
//   C3 = txn.control                 — I2C sub-address / register byte.
//                                      Matches mi0bot queue entry .control.
//   C4 = txn.writeData               — write data byte.
//
// Returns true if a frame was composed and the txn dequeued; false if queue
// was empty or m_io is null.
// ---------------------------------------------------------------------------
bool P1CodecHl2::tryComposeI2cFrame(quint8 out[5], bool mox) const
{
    if (!m_io || m_io->i2cQueueIsEmpty()) { return false; }
    IoBoardHl2::I2cTxn txn;
    if (!m_io->dequeueI2c(txn)) { return false; }

    // Record the read so applyI2cReadResponse() can route the bytes to the
    // right destination (HW version → m_hardwareVersion, register read →
    // registers[sub..sub+3]).  FIFO order matches the wire request order.
    if (txn.isRead) {
        m_io->pushPendingRead({txn.address, txn.control});
    }

    for (int i = 0; i < 5; ++i) { out[i] = 0; }

    // C0: XmitBit in bit 0, I2C chip select in bits 1-7, ctrl_request in bit 7.
    // Source: mi0bot networkproto1.c:912-919 [@c26a8a4]
    const quint8 xmitBit = mox ? quint8(0x01) : quint8(0x00);
    const quint8 ctrlReq = txn.needsResponse ? quint8(0x01) : quint8(0x00);
    if (txn.bus == 0) {
        out[0] = xmitBit | quint8(0x3c << 1) | quint8(ctrlReq << 7);  // I2C1 0x3c
    } else {
        out[0] = xmitBit | quint8(0x3d << 1) | quint8(ctrlReq << 7);  // I2C2 0x3d
    }

    // C1: 0x07 = read, 0x06 = write.
    // Source: mi0bot networkproto1.c:930-935 [@c26a8a4]
    out[1] = txn.isRead ? quint8(0x07) : quint8(0x06);

    // C2: 0x80 | device address (stop bit). Address > 0x7F → shift right to strip r/w bit.
    // Source: mi0bot networkproto1.c:921-928 [@c26a8a4]
    quint8 address = txn.address;
    if (address > 0x7F) { address = address >> 1; }
    out[2] = quint8(0x80) | address;  // Stop request

    // C3: I2C sub-address / register byte verbatim.
    // Source: mi0bot networkproto1.c:939 [@c26a8a4]
    out[3] = txn.control;

    // C4: write data byte.
    // Source: mi0bot networkproto1.c:940 [@c26a8a4]
    out[4] = txn.writeData;

    emit m_io->i2cTxComposed(out[0], out[1], out[2], out[3], out[4]);
    qDebug("HL2 I2C OUT: C0=%02X C1=%02X C2=%02X C3=%02X C4=%02X (bus=%u addr=%02X reg=%02X read=%d req=%d)",
           out[0], out[1], out[2], out[3], out[4],
           txn.bus, txn.address, txn.control, int(txn.isRead), int(txn.needsResponse));
    return true;
}

// =================================================================
// Phase 3M-4 Task 5: PureSignal DDC config — HL2 branch
// =================================================================
//
// Verbatim port of the HL2 branch in mi0bot console.cs UpdateDDCs().
// In mi0bot, HpsdrModel.HERMESLITE is grouped with HERMES/ANAN10/ANAN100
// at line 8408-8409, so this method ports the full HERMES-class branch
// PLUS the HL2-specific rate override at lines 8476-8485.
//
// Source: mi0bot console.cs:8408-8490 [v2.10.3.13-beta2]
//
// case HPSDRModel.HERMES:
// case HPSDRModel.HERMESLITE:     // MI0BOT: HL2
// case HPSDRModel.ANAN10:
// case HPSDRModel.ANAN100:
//     P1_rxcount = 4;             // RX4 used for puresignal feedback
//     nddc = 4;
//     ...
//     else // transmitting and PS is ON
//     {
//         P1_DDCConfig = 6;
//         DDCEnable = DDC0;
//         SyncEnable = DDC1;
//         Rate[0] = ps_rate;
//         Rate[1] = ps_rate;
//         if (hpsdr_model == HPSDRModel.HERMESLITE) // MI0BOT: HL2 can work at a high sample rate
//         {
//             Rate[0] = rx1_rate;
//             Rate[1] = rx1_rate;
//         }
//         ...
//         cntrl1 = 4;
//         cntrl2 = 0;
//     }
//
// `ps_rate` is the static cmaster.PSrate = 192000
// (cmaster.cs:424 [v2.10.3.13-beta2], unchanged from ramdor).
PsDdcConfig P1CodecHl2::applyPureSignalDdcConfig(
    HPSDRModel /*model*/,
    bool psEnabled,
    bool diversityEnabled,
    bool moxState,
    int rx1Rate,
    int rx2Rate,
    bool rx2Enabled,
    quint8 /*adcCtrl1*/,
    quint8 /*adcCtrl2*/) const
{
    PsDdcConfig cfg;
    constexpr uint8_t DDC0 = 1, DDC1 = 2;

    // From mi0bot console.cs:8409-8413 [v2.10.3.13-beta2]
    //   case HPSDRModel.HERMESLITE: // MI0BOT: HL2
    //
    // Inline tag preserved per CLAUDE.md "Inline comment preservation":
    //MI0BOT  [HL2 case-statement marker at console.cs:8409]
    cfg.nDdc = 4;

    // ── APPROVED DEVIATION FROM mi0bot, 2026-07-31 ─────────────────────────
    //
    // mi0bot sets P1_rxcount = 4 unconditionally here
    // (console.cs:8412-8413 [v2.10.3.13-beta2]), in every MOX, diversity and
    // PureSignal state and at every sample rate. This is NOT a port. It is a
    // NereusSDR-original divergence justified by the link budget, approved by
    // the maintainer on 2026-07-31 conditional on bench verification.
    //
    // p1RxCount becomes the wire C4 field, nddc - 1 << 3
    // (P1RadioConnection::composeCcBank0), and the ep6 slot layout,
    // slotBytes = 6 * numRx + 2. Because ep6 datagrams are fixed at 1032
    // bytes, sample capacity falls as the count rises
    // (networkproto1.c:527: spr = 504 / (6 * nddc + 2)), so the datagram rate
    // scales with the count whether or not the extra DDCs are consumed.
    // Announcing 4 for a two-panadapter board costs about 44 Mbit/s at
    // 192 kHz and 89 at 384, against 23 and 47 for 2.
    //
    // PureSignal genuinely needs four: DDC0 and DDC1 as the sync pair, DDC2
    // feedback, DDC3 TX monitor (mi0bot console.cs:8757-8762
    // [v2.10.3.13-beta2] GetDDC: rx1 = 0; rx2 = 1; psrx = 2; pstx = 3).
    //
    // Keyed on psEnabled, the operator's master toggle, NOT on moxState or
    // the PS run state. Keying on either would flip the count on every
    // key-down, changing the slot layout mid-stream. The floor is 2 rather
    // than 1 because 2 is already sent on every P1 connect
    // (P1RadioConnection.cpp:695) and 1 never has been.
    //
    // nDdc stays 4 in all cases: it feeds only the bank-2/3 frequency
    // override gate (m_psNDdc), not the wire count.
    //
    // Full rationale and the bench gate that conditions this approval:
    // docs/architecture/2026-07-31-hl2-slice-cap-design.md section 6.2.
    //
    // RX4 used for puresignal feedback  [original inline comment from mi0bot console.cs:8412]
    cfg.p1RxCount = psEnabled ? 4 : 2;

    if (!moxState) {
        if (!diversityEnabled) {
            // From mi0bot console.cs:8416-8429 [v2.10.3.13-beta2]
            cfg.p1DdcConfig = 4;
            cfg.ddcEnable   = DDC0;
            cfg.syncEnable  = 0;
            cfg.rate[0]     = static_cast<uint32_t>(rx1Rate);
            cfg.cntrl1      = 0;
            cfg.cntrl2      = 0;

            if (rx2Enabled) {
                cfg.ddcEnable = static_cast<uint8_t>(cfg.ddcEnable + DDC1);
                cfg.rate[1]   = static_cast<uint32_t>(rx2Rate);
            }
        } else {
            // From mi0bot console.cs:8430-8440 [v2.10.3.13-beta2]
            cfg.p1DdcConfig = 5;
            cfg.ddcEnable   = DDC0;
            cfg.syncEnable  = DDC1;
            cfg.rate[0]     = static_cast<uint32_t>(rx1Rate);
            cfg.rate[1]     = static_cast<uint32_t>(rx1Rate);
            cfg.cntrl1      = 0;
            cfg.cntrl2      = 0;
        }
    } else {
        if (!diversityEnabled && !psEnabled) {
            // From mi0bot console.cs:8444-8457 [v2.10.3.13-beta2]
            cfg.p1DdcConfig = 4;
            cfg.ddcEnable   = DDC0;
            cfg.syncEnable  = 0;
            cfg.rate[0]     = static_cast<uint32_t>(rx1Rate);
            cfg.cntrl1      = 0;
            cfg.cntrl2      = 0;

            if (rx2Enabled) {
                cfg.ddcEnable = static_cast<uint8_t>(cfg.ddcEnable + DDC1);
                cfg.rate[1]   = static_cast<uint32_t>(rx2Rate);
            }
        } else if (diversityEnabled && !psEnabled) {
            // From mi0bot console.cs:8458-8467 [v2.10.3.13-beta2]
            cfg.p1DdcConfig = 5;
            cfg.ddcEnable   = DDC0;
            cfg.syncEnable  = DDC1;
            cfg.rate[0]     = static_cast<uint32_t>(rx1Rate);
            cfg.rate[1]     = static_cast<uint32_t>(rx1Rate);
            cfg.cntrl1      = 0;
            cfg.cntrl2      = 0;
        } else { // transmitting and PS is ON
            // From mi0bot console.cs:8469-8488 [v2.10.3.13-beta2]
            //   else // transmitting and PS is ON
            //   {
            //       P1_DDCConfig = 6;
            //       DDCEnable = DDC0;
            //       SyncEnable = DDC1;
            //       Rate[0] = ps_rate;
            //       Rate[1] = ps_rate;
            //       if (hpsdr_model == HPSDRModel.HERMESLITE) // MI0BOT: HL2 can work at a high sample rate
            //       {
            //           Rate[0] = rx1_rate;
            //           Rate[1] = rx1_rate;
            //       }
            //       else
            //       {
            //           Rate[0] = ps_rate;
            //           Rate[1] = ps_rate;
            //       }
            //       cntrl1 = 4;
            //       cntrl2 = 0;
            //   }
            cfg.p1DdcConfig = 6;
            cfg.ddcEnable   = DDC0;
            cfg.syncEnable  = DDC1;
            // MI0BOT: HL2 can work at a high sample rate — always rx1_rate for HL2
            cfg.rate[0]     = static_cast<uint32_t>(rx1Rate);
            cfg.rate[1]     = static_cast<uint32_t>(rx1Rate);
            // From mi0bot console.cs:8486 [v2.10.3.13-beta2]:
            //   cntrl1 = 4;
            //   cntrl2 = 0;
            //
            // Earlier PR #212 bench-fix (since reverted, J.J. KG4VCF,
            // 2026-05-07) changed this to `cntrl1 = 0` citing a "ramdor-
            // Thetis HL2 PS-MOX session" pcap.  The deep cross-codebase
            // review (PR #212 follow-up) found ramdor-Thetis has no HL2
            // case in UpdateDDCs at all, so the comparison was invalid;
            // mi0bot Thetis (the working reference on the user's HL2 +
            // N2ADR + amp bench) explicitly emits cntrl1=4.  Restored
            // here to match mi0bot byte-for-byte.  The actual PureSignal
            // breakthrough on user's bench came from the
            // setTxStepAttenuation negative-clamp fix in
            // P1RadioConnection.cpp, NOT this byte.
            cfg.cntrl1      = 4;
            cfg.cntrl2      = 0;

            // Phase 3M-4 mi0bot audit: PS DDC pair indices for HL2 / Hermes /
            // ANAN-10 / ANAN-100 (nddc=4 family).
            //
            // From mi0bot ChannelMaster/networkproto1.c:549-553
            // [v2.10.3.13-beta2] MetisReadThreadMainLoop_HL2 case 4:
            //   xrouter(0, 0, 0, spr, prn->RxBuff[0]);   // DDC0 → main RX
            //   twist(spr, 2, 3, 1);                      // DDC2+DDC3 → PS pair
            //   xrouter(0, 0, 2, spr, prn->RxBuff[1]);    // DDC1 → secondary RX
            //
            // Plus mi0bot console.cs:8757-8762 [v2.10.3.13-beta2] GetDDC()
            // HL2 P1 PS-MOX (tot=5: MOX=1, Diversity=0, PS=1):
            //   rx1 = 0; rx2 = 1; psrx = 2; pstx = 3;
            //
            // psFbDdc=2 (PS feedback / loopback from PA — the ADC1-routed
            // path enabled by cntrl1=4) and txMonDdc=3 (TX monitor — direct
            // sample of the TX I/Q feed).  PsccPump consumes
            // iqDataReceived(2, ...) and iqDataReceived(3, ...) as the
            // pscc(channel, size, tx, rx) pair.
            cfg.psFbDdc  = 2;     // RX2 audio (rx2=1) but during PS-MOX the
                                  // firmware routes loopback into slot 2
            cfg.txMonDdc = 3;     // TX monitor in slot 3
        }
    }

    return cfg;
}

// =================================================================
// P1CodecHl2::applyDdcAssignment  (Phase 3F Sub-Epic B Task 9)
// =================================================================
//
// Porting from mi0bot-Thetis console.cs:8409-8488 [v2.10.3.13-beta2]
// (UpdateDDCs, HPSDRModel.HERMESLITE case grouped with HERMES/ANAN10/ANAN100).
//
//MI0BOT  [HL2 case-statement marker at console.cs:8409 `case HPSDRModel.HERMESLITE: // MI0BOT: HL2`]
//
// Original C# logic quoted:
//   case HPSDRModel.HERMES:
//   case HPSDRModel.HERMESLITE:     // MI0BOT: HL2
//   case HPSDRModel.ANAN10:
//   case HPSDRModel.ANAN100:
//       P1_rxcount = 4;             // RX4 used for puresignal feedback
//       nddc = 4;
//       if (!_mox)
//       {
//           if (!diversity_enabled)
//           {
//               P1_DDCConfig = 4;
//               DDCEnable = DDC0; SyncEnable = 0;
//               Rate[0] = rx1_rate; cntrl1 = 0; cntrl2 = 0;
//               if (rx2_enabled) { DDCEnable += DDC1; Rate[1] = rx2_rate; }
//           }
//           else
//           {
//               P1_DDCConfig = 5;
//               DDCEnable = DDC0; SyncEnable = DDC1;
//               Rate[0] = rx1_rate; Rate[1] = rx1_rate; cntrl1 = 0; cntrl2 = 0;
//           }
//       }
//       else
//       {
//           if (!diversity_enabled && !puresignal_enabled)
//           {
//               P1_DDCConfig = 4;
//               DDCEnable = DDC0; SyncEnable = 0;
//               Rate[0] = rx1_rate; cntrl1 = 0; cntrl2 = 0;
//               if (rx2_enabled) { DDCEnable += DDC1; Rate[1] = rx2_rate; }
//           }
//           else if (diversity_enabled && !puresignal_enabled)
//           {
//               P1_DDCConfig = 5;
//               DDCEnable = DDC0; SyncEnable = DDC1;
//               Rate[0] = rx1_rate; Rate[1] = rx1_rate; cntrl1 = 0; cntrl2 = 0;
//           }
//           else // transmitting and PS is ON
//           {
//               P1_DDCConfig = 6;
//               DDCEnable = DDC0; SyncEnable = DDC1;
//               Rate[0] = ps_rate; Rate[1] = ps_rate;
//               if (hpsdr_model == HPSDRModel.HERMESLITE) // MI0BOT: HL2 can work at a high sample rate
//               {
//                   Rate[0] = rx1_rate;
//                   Rate[1] = rx1_rate;
//               }
//               else { Rate[0] = ps_rate; Rate[1] = ps_rate; }
//               cntrl1 = 4; cntrl2 = 0;
//           }
//       }
//
// NereusSDR architectural note:
//   Streams 0 and 1 map to DDC0 and DDC1, per the mi0bot rx2_enabled blocks
//   restored below (console.cs:8425-8429 and :8453-8457 [v2.10.3.13-beta2]).
//   Streams 2-4 are never assigned: no arm of the mi0bot HERMESLITE case
//   enables anything above DDC1, and DDC2/DDC3 are the PureSignal pair
//   (console.cs:8757-8762 [v2.10.3.13-beta2] GetDDC()).
//   The diversity path (P1_DDCConfig=5) is logically available but HL2 hardware
//   does not support RX diversity in practice; retained for completeness.
//   See docs/architecture/2026-07-31-hl2-slice-cap-design.md for the receiver
//   capacity this function is expected to expose.
//
// mi0bot divergence from ramdor: the PS-MOX branch uses rx1_rate (not ps_rate=192k)
//   for HL2, preserving high-rate operation through PureSignal TX.
//   From mi0bot console.cs:8476-8479 [v2.10.3.13-beta2]:
//     if (hpsdr_model == HPSDRModel.HERMESLITE) // MI0BOT: HL2 can work at a high sample rate
//     {   Rate[0] = rx1_rate; Rate[1] = rx1_rate;  }
// =================================================================
DdcAssignment P1CodecHl2::applyDdcAssignment(
    const CodecContext& ctx,
    const std::array<SliceConfig, 5>& slices) const
{
    DdcAssignment a{};
    constexpr int DDC0bit = 1;
    constexpr int DDC1bit = 2;

    // From mi0bot console.cs:8412-8413 [v2.10.3.13-beta2]:
    //   case HPSDRModel.HERMESLITE: // MI0BOT: HL2 (at console.cs:8409)
    //   P1_rxcount = 4;   // RX4 used for puresignal feedback
    //   nddc = 4;
    //MI0BOT  [HL2 case-statement marker at console.cs:8409 — within ±5 of 8412]
    a.p1RxCount = 4;  // RX4 used for puresignal feedback
    a.nDdc = 4;

    // Phase 3F: stream 0 on DDC0, stream 1 on DDC1.
    //
    // Streams 2-4 are never assigned on the HL2. No arm of the mi0bot
    // HERMESLITE case enables anything above DDC1, and DDC2/DDC3 are the
    // PureSignal pair (mi0bot console.cs:8757-8762 [v2.10.3.13-beta2]
    // GetDDC() returns rx1 = 0; rx2 = 1; psrx = 2; pstx = 3 for HL2 P1
    // PS-MOX).
    //
    // Neither stream is assumed live. A slice-B-only configuration is
    // reachable whenever slice A is removed from a two-slice layout, and an
    // early return on slices[0] stranded it.
    const bool rx1Live = slices[0].live;
    const bool rx2Live = slices[1].live;
    const int  rx1Rate = rx1Live ? slices[0].sampleRateHz : 0;
    const int  rx2Rate = rx2Live ? slices[1].sampleRateHz : 0;

    if (rx1Live) { a.streamDdc[0] = 0; }

    if (!ctx.mox) {
        if (!ctx.diversity) {
            // From mi0bot console.cs:8417-8430 [v2.10.3.13-beta2]:
            //   P1_DDCConfig = 4; DDCEnable = DDC0; SyncEnable = 0;
            //   Rate[0] = rx1_rate; cntrl1 = 0; cntrl2 = 0;
            //   if (rx2_enabled)
            //   {
            //       DDCEnable += DDC1;
            //       Rate[1] = rx2_rate;
            //   }
            a.p1DdcConfig = 4;
            a.ddcEnable = DDC0bit;
            a.syncEnable = 0;
            a.rate[0] = rx1Rate;
            a.adcCtrl1 = 0;
            a.adcCtrl2 = 0;

            // From mi0bot console.cs:8425-8429 [v2.10.3.13-beta2]
            if (rx2Live) {
                a.ddcEnable += DDC1bit;
                a.rate[1] = rx2Rate;
                a.streamDdc[1] = 1;
            }
        } else {
            // From mi0bot console.cs:8432-8440 [v2.10.3.13-beta2]:
            //   P1_DDCConfig = 5; DDCEnable = DDC0; SyncEnable = DDC1;
            //   Rate[0] = rx1_rate; Rate[1] = rx1_rate; cntrl1 = 0; cntrl2 = 0;
            a.p1DdcConfig = 5;
            a.ddcEnable = DDC0bit;
            a.syncEnable = DDC1bit;
            a.rate[0] = rx1Rate;
            a.rate[1] = rx1Rate;
            a.adcCtrl1 = 0;
            a.adcCtrl2 = 0;
        }
    } else {
        if (!ctx.diversity && !ctx.puresignalRun) {
            // From mi0bot console.cs:8444-8458 [v2.10.3.13-beta2]:
            //   P1_DDCConfig = 4; DDCEnable = DDC0; SyncEnable = 0;
            //   Rate[0] = rx1_rate; cntrl1 = 0; cntrl2 = 0;
            //   if (rx2_enabled)
            //   {
            //       DDCEnable += DDC1;
            //       Rate[1] = rx2_rate;
            //   }
            a.p1DdcConfig = 4;
            a.ddcEnable = DDC0bit;
            a.syncEnable = 0;
            a.rate[0] = rx1Rate;
            a.adcCtrl1 = 0;
            a.adcCtrl2 = 0;

            // From mi0bot console.cs:8453-8457 [v2.10.3.13-beta2]
            if (rx2Live) {
                a.ddcEnable += DDC1bit;
                a.rate[1] = rx2Rate;
                a.streamDdc[1] = 1;
            }
        } else if (ctx.diversity && !ctx.puresignalRun) {
            // From mi0bot console.cs:8459-8468 [v2.10.3.13-beta2]:
            //   P1_DDCConfig = 5; DDCEnable = DDC0; SyncEnable = DDC1;
            //   Rate[0] = rx1_rate; Rate[1] = rx1_rate; cntrl1 = 0; cntrl2 = 0;
            a.p1DdcConfig = 5;
            a.ddcEnable = DDC0bit;
            a.syncEnable = DDC1bit;
            a.rate[0] = rx1Rate;
            a.rate[1] = rx1Rate;
            a.adcCtrl1 = 0;
            a.adcCtrl2 = 0;
        } else { // transmitting and PS is ON
            // From mi0bot console.cs:8469-8488 [v2.10.3.13-beta2]:
            //   P1_DDCConfig = 6; DDCEnable = DDC0; SyncEnable = DDC1;
            //   Rate[0] = ps_rate; Rate[1] = ps_rate;
            //   if (hpsdr_model == HPSDRModel.HERMESLITE) // MI0BOT: HL2 can work at a high sample rate
            //   {   Rate[0] = rx1_rate; Rate[1] = rx1_rate;  }
            //   else { Rate[0] = ps_rate; Rate[1] = ps_rate; }
            //   cntrl1 = 4; cntrl2 = 0;
            a.p1DdcConfig = 6;
            a.ddcEnable  = DDC0bit;
            a.syncEnable = DDC1bit;
            // MI0BOT: HL2 can work at a high sample rate  [console.cs:8476]
            // HL2-specific divergence from ramdor: use rx1_rate, NOT ps_rate (192000).
            a.rate[0] = rx1Rate;
            a.rate[1] = rx1Rate;
            a.adcCtrl1 = 4;
            a.adcCtrl2 = 0;
            // PS DDC pair indices (same as applyPureSignalDdcConfig — psFbDdc=2, txMonDdc=3).
            // From mi0bot networkproto1.c:549-553 [v2.10.3.13-beta2].
            a.psFwdDdc = 0;
            a.psRevDdc = 1;
        }
    }

    return a;
}

} // namespace NereusSDR
