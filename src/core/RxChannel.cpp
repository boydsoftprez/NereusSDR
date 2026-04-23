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
#include "LogCategories.h"
#include "NbFamily.h"
#include "wdsp_api.h"

#ifdef HAVE_DFNR
#include "DeepFilterFilter.h"
#endif

#ifdef HAVE_MNR
#include "MacNRFilter.h"
#endif

#include <cmath>

namespace NereusSDR {

RxChannel::RxChannel(int channelId, int bufferSize, int sampleRate,
                     QObject* parent)
    : QObject(parent)
    , m_channelId(channelId)
    , m_bufferSize(bufferSize)
    , m_sampleRate(sampleRate)
{
#ifdef HAVE_WDSP
    // From design doc §sub-epic B — one NbFamily per WDSP channel.
    m_nb = std::make_unique<NereusSDR::NbFamily>(
        m_channelId,
        /*sampleRate=*/ m_sampleRate,
        /*bufferSize=*/ m_bufferSize);
#endif

#ifdef HAVE_DFNR
    // Sub-epic C-1 Task 9 — DeepFilterNet3 post-WDSP noise reduction.
    // Instantiate unconditionally; the filter self-disables if the model
    // tarball is not found (isValid() returns false).
    m_dfnr = std::make_unique<NereusSDR::DeepFilterFilter>();
    if (!m_dfnr->isValid()) {
        qCWarning(lcDsp) << "DFNR not available on channel" << m_channelId
                         << "(model not found or df_create failed)";
        m_dfnr.reset();
    }
#endif

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
    // From Thetis wdsp-integration.md section 4.2
    SetRXAMode(m_channelId, val);
#endif

    emit modeChanged(mode);
}

// ---------------------------------------------------------------------------
// Bandpass filter
// ---------------------------------------------------------------------------

void RxChannel::setFilterFreqs(double lowHz, double highHz)
{
    if (m_filterLow == lowHz && m_filterHigh == highHz) {
        return;
    }

    m_filterLow = lowHz;
    m_filterHigh = highHz;

#ifdef HAVE_WDSP
    // From Thetis rxa.cs:110-111, radio.cs:603-604 — both bp1 and nbp0
    // filters must be updated together. SetRXABandpassFreqs only touches
    // bp1, which runs only when AMD/SNBA/EMNR/ANF/ANR is enabled.
    // RXANBPSetFreqs touches nbp0, the filter that runs unconditionally
    // in the SSB/CW/AM audio path. Calling only one leaves the SSB
    // bandpass stuck at nbp0's create-time default of -4150..-150
    // (LSB-shaped), which silently breaks USB, AM, and FM demod.
    SetRXABandpassFreqs(m_channelId, lowHz, highHz);
    RXANBPSetFreqs(m_channelId, lowHz, highHz);
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
    SetRXAAGCMode(m_channelId, val);
#endif

    emit agcModeChanged(mode);
}

void RxChannel::setAgcTop(double topdB)
{
#ifdef HAVE_WDSP
    SetRXAAGCTop(m_channelId, topdB);
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
#ifdef HAVE_WDSP
    // Read resulting threshold after SetRXAAGCTop modified it.
    // From Thetis console.cs:50350 pattern — GetRXAAGCThresh after SetRXAAGCTop
    // Upstream inline attribution preserved verbatim (console.cs:50345):
    //   if (agc_thresh_point < -160.0) agc_thresh_point = -160.0; //[2.10.3.6]MW0LGE changed from -143
    // kDspSize must match the size passed to SetRXAAGCThresh (4096).
    static constexpr double kDspSize = 4096.0;
    double thresh = 0.0;
    GetRXAAGCThresh(m_channelId, &thresh, kDspSize, static_cast<double>(m_sampleRate));
    return std::clamp(thresh, -160.0, 0.0);
#else
    return -20.0;
#endif
}

void RxChannel::setAgcThreshold(int dBu)
{
    if (dBu == m_agcThreshold.load()) {
        return;
    }

    m_agcThreshold.store(dBu);

#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/Console/console.cs:45976-45977
    //   size = (double)specRX.GetSpecRX(0).FFTSize;  // 4096
    //   WDSP.SetRXAAGCThresh(WDSP.id(0, 0), agc_thresh_point, size, sample_rate_rx1);
    // WDSP third_party/wdsp/src/wcpAGC.c:504
    // NB: 'size' is the DSP analysis buffer size (4096, matching OpenChannel dsp_size),
    //     NOT the fexchange2 input chunk size (m_bufferSize).
    static constexpr double kDspSize = 4096.0;
    SetRXAAGCThresh(m_channelId, static_cast<double>(dBu),
                    kDspSize,
                    static_cast<double>(m_sampleRate));
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
    // From Thetis Project Files/Source/Console/radio.cs:1056-1073
    //   WDSP.SetRXAAGCHang(WDSP.id(thread, subrx), value)
    // WDSP third_party/wdsp/src/wcpAGC.c:436
    SetRXAAGCHang(m_channelId, ms);
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
    // From Thetis Project Files/Source/Console/radio.cs:1107-1124
    //   WDSP.SetRXAAGCSlope(WDSP.id(thread, subrx), value)
    // WDSP third_party/wdsp/src/wcpAGC.c:537
    SetRXAAGCSlope(m_channelId, slope);
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
    // From Thetis Project Files/Source/Console/dsp.cs:116-117
    //   SetRXAAGCAttack declared; no explicit radio.cs call site (disabled in UI)
    // WDSP third_party/wdsp/src/wcpAGC.c:418
    SetRXAAGCAttack(m_channelId, ms);
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
    // From Thetis Project Files/Source/Console/radio.cs:1037-1054
    //   WDSP.SetRXAAGCDecay(WDSP.id(thread, subrx), value)
    // WDSP third_party/wdsp/src/wcpAGC.c:427
    SetRXAAGCDecay(m_channelId, ms);
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
    // From Thetis v2.10.3.13 setup.cs:9081
    //   WDSP.SetRXAAGCHangThreshold(WDSP.id(0, 0), value)
    // WDSP third_party/wdsp/src/wcpAGC.c
    SetRXAAGCHangThreshold(m_channelId, val);
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
    // From Thetis v2.10.3.13 setup.cs:9001
    //   WDSP.SetRXAAGCFixed(WDSP.id(0, 0), value)
    // WDSP third_party/wdsp/src/wcpAGC.c
    SetRXAAGCFixed(m_channelId, static_cast<double>(dB));
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
    // From Thetis v2.10.3.13 setup.cs:9011
    //   WDSP.SetRXAAGCTop(WDSP.id(0, 0), (double)value)
    // WDSP third_party/wdsp/src/wcpAGC.c
    SetRXAAGCTop(m_channelId, static_cast<double>(dB));
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

// Per-slice NB tuning pass-through (setNbTuning / nbTuning / setNbThreshold
// / setNbLagMs / setNbLeadMs / setNbTransitionMs) removed 2026-04-22. NB
// tuning is global per-channel now; Setup → DSP → NB/SNB calls WDSP
// SetEXTANB* directly on channel 0. See DspSetupPages.cpp.

// ---------------------------------------------------------------------------
// Noise reduction
// ---------------------------------------------------------------------------

void RxChannel::setNrEnabled(bool enabled)
{
    m_nrEnabled.store(enabled);

#ifdef HAVE_WDSP
    SetRXAANRRun(m_channelId, enabled ? 1 : 0);
#endif
}

void RxChannel::setAnfEnabled(bool enabled)
{
    m_anfEnabled.store(enabled);

#ifdef HAVE_WDSP
    SetRXAANFRun(m_channelId, enabled ? 1 : 0);
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
    // From Thetis Project Files/Source/Console/radio.cs:2216-2232
    //   WDSP.SetRXAEMNRRun(WDSP.id(thread, subrx), value)
    // WDSP third_party/wdsp/src/emnr.c:1283
    SetRXAEMNRRun(m_channelId, enabled ? 1 : 0);
#else
    Q_UNUSED(enabled);
#endif
}

void RxChannel::setEmnrGainMethod(int method)
{
#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/Console/radio.cs:2062-2078
    //   WDSP.SetRXAEMNRgainMethod(WDSP.id(thread, subrx), value)
    // WDSP third_party/wdsp/src/emnr.c:1298
    SetRXAEMNRgainMethod(m_channelId, method);
#else
    Q_UNUSED(method);
#endif
}

void RxChannel::setEmnrNpeMethod(int method)
{
#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/Console/radio.cs:2081-2097
    //   WDSP.SetRXAEMNRnpeMethod(WDSP.id(thread, subrx), value)
    // WDSP third_party/wdsp/src/emnr.c:1306
    SetRXAEMNRnpeMethod(m_channelId, method);
#else
    Q_UNUSED(method);
#endif
}

void RxChannel::setEmnrAeRun(bool run)
{
#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/Console/radio.cs:2101-2117
    //   WDSP.SetRXAEMNRaeRun(WDSP.id(thread, subrx), value)
    // WDSP third_party/wdsp/src/emnr.c:1314
    SetRXAEMNRaeRun(m_channelId, run ? 1 : 0);
#else
    Q_UNUSED(run);
#endif
}

void RxChannel::setEmnrPosition(int position)
{
#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/Console/radio.cs:2235-2251
    //   WDSP.SetRXAEMNRPosition(WDSP.id(thread, subrx), value)
    // WDSP third_party/wdsp/src/emnr.c:1322
    // position=1 → post-AGC placement (Thetis default rx_nr2_position=1)
    SetRXAEMNRPosition(m_channelId, position);
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
    // From Thetis radio.cs:681-698 [v2.10.3.13] — SetNRVals() calls
    // WDSP.SetRXAANRVals(id, taps, delay, gain, leak) with already-scaled values.
    SetRXAANRVals(m_channelId, t.taps, t.delay, t.gain, t.leakage);
    SetRXAANRPosition(m_channelId, static_cast<int>(t.position));
#endif
}

void RxChannel::setAnrTaps(int taps)
{
    m_nr1Tuning.taps = taps;
#ifdef HAVE_WDSP
    // From Thetis radio.cs:681-698 [v2.10.3.13]
    SetRXAANRTaps(m_channelId, taps);
#endif
}

void RxChannel::setAnrDelay(int delay)
{
    m_nr1Tuning.delay = delay;
#ifdef HAVE_WDSP
    // From Thetis radio.cs:681-698 [v2.10.3.13]
    SetRXAANRDelay(m_channelId, delay);
#endif
}

void RxChannel::setAnrGain(double gain)
{
    m_nr1Tuning.gain = gain;
#ifdef HAVE_WDSP
    // From Thetis setup.cs:8545 [v2.10.3.13] — caller has already applied ×1e-6.
    // Passes raw WDSP-domain value directly to SetRXAANRGain.
    SetRXAANRGain(m_channelId, gain);
#endif
}

void RxChannel::setAnrLeakage(double leakage)
{
    m_nr1Tuning.leakage = leakage;
#ifdef HAVE_WDSP
    // From Thetis setup.cs:8550 [v2.10.3.13] — caller has already applied ×1e-3.
    // Passes raw WDSP-domain value directly to SetRXAANRLeakage.
    SetRXAANRLeakage(m_channelId, leakage);
#endif
}

void RxChannel::setAnrPosition(NrPosition p)
{
    m_nr1Tuning.position = p;
#ifdef HAVE_WDSP
    // From Thetis setup.cs:8723 [v2.10.3.13]
    SetRXAANRPosition(m_channelId, static_cast<int>(p));
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
#endif
}

void RxChannel::setEmnrTrainT1(double t1)
{
#ifdef HAVE_WDSP
    // From Thetis dsp.cs:315 [v2.10.3.13] — SetRXAEMNRtrainZetaThresh
    // "T1" in the UI maps to zetathresh in emnr.c:1352
    SetRXAEMNRtrainZetaThresh(m_channelId, t1);
#else
    Q_UNUSED(t1);
#endif
}

void RxChannel::setEmnrTrainT2(double t2)
{
#ifdef HAVE_WDSP
    // From Thetis dsp.cs:318 [v2.10.3.13] — SetRXAEMNRtrainT2
    SetRXAEMNRtrainT2(m_channelId, t2);
#else
    Q_UNUSED(t2);
#endif
}

void RxChannel::setEmnrAeZetaThresh(double v)
{
#ifdef HAVE_WDSP
    // From Thetis dsp.cs:287 [v2.10.3.13] — SetRXAEMNRaeZetaThresh
    SetRXAEMNRaeZetaThresh(m_channelId, v);
#else
    Q_UNUSED(v);
#endif
}

void RxChannel::setEmnrAePsi(double v)
{
#ifdef HAVE_WDSP
    // From Thetis dsp.cs:289 [v2.10.3.13] — SetRXAEMNRaePsi
    SetRXAEMNRaePsi(m_channelId, v);
#else
    Q_UNUSED(v);
#endif
}

void RxChannel::setEmnrPost2Run(bool on)
{
    m_nr2Tuning.post2Run = on;
#ifdef HAVE_WDSP
    // From Thetis setup.cs:34719-34720, radio.cs:2122 [v2.10.3.13]
    SetRXAEMNRpost2Run(m_channelId, on ? 1 : 0);
#endif
}

void RxChannel::setEmnrPost2Level(double level)
{
    m_nr2Tuning.post2Level = level;
#ifdef HAVE_WDSP
    // From Thetis setup.cs:34711, radio.cs:2141-2155 [v2.10.3.13]
    // Q-c verified: radio.cs passes the raw double value; no ÷100 applied.
    SetRXAEMNRpost2Nlevel(m_channelId, level);
#endif
}

void RxChannel::setEmnrPost2Factor(double factor)
{
    m_nr2Tuning.post2Factor = factor;
#ifdef HAVE_WDSP
    // From Thetis setup.cs:34712, radio.cs:2160-2174 [v2.10.3.13]
    // Q-c verified: radio.cs passes the raw double value; no ÷100 applied.
    SetRXAEMNRpost2Factor(m_channelId, factor);
#endif
}

void RxChannel::setEmnrPost2Rate(double rate)
{
    m_nr2Tuning.post2Rate = rate;
#ifdef HAVE_WDSP
    // From Thetis setup.cs:34713, radio.cs:2179-2193 [v2.10.3.13]
    // Q-c verified: radio.cs passes the raw double value; no scaling.
    SetRXAEMNRpost2Rate(m_channelId, rate);
#endif
}

void RxChannel::setEmnrPost2Taper(int taper)
{
    m_nr2Tuning.post2Taper = taper;
#ifdef HAVE_WDSP
    // From Thetis setup.cs:34714, radio.cs:2198-2212 [v2.10.3.13]
    // Q-c verified: radio.cs passes the raw int value; no scaling.
    SetRXAEMNRpost2Taper(m_channelId, taper);
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
    // From Thetis radio.cs:2275-2295 [v2.10.3.13]
    SetRXARNNRPosition       (m_channelId, static_cast<int>(t.position));
    SetRXARNNRUseDefaultGain (m_channelId, t.useDefaultGain ? 1 : 0);
#endif
}

void RxChannel::setRnnrPosition(NrPosition p)
{
    m_nr3Tuning.position = p;
#ifdef HAVE_WDSP
    // From Thetis radio.cs:2275 [v2.10.3.13]
    SetRXARNNRPosition(m_channelId, static_cast<int>(p));
#endif
}

void RxChannel::setRnnrUseDefaultGain(bool on)
{
    m_nr3Tuning.useDefaultGain = on;
#ifdef HAVE_WDSP
    // From Thetis setup.cs:35460-35462, radio.cs:2293-2311 [v2.10.3.13]
    // "Use fixed gain for input samples" checkbox maps to SetRXARNNRUseDefaultGain.
    SetRXARNNRUseDefaultGain(m_channelId, on ? 1 : 0);
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
    // From Thetis radio.cs:2312-2355 [v2.10.3.13]
    SetRXASBNRreductionAmount    (m_channelId, static_cast<float>(t.reductionAmount));
    SetRXASBNRsmoothingFactor    (m_channelId, static_cast<float>(t.smoothingFactor));
    SetRXASBNRwhiteningFactor    (m_channelId, static_cast<float>(t.whiteningFactor));
    SetRXASBNRnoiseRescale       (m_channelId, static_cast<float>(t.noiseRescale));
    SetRXASBNRpostFilterThreshold(m_channelId, static_cast<float>(t.postFilterThreshold));
    SetRXASBNRnoiseScalingType   (m_channelId, static_cast<int>(t.algo));
#endif
}

void RxChannel::setSbnrReductionAmount(double dB)
{
    m_nr4Tuning.reductionAmount = dB;
#ifdef HAVE_WDSP
    // From Thetis radio.cs:2331 [v2.10.3.13]
    SetRXASBNRreductionAmount(m_channelId, static_cast<float>(dB));
#endif
}

void RxChannel::setSbnrSmoothingFactor(double pct)
{
    m_nr4Tuning.smoothingFactor = pct;
#ifdef HAVE_WDSP
    // From Thetis radio.cs:2338 [v2.10.3.13]
    SetRXASBNRsmoothingFactor(m_channelId, static_cast<float>(pct));
#endif
}

void RxChannel::setSbnrWhiteningFactor(double pct)
{
    m_nr4Tuning.whiteningFactor = pct;
#ifdef HAVE_WDSP
    // From Thetis radio.cs:2345 [v2.10.3.13]
    SetRXASBNRwhiteningFactor(m_channelId, static_cast<float>(pct));
#endif
}

void RxChannel::setSbnrNoiseRescale(double dB)
{
    m_nr4Tuning.noiseRescale = dB;
#ifdef HAVE_WDSP
    // From Thetis radio.cs:2349 [v2.10.3.13]
    SetRXASBNRnoiseRescale(m_channelId, static_cast<float>(dB));
#endif
}

void RxChannel::setSbnrPostFilterThreshold(double dB)
{
    m_nr4Tuning.postFilterThreshold = dB;
#ifdef HAVE_WDSP
    // From Thetis radio.cs:2353 [v2.10.3.13]
    SetRXASBNRpostFilterThreshold(m_channelId, static_cast<float>(dB));
#endif
}

void RxChannel::setSbnrAlgo(SbnrAlgo a)
{
    m_nr4Tuning.algo = a;
#ifdef HAVE_WDSP
    // From Thetis setup.cs:34511-34527 [v2.10.3.13] — Algo 1/2/3 maps to
    // noiseScalingType 0/1/2 (SbnrAlgo enum values are 0/1/2 accordingly).
    SetRXASBNRnoiseScalingType(m_channelId, static_cast<int>(a));
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

void RxChannel::setActiveNr(NrSlot slot)
{
    m_activeNr.store(slot, std::memory_order_release);

#ifdef HAVE_WDSP
    // From Thetis console.cs:43297-43450 SelectNR() [v2.10.3.13] —
    // flip all four WDSP NR Run flags so exactly zero or one is active.
    SetRXAANRRun (m_channelId, (slot == NrSlot::NR1) ? 1 : 0);
    SetRXAEMNRRun(m_channelId, (slot == NrSlot::NR2) ? 1 : 0);
    SetRXARNNRRun(m_channelId, (slot == NrSlot::NR3) ? 1 : 0);
    SetRXASBNRRun(m_channelId, (slot == NrSlot::NR4) ? 1 : 0);
#endif

    // Post-WDSP filter flags.  Filter instances added in Tasks 9-11; for now
    // these atomics just record intent so flag-flipping can be tested before
    // the filter objects exist.
    m_dfnrActive.store(slot == NrSlot::DFNR, std::memory_order_release);
    m_bnrActive .store(slot == NrSlot::BNR,  std::memory_order_release);
    m_mnrActive .store(slot == NrSlot::MNR,  std::memory_order_release);

    // Keep legacy stub atomics in sync with the new single source of truth
    // until Task 12 retires setEmnrEnabled / setNrEnabled.  Not strictly
    // required for correctness, but avoids surprising readers of the old API.
    m_nrEnabled  .store(slot == NrSlot::NR1 || slot == NrSlot::NR2 ||
                        slot == NrSlot::NR3 || slot == NrSlot::NR4);
    m_emnrEnabled.store(slot == NrSlot::NR2);
}

// ---------------------------------------------------------------------------
// SNB (Spectral Noise Blanker)
// ---------------------------------------------------------------------------

void RxChannel::setSnbEnabled(bool enabled)
{
    if (m_nb) m_nb->setSnbEnabled(enabled);
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
    // From Thetis Project Files/Source/Console/radio.cs:1910-1927
    //   WDSP.SetRXASPCWRun(WDSP.id(thread, subrx), value)
    // WDSP third_party/wdsp/src/apfshadow.c:93
    SetRXASPCWRun(m_channelId, enabled ? 1 : 0);
#else
    Q_UNUSED(enabled);
#endif
}

void RxChannel::setApfFreq(double hz)
{
#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/Console/radio.cs:1929-1946
    //   WDSP.SetRXASPCWFreq(WDSP.id(thread, subrx), value)
    //   Freq = CWPitch + tuneOffset (setup.cs:17071)
    // WDSP third_party/wdsp/src/apfshadow.c:117
    SetRXASPCWFreq(m_channelId, hz);
#else
    Q_UNUSED(hz);
#endif
}

void RxChannel::setApfBandwidth(double hz)
{
#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/Console/radio.cs:1948-1965
    //   WDSP.SetRXASPCWBandwidth(WDSP.id(thread, subrx), value)
    //   Default rx_apf_bw = 600.0 Hz
    // WDSP third_party/wdsp/src/apfshadow.c:141
    SetRXASPCWBandwidth(m_channelId, hz);
#else
    Q_UNUSED(hz);
#endif
}

void RxChannel::setApfGain(double gain)
{
#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/Console/radio.cs:1967-1984
    //   WDSP.SetRXASPCWGain(WDSP.id(thread, subrx), value)
    //   Default rx_apf_gain = 1.0 (linear)
    // WDSP third_party/wdsp/src/apfshadow.c:165
    SetRXASPCWGain(m_channelId, gain);
#else
    Q_UNUSED(gain);
#endif
}

void RxChannel::setApfSelection(int selection)
{
#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/Console/radio.cs:1986-2008
    //   WDSP.SetRXASPCWSelection(WDSP.id(thread, subrx), value)
    //   Default _rx_apf_type = 3 (bi-quad)
    //   0=double-pole, 1=matched, 2=gaussian, 3=bi-quad
    // WDSP third_party/wdsp/src/apfshadow.c:45
    SetRXASPCWSelection(m_channelId, selection);
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
    // From Thetis Project Files/Source/Console/radio.cs:1185-1207
    //   WDSP.SetRXASSQLRun(WDSP.id(thread, subrx), value)
    // WDSP third_party/wdsp/src/ssql.c:331
    SetRXASSQLRun(m_channelId, enabled ? 1 : 0);
#else
    Q_UNUSED(enabled);
#endif
}

void RxChannel::setSsqlThresh(double threshold)
{
#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/Console/radio.cs:1209-1228
    //   WDSP.SetRXASSQLThreshold(WDSP.id(thread, subrx), _fSSqlThreshold)
    //   threshold range clamped 0.0..1.0 as per ssql.c
    //   Thetis default _fSSqlThreshold = 0.16f
    // WDSP third_party/wdsp/src/ssql.c:339
    SetRXASSQLThreshold(m_channelId, threshold);
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
    // From Thetis Project Files/Source/Console/radio.cs:1293-1310
    //   WDSP.SetRXAAMSQRun(WDSP.id(thread, subrx), value)
    // WDSP third_party/wdsp/src/amsq.c (SetRXAAMSQRun)
    SetRXAAMSQRun(m_channelId, enabled ? 1 : 0);
#else
    Q_UNUSED(enabled);
#endif
}

void RxChannel::setAmsqThresh(double dB)
{
#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/Console/radio.cs:1164-1178
    //   WDSP.SetRXAAMSQThreshold(WDSP.id(thread, subrx), value)
    //   value is in dB; WDSP amsq.c applies pow(10.0, threshold/20.0) internally
    //   Thetis default rx_squelch_threshold = -150.0f dB
    // WDSP third_party/wdsp/src/amsq.c (SetRXAAMSQThreshold)
    SetRXAAMSQThreshold(m_channelId, dB);
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
    // From Thetis Project Files/Source/Console/radio.cs:1312-1329
    //   WDSP.SetRXAFMSQRun(WDSP.id(thread, subrx), value)
    // WDSP third_party/wdsp/src/fmsq.c:236
    SetRXAFMSQRun(m_channelId, enabled ? 1 : 0);
#else
    Q_UNUSED(enabled);
#endif
}

void RxChannel::setFmsqThresh(double dB)
{
#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/Console/radio.cs:1274-1291
    //   WDSP.SetRXAFMSQThreshold(WDSP.id(thread, subrx), value)
    //   Thetis fm_squelch_threshold = 1.0f is LINEAR (0..1 scale).
    //   SliceModel stores in dB domain (m_fmsqThresh = -150.0 default).
    //   Convert dB → linear before passing to WDSP.
    //   -150.0 dB → ~3.16e-8 (effectively muted = squelch open on FM)
    // WDSP third_party/wdsp/src/fmsq.c:244 — assigns threshold directly to tail_thresh (linear)
    const double linear = std::pow(10.0, dB / 20.0);
    SetRXAFMSQThreshold(m_channelId, linear);
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
    // Mute → run=0 (panel disabled), unmute → run=1 (panel enabled).
    // From Thetis Project Files/Source/Console/dsp.cs:393-394 — P/Invoke decl
    // WDSP: third_party/wdsp/src/patchpanel.c:126
    SetRXAPanelRun(m_channelId, muted ? 0 : 1);
#else
    Q_UNUSED(muted);
#endif
}

void RxChannel::setAudioPan(double pan)
{
#ifdef HAVE_WDSP
    // Convert NereusSDR -1.0..+1.0 to WDSP 0.0..1.0:
    //   wdsp_pan = (nereus_pan + 1.0) / 2.0
    //   -1.0 → 0.0 (full left), 0.0 → 0.5 (center), +1.0 → 1.0 (full right)
    // WDSP applies sin-law: gain2I = sin(pan*PI), gain2Q = 1 when pan>0.5
    // From Thetis Project Files/Source/Console/radio.cs:1386-1403
    //   default pan = 0.5f (center in WDSP 0..1 scale → NereusSDR 0.0)
    // WDSP: third_party/wdsp/src/patchpanel.c:159
    const double wdspPan = (pan + 1.0) / 2.0;
    SetRXAPanelPan(m_channelId, wdspPan);
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
    // bin=1 → copy=0 → binaural (I/Q separate headphone stereo image)
    // bin=0 → copy=1 → dual-mono (Q := I, same audio on both channels)
    // From Thetis Project Files/Source/Console/radio.cs:1145-1162
    //   default bin_on = false → dual-mono
    // WDSP: third_party/wdsp/src/patchpanel.c:187
    SetRXAPanelBinaural(m_channelId, enabled ? 1 : 0);
#else
    Q_UNUSED(enabled);
#endif
}

// ---------------------------------------------------------------------------
// Frequency shift (pan offset from VFO)
// ---------------------------------------------------------------------------

void RxChannel::setShiftFrequency(double offsetHz)
{
#ifdef HAVE_WDSP
    if (std::abs(offsetHz) < 0.5) {
        // No offset — disable shift for efficiency
        SetRXAShiftRun(m_channelId, 0);
    } else {
        // From Thetis radio.cs:1417-1418 — both calls use the same sign
        SetRXAShiftFreq(m_channelId, offsetHz);
        RXANBPSetShiftFrequency(m_channelId, offsetHz);
        SetRXAShiftRun(m_channelId, 1);
    }
#else
    Q_UNUSED(offsetHz);
#endif
}

// ---------------------------------------------------------------------------
// Channel state
// ---------------------------------------------------------------------------

void RxChannel::setActive(bool active)
{
    if (active == m_active.load()) {
        return;
    }

    m_active.store(active);

#ifdef HAVE_WDSP
    // state=1 on, state=0 off; dmode=0 for no drain, dmode=1 for drain
    SetChannelState(m_channelId, active ? 1 : 0, active ? 0 : 1);
#endif

    qCDebug(lcDsp) << "RxChannel" << m_channelId
                    << (active ? "activated" : "deactivated");
    emit activeChanged(active);
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

    if (!m_active.load()) {
        // Channel inactive — output silence
        std::memset(outI, 0, sampleCount * sizeof(float));
        std::memset(outQ, 0, sampleCount * sizeof(float));
        return;
    }

#ifdef HAVE_WDSP
    // NB1 and NB2 process raw I/Q BEFORE the main WDSP channel.
    // They operate in-place on separate I and Q buffers.
    // From Thetis wdsp-integration.md section 4.3
    // From design doc §sub-epic B — mutually exclusive NB/NB2 via one atomic.
    switch (m_nb ? m_nb->mode() : NereusSDR::NbMode::Off) {
        case NereusSDR::NbMode::NB:  xanbEXTF(m_channelId, inI, inQ); break;
        case NereusSDR::NbMode::NB2: xnobEXTF(m_channelId, inI, inQ); break;
        case NereusSDR::NbMode::Off: /* no-op */                      break;
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
    if (m_dfnr && m_dfnrActive.load(std::memory_order_acquire)) {
        m_dfnr->process(outI, outQ, postCount);
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
#ifdef HAVE_WDSP
    // GetRXAMeter reads WDSP channel state that is only valid once
    // SetChannelState(channel, 1, 0) has been called. Reading before the
    // channel is active segfaults (seen after P1 connect in Phase 3I,
    // because the P1 path had not yet called setActive(true) on the
    // downstream RxChannel the MeterPoller was bound to). Guard here:
    // the contract is "no meter data until active".
    if (!m_active.load()) {
        return -140.0;
    }
    return GetRXAMeter(m_channelId, static_cast<int>(type));
#else
    Q_UNUSED(type);
    return -140.0;
#endif
}

// ---------------------------------------------------------------------------
// DFNR tuning setters (Sub-epic C-1, Task 9)
// ---------------------------------------------------------------------------

#ifdef HAVE_DFNR
void RxChannel::setDfnrAttenLimit(float dB)
{
    if (m_dfnr) {
        m_dfnr->setAttenLimit(dB);
    }
}

void RxChannel::setDfnrPostFilterBeta(float beta)
{
    if (m_dfnr) {
        m_dfnr->setPostFilterBeta(beta);
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

} // namespace NereusSDR
