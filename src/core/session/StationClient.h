// 2026-10-01: Authenticated Core address inventory and reconnect learning.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex. NereusSDR-original.

#pragma once
// =================================================================
// src/core/session/StationClient.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 18.
//
// The GUI half of the wss session: the object that makes a RadioModel
// which never calls connectToRadio() behave as though it had.
//
// ── WHAT IT DOES, IN ORDER ───────────────────────────────────────────────
//
//   1. Opens wss:// to the station and PINS the certificate fingerprint it
//      was given out of band (parent design section 10.5: "the daemon
//      generates a self-signed certificate on first run and the client
//      pins its fingerprint, displayed at pairing time alongside the
//      token"). A self-signed certificate produces SSL errors by
//      definition; this class ignores exactly the errors that are
//      explained by self-signing, and only after the fingerprint matches.
//   2. Reads the station's Hello and applies section 7.0's version policy
//      from its own side: refuse on major mismatch naming BOTH versions,
//      negotiate down on minor.
//   3. Compares the station's AppSettings schema version against its own,
//      BY NAME -- both sides read the value stored under the literal key
//      "SettingsSchemaVersion" in their own store. Skew is reported, not
//      refused: that version governs the shape of each side's own local
//      settings file, not the wire contract.
//   4. Sends its Hello and then its token.
//   5. On Capabilities, writes station identity, the EFFECTIVE slice limit
//      and userDdcCount into RadioModel and drives it to Connected
//      (RadioModel::applyStationCapabilities). This is the step that makes
//      task 3's storage-backed isConnected() true, unpins maxSlices() from
//      its disconnected default of 1, and wakes the GUI.
//   6. On the settings snapshot, feeds SettingsProxy and marks it ready.
//   7. On the mirror burst, builds client-side objects and applies their
//      state; on the snapshot-complete marker, starts forwarding local
//      changes back.
//
// ── BOTH DIRECTIONS OF THE PROPERTY MIRROR, AND WHY THEY DIFFER ──────────
//
// INBOUND (station to here) is unconditional. The daemon owns the radio,
// so whatever it reports is true by definition and must land, including on
// properties with no Q_PROPERTY WRITE. MirrorPolicy is deliberately NOT
// consulted on this path: that table answers "may a remote GUI write this
// to the daemon", which is the opposite question. Applying an inbound
// value goes, in order:
//
//   1. MirrorSchema::write() when the property has a WRITE accessor. This
//      is the overwhelming majority -- 24 of 148 mirrored properties lack
//      one (design addendum section 3).
//   2. The model's own Q_INVOKABLE applyMirroredValue hook, but ONLY for
//      an explicit allowlist of (class, property) pairs whose hook is a
//      genuine STATE APPLY. Exactly one today: SliceModel::
//      signalStrengthDbm, whose hook calls a plain setter task 12 added
//      for precisely this path.
//
//      The allowlist exists because that hook is the DAEMON's inbound
//      path ("a peer is asking this model to do something"), and some
//      implementations of it are COMMAND SENDERS. TunerModel answers
//      isOperate / isBypass / antennaA by forwarding to a bound
//      TgxlConnection. Consulting it here fed a station STATE REPORT into
//      a command sender, which inverts the link; it was inert only
//      because a remote client has no TgxlConnection, and binding one
//      would have turned every inbound tuner delta into an outbound
//      tuner command. Those setters also no-op with no connection while
//      the hook still reports success, so the properties reported as
//      applied, changed nothing, and never reached unappliedProperties().
//      The hook itself is deliberately left alone -- see the .cpp for why
//      that behaviour is Task 8's tested, documented choice.
//   3. A small client-side adapter for the handful of properties whose
//      only legitimate CLIENT-side writer is this class, but whose only
//      legitimate DAEMON-side writer is an arbiter that must not be
//      bypassed: SliceModel::active and SliceModel::txSlice. Their
//      applyMirroredValue refusals are correct on the daemon (a remote
//      peer must go through setActiveSliceById / TxSliceArbiter) and
//      wrong here, where the arbiter has already spoken and this is
//      simply its answer arriving.
//
// Anything none of the three can apply is counted and logged ONCE per
// (class, property) rather than per delta, so a bench session gets one
// line naming a real gap instead of a flood. unappliedProperties() below
// exposes the set.
//
// OUTBOUND (here to station) IS MirrorPolicy-gated, because it is the
// direction MirrorPolicy describes: only Bidirectional properties are
// forwarded, and everything else the operator's own GUI happens to move
// locally is dropped rather than argued about with the station.
//
// ── THE ECHO GUARD ───────────────────────────────────────────────────────
//
// Applying an inbound value calls a real setter, which emits a real
// NOTIFY, which the outbound watcher would forward straight back. The
// daemon has StateMirror::m_applying for the mirror-image problem; this
// class has m_applyingInbound, checked at the top of the outbound
// observer, before it asks anything else. It works for the same reason the
// daemon's does and no other: observer and applier are on ONE thread, so
// the NOTIFY is delivered synchronously, inside the guarded region. See
// the threading note below.
//
// ── THREADING ────────────────────────────────────────────────────────────
//
// This object, its transport, its StateMirror, the RadioModel it drives
// and every SliceModel under it live on ONE thread. Same invariant, same
// reasons, as StationServer's (see its header). The echo guard above is
// one of the three things that silently stops working if that is ever
// violated.
//
// ── ORDERING PRECONDITIONS THE CALLER OWNS ───────────────────────────────
//
// Two, both inherited from task 15 and both invisible from inside this
// class, so the constructor checks what it can and this comment records
// the rest:
//
//   - **Construct this AFTER CoreInit::initialize().** AppSettings::load()
//     bulk-populates and cannot leak, but the schema migrations
//     (AppSettings.cpp) go through setValue(), so installing the remote
//     backend before them would push this machine's own migrated keys up
//     to the station. The constructor warns if the schema-version key is
//     absent, which is the observable trace of migrations not having run.
//   - **SettingsProxy::ready() must not be true before the models are
//     constructed.** SliceModel, NotchModel, FilterPresetStore and
//     TciServer all do contains()-then-seed against Station-classified
//     prefixes in their constructors, and the ONLY thing stopping them
//     writing ship defaults into the station's store is that writes are
//     dropped while not ready. This class never sets ready() before the
//     handshake completes, which is necessarily after the RadioModel it
//     was handed already existed.
//
// ── LINK LOSS AND RECONNECT (Task 19) ────────────────────────────────────
//
// Nothing before this task defined what happens to a mirror of objects
// that no longer exist. Parent design section 13 ("Error handling and
// reconnect"): "The client retains last-known state, indicates staleness
// rather than showing stale values as live, and reconnects with
// exponential backoff."
//
//   - endSession() (the one place a session ends, unchanged in that
//     respect) now ALSO tears down the client-side mirror registry
//     (m_objects), the outbound watcher (m_outboundMirror->unwatchAll())
//     and its coalescer (m_outboundCoalescer.clear()) -- see endSession()
//     for why the coalescer clear is load-bearing and not just hygiene.
//     RadioModel's own state (SliceModel property values) is left exactly
//     where the last inbound delta set it: retained, not reset, per
//     section 13. isStale() is what a caller checks before trusting it.
//   - A reconnect ADOPTS the retained SliceModel objects under the
//     station's ids (StationClient::resolveOrCreate(), unchanged by this
//     task) rather than recreating them, so a GUI holding a raw pointer to
//     one survives a reconnect with no rebinding.
//   - ...and, whole-branch review Important 1, REAPS the ones the station
//     did not name. Adoption alone covered only the ids the station
//     still has, so a daemon that came back with fewer slices left the
//     surplus on screen forever: unwatched, unmirrored, and silently
//     swallowing every edit made to it. reconcileSlicesAgainstStation()
//     closes that, and runs at the SnapshotComplete marker specifically
//     -- see its own comment in the .cpp for why no earlier point can
//     tell "the station does not have this" apart from "it has not
//     arrived yet".
//   - sessionEpoch() bumps on every attachTransport(), including the
//     first, so a caller that captured it before an asynchronous
//     operation can tell whether the session it was about is still
//     current.
//   - Automatic reconnect is owned by an OWNED, CANCELLABLE, single-shot
//     QTimer (m_reconnectTimer) with a latched URL/token/fingerprint --
//     never static QTimer::singleShot. Section 13 names PgxlConnection and
//     TgxlConnection as the in-tree pattern to NOT copy: both arm retry
//     with static QTimer::singleShot (PgxlConnection.cpp:421), so a
//     pending reconnect cannot be cancelled, and both declare an
//     m_reconnectTimer member that is referenced nowhere in either .cpp
//     (verified against this tree: PgxlConnection.h:149 and
//     TgxlConnection.h:150 are the only two occurrences of that name
//     anywhere in either pair of files). Only the EXPONENTIAL
//     BACKOFF SCHEDULE is reused from that class (PgxlConnection.cpp:30,
//     1/2/5/10/30/60 s, saturating), scaled by reconnectBackoffUnitMs()
//     for tests -- the schedule was never the part section 13 objects to,
//     only the timer's ownership.
//   - Only a closure that WANTS a retry re-arms the timer: a plain
//     transport close or socket-level error does (the "kill the daemon"
//     and heartbeat-timeout cases section 13 exists for), but a closure
//     that carries an explicit reason FROM THE DAEMON (a version refusal,
//     an auth refusal, a preemption, a peer-limit refusal) does not --
//     retrying with the same latched credentials against a daemon that
//     just explicitly refused for a stated reason cannot converge, and
//     for a preemption specifically would fight the very session that
//     just took over. This is also how a reconnecting client cannot turn
//     into a preemption storm against StationServer::kMaxConcurrentPeers
//     (Task 18 review, carried forward): a preempted client's SessionEnd
//     is exactly the "daemon spoke with a reason" case, so it is never
//     retried. disconnectFromStation() -- the operator's own call, and the
//     only one that must win even while no session is active at all, mid
//     backoff-wait -- is the other thing that must cancel a pending retry
//     (section 13: "ICE restart and operator-initiated disconnect both
//     need [cancellability]").
//   - The one defect the Task 18 review found and graded Minor, because
//     nothing called connectToStation() twice on one client before this
//     task existed to do exactly that: the sslErrors/errorOccurred lambdas
//     below are connected with the QWebSocket as sender, not the
//     SessionTransport wrapping it, so attachTransport()'s
//     disconnect(stale, ...) release does not cover them. A stale socket's
//     asynchronous error, delivered after attachTransport() has already
//     moved m_transport on to a freshly attached session, used to reach
//     endSession() unconditionally and tear that fresh session down. Fixed
//     by capturing the transport in the lambda and returning early when it
//     is no longer m_transport -- the same guard onTransportClosed()
//     already used for the identical reason.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30  J.J. Boyd / KG4VCF  Level Cal: rx2PreampModeAvailable,
//                                    RX2's own preamp mode on the Core
//                                    (radioHardwareVersion 12). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Level Cal: startLevelCalibration and
//                                    cancelLevelCalibration, and the
//                                    levelCalibration feature for the run's
//                                    progress (radioHardwareVersion 12).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Level Cal: resetLevelCalibration
//                                    (radioHardwareVersion 12). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Direct media fix wave:
//                                    mediaTunnelOnlyIceConfiguration, the
//                                    tunnel alone for the fallback;
//                                    mediaTunnelInUse.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  The direct media ladder:
//                                    mediaStunServer, mediaDirectAvailable,
//                                    mediaDirectIceConfiguration.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-08-08  J.J. Boyd / KG4VCF  Remote daemon R2 Task 18: the GUI half
//                                    of the wss session. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-08-08  J.J. Boyd / KG4VCF  Remote daemon R2 Task 19: link loss,
//                                    daemon restart and reconnect (mirror
//                                    teardown, the stale-state accessor,
//                                    the session epoch, the owned
//                                    cancellable auto-reconnect timer, and
//                                    the stale-transport-error fix above).
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-08-09  J.J. Boyd / KG4VCF  Remote daemon R2: implement
//                                    IStationLink, so the GUI's slice
//                                    controls actually reach the daemon.
//                                    invokeCommand() had zero callers.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 4 (R-IOS-01): the
//                                    client picks the highest link major
//                                    it shares with the station, or leaves
//                                    without retrying; stationDeclares().
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22: the amp's and
//                                    tuner's own settings (requests,
//                                    `accessorySettings`, refusals routed
//                                    to the Advanced pages). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 / R-R3-47: the Tuner Genius's antenna,
//                                    operate and bypass requests.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 18 (R-IOS-08,
//                                    R-IOS-17): the hello declares
//                                    deviceAuth 1; this computer signs in
//                                    by its own device key, enrols it on a
//                                    token sign-in, trusts a paired Core by
//                                    its identity key and certificate
//                                    binding (identityChanged otherwise),
//                                    and the end report reads the Core's
//                                    end code. AI-assisted transformation
//                                    via Anthropic Claude Code.
//   2026-09-24: Part C fix wave: the optional device shortName in
//               auth.request, stored with the device. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app plan, desktop remote transmit (R-IOS-13,
//               R-R3-42): the hello declares remoteTx 1; the transmit verbs
//               go out through RemoteTransmitClient, three copies each.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 39 (D14, R-IOS-13): the Core's
//               `txState` object (TransmitState, txStateVersion 1), read-only,
//               for the window's transmit meters. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 1):
//                                    transmitSettingsAvailable.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 2):
//                                    requestTunePowerForTxBand.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 3): the TX
//                                    profile requests and
//                                    requestRadeResetVocoder.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 8): the Tuner
//                                    Genius relay nudge, LAN scan and
//                                    address requests
//                                    (remoteTgxlControlVersion 4).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 9): the Power
//                                    Genius operate, LAN scan and address
//                                    requests (remotePgxlControlVersion 4).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 10): the RF-Kit
//                                    operate, antenna, TCI mode and address
//                                    requests (remoteRfKitControlVersion 4).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  R-R3-46 / R-R3-32 (parity Task 14):
//                                    radioHardwareAvailable, the I/O board's
//                                    I2C and output pin requests
//                                    (radioHardwareVersion 7), and the HL2
//                                    link (stationTelemetryVersion 5).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave: I4 thisDeviceWireId and
//               transmitHolderText; M6 voxArmedHere; M7 a Core stop ends
//               this window's key. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave 2, the re-review's minors:
//               holderTransferring true while keys are refused for a
//               transfer's reasons (a dropped holder's fence, a transfer
//               ended with MOX on); stopEpoch names the key a stop ended so
//               a newer key is never ended by it; VOX at the Core listens
//               only to the device that armed it; the window says why MOX
//               and TUNE wait while another device holds. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 16):
//                                    requestFilterResponse
//                                    (dspInfoVersion 1).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 21 (R-IOS-18): the Core's
//                                    `stationRadios` stream and the station
//                                    radio verbs. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 19 (R-IOS-25): the Core's
//                                    record streams applied (spots and
//                                    the spot consoles), the `spotSources`
//                                    object and the spots.* verbs.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 18 (B3.1):
//                                    requestSelectBand (slice.selectBand,
//                                    bandSelectVersion 1).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 27 (R-IOS-16): the Core's last good
//               addresses are tried first (setCachedAddresses()), and each
//               connection attempt is recorded path by path
//               (StationConnectionAttempt) for the connection messages. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03, R-IOS-13):
//               tgxlAutotuneAvailable, holdsTransmitHere, otherHolderReason.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 78 (R-IOS-02, R-IOS-07, R-IOS-30): a
//               window signed in with its own key declares sessionHolder 1;
//               connectedDevices, devices, markers, confirm.request and
//               notice kept in RemoteDevicesState; tx.take, confirm.proceed,
//               confirm.cancel, notice.takeBack and session.leave. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: iPhone app plan Task 28 (R-IOS-16): connectThroughService(),
//               a session over a control connection the remote access
//               service introduced, retried the same way, and
//               sessionIceConfiguration() for its media. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: iPhone plan Task 22 / parity Task 20 (R-IOS-26):
//               stationFreedvAvailable() and requestFreedv(). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: iPhone app plan Task 29 (R-IOS-16): the path race and moving
//               the session (link section 21). J.J. Boyd (KG4VCF), AI-
//               assisted via Anthropic Claude Code.
//   2026-09-27: remote-window parity Task 22 (R-R3-49, R-IOS-18):
//               supportBundleAvailable(), requestSupportBundle(),
//               requestLogCategories() and requestCoreLog(). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27: Parity Task 33 (R-R3-49, R-R3-32): txReadingsAvailable(),
//               setCfcCompressionWanted() and cfcCompressionReceived (the
//               Core's txCfcCompression stream). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 step 2b (R-IOS-16, R-IOS-08): the
//               web relay's leg (RelayLeg) and its per-connection candidate
//               sources; the computer's own proxy settings (SystemProxy).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27: R-IOS-13 / R-R3-49: txModMonitorAvailable(),
//               setModMonitorSource() and requestModMonitorReset(), the AM
//               Mod Monitor in a remote window. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-28: iPhone app plan Task 78 items 3 and 7 (G-53): session.held
//               read into RemoteDevicesState and answered by answerHeld();
//               the takenOver end's name, id and time kept in
//               StationEndReport. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-28: iPhone app plan Task 25: deviceAdminAvailable(),
//               pairingAvailable() and requestDeviceAdmin() for the This
//               Core page. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: withholdFeatureForTest. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Addendum G-42: transmitSettingsPermitted
//                                    and transmitPermissionReason, this
//                                    window's transmit permission for the
//                                    settings only a permitted device may
//                                    change. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  requestCfcProfile
//                                    (transmitSettingsVersion 15).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  setTransmitSettingsVersionForTest:
//                                    a window of a current Core that
//                                    answers as an older one.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-28: slice control and shared listening plan Task 5:
//               sliceAccess() (SliceAccessMirror), remoteSliceAccessAvailable,
//               requestListen, requestStopListening, requestTakeControl,
//               requestRelease and sliceAccessHeld. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 14b: requestListenLevel
//               (slice.setListenLevel), a listened flag's "Your volume".
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 11: requestTxSlice sends
//               tx.setTxSlice for the TX applet's transmit-slice letters,
//               answered on deviceCommandFinished. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 17: setTokenSliceAccessForTest,
//               a remote window test's bench link declares sliceAccess
//               with sessionHolder, as a device-key sign-in does.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: take-over parity: the hello declares sliceAccess 2;
//               controlTakeBackAvailable() and its reason for the
//               controlTaken card's Take it back. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-30: take-over fix wave (M-3): a controlTaken card stays when
//               its Take it back may be tried again
//               (controlTakeBackMayBeTriedAgain). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-30: core-slice take-over: coreSliceTakeAvailable() and
//               coreSliceTakeUnavailableReason(); the hello declares
//               sliceAccess 3. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-30: inbound sibling lane: a pending write keeps the operator's
//               value (PendingWrite), restoreOperatorValues() puts it back
//               when an inbound apply moves it as a side effect, and the
//               pauseWriteFlushForTest / flushWritesForTest seams.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: inbound sibling fix round 1: a delta whose side effect
//               moves an UNSENT edit cancels it (SideEffectRule::Delta),
//               following Thetis's per-mode filter edges. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: inbound sibling fix round 2: m_coreValues (the last value
//               the Core sent), stepBackUnsentEdits, cancelUnsentEdit, and
//               PendingWrite's in-flight write. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-30: inbound sibling fix round 3: the unsent edits a delta
//               cancels are named per cause (kDeltaCancelRules), not
//               inferred from values; the generic step-back is gone.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: inbound sibling fix round 4: unresolvedDeltaCancelRuleNames()
//               checks the rule table against the schemas. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QAbstractSocket>
#include <QByteArray>
#include <QDateTime>
#include <QElapsedTimer>
#include "core/settings/SettingsBackupTransfer.h"
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QUrl>

#include <memory>
#include <functional>
#include <optional>

#include "core/session/IStationLink.h"
#include "core/session/RemoteTransmitClient.h"
#include "core/session/LinkVersion.h"
#include "models/Band.h"
#include "core/session/MirrorSchema.h"
#include "core/session/SessionMessages.h"
#include "core/session/StateMirror.h"
#include "core/session/StationCapabilities.h"
#include "core/session/SessionTransport.h"
#include "core/session/TxWatchClient.h"
#include "core/session/PathRacer.h"
#include "core/session/IceConfiguration.h"

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

namespace NereusSDR {

class ClientDeviceIdentity;
class DataChannelTransport;
class IceConfiguration;
class RelayLeg;
class RadioModel;
class RendezvousDialer;
class SessionTransport;
class SwitchableTransport;
class MediaTunnel;
class RemoteDevicesState;
class SliceAccessMirror;
class SettingsProxy;
class TransmitState;
class StationVax;
class TxWatchClient;

/// R-R3-21 / R-R3-23 / R-R3-38: how the last session ended, as far as it
/// decides what a remote window offers next. Only an end that will not
/// fix itself is recorded; a dropped link or any end the Core marks
/// retryable leaves kind None, and the window retries as before.
/// iPhone app plan Task 27 (R-IOS-16; spec section 5.3 item 14, "what
/// the phone tried (this Wi-Fi, direct, relay)"): one connection attempt,
/// path by path, as the desktop's connection messages describe it. The
/// iPhone app's twin is ConnectionAttempt (plan Task 27a), with the same
/// fields.
struct StationConnectionAttempt {
    /// This network: an address on one of this computer's own networks (or
    /// this computer). Direct: any other address. Relay: through the remote
    /// access service's relay (the control session across NAT, plan Task
    /// 28, records it).
    /// Service (iPhone app plan Task 29): through the internet service,
    /// before the connection shows whether it went through the relay.
    /// WebRelay (step 2b): through the web relay on the service's name
    /// (the rendezvous document, section 12).
    enum class Path { ThisNetwork, Direct, Relay, Service, WebRelay };
    enum class Outcome {
        Trying,
        Connected,
        NoAnswer,     ///< nothing answered at that address
        TimedOut,     ///< it did not answer in time
        NotThisCore,  ///< another computer answered there
        Failed,       ///< it answered, and the attempt ended there
        // iPhone app plan Task 29 (link section 21.1): the race.
        AnotherPathFirst, ///< another path reached the Core first
        RelayOff,         ///< the Core has the relay turned off
        CoreTooOld,       ///< the Core does not answer through the service
        MovedOn,          ///< connected, then the session moved to a better path
        // Step 2b: the web relay ended the leg; Try::reason holds its words.
        WebRelayEnded,
    };
    struct Try {
        Path path = Path::Direct;
        /// host:port as the operator would type it.
        QString address;
        Outcome outcome = Outcome::Trying;
        /// Step 2b: the words to show for this line in place of the
        /// outcome's (the web relay's end, section 12.4), when set.
        QString reason;
    };

    QDateTime started;
    QList<Try> tries;

    bool connected() const;
    /// Plain words for the connection messages, for example "Tried this
    /// network at 192.168.1.20:47910: no answer. Direct at
    /// shack.example.net:47910: connected." Empty before any try.
    QString summary() const;
    static QString pathText(Path path);
    static QString outcomeText(Outcome outcome);
    /// ThisNetwork for a loopback address or one inside the subnet of an
    /// address of one of this computer's running interfaces, or a name
    /// ending ".local"; Direct otherwise.
    static Path pathFor(const QUrl& url);
};

struct StationEndReport {
    enum class Kind {
        None,           ///< nothing recorded (still running, or a retryable end)
        TakenOver,      ///< another app connected to the Core and took over
        VersionRefused, ///< the link versions are too far apart
        Refused,        ///< any other end the Core marked not retryable
        // iPhone app Task 18 (R-IOS-08), chosen by the end's code:
        DeviceRemoved,   ///< deviceRemoved or deviceNotPaired: pair this computer again
        PairingRequired, ///< pairingRequired: the Core signs in paired devices only
        IdentityChanged, ///< identityChanged (this app's own end): not the Core it paired with
    };
    Kind kind = Kind::None;
    /// The end's code as sent (SessionEndCode), or this app's own
    /// identityChanged; empty from an older Core, which sends none, and
    /// then the kind comes from the reason's words
    /// (SessionEndReasons::parse).
    QString code;
    /// The reason as sent (the Core's own words, or this app's own for a
    /// version refusal it made itself). Raw: for the log and for
    /// OperatorReasonText, never shown as is.
    QString reason;
    /// TakenOver only: the network address of the app that took over, as
    /// the Core names it in its reason. Empty when the Core does not say.
    QString takenOverBy;
    /// TakenOver only (iPhone app plan Task 78 item 3, G-53): the name and
    /// wire id of the device that took this window's place, as the Core's
    /// end sends them (`takenOverBy`, `takenOverById`), and when, by this
    /// computer's clock (the end's `secondsAgo` before it arrived). Empty
    /// and invalid from an older Core.
    QString takenOverByName;
    QString takenOverById;
    QDateTime endedAt;
    /// VersionRefused only: the major link versions of each side, or -1
    /// when the reason did not carry them.
    int appMajor = -1;
    int coreMajor = -1;
};

/// QObject first, deliberately: moc requires the QObject base to come
/// first, and IStationLink is a plain abstract interface with no metatype
/// involvement, so the pair compose without any virtual-inheritance
/// gymnastics.
class StationClient : public QObject, public IStationLink {
    Q_OBJECT

public:
    /// Matches StationServer's, and for the same reasons -- see that
    /// class's heartbeat section. Both ends heartbeat independently: a
    /// silently dead link has to be detected from whichever side is still
    /// alive, and which side that is is not knowable in advance.
    static constexpr int kDefaultHeartbeatIntervalMs = 20000;
    static constexpr int kDefaultMaxMissedPongs = 2;
    /// Task 29 step 2b (fast failure detection): on a path through a relay
    /// (TURN or the web relay) or with media in the WebSocket tunnel, the
    /// window pings this often instead, so a link that died is found in
    /// kMaxMissedPongs of these (4 to 6 s), not 40 to 60 s. Never longer
    /// than the heartbeat set (setHeartbeatIntervalMs), and not at all
    /// while the heartbeat is off.
    static constexpr int kRelayedHeartbeatIntervalMs = 2000;
    /// The cadence in use now (kRelayedHeartbeatIntervalMs on such a path).
    int effectiveHeartbeatIntervalMs() const;
    /// Step 2b: media runs in the WebSocket tunnel (the media controller
    /// says so), which counts as a relayed path for the heartbeat.
    void setMediaTunnelInUse(bool inUse);
    bool mediaTunnelInUse() const { return m_mediaTunnelInUse; }

    /// How often locally-observed property changes are drained toward the
    /// station. See StationServer::kDefaultDeltaFlushMs for the same
    /// reasoning in the other direction.
    static constexpr int kDefaultWriteFlushMs = 50;

    /// The exponential-backoff schedule's unit (see scheduleReconnect() in
    /// the .cpp). Production default is 1000, so the schedule below reads
    /// in real seconds; a test shrinks this so the mechanism can be
    /// exercised without a multi-minute wait -- see
    /// setReconnectBackoffUnitMs().
    static constexpr int kDefaultReconnectBackoffUnitMs = 1000;

    /// Largest inbound WebSocket message, and frame, on this client's own
    /// socket. Applied by WebSocketTransport's constructor.
    ///
    /// Two orders of magnitude above StationServer::kMaxIncomingMessageBytes
    /// because the two directions carry different traffic, not because one
    /// side is trusted more. The client's largest legitimate inbound
    /// message is the connect-time settings snapshot, which
    /// StationServer::promoteToSession sends as ONE SettingsSnapshot
    /// carrying every Station-classified key.
    ///
    /// Sized against the measurement in R2 design addendum section 8: a
    /// real settings file of 15,201 keys and 3,186,789 bytes across five
    /// MACs, of which the subset a snapshot actually carries (the
    /// connected MAC's hardware/ subtree, hardware/oc/, and the
    /// non-hardware Station keys -- SettingsProxyServer::buildSnapshot) is
    /// roughly 2,900 keys and 190 KiB. The JSON envelope adds about 30
    /// bytes per entry, so that encodes to roughly 275 KiB. 8 MiB is about
    /// 30 times the measured snapshot and still covers the pathological
    /// case of an entire 3.19 MB store classifying Station, while staying
    /// 256 times below Qt's own default.
    ///
    /// Qt's default would let a station this client has pinned, but which
    /// has since been compromised, allocate about 2 GiB in the operator's
    /// GUI. A pin is an identity check, not a promise of good behaviour.
    static constexpr quint64 kMaxIncomingMessageBytes = 8ULL * 1024ULL * 1024ULL;

#ifdef Q_OS_MAC
    static constexpr bool kBuiltForMacOs = true;
#else
    static constexpr bool kBuiltForMacOs = false;
#endif

    /// R-R3-17: the operator reason for a failed connect attempt. Pure, so
    /// it is testable without a socket. `errorText` is the socket's own
    /// error string, which is returned unchanged except in one case: on
    /// macOS, a host-unreachable failure (NetworkError whose text reads
    /// "Host unreachable" or "No route to host") toward a private or
    /// link-local address literal. That is what macOS Local Network
    /// privacy reports when it blocks the app, so the reason names the
    /// setting to change instead. `host` is the host the operator entered.
    static QString connectionFailureReason(QAbstractSocket::SocketError error,
                                           const QString& errorText,
                                           const QString& host,
                                           bool macOs = kBuiltForMacOs);

    /// `radioModel` must be Role::Remote and is NOT owned. `settingsProxy`
    /// is the backend a remote-mode GUI installs via
    /// AppSettings::setRemoteBackend(); also not owned. Both must live on
    /// this object's thread.
    ///
    /// `supportedMajors` (iPhone app Task 4) is the link majors this client
    /// supports, oldest first. The default is the build's own
    /// (kSupportedSessionMajors); tests inject theirs.
    enum class SessionPurpose { Ordinary, RenameOnly };
    SessionPurpose sessionPurpose() const { return m_sessionPurpose; }
    /// A temporary rename rechecks desktop exclusion immediately before auth.
    void setAdmissionGuard(std::function<bool()> guard) { m_admissionGuard = std::move(guard); }
    QByteArray deviceIdentityFingerprint() const;
    explicit StationClient(RadioModel* radioModel, SettingsProxy* settingsProxy,
                           QObject* parent = nullptr,
                           const QList<quint16>& supportedMajors =
                               LinkVersion::supportedMajors(),
                           SessionPurpose purpose = SessionPurpose::Ordinary);
    ~StationClient() override;

    StationClient(const StationClient&) = delete;
    StationClient& operator=(const StationClient&) = delete;

    /// Open a wss connection. `expectedFingerprint` is the station's
    /// SHA-256 in CertificateStore::fingerprintSha256()'s colon-separated
    /// uppercase form; an empty one means "do not pin", which is refused
    /// unless allowUnpinned is true, because silently not pinning is the
    /// failure mode that makes the whole certificate model decorative.
    ///
    /// iPhone app Task 18: `stationIdentityFingerprint` (32 bytes) is the
    /// identity key of the Core this computer paired with. When set, the
    /// Core is trusted by that key and not by the pin (so the pin may be
    /// empty): the hello must show that key, its certificate binding must
    /// verify for the certificate this connection presents, and this
    /// computer signs in with its device key (setDeviceIdentity()) instead
    /// of the token. Anything else ends the attempt, not retryably, with
    /// the end code identityChanged. Needs wss://.
    void connectToStation(const QUrl& url, const QString& token,
                          const QString& expectedFingerprint,
                          bool allowUnpinned = false,
                          const QByteArray& stationIdentityFingerprint = QByteArray());

    /// iPhone app plan Task 27 (R-IOS-16; the pairing design, section 5.3,
    /// "cached address first"): the Core's last good addresses, most recent
    /// first, tried before the url connectToStation() is given, on that
    /// connect and on every automatic reconnect. An address that does not
    /// answer, or at which another computer answers, gives way to the next
    /// at once; one that is not the last in the list has
    /// kCachedAddressOpenTimeoutMs to open before it gives way. Takes effect
    /// on the next connectToStation().
    ///
    /// Fix wave I1: a connect with allowUnpinned set dials only its own url,
    /// never a cached address (nothing proves who answers there, and the
    /// token would go to it). A pinned connect whose certificate does not
    /// match at an address that is not the last in the list gives way to
    /// the next with the token unsent, as a paired Core does on
    /// identityChanged; only a mismatch at the last address ends it.
    void setCachedAddresses(const QList<QUrl>& addresses);

    /// iPhone app plan Task 28 (R-IOS-16): connects to the paired Core
    /// through the remote access service: `servers` in order
    /// (RendezvousClient::serverUrls), the Core's rendezvous id
    /// (RendezvousWire::rendezvousId of its identity key) and its identity
    /// fingerprint, which trusts it exactly as connectToStation() does for
    /// a paired Core (the hello must show that key, and its certificate
    /// binding must verify for the certificate the Core presents in DTLS,
    /// before anything is sent). Needs this computer's device key
    /// (setDeviceIdentity()): only a paired device is introduced. The
    /// session runs over the control connection (DataChannelTransport) and
    /// is retried like any other, through the service again.
    void connectThroughService(const QList<QUrl>& servers, const QString& stationRendezvousId,
                               const QByteArray& stationIdentityFingerprint);
    /// The ICE settings of the session's media when the session came
    /// through the service: the control connection's STUN server, and its
    /// relay only when the control connection's path is relayed
    /// (DataChannelTransport::mediaIceConfiguration()); none for a
    /// WebSocket session.
    std::optional<IceConfiguration> sessionIceConfiguration() const;
    /// Task 29 step 2b (link section 21, "The media tunnel"): the Core told
    /// mediaTunnelVersion 1, so the media start may declare the tunnel even
    /// before a later path move to a direct WebSocket.
    bool mediaTunnelAvailable() const;
    /// The ICE settings for media over the tunnel (made on first use on
    /// this session's transport); none unless the current path carries binary.
    std::optional<IceConfiguration> mediaTunnelIceConfiguration();
    /// The direct media ladder (link section 21): the STUN server every
    /// media connection uses. The first stun: entry of the Core's
    /// mediaStunUrls; without one, this session's service STUN server, or
    /// the last one a session through the service used (in memory only).
    std::optional<IceServerAddress> mediaStunServer() const;
    /// The Core told mediaDirectVersion 1: a media replace may add
    /// "mediaDirectVersion": 1 for a connection without tunnel or relay.
    bool mediaDirectAvailable() const;
    /// STUN and host candidates only: no relay, no tunnel.
    IceConfiguration mediaDirectIceConfiguration() const;
    /// The fallback from a silent direct path: the tunnel's candidates
    /// alone, no STUN and no host candidates (none unless the tunnel is
    /// usable, as mediaTunnelIceConfiguration()).
    std::optional<IceConfiguration> mediaTunnelOnlyIceConfiguration();

    /// iPhone app plan Task 29 (R-IOS-16; link section 21): where the
    /// paired Core can be reached through the internet service: the
    /// servers (RendezvousClient::serverUrls), the Core's rendezvous id,
    /// and what its last session said (relayAllowed; controlChannelVersion,
    /// -1 when none is recorded). With a usable route, connectToStation()
    /// for a paired Core races the service beside the Core's addresses.
    struct ServiceRoute {
        QList<QUrl> servers;
        QString rendezvousId;
        bool relayAllowed = true;
        int controlChannelVersion = -1;
        // Consult the saved observation again for automatic retries, so a
        // negative result can expire while this window stays open.
        std::function<int()> currentControlChannelVersion;
    };
    void setServiceRoute(const ServiceRoute& route) { m_serviceRoute = route; }
    ServiceRoute serviceRoute() const { return m_serviceRoute; }
    struct ConnectionCandidates { QList<QUrl> addresses; ServiceRoute service; };
    /// Only ordinary initial/retry races consume this; nullopt retires the saved lease.
    using CandidateSource = std::function<std::optional<ConnectionCandidates>()>;
    void setCandidateSource(CandidateSource source) { m_candidateSource = std::move(source); }
    /// The paired Core's hello proves it: its identity key is the one
    /// `expectedIdentity` fingerprints and its certificate binding verifies
    /// for `certSha256`, the certificate that connection presented (link
    /// section 3.4). `rendezvousId`, when given, receives the Core's
    /// rendezvous id (the rendezvous document, section 4.2).
    static bool helloProvesPairedCore(const SessionMessage& hello, const QByteArray& certSha256,
                                      const QByteArray& expectedIdentity,
                                      QString* rendezvousId = nullptr);
    /// The rendezvous id of the Core this session signed in to (from its
    /// identity key), empty for one without an identity. What a window
    /// saves with the paired Core so it can reach it through the service.
    QString stationRendezvousId() const { return m_stationRendezvousId; }
    /// The rank of the path the session runs on (PathRacer::Rank), or -1
    /// when it did not come from a race.
    int pathRank() const { return m_pathRank; }
    /// Moves of this session to a better path (link section 21.2).
    int pathSwitches() const { return m_pathSwitches; }
    /// Test seams: the upgrade schedule (PathRacer::kUpgradeRetryMs; the
    /// last repeats), and the rendezvous rung's deadlines in a race.
    void setUpgradeScheduleForTest(const QList<int>& delaysMs) { m_upgradeScheduleMs = delaysMs; }
    /// Test seams (Task 29 step 2a re-review, Minor 14): the upgrade
    /// schedule's step, and whether a move is waiting for its ticket.
    int upgradeAttemptForTest() const { return m_upgradeAttempt; }
    bool upgradeUnderWayForTest() const { return m_upgrade.has_value(); }
    void setServiceRungDeadlinesForTest(int dialMs, int answerMs)
    {
        m_serviceDialDeadlineMs = dialMs;
        m_serviceAnswerDeadlineMs = answerMs;
    }
    /// Test seam (link section 21.2): moves the session to `next`, a
    /// connection on which the Core's hello was already read, as an
    /// upgrade does: a ticket on the session, this window's hello and
    /// path.join on `next`, then the barrier. `rank` is pathRank() once
    /// moved. False when the session cannot move now (canMovePathNow()).
    bool moveSessionForTest(SessionTransport* next, int rank, const QUrl& url = {});
    /// Test seam: the service attempt's bound
    /// (RendezvousDialer::kDialDeadlineMs).
    void setServiceDialDeadlineMs(int ms) { m_serviceDialDeadlineMs = ms; }
    QList<QUrl> cachedAddresses() const { return m_cachedAddresses; }
    /// How long an address that is not the last to try may take to open.
    static constexpr int kCachedAddressOpenTimeoutMs = 4000;
    /// The open time in use: kCachedAddressOpenTimeoutMs unless a test
    /// shortened it (as setHandshakeDeadlineMs() is). Applies from the next
    /// address dialled.
    void setCachedAddressOpenTimeoutMs(int ms);
    int cachedAddressOpenTimeoutMs() const { return m_openTimer->interval(); }
    /// The current (or last) connection attempt, path by path.
    const StationConnectionAttempt& connectionAttempt() const { return m_attempt; }
    /// The address this session reached the Core at; empty until the
    /// connect sequence completes.
    QUrl connectedUrl() const { return m_connectedUrl; }

    /// Drive the session over an already-open transport instead of dialing
    /// one. Same code path from the first message onward; this is how the
    /// protocol half is exercised without TLS (SessionTransport.h explains
    /// why that matters). Takes ownership by reparenting.
    ///
    /// `expectedFingerprint` is the pin this session owes, and defaults to
    /// none because the transports adopted here today carry no TLS. It is
    /// not decoration: a caller adopting an already-open link that DID
    /// negotiate TLS has to be able to state the pin, or the token would
    /// leave this process without the comparison connectToStation()
    /// guarantees. A non-empty value with a transport that cannot produce
    /// a peer certificate is refused rather than waved through.
    ///
    /// `stationIdentityFingerprint` is connectToStation()'s: the paired
    /// Core's identity, checked against what `transport` presents
    /// (SessionTransport::peerCertificateSha256()).
    void startSession(SessionTransport* transport, const QString& token,
                      const QString& expectedFingerprint = QString(),
                      const QByteArray& stationIdentityFingerprint = QByteArray());

    /// iPhone app Task 18 (R-IOS-08): this computer's own device key and
    /// the name the Core lists it by. With a usable key the hello declares
    /// `features.deviceAuth` 1, a paired Core is signed in to by key, and
    /// a token sign-in to a Core that has an identity enrols the key in
    /// the same step (the link document, section 3.5). Without one this
    /// client signs in with the token alone, as before. Not owned beyond
    /// the shared pointer; applies from the next hello.
    /// Part C fix wave: `shortName`, when not empty, goes in every device
    /// block as its `shortName` (ClientDeviceIdentity::machineShortName()).
    void setDeviceIdentity(std::shared_ptr<const ClientDeviceIdentity> identity,
                           const QString& deviceName, const QString& shortName = QString());

    /// The identity fingerprint this client trusts the Core by: the one it
    /// was given to connect with, or the one its key was just enrolled
    /// with. Empty for a Core trusted by its pin.
    QByteArray stationIdentityFingerprint() const { return m_stationIdentity; }

    /// `attemptReconnect` (Task 19) decides whether this closure re-arms
    /// the automatic reconnect timer once the session has ended. Defaults
    /// to false, which is correct for every caller that has not thought
    /// about it: an operator's own "Disconnect" click (Task 20) must not
    /// be followed by a surprise redial, and neither must any of this
    /// class's own internal calls that already carry an explicit reason
    /// FROM THE DAEMON (version refusal, auth refusal, preemption,
    /// peer-limit refusal) -- see the class comment's link-loss section
    /// for why retrying those cannot converge. onHeartbeatTick() is the
    /// one internal caller that passes true. Cancels a PENDING retry too,
    /// even when no session is active at all (mid backoff-wait) -- see
    /// the class comment.
    void disconnectFromStation(const QString& reason, bool attemptReconnect = false);

    bool isHandshakeComplete() const { return m_handshakeComplete; }
    QString lastError() const { return m_lastError; }

    /// Task 19. True once a session has been fully established
    /// (handshakeComplete()) at least once, and is not right now.
    /// Distinguishes "no data exists yet" (never connected: false) from
    /// "there IS retained state, but it can no longer be trusted as live"
    /// (true) -- design doc section 13. RadioModel's own property values
    /// (SliceModel::frequency() and the rest) are untouched by a link
    /// loss; this is what a caller checks before trusting them. Cleared
    /// the moment handshakeComplete() fires again.
    bool isStale() const { return m_everConnected && !m_handshakeComplete; }

    /// Task 19. Bumped by one on every attachTransport() call, including
    /// the very first (so the first session is epoch 1; 0 means "never
    /// attached"). See the class comment.
    quint32 sessionEpoch() const { return m_sessionEpoch; }

    /// Task 19. True while the owned reconnect timer is counting down
    /// between attempts.
    bool isReconnectPending() const;

    /// A dial, live session or automatic retry is in progress. Unlike the
    /// mirrored radio state, this remains true when Core is reachable but
    /// its radio is offline. Used by the operator's Connect/Disconnect actions.
    bool isConnectionActive() const
    {
        return m_sessionActive || isReconnectPending() || m_serviceDialing
            || (m_racer && m_racer->running());
    }

    /// R-R3-38: the last end that will not fix itself (a takeover, a
    /// version refusal, or any other end the Core marked not retryable).
    /// Cleared when the next link is attached, so it describes the end
    /// that stopped this client, never an older one. Read from the Core's
    /// session end or sign-in refusal as sent: its code (iPhone app Task
    /// 18) chooses the kind, and a Core that sends no code is read by its
    /// reason's words. This client's own identityChanged end is recorded
    /// here too.
    StationEndReport lastEndReport() const { return m_lastEndReport; }
    /// The Core ended this session to change its radio (session.end code
    /// radioChanging) and this client is reconnecting: the Core's words,
    /// until the next session is up, the Core answers with any other end,
    /// or a redial fails after the backoff's longest wait (follow-up N2).
    /// Empty otherwise.
    QString radioChangeReason() const { return m_radioChangeReason; }

    /// Test seam: production default is kDefaultReconnectBackoffUnitMs
    /// (real seconds). See scheduleReconnect() in the .cpp for the
    /// schedule this scales.
    void setReconnectBackoffUnitMs(int ms);
    int reconnectBackoffUnitMs() const { return m_reconnectBackoffUnitMs; }

    /// The station's descriptor as applied. Default-constructed before the
    /// capability exchange.
    const StationCapabilities& capabilities() const { return m_capabilities; }
    /// Test seam: declare a hello feature this window does not (an app's,
    /// such as audioQuality). Before startSession().
    void declareFeatureForTest(const QByteArray& name, int version)
    {
        m_declaredFeatures.insert(name, version);
    }
    /// Test seam: the transmitSettingsVersion this window heard, as an
    /// older Core would have sent it. After the handshake.
    void setTransmitSettingsVersionForTest(int version)
    {
        m_capabilities.transmitSettingsVersion = version;
    }
    /// Test seam: stops the write-flush timer, so a test decides when the
    /// window's pending writes leave (flushWritesForTest). After the
    /// handshake; the next SnapshotComplete starts the timer again.
    void pauseWriteFlushForTest() { m_writeFlushTimer->stop(); }
    /// Test seam: one write-flush tick, now.
    void flushWritesForTest() { onWriteFlushTick(); }
    /// Every class and property name in the delta cancel rules
    /// (kDeltaCancelRules in StationClient.cpp) that the mirror schema
    /// does not resolve, as "Class.property". Empty when the table is
    /// sound. applyUpdates skips a name it cannot resolve, so a typo in
    /// the table would otherwise switch its rule off silently.
    static QList<QByteArray> unresolvedDeltaCancelRuleNames();

    /// The minor version both ends agreed on (section 7.0: negotiate down
    /// to the lower). Meaningful once the station's Hello has arrived.
    quint16 agreedMinor() const { return m_agreedMinor; }

    /// iPhone app Task 4 (R-IOS-01): the link major this client chose, the
    /// highest it shares with the station's hello. 0 before that hello, and
    /// again from each new attach until the next one.
    quint16 agreedMajor() const { return m_agreedMajor; }

    /// True when the station's hello declared `feature` at `minVersion` or
    /// later. What this client must know before capabilities arrive is
    /// asked here. An older station declares nothing, and a new attach
    /// forgets the previous station's declarations.
    bool stationDeclares(const QByteArray& feature, int minVersion) const;

    bool mediaAvailable() const;
    bool remoteWidebandAvailable() const;
    /// Agreed minor 8 or later and advertised by Core: audio contexts carry
    /// the encoder profile or the off reason.
    bool remoteAudioStatusAvailable() const;
    /// Agreed minor 9 or later and advertised by Core: spectrum contexts
    /// report the grant Core made for the endpoint.
    bool spectrumGrantAvailable() const;
    /// Parity Task 17 (R-R3-01): the Core takes a subscribe's `decimation`
    /// (spectrumGrantVersion 2); a window below it never sends one.
    bool spectrumDecimationAvailable() const;
    std::optional<DisplayBudgetLimits> remoteDisplayBudgetLimits() const;
    /// Why the Core's display budget is below its ceiling (R-R3-08, R-R3-37):
    /// CoreBusy while the Core computer is short of processing time. None
    /// without a budget, before minor 11, or when Core did not say.
    /// displayBudgetChanged() fires when it changes.
    DisplayBudgetReason remoteDisplayBudgetReason() const;
    bool remotePs3DisplaySubscribed() const;
    quint32 requestPs3DisplaySubscription(bool enabled);
    bool remoteCtunAvailable() const;
    bool remoteTgxlConfigAvailable() const override;
    bool remoteFourO3AControlAvailable() const override;
    // R-R3-47 / R-R3-22: see IStationLink.
    bool stationLinkReady() const override;
    // Merge of Tasks 38 and 39: see IStationLink.
    bool transmitTimeOutAvailable() const override;
    bool adcAttenuatorsAvailable() const override;
    bool remoteAmplifierStatusAvailable() const override;
    bool remoteRfKitStatusAvailable() const override;
    // R-R3-47 / R-R3-22: see IStationLink.
    bool remotePgxlControlAvailable() const override;
    // R-R3-47 / R-R3-48: see IStationLink.
    bool remoteRfKitControlAvailable() const override;
    // R-R3-47 / R-R3-22: see IStationLink.
    bool accessoryDataAvailable() const override;
    // R-R3-47 / R-R3-22: see IStationLink.
    bool pgxlDeviceSettingsAvailable() const override;
    bool tgxlDeviceSettingsAvailable() const override;
    bool tgxlControlAvailable() const override;
    bool tgxlAutotuneAvailable() const override;
    bool tgxlOperateAppliesWhole() const override;
    bool tgxlFullControlAvailable() const override;
    bool pgxlFullControlAvailable() const override;
    bool rfKitFullControlAvailable() const override;
    bool rfKitCountersAvailable() const override;
    bool rfKitResponseTimeAvailable() const override;
    /// R-R3-49 (parity Task 1): the link is ready at minor 11 and the Core
    /// offers transmitSettingsVersion at least `minVersion` (1 or more): it
    /// takes this window's transmit settings while its radio is off the
    /// air. False: IStationLink::transmitSettingsUnavailableReason().
    bool transmitSettingsAvailable(int minVersion = 1) const override;
    bool transmitSettingsPermitted() const override;
    QString transmitPermissionReason() const override;
    /// R-R3-49 (parity Task 7): minor 11 and transmitSettingsVersion 7: the
    /// Core takes this window's PureSignal arming off the air.
    bool pureSignalArmingOffered() const;
    bool stationTciAvailable() const override;
    bool coreServesTciOnThisComputer() const override;
    /// iPhone app plan Task 39 (D14, R-IOS-13): the Core's `txState` as this
    /// window last heard it (never null). Its values are the idle ones while
    /// the Core does not send it (txStateVersion 0) and after a session
    /// ends; its stop fields keep the last stop until the next snapshot.
    TransmitState* transmitState() const { return m_transmitState; }
    /// iPhone app plan Task 25 (R-IOS-18): the window's copy of the Core
    /// computer's VAX channels (the `vax` object; never null). Its values
    /// are the idle ones while the Core does not send it.
    StationVax* stationVax() const { return m_stationVax; }
    /// The Core sends its computer's VAX channels (vaxVersion 1): a Core the
    /// desktop hosts.
    bool stationVaxAvailable() const;
    /// The window holds the Core's `vax` object now (this session's snapshot
    /// carried it): what the VAX applet's "Station computer" section follows.
    bool stationVaxHeld() const { return m_stationVaxHeld; }
    /// Whether this window shows the Core computer's VAX meters. While true
    /// (and the Core sends them) the window subscribes to the vaxLevels
    /// stream, again after each reconnect; false unsubscribes.
    void setStationVaxLevelsWanted(bool wanted);
    /// Parity Task 33 (R-R3-49, R-R3-32): the Core sends its transmit
    /// readings (txReadingsVersion 1): `txState`'s forwardAdcRaw and
    /// reflectedAdcRaw, and the txCfcCompression stream.
    bool txReadingsAvailable() const;
    /// A9 (iPhone app plan Task 39): the Core also sends the seven stage
    /// readings a local window's container meters show (txReadingsVersion
    /// 3): `txState`'s eqDb, levelerDb, levelerGainDb, cfcDb, cfcGainDb,
    /// alcGainDb and alcGroupDb.
    bool txStageReadingsAvailable() const;
    /// Parity Task 33: whether this window shows the CFC bar chart. While
    /// true (and the Core sends it) the window subscribes to the Core's
    /// txCfcCompression stream, again after each reconnect; false
    /// unsubscribes, so the Core stops reading.
    void setCfcCompressionWanted(bool wanted);
    bool cfcCompressionWanted() const { return m_cfcCompressionWanted; }
    /// Fix wave I4: this window's device id as the Core sends device ids
    /// (connectedDevices, txState's holderDeviceId), or empty without a
    /// device identity.
    QString thisDeviceWireId() const;
    /// Fix wave M6: this window armed the Core's VOX (its own write turned
    /// `transmit.voxEnabled` on, and VOX is still on). VOX another device
    /// armed is not this window's: it neither streams its microphone for it
    /// nor keeps its keepalives going.
    bool voxArmedHere() const { return m_voxArmedHere; }
    /// Fix wave I4: who holds transmit on the Core, in plain words for the
    /// window ("This computer holds transmit.", "Grant's iPhone holds
    /// transmit. MOX and TUNE here wait until it lets go.", "... and is
    /// away. ...", "Transmit is changing hands.", "The radio did not
    /// confirm it stopped transmitting."), or empty while nobody does or
    /// the Core does not say (txStateVersion 2).
    QString transmitHolderText() const;
    /// iPhone app plan Task 77 (ruling 7.7): while another device holds
    /// transmit, the transmitter's settings are its own: "<holder> has the
    /// transmitter.", as the Core refuses a change. Empty while this window
    /// holds transmit, nobody does, or the Core does not say.
    QString otherHolderReason() const;
    /// Task 77 (ruling 8.4): whether this window holds transmit on the Core
    /// (false while nobody does, or the Core does not say).
    bool holdsTransmitHere() const override;
    /// Task 77: whether the holder's rules reach this window: the Core
    /// names who holds transmit (txStateVersion 2) and takes remote keys
    /// (not receive-only).
    bool knowsTransmitHolder() const;

    // ── iPhone app plan Task 78: several devices on one Core ────────────
    /// What the Core says about the other devices (never null).
    RemoteDevicesState* remoteDevices() const { return m_remoteDevices; }
    /// Slice control plan Task 5: who controls and who listens to each
    /// slice on the Core, as its `access:<id>` objects say (never null;
    /// empty on a Core without sliceAccessVersion).
    SliceAccessMirror* sliceAccess() const { return m_sliceAccess; }
    /// The Core treats this window as a device that shares it: it signed
    /// in with this computer's own key, declared sessionHolder 1, and the
    /// Core answered sessionHolderVersion 1 at minor 11.
    bool sessionHolderAvailable() const;
    /// tx.take may be sent: sessionHolderAvailable() and remoteTxVersion 2
    /// on a Core that takes this window's keys.
    bool transmitTakeAvailable() const;
    /// Another device (or the radio's own PTT) holds transmit and this
    /// window could take it: transmitTakeAvailable() and a holder that is
    /// not this window.
    bool transmitHeldElsewhere() const;
    /// `tx.take`: with `shown`, the holder epoch and on-air state the
    /// operator was shown before confirming (ruling 8.7), so the Core takes
    /// at once when nothing changed. Returns the command id, 0 when it
    /// could not be sent.
    quint32 requestTakeTransmit(bool shown, qint64 holderEpoch, bool shownKeyed);
    /// Slice control plan Task 11 (U8): `tx.setTxSlice {sliceId}`, the
    /// holder's choice of the slice it transmits on (ruling 8.10: a keyed
    /// move unkeys first). The answer arrives on deviceCommandFinished.
    /// Returns the command id, 0 when it could not be sent.
    quint32 requestTxSlice(int sliceId);
    /// `confirm.proceed {id, choice}` (-1 for a question with no choices).
    quint32 proceedQuestion(qint64 id, qint64 choice);
    /// `confirm.cancel {id}`.
    quint32 cancelQuestion(qint64 id);
    /// `notice.takeBack {id}`. The card goes at once, except a
    /// controlTaken card's (take-over fix wave, M-3): that one goes when
    /// the take-back works or can never work now, and stays when it was
    /// refused and may be tried again (controlTakeBackMayBeTriedAgain).
    quint32 takeBackNotice(qint64 id);
    /// Take-over parity (sliceAccessVersion 2): a controlTaken notice's
    /// Take it back works here. An older Core offers none, and the card
    /// shows it off with controlTakeBackUnavailableReason().
    bool controlTakeBackAvailable() const;
    static QString controlTakeBackUnavailableReason();
    /// Core-slice take-over (JJ, 2026-09-30): the Core sent
    /// sliceAccessVersion 3, so Take control of a slice its own position
    /// controls is offered like any other. Below 3 the Core refuses it,
    /// and Take control on that slice is shown disabled with
    /// coreSliceTakeUnavailableReason(), the Core's own refusal words.
    bool coreSliceTakeAvailable() const;
    static QString coreSliceTakeUnavailableReason(QChar letter);
    /// Take-over fix wave (M-3; the phone contract: a take-back refused
    /// while the slice transmits "may be tried again"): whether Take it
    /// back on controlTaken `notice`, refused with `reason`, may be tried
    /// again. The Core keeps the take-back in exactly that case: the slice
    /// the notice names is still that slice (`incarnationNow`, -1 when it
    /// is gone) at the control revision the notice named (`revisionNow`).
    /// Never after "That can no longer be taken back."
    static bool controlTakeBackMayBeTriedAgain(const SessionPrompt& notice, const QString& reason,
                                               qint64 incarnationNow, qint64 revisionNow);
    /// `session.leave`, when the Core offers it: the operator is done with
    /// the Core here (Disconnect, or quitting). Sent before the link
    /// closes; nothing waits for its answer.
    quint32 leaveSession();
    /// The reason a held change carries while its question is asked.
    static bool isAwaitingConfirmation(const QString& reason);
    /// session.takeover (iPhone app plan Task 78 item 7, G-53): the answer
    /// to the Core's session.held, with the revision it showed. An empty
    /// id declines, and the Core then ends this session as full. False
    /// when no list is being asked.
    bool answerHeld(const QString& deviceId);
#ifdef NEREUS_BUILD_TESTS
    /// Test seam: a bench link signs in with the token, which never declares
    /// sessionHolder; a window test says it does, and names the id the Core
    /// numbered the token window with.
    void setTokenSessionHolderForTest(const QString& wireId)
    { m_tokenSessionHolderIdForTest = wireId; }
    /// Test seam: with setTokenSessionHolderForTest, the bench link also
    /// declares sliceAccess, as a device-key sign-in always does.
    void setTokenSliceAccessForTest(bool declares) { m_tokenSliceAccessForTest = declares; }
    /// Test seam: the sliceAccess version the hello declares (2), 1 for a
    /// window from before Take it back on controlTaken.
    void setSliceAccessDeclaredForTest(int version) { m_sliceAccessDeclared = version; }
    /// Test seam: an older window, which never declares sessionHolder.
    void setDeclaresSessionHolder(bool declares) { m_declaresSessionHolder = declares; }
#endif
    int coreStationTciStored() const override;
    /// Test seam: whether the Core counts as on this computer (a session
    /// started without a dial has no address to judge by).
    void setCoreOnThisComputerForTest(bool onThisComputer)
    { m_coreOnThisComputerForTest = onThisComputer ? 1 : 0; }
    bool telemetryAvailable() const;
    std::optional<SessionTransportTelemetry> transportTelemetry() const;
    /// Local auxiliary-watch bytes for this logical primary session. An
    /// established session without a watch reports measured zero.
    std::optional<AuxiliaryWatchTelemetry> auxiliaryWatchTelemetry() const;
    bool sendMediaControl(const QJsonObject& payload, quint32 expectedEpoch);

    /// R-R3-28. The media layer calls this once the media session it
    /// started for `expectedEpoch` is ready. When media was negotiated,
    /// this, not the control handshake, is what proves the session works
    /// and resets the reconnect backoff: a Core whose control handshake
    /// succeeds but whose media keeps failing must see its retries slow
    /// down (1, 2, 5 s...) rather than retry at the first step forever.
    /// Without negotiated media the handshake still resets it, as before.
    /// A call naming any epoch but the current one, or made while media
    /// is unavailable, changes nothing, so a late ready from a retired
    /// peer cannot reset a newer session's schedule.
    ///
    /// The media layer also calls it when media ends for good WITHOUT a
    /// retry (Core refused the start, a media error after start, or a
    /// permanent start refusal on this computer): the session then settles
    /// as control only, which the handshake has already proven, so a later
    /// unrelated drop retries at the first step again. No such path
    /// retries, so this cannot restart a fast retry loop.
    void noteMediaEstablished(quint32 expectedEpoch);

    /// R-R3-28. True once an automatic retry has already waited the longest
    /// step of the reconnect backoff (see scheduleReconnect()) since the
    /// schedule last started over. The media layer stops retrying a
    /// transient start refusal from here on.
    bool reconnectBackoffExhausted() const;

    /// Non-zero when the station's AppSettings schema version differs from
    /// this build's. Reported, never a refusal -- see the class comment.
    bool hasSettingsSchemaSkew() const { return m_settingsSchemaSkew; }
    qint32 stationSettingsSchemaVersion() const { return m_stationSettingsSchema; }
    qint32 localSettingsSchemaVersion() const { return m_localSettingsSchema; }

    /// Mirrored property names the station sent that this build's own
    /// MirrorSchema does not carry, and vice versa: schema skew caught by
    /// NAME comparison at handshake, per task 18 step 8. Keyed
    /// "ClassName.propertyName". Empty when the two schemas agree.
    QSet<QByteArray> schemaNamesOnlyOnStation() const { return m_schemaOnlyOnStation; }
    QSet<QByteArray> schemaNamesOnlyLocal() const { return m_schemaOnlyLocal; }

    /// "ClassName.propertyName" for every inbound property none of the
    /// three apply strategies could land. See the class comment.
    QSet<QByteArray> unappliedProperties() const { return m_unapplied; }

    /// The client-side object registry: wire key to live object. Task 19
    /// tears this down on link loss.
    QList<QByteArray> mirroredObjectKeys() const;
    QObject* mirroredObject(const QByteArray& objectKey) const;

    /// iPhone app plan Task 29: the transport beneath the session, which a
    /// move keeps (m_transport). transport() is the connection it carries
    /// now.
    SwitchableTransport* sessionTransport() const;
    /// The transport currently carrying this session, or null. Non-owning,
    /// for tests and diagnostics, matching StationServer's own subsystem
    /// accessors. A caller must not hold this across an event-loop turn:
    /// attachTransport() releases and deletes a superseded one.
    SessionTransport* transport() const;
    /// The separate direct WSS transmit watch has received Core's attach ack.
    bool directWatchReady() const;
    /// Either independent watch route has received its attach ack.
    bool transmitWatchReady() const;

    /// True when this attach either owes no certificate comparison (the
    /// non-TLS transport seam, or an explicit unpinned bench run) or has
    /// already passed one. The pre-shared token is never sent while this
    /// is false; see ensurePinSatisfied() in the .cpp.
    bool isPinSatisfied() const { return m_pinSatisfied; }

    /// Send a command verb (SessionCommandDispatcher's five) to the
    /// station. Returns the commandId the result will echo, or 0 when
    /// there is no session.
    ///
    /// The generic form. Prefer the IStationLink overrides below for the
    /// five verbs that have one: they also REMEMBER the verb and the
    /// slice it named, which is what lets a refusal come back to the
    /// operator on the right signal instead of being counted and
    /// forgotten.
    quint32 invokeCommand(const QByteArray& verb, const QList<MirrorUpdate>& arguments);

    // ── IStationLink: the operator's clicks leaving this process ─────────
    //
    // Attached to the RadioModel in this class's CONSTRUCTOR and detached
    // in its destructor, not at handshake and link loss. The model has a
    // link for the whole life of a remote-mode GUI, and whether a command
    // can actually be sent right now is invokeCommand()'s existing
    // question (no transport, or not yet authenticated) rather than a
    // second piece of attach/detach state to keep in step with it. A
    // click before the first handshake, or during a reconnect backoff,
    // therefore reaches the operator as "the session is not established"
    // rather than doing something locally.
    //
    // Each records {verb, sliceId} against the commandId it returns, so
    // handleCommandResult() can route a refusal to sliceRetuneRejected
    // (rate changes, which carry a slice id) or sliceAddRejected
    // (everything else). See the .cpp for the map's bound.
    CommandOutcome requestAddSlice(const QString& initialPanId) override;
    CommandOutcome requestAddSliceOnPan(const QString& panId) override;
    CommandOutcome requestRemoveSlice(int sliceId) override;
    CommandOutcome requestActiveSlice(int sliceId) override;
    // Parity Task 18 (B3.1): slice.selectBand for a named slice.
    bool bandSelectAvailable() const override;
    /// R-IOS-26 / R-R3-49: the Core knows 2 m as its own band
    /// (band2mVersion 1 at minor 11). Without it a 2 m band button, menu
    /// entry or antenna row cannot reach the Core, and says why.
    bool station2mAvailable() const;
    static QString station2mUnavailableReason();
    QString band2mUnavailableReason() const override;
    CommandOutcome requestSelectBand(int sliceId, int band) override;
    // Slice control plan Task 5 (sliceAccessVersion 1 at minor 11): listen
    // to, stop listening to, take control of and release a slice. The
    // answers arrive on deviceCommandFinished; the access objects follow.
    // setActiveSliceById (requestActiveSlice) also chooses a listened slice
    // as this window's receive slice on such a Core.
    bool remoteSliceAccessAvailable() const override;
    CommandOutcome requestListen(int sliceId, quint64 incarnation) override;
    CommandOutcome requestStopListening(int sliceId, quint64 incarnation) override;
    CommandOutcome requestTakeControl(int sliceId, quint64 incarnation,
                                      quint64 controlRevision) override;
    CommandOutcome requestRelease(int sliceId, quint64 incarnation,
                                  quint64 controlRevision) override;
    // Task 14b: this window's own volume and mute for a listened slice.
    CommandOutcome requestListenLevel(int sliceId, quint64 incarnation, double level,
                                      bool muted) override;
    CommandOutcome requestSliceSampleRate(int sliceId, int rateHz) override;
    CommandOutcome requestStreamCtunPinned(int sliceId, bool pinned) override;
    CommandOutcome requestStreamCentre(int sliceId, double centreHz) override;
    CommandOutcome requestConfigureTgxl(const QString& host, quint16 port) override;
    CommandOutcome requestDisconnectTgxl() override;
    CommandOutcome requestFourO3AEnabled(bool enabled) override;
    CommandOutcome requestConfigurePgxl(const QString& host, quint16 port) override;
    CommandOutcome requestDisconnectPgxl() override;
    CommandOutcome requestPgxlConnectionSettings(bool autoReconnect, int keepaliveSec,
                                                 int pingSec) override;
    CommandOutcome requestConfigureRfKit(const QString& host, quint16 port) override;
    CommandOutcome requestDisconnectRfKit() override;
    bool rfKitSettingsAvailable() const override;
    CommandOutcome requestResetRfKitError() override;
    CommandOutcome requestRfKitEnabled(bool enabled) override;
    CommandOutcome requestStationTci(bool enabled, quint16 port) override;
    // Parity Task 23 (stationTciVersion 2).
    bool stationTciServerAvailable() const override;
    CommandOutcome requestStationTciOptions(bool emulateExpertSdr3, bool emulateSunSdr2Pro,
                                            bool cwluBecomesCw,
                                            bool sendInitialState) override;
    CommandOutcome requestDisconnectStationTciClient(const QString& id) override;
    // JJ's ruling of 2026-09-28 (stationTciSettingsVersion 1).
    bool stationTciSettingsAvailable() const override;
    CommandOutcome requestStationTciSetting(const QByteArray& name,
                                            const QVariant& value) override;
    CommandOutcome requestTxInterlockPolicy(int mode, int graceMs, bool swrGateEnabled,
                                            double swrGateMax) override;
    CommandOutcome requestPgxlPowerCap(bool enabled, int watts) override;
    CommandOutcome requestClearAccessoryFaults(const QString& device) override;
    CommandOutcome requestPgxlName(const QString& name) override;
    CommandOutcome requestPgxlHardware(const QString& setting, const QString& value) override;
    CommandOutcome requestPgxlNetwork(bool dhcp, const QString& address,
                                      const QString& netmask, const QString& gateway) override;
    CommandOutcome requestPgxlSaveAndRestart() override;
    CommandOutcome requestPgxlReadSettings() override;
    CommandOutcome requestTgxlName(const QString& name) override;
    CommandOutcome requestTgxlNetwork(bool dhcp, const QString& address,
                                      const QString& netmask, const QString& gateway) override;
    CommandOutcome requestTgxlSaveAndRestart() override;
    CommandOutcome requestTgxlReadSettings() override;
    CommandOutcome requestTgxlAntenna(int port) override;
    CommandOutcome requestTgxlOperate(bool on) override;
    CommandOutcome requestTgxlBypass(bool on) override;
    CommandOutcome requestTgxlRelayMove(int relay, int direction) override;
    CommandOutcome requestTgxlLanScan() override;
    CommandOutcome requestTgxlAddress(const QString& host, int port) override;
    CommandOutcome requestPgxlOperate(bool on) override;
    CommandOutcome requestPgxlLanScan() override;
    CommandOutcome requestPgxlAddress(const QString& host, int port) override;
    CommandOutcome requestRfKitOperate(bool on) override;
    CommandOutcome requestRfKitAntenna(int port) override;
    CommandOutcome requestRfKitTciMode() override;
    CommandOutcome requestRfKitAddress(const QString& host, int port) override;
    // R-R3-49 (parity Task 2): see IStationLink.
    CommandOutcome requestTunePowerForTxBand(int watts) override;
    // R-R3-49 (parity Task 3): see IStationLink. Sent only to a Core at
    // transmitSettingsVersion 3.
    CommandOutcome requestTxProfileSelect(const QString& name) override;
    CommandOutcome requestTxProfileSave(const QString& name) override;
    CommandOutcome requestTxProfileDelete(const QString& name) override;
    CommandOutcome requestRadeResetVocoder() override;
    // transmitSettingsVersion 15: see IStationLink. Sent only to a Core at
    // transmitSettingsVersion 15.
    CommandOutcome requestCfcProfile(const QString& profileJson,
                                     const QString& expectedRevision) override;
    CommandOutcome requestApplyNnrModels(quint32 revision) override;
    bool nnrControlAvailable() const override;
    // R-R3-21: the Core advertised dspAssetVersion 2 on a session that
    // negotiated DSP control, so its NR3 models can be listed and chosen.
    bool remoteNr3ModelsAvailable() const;
    // R-R3-21 / R-R3-09: the Core advertised notchControlVersion 1 on a
    // session that negotiated DSP control. The window's NotchModel mirrors
    // the Core's list and sends notch.* requests.
    bool remoteNotchControlAvailable() const;
    // R-R3-46 / R-R3-11: the Core advertised radioHardwareVersion 1 on a
    // session at minor 11 with property results. The window's `stepAtt`
    // edits reach the Core's step attenuator and preamp.
    bool remoteRadioHardwareAvailable() const;
    /// Why the window's attenuator and preamp edits cannot reach the Core,
    /// in plain words: not connected yet, or a Core that does not offer
    /// them. Empty while remoteRadioHardwareAvailable().
    QString radioHardwareUnavailableReason() const;
    // R-R3-46: the Core advertised radioHardwareVersion 2 on a session at
    // minor 11 with property results. Hardware Config's receive settings
    // reach the Core (its `alexAntennas` object, the hardware apply step
    // after a settings write, and the I/O board probe).
    bool remoteHardwareConfigAvailable() const;
    /// Why Hardware Config edits cannot reach the Core, in plain words.
    /// Empty while remoteHardwareConfigAvailable().
    QString hardwareConfigUnavailableReason() const;
    /// Group B fix wave (radioHardwareVersion 5): the Core takes this
    /// window's RX bypass on TX (`rxOutOnTx` on `alexAntennas`).
    bool remoteRxBypassOnTxAvailable() const;
    /// Empty while remoteRxBypassOnTxAvailable().
    QString rxBypassOnTxUnavailableReason() const;
    /// Parity Task 12 (radioHardwareVersion 6): the Core takes this
    /// window's transmit antennas and relays on `alexAntennas` (the TX
    /// antenna for each band, Block TX on Ant 2 and 3, Ext 1 and Ext 2 on
    /// TX and the RX bypass relay override).
    bool remoteTransmitAntennasAvailable() const;
    /// Empty while remoteTransmitAntennasAvailable().
    QString transmitAntennasUnavailableReason() const;
    /// Verb "requestIoBoardProbe": probe the Core's radio's HL2 I/O board.
    CommandOutcome requestIoBoardProbe() override;
    /// Parity Task 14: the Core advertised radioHardwareVersion at least
    /// `minVersion` on a session at minor 11 with property results.
    bool radioHardwareAvailable(int minVersion) const override;
    /// Parity Task 14 (radioHardwareVersion 7). Verb "requestIoBoardI2c".
    CommandOutcome requestIoBoardI2c(int bus, int address, int reg, bool write,
                                     int value) override;
    /// Parity Task 14 (radioHardwareVersion 7). Verb "setIoBoardOutput".
    CommandOutcome requestIoBoardOutput(int pin, bool on) override;
    /// Parity ruling C4: the Core offers setRadioSampleRate
    /// (radioHardwareVersion 9).
    bool radioSampleRateAvailable() const override;
    CommandOutcome requestRadioSampleRate(int rateHz) override;
    /// Level Cal: the Core offers resetLevelCalibration
    /// (radioHardwareVersion 12).
    bool levelCalibrationResetAvailable() const override;
    CommandOutcome requestResetLevelCalibration() override;
    /// Level Cal: the Core offers startLevelCalibration and
    /// cancelLevelCalibration (radioHardwareVersion 12).
    bool levelCalibrationRunAvailable() const override;
    CommandOutcome requestStartLevelCalibration(float levelDbm, double frequencyHz,
                                                int sliceId) override;
    CommandOutcome requestCancelLevelCalibration() override;
    /// Level Cal: the Core's stepAtt carries rx2PreampMode
    /// (radioHardwareVersion 12).
    bool rx2PreampModeAvailable() const override;
    /// Parity Task 16 (dspInfoVersion 1). Verb "dsp.filterResponse". The
    /// answer goes to RadioModel::reportStationFilterResponse.
    CommandOutcome requestFilterResponse(int sliceId, bool highResolution) override;
    /// Parity Task 16: minor 11 and dspInfoVersion at least 1.
    bool dspInfoAvailable() const;
    /// Parity Task 19 (R-IOS-25): minor 11 and recordStreamVersion at least
    /// 1 on a ready session.
    bool spotSourcesAvailable() const override;
    /// Parity Task 19. Verbs spots.connect, spots.disconnect,
    /// spots.sendCommand and spots.clearAll; a refusal goes to the spot
    /// source host (SpotSourceHost::reportStationRefusal).
    CommandOutcome requestSpotSource(const QByteArray& verb, const QString& source,
                                     const QString& text) override;
    /// iPhone plan Task 22 / parity Task 20: spotSourcesAvailable() and
    /// stationFreedvVersion at least 1.
    bool stationFreedvAvailable() const override;
    /// Verbs freedv.setMessage, freedv.sendQsy and freedv.setHidden; a
    /// refusal is shown as the Core's other refusals are.
    CommandOutcome requestFreedv(const QByteArray& verb, const QVariantMap& args) override;
    /// Parity Task 22 (R-R3-49): minor 11 and supportBundleVersion at least
    /// 1 on a ready session.
    bool supportBundleAvailable() const override;
    /// Verb support.collect; the answer goes to
    /// RadioModel::reportStationSupportBundle.
    CommandOutcome requestSupportBundle() override;
    /// Verb support.setLogCategories; a refusal goes to
    /// RadioModel::reportStationLogCategoriesRefused.
    CommandOutcome requestLogCategories(const QString& categories) override;
    /// records.subscribe (true) or records.unsubscribe (false) for coreLog.
    void requestCoreLog(bool follow) override;
    /// R-IOS-13 / R-R3-49: spotSourcesAvailable() and txModMonitorVersion
    /// at least 1.
    bool txModMonitorAvailable() const override;
    /// The source the window's Mod Monitor watches (-1 none); subscribes
    /// to its stream now when available, and again after each snapshot.
    void setModMonitorSource(int source) override;
    /// txModMonitor.reset for a source.
    CommandOutcome requestModMonitorReset(int source) override;
    /// Parity Task 21 (R-IOS-18): minor 11 and stationRadiosVersion at
    /// least 1 on a ready session.
    bool stationRadiosAvailable() const override;
    bool settingsHygieneAvailable() const override;
    bool settingsRepairAvailable() const override;
    bool settingsBackupExportAvailable() const override;
    CommandOutcome requestSettingsBackupExport() override;
    void cancelSettingsBackupExport(quint32 operationId = 0) override;
    CommandOutcome requestSettingsHygiene(const QByteArray& verb, const QString& mac) override;
    /// Fix wave (I5): this session signed in with this computer's own key.
    bool signedInWithDeviceKey() const override
    { return m_deviceKeySignInForTest >= 0 ? m_deviceKeySignInForTest == 1 : m_signedInWithDeviceKey; }
    /// Follow-up N1: this token sign-in enrolled this computer's key.
    bool enrolledDeviceKeyThisSession() const override
    { return m_enrolledKeyForTest >= 0 ? m_enrolledKeyForTest == 1 : m_enrolledDeviceKey; }
#ifdef NEREUS_BUILD_TESTS
    /// Test seam: a bench link (no TLS pin) never signs in by key; a window
    /// test says it did (see StationServer::setTokenSessionsMayChangeRadioForTest).
    void setSignedInWithDeviceKeyForTest(bool byKey) { m_deviceKeySignInForTest = byKey ? 1 : 0; }
    /// Test seam: a bench link cannot enrol its key either; a window test
    /// says this token sign-in did (follow-up N1).
    void setEnrolledDeviceKeyForTest(bool enrolled) { m_enrolledKeyForTest = enrolled ? 1 : 0; }
    /// Test seam: the next hello leaves out `feature`, as a window built
    /// before it did (a Core then answers as it would that window).
    void withholdFeatureForTest(const QByteArray& feature) { m_declaredFeatures.remove(feature); }
#endif
    /// iPhone app plan Task 25: minor 11, deviceAdminVersion (or
    /// pairingVersion) at least 1, and this session signed in with this
    /// computer's own key.
    bool deviceAdminAvailable() const override;
    bool pairingAvailable() const override;
    /// Verbs devices.revoke (`id`), station.acknowledgeKeyBackup,
    /// pairing.open and pairing.close.
    CommandOutcome requestDeviceAdmin(const QByteArray& verb, const QString& id) override;
    /// Parity Task 21. Verbs station.selectRadio, station.rescanRadios,
    /// station.setRadioModel and station.forgetRadio.
    CommandOutcome requestStationRadio(const QByteArray& verb, const QString& mac,
                                       int model) override;
    /// R-R3-46 fix wave (radioHardwareVersion 3). Verb "setAlexRxAntenna":
    /// one band's RX antenna (rxOnly false, 1..3) or RX-only antenna
    /// (rxOnly true, 0..3) on the Core.
    CommandOutcome requestAlexRxAntenna(Band band, int antenna, bool rxOnly);
    /// Parity mini-round (radioHardwareVersion 6). Verb "setAlexTxAntenna":
    /// one band's TX antenna (1..3) on the Core.
    CommandOutcome requestAlexTxAntenna(Band band, int antenna);
    /// R-R3-46 / R-R3-21 (radioHardwareVersion 4). Verb "setAlexBpfMode":
    /// one receive filter chain's filter policy on the Core.
    bool filterPolicyEditAvailable() const override;
    QString filterPolicyUnavailableReason() const override;
    CommandOutcome requestFilterPolicy(int chain, int mode) override;
    CommandOutcome requestNnrDiagnostics(int sliceId, int testMode, int outputMode) override;
    /// R-R3-40: the station can clear a runtime NNR limit on request
    /// (negotiated minor 11 and NNR control).
    bool nnrRetryAvailable() const;
    /// R-R3-40: ask the station for the slice's saved NNR choice back
    /// ("Try again", or choosing a model while limited). Sent for the
    /// operator's own action only, never for a station echo.
    CommandOutcome requestNnrRetry(int sliceId);

    /// iPhone app plan, desktop remote transmit (R-IOS-13): the transmit
    /// verbs (link section 18.6). Available while the session is up and
    /// the Core told this window remoteTxVersion 1 or later.
    RemoteTransmitClient* remoteTransmit() override { return m_remoteTransmit; }
    bool remoteTransmitAvailable() const;

    void setHeartbeatIntervalMs(int ms);
    int heartbeatIntervalMs() const { return m_heartbeatIntervalMs; }
    void setMaxMissedPongs(int misses);
    int maxMissedPongs() const { return m_maxMissedPongs; }

    /// R-R3-16/17: the operator reason recorded when the handshake deadline
    /// expires. Plain English on purpose; it reaches the link-lost toast
    /// and the Core connection status unchanged.
    static QString handshakeDeadlineReason();

    /// R-R3-16/17: how long an attached transport may take to finish the
    /// connect sequence (snapshot-complete marker included) before this
    /// client closes it and schedules the next attempt with the normal
    /// backoff. Production default kStationHandshakeDeadlineMs, shared with
    /// StationServer. Values below 1 disable it. Applies from the next
    /// attach; a test shrinks it so no real 30 s wait is needed.
    void setHandshakeDeadlineMs(int ms);
    int handshakeDeadlineMs() const { return m_handshakeDeadlineMs; }

signals:
    /// Parity Task 33: the Core's CFC display, one value per bin
    /// (TxChannel::kCfcDisplayBinCount, in dB to a tenth), read at
    /// `atMs` on the Core's clock.
    void cfcCompressionReceived(const QList<double>& binsDb, qint64 atMs);
    /// iPhone app plan Task 25: whether the Core's `vax` object is held
    /// (stationVaxAvailable()) changed.
    void stationVaxAvailabilityChanged();
    /// iPhone app plan Task 78: a several-devices verb was answered
    /// (tx.take, confirm.proceed, confirm.cancel, notice.takeBack; slice
    /// control plan Task 5: slice.listen, slice.stopListening,
    /// slice.takeControl, slice.release).
    /// `awaitingConfirmation` when the Core asked a question instead (it
    /// follows as a confirm.request).
    void deviceCommandFinished(const QByteArray& verb, quint32 commandId, bool accepted,
                               const QString& reason, bool awaitingConfirmation);
    /// Slice control plan Task 5: a change to slice `sliceId` was held back
    /// here because this window only listens to it; `reason` is the Core's
    /// listener words. Nothing was sent.
    void sliceAccessHeld(int sliceId, const QString& reason);
    /// The holder's rules or this window's ability to take transmit changed.
    void transmitTakeAvailabilityChanged();
    /// Fix wave M6: voxArmedHere() changed.
    void voxArmedHereChanged(bool armed);
    void displayBudgetChanged();
    void ps3DisplaySubscriptionRequested(bool enabled);
    /// Published before transport callbacks can deliver a synchronous reply.
    void ps3DisplaySubscriptionStarted(quint32 commandId, bool enabled);
    void ps3DisplaySubscriptionFinished(quint32 commandId, bool enabled,
                                        bool accepted, const QString& reason);
    void propertyWriteCompleted(const QByteArray& objectKey, const QByteArray& property,
                                quint32 writeId, bool accepted, const QString& reason);
    /// Refresh connection controls after a dial, closure or retry cancellation.
    void connectionActivityChanged();
    /// iPhone app plan Task 27: connectionAttempt() changed.
    void connectionAttemptChanged();
    void mediaControlReceived(const QJsonObject& payload, quint32 epoch);
    /// Retires media even when a deliberate redial silently supersedes a link.
    void mediaSessionEnded(quint32 epoch);
    void telemetryReceived(const StationTelemetrySnapshot& snapshot, quint32 epoch);
    void telemetrySessionEnded(quint32 epoch);
    /// The full section 7.0 sequence completed, snapshot-complete marker
    /// included. Parent section 12.2 gates TX on exactly this point.
    void handshakeComplete();
    /// Every full state publication, including reseeding an existing session.
    /// Does not recreate media or reset the session epoch.
    void stateSnapshotApplied();

    /// The session ended, with the station's own reason where it gave one
    /// (a version refusal, a failed authentication, or being displaced by
    /// a newer connection).
    void sessionEnded(const QString& reason);

    /// A CommandResult came back. `commandId` matches invokeCommand()'s
    /// return value.
    void commandResponse(const NereusSDR::SessionMessage& message);
    void commandResult(quint32 commandId, bool accepted, const QString& reason);
    void streamCtunPinFinished(int sliceId, quint64 streamEpoch, bool pinned, bool accepted);
    void streamCentreFinished(int sliceId, quint64 streamEpoch, bool accepted);

    /// The heartbeat declared the station dead: it stopped answering pings
    /// without closing.
    void stationHeartbeatTimeout();

    /// Task 19. A reconnect attempt was scheduled after a retry-eligible
    /// closure. attemptNumber starts at 1; delayMs is the actual delay
    /// this attempt will wait (reconnectBackoffUnitMs()-scaled). Mirrors
    /// PgxlConnection::reconnectAttempt's shape for a future GUI
    /// ("Reconnecting... attempt 3, retrying in 10s"); see the class
    /// comment for why the underlying timer mechanism is NOT copied from
    /// that class.
    void reconnectScheduled(int attemptNumber, int delayMs);

    /// iPhone app plan Task 29 (link section 21.2): the session moved to a
    /// better path (pathRank()), without ending. A window with media moves
    /// its media too (the media `replace`).
    void pathChanged();

    /// iPhone app Task 18 (R-IOS-08): a token sign-in enrolled this
    /// computer's device key with the Core whose identity has this
    /// fingerprint (32 bytes). From now on this client trusts that Core by
    /// the key and signs in with its own; the caller saves it with the
    /// Core so later connections do the same.
    void stationIdentityLearned(const QByteArray& identityFingerprint);

private:
    void attachTransport(SessionTransport* transport, const QString& token);

    /// The actual dial: latches url/token/expectedFingerprint/allowUnpinned
    /// (so onReconnectTimeout() can redial identically), creates the
    /// QWebSocket, wires its error paths (guarded against a stale prior
    /// socket -- see the class comment), and attaches it. connectToStation()
    /// is the deliberate-fresh-attempt entry (it resets m_reconnectAttempts
    /// and applies the empty-fingerprint refusal before ever reaching
    /// here); onReconnectTimeout() is the automatic-retry entry (it must
    /// NOT reset the attempt counter, or the backoff would never advance
    /// past its first step).
    /// Task 28: one attempt through the service (connectThroughService()
    /// and each retry).
    void dialThroughService();
    void stopServiceDial();
    /// iPhone app plan Task 29 (link section 21.1): a paired Core's race,
    /// its winner, its failure and its record.
    void startRace();
    void stopRace();
    void adoptRaceWinner(const PathRacer::Ready& ready);
    void onRaceFailed(const QString& reason);
    void syncAttemptFromRace(const PathRacer* racer);
    /// Link section 21.3: looking for a better path, and moving to it.
    void scheduleUpgrade(bool advance);
    void startUpgradeRace();
    /// Task 29 fix wave (review Minor 7): a session through the service
    /// takes its rank from the pair its connection settled on now (the
    /// agent may nominate a relayed pair first and a direct one later).
    void refreshPathRank();
    void beginUpgrade(PathRacer::Ready ready);
    void abandonUpgrade(const QString& why, bool reschedule);
    void onPathTicket(const SessionMessage& result);
    void onPathSwitched();
    bool canMovePathNow() const;
    PathRacer* newRacer(bool upgrade);
    void dialStation(const QUrl& url, const QString& token,
                     const QString& expectedFingerprint, bool allowUnpinned,
                     const QByteArray& stationIdentityFingerprint);

    /// iPhone app Task 18: the sign-in after the pin (or the identity)
    /// holds: this hello, then `auth.request` with the token, the device
    /// block, or both. Returns false when it ended the attempt instead.
    bool signIn(const SessionMessage& hello);
    /// The paired Core's checks (identity key, certificate binding),
    /// ending the attempt with identityChanged when one fails. The Core's
    /// key (SPKI DER) and this connection's certificate SHA-256 on
    /// success.
    bool verifyStationIdentity(const SessionMessage& hello, const QByteArray& expected,
                               QByteArray* stationSpki, QByteArray* certSha256);
    /// Ends this attempt, not retryably, with this app's own end `kind`
    /// and `code`, before anything was sent.
    void refuseStation(const QString& reason, StationEndReport::Kind kind,
                       const QString& code);

    /// Compares the station's presented certificate against the pinned
    /// fingerprint, exactly once per attach, and ends the session
    /// (non-retryably) when it does not match or when there is no
    /// certificate to compare. Returns true when this attach may proceed
    /// to send the token.
    ///
    /// Called from THREE places on purpose. The sslErrors handler surfaces
    /// a mismatch at the earliest possible moment. The connected() handler
    /// covers every handshake that reported no errors at all, which is the
    /// case the original code never checked and the case pinning exists
    /// for. handleHello() gates the one line that actually sends the
    /// token, so the property holds regardless of which handler ran.
    bool ensurePinSatisfied();

    void onTransportText(const QByteArray& wire);
    void onTransportClosed();

    /// The single place a session ends. Emits sessionEnded() AT MOST ONCE
    /// per attachTransport(), whichever of the six paths reached it (peer
    /// close, socket error, heartbeat timeout, the station's own
    /// SessionEnd, a version refusal, an auth refusal). Idempotent: a
    /// second call for the same attach returns without emitting. Fix
    /// round 1, Minor 7: worded "at most once", not "exactly once" -- an
    /// attach that is SUPERSEDED by a fresh attachTransport() before its
    /// own endSession() ever runs (a reconnect landing on top of a stale
    /// transport that never got the chance to report) is released
    /// silently, with no sessionEnded for it at all.
    /// staleTransportErrorDoesNotTearDownAFreshlyAttachedSession in
    /// tst_session_link_loss.cpp pins exactly this: nine superseded
    /// attaches, zero sessionEnded emissions for any of them.
    /// `attemptReconnect` (Task 19) is disconnectFromStation()'s own
    /// parameter, threaded through: true arms the reconnect timer once
    /// the mirror teardown and connection-state transition below have
    /// run; false leaves it untouched (already stopped, or never started).
    void endSession(const QString& reason, bool attemptReconnect,
                    bool reportSessionEnd = true);
    void onHeartbeatTick();
    void onWriteFlushTick();
    void onHandshakeDeadline();

    /// Task 19. Arms m_reconnectTimer at the current backoff step and
    /// advances the step for next time. Only ever called from endSession()
    /// and only when there is a latched URL to redial.
    void scheduleReconnect();
    void onReconnectTimeout();

    void handleHello(const SessionMessage& message);
    void handleAuthResult(const SessionMessage& message);
    void handleCapabilities(const SessionMessage& message);
    void handleSettingsSnapshot(const SessionMessage& message);
    void handleSchema(const SessionMessage& message);

    /// The NAME comparison itself, shared by handleSchema() (when an
    /// instance of the class already exists) and handleObjectCreate()
    /// (when the schema arrived first, which is always the case for
    /// slices).
    void compareSchema(const QByteArray& className, const QSet<QByteArray>& stationNames,
                       const QMetaObject* mo);
    /// Remove every client-side slice the station's just-finished
    /// snapshot did not name. Called from the SnapshotComplete arm, and
    /// only from there -- see the .cpp for why no earlier point can
    /// answer the question this asks.
    void reconcileSlicesAgainstStation();

    void handleObjectCreate(const SessionMessage& message);
    void handleObjectDestroy(const SessionMessage& message);
    void handleDelta(const SessionMessage& message);
    void handlePropertyResult(const SessionMessage& message);
    bool propertyResultsAvailable() const;

    /// The station's verdict on a command this client sent. Clears the
    /// pending entry either way, and puts a refusal in front of the
    /// operator through the RadioModel signal that already reaches
    /// MainWindow's toast for that kind of refusal.
    void handleCommandResult(const SessionMessage& message);
    void handleSettingsBackupExportResult(const SessionMessage& message);
    void requestNextSettingsBackupChunk();
    void finishSettingsBackupExport(bool accepted, const QString& reason,
                                    const QByteArray& xml = {}, bool cancelRemote = false);

    /// Shared tail of the five IStationLink overrides: send, remember,
    /// and turn "there is no session" into a sentence an operator can
    /// read. `sliceId` is -1 for the verbs that name no slice.
    CommandOutcome sendCommand(const QByteArray& verb, int sliceId,
                               const QList<MirrorUpdate>& arguments,
                               const QString& action);
    void handleSettingsValue(const SessionMessage& message);
    void handleSettingsReject(const SessionMessage& message);

    /// What an inbound apply does to a pending write it moves as a side
    /// effect. Restore puts the operator's value back (a Core answer to
    /// this window's own write, and an object.create). Delta does too,
    /// except for the unsent edits a cause it applies defines
    /// (kDeltaCancelRules in StationClient.cpp): those are cancelled.
    enum class SideEffectRule { Restore, Delta };
    /// Inbound apply for one object, under the echo guard. See the class
    /// comment's three-strategy list. `heldValues` are the values a delta
    /// carried for properties it skipped because they were pending; one
    /// applies when its unsent edit is cancelled.
    void applyUpdates(QObject* target, const QByteArray& objectKey,
                      const QList<MirrorUpdate>& updates,
                      SideEffectRule rule = SideEffectRule::Restore,
                      const QList<MirrorUpdate>& heldValues = {});
    /// After an inbound apply: every property with a pending write whose
    /// live value the apply moved as a side effect (it was not one of
    /// `applied`, the properties the Core's message named for
    /// `appliedKey`) gets the operator's value back, under the echo
    /// guard. Oldest operator change first, so the newest one wins.
    void restoreOperatorValues(const QByteArray& appliedKey,
                               const QSet<QByteArray>& applied);
    /// Cancels the unsent edit of `prop`: its coalesced write goes, and
    /// either its write in flight comes back (value and writeId, so that
    /// write's answer applies) or the hold ends and the delta's value
    /// (`heldValues`), else the last value the Core sent, applies.
    void cancelUnsentEdit(const QByteArray& objectKey, QObject* object,
                          const MirrorProperty& prop,
                          const QList<MirrorUpdate>& heldValues);
    bool applyOne(QObject* target, const MirrorProperty& prop, const MirrorUpdate& update);

    /// Client-side adapter for daemon-to-client-only properties whose
    /// applyMirroredValue hook correctly refuses on the daemon side.
    /// Returns false when this class has no adapter for the property.
    bool applyClientOnlyProperty(QObject* target, const QByteArray& className,
                                 const QByteArray& propertyName, const QVariant& native);

    /// Resolve a wire object key onto a live client-side object, creating
    /// a slice when the key names one this client does not hold yet.
    QObject* resolveOrCreate(const QByteArray& objectKey, const QByteArray& className);

    void send(const SessionMessage& message);
    void watchForOutbound(const QByteArray& objectKey, QObject* object);

    const SessionPurpose m_sessionPurpose;
    std::function<bool()> m_admissionGuard;
    CandidateSource m_candidateSource;
    quint64 m_connectionRequestGeneration = 0;
    QPointer<RadioModel> m_radioModel;
    quint32 m_hygieneValidateId{0};
    quint32 m_hygieneValidateEpoch{0};
    QString m_hygieneValidateMac;
    bool m_hygieneValidateDirty{false};
    QHash<quint32, QPair<quint32, QString>> m_hygieneMutations;
    void refreshSettingsHygiene();
    void handleSettingsHygieneResult(const SessionMessage& message);
    QPointer<SettingsProxy> m_settingsProxy;
    SessionTransport* m_transport = nullptr;

    QString m_token;
    QString m_lastError;
    StationEndReport m_lastEndReport;
    bool m_handshakeComplete = false;
    bool m_authenticated = false;
    bool m_signedInWithDeviceKey = false;
    // R-IOS-13 / R-R3-49: the Mod Monitor's source the window wants (-1
    // none), and the stream this session is subscribed to (empty none).
    int m_modMonitorSource = -1;
    QString m_modMonitorStream;
    void syncModMonitorSubscription();
    /// Task 78: this session's hello declared sessionHolder 1.
    bool m_declaredSessionHolder = false;
    bool m_declaresSessionHolder = true;
    QString m_tokenSessionHolderIdForTest;
    bool m_tokenSliceAccessForTest = false;
    /// Take-over parity: the sliceAccess version the hello declares.
    /// Core-slice take-over: 3, the Core's own slice may be taken.
    int m_sliceAccessDeclared = 3;
    RemoteDevicesState* m_remoteDevices = nullptr;
    /// Task 78 item 3: the device that took this window's place, from the
    /// end that stopped it, so the next session.held starts on it (Take it
    /// back). Cleared once a session is let in.
    QString m_takeBackDeviceId;
    SliceAccessMirror* m_sliceAccess = nullptr;
    QString m_radioChangeReason;
    int m_deviceKeySignInForTest = -1;
    bool m_enrolledDeviceKey = false;
    int m_enrolledKeyForTest = -1;
    quint16 m_agreedMinor = 0;
    // Parity Task 33: this window shows the CFC bar chart.
    bool m_cfcCompressionWanted = false;
    void sendCfcCompressionSubscription(bool subscribe);

    /// iPhone app Task 4: this client's link majors (oldest first) and
    /// features, and what the current station's hello declared.
    QList<quint16> m_supportedMajors;
    QHash<QByteArray, int> m_declaredFeatures;
    /// Desktop remote transmit: owned (child).
    RemoteTransmitClient* m_remoteTransmit = nullptr;
    void refreshRemoteTransmit();
    bool directWatchEligible() const;
    bool relayWatchEligible(bool newAdmission) const;
    void requestWatchAttempt();
    void startRelayWatch();
    void handleRelayWatchOffer(const QString& sdp, const QString& type,
                               DataChannelTransport* peer, quint64 generation,
                               quint32 sessionEpoch);
    void handleRelayWatchResult(const SessionMessage& message);
    void bindWatchClient(TxWatchClient* watch, quint64 generation, quint32 sessionEpoch);
    void requestDirectWatchTicket();
    void handleDirectWatchTicket(const SessionMessage& message);
    void retireDirectWatch();
    void retryDirectWatch(const QString& reason);
    QPointer<TxWatchClient> m_directWatch;
    // Final snapshots of retired attempts in the current primary epoch.
    AuxiliaryWatchTelemetry m_retiredWatchTelemetry;
    QPointer<DataChannelTransport> m_pendingWatchRelayPeer;
    std::shared_ptr<RelayLeg> m_watchRelayLeg;
    QTimer* m_directWatchRetryTimer = nullptr;
    QTimer* m_directWatchTicketTimer = nullptr;
    QElapsedTimer m_directWatchClock;
    qint64 m_lastDirectWatchRequestMs = -1000;
    quint64 m_directWatchGeneration = 0;
    quint32 m_directWatchTicketId = 0;
    quint64 m_directWatchTicketGeneration = 0;
    quint32 m_directWatchTicketSessionEpoch = 0;
    bool m_directWatchDeclared = false;
    bool m_watchRelayDeclared = false;
    bool m_watchIsRelay = false;
    bool m_watchPreparing = false;

    /// iPhone app Task 18: this computer's device key and name, the paired
    /// Core's identity fingerprint this client trusts (latched across
    /// redials; empty for a pin-trusted Core), and the identity a token
    /// sign-in in flight is enrolling with (empty when none is).
    std::shared_ptr<const ClientDeviceIdentity> m_deviceIdentity;
    QString m_deviceName;
    QString m_deviceShortName;
    QByteArray m_stationIdentity;
    QByteArray m_enrollingIdentity;
    quint16 m_agreedMajor = 0;
    QHash<QByteArray, int> m_stationFeatures;

    StationCapabilities m_capabilities;

    qint32 m_localSettingsSchema = 0;
    qint32 m_stationSettingsSchema = 0;
    bool m_settingsSchemaSkew = false;

    /// Station schemas that arrived before any instance of their class
    /// existed here, waiting for one. See handleSchema().
    QHash<QByteArray, QSet<QByteArray>> m_pendingStationSchemas;

    QSet<QByteArray> m_schemaOnlyOnStation;
    QSet<QByteArray> m_schemaOnlyLocal;
    QSet<QByteArray> m_unapplied;
    // Object keys whose deltas this session dropped (logged once each).
    QSet<QByteArray> m_unheldDeltaKeys;

    QHash<QByteArray, QPointer<QObject>> m_objects;

    /// The outbound half. StateMirror is reused rather than reimplemented:
    /// its propertiesChanged() signal (tasks 7-8) fires for every watched
    /// object regardless of whether a session was ever attached, which is
    /// exactly the observer this direction needs. attachSession() is never
    /// called on it -- that is the DAEMON's connect-time burst, and a
    /// client has nothing to burst.
    StateMirror* m_outboundMirror = nullptr;
    // iPhone app plan Task 39: the Core's `txState` (Qt-parented to this).
    TransmitState* m_transmitState = nullptr;
    // iPhone app plan Task 25: the Core computer's VAX, and whether this
    // window shows its meters.
    StationVax* m_stationVax = nullptr;
    bool m_stationVaxHeld = false;
    bool m_stationVaxLevelsWanted = false;
    void sendStationVaxLevelsSubscription(bool subscribe);
    MirrorCoalescer m_outboundCoalescer;

    /// True for the duration of one inbound apply. See the class comment's
    /// echo-guard section.
    bool m_applyingInbound = false;
    bool m_voxArmedHere = false;   // fix wave M6

    /// True once the snapshot-complete marker has arrived. Until then no
    /// local change is forwarded: everything moving is the station's own
    /// burst landing, and forwarding any of it would tell the station its
    /// own state back.
    bool m_forwardLocalChanges = false;

    /// True from attachTransport() until endSession() reports. What makes
    /// "exactly one sessionEnded per attach" true, and what makes a FAILED
    /// INITIAL CONNECT reportable at all: the previous shape gated the
    /// emit on m_handshakeComplete, so a station that was simply down
    /// produced no signal whatsoever.
    bool m_sessionActive = false;

    /// True once a frame has actually arrived from the station. Gates the
    /// heartbeat: a wss dial can take seconds, and counting missed pongs
    /// across a socket that has not finished connecting reports a slow
    /// dial as a dead station.
    bool m_linkUp = false;

    QTimer* m_heartbeatTimer = nullptr;
    QTimer* m_writeFlushTimer = nullptr;

    /// R-R3-16/17. Owned single-shot, armed by attachTransport() and stopped
    /// by the snapshot-complete marker or endSession(). The heartbeat cannot
    /// cover this window: it starts on the station's first frame, and the
    /// 2026-09-23 incident stalled before one ever arrived.
    QTimer* m_handshakeDeadlineTimer = nullptr;
    int m_handshakeDeadlineMs = kStationHandshakeDeadlineMs;
    int m_heartbeatIntervalMs = kDefaultHeartbeatIntervalMs;
    bool m_mediaTunnelInUse = false;
    int m_maxMissedPongs = kDefaultMaxMissedPongs;
    int m_pingsAwaitingPong = 0;

    quint32 m_nextCommandId = 1;
    struct SettingsBackupExportPending {
        quint32 operationId = 0;
        quint32 expectedCommandId = 0;
        quint32 sessionEpoch = 0;
        QByteArray expectedVerb;
        QByteArray transferId;
        SettingsBackupTransferAssembler assembler;
    };
    std::optional<SettingsBackupExportPending> m_settingsBackupExport;
    // Cancellation before begin's reply still retires a server snapshot if
    // that reply eventually supplies its transfer ID.
    std::optional<QPair<quint32, quint32>> m_cancelledSettingsBackupBegin;
    QTimer* m_settingsBackupReplyTimer = nullptr;
    QTimer* m_settingsBackupOverallTimer = nullptr;
    quint32 m_nextPropertyWriteId = 1;
    /// One property the operator changed that the Core has not answered.
    /// writeId zero marks an edit waiting for the coalescer; nonzero marks
    /// its most recent sent batch. Both protect the value from an older
    /// answer. `value` is the operator's value (what the observer read
    /// when the property went dirty, then what the flush sent); `order`
    /// is when the operator last changed it. An unsent edit made while
    /// an earlier write of the same property was still on its way keeps
    /// that write's id and value (inFlightWriteId, inFlightValue) until
    /// its answer arrives or the edit is sent.
    struct PendingWrite {
        quint32 writeId = 0;
        MirrorUpdate value;
        quint64 order = 0;
        quint32 inFlightWriteId = 0;
        MirrorUpdate inFlightValue;
    };
    /// Object key -> property name -> its pending write.
    QHash<QByteArray, QHash<QByteArray, PendingWrite>> m_pendingWrites;
    quint64 m_nextPendingWriteOrder = 1;
    /// Object key -> property name -> the last value the Core sent for
    /// it (object.create, delta, or a result's value). A delta that
    /// cancels an unsent edit without carrying the property applies it.
    QHash<QByteArray, QHash<QByteArray, MirrorUpdate>> m_coreValues;

    /// What each in-flight command was about, keyed by the commandId the
    /// station echoes back. A CommandResult carries the verb but no slice
    /// id, and a REFUSAL carries no affectedKeys either, so the id the
    /// operator's refusal message needs exists only here.
    struct PendingCommand {
        QByteArray verb;
        int sliceId = -1;
        quint64 streamEpoch = 0;
        bool requestedPin = false;
        // clearAccessoryFaults: which device's history (L1 routing).
        QString faultsDevice;
        // spots.*: which spot source (parity Task 19), for its refusal.
        QString spotSource;
    };
    QHash<quint32, PendingCommand> m_pendingCommands;
    /// Take-over fix wave (M-3): notice.takeBack commands for controlTaken
    /// notices, by command id, to their notice ids.
    QHash<quint32, qint64> m_controlTakeBacks;
    std::optional<QPair<quint32, bool>> m_pendingPs3Display;

    // ---- Task 19: stale state and session epoch ----

    /// See isStale(). Once true, never cleared: it records that a session
    /// has EVER fully established, not that one is established now.
    bool m_everConnected = false;

    /// Rework follow-up 4 (R-R3-48): this link has applied a settings
    /// snapshot (reset on every attach and link loss), so a rule about the
    /// Core's settings reads this Core's, not a previous one's.
    bool m_settingsSnapshotThisLink = false;
    /// Whether this link's Core keeps StationTci_Enabled (from its snapshot
    /// and later changes; the proxy's cache may still hold a previous
    /// Core's keys).
    bool m_coreKeepsTciSwitch = false;

    /// See sessionEpoch(). Bumped in attachTransport().
    quint32 m_sessionEpoch = 0;
    quint32 m_lastTelemetrySequence = 0;
    qint64 m_lastTelemetrySampleElapsedMs = -1;

    // ---- Task 19: automatic reconnect ----

    /// Latched by dialStation() so onReconnectTimeout() can redial
    /// identically. Invalid (QUrl().isValid() == false) for a session
    /// established via startSession() -- the transport-seam path used by
    /// every non-TLS test in this suite and by this task's own
    /// tst_session_link_loss.cpp for the mirror-teardown and stale-state
    /// assertions -- because startSession() explicitly clears it (fix
    /// round 1, Minor 3): a client that once dialed via connectToStation()
    /// and later runs a startSession()-based seam session would otherwise
    /// keep the STALE latch from the earlier real dial, and a
    /// retry-eligible close of the seam session would silently redial that
    /// unrelated earlier target. The adopted entry also clears paired-race
    /// ownership and service routes. endSession() checks the owned URL or
    /// paired race before arming a retry: an adopted link has neither.
    QUrl m_lastUrl;
    /// iPhone app plan Task 27: the addresses one attempt tries in order
    /// (the cached ones, then the one asked for), where it stands, what it
    /// was asked with, and its record.
    QList<QUrl> m_cachedAddresses;
    QList<QUrl> m_dialPlan;
    /// Task 28: the service route of the last connectThroughService(),
    /// what a retry dials again; empty for a WebSocket connection.
    QList<QUrl> m_serviceServers;
    QString m_serviceStationId;
    QPointer<RendezvousDialer> m_serviceDialer;
    int m_serviceDialDeadlineMs = 0;
    // iPhone app plan Task 29.
    ServiceRoute m_serviceRoute;
    int m_serviceAnswerDeadlineMs = 0;
    bool m_raceMode = false;
    QPointer<PathRacer> m_racer;
    QPointer<PathRacer> m_upgradeRacer;
    QUrl m_raceWinnerUrl;
    int m_raceWinnerLine = -1;
    QList<PathRacer::Line> m_raceLines;
    int m_pathRank = -1;
    int m_pathSwitches = 0;
    QString m_stationRendezvousId;
    QTimer* m_upgradeTimer = nullptr;
    int m_upgradeAttempt = 0;
    QList<int> m_upgradeScheduleMs;
    std::optional<PathRacer::Ready> m_upgrade;
    quint32 m_pathTicketCommandId = 0;
    QTimer* m_upgradeDeadline = nullptr;
    /// The ICE settings of the connection through the service this session
    /// left, so its media keeps the service's STUN server (link 21.3).
    std::optional<IceConfiguration> m_serviceIce;
    /// The last STUN server a session through the service used (the
    /// direct media ladder's fallback); never saved.
    mutable std::optional<IceServerAddress> m_lastServiceStun;
    /// Task 29 step 2b: the media tunnel on this session's transport.
    std::shared_ptr<MediaTunnel> m_mediaTunnel;
    bool m_serviceDialing = false;
    int m_dialIndex = 0;
    QString m_planToken;
    bool m_transportOpened = false;
    QTimer* m_openTimer = nullptr;
    StationConnectionAttempt m_attempt;
    QUrl m_connectedUrl;
    /// Moves on to the next address of this attempt, recording how the
    /// current one ended. False when none is left.
    bool advanceDialPlan(StationConnectionAttempt::Outcome outcome);
    void recordOutcome(StationConnectionAttempt::Outcome outcome);
    void startDialPlan();
    // R-R3-48: -1 judge by m_lastUrl; 0 or 1 set by a test.
    int m_coreOnThisComputerForTest = -1;
    QString m_lastFingerprint;
    bool m_lastAllowUnpinned = false;

    /// Whether THIS attempt owes a certificate comparison before it may
    /// send the token, and whether it has passed one. Set by
    /// dialStation()/startSession() and turned into m_pinSatisfied by
    /// attachTransport(), per attach, never carried across a reconnect:
    /// a redial gets a fresh handshake and possibly a different
    /// certificate. Default true on m_pinSatisfied so a client that has
    /// never dialed is not in a refusing state.
    bool m_pinRequired = false;
    bool m_pinSatisfied = true;

    /// Owned, single-shot, cancellable. NEVER static QTimer::singleShot --
    /// see the class comment's link-loss section for why that matters more
    /// here than in the in-tree reference (PgxlConnection/TgxlConnection)
    /// that gets it wrong.
    QTimer* m_reconnectTimer = nullptr;

    /// How many consecutive retry-eligible failures have happened since
    /// the last PROVEN success (handshakeComplete, or with negotiated media
    /// noteMediaEstablished(), R-R3-28) or the last DELIBERATE fresh
    /// connectToStation() call -- both reset this to 0. Indexes
    /// scheduleReconnect()'s backoff schedule.
    int m_reconnectAttempts = 0;
    int m_reconnectBackoffUnitMs = kDefaultReconnectBackoffUnitMs;
};

} // namespace NereusSDR
