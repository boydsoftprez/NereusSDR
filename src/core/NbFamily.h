// =================================================================
// src/core/NbFamily.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/ChannelMaster/cmaster.c (NB/NB2 create defaults)
//   Project Files/Source/Console/HPSDR/specHPSDR.cs (P/Invoke signatures)
//   Project Files/Source/Console/console.cs (chkNB tri-state cycle)
//   Project Files/Source/Console/setup.cs (NB1 UI scaling factors)
//   Project Files/Source/wdsp/nob.h, nobII.h, snb.h (Warren Pratt, NR0V)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-22 — New NereusSDR facade class for the NB / NB2 / SNB
//                family. The WDSP C functions it wraps retain their
//                Warren Pratt / Richard Samphire copyrights verbatim
//                in third_party/wdsp/src/*.c headers.
//                Authored by J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Anthropic Claude Code.
// =================================================================

// --- From cmaster.c ---
/*  cmaster.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014-2019 Warren Pratt, NR0V

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

warren@wpratt.com

*/

// --- From Project Files/Source/Console/HPSDR/specHPSDR.cs ---
/*
*
* Copyright (C) 2010-2018  Doug Wigley
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// --- From nob.h ---
/*  nob.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2014 Warren Pratt, NR0V

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

warren@wpratt.com

*/

// --- From nobII.h ---
/*  nobII.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014 Warren Pratt, NR0V

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

warren@wpratt.com

*/

// --- From snb.h ---
/*  snb.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2015, 2016 Warren Pratt, NR0V

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

warren@wpratt.com

*/

#pragma once

#include <atomic>

#include "WdspTypes.h"

namespace NereusSDR {

// NB1/NB2 tuning knobs. Field units chosen to match Thetis setup.cs
// user-visible units (milliseconds for times, linear for threshold)
// so UI sliders round-trip without conversion; WDSP calls are converted
// to seconds at the wdsp_api.h boundary.
//
// NB1 defaults below are Thetis's EFFECTIVE runtime values, not cmaster.c's
// create-time seeds. cmaster.c:49-53 initializes WDSP with tau=0.0001s,
// hangtime=0.0001s, advtime=0.0001s, threshold=30.0, but Thetis's Setup
// page loads its designer-default slider positions on startup — per
// setup.designer.cs:44459-44604 [v2.10.3.13] udDSPNBTransition.Value=0.01,
// udDSPNBLead=0.01, udDSPNBLag=0.01 (ms), udDSPNB.Value=30 — and
// ValueChanged handlers at setup.cs:8572,16222,16229,16236 [v2.10.3.13]
// immediately push the scaled values (×0.165 for threshold, ×0.001 for
// times) to WDSP. Net effect: every Thetis user's WDSP sees 4.95 / 10 µs
// once the Setup page is ever touched. Matching that here avoids the
// 6-10× "why isn't NB doing anything" disparity a cmaster.c-literal
// default would ship with.
struct NbTuning
{
    // NB1 — applied via create_anbEXT + SetEXTANB* post-create setters.
    double nbTauMs       = 0.01;   // Thetis effective: udDSPNBTransition=0.01 ms × 0.001 = 10 µs
    double nbHangMs      = 0.01;   // Thetis effective: udDSPNBLag       =0.01 ms × 0.001 = 10 µs
    double nbAdvMs       = 0.01;   // Thetis effective: udDSPNBLead      =0.01 ms × 0.001 = 10 µs
    double nbBacktau     = 0.05;   // cmaster.c:52 backtau=0.05 s (stored in s; no Thetis UI to override)
    double nbThreshold   = 4.95;   // Thetis effective: udDSPNB=30 × 0.165 = 4.95 (cmaster.c:53 seed=30.0 is immediately overridden)

    // NB2 — applied via create_nobEXT + SetEXTNOB* post-create setters.
    int    nb2Mode       = 0;      // cmaster.c:61   mode=0 (zero-fill)
    double nb2SlewMs     = 0.1;    // cmaster.c:62/64 advslewtime=hangslewtime=0.0001 s
    double nb2AdvMs      = 0.1;    // cmaster.c:63   advtime=0.0001 s
    double nb2HangMs     = 0.1;    // cmaster.c:65   hangtime=0.0001 s
    double nb2MaxImpMs   = 25.0;   // cmaster.c:66   max_imp_seq_time=0.025 s
    double nb2Backtau    = 0.05;   // cmaster.c:67   backtau=0.05 s (stored in s, not ms)
    double nb2Threshold  = 30.0;   // cmaster.c:68   threshold=30.0
};

// Cycles the NbMode on user toggle: Off → NB → NB2 → Off.
// From Thetis console.cs:42476-42482 [v2.10.3.13] — space-bar increment
// and console.cs:43513-43560 [v2.10.3.13] — chkNB CheckState transition.
NbMode cycleNbMode(NbMode current);

// Wrapper facade. One instance per RxChannel. Holds no WDSP-side ownership —
// the underlying anb/nob objects are owned by the channel-master inside WDSP
// and keyed by channel id. This class centralizes the API surface and the
// audio-thread-safe mode flag.
class NbFamily
{
public:
    // channelId is the WDSP channel (== RxChannel::m_channelId).
    // sampleRate is the input sample rate handed to create_anbEXT/nobEXT.
    // bufferSize is the number of COMPLEX samples per xanbEXTF/xnobEXTF call
    // (matches fexchange2 input buffer size).
    NbFamily(int channelId, int sampleRate, int bufferSize);
    ~NbFamily();

    NbFamily(const NbFamily&)            = delete;
    NbFamily& operator=(const NbFamily&)  = delete;

    // Mode/SNB setters — safe to call from any thread. processIq() reads
    // m_mode via load(std::memory_order_acquire).
    void setMode(NbMode mode);
    NbMode mode() const { return m_mode.load(std::memory_order_acquire); }

    void setSnbEnabled(bool enabled);
    bool snbEnabled() const { return m_snbEnabled.load(std::memory_order_acquire); }

    // Push persisted SNB Setup defaults (K1 / K2 / OutputBW) to WDSP.
    // MUST be called AFTER OpenChannel has initialised rxa[channelId].snba
    // — invoked from WdspEngine::createRxChannel once the channel is up.
    // Safe to omit in test paths that fabricate an unopened channel id.
    void seedSnbFromSettings();

    // Tuning setter — pushes every field through to WDSP. Also stores
    // the struct locally so subsequent partial setters (e.g. setNbThreshold
    // from a slider) can mutate in place.
    void setTuning(const NbTuning& tuning);
    const NbTuning& tuning() const { return m_tuning; }

    // Per-knob convenience setters (used by RxApplet tuning sliders).
    // Each pushes only its one WDSP call; the local m_tuning is updated too.
    void setNbThreshold(double threshold);
    void setNbTauMs(double ms);
    void setNbLeadMs(double advMs);    // "lead" ≡ advtime
    void setNbLagMs(double hangMs);    // "lag"  ≡ hangtime

private:
    const int m_channelId;
    const int m_sampleRate;
    const int m_bufferSize;

    std::atomic<NbMode> m_mode{NbMode::Off};
    std::atomic<bool>   m_snbEnabled{false};
    NbTuning            m_tuning{};

    // Pushes all tuning fields through WDSP post-create setters for both
    // NB1 and NB2. Called only from setTuning() — the ctor passes initial
    // values directly to create_anbEXT / create_nobEXT, so a post-create
    // push during construction would be redundant.
    void pushAllTuning();
};

} // namespace NereusSDR
