// =================================================================
// src/core/codec/P1CodecStandard.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/ChannelMaster/networkproto1.c:419-698 (WriteMainLoop)
//   original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Lifted from P1RadioConnection::composeCcForBank
//                which previously held the inline ramdor port; now
//                delegates here.
//   2026-10-26: CW transmit, bank12CwKeyer (banks 12-14 carry the CW
//               keyer, networkproto1.c:618-646 [v2.10.3.15]). J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
//   2026-11-19 - The codec holds the discovery firmware version, for the
//                two-receiver map of an early Hermes / ANAN-10 / ANAN-100.
//                Brian Sutton (W4KZO), AI-assisted via xAI Grok.
//   2026-11-25 - That map covers only codes 14 to 19, and only those three
//                models (TAPR release notes). J.J. Boyd (KG4VCF),
//                AI-assisted via Claude Code.
//   2026-11-26 - hermesPureSignalOnTwoReceivers renamed hermesHasTwoReceivers:
//                it tells the receiver count. TAPR Hermes v1.x has no
//                PureSignal feedback. J.J. Boyd (KG4VCF), AI-assisted via
//                Claude Code / Opus 5.5.
// =================================================================
//
// === Verbatim Thetis ChannelMaster/networkproto1.c header (lines 1-45) ===
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

#pragma once

#include "IP1Codec.h"

namespace NereusSDR {

// ramdor WriteMainLoop port. Banks 0-16. Used for every non-HL2 P1
// board by default; AnvelinaPro3 and RedPitaya extend it with bank-17
// or bank-12 overrides (see those subclasses).
class P1CodecStandard : public IP1Codec {
public:
    void composeCcForBank(int bank, const CodecContext& ctx, quint8 out[5]) const override;
    int  maxBank() const override { return 16; }

    // Phase 3M-4 Task 5: PureSignal DDC config emission.  P1CodecStandard
    // dispatches on `model` because it covers multiple HpsdrModel values
    // (HERMES, ANAN10/10E, ANAN100/100B, ANAN100D, ANAN200D, ORIONMKII,
    // ANAN7000D, ANAN8000D, ANAN_G2, ANAN_G2_1K, HPSDR), each with a
    // distinct branch in Thetis console.cs UpdateDDCs.  AnvelinaPro3
    // shares the G2-class branch via override; RedPitaya has its own
    // branch via override.
    //
    // Source: Thetis console.cs:8186-8525 [v2.10.3.13]
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

    // Discovery code version (major*10+minor). 0 means unknown. Set by
    // P1RadioConnection when the codec is built.
    void setProtocol1FirmwareVersion(int version) { m_protocol1FirmwareVersion = version; }
    int protocol1FirmwareVersion() const { return m_protocol1FirmwareVersion; }
    // True for a Hermes, ANAN-10 or ANAN-100 whose firmware has two
    // receivers. Thetis has no such rule: it asks these models for four
    // whatever their firmware (console.cs:8387-8392 [v2.10.3.15]). The
    // receiver counts are from the TAPR release notes, TAPR/OpenHPSDR-Firmware
    // [@e7c6584] "Protocol 1/Hermes/previous Hermes versions/Release
    // notes_v1.3 to v3.1.txt": v1.4 added the second receiver and v1.9 still
    // has two; v2.0 has four, v2.1 to v2.8 four or five, v2.9 and later four.
    // Below v1.4 the notes give one receiver (v1.3) and five (v0.4), so those
    // codes keep the four-receiver map too.
    bool hermesHasTwoReceivers(HPSDRModel model) const
    {
        switch (model) {
            case HPSDRModel::HERMES:
            case HPSDRModel::ANAN10:
            case HPSDRModel::ANAN100:
                return m_protocol1FirmwareVersion >= kFirstTwoReceiverHermesCode
                    && m_protocol1FirmwareVersion < kFirstFourReceiverHermesCode;
            default:
                return false;
        }
    }

    DdcAssignment applyDdcAssignment(
        const CodecContext& ctx,
        const std::array<SliceConfig, 5>& slices) const override;

protected:
    // Helpers exposed so subclasses can call into specific banks they
    // don't override.
    void bank0(const CodecContext& ctx, quint8 out[5]) const;
    void bank10(const CodecContext& ctx, quint8 out[5]) const;
    void bank11(const CodecContext& ctx, quint8 out[5]) const;
    virtual void bank12(const CodecContext& ctx, quint8 out[5]) const;  // RedPitaya overrides

    // Bank 12's CW keyer half (C2 bit 6, C3, C4) from ctx.cw, ORed over the
    // attenuator bytes bank12 already wrote. RedPitaya's bank12 calls it too.
    // From Thetis networkproto1.c:618-628 [v2.10.3.15]
    static void bank12CwKeyer(const CodecContext& ctx, quint8 out[5]);

    // Phase 3M-4 Task 5: per-branch helpers reusable from subclasses
    // (AnvelinaPro3 = G2-class, RedPitaya overrides directly).  Each
    // helper transcribes one Thetis switch-case verbatim.
    PsDdcConfig psDdcConfigG2Class(
        bool psEnabled, bool diversityEnabled, bool moxState,
        int rx1Rate, int rx2Rate, bool rx2Enabled,
        quint8 adcCtrl1, quint8 adcCtrl2) const;

    PsDdcConfig psDdcConfigHermesClass(
        bool psEnabled, bool diversityEnabled, bool moxState,
        int rx1Rate, int rx2Rate, bool rx2Enabled) const;

    PsDdcConfig psDdcConfigHermesIIClass(
        bool psEnabled, bool diversityEnabled, bool moxState,
        int rx1Rate, int rx2Rate, bool rx2Enabled) const;

    // Plan Task 11: Thetis's Protocol 1 receiver layout per model family
    // (console.cs UpdateDDCs + GetDDC + cmaster.cs CMLoadRouterAll
    // [v2.10.3.15]). applyDdcAssignment dispatches on CodecContext::model;
    // AnvelinaPro3 and RedPitaya call the Orion-class helper directly.
    // Stream values and the PureSignal pair are Protocol 1 frame slots.
    DdcAssignment ddcAssignmentHermesClass(
        const CodecContext& ctx, const std::array<SliceConfig, 5>& slices) const;
    DdcAssignment ddcAssignmentHermesIIClass(
        const CodecContext& ctx, const std::array<SliceConfig, 5>& slices) const;
    DdcAssignment ddcAssignmentOrionClass(
        const CodecContext& ctx, const std::array<SliceConfig, 5>& slices,
        bool redPitaya) const;

    // TAPR Hermes release notes, cited above: v1.4 and v2.0.
    static constexpr int kFirstTwoReceiverHermesCode  = 14;
    static constexpr int kFirstFourReceiverHermesCode = 20;

    int m_protocol1FirmwareVersion{0};
};

} // namespace NereusSDR
