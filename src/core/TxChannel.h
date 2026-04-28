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
// =================================================================

#pragma once

#include <QObject>
#include <QTimer>

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
// Thread safety:
//   - Main thread: create/destroy, all stage-run-state queries (stageRunning)
//   - Audio thread (future, C.4+): setRunning / processTx
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
    /// it mirrors the local m_running flag, which is sufficient for 3M-1a
    /// single-threaded use (main thread is the only writer in 3M-1a).
    bool isRunning() const noexcept { return m_running; }

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
    // ── Test seam (Phase 3M-1b D.1) ─────────────────────────────────────────
    //
    // Synchronously drive one fexchange2 cycle. Bypasses the 5 ms QTimer so
    // tests can deterministically inspect input buffers after pullSamples has
    // run without waiting for timer ticks.
    //
    // Only available when NEREUS_BUILD_TESTS is defined.  Production builds do
    // not include this method; the timer-based path is the only entry point.
    void tickForTest() { driveOneTxBlock(); }

    // Read-only access to fexchange2 input buffers after a tickForTest() cycle.
    // Used by D.1 tests to verify: (a) m_inI matches the injected mic source's
    // output, and (b) m_inQ is zero-filled (real mic input has no Q component).
    const std::vector<float>& inIForTest() const { return m_inI; }
    const std::vector<float>& inQForTest() const { return m_inQ; }

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
#endif // NEREUS_BUILD_TESTS

private:
    // ── TX I/Q production loop internals (3M-1a G.1) ─────────────────────────

    // Drive one fexchange2 call: pull m_inI.size() samples from the mic
    // router (zeros in 3M-1a), call fexchange2(channelId, …), interleave
    // the output, and push to m_connection->sendTxIq().
    //
    // Called by m_txProductionTimer on every tick while m_running is true.
    // Must not block; guards against null m_connection and uninitialized
    // WDSP channel.
    void driveOneTxBlock();

    // Production timer — fires at 5 ms intervals while TX is active.
    // fexchange2 processes m_inputBufferSize (256) samples per call.
    // At 48 kHz input: 256/48000 = ~5.33 ms per block → one fexchange2 call per tick.
    // At 192 kHz output (P2 Saturn): m_outputBufferSize = 1024 samples per call.
    // The P2 consumer (m_txIqTimer at 5 ms, 4 frames/tick) drains ~960 samples
    // per tick — slightly under the producer rate, so the SPSC ring fills
    // gradually until natural backpressure (audio underrun would block).
    //
    // Qt::PreciseTimer: reduces OS-scheduler jitter on the audio path.
    QTimer* m_txProductionTimer{nullptr};

    // Non-owning pointers — injected by RadioModel, cleared on disconnect.
    RadioConnection* m_connection{nullptr};  // sendTxIq recipient
    TxMicRouter*     m_micRouter{nullptr};   // mic samples (NullMicSource for 3M-1a)

    // fexchange2 I/O buffers — allocated at construction from m_inputBufferSize /
    // m_outputBufferSize, reused each driveOneTxBlock() call.
    //
    // fexchange2 requires:
    //   Iin/Qin:   exactly in_size == m_inputBufferSize samples
    //   Iout/Qout: exactly out_size == m_outputBufferSize samples
    //
    // Calling fexchange2 with wrong-sized buffers produces no output (silent error).
    // Previous code used kTxDspBufferSize (4096) for all four buffers — this was
    // the root cause of the bench-round-3 silent failure.
    //
    // From Thetis wdsp/iobuffs.c fexchange2 [v2.10.3.13].
    // From Thetis wdsp/cmaster.c:177-190 [v2.10.3.13] — in_size / ch_outrate.
    int m_inputBufferSize{256};   // == OpenChannel in_size; fexchange2 Iin/Qin size
    int m_outputBufferSize{256};  // == in_size × out_rate / in_rate; fexchange2 Iout/Qout size
    std::vector<float> m_inI;
    std::vector<float> m_inQ;
    std::vector<float> m_outI;
    std::vector<float> m_outQ;
    // Interleaved [I0,Q0,I1,Q1,…] output for sendTxIq.
    // Size: 2 × m_outputBufferSize floats.
    std::vector<float> m_outInterleaved;

    const int m_channelId;
    bool m_running{false};

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
};

} // namespace NereusSDR
