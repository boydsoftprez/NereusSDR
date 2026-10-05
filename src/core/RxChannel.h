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
//   2026-09-23 - dspLoad() reader for the WDSP worker's per-block load
//                 counters (R-R3-40) by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//                 NereusSDR-original; no Thetis counterpart. Later the same
//                 day: the block in progress and the per-interval longest
//                 block (takeDspIntervalMaxBlockUs).
//                 Later the same day: the read time (readNs), so a load
//                 reads busy time over wall time (R-R3-40, R-R3-37).
//                 Later the same day: DspLoadCounters::consistent, false for
//                 a read whose busy pair may be torn (R-R3-40).
//   2026-09-24 - A stopping channel is fed until WDSP finishes its stop
//                 (Task 8 of the receiver and transmit gaps plan, Phase 3F
//                 section 3), after Thetis ChannelMaster cmaster.c:365-366
//                 [v2.10.3.15], by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-24 - R-R3-39: every WDSP call runs on the receive lane
//                 (DspControlThread). Setters change the wrapper's state at
//                 once and post their WDSP call, keyed per parameter; meters
//                 read a cache the lane refreshes; readbacks are requests;
//                 processIq outputs silence until the lane has opened the
//                 channel. NereusSDR-original, by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
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
//   2026-09-27 - Nr1Tuning's defaults are Thetis's NR spinbox defaults as
//                 its Setup applies them (gain 100e-6, leak 100e-3), read
//                 from ControlRanges.h (R-IOS-06, R-IOS-27). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
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

#include "core/NereusCoreExport.h"
#include "ControlRanges.h"
#include "NbFamily.h"
#include "WdspTypes.h"
#include "dsp/ChannelConfig.h"
#include "dsp/NnrSettings.h"
#include "dsp/Notch.h"
#include "dsp/RxChannelState.h"

#ifdef HAVE_DFNR
#include "DeepFilterFilter.h"
#endif

#ifdef HAVE_MNR
#include "MacNRFilter.h"
#endif

#include <QList>
#include <QObject>

#include <array>
#include <atomic>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

namespace NereusSDR {

class WdspEngine;  // forward declaration for rebuild()
class DspControlThread;

// The DSP > Options mode group a slice mode reads its per-mode keys from:
// "Phone" (USB, LSB, AM, SAM, DSB), "Cw" (CWU, CWL), "Dig" (DIGU, DIGL,
// SPEC, DRM) or "Fm" (FM); any other mode reads "Phone". It is the <Group>
// in DspOptions<Setting><Group>Rx. RxChannel::onModeChanged, RadioModel's
// remote DSP Options apply and DspOptionsPage's live-apply gate all use this
// one mapping, so a write always applies to the slices that read it
// (R-R3-21). NereusSDR-original, design Section 4B.
QString dspOptionsModeGroup(DSPMode mode);

// Per-receiver WDSP channel wrapper.
//
// Owns one WDSP RX channel and provides Qt property access to DSP
// parameters. Each setter immediately calls the corresponding WDSP
// API function and emits a change signal.
//
// Thread safety:
//   - Main thread: create/destroy, all property setters
//   - DSP thread (RxDspWorker): processIq() calls fexchange2
//   - Receive lane (R-R3-39, setControlLane): every other WDSP call. A
//     setter changes the wrapper's state at once and posts its WDSP call;
//     getMeter() and the readbacks read what the lane last read. With no
//     lane set (unit tests, a bare WdspEngine) every WDSP call runs on the
//     caller's thread, as before.
//
// Ported from Thetis cmaster.c create_rcvr / wdsp-integration.md section 4.
class NEREUS_CORE_EXPORT RxChannel : public QObject {
    Q_OBJECT

    Q_PROPERTY(NereusSDR::DSPMode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(NereusSDR::AGCMode agcMode READ agcMode WRITE setAgcMode NOTIFY agcModeChanged)
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)

public:
    explicit RxChannel(int channelId, int bufferSize, int sampleRate,
                       QObject* parent = nullptr);
    // R-R3-39: a channel whose WDSP calls run on `lane` (see
    // setControlLane). Its NbFamily makes no WDSP call here: WdspEngine
    // runs createWdspObjectsOnLane() on the lane after OpenChannel.
    RxChannel(int channelId, int bufferSize, int sampleRate,
              DspControlThread* lane, QObject* parent);
    ~RxChannel() override;

    int channelId() const { return m_channelId; }
    int bufferSize() const { return m_bufferSize.load(std::memory_order_acquire); }
    int sampleRate() const { return m_sampleRate.load(std::memory_order_acquire); }

    // --- R-R3-39: the receive lane ---
    //
    // Every WDSP call this channel (and its NbFamily and NNR) makes runs on
    // `lane`. A setter updates the wrapper's state at once and posts its
    // WDSP call, keyed per parameter, so a burst of changes to one control
    // costs the lane one call. A call made on the lane itself runs at once.
    // Null (the default) runs every WDSP call on the caller's thread.
    // Set by WdspEngine before the channel is used; never while jobs of this
    // channel are queued.
    void setControlLane(DspControlThread* lane);
    DspControlThread* controlLane() const { return m_lane; }

    // True once the WDSP channel behind this wrapper is open. WdspEngine
    // clears it on a wrapper whose OpenChannel is still queued on the lane
    // and sets it from there; processIq outputs silence until then.
    bool isWdspReady() const { return m_wdspReady.load(std::memory_order_acquire); }
    void setWdspReady(bool ready) { m_wdspReady.store(ready, std::memory_order_release); }

    // WdspEngine's lane-side lifecycle hooks: the NbFamily's WDSP objects are
    // created after OpenChannel and destroyed before CloseChannel, on the
    // lane. markRetired() makes every job of this wrapper still queued a
    // no-op. Lane (or the caller's thread with no lane) only.
    void createWdspObjectsOnLane();
    void destroyWdspObjectsOnLane();
    void markRetired();

    // The C++ side of setSampleRate: the rate and input size carried by the
    // wrapper, no WDSP call. WdspEngine's asynchronous rate change updates it
    // at once and runs applySampleRateOnLane() from the lane with the rate
    // and input size the carry holds.
    void setSampleRateCarry(int newRateHz);
    void applySampleRateOnLane(int rateHz, int bufferSize);
    // --- Live sample-rate change (Thetis-faithful, replaces rebuild) ---
    //
    // Apply a new wire input rate to the existing WDSP channel without
    // destroying the C++ wrapper.  Mirrors the pattern at
    // ChannelMaster/cmaster.c:453-507 [v2.10.3.13] (SetXcmInrate):
    //   SetInputSamplerate(channelId, rate)
    //   SetInputBuffsize(channelId, bufferSizeForRate(rate))
    //
    // The WDSP channel object stays alive across the call — no holders of
    // the RxChannel raw pointer are invalidated.  This is the property the
    // post-v0.3.2 destroy-and-recreate path violated, causing the
    // setSampleRateLive crash on PR #221.
    //
    // Idempotent: a no-op when newRate equals the cached current rate.
    // A no-drain stop still pending is dropped here: the caller's rebuild
    // finishes it (Task 8).
    // Caller is responsible for the surrounding orchestration (drain via
    // setActive(false), stop radio, wait for inflight, then call this,
    // then restart radio, then setActive(true)) — see
    // RadioModel::setSampleRateLive for the full sequence ported from
    // setup.cs::comboAudioSampleRate1_SelectedIndexChanged
    // [v2.10.3.13:7003-7159].
    void setSampleRate(int newRateHz);

    // --- Demodulation ---

    DSPMode mode() const { return static_cast<DSPMode>(m_mode.load()); }
    void setMode(DSPMode mode);

    // Translate a slice-facing DSPMode to the value that WDSP's SetRXAMode
    // should receive. RADE_U / RADE_L are NereusSDR-native (WdspTypes.h
    // :159-186); WDSP has no knowledge of them. The Phase 3R K-bench RX
    // pipeline runs WDSP as the demod front-end in RADE modes so RADE_U
    // -> USB and RADE_L -> LSB. All other modes pass through unchanged.
    // Static so tests can assert the mapping without constructing a
    // RxChannel.
    static DSPMode wdspModeFor(DSPMode mode);

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
    // Upstream inline attribution preserved verbatim (console.cs:50424 [v2.10.3.15]):
    //   if (agc_thresh_point < -160.0) agc_thresh_point = -160.0; //[2.10.3.6]MW0LGE changed from -143
    // Returns clamped value in -160..+2 dB range (Thetis console.cs:50423-50424 [v2.10.3.15]).
    double readBackAgcThresh() const;

    // R-R3-39: both readbacks above, read on the lane after every AGC
    // setter already posted, then `done(top, thresh)` on `context`'s thread.
    // `done` never runs once `context` is gone. With no lane both are read
    // and `done` runs before this returns.
    void requestAgcReadBack(QObject* context,
                            std::function<void(double top, double thresh)> done);

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

    // Per-slice NB / SNB detailed tuning. Removed 2026-04-22 in favour of
    // Setup → DSP → NB/SNB calling SetEXTANB* / SetRXASNBA* directly, but
    // those calls hardcoded channel 0, so tuning the blanker always landed on
    // receiver A whichever receiver the operator was working. Restored by the
    // Sub-Epic J follow-up: SliceModel owns the state and RadioModel pushes it
    // to rxChannel(slice->sliceIndex()) like every other per-slice setting.
    //
    // NB1 / NB2 are per-receiver in WDSP (panb[id] / pnob[id], Thetis
    // cmaster.h:74-82 [v2.10.3.15]), so RadioModel additionally mirrors those
    // five across co-hosted slices. SNB is per WDSP channel
    // (rxa[channel].snba) and stays independent.
    void setNbThreshold(double threshold);
    void setNbTransitionMs(double ms);
    void setNbLeadMs(double ms);
    void setNbLagMs(double ms);
    void setNb2Mode(int mode);
    void setSnbK1(double k1);
    void setSnbK2(double k2);
    void setSnbOutputBandwidthHz(int bandwidthHz);

    // Non-owning handle to the NbFamily facade. Used by WdspEngine to
    // call seedSnbFromSettings() after OpenChannel succeeds — see
    // WdspEngine::createRxChannel. Returns nullptr if built without
    // HAVE_WDSP.
    NereusSDR::NbFamily* nb() { return m_nb.get(); }

    /// Phase 3F Sub-Epic I Task 4b: when true, processIq skips its internal
    /// noise-blanker pass because the caller has already blanked the shared
    /// chunk for this DDC stream. Set on every slice EXCEPT the one that
    /// owns the stream's blanker.
    ///
    /// Upstream keeps one ANB / NOB per receiver, not per sub-receiver
    /// (ChannelMaster cmaster.h:79-81 [v2.10.3.15]), so blanking belongs to
    /// the stream. Without this, each co-hosted slice would re-blank the
    /// same in-place buffer, which is order-dependent and wrong.
    void setNoiseBlankerBypassed(bool bypassed);

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
    // The struct stores raw WDSP-domain values, NOT UI units, so the
    // setAnrGain / setAnrLeakage setters pass them straight to WDSP. The UI
    // layer applies Thetis's conversion first (gain = 1e-6 x the Gain
    // control, leak = 1e-3 x the Leak control; ControlRanges.h, from
    // setup.cs:8573-8586 [v2.10.3.15]).
    // Defaults are Thetis's NR spinbox defaults so converted (taps 64,
    // delay 16, gain 100e-6, leak 100e-3): Thetis's Setup applies them over
    // radio.cs's field initialisers (16e-4, 10e-7) when it loads.
    struct Nr1Tuning {
        int        taps     = static_cast<int>(ControlRanges::kNr1Taps.defaultValue);
        int        delay    = static_cast<int>(ControlRanges::kNr1Delay.defaultValue);
        double     gain     = ControlRanges::kNr1Gain.defaultValue;
        double     leakage  = ControlRanges::kNr1Leak.defaultValue;
        NrPosition position = static_cast<NrPosition>(ControlRanges::kNrPositionDefault);
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
    // Returns the actual accepted WDSP values; an unavailable model changes
    // neither the saved tuning nor the currently active reduction mode.
    bool setNnrTuning(const NnrSettings& settings, QString* reason = nullptr);
    NnrSettings nnrTuning() const;
    NnrDiagnostics nnrDiagnostics() const;
    bool setNnrDiagnostics(int testMode, int outputMode, QString* reason = nullptr);
    // R-R3-40: runtime NNR limit (NnrLimit: 0 none, 1 standard model only,
    // 2 off). Returns at once without the channel's DSP lock; the WDSP worker
    // applies it at its next block. The saved tuning is unchanged, and the
    // limit clamps whatever tuning is applied later. False for a value
    // outside NnrLimit. Main/control thread.
    bool requestNnrLimit(int limit);
    int nnrLimit() const { return m_nnrLimit.load(std::memory_order_acquire); }

    // --- R-R3-39: NNR with a receive lane ---
    //
    // With a lane, NNR's WDSP calls (which take the channel's DSP lock) run
    // there. setNnrTuning, setNnrDiagnostics, setActiveNr and selectNr then
    // decide at once from what the lane last read (nnrDiagnostics()),
    // refusing with the same reasons WDSP gives, and accept otherwise; the
    // lane applies the change and, should WDSP still refuse it, emits
    // nnrRequestRefused. After every NNR call the lane re-reads the
    // diagnostics and emits nnrDiagnosticsRefreshed. Without a lane all of
    // this runs at once, as before, and the signals are emitted before the
    // call returns.

    // Re-reads the NNR diagnostics (on the lane), then emits
    // nnrDiagnosticsRefreshed.
    void refreshNnrDiagnostics();

    // The selection path RadioModel's selection applier runs: when
    // `tuningFirst` is set, that tuning is applied before the selection and
    // a refused tuning refuses the selection too (so a receiver never runs a
    // model other than the saved one); then setActiveNr(slot). `previous` is
    // the selection to go back to when the lane refuses it later.
    bool selectNr(NrSlot slot, const NnrSettings* tuningFirst, NrSlot previous,
                  QString* reason = nullptr);

    // RadioModel::applyNnrStateToChannel as one operation: the limit, the
    // saved tuning, then the saved selection, or NR off when the tuning is
    // refused while NNR is selected or when `nr3Blocked`. The diagnostics it
    // leaves carry the tuning's refusal reason as their explanation.
    void applyNnrState(int limit, const NnrSettings& settings, NrSlot slot,
                       bool nr3Blocked);

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
    bool   setActiveNr(NrSlot slot);
    NrSlot activeNr() const { return m_activeNr.load(std::memory_order_acquire); }

    // Accessors for post-WDSP filter atomics (filter classes land in Tasks 9-11).
    bool dfnrActive() const { return m_dfnrActive.load(std::memory_order_acquire); }
    bool bnrActive () const { return m_bnrActive .load(std::memory_order_acquire); }
    bool mnrActive () const { return m_mnrActive .load(std::memory_order_acquire); }

    // DFNR — DeepFilterNet3 neural noise reduction (Sub-epic C-1, Task 9)
    // Tuning setters keep the value and forward it to the DeepFilterFilter
    // instance once one exists; an instance built later starts with them.
    // Safe to call unconditionally — no-ops when HAVE_DFNR is not defined.
#ifdef HAVE_DFNR
    void setDfnrAttenLimit(float dB);
    void setDfnrPostFilterBeta(float beta);
#endif

    // R-R3-39: whether DFNR can run in this build: HAVE_DFNR and a
    // DeepFilterNet3 model ModelPaths can find. Loads nothing.
    static bool dfnrAvailable();
    // True once this channel's DeepFilterNet3 instance is built. It is built
    // at the channel's first DFNR selection, on the receive lane, and kept
    // until the channel is destroyed.
    bool dfnrLoaded() const;

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

    // AF Gain: 0.0..1.0 linear, fed straight to the WDSP RX audio panel.
    // wdsp/rxa.c:538 [v2.10.3.14] initializes panel.gain1 = 4.0 (+12 dB),
    // so the host MUST call this to bring the panel down to a sane unity
    // level — without it, downstream audio peaks ~4× hot and clips on the
    // device. Thetis routes the per-slice AF slider through the same
    // setter via the RXOutputGain property at radio.cs:1089 [v2.10.3.14],
    // with slider/Maximum yielding 0.0..1.0.
    // From Thetis Project Files/Source/Console/radio.cs:1077-1107 [v2.10.3.14]
    // WDSP: third_party/wdsp/src/patchpanel.c:142
    double afGain() const { return m_afGain.load(); }
    void setAfGain(double gain);  // gain ∈ [0.0, 1.0], clamped

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

    // The offset last handed to setShiftFrequency, in Hz. Carried because
    // WDSP exposes no getter for shift.freq, and the design-doc 4.1
    // invariant (notch tune frequency + shift == the slice's demodulated
    // RF) has to be assertable from the caller side.
    double shiftOffsetHz() const { return m_shiftOffsetHz; }

    // --- Notch bandpass tune frequency (TNF section 4) ---

    // The RF origin the per-channel notch database maps its absolute-Hz
    // notch centres from: the hosting DDC stream's CENTRE, not the slice
    // frequency. WDSP sums it with the shift above
    // (offset = tunefreq + shift, third_party/wdsp/src/nbp.c:192) and
    // setShiftFrequency already carries the slice's displacement from that
    // centre, so driving this from the slice frequency would compute
    // 2*sliceFreq - streamCentre.
    // From Thetis console.cs:31940-31941 [v2.10.3.15].
    void setNotchTuneFrequency(double absoluteHz);
    double notchTuneFrequencyHz() const { return m_notchTuneFrequencyHz; }

    // The shift value last handed to RXANBPSetShiftFrequency. Deliberately
    // distinct from shiftOffsetHz(): it is written next to that call, so it
    // is what tells a caller (and the test suite) whether the push actually
    // happened. NOTCHDB->shift is write-only from the host side
    // (third_party/wdsp/src/nbp.c:487-496 is its sole writer) and
    // calc_nbp_lightweight reads it with no reference to any run flag
    // (nbp.c:192), so a shift that stops being pushed fails silently.
    double notchShiftHz() const { return m_notchShiftHz.load(std::memory_order_acquire); }

    // --- Manual notch filter (TNF) ---
    //
    // WDSP owns the authoritative per-channel notch database; RxChannel is a
    // thin forwarder. List position IS the WDSP notch index, so every caller
    // must keep its own ordering in lockstep (design doc section 5.2).
    //
    // WDSP builds each RXA channel's database with room for 1024 notches
    // (third_party/wdsp/src/RXA.c:88). RXANBPAddNotch returns -1 and mutates
    // nothing once nn reaches that (nbp.c:368).
    static constexpr int kMaxNotches = 1024;

    // WDSP sizes rxa[] at MAX_CHANNELS (third_party/wdsp/src/comm.h:110), and
    // every notch READBACK dereferences a nested pointer inside that slot:
    // RXANBPGetNumNotches and RXANBPGetNotch reach rxa[ch].ndb.p, and
    // RXANBPGetMinNotchWidth reaches rxa[ch].nbp0.p (nbp.c:465, :393, :594).
    // On an out-of-range or never-opened channel that pointer is garbage or
    // null and the read segfaults, where a plain WDSP setter merely scribbles.
    //
    // Test fixtures across this tree construct RxChannel with kTestChannel = 99
    // for software-only isolation (see design section 11.1), so the readbacks
    // below must be inert there. Same guard NbFamily already carries for the
    // same reason (NbFamily.h:269-275, added after Linux CI #238); production
    // channel ids are 0..maxSlices so this never fires outside tests.
    static constexpr int kWdspMaxChannels = 32;
    bool wdspChannelInRange() const
    {
        return m_channelId >= 0 && m_channelId < kWdspMaxChannels;
    }

    /// Number of notches currently installed on this channel.
    /// Returns 0 when the channel id is outside WDSP's range.
    int notchCount() const;

    /// Insert `n` at WDSP notch index `index`. Returns false when WDSP
    /// refuses (index past the end, or kMaxNotches reached), in which case
    /// nothing was mutated and the caller should resync.
    bool addNotch(int index, const Notch& n);

    /// Overwrite the notch at WDSP index `index`. Returns false when the
    /// index is past the end, in which case nothing was mutated.
    bool editNotch(int index, const Notch& n);

    /// Erase the notch at WDSP index `index`. WDSP shifts the remaining
    /// entries down one slot, so callers must do the same to keep list
    /// position == WDSP index. Returns false when the index is past the end.
    bool deleteNotch(int index);

    /// Replace this channel's entire notch set with `notches`, in list order,
    /// so list position == WDSP notch index. Used on channel activation and
    /// after NotchModel::restoreFromSettings; live edits use the incremental
    /// calls above because this one designs 2N filter pairs.
    void syncNotches(const QList<Notch>& notches);

    /// Master notch enable for THIS channel. WDSP builds every notch database
    /// inert (third_party/wdsp/src/RXA.c:87), so a channel that never gets
    /// this call is notch-inert rather than merely notch-empty.
    void setNotchesRun(bool run);
    bool notchesRun() const { return m_notchesRun; }

    /// Let WDSP widen a notch that is narrower than the filter can realise,
    /// instead of dropping it.
    void setNotchAutoIncrease(bool on);
    bool notchAutoIncrease() const { return m_notchAutoIncrease; }

    /// Narrowest notch the current filter can realise, in Hz. Varies with the
    /// filter's coefficient count and the channel's DSP rate, so it must be
    /// re-read after either changes. Observers follow minNotchWidthChanged
    /// rather than polling.
    double minNotchWidthHz() const;

    /// Read one notch straight back out of WDSP's per-channel database.
    /// RXANBPGetNotch (third_party/wdsp/src/nbp.c:393) returns 0 on success
    /// and -1 with sentinel outputs (fcenter -1.0, fwidth 0.0, active -1)
    /// when `index` is past the end, so a caller that ignored the return
    /// would read a notch that does not exist. `out.id` is left untouched:
    /// WDSP's database is positional and carries no id.
    bool notchAt(int index, Notch& out) const;

    // --- R-R3-39: the notch database with a receive lane ---
    //
    // notchCount(), notchAt() and the WDSP-reading paths above take the
    // channel's DSP lock; with a lane only the lane may call them (and
    // minNotchWidthHz() returns what the lane last read). The owner uses
    // the forms below, which run in order on the lane (at once without
    // one).

    /// addNotch / editNotch / deleteNotch, then RadioModel's recovery: a
    /// refused change resyncs the whole list to `expected`, and a count that
    /// still differs from `expected.size()` resyncs too.
    void addNotchReconciled(int index, const Notch& n, const QList<Notch>& expected);
    void editNotchReconciled(int index, const Notch& n, const QList<Notch>& expected);
    void deleteNotchReconciled(int index, const QList<Notch>& expected);
    /// Resyncs to `expected` when WDSP holds a different number of notches.
    void reconcileNotchCount(const QList<Notch>& expected);

    /// Readbacks on the lane, answered on `context`'s thread (never once
    /// `context` is gone); at once without a lane.
    void requestNotchCount(QObject* context, std::function<void(int)> done);
    void requestNotchAt(int index, QObject* context,
                        std::function<void(bool ok, Notch notch)> done);
    void requestMinNotchWidth(QObject* context, std::function<void(double)> done);

    // --- Filter convenience setters (single-axis) ---
    // Thin wrappers that remember the pending low/high and call setFilterFreqs.
    // Carry-only for state preservation in captureState/applyState; WDSP wiring
    // is via setFilterFreqs which is called when both low+high are applied.
    void setFilterLow(int lowHz);
    void setFilterHigh(int highHz);

    // --- EQ carry fields (wired to WDSP in a later task) ---
    // Carry-only for state preservation; no WDSP calls until EQ task lands.
    void setEqEnabled(bool enabled);
    void setEqPreamp(int preampDb);
    void setEqBand(int bandIndex, int gainDb);

    // --- Squelch carry (single unified squelch for state round-trip) ---
    // Carry-only; per-mode squelch (SSQL/AMSQ/FMSQ) is still the primary API.
    void setSquelchEnabled(bool enabled);
    void setSquelchThreshold(int thresholdDb);

    // --- RIT offset (carry; wired to WDSP in RIT task) ---
    void setRitOffset(int ritHz);

    // --- Antenna index (carry; routed via AlexController in antenna task) ---
    void setAntennaIndex(int index);

    // --- Shift offset (convenience alias for setShiftFrequency) ---
    void setShiftOffset(double offsetHz);

    // --- NB enabled (carry; NbFamily is the primary API) ---
    void setNbEnabled(bool enabled);

    // --- NR mode (carry; setActiveNr(NrSlot) is the primary API) ---
    void setNrMode(int nrMode);

    // --- Filter frequency response (Task 1.5) ---

    /// Returns the FFT magnitude (in dB) of the current filter taps, sampled
    /// at nPoints uniformly across [0, sampleRate/2].  For the high-resolution
    /// filter graph (Section 4D, DspOptionsHighResFilterCharacteristics).
    ///
    /// Implementation:
    ///   Option B (synthesized) — uses WDSP's fir_bandpass() with the same
    ///   arguments that the internal BANDPASS struct uses, then computes the
    ///   DFT magnitude via FFTW3 double-precision.  This matches the actual
    ///   WDSP filter response because fir_bandpass() is the exact function
    ///   that CalcBandpassFilter() calls internally.
    ///
    ///   Requires HAVE_WDSP and HAVE_FFTW3.  Returns an empty vector when
    ///   either is absent or when nPoints <= 0.
    ///
    /// NereusSDR-original — no Thetis source ported; algorithm is generic.
    QVector<float> filterResponseMagnitudes(int nPoints) const;

    /// Remote-window parity Task 16 (R-R3-49): the FFT size the response
    /// above is computed at, and its magnitude at every bin from DC to
    /// Nyquist (kFilterResponseFftSize / 2 + 1 values, bin j at
    /// j * sampleRate / kFilterResponseFftSize Hz), before any resampling.
    /// `stepHz`, when given, receives that bin spacing. Empty, as above,
    /// without WDSP or FFTW3. filterResponseMagnitudes(n) is
    /// resampleFilterResponse(filterResponseBins(), n), so a Core can send
    /// the bins and a window draw them exactly as a local window draws its
    /// own channel's. NereusSDR-original.
    static constexpr int kFilterResponseFftSize = 4096;
    QVector<double> filterResponseBins(double* stepHz = nullptr) const;
    /// The local filter graph's resampling: `nPoints` values in dB,
    /// normalised to the peak (0 dB), clamped at -120 dB, spread uniformly
    /// from the first bin to the last by linear interpolation of the
    /// magnitudes. Empty for nPoints <= 0 or no bins.
    static QVector<float> resampleFilterResponse(const QVector<double>& binMagnitudes,
                                                 int nPoints);

    // --- State snapshot / restore (Task 1.2) ---
    // Capture all DSP state into a portable struct.
    // Restore the same state (calls all setters above).
    RxChannelState captureState() const;
    void applyState(const RxChannelState& state);

    // --- Channel rebuild (Task 1.3) ---
    // Tear down the WDSP channel, recreate with new config, reapply
    // captured state. Delegates to WdspEngine::rebuildRxChannel().
    //
    // Returns elapsed milliseconds (≥ 0 on success). Returns -1 if
    // the channel was not found in the engine (should not happen in
    // normal operation — the engine owns all channels).
    //
    // Thread safety: call on main thread only. Caller must ensure the
    // audio thread is not currently processing samples on this channel.
    qint64 rebuild(WdspEngine& engine, const ChannelConfig& cfg);

    // ── In-place filter resize / filter type change ─────────────────────────
    //
    // Wraps the WDSP entry points that Thetis calls from its DSPRX property
    // setters at radio.cs:542 / 561 [v2.10.3.15]:
    //   FilterSize → WDSP.RXASetNC
    //   FilterType → WDSP.RXASetMP (Low Latency = minimum phase, MP 1:
    //   enums.cs:404-408, radio.cs:571 [v2.10.3.15])
    //
    // These are SAFE to call from the main thread while the audio worker is
    // alive — RXASetNC/RXASetMP internally quiesce via SetChannelState's
    // flushflag handshake (third_party/wdsp/src/channel.c:259-297
    // [v2.10.3.13]) before reconfiguring all dependent subsystems, then
    // restore the prior run state.  No external worker quiesce required.
    //
    // Idempotent: a no-op when the new value matches the cached current value.
    void setFilterSizeSamples(int nc);
    void setFilterTypeLinearPhase(bool linearPhase);

    // ── DSP block size (live-apply via WDSP SetDSPBuffsize) ─────────────────
    //
    // Wraps the WDSP entry point Thetis calls from DSPRX.BufferSize setter
    // at radio.cs:521 [v2.10.3.13]:
    //   WDSP.SetDSPBuffsize(WDSP.id(thread, subrx), value);
    //
    // Thetis invariant from console.cs:38911 [v2.10.3.13]:
    //   if (filtsize < bufsize) bufsize = filtsize;
    // i.e. buffer size must never exceed filter size.  Required by WDSP
    // fircore — firmin.c:135 [v2.10.3.13] computes nfor = nc / size and
    // crashes at fftw_execute(NULL) if nc < size.  This setter silently
    // clamps `size` down to `m_filterSize` to maintain the invariant
    // (matches Thetis behaviour exactly — user's selection in the UI
    // combo may differ from the actual applied value).
    //
    // Internally calls SetDSPBuffsize, which quiesces via SetChannelState
    // and rebuilds the DSP graph with the new block size — heavier than
    // RXASetNC but still safe to call while the audio worker is alive.
    // Channel.c:181-194 [v2.10.3.13] for the WDSP-side semantics.
    void setDspBufferSizeSamples(int size);
    int  dspBlockSize() const { return m_dspBlockSize; }

    // --- Channel state ---

    // True while the channel runs. setActive(false) is the draining stop
    // (SetChannelState dmode 1): the channel stays active, and processIq keeps
    // exchanging on it, until WDSP has slewed it down and flushed it, a few
    // blocks of input while I/Q flows (Task 8). Then it counts as inactive
    // and activeChanged(false) is emitted. With no I/Q arriving the stop ends
    // at WDSP's 100 ms drain timeout.
    //
    // R-R3-39: with a receive lane this is the state the owner asked for; it
    // changes at once, while the lane runs the SetChannelState calls (and a
    // draining stop's wait) in the order above. processIq follows the lane.
    bool isActive() const { return m_active.load(); }

#ifdef NEREUS_BUILD_TESTS
    // R-IOS-13 (2026-09-27): WDSP's minimum-phase flag on the channel's
    // notched bandpass (rxa[].nbp0.p->mp, what RXASetMP sets first), or -1
    // when the channel is not open.
    int bandpassMinimumPhaseForTest() const;
    // Test-only: true while a no-drain stop is waiting for WDSP to report it
    // done (m_pendingStop). Lets a test see processIq's feed finish the stop
    // rather than the restart's finishPendingStop fallback.
    bool stopPendingForTest() const
    {
        return m_pendingStop.load(std::memory_order_acquire) != 0;
    }
#endif
    void setActive(bool active);

    // Switch the channel off without waiting for WDSP to drain it
    // (SetChannelState dmode 0), the form upstream uses for sub-receiver
    // channels. It returns at once and the channel counts as inactive, but
    // processIq keeps exchanging on it until WDSP reports the slew-down done,
    // which clears the flags the stop set (Task 8). A restart before that
    // (no I/Q reached the channel after the stop) first finishes the stop
    // with a drain, so the channel never restarts silent (fix wave 1, C1);
    // a rebuild of the channel (setSampleRate, then SetInputSamplerate) also
    // finishes it.
    void deactivateWithoutDrain();

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

    // R-R3-39: with a receive lane this reads a cache and asks the lane to
    // refresh it (one queued refresh however many readers ask, so the cache
    // follows the fastest reader's interval); it never touches WDSP on the
    // caller's thread. -140 dBm until the lane's first read of a running
    // channel, and while the channel is off, as before.
    double getMeter(RxMeterType type) const;

    // R-R3-39: whether getMeter's cache holds a reading the lane took after
    // the last change of isActive() (always true with no lane). Until then
    // the cache may still show the state before it; a reader that must not
    // publish a stale reading treats the channel as having none yet.
    bool meterReadingReady() const;

    // --- DSP load (R-R3-40, NereusSDR-original) ---
    //
    // Cumulative counters kept by this channel's WDSP worker since the
    // process started (third_party/wdsp/src/dsplock.c). Every field only
    // grows except blockPeriodUs, the block period (dsp_size / dsp_rate) of
    // the worker's latest block, 0 before its first block. The WDSP channel
    // id can be reused by a later channel, so callers compare two reads to
    // get the load over an interval rather than reading one in isolation.
    struct DspLoadCounters {
        qint64 blocks{0};
        qint64 busyNs{0};
        qint64 lateBlocks{0};
        qint64 maxBlockUs{0};
        int    blockPeriodUs{0};
        // How long the block in progress has run so far; 0 when the worker
        // is not inside a block. Not cumulative.
        qint64 currentBlockNs{0};
        // When the read was taken (WDSP's monotonic clock, the same read
        // that gave currentBlockNs). busyNs + currentBlockNs is the worker's
        // time inside blocks up to readNs.
        qint64 readNs{0};
        // False when the worker kept busyNs and the block in progress
        // changing through every read attempt, so busyNs + currentBlockNs
        // may not be one instant's value: measure nothing with this read.
        bool consistent{true};
    };

    // Reads the counters without the channel's DSP lock, so it never waits
    // for the worker; safe from any thread. Returns false (and leaves `out`
    // zeroed) when the channel id is outside WDSP's range or WDSP is absent.
    bool dspLoad(DspLoadCounters& out) const;

    // The longest block the worker completed since the previous call, which
    // starts the next interval; 0 if none completed or WDSP is absent. One
    // periodic owner only: RadioModel's load sampler (a second caller would
    // split its intervals). Never waits for the worker.
    qint64 takeDspIntervalMaxBlockUs() const;

    // --- Per-mode DSP-Options live-apply (Task 4.2) ---
    //
    // Called when SliceModel emits dspModeChanged. Reads per-mode DSP-Options
    // AppSettings keys (DspOptionsBufferSize<Mode>, DspOptionsFilterSize<Mode>,
    // DspOptionsFilterType<Mode>Rx) for the new mode and triggers rebuild()
    // if any buffer/filter/filter-type setting differs from the currently
    // active channel config.
    //
    // Returns elapsed milliseconds if a rebuild occurred (≥ 0), 0 if nothing
    // changed, or -1 if the channel was not found in the engine.
    //
    // Thread safety: call on main thread only. Requires m_wdspEngine to have
    // been set via setWdspEngine() before this slot fires.
    void setWdspEngine(WdspEngine* engine) { m_wdspEngine = engine; }

    // R-R3-39: with a receive lane the new sizes are carried at once, the
    // WDSP calls run on the lane, and this returns 0; dspOptionsApplied
    // carries the time the lane took.
    qint64 onModeChanged(DSPMode newMode);

signals:
    // R-R3-39: the lane re-read the NNR diagnostics (nnrDiagnostics() now
    // returns them).
    void nnrDiagnosticsRefreshed();

    // R-R3-39: an NNR change accepted at once was refused by WDSP on the
    // lane. `previousSlot` is the NrSlot to go back to for a refused
    // selection, or -1 for a refused tuning or diagnostic mode.
    void nnrRequestRefused(const QString& reason, int previousSlot);

    // R-R3-49, Sub-epic C-1: this channel's first DFNR selection found the
    // DeepFilterNet model missing (modelMissing) or failing to load, so DFNR
    // cannot run. Emitted once, on the receive lane (the caller's thread
    // with no lane). RadioModel turns the Core's dfnrRunnable off.
    void dfnrUnavailable(bool modelMissing);

    // R-R3-39: onModeChanged's filter and buffer sizes were applied on the
    // lane, taking `elapsedMs`.
    void dspOptionsApplied(qint64 elapsedMs);

    void modeChanged(NereusSDR::DSPMode mode);
    void agcModeChanged(NereusSDR::AGCMode mode);
    void activeChanged(bool active);
    void filterChanged(double low, double high);

    /// TNF design section 9: the narrowest realisable notch moved.
    ///
    /// WDSP recomputes it as 1600.0 / (nc / 256) * (rate / 48000) on every
    /// read (third_party/wdsp/src/nbp.c:82-96), so it changes silently
    /// whenever the coefficient count or the channel rate does. Thetis has
    /// the same problem and solves it the same way: it re-reads through
    /// UpdateMinimumNotchWidthRX and fires MinimumRXNotchWidthChangedHandlers
    /// (console.cs:48787-48818 [v2.10.3.15]), called from the DSP-options
    /// apply path at console.cs:39052-39053.
    ///
    /// Carries the freshly read value so observers need no second call.
    void minNotchWidthChanged(double minWidthHz);

    // Phase 3J-1 (Task 16.2): TCI audio tap. Emitted from the audio thread
    // after fexchange2 and any post-DSP NR (DFNR, MNR) produce the final
    // enhanced audio block for this receiver. Receivers MUST use
    // Qt::DirectConnection — the L/R pointers are owned by the audio thread
    // (outI/outQ scratch buffers in RxDspWorker) and are valid only for the
    // duration of the slot call. Cross-thread listeners (e.g. TciServer)
    // must copy the audio into their own buffer (typically an SPSC ring)
    // before the slot returns.
    //
    // slice: WDSP channel ID (0 = RX1, 1 = RX2) — maps to TCI trx:N at the
    //   protocol-layer boundary. Phase 16+ may introduce a dedicated receiver-
    //   index field when Slice C/D arrive; for now channelId() == sliceIndex.
    // L, R: post-DSP audio in 32-bit float, linear, [-1.0..+1.0].
    // n: number of float samples per channel (NOT byte count, NOT stereo-frame).
    // srcRate: WDSP output sample rate (48000 Hz for all current RX channels).
    //
    // From design doc §3.5 + §1 (TCI thread architecture). Phase 16 Task 16.3
    // (TciServer-side) connects this signal with Qt::DirectConnection and
    // pushes into AudioRingSpsc.
    void audioFrameReady(int slice, const float* L, const float* R,
                         int n, int srcRate);

private:
    // Shared body of setActive / deactivateWithoutDrain.
    void applyActive(bool active, bool drainOnStop);
    // readBackAgcThresh at a given channel rate (a lane job reads the rate
    // the owner had when it asked).
    double readBackAgcThreshAt(int sampleRate) const;
    // The WDSP half of applyActive, in the lane's order (R-R3-39).
    void applyActiveOnLane(bool active, bool drainOnStop, bool alsoRequested);
    // Completes a no-drain stop WDSP has not reported done (Task 8).
    void finishPendingStop();

    // ── R-R3-39: running WDSP calls on the receive lane ──────────────────
    // runKeyed: at once with no lane (or on the lane itself); otherwise
    // queued under the key of (channel, parameter, sub), so a newer call for
    // the same parameter replaces one still queued. runOrdered: the same,
    // unkeyed, for calls whose order against the others matters (state,
    // notches, NNR, sizes). Both drop the job once the wrapper is retired.
    void runKeyed(quint64 parameter, int sub, std::function<void()> job) const;
    void runOrdered(std::function<void()> job) const;
    // Posts one keyed meter-cache refresh (getMeter).
    void requestMeterRefresh() const;
    void refreshMeterCacheOnLane(quint64 generation) const;
    // NNR on the lane: re-read the diagnostics into the cache (with an
    // optional explanation override) and emit nnrDiagnosticsRefreshed.
    void refreshNnrDiagnosticsOnLane(const QString* explanationOverride = nullptr);
    // The body setActiveNr always ran; returns false when NNR would not run.
    bool applyActiveNrOnLane(NrSlot slot);
    // Sets the NR selection flags (m_activeNr and the ones kept with it).
    void storeActiveNrFlags(NrSlot slot);
    // R-R3-39: builds this channel's DeepFilterNet3 instance when `slot` is
    // DFNR and none is built yet (on the receive lane when there is one), publishes the
    // pointer, then sets m_dfnrActive from the current selection.
    void ensureDfnrOnLane(NrSlot slot);
    // NNR readiness as the lane last read it: nullopt before its first read.
    std::optional<NnrDiagnostics> knownNnrDiagnostics() const;
    void refreshMinNotchWidthOnLane();
    double readMinNotchWidthNow() const;
    void syncNotchesNow(const QList<Notch>& notches);
    void reconcileNotchCountNow(const QList<Notch>& expected);

    DspControlThread* m_lane{nullptr};
    // Shared with every queued job: false once the wrapper is retired, so a
    // job still queued never touches it (WdspEngine deletes it afterwards).
    std::shared_ptr<std::atomic<bool>> m_alive{std::make_shared<std::atomic<bool>>(true)};
    // True once the WDSP channel is open (see isWdspReady).
    std::atomic<bool> m_wdspReady{true};
    // The state the lane has applied: processIq and the meter cache follow
    // this, isActive() follows m_active (R-R3-39). Equal without a lane.
    std::atomic<bool> m_dspActive{false};
    // getMeter's cache, one slot per RxMeterType, refreshed on the lane.
    static constexpr int kRxMeterTypes = static_cast<int>(RxMeterType::AgcAvg) + 1;
    mutable std::array<std::atomic<double>, kRxMeterTypes> m_meterCache{};
    // Bumped by every change of the requested state; a refresh posted
    // before the change does not mark the cache ready.
    std::atomic<quint64> m_meterGeneration{0};
    mutable std::atomic<bool> m_meterCacheReady{false};
    // NNR as the lane last read it, and the accepted tuning (the lane writes
    // what WDSP accepted). Guarded by m_nnrMutex: read on the owner's
    // thread, written on the lane.
    mutable std::mutex m_nnrMutex;
    NnrDiagnostics m_nnrDiagnosticsCache;
    bool m_nnrDiagnosticsKnown{false};
    // The narrowest notch the lane last read (minNotchWidthHz with a lane).
    std::atomic<double> m_minNotchWidthCache{0.0};

    const int m_channelId;
    // m_bufferSize and m_sampleRate are mutated by setSampleRate() — they
    // were const in the original construction-time-immutable design, but
    // live rate change (Thetis cmaster.c:453-507 [v2.10.3.13]) mutates the
    // WDSP-side rate/size so the cached values must follow.
    // Atomic (R-R3-39): the owner changes the carry at once while the DSP
    // worker reads bufferSize() for its external-diversity chunk.
    std::atomic<int> m_bufferSize;
    std::atomic<int> m_sampleRate;

    // Atomic flags for lock-free audio thread reads
    std::atomic<int> m_mode{static_cast<int>(DSPMode::LSB)};  // Must match WdspEngine::createRxChannel init
    std::atomic<int> m_agcMode{static_cast<int>(AGCMode::Med)};
    std::unique_ptr<NereusSDR::NbFamily> m_nb;
    // Phase 3F Sub-Epic I Task 4b: NB bypass for co-hosted slices. Atomic
    // because processIq reads it on the DSP thread; matches the convention
    // of every other flag this hot path reads (m_active, m_dfnrActive,
    // m_bnrActive, m_mnrActive). The carry-only plain bools further down
    // (m_nbEnabled, m_eqEnabled) are never read here, hence not atomic.
    std::atomic<bool> m_nbBypassed{false};
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
    // afGain: 0.0..1.0 linear, mirrors Thetis radio.cs:1078 rx_output_gain
    // [v2.10.3.14] (default 1.0 = unity panel gain). The WDSP RX panel
    // initializes its internal gain1 to 4.0 in rxa.c:538, so setActive()
    // pushes m_afGain via SetRXAPanelGain1 to override the default before
    // any audio flows.
    std::atomic<double> m_afGain{1.0};
    // Slice control plan Task 6 (JJ's ruling): the WDSP panel gain is held
    // at unity and AudioEngine's mixer applies the AF level, a departure
    // from Thetis radio.cs, which sets AF as SetRXAPanelGain1.
    static constexpr double kPanelGain1Unity = 1.0;
    // binauralEnabled: binaural audio — off by default (dual-mono)
    // From Thetis radio.cs:1145-1162 — bin_on = false
    std::atomic<bool> m_binauralEnabled{false};
    std::atomic<bool> m_active{false};
    // Task 8: nonzero while a no-drain stop is waiting for WDSP to report it
    // done. processIq keeps exchanging on the channel meanwhile and clears it
    // (compare-and-swap, so a newer stop's token survives); applyActive and
    // setSampleRate clear it on the control thread. m_stopSerial issues the
    // tokens and is touched only by the control thread.
    std::atomic<quint32> m_pendingStop{0};
    quint32 m_stopSerial{0};

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
    NnrSettings m_nnrTuning;
    std::atomic<int> m_nnrLimit{0};

    // Post-WDSP filter "on" flags.  Filter instances (DeepFilterFilter,
    // NvidiaBnrFilter, MacNRFilter) land in Tasks 9-11; these atomics exist now
    // so setActiveNr() can flip them and callers can read them.
    std::atomic<bool> m_dfnrActive{false};
    std::atomic<bool> m_bnrActive{false};
    std::atomic<bool> m_mnrActive{false};

#ifdef HAVE_DFNR
    // DeepFilterNet3 filter instance (Sub-epic C-1, Task 9).
    // R-R3-39: built at the channel's first DFNR selection, on the receive
    // lane (ensureDfnrOnLane), never in the constructor: the model load is
    // about 250 ms and a connect opens five channels. m_dfnr owns it and is
    // written only there; it is freed with the channel. The audio thread
    // reads m_dfnrActive (acquire) and then m_dfnrInstance (acquire), and
    // uses the instance only when both are set. The instance is published
    // (release) before m_dfnrActive can be set, so a set flag never shows a
    // half-built instance.
    std::unique_ptr<NereusSDR::DeepFilterFilter> m_dfnr;
    std::atomic<NereusSDR::DeepFilterFilter*> m_dfnrInstance{nullptr};
    // Set when the model could not be found or loaded; DFNR then stays off
    // on this channel and the load is not tried again.
    std::atomic<bool> m_dfnrUnavailable{false};
    // Tuning kept for an instance built later (DeepFilterFilter's defaults).
    std::atomic<float> m_dfnrAttenLimit{100.0f};
    std::atomic<float> m_dfnrPostFilterBeta{0.0f};
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

    // --- Carry-only fields for captureState/applyState round-trip ---
    // These hold state that will be wired to WDSP in later tasks.
    // The single-axis filter setters below store here and feed setFilterFreqs.
    int  m_filterLowInt{150};     // mirror of m_filterLow as int (setFilterLow/High carry)
    int  m_filterHighInt{2850};   // mirror of m_filterHigh as int

    // EQ — carry until EQ task wires SetRXAGrphEQ
    bool m_eqEnabled{false};
    int  m_eqPreampDb{0};
    int  m_eqBandsDb[10]{0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    // Squelch unified carry (per-mode SSQL/AMSQ/FMSQ are still primary)
    bool m_squelchEnabled{false};
    int  m_squelchThresholdDb{-150};

    // RIT offset carry (wired in RIT task)
    int  m_ritOffsetHz{0};

    // Antenna index carry (routed via AlexController in antenna task)
    int  m_antennaIndex{0};

    // Shift offset carry (mirrors what was last passed to setShiftFrequency)
    double m_shiftOffsetHz{0.0};

    // Notch tune-frequency carry (mirrors what was last passed to
    // RXANBPSetTuneFrequency). Plain member, not atomic: WDSP owns the
    // authoritative notch database and the audio thread never reads this,
    // so it is main-thread-only state.
    double m_notchTuneFrequencyHz{0.0};

    // Notch shift carry (mirrors what was last passed to
    // RXANBPSetShiftFrequency). Main-thread-only, same reasoning as
    // m_notchTuneFrequencyHz. Do NOT move this write away from the WDSP
    // call it mirrors in setShiftFrequency; that co-location is the point.
    // Atomic: with a receive lane it is written there (R-R3-39).
    std::atomic<double> m_notchShiftHz{0.0};

    // Manual notch carries. Main-thread only, no atomics: WDSP owns the
    // authoritative per-channel notch state and there is no WDSP getter for
    // either flag, so these mirror the last value pushed purely so callers
    // and tests can read back what a channel was told.
    // Defaults mirror WDSP's construction values so the carry is not a lie
    // about a freshly opened channel: create_notchdb master_run = 0
    // (third_party/wdsp/src/RXA.c:87), create_nbp autoincr = 1 (RXA.c:105).
    bool m_notchesRun{false};
    bool m_notchAutoIncrease{true};

    // NB enabled carry (NbFamily is the primary API; this is a convenience bool
    // that reflects whether NbMode != Off)
    bool m_nbEnabled{false};

    // NR mode carry (setActiveNr(NrSlot) is the primary API)
    int  m_nrMode{0};

    // ── Task 4.2: per-mode DSP-Options live-apply ────────────────────────────
    // Non-owning pointer to the WdspEngine set by RadioModel after channel
    // creation. Required for onModeChanged() to call rebuild().
    WdspEngine* m_wdspEngine{nullptr};

    // Current filter size, filter type, and DSP block size — tracked here
    // so the in-place WDSP setters can skip when nothing changed and so
    // setFilterSizeSamples can enforce the Thetis invariant
    // (filter >= buffer per console.cs:38911 [v2.10.3.13]).
    //
    // Defaults: filterSize=4096 + filterType=0 from ChannelConfig defaults.
    // m_dspBlockSize=4096 matches the value RadioModel::connectToRadio
    // passes to WdspEngine::createRxChannel (RadioModel.cpp:1445 — the
    // dsp_size argument to OpenChannel).  WDSP fircore.size is set there
    // and stays in sync with m_dspBlockSize through setDspBufferSizeSamples.
    int m_filterSize{4096};
    // 0 = LowLatency, 1 = LinearPhase. R-IOS-13: starts at LinearPhase,
    // the state WDSP opens the channel in (RXA.c create_nbp mp 0), so the
    // first apply of "Low Latency" reaches RXASetMP. (Thetis's cache starts
    // at Low_Latency, radio.cs:559 [v2.10.3.15], and its first apply is
    // forced.)
    int m_filterType{1};
    int m_dspBlockSize{4096}; // matches createRxChannel dsp_size
};

} // namespace NereusSDR
