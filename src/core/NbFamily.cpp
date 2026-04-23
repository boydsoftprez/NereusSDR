// =================================================================
// src/core/NbFamily.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/ChannelMaster/cmaster.c, original licence from Thetis source is included below
//   Project Files/Source/Console/HPSDR/specHPSDR.cs (Copyright (C) 2010-2018 Doug Wigley, GPLv2+)
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// See also src/core/NbFamily.h for the nob.h / nobII.h / snb.h upstream
// attribution blocks (Warren Pratt, NR0V) that cover the WDSP objects this
// file creates and destroys.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-22 — NbFamily lifecycle + tuning implementation.
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

#include "NbFamily.h"

#include "AppSettings.h"
#include "wdsp_api.h"

namespace NereusSDR {

namespace {
constexpr double kMsToSec = 0.001;
}

NbMode cycleNbMode(NbMode current)
{
    // From Thetis console.cs:42476-42482 [v2.10.3.13] — space-bar cycle:
    //   Unchecked/Indeterminate → Checked
    //   Checked                 → Unchecked
    //   (second press path handles Unchecked/Checked → Indeterminate)
    // Our simpler Off→NB→NB2→Off chain matches the user-visible three-state
    // rotation from console.cs:43513-43560 without Thetis's two-switch
    // hidden-state complexity.
    switch (current) {
        case NbMode::Off:  return NbMode::NB;
        case NbMode::NB:   return NbMode::NB2;
        case NbMode::NB2:  return NbMode::Off;
    }
    return NbMode::Off;
}

NbFamily::NbFamily(int channelId, int sampleRate, int bufferSize)
    : m_channelId(channelId)
    , m_sampleRate(sampleRate)
    , m_bufferSize(bufferSize)
{
#ifdef HAVE_WDSP
    // Global defaults from Setup → DSP → NB/SNB. NB tuning is global per
    // channel (not per-band) per strict Thetis parity; see NbFamily.h for
    // the derivation of the scaling factors.
    //
    // Scaling:
    //   udDSPNB (1-1000, default 30)           × 0.165       → threshold
    //   udDSPNBTransition/Lead/Lag (1-200)     × 0.01 ms     → m_tuning *Ms
    //     (WDSP seconds = slider × 0.01 × 0.001 = slider × 1e-5)
    //   Nb2DefaultMode (0-2)                   raw           → nb2Mode
    {
        auto& s = AppSettings::instance();
        m_tuning.nbThreshold = 0.165 * s.value(QStringLiteral("NbDefaultThresholdSlider"), 30).toDouble();
        m_tuning.nbTauMs     = 0.01  * s.value(QStringLiteral("NbDefaultTransition"), 1).toDouble();
        m_tuning.nbAdvMs     = 0.01  * s.value(QStringLiteral("NbDefaultLead"),       1).toDouble();
        m_tuning.nbHangMs    = 0.01  * s.value(QStringLiteral("NbDefaultLag"),        1).toDouble();
        m_tuning.nb2Mode     =         s.value(QStringLiteral("Nb2DefaultMode"),      0).toInt();
        // nb2Threshold / nb2*Ms stay at NbTuning's struct defaults — Thetis
        // has no NB2 tuning UI, so there's no upstream override to apply.
    }

    // From Thetis cmaster.c:43-53 [v2.10.3.13] — NB (anb) create call.
    //
    // CRITICAL — pass run=1, not 0:
    //   xanb() at nob.c:113 gates the ENTIRE blanker body behind
    //   `if (a->run)`. The mutual-exclusion dispatch lives in
    //   RxChannel::processIq (switch on m_mode), which only calls
    //   xanbEXTF when mode==NB; so a->run can safely stay at 1 for the
    //   whole channel lifetime and the Qt-side mode atomic is what
    //   actually enables/disables blanking. Shipping with run=0 here
    //   made the blanker a no-op in the original sub-epic B PR (user
    //   reported: electric-fence pops weren't blanked even with NB lit).
    create_anbEXT(
        m_channelId,
        /*run=*/1,
        m_bufferSize,
        static_cast<double>(m_sampleRate),
        m_tuning.nbTauMs    * kMsToSec,
        m_tuning.nbHangMs   * kMsToSec,
        m_tuning.nbAdvMs    * kMsToSec,
        m_tuning.nbBacktau,
        m_tuning.nbThreshold);

    // From Thetis specHPSDR.cs:896-907 [v2.10.3.13] — NB2 P/Invoke 10-arg form:
    //   (id, run, mode, buffsize, samplerate, tau, hangtime, advtime, backtau, threshold)
    // This is a reduction of the nobII.c 13-arg C signature. advslewtime and
    // hangslewtime collapse into a single `tau` arg (SetEXTNOBTau at
    // nobII.c:686 pushes both simultaneously — see pushAllTuning below).
    // max_imp_seq_time has no post-create EXT setter and is fixed at create
    // time per nobII.c defaults. `nb2MaxImpMs` in NbTuning is reserved for a
    // future direct C-API wrapper and is unused in this path.
    // Same run=1 rule as create_anbEXT above — xnob() at nobII.c:161
    // gates on a->run, processIq gates on m_mode. See the note on the
    // NB (anb) create call for the full rationale.
    create_nobEXT(
        m_channelId,
        /*run=*/1,
        m_tuning.nb2Mode,
        m_bufferSize,
        static_cast<double>(m_sampleRate),
        m_tuning.nb2SlewMs     * kMsToSec,       // tau  (cmaster.c:62/64 default 0.0001 s)
        m_tuning.nb2HangMs     * kMsToSec,       // hangtime
        m_tuning.nb2AdvMs      * kMsToSec,       // advtime
        m_tuning.nb2Backtau,
        m_tuning.nb2Threshold);
    // NOTE: SNB (channels[id].rxa.snba) seeding deliberately NOT done here
    // — it requires OpenChannel to have initialised the RXA channel first,
    // and NbFamily is constructed in contexts where that may not be true
    // (e.g. the tst_rxchannel_nb2_polish test uses a fabricated channel id
    // that never saw OpenChannel). The SNB seed lives in
    // WdspEngine::createRxChannel (called only from the production path,
    // after OpenChannel succeeds). See seedSnbFromSettings() below.
#endif
}

void NbFamily::seedSnbFromSettings()
{
#ifdef HAVE_WDSP
    // Push persisted SNB Setup defaults to the RXA channel. Callable only
    // after OpenChannel has initialised rxa[m_channelId].snba — otherwise
    // WDSP SetRXASNBA* setters null-deref. See the note in NbFamily() ctor.
    // (Codex review PR #120, P2 — 2026-04-23.)
    //
    // SNB raw pass-through per Thetis setup.cs:17609,17617 [v2.10.3.13];
    // output-BW symmetric around DC (matches our DspSetupPages wiring).
    auto& s = AppSettings::instance();
    const double snbK1 = s.value(QStringLiteral("SnbDefaultK1"),  8.0).toDouble();
    const double snbK2 = s.value(QStringLiteral("SnbDefaultK2"), 20.0).toDouble();
    const int    snbBw = s.value(QStringLiteral("SnbDefaultOutputBW"), 6000).toInt();
    SetRXASNBAk1(m_channelId, snbK1);
    SetRXASNBAk2(m_channelId, snbK2);
    const double half = static_cast<double>(snbBw) / 2.0;
    SetRXASNBAOutputBandwidth(m_channelId, -half, half);
#endif
}

NbFamily::~NbFamily()
{
#ifdef HAVE_WDSP
    // From Thetis cmaster.c:104-105 [v2.10.3.13] — destroy in reverse of create.
    destroy_nobEXT(m_channelId);
    destroy_anbEXT(m_channelId);
#endif
}

void NbFamily::setMode(NbMode mode)
{
    const NbMode prev = m_mode.exchange(mode, std::memory_order_acq_rel);
    if (prev == mode) return;

#ifdef HAVE_WDSP
    // Flush whichever blanker we just left so its state-machine
    // (avg, count, ring indices) doesn't carry stale state into the
    // next time the user returns to that mode. WDSP flush_* functions
    // reset the anb/nob struct to just-created state without touching
    // the run flag.
    //
    // WDSP: third_party/wdsp/src/nob.c:225  (flush_anb / flush_anbEXT)
    //       third_party/wdsp/src/nobII.c    (flush_nob / flush_nobEXT)
    if (prev == NbMode::NB  && mode != NbMode::NB)  flush_anbEXT(m_channelId);
    if (prev == NbMode::NB2 && mode != NbMode::NB2) flush_nobEXT(m_channelId);
#endif
    // The per-buffer xanbEXTF / xnobEXTF dispatch in RxChannel::processIq()
    // reads m_mode and picks the right path; no WDSP run-flag toggle needed.
}

void NbFamily::setSnbEnabled(bool enabled)
{
    // Short-circuit on unchanged state. Mirrors the guard in the old
    // RxChannel::setSnbEnabled() body; avoids redundant SetRXASNBARun
    // calls when upstream model signals re-fire on unchanged values.
    if (m_snbEnabled.load(std::memory_order_acquire) == enabled) return;
    m_snbEnabled.store(enabled, std::memory_order_release);
#ifdef HAVE_WDSP
    // From Thetis console.cs:36347 [v2.10.3.13]
    //   WDSP.SetRXASNBARun(WDSP.id(0, 0), chkDSPNB2.Checked)
    // WDSP: third_party/wdsp/src/snb.c
    SetRXASNBARun(m_channelId, enabled ? 1 : 0);
#endif
}

void NbFamily::setTuning(const NbTuning& tuning)
{
    m_tuning = tuning;
    pushAllTuning();
}

void NbFamily::setNbThreshold(double threshold)
{
    m_tuning.nbThreshold = threshold;
#ifdef HAVE_WDSP
    SetEXTANBThreshold(m_channelId, threshold);
#endif
}

void NbFamily::setNbTauMs(double ms)
{
    m_tuning.nbTauMs = ms;
#ifdef HAVE_WDSP
    // From Thetis setup.cs:16222 [v2.10.3.13]
    //   NBTau = 0.001 * (double)udDSPNBTransition.Value
    SetEXTANBTau(m_channelId, ms * kMsToSec);
#endif
}

void NbFamily::setNbLeadMs(double advMs)
{
    m_tuning.nbAdvMs = advMs;
#ifdef HAVE_WDSP
    // From Thetis setup.cs:16229 [v2.10.3.13]
    //   NBAdvTime = 0.001 * (double)udDSPNBLead.Value
    SetEXTANBAdvtime(m_channelId, advMs * kMsToSec);
#endif
}

void NbFamily::setNbLagMs(double hangMs)
{
    m_tuning.nbHangMs = hangMs;
#ifdef HAVE_WDSP
    // From Thetis setup.cs:16236 [v2.10.3.13]
    //   NBHangTime = 0.001 * (double)udDSPNBLag.Value
    SetEXTANBHangtime(m_channelId, hangMs * kMsToSec);
#endif
}

void NbFamily::pushAllTuning()
{
#ifdef HAVE_WDSP
    // NB1
    SetEXTANBTau      (m_channelId, m_tuning.nbTauMs  * kMsToSec);
    SetEXTANBHangtime (m_channelId, m_tuning.nbHangMs * kMsToSec);
    SetEXTANBAdvtime  (m_channelId, m_tuning.nbAdvMs  * kMsToSec);
    SetEXTANBBacktau  (m_channelId, m_tuning.nbBacktau);
    SetEXTANBThreshold(m_channelId, m_tuning.nbThreshold);

    // NB2
    SetEXTNOBMode     (m_channelId, m_tuning.nb2Mode);
    SetEXTNOBTau      (m_channelId, m_tuning.nb2SlewMs   * kMsToSec);
    SetEXTNOBHangtime (m_channelId, m_tuning.nb2HangMs   * kMsToSec);
    SetEXTNOBAdvtime  (m_channelId, m_tuning.nb2AdvMs    * kMsToSec);
    SetEXTNOBBacktau  (m_channelId, m_tuning.nb2Backtau);
    SetEXTNOBThreshold(m_channelId, m_tuning.nb2Threshold);
    // max_imp_seq_time has no post-create setter in Thetis's spec — it is
    // fixed at create time per cmaster.c:66. Changes require channel re-create.
#endif
}

} // namespace NereusSDR
