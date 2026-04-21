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
#include "wdsp_api.h"

#include <cmath>

namespace NereusSDR {

RxChannel::RxChannel(int channelId, int bufferSize, int sampleRate,
                     QObject* parent)
    : QObject(parent)
    , m_channelId(channelId)
    , m_bufferSize(bufferSize)
    , m_sampleRate(sampleRate)
{
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
// Noise blanker
// ---------------------------------------------------------------------------

void RxChannel::setNb1Enabled(bool enabled)
{
    m_nb1Enabled.store(enabled);
    // NB1 is applied in processIq() — no WDSP call needed here,
    // the xanbEXTF call is gated by the atomic flag.
}

void RxChannel::setNb2Enabled(bool enabled)
{
    m_nb2Enabled.store(enabled);
    // NB2 is applied in processIq() — same pattern as NB1.
}

void RxChannel::setNb2Mode(int mode)
{
#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/Console/HPSDR/specHPSDR.cs:937
    //   SetEXTNOBMode(int id, int mode)
    //   mode: 0=zero, 1=sample-hold, 2=mean-hold, 3=hold-sample, 4=interpolate
    // Thetis cmaster.c default: mode=0 (zero)
    // WDSP: third_party/wdsp/src/nobII.c:658
    SetEXTNOBMode(m_channelId, mode);
#else
    Q_UNUSED(mode);
#endif
}

void RxChannel::setNb2Tau(double tau)
{
#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/Console/HPSDR/specHPSDR.cs:922
    //   SetEXTNOBTau(int id, double tau) — sets advslewtime + hangslewtime
    // Thetis cmaster.c default: slewtime=0.0001 s
    // WDSP: third_party/wdsp/src/nobII.c:686
    SetEXTNOBTau(m_channelId, tau);
#else
    Q_UNUSED(tau);
#endif
}

void RxChannel::setNb2LeadTime(double time)
{
#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/Console/HPSDR/specHPSDR.cs:928
    //   SetEXTNOBAdvtime(int id, double time) — advance/lead time
    // Thetis cmaster.c default: advtime=0.0001 s
    // WDSP: third_party/wdsp/src/nobII.c:707
    SetEXTNOBAdvtime(m_channelId, time);
#else
    Q_UNUSED(time);
#endif
}

void RxChannel::setNb2HangTime(double time)
{
#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/Console/HPSDR/specHPSDR.cs:925
    //   SetEXTNOBHangtime(int id, double time) — hang time
    // Thetis cmaster.c default: hangtime=0.0001 s
    // WDSP: third_party/wdsp/src/nobII.c:697
    SetEXTNOBHangtime(m_channelId, time);
#else
    Q_UNUSED(time);
#endif
}

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
// SNB (Spectral Noise Blanker)
// ---------------------------------------------------------------------------

void RxChannel::setSnbEnabled(bool enabled)
{
    if (enabled == m_snbEnabled.load()) {
        return;
    }

    m_snbEnabled.store(enabled);

#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/Console/console.cs:36347
    //   WDSP.SetRXASNBARun(WDSP.id(0, 0), chkDSPNB2.Checked)
    // Thetis dsp.cs:692-693 — P/Invoke declaration
    // WDSP third_party/wdsp/src/snb.c:579
    SetRXASNBARun(m_channelId, enabled ? 1 : 0);
#else
    Q_UNUSED(enabled);
#endif
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
                          int sampleCount)
{
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
    if (m_nb1Enabled.load()) {
        xanbEXTF(m_channelId, inI, inQ);
    }
    if (m_nb2Enabled.load()) {
        xnobEXTF(m_channelId, inI, inQ);
    }

    // Main WDSP processing: demod, AGC, NR, ANF, filter, EQ, audio panel.
    int error = 0;
    fexchange2(m_channelId, inI, inQ, outI, outQ, &error);

    if (error != 0) {
        qCWarning(lcDsp) << "fexchange2 error on channel"
                         << m_channelId << ":" << error;
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

} // namespace NereusSDR
