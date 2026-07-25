// =================================================================
// src/core/DdcAssignment.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original aggregator struct. Field names and
//   field-level comments cite Thetis console.cs UpdateDDCs for semantic
//   context only; this file contains no ported logic, no ported constants,
//   and no ported algorithms. It is a NereusSDR-native data structure whose
//   shape is informed by Thetis UpdateDDCs output conventions. No PROVENANCE
//   row is needed.
//
// Shape informed by Thetis:
//   Project Files/Source/Console/console.cs (UpdateDDCs output state)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27. Created in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//               with AI-assisted transformation via Anthropic Claude Code.
//               Extension of existing PsDdcConfig (Phase 3M-4) to carry
//               up to 5 user slices' DDC assignments per Phase 3F design
//               docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §4.
// =================================================================
#pragma once

#include <array>

namespace NereusSDR {

/// Per-DDC assignment emitted by codec, consumed by P2RadioConnection (P2 wire bytes)
/// or NetworkIO calls (P1). Strict extension of PsDdcConfig from Phase 3M-4.
/// When only RX1/RX2 + PS are active, this struct's fields mirror the output of
/// Thetis UpdateDDCs (console.cs:8186-8538 [v2.10.3.15]) for byte-faithful behaviour.
struct DdcAssignment {
    /// Per-DDC sample rate in Hz, 0 = not enabled. Index 0..7 (Thetis convention).
    std::array<int, 8> rate{};

    /// Bitmask: DDC0=bit0, DDC1=bit1, ..., DDC6=bit6. Which DDCs the radio enables.
    int ddcEnable {0};

    /// Bitmask: which DDCs sync to DDC0 (typically DDC1 for diversity or PS pair).
    int syncEnable {0};

    /// ADC routing byte for DDC0-3: 2 bits per DDC, value 0=ADC0, 1=ADC1, 2=ADC2(PS-FB).
    int adcCtrl1 {0};

    /// ADC routing byte for DDC4-7.
    int adcCtrl2 {0};

    /// P1-only opaque preset (0-6), board-specific firmware interpretation.
    /// Ignored by P2 codecs.
    int p1DdcConfig {0};

    /// P1 diversity flag.
    int p1Diversity {0};

    /// P1 receiver count (typically 1-7).
    int p1RxCount {0};

    /// Total enabled DDC count (sum of bits in ddcEnable).
    int nDdc {0};

    /// PureSignal feedback forward DDC index (-1 if PS not active).
    int psFwdDdc {-1};

    /// PureSignal feedback reverse DDC index (-1 if PS not active).
    int psRevDdc {-1};

    // Phase 3F Sub-Epic I Task 7: per-slice DDC choice. Index = slice
    // index, value = DDC number, -1 when that slice is not live. The wire
    // bytes above are derived from this; keeping it explicit lets
    // RadioModel publish the mapping onto each SliceModel without
    // re-deriving it from the enable bitmask.
    int sliceDdc[5] = {-1, -1, -1, -1, -1};
};

} // namespace NereusSDR
