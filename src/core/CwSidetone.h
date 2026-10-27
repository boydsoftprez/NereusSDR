// =================================================================
// src/core/CwSidetone.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis ChannelMaster sources:
//   Project Files/Source/ChannelMaster/sidetone.c
//   Project Files/Source/ChannelMaster/sidetone.h
// original licence from Thetis source is included below. sidetone.h
// carries one Windows-1252 byte (0x85) that tools read as a NEL line
// terminator; it sits in the file's closing SYMBOL GENERATION comment,
// not in the header copied here, which is unchanged.
//
// The computer sidetone (R-CW-39, D15): the sidetone generator's tone,
// its raised-cosine edges and its key-driven state machine, producing
// interleaved stereo into the audio engine's transmit monitor slot. One
// per engine; no Qt. Pitch, volume, run and key are atomics any thread
// may write while the audio thread renders.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the computer sidetone (R-CW-39, R-CW-51):
//               reimplemented in C++20 for NereusSDR from sidetone.c and
//               sidetone.h, lock-free for the audio drain. J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
// =================================================================

// --- From sidetone.c ---
/*  sidetone.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2024 Warren Pratt, NR0V

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

warren@pratt.one

*/

// --- From sidetone.h ---
/*  sidetone.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2024 Warren Pratt, NR0V

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

warren@pratt.one

*/

#pragma once

#include <atomic>
#include <vector>

#include "core/NereusCoreExport.h"

namespace NereusSDR {

// What NereusSDR keeps of the sidetone generator, and what it leaves out:
//
//   * Kept: calc_tone1, calc_rising_edge, calc_falling_edge, osc_init, osc
//     and xsidetone's LOW / RISE / HIGH / FALL machine on the key bit, with
//     the sidetone output (run_st, volume_st) written as interleaved L/R.
//   * Left out: the transmit IQ output (run_tx, volume_tx, IQ_polarity),
//     because the radio's own keyer makes the CW signal; the dot and dash
//     bits with calc_wpm_times (makedotSidetone, makedashSidetone), because
//     the paddle never keys the computer sidetone (spec C8); the key source
//     select (SetSidetoneSelectKey), because the CWX sender is its only
//     feed.
//   * Changed: the critical section becomes atomics, so the audio drain
//     never takes a lock (CLAUDE.md). A pitch change is picked up at the
//     next render; the edge tables are built in configure(), off the audio
//     thread, never while rendering.
class NEREUS_CORE_EXPORT CwSidetone {
public:
    // From Thetis cmaster.c:235-250 [v2.10.3.15]: create_sidetone's
    // "raised-cosine edge" (0) and "edge length" (0.005 s), and its
    // volume_sidetone (1.0).
    static constexpr int kRaisedCosineEdge = 0;
    static constexpr double kDefaultEdgeSeconds = 0.005;
    static constexpr double kDefaultVolume = 1.0;
    // From Thetis setup.designer.cs:38256-38285 [v2.10.3.15]: the CW pitch
    // default (udDSPCWPitch), 600 Hz.
    static constexpr double kDefaultPitchHz = 600.0;

    CwSidetone() = default;
    CwSidetone(const CwSidetone&) = delete;
    CwSidetone& operator=(const CwSidetone&) = delete;

    // Control thread, never while render() runs: the rate and the block
    // size, and the edge tables for the edge setEdge() last chose.
    void configure(int sampleRate, int blockSize);
    void setPitchHz(double hz);
    void setVolume(double v);
    void setRun(bool on);
    // 0 raised cosine, 0.005 s. Takes effect at the next configure().
    void setEdge(int edgeType, double edgeSeconds);
    // 0 = KEY-UP; 1 = KEY-DOWN. Atomic, any thread.
    void setKey(bool down);

    // Audio thread. Fills interleaved stereo float frames; returns false
    // when not running (and writes nothing).
    bool render(float* out, int frames);

    bool isRunning() const { return m_run.load(std::memory_order_acquire); }
    bool isKeyed() const { return m_key.load(std::memory_order_acquire); }
    double pitchHz() const { return m_pitchHz.load(std::memory_order_acquire); }
    double volume() const { return m_volume.load(std::memory_order_acquire); }
    int sampleRate() const { return m_rate; }
    int blockSize() const { return m_size; }
    // Samples in each edge (nrise, nfall): edge length times the rate.
    int edgeSamples() const { return m_nrise; }

private:
    // From Thetis sidetone.c:164-170 [v2.10.3.15].
    enum State { LOW, RISE, HIGH, FALL };

    void calcTone1();
    void calcRisingEdge();
    void calcFallingEdge();
    void oscInit();
    void osc();

    // Any thread.
    std::atomic<double> m_pitchHz{kDefaultPitchHz};
    std::atomic<double> m_volume{kDefaultVolume};
    std::atomic<bool> m_run{false};
    std::atomic<bool> m_key{false};
    std::atomic<int> m_edgeTypeWanted{kRaisedCosineEdge};
    std::atomic<double> m_edgeSecondsWanted{kDefaultEdgeSeconds};

    // configure() only.
    int m_rate{0};                  // sample rate
    int m_size{0};                  // buffer size
    int m_edgetype{kRaisedCosineEdge};  // cw edge; 0 -> raised_cosine
    double m_edgelength{kDefaultEdgeSeconds};  // length of a rising or falling edge (seconds)

    int m_nrise{0};
    std::vector<double> m_riseSamps;
    double m_riseDelta{0.0};
    double m_riseTheta{0.0};

    int m_nfall{0};
    std::vector<double> m_fallSamps;
    double m_fallDelta{0.0};
    double m_fallTheta{0.0};

    // Audio thread only.
    double m_pitch{0.0};            // pitch frequency for both sidetone and transmit (Hz)
    double m_tonePhs{0.0};
    double m_toneCosPhs{1.0};
    double m_toneSinPhs{0.0};
    double m_toneDelta{0.0};
    double m_toneCosdelta{1.0};
    double m_toneSindelta{0.0};

    int m_state{LOW};
    double m_toneOut0{0.0};
    double m_toneOut1{0.0};
    double m_outI{0.0};             // I value without volume applied
    double m_outQ{0.0};             // Q value without volume applied
    int m_ntimer{0};
};

} // namespace NereusSDR
