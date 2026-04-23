#pragma once

// =================================================================
// src/core/RxChannel.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/radio.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/dsp.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/HPSDR/specHPSDR.cs, original licence from Thetis source is included below
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

#include "NbFamily.h"
#include "WdspTypes.h"

#ifdef HAVE_DFNR
#include "DeepFilterFilter.h"
#endif

#ifdef HAVE_MNR
#include "MacNRFilter.h"
#endif

#include <QObject>

#include <atomic>
#include <cstring>
#include <memory>

namespace NereusSDR {

// Per-receiver WDSP channel wrapper.
//
// Owns one WDSP RX channel and provides Qt property access to DSP
// parameters. Each setter immediately calls the corresponding WDSP
// API function and emits a change signal.
//
// Thread safety:
//   - Main thread: create/destroy, all property setters
//   - Audio thread (future): processIq() calls fexchange2
//   - Meter timer: getMeter() — WDSP meter reads are lock-free
//
// Ported from Thetis cmaster.c create_rcvr / wdsp-integration.md section 4.
class RxChannel : public QObject {
    Q_OBJECT

    Q_PROPERTY(NereusSDR::DSPMode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(NereusSDR::AGCMode agcMode READ agcMode WRITE setAgcMode NOTIFY agcModeChanged)
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)

public:
    explicit RxChannel(int channelId, int bufferSize, int sampleRate,
                       QObject* parent = nullptr);
    ~RxChannel() override;

    int channelId() const { return m_channelId; }
    int bufferSize() const { return m_bufferSize; }
    int sampleRate() const { return m_sampleRate; }

    // --- Demodulation ---

    DSPMode mode() const { return static_cast<DSPMode>(m_mode.load()); }
    void setMode(DSPMode mode);

    // --- Bandpass filter ---

    void setFilterFreqs(double lowHz, double highHz);
    double filterLow() const { return m_filterLow; }
    double filterHigh() const { return m_filterHigh; }

    // --- AGC ---

    AGCMode agcMode() const { return static_cast<AGCMode>(m_agcMode.load()); }
    void setAgcMode(AGCMode mode);
    void setAgcTop(double topdB);

    // Read back AGC top (max gain in dB) from WDSP after threshold change.
    // From Thetis console.cs:45978 — GetRXAAGCTop after SetRXAAGCThresh
    // Returns clamped value in -20..120 dB range.
    double readBackAgcTop() const;

    // Read back AGC threshold from WDSP after top/RF gain change.
    // From Thetis console.cs:50350 pattern — GetRXAAGCThresh after SetRXAAGCTop
    // Upstream inline attribution preserved verbatim (console.cs:50345):
    //   if (agc_thresh_point < -160.0) agc_thresh_point = -160.0; //[2.10.3.6]MW0LGE changed from -143
    // Returns clamped value in -160..0 dB range.
    double readBackAgcThresh() const;

    // AGC advanced parameters
    // From Thetis Project Files/Source/Console/radio.cs:1037-1124
    // From Thetis Project Files/Source/Console/console.cs:45977
    int agcThreshold() const { return m_agcThreshold.load(); }
    void setAgcThreshold(int dBu);

    int agcHang() const { return m_agcHang.load(); }
    void setAgcHang(int ms);

    int agcSlope() const { return m_agcSlope.load(); }
    void setAgcSlope(int slope);

    int agcAttack() const { return m_agcAttack.load(); }
    void setAgcAttack(int ms);

    int agcDecay() const { return m_agcDecay.load(); }
    void setAgcDecay(int ms);

    int agcHangThreshold() const { return m_agcHangThreshold.load(); }
    void setAgcHangThreshold(int val);

    int agcFixedGain() const { return m_agcFixedGain.load(); }
    void setAgcFixedGain(int dB);

    int agcMaxGain() const { return m_agcMaxGain.load(); }
    void setAgcMaxGain(int dB);

    // --- Noise blanker ---

    // From design doc phase3g-rx-experience-epic-design.md §sub-epic B —
    // NB/NB2 are mutually exclusive via a single NbMode atomic. SNB is
    // independent and runs alongside whichever NbMode is active.
    void setNbMode(NereusSDR::NbMode mode);
    NereusSDR::NbMode nbMode() const;

    // Per-slice NB tuning (setNbTuning, setNbThreshold, etc.) removed
    // 2026-04-22 for strict Thetis parity. NB tuning is global per-channel
    // now; Setup → DSP → NB/SNB calls SetEXTANB* directly on channel 0.

    // Non-owning handle to the NbFamily facade. Used by WdspEngine to
    // call seedSnbFromSettings() after OpenChannel succeeds — see
    // WdspEngine::createRxChannel. Returns nullptr if built without
    // HAVE_WDSP.
    NereusSDR::NbFamily* nb() { return m_nb.get(); }

    // --- Noise reduction ---

    void setNrEnabled(bool enabled);
    void setAnfEnabled(bool enabled);

    // EMNR (NR2) — Enhanced Multiband Noise Reduction
    // From Thetis Project Files/Source/Console/radio.cs:2216-2251
    bool emnrEnabled() const { return m_emnrEnabled.load(); }
    void setEmnrEnabled(bool enabled);
    void setEmnrGainMethod(int method);
    void setEmnrNpeMethod(int method);
    void setEmnrAeRun(bool run);
    void setEmnrPosition(int position);

    // ----- NR tuning structs (Sub-epic C-1) -----
    // From Thetis Setup → DSP → NR/ANF tab [v2.10.3.13] — one struct per
    // WDSP NR stage.  Defaults match Thetis radio.cs / RXA.c byte-for-byte.

    // NR1 — LMS Adaptive Noise Reduction (Thetis: WDSP anr.c, Warren Pratt NR0V)
    // Gain/leakage stored in UI units; NereusSDR setters apply the same
    // scaling Thetis setup.cs:8545-8550 applies before the WDSP call:
    //   WDSP gain    = 1e-6 * gainUiValue   (Thetis udLMSNRgain  → SetRXAANRVals)
    //   WDSP leakage = 1e-3 * leakUiValue   (Thetis udLMSNRLeak  → SetRXAANRVals)
    // Defaults match the radio.cs private field initialisers:
    //   nr_gain  = 16e-4  →  gainUiValue = 1600.0  (nr_gain / 1e-6)   — unused, see below
    // *** The struct stores raw WDSP-domain values, NOT UI units, so
    //     the setAnrGain / setAnrLeakage setters accept raw values and pass
    //     them straight to WDSP.  The UI layer is responsible for the /1e6
    //     and /1e3 conversions before calling these setters. ***
    // From Thetis radio.cs:673-699 [v2.10.3.13]
    struct Nr1Tuning {
        int        taps     = 64;       // radio.cs:674   nr_taps = 64
        int        delay    = 16;       // radio.cs:675   nr_delay = 16
        double     gain     = 16e-4;    // radio.cs:677   nr_gain = 16e-4
        double     leakage  = 10e-7;    // radio.cs:679   nr_leak = 10e-7
        NrPosition position = NrPosition::PostAgc;  // setup.cs:8723
    };

    // NR2 — EMNR (Enhanced Multiband NR, Warren Pratt NR0V)
    // All post2 values are raw passthrough to WDSP (no scaling at setter boundary).
    // From Thetis radio.cs:2062-2213, setup.cs:34711-34748 [v2.10.3.13]
    struct Nr2Tuning {
        EmnrGainMethod gainMethod = EmnrGainMethod::Gamma;  // setup.cs:17359-17468
        EmnrNpeMethod  npeMethod  = EmnrNpeMethod::Osms;    // setup.cs:17374-17404
        bool           aeFilter   = true;    // radio.cs:2103  rx_nr2_ae_run=1
        NrPosition     position   = NrPosition::PostAgc;    // radio.cs:2237
        // Post-processing cascade (Thetis "Noise post proc" group, dsp.cs:295-312)
        bool   post2Run    = false;    // radio.cs:2122  rx_nr2_ae_post2_run default
        double post2Level  = 15.0;    // radio.cs:2139  rx_nr2_ae_post2_nlevel = 15.0
        double post2Factor = 15.0;    // radio.cs:2158  rx_nr2_ae_post2_factor = 15.0
        double post2Rate   = 5.0;     // radio.cs:2177  rx_nr2_ae_post2_rate = 5.0
        int    post2Taper  = 12;      // radio.cs:2196  rx_nr2_ae_post2_taper = 12
    };

    // NR3 — RNNR (Recurrent Neural Network NR, MW0LGE / Samphire)
    // From Thetis radio.cs:2257-2311, setup.cs:35460-35462 [v2.10.3.13]
    struct Nr3Tuning {
        NrPosition position       = NrPosition::PostAgc;  // radio.cs:2275
        bool       useDefaultGain = true;   // setup.cs:35460  RXANR3FixedGain  default
        // Note: RNNR model path is global (RNNRloadModel), not per-channel.
    };

    // NR4 — SBNR (Spectral Bleach NR, MW0LGE / Samphire)
    // All values are raw passthrough to WDSP (float casts happen at setter boundary).
    // From Thetis radio.cs:2312-2355, setup.cs:34511-34527 [v2.10.3.13]
    struct Nr4Tuning {
        double   reductionAmount     = 10.0;   // setup.cs default
        double   smoothingFactor     = 65.0;   // setup.cs default
        double   whiteningFactor     = 2.0;    // setup.cs default
        double   noiseRescale        = 2.0;    // setup.cs default
        double   postFilterThreshold = -10.0;  // setup.cs default
        SbnrAlgo algo                = SbnrAlgo::Algo2;  // setup.cs:34511-34527
    };

    // ----- NR API (Sub-epic C-1) -----
    // New unified NR surface.  Coexists with legacy setEmnrEnabled /
    // setNrEnabled stubs until Task 12 finishes the SliceModel cutover.

    // Struct-level setters — push full tuning state through multiple WDSP calls.
    void setAnrTuning  (const Nr1Tuning& t);
    void setEmnrTuning (const Nr2Tuning& t);
    void setRnnrTuning (const Nr3Tuning& t);
    void setSbnrTuning (const Nr4Tuning& t);

    // Per-knob convenience setters (single WDSP call each).
    // Gain/leakage are in raw WDSP domain (caller is responsible for 1e-6/1e-3 scaling).
    // From Thetis setup.cs:8539-8566 [v2.10.3.13]
    void setAnrTaps    (int taps);
    void setAnrDelay   (int delay);
    void setAnrGain    (double gain);        // raw WDSP domain; SetRXAANRGain
    void setAnrLeakage (double leakage);     // raw WDSP domain; SetRXAANRLeakage
    void setAnrPosition(NrPosition p);

    // EMNR per-knob setters not in the legacy API.
    // From Thetis setup.cs NR2 group [v2.10.3.13]
    void setEmnrTrainT1        (double t1);     // SetRXAEMNRtrainZetaThresh
    void setEmnrTrainT2        (double t2);     // SetRXAEMNRtrainT2
    void setEmnrAeZetaThresh   (double v);      // SetRXAEMNRaeZetaThresh
    void setEmnrAePsi          (double v);      // SetRXAEMNRaePsi
    void setEmnrPost2Run       (bool on);
    void setEmnrPost2Level     (double level);  // raw passthrough; SetRXAEMNRpost2Nlevel
    void setEmnrPost2Factor    (double factor); // raw passthrough; SetRXAEMNRpost2Factor
    void setEmnrPost2Rate      (double rate);   // raw passthrough; SetRXAEMNRpost2Rate
    void setEmnrPost2Taper     (int taper);     // raw passthrough; SetRXAEMNRpost2Taper

    // RNNR per-knob setters.
    void setRnnrPosition       (NrPosition p);
    void setRnnrUseDefaultGain (bool on);

    // SBNR per-knob setters.
    void setSbnrReductionAmount     (double dB);
    void setSbnrSmoothingFactor     (double pct);
    void setSbnrWhiteningFactor     (double pct);
    void setSbnrNoiseRescale        (double dB);
    void setSbnrPostFilterThreshold (double dB);
    void setSbnrAlgo                (SbnrAlgo a);

    // Central mode dispatch — flip SetRXA*Run flags so exactly 0 or 1 is on.
    // Byte-for-byte from Thetis console.cs:43297-43450 SelectNR() [v2.10.3.13].
    void   setActiveNr(NrSlot slot);
    NrSlot activeNr() const { return m_activeNr.load(std::memory_order_acquire); }

    // Accessors for post-WDSP filter atomics (filter classes land in Tasks 9-11).
    bool dfnrActive() const { return m_dfnrActive.load(std::memory_order_acquire); }
    bool bnrActive () const { return m_bnrActive .load(std::memory_order_acquire); }
    bool mnrActive () const { return m_mnrActive .load(std::memory_order_acquire); }

    // DFNR — DeepFilterNet3 neural noise reduction (Sub-epic C-1, Task 9)
    // Tuning setters forward to the DeepFilterFilter instance if present.
    // Safe to call unconditionally — no-ops when HAVE_DFNR is not defined.
#ifdef HAVE_DFNR
    void setDfnrAttenLimit(float dB);
    void setDfnrPostFilterBeta(float beta);
#endif

    // MNR — Apple Accelerate MMSE-Wiener spectral NR (Sub-epic C-1, Task 11).
    // macOS only (HAVE_MNR is defined only on Apple platforms). On other
    // platforms the setters are declared for API consistency but
    // compile to no-op stubs (see RxChannel.cpp #else branch).
    // Strength: 0 = bypass, 1 = full NR.
    void setMnrStrength(float strength);
    void setMnrOversub(float oversub);   // MMSE-Wiener oversubtraction 0.01-1000
    void setMnrFloor(float floor);       // min Wiener gain 0.0-2.0
    void setMnrAlpha(float alpha);       // decision-directed smoothing 0.0-1.0
    void setMnrBias(float bias);         // noise-floor bias correction 0.0-10.0
    void setMnrGsmooth(float gsmooth);   // temporal gain smoothing 0.0-1.0

    // SNB — Spectral Noise Blanker
    // From Thetis Project Files/Source/Console/radio.cs (SetRXASNBARun call site)
    bool snbEnabled() const { return m_nb ? m_nb->snbEnabled() : false; }
    void setSnbEnabled(bool enabled);

    // APF — Audio Peak Filter (CW narrow-peak filter via WDSP SPCW module)
    // From Thetis Project Files/Source/Console/radio.cs:1910-2008
    // WDSP: third_party/wdsp/src/apfshadow.c
    bool apfEnabled() const { return m_apfEnabled.load(); }
    void setApfEnabled(bool enabled);
    void setApfFreq(double hz);
    void setApfBandwidth(double hz);
    void setApfGain(double gain);
    void setApfSelection(int selection);

    // Squelch — SSB (syllabic squelch, WDSP SSQL module)
    // From Thetis Project Files/Source/Console/radio.cs:1185-1229
    // WDSP: third_party/wdsp/src/ssql.c:331,339
    // threshold: 0.0..1.0 linear (Thetis default _fSSqlThreshold = 0.16f)
    bool ssqlEnabled() const { return m_ssqlEnabled.load(); }
    void setSsqlEnabled(bool enabled);
    void setSsqlThresh(double threshold);  // 0.0..1.0 linear

    // Squelch — AM (WDSP AMSQ module)
    // From Thetis Project Files/Source/Console/radio.cs:1164-1178, 1293-1310
    // WDSP: third_party/wdsp/src/amsq.c — threshold in dB
    // threshold: dB domain; WDSP applies pow(10.0, t/20.0) internally
    bool amsqEnabled() const { return m_amsqEnabled.load(); }
    void setAmsqEnabled(bool enabled);
    void setAmsqThresh(double dB);

    // Squelch — FM (WDSP FMSQ module)
    // From Thetis Project Files/Source/Console/radio.cs:1274-1329
    // WDSP: third_party/wdsp/src/fmsq.c:236,244
    // threshold: linear 0..1 (Thetis fm_squelch_threshold = 1.0f, NOT dB)
    // SliceModel m_fmsqThresh{-150.0} is in dB and must be converted:
    //   linear = pow(10.0, dB / 20.0)
    bool fmsqEnabled() const { return m_fmsqEnabled.load(); }
    void setFmsqEnabled(bool enabled);
    void setFmsqThresh(double dB);  // converts dB → linear before WDSP call

    // --- Audio panel (mute / pan / binaural) ---

    // Mute: run=0 silences the audio panel output; run=1 restores it.
    // Maps to SetRXAPanelRun. Default: unmuted (panel runs).
    // From Thetis Project Files/Source/Console/dsp.cs:393-394
    // WDSP: third_party/wdsp/src/patchpanel.c:126
    bool muted() const { return m_muted.load(); }
    void setMuted(bool muted);

    // Audio pan: NereusSDR range -1.0..+1.0 (0.0 = center).
    // Converted to WDSP 0.0..1.0 via wdsp_pan = (pan + 1.0) / 2.0.
    // From Thetis Project Files/Source/Console/radio.cs:1386-1403
    //   Thetis default pan = 0.5f (center in 0..1 scale)
    // WDSP: third_party/wdsp/src/patchpanel.c:159
    void setAudioPan(double pan);  // pan in -1.0..+1.0

    // Binaural mode: enabled → I and Q carry separate headphone channels.
    // Disabled (default) → dual-mono (Q copies I).
    // From Thetis Project Files/Source/Console/radio.cs:1145-1162
    //   Thetis default bin_on = false
    // WDSP: third_party/wdsp/src/patchpanel.c:187
    bool binauralEnabled() const { return m_binauralEnabled.load(); }
    void setBinauralEnabled(bool enabled);

    // --- Frequency shift (for pan offset from VFO) ---

    void setShiftFrequency(double offsetHz);

    // --- Channel state ---

    bool isActive() const { return m_active.load(); }
    void setActive(bool active);

    // --- Audio processing (called from audio thread) ---

    // Process I/Q samples through the WDSP RX chain.
    // Input:  inI/inQ arrays of sampleCount floats (raw I/Q from radio)
    // Output: outI/outQ arrays of sampleCount floats (decoded audio L/R)
    //
    // NB1/NB2 are processed before fexchange2:
    //   xanbEXTF(id, I, Q)  -- if NB1 enabled (in-place)
    //   xnobEXTF(id, I, Q)  -- if NB2 enabled (in-place)
    //   fexchange2(channel, Iin, Qin, Iout, Qout, &error)
    //
    // From Thetis wdsp-integration.md section 4.3
    // sampleCount: number of INPUT samples in inI/inQ (drives fexchange2 input)
    // outSampleCount: number of OUTPUT samples fexchange2 writes to outI/outQ
    //                (post-decimation; defaults to sampleCount for back-compat
    //                 but WDSP output is typically smaller, e.g. 64 at 48 kHz).
    //                Post-WDSP filters (DFNR, MNR) operate only on
    //                outI/outQ[0..outSampleCount-1] so they don't process the
    //                zero-padded tail.
    void processIq(float* inI, float* inQ,
                   float* outI, float* outQ,
                   int sampleCount, int outSampleCount = -1);

    // --- Metering ---

    double getMeter(RxMeterType type) const;

signals:
    void modeChanged(NereusSDR::DSPMode mode);
    void agcModeChanged(NereusSDR::AGCMode mode);
    void activeChanged(bool active);
    void filterChanged(double low, double high);

private:
    const int m_channelId;
    const int m_bufferSize;
    const int m_sampleRate;

    // Atomic flags for lock-free audio thread reads
    std::atomic<int> m_mode{static_cast<int>(DSPMode::LSB)};  // Must match WdspEngine::createRxChannel init
    std::atomic<int> m_agcMode{static_cast<int>(AGCMode::Med)};
    std::unique_ptr<NereusSDR::NbFamily> m_nb;
    std::atomic<bool> m_nrEnabled{false};
    std::atomic<bool> m_anfEnabled{false};
    // emnr: From Thetis radio.cs:2216 — rx_nr2_run default = 0
    std::atomic<bool> m_emnrEnabled{false};
    // apf: Audio Peak Filter — off by default
    // From Thetis radio.cs:1910 — rx_apf_run default = false
    std::atomic<bool> m_apfEnabled{false};
    // ssql: SSB syllabic squelch — off by default
    // From Thetis radio.cs:1185 — _bSSqlOn default = false
    // Upstream inline attribution preserved verbatim (radio.cs:1183):
    //   // MW0LGE [2.9.0.8]
    //   // Voice Squeltch - SSQL from 1.21 WDSP
    std::atomic<bool> m_ssqlEnabled{false};
    // amsq: AM squelch — off by default
    // From Thetis radio.cs:1293 — rx_am_squelch_on default = false
    std::atomic<bool> m_amsqEnabled{false};
    // fmsq: FM squelch — off by default
    // From Thetis radio.cs:1312 — rx_fm_squelch_on default = false
    std::atomic<bool> m_fmsqEnabled{false};
    // muted: audio panel mute — off by default (panel runs)
    // From Thetis Project Files/Source/Console/dsp.cs:393-394
    std::atomic<bool> m_muted{false};
    // binauralEnabled: binaural audio — off by default (dual-mono)
    // From Thetis radio.cs:1145-1162 — bin_on = false
    std::atomic<bool> m_binauralEnabled{false};
    std::atomic<bool> m_active{false};

    // AGC advanced parameters — atomic for thread-safe reads from audio thread
    // Defaults from Thetis Project Files/Source/Console/radio.cs:1037-1124
    // threshold default: From Thetis console.cs:45977 — agc_thresh_point = -20
    std::atomic<int> m_agcThreshold{-20};
    // hang: From Thetis radio.cs:1056-1057 — rx_agc_hang = 250 ms
    std::atomic<int> m_agcHang{250};
    // slope: From Thetis radio.cs:1107-1108 — rx_agc_slope = 0
    std::atomic<int> m_agcSlope{0};
    // attack: From WDSP wcpAGC.c create_wcpagc — tau_attack default 2 ms
    std::atomic<int> m_agcAttack{2};
    // decay: From Thetis radio.cs:1037-1038 — rx_agc_decay = 250 ms
    std::atomic<int> m_agcDecay{250};
    // hangThreshold: From Thetis v2.10.3.13 setup.designer.cs:39418
    std::atomic<int> m_agcHangThreshold{0};
    // fixedGain: From Thetis v2.10.3.13 setup.designer.cs:39320
    std::atomic<int> m_agcFixedGain{20};
    // maxGain: From Thetis v2.10.3.13 setup.designer.cs:39245
    std::atomic<int> m_agcMaxGain{90};

    // ----- NR state (Sub-epic C-1) -----
    // From Thetis console.cs:43297-43450 SelectNR() [v2.10.3.13]
    std::atomic<NrSlot> m_activeNr{NrSlot::Off};

    // Full tuning state per stage.  Written by the main thread under no lock
    // (struct setters copy by value; WDSP setters are the authoritative state
    // for the audio thread).
    Nr1Tuning m_nr1Tuning;
    Nr2Tuning m_nr2Tuning;
    Nr3Tuning m_nr3Tuning;
    Nr4Tuning m_nr4Tuning;

    // Post-WDSP filter "on" flags.  Filter instances (DeepFilterFilter,
    // NvidiaBnrFilter, MacNRFilter) land in Tasks 9-11; these atomics exist now
    // so setActiveNr() can flip them and callers can read them.
    std::atomic<bool> m_dfnrActive{false};
    std::atomic<bool> m_bnrActive{false};
    std::atomic<bool> m_mnrActive{false};

#ifdef HAVE_DFNR
    // DeepFilterNet3 filter instance (Sub-epic C-1, Task 9).
    // Created in constructor; null if model not found or df_create failed.
    // Accessed only from the audio thread during processIq(); main thread
    // writes tuning parameters via atomic setters in DeepFilterFilter.
    std::unique_ptr<NereusSDR::DeepFilterFilter> m_dfnr;
#endif

#ifdef HAVE_MNR
    // Apple Accelerate MMSE-Wiener NR instance (Sub-epic C-1, Task 11).
    // Created in constructor; isValid() always true on macOS (Accelerate is
    // a system framework — no external model or library dependency).
    // Accessed only from the audio thread during processIq(); main thread
    // writes strength via setMnrStrength() which calls the atomic setter.
    std::unique_ptr<NereusSDR::MacNRFilter> m_mnr;
#endif

    // Cached filter state
    double m_filterLow{150.0};
    double m_filterHigh{2850.0};
};

} // namespace NereusSDR
