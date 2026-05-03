// no-port-check: NereusSDR-original glue stub. Not a port of Thetis logic;
// provides the SetTXFixedGain / SetTXFixedGainRun symbols so the bundled
// wdsp_static library exposes the API surface that Thetis's ChannelMaster
// module exports. The real Thetis implementation at
// `Project Files/Source/ChannelMaster/txgain.c [v2.10.3.13]` writes
// `pcm->xmtr[txid].pgain->Igain / Qgain` under cs_update0; that pcm-backed
// storage is part of the wider ChannelMaster module, which NereusSDR has
// not bundled yet (only individual primitives like cmbuffs.c have been
// ported into the NereusSDR src/ tree, see TxMicSource).
//
// Until the ChannelMaster TXGAIN structure is ported, this stub stores
// the most-recent fixed-gain value into a per-channel static so the symbol
// resolves and downstream callers can be unit-tested. The C++ wrapper at
// src/core/TxChannel.cpp `setTxFixedGain` already idempotency-suppresses
// duplicate calls before reaching this entry, so the stub is hit once per
// distinct value.

/*  txgain_stub.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2026 J.J. Boyd, KG4VCF (NereusSDR-original glue stub)

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

boydsoftprez@gmail.com

*/

// =================================================================
// third_party/wdsp/src/txgain_stub.c (NereusSDR)
// =================================================================
//
// Upstream source: Thetis Project Files/Source/ChannelMaster/txgain.c
// (Warren Pratt, NR0V) supplies the `SetTXFixedGain` / `SetTXFixedGainRun`
// API surface that NereusSDR provides via this glue. NereusSDR does NOT
// reproduce the Thetis implementation here — only a minimum storage stub
// that satisfies the linker. A future phase that ports the ChannelMaster
// TXGAIN pcm structure proper can drop this file and replace it with the
// real `Project Files/Source/ChannelMaster/txgain.c` byte-for-byte port.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-03 — NereusSDR-original glue stub created by J.J. Boyd
//                 (KG4VCF) during issue #167 Phase 1 Agent 1C, with
//                 AI-assisted implementation via Anthropic Claude Code.
//                 Implements `SetTXFixedGain` / `SetTXFixedGainRun` minimum
//                 surface; per-channel static array stores the most-recent
//                 (Igain, Qgain) pair plus the run flag.
// =================================================================

#include <stdint.h>

// NereusSDR uses a small fixed channel cap (RX0/RX1 + TX1 == 3 typically;
// 16 is generous and matches the kMaxChannels-style sentinel used in
// other NereusSDR callsites).
#define NEREUS_TXGAIN_STUB_MAX_CHANNELS 16

static struct {
    double i_gain;
    double q_gain;
    int    run_fixed;
} s_txgain_state[NEREUS_TXGAIN_STUB_MAX_CHANNELS];

// Mirrors the Thetis ChannelMaster API surface — see
// `Project Files/Source/ChannelMaster/txgain.c:117-134 [v2.10.3.13]`.
// The Thetis impl walks `pcm->xmtr[txid].pgain` and takes cs_update0 around
// the field write; NereusSDR's stub stores into a flat per-channel array
// instead, since the pcm structure is not yet ported.

void SetTXFixedGain(int txid, double Igain, double Qgain)
{
    if (txid < 0 || txid >= NEREUS_TXGAIN_STUB_MAX_CHANNELS) return;
    s_txgain_state[txid].i_gain = Igain;
    s_txgain_state[txid].q_gain = Qgain;
}

void SetTXFixedGainRun(int txid, int run)
{
    if (txid < 0 || txid >= NEREUS_TXGAIN_STUB_MAX_CHANNELS) return;
    s_txgain_state[txid].run_fixed = run;
}
