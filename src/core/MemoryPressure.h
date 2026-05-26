// =================================================================
// src/core/MemoryPressure.h  (NereusSDR-native)
// =================================================================
// 2026-05-26  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
//
// Cross-platform memory-pressure poll.  Returns a snapshot of:
//
//   * `compressing` -- true when the OS is signalling real memory
//     distress, NOT just background tiering.  On macOS this reads
//     kern.memorystatus_vm_pressure_level (1=Normal, 2=Warning,
//     4=Critical) and only flags when >= 2.  An earlier revision
//     used the compression-counter delta which fired constantly
//     because modern macOS treats compressed memory as a tier
//     and runs the compressor continuously regardless of system
//     pressure -- the kernel's own classification is the right
//     signal.  Linux: swap-out delta > 0.  Windows: working set
//     trimmed by the kernel.  When this is true, large allocations
//     or memory touches become variable-latency and contribute to
//     jitter independently of CPU scheduling.
//
//   * `footprintMb` -- current process RSS / phys_footprint in MiB.
//     Less actionable but useful for spotting runaway growth.
//
// Intended to be called once per second from a main-thread timer.
// Soft-fails to (false, 0.0) on platforms where the underlying
// system call isn't available.
// =================================================================
#pragma once

namespace NereusSDR {

struct MemoryPressureSample {
    bool   compressing{false};
    double footprintMb{0.0};
};

// Poll the OS for the current memory-pressure indicators.  Internally
// stateful (remembers the previous compression counter so it can
// compute a delta), so call from a single thread at a steady cadence.
MemoryPressureSample pollMemoryPressure();

} // namespace NereusSDR
