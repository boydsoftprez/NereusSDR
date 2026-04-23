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
    void processIq(float* inI, float* inQ,
                   float* outI, float* outQ,
                   int sampleCount);

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

    // Cached filter state
    double m_filterLow{150.0};
    double m_filterHigh{2850.0};
};

} // namespace NereusSDR
