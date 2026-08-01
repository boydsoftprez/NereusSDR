/*
 * network.c
 * Copyright (C) 2015-2020 Doug Wigley (W5WC)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

// =================================================================
// src/core/codec/P2CodecOrionMkII.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources (multi-source) [v2.10.3.13]:
//   Project Files/Source/ChannelMaster/network.c:821-1248
//     (P2 CmdGeneral / CmdHighPriority / CmdRx / CmdTx packet builders)
//   Project Files/Source/Console/console.cs:8211-8521
//     (UpdateDDCs() per-board PureSignal DDC config: G2-class
//     8211-8295, Hermes-class 8378-8449, HermesII-class 8451-8521)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Lifted from P2RadioConnection inline compose
//                helpers (extracted in Phase 3P-B Task 1); now delegates
//                here once Task 7 cutover lands.
//   2026-05-17 — applyPureSignalDdcConfig dispatches on HPSDRModel and
//                ports the HermesII (ANAN-10E / ANAN-100B) and Hermes
//                (ANAN-10 / ANAN-100) P2 branches of Thetis console.cs
//                UpdateDDCs() (lines 8378-8521 [v2.10.3.13]). Closes
//                issue #263: ANAN-10E running community P2 firmware
//                disconnected after the 2 s connect watchdog because the
//                G2-class branch placed RX1 on DDC2 instead of DDC0.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
//
// === Verbatim Thetis Console/console.cs header (lines 1-50) ===
//
// //=================================================================
// // console.cs
// //=================================================================
// // Thetis is a C# implementation of a Software Defined Radio.
// // Copyright (C) 2004-2009  FlexRadio Systems
// // Copyright (C) 2010-2020  Doug Wigley
// // Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
// //
// // This program is free software; you can redistribute it and/or
// // modify it under the terms of the GNU General Public License
// // as published by the Free Software Foundation; either version 2
// // of the License, or (at your option) any later version.
// //
// // This program is distributed in the hope that it will be useful,
// // but WITHOUT ANY WARRANTY; without even the implied warranty of
// // MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// // GNU General Public License for more details.
// //
// // You should have received a copy of the GNU General Public License
// // along with this program; if not, write to the Free Software
// // Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
// //
// // You may contact us via email at: sales@flex-radio.com.
// // Paper mail may be sent to:
// //    FlexRadio Systems
// //    8900 Marybank Dr.
// //    Austin, TX 78750
// //    USA
// //
// //=================================================================
// // Modifications to support the Behringer Midi controllers
// // by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines.
// // Modifications for using the new database import function.  W2PA, 29 May 2017
// // Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019
// // Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
// //
// //============================================================================================//
// // Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// // ------------------------------------------------------------------------------------------ //
// // For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// // made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// // right to use, license, and distribute such code under different terms, including           //
// // closed-source and proprietary licences, in addition to the GNU General Public License      //
// // granted above. Nothing in this statement restricts any rights granted to recipients under  //
// // the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// // its original terms and is not affected by this dual-licensing statement in any way.        //
// // Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
// //============================================================================================//
// //
// // Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12
//
// =================================================================

#pragma once

#include "IP2Codec.h"
#include <QtGlobal>

namespace NereusSDR {

// Base P2 codec for the Orion-MKII / 7000D / 8000D / AnvelinaPro3 family.
// Saturn (ANAN-G2 / G2-1K) extends this with Saturn BPF1 band-edge
// override (Phase B Task 5, P2CodecSaturn).
//
// Source: network.c:821-1248 [@501e3f5]
class P2CodecOrionMkII : public IP2Codec {
public:
    void composeCmdGeneral     (const CodecContext& ctx, quint8 buf[60])   const override;
    void composeCmdHighPriority(const CodecContext& ctx, quint8 buf[1444]) const override;
    void composeCmdRx          (const CodecContext& ctx, quint8 buf[1444]) const override;
    void composeCmdTx          (const CodecContext& ctx, quint8 buf[60])   const override;

    // Phase 3M-4 Task 5 + 2026-05-17 expansion: PureSignal DDC config
    // emission. Dispatches on `model` and returns the per-board branch:
    //   - G2-class (ANAN-100D / 200D / OrionMkII / 7000D / 8000D / G2 /
    //     G2-1K / ANVELINAPRO3): psDdcConfigG2Class
    //     Source: Thetis console.cs:8211-8295 [v2.10.3.13].
    //   - Hermes-class (HERMES / ANAN10 / ANAN100): psDdcConfigHermesClass
    //     Source: Thetis console.cs:8378-8449 [v2.10.3.13].
    //   - HermesII-class (ANAN10E / ANAN100B): psDdcConfigHermesIIClass
    //     Source: Thetis console.cs:8451-8521 [v2.10.3.13].
    //   - Other models (HPSDR / HERMESLITE / REDPITAYA / unknown): empty
    //     cfg (no PureSignal hardware; matches Thetis fall-through).
    //
    // The 1-ADC branches (Hermes, HermesII) are required for community P2
    // firmware on those boards — without them the codec selects the
    // G2-class DDC2 layout and the radio never streams I/Q (issue #263).
    PsDdcConfig applyPureSignalDdcConfig(
        HPSDRModel model,
        bool psEnabled,
        bool diversityEnabled,
        bool moxState,
        int rx1Rate,
        int rx2Rate,
        bool rx2Enabled,
        quint8 adcCtrl1,
        quint8 adcCtrl2
    ) const override;

    // Phase 3F Sub-Epic B Task 2: stub. Real implementation in Task 7
    // per Thetis UpdateDDCs OrionMkII/G2-class branch.
    DdcAssignment applyDdcAssignment(
        const CodecContext& ctx,
        const std::array<SliceConfig, 5>& slices) const override;

protected:
    // Build Alex0 (bytes 1432-1435) and Alex1 (bytes 1428-1431) 32-bit registers.
    // Protected so P2CodecSaturn can override Alex1 with Saturn BPF1 band-edge bits.
    // Source: network.c:1040-1050 [@501e3f5]
    virtual quint32 buildAlex0(const CodecContext& ctx) const;
    virtual quint32 buildAlex1(const CodecContext& ctx) const;

    // Per-board PsDdcConfig branches.  Marked protected so subclasses
    // (P2CodecSaturn) can call them directly if they want to compose a
    // different dispatch table.  See Thetis console.cs UpdateDDCs()
    // [v2.10.3.13]:
    //   psDdcConfigG2Class       — console.cs:8211-8295
    //   psDdcConfigHermesClass   — console.cs:8378-8449
    //   psDdcConfigHermesIIClass — console.cs:8451-8521
    static PsDdcConfig psDdcConfigG2Class(
        bool psEnabled, bool diversityEnabled, bool moxState,
        int rx1Rate, int rx2Rate, bool rx2Enabled,
        quint8 adcCtrl1, quint8 adcCtrl2) noexcept;

    static PsDdcConfig psDdcConfigHermesClass(
        bool psEnabled, bool diversityEnabled, bool moxState,
        int rx1Rate, int rx2Rate, bool rx2Enabled) noexcept;

    static PsDdcConfig psDdcConfigHermesIIClass(
        bool psEnabled, bool diversityEnabled, bool moxState,
        int rx1Rate, int rx2Rate, bool rx2Enabled) noexcept;

    // Write a 32-bit value big-endian at offset into buf.
    static void writeBE32(quint8* buf, int offset, quint32 value);

    // Convert frequency in Hz to NCO phase word (2^32 / 122880000 * freqHz),
    // scaled by `factor` (Thetis FreqCorrectionFactor; 1.0 = no correction).
    // Source: network.c:936-1005; setup.cs:14036-14050 [@501e3f5]
    static quint32 hzToPhaseWord(quint64 freqHz, double factor);

    static constexpr int kMaxDdc = 7;
    static constexpr int kBufLen = 1444;
};

} // namespace NereusSDR
