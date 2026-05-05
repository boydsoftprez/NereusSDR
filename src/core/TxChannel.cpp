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
//   2026-05-02 — Plan 4 D8: requestFilterChange / applyPendingFilter /
//                 applyTxFilterForMode implemented by J.J. Boyd (KG4VCF).
//                 50 ms debounce QTimer wired in constructor.  Per-mode
//                 IQ-space mapping is NereusSDR-original glue identical
//                 to the TUN bandpass logic at setTuneTone():505-528,
//                 cited from deskhpsdr/transmitter.c:2136-2186 [@120188f].
//                 The TUN path at lines 520-528 is untouched.
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-05-03 — Issue #167 Phase 1 Agent 1C — setTxFixedGain(double)
//                 implemented by J.J. Boyd (KG4VCF).  Thin wrapper around
//                 cmaster/ChannelMaster SetTXFixedGain (Thetis cmaster.cs:
//                 1115-1119 CMSetTXOutputLevel [v2.10.3.13]).  NaN-aware
//                 idempotent guard via m_lastFixedGain mirrors the
//                 setMicPreamp / D.3 / D.6 pattern.  WDSP entry-point
//                 forward-declaration colocated with the existing
//                 GetTXACFCOMPDisplayCompression block; the underlying
//                 SetTXFixedGain symbol lives in NereusSDR's bundled
//                 third_party/wdsp/src/txgain_stub.c glue file (issue #167
//                 Phase 1) until a future phase ports the full Thetis
//                 ChannelMaster TXGAIN pcm storage.  RadioModel call-site
//                 composition (audio_volume * swrProtect) is deferred to
//                 issue #167 Phase 4A.  AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-05-03 — Phase 3M-3a-iii Task 20 (bench fix): setDexpBuffer() +
//                 pumpDexp() helpers implemented by J.J. Boyd (KG4VCF).
//                 pumpDexp() copies the worker-thread-owned mic block
//                 into the WdspEngine-owned per-channel DEXP buffer (set
//                 once via setDexpBuffer at TX-channel-create time) and
//                 invokes xdexp(channelId), mirroring Thetis cmaster.c:388
//                 [v2.10.3.13] xdexp(tx) call BEFORE fexchange0.  Same
//                 null-guard pattern as the existing setVox* / DEXP
//                 setters.  Critical correctness fix — without this, the
//                 entire DEXP feature was a no-op shell because the
//                 create_dexp call had never been ported into NereusSDR's
//                 createTxChannel.  See the WdspEngine.cpp comment block
//                 at the create_dexp callsite for the full root-cause /
//                 buffer-architecture narrative.  AI-assisted transformation
//                 via Anthropic Claude Code.
//   2026-05-04 — Phase 3M-3a-iii Task 17 (bench fix): static
//                 s_pushVoxCallback() bridge + s_voxKeyInstance lookup +
//                 registerVoxCallback() / unregisterVoxCallback() helpers
//                 implemented by J.J. Boyd (KG4VCF).  Constructor calls
//                 registerVoxCallback() AFTER WDSP TXA pipeline init has
//                 set up the DEXP detector (cmaster.c:130-157
//                 [v2.10.3.13] — create_dexp runs before OpenChannel
//                 returns, so pdexp[m_channelId] is non-null at this
//                 point).  Destructor calls unregisterVoxCallback() so a
//                 late callback fired post-dtor on the audio worker thread
//                 dereferences nullptr (the bridge's defensive null
//                 check makes that a no-op) instead of a stale this.
//                 Closes the deferred wire from 3M-1b RadioModel.cpp:756
//                 ("onVoxActive: 3M-3a or via TxChannel TX-meter polling
//                 (WDSP DEXP output)") that the 3M-3a-iii implementation
//                 plan did not capture as a task.  Caught on JJ's bench
//                 of the post-3M-3a-iii build: clicking [VOX] correctly
//                 set run_vox=1 in WDSP but mic envelope crossings never
//                 reached MoxController.  AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-05-03 — Phase 3M-3a-iii Tasks 1-2: setDexpRun(bool),
//                 setDexpDetectorTau(double), setDexpAttackTime(double),
//                 setDexpReleaseTime(double) wrappers implemented by
//                 J.J. Boyd (KG4VCF).  setDexpRun gates the audio-domain
//                 expansion (distinct from the existing setVoxRun() VOX-
//                 keying gate); the three timing setters wrap the
//                 detector-tau / attack / release envelope parameters.
//                 Same idempotent pattern as the existing setVox*()
//                 family with NaN-aware guards on doubles, ms→s
//                 conversion at the WDSP boundary, std::clamp at the
//                 wrapper boundary for the Thetis ranges
//                 (setup.Designer.cs:44967-45098 [v2.10.3.13]:
//                 udDEXPDetTau 1..100, udDEXPAttack 2..100,
//                 udDEXPRelease 2..1000).  AI-assisted transformation
//                 via Anthropic Claude Code.
//   2026-05-03 — Phase 3M-3a-iii Task 3: setDexpExpansionRatio(double)
//                 and setDexpHysteresisRatio(double) wrappers implemented
//                 by J.J. Boyd (KG4VCF).  Wrapper API takes dB
//                 (operator-friendly, matches Thetis Setup-form spinbox);
//                 internally converts to linear via std::pow(10.0, ±dB/20.0)
//                 before WDSP push.  POSITIVE sign for ExpansionRatio
//                 (setup.cs:18918 [v2.10.3.13]); NEGATIVE sign for
//                 HysteresisRatio (setup.cs:18924 [v2.10.3.13]).
//                 Idempotent guard compares the user-facing dB.  Ranges
//                 0..30 dB (Expansion) and 0..10 dB (Hysteresis) per
//                 setup.Designer.cs:44845-44905 [v2.10.3.13].  AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-05-03 — Phase 3M-3a-iii Task 4: setDexpLowCut(double),
//                 setDexpHighCut(double), setDexpRunSideChannelFilter(bool)
//                 wrappers implemented by J.J. Boyd (KG4VCF).  These
//                 control the side-channel band-pass filter that shapes
//                 the DEXP detector input (NOT the audio that gets
//                 gated).  Hz on both sides (no unit conversion); ranges
//                 100..10000 Hz on both cuts per setup.Designer.cs:
//                 45200-45245 [v2.10.3.13].  Thetis ships chkSCFEnable
//                 as default CHECKED (setup.Designer.cs:45250-45251
//                 [v2.10.3.13]); the wrapper cache initialises false
//                 to match WDSP's create_dexp boot state and the caller
//                 must push true at startup for Thetis-default behavior.
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-05-03 — Phase 3M-3a-iii Task 5: setDexpRunAudioDelay(bool)
//                 and setDexpAudioDelay(double) wrappers implemented
//                 by J.J. Boyd (KG4VCF).  These control the audio
//                 look-ahead delay line that lets VOX or the expander
//                 fire BEFORE the first syllable instead of clipping
//                 it.  AudioDelay takes ms at the wrapper, divides by
//                 1000.0 before WDSP push (matching setup.cs:18961
//                 [v2.10.3.13]).  Range 10..999 ms, default 60 (per
//                 setup.Designer.cs:44765-44793).  Thetis ships
//                 chkDEXPLookAheadEnable as default CHECKED
//                 (setup.Designer.cs:44808-44809 [v2.10.3.13]); the
//                 wrapper cache initialises false to match WDSP's
//                 create_dexp boot state and the caller must push true
//                 at startup for Thetis-default behavior.
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-05-04 — Phase 3M-3a-iii Task 18 (bench fix): setVoxListening(bool)
//                 + m_voxListening atomic + pump gate split implemented by
//                 J.J. Boyd (KG4VCF).  Second of two VOX-keying gaps the
//                 original 3M-3a-iii plan missed: Task 17 wired the WDSP
//                 pushvox callback (commit 56d6921), but the callback never
//                 fired because the TXA pipeline pump itself only ran when
//                 MOX was engaged (driveOneTxBlock + driveOneTxBlockFromInter
//                 leaved both early-returned on !m_running).  In Thetis the
//                 TXA pump is driven by hardware audio cadence (HPSDR EP6
//                 Tx audio) and is always-on after channel-open; MOX gates
//                 radio-write at the connection layer, not the pump itself.
//                 Per wdsp/dexp.c:304 [v2.10.3.13] "DEXP code runs
//                 continuously so it can be used to trigger VOX also."
//                 Fix splits the pump gate into two: pre-fexchange0 gate
//                 becomes (m_running || m_voxListening); post-fexchange0
//                 sendTxIq stays gated on m_running ONLY.  setVoxListening
//                 mirrors WDSP TXA channel state (SetChannelState +
//                 SetTXACFIRRun) when MOX is off so fexchange0 actually
//                 processes audio for the DEXP detector.  AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-05-03 — Phase 3M-3a-iii Task 6: getDexpPeakSignal() and
//                 getTxMicMeterDb() read accessors implemented by
//                 J.J. Boyd (KG4VCF).  Both `const noexcept` for the
//                 100 ms picVOX/picNoiseGate poll cadence on the GUI
//                 thread.  getDexpPeakSignal returns LINEAR amplitude
//                 (caller does 20*log10 dB conversion + mic-boost
//                 division per console.cs:28952-28954 [v2.10.3.13]);
//                 getTxMicMeterDb returns RAW negative dB (caller
//                 does sign treatment + 3 dB offset per
//                 console.cs:25353-25354 [v2.10.3.13]).  Idle returns
//                 (channel unopened): 0.0 for the DEXP peak (matches
//                 console.cs:28951 init), -200.0 for the mic-meter dB
//                 (matches console.cs:25346 noise_gate_data init).
//                 Reads TXA_MIC_AV (= 1, average), NOT TXA_MIC_PK
//                 (= 0, peak) — the existing getTxMicMeter() reads
//                 the peak; Thetis's UpdateNoiseGate uses the average.
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
// Phase 3M-3a-iii Task 17 — DEXP pushvox bridge static members
//
// s_voxKeyInstance is the single-instance lookup table for the WDSP DEXP
// pushvox callback.  3M-3a-iii ships exactly one TxChannel (channel id 1);
// phase 3F multi-pan TX will turn this into a small std::array indexed by id.
//
// s_pushVoxCallback is the C-callable bridge that WDSP fires from inside
// `xdexp` (wdsp/dexp.c:330,339 [v2.10.3.13]) on the audio worker thread
// (TxWorkerThread for NereusSDR).  Looks up the instance via the channel
// id passed by WDSP and emits voxActiveChanged — RadioModel routes that
// signal into MoxController::onVoxActive (Qt::AutoConnection promotes to
// QueuedConnection across threads).
//
// Defensive: if the lookup misses (instance null, or wrong channel id, or
// callback fires after the dtor cleared the slot), the bridge is a no-op.
// Without this guard a stray callback fired during teardown would
// dereference a stale pointer.
// ---------------------------------------------------------------------------
TxChannel* TxChannel::s_voxKeyInstance = nullptr;

void NEREUS_STDCALL TxChannel::s_pushVoxCallback(int id, int active)
{
    TxChannel* inst = s_voxKeyInstance;
    if (inst == nullptr || inst->m_channelId != id) {
        return;
    }
    emit inst->voxActiveChanged(active != 0);
}

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

    // Plan 4 D8 + bench-fix: the original 50 ms QTimer-based debounce was
    // removed because TxWorkerThread::run() is a custom semaphore-wake loop
    // (not QThread::exec()).  QTimer events only dispatch when processEvents()
    // is called between audio frames, which makes the timer unreliable —
    // bench-confirmed by JJ on ANAN-G2: spinbox changes never reached
    // SetTXABandpassFreqs.  See requestFilterChange comment for details.
    //
    // The timer + slot are retained as no-op members for ABI stability of the
    // public API surface (tst_tx_filter_offset_to_wdsp still references the
    // signal); requestFilterChange now calls applyTxFilterForMode directly.
    m_filterDebounceTimer.setParent(this);
    m_filterDebounceTimer.setSingleShot(true);
    m_filterDebounceTimer.setInterval(50);
    connect(&m_filterDebounceTimer, &QTimer::timeout,
            this, &TxChannel::applyPendingFilter);

    qCInfo(lcDsp) << "TxChannel" << m_channelId
                  << "wrapper constructed; WDSP TXA pipeline (31 stages)"
                  << "inBuf=" << m_inputBufferSize
                  << "outBuf=" << m_outputBufferSize;

    // Phase 3M-3a-iii Task 17 (bench fix): register the WDSP DEXP pushvox
    // callback so threshold crossings drive MoxController::onVoxActive
    // through the voxActiveChanged Qt signal.  Without this, [VOX] toggles
    // run_vox=1 in WDSP but mic envelope crossings have nowhere to go
    // (a->pushvox is NULL).  See registerVoxCallback() comment block for
    // the full root-cause + fix narrative.
    registerVoxCallback();
}

TxChannel::~TxChannel()
{
    // Phase 3M-3a-iii Task 17: clear s_voxKeyInstance and re-register
    // nullptr with WDSP so a late callback fired after destruction is a
    // no-op.  Order matters: must run BEFORE the QObject base-class
    // destructor invalidates the QObject so the WDSP unregister call
    // doesn't risk emitting from a dead object.
    unregisterVoxCallback();
}

// ---------------------------------------------------------------------------
// registerVoxCallback() — Phase 3M-3a-iii Task 17 (bench fix)
//
// Stores `this` in s_voxKeyInstance and wires SendCBPushDexpVox(channel,
// &s_pushVoxCallback) so the WDSP DEXP detector's threshold-crossing
// callback fires through to MoxController::onVoxActive via the Qt signal
// bridge.
//
// The registration is safe to issue here because Thetis create_xmtr
// (cmaster.c:130-157 [v2.10.3.13]) calls create_dexp BEFORE OpenChannel
// returns, and NereusSDR's WdspEngine::createTxChannel mirrors that order
// (OpenChannel(type=1) constructs the full TXA pipeline including DEXP
// before this constructor runs).  pdexp[m_channelId] is therefore non-null
// at this point — the same invariant the existing setVoxRun / setDexpRun
// wrappers rely on.
//
// One-instance assumption: 3M-3a-iii ships exactly one TxChannel (channel
// id 1).  Construction of a second TxChannel would silently overwrite
// s_voxKeyInstance — phase 3F multi-pan TX must replace the bare static
// pointer with a small std::array<TxChannel*, N> indexed by m_channelId.
// A diagnostic warning fires if the slot is already occupied so that
// future regressions (or test fixtures that construct two TxChannels)
// surface immediately.
//
// Cite: Thetis cmaster.cs:1125 [v2.10.3.13] — analogous registration
// against the ChannelMaster wrapper VOX (`SendCBPushVox(0, PushVoxDel)`).
// NereusSDR has no ChannelMaster shim, so the registration goes against
// WDSP's DEXP pushvox directly (wdsp/dexp.c:399-403 [v2.10.3.13]).
// ---------------------------------------------------------------------------
void TxChannel::registerVoxCallback()
{
    if (s_voxKeyInstance != nullptr && s_voxKeyInstance != this) {
        // Diagnostic: another TxChannel already owns the slot.  Phase 3F
        // multi-pan TX should replace this static pointer with a per-id
        // table; for 3M-3a-iii one-TxChannel-only is the contract.
        qCWarning(lcDsp) << "TxChannel" << m_channelId
                         << "registerVoxCallback: s_voxKeyInstance already set"
                         << "to channel" << s_voxKeyInstance->m_channelId
                         << "— overwriting (phase 3F follow-up: per-id table).";
    }
    s_voxKeyInstance = this;
#ifdef HAVE_WDSP
    // Phase 3M-1c TX pump v3: pdexp[ch] + txa.rsmpin.p null-guards.  Test
    // builds construct TxChannel directly without going through
    // WdspEngine::createTxChannel → OpenChannel(type=1), so neither
    // pdexp[m_channelId] nor txa[m_channelId].rsmpin.p is allocated and
    // SendCBPushDexpVox would dereference null (dexp.c:402:
    // `DEXP a = pdexp[id]; ... a->pushvox = pushvox;`).  Same pattern as
    // setVoxRun / setDexpRun / all other DEXP setters in this file.
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    if (pdexp[m_channelId] == nullptr) return;
    // From Thetis wdsp/dexp.c:399-403 [v2.10.3.13] — SendCBPushDexpVox impl.
    // Cite: Thetis cmaster.cs:1125 [v2.10.3.13] — analogous registration.
    SendCBPushDexpVox(m_channelId, &TxChannel::s_pushVoxCallback);
#endif
}

// ---------------------------------------------------------------------------
// unregisterVoxCallback() — Phase 3M-3a-iii Task 17 (bench fix)
//
// Symmetric tear-down for registerVoxCallback().  Re-registers nullptr with
// WDSP so any in-flight callback (e.g. one already dispatched on the audio
// worker thread but not yet executed) becomes a no-op when it runs, and
// clears s_voxKeyInstance.
//
// The HAVE_WDSP guard mirrors registerVoxCallback().  Skipping the WDSP
// call in test builds is safe: those builds either don't link WDSP at all
// (no callback to unregister) or never exercise the SendCBPushDexpVox
// path that sets a->pushvox to a non-null value.
// ---------------------------------------------------------------------------
void TxChannel::unregisterVoxCallback()
{
#ifdef HAVE_WDSP
    // Same null-guard pair as registerVoxCallback (and all DEXP setters in
    // this file).  Test builds never drove OpenChannel(type=1) so pdexp[]
    // is null and the WDSP unregister would crash; production builds
    // always have a live DEXP at dtor time.
    if (txa[m_channelId].rsmpin.p == nullptr) {
        // Skip WDSP call; still clear the lookup pointer below.
    } else if (pdexp[m_channelId] == nullptr) {
        // Skip WDSP call; still clear the lookup pointer below.
    } else {
        // Pass nullptr so any callback already in flight on the WDSP worker
        // thread becomes a no-op when it executes.
        SendCBPushDexpVox(m_channelId, nullptr);
    }
#endif
    if (s_voxKeyInstance == this) {
        s_voxKeyInstance = nullptr;
    }
}

// ---------------------------------------------------------------------------
// setDexpBuffer() — Phase 3M-3a-iii Task 20
//
// Wires the WdspEngine-owned DEXP I/O buffer pointer into this wrapper so
// pumpDexp() has a valid destination for its per-block memcpy.
//
// `dexpBuf` is a non-owning raw pointer; ownership stays with WdspEngine's
// per-channel m_dexpBuffers entry.  `sizeDoubles` is the buffer length in
// DOUBLES (== 2 * complex samples) and must equal 2 * inputBufferSize so
// the per-block memcpy in pumpDexp covers the entire DEXP detection window.
// nullptr / 0 detaches (e.g. on disconnect).
// ---------------------------------------------------------------------------
void TxChannel::setDexpBuffer(double* dexpBuf, std::size_t sizeDoubles)
{
    m_dexpBuffer = dexpBuf;
    m_dexpBufferSizeDoubles = sizeDoubles;
}

// ---------------------------------------------------------------------------
// pumpDexp() — Phase 3M-3a-iii Task 20
//
// Pump one audio block through the WDSP DEXP detector and run xdexp().
// Mirrors Thetis cmaster.c:388 [v2.10.3.13] xdexp(tx) call BEFORE
// fexchange0 at cmaster.c:389.
//
// Buffer architecture is PARALLEL-ONLY in NereusSDR (see WdspEngine.cpp
// create_dexp callsite for the full narrative): the block is copied
// byte-for-byte into the WdspEngine-owned per-channel DEXP buffer, then
// xdexp() runs.  The DEXP module's output is discarded — only the
// VOX-keying side effect (pushvox callback firing inside the DEXP state
// machine, see dexp.c:330,339 [v2.10.3.13]) is observable downstream.
//
// Null-safe degradation: skips the WDSP call if any precondition is
// missing (no buffer pointer, no input pointer, pdexp[id] null in test
// builds, or txa[].rsmpin.p null).  Production callsites always have
// every precondition satisfied because WdspEngine::createTxChannel
// orders create_dexp BEFORE constructing the wrapper AND wires the
// buffer pointer immediately after.
// ---------------------------------------------------------------------------
void TxChannel::pumpDexp(const double* interleavedIn)
{
    if (interleavedIn == nullptr || m_dexpBuffer == nullptr ||
        m_dexpBufferSizeDoubles == 0) {
        return;
    }
#ifdef HAVE_WDSP
    // Same null-guard pair as setVoxRun / registerVoxCallback / all DEXP
    // setters in this file.  Test builds never drove OpenChannel(type=1)
    // so txa[].rsmpin.p is nullptr; calling xdexp on a missing DEXP module
    // would dereference null.
    if (txa[m_channelId].rsmpin.p == nullptr) {
        return;
    }
    if (pdexp[m_channelId] == nullptr) {
        return;
    }
    // Copy the worker-thread-owned mic block into the WDSP-visible DEXP
    // buffer, then drive the per-block detector.  WDSP synchronises
    // internally via dexp.cs_update; no additional locking needed here.
    std::memcpy(m_dexpBuffer, interleavedIn, m_dexpBufferSizeDoubles * sizeof(double));
    xdexp(m_channelId);
#else
    // Non-HAVE_WDSP build: still copy into the buffer so the storage
    // exercise path matches between configs (the DEXP module isn't there
    // to consume it, but the memcpy itself stays exercised).
    std::memcpy(m_dexpBuffer, interleavedIn, m_dexpBufferSizeDoubles * sizeof(double));
#endif
}

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

    // CFIR is the CIC-compensating FIR filter that exists in the TXA pipeline
    // AFTER gen1 (TUNE tone insertion).  It is REQUIRED for Protocol 2's
    // higher-rate multi-stage CIC interpolators and MUST be OFF on Protocol 1
    // — on P1 the unmatched filter attenuates the post-gen tone by ~13 dB
    // (bench-measured TX I/Q peak 0.214 vs Thetis 0.98 on HL2 at the same
    // mag=0.99999 tune-tone setting).
    //
    // Authoritative source: Thetis ChannelMaster/cmaster.cs:525-533 [v2.10.3.14]
    //   if (CurrentRadioProtocol == RadioProtocol.USB) //p1
    //       WDSP.SetTXACFIRRun(txch, false);
    //   else
    //       WDSP.SetTXACFIRRun(txch, true);
    //
    // Use the connection's protocolVersion() to gate.  Default (no connection
    // wired in unit-test stubs) is P1 → CFIR OFF, matching the test
    // expectation of byte-identical output without WDSP filtering.
    const int proto = (m_connection ? m_connection->protocolVersion() : 1);
    const int cfirRun = (proto == 2) ? 1 : 0;

    if (on) {
        SetTXACFIRRun(m_channelId, cfirRun);   // p1=0 / p2=1 — Thetis cmaster.cs:525-533

        // Turn the TXA channel ON: state=1, dmode=0 (immediate start, no flush).
        // From Thetis console.cs:29595 [v2.10.3.13] — RX→TX transition:
        //   WDSP.SetChannelState(WDSP.id(1, 0), 1, 0);
        SetChannelState(m_channelId, 1, 0);   // channel.c:259 [v2.10.3.13]
    } else {
        // 3M-3a-iii Task 18: if VOX-listening is on, leave the WDSP TXA
        // channel running so the DEXP detector keeps receiving fexchange0
        // calls when MOX drops back to RX.  The pre-fexchange0 gate in
        // driveOneTxBlockFromInterleaved is (m_running || m_voxListening),
        // so dropping the WDSP channel state here would silently break the
        // VOX-listening path on every MOX→RX transition.  Only tear down
        // the WDSP channel when neither MOX nor VOX-listening wants the
        // pipeline up.
        const bool voxListening = m_voxListening.load(std::memory_order_acquire);
        if (!voxListening) {
            // Turn the TXA channel OFF: state=0, dmode=1 (drain in-flight samples).
            // From Thetis console.cs:29607 [v2.10.3.13] — TX→RX transition:
            //   WDSP.SetChannelState(WDSP.id(1, 0), 0, 1);   // turn off, drain
            //   (preceded by: Thread.Sleep(space_mox_delay); // default 0 // from PSDR MW0LGE [console.cs:29603])
            SetChannelState(m_channelId, 0, 1);   // channel.c:259 [v2.10.3.13]

            // Drop CFIR after channel drain so no residual samples process
            // through it on the next RX→TX engagement (P2 will re-arm above).
            SetTXACFIRRun(m_channelId, 0);
        }
    }
#endif // HAVE_WDSP
}

// ---------------------------------------------------------------------------
// setVoxListening()  [3M-3a-iii Task 18 — bench fix]
//
// Enable/disable continuous TXA pipeline pumping for VOX detection.
// Forces the pump to run independently of m_running so the WDSP DEXP
// detector can monitor mic envelope when MOX is off.  Radio-write side
// remains gated on m_running ONLY (see driveOneTxBlockFromInterleaved
// post-fexchange0 split).
//
// From Thetis wdsp/dexp.c:304 [v2.10.3.13]:
//   "DEXP code runs continuously so it can be used to trigger VOX also."
// Thetis's TXA pipeline is pumped continuously after channel-open (the
// ChannelMaster wrapper is driven by HPSDR EP6 Tx audio cadence, not by
// MOX state).  Audio.VOXEnabled in audio.cs:168-192 [v2.10.3.13] only
// flips DEXP's run_vox flag via cmaster.CMSetTXAVoxRun(0); it does not
// touch the channel state.  NereusSDR's TxWorkerThread + m_running gate
// is a power-saving departure from Thetis (no pumping when neither MOX
// nor VOX is in play).  This setter restores Thetis-equivalent VOX
// detection while keeping power saving everywhere else.
//
// Critical: when entering vox-listening mode the WDSP TXA channel must
// be in the running state (SetChannelState(channelId, 1, 0)) so
// fexchange0 actually processes audio.  The setRunning(true) path
// already does this for MOX engagement; we mirror it here.  When
// leaving vox-listening AND MOX is also off, drop the channel state.
// When MOX is on, leave WDSP state alone — setRunning manages it.
// ---------------------------------------------------------------------------
void TxChannel::setVoxListening(bool on)
{
    const bool wasOn = m_voxListening.exchange(on, std::memory_order_acq_rel);
    if (wasOn == on) {
        return;  // idempotent
    }

    qCDebug(lcDsp) << "TxChannel" << m_channelId
                   << (on ? "vox-listening ON (pump forced)"
                          : "vox-listening OFF");

#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) {
        return;  // WDSP not initialised — same null-guard as setRunning
    }

    // Mirror the WDSP TXA channel-state gating from setRunning() so
    // fexchange0 actually processes audio when we're pumping for VOX.
    const bool actualRunning = m_running.load(std::memory_order_acquire);
    if (on) {
        // Entering vox-listening: ensure WDSP TXA channel is on.
        // Idempotent if already on (setRunning(true) already called it).
        if (!actualRunning) {
            const int proto = (m_connection ? m_connection->protocolVersion() : 1);
            const int cfirRun = (proto == 2) ? 1 : 0;
            SetTXACFIRRun(m_channelId, cfirRun);
            SetChannelState(m_channelId, 1, 0);
        }
    } else {
        // Leaving vox-listening AND MOX is also off: drop the WDSP
        // TXA channel state (matches setRunning(false) path).
        if (!actualRunning) {
            SetChannelState(m_channelId, 0, 1);
            SetTXACFIRRun(m_channelId, 0);
        }
        // If MOX is on (m_running=true), leave WDSP state alone —
        // setRunning will manage it on the next MOX→RX transition.
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
// requestFilterChange()
//
// Plan 4 D8 — NereusSDR-original glue that wires TransmitModel::filterChanged
// into the WDSP bandpass via a 50 ms debounce.
//
// Stores the pending audio-space values and (re)starts the single-shot debounce
// timer.  Rapid successive calls (e.g. spinbox arrow-key held down) coalesce
// to a single applyPendingFilter() fire once the user pauses.
// ---------------------------------------------------------------------------
void TxChannel::requestFilterChange(int audioLowHz, int audioHighHz, DSPMode mode)
{
    // Plan 4 D8 follow-up — the original 50 ms QTimer-based debounce did not
    // work on TxWorkerThread.  TxWorkerThread::run() is a custom semaphore-
    // wake loop (not QThread::exec()), so QTimer events only get dispatched
    // when processEvents() is called between audio frames — which means the
    // timer effectively never fires while not transmitting, and even during
    // TX the dispatch cadence is unreliable on macOS.  Bench-confirmed by
    // JJ on ANAN-G2: TX BW spinbox changes never reached SetTXABandpassFreqs.
    //
    // Apply directly.  Rapid UI events (spinbox arrow-key held down) still
    // coalesce naturally via Qt's queued-event compression on the worker
    // thread.  The slot is cheap (mode dispatch + WDSP setter call), so the
    // per-tick cost is acceptable even without explicit debouncing.
    m_pendingAudioLow  = audioLowHz;
    m_pendingAudioHigh = audioHighHz;
    m_pendingMode      = mode;
    applyTxFilterForMode(audioLowHz, audioHighHz, mode);
}

// ---------------------------------------------------------------------------
// applyPendingFilter()
//
// Called when m_filterDebounceTimer fires (50 ms after the last
// requestFilterChange call).  Delegates to applyTxFilterForMode with the
// stored pending values.
// ---------------------------------------------------------------------------
void TxChannel::applyPendingFilter()
{
    applyTxFilterForMode(m_pendingAudioLow, m_pendingAudioHigh, m_pendingMode);
}

// ---------------------------------------------------------------------------
// applyTxFilterForMode()
//
// NereusSDR-original glue: maps audio-space Hz to IQ-space (signed) per mode,
// emits txFilterApplied, then calls setTxBandpass.
//
// Per-mode IQ-space sign convention from deskhpsdr/transmitter.c:2136-2186
// [@120188f] (same source as the TUN bandpass mapping at setTuneTone()
// lines 520-528 — the TUN path is NOT modified here; this function is called
// only from applyPendingFilter, never from setTuneTone):
//
//   USB family (USB / DIGU / CWU / SPEC / others): IQ = [+audioLow, +audioHigh]
//   LSB family (LSB / DIGL / CWL):                 IQ = [-audioHigh, -audioLow]
//   Symmetric  (AM / SAM / DSB / FM / DRM):        IQ = [-audioHigh, +audioHigh]
//     (low ignored for symmetric; only audioHigh defines the bandwidth)
//
// CW modes:  CWL → LSB family, CWU → USB family.  The tune-tone path swaps
// CWL→LSB and CWU→USB upstream (MoxController G.4) before calling setTuneTone,
// so reaching this function with a CW mode is unusual but handled safely.
// ---------------------------------------------------------------------------
void TxChannel::applyTxFilterForMode(int audioLowHz, int audioHighHz, DSPMode mode)
{
    // Per deskhpsdr/transmitter.c:2136-2186 [@120188f] — tx_set_filter per-mode
    // IQ-space sign convention.  Same mapping as setTuneTone() lines 520-528.
    auto isLsbFamily = [](DSPMode m) {
        return m == DSPMode::LSB || m == DSPMode::DIGL || m == DSPMode::CWL;
    };
    auto isSymmetric = [](DSPMode m) {
        return m == DSPMode::AM  || m == DSPMode::SAM
            || m == DSPMode::DSB || m == DSPMode::FM
            || m == DSPMode::DRM;
    };

    int iqLow, iqHigh;
    if (isLsbFamily(mode)) {
        // LSB family: negate and swap — bandpass sits below the carrier.
        iqLow  = -audioHighHz;
        iqHigh = -audioLowHz;
    } else if (isSymmetric(mode)) {
        // Symmetric modes: equal sidebands around the carrier.
        iqLow  = -audioHighHz;
        iqHigh = +audioHighHz;
    } else {
        // USB family (USB / DIGU / CWU / SPEC / others): positive sideband.
        iqLow  = +audioLowHz;
        iqHigh = +audioHighHz;
    }

    // Signal first so QSignalSpy sees the values before the WDSP call.
    emit txFilterApplied(iqLow, iqHigh);
    setTxBandpass(iqLow, iqHigh);
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
    // rsmpin.p is non-null.  Task 20 (commit 109c09e) ports create_dexp
    // into NereusSDR's WdspEngine::createTxChannel so pdexp[i] is now
    // non-null in production; the guard remains for unit-test builds
    // that don't drive WdspEngine::initialize().
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
// setDexpRun()  — Phase 3M-3a-iii Task 1
//
// Enable or disable DEXP audio-domain expansion (the master "noise gate" gate).
//
// Porting from Thetis cmaster.cs:166-167 [v2.10.3.13] — SetDEXPRun DLL import:
//   [DllImport("wdsp.dll", EntryPoint = "SetDEXPRun", ...)]
//   public static extern void SetDEXPRun (int id, bool run);
//
// Distinction note: SetDEXPRunVox (cmaster.cs:199-200, wrapped by setVoxRun
// in 3M-1b D.3) only gates whether the DEXP detector fires VOX keying.
// SetDEXPRun gates whether the audio is actually attenuated when below
// threshold — they are independent flags inside the same DEXP struct.
// See wdsp/dexp.c:409 [v2.10.3.13] comment:
//   "run != 0, puts dexp in the audio processing path; otherwise, it's only
//    used to trigger VOX"
//
// Thetis call-site setup.cs:18882-18888 [v2.10.3.13]:
//   private void chkDEXPEnable_CheckedChanged(object sender, EventArgs e)
//   {
//       if (initializing) return;
//       cmaster.SetDEXPRun(0, chkDEXPEnable.Checked);
//       console.NoiseGateEnabled = chkDEXPEnable.Checked;
//       chkDEXPLookAheadEnable_CheckedChanged(this, EventArgs.Empty);
//   }
//
// Idempotent: bool `==` guard against m_dexpRunLast.  WDSP signature uses
// `int` for the bool parameter; explicit cast applied (matches setVoxRun).
//
// From Thetis wdsp/dexp.c:407 [v2.10.3.13] — SetDEXPRun implementation.
// ---------------------------------------------------------------------------
void TxChannel::setDexpRun(bool run)
{
    if (run == m_dexpRunLast) return;  // idempotent guard
    m_dexpRunLast = run;
#ifdef HAVE_WDSP
    // From Thetis cmaster.cs:166-167 [v2.10.3.13]
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // Phase 3M-1c TX pump v3: pdexp[ch] null-guard — see setVoxRun for rationale.
    // SetDEXPRun (dexp.c:410) dereferences pdexp[id] under cs_update.
    if (pdexp[m_channelId] == nullptr) return;
    SetDEXPRun(m_channelId, run ? 1 : 0);
#else
    Q_UNUSED(run);
#endif
}

// ---------------------------------------------------------------------------
// setDexpDetectorTau()  — Phase 3M-3a-iii Task 1
//
// Set DEXP detector smoothing time constant (input-envelope low-pass).
//
// Porting from Thetis cmaster.cs:169-170 [v2.10.3.13] — SetDEXPDetectorTau:
//   [DllImport("wdsp.dll", EntryPoint = "SetDEXPDetectorTau", ...)]
//   public static extern void SetDEXPDetectorTau(int id, double tau);
//
// Units: seconds at the WDSP boundary (wdsp/dexp.c:468 [v2.10.3.13]
// "Time-constant for smoothing the signal for detection (seconds).").
// Thetis converts from ms at the call-site (setup.cs:18927-18931 [v2.10.3.13]):
//   private void udDEXPDetTau_ValueChanged(object sender, EventArgs e)
//   {
//       if (initializing) return;
//       cmaster.SetDEXPDetectorTau(0, (double)udDEXPDetTau.Value / 1000.0);
//   }
//
// NereusSDR follows the same idiom: callers pass milliseconds for symmetry
// with setVoxHangTime's seconds API would have been wrong here — Thetis
// exposes ms in the spinbox (udDEXPDetTau Min=1 Max=100, default 20 per
// setup.Designer.cs:45078-45093 [v2.10.3.13]) and divides by 1000 just
// before the WDSP call.  Wrapper clamps at the spinbox range, then divides.
//
// Range 1..100 ms (setup.Designer.cs:45078-45093 [v2.10.3.13]).  Default 20
// (setup.Designer.cs:45093).
//
// Idempotent guard: NaN-aware first-call sentinel, then qFuzzyCompare on
// the post-clamp ms value (matches setVoxAttackThreshold's pattern but uses
// qFuzzyCompare instead of `==` because the clamp can introduce a tiny
// representation difference for boundary inputs like 100.0 vs 100.0001).
//
// From Thetis wdsp/dexp.c:466 [v2.10.3.13] — SetDEXPDetectorTau impl.
// ---------------------------------------------------------------------------
void TxChannel::setDexpDetectorTau(double tauMs)
{
    // Range 1..100 ms per setup.Designer.cs:45078-45093 [v2.10.3.13].
    const double clamped = std::clamp(tauMs, 1.0, 100.0);
    if (!std::isnan(m_dexpDetectorTauMsLast)
        && qFuzzyCompare(clamped, m_dexpDetectorTauMsLast)) {
        return;  // idempotent guard
    }
    m_dexpDetectorTauMsLast = clamped;
#ifdef HAVE_WDSP
    // From Thetis cmaster.cs:169-170 [v2.10.3.13]
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // Phase 3M-1c TX pump v3: pdexp[ch] null-guard — see setVoxRun for rationale.
    if (pdexp[m_channelId] == nullptr) return;
    // ms→seconds for WDSP, matching setup.cs:18930 [v2.10.3.13]:
    //   cmaster.SetDEXPDetectorTau(0, (double)udDEXPDetTau.Value / 1000.0);
    SetDEXPDetectorTau(m_channelId, clamped / 1000.0);
#endif
}

// ---------------------------------------------------------------------------
// setDexpAttackTime()  — Phase 3M-3a-iii Task 2
//
// Set DEXP envelope attack time (low → high gain ramp duration).
//
// Porting from Thetis cmaster.cs:172-173 [v2.10.3.13] — SetDEXPAttackTime:
//   [DllImport("wdsp.dll", EntryPoint = "SetDEXPAttackTime", ...)]
//   public static extern void SetDEXPAttackTime(int id, double time);
//
// Units: seconds at the WDSP boundary (wdsp/dexp.c:481 [v2.10.3.13]
// "Set attack time, seconds.  0.002 - 0.100 should be a good range.").
// Thetis converts from ms at the call-site (setup.cs:18890-18894 [v2.10.3.13]):
//   private void udDEXPAttack_ValueChanged(object sender, EventArgs e)
//   {
//       if (initializing) return;
//       cmaster.SetDEXPAttackTime(0, (double)udDEXPAttack.Value / 1000.0);
//   }
//
// Range 2..100 ms (setup.Designer.cs:45035-45050 [v2.10.3.13]).  Default 2
// (setup.Designer.cs:45050).
//
// Idempotent guard: NaN-aware first-call sentinel + qFuzzyCompare on the
// post-clamp ms value (matches setDexpDetectorTau's pattern exactly).
//
// From Thetis wdsp/dexp.c:479 [v2.10.3.13] — SetDEXPAttackTime impl.
// ---------------------------------------------------------------------------
void TxChannel::setDexpAttackTime(double attackMs)
{
    // Range 2..100 ms per setup.Designer.cs:45035-45050 [v2.10.3.13].
    const double clamped = std::clamp(attackMs, 2.0, 100.0);
    if (!std::isnan(m_dexpAttackTimeMsLast)
        && qFuzzyCompare(clamped, m_dexpAttackTimeMsLast)) {
        return;  // idempotent guard
    }
    m_dexpAttackTimeMsLast = clamped;
#ifdef HAVE_WDSP
    // From Thetis cmaster.cs:172-173 [v2.10.3.13]
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // Phase 3M-1c TX pump v3: pdexp[ch] null-guard — see setVoxRun for rationale.
    if (pdexp[m_channelId] == nullptr) return;
    // ms→seconds for WDSP, matching setup.cs:18893 [v2.10.3.13]:
    //   cmaster.SetDEXPAttackTime(0, (double)udDEXPAttack.Value / 1000.0);
    SetDEXPAttackTime(m_channelId, clamped / 1000.0);
#endif
}

// ---------------------------------------------------------------------------
// setDexpReleaseTime()  — Phase 3M-3a-iii Task 2
//
// Set DEXP envelope release time (high → low gain ramp duration).
//
// Porting from Thetis cmaster.cs:175-176 [v2.10.3.13] — SetDEXPReleaseTime:
//   [DllImport("wdsp.dll", EntryPoint = "SetDEXPReleaseTime", ...)]
//   public static extern void SetDEXPReleaseTime(int id, double time);
//
// Units: seconds at the WDSP boundary (wdsp/dexp.c:494 [v2.10.3.13]
// "Set release time, seconds.  0.002 - 0.999 should be a good range.").
// Thetis converts from ms at the call-site (setup.cs:18902-18906 [v2.10.3.13]):
//   private void udDEXPRelease_ValueChanged(object sender, EventArgs e)
//   {
//       if (initializing) return;
//       cmaster.SetDEXPReleaseTime(0, (double)udDEXPRelease.Value / 1000.0);
//   }
//
// Range 2..1000 ms (setup.Designer.cs:44975-44990 [v2.10.3.13]).  Default 100
// (setup.Designer.cs:44990).
//
// Idempotent guard: NaN-aware first-call sentinel + qFuzzyCompare on the
// post-clamp ms value (matches setDexpAttackTime's pattern exactly).
//
// From Thetis wdsp/dexp.c:492 [v2.10.3.13] — SetDEXPReleaseTime impl.
// ---------------------------------------------------------------------------
void TxChannel::setDexpReleaseTime(double releaseMs)
{
    // Range 2..1000 ms per setup.Designer.cs:44975-44990 [v2.10.3.13].
    const double clamped = std::clamp(releaseMs, 2.0, 1000.0);
    if (!std::isnan(m_dexpReleaseTimeMsLast)
        && qFuzzyCompare(clamped, m_dexpReleaseTimeMsLast)) {
        return;  // idempotent guard
    }
    m_dexpReleaseTimeMsLast = clamped;
#ifdef HAVE_WDSP
    // From Thetis cmaster.cs:175-176 [v2.10.3.13]
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // Phase 3M-1c TX pump v3: pdexp[ch] null-guard — see setVoxRun for rationale.
    if (pdexp[m_channelId] == nullptr) return;
    // ms→seconds for WDSP, matching setup.cs:18905 [v2.10.3.13]:
    //   cmaster.SetDEXPReleaseTime(0, (double)udDEXPRelease.Value / 1000.0);
    SetDEXPReleaseTime(m_channelId, clamped / 1000.0);
#endif
}

// ---------------------------------------------------------------------------
// setDexpExpansionRatio()  — Phase 3M-3a-iii Task 3
//
// Set DEXP downward-expansion ratio (the "depth" of the gate).
//
// Porting from Thetis cmaster.cs:181-182 [v2.10.3.13] — SetDEXPExpansionRatio:
//   [DllImport("wdsp.dll", EntryPoint = "SetDEXPExpansionRatio", ...)]
//   public static extern void SetDEXPExpansionRatio(int id, double ratio);
//
// WDSP takes a LINEAR ratio (wdsp/dexp.c:518-528 [v2.10.3.13]; comment at
// dexp.c:520-521: "High_gain = 1.0; Low_gain = 1.0/exp_ratio.  Range of
// 1.0 - 30.0 should be good.  Could use dB:  0.0 - 30.0dB.").
//
// Thetis converts from dB at the call-site (setup.cs:18915-18919 [v2.10.3.13]):
//   private void udDEXPExpansionRatio_ValueChanged(object sender, EventArgs e)
//   {
//       if (initializing) return;
//       cmaster.SetDEXPExpansionRatio(0,
//                                     Math.Pow(10.0, (double)udDEXPExpansionRatio.Value / 20.0));
//   }
//
// Note POSITIVE sign in Math.Pow.  This is opposite to Hysteresis which uses
// negative.  More dB → larger linear ratio → harder gate.
//
// Range 0..30 dB (setup.Designer.cs:44885-44900 [v2.10.3.13]).  Default 10.0
// (setup.Designer.cs:44900-44904; raw decimal Value=10 with no scale shift
// = 10.0 dB).
//
// Idempotent guard: NaN-aware first-call sentinel + qFuzzyCompare on the
// post-clamp dB value (matches the timing setters' pattern; comparison is
// in user-facing dB so re-pushes of the same dB are absorbed even if the
// linear conversion would otherwise jitter on FP rounding).
//
// From Thetis wdsp/dexp.c:518 [v2.10.3.13] — SetDEXPExpansionRatio impl.
// ---------------------------------------------------------------------------
void TxChannel::setDexpExpansionRatio(double ratioDb)
{
    // Range 0..30 dB per setup.Designer.cs:44885-44900 [v2.10.3.13].
    const double clamped = std::clamp(ratioDb, 0.0, 30.0);
    if (!std::isnan(m_dexpExpansionRatioDbLast)
        && qFuzzyCompare(clamped, m_dexpExpansionRatioDbLast)) {
        return;  // idempotent guard
    }
    m_dexpExpansionRatioDbLast = clamped;
#ifdef HAVE_WDSP
    // From Thetis cmaster.cs:181-182 [v2.10.3.13]
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // Phase 3M-1c TX pump v3: pdexp[ch] null-guard — see setVoxRun for rationale.
    if (pdexp[m_channelId] == nullptr) return;
    // dB→linear via Math.Pow(10, dB/20.0) — POSITIVE sign — matches Thetis
    // setup.cs:18918 [v2.10.3.13]:
    //   cmaster.SetDEXPExpansionRatio(0,
    //                                 Math.Pow(10.0, (double)udDEXPExpansionRatio.Value / 20.0));
    SetDEXPExpansionRatio(m_channelId, std::pow(10.0, clamped / 20.0));
#endif
}

// ---------------------------------------------------------------------------
// setDexpHysteresisRatio()  — Phase 3M-3a-iii Task 3
//
// Set DEXP hysteresis ratio (release-vs-attack threshold gap).
//
// Porting from Thetis cmaster.cs:184-185 [v2.10.3.13] — SetDEXPHysteresisRatio:
//   [DllImport("wdsp.dll", EntryPoint = "SetDEXPHysteresisRatio", ...)]
//   public static extern void SetDEXPHysteresisRatio(int id, double ratio);
//
// WDSP takes a LINEAR ratio (wdsp/dexp.c:531-541 [v2.10.3.13]; comment at
// dexp.c:533-534: "Hold_thresh = hysteresis_ratio * Attack_thresh.  Expose
// to operator in dB: 0.0dB - 9.9dB should be good (1.000 - 0.320).").
//
// Thetis converts from dB at the call-site (setup.cs:18921-18925 [v2.10.3.13]):
//   private void udDEXPHysteresisRatio_ValueChanged(object sender, EventArgs e)
//   {
//       if (initializing) return;
//       cmaster.SetDEXPHysteresisRatio(0,
//                                      Math.Pow(10.0, -(double)udDEXPHysteresisRatio.Value / 20.0));
//   }
//
// IMPORTANT — note the NEGATIVE sign in Math.Pow.  This is the opposite of
// ExpansionRatio (which uses positive).  Hold_thresh sits BELOW Attack_thresh
// (hysteresis_ratio < 1.0 always), so user-facing dB grows as the linear
// ratio shrinks: 0 dB = 1.0 (no gap), 6 dB = 0.5 (release at half attack),
// 9.9 dB = 0.32 (release at ~1/3 attack — wide hysteresis, harder to chatter).
//
// Range 0..10 dB (setup.Designer.cs:44854-44869 [v2.10.3.13]).  Default 2.0
// (setup.Designer.cs:44869-44873; raw decimal Value=20 with DecimalPlaces=1
// / scale shift 65536 = 2.0 dB).
//
// Idempotent guard: NaN-aware first-call sentinel + qFuzzyCompare on the
// post-clamp dB value (matches setDexpExpansionRatio's pattern exactly).
//
// From Thetis wdsp/dexp.c:531 [v2.10.3.13] — SetDEXPHysteresisRatio impl.
// ---------------------------------------------------------------------------
void TxChannel::setDexpHysteresisRatio(double ratioDb)
{
    // Range 0..10 dB per setup.Designer.cs:44854-44869 [v2.10.3.13].
    const double clamped = std::clamp(ratioDb, 0.0, 10.0);
    if (!std::isnan(m_dexpHysteresisRatioDbLast)
        && qFuzzyCompare(clamped, m_dexpHysteresisRatioDbLast)) {
        return;  // idempotent guard
    }
    m_dexpHysteresisRatioDbLast = clamped;
#ifdef HAVE_WDSP
    // From Thetis cmaster.cs:184-185 [v2.10.3.13]
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // Phase 3M-1c TX pump v3: pdexp[ch] null-guard — see setVoxRun for rationale.
    if (pdexp[m_channelId] == nullptr) return;
    // dB→linear via Math.Pow(10, -dB/20.0) — NEGATIVE sign — matches Thetis
    // setup.cs:18924 [v2.10.3.13]:
    //   cmaster.SetDEXPHysteresisRatio(0,
    //                                  Math.Pow(10.0, -(double)udDEXPHysteresisRatio.Value / 20.0));
    SetDEXPHysteresisRatio(m_channelId, std::pow(10.0, -clamped / 20.0));
#endif
}

// ---------------------------------------------------------------------------
// setDexpLowCut()  — Phase 3M-3a-iii Task 4
//
// Set the side-channel band-pass filter low-cut frequency (Hz).
//
// Porting from Thetis cmaster.cs:190-191 [v2.10.3.13] — SetDEXPLowCut:
//   [DllImport("wdsp.dll", EntryPoint = "SetDEXPLowCut", ...)]
//   public static extern void SetDEXPLowCut(int id, double lowcut);
//
// Units: Hz on both sides (wdsp/dexp.c:584 [v2.10.3.13] comment "Set
// side-channel filter low_cut (Hertz)."  No unit conversion needed.
// Thetis call-site: setup.cs:18939-18943 udSCFLowCut_ValueChanged:
//   private void udSCFLowCut_ValueChanged(object sender, EventArgs e)
//   {
//       if (initializing) return;
//       cmaster.SetDEXPLowCut(0, (double)udSCFLowCut.Value);
//   }
//
// Range 100..10000 Hz (setup.Designer.cs:45230-45235 [v2.10.3.13]).
// Default 500 Hz (setup.Designer.cs:45240-45245).
//
// Idempotent guard: NaN-aware first-call sentinel + qFuzzyCompare on the
// post-clamp Hz value.
//
// From Thetis wdsp/dexp.c:582 [v2.10.3.13] — SetDEXPLowCut impl.
// ---------------------------------------------------------------------------
void TxChannel::setDexpLowCut(double lowCutHz)
{
    // Range 100..10000 Hz per setup.Designer.cs:45230-45235 [v2.10.3.13].
    const double clamped = std::clamp(lowCutHz, 100.0, 10000.0);
    if (!std::isnan(m_dexpLowCutHzLast)
        && qFuzzyCompare(clamped, m_dexpLowCutHzLast)) {
        return;  // idempotent guard
    }
    m_dexpLowCutHzLast = clamped;
#ifdef HAVE_WDSP
    // From Thetis cmaster.cs:190-191 [v2.10.3.13]
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // Phase 3M-1c TX pump v3: pdexp[ch] null-guard — see setVoxRun for rationale.
    if (pdexp[m_channelId] == nullptr) return;
    SetDEXPLowCut(m_channelId, clamped);  // Hz, no conversion
#endif
}

// ---------------------------------------------------------------------------
// setDexpHighCut()  — Phase 3M-3a-iii Task 4
//
// Set the side-channel band-pass filter high-cut frequency (Hz).
//
// Porting from Thetis cmaster.cs:193-194 [v2.10.3.13] — SetDEXPHighCut:
//   [DllImport("wdsp.dll", EntryPoint = "SetDEXPHighCut", ...)]
//   public static extern void SetDEXPHighCut(int id, double highcut);
//
// Units: Hz on both sides (wdsp/dexp.c:596 [v2.10.3.13] comment "Set
// side-channel filter high_cut (Hertz)."  No unit conversion needed.
// Thetis call-site: setup.cs:18945-18949 udSCFHighCut_ValueChanged:
//   private void udSCFHighCut_ValueChanged(object sender, EventArgs e)
//   {
//       if (initializing) return;
//       cmaster.SetDEXPHighCut(0, (double)udSCFHighCut.Value);
//   }
//
// Range 100..10000 Hz (setup.Designer.cs:45200-45205 [v2.10.3.13]).
// Default 1500 Hz (setup.Designer.cs:45210-45215).
//
// Idempotent guard: NaN-aware first-call sentinel + qFuzzyCompare on the
// post-clamp Hz value.
//
// From Thetis wdsp/dexp.c:594 [v2.10.3.13] — SetDEXPHighCut impl.
// ---------------------------------------------------------------------------
void TxChannel::setDexpHighCut(double highCutHz)
{
    // Range 100..10000 Hz per setup.Designer.cs:45200-45205 [v2.10.3.13].
    const double clamped = std::clamp(highCutHz, 100.0, 10000.0);
    if (!std::isnan(m_dexpHighCutHzLast)
        && qFuzzyCompare(clamped, m_dexpHighCutHzLast)) {
        return;  // idempotent guard
    }
    m_dexpHighCutHzLast = clamped;
#ifdef HAVE_WDSP
    // From Thetis cmaster.cs:193-194 [v2.10.3.13]
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // Phase 3M-1c TX pump v3: pdexp[ch] null-guard — see setVoxRun for rationale.
    if (pdexp[m_channelId] == nullptr) return;
    SetDEXPHighCut(m_channelId, clamped);  // Hz, no conversion
#endif
}

// ---------------------------------------------------------------------------
// setDexpRunSideChannelFilter()  — Phase 3M-3a-iii Task 4
//
// Enable or disable the side-channel band-pass filter (the master enable
// for the detector-input shaping filter).
//
// Porting from Thetis cmaster.cs:196-197 [v2.10.3.13] —
// SetDEXPRunSideChannelFilter:
//   [DllImport("wdsp.dll", EntryPoint = "SetDEXPRunSideChannelFilter", ...)]
//   public static extern void SetDEXPRunSideChannelFilter(int id, bool run);
//
// WDSP impl wdsp/dexp.c:606 [v2.10.3.13]; comment at dexp.c:608 "Turn
// OFF/ON the side-channel filter and its compensating delay."
//
// Thetis call-site setup.cs:18933-18937 [v2.10.3.13]:
//   private void chkSCFEnable_CheckedChanged(object sender, EventArgs e)
//   {
//       if (initializing) return;
//       cmaster.SetDEXPRunSideChannelFilter(0, chkSCFEnable.Checked);
//   }
//
// Thetis ships chkSCFEnable as DEFAULT CHECKED
// (setup.Designer.cs:45250-45251 [v2.10.3.13]:
// chkSCFEnable.Checked = true), but the wrapper-side cache initialises
// to false to match the WDSP create_dexp boot state (a->run_filt = 0).
// Caller (UI/model) must push true at startup if Thetis-default behavior
// is desired.
//
// Idempotent: bool `==` guard against m_dexpRunSideChannelFilterLast.
// WDSP signature uses `int` for the bool parameter; explicit cast
// applied (matches setVoxRun / setDexpRun).
//
// From Thetis wdsp/dexp.c:606 [v2.10.3.13] — SetDEXPRunSideChannelFilter impl.
// ---------------------------------------------------------------------------
void TxChannel::setDexpRunSideChannelFilter(bool run)
{
    if (run == m_dexpRunSideChannelFilterLast) return;  // idempotent guard
    m_dexpRunSideChannelFilterLast = run;
#ifdef HAVE_WDSP
    // From Thetis cmaster.cs:196-197 [v2.10.3.13]
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // Phase 3M-1c TX pump v3: pdexp[ch] null-guard — see setVoxRun for rationale.
    if (pdexp[m_channelId] == nullptr) return;
    SetDEXPRunSideChannelFilter(m_channelId, run ? 1 : 0);
#else
    Q_UNUSED(run);
#endif
}

// ---------------------------------------------------------------------------
// setDexpRunAudioDelay()  — Phase 3M-3a-iii Task 5
//
// Enable or disable the DEXP audio delay line (look-ahead).  When ON, the
// audio is delayed by N ms so the DEXP detector can peek at samples that
// haven't been gated yet — VOX or the expander can open BEFORE the first
// syllable instead of clipping it.
//
// Porting from Thetis cmaster.cs:202-203 [v2.10.3.13] — SetDEXPRunAudioDelay:
//   [DllImport("wdsp.dll", EntryPoint = "SetDEXPRunAudioDelay", ...)]
//   public static extern void SetDEXPRunAudioDelay(int id, bool run);
//
// WDSP impl wdsp/dexp.c:626 [v2.10.3.13]; comment at dexp.c:628 "Turn
// OFF/ON audio delay line."
//
// Thetis call-site setup.cs:18951-18956 [v2.10.3.13]:
//   private void chkDEXPLookAheadEnable_CheckedChanged(object sender, EventArgs e)
//   {
//       if (initializing) return;
//       bool enable = chkDEXPLookAheadEnable.Checked
//                     && (chkVOXEnable.Checked || chkDEXPEnable.Checked);
//       cmaster.SetDEXPRunAudioDelay(0, enable);
//   }
//
// Note Thetis ANDs the user checkbox with the global VOX or DEXP enable
// at the call-site — that gating is a UI-layer responsibility; the
// wrapper just pushes the bool the caller hands it.
//
// Thetis ships chkDEXPLookAheadEnable as DEFAULT CHECKED
// (setup.Designer.cs:44808-44809 [v2.10.3.13]), but the wrapper-side
// cache initialises to false to match the WDSP create_dexp boot state
// (a->run_audelay = 0).  Caller (UI/model) is responsible for pushing
// true at startup if Thetis-default behavior is desired.
//
// Idempotent: bool `==` guard against m_dexpRunAudioDelayLast.
// WDSP signature uses `int` for the bool parameter; explicit cast applied.
//
// From Thetis wdsp/dexp.c:626 [v2.10.3.13] — SetDEXPRunAudioDelay impl.
// ---------------------------------------------------------------------------
void TxChannel::setDexpRunAudioDelay(bool run)
{
    if (run == m_dexpRunAudioDelayLast) return;  // idempotent guard
    m_dexpRunAudioDelayLast = run;
#ifdef HAVE_WDSP
    // From Thetis cmaster.cs:202-203 [v2.10.3.13]
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // Phase 3M-1c TX pump v3: pdexp[ch] null-guard — see setVoxRun for rationale.
    if (pdexp[m_channelId] == nullptr) return;
    SetDEXPRunAudioDelay(m_channelId, run ? 1 : 0);
#else
    Q_UNUSED(run);
#endif
}

// ---------------------------------------------------------------------------
// setDexpAudioDelay()  — Phase 3M-3a-iii Task 5
//
// Set the DEXP audio look-ahead delay (ms at the wrapper, seconds at WDSP).
//
// Porting from Thetis cmaster.cs:205-206 [v2.10.3.13] — SetDEXPAudioDelay:
//   [DllImport("wdsp.dll", EntryPoint = "SetDEXPAudioDelay", ...)]
//   public static extern void SetDEXPAudioDelay(int id, double delay);
//
// Units: seconds at the WDSP boundary (wdsp/dexp.c:638 [v2.10.3.13]
// comment "Set the audio delay, seconds.").
// Thetis converts from ms at the call-site (setup.cs:18958-18962 [v2.10.3.13]):
//   private void udDEXPLookAhead_ValueChanged(object sender, EventArgs e)
//   {
//       if (initializing) return;
//       cmaster.SetDEXPAudioDelay(0, (double)udDEXPLookAhead.Value / 1000.0);
//   }
//
// Range 10..999 ms (setup.Designer.cs:44773-44782 [v2.10.3.13]).  Default
// 60 ms (setup.Designer.cs:44788).
//
// Idempotent guard: NaN-aware first-call sentinel + qFuzzyCompare on the
// post-clamp ms value (matches the timing setters' pattern).
//
// From Thetis wdsp/dexp.c:636 [v2.10.3.13] — SetDEXPAudioDelay impl.
// ---------------------------------------------------------------------------
void TxChannel::setDexpAudioDelay(double delayMs)
{
    // Range 10..999 ms per setup.Designer.cs:44773-44782 [v2.10.3.13].
    const double clamped = std::clamp(delayMs, 10.0, 999.0);
    if (!std::isnan(m_dexpAudioDelayMsLast)
        && qFuzzyCompare(clamped, m_dexpAudioDelayMsLast)) {
        return;  // idempotent guard
    }
    m_dexpAudioDelayMsLast = clamped;
#ifdef HAVE_WDSP
    // From Thetis cmaster.cs:205-206 [v2.10.3.13]
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    // Phase 3M-1c TX pump v3: pdexp[ch] null-guard — see setVoxRun for rationale.
    if (pdexp[m_channelId] == nullptr) return;
    // ms→seconds for WDSP, matching setup.cs:18961 [v2.10.3.13]:
    //   cmaster.SetDEXPAudioDelay(0, (double)udDEXPLookAhead.Value / 1000.0);
    SetDEXPAudioDelay(m_channelId, clamped / 1000.0);
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
    //
    // 3M-3a-iii Task 18 (bench fix): pump must run if EITHER m_running OR
    // m_voxListening is true.  Without the m_voxListening branch the WDSP
    // DEXP detector never sees mic envelope when MOX is off, so VOX cannot
    // engage MOX (chicken-and-egg).  See setVoxListening doc-comment and
    // wdsp/dexp.c:304 [v2.10.3.13].  Radio-write side is gated separately
    // in driveOneTxBlockFromInterleaved on m_running ONLY.
    const bool actualRunning = m_running.load(std::memory_order_acquire);
    const bool voxListening  = m_voxListening.load(std::memory_order_acquire);
    if ((!actualRunning && !voxListening) || !m_connection) {
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
    //
    // 3M-3a-iii Task 18 (bench fix): two-gate split.
    //   shouldPumpDsp = m_running || m_voxListening
    //                — controls fexchange0 dispatch (DEXP needs samples).
    //   shouldWriteRx = m_running ONLY
    //                — controls sendTxIq (radio-write).
    // The gates are independent so VOX-listening pumps the WDSP TXA pipeline
    // (so the DEXP detector can see mic envelope and fire pushvox to engage
    // MOX), but TX I/Q never reaches the radio wire until MOX engages for
    // real (m_running=true via the existing MoxController::txReady chain).
    //
    // From Thetis wdsp/dexp.c:304 [v2.10.3.13]:
    //   "DEXP code runs continuously so it can be used to trigger VOX also."
    // In Thetis the TXA pipeline pumps continuously regardless of MOX
    // (HPSDR EP6 audio cadence drives ChannelMaster, not MOX state); MOX
    // gates radio-write at the connection layer.  NereusSDR mirrors that
    // via the two-gate split here.
    const bool actualRunning = m_running.load(std::memory_order_acquire);
    const bool voxListening  = m_voxListening.load(std::memory_order_acquire);
    const bool shouldPumpDsp = actualRunning || voxListening;
    if (!shouldPumpDsp || !m_connection) {
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

    // 3M-3a-iii Task 18 (bench fix): radio-write split.
    //
    // VOX-listening only (m_running=false, m_voxListening=true) means we
    // pumped the WDSP TXA pipeline so the DEXP detector could see mic
    // envelope, but no TX I/Q reaches the radio wire.  When VOX fires
    // (DEXP's pushvox callback → MoxController::onVoxActive → MOX engage),
    // m_running flips true via MoxController::txReady → setRunning(true)
    // and the radio-write resumes normally on the next pump cycle.
    //
    // The MON-path siphon (sip1OutputReady) below is also skipped — MON
    // audio is post-SSB-modulator output that we don't want to monitor
    // until the operator is actually transmitting.
    const bool shouldWriteRx = actualRunning;
    if (!shouldWriteRx) {
        return;
    }

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
// getDexpPeakSignal()  — Phase 3M-3a-iii Task 6
//
// Live VOX peak signal in LINEAR amplitude (typical 0.0..1.0).
//
// Porting from Thetis cmaster.cs:163-164 [v2.10.3.13] —
// GetDEXPPeakSignal DLL import:
//   [DllImport("wdsp.dll", EntryPoint = "GetDEXPPeakSignal", ...)]
//   public static extern void GetDEXPPeakSignal(int id, double* peak);
//
// Caller pattern from Thetis console.cs:28949-28960 [v2.10.3.13] —
// picVOX_Paint:
//   double audio_peak = 0.0;
//   cmaster.GetDEXPPeakSignal(0, &audio_peak);
//   if (mic_boost) audio_peak /= Audio.VOXGain;
//   int peak_x = (int)(picVOX.Width - 20.0 * Math.Log10(audio_peak)
//                      * picVOX.Width / ptbVOX.Minimum);
//
// The 20*Math.Log10 conversion to dB is the CALLER's responsibility — the
// wrapper returns the raw linear `a->peak` snapshot.  Mic-boost compensation
// (audio_peak /= Audio.VOXGain) is also caller-side.
//
// Porting from Thetis wdsp/dexp.c:647-654 [v2.10.3.13] — implementation:
//   PORT
//   void GetDEXPPeakSignal (int id, double* peak)
//   {
//       DEXP a = pdexp[id];
//       EnterCriticalSection (&a->cs_update);
//       *peak = a->peak;
//       LeaveCriticalSection (&a->cs_update);
//   }
//
// Note the immediate `pdexp[id]` deref — same pre-deref pattern as the
// existing setVoxRun guard (TxChannel.cpp:1219).  Without a null-guard,
// calling this on an unopened TX channel segfaults under unit-test builds.
// ---------------------------------------------------------------------------
double TxChannel::getDexpPeakSignal() const noexcept
{
#ifdef HAVE_WDSP
    // From Thetis wdsp/dexp.c:650 [v2.10.3.13] — `DEXP a = pdexp[id];`
    // dereferences before any guard.  Match the existing setVoxRun /
    // setDexpRun null-guards (pdexp[m_channelId] == nullptr) so that
    // unopened TX channels return the Thetis-faithful idle default
    // (console.cs:28951 [v2.10.3.13]: `double audio_peak = 0.0;`).
    if (pdexp[m_channelId] == nullptr) return 0.0;
    double peak = 0.0;
    GetDEXPPeakSignal(m_channelId, &peak);
    return peak;
#else
    return 0.0;
#endif
}

// ---------------------------------------------------------------------------
// getTxMicMeterDb()  — Phase 3M-3a-iii Task 6
//
// Live TX mic-meter reading in RAW dB.  Reads WDSP TXA_MIC_AV (= 1) via
// GetTXAMeter — the AVERAGE mic level, NOT the peak (TXA_MIC_PK = 0).
//
// Porting from Thetis console.cs:25346-25359 [v2.10.3.13] — UpdateNoiseGate:
//   private float noise_gate_data = -200.0f;
//   private async void UpdateNoiseGate()
//   {
//       while (chkPower.Checked)
//       {
//           if (_mox)
//           {
//               float num = -WDSP.CalculateTXMeter(1, WDSP.MeterType.MIC);
//               noise_gate_data = num + 3.0f;
//               picNoiseGate.Invalidate();
//           }
//           await Task.Delay(100);
//       }
//   }
//
// Porting from Thetis dsp.cs:992-1057 [v2.10.3.13] — CalculateTXMeter:
//   public static float CalculateTXMeter (uint thread, MeterType MT)
//   {
//       int channel = ...;   // txachannel
//       double val;
//       switch (MT)
//       {
//       case MeterType.MIC:
//           val = GetTXAMeter(channel, txaMeterType.TXA_MIC_AV);  // [TXA.h:52]
//           break;
//       ...
//       }
//       return -(float)val;   // SIGN-FLIP at WDSP boundary
//   }
//
// Net effect at the UpdateNoiseGate callsite: `num` ends up as the raw
// negative dB value (the leading `-` cancels CalculateTXMeter's sign-flip).
// Our wrapper short-circuits the double-flip and returns `val` directly.
//
// CALLER RESPONSIBILITIES (mirroring Thetis console.cs:25353-25354):
//   1. Sign treatment — already correct; getTxMicMeterDb() returns the raw
//      negative dB.  No further flip needed at the call-site.
//   2. +3 dB offset (console.cs:25354 `noise_gate_data = num + 3.0f`) —
//      apply at the call-site if mirroring picNoiseGate exactly.
//
// Porting from Thetis wdsp/TXA.h:51-52 [v2.10.3.13]:
//   TXA_MIC_PK = 0  (peak — used by getTxMicMeter / 3M-1b D.7 path)
//   TXA_MIC_AV = 1  (average — used HERE for picNoiseGate)
//
// Porting from Thetis wdsp/meter.c:151-159 [v2.10.3.13] — GetTXAMeter:
//   double GetTXAMeter(int channel, int mt)
//   { ... val = txa[channel].meter[mt]; ... return val; }
//
// Idle return value -200.0 matches Thetis's noise_gate_data initialiser
// (console.cs:25346 [v2.10.3.13]: `private float noise_gate_data = -200.0f;`).
// ---------------------------------------------------------------------------
double TxChannel::getTxMicMeterDb() const noexcept
{
#ifdef HAVE_WDSP
    // Same null-guard pattern as getTxMicMeter() (TxChannel.cpp:2200) and
    // getAlcMeter() — txa[].rsmpin.p == nullptr means OpenChannel was
    // never called, so GetTXAMeter would deref a null CRITICAL_SECTION
    // pointer (wdsp/meter.c:153 [v2.10.3.13]) and segfault.
    if (txa[m_channelId].rsmpin.p == nullptr) return -200.0;  // Thetis floor (console.cs:25346)
    return GetTXAMeter(m_channelId, TXA_MIC_AV);  // TXA_MIC_AV = 1 [TXA.h:52]
#else
    return -200.0;
#endif
}

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

// ── Issue #175 Task 2: HL2 sub-step DSP audio-gain modulation ────────────────
//
// From mi0bot-Thetis console.cs:47666 [v2.10.3.13-beta2] — HL2 TUNE path:
//   if (new_pwr <= 51) {
//       radio.GetDSPTX(0).TXPostGenToneMag = (double)(new_pwr + 40) / 100;
//       new_pwr = 0;
//   } else {
//       radio.GetDSPTX(0).TXPostGenToneMag = 0.9999;
//       new_pwr = (new_pwr - 54) * 2;
//   }
// WDSP: SetTXAPostGenToneMag — third_party/wdsp/src/gen.c:800-805 [v2.10.3.13]
//   txa[ch].gen1.p->tone.mag = mag;
void TxChannel::setPostGenToneMag(double mag)
{
    m_postGenToneMag = mag;
#ifdef HAVE_WDSP
    if (txa[m_channelId].rsmpin.p == nullptr) return;
    SetTXAPostGenToneMag(m_channelId, mag);   // gen.c:800 [v2.10.3.13]
#else
    Q_UNUSED(mag);
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

// ── B-3.2: TX CFC live-display readback (Phase 3M-3a-ii follow-up) ──────────
//
// Main-thread readback of WDSP's CFC compression-display snapshot for the
// parametric-EQ widget bar chart.  WDSP populates `cfc_gain_copy[]` and
// `delta_copy[]` snapshots inside the audio thread (cfcomp.c:558-566
// [v2.10.3.13] — sets mask_ready=1 once per FFT mask update).  The wrapper
// is csDSP-protected inside WDSP via EnterCriticalSection (cfcomp.c:744)
// so it is safe to call from the main thread while the audio thread runs.
//
// Forward-declare the WDSP entry point — neither cfcomp.h nor TXA.h export it
// in TAPR v1.29 (it's PORT-tagged in cfcomp.c only).  C# Thetis declares it
// via P/Invoke at dsp.cs:798-800 [v2.10.3.13]:
//   [DllImport("wdsp.dll", EntryPoint="GetTXACFCOMPDisplayCompression", ...)]
//   public static extern void GetTXACFCOMPDisplayCompression(int, double*, int*);

#ifdef HAVE_WDSP
extern "C" {
    void GetTXACFCOMPDisplayCompression(int channel, double* compValues, int* ready);
}
#endif

// From Thetis cfcomp.c:740-757 [v2.10.3.13].
bool TxChannel::getCfcDisplayCompression(double* compValues, int bufferSize) noexcept
{
    if (compValues == nullptr) return false;
    if (bufferSize < kCfcDisplayBinCount) return false;
#ifdef HAVE_WDSP
    int ready = 0;
    GetTXACFCOMPDisplayCompression(m_channelId, compValues, &ready);
    return ready != 0;
#else
    Q_UNUSED(bufferSize);
    return false;
#endif
}

// ── TX fixed-gain output level (issue #167 Phase 1 Agent 1C) ────────────────
//
// Set the TXA fixed-gain scalar applied uniformly to the I and Q audio paths.
// Wraps the cmaster/ChannelMaster SetTXFixedGain entry point used by Thetis
// at cmaster.cs:1115-1119 [v2.10.3.13] CMSetTXOutputLevel:
//
//   public static void CMSetTXOutputLevel()
//   {
//       double level = Audio.RadioVolume * Audio.HighSWRScale;
//       cmaster.SetTXFixedGain(0, level, level);
//   }
//
// Composition of `Audio.RadioVolume * Audio.HighSWRScale` is the caller's
// concern — RadioModel call-site composition lands in issue #167 Phase 4A.
// This wrapper takes the already-composed `level` and pushes it to WDSP/
// ChannelMaster with channel == m_channelId and Igain == Qgain == level.
//
// Idempotent guard: NaN-aware, matching the setMicPreamp / D.3 / D.6 pattern.
// m_lastFixedGain initialises to quiet_NaN so the first call (any value,
// including 0.0) always passes.  No qCWarning / qCInfo logging — the WDSP
// setter is hot-path during TX (every CMSetTXOutputLevel callsite at
// cmaster.cs is per-MOX-cycle or per-Audio.RadioVolume change), so log noise
// is undesirable.
//
// Forward-declare the WDSP entry point.  In Thetis the symbol is exported
// from ChannelMaster.dll (txgain.c:127 [v2.10.3.13] — PORT-tagged) and
// imported by C# via P/Invoke at cmaster.cs:273-274 [v2.10.3.13]:
//   [DllImport("ChannelMaster.dll", EntryPoint = "SetTXFixedGain", ...)]
//   public static extern void SetTXFixedGain(int id, double Igain, double Qgain);
// NereusSDR's bundled wdsp library provides the same symbol via a NereusSDR-
// original glue stub at third_party/wdsp/src/txgain_stub.c (issue #167
// Phase 1) — the real per-channel pcm storage that Thetis's
// ChannelMaster owns is deferred to a future ChannelMaster-port phase.

#ifdef HAVE_WDSP
extern "C" {
    void SetTXFixedGain(int channel, double Igain, double Qgain);
}
#endif

void TxChannel::setTxFixedGain(double level)
{
    // NaN-aware idempotent guard (matching D.3 pattern for double setters).
    // The plain `level == m_lastFixedGain` expression handles non-NaN sentinels
    // correctly; the explicit isnan check makes the first-call dispatch
    // intent obvious to readers.
    if (!std::isnan(m_lastFixedGain) && level == m_lastFixedGain) return;
    m_lastFixedGain = level;
#ifdef HAVE_WDSP
    // From Thetis cmaster.cs:1115-1119 [v2.10.3.13] CMSetTXOutputLevel —
    // cmaster.SetTXFixedGain(0, level, level).  cmaster.SetTXFixedGain is
    // the C# P/Invoke at cmaster.cs:273-274 [v2.10.3.13]; the native impl
    // is Thetis ChannelMaster/txgain.c:127-134 [v2.10.3.13] —
    //   void SetTXFixedGain(int txid, double Igain, double Qgain)
    //   {
    //       TXGAIN a = pcm->xmtr[txid].pgain;
    //       EnterCriticalSection (&a->cs_update0);
    //       a->Igain = Igain;
    //       a->Qgain = Qgain;
    //       LeaveCriticalSection (&a->cs_update0);
    //   }
    SetTXFixedGain(m_channelId, level, level);
#endif
}

} // namespace NereusSDR
