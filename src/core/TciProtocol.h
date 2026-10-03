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
//   2026-09-23 - R3 receiver audio plan, Task 4 (R-R3-42, R-R3-25) by
//                J.J. Boyd (KG4VCF): remote-window mode (receive-only init
//                burst, transmit refused, vfo/modulation answer with the
//                value the slice holds). AI-assisted transformation via
//                Anthropic Claude Code.
//   2026-09-24 - R-R3-48 / R-R3-25: setStationReceiveOnly() and
//                transmitRefused(). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 3): buildTxProfilesExLine and
//                buildTxProfileExLine public for TciServer's remote-window
//                profile broadcast. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-25 - Receiver and transmit gaps plan, Task 12 (R-R3-49) by
//                J.J. Boyd (KG4VCF): if goes out with each VFO and centre
//                change, dds carries the centre, one if builder for the
//                init burst and the live path, and each drained line
//                carries the update gate of the event that queued it.
//                AI-assisted transformation via Anthropic Claude Code.
//   2026-09-25 - iPhone app Task 73 (R-IOS-02, ruling 5.13): a slice write
//                gate; a per-receiver set command naming a slice the gate
//                refuses changes nothing and is answered as its query.
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 35 (R-IOS-13, ruling 8.14): a remote
//                window that forwards transmit to its Core
//                (setRemoteTransmitForwarded): its init burst no longer says
//                receive-only, and TciServer answers a trx set command with
//                the Core's verdict. NereusSDR-original. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Desktop-host receiver-to-owned-slice mapping.
//                NereusSDR-original, AI-assisted via OpenAI Codex.
//   2026-09-29 - The three second-receiver VFO options (copy VFO B to
//                VFO A, forget VFO B, use RX1 VFO A for RX2 VFO A) act as
//                Thetis's do; their defaults are named once here.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Level Cal: calibration_ex carries the meter and display
//                calibration and goes to apps when either changes. J.J.
//                Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "TciUpdateGap.h"
#include <functional>

#include "TciVfoCoalescer.h"

#include <QHash>
#include <QList>

#include <optional>

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
// | TciRateLimitMs                       | int    | 100     | Update gap per app [0..1000] (TciUpdateGap)
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
// | TciForgetRx2VfoBOnDisconnect         | bool   | False   | RX2 VFO option (kTciForgetRx2VfobDefault)
// | TciUseRx1VfoaForRx2Vfoa              | bool   | False   | RX2 VFO option (kTciUseRx1VfoaForRx2VfoaDefault)
// | TciCopyRx2VfobToVfoa                 | bool   | True    | RX2 VFO option (kTciCopyRx2VfobToVfoaDefault)
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

// The second-receiver VFO options' defaults. Every reader (TciProtocol, the
// Setup pages, StationTciModel) takes its fallback from these, so each
// default is changed in exactly one place.
//
// From Thetis setup.cs:381-382 [v2.10.3.15]:
//   // some default tci server states MW0LGE_21k9d
//   chkCopyRX2VFObToVFOa.Checked = true;
//   chkUseRX1vfoaForRX2vfoa.Checked = true;
// chkForgetRX2VfoBVFOinfo is unchecked in the designer and only enabled
// while chkCopyRX2VFObToVFOa is checked (setup.cs:22568-22572 [v2.10.3.15]).
//
// Copy follows Thetis (on) and Forget follows Thetis (off); JJ's ruling of
// 2026-09-29 settles all three.
inline constexpr bool kTciCopyRx2VfobToVfoaDefault    = true;
inline constexpr bool kTciForgetRx2VfobDefault        = false;
// DIVERGENCE (JJ's ruling, 2026-09-29): Thetis defaults Use RX1 VFO A for
// RX2 VFO A ON, from Thetis setup.cs:381-382 [v2.10.3.15]:
//   // some default tci server states MW0LGE_21k9d
//   chkUseRX1vfoaForRX2vfoa.Checked = true;
// NereusSDR defaults it OFF so existing TCI client output is unchanged.
inline constexpr bool kTciUseRx1VfoaForRx2VfoaDefault = false;

class SliceModel;
class TciProtocol : public QObject {
    Q_OBJECT
public:
    explicit TciProtocol(QObject* radio = nullptr, QObject* parent = nullptr);
    ~TciProtocol() override = default;

    // Dispatch a single TCI command line. Returns the synchronous response
    // (empty for commands with no reply, or for the silent-error invariant
    // — unknown commands produce zero outbound traffic per design doc §4.1).
    QString handleCommand(const QString& command);

    // Level Cal: the calibration_ex line for receiver `rx` (0 or 1) with
    // the radio's calibration now, Thetis CalibrationChanged
    // (TCIServer.cs:1160-1176 [v2.10.3.15]). TciServer broadcasts it when
    // the calibration changes; the client query sends it too.
    QString calibrationExLineFor(int rx) const;

    // Notification queue — drained by TciServer after each handleCommand.
    bool hasPendingNotification() const;
    QString takePendingNotification();

    // Drain coalesced VFO updates into the pending notification queue.
    // Called by TciServer from the 5ms drain timer; ensures rapid VFO bursts
    // collapse to ≤ 1 frame per (rx, chan) per drain tick.
    // From Thetis TCIServer.cs:1722-1727 [v2.10.3.13] — outbound-coalesced map.
    void drainCoalescedNotifications();

    // One queued line and the update gate of the event that queued it
    // (Task 12, R-R3-49). gate is empty for a line queued without one:
    // one-shot events, and the vfo answer to an app's own vfo set command,
    // which TciUpdateGap sorts by command.
    struct PendingLine {
        QString frame;
        std::optional<TciUpdateGap::Gate> gate;
    };

    // As takePendingNotification, with the line's gate. TciServer uses this
    // so an if line reaches the gate its event named, never one inferred
    // from where it sits in the tick (rereview of the fix wave, N2).
    PendingLine takePendingLine();

    // ── Phase 3J-1 closeout (2026-05-22): local state-change broadcast ──────
    //
    // Push a TCI frame produced by the LOCAL operator (UI tuning, mode/filter
    // change, mute toggle, etc.) into the outbound notification queue.  The
    // 5ms drain timer in TciServer then broadcasts it to all connected
    // clients.
    //
    // Mirrors Thetis TCIServer.cs:6730-6790 [v2.10.3.15]: TCIServer subscribes
    // to ~40 Console events (FilterChangedHandlers, RX2EnabledChangedHandlers,
    // VfoALockChangedHandlers, NRChangedHandlers, etc.) and routes each to an
    // OnXxxChanged delegate that calls sendXxx (which enqueues the wire frame).
    // Without this path, NereusSDR TCI clients only see state set BY a TCI
    // client (the SET path in handleCommand wires the same coalescer + queue);
    // local UI tunes / mode changes never propagate -- bench bug 2026-05-22.
    //
    // enqueueLocalBroadcast:    push directly to m_pendingNotifications.  Use
    //                           for one-shot events (mode, filter, AGC, etc.).
    // enqueueLocalBroadcastVfo: route through m_vfoCoalescer for rapid-fire
    //                           VFO updates that need Layer-3 dedup (rotary
    //                           encoder spin can fire dozens of frequency
    //                           changes per second).  Emits the vfo:* +
    //                           dds:* + tx_frequency:* triplet that
    //                           sendInitialRadioState bundles.
    //
    // isTxBound says whether THIS receiver is the one currently driving the
    // transmitter, and only it emits the tx_frequency pair. Codex review
    // round 6, PR #293: the old test was `rxIndex == 0`, with a comment
    // saying the caller would decide once 3F multi-pan landed the real TXFreq
    // logic. It has landed, TxSliceArbiter can bind TX to any slice, and
    // until now tuning Slice A still advertised A as the transmit frequency
    // while the transmitter sat on B, and tuning B advertised nothing.
    // TciProtocol has no RadioModel handle by design, so the caller answers
    // it, exactly as that comment anticipated.
    //
    // Task 12 (R-R3-49): a VFO change sends if then vfo for each channel, as
    // Thetis's VFOdata thread does for a VFO event (TCIServer.cs:1396-1398
    // [v2.10.3.15]); it no longer sends dds, which belongs to the centre
    // event below. The if offset is read when the queue drains, so it names
    // the offset the slice settled on, not the one it passed through.
    void enqueueLocalBroadcast(const QString& frame);
    // R-R3-49 (parity Task 3): the tx_profiles_ex / tx_profile_ex lines,
    // public so TciServer's remote-window profile broadcast writes the same
    // frames. From Thetis TCIServer.cs:4721-4731 / 4715-4720 [v2.10.3.13]
    // (sendTXProfiles, sendTXProfile).
    static QString buildTxProfilesExLine(const QStringList& names);
    static QString buildTxProfileExLine(const QString& active);
    void enqueueLocalBroadcastVfo(int rxIndex, qint64 hz, bool isTxBound);

    /// A centre change for receiver rxIndex: dds then if:rx,0, both on the
    /// centre gate, as Thetis's VFOdata thread sends a centre event
    /// (TCIServer.cs:1378-1382 [v2.10.3.15]). Both values are read when the
    /// queue drains, and the pair is dropped then if the centre is the one
    /// last sent, so a retune that leaves the pan where it was sends no dds.
    ///
    /// Thetis sends the if only with CTUN on (OnCentreFrequencyChanged,
    /// TCIServer.cs:7364-7376 [v2.10.3.15]):
    ///   //only want to send IF with this if CTUN is enabled
    /// [original inline comment from TCIServer.cs:7366]. NereusSDR has no model-level
    /// CTUN for a local window, and with the pan following the VFO the
    /// offset is 0, so the if goes every time and is always true.
    void enqueueLocalBroadcastCentre(int rxIndex);

    /// Emit the untagged tx_frequency pair on its own.
    ///
    /// Neither frame carries a receiver index, so this is legal for any
    /// slice holding the transmitter, including one past the advertised
    /// trx_count. enqueueLocalBroadcastVfo calls it when isTxBound; the TX
    /// handoff path calls it directly, because a handoff changes the
    /// transmit frequency without anyone having tuned anything.
    void enqueueLocalBroadcastTxFrequency(qint64 hz);

    /// How many receivers this server exposes on the wire.
    ///
    /// This is the number sent as `trx_count:N;` in the init burst, and it
    /// is the contract a client allocates its state from. Locked at 2 for
    /// strict Thetis client compatibility (Thetis TCIServer.cs:2530
    /// [v2.10.3.13]); Phase 3J-1 design doc §1.2 records it as a locked
    /// decision, with Slice C/D/E internal and a future opt-in to 4 out of
    /// scope.
    ///
    /// Named rather than repeated, because it was a bare literal in the init
    /// burst that nothing else could read: the local-broadcast path then
    /// wired every slice it found, including C/D/E as receivers 2 to 4,
    /// emitting unsolicited frames outside the count it had just negotiated
    /// (Codex review round 6, PR #293). Raising this alone is NOT enough to
    /// expose more receivers; the init burst carries per-receiver state that
    /// would have to be widened with it.
    static constexpr int kExposedReceiverCount = 2;

    // The second-receiver VFO options, read from AppSettings when used,
    // each falling back to its kTci...Default above.
    static bool copyRx2VfobToVfoaSetting();
    static bool forgetRx2VfobSetting();
    static bool useRx1VfoaForRx2VfoaSetting();

    // RX2 is on (Thetis console.RX2Enabled): receiver 1 has a slice. With a
    // receiver map that is the map's receiver 1; without one (the Core's
    // station server, a plain desktop) it is slice 1, since trx:N is slice N.
    // A test radio that is not a RadioModel answers through rx2Enabled().
    bool rx2EnabledNow() const;
    // The receiver whose slice vfo:rx,chan reads and writes, and the slice
    // VFO (chan) it uses: with RX2 on, channel 1 of either receiver is RX2's
    // VFO B, receiver 1's slice frequency; with Use RX1 VFO A for RX2 VFO A,
    // receiver 1 channel 0 is receiver 0's VFO A.
    struct VfoTarget { int rx; int chan; };
    VfoTarget vfoTarget(int rx, int chan) const;

    // Build the post-connect init burst. Stub returns empty list in Phase 3;
    // Phase 4 Task 4.1 replaces with the 8-line wrapper from
    // Thetis TCIServer.cs:2512-2552 [v2.10.3.13].
    QStringList buildInitBurst() const;

    // R-R3-42 / R-R3-25: this protocol serves a remote window, whose
    // receivers belong to a Core that does not transmit for it. The init
    // burst then says receive_only:true and tx_enable false on both
    // receivers, trx set commands never touch MOX and answer trx:N,false,
    // and vfo / modulation set commands answer with the value the slice
    // holds after the write (the Core's accepted value arrives later
    // through the slice's own change broadcast). Off by default: local
    // operation is unchanged.
    void setRemoteWindow(bool remote) { m_remoteWindow = remote; }
    bool remoteWindow() const { return m_remoteWindow; }

    // R-R3-48 / R-R3-25: the Core's station server transmits for no app
    // until remote transmit: receive_only:true, tx_enable false, and trx
    // set commands answer trx:N,false without touching MOX. Unlike a
    // remote window, vfo and modulation act on the Core's own slices.
    void setStationReceiveOnly(bool receiveOnly) { m_stationReceiveOnly = receiveOnly; }
    bool stationReceiveOnly() const { return m_stationReceiveOnly; }
    // iPhone app plan Task 35 (ruling 8.14): a remote window whose
    // TciServer forwards an app's transmit to the Core
    // (TciServer::setRemoteTransmit). A trx set command is then answered by
    // the server with the Core's verdict, never here, and the window is not
    // receive-only (the Core decides each key). Off by default.
    void setRemoteTransmitForwarded(bool forwarded) { m_remoteTransmitForwarded = forwarded; }
    bool remoteTransmitForwarded() const { return m_remoteWindow && m_remoteTransmitForwarded; }
    // Transmit is refused (a remote window that does not forward it, or the
    // station server).
    bool transmitRefused() const
    {
        return (m_remoteWindow && !m_remoteTransmitForwarded) || m_stationReceiveOnly;
    }

    // iPhone app Task 73 (the several-devices design, ruling 5.13): the
    // Core's own server reads every slice (trx:N is slice N) and changes
    // only the station device's own. With a gate set, a per-receiver set
    // command (vfo, modulation, rx_filter_band, the DSP switches, ...)
    // naming a slice the gate refuses changes nothing and is answered with
    // the value the slice holds, as its query would be. No gate (a desktop
    // on its own): every slice, as before.
    using SliceWriteGate = std::function<bool(int sliceId)>;
    void setSliceWriteGate(SliceWriteGate gate) { m_sliceWriteGate = std::move(gate); }
    // Desktop hosting: map TCI receiver N to the station device's Nth
    // owned slice. Empty restores the ordinary identity mapping.
    void setReceiverSliceMap(std::function<int(int)> map) { m_receiverSliceMap = std::move(map); }
    int receiverSlice(int receiver) const;
    int sliceReceiver(int sliceId) const;
    // M1 (R-R3-48 / R-R3-25): `command` (one TCI command, with or without
    // its ';') changes transmit configuration an app may not change on the
    // receive-only station server: tx_profile_ex, xit_enable and xit_offset
    // set commands. Queries are not changes.
    static bool isTransmitSettingChange(const QString& command);

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

    // From Thetis TCIServer.cs:2260-2279 [v2.10.3.15] -- agcModeToTciMode.
    // Maps RadioModel::agcMode enum-style names ("OFF" / "LONG" / "SLOW" /
    // "MED" / "FAST" / "CUSTOM" / "FIXD") to the lowercase TCI wire token
    // Thetis sends ("off" / "long" / "slow" / "normal" / "fast" /
    // "custom").  Public so the TciServer broadcast handler for
    // SliceModel::agcModeChanged can match the init burst formatting.
    static QString tciAgcModeForWire(const QString& enumName);

private:
    SliceModel* mappedSlice(int receiver) const;
    // From Thetis TCIServer.cs:4924-5128 [v2.10.3.13] — 60-case set-command switch.
    // Phase 5+ adds individual cases via the matrix runner.
    QString handleSetCommand(const QString& name, const QStringList& args);

    // From Thetis TCIServer.cs:5134-5197 [v2.10.3.13] — 21-case query-command switch.
    // Phase 5+ adds individual cases via the matrix runner.
    QString handleQueryCommand(const QString& name);

    // ── VFO family handlers (Phase 6) ─────────────────────────────────────────
    // From Thetis TCIServer.cs:3724-3833 [v2.10.3.13] — handleVFOMessage.
    // Dispatches set (3 args) or query (2 args) by args.size().
    // With RX2 on and Use RX1 VFO A for RX2 VFO A set, receiver 1 channel 0
    // is receiver 0's VFO for set and query (TCIServer.cs:3858-3967
    // [v2.10.3.15]).
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
    // handleRXEnable at TCIServer.cs:4595-4629 [v2.10.3.15]:
    //   1-arg = query: rx 0 -> !MOX, rx 1 -> RX2Enabled && !MOX.
    //   2-arg = set (rx, bool): rx==1 sets RX2Enabled; nothing is sent.
    // NereusSDR: RX2 is receiver 1's slice, which TCI does not open or
    // close, so a set changes nothing.
    // sendRXEnable at TCIServer.cs:2279-2283 [v2.10.3.13]: "rx_enable:rx,bool;"
    QString handleRxEnableCommand(const QStringList& args);

    // From Thetis TCIServer.cs:5456-5458 [v2.10.3.15] — rx_channel_enable case;
    // handleRxChannelEnable at TCIServer.cs:6252-6291 [v2.10.3.15]:
    //   2-arg = query (rx, chan), 3-arg = set (rx, chan, bool).
    // The reply goes to the asking app only (sendTextFrame on its listener).
    QString handleRxChannelEnableCommand(const QStringList& args);
    // The value a rx_channel_enable query answers for rx, chan.
    bool rxChannelEnabledNow(int rx, int chan) const;

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

    // The offset an if line carries: vfo - centre + RIT. Task 12 (R-R3-49).
    //
    // From Thetis TCIServer.cs:7268 and :7293 [v2.10.3.15], the live path:
    // offsetHz = (int)-offset, where offset is RXOsc; and console.cs:31409
    // and :31457-31458 [v2.10.3.15]:
    //   double rx1_osc = Math.Round(-(freq - CentreFrequency) * 1.0e6);
    //   if (chkRIT.Checked && bRitOk)
    //       rx1_osc -= (int)udRIT.Value;
    // so -RXOsc = (vfo - centre) + RIT.
    //
    // Divergence: Thetis's init burst calls sendIF with no offset, which
    // reads +RXOsc (TCIServer.cs:2136-2152 [v2.10.3.15]), the opposite sign
    // of its live path. One builder serves both here, with the live sign,
    // the one that keeps vfo = dds + if.
    //
    // No DIG click-tune offset: Thetis's RXOsc never carries it, while
    // NereusSDR's composed WDSP shift does.
    //
    // No CW pitch term. Thetis's sendIF ends with
    //   offset += -consoleThreadSafe.GetDSPcwPitchShiftToZero(rx + 1); //MW0LGE [2.9.0.7] note we invert with -
    // [original inline comment from TCIServer.cs:2154]
    // because in CW it tunes the DDC off the VFO by the pitch
    // (console.cs:31826-31838 [v2.10.3.15]). NereusSDR does not, so the
    // term would break vfo = dds + if here. Revisit with CW transmit (3M-2).
    static qint64 ifOffsetHz(qint64 vfoHz, qint64 centreHz, int ritHz);

    // The one if builder, for the init burst and the live path alike: reads
    // receiver rx's vfo, centre and RIT and formats the line.
    QString buildIfLineForRx(int rx, int chan) const;
    // As buildIfLineForRx, labelled rx but read from receiver sourceRx: the
    // if:1,0 that Use RX1 VFO A for RX2 VFO A sends for receiver 0's VFO.
    QString buildIfLineFrom(int labelRx, int chan, int sourceRx) const;

    // Receiver rx's dds line, its centre read from the radio. The init burst
    // uses it; the drain reads the same readDdsHz so it can record what it
    // sent (m_lastBroadcastDdsHz).
    QString buildDdsLineForRx(int rx) const;
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
    // Upstream tags preserved: //MW0LGE (from cited TCIServer.cs:4482) [v2.10.3.15]
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
    // From Thetis TCIServer.cs:4715-4720 [v2.10.3.13] — sendTXProfile.
    // buildTxProfilesExLine / buildTxProfileExLine: public (R-R3-49 parity
    // Task 3, TciServer's remote-window profile broadcast).
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
    // Upstream tags preserved: //MW0LGE (from cited TCIServer.cs:2154) [v2.10.3.15]
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
    // Lines drained from the coalescer, each with its event's gate (Task
    // 12). Taken after m_pendingNotifications, the order the one list had.
    QList<PendingLine> m_pendingDrained;
    // The centre the drain last sent as dds per receiver, so a centre event
    // that moved nothing sends nothing. Written by the drain only: the drain
    // reaches every app, while an init burst reaches one new app, and
    // letting the burst write here hid a pending move from every app already
    // connected (whole-branch review M3, R-R3-49). Thetis keeps the same
    // state per socket, so one app's connect cannot touch another's.
    QHash<int, qint64> m_lastBroadcastDdsHz;
    qint64 readDdsHz(int rx) const;
    qint64 readVfoHzForRx(int rx, int chan) const;
    // Phase 15: coalescer for rapid VFO updates (Layer 3 of Thetis 3-layer
    // throttle at TCIServer.cs:1722-1727 [v2.10.3.13]). Layer 1, the per-app
    // update gap, runs after it in TciServer (TciUpdateGap, Task 10);
    // Layer 2 is subsumed by it. See TciVfoCoalescer.h.
    TciVfoCoalescer m_vfoCoalescer;
    // The receiver an if key queued by the VFO path is read from, when it
    // is not the receiver the key names (Use RX1 VFO A for RX2 VFO A).
    QHash<QString, int> m_ifSourceReceiver;
    int m_setDispatchCount{0};
    int m_queryDispatchCount{0};
    bool m_remoteWindow{false};
    bool m_remoteTransmitForwarded{false};   // Task 35
    bool m_stationReceiveOnly{false};
    SliceWriteGate m_sliceWriteGate;
    std::function<int(int)> m_receiverSliceMap;
};

} // namespace NereusSDR
