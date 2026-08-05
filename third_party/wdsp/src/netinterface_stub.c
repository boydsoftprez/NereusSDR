// no-port-check: NereusSDR-original glue stub. Not a port of Thetis logic;
// provides the SetADCSupply and LRAudioSwap symbols so the bundled wdsp_static
// library exposes the API surface that Thetis's ChannelMaster module exports.
// The real Thetis implementations live at:
//   Project Files/Source/ChannelMaster/txgain.c:164 [v2.10.3.15] — SetADCSupply
//   Project Files/Source/ChannelMaster/netInterface.c:1409 [v2.10.3.15] — LRAudioSwap
// Both operate on ChannelMaster's pcm / prn global structs; that pcm-backed
// storage is part of the wider ChannelMaster module, which NereusSDR has not
// bundled yet.
//
// Until the ChannelMaster structures are ported, these stubs store the
// most-recent values into file-static variables so the symbols resolve and
// downstream callers can be unit-tested. The C++ wrapper at
// src/core/WdspEngine.cpp `setAdcSupply` / `setLRAudioSwap` already
// idempotency-suppresses duplicate calls before reaching these entries, so
// each stub is hit once per distinct value.
//
// Mirrors the existing txgain_stub.c / ps_sync_stub.c convention used for
// other ChannelMaster-exported symbols NereusSDR consumes.

/*  netinterface_stub.c

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
// third_party/wdsp/src/netinterface_stub.c (NereusSDR)
// =================================================================
//
// Upstream sources:
//   Thetis Project Files/Source/ChannelMaster/txgain.c:164 [v2.10.3.15]
//     (Warren Pratt, NR0V) supplies the `SetADCSupply` API surface.
//     NereusSDR-original glue stores the most-recent value per txid.
//   Thetis Project Files/Source/ChannelMaster/netInterface.c:1409 [v2.10.3.15]
//     (Warren Pratt, NR0V) supplies the `LRAudioSwap` API surface.
//     NereusSDR-original glue stores the most-recent swap flag.
// A future phase that ports the ChannelMaster module proper can drop
// this file and replace it with byte-for-byte ports of the originals.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-22 — NereusSDR-original glue stub created by J.J. Boyd
//                 (KG4VCF) during ANAN-G2E port Phase B4'/B5', with
//                 AI-assisted implementation via Anthropic Claude Code
//                 (claude-sonnet-4-6). Implements `SetADCSupply` /
//                 `LRAudioSwap` minimum surface; per-txid static array
//                 (SetADCSupply) and global (LRAudioSwap) store the
//                 most-recent values so WdspEngine wrappers and unit
//                 tests can call through without a live ChannelMaster.
// =================================================================

// NereusSDR uses a small fixed transmitter cap (txid 0 is the only one
// used by current OpenHPSDR boards; 8 is generous and matches the
// kMaxXmtrs-style sentinel used in other NereusSDR callsites).
#define NEREUS_NET_STUB_MAX_TXID 8

static struct {
    int adc_supply;
} s_txgain_net_state[NEREUS_NET_STUB_MAX_TXID];

// Global L/R swap flag — mirrors prn->lr_audio_swap in Thetis's radionet struct.
// Only one radio connection is active at a time, so a global scalar is sufficient.
static int s_lr_audio_swap = 0;

// Mirrors Thetis ChannelMaster/txgain.c:164 [v2.10.3.15]:
//   PORT void SetADCSupply(int txid, int v)
//   {
//       TXGAIN a = pcm->xmtr[txid].pgain;
//       a->adc_supply = v;
//   }
// Used by WdspEngine::setAdcSupply() at connect time to register the
// per-board ADC supply voltage (33 V or 50 V) with the DSP engine's
// PA over-drive protection scaling path (xtxgain() in txgain.c:90-100).
void SetADCSupply(int txid, int v)
{
    if (txid < 0 || txid >= NEREUS_NET_STUB_MAX_TXID) { return; }
    s_txgain_net_state[txid].adc_supply = v;
}

// Mirrors Thetis ChannelMaster/netInterface.c:1409 [v2.10.3.15]:
//   PORT void LRAudioSwap(int swap)
//   {
//       if (prn->lr_audio_swap != swap)
//           prn->lr_audio_swap = swap;
//   }
// Used by WdspEngine::setLRAudioSwap() at connect time to register the
// per-board L/R audio sample-pair swap for the outbound audio stream
// (consumed by sendOutbound() in the P2/ETH audio path).
void LRAudioSwap(int swap)
{
    if (s_lr_audio_swap != swap) {
        s_lr_audio_swap = swap;
    }
}
