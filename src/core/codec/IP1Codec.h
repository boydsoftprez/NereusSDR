// =================================================================
// src/core/codec/IP1Codec.h  (NereusSDR)
// =================================================================
//
// Per-board codec interface for the Protocol 1 EP2 C&C bank compose
// layer. Subclasses model Thetis's behavioral splits literally:
// P1CodecStandard mirrors ramdor's WriteMainLoop; P1CodecHl2
// mirrors mi0bot's WriteMainLoop_HL2; P1CodecAnvelinaPro3 and
// P1CodecRedPitaya extend Standard with the per-board carve-outs
// ramdor's main loop already gates with `if (HPSDRModel == ...)`.
//
// P1RadioConnection owns std::unique_ptr<IP1Codec> chosen at connect
// time from m_hardwareProfile.model (see applyBoardQuirks()).
//
// NereusSDR-original. No Thetis port; no PROVENANCE row.
// Independently implemented from Protocol 1 interface design.
// =================================================================

#pragma once

#include <array>
#include <QtGlobal>
#include "CodecContext.h"
#include "../DdcAssignment.h"
#include "../HpsdrModel.h"

namespace NereusSDR {

class IP1Codec {
public:
    virtual ~IP1Codec() = default;

    // Emit the 5-byte C&C payload for the given bank into out[0..4].
    // bank ∈ [0, maxBank()].
    virtual void composeCcForBank(int bank, const CodecContext& ctx,
                                  quint8 out[5]) const = 0;

    // Highest bank index this codec emits. Standard = 16, AnvelinaPro3 = 17,
    // Hl2 = 18. P1RadioConnection cycles 0..maxBank() round-robin.
    virtual int  maxBank() const = 0;

    // True for HL2 — when the I2C queue (Phase E) is non-empty, frame
    // compose is overridden to carry I2C TLV bytes instead of normal
    // C&C. False for Standard / AnvelinaPro3 / RedPitaya.
    virtual bool usesI2cIntercept() const { return false; }

    // PureSignal DDC config emission. Phase 3M-4 Task 5.
    // Returns the wire-byte map describing how this board configures its
    // DDCs when PureSignal is active. Called from
    // ReceiverManager::updateDdcAssignment() (Task 6) to drive the PS
    // branches of per-board UpdateDDCs.
    //
    // The `model` parameter is required because P1CodecStandard maps to
    // multiple HpsdrModel values (HERMES/ANAN10/ANAN100/HermesII/ANAN10E/
    // ANAN100B), each with a distinct PS branch in Thetis UpdateDDCs.
    // Subclasses that map 1:1 to a single board family (P1CodecHl2,
    // P1CodecAnvelinaPro3, P1CodecRedPitaya) ignore the parameter.
    //
    // The `adcCtrl1` / `adcCtrl2` parameters carry the live ADC control
    // bytes (Thetis console.cs `rx_adc_ctrl1` / `rx_adc_ctrl2` members
    // at lines 8230, 8254, 8264-8265, etc. [v2.10.3.13]) into the masked
    // emission formulas.  The PS-on G2-class branch substitutes its own
    // bit pattern into bits 2-3 of cntrl1 while preserving the caller's
    // other bits; the HermesII/HL2 branches override to literals.
    //
    // Source: ports the per-board branches in Thetis console.cs UpdateDDCs()
    // (lines 8186-8538) [v2.10.3.13] + mi0bot deltas at 8409-8488
    // [v2.10.3.13-beta2] for HL2.
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
