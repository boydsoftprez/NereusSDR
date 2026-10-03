// =================================================================
// src/core/audio/PipeWireOutputFrames.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R-R3-44 (R3 receiver audio fix wave).
//
// How many frames a PipeWire OUTPUT stream fills in one process cycle.
// PipeWire hands the stream a buffer (datas[0].maxsize bytes) and, since
// 0.3.49, pw_buffer::requested: the frames the graph wants this cycle,
// usually far fewer than the buffer holds. Filling and counting the whole
// buffer every cycle ran the VAX output ahead of the graph's real clock, so
// a VAX feeder pacing by it pushed audio faster than an app read it.
//
// Pure and free of PipeWire headers, so it is tested on every platform.
// The build requires libpipewire >= 0.3.50 (CMakeLists.txt), which has the
// field.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23: Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#pragma once

#include <cstdint>

namespace NereusSDR {

/// Frames to fill this cycle: `requested` (pw_buffer::requested) when the
/// graph asked for a number, clamped to what the buffer holds
/// (maxsizeBytes / strideBytes); the whole buffer when it did not (0).
constexpr std::uint32_t pipeWireOutputFrames(std::uint64_t requested,
                                             std::uint32_t maxsizeBytes,
                                             std::uint32_t strideBytes)
{
    if (strideBytes == 0) {
        return 0;
    }
    const std::uint32_t capacity = maxsizeBytes / strideBytes;
    if (requested == 0 || requested > capacity) {
        return capacity;
    }
    return static_cast<std::uint32_t>(requested);
}

} // namespace NereusSDR
