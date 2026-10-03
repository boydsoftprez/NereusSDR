// =================================================================
// src/core/RxChannel.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/radio.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/dsp.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/rxa.cs (upstream has no top-of-file header — project-level LICENSE applies)
//   Project Files/Source/Console/HPSDR/specHPSDR.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/cmaster.c, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-23 - dspLoad() reader for the WDSP worker's per-block load
//                 counters (R-R3-40) by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//                 NereusSDR-original; no Thetis counterpart. Later the same
//                 day: the block in progress and the per-interval longest
//                 block (takeDspIntervalMaxBlockUs).
//                 Later the same day: setActiveNr(NNR) no longer re-applies
//                 a stale cached NNR tuning; requestNnrLimit and nnrLimit
//                 (runtime NNR limit, carried across a rebuild).
//                 Later the same day: the read time (readNs), so a load
//                 reads busy time over wall time (R-R3-40, R-R3-37).
//                 Later the same day: DspLoadCounters::consistent, false for
//                 a read whose busy pair may be torn (R-R3-40).
//   2026-09-24 - A stopping channel is fed until WDSP finishes its stop
//                 (Task 8 of the receiver and transmit gaps plan, Phase 3F
//                 section 3), after Thetis ChannelMaster cmaster.c:365-366
//                 [v2.10.3.15], by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-25 - R-R3-39, Sub-epic C-1: the DeepFilterNet3 instance is built
//                 at a channel's first DFNR selection, on the receive lane,
//                 not in the constructor; availability comes from HAVE_DFNR
//                 and ModelPaths without a load. NereusSDR-original, by
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-25 - R-R3-49, Sub-epic C-1: dfnrUnavailable reports a first
//                 DFNR selection whose model is missing or failed to load.
//                 NereusSDR-original, by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26 - Remote-window parity Task 16 (R-R3-49): the filter
//                 response split into filterResponseBins and
//                 resampleFilterResponse, so a Core can send its bins.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code. NereusSDR-original.
//   2026-09-27 — R-IOS-13: the filter type sends Thetis's MP (Low Latency
//                 = minimum phase, enums.cs:404-408, radio.cs:571
//                 [v2.10.3.15]; it was inverted); the type cache starts at
//                 Linear Phase, where WDSP opens the channel. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Slice control plan Task 6 (JJ's ruling): the WDSP panel
//                 gain is held at unity and AudioEngine's mixer applies the
//                 AF level, a departure from Thetis radio.cs, which sets AF
//                 as SetRXAPanelGain1. By J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

//=================================================================
// radio.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Copyright (C) 2019-2026  Richard Samphire
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

/*  wdsp.cs

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013-2017 Warren Pratt, NR0V

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
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

//
// Upstream source 'Project Files/Source/Console/rxa.cs' has no top-of-file GPL header —
// project-level Thetis LICENSE applies.

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

//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

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

#include "RxChannel.h"
#include "AppSettings.h"
#include "DspControlThread.h"
#include "LogCategories.h"
#include "NbFamily.h"
#include "SampleRateCatalog.h"  // bufferSizeForRate() — for setSampleRate()
#include "WdspEngine.h"
#include "wdsp_api.h"
#include "platform/ThreadPlacement.h"
#include "dsp/NnrAdapter.h"

#include <QElapsedTimer>

#ifdef HAVE_DFNR
#include "DeepFilterFilter.h"
#include "ModelPaths.h"
#endif

#ifdef HAVE_MNR
#include "MacNRFilter.h"
#endif

// filterResponseMagnitudes() uses fir_bandpass() from WDSP and fftw_* from FFTW3.
// Both are guarded below with #if defined(HAVE_WDSP) && defined(HAVE_FFTW3).
#if defined(HAVE_WDSP) && defined(HAVE_FFTW3)
extern "C" {
#include "fir.h"         // fir_bandpass() — third_party/wdsp/src/fir.h (C linkage)
}
#include <fftw3.h>       // fftw_plan_dft_1d, fftw_execute, fftw_alloc_complex, etc.
#endif

#include <cmath>
#include <cstring>
#include <string_view>

namespace NereusSDR {

namespace {
// R-R3-39: the receive-lane key of one WDSP parameter, from the name of the
// setter that writes it (FNV-1a). runKeyed mixes in the channel.
constexpr quint64 laneParameter(std::string_view name)
{
    quint64 hash = 1469598103934665603ull;
    for (const char c : name) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ull;
    }
    return hash;
}
} // namespace

#ifdef HAVE_WDSP
namespace {
// Task 8: a quiet-NaN bit pattern processIq leaves in the first output
// sample of a stopping channel's exchange. fexchange2 overwrites it whenever
// the channel still exchanges; WDSP never produces this exact pattern. It is
// compared as bits, so no floating-point mode can change the test.
constexpr quint32 kStopSentinelBits = 0x7fc0beefu;
} // namespace
#endif

RxChannel::RxChannel(int channelId, int bufferSize, int sampleRate,
                     QObject* parent)
    : RxChannel(channelId, bufferSize, sampleRate, nullptr, parent)
{
}

RxChannel::RxChannel(int channelId, int bufferSize, int sampleRate,
                     DspControlThread* lane, QObject* parent)
    : QObject(parent)
    , m_channelId(channelId)
    , m_bufferSize(bufferSize)
    , m_sampleRate(sampleRate)
{
    m_lane = lane;
    for (auto& slot : m_meterCache) {
        slot.store(-140.0, std::memory_order_relaxed);
    }
#ifdef HAVE_WDSP
    // From design doc §sub-epic B — one NbFamily per WDSP channel.
    // R-R3-39: with a lane its WDSP objects are made on the lane, after
    // OpenChannel (createWdspObjectsOnLane), and it posts its calls there.
    m_nb = std::make_unique<NereusSDR::NbFamily>(
        m_channelId,
        /*sampleRate=*/ m_sampleRate.load(),
        /*bufferSize=*/ m_bufferSize.load(),
        /*createWdspObjects=*/ m_lane == nullptr);
    if (m_lane != nullptr) {
        m_nb->setDispatcher([this](quint64 parameter, std::function<void()> job) {
            if (parameter == 0) {
                runOrdered(std::move(job));
            } else {
                runKeyed(parameter, 0, std::move(job));
            }
        });
    }
#endif

    // Sub-epic C-1 Task 9 — DeepFilterNet3 post-WDSP noise reduction.
    // R-R3-39: no instance here. The model load is about 250 ms and a
    // connect opens five channels; the instance is built at this channel's
    // first DFNR selection, on the receive lane (ensureDfnrOnLane).

#ifdef HAVE_MNR
    // Sub-epic C-1 Task 11 — Apple Accelerate MMSE-Wiener post-WDSP NR.
    // Accelerate is a system framework — always available on macOS.
    // isValid() returns false only if vDSP_create_fftsetup failed (never
    // in practice), so no warning-and-reset needed; log if it ever fires.
    m_mnr = std::make_unique<NereusSDR::MacNRFilter>();
    if (!m_mnr->isValid()) {
        qCWarning(lcDsp) << "MNR (Apple Accelerate) FFT setup failed on channel"
                         << m_channelId;
        m_mnr.reset();
    }
#endif
}

RxChannel::~RxChannel() = default;

// ---------------------------------------------------------------------------
// R-R3-39: the receive lane
//
// NereusSDR-original. Every WDSP call below goes through runKeyed or
// runOrdered: at once with no lane, or on the lane itself; otherwise queued
// on the lane, where it runs after every call already queued (a keyed call
// replaces an older queued call with the same key, taking the newest
// position). Each queued job holds m_alive, so a job still queued when
// WdspEngine retires the wrapper does nothing.
// ---------------------------------------------------------------------------

void RxChannel::setControlLane(DspControlThread* lane)
{
    m_lane = lane;
#ifdef HAVE_WDSP
    if (m_nb) {
        if (lane == nullptr) {
            m_nb->setDispatcher({});
        } else {
            m_nb->setDispatcher([this](quint64 parameter, std::function<void()> job) {
                if (parameter == 0) {
                    runOrdered(std::move(job));
                } else {
                    runKeyed(parameter, 0, std::move(job));
                }
            });
        }
    }
#endif
}

void RxChannel::runKeyed(quint64 parameter, int sub, std::function<void()> job) const
{
    if (m_lane == nullptr || m_lane->isCurrentThread()) {
        job();
        return;
    }
    const quint64 key = parameter
        ^ (static_cast<quint64>(static_cast<quint32>(m_channelId) + 1u) * 0x9E3779B97F4A7C15ull)
        ^ (static_cast<quint64>(static_cast<quint32>(sub)) << 17);
    m_lane->postKeyed(key, [alive = m_alive, job = std::move(job)]() {
        if (alive->load(std::memory_order_acquire)) {
            job();
        }
    });
}

void RxChannel::runOrdered(std::function<void()> job) const
{
    if (m_lane == nullptr || m_lane->isCurrentThread()) {
        job();
        return;
    }
    m_lane->post([alive = m_alive, job = std::move(job)]() {
        if (alive->load(std::memory_order_acquire)) {
            job();
        }
    });
}

void RxChannel::markRetired()
{
    m_alive->store(false, std::memory_order_release);
    m_wdspReady.store(false, std::memory_order_release);
}

void RxChannel::createWdspObjectsOnLane()
{
#ifdef HAVE_WDSP
    if (m_nb) {
        m_nb->createWdspObjects();
    }
#endif
    refreshMinNotchWidthOnLane();
    refreshNnrDiagnosticsOnLane();
}

void RxChannel::destroyWdspObjectsOnLane()
{
#ifdef HAVE_WDSP
    if (m_nb) {
        m_nb->destroyWdspObjects();
    }
#endif
}

// ---------------------------------------------------------------------------
// Live sample-rate change (Thetis-faithful, carry-only)
//
// Replaces the destroy-and-recreate path that crashed on PR #221 (a4d076f)
// when setSampleRateLive moved the dangling m_txChannel to its worker
// thread.  Mirrors the Thetis split between audio.cs::SampleRate1 setter
// (state mutation) and ChannelMaster/cmaster.c::SetXcmInrate (the WDSP-side
// rate work) [v2.10.3.13]:
//
//   This method:        carry-only state update on the C++ wrapper.
//                       Idempotent on equality.
//   WdspEngine path:    SetInputSamplerate + SetInputBuffsize on the same
//                       channel ID — channel object stays alive, no holders
//                       of the RxChannel raw pointer are invalidated.
//
// Splitting the responsibilities lets unit tests exercise the state path
// without dragging an opened WDSP channel along.  The production caller
// (RadioModel::setSampleRateLive) goes through WdspEngine which performs
// both the WDSP call and the state update.
// ---------------------------------------------------------------------------

void RxChannel::setSampleRate(int newRateHz)
{
    if (newRateHz == m_sampleRate) {
        // Mirrors SetXcmInrate guard: cmaster.c:457 [v2.10.3.13]
        //   if (pcm->xcm_inrate[in_id] != rate) { ... }
        return;
    }

    setSampleRateCarry(newRateHz);
    const int rate = m_sampleRate;
    const int size = m_bufferSize;
    runOrdered([this, rate, size]() { applySampleRateOnLane(rate, size); });
}

void RxChannel::setSampleRateCarry(int newRateHz)
{
    m_sampleRate.store(newRateHz, std::memory_order_release);
    m_bufferSize.store(bufferSizeForRate(newRateHz), std::memory_order_release);
    if (m_nb) {
        m_nb->setSampleRateCarry(m_sampleRate.load(), m_bufferSize.load());
    }
}

void RxChannel::applySampleRateOnLane(int rateHz, int bufferSize)
{
    // Task 8: the caller rebuilds the WDSP channel next (WdspEngine::
    // setRxChannelRate, SetInputSamplerate), which clears exchange and the
    // flush and slew flags (pre_main_destroy channel.c:119, pre_main_build
    // channel.c:69, the slews rebuilt at iobuffs.c:78-79) and leaves a
    // stopped channel's exchange clear (post_main_build channel.c:73-78). A
    // no-drain stop still pending is finished by that.
    m_pendingStop.store(0, std::memory_order_release);

    // Propagate to NB1/NB2 so initBlanker()/init_nob() recompute time
    // constants for the new rate. Mirrors cmaster.c:464-470 [v2.10.3.13]
    // SetXcmInrate case 0 (receiver):
    //   SetRCVRANBBuffsize/Samplerate + SetRCVRNOBBuffsize/Samplerate.
    // Without this, NB stays configured for the original rate and the
    // blanker's slewtime/hangtime/advtime envelope is wrong after a
    // setSampleRateLive — manifests as metallic ringing at higher rates.
    //
    // R-R3-39: the carry (setSampleRateCarry) is already in place; this is
    // the WDSP half, run on the lane (or at once without one).
    if (m_nb) {
        m_nb->applySampleRateWdsp(rateHz, bufferSize);
    }

    // min_notch_width scales with the channel rate as well as with nc
    // (third_party/wdsp/src/nbp.c:82-96). Thetis re-reads the value on the
    // same sample-rate path (console.cs:39052-39053 ->
    // UpdateMinimumNotchWidthRX [v2.10.3.15]), so the readout follows a rate
    // change here rather than going stale until the next filter-size change.
    refreshMinNotchWidthOnLane();
}

// ---------------------------------------------------------------------------
// Demodulation
// ---------------------------------------------------------------------------

void RxChannel::setMode(DSPMode mode)
{
    int val = static_cast<int>(mode);
    if (val == m_mode.load()) {
        return;
    }

    m_mode.store(val);

#ifdef HAVE_WDSP
    runKeyed(laneParameter("setMode"), 0, [=, this]() {
        // Phase 3R K-bench: RADE_U / RADE_L are NereusSDR-native modes
        // (WdspTypes.h:159-186) that WDSP has no knowledge of. The RX
        // pipeline keeps WDSP alive as the demod front-end in RADE
        // modes (RxDspWorker.cpp:160-191 — "WDSP always runs ... RADE
        // post-SSB-demod fork"), so map RADE_U -> USB and RADE_L -> LSB
        // here before passing to SetRXAMode. The slice-facing mode()
        // accessor and the modeChanged signal both still report the
        // user-requested DSPMode; only the WDSP API call is mapped.
        // Without this mapping, raw enum 12/13 lands in WDSP's mode
        // enum and triggers undefined behavior (review finding
        // 2026-05-12, PR #238).
        // From Thetis wdsp-integration.md section 4.2
        SetRXAMode(m_channelId, static_cast<int>(wdspModeFor(mode)));
    });
#endif

    emit modeChanged(mode);
}

DSPMode RxChannel::wdspModeFor(DSPMode mode)
{
    // NereusSDR-native: WdspTypes.h:181-186 reserves RADE_U / RADE_L
    // as non-WDSP slice modes. The RX K-bench pipeline runs WDSP as
    // the SSB demod front-end in both, so the WDSP-facing equivalent
    // is USB for RADE_U and LSB for RADE_L.
    if (mode == DSPMode::RADE_U) return DSPMode::USB;
    if (mode == DSPMode::RADE_L) return DSPMode::LSB;
    return mode;
}

// ---------------------------------------------------------------------------
// Bandpass filter
// ---------------------------------------------------------------------------

void RxChannel::setFilterFreqs(double lowHz, double highHz)
{
    if (m_filterLow == lowHz && m_filterHigh == highHz) {
        return;
    }

    m_filterLow  = lowHz;
    m_filterHigh = highHz;
    // Keep int carry fields in sync so captureState() sees consistent values
    // regardless of whether the caller used setFilterFreqs() directly or
    // went through setFilterLow/setFilterHigh first.
    m_filterLowInt  = static_cast<int>(std::round(lowHz));
    m_filterHighInt = static_cast<int>(std::round(highHz));

#ifdef HAVE_WDSP
    runKeyed(laneParameter("setFilterFreqs"), 0, [=, this]() {
        // From Thetis rxa.cs:110-111, radio.cs:603-604 — both bp1 and nbp0
        // filters must be updated together. SetRXABandpassFreqs only touches
        // bp1, which runs only when AMD/SNBA/EMNR/ANF/ANR is enabled.
        // RXANBPSetFreqs touches nbp0, the filter that runs unconditionally
        // in the SSB/CW/AM audio path. Calling only one leaves the SSB
        // bandpass stuck at nbp0's create-time default of -4150..-150
        // (LSB-shaped), which silently breaks USB, AM, and FM demod.
        SetRXABandpassFreqs(m_channelId, lowHz, highHz);
        RXANBPSetFreqs(m_channelId, lowHz, highHz);
    });
#endif

    emit filterChanged(lowHz, highHz);
}

// ---------------------------------------------------------------------------
// AGC
// ---------------------------------------------------------------------------

void RxChannel::setAgcMode(AGCMode mode)
{
    int val = static_cast<int>(mode);
    if (val == m_agcMode.load()) {
        return;
    }

    m_agcMode.store(val);

#ifdef HAVE_WDSP
    runKeyed(laneParameter("setAgcMode"), 0, [=, this]() {
        SetRXAAGCMode(m_channelId, val);
    });
#endif

    emit agcModeChanged(mode);
}

void RxChannel::setAgcTop(double topdB)
{
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setAgcTop"), 0, [=, this]() {
        SetRXAAGCTop(m_channelId, topdB);
    });
#else
    Q_UNUSED(topdB);
#endif
}

double RxChannel::readBackAgcTop() const
{
#ifdef HAVE_WDSP
    // Read resulting max_gain after SetRXAAGCThresh modified it.
    // From Thetis console.cs:45978 — GetRXAAGCTop after SetRXAAGCThresh
    // Clamp matches Thetis console.cs:45988-45989 guard on RFGain slider range.
    double top = 0.0;
    GetRXAAGCTop(m_channelId, &top);
    return std::clamp(top, -20.0, 120.0);
#else
    return 80.0;
#endif
}

double RxChannel::readBackAgcThresh() const
{
    return readBackAgcThreshAt(m_sampleRate);
}

double RxChannel::readBackAgcThreshAt(int sampleRate) const
{
#ifdef HAVE_WDSP
    // Read resulting threshold after SetRXAAGCTop modified it.
    // From Thetis console.cs:50350 pattern — GetRXAAGCThresh after SetRXAAGCTop
    // Range clamp as Thetis applies it:
    // From Thetis console.cs:50423-50424 [v2.10.3.15]
    //   if (agc_thresh_point > 2) agc_thresh_point = 2;
    //   if (agc_thresh_point < -160.0) agc_thresh_point = -160.0; //[2.10.3.6]MW0LGE changed from -143
    //   (MW0LGE_21k7 on the FFT-size line that follows)
    // kDspSize must match the size passed to SetRXAAGCThresh (4096).
    static constexpr double kDspSize = 4096.0;
    double thresh = 0.0;
    GetRXAAGCThresh(m_channelId, &thresh, kDspSize, static_cast<double>(sampleRate));
    return std::clamp(thresh, -160.0, 2.0);
#else
    Q_UNUSED(sampleRate);
    return -20.0;
#endif
}

// R-R3-39: both AGC readbacks as one lane request. Posted after the setter
// it follows, so it reads what that setter left (Thetis reads straight after
// the set, console.cs:45978 and :50350).
void RxChannel::requestAgcReadBack(QObject* context,
                                   std::function<void(double top, double thresh)> done)
{
    const int sampleRate = m_sampleRate;
    if (m_lane == nullptr || m_lane->isCurrentThread()) {
        const double top = readBackAgcTop();
        const double thresh = readBackAgcThreshAt(sampleRate);
        if (done) {
            done(top, thresh);
        }
        return;
    }
    using Reading = std::optional<std::pair<double, double>>;
    m_lane->request<Reading>(
        [this, alive = m_alive, sampleRate]() -> Reading {
            if (!alive->load(std::memory_order_acquire)) {
                return std::nullopt;
            }
            return std::make_pair(readBackAgcTop(), readBackAgcThreshAt(sampleRate));
        },
        context,
        [done = std::move(done)](Reading reading) {
            if (reading && done) {
                done(reading->first, reading->second);
            }
        });
}

void RxChannel::setAgcThreshold(int dBu)
{
    if (dBu == m_agcThreshold.load()) {
        return;
    }

    m_agcThreshold.store(dBu);

#ifdef HAVE_WDSP
    // The rate is read here, on the owner's thread, where it changes.
    const int sampleRate = m_sampleRate;
    runKeyed(laneParameter("setAgcThreshold"), 0, [=, this]() {
        // From Thetis Project Files/Source/Console/console.cs:45976-45977
        //   size = (double)specRX.GetSpecRX(0).FFTSize;  // 4096
        //   WDSP.SetRXAAGCThresh(WDSP.id(0, 0), agc_thresh_point, size, sample_rate_rx1);
        // WDSP third_party/wdsp/src/wcpAGC.c:504
        // NB: 'size' is the DSP analysis buffer size (4096, matching OpenChannel dsp_size),
        //     NOT the fexchange2 input chunk size (m_bufferSize).
        static constexpr double kDspSize = 4096.0;
        SetRXAAGCThresh(m_channelId, static_cast<double>(dBu),
                        kDspSize,
                        static_cast<double>(sampleRate));
    });
#else
    Q_UNUSED(dBu);
#endif
}

void RxChannel::setAgcHang(int ms)
{
    if (ms == m_agcHang.load()) {
        return;
    }

    m_agcHang.store(ms);

#ifdef HAVE_WDSP
    runKeyed(laneParameter("setAgcHang"), 0, [=, this]() {
        // From Thetis Project Files/Source/Console/radio.cs:1056-1073
        //   WDSP.SetRXAAGCHang(WDSP.id(thread, subrx), value)
        // WDSP third_party/wdsp/src/wcpAGC.c:436
        SetRXAAGCHang(m_channelId, ms);
    });
#else
    Q_UNUSED(ms);
#endif
}

void RxChannel::setAgcSlope(int slope)
{
    if (slope == m_agcSlope.load()) {
        return;
    }

    m_agcSlope.store(slope);

#ifdef HAVE_WDSP
    runKeyed(laneParameter("setAgcSlope"), 0, [=, this]() {
        // From Thetis Project Files/Source/Console/radio.cs:1107-1124
        //   WDSP.SetRXAAGCSlope(WDSP.id(thread, subrx), value)
        // WDSP third_party/wdsp/src/wcpAGC.c:537
        SetRXAAGCSlope(m_channelId, slope);
    });
#else
    Q_UNUSED(slope);
#endif
}

void RxChannel::setAgcAttack(int ms)
{
    if (ms == m_agcAttack.load()) {
        return;
    }

    m_agcAttack.store(ms);

#ifdef HAVE_WDSP
    runKeyed(laneParameter("setAgcAttack"), 0, [=, this]() {
        // From Thetis Project Files/Source/Console/dsp.cs:116-117
        //   SetRXAAGCAttack declared; no explicit radio.cs call site (disabled in UI)
        // WDSP third_party/wdsp/src/wcpAGC.c:418
        SetRXAAGCAttack(m_channelId, ms);
    });
#else
    Q_UNUSED(ms);
#endif
}

void RxChannel::setAgcDecay(int ms)
{
    if (ms == m_agcDecay.load()) {
        return;
    }

    m_agcDecay.store(ms);

#ifdef HAVE_WDSP
    runKeyed(laneParameter("setAgcDecay"), 0, [=, this]() {
        // From Thetis Project Files/Source/Console/radio.cs:1037-1054
        //   WDSP.SetRXAAGCDecay(WDSP.id(thread, subrx), value)
        // WDSP third_party/wdsp/src/wcpAGC.c:427
        SetRXAAGCDecay(m_channelId, ms);
    });
#else
    Q_UNUSED(ms);
#endif
}

void RxChannel::setAgcHangThreshold(int val)
{
    if (val == m_agcHangThreshold.load()) {
        return;
    }

    m_agcHangThreshold.store(val);

#ifdef HAVE_WDSP
    runKeyed(laneParameter("setAgcHangThreshold"), 0, [=, this]() {
        // From Thetis v2.10.3.13 setup.cs:9081
        //   WDSP.SetRXAAGCHangThreshold(WDSP.id(0, 0), value)
        // WDSP third_party/wdsp/src/wcpAGC.c
        SetRXAAGCHangThreshold(m_channelId, val);
    });
#else
    Q_UNUSED(val);
#endif
}

void RxChannel::setAgcFixedGain(int dB)
{
    if (dB == m_agcFixedGain.load()) {
        return;
    }

    m_agcFixedGain.store(dB);

#ifdef HAVE_WDSP
    runKeyed(laneParameter("setAgcFixedGain"), 0, [=, this]() {
        // From Thetis v2.10.3.13 setup.cs:9001
        //   WDSP.SetRXAAGCFixed(WDSP.id(0, 0), value)
        // WDSP third_party/wdsp/src/wcpAGC.c
        SetRXAAGCFixed(m_channelId, static_cast<double>(dB));
    });
#else
    Q_UNUSED(dB);
#endif
}

void RxChannel::setAgcMaxGain(int dB)
{
    if (dB == m_agcMaxGain.load()) {
        return;
    }

    m_agcMaxGain.store(dB);

#ifdef HAVE_WDSP
    runKeyed(laneParameter("setAgcMaxGain"), 0, [=, this]() {
        // From Thetis v2.10.3.13 setup.cs:9011
        //   WDSP.SetRXAAGCTop(WDSP.id(0, 0), (double)value)
        // WDSP third_party/wdsp/src/wcpAGC.c
        SetRXAAGCTop(m_channelId, static_cast<double>(dB));
    });
#else
    Q_UNUSED(dB);
#endif
}

// ---------------------------------------------------------------------------
// Noise blanker family (NB / NB2 / SNB) — see NbFamily.h
// ---------------------------------------------------------------------------

void RxChannel::setNbMode(NereusSDR::NbMode mode)
{
    if (m_nb) m_nb->setMode(mode);
}

NereusSDR::NbMode RxChannel::nbMode() const
{
    return m_nb ? m_nb->mode() : NereusSDR::NbMode::Off;
}

// Phase 3F Sub-Epic I Task 4b. Written by RxDspWorker's drain loop before
// each slice's processIq; read inside processIq. Release/acquire matches the
// pairing setActiveNr() uses for the other hot-path flags.
void RxChannel::setNoiseBlankerBypassed(bool bypassed)
{
    m_nbBypassed.store(bypassed, std::memory_order_release);
}

// Per-slice NB tuning pass-through (setNbTuning / nbTuning / setNbThreshold
// / setNbLagMs / setNbLeadMs / setNbTransitionMs) removed 2026-04-22. NB
// tuning is global per-channel now; Setup → DSP → NB/SNB calls WDSP
// SetEXTANB* directly on channel 0. See DspSetupPages.cpp.

// ---------------------------------------------------------------------------
// Noise reduction
// ---------------------------------------------------------------------------

void RxChannel::setNrEnabled(bool enabled)
{
    if (enabled == m_nrEnabled.load()) {
        return;
    }

    m_nrEnabled.store(enabled);

#ifdef HAVE_WDSP
    runKeyed(laneParameter("setNrEnabled"), 0, [=, this]() {
        SetRXAANRRun(m_channelId, enabled ? 1 : 0);
    });
#endif
}

void RxChannel::setAnfEnabled(bool enabled)
{
    if (enabled == m_anfEnabled.load()) {
        return;
    }

    m_anfEnabled.store(enabled);

#ifdef HAVE_WDSP
    runKeyed(laneParameter("setAnfEnabled"), 0, [=, this]() {
        SetRXAANFRun(m_channelId, enabled ? 1 : 0);
    });
#endif
}

// ---------------------------------------------------------------------------
// EMNR (NR2)
// ---------------------------------------------------------------------------

void RxChannel::setEmnrEnabled(bool enabled)
{
    if (enabled == m_emnrEnabled.load()) {
        return;
    }

    m_emnrEnabled.store(enabled);

#ifdef HAVE_WDSP
    runKeyed(laneParameter("setEmnrEnabled"), 0, [=, this]() {
        // From Thetis Project Files/Source/Console/radio.cs:2216-2232
        //   WDSP.SetRXAEMNRRun(WDSP.id(thread, subrx), value)
        // WDSP third_party/wdsp/src/emnr.c:1283
        SetRXAEMNRRun(m_channelId, enabled ? 1 : 0);
    });
#else
    Q_UNUSED(enabled);
#endif
}

void RxChannel::setEmnrGainMethod(int method)
{
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setEmnrGainMethod"), 0, [=, this]() {
        // From Thetis Project Files/Source/Console/radio.cs:2062-2078
        //   WDSP.SetRXAEMNRgainMethod(WDSP.id(thread, subrx), value)
        // WDSP third_party/wdsp/src/emnr.c:1298
        SetRXAEMNRgainMethod(m_channelId, method);
    });
#else
    Q_UNUSED(method);
#endif
}

void RxChannel::setEmnrNpeMethod(int method)
{
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setEmnrNpeMethod"), 0, [=, this]() {
        // From Thetis Project Files/Source/Console/radio.cs:2081-2097
        //   WDSP.SetRXAEMNRnpeMethod(WDSP.id(thread, subrx), value)
        // WDSP third_party/wdsp/src/emnr.c:1306
        SetRXAEMNRnpeMethod(m_channelId, method);
    });
#else
    Q_UNUSED(method);
#endif
}

void RxChannel::setEmnrAeRun(bool run)
{
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setEmnrAeRun"), 0, [=, this]() {
        // From Thetis Project Files/Source/Console/radio.cs:2101-2117
        //   WDSP.SetRXAEMNRaeRun(WDSP.id(thread, subrx), value)
        // WDSP third_party/wdsp/src/emnr.c:1314
        SetRXAEMNRaeRun(m_channelId, run ? 1 : 0);
    });
#else
    Q_UNUSED(run);
#endif
}

void RxChannel::setEmnrPosition(int position)
{
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setEmnrPosition"), 0, [=, this]() {
        // From Thetis Project Files/Source/Console/radio.cs:2235-2251
        //   WDSP.SetRXAEMNRPosition(WDSP.id(thread, subrx), value)
        // WDSP third_party/wdsp/src/emnr.c:1322
        // position=1 → post-AGC placement (Thetis default rx_nr2_position=1)
        SetRXAEMNRPosition(m_channelId, position);
    });
#else
    Q_UNUSED(position);
#endif
}

// ---------------------------------------------------------------------------
// NR1 — ANR tuning (Sub-epic C-1)
// Porting from Thetis setup.cs:8539-8566, radio.cs:673-699 [v2.10.3.13]
// Original C# logic:
//   private void udLMSNR_ValueChanged(...)
//   {
//       console.radio.GetDSPRX(0, 0).SetNRVals(
//           (int)udLMSNRtaps.Value,
//           (int)udLMSNRdelay.Value,
//           1e-6 * (double)udLMSNRgain.Value,    // ← UI int scaled ×1e-6
//           1e-3 * (double)udLMSNRLeak.Value);   // ← UI int scaled ×1e-3
//   }
// Q-c verify: UI spinboxes use 1e-6/1e-3 factors respectively.  The Nr1Tuning
// struct and these per-knob setters store and accept raw WDSP-domain doubles.
// The UI layer must apply the ×1e-6 / ×1e-3 conversions before calling here.
// ---------------------------------------------------------------------------

void RxChannel::setAnrTuning(const Nr1Tuning& t)
{
    m_nr1Tuning = t;
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setAnrTuning"), 0, [=, this]() {
        // From Thetis radio.cs:681-698 [v2.10.3.13] — SetNRVals() calls
        // WDSP.SetRXAANRVals(id, taps, delay, gain, leak) with already-scaled values.
        SetRXAANRVals(m_channelId, t.taps, t.delay, t.gain, t.leakage);
        SetRXAANRPosition(m_channelId, static_cast<int>(t.position));
    });
#endif
}

void RxChannel::setAnrTaps(int taps)
{
    m_nr1Tuning.taps = taps;
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setAnrTaps"), 0, [=, this]() {
        // From Thetis radio.cs:681-698 [v2.10.3.13]
        SetRXAANRTaps(m_channelId, taps);
    });
#endif
}

void RxChannel::setAnrDelay(int delay)
{
    m_nr1Tuning.delay = delay;
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setAnrDelay"), 0, [=, this]() {
        // From Thetis radio.cs:681-698 [v2.10.3.13]
        SetRXAANRDelay(m_channelId, delay);
    });
#endif
}

void RxChannel::setAnrGain(double gain)
{
    m_nr1Tuning.gain = gain;
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setAnrGain"), 0, [=, this]() {
        // From Thetis setup.cs:8545 [v2.10.3.13] — caller has already applied ×1e-6.
        // Passes raw WDSP-domain value directly to SetRXAANRGain.
        SetRXAANRGain(m_channelId, gain);
    });
#endif
}

void RxChannel::setAnrLeakage(double leakage)
{
    m_nr1Tuning.leakage = leakage;
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setAnrLeakage"), 0, [=, this]() {
        // From Thetis setup.cs:8550 [v2.10.3.13] — caller has already applied ×1e-3.
        // Passes raw WDSP-domain value directly to SetRXAANRLeakage.
        SetRXAANRLeakage(m_channelId, leakage);
    });
#endif
}

void RxChannel::setAnrPosition(NrPosition p)
{
    m_nr1Tuning.position = p;
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setAnrPosition"), 0, [=, this]() {
        // From Thetis setup.cs:8723 [v2.10.3.13]
        SetRXAANRPosition(m_channelId, static_cast<int>(p));
    });
#endif
}

// ---------------------------------------------------------------------------
// NR2 — EMNR tuning (Sub-epic C-1)
// Porting from Thetis setup.cs:34711-34748, radio.cs:2062-2213 [v2.10.3.13]
// All post2 values are raw passthrough to WDSP (verified: radio.cs properties
// set and forward the value unchanged — no ÷100 at the boundary).
// ---------------------------------------------------------------------------

void RxChannel::setEmnrTuning(const Nr2Tuning& t)
{
    m_nr2Tuning = t;
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setEmnrTuning"), 0, [=, this]() {
        // From Thetis radio.cs:2062-2213 [v2.10.3.13]
        SetRXAEMNRgainMethod(m_channelId, static_cast<int>(t.gainMethod));
        SetRXAEMNRnpeMethod (m_channelId, static_cast<int>(t.npeMethod));
        SetRXAEMNRaeRun     (m_channelId, t.aeFilter ? 1 : 0);
        SetRXAEMNRPosition  (m_channelId, static_cast<int>(t.position));
        SetRXAEMNRpost2Run  (m_channelId, t.post2Run ? 1 : 0);
        SetRXAEMNRpost2Nlevel(m_channelId, t.post2Level);
        SetRXAEMNRpost2Factor(m_channelId, t.post2Factor);
        SetRXAEMNRpost2Rate  (m_channelId, t.post2Rate);
        SetRXAEMNRpost2Taper (m_channelId, t.post2Taper);
    });
#endif
}

void RxChannel::setEmnrTrainT1(double t1)
{
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setEmnrTrainT1"), 0, [=, this]() {
        // From Thetis dsp.cs:315 [v2.10.3.13] — SetRXAEMNRtrainZetaThresh
        // "T1" in the UI maps to zetathresh in emnr.c:1352
        SetRXAEMNRtrainZetaThresh(m_channelId, t1);
    });
#else
    Q_UNUSED(t1);
#endif
}

void RxChannel::setEmnrTrainT2(double t2)
{
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setEmnrTrainT2"), 0, [=, this]() {
        // From Thetis dsp.cs:318 [v2.10.3.13] — SetRXAEMNRtrainT2
        SetRXAEMNRtrainT2(m_channelId, t2);
    });
#else
    Q_UNUSED(t2);
#endif
}

void RxChannel::setEmnrAeZetaThresh(double v)
{
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setEmnrAeZetaThresh"), 0, [=, this]() {
        // From Thetis dsp.cs:287 [v2.10.3.13] — SetRXAEMNRaeZetaThresh
        SetRXAEMNRaeZetaThresh(m_channelId, v);
    });
#else
    Q_UNUSED(v);
#endif
}

void RxChannel::setEmnrAePsi(double v)
{
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setEmnrAePsi"), 0, [=, this]() {
        // From Thetis dsp.cs:289 [v2.10.3.13] — SetRXAEMNRaePsi
        SetRXAEMNRaePsi(m_channelId, v);
    });
#else
    Q_UNUSED(v);
#endif
}

void RxChannel::setEmnrPost2Run(bool on)
{
    m_nr2Tuning.post2Run = on;
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setEmnrPost2Run"), 0, [=, this]() {
        // From Thetis setup.cs:34719-34720, radio.cs:2122 [v2.10.3.13]
        SetRXAEMNRpost2Run(m_channelId, on ? 1 : 0);
    });
#endif
}

void RxChannel::setEmnrPost2Level(double level)
{
    m_nr2Tuning.post2Level = level;
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setEmnrPost2Level"), 0, [=, this]() {
        // From Thetis setup.cs:34711, radio.cs:2141-2155 [v2.10.3.13]
        // Q-c verified: radio.cs passes the raw double value; no ÷100 applied.
        SetRXAEMNRpost2Nlevel(m_channelId, level);
    });
#endif
}

void RxChannel::setEmnrPost2Factor(double factor)
{
    m_nr2Tuning.post2Factor = factor;
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setEmnrPost2Factor"), 0, [=, this]() {
        // From Thetis setup.cs:34712, radio.cs:2160-2174 [v2.10.3.13]
        // Q-c verified: radio.cs passes the raw double value; no ÷100 applied.
        SetRXAEMNRpost2Factor(m_channelId, factor);
    });
#endif
}

void RxChannel::setEmnrPost2Rate(double rate)
{
    m_nr2Tuning.post2Rate = rate;
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setEmnrPost2Rate"), 0, [=, this]() {
        // From Thetis setup.cs:34713, radio.cs:2179-2193 [v2.10.3.13]
        // Q-c verified: radio.cs passes the raw double value; no scaling.
        SetRXAEMNRpost2Rate(m_channelId, rate);
    });
#endif
}

void RxChannel::setEmnrPost2Taper(int taper)
{
    m_nr2Tuning.post2Taper = taper;
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setEmnrPost2Taper"), 0, [=, this]() {
        // From Thetis setup.cs:34714, radio.cs:2198-2212 [v2.10.3.13]
        // Q-c verified: radio.cs passes the raw int value; no scaling.
        SetRXAEMNRpost2Taper(m_channelId, taper);
    });
#endif
}

// ---------------------------------------------------------------------------
// NR3 — RNNR tuning (Sub-epic C-1)
// Porting from Thetis radio.cs:2257-2311, setup.cs:35460-35462 [v2.10.3.13]
// ---------------------------------------------------------------------------

void RxChannel::setRnnrTuning(const Nr3Tuning& t)
{
    m_nr3Tuning = t;
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setRnnrTuning"), 0, [=, this]() {
        // From Thetis radio.cs:2275-2295 [v2.10.3.13]
        SetRXARNNRPosition       (m_channelId, static_cast<int>(t.position));
        SetRXARNNRUseDefaultGain (m_channelId, t.useDefaultGain ? 1 : 0);
    });
#endif
}

void RxChannel::setRnnrPosition(NrPosition p)
{
    m_nr3Tuning.position = p;
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setRnnrPosition"), 0, [=, this]() {
        // From Thetis radio.cs:2275 [v2.10.3.13]
        SetRXARNNRPosition(m_channelId, static_cast<int>(p));
    });
#endif
}

void RxChannel::setRnnrUseDefaultGain(bool on)
{
    m_nr3Tuning.useDefaultGain = on;
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setRnnrUseDefaultGain"), 0, [=, this]() {
        // From Thetis setup.cs:35460-35462, radio.cs:2293-2311 [v2.10.3.13]
        // "Use fixed gain for input samples" checkbox maps to SetRXARNNRUseDefaultGain.
        SetRXARNNRUseDefaultGain(m_channelId, on ? 1 : 0);
    });
#endif
}

// ---------------------------------------------------------------------------
// NR4 — SBNR tuning (Sub-epic C-1)
// Porting from Thetis radio.cs:2312-2355, setup.cs:34511-34527 [v2.10.3.13]
// All values are float-cast at the WDSP boundary (WDSP sbnr.c uses float).
// ---------------------------------------------------------------------------

void RxChannel::setSbnrTuning(const Nr4Tuning& t)
{
    m_nr4Tuning = t;
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setSbnrTuning"), 0, [=, this]() {
        // From Thetis radio.cs:2312-2355 [v2.10.3.13]
        SetRXASBNRreductionAmount    (m_channelId, static_cast<float>(t.reductionAmount));
        SetRXASBNRsmoothingFactor    (m_channelId, static_cast<float>(t.smoothingFactor));
        SetRXASBNRwhiteningFactor    (m_channelId, static_cast<float>(t.whiteningFactor));
        SetRXASBNRnoiseRescale       (m_channelId, static_cast<float>(t.noiseRescale));
        SetRXASBNRpostFilterThreshold(m_channelId, static_cast<float>(t.postFilterThreshold));
        SetRXASBNRnoiseScalingType   (m_channelId, static_cast<int>(t.algo));
    });
#endif
}

void RxChannel::setSbnrReductionAmount(double dB)
{
    m_nr4Tuning.reductionAmount = dB;
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setSbnrReductionAmount"), 0, [=, this]() {
        // From Thetis radio.cs:2331 [v2.10.3.13]
        SetRXASBNRreductionAmount(m_channelId, static_cast<float>(dB));
    });
#endif
}

void RxChannel::setSbnrSmoothingFactor(double pct)
{
    m_nr4Tuning.smoothingFactor = pct;
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setSbnrSmoothingFactor"), 0, [=, this]() {
        // From Thetis radio.cs:2338 [v2.10.3.13]
        SetRXASBNRsmoothingFactor(m_channelId, static_cast<float>(pct));
    });
#endif
}

void RxChannel::setSbnrWhiteningFactor(double pct)
{
    m_nr4Tuning.whiteningFactor = pct;
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setSbnrWhiteningFactor"), 0, [=, this]() {
        // From Thetis radio.cs:2345 [v2.10.3.13]
        SetRXASBNRwhiteningFactor(m_channelId, static_cast<float>(pct));
    });
#endif
}

void RxChannel::setSbnrNoiseRescale(double dB)
{
    m_nr4Tuning.noiseRescale = dB;
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setSbnrNoiseRescale"), 0, [=, this]() {
        // From Thetis radio.cs:2349 [v2.10.3.13]
        SetRXASBNRnoiseRescale(m_channelId, static_cast<float>(dB));
    });
#endif
}

void RxChannel::setSbnrPostFilterThreshold(double dB)
{
    m_nr4Tuning.postFilterThreshold = dB;
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setSbnrPostFilterThreshold"), 0, [=, this]() {
        // From Thetis radio.cs:2353 [v2.10.3.13]
        SetRXASBNRpostFilterThreshold(m_channelId, static_cast<float>(dB));
    });
#endif
}

void RxChannel::setSbnrAlgo(SbnrAlgo a)
{
    m_nr4Tuning.algo = a;
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setSbnrAlgo"), 0, [=, this]() {
        // From Thetis setup.cs:34511-34527 [v2.10.3.13] — Algo 1/2/3 maps to
        // noiseScalingType 0/1/2 (SbnrAlgo enum values are 0/1/2 accordingly).
        SetRXASBNRnoiseScalingType(m_channelId, static_cast<int>(a));
    });
#endif
}

// ---------------------------------------------------------------------------
// setActiveNr — central mode dispatch (Sub-epic C-1)
// Porting from Thetis console.cs:43297-43450 SelectNR() [v2.10.3.13]
// Original C# logic (condensed — NR1 case shown):
//   case RadioButtonNR1:
//       rad.RXANR4Run = 0;
//       rad.RXANR3Run = 0;
//       rad.RXANR2Run = 0;
//       rad.RXANR1Run = 1;
// All four Run flags are written on every call so exactly 0 or 1 is active.
// ---------------------------------------------------------------------------

bool RxChannel::setNnrTuning(const NnrSettings& settings, QString* reason)
{
    if (m_lane == nullptr || m_lane->isCurrentThread()) {
        if (const auto accepted = NnrAdapter::apply(m_channelId, settings, reason)) {
            std::lock_guard<std::mutex> lock(m_nnrMutex);
            m_nnrTuning = *accepted;
            return true;
        }
        return false;
    }

    // R-R3-39: decide at once from what the lane last read, with the reasons
    // NnrAdapter::apply gives for the same refusals (ConfigureRXANNR refuses
    // an invalid configuration, a channel that is not a running receiver,
    // and a model slot whose network is not loaded; nnr.c).
    if (reason) {
        reason->clear();
    }
    if (!settings.isValid()) {
        if (reason) {
            *reason = QStringLiteral("NNR values must be finite and within their supported ranges.");
        }
        return false;
    }
    if (const auto known = knownNnrDiagnostics();
        known && (!known->available || settings.modelSlot < 0 || settings.modelSlot > 1
                  || !known->modelAvailable[static_cast<std::size_t>(settings.modelSlot)])) {
        if (reason) {
            *reason = QStringLiteral("The requested NNR model is not ready; no tuning was changed.");
        }
        return false;
    }
    NnrSettings previous;
    {
        std::lock_guard<std::mutex> lock(m_nnrMutex);
        previous = m_nnrTuning;
        m_nnrTuning = settings;
    }
    runOrdered([this, settings, previous]() {
        QString refusal;
        const auto accepted = NnrAdapter::apply(m_channelId, settings, &refusal);
        {
            std::lock_guard<std::mutex> lock(m_nnrMutex);
            if (accepted) {
                m_nnrTuning = *accepted;
            } else if (m_nnrTuning == settings) {
                m_nnrTuning = previous;
            }
        }
        refreshNnrDiagnosticsOnLane();
        if (!accepted) {
            emit nnrRequestRefused(refusal, -1);
        }
    });
    return true;
}

NnrSettings RxChannel::nnrTuning() const
{
    if (m_lane != nullptr && !m_lane->isCurrentThread()) {
        // R-R3-39: what WDSP last accepted, as the lane wrote it.
        std::lock_guard<std::mutex> lock(m_nnrMutex);
        return m_nnrTuning;
    }
    NnrSettings carried;
    {
        std::lock_guard<std::mutex> lock(m_nnrMutex);
        carried = m_nnrTuning;
    }
    return NnrAdapter::readSettings(m_channelId).value_or(carried);
}

NnrDiagnostics RxChannel::nnrDiagnostics() const
{
    if (m_lane != nullptr && !m_lane->isCurrentThread()) {
        // R-R3-39: as the lane last read them.
        std::lock_guard<std::mutex> lock(m_nnrMutex);
        return m_nnrDiagnosticsCache;
    }
    return NnrAdapter::diagnostics(m_channelId);
}

std::optional<NnrDiagnostics> RxChannel::knownNnrDiagnostics() const
{
    std::lock_guard<std::mutex> lock(m_nnrMutex);
    if (!m_nnrDiagnosticsKnown) {
        return std::nullopt;
    }
    return m_nnrDiagnosticsCache;
}

void RxChannel::refreshNnrDiagnostics()
{
    runOrdered([this]() { refreshNnrDiagnosticsOnLane(); });
}

void RxChannel::refreshNnrDiagnosticsOnLane(const QString* explanationOverride)
{
    NnrDiagnostics diagnostics = NnrAdapter::diagnostics(m_channelId);
    if (explanationOverride != nullptr) {
        diagnostics.explanation = *explanationOverride;
    }
    {
        std::lock_guard<std::mutex> lock(m_nnrMutex);
        m_nnrDiagnosticsCache = diagnostics;
        m_nnrDiagnosticsKnown = true;
    }
    emit nnrDiagnosticsRefreshed();
}

bool RxChannel::setNnrDiagnostics(int testMode, int outputMode, QString* reason)
{
    if (m_lane == nullptr || m_lane->isCurrentThread()) {
        return NnrAdapter::setDiagnostics(m_channelId, testMode, outputMode, reason);
    }
    // R-R3-39: NnrAdapter::setDiagnostics's refusals, decided at once from
    // what the lane last read (SetRXANNRDiagnostics refuses a mode out of
    // range and a receiver whose network is not loaded; nnr.c).
    if (reason) {
        reason->clear();
    }
    if (testMode < 0 || testMode > 2 || outputMode < 0 || outputMode > 1) {
        if (reason) {
            *reason = QStringLiteral("Unsupported NNR diagnostic mode.");
        }
        return false;
    }
    if (const auto known = knownNnrDiagnostics(); known && (!known->available || !known->ready)) {
        if (reason) {
            *reason = QStringLiteral("The NNR receiver is not ready.");
        }
        return false;
    }
    runOrdered([this, testMode, outputMode]() {
        QString refusal;
        const bool accepted =
            NnrAdapter::setDiagnostics(m_channelId, testMode, outputMode, &refusal);
        refreshNnrDiagnosticsOnLane();
        if (!accepted) {
            emit nnrRequestRefused(refusal, -1);
        }
    });
    return true;
}

bool RxChannel::requestNnrLimit(int limit)
{
    if (m_lane == nullptr || m_lane->isCurrentThread()) {
        if (!NnrAdapter::requestLimit(m_channelId, limit)) {
            return false;
        }
        m_nnrLimit.store(limit, std::memory_order_release);
        return true;
    }
    if (!isValidNnrLimit(limit)) {
        return false;
    }
    m_nnrLimit.store(limit, std::memory_order_release);
    // In order with the other NNR calls: RadioModel clears a limit before
    // NNR goes on and after it goes off (R-R3-40).
    runOrdered([this, limit]() { NnrAdapter::requestLimit(m_channelId, limit); });
    return true;
}

void RxChannel::storeActiveNrFlags(NrSlot slot)
{
    m_activeNr.store(slot, std::memory_order_release);

    // Post-WDSP filter flags.  Filter instances added in Tasks 9-11; for now
    // these atomics just record intent so flag-flipping can be tested before
    // the filter objects exist.
#ifdef HAVE_DFNR
    // R-R3-39: DFNR runs only once its instance is published (release,
    // before this store), so the audio thread never sees the flag set with
    // a half-built instance. ensureDfnrOnLane sets it after publishing.
    m_dfnrActive.store(slot == NrSlot::DFNR
                           && m_dfnrInstance.load(std::memory_order_acquire) != nullptr,
                       std::memory_order_release);
#else
    m_dfnrActive.store(slot == NrSlot::DFNR, std::memory_order_release);
#endif
    m_bnrActive .store(slot == NrSlot::BNR,  std::memory_order_release);
    m_mnrActive .store(slot == NrSlot::MNR,  std::memory_order_release);

    // Keep legacy stub atomics in sync with the new single source of truth
    // until Task 12 retires setEmnrEnabled / setNrEnabled.  Not strictly
    // required for correctness, but avoids surprising readers of the old API.
    m_nrEnabled  .store(slot == NrSlot::NR1 || slot == NrSlot::NR2 ||
                        slot == NrSlot::NR3 || slot == NrSlot::NR4 || slot == NrSlot::NNR);
    m_emnrEnabled.store(slot == NrSlot::NR2);
}

bool RxChannel::dfnrAvailable()
{
#ifdef HAVE_DFNR
    return !NereusSDR::ModelPaths::dfnrModelTarball().isEmpty();
#else
    return false;
#endif
}

bool RxChannel::dfnrLoaded() const
{
#ifdef HAVE_DFNR
    return m_dfnrInstance.load(std::memory_order_acquire) != nullptr;
#else
    return false;
#endif
}

void RxChannel::ensureDfnrOnLane(NrSlot slot)
{
#ifdef HAVE_DFNR
    if (slot == NrSlot::DFNR && m_dfnr == nullptr
        && !m_dfnrUnavailable.load(std::memory_order_acquire)) {
        if (!dfnrAvailable()) {
            qCWarning(lcDsp) << "DFNR not available on channel" << m_channelId
                             << "(model not found)";
            m_dfnrUnavailable.store(true, std::memory_order_release);
            emit dfnrUnavailable(true);
        } else {
            auto instance = std::make_unique<NereusSDR::DeepFilterFilter>();
            if (!instance->isValid()) {
                qCWarning(lcDsp) << "DFNR not available on channel" << m_channelId
                                 << "(model failed to load)";
                m_dfnrUnavailable.store(true, std::memory_order_release);
                emit dfnrUnavailable(false);
            } else {
                m_dfnr = std::move(instance);
                // Publish before any flag can be set (release pairs with the
                // audio thread's acquire load after it reads the flag).
                m_dfnrInstance.store(m_dfnr.get(), std::memory_order_seq_cst);
                // After publishing, so a tuning setter that missed the
                // instance has already stored the value read here.
                m_dfnr->setAttenLimit(m_dfnrAttenLimit.load(std::memory_order_seq_cst));
                m_dfnr->setPostFilterBeta(m_dfnrPostFilterBeta.load(std::memory_order_seq_cst));
            }
        }
    }
    // The selection may have changed while the model loaded; the flag
    // follows the current one. Every later selection posts its own lane
    // job, which sets the flag again from the selection it made.
    m_dfnrActive.store(m_activeNr.load(std::memory_order_acquire) == NrSlot::DFNR
                           && m_dfnrInstance.load(std::memory_order_acquire) != nullptr,
                       std::memory_order_release);
#else
    Q_UNUSED(slot);
#endif
}

bool RxChannel::applyActiveNrOnLane(NrSlot slot)
{
    // R-R3-39: DFNR's instance is built at its first selection, here, on
    // the receive lane; any other selection re-reads the DFNR flag so a
    // build that raced a newer selection cannot leave DFNR running.
    ensureDfnrOnLane(slot);

    // Disable NNR before changing any retained NR run flag. Readiness and
    // full tuning are accepted before enabling it below.
    NnrAdapter::setRunning(m_channelId, false);

#ifdef HAVE_WDSP
    // From Thetis console.cs:43297-43450 SelectNR() [v2.10.3.13] —
    // flip all four WDSP NR Run flags so exactly zero or one is active.
    if (NnrAdapter::diagnostics(m_channelId).available) {
        SetRXAANRRun (m_channelId, (slot == NrSlot::NR1) ? 1 : 0);
        SetRXAEMNRRun(m_channelId, (slot == NrSlot::NR2) ? 1 : 0);
        SetRXARNNRRun(m_channelId, (slot == NrSlot::NR3) ? 1 : 0);
        SetRXASBNRRun(m_channelId, (slot == NrSlot::NR4) ? 1 : 0);
    }
#endif

    // Channel lifetime is serialized by WdspEngine. If that boundary was
    // nevertheless lost, report bypass instead of a running NNR claim.
    return slot != NrSlot::NNR || NnrAdapter::setRunning(m_channelId, true);
}

bool RxChannel::setActiveNr(NrSlot slot)
{
    if (static_cast<int>(slot) < 0 || static_cast<int>(slot) > static_cast<int>(NrSlot::NNR))
        return false;

    if (m_lane == nullptr || m_lane->isCurrentThread()) {
        if (slot == NrSlot::NNR) {
            // R-R3-40: enable the configuration WDSP last accepted. Never re-apply
            // m_nnrTuning here: it starts as the default (Standard) and changes
            // only when an apply succeeds, so after a refused apply it is stale,
            // and re-applying it ran Standard while the slice showed Premium.
            // The owner (RadioModel) applies the saved choice before this call.
            const auto state = NnrAdapter::diagnostics(m_channelId);
            if (!state.ready || !state.rateSupported) {
                return false;
            }
        }
        if (!applyActiveNrOnLane(slot)) {
            storeActiveNrFlags(NrSlot::Off);
            return false;
        }
        storeActiveNrFlags(slot);
        return true;
    }

    // R-R3-39: the same readiness test, from what the lane last read; the
    // lane has the last word.
    if (slot == NrSlot::NNR) {
        if (const auto known = knownNnrDiagnostics();
            known && (!known->ready || !known->rateSupported)) {
            return false;
        }
    }
    storeActiveNrFlags(slot);
    runOrdered([this, slot]() {
        if (!applyActiveNrOnLane(slot)) {
            NrSlot expected = slot;
            if (m_activeNr.compare_exchange_strong(expected, NrSlot::Off,
                                                   std::memory_order_acq_rel)) {
                storeActiveNrFlags(NrSlot::Off);
            }
            refreshNnrDiagnosticsOnLane();
            emit nnrRequestRefused(QString(), -1);
            return;
        }
        refreshNnrDiagnosticsOnLane();
    });
    return true;
}

bool RxChannel::selectNr(NrSlot slot, const NnrSettings* tuningFirst, NrSlot previous,
                         QString* reason)
{
    if (reason) {
        reason->clear();
    }
    if (m_lane == nullptr || m_lane->isCurrentThread()) {
        if (tuningFirst != nullptr && !setNnrTuning(*tuningFirst, reason)) {
            return false;
        }
        return setActiveNr(slot);
    }
    if (static_cast<int>(slot) < 0 || static_cast<int>(slot) > static_cast<int>(NrSlot::NNR)) {
        return false;
    }

    // R-R3-39: the refusals of setNnrTuning and setActiveNr, decided at once
    // from what the lane last read. After the tuning is applied the running
    // network is the one for its model slot, so that slot's model decides
    // readiness.
    const std::optional<NnrDiagnostics> known = knownNnrDiagnostics();
    if (tuningFirst != nullptr) {
        if (!tuningFirst->isValid()) {
            if (reason) {
                *reason = QStringLiteral("NNR values must be finite and within their supported ranges.");
            }
            return false;
        }
        if (known && (!known->available || tuningFirst->modelSlot < 0 || tuningFirst->modelSlot > 1
                      || !known->modelAvailable[static_cast<std::size_t>(tuningFirst->modelSlot)])) {
            if (reason) {
                *reason = QStringLiteral("The requested NNR model is not ready; no tuning was changed.");
            }
            return false;
        }
    }
    if (slot == NrSlot::NNR && known) {
        const bool ready = tuningFirst != nullptr
            ? known->modelAvailable[static_cast<std::size_t>(tuningFirst->modelSlot)]
            : known->ready;
        if (!ready || !known->rateSupported) {
            return false;
        }
    }

    std::optional<NnrSettings> tuning;
    NnrSettings previousTuning;
    if (tuningFirst != nullptr) {
        tuning = *tuningFirst;
        std::lock_guard<std::mutex> lock(m_nnrMutex);
        previousTuning = m_nnrTuning;
        m_nnrTuning = *tuningFirst;
    }
    storeActiveNrFlags(slot);
    runOrdered([this, slot, tuning, previousTuning, previous]() {
        auto refuse = [this, slot, previous](const QString& why) {
            NrSlot expected = slot;
            if (m_activeNr.compare_exchange_strong(expected, NrSlot::Off,
                                                   std::memory_order_acq_rel)) {
                storeActiveNrFlags(NrSlot::Off);
            }
            refreshNnrDiagnosticsOnLane();
            emit nnrRequestRefused(why, static_cast<int>(previous));
        };
        if (tuning) {
            QString refusal;
            const auto accepted = NnrAdapter::apply(m_channelId, *tuning, &refusal);
            {
                std::lock_guard<std::mutex> lock(m_nnrMutex);
                if (accepted) {
                    m_nnrTuning = *accepted;
                } else if (m_nnrTuning == *tuning) {
                    m_nnrTuning = previousTuning;
                }
            }
            if (!accepted) {
                // The selection is refused with the tuning, as at once: NR
                // stays as it was in WDSP (nothing below ran).
                NrSlot expected = slot;
                if (m_activeNr.compare_exchange_strong(expected, previous,
                                                       std::memory_order_acq_rel)) {
                    storeActiveNrFlags(previous);
                }
                refreshNnrDiagnosticsOnLane();
                emit nnrRequestRefused(refusal, static_cast<int>(previous));
                return;
            }
        }
        if (!applyActiveNrOnLane(slot)) {
            refuse(QString());
            return;
        }
        refreshNnrDiagnosticsOnLane();
    });
    return true;
}

void RxChannel::applyNnrState(int limit, const NnrSettings& settings, NrSlot slot,
                              bool nr3Blocked)
{
    // The selection before this call, kept when NNR turns out not to be
    // ready: setActiveNr(NNR) then returns without touching the channel.
    const NrSlot before = activeNr();
    const NrSlot optimistic = nr3Blocked ? NrSlot::Off : slot;
    if (isValidNnrLimit(limit)) {
        m_nnrLimit.store(limit, std::memory_order_release);
    }
    storeActiveNrFlags(optimistic);
    runOrdered([this, limit, settings, slot, nr3Blocked, before, optimistic]() {
        QString reason;
        // R-R3-40: a new or reopened channel starts with the slice's runtime
        // limit, applied by the tuning call below.
        NnrAdapter::requestLimit(m_channelId, limit);
        const auto accepted = NnrAdapter::apply(m_channelId, settings, &reason);
        if (accepted) {
            std::lock_guard<std::mutex> lock(m_nnrMutex);
            m_nnrTuning = *accepted;
        }
        // A missing saved model must not enable a different model silently.
        // Retain the saved preference so the operator can repair the asset.
        const NrSlot run = (!nr3Blocked && (accepted || slot != NrSlot::NNR)) ? slot : NrSlot::Off;
        NrSlot result = run;
        if (run == NrSlot::NNR) {
            const auto state = NnrAdapter::diagnostics(m_channelId);
            if (!state.ready || !state.rateSupported) {
                result = before;   // setActiveNr(NNR) refused before any change
            } else if (!applyActiveNrOnLane(run)) {
                result = NrSlot::Off;
            }
        } else if (!applyActiveNrOnLane(run)) {
            result = NrSlot::Off;
        }
        NrSlot expected = optimistic;
        if (result == optimistic
            || m_activeNr.compare_exchange_strong(expected, result, std::memory_order_acq_rel)) {
            storeActiveNrFlags(result);
        }
        refreshNnrDiagnosticsOnLane(accepted ? nullptr : &reason);
    });
}

// ---------------------------------------------------------------------------
// SNB (Spectral Noise Blanker)
// ---------------------------------------------------------------------------

void RxChannel::setSnbEnabled(bool enabled)
{
    if (m_nb) m_nb->setSnbEnabled(enabled);
}

// ── NB1 / NB2 / SNB detailed tuning ─────────────────────────────────────────
// Re-added per slice after the Sub-Epic J follow-up. These were removed
// 2026-04-22 in favour of the NB/SNB setup page calling WDSP directly, but
// that page hardcoded channel 0, so tuning the blanker always hit receiver A
// whichever receiver the operator was working. SliceModel owns the state now
// and RadioModel pushes it here for the addressed slice, the same route every
// other per-slice DSP setting takes.
void RxChannel::setNbThreshold(double threshold)
{
    if (m_nb) m_nb->setNbThreshold(threshold);
}

void RxChannel::setNbTransitionMs(double ms)
{
    if (m_nb) m_nb->setNbTauMs(ms);
}

void RxChannel::setNbLeadMs(double ms)
{
    if (m_nb) m_nb->setNbLeadMs(ms);
}

void RxChannel::setNbLagMs(double ms)
{
    if (m_nb) m_nb->setNbLagMs(ms);
}

void RxChannel::setNb2Mode(int mode)
{
    if (m_nb) m_nb->setNb2Mode(mode);
}

void RxChannel::setSnbK1(double k1)
{
    if (m_nb) m_nb->setSnbK1(k1);
}

void RxChannel::setSnbK2(double k2)
{
    if (m_nb) m_nb->setSnbK2(k2);
}

void RxChannel::setSnbOutputBandwidthHz(int bandwidthHz)
{
    if (m_nb) m_nb->setSnbOutputBandwidthHz(bandwidthHz);
}

// ---------------------------------------------------------------------------
// APF — Audio Peak Filter
// ---------------------------------------------------------------------------

void RxChannel::setApfEnabled(bool enabled)
{
    if (enabled == m_apfEnabled.load()) {
        return;
    }

    m_apfEnabled.store(enabled);

#ifdef HAVE_WDSP
    runKeyed(laneParameter("setApfEnabled"), 0, [=, this]() {
        // From Thetis Project Files/Source/Console/radio.cs:1910-1927
        //   WDSP.SetRXASPCWRun(WDSP.id(thread, subrx), value)
        // WDSP third_party/wdsp/src/apfshadow.c:93
        SetRXASPCWRun(m_channelId, enabled ? 1 : 0);
    });
#else
    Q_UNUSED(enabled);
#endif
}

void RxChannel::setApfFreq(double hz)
{
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setApfFreq"), 0, [=, this]() {
        // From Thetis Project Files/Source/Console/radio.cs:1929-1946
        //   WDSP.SetRXASPCWFreq(WDSP.id(thread, subrx), value)
        //   Freq = CWPitch + tuneOffset (setup.cs:17071)
        // WDSP third_party/wdsp/src/apfshadow.c:117
        SetRXASPCWFreq(m_channelId, hz);
    });
#else
    Q_UNUSED(hz);
#endif
}

void RxChannel::setApfBandwidth(double hz)
{
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setApfBandwidth"), 0, [=, this]() {
        // From Thetis Project Files/Source/Console/radio.cs:1948-1965
        //   WDSP.SetRXASPCWBandwidth(WDSP.id(thread, subrx), value)
        //   Default rx_apf_bw = 600.0 Hz
        // WDSP third_party/wdsp/src/apfshadow.c:141
        SetRXASPCWBandwidth(m_channelId, hz);
    });
#else
    Q_UNUSED(hz);
#endif
}

void RxChannel::setApfGain(double gain)
{
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setApfGain"), 0, [=, this]() {
        // From Thetis Project Files/Source/Console/radio.cs:1967-1984
        //   WDSP.SetRXASPCWGain(WDSP.id(thread, subrx), value)
        //   Default rx_apf_gain = 1.0 (linear)
        // WDSP third_party/wdsp/src/apfshadow.c:165
        SetRXASPCWGain(m_channelId, gain);
    });
#else
    Q_UNUSED(gain);
#endif
}

void RxChannel::setApfSelection(int selection)
{
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setApfSelection"), 0, [=, this]() {
        // From Thetis Project Files/Source/Console/radio.cs:1986-2008
        //   WDSP.SetRXASPCWSelection(WDSP.id(thread, subrx), value)
        //   Default _rx_apf_type = 3 (bi-quad)
        //   0=double-pole, 1=matched, 2=gaussian, 3=bi-quad
        // WDSP third_party/wdsp/src/apfshadow.c:45
        SetRXASPCWSelection(m_channelId, selection);
    });
#else
    Q_UNUSED(selection);
#endif
}

// ---------------------------------------------------------------------------
// Squelch — SSB (syllabic squelch)
// ---------------------------------------------------------------------------

void RxChannel::setSsqlEnabled(bool enabled)
{
    if (enabled == m_ssqlEnabled.load()) {
        return;
    }

    m_ssqlEnabled.store(enabled);

#ifdef HAVE_WDSP
    runKeyed(laneParameter("setSsqlEnabled"), 0, [=, this]() {
        // From Thetis Project Files/Source/Console/radio.cs:1185-1207
        //   WDSP.SetRXASSQLRun(WDSP.id(thread, subrx), value)
        // WDSP third_party/wdsp/src/ssql.c:331
        SetRXASSQLRun(m_channelId, enabled ? 1 : 0);
    });
#else
    Q_UNUSED(enabled);
#endif
}

void RxChannel::setSsqlThresh(double threshold)
{
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setSsqlThresh"), 0, [=, this]() {
        // From Thetis Project Files/Source/Console/radio.cs:1209-1228
        //   WDSP.SetRXASSQLThreshold(WDSP.id(thread, subrx), _fSSqlThreshold)
        //   threshold range clamped 0.0..1.0 as per ssql.c
        //   Thetis default _fSSqlThreshold = 0.16f
        // WDSP third_party/wdsp/src/ssql.c:339
        SetRXASSQLThreshold(m_channelId, threshold);
    });
#else
    Q_UNUSED(threshold);
#endif
}

// ---------------------------------------------------------------------------
// Squelch — AM
// ---------------------------------------------------------------------------

void RxChannel::setAmsqEnabled(bool enabled)
{
    if (enabled == m_amsqEnabled.load()) {
        return;
    }

    m_amsqEnabled.store(enabled);

#ifdef HAVE_WDSP
    runKeyed(laneParameter("setAmsqEnabled"), 0, [=, this]() {
        // From Thetis Project Files/Source/Console/radio.cs:1293-1310
        //   WDSP.SetRXAAMSQRun(WDSP.id(thread, subrx), value)
        // WDSP third_party/wdsp/src/amsq.c (SetRXAAMSQRun)
        SetRXAAMSQRun(m_channelId, enabled ? 1 : 0);
    });
#else
    Q_UNUSED(enabled);
#endif
}

void RxChannel::setAmsqThresh(double dB)
{
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setAmsqThresh"), 0, [=, this]() {
        // From Thetis Project Files/Source/Console/radio.cs:1164-1178
        //   WDSP.SetRXAAMSQThreshold(WDSP.id(thread, subrx), value)
        //   value is in dB; WDSP amsq.c applies pow(10.0, threshold/20.0) internally
        //   Thetis default rx_squelch_threshold = -150.0f dB
        // WDSP third_party/wdsp/src/amsq.c (SetRXAAMSQThreshold)
        SetRXAAMSQThreshold(m_channelId, dB);
    });
#else
    Q_UNUSED(dB);
#endif
}

// ---------------------------------------------------------------------------
// Squelch — FM
// ---------------------------------------------------------------------------

void RxChannel::setFmsqEnabled(bool enabled)
{
    if (enabled == m_fmsqEnabled.load()) {
        return;
    }

    m_fmsqEnabled.store(enabled);

#ifdef HAVE_WDSP
    runKeyed(laneParameter("setFmsqEnabled"), 0, [=, this]() {
        // From Thetis Project Files/Source/Console/radio.cs:1312-1329
        //   WDSP.SetRXAFMSQRun(WDSP.id(thread, subrx), value)
        // WDSP third_party/wdsp/src/fmsq.c:236
        SetRXAFMSQRun(m_channelId, enabled ? 1 : 0);
    });
#else
    Q_UNUSED(enabled);
#endif
}

void RxChannel::setFmsqThresh(double dB)
{
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setFmsqThresh"), 0, [=, this]() {
        // From Thetis Project Files/Source/Console/radio.cs:1274-1291
        //   WDSP.SetRXAFMSQThreshold(WDSP.id(thread, subrx), value)
        //   Thetis fm_squelch_threshold = 1.0f is LINEAR (0..1 scale).
        //   SliceModel stores in dB domain (m_fmsqThresh = -150.0 default).
        //   Convert dB → linear before passing to WDSP.
        //   -150.0 dB → ~3.16e-8 (effectively muted = squelch open on FM)
        // WDSP third_party/wdsp/src/fmsq.c:244 — assigns threshold directly to tail_thresh (linear)
        const double linear = std::pow(10.0, dB / 20.0);
        SetRXAFMSQThreshold(m_channelId, linear);
    });
#else
    Q_UNUSED(dB);
#endif
}

// ---------------------------------------------------------------------------
// Audio panel — mute / pan / binaural
// ---------------------------------------------------------------------------

void RxChannel::setMuted(bool muted)
{
    if (muted == m_muted.load()) {
        return;
    }

    m_muted.store(muted);

#ifdef HAVE_WDSP
    runKeyed(laneParameter("setMuted"), 0, [=, this]() {
        // Mute → run=0 (panel disabled), unmute → run=1 (panel enabled).
        // From Thetis Project Files/Source/Console/dsp.cs:393-394 — P/Invoke decl
        // WDSP: third_party/wdsp/src/patchpanel.c:126
        SetRXAPanelRun(m_channelId, muted ? 0 : 1);
    });
#else
    Q_UNUSED(muted);
#endif
}

void RxChannel::setAfGain(double gain)
{
    // Clamp into the same 0.0..1.0 envelope Thetis enforces upstream by
    // dividing slider/Maximum at the call site (console.cs:36701, 38717
    // [v2.10.3.14]). WDSP itself does not range-check gain1, so a stray
    // value above 1.0 would re-introduce the +12 dB hot-output behaviour
    // we are explicitly fixing.
    gain = std::clamp(gain, 0.0, 1.0);
    if (gain == m_afGain.load()) {
        return;
    }
    m_afGain.store(gain);

#ifdef HAVE_WDSP
    runKeyed(laneParameter("setAfGain"), 0, [=, this]() {
        // From Thetis Project Files/Source/Console/radio.cs:1077-1107 [v2.10.3.14]
        //   rx_output_gain_dsp = 1.0 + WDSP.SetRXAPanelGain1(WDSP.id(thread, subrx), value)
        //   //[2.10.3.5]MW0LGE wave recorder volume normalise  — wave recorder
        //     branch deliberately not ported here; NereusSDR has no wave_file_writer
        //     yet, and recorder gain hooks belong in the recorder module when it lands.
        // WDSP: third_party/wdsp/src/patchpanel.c:142 — assigns directly to
        //   rxa[channel].panel.p->gain1 under csDSP critical section.
        //
        // Slice control plan Task 6 (JJ's ruling), a departure from the
        // radio.cs wiring above: the AF level is applied in AudioEngine's
        // mixer, to the controller's audio only, so each listener can hear
        // the slice at its own level and VAX stays audible at AF 0. The
        // panel gain stays at unity; m_afGain is kept as the slice's value.
        Q_UNUSED(gain);
        SetRXAPanelGain1(m_channelId, kPanelGain1Unity);
    });
#endif
}

void RxChannel::setAudioPan(double pan)
{
#ifdef HAVE_WDSP
    runKeyed(laneParameter("setAudioPan"), 0, [=, this]() {
        // Convert NereusSDR -1.0..+1.0 to WDSP 0.0..1.0:
        //   wdsp_pan = (nereus_pan + 1.0) / 2.0
        //   -1.0 → 0.0 (full left), 0.0 → 0.5 (center), +1.0 → 1.0 (full right)
        // WDSP applies sin-law: gain2I = sin(pan*PI), gain2Q = 1 when pan>0.5
        // From Thetis Project Files/Source/Console/radio.cs:1386-1403
        //   default pan = 0.5f (center in WDSP 0..1 scale → NereusSDR 0.0)
        // WDSP: third_party/wdsp/src/patchpanel.c:159
        const double wdspPan = (pan + 1.0) / 2.0;
        SetRXAPanelPan(m_channelId, wdspPan);
    });
#else
    Q_UNUSED(pan);
#endif
}

void RxChannel::setBinauralEnabled(bool enabled)
{
    if (enabled == m_binauralEnabled.load()) {
        return;
    }

    m_binauralEnabled.store(enabled);

#ifdef HAVE_WDSP
    runKeyed(laneParameter("setBinauralEnabled"), 0, [=, this]() {
        // bin=1 → copy=0 → binaural (I/Q separate headphone stereo image)
        // bin=0 → copy=1 → dual-mono (Q := I, same audio on both channels)
        // From Thetis Project Files/Source/Console/radio.cs:1145-1162
        //   default bin_on = false → dual-mono
        // WDSP: third_party/wdsp/src/patchpanel.c:187
        SetRXAPanelBinaural(m_channelId, enabled ? 1 : 0);
    });
#else
    Q_UNUSED(enabled);
#endif
}

// ---------------------------------------------------------------------------
// Frequency shift (pan offset from VFO)
// ---------------------------------------------------------------------------

void RxChannel::setShiftFrequency(double offsetHz)
{
    if (offsetHz == m_shiftOffsetHz) {
        return;
    }

    m_shiftOffsetHz = offsetHz;

#ifdef HAVE_WDSP
    // From Thetis radio.cs:1419-1420 [v2.10.3.15]: both calls use the same
    // sign, and both fire on EVERY RXOsc change, including a change back to
    // zero. Thetis has no run gate at all: SetRXAShiftRun appears nowhere in
    // its Console tree, so the gate below is NereusSDR-original and now
    // covers only the run flag.
    //
    // The two frequency pushes used to sit inside the else of an
    // if (std::abs(offsetHz) < 0.5) branch, so returning to zero skipped
    // them. SetRXAShiftRun writes rxa[channel].shift.p->run (shift.c:113-118)
    // and never touches NOTCHDB->shift, RXANBPSetShiftFrequency is that
    // field's sole writer (nbp.c:487-496), and calc_nbp_lightweight consumes
    // it unconditionally (nbp.c:192). The stored shift therefore went stale
    // on every RIT-off, DIGU/DIGL exit, band jump and CTUN-off, and every
    // notch would have been mapped off its carrier.
    //
    // No sign change. Thetis's -value is not a divergence: rx_osc is already
    // the negated quantity upstream (console.cs:31916-31922,
    // rx2_osc = RXOsc - diff), so Thetis's -rx_osc equals the offsetHz handed
    // in here, which equals frequencyHz - centreHz at
    // SliceStreamAllocator.cpp:70.
    runKeyed(laneParameter("setShiftFrequency"), 0, [this, offsetHz]() {
        SetRXAShiftFreq(m_channelId, offsetHz);
        RXANBPSetShiftFrequency(m_channelId, offsetHz);
        // Written here, next to the call it mirrors, and not up beside
        // m_shiftOffsetHz: notchShiftHz() exists to say whether the push above
        // really happened.
        m_notchShiftHz.store(offsetHz, std::memory_order_release);
        // No offset: disable shift for efficiency. The run flag is the only
        // thing the magnitude gate still controls.
        SetRXAShiftRun(m_channelId, std::abs(offsetHz) < 0.5 ? 0 : 1);
    });
#else
    m_notchShiftHz.store(offsetHz, std::memory_order_release);
#endif
}

// ---------------------------------------------------------------------------
// Notch bandpass tune frequency (TNF section 4)
// ---------------------------------------------------------------------------

void RxChannel::setNotchTuneFrequency(double absoluteHz)
{
    // Carry set outside the WDSP guard, mirroring setShiftFrequency, so a
    // stub build and the unit tests still see the quantity the caller
    // resolved.
    m_notchTuneFrequencyHz = absoluteHz;

#ifdef HAVE_WDSP
    runKeyed(laneParameter("setNotchTuneFrequency"), 0, [=, this]() {
        // From Thetis console.cs:31940-31941 [v2.10.3.15]: pushed on every
        // retune, unconditionally, and the SAME value goes to every subrx
        // sharing the stream. RXANBPSetTuneFrequency is internally idempotent
        // (nbp.c:479, if (tunefreq != a->tunefreq)), so an unconditional push
        // costs nothing.
        RXANBPSetTuneFrequency(m_channelId, absoluteHz);
    });
#endif
}

// ---------------------------------------------------------------------------
// Manual notch filter (TNF): the per-channel WDSP notch database
// ---------------------------------------------------------------------------

bool RxChannel::addNotch(int index, const Notch& n)
{
#ifdef HAVE_WDSP
    // From Thetis console.cs:40271-40273 [v2.10.3.15], AddNotch pushes the
    // same (index, centre, width, active) tuple to every RX channel. Centre
    // and width are absolute Hz on the wire (console.cs:40271 passes fFreqHZ
    // straight through).
    // WDSP: third_party/wdsp/src/nbp.c:362, an INSERT guarded by
    // `notch <= b->nn && b->nn < b->maxnotches`; returns -1 with no mutation
    // otherwise.
    const int rval = RXANBPAddNotch(m_channelId, index, n.centerHz, n.widthHz,
                                    n.active ? 1 : 0);
    if (rval < 0) {
        qCWarning(lcDsp) << "RxChannel" << m_channelId
                         << "RXANBPAddNotch rejected index" << index
                         << "centreHz" << n.centerHz
                         << "existing" << notchCount();
        return false;
    }
    return true;
#else
    Q_UNUSED(index);
    Q_UNUSED(n);
    return false;
#endif
}

bool RxChannel::editNotch(int index, const Notch& n)
{
#ifdef HAVE_WDSP
    // From Thetis console.cs:40028-40030 [v2.10.3.15] (ChangeNotchBW) and
    // console.cs:40100-40102 [v2.10.3.15] (ChangeNotchCentreFrequency). Both
    // Thetis edit paths read the current tuple back, change one member and
    // push the whole tuple; NereusSDR's caller already holds the whole tuple,
    // so the readback is unnecessary.
    // WDSP: third_party/wdsp/src/nbp.c:444, returns -1 when notch >= nn.
    //
    // Not cheap: RXANBPEditNotch runs UpdateNBPFilters (nbp.c:345-359), which
    // designs nbp0 AND recalc_bpsnba_filter (snb.c:814-828). That is one
    // filter pair per edit, versus 2N for a full syncNotches, which is why
    // live edits take this path.
    const int rval = RXANBPEditNotch(m_channelId, index, n.centerHz, n.widthHz,
                                     n.active ? 1 : 0);
    if (rval < 0) {
        qCWarning(lcDsp) << "RxChannel" << m_channelId
                         << "RXANBPEditNotch rejected index" << index
                         << "of" << notchCount();
        return false;
    }
    return true;
#else
    Q_UNUSED(index);
    Q_UNUSED(n);
    return false;
#endif
}

bool RxChannel::deleteNotch(int index)
{
#ifdef HAVE_WDSP
    // From Thetis console.cs:40207-40209 [v2.10.3.15], removeNotch.
    // WDSP: third_party/wdsp/src/nbp.c:418, erases and shifts the array down,
    // so the caller's list must shift the same way (design doc section 5.2).
    const int rval = RXANBPDeleteNotch(m_channelId, index);
    if (rval < 0) {
        qCWarning(lcDsp) << "RxChannel" << m_channelId
                         << "RXANBPDeleteNotch rejected index" << index
                         << "of" << notchCount();
        return false;
    }
    return true;
#else
    Q_UNUSED(index);
    return false;
#endif
}

void RxChannel::syncNotches(const QList<Notch>& notches)
{
    runOrdered([this, notches]() { syncNotchesNow(notches); });
}

void RxChannel::syncNotchesNow(const QList<Notch>& notches)
{
#ifdef HAVE_WDSP
    // Drop whatever the channel is currently carrying. Always erase index 0:
    // RXANBPDeleteNotch shifts the array down (nbp.c:426-434), so repeatedly
    // removing the head walks the whole database without index arithmetic.
    for (int remaining = notchCount(); remaining > 0; --remaining) {
        RXANBPDeleteNotch(m_channelId, 0);
    }

    // From Thetis setup.cs:18002-18004 [v2.10.3.15],
    // RestoreNotchesFromDatabase: one RXANBPAddNotch per stored notch with
    // the loop counter as the index, which is what makes list position and
    // WDSP index the same thing (design doc section 5.2).
    // sets max limits, and selects first notch if one exists MW0LGE
    //   [original inline comment from setup.cs:18007]
    for (int i = 0; i < notches.size(); ++i) {
        const Notch& n = notches.at(i);
        if (RXANBPAddNotch(m_channelId, i, n.centerHz, n.widthHz,
                           n.active ? 1 : 0) < 0) {
            qCWarning(lcDsp) << "RxChannel" << m_channelId
                             << "notch sync truncated at index" << i
                             << "of" << notches.size();
            return;
        }
    }
#else
    Q_UNUSED(notches);
#endif
}

int RxChannel::notchCount() const
{
#ifdef HAVE_WDSP
    // From Thetis console.cs:40265 [v2.10.3.15], AddNotch reads the count
    // back out of WDSP before it picks an insert index.
    // WDSP: third_party/wdsp/src/nbp.c:465
    if (!wdspChannelInRange()) {
        return 0;
    }
    int n = 0;
    RXANBPGetNumNotches(m_channelId, &n);
    return n;
#else
    return 0;
#endif
}

void RxChannel::setNotchesRun(bool run)
{
    // Carry set outside the WDSP guard, mirroring setNotchTuneFrequency, so
    // a stub build and the unit tests still see what the channel was told.
    m_notchesRun = run;

#ifdef HAVE_WDSP
    runOrdered([=, this]() {
        // From Thetis console.cs:40000-40002 [v2.10.3.15], the TNFActive setter
        // fans the same flag to all three fixed channel ids.
        // WDSP: third_party/wdsp/src/nbp.c:499, the only writer of
        // notchdb.master_run; it also drives nbp0.fnfrun and re-runs
        // RXAbpsnbaCheck / RXAbpsnbaSet, so it is not a cheap toggle.
        RXANBPSetNotchesRun(m_channelId, run ? 1 : 0);
    });
#endif
}

void RxChannel::setNotchAutoIncrease(bool on)
{
    m_notchAutoIncrease = on;

#ifdef HAVE_WDSP
    runOrdered([=, this]() {
        // From Thetis setup.cs:17928-17930 [v2.10.3.15],
        // chkMNFAutoIncrease_CheckedChanged.
        // WDSP: third_party/wdsp/src/nbp.c:604, touches both nbp0 and bpsnba.
        RXANBPSetAutoIncrease(m_channelId, on ? 1 : 0);
    });
#endif
}

double RxChannel::minNotchWidthHz() const
{
    if (m_lane != nullptr && !m_lane->isCurrentThread()) {
        // R-R3-39: as the lane last read it.
        return m_minNotchWidthCache.load(std::memory_order_acquire);
    }
    return readMinNotchWidthNow();
}

double RxChannel::readMinNotchWidthNow() const
{
#ifdef HAVE_WDSP
    // From Thetis console.cs:48804 [v2.10.3.15], the per-RX minimum notch
    // width readback that feeds Thetis's _minimum_rx_notch_width map.
    // WDSP: third_party/wdsp/src/nbp.c:594 -> min_notch_width (nbp.c:82-95),
    // which scales with the filter's coefficient count and sample rate.
    if (!wdspChannelInRange()) {
        return 0.0;
    }
    double minWidth = 0.0;
    RXANBPGetMinNotchWidth(m_channelId, &minWidth);
    return minWidth;
#else
    return 0.0;
#endif
}

bool RxChannel::notchAt(int index, Notch& out) const
{
#ifdef HAVE_WDSP
    if (!wdspChannelInRange()) {
        return false;
    }
    double centerHz = 0.0;
    double widthHz  = 0.0;
    int    active   = 0;
    // WDSP: third_party/wdsp/src/nbp.c:393 returns 0 on success; on -1 it
    // writes fcenter -1.0 / fwidth 0.0 / active -1 (nbp.c:406-411), which
    // must not reach the caller as if it were a real notch.
    if (RXANBPGetNotch(m_channelId, index, &centerHz, &widthHz, &active) != 0) {
        return false;
    }
    out.centerHz = centerHz;
    out.widthHz  = widthHz;
    out.active   = (active != 0);
    return true;
#else
    Q_UNUSED(index);
    Q_UNUSED(out);
    return false;
#endif
}

// ---------------------------------------------------------------------------
// R-R3-39: the notch database on the receive lane
//
// NereusSDR-original. RadioModel's fan-out used to call addNotch /
// editNotch / deleteNotch, resync on a refusal, then compare notchCount()
// with the model (design section 6.2). Every one of those takes the
// channel's DSP lock, so the whole sequence now runs as one ordered lane job.
// ---------------------------------------------------------------------------

void RxChannel::addNotchReconciled(int index, const Notch& n, const QList<Notch>& expected)
{
    runOrdered([this, index, n, expected]() {
        // RXANBPAddNotch is an INSERT guarded by
        // "notch <= b->nn && b->nn < b->maxnotches", returning -1 with no
        // mutation at all (third_party/wdsp/src/nbp.c:362-390). Design
        // section 6.2: surface it, and recover with a full resync rather
        // than an assert, which a release build compiles out.
        if (!addNotch(index, n)) {
            syncNotchesNow(expected);
        }
        reconcileNotchCountNow(expected);
    });
}

void RxChannel::editNotchReconciled(int index, const Notch& n, const QList<Notch>& expected)
{
    runOrdered([this, index, n, expected]() {
        if (!editNotch(index, n)) {
            syncNotchesNow(expected);
        }
        reconcileNotchCountNow(expected);
    });
}

void RxChannel::deleteNotchReconciled(int index, const QList<Notch>& expected)
{
    runOrdered([this, index, expected]() {
        // The former index, not the entry's current one: the entry is gone
        // from the model by now. WDSP shifts its own array down internally
        // (nbp.c:418-441) and the model's list does the same, so positions
        // stay aligned (design section 5.2).
        if (!deleteNotch(index)) {
            syncNotchesNow(expected);
        }
        reconcileNotchCountNow(expected);
    });
}

void RxChannel::reconcileNotchCount(const QList<Notch>& expected)
{
    runOrdered([this, expected]() { reconcileNotchCountNow(expected); });
}

void RxChannel::reconcileNotchCountNow(const QList<Notch>& expected)
{
    // RXANBPGetNumNotches takes the channel's DSP critical section
    // (third_party/wdsp/src/nbp.c:465-472). Negligible next to
    // UpdateNBPFilters, which every mutation already pays and which designs
    // two filters, nbp0 plus recalc_bpsnba_filter (nbp.c:345-359 ->
    // snb.c:814-828).
    const int actual = notchCount();
    if (actual == expected.size()) {
        return;
    }
    qCWarning(lcDsp) << "Notch index divergence on RX channel"
                     << m_channelId << "- WDSP holds" << actual
                     << "notches, the model holds" << expected.size()
                     << "- resyncing";
    syncNotchesNow(expected);
}

void RxChannel::refreshMinNotchWidthOnLane()
{
    const double minWidth = readMinNotchWidthNow();
    m_minNotchWidthCache.store(minWidth, std::memory_order_release);
    emit minNotchWidthChanged(minWidth);
}

void RxChannel::requestNotchCount(QObject* context, std::function<void(int)> done)
{
    if (m_lane == nullptr || m_lane->isCurrentThread()) {
        if (done) {
            done(notchCount());
        }
        return;
    }
    m_lane->request<std::optional<int>>(
        [this, alive = m_alive]() -> std::optional<int> {
            if (!alive->load(std::memory_order_acquire)) {
                return std::nullopt;
            }
            return notchCount();
        },
        context,
        [done = std::move(done)](std::optional<int> count) {
            if (count && done) {
                done(*count);
            }
        });
}

void RxChannel::requestNotchAt(int index, QObject* context,
                               std::function<void(bool ok, Notch notch)> done)
{
    if (m_lane == nullptr || m_lane->isCurrentThread()) {
        Notch notch;
        const bool ok = notchAt(index, notch);
        if (done) {
            done(ok, notch);
        }
        return;
    }
    using Reading = std::optional<std::pair<bool, Notch>>;
    m_lane->request<Reading>(
        [this, alive = m_alive, index]() -> Reading {
            if (!alive->load(std::memory_order_acquire)) {
                return std::nullopt;
            }
            Notch notch;
            const bool ok = notchAt(index, notch);
            return std::make_pair(ok, notch);
        },
        context,
        [done = std::move(done)](Reading reading) {
            if (reading && done) {
                done(reading->first, reading->second);
            }
        });
}

void RxChannel::requestMinNotchWidth(QObject* context, std::function<void(double)> done)
{
    if (m_lane == nullptr || m_lane->isCurrentThread()) {
        if (done) {
            done(readMinNotchWidthNow());
        }
        return;
    }
    m_lane->request<std::optional<double>>(
        [this, alive = m_alive]() -> std::optional<double> {
            if (!alive->load(std::memory_order_acquire)) {
                return std::nullopt;
            }
            return readMinNotchWidthNow();
        },
        context,
        [done = std::move(done)](std::optional<double> width) {
            if (width && done) {
                done(*width);
            }
        });
}

// ---------------------------------------------------------------------------
// Channel state
// ---------------------------------------------------------------------------

void RxChannel::setActive(bool active)
{
    applyActive(active, /*drainOnStop=*/true);
}

void RxChannel::deactivateWithoutDrain()
{
    applyActive(false, /*drainOnStop=*/false);
}

void RxChannel::applyActive(bool active, bool drainOnStop)
{
    if (active == m_active.load()) {
        return;
    }

    if (m_lane == nullptr || m_lane->isCurrentThread()) {
        applyActiveOnLane(active, drainOnStop, /*alsoRequested=*/true);
        // R-R3-41: nereusd gives an active receive worker a fast core of its own
        // and returns it when the channel stops (no-op in the GUI).
        ThreadPlacement::instance().setChannelActive(ThreadRole::RxWorker,
                                                     m_channelId, active);
#ifdef HAVE_WDSP
        // wdsp/rxa.c:538 [v2.10.3.14] seeds the audio panel with gain1 = 4.0
        // (+12 dB).  Push our cached m_afGain once the channel is alive so the
        // default never reaches the audio sink.  Thetis equivalent: radio.cs
        // initializer at radio.cs:318 + rebroadcast on Update() at radio.cs:422.
        // The model layer will follow up with the persisted slice gain; this
        // is a defence-in-depth seed for the gap between createRxChannel and
        // the first slice sync.
        //
        // Slice control plan Task 6: unity, not m_afGain; the mixer applies
        // the AF level (see setAfGain).
        if (active) {
            SetRXAPanelGain1(m_channelId, kPanelGain1Unity);
        }
#endif
        qCDebug(lcDsp) << "RxChannel" << m_channelId
                        << (active ? "activated" : "deactivated");
        emit activeChanged(active);
        return;
    }

    // R-R3-39: the requested state changes at once; the lane runs the WDSP
    // side in the order applyActiveOnLane keeps (a draining stop waits
    // there, not here), then the AF gain seed. The meter cache holds the
    // old state's reading until a refresh posted after this one runs.
    m_active.store(active);
    m_meterGeneration.fetch_add(1, std::memory_order_acq_rel);
    m_meterCacheReady.store(false, std::memory_order_release);
    ThreadPlacement::instance().setChannelActive(ThreadRole::RxWorker,
                                                 m_channelId, active);
    runOrdered([this, active, drainOnStop]() {
        applyActiveOnLane(active, drainOnStop, /*alsoRequested=*/false);
#ifdef HAVE_WDSP
        if (active) {
            // Slice control plan Task 6: unity; the mixer applies AF.
            SetRXAPanelGain1(m_channelId, kPanelGain1Unity);
        }
#endif
    });
    qCDebug(lcDsp) << "RxChannel" << m_channelId
                    << (active ? "activated" : "deactivated");
    emit activeChanged(active);
}

void RxChannel::applyActiveOnLane(bool active, bool drainOnStop, bool alsoRequested)
{
    // Called at once (no lane, or on the lane itself), m_active and
    // m_dspActive move together, exactly as m_active alone did. From a
    // queued lane job m_active already holds the request (R-R3-39).
    auto setRunning = [this, alsoRequested](bool running) {
        m_dspActive.store(running);
        if (alsoRequested) {
            m_active.store(running);
        }
    };

#ifdef HAVE_WDSP
    // Task 8 (receiver and transmit gaps plan): a stopping channel is fed
    // until WDSP has finished the stop.
    //
    // SetChannelState(ch, 0, dmode) only asks for the stop. It sets
    // slew.downflag and flushflag (third_party/wdsp/src/channel.c:288-290).
    // The channel's following fexchange2 calls slew the output down, then
    // clear exchange and release the flush thread, which flushes the buffers
    // and clears flushflag (iobuffs.c:553-560, channel.c:146-166). With
    // dmode 1 SetChannelState waits for flushflag, and after 100 Sleep(1)
    // calls gives up and clears exchange, flushflag and downflag itself
    // (channel.c:291-304). So the stop completes only if I/Q keeps reaching
    // the channel through it, as it does upstream, where the receive loop
    // calls fexchange0 on every sub-receiver channel on every pass whatever
    // its state:
    //   From Thetis ChannelMaster/cmaster.c:365-366 [v2.10.3.15]
    //     for (j = 0; j < pcm->cmSubRCVR; j++)
    //         fexchange0 (chid (stream, j), pcm->in[stream], pcm->rcvr[rx].audio[j], &error);		// dsp
    // and the rate change stops its channels for that reason:
    //   From Thetis Console/setup.cs:7042 [v2.10.3.15]
    //     // turn OFF the DSP channels so they get flushed out (must do while data is flowing to get slew-down and flush)
    //
    // This channel used to mark itself inactive before the call, and
    // processIq stopped exchanging on an inactive channel, so a draining
    // stop always waited out the timeout and a no-drain stop left the flags
    // for a restart to trip over.
    if (active) {
        finishPendingStop();
        setRunning(true);
        // state=1 on, dmode=0
        SetChannelState(m_channelId, 1, 0);
    } else if (drainOnStop) {
        // Still active while WDSP drains it: processIq keeps exchanging, so
        // the slew-down and flush finish in a few blocks of input. When the
        // call returns exchange is clear, whether the flush finished or the
        // wait timed out.
        SetChannelState(m_channelId, 0, 1);
        setRunning(false);
    } else {
        // Returns at once. processIq keeps exchanging on the stopping channel
        // until WDSP reports the stop done (see processIq), and a restart
        // before then finishes the stop first (finishPendingStop).
        quint32 token = ++m_stopSerial;
        if (token == 0) {
            token = ++m_stopSerial;
        }
        m_pendingStop.store(token, std::memory_order_release);
        SetChannelState(m_channelId, 0, 0);
        setRunning(false);
    }

#else
    Q_UNUSED(drainOnStop);
    setRunning(active);
#endif
}

void RxChannel::finishPendingStop()
{
#ifdef HAVE_WDSP
    if (m_pendingStop.load(std::memory_order_acquire) == 0) {
        return;
    }
    // A no-drain stop WDSP has not reported done: no I/Q reached the channel
    // after it (a stream that stopped delivering, or a feed disconnected
    // straight after the stop). Its slew.downflag may still be set, and a
    // restart over it would slew the first block down and clear exchange,
    // leaving the channel silent while isActive() reports true (fix wave 1,
    // C1). WDSP clears that flag only in a stop it finishes or times out
    // (channel.c:291-304), and SetChannelState acts only on a change of
    // state, so switch the channel on and stop it again, draining. With I/Q
    // flowing processIq feeds that drain; without it, the wait times out
    // and WDSP clears the flags itself.
    //
    // A window this does not close (review M3). processIq counts a no-drain
    // stop done once fexchange2 stops writing, which is when the slew-down
    // clears exchange and releases the flush thread
    // (third_party/wdsp/src/iobuffs.c:558-562 [v2.10.3.15], unchanged from
    // Thetis). The flush thread runs after that: it sets exec_bypass and
    // then clears flushflag (third_party/wdsp/src/channel.c:152-162, Thetis
    // wdsp/channel.c:134-144 [v2.10.3.15]). A restart that lands between
    // the two has the exec_bypass reset of SetChannelState(ch, 1, ...)
    // (third_party/wdsp/src/channel.c:309, Thetis wdsp/channel.c:291
    // [v2.10.3.15]) undone by the flush, and the channel stays silent while
    // it reports active. It is left alone: the window is the flush thread's
    // wake-up, the only path that stops without a drain and restarts is
    // setSampleRateLive, which puts at least 40 ms of fixed waits between
    // the stop and the restart, and WDSP has the same race upstream.
    SetChannelState(m_channelId, 1, 0);
    SetChannelState(m_channelId, 0, 1);
    m_pendingStop.store(0, std::memory_order_release);
#endif
}

// ---------------------------------------------------------------------------
// Audio processing — hot path
// ---------------------------------------------------------------------------

void RxChannel::processIq(float* inI, float* inQ,
                          float* outI, float* outQ,
                          int sampleCount, int outSampleCount)
{
    // fexchange2 writes outSampleCount samples (WDSP's decimated output
    // rate) which may be smaller than sampleCount (input rate). Post-WDSP
    // processors must use the SMALLER count or they'll process zero-padded
    // tails and produce garbage. Default -1 preserves old contract.
    [[maybe_unused]] const int postCount = (outSampleCount > 0) ? outSampleCount : sampleCount;

    if (!m_wdspReady.load(std::memory_order_acquire)) {
        // R-R3-39: the lane has not opened this channel's WDSP side yet (or
        // has retired it). Silence, and no WDSP call.
        std::memset(outI, 0, sampleCount * sizeof(float));
        std::memset(outQ, 0, sampleCount * sizeof(float));
        return;
    }

    // m_dspActive: what the lane has applied (m_active with no lane).
    if (!m_dspActive.load()) {
        // Channel inactive — output silence
        std::memset(outI, 0, sampleCount * sizeof(float));
        std::memset(outQ, 0, sampleCount * sizeof(float));
#ifdef HAVE_WDSP
        // Task 8: a channel stopped without a drain is still fed until WDSP
        // reports the stop done, so its slew-down runs and the flush thread
        // clears its flags (see applyActive). fexchange2 writes both output
        // legs whenever exchange is set, and returns without touching them
        // once the slew-down has cleared it (iobuffs.c:525; channel.h:35,
        // "when 0, it just returns"). A sentinel left in place is that
        // report. The blanker and the post-DSP stages stay off: the channel
        // is no longer running.
        quint32 token = m_pendingStop.load(std::memory_order_acquire);
        if (token != 0) {
            std::memcpy(outI, &kStopSentinelBits, sizeof(float));
            int error = 0;
            fexchange2(m_channelId, inI, inQ, outI, outQ, &error);
            quint32 first = 0;
            std::memcpy(&first, outI, sizeof(float));
            if (first == kStopSentinelBits) {
                outI[0] = 0.0f;
                m_pendingStop.compare_exchange_strong(token, 0,
                                                      std::memory_order_acq_rel);
            }
        }
#endif
        return;
    }

#ifdef HAVE_WDSP
    // NB1 and NB2 process raw I/Q BEFORE the main WDSP channel.
    // They operate in-place on separate I and Q buffers.
    // From Thetis wdsp-integration.md section 4.3
    // From design doc §sub-epic B — mutually exclusive NB/NB2 via one atomic.
    //
    // Phase 3F Sub-Epic I Task 4b: skipped on co-hosted slices. Because the
    // blanker runs IN PLACE on the caller's legs, and every slice bound to a
    // DDC stream is handed the same chunk, only the stream-owning slice may
    // blank it; the rest would re-blank an already-blanked buffer. Upstream
    // holds one ANB / NOB per receiver rather than per sub-receiver
    // (ChannelMaster cmaster.h:79-81 [v2.10.3.15]).
    if (!m_nbBypassed.load(std::memory_order_acquire)) {
        switch (m_nb ? m_nb->mode() : NereusSDR::NbMode::Off) {
            case NereusSDR::NbMode::NB:  xanbEXTF(m_channelId, inI, inQ); break;
            case NereusSDR::NbMode::NB2: xnobEXTF(m_channelId, inI, inQ); break;
            case NereusSDR::NbMode::Off: /* no-op */                      break;
        }
    }

    // Main WDSP processing: demod, AGC, NR, ANF, filter, EQ, audio panel.
    int error = 0;
    fexchange2(m_channelId, inI, inQ, outI, outQ, &error);

    if (error != 0) {
        qCWarning(lcDsp) << "fexchange2 error on channel"
                         << m_channelId << ":" << error;
    }

#ifdef HAVE_DFNR
    // Sub-epic C-1 Task 9 — post-fexchange2 DeepFilterNet3 noise reduction.
    // Runs only when m_dfnrActive is set via setActiveNr(NrSlot::DFNR).
    // outI/outQ are 48 kHz stereo float at this point — DFNR's native rate.
    // R-R3-39: the flag first (acquire), then the instance (acquire); the
    // instance is published before the flag is ever set, so a set flag
    // means a fully built instance. No lock on this thread.
    if (m_dfnrActive.load(std::memory_order_acquire)) {
        if (NereusSDR::DeepFilterFilter* dfnr =
                m_dfnrInstance.load(std::memory_order_acquire)) {
            dfnr->process(outI, outQ, postCount);
        }
    }
#endif

#ifdef HAVE_MNR
    // Sub-epic C-1 Task 11 — post-fexchange2 Apple Accelerate MMSE-Wiener NR.
    // Runs only when m_mnrActive is set via setActiveNr(NrSlot::MNR).
    // outI/outQ are 48 kHz stereo float at this point.
    // Ported from AetherSDR src/core/MacNRFilter.{h,cpp} [@0cd4559]; retuned
    // for 48 kHz (LOG2N 9→10, FFT 512→1024, hop 256→512).
    if (m_mnr && m_mnrActive.load(std::memory_order_acquire)) {
        m_mnr->process(outI, outQ, postCount);
    }
#endif

    // Phase 3J-1 Task 16.2 — TCI audio tap.
    // Emit post-DSP stereo audio for any TCI clients subscribed via the
    // TciServer audio binary pipeline (Phase 16 Task 16.3). This fires
    // after fexchange2 AND all post-DSP NR stages (DFNR, MNR) so the tap
    // captures the same enhanced audio that flows to AudioEngine.
    //
    // Direct-connection only: receivers must copy into their own buffer
    // (e.g. AudioRingSpsc) before returning — outI/outQ are scratch
    // buffers owned by RxDspWorker and may be reused on the next chunk.
    //
    // srcRate: WDSP RX output rate is always 48000 Hz (set in
    // WdspEngine::createRxChannel — RadioModel.cpp:1534-1535 [v0.4.0]).
    // Phase 16 Task 16.3 (TciServer) connects this with Qt::DirectConnection.
    {
        constexpr int kWdspRxOutputRate = 48000;
        emit audioFrameReady(m_channelId, outI, outQ, postCount, kWdspRxOutputRate);
    }

#else
    // WDSP not available — output silence
    std::memset(outI, 0, sampleCount * sizeof(float));
    std::memset(outQ, 0, sampleCount * sizeof(float));
#endif
}

// ---------------------------------------------------------------------------
// Metering
// ---------------------------------------------------------------------------

double RxChannel::getMeter(RxMeterType type) const
{
    if (m_lane != nullptr && !m_lane->isCurrentThread()) {
        // R-R3-39: never a WDSP call here. One keyed refresh however many
        // readers ask, so the cache follows the fastest one.
        requestMeterRefresh();
        const int index = static_cast<int>(type);
        if (index < 0 || index >= kRxMeterTypes) {
            return -140.0;
        }
        return m_meterCache[static_cast<std::size_t>(index)].load(std::memory_order_acquire);
    }
#ifdef HAVE_WDSP
    // GetRXAMeter reads WDSP channel state that is only valid once
    // SetChannelState(channel, 1, 0) has been called. Reading before the
    // channel is active segfaults (seen after P1 connect in Phase 3I,
    // because the P1 path had not yet called setActive(true) on the
    // downstream RxChannel the MeterPoller was bound to). Guard here:
    // the contract is "no meter data until active".
    if (!m_dspActive.load() || !m_wdspReady.load(std::memory_order_acquire)) {
        return -140.0;
    }
    return GetRXAMeter(m_channelId, static_cast<int>(type));
#else
    Q_UNUSED(type);
    return -140.0;
#endif
}

void RxChannel::requestMeterRefresh() const
{
    const quint64 generation = m_meterGeneration.load(std::memory_order_acquire);
    runKeyed(laneParameter("meterRefresh"), 0,
             [this, generation]() { refreshMeterCacheOnLane(generation); });
}

bool RxChannel::meterReadingReady() const
{
    if (m_lane == nullptr || m_lane->isCurrentThread()) {
        return true;
    }
    return m_meterCacheReady.load(std::memory_order_acquire);
}

void RxChannel::refreshMeterCacheOnLane(quint64 generation) const
{
    for (int index = 0; index < kRxMeterTypes; ++index) {
        double value = -140.0;
#ifdef HAVE_WDSP
        // Same guard as getMeter: no meter read of a channel that is not
        // running (the segfault note above).
        if (m_dspActive.load() && m_wdspReady.load(std::memory_order_acquire)) {
            value = GetRXAMeter(m_channelId, index);
        }
#endif
        m_meterCache[static_cast<std::size_t>(index)].store(value, std::memory_order_release);
    }
    // Posted after the owner's last change of state, so it read that state.
    if (generation == m_meterGeneration.load(std::memory_order_acquire)) {
        m_meterCacheReady.store(true, std::memory_order_release);
    }
}

// NereusSDR-original (R-R3-40): reads the WDSP worker's load counters from
// dsplock.c. GetChannelDspLoad takes no lock, so this never waits for the
// worker.
bool RxChannel::dspLoad(DspLoadCounters& out) const
{
    out = DspLoadCounters{};
#ifdef HAVE_WDSP
    if (!wdspChannelInRange()) {
        return false;
    }
    WdspChannelLoad load{};
    // 1: the counters were read, but the busy pair may be torn (dsplock.c).
    const int result = GetChannelDspLoad(m_channelId, &load);
    if (result != 0 && result != 1) {
        return false;
    }
    out.consistent = result == 0;
    out.blocks        = load.blocks;
    out.busyNs        = load.busyNs;
    out.lateBlocks    = load.lateBlocks;
    out.maxBlockUs    = load.maxBlockUs;
    out.blockPeriodUs = load.blockPeriodUs;
    out.currentBlockNs = load.currentBlockNs;
    out.readNs        = load.readNs;
    return true;
#else
    return false;
#endif
}

qint64 RxChannel::takeDspIntervalMaxBlockUs() const
{
#ifdef HAVE_WDSP
    if (!wdspChannelInRange()) {
        return 0;
    }
    return std::max<qint64>(0, TakeChannelDspIntervalMaxBlockUs(m_channelId));
#else
    return 0;
#endif
}

// ---------------------------------------------------------------------------
// DFNR tuning setters (Sub-epic C-1, Task 9)
// ---------------------------------------------------------------------------

#ifdef HAVE_DFNR
void RxChannel::setDfnrAttenLimit(float dB)
{
    // R-R3-39: kept for an instance built later; DeepFilterFilter's setters
    // are atomic, so any thread may forward to a published instance.
    m_dfnrAttenLimit.store(dB, std::memory_order_seq_cst);
    if (NereusSDR::DeepFilterFilter* dfnr = m_dfnrInstance.load(std::memory_order_seq_cst)) {
        dfnr->setAttenLimit(dB);
    }
}

void RxChannel::setDfnrPostFilterBeta(float beta)
{
    m_dfnrPostFilterBeta.store(beta, std::memory_order_seq_cst);
    if (NereusSDR::DeepFilterFilter* dfnr = m_dfnrInstance.load(std::memory_order_seq_cst)) {
        dfnr->setPostFilterBeta(beta);
    }
}
#endif

// ---------------------------------------------------------------------------
// MNR tuning setter (Sub-epic C-1, Task 11)
// ---------------------------------------------------------------------------

#ifdef HAVE_MNR
void RxChannel::setMnrStrength(float strength)
{
    if (m_mnr) { m_mnr->setStrength(strength); }
}
void RxChannel::setMnrOversub(float oversub)
{
    if (m_mnr) { m_mnr->setOversub(oversub); }
}
void RxChannel::setMnrFloor(float floor)
{
    if (m_mnr) { m_mnr->setFloor(floor); }
}
void RxChannel::setMnrAlpha(float alpha)
{
    if (m_mnr) { m_mnr->setAlpha(alpha); }
}
void RxChannel::setMnrBias(float bias)
{
    if (m_mnr) { m_mnr->setBias(bias); }
}
void RxChannel::setMnrGsmooth(float gsmooth)
{
    if (m_mnr) { m_mnr->setGsmooth(gsmooth); }
}
#else
void RxChannel::setMnrStrength(float) {}
void RxChannel::setMnrOversub(float) {}
void RxChannel::setMnrFloor(float) {}
void RxChannel::setMnrAlpha(float) {}
void RxChannel::setMnrBias(float) {}
void RxChannel::setMnrGsmooth(float) {}
#endif

// ---------------------------------------------------------------------------
// Filter convenience setters — single-axis carry setters (Task 1.2)
// Store int mirror values and sync the double carries; do NOT call WDSP.
// These are used by callers (e.g. SliceModel filter-preset machinery) that
// need to update one edge without triggering a WDSP push. A subsequent call
// to setFilterFreqs() will push both edges together in one WDSP call.
// applyState() uses setFilterFreqs() directly — not these setters.
// ---------------------------------------------------------------------------

void RxChannel::setFilterLow(int lowHz)
{
    m_filterLowInt = lowHz;
    m_filterLow = static_cast<double>(lowHz);  // keep double carry in sync
}

void RxChannel::setFilterHigh(int highHz)
{
    m_filterHighInt = highHz;
    m_filterHigh = static_cast<double>(highHz);  // keep double carry in sync
}

// ---------------------------------------------------------------------------
// EQ carry setters (Task 1.2 — no WDSP wiring yet)
// Carry-only for state preservation; WDSP SetRXAGrphEQ wiring in EQ task.
// ---------------------------------------------------------------------------

void RxChannel::setEqEnabled(bool enabled)
{
    m_eqEnabled = enabled;
}

void RxChannel::setEqPreamp(int preampDb)
{
    m_eqPreampDb = preampDb;
}

void RxChannel::setEqBand(int bandIndex, int gainDb)
{
    if (bandIndex >= 0 && bandIndex < 10) {
        m_eqBandsDb[bandIndex] = gainDb;
    }
}

// ---------------------------------------------------------------------------
// Squelch unified carry setters (Task 1.2)
// Carry-only; per-mode SSQL/AMSQ/FMSQ are still the primary API.
// ---------------------------------------------------------------------------

void RxChannel::setSquelchEnabled(bool enabled)
{
    m_squelchEnabled = enabled;
}

void RxChannel::setSquelchThreshold(int thresholdDb)
{
    m_squelchThresholdDb = thresholdDb;
}

// ---------------------------------------------------------------------------
// RIT offset carry setter (Task 1.2 — no WDSP wiring yet)
// ---------------------------------------------------------------------------

void RxChannel::setRitOffset(int ritHz)
{
    m_ritOffsetHz = ritHz;
}

// ---------------------------------------------------------------------------
// Antenna index carry setter (Task 1.2 — routed via AlexController)
// ---------------------------------------------------------------------------

void RxChannel::setAntennaIndex(int index)
{
    m_antennaIndex = index;
}

// ---------------------------------------------------------------------------
// Shift offset convenience alias (Task 1.2)
// Delegates to setShiftFrequency and keeps the carry field in sync.
// ---------------------------------------------------------------------------

void RxChannel::setShiftOffset(double offsetHz)
{
    // setShiftFrequency now maintains m_shiftOffsetHz and has the WDSP call.
    setShiftFrequency(offsetHz);
}

// ---------------------------------------------------------------------------
// NB enabled carry setter (Task 1.2)
// Carry-only bool; NbFamily::setMode is the primary API.
// ---------------------------------------------------------------------------

void RxChannel::setNbEnabled(bool enabled)
{
    m_nbEnabled = enabled;
}

// ---------------------------------------------------------------------------
// NR mode carry setter (Task 1.2)
// Carry-only int; setActiveNr(NrSlot) is the primary API.
// ---------------------------------------------------------------------------

void RxChannel::setNrMode(int nrMode)
{
    m_nrMode = nrMode;
}

// ---------------------------------------------------------------------------
// State snapshot / restore (Task 1.2)
// captureState() reads all DSP state into a portable RxChannelState struct.
// applyState() restores from that struct by calling all individual setters.
// ---------------------------------------------------------------------------

RxChannelState RxChannel::captureState() const
{
    RxChannelState s;

    s.mode                = static_cast<SliceModel::Mode>(m_mode.load());
    s.filterLowHz         = m_filterLowInt;
    s.filterHighHz        = m_filterHighInt;

    // AGC
    s.agcMode             = m_agcMode.load();
    s.agcAttackMs         = m_agcAttack.load();
    s.agcDecayMs          = m_agcDecay.load();
    s.agcHangMs           = m_agcHang.load();
    s.agcSlope            = m_agcSlope.load();
    s.agcMaxGainDb        = m_agcMaxGain.load();
    s.agcFixedGainDb      = m_agcFixedGain.load();
    s.agcHangThresholdPct = m_agcHangThreshold.load();

    // Noise blanker
    s.nbEnabled           = m_nbEnabled;
    s.nbMode              = static_cast<int>(nbMode());

    // Noise reduction
    s.nrEnabled           = m_nrEnabled.load();
    s.nrMode              = m_nrMode;
    s.activeNr            = activeNr();
    s.nnrTuning           = nnrTuning();
    s.nnrLimit            = nnrLimit();
    s.anfEnabled          = m_anfEnabled.load();

    // EQ
    s.eqEnabled           = m_eqEnabled;
    s.eqPreampDb          = m_eqPreampDb;
    for (int i = 0; i < 10; ++i) {
        s.eqBandsDb[i] = m_eqBandsDb[i];
    }

    // Squelch
    s.squelchEnabled      = m_squelchEnabled;
    s.squelchThresholdDb  = m_squelchThresholdDb;

    // RIT, antenna, shift offset
    s.ritOffsetHz         = m_ritOffsetHz;
    s.antennaIndex        = m_antennaIndex;
    s.shiftOffsetHz       = m_shiftOffsetHz;

    return s;
}

void RxChannel::applyState(const RxChannelState& s)
{
    setMode(s.mode);
    // setFilterFreqs is the canonical live-apply path: pushes both edges to
    // WDSP in one call and emits filterChanged. The carry-only setFilterLow/
    // setFilterHigh setters are NOT called here — calling them first would
    // sync m_filterLow/m_filterHigh to the new values, causing setFilterFreqs
    // to hit its equality guard and early-return without touching WDSP.
    // setFilterFreqs() also updates m_filterLowInt/m_filterHighInt, so
    // captureState() after applyState() sees consistent int carry values.
    setFilterFreqs(static_cast<double>(s.filterLowHz),
                   static_cast<double>(s.filterHighHz));

    // AGC
    setAgcMode(static_cast<AGCMode>(s.agcMode));
    setAgcAttack(s.agcAttackMs);
    setAgcDecay(s.agcDecayMs);
    setAgcHang(s.agcHangMs);
    setAgcSlope(s.agcSlope);
    setAgcMaxGain(s.agcMaxGainDb);
    setAgcFixedGain(s.agcFixedGainDb);
    setAgcHangThreshold(s.agcHangThresholdPct);

    // Noise blanker
    setNbEnabled(s.nbEnabled);
    setNbMode(static_cast<NbMode>(s.nbMode));

    // Noise reduction
    setNrEnabled(s.nrEnabled);
    setNrMode(s.nrMode);
    requestNnrLimit(s.nnrLimit);   // before the tuning, which applies it
    setNnrTuning(s.nnrTuning);
    setActiveNr(s.activeNr);
    setAnfEnabled(s.anfEnabled);

    // EQ
    setEqEnabled(s.eqEnabled);
    setEqPreamp(s.eqPreampDb);
    for (int i = 0; i < 10; ++i) {
        setEqBand(i, s.eqBandsDb[i]);
    }

    // Squelch
    setSquelchEnabled(s.squelchEnabled);
    setSquelchThreshold(s.squelchThresholdDb);

    // RIT, antenna, shift offset
    setRitOffset(s.ritOffsetHz);
    setAntennaIndex(s.antennaIndex);
    setShiftOffset(s.shiftOffsetHz);
}

// ---------------------------------------------------------------------------
// In-place RX filter resize / filter type change
// ---------------------------------------------------------------------------
//
// These two setters wrap the WDSP entry points that Thetis calls from its
// DSPRX property setters at radio.cs:542-574 [v2.10.3.15]:
//
//   public int FilterSize {
//       set {
//           filter_size = value;
//           if (update) {
//               if (value != filter_size_dsp || force) {
//                   WDSP.RXASetNC(WDSP.id(thread, subrx), value);
//                   filter_size_dsp = value;
//               }
//           }
//       }
//   }
//   public DSPFilterType FilterType {
//       set {
//           filter_type = value;
//           if (update) {
//               if (value != filter_type_dsp || force) {
//                   WDSP.RXASetMP(WDSP.id(thread, subrx), Convert.ToBoolean(value));
//                   filter_type_dsp = value;
//               }
//           }
//       }
//   }
//
// RXASetNC and RXASetMP at Thetis wdsp/RXA.c:1043-1066 [v2.10.3.15]
// internally quiesce the channel via SetChannelState(channel, 0, 1) — the
// cm_main flushflag handshake at channel.c:259-297 [v2.10.3.13] — reconfigure
// every dependent subsystem, then restore the prior run state.  Safe to call
// from the main thread while the WDSP worker is alive.

void RxChannel::setDspBufferSizeSamples(int size)
{
    if (size <= 0) {
        return;
    }
    // Thetis invariant (console.cs:38911 [v2.10.3.13]):
    //   if (filtsize < bufsize) bufsize = filtsize;
    // Equivalent to: buffer must never exceed filter.  WDSP fircore relies
    // on this — nfor = nc/size in firmin.c:135 [v2.10.3.13] gives 0 when
    // nc < size, leading to null FFTW plans and SIGSEGV in the audio
    // thread.  Mirror Thetis: silently clamp buffer down to filter.
    if (size > m_filterSize) {
        qCWarning(lcDsp) << "RxChannel::setDspBufferSizeSamples: requested size="
                          << size << "exceeds current filter size=" << m_filterSize
                          << "— clamping to filter (Thetis console.cs:38911 invariant).";
        size = m_filterSize;
    }
    if (size == m_dspBlockSize) {
        return;
    }
    m_dspBlockSize = size;
#ifdef HAVE_WDSP
    // From Thetis radio.cs:521 [v2.10.3.13] DSPRX.BufferSize setter:
    //   WDSP.SetDSPBuffsize(WDSP.id(thread, subrx), value);
    // SetDSPBuffsize at channel.c:181 [v2.10.3.13] internally quiesces via
    // SetChannelState's flushflag handshake, then runs a full DSP destroy
    // + rebuild with the new dsp_size.  Heavier than RXASetNC but still
    // safe to call from main thread while audio worker is alive.
    // R-R3-39: on the receive lane, in order with the filter size.
    runOrdered([this, size]() { SetDSPBuffsize(m_channelId, size); });
#endif
}

void RxChannel::setFilterSizeSamples(int nc)
{
    if (nc <= 0 || nc == m_filterSize) {
        return;
    }
    // Thetis invariant (console.cs:38911 [v2.10.3.13]): filter >= buffer.
    // If new filter is smaller than the current DSP block size, shrink
    // the buffer FIRST so the WDSP fircore precondition (nc >= size,
    // firmin.c:135 [v2.10.3.13]) is satisfied when RXASetNC runs.  Order
    // mirrors Thetis UpdateDSP: BufferSize setter (radio.cs:521) is
    // called before FilterSize setter (radio.cs:540) at console.cs:38918+.
    const bool shrinkBuffer = nc < m_dspBlockSize;
    if (shrinkBuffer) {
        m_dspBlockSize = nc;
    }
    m_filterSize = nc;
    // R-R3-39: the carries above change at once; the WDSP calls run on the
    // receive lane, in this order.
    runOrdered([this, nc, shrinkBuffer]() {
#ifdef HAVE_WDSP
        if (shrinkBuffer) {
            // From Thetis radio.cs:521 [v2.10.3.13] DSPRX.BufferSize setter.
            SetDSPBuffsize(m_channelId, nc);
        }
        // From Thetis radio.cs:540 [v2.10.3.13] DSPRX.FilterSize setter.
        RXASetNC(m_channelId, nc);
#else
        Q_UNUSED(nc);
        Q_UNUSED(shrinkBuffer);
#endif
        // RXASetNC reaches nbp0 through RXANBPSetNC (third_party/wdsp/src/RXA.c:1043),
        // and min_notch_width divides by nc (nbp.c:82-96), so the narrowest
        // realisable notch just moved. Thetis re-reads it at exactly this point
        // in its own DSP-options apply path (console.cs:39052-39053 ->
        // UpdateMinimumNotchWidthRX, :48787-48818 [v2.10.3.15]).
        refreshMinNotchWidthOnLane();
    });
}

void RxChannel::setFilterTypeLinearPhase(bool linearPhase)
{
    const int newType = linearPhase ? 1 : 0;
    if (newType == m_filterType) {
        return;
    }
    m_filterType = newType;
#ifdef HAVE_WDSP
    // From Thetis radio.cs:571 [v2.10.3.15] DSPRX.FilterType setter:
    //   WDSP.RXASetMP(WDSP.id(thread, subrx), Convert.ToBoolean(value));
    // with enums.cs:404-408 [v2.10.3.15]
    //   public enum DSPFilterType { Linear_Phase = 0, Low_Latency = 1, }
    // so Low_Latency sends minimum phase (MP 1) and Linear_Phase MP 0.
    // m_filterType counts the other way (0 = Low Latency), so the MP flag
    // is its inverse (R-IOS-13, 2026-09-27: it used to be sent as is).
    // R-R3-39: on the receive lane, in order with the sizes.
    const int minimumPhase = linearPhase ? 0 : 1;
    runOrdered([this, minimumPhase]() { RXASetMP(m_channelId, minimumPhase); });
#endif
}

// ---------------------------------------------------------------------------
// Channel rebuild (Task 1.3)
//
// LEGACY heavy-rebuild path retained for sample-rate live-apply where a
// full close-and-reopen may be required.  NOT USED for filter size / filter
// type changes — those go through setFilterSizeSamples / setFilterTypeLinearPhase
// above which use the in-place WDSP entry points (mirrors Thetis radio.cs:540
// + 559 [v2.10.3.13]).
// ---------------------------------------------------------------------------

qint64 RxChannel::rebuild(WdspEngine& engine, const ChannelConfig& cfg)
{
    return engine.rebuildRxChannel(m_channelId, cfg);
}

// ---------------------------------------------------------------------------
// Per-mode DSP-Options live-apply (Task 4.2)
// ---------------------------------------------------------------------------
//
// Reads the per-mode AppSettings keys for newMode and calls rebuild() if any
// value differs from the current channel config (m_bufferSize, m_filterSize,
// m_filterType). The cacheImpulse / highResFilterCharacteristics keys are
// global (not per-mode) and are also forwarded to ChannelConfig.
//
// Returns elapsed ms if rebuild occurred, 0 if nothing changed, -1 if no
// engine is attached or the channel is not in the engine.
//
// NereusSDR-original — no Thetis source ported; the per-mode key naming
// mirrors the DspOptionsPage AppSettings keys (design Section 4B).

// Maps DSPMode to the DspOptions key suffix used in AppSettings.
// From design Section 4B: Phone covers SSB/AM/SAM/DSB, CW covers
// CWU/CWL, Dig covers DIGU/DIGL/SPEC/DRM, FM covers FM.
//
// NereusSDR-original helper, no Thetis source ported. Declared in
// RxChannel.h so RadioModel and DspOptionsPage share this one mapping.
QString dspOptionsModeGroup(DSPMode mode)
{
    switch (mode) {
        case DSPMode::USB:
        case DSPMode::LSB:
        case DSPMode::AM:
        case DSPMode::SAM:
        case DSPMode::DSB:
            return QStringLiteral("Phone");
        case DSPMode::CWU:
        case DSPMode::CWL:
            return QStringLiteral("Cw");
        case DSPMode::DIGU:
        case DSPMode::DIGL:
        case DSPMode::SPEC:
        case DSPMode::DRM:
            return QStringLiteral("Dig");
        case DSPMode::FM:
            return QStringLiteral("Fm");
        default:
            return QStringLiteral("Phone");
    }
}

qint64 RxChannel::onModeChanged(DSPMode newMode)
{
    // Engine guard: no engine attached → return 0 (no rebuild possible).
    // Matches the function-header contract block and
    // tst_dsp_options_per_mode_apply.cpp::rx_no_engine_returns_zero.
    if (!m_wdspEngine) {
        return 0;
    }

    auto& s = AppSettings::instance();
    const QString modeKey = dspOptionsModeGroup(newMode);

    // Read per-mode RX-side DSP settings — Thetis-faithful split keys
    // post schema-v5 (radio.cs:519-574 [v2.10.3.13] DSPRX persists
    // BufferSize, FilterSize, and FilterType independently from DSPTX).
    const int newBufSize   =
        s.value(QStringLiteral("DspOptionsBufferSize") + modeKey + QStringLiteral("Rx"),
                64).toInt();

    const int newFiltSize  =
        s.value(QStringLiteral("DspOptionsFilterSize") + modeKey + QStringLiteral("Rx"),
                4096).toInt();

    const QString typeKey  =
        QStringLiteral("DspOptionsFilterType") + modeKey + QStringLiteral("Rx");
    const QString typeStr  = s.value(typeKey, QStringLiteral("Low Latency")).toString();
    const int newFiltType  = (typeStr == QStringLiteral("Low Latency")) ? 0 : 1;

    // No-change check: settings already match channel state → return 0
    // (no rebuild).  Matches the function-header contract block and
    // tst_dsp_options_per_mode_apply.cpp::rx_same_settings_returns_zero_no_rebuild.
    // RadioModel.cpp:3754 gates dspChangeMeasured on `elapsed > 0` so a
    // 0-return here doesn't emit a spurious "0 ms applied" UI update.
    if (newBufSize == m_dspBlockSize && newFiltSize == m_filterSize &&
        newFiltType == m_filterType) {
        return 0;
    }

    // Channel-in-map guard: settings differ → rebuild is required, but
    // the channel must exist in the engine's map first.  Without this,
    // the in-place WDSP setters below would dereference ch[channelId]
    // past MAX_CHANNELS=32 (UB; SIGBUS on macOS arm64 strict-alignment;
    // silent corruption elsewhere).  Matches the contract block and
    // tst_dsp_options_per_mode_apply.cpp::rx_engine_attached_channel_not_
    // in_map_returns_minus_one + the changed_filter_size /
    // changed_filter_type "rebuild attempt" tests.
    if (!m_wdspEngine->rxChannel(m_channelId)) {
        return -1;
    }

    qCInfo(lcDsp) << "RxChannel::onModeChanged: mode=" << static_cast<int>(newMode)
                  << "key=" << modeKey
                  << "bufSize:" << m_dspBlockSize << "->" << newBufSize
                  << "filtSize:" << m_filterSize << "->" << newFiltSize
                  << "filtType:" << m_filterType << "->" << newFiltType;

    // Apply order matters — Thetis pushes BufferSize first, FilterSize
    // second (UpdateDSP at console.cs:38918+ [v2.10.3.13]).  Each setter
    // internally enforces the filter >= buffer invariant (radio.cs:521 +
    // 540 [v2.10.3.13] respectively) so out-of-order user input still
    // produces a valid WDSP state.  Each WDSP entry point quiesces via
    // SetChannelState's flushflag handshake — safe to call from the main
    // thread while audio worker is alive.
    if (m_lane != nullptr && !m_lane->isCurrentThread()) {
        // R-R3-39: the setters below carry the new sizes at once and queue
        // their WDSP calls; the lane times them from the first to the last
        // and reports it through dspOptionsApplied.
        auto timer = std::make_shared<QElapsedTimer>();
        runOrdered([timer]() { timer->start(); });
        setFilterSizeSamples(newFiltSize);
        setDspBufferSizeSamples(newBufSize);
        setFilterTypeLinearPhase(newFiltType == 1);
        runOrdered([this, timer]() { emit dspOptionsApplied(timer->elapsed()); });
        return 0;
    }

    QElapsedTimer t;
    t.start();
    // setFilterSizeSamples cascades a buffer shrink internally when filter
    // < dsp_size, so apply filter BEFORE buffer.  If we did it the Thetis
    // order (buffer first), a smaller filter coming later would still
    // trigger its own internal buffer shrink — same end state, but the
    // filter-first path avoids one redundant SetDSPBuffsize call.  When
    // buffer is going LARGER (or equal), the filter setter doesn't
    // cascade and the explicit buffer setter does the work.
    setFilterSizeSamples(newFiltSize);
    setDspBufferSizeSamples(newBufSize);
    setFilterTypeLinearPhase(newFiltType == 1);
    return t.elapsed();
}

// ---------------------------------------------------------------------------
// Filter frequency response (Task 1.5)
// ---------------------------------------------------------------------------
//
// NereusSDR-original — no Thetis source ported; algorithm is generic FFT-of-
// filter-taps.  Uses WDSP fir_bandpass() because it is the exact function
// that WDSP's CalcBandpassFilter() calls internally, so the synthesized taps
// match the filter WDSP is actually running.  The taps are zero-padded into a
// double-precision FFTW3 FFT of size fftSize, then the positive half-spectrum
// magnitude is decimated to nPoints output samples.
//
// Approach: Option B (synthesized via fir_bandpass).
// Fallback:  returns empty QVector when HAVE_WDSP or HAVE_FFTW3 absent.

QVector<float> RxChannel::filterResponseMagnitudes(int nPoints) const
{
    if (nPoints <= 0) {
        return {};
    }
    // Parity Task 16: the bins, then the graph's resampling, so a remote
    // window drawing a Core's bins draws what this channel would.
    return resampleFilterResponse(filterResponseBins(), nPoints);
}

QVector<double> RxChannel::filterResponseBins(double* stepHz) const
{
    if (stepHz) {
        *stepHz = 0.0;
    }
#if defined(HAVE_WDSP) && defined(HAVE_FFTW3)
    // Number of FIR taps.  The default WDSP BANDPASS uses nc = 1025 taps
    // (a power-of-two-plus-one) with wintype=1 (7-term Blackman-Harris) and
    // rtype=0 (real output) at gain=1.  We replicate those choices here.
    // WDSP fir_bandpass() normalises frequencies in [0, 0.5) relative to
    // samplerate (it computes ft = (f_high - f_low) / (2.0 * samplerate)).
    static constexpr int kTapCount = 1025;  // nc default for BANDPASS struct

    // f_low and f_high in Hz (signed; can be negative for LSB).
    const double fLow  = m_filterLow;
    const double fHigh = m_filterHigh;
    const double sr    = static_cast<double>(m_sampleRate.load());

    // Synthesise filter taps using the exact WDSP function.
    // rtype=0 → real coefficients (N doubles), not complex pairs.
    // scale=1.0 (no gain correction needed here; we normalise the response).
    // The return pointer is owned by WDSP's impulse cache — do NOT free it.
    const double* taps = fir_bandpass(kTapCount, fLow, fHigh, sr,
                                      /*wintype=*/ 1,
                                      /*rtype=*/   0,
                                      /*scale=*/   1.0);
    if (!taps) {
        return {};
    }

    // Zero-pad taps into a double-precision FFTW3 buffer.
    // FFT size must be >= kTapCount; use next power-of-two >= 4096 to ensure
    // sufficient frequency resolution for the display (≥ 4 Hz/bin at 48 kHz).
    // A 4096-point FFT gives 48000 / 4096 ≈ 11.7 Hz/bin.
    constexpr int kFftSize = kFilterResponseFftSize;
    static_assert(kFftSize >= kTapCount, "FFT size must exceed tap count");

    // Allocate aligned FFTW3 buffers.
    fftw_complex* in  = fftw_alloc_complex(kFftSize);
    fftw_complex* out = fftw_alloc_complex(kFftSize);
    if (!in || !out) {
        if (in)  { fftw_free(in);  }
        if (out) { fftw_free(out); }
        return {};
    }

    // Zero-fill, then copy real taps into the real part of the input buffer.
    for (int i = 0; i < kFftSize; ++i) {
        in[i][0] = 0.0;
        in[i][1] = 0.0;
    }
    for (int i = 0; i < kTapCount; ++i) {
        in[i][0] = taps[i];
    }

    // Use FFTW_ESTIMATE to avoid touching the global FFTW wisdom/mutex.
    fftw_plan plan = fftw_plan_dft_1d(kFftSize, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
    if (!plan) {
        fftw_free(in);
        fftw_free(out);
        return {};
    }

    fftw_execute(plan);
    fftw_destroy_plan(plan);

    // Magnitude for the positive half-spectrum [0, sampleRate/2].
    // out[0..kFftSize/2] covers DC to Nyquist — kFftSize/2 + 1 unique bins.
    const int halfSize = kFftSize / 2 + 1;  // DC + positive frequencies
    QVector<double> bins(halfSize);
    for (int j = 0; j < halfSize; ++j) {
        const double re = out[j][0];
        const double im = out[j][1];
        bins[j] = std::sqrt(re * re + im * im);
    }

    fftw_free(in);
    fftw_free(out);

    if (stepHz) {
        *stepHz = sr / static_cast<double>(kFftSize);
    }
    return bins;

#else
    // WDSP or FFTW3 not available: return an empty vector so callers can
    // gracefully skip high-resolution rendering.
    return {};
#endif
}

QVector<float> RxChannel::resampleFilterResponse(const QVector<double>& binMagnitudes,
                                                 int nPoints)
{
    const int halfSize = static_cast<int>(binMagnitudes.size());
    if (nPoints <= 0 || halfSize <= 0) {
        return {};
    }

    // Decimate to nPoints output samples by linear interpolation over the
    // positive half-spectrum.  Index j in [0, halfSize-1] maps to frequency
    // j * (sampleRate/2) / (halfSize - 1).  We want nPoints uniformly
    // spaced samples of the magnitude from 0 Hz to sampleRate/2.
    QVector<float> result(nPoints);

    const double floatHalfSize = static_cast<double>(halfSize - 1);
    const double floatNPoints  = static_cast<double>(nPoints - 1 > 0 ? nPoints - 1 : 1);

    // Find peak magnitude for normalisation (reference = peak = 0 dB).
    double peakMag = 0.0;
    for (int j = 0; j < halfSize; ++j) {
        if (binMagnitudes[j] > peakMag) {
            peakMag = binMagnitudes[j];
        }
    }
    if (peakMag < 1e-30) {
        peakMag = 1e-30;  // Guard against all-zero filter (no crash)
    }

    for (int i = 0; i < nPoints; ++i) {
        // Map output sample index i → fractional bin index in [0, halfSize-1].
        const double fracIdx = static_cast<double>(i) / floatNPoints * floatHalfSize;
        const int    idx0    = static_cast<int>(fracIdx);
        const int    idx1    = (idx0 + 1 < halfSize) ? idx0 + 1 : idx0;
        const double frac    = fracIdx - static_cast<double>(idx0);

        // Linear-interpolate magnitude (not dB) for smooth curve.
        const double mag = binMagnitudes[idx0] * (1.0 - frac) + binMagnitudes[idx1] * frac;

        // Convert to dB, normalised to peak (0 dB = passband).
        // Clamp at -120 dB to avoid -inf in dead stopband.
        const double magDb = 20.0 * std::log10(mag / peakMag);
        result[i] = static_cast<float>(std::max(magDb, -120.0));
    }

    return result;
}

#ifdef NEREUS_BUILD_TESTS
// R-IOS-13: rxa[] is read in TxChannel.cpp, the one file that includes
// WDSP's internal headers (their min/max macros break this file).
int wdspRxBandpassMinimumPhaseForTest(int channelId);
int RxChannel::bandpassMinimumPhaseForTest() const
{
    return wdspRxBandpassMinimumPhaseForTest(m_channelId);
}
#endif

} // namespace NereusSDR
