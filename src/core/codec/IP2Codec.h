// =================================================================
// src/core/codec/IP2Codec.h  (NereusSDR)
// =================================================================
//
// Per-board codec interface for the Protocol 2 command-packet compose
// layer. Subclasses model board variants:
// P2CodecOrionMkII handles the Orion-MKII / 7000D / 8000D / AnvelinaPro3
// family; P2CodecSaturn extends it with the G8NJJ Saturn BPF1 band-edge
// override for ANAN-G2 / G2-1K.
//
// P2RadioConnection owns std::unique_ptr<IP2Codec> chosen at connect time
// from m_hardwareProfile.model (see applyBoardQuirks()).
//
// Parallel to IP1Codec (which composes 5-byte C&C banks for Protocol 1);
// P2 uses four fixed-size command packets instead — CmdGeneral (60),
// CmdHighPriority (1444), CmdRx (1444), CmdTx (60).
//
// NereusSDR-original. Independently implemented from IP1Codec.h interface.
// No Thetis port; no PROVENANCE row.
// =================================================================

#pragma once

#include <QtGlobal>
#include <array>
#include "CodecContext.h"
#include "../DdcAssignment.h"
#include "../HpsdrModel.h"

namespace NereusSDR {

class IP2Codec {
public:
    virtual ~IP2Codec() = default;

    // Each composer fills the buffer with the post-Phase-B byte layout.
    // Buffers are caller-allocated and zero-initialized by the caller.
    // Sequence numbers (bytes 0-3 of each packet) are NOT stamped here —
    // the caller (P2RadioConnection::sendCmd*) stamps them just before
    // UDP transmission so they remain monotonic across retransmits.
    virtual void composeCmdGeneral     (const CodecContext& ctx, quint8 buf[60])   const = 0;
    virtual void composeCmdHighPriority(const CodecContext& ctx, quint8 buf[1444]) const = 0;
    virtual void composeCmdRx          (const CodecContext& ctx, quint8 buf[1444]) const = 0;
    virtual void composeCmdTx          (const CodecContext& ctx, quint8 buf[60])   const = 0;

    // PureSignal DDC config emission. Phase 3M-4 Task 5 + 2026-05-17 expansion.
    // Returns the wire-byte map describing how this board configures its
    // DDCs in every (mox × diversity × PS) state. Called from
    // ReceiverManager::updateDdcAssignment() (Task 6) on every MOX or PS
    // state change, and from P2RadioConnection::connectToRadio (issue
    // #263 fix) to seed the initial RX-only DDC topology.
    //
    // The `model` parameter selects the per-board branch:
    //   - G2-class (ANAN-100D / 200D / OrionMkII / 7000D / 8000D / G2 /
    //     G2-1K / ANVELINAPRO3): RX1 on DDC2, PS pair on DDC0+DDC1
    //   - Hermes-class (HERMES / ANAN10 / ANAN100): RX1 on DDC0,
    //     PS pair on DDC0+DDC1 (cntrl1=4 ADC steering on P1; P2 freq
    //     override at network.c:936-945 handles the steering on P2)
    //   - HermesII-class (ANAN10E / ANAN100B): same as Hermes-class but
    //     with nddc=2 instead of nddc=4
    //   - HPSDR / HERMESLITE / REDPITAYA / unknown: empty cfg
    //
    // P2 codec subclasses inherit one shared dispatch table via
    // P2CodecOrionMkII; P2CodecSaturn re-uses it untouched because Thetis
    // groups G2 and Saturn into the same case statement
    // (console.cs:8211-8218 [v2.10.3.13]).
    //
    // The `adcCtrl1` / `adcCtrl2` parameters carry the live ADC control
    // bytes (Thetis console.cs `rx_adc_ctrl1` / `rx_adc_ctrl2` members
    // [v2.10.3.13]) into the masked emission formulas. Ignored on 1-ADC
    // boards (Hermes / HermesII have no ADC selector bits — cfg.cntrl1
    // and cfg.cntrl2 are always 0 or 4 there).
    //
    // Source: ports the per-board branches in Thetis console.cs UpdateDDCs()
    // (lines 8186-8538) [v2.10.3.13].
    virtual PsDdcConfig applyPureSignalDdcConfig(
        HPSDRModel model,
        bool psEnabled,
        bool diversityEnabled,
        bool moxState,
        int rx1Rate,
        int rx2Rate,
        bool rx2Enabled,
        quint8 adcCtrl1,
        quint8 adcCtrl2
    ) const = 0;

    /// Phase 3F: produce a DDC assignment for up to 5 user slices.
    /// SliceConfig[0] = Slice A, [1] = B, [2] = C, [3] = D, [4] = E.
    /// Slices with .live=false are skipped. Backward compat: when only 1-2 slices
    /// are live and PS state matches, output is byte-faithful to Thetis console.cs:8186-8538
    /// (the existing applyPureSignalDdcConfig path remains for codec-internal use).
    /// See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §4.
    virtual DdcAssignment applyDdcAssignment(
        const CodecContext& ctx,
        const std::array<SliceConfig, 5>& slices) const = 0;
};

} // namespace NereusSDR
