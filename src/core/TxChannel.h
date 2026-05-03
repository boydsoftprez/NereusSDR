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
// src/core/TxChannel.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/wdsp/TXA.c    — TXA pipeline construction (create_txa),
//                                         licence above (Warren Pratt, NR0V)
//   Project Files/Source/ChannelMaster/cmaster.c — channel lifecycle,
//                                         licence above (Warren Pratt, NR0V)
//
// Ported from Thetis wdsp/TXA.c:31-479 [v2.10.3.13]
// Ported from Thetis wdsp/cmaster.c:177-190 [v2.10.3.13]
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-25 — Stub created by J.J. Boyd (KG4VCF) during 3M-1a Task C.1.
//   2026-04-25 — Full class body (31-stage TXA pipeline wrapper, stageRunning
//                 introspection) added by J.J. Boyd (KG4VCF) during 3M-1a
//                 Task C.2, with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-04-26 — kMaxToneMag constant + setTuneTone() declaration added
//                 by J.J. Boyd (KG4VCF) during 3M-1a Task C.3.
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-04-26 — setRunning(bool) / isRunning() / setStageRunning(Stage, bool)
//                 added by J.J. Boyd (KG4VCF) during 3M-1a Task C.4 (channel
//                 state + 3M-1a active-stage activation). AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-04-26 — setConnection() / setMicRouter() / driveOneTxBlock() /
//                 m_txProductionTimer added by J.J. Boyd (KG4VCF) during
//                 3M-1a Task G.1 (TX I/Q production loop — bench fix:
//                 fexchange2 output now reaches RadioConnection::sendTxIq).
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-04-27 — setTxMode(DSPMode) / setTxBandpass(int, int) /
//                 setSubAmMode(int) added by J.J. Boyd (KG4VCF) during
//                 3M-1b Task D.2 (per-mode TXA config setters). AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-04-27 — setStageRunning() expanded with explicit cases for
//                 MicMeter, AlcMeter, AmMod, FmMod (+ Panel verified) by
//                 J.J. Boyd (KG4VCF) during 3M-1b Task D.4. All 4 new
//                 stages are documented no-ops: MicMeter/AlcMeter have no
//                 public WDSP Run setter (meter.c:36-57 [v2.10.3.13]);
//                 AmMod/FmMod run-controlled only via SetTXAMode()
//                 (TXA.c:753-789 [v2.10.3.13]). AI-assisted transformation
//                 via Anthropic Claude Code.
//   2026-04-27 — sip1OutputReady(const float*, int) signal added by
//                 J.J. Boyd (KG4VCF) during 3M-1b Task D.5. Emitted inside
//                 driveOneTxBlock() after fexchange2 + sendTxIq; carries
//                 m_outI.data() + m_outputBufferSize for the MON siphon
//                 path (AudioEngine::txMonitorBlockReady in Phase L).
//                 DirectConnection-only contract documented in signal
//                 doc-comment. AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-04-27 — setMicPreamp(double) / recomputeTxAPanelGain1() added by
//                 J.J. Boyd (KG4VCF) during 3M-1b Task D.6 (mic-mute path).
//                 NaN-aware idempotent guard. When TransmitModel::micPreampChanged
//                 fires 0.0 (MicMute toggled off / Thetis mute=true path),
//                 SetTXAPanelGain1 is called with 0, silencing the mic in WDSP.
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-04-27 — getTxMicMeter() / getAlcMeter() (2 wired) + getEqMeter() /
//                 getLvlrMeter() / getCfcMeter() / getCompMeter() (4 deferred)
//                 + kMeterUninitialisedSentinel constant added by J.J. Boyd
//                 (KG4VCF) during 3M-1b Task D.7 (TX meter readouts).
//                 Active meters read GetTXAMeter(TXA_MIC_PK / TXA_ALC_PK) from
//                 Thetis wdsp/TXA.h:51-64 [v2.10.3.13]; deferred meters return
//                 0.0f unconditionally per master design §5.2.1 (3M-3a scope).
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-04-28 — Phase 3M-1c E.1 — push-driven TX pump.  driveOneTxBlock()
//                 converted from QTimer-driven pull to a slot accepting
//                 (const float* samples, int frames).  The QTimer (and the
//                 1b353f4 partial-read zero-fill workaround) were dropped:
//                 AudioEngine::micBlockReady (Phase D.1) now drives fexchange2
//                 directly via Qt::DirectConnection.  TxMicRouter retained
//                 for the future Radio-mic source path; the PC-mic path no
//                 longer pulls from it.  tickForTest seam updated to
//                 (samples, frames).  J.J. Boyd (KG4VCF), AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-04-29 — Phase 3M-1c TX pump architecture redesign by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.  Dropped the bench-fix-B silence-drive timer
//                 (m_silenceTimer / m_lastDriveTimer / kSilenceTimerIntervalMs
//                 / kSilenceStaleThresholdMs / onSilenceTimer slot) — the new
//                 TxWorkerThread pump pulls every 5 ms unconditionally and
//                 zero-fills the gap when AudioEngine::pullTxMic returns
//                 partial / no data, so the silence path "falls out for free"
//                 (PostGen TUNE-tone still produces output; SSB with no mic
//                 produces silent — both correct).
//                 Public state-mutation setters remain in `public:`. The L.2
//                 fixup connects + 3M-1c spec-compliance MoxController→TxChannel
//                 connects use Qt5 functor syntax (`connect(emit, sig,
//                 m_txChannel, lambda_or_methodptr)`) which dispatches via
//                 AutoConnection — auto-routes to QueuedConnection when the
//                 receiver lives on a different thread, no slot annotation
//                 required.  Only `driveOneTxBlock` is in `public slots:`
//                 because TxWorkerThread invokes it directly via member-pointer
//                 call (same-thread dispatch on the worker, not via Qt's
//                 connect machinery).
//                 Plan: docs/architecture/phase3m-1c-tx-pump-architecture-plan.md
//   2026-04-28 — Phase 3M-1c E.2-E.6 — TXA PostGen wrapper setters (12 methods)
//                 added by J.J. Boyd (KG4VCF):
//                   E.2: setTxPostGenMode(int)
//                   E.3: setTxPostGenTT{Freq1,Freq2,Mag1,Mag2}(double)
//                   E.4: setTxPostGenTTPulseToneFreq{1,2}(double),
//                        setTxPostGenTTPulseMag{1,2}(double)
//                   E.5: setTxPostGenTTPulse{Freq(int),DutyCycle(double),
//                        Transition(double)}
//                   E.6: setTxPostGenRun(bool)
//                 Thin pass-through wrappers that drive the WDSP two-tone
//                 IMD test stage (gen1 PostGen).  Phase I will wire these
//                 into a SetupForm.cs-style handler; Phase L will wire the
//                 signal/slot connections.  Internal cache fields for the
//                 split-property freq1/freq2 / mag1/mag2 partners match the
//                 Thetis radio.cs:3697-4032 [v2.10.3.13] pattern (each
//                 individual setter calls the combined WDSP function with
//                 both values).  AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-04-29 — Stage-2 review fix I1 — refreshed v2-era doc comments
//                 (fexchange2 / 256-block / 5 ms cadence / kPumpIntervalMs /
//                 onPumpTick / QTimer) on driveOneTxBlock + class
//                 thread-safety block to reflect v3 (fexchange0 / 64-block /
//                 semaphore-wake / TxMicSource-driven cadence).  No
//                 behavioural change; documentation refresh only.  J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-04-30 — Phase 3M-3a-ii Batch 1 — 9 TX dynamics-section wrappers
//                 added by J.J. Boyd (KG4VCF):
//                   CFC (6):   setTxCfcRunning(bool)
//                              setTxCfcPosition(int)
//                              setTxCfcProfile(F, G, E, Qg, Qe)
//                              setTxCfcPrecompDb(double)
//                              setTxCfcPostEqRunning(bool)
//                              setTxCfcPrePeqDb(double)
//                   CPDR (2):  setTxCpdrOn(bool)
//                              setTxCpdrGainDb(double)
//                   CESSB (1): setTxCessbOn(bool)
//                 Thin pass-through wrappers over wdsp/cfcomp.c, compress.c
//                 and osctrl.c [v2.10.3.13] on top of the WDSP boot defaults
//                 shipped in 3M-1c.  setTxCfcProfile accepts the Thetis
//                 7-arg surface (F/G/E/Qg/Qe) but the bundled WDSP only
//                 ports the 5-arg variant — Qg/Qe are validated and
//                 dropped today, ready for live forwarding when WDSP is
//                 upgraded to v2.10.3.13.  Stage::CfComp / Stage::Compressor
//                 / Stage::OsCtrl already had explicit case arms in
//                 setStageRunning since 3M-1a Task C.4 — verified to mirror
//                 the 3M-3a-i style (direct WDSP call, version-stamped
//                 cite).  AI-assisted transformation via Anthropic Claude
//                 Code.
//   2026-04-30 — Phase 3M-3a-ii Batch 1.5 — bundled cfcomp.{c,h} upgraded
//                 from TAPR v1.29 to Thetis v2.10.3.13 (partial WDSP
//                 upstream-sync).  setTxCfcProfile now forwards Qg/Qe
//                 (or nullptr for empty vectors per cfcomp.c:669-682
//                 NULL-skirt semantics) instead of dropping them at the
//                 linker.  J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-04-30 — Phase 3M-3a-ii Batch 1.6 — 3 TX phase-rotator parameter
//                 wrappers added by J.J. Boyd (KG4VCF):
//                   setTxPhrotCornerHz(double)  → SetTXAPHROTCorner
//                   setTxPhrotNstages(int)      → SetTXAPHROTNstages
//                   setTxPhrotReverse(bool)     → SetTXAPHROTReverse
//                 Thin pass-through wrappers over wdsp/iir.c:675-703
//                 [v2.10.3.13] on top of the WDSP boot defaults shipped
//                 in 3M-1c.  3M-3a-i shipped Stage::PhRot + the
//                 setStageRunning(Stage::PhRot,...) arm calling
//                 SetTXAPHROTRun; the parameter setters were deferred to
//                 this batch because their persistence keys
//                 (CFCPhaseRotatorFreq / CFCPhaseRotatorStages /
//                 CFCPhaseReverseEnabled) live in the Thetis tpDSPCFC tab
//                 alongside the CFC controls.  AI-assisted transformation
//                 via Anthropic Claude Code.
//   2026-04-30 — Phase 3M-3a-ii follow-up Batch 7 — getCfcDisplayCompression
//                 main-thread readback added by J.J. Boyd (KG4VCF) to feed
//                 the parametric-EQ widget bar chart at 50 ms cadence.  Wraps
//                 WDSP::GetTXACFCOMPDisplayCompression (cfcomp.c:740-757
//                 [v2.10.3.13]); pure pass-through with null + size validation.
//                 Two new public constants pin the WDSP geometry
//                 (kCfcDisplayBinCount = 1025 = fsize/2+1; kCfcDisplaySampleRateHz
//                 = 48000.0 = TX dsp_rate / 2) so consumers don't reinvent
//                 them.  AI-assisted transformation via Anthropic Claude
//                 Code.
//   2026-05-02 — Plan 4 D8: requestFilterChange / applyPendingFilter /
//                 applyTxFilterForMode (debounced per-profile TX filter wiring)
//                 + txFilterApplied signal added by J.J. Boyd (KG4VCF).
//                 50 ms QTimer debounce coalesces rapid spinbox-arrow calls.
//                 applyTxFilterForMode maps audio Hz → IQ-space sign convention
//                 per deskhpsdr/transmitter.c:2136-2186 [@120188f] (same cite
//                 as the TUN bandpass at setTuneTone() lines 505-528 — the TUN
//                 path is unaffected; this helper runs only from the debounce
//                 timer, not from setTuneTone).  AI-assisted transformation
//                 via Anthropic Claude Code.
//   2026-05-03 — Issue #167 Phase 1 Agent 1C — setTxFixedGain(double) wrapper
//                 added by J.J. Boyd (KG4VCF).  Mirrors Thetis
//                 cmaster.cs:1115-1119 CMSetTXOutputLevel [v2.10.3.13]:
//                 the wrapper takes the already-composed `level` argument
//                 (Audio.RadioVolume * Audio.HighSWRScale at the Thetis
//                 call-site) and pushes it to cmaster SetTXFixedGain via
//                 m_channelId with Igain == Qgain == level.  NaN-aware
//                 idempotent guard via m_lastFixedGain matches the
//                 setMicPreamp / D.3 / D.6 pattern.  RadioModel call-site
//                 composition (audio_volume * swrProtect) is deferred to
//                 issue #167 Phase 4A.  AI-assisted transformation via
//                 Anthropic Claude Code.
// =================================================================

#pragma once

#include <QObject>
#include <QTimer>

#include <array>    // std::array — TX EQ 10-band graphic vector (3M-3a-i B-1)
#include <atomic>   // std::atomic<bool> — m_running cross-thread mirror (3M-1c TxWorkerThread)
#include <limits>   // std::numeric_limits — quiet_NaN() initialiser (D.3)
#include <vector>

#include "WdspTypes.h"

namespace NereusSDR {

class RadioConnection;
class TxMicRouter;

// Per-transmitter WDSP channel wrapper.
//
// Wraps the TXA DSP pipeline constructed by WDSP's create_txa() when
// OpenChannel(... type=1 ...) is called in WdspEngine::createTxChannel().
// The 31 pipeline stages are already in WDSP-managed memory when this
// constructor runs; this class provides a typed C++ facade over them.
//
// Thread safety (Phase 3M-1c TX pump architecture redesign v3):
//   The TxChannel object is moved to TxWorkerThread by RadioModel after
//   construction + initial wiring.  The worker thread runs a
//   semaphore-wake loop (`TxWorkerThread::run` — mirrors Thetis
//   cm_main at cmbuffs.c:151-168 [v2.10.3.13]) sourced from radio mic
//   frames via TxMicSource.  Cadence is the radio's natural mic-frame
//   stream — at 48 kHz mic rate with 64-frame blocks, that's ~1.33 ms
//   per block on both P1 EP6 (with HL2 pipelined frames at 126 mic
//   samples/frame, two frames per packet) and P2 port-1026 (mic-only
//   datagrams at 64 samples per packet).  Each tick invokes
//   driveOneTxBlockFromInterleaved → fexchange0 + sendTxIq.
//
//   The previous v2 design (QTimer-driven 5 ms polling, fexchange2,
//   256-block kPumpIntervalMs) was scrapped on 2026-04-29 in favour of
//   the semaphore-wake/64-block design that matches Thetis's
//   getbuffsize(48000) buffer plumbing exactly.
//
//   - Construct + destroy: main thread.  Destruction MUST happen on the
//     thread the TxChannel currently lives on; RadioModel teardown moves
//     it back to the main thread before destroying.
//   - Public state-mutation setters (setMicPreamp, setVoxRun, setTuneTone,
//     setTxPostGen*, setTxMode, setTxBandpass, etc.):
//     called from the main thread.  WDSP's per-channel csDSP critical
//     section serializes setter↔setter access (multiple writers are
//     mutually-excluded inside WDSP).  Setter↔DSP-read is unprotected —
//     Thetis itself relies on x86-style atomic-double semantics here
//     (e.g., `xgen` in wdsp/gen.c:215 [v2.10.3.13] reads `tt.f1`/`tt.f2`/
//     `tt.mag` without taking csDSP).  NereusSDR inherits the same race
//     surface and acceptance criterion.  The C++ idempotent-guard fields
//     (m_micPreampLast, m_voxRunLast, etc.) are accessed only from within
//     their own setters, which are all main-thread-only.  No concurrent
//     setter calls in the codebase today.  See plan §4.4 for the full
//     audit.
//   - Lifecycle setters (setConnection, setMicRouter, setRunning):
//     setConnection / setMicRouter are called from the main thread BEFORE
//     moveToThread (initial wiring) and AFTER the worker has been stopped
//     (teardown).  setRunning is invoked via MoxController connect()
//     lambdas with receiver=m_txChannel — AutoConnection routes the
//     lambda body to the worker thread, where the m_txChannel->setRunning()
//     call becomes same-thread.  m_running is std::atomic for safe
//     isRunning() reads from any thread (e.g., main-thread UI queries
//     while the channel lives on TxWorkerThread, test code, future
//     cross-thread instrumentation).
//   - driveOneTxBlockFromInterleaved: called from TxWorkerThread (the
//     worker) once per drained mic block.  Runs fexchange0 → sendTxIq
//     synchronously on that thread.  driveOneTxBlock(const float*, int)
//     is retained as a legacy / test-only seam (see public-slots
//     section below).
//   - stageRunning / isRunning / get*Meter: read-only introspection,
//     safe from any thread (atomics + WDSP-internal locking).
//
// Ported from Thetis wdsp/TXA.c:31-479 [v2.10.3.13] — create_txa() signal
// flow order determines the Stage enum ordinal values.
class TxChannel : public QObject {
    Q_OBJECT

public:
    // ── Stage identity ──────────────────────────────────────────────────────
    //
    // Ordinal values MUST match the signal-flow order from create_txa() in
    // wdsp/TXA.c:31-479 [v2.10.3.13].  Do not reorder.
    //
    // Default Run values per create_txa() first-argument ("run" parameter):
    //   ON  (1): Panel, MicMeter, EqMeter, LvlrMeter, CfcMeter, Bp0, CompMeter,
    //            Alc, AlcMeter, Sip1, Calcc, OutMeter
    //   OFF (0): RsmpIn, Gen0, PhRot, AmSq, Eqp, PreEmph, Leveler, CfComp,
    //            Compressor, Bp1, OsCtrl, Bp2, AmMod, FmMod, Gen1, UsLew*,
    //            Iqc, Cfir, RsmpOut
    //
    // * UsLew: create_uslew() takes no explicit "run" parameter — the uslew
    //   ramp engages via the channel upslew flag (ch[].iob.ch_upslew), not a
    //   stage-level run bit.  stageRunning(UsLew) always returns false when
    //   WDSP is live; the test documents this as EXPECTED.
    //
    // NOTE: The pre-code review §8.1 table listed "25 stages (indices 0-25)".
    // The authoritative source — wdsp/TXA.c create_txa() — constructs 31
    // distinct stages (verified by counting create_* calls at lines 40-475).
    // Five stages were omitted from the pre-code table: PreEmph, Leveler,
    // LvlrMeter, CfComp, CfcMeter (all appear between EqMeter and Bp0 in
    // the signal chain).  This is a pre-code review correction; use the 31-
    // stage list here as authoritative.
    enum class Stage : int {
        // From Thetis wdsp/TXA.c:40-49   [v2.10.3.13] — rsmpin (input resampler)
        RsmpIn     =  0,
        // From Thetis wdsp/TXA.c:51-57   [v2.10.3.13] — gen0 (PreGen, mode=2 noise)
        Gen0       =  1,
        // From Thetis wdsp/TXA.c:59-69   [v2.10.3.13] — panel (audio panel/level)
        Panel      =  2,
        // From Thetis wdsp/TXA.c:71-78   [v2.10.3.13] — phrot (phase rotator)
        PhRot      =  3,
        // From Thetis wdsp/TXA.c:80-93   [v2.10.3.13] — micmeter
        MicMeter   =  4,
        // From Thetis wdsp/TXA.c:95-109  [v2.10.3.13] — amsq (AM squelch)
        AmSq       =  5,
        // From Thetis wdsp/TXA.c:115-128 [v2.10.3.13] — eqp (parametric EQ)
        Eqp        =  6,
        // From Thetis wdsp/TXA.c:130-143 [v2.10.3.13] — eqmeter
        EqMeter    =  7,
        // From Thetis wdsp/TXA.c:145-156 [v2.10.3.13] — preemph (pre-emphasis)
        PreEmph    =  8,
        // From Thetis wdsp/TXA.c:158-181 [v2.10.3.13] — leveler (wcpagc, mode=5)
        Leveler    =  9,
        // From Thetis wdsp/TXA.c:183-196 [v2.10.3.13] — lvlrmeter
        LvlrMeter  = 10,
        // From Thetis wdsp/TXA.c:198-222 [v2.10.3.13] — cfcomp (multiband compander)
        CfComp     = 11,
        // From Thetis wdsp/TXA.c:224-237 [v2.10.3.13] — cfcmeter
        CfcMeter   = 12,
        // From Thetis wdsp/TXA.c:239-251 [v2.10.3.13] — bp0 (mandatory BPF)
        Bp0        = 13,
        // From Thetis wdsp/TXA.c:253-258 [v2.10.3.13] — compressor
        Compressor = 14,
        // From Thetis wdsp/TXA.c:260-272 [v2.10.3.13] — bp1 (post-comp BPF)
        Bp1        = 15,
        // From Thetis wdsp/TXA.c:274-280 [v2.10.3.13] — osctrl (output soft clip)
        OsCtrl     = 16,
        // From Thetis wdsp/TXA.c:282-294 [v2.10.3.13] — bp2 (post-clip BPF)
        Bp2        = 17,
        // From Thetis wdsp/TXA.c:296-309 [v2.10.3.13] — compmeter
        CompMeter  = 18,
        // From Thetis wdsp/TXA.c:311-334 [v2.10.3.13] — alc (ALC, always-on wcpagc)
        Alc        = 19,
        // From Thetis wdsp/TXA.c:336-342 [v2.10.3.13] — ammod (AM modulation)
        AmMod      = 20,
        // From Thetis wdsp/TXA.c:345-359 [v2.10.3.13] — fmmod (FM modulation)
        FmMod      = 21,
        // From Thetis wdsp/TXA.c:361-367 [v2.10.3.13] — gen1 (PostGen, mode=0 tone)
        Gen1       = 22,
        // From Thetis wdsp/TXA.c:369-377 [v2.10.3.13] — uslew (5 ms upslew ramp)
        UsLew      = 23,
        // From Thetis wdsp/TXA.c:379-392 [v2.10.3.13] — alcmeter
        AlcMeter   = 24,
        // From Thetis wdsp/TXA.c:394-403 [v2.10.3.13] — sip1 (siphon / spectrum)
        Sip1       = 25,
        // From Thetis wdsp/TXA.c:405-422 [v2.10.3.13] — calcc (PureSignal calibration)
        Calcc      = 26,
        // From Thetis wdsp/TXA.c:424-432 [v2.10.3.13] — iqc (IQ correction)
        Iqc        = 27,
        // From Thetis wdsp/TXA.c:434-449 [v2.10.3.13] — cfir (custom CIC FIR)
        Cfir       = 28,
        // From Thetis wdsp/TXA.c:451-460 [v2.10.3.13] — rsmpout (output resampler)
        RsmpOut    = 29,
        // From Thetis wdsp/TXA.c:462-475 [v2.10.3.13] — outmeter
        OutMeter   = 30,

        // Sentinel — equals the number of TXA stages in wdsp/TXA.c [v2.10.3.13].
        // Verified by counting create_* calls in create_txa(): 31 stages total.
        kStageCount = 31
    };

    // ── TUNE-tone magnitude constant ─────────────────────────────────────────
    //
    // From Thetis console.cs:29954 [v2.10.3.13]:
    //   private const double MAX_TONE_MAG = 0.99999f; // why not 1?  clipping?
    //
    // NereusSDR mirrors the Thetis declaration byte-exactly: `0.99999f` widens
    // to `double` on assignment, producing the same bit pattern Thetis stores
    // at runtime (~0.99998999641968).  The C# `f` suffix forces float precision
    // first then widens to double; using a bare double literal `0.99999` would
    // store `0.99999000…`, differing from Thetis by ~1.4e-8.  Using `0.99999f`
    // here reproduces the identical widening and keeps the values byte-exact.
    //
    // NOTE: If 3M-3a adds 2-TONE support and needs this value, move it to a
    // shared WdspTuneConstants.h and include from both TxChannel.h and any
    // new 2-TONE source. Today the constant has one owner; do not duplicate.
    static constexpr double kMaxToneMag = 0.99999f;  // why not 1?  clipping?

    // inputBufferSize:  fexchange2 input block == OpenChannel in_size (default 256).
    // outputBufferSize: fexchange2 output block == in_size × out_rate / in_rate.
    //   At 48 kHz in / 48 kHz out (P1/HL2): 256 samples.
    //   At 48 kHz in / 192 kHz out (P2 Saturn): 256 × 4 = 1024 samples.
    //
    // Both sizes are passed from WdspEngine::createTxChannel() which holds the
    // authoritative rate values used in OpenChannel().
    //
    // Defaults (256/256) are correct for P1 (48 kHz in/out, 1:1 ratio) and
    // unit tests that don't call WdspEngine::initialize().  Real P2 callers
    // (WdspEngine::createTxChannel) pass explicit values: in_size=256 and
    // out_size=1024 (= 256 × 192 000 / 48 000).
    //
    // The 256 default (vs Thetis cmaster.c xcm_insize values that vary per
    // hardware) is required because WDSP iobuffs.c:577 wraps r2_outidx with
    // `==` rather than modulo, so out_size must divide r2_active_buffsize
    // exactly.  See WdspEngine::kTxDspBufferSize for the full derivation.
    //
    // From Thetis wdsp/cmaster.c:177-190 [v2.10.3.13] — OpenChannel in_size /
    //   pcm->xmtr[i].ch_outrate.
    explicit TxChannel(int channelId,
                       int inputBufferSize  = 256,
                       int outputBufferSize = 256,
                       QObject* parent = nullptr);
    ~TxChannel() override;

    int channelId() const noexcept { return m_channelId; }

    // ── Stage introspection (3M-1a C.2) ─────────────────────────────────────
    //
    // Returns whether the given TXA stage currently has its run flag set.
    //
    // Implementation reads the WDSP struct field directly:
    //   txa[m_channelId].stagename.p->run
    // via the extern struct _txa txa[] declared in wdsp/TXA.h.
    //
    // If txa[channelId].rsmpin.p is null (channel not yet opened via
    // OpenChannel — typical in unit-test builds that link WDSP but don't
    // call WdspEngine::initialize), falls through to compile-time defaults.
    //
    // Without HAVE_WDSP, always returns compile-time defaults per create_txa().
    //
    // Thread safety: call only from the main thread.
    //
    // From Thetis wdsp/TXA.c:31-479 [v2.10.3.13] — run values are the first
    // argument to each create_*() call in create_txa().
    bool stageRunning(Stage s) const;

    // ── TUNE-tone PostGen (3M-1a C.3) ───────────────────────────────────────
    //
    // Enable / disable the TUNE-tone PostGen (gen1) on this TX channel.
    //
    // Wires WDSP's `SetTXAPostGen*` API per the TUNE pattern from Thetis
    // `chkTUN_CheckedChanged` (console.cs:30031-30040 [v2.10.3.13]).
    //
    // Call order matches Thetis exactly (freq → mode → mag → run):
    //   - Frequency = signed Hz; caller is responsible for sign-flipping per
    //     the current DSP mode (LSB/CWL/DIGL → -cw_pitch; everything else
    //     → +cw_pitch). See pre-code review §3.5. Default 0.0 (caller passes
    //     signed cw_pitch; G.4 TUNE function port computes the sign).
    //   - Mode = 0 (sine tone — per `wdsp/gen.c:144-193 [v2.10.3.13]`)
    //   - Magnitude = kMaxToneMag (= 0.99999) by default; the `magnitude`
    //     parameter exists so future callers (e.g., 2-TONE test in 3M-3a)
    //     can override.
    //   - Run = 1 (on) or 0 (off)
    //
    // 3M-1a callers (Task G.4 TUNE function port + TxApplet) drive this from
    // `MoxController::txAboutToBegin` for TUN-on and `txAboutToEnd` for TUN-off.
    //
    // **Scope (3M-1a C.3):** this method only touches the gen1 PostGen API.
    // It does NOT engage MOX (that is MoxController), does NOT configure the
    // TXA channel's DSP mode (that is C.4 / G.4), and does NOT compute the
    // cw_pitch sign (that is G.4's responsibility).
    //
    // From Thetis console.cs:30031-30040 [v2.10.3.13] — chkTUN_CheckedChanged.
    // From Thetis wdsp/gen.c:783-813 [v2.10.3.13] — SetTXAPostGen* API.
    void setTuneTone(bool on,
                     double freqHz   = 0.0,
                     double magnitude = kMaxToneMag);

    // ── TX I/Q production loop (3M-1a G.1) ──────────────────────────────────
    //
    // Attach or detach the RadioConnection that receives TX I/Q output from
    // fexchange2.  Non-owning; the caller (RadioModel) owns the connection.
    // Pass nullptr to detach (e.g. on disconnect).  Safe to call from the
    // main thread at any time; driveOneTxBlock() guards against null.
    //
    // Must be called before setRunning(true) to get samples on the wire.
    void setConnection(RadioConnection* conn);

    // Attach or detach the mic router used as fexchange2 input source.
    // Non-owning; the caller (RadioModel) owns the unique_ptr.
    // In 3M-1a, NullMicSource provides zero-padded silence — functionally
    // inert because gen1 PostGen overwrites the input during TUNE.
    // Pass nullptr to detach.  Safe to call from the main thread.
    //
    // TODO [3M-1b]: Replace NullMicSource with PcMicSource / RadioMicSource
    //   per user preference and BoardCapabilities. See TxMicRouter.h §TODO.
    void setMicRouter(TxMicRouter* router);

    // ── Channel state (3M-1a C.4) ────────────────────────────────────────────
    //
    // Activate or deactivate the WDSP TXA channel.
    //
    // Calls `SetChannelState(channelId, on ? 1 : 0, on ? 0 : 1)`. The dmode
    // (drain) convention matches Thetis console.cs:29595/29607 [v2.10.3.13]:
    //   - Turn ON  (RX→TX): state=1, dmode=0  — immediate start, no flush
    //   - Turn OFF (TX→RX): state=0, dmode=1  — drain in-flight samples before stop
    //
    // On `true`, also activates the one stage that defaults OFF in create_txa()
    // but is required for the 3M-1a TUNE-carrier flow under Protocol 2:
    //   - cfir (stage 28, custom CIC FIR filter): SetTXACFIRRun(ch, 1)
    //     * From Thetis cmaster.cs:522-527 [v2.10.3.13]: enabled for P2, disabled
    //       for P1 (USB protocol). NereusSDR activates it unconditionally here;
    //       3M-1b will gate it on the active protocol when P1/P2 divergence matters.
    //
    // Stages NOT needing explicit activation:
    //   - rsmpin (stage 0): run flag managed internally by WDSP's TXAResCheck()
    //     (wdsp/TXA.c:809-817 [v2.10.3.13]); set to 1 iff in_rate != dsp_rate.
    //     No public SetTXA*Run API exists for rsmpin/rsmpout — WDSP controls them.
    //   - rsmpout (stage 29): same — TXAResCheck() manages the run flag.
    //   - bp0 / alc / sip1 / alcmeter / outmeter: all default ON (run=1) in create_txa().
    //   - gen1: activated separately by setTuneTone(true).
    //   - uslew: always-on inside the xuslew state machine (no run flag).
    //
    // On `false`, deactivates cfir so it does not process during RX.
    //
    // Scope (3M-1a C.4): this method manages the minimum 3M-1a signal path.
    // 3M-1b will activate panel + micmeter for full SSB mic path; 3M-3a
    // activates speech-processing chain; 3M-4 activates calcc/iqc for PureSignal.
    //
    // From Thetis console.cs:29595 [v2.10.3.13] — TX-on callsite.
    // From Thetis console.cs:29607 [v2.10.3.13] — TX-off callsite.
    //   space_mox_delay: default 0 // from PSDR MW0LGE  [console.cs:29603]
    // From Thetis cmaster.cs:522-527 [v2.10.3.13] — cfir P2 activation.
    // From Thetis wdsp/channel.c:259-294 [v2.10.3.13] — SetChannelState impl.
    // From Thetis wdsp/cfir.c:233-238 [v2.10.3.13] — SetTXACFIRRun impl.
    void setRunning(bool on);

    /// Returns whether the WDSP TXA channel state is currently ON.
    ///
    /// Reflects the value set by the last call to setRunning(). Initialises
    /// to false (channel created stopped).  Does NOT query WDSP directly —
    /// it mirrors the local m_running atomic, which is updated by setRunning
    /// on the worker thread (after Phase 3M-1c TxWorkerThread move) and read
    /// from any thread.
    bool isRunning() const noexcept { return m_running.load(std::memory_order_acquire); }

    // ── Per-mode TXA configuration (3M-1b D.2) ──────────────────────────────

    /// Set the TXA channel's DSP mode (LSB / USB / DIGL / DIGU / etc.).
    ///
    /// Thin wrapper over WDSP SetTXAMode(channelId, mode).  Mode-gating
    /// (rejecting AM / SAM / FM / DSB in 3M-1b) is BandPlanGuard's
    /// responsibility at the MOX-engage layer; setTxMode itself is mode-agnostic.
    ///
    /// For AM/SAM modes the caller must also invoke setSubAmMode() (deferred to
    /// 3M-3b) to select the sideband dispatch.  setTxMode alone will call
    /// SetTXAMode with the raw AM/SAM integer value, which is correct WDSP
    /// usage; the sub-mode dispatch is additive.
    ///
    /// 3M-1b SSB scope: callers should pass LSB / USB / DIGL / DIGU only.
    ///
    /// From Thetis radio.cs:2670-2696 [v2.10.3.13] — CurrentDSPMode setter
    /// (else-branch: WDSP.SetTXAMode(WDSP.id(thread, 0), value) for non-AM/SAM).
    void setTxMode(DSPMode mode);

    /// Set the TXA channel's bandpass filter cutoff frequencies.
    ///
    /// Thin wrapper over WDSP SetTXABandpassFreqs(channelId, lowHz, highHz).
    /// No pre-validation is applied — Thetis SetTXFilter passes the values
    /// through to WDSP without range-checking (radio.cs:2730-2780 [v2.10.3.13]).
    ///
    /// IQ-space conventions (USB/LSB/AM/FM vary):
    ///   USB / DIGU: low=+low_audio,  high=+high_audio  (e.g. +150, +2850)
    ///   LSB / DIGL: low=-high_audio, high=-low_audio   (e.g. -2850, -150)
    ///   AM / DSB:   low=-high_audio, high=+high_audio  (e.g. -2850, +2850)
    ///
    /// From Thetis radio.cs:2730-2780 [v2.10.3.13] — SetTXFilter /
    /// TXFilterLow / TXFilterHigh setters.
    void setTxBandpass(int lowHz, int highHz);

    /// Set the AM/SAM sub-mode dispatch (0=DSB, 1=AM_LSB, 2=AM_USB).
    ///
    /// **Deferred to 3M-3b.** Throws std::logic_error if called in 3M-1b
    /// because AM/SAM TX is not enabled in this phase.  The method exists so
    /// the API is stable for 3M-3b; 3M-1b development that accidentally
    /// reaches this code path surfaces immediately as a test failure.
    ///
    /// From Thetis radio.cs:2699-2728 [v2.10.3.13] — SubAMMode setter
    /// (sub_am_mode 0=double-sided AM, 1=AM_LSB, 2=AM_USB).
    [[noreturn]] void setSubAmMode(int sub);

    // ── VOX / anti-VOX WDSP wrappers (3M-1b D.3) ────────────────────────────

    /// VOX run gate — wires WDSP SetDEXPRunVox.
    ///
    /// VOX is mode-gated at the MoxController layer (Phase H.1) — this is
    /// just the WDSP-side wrapper.  Idempotent: skips the WDSP call if the
    /// value is unchanged from the last call.
    ///
    /// From Thetis cmaster.cs:199-200 [v2.10.3.13] — SetDEXPRunVox DLL import.
    void setVoxRun(bool run);

    /// VOX attack threshold — wires WDSP SetDEXPAttackThreshold.
    ///
    /// Mic-boost-aware scaling (CMSetTXAVoxThresh in Thetis cmaster.cs:1057)
    /// is applied at the MoxController layer (Phase H.2); this method is a
    /// thin wrapper.  Idempotent: skips WDSP if value unchanged.
    ///
    /// From Thetis cmaster.cs:187-188 [v2.10.3.13] — SetDEXPAttackThreshold.
    void setVoxAttackThreshold(double thresh);

    /// VOX hang time in seconds — wires WDSP SetDEXPHoldTime.
    ///
    /// WDSP names this parameter "HoldTime" (wdsp/dexp.c:SetDEXPHoldTime);
    /// Thetis exposes it as VOXHangTime (console.cs:14706).  There is no
    /// SetDEXPHangTime in the WDSP source.  The mapping is:
    ///   NereusSDR setVoxHangTime(seconds) → WDSP SetDEXPHoldTime(id, seconds)
    /// Thetis passes milliseconds/1000.0 (setup.cs:18899):
    ///   cmaster.SetDEXPHoldTime(0, (double)udDEXPHold.Value / 1000.0)
    /// Callers are responsible for the ms→s conversion.
    ///
    /// Idempotent: skips WDSP if value unchanged.
    ///
    /// From Thetis cmaster.cs:178-179 [v2.10.3.13] — SetDEXPHoldTime DLL import.
    void setVoxHangTime(double seconds);

    /// Anti-VOX run gate — wires WDSP SetAntiVOXRun.
    ///
    /// Idempotent: skips WDSP if value unchanged.
    ///
    /// From Thetis cmaster.cs:208-209 [v2.10.3.13] — SetAntiVOXRun DLL import.
    void setAntiVoxRun(bool run);

    /// Anti-VOX gain — wires WDSP SetAntiVOXGain.
    ///
    /// Idempotent: skips WDSP if value unchanged.
    ///
    /// From Thetis cmaster.cs:211-212 [v2.10.3.13] — SetAntiVOXGain DLL import.
    void setAntiVoxGain(double gain);

    // ── Mic preamp / mic-mute path (3M-1b D.6) ──────────────────────────────

    /// Set the mic preamp linear scalar pushed to WDSP via SetTXAPanelGain1.
    ///
    /// Called by TransmitModel::micPreampChanged subscriber (wired in
    /// Phase L). When MicMute toggles off (chkMicMute.Checked == false in
    /// Thetis terms — note counter-intuitive naming: Checked == mic in use),
    /// TransmitModel sets micPreamp to 0.0, which lands here and silences
    /// the mic via SetTXAPanelGain1(channelId, 0).
    ///
    /// Idempotent: skips WDSP call if value unchanged. Uses NaN-aware
    /// guard matching D.3's pattern: m_micPreampLast initialises to
    /// quiet_NaN so the first call (any value) always passes.
    ///
    /// From Thetis console.cs:28805-28817 [v2.10.3.13] — setAudioMicGain():
    ///   Audio.MicPreamp = Math.Pow(10.0, gain_db / 20.0);  // mute=false (mic active)
    ///   Audio.MicPreamp = 0.0;                              // mute=true  (mic silent)
    /// Audio.MicPreamp setter calls CMSetTXAPanelGain1 → SetTXAPanelGain1.
    void setMicPreamp(double linearGain);

    /// Re-push the current m_micPreampLast value to WDSP.
    ///
    /// Called internally by setMicPreamp. Also exposed publicly for callers
    /// that need to force a refresh after channel state changes (e.g., after
    /// setRunning(true) re-initialises the channel).
    ///
    /// Idempotent only at the WDSP level — re-pushing the same value
    /// triggers a redundant WDSP call. Callers that care about idempotency
    /// should track state externally; this method always issues the WDSP
    /// call when invoked (subject to HAVE_WDSP + null-guard).
    void recomputeTxAPanelGain1();

    // ── TX meter readouts (3M-1b D.7) ───────────────────────────────────────

    /// Sentinel value returned by active meters when the WDSP channel is not
    /// initialised (txa[channelId].rsmpin.p == nullptr, or HAVE_WDSP not defined).
    ///
    /// UI code should treat this value as "channel not running" — distinct from
    /// "no signal" (which is typically -120 dB or -inf, not exactly -999).
    ///
    /// Deferred meters (getEqMeter / getLvlrMeter / getCfcMeter / getCompMeter)
    /// return 0.0f unconditionally; callers should treat 0.0f as "meter not yet
    /// active" rather than the -999 "channel not initialised" sentinel.
    static constexpr float kMeterUninitialisedSentinel = -999.0f;

    /// TX mic input peak meter — reads WDSP TXA_MIC_PK channel state.
    ///
    /// Returns the peak mic-input level in dB (typical range -100..0).
    /// Returns kMeterUninitialisedSentinel (-999.0f) if the WDSP channel is
    /// not initialised or HAVE_WDSP is not defined.
    ///
    /// Active during MOX in 3M-1b. Phase J.1 binds this to the TxApplet
    /// mic-meter widget.
    ///
    /// Porting from Thetis dsp.cs:390-391 [v2.10.3.13] — GetTXAMeter DLL import:
    ///   public static extern double GetTXAMeter(int channel, txaMeterType meter);
    /// Porting from Thetis wdsp/TXA.h:51 [v2.10.3.13]:
    ///   TXA_MIC_PK  = 0  (first value in txaMeterType enum)
    /// Porting from Thetis wdsp/meter.c:151-159 [v2.10.3.13] — GetTXAMeter impl.
    float getTxMicMeter() const;

    /// TX ALC peak meter — reads WDSP TXA_ALC_PK channel state.
    ///
    /// Returns the peak ALC level in dB (typical range -30..+10).
    /// Returns kMeterUninitialisedSentinel (-999.0f) if the WDSP channel is
    /// not initialised or HAVE_WDSP is not defined.
    ///
    /// Active during MOX in 3M-1b.
    ///
    /// Porting from Thetis dsp.cs:390-391 [v2.10.3.13] — GetTXAMeter DLL import.
    /// Porting from Thetis wdsp/TXA.h:63 [v2.10.3.13]:
    ///   TXA_ALC_PK  = 12  (12th value in txaMeterType enum, 0-indexed)
    /// Porting from Thetis wdsp/meter.c:151-159 [v2.10.3.13] — GetTXAMeter impl.
    float getAlcMeter() const;

    /// TX EQ meter — DEFERRED to 3M-3a. Returns 0.0f unconditionally in 3M-1b.
    ///
    /// Callers should treat 0.0f as "meter not yet active" (distinct from the
    /// active meters' kMeterUninitialisedSentinel = -999.0f "channel not running").
    ///
    /// Reads TXA_EQ_PK (= 2) when wired. From Thetis wdsp/TXA.h:53 [v2.10.3.13].
    /// Full wiring in 3M-3a per master design §5.2.1.
    float getEqMeter() const;

    /// TX Leveler meter — DEFERRED to 3M-3a. Returns 0.0f unconditionally.
    ///
    /// Reads TXA_LVLR_PK (= 4) when wired. From Thetis wdsp/TXA.h:55 [v2.10.3.13].
    /// Full wiring in 3M-3a per master design §5.2.1.
    float getLvlrMeter() const;

    /// TX CFC (continuous frequency compander) meter — DEFERRED to 3M-3a.
    /// Returns 0.0f unconditionally.
    ///
    /// Reads TXA_CFC_PK (= 7) when wired. From Thetis wdsp/TXA.h:58 [v2.10.3.13].
    /// Full wiring in 3M-3a per master design §5.2.1.
    float getCfcMeter() const;

    /// TX Compressor meter — DEFERRED to 3M-3a. Returns 0.0f unconditionally.
    ///
    /// Reads TXA_COMP_PK (= 10) when wired. From Thetis wdsp/TXA.h:61 [v2.10.3.13].
    /// Full wiring in 3M-3a per master design §5.2.1.
    float getCompMeter() const;

    // ── TXA PostGen wrapper setters (3M-1c E.2-E.6) ─────────────────────────
    //
    // Twelve thin C++ wrappers over the WDSP `SetTXAPostGen*` family that
    // drives the gen1 (TXA stage 22) two-tone / pulsed-IMD test source.
    // Phase I will wire these into a SetupForm.cs-style handler; Phase L
    // wires the signal/slot connections from TransmitModel.
    //
    // The C# Thetis property surface exposes Freq1/Freq2 / Mag1/Mag2 as
    // separate properties, but the underlying WDSP C API combines both into
    // single calls (`SetTXAPostGenTTFreq(ch, f1, f2)` /
    // `SetTXAPostGenTTMag(ch, m1, m2)` / `SetTXAPostGenTTPulseToneFreq(ch,
    // f1, f2)` / `SetTXAPostGenTTPulseMag(ch, m1, m2)`).  NereusSDR caches
    // the partner value internally so each individual setX1 / setX2 wrapper
    // can invoke the combined WDSP call — matching Thetis radio.cs:3697-
    // 4032 [v2.10.3.13]'s `tx_postgen_tt_freq1_dsp` / `_freq2_dsp` /
    // `_mag1_dsp` / `_mag2_dsp` cache fields.
    //
    // Pass-through semantics: no idempotency guard, no validation.  WDSP
    // pre-validates internally (gen.c:817-964 [v2.10.3.13]), and Phase I's
    // handler is responsible for choosing legal values.

    /// TXA PostGen mode select.
    ///
    /// Wraps SetTXAPostGenMode(channel, mode).
    /// Mode values: 0 = off, 1 = continuous two-tone, 7 = pulsed two-tone
    /// (other modes 2/3/4/5/6 — noise/sweep/sawtooth/triangle/pulse — exist
    /// in WDSP but are out of 3M-1c scope).
    ///
    /// From Thetis setup.cs:11084 / 11096 [v2.10.3.13]:
    ///   console.radio.GetDSPTX(0).TXPostGenMode = 7;  // pulsed
    ///   console.radio.GetDSPTX(0).TXPostGenMode = 1;  // continuous
    /// From Thetis wdsp/gen.c:792-797 [v2.10.3.13] — SetTXAPostGenMode impl.
    ///
    /// 'virtual' for the I.1 TwoToneController test seam — TestableTxChannel
    /// in tests/tst_two_tone_controller.cpp overrides to record the call
    /// sequence without needing a real WDSP context.  Production callers do
    /// not subclass.
    virtual void setTxPostGenMode(int mode);

    /// Continuous-mode two-tone freq 1 (Hz, lower tone).
    ///
    /// Wraps SetTXAPostGenTTFreq(channel, freq1, freq2_cached).
    /// The partner freq2 is cached from the most recent setTxPostGenTTFreq2
    /// call (default 0.0).  Matches Thetis radio.cs:3735-3751 [v2.10.3.13]
    /// TXPostGenTTFreq1 setter pattern.
    ///
    /// From Thetis setup.cs:11099 [v2.10.3.13]:
    ///   console.radio.GetDSPTX(0).TXPostGenTTFreq1 = ttfreq1;
    /// From Thetis wdsp/gen.c:826-833 [v2.10.3.13] — SetTXAPostGenTTFreq impl.
    /// 'virtual' for the I.1 TwoToneController test seam (see setTxPostGenMode).
    virtual void setTxPostGenTTFreq1(double hz);

    /// Continuous-mode two-tone freq 2 (Hz, upper tone).
    ///
    /// Wraps SetTXAPostGenTTFreq(channel, freq1_cached, freq2).
    /// Matches Thetis radio.cs:3755-3771 [v2.10.3.13] TXPostGenTTFreq2 setter.
    ///
    /// From Thetis setup.cs:11100 [v2.10.3.13]:
    ///   console.radio.GetDSPTX(0).TXPostGenTTFreq2 = ttfreq2;
    /// 'virtual' for the I.1 TwoToneController test seam.
    virtual void setTxPostGenTTFreq2(double hz);

    /// Continuous-mode two-tone mag 1 (linear amplitude, tone 1).
    ///
    /// Wraps SetTXAPostGenTTMag(channel, mag1, mag2_cached).
    /// Matches Thetis radio.cs:3697-3712 [v2.10.3.13] TXPostGenTTMag1 setter.
    ///
    /// From Thetis setup.cs:11102 [v2.10.3.13]:
    ///   console.radio.GetDSPTX(0).TXPostGenTTMag1 = ttmag1;
    /// From Thetis wdsp/gen.c:817-823 [v2.10.3.13] — SetTXAPostGenTTMag impl.
    /// 'virtual' for the I.1 TwoToneController test seam.
    virtual void setTxPostGenTTMag1(double linear);

    /// Continuous-mode two-tone mag 2 (linear amplitude, tone 2).
    ///
    /// Wraps SetTXAPostGenTTMag(channel, mag1_cached, mag2).
    /// Matches Thetis radio.cs:3716-3731 [v2.10.3.13] TXPostGenTTMag2 setter.
    ///
    /// From Thetis setup.cs:11103 [v2.10.3.13]:
    ///   console.radio.GetDSPTX(0).TXPostGenTTMag2 = ttmag2;
    /// 'virtual' for the I.1 TwoToneController test seam.
    virtual void setTxPostGenTTMag2(double linear);

    /// Pulsed-mode two-tone freq 1 (Hz, lower tone in pulsed window).
    ///
    /// Wraps SetTXAPostGenTTPulseToneFreq(channel, freq1, freq2_cached).
    /// Matches Thetis radio.cs:4000-4015 [v2.10.3.13] TXPostGenTTPulseToneFreq1.
    ///
    /// From Thetis setup.cs:11087 [v2.10.3.13]:
    ///   console.radio.GetDSPTX(0).TXPostGenTTPulseToneFreq1 = ttfreq1;
    /// From Thetis wdsp/gen.c:944-952 [v2.10.3.13] — SetTXAPostGenTTPulseToneFreq.
    /// 'virtual' for the I.1 TwoToneController test seam.
    virtual void setTxPostGenTTPulseToneFreq1(double hz);

    /// Pulsed-mode two-tone freq 2 (Hz, upper tone in pulsed window).
    ///
    /// Wraps SetTXAPostGenTTPulseToneFreq(channel, freq1_cached, freq2).
    /// Matches Thetis radio.cs:4018-4033 [v2.10.3.13] TXPostGenTTPulseToneFreq2.
    ///
    /// From Thetis setup.cs:11088 [v2.10.3.13]:
    ///   console.radio.GetDSPTX(0).TXPostGenTTPulseToneFreq2 = ttfreq2;
    /// 'virtual' for the I.1 TwoToneController test seam.
    virtual void setTxPostGenTTPulseToneFreq2(double hz);

    /// Pulsed-mode two-tone mag 1 (linear amplitude, tone 1).
    ///
    /// Wraps SetTXAPostGenTTPulseMag(channel, mag1, mag2_cached).
    /// Matches Thetis radio.cs:3964-3979 [v2.10.3.13] TXPostGenTTPulseMag1.
    ///
    /// From Thetis setup.cs:11090 [v2.10.3.13]:
    ///   console.radio.GetDSPTX(0).TXPostGenTTPulseMag1 = ttmag1;
    /// From Thetis wdsp/gen.c:915-923 [v2.10.3.13] — SetTXAPostGenTTPulseMag.
    /// 'virtual' for the I.1 TwoToneController test seam.
    virtual void setTxPostGenTTPulseMag1(double linear);

    /// Pulsed-mode two-tone mag 2 (linear amplitude, tone 2).
    ///
    /// Wraps SetTXAPostGenTTPulseMag(channel, mag1_cached, mag2).
    /// Matches Thetis radio.cs:3982-3997 [v2.10.3.13] TXPostGenTTPulseMag2.
    ///
    /// From Thetis setup.cs:11091 [v2.10.3.13]:
    ///   console.radio.GetDSPTX(0).TXPostGenTTPulseMag2 = ttmag2;
    /// 'virtual' for the I.1 TwoToneController test seam.
    virtual void setTxPostGenTTPulseMag2(double linear);

    /// Pulsed-mode window pulse rate (Hz).
    ///
    /// Wraps SetTXAPostGenTTPulseFreq(channel, hz).  Single-parameter
    /// (unlike PulseToneFreq which takes a pair).
    ///
    /// From Thetis setup.cs:34415 [v2.10.3.13] — setupTwoTonePulse:
    ///   console.radio.GetDSPTX(0).TXPostGenTTPulseFreq = (int)nudPulsed_TwoTone_window.Value;
    /// From Thetis wdsp/gen.c:926-933 [v2.10.3.13] — SetTXAPostGenTTPulseFreq.
    /// 'virtual' for the I.1 TwoToneController test seam.
    virtual void setTxPostGenTTPulseFreq(int hz);

    /// Pulsed-mode duty cycle (fraction 0..1; e.g. 0.10 = 10%).
    ///
    /// Wraps SetTXAPostGenTTPulseDutyCycle(channel, dc).
    ///
    /// From Thetis setup.cs:34416 [v2.10.3.13] — setupTwoTonePulse:
    ///   console.radio.GetDSPTX(0).TXPostGenTTPulseDutyCycle =
    ///       (float)(nudPulsed_TwoTone_percent.Value) / 100f;
    /// From Thetis wdsp/gen.c:935-942 [v2.10.3.13] — SetTXAPostGenTTPulseDutyCycle.
    /// 'virtual' for the I.1 TwoToneController test seam.
    virtual void setTxPostGenTTPulseDutyCycle(double pct);

    /// Pulsed-mode ramp transition time (seconds; e.g. 0.005 = 5 ms).
    ///
    /// Wraps SetTXAPostGenTTPulseTransition(channel, sec).
    ///
    /// From Thetis setup.cs:34417 [v2.10.3.13] — setupTwoTonePulse:
    ///   console.radio.GetDSPTX(0).TXPostGenTTPulseTransition =
    ///       (float)(nudPulsed_TwoTone_ramp.Value) / 1000f;
    /// From Thetis wdsp/gen.c:955-962 [v2.10.3.13] — SetTXAPostGenTTPulseTransition.
    /// 'virtual' for the I.1 TwoToneController test seam.
    virtual void setTxPostGenTTPulseTransition(double sec);

    /// Pulsed-mode I/Q-out enable flag (true = I and Q both, false = real-out
    /// only).  Required for the pulsed two-tone path to actually emit on the
    /// I/Q axes.
    ///
    /// Wraps SetTXAPostGenTTPulseIQout(channel, on ? 1 : 0).
    ///
    /// From Thetis setup.cs:34414 [v2.10.3.13] — setupTwoTonePulse:
    ///   console.radio.GetDSPTX(0).TXPostGenTTPulseIQOut = true;
    /// From Thetis wdsp/gen.c:963-969 [v2.10.3.13] — SetTXAPostGenTTPulseIQout.
    /// 'virtual' for the I.1 TwoToneController test seam.
    virtual void setTxPostGenTTPulseIQOut(bool on);

    /// TXA PostGen run gate (engages / disengages the gen1 stage).
    ///
    /// Wraps SetTXAPostGenRun(channel, on ? 1 : 0).
    ///
    /// From Thetis setup.cs:11107 / 11166 [v2.10.3.13]:
    ///   console.radio.GetDSPTX(0).TXPostGenRun = 1;  // on  (line 11107)
    ///   console.radio.GetDSPTX(0).TXPostGenRun = 0;  // off (line 11166)
    /// From Thetis wdsp/gen.c:784-789 [v2.10.3.13] — SetTXAPostGenRun impl.
    /// 'virtual' for the I.1 TwoToneController test seam.
    virtual void setTxPostGenRun(bool on);

    // ── TX EQ wrappers (3M-3a-i Task B-1) ───────────────────────────────────
    //
    // Seven thin wrappers over the WDSP `SetTXAEQ*` family that drives the
    // eqp (TXA stage 6) parametric / graphic equalizer.  Phase 3M-3a-i UI
    // batches will wire these into the Setup → DSP → EQ page; this batch
    // ships the DSP plumbing only.
    //
    // Threading: the Run wrapper (setTxEqRunning → SetTXAEQRun) is csDSP-
    // protected (eq.c:742-747 [v2.10.3.13]) and therefore audio-safe to
    // call from the main thread at any time.  All other wrappers
    // (Graph10 / Profile / Nc / Mp / Ctfmode / Wintype) reallocate the EQ
    // impulse-response and are NOT csDSP-protected; per Thetis precedent
    // (setup.cs handlers run on the form's UI thread) they are safe only
    // from the main thread.  The audio thread sees a momentary tear if
    // the call lands mid-block — Thetis lives with this; NereusSDR
    // mirrors the policy.
    //
    // 3M-3a-i Batch 1 ships the DSP wrappers only.  Phase 3M-3a-i Batch 2+
    // wires TransmitModel signals into these via RadioModel.

    /// TX EQ run gate.
    ///
    /// Wraps SetTXAEQRun(channel, on ? 1 : 0).  Equivalent to
    /// setStageRunning(Stage::Eqp, on) — provided as a convenience and for
    /// the 3M-3a-i UI batch which calls into a per-feature setter API.
    ///
    /// From Thetis wdsp/eq.c:742-747 [v2.10.3.13] — SetTXAEQRun impl
    ///   (csDSP-protected; audio-safe).
    /// From Thetis radio.cs TXEQRun setter (txa[].eqp.run gate via WDSP).
    void setTxEqRunning(bool on);

    /// TX EQ — 10-band graphic EQ shape.
    ///
    /// Wraps SetTXAGrphEQ10(channel, txeq[]).  preampPlus10Bands[0] is the
    /// preamp gain (dB); preampPlus10Bands[1..10] are the 10 band gains
    /// (dB) at the WDSP-fixed band centers
    /// 32 / 63 / 125 / 250 / 500 / 1k / 2k / 4k / 8k / 16k Hz.
    ///
    /// SetTXAGrphEQ10 reallocates the impulse response — main-thread only.
    ///
    /// From Thetis wdsp/eq.c:859-883 [v2.10.3.13] — SetTXAGrphEQ10 impl.
    /// From Thetis wdsp/TXA.c:112-113 [v2.10.3.13] — default_F / default_G
    ///   define the band centers and Thetis's factory G-shape.
    void setTxEqGraph10(const std::array<int, 11>& preampPlus10Bands);

    /// TX EQ — full custom-frequency profile path.
    ///
    /// Wraps SetTXAEQProfile(channel, 10, F[11], G[11]).  Used when
    /// the user moves the per-band frequency sliders so the bands no
    /// longer sit at the WDSP-fixed centers used by setTxEqGraph10.
    ///
    /// freqs10 must contain exactly 10 band-center frequencies (Hz).
    /// gains11 must contain exactly 11 entries (gains11[0] = preamp dB,
    /// gains11[1..10] = band gains dB).  Both vectors are validated;
    /// size mismatches log a qCWarning and the call early-returns.
    ///
    /// Internally builds F[11] = {0.0, freqs10[0..9]} (F[0] is the
    /// WDSP-padding slot — Thetis SetTXAEQProfile expects a 1-indexed
    /// vector with [0] unused) and G[11] = {gains11[0..10]}.  Q is NULL
    /// for graphic-EQ-style usage (Thetis never passes a Q vector through
    /// the 10-band UI).
    ///
    /// SetTXAEQProfile reallocates the impulse response — main-thread only.
    ///
    /// From Thetis wdsp/eq.c:779-804 [v2.10.3.13] — SetTXAEQProfile impl.
    /// From Thetis wdsp/TXA.c:112-127 [v2.10.3.13] — create_eqp call shape
    ///   showing F[0..nfreqs] / G[0..nfreqs] / Q=NULL convention.
    void setTxEqProfile(const std::vector<double>& freqs10,
                        const std::vector<double>& gains11);

    /// TX EQ — filter coefficient count.
    ///
    /// Wraps SetTXAEQNC(channel, nc).  Default 2048 per WDSP create_eqp
    /// call (TXA.c:118 [v2.10.3.13]: max(2048, ch[].dsp_size)).
    ///
    /// SetTXAEQNC reallocates the impulse response — main-thread only.
    ///
    /// From Thetis wdsp/eq.c:750-764 [v2.10.3.13] — SetTXAEQNC impl
    ///   (csDSP-protected, but allocates impulse — main-thread only).
    void setTxEqNc(int nc);

    /// TX EQ — minimum-phase flag.
    ///
    /// Wraps SetTXAEQMP(channel, mp ? 1 : 0).
    ///
    /// SetTXAEQMP reallocates the impulse response — main-thread only.
    ///
    /// From Thetis wdsp/eq.c:767-776 [v2.10.3.13] — SetTXAEQMP impl
    ///   (no csDSP — main-thread only).
    void setTxEqMp(bool mp);

    /// TX EQ — cutoff/transition mode.
    ///
    /// Wraps SetTXAEQCtfmode(channel, mode).
    ///
    /// SetTXAEQCtfmode reallocates the impulse response — main-thread only.
    ///
    /// From Thetis wdsp/eq.c:807-816 [v2.10.3.13] — SetTXAEQCtfmode impl
    ///   (no csDSP — main-thread only).
    void setTxEqCtfmode(int mode);

    /// TX EQ — window type.
    ///
    /// Wraps SetTXAEQWintype(channel, wintype).
    ///
    /// SetTXAEQWintype reallocates the impulse response — main-thread only.
    ///
    /// From Thetis wdsp/eq.c:819-828 [v2.10.3.13] — SetTXAEQWintype impl
    ///   (no csDSP — main-thread only).
    void setTxEqWintype(int wintype);

    // ── TX Leveler / ALC wrappers (3M-3a-i Task B-2) ────────────────────────
    //
    // Six thin wrappers over the WDSP `SetTXALeveler*` and `SetTXAALC*`
    // families.  Leveler is TXA stage 9 (slow speech-leveling AGC,
    // wcpAGC mode=5) and ALC is TXA stage 19 (final clip protection,
    // wcpAGC mode=5 — always-on per Thetis schema, no Run setter exposed
    // to the user).
    //
    // All six setters are csDSP-protected by WDSP (wcpAGC.c:570-650
    // [v2.10.3.13]) and therefore audio-safe to call from the main thread
    // at any time.  No shadow atomics needed.
    //
    // ALC Run is locked-on in Thetis (no chkALCEnabled checkbox in the
    // Setup UI; WdspEngine boot sets SetTXAALCSt(1) at WdspEngine.cpp:438).
    // We deliberately do NOT expose a setTxAlcOn wrapper.

    /// TX Leveler run gate.
    ///
    /// Wraps SetTXALevelerSt(channel, on ? 1 : 0).
    ///
    /// From Thetis wdsp/wcpAGC.c:613-618 [v2.10.3.13] — SetTXALevelerSt impl
    ///   (csDSP-protected; audio-safe).
    /// From Thetis setup.cs:9108-9123 [v2.10.3.13] — chkDSPLevelerEnabled_CheckedChanged
    ///   handler that wires through to TXLevelerOn → SetTXALevelerSt.
    void setTxLevelerOn(bool on);

    /// TX Leveler max-gain (Top) in dB.
    ///
    /// Wraps SetTXALevelerTop(channel, dB).  WDSP converts to linear
    /// internally via pow(10, dB/20.0); we pass dB straight per the Thetis
    /// radio.cs TXLevelerMaxGain setter pattern.
    ///
    /// Thetis Designer range: 0..20 dB (setup.Designer.cs:38718-38738
    ///   [v2.10.3.13] — udDSPLevelerThreshold).  Default 15 dB.
    ///
    /// From Thetis wdsp/wcpAGC.c:647-650 [v2.10.3.13] — SetTXALevelerTop impl
    ///   (csDSP-protected; audio-safe).
    /// From Thetis setup.cs:9095-9099 [v2.10.3.13] — udDSPLevelerThreshold_ValueChanged
    ///   handler routes through TXLevelerMaxGain → SetTXALevelerTop.
    void setTxLevelerTopDb(double dB);

    /// TX Leveler decay time-constant in milliseconds.
    ///
    /// Wraps SetTXALevelerDecay(channel, ms).  Thetis stores the value as
    /// ms / 1000.0 sec inside WDSP; the setter takes the raw int ms.
    ///
    /// Thetis Designer range: 1..5000 ms (setup.Designer.cs:38744-38772
    ///   [v2.10.3.13] — udDSPLevelerDecay).  Default 100 ms.
    ///
    /// From Thetis wdsp/wcpAGC.c:629-635 [v2.10.3.13] — SetTXALevelerDecay impl
    ///   (csDSP-protected; audio-safe).
    /// From Thetis setup.cs:9101-9105 [v2.10.3.13] — udDSPLevelerDecay_ValueChanged
    ///   handler routes through TXLevelerDecay → SetTXALevelerDecay.
    void setTxLevelerDecayMs(int ms);

    /// TX ALC max-gain in dB.
    ///
    /// Wraps SetTXAALCMaxGain(channel, dB).  WDSP converts to linear
    /// internally via pow(10, dB/20.0); we pass dB straight per the Thetis
    /// setup.cs handler pattern (setup.cs:9132).
    ///
    /// Thetis Designer range: 0..120 dB (setup.Designer.cs:38814-38833
    ///   [v2.10.3.13] — udDSPALCMaximumGain).  Default 3 dB
    ///   (database.cs:4592 [v2.10.3.13] — TXProfile ALC_MaximumGain).
    ///
    /// From Thetis wdsp/wcpAGC.c:603-610 [v2.10.3.13] — SetTXAALCMaxGain impl
    ///   (csDSP-protected; audio-safe).
    /// From Thetis setup.cs:9129-9134 [v2.10.3.13] — udDSPALCMaximumGain_ValueChanged
    ///   handler calls SetTXAALCMaxGain directly (no radio.cs property hop).
    void setTxAlcMaxGainDb(double dB);

    /// TX ALC decay time-constant in milliseconds.
    ///
    /// Wraps SetTXAALCDecay(channel, ms).
    ///
    /// Thetis Designer range: 1..50 ms (setup.Designer.cs:38845-38866
    ///   [v2.10.3.13] — udDSPALCDecay).  Default 10 ms.
    ///
    /// From Thetis wdsp/wcpAGC.c:585-592 [v2.10.3.13] — SetTXAALCDecay impl
    ///   (csDSP-protected; audio-safe).
    /// From Thetis setup.cs:9136-9140 [v2.10.3.13] — udDSPALCDecay_ValueChanged
    ///   handler routes through TXALCDecay → SetTXAALCDecay.
    void setTxAlcDecayMs(int ms);

    // ── B-3: TX CFC + CPDR + CESSB wrappers (Phase 3M-3a-ii Batch 1) ─────────
    //
    // Nine thin C++ wrappers over the TXA dynamics section:
    //   - CFC   (cfcomp.c:632-737)  Continuous Frequency Compander (stage 11)
    //   - CPDR  (compress.c:99-117) speech compressor (stage 14)
    //   - CESSB (osctrl.c:142-150)  controlled-envelope SSB (stage 16)
    //
    // All nine wrappers route to TX channel kTxChannelId = 1 and inherit the
    // standard rsmpin.p == nullptr null-guard pattern.  All are csDSP-protected
    // inside WDSP, so they are safe to call from the main thread while audio
    // thread runs.  All sit on top of the WDSP boot defaults shipped in 3M-1c
    // (no defaults invented here).

    /// CFC run gate.  Wraps SetTXACFCOMPRun(channel, on ? 1 : 0).
    ///
    /// From Thetis wdsp/cfcomp.c:632-641 [v2.10.3.13].
    void setTxCfcRunning(bool on);

    /// CFC pre/post-EQ position.  Wraps SetTXACFCOMPPosition(channel, pos).
    ///
    /// Thetis usage (radio.cs / setup.cs): 0 = pre-EQ, 1 = post-EQ.  The exact
    /// meaning is determined by where WDSP places cfcomp in the TXA chain at
    /// run time (TXA.c:198-222 [v2.10.3.13]).
    ///
    /// From Thetis wdsp/cfcomp.c:643-653 [v2.10.3.13].
    void setTxCfcPosition(int pos);

    /// CFC compression profile arrays.  Wraps SetTXACFCOMPprofile.
    ///
    /// F / G / E must all have the same length, `nfreqs`.  Qg and Qe are
    /// optional per-band Q parameters:
    ///   - Qg controls the Q (sharpness) of the gain skirt around each F[i].
    ///   - Qe controls the Q (sharpness) of the ceiling skirt around each F[i].
    /// Pass an empty vector for either Qg or Qe to opt out of per-band Q on
    /// that skirt — the wrapper forwards `nullptr` for the empty case, which
    /// matches the Thetis WDSP NULL-skirt semantics (cfcomp.c:669-682
    /// [v2.10.3.13]: when Qg/Qe is NULL, calc_comp falls back to linear
    /// interpolation between bands instead of the Gaussian-tail Q-shaped
    /// path).  Either Qg or Qe may be empty independently.
    ///
    /// The wrapper validates length consistency on F/G/E (and on any
    /// non-empty Qg/Qe vector) and emits a qCWarning + early-return on
    /// mismatch — no partial application.
    ///
    /// As of Phase 3M-3a-ii Batch 1.5 (2026-04-30) the bundled
    /// `third_party/wdsp/src/cfcomp.c` is the Thetis v2.10.3.13 version, so
    /// Qg/Qe forward straight through to WDSP — no linker-boundary drop.
    ///
    /// From Thetis wdsp/cfcomp.c:656 [v2.10.3.13] — 7-arg with per-band Qg
    /// (gain skirt Q) and Qe (ceiling skirt Q).  Either Qg or Qe may be
    /// NULL to opt out per-skirt.
    void setTxCfcProfile(const std::vector<double>& F,
                         const std::vector<double>& G,
                         const std::vector<double>& E,
                         const std::vector<double>& Qg,
                         const std::vector<double>& Qe);

    /// CFC pre-compression in dB.  Wraps SetTXACFCOMPPrecomp(channel, dB).
    /// WDSP stores pow(10, 0.05 * dB) as `precomplin` and re-multiplies the
    /// linear gain through cfc_gain[].
    ///
    /// From Thetis wdsp/cfcomp.c:700-715 [v2.10.3.13].
    void setTxCfcPrecompDb(double dB);

    /// CFC post-EQ run gate.  Wraps SetTXACFCOMPPeqRun(channel, on ? 1 : 0).
    /// Independent from setTxCfcRunning — when on, WDSP applies the peq
    /// filter after the comp curve.
    ///
    /// From Thetis wdsp/cfcomp.c:717-727 [v2.10.3.13].
    void setTxCfcPostEqRunning(bool on);

    /// CFC pre-PEQ gain in dB.  Wraps SetTXACFCOMPPrePeq(channel, dB).  WDSP
    /// stores pow(10, 0.05 * dB) as `prepeqlin`.
    ///
    /// From Thetis wdsp/cfcomp.c:729-737 [v2.10.3.13].
    void setTxCfcPrePeqDb(double dB);

    /// CPDR (compressor) run gate.  Wraps SetTXACompressorRun(channel,
    /// on ? 1 : 0).
    ///
    /// SIDE EFFECT: SetTXACompressorRun calls TXASetupBPFilters(channel)
    /// internally (compress.c:106 [v2.10.3.13]) — toggling CPDR rebuilds bp1
    /// and the gated bp2 to track the compression-and-clip routing in the
    /// TXA pipeline.  Callers don't need to do anything special; just be
    /// aware that this is more than a single-flag toggle.
    ///
    /// From Thetis wdsp/compress.c:99-109 [v2.10.3.13].
    void setTxCpdrOn(bool on);

    /// CPDR (compressor) gain in dB.  Wraps SetTXACompressorGain(channel, dB).
    /// WDSP converts to linear via pow(10, dB / 20.0) internally.
    ///
    /// From Thetis wdsp/compress.c:111-117 [v2.10.3.13].
    void setTxCpdrGainDb(double dB);

    /// CESSB (osctrl) run gate.  Wraps SetTXAosctrlRun(channel, on ? 1 : 0).
    ///
    /// SIDE EFFECT 1: SetTXAosctrlRun calls TXASetupBPFilters(channel)
    /// internally (osctrl.c:148 [v2.10.3.13]) — toggling CESSB rebuilds bp2.
    ///
    /// SIDE EFFECT 2 / IMPORTANT SEMANTIC: bp2.run (the CESSB-side bandpass)
    /// is only set when *both* `compressor.run` AND `osctrl.run` are 1
    /// (TXA.c:843-852, parallel switch arms in TXASetupBPFilters
    /// [v2.10.3.13]).  Calling setTxCessbOn(true) without first turning CPDR
    /// on is therefore effectively a no-op at the audio level — osctrl.run
    /// is set, but the BP filter that feeds it stays off.  This wrapper does
    /// NOT enforce that coupling; it matches Thetis behaviour exactly and
    /// lets WDSP own the dependency.
    ///
    /// From Thetis wdsp/osctrl.c:142-150 [v2.10.3.13].
    /// From Thetis wdsp/TXA.c:843-868 [v2.10.3.13] — bp2.run gating.
    void setTxCessbOn(bool on);

    // ── B-3.1: TX Phase Rotator parameter wrappers (Phase 3M-3a-ii Batch 1.6) ─
    //
    // Three thin C++ wrappers over the TXA phrot parameter section
    // (iir.c:675-703).  3M-3a-i Stage::PhRot already wired the Run flag via
    // SetTXAPHROTRun (setStageRunning case arm).  These three pick up the
    // remaining tunables that the Thetis tpDSPCFC tab exposes alongside the
    // CFC controls.  All wrappers route to TX channel kTxChannelId = 1 and
    // inherit the standard rsmpin.p == nullptr null-guard pattern.  All are
    // csDSP-protected (cs_update) inside WDSP, so they are safe to call from
    // the main thread while the audio thread runs.

    /// Phase-rotator corner frequency in Hz.  Wraps SetTXAPHROTCorner.
    ///
    /// WDSP rebuilds the all-pass coefficient bank on every set
    /// (decalc_phrot + calc_phrot internally) — non-trivial cost; do not
    /// spam this from a slider's continuous-change signal without rate-
    /// limiting.
    ///
    /// From Thetis wdsp/iir.c:675-683 [v2.10.3.13].
    void setTxPhrotCornerHz(double hz);

    /// Phase-rotator number of all-pass stages.  Wraps SetTXAPHROTNstages.
    ///
    /// WDSP rebuilds the coefficient bank on every set (decalc_phrot +
    /// calc_phrot internally) — non-trivial cost; do not spam.
    ///
    /// From Thetis wdsp/iir.c:686-694 [v2.10.3.13].
    void setTxPhrotNstages(int nstages);

    /// Phase-rotator reverse-rotation flag.  Wraps SetTXAPHROTReverse.
    ///
    /// Cheap — WDSP just flips the sign; no coefficient rebuild.
    ///
    /// From Thetis wdsp/iir.c:697-703 [v2.10.3.13].
    void setTxPhrotReverse(bool reverse);

    // ── B-3.2: TX CFC live-display readback (Phase 3M-3a-ii follow-up) ──────
    //
    // Live CFC compression-display readback for the parametric-EQ widget bar
    // chart.  WDSP populates `cfc_gain_copy[]` and `delta_copy[]` snapshots
    // inside the audio thread (cfcomp.c:558-566 [v2.10.3.13] — sets
    // mask_ready=1 once per FFT mask update); this wrapper is the main-thread
    // pull side that calls GetTXACFCOMPDisplayCompression to drain those
    // snapshots into a caller-owned buffer.  csDSP-protected inside WDSP.

    /// Number of bins delivered by GetTXACFCOMPDisplayCompression.
    ///
    /// From Thetis cfcomp.c:379 [v2.10.3.13] — msize = fsize/2+1 with
    /// fsize=2048 (TXA.c:209).
    static constexpr int    kCfcDisplayBinCount      = 1025;

    /// Sample rate the bins are spaced against (so callers can map bin index
    /// to Hz: `hz = i * kCfcDisplaySampleRateHz / (kCfcDisplayBinCount - 1)`).
    ///
    /// From Thetis frmCFCConfig.cs:411 [v2.10.3.13] — TX dsp_rate=96000,
    /// Nyquist=48 kHz.
    static constexpr double kCfcDisplaySampleRateHz  = 48000.0;

    /// Pull the latest CFC compression-display snapshot from WDSP.
    ///
    /// Wraps WDSP::GetTXACFCOMPDisplayCompression — From Thetis
    /// cfcomp.c:740-757 [v2.10.3.13].  Returns true iff WDSP had new data
    /// since the previous call (i.e. mask_ready was 1 inside WDSP); on true,
    /// fills `compValues[0..kCfcDisplayBinCount-1]` with per-bin compression
    /// in dB.  On false, `compValues` is untouched.  Thread-safe (WDSP-
    /// internal csDSP lock).
    ///
    /// `bufferSize` must be ≥ kCfcDisplayBinCount or the wrapper returns
    /// false without touching WDSP.
    bool getCfcDisplayCompression(double* compValues, int bufferSize) noexcept;

    // ── TX fixed-gain output level (issue #167 Phase 1 Agent 1C) ────────────
    //
    // Sets the TXA fixed-gain scalar applied uniformly to the I and Q audio
    // paths via WDSP/ChannelMaster SetTXFixedGain(channel, Igain, Qgain).
    //
    // Mirrors Thetis cmaster.cs:1115-1119 [v2.10.3.13] CMSetTXOutputLevel:
    //   public static void CMSetTXOutputLevel()
    //   {
    //       double level = Audio.RadioVolume * Audio.HighSWRScale;
    //       cmaster.SetTXFixedGain(0, level, level);
    //   }
    //
    // The `level` argument is the already-composed product of
    // `Audio.RadioVolume * Audio.HighSWRScale` — SWR-foldback compensation
    // enters the chain here per Thetis cmaster.cs:1115-1119
    // CMSetTXOutputLevel.  RadioModel call-site composition
    // (audio_volume * swrProtect) is deferred to issue #167 Phase 4A; this
    // wrapper takes the composed scalar and pushes it through to WDSP.
    //
    // Idempotent: the WDSP entry point is invoked only when `level` differs
    // from the last applied value (matches the NaN-aware double-setter
    // pattern used by setMicPreamp / setVoxAttackThreshold / D.3 / D.6).
    // m_lastFixedGain initialises to quiet_NaN so the first call (any
    // value, including 0.0) always passes the guard.
    //
    // Range: typically [0, 1]; values >1 are accepted (matches Thetis —
    // the cmaster.cs setter applies no clamp; downstream WDSP handles).

    /// Set the TXA fixed-gain scalar applied to the I and Q audio paths.
    void setTxFixedGain(double level);

    // ── Per-stage Run override (3M-1a C.4) ──────────────────────────────────
    //
    // Activate or deactivate a single TXA pipeline stage by name.
    //
    // Callers that need to enable or disable individual stages outside the
    // 3M-1a minimum-path bulk-activate (handled by setRunning()) may use
    // this method directly.  3M-1b will use it to activate Stage::Panel and
    // Stage::MicMeter; 3M-3a will use it for Stage::Eqp, Stage::Leveler,
    // Stage::Compressor, etc.
    //
    // Supported stages and their WDSP APIs (from wdsp/ sources [v2.10.3.13]):
    //   Gen0        → SetTXAPreGenRun        (gen.c:636-641)
    //   Gen1        → SetTXAPostGenRun       (gen.c:784-789)
    //   Panel       → SetTXAPanelRun         (patchpanel.c:201-206)
    //   PhRot       → SetTXAPHROTRun         (iir.c:665-670)
    //   AmSq        → SetTXAAMSQRun          (amsq.c:246-252)
    //   Eqp         → SetTXAEQRun            (eq.c:742-747)
    //   Compressor  → SetTXACompressorRun    (compress.c:99-109)
    //   OsCtrl      → SetTXAosctrlRun        (osctrl.c:142-147)
    //   Cfir        → SetTXACFIRRun          (cfir.c:233-238)
    //   CfComp      → SetTXACFCOMPRun        (cfcomp.c:632-637)
    //   Leveler     → SetTXALevelerSt        (wcpAGC.c:613-618) [3M-3a-i B-2]
    //   Alc         → SetTXAALCSt            (wcpAGC.c:570-575) [3M-3a-i B-2]
    //
    // Unsupported stages (no public WDSP Run API, or managed internally):
    //   RsmpIn / RsmpOut: run managed by TXAResCheck() — not externally settable.
    //   UsLew:            no run flag; channel-upslew driven.
    //   MicMeter / AlcMeter: always-on (run=1); no public Run setter in WDSP
    //     meter.c:36-57 [v2.10.3.13]. Documented no-op + qCDebug log (D.4).
    //   AmMod / FmMod:   run controlled exclusively by SetTXAMode() (TXA.c:753-789
    //     [v2.10.3.13]). No standalone Set*Run API. Documented no-op + qCWarning
    //     log (D.4). Use setTxMode() to activate these stages.
    //   EqMeter / LvlrMeter / CfcMeter / CompMeter /
    //   OutMeter / Sip1 / Calcc / Iqc / Alc / Bp0 / Bp1 / Bp2:
    //     use SetTXA*Run variants added in later tasks (3M-3a, 3M-4)
    //     or remain always-on for the lifetime of the 3M-1a/1b session.
    //
    // For unsupported stages this method logs a warning and is a no-op.
    //
    // From Thetis wdsp/ source files [v2.10.3.13] — individual Set*Run APIs.
    void setStageRunning(Stage s, bool run);

#ifdef NEREUS_BUILD_TESTS
    // ── Test seam (Phase 3M-1b D.1, updated for 3M-1c E.1 push model) ─────
    //
    // Synchronously drive one fexchange2 cycle by pushing the given mic
    // block through the production slot.  Mirrors what AudioEngine's
    // micBlockReady signal does at runtime (Qt::DirectConnection, Phase L
    // wire-up).
    //
    // Pass `samples == nullptr` and `frames == 0` to drive the silence
    // path (TUNE-tone PostGen output still reaches sendTxIq).  Pass any
    // mismatched frame count to exercise the contract-violation guard.
    //
    // Only available when NEREUS_BUILD_TESTS is defined.  Production
    // builds rely on the slot connection wired by RadioModel (Phase L).
    void tickForTest(const float* samples, int frames)
    {
        driveOneTxBlock(samples, frames);
    }

    // Read-only access to fexchange0 input I-channel after a tickForTest()
    // cycle.  Phase 3M-1c TX pump v3: internal storage migrated from
    // separate float Iin/Qin buffers (fexchange2) to a single interleaved
    // double m_in (fexchange0), so these accessors deinterleave + downcast
    // on demand to keep existing test API stable.  cached_inI / cached_inQ
    // are mutable so the const accessors can refresh them.
    const std::vector<float>& inIForTest() const {
        m_cachedInI.resize(static_cast<size_t>(m_inputBufferSize));
        for (int i = 0; i < m_inputBufferSize; ++i) {
            m_cachedInI[static_cast<size_t>(i)] = static_cast<float>(m_in[2 * i + 0]);
        }
        return m_cachedInI;
    }
    const std::vector<float>& inQForTest() const {
        m_cachedInQ.resize(static_cast<size_t>(m_inputBufferSize));
        for (int i = 0; i < m_inputBufferSize; ++i) {
            m_cachedInQ[static_cast<size_t>(i)] = static_cast<float>(m_in[2 * i + 1]);
        }
        return m_cachedInQ;
    }
    // Direct double interleaved view for tests that want to read m_in
    // without the float deinterleave step.
    const std::vector<double>& inForTest() const { return m_in; }

    // ── Test seams (Phase 3M-1b D.3) — VOX / anti-VOX last-value read-back ──
    //
    // Allow tests to verify:
    //   (a) The first call propagates (NaN sentinel fires → value stored).
    //   (b) Round-trip updates (set A → set B → last returns B).
    //   (c) Idempotent guard fires on duplicate calls (value unchanged).
    bool   lastVoxRunForTest()                const noexcept { return m_voxRunLast; }
    double lastVoxAttackThresholdForTest()    const noexcept { return m_voxAttackThresholdLast; }
    double lastVoxHangTimeForTest()           const noexcept { return m_voxHangTimeLast; }
    bool   lastAntiVoxRunForTest()            const noexcept { return m_antiVoxRunLast; }
    double lastAntiVoxGainForTest()           const noexcept { return m_antiVoxGainLast; }

    // ── Test seam (Phase 3M-1b D.6) — mic preamp last-value read-back ────────
    //
    // Allow tests to verify:
    //   (a) First call with any value passes the NaN guard and stores the value.
    //   (b) Zero-value (mute case) stores 0.0 correctly.
    //   (c) Idempotent guard fires on duplicate calls (value unchanged).
    double lastMicPreampForTest()             const noexcept { return m_micPreampLast; }

    // ── Test seam (issue #167 Phase 1 Agent 1C) — TX fixed-gain last-value ──
    //
    // Allow tests to verify:
    //   (a) Initial state is NaN (first call always passes the guard).
    //   (b) First call dispatches and stores the value (NaN sentinel fires).
    //   (c) Identical second call is a no-op (idempotent guard fires).
    //   (d) Different value updates the stored last-value.
    double lastFixedGainForTest()             const noexcept { return m_lastFixedGain; }
#endif // NEREUS_BUILD_TESTS

public slots:
    // ── Per-profile TX filter debounce (Plan 4 D8) ───────────────────────────

    /// Request a TX filter change (audio Hz, mode-agnostic).  Coalesces rapid
    /// successive calls via a 50 ms debounce so spinbox arrow-clicks don't spam
    /// WDSP.  When the timer fires, applyTxFilterForMode maps the audio Hz
    /// values to IQ-space per the current mode and calls setTxBandpass.
    ///
    /// Cross-thread safe: designed for auto-queued connection from
    /// TransmitModel::filterChanged (main thread) → TxChannel (audio thread)
    /// via RadioModel::txFilterRequest intermediate signal.  Qt auto-connection
    /// routes to QueuedConnection once TxChannel has been moved to
    /// TxWorkerThread, so QTimer::start() executes on the timer's owning
    /// thread.
    ///
    /// NereusSDR-original glue.  Per-mode mapping is identical to the TUN
    /// bandpass in setTuneTone() (TxChannel.cpp:505-528), cited from
    /// deskhpsdr/transmitter.c:2136-2186 [@120188f].
    void requestFilterChange(int audioLowHz, int audioHighHz, DSPMode mode);

    // ── TX pump slot (Phase 3M-1c TX pump architecture redesign v3) ──────────
    //
    /// LEGACY / TEST-ONLY: drive one fexchange0 cycle from a mono float
    /// TX-mic block.  Retained as a test seam (TxWorkerThread::tickForTest
    /// pre-v3 paths and unit tests that pre-date the v3 redesign still
    /// invoke this overload).  Production callsite is
    /// driveOneTxBlockFromInterleaved (below) — TxWorkerThread::run
    /// drains an interleaved I/Q double buffer from TxMicSource and hands
    /// it directly to that overload, bypassing the float→double
    /// conversion path.
    ///
    /// Contract (unchanged from 3M-1c E.1):
    ///   - `samples == nullptr && frames == 0`  →  silence path: zero-fill
    ///     m_in and dispatch fexchange0 (TUNE-tone PostGen output still
    ///     reaches sendTxIq).  Used by tests; production v3 callers route
    ///     through driveOneTxBlockFromInterleaved instead.
    ///   - `samples != nullptr && frames == m_inputBufferSize`  →  copy
    ///     samples into m_in's I channel, zero-fill Q, dispatch fexchange0.
    ///   - `samples != nullptr && frames != m_inputBufferSize`  →  contract
    ///     violation: log a qCWarning and return without dispatching.
    ///     The block-size invariant matches Thetis cmaster.c:460-487
    ///     [v2.10.3.13] — `r1_outsize == xcm_insize == in_size` end-to-end
    ///     (NereusSDR uses 64 in v3, dictated by Thetis getbuffsize(48000)
    ///     parity at cmsetup.c:106-110 [v2.10.3.13]).
    ///
    /// **Thread affinity:** runs on the TxChannel's current thread.
    /// !m_running and !m_connection short-circuit the slot to a no-op.
    void driveOneTxBlock(const float* samples, int frames);

    /// Drive one fexchange0 cycle from a pre-populated interleaved I/Q
    /// double buffer.  Phase 3M-1c TX pump v3 production callsite:
    /// TxWorkerThread::dispatchOneBlock hands this method the buffer it
    /// just drained from TxMicSource (one block of m_inputBufferSize
    /// pairs == 2*m_inputBufferSize doubles).
    ///
    /// `interleavedIn` MUST point to 2 * m_inputBufferSize doubles
    /// (interleaved I0,Q0,I1,Q1,…) — passing a smaller buffer is UB.
    /// nullptr is treated as the silence path: m_in is zero-filled and
    /// fexchange0 is dispatched (TUN PostGen still produces clean carrier).
    ///
    /// Mirrors Thetis cmaster.c:389 [v2.10.3.13]:
    ///   fexchange0 (chid (stream, 0), pcm->in[stream],
    ///               pcm->xmtr[tx].out[0], &error);
    void driveOneTxBlockFromInterleaved(const double* interleavedIn);

signals:
    // ── Per-profile TX filter applied (Plan 4 D8) ────────────────────────────
    //
    /// Emitted from applyTxFilterForMode after computing IQ-space values and
    /// before calling setTxBandpass.  Carries the IQ-space values (signed,
    /// post-mode-mapping) so tests can verify both that the debounce fired AND
    /// that the per-mode mapping is correct.
    ///
    /// Production code may ignore this signal — the actual WDSP call happens
    /// via setTxBandpass immediately after the emit.  Tests wire a QSignalSpy.
    void txFilterApplied(int lowHzIq, int highHzIq);

    // ── MON siphon signal (3M-1b D.5) ────────────────────────────────────────
    //
    /// Emitted on the audio thread inside driveOneTxBlock() after every
    /// fexchange2 cycle (and after sendTxIq delivers the block to the radio).
    /// Carries the post-SSB-modulator I-channel audio (m_outI) as a raw
    /// pointer plus the frame count.
    ///
    /// **DirectConnection ONLY.** The pointer is only valid during the
    /// synchronous slot dispatch. Subscribers MUST connect with
    /// Qt::DirectConnection. QueuedConnection is unsafe: the buffer will
    /// be reused on the next driveOneTxBlock() cycle before the queued
    /// slot runs, leading to silent data corruption.
    ///
    /// Design note (3M-1b): the signal carries m_outI.data() directly —
    /// the interleaved-I output of fexchange2, which IS the post-SSB-
    /// modulator I-channel. This is the simplest correct tap for MON
    /// playback. A dedicated Sip1-stage tap (wdsp/sip.c, TXA stage 25)
    /// can be wired in 3M-3 if a different processing point is needed for
    /// acoustic monitoring; for 3M-1b the m_outI path is sufficient.
    ///
    /// Sample rate: matches TXA dsp-rate —
    ///   96 kHz on P2 (Saturn / Orion-II with 192 kHz ADC output rate);
    ///   48 kHz on P1 (Hermes / HL2 / Angelia).
    /// Subscribers (AudioEngine::txMonitorBlockReady in Phase L) are
    /// responsible for downmix / resample to speaker output rate.
    ///
    /// Plan: 3M-1b D.5 (this commit). Pre-code review §4.3.
    void sip1OutputReady(const float* samples, int frames);

private slots:
    // ── Per-profile TX filter (Plan 4 D8) — debounce fire slot ───────────────

    /// Called when m_filterDebounceTimer fires.  Reads m_pending* fields
    /// and invokes applyTxFilterForMode + setTxBandpass.
    void applyPendingFilter();

private:
    // ── Per-profile TX filter (Plan 4 D8) — private helper ───────────────────

    /// Map (audioLow, audioHigh, mode) → IQ-space (signed) and call
    /// setTxBandpass.  Emits txFilterApplied(iqLow, iqHigh) before the WDSP
    /// call so tests can spy on the result.
    ///
    /// Per-mode mapping from deskhpsdr/transmitter.c:2136-2186 [@120188f]
    /// (same cite as setTuneTone() TUN bandpass at TxChannel.cpp:505-528 —
    /// the TUN path is NOT touched; this helper runs only from the debounce
    /// timer, never from setTuneTone):
    ///   USB family (USB / DIGU / CWU / others): IQ = [+low, +high]
    ///   LSB family (LSB / DIGL / CWL):          IQ = [-high, -low]
    ///   Symmetric  (AM / SAM / DSB / FM / DRM): IQ = [-high, +high]
    void applyTxFilterForMode(int audioLowHz, int audioHighHz, DSPMode mode);

    // ── TX I/Q production loop internals ────────────────────────────────────
    //
    // History (deleted machinery):
    //   - Phase 3M-1a G.1 introduced an m_txProductionTimer firing every 5 ms.
    //   - Phase 3M-1c E.1 dropped that timer in favour of an
    //     AudioEngine::micBlockReady → driveOneTxBlock slot wired via
    //     Qt::DirectConnection.
    //   - Phase 3M-1c E.1 bench-fix added an m_silenceTimer fallback for the
    //     no-mic case (gravelly SSB voice TX bench regression).
    //   - Phase 3M-1c TX pump architecture redesign (2026-04-29) deleted both
    //     timers entirely.  TxWorkerThread now drives driveOneTxBlock at
    //     ~5 ms cadence; when AudioEngine::pullTxMic returns < kBlockFrames,
    //     TxWorkerThread zero-fills the gap and silence falls out for free
    //     (PostGen TUNE-tone still produces output; mic-driven SSB still
    //     works at the natural 256/48 kHz cadence).  See
    //     docs/architecture/phase3m-1c-tx-pump-architecture-plan.md for
    //     the architectural rationale.

    // Non-owning pointers — injected by RadioModel, cleared on disconnect.
    RadioConnection* m_connection{nullptr};  // sendTxIq recipient
    // Retained for the future Radio-mic source path (mic source piped through
    // the radio's discovery socket).  In 3M-1c E.1 the PC-mic path is push-
    // driven and no longer pulls from m_micRouter; this field is reserved
    // so a future task can wire the Radio-mic source through the same slot
    // without touching TxChannel's public surface again.
    TxMicRouter*     m_micRouter{nullptr};

    // fexchange0 I/O buffers — allocated at construction from
    // m_inputBufferSize / m_outputBufferSize, reused each driveOneTxBlock()
    // call.  Phase 3M-1c TX pump v3 switched from fexchange2 (separate
    // float I/Q) to fexchange0 (interleaved double I/Q) to match Thetis's
    // cmaster.c:389 [v2.10.3.13] callsite exactly.
    //
    // fexchange0 requires:
    //   in:  exactly in_size  complex (== m_inputBufferSize  pairs == 2*N doubles)
    //   out: exactly out_size complex (== m_outputBufferSize pairs == 2*N doubles)
    //
    // From Thetis wdsp/iobuffs.c:464-516 [v2.10.3.13] — fexchange0 prototype.
    // From Thetis wdsp/cmaster.c:177-190 [v2.10.3.13] — in_size / ch_outrate.
    // Default 64 matches Thetis getbuffsize(48000) (cmsetup.c:106-110
    // [v2.10.3.13]); WdspEngine::createTxChannel passes the actual sizes
    // computed from the TX out-rate (e.g., 256 out at 192 kHz for P2 G2).
    int m_inputBufferSize{64};   // == OpenChannel in_size
    int m_outputBufferSize{64};  // == in_size × out_rate / in_rate

    // Interleaved I/Q double buffers — fexchange0 layout.
    // m_in.size()  == 2 * m_inputBufferSize  (one I + one Q per frame)
    // m_out.size() == 2 * m_outputBufferSize (likewise)
    std::vector<double> m_in;
    std::vector<double> m_out;

    // Float scratch for sendTxIq — sendTxIq's signature still takes float*
    // (it pushes into the connection's SPSC ring which holds floats).
    // Convert from m_out (double) → m_outInterleavedFloat (float) before
    // calling sendTxIq.  Size: 2 * m_outputBufferSize floats.
    std::vector<float> m_outInterleavedFloat;

    // Float scratch for the post-fexchange0 MON siphon emit — the
    // sip1OutputReady signal carries `const float*`, but m_out is double,
    // so cache a downcast I-only view here.  Size: m_outputBufferSize floats.
    std::vector<float> m_outIFloatScratch;

#ifdef NEREUS_BUILD_TESTS
    // Mutable test caches — populated lazily by inIForTest/inQForTest so
    // those accessors stay drop-in compatible with pre-3M-1c-v3 tests
    // that consumed `vector<float>` views of the (now defunct) Iin/Qin
    // buffers.  Marked mutable so the accessors can stay const.
    mutable std::vector<float> m_cachedInI;
    mutable std::vector<float> m_cachedInQ;
#endif

    const int m_channelId;
    // m_running is std::atomic so isRunning() can be safely read from
    // threads other than the TxChannel's own thread (e.g., main-thread UI
    // queries while the channel lives on TxWorkerThread).  setRunning runs
    // on the channel's thread (worker after moveToThread); driveOneTxBlock
    // reads it on the same thread.
    std::atomic<bool> m_running{false};

    // ── VOX / anti-VOX last-set values (D.3) ─────────────────────────────────
    //
    // Each setter stores the last value dispatched to WDSP and skips the WDSP
    // call if the incoming value matches (idempotent guard).
    //
    // Bool setters initialise to false (matches WDSP DEXP/AntiVOX defaults).
    // First call with false is therefore a no-op — intentional: the MoxController
    // (Phase H.1) calls these at TX-on/TX-off; initialising to false avoids a
    // redundant WDSP call on the first TX-off cycle.
    //
    // Double setters initialise to quiet_NaN so the first call (whatever value)
    // always passes the guard.  NaN != NaN is guaranteed by IEEE 754, so the
    // plain `thresh == m_voxAttackThresholdLast` expression fires when NaN is
    // the stored value; the `std::isnan` pre-check is added for clarity.
    bool   m_voxRunLast             = false;
    double m_voxAttackThresholdLast = std::numeric_limits<double>::quiet_NaN();
    double m_voxHangTimeLast        = std::numeric_limits<double>::quiet_NaN();
    bool   m_antiVoxRunLast         = false;
    double m_antiVoxGainLast        = std::numeric_limits<double>::quiet_NaN();

    // ── Mic preamp last-set value (D.6) ──────────────────────────────────────
    //
    // Initialised to quiet_NaN so the first setMicPreamp() call (any value,
    // including 0.0) always passes the idempotent guard.  NaN != NaN is
    // guaranteed by IEEE 754; the `std::isnan` pre-check in setMicPreamp()
    // makes this explicit.
    //
    // From Thetis console.cs:28805-28817 [v2.10.3.13] — setAudioMicGain:
    //   Audio.MicPreamp = 0.0  (mute=true)
    //   Audio.MicPreamp = Math.Pow(10.0, gain_db / 20.0)  (mute=false)
    double m_micPreampLast = std::numeric_limits<double>::quiet_NaN();

    // ── TX fixed-gain last-set value (issue #167 Phase 1 Agent 1C) ───────────
    //
    // Initialised to quiet_NaN so the first setTxFixedGain() call (any value,
    // including 0.0) always passes the idempotent guard.  NaN != NaN is
    // guaranteed by IEEE 754; the `std::isnan` pre-check in setTxFixedGain()
    // makes this explicit.  This field exists purely for the redundant-call
    // suppression — the WDSP entry point lives in cmaster.SetTXFixedGain
    // (Thetis ChannelMaster/txgain.c:127-134 [v2.10.3.13]) and is hot-path
    // during TX, so suppressing duplicate calls keeps cs_update0 contention
    // minimal.
    //
    // From Thetis cmaster.cs:1115-1119 [v2.10.3.13] — CMSetTXOutputLevel:
    //   double level = Audio.RadioVolume * Audio.HighSWRScale;
    //   cmaster.SetTXFixedGain(0, level, level);
    double m_lastFixedGain = std::numeric_limits<double>::quiet_NaN();

    // ── TXA PostGen split-property cache (3M-1c E.3 / E.4) ──────────────────
    //
    // The WDSP `SetTXAPostGenTTFreq(ch, f1, f2)` /
    // `SetTXAPostGenTTMag(ch, m1, m2)` /
    // `SetTXAPostGenTTPulseToneFreq(ch, f1, f2)` /
    // `SetTXAPostGenTTPulseMag(ch, m1, m2)` calls take BOTH partners at
    // once, but the Thetis C# property surface exposes Freq1/Freq2 / Mag1/
    // Mag2 as separate setters.  Each setter caches the partner value here
    // so the combined WDSP call uses the most recent both-values pair.
    //
    // Default 0.0 matches Thetis radio.cs:3697-4033 [v2.10.3.13] private
    // field defaults (`tx_postgen_tt_freq1_dsp = 0.0`, etc.).
    //
    // No NaN sentinel is needed — these are pass-through wrappers without
    // an idempotency guard (the wrappers always issue the WDSP call).
    double m_postGenTTFreq1Cache         = 0.0;
    double m_postGenTTFreq2Cache         = 0.0;
    double m_postGenTTMag1Cache          = 0.0;
    double m_postGenTTMag2Cache          = 0.0;
    double m_postGenTTPulseToneFreq1Cache = 0.0;
    double m_postGenTTPulseToneFreq2Cache = 0.0;
    double m_postGenTTPulseMag1Cache      = 0.0;
    double m_postGenTTPulseMag2Cache      = 0.0;

    // ── Per-profile TX filter debounce (Plan 4 D8) ───────────────────────────
    //
    // 50 ms single-shot timer — matches the magnitude of Thetis's
    // SetTXFilter coalescing behaviour (rapid UI spinbox ticks coalesce
    // to one WDSP call once the user pauses).  Pending audio-space values
    // are stored here and consumed by applyPendingFilter() on timeout.
    //
    // Thread affinity: the timer is owned by TxChannel, which is moved
    // to TxWorkerThread in RadioModel.  After moveToThread, all start()
    // calls (via requestFilterChange, which arrives as a QueuedConnection)
    // run on the worker thread — fully thread-safe by Qt's timer design.
    QTimer  m_filterDebounceTimer;
    int     m_pendingAudioLow  = 0;
    int     m_pendingAudioHigh = 0;
    DSPMode m_pendingMode      = DSPMode::USB;
};

} // namespace NereusSDR
