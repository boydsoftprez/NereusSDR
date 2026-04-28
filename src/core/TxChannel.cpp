/*  TXA.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2014, 2016, 2017, 2021, 2023 Warren Pratt, NR0V

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

// =================================================================
// src/core/TxChannel.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/wdsp/TXA.c    — TXA pipeline construction,
//                                         licence above (Warren Pratt, NR0V)
//   Project Files/Source/ChannelMaster/cmaster.c — channel lifecycle,
//                                         licence above (Warren Pratt, NR0V)
//
// Ported from Thetis wdsp/TXA.c:31-479 [v2.10.3.13]
// Ported from Thetis wdsp/cmaster.c:177-190 [v2.10.3.13]
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-25 — TxChannel C++ wrapper implemented by J.J. Boyd (KG4VCF)
//                 during 3M-1a Task C.2, with AI-assisted transformation
//                 via Anthropic Claude Code.
//   2026-04-25 — setTuneTone() PostGen wiring added by J.J. Boyd (KG4VCF)
//                 during 3M-1a Task C.3, with AI-assisted transformation
//                 via Anthropic Claude Code.
//   2026-04-26 — setRunning(bool) / isRunning() / setStageRunning(Stage, bool)
//                 implemented by J.J. Boyd (KG4VCF) during 3M-1a Task C.4.
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-04-26 — setConnection() / setMicRouter() / driveOneTxBlock() /
//                 m_txProductionTimer implemented by J.J. Boyd (KG4VCF) during
//                 3M-1a Task G.1 (TX I/Q production loop). AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-04-26 — Bench round 3 fix (Issues A+B+C): constructor now accepts
//                 inputBufferSize + outputBufferSize and sizes m_inI/inQ/outI/
//                 outQ accordingly.  Previous code used kTxDspBufferSize (4096)
//                 for all four buffers, causing fexchange2 to produce no output.
//                 By J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "TxChannel.h"
#include "LogCategories.h"
#include "RadioConnection.h"
#include "TxMicRouter.h"

// WDSP API declarations (SetTXAPostGen*, fexchange2, etc.) — guarded by
// HAVE_WDSP internally.  Include unconditionally; the header guards itself.
#include "wdsp_api.h"

// Direct WDSP struct access for stage-Run introspection.
// WDSP declares "extern struct _txa txa[];" in TXA.h; we access
// txa[channel].stagename.p->run directly because no GetTXA*Run API exists.
// From Thetis wdsp/TXA.h:165 [v2.10.3.13] — extern struct _txa txa[];
#ifdef HAVE_WDSP
extern "C" {
#include "../../third_party/wdsp/src/TXA.h"
}
#endif

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Constructor
//
// The WDSP-side TXA channel (all 31 pipeline stages) was already constructed
// by OpenChannel(... type=1 ...) in WdspEngine::createTxChannel() before
// this constructor runs.
//
// inputBufferSize:  fexchange2 Iin/Qin size == OpenChannel in_size (default 256).
// outputBufferSize: fexchange2 Iout/Qout size == in_size × out_rate / in_rate.
//   At 48 kHz out: 256.  At 192 kHz out (P2 Saturn): 256 × 4 = 1024.
//
// CRITICAL: fexchange2 requires Iin/Qin to be exactly in_size samples and
// Iout/Qout to be exactly out_size samples.  Calling it with wrong-sized
// buffers (e.g. kTxDspBufferSize = 2048) produces no output (silent error).
//
// From Thetis wdsp/TXA.c:31-479 [v2.10.3.13] — create_txa() signal flow.
// From Thetis wdsp/cmaster.c:177-190 [v2.10.3.13] — OpenChannel in_size / ch_outrate.
// ---------------------------------------------------------------------------
TxChannel::TxChannel(int channelId,
                     int inputBufferSize,
                     int outputBufferSize,
                     QObject* parent)
    : QObject(parent)
    // Init order must match declaration order (-Wreorder-ctor):
    // m_inputBufferSize, m_outputBufferSize, m_channelId.
    , m_inputBufferSize(inputBufferSize > 0 ? inputBufferSize : 256)
    , m_outputBufferSize(outputBufferSize > 0 ? outputBufferSize : 256)
    , m_channelId(channelId)
{
    // Allocate fexchange2 I/O buffers at correct sizes.
    // Iin/Qin:   m_inputBufferSize samples  (== OpenChannel in_size)
    // Iout/Qout: m_outputBufferSize samples (== in_size × out_rate / in_rate)
    //
    // Bench fix round 3 (Issue A): previous code used kTxDspBufferSize for all
    // four buffers.  fexchange2 requires Iin/Qin of exactly in_size and
    // Iout/Qout of exactly out_size; the dsp-size-sized call with in_size=256
    // would produce no output (silent error).
    //
    // From Thetis wdsp/iobuffs.c fexchange2 [v2.10.3.13].
    // From Thetis wdsp/cmaster.c:179-183 [v2.10.3.13] — in_size, ch_outrate.
    m_inI.assign(m_inputBufferSize, 0.0f);
    m_inQ.assign(m_inputBufferSize, 0.0f);
    m_outI.assign(m_outputBufferSize, 0.0f);
    m_outQ.assign(m_outputBufferSize, 0.0f);
    m_outInterleaved.assign(m_outputBufferSize * 2, 0.0f);

    // Production timer — drives driveOneTxBlock() while TX is active.
    // Cadence: 5 ms.  At 48 kHz input / 256 samples: 256/48000 ≈ 5.33 ms → one
    // fexchange2 call per tick.  At 192 kHz output (P2): out block = 1024 samples.
    // The P2 consumer (m_txIqTimer at 5 ms, 4 frames/tick) drains ~960 samples
    // per tick — slightly under producer; the SPSC ring absorbs the surplus.
    //
    // Qt::PreciseTimer reduces OS scheduler jitter on the audio path.
    m_txProductionTimer = new QTimer(this);
    m_txProductionTimer->setTimerType(Qt::PreciseTimer);
    m_txProductionTimer->setInterval(5);  // 5 ms
    connect(m_txProductionTimer, &QTimer::timeout, this, &TxChannel::driveOneTxBlock);

    qCInfo(lcDsp) << "TxChannel" << m_channelId
                  << "wrapper constructed; WDSP TXA pipeline (31 stages)"
                  << "inBuf=" << m_inputBufferSize
                  << "outBuf=" << m_outputBufferSize;
}

TxChannel::~TxChannel() = default;

// ---------------------------------------------------------------------------
// stageRunningDefault()
//
// Returns the compile-time default run state for each TXA stage.
// This matches the first argument of each create_*() call in
// wdsp/TXA.c:31-479 [v2.10.3.13].
//
// Used by stageRunning() in two cases:
//   1. WDSP not compiled in (!HAVE_WDSP).
//   2. WDSP compiled in but the channel was never opened (txa[] uninitialized).
//      This occurs in unit-test builds that link WDSP but don't call
//      OpenChannel (the HAVE_WDSP define propagates via NereusSDRObjs PUBLIC).
// ---------------------------------------------------------------------------
static bool stageRunningDefault(TxChannel::Stage s)
{
    switch (s) {
    case TxChannel::Stage::RsmpIn:    return false;  // TXA.c:40  run=0
    case TxChannel::Stage::Gen0:      return false;  // TXA.c:51  run=0
    case TxChannel::Stage::Panel:     return true;   // TXA.c:59  run=1
    case TxChannel::Stage::PhRot:     return false;  // TXA.c:71  run=0
    case TxChannel::Stage::MicMeter:  return true;   // TXA.c:80  run=1
    case TxChannel::Stage::AmSq:      return false;  // TXA.c:95  run=0
    case TxChannel::Stage::Eqp:       return false;  // TXA.c:115 run=0 (OFF by default)
    case TxChannel::Stage::EqMeter:   return true;   // TXA.c:130 run=1 (gated on eqp.run)
    case TxChannel::Stage::PreEmph:   return false;  // TXA.c:145 run=0
    case TxChannel::Stage::Leveler:   return false;  // TXA.c:158 run=0
    case TxChannel::Stage::LvlrMeter: return true;   // TXA.c:183 run=1 (gated on leveler.run)
    case TxChannel::Stage::CfComp:    return false;  // TXA.c:202 run=0
    case TxChannel::Stage::CfcMeter:  return true;   // TXA.c:224 run=1 (gated on cfcomp.run)
    case TxChannel::Stage::Bp0:       return true;   // TXA.c:239 run=1 (always runs)
    case TxChannel::Stage::Compressor:return false;  // TXA.c:253 run=0
    case TxChannel::Stage::Bp1:       return false;  // TXA.c:260 run=0 (only with compressor)
    case TxChannel::Stage::OsCtrl:    return false;  // TXA.c:274 run=0
    case TxChannel::Stage::Bp2:       return false;  // TXA.c:282 run=0 (only with compressor)
    case TxChannel::Stage::CompMeter: return true;   // TXA.c:296 run=1 (gated on compressor.run)
    case TxChannel::Stage::Alc:       return true;   // TXA.c:311 run=1 (always on)
    case TxChannel::Stage::AmMod:     return false;  // TXA.c:336 run=0
    case TxChannel::Stage::FmMod:     return false;  // TXA.c:345 run=0
    case TxChannel::Stage::Gen1:      return false;  // TXA.c:361 run=0 (TUNE source, off at rest)
    case TxChannel::Stage::UsLew:     return false;  // TXA.c:369 no run param — channel-upslew driven
    case TxChannel::Stage::AlcMeter:  return true;   // TXA.c:379 run=1
    case TxChannel::Stage::Sip1:      return true;   // TXA.c:394 run=1
    case TxChannel::Stage::Calcc:     return true;   // TXA.c:405 run=1 (PureSignal, on but unused until 3M-4)
    case TxChannel::Stage::Iqc:       return false;  // TXA.c:424 run=0
    case TxChannel::Stage::Cfir:      return false;  // TXA.c:434 run=0 (turned on if needed)
    case TxChannel::Stage::RsmpOut:   return false;  // TXA.c:451 run=0 (turned on if needed)
    case TxChannel::Stage::OutMeter:  return true;   // TXA.c:462 run=1
    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// stageRunning()
//
// Returns the current run flag for the specified TXA stage.
//
// With HAVE_WDSP and an initialized channel: reads txa[m_channelId].stagename.p->run
// directly.  The WDSP `struct _txa` is declared with extern linkage in
// TXA.h:165 [v2.10.3.13], so direct field access is valid without locking as
// long as this is called from the main thread (no audio-thread writer for run
// flags at this point in 3M-1a).
//
// With HAVE_WDSP but uninitialized channel (txa[] pointers are null because
// OpenChannel was never called — typical in unit-test builds that link WDSP
// via NereusSDRObjs PUBLIC but don't initialize the engine): falls through to
// stageRunningDefault(), which returns compile-time defaults matching
// create_txa()'s run arguments.
//
// Without HAVE_WDSP: always uses stageRunningDefault().
//
// UsLew exception: create_uslew() takes no "run" parameter — the uslew
// ramp is driven by the channel upslew flag (ch[].iob.ch_upslew), not a
// stage-level run bit accessible via a simple struct field.  For UsLew,
// all paths return false.  Tests document this as EXPECTED.
//
// From Thetis wdsp/TXA.c:31-479 [v2.10.3.13] — run defaults from first
// argument of each create_*() call.
// ---------------------------------------------------------------------------
bool TxChannel::stageRunning(Stage s) const
{
#ifdef HAVE_WDSP
    // Null-guard: txa[] is a zero-initialized global array; if OpenChannel was
    // never called for this channel ID (e.g. unit-test builds that link WDSP
    // but don't call WdspEngine::initialize), all pointer fields are null.
    // Check the sentinel rsmpin.p — if null, the channel is uninitialized.
    if (txa[m_channelId].rsmpin.p == nullptr) {
        return stageRunningDefault(s);
    }

    const int ch = m_channelId;
    switch (s) {
    // From TXA.c:40   run=0 — rsmpin (turned on later if rate conversion needed)
    case Stage::RsmpIn:    return txa[ch].rsmpin.p->run    != 0;
    // From TXA.c:51   run=0 — gen0 (PreGen, 2-TONE noise source)
    case Stage::Gen0:      return txa[ch].gen0.p->run      != 0;
    // From TXA.c:59   run=1 — panel (audio panel/gain control)
    case Stage::Panel:     return txa[ch].panel.p->run     != 0;
    // From TXA.c:71   run=0 — phrot (phase rotator)
    case Stage::PhRot:     return txa[ch].phrot.p->run     != 0;
    // From TXA.c:80   run=1 — micmeter
    case Stage::MicMeter:  return txa[ch].micmeter.p->run  != 0;
    // From TXA.c:95   run=0 — amsq (AM squelch)
    case Stage::AmSq:      return txa[ch].amsq.p->run      != 0;
    // From TXA.c:115  run=0 — eqp (parametric EQ, OFF by default)
    case Stage::Eqp:       return txa[ch].eqp.p->run       != 0;
    // From TXA.c:130  run=1 — eqmeter (gated on eqp.run via second param)
    case Stage::EqMeter:   return txa[ch].eqmeter.p->run   != 0;
    // From TXA.c:145  run=0 — preemph (pre-emphasis filter)
    case Stage::PreEmph:   return txa[ch].preemph.p->run   != 0;
    // From TXA.c:158  run=0 — leveler (wcpagc, OFF by default)
    case Stage::Leveler:   return txa[ch].leveler.p->run   != 0;
    // From TXA.c:183  run=1 — lvlrmeter (gated on leveler.run)
    case Stage::LvlrMeter: return txa[ch].lvlrmeter.p->run != 0;
    // From TXA.c:202  run=0 — cfcomp (multiband compander)
    case Stage::CfComp:    return txa[ch].cfcomp.p->run    != 0;
    // From TXA.c:224  run=1 — cfcmeter (gated on cfcomp.run)
    case Stage::CfcMeter:  return txa[ch].cfcmeter.p->run  != 0;
    // From TXA.c:239  run=1 — bp0 (mandatory BPF, always runs)
    case Stage::Bp0:       return txa[ch].bp0.p->run       != 0;
    // From TXA.c:253  run=0 — compressor (OFF by default)
    case Stage::Compressor:return txa[ch].compressor.p->run!= 0;
    // From TXA.c:260  run=0 — bp1 (only runs when compressor is used)
    case Stage::Bp1:       return txa[ch].bp1.p->run       != 0;
    // From TXA.c:274  run=0 — osctrl (output soft clip)
    case Stage::OsCtrl:    return txa[ch].osctrl.p->run    != 0;
    // From TXA.c:282  run=0 — bp2 (only runs when compressor is used)
    case Stage::Bp2:       return txa[ch].bp2.p->run       != 0;
    // From TXA.c:296  run=1 — compmeter (gated on compressor.run)
    case Stage::CompMeter: return txa[ch].compmeter.p->run != 0;
    // From TXA.c:311  run=1 — alc (ALC, always-on AGC)
    case Stage::Alc:       return txa[ch].alc.p->run       != 0;
    // From TXA.c:336  run=0 — ammod (AM modulation, OFF by default)
    case Stage::AmMod:     return txa[ch].ammod.p->run     != 0;
    // From TXA.c:345  run=0 — fmmod (FM modulation, OFF by default)
    case Stage::FmMod:     return txa[ch].fmmod.p->run     != 0;
    // From TXA.c:361  run=0 — gen1 (PostGen, TUNE tone source)
    case Stage::Gen1:      return txa[ch].gen1.p->run      != 0;
    // From TXA.c:369  — uslew: no "run" parameter in create_uslew();
    //   ramp is gated by ch[].iob.ch_upslew flag, not a stage run bit.
    //   Always returns false here; the ramp activates at channel-state change.
    case Stage::UsLew:     return false;
    // From TXA.c:379  run=1 — alcmeter
    case Stage::AlcMeter:  return txa[ch].alcmeter.p->run  != 0;
    // From TXA.c:394  run=1 — sip1 (siphon for TX spectrum)
    case Stage::Sip1:      return txa[ch].sip1.p->run      != 0;
    // From TXA.c:405  run=1 (runcal) — calcc (PureSignal calibration, ON but unused until 3M-4)
    // calcc struct uses 'runcal' not 'run' — from wdsp/calcc.h:34 [v2.10.3.13]
    case Stage::Calcc:     return txa[ch].calcc.p->runcal  != 0;
    // From TXA.c:424  run=0 — iqc (IQ correction)
    case Stage::Iqc:       return txa[ch].iqc.p0->run      != 0;
    // From TXA.c:434  run=0 — cfir (custom CIC FIR, turned on if needed)
    case Stage::Cfir:      return txa[ch].cfir.p->run      != 0;
    // From TXA.c:451  run=0 — rsmpout (output resampler, turned on if needed)
    case Stage::RsmpOut:   return txa[ch].rsmpout.p->run   != 0;
    // From TXA.c:462  run=1 — outmeter
    case Stage::OutMeter:  return txa[ch].outmeter.p->run  != 0;
    default:
        qCWarning(lcDsp) << "TxChannel::stageRunning: unknown stage" << static_cast<int>(s);
        return false;
    }
#else
    return stageRunningDefault(s);
#endif
}

// ---------------------------------------------------------------------------
// setTuneTone()
//
// Enables or disables the TUNE sine-tone carrier via the WDSP gen1 PostGen.
//
// Porting from Thetis console.cs:30031-30040 [v2.10.3.13] — original C# logic
// inside chkTUN_CheckedChanged (non-pulse branch):
//
//   // put tone in opposite sideband (LSB/CWL/DIGL → negative, else positive)
//   switch (Audio.TXDSPMode) {
//       case DSPMode.LSB:
//       case DSPMode.CWL:
//       case DSPMode.DIGL:
//           radio.GetDSPTX(0).TXPostGenToneFreq = -cw_pitch;   // console.cs:30031
//           break;
//       default:
//           radio.GetDSPTX(0).TXPostGenToneFreq = +cw_pitch;   // console.cs:30034
//           break;
//   }
//   radio.GetDSPTX(0).TXPostGenMode   = 0;             // console.cs:30038
//   radio.GetDSPTX(0).TXPostGenToneMag = MAX_TONE_MAG; // console.cs:30039
//   radio.GetDSPTX(0).TXPostGenRun    = 1;             // console.cs:30040
//
// NereusSDR translation: the sign-flip is the caller's responsibility (G.4
// TUNE function port computes ±cw_pitch and passes it as freqHz).  The call
// order is preserved verbatim: freq → mode → mag → run.
//
// WDSP API:
//   SetTXAPostGenToneFreq — gen.c:808-813 [v2.10.3.13]: txa[ch].gen1.p->tone.freq
//                           + calls calc_tone() to recompute phase increment.
//   SetTXAPostGenMode     — gen.c:792-797 [v2.10.3.13]: txa[ch].gen1.p->mode
//   SetTXAPostGenToneMag  — gen.c:800-805 [v2.10.3.13]: txa[ch].gen1.p->tone.mag
//   SetTXAPostGenRun      — gen.c:784-789 [v2.10.3.13]: txa[ch].gen1.p->run
// ---------------------------------------------------------------------------
void TxChannel::setTuneTone(bool on, double freqHz, double magnitude)
{
#ifdef HAVE_WDSP
    // Null-guard: if the TXA channel was never opened (e.g. unit-test builds
    // that link WDSP but don't call WdspEngine::initialize()), txa[ch].rsmpin.p
    // is null.  The SetTXAPostGen* functions call EnterCriticalSection on
    // ch[channel].csDSP, which would also be uninitialized and segfault.
    // Match the same sentinel guard used in stageRunning().
    if (txa[m_channelId].rsmpin.p == nullptr) {
        return;
    }

    // 3M-1a bench fix: configure TXA mode + bandpass FIRST, before enabling the
    // PostGen tone.  Without this the bp0 default cutoffs are [-5000, -100] Hz
    // (TXA.c:34-35 [v2.10.3.13]), which is LSB-only — USB tones at +600 Hz get
    // BLOCKED by the filter and the carrier never reaches the radio.
    //
    // Cite: deskhpsdr/src/transmitter.c:2828-2829 [@120188f] —
    //   SetTXAMode(tx->id, mode);
    //   tx_set_filter(tx);            // → SetTXABandpassFreqs(tx->id, low, high)
    // Per-mode IQ-space bandpass mapping from deskhpsdr tx_set_filter
    // (transmitter.c:2136-2186 [@120188f]):
    //   USB / DIGU: [+150, +2850]
    //   LSB / DIGL: [-2850, -150]
    //   AM / DSB / SAM / SPEC: [-2850, +2850]  (or +/- high)
    //   FM: [-3000, +3000]
    //   CW (not used in TUN — swapped to LSB/USB by G.4 orchestrator first)
    //
    // For TUN we use a generous filter so the gen1 tone passes regardless of
    // exact cw_pitch.  The mode is determined by the sign of freqHz:
    //   freqHz < 0 → LSB-family (tone in lower sideband)
    //   freqHz > 0 → USB-family (tone in upper sideband)
    if (on) {
        const bool isLsb = (freqHz < 0.0);
        const int  txaMode = isLsb ? 0 /*TXA_LSB*/ : 1 /*TXA_USB*/;
        SetTXAMode(m_channelId, txaMode);
        if (isLsb) {
            SetTXABandpassFreqs(m_channelId, -2850.0, -150.0);
        } else {
            SetTXABandpassFreqs(m_channelId, +150.0, +2850.0);
        }
    }

    // From Thetis console.cs:30031-30040 [v2.10.3.13] — chkTUN_CheckedChanged.
    // Caller passes signed freqHz (±cw_pitch); sign-flip per DSP mode is G.4's job.
    // Call order matches Thetis: freq → mode → mag → run.
    SetTXAPostGenToneFreq(m_channelId, freqHz);          // gen.c:808 [v2.10.3.13]
    SetTXAPostGenMode(m_channelId, 0);                   // gen.c:792 [v2.10.3.13] — 0 = sine tone
    SetTXAPostGenToneMag(m_channelId, magnitude);        // gen.c:800 [v2.10.3.13]
    SetTXAPostGenRun(m_channelId, on ? 1 : 0);           // gen.c:784 [v2.10.3.13]
#else
    Q_UNUSED(on);
    Q_UNUSED(freqHz);
    Q_UNUSED(magnitude);
#endif
}

// ---------------------------------------------------------------------------
// setRunning()
//
// Activate or deactivate the WDSP TXA channel.
//
// Porting from Thetis console.cs callsite pattern [v2.10.3.13]:
//   WDSP.SetChannelState(WDSP.id(1, 0), 1, 0);   // console.cs:29595 — RX→TX
//   WDSP.SetChannelState(WDSP.id(1, 0), 0, 1);   // console.cs:29607 — TX→RX (drain)
//
// cfir is additionally activated for Protocol 2 operation, matching Thetis
// cmaster.cs:522-527 [v2.10.3.13]:
//   WDSP.SetTXACFIRRun(txch, false);   // P1 (USB protocol)
//   WDSP.SetTXACFIRRun(txch, true);    // P2
// NereusSDR activates cfir unconditionally in 3M-1a; P1/P2 gating is deferred
// to 3M-1b when the active protocol is exposed through TxChannel.
//
// Stages NOT activated here:
//   rsmpin / rsmpout: no public Set*Run API — managed by WDSP's internal
//     TXAResCheck() (wdsp/TXA.c:809-817 [v2.10.3.13]), which sets their run
//     flag based on rate mismatch at create_txa() and rate-change calls.
//   bp0 / alc / sip1 / alcmeter / outmeter: default ON in create_txa().
//   gen1: activated by setTuneTone(true).
//   uslew: always-on inside WDSP's xuslew state machine (no run flag).
// ---------------------------------------------------------------------------
void TxChannel::setRunning(bool on)
{
    // Always update the run-state flag and start/stop the production timer,
    // regardless of whether the WDSP channel is open.  This ensures the timer
    // fires correctly in unit-test builds (HAVE_WDSP compiled in but no
    // OpenChannel called — txa[].rsmpin.p is null) and in stub builds (!HAVE_WDSP).
    //
    // driveOneTxBlock() guards against null m_connection and the HAVE_WDSP
    // null-channel path, so it is safe to let the timer fire in both cases.
    m_running = on;

    // Start / stop the TX I/Q production timer (3M-1a G.1).
    // The timer drives driveOneTxBlock() which calls fexchange2 and pushes
    // output to m_connection->sendTxIq() (the SPSC ring producer side).
    if (on) {
        if (m_txProductionTimer) {
            m_txProductionTimer->start();
        }
    } else {
        if (m_txProductionTimer) {
            m_txProductionTimer->stop();
        }
    }

    qCDebug(lcDsp) << "TxChannel" << m_channelId
                   << (on ? "started (channel ON, production timer running)"
                          : "stopped (channel OFF, drain, production timer stopped)");

#ifdef HAVE_WDSP
    // Null-guard: txa[] is a zero-initialized global array; if OpenChannel was
    // never called for this channel ID (e.g. unit-test builds that link WDSP
    // but don't call WdspEngine::initialize()), all pointer fields are null.
    // Check the sentinel rsmpin.p — if null, the channel is uninitialized.
    // Match the same guard used in stageRunning() and setTuneTone().
    if (txa[m_channelId].rsmpin.p == nullptr) {
        return;   // m_running and timer already updated above
    }

    if (on) {
        // Activate cfir: custom CIC FIR filter required for Protocol 2 output.
        // From Thetis wdsp/cfir.c:233-238 [v2.10.3.13] — SetTXACFIRRun.
        // From Thetis cmaster.cs:526-527 [v2.10.3.13]:
        //   // setup CFIR to run; it will always be ON with new protocol firmware
        //   WDSP.SetTXACFIRRun(txch, true);
        // (Diagnostic test confirmed: disabling cfir does NOT change the gen1
        //  output amplitude — bench bug isn't in cfir.  See bench-handoff doc.)
        SetTXACFIRRun(m_channelId, 1);   // cfir.c:233 [v2.10.3.13]

        // Turn the TXA channel ON: state=1, dmode=0 (immediate start, no flush).
        // From Thetis console.cs:29595 [v2.10.3.13] — RX→TX transition:
        //   WDSP.SetChannelState(WDSP.id(1, 0), 1, 0);
        SetChannelState(m_channelId, 1, 0);   // channel.c:259 [v2.10.3.13]
    } else {
        // Turn the TXA channel OFF: state=0, dmode=1 (drain in-flight samples).
        // From Thetis console.cs:29607 [v2.10.3.13] — TX→RX transition:
        //   WDSP.SetChannelState(WDSP.id(1, 0), 0, 1);   // turn off, drain
        //   (preceded by: Thread.Sleep(space_mox_delay); // default 0 // from PSDR MW0LGE [console.cs:29603])
        SetChannelState(m_channelId, 0, 1);   // channel.c:259 [v2.10.3.13]

        // Deactivate cfir after channel drain so no residual samples process.
        // From Thetis wdsp/cfir.c:233-238 [v2.10.3.13] — SetTXACFIRRun.
        SetTXACFIRRun(m_channelId, 0);   // cfir.c:233 [v2.10.3.13]
    }
#endif // HAVE_WDSP
}

// ---------------------------------------------------------------------------
// setStageRunning()
//
// Enable or disable a single TXA pipeline stage by name.
//
// Each supported stage maps to the corresponding WDSP Set*Run function.
// Unsupported stages (rsmpin/rsmpout — managed by TXAResCheck; uslew — no
// run flag; meters/Iqc/Calcc/Alc/Bp*/AmMod/FmMod — added in later tasks)
// log a warning and return without calling WDSP.
//
// From Thetis wdsp/ source files [v2.10.3.13] — individual Set*Run APIs.
// ---------------------------------------------------------------------------
void TxChannel::setStageRunning(Stage s, bool run)
{
    const int r = run ? 1 : 0;

#ifdef HAVE_WDSP
    // Null-guard: same sentinel as stageRunning() / setTuneTone() / setRunning().
    if (txa[m_channelId].rsmpin.p == nullptr) {
        qCWarning(lcDsp) << "TxChannel::setStageRunning: channel" << m_channelId
                         << "not initialized (no OpenChannel call)";
        return;
    }
#endif

    switch (s) {
    // gen0 (PreGen, stage 1): 2-TONE noise source.
    // From Thetis wdsp/gen.c:636-641 [v2.10.3.13].
    case Stage::Gen0:
#ifdef HAVE_WDSP
        SetTXAPreGenRun(m_channelId, r);   // gen.c:636 [v2.10.3.13]
#endif
        return;

    // gen1 (PostGen, stage 22): TUNE tone / 2-TONE. Also driven by setTuneTone().
    // From Thetis wdsp/gen.c:784-789 [v2.10.3.13].
    case Stage::Gen1:
#ifdef HAVE_WDSP
        SetTXAPostGenRun(m_channelId, r);  // gen.c:784 [v2.10.3.13]
#endif
        return;

    // panel (stage 2): audio panel / mic gain. Activated by 3M-1b.
    // From Thetis wdsp/patchpanel.c:201-206 [v2.10.3.13] and patchpanel.h:74.
    case Stage::Panel:
#ifdef HAVE_WDSP
        SetTXAPanelRun(m_channelId, r);    // patchpanel.c:201 [v2.10.3.13]
#endif
        return;

    // phrot (stage 3): phase rotator for SSB carrier-phase correction.
    // From Thetis wdsp/iir.c:665-670 [v2.10.3.13].
    case Stage::PhRot:
#ifdef HAVE_WDSP
        SetTXAPHROTRun(m_channelId, r);    // iir.c:665 [v2.10.3.13]
#endif
        return;

    // amsq (stage 5): TX AM squelch / downward expander.
    // From Thetis wdsp/amsq.c:246-252 [v2.10.3.13] and amsq.h:83.
    case Stage::AmSq:
#ifdef HAVE_WDSP
        SetTXAAMSQRun(m_channelId, r);     // amsq.c:246 [v2.10.3.13]
#endif
        return;

    // eqp (stage 6): TX parametric EQ. Activated by 3M-3a.
    // From Thetis wdsp/eq.c:742-747 [v2.10.3.13].
    case Stage::Eqp:
#ifdef HAVE_WDSP
        SetTXAEQRun(m_channelId, r);       // eq.c:742 [v2.10.3.13]
#endif
        return;

    // compressor (stage 14): TX speech compressor. Also adjusts bp1/bp2.
    // From Thetis wdsp/compress.c:99-109 [v2.10.3.13] and compress.h:60.
    case Stage::Compressor:
#ifdef HAVE_WDSP
        SetTXACompressorRun(m_channelId, r);  // compress.c:100 [v2.10.3.13]
#endif
        return;

    // osctrl (stage 16): CESSB overshoot control. Activated by 3M-3a.
    // From Thetis wdsp/osctrl.c:142-147 [v2.10.3.13].
    case Stage::OsCtrl:
#ifdef HAVE_WDSP
        SetTXAosctrlRun(m_channelId, r);   // osctrl.c:142 [v2.10.3.13]
#endif
        return;

    // cfir (stage 28): custom CIC FIR. Also driven by setRunning().
    // From Thetis wdsp/cfir.c:233-238 [v2.10.3.13] and cfir.h:71.
    case Stage::Cfir:
#ifdef HAVE_WDSP
        SetTXACFIRRun(m_channelId, r);     // cfir.c:233 [v2.10.3.13]
#endif
        return;

    // cfcomp (stage 11): continuous frequency compander. Activated by 3M-3a.
    // From Thetis wdsp/cfcomp.c:632-637 [v2.10.3.13].
    case Stage::CfComp:
#ifdef HAVE_WDSP
        SetTXACFCOMPRun(m_channelId, r);   // cfcomp.c:632 [v2.10.3.13]
#endif
        return;

    // Permanently uncontrollable stages — explicit case arms so the default:
    // below only catches genuinely deferred stages.

    case Stage::RsmpIn:
    case Stage::RsmpOut:
        // Permanently uncontrollable from C++. WDSP's TXAResCheck()
        // (wdsp/TXA.c:809-817 [v2.10.3.13]) sets these run flags
        // automatically based on input/output sample-rate mismatch at
        // create_txa() time. No public PORT API exists.
        qCDebug(lcDsp) << "TxChannel" << m_channelId
                       << "setStageRunning(" << static_cast<int>(s) << ","
                       << run << "): rsmpin/rsmpout managed by TXAResCheck — no-op";
        return;

    case Stage::UsLew:
        // Permanently uncontrollable from a per-stage run flag. The uslew
        // ramp uses a state machine (BEGIN/WAIT/UP/ON) gated by
        // ch[].iob.ch_upslew, not a stage-level run bit.
        // From wdsp/slew.c:62-75 [v2.10.3.13].
        qCDebug(lcDsp) << "TxChannel" << m_channelId
                       << "setStageRunning(UsLew," << run
                       << "): uslew uses runmode state machine — no-op";
        return;

    case Stage::kStageCount:
        // Sentinel — never a valid argument.
        qCWarning(lcDsp) << "TxChannel" << m_channelId
                         << "setStageRunning(kStageCount): sentinel value, ignoring";
        return;

    // Deferred stages — Set*Run API exists in WDSP but is not yet declared
    // in wdsp_api.h. Each will get its own explicit case arm when wired in
    // 3M-1b / 3M-3a / 3M-4.
    default:
        qCWarning(lcDsp) << "TxChannel" << m_channelId
                         << "setStageRunning(" << static_cast<int>(s) << ","
                         << run << "): WDSP Set*Run API for this stage is "
                         << "not yet declared in wdsp_api.h — deferred to "
                         << "3M-1b/3M-3a. No-op in 3M-1a.";
        return;
    }
}

// ---------------------------------------------------------------------------
// setConnection()
//
// Attach or detach the RadioConnection that will receive TX I/Q output.
// Non-owning: caller (RadioModel) owns the connection and TxChannel.
// Pass nullptr to detach before connection teardown.
//
// Thread safety: call from the main thread only.  driveOneTxBlock() reads
// m_connection under the timer (QTimer fires on the main thread event loop),
// so no synchronization is needed as long as this setter and the timer
// share the main thread.  If the timer is ever moved to a worker thread,
// add an atomic or mutex here.
// ---------------------------------------------------------------------------
void TxChannel::setConnection(RadioConnection* conn)
{
    m_connection = conn;
    qCDebug(lcDsp) << "TxChannel" << m_channelId
                   << "connection" << (conn ? "attached" : "detached");
}

// ---------------------------------------------------------------------------
// setMicRouter()
//
// Attach or detach the TxMicRouter used as fexchange2 input source.
// Non-owning: caller (RadioModel) owns the unique_ptr.
// In 3M-1a, NullMicSource provides zero-padded silence — functionally
// inert because gen1 PostGen overwrites the rsmpin input at TXA stage 22.
//
// Thread safety: see setConnection() note above.
// ---------------------------------------------------------------------------
void TxChannel::setMicRouter(TxMicRouter* router)
{
    m_micRouter = router;
    qCDebug(lcDsp) << "TxChannel" << m_channelId
                   << "mic router" << (router ? "attached" : "detached");
}

// ---------------------------------------------------------------------------
// driveOneTxBlock()
//
// Drive one fexchange2 call: pull m_inI.size() samples from the mic router,
// call fexchange2(channelId, inI, inQ, outI, outQ, &error), interleave the
// output, and push to m_connection->sendTxIq() (the SPSC ring producer side).
//
// Called by m_txProductionTimer on every 5 ms tick while m_running is true.
//
// In 3M-1a TUNE-only mode:
//   - NullMicSource fills m_inI with zeros; m_inQ stays zero.
//   - WDSP gen1 PostGen (TXA stage 22, set by setTuneTone()) overwrites the
//     zero input with the TUNE sine carrier before bp0 processes it.
//   - fexchange2 output is the modulated I/Q stream ready for the radio DUC.
//
// Guard conditions (all return early):
//   - !m_running: timer should not fire, but guard in case of race at stop.
//   - !m_connection: connection not yet injected or already detached.
//   - m_inI.empty(): buffers not allocated (should never happen after ctor).
//
// WDSP guard: without HAVE_WDSP, fexchange2 is not available; the method
// pushes zeros (silence) to m_connection->sendTxIq() via the pre-zeroed
// m_outInterleaved buffer. This keeps the ring populated in stub builds
// (unit tests, CI without WDSP) so connection-side drain path stays warm.
// ---------------------------------------------------------------------------
void TxChannel::driveOneTxBlock()
{
    if (!m_running || !m_connection) {
        return;
    }

    const int inN  = m_inputBufferSize;
    const int outN = m_outputBufferSize;
    if (inN == 0 || m_inI.empty()) {
        return;
    }

    // Pull mic samples from the router into Iin (size == inN == in_size).
    // In 3M-1a: NullMicSource writes inN zeros to m_inI (functionally inert
    // during TUNE because gen1 PostGen overwrites the rsmpin stage input).
    // m_inQ stays zero throughout (real mono mic input; Q=0 for real signals).
    if (m_micRouter) {
        // pullSamples fills the I buffer with mono mic samples.
        // Q channel for real mic input is zero (WDSP handles SSB modulation).
        m_micRouter->pullSamples(m_inI.data(), inN);
        std::fill(m_inQ.begin(), m_inQ.end(), 0.0f);
    } else {
        // No mic router — send pure silence to WDSP input.
        // gen1 PostGen will still inject the TUNE carrier downstream.
        std::fill(m_inI.begin(), m_inI.end(), 0.0f);
        std::fill(m_inQ.begin(), m_inQ.end(), 0.0f);
    }

#ifdef HAVE_WDSP
    // fexchange2 — drives the WDSP TX channel.
    //
    // CRITICAL: Iin/Qin must be exactly in_size (== m_inputBufferSize) samples;
    // Iout/Qout must be exactly out_size (== m_outputBufferSize) samples.
    // fexchange2 silently produces no output if the sizes don't match the channel
    // parameters set in OpenChannel().
    //
    // gen1 PostGen (set by setTuneTone()) injects the TUNE tone after bp0;
    // output is the modulated I/Q stream ready for the radio's DUC.
    //
    // From Thetis wdsp/iobuffs.c [v2.10.3.13] — fexchange2 prototype:
    //   void fexchange2(int id, double* Iin, double* Qin,
    //                   double* Iout, double* Qout, int* error)
    // NereusSDR uses the float variant declared in wdsp_api.h (INREAL=float).
    int error = 0;
    fexchange2(m_channelId,
               m_inI.data(), m_inQ.data(),   // inN samples each
               m_outI.data(), m_outQ.data(),  // outN samples each
               &error);
    if (error != 0) {
        qCWarning(lcDsp) << "TxChannel" << m_channelId
                         << "fexchange2 error" << error;
        return;
    }
#endif // HAVE_WDSP

    // Interleave outN I/Q pairs into [I0,Q0,I1,Q1,...] for sendTxIq's layout.
    // Without HAVE_WDSP, m_outI/m_outQ were never written so m_outInterleaved
    // stays all-zeros (silence stream) — keeps the ring warm in stub builds.
    for (int i = 0; i < outN; ++i) {
        m_outInterleaved[2 * i]     = m_outI[i];
        m_outInterleaved[2 * i + 1] = m_outQ[i];
    }

    // Push to connection's SPSC ring (producer side).
    // The connection thread's E.6 drain (P2 port 1029 / P1 EP2 zones)
    // consumes the ring and emits to UDP.
    // sendTxIq(iq, n): n = number of complex samples; buffer has 2*n floats.
    m_connection->sendTxIq(m_outInterleaved.data(), outN);
}

} // namespace NereusSDR
