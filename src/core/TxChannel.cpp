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
//   2026-04-27 — setTxMode(DSPMode) / setTxBandpass(int, int) /
//                 setSubAmMode(int) implemented by J.J. Boyd (KG4VCF) during
//                 3M-1b Task D.2 (per-mode TXA config setters). AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-04-27 — setVoxRun(bool) / setVoxAttackThreshold(double) /
//                 setVoxHangTime(double) / setAntiVoxRun(bool) /
//                 setAntiVoxGain(double) implemented by J.J. Boyd (KG4VCF)
//                 during 3M-1b Task D.3 (VOX/anti-VOX WDSP wrappers).
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-04-27 — setStageRunning() expanded with explicit cases for
//                 MicMeter, AlcMeter, AmMod, FmMod by J.J. Boyd (KG4VCF)
//                 during 3M-1b Task D.4. All 4 new cases are documented
//                 no-ops: MicMeter/AlcMeter have no public WDSP Run setter
//                 (meter.c:36-57 [v2.10.3.13]); AmMod/FmMod run controlled
//                 only by SetTXAMode() (TXA.c:753-789 [v2.10.3.13]).
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-04-27 — emit sip1OutputReady(m_outI.data(), m_outputBufferSize)
//                 added inside driveOneTxBlock() after sendTxIq() by
//                 J.J. Boyd (KG4VCF) during 3M-1b Task D.5. DirectConnection-
//                 only contract; full Sip1-stage tap deferred to 3M-3.
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-04-27 — setMicPreamp(double) / recomputeTxAPanelGain1() implemented
//                 by J.J. Boyd (KG4VCF) during 3M-1b Task D.6 (mic-mute path).
//                 NaN-aware idempotent guard; HAVE_WDSP + null-guard consistent
//                 with D.3. SetTXAPanelGain1 called with 0.0 when mute=true.
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-04-27 — getTxMicMeter() / getAlcMeter() (2 wired: GetTXAMeter with
//                 TXA_MIC_PK=0 / TXA_ALC_PK=12) + 4 deferred stubs returning
//                 0.0f (getEqMeter / getLvlrMeter / getCfcMeter / getCompMeter)
//                 implemented by J.J. Boyd (KG4VCF) during 3M-1b Task D.7.
//                 Constants sourced from Thetis wdsp/TXA.h:49-69 [v2.10.3.13].
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-04-28 — Phase 3M-1c E.1 — push-driven TX pump.  driveOneTxBlock()
//                 now accepts (const float* samples, int frames) and is wired
//                 by RadioModel (Phase L) to AudioEngine::micBlockReady via
//                 Qt::DirectConnection.  Removed m_txProductionTimer + 5 ms
//                 QTimer pull-model + the 1b353f4 partial-read zero-fill
//                 workaround (the push model has no underrun pathology by
//                 construction).  m_micRouter retained for future Radio-mic
//                 source path (the PC-mic path no longer pulls from it).
//                 J.J. Boyd (KG4VCF), AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-04-29 — Phase 3M-1c E.2-E.6 — TXA PostGen wrapper setters (12 methods)
//                 implemented by J.J. Boyd (KG4VCF):
//                   E.2: setTxPostGenMode(int)
//                   E.3: setTxPostGenTT{Freq1,Freq2,Mag1,Mag2}(double)
//                   E.4: setTxPostGenTTPulseToneFreq{1,2}(double),
//                        setTxPostGenTTPulseMag{1,2}(double)
//                   E.5: setTxPostGenTTPulse{Freq(int),DutyCycle(double),
//                        Transition(double)}
//                   E.6: setTxPostGenRun(bool)
//                 Each is a thin pass-through wrapper to the underlying
//                 SetTXAPostGen* WDSP function.  Split-property setters
//                 (Freq1/Freq2 / Mag1/Mag2) cache the partner value in
//                 m_postGen* cache fields so the combined WDSP call uses
//                 both — matching Thetis radio.cs:3697-4032 [v2.10.3.13]
//                 `tx_postgen_tt_*_dsp` cache pattern.  The same null-guard
//                 sentinel (txa[ch].rsmpin.p == nullptr) used throughout
//                 this class protects unit-test builds that link WDSP but
//                 don't call OpenChannel.  AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-04-29 — Stage-2 review fix I1 — refreshed v2-era doc comments
//                 (fexchange2 / 256-block / 5 ms cadence / kPumpIntervalMs /
//                 onPumpTick / QTimer references) to reflect the v3
//                 redesign (fexchange0 / 64-block / semaphore-wake /
//                 TxMicSource-driven cadence).  No behavioural change;
//                 documentation refresh only.  J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-04-30 — Phase 3M-3a-ii Batch 1.6 — 3 TX phase-rotator parameter
//                 wrappers implemented by J.J. Boyd (KG4VCF):
//                   setTxPhrotCornerHz(double) → SetTXAPHROTCorner
//                   setTxPhrotNstages(int)     → SetTXAPHROTNstages
//                   setTxPhrotReverse(bool)    → SetTXAPHROTReverse
//                 Thin pass-through wrappers over wdsp/iir.c:675-703
//                 [v2.10.3.13] on top of the WDSP boot defaults shipped
//                 in 3M-1c.  3M-3a-i shipped Stage::PhRot + the
//                 setStageRunning(Stage::PhRot,...) arm calling
//                 SetTXAPHROTRun; the parameter setters were deferred to
//                 this batch because their persistence keys live in the
//                 Thetis tpDSPCFC tab alongside the CFC controls.
//                 AI-assisted transformation via Anthropic Claude Code.
// =================================================================

#include "TxChannel.h"  // brings in WdspTypes.h (DSPMode)
#include "LogCategories.h"
#include "RadioConnection.h"
#include "TxMicRouter.h"

#include <algorithm>
#include <cmath>        // std::isnan — NaN sentinel for double idempotent guards (D.3)
#include <cstring>
#include <stdexcept>

// WDSP API declarations (SetTXAPostGen*, fexchange0, fexchange2, etc.) —
// guarded by HAVE_WDSP internally.  Include unconditionally; the header
// guards itself.  v3 callsites use fexchange0; v2 used fexchange2.  Both
// are still in scope because the legacy driveOneTxBlock(float*, int)
// overload + tests retain coverage.
#include "wdsp_api.h"

// Direct WDSP struct access for stage-Run introspection.
// WDSP declares "extern struct _txa txa[];" in TXA.h; we access
// txa[channel].stagename.p->run directly because no GetTXA*Run API exists.
// From Thetis wdsp/TXA.h:165 [v2.10.3.13] — extern struct _txa txa[];
#ifdef HAVE_WDSP
extern "C" {
#include "../../third_party/wdsp/src/TXA.h"

// Phase 3M-1c TX pump v3: VOX defensive guards.  Need to read pdexp[id]
// to detect whether create_dexp has been called for the channel.
// dexp.h is not include-clean (its struct depends on Windows-isms that
// linux_port.h shims for the WDSP build but aren't visible here), so
// we forward-declare just the bits we need.  pdexp is a `DEXP[]` of
// pointer-to-struct; we only test whether the entry is non-null.
// From Thetis wdsp/dexp.h:104 [v2.10.3.13]:
//   extern DEXP pdexp[];
// (DEXP is `typedef struct _dexp *DEXP;` at dexp.h:102.)
struct _dexp;
typedef struct _dexp *DEXP;
extern DEXP pdexp[];
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
// inputBufferSize:  fexchange0 in/out pairs == OpenChannel in_size (default 64
//                   in v3; was 256 in v2 prior to 2026-04-29).
// outputBufferSize: fexchange0 output pairs == in_size × out_rate / in_rate.
//   At 48 kHz out: 64.  At 192 kHz out (P2 Saturn): 64 × 4 = 256.
//
// CRITICAL: fexchange0 requires the in/out buffers to be exactly
// (in_size × 2) and (out_size × 2) doubles respectively.  Calling it with
// wrong-sized buffers produces no output (silent error).
//
// v3 size of 64 mirrors Thetis getbuffsize(48000) at cmsetup.c:106-110
// [v2.10.3.13] exactly.
//
// From Thetis wdsp/TXA.c:31-479 [v2.10.3.13] — create_txa() signal flow.
// From Thetis wdsp/cmaster.c:177-190 [v2.10.3.13] — OpenChannel in_size / ch_outrate.
// From Thetis wdsp/iobuffs.c:464-516 [v2.10.3.13] — fexchange0 prototype.
// ---------------------------------------------------------------------------
TxChannel::TxChannel(int channelId,
                     int inputBufferSize,
                     int outputBufferSize,
                     QObject* parent)
    : QObject(parent)
    // Init order must match declaration order (-Wreorder-ctor):
    // m_inputBufferSize, m_outputBufferSize, m_channelId.
    , m_inputBufferSize(inputBufferSize > 0 ? inputBufferSize : 64)
    , m_outputBufferSize(outputBufferSize > 0 ? outputBufferSize : 64)
    , m_channelId(channelId)
{
    // Allocate fexchange0 I/O buffers at correct sizes.
    //
    // Phase 3M-1c TX pump v3: switched from fexchange2 (separate float
    // I/Q buffers) to fexchange0 (interleaved double I/Q buffers) to
    // match Thetis cmaster.c:389 [v2.10.3.13] callsite exactly.  The
    // ratio is the same — m_inputBufferSize and m_outputBufferSize are
    // pair counts; the underlying storage is 2× that in doubles.
    //
    // From Thetis wdsp/iobuffs.c:464-516 [v2.10.3.13] — fexchange0 prototype.
    // From Thetis wdsp/cmaster.c:179-183 [v2.10.3.13] — in_size, ch_outrate.
    m_in.assign(static_cast<size_t>(m_inputBufferSize) * 2, 0.0);
    m_out.assign(static_cast<size_t>(m_outputBufferSize) * 2, 0.0);
    m_outInterleavedFloat.assign(static_cast<size_t>(m_outputBufferSize) * 2, 0.0f);
    m_outIFloatScratch.assign(static_cast<size_t>(m_outputBufferSize), 0.0f);

    // Phase 3M-1c TX pump v3 (2026-04-29): semaphore-wake.  No QTimer.
    // TxWorkerThread::run blocks on TxMicSource::waitForBlock; the
    // radio's mic-frame stream IS the cadence source — at 48 kHz mic
    // rate with 64-frame blocks the loop wakes every ~1.33 ms and runs
    // fexchange0 once per drained block.  Block size is 64 frames
    // end-to-end (Thetis getbuffsize(48000) parity at cmsetup.c:106-110
    // [v2.10.3.13]).  PC mic override splices PC samples on top of the
    // radio mic per Thetis cmaster.c:379 [v2.10.3.13]
    // (asioIN(pcm->in[stream]) pattern).
    //
    // Replaces the deleted D.1 720-sample accumulator + E.1 push slot +
    // L.4 MicReBlocker + bench-fix-A AudioEngine pump + bench-fix-B
    // TxChannel silence timer (all part of the v2 design that was
    // scrapped 2026-04-29 in favour of the Thetis-faithful semaphore-
    // wake architecture).  v2-history: the previous attempt drove
    // driveOneTxBlock at a 5 ms QTimer cadence with 256-sample blocks
    // and zero-fill on partial pull — superseded.
    //
    // Plan: docs/architecture/phase3m-1c-tx-pump-architecture-plan.md

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
    // Update the run-state atomic.  Phase 3M-1c TX pump v3:
    // TxWorkerThread::run drains a block from TxMicSource at the radio's
    // natural mic-frame cadence (~1.33 ms per 64-frame block at 48 kHz)
    // and calls driveOneTxBlockFromInterleaved unconditionally;
    // driveOneTxBlockFromInterleaved early-returns on !m_running, so
    // toggling this flag is sufficient to gate fexchange0.  No timer to
    // start/stop here — the worker runs as long as TxMicSource is
    // running, and the !m_running guard handles RX↔TX transitions.
    // release ordering pairs with driveOneTxBlockFromInterleaved's
    // acquire load.
    m_running.store(on, std::memory_order_release);

    qCDebug(lcDsp) << "TxChannel" << m_channelId
                   << (on ? "started (channel ON, worker-thread pump armed)"
                          : "stopped (channel OFF, drain, worker-thread pump idle)");

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
// run flag; MicMeter/AlcMeter — always-on with no public Run setter;
// AmMod/FmMod — run managed by SetTXAMode only; remaining
// meters/Iqc/Calcc/Alc/Bp* — added in later tasks)
// log a warning or debug note and return without calling WDSP.
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

    // compressor (stage 14): TX speech compressor (CPDR). Side effect:
    // SetTXACompressorRun calls TXASetupBPFilters at compress.c:106, which
    // rebuilds bp1 + gated bp2 (TXA.c:843-868 [v2.10.3.13]).  Activated by
    // 3M-3a-ii Batch 1.  Mirrors setTxCpdrOn (which is the preferred
    // public-API surface; this case arm is available for callers that
    // already speak the Stage::* idiom).
    // From Thetis wdsp/compress.c:99-109 [v2.10.3.13] and compress.h:60.
    case Stage::Compressor:
#ifdef HAVE_WDSP
        SetTXACompressorRun(m_channelId, r);  // compress.c:100 [v2.10.3.13]
#endif
        return;

    // osctrl (stage 16): CESSB overshoot control.  Side effect:
    // SetTXAosctrlRun calls TXASetupBPFilters at osctrl.c:148, which
    // rebuilds bp2.  bp2.run is only set when both compressor.run AND
    // osctrl.run are 1 (TXA.c:843-868 [v2.10.3.13]).  Activated by 3M-3a-ii
    // Batch 1.  Mirrors setTxCessbOn.
    // From Thetis wdsp/osctrl.c:142-150 [v2.10.3.13].
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

    // cfcomp (stage 11): continuous frequency compander (CFC).  Activated
    // by 3M-3a-ii Batch 1.  Mirrors setTxCfcRunning.
    // From Thetis wdsp/cfcomp.c:632-641 [v2.10.3.13].
    case Stage::CfComp:
#ifdef HAVE_WDSP
        SetTXACFCOMPRun(m_channelId, r);   // cfcomp.c:632 [v2.10.3.13]
#endif
        return;

    // leveler (stage 9): slow speech-leveling AGC. Activated by 3M-3a-i.
    // From Thetis wdsp/wcpAGC.c:613-618 [v2.10.3.13].
    case Stage::Leveler:
#ifdef HAVE_WDSP
        SetTXALevelerSt(m_channelId, r);   // wcpAGC.c:613 [v2.10.3.13]
#endif
        return;

    // alc (stage 19): final clip protection (always-on per Thetis schema —
    // there is no chkALCEnabled checkbox in the Thetis UI; WdspEngine boot
    // sets SetTXAALCSt(1) at WdspEngine.cpp:438).  Exposed as a Run case
    // here because WDSP itself does have a Run setter (SetTXAALCSt) and
    // some test paths or future Setup → Audio extras might want to flip
    // it.  Documented as "intentionally settable but Thetis never flips it".
    // From Thetis wdsp/wcpAGC.c:570-575 [v2.10.3.13].
    case Stage::Alc:
#ifdef HAVE_WDSP
        SetTXAALCSt(m_channelId, r);       // wcpAGC.c:570 [v2.10.3.13]
#endif
        return;

    // ── D.4: MicMeter and AlcMeter — always-on meter stages ─────────────────
    //
    // WDSP analysis (v2.10.3.13): create_meter() (wdsp/meter.c:36-57) takes
    // a `run` arg (1 for micmeter, 1 for alcmeter) and a `prun` pointer.
    // xmeter() gates on `a->run && srun`; `srun` is *(prun) if prun != 0.
    // For micmeter, prun = &txa[ch].panel.p->run (TXA.c:80-93): the meter's
    // secondary gate is panel.run, not a separately settable extern function.
    // No PORT-exported SetTXAMicMeterRun / SetTXAAlcMeterRun function exists
    // in wdsp/meter.c — the file exports only GetRXAMeter and GetTXAMeter.
    // These stages are always-on (run=1 in create_txa) and have no public
    // Run setter — documented no-op.
    //
    // From Thetis wdsp/TXA.c:80-93   [v2.10.3.13] — micmeter created with run=1.
    // From Thetis wdsp/TXA.c:379-392 [v2.10.3.13] — alcmeter created with run=1.
    // From Thetis wdsp/meter.c:36-57  [v2.10.3.13] — no public Run setter.

    case Stage::MicMeter:
        // No public WDSP Run setter. micmeter.run = 1 always (create_txa TXA.c:80-93).
        // The meter's srun secondary gate is controlled by panel.run (*(prun)).
        // Call SetTXAPanelRun to gate the mic path via panel, not directly here.
        qCDebug(lcDsp) << "TxChannel" << m_channelId
                       << "setStageRunning(MicMeter," << run
                       << "): no public WDSP Run setter (meter.c:36-57"
                          " [v2.10.3.13]); micmeter.run=1 always-on, gated"
                          " by panel.run — use Stage::Panel to gate the mic path.";
        return;

    case Stage::AlcMeter:
        // No public WDSP Run setter. alcmeter.run = 1 always (create_txa TXA.c:379-392).
        qCDebug(lcDsp) << "TxChannel" << m_channelId
                       << "setStageRunning(AlcMeter," << run
                       << "): no public WDSP Run setter (meter.c:36-57"
                          " [v2.10.3.13]); alcmeter.run=1 always-on — no-op.";
        return;

    // ── D.4: AmMod and FmMod — run controlled via SetTXAMode() only ─────────
    //
    // WDSP analysis (v2.10.3.13): ammod.run and fmmod.run are set ONLY inside
    // SetTXAMode() (wdsp/TXA.c:753-789). SetTXAMode resets both to 0, then sets
    // ammod.run=1 for TXA_AM/SAM/DSB/AM_LSB/AM_USB, or fmmod.run=1 for TXA_FM
    // (TXA.c:759-785). This also calls TXASetupBPFilters() to update bp0/bp1/bp2
    // atomically. No standalone SetTXAamModRun or SetTXAfmModRun PORT function
    // exists in wdsp/ammod.c or wdsp/fmmod.c — the files export only parameter
    // setters (SetTXAAMCarrierLevel, SetTXAFMDeviation, SetTXACTCSSRun, etc.).
    //
    // Correct call sequence for AM/FM modes: setTxMode(DSPMode::AM/FM) via
    // TxChannel::setTxMode(), which calls SetTXAMode() and handles the full
    // pipeline reconfiguration atomically.
    //
    // From Thetis wdsp/TXA.c:753-789 [v2.10.3.13] — SetTXAMode sets ammod/fmmod.run.
    // From Thetis wdsp/ammod.c:29-41  [v2.10.3.13] — no public Run setter.
    // From Thetis wdsp/fmmod.c:42-65  [v2.10.3.13] — no public Run setter.

    case Stage::AmMod:
        // ammod.run is managed exclusively by SetTXAMode() — no standalone Run setter.
        // Use setTxMode(DSPMode::AM) / setTxMode(DSPMode::DSB) etc. instead.
        qCWarning(lcDsp) << "TxChannel" << m_channelId
                         << "setStageRunning(AmMod," << run
                         << "): ammod.run is controlled only by SetTXAMode()"
                            " (TXA.c:753-789 [v2.10.3.13]). No standalone"
                            " SetTXAamModRun function exists in WDSP."
                            " Use setTxMode(DSPMode::AM/DSB/...) instead — no-op.";
        return;

    case Stage::FmMod:
        // fmmod.run is managed exclusively by SetTXAMode() — no standalone Run setter.
        // Use setTxMode(DSPMode::FM) instead.
        qCWarning(lcDsp) << "TxChannel" << m_channelId
                         << "setStageRunning(FmMod," << run
                         << "): fmmod.run is controlled only by SetTXAMode()"
                            " (TXA.c:753-789 [v2.10.3.13]). No standalone"
                            " SetTXAfmModRun function exists in WDSP."
                            " Use setTxMode(DSPMode::FM) instead — no-op.";
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

    // Remaining deferred stages — Set*Run API exists in WDSP but is not yet
    // declared in wdsp_api.h. Each will get its own explicit case arm when
    // wired in 3M-3a-ii / 3M-4. Stages that hit this branch:
    //   EqMeter / LvlrMeter / CfcMeter / CompMeter / OutMeter / Sip1 /
    //   Calcc / Iqc / Bp0 / Bp1 / Bp2 / PreEmph.
    // AmMod / FmMod / MicMeter / AlcMeter all have explicit case arms above
    // (D.4); Leveler / Alc added by 3M-3a-i Batch 1; none should reach default:.
    default:
        qCWarning(lcDsp) << "TxChannel" << m_channelId
                         << "setStageRunning(" << static_cast<int>(s) << ","
                         << run << "): WDSP Set*Run API for this stage is "
                         << "not yet declared in wdsp_api.h — deferred to "
                         << "3M-3a/3M-4. No-op.";
        return;
    }
}

// ---------------------------------------------------------------------------
// setTxMode()
//
// Set the TXA channel's DSP mode.
//
// Porting from Thetis radio.cs:2670-2696 [v2.10.3.13] — CurrentDSPMode setter,
// original C# logic (else-branch, all modes that are not AM/SAM):
//
//   else
//       WDSP.SetTXAMode(WDSP.id(thread, 0), value);
//
// For AM/SAM the Thetis setter dispatches through sub_am_mode first; that
// dispatch is setSubAmMode()'s job (deferred to 3M-3b).  setTxMode() calls
// SetTXAMode with the raw mode integer — correct for all non-AM/SAM modes,
// and correct as a base call even for AM/SAM (SetTXAMode wires the WDSP
// pipeline; the sub-mode refines the sideband selection afterwards).
//
// WDSP SetTXAMode wires:
//   - Activates ammod (stage 20) for AM/DSB modes.
//   - Activates fmmod (stage 21) for FM modes.
//   - Activates preemph (stage 8) for FM.
//   - Calls TXASetupBPFilters() to reconfigure bp0/bp1/bp2 to match mode.
// From Thetis wdsp/TXA.c:753-789 [v2.10.3.13].
//
// In unit-test builds that link WDSP but haven't called OpenChannel, the
// txa[].rsmpin.p null-guard fires and the call is a no-op — same pattern as
// setTuneTone() and setRunning().
// ---------------------------------------------------------------------------
void TxChannel::setTxMode(DSPMode mode)
{
#ifdef HAVE_WDSP
    // From Thetis radio.cs:2670-2696 [v2.10.3.13]
    if (txa[m_channelId].rsmpin.p == nullptr) {
        return;  // channel not yet opened (unit-test path)
    }
    SetTXAMode(m_channelId, static_cast<int>(mode));
#else
    Q_UNUSED(mode);
#endif
}

// ---------------------------------------------------------------------------
// setTxBandpass()
//
// Set the TXA channel's bandpass filter cutoff frequencies.
//
// Porting from Thetis radio.cs:2730-2780 [v2.10.3.13] — SetTXFilter /
// TXFilterLow / TXFilterHigh setters, original C# logic:
//
//   public void SetTXFilter(int low, int high)
//   {
//       ...
//       WDSP.SetTXABandpassFreqs(WDSP.id(thread, 0), low, high);
//       ...
//   }
//
// NereusSDR translation: no change-detection guard (Thetis checks low != dsp
// or high != dsp; NereusSDR's callers are responsible for avoiding unnecessary
// calls, so we call WDSP unconditionally — simpler and correct for 3M-1b
// where the same filter values may be set at channel-start to ensure the
// bandpass is correct regardless of prior state).
//
// IQ-space conventions documented in TxChannel.h; WDSP maps the pair to
// bp0/bp1/bp2 cutoffs internally.
//
// From Thetis wdsp/TXA.c:792-799 [v2.10.3.13] — SetTXABandpassFreqs.
// ---------------------------------------------------------------------------
void TxChannel::setTxBandpass(int lowHz, int highHz)
{
#ifdef HAVE_WDSP
    // From Thetis radio.cs:2730-2780 [v2.10.3.13]
    if (txa[m_channelId].rsmpin.p == nullptr) {
        return;  // channel not yet opened (unit-test path)
    }
    SetTXABandpassFreqs(m_channelId,
                        static_cast<double>(lowHz),
                        static_cast<double>(highHz));
#else
    Q_UNUSED(lowHz);
    Q_UNUSED(highHz);
#endif
}

// ---------------------------------------------------------------------------
// setSubAmMode()
//
// DEFERRED TO 3M-3b.  Throws std::logic_error unconditionally.
//
// The full dispatch logic (porting from Thetis radio.cs:2699-2728
// [v2.10.3.13] — SubAMMode setter) is:
//
//   switch (sub_am_mode)
//   {
//       case 0: WDSP.SetTXAMode(..., DSPMode.AM);     break;  // double-sided
//       case 1: WDSP.SetTXAMode(..., DSPMode.AM_LSB); break;
//       case 2: WDSP.SetTXAMode(..., DSPMode.AM_USB); break;
//   }
//
// In 3M-1b: AM/SAM TX is gated off by BandPlanGuard. This stub exists so
// the API surface is stable for 3M-3b, and so any accidental 3M-1b caller
// surfaces immediately as a crash rather than silent misbehaviour.
//
// From Thetis radio.cs:2699-2728 [v2.10.3.13] — SubAMMode setter.
// ---------------------------------------------------------------------------
[[noreturn]] void TxChannel::setSubAmMode(int /*sub*/)
{
    // Defer per Plan §3 Phase D.2 — 3M-3b will activate AM/SAM/DSB TX.
    // Throws so accidental 3M-1b callers surface immediately.
    throw std::logic_error(
        "TxChannel::setSubAmMode deferred to 3M-3b — "
        "AM/SAM TX is not enabled in 3M-1b. "
        "Full SubAMMode dispatch (radio.cs:2699-2728 [v2.10.3.13]) "
        "will be ported in Phase 3M-3b.");
}

// ---------------------------------------------------------------------------
// setVoxRun()
//
// Enable or disable VOX gating inside the WDSP DEXP expander.
//
// Porting from Thetis cmaster.cs:199-200 [v2.10.3.13] — SetDEXPRunVox DLL import:
//   [DllImport("wdsp.dll", EntryPoint = "SetDEXPRunVox", ...)]
//   public static extern void SetDEXPRunVox(int id, bool run);
//
// Idempotent: skips the WDSP call if `run` equals the last value stored in
// m_voxRunLast.  Bool guard is a plain `==` comparison (no NaN issue).
//
// WDSP signature takes `int` for the bool parameter (0=false, 1=true);
// the cast is explicit.
//
// From Thetis wdsp/dexp.c:616 [v2.10.3.13] — SetDEXPRunVox implementation.
// ---------------------------------------------------------------------------
void TxChannel::setVoxRun(bool run)
{
    if (run == m_voxRunLast) return;  // idempotent guard
    m_voxRunLast = run;
#ifdef HAVE_WDSP
    // From Thetis cmaster.cs:199-200 [v2.10.3.13]
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // Phase 3M-1c TX pump v3: pdexp[ch] null-guard.
    // Thetis create_xmtr (cmaster.c:130-157 [v2.10.3.13]) calls
    // create_dexp BEFORE OpenChannel, so pdexp[i] is non-null whenever
    // rsmpin.p is non-null.  NereusSDR ports OpenChannel but NOT
    // create_dexp (deferred follow-up), so the txa.rsmpin.p check
    // alone does not imply pdexp[ch] is allocated.  Without this
    // guard SetDEXPRunVox(id, ...) dereferences pdexp[id] (dexp.c:619)
    // and crashes.
    if (pdexp[m_channelId] == nullptr) return;
    SetDEXPRunVox(m_channelId, run ? 1 : 0);
#else
    Q_UNUSED(run);
#endif
}

// ---------------------------------------------------------------------------
// setVoxAttackThreshold()
//
// Set the VOX trigger threshold (linear amplitude).
//
// Porting from Thetis cmaster.cs:187-188 [v2.10.3.13] — SetDEXPAttackThreshold:
//   [DllImport("wdsp.dll", EntryPoint = "SetDEXPAttackThreshold", ...)]
//   public static extern void SetDEXPAttackThreshold(int id, double thresh);
//
// Caller (MoxController, Phase H.2) is responsible for mic-boost-aware scaling.
// This wrapper calls WDSP unconditionally for the given threshold value.
//
// Idempotent guard: uses NaN-aware first-call sentinel.  m_voxAttackThresholdLast
// is initialised to quiet_NaN so the first call (whatever value) always passes.
// Subsequent calls with the same value skip WDSP.  Exact IEEE 754 `==` is used
// for double comparison — callers round-trip the same value they stored; partial
// floating-point drift is not expected here (this is a direct user-set parameter).
//
// From Thetis wdsp/dexp.c:544 [v2.10.3.13] — SetDEXPAttackThreshold impl.
// ---------------------------------------------------------------------------
void TxChannel::setVoxAttackThreshold(double thresh)
{
    if (!std::isnan(m_voxAttackThresholdLast) && thresh == m_voxAttackThresholdLast) return;
    m_voxAttackThresholdLast = thresh;
#ifdef HAVE_WDSP
    // From Thetis cmaster.cs:187-188 [v2.10.3.13]
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // Phase 3M-1c TX pump v3: pdexp[ch] null-guard — see setVoxRun for the full rationale.
    if (pdexp[m_channelId] == nullptr) return;
    SetDEXPAttackThreshold(m_channelId, thresh);
#else
    Q_UNUSED(thresh);
#endif
}

// ---------------------------------------------------------------------------
// setVoxHangTime()
//
// Set the VOX hold/hang time (seconds).
//
// Porting from Thetis cmaster.cs:178-179 [v2.10.3.13] — SetDEXPHoldTime:
//   [DllImport("wdsp.dll", EntryPoint = "SetDEXPHoldTime", ...)]
//   public static extern void SetDEXPHoldTime(int id, double time);
//
// WDSP terminology note: the DEXP parameter is "HoldTime" (wdsp/dexp.c:505);
// Thetis exposes this as "VOXHangTime" (console.cs:14706).  There is no
// SetDEXPHangTime function in the WDSP source.  NereusSDR names the public
// method setVoxHangTime() to match Thetis semantics but calls SetDEXPHoldTime
// internally.
//
// Callers pass seconds.  Thetis converts ms → s at the callsite:
//   cmaster.SetDEXPHoldTime(0, (double)udDEXPHold.Value / 1000.0)
//   — Thetis setup.cs:18899 [v2.10.3.13]
// NereusSDR callers are responsible for the same conversion.
//
// Idempotent guard: NaN-aware first-call sentinel (same pattern as
// setVoxAttackThreshold above).
//
// From Thetis wdsp/dexp.c:505 [v2.10.3.13] — SetDEXPHoldTime impl.
// ---------------------------------------------------------------------------
void TxChannel::setVoxHangTime(double seconds)
{
    if (!std::isnan(m_voxHangTimeLast) && seconds == m_voxHangTimeLast) return;
    m_voxHangTimeLast = seconds;
#ifdef HAVE_WDSP
    // From Thetis cmaster.cs:178-179 [v2.10.3.13]
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // Phase 3M-1c TX pump v3: pdexp[ch] null-guard — see setVoxRun for rationale.
    if (pdexp[m_channelId] == nullptr) return;
    SetDEXPHoldTime(m_channelId, seconds);
#else
    Q_UNUSED(seconds);
#endif
}

// ---------------------------------------------------------------------------
// setAntiVoxRun()
//
// Enable or disable anti-VOX side-chain cancellation.
//
// Porting from Thetis cmaster.cs:208-209 [v2.10.3.13] — SetAntiVOXRun DLL import:
//   [DllImport("wdsp.dll", EntryPoint = "SetAntiVOXRun", ...)]
//   public static extern void SetAntiVOXRun(int id, bool run);
//
// Idempotent guard: plain bool `==` comparison.
//
// From Thetis wdsp/dexp.c:657 [v2.10.3.13] — SetAntiVOXRun impl.
// ---------------------------------------------------------------------------
void TxChannel::setAntiVoxRun(bool run)
{
    if (run == m_antiVoxRunLast) return;  // idempotent guard
    m_antiVoxRunLast = run;
#ifdef HAVE_WDSP
    // From Thetis cmaster.cs:208-209 [v2.10.3.13]
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // Phase 3M-1c TX pump v3: pdexp[ch] null-guard — see setVoxRun for rationale.
    // Anti-VOX setters live inside the same DEXP struct as VOX setters
    // (dexp.c:657 SetAntiVOXRun dereferences pdexp[id]), so the same guard
    // applies here.
    if (pdexp[m_channelId] == nullptr) return;
    SetAntiVOXRun(m_channelId, run ? 1 : 0);
#else
    Q_UNUSED(run);
#endif
}

// ---------------------------------------------------------------------------
// setAntiVoxGain()
//
// Set the anti-VOX side-chain coupling gain.
//
// Porting from Thetis cmaster.cs:211-212 [v2.10.3.13] — SetAntiVOXGain DLL import:
//   [DllImport("wdsp.dll", EntryPoint = "SetAntiVOXGain", ...)]
//   public static extern void SetAntiVOXGain(int id, double gain);
//
// Idempotent guard: NaN-aware first-call sentinel (same pattern as
// setVoxAttackThreshold).
//
// From Thetis wdsp/dexp.c:688 [v2.10.3.13] — SetAntiVOXGain impl.
// ---------------------------------------------------------------------------
void TxChannel::setAntiVoxGain(double gain)
{
    if (!std::isnan(m_antiVoxGainLast) && gain == m_antiVoxGainLast) return;
    m_antiVoxGainLast = gain;
#ifdef HAVE_WDSP
    // From Thetis cmaster.cs:211-212 [v2.10.3.13]
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // Phase 3M-1c TX pump v3: pdexp[ch] null-guard — see setVoxRun for rationale.
    if (pdexp[m_channelId] == nullptr) return;
    SetAntiVOXGain(m_channelId, gain);
#else
    Q_UNUSED(gain);
#endif
}

// ---------------------------------------------------------------------------
// setConnection()
//
// Attach or detach the RadioConnection that will receive TX I/Q output.
// Non-owning: caller (RadioModel) owns the connection and TxChannel.
// Pass nullptr to detach before connection teardown.
//
// Thread safety: call from the main thread only.  driveOneTxBlock() reads
// m_connection on the audio thread (the slot is wired to AudioEngine::
// micBlockReady via Qt::DirectConnection per Phase 3M-1c E.1), so the
// raw pointer must be set before setRunning(true) and not torn down while
// the channel is active.  RadioModel orchestrates this ordering: it sets
// the connection first, sets running, and detaches only after stopping.
// If a future task makes the slot connection runtime-mutable while
// running, add an atomic or mutex here.
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
// driveOneTxBlock(const float* samples, int frames)  [3M-1c TX pump redesign]
//
// TX pump slot called by TxWorkerThread::onPumpTick at ~5 ms cadence
// (kPumpIntervalMs).  Runs synchronously on the worker thread.
//
// Behavior:
//   - samples != nullptr, frames == m_inputBufferSize:  copy into m_inI,
//     zero-fill m_inQ, dispatch fexchange2.
//   - samples == nullptr, frames == 0:                  silence path —
//     zero-fill m_inI/m_inQ and dispatch fexchange2 (TUNE-tone PostGen
//     output still reaches sendTxIq).  Used by tests today; production
//     callers always pass a kBlockFrames-sized buffer (TxWorkerThread
//     zero-fills the gap when AudioEngine::pullTxMic returns partial).
//   - samples != nullptr, frames != m_inputBufferSize:  contract
//     violation — log a qCWarning and return without dispatching.
//
// Block-size invariant matches Thetis cmaster.c:460-487 [v2.10.3.13]:
//   r1_outsize == xcm_insize == in_size
// — a single uniform block size end-to-end.  NereusSDR uses 256 (rather
// than Thetis's 64) due to the WDSP r2-ring divisibility constraint
// (2048 % 256 == 0).
//
// Guards:
//   - !m_running       — channel not active; return.
//   - !m_connection    — no recipient for sendTxIq; return.
//   - m_inI.empty()    — buffers not allocated (should never happen after ctor).
//
// WDSP guard: without HAVE_WDSP, fexchange2 is not available; the method
// pushes zeros (silence) to m_connection->sendTxIq() via the pre-zeroed
// m_outInterleaved buffer.  This keeps the ring populated in stub builds
// (unit tests, CI without WDSP) so connection-side drain path stays warm.
//
// History note: Phase 3M-1a G.1 introduced an m_txProductionTimer firing
// every 5 ms; PR #149 added a partial-read zero-fill workaround (commit
// 1b353f4) for the timer-vs-sample-rate race.  Phase 3M-1c E.1 dropped
// both in favour of an AudioEngine::micBlockReady push slot.  Phase
// 3M-1c TX pump architecture redesign (2026-04-29) replaces the push
// model with TxWorkerThread; the slot signature is unchanged.
// ---------------------------------------------------------------------------
void TxChannel::driveOneTxBlock(const float* samples, int frames)
{
    // ── float-buffer entry point (back-compat) ──────────────────────────────
    //
    // Phase 3M-1c TX pump v3: convert the float buffer into the interleaved
    // double layout that fexchange0 wants, then delegate to the canonical
    // driveOneTxBlockFromInterleaved entry point.  Q is always zero (real
    // mic input is mono).
    //
    // Three call shapes:
    //   1. (samples != null, frames == m_inputBufferSize) — mic-block path
    //   2. (samples == null, frames == 0)                 — silence path
    //   3. (samples != null, frames != m_inputBufferSize) — contract violation
    if (!m_running.load(std::memory_order_acquire) || !m_connection) {
        return;
    }
    const int inN = m_inputBufferSize;
    if (inN == 0 || m_in.empty()) {
        return;
    }
    if (samples != nullptr && frames != inN) {
        qCWarning(lcDsp) << "TxChannel" << m_channelId
                         << "driveOneTxBlock: frames=" << frames
                         << "does not match m_inputBufferSize=" << inN
                         << "; skipping fexchange0 (caller must re-block "
                            "samples to inputBufferSize before pushing).";
        return;
    }

    if (samples != nullptr) {
        for (int i = 0; i < inN; ++i) {
            m_in[static_cast<size_t>(2 * i + 0)] = static_cast<double>(samples[i]);
            m_in[static_cast<size_t>(2 * i + 1)] = 0.0;
        }
        // Note: pass nullptr to driveOneTxBlockFromInterleaved so it
        // treats m_in as already populated and skips the redundant copy.
        driveOneTxBlockFromInterleaved(m_in.data());
    } else {
        // Silence path — fill m_in with zeros, then dispatch.
        std::fill(m_in.begin(), m_in.end(), 0.0);
        driveOneTxBlockFromInterleaved(m_in.data());
    }
}

void TxChannel::driveOneTxBlockFromInterleaved(const double* interleavedIn)
{
    // ── TxWorkerThread canonical pump entry (3M-1c TX pump v3) ──────────────
    //
    // Mirrors Thetis cmaster.c:389 [v2.10.3.13] callsite of fexchange0:
    //   fexchange0 (chid (stream, 0), pcm->in[stream],
    //               pcm->xmtr[tx].out[0], &error);
    //
    // Block-size invariant matches Thetis cmaster.c:460-487 [v2.10.3.13]:
    //   r1_outsize == xcm_insize == in_size (= getbuffsize(48000) = 64).
    if (!m_running.load(std::memory_order_acquire) || !m_connection) {
        return;
    }

    const int inN  = m_inputBufferSize;
    const int outN = m_outputBufferSize;
    if (inN == 0 || m_in.empty()) {
        return;
    }

    // If the caller handed us an external buffer (not m_in.data()), copy
    // into m_in.  Identity comparison: TxWorkerThread will hand us its own
    // scratch buffer; the float-overload above hands us m_in.data() back
    // (no-op copy avoided).
    if (interleavedIn != nullptr && interleavedIn != m_in.data()) {
        std::memcpy(m_in.data(), interleavedIn, sizeof(double) * 2 * inN);
    } else if (interleavedIn == nullptr) {
        std::fill(m_in.begin(), m_in.end(), 0.0);
    }

#ifdef HAVE_WDSP
    // fexchange0 — drives the WDSP TX channel with interleaved double I/Q.
    //
    // CRITICAL: in/out must be exactly 2*in_size / 2*out_size doubles.
    // From Thetis wdsp/iobuffs.c:464-516 [v2.10.3.13] — fexchange0 prototype:
    //   void fexchange0 (int channel, double* in, double* out, int* error)
    int error = 0;
    fexchange0(m_channelId, m_in.data(), m_out.data(), &error);
    if (error != 0) {
        qCWarning(lcDsp) << "TxChannel" << m_channelId
                         << "fexchange0 error" << error;
        return;
    }
#endif // HAVE_WDSP

    // Convert m_out (interleaved double) → m_outInterleavedFloat for
    // sendTxIq, which still uses the float* SPSC ring layout.
    // Without HAVE_WDSP, m_out stays all-zeros (silence stream) — keeps
    // the ring warm in stub builds.
    for (int i = 0; i < outN; ++i) {
        m_outInterleavedFloat[static_cast<size_t>(2 * i + 0)] =
            static_cast<float>(m_out[static_cast<size_t>(2 * i + 0)]);
        m_outInterleavedFloat[static_cast<size_t>(2 * i + 1)] =
            static_cast<float>(m_out[static_cast<size_t>(2 * i + 1)]);
    }

    // Push to connection's SPSC ring (producer side).
    // sendTxIq(iq, n): n = number of complex samples; buffer has 2*n floats.
    m_connection->sendTxIq(m_outInterleavedFloat.data(), outN);

    // Siphon signal — MON path (3M-1b D.5).
    //
    // Emit post-SSB-modulator I-channel audio to any subscribed MON consumer
    // (AudioEngine::txMonitorBlockReady wired in Phase L).  Cache an I-only
    // float view in m_outIFloatScratch for the legacy float* signal API.
    //
    // CRITICAL: DirectConnection ONLY. The pointer is valid only during
    // this synchronous slot dispatch.  See sip1OutputReady doc-comment.
    for (int i = 0; i < outN; ++i) {
        m_outIFloatScratch[static_cast<size_t>(i)] =
            static_cast<float>(m_out[static_cast<size_t>(2 * i + 0)]);
    }
    emit sip1OutputReady(m_outIFloatScratch.data(), m_outputBufferSize);
}

// ---------------------------------------------------------------------------
// setMicPreamp()
//
// Set the mic preamp linear scalar, then push it to WDSP via
// recomputeTxAPanelGain1().  This is the NereusSDR translation of
// Audio.MicPreamp in Thetis (audio.cs:216-243 [v2.10.3.13]), which calls
// CMSetTXAPanelGain1 → SetTXAPanelGain1 every time the property is set.
//
// Porting from Thetis console.cs:28805-28817 [v2.10.3.13] — setAudioMicGain():
//   private void setAudioMicGain(double gain_db)
//   {
//       if (chkMicMute.Checked) // although it is called chkMicMute, Checked = mic in use
//       {
//           Audio.MicPreamp = Math.Pow(10.0, gain_db / 20.0); // convert to scalar
//           _mic_muted = false;
//       }
//       else
//       {
//           Audio.MicPreamp = 0.0;
//           _mic_muted = true;
//       }
//   }
// Note: chkMicMute.Checked == true means mic IS active (counter-intuitive naming).
// When chkMicMute.Checked == false, Audio.MicPreamp is set to 0.0 (mic silent).
//
// The dB→linear conversion (Math.Pow(10.0, gain_db / 20.0)) happens in
// TransmitModel::setMicGainDb (C.1), not here.  This method receives the
// already-scaled linear value — or 0.0 for the mute case.
//
// Idempotent guard: NaN-aware (same pattern as setVoxAttackThreshold / D.3).
// m_micPreampLast initialises to quiet_NaN so the first call (any value,
// including 0.0) always passes.
//
// From Thetis console.cs:28805-28817 [v2.10.3.13] — setAudioMicGain.
// From Thetis audio.cs:216-243 [v2.10.3.13] — MicPreamp property setter.
// From Thetis wdsp/patchpanel.c:209-216 [v2.10.3.13] — SetTXAPanelGain1 impl.
// ---------------------------------------------------------------------------
void TxChannel::setMicPreamp(double linearGain)
{
    // NaN-aware idempotent guard (matching D.3 pattern for double setters).
    if (!std::isnan(m_micPreampLast) && linearGain == m_micPreampLast) return;
    m_micPreampLast = linearGain;
    recomputeTxAPanelGain1();
}

// ---------------------------------------------------------------------------
// recomputeTxAPanelGain1()
//
// Push the current m_micPreampLast to WDSP SetTXAPanelGain1.
//
// Called internally by setMicPreamp after the idempotent guard passes.
// Also exposed publicly so callers can force-refresh the WDSP state after
// channel rebuild (e.g., after setRunning(true) re-initialises the channel).
//
// This method ALWAYS issues the WDSP call when invoked (subject to
// HAVE_WDSP + null-guard).  It does NOT apply the NaN-aware idempotent
// guard — that is setMicPreamp's responsibility.  If m_micPreampLast is
// still NaN (never set via setMicPreamp), the WDSP call is a no-op due to
// the null-guard or the channel not being open.
//
// From Thetis console.cs:28805-28817 [v2.10.3.13] — setAudioMicGain.
// From Thetis wdsp/patchpanel.c:209-216 [v2.10.3.13] — SetTXAPanelGain1.
// ---------------------------------------------------------------------------
void TxChannel::recomputeTxAPanelGain1()
{
#ifdef HAVE_WDSP
    // From Thetis wdsp/patchpanel.c:209-216 [v2.10.3.13]
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    SetTXAPanelGain1(m_channelId, m_micPreampLast);
#endif
}

// ---------------------------------------------------------------------------
// getTxMicMeter()
//
// TX mic input peak meter — reads WDSP TXA_MIC_PK (= 0) via GetTXAMeter.
//
// Porting from Thetis dsp.cs:390-391 [v2.10.3.13] — GetTXAMeter DLL import:
//   [DllImport("wdsp.dll", EntryPoint = "GetTXAMeter", ...)]
//   public static extern double GetTXAMeter(int channel, txaMeterType meter);
// Porting from Thetis dsp.cs:1025-1026 [v2.10.3.13] — callsite:
//   case txaMeterType.TXA_MIC_PK:
//       val = GetTXAMeter(channel, txaMeterType.TXA_MIC_PK);
// Porting from Thetis wdsp/TXA.h:51 [v2.10.3.13]:
//   TXA_MIC_PK  = 0  (first value in txaMeterType enum)
// Porting from Thetis wdsp/meter.c:151-159 [v2.10.3.13] — GetTXAMeter:
//   double GetTXAMeter(int channel, int mt)
//   { ... val = txa[channel].meter[mt]; ... return val; }
// ---------------------------------------------------------------------------
float TxChannel::getTxMicMeter() const
{
#ifdef HAVE_WDSP
    // From Thetis wdsp/meter.c:153-157 [v2.10.3.13] — GetTXAMeter accesses
    // txa[channel].pmtupdate[mt] (CRITICAL_SECTION*). Guard against uninitialised
    // channel using the same rsmpin.p sentinel used throughout this class.
    if (txa[m_channelId].rsmpin.p == nullptr) return kMeterUninitialisedSentinel;
    return static_cast<float>(GetTXAMeter(m_channelId, TXA_MIC_PK));  // TXA_MIC_PK = 0 [TXA.h:51]
#else
    return kMeterUninitialisedSentinel;
#endif
}

// ---------------------------------------------------------------------------
// getAlcMeter()
//
// TX ALC peak meter — reads WDSP TXA_ALC_PK (= 12) via GetTXAMeter.
//
// Porting from Thetis dsp.cs:390-391 [v2.10.3.13] — GetTXAMeter DLL import.
// Porting from Thetis dsp.cs:1028-1029 [v2.10.3.13] — callsite:
//   case txaMeterType.TXA_ALC_PK:
//       val = GetTXAMeter(channel, txaMeterType.TXA_ALC_PK);
// Porting from Thetis wdsp/TXA.h:63 [v2.10.3.13]:
//   TXA_ALC_PK  = 12  (12th value in txaMeterType enum, 0-indexed)
// Porting from Thetis wdsp/meter.c:151-159 [v2.10.3.13] — GetTXAMeter impl.
// ---------------------------------------------------------------------------
float TxChannel::getAlcMeter() const
{
#ifdef HAVE_WDSP
    // Same rsmpin.p null-guard as getTxMicMeter().
    if (txa[m_channelId].rsmpin.p == nullptr) return kMeterUninitialisedSentinel;
    return static_cast<float>(GetTXAMeter(m_channelId, TXA_ALC_PK));  // TXA_ALC_PK = 12 [TXA.h:63]
#else
    return kMeterUninitialisedSentinel;
#endif
}

// ---------------------------------------------------------------------------
// Deferred TX meter stubs — return 0.0f unconditionally (3M-3a scope)
//
// Per master design §5.2.1: EQ / Leveler / CFC / Compressor meters are
// deferred to Phase 3M-3a when the full speech-processing chain is wired.
// These stubs return 0.0f so callers can safely poll them during 3M-1b
// without crashing; UI code treats 0.0f as "meter not yet active".
//
// The active meters (getTxMicMeter / getAlcMeter) return
// kMeterUninitialisedSentinel (-999.0f) when the channel is uninitialised —
// a different sentinel to keep the two "inactive" states distinguishable.
//
// Future wiring in 3M-3a:
//   getEqMeter()   → GetTXAMeter(ch, TXA_EQ_PK)    TXA.h:53 [v2.10.3.13]
//   getLvlrMeter() → GetTXAMeter(ch, TXA_LVLR_PK)  TXA.h:55 [v2.10.3.13]
//   getCfcMeter()  → GetTXAMeter(ch, TXA_CFC_PK)   TXA.h:58 [v2.10.3.13]
//   getCompMeter() → GetTXAMeter(ch, TXA_COMP_PK)  TXA.h:61 [v2.10.3.13]
// ---------------------------------------------------------------------------
float TxChannel::getEqMeter()   const { return 0.0f; }  // deferred 3M-3a
float TxChannel::getLvlrMeter() const { return 0.0f; }  // deferred 3M-3a
float TxChannel::getCfcMeter()  const { return 0.0f; }  // deferred 3M-3a
float TxChannel::getCompMeter() const { return 0.0f; }  // deferred 3M-3a

// ---------------------------------------------------------------------------
// TXA PostGen wrapper setters (3M-1c E.2-E.6)
//
// Twelve thin C++ wrappers over the WDSP `SetTXAPostGen*` family that drives
// the gen1 (TXA stage 22) two-tone / pulsed-IMD test source.
//
// The C# Thetis property surface exposes Freq1/Freq2 / Mag1/Mag2 as separate
// setters, but the underlying WDSP C API combines both into single calls.
// NereusSDR caches the partner value internally (m_postGen*Cache fields)
// so each individual setX1 / setX2 wrapper can invoke the combined WDSP
// call with both fields — matching Thetis radio.cs:3697-4032 [v2.10.3.13]
// `tx_postgen_tt_freq1_dsp` / `_freq2_dsp` / etc. cache fields.
//
// Pass-through semantics: no idempotency guard, no validation.  WDSP's
// gen.c:817-962 [v2.10.3.13] handles internal validation; Phase I's handler
// is responsible for choosing legal values.  The same `txa[ch].rsmpin.p ==
// nullptr` null-guard used throughout this class protects unit-test builds
// that link WDSP but don't call OpenChannel.
// ---------------------------------------------------------------------------

// ── E.2: setTxPostGenMode ────────────────────────────────────────────────────
//
// From Thetis setup.cs:11084 [v2.10.3.13]:
//   console.radio.GetDSPTX(0).TXPostGenMode = 7;   // pulsed
// From Thetis setup.cs:11096 [v2.10.3.13]:
//   console.radio.GetDSPTX(0).TXPostGenMode = 1;   // continuous
// Mode values: 0 = off, 1 = continuous two-tone, 7 = pulsed two-tone.
// Other modes 2/3/4/5/6 (noise/sweep/sawtooth/triangle/pulse) exist in
// gen.c but are out of 3M-1c scope.
void TxChannel::setTxPostGenMode(int mode)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    SetTXAPostGenMode(m_channelId, mode);   // gen.c:792-797 [v2.10.3.13]
#else
    Q_UNUSED(mode);
#endif
}

// ── E.3: setTxPostGenTTFreq1 ─────────────────────────────────────────────────
//
// From Thetis setup.cs:11099 [v2.10.3.13]:
//   console.radio.GetDSPTX(0).TXPostGenTTFreq1 = ttfreq1;
// From Thetis radio.cs:3735-3751 [v2.10.3.13] — TXPostGenTTFreq1 setter:
//   tx_postgen_tt_freq1_dsp = value;
//   WDSP.SetTXAPostGenTTFreq(WDSP.id(thread, 0),
//                            tx_postgen_tt_freq1_dsp,
//                            tx_postgen_tt_freq2_dsp);
// Cache freq2 partner in m_postGenTTFreq2Cache so the combined call uses both.
void TxChannel::setTxPostGenTTFreq1(double hz)
{
    m_postGenTTFreq1Cache = hz;
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    SetTXAPostGenTTFreq(m_channelId,
                        m_postGenTTFreq1Cache,
                        m_postGenTTFreq2Cache);   // gen.c:826-833 [v2.10.3.13]
#endif
}

// ── E.3: setTxPostGenTTFreq2 ─────────────────────────────────────────────────
//
// From Thetis setup.cs:11100 [v2.10.3.13]:
//   console.radio.GetDSPTX(0).TXPostGenTTFreq2 = ttfreq2;
// From Thetis radio.cs:3755-3771 [v2.10.3.13] — TXPostGenTTFreq2 setter
// (mirror of Freq1 — cache-and-call pattern, same WDSP function).
void TxChannel::setTxPostGenTTFreq2(double hz)
{
    m_postGenTTFreq2Cache = hz;
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    SetTXAPostGenTTFreq(m_channelId,
                        m_postGenTTFreq1Cache,
                        m_postGenTTFreq2Cache);   // gen.c:826-833 [v2.10.3.13]
#endif
}

// ── E.3: setTxPostGenTTMag1 ──────────────────────────────────────────────────
//
// From Thetis setup.cs:11102 [v2.10.3.13]:
//   console.radio.GetDSPTX(0).TXPostGenTTMag1 = ttmag1;
// From Thetis radio.cs:3697-3712 [v2.10.3.13] — TXPostGenTTMag1 setter
// (cache-and-call pattern; combined WDSP call uses mag1+mag2_dsp).
void TxChannel::setTxPostGenTTMag1(double linear)
{
    m_postGenTTMag1Cache = linear;
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    SetTXAPostGenTTMag(m_channelId,
                       m_postGenTTMag1Cache,
                       m_postGenTTMag2Cache);     // gen.c:817-823 [v2.10.3.13]
#endif
}

// ── E.3: setTxPostGenTTMag2 ──────────────────────────────────────────────────
//
// From Thetis setup.cs:11103 [v2.10.3.13]:
//   console.radio.GetDSPTX(0).TXPostGenTTMag2 = ttmag2;
// From Thetis radio.cs:3716-3731 [v2.10.3.13] — TXPostGenTTMag2 setter.
void TxChannel::setTxPostGenTTMag2(double linear)
{
    m_postGenTTMag2Cache = linear;
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    SetTXAPostGenTTMag(m_channelId,
                       m_postGenTTMag1Cache,
                       m_postGenTTMag2Cache);     // gen.c:817-823 [v2.10.3.13]
#endif
}

// ── E.4: setTxPostGenTTPulseToneFreq1 ────────────────────────────────────────
//
// From Thetis setup.cs:11087 [v2.10.3.13]:
//   console.radio.GetDSPTX(0).TXPostGenTTPulseToneFreq1 = ttfreq1;
// From Thetis radio.cs:4000-4015 [v2.10.3.13] — TXPostGenTTPulseToneFreq1
// setter (cache-and-call; combined WDSP call uses freq1+freq2_dsp).
void TxChannel::setTxPostGenTTPulseToneFreq1(double hz)
{
    m_postGenTTPulseToneFreq1Cache = hz;
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    SetTXAPostGenTTPulseToneFreq(m_channelId,
                                 m_postGenTTPulseToneFreq1Cache,
                                 m_postGenTTPulseToneFreq2Cache);   // gen.c:944-952 [v2.10.3.13]
#endif
}

// ── E.4: setTxPostGenTTPulseToneFreq2 ────────────────────────────────────────
//
// From Thetis setup.cs:11088 [v2.10.3.13]:
//   console.radio.GetDSPTX(0).TXPostGenTTPulseToneFreq2 = ttfreq2;
// From Thetis radio.cs:4018-4033 [v2.10.3.13] — TXPostGenTTPulseToneFreq2.
void TxChannel::setTxPostGenTTPulseToneFreq2(double hz)
{
    m_postGenTTPulseToneFreq2Cache = hz;
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    SetTXAPostGenTTPulseToneFreq(m_channelId,
                                 m_postGenTTPulseToneFreq1Cache,
                                 m_postGenTTPulseToneFreq2Cache);   // gen.c:944-952 [v2.10.3.13]
#endif
}

// ── E.4: setTxPostGenTTPulseMag1 ─────────────────────────────────────────────
//
// From Thetis setup.cs:11090 [v2.10.3.13]:
//   console.radio.GetDSPTX(0).TXPostGenTTPulseMag1 = ttmag1;
// From Thetis radio.cs:3964-3979 [v2.10.3.13] — TXPostGenTTPulseMag1 setter.
void TxChannel::setTxPostGenTTPulseMag1(double linear)
{
    m_postGenTTPulseMag1Cache = linear;
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    SetTXAPostGenTTPulseMag(m_channelId,
                            m_postGenTTPulseMag1Cache,
                            m_postGenTTPulseMag2Cache);             // gen.c:915-923 [v2.10.3.13]
#endif
}

// ── E.4: setTxPostGenTTPulseMag2 ─────────────────────────────────────────────
//
// From Thetis setup.cs:11091 [v2.10.3.13]:
//   console.radio.GetDSPTX(0).TXPostGenTTPulseMag2 = ttmag2;
// From Thetis radio.cs:3982-3997 [v2.10.3.13] — TXPostGenTTPulseMag2 setter.
void TxChannel::setTxPostGenTTPulseMag2(double linear)
{
    m_postGenTTPulseMag2Cache = linear;
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    SetTXAPostGenTTPulseMag(m_channelId,
                            m_postGenTTPulseMag1Cache,
                            m_postGenTTPulseMag2Cache);             // gen.c:915-923 [v2.10.3.13]
#endif
}

// ── E.5: setTxPostGenTTPulseFreq ─────────────────────────────────────────────
//
// From Thetis setup.cs:34415 [v2.10.3.13] — setupTwoTonePulse:
//   console.radio.GetDSPTX(0).TXPostGenTTPulseFreq =
//       (int)nudPulsed_TwoTone_window.Value;
// Single-parameter (window pulse rate in Hz) — distinct from PulseToneFreq
// above which takes a (freq1, freq2) pair.
void TxChannel::setTxPostGenTTPulseFreq(int hz)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // WDSP signature takes double; widen from int (Thetis stores as int and
    // crosses the C# double boundary on the property setter — same widening
    // semantics here).
    SetTXAPostGenTTPulseFreq(m_channelId, static_cast<double>(hz));   // gen.c:926-933 [v2.10.3.13]
#else
    Q_UNUSED(hz);
#endif
}

// ── E.5: setTxPostGenTTPulseDutyCycle ────────────────────────────────────────
//
// From Thetis setup.cs:34416 [v2.10.3.13] — setupTwoTonePulse:
//   console.radio.GetDSPTX(0).TXPostGenTTPulseDutyCycle =
//       (float)(nudPulsed_TwoTone_percent.Value) / 100f;
// Caller is responsible for the percent → fraction (÷100) conversion;
// the wrapper passes through to WDSP unchanged.
void TxChannel::setTxPostGenTTPulseDutyCycle(double pct)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    SetTXAPostGenTTPulseDutyCycle(m_channelId, pct);   // gen.c:935-942 [v2.10.3.13]
#else
    Q_UNUSED(pct);
#endif
}

// ── E.5: setTxPostGenTTPulseTransition ───────────────────────────────────────
//
// From Thetis setup.cs:34417 [v2.10.3.13] — setupTwoTonePulse:
//   console.radio.GetDSPTX(0).TXPostGenTTPulseTransition =
//       (float)(nudPulsed_TwoTone_ramp.Value) / 1000f;
// Caller is responsible for the ms → s (÷1000) conversion; the wrapper
// passes through to WDSP unchanged.
void TxChannel::setTxPostGenTTPulseTransition(double sec)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    SetTXAPostGenTTPulseTransition(m_channelId, sec);  // gen.c:955-962 [v2.10.3.13]
#else
    Q_UNUSED(sec);
#endif
}

// ── I.1: setTxPostGenTTPulseIQOut ────────────────────────────────────────────
//
// From Thetis setup.cs:34414 [v2.10.3.13] — setupTwoTonePulse:
//   console.radio.GetDSPTX(0).TXPostGenTTPulseIQOut = true;
// From Thetis radio.cs:4090-4105 [v2.10.3.13] — TXPostGenTTPulseIQOut setter.
// From Thetis wdsp/gen.c:963-969 [v2.10.3.13] — SetTXAPostGenTTPulseIQout impl.
//
// Added in 3M-1c chunk I (rather than chunk E.5) because the TwoToneController
// activation flow is the only call site, and adding the wrapper here keeps the
// activation handler's setupTwoTonePulse() port complete in a single phase.
void TxChannel::setTxPostGenTTPulseIQOut(bool on)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    SetTXAPostGenTTPulseIQout(m_channelId, on ? 1 : 0); // gen.c:963-969 [v2.10.3.13]
#else
    Q_UNUSED(on);
#endif
}

// ── E.6: setTxPostGenRun ─────────────────────────────────────────────────────
//
// From Thetis setup.cs:11107 [v2.10.3.13]:
//   console.radio.GetDSPTX(0).TXPostGenRun = 1;   // on
// From Thetis setup.cs:11166 [v2.10.3.13]:
//   console.radio.GetDSPTX(0).TXPostGenRun = 0;   // off
void TxChannel::setTxPostGenRun(bool on)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    SetTXAPostGenRun(m_channelId, on ? 1 : 0);   // gen.c:784-789 [v2.10.3.13]
#else
    Q_UNUSED(on);
#endif
}

// ── B-1: TX EQ wrappers (Phase 3M-3a-i Batch 1) ─────────────────────────────
//
// Each wrapper is a thin pass-through over the WDSP `SetTXAEQ*` family.  Run
// is csDSP-protected and audio-safe; all other setters reallocate the EQ
// impulse and are main-thread-only (per Thetis precedent — UI handlers run
// on the SetupForm thread).  See TxChannel.h doc-comments for the full
// threading rationale.

void TxChannel::setTxEqRunning(bool on)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/eq.c:742-747 [v2.10.3.13] — SetTXAEQRun(channel, run).
    // csDSP-protected; safe from main thread while audio thread runs.
    SetTXAEQRun(m_channelId, on ? 1 : 0);
#else
    Q_UNUSED(on);
#endif
}

void TxChannel::setTxEqGraph10(const std::array<int, 11>& preampPlus10Bands)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/eq.c:859-883 [v2.10.3.13] — SetTXAGrphEQ10(channel, txeq).
    //   txeq[0]      = preamp dB
    //   txeq[1..10]  = 10-band gains dB at fixed centers per WDSP eq.c:870-879.
    // SetTXAGrphEQ10 reallocates the impulse — main-thread only.
    int txeq[11];
    for (int i = 0; i < 11; ++i) {
        txeq[i] = preampPlus10Bands[static_cast<std::size_t>(i)];
    }
    SetTXAGrphEQ10(m_channelId, txeq);
#else
    Q_UNUSED(preampPlus10Bands);
#endif
}

void TxChannel::setTxEqProfile(const std::vector<double>& freqs10,
                               const std::vector<double>& gains11)
{
    // Validate inputs before touching WDSP — early-return on size mismatch.
    if (freqs10.size() != 10) {
        qCWarning(lcDsp) << "TxChannel::setTxEqProfile: freqs10 must have 10 entries, got"
                         << freqs10.size() << "— ignoring call";
        return;
    }
    if (gains11.size() != 11) {
        qCWarning(lcDsp) << "TxChannel::setTxEqProfile: gains11 must have 11 entries, got"
                         << gains11.size() << "— ignoring call";
        return;
    }

#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/eq.c:779-804 [v2.10.3.13] — SetTXAEQProfile(channel, nfreqs, F[], G[]).
    // F is 1-indexed inside WDSP (F[0] is the unused pad slot), G is 0-indexed
    // (G[0] = preamp).  Both buffers must be at least nfreqs+1 entries; we
    // build them fresh on the stack.  (Q vector is exclusive to the parametric
    // SetTXAGrphEQProfile variant — graphic EQ doesn't take a Q.)
    //
    // Mirrors the create_eqp call at wdsp/TXA.c:111-127 [v2.10.3.13]:
    //   double default_F[11] = {0.0,  32.0, ...};  // F[0] = 0.0 pad
    //   double default_G[11] = {0.0, -12.0, ...};  // G[0] = preamp (0 by default)
    //   //double default_G[11] =   {0.0,   0.0,   0.0,   0.0,   0.0,   0.0,    0.0,    0.0,    0.0,    0.0,     0.0};
    //   create_eqp(..., 10, default_F, default_G, ...);
    constexpr int kNfreqs = 10;
    double F[kNfreqs + 1];
    double G[kNfreqs + 1];
    F[0] = 0.0;  // WDSP F[0] pad slot
    for (int i = 0; i < kNfreqs; ++i) {
        F[i + 1] = freqs10[static_cast<std::size_t>(i)];
    }
    for (int i = 0; i < kNfreqs + 1; ++i) {
        G[i] = gains11[static_cast<std::size_t>(i)];
    }
    SetTXAEQProfile(m_channelId, kNfreqs, F, G);
#endif
}

void TxChannel::setTxEqNc(int nc)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/eq.c:750-764 [v2.10.3.13] — SetTXAEQNC(channel, nc).
    // Allocates eq_impulse — main-thread only.
    SetTXAEQNC(m_channelId, nc);
#else
    Q_UNUSED(nc);
#endif
}

void TxChannel::setTxEqMp(bool mp)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/eq.c:767-776 [v2.10.3.13] — SetTXAEQMP(channel, mp).
    // Allocates min-phase impulse via setMp_fircore — main-thread only.
    SetTXAEQMP(m_channelId, mp ? 1 : 0);
#else
    Q_UNUSED(mp);
#endif
}

void TxChannel::setTxEqCtfmode(int mode)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/eq.c:807-816 [v2.10.3.13] — SetTXAEQCtfmode(channel, mode).
    // Allocates eq_impulse — main-thread only.
    SetTXAEQCtfmode(m_channelId, mode);
#else
    Q_UNUSED(mode);
#endif
}

void TxChannel::setTxEqWintype(int wintype)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/eq.c:819-828 [v2.10.3.13] — SetTXAEQWintype(channel, wintype).
    // Allocates eq_impulse — main-thread only.
    SetTXAEQWintype(m_channelId, wintype);
#else
    Q_UNUSED(wintype);
#endif
}

// ── B-2: TX Leveler / ALC wrappers (Phase 3M-3a-i Batch 1) ─────────────────
//
// Six wrappers over the wcpAGC.c Leveler+ALC API.  All are csDSP-protected
// in WDSP (wcpAGC.c:570-650 [v2.10.3.13]) and therefore audio-safe to call
// from the main thread.  ALC Run is locked-on per Thetis schema — no
// setTxAlcOn wrapper is exposed.

void TxChannel::setTxLevelerOn(bool on)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/wcpAGC.c:613-618 [v2.10.3.13] — SetTXALevelerSt(channel, state).
    // csDSP-protected.
    // Cited handler: setup.cs:9108-9123 [v2.10.3.13] — chkDSPLevelerEnabled_CheckedChanged
    //   routes through radio.cs DSPTX::TXLevelerOn setter which calls SetTXALevelerSt.
    SetTXALevelerSt(m_channelId, on ? 1 : 0);
#else
    Q_UNUSED(on);
#endif
}

void TxChannel::setTxLevelerTopDb(double dB)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/wcpAGC.c:647-650 [v2.10.3.13] — SetTXALevelerTop(channel, maxgain).
    // Thetis converts dB → linear via pow(10, dB/20.0) inside wcpAGC; we pass dB
    // straight, matching the radio.cs TXLevelerMaxGain setter pattern.
    // Cited handler: setup.cs:9095-9099 [v2.10.3.13] — udDSPLevelerThreshold_ValueChanged.
    SetTXALevelerTop(m_channelId, dB);
#else
    Q_UNUSED(dB);
#endif
}

void TxChannel::setTxLevelerDecayMs(int ms)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/wcpAGC.c:629-635 [v2.10.3.13] — SetTXALevelerDecay(channel, decay).
    // csDSP-protected.  WDSP stores decay/1000.0 sec internally.
    // Cited handler: setup.cs:9101-9105 [v2.10.3.13] — udDSPLevelerDecay_ValueChanged.
    SetTXALevelerDecay(m_channelId, ms);
#else
    Q_UNUSED(ms);
#endif
}

void TxChannel::setTxAlcMaxGainDb(double dB)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/wcpAGC.c:603-610 [v2.10.3.13] — SetTXAALCMaxGain(channel, maxgain).
    // Thetis converts dB → linear via pow(10, dB/20.0) inside wcpAGC.
    // Cited handler: setup.cs:9129-9134 [v2.10.3.13] — udDSPALCMaximumGain_ValueChanged
    //   calls SetTXAALCMaxGain directly + updates WDSP.ALCGain readout cache.
    SetTXAALCMaxGain(m_channelId, dB);
#else
    Q_UNUSED(dB);
#endif
}

void TxChannel::setTxAlcDecayMs(int ms)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/wcpAGC.c:585-592 [v2.10.3.13] — SetTXAALCDecay(channel, decay).
    // csDSP-protected.  WDSP stores decay/1000.0 sec internally.
    // Cited handler: setup.cs:9136-9140 [v2.10.3.13] — udDSPALCDecay_ValueChanged.
    SetTXAALCDecay(m_channelId, ms);
#else
    Q_UNUSED(ms);
#endif
}

// ── B-3: TX CFC + CPDR + CESSB wrappers (Phase 3M-3a-ii Batch 1) ────────────
//
// Nine wrappers over the WDSP TXA dynamics section:
//   - CFC   (cfcomp.c:632-737)  Continuous Frequency Compander (stage 11)
//   - CPDR  (compress.c:99-117) speech compressor (stage 14)
//   - CESSB (osctrl.c:142-150)  controlled-envelope SSB (stage 16)
//
// All nine are csDSP-protected inside WDSP and audio-safe to call from the
// main thread.  See the per-method comments in TxChannel.h for the side-
// effect surface (TXASetupBPFilters re-entry on CPDR / CESSB toggle, bp2.run
// gating, etc).

void TxChannel::setTxCfcRunning(bool on)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/cfcomp.c:632-641 [v2.10.3.13] — SetTXACFCOMPRun(channel, run).
    // csDSP-protected.
    SetTXACFCOMPRun(m_channelId, on ? 1 : 0);
#else
    Q_UNUSED(on);
#endif
}

void TxChannel::setTxCfcPosition(int pos)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/cfcomp.c:643-653 [v2.10.3.13] — SetTXACFCOMPPosition(channel, pos).
    // csDSP-protected.
    SetTXACFCOMPPosition(m_channelId, pos);
#else
    Q_UNUSED(pos);
#endif
}

void TxChannel::setTxCfcProfile(const std::vector<double>& F,
                                const std::vector<double>& G,
                                const std::vector<double>& E,
                                const std::vector<double>& Qg,
                                const std::vector<double>& Qe)
{
    // Validate arity before touching WDSP — early-return on size mismatch.
    // F / G / E must all have the same length (nfreqs).  Qg / Qe are optional
    // in Thetis v2.10.3.13 — empty vectors signal "not provided" (the wrapper
    // forwards nullptr to WDSP for the empty case, matching cfcomp.c:669-682
    // [v2.10.3.13] semantics).
    const std::size_t nfreqs = F.size();
    if (nfreqs == 0) {
        qCWarning(lcDsp) << "TxChannel::setTxCfcProfile: F is empty — nothing"
                            " to apply, ignoring call";
        return;
    }
    if (G.size() != nfreqs) {
        qCWarning(lcDsp) << "TxChannel::setTxCfcProfile: G size" << G.size()
                         << "does not match F size" << nfreqs
                         << "— ignoring call";
        return;
    }
    if (E.size() != nfreqs) {
        qCWarning(lcDsp) << "TxChannel::setTxCfcProfile: E size" << E.size()
                         << "does not match F size" << nfreqs
                         << "— ignoring call";
        return;
    }
    if (!Qg.empty() && Qg.size() != nfreqs) {
        qCWarning(lcDsp) << "TxChannel::setTxCfcProfile: Qg size" << Qg.size()
                         << "must be empty or match F size" << nfreqs
                         << "— ignoring call";
        return;
    }
    if (!Qe.empty() && Qe.size() != nfreqs) {
        qCWarning(lcDsp) << "TxChannel::setTxCfcProfile: Qe size" << Qe.size()
                         << "must be empty or match F size" << nfreqs
                         << "— ignoring call";
        return;
    }

#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/cfcomp.c:656-698 [v2.10.3.13] — SetTXACFCOMPprofile.
    //
    // As of Phase 3M-3a-ii Batch 1.5 the bundled third_party/wdsp/src/cfcomp.c
    // is the Thetis v2.10.3.13 version, so the 7-arg signature is exported
    // and Qg/Qe forward through.  Empty vectors map to nullptr per the
    // WDSP NULL-skirt semantics (cfcomp.c:669-682 [v2.10.3.13]: WDSP keeps
    // a->Qg / a->Qe unallocated and calc_comp falls back to the linear
    // interpolation path for that skirt).
    double* qg = Qg.empty() ? nullptr : const_cast<double*>(Qg.data());
    double* qe = Qe.empty() ? nullptr : const_cast<double*>(Qe.data());
    SetTXACFCOMPprofile(m_channelId, static_cast<int>(nfreqs),
                        const_cast<double*>(F.data()),
                        const_cast<double*>(G.data()),
                        const_cast<double*>(E.data()), qg, qe);
#else
    Q_UNUSED(Qg);
    Q_UNUSED(Qe);
#endif
}

void TxChannel::setTxCfcPrecompDb(double dB)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/cfcomp.c:700-715 [v2.10.3.13] — SetTXACFCOMPPrecomp(channel, precomp).
    // csDSP-protected.  WDSP stores precomplin = pow(10, 0.05 * dB) and
    // re-multiplies cfc_gain[].
    SetTXACFCOMPPrecomp(m_channelId, dB);
#else
    Q_UNUSED(dB);
#endif
}

void TxChannel::setTxCfcPostEqRunning(bool on)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/cfcomp.c:717-727 [v2.10.3.13] — SetTXACFCOMPPeqRun(channel, run).
    // csDSP-protected.
    SetTXACFCOMPPeqRun(m_channelId, on ? 1 : 0);
#else
    Q_UNUSED(on);
#endif
}

void TxChannel::setTxCfcPrePeqDb(double dB)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/cfcomp.c:729-737 [v2.10.3.13] — SetTXACFCOMPPrePeq(channel, prepeq).
    // csDSP-protected.  WDSP stores prepeqlin = pow(10, 0.05 * dB).
    SetTXACFCOMPPrePeq(m_channelId, dB);
#else
    Q_UNUSED(dB);
#endif
}

void TxChannel::setTxCpdrOn(bool on)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/compress.c:99-109 [v2.10.3.13] — SetTXACompressorRun(channel, run).
    // csDSP-protected.  Side effect: calls TXASetupBPFilters(channel) at
    // compress.c:106, which rebuilds bp1 + the gated bp2 to track the
    // compression-and-clip routing.  See header doc for the rationale.
    SetTXACompressorRun(m_channelId, on ? 1 : 0);
#else
    Q_UNUSED(on);
#endif
}

void TxChannel::setTxCpdrGainDb(double dB)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/compress.c:111-117 [v2.10.3.13] — SetTXACompressorGain(channel, gain).
    // csDSP-protected.  WDSP stores pow(10, dB / 20.0) internally.
    SetTXACompressorGain(m_channelId, dB);
#else
    Q_UNUSED(dB);
#endif
}

void TxChannel::setTxCessbOn(bool on)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/osctrl.c:142-150 [v2.10.3.13] — SetTXAosctrlRun(channel, run).
    // csDSP-protected.  Side effect: calls TXASetupBPFilters(channel) at
    // osctrl.c:148, which rebuilds bp2.
    //
    // bp2.run gating semantic: the CESSB-side bandpass only runs when *both*
    // compressor.run AND osctrl.run are 1 (TXA.c:843-868 [v2.10.3.13],
    // parallel switch arms).  Calling setTxCessbOn(true) without first
    // turning CPDR on is therefore effectively a no-op at the audio level.
    // This wrapper does NOT enforce that coupling — Thetis lets WDSP own it,
    // and we match.
    SetTXAosctrlRun(m_channelId, on ? 1 : 0);
#else
    Q_UNUSED(on);
#endif
}

// ── B-3.1: TX Phase Rotator parameter wrappers (Phase 3M-3a-ii Batch 1.6) ────
//
// Three thin pass-through wrappers over the TXA phrot parameter setters in
// wdsp/iir.c:675-703 [v2.10.3.13].  3M-3a-i Stage::PhRot already wired the
// Run flag via SetTXAPHROTRun (in setStageRunning).  These three pick up the
// remaining tunables that the Thetis tpDSPCFC tab exposes alongside the CFC
// controls.  All three are csDSP-protected (cs_update) inside WDSP and
// audio-safe to call from the main thread.  See TxChannel.h wrapper docs for
// the cost/threading rationale (Corner / Nstages call decalc_phrot +
// calc_phrot internally; Reverse just flips the sign).

void TxChannel::setTxPhrotCornerHz(double hz)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/iir.c:675-683 [v2.10.3.13] — SetTXAPHROTCorner(channel, corner).
    // csDSP-protected.  WDSP rebuilds the all-pass bank on every set
    // (decalc_phrot + a->fc = corner + calc_phrot) — non-trivial cost.
    SetTXAPHROTCorner(m_channelId, hz);
#else
    Q_UNUSED(hz);
#endif
}

void TxChannel::setTxPhrotNstages(int nstages)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/iir.c:686-694 [v2.10.3.13] — SetTXAPHROTNstages(channel, nstages).
    // csDSP-protected.  WDSP rebuilds the coefficient bank on every set
    // (decalc_phrot + a->nstages = nstages + calc_phrot) — non-trivial cost.
    SetTXAPHROTNstages(m_channelId, nstages);
#else
    Q_UNUSED(nstages);
#endif
}

void TxChannel::setTxPhrotReverse(bool reverse)
{
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // From Thetis wdsp/iir.c:697-703 [v2.10.3.13] — SetTXAPHROTReverse(channel, reverse).
    // csDSP-protected.  Cheap — just flips a->reverse; no coefficient rebuild.
    SetTXAPHROTReverse(m_channelId, reverse ? 1 : 0);
#else
    Q_UNUSED(reverse);
#endif
}

} // namespace NereusSDR
