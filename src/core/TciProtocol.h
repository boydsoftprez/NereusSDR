// no-port-check: NereusSDR-original TciProtocol skeleton — parser ported from
// Thetis TCIServer.cs:4900-4924 [v2.10.3.13] (split on first ':', case-insensitive
// command lookup), dispatch shape from TCIServer.cs:4924-5197 [v2.10.3.13] (two
// switches: set if parts.Length==2, query if parts.Length==1). Empty handler
// stubs land Phase 5+. AetherSDR seam pattern reference: TciProtocol.{h,cpp} [@0cd4559].

// src/core/TciProtocol.h  (NereusSDR)
// NereusSDR-original — TCI command protocol handler.
//
// Parser ported from Thetis TCIServer.cs:4900-4924 [v2.10.3.13].
// Two-switch dispatch shape from Thetis TCIServer.cs:4924-5197 [v2.10.3.13].
// AetherSDR seam pattern reference: TciProtocol.{h,cpp} [@0cd4559].
//
// This file REPLACES the Phase 1 stub (commit 77d27b3).
//
// Modification history (NereusSDR):
//   2026-05-10 — Phase 3J-1 Task 3.1 by J.J. Boyd (KG4VCF);
//                AI-assisted transformation via Anthropic Claude Code.

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "TciVfoCoalescer.h"

namespace NereusSDR {

// ─────────────────────────────────────────────────────────────────────────
// AppSettings keys introduced by Phase 3J-1 TCI Server.
// All persisted to ~/.config/NereusSDR/NereusSDR.settings via AppSettings
// (NOT QSettings; per project convention). PascalCase key names.
//
// | Key                                  | Type   | Default | Notes
// | TciServerEnabled                     | bool   | False   | Server on/off
// | TciServerPort                        | int    | 50001   | Bind port (addr always 127.0.0.1)
// | TciSendInitialFrequencyStateOnConnect| bool   | True    | VFO/IF/DDS in init burst
// | TciRateLimitMsgsPerSec               | int    | 60      | Per-client message rate cap
// | TciAudioStreamSamples                | int    | 2048    | Audio block size [100..2048]
// | TciTxChannel                         | string | "Both"  | TX audio channel: Left/Right/Both
// | TciRxSensorIntervalMs                | int    | 200     | RX sensor push [30..1000]
// | TciTxSensorIntervalMs                | int    | 200     | TX sensor push [30..1000]
// | TciEmulateExpertSDR3Protocol         | bool   | False   | Compat flag
// | TciEmulateSunSDR2Pro                 | bool   | False   | Compat flag
// | TciCwluBecomesCw                     | bool   | False   | Compat flag
// | TciCwBecomesCwuAbove10mhz            | bool   | False   | Compat flag (W2PA #559)
// | TciIqSwap                            | bool   | True    | Compat flag
// | TciAlwaysStreamIq                    | bool   | False   | Compat flag
// | TciForgetRx2VfoBOnDisconnect         | bool   | False   | VFO quirk
// | TciUseRx1VfoaForRx2Vfoa              | bool   | False   | VFO quirk
// | TciCopyRx2VfobToVfoa                 | bool   | False   | VFO quirk
//
// Additional keys introduced by AudioTciPage (Phase 3J-1 Task 2.7):
// | TciSliceA_OutputSampleRate           | int    | 48000   | Slice A default audio rate (applied at connect-time)
// | TciSliceB_OutputSampleRate           | int    | 48000   | Slice B default audio rate (applied at connect-time for rx=1)
// | TciAudioStreamSampleType             | string | Float32 | Default sample type (Int16/Int24/Int32/Float32)
// | TciTxStreamBufferingMs               | int    | 50      | TX pre-buffer latency [10..200 ms] — Phase 3J-2 effect-site
//
// TciSliceA/B_OutputSampleRate and TciAudioStreamSampleType are applied to
// each new TciClientSession in TciServer::onNewConnection() and can be
// overridden per-client via explicit audio_samplerate: / audio_stream_*
// commands (TciServer interceptor in onTextMessageReceived, finding #1).
//
// Phases 5+ wire these into compat-flag handling. Phase 20 surfaces them in
// Setup → Network → TCI Server. See design doc Section 10.
// ─────────────────────────────────────────────────────────────────────────

class TciProtocol : public QObject {
    Q_OBJECT
public:
    explicit TciProtocol(QObject* radio = nullptr, QObject* parent = nullptr);
    ~TciProtocol() override = default;

    // Dispatch a single TCI command line. Returns the synchronous response
    // (empty for commands with no reply, or for the silent-error invariant
    // — unknown commands produce zero outbound traffic per design doc §4.1).
    QString handleCommand(const QString& command);

    // Notification queue — drained by TciServer after each handleCommand.
    bool hasPendingNotification() const;
    QString takePendingNotification();

    // Drain coalesced VFO updates into the pending notification queue.
    // Called by TciServer from the 5ms drain timer; ensures rapid VFO bursts
    // collapse to ≤ 1 frame per (rx, chan) per drain tick.
    // From Thetis TCIServer.cs:1722-1727 [v2.10.3.13] — outbound-coalesced map.
    void drainCoalescedNotifications();

    // Build the post-connect init burst. Stub returns empty list in Phase 3;
    // Phase 4 Task 4.1 replaces with the 8-line wrapper from
    // Thetis TCIServer.cs:2512-2552 [v2.10.3.13].
    QStringList buildInitBurst() const;

    // Slice ↔ trx mapping (NereusSDR architectural divergence per design doc §1.2):
    //   Slice A | trx:0,    Slice B | trx:1,    Slice C | trx:2,    Slice D | trx:3
    // Identity mapping in Phase 3; same mapping persists through subsequent phases.
    static int sliceToTrx(int slice);
    static int trxToSlice(int trx);

    // Phase 3 test-only instrumentation — incremented in handleSetCommand /
    // handleQueryCommand stubs so the dispatch-seam test can verify routing.
    // Phase 5+ may keep or remove these counters.
    int setDispatchCount() const { return m_setDispatchCount; }
    int queryDispatchCount() const { return m_queryDispatchCount; }
    void resetDispatchCounters();

private:
    // From Thetis TCIServer.cs:4924-5128 [v2.10.3.13] — 60-case set-command switch.
    // Phase 5+ adds individual cases via the matrix runner.
    QString handleSetCommand(const QString& name, const QStringList& args);

    // From Thetis TCIServer.cs:5134-5197 [v2.10.3.13] — 21-case query-command switch.
    // Phase 5+ adds individual cases via the matrix runner.
    QString handleQueryCommand(const QString& name);

    // ── VFO family handlers (Phase 6) ─────────────────────────────────────────
    // From Thetis TCIServer.cs:3724-3833 [v2.10.3.13] — handleVFOMessage.
    // Dispatches set (3 args) or query (2 args) by args.size().
    // UseRX1VFOaForRX2VFOa quirk deferred to Phase 6+ refinement.
    QString handleVfoCommand(const QStringList& args);

    // From Thetis TCIServer.cs:3284-3302 [v2.10.3.13] — handleVFOLock.
    // 3 args = set; 2 args = query (returns vfo_lock:rx,chan,bool; as response).
    QString handleVfoLockCommand(const QStringList& args);

    // From Thetis TCIServer.cs:3265-3283 [v2.10.3.13] — handleLock.
    // 2 args = set; 1 arg = query (returns lock:rx,bool; as response).
    // Drops channel arg per sendLock at TCIServer.cs:1921-1925 [v2.10.3.13].
    QString handleLockCommand(const QStringList& args);

    // ── Mode / filter family handlers (Phase 7) ───────────────────────────────
    // From Thetis TCIServer.cs:4926 [v2.10.3.13] — modulation case in set switch.
    // handleModulationMessage at TCIServer.cs:3837 [v2.10.3.13] dispatches by args.size():
    //   args.size() == 2 → set (parse mode string, set DSPMode + enqueue notification)
    //   args.size() == 1 → query (return current mode uppercase)
    // CWLUbecomesCW (TCIServer.cs:2148-2153 [v2.10.3.13]) DEFERRED (Phase 11/12 follow-up).
    // CWbecomesCWUabove10mhz (TCIServer.cs:3868-3895 [v2.10.3.13]) DEFERRED (needs VFOATX/VFOBTX).
    QString handleModulationCommand(const QStringList& args);

    // From Thetis TCIServer.cs:5085 [v2.10.3.13] — rx_filter_band case in set switch.
    // handleRxFilterBand at TCIServer.cs:4366-4413 [v2.10.3.13] dispatches by args.size():
    //   args.size() == 3 → set (rx, low, high — all int)
    //   args.size() == 1 → query (rx)
    QString handleRxFilterBandCommand(const QStringList& args);

    // ── TRX family handlers (Phase 8) ─────────────────────────────────────────
    // From Thetis TCIServer.cs:4932 [v2.10.3.13] — trx case in set switch.
    // handleTrxMessage at TCIServer.cs:3459-3559 [v2.10.3.13]:
    //   args.size() >= 2 → set (rx, mox[, "tci"]); SIMPLIFIED — TX mutex deferred to Phase 17.
    //   args.size() == 1 → query (rx) → sendMOX(rx, MOX, m_txUsesTCIAudio).
    //   Phase 8 emits trx:rx,bool; without ",tci" suffix.
    QString handleTrxCommand(const QStringList& args);

    // From Thetis TCIServer.cs:4935 [v2.10.3.13] — split_enable case in set switch.
    // handleSplitEnableMessage at TCIServer.cs:3091-3127 [v2.10.3.13]:
    //   args.size() == 2 → set (rx, bool); args.size() == 1 → query.
    //   SplitFromCATorTCIcancelsQSPLIT at TCIServer.cs:3107 [v2.10.3.13] deferred to Phase 17.
    QString handleSplitEnableCommand(const QStringList& args);

    // From Thetis TCIServer.cs:5061 [v2.10.3.13] — mute case in set switch.
    // handleMute at TCIServer.cs:4051-4067 [v2.10.3.13]:
    //   set: 1-arg (bool) sets global mute (MUT + MUT2).
    QString handleMuteSetCommand(const QStringList& args);

    // From Thetis TCIServer.cs:5145 [v2.10.3.13] — mute case in 1-arg query switch.
    // handleMute(null, false) — queries MUT || MUT2 → sendMute(...).
    QString handleMuteQueryCommand();

    // From Thetis TCIServer.cs:5067 [v2.10.3.13] — rx_mute case in set switch.
    // handleMuteRX at TCIServer.cs:4069-4090 [v2.10.3.13]:
    //   args.size() == 2 → set (rx, bool); args.size() == 1 → query.
    QString handleRxMuteCommand(const QStringList& args);

    // ── DSP family handlers (Phase 9) ─────────────────────────────────────────
    // From Thetis TCIServer.cs:4950 [v2.10.3.13] — rx_nb_enable case in set switch.
    // handleRxNbEnable at TCIServer.cs:3192-3207 [v2.10.3.13] — 1-arg query, 2-arg set.
    QString handleRxNbEnableCommand(const QStringList& args);

    // From Thetis TCIServer.cs:4953 [v2.10.3.13] — rx_bin_enable case in set switch.
    // handleRxBinEnable at TCIServer.cs:3208-3223 [v2.10.3.13] — 1-arg query, 2-arg set.
    QString handleRxBinEnableCommand(const QStringList& args);

    // From Thetis TCIServer.cs:4956 [v2.10.3.13] — rx_apf_enable case in set switch.
    // handleRxApfEnable at TCIServer.cs:3224-3247 [v2.10.3.13] — 1-arg query, 2-arg set.
    // NOTE: Thetis gates on !IsSetupFormNull; Phase 9 stub omits that gate
    //       (SetupForm integration deferred to Phase 20).
    QString handleRxApfEnableCommand(const QStringList& args);

    // From Thetis TCIServer.cs:4959 [v2.10.3.13] — rx_nf_enable case in set switch.
    // handleRxNfEnable at TCIServer.cs:3249-3264 [v2.10.3.13] — 1-arg query, 2-arg set.
    QString handleRxNfEnableCommand(const QStringList& args);

    // From Thetis TCIServer.cs:5103 [v2.10.3.13] — rx_anf_enable case in set switch.
    // handleAnfEnable at TCIServer.cs:4521-4541 [v2.10.3.13] — 1-arg query, 2-arg set.
    QString handleRxAnfEnableCommand(const QStringList& args);

    // From Thetis TCIServer.cs:5097 [v2.10.3.13] — rx_nr_enable case in set switch.
    // handleNrEnable(args, false) at TCIServer.cs:4488-4519 [v2.10.3.13].
    // Basic form: 2-arg set (rx, bool), 1-arg query → sendNrEnable(rx, en, false, nr).
    QString handleRxNrEnableCommand(const QStringList& args);

    // From Thetis TCIServer.cs:5100 [v2.10.3.13] — rx_nr_enable_ex case in set switch.
    // handleNrEnable(args, true) at TCIServer.cs:4488-4519 [v2.10.3.13].
    // Extended form: 3-arg set (rx, bool, nr_index), 1-arg query → sendNrEnable(rx, en, true, nr).
    QString handleRxNrEnableExCommand(const QStringList& args);

    // From Thetis TCIServer.cs:5112 [v2.10.3.13] — agc_mode case in set switch.
    // handleAgcMode at TCIServer.cs:4658-4671 [v2.10.3.13] — 1-arg query, 2-arg set.
    // agcModeToTciMode / tciModeToAgcMode at TCIServer.cs:2192-2235 [v2.10.3.13]:
    //   FIXD→"off", LONG→"long", SLOW→"slow", FAST→"fast", CUSTOM→"custom", MED→"normal".
    //   Incoming aliases: "off"/"fixd"/"fixed" → "off"; "normal"/"med"/"medium" → "normal".
    //   Unknown input defaults to "normal" (AGCMode.MED) per Thetis default: return AGCMode.MED.
    QString handleAgcModeCommand(const QStringList& args);

    // From Thetis TCIServer.cs:5115 [v2.10.3.13] — agc_gain case in set switch.
    // handleAgcGain at TCIServer.cs:4673-4689 [v2.10.3.13] — 1-arg query, 2-arg set.
    // Gain clamped to [-20, 120] per TCIServer.cs:4686 [v2.10.3.13].
    QString handleAgcGainCommand(const QStringList& args);

    // From Thetis TCIServer.cs:4968 [v2.10.3.13] — sql_enable case in set switch.
    // handleSqlEnable at TCIServer.cs:3301-3316 [v2.10.3.13] — 1-arg query, 2-arg set.
    QString handleSqlEnableCommand(const QStringList& args);

    // From Thetis TCIServer.cs:4971 [v2.10.3.13] — sql_level case in set switch.
    // handleSqlLevel at TCIServer.cs:3317-3333 [v2.10.3.13] — 1-arg query, 2-arg set.
    // Level clamped to [-140, 0] per TCIServer.cs:3330 [v2.10.3.13].
    QString handleSqlLevelCommand(const QStringList& args);

    // From Thetis TCIServer.cs:4938 [v2.10.3.13] — rit_enable case in set switch.
    // handleRITEnableMessage at TCIServer.cs:3128-3143 [v2.10.3.13] — 1-arg query, 2-arg set.
    // RIT is GLOBAL; rx arg accepted but only gates on rx==0||rx==1 per Thetis.
    QString handleRitEnableCommand(const QStringList& args);

    // From Thetis TCIServer.cs:4944 [v2.10.3.13] — rit_offset case in set switch.
    // handleRITOffsetMessage at TCIServer.cs:3160-3175 [v2.10.3.13] — 1-arg query, 2-arg set.
    QString handleRitOffsetCommand(const QStringList& args);

    // From Thetis TCIServer.cs:4941 [v2.10.3.13] — xit_enable case in set switch.
    // handleXITEnableMessage at TCIServer.cs:3144-3159 [v2.10.3.13] — 1-arg query, 2-arg set.
    // XIT is GLOBAL; same rx-gating as RIT.
    QString handleXitEnableCommand(const QStringList& args);

    // From Thetis TCIServer.cs:4947 [v2.10.3.13] — xit_offset case in set switch.
    // handleXITOffsetMessage at TCIServer.cs:3176-3191 [v2.10.3.13] — 1-arg query, 2-arg set.
    QString handleXitOffsetCommand(const QStringList& args);

    // From Thetis TCIServer.cs:5109 [v2.10.3.13] — rx_balance case in set switch.
    // handleRxBalance at TCIServer.cs:4631-4656 [v2.10.3.13] — 2-arg query, 3-arg set.
    // F2 C-locale float. Balance clamped to [-40.0, 40.0] per TCIServer.cs:4650 [v2.10.3.13].
    // NOTE: Thetis internal pan = (40-bal)/0.8 transform at TCIServer.cs:4644-4652
    //       [v2.10.3.13] is NOT replicated in Phase 9 stub; NereusSDR stores F2 dB
    //       directly without the Thetis pan-slider calibration (deferred to Phase 20).
    QString handleRxBalanceCommand(const QStringList& args);

    // ── Phase 13: Bespoke _ex command handlers ───────────────────────────────
    // From Thetis TCIServer.cs:5010 [v2.10.3.13] — rx_enable case in set switch.
    // handleRXEnable at TCIServer.cs:4413-4450 [v2.10.3.13]:
    //   1-arg = query (rx → emit rx_enable:rx,bool;)
    //   2-arg = set (rx, bool).
    // rx==0 is always enabled in Thetis; rx==1 sets RX2Enabled.
    // NereusSDR: MOX-gating of query result deferred to Phase 17; stored directly.
    // sendRXEnable at TCIServer.cs:2279-2283 [v2.10.3.13]: "rx_enable:rx,bool;"
    QString handleRxEnableCommand(const QStringList& args);

    // From Thetis TCIServer.cs:5118 [v2.10.3.13] — rx_ctun_ex case in set switch.
    // handleCTUN at TCIServer.cs:4696-4710 [v2.10.3.13]:
    //   1-arg = query (rx → sendCTUN(rx, GetCTUN(rx+1)))
    //   2-arg = set (rx, bool → SetCTUN(rx+1, enable)).
    // sendCTUN at TCIServer.cs:4690-4694 [v2.10.3.13]: "rx_ctun_ex:rx,bool;"
    QString handleRxCtunExCommand(const QStringList& args);

    // From Thetis TCIServer.cs:5121 [v2.10.3.13] — tx_profile_ex case in set switch.
    // handleTXProfile at TCIServer.cs:4732-4748 [v2.10.3.13]:
    //   args.Length == 0 → query (get active TX profile name, sendTXProfile).
    //   args.Length == 1 → set (SafeTXProfileSet(args[0])).
    //   args.Length > 1 → ignored.
    // sendTXProfile at TCIServer.cs:4715-4720 [v2.10.3.13]: "tx_profile_ex:name;"
    // Set form: dispatched from handleSetCommand (args.size()==1 after colon split).
    // Query form: dispatched from handleQueryCommand (1-arg query switch: tmpArgs=[]).
    QString handleTxProfileExSetCommand(const QStringList& args);
    QString handleTxProfileExQueryCommand();

    // From Thetis TCIServer.cs:5187 [v2.10.3.13] — tx_profiles_ex case in 1-arg query switch.
    // handleTXProfiles at TCIServer.cs:4748-4752 [v2.10.3.13] → sendTXProfiles.
    // sendTXProfiles at TCIServer.cs:4721-4731 [v2.10.3.13]: CSV of profile names.
    QString handleTxProfilesExQueryCommand();

    // From Thetis TCIServer.cs:5124 [v2.10.3.13] — calibration_ex case in set switch.
    // handleCalibration at TCIServer.cs:4776-4782 [v2.10.3.13]:
    //   args.Length != 1 → return (ignores 0 or 2+ args).
    //   args.Length == 1 → parse rx, call CalibrationChanged(rx).
    // CalibrationChanged at TCIServer.cs:1152-1170 [v2.10.3.13]: queries 5 floats
    //   from radio model and emits sendCalibration(rx, meter, display, xvtr, 6m, txDisp).
    // sendCalibration at TCIServer.cs:4766-4775 [v2.10.3.13]: F6 C-locale per value.
    // Note: this is NOT a client-supplied 6-arg write; client sends rx only and the
    //   server pushes back the radio's calibration values (query-like semantics).
    QString handleCalibrationExCommand(const QStringList& args);

    // From Thetis TCIServer.cs:5190 [v2.10.3.13] — shutdown_ex case in 1-arg query switch.
    // handleShutdown at TCIServer.cs:4752-4765 [v2.10.3.13]: BeginInvoke → console.Close().
    // STUB: logs warning + returns empty. Actual shutdown wiring deferred to Phase 24+
    //   (maintainer-policy decision: TCI client must not be able to close the app).
    QString handleShutdownExCommand();

    // ── Phase 12: Spot + CW stubs ────────────────────────────────────────────
    // All return empty + log at lcTci info; real handlers in Phase 3J-2/3M-2.
    // From Thetis TCIServer.cs:5049 [v2.10.3.13] — spot case in set switch.
    QString handleSpotCommand(const QStringList& args);
    // From Thetis TCIServer.cs:5052 [v2.10.3.13] — spot_delete case in set switch.
    QString handleSpotDeleteCommand(const QStringList& args);
    // From Thetis TCIServer.cs:5082 [v2.10.3.13] — spot_simulate_click case in set switch.
    QString handleSpotSimulateClickCommand(const QStringList& args);
    // From Thetis TCIServer.cs:5184 [v2.10.3.13] — spot_clear case in 1-arg query switch.
    QString handleSpotClearCommand();
    // From Thetis TCIServer.cs:4989 [v2.10.3.13] — cw_macros_speed_up case in set switch.
    QString handleCwMacrosSpeedUpCommand(const QStringList& args);
    // From Thetis TCIServer.cs:4992 [v2.10.3.13] — cw_macros_speed_down case in set switch.
    QString handleCwMacrosSpeedDownCommand(const QStringList& args);
    // From Thetis TCIServer.cs:5001 [v2.10.3.13] — cw_msg case in set switch.
    QString handleCwMsgCommand(const QStringList& args);

    // ── IQ stream family handlers (Phase 11) ─────────────────────────────────
    // From Thetis TCIServer.cs:5016-5026 [v2.10.3.13] — set-switch IQ cases.
    // Plan corrections: "iq_stream" is not a real Thetis case (skipped);
    //   "iq_sample_rate" Thetis spelling is iq_samplerate (one word, line 5016).
    // IQSwap + AlwaysStreamIQ effect-sites deferred to Phase 18 (binary IQ
    // frame builder) and Phase 16/18 (wantsIQStream early-out) respectively.

    // From Thetis TCIServer.cs:5016 [v2.10.3.13] — iq_samplerate set case.
    // handleIQSampleRate at TCIServer.cs:5705-5722 [v2.10.3.13]:
    //   1-arg set (int). Thetis does NOT clamp to specific rates — it accepts any
    //   int, stores it, and echoes it back unchanged (the hardware-rate coupling
    //   code is commented out at TCIServer.cs:5712-5719 [v2.10.3.13]).
    //   Phase 11 faithful: parse int, store if valid, always echo current value.
    //   Query: TCIServer.cs:5175 [v2.10.3.13] — sendIQSampleRate(getPublishedIQSampleRate()).
    QString handleIqSampleRateCommand(const QStringList& args);
    // From Thetis TCIServer.cs:5175 [v2.10.3.13] — iq_samplerate query case.
    // sendIQSampleRate(getPublishedIQSampleRate()) → direct unicast response.
    QString handleIqSampleRateQueryCommand();

    // From Thetis TCIServer.cs:5022-5026 [v2.10.3.13] — iq_start/iq_stop cases.
    // dispatch through handleIQStart at TCIServer.cs:5797-5813 [v2.10.3.13].
    //
    // Phase 11 STUB: validate args parses an int receiver and return empty.
    // DEFERRED to Phase 18 (IQ binary stream pipeline): per-client subscription
    // tracking via m_iqStreamEnabled HashSet (Thetis TCIServer.cs:766).
    // AlwaysStreamIQ flag effect (Thetis TCIServer.cs:5401) lands at the same
    // time. Phase 11's placeholder rows for compat_always_stream_iq_on are
    // updated in this commit's matrix.csv to reflect Phase 18 deferral.
    QString handleIqStartStopCommand(const QStringList& args, bool enable);

    // ── Audio stream family handlers (Phase 10) ───────────────────────────────
    // From Thetis TCIServer.cs:5019-5041 [v2.10.3.13] — set-switch audio cases.
    // Corrections vs. plan: "audio_stream" → "audio_start"/"audio_stop";
    //   "audio_gain" does not exist in Thetis — the actual case is "volume";
    //   "line_out_*" do not have NereusSDR equivalents — skipped.

    // From Thetis TCIServer.cs:5019 [v2.10.3.13] — audio_samplerate set case.
    // handleAudioSampleRate at TCIServer.cs:5740-5795 [v2.10.3.13]:
    //   1-arg set. Accepts 8000/12000/24000/48000; ignores other values but still
    //   echoes current rate. Phase 10 simplified: no default-samples adjustment
    //   (m_audioStreamSamplesExplicitlySet tracking deferred to Phase 16).
    //   Query: TCIServer.cs:5178 [v2.10.3.13] — sendAudioSampleRate(m_audioSampleRate).
    QString handleAudioSampleRateCommand(const QStringList& args);
    // From Thetis TCIServer.cs:5178 [v2.10.3.13] — audio_samplerate query case.
    QString handleAudioSampleRateQueryCommand();

    // From Thetis TCIServer.cs:5034 [v2.10.3.13] — audio_stream_sample_type set case.
    // handleAudioStreamSampleType at TCIServer.cs:5908-5933 [v2.10.3.13]:
    //   1-arg set. Accepts "int16"/"int24"/"int32"/"float32"; defaults to float32
    //   for unknown values (Thetis default: case "float32": default:).
    //   No 1-arg query case in Thetis query switch — set-only.
    QString handleAudioStreamSampleTypeCommand(const QStringList& args);

    // From Thetis TCIServer.cs:5037 [v2.10.3.13] — audio_stream_channels set case.
    // handleAudioStreamChannels at TCIServer.cs:5935-5949 [v2.10.3.13]:
    //   1-arg set. Accepts 1 (mono) or 2 (stereo); ignores other values.
    //   No 1-arg query case in Thetis query switch — set-only.
    QString handleAudioStreamChannelsCommand(const QStringList& args);

    // From Thetis TCIServer.cs:5040 [v2.10.3.13] — audio_stream_samples set case.
    // handleAudioStreamSamples at TCIServer.cs:5951-5983 [v2.10.3.13]:
    //   1-arg set. Range [100..2048] — values outside range silently ignored and
    //   current value echoed. Phase 10: upper clamp returns 2048 in echo.
    //   No 1-arg query case in Thetis query switch — set-only.
    QString handleAudioStreamSamplesCommand(const QStringList& args);

    // From Thetis TCIServer.cs:5028-5032 [v2.10.3.13] — audio_start / audio_stop cases.
    // handleAudioStart at TCIServer.cs:5891-5906 [v2.10.3.13]:
    //   Manages per-receiver audio stream subscription (m_audioStreamEnabled).
    //   STUBBED in Phase 10 — no per-client subscription model yet.
    //   Phase 16 wires per-client subscription tracking and streaming callbacks.
    //   Returns empty; enqueues no notification.
    QString handleAudioStartCommand(const QStringList& args, bool enable);

    // From Thetis TCIServer.cs:5064 [v2.10.3.13] — volume set case.
    // handleVolume at TCIServer.cs:4150-4161 [v2.10.3.13]:
    //   1-arg set (double dB). Converts via dbToLinearVolume and writes AF.
    //   sendVolume at TCIServer.cs:2173-2179 [v2.10.3.13]: F1 C-locale; guards
    //   on -60 <= db <= 0 before sending.
    //   Query: TCIServer.cs:5148 [v2.10.3.13] — sendVolume(linearToDbVolume(AF)).
    QString handleVolumeCommand(const QStringList& args);

    // From Thetis TCIServer.cs:5148 [v2.10.3.13] — volume query case.
    QString handleVolumeQueryCommand();

    // From Thetis TCIServer.cs:5070 [v2.10.3.13] — mon_volume set case.
    // handleMONVolume at TCIServer.cs:4133-4148 [v2.10.3.13]:
    //   1-arg set (double dB). Converts via dbToLinearVolume and writes TXAF.
    //   sendMONVolume at TCIServer.cs:2180-2186 [v2.10.3.13]: F1 C-locale; same
    //   -60..0 guard as sendVolume.
    //   Query: TCIServer.cs:5154 [v2.10.3.13] — sendMONVolume(linearToDbVolume(TXAF)).
    QString handleMonVolumeCommand(const QStringList& args);

    // From Thetis TCIServer.cs:5154 [v2.10.3.13] — mon_volume query case.
    QString handleMonVolumeQueryCommand();

    // From Thetis TCIServer.cs:2363-2510 [v2.10.3.13] — sendInitialRadioState body.
    // Phase 4 Task 4.2 fills the body (up to 97 wire lines per Sweep D).
    // Each build<Foo>Line helper is a pure static function: takes value args,
    // returns a formatted line. Phase 6+ feeds real RadioModel state.
    QStringList buildInitialRadioStateLines() const;

    // ── Freq / IF / VFO helpers ───────────────────────────────────────────────
    // From Thetis TCIServer.cs:2334-2348 [v2.10.3.13] — sendDDS format string.
    // ddsFreq += GetDSPcwPitchShiftToZero(rx+1); //MW0LGE [2.9.0.7]
    static QString buildDdsLine(int rx, qint64 hz);
    // From Thetis TCIServer.cs:2096-2120 [v2.10.3.13] — sendIF format string.
    // offset += -GetDSPcwPitchShiftToZero(rx+1); //MW0LGE [2.9.0.7] note we invert with -
    static QString buildIfLine(int rx, int chan, qint64 offsetHz);
    // From Thetis TCIServer.cs:2061-2095 [v2.10.3.13] — sendVFO format string.
    static QString buildVfoLine(int rx, int chan, qint64 hz);
    // From Thetis TCIServer.cs:2246-2259 [v2.10.3.13] — sendTXFrequencyChanged.
    static QString buildTxFrequencyLine(qint64 hz);
    static QString buildTxFrequencyThetisLine(qint64 hz, const QString& band,
                                              bool rx2en, bool txvfob);

    // ── Mode / filter helpers ─────────────────────────────────────────────────
    // From Thetis TCIServer.cs:2136-2157 [v2.10.3.13] — sendMode format string.
    static QString buildModulationLine(int rx, const QString& modeUpper);
    // From Thetis TCIServer.cs:2349-2353 [v2.10.3.13] — sendFilterBand.
    // Adjacent sendDDS at TCIServer.cs:2344 carries: //MW0LGE [2.9.0.7]
    static QString buildRxFilterBandLine(int rx, int lowHz, int highHz);

    // ── RX-enable / NR / NB / BIN / ANF / APF / NF ───────────────────────────
    // From Thetis TCIServer.cs:2279-2283 [v2.10.3.13] — sendRXEnable.
    static QString buildRxEnableLine(int rx, bool en);
    // From Thetis TCIServer.cs:4472-4480 [v2.10.3.13] — sendNrEnable (non-extended).
    static QString buildRxNrEnableLine(int rx, bool en);
    // From Thetis TCIServer.cs:4472-4480 [v2.10.3.13] — sendNrEnable (extended).
    static QString buildRxNrEnableExLine(int rx, bool en, int nrIndex);
    // From Thetis TCIServer.cs:1901-1905 [v2.10.3.13] — sendRxNbEnable.
    static QString buildRxNbEnableLine(int rx, bool en);
    // From Thetis TCIServer.cs:1906-1910 [v2.10.3.13] — sendRxBinEnable.
    static QString buildRxBinEnableLine(int rx, bool en);
    // From Thetis TCIServer.cs:4482-4487 [v2.10.3.13] — sendAnfEnable.
    static QString buildRxAnfEnableLine(int rx, bool en);
    // From Thetis TCIServer.cs:1911-1915 [v2.10.3.13] — sendRxApfEnable.
    static QString buildRxApfEnableLine(int rx, bool en);
    // From Thetis TCIServer.cs:1916-1920 [v2.10.3.13] — sendRxNfEnable.
    static QString buildRxNfEnableLine(int rx, bool en);

    // ── Volume / balance helpers ──────────────────────────────────────────────
    // From Thetis TCIServer.cs:4563-4568 [v2.10.3.13] — sendRxVolume (F2 C-locale).
    static QString buildRxVolumeLine(int rx, int chan, double db);
    // From Thetis TCIServer.cs:2187-2191 [v2.10.3.13] — sendRxBalance (F2 C-locale).
    static QString buildRxBalanceLine(int rx, int chan, double bal);

    // ── AGC helpers ───────────────────────────────────────────────────────────
    // From Thetis TCIServer.cs:2236-2240 [v2.10.3.13] — sendAgcMode.
    static QString buildAgcModeLine(int rx, const QString& mode);
    // From Thetis TCIServer.cs:2241-2245 [v2.10.3.13] — sendAgcGain.
    static QString buildAgcGainLine(int rx, int gain);

    // ── CTUN / TX profile / calibration ──────────────────────────────────────
    // From Thetis TCIServer.cs:4690-4694 [v2.10.3.13] — sendCTUN (rx_ctun_ex suffix).
    static QString buildRxCtunExLine(int rx, bool en);
    // From Thetis TCIServer.cs:4721-4731 [v2.10.3.13] — sendTXProfiles.
    static QString buildTxProfilesExLine(const QStringList& names);
    // From Thetis TCIServer.cs:4715-4720 [v2.10.3.13] — sendTXProfile.
    static QString buildTxProfileExLine(const QString& active);
    // From Thetis TCIServer.cs:4766-4775 [v2.10.3.13] — sendCalibration (F6 C-locale).
    static QString buildCalibrationExLine(int rx, double meter, double display,
                                          double xvtr, double sixMeter,
                                          double txDisplay);

    // ── RIT / XIT helpers ─────────────────────────────────────────────────────
    // From Thetis TCIServer.cs:1881-1885 [v2.10.3.13] — sendRITEnable.
    static QString buildRitEnableLine(int rx, bool en);
    // From Thetis TCIServer.cs:1886-1890 [v2.10.3.13] — sendXITEnable.
    static QString buildXitEnableLine(int rx, bool en);
    // From Thetis TCIServer.cs:1891-1895 [v2.10.3.13] — sendRITOffset.
    static QString buildRitOffsetLine(int rx, int hz);
    // From Thetis TCIServer.cs:1896-1900 [v2.10.3.13] — sendXITOffset.
    static QString buildXitOffsetLine(int rx, int hz);

    // ── Lock helpers ──────────────────────────────────────────────────────────
    // From Thetis TCIServer.cs:1921-1925 [v2.10.3.13] — sendLock.
    static QString buildLockLine(int rx, bool locked);
    // From Thetis TCIServer.cs:1926-1930 [v2.10.3.13] — sendVFOLock.
    // From Thetis TCIServer.cs:2032-2049 [v2.10.3.13] — sendAllVFOLocks logic.
    // When rx2Enabled: emits vfo_lock:0,0; vfo_lock:1,0; vfo_lock:1,1 (3 lines).
    // When !rx2Enabled: emits vfo_lock:0,0; vfo_lock:0,1 (2 lines).
    static QStringList buildAllVfoLocksLines(bool rx2Enabled,
                                             bool vfoALock, bool vfoBLock);

    // ── SQL / DIGL / DIGU helpers ─────────────────────────────────────────────
    // From Thetis TCIServer.cs:1931-1935 [v2.10.3.13] — sendSqlEnable.
    static QString buildSqlEnableLine(int rx, bool en);
    // From Thetis TCIServer.cs:1936-1940 [v2.10.3.13] — sendSqlLevel.
    static QString buildSqlLevelLine(int rx, int level);
    // From Thetis TCIServer.cs:2051-2055 [v2.10.3.13] — sendDiglOffset.
    static QString buildDiglOffsetLine(int hz);
    // From Thetis TCIServer.cs:2056-2060 [v2.10.3.13] — sendDiguOffset.
    static QString buildDiguOffsetLine(int hz);

    // ── CW macro helpers ──────────────────────────────────────────────────────
    // From Thetis TCIServer.cs:1941-1944 [v2.10.3.13] — sendCwMacrosSpeed.
    static QString buildCwMacrosSpeedLine(int wpm);
    // From Thetis TCIServer.cs:1945-1948 [v2.10.3.13] — sendCwMacrosDelay.
    static QString buildCwMacrosDelayLine(int ms);
    // From Thetis TCIServer.cs:1949-1952 [v2.10.3.13] — sendCwKeyerSpeed.
    static QString buildCwKeyerSpeedLine(int wpm);

    // ── Split / TX-enable helpers ─────────────────────────────────────────────
    // From Thetis TCIServer.cs:1876-1880 [v2.10.3.13] — sendSplit.
    static QString buildSplitEnableLine(int rx, bool en);
    // From Thetis TCIServer.cs:2284-2288 [v2.10.3.13] — sendTXEnable.
    static QString buildTxEnableLine(int rx, bool en);

    // ── RX channel enable ─────────────────────────────────────────────────────
    // From Thetis TCIServer.cs:5887-5890 [v2.10.3.13] — sendRxChannelEnable.
    static QString buildRxChannelEnableLine(int rx, int chan, bool en);

    // ── MOX / TUNE helpers ────────────────────────────────────────────────────
    // From Thetis TCIServer.cs:2121-2131 [v2.10.3.13] — sendMOX (no ",tci" suffix).
    // Adjacent sendIF at TCIServer.cs:2116 carries: //MW0LGE [2.9.0.7] note we invert with -
    static QString buildTrxLine(int rx, bool mox);
    // From Thetis TCIServer.cs:2274-2278 [v2.10.3.13] — sendTune.
    static QString buildTuneLine(int rx, bool tuning);

    // ── IQ helpers ────────────────────────────────────────────────────────────
    // From Thetis TCIServer.cs:5814-5817 [v2.10.3.13] — sendIQStartStop (iq_stop form).
    static QString buildIqStopLine(int rx);
    // From Thetis TCIServer.cs:5364-5370 [v2.10.3.13] — sendIQSampleRate.
    static QString buildIqSampleRateLine(int sr);

    // ── Audio-stream helpers ──────────────────────────────────────────────────
    // From Thetis TCIServer.cs:5372-5375 [v2.10.3.13] — sendAudioSampleRate.
    static QString buildAudioSampleRateLine(int sr);
    // From Thetis TCIServer.cs:5377-5380 [v2.10.3.13] — sendAudioStreamSampleType.
    static QString buildAudioStreamSampleTypeLine(const QString& typeLower);
    // From Thetis TCIServer.cs:5382-5385 [v2.10.3.13] — sendAudioStreamChannels.
    static QString buildAudioStreamChannelsLine(int n);
    // From Thetis TCIServer.cs:5387-5390 [v2.10.3.13] — sendAudioStreamSamples.
    static QString buildAudioStreamSamplesLine(int n);
    // From Thetis TCIServer.cs:5392-5395 [v2.10.3.13] — sendTxStreamAudioBuffering.
    static QString buildTxStreamAudioBufferingLine(int ms);

    // ── Mute / volume / MON helpers ───────────────────────────────────────────
    // From Thetis TCIServer.cs:2158-2162 [v2.10.3.13] — sendMute.
    static QString buildMuteLine(bool muted);
    // From Thetis TCIServer.cs:2163-2167 [v2.10.3.13] — sendMuteRX.
    static QString buildRxMuteLine(int rx, bool muted);
    // From Thetis TCIServer.cs:2173-2179 [v2.10.3.13] — sendVolume (F1, clamp -60..0).
    static QString buildVolumeLine(double db);
    // From Thetis TCIServer.cs:2168-2172 [v2.10.3.13] — sendMONEnable.
    static QString buildMonEnableLine(bool en);
    // From Thetis TCIServer.cs:2180-2186 [v2.10.3.13] — sendMONVolume (F1, clamp -60..0).
    static QString buildMonVolumeLine(double db);

    // ── Start / stop ──────────────────────────────────────────────────────────
    // From Thetis TCIServer.cs:1868-1875 [v2.10.3.13] — sendStartStop.
    static QString buildStartStopLine(bool powerOn);

    QObject* m_radio{nullptr};

    // Notification queue drained by TciServer after each handleCommand call.
    //
    // Thread-safety contract: ALL writes to m_pendingNotifications MUST happen
    // on the TciProtocol's owning thread (currently the main thread). All reads
    // (hasPendingNotification + takePendingNotification) come from the same
    // thread via the 5ms drain timer in TciServer.
    //
    // If a future phase adds cross-thread emitters (e.g. a RadioModel slot
    // auto-queued from a worker thread to a TciProtocol slot), that slot writes
    // m_pendingNotifications and would need either:
    //   (a) Qt::QueuedConnection routing to TciProtocol's owning thread (recommended), OR
    //   (b) a QMutex around all reads and writes here.
    // See m_vfoCoalescer for the QMutex pattern if option (b) is needed.
    //
    // Phase 26 review finding #9: pinned explicitly so the contract is visible
    // at the declaration site and does not silently break when new signals are
    // wired from worker threads in Phase 24+.
    QStringList m_pendingNotifications;
    // Phase 15: coalescer for rapid VFO updates (Layer 3 of Thetis 3-layer
    // throttle at TCIServer.cs:1722-1727 [v2.10.3.13]). Layers 1+2 subsumed
    // by Qt event loop + 5ms TciServer drain timer.
    TciVfoCoalescer m_vfoCoalescer;
    int m_setDispatchCount{0};
    int m_queryDispatchCount{0};
};

} // namespace NereusSDR
