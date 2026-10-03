#pragma once
// =================================================================
// src/core/session/StationServer.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 18.
//
// The daemon half of the wss session. Everything tasks 7 through 17 built
// converges here: this is the class that puts StateMirror, ObjectRegistry,
// SessionCommandDispatcher and SettingsProxyServer on a socket, behind
// task 17's TLS certificate and task 18's own TokenStore.
//
// ── THE CONNECT SEQUENCE (parent design section 7.0) ─────────────────────
//
//   TLS establish
//     -> protocol hello carrying a semantic version from both ends
//     -> authentication
//     -> capability exchange
//     -> state snapshot
//     -> snapshot-complete marker
//
// Concretely, per accepted connection, the daemon:
//
//   1. sends Hello (its own major.minor plus its AppSettings schema
//      version) the moment the socket is up, so a client can refuse
//      without ever revealing that it holds a token;
//   2. waits for the client's Hello, and REFUSES on a major mismatch with
//      a reason naming BOTH versions (section 7.0: "the connection is
//      refused with a message naming both versions rather than failing
//      obscurely"). Equal major with a differing minor NEGOTIATES DOWN to
//      the lower of the two and records it as agreedMinor();
//   3. waits for AuthRequest and runs it past TokenStore, which is
//      rate-limited (section 7.1);
//   4. sends AuthResult, then Capabilities;
//   5. sends the settings snapshot (SettingsProxyServer::buildSnapshot);
//   6. attaches the state mirror, which sends a schema per class, an
//      object.create per live object, and the snapshot-complete marker
//      LAST (StateMirror::attachSession).
//
// ── UP TO FOUR DEVICES, A MIRROR VIEW EACH (topology) ───────────────────
//
// Task 18 chose one shared StateMirror because the remote design's section
// 7.1 then allowed one session at a time, with a newcomer preempting it.
// iPhone app Task 71 replaces that: up to four devices (kMaxDeviceSessions)
// hold sessions at once, each a device (a paired device, a window signed in
// with the older token, or a hosting desktop's own window), and the
// operator's control is the transmit holder, not the one session (the
// several-devices design, docs/architecture/2026-09-24-several-devices-on-
// one-core-design.md, sections 2 and 4). No sign-in ever ends another
// device's session. DeviceSessionRegistry decides who is let in after every
// accepted sign-in: a device that already holds a place (live, or away in
// its 180 s) replaces its own older connection with sameDevice; with a
// place free the device is admitted; a full Core refuses, retryable.
//
// iPhone app Task 72 (the several-devices design, rulings 5.6 to 5.8): the
// StateMirror keeps its one set of watches, and each admitted session gets
// a MirrorView (Peer::view) with its own outbound coalescer, its own attach
// burst and its own sink, sendToPeer(), which fits every message to what
// that session negotiated (today's filter by minor and capabilities, and
// from Task 73 ownership: a view receives its own `slice:` objects and,
// with sessionHolderVersion 1, a `marker:` for every other slice; an older
// view its own slices and no marker). A newcomer's attach therefore never touches what
// another session has pending. Echo is per writer: a property write is
// applied as its session's write, and what it changes, on the written
// object or as a side effect on another (a shared receiver's blanker), is
// withheld from that session's view only and reaches every other. Routing:
// command.result, property.result and settings.reject go to the session
// that asked (confirm.request and notice, when they exist, to the device
// they are for); delta, object.create, object.destroy and settings.value go
// to every view (settings.value keeps its writer's origin); a write's
// readback of its side effects on the written object goes to the writer
// alone. The dispatcher's owner is station:<sessionId>, so a device's
// DSP-asset jobs end with its own session and nobody else's.
// Media and telemetry (iPhone app Task 76, the several-devices design,
// rulings 9.1 to 9.4): every admitted session has its own media epoch,
// given when it is let in, and is told media is on; its media control
// reaches the media controller for that epoch alone (DaemonMediaHub makes
// one per session), and telemetry goes to every session that negotiated
// it. The Core's one display budget (set by configuration or the load
// governor) is split among the sessions by DisplayBudgetSplit, and each
// session's capabilities carry its own share, generation and reason. The
// PureSignal display goes to the session that subscribed to it, and is
// charged to that session alone. Transmit joins here once Task 34 lands:
// TransmitHolder's holder feeds the split (see recomputeDisplayBudgetShares).
//
// A connection that has NOT yet authenticated does not touch the mirror at
// all -- it holds nothing but its own handshake state -- so a peer
// mid-handshake cannot disturb a live session. That is what keeps a failed
// or hostile connection attempt from being a denial of service against the
// operator's own sessions. Two bounds keep the mid-handshake population
// from becoming its own problem: kMaxConcurrentPeers caps how many sockets
// can exist at once, and kDefaultAuthDeadlineMs drops any that has not
// finished connecting in time (a peer that opens a socket and answers pings
// but never authenticates would otherwise live forever).
//
// ── THREADING ────────────────────────────────────────────────────────────
//
// This object, its QWebSocketServer, every transport it accepts, the
// StateMirror, the ObjectRegistry, the SessionCommandDispatcher, the
// SettingsProxyServer, the daemon's AppSettings and the RadioModel with
// every SliceModel under it ALL live on ONE thread: RadioModel's.
//
// That is not a convenience. StateMirror.h's attachSession() precondition
// spells out what breaks otherwise, and it breaks SILENTLY: every
// connection StateMirror::watch() makes is Qt::AutoConnection, which
// resolves to a direct call only while sender and receiver share a thread.
// Give the session its own thread and construct the mirror there, and
// onWatchedPropertyChanged() starts running AFTER applyInbound() has
// returned and cleared its m_applying guard, so every echo of a remote
// peer's own write leaks straight back to it -- with no test failing,
// because every test in this suite constructs on one thread.
// SessionCommandDispatcher.h states the same requirement for dispatch(),
// and SettingsProxyServer.h states it for AppSettings, which has no
// internal locking at all.
//
// So: the session read loop runs on the RadioModel thread. There is no I/O
// thread. Qt's WebSocket stack is event-loop driven, so this costs nothing
// a headless daemon notices; what it buys is that three separate
// documented invariants stay true by construction rather than by review.
//
// ── THE HEARTBEAT (task 18 step 2a) ──────────────────────────────────────
//
// **A TCP connection that dies silently never produces a close.** A laptop
// lid, a cell handoff, a NAT timeout: in all three the peer simply stops
// existing as far as the wire is concerned, and nothing about the socket
// says so. Without a heartbeat the daemon sits believing a dead client is
// alive, which is the state parent section 12.1's TX watchdog exists to
// make impossible.
//
// TciServer.cpp's 20 s QTimer + QWebSocket::ping is the in-tree precedent
// and this class copies its SHAPE. It deliberately does NOT copy its
// DETECTION MODEL. That precedent's own comment says it plainly:
//
//     "we don't expect a Pong back within any timeout -- we use the ping
//     itself to surface a dead socket via Qt's automatic write-error path"
//
// A write error is not a timely signal. A silently dead TCP connection can
// take minutes of retransmit backoff to produce one, and on a path where
// the far side vanished mid-NAT-mapping it may never produce one at all.
// So this class TRACKS PONGS: SessionTransport::pongReceived() resets a
// per-peer miss counter, and a peer that lets kDefaultMaxMissedPongs
// consecutive pings go unanswered is declared dead and closed.
//
// **Only a pong counts, deliberately.** An arbitrary inbound frame proves
// the peer's send path works; a pong proves the ROUND TRIP works, because
// the peer's WebSocket layer only emits one in response to a ping it
// actually received. The asymmetric case -- a client happily sending
// commands whose replies never reach it -- is exactly the case where the
// operator most needs the session torn down, and counting any inbound
// traffic as liveness would keep it alive indefinitely.
//
// **The numbers.** 20 s interval, 2 missed pongs, so a peer is declared
// dead between 40 s and 60 s after it actually went silent. The interval
// is TciServer's, which is this tree's own precedent and is itself ported
// from Thetis. The miss count comes from the only shipping configuration
// on real internet links anyone has measured for us: piHPSDR runs a 15 s
// heartbeat against a 30 s receive timeout (server_thread.c:925-934
// [@4aa95c5], verified 2026-08-08 by the maintainer), a ratio of two.
// One miss (20 s) would kill a session on a single dropped ping over a
// lossy mobile link, which is a false positive on precisely the links this
// feature exists for; three (60-80 s) is longer than a session-liveness
// signal needs to be.
//
// **What this is NOT: section 12.1's keyed-state deadline.** That section
// requires "a deadline in the low hundreds of milliseconds while MOX is
// asserted", which is two orders of magnitude tighter than the idle
// deadline above. R2 has no remote TX at all (design addendum section 2:
// "no MOX; TX is R4 in its entirety"), so there is no keyed state here to
// hang that deadline on and building one would be untestable speculation.
// The mechanism is the reusable part: setHeartbeatIntervalMs() and
// setMaxMissedPongs() are live-settable, so whichever task brings remote
// TX tightens an existing, soaked mechanism rather than introducing a
// second one at the moment it first becomes safety-critical. Pulling the
// heartbeat forward into R2 was a maintainer directive with exactly that
// soak time as its stated purpose.
//
// ── SCOPE ────────────────────────────────────────────────────────────────
//
// No ICE, no codecs, no spectrum, no audio. R2's demo is a blank
// panadapter, a blank waterfall and silent speakers, on purpose.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29: the direct media ladder: the Core's STUN server for media
//               (setMediaStun, mediaStunUrls) and the direct-only replace
//               (mediaDirectVersion). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-29: The Core's TCI server settings (JJ's ruling of 2026-09-28,
//               stationTciSettingsVersion 1). J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-29: iPhone app plan Task 23 (R-IOS-09, audioQualityVersion 1):
//               a device's own Opus bitrate. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-08-08  J.J. Boyd / KG4VCF  Remote daemon R2 Task 18: the daemon
//                                    half of the wss session. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-08/37/40: displayBudgetLimits()
//                                    is the budget in force for the
//                                    session; a computed one reaches only
//                                    minor-11 peers. AI-assisted
//                                    implementation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 4 (R-IOS-01): link
//                                    majors both ways (the hello's
//                                    `majors`, the plain-words refusal),
//                                    the peer's declared features and an
//                                    explicit TLS 1.2 minimum. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22: pgxlControlVersion
//                                    3 and tgxlControlVersion 1, the amp's
//                                    and tuner's own settings. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 / R-R3-47: remoteTgxlControlVersion 2.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 12 (R-IOS-08,
//                                    R-IOS-02): the Core's identity key,
//                                    paired devices and device sign-in;
//                                    token enrolment; the first-run banner
//                                    names the identity key. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 13 (R-IOS-08): the
//                                    `devices` object for a device that
//                                    declares deviceAuth, its verbs, and a
//                                    connection ended when its device is
//                                    removed or the token it signed in
//                                    with is retired. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 14 (R-IOS-08, D37):
//                                    the pairing window, pairing by one
//                                    tap and by code (SPAKE2+EE), the
//                                    hello's features.pairing,
//                                    pairingVersion 1, pairing.open and
//                                    pairing.close, and the code sent only
//                                    to a connection signed in with a
//                                    paired device's key. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24: Part C fix wave: the pairing code is never printed
//               to standard output (the journal on a packaged Core). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-24: Part C fix wave (security Minors R1-M1, M2, M4,
//               M5): the confirm-step recheck, the step 1 point check, the
//               per-address handshake cap and 0600 on load. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 19 (R-IOS-06): the
//                                    `catalog` object and
//                                    stationCatalogVersion 1. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 20 (R-IOS-27):
//                                    displayExtrasVersion 1. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 1):
//                                    transmitSettingsVersion and
//                                    isTransmitSettingKeyAcceptedOffAir.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 2):
//                                    transmitSettingsVersion 2.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 3):
//                                    transmitSettingsVersion 3.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  Core WebSocket opening (R-IOS-01,
//                                    R-R3-26): StationOpeningGate listens in
//                                    front of the QWebSocketServer, so every
//                                    Host form opens and a request the Core
//                                    cannot read gets 400.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app Task 71 (R-IOS-02): up to
//                                    four device sessions, admission and
//                                    the same-device rule in place of
//                                    preemption, the away state,
//                                    session.leave, sessionHolder 1 and
//                                    the `connectedDevices` object; 24
//                                    sockets. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app Task 72 (R-IOS-02): a
//                                    MirrorView per session, echo per
//                                    writer, routing per session and the
//                                    dispatcher's owner per session. AI-
//                                    assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app Task 73 (R-IOS-02): slice
//                                    ownership in every view (own slices,
//                                    markers for the others), refusals for
//                                    another device's slice, where a
//                                    device's slices come from at
//                                    admission and where they go when it
//                                    leaves, is away past its 180 s or is
//                                    revoked, and listeningOn. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 34 (R-IOS-02, R-IOS-03, R-IOS-13): the
//               station transmit gate (txPermitted per session, sent again when
//               it changes; remoteTxVersion 1 for a peer declaring remoteTx),
//               TransmitHolder and the keying gate on the model's
//               MoxController, a dropped holder, releases on leave, revoke and
//               the end of its 180 s, the on-air refusals, tx.setTxSlice, and
//               remote_transmit in place of the blanket receive-only policy.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 35 (R-IOS-13, R-IOS-02): keying from a
//               remote device (tx.key, tx.unkey, tx.tune, tx.twoTone through
//               RemoteKeying, for a peer at minor 11 declaring remoteTx); a
//               VOX key while a device holds transmit is that device's; a
//               write of transmit's mox or tune is refused "Use the transmit
//               button."; a session's keying commands are forgotten when it
//               ends. J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-25 - iPhone app plan, desktop remote transmit (R-IOS-13,
//                R-R3-42): txRefusalOf() and the refusal last sent to each
//                peer. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                Code.
//   2026-09-25: iPhone app plan Task 39 (D14, R-IOS-13, R-IOS-21): the
//               `txState` object (TransmitState, txStateVersion 1) to a peer
//               at minor 11 declaring remoteTx; a link lost and a device
//               removed while keyed record their stop reasons. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app Task 74 (R-IOS-02,
//                                    R-IOS-30): receivers several devices
//                                    share (the anchor, the pin, pan
//                                    moves, takes and Take it back), the
//                                    confirm step with its readback, and
//                                    notices (StationReceivers.cpp).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app Task 76 (R-IOS-31): media
//                                    and telemetry for every admitted
//                                    session, each with its own media
//                                    epoch; the display budget split among
//                                    them (DisplayBudgetSplit) with each
//                                    device's share and reason in its own
//                                    capabilities; media control from each
//                                    session for its own media; the
//                                    PureSignal display's subscriber.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-IOS-27, R-IOS-06: bandSelectVersion
//                                    1 and slice.selectBand.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-IOS-27, R-IOS-06:
//                                    displayExtrasVersion 2 (clarity-retune).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 7):
//                                    transmitSettingsVersion 7 and
//                                    pureSignalArmingOffered.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 8):
//                                    remoteTgxlControlVersion 4.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 9):
//                                    remotePgxlControlVersion 4.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 10):
//                                    remoteRfKitControlVersion 4 and
//                                    accessoryDataVersion 2.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 / R-R3-46 (parity Task 13):
//                                    transmitSettingsVersion 8 and
//                                    isTransmitSettingKeyTakenOnAir.
//   2026-09-26  J.J. Boyd / KG4VCF  Checkpoint carry: the uncapped message
//                                    size's memory figure follows
//                                    kMaxConcurrentPeers 24 (48 GiB).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  R-R3-13 / R-R3-49 (parity Task 15):
//                                    meterReadingsVersion.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave C2: voxArmedByChanged; VOX a
//               device armed goes off when that device's own line closes.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave: I4 txStateVersion 2. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: Transmit group fix wave I2: the transmit slice is frozen
//               while the radio's own PTT keys it (ruling 8.11). J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave 2: every holder on the air counts,
//               the station device's own keys included (onAirHolder),
//               exempt by change not by holder; ruling 8.11's freeze on
//               every path (XIT, pan moves, a stored change at proceed); a
//               hosting desktop's key named after it. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave 2, Important 2: a refused TUNE or
//               two-tone takes nothing (admitKey asks TX inhibit, the PA
//               trip, receive only and the interlock before the gate; a
//               take whose key never starts is released). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  R-R3-49 / R-R3-21 / R-R3-40 (parity
//                                    Task 16): dspInfoVersion.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 27 (R-IOS-08): acceptPairingMailbox(),
//               a pairing through the remote access service's mailbox (pair.*
//               only, no hellos, no address). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03, R-IOS-13):
//               taking transmit (StationTransmitTake.cpp), remoteTxVersion 2,
//               the radio's PTT take, TX marks, the transmit slice on a
//               change of holder. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  R-IOS-25 / R-R3-49 (parity Task 19):
//                                    recordStreamVersion, the record
//                                    streams (spots, spotConsole:<source>)
//                                    and the `spotSources` object.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26: D79 (R-IOS-11, R-R3-49): bandPlanRefusal() and
//               applyBandPlanSetting(). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 28 (R-IOS-16): certificatePemPath()
//               and privateKeyPemPath(), for the control connection through
//               the remote access service. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: Parity Task 32 (R-IOS-13, R-R3-49): txMonitorAudioVersion()
//               and txMonitorAudioAvailable(). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: iPhone plan Task 22 / parity Task 20 (R-IOS-26):
//               stationFreedvVersion(). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 (R-IOS-16): SwitchableTransport
//               under every session, session.pathTicket and path.join, the
//               Task 29 capabilities. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-27: Parity Task 33 (R-R3-49, R-R3-32): txReadingsVersion()
//               and the txCfcCompression stream's reader. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: R-IOS-13 / R-R3-49: txModMonitorVersion(), the AM Mod
//               Monitor's record streams (ModMonitorPublisher) and
//               txModMonitor.reset. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-28: R-IOS-13 / R-R3-49: peerGetsTxEqCurve() and
//               fitTxEqCurveToPeer(): transmit's txEqCurve and
//               txEqCurveVersion only to a peer that declared txEqCurve.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28: Phone wire batch: peerGetsFeatureProperties() and
//               fitPeerOnlyProperties(): a declared feature's properties
//               only to a peer that declared it. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-28: Phone wire batch: fitRecordBatchToPeer(): a declared
//               feature's record fields only to a peer that declared it.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28: Ruling 7.1a: SharedCategory, SharedTier and
//               sharedTierOf (one table for both tiers), connectedAffected,
//               applySharedNow and tellAppliedNow. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-28: addendum G-42: transmitGateSettingRefusal(), the Core's
//               Extended transmit setting taken only with transmit
//               permission, off the air, as True or False. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28: R-IOS-13 / R-R3-49: txEqCurveVersionFor() (2 to a peer
//               that declared txEqCurve 2) and handleTxEqCurveCommand():
//               txEq.setCurve and txEq.resetCurve applied as the peer's
//               txEqParaEqData write. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-29: takesTransmitSettingsOnAir() (transmitSettingsVersion 13).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: The phone's direct addresses: coreAddressWatcher() and
//               peerGetsCoreAddresses(): devices' coreAddresses and
//               coreAddressesVersion only to a device signed in with its
//               own key that declared coreAddresses. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-28: R-R3-46 / R-R3-11: peerGetsAdcAttenuators() and
//               fitAdcAttenuatorsToPeer(). J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-29: transmitSettingsVersion 15: handleCfcProfileCommand()
//               (cfc.setProfile). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-28: slice control and shared listening plan Task 2:
//               changeRefusal, mediaSessionControlsSlice / HearsSlice /
//               SeesSlice and listenerChangeReason (SliceAccessPolicy).
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-28: slice control and shared listening plan Task 4:
//               sliceAccessVersion(), peerHasSliceAccess(), the
//               SliceAccess objects, the slice and marker forms on a join
//               or a leave (onSliceAccessChanged) and the
//               SliceAccessController behind the listen, stop listening,
//               take control and release verbs. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 9: usableSlicesJson(),
//               questionPlanner(), closeForTake's listenedBy and
//               tellListeners(). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 10: invokeAsStationDevice() and
//               setStationNoticeHandler(): the hosting desktop's slice
//               requests run as the station device through the same
//               dispatcher, checks, confirm step and SliceAccessController
//               as a remote device's, answered through callbacks. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-29: slice control plan Task 11: transmitPreferenceFor() and
//               takenSliceKeyRefusal(), the keying refusal on a slice
//               taken from another device and not chosen. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-29: slice control plan Task 17: sliceHolderWords(), who holds
//               a slice as a refusal names it. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: slice control fix wave (whole-branch review, Critical 1):
//               closeUnclaimedOrDefer() and fireDeferredCloses(), a slice
//               nobody is on closes only once it is not transmitting.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-30: take-over parity: sliceAccessVersion 2 (Take it back on
//               controlTaken), peerTakesControlBack(), takeBackControl();
//               the hosting desktop's slices can pass to a remote device.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-30: take-over fix wave (I-2): othersSliceKeyRefusal(), a
//               key never lands on the slice a device lost; re-review
//               (N-1): keyerSharesSlices(), nor on another device's
//               slice. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: TX rulings (8.11 on a hosting desktop):
//               radioPttKeyRefusal(), the radio's own PTT keys the
//               desktop's active slice. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: core-slice take-over (JJ, 2026-09-30): sliceAccessVersion
//               3, peerTakesCoreSlice(); handOffRefusal() takes the
//               taker, and with nobody at the Core's desktop the Core's
//               own slice passes to a taker at 3. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: JJ's wider ruling: staysListeningAfterTake(). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-30: TX badge take fix round 1: setTokenSessionsMayTransmitForTest(),
//               so a window test reaches a real take of transmit. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-10-01: TX mic thread (JJ approved): txChannelMessage takes the
//               keepalive's wait since its receipt (heldUs). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: Control logging lane: each device's control writes,
//               commands and their answers, and the gaps between its
//               control messages, logged with a per-device rate limit.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: Control logging lane fix round: the log moves to
//               ControlLog (sanitized names, receipt times, token buckets,
//               a Core-wide cap, folded answers, the link, keepalive gaps
//               per channel and watchdog stops). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/IceConfiguration.h"

#include <QHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QPair>
#include <QObject>
#include <QPointer>
#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QSslConfiguration>
#include <QString>

#include <map>
#include <memory>
#include <functional>
#include <optional>
#include <utility>

#include "core/DeviceLayoutStore.h"
#include "core/SliceOwnership.h"
#include "core/session/ConfirmStep.h"
#include "core/session/ControlLog.h"
#include "core/session/LinkVersion.h"
#include "core/session/ReceiverPlanner.h"
#include "core/DisturbanceCheck.h"
#include "core/session/RecordStream.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/settings/SettingsBackupTransfer.h"
#include "core/safety/StarvationPolicy.h"
#include "core/safety/StationTxGate.h"
#include "core/safety/TransmitHolder.h"
#include "core/session/media/DisplayBudgetSplit.h"

QT_BEGIN_NAMESPACE
class QThread;
class QTimer;
class QWebSocketServer;
QT_END_NAMESPACE

namespace NereusSDR {

class AppSettings;
class CertificateStore;
class DeviceAuthenticator;
class DeviceStore;
class PairingWindow;
class SpakeExchange;
class StationIdentity;
class ObjectRegistry;
class MirrorView;
class SliceMarkerSet;
class SliceAccessSet;
class SliceAccessController;
class ConnectedDevicesFacade;
class DeviceSessionRegistry;
class ModMonitorPublisher;
class RadioModel;
class SessionCommandDispatcher;
class SessionTransport;
class MediaTunnel;
class SettingsProxyServer;
class StateMirror;
class StationOpeningGate;
class StationCatalog;
class PaProfilesFacade;
class SetupDescription;
using SetupDescriptionService = SetupDescription;
class StationRadios;
class StationDevicesFacade;
class CoreAddressWatcher;
class TokenStore;
class TransmitHolder;
class TransmitState;
class StationVax;
class RemoteKeying;
class RemoteTxWatchdog;
class TxWatchServer;

/// Slice control plan Task 10: how the hosting desktop hears what a
/// request as the station device answers, asks or tells.
using StationAnswer = std::function<void(const SessionMessage&)>;

class StationServer : public QObject {
    Q_OBJECT

public:
    /// See the class comment's heartbeat section for both numbers and the
    /// reasoning behind each.
    static constexpr int kDefaultHeartbeatIntervalMs = 20000;
    static constexpr int kDefaultMaxMissedPongs = 2;

    /// How often the outbound delta coalescer is drained. StateMirror's
    /// coalescer is latest-wins and re-resolves against the live model at
    /// flush time, so this is a rate limit rather than a delay budget: one
    /// band-button press runs 75 setters on each of up to five slices
    /// (design addendum section 7), and without a flush cadence that is
    /// roughly 250 messages for one keypress.
    static constexpr int kDefaultDeltaFlushMs = 50;
    static constexpr int kTakeoverAnswerMs = 60000;

    /// How long a connection has to complete the section 7.0 handshake
    /// before it is dropped. Without it, a peer that opens a socket and
    /// answers pings but never authenticates lives forever, holding a slot
    /// and a file descriptor, which is a cheap way to sit on a station.
    /// Generous on purpose: the only work between accept and authenticate
    /// is two small messages, so 30 s is far more than a real client needs
    /// even on a bad link, and short enough that a stuck peer clears
    /// without operator action.
    ///
    /// R-R3-16/17: the value is kStationHandshakeDeadlineMs, shared with
    /// StationClient's own deadline, and it now runs until the peer's
    /// snapshot has been sent rather than until it authenticates.
    static constexpr int kDefaultAuthDeadlineMs = kStationHandshakeDeadlineMs;

    /// iPhone app Task 71 (ruling 4.5): a cap on sockets of every kind,
    /// signed in or not, pairing or connecting. Devices hold at most
    /// kMaxDeviceSessions places; while a device reconnects it can hold its
    /// old socket (not yet noticed dead) and four racing attempts (direct
    /// over IPv6 and IPv4, the rendezvous path and the relay), so five
    /// sockets for each of four devices is 20, and a fifth device's four
    /// attempts make 24. At kMaxIncomingMessageBytes that is 24 MiB of
    /// exposure before sign-in, which a Pi 4 does not notice. The next
    /// socket is refused before any hello, retryable.
    static constexpr int kMaxConcurrentPeers = 24;
    /// iPhone app Task 71 (D44, ruling 4.4): devices that hold a place at
    /// once (DeviceSessionRegistry::kMaxDeviceSessions): admitted sessions,
    /// devices away in their grace period and a hosting desktop's own
    /// window. Not connections still connecting, pairing connections, or
    /// the station device of a Core with no desktop.
    static constexpr int kMaxDeviceSessions = 4;
    /// Part C fix wave (R1-M4): connections from one address that are
    /// still connecting (their snapshot not yet sent), so one host cannot
    /// hold every kMaxConcurrentPeers slot by redialling within the
    /// handshake deadline. IPv6 addresses are counted per /64 (see
    /// addressKey()). A connection with no address of its own (the
    /// relay, later) is not counted here.
    static constexpr int kMaxHandshakesPerAddress = 2;
    /// Core WebSocket opening (R-IOS-01, R-R3-26): connections whose TLS
    /// or WebSocket upgrade has not finished, counted by
    /// StationOpeningGate apart from the peers above, each until it opens,
    /// is refused or reaches kDefaultOpeningDeadlineMs. At this total the
    /// oldest unfinished opening is closed to make room for the new one;
    /// kMaxHandshakesPerAddress of them per address (IPv6 by /64) still
    /// applies first, closing that address's own oldest.
    ///
    /// Why 64, and not kMaxConcurrentPeers: the pool is what a flood has
    /// to fill to push a real device's opening out. Each address holds
    /// only 2, so pushing a device out takes 64 connects from at least 32
    /// addresses or /64s inside that device's own opening time (TCP
    /// connect to the end of its request, well under a second): at 8 it
    /// took 8 plain TCP connects from 5 addresses. What 64 costs is file
    /// descriptors, one per opening. packaging/nereusd.service.in sets no
    /// LimitNOFILE, so nereusd runs under the default soft limit of 1024;
    /// 64 openings, kMaxConcurrentPeers peers, the status page and the
    /// radio's sockets stay far below it, and the pool keeps a flood from
    /// ever reaching it (before the gate a flood of unfinished openings
    /// could climb toward 1024 at about 100 connects a second).
    static constexpr int kMaxUnfinishedOpenings = 64;

    /// Largest inbound WebSocket message, and frame, on an ACCEPTED
    /// socket. Applied by WebSocketTransport's constructor.
    ///
    /// This bound is PRE-AUTHENTICATION and that is the whole reason it
    /// exists. Qt's default, measured on Qt 6.11 by reading it back off an
    /// accepted socket, is 2147483646 bytes: just under 2 GiB, per message,
    /// per socket. Qt buffers a complete message before it emits
    /// textMessageReceived, so kDefaultAuthDeadlineMs bounds how LONG an
    /// unauthenticated peer may sit here but bounds no BYTES at all.
    /// Uncapped, kMaxConcurrentPeers (24) of them come to roughly 48 GiB on
    /// a daemon whose stated hardware floor is a Pi 4.
    ///
    /// Sized against the largest message a legitimate client can send,
    /// which is not a guess:
    ///
    ///   - Hello, AuthRequest, SettingsRemove: hundreds of bytes. The
    ///     token is 43 base64url characters (TokenStore.h).
    ///   - CommandInvoke: at most two named scalar arguments across the
    ///     four known verbs (SessionMessage::commandVerb).
    ///   - PropertyWrite: one object's coalesced dirty set, bounded by
    ///     that class's whole property table. SliceModel is the largest
    ///     mirrored class at over a hundred Q_PROPERTY declarations, and
    ///     its only QString-valued mirrored properties are antenna names,
    ///     panKey and lastRadeRxCallsign. At a generous 128 bytes per JSON
    ///     entry that is under 16 KiB.
    ///   - SettingsWrite: one key plus one value. The longest values this
    ///     tree stores under a Station-classified key are persisted JSON
    ///     blobs, and the largest of those is bounded by construction:
    ///     FaultLog is a 10-entry ring of six short fields
    ///     (FaultLog.cpp:19, kMaxEvents = 10).
    ///
    /// So roughly 16 KiB is the real ceiling, and 1 MiB is about 64 times
    /// that. It leaves 8 MiB of total pre-auth exposure across every peer
    /// slot, which is a number a Pi 4 does not notice. TciServer.cpp:1470
    /// makes the same call at the same magnitude for a socket that is
    /// loopback-only.
    static constexpr quint64 kMaxIncomingMessageBytes = 1024ULL * 1024ULL;

    /// `radioModel` and `settings` are NOT owned and must outlive this
    /// object; both must live on this object's thread (see the class
    /// comment's threading section, which is where the consequences of
    /// getting that wrong are spelled out).
    ///
    /// `securityDirectory` is where the TLS certificate and the auth token
    /// live. Empty means the daemon profile's own config directory, which
    /// is the production answer; tests pass a scratch directory so a run
    /// never reads back or overwrites a real station's identity.
    ///
    /// `supportedMajors` (iPhone app Task 4) is the link majors this
    /// station advertises and accepts, oldest first. The default is the
    /// build's own (kSupportedSessionMajors); tests inject theirs, and a
    /// debug build of nereusd takes --test-link-majors.
    explicit StationServer(RadioModel* radioModel, AppSettings& settings,
                           const QString& securityDirectory = QString(),
                           QObject* parent = nullptr,
                           const QList<quint16>& supportedMajors =
                               LinkVersion::supportedMajors());
    ~StationServer() override;

    StationServer(const StationServer&) = delete;
    StationServer& operator=(const StationServer&) = delete;

    /// Binds a wss listener. False (with lastError() set) when TLS is
    /// unavailable, the certificate could not be provisioned, or the bind
    /// failed. Port 0 asks the OS for a free one; read it back with
    /// serverPort().
    bool listen(const QHostAddress& address, quint16 port);

    void close();
    /// Fix wave after parity Tasks 19 and 21: ends every session for a
    /// radio change (the Core restarts its run) with `reason`, retryable,
    /// code radioChanging. Each connection outlives this server until its
    /// close is written (or kRadioChangeLingerMs), so what was sent before
    /// it (a command's answer, the confirm step's notices) and the end
    /// itself reach the wire.
    void endSessionsForRadioChange(const QString& reason);
    /// Follow-up N3 (the coordinator's ruling (a)): the Core's DaemonApp
    /// accepted a radio change (StationRadios::onSelect). Until
    /// finishRadioChange, the chooser's accepted station.selectRadio answer
    /// (or, after the confirm step, its confirm.proceed answer) and the
    /// other devices' settingChanged notices are held, as a rate change's
    /// proceed is (DeferredProceed).
    void holdRadioChangeAnswers();
    /// The restart turn: sends what was held. `proceeded`: the answer as
    /// accepted, then the notices (before endSessionsForRadioChange).
    /// Otherwise the answer is refused with `refusal` and nobody is told.
    void finishRadioChange(bool proceeded, const QString& refusal);
    /// NereusSDR-original bound: how long an ended connection waits for its
    /// close to be written before it is deleted.
    static constexpr int kRadioChangeLingerMs = 5000;
    bool isListening() const;
    quint16 serverPort() const;
    QHostAddress serverAddress() const;
    QString lastError() const { return m_lastError; }

    /// The listener's TLS configuration as it is in force, read back from
    /// the listener (default-constructed before the first listen()). Its
    /// protocol() is QSsl::TlsV1_2OrLater: the minimum is set explicitly
    /// rather than left to Qt's default.
    QSslConfiguration tlsConfiguration() const;

    /// iPhone app Task 4 (R-IOS-01): the link majors this station accepts,
    /// oldest first.
    QList<quint16> supportedMajors() const { return m_supportedMajors; }

    /// The major `peer`'s session runs at: the one its hello chose from
    /// this station's list. 0 before that hello was accepted, or for a
    /// transport that is not a peer.
    quint16 peerAgreedMajor(SessionTransport* peer) const;

    /// True when `peer`'s hello declared `feature` at `minVersion` or
    /// later. What the station must know before capabilities are sent
    /// (device authentication, pairing, the takeover question, Setup
    /// descriptions) is asked here. An older app declares nothing.
    bool peerDeclares(SessionTransport* peer, const QByteArray& feature, int minVersion) const;
    /// R-IOS-26 / R-R3-49: the peer knows 2 m as its own band (band2m 1 in
    /// its hello, at minor 11), so it is sent Band 27 and the per-band
    /// lists with 2 m. Any other peer sees 2 m as GEN (BandLinkFit.h).
    bool peerKnows2m(SessionTransport* peer) const;
    /// The encoded `message` for `transport`: fitted by BandLinkFit for a
    /// peer that does not know 2 m.
    QByteArray encodeFor(SessionTransport* transport, const SessionMessage& message) const;

    /// The pairing token of a Core upgraded from before paired devices,
    /// until it is retired; empty on a new Core (iPhone app Task 12: none
    /// is generated any more). And the TLS fingerprint a client pins.
    QString token() const;
    QString certificateFingerprint() const;
    /// iPhone app plan Task 28 (R-IOS-16): the PEM files of the Core's own
    /// TLS certificate and its key, what a control connection through the
    /// remote access service presents in DTLS (DataChannelTransport), so a
    /// device sees there the certificate the hello binds. Empty when the
    /// certificate is not usable.
    QString certificatePemPath() const;
    QString privateKeyPemPath() const;

    /// iPhone app Task 12 (R-IOS-08): the Core's paired devices and its
    /// identity key. Never null / always present; the identity may be
    /// invalid (StationIdentity::isValid()) when its file is damaged, and
    /// then listen() refuses.
    DeviceStore* deviceStore() const;
    const StationIdentity& stationIdentity() const;
    /// iPhone app Task 13 (R-IOS-08): the mirrored `devices` object and the
    /// device administration verbs behind it.
    StationDevicesFacade* devicesFacade() const;
    /// The phone's direct addresses: what reads the addresses a device can
    /// dial this Core at into devices' coreAddresses. It follows the
    /// listener (listeningChanged); a test starts it on a listener and
    /// interfaces of its own. Never null.
    CoreAddressWatcher* coreAddressWatcher() const;
    /// 1 when the Core sends `devices` and takes its verbs (its identity
    /// key is usable), else 0.
    int deviceAdminVersion() const;
    /// iPhone app Task 19 (R-IOS-06): the mirrored `catalog` object, the
    /// values the Core owns and an app draws its controls from. Never null.
    StationCatalog* catalog() const;
    SetupDescriptionService* setupDescription() const { return m_setupDescription.get(); }
    /// 1: the Core sends `catalog` to a peer at minor 11.
    int stationCatalogVersion() const;
    /// iPhone app Task 20 (R-IOS-27): 2 while media is enabled (0 without);
    /// a peer at minor 11 may then ask a spectrum subscription for display
    /// extras (1) and send clarity-retune (2, R-IOS-27, R-IOS-06).
    int displayExtrasVersion() const;

    /// The first-run block, exactly as the operator is shown it: the TLS
    /// pin and the identity key's path with the prompt to back it up.
    ///
    /// Pure and public for two reasons. It keeps the one place the block
    /// is FORMATTED separate from the one place it is WRITTEN, so the
    /// write side can be a single stdout call with no formatting logic in
    /// it; and it lets a test assert on the exact text without capturing a
    /// stream. See writePairingBanner() in the .cpp for why the banner
    /// does not go through qCInfo() like every other line in this class.
    static QString formatFirstRunBanner(const QString& fingerprint,
                                        const QString& identityKeyPath);

    /// Adopt an already-connected transport as a new peer. This is what
    /// the QWebSocketServer's newConnection handler calls, and it is also
    /// how a test drives the real handshake over an in-process pipe: ONE
    /// code path, no test-only branch (SessionTransport.h explains why the
    /// seam exists at all). Takes ownership by reparenting.
    void acceptTransport(SessionTransport* transport);
    /// iPhone app plan Task 27 (R-IOS-08): adopts a pairing that arrives
    /// through the remote access service's mailbox
    /// (RendezvousMailboxTransport). A mailbox carries the link's `pair.*`
    /// messages and nothing else (the rendezvous document, section 6.5), so
    /// no hello goes either way: the connection starts where pair.start is
    /// expected, and any other kind ends it as a protocol error. It has no
    /// address, so one tap is refused and only the code pairs.
    void acceptPairingMailbox(SessionTransport* transport);
    /// iPhone app plan Task 28 fix wave (R-IOS-16; review Important 1 and
    /// 2): adopts a control connection the remote access service
    /// introduced (StationRendezvous, a DataChannelTransport once open).
    /// The session is the same as a direct one's, with three differences
    /// the link document's section 20 states: pair.* is refused (pairing
    /// through the service stays the mailbox's, with the code), a token
    /// auth.request is refused (the device's key only), and every such
    /// connection still connecting counts as one source against
    /// kMaxHandshakesPerAddress, whatever address it reports, because the
    /// service can replay an introduction (the rendezvous document's
    /// "not device authentication"). `introductionId` is the service's id
    /// for it, handed to the sign-in limits.
    /// iPhone app plan Task 29: `deviceId`, when given, is the paired
    /// device the service introduced (RendezvousIntroduction::deviceId),
    /// the only device whose session such a connection may join (link
    /// section 21.2).
    void acceptIntroducedTransport(SessionTransport* transport, const QString& introductionId,
                                   const QByteArray& deviceId = QByteArray());

    /// Parent design section 4.5's EFFECTIVE slice limit: what this daemon
    /// can sustain, which on the Pi 4 floor may be fewer than the radio
    /// supports. Defaults to the board's own maxSlices, i.e. no narrowing,
    /// because R2 builds no PerfMonitor to compute anything better. Values
    /// below 1 are ignored.
    void setSustainableSliceLimit(int slices);
    int sustainableSliceLimit() const { return m_sustainableSliceLimit; }

    /// See the class comment's heartbeat section. Applied to the running
    /// timer immediately. An interval of 0 or less STOPS the heartbeat
    /// entirely, which exists for a bench session an operator is
    /// deliberately holding open through a laptop suspend; it is not a
    /// supported production configuration and is logged as a warning.
    void setHeartbeatIntervalMs(int ms);
    int heartbeatIntervalMs() const { return m_heartbeatIntervalMs; }

    void setMaxMissedPongs(int misses);
    int maxMissedPongs() const { return m_maxMissedPongs; }

    /// Consecutive failed authentications tolerated before the station
    /// stops answering, and for how long. Forwards to TokenStore; see its
    /// header for the semantics and for why RateLimited is a distinct
    /// outcome from Rejected. Defaults are TokenStore's own.
    void setAuthRateLimit(int maxFailures, int lockoutMs);

    /// iPhone app Task 12: nereusd.conf's `pairing_lan_click = allow|deny`
    /// (default allow). Whether a device on this Core's own network may
    /// pair with one tap while the Core is unclaimed; the pairing window
    /// (Task 14) reads it. Deny forces the code for every pairing.
    void setPairingLanClickAllowed(bool allowed) { m_pairingLanClickAllowed = allowed; }
    bool pairingLanClickAllowed() const { return m_pairingLanClickAllowed; }

    /// iPhone app Task 14 (R-IOS-08): the Core's pairing window. Never null.
    /// Its state is the Core's, not any connection's: open with no timer
    /// while the Core is unclaimed, reopened from the console (reopen())
    /// or a paired device (`pairing.open`).
    PairingWindow* pairingWindow() const;
    /// 1 when the Core pairs devices (its identity key is usable): the
    /// hello declares features.pairing 1 and capabilities carry
    /// pairingVersion 1. 0 otherwise.
    int pairingVersion() const;

    /// The pairing code is never printed or logged (Part C fix wave: on a
    /// packaged Core standard output lands in the journal). The console's
    /// `nereusd pairing show` gives it on request, over the owner-only
    /// control socket, and so does the status page while unclaimed.
    /// Writes `text` to standard output and flushes it: the Core's console,
    /// as the first-run banner is written.
    static void printToConsole(const QString& text);
    /// Part C fix wave (R1-M4): `address` as the per-address handshake
    /// count keys it: IPv4, and an IPv4-mapped IPv6 address, as the full
    /// IPv4 address; any other IPv6 address as its /64 prefix
    /// ("2001:db8:1:2::/64"), no scope id. "" for "".
    static QString addressKey(const QString& address);
    /// Whether `address` (a connection's peer address) is on one of this
    /// machine's directly connected networks: a loopback address, or one
    /// inside the subnet of an address of a running interface. Empty (a
    /// relayed connection) is not. One tap pairs only from such an address.
    static bool isOnDirectNetwork(const QString& address);

#ifdef NEREUS_BUILD_TESTS
    /// Replaces the code's hash (SpakeExchange::storedData) so a test can
    /// hold it on the worker and watch the event loop keep serving. Null
    /// restores the real one.
    void setPairingHasherForTest(std::function<QByteArray(const QString&)> hasher);
    bool isHashingPairingCodeForTest() const;
    /// The opening pool's total and per-address limits in place of
    /// kMaxUnfinishedOpenings and kMaxHandshakesPerAddress, from the next
    /// listen() on, so a test on one loopback address can reach the total.
    void setOpeningLimitsForTest(int total, int perAddress);
    /// A window over a bench link (no TLS pin) cannot enrol its key, so it
    /// always signs in with the token. This lets such a window's session
    /// change the Core's radio, so a window test can reach the Core's
    /// answers (fix wave, I5).
    void setTokenSessionsMayChangeRadioForTest(bool may) { m_tokenSessionsMayChangeRadioForTest = may; }
    /// The same bench window cannot pair, so the transmit gate refuses it
    /// as not paired. This counts such a session as paired for that gate,
    /// so a window test can take transmit. Set before the window connects.
    void setTokenSessionsMayTransmitForTest(bool may) { m_tokenSessionsMayTransmitForTest = may; }
    /// Deterministic export expiry; caller drives cleanup after advancing time.
    void setSettingsExportClockForTest(std::function<qint64()> clock)
    { m_settingsExportNowForTest = std::move(clock); }
    void expireSettingsExportsForTest() { expireSettingsExports(); }
    /// Control logging lane: the clock the control log reads (ms).
    void setControlLogClockForTest(std::function<qint64()> clock)
    { m_controlLog.setClock(std::move(clock)); }
#endif

    /// See kDefaultAuthDeadlineMs. Values below 1 disable the deadline,
    /// which is logged as a warning rather than silently accepted.
    void setAuthDeadlineMs(int ms);
    int authDeadlineMs() const { return m_authDeadlineMs; }

    /// Core WebSocket opening (R-IOS-01, R-R3-26): TCP accept to the 101,
    /// TLS included, is bounded by this; StationOpeningGate.h explains the
    /// value (Qt's own handshake timeout default) and the .cpp asserts the
    /// two agree. Values below 1 are ignored. Takes effect at the next
    /// listen(); tests shorten it.
    static constexpr int kDefaultOpeningDeadlineMs = 10000;
    void setOpeningDeadlineMs(int ms);
    int openingDeadlineMs() const { return m_openingDeadlineMs; }

    /// Connections accepted whose opening (TLS and the WebSocket upgrade)
    /// has not finished. They are not peers yet and are not in peerCount().
    int openingCount() const;

    /// Every peer currently attached, authenticated or not.
    int peerCount() const { return static_cast<int>(m_peers.size()); }

    /// At least one device's session is admitted and live (up to
    /// kMaxDeviceSessions may be; see the topology note above).
    bool hasAuthenticatedSession() const;
    /// Live admitted sessions, 0 to kMaxDeviceSessions.
    int authenticatedSessionCount() const;

    /// Configure before accepting sessions. Old peers remain control-only.
    void setMediaEnabled(bool enabled);
    /// Each takes a media session's epoch (iPhone app Task 76: every
    /// admitted session has its own). The forms without one answer for
    /// the primary media session: the earliest admitted of those live.
    bool mediaAvailable() const;
    bool mediaAvailable(quint64 epoch) const;
    /// iPhone app plan Task 28 (R-IOS-16): the ICE settings of the session
    /// `epoch`'s media when the session came through the remote access
    /// service (a DataChannelTransport): the control connection's STUN
    /// server, and its relay only when the control connection's path is
    /// relayed (DataChannelTransport::mediaIceConfiguration(), the safety
    /// review's Important 4). None for a WebSocket session.
    std::optional<IceConfiguration> sessionIceConfiguration(quint64 epoch) const;
    bool remoteWidebandAvailable() const;
    bool remoteWidebandAvailable(quint64 epoch) const;
    /// The session agreed minor 8 or later: audio contexts carry the encoder
    /// profile or the off reason. Minor-7 peers keep the eight-key context.
    bool remoteAudioStatusAvailable() const;
    bool remoteAudioStatusAvailable(quint64 epoch) const;
    /// The session agreed minor 9 or later: spectrum contexts report the
    /// grant Core made. Minor-8 peers keep the 19-key (20 with wideband) context.
    bool spectrumGrantAvailable() const;
    bool spectrumGrantAvailable(quint64 epoch) const;
    /// The session agreed minor 11 and the Core advertised
    /// displayExtrasVersion 1: a subscription may carry the display extras
    /// fields (iPhone app Task 20, display extras v1).
    bool displayExtrasAvailable() const;
    bool displayExtrasAvailable(quint64 epoch) const;
    /// iPhone app plan Task 23 (R-IOS-09): the media session `epoch`'s
    /// device was told audioQualityVersion 1, so its audio control may
    /// carry `opusBitrate`.
    bool audioQualityAvailable(quint64 epoch) const;
    /// JJ's ruling of 2026-09-28 (stationTciSettingsVersion 1): `transport`
    /// declared stationTciSettings 1 at minor 11 on a Core at
    /// stationTciVersion 2, so it gets the `stationTci` object's other
    /// eleven settings and may send setStationTciSettings.
    bool peerGetsStationTciSettings(SessionTransport* transport) const;
    /// ...and every other peer gets today's `stationTci`: those settings
    /// taken out of its schema, snapshot and deltas (a delta of only them is
    /// not sent: false).
    bool fitStationTciSettingsToPeer(SessionTransport* transport,
                                     SessionMessage& message) const;
    /// iPhone app Task 76: the epochs of the media sessions live now, in
    /// admission order; the device a media session is for. Slice control
    /// plan Task 2 (SliceAccessPolicy): whether that device controls a
    /// slice (its owner mix, raw I/Q, headphones), may hear it (receiver
    /// streams) and may see it (display subscriptions). Until Task 3 lets
    /// devices listen, all three read the same: the device's own slices
    /// (ruling 9.1).
    QList<quint64> mediaSessionEpochs() const;
    QByteArray mediaSessionDevice(quint64 epoch) const;
    bool mediaSessionControlsSlice(quint64 epoch, int sliceId) const;
    bool mediaSessionHearsSlice(quint64 epoch, int sliceId) const;
    bool mediaSessionSeesSlice(quint64 epoch, int sliceId) const;
    /// The media session the PureSignal display goes to (the one whose
    /// ps3.subscribeDisplay was last accepted), or 0 for none known.
    quint64 ps3DisplaySubscriberEpoch() const { return m_ps3SubscriberEpoch; }
    /// iPhone app plan Task 36 (R-IOS-13): the media session `epoch` was
    /// told remoteTxVersion (it agreed minor 11 and its hello declared
    /// remoteTx 1), so its media start may carry remoteTxVersion and get the
    /// microphone line.
    bool remoteTxAvailableForMedia(quint64 epoch) const;
    /// Task 36: whether the media session `epoch` may transmit now (its
    /// txPermitted).
    bool mediaSessionTxPermitted(quint64 epoch) const;
    /// Installs newer limits (a later generation) and why they are below the
    /// Core's ceiling (R-R3-08, R-R3-37). A new reason needs a new
    /// generation; the same limits with the same reason are accepted as-is.
    bool setDisplayBudgetLimits(const DisplayBudgetLimits& limits,
                                DisplayBudgetReason reason = DisplayBudgetReason::None);
    /// The budget in force for a media session: its share of the limits
    /// last set (iPhone app Task 76, DisplayBudgetSplit), except that with
    /// setDisplayBudgetForReasonPeersOnly(true) a peer below
    /// kDisplayBudgetReasonSessionProtocolMinor (or no peer) has none and
    /// keeps legacy mode. Without an epoch: the primary media session's,
    /// or with none, what a first session would be given.
    std::optional<DisplayBudgetLimits> displayBudgetLimits() const;
    std::optional<DisplayBudgetLimits> displayBudgetLimits(quint64 epoch) const;
    /// The share a media session would have with the PureSignal display
    /// charged to it (ruling 9.3 item 4), for its subscription's admission.
    std::optional<DisplayBudgetLimits> displayBudgetLimitsAsPs3Subscriber(quint64 epoch) const;
    /// Prospective share after this session asks for additional raw-I/Q
    /// bytes. Admission reads it before the stream starts sending.
    std::optional<DisplayBudgetLimits> displayBudgetLimitsWithAdditionalDemand(
        quint64 epoch, quint64 applicationBytesPerSecond) const;
    /// Why a media session's share is short (ruling 9.3a): its own reason,
    /// before the mapping for a device without sessionHolder.
    DisplayBudgetReason displayBudgetShareReason(quint64 epoch) const;
    /// The limits last set, whichever peer is attached.
    std::optional<DisplayBudgetLimits> configuredDisplayBudgetLimits() const
    {
        return m_displayBudget;
    }
    /// True for a budget nereusd computed rather than read from its
    /// configuration (display_adaptive on, no limits configured): only an
    /// app that understands the budget reason is put in budget mode.
    void setDisplayBudgetForReasonPeersOnly(bool reasonPeersOnly);
    /// The reason of the Core's total (the governor's CoreBusy, or None).
    DisplayBudgetReason displayBudgetReason() const { return m_displayBudgetReason; }
    /// Fix wave 3 (ruling 9.3, the governor's floor): how many admitted
    /// network devices the display budget is split among now (those
    /// splitDisplayBudget shares it with), 0 with no budget in force. The
    /// load governor keeps one floor pan for each.
    int displayBudgetSharingCount() const;
    void setDisplayBudgetEnforcementEnabled(bool enabled);
    bool displayBudgetAvailable() const;
    bool displayBudgetAvailable(quint64 epoch) const;
    /// Splits the budget again and sends each media session whose budget
    /// entries changed its capabilities (Task 76).
    void publishDisplayBudgetCapabilities();
    using Ps3DisplayAdmissionHandler = std::function<bool(bool, QString*)>;
    void setPs3DisplayAdmissionHandler(Ps3DisplayAdmissionHandler handler);
    /// iPhone app Task 76: the admission handler told which media session
    /// asks (0 when the asker has none).
    using SessionPs3DisplayAdmissionHandler = std::function<bool(quint64, bool, QString*)>;
    void setSessionPs3DisplayAdmissionHandler(SessionPs3DisplayAdmissionHandler handler);
    /// Fix wave I5 (ruling 9.3): each media session's display demand, the
    /// charges of its displays as subscribed (before grants clamp them, a
    /// display refused for the budget included). DaemonMediaHub installs
    /// it; a session it has no controller for asks for nothing. Without a
    /// provider every budget-aware session asks for the whole total, as
    /// before several devices.
    using DisplayDemandProvider = std::function<std::optional<DisplayBudgetCharge>(quint64)>;
    void setDisplayDemandProvider(DisplayDemandProvider provider)
    { m_displayDemand = std::move(provider); }
    /// The primary media session's epoch (0 with none).
    quint64 mediaSessionEpoch() const;
    /// expectedEpoch is captured by the producer when its session starts;
    /// late work must never target a replacement session.
    bool sendMediaControl(const QJsonObject& payload, quint64 expectedEpoch);

    /// Observations are independently available even in a control-only session.
    /// Configure before accepting a client; never change negotiated support live.
    void setTelemetryEnabled(bool enabled);
    bool telemetryAvailable() const;
    bool telemetryAvailable(quint64 epoch) const;
    quint64 sessionEpoch() const { return mediaSessionEpoch(); }
    /// To the media session `expectedEpoch` names, when it negotiated
    /// telemetry (iPhone app Task 76: every session that did gets its own).
    bool sendTelemetry(const StationTelemetrySnapshot& snapshot, quint64 expectedEpoch);

    /// The capability descriptor this daemon would advertise right now.
    /// Public so a caller (and this task's tests) can inspect what a
    /// client is about to be told without standing up a client.
    StationCapabilities buildCapabilities() const;
    /// R-R3-46: what this Core offers a window of its radio's hardware:
    /// 0 nothing, 1 the `stepAtt` object, 2 also `alexAntennas`, the
    /// hardware apply step and the I/O board probe; the I/O board today
    /// raises it to 6 (the ioBoard object, the per-band antenna and filter
    /// policy verbs, and the transmit antennas and relays two-way).
    int radioHardwareVersion() const;
    // R-R3-47 / R-R3-22: 1 when this Core owns its accessories and mirrors
    // the `amplifier` and `rfkit` objects (remotePgxlControlVersion and
    // remoteRfKitControlVersion); 0 otherwise.
    int accessoryStatusVersion() const;
    // R-R3-47: remotePgxlControlVersion. 4 on a Core that owns its
    // accessories (the `amplifier` object, the configurePgxl,
    // disconnectPgxl and setPgxlConnectionSettings verbs, and the amp's own
    // settings on `accessorySettings` with their verbs); 4 from parity
    // Task 9 (setPgxlOperate, scanPgxlLan, setPgxlAddress); 0 otherwise.
    int pgxlControlVersion() const;
    // R-R3-47 / R-R3-22: remoteTgxlControlVersion. 2 on a Core that owns its
    // accessories (the tuner's own settings on `accessorySettings` and the
    // setTgxlName, setTgxlNetwork, saveTgxlSettings and readTgxlSettings
    // verbs; from 2, R-R3-49, setTgxlAntenna, setTgxlOperate and
    // setTgxlBypass; from 4, parity Task 8, moveTgxlRelay, scanTgxlLan and
    // setTgxlAddress); 0 otherwise.
    int tgxlControlVersion() const;
    // R-R3-49 (parity Task 1): transmitSettingsVersion. 1 on a Core with a
    // radio model: a receive-only Core takes a `transmit` write outside the
    // keying set (mox, tune, voxEnabled, twoToneActive) and a key on
    // isTransmitSettingKeyAcceptedOffAir's list while its radio is off the
    // air, and refuses each while it is on the air; 0 otherwise. 2 (parity
    // Task 2): also the TX and Phone/CW applets' settings on `transmit`,
    // each refused outside its range, and setTunePowerForTxBand. 3 (parity
    // Task 3): also the radio microphone settings, the Core's TX profiles
    // (activeTxProfile, txProfilesJson), the txProfile verbs and
    // rade.resetVocoder. 7 (parity Task 7): also PureSignal arming and its
    // settings, off the air. 8 (parity Task 13): also Hardware Config's OC
    // transmit pins off the air, and its OC pin actions, TX Display Cal and
    // Volts/Amps Calibration on and off the air.
    int transmitSettingsVersion() const;
    // R-IOS-27, R-IOS-06: bandSelectVersion. 1 on a Core with a radio
    // model: it takes slice.selectBand from a peer at minor 11; 0 otherwise.
    int bandSelectVersion() const;
    // R-R3-13 / R-R3-49 (parity Task 15): meterReadingsVersion. 1 on a Core
    // whose radio model runs its meter pump (the slices carry the ADC and
    // AGC readings, and a window's Multimeter polling delay sets the pump's
    // rate at once); 0 otherwise.
    int meterReadingsVersion() const;
    // R-R3-49 / R-R3-21 / R-R3-40 (parity Task 16): dspInfoVersion. 1 on a
    // Core with a local radio model: its `radio` object carries
    // dspOptionsLastApplyMs, its slices
    // minNotchWidthHz, and it takes dsp.filterResponse; 0 otherwise.
    int dspInfoVersion() const;
    // R-IOS-25 / R-R3-49 (parity Task 19): recordStreamVersion. 1 on a Core
    // with a local radio model: records.subscribe / records.unsubscribe
    // and record.batch (the `spots` stream and each station source's
    // spotConsole:<source>), the read-only `spotSources` object, and the
    // spots.* verbs; 0 otherwise.
    int recordStreamVersion() const;
    // R-IOS-18 / R-R3-49 (parity Task 21): stationRadiosVersion. 1 when the
    // Core chooses its own radio (nereusd attaches StationRadios): the
    // `stationRadios` stream and the station.selectRadio,
    // station.rescanRadios, station.setRadioModel and station.forgetRadio
    // verbs; 0 otherwise.
    int stationRadiosVersion() const;
    // R-R3-49 / A11 (parity Task 28): txDisplayVersion. 3 (parity Task 30:
    // the Core applies a window's TX Display analyzer settings at once;
    // parity Task 31: a subscribe may carry `duplex`) while media is on and the Core has a TX analyzer
    // (RadioModel::txDisplayFeed); 0 otherwise. txDisplayAvailable(epoch): that session's peer agreed minor
    // 11 and was told it, so its media start may declare it.
    int txDisplayVersion() const;
    bool txDisplayAvailable(quint64 epoch) const;
    bool miniDisplayAvailable(quint64 epoch) const;
    // R-IOS-16 (iPhone app plan Task 28 fix wave, the safety review's
    // Important 5): controlChannelVersion. 1 when the Core has a bound
    // certificate, so it can answer an introduction through the remote
    // access service with the control session over a data channel (link
    // section 20); 0 otherwise.
    int controlChannelVersion() const;
    // R-IOS-13 / R-R3-49 (parity Task 32): txMonitorAudioVersion. 1 while
    // media is on with the Core's own radio model: the transmit monitor
    // goes to the device that holds transmit (monitor-audio); 0 otherwise.
    // txMonitorAudioAvailable(epoch): that session's peer agreed minor 11
    // and was told it, so its media start may declare it.
    int txMonitorAudioVersion() const;
    bool txMonitorAudioAvailable(quint64 epoch) const;
    int remoteIqVersion() const;
    bool remoteIqAvailable(quint64 epoch) const;
    // R-IOS-26 / R-R3-49 (iPhone plan Task 22, parity Task 20):
    // stationFreedvVersion. 1 with recordStreamVersion 1 and the Core's own
    // FreeDV Reporter: the freedvStations stream, the FreeDV Reporter
    // console and state in spotSources, and the freedv.* verbs; 0
    // otherwise.
    int stationFreedvVersion() const;
    // R-R3-49 / R-IOS-18 (remote-window parity Task 22, iPhone app plan
    // Task 25): supportBundleVersion. 1 on every Core with a radio model:
    // support.collect, support.setLogCategories, the `coreLog` record
    // stream and radio's logCategories.
    int supportBundleVersion() const;
    // nereusd's configuration file, carried (secrets removed) in the Core's
    // support bundle. Set by DaemonApp; empty on a desktop hosting the Core.
    void setSupportConfigPath(const QString& path) { m_supportConfigPath = path; }
    // R-IOS-13 / R-R3-49 (iPhone plan Task 39 row A10): txModMonitorVersion.
    // 1 with recordStreamVersion 1: the AM Mod Monitor's readings on the
    // txAmModulation and txAmModulationFeedback streams while a subscriber
    // watches and the radio is keyed in AM, SAM or DSB, the
    // txModMonitor.reset verb, and a window's ModMon/FbStream applied at
    // once; 0 otherwise.
    int txModMonitorVersion() const;
    /// For a test: the Core's side of the Mod Monitor streams, or null.
    ModMonitorPublisher* modMonitorPublisherForTest() const { return m_modMonitor.get(); }
    // iPhone app plan Task 29 (R-IOS-16; link section 21): mediaReplaceVersion
    // 1 whenever media is on (the media `replace` operation);
    // controlSwitchVersion 1 always (session.pathTicket, path.join and
    // moving a session to another connection).
    int mediaReplaceVersion() const;
    int controlSwitchVersion() const;
    /// Task 29 step 2b (link section 21, "The media tunnel"): 1 on every
    /// Core of this build; told a peer with media (mediaTunnelVersion).
    int mediaTunnelVersion() const { return 1; }
    /// The session with media `epoch` was told mediaTunnelVersion 1, so its
    /// media start may declare the tunnel even before a move to WebSocket.
    bool mediaTunnelAvailable(quint64 epoch) const;
    /// The ICE settings for that session's media over the tunnel (the
    /// tunnel made on first use); none unless the current path carries binary.
    std::optional<IceConfiguration> mediaTunnelIceConfiguration(quint64 epoch);
    /// iPhone app plan Task 29: the session with media `epoch` agreed
    /// minor 11 and was told mediaReplaceVersion 1, so it may send
    /// `replace`.
    bool mediaReplaceAvailable(quint64 epoch) const;
    /// The direct media ladder (link section "Direct media"): the STUN
    /// servers the rendezvous hello gave this Core (only `stun:` and
    /// `stuns:` URLs are kept; never TURN, credentials or tokens), and
    /// what their names resolved to. Every media connection gathers from
    /// the chosen one, the tunnel's included. A change is told again to
    /// every session that was told mediaStunUrls.
    void setMediaStun(const QStringList& urls, const HostFamilies& families = {});
    QStringList mediaStunUrls() const { return m_mediaStunUrls; }
    /// The STUN server a media connection uses (none: host candidates).
    std::optional<IceServerAddress> mediaStunServer() const;
    /// 1 on every Core of this build: told, with mediaStunUrls, to a peer
    /// with media whose hello declared `mediaDirect` 1.
    int mediaDirectVersion() const { return 1; }
    /// The session with media `epoch` was told mediaDirectVersion 1, so a
    /// `replace` may carry `mediaDirectVersion` 1.
    bool mediaDirectAvailable(quint64 epoch) const;
    /// A direct-only replacement's ICE settings: host candidates and the
    /// STUN server, no relay and no tunnel.
    IceConfiguration mediaDirectIceConfiguration() const;
    /// iPhone app plan Task 29: whether this Core allows the relay
    /// (nereusd.conf `relay`), told to every device as `relayAllowed`.
    /// Default true, the setting's default; DaemonApp sets it.
    void setRelayAllowed(bool allowed) { m_relayAllowed = allowed; }
    bool relayAllowed() const { return m_relayAllowed; }
    /// iPhone app plan Task 29 (link section 21.2): how long a ticket from
    /// session.pathTicket stays good.
    static constexpr int kPathTicketLifetimeMs = 10000;
    /// Test seam: the ticket lifetime in use.
    void setPathTicketLifetimeMsForTest(int ms) { m_pathTicketLifetimeMs = ms; }
    /// Test seam: the station's move deadline for connections accepted from
    /// now on (SwitchableTransport::kSwitchDeadlineMs).
    void setPathSwitchDeadlineMsForTest(int ms) { m_pathSwitchDeadlineMs = ms; }
    /// iPhone app plan Task 29: sessions moved to another connection since
    /// this server started (for a test and the log).
    int sessionsMoved() const { return m_sessionsMoved; }
    /// Parity Task 21: the Core's radios (nereusd's DaemonApp owns it).
    void setStationRadios(StationRadios* radios);
    /// For a test: the stream by name (spots, spotConsole:<source>), or
    /// null.
    RecordStream* recordStreamForTest(const QString& name) const;
    // R-R3-49 (parity Task 7): the peer was offered transmitSettingsVersion
    // 7: it arms PureSignal and changes pureSignalSettings off the air.
    bool pureSignalArmingOffered(SessionTransport* transport) const;
    /// Remote parity on the air (transmitSettingsVersion 13): whether a
    /// transmit setting from `transport` is taken while the radio is on the
    /// air: on a receive-only Core from a peer offered the transmit
    /// settings, otherwise from a session the station transmit gate
    /// permits.
    bool takesTransmitSettingsOnAir(SessionTransport* transport) const;
    // R-R3-49 (parity Task 1): the one list of transmit settings keys a
    // receive-only Core takes while its radio is off the air (today the
    // DSP > Options TX keys, DspOptions<Setting><Mode>Tx). Every other
    // transmit-side key a receive-only Core refuses stays refused.
    static bool isTransmitSettingKeyAcceptedOffAir(const QString& key);
    // R-R3-46 / R-R3-49 (parity Task 13): the keys on that list the Core
    // also takes while its radio is on the air, because Thetis changes them
    // while transmitting (the OC pin actions, TX Display Cal and Volts/Amps
    // Calibration). The on-air refusal skips them.
    static bool isTransmitSettingKeyTakenOnAir(const QString& key);
    // R-R3-47: remoteRfKitControlVersion. 4 on a Core that owns its
    // accessories (the `rfkit` object with its interface, antenna, tuner
    // and band-follow rows, the configureRfKit, disconnectRfKit and
    // setRfKitEnabled verbs, from 3 the resetRfKitError verb and a
    // window's auto-reconnect and poll interval applied at once, and from
    // 4 setRfKitOperate, setRfKitAntenna, setRfKitTciMode and
    // setRfKitAddress, parity Task 10); 0 otherwise.
    int rfKitControlVersion() const;
    // R-R3-48: stationTciVersion. 1 on a Core that runs its own station
    // TCI server (the `stationTci` object and the setStationTci verb); 2
    // (parity Task 23) with the record streams: the tciClients stream,
    // setStationTciOptions and disconnectStationTciClient.
    int stationTciVersion() const;
    // R-R3-47 / R-R3-22: accessoryDataVersion. 2 on a Core that owns its
    // accessories (the `accessoryData` object and the setTxInterlockPolicy,
    // setPgxlPowerCap and clearAccessoryFaults verbs; from 2 the RF-Kit's
    // rfkit* connection counts, parity Task 10); 0 otherwise.
    int accessoryDataVersion() const;
    int accessoryTxVersion() const { return accessoryDataVersion() > 0 ? 1 : 0; }

    /// iPhone app Task 71 (R-IOS-02): who holds a place on the Core, and
    /// the mirrored `connectedDevices` object. Never null. Task 48
    /// registers a hosting desktop's own window on the registry; the LAN
    /// announcement and the Bonjour record count its placesTaken().
    DeviceSessionRegistry* deviceSessions() const { return m_deviceSessions.get(); }
    ConnectedDevicesFacade* connectedDevices() const { return m_connectedDevices.get(); }
    /// 1: the Core admits up to four devices and sends `connectedDevices`
    /// to a peer that declared the hello feature `sessionHolder` 1 with
    /// deviceAuth 1, at minor 11 (the design's ruling 10.1), and takes
    /// session.leave.
    int sessionHolderVersion() const { return 1; }
    /// Slice control plan Task 4: 1 on a Core that runs its radio: it sends
    /// the `SliceAccess` objects, each joined slice as `slice:<id>` and
    /// every other as `marker:<id>`, and takes slice.listen,
    /// slice.stopListening, slice.takeControl and slice.release, to a peer
    /// at minor 11 that declared sliceAccess 1 with sessionHolder.
    /// Take-over parity: 2 adds Take it back on the controlTaken notice
    /// (notice.takeBack runs slice.takeControl with the notice's slice).
    /// Core-slice take-over (JJ, 2026-09-30): 3, with nobody at the Core's
    /// desktop a device may take the Core's own slice (handOffRefusal).
    /// A peer is sent the lower of this and the version its hello declared.
    int sliceAccessVersion() const;
    /// Slice control plan Task 4: sliceAccessVersion 1 reached `transport`
    /// (sessionHolderVersion 1, and sliceAccess 1 in its hello).
    bool peerHasSliceAccess(SessionTransport* transport) const;
    /// Take-over parity: sliceAccessVersion 2 reached `transport` (its hello
    /// declared sliceAccess 2): its controlTaken notices offer Take it back.
    bool peerTakesControlBack(SessionTransport* transport) const;
    /// Core-slice take-over (JJ, 2026-09-30): sliceAccessVersion 3 reached
    /// `transport` (its hello declared sliceAccess 3): with nobody at the
    /// Core's desktop it may take the Core's own slice.
    bool peerTakesCoreSlice(SessionTransport* transport) const;
    /// Slice control plan Task 4: each attached view's `slice:` and
    /// `marker:` forms of `sliceId` after its controller or its listeners
    /// changed: object.destroy of the form it had, object.create of the
    /// form it has now.
    void onSliceAccessChanged(int sliceId);
    /// Slice control plan Task 4: the listen, stop listening, take control
    /// and release checks, for the hosting desktop's own window as for a
    /// device's command (Task 10). Never null on a Core with a radio model.
    SliceAccessController* sliceAccessController() const { return m_sliceAccessController; }
    /// Slice control plan Task 13: whether `sliceId` is transmitting now, as
    /// the take and release checks read it (the hosting desktop's chooser).
    bool sliceOnAir(int sliceId) const { return sliceTransmitting(sliceId); }

    // ── iPhone app plan Task 34: transmit (R-IOS-02, R-IOS-03, R-IOS-13) ──

    /// The config key remote_transmit (DaemonConfig): allow lets a session
    /// the gate permits transmit (StationTxGate), and lifts the Core's
    /// blanket receive-only policy; deny keeps it, and every session's
    /// txPermitted is false. Deny by default, as every Core was before;
    /// nereusd applies its config (default allow).
    void setRemoteTransmitAllowed(bool allowed);
    bool remoteTransmitAllowed() const { return m_txGate.remoteTransmitAllowed(); }
    /// Who holds transmit (never null).
    TransmitHolder* transmitHolder() const { return m_transmitHolder.get(); }
    /// Test hook (slice control plan Task 4): the transmit slice `device`
    /// last chose (ruling 8.10), or -1.
    int chosenTxSliceForTest(const QByteArray& device) const
    {
        return m_chosenTxSlice.value(device, -1);
    }
    /// Slice control fix wave (Important 4): the slice `device` last chose
    /// for transmit itself (tx.setTxSlice, or the hosting desktop's own
    /// selection for the station device), or -1. Never written by the
    /// binding a holder gets by itself; cleared when control of the slice
    /// passes from the device, when the slice closes and when the device
    /// is removed. Task 11 refuses keying without it.
    int explicitTxSliceFor(const QByteArray& device) const
    {
        return m_explicitTxSlice.value(device, -1);
    }
    /// The Core's model (the conformance runner presses its radio's PTT).
    RadioModel* radioModel() const;
    /// Fix wave 2 (ruling 8.1): a desktop that hosts this Core names the
    /// station device after itself, so its own MOX or TUNE is published,
    /// and refused on the air, in the desktop's words. Unset on a Core no
    /// desktop hosts: the station device's own keys are "Radio".
    void setStationDeviceWords(const QString& name, const QString& shortName);
    /// Slice control plan Task 10: a command.invoke from the hosting
    /// desktop, run as the station device through the same dispatcher,
    /// checks, confirm step and SliceAccessController a remote device's
    /// goes through. `answer` gets its command.result (now or on a later
    /// turn); `question` gets any confirm.request it raises, and every
    /// later one for the station device until another invoke names its
    /// own. Only the slice verbs the desktop uses are taken (removeSlice,
    /// addSlice, addSliceOnPan, setActiveSliceById, slice.listen,
    /// slice.stopListening, slice.takeControl, slice.release,
    /// slice.setListenLevel, confirm.proceed, confirm.cancel,
    /// notice.takeBack); any other is refused.
    void invokeAsStationDevice(const SessionMessage& invoke, StationAnswer answer,
                               StationAnswer question);
    /// Slice control plan Task 10: where the station device's notices go
    /// (a slice it listened to closed, a take of its slice). Unset, they
    /// wait as an away device's do; set, any waiting are handed over.
    void setStationNoticeHandler(StationAnswer notice);
    /// iPhone app plan Task 77 (ruling 8.9a, D64): the hosting desktop's
    /// MOX or TUNE while another device holds transmit takes only through
    /// tx.take's rules. AtOnce runs the take (`done(taken)` when it ends;
    /// the button keys after it); Ask means the desktop shows its question
    /// and calls again with the holderEpoch and keyed state it showed;
    /// AlreadyHeld and Refuse change nothing.
    TransmitHolder::TakeVerdict takeTransmitForStation(std::optional<quint64> shownEpoch,
                                                       std::optional<bool> shownKeyed,
                                                       std::function<void(bool taken)> done = {});
    /// 1: txPermitted per session, the `tx.setTxSlice` verb, the on-air
    /// refusals, and (Task 35) keying: `tx.key`, `tx.unkey`, `tx.tune` and
    /// `tx.twoTone`, for a peer at minor 11 whose hello declared remoteTx 1.
    /// 2 (Task 77): `tx.take` (with sessionHolderVersion 1), its
    /// takeTransmit question and transmitTaken notice, the radio's PTT
    /// taking transmit, the transmitter's settings held by the holder, and
    /// `tx.tunerTune`, the Tuner Genius autotune.
    int remoteTxVersion() const { return 2; }
    /// Task 35: keying from a remote device; null without a Local model.
    RemoteKeying* remoteKeying() const { return m_remoteKeying.get(); }
    /// Task 37 (R-IOS-13; remote design section 12.1): the transmit
    /// watchdog; null without a Local model. It watches a device while it
    /// is keyed or has VOX armed and stops transmitting when its
    /// keepalives (tx.keepalive on the session, or the media connection's
    /// "tx" data channel) stop for more than 400 ms, or its session ends.
    RemoteTxWatchdog* txWatchdog() const { return m_txWatchdog.get(); }
    /// Task 37: a keepalive from the media connection's "tx" data channel
    /// (RemoteTxWatchdog::channelKeepalive's 13 bytes), for the device the
    /// media session `epoch` is for. Anything else is ignored. TX mic
    /// thread: `heldUs` is how long it waited since its receipt off the
    /// network; the watchdog counts it as heard then.
    void txChannelMessage(quint64 epoch, const QByteArray& message, qint64 heldUs = 0);
    /// Task 37 (remote design section 12.3): `deviceId`'s microphone line
    /// starved (true) or carries audio again (false) while it is keyed on
    /// it; the per-mode action (StarvationPolicy) follows.
    void remoteMicStarved(const QByteArray& deviceId, bool starved);
    /// Task 37: the device whose accepted write turned VOX on, while VOX is
    /// on; empty when VOX is off or was turned on at the Core itself.
    QByteArray voxArmedBy() const { return m_voxArmedBy; }
    /// iPhone app plan Task 39 (D14, R-IOS-13): the mirrored `txState`
    /// object (never null; follows a Local model).
    TransmitState* transmitState() const { return m_transmitState; }
    /// 1: the Core sends `txState` to a peer at minor 11 whose hello
    /// declared remoteTx 1 (after remoteTxVersion in its capabilities).
    /// 2 (fix wave I4): `txState` names the holder of transmit (ruling 8.1)
    /// and carries keyedForSeconds.
    int txStateVersion() const { return 2; }
    /// Parity Task 33 (R-R3-49, R-R3-32): 1 when `txState` also carries
    /// forwardAdcRaw and reflectedAdcRaw and the Core keeps the
    /// txCfcCompression record stream (a Core with its own radio model and
    /// record streams); sent right after txStateVersion and only with it.
    int txReadingsVersion() const;
    /// Parity Task 33: whether the Core reads the CFC display now (a peer
    /// subscribes to txCfcCompression, the radio is keyed and CFC is on).
    bool cfcCompressionPollingForTest() const;
    /// Parity Task 33: replaces TxChannel::getCfcDisplayCompression for a
    /// test (same contract: fills kCfcDisplayBinCount values, true when
    /// WDSP has new data).
    void setCfcDisplayReaderForTest(std::function<bool(double*, int)> reader);
    /// iPhone app plan Task 25 (R-IOS-18): 1 on a Core whose audio engine
    /// publishes VAX devices (a Core the desktop hosts), with record
    /// streams: the `vax` object and the `vaxLevels` stream, for a peer whose
    /// hello declared `vax` 1. 0 otherwise.
    int vaxVersion() const;
    /// The `vax` object (null on a Core without an audio engine).
    StationVax* stationVax() const { return m_stationVax; }
    /// Whether the Core reads the VAX meters now (a peer subscribes to
    /// vaxLevels).
    bool vaxLevelsPollingForTest() const;
    /// Replaces the audio engine's VAX meters for a test: fills the four
    /// receive levels and the transmit level, 0 to 1.
    void setVaxLevelReaderForTest(std::function<void(double*, double*)> reader);
    /// Reads the VAX meters once now, as the 5 Hz timer does.
    void pollVaxLevelsForTest() { pollVaxLevels(); }
    /// The gate's answer for `transport` (what its txPermitted says).
    TxDecision txDecisionFor(SessionTransport* transport) const;
    /// The refusal a capabilities message carries (empty without one).
    static TxRefusal txRefusalOf(const StationCapabilities& caps);
    /// Releases transmit if `deviceId` holds it, through a transfer to
    /// nobody (a fifth device replacing it, Task 41).
    void releaseTransmitFor(const QByteArray& deviceId, const QString& reason);
    /// iPhone app Task 73 (ruling 5.2 step 2): the saved slices of
    /// `deviceId` that did not fit at its last admission (no free letter or
    /// no receiver), kept in its layout store and reported by Task 74's
    /// slicesNotRestored or graceEnded notice after snapshot.complete.
    QList<SavedSlice> slicesNotRestored(const QByteArray& deviceId) const
    {
        return m_slicesNotRestored.value(deviceId);
    }
    /// Places taken as the LAN announcement and the Bonjour record count
    /// them: DeviceSessionRegistry::placesTaken(), or 0 on a Core no device
    /// has claimed (ruling 10.4).
    int devicesConnectedForDiscovery() const;

    // ---- Subsystem accessors, non-owning, for tests and diagnostics ----
    StateMirror* stateMirror() const { return m_mirror; }
    ObjectRegistry* objectRegistry() const { return m_registry; }
    SettingsProxyServer* settingsServer() const { return m_settingsServer; }

signals:
    /// Fix wave C2: voxArmedBy() changed (VOX follows the device that armed
    /// it; the media controllers mark which line VOX listens to).
    void voxArmedByChanged(const QByteArray& deviceId);
    void listeningChanged(bool listening);
    void displayBudgetChanged();
    void telemetrySessionStarted(quint64 epoch);
    void telemetrySessionEnded(quint64 epoch);
    void mediaSessionStarted(quint64 epoch);
    void mediaSessionEnded(quint64 epoch);
    void mediaControlReceived(const QJsonObject& payload, quint64 epoch);
    /// A peer completed the full section 7.0 sequence and is now one of the
    /// admitted sessions.
    void clientAuthenticated(const QString& peer);

    /// A peer went away, for any reason, with the reason. Fires for
    /// unauthenticated peers too.
    void peerDisconnected(const QString& peer, const QString& reason);

    // iPhone app Task 71: sessionPreempted is gone with preemption. No
    // sign-in ever ends another device's session; a device's own newer
    // connection ends its older one with sameDevice (ruling 4.8).

    /// The heartbeat declared a peer dead: it stopped answering pings
    /// without closing. Distinct from peerDisconnected's ordinary path
    /// because this is the case that has no TCP close behind it at all.
    void peerHeartbeatTimeout(const QString& peer);

private:
    /// R-R3-49 (parity Task 1): the on-air reason for a settings write or
    /// remove of a key on isTransmitSettingKeyAcceptedOffAir's list on a
    /// receive-only Core; empty when the key may be applied now.
    QString transmitSettingOnAirRefusal(const QString& key) const;
    // R-R3-49 / R-IOS-27 (JJ's ruling, follow Thetis): a raw PA profile
    // key on the air (RadioModel::paSettingOnAirRefusal), for this peer:
    // taken only for the active profile's transmitting band, from the
    // device that holds transmit. `value` is null for a remove.
    QString paSettingOnAirRefusalFor(SessionTransport* transport, const QString& key,
                                     const QString* value) const;
    /// Addendum G-42 (JJ's ruling 2026-09-28): a key the transmit gate
    /// reads (RadioModel::kExtendedTransmitKey). Its write or removal needs
    /// this session's transmit permission (txDecisionFor), waits while the
    /// radio is on the air, and a write must be "True" or "False".
    static bool isTransmitGateSettingKey(const QString& key);
    /// The refusal for that write (`value` set) or removal (`value` null);
    /// empty when it may be applied now.
    QString transmitGateSettingRefusal(SessionTransport* transport, const QString& key,
                                       const QVariant* value) const;
    /// R-R3-49 (parity Task 1): this peer agreed minor 11 and was offered
    /// transmitSettingsVersion 1.
    bool transmitSettingsOffered(SessionTransport* transport) const;
    /// Whether a receive-only Core refuses a settings write or remove of
    /// `key` from this peer as transmit configuration (with
    /// kReceiveOnlyTransmitReason).
    bool receiveOnlyRefusesKey(SessionTransport* transport, const QString& key) const;

    /// Per-connection state. Deliberately small: everything that is not
    /// per-CONNECTION (the mirror, the registry, the dispatcher, the
    /// settings server) is shared among the admitted sessions until Tasks
    /// 72 and 76 give each device its own view and media (see the topology
    /// note). Who holds a place is DeviceSessionRegistry's.
    /// iPhone app Task 14: one connection's pairing (StationServer.cpp).
    struct PairingAttempt;

    struct Peer {
        SessionTransport* transport = nullptr;
        QString description;
        bool helloReceived = false;
        /// iPhone app plan Task 27: a pairing through the rendezvous's
        /// mailbox (acceptPairingMailbox()), which carries pair.* only.
        bool mailboxPairing = false;
        /// iPhone app plan Task 28 fix wave (review Important 1 and 2): a
        /// control connection the remote access service introduced
        /// (acceptIntroducedTransport()). It never pairs and signs in by
        /// device key only; while it is still connecting it counts against
        /// the one source every introduced connection shares.
        bool introduced = false;
        /// The service's id for that introduction (DeviceAuthRequest's
        /// `introduction`, which the sign-in limits key on).
        QString introductionId;
        /// iPhone app plan Task 29: the paired device the service
        /// introduced (its id, as deviceId below), or empty.
        QByteArray introducedDeviceId;
        /// iPhone app plan Task 29 (link section 21.2): this session's
        /// ticket from session.pathTicket (32 bytes; never logged) and
        /// when it stops being good. Empty with none.
        QByteArray pathTicket;
        QDeadlineTimer pathTicketDeadline;
        /// iPhone app plan Task 29 (link section 21.3): the ICE settings
        /// of the connection through the service this session left, so its
        /// media keeps the service's STUN server after a move.
        std::optional<IceConfiguration> serviceIce;
        /// Task 29 step 2b: the media tunnel on this session's WebSocket,
        /// made the first time its media declares it.
        std::shared_ptr<MediaTunnel> mediaTunnel;
        bool authenticated = false;
        quint16 agreedMinor = 0;
        /// iPhone app Task 4: the major the peer's hello chose (0 until
        /// accepted) and what that hello declared.
        quint16 agreedMajor = 0;
        QHash<QByteArray, int> features;
        bool snapshotComplete = false;
        /// iPhone app Task 12: this connection's device sign-in challenge
        /// (32 bytes, sent in the hello) and, once authenticated, the
        /// paired device it signed in as (empty for a token sign-in that
        /// enrolled nothing).
        QByteArray challenge;
        QByteArray deviceId;
        /// iPhone app Task 13: signed in with the pairing token (whether or
        /// not it also enrolled its device key). Retiring the token ends it.
        bool signedInWithToken = false;
        /// iPhone app Task 14: this connection's pairing, from pair.start
        /// to its end. Shared, not owned alone, only because Peer is copied.
        std::shared_ptr<PairingAttempt> pairing;
        /// iPhone app Task 71: the device this connection's session is for
        /// (DeviceSessionRegistry's id: a paired device's raw id, or
        /// "token:<n>"), set once it is admitted; empty before, and for a
        /// connection turned away from a full Core.
        QByteArray sessionDeviceId;
        /// How this admitted session's end reaches the registry: a
        /// connection replaced by its own device's newer one (sameDevice),
        /// or one whose device left on purpose or was revoked, frees or
        /// keeps its place by itself and must not be marked away.
        bool placeSettled = false;
        bool dropping = false;
        /// Authenticated paired peer waiting for a place, with no session.
        quint64 heldSerial = 0;
        bool leaving = false;
        // iPhone app Task 73: admitted as a device that already held a place
        // (ruling 4.8), which keeps its slices as they are.
        bool returning = false;
        /// iPhone app Task 72: this session's view of the mirror (ruling
        /// 5.6), made when it is let in and closed when it ends; and its
        /// id, for the dispatcher's owner string station:<sessionId>.
        QPointer<MirrorView> view;
        quint64 sessionId = 0;
        /// Changes before a primary path move, even if the move fails.
        quint64 txWatchGeneration = 0;
        bool txWatchPathChanging = false;
        /// iPhone app plan Task 34: the txPermitted this session was last
        /// sent, so a change is sent again and nothing else is.
        bool txPermittedSent = false;
        int txWatchPathVersionSent = 0;
        /// Desktop remote transmit: the refusal it was last sent with it
        /// (empty for a peer without remoteTx, or while permitted).
        TxRefusal txRefusalSent;
        /// iPhone app Task 76: this admitted session's media epoch (never
        /// 0 once admitted, unique for the Core's life), its share of the
        /// display budget and why, and what its capabilities last said of
        /// the budget, so a change is published once.
        quint64 mediaEpoch = 0;
        std::optional<DisplayBudgetLimits> budgetShare;
        DisplayBudgetReason budgetShareReason = DisplayBudgetReason::None;
        QByteArray publishedBudget;

        /// Pings sent since the last pong. Reset to 0 by every pong; the
        /// heartbeat tick declares death when it reaches maxMissedPongs().
        int pingsAwaitingPong = 0;

        /// Owned single-shot finish-the-handshake-or-drop timer, parented
        /// to the transport so it dies with it. Stopped once the peer's
        /// snapshot has been sent (R-R3-16/17). An OWNED timer rather than static
        /// QTimer::singleShot deliberately: the plan's own section 13 note
        /// records that PgxlConnection and TgxlConnection get that wrong,
        /// and cancellability matters more here because this subsystem
        /// gates a transmitter.
        QTimer* authDeadline = nullptr;
    };

    // Control logging lane: logging only (ControlLog).
    static ControlLog::PeerInfo controlLogPeer(const Peer& peer);
    void noteControlIn(SessionTransport* transport, const SessionMessage& message);
    void logControlResult(SessionTransport* transport, const SessionMessage& message);
    SessionTransport* controlTransportForDevice(const QByteArray& deviceId) const;
    void controlLogKeepalive(const QByteArray& deviceId, ControlLog::KeepaliveChannel channel,
                             qint64 ageMs, bool watched);

    void onNewWebSocketConnection();
    void handleTxWatchTicket(SessionTransport* transport, const SessionMessage& message);
    void handleTxWatchRelay(SessionTransport* transport, const SessionMessage& message);
    void handleSettingsExport(SessionTransport* transport, const SessionMessage& message,
                              const QByteArray& wire);
    bool settingsExportEligible(SessionTransport* transport) const;
    void expireSettingsExports();
    qint64 settingsExportNow() const;
    bool txWatchEligible(SessionTransport* transport) const;
    bool txWatchRelayEligible(SessionTransport* transport) const;
    bool txWatchRelayCapability(SessionTransport* transport) const;
    bool txWatchAuthorityCurrent(SessionTransport* transport) const;
    bool txWatchBindingCurrent(SessionTransport* transport, quint64 sessionId,
                               const QByteArray& deviceId, quint64 generation) const;
    struct PendingRelayWatch;
    void retirePendingRelayWatch(SessionTransport* primary);
    void onTransportText(SessionTransport* transport, const QByteArray& wire);
    void onTransportClosed(SessionTransport* transport);
    void onHeartbeatTick();

    // Every handler takes the TRANSPORT and looks its Peer up itself,
    // never a Peer& held across a call. dropPeer() erases from m_peers,
    // and Qt6's QHash does not promise a reference into it survives an
    // unrelated erase -- promoteToSession() drops the INCUMBENT session
    // while holding the newcomer's entry, which is exactly the shape that
    // would go wrong.
    void handleHello(SessionTransport* transport, const SessionMessage& message);
    void handleAuthRequest(SessionTransport* transport, const SessionMessage& message);
    void handlePropertyWrite(SessionTransport* transport, const SessionMessage& message);
    // Parity Task 19 (R-IOS-25): the record streams.
    void setUpRecordStreams();
    // Parity Task 22: the Core's log as a record stream, fed from the log
    // sink while someone follows it.
    void setUpCoreLogStream();
    void pullCoreLog();
    // R-IOS-13 / R-R3-49: the Mod Monitor's two streams and txModMonitor.reset.
    void setUpModMonitorStreams();
    void handleModMonitorReset(SessionTransport* transport, const SessionMessage& message);
    // R-IOS-13 / R-R3-49 (txEqCurveVersion 2): txEq.setCurve and
    // txEq.resetCurve, applied as this peer's txEqParaEqData write
    // (applyPropertyWrite), so every rule that write meets applies.
    void handleTxEqCurveCommand(SessionTransport* transport, const SessionMessage& message);
    // transmitSettingsVersion 15: cfc.setProfile, the CFC band editor
    // applied at once against an expected revision, as the asking
    // connection's cfcParaEqData write.
    void handleCfcProfileCommand(SessionTransport* transport, const SessionMessage& message);
    void handleRecordsCommand(SessionTransport* transport, const SessionMessage& message);
    void scheduleRecordFlush();
    void flushRecordStreams();
    // Parity Task 33: the CFC display read every 50 ms while it is wanted.
    void updateCfcCompressionPolling();
    void pollCfcCompression();
    // iPhone app plan Task 25: the vaxLevels stream's reads.
    void updateVaxLevelsPolling();
    void pollVaxLevels();
    void handleSettingsWrite(SessionTransport* transport, const SessionMessage& message);
    /// The body of handleSettingsWrite after its checks: applies the write
    /// through the settings proxy. A refusal goes to `refusal` when given,
    /// otherwise to the writer as settings.reject. True when applied.
    bool applySettingsWrite(SessionTransport* transport, const SessionMessage& message,
                            QString* refusal);
    /// R-R3-46 / R-R3-49: after a low-pass edge write or removal, stores the
    /// neighbouring edges the Filters tab's rule moves (no-op for other keys).
    void applyAlexLpfNeighbourRule(const QString& key);
    void handleSettingsRemove(SessionTransport* transport, const SessionMessage& message);
    // iPhone app Task 14 (R-IOS-08): pairing, before any sign-in.
    void handlePairStart(SessionTransport* transport, const SessionMessage& message);
    void handlePairSpake(SessionTransport* transport, const SessionMessage& message);
    void handlePairConfirm(SessionTransport* transport, const SessionMessage& message);
    void handlePairFailFromDevice(SessionTransport* transport);
    /// Sends pair.fail with `reason` and `retryAfterMs`, then ends the
    /// connection. A code the connection had taken is burned by dropPeer.
    void sendPairFail(SessionTransport* transport, const QString& reason, qint64 retryAfterMs);
    /// Hashes the window's current code (one Argon2id hash per code) on a
    /// worker thread, never on this event loop; finishPairingHash() takes
    /// the result back here, keeps it while the code is current, and sends
    /// step 0 to the connections waiting for it.
    void startPairingHash();
    void finishPairingHash(quint64 serial, const QByteArray& stored);
    /// Sends step 0 from the kept hash.
    void beginCodeExchange(SessionTransport* transport);
    /// Signed in with a paired device's own key (not the old token): the
    /// only connections the pairing code is sent to.
    bool peerSeesPairingCode(SessionTransport* transport) const;
    /// `message` as `transport` may see it: the pairing code blanked on the
    /// `devices` object and in pairing.open's result for any other
    /// connection.
    SessionMessage withPairingCodeFor(SessionTransport* transport,
                                      const SessionMessage& message) const;

    /// acceptTransport(), acceptPairingMailbox() and
    /// acceptIntroducedTransport(): `mailbox` skips the hello and admits
    /// pair.* only; `introduced` marks a connection the service introduced.
    void adoptTransport(SessionTransport* transport, bool mailbox,
                        bool introduced = false, const QString& introductionId = QString(),
                        const QByteArray& introducedDeviceId = QByteArray());
    /// iPhone app plan Task 29 (link section 21.2): session.pathTicket from
    /// an admitted session, and path.join on a new connection.
    void handlePathTicket(SessionTransport* transport, const SessionMessage& message);
    void handlePathJoin(SessionTransport* transport, const SessionMessage& message);
    /// The radio is idle (MoxController in Rx: not keyed, no MOX delay
    /// timer running), so a session or its media may move.
    bool radioIdleForPathChange() const;
    /// The key of `transport`'s entry in m_peers: itself, or the session
    /// transport that carries it (a caller holding the connection it
    /// handed to acceptTransport()). Null when neither is known.
    SessionTransport* peerKey(SessionTransport* transport) const;
    /// iPhone app Task 71: after an accepted sign-in, asks the registry
    /// who is let in (ruling 4.4) and ends the device's own older
    /// connection (sameDevice), admits, or turns a full Core's newcomer
    /// away.
    void admit(SessionTransport* transport, const QString& name, const QString& shortName,
               const QString& kind);

    /// Completes the session: capability exchange, settings snapshot,
    /// mirror attach, snapshot-complete marker. Never ends another session.
    void promoteToSession(SessionTransport* transport);

    /// Sends the identity-dependent portion of the session state. Returns
    /// false if `transport` is no longer an admitted session after either
    /// send. The initial handshake uses this before the mirror attach; a
    /// later radio identity event uses it on each admitted session without
    /// replaying the mirror snapshot or restarting media.
    bool sendCapabilitiesAndSettingsSnapshot(SessionTransport* transport);

    /// `retryable` rides out on the SessionEnd and tells the client's
    /// reconnect policy whether the condition that produced this drop
    /// clears on its own. Required, not defaulted, so a new refusal cannot
    /// be added without someone deciding which kind it is. See
    /// SessionMessage::retryable; the classification for each call site is
    /// argued at the site.
    /// `endCode` (iPhone app Task 12) is the SessionEndCode the
    /// session.end carries; empty for an end that has none.
    void dropPeer(SessionTransport* transport, const QString& reason,
                  bool sendSessionEnd, bool retryable,
                  const QString& endCode = QString());
    void send(SessionTransport* transport, const SessionMessage& message);
    /// iPhone app Task 71: `message` to every admitted session (each fitted
    /// to what that peer negotiated), or during an attach's burst the
    /// burst's own messages to the attaching session alone.
    // iPhone app Task 72 (ruling 5.8): to every view that holds the object
    // or key. Every other message goes to one session, through send() or
    // sendToPeer().
    void sendToEveryView(const SessionMessage& message);
    QList<QPointer<MirrorView>> attachedViews() const;
    static QString sessionOwner(quint64 sessionId);
    /// One mirror or control message to one admitted peer, fitted to it.
    void sendToPeer(SessionTransport* transport, const SessionMessage& message);
    /// The capability descriptor `transport` is told.
    StationCapabilities buildCapabilitiesFor(SessionTransport* transport) const;
    QString radioAntennaRowRefusal(SessionTransport* transport,
                                   const SessionMessage& invoke) const;
    /// sessionHolder 1 in `transport`'s hello, with deviceAuth 1 (ruling
    /// 10.1: the one without the other is not declared).
    bool peerHoldsSessions(SessionTransport* transport) const;
    /// iPhone app Task 71: sessionHolderVersion 1 reached `transport`
    /// (minor 11 and the feature declared).
    bool peerHasSessionHolderVersion(SessionTransport* transport) const;
    /// R-IOS-13 / R-R3-49: txEqCurveVersion 1 reaches `transport` (minor
    /// 11, a radio model, and txEqCurve 1 declared in its hello).
    bool peerGetsTxEqCurve(SessionTransport* transport) const;
    /// The txEqCurveVersion `transport` is offered: 0 when it does not get
    /// the curve, else its declared txEqCurve up to 2 (2 adds
    /// txEq.setCurve and txEq.resetCurve).
    int txEqCurveVersionFor(SessionTransport* transport) const;
    /// R-R3-49 / R-IOS-18: paProfileVersion 1 reaches `transport` (minor 11,
    /// a Core with its own PA profile bank, and paProfiles 1 declared).
    bool peerGetsPaProfiles(SessionTransport* transport) const;
    /// The refusal a PA profile change from `transport` meets, the one the
    /// settings path gives the desktop's own hardware/<mac>/pa/... writes
    /// (receive-only refusal unless transmit settings were offered; the
    /// station transmit decision with remote transmit allowed); empty when
    /// it may. The radio's on-air rule is RadioModel's.
    QString paProfileRefusal(SessionTransport* transport) const;
    /// Takes transmit's txEqCurve out of a schema, object.create or delta
    /// for a peer that does not get it, so an older app sees today's wire.
    /// False when a delta has nothing left worth sending.
    bool fitTxEqCurveToPeer(SessionTransport* transport, SessionMessage& message) const;
    /// Phone wire batch: `feature` 1 in `transport`'s hello, at minor 11,
    /// on a Core with a radio model: that feature's properties reach it.
    bool peerGetsFeatureProperties(SessionTransport* transport,
                                   const QByteArray& feature) const;
    /// The phone's direct addresses: devices' coreAddresses reach
    /// `transport` only when it declared coreAddresses 1 (as
    /// peerGetsFeatureProperties) and deviceAuth 1, the Core sends the
    /// devices object, and it signed in with a paired device's own key
    /// (peerSeesPairingCode): never a token sign-in.
    bool peerGetsCoreAddresses(SessionTransport* transport) const;
    /// Takes each declared feature's properties (kPeerOnlyProperties) out
    /// of a schema, object.create or delta for a peer that did not declare
    /// the feature, so an older app sees today's wire. False when a delta
    /// has nothing left worth sending.
    bool fitPeerOnlyProperties(SessionTransport* transport, SessionMessage& message) const;
    /// Takes each declared feature's record fields (kPeerOnlyRecordFields)
    /// out of a record batch for a peer that did not declare the feature.
    RecordBatch fitRecordBatchToPeer(SessionTransport* transport, RecordBatch batch) const;
    /// R-R3-46 / R-R3-11: adcAttenuatorVersion 1 reaches `transport`
    /// (minor 11, a Core offering `stepAtt`, adcAttenuators 1 declared).
    bool peerGetsAdcAttenuators(SessionTransport* transport) const;
    /// Takes stepAtt's rx2AttenuationDb and rx2SliceMask out of a schema,
    /// object.create or delta for a peer that does not get them. False when
    /// a delta has nothing left worth sending.
    bool fitAdcAttenuatorsToPeer(SessionTransport* transport, SessionMessage& message) const;
    /// A command, property write or settings write from `transport`'s
    /// device (never a heartbeat).
    void noteActivity(SessionTransport* transport);
    /// Re-arms the grace timer for the next away device's end.
    void scheduleGraceCheck();
    struct HeldQuestion {
        QPointer<SessionTransport> transport;
        QByteArray deviceId;
        QString name;
        QString shortName;
        QString kind;
        quint64 serial = 0;
        qint64 deadlineMs = 0;
    };
    void holdForPlace(SessionTransport* transport, const QByteArray& deviceId,
                      const QString& name, const QString& shortName, const QString& kind);
    void sendHeld(SessionTransport* transport);
    void refreshHeld();
    void drainHeld();
    void handleTakeover(SessionTransport* transport, const SessionMessage& message);
    void finishTakeover(quint64 serial, const QByteArray& targetId,
                        QPointer<SessionTransport> incumbentSession,
                        const QObject* originalSession, bool originalLive, bool released);
    QJsonArray heldCandidates() const;
    bool m_settlingTakeover = false;
    int m_dropPeerDepth = 0;
    quint64 m_activeTakeoverSerial = 0;
    quint64 m_takeoverHolderEpoch = 0;
    bool m_takeoverRequiredUnkey = false;
    QPointer<SessionTransport> m_reservedTransport;
    bool m_slotReserved = false;
    struct DeferredAdmission {
        QPointer<SessionTransport> transport;
        QString name;
        QString shortName;
        QString kind;
    };
    QList<DeferredAdmission> m_deferredAdmissions;
    struct DeferredAuthRequest {
        QPointer<SessionTransport> transport;
        SessionMessage message;
    };
    QList<DeferredAuthRequest> m_deferredAuthRequests;
    quint64 m_nextHeldSerial = 1;
    quint32 m_heldRevision = 1;
    QByteArray m_heldSignature;
    QList<HeldQuestion> m_heldQueue;

    /// iPhone app Task 13 (R-IOS-08): ends every authenticated connection
    /// `matches` picks with session.end, not retryable, `reason` and
    /// `endCode`. The connection whose command is being dispatched right
    /// now is ended just after its result has gone out.
    void endAuthenticatedPeers(const std::function<bool(const Peer&)>& matches,
                               const QString& reason, const char* endCode);
    /// Tells the devices object which paired devices are connected.
    void publishConnectedDevices();

    /// Watches the five singleton mirrored models plus every slice
    /// RadioModel already holds. Idempotent.
    void buildMirror();

    // ── iPhone app Task 73: slice ownership ─────────────────────────────
    /// Ruling 5.2: held slices, saved slices, adoption, a first slice.
    void placeSlicesForAdmission(const QByteArray& deviceId);
    /// Slice control plan Task 8 (approved policy 6): every claim of
    /// `deviceId` goes when its 180 s end (`awayGeneration`, acted on only
    /// while that absence is still the current one), it leaves, a token
    /// window's session ends, it is revoked, or a fifth device takes its
    /// place (nullopt). Each slice it controlled keeps running with no
    /// controller for its other listeners, or closes with nobody left
    /// (saved for a paired device); it leaves every slice it only listened
    /// to, which closes with nobody left. No slice is held for it (Q12).
    /// Its C-Tune pins go.
    void releaseDeviceClaims(const QByteArray& deviceId,
                             std::optional<quint64> awayGeneration);
    /// Fix wave 2: at the end of an away device's 180 s, the slices other
    /// devices took from it (kept by its waiting Take it back notices) are
    /// saved in its DeviceLayoutStore, so its next admission restores them.
    void saveTakenSlicesFor(const QByteArray& deviceId);
    /// Closes slice `sliceId` for a reason other than its owner's own
    /// request, saving it for `saveFor` when set; false (nothing done)
    /// when it is the Core's last slice. Slice control plan Task 8:
    /// `unclaimed` closes only a slice nobody controls or listens to,
    /// whatever the count (RadioModel::closeUnclaimedSlice).
    bool closeSliceFor(int sliceId, const QByteArray& saveFor, SavedSlice* closed = nullptr,
                       bool unclaimed = false);
    /// Fix wave C2 (ruling 5.2, its last paragraph): the paired device a
    /// slice another device's take, pan move or rate change is about to
    /// close must be saved for, because it has left (no registry entry)
    /// and nobody is there to ask or tell; empty for a device holding a
    /// place (its notice keeps the slice), a token window, the station
    /// device, or a slice nobody owns.
    QByteArray saveForAbsentSubject(int sliceId) const;
    /// Fix wave I3: the foreign-slice refusal for a write or removal of a
    /// slice's own settings key (Slice<N>/...) from a session whose device does not own live slice N; empty when
    /// it may, or when the key is not a slice's.
    QString sliceSettingsRefusal(SessionTransport* transport, const QString& key) const;
    /// D79 (R-IOS-11): the refusal for a BandPlanName write naming a plan
    /// this Core does not have; empty otherwise.
    QString bandPlanRefusal(const QString& key, const QVariant& value) const;
    /// D79: after a taken BandPlanName write or removal, the Core's own
    /// BandPlanManager takes the stored plan (ARRL (US) when absent).
    void applyBandPlanSetting(const QString& key);
    static constexpr const char* kBandPlanNameKey = "BandPlanName";
    /// Removes a settings key and applies its default live (the removal's
    /// own effect, after its checks).
    void applySettingsRemove(const SessionMessage& message);
    /// Each attached view's `slice:` and `marker:` forms after an owner
    /// change: object.destroy of the old form, object.create of the new.
    void onSliceOwnerChanged(int sliceId, const QByteArray& oldOwner,
                             const QByteArray& oldHeldFor);
    /// Ruling 5.6: whether `transport`'s view receives `message`'s slice or
    /// marker (any other message: yes).
    bool ownershipAllows(SessionTransport* transport, const SessionMessage& message) const;
    /// Slice control plan Task 4: which forms of a slice with `mark` and
    /// `listeners` (SliceOwnership::listenersOf) `device`'s view has: its
    /// `slice:` while it is joined; its `marker:` to a view with
    /// sessionHolderVersion, for a view that shares slices while it is not
    /// joined, for any other while the slice is not its own (today's rule).
    struct SliceForms {
        bool slice = false;
        bool marker = false;
    };
    SliceForms sliceFormsFor(SessionTransport* transport, const QByteArray& device,
                             const SliceOwnership::Mark& mark,
                             const QList<QByteArray>& listeners) const;
    /// Slice control plan Task 4: `device`'s id as connectedDevices names
    /// it; "station" for the Core's own operating position.
    QString wireIdOf(const QByteArray& device) const;
    /// Slice control plan Task 4: the slice is the transmit slice while a
    /// holder is on the air (or MOX has not yet read off), or the one the
    /// station freeze holds; fix wave (Critical 1): or the slice a transmit
    /// move waits for the unkey gate to land on.
    bool sliceTransmitting(int sliceId) const;
    /// Slice control plan Task 4 (ruling Q7): why control of `sliceId`
    /// cannot pass from `controller` to `taker`, or empty. Core-slice
    /// take-over (JJ's wider ruling): only the Core's own slice with nobody
    /// at its desktop, to a taker below sliceAccessVersion 3; every other
    /// slice passes (a slice on the air is refused by the take itself).
    QString handOffRefusal(const QByteArray& controller, const QByteArray& taker,
                           int sliceId) const;
    /// JJ's wider ruling (2026-09-30): whether `former` stays joined as a
    /// listener of a slice taken from it: the Core's own position, a
    /// session that shares slices, or a device away within its grace.
    /// Any other loses the slice.
    bool staysListeningAfterTake(const QByteArray& former) const;
    /// Slice control plan Task 4 (ruling Q8): `former`'s transmit selection
    /// of `sliceId` cleared before control passes from it (`former` may be
    /// empty for a slice with no controller); fix wave: whoever holds
    /// transmit with the flag on `sliceId` loses that selection, not only
    /// `former`.
    void clearTransmitSelection(const QByteArray& former, int sliceId);
    /// Slice control plan Task 4: closes a slice nobody is on (false: the
    /// Core's last slice stays).
    bool closeSliceNobodyIsOn(int sliceId);
    /// Slice control fix wave (whole-branch review, Critical 1): closes a
    /// slice nobody is on (saving it for `saveFor` when set and
    /// `saveLayout`), or, while it transmits (sliceTransmitting), records
    /// the close for fireDeferredCloses and returns false.
    bool closeUnclaimedOrDefer(int sliceId, const QByteArray& saveFor, bool saveLayout);
    /// Runs each deferred close whose slice no longer transmits, if it is
    /// still the same slice (incarnation) and still nobody is on it; a
    /// slice someone came back to drops its record.
    void fireDeferredCloses();
    /// Queues fireDeferredCloses once on the event loop (MOX, transmit
    /// holder and pending hand-off changes).
    void scheduleDeferredCloses();
    /// Slice control plan Task 4: the former controller is told.
    void tellControlTaken(int sliceId, const QByteArray& former, const QByteArray& taker);
    /// Ruling 5.9, slice control plan Task 2: the plain refusal when
    /// `requester` may not change `sliceId` (SliceAccessPolicy::mayChange:
    /// another device's slice, or one it only listens to), else empty.
    QString changeRefusal(const QByteArray& requester, int sliceId) const;
    /// Slice control plan Task 4: `device`'s live session shares slices
    /// (peerHasSliceAccess).
    bool deviceSharesSlices(const QByteArray& device) const;

    // ── iPhone app plan Task 34: transmit ───────────────────────────────
    SessionPeerInfo peerInfoFor(SessionTransport* transport) const;
    /// Sends `capabilities` again to every session whose txPermitted
    /// changed.
    void publishTxPermitted();
    /// The holder changed (who, keyed, away, a transfer): every session's
    /// txPermitted, connectedDevices, and the model's transmit holder.
    void onTransmitHolderChanged();
    /// Fix wave 2, Important 2: releases the take of epoch `epoch` once it
    /// is clear its key never started (TransmitHolder::releaseUnstartedTake).
    void watchUnstartedTake(quint64 epoch);
    static constexpr int kUnstartedTakeRecheckMs = 50;
    // Task 37: the watchdog follows who is keyed (RadioModel::keyedBy).
    void followKeyedForWatchdog();
    // Task 37: turns VOX off when `deviceId` armed it (its session ended,
    // its link went quiet, its microphone line closed).
    void disarmVoxArmedBy(const QByteArray& deviceId, const char* why);
    // Task 37: the device's name for a stop sentence.
    QString deviceNameForStop(const QByteArray& deviceId) const;
    /// iPhone app plan Task 39: records on `txState` that the Core is
    /// stopping `deviceId`'s key for `stopReason` (TransmitState::kStop*),
    /// when that device holds transmit and is on the air. Returns the stop's
    /// words, or an empty string when that device was not on the air.
    QString noteHolderStopped(const QByteArray& deviceId, const char* stopReason);
    /// Merge of Tasks 37 and 39: records on `txState` why the Core is about
    /// to stop transmitting (the watchdog's linkLost, the starvation's
    /// micStarved), in the words it stops with. Called before StopAllTx.
    void recordTransmitStop(const char* stopReason, const QString& text);
    /// Ruling 7.4 (D60): the on-air refusal for a change from `requester`,
    /// or empty (nobody on the air, or the holder's own change).
    TxRefusal onAirRefusal(const QByteArray& requester) const;
    /// Fix wave 2, Important 1: the holder while it is on the air (any
    /// holder, the station device's own keys included); nullopt otherwise.
    std::optional<TransmitHolder::Holder> onAirHolder() const;
    /// Ruling 7.4's words for `holder` on the air.
    TxRefusal onAirWords(const TransmitHolder::Holder& holder) const;
    /// Fix wave I2 (ruling 8.11, D64): the slice frozen while the station
    /// device is keyed (the radio's own PTT, or the Core's own keys): the
    /// transmit slice, whoever owns it; -1 otherwise.
    int stationFrozenSlice() const;
    /// The refusal for a change to `sliceId` that the freeze stops (its
    /// frequency, mode, filter, band or transmit antenna, or closing it),
    /// or empty.
    TxRefusal stationFreezeRefusal(int sliceId) const;
    /// Fix wave 2, Important 3: the freeze's refusal for a whole message (a
    /// slice property write naming a frozen property, removeSlice or
    /// slice.selectBand), asked on arrival and again when a confirmed,
    /// stored change is applied.
    TxRefusal freezeRefusalFor(const SessionMessage& message) const;
    /// Fix wave 2, Important 3: the refusal for proceeding with `question`
    /// (answer `choice`, from `device`) while a holder is on the air: the
    /// freeze of its stored change or of the slices it moves, or a take of
    /// the transmit slice or its receiver. Empty when it may proceed.
    TxRefusal proceedOnAirRefusal(const ConfirmStep::Question& question, int choice,
                                  const QByteArray& device) const;
    /// The same for a property write to `objectKey`.`property`: a change to
    /// the transmit path (an antenna, PureSignal) while the holder is on
    /// the air.
    TxRefusal onAirPropertyRefusal(const QByteArray& requester, const QByteArray& objectKey,
                                   const QByteArray& property) const;
    /// Slice control plan Task 17: who holds a slice, as a refusal names
    /// it: the device's name, "a phone" / "a tablet" / "a computer" for a
    /// device with no name, "another device" for one the Core does not
    /// know, and "the Core" for nobody or the station device.
    QString sliceHolderWords(const QByteArray& device) const;
    QString ownedElsewhereReason(int sliceId) const;
    /// Slice control plan Task 2: a listener's refusal ("Slice A is
    /// controlled by <name>. Take control to change it."), for a window
    /// that listens; Task 4 sends it.
    QString listenerChangeReason(int sliceId) const;
    /// What `deviceId` owns, for connectedDevices.listeningOn.
    QJsonArray listeningOn(const QByteArray& deviceId) const;
    /// At most the board's maxSlices saved slices per device.
    int deviceLayoutLimit() const;
    bool anotherDeviceHoldsAPlace(const QByteArray& deviceId) const;
    /// A device alone on the Core adopts the slices nobody owns (ruling
    /// 5.2 step 3, applied also to slices the Core makes while it is
    /// there).
    void adoptForLoneDevice();

    // ── iPhone app Task 74 (R-IOS-30): receivers, anchors, the confirm
    //    step and notices (StationReceivers.cpp) ─────────────────────────
    /// The rest of handlePropertyWrite: applies the write as this
    /// session's, answers it (when `answer`), and sends its side effects.
    /// `adjust` may reword the results first. Returns them.
    QList<SessionPropertyResult> applyPropertyWrite(
        SessionTransport* transport, const SessionMessage& message, bool answer,
        const std::function<void(QList<SessionPropertyResult>&)>& adjust);
    /// A command about receivers (the pin, a C-Tune move, adding a slice or
    /// a pan) under the anchor and take rules. True when it answered.
    bool handleReceiverCommand(SessionTransport* transport, const SessionMessage& message);
    /// A slice retune that leaves its shared receiver (ruling 6.5). True
    /// when it answered.
    bool handleSliceRetune(SessionTransport* transport, const SessionMessage& message);
    /// confirm.proceed, confirm.cancel, notice.takeBack.
    SessionMessage answerConfirm(const SessionMessage& invoke, int id, int choice);
    /// "<n> of your slices could not be restored: all the radio's receivers
    /// are in use." (ruling 5.2 step 2), for slicesNotRestored and a
    /// partial graceEnded.
    static QString notRestoredSentence(qsizetype count);
    /// Fix wave I2: the slices `question` names (the written slice, a
    /// `sliceId` argument, the slices in `moving`).
    QList<int> slicesNamedBy(const ConfirmStep::Question& question) const;
    /// Fix wave I2: a slice a question names closed or changed owner; the
    /// question can no longer be proceeded.
    void dropQuestionsNaming(int sliceId);
    /// Fix wave I2: the refusal for a question whose slices are no longer
    /// the requester's: "changed since you asked" for a shared setting,
    /// "what this change reaches has changed" for any other kind.
    static QString changedSinceAskedReason(const QString& kind);
    /// The shared setting's "changed since you asked" words.
    static QString sharedTargetChangedReason();
    /// Ruling 10.2's refusal for an older window left with no slice at
    /// admission; empty when it has one or is not an older window.
    QString olderWindowWithoutSliceReason(SessionTransport* transport) const;
    /// graceEnded or slicesNotRestored, then the notices that waited.
    void deliverAdmissionNotices(SessionTransport* transport,
                                 std::optional<qint64> timeRanOutAtMs);
    struct PanMoveCheck {
        /// None: not a pan move (today's path); Apply: nobody would be
        /// asked; Ask: another device's slice moves or closes; OnAir: it
        /// would move or close the transmit slice of a holder on the air
        /// (ruling 7.4, and ruling 8.11's freeze), refused with `onAir`.
        enum class Kind { None, Apply, Ask, OnAir };
        Kind kind = Kind::None;
        TxRefusal onAir;
        int stream = -1;
        double centreHz = 0.0;
        int exemptSliceId = -1;
        ReceiverPlanner::WindowMove plan;
        /// The disturbed slices of devices (a slice nobody owns moves or
        /// closes without asking anyone).
        QList<ReceiverPlanner::Disturbed> named;
    };
    PanMoveCheck checkPanMove(const QByteArray& requester, const SessionMessage& original) const;
    ReceiverPlanner::DeviceInfo planDevice(const QByteArray& deviceId) const;
    ReceiverPlanner receiverPlanner() const;
    /// Slice control plan Task 9: receiverPlanner() naming each slice's
    /// listeners when `transport` shares slices.
    ReceiverPlanner questionPlanner(SessionTransport* transport) const;
    SessionTransport* liveTransportFor(const QByteArray& deviceId) const;
    QString withHolderNames(const QString& reason, const QByteArray& requester) const;
    /// Slice control plan Task 9: the usableSlices result value, a JSON
    /// array of {sliceId, incarnation, letter, controllerDeviceId} for every
    /// live slice in id order ("" for a slice with no controller the link
    /// can name).
    QString usableSlicesJson() const;
    QString namesOf(const QList<ReceiverPlanner::Disturbed>& disturbed) const;
    void answerHere(SessionTransport* transport, const SessionMessage& result);
    void answerWrite(SessionTransport* transport, const SessionMessage& write,
                     const QString& reason);
    bool handleCentreMove(SessionTransport* transport, const SessionMessage& message,
                          const QByteArray& requester);
    bool handleAddWithTake(SessionTransport* transport, const SessionMessage& message,
                           const QByteArray& requester);
    void refuseWhileAsking(SessionTransport* transport, const SessionMessage& original);
    void sendQuestion(SessionTransport* transport, ConfirmStep::Question question,
                      SessionPrompt prompt);
    void askPanMove(SessionTransport* transport, const SessionMessage& original,
                    const PanMoveCheck& check, const std::optional<QJsonObject>& change);
    bool askTake(SessionTransport* transport, const SessionMessage& original,
                 const ReceiverPlanner::TakeRequest& request);
    bool askTakeSlice(SessionTransport* transport, const SessionMessage& original,
                      const QList<ReceiverPlanner::Choice>& choices);
    void applyPanMove(const PanMoveCheck& check, const QByteArray& requester);
    SessionMessage runHeldCommand(const SessionMessage& original);
    SessionMessage applyHeld(SessionTransport* transport, const ConfirmStep::Question& question,
                             int stream, const SessionMessage& invoke);
    bool heldFitsNow(const ConfirmStep::Question& question) const;
    /// Slice control plan Task 9: `listenedBy`, when given, gets each
    /// closed slice under every other device that was listening to it.
    QHash<QByteArray, QList<SavedSlice>> closeForTake(
        const QList<int>& sliceIds, QHash<QByteArray, QList<SavedSlice>>* listenedBy = nullptr);
    /// Slice control plan Task 9: a sliceClosed notice to each listener of
    /// a slice `by` closed, never to `by`. `why` is receiverTaken,
    /// sliceTaken or panMove.
    void tellListeners(const QHash<QByteArray, QList<SavedSlice>>& listenedBy,
                       const QByteArray& by, const QString& why);
    void tellTaken(const QHash<QByteArray, QList<SavedSlice>>& closedBy, const QByteArray& taker,
                   const QString& kind, int stream, int takerSlice);
    void endOlderWindowsWithoutSlices(const QList<QByteArray>& devices, const QByteArray& taker);
    void tellDevice(ConfirmStep::Notice notice, const QByteArray& by);
    void sendNotice(SessionTransport* transport, const ConfirmStep::Notice& notice);
    SessionMessage askAgain(const SessionMessage& invoke);
    SessionMessage proceedPanMove(SessionTransport* transport, const ConfirmStep::Question& question,
                                  const SessionMessage& invoke);
    SessionMessage proceedTakeReceiver(SessionTransport* transport,
                                       const ConfirmStep::Question& question, int choice,
                                       const SessionMessage& invoke);
    SessionMessage proceedTakeSlice(SessionTransport* transport,
                                    const ConfirmStep::Question& question, int choice,
                                    const SessionMessage& invoke);
    SessionMessage askTakeBack(SessionTransport* transport, const SessionMessage& invoke,
                               int noticeId);
    SessionMessage proceedTakeBack(SessionTransport* transport,
                                   const ConfirmStep::Question& question, int choice,
                                   const SessionMessage& invoke);
    void sendHeldQuestions();

    // ── iPhone app plan Task 77 (R-IOS-02, R-IOS-03): taking transmit
    //    (StationTransmitTake.cpp) ───────────────────────────────────────
    /// tx.take from `transport`'s device (rulings 8.4, 8.7): `reply` runs
    /// once, now or when the transfer ends.
    void takeTransmit(SessionTransport* transport, const SessionMessage& invoke,
                      std::optional<quint64> shownEpoch, std::optional<bool> shownKeyed,
                      std::function<void(const SessionMessage& result)> reply);
    /// The session's own transmit gate (remote_transmit, its hello, its
    /// pairing, its snapshot), without the holder's rule; empty when it
    /// passes.
    TxRefusal sessionTransmitRefusal(SessionTransport* transport) const;
    /// The takeTransmit question's `holder` entry.
    QJsonObject holderEntryJson(const TransmitHolder::Holder& holder) const;
    /// Asks `transport`'s operator whether to take transmit from the holder
    /// (confirm.request takeTransmit).
    void askTakeTransmit(SessionTransport* transport, const SessionMessage& original,
                         ConfirmStep::Held held, qint64 noticeId);
    /// The take (ruling 8.2's transfer to `taker`), the old holder told
    /// (transmitTaken, with Take it back) and, when it was on the air, the
    /// stop recorded as takenOver. `done(assigned)` when it ends.
    void runTake(const QByteArray& taker, TransmitHolder::Source source,
                 std::function<void(bool assigned)> done);
    /// Fix wave I2: a taker whose session ended while its take ran is
    /// never left holding as if it were here. Paired and away: held for it,
    /// away (holderDropped, its 180 s running); gone (it left, or a token
    /// window): released. A taker with a live session is left as it is.
    void settleTakerWithoutSession(const QByteArray& taker);
    /// Fix wave M6: a tx.take and its copies (the same command id on the
    /// same session): the first is run; a copy while it runs is answered by
    /// its one result, and a later copy gets the same answer. Never a second
    /// take.
    struct TakeCopy {
        quint32 commandId = 0;
        bool answered = false;
        SessionMessage result;
    };
    /// Per session id, the most recent tx.take ids (at most kTakeCopiesKept).
    QHash<quint64, QList<std::shared_ptr<TakeCopy>>> m_takeCopies;
    static constexpr int kTakeCopiesKept = 16;
    /// Runs the take for a confirm.proceed or notice.takeBack being
    /// answered now: its result when the transfer ends at once, otherwise
    /// the answer goes later (m_proceedAnsweredLater) and the returned
    /// message is dropped. `onTaken` runs when the take is assigned.
    SessionMessage takeAnswering(SessionTransport* transport, const SessionMessage& invoke,
                                 std::function<void()> onTaken = {});
    SessionMessage proceedTakeTransmit(SessionTransport* transport,
                                       const ConfirmStep::Question& question,
                                       const SessionMessage& invoke);
    /// notice.takeBack for a transmitTaken notice: tx.take {} with its
    /// usual confirmation (section 8.6).
    SessionMessage takeBackTransmit(SessionTransport* transport, const SessionMessage& invoke,
                                    qint64 noticeId);
    /// Take-over parity: notice.takeBack for a controlTaken notice:
    /// SliceAccessController::takeControl with the slice, incarnation and
    /// control revision the notice named, under the same checks as
    /// slice.takeControl. Transmit does not move with it (ruling Q8).
    SessionMessage takeBackControl(SessionTransport* transport, const SessionMessage& invoke,
                                   const ConfirmStep::Notice& record);
    /// The words of the station device as a taker or holder: "Radio" after
    /// the radio's PTT, otherwise a hosting desktop's own name.
    TransmitHolder::Words stationTakerWords(TransmitHolder::Source source) const;
    /// Ruling 5.4a: each slice's TX mark on the link, true only while its
    /// owner holds transmit (SliceModel::setTxMarkAllowed).
    void refreshTxMarks();
    /// Ruling 8.10: when a transfer assigned a new holder, the transmit
    /// slice goes to its chosen transmit slice, else its active slice (not
    /// for the radio's own PTT, which transmits where the flag is).
    void bindTransmitSliceForHolder();
    /// Slice control plan Task 11: the slice `device`'s binding prefers
    /// when it holds transmit: its remembered choice, unless that is a
    /// slice it took and has not chosen, then its active slice, then its
    /// first other slice it may transmit on. -1 lets the arbiter pick.
    int transmitPreferenceFor(const QByteArray& device) const;
    /// Slice control plan Task 11 (ruling Q8, narrow reading): the refusal
    /// for a key from `device` that would land on a slice it took control
    /// of from another device and has not chosen, when it has no other
    /// slice it may transmit on; empty otherwise. A holder bound on such a
    /// slice with another of its own is moved there first (unkeyed only).
    TxRefusal takenSliceKeyRefusal(const QByteArray& device);
    /// Take-over fix wave (I-2, ruling Q8) and re-review (N-1): the
    /// refusal for a key from `device` that would land on another device's
    /// slice, when `device` shares slices (keyerSharesSlices) or the slice
    /// is the one it lost (m_lostTxSlice): noTransmitSlice when it has no
    /// slice it may transmit on. With one, nothing is refused and
    /// `moveTo` names it; the gate moves the flag there once askKey admits
    /// the key (N-2). A slice nobody owns is left as it was. The radio's
    /// own PTT is asked radioPttKeyRefusal instead (ruling 8.11).
    TxRefusal othersSliceKeyRefusal(const QByteArray& device, int* moveTo);
    /// TX rulings (ruling 8.11 for a hosting desktop): the refusal for a
    /// key from the radio's own PTT (RadioPtt) on a Core with a hosting
    /// desktop, while the flag is on another device's slice. Nothing is
    /// refused, and the flag stays, when it is on one of the desktop's own
    /// slices (a non-active one included: split transmit, JJ 2026-09-30,
    /// keys that chosen slice), on one nobody controls, or on the desktop's
    /// active slice, and on a Core with no desktop. Otherwise `moveTo` names the desktop's active slice
    /// (its receive focus, wherever it is) and the gate moves the flag
    /// there once askKey admits the key; noTransmitSlice when the desktop
    /// has no slice, radioOnAir while the radio is not back in receive or
    /// the flag is frozen.
    TxRefusal radioPttKeyRefusal(int* moveTo);
    /// Take-over re-review (N-1): the hosting desktop once it takes its
    /// notices, or a device that declared sliceAccess.
    bool keyerSharesSlices(const QByteArray& device) const;
    /// Ruling 8.12: the holder's last slice closed: transmit is released.
    void onSliceClosedForHolder(int sliceId);
    /// Each device's chosen transmit slice (ruling 8.10), by device.
    QHash<QByteArray, int> m_chosenTxSlice;
    /// Slice control fix wave (Important 4): each device's explicit
    /// transmit choice (explicitTxSliceFor).
    QHash<QByteArray, int> m_explicitTxSlice;
    /// Slice control fix wave (whole-branch review, Critical 1): closes of
    /// a slice nobody is on, waiting for it to stop transmitting, by id.
    struct DeferredClose {
        quint64 incarnation = 0;
        QByteArray saveFor;
        bool saveLayout = false;
    };
    QHash<int, DeferredClose> m_deferredCloses;
    bool m_deferredClosesQueued = false;
    /// The holder epoch the transmit slice was last bound for.
    quint64 m_txSliceBoundEpoch = 0;
    /// Section 7.3's refusal for an older window, naming who a change
    /// would affect.
    static QString olderWindowReason(const QString& names);

    // ── iPhone app Task 75 (R-IOS-30): settings that affect every device
    //    (StationSharedSettings.cpp) ────────────────────────────────────
    /// A change on the several-devices design's list (7.1): what it
    /// touches, its words, and what it acts on (ruling 7.6).
    /// JJ's ruling of 2026-09-28 (the several-devices design, ruling
    /// 7.1a): what a listed change is, as design table 7.1's rows name it.
    /// kSharedTiers (StationSharedSettings.cpp) is the one table that says
    /// which rows ask first and which apply at once and tell.
    enum class SharedCategory : quint8 {
        SampleRate,
        Radio,
        ReceiveAntenna,
        TransmitAntenna,
        PureSignal,
        Diversity,
        FourO3A,
        Amplifier,
        Tuner,
        Interlock,
        Transmitter,
        Attenuator,
        NoiseBlanker,
        Notches,
        ReceiveOptions,
        FilterPolicy,
    };
    /// Ask: the change waits for its requester's confirm while a connected
    /// device is disturbed. Notify: it applies at once and each disturbed
    /// device is told.
    enum class SharedTier : quint8 { Ask, Notify };
    struct SharedChange {
        /// On the list and a real change (not the value already there).
        bool shared = false;
        /// Ruling 7.1a: every table-7.1 row the message touches, one bit
        /// per SharedCategory. None marked reads as Ask.
        quint32 categories = 0;
        DisturbanceCheck::Scope scope;
        /// {label, from, to} in plain words.
        QJsonObject change;
        /// What it acts on and that target's value now; set whenever the
        /// message names a listed target, shared or not.
        QString target;
        QString targetValue;
        /// A sample rate the Core's own plan refuses for the requester's
        /// own slice: left to today's path, which refuses it.
        bool refusedToday = false;
        /// A sample rate: other devices' slices it closes before applying.
        QList<int> closes;
    };
    SharedChange classifyShared(const SessionMessage& message, const QByteArray& requester) const;
    /// Ruling 7.1a: Notify when every row the change touches is a Notify
    /// row in kSharedTiers, Ask otherwise.
    static SharedTier sharedTierOf(const SharedChange& change);
    /// Ruling 7.1a: the disturbed devices that are connected now (not
    /// away); only these are asked about.
    QList<DisturbanceCheck::Affected> connectedAffected(
        const QList<DisturbanceCheck::Affected>& affected) const;
    /// What each disturbed slice is now, for the notices (a closing slice
    /// is gone once applied).
    QHash<int, QJsonObject> sharedSliceWords(
        const QList<DisturbanceCheck::Affected>& affected) const;
    /// Ruling 7.1a: a change that applies at once (a Notify row, or no
    /// connected device disturbed) with devices to tell. A command is run
    /// here and told on its result; a property or settings write is left
    /// to today's path, and tellAppliedNow() tells once it has applied.
    bool applySharedNow(SessionTransport* transport, const SessionMessage& message,
                        const SharedChange& change,
                        const QList<DisturbanceCheck::Affected>& affected);
    /// The write held by applySharedNow(): its devices are told when
    /// `applied`, and it goes either way.
    void tellAppliedNow(bool applied);
    DisturbanceCheck::Topology sharedTopology() const;
    /// Who holds transmit, for the check. Empty until Task 34's
    /// TransmitHolder joins here.
    DisturbanceCheck::Transmit transmitForCheck() const;
    /// A command, property write or settings write on the list: asked
    /// (a device with the feature), refused (an older window), or left to
    /// today's path. True when it answered.
    bool handleSharedSetting(SessionTransport* transport, const SessionMessage& message);
    QJsonArray sharedAffectedJson(const QList<DisturbanceCheck::Affected>& affected) const;
    static QSet<QString> sharedShown(const QList<DisturbanceCheck::Affected>& affected);
    void askSharedSetting(SessionTransport* transport, const SessionMessage& original,
                          const SharedChange& change,
                          const QList<DisturbanceCheck::Affected>& affected,
                          bool answerOriginal);
    SessionMessage proceedSharedSetting(SessionTransport* transport,
                                        const ConfirmStep::Question& question,
                                        const SessionMessage& invoke);
    void tellSettingChanged(const QList<DisturbanceCheck::Affected>& affected,
                            const QHash<int, QJsonObject>& sliceWords,
                            const QJsonObject& change, const QByteArray& by);
    /// A proceed whose held command answers on a later turn
    /// (requestSliceSampleRate, the dispatcher's one asynchronous verb):
    /// its answer, the readback and the notices wait for that result.
    /// Fix wave I1: a command result is named by the session that asked
    /// with its verb and id, since every client counts its ids from 1.
    struct ResultKey {
        quint64 sessionId = 0;
        QByteArray verb;
        quint32 commandId = 0;
        friend bool operator==(const ResultKey& a, const ResultKey& b)
        {
            return a.sessionId == b.sessionId && a.commandId == b.commandId && a.verb == b.verb;
        }
        friend size_t qHash(const ResultKey& key, size_t seed = 0) noexcept
        {
            return qHashMulti(seed, key.sessionId, key.verb, key.commandId);
        }
    };
    /// The session id an owner string `station:<sessionId>` names; 0 for
    /// any other.
    static quint64 sessionIdOfOwner(const QString& owner);
    /// The key of `result` for the session the dispatcher says it answers.
    ResultKey resultKeyOf(const SessionMessage& result) const;
    /// Whether `result` is the last its command sends (its route goes).
    static bool isLastResult(const SessionMessage& result);
    struct DeferredProceed {
        QPointer<SessionTransport> transport;
        QByteArray proceedVerb;
        quint32 proceedId = 0;
        QList<DisturbanceCheck::Affected> affected;
        QHash<int, QJsonObject> sliceWords;
        QJsonObject change;
        QByteArray requester;
        QList<QByteArray> closedDevices;
        /// Ruling 7.1a: false for a change applied at once, whose own
        /// result goes to its requester as today; only the notices wait.
        bool answersProceed = true;
    };
    QHash<ResultKey, DeferredProceed> m_deferredProceeds;
    /// Ruling 7.1a: a property or settings write applied at once, whose
    /// devices are told by tellAppliedNow().
    std::optional<DeferredProceed> m_appliedNow;
    /// Ruling 7.1a: a radio change applied at once, told when its held
    /// answer is (finishRadioChange).
    std::optional<DeferredProceed> m_appliedNowRadio;
    /// Follow-up N3: a radio change's answer (and, after the confirm step,
    /// its notices), held from holdRadioChangeAnswers to finishRadioChange.
    struct HeldRadioChange {
        ResultKey key;
        SessionMessage result;
        bool proceed = false;  // a confirm.proceed answer, with `later`
        DeferredProceed later;
        /// Ruling 7.1a: a change applied at once; `later` holds its
        /// notices.
        bool tellOnFinish = false;
    };
    bool m_holdingRadioChange = false;
    std::optional<HeldRadioChange> m_heldRadioChange;
    /// The proceed answered later, whose immediate answer is not sent.
    std::optional<ResultKey> m_proceedAnsweredLater;
    /// True when `result`, keyed `key`, finished a deferred proceed (and
    /// was consumed).
    bool finishDeferredProceed(const ResultKey& key, const SessionMessage& result);
    /// Ruling 5.11a: RadioModel kept the receive antenna; the person tuning
    /// is told.
    void onReceiveAntennaKept(int sliceId, const QString& antenna,
                              const QList<QByteArray>& listeners);

    std::unique_ptr<ConfirmStep> m_confirm;
    bool m_holdQuestions = false;
    QList<QPair<SessionTransport*, SessionMessage>> m_heldQuestions;
    /// While set, every result the dispatcher emits passes through it
    /// first; false keeps it from being sent.
    std::function<bool(SessionMessage&)> m_resultHook;

    QPointer<RadioModel> m_radioModel;
    AppSettings& m_settings;
    QString m_securityDirectory;
    QString m_lastError;

    /// iPhone app Task 4: what this station's hello advertises. Task 12
    /// declares deviceAuth 1 (when the identity key is usable); the tasks
    /// that add pairing, the takeover question and Setup descriptions add
    /// theirs here.
    QList<quint16> m_supportedMajors;
    QHash<QByteArray, int> m_declaredFeatures;

    std::unique_ptr<CertificateStore> m_certificates;
    std::unique_ptr<TokenStore> m_tokens;
    // iPhone app Task 12 (R-IOS-08): the Core's identity key, its paired
    // devices, device sign-in, the certificate's SHA-256 (what a device
    // signs) and the identity's signature over it (sent in every hello).
    std::unique_ptr<StationIdentity> m_identity;
    std::unique_ptr<DeviceStore> m_devices;
    std::unique_ptr<DeviceAuthenticator> m_deviceAuth;
    // iPhone app Task 13: after the three it reads, so it goes first.
    std::unique_ptr<StationDevicesFacade> m_devicesFacade;
    // The phone's direct addresses: into m_devicesFacade's coreAddresses.
    std::unique_ptr<CoreAddressWatcher> m_coreAddresses;
    // iPhone app Task 19: the Core's catalogue.
    std::unique_ptr<StationCatalog> m_catalog;
    // R-R3-49 / R-IOS-18: the Core's PA Gain profiles (`paProfiles`).
    std::unique_ptr<PaProfilesFacade> m_paProfiles;
    std::unique_ptr<SetupDescriptionService> m_setupDescription;
    // Parity Task 19 (R-IOS-25): the record streams by name, the id of the
    // last console line, and the send that follows a change.
    std::map<QString, std::unique_ptr<RecordStream>> m_recordStreams;
    quint64 m_consoleLineId = 0;
    QTimer* m_recordFlushTimer = nullptr;
    // Parity Task 22: the log sink's last line put in `coreLog`, the pull
    // that runs while it has a subscriber, the newest telemetry this Core
    // measured (for the support bundle) and nereusd's configuration file.
    quint64 m_coreLogSequence = 0;
    bool m_coreLogInPrivateKey = false;
    QTimer* m_coreLogTimer = nullptr;
    QJsonObject m_lastTelemetry;
    qint64 m_lastTelemetryAtMs = 0;
    QString m_supportConfigPath;
    // Parity Task 33: the txCfcCompression stream's reader and its timer.
    QTimer* m_cfcPollTimer = nullptr;
    std::function<bool(double*, int)> m_cfcDisplayReader;
    // iPhone app plan Task 25: the `vax` object and the vaxLevels stream's
    // reader, its timer and its last record.
    StationVax* m_stationVax = nullptr;
    QTimer* m_vaxLevelsTimer = nullptr;
    std::function<void(double*, double*)> m_vaxLevelReader;
    QJsonObject m_lastVaxLevels;
    // R-IOS-13 / R-R3-49: the Core's side of the Mod Monitor streams.
    std::unique_ptr<ModMonitorPublisher> m_modMonitor;
    // Parity Task 21: the Core's radios and their stream.
    QPointer<StationRadios> m_stationRadios;
    void publishStationRadios();
    // Parity Task 23: the apps on the Core's station TCI server.
    void publishStationTciClients();
    // iPhone app Task 71: who holds a place on the Core.
    std::unique_ptr<DeviceSessionRegistry> m_deviceSessions;
    // The connection whose command.invoke is being dispatched, and the end
    // it is owed once its result has been sent (a self-revoke, or a token
    // session retiring the token).
    SessionTransport* m_dispatchingTransport = nullptr;
    std::optional<std::pair<QString, QString>> m_pendingEnd;
    QByteArray m_certSha256;
    QByteArray m_certBinding;
    bool m_pairingLanClickAllowed = true;
    // iPhone app Task 14: the pairing window, the stored data for its
    // current code (wiped when the code changes), and the console.
    std::unique_ptr<PairingWindow> m_pairingWindow;
    QByteArray m_pairingStored;
    quint64 m_pairingStoredSerial = 0;
    // The hash worker (one at a time), the code serial it hashes, and the
    // hash itself (SpakeExchange::storedData; a test may hold it).
    std::unique_ptr<QThread> m_pairingHashThread;
    quint64 m_pairingHashSerial = 0;
    std::function<QByteArray(const QString&)> m_pairingHasher;

    QWebSocketServer* m_wsServer = nullptr;
    std::unique_ptr<TxWatchServer> m_txWatchServer;
    QHash<SessionTransport*, std::shared_ptr<PendingRelayWatch>> m_pendingRelayWatches;
    quint64 m_nextTxWatchGeneration = 0;
    StationOpeningGate* m_openingGate = nullptr;
    int m_openingDeadlineMs = kDefaultOpeningDeadlineMs;
    int m_maxOpenings = kMaxUnfinishedOpenings;
    int m_maxOpeningsPerAddress = kMaxHandshakesPerAddress;

    StateMirror* m_mirror = nullptr;
    ObjectRegistry* m_registry = nullptr;
    SessionCommandDispatcher* m_dispatcher = nullptr;
    SettingsProxyServer* m_settingsServer = nullptr;
    bool m_mirrorBuilt = false;

    QHash<SessionTransport*, Peer> m_peers;
    /// Slice control plan Task 10: the station device as a peer for the
    /// hosting desktop's requests. Never in m_peers, so every loop over
    /// the peers skips it; peerFor() and peerPtr() find it by its sentinel
    /// transport, and send() hands what reaches that transport to the
    /// callbacks below.
    std::unique_ptr<SessionTransport> m_stationTransport;
    Peer m_stationPeer;
    QHash<ResultKey, StationAnswer> m_stationAnswers;
    StationAnswer m_stationQuestion;
    StationAnswer m_stationNotice;
    /// The peer for `transport` (a copy, empty when none): the station
    /// peer for its sentinel, else m_peers' entry.
    Peer peerFor(SessionTransport* transport) const;
    /// The same by pointer, through peerKey(), or nullptr.
    const Peer* peerPtr(SessionTransport* transport) const;
    bool hasPeer(SessionTransport* transport) const;
    bool isStationTransport(SessionTransport* transport) const
    {
        return transport != nullptr && transport == m_stationTransport.get();
    }
    void deliverToStation(const SessionMessage& message);
    /// The command.invoke dispatch every admitted peer's request and the
    /// station device's share: the owner, requester and sharing set for
    /// it, the refusals, the confirm step, the dispatcher, and a result
    /// still owed routed back to it.
    void runInvoke(SessionTransport* transport, const SessionMessage& message);
    struct SettingsExportJob {
        SettingsBackupTransferSource source;
        QByteArray transferId;
        qint64 nextOffset = 0;
        qint64 bornMs = 0;
        qint64 lastReadMs = 0;
    };
    // Session ID, never device ID: a replacement connection cannot inherit bytes.
    QHash<quint64, SettingsExportJob> m_settingsExports;
    QElapsedTimer m_settingsExportClock;
    QTimer* m_settingsExportCleanup = nullptr;
    std::function<qint64()> m_settingsExportNowForTest;
    // Control logging lane: the Core's log of each device's control
    // traffic. Logging only.
    ControlLog m_controlLog;
    /// iPhone app Task 76: the admitted session with this media epoch, or
    /// null; the earliest admitted of those live (the primary).
    SessionTransport* mediaSessionFor(quint64 epoch) const;
    SessionTransport* primaryMediaSession() const;
    bool mediaAvailableFor(SessionTransport* transport) const;
    /// The Core's total as a peer sees it (none for an older peer when it
    /// is computed, setDisplayBudgetForReasonPeersOnly).
    std::optional<DisplayBudgetLimits> displayBudgetTotalFor(SessionTransport* transport) const;
    /// Splits the total among the media sessions (DisplayBudgetSplit) into
    /// each Peer's share; true when any share or reason changed.
    bool recomputeDisplayBudgetShares();
    /// The split itself: each sharing session with its share. With
    /// `ps3Subscriber` the PureSignal display is charged to that session
    /// whether or not it is subscribed now.
    QList<QPair<SessionTransport*, DisplayBudgetShare>> splitDisplayBudget(
        std::optional<quint64> ps3Subscriber,
        std::optional<QPair<quint64, quint64>> additionalDemand = std::nullopt) const;
    /// What a session's capabilities say of the budget, to publish a
    /// change once.
    QByteArray budgetEntriesFor(SessionTransport* transport) const;
    void publishBudgetToChangedSessions();
    quint64 m_ps3SubscriberEpoch = 0;
    /// close() is ending every session.
    bool m_closing = false;
    SessionPs3DisplayAdmissionHandler m_ps3DisplayAdmission;
    DisplayDemandProvider m_displayDemand;
    /// During promoteToSession()'s attach: the session its burst is for.
    quint64 m_nextSessionId = 0;
    /// Command results owed to a session other than the one being
    /// dispatched now (a result that arrives on a later turn), by the
    /// session, verb and id (fix wave I1). A route is erased once its last
    /// result is delivered (a PureSignal action's completed or failed
    /// phase; any other command's one result), or when its session ends.
    QHash<ResultKey, QPointer<SessionTransport>> m_resultRoutes;
    bool m_resultSentInDispatch = false;
    std::unique_ptr<ConnectedDevicesFacade> m_connectedDevices;
    // iPhone app plan Task 34: who holds transmit, and who may transmit.
    std::unique_ptr<TransmitHolder> m_transmitHolder;
    /// setStationDeviceWords.
    struct StationWords {
        QString name;
        QString shortName;
    } m_stationWords;
    StationTxGate m_txGate;
    // iPhone app plan Task 35: keying from a remote device (a Local model
    // with a MoxController only).
    std::unique_ptr<RemoteKeying> m_remoteKeying;
    // iPhone app plan Task 37: the transmit watchdog, its one check timer
    // (a child, so the conformance player's virtual clock drives it), the
    // per-mode starvation action, the device whose key the watchdog
    // follows now, and the device that armed VOX.
    std::unique_ptr<RemoteTxWatchdog> m_txWatchdog;
    QTimer* m_txWatchdogTimer = nullptr;
    StarvationPolicy m_starvation;
    QByteArray m_watchedKeyedDevice;
    QByteArray m_voxArmedBy;
    // iPhone app plan Task 39: the `txState` object. Qt-parented to this, so
    // its meter timer is one of the server's timers (the conformance
    // runner's virtual clock drives them all).
    TransmitState* m_transmitState = nullptr;
    // iPhone app Task 73: one marker per slice (Qt-parented to this).
    SliceMarkerSet* m_markers = nullptr;
    // Slice control plan Task 4: one SliceAccess object per slice, and the
    // verbs' checks (both Qt-parented to this).
    SliceAccessSet* m_sliceAccessSet = nullptr;
    SliceAccessController* m_sliceAccessController = nullptr;
    // Slice control plan Task 4: each slice's mark and listeners as every
    // view's forms of it were last decided, so a join, a leave or a change
    // of controller sends each view only the difference.
    struct SliceFormState {
        SliceOwnership::Mark mark;
        QList<QByteArray> listeners;
    };
    QHash<int, SliceFormState> m_sliceForms;
    // Slice control plan Task 4 (ruling Q8): slices each device took
    // control of and has not chosen to transmit on since; its transmit
    // binding never picks one up by itself.
    QHash<QByteArray, QSet<int>> m_takenNotChosenForTx;
    // Take-over fix wave (I-2, ruling Q8): the slice the transmit flag was
    // on when control of it passed from each device. A key from that device
    // never lands there while another device controls it.
    QHash<QByteArray, QSet<int>> m_lostTxSlice;
    // True while a restored layout's owners are settled just before every
    // view's burst is sent again (receiveLayoutHydrated).
    bool m_ownerChangesInBurst = false;
    QHash<QByteArray, QList<SavedSlice>> m_slicesNotRestored;
    QTimer* m_graceTimer = nullptr;
    bool m_mediaEnabled = false;
    bool m_displayBudgetEnforcementEnabled = false;
    std::optional<DisplayBudgetLimits> m_displayBudget;
    bool m_displayBudgetForReasonPeersOnly = false;
    DisplayBudgetReason m_displayBudgetReason = DisplayBudgetReason::None;
    bool m_telemetryEnabled = false;
    quint64 m_mediaSessionEpoch = 0;

    QTimer* m_heartbeatTimer = nullptr;
    QTimer* m_deltaFlushTimer = nullptr;

    int m_authDeadlineMs = kDefaultAuthDeadlineMs;
    // iPhone app plan Task 29.
    bool m_relayAllowed = true;
    // The direct media ladder: the rendezvous hello's STUN servers.
    QStringList m_mediaStunUrls;
    HostFamilies m_mediaStunFamilies;
    int m_pathTicketLifetimeMs = kPathTicketLifetimeMs;
    int m_pathSwitchDeadlineMs = 0;
    int m_sessionsMoved = 0;
    int m_heartbeatIntervalMs = kDefaultHeartbeatIntervalMs;
    bool m_tokenSessionsMayChangeRadioForTest = false;
    bool m_tokenSessionsMayTransmitForTest = false;
    int m_maxMissedPongs = kDefaultMaxMissedPongs;
    int m_sustainableSliceLimit = 0;
};

} // namespace NereusSDR
