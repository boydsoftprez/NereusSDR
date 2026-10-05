// 2026-09-27: validate transmit-region writes and shared confirmations.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// Modification history (NereusSDR):
//   2026-10-04: Resume retained automatic PureSignal intent after the
//               first successful media admission, preserving retirement.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-09-30: Fix round 1 for LINK-I4: the pairing window is built
//               with the Core's settings, so a restart keeps service
//               pairing shut. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-30: Fix wave LINK-I4: pairing through the remote access
//               service is refused, with no time to try again, once the
//               pairing window has shut it after too many wrong codes.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: Fix wave LINK minor 2 (TX path): dropPeer stops a dropped
//               device's transmit (VOX disarm, watchdog or stopAllTx)
//               before anything else in the drop. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-30: Fix wave LINK minor 5: a token check counts failures per
//               source address. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-30: RADE reason: radeReasonVersion 1 and each slice's
//               radeReason only to a peer that declared radeReason 1
//               (fitPeerOnlyProperties; before coreBuildInfo). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: Station VOX (whole-branch review, TX path): the keying
//               gate refuses a station VOX key while a device holds
//               transmit that did not arm VOX (VOX armed at the Core keys
//               only for the station device). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-30: TX badge take fix round 1: peerInfoFor() counts a token
//               session as paired under setTokenSessionsMayTransmitForTest()
//               (tests only). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-30: Shared-input filters (ruling (d)): radio's
//               rxFilter0LowPassReason and rxFilter0LowPassSlice go to a
//               peer that declared rxFilterLowPass 1, which is sent
//               rxFilterLowPassVersion 1 (before coreBuildInfo). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: Radio codec lane: radioHardwareVersion 13, the receive
//               audio to the Core's radio and HL2 Swap audio channels from
//               a window. Setup description version 24 (Audio > TX Input's
//               Line In Gain steps and Saturn Mic Tip-Ring), and the radio
//               mic catalogue (radioMicVersion). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-29: One setup description revision per on-air edge (PA and the
//               DSP RX buffer lock together). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-29: Level Cal: startLevelCalibration (a paired device, off the
//               air) and cancelLevelCalibration, and the run's progress to
//               a peer that declared levelCalibration. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-29: Level Cal: radioHardwareVersion 12, resetLevelCalibration,
//               and a window's level calibration write reaches the Core's
//               meter and TCI. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-29: Setup description version 20 (R-R3-49, R-IOS-18): PA Gain
//               publishes its on-the-air lock per row, the transmitting band
//               open to the transmit holder only. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-29: the direct media ladder: the Core's STUN server on every
//               media connection, mediaStunUrls and mediaDirectVersion.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: The Core's TCI server settings (JJ's ruling of 2026-09-28,
//               stationTciSettingsVersion 1). J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-29: iPhone app plan Task 23 (R-IOS-09, audioQualityVersion 1):
//               a device's own Opus bitrate. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-01: TX diagnostics lane: the device id prints as hex in the
//               log, not as raw bytes. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// 2026-09-27: Confirm existing paired devices through the full code exchange.
// J.J. Boyd (KG4VCF), AI-assisted implementation via OpenAI Codex.
//   2026-09-27  J.J. Boyd / KG4VCF  Task 24: negotiated Settings Hygiene
//                                    capability and paired mutation gate.
//                                    AI-assisted implementation via Codex.
// 2026-09-27: Preserve final pairing output through connection drain.
// J.J. Boyd (KG4VCF), AI-assisted implementation via OpenAI Codex.
// 2026-09-28: Parity ruling C12: a window's per-band grid write reaches the
// Core's pans. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - R-R3-49 / R-R3-46: Setup > Transmit > Power's Disable HF PA
//                applied (Thetis DisablePA and hf_tr_relay,
//                transmitSettingsVersion 11). J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-28 - R-R3-46 / R-R3-49: the Alex Filters tabs' receive filter rows
//                (per-row bypass and edges, Alex-2 master bypass) select the
//                receive high-pass as Thetis's setAlexHPF /
//                setBPF1ForOrionIISaturn / setAlex2HPF do (radioHardwareVersion
//                8). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-28: Parity ruling C4: radioHardwareVersion 9, setRadioSampleRate
// for a paired device, off the air. J.J. Boyd (KG4VCF), AI-assisted via
// Anthropic Claude Code.
//   2026-09-29 - R-R3-46 / R-R3-49: the Alex-1 Filters tab's low-pass rows
//                and 6m/ByPass on RX select the low-pass as Thetis's
//                setAlexLPF does (radioHardwareVersion 10), and radio's
//                alexLpfBits goes to a peer that declared alexLpf 1. J.J.
//                Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - A low-pass edge outside its spinner's range is refused
//                (alexLpfKeyValueRefusal), and an accepted edge moves its
//                neighbours as the Filters tab's rule does
//                (applyAlexLpfNeighbourRule). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-28: R-R3-49 (lead's ruling): a calibration settings write
//               outside its control's range (the Watt Meter points and
//               their class, TX Display Cal, the correction factors, the
//               10 MHz box, the 6 m LNA offsets, Volts/Amps) is refused
//               whole with the range in plain words
//               (calibrationKeyValueRefusal). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-29: R-R3-49 (lead's ruling): the correction factors are
//               refused outside Thetis's 0..65. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-29: R-R3-49 / R-IOS-18: paProfileVersion 1, the paProfiles
//               object and the paProfile verbs (peerGetsPaProfiles,
//               paProfileRefusal), and Setup description version 14.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: HL2 port part 2: txInhibitReasonVersion 1 and radio's
//               txInhibitReason only to a peer that declared
//               txInhibitReason 1 (fitPeerOnlyProperties). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: HL2 clock options: radioHardwareVersion 11 (the Core
//               sends Enable CL2, CL2 frequency and External 10 MHz to its
//               radio) and Setup description version 18. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: transmitSettingsVersion 15: transmit's cfcProfile only to
//               a peer that declared cfcProfile 1, and cfc.setProfile
//               (handleCfcProfileCommand), the CFC band editor applied at
//               once against an expected revision as the peer's own
//               cfcParaEqData write. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-30: TGXL tune lane (JJ's ruling): the Tuner Genius's own
//               front-panel TUNE (KeyerIdentity::tunerPress) takes
//               transmit and keys as the radio's own PTT does (ruling
//               8.9). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
// =================================================================
// src/core/session/StationServer.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 18.
// See StationServer.h for the connect sequence, the one-session topology
// decision, the threading invariant, and the heartbeat's detection model.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29  J.J. Boyd / KG4VCF  A write's corrections treat a value
//                                    that is not a number as unchanged
//                                    when it was not a number before
//                                    (sameSettledValue). AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  RADE status: radeStatusVersion 1 and
//                                    each slice's radeSynced and
//                                    radeFreqOffsetHz only to a peer that
//                                    declared radeStatus 1
//                                    (fitPeerOnlyProperties). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-08-08  J.J. Boyd / KG4VCF  Remote daemon R2 Task 18: the daemon
//                                    half of the wss session. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-08-09  J.J. Boyd / KG4VCF  Whole-branch review, Important 4:
//                                    relay a settings removal as an
//                                    absence frame, not as a value of "".
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-08-09  J.J. Boyd / KG4VCF  Whole-branch review, Minor 4:
//                                    handlePropertyWrite() answers one
//                                    inbound frame with one snapshot and
//                                    one outbound frame, not N of each.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-40: station telemetry carries
//                                    each receiver's processing load only
//                                    for a peer that negotiated minor 11
//                                    and stationTelemetryVersion 3.
//                                    AI-assisted implementation via
//                                    Anthropic Claude Code.
//                                    Later the same day: SliceModel
//                                    nnrLimit and the nnr.tryAgain command
//                                    only for a peer at minor 11, including
//                                    a write's side effects; nnrStatus
//                                    carries the step-back reason in the
//                                    Core wording to every peer.
//                                    Final review fix wave: a message is
//                                    checked read-only for nnrLimit or
//                                    nnrStatus first, and copied only
//                                    when one is present.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-08/37/40: a computed display
//                                    budget (adaptation on, nothing
//                                    configured) is in force only for a
//                                    peer at minor 11; older peers keep
//                                    legacy mode. AI-assisted
//                                    implementation via Anthropic Claude
//                                    Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-21 / R-R3-09: the `notches`
//                                    object, notchControlVersion 1, and the
//                                    plain refusal of raw Notch* writes.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-45: headphonesMixVersion 1 with
//                                    media. AI-assisted via Anthropic Claude
//                                    Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-46: hpsdrModel, radioProtocol and
//                                    radioAddress for a peer at minor 11.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-46 / R-R3-11: the `stepAtt`
//                                    object and radioHardwareVersion 1 for a
//                                    peer at minor 11, its settle reasons,
//                                    and the plain refusal of raw attenuator
//                                    and preamp settings. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-46: radioHardwareVersion 2 with
//                                    the `alexAntennas` object, the
//                                    hardware apply step after a settings
//                                    write, and hardware/<mac>/ writes
//                                    refused for any radio but the
//                                    connected one. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22: the read-only
//                                    `amplifier` and `rfkit` objects with
//                                    remotePgxlControlVersion 1 and
//                                    remoteRfKitControlVersion 1 for a peer
//                                    at minor 11, and the plain refusal of a
//                                    write to either. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-46 fix wave: a receive-only Core
//                                    refuses raw writes and removes of the
//                                    transmit-side hardware keys
//                                    (isTransmitHardwareKey). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-46 fix wave: radioHardwareVersion
//                                    3, the read-only `ioBoard` object.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-46 follow-up: the Alex TX
//                                    low-pass table and master TX switches
//                                    and OC hot switching are transmit keys.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22 / R-R3-25:
//                                    remotePgxlControlVersion 2 with the
//                                    configurePgxl, disconnectPgxl and
//                                    setPgxlConnectionSettings verbs (minor
//                                    11); a receive-only Core refuses the
//                                    tuner's isOperate, isBypass and antennaA
//                                    writes and the amplifier's operate with
//                                    one plain reason. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-48 / R-R3-25:
//                                    remoteRfKitControlVersion 2 with the
//                                    configureRfKit, disconnectRfKit and
//                                    setRfKitEnabled verbs; a raw
//                                    rfKitEnabled write refused in plain
//                                    words; stationTciVersion 1 with the
//                                    read-only `stationTci` object and the
//                                    setStationTci verb. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-IOS-01: a write to any property
//                                    MirrorPolicy marks outbound is refused
//                                    in plain words before anything is
//                                    applied. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-IOS-01: a write to a slice's
//                                    `active` is refused with
//                                    SliceModel::activeWriteReason().
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22: accessoryDataVersion
//                                    1 with the read-only `accessoryData`
//                                    object and the setTxInterlockPolicy,
//                                    setPgxlPowerCap and clearAccessoryFaults
//                                    verbs; a window's accessory setting
//                                    write reaches the Core's live objects.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 4 (R-IOS-01): the
//                                    hello advertises the station's link
//                                    majors and features; a client major
//                                    outside that list is refused in plain
//                                    words; the peer's declared features;
//                                    TLS 1.2 or later set explicitly.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24 - iPhone app Task 4b (R-IOS-01, R-R3-21): the reasons this
//                file sends an app are in operator words. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - iPhone app Part A fix wave (R-IOS-01): the hello's
//                `major` is the oldest supported major, so a client built
//                before `majors` is still served. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22:
//                                    remotePgxlControlVersion 3 and
//                                    remoteTgxlControlVersion 1 with the
//                                    read-only `accessorySettings` object
//                                    and the amp's and tuner's own settings
//                                    verbs. AI-assisted via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47: remoteRfKitControlVersion 3
//                                    with the resetRfKitError verb.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-46 / R-R3-21: radioHardwareVersion
//                                    4 with the setAlexBpfMode verb, the
//                                    filter policy from a remote window.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49: a window's Network Watchdog
//                                    change is applied to the Core's radio
//                                    when it arrives. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  Lane B takes integration (R-IOS-01,
//                                    R-R3-21): the accessory settings and
//                                    RF-Kit reset refusals in plain words.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 12 (R-IOS-08,
//                                    R-IOS-02, R-IOS-01): the Core's own
//                                    identity key and paired devices; the
//                                    hello carries the identity, its
//                                    certificate binding and a per-
//                                    connection challenge and declares
//                                    deviceAuth 1; device sign-in with its
//                                    own rate limits; a window signing in
//                                    with the token enrols its device key;
//                                    a Core without a token refuses token
//                                    sign-in; an end code on every
//                                    permanent end; stationIdentityVersion
//                                    1. AI-assisted via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R3 completion carry, review I1
//                                    (R-R3-21, R-R3-38, R-IOS-01): the
//                                    takeover and version reasons come from
//                                    SessionEndReasons, which the app parses.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 / R-R3-47: remoteTgxlControlVersion 2
//                                    (the Tuner Genius's antenna, operate
//                                    and bypass verbs).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 fix wave: remoteTgxlControlVersion 3
//                                    (setTgxlOperate on puts the tuner in
//                                    OPERATE whole).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49, Sub-epic C-1: dspAssetVersion 3 (DspAssetService
//                sends dfnrRunnable and dfnrModelStatus). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49, Sub-epic C-1: dspAssetVersion 4 (DspAssetService
//                sends mnrRunnable and mnrStatus). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  Receiver and transmit gaps plan,
//                                    Task 13: a window's External TX
//                                    Inhibit change reaches the Core's gate.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 13 (R-IOS-08): the
//                                    `devices` object and deviceAdminVersion
//                                    1 for a device at minor 11 whose hello
//                                    declares deviceAuth; its four verbs; a
//                                    removed device's connection and every
//                                    token connection after the token is
//                                    retired end at once (the requester's
//                                    own just after its result); the plain
//                                    refusal of a raw StationLabel remove.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 14 (R-IOS-08, D37):
//                                    the pairing window; pair.start in LAN
//                                    mode (unclaimed, allowed, on a
//                                    directly connected network) and code
//                                    mode (SPAKE2+EE, the code taken once
//                                    and burned on anything but success);
//                                    features.pairing and pairingVersion;
//                                    pairing.open and pairing.close; the
//                                    code on the console and only to a
//                                    connection signed in with a paired
//                                    device's key. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24: Part C fix wave: the optional device shortName in
//               auth.request, stored with the device. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24: Part C fix wave: the pairing code is never printed
//               to standard output (the journal on a packaged Core). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-24: Part C fix wave (security Minors R1-M1, M2, M4,
//               M5): the confirm-step recheck, the step 1 point check, the
//               per-address handshake cap and 0600 on load. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-24: Part C follow-up (R-IOS-08): IPv6 peers counted per /64
//               in the handshake cap; pairing.open refused to a token
//               session. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 19 (R-IOS-06): the
//                                    `catalog` object for a peer at minor
//                                    11, stationCatalogVersion 1 last in
//                                    the minor-11 block, and a refresh
//                                    when a filter preset or the CW pitch
//                                    changes in the Core's settings.
//                                    AI-assisted via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 20 (R-IOS-27):
//                                    displayExtrasVersion 1 last in the
//                                    minor-11 block. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 1):
//                                    transmitSettingsVersion 1. A
//                                    receive-only Core takes a transmit
//                                    setting (a `transmit` write outside
//                                    the keying set, a DspOptions*Tx key)
//                                    while the radio is off the air and
//                                    refuses it while it is on the air.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 2):
//                                    transmitSettingsVersion 2: the TX
//                                    and Phone/CW applets' settings on
//                                    `transmit`, a value outside a
//                                    setting's range refused in plain
//                                    words, and setTunePowerForTxBand.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 3):
//                                    transmitSettingsVersion 3: the radio
//                                    microphone settings and the Core's TX
//                                    profiles on `transmit`, the Line In
//                                    gain range, the txProfile verbs and
//                                    rade.resetVocoder.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 4):
//                                    transmitSettingsVersion 4: the TX EQ,
//                                    CFC, phase rotator, CESSB, leveler and
//                                    ALC settings on `transmit`.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 5):
//                                    transmitSettingsVersion 5: Setup >
//                                    Transmit > Power, DEXP/VOX and Test >
//                                    Two-Tone IMD on `transmit`, ATT on TX
//                                    and Force ATT on `stepAtt`; the SWR
//                                    protection and External TX Inhibit keys
//                                    taken off the air, the SWR protection
//                                    applied to the Core's controller at
//                                    once.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 / R-R3-32 (parity Task 6):
//                                    transmitSettingsVersion 6: Setup > PA's
//                                    pa/ and paCalibration/ keys taken off
//                                    the air and applied at once;
//                                    stationTelemetryVersion 4: the radio's
//                                    PA readings and link quality for a
//                                    peer at minor 11.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  Receiver and transmit gaps plan,
//                                    Task 13: a window's External TX
//                                    Inhibit change reaches the Core's gate.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  Core WebSocket opening (R-IOS-01,
//                                    R-R3-26): StationOpeningGate listens in
//                                    front of the QWebSocketServer, so every
//                                    Host form opens and a request the Core
//                                    cannot read gets 400.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25: iPhone app Task 71 (R-IOS-02): up to four device
//               sessions (DeviceSessionRegistry), admission and the same-device
//               rule in place of preemption, the away state and its grace timer,
//               session.leave, sessionHolder 1, sessionHolderVersion and
//               `connectedDevices`, per-peer capabilities and mirror sends,
//               command results to the asking session, media and telemetry to
//               one session until Task 76. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app Task 73 (R-IOS-02): slice ownership. Each view
//               receives its own slices and a SliceMarker for every other
//               (none to an older window); a write or verb naming another
//               device's slice is refused with its owner named; admission
//               returns held slices, restores saved ones, adopts unowned
//               ones for the first device alone and gives a slice-less
//               device one; the end of its 180 s, leaving and revocation
//               close, save or hold its slices; listeningOn. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
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
//                R-R3-42): capabilities carry the transmit refusal
//                (txRefusalCode/Reason/Fix) for a peer that declared
//                remoteTx, sent again when it changes. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 37 (R-IOS-13): the transmit watchdog
//               (tx.keepalive on the session and the media connection's
//               "tx" data channel; a session that ends while its device is
//               keyed stops transmitting at once), the per-mode microphone
//               starvation action, and VOX a device armed turned off with
//               its session, its link or its microphone line (a station VOX
//               key is refused while that VOX has no line to listen to).
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 39 (D14, R-IOS-13, R-IOS-21): the
//               `txState` object (TransmitState, txStateVersion 1) to a peer
//               at minor 11 declaring remoteTx; a link lost and a device
//               removed while keyed record their stop reasons. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: merge of Tasks 37 to 39: the watchdog's stop records
//               linkLost (its "went quiet" sentence, or "was lost" when the
//               session ended) and the starvation's records micStarved on
//               `txState` before StopAllTx (recordTransmitStop). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-25: iPhone app Task 74 (R-IOS-02, R-IOS-30): the receiver
//               commands and a retune leaving a shared receiver go through
//               the confirm step (StationReceivers.cpp); the property write
//               body is applyPropertyWrite; an older window with no slice
//               at admission is refused; notices after snapshot.complete.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-IOS-27, R-IOS-06: bandSelectVersion
//                                    1, last in the minor-11 block, and
//                                    slice.selectBand refused below it.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-IOS-27, R-IOS-06: notchControlVersion
//                                    2 (notch.addAtSlice).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-IOS-27, R-IOS-06:
//                                    displayExtrasVersion 2 (clarity-retune).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 7):
//                                    transmitSettingsVersion 7: a peer
//                                    offered it arms PureSignal off the air
//                                    (ps3.single, ps3.automatic,
//                                    ps3.applyCurrent, ps3.restoreCorrection)
//                                    and changes pureSignalSettings live,
//                                    refused while the radio is on the air.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 8):
//                                    remoteTgxlControlVersion 4: the Tuner
//                                    Genius's relay nudge, the Core's LAN
//                                    scan and the saved address
//                                    (moveTgxlRelay, scanTgxlLan,
//                                    setTgxlAddress).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 9):
//                                    remotePgxlControlVersion 4: the Power
//                                    Genius's OPERATE and STANDBY, the
//                                    Core's LAN scan and the saved address
//                                    (setPgxlOperate, scanPgxlLan,
//                                    setPgxlAddress).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 10):
//                                    remoteRfKitControlVersion 4: the
//                                    RF-Kit's OPERATE and STANDBY, antenna,
//                                    TCI mode and saved address
//                                    (setRfKitOperate, setRfKitAntenna,
//                                    setRfKitTciMode, setRfKitAddress);
//                                    accessoryDataVersion 2 (the rfkit*
//                                    connection counts).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 / R-R3-46 (parity Task 12):
//                                    radioHardwareVersion 6, the TX
//                                    antennas and relays on `alexAntennas`
//                                    two-way, with no on-air rule, as in
//                                    Thetis.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 / R-R3-46 (parity Task 13):
//                                    transmitSettingsVersion 8: the OC
//                                    transmit pins off the air; the OC pin
//                                    actions, TX Display Cal and Volts/Amps
//                                    Calibration on and off the air, as in
//                                    Thetis.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  Receiver and transmit gaps plan,
//                                    Task 16: a window's Receive Only change
//                                    reaches the Core's gate. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  R-R3-46 / R-R3-32 / R-R3-49 (parity
//                                    Task 14): radioHardwareVersion 7 (the
//                                    I/O board's I2C tool and output pins,
//                                    `ioBoard` outputs, and the Alex tab's
//                                    three transmit high-pass switches from
//                                    a window, on and off the air as in
//                                    Thetis); stationTelemetryVersion 5 (the
//                                    Core's HL2 link).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  R-R3-13 / R-R3-49 (parity Task 15):
//                                    meterReadingsVersion 1 (the slices'
//                                    ADC and AGC readings); a window's
//                                    Multimeter polling delay reaches the
//                                    Core's meter pump at once.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave I1: the station device's take
//               (the radio's PTT, the Core's own keys and VOX) is
//               released when its key ends, until Task 77. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: Transmit group fix wave C1: the session gate a key
//               without a microphone line is judged by. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: Transmit group fix wave C2: voxArmedByChanged; VOX a
//               device armed goes off when that device's own line closes.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave: I4 txState's holder published
//               from TransmitHolder, txStateVersion 2; M2 a two-tone stop
//               is the holder's; M8 VOX from a device with no microphone
//               line refused with the reason; M10 the keying gate judges
//               the connection the key came on. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave I2: the transmit slice is frozen
//               while the radio's own PTT keys it (ruling 8.11). J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave: the Core's own MOX is not a
//               holder on the air for ruling 7.4; the freeze is the
//               radio's own PTT's. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
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
//   2026-09-26: Transmit group fix wave 2, the re-review's minors:
//               holderTransferring true while keys are refused for a
//               transfer's reasons (a dropped holder's fence, a transfer
//               ended with MOX on); stopEpoch names the key a stop ended so
//               a newer key is never ended by it; VOX at the Core listens
//               only to the device that armed it; the window says why MOX
//               and TUNE wait while another device holds. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  R-R3-49 / R-R3-21 / R-R3-40 (parity
//                                    Task 16): dspInfoVersion 1 (the Core's
//                                    noise reduction, DSP Options apply
//                                    time, minimum notch widths and
//                                    dsp.filterResponse).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  R-R3-01 / R-R3-49 (parity Task 17):
//                                    spectrumGrantVersion 2 (a subscribe's
//                                    decimation reaches the pan's engine).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 27 (R-IOS-08): acceptPairingMailbox(),
//               a pairing through the remote access service's mailbox (pair.*
//               only, no hellos, no address). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Task 27 fix wave (I5, the operator's ruling): a code burned
//               through the mailbox counts toward the pause of pairing
//               through the service, not the window's ceiling, and a paused
//               mailbox pairing is refused before it takes a code. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03, R-IOS-13): the
//               radio's PTT takes transmit from another holder; the station
//               keeps transmit after its key; tx.take wiring; VOX arming
//               needs transmit; transmittingOn; the arbiter's freeze. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: iPhone app plan Task 77 fix round 2: the amplifier's
//               pending-key probe (a take, a device's key waiting for its
//               microphone or its two-tone settling) and the retries of an
//               owed amplifier restore. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  R-IOS-25 / R-R3-49 (parity Task 19):
//                                    recordStreamVersion 1: the record
//                                    streams (records.subscribe,
//                                    records.unsubscribe, record.batch),
//                                    the `spots` and spotConsole:<source>
//                                    streams and the read-only
//                                    `spotSources` object.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  R-IOS-18 / R-R3-49 (parity Task 21):
//                                    stationRadiosVersion 1: the
//                                    `stationRadios` stream and the four
//                                    station radio verbs.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26: D79 (R-IOS-11, R-R3-49): a taken BandPlanName write or
//               removal moves the Core's own band plan; a plan the Core
//               does not have is refused. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 77 fix round 4 (R-IOS-02, R-IOS-03,
//               R-IOS-13): a device's tunerTune that ends without keying
//               tells that device why (notice tuneEnded). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  R-R3-49 / A11 (parity Task 28):
//                                    txDisplayVersion 1 after
//                                    stationRadiosVersion: the transmit
//                                    display for a declaring media peer.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 28 (R-IOS-16): certificatePemPath(),
//               privateKeyPemPath() and sessionIceConfiguration(). J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-26: Task 28 fix wave (R-IOS-16): acceptIntroducedTransport()
//               (no pairing, device key only, one source's handshake cap,
//               the introduction to the limits), media's relay only on a
//               relayed control path, controlChannelVersion. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: Parity Task 30 (R-R3-49, R-R3-21, A12): txDisplayVersion
//               2; a window's TX Display analyzer setting, written or
//               removed, reaches the Core's TX analyzer at once, on and
//               off the air. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-27: Parity Task 31 (A11, R-R3-49): txDisplayVersion 3; a media
//               peer that declares 3 may add `duplex` to its subscribes.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27: Parity Task 32 (R-IOS-13, R-R3-49): txMonitorAudioVersion
//               1, after controlChannelVersion; while a remote device holds
//               transmit the Core's own outputs leave MON out (JJ's MON
//               ruling of 2026-09-26). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-27: Parity Task 23 (R-R3-48, R-R3-42, R-R3-49):
//               stationTciVersion 2 with the record streams: the tciClients
//               stream (the apps on the Core's station TCI server),
//               setStationTciOptions and disconnectStationTciClient. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27: iPhone plan Task 22 / parity Task 20 (R-IOS-26, R-R3-49):
//               stationFreedvVersion 1, after txMonitorAudioVersion: the
//               Core runs FreeDV Reporter and sends its list as the
//               freedvStations stream (the newest 1000 stations) and its
//               console as spotConsole:freedvReporter. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 (R-IOS-16): every session on a
//               SwitchableTransport; session.pathTicket and path.join move a
//               session to another connection; mediaReplaceVersion,
//               controlSwitchVersion and relayAllowed; media after a move
//               keeps the service's STUN server. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: remote-window parity Task 22 / iPhone app plan Task 25
//               (R-R3-49, R-IOS-18): supportBundleVersion 1, the `coreLog`
//               record stream from the log sink, and what support.collect's
//               bundle is made from (the newest telemetry, nereusd's
//               configuration file). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-27: bounded support collection and Core-log credential redaction
//               refined with OpenAI Codex assistance.
//   2026-09-27: Parity Task 33 (R-R3-49, R-R3-32, R-IOS-13):
//               txReadingsVersion 1, right after txStateVersion; the
//               txCfcCompression record stream, read every 50 ms (Thetis
//               frmCFCConfig.cs timerTick) only while a peer subscribes and
//               the radio is keyed with CFC on. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-27: R-IOS-13 / R-R3-49 (iPhone plan Task 39 row A10):
//               txModMonitorVersion 1, after relayAllowed: the AM Mod
//               Monitor's readings on the txAmModulation and
//               txAmModulationFeedback streams (ModMonitorPublisher),
//               txModMonitor.reset, and a window's ModMon/FbStream applied
//               to the Core's feedback analyzer. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-28: spot resolved mode (R-IOS-25): recordStreamVersion 2, each
//               spots record carrying resolvedMode. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-28: FreeDV band (R-IOS-26): stationFreedvVersion 2, each
//               freedvStations record carrying the station's band. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28: iPhone app plan Task 25 (R-IOS-18): vaxVersion 1, the
//               `vax` object (StationVax) for a peer whose hello declared
//               vax 1, its writes (levels 0 to 1, txGain only from a device
//               that may transmit) and the vaxLevels record stream, read 5
//               times a second while a peer subscribes. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-28: R-R3-46 / R-R3-11: adcAttenuatorVersion 1 and stepAtt's
//               rx2AttenuationDb and rx2SliceMask only to a peer that
//               declared adcAttenuators. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-28: R-IOS-13 / R-R3-49: txEqCurveVersion 1 and transmit's
//               read-only txEqCurve (the TX EQ parametric curve derived
//               from txEqParaEqData) only to a peer at minor 11 whose hello
//               declared txEqCurve 1; every other peer's schema, snapshot
//               and deltas stay as they were. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28: Phone wire batch: diversityPatternVersion 1 and each
//               slice's read-only diversityPattern only to a peer at minor
//               11 whose hello declared diversityPattern 1
//               (fitPeerOnlyProperties); every other peer's schema,
//               snapshot and deltas stay as they were. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-28: Phone wire batch: logCategoryListVersion 1 and radio's
//               logCategoryList the same way. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-28: Phone wire batch: radioModelsVersion 1 and stationRadios'
//               modelLabel and models only to a peer that declared
//               radioModels 1 (fitRecordBatchToPeer). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-28: Ruling 7.1a: a shared change applied at once tells its
//               devices after it applied (tellAppliedNow; a radio change's
//               on its restart turn). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-28: addendum G-42 (JJ's ruling): Extended transmit is the
//               Core's ExtendedTransmit setting, changed only with transmit
//               permission and off the air; transmitSettingsVersion 12 (11 is Disable HF PA).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: scoped review: setOtherDeviceHoldsRefusal, so a hosting
//               desktop's own Extended waits while another device holds
//               transmit. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-28: R-IOS-13 / R-R3-49: txEqCurveVersion 2 to a peer that
//               declared txEqCurve 2, with txEq.setCurve and
//               txEq.resetCurve applied as that peer's txEqParaEqData write
//               (the same gates, the Core's rounding and ordering, the
//               settled curve in the result). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-28: R-IOS-13 / R-R3-49 (JJ's TX EQ ruling): a receive-only
//               Core takes the TX EQ dialog's ten settings on the air, as a
//               local window changes them while transmitting
//               (isTxEqSettingTakenOnAir). J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-29: Remote parity on the air (transmitSettingsVersion 13):
//               every transmit setting a local window takes while
//               transmitting is taken on the air from a peer that may
//               change the transmit settings (takesTransmitSettingsOnAir);
//               the OC transmit pins and Region still wait. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: Unkey drain review: a revoked device on the air is stopped
//               with Stop All TX, so its unkey never waits for the send
//               ring. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
//   2026-09-29: Unkey drain review: the same device connecting again while
//               its older link was on the air is stopped with Stop All TX
//               too. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
//   2026-09-29: The phone's direct addresses: devices' coreAddresses from
//               a CoreAddressWatcher that follows the listener, with
//               coreAddressesVersion 1, only to a device signed in with its
//               own key that declared coreAddresses (peerGetsCoreAddresses).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: PA on-air gate review: the on-air PA publish also follows
//               the Core's transmit band change. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-28: R-R3-49 / R-IOS-18: Setup description version 13 (PA and
//               Hardware Config); the description also carries the Core's
//               radio for Radio Info. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-29: R-R3-49 / R-IOS-18: Setup description version 15 (the rest
//               of DSP, Transmit, Audio, Diagnostics and CAT & Network).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28: slice control and shared listening plan Task 2: who may
//               see, hear and change a slice is SliceAccessPolicy's
//               (changeRefusal, mediaSessionControlsSlice / HearsSlice /
//               SeesSlice, a listener's refusal words). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-28: slice control and shared listening plan Task 3: a
//               revoked device and a token window that has gone leave the
//               slices whose owner they were. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-28: slice control and shared listening plan Task 4:
//               sliceAccessVersion for a peer that declares sliceAccess,
//               the SliceAccess objects, each view's slice and marker
//               forms on a join, a leave or a change of controller, the
//               listen, stop listening, take control and release verbs
//               (SliceAccessController) with the controlTaken notice, a
//               listener's refusal words, and a lone device adopting only
//               unclaimed slices. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-28: slice control fix wave for the Tasks 1-4 review: a slice a
//               transmit move waits to land on counts as transmitting, and
//               a change of control clears the transmit selection of any
//               holder whose flag is on the slice; a device that leaves,
//               is revoked or stays away past its 180 s stops listening,
//               and a slice kept only for it closes; a record of each
//               device's explicit transmit choice, written only by
//               tx.setTxSlice and the hosting desktop's own selection;
//               each slice's onAir refreshed on every MOX step and when a
//               transmit move starts or stops waiting. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-29: slice control plan Task 7: a slice nobody is on closes
//               whatever the count (the Core may have none). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-29: slice control plan Task 8 (approved policy 6):
//               releaseDeviceClaims at the end of a device's 180 s (for
//               that absence only), leave, a token window's end, revoke and
//               a fifth device's take; no new holds; an away device's slice
//               may be taken during its 180 s; the away devices published
//               for Amendment 8a. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 6:
//                                    slice.setListenLevel behind the same
//                                    sliceAccess gate. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-29: slice control plan Task 10: the station device as a peer
//               for the hosting desktop (invokeAsStationDevice, runInvoke,
//               peerFor, the notice handler): its slice requests take the
//               same dispatcher, checks and confirm step as a remote
//               device's. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 11: the keying gate refuses a key
//               on a slice taken from another device and not chosen, and
//               only an explicit choice (tx.setTxSlice, the hosting
//               desktop's selection) clears the taken mark. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-29: slice control plan Task 17: sliceHolderWords() names who
//               holds a slice in a refusal: the device's name, the plain
//               word for its kind, or "another device"; the Core only for
//               nobody or the station device. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: slice control fix wave (whole-branch review, Critical 1):
//               a slice nobody is on closes only once it stops
//               transmitting (closeUnclaimedOrDefer, fireDeferredCloses);
//               slice.release and slice.stopListening on the station's
//               frozen slice are refused. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: take-over parity: sliceAccessVersion 2 offers Take it back
//               on the controlTaken notice, which carries the slice's
//               incarnation and control revision; a peer is sent the lower
//               of the Core's version and its own. Control of the hosting
//               desktop's slice passes to a remote device as from any
//               device, and the desktop is told. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: take-over fix wave (I-2): the keying gate refuses a
//               device's key that would land on the slice it lost
//               (othersSliceKeyRefusal, m_lostTxSlice). Re-review (N-1,
//               N-2): or, for a keyer that shares slices, on any other
//               device's slice; the flag moves after askKey admits. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: Level Cal 2: rx2AttenuatorVersion 1 for a peer that
//               declared rx2Attenuator. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: TX rulings (JJ's ruling 8.11 for a hosting desktop): the
//               radio's own PTT on a hosting desktop moves the flag to the
//               desktop's active slice after askKey admits, and keys there
//               (radioPttKeyRefusal). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: TX rulings (listened slice): a device's attenuator and
//               preamp writes are refused while the slice it is shown is
//               one it only listens to (listenerChangeReason). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-30: core-slice take-over (JJ, 2026-09-30): sliceAccessVersion
//               3. With nobody at the Core's desktop (no hosting desktop
//               takes the station device's notices) a device at 3 takes
//               the Core's own slice with no one to ask; a peer below 3
//               is refused as before. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: JJ's wider ruling: every slice can be taken; only a
//               slice on the air is refused. The "needs an update" and
//               "is away" refusals are gone: a controller that cannot stay
//               listening loses the slice (staysListeningAfterTake), is
//               sent no controlTaken, and an older window left with none
//               ends (ruling 6.10). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-10-01: TX mic thread (JJ approved): a "tx" channel keepalive is
//               heard at its receipt (txChannelMessage's heldUs). J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: Control logging lane: one line per control write and
//               command from a device and per answer (with its handling
//               time), and the gaps between a device's control messages,
//               rate-limited per device. Logging only. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-01: Control logging lane fix round: the log is ControlLog's;
//               hooks for each keepalive's channel, a new watch, the
//               watchdog's stop, the heartbeat and a connection's end.
//               Logging only. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "core/session/StationServer.h"
#include "core/session/BandLinkFit.h"

#include "core/TxSliceArbiter.h"
#include "core/Hl2OptionsModel.h"
#include "core/session/DataChannelTransport.h"
#include "core/session/RelayLeg.h"
#include "core/session/media/IMediaTransport.h"
#include "core/session/SwitchableTransport.h"
#include "core/session/MediaTunnel.h"
#include "core/session/ModMonitorPublisher.h"
#include "core/session/ModMonitorRecord.h"

#include "core/AppSettings.h"
#include "core/BuildIdentity.h"
#include "core/BoardCapabilities.h"
#include "core/DxccColorProvider.h"
#include "core/HardwareProfile.h"
#include "core/CfcProfile.h"
#include "core/ParaEqCurve.h"
#include "core/SpotSourceHost.h"
#include "core/LogSink.h"
#include "core/SupportBundle.h"
#include "core/session/IStationLink.h"
#include "core/session/StationTelemetry.h"
#include <QDateTime>
#include <limits>
#include <QCoreApplication>
#include <QElapsedTimer>
#include "core/station/StationRadios.h"
#include "core/WdspEngine.h"
#include "core/dsp/NnrSettings.h"
#include "core/security/CertificateStore.h"
#include "core/security/DeviceAuthenticator.h"
#include "core/security/DeviceStore.h"
#include "core/security/PairingCode.h"
#include "core/security/PairingWindow.h"
#include "core/security/SpakeExchange.h"
#include "core/security/StationIdentity.h"
#include "core/security/TokenStore.h"
#include "core/session/MirrorPolicy.h"
#include "core/session/StationVaxFacade.h"
#include "core/session/TransmitStateFacade.h"
#include "core/TxChannel.h"
#include "core/session/MirrorSchema.h"
#include "core/session/MirrorView.h"
#include "core/session/ObjectRegistry.h"
#include "core/session/SessionCommandDispatcher.h"
#include "core/session/SliceAccessController.h"
#include "core/session/SliceAccessPolicy.h"
#include "core/session/SliceAccessSet.h"
#include "core/session/SliceMarker.h"
#include "core/SliceOwnership.h"
#include "core/MoxController.h"
#include "core/safety/BandPlanGuard.h"
#include "core/safety/RemoteTxWatchdog.h"
#include "core/session/TxWatchServer.h"
#include "core/safety/TransmitHolder.h"
#include "core/safety/StationSliceFreeze.h"
#include "core/TwoToneController.h"
#include "core/session/RemoteKeying.h"
#include "core/safety/UnkeyGate.h"
#include "core/session/media/DisplayBudgetSplit.h"
#include "core/session/media/DisplayLoadGovernor.h"
#include "core/session/ConfirmStep.h"
#include "core/session/ConnectedDevicesFacade.h"
#include "core/session/DeviceSessionRegistry.h"
#include "core/session/SessionEndReasons.h"
#include "core/session/SessionTransport.h"
#include "core/session/StationOpeningGate.h"
#include "core/session/StateMirror.h"
#include "core/session/StationCatalog.h"
#include "core/session/PaProfilesFacade.h"
#include "core/PaProfileManager.h"
#include "core/session/StationDevicesFacade.h"
#include "core/session/CoreAddresses.h"
#include "core/settings/SettingsProxyServer.h"
#include "core/settings/SettingsBackupTransfer.h"
#include "core/session/SettingsBackupExportWire.h"
#include "core/settings/SettingsScope.h"
#include "models/NotchModel.h"
#include "models/FreeDVStationModel.h"
#include "models/PanadapterModel.h"
#include "models/PureSignalSettings.h"
#include "core/dsp/DspAssetService.h"
#include "PureSignalSessionFacade.h"
#include "core/PureSignal.h"
#include "core/StepAttenuatorFacade.h"
#include "core/accessories/AlexAntennaFacade.h"
#include "core/IoBoardHl2Facade.h"
#include <QScopeGuard>
#include <utility>
#include "models/BandPlanManager.h"
#include "core/AudioEngine.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include "models/AmplifierModel.h"
#include "models/RfKitModel.h"
#include "models/SpotModel.h"
#include "models/StationTciModel.h"
#include "models/AccessoryDataModel.h"
#include "models/AccessorySettingsModel.h"
#include "models/TunerModel.h"
#include "core/setup/SetupDescriptionService.h"
#include "core/PaCalProfile.h"
#include "core/codec/AlexFilterMap.h"

#include <optional>

#include <cmath>
#include <QJsonDocument>
#include <QJsonObject>
#include <array>
#include <cmath>
#include <QLoggingCategory>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QSet>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QThread>
#include <QTimer>
#include <QWebSocket>
#include <QWebSocketServer>
#include <QUrl>
#include <QUuid>

#include <algorithm>
#include <array>
#include <cstdio>

namespace NereusSDR {

struct StationServer::PendingRelayWatch {
    QPointer<DataChannelTransport> transport;
    std::shared_ptr<RelayLeg> leg;
    QElapsedTimer lifetime;
    QByteArray ticket;
    quint64 sessionId = 0;
    QByteArray deviceId;
    quint64 generation = 0;
    quint32 commandId = 0;
    QByteArray commandVerb;
    bool answered = false;
};

namespace {

// Slice control plan Task 10: the station device's stand-in transport.
// Nothing is written to it: send() hands what is addressed to it to the
// hosting desktop's callbacks (StationServer::deliverToStation).
class StationDeviceTransport final : public SessionTransport {
public:
    void sendText(const QByteArray&) override {}
    void ping() override {}
    void closeLink(const QString&) override {}
    bool isOpen() const override { return true; }
    QString peerDescription() const override { return QStringLiteral("this computer"); }
};
Q_LOGGING_CATEGORY(lcStation, "nereus.station")

// The wire identities the five singleton mirrored models are watched
// under. Slices use ObjectRegistry::keyForSlice() instead, which is
// already shared with the daemon's own lifecycle tracking.
constexpr const char* kRadioKey = "radio";
constexpr const char* kTransmitKey = "transmit";
constexpr const char* kTunerKey = "tuner";

// R-R3-40: the Core's step-back reason in a slice's nnrStatus, reworded for
// a remote window, which is not the computer that could not keep up.
QVariant coreWordedNnrStatus(const QVariant& value)
{
    const QString text = value.toString();
    for (const NnrLimit limit : {NnrLimit::StandardOnly, NnrLimit::Off}) {
        if (text == nnrLimitExplanation(static_cast<int>(limit))) {
            return nnrLimitExplanation(static_cast<int>(limit), NnrLimitSite::CoreComputer);
        }
    }
    return value;
}

const QByteArray& nnrLimitName()
{
    static const QByteArray name("nnrLimit");
    return name;
}

const QByteArray& nnrStatusName()
{
    static const QByteArray name("nnrStatus");
    return name;
}

// R-R3-40: whether fitNnrLimitToPeer would change `message` for this peer.
// Read-only, so the common case (no NNR field) neither copies nor detaches.
bool needsNnrFit(const SessionMessage& message, quint16 agreedMinor)
{
    const bool older = agreedMinor < kNnrLimitSessionProtocolMinor;
    const auto touches = [&](const QList<MirrorUpdate>& updates) {
        return std::any_of(updates.cbegin(), updates.cend(), [&](const MirrorUpdate& u) {
            return u.name == nnrStatusName() || (older && u.name == nnrLimitName());
        });
    };
    switch (message.kind) {
    case SessionMessageKind::Schema:
        return older && message.className == "SliceModel"
            && std::any_of(message.fields.cbegin(), message.fields.cend(),
                           [](const SessionSchemaField& field) {
                               return field.name == nnrLimitName();
                           });
    case SessionMessageKind::ObjectCreate:
        return message.className == "SliceModel" && touches(message.updates);
    case SessionMessageKind::Delta:
        return message.objectKey.startsWith("slice:") && touches(message.updates);
    default:
        return false;
    }
}

// Only removing nnrLimit can empty a slice delta; one left empty is not sent.
bool worthSendingAfterNnrFit(const SessionMessage& message)
{
    return message.kind != SessionMessageKind::Delta || !message.objectKey.startsWith("slice:")
        || !message.updates.isEmpty();
}

// R-R3-40: fits a mirror message to one peer. Every peer reads the Core's
// step-back reason (nnrStatus) in the Core wording; a peer below minor 11
// also loses SliceModel's nnrLimit, which it would otherwise log as a schema
// skew. Returns false when nothing is left worth sending.
bool fitNnrLimitToPeer(SessionMessage& message, quint16 agreedMinor)
{
    if (!needsNnrFit(message, agreedMinor)) {
        return worthSendingAfterNnrFit(message);
    }
    const bool older = agreedMinor < kNnrLimitSessionProtocolMinor;
    if (message.kind == SessionMessageKind::Schema) {
        message.fields.removeIf([](const SessionSchemaField& field) {
            return field.name == nnrLimitName();
        });
        return true;
    }
    if (older) {
        message.updates.removeIf([](const MirrorUpdate& update) {
            return update.name == nnrLimitName();
        });
    }
    for (MirrorUpdate& update : message.updates) {
        if (update.name == nnrStatusName()) {
            update.value = coreWordedNnrStatus(update.value);
        }
    }
    return worthSendingAfterNnrFit(message);
}

// R-R3-46: the Core's step attenuator and preamp, mirrored for a peer that
// negotiated kRadioIdentitySessionProtocolMinor. An older peer never sees
// the object, so the burst it receives is exactly the one it was built for.
constexpr const char* kStepAttKey = "stepAtt";

bool isStepAttMessage(const SessionMessage& message)
{
    return message.objectKey == kStepAttKey
        || (message.kind == SessionMessageKind::Schema
            && message.className == "StepAttenuatorFacade");
}

// R-R3-46 (radioHardwareVersion 2): the Core's Alex antenna settings, for a
// peer at kRadioIdentitySessionProtocolMinor, from a Core that also applies
// Hardware Config writes (scheduleRemoteHardwareApply).
constexpr const char* kAlexAntennasKey = "alexAntennas";

bool isAlexAntennasMessage(const SessionMessage& message)
{
    return message.objectKey == kAlexAntennasKey
        || (message.kind == SessionMessageKind::Schema
            && message.className == "AlexAntennaFacade");
}

// R-R3-46 (radioHardwareVersion 3): the Core's HL2 I/O board, read-only,
// for a peer at kRadioIdentitySessionProtocolMinor.
constexpr const char* kIoBoardKey = "ioBoard";

bool isIoBoardMessage(const SessionMessage& message)
{
    return message.objectKey == kIoBoardKey
        || (message.kind == SessionMessageKind::Schema
            && message.className == "IoBoardHl2Facade");
}

// R-R3-47 / R-R3-22: the Core's Power Genius XL and RF-Kit RF2K-S status,
// read-only, for a peer at kRadioIdentitySessionProtocolMinor on a Core
// that owns its accessories. An older peer never sees either object.
constexpr const char* kAmplifierKey = "amplifier";
constexpr const char* kRfKitKey = "rfkit";

bool isAmplifierMessage(const SessionMessage& message)
{
    return message.objectKey == kAmplifierKey
        || (message.kind == SessionMessageKind::Schema
            && message.className == "AmplifierModel");
}

bool isRfKitMessage(const SessionMessage& message)
{
    return message.objectKey == kRfKitKey
        || (message.kind == SessionMessageKind::Schema
            && message.className == "RfKitModel");
}

// R-R3-48 (stationTciVersion 1): the Core's station TCI server, read-only,
// for a peer at kRadioIdentitySessionProtocolMinor on a Core that runs one.
constexpr const char* kStationTciKey = "stationTci";

bool isStationTciMessage(const SessionMessage& message)
{
    return message.objectKey == kStationTciKey
        || (message.kind == SessionMessageKind::Schema
            && message.className == "StationTciModel");
}

// R-IOS-25 / R-R3-49 (parity Task 19, recordStreamVersion 1): the Core's
// spot sources, read-only, for a peer at kRadioIdentitySessionProtocolMinor
// on a Core with a local radio model.
constexpr const char* kSpotSourcesKey = "spotSources";

bool isSpotSourcesMessage(const SessionMessage& message)
{
    return message.objectKey == kSpotSourcesKey
        || (message.kind == SessionMessageKind::Schema
            && message.className == "SpotSourceHost");
}

// Parity Task 19: the streams and their capacities (the link's "Record
// streams" section): the newest 500 spots, the last 200 console lines.
constexpr int kSpotsStreamCapacity = 500;
constexpr int kSpotConsoleCapacity = 200;
// Parity Task 22 (R-R3-49): the Core's log keeps its newest 200 lines (a
// window's backlog), read from the log sink every 250 ms while followed.
constexpr int kCoreLogCapacity = 200;
constexpr int kCoreLogPullMs = 250;
// Parity Task 21: the radios a Core can list.
constexpr int kStationRadiosCapacity = 64;
// iPhone plan Task 22 / parity Task 20: the FreeDV Reporter stations a Core
// lists at once (qso.freedv.org lists a few hundred).
constexpr int kFreedvStationsCapacity = 1000;
// Parity Task 33: Thetis's CFC display interval, 50 ms.
constexpr int kCfcDisplayPollMs = 50;

// R-R3-47 / R-R3-22 (accessoryDataVersion 1): the Core's accessory records
// and settings, read-only, for a peer at kRadioIdentitySessionProtocolMinor
// on a Core that owns its accessories.
constexpr const char* kAccessoryDataKey = "accessoryData";

bool isAccessoryDataMessage(const SessionMessage& message)
{
    return message.objectKey == kAccessoryDataKey
        || (message.kind == SessionMessageKind::Schema
            && message.className == "AccessoryDataModel");
}

// R-R3-47 / R-R3-22 (remotePgxlControlVersion 3, remoteTgxlControlVersion
// 1): the amp's and tuner's own settings, read-only, for a peer at
// kRadioIdentitySessionProtocolMinor on a Core that owns its accessories.
constexpr const char* kAccessorySettingsKey = "accessorySettings";

// iPhone app Task 13 (R-IOS-08, deviceAdminVersion 1): the Core's paired
// devices and label, read-only, for a device at
// kRadioIdentitySessionProtocolMinor whose hello declares deviceAuth. Any
// other peer never sees the object, so its burst is today's.
constexpr const char* kDevicesKey = "devices";

bool isDevicesMessage(const SessionMessage& message)
{
    return message.objectKey == kDevicesKey
        || (message.kind == SessionMessageKind::Schema
            && message.className == "StationDevicesFacade");
}

bool isDeviceAdminVerb(const QByteArray& verb)
{
    return verb == "devices.revoke" || verb == "station.rename"
        || verb == "station.acknowledgeKeyBackup" || verb == "station.retireToken";
}

// iPhone app Task 14 (R-IOS-08, pairingVersion 1): the pairing window's
// verbs, for the same peers as the device administration verbs.
bool isPairingVerb(const QByteArray& verb)
{
    return verb == "pairing.open" || verb == "pairing.close";
}

// The `devices` property that carries the pairing code.
constexpr const char* kPairingCodeProperty = "pairingCode";

// The settings the devices object reads: its label and key backup.
bool isDevicesSettingsKey(const QString& key)
{
    return key.compare(QLatin1String("StationCallsign"), Qt::CaseInsensitive) == 0
        || isCoreOwnedIdentitySettingsKey(key);
}

// iPhone app Task 19 (R-IOS-06, stationCatalogVersion 1): the values the
// Core owns and an app draws its controls from, read-only, for a peer at
// kRadioIdentitySessionProtocolMinor. An older peer never sees the object.
constexpr const char* kCatalogKey = "catalog";

constexpr const char* kSetupDescriptionKey = "setup";

bool isSetupDescriptionMessage(const SessionMessage& message)
{
    return message.objectKey == kSetupDescriptionKey
        || (message.kind == SessionMessageKind::Schema
            && message.className == "SetupDescription");
}

// R-R3-49 / R-IOS-18 (paProfileVersion 1): the PA Gain profiles.
constexpr const char* kPaProfilesKey = "paProfiles";

bool isPaProfilesMessage(const SessionMessage& message)
{
    return message.objectKey == kPaProfilesKey
        || (message.kind == SessionMessageKind::Schema
            && message.className == "PaProfilesFacade");
}

bool isCatalogMessage(const SessionMessage& message)
{
    return message.objectKey == kCatalogKey
        || (message.kind == SessionMessageKind::Schema
            && message.className == "StationCatalog");
}

// The settings the catalogue reads: the filter presets (FilterPresetStore's
// filters/<mode>/<slot>/...) and the CW pitch the CW presets follow.
bool isCatalogSettingsKey(const QString& key)
{
    return key.startsWith(QLatin1String("filters/"))
        || key.compare(QLatin1String("CWPitch"), Qt::CaseInsensitive) == 0;
}

// iPhone app Task 71 (rulings 4.4 and 4.8): a full Core's refusal, and the
// end of a device's older connection when it connects again.
constexpr const char* kCoreFullReason = "The Core already has four devices connected.";
// The several-devices design, 15.2 (the operator-confirmed wording): an
// older window cannot answer the fifth-device question, so it is told to
// update or try again.
constexpr const char* kOlderWindowCoreFullReason =
    "The Core is full. Update NereusSDR to take a device's place, or try again later.";
constexpr const char* kSameDeviceReason = "This device connected again.";
// iPhone app Task 71 (ruling 4.12): session.leave's close; no session.end
// carries it (the accepted result already told the device).
constexpr const char* kLeftReason = "This device left the Core.";

// The link's maxDeviceSessions is the registry's.
static_assert(StationServer::kMaxDeviceSessions == DeviceSessionRegistry::kMaxDeviceSessions);

// iPhone app Task 71 (sessionHolderVersion 1): who is on the Core, for a
// view at kRadioIdentitySessionProtocolMinor whose hello declared
// sessionHolder 1 with deviceAuth 1. Any other peer never sees the object.
constexpr const char* kConnectedDevicesKey = "connectedDevices";

bool isConnectedDevicesMessage(const SessionMessage& message)
{
    return message.objectKey == kConnectedDevicesKey
        || (message.kind == SessionMessageKind::Schema
            && message.className == "ConnectedDevicesFacade");
}

// iPhone app plan Task 39 (txStateVersion 1): the Core's transmit state and
// meters, for a peer at kRadioIdentitySessionProtocolMinor whose hello
// declared remoteTx 1. Any other peer never sees the object.
constexpr const char* kTxStateKey = "txState";

bool isTxStateMessage(const SessionMessage& message)
{
    return message.objectKey == kTxStateKey
        || (message.kind == SessionMessageKind::Schema && message.className == "TransmitState");
}

// iPhone app plan Task 25 (vaxVersion 1): the station computer's VAX
// channels, for a peer at kRadioIdentitySessionProtocolMinor whose hello
// declared vax 1. Any other peer never sees the object.
constexpr const char* kVaxKey = "vax";

bool isVaxMessage(const SessionMessage& message)
{
    return message.objectKey == kVaxKey
        || (message.kind == SessionMessageKind::Schema && message.className == "StationVax");
}

// Phone wire batch: properties on an object every peer already gets that go
// only to a peer at kRadioIdentitySessionProtocolMinor whose hello declared
// `feature` 1 (StationServer::fitPeerOnlyProperties). Every other peer's
// schema, snapshot and deltas stay exactly as they were.
struct PeerOnlyProperty {
    const char* className;
    const char* objectKey;   // the key, or its prefix when keyIsPrefix
    bool keyIsPrefix;
    const char* property;
    const char* feature;
    // The phone's direct addresses: only to a connection signed in with a
    // paired device's own key (StationServer::peerGetsCoreAddresses).
    bool deviceKeyOnly = false;
};

constexpr PeerOnlyProperty kPeerOnlyProperties[] = {
    // The Diversity dialog's sensitivity pattern (diversityPatternVersion 1).
    {"SliceModel", "slice:", true, "diversityPattern", "diversityPattern"},
    {"RadioModel", "radio", false, "diversityState", "diversityControl"},
    // The Support dialog's categories with labels (logCategoryListVersion 1).
    {"RadioModel", "radio", false, "logCategoryList", "logCategoryList"},
    // Why the Core's transmit is held off (txInhibitReasonVersion 1).
    {"RadioModel", "radio", false, "txInhibitReason", "txInhibitReason"},
    // The PA row the Core holds on the air (paTransmitBandVersion 1).
    {"RadioModel", "radio", false, "paTransmitBand", "paTransmitBand"},
    // Where a device can dial this Core (coreAddressesVersion 1).
    {"StationDevicesFacade", "devices", false, "coreAddresses", "coreAddresses", true},
    // The RADE decoder's sync and frequency offset (radeStatusVersion 1).
    {"SliceModel", "slice:", true, "radeSynced", "radeStatus"},
    {"SliceModel", "slice:", true, "radeFreqOffsetHz", "radeStatus"},
    // Why a RADE slice has no working decoder (radeReasonVersion 1).
    {"SliceModel", "slice:", true, "radeReason", "radeReason"},
    // The Alex-1 low-pass in use, for the Alex tab's lamps (alexLpf 1,
    // radioHardwareVersion 10).
    {"RadioModel", "radio", false, "alexLpfBits", "alexLpf"},
    // The Core's level calibration run (levelCalibration 1,
    // radioHardwareVersion 12).
    {"RadioModel", "radio", false, "levelCalRunning", "levelCalibration"},
    {"RadioModel", "radio", false, "levelCalPercent", "levelCalibration"},
    {"RadioModel", "radio", false, "levelCalMessage", "levelCalibration"},
    {"RadioModel", "radio", false, "levelCalSucceeded", "levelCalibration"},
    // Why the receive low-pass on chain 0's input is held, and the slice
    // holding it (rxFilterLowPassVersion 1, shared-input filters ruling (d)).
    {"RadioModel", "radio", false, "rxFilter0LowPassReason", "rxFilterLowPass"},
    {"RadioModel", "radio", false, "rxFilter0LowPassSlice", "rxFilterLowPass"},
    // The CFC dialog's band editor (transmitSettingsVersion 15).
    {"TransmitModel", "transmit", false, "cfcProfile", "cfcProfile"},
};

// Phone wire batch: record fields that go only to a peer at
// kRadioIdentitySessionProtocolMinor whose hello declared `feature` 1
// (StationServer::fitRecordBatchToPeer).
struct PeerOnlyRecordField {
    const char* stream;
    const char* field;
    const char* feature;
};

constexpr PeerOnlyRecordField kPeerOnlyRecordFields[] = {
    // Each radio's model label and the models it can run as
    // (radioModelsVersion 1).
    {"stationRadios", "modelLabel", "radioModels"},
    {"stationRadios", "models", "radioModels"},
};

bool peerOnlyPropertyApplies(const PeerOnlyProperty& entry, const SessionMessage& message)
{
    if (message.kind == SessionMessageKind::Schema) {
        return message.className == entry.className;
    }
    return entry.keyIsPrefix ? message.objectKey.startsWith(entry.objectKey)
                             : message.objectKey == entry.objectKey;
}

// iPhone app Task 13: why a connection ends when its device is removed, and
// when the token it signed in with is retired (Task 12's pairing text).
constexpr const char* kDeviceRemovedReason = "This device was removed from the Core.";
constexpr const char* kPairingRequiredReason =
    "This Core uses paired devices. Pair this device first.";

bool isAccessorySettingsMessage(const SessionMessage& message)
{
    return message.objectKey == kAccessorySettingsKey
        || (message.kind == SessionMessageKind::Schema
            && message.className == "AccessorySettingsModel");
}

// R-R3-47 / R-R3-22: the verbs for the amp's and the tuner's own settings.
bool isPgxlDeviceSettingsVerb(const QByteArray& verb)
{
    return verb == "setPgxlName" || verb == "setPgxlHardware" || verb == "setPgxlNetwork"
        || verb == "savePgxlSettings" || verb == "readPgxlSettings";
}

bool isTgxlDeviceSettingsVerb(const QByteArray& verb)
{
    return verb == "setTgxlName" || verb == "setTgxlNetwork" || verb == "saveTgxlSettings"
        || verb == "readTgxlSettings";
}

// R-R3-49 / R-R3-47: the Tuner Genius's antenna, operate and bypass
// (remoteTgxlControlVersion 2).
bool isTgxlControlVerb(const QByteArray& verb)
{
    return verb == "setTgxlAntenna" || verb == "setTgxlOperate" || verb == "setTgxlBypass";
}

// R-R3-49 (parity Task 9): the Power Genius's OPERATE and STANDBY, the
// Core's LAN scan and the saved address (remotePgxlControlVersion 4).
bool isPgxlFullControlVerb(const QByteArray& verb)
{
    return verb == "setPgxlOperate" || verb == "scanPgxlLan" || verb == "setPgxlAddress";
}

// R-R3-49 (parity Task 10): the RF-Kit's OPERATE and STANDBY, antenna, TCI
// mode and saved address (remoteRfKitControlVersion 4).
bool isRfKitFullControlVerb(const QByteArray& verb)
{
    return verb == "setRfKitOperate" || verb == "setRfKitAntenna" || verb == "setRfKitTciMode"
        || verb == "setRfKitAddress";
}

bool isAccessoryTxVerb(const QByteArray& verb)
{
    return verb == "amp.operate" || verb == "amp.standby" || verb == "tuner.tune"
        || verb == "tuner.operate" || verb == "tuner.bypass" || verb == "tuner.antenna"
        || verb == "rfkit.operate" || verb == "rfkit.standby" || verb == "rfkit.antenna";
}

bool isLegacyAccessoryTxVerb(const QByteArray& verb)
{
    return verb == "setPgxlOperate" || verb == "setTgxlOperate"
        || verb == "setTgxlBypass" || verb == "setTgxlAntenna"
        || verb == "setRfKitOperate" || verb == "setRfKitAntenna";
}

bool accessoryTxArgumentsValid(const SessionMessage& message)
{
    const QByteArray& verb = message.commandVerb;
    const bool boolVerb = verb == "tuner.operate" || verb == "tuner.bypass"
        || verb == "setPgxlOperate" || verb == "setTgxlOperate"
        || verb == "setTgxlBypass" || verb == "setRfKitOperate";
    const bool portVerb = verb == "tuner.antenna" || verb == "rfkit.antenna"
        || verb == "setTgxlAntenna" || verb == "setRfKitAntenna";
    if (!boolVerb && !portVerb) {
        return message.arguments.isEmpty();
    }
    if (message.arguments.size() != 1 || message.arguments.first().ordinal != 0) {
        return false;
    }
    const MirrorUpdate& argument = message.arguments.first();
    if (boolVerb) {
        return argument.name == "on" && argument.kind == MirrorWireKind::Bool
            && argument.value.typeId() == QMetaType::Bool;
    }
    bool converted = false;
    const qlonglong port = argument.value.toLongLong(&converted);
    return argument.name == "port" && argument.kind == MirrorWireKind::Int64
        && converted && (isLegacyAccessoryTxVerb(verb)
            || (port >= 1 && port <= (verb == "tuner.antenna" ? 3 : 4)));
}

QString legacyAccessoryInvalidReason(const QByteArray& verb)
{
    if (verb == "setPgxlOperate") {
        return QStringLiteral("The request to put the Power Genius in operate or standby was not "
                              "understood.");
    }
    if (verb == "setTgxlOperate") {
        return QStringLiteral("The request to put the Tuner Genius in operate or standby was not "
                              "understood.");
    }
    if (verb == "setTgxlBypass") {
        return QStringLiteral("The request to bypass the Tuner Genius was not understood.");
    }
    if (verb == "setTgxlAntenna") {
        return QStringLiteral("The request to switch the Tuner Genius antenna was not "
                              "understood.");
    }
    if (verb == "setRfKitOperate") {
        return QStringLiteral("The request to put the RF-Kit amplifier in operate or standby was "
                              "not understood.");
    }
    return QStringLiteral("The request to switch the RF-Kit amplifier's antenna was not "
                          "understood.");
}

// R-R3-49 (parity Task 8): the relay nudge, the Core's LAN scan and the
// saved address (remoteTgxlControlVersion 4).
bool isTgxlFullControlVerb(const QByteArray& verb)
{
    return verb == "moveTgxlRelay" || verb == "scanTgxlLan" || verb == "setTgxlAddress";
}

// R-R3-47: why a raw write of the RF-Kit switch is refused. A current app
// sends setRfKitEnabled; an older one only ever wrote the value.
constexpr const char* kRfKitSwitchWriteReason =
    "Update this app to turn the RF-Kit amplifier on or off on this Core.";

// The one reason a receive-only Core gives for every transmit
// configuration write it refuses: the keying `transmit` properties and the
// transmit-side hardware keys alike (R-R3-21). R-R3-49 (parity Task 1): the
// other transmit settings are taken while the radio is off the air.
constexpr const char* kReceiveOnlyTransmitReason =
    "Transmit configuration is unavailable on this receive-only Core.";

// iPhone app plan Task 34: a property write never keys the transmitter. MOX
// and TUNE come only from the transmit controls (the transmit verbs), which
// pass the Core's gates.
// Task 35: the words the plan gives it.
constexpr const char* kPropertyNeverKeysReason = "Use the transmit button.";
// R-R3-49 (parity Task 1): the `transmit` properties that key the radio or
// arm it to key. A receive-only Core refuses a write of any of them, on and
// off the air, whether or not it mirrors the property; every other
// `transmit` property is a setting (transmitSettingsVersion 1).
bool isTransmitKeyingProperty(const QByteArray& name)
{
    return name == "mox" || name == "tune" || name == "voxEnabled" || name == "twoToneActive";
}

// R-IOS-01: the one reason for a write to a property MirrorPolicy marks
// outbound (the station's own readings and derived values, and properties
// with a command of their own). Refused before anything is applied, so a
// model's inbound hook, which exists to apply the station's reports on a
// client, never runs on the station for a client's write.
constexpr const char* kOutboundWriteReason =
    "The Core sets this itself; it cannot be changed from here.";

// The tuner properties whose remote write reaches the tuner itself
// (TunerModel::applyMirroredValue sends operate, bypass or antenna
// commands). A receive-only Core never lets a write get there.
bool isTunerTransmitPathProperty(const QByteArray& name)
{
    return name == "isOperate" || name == "isBypass" || name == "antennaA";
}

// TX rulings (JJ, 2026-09-30): the stepAtt properties that set the
// receive level, for either ADC: the attenuator, its on/off, the preamp and
// auto-attenuation (review I-2, the controller's ruling: they change the
// controlling device's receive level too). A listener of the slice it is
// shown may not change them. The transmit settings (attOnTx*,
// forceAttWhenPsOff) are not the slice's and stay as they are.
bool isListenedAttProperty(const QByteArray& name)
{
    static const QSet<QByteArray> kProperties{
        "enabled", "attenuationDb", "preampMode", "rx1Preamp",
        "autoAttEnabled", "autoAttMode", "autoAttUndo", "autoAttUndoDelayMs", "autoAttHoldMs",
        "rx2StepAttEnabled", "rx2AttenuationDb", "rx2PreampMode",
        "rx2AutoAttEnabled", "rx2AutoAttUndo", "rx2AutoAttUndoDelayMs"};
    return kProperties.contains(name);
}

// DSP > Options TX combos persist to DspOptions<Setting><Mode>Tx
// (DspOptionsPage::buildUI). The DspOptions prefix is Station-scoped, so
// these keys are station transmit settings; their Rx siblings are not.
bool isTransmitDspOptionsKey(const QString& key)
{
    return key.startsWith(QLatin1String("DspOptions"))
        && key.endsWith(QLatin1String("Tx"));
}

// R-R3-46 / R-R3-21: the transmit side of the Hardware Config and PA pages.
// Since the Core's hardware apply step reloads oc/, cal/ and hl2/ into its
// live controllers (RadioModel::scheduleRemoteHardwareApply), a raw write
// of one of these would reach the radio's transmit path, so a receive-only
// Core refuses them as it refuses TransmitModel writes. Covered:
//   hardware/<mac>/oc/tx/...            OC transmit pins (OcMatrix)
//   hardware/<mac>/oc/actions/...       OC pin transmit actions (OcMatrix)
//   hardware/<mac>/cal/{txDisplayOffset,paSens,paOffset}  (CalibrationController)
//   hardware/<mac>/paCalibration/...    PA forward-power table (CalibrationController)
//                                       and the Calibration tab's own copies of
//                                       its transmit fields (paCalibration/cal/...)
//   hardware/<mac>/hl2/{pttHangMs,txLatencyMs}            (Hl2OptionsModel)
//   hardware/<mac>/tx/...               TransmitModel, User Dig Out, mic profiles
//   hardware/<mac>/pa/...               PA profiles (PaProfileManager)
//   hardware/<mac>/powerByBand/..., tunePowerByBand/...   (TransmitModel)
//   any .../oc/extPa/...                the external PA group (OcOutputsHfTab)
//   any .../oc/allowHotSwitching        OC lines switching while transmitting
//   any .../alex/master/{hpfBypassOnTx,hpfBypassOnPs,disable6mLnaOnTx}
//   any .../alex/lpf/...                the Alex TX low-pass table
//                                       (AntennaAlexAlex1Tab, its own keys and
//                                       the Hardware page's copies)
bool isTransmitHardwareKey(const QString& rawKey)
{
    const QString key = rawKey.toLower();
    if (!key.startsWith(QLatin1String("hardware/"))) {
        return false;
    }
    const QStringList parts = key.split(QLatin1Char('/'));
    for (int i = 1; i + 1 < parts.size(); ++i) {
        const QString& here = parts[i];
        const QString& next = parts[i + 1];
        if (here == QLatin1String("oc")
            && (next == QLatin1String("extpa") || next == QLatin1String("allowhotswitching"))) {
            return true;
        }
        if (here == QLatin1String("alex") && next == QLatin1String("lpf")) {
            return true;
        }
        if (here == QLatin1String("alex") && next == QLatin1String("master") && i + 2 < parts.size()) {
            const QString& field = parts[i + 2];
            if (field == QLatin1String("hpfbypassontx") || field == QLatin1String("hpfbypassonps")
                || field == QLatin1String("disable6mlnaontx")) {
                return true;
            }
        }
    }
    if (parts.size() < 4) {
        return false;
    }
    const QString& area = parts[2];
    const QString& item = parts[3];
    if (area == QLatin1String("oc")) {
        return item == QLatin1String("tx") || item == QLatin1String("actions");
    }
    if (area == QLatin1String("cal")) {
        return item == QLatin1String("txdisplayoffset") || item == QLatin1String("pasens")
            || item == QLatin1String("paoffset");
    }
    if (area == QLatin1String("pacalibration")) {
        if (item != QLatin1String("cal")) {
            return true;
        }
        const QString field = parts.size() > 4 ? parts[4] : QString();
        return field == QLatin1String("txdisplayoffset") || field == QLatin1String("pasens")
            || field == QLatin1String("paoffset") || field == QLatin1String("padefaultrestored")
            || field == QLatin1String("logvoltsamps");
    }
    if (area == QLatin1String("hl2")) {
        return item == QLatin1String("ptthangms") || item == QLatin1String("txlatencyms");
    }
    return area == QLatin1String("tx") || area == QLatin1String("pa")
        || area == QLatin1String("powerbyband") || area == QLatin1String("tunepowerbyband");
}

// R-R3-49 (parity Task 5): Setup > Transmit > Power's SWR Protection and
// External TX Inhibit groups, Station keys (SettingsScope.cpp) that gate
// the Core's own transmitting, and (transmitSettingsVersion 11) "Disable
// HF PA" (DisableHfPa). On isTransmitSettingKeyAcceptedOffAir's list, so a
// receive-only Core takes them; since transmitSettingsVersion 13 they are
// taken on the air as well and apply at once (transmitSettingOnAirRefusal
// holds back only the OC transmit pins). Thetis's MOX setter greys none of
// them (setup.cs:5132-5161 [v2.10.3.15]).
bool isPowerPageTransmitKey(const QString& key)
{
    return RadioModel::isSwrProtectionSettingKey(key)
        || key == QLatin1String("TxInhibitMonitorEnabled")
        || key == QLatin1String("TxInhibitMonitorReversed")
        || key == QLatin1String(RadioModel::kDisableHfPaKey);
}

// R-R3-46 / R-R3-49 (parity Task 6): Setup > PA's keys, the PA profiles
// (hardware/<mac>/pa/..., PaProfileManager: PA Gain's profiles, per-band
// gains, adjust matrix and max power) and the PA forward-power table
// (hardware/<mac>/paCalibration/..., CalibrationController: the Watt Meter
// page). Taken on the air since transmitSettingsVersion 13, as a local
// window takes them. On the air a PA profile write goes through
// RadioModel::paSettingOnAirRefusal (the version 20 lock: only the active
// profile's transmitting-band row, from the transmit holder); the Watt
// Meter points are taken and applied once the radio is back on receive
// (RadioModel::scheduleRemoteHardwareApply, flushRemoteHardwareApply).
// Off the air both apply at once. The Calibration tab's own
// copies of its transmit fields (paCalibration/cal/...) are Hardware
// Config's, not the PA pages': parity Task 13 takes them
// (isTransmitHardwareKeyTakenOnAir).
bool isPaPageTransmitKey(const QString& rawKey)
{
    const QStringList parts = rawKey.toLower().split(QLatin1Char('/'));
    if (parts.size() < 4 || parts[0] != QLatin1String("hardware")) {
        return false;
    }
    if (parts[2] == QLatin1String("pa")) {
        return true;
    }
    return parts[2] == QLatin1String("pacalibration") && parts[3] != QLatin1String("cal");
}

// R-R3-46 / R-R3-49 (parity Task 13): Setup > Hardware Config's OC
// Outputs transmit pins (the HF and SWL TX matrices and their resets:
// hardware/<mac>/oc/tx/...). Taken while the radio is off the air and
// applied to the Core's OC matrix (the codec reads it for every C&C frame)
// once the radio is back on receive. Thetis greys these boxes while MOX is
// on unless OC hot switching is allowed, which NereusSDR does not build:
// From Thetis setup.cs:21944 [v2.10.3.15] UpdateForHotSwitch
// (enable = !tx || (tx && chkAllowHotSwitching.Checked)), called on every
// MOX edge from console.cs:14920 [v2.10.3.15] updateOCTXPins.
bool isOcTransmitPinKey(const QStringList& parts)
{
    return parts.size() >= 4 && parts[0] == QLatin1String("hardware")
        && parts[2] == QLatin1String("oc") && parts[3] == QLatin1String("tx");
}

// R-R3-46 / R-R3-49 (parity Task 13): the transmit settings of Hardware
// Config that Thetis changes while transmitting, so a Core takes them on
// and off the air:
//   hardware/<mac>/oc/actions/...     OC Outputs' TX pin actions. Thetis's
//     comboPin<N>TXAction handlers only store the action (Penny
//     setTXPinAction), with no MOX check.
//     From Thetis setup.cs:21773 [v2.10.3.15] comboPin1TXActionHF_SelectedIndexChanged
//   hardware/<mac>/cal/{txDisplayOffset,paSens,paOffset}, and the
//   Calibration tab's own copies under paCalibration/cal/ (with its
//   paDefaultRestored and logVoltsAmps): TX Display Cal and Volts/Amps
//   Calibration. Thetis applies each at once, keyed or not.
//     From Thetis setup.cs:14364 [v2.10.3.15] udTXDisplayCalOffset_ValueChanged
//     From Thetis setup.cs:24353 [v2.10.3.15] udAmpVoff_ValueChanged
//     From Thetis setup.cs:27627 [v2.10.3.15] chkLogVoltsAmps_CheckedChanged
bool isTransmitHardwareKeyTakenOnAir(const QStringList& parts)
{
    if (parts.size() < 4 || parts[0] != QLatin1String("hardware")) {
        return false;
    }
    const QString& area = parts[2];
    const QString& item = parts[3];
    if (area == QLatin1String("oc")) {
        return item == QLatin1String("actions");
    }
    if (area == QLatin1String("cal")) {
        return item == QLatin1String("txdisplayoffset") || item == QLatin1String("pasens")
            || item == QLatin1String("paoffset");
    }
    if (area == QLatin1String("pacalibration") && item == QLatin1String("cal")
        && parts.size() > 4) {
        const QString& field = parts[4];
        return field == QLatin1String("txdisplayoffset") || field == QLatin1String("pasens")
            || field == QLatin1String("paoffset") || field == QLatin1String("padefaultrestored")
            || field == QLatin1String("logvoltsamps");
    }
    return false;
}

// R-R3-49 (parity Task 5): the plain refusal for a Power page key's value
// the page's own control cannot hold; empty when it can. The boxes write
// "True" or "False"; the page's udTunePowerSwrIgnore spin box holds 5 to
// 50 W (PowerPage::buildSwrProtectionGroup).
// SwrProtectionLimit keeps SettingsProxyServer's own range check.
QString powerPageKeyValueRefusal(const QString& key, const QVariant& value)
{
    if (!isPowerPageTransmitKey(key) || key == QLatin1String("SwrProtectionLimit")) {
        return {};
    }
    const QString text = value.toString();
    if (key == QLatin1String("TunePowerSwrIgnore")) {
        bool ok = false;
        const int watts = text.toInt(&ok);
        return ok && watts >= 5 && watts <= 50
            ? QString()
            : QStringLiteral("Choose a tune power to ignore from 5 to 50 W.");
    }
    return text == QLatin1String("True") || text == QLatin1String("False")
        ? QString()
        : QStringLiteral("The Core expected this box to be on or off.");
}

// R-R3-49 (lead's ruling, PA and Hardware Config publication): the plain
// refusal for a calibration value its Setup control cannot hold; empty when
// it can, or when the key is not one of these. The value is refused whole,
// never clamped, as the TX EQ band arrays are. The ranges are the controls'
// own, Thetis's where the control carries them:
//   Watt Meter points: paCalPointSpec (setup.designer.cs ud{10|100|200}PA{N}W
//     [v2.10.3.15]) for the Core's radio's class; its boardClass is that class.
//   TX Display Cal:  From Thetis setup.designer.cs:11870 [v2.10.3.15]
//     udTXDisplayCalOffset Maximum = 100, Minimum = -100
//   6 m LNA offsets: From Thetis setup.designer.cs:12096 [v2.10.3.15]
//     ud6mLNAGainOffset Maximum = 25, Minimum = 0 (ud6mRx2LNAGainOffset :12054)
//   Volts/Amps:      From Thetis setup.designer.cs:11789 [v2.10.3.15]
//     udAmpSens Maximum = 5000, Minimum = 0.001; :11819 udAmpVoff 0 to 5000
//   Correction factors: From Thetis setup.designer.cs:11983 [v2.10.3.15]
//     udHPSDRFreqCorrectFactor Maximum = 65, Minimum = 0 (the 10 MHz box
//     :11928, the same; lead's ruling: Thetis's range).
// The Calibration tab also keeps its own copies under paCalibration/cal/.
QString calibrationKeyValueRefusal(const QString& key, const QVariant& value, HPSDRModel model)
{
    const QStringList parts = key.split(QLatin1Char('/'));
    if (parts.size() < 4 || parts[0].compare(QLatin1String("hardware"), Qt::CaseInsensitive) != 0) {
        return {};
    }
    QString rest = parts.mid(2).join(QLatin1Char('/'));
    if (rest.startsWith(QLatin1String("paCalibration/cal/"), Qt::CaseInsensitive)) {
        rest = QStringLiteral("cal/") + rest.mid(QStringLiteral("paCalibration/cal/").size());
    }
    const QString text = value.toString();
    bool ok = false;
    const double number = text.toDouble(&ok);
    const bool finite = ok && std::isfinite(number);
    const auto within = [&](double lo, double hi) { return finite && number >= lo && number <= hi; };
    if (rest.startsWith(QLatin1String("paCalibration/calPoint"), Qt::CaseInsensitive)) {
        bool pointOk = false;
        const int point = rest.mid(QStringLiteral("paCalibration/calPoint").size()).toInt(&pointOk);
        const PaCalPointSpec spec = paCalPointSpec(paCalBoardClassFor(model), point);
        if (!pointOk || spec.maximum <= 0.0) {
            return QStringLiteral("This radio has no power meter calibration.");
        }
        return within(0.0, spec.maximum)
            ? QString()
            : QStringLiteral("Choose a calibration point from 0 to %1 W.").arg(spec.maximum);
    }
    if (rest.compare(QLatin1String("paCalibration/boardClass"), Qt::CaseInsensitive) == 0) {
        const PaCalBoardClass boardClass = paCalBoardClassFor(model);
        return boardClass != PaCalBoardClass::None
                && text == QString::number(static_cast<int>(boardClass))
            ? QString()
            : QStringLiteral("The Core expected this radio's power calibration table.");
    }
    if (rest.compare(QLatin1String("cal/txDisplayOffset"), Qt::CaseInsensitive) == 0) {
        return within(-100.0, 100.0)
            ? QString() : QStringLiteral("Choose a TX display offset from -100 to 100 dB.");
    }
    if (rest.compare(QLatin1String("cal/freqFactor"), Qt::CaseInsensitive) == 0
        || rest.compare(QLatin1String("cal/freqFactor10M"), Qt::CaseInsensitive) == 0) {
        return within(0.0, 65.0) ? QString()
                                 : QStringLiteral("Choose a correction factor from 0 to 65.");
    }
    if (rest.compare(QLatin1String("cal/rx1_6mLna"), Qt::CaseInsensitive) == 0
        || rest.compare(QLatin1String("cal/rx2_6mLna"), Qt::CaseInsensitive) == 0) {
        return within(0.0, 25.0) ? QString()
                                 : QStringLiteral("Choose a 6 m LNA offset from 0 to 25 dB.");
    }
    if (rest.compare(QLatin1String("cal/paSens"), Qt::CaseInsensitive) == 0) {
        return within(0.001, 5000.0)
            ? QString() : QStringLiteral("Choose an amp sensitivity from 0.001 to 5000.");
    }
    if (rest.compare(QLatin1String("cal/paOffset"), Qt::CaseInsensitive) == 0) {
        return within(0.0, 5000.0)
            ? QString() : QStringLiteral("Choose an amp voltage offset from 0 to 5000.");
    }
    if (rest.compare(QLatin1String("cal/using10M"), Qt::CaseInsensitive) == 0
        || rest.compare(QLatin1String("cal/logVoltsAmps"), Qt::CaseInsensitive) == 0) {
        // The Calibration tab's own copies (paCalibration/cal/) hold a
        // stored bool, which the settings proxy sends as "true"/"false".
        return text.compare(QLatin1String("True"), Qt::CaseInsensitive) == 0
                || text.compare(QLatin1String("False"), Qt::CaseInsensitive) == 0
            ? QString() : QStringLiteral("The Core expected this box to be on or off.");
    }
    return {};
}

// A write's corrections compare each settled value with the one before the
// write. A double that is not a number (a slice's SNR before a RADE decoder
// locks) never equals itself as a QVariant, so without this it read as
// changed and went back to the writer after every write.
bool sameSettledValue(const QVariant& a, const QVariant& b)
{
    if (a.typeId() == QMetaType::Double && b.typeId() == QMetaType::Double
        && std::isnan(a.toDouble()) && std::isnan(b.toDouble())) {
        return true;
    }
    return a == b;
}

// The plain refusal for an HL2 CL2 frequency its Setup box cannot hold;
// empty when it can, or when the key is not that one. Refused whole, never
// clamped, as the calibration values are. From mi0bot setup.designer.cs:
// 11133-11163 [@c26a8a4] udCl2Freq: Maximum = 200, Minimum = 1 (MHz),
// DecimalPlaces = 3.
QString hl2ClockKeyValueRefusal(const QString& key, const QVariant& value)
{
    const QStringList parts = key.split(QLatin1Char('/'));
    if (parts.size() != 4 || parts[0].compare(QLatin1String("hardware"), Qt::CaseInsensitive) != 0
        || parts[2].compare(QLatin1String("hl2"), Qt::CaseInsensitive) != 0
        || parts[3].compare(QLatin1String("cl2FreqMHz"), Qt::CaseInsensitive) != 0) {
        return {};
    }
    int kHz = 0;
    return Hl2OptionsModel::parseCl2FreqMHz(value.toString(), &kHz)
            && kHz >= Hl2OptionsModel::kCl2FreqMinKHz && kHz <= Hl2OptionsModel::kCl2FreqMaxKHz
        ? QString()
        : QStringLiteral("Choose a CL2 frequency from 1 to 200 MHz.");
}

// R-R3-49 (parity Task 5): true when both values are the same JSON object.
// The Core writes a band map (powerByBandJson, tunePowerByBandJson) back with
// its keys in its own order, so a map it took whole reads back as the same
// object, not the same text.
bool sameJsonObject(const QVariant& a, const QVariant& b)
{
    const QJsonDocument left = QJsonDocument::fromJson(a.toString().toUtf8());
    const QJsonDocument right = QJsonDocument::fromJson(b.toString().toUtf8());
    if (!left.isObject() || !right.isObject()) {
        return false;
    }
    // R-IOS-26 / R-R3-49: a per-band watts map written without "2m" (a
    // peer built before 2 m, or one writing the 14 it knows) keeps 2 m's
    // value; the Core's map then matches it without its "2m" key.
    QJsonObject kept = left.object();
    if (!right.object().contains(QLatin1String("2m"))) {
        kept.remove(QLatin1String("2m"));
    }
    return kept == right.object();
}

// R-IOS-26 / R-R3-49: a per-band antenna list written with the 14 entries
// of a peer built before 2 m keeps 2 m's entry (AlexAntennaFacade::decode);
// the Core's 15-entry list then matches it without its last entry.
bool sameAntennaListWithout2m(const QVariant& actual, const QVariant& written)
{
    QStringList kept = actual.toString().split(QLatin1Char(','));
    const QStringList sent = written.toString().split(QLatin1Char(','));
    if (kept.size() != BandLinkFit::kListEntriesWithout2m + 1
        || sent.size() != BandLinkFit::kListEntriesWithout2m) {
        return false;
    }
    kept.removeLast();
    return kept == sent;
}

// R-R3-46 / R-R3-49 (parity Task 14, radioHardwareVersion 7): the Alex
// tab's three transmit high-pass switches (HPF Bypass on TX, HPF Bypass on
// PureSignal, Disable 6m LNA on TX), under any .../alex/master/. The Core
// applies them to its connection at once (RadioModel::
// applyAlexHpfSwitchSettings). A receive-only Core takes them from a peer
// offered version 7, on the air too: Thetis's setters apply each at once
// with no MOX check, as Task 12 found for the TX antennas.
// From Thetis console.cs:18719-18803 [v2.10.3.15] Disable6mLNAonTX /
//   DisableHPFonTX / DisableHPFonPS { set { ...; setAlex1HPF(freq); } }
// Upstream inline attribution preserved verbatim (console.cs:18731):
//   HardwareSpecific.Model == HPSDRModel.ANAN_G2_1K || HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
bool isAlexHpfTransmitSwitchKey(const QString& rawKey)
{
    const QStringList parts = rawKey.toLower().split(QLatin1Char('/'));
    if (parts.isEmpty() || parts[0] != QLatin1String("hardware")) {
        return false;
    }
    for (int i = 1; i + 2 < parts.size(); ++i) {
        if (parts[i] == QLatin1String("alex") && parts[i + 1] == QLatin1String("master")) {
            const QString& field = parts[i + 2];
            return field == QLatin1String("hpfbypassontx")
                || field == QLatin1String("hpfbypassonps")
                || field == QLatin1String("disable6mlnaontx");
        }
    }
    return false;
}

// R-R3-46 / R-R3-49 (radioHardwareVersion 10): the Alex-1 Filters tab's
// low-pass rows, under any .../alex/lpf/. The Core stores them for its
// connection's next selection (RadioModel::applyAlexHpfSwitchSettings), as
// Thetis's spinner handlers store them with no MOX check and no re-select
// (setup.cs:15888-15994 [v2.10.3.15]). A receive-only Core takes them from
// a peer offered version 10, on the air too.
bool isAlexLpfRowKey(const QString& rawKey)
{
    const QStringList parts = rawKey.toLower().split(QLatin1Char('/'));
    if (parts.isEmpty() || parts[0] != QLatin1String("hardware")) {
        return false;
    }
    for (int i = 1; i + 1 < parts.size(); ++i) {
        if (parts[i] == QLatin1String("alex") && parts[i + 1] == QLatin1String("lpf")) {
            return true;
        }
    }
    return false;
}

// One low-pass row edge's key: hardware/<mac>/alex/lpf/<slug>/<start|end>.
struct AlexLpfEdgeKey {
    QString mac;
    QStringList prefix;  // "hardware", the MAC as written, "alex", "lpf"
    int row {0};
    bool isEnd {false};
    // Written as the Core stores it (lowercase "hardware/<mac>/alex/lpf/
    // <slug>/<start|end>"). The key is recognized whatever its case, so a
    // write in any other case is refused rather than stored under a key
    // the radio and the neighbour rule never read.
    bool canonical {false};
};

QString alexLpfEdgeKeyFor(const AlexLpfEdgeKey& base, int row, bool isEnd)
{
    QStringList parts = base.prefix;
    parts << QString::fromLatin1(codec::alex::kAlexLpfRowSlugs[row])
          << (isEnd ? QStringLiteral("end") : QStringLiteral("start"));
    return parts.join(QLatin1Char('/'));
}

std::optional<AlexLpfEdgeKey> parseAlexLpfEdgeKey(const QString& rawKey)
{
    const QStringList parts = rawKey.split(QLatin1Char('/'));
    if (parts.size() != 6
        || parts[0].compare(QLatin1String("hardware"), Qt::CaseInsensitive) != 0
        || parts[2].compare(QLatin1String("alex"), Qt::CaseInsensitive) != 0
        || parts[3].compare(QLatin1String("lpf"), Qt::CaseInsensitive) != 0) {
        return std::nullopt;
    }
    AlexLpfEdgeKey out;
    out.mac = parts[1];
    out.prefix = QStringList{QStringLiteral("hardware"), parts[1], QStringLiteral("alex"),
                             QStringLiteral("lpf")};
    out.row = -1;
    for (int i = 0; i < codec::alex::kAlexLpfRowCount; ++i) {
        if (parts[4].compare(QLatin1String(codec::alex::kAlexLpfRowSlugs[i]),
                             Qt::CaseInsensitive) == 0) {
            out.row = i;
        }
    }
    if (out.row < 0) {
        return std::nullopt;
    }
    if (parts[5].compare(QLatin1String("start"), Qt::CaseInsensitive) == 0) {
        out.isEnd = false;
    } else if (parts[5].compare(QLatin1String("end"), Qt::CaseInsensitive) == 0) {
        out.isEnd = true;
    } else {
        return std::nullopt;
    }
    out.canonical = rawKey == alexLpfEdgeKeyFor(out, out.row, out.isEnd);
    return out;
}

// R-R3-46 / R-R3-49 (review C1): the plain refusal for a low-pass edge its
// spinner could not hold; empty when it can, or when the key is not one.
// The value is refused whole, never clamped, as the calibration values are.
// The ranges are the spinners' own (codec::alex::kAlexLpfEdgeLimits,
// setup.designer.cs [v2.10.3.15]): an edge outside them would send a
// transmission through a low-pass below its frequency.
QString alexLpfKeyValueRefusal(const QString& key, const QVariant& value)
{
    const std::optional<AlexLpfEdgeKey> edge = parseAlexLpfEdgeKey(key);
    if (!edge) {
        return {};
    }
    // A band name (160m to 6m), "start" or "end", and the two limits.
    const QString band = QLatin1String(codec::alex::kAlexLpfRowSlugs[edge->row]);
    const QString edgeName = edge->isEnd ? QStringLiteral("end") : QStringLiteral("start");
    // Review follow-up: a key in another case is refused, never stored.
    if (!edge->canonical) {
        return QStringLiteral("This app named the %1 low-pass %2 in a way the Core "
                              "does not store. Updating the app may help.")
            .arg(band, edgeName);
    }
    bool ok = false;
    const double mhz = value.toString().toDouble(&ok);
    if (ok && codec::alex::alexLpfEdgeAllowed(edge->row, edge->isEnd, mhz)) {
        return {};
    }
    const codec::alex::AlexLpfEdgeLimits& lim =
        codec::alex::kAlexLpfEdgeLimits[static_cast<size_t>(edge->row)];
    const double lo = edge->isEnd ? lim.endMin : lim.startMin;
    const double hi = edge->isEnd ? lim.endMax : lim.startMax;
    const QString lowest = QString::number(lo, 'g', 10);
    const QString highest = QString::number(hi, 'g', 10);
    return QStringLiteral("Choose the %1 low-pass %2 from %3 to %4 MHz.")
        .arg(band, edgeName, lowest, highest);
}

// Every settings key a receive-only Core refuses as transmit configuration.
// R-R3-49 (parity Task 1): from a peer offered transmitSettingsVersion 1,
// the keys StationServer::isTransmitSettingKeyAcceptedOffAir lists are
// taken off the air instead (StationServer::receiveOnlyRefusesKey).
bool isReceiveOnlyRefusedKey(const QString& key)
{
    return isTransmitDspOptionsKey(key) || isTransmitHardwareKey(key);
}

QByteArray panKey(int index)
{
    return QByteArrayLiteral("pan:") + QByteArray::number(index);
}

QString peerNameForThisProcess()
{
    return QStringLiteral("nereusd");
}

// Each side's own AppSettings schema version, read by the key name
// AppSettings::ensureSettingsAtVersion() writes it under. Read rather than
// hardcoded: the literal lives at exactly one place today (CoreInit.cpp's
// ensureSettingsAtVersion(9) call), and duplicating it here would create a
// second copy free to drift from the migrations that actually ran.
qint32 settingsSchemaVersionOf(const AppSettings& settings)
{
    return static_cast<qint32>(
        settings.value(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("0"))
            .toString()
            .toInt());
}

// Written straight to stdout with C stdio, deliberately NOT through
// qCInfo() like every other line in this class. That is a correctness fix,
// not a style preference, and it closes two separate defects.
//
// FIRST, the banner was arriving MANGLED. CoreInit::initialize() installs
// a process-wide qInstallMessageHandler whose handler passes every message
// through redactPii() before it reaches stderr or the log file, and
// redactPii's MAC rule used to be a bare six-pair hex body -- which is a
// strict prefix of the 32-pair colon-separated SHA-256 fingerprint printed
// two lines below. It matched five times over inside one fingerprint and
// replaced 25 of its 32 bytes with asterisks, while the banner still said
// the values were printed once, here. StationClient::connectToStation
// refuses to dial without a fingerprint and nereusd has no option to
// reprint one, so that left an operator with no way forward. redactPii is
// now narrowed too (CoreInit.cpp), because a fingerprint logged from
// anywhere else would otherwise still be destroyed; this function is the
// other half, not a substitute for it.
//
// SECOND, and the reason the fix is a different STREAM rather than a
// different regex: that same handler writes every message verbatim into
// ~/.config/NereusSDR/profiles/<profile>/nereussdr-<stamp>.log, kept
// indefinitely and symlinked as nereussdr.log. That is the file
// CONTRIBUTING.md tells operators to attach to a bug report. Routing the
// token through it contradicts TokenStore.h's own stated reason for
// keeping the secret out of AppSettings -- "a secret sitting in the same
// XML the operator backs up, mails to a maintainer with a bug report" --
// against a worse medium than the one that header rejects. Bypassing the
// handler entirely is what keeps the token out of the log file; no
// redaction rule could, because the token is 43 characters of base64url
// with no shape to match on.
//
// STDOUT rather than stderr. Under packaging/nereusd.service.in this
// process sets neither StandardOutput= nor StandardError=, so systemd's
// defaults put both streams in the journal and the banner reaches
// `journalctl -u nereusd` either way; the systemd case does not decide it.
// What decides it is what each stream means. This banner is the run's
// primary output, two values the operator is being asked to copy, not a
// diagnostic. stderr in this process is already owned by the Qt handler,
// so putting the banner there would interleave a copy-paste block with
// redacted diagnostic lines, and an operator debugging by hand with
// `nereusd 2> daemon-errors.log` would lose it off the terminal.
//
// The explicit fflush is load-bearing rather than hygiene: stdout is
// block-buffered whenever it is not a terminal, which is exactly the
// systemd case, so without it the banner would sit in libc's buffer until
// it filled or the daemon exited.
void writePairingBanner(const QString& banner)
{
    const QByteArray bytes = banner.toUtf8();
    std::fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stdout);
    std::fflush(stdout);
}
} // namespace

// iPhone app Task 14 (R-IOS-08): one connection's pairing, from its
// pair.start until the connection ends.
struct StationServer::PairingAttempt {
    bool codeMode = false;
    // Code pairing may confirm an existing device without changing its record.
    std::optional<PairedDevice> existingDevice;
    bool existingDeviceRevoked = false;
    /// The device's key as pair.start sent it (base64url) and as SPKI DER.
    QString publicKeyText;
    QByteArray publicKeySpki;
    QString name;
    QString kind;
    /// Code mode: the code this exchange started on, the exchange, and the
    /// device's next message: its step 1, then its step 3, then its box.
    quint64 codeSerial = 0;
    std::unique_ptr<SpakeExchange> exchange;
    int expecting = 1;
    /// Waiting for the code's hash (a worker) before step 0 is sent.
    bool awaitingHash = false;
    /// The window gave this exchange the code: from here the code is
    /// paired with or burned (dropPeer burns it unless `finished`).
    bool codeTaken = false;
    bool finished = false;
    /// Through the remote access service's mailbox, or direct: what a
    /// burned code counts toward (PairingWindow, the ruling on Task 27
    /// item I5).
    PairingWindow::Route route = PairingWindow::Route::Direct;
};

StationServer::StationServer(RadioModel* radioModel, AppSettings& settings,
                             const QString& securityDirectory, QObject* parent,
                             const QList<quint16>& supportedMajors)
    : QObject(parent)
    , m_radioModel(radioModel)
    , m_settings(settings)
    , m_securityDirectory(securityDirectory.isEmpty() ? CertificateStore::defaultDirectory()
                                                      : securityDirectory)
    , m_supportedMajors(supportedMajors.isEmpty() ? LinkVersion::supportedMajors()
                                                  : supportedMajors)
{
    // Oldest first and each once, as the hello sends them.
    std::sort(m_supportedMajors.begin(), m_supportedMajors.end());
    m_supportedMajors.erase(std::unique(m_supportedMajors.begin(), m_supportedMajors.end()),
                            m_supportedMajors.end());
    // Slice control plan Task 10: the station device as a peer that shares
    // slices, for the hosting desktop's requests (invokeAsStationDevice).
    m_stationTransport = std::make_unique<StationDeviceTransport>();
    m_stationPeer.transport = m_stationTransport.get();
    m_stationPeer.description = QStringLiteral("this computer");
    m_stationPeer.helloReceived = true;
    m_stationPeer.authenticated = true;
    m_stationPeer.snapshotComplete = true;
    m_stationPeer.agreedMajor = kSessionProtocolMajor;
    m_stationPeer.agreedMinor = kSessionProtocolMinor;
    m_stationPeer.features = {{QByteArrayLiteral("deviceAuth"), 1},
                              {QByteArrayLiteral("sessionHolder"), 1},
                              // Take-over parity: Take it back on
                              // controlTaken (sliceAccessVersion 2).
                              // Core-slice take-over: 3.
                              {QByteArrayLiteral("sliceAccess"), 3},
                              {QByteArrayLiteral("diversityControl"), 1},
                              {QByteArrayLiteral("diversityPattern"), 1}};
    m_stationPeer.deviceId = SliceOwnership::stationDevice();
    m_stationPeer.sessionDeviceId = SliceOwnership::stationDevice();
    m_stationPeer.placeSettled = true;
    // Above any id ++m_nextSessionId gives an admitted session.
    m_stationPeer.sessionId = std::numeric_limits<quint64>::max();
    // iPhone app plan Task 34: the Core's blanket receive-only policy is now
    // its remote_transmit setting (setRemoteTransmitAllowed). Deny, the
    // default, keeps it as before: the hardware-owning model refuses every
    // key and transmit side effect, so a standalone StationServer host
    // cannot admit one through a local callback. DaemonApp applies its
    // config (default allow) before startup; nothing clears it when a
    // session ends.
    if (m_radioModel) {
        m_radioModel->setReceiveOnlyStationPolicy(!m_txGate.remoteTransmitAllowed());
    }

    m_certificates = std::make_unique<CertificateStore>(m_securityDirectory);
    // iPhone app Task 12: loaded when an earlier Core left one, never
    // created (TokenStore.h).
    m_tokens = std::make_unique<TokenStore>(m_securityDirectory);
    m_identity = std::make_unique<StationIdentity>(
        StationIdentity::loadOrCreate(m_securityDirectory));
    m_devices = std::make_unique<DeviceStore>(m_securityDirectory, m_tokens.get());
    m_deviceAuth = std::make_unique<DeviceAuthenticator>(*m_devices, *m_identity);
    m_txWatchServer = std::make_unique<TxWatchServer>(
        [this](SessionTransport* primary, quint64 sessionId, const QByteArray& deviceId,
               quint64 generation) {
            return txWatchBindingCurrent(primary, sessionId, deviceId, generation);
        },
        [this](const QByteArray& deviceId, quint64 sequence, quint32 epoch) {
            if (m_txWatchdog) {
                // Control logging lane: logged after, as it was watched.
                const bool watched = m_txWatchdog->isWatching(deviceId);
                m_txWatchdog->keepalive(deviceId, sequence, epoch,
                                        RemoteTxWatchdog::Path::Auxiliary);
                controlLogKeepalive(deviceId, ControlLog::KeepaliveChannel::TxWatch, 0, watched);
            }
        }, this);
    // iPhone app Task 71 (R-IOS-02): who holds a place, and the mirrored
    // `connectedDevices` object that shows it.
    m_deviceSessions = std::make_unique<DeviceSessionRegistry>();
    // iPhone app Task 74 (R-IOS-30): the questions asked and notices kept.
    m_confirm = std::make_unique<ConfirmStep>();
    m_connectedDevices = std::make_unique<ConnectedDevicesFacade>(*m_deviceSessions, *m_devices);

    // iPhone app plan Task 34 (R-IOS-02, rulings 8.1, 8.2, 8.4, 8.15): who
    // holds transmit. Its hooks reach the radio through the model: the
    // unkey gate, StopAllTx, MOX as MoxController reads it, VOX, and the
    // devices' words as connectedDevices numbers them.
    m_transmitHolder = std::make_unique<TransmitHolder>();
    {
        TransmitHolder::Hooks hooks;
        hooks.clock = [this]() { return m_deviceSessions->now(); };
        hooks.moxOn = [this]() {
            const MoxController* mox = m_radioModel ? m_radioModel->moxController() : nullptr;
            return mox != nullptr && (mox->isMox() || mox->state() != MoxState::Rx);
        };
        hooks.unkey = [this](const QString& reason, std::function<void(UnkeyOutcome)> done) {
            if (m_radioModel && m_radioModel->unkeyGate() != nullptr) {
                m_radioModel->unkeyGate()->unkey(reason, this, std::move(done));
            } else if (done) {
                done(UnkeyOutcome::Confirmed);
            }
        };
        hooks.stopAllTx = [this](const QString& reason) {
            if (m_radioModel) {
                m_radioModel->stopAllTx(reason);
            }
        };
        hooks.disarmVox = [this]() {
            if (m_radioModel && m_radioModel->transmitModel().voxEnabled()) {
                m_radioModel->transmitModel().setVoxEnabled(false);
            }
        };
        hooks.schedule = [this](int ms, std::function<void()> fire) {
            QTimer::singleShot(ms, this, std::move(fire));
        };
        hooks.describe = [this](const QByteArray& id) -> std::optional<TransmitHolder::Words> {
            if (id == KeyerIdentity::kStationDeviceId) {
                // Fix wave 2 (ruling 8.1): the hosting desktop's words, when
                // one hosts this Core.
                if (m_stationWords.name.isEmpty()) {
                    return std::nullopt;
                }
                return TransmitHolder::Words{m_stationWords.name, m_stationWords.shortName,
                                             QStringLiteral("station")};
            }
            if (const auto words = m_connectedDevices->describe(id)) {
                return TransmitHolder::Words{words->name, words->shortName, words->kind};
            }
            return std::nullopt;
        };
        m_transmitHolder->setHooks(std::move(hooks));
    }
    m_txGate.setTransmitHolder(m_transmitHolder.get());
    connect(m_transmitHolder.get(), &TransmitHolder::changed, this,
            &StationServer::onTransmitHolderChanged);
    // Slice control fix wave (whole-branch review, Critical 1): the station
    // freeze ends with the holder's key; a close waiting for it may run.
    connect(m_transmitHolder.get(), &TransmitHolder::changed, this,
            &StationServer::scheduleDeferredCloses);
    m_connectedDevices->setTransmitProvider([this]() {
        ConnectedDevicesFacade::TransmitState state;
        if (const auto holder = m_transmitHolder->holder()) {
            state.holderDeviceId = holder->deviceId;
            state.keyed = holder->keyed;
            state.keyedSinceMs = holder->keyedSinceMs;
            // Task 77: connectedDevices.transmittingOn.
            const SliceModel* slice = m_radioModel ? m_radioModel->txBoundSlice() : nullptr;
            if (holder->keyed && slice != nullptr) {
                const int id = slice->sliceIndex();
                state.transmittingOn = QJsonObject{
                    {QStringLiteral("sliceId"), id},
                    {QStringLiteral("letter"), QString(QChar(QLatin1Char('A').unicode() + id))},
                    {QStringLiteral("band"), static_cast<int>(slice->band())},
                    {QStringLiteral("mode"), static_cast<int>(slice->dspMode())},
                };
            }
        }
        return state;
    });
    if (m_radioModel && m_radioModel->role() == RadioModel::Role::Local
        && m_radioModel->moxController() != nullptr) {
        MoxController* mox = m_radioModel->moxController();
        // Rulings 8.8 and 8.13: every key, local or remote, asks here first.
        mox->setKeyingGate([this](PttMode source, const KeyerIdentity& keyer) -> KeyingAnswer {
            if (!keyer.isStation()) {
                // A remote key: the session's own gate first (remote_transmit,
                // its hello, its pairing, its snapshot).
                // Fix wave M10: the connection the key came on, when the
                // key names it; otherwise the device's live connection.
                SessionTransport* session = nullptr;
                const quint64 sessionId = sessionIdOfOwner(keyer.session);
                for (auto it = m_peers.cbegin(); it != m_peers.cend(); ++it) {
                    if (it->sessionDeviceId != keyer.deviceId) {
                        continue;
                    }
                    if (sessionId != 0 ? it->sessionId == sessionId : session == nullptr) {
                        session = it.key();
                        if (sessionId != 0) {
                            break;
                        }
                    }
                }
                if (session == nullptr) {
                    return {KeyingVerdict::Refuse, TxRefusals::notReady()};
                }
                const SessionPeerInfo peer = peerInfoFor(session);
                StationTxGate sessionOnly;
                sessionOnly.setRemoteTransmitAllowed(m_txGate.remoteTransmitAllowed());
                const TxDecision decision = sessionOnly.decide(peer);
                if (!decision.permitted) {
                    return {KeyingVerdict::Refuse, decision.refusal};
                }
            }
            // iPhone app plan Task 37: VOX a remote device armed listens to
            // that device's microphone line or keys nothing; the Core never
            // keys from its own microphone because of it.
            if (keyer.isStation() && source == PttMode::Vox && !m_voxArmedBy.isEmpty()
                && m_radioModel && m_radioModel->remoteVoxDevice() != m_voxArmedBy) {
                return {KeyingVerdict::Refuse, TxRefusals::micNotReady()};
            }
            TransmitHolder::KeyRequest request;
            request.deviceId = keyer.deviceId;
            request.program = keyer.program;
            request.vox = source == PttMode::Vox;
            // Task 35 (ruling 8.4): VOX keys on audio, not on a person, and
            // follows the holder: while a device holds transmit (VOX is
            // disarmed at every change of holder, so it was armed for that
            // device), a VOX key is that device's.
            if (keyer.isStation() && source == PttMode::Vox
                && m_transmitHolder->state() == TransmitHolder::State::Held) {
                if (const auto holder = m_transmitHolder->holder()) {
                    // Station VOX (whole-branch review, TX path): VOX keys
                    // for the device that armed it, and only while it holds
                    // transmit. VOX armed at the Core (nobody in
                    // m_voxArmedBy) while another device holds transmit is
                    // never that device's key: refused naming the holder,
                    // as the station's own key is.
                    if (holder->deviceId != KeyerIdentity::kStationDeviceId
                        && (m_voxArmedBy.isEmpty() || holder->deviceId != m_voxArmedBy)) {
                        return {KeyingVerdict::Refuse,
                                m_transmitHolder->keyRefusalFor(
                                    QByteArray(KeyerIdentity::kStationDeviceId), false)};
                    }
                    request.deviceId = holder->deviceId;
                }
            } else if (keyer.isStation() && source == PttMode::Vox && m_radioModel
                       && !m_radioModel->remoteVoxDevice().isEmpty()) {
                // iPhone app plan Task 36: VOX is listening to a remote
                // device's microphone (it has VOX armed), so its VOX key is
                // that device's, on unheld transmit too: it becomes the
                // holder as its own key would, and VOX stays armed.
                request.deviceId = m_radioModel->remoteVoxDevice();
            }
            // iPhone app plan Task 77 (rulings 8.8, 8.9; D52 as D58 amended
            // it): a press of the radio's own PTT (its mic or a footswitch)
            // while another device holds transmit, keyed or not, away
            // included, takes it without a question, the press being the
            // confirmation. The transfer unkeys a holder on the air first;
            // the press keys nothing while it runs, and keys afterwards,
            // through this gate as a new key, only if it is still down
            // (MoxController::onTakeFinished). The hosting desktop's own MOX
            // and TUNE never take here (ruling 8.9a): they are refused
            // naming the holder, and the desktop asks through tx.take's
            // rules (takeTransmitForStation).
            // TGXL tune lane (JJ's ruling, 2026-09-30): the Tuner Genius's
            // own front-panel TUNE is a press at the station too, and takes
            // and keys exactly as the radio's PTT does (tunerPress, set only
            // for a cycle the tuner started).
            const bool radioPress =
                keyer.isStation() && (source == PttMode::Mic || keyer.tunerPress);
            if (radioPress
                && m_transmitHolder->state() == TransmitHolder::State::Held
                && !m_transmitHolder->isFenced() && !m_transmitHolder->isStopUnconfirmed()) {
                const std::optional<TransmitHolder::Holder> holder = m_transmitHolder->holder();
                if (holder && holder->deviceId != KeyerIdentity::kStationDeviceId) {
                    const QPointer<MoxController> moxLater(m_radioModel->moxController());
                    runTake(QByteArray(KeyerIdentity::kStationDeviceId),
                            TransmitHolder::Source::RadioPtt,
                            [this, moxLater, keyer](bool assigned) {
                                // A turn later, out of the call that pressed.
                                QTimer::singleShot(0, this, [moxLater, keyer, assigned]() {
                                    if (!moxLater.isNull()) {
                                        moxLater->onTakeFinished(keyer, assigned);
                                    }
                                });
                            });
                    return {KeyingVerdict::Take, {}};
                }
            }
            // The radio's own PTT (its mic or a footswitch) is PttMode::Mic
            // from the station device (ruling 8.5).
            request.source = radioPress
                                 ? TransmitHolder::Source::RadioPtt
                                 : TransmitHolder::Source::Device;
            // Slice control plan Task 11 (ruling Q8): a key that would land
            // on a slice the device took from another device and has not
            // chosen, with no other slice it may transmit on, is refused.
            // The radio's own PTT transmits where the flag is (ruling 8.11)
            // on a Core with no desktop; on a hosting desktop it keys the
            // desktop's active slice (radioPttKeyRefusal, below).
            // Asked after the holder's own refusals, so another device's
            // hold is still named first.
            // Take-over re-review (N-2): the slice of its own the flag moves
            // to, once the key is admitted.
            int moveTo = -1;
            if (request.source == TransmitHolder::Source::Device
                && m_transmitHolder->keyRefusalFor(request.deviceId, request.program).isEmpty()) {
                if (const TxRefusal taken = takenSliceKeyRefusal(request.deviceId);
                    !taken.isEmpty()) {
                    return {KeyingVerdict::Refuse, taken};
                }
                // Take-over fix wave (I-2): nor on the slice it lost, where
                // the flag stays after a take from a device that did not
                // hold transmit (the hosting desktop included). Re-review
                // (N-1): nor, for a keyer that shares slices, on any other
                // device's slice. The radio's own PTT (RadioPtt) is not
                // asked this; it has its own rule, below.
                if (const TxRefusal others = othersSliceKeyRefusal(request.deviceId, &moveTo);
                    !others.isEmpty()) {
                    return {KeyingVerdict::Refuse, others};
                }
            } else if (request.source == TransmitHolder::Source::RadioPtt
                       && m_transmitHolder->keyRefusalFor(request.deviceId, request.program)
                              .isEmpty()) {
                // TX rulings (JJ, 2026-09-30, ruling 8.11 for a hosting
                // desktop): with the flag on another device's slice, the
                // desktop's footswitch and mic PTT key the desktop's active
                // slice, even one another device controls: the flag moves
                // there once the key is admitted, in the N-2 order. With
                // the flag on one of the desktop's own slices, a non-active
                // one included (split transmit, JJ 2026-09-30), they key
                // that chosen slice. A Core with no desktop keeps 8.11 as
                // it is, transmitting where the flag is.
                if (const TxRefusal ptt = radioPttKeyRefusal(&moveTo); !ptt.isEmpty()) {
                    return {KeyingVerdict::Refuse, ptt};
                }
            }
            const quint64 epochBefore = m_transmitHolder->epoch();
            const KeyingAnswer answer = m_transmitHolder->askKey(request);
            // Take-over re-review (N-2): the flag leaves the other device's
            // slice only for a key that was admitted. A move that did not
            // land refuses the key rather than key that slice; a take it
            // made is released as an unstarted one.
            if (moveTo >= 0 && answer.verdict == KeyingVerdict::Admit) {
                TxSliceArbiter* arbiter = m_radioModel->txSliceArbiter();
                // The radio's own PTT moves the flag to a slice the desktop
                // may only listen to (ruling 8.11 for a hosting desktop), so
                // it asks without a requester, whose slices it would check.
                const bool moved = arbiter != nullptr
                    && (request.source == TransmitHolder::Source::RadioPtt
                            ? arbiter->requestHandoff(moveTo)
                            : arbiter->requestHandoff(moveTo, request.deviceId));
                if (!moved || arbiter->txBoundSliceId() != moveTo) {
                    qCWarning(lcStation) << "The transmit flag could not move to the keyer's slice;"
                                            " the key is refused";
                    if (m_transmitHolder->isTakeUnstarted()
                        && m_transmitHolder->epoch() != epochBefore) {
                        watchUnstartedTake(m_transmitHolder->epoch());
                    }
                    // TX rulings review: the radio's own PTT has its
                    // active slice; what stopped the move is the radio on
                    // the air or the flag frozen.
                    return {KeyingVerdict::Refuse,
                            request.source == TransmitHolder::Source::RadioPtt
                                ? TxRefusals::radioOnAir()
                                : TxRefusals::noTransmitSlice()};
                }
            }
            // Fix wave 2, Important 2: a take whose key never starts (a
            // TUNE or two-tone refused after the gate) is released.
            if (answer.verdict == KeyingVerdict::Admit && m_transmitHolder->isTakeUnstarted()
                && m_transmitHolder->epoch() != epochBefore) {
                watchUnstartedTake(m_transmitHolder->epoch());
            }
            return answer;
        });
        // iPhone app plan Task 77 fix wave, I1 (ruling 8.9): the radio's
        // PTT press asks the gate while another device holds transmit,
        // whatever that device's key (TUNE, two-tone, a tuner autotune,
        // VOX), or while transmit is changing hands from it.
        mox->setOtherDeviceHolds([this]() {
            const std::optional<TransmitHolder::Holder> holder = m_transmitHolder->holder();
            return holder.has_value() && holder->deviceId != KeyerIdentity::kStationDeviceId;
        });
        // Whether the holder is on the air, and MOX as the transfer reads it.
        connect(mox, &MoxController::stateChanged, this, [this, mox](MoxState state) {
            const bool on = mox->isMox() || state != MoxState::Rx;
            if (on) {
                const auto holder = m_transmitHolder->holder();
                // Task 35 (ruling 8.4): a VOX key is the holder's.
                const KeyerIdentity& keyer = mox->currentKeyer();
                const bool holdersKey = holder
                    && (keyer.deviceId == holder->deviceId
                        || (keyer.isStation() && keyer.source == PttMode::Vox));
                if (holdersKey && mox->isMox()) {
                    m_transmitHolder->setKeyed(true);
                }
            } else {
                m_transmitHolder->setKeyed(false);
            }
            m_transmitHolder->onMoxReading(on);
            // Slice control fix wave (minor): each slice's onAir reads MOX
            // itself (a key walking down, a move waiting for the unkey),
            // so it is refreshed on every MOX step, not only when the
            // holder changes.
            if (m_sliceAccessSet) {
                m_sliceAccessSet->refresh();
            }
            // Slice control fix wave (whole-branch review, Critical 1): a
            // close waiting for the unkey runs once MOX reads off.
            scheduleDeferredCloses();
            // Task 77 (ruling 8.1): the station device's take (the radio's
            // PTT, the Core's own keys, its VOX) no longer ends with its
            // key: the station holds transmit, unkeyed, until a device
            // takes it (tx.take), as any holder does. Fix wave I1's interim
            // release is gone with the take that replaces it.
        });
        // iPhone app plan Task 35 (R-IOS-13): keying from a remote device.
        // Created after the holder's MOX follower above, so the holder
        // knows it is keyed before keyedBy is published.
        m_remoteKeying = std::make_unique<RemoteKeying>(m_radioModel, m_transmitHolder.get());
        // iPhone app plan Task 77 fix round 2: the Core never switches the
        // Power Genius while a take runs, a device's key waits for its
        // microphone or its two-tone settles; a switch it owes is retried
        // when those end.
        m_radioModel->setAmpKeyPendingProbe([this]() {
            return m_transmitHolder->state() == TransmitHolder::State::Transferring
                || (m_remoteKeying && m_remoteKeying->keyPending());
        });
        connect(m_transmitHolder.get(), &TransmitHolder::changed, m_radioModel,
                &RadioModel::retryOwedAmpRestore);
        // Scoped review (addendum G-42): a hosting desktop's own window
        // waits, as every other device does, while another device holds
        // transmit before changing Extended transmit.
        m_radioModel->setOtherDeviceHoldsRefusal([this]() {
            const std::optional<TransmitHolder::Holder> holder = m_transmitHolder->holder();
            if (!holder || holder->deviceId == KeyerIdentity::kStationDeviceId) {
                return QString();
            }
            return TxRefusals::otherDeviceHolds(holder->name).text;
        });
        connect(m_transmitHolder.get(), &TransmitHolder::changed, m_radioModel,
                &RadioModel::reportTransmitHolderChanged);
        connect(m_remoteKeying.get(), &RemoteKeying::pendingKeyEnded, m_radioModel,
                &RadioModel::retryOwedAmpRestore);
        // Task 77 fix round 4: a device's tunerTune that ended without
        // keying: that device is told why (about its own request: no `by`
        // keys, no Take it back).
        connect(m_remoteKeying.get(), &RemoteKeying::tunerTuneEndedUnkeyed, this,
                [this](const QByteArray& deviceId, const QString& reason) {
                    ConfirmStep::Notice notice;
                    notice.device = deviceId;
                    notice.reason = reason;
                    notice.prompt.kind = QStringLiteral("tuneEnded");
                    tellDevice(notice, QByteArray());
                });
        // Fix wave C1: the session gate a key without a microphone line is
        // judged by first, on the connection the command came on.
        m_remoteKeying->setSessionGate([this](const RemoteKeying::Command& command) -> TxRefusal {
            const quint64 sessionId = sessionIdOfOwner(command.session);
            for (auto it = m_peers.cbegin(); it != m_peers.cend(); ++it) {
                if (it->sessionId == sessionId && it->sessionDeviceId == command.deviceId) {
                    StationTxGate sessionOnly;
                    sessionOnly.setRemoteTransmitAllowed(m_txGate.remoteTransmitAllowed());
                    const TxDecision decision = sessionOnly.decide(peerInfoFor(it.key()));
                    return decision.permitted ? TxRefusal{} : decision.refusal;
                }
            }
            return TxRefusals::notReady();
        });

        // iPhone app plan Task 37 (R-IOS-13; remote design section 12.1):
        // the transmit watchdog. It watches a device while keyedBy names it
        // or while it has VOX armed, on the Core's clock (the session
        // registry's, so the conformance player's virtual time moves it)
        // and one child timer. Its stop turns off the VOX that device armed
        // first, so VOX cannot key again after it, then stops transmitting
        // when the key on the air is that device's.
        m_txWatchdog = std::make_unique<RemoteTxWatchdog>();
        m_txWatchdogTimer = new QTimer(this);
        m_txWatchdogTimer->setSingleShot(true);
        m_txWatchdogTimer->setTimerType(Qt::PreciseTimer);
        connect(m_txWatchdogTimer, &QTimer::timeout, this, [this]() {
            if (m_txWatchdog) {
                m_txWatchdog->onTimer();
            }
        });
        {
            RemoteTxWatchdog::Hooks hooks;
            hooks.clock = [this]() { return m_deviceSessions->now(); };
            hooks.startTimer = [this](int ms) { m_txWatchdogTimer->start(ms); };
            hooks.stopTimer = [this]() { m_txWatchdogTimer->stop(); };
            hooks.stop = [this](const QByteArray& deviceId, const QString& message) {
                disarmVoxArmedBy(deviceId, "its link went quiet");
                if (m_radioModel && m_radioModel->keyedBy().deviceId == deviceId) {
                    // Task 39: the window and the phone are told why, in the
                    // words the Core stopped with, before the stop runs.
                    recordTransmitStop(TransmitState::kStopLinkLost, message);
                    m_radioModel->stopAllTx(message);
                }
            };
            hooks.deviceName = [this](const QByteArray& id) { return deviceNameForStop(id); };
            m_txWatchdog->setHooks(std::move(hooks));
        }
        // Control logging lane: a watchdog stop, beside the keepalives and
        // the control state it happened in. Logging only.
        connect(m_txWatchdog.get(), &RemoteTxWatchdog::tripped, this,
                [this](const QByteArray& deviceId, bool linkClosed, qint64 silentMs) {
                    SessionTransport* const transport = controlTransportForDevice(deviceId);
                    ControlLog::PeerInfo info;
                    info.deviceId = deviceId;
                    if (transport != nullptr) {
                        info = controlLogPeer(*m_peers.constFind(transport));
                    }
                    m_controlLog.watchdogStopped(deviceId, transport, info, linkClosed,
                                                 silentMs);
                });
        connect(m_radioModel, &RadioModel::keyedByChanged, this,
                &StationServer::followKeyedForWatchdog);

        // Task 37 (remote design section 12.3): a keyed device's microphone
        // line starving on a live link, per transmit mode.
        {
            StarvationPolicy::Hooks hooks;
            hooks.transmitMode = [this]() -> std::optional<DSPMode> {
                const SliceModel* slice = m_radioModel ? m_radioModel->txBoundSlice() : nullptr;
                if (slice == nullptr) {
                    return std::nullopt;
                }
                return slice->dspMode();
            };
            hooks.microphoneUnused = [this]() {
                if (!m_radioModel) {
                    return false;
                }
                const TwoToneController* twoTone = m_radioModel->twoToneController();
                return m_radioModel->isTune()
                    || (twoTone != nullptr
                        && (twoTone->isActive() || twoTone->isActivationInFlight()));
            };
            hooks.stopAllTx = [this](const QString& message) {
                if (m_radioModel) {
                    // Task 39: the reason, before the stop runs.
                    recordTransmitStop(TransmitState::kStopMicStarved, message);
                    m_radioModel->stopAllTx(message);
                }
            };
            hooks.deviceName = [this](const QByteArray& id) { return deviceNameForStop(id); };
            m_starvation.setHooks(std::move(hooks));
        }

        // Task 37 (carried from the desktop's transmit): the device whose
        // write turned VOX on is watched while VOX stays on, and VOX goes
        // off with its session, its link or its microphone line. VOX off
        // by anyone ends it.
        connect(&m_radioModel->transmitModel(), &TransmitModel::voxEnabledChanged, this,
                [this](bool on) {
                    if (on || m_voxArmedBy.isEmpty()) {
                        return;
                    }
                    const QByteArray was = std::exchange(m_voxArmedBy, QByteArray());
                    if (m_txWatchdog) {
                        m_txWatchdog->setVoxArmed(was, false);
                    }
                    emit voxArmedByChanged({});
                });
        connect(m_radioModel, &RadioModel::remoteMicLinesChanged, this, [this]() {
            if (!m_voxArmedBy.isEmpty() && m_radioModel
                && !m_radioModel->remoteMicLineOpen(m_voxArmedBy)) {
                // The arming device's microphone line closed: VOX would
                // otherwise listen to the Core's own microphone.
                disarmVoxArmedBy(m_voxArmedBy, "its microphone line closed");
            }
        });
    }
    // iPhone app plan Task 39 (D14, R-IOS-13): the `txState` object, on the
    // devices' clock (keyedSinceMs).
    m_transmitState = new TransmitState(this);
    m_transmitState->setClock([this]() { return m_deviceSessions->now(); });
    if (m_radioModel && m_radioModel->role() == RadioModel::Role::Local) {
        m_transmitState->bind(m_radioModel);
    }
    // iPhone app plan Task 25 (R-IOS-18): the `vax` object, over the Core's
    // own audio engine, saving a device's writes where its applet does.
    m_stationVax = new StationVax(this);
    if (m_radioModel && m_radioModel->role() == RadioModel::Role::Local) {
        m_stationVax->bind(m_radioModel, m_radioModel->localAudioDevices(), &m_settings);
    }
    // Revoking a device frees its place at once, live or away, and forgets
    // that its time ran out (ruling 4.11). Its live connection ends in the
    // next deviceRemoved handler, with no away state: the place is already
    // free.
    connect(m_devices.get(), &DeviceStore::deviceRemoved, this, [this](const QByteArray& id) {
        m_deviceSessions->remove(id);
        // iPhone app plan Task 39: a revoked device that was on the air is
        // stopped by the Core; recorded before the release unkeys it.
        const QString stopText = noteHolderStopped(id, TransmitState::kStopRevoked);
        // Unkey drain review (G-05): that stop is the Core's, so it is Stop
        // All TX, as the watchdog's and the time-out's are. The release's
        // own unkey (the unkey gate's normal unkey) is the operator's, and
        // would hold the hardware keyed for the queued transmit audio, up
        // to the send ring's length.
        if (!stopText.isEmpty() && m_radioModel) {
            m_radioModel->stopAllTx(stopText);
        }
        // iPhone app plan Task 34 (ruling 8.15): a revoked device's hold
        // on transmit is released through a transfer to nobody.
        if (m_transmitHolder) {
            m_transmitHolder->release(id, QStringLiteral("The device was removed from the Core."));
        }
    });
    // iPhone app Task 73 (ruling 4.11): revoking a device closes its slices,
    // held ones included, and forgets its saved layout. Slice control plan
    // Task 8: a slice it controlled that others listen to stays for them.
    connect(m_devices.get(), &DeviceStore::deviceRemoved, this, [this](const QByteArray& id) {
        // Slice control plan Task 8: every claim goes, as at the end of its
        // 180 s; its saved layout is forgotten below.
        releaseDeviceClaims(id, std::nullopt);
        DeviceLayoutStore::forgetDevice(AppSettings::instance(), id);
        m_slicesNotRestored.remove(id);
        m_explicitTxSlice.remove(id);
        // iPhone app Task 74 (7.4): its questions and waiting notices go.
        m_confirm->forgetDevice(id);
    });
    // iPhone app Task 73 (ruling 4.11): the end of a device's 180 s.
    connect(m_deviceSessions.get(), &DeviceSessionRegistry::graceEnded, this,
            [this](const QByteArray& deviceId, quint64 awayGeneration) {
                // Slice control plan Task 8: only for the absence that
                // ended (never a later one, never after it came back).
                if (!m_deviceSessions->isCurrentAbsence(deviceId, awayGeneration)) {
                    return;
                }
                // Fix wave 2: a slice another device took while this one
                // was away lived only in its Take it back notice; now that
                // Take it back is gone it is saved like the device's own.
                saveTakenSlicesFor(deviceId);
                releaseDeviceClaims(deviceId, awayGeneration);
            });
    // Slice control plan Task 8, Amendment 8a: the devices away within
    // their 180 s, for the preselector and the several-devices questions.
    connect(m_deviceSessions.get(), &DeviceSessionRegistry::changed, this, [this]() {
        if (m_radioModel.isNull() || m_radioModel->role() != RadioModel::Role::Local) {
            return;
        }
        QSet<QByteArray> away;
        for (const DeviceSessionRegistry::Entry& entry : m_deviceSessions->entries()) {
            if (entry.state == DeviceSessionRegistry::State::Away) {
                away.insert(entry.deviceId);
            }
        }
        m_radioModel->sliceOwnership()->setAwayDevices(away);
    });
    // iPhone app plan Task 34 (ruling 8.15): its 180 s ended.
    connect(m_deviceSessions.get(), &DeviceSessionRegistry::graceEnded, this,
            [this](const QByteArray& id, quint64 awayGeneration) {
                if (!m_deviceSessions->isCurrentAbsence(id, awayGeneration)) {
                    return;
                }
                m_transmitHolder->release(id, QStringLiteral("The device was away too long."));
            });
    // iPhone app Task 13 (R-IOS-08): the `devices` object. A device removed
    // by anything (devices.revoke, the console, a reset) loses its
    // connection at once; so does every connection signed in with the token
    // once it is retired.
    // iPhone app Task 14: the pairing window before the devices object, so
    // it follows a change of the device store first and the object then
    // counts the change once.
    // LINK-I4 fix round 1: the count of wrong codes through the service is
    // read from the Core's settings, so a restart keeps pairing shut.
    m_pairingWindow = std::make_unique<PairingWindow>(*m_devices, m_settings);
    m_pairingHasher = &SpakeExchange::storedData;
    m_devicesFacade = std::make_unique<StationDevicesFacade>(
        *m_devices, *m_tokens, *m_identity, m_settings, nullptr, m_pairingWindow.get());
    // The phone's direct addresses: the addresses a device can dial the
    // listener at, read while it listens (every 5 s, so a renumbered
    // address reaches the devices), into the devices object.
    m_coreAddresses = std::make_unique<CoreAddressWatcher>();
    connect(m_coreAddresses.get(), &CoreAddressWatcher::addressesChanged, this,
            [this](const QStringList& addresses) {
                if (m_devicesFacade) {
                    m_devicesFacade->setCoreAddresses(CoreAddresses::toJson(addresses));
                }
            });
    connect(this, &StationServer::listeningChanged, this, [this](bool listening) {
        if (!m_coreAddresses) {
            return;
        }
        if (listening) {
            m_coreAddresses->start(serverAddress(), serverPort());
        } else {
            m_coreAddresses->stop();
        }
    });
    connect(m_devices.get(), &DeviceStore::deviceRemoved, this, [this](const QByteArray& id) {
        // Remember revocation even if the same key is added again before confirm.
        for (const Peer& peer : std::as_const(m_peers)) {
            if (peer.pairing && peer.pairing->existingDevice
                && peer.pairing->existingDevice->id == id) {
                peer.pairing->existingDeviceRevoked = true;
            }
        }
        endAuthenticatedPeers([&id](const Peer& peer) { return peer.deviceId == id; },
                              QString::fromLatin1(kDeviceRemovedReason),
                              SessionEndCode::kDeviceRemoved);
    });
    // iPhone app Task 19 (R-IOS-06): the catalogue follows the Core's radio,
    // its filter presets and its band plans from here on.
    m_catalog = std::make_unique<StationCatalog>();
    // R-R3-49 / R-IOS-18: the Core's PA Gain profiles follow its bank.
    m_paProfiles = std::make_unique<PaProfilesFacade>();
    if (radioModel != nullptr && radioModel->role() != RadioModel::Role::Remote) {
        const QPointer<RadioModel> model(radioModel);
        m_paProfiles->bind(radioModel->paProfileManager(), [model]() {
            return model ? model->hardwareProfile().model : HPSDRModel::FIRST;
        });
        connect(radioModel, &RadioModel::currentRadioChanged, m_paProfiles.get(),
                [this](const NereusSDR::RadioInfo&) { m_paProfiles->refresh(); });
    }
    m_catalog->bind(radioModel);
    m_setupDescription = std::make_unique<SetupDescriptionService>();
    if (radioModel != nullptr) {
        m_setupDescription->setRadioContext(radioModel->boardCapabilities(),
                                            radioModel->hardwareProfile().model,
                                            radioModel->currentRadioInfo());
        connect(radioModel, &RadioModel::currentRadioChanged, this,
                [this](const NereusSDR::RadioInfo&) {
                    if (m_radioModel && m_setupDescription) {
                        m_setupDescription->setRadioContext(m_radioModel->boardCapabilities(),
                                                            m_radioModel->hardwareProfile().model,
                                                            m_radioModel->currentRadioInfo());
                    }
                });
        // Setup description version 20 (R-R3-49, JJ's ruling: follow
        // Thetis): on the air PA Gain publishes which rows are locked and
        // why, with the transmitting band open to the transmit holder only.
        const auto applyPaOnAir = [this](bool onAir) {
            if (m_radioModel && m_setupDescription) {
                // Version 22: and DSP > Options' RX buffer sizes (Thetis
                // setup.cs:5159 [v2.10.3.15], grpDSPBufferSize), in the
                // same revision.
                m_setupDescription->setOnAirState(
                    onAir, onAir ? m_radioModel->paOnAirBandIndex() : -1);
            }
        };
        applyPaOnAir(radioModel->isCoreOnAir());
        connect(radioModel, &RadioModel::coreOnAirChanged, this, applyPaOnAir);
        // The open row is the Core's transmit band, which holds while keyed;
        // a transmit band change while on the air (not through MOX) moves it.
        connect(radioModel, &RadioModel::transmitBandChanged, this,
                [this, applyPaOnAir]() {
                    if (m_radioModel && m_radioModel->isCoreOnAir()) {
                        applyPaOnAir(true);
                    }
                });
    }
    if (m_transmitHolder) {
        connect(m_transmitHolder.get(), &TransmitHolder::changed, this, [this]() {
            if (m_setupDescription) { m_setupDescription->noteTransmitHolderChanged(); }
        });
    }
    // Parity Task 19 (R-IOS-25): the record streams follow the Core's spots
    // and its spot sources' consoles from here on.
    setUpRecordStreams();
    // Parity Task 22 (R-R3-49): and the Core's log.
    setUpCoreLogStream();
    // R-IOS-13 / R-R3-49: and the AM Mod Monitor's two streams.
    setUpModMonitorStreams();

    connect(m_devicesFacade.get(), &StationDevicesFacade::tokenRetired, this, [this]() {
        endAuthenticatedPeers([](const Peer& peer) { return peer.signedInWithToken; },
                              QString::fromLatin1(kPairingRequiredReason),
                              SessionEndCode::kPairingRequired);
    });
    if (m_certificates->isValid()) {
        // The pin is the certificate's SHA-256 (CertificateStore), which is
        // exactly the hash a device signs and the binding covers.
        QString hex = m_certificates->fingerprintSha256();
        hex.remove(QLatin1Char(':'));
        m_certSha256 = QByteArray::fromHex(hex.toLatin1());
    }
    if (m_identity->isValid() && m_certSha256.size() == 32) {
        m_certBinding = m_identity->certBinding(m_certSha256);
        m_declaredFeatures.insert(QByteArrayLiteral("deviceAuth"), 1);
        m_declaredFeatures.insert(QByteArrayLiteral("radioMic"), 2);
        // iPhone app Task 71 (ruling 10.1): the Core admits up to four
        // devices and may ask the fifth-device question (Task 41). A
        // client uses it only with deviceAuth, so it is declared with it.
        m_declaredFeatures.insert(QByteArrayLiteral("sessionHolder"), 1);
        // iPhone app Task 14: pairing needs the identity key too (the
        // device learns it from the Core's pair.accept or box). Declared by
        // the Core only, and never asked of a client.
        if (SpakeExchange::isAvailable()) {
            m_declaredFeatures.insert(QByteArrayLiteral("pairing"), 1);
        }
    }

    // iPhone app Task 14 (R-IOS-08): the pairing window follows the device
    // store (open with no timer while unclaimed). The `devices` object
    // shows it, and the stored data for the old code is wiped. The code is
    // never printed or logged (Part C fix wave: standard output is the
    // journal on a packaged Core); `nereusd pairing show` gives it.
    connect(m_pairingWindow.get(), &PairingWindow::codeChanged, this,
            [this](const QString& code) {
                if (m_pairingStoredSerial != m_pairingWindow->codeSerial() || code.isEmpty()) {
                    SpakeExchange::wipe(m_pairingStored);
                    m_pairingStoredSerial = 0;
                }
            });

    // The first start of a Core: the TLS pin a window checks and where the
    // identity key lives, with the prompt to back it up (pairing design
    // section 3.2: losing it means every paired device pairs again).
    // Printed ONCE, on the run that creates the key. There is no secret in
    // it any more (a new Core has no token), but it still goes to stdout
    // rather than the log: see writePairingBanner() for what the logging
    // handler used to do to the fingerprint.
    if (m_identity->wasCreatedThisRun()) {
        writePairingBanner(formatFirstRunBanner(m_certificates->fingerprintSha256(),
                                                m_identity->keyPath()));
        qCInfo(lcStation)
            << "First run for this profile: the Core's identity key was created and "
               "its location and TLS certificate fingerprint were printed to stdout.";
    }
    if (!m_identity->isValid()) {
        qCWarning(lcStation) << "The Core's identity key is unavailable:"
                             << m_identity->lastError();
    }
    if (!m_tokens->isValid()) {
        qCWarning(lcStation) << "Pairing token unavailable:" << m_tokens->lastError();
    }

    m_mirror = new StateMirror(this);
    m_registry = new ObjectRegistry(radioModel, m_mirror, this);
    m_dispatcher = new SessionCommandDispatcher(radioModel, this);
    m_dispatcher->setDeviceAdmin(m_devicesFacade.get());
    // iPhone app Task 76 (ruling 9.3 item 4): the PureSignal display goes
    // to, and is charged to, the session that subscribed to it. The
    // dispatcher asks this gate; it names the asking session's media epoch
    // to the handler DaemonMediaHub installs and records the subscriber.
    m_dispatcher->setPs3DisplayAdmissionHandler([this](bool enabled, QString* refusal) {
        const auto asker = m_peers.constFind(m_dispatchingTransport);
        const quint64 epoch = asker != m_peers.cend() ? asker->mediaEpoch : 0;
        if (m_ps3DisplayAdmission && !m_ps3DisplayAdmission(epoch, enabled, refusal)) {
            return false;
        }
        const quint64 before = m_ps3SubscriberEpoch;
        m_ps3SubscriberEpoch = enabled ? epoch : 0;
        if (before != m_ps3SubscriberEpoch) {
            // The display (and its charge) moves between sessions without
            // the facade's subscription changing: split again at once.
            publishDisplayBudgetCapabilities();
        }
        return true;
    });
    m_settingsServer = new SettingsProxyServer(settings, this);
    // R-R3-46: the Core applies hardware settings for its connected radio
    // only; a write naming any other radio's MAC is refused.
    if (radioModel) {
        m_settingsServer->setConnectedMacProvider([model = QPointer<RadioModel>(radioModel)] {
            return model ? model->currentRadioMac() : QString();
        });
    }

    // Outbound: iPhone app Task 72 (ruling 5.6). Each admitted session has
    // its own MirrorView (promoteToSession), which sends the mirror's
    // messages to that session alone, fitted by sendToPeer; nothing goes
    // to a connection that holds no view.
    connect(radioModel, &RadioModel::receiveLayoutHydrated, this, [this] {
        // iPhone app Task 73 (ruling 5.2): a layout restored while devices
        // are on the Core (a radio that arrived late) brings its slices
        // with the owners its manifest names, each held for its device.
        // A device holding a place takes its own back, and a device alone
        // on the Core adopts the slices nobody owns, as at its admission.
        // The bursts below carry the result, so no view is sent the
        // change of owner on its own.
        if (m_radioModel && m_radioModel->role() == RadioModel::Role::Local) {
            m_ownerChangesInBurst = true;
            SliceOwnership* ownership = m_radioModel->sliceOwnership();
            const QList<DeviceSessionRegistry::Entry> entries = m_deviceSessions->entries();
            for (const DeviceSessionRegistry::Entry& entry : entries) {
                ownership->returnHeld(entry.deviceId);
            }
            adoptForLoneDevice();
            m_ownerChangesInBurst = false;
            m_connectedDevices->refresh();
        }
        if (hasAuthenticatedSession() && m_mirrorBuilt) {
            // Shared slice QObjects were restored without individual notify
            // signals. Re-seed their entire settled state on every session,
            // each view its own burst.
            for (const QPointer<MirrorView>& view : attachedViews()) {
                if (!view.isNull()) {
                    view->attach();
                }
            }
        }
    });
    // Parity Task 19 (R-IOS-25): a subscription belongs to the connection
    // that asked, so the Core answers it (and sends the backlog) itself.
    m_dispatcher->setRecordAccess([this](const SessionMessage& invoke) {
        if (m_dispatchingTransport == nullptr) {
            return false;
        }
        m_invokeFrame->resultSent = true;
        m_invokeFrame->terminalResultSent = true;
        // R-IOS-13 / R-R3-49: the Mod Monitor's reset is the Core's too.
        if (invoke.commandVerb == "txModMonitor.reset") {
            handleModMonitorReset(m_dispatchingTransport, invoke);
        } else {
            handleRecordsCommand(m_dispatchingTransport, invoke);
        }
        return true;
    });
    // R-IOS-13 / R-R3-49 (txEqCurveVersion 2): the TX EQ curve verbs are
    // the asking connection's txEqParaEqData write.
    m_dispatcher->setTxEqCurveAccess([this](const SessionMessage& invoke) {
        if (m_dispatchingTransport == nullptr) {
            return false;
        }
        m_invokeFrame->resultSent = true;
        m_invokeFrame->terminalResultSent = true;
        handleTxEqCurveCommand(m_dispatchingTransport, invoke);
        return true;
    });
    // transmitSettingsVersion 15: cfc.setProfile is the asking
    // connection's cfcParaEqData write.
    m_dispatcher->setCfcProfileAccess([this](const SessionMessage& invoke) {
        if (m_dispatchingTransport == nullptr) {
            return false;
        }
        m_invokeFrame->resultSent = true;
        m_invokeFrame->terminalResultSent = true;
        handleCfcProfileCommand(m_dispatchingTransport, invoke);
        return true;
    });
    // Parity Task 22 / the iPhone app plan's Task 25 (R-R3-49, R-IOS-18):
    // what the Core's support bundle is made from, read here on the main
    // thread (the bundle itself is written on a worker thread).
    m_dispatcher->setSupportInputs([this]() {
        SupportBundle::Inputs inputs = SupportBundle::gatherInputs(m_radioModel.data());
        inputs.daemonConfigPath = m_supportConfigPath;
        if (m_tokens != nullptr && !m_tokens->token().isEmpty()) {
            inputs.knownSecrets.append(m_tokens->token());
        }
        if (m_pairingWindow != nullptr && !m_pairingWindow->currentCode().isEmpty()) {
            inputs.knownSecrets.append(m_pairingWindow->currentCode());
        }
        if (!m_lastTelemetry.isEmpty()) {
            inputs.telemetry = QJsonObject{
                {QStringLiteral("ageMs"),
                 static_cast<double>(QDateTime::currentMSecsSinceEpoch() - m_lastTelemetryAtMs)},
                {QStringLiteral("snapshot"), m_lastTelemetry}};
        }
        return inputs;
    });
    // iPhone app Task 74 (R-IOS-30): confirm.proceed, confirm.cancel and
    // notice.takeBack are answered by the confirm step.
    m_dispatcher->setConfirmAnswer([this](const SessionMessage& invoke, int id, int choice) {
        return answerConfirm(invoke, id, choice);
    });
    // iPhone app Task 71: a command's result goes to the session that asked:
    // the one being dispatched now, or, for a result that arrives on a later
    // turn, the one its verb and id were recorded for. The media session
    // keeps any other (as the one session did before).
    //
    // Fix wave I1: every client counts its command ids from 1, so a result
    // is named by the session it answers (the dispatcher's resultOwner())
    // with its verb and id, never by verb and id alone. A result nobody's
    // route names is dropped, not handed to another session.
    connect(m_dispatcher, &SessionCommandDispatcher::commandResultReady, this,
            [this](const SessionMessage& original) {
                const QPointer<StationServer> self(this);
                const ResultKey key = resultKeyOf(original);
                // iPhone app Task 75: a proceed whose held command answers
                // later is answered with that command's result.
                if (m_proceedAnsweredLater && *m_proceedAnsweredLater == key) {
                    m_proceedAnsweredLater.reset();
                    return;
                }
                if (finishDeferredProceed(key, original)) {
                    return;
                }
                if (!self) { return; }
                // iPhone app Task 74: the confirm step reads (and may keep,
                // or reword) a result of a change it is running.
                SessionMessage result = original;
                if (m_resultHook && !m_resultHook(result)) {
                    return;
                }
                if (!self) { return; }
                // Follow-up N3: an accepted radio change answers on the
                // restart turn (finishRadioChange).
                if (m_holdingRadioChange && !m_heldRadioChange && result.accepted
                    && result.commandVerb == "station.selectRadio") {
                    m_heldRadioChange = HeldRadioChange{key, result, false, {}};
                    // Ruling 7.1a: a radio change applied at once tells
                    // its devices when it answers.
                    if (m_appliedNowRadio) {
                        m_heldRadioChange->later = *m_appliedNowRadio;
                        m_heldRadioChange->tellOnFinish = true;
                        m_appliedNowRadio.reset();
                    }
                    return;
                }
                QPointer<SessionTransport> to;
                for (InvokeFrame* frame = m_invokeFrame; frame != nullptr; frame = frame->parent) {
                    if (frame->key == key && !frame->terminalResultSent && frame->transport
                        && hasReplySession(frame->transport, key.sessionId)) {
                        to = frame->transport;
                        frame->resultSent = true;
                        frame->terminalResultSent = isLastResult(result);
                        break;
                    }
                }
                if (!to) {
                    const auto route = m_resultRoutes.find(key);
                    if (route != m_resultRoutes.end()) {
                        if (*route && hasReplySession(*route, key.sessionId)) {
                            to = *route;
                        }
                        if (isLastResult(result) && !answersEveryKeyingCopy(key.verb)) {
                            m_resultRoutes.erase(route);
                        }
                    }
                }
                if (answersEveryKeyingCopy(key.verb)) {
                    // Spend before send: delivery can receive another copy or
                    // end the session. No iterator survives that callout.
                    consumeKeyingReply(key);
                } else if (key.verb.startsWith("ps3.")) {
                    // A duplicate's plain refusal answers only that invoke;
                    // the original operation's terminal phase retires its route.
                    for (const MirrorUpdate& update : result.updates) {
                        if (update.name == "phase"
                            && (update.value.toString() == QLatin1String("completed")
                                || update.value.toString() == QLatin1String("failed"))) {
                            m_resultRoutes.remove(key);
                            break;
                        }
                    }
                }
                if (to && hasReplySession(to, key.sessionId)) {
                    sendToPeer(to, result);
                } else {
                    qCDebug(lcStation) << "No session asked for" << result.commandVerb
                                       << result.commandId << "; dropped";
                }
            });
    // iPhone app Task 71 (ruling 4.12): session.leave was accepted for the
    // connection being dispatched; it ends once its result has gone out.
    connect(m_dispatcher, &SessionCommandDispatcher::sessionLeaveRequested, this, [this]() {
        if (m_dispatchingTransport != nullptr) {
            const auto peer = m_peers.find(m_dispatchingTransport);
            if (peer != m_peers.end()) {
                peer->leaving = true;
            }
        }
    });
    connect(m_settingsServer, &SettingsProxyServer::outboundValueChanged, this,
            [this](const QString& key, const QVariant& value, const QString& originTag) {
                // Ruling 5.8: to every view holding the key (every session
                // holds every Station key it was sent in its snapshot), the
                // writer's origin kept so the writer knows its own echo.
                sendToEveryView(
                    SessionMessages::settingsValue(key, value.toString(), originTag));
            });
    // R-R3-49: the Network Watchdog is a radio setting, applied where the
    // radio is. A window's change lands in the Core's settings above; the
    // Core's radio takes it here, whichever path stored it.
    connect(m_settingsServer, &SettingsProxyServer::outboundValueChanged, this,
            [this](const QString& key, const QVariant& value, const QString&) {
                if (key == QLatin1String("NetworkWatchdogEnabled") && m_radioModel) {
                    m_radioModel->applyNetworkWatchdog(value.toString() == QLatin1String("True"));
                }
            });
    // Task 13: External TX Inhibit gates the Core's own keying, so a
    // window's change reaches the Core's TxInhibitMonitor, whichever path
    // stored it. Thetis setup.cs:16660-16667 [v2.10.3.15] applies each box
    // at once.
    connect(m_settingsServer, &SettingsProxyServer::outboundValueChanged, this,
            [this](const QString& key, const QVariant& value, const QString&) {
                if (!m_radioModel) {
                    return;
                }
                const bool on = value.toString() == QLatin1String("True");
                if (key == QLatin1String("TxInhibitMonitorEnabled")) {
                    m_radioModel->txInhibit().setEnabled(on);
                } else if (key == QLatin1String("TxInhibitMonitorReversed")) {
                    m_radioModel->txInhibit().setReverseLogic(on);
                } else if (key == QLatin1String("RxOnly")) {
                    // Task 16: Receive Only gates the Core's keying, as
                    // console.RXOnly does at once (setup.cs:6498
                    // [v2.10.3.15]: console.RXOnly = chkGeneralRXOnly.Checked).
                    m_radioModel->applyRxOnlySetting(on);
                } else if (key == QLatin1String(RadioModel::kDisableHfPaKey)) {
                    // transmitSettingsVersion 11: Disable HF PA reaches the
                    // Core's radio and SWR protection at once, as Thetis's
                    // console.HFTRRelay does (setup.cs:16750-16754
                    // [v2.10.3.15]).
                    m_radioModel->applyDisableHfPaSetting(value);
                }
            });
    // Whole-branch review, Important 4. A removal has its own signal and
    // its own frame. It used to arrive here as an outboundValueChanged
    // carrying an INVALID QVariant, and the value.toString() above turned
    // that into "" -- so every client cached an empty string for a key
    // the station no longer had, and a client that had just correctly
    // removed the key itself had it resurrected by the echo.
    connect(m_settingsServer, &SettingsProxyServer::outboundValueRemoved, this,
            [this](const QString& key) {
                sendToEveryView(SessionMessages::settingsValueAbsent(key, QString()));
            });
    // R-R3-49: a removal (a settings reset on the Core while it runs)
    // leaves the settings reading the default, so the radio takes the
    // default too rather than keep the last value it was given. The schema
    // v7 reset is not seen here: it runs in CoreInit before this server
    // exists, and the radio reads the reset value when it connects.
    // iPhone app Task 13: the devices object's label follows StationCallsign
    // until a rename, and its key backup follows its setting.
    connect(m_settingsServer, &SettingsProxyServer::outboundValueChanged, this,
            [this](const QString& key, const QVariant&, const QString&) {
                if (isDevicesSettingsKey(key)) {
                    m_devicesFacade->refresh();
                }
                // iPhone app Task 19: a preset or the CW pitch changed,
                // from this computer or a window. Coalesced, so the three
                // settings of one preset move the revision once.
                if (isCatalogSettingsKey(key)) {
                    m_catalog->scheduleRefresh();
                }
            });
    connect(m_settingsServer, &SettingsProxyServer::outboundValueRemoved, this,
            [this](const QString& key) {
                if (isDevicesSettingsKey(key)) {
                    m_devicesFacade->refresh();
                }
                if (isCatalogSettingsKey(key)) {
                    m_catalog->scheduleRefresh();
                }
            });
    connect(m_settingsServer, &SettingsProxyServer::outboundValueRemoved, this,
            [this](const QString& key) {
                if (key == QLatin1String("NetworkWatchdogEnabled") && m_radioModel) {
                    m_radioModel->applyNetworkWatchdog(RadioModel::kNetworkWatchdogDefault);
                }
                // Task 13: both External TX Inhibit boxes default off
                // (console.cs:15336-15337 [v2.10.3.15]).
                if (key == QLatin1String("TxInhibitMonitorEnabled") && m_radioModel) {
                    m_radioModel->txInhibit().setEnabled(false);
                } else if (key == QLatin1String("TxInhibitMonitorReversed") && m_radioModel) {
                    m_radioModel->txInhibit().setReverseLogic(false);
                } else if (key == QLatin1String("RxOnly") && m_radioModel) {
                    // Task 16: Receive Only defaults off.
                    m_radioModel->applyRxOnlySetting(false);
                } else if (key == QLatin1String(RadioModel::kDisableHfPaKey) && m_radioModel) {
                    // Disable HF PA defaults off (console.cs:10891
                    // [v2.10.3.15] hf_tr_relay = false).
                    m_radioModel->applyDisableHfPaSetting(QVariant());
                }
            });

    // A daemon can authenticate a GUI before its configured radio is
    // discoverable.  currentRadioChanged is emitted only after RadioModel's
    // Connected handlers have populated the live identity and profile, but
    // defer the wire update one event turn so every other observer of that
    // signal has finished too.  Capture the current session now: a later
    // authenticated replacement already received its own initial snapshot,
    // and an old callback must never refresh it.
    if (m_radioModel) {
        // Withdrawing the optional row edit offer is meaningful while a
        // session stays up across a radio disconnect. A later connected
        // radio is refreshed by currentRadioChanged below.
        connect(m_radioModel, &RadioModel::connectionStateChanged, this,
                [this](ConnectionState state) {
                    if (state == ConnectionState::Connected) {
                        return;
                    }
                    // sendText can synchronously close a peer (or replace
                    // its device's session). Copy identities before any
                    // send, then re-find each original session. Neither a
                    // QHash iterator nor an old disconnect may address a
                    // replacement that joined inside a send callback.
                    struct Target {
                        QPointer<SessionTransport> transport;
                        quint64 sessionId = 0;
                    };
                    QList<Target> targets;
                    for (auto it = m_peers.cbegin(); it != m_peers.cend(); ++it) {
                        if (it->authenticated && it->snapshotComplete
                            && it->sessionId != 0
                            && it->agreedMinor >= kRadioIdentitySessionProtocolMinor
                            && it->features.value(QByteArrayLiteral("radioAntennaRows")) == 1) {
                            targets.append({QPointer<SessionTransport>(it.key()), it->sessionId});
                        }
                    }
                    const QPointer<StationServer> self(this);
                    for (const Target& target : std::as_const(targets)) {
                        if (!self || m_radioModel.isNull()
                            || m_radioModel->connectionState() == ConnectionState::Connected) {
                            return;
                        }
                        SessionTransport* const transport = target.transport.data();
                        if (transport == nullptr) {
                            continue;
                        }
                        bool stillAdmitted = false;
                        {
                            const auto peer = m_peers.constFind(transport);
                            stillAdmitted = peer != m_peers.cend()
                                && peer->sessionId == target.sessionId
                                && peer->authenticated && peer->snapshotComplete
                                && !peer->dropping
                                && peer->agreedMinor >= kRadioIdentitySessionProtocolMinor
                                && peer->features.value(QByteArrayLiteral("radioAntennaRows")) == 1;
                        }
                        if (!stillAdmitted) {
                            continue;
                        }
                        send(transport, SessionMessages::capabilities(
                            buildCapabilitiesFor(transport).toUpdates()));
                        if (!self) {
                            return;
                        }
                    }
                });
        // iPhone app Task 71: every admitted session, each with its own
        // capabilities. A session that ended (its transport gone) or a
        // connection no longer admitted is skipped.
        connect(m_radioModel, &RadioModel::currentRadioChanged, this,
                [this](const NereusSDR::RadioInfo&) {
                    QList<QPointer<SessionTransport>> sessions;
                    for (const Peer& peer : std::as_const(m_peers)) {
                        if (!peer.sessionDeviceId.isEmpty()) {
                            sessions.append(QPointer<SessionTransport>(peer.transport));
                        }
                    }
                    if (sessions.isEmpty()) {
                        return;
                    }
                    QTimer::singleShot(0, this, [this, sessions]() {
                        for (const QPointer<SessionTransport>& session : sessions) {
                            if (!session.isNull()) {
                                sendCapabilitiesAndSettingsSnapshot(session.data());
                            }
                        }
                    });
                });
        // iPhone app Task 75 (ruling 5.11a): the receive antenna stayed put
        // on a band crossing; the person tuning is told.
        connect(m_radioModel, &RadioModel::receiveAntennaKept, this,
                [this](int sliceId, const QString& antenna, const QList<QByteArray>& listeners) {
                    onReceiveAntennaKept(sliceId, antenna, listeners);
                });
    }

    // ObjectRegistry's create/destroy events are the lifecycle half of the
    // mirror; StateMirror only carries property deltas for objects it
    // already knows about.
    connect(m_registry, &ObjectRegistry::objectCreated, this,
            [this](const QByteArray& objectKey, const QByteArray& className, int,
                   const QList<MirrorUpdate>& snapshot) {
                sendToEveryView(
                    SessionMessages::objectCreate(objectKey, className, snapshot));
            });
    connect(m_registry, &ObjectRegistry::objectDestroyed, this,
            [this](const QByteArray& objectKey, const QByteArray& className, int) {
                sendToEveryView(SessionMessages::objectDestroy(objectKey, className));
            });

    // iPhone app Task 73 (ruling 5.4): a marker per slice, made after
    // ObjectRegistry's connections so a slice's create and destroy go out
    // before its marker's. Its owner fields name the device the slice is
    // held for when held, else its owner, one device named the same way on
    // every page (ruling 4.3).
    m_markers = new SliceMarkerSet(radioModel, m_mirror, [this](int sliceId) {
        SliceMarker::Owner owner;
        if (!m_radioModel) {
            return owner;
        }
        const SliceOwnership::Mark mark = m_radioModel->sliceOwnership()->mark(sliceId);
        const QByteArray subject = mark.subject();
        if (subject.isEmpty() || subject == SliceOwnership::stationDevice()) {
            owner.kind = QStringLiteral("station");
            return owner;
        }
        if (const auto words = m_connectedDevices->describe(subject)) {
            owner.deviceId = words->wireId;
            owner.name = words->name;
            owner.shortName = words->shortName;
            owner.kind = words->kind;
        } else {
            owner.deviceId = StationIdentity::toBase64Url(subject);
        }
        const std::optional<DeviceSessionRegistry::Entry> entry = m_deviceSessions->entry(subject);
        owner.away = mark.isHeld()
            || (entry && entry->state == DeviceSessionRegistry::State::Away);
        return owner;
    }, this);
    connect(m_markers, &SliceMarkerSet::markerCreated, this,
            [this](const QByteArray& key, const QByteArray& className,
                   const QList<MirrorUpdate>& snapshot) {
                sendToEveryView(SessionMessages::objectCreate(key, className, snapshot));
            });
    connect(m_markers, &SliceMarkerSet::markerDestroyed, this,
            [this](const QByteArray& key, const QByteArray& className) {
                sendToEveryView(SessionMessages::objectDestroy(key, className));
            });
    // Away, back, a name or a short name changed: the markers say so.
    connect(m_deviceSessions.get(), &DeviceSessionRegistry::changed, m_markers,
            &SliceMarkerSet::refreshOwners);
    connect(m_devices.get(), &DeviceStore::devicesChanged, m_markers,
            &SliceMarkerSet::refreshOwners);
    // Slice control plan Task 4: who controls and who listens to each
    // slice, one SliceAccess object per slice, made after the markers so a
    // slice's object.create and its marker's go out before it. Only a view
    // that shares slices receives them (ownershipAllows).
    m_sliceAccessSet = new SliceAccessSet(radioModel, m_mirror, [this](int sliceId) {
        SliceAccess::Fields fields;
        if (!m_radioModel) {
            return fields;
        }
        const SliceOwnership* ownership = m_radioModel->sliceOwnership();
        const QByteArray controller = ownership->mark(sliceId).owner;
        fields.controllerDeviceId = controller.isEmpty() ? QString() : wireIdOf(controller);
        fields.controlRevision = static_cast<qint64>(ownership->controlRevision(sliceId));
        QJsonArray listeners;
        QJsonArray receiving;
        for (const QByteArray& device : ownership->listenersOf(sliceId)) {
            listeners.append(wireIdOf(device));
            if (ownership->activeRxFor(device) == sliceId) {
                receiving.append(wireIdOf(device));
            }
        }
        fields.listenerDeviceIds =
            QString::fromUtf8(QJsonDocument(listeners).toJson(QJsonDocument::Compact));
        fields.activeRxDeviceIds =
            QString::fromUtf8(QJsonDocument(receiving).toJson(QJsonDocument::Compact));
        const SliceModel* slice = m_radioModel->sliceById(sliceId);
        fields.txSelected = slice != nullptr && slice->txSliceMarked();
        fields.onAir = sliceTransmitting(sliceId);
        return fields;
    }, this);
    connect(m_sliceAccessSet, &SliceAccessSet::accessCreated, this,
            [this](const QByteArray& key, const QByteArray& className,
                   const QList<MirrorUpdate>& snapshot) {
                sendToEveryView(SessionMessages::objectCreate(key, className, snapshot));
            });
    connect(m_sliceAccessSet, &SliceAccessSet::accessDestroyed, this,
            [this](const QByteArray& key, const QByteArray& className) {
                sendToEveryView(SessionMessages::objectDestroy(key, className));
            });
    if (radioModel) {
        // Slice control plan Task 4: every view's forms of a slice as they
        // were last decided, kept from its creation to its removal.
        const auto noteForms = [this](int sliceId) {
            if (m_radioModel) {
                const SliceOwnership* ownership = m_radioModel->sliceOwnership();
                m_sliceForms.insert(sliceId, SliceFormState{ownership->mark(sliceId),
                                                            ownership->listenersOf(sliceId)});
            }
        };
        for (SliceModel* slice : radioModel->slices()) {
            if (slice != nullptr) {
                noteForms(slice->sliceIndex());
            }
        }
        connect(radioModel, &RadioModel::sliceAdded, this, noteForms);
        connect(radioModel, &RadioModel::sliceRemoved, this, [this](int sliceId) {
            m_sliceForms.remove(sliceId);
            for (auto it = m_takenNotChosenForTx.begin(); it != m_takenNotChosenForTx.end(); ++it) {
                it->remove(sliceId);
            }
            // Take-over fix wave (I-2): a closed slice is nobody's lost one.
            for (auto it = m_lostTxSlice.begin(); it != m_lostTxSlice.end(); ++it) {
                it->remove(sliceId);
            }
            // Fix wave (Important 4): a closed slice is nobody's choice.
            for (auto it = m_explicitTxSlice.begin(); it != m_explicitTxSlice.end();) {
                it = it.value() == sliceId ? m_explicitTxSlice.erase(it) : std::next(it);
            }
        });
        // Fix wave (Important 4): the hosting desktop's own selection is
        // the station device's explicit choice.
        connect(radioModel, &RadioModel::txSliceSelected, this, [this](int sliceId) {
            m_explicitTxSlice.insert(SliceOwnership::stationDevice(), sliceId);
            // Task 11: a slice it took is its transmit slice once chosen.
            m_takenNotChosenForTx[SliceOwnership::stationDevice()].remove(sliceId);
        });
        // The checks and the change behind slice.listen, slice.stopListening,
        // slice.takeControl and slice.release (and, in Task 10, the hosting
        // desktop's own window).
        SliceAccessController::Hooks hooks;
        hooks.transmitting = [this](int sliceId) { return sliceTransmitting(sliceId); };
        hooks.cannotHandOff = [this](const QByteArray& controller, const QByteArray& taker,
                                     int sliceId) {
            return handOffRefusal(controller, taker, sliceId);
        };
        hooks.staysListening = [this](const QByteArray& former) {
            return staysListeningAfterTake(former);
        };
        hooks.clearTransmitSelection = [this](const QByteArray& former, int sliceId) {
            clearTransmitSelection(former, sliceId);
        };
        hooks.tookControl = [this](const QByteArray& taker, int sliceId) {
            m_takenNotChosenForTx[taker].insert(sliceId);
        };
        hooks.close = [this](int sliceId) { return closeSliceNobodyIsOn(sliceId); };
        m_sliceAccessController = new SliceAccessController(radioModel, std::move(hooks), this);
        connect(m_sliceAccessController, &SliceAccessController::controlTaken, this,
                &StationServer::tellControlTaken);
        m_dispatcher->setSliceAccessController(m_sliceAccessController);
    }
    // connectedDevices.listeningOn: a device's slices, their bands and
    // modes.
    m_connectedDevices->setListeningProvider(
        [this](const QByteArray& deviceId) { return listeningOn(deviceId); });
    m_dispatcher->setSliceAccess([this](const QByteArray& requester, int sliceId) {
        return changeRefusal(requester, sliceId);
    });
    // iPhone app plan Task 34: the on-air refusals (ruling 7.4) and the
    // holder's tx.setTxSlice (ruling 8.10).
    {
        SessionCommandDispatcher::TransmitAccess access;
        access.onAir = [this](const QByteArray& requester) { return onAirRefusal(requester); };
        access.micSource = [this](const SessionMessage& invoke, const QString& owner,
                                  const QByteArray& requester, std::optional<RemoteMicSource> source) {
            const auto peer = m_peers.constFind(m_dispatchingTransport);
            if (peer == m_peers.cend() || !peer->authenticated || !peer->snapshotComplete
                || peer->sessionDeviceId != requester || sessionOwner(peer->sessionId) != owner
                || requester.isEmpty() || !m_radioModel) {
                return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId,
                                                       false, TxRefusals::notReady().text, {});
            }
            const QByteArray request = SessionMessages::encode(invoke);
            const auto cache = m_micSourceReplies.value(owner);
            const auto prior = cache.constFind(invoke.commandId);
            if (prior != cache.cend()) {
                return prior->first == request ? prior->second
                    : SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                        QStringLiteral("The Core could not read this request."), {});
            }
            if (!source) {
                // Malformed negotiated requests reserve their exact session
                // id too, before returning an answer or disclosing source state.
                const auto result = SessionMessages::commandResult(invoke.commandVerb, invoke.commandId,
                    false, QStringLiteral("The Core could not read this request."), {});
                m_micSourceReplies[owner].insert(invoke.commandId, {request, result});
                return result;
            }
            const RemoteMicSource requested = *source;
            QString reason;
            TxRefusal refusal;
            const auto current = m_radioModel->remoteMicSelection(owner, requester);
            const auto decision = txDecisionFor(m_dispatchingTransport);
            if (!decision.permitted) {
                refusal = decision.refusal;
            } else if (m_transmitHolder) {
                refusal = m_transmitHolder->keyRefusalFor(requester);
            }
            if (!refusal.isEmpty()) {
                reason = refusal.text;
            } else if (m_radioModel->moxController()->isMox()
                       || (m_remoteKeying && m_remoteKeying->keyPending())) {
                reason = QStringLiteral("Release transmit before changing the microphone source.");
            } else if (requested == RemoteMicSource::RadioMic
                       && !m_radioModel->boardCapabilities().radioMicSelectable()) {
                reason = QStringLiteral("This radio does not offer a radio microphone input.");
            }
            bool accepted = reason.isEmpty();
            if (accepted) {
                const QPointer<StationServer> self(this);
                const QPointer<SessionTransport> transport(m_dispatchingTransport);
                const auto stillCurrent = [self, transport, owner, requester]() {
                    if (!self || !transport || !self->m_radioModel) { return false; }
                    const auto current = self->m_peers.constFind(transport);
                    return current != self->m_peers.cend() && current->authenticated
                        && current->snapshotComplete && current->sessionDeviceId == requester
                        && sessionOwner(current->sessionId) == owner;
                };
                if (requested == RemoteMicSource::RadioMic && m_voxArmedBy == requester) {
                    disarmVoxArmedBy(requester, "radio microphone selected");
                }
                if (!stillCurrent()) {
                    return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId,
                                                           false, TxRefusals::notReady().text, {});
                }
                // Disarming VOX notifies synchronous observers. They can key,
                // change the holder, or change station policy before we commit.
                const auto now = txDecisionFor(transport);
                refusal = !now.permitted ? now.refusal
                    : m_transmitHolder ? m_transmitHolder->keyRefusalFor(requester) : TxRefusal{};
                if (!refusal.isEmpty()) {
                    reason = refusal.text;
                } else if (m_radioModel->moxController()->isMox()
                           || (m_remoteKeying && m_remoteKeying->keyPending())) {
                    reason = QStringLiteral("Release transmit before changing the microphone source.");
                } else if (requested == RemoteMicSource::RadioMic
                           && !m_radioModel->boardCapabilities().radioMicSelectable()) {
                    reason = QStringLiteral("This radio does not offer a radio microphone input.");
                }
                accepted = reason.isEmpty();
                if (accepted) { m_radioModel->setRemoteMicSelection(owner, requester, requested); }
            }
            QList<MirrorUpdate> values{{0, "source", MirrorWireKind::Utf8,
                remoteMicSourceName(accepted ? requested : current)}};
            if (!refusal.isEmpty()) {
                values.append({0, "refusalCode", MirrorWireKind::Utf8, QString::fromUtf8(refusal.code)});
                values.append({0, "refusalFix", MirrorWireKind::Utf8, QString::fromUtf8(refusal.fix)});
            }
            const auto result = SessionMessages::commandResult(invoke.commandVerb, invoke.commandId,
                                                                accepted, reason, {}, values);
            m_micSourceReplies[owner].insert(invoke.commandId, {request, result});
            return result;
        };
        // Fix wave M2 (ruling 8.5): a release is the holder's.
        access.release = [this](const QByteArray& requester) -> TxRefusal {
            const std::optional<TransmitHolder::Holder> holder = m_transmitHolder->holder();
            if (!holder || requester.isEmpty() || holder->deviceId == requester) {
                return {};
            }
            return TxRefusals::otherDeviceHoldsStop(holder->name);
        };
        access.txSlice = [this](const QByteArray& requester) -> TxRefusal {
            // The session's own gate first (remote_transmit, pairing, ...).
            for (auto it = m_peers.cbegin(); it != m_peers.cend(); ++it) {
                if (it->sessionDeviceId == requester) {
                    const TxDecision decision = txDecisionFor(it.key());
                    if (!decision.permitted) {
                        return decision.refusal;
                    }
                    break;
                }
            }
            // The holder's verb. With transmit unheld the design is silent:
            // refused, and the device is asked to take transmit first.
            if (m_transmitHolder->isHeldBy(requester)) {
                return {};
            }
            const TxRefusal refusal = m_transmitHolder->keyRefusalFor(requester);
            return refusal.isEmpty() ? TxRefusals::notHolder() : refusal;
        };
        // Slice control fix wave (Important 4): only tx.setTxSlice writes
        // a device's explicit transmit choice.
        access.txSliceChosen = [this](const QByteArray& requester, int sliceId) {
            m_explicitTxSlice.insert(requester, sliceId);
            // Slice control plan Task 11 (ruling Q8): a slice it took is its
            // transmit slice once it chooses it here, never by a binding
            // it got by itself.
            m_takenNotChosenForTx[requester].remove(sliceId);
        };
        // Task 35: tx.key, tx.unkey, tx.tune and tx.twoTone.
        access.keying = [this](const RemoteKeying::Command& command, RemoteKeying::Reply reply) {
            if (!m_remoteKeying) {
                RemoteKeying::Result result;
                result.reason = QStringLiteral("The Core has no radio ready.");
                reply(result);
                return;
            }
            // Task 36: a key waiting for its microphone buffer answers later.
            m_remoteKeying->handle(command, std::move(reply));
        };
        // Task 37: tx.keepalive on the session's own link.
        access.keepalive = [this](const QByteArray& requester, quint64 sequence, quint32 epoch) {
            if (m_txWatchdog) {
                // Control logging lane: logged after, as it was watched,
                // with its wait since the control link received it.
                const bool watched = m_txWatchdog->isWatching(requester);
                const std::optional<qint64> waitUs = m_dispatchingTransport != nullptr
                    ? m_dispatchingTransport->deliveringMessageWaitUs()
                    : std::nullopt;
                m_txWatchdog->keepalive(requester, sequence, epoch,
                                        RemoteTxWatchdog::Path::Session);
                controlLogKeepalive(requester, ControlLog::KeepaliveChannel::Control,
                                    waitUs.value_or(0) / 1000, watched);
            }
        };
        // Task 77 (ruling 7.7): the transmitter's own settings are the
        // holder's while transmit is held.
        // On the air, ruling 7.4's words come first. A receive-only Core
        // has no device holding transmit to defer to: its transmit settings
        // stay the windows' to change off the air (R-R3-49).
        access.transmitter = [this](const QByteArray& requester) -> TxRefusal {
            if (const TxRefusal onAir = onAirRefusal(requester); !onAir.isEmpty()) {
                return onAir;
            }
            const std::optional<TransmitHolder::Holder> holder = m_transmitHolder->holder();
            if (!m_txGate.remoteTransmitAllowed() || !holder || requester.isEmpty()
                || holder->deviceId == requester) {
                return {};
            }
            return TxRefusals::otherDeviceHolds(holder->name);
        };
        // R-R3-49 / R-IOS-27: the PA profile verbs on the air.
        access.holdsTransmit = [this](const QByteArray& requester) {
            return !requester.isEmpty() && m_transmitHolder->isHeldBy(requester);
        };
        access.accessory = [this](const QByteArray& requester) -> TxRefusal {
            SessionTransport* const transport = m_dispatchingTransport;
            if (transport == nullptr || peerInfoFor(transport).deviceId != requester) {
                return TxRefusals::notReady();
            }
            StationTxGate sessionOnly;
            sessionOnly.setRemoteTransmitAllowed(m_txGate.remoteTransmitAllowed());
            return sessionOnly.decide(peerInfoFor(transport)).refusal;
        };
        // Task 77: tx.take, on the connection being dispatched.
        access.take = [this](const SessionMessage& invoke, std::optional<quint64> holderEpoch,
                             std::optional<bool> shownKeyed,
                             std::function<void(const SessionMessage&)> reply) {
            takeTransmit(m_dispatchingTransport, invoke, holderEpoch, shownKeyed,
                         std::move(reply));
        };
        m_dispatcher->setTransmitAccess(std::move(access));
    }
    if (radioModel) {
        connect(radioModel->sliceOwnership(), &SliceOwnership::markChanged, this,
                &StationServer::onSliceOwnerChanged);
        // Slice control plan Task 4: a join or a leave (stop listening, the
        // listener half of a claims removal) swaps that view's form too.
        connect(radioModel->sliceOwnership(), &SliceOwnership::listenersChanged, this,
                &StationServer::onSliceAccessChanged);
        const auto followSlice = [this](SliceModel* slice) {
            if (slice == nullptr) {
                return;
            }
            connect(slice, &SliceModel::bandChanged, m_connectedDevices.get(),
                    &ConnectedDevicesFacade::refresh, Qt::UniqueConnection);
            connect(slice, &SliceModel::dspModeChanged, m_connectedDevices.get(),
                    &ConnectedDevicesFacade::refresh, Qt::UniqueConnection);
        };
        for (SliceModel* slice : radioModel->slices()) {
            followSlice(slice);
        }
        connect(radioModel, &RadioModel::sliceAdded, this, [this, followSlice](int sliceId) {
            if (m_radioModel) {
                followSlice(m_radioModel->sliceById(sliceId));
                // A slice the Core made for nobody (a radio that arrived
                // after the device, the Core's own top-up) goes to a device
                // alone on the Core, as the slices it found at admission
                // did. A turn later, so a layout being restored settles its
                // owners first.
                if (m_radioModel->sliceOwnership()->mark(sliceId).owner.isEmpty()) {
                    QTimer::singleShot(0, this, [this]() { adoptForLoneDevice(); });
                }
            }
            m_connectedDevices->refresh();
        });
        connect(radioModel, &RadioModel::sliceRemoved, m_connectedDevices.get(),
                &ConnectedDevicesFacade::refresh);
        // iPhone app plan Task 77 (ruling 8.12): the holder's last slice
        // closing releases transmit; (ruling 5.4a) a new slice's TX mark.
        connect(radioModel, &RadioModel::sliceRemoved, this,
                [this](int sliceId) { onSliceClosedForHolder(sliceId); });
        connect(radioModel, &RadioModel::sliceAdded, this, [this](int) { refreshTxMarks(); });
        if (TxSliceArbiter* arbiter = radioModel->txSliceArbiter();
            arbiter != nullptr && radioModel->role() == RadioModel::Role::Local) {
            // Ruling 8.11: the flag never moves while the station device
            // is keyed.
            arbiter->setFrozen([this]() { return stationFrozenSlice() >= 0; });
            // Slice control fix wave: a move waiting for the unkey makes its
            // slice read as on the air (sliceTransmitting).
            connect(arbiter, &TxSliceArbiter::pendingHandoffChanged, this, [this](int) {
                if (m_sliceAccessSet) {
                    m_sliceAccessSet->refresh();
                }
                // Fix wave (whole-branch review, Critical 1): a close
                // waiting on the pending slice may run now.
                scheduleDeferredCloses();
            });
            // Ruling 8.10: the holder's choice is remembered for the next
            // time it holds transmit; connectedDevices.transmittingOn.
            connect(arbiter, &TxSliceArbiter::txBoundSliceChanged, this,
                    [this](int, int newId) {
                        const auto holder = m_transmitHolder->holder();
                        if (holder && m_radioModel
                            && SliceAccessPolicy::mayTransmitOn(
                                *m_radioModel->sliceOwnership(), holder->deviceId, newId)) {
                            m_chosenTxSlice.insert(holder->deviceId, newId);
                            // Slice control plan Task 11: the taken mark goes
                            // only on an explicit choice (txSliceChosen,
                            // txSliceSelected), never on this binding.
                        }
                        m_connectedDevices->refresh();
                        if (m_sliceAccessSet) {
                            m_sliceAccessSet->refresh();
                        }
                    });
        }
        // Fix wave I2: a question naming a slice that closed can no longer
        // be proceeded (its id may soon be another device's).
        connect(radioModel, &RadioModel::sliceRemoved, this,
                [this](int sliceId) { dropQuestionsNaming(sliceId); });
    }

    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(m_heartbeatIntervalMs);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &StationServer::onHeartbeatTick);

    m_settingsExportClock.start();
    m_settingsExportCleanup = new QTimer(this);
    m_settingsExportCleanup->setInterval(1000);
    connect(m_settingsExportCleanup, &QTimer::timeout, this,
            &StationServer::expireSettingsExports);
    m_settingsExportCleanup->start();

    m_deltaFlushTimer = new QTimer(this);
    m_deltaFlushTimer->setInterval(kDefaultDeltaFlushMs);
    connect(m_deltaFlushTimer, &QTimer::timeout, this, [this]() {
        // iPhone app Task 72: each session's own view, its own pending.
        for (const QPointer<MirrorView>& view : attachedViews()) {
            if (!view.isNull()) {
                view->flush();
            }
        }
    });

    // iPhone app Task 71 (ruling 4.11): an away device's place frees when
    // its 180 s end. One timer for the next end, re-armed on every change.
    m_graceTimer = new QTimer(this);
    m_graceTimer->setSingleShot(true);
    connect(m_graceTimer, &QTimer::timeout, this, [this]() {
        m_deviceSessions->expireAway();
        scheduleGraceCheck();
    });
    connect(m_deviceSessions.get(), &DeviceSessionRegistry::changed, this,
            &StationServer::scheduleGraceCheck);
    connect(m_deviceSessions.get(), &DeviceSessionRegistry::changed, this, [this]() {
        QMetaObject::invokeMethod(this, [this]() {
            if (m_activeTakeoverSerial != 0 && !m_slotReserved
                && m_deviceSessions->hasPlaceFree()) {
                // A place freed independently while unkey was pending.
                // Wait until the old session's slice cleanup has finished.
                m_activeTakeoverSerial = 0;
                m_settlingTakeover = false;
            }
            drainHeld();
            refreshHeld();
        }, Qt::QueuedConnection);
    });
    connect(m_connectedDevices.get(), &ConnectedDevicesFacade::connectedDevicesChanged,
            this, [this]() { QMetaObject::invokeMethod(this, [this]() { refreshHeld(); },
                                                  Qt::QueuedConnection); });
    // Last fully constructed Core server is canonical for this local model.
    // A caller cannot install admission/freeze callbacks through this association.
    if (m_radioModel && m_radioModel->role() == RadioModel::Role::Local) {
        m_radioModel->m_diversityStationServer = this;
    }
}

StationServer::~StationServer()
{
    const bool canonical = m_radioModel && m_radioModel->m_diversityStationServer == this;
    if (canonical) { m_radioModel->m_diversityStationServer.clear(); }

    // iPhone app plan Task 34: the keying gate asks this object; the model
    // may outlive it.
    if (canonical && m_radioModel && m_radioModel->moxController() != nullptr
        && m_radioModel->role() == RadioModel::Role::Local) {
        m_radioModel->moxController()->setKeyingGate({});
        m_radioModel->moxController()->setOtherDeviceHolds({});
        // Task 77 fix round 2: the amplifier's pending-key probe asks it too.
        m_radioModel->setAmpKeyPendingProbe({});
        m_radioModel->setOtherDeviceHoldsRefusal({});
    }
    // Parity Task 32: MON back on the Core's own outputs, as without a
    // station server.
    if (canonical && m_radioModel && m_radioModel->role() == RadioModel::Role::Local
        && m_radioModel->audioEngine() != nullptr) {
        m_radioModel->audioEngine()->setTxMonitorLocal(true);
    }
    // Task 77: the arbiter's freeze asks this object too.
    if (canonical && m_radioModel && m_radioModel->txSliceArbiter() != nullptr
        && m_radioModel->role() == RadioModel::Role::Local) {
        m_radioModel->txSliceArbiter()->setFrozen({});
    }
    // iPhone app plan Task 39: the transmit state follows the model, which
    // may outlive this object; it lets go first, before the devices' clock
    // it reads is gone.
    if (m_transmitState != nullptr) {
        m_transmitState->unbind();
    }
    // iPhone app Task 14: a pairing code being hashed finishes first; its
    // result is dropped with this object.
    if (m_pairingHashThread) {
        m_pairingHashThread->wait();
    }
    close();
}

// ── Listener lifecycle ───────────────────────────────────────────────────

bool StationServer::listen(const QHostAddress& address, quint16 port)
{
    m_lastError.clear();

    // Idempotent. A second call used to re-apply the SSL configuration and
    // then fail the bind into lastError(), so a caller that could not
    // cheaply tell whether it had already started ended up with a working
    // listener AND an error string describing it as broken. Rebinding
    // somewhere else is close() then listen() again, deliberately explicit.
    if (isListening()) {
        qCDebug(lcStation) << "listen() ignored: already listening on port"
                            << m_openingGate->serverPort();
        return true;
    }

    if (!QSslSocket::supportsSsl()) {
        m_lastError = CertificateStore::tlsBackendDiagnostic();
        if (m_lastError.isEmpty()) {
            m_lastError = QStringLiteral("Qt reports no working TLS backend");
        }
        qCWarning(lcStation) << "Refusing to listen:" << m_lastError;
        return false;
    }
    if (!m_certificates->isValid()) {
        m_lastError = m_certificates->lastError();
        qCWarning(lcStation) << "Refusing to listen:" << m_lastError;
        return false;
    }
    if (!m_identity->isValid()) {
        // iPhone app Task 12: listening without the Core's identity would
        // accept no device, forever, while looking healthy (and a new Core
        // has no token either). Refuse loudly instead.
        m_lastError = m_identity->lastError().isEmpty()
                          ? QStringLiteral("The Core's own key is unavailable")
                          : m_identity->lastError();
        qCWarning(lcStation) << "Refusing to listen:" << m_lastError;
        return false;
    }

    if (m_wsServer == nullptr) {
        m_wsServer = new QWebSocketServer(QStringLiteral("NereusSDR station"),
                                          QWebSocketServer::SecureMode, this);
        connect(m_wsServer, &QWebSocketServer::newConnection, this,
                &StationServer::onNewWebSocketConnection);
    }
    if (m_openingGate == nullptr) {
        // The listening socket is the gate's, not Qt's: the gate runs TLS,
        // reads the opening request, fixes a Host Qt cannot read or
        // answers 400, and hands the connection to m_wsServer for the 101
        // (StationOpeningGate.h has the cause). m_wsServer itself never
        // listens.
        m_openingGate = new StationOpeningGate(m_wsServer, m_maxOpenings,
                                               m_maxOpeningsPerAddress,
                                               &StationServer::addressKey, this);
    }
    m_openingGate->setOpeningLimits(m_maxOpenings, m_maxOpeningsPerAddress);
    m_openingGate->setOpeningDeadlineMs(m_openingDeadlineMs);

    QSslConfiguration tls = QSslConfiguration::defaultConfiguration();
    tls.setLocalCertificate(m_certificates->certificate());
    tls.setPrivateKey(m_certificates->privateKey());
    // The client pins this certificate's fingerprint (parent design
    // section 10.5), so it is the client's job to decide whether to trust
    // it. Asking for a client certificate here would be a second,
    // unimplemented identity mechanism.
    tls.setPeerVerifyMode(QSslSocket::VerifyNone);
    // iPhone app Task 4 (R-IOS-01): the minimum is set here rather than
    // left to Qt's default (QSsl::SecureProtocols, which is TLS 1.2 or
    // later today but may change with Qt). The link document's section 2
    // states it; tst_link_version reads it back from the listener.
    tls.setProtocol(QSsl::TlsV1_2OrLater);
    // The gate's copy is the one every connection uses (it runs TLS;
    // m_wsServer never listens), so it is the only copy, and
    // tlsConfiguration() reads it back from the gate.
    m_openingGate->setTlsConfiguration(tls);

    if (!m_openingGate->listen(address, port)) {
        m_lastError = m_openingGate->errorString();
        qCWarning(lcStation) << "Listen failed:" << m_lastError;
        return false;
    }

    qCInfo(lcStation) << "Station listening on wss://" << address.toString() << ":"
                      << m_openingGate->serverPort();
    emit listeningChanged(true);
    return true;
}

void StationServer::close()
{
    m_settingsExports.clear();
    const QList<SessionTransport*> pendingPrimaries = m_pendingRelayWatches.keys();
    for (SessionTransport* primary : pendingPrimaries) {
        retirePendingRelayWatch(primary);
    }
    if (m_txWatchServer) {
        m_txWatchServer->retireAll();
    }
    const bool wasListening = isListening();
    const QList<SessionTransport*> transports = m_peers.keys();
    // Task 76: the sessions still to be ended are not sent new shares.
    m_closing = true;
    for (SessionTransport* transport : transports) {
        dropPeer(transport, QStringLiteral("The Core is shutting down."), true,
                 /*retryable=*/true);
    }
    m_closing = false;
    if (m_openingGate != nullptr) {
        m_openingGate->closeAll();
    }
    if (m_heartbeatTimer != nullptr) {
        m_heartbeatTimer->stop();
    }
    if (m_deltaFlushTimer != nullptr) {
        m_deltaFlushTimer->stop();
    }
    if (wasListening) { emit listeningChanged(false); }
}

void StationServer::endSessionsForRadioChange(const QString& reason)
{
    const QList<SessionTransport*> transports = m_peers.keys();
    m_closing = true;
    for (SessionTransport* transport : transports) {
        dropPeer(transport, reason, true, /*retryable=*/true,
                 QString::fromLatin1(SessionEndCode::kRadioChanging));
    }
    m_closing = false;
}

void StationServer::holdRadioChangeAnswers()
{
    m_holdingRadioChange = true;
}

void StationServer::finishRadioChange(bool proceeded, const QString& refusal)
{
    m_holdingRadioChange = false;
    if (!m_heldRadioChange) {
        return;
    }
    const HeldRadioChange held = *m_heldRadioChange;
    m_heldRadioChange.reset();
    // The answer's route (recorded because it was not sent in dispatch).
    SessionTransport* to = nullptr;
    const auto route = m_resultRoutes.find(held.key);
    if (route != m_resultRoutes.end()) {
        to = route->data();
        m_resultRoutes.erase(route);
    }
    if (held.proceed) {
        to = held.later.transport.data();
    }
    // Dropped: the change did not happen, in the Core's own words.
    const SessionMessage answer = proceeded
        ? held.result
        : SessionMessages::commandResult(held.result.commandVerb, held.result.commandId, false,
                                         refusal, {});
    if (to != nullptr && hasPeer(to)) {
        sendToPeer(to, answer);
    }
    if (held.proceed || held.tellOnFinish) {
        if (proceeded) {
            tellSettingChanged(held.later.affected, held.later.sliceWords, held.later.change,
                               held.later.requester);
        }
        endOlderWindowsWithoutSlices(held.later.closedDevices, held.later.requester);
    }
}

bool StationServer::isListening() const
{
    return m_openingGate != nullptr && m_openingGate->isListening();
}

QSslConfiguration StationServer::tlsConfiguration() const
{
    // The gate's copy: the one its connections actually run TLS with.
    return m_openingGate != nullptr ? m_openingGate->tlsConfiguration() : QSslConfiguration();
}

quint16 StationServer::peerAgreedMajor(SessionTransport* peer) const
{
    const Peer* it = peerPtr(peer);
    return it != nullptr ? it->agreedMajor : quint16(0);
}

bool StationServer::peerDeclares(SessionTransport* peer, const QByteArray& feature,
                                 int minVersion) const
{
    const Peer* it = peerPtr(peer);
    if (it == nullptr || !it->features.contains(feature)) {
        return false;
    }
    return it->features.value(feature) >= minVersion;
}

bool StationServer::peerKnows2m(SessionTransport* peer) const
{
    const auto it = m_peers.constFind(peerKey(peer));
    return it != m_peers.constEnd() && it->agreedMinor >= kRadioIdentitySessionProtocolMinor
        && peerDeclares(peer, QByteArray(BandLinkFit::kFeature), 1);
}

QByteArray StationServer::encodeFor(SessionTransport* transport,
                                    const SessionMessage& message) const
{
    const QByteArray wire = SessionMessages::encode(message);
    return peerKnows2m(transport) ? wire : BandLinkFit::forPeerWithout2m(wire);
}

StationServer::Peer StationServer::peerFor(SessionTransport* transport) const
{
    if (isStationTransport(transport)) {
        return m_stationPeer;
    }
    return m_peers.value(transport);
}

const StationServer::Peer* StationServer::peerPtr(SessionTransport* transport) const
{
    if (isStationTransport(transport)) {
        return &m_stationPeer;
    }
    const auto it = m_peers.constFind(peerKey(transport));
    return it != m_peers.constEnd() ? &it.value() : nullptr;
}

bool StationServer::hasPeer(SessionTransport* transport) const
{
    return isStationTransport(transport) || m_peers.contains(transport);
}

void StationServer::deliverToStation(const SessionMessage& message)
{
    // Slice control plan Task 10: what the Core sends the station device
    // goes to the hosting desktop's callbacks, never onto a wire.
    switch (message.kind) {
    case SessionMessageKind::CommandResult: {
        const ResultKey key{m_stationPeer.sessionId, message.commandVerb, message.commandId};
        const auto it = m_stationAnswers.find(key);
        if (it == m_stationAnswers.end()) {
            return;
        }
        const StationAnswer answer = it.value();
        // "Waiting for you to confirm." is not the last answer: the final
        // one follows the question, under the same verb and id.
        bool awaiting = false;
        for (const MirrorUpdate& update : message.updates) {
            if (update.name == "phase"
                && update.value.toString() == QLatin1String("needsConfirmation")) {
                awaiting = !message.accepted;
            }
        }
        if (!awaiting && isLastResult(message)) {
            m_stationAnswers.erase(it);
        }
        if (answer) {
            answer(message);
        }
        return;
    }
    case SessionMessageKind::ConfirmRequest:
        if (m_stationQuestion) {
            m_stationQuestion(message);
        }
        return;
    case SessionMessageKind::Notice:
        if (m_stationNotice) {
            m_stationNotice(message);
        }
        return;
    default:
        return;
    }
}

void StationServer::invokeAsStationDevice(const SessionMessage& invoke, StationAnswer answer,
                                          StationAnswer question)
{
    static const QList<QByteArray> kHostVerbs = {
        QByteArrayLiteral("removeSlice"),        QByteArrayLiteral("addSlice"),
        QByteArrayLiteral("addSliceOnPan"),      QByteArrayLiteral("setActiveSliceById"),
        QByteArrayLiteral("slice.listen"),       QByteArrayLiteral("slice.stopListening"),
        QByteArrayLiteral("slice.takeControl"),  QByteArrayLiteral("slice.release"),
        QByteArrayLiteral("diversity.setTarget"),
        QByteArrayLiteral("slice.setListenLevel"), QByteArrayLiteral("confirm.proceed"),
        QByteArrayLiteral("confirm.cancel"),     QByteArrayLiteral("notice.takeBack"),
    };
    if (invoke.kind != SessionMessageKind::CommandInvoke
        || !kHostVerbs.contains(invoke.commandVerb)) {
        if (answer) {
            answer(SessionMessages::commandResult(
                invoke.commandVerb, invoke.commandId, false,
                QStringLiteral("The Core does not know this request. Updating the Core may help."),
                {}));
        }
        return;
    }
    if (answer) {
        m_stationAnswers.insert(
            ResultKey{m_stationPeer.sessionId, invoke.commandVerb, invoke.commandId}, answer);
    }
    if (question) {
        m_stationQuestion = std::move(question);
    }
    runInvoke(m_stationTransport.get(), invoke);
}

void StationServer::setStationNoticeHandler(StationAnswer notice)
{
    m_stationNotice = std::move(notice);
    if (!m_stationNotice) {
        return;
    }
    // Any waiting as an away device's do are handed over now.
    const QList<ConfirmStep::Notice> waiting =
        m_confirm->takePending(SliceOwnership::stationDevice());
    for (const ConfirmStep::Notice& notice : waiting) {
        sendNotice(m_stationTransport.get(), notice);
    }
}

quint16 StationServer::serverPort() const
{
    return m_openingGate != nullptr ? m_openingGate->serverPort() : 0;
}

QHostAddress StationServer::serverAddress() const
{
    return isListening() ? m_openingGate->serverAddress() : QHostAddress{};
}

QString StationServer::token() const
{
    return m_tokens != nullptr ? m_tokens->token() : QString();
}

QString StationServer::certificateFingerprint() const
{
    return m_certificates != nullptr ? m_certificates->fingerprintSha256() : QString();
}

QString StationServer::certificatePemPath() const
{
    return m_certificates != nullptr && m_certificates->isValid()
               ? m_certificates->certificatePath()
               : QString();
}

QString StationServer::privateKeyPemPath() const
{
    return m_certificates != nullptr && m_certificates->isValid()
               ? m_certificates->privateKeyPath()
               : QString();
}

DeviceStore* StationServer::deviceStore() const
{
    return m_devices.get();
}

const StationIdentity& StationServer::stationIdentity() const
{
    return *m_identity;
}

StationDevicesFacade* StationServer::devicesFacade() const
{
    return m_devicesFacade.get();
}

CoreAddressWatcher* StationServer::coreAddressWatcher() const
{
    return m_coreAddresses.get();
}

StationCatalog* StationServer::catalog() const
{
    return m_catalog.get();
}

int StationServer::stationCatalogVersion() const
{
    return 1;
}

int StationServer::displayExtrasVersion() const
{
    // The extras travel on the media display channel, so they come with it.
    // 2 (R-IOS-27, R-IOS-06): also the clarity-retune operation.
    // 3: activePeakHold.onTx, and the peak hold's hold time and transmit
    // gate as the desktop's.
    // 4 (R-IOS-18): noiseFloor.fastAttack and the noise floor state section.
    return m_mediaEnabled ? 4 : 0;
}

int StationServer::deviceAdminVersion() const
{
    // The same condition as stationIdentityVersion: a Core that signs
    // devices in by key can list and administer them.
    return m_certBinding.isEmpty() ? 0 : 1;
}

PairingWindow* StationServer::pairingWindow() const
{
    return m_pairingWindow.get();
}

int StationServer::pairingVersion() const
{
    return m_declaredFeatures.value(QByteArrayLiteral("pairing"), 0) >= 1 ? 1 : 0;
}

void StationServer::printToConsole(const QString& text)
{
    writePairingBanner(text);
}

QString StationServer::addressKey(const QString& address)
{
    if (address.isEmpty()) {
        return {};
    }
    QHostAddress host(address);
    if (host.isNull()) {
        return address;
    }
    bool mapped = false;
    const quint32 ipv4 = host.toIPv4Address(&mapped);
    if (mapped) {
        // IPv4, and IPv4-mapped IPv6, by the full address.
        return QHostAddress(ipv4).toString();
    }
    if (host.protocol() != QAbstractSocket::IPv6Protocol) {
        return host.toString();
    }
    // Part C follow-up (R-IOS-08): an IPv6 host is handed a whole /64 and
    // can dial from any address in it, so IPv6 peers are counted by their
    // /64 prefix. A household on one /64 then shares the two connecting
    // slots the way one behind IPv4 NAT does; signed-in sessions are not
    // counted, and the refusal is retryable.
    Q_IPV6ADDR bytes = host.toIPv6Address();
    for (int i = 8; i < 16; ++i) {
        bytes[i] = 0;
    }
    return QHostAddress(bytes).toString() + QStringLiteral("/64");
}

bool StationServer::isOnDirectNetwork(const QString& address)
{
    if (address.isEmpty()) {
        return false;
    }
    QHostAddress peer(address);
    if (peer.isNull()) {
        return false;
    }
    if (peer.isLoopback()) {
        return true;
    }
    bool mapped = false;
    const quint32 ipv4 = peer.toIPv4Address(&mapped);
    if (mapped) {
        peer = QHostAddress(ipv4);
    }
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& interface : interfaces) {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp)
            || !flags.testFlag(QNetworkInterface::IsRunning)) {
            continue;
        }
        const QList<QNetworkAddressEntry> entries = interface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries) {
            const int prefix = entry.prefixLength();
            if (prefix < 0 || entry.ip().protocol() != peer.protocol()) {
                continue;
            }
            // Scope ids are not part of the subnet question.
            QHostAddress ip = entry.ip();
            ip.setScopeId(QString());
            QHostAddress candidate = peer;
            candidate.setScopeId(QString());
            if (candidate.isInSubnet(ip, prefix)) {
                return true;
            }
        }
    }
    return false;
}

void StationServer::publishConnectedDevices()
{
    if (!m_devicesFacade) {
        return;
    }
    // iPhone app Task 71: admitted sessions only; a sign-in turned away from
    // a full Core never shows as connected.
    QSet<QByteArray> connected;
    for (const Peer& peer : std::as_const(m_peers)) {
        if (!peer.sessionDeviceId.isEmpty() && !peer.deviceId.isEmpty()) {
            connected.insert(peer.deviceId);
        }
    }
    m_devicesFacade->setConnectedDevices(connected);
}

void StationServer::endAuthenticatedPeers(const std::function<bool(const Peer&)>& matches,
                                          const QString& reason, const char* endCode)
{
    // Copied: dropPeer() erases from m_peers.
    const QList<SessionTransport*> transports = m_peers.keys();
    for (SessionTransport* transport : transports) {
        const auto it = m_peers.constFind(transport);
        if (it == m_peers.cend() || !it->authenticated || !matches(*it)) {
            continue;
        }
        if (transport == m_dispatchingTransport) {
            // Its own request did this: the result goes out first.
            m_invokeFrame->pendingEnd = std::make_pair(reason, QString::fromLatin1(endCode));
            continue;
        }
        dropPeer(transport, reason, true, /*retryable=*/false, QString::fromLatin1(endCode));
    }
}

QString StationServer::formatFirstRunBanner(const QString& fingerprint,
                                            const QString& identityKeyPath)
{
    return QStringLiteral(
               "\n"
               "  ============================================================\n"
               "  NereusSDR Core: first run\n"
               "  ------------------------------------------------------------\n"
               "  TLS SHA-256:  %1\n"
               "  Identity key: %2\n"
               "  ------------------------------------------------------------\n"
               "  Back up the identity key file. It is this Core's identity:\n"
               "  if it is lost, every paired device has to pair again.\n"
               "  ============================================================\n")
        .arg(fingerprint, identityKeyPath);
}

bool StationServer::hasAuthenticatedSession() const
{
    return authenticatedSessionCount() > 0;
}

int StationServer::authenticatedSessionCount() const
{
    int count = 0;
    for (const Peer& peer : std::as_const(m_peers)) {
        if (!peer.sessionDeviceId.isEmpty()) {
            ++count;
        }
    }
    return count;
}

int StationServer::devicesConnectedForDiscovery() const
{
    // Ruling 10.4: a Core no device has claimed has no devices on it and
    // sends 0; the count is a number only, never who.
    return m_devices->isClaimed() ? m_deviceSessions->placesTaken() : 0;
}

bool StationServer::peerHoldsSessions(SessionTransport* transport) const
{
    return peerDeclares(transport, QByteArrayLiteral("sessionHolder"), 1)
        && peerDeclares(transport, QByteArrayLiteral("deviceAuth"), 1);
}

bool StationServer::peerHasSessionHolderVersion(SessionTransport* transport) const
{
    const Peer* it = isStationTransport(transport) ? &m_stationPeer : nullptr;
    if (it == nullptr) {
        const auto found = m_peers.constFind(transport);
        it = found != m_peers.cend() ? &found.value() : nullptr;
    }
    return it != nullptr && it->agreedMinor >= kRadioIdentitySessionProtocolMinor
        && peerHoldsSessions(transport) && sessionHolderVersion() >= 1;
}

int StationServer::sliceAccessVersion() const
{
    // Slice control plan Task 4: a Core that runs its radio keeps who
    // controls and who listens to each slice.
    // Take-over parity: 2 adds Take it back on the controlTaken notice.
    // Core-slice take-over (JJ, 2026-09-30): 3, a device may take the
    // Core's own slice with nobody at the Core's desktop.
    return m_radioModel && m_radioModel->role() == RadioModel::Role::Local ? 3 : 0;
}

bool StationServer::peerHasSliceAccess(SessionTransport* transport) const
{
    return peerHasSessionHolderVersion(transport)
        && peerDeclares(transport, QByteArrayLiteral("sliceAccess"), 1)
        && sliceAccessVersion() >= 1;
}

bool StationServer::peerTakesControlBack(SessionTransport* transport) const
{
    return peerHasSliceAccess(transport)
        && peerDeclares(transport, QByteArrayLiteral("sliceAccess"), 2)
        && sliceAccessVersion() >= 2;
}

bool StationServer::peerTakesCoreSlice(SessionTransport* transport) const
{
    return peerHasSliceAccess(transport)
        && peerDeclares(transport, QByteArrayLiteral("sliceAccess"), 3)
        && sliceAccessVersion() >= 3;
}

void StationServer::noteActivity(SessionTransport* transport)
{
    const auto it = m_peers.constFind(transport);
    if (it != m_peers.cend() && !it->sessionDeviceId.isEmpty()) {
        m_deviceSessions->noteActivity(it->sessionDeviceId);
    }
}

void StationServer::scheduleGraceCheck()
{
    if (m_graceTimer == nullptr) {
        return;
    }
    const std::optional<qint64> next = m_deviceSessions->nextExpiryMs();
    if (!next) {
        m_graceTimer->stop();
        return;
    }
    const qint64 remaining = std::max<qint64>(0, *next - m_deviceSessions->now());
    m_graceTimer->start(static_cast<int>(std::min<qint64>(remaining, 24LL * 3600 * 1000)));
}

// ── Peer lifecycle ───────────────────────────────────────────────────────

void StationServer::onNewWebSocketConnection()
{
    while (m_wsServer != nullptr && m_wsServer->hasPendingConnections()) {
        QWebSocket* socket = m_wsServer->nextPendingConnection();
        if (socket == nullptr) {
            break;
        }
        // Opened: it stops counting among the openings still in progress,
        // and from here the peer caps below govern it.
        if (m_openingGate != nullptr) {
            m_openingGate->markOpened(socket);
        }
        const QUrl request = socket->requestUrl();
        if (request.path(QUrl::FullyEncoded) == QLatin1String(TxWatchServer::kPath)) {
            // Branch before peer adoption. The wrapper installs both Qt
            // frame and message caps before the event loop sees input.
            auto* watch = new WebSocketTransport(socket, TxWatchServer::kAttachBytes);
            if (request.hasQuery() || !request.userInfo().isEmpty()) {
                watch->closeLink(QStringLiteral("The Core could not read this transmit watch address."));
                watch->deleteLater();
            } else {
                m_txWatchServer->acceptTransport(watch, addressKey(watch->peerAddress()));
            }
            continue;
        }
        // The cap goes on inside WebSocketTransport's constructor, which
        // runs here, inside the newConnection slot, before control returns
        // to the event loop -- so no frame on this socket has been
        // processed yet. See kMaxIncomingMessageBytes for the arithmetic
        // and for why an uncapped accepted socket is a pre-authentication
        // memory-exhaustion path rather than a theoretical one.
        acceptTransport(
            new WebSocketTransport(socket, kMaxIncomingMessageBytes));
    }
}

void StationServer::acceptTransport(SessionTransport* transport)
{
    adoptTransport(transport, /*mailbox=*/false);
}

void StationServer::acceptPairingMailbox(SessionTransport* transport)
{
    adoptTransport(transport, /*mailbox=*/true);
}

void StationServer::acceptIntroducedTransport(SessionTransport* transport,
                                              const QString& introductionId,
                                              const QByteArray& deviceId)
{
    adoptTransport(transport, /*mailbox=*/false, /*introduced=*/true, introductionId, deviceId);
}

void StationServer::adoptTransport(SessionTransport* transport, bool mailbox, bool introduced,
                                   const QString& introductionId,
                                   const QByteArray& introducedDeviceId)
{
    if (transport == nullptr) {
        return;
    }
    transport->setParent(this);

    // Socket cap. Refused BEFORE any state is allocated for it, and with a
    // reason on the wire so a legitimate client that hits this knows why
    // rather than seeing an unexplained close. iPhone app Task 71: every
    // socket counts, signed in or not (kMaxConcurrentPeers' comment has the
    // arithmetic); who holds a device's place is the registry's.
    if (m_peers.size() >= kMaxConcurrentPeers) {
        qCWarning(lcStation) << "Refusing connection from" << transport->peerDescription()
                             << ": already at" << kMaxConcurrentPeers << "peers";
        // RETRYABLE, and this is the one that mattered most. The header
        // sizes kMaxConcurrentPeers for four devices each reconnecting with
        // an old socket and racing attempts, so this cap is expected to be
        // hit BY a reconnecting client, transiently, while its own dead
        // sockets are still draining. Sent as permanent, it told exactly
        // that client to stop trying forever.
        transport->sendText(SessionMessages::encode(SessionMessages::sessionEnd(
            QStringLiteral("The Core already has as many connections as it allows. Try again shortly."),
            /*retryable=*/true)));
        transport->closeLink(QStringLiteral("The Core already has as many connections as it allows. Try again shortly."));
        transport->deleteLater();
        return;
    }

    // Part C fix wave (R1-M4): one address holds at most
    // kMaxHandshakesPerAddress of the slots while it is still connecting,
    // so a host on the internet cannot keep the phone out by holding every
    // one of them. The same retryable refusal as the cap above.
    //
    // Task 28 fix wave (review Important 2): every connection the remote
    // access service introduced is one source, whatever address it reports
    // (a relayed one reports none): the service can replay introductions,
    // and without this it could hold every slot the direct listener shares
    // while home-network devices are turned away.
    const QString address = introduced ? QString() : addressKey(transport->peerAddress());
    if (introduced || !address.isEmpty()) {
        int connecting = 0;
        for (const Peer& other : std::as_const(m_peers)) {
            if (other.snapshotComplete || other.transport == nullptr) {
                continue;
            }
            if (introduced ? other.introduced
                           : (!other.introduced
                              && addressKey(other.transport->peerAddress()) == address)) {
                ++connecting;
            }
        }
        if (connecting >= kMaxHandshakesPerAddress) {
            qCWarning(lcStation) << "Refusing connection from" << transport->peerDescription()
                                 << (introduced ? ": the remote access service already has"
                                                : ": that address already has")
                                 << connecting << "connections still connecting";
            const QString reason = QStringLiteral(
                "The Core already has as many connections as it allows. Try again shortly.");
            transport->sendText(SessionMessages::encode(
                SessionMessages::sessionEnd(reason, /*retryable=*/true)));
            transport->closeLink(reason);
            transport->deleteLater();
            return;
        }
    }

    // iPhone app plan Task 29 (R-IOS-16; link section 21.2): every
    // session runs on a SwitchableTransport, which can move it to another
    // connection without ending it. A mailbox carries pairing alone and
    // never moves.
    if (!mailbox) {
        auto* switchable = new SwitchableTransport(
            transport, SwitchableTransport::Side::Station, kMaxIncomingMessageBytes, this);
        if (m_pathSwitchDeadlineMs > 0) {
            switchable->setSwitchDeadlineMsForTest(m_pathSwitchDeadlineMs);
        }
        transport = switchable;
    }

    Peer peer;
    peer.transport = transport;
    peer.txWatchGeneration = ++m_nextTxWatchGeneration;
    peer.description = transport->peerDescription();
    // iPhone app Task 12: this connection's own challenge, so a signature
    // made for another connection never verifies on this one.
    peer.challenge = m_deviceAuth->newChallenge();

    // Finish-the-handshake-or-drop. Parented to the transport so it cannot
    // outlive the peer it is about, and stopped once this peer's snapshot
    // has been sent (promoteToSession()), not merely at authentication:
    // R-R3-16/17 bounds the whole connect sequence with the same
    // kStationHandshakeDeadlineMs the GUI uses. An OWNED single-shot timer,
    // not static QTimer::singleShot: cancellability is the whole point.
    //
    // One log line per expiry: dropPeer()'s "Peer detached" line names the
    // peer and this reason, and dropPeer() is also what retires a media
    // context the peer holds (mediaSessionEnded when it is the session).
    if (m_authDeadlineMs > 0) {
        auto* deadline = new QTimer(transport);
        deadline->setSingleShot(true);
        deadline->setInterval(m_authDeadlineMs);
        connect(deadline, &QTimer::timeout, this, [this, transport]() {
            auto it = m_peers.find(transport);
            if (it == m_peers.end() || it->snapshotComplete) {
                return;
            }
            dropPeer(transport, QStringLiteral("This app did not finish connecting to the Core in time."), true,
                     /*retryable=*/true);
        });
        deadline->start();
        peer.authDeadline = deadline;
    }

    // iPhone app plan Task 27: a mailbox carries no hello either way; the
    // pairing starts at pair.start.
    peer.mailboxPairing = mailbox;
    peer.helloReceived = mailbox;
    peer.introduced = introduced;
    peer.introductionId = introductionId;
    peer.introducedDeviceId = introducedDeviceId;

    m_peers.insert(transport, peer);

    connect(transport, &SessionTransport::textReceived, this,
            [this, transport](const QByteArray& wire) { onTransportText(transport, wire); });
    connect(transport, &SessionTransport::pongReceived, this, [this, transport]() {
        auto it = m_peers.find(transport);
        if (it != m_peers.end()) {
            it->pingsAwaitingPong = 0;
        }
    });
    connect(transport, &SessionTransport::closed, this,
            [this, transport]() { onTransportClosed(transport); });

    if (!m_heartbeatTimer->isActive() && m_heartbeatIntervalMs > 0) {
        m_heartbeatTimer->start();
    }

    if (mailbox) {
        qCDebug(lcStation) << "Pairing mailbox attached";
        return;
    }

    // The daemon greets first, so a client can refuse on a major version
    // mismatch without ever having sent its token. Section 7.0's sequence
    // does not fix which end speaks first; sending it in the direction
    // that avoids exposing a secret to an incompatible peer is this
    // task's own choice, recorded here.
    //
    // iPhone app Task 4 (R-IOS-01): the hello names every major this
    // station accepts, and declares its features, so the client can pick
    // the highest shared major before it answers. `major` is the OLDEST of
    // them: a client built before `majors` existed reads only `major` and
    // leaves unless it is the one major it speaks, so the newest there would
    // turn away every such client this station could still serve (spec D39).
    // A client that reads `majors` ignores `major`.
    SessionMessage hello =
        SessionMessages::hello(m_supportedMajors.first(), kSessionProtocolMinor,
                               settingsSchemaVersionOf(m_settings), peerNameForThisProcess(),
                               m_supportedMajors, m_declaredFeatures);
    // iPhone app Task 12 (R-IOS-08): the Core's identity key, its binding
    // to the certificate this connection presents, and the challenge a
    // paired device signs. An older app ignores all three.
    if (!m_certBinding.isEmpty()) {
        hello.stationIdentity = SessionStationIdentity{
            StationIdentity::toBase64Url(m_identity->publicKeySpki()),
            StationIdentity::toBase64Url(m_certBinding),
        };
        hello.challenge = StationIdentity::toBase64Url(peer.challenge);
    }
    send(transport, hello);

    qCDebug(lcStation) << "Peer attached:" << peer.description;
}

void StationServer::onTransportClosed(SessionTransport* transport)
{
    dropPeer(transport, QStringLiteral("The app closed the connection."), false,
             /*retryable=*/true);
}

void StationServer::dropPeer(SessionTransport* transport, const QString& reason,
                             bool sendSessionEnd, bool retryable, const QString& endCode)
{
    const QPointer<StationServer> self(this);
    const QPointer<SessionTransport> peerGuard(transport);
    auto it = m_peers.find(transport);
    if (it == m_peers.end() || it->dropping) {
        return;
    }
    it->dropping = true;
    // LINK minor 2 (TX path): the transmit stop runs first, before any
    // other step of the drop can return early (a view's close, a record
    // stream, the session.end, each of which can end this object or this
    // peer's entry), so no exit path leaves the radio keyed for a device
    // that is gone.
    {
        const QByteArray sessionDevice = it->sessionDeviceId;
        // iPhone app plan Task 37 (R-IOS-13; remote design section 12.1, spec
        // section 4.6 item 1): the session of a device that is keyed, or has
        // VOX armed, ended (a drop, leaving, a replacement or a revocation).
        // The VOX it armed goes off first, then its key stops at once (the
        // emergency stop, not the normal unkey), before the holder's own
        // rules below run.
        if (!sessionDevice.isEmpty() && m_txWatchdog) {
            disarmVoxArmedBy(sessionDevice, "its connection ended");
            if (!self) { return; }
            // Merge of Tasks 37 and 39: a session that ended (rather than went
            // quiet) is told to the window and the phone as a lost link, in
            // txState's own words; recorded before the stop, so the watchdog's
            // "went quiet" reason below does not replace it.
            if (m_radioModel && m_radioModel->keyedBy().deviceId == sessionDevice) {
                recordTransmitStop(
                    TransmitState::kStopLinkLost,
                    TransmitState::linkLostText(deviceNameForStop(sessionDevice)));
                if (!self) { return; }
            }
            if (m_txWatchdog->isWatching(sessionDevice)) {
                m_txWatchdog->linkClosed(sessionDevice);
                if (!self) { return; }
            } else if (m_radioModel && m_radioModel->keyedBy().deviceId == sessionDevice) {
                m_radioModel->stopAllTx(
                    RemoteTxWatchdog::stopMessage(deviceNameForStop(sessionDevice)));
                if (!self) { return; }
            }
        }
        it = m_peers.find(transport);
        if (it == m_peers.end()) {
            return;
        }
    }
    m_settingsExports.remove(it->sessionId);
    it->txWatchGeneration = ++m_nextTxWatchGeneration;
    retirePendingRelayWatch(transport);
    ++m_dropPeerDepth;
    auto finishDrop = qScopeGuard([this, self]() {
        if (!self) { return; }
        if (--m_dropPeerDepth == 0) {
            if (m_activeTakeoverSerial != 0 && !m_slotReserved
                && m_deviceSessions->hasPlaceFree()) {
                m_activeTakeoverSerial = 0;
                m_settlingTakeover = false;
            }
            drainHeld();
        }
    });
    if (m_txWatchServer) {
        m_txWatchServer->retire(transport, /*primaryEnded=*/true);
    }
    if (!self || !peerGuard) {
        return;
    }
    it = m_peers.find(transport);
    if (it == m_peers.end()) {
        return;
    }
    const QString description = it->description;

    if (it->heldSerial != 0) {
        const quint64 serial = it->heldSerial;
        it->heldSerial = 0;
        m_heldQueue.erase(std::remove_if(m_heldQueue.begin(), m_heldQueue.end(),
                         [serial](const HeldQuestion& q) { return q.serial == serial; }),
                         m_heldQueue.end());
    }

    if (sendSessionEnd) {
        send(transport, SessionMessages::sessionEnd(reason, retryable, endCode));
        if (!self) { return; }
    }
    it = m_peers.find(transport);
    if (it == m_peers.end()) return;
    // iPhone app Task 14: a code this connection took and did not pair
    // with is burned, however the connection ends (a wrong code, a device
    // that gave up after its own step 3, a dropped link, the deadline).
    const std::shared_ptr<PairingAttempt> pairing = it->pairing;
    // iPhone app Task 71: an admitted session's end reaches the registry. A
    // paired device that did not leave on purpose is away for 180 s,
    // keeping its place (ruling 4.10); a token window, and a device that
    // left, frees its place at once. A connection replaced by its own
    // device's newer one, or whose device was revoked, settled its place
    // already.
    const QByteArray sessionDevice = it->sessionDeviceId;
    const bool placeSettled = it->placeSettled;
    const bool leaving = it->leaving;
    // iPhone app Task 72: the session's view stops at once (a burst in
    // progress sends nothing more) and goes with the event loop; the
    // DSP-asset jobs this session started are cancelled, and only those
    // (ruling 5.8).
    const QPointer<MirrorView> view = it->view;
    const QString owner = sessionOwner(it->sessionId);
    // iPhone app Task 76: this session's own media ends with it.
    const quint64 mediaEpoch = it->mediaEpoch;
    // Task 77 fix wave, M6: its tx.take copies are forgotten with it.
    m_takeCopies.remove(it->sessionId);
    // Control logging lane: its skipped counts and folded answers.
    m_controlLog.closed(transport, controlLogPeer(*it));
    m_peers.erase(it);
    // Parity Task 19: nothing more from any record stream.
    for (auto& [name, stream] : m_recordStreams) {
        Q_UNUSED(name);
        stream->unsubscribe(transport);
    }
    // Parity Task 33: a gone viewer of the CFC display may end the reads.
    updateCfcCompressionPolling();
    updateVaxLevelsPolling();
    if (!self) { return; }
    // R-IOS-13 / R-R3-49: the Mod Monitor stops with its last watcher.
    if (m_modMonitor) {
        m_modMonitor->subscriptionsChanged();
        if (!self) { return; }
    }
    if (!view.isNull()) {
        view->close();
        if (!self) { return; }
        if (view) view->deleteLater();
    }
    if (!owner.isEmpty()) {
        m_micSourceReplies.remove(owner);
        if (m_radioModel) { m_radioModel->forgetRemoteMicSession(owner); }
        m_dispatcher->endSessionOwner(owner);
        if (!self) { return; }
        // Task 35: its keying commands (and their copies) are forgotten, so
        // a reconnect never replays a key.
        if (m_remoteKeying) {
            m_remoteKeying->forgetSession(owner);
            if (!self) { return; }
        }
    }
    for (auto route = m_resultRoutes.begin(); route != m_resultRoutes.end();) {
        if (route->isNull() || route->data() == transport) {
            m_keyingReplyCounts.remove(route.key());
            route = m_resultRoutes.erase(route);
        } else {
            ++route;
        }
    }
    if (pairing && pairing->codeTaken && !pairing->finished) {
        pairing->finished = true;
        m_pairingWindow->pairingFailed(pairing->route);
        if (!self) { return; }
    }
    // iPhone app Task 74 (ruling 7.5): a session's open question ends with
    // it (a newer connection of the same device asks afresh).
    if (!sessionDevice.isEmpty()) {
        m_confirm->dropQuestion(sessionDevice);
        if (!self) { return; }
    }
    if (!sessionDevice.isEmpty() && !placeSettled) {
        m_deviceSessions->sessionEnded(sessionDevice, transport,
                                       leaving ? DeviceSessionRegistry::EndKind::Left
                                               : DeviceSessionRegistry::EndKind::Dropped);
        if (!self) { return; }
        // iPhone app Task 73 (ruling 4.12): a device that left, and a token
        // window (no 180 s: it cannot be recognised when it comes back),
        // give up their slices now. A paired device that dropped keeps them
        // for its 180 s (graceEnded).
        if (leaving || sessionDevice.startsWith("token:")) {
            releaseDeviceClaims(sessionDevice, std::nullopt);
            if (!self) { return; }
        }
        // iPhone app plan Task 34 (ruling 8.15): the holder that left on
        // purpose (or a token window, which cannot be recognised again)
        // releases transmit through a transfer to nobody; a dropped holder
        // is unkeyed at once through the fence and keeps transmit, away.
        // iPhone app plan Task 77: a Tuner Genius autotune the device asked
        // for ends with its session, keyed or still waiting for the
        // amplifier, so it never keys for a device that is gone.
        if (m_radioModel) {
            m_radioModel->cancelTgxlAutotuneFor(
                sessionDevice,
                RadioModel::tunerTuneEndedReason(RadioModel::TunerTuneEnd::LinkLost));
            if (!self) { return; }
        }
        if (leaving || sessionDevice.startsWith("token:")) {
            m_transmitHolder->release(sessionDevice, QStringLiteral("The device left the Core."));
            if (!self) { return; }
        } else {
            // iPhone app plan Task 39: a holder whose link dropped while it
            // was on the air is stopped by the Core.
            noteHolderStopped(sessionDevice, TransmitState::kStopLinkLost);
            if (!self) { return; }
            m_transmitHolder->holderDropped(sessionDevice,
                                            QStringLiteral("The device's link was lost."));
            if (!self) { return; }
        }
    }
    publishConnectedDevices();
    if (!self) { return; }

    if (mediaEpoch != 0) {
        // iPhone app Task 76: this session's media and telemetry end; every
        // other session's go on. The session state the old one-media-
        // session rule reset at its end is the Core's again once no session
        // is left; the PureSignal display leaves with its subscriber.
        const bool lastSession = primaryMediaSession() == nullptr;
        if (lastSession) {
            m_ps3SubscriberEpoch = 0;
            m_dispatcher->resetSessionState();
            if (!self) { return; }
            if (m_radioModel) {
                m_radioModel->pureSignalFacade()->resetSession();
                if (!self) { return; }
            }
            // Fix wave: the C-Tune pins are not the session's. They end
            // when their device leaves for good (releaseDeviceClaims and
            // revocation), so a device coming back keeps them (ruling 4.8).
        } else if (m_ps3SubscriberEpoch == mediaEpoch) {
            m_ps3SubscriberEpoch = 0;
            if (m_radioModel) {
                m_radioModel->pureSignalFacade()->setRemoteAmpViewSubscribed(false);
                if (!self) { return; }
            }
        }
        emit mediaSessionEnded(mediaEpoch);
        if (!self) { return; }
        emit telemetrySessionEnded(mediaEpoch);
        if (!self) { return; }
        // The others' shares grow back (new generations to each), unless
        // the Core is ending every session.
        if (!m_closing) {
            if (recomputeDisplayBudgetShares()) {
                emit displayBudgetChanged();
                if (!self) { return; }
            }
            publishBudgetToChangedSessions();
            if (!self) { return; }
        }
    }
    if (!hasAuthenticatedSession()) {
        // Stop draining deltas into nothing. StateMirror keeps watching --
        // the daemon's own state is not the sessions' to tear down -- and
        // the next attachSession() clears whatever the coalescer holds
        // anyway, because a fresh burst already carries every watched
        // object's current value (each view clears its own at attach).
        m_deltaFlushTimer->stop();
    }

    // Keep queued final output (including pair.confirm) alive until close
    // completes. Detach from the server so destroying it cannot truncate
    // the drain. Register first: some transports emit closed synchronously.
    // The existing radio-change bound also limits an unresponsive close.
    if (!peerGuard) return;
    transport->setParent(nullptr);
    connect(transport, &SessionTransport::closed, transport, &QObject::deleteLater);
    QTimer::singleShot(kRadioChangeLingerMs, transport, &QObject::deleteLater);
    transport->closeLink(reason);
    if (!self) { return; }

    if (m_peers.isEmpty() && m_heartbeatTimer != nullptr) {
        m_heartbeatTimer->stop();
    }

    qCInfo(lcStation) << "Peer detached:" << description << "reason:" << reason;
    emit peerDisconnected(description, reason);
}

// ── Heartbeat ────────────────────────────────────────────────────────────

void StationServer::setHeartbeatIntervalMs(int ms)
{
    m_heartbeatIntervalMs = ms;
    if (ms <= 0) {
        qCWarning(lcStation)
            << "Heartbeat disabled. A peer that dies without closing the TCP "
               "connection will not be detected.";
        m_heartbeatTimer->stop();
        return;
    }
    m_heartbeatTimer->setInterval(ms);
    if (!m_peers.isEmpty()) {
        m_heartbeatTimer->start();
    }
}

void StationServer::setMaxMissedPongs(int misses)
{
    m_maxMissedPongs = misses < 1 ? 1 : misses;
}

static_assert(StationServer::kDefaultOpeningDeadlineMs
                  == StationOpeningGate::kDefaultOpeningDeadlineMs,
              "the Core and its opening gate state one opening deadline");

#ifdef NEREUS_BUILD_TESTS
void StationServer::setOpeningLimitsForTest(int total, int perAddress)
{
    m_maxOpenings = total > 0 ? total : kMaxUnfinishedOpenings;
    m_maxOpeningsPerAddress = perAddress > 0 ? perAddress : kMaxHandshakesPerAddress;
}
#endif

void StationServer::setOpeningDeadlineMs(int ms)
{
    if (ms > 0) {
        m_openingDeadlineMs = ms;
    }
}

int StationServer::openingCount() const
{
    return m_openingGate != nullptr ? m_openingGate->pendingCount() : 0;
}

void StationServer::setAuthDeadlineMs(int ms)
{
    m_authDeadlineMs = ms;
    if (ms < 1) {
        qCWarning(lcStation)
            << "Handshake deadline disabled. A peer that connects and answers pings "
               "but never authenticates will hold its slot indefinitely.";
    }
}

void StationServer::setAuthRateLimit(int maxFailures, int lockoutMs)
{
    if (m_tokens != nullptr) {
        m_tokens->setRateLimit(maxFailures, lockoutMs);
    }
}

void StationServer::onHeartbeatTick()
{
    // Copied deliberately: dropPeer() mutates m_peers, and a peer declared
    // dead here is dropped inside this loop.
    const QList<SessionTransport*> transports = m_peers.keys();
    for (SessionTransport* transport : transports) {
        auto it = m_peers.find(transport);
        if (it == m_peers.end()) {
            continue;
        }
        if (it->pingsAwaitingPong >= m_maxMissedPongs) {
            const QString description = it->description;
            qCWarning(lcStation)
                << "Peer" << description << "missed" << it->pingsAwaitingPong
                << "consecutive pongs; declaring the link dead";
            emit peerHeartbeatTimeout(description);
            dropPeer(transport, QStringLiteral("This app stopped answering, so the Core closed the connection."), true,
                     /*retryable=*/true);
            continue;
        }
        ++it->pingsAwaitingPong;
        transport->ping();
        // Control logging lane: skipped counts, folded answers and the link.
        if (const auto peer = m_peers.constFind(transport); peer != m_peers.cend()) {
            m_controlLog.tick(transport, controlLogPeer(*peer));
        }
    }
    m_controlLog.tickCore();
    // The selected relay pair and the separate leg can change after the
    // initial snapshot without changing transmit permission.
    publishTxPermitted();
}

// ── Inbound dispatch ─────────────────────────────────────────────────────

void StationServer::onTransportText(SessionTransport* transport, const QByteArray& wire)
{
    auto it = m_peers.find(transport);
    if (it == m_peers.end()) {
        return;
    }

    SessionMessage message;
    if (!SessionMessages::decode(wire, &message)) {
        dropPeer(transport, QStringLiteral("The Core could not read a message from this app."), true,
                 /*retryable=*/false, QString::fromLatin1(SessionEndCode::kProtocolError));
        return;
    }
    // Control logging lane: logging only; nothing below depends on it.
    noteControlIn(transport, message);
    if (it->heldSerial != 0) {
        if (message.kind == SessionMessageKind::SessionTakeover
            && peerHasSessionHolderVersion(transport)) {
            handleTakeover(transport, message);
        } else {
            dropPeer(transport, QStringLiteral("The Core could not read a message from this app."),
                     true, false, QString::fromLatin1(SessionEndCode::kProtocolError));
        }
        return;
    }
    if (message.kind == SessionMessageKind::SessionTakeover) {
        dropPeer(transport, QStringLiteral("The Core could not read a message from this app."),
                 true, false, QString::fromLatin1(SessionEndCode::kProtocolError));
        return;
    }

    // iPhone app plan Task 27: a mailbox carries pairing messages only.
    if (it->mailboxPairing && message.kind != SessionMessageKind::PairStart
        && message.kind != SessionMessageKind::PairSpake
        && message.kind != SessionMessageKind::PairConfirm
        && message.kind != SessionMessageKind::PairFail) {
        dropPeer(transport, QStringLiteral("This app started pairing out of order."), true,
                 /*retryable=*/false, QString::fromLatin1(SessionEndCode::kProtocolError));
        return;
    }

    // Task 28 fix wave (review Important 1): a connection the remote
    // access service introduced never pairs. Pairing through the service
    // is the mailbox's, by code, where a burned code counts as the
    // service's and the service's pause applies (PairingWindow); on this
    // connection it would count as a direct one's. Refused before anything
    // takes a code.
    if (it->introduced
        && (message.kind == SessionMessageKind::PairStart
            || message.kind == SessionMessageKind::PairSpake
            || message.kind == SessionMessageKind::PairConfirm
            || message.kind == SessionMessageKind::PairFail)) {
        sendPairFail(transport,
                     QStringLiteral("A device cannot pair over a connection through the remote "
                                    "access service. Pair it with the Core's pairing code."),
                     0);
        return;
    }

    switch (message.kind) {
    case SessionMessageKind::Hello:
        handleHello(transport, message);
        return;
    case SessionMessageKind::AuthRequest:
        handleAuthRequest(transport, message);
        return;
    // iPhone app plan Task 29 (link section 21.2): a new connection joins
    // a session in place of signing in.
    case SessionMessageKind::PathJoin:
        handlePathJoin(transport, message);
        return;
    case SessionMessageKind::PathSwitch:
        // The barrier is the SwitchableTransport's; one that reaches here
        // came outside a move and changes nothing.
        qCInfo(lcStation) << "Ignoring a path.switch outside a move from" << it->description;
        return;
    // iPhone app Task 14: pairing runs in place of a sign-in.
    case SessionMessageKind::PairStart:
        handlePairStart(transport, message);
        return;
    case SessionMessageKind::PairSpake:
        handlePairSpake(transport, message);
        return;
    case SessionMessageKind::PairConfirm:
        handlePairConfirm(transport, message);
        return;
    case SessionMessageKind::PairFail:
        if (!it->authenticated) {
            handlePairFailFromDevice(transport);
            return;
        }
        break;
    default:
        break;
    }

    if (!it->authenticated) {
        // Everything below this line moves radio or settings state. A peer
        // that has not proved it holds the token gets exactly one answer.
        dropPeer(transport, QStringLiteral("This app sent a request before the Core had accepted its pairing token."), true,
                 /*retryable=*/false, QString::fromLatin1(SessionEndCode::kProtocolError));
        return;
    }

    // iPhone app Task 71: a command, property write or settings write is the
    // device's activity (heartbeats and media control are not).
    if (message.kind == SessionMessageKind::CommandInvoke
        || message.kind == SessionMessageKind::PropertyWrite
        || message.kind == SessionMessageKind::SettingsWrite
        || message.kind == SessionMessageKind::SettingsRemove) {
        noteActivity(transport);
    }

    switch (message.kind) {
    case SessionMessageKind::CommandInvoke:
        // A source command id belongs to this authenticated session's exact
        // request, including its verb. Reusing it must never key the radio.
        if (const auto cache = m_micSourceReplies.value(sessionOwner(it->sessionId));
            cache.contains(message.commandId)
            && cache.value(message.commandId).first != SessionMessages::encode(message)) {
            send(transport, SessionMessages::commandResult(message.commandVerb, message.commandId,
                false, QStringLiteral("The Core could not read this request."), {}));
            break;
        }
        if (message.commandVerb.startsWith("station.settingsExport.")) {
            handleSettingsExport(transport, message, wire);
            break;
        }
        if (message.commandVerb == "tx.watchRelay") {
            handleTxWatchRelay(transport, message);
            break;
        }
        if (message.commandVerb == "tx.watchTicket") {
            handleTxWatchTicket(transport, message);
            break;
        }
        // iPhone app plan Task 29 (link section 21.2): the ticket that lets
        // a new connection join this session; the Core's own, never the
        // dispatcher's.
        if (message.commandVerb == "session.pathTicket") {
            handlePathTicket(transport, message);
            break;
        }
        // iPhone app Task 71 (ruling 10.1): session.leave came with
        // sessionHolderVersion 1; from any other peer it is a verb this
        // Core does not route, answered in the same words.
        if ((message.commandVerb == "session.leave" || message.commandVerb == "confirm.proceed"
             || message.commandVerb == "confirm.cancel"
             || message.commandVerb == "notice.takeBack")
            && !peerHasSessionHolderVersion(transport)) {
            // SessionCommandDispatcher's own words for a verb it does not
            // route.
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                QStringLiteral("The Core does not know this request. Updating the Core may help."),
                {}));
            break;
        }
        if (message.commandVerb == "setFourO3AEnabled"
            && it->agreedMinor < kRemoteFourO3AControlSessionProtocolMinor) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                QStringLiteral("Update this app to control 4O3A on this Core."), {}));
            break;
        }
        if ((message.commandVerb.startsWith("nnr.") || message.commandVerb.startsWith("ps3.")
             || message.commandVerb.startsWith("dspAssets."))
            && it->agreedMinor < kDspControlSessionProtocolMinor) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                QStringLiteral("Update this app to use this control on this Core."), {}));
            break;
        }
        if (message.commandVerb == "nnr.tryAgain"
            && it->agreedMinor < kNnrLimitSessionProtocolMinor) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                QStringLiteral("Update this app to try noise reduction again on this Core."), {}));
            break;
        }
        if (message.commandVerb.startsWith("notch.")
            && it->agreedMinor < kDspControlSessionProtocolMinor) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                QStringLiteral("Update this app to change notches on this Core."), {}));
            break;
        }
        // R-R3-47: the Power Genius verbs came with remotePgxlControlVersion
        // 2, in the minor-11 capability block.
        if ((message.commandVerb == "configurePgxl" || message.commandVerb == "disconnectPgxl"
             || message.commandVerb == "setPgxlConnectionSettings")
            && it->agreedMinor < kRadioIdentitySessionProtocolMinor) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                QStringLiteral("Update this app to set up the Power Genius on this Core."), {}));
            break;
        }
        // R-R3-47 / R-R3-48: the RF-Kit verbs came with
        // remoteRfKitControlVersion 2 and the station TCI verb with
        // stationTciVersion 1, in the same minor-11 block.
        if ((message.commandVerb == "configureRfKit" || message.commandVerb == "disconnectRfKit"
             || message.commandVerb == "setRfKitEnabled")
            && it->agreedMinor < kRadioIdentitySessionProtocolMinor) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                QStringLiteral("Update this app to set up the RF-Kit amplifier on this Core."), {}));
            break;
        }
        // R-R3-46 / R-R3-21: the filter policy verb came with
        // radioHardwareVersion 4, in the minor-11 block.
        if (message.commandVerb == "setAlexBpfMode"
            && (it->agreedMinor < kRadioIdentitySessionProtocolMinor
                || radioHardwareVersion() < 4)) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                it->agreedMinor < kRadioIdentitySessionProtocolMinor
                    ? QStringLiteral("Update this app to change the filter policy on this Core.")
                    : QStringLiteral("The Core has no filter settings ready."), {}));
            break;
        }
        // R-R3-49 (parity Task 10): the RF-Kit's OPERATE and STANDBY,
        // antenna, TCI mode and saved address came with
        // remoteRfKitControlVersion 4.
        if (isRfKitFullControlVerb(message.commandVerb)
            && (it->agreedMinor < kRadioIdentitySessionProtocolMinor
                || rfKitControlVersion() < 4)) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                it->agreedMinor < kRadioIdentitySessionProtocolMinor
                    ? QStringLiteral("Update this app to switch the RF-Kit amplifier on this "
                                     "Core.")
                    : QStringLiteral("This Core cannot change its amplifier and tuner settings."), {}));
            break;
        }
        // I4 (R-R3-47): Reset amp error came with remoteRfKitControlVersion 3.
        if (message.commandVerb == "resetRfKitError"
            && (it->agreedMinor < kRadioIdentitySessionProtocolMinor
                || rfKitControlVersion() < 3)) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                it->agreedMinor < kRadioIdentitySessionProtocolMinor
                    ? QStringLiteral("Update this app to reset the RF-Kit amplifier's error on "
                                     "this Core.")
                    : QStringLiteral("This Core cannot reset its RF-Kit amplifier's error."), {}));
            break;
        }
        if (message.commandVerb == "setStationTci"
            && (it->agreedMinor < kRadioIdentitySessionProtocolMinor
                || stationTciVersion() < 1)) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                it->agreedMinor < kRadioIdentitySessionProtocolMinor
                    ? QStringLiteral("Update this app to turn the Core's TCI server on or off.")
                    : QStringLiteral("This Core has no TCI server."), {}));
            break;
        }
        // JJ's ruling of 2026-09-28: the rest of the TCI server's settings,
        // for a peer that declared stationTciSettings.
        if (message.commandVerb == "setStationTciSettings"
            && !peerGetsStationTciSettings(transport)) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                it->agreedMinor < kRadioIdentitySessionProtocolMinor
                    ? QStringLiteral("Update this app to change the Core's TCI server.")
                    : QStringLiteral("This Core cannot change its TCI server's settings or "
                                     "apps from here."), {}));
            break;
        }
        // Parity Task 23: the station TCI server's options and apps came
        // with stationTciVersion 2.
        if ((message.commandVerb == "setStationTciOptions"
             || message.commandVerb == "disconnectStationTciClient")
            && (it->agreedMinor < kRadioIdentitySessionProtocolMinor
                || stationTciVersion() < 2)) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                it->agreedMinor < kRadioIdentitySessionProtocolMinor
                    ? QStringLiteral("Update this app to change the Core's TCI server.")
                    : QStringLiteral("This Core cannot change its TCI server's settings or "
                                     "apps from here."), {}));
            break;
        }
        // R-R3-47 / R-R3-22: the accessory record verbs came with
        // accessoryDataVersion 1, in the same minor-11 block.
        if ((message.commandVerb == "setTxInterlockPolicy"
             || message.commandVerb == "setPgxlPowerCap"
             || message.commandVerb == "clearAccessoryFaults")
            && (it->agreedMinor < kRadioIdentitySessionProtocolMinor
                || accessoryDataVersion() < 1)) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                it->agreedMinor < kRadioIdentitySessionProtocolMinor
                    ? QStringLiteral("Update this app to change the station's amplifier and "
                                     "tuner settings on this Core.")
                    : QStringLiteral("This Core cannot change its amplifier and tuner settings."), {}));
            break;
        }
        if (isAccessoryTxVerb(message.commandVerb)
            || isLegacyAccessoryTxVerb(message.commandVerb)) {
            const bool newAccessoryTxVerb = isAccessoryTxVerb(message.commandVerb);
            const bool legacyAccessoryTxVerb = !newAccessoryTxVerb;
            if (newAccessoryTxVerb && (it->agreedMinor < kRadioIdentitySessionProtocolMinor
                || accessoryTxVersion() < 1)) {
                const TxRefusal refusal = TxRefusals::appCannotTransmit();
                send(transport, SessionMessages::commandResult(
                    message.commandVerb, message.commandId, false, refusal.text, {},
                    {{0, "refusalCode", MirrorWireKind::Utf8, QString::fromUtf8(refusal.code)},
                     {0, "refusalFix", MirrorWireKind::Utf8, QString::fromUtf8(refusal.fix)}}));
                break;
            }
            if (!accessoryTxArgumentsValid(message)) {
                send(transport, SessionMessages::commandResult(
                    message.commandVerb, message.commandId, false,
                    legacyAccessoryTxVerb ? legacyAccessoryInvalidReason(message.commandVerb)
                                          : QStringLiteral("The Core could not read this request."), {},
                    legacyAccessoryTxVerb ? QList<MirrorUpdate>{}
                        : QList<MirrorUpdate>{{0, "refusalCode", MirrorWireKind::Utf8,
                                               QStringLiteral("invalidRequest")},
                                              {0, "refusalFix", MirrorWireKind::Utf8, QString()}}));
                break;
            }
            // Gate before the shared-setting question, then once more when
            // its held command runs on confirm.proceed.
            StationTxGate sessionOnly;
            sessionOnly.setRemoteTransmitAllowed(m_txGate.remoteTransmitAllowed());
            const TxDecision decision = sessionOnly.decide(peerInfoFor(transport));
            if (!decision.permitted) {
                send(transport, SessionMessages::commandResult(
                    message.commandVerb, message.commandId, false, decision.refusal.text, {},
                    legacyAccessoryTxVerb ? QList<MirrorUpdate>{}
                        : QList<MirrorUpdate>{{0, "refusalCode", MirrorWireKind::Utf8,
                                               QString::fromUtf8(decision.refusal.code)},
                                              {0, "refusalFix", MirrorWireKind::Utf8,
                                               QString::fromUtf8(decision.refusal.fix)}}));
                break;
            }
        }
        // R-R3-47 / R-R3-22: the amp's own settings came with
        // remotePgxlControlVersion 3 and the tuner's with
        // remoteTgxlControlVersion 1, in the same minor-11 block.
        if (isPgxlDeviceSettingsVerb(message.commandVerb)
            && (it->agreedMinor < kRadioIdentitySessionProtocolMinor
                || pgxlControlVersion() < 3)) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                it->agreedMinor < kRadioIdentitySessionProtocolMinor
                    ? QStringLiteral("Update this app to change the Power Genius's own settings "
                                     "on this Core.")
                    : QStringLiteral("This Core cannot change its amplifier and tuner settings."), {}));
            break;
        }
        // R-R3-49 (parity Task 9): the Power Genius's OPERATE and STANDBY,
        // LAN scan and saved address came with remotePgxlControlVersion 4.
        if (isPgxlFullControlVerb(message.commandVerb)
            && (it->agreedMinor < kRadioIdentitySessionProtocolMinor
                || pgxlControlVersion() < 4)) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                it->agreedMinor < kRadioIdentitySessionProtocolMinor
                    ? QStringLiteral("Update this app to switch the Power Genius on this Core.")
                    : QStringLiteral("This Core cannot change its amplifier and tuner settings."), {}));
            break;
        }
        if (isTgxlDeviceSettingsVerb(message.commandVerb)
            && (it->agreedMinor < kRadioIdentitySessionProtocolMinor
                || tgxlControlVersion() < 1)) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                it->agreedMinor < kRadioIdentitySessionProtocolMinor
                    ? QStringLiteral("Update this app to change the Tuner Genius's own settings "
                                     "on this Core.")
                    : QStringLiteral("This Core cannot change its amplifier and tuner settings."), {}));
            break;
        }
        // R-R3-49 / R-R3-47: the tuner's antenna, operate and bypass came
        // with remoteTgxlControlVersion 2, in the same minor-11 block.
        // R-R3-49 (parity Task 8): the relay nudge, LAN scan and saved
        // address came with remoteTgxlControlVersion 4, with the same
        // reasons.
        if ((isTgxlControlVerb(message.commandVerb) || isTgxlFullControlVerb(message.commandVerb))
            && (it->agreedMinor < kRadioIdentitySessionProtocolMinor
                || tgxlControlVersion() < (isTgxlControlVerb(message.commandVerb) ? 2 : 4))) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                it->agreedMinor < kRadioIdentitySessionProtocolMinor
                    ? QStringLiteral("Update this app to switch the Tuner Genius on this Core.")
                    : QStringLiteral("This Core cannot change its amplifier and tuner settings."), {}));
            break;
        }
        // R-R3-49 (parity Task 2): the Tune Power slider's command came
        // with transmitSettingsVersion 2, in the same minor-11 block.
        if (message.commandVerb == "setTunePowerForTxBand"
            && (it->agreedMinor < kRadioIdentitySessionProtocolMinor
                || transmitSettingsVersion() < 2)) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                it->agreedMinor < kRadioIdentitySessionProtocolMinor
                    ? QStringLiteral("Update this app to change the tune power on this Core.")
                    : QStringLiteral("This Core cannot change its transmit settings."), {}));
            break;
        }
        // transmitSettingsVersion 15: the CFC band editor's command, in the
        // minor-11 block.
        if (message.commandVerb == "cfc.setProfile"
            && (it->agreedMinor < kRadioIdentitySessionProtocolMinor
                || transmitSettingsVersion() < kTransmitSettingsCfcProfileVersion)) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                it->agreedMinor < kRadioIdentitySessionProtocolMinor
                    ? QStringLiteral("Update this app to change the CFC settings on this Core.")
                    : QStringLiteral("This Core cannot change its transmit settings."), {}));
            break;
        }
        // R-IOS-13 / R-R3-49: the TX EQ curve verbs came with
        // txEqCurveVersion 2, for a peer whose hello declared txEqCurve 2.
        if ((message.commandVerb == "txEq.setCurve" || message.commandVerb == "txEq.resetCurve")
            && txEqCurveVersionFor(transport) < 2) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                QStringLiteral("Update this app to change the TX EQ curve on this Core."), {}));
            break;
        }
        // Slice control plan Task 4: listening and control came with
        // sliceAccessVersion 1, for a device at minor 11 whose hello
        // declared sliceAccess with sessionHolder.
        if ((message.commandVerb == "slice.listen" || message.commandVerb == "slice.stopListening"
             || message.commandVerb == "slice.takeControl"
             || message.commandVerb == "slice.release"
             || message.commandVerb == "slice.setListenLevel")
            && !peerHasSliceAccess(transport)) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                sliceAccessVersion() < 1
                    ? QStringLiteral("This Core cannot share slices between devices.")
                    : QStringLiteral("Update this app to listen to and take slices on this Core."),
                {}));
            break;
        }
        // R-IOS-27, R-IOS-06: a slice's band buttons came with
        // bandSelectVersion 1, in the minor-11 block.
        if (message.commandVerb == "slice.selectBand"
            && (it->agreedMinor < kRadioIdentitySessionProtocolMinor
                || bandSelectVersion() < 1)) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                it->agreedMinor < kRadioIdentitySessionProtocolMinor
                    ? QStringLiteral("Update this app to change bands on this Core.")
                    : QStringLiteral("This Core cannot change bands for an app."), {}));
            break;
        }
        // R-R3-49 (parity Task 3): the TX profile verbs and the RADE vocoder
        // reset came with transmitSettingsVersion 3, in the minor-11 block.
        if ((message.commandVerb.startsWith("txProfile.")
             || message.commandVerb == "rade.resetVocoder")
            && (it->agreedMinor < kRadioIdentitySessionProtocolMinor
                || transmitSettingsVersion() < 3)) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                it->agreedMinor < kRadioIdentitySessionProtocolMinor
                    ? QStringLiteral("Update this app to change transmit profiles on this Core.")
                    : QStringLiteral("This Core cannot change its transmit settings."), {}));
            break;
        }
        if ((message.commandVerb == "configureTgxl" || message.commandVerb == "disconnectTgxl")
            && it->agreedMinor < kRemoteTgxlConfigSessionProtocolMinor) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                QStringLiteral("Update this app to set up the Tuner Genius XL on this Core."), {}));
            break;
        }
        if ((message.commandVerb == "requestStreamCtunPinned"
             || message.commandVerb == "requestStreamCentre")
            && it->agreedMinor < kRemoteCtunSessionProtocolMinor) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                QStringLiteral("Update this app to use C-Tune on this Core."), {}));
            break;
        }
        if (message.commandVerb == "tx.setMicSource"
            && (it->agreedMinor < kRadioIdentitySessionProtocolMinor
                || !peerDeclares(transport, QByteArrayLiteral("remoteTx"), 1)
                || !peerDeclares(transport, QByteArrayLiteral("radioMic"), 2))) {
            send(transport, SessionMessages::commandResult(message.commandVerb, message.commandId,
                                                           false, remoteMicLegacyReason(), {}));
            break;
        }
        // iPhone app plan Task 34: tx.setTxSlice came with remoteTxVersion
        // 1, for a peer at minor 11 whose hello declared remoteTx; Task 35's
        // keying verbs and Task 37's tx.keepalive with it.
        if ((message.commandVerb == "tx.setTxSlice" || message.commandVerb == "tx.key"
             || message.commandVerb == "tx.unkey" || message.commandVerb == "tx.tune"
             || message.commandVerb == "tx.twoTone" || message.commandVerb == "tx.keepalive")
            && (it->agreedMinor < kRadioIdentitySessionProtocolMinor
                || !peerDeclares(transport, QByteArrayLiteral("remoteTx"), 1))) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                TxRefusals::appCannotTransmit().text, {}));
            break;
        }
        // R-R3-49 / R-IOS-18 (paProfileVersion 1): the PA profile verbs, to a
        // peer that declared paProfiles, gated as the desktop's own PA
        // profile writes are.
        if (message.commandVerb.startsWith("paProfile.")) {
            QString refusal;
            if (it->agreedMinor < kRadioIdentitySessionProtocolMinor) {
                refusal = QStringLiteral("Update this app to change PA profiles on this Core.");
            } else if (!peerGetsPaProfiles(transport)) {
                refusal = QStringLiteral("This app cannot change the Core's PA profiles.");
            } else {
                refusal = paProfileRefusal(transport);
            }
            if (!refusal.isEmpty()) {
                send(transport, SessionMessages::commandResult(
                    message.commandVerb, message.commandId, false, refusal, {}));
                break;
            }
        }
        if (message.commandVerb == "tx.twoTonePreset"
            && (it->agreedMinor < kRadioIdentitySessionProtocolMinor
                || !peerDeclares(transport, QByteArrayLiteral("setupDescription"), 1)
                || !transmitSettingsOffered(peerKey(transport)))) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                QStringLiteral("This app cannot change the Core's two-tone preset."), {}));
            break;
        }
        if (message.commandVerb == "tx.twoTonePreset" && !m_radioModel.isNull()
            && !m_radioModel->receiveOnlyStationPolicy()) {
            const TxDecision decision = txDecisionFor(transport);
            if (!decision.permitted) {
                send(transport, SessionMessages::commandResult(
                    message.commandVerb, message.commandId, false,
                    decision.refusal.text, {}));
                break;
            }
        }
        if ((message.commandVerb == "station.validateSettings"
             || message.commandVerb == "station.forgetSettings"
             || message.commandVerb == "station.repairSettings")
            && (it->agreedMinor < kRadioIdentitySessionProtocolMinor
                || !peerDeclares(transport, QByteArrayLiteral("settingsHygiene"),
                                 message.commandVerb == "station.repairSettings" ? 2 : 1))) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                QStringLiteral("Update this app to use Settings Validation on this Core."), {}));
            break;
        }
        // iPhone app Task 13 (R-IOS-08): the device administration verbs
        // came with deviceAdminVersion 1, for a device at minor 11 that
        // declares deviceAuth (the peers the `devices` object goes to).
        if (isDeviceAdminVerb(message.commandVerb)
            && (it->agreedMinor < kRadioIdentitySessionProtocolMinor
                || !peerDeclares(transport, QByteArrayLiteral("deviceAuth"), 1)
                || deviceAdminVersion() < 1)) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                deviceAdminVersion() < 1
                    ? QStringLiteral("This Core cannot manage its paired devices.")
                    : QStringLiteral("Update this app to manage this Core's paired devices."),
                {}));
            break;
        }
        // iPhone app Task 14 (R-IOS-08): the pairing window's verbs came
        // with pairingVersion 1, for the same peers.
        if (isPairingVerb(message.commandVerb)
            && (it->agreedMinor < kRadioIdentitySessionProtocolMinor
                || !peerDeclares(transport, QByteArrayLiteral("deviceAuth"), 1)
                || pairingVersion() < 1)) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                pairingVersion() < 1
                    ? QStringLiteral("This Core cannot pair new devices.")
                    : QStringLiteral("Update this app to pair new devices with this Core."),
                {}));
            break;
        }
        // Part C follow-up (R-IOS-08): only a device signed in with its own
        // key reopens pairing. A window signed in with the pairing token is
        // refused before the dispatcher sees the verb; pairing.close stays
        // open to it, since closing only narrows who can pair.
        if (message.commandVerb == "pairing.open" && !peerSeesPairingCode(transport)) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                QStringLiteral("Open pairing from a paired device or from the Core's console."),
                {}));
            break;
        }
        // Fix wave, I5 (the iPhone plan's Task 25: selectRadio is for paired
        // devices only): the four radio verbs are refused the same way to a
        // window signed in with the pairing token and no device key. A
        // desktop window signs in with its own key once enrolled.
        if ((message.commandVerb == "station.selectRadio"
             || message.commandVerb == "station.setRadioModel"
             || message.commandVerb == "station.forgetRadio"
             || message.commandVerb == "station.rescanRadios")
            && !peerSeesPairingCode(transport) && !m_tokenSessionsMayChangeRadioForTest) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                StationRadios::pairedDeviceReason(), {}));
            break;
        }
        if ((message.commandVerb == "station.forgetSettings"
             || message.commandVerb == "station.repairSettings")
            && !peerSeesPairingCode(transport)) {
            send(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                StationRadios::pairedDeviceReason(), {}));
            break;
        }
        // Parity ruling C4 (radioHardwareVersion 8): the radio's sample
        // rate is a radio-wide change, so like the Core's radio verbs it is
        // for a paired device, and like the transmit region it waits while
        // the radio is on the air, whoever holds transmit (the change stops
        // the radio's data flow).
        if (message.commandVerb == "setRadioSampleRate") {
            QString refusal;
            if (!peerSeesPairingCode(transport) && !m_tokenSessionsMayChangeRadioForTest) {
                refusal = QStringLiteral("Change the radio's sample rate from a paired device.");
            } else if (!m_radioModel.isNull()) {
                m_radioModel->stationOnAirRefusal(&refusal);
            }
            if (!refusal.isEmpty()) {
                send(transport, SessionMessages::commandResult(
                    message.commandVerb, message.commandId, false, refusal, {}));
                break;
            }
        }
        // Level Cal (radioHardwareVersion 12): the calibration run retunes
        // a slice, switches the preamp and step attenuator and rewrites the
        // station's calibration, so it is for a paired device, and it
        // waits while the radio is on the air, like the Core's other radio
        // verbs (NereusSDR's rule: Thetis runs it from its own console,
        // which is not transmitting while the calibration holds it). Cancel
        // only stops a run and is taken from anyone.
        if (message.commandVerb == "startLevelCalibration") {
            QString refusal;
            if (!peerSeesPairingCode(transport) && !m_tokenSessionsMayChangeRadioForTest) {
                refusal = QStringLiteral("Calibrate the receive level from a paired device.");
            } else if (!m_radioModel.isNull()) {
                m_radioModel->stationOnAirRefusal(&refusal);
            }
            if (!refusal.isEmpty()) {
                send(transport, SessionMessages::commandResult(
                    message.commandVerb, message.commandId, false, refusal, {}));
                break;
            }
        }
        {
            // A revoke of the requester's own device, or a token session
            // retiring the token, ends this connection only after its
            // result (sent synchronously by dispatch()) has gone out.
            const QPointer<StationServer> self(this);
            const QPointer<SessionTransport> asking(transport);
            const quint64 askingSession = peerFor(transport).sessionId;
            const auto pendingEnd = runInvoke(transport, message);
            if (!self || !asking || !hasReplySession(asking, askingSession)) { return; }
            if (pendingEnd) {
                const auto [reason, code] = *pendingEnd;
                dropPeer(asking, reason, true, /*retryable=*/false, code);
                return;
            }
            // Ruling 4.12: the device left on purpose. Its place is free at
            // once, with no away state; the accepted result has gone out,
            // and the Core closes the connection (no session.end: the
            // result already said it).
            const auto leaver = m_peers.constFind(transport);
            if (leaver != m_peers.cend() && leaver->leaving) {
                dropPeer(transport, QString::fromLatin1(kLeftReason), false,
                         /*retryable=*/false);
                return;
            }
        }
        break;
    case SessionMessageKind::MediaControl: {
        // iPhone app Task 76 (the link, section 11): from each admitted
        // session, for its own media.
        const quint64 epoch = peerFor(transport).mediaEpoch;
        if (epoch != 0 && mediaAvailable(epoch)) {
            emit mediaControlReceived(message.mediaPayload, epoch);
        }
        break;
    }
    case SessionMessageKind::PropertyWrite:
        handlePropertyWrite(transport, message);
        break;
    case SessionMessageKind::SettingsWrite:
        handleSettingsWrite(transport, message);
        break;
    case SessionMessageKind::SettingsRemove:
        handleSettingsRemove(transport, message);
        break;
    default:
        // Every remaining kind is daemon-to-client. A client sending one
        // is confused rather than hostile, so it is logged and ignored
        // rather than being grounds to close a working session.
        qCWarning(lcStation) << "Ignoring client message of daemon-only kind:"
                             << SessionMessages::kindName(message.kind);
        break;
    }
}

void StationServer::handleHello(SessionTransport* transport, const SessionMessage& message)
{
    auto it = m_peers.find(transport);
    if (it == m_peers.end()) {
        return;
    }
    if (it->helloReceived) {
        dropPeer(transport, QStringLiteral("This app started connecting twice on one connection."), true, /*retryable=*/false,
                 QString::fromLatin1(SessionEndCode::kProtocolError));
        return;
    }
    it->helloReceived = true;

    // Parent design section 7.0's version policy, both halves, with the
    // iPhone app spec's D23 and D39 (R-IOS-01): the client's hello names
    // the major it chose from this station's list, and the station accepts
    // exactly that. A client that shares no major with this station (two
    // or more apart) has chosen one outside the list, or is an older app
    // on another major, and is refused naming both sides' versions.
    if (!m_supportedMajors.contains(message.protocolMajor)) {
        const QString reason =
            SessionEndReasons::versionRefused(m_supportedMajors, message.supportedMajors);
        qCWarning(lcStation) << "Refusing a client on link major" << message.protocolMajor
                             << "(it supports" << message.supportedMajors
                             << "; this station supports" << m_supportedMajors << "):"
                             << reason;
        // NOT retryable: an incompatible wire contract does not become
        // compatible by being dialed again. The operator has to upgrade
        // one end.
        dropPeer(transport, reason, true, /*retryable=*/false,
                 QString::fromLatin1(SessionEndCode::kLinkVersion));
        return;
    }
    it->agreedMajor = message.protocolMajor;
    it->features = message.features;

    // Equal major, differing minor: negotiate DOWN to the lower of the
    // two. A desktop GUI several releases ahead of a Pi still running this
    // one is the EXPECTED case, and it degrades rather than refusing.
    it->agreedMinor = std::min(kSessionProtocolMinor, message.protocolMinor);

    if (message.settingsSchemaVersion != settingsSchemaVersionOf(m_settings)) {
        // Not a refusal. The settings schema governs how each side's own
        // local store is shaped, not the wire contract, and the client is
        // the side that has to decide what to do about it (see
        // StationClient's own skew check). Logged here so a bench session
        // shows the skew from both ends.
        qCWarning(lcStation) << "Settings schema skew: station is at"
                             << settingsSchemaVersionOf(m_settings) << "client is at"
                             << message.settingsSchemaVersion;
    }

    qCDebug(lcStation) << "Hello from" << message.peerName << "version"
                       << message.protocolMajor << "." << message.protocolMinor
                       << "agreed major" << it->agreedMajor << "agreed minor"
                       << it->agreedMinor << "declares" << it->features.keys();
}

void StationServer::handleAuthRequest(SessionTransport* transport,
                                      const SessionMessage& message)
{
    auto it = m_peers.find(transport);
    if (it == m_peers.end()) {
        return;
    }
    // Final cutover is synchronous but emits direct signals. A reentrant
    // sign-in must wait before proof, auth.result or any admission work;
    // the selected waiter owns the place until its settlement completes.
    if (m_slotReserved && transport != m_reservedTransport) {
        m_deferredAuthRequests.append({transport, message});
        return;
    }
    if (!it->helloReceived) {
        dropPeer(transport, QStringLiteral("This app sent its pairing token out of order."), true, /*retryable=*/false,
                 QString::fromLatin1(SessionEndCode::kProtocolError));
        return;
    }
    // iPhone app Task 14: a pairing connection never signs in; the device
    // signs in on a new connection once paired.
    if (it->authenticated || it->pairing) {
        dropPeer(transport, QStringLiteral("This app sent its pairing token out of order."), true, /*retryable=*/false,
                 QString::fromLatin1(SessionEndCode::kProtocolError));
        return;
    }

    const QString description = it->description;
    const QByteArray challenge = it->challenge;
    const QString address = transport->peerAddress();

    // One refusal shape for every path below: auth.result carrying the
    // reason, the retry advice and the end code, then the close. Nothing
    // here ever logs a candidate token, a signature or a key.
    const auto refuse = [this, transport, &description](const QString& reason, bool retryable,
                                                        const char* code) {
        const QString endCode = code != nullptr ? QString::fromLatin1(code) : QString();
        send(transport, SessionMessages::authResult(false, reason, retryable, endCode));
        qCWarning(lcStation) << "Authentication refused for" << description << ":" << reason;
        dropPeer(transport, reason, false, retryable);
    };

    // iPhone app Task 12 (R-IOS-08): what a device sends, as the
    // authenticator reads it. The address is the peer's own; over the
    // relay it is empty. Through the service the limits also key on the
    // introduction.
    // Task 28 fix wave (review Important 1): over a connection the remote
    // access service introduced, only a paired device's own key signs in
    // (the link document, section 20). A token, or a token enrolling a
    // key, is refused before either limiter sees it.
    if (it->introduced && !(message.device && message.token.isEmpty())) {
        refuse(QStringLiteral("Through the remote access service, a device signs in with its "
                              "own key. Pair this device with the Core first."),
               /*retryable=*/false, SessionEndCode::kProtocolError);
        return;
    }

    DeviceAuthRequest request;
    if (message.device) {
        request.id = message.device->id;
        request.publicKey = message.device->publicKey;
        request.name = message.device->name;
        request.kind = message.device->kind;
        request.signature = message.device->signature;
        request.sourceAddress = address;
        // Task 28 fix wave (review Important 2): the service's introduction,
        // so a replayed one is limited as one.
        request.introduction = it->introductionId;
    }

    QByteArray deviceId;
    if (message.device && message.token.isEmpty()) {
        // ── A paired device signing in with its own key ──
        //
        // The device limiter only: the token's limiter is never consulted,
        // so wrong tokens from anyone cannot lock out a device's key, and
        // this path's failures never count against the token.
        const AuthOutcome outcome = m_deviceAuth->verify(request, challenge, m_certSha256);
        switch (outcome.result) {
        case AuthOutcome::Result::Admitted:
            deviceId = outcome.deviceId;
            break;
        case AuthOutcome::Result::RateLimited:
            // Retryable: it clears by itself after the lockout.
            refuse(QStringLiteral("The Core is refusing sign-ins from this device for a while after too many failed ones. Try again later."),
                   /*retryable=*/true, nullptr);
            return;
        case AuthOutcome::Result::NotPaired:
            refuse(QStringLiteral("This device is not paired with this Core. Pair it first."),
                   /*retryable=*/false, SessionEndCode::kDeviceNotPaired);
            return;
        case AuthOutcome::Result::Proved:
        case AuthOutcome::Result::ProofFailed:
            refuse(QStringLiteral("This device could not prove it is paired with this Core."),
                   /*retryable=*/false, SessionEndCode::kDeviceProofFailed);
            return;
        }
    } else {
        // ── The pairing token (a window from before paired devices) ──
        //
        // A Core with no token (a new one, or one whose token was retired)
        // has nothing to check it against: the window has to pair. Refused
        // before the token's limiter, which a token that cannot succeed
        // must not feed.
        if (!m_tokens->isActive()) {
            refuse(QStringLiteral("This Core uses paired devices. Pair this device first."),
                   /*retryable=*/false, SessionEndCode::kPairingRequired);
            return;
        }
        // LINK minor 5: the token's limiter is kept per source address
        // (empty over the relay).
        const TokenStore::VerifyResult result =
            m_tokens->verify(message.token, transport->peerAddress());
        if (result != TokenStore::VerifyResult::Accepted) {
            // THE distinction TokenStore.h says the two results exist to
            // preserve, carried through to the client's retry policy.
            //
            // RateLimited is retryable: it is transient BY CONSTRUCTION --
            // the lockout expires on TokenStore's own timer, and the
            // refusal text literally says "try again later". The limiter
            // is kept per source address (LINK minor 5), but over the relay
            // every connection shares the empty address, and a lockout
            // refuses a connection "including one carrying the correct
            // token", so bad guesses from a stranger sharing that source
            // can refuse the OPERATOR's token too. Marked permanent, that turned somebody
            // else's failed guesses into the operator being locked out of
            // their own station with no automatic recovery. (A paired
            // device's key is not refused by it: see above.)
            //
            // Rejected is NOT retryable: the token is simply wrong,
            // redialing cannot make it right, and a client that retried
            // forever would feed the very rate limiter above and keep the
            // station locked out on the operator's own behalf.
            if (result == TokenStore::VerifyResult::RateLimited) {
                refuse(QStringLiteral("The Core is refusing pairing tokens for a while after too many wrong ones. Try again later."),
                       /*retryable=*/true, nullptr);
            } else {
                refuse(QStringLiteral("The Core did not accept this app's pairing token. Check the token saved for this Core."),
                       /*retryable=*/false, SessionEndCode::kWrongToken);
            }
            return;
        }
        if (message.device) {
            // The token vouched for the connection; the device block has
            // to prove its key signed THIS connection's transcript before
            // the key is enrolled, so a token holder cannot enrol a key it
            // does not hold. Enrolled once, as a computer, and from then on
            // the window signs in with its key, typing nothing.
            const AuthOutcome proof =
                m_deviceAuth->verifyPossession(request, challenge, m_certSha256);
            if (proof.result != AuthOutcome::Result::Proved) {
                refuse(QStringLiteral("This device could not prove it is paired with this Core."),
                       /*retryable=*/false, SessionEndCode::kDeviceProofFailed);
                return;
            }
            deviceId = proof.deviceId;
            if (!m_devices->find(deviceId)) {
                PairedDevice device;
                device.id = proof.deviceId;
                device.publicKeySpki = proof.publicKeySpki;
                device.name = DeviceStore::isValidName(message.device->name)
                                  ? message.device->name
                                  : QStringLiteral("Computer");
                device.kind = QStringLiteral("computer");
                device.enrolledThroughToken = true;
                // Part C fix wave: its short name, when it sent a usable one.
                if (DeviceStore::isValidShortName(message.device->shortName)) {
                    device.shortName = message.device->shortName;
                }
                if (m_devices->add(device)) {
                    qCInfo(lcStation) << "Enrolled the device key of" << description
                                      << "signing in with the pairing token";
                } else {
                    // The token already admitted the window; not being able
                    // to write the list must not lock it out. It enrols on
                    // a later sign-in.
                    qCWarning(lcStation) << "Could not enrol the device key of" << description;
                    deviceId.clear();
                }
            }
        }
    }

    it = m_peers.find(transport);
    if (it == m_peers.end()) {
        return;
    }
    it->authenticated = true;
    it->deviceId = deviceId;
    // iPhone app Task 13: a token sign-in, enrolled or not, ends when the
    // token is retired.
    it->signedInWithToken = !(message.device && message.token.isEmpty());
    // One change to the devices object for this sign-in, not two; and one
    // to connectedDevices (a new short name, then the admission).
    m_devicesFacade->holdRefresh();
    m_connectedDevices->holdRefresh();
    QString name;
    QString shortName;
    QString kind = QStringLiteral("computer");
    if (!deviceId.isEmpty()) {
        // lastSeen and lastAddress, on every authenticated connection, and
        // the short name the device sent this time (Part C fix wave: it
        // replaces the stored one when usable; outside the signed transcript).
        // Slice control plan Task 8b: and its name, as it sends it now (a
        // window run with a profile names the profile).
        m_devices->touch(deviceId, address,
                         message.device ? message.device->shortName : QString(),
                         message.device ? message.device->name : QString());
        if (const std::optional<PairedDevice> paired = m_devices->find(deviceId)) {
            name = paired->name;
            shortName = paired->shortName;
            kind = paired->kind;
        }
    } else {
        // Ruling 4.1: a window signed in with the older token and no key is
        // a device for the life of its session, named by its address.
        name = DeviceSessionRegistry::tokenWindowName(address);
    }
    send(transport, SessionMessages::authResult(true, QString(), /*retryable=*/false));
    // admit() ends the hold (resumeRefresh) once the device's connected
    // flag is set too, before the snapshot goes out.
    admit(transport, name, shortName, kind);
}

void StationServer::admit(SessionTransport* transport, const QString& name,
                          const QString& shortName, const QString& kind)
{
    // The devices object's hold from handleAuthRequest ends here on every
    // path: one change for a sign-in, as before Task 71.
    const QPointer<StationServer> self(this);
    auto resumeDevices = qScopeGuard([this, self]() {
        if (!self) { return; }
        m_devicesFacade->resumeRefresh();
        if (!self) { return; }
        m_connectedDevices->resumeRefresh();
        if (!self) { return; }
    });
    auto it = m_peers.find(transport);
    if (it == m_peers.end()) {
        return;
    }
    // iPhone app Task 71 (rulings 4.4 and 4.8), after auth.result accepted.
    DeviceSessionRegistry::Entry device;
    if (!it->deviceId.isEmpty()) {
        device.deviceId = it->deviceId;
        device.kind = DeviceSessionRegistry::Kind::Paired;
    } else {
        device.deviceId = m_deviceSessions->nextTokenDeviceId();
        device.kind = DeviceSessionRegistry::Kind::Token;
    }
    device.name = name;
    device.shortName = shortName;
    device.deviceKind = kind;
    // A synchronous registry/slice/end signal during final cutover cannot
    // let another sign-in steal the selected waiter's temporarily free slot.
    if (m_slotReserved && transport != m_reservedTransport) {
        m_deferredAdmissions.append({transport, name, shortName, kind});
        return;
    }
    const QString description = it->description;
    const DeviceSessionRegistry::AdmitResult result = m_deviceSessions->admit(device, transport);
    if (!self) { return; }
    if (result.admission != DeviceSessionRegistry::Admission::Full) {
        // A pending connection for this same key cannot keep a second
        // question after a newer connection has acquired its place.
        QPointer<SessionTransport> older;
        for (const HeldQuestion& q : std::as_const(m_heldQueue)) {
            if (q.deviceId == device.deviceId && q.transport != transport && q.transport) {
                older = q.transport;
                break;
            }
        }
        if (older) dropPeer(older, QString::fromLatin1(kSameDeviceReason), true, false,
                            QString::fromLatin1(SessionEndCode::kSameDevice));
        if (!self) { return; }
    }

    switch (result.admission) {
    case DeviceSessionRegistry::Admission::Full:
        // Every place is taken. Retryable, with no code: the device tries
        // again on its own backoff, and a place may free meanwhile. From
        // Task 41 a device that declared sessionHolder is asked the
        // fifth-device question instead; an older window keeps this.
        qCInfo(lcStation) << "Turning away" << description << ": every place on the Core is taken";
        if (device.kind == DeviceSessionRegistry::Kind::Paired
            && peerHasSessionHolderVersion(transport)) {
            holdForPlace(transport, device.deviceId, name, shortName, kind);
        } else {
            dropPeer(transport, QString::fromLatin1(kOlderWindowCoreFullReason), true,
                     /*retryable=*/true);
        }
        return;
    case DeviceSessionRegistry::Admission::SameDevice: {
        // Ruling 4.8: the device's own older connection ends at once, with
        // no question; its place stays the device's. No other session is
        // touched.
        auto* older = const_cast<SessionTransport*>(
            qobject_cast<const SessionTransport*>(result.replacedSession));
        const auto olderPeer = older != nullptr ? m_peers.find(older) : m_peers.end();
        if (olderPeer != m_peers.end()) {
            olderPeer->placeSettled = true;
            qCInfo(lcStation) << "The device of" << olderPeer->description
                              << "connected again as" << description;
            // NOT retryable: a client that redialled would replace the newer
            // connection of its own device, and the two would trade places.
            // iPhone app plan Task 34 (ruling 8.15): a key on the older
            // connection is stopped through the fence; the hold carries
            // over to the new connection, unkeyed.
            // iPhone app plan Task 39: its older link is gone; a key on it
            // is stopped by the Core.
            const QString stopText =
                noteHolderStopped(device.deviceId, TransmitState::kStopLinkLost);
            if (!self) { return; }
            // Unkey drain review (G-05): that stop is the Core's, so it is
            // Stop All TX, as a dropped link's is. The fence's own unkey (the
            // unkey gate's normal unkey) is the operator's, and would hold
            // the hardware keyed for the queued transmit audio, up to the
            // send ring's length.
            if (!stopText.isEmpty() && m_radioModel) {
                m_radioModel->stopAllTx(stopText);
                if (!self) { return; }
            }
            m_transmitHolder->holderDropped(device.deviceId,
                                            QStringLiteral("This device connected again."));
            if (!self) { return; }
            dropPeer(older, QString::fromLatin1(kSameDeviceReason), true, /*retryable=*/false,
                     QString::fromLatin1(SessionEndCode::kSameDevice));
            if (!self) { return; }
        }
        // Back within its 180 s: still the holder, unkeyed; it keys with its
        // next press.
        m_transmitHolder->holderReturned(device.deviceId);
        if (!self) { return; }
        break;
    }
    case DeviceSessionRegistry::Admission::Admitted:
        break;
    }

    it = m_peers.find(transport);
    if (it == m_peers.end()) {
        return;
    }
    it->sessionDeviceId = device.deviceId;
    // iPhone app Task 73 (ruling 4.8): a device back to its own place keeps
    // its slices, pans and active slice as they are.
    it->returning = result.admission == DeviceSessionRegistry::Admission::SameDevice;
    const bool returning = it->returning;
    publishConnectedDevices();
    if (!self) { return; }
    // iPhone app Task 73 (ruling 5.2): the device's slices, before its own
    // burst so the burst carries them, and within the held refresh so its
    // sign-in is still one change to `connectedDevices`. Other views see
    // them as markers.
    if (!returning) {
        placeSlicesForAdmission(device.deviceId);
        if (!self) { return; }
        if (!m_peers.contains(transport)) {
            return;
        }
        // iPhone app Task 74 (ruling 10.2): an older window needs a slice
        // of its own; with none to give it, it is refused, retryable, in
        // words that name the limit that is full.
        const QString noSlice = olderWindowWithoutSliceReason(transport);
        if (!noSlice.isEmpty()) {
            m_peers[transport].leaving = true;
            dropPeer(transport, noSlice, true, /*retryable=*/true);
            return;
        }
    }
    resumeDevices.dismiss();
    m_devicesFacade->resumeRefresh();
    if (!self) { return; }
    m_connectedDevices->resumeRefresh();
    if (!self) { return; }
    promoteToSession(transport);
    if (!self) { return; }
    // iPhone app Task 74 (7.4): after snapshot.complete, graceEnded or
    // slicesNotRestored, then the notices that waited while it was away.
    if (m_peers.contains(transport)) {
        deliverAdmissionNotices(transport, result.timeRanOutAtMs);
        if (!self) { return; }
    }
    if (result.placeTaken && m_peers.contains(transport)) {
        SessionPrompt notice;
        notice.id = m_nextSessionId++;
        notice.kind = QStringLiteral("placeTaken");
        notice.secondsAgo = std::max<qint64>(0, (m_deviceSessions->now() - result.placeTaken->atMs) / 1000);
        notice.byDeviceId = StationIdentity::toBase64Url(result.placeTaken->byId);
        notice.byName = result.placeTaken->byName;
        send(transport, SessionMessages::notice(notice,
            QStringLiteral("Your place on the Core was taken.")));
    }
}

void StationServer::holdForPlace(SessionTransport* transport, const QByteArray& deviceId,
                                 const QString& name, const QString& shortName,
                                 const QString& kind)
{
    const QPointer<StationServer> self(this);
    auto peer = m_peers.find(transport);
    if (peer == m_peers.end() || !peer->authenticated || peer->deviceId != deviceId
        || !peerHasSessionHolderVersion(transport)) {
        return;
    }
    HeldQuestion question;
    question.transport = transport;
    question.deviceId = deviceId;
    question.name = name;
    question.shortName = shortName;
    question.kind = kind;
    question.serial = m_nextHeldSerial++;
    question.deadlineMs = m_deviceSessions->now() + kTakeoverAnswerMs;
    int position = m_heldQueue.size();
    SessionTransport* old = nullptr;
    for (int i = 0; i < m_heldQueue.size(); ++i) {
        if (m_heldQueue.at(i).deviceId == deviceId) {
            position = i;
            old = m_heldQueue.at(i).transport;
            question.deadlineMs = m_heldQueue.at(i).deadlineMs;
            break;
        }
    }
    if (old) {
        auto older = m_peers.find(old);
        if (older != m_peers.end()) older->heldSerial = 0;
        m_heldQueue[position] = question;
    } else {
        m_heldQueue.append(question);
    }
    peer = m_peers.find(transport);
    if (peer == m_peers.end()) return;
    peer->heldSerial = question.serial;
    if (peer->authDeadline) peer->authDeadline->stop();
    if (old) {
        dropPeer(old, QString::fromLatin1(kSameDeviceReason), true, false,
                 QString::fromLatin1(SessionEndCode::kSameDevice));
        if (!self) return;
    }
    if (m_peers.contains(transport)) sendHeld(transport);
    if (!self) return;
    auto* answerTimer = new QTimer(this);
    answerTimer->setSingleShot(true);
    connect(answerTimer, &QTimer::timeout, this, [this, answerTimer, serial = question.serial]() {
        answerTimer->deleteLater();
        for (const HeldQuestion& q : std::as_const(m_heldQueue)) {
            if (q.serial == serial && q.transport && m_deviceSessions->now() >= q.deadlineMs) {
                dropPeer(q.transport, QString::fromLatin1(kCoreFullReason), true, false,
                         QString::fromLatin1(SessionEndCode::kCoreFull));
                return;
            }
        }
    });
    answerTimer->start(static_cast<int>(std::max<qint64>(0, question.deadlineMs - m_deviceSessions->now())));
}

QJsonArray StationServer::heldCandidates() const
{
    const QJsonArray connected = QJsonDocument::fromJson(m_connectedDevices->listJson().toUtf8()).array();
    QHash<QString, QJsonObject> byId;
    for (const QJsonValue& value : connected) {
        const QJsonObject object = value.toObject();
        byId.insert(object.value(QStringLiteral("deviceId")).toString(), object);
    }
    const auto holder = m_transmitHolder->holder();
    const QByteArray onAir = holder && holder->keyed ? holder->deviceId : QByteArray();
    QJsonArray ordered;
    const qint64 now = m_deviceSessions->now();
    for (const auto& entry : m_deviceSessions->replacementCandidates(onAir)) {
        const QString wireId = entry.kind == DeviceSessionRegistry::Kind::Token
            ? QString::fromLatin1(entry.deviceId)
            : StationIdentity::toBase64Url(entry.deviceId);
        QJsonObject object = byId.value(wireId);
        if (object.isEmpty()) continue;
        object.remove(QStringLiteral("paired"));
        object.remove(QStringLiteral("hostsCore"));
        object.remove(QStringLiteral("revocable"));
        object.insert(QStringLiteral("replaceable"),
                      entry.kind != DeviceSessionRegistry::Kind::Hosting);
        object.insert(QStringLiteral("lastActivitySeconds"),
                      onAir == entry.deviceId ? 0 : std::max<qint64>(0, (now - entry.lastActivityMs) / 1000));
        QJsonArray listening;
        for (const QJsonValue& sliceValue : object.value(QStringLiteral("listeningOn")).toArray()) {
            QJsonObject slice = sliceValue.toObject();
            const SliceModel* live = m_radioModel
                ? m_radioModel->sliceById(slice.value(QStringLiteral("sliceId")).toInt(-1)) : nullptr;
            if (live) slice.insert(QStringLiteral("frequencyHz"), live->frequency());
            listening.append(slice);
        }
        object.insert(QStringLiteral("listeningOn"), listening);
        if (object.contains(QStringLiteral("transmittingOn"))) {
            QJsonObject slice = object.value(QStringLiteral("transmittingOn")).toObject();
            const SliceModel* live = m_radioModel
                ? m_radioModel->sliceById(slice.value(QStringLiteral("sliceId")).toInt(-1)) : nullptr;
            if (live) slice.insert(QStringLiteral("frequencyHz"), live->frequency());
            object.insert(QStringLiteral("transmittingOn"), slice);
        }
        QString from = QStringLiteral("relay");
        if (entry.session) {
            auto* live = const_cast<SessionTransport*>(qobject_cast<const SessionTransport*>(entry.session));
            if (live && m_peers.contains(live) && !live->peerAddress().isEmpty()) {
                from = live->peerAddress();
            }
        }
        object.insert(QStringLiteral("from"), from);
        ordered.append(object);
    }
    return ordered;
}

void StationServer::sendHeld(SessionTransport* transport)
{
    auto peer = m_peers.constFind(transport);
    if (peer == m_peers.cend() || peer->heldSerial == 0) return;
    const QJsonArray candidates = heldCandidates();
    QJsonArray stable;
    for (const QJsonValue& value : candidates) {
        QJsonObject object = value.toObject();
        for (const char* key : {"lastActivitySeconds", "connectedForSeconds",
                                "awayForSeconds", "transmittingForSeconds"}) {
            object.remove(QLatin1String(key));
        }
        stable.append(object);
    }
    QByteArray signature = QJsonDocument(stable).toJson(QJsonDocument::Compact);
    signature += ':' + QByteArray::number(m_deviceSessions->revision());
    if (signature != m_heldSignature) {
        m_heldSignature = signature;
        ++m_heldRevision;
    }
    std::optional<QJsonObject> taken;
    std::optional<QJsonObject> freed;
    if (const auto record = m_deviceSessions->placeTakenBy(peer->deviceId)) {
        taken = QJsonObject{{QStringLiteral("byName"), record->byName},
                            {QStringLiteral("byId"), StationIdentity::toBase64Url(record->byId)},
                            {QStringLiteral("secondsAgo"),
                             std::max<qint64>(0, (m_deviceSessions->now() - record->atMs) / 1000)}};
    } else if (const auto at = m_deviceSessions->timeRanOutAtMs(peer->deviceId)) {
        freed = QJsonObject{{QStringLiteral("secondsAgo"),
                             std::max<qint64>(0, (m_deviceSessions->now() - *at) / 1000)}};
    }
    send(transport, SessionMessages::sessionHeld(candidates, m_heldRevision, taken, freed));
}

void StationServer::refreshHeld()
{
    const QPointer<StationServer> self(this);
    const QList<HeldQuestion> questions = m_heldQueue;
    for (const HeldQuestion& q : questions) {
        if (q.transport && m_peers.contains(q.transport)
            && peerFor(q.transport).heldSerial == q.serial) {
            if (m_deviceSessions->now() >= q.deadlineMs) {
                dropPeer(q.transport, QString::fromLatin1(kCoreFullReason), true, false,
                         QString::fromLatin1(SessionEndCode::kCoreFull));
            } else {
                sendHeld(q.transport);
            }
            if (!self) return;
        }
    }
}

void StationServer::drainHeld()
{
    if (m_closing || m_settlingTakeover || m_slotReserved || m_dropPeerDepth > 0) return;
    const QPointer<StationServer> self(this);
    m_settlingTakeover = true;
    while (m_deviceSessions->hasPlaceFree() && !m_heldQueue.isEmpty()) {
        const HeldQuestion q = m_heldQueue.takeFirst();
        if (!q.transport) continue;
        auto peer = m_peers.find(q.transport);
        if (peer == m_peers.end() || peer->heldSerial != q.serial) continue;
        if (m_deviceSessions->now() >= q.deadlineMs) {
            dropPeer(q.transport, QString::fromLatin1(kCoreFullReason), true, false,
                     QString::fromLatin1(SessionEndCode::kCoreFull));
            if (!self) return;
            continue;
        }
        peer->heldSerial = 0;
        admit(q.transport, q.name, q.shortName, q.kind);
        if (!self) return;
    }
    m_settlingTakeover = false;
    refreshHeld();
}

void StationServer::handleTakeover(SessionTransport* transport, const SessionMessage& message)
{
    const QPointer<StationServer> self(this);
    auto peer = m_peers.constFind(transport);
    if (peer == m_peers.cend() || peer->heldSerial == 0 || !peer->authenticated
        || peer->deviceId.isEmpty() || !peerHasSessionHolderVersion(transport)) return;
    const quint64 serial = peer->heldSerial;
    auto question = std::find_if(m_heldQueue.cbegin(), m_heldQueue.cend(),
                                 [serial](const HeldQuestion& q) { return q.serial == serial; });
    if (question == m_heldQueue.cend()) return;
    if (m_deviceSessions->now() >= question->deadlineMs || message.takeoverDeviceId.isEmpty()) {
        dropPeer(transport, QString::fromLatin1(kCoreFullReason), true, false,
                 QString::fromLatin1(SessionEndCode::kCoreFull));
        return;
    }
    if (m_activeTakeoverSerial != 0) {
        sendHeld(transport);
        return;
    }
    // Rebuild before comparing: registry activity, holder state and a path
    // move can invalidate what the app last displayed.
    sendHeld(transport);
    if (!self) return;
    if (!m_peers.contains(transport) || peerFor(transport).heldSerial != serial) return;
    if (message.heldRevision != m_heldRevision) return;
    const QJsonArray candidates = heldCandidates();
    bool selectable = false;
    for (const QJsonValue& value : candidates) {
        const QJsonObject candidate = value.toObject();
        if (candidate.value(QStringLiteral("deviceId")).toString() == message.takeoverDeviceId) {
            selectable = candidate.value(QStringLiteral("replaceable")).toBool();
            break;
        }
    }
    if (!selectable) {
        sendHeld(transport);
        return;
    }
    const QByteArray targetId = message.takeoverDeviceId.startsWith(QStringLiteral("token:"))
        ? message.takeoverDeviceId.toLatin1()
        : StationIdentity::fromBase64Url(message.takeoverDeviceId);
    const auto target = m_deviceSessions->entry(targetId);
    if (!target || target->kind == DeviceSessionRegistry::Kind::Hosting) {
        sendHeld(transport);
        return;
    }
    QPointer<SessionTransport> incumbent;
    const bool originalLive = target->state == DeviceSessionRegistry::State::Listening;
    const QObject* originalSession = target->session;
    if (target->session) {
        incumbent = const_cast<SessionTransport*>(qobject_cast<const SessionTransport*>(target->session));
        if (!incumbent || !m_peers.contains(incumbent)) {
            sendHeld(transport);
            return;
        }
    }
    // This state is established before transferTo: its completion may run
    // inline, including after an immediate refusal of a competing transfer.
    m_activeTakeoverSerial = serial;
    m_settlingTakeover = true;
    const auto holder = m_transmitHolder->holder();
    m_takeoverRequiredUnkey = holder && holder->deviceId == targetId;
    m_takeoverHolderEpoch = m_transmitHolder->epoch();
    if (m_takeoverRequiredUnkey) {
        m_transmitHolder->transferTo(std::nullopt,
            QStringLiteral("A device is taking this place on the Core."),
            [self = QPointer<StationServer>(this), serial, targetId, incumbent,
             originalSession, originalLive](bool released) {
                if (self) self->finishTakeover(serial, targetId, incumbent,
                                               originalSession, originalLive, released);
            });
    } else {
        finishTakeover(serial, targetId, incumbent, originalSession, originalLive, true);
    }
}

void StationServer::finishTakeover(quint64 serial, const QByteArray& targetId,
                                   QPointer<SessionTransport> incumbent,
                                   const QObject* originalSession, bool originalLive,
                                   bool released)
{
    if (m_activeTakeoverSerial != serial) return;
    // The scoped barrier starts before any final read or callback-bearing
    // mutation. New keys, takes and transfers to another holder cannot
    // cross the cutover, while an unrelated holder is left alone.
    const QPointer<StationServer> self(this);
    m_transmitHolder->runWithKeyingBlocked([this, self, serial, targetId, incumbent,
                                           originalSession, originalLive, released]() {
        if (!self) return;
        auto finish = qScopeGuard([this, self]() {
            if (!self) return;
            m_reservedTransport.clear();
            m_slotReserved = false;
            m_activeTakeoverSerial = 0;
            m_settlingTakeover = false;
            drainHeld();
            if (!self) return;
            const QList<DeferredAdmission> deferred = std::exchange(m_deferredAdmissions, {});
            for (const DeferredAdmission& entry : deferred) {
                if (entry.transport && m_peers.contains(entry.transport)) {
                    admit(entry.transport, entry.name, entry.shortName, entry.kind);
                    if (!self) return;
                }
            }
            const QList<DeferredAuthRequest> authRequests =
                std::exchange(m_deferredAuthRequests, {});
            for (const DeferredAuthRequest& entry : authRequests) {
                if (entry.transport && m_peers.contains(entry.transport)) {
                    handleAuthRequest(entry.transport, entry.message);
                    if (!self) return;
                }
            }
            refreshHeld();
        });
        auto question = std::find_if(m_heldQueue.cbegin(), m_heldQueue.cend(),
                                     [serial](const HeldQuestion& q) { return q.serial == serial; });
        if (!released || question == m_heldQueue.cend() || !question->transport
            || m_deviceSessions->now() >= question->deadlineMs) return;
        const HeldQuestion selection = *question;
        const QPointer<SessionTransport> taker = selection.transport;
        const auto takerPeer = m_peers.constFind(taker);
        if (takerPeer == m_peers.cend() || takerPeer->heldSerial != serial
            || !takerPeer->authenticated || takerPeer->deviceId != selection.deviceId
            || !peerHasSessionHolderVersion(taker)) return;
        const auto target = m_deviceSessions->entry(targetId);
        if (m_deviceSessions->hasPlaceFree() || !target
            || target->kind == DeviceSessionRegistry::Kind::Hosting
            || (originalLive && (!incumbent || target->session != originalSession
                                 || target->state != DeviceSessionRegistry::State::Listening))
            || (!originalLive && (target->session || target->state != DeviceSessionRegistry::State::Away))) return;
        const auto holder = m_transmitHolder->holder();
        if (m_transmitHolder->state() == TransmitHolder::State::Transferring
            || (holder && holder->deviceId == targetId)
            || m_transmitHolder->epoch() != m_takeoverHolderEpoch + (m_takeoverRequiredUnkey ? 1 : 0)) return;
        const MoxController* mox = m_radioModel ? m_radioModel->moxController() : nullptr;
        if (m_takeoverRequiredUnkey && mox && (mox->isMox() || mox->state() != MoxState::Rx)) return;
        const auto takerWords = m_connectedDevices->describe(selection.deviceId);
        const QString takerName = takerWords ? takerWords->name : selection.name;
        const QString takerWireId = StationIdentity::toBase64Url(selection.deviceId);
        const QString reason = QStringLiteral("%1 took this device's place on the Core.").arg(takerName);
        m_reservedTransport = taker;
        m_slotReserved = true;
        // The replacement commits here, immediately before the first
        // destructive target action. A disconnected taker after this point
        // cannot restore the incumbent's slices or place.
        saveTakenSlicesFor(targetId);
        if (!self) return;
        releaseDeviceClaims(targetId, std::nullopt);
        if (!self) return;
        const auto afterSlices = m_deviceSessions->entry(targetId);
        if (!afterSlices) return;
        if (afterSlices->session != originalSession
            && !(originalLive && afterSlices->state == DeviceSessionRegistry::State::Away
                 && !afterSlices->session)) return;
        if (incumbent && m_peers.contains(incumbent)) {
            m_peers[incumbent].placeSettled = true;
        }
        m_deviceSessions->replace(targetId, selection.deviceId, takerName);
        if (!self) return;
        if (incumbent && m_peers.contains(incumbent)) {
            SessionMessage end = SessionMessages::sessionEnd(
                reason, false, QString::fromLatin1(SessionEndCode::kTakenOver));
            end.takenOverBy = takerName;
            end.takenOverById = takerWireId;
            end.secondsAgo = 0;
            send(incumbent, end);
            if (!self) return;
            if (m_peers.contains(incumbent)) dropPeer(incumbent, reason, false, false);
            if (!self) return;
        }
        // The selected taker owns this free place throughout the signals
        // above; retire its question immediately before ordinary admission.
        auto current = std::find_if(m_heldQueue.begin(), m_heldQueue.end(),
                                    [serial](const HeldQuestion& q) { return q.serial == serial; });
        if (current == m_heldQueue.end() || !taker || !m_peers.contains(taker)
            || peerFor(taker).heldSerial != serial) return;
        const HeldQuestion selected = *current;
        m_heldQueue.erase(current);
        m_peers[taker].heldSerial = 0;
        admit(taker, selected.name, selected.shortName, selected.kind);
    });
}

// ── Pairing (iPhone app Task 14, R-IOS-08) ───────────────────────────────
//
// A device that is not paired sends pair.start after its hello instead of
// auth.request (the link document's Pairing section). Nothing on a pairing
// connection signs in: it ends after pair.accept, the Core's pair.confirm,
// or pair.fail, and the device then signs in by key on a new connection.
//
//   lan   one tap: only while the Core is unclaimed, only when
//         pairing_lan_click allows it, and only from an address on one of
//         the Core's directly connected networks. The device is added and
//         pair.accept carries the Core's identity and label.
//   code  SPAKE2+EE over the window's current code. The Core sends step 0,
//         takes the code when the device's step 1 arrives (from there it
//         is paired with or burned), answers step 2, checks step 3 (step
//         4), opens the device's box (its key, name and kind, which win
//         over pair.start's name and kind, and its key must be the one
//         pair.start named), adds the device and answers with its own box
//         (identity and label).
//
// Nothing here logs the code, a key or a box.

void StationServer::sendPairFail(SessionTransport* transport, const QString& reason,
                                 qint64 retryAfterMs)
{
    send(transport, SessionMessages::pairFail(reason, std::max<qint64>(0, retryAfterMs)));
    qCInfo(lcStation) << "Pairing refused for" << peerFor(transport).description << ":"
                      << reason;
    // A code this connection took is burned here (dropPeer).
    dropPeer(transport, reason, false, /*retryable=*/false);
}

void StationServer::startPairingHash()
{
    // One hash at a time; a finished one for an old code starts the next.
    if (m_pairingHashThread) {
        return;
    }
    const QString code = m_pairingWindow->currentCode();
    if (code.isEmpty()) {
        return;
    }
    const quint64 serial = m_pairingWindow->codeSerial();
    m_pairingHashSerial = serial;
    auto result = std::make_shared<QByteArray>();
    // The code and the hash cross to the worker and back only in memory,
    // never in a log line; the worker's copy of the code is overwritten
    // once it is hashed.
    auto codeCopy = std::make_shared<QString>(code);
    auto* worker = QThread::create([result, codeCopy, hasher = m_pairingHasher]() {
        *result = hasher(*codeCopy);
        codeCopy->fill(QChar(u'\0'));
    });
    worker->setObjectName(QStringLiteral("StationPairingHash"));
    m_pairingHashThread.reset(worker);
    connect(worker, &QThread::finished, this, [this, worker, serial, result]() {
        if (m_pairingHashThread.get() != worker) {
            return;
        }
        worker->wait();
        m_pairingHashThread.reset();
        finishPairingHash(serial, *result);
        SpakeExchange::wipe(*result);
    });
    worker->start();
}

void StationServer::finishPairingHash(quint64 serial, const QByteArray& stored)
{
    // Kept only while its code is still the window's current one.
    if (serial == m_pairingWindow->codeSerial() && !m_pairingWindow->currentCode().isEmpty()
        && !stored.isEmpty()) {
        SpakeExchange::wipe(m_pairingStored);
        m_pairingStored = stored;
        m_pairingStoredSerial = serial;
    }
    bool anotherNeeded = false;
    const QList<SessionTransport*> transports = m_peers.keys();
    for (SessionTransport* transport : transports) {
        const auto it = m_peers.constFind(transport);
        if (it == m_peers.cend() || !it->pairing || !it->pairing->awaitingHash) {
            continue;
        }
        const std::shared_ptr<PairingAttempt> attempt = it->pairing;
        if (attempt->codeSerial == m_pairingStoredSerial && !m_pairingStored.isEmpty()) {
            attempt->awaitingHash = false;
            beginCodeExchange(transport);
        } else if (attempt->codeSerial != m_pairingWindow->codeSerial()) {
            attempt->awaitingHash = false;
            sendPairFail(transport,
                         QStringLiteral("The pairing code changed. Enter the code the Core "
                                        "shows now."),
                         m_pairingWindow->retryAfterMs(attempt->route));
        } else if (serial == attempt->codeSerial) {
            // Its own code's hash came back empty.
            attempt->awaitingHash = false;
            sendPairFail(transport, QStringLiteral("This Core cannot pair new devices."), 0);
        } else {
            anotherNeeded = true;
        }
    }
    if (anotherNeeded) {
        startPairingHash();
    }
}

void StationServer::beginCodeExchange(SessionTransport* transport)
{
    const auto it = m_peers.constFind(transport);
    if (it == m_peers.cend() || !it->pairing) {
        return;
    }
    const std::shared_ptr<PairingAttempt> attempt = it->pairing;
    attempt->exchange = std::make_unique<SpakeExchange>(SpakeExchange::Role::Station);
    const QByteArray step0 = attempt->exchange->stationStep0(m_pairingStored);
    if (step0.isEmpty()) {
        sendPairFail(transport, QStringLiteral("This Core cannot pair new devices."), 0);
        return;
    }
    attempt->expecting = 1;
    send(transport, SessionMessages::pairSpake(0, StationIdentity::toBase64Url(step0)));
}

#ifdef NEREUS_BUILD_TESTS
void StationServer::setPairingHasherForTest(std::function<QByteArray(const QString&)> hasher)
{
    m_pairingHasher = hasher ? std::move(hasher) : &SpakeExchange::storedData;
}

bool StationServer::isHashingPairingCodeForTest() const
{
    return m_pairingHashThread != nullptr;
}
#endif

void StationServer::handlePairStart(SessionTransport* transport, const SessionMessage& message)
{
    auto it = m_peers.find(transport);
    if (it == m_peers.end()) {
        return;
    }
    if (!it->helloReceived || it->authenticated || it->pairing) {
        dropPeer(transport, QStringLiteral("This app started pairing out of order."), true,
                 /*retryable=*/false, QString::fromLatin1(SessionEndCode::kProtocolError));
        return;
    }
    auto attempt = std::make_shared<PairingAttempt>();
    it->pairing = attempt;
    attempt->route = it->mailboxPairing ? PairingWindow::Route::Service
                                        : PairingWindow::Route::Direct;
    const QString address = transport->peerAddress();

    if (pairingVersion() < 1) {
        sendPairFail(transport, QStringLiteral("This Core cannot pair new devices."), 0);
        return;
    }
    // The device, as pair.start names it. In code mode the name and kind
    // inside its box win; its key must be this one.
    const SessionPairDevice device = message.pairDevice.value_or(SessionPairDevice{});
    bool keyOk = false;
    attempt->publicKeyText = device.publicKey;
    attempt->publicKeySpki = StationIdentity::fromBase64Url(device.publicKey, &keyOk);
    attempt->name = device.name;
    attempt->kind = device.kind;
    if (!keyOk || !StationIdentity::isP256Spki(attempt->publicKeySpki)
        || !DeviceStore::isValidName(attempt->name) || !DeviceStore::isKnownKind(attempt->kind)) {
        sendPairFail(transport,
                     QStringLiteral("The Core could not read this device's details. Update "
                                    "this app."),
                     0);
        return;
    }
    attempt->existingDevice =
        m_devices->find(StationIdentity::fingerprintOf(attempt->publicKeySpki));
    if (attempt->existingDevice && message.pairMode != QLatin1String("code")) {
        sendPairFail(transport,
                     QStringLiteral("This device is already paired with this Core. Connect "
                                    "to it instead."),
                     0);
        return;
    }
    if (!m_pairingWindow->isOpen()) {
        sendPairFail(transport,
                     QStringLiteral("This Core is not taking new devices. Open pairing on the "
                                    "Core or on a paired device first."),
                     0);
        return;
    }

    if (message.pairMode == QLatin1String("lan")) {
        // ── One tap ──
        if (m_pairingWindow->state() != PairingWindow::State::OpenUnclaimed) {
            sendPairFail(transport,
                         QStringLiteral("One tap pairs only a Core with no paired devices. Use "
                                        "the pairing code the Core shows."),
                         0);
            return;
        }
        if (!m_pairingLanClickAllowed) {
            sendPairFail(transport,
                         QStringLiteral("This Core pairs only with its code. Use the pairing "
                                        "code the Core shows."),
                         0);
            return;
        }
        if (!isOnDirectNetwork(address)) {
            sendPairFail(transport,
                         QStringLiteral("One tap works only on the Core's own network. Use the "
                                        "pairing code the Core shows."),
                         0);
            return;
        }
        PairedDevice paired;
        paired.id = StationIdentity::fingerprintOf(attempt->publicKeySpki);
        paired.publicKeySpki = attempt->publicKeySpki;
        paired.name = attempt->name;
        paired.kind = attempt->kind;
        paired.lastAddress = address;
        if (!m_devices->add(paired)) {
            sendPairFail(transport,
                         QStringLiteral("The Core could not save this device. Try again."), 0);
            return;
        }
        attempt->finished = true;
        m_pairingWindow->pairingSucceeded();
        qCInfo(lcStation) << "Paired a device by one tap from" << it->description;
        send(transport,
             SessionMessages::pairAccept(
                 SessionStationIdentity{StationIdentity::toBase64Url(m_identity->publicKeySpki()),
                                        StationIdentity::toBase64Url(m_certBinding)},
                 m_devicesFacade->stationLabel()));
        dropPeer(transport, QStringLiteral("This device is paired with the Core."), false,
                 /*retryable=*/false);
        return;
    }

    // ── The code ──
    attempt->codeMode = true;
    // The ruling on Task 27 item I5: after wrong codes in a row through the
    // remote access service, pairing through it pauses for a while. Refused
    // before any code is taken, so it burns nothing; a direct connection is
    // not paused.
    // LINK-I4 (JJ's ruling, 2026-09-30): after too many wrong codes through
    // the service in total, pairing through it is off until the Core
    // reopens pairing. No time to try again: waiting does not help.
    if (attempt->route == PairingWindow::Route::Service && m_pairingWindow->isServiceShut()) {
        sendPairFail(transport,
                     QStringLiteral("The Core has turned off pairing from outside its network "
                                    "after too many wrong codes. Pair on the Core's own "
                                    "network, or reopen pairing at the Core."),
                     0);
        return;
    }
    if (m_pairingWindow->isPaused(attempt->route)) {
        sendPairFail(transport,
                     QStringLiteral("The Core has paused pairing from outside its network "
                                    "after several wrong codes. Try again later, or pair on "
                                    "the Core's own network."),
                     m_pairingWindow->retryAfterMs(attempt->route));
        return;
    }
    if (m_pairingWindow->codeInUse()) {
        sendPairFail(transport,
                     QStringLiteral("Another device is pairing with this Core right now. Try "
                                    "again shortly."),
                     m_pairingWindow->retryAfterMs(attempt->route));
        return;
    }
    if (m_pairingWindow->currentCode().isEmpty()) {
        sendPairFail(transport,
                     QStringLiteral("The Core is waiting before it shows a new pairing code. "
                                    "Try again when the new code appears."),
                     m_pairingWindow->retryAfterMs(attempt->route));
        return;
    }
    attempt->codeSerial = m_pairingWindow->codeSerial();
    if (m_pairingStoredSerial == attempt->codeSerial && !m_pairingStored.isEmpty()) {
        beginCodeExchange(transport);
        return;
    }
    // The code is hashed once (Argon2id) on a worker, never on this event
    // loop, so no other device's session waits on it; step 0 follows when
    // the hash is back (finishPairingHash).
    attempt->awaitingHash = true;
    startPairingHash();
}

void StationServer::handlePairSpake(SessionTransport* transport, const SessionMessage& message)
{
    auto it = m_peers.find(transport);
    if (it == m_peers.end()) {
        return;
    }
    const std::shared_ptr<PairingAttempt> attempt = it->pairing;
    if (!attempt || !attempt->codeMode || !attempt->exchange || attempt->finished
        || attempt->awaitingHash || message.pairStep != attempt->expecting) {
        dropPeer(transport, QStringLiteral("This app sent a pairing step out of order."), true,
                 /*retryable=*/false, QString::fromLatin1(SessionEndCode::kProtocolError));
        return;
    }
    bool ok = false;
    const QByteArray data = StationIdentity::fromBase64Url(message.pairData, &ok);

    if (attempt->expecting == 1) {
        // The Core commits to the code here: the window gives it to this
        // exchange once, and from now on it is paired with or burned.
        if (m_pairingStoredSerial != attempt->codeSerial || m_pairingStored.isEmpty()) {
            sendPairFail(transport,
                         QStringLiteral("The pairing code changed. Enter the code the Core "
                                        "shows now."),
                         m_pairingWindow->retryAfterMs(attempt->route));
            return;
        }
        QByteArray stored = m_pairingStored;
        if (!m_pairingWindow->takeCode(attempt->codeSerial)) {
            SpakeExchange::wipe(stored);
            sendPairFail(transport,
                         QStringLiteral("Another device is pairing with this Core right now. "
                                        "Try again shortly."),
                         m_pairingWindow->retryAfterMs(attempt->route));
            return;
        }
        attempt->codeTaken = true;
        const QByteArray step2 =
            ok ? attempt->exchange->stationStep2(stored, data) : QByteArray();
        SpakeExchange::wipe(stored);
        if (step2.isEmpty()) {
            attempt->finished = true;
            m_pairingWindow->pairingFailed(attempt->route);
            sendPairFail(transport,
                         QStringLiteral("The pairing code was not accepted. A new code will "
                                        "appear on the Core."),
                         m_pairingWindow->retryAfterMs(attempt->route));
            return;
        }
        attempt->expecting = 3;
        send(transport, SessionMessages::pairSpake(2, StationIdentity::toBase64Url(step2)));
        return;
    }

    // Step 3: the device shows it held the same code (step 4).
    if (!ok || !attempt->exchange->stationStep4(data)) {
        attempt->finished = true;
        m_pairingWindow->pairingFailed(attempt->route);
        sendPairFail(transport,
                     QStringLiteral("The pairing code was not right. A new code will appear on "
                                    "the Core."),
                     m_pairingWindow->retryAfterMs(attempt->route));
        return;
    }
    attempt->expecting = 4;   // its box
}

void StationServer::handlePairConfirm(SessionTransport* transport, const SessionMessage& message)
{
    auto it = m_peers.find(transport);
    if (it == m_peers.end()) {
        return;
    }
    const std::shared_ptr<PairingAttempt> attempt = it->pairing;
    if (!attempt || !attempt->codeMode || !attempt->exchange || attempt->finished
        || attempt->expecting != 4) {
        dropPeer(transport, QStringLiteral("This app sent a pairing step out of order."), true,
                 /*retryable=*/false, QString::fromLatin1(SessionEndCode::kProtocolError));
        return;
    }
    const auto refuse = [this, transport, &attempt](const QString& reason) {
        attempt->finished = true;
        m_pairingWindow->pairingFailed(attempt->route);
        sendPairFail(transport, reason, m_pairingWindow->retryAfterMs(attempt->route));
    };
    // Part C fix wave (R1-M1): a window that closed (the operator's close,
    // its ten minutes, or a close and reopen) since this exchange took the
    // code pairs nothing; the code is burned.
    if (!m_pairingWindow->holdsCode(attempt->codeSerial)) {
        refuse(QStringLiteral("This Core is not taking new devices. Open pairing on the Core or "
                              "on a paired device first."));
        return;
    }
    bool ok = false;
    const QByteArray box = StationIdentity::fromBase64Url(message.pairBox, &ok);
    const std::optional<QByteArray> plain =
        ok ? attempt->exchange->openConfirmation(box) : std::nullopt;
    const QJsonObject contents =
        plain ? QJsonDocument::fromJson(*plain).object() : QJsonObject{};
    const QString boxKey = contents.value(QStringLiteral("publicKey")).toString();
    const QString name = contents.value(QStringLiteral("name")).toString();
    const QString kind = contents.value(QStringLiteral("kind")).toString();
    // The key the device pairs is the one pair.start named, now vouched for
    // by the code; the name and kind are the box's.
    if (!plain || boxKey != attempt->publicKeyText || !DeviceStore::isValidName(name)
        || !DeviceStore::isKnownKind(kind)) {
        refuse(QStringLiteral("The Core could not read this device's details. Update this "
                              "app."));
        return;
    }
    PairedDevice paired;
    paired.id = StationIdentity::fingerprintOf(attempt->publicKeySpki);
    paired.publicKeySpki = attempt->publicKeySpki;
    paired.name = name;
    paired.kind = kind;
    paired.lastAddress = transport->peerAddress();
    if (attempt->existingDevice) {
        const auto current = m_devices->find(paired.id);
        if (attempt->existingDeviceRevoked || !current
            || current->publicKeySpki != attempt->existingDevice->publicKeySpki
            || current->pairedAt != attempt->existingDevice->pairedAt) {
            refuse(QStringLiteral("This device was removed from the Core while pairing. "
                                   "Open pairing again to reconnect it."));
            return;
        }
        // Full code proof succeeded. Keep name, dates, and permissions as saved;
        // ordinary device-key authentication still happens on a new connection.
    } else if (!m_devices->add(paired)) {
        refuse(QStringLiteral("The Core could not save this device. Try again."));
        return;
    }
    attempt->finished = true;
    m_pairingWindow->pairingSucceeded();
    qCInfo(lcStation) << "Paired a device by code from" << it->description;
    const QJsonObject station{
        {QStringLiteral("identity"),
         QJsonObject{
             {QStringLiteral("publicKey"),
              StationIdentity::toBase64Url(m_identity->publicKeySpki())},
             {QStringLiteral("certBinding"), StationIdentity::toBase64Url(m_certBinding)},
         }},
        {QStringLiteral("label"), m_devicesFacade->stationLabel()},
    };
    const QByteArray sealed = attempt->exchange->sealConfirmation(
        QJsonDocument(station).toJson(QJsonDocument::Compact));
    send(transport, SessionMessages::pairConfirm(StationIdentity::toBase64Url(sealed)));
    dropPeer(transport, QStringLiteral("This device is paired with the Core."), false,
             /*retryable=*/false);
}

void StationServer::handlePairFailFromDevice(SessionTransport* transport)
{
    auto it = m_peers.find(transport);
    if (it == m_peers.end()) {
        return;
    }
    const std::shared_ptr<PairingAttempt> attempt = it->pairing;
    if (!attempt || !attempt->codeMode || attempt->finished) {
        dropPeer(transport, QStringLiteral("This app sent a pairing step out of order."), true,
                 /*retryable=*/false, QString::fromLatin1(SessionEndCode::kProtocolError));
        return;
    }
    // The device's step 3 failed: the codes differ. Its reason is not read
    // (it is the device's own text). The code is burned, and the device is
    // told when the next one appears.
    if (attempt->codeTaken) {
        attempt->finished = true;
        m_pairingWindow->pairingFailed(attempt->route);
    }
    sendPairFail(transport,
                 QStringLiteral("The pairing code was not right. A new code will appear on "
                                "the Core."),
                 m_pairingWindow->retryAfterMs(attempt->route));
}

void StationServer::promoteToSession(SessionTransport* transport)
{
    const QString description = peerFor(transport).description;

    // iPhone app Task 71: no other session is ended here, ever. The remote
    // design's section 7.1 preemption is gone (the several-devices design,
    // section 4): admit() has already decided this device holds a place.

    // BEFORE any session is attached for the first time, deliberately.
    // buildMirror() ends in ObjectRegistry::backfillExistingSlices(), which
    // emits objectCreated for every slice the daemon already holds -- and
    // those are wired straight to sendToEveryView(). With no view yet
    // attached they go nowhere, which is exactly right: attachSession()
    // below sends an object.create for every watched object anyway, AFTER
    // the schema messages that a client needs in order to make sense of
    // one.
    buildMirror();

    // iPhone app Task 76: every admitted session has media, with its own
    // epoch. The first session on a Core with none starts the session
    // state afresh, as the one media session did before.
    const bool firstMediaSession = primaryMediaSession() == nullptr;
    if (firstMediaSession) {
        m_ps3SubscriberEpoch = 0;
        m_radioModel->pureSignalFacade()->resetSession();
        m_dispatcher->resetSessionState();
    }
    ++m_mediaSessionEpoch;
    if (m_mediaSessionEpoch == 0) {
        ++m_mediaSessionEpoch;
    }
    m_peers[transport].mediaEpoch = m_mediaSessionEpoch;
    // Its share of the display budget, in its first capabilities; the
    // others' shrink (new generations, published below once it is in).
    const bool sharesChanged = recomputeDisplayBudgetShares();
    // iPhone app Task 72: the session's own id, for the dispatcher's owner
    // string (station:<sessionId>).
    m_peers[transport].sessionId = ++m_nextSessionId;

    if (!sendCapabilitiesAndSettingsSnapshot(transport)) {
        return;
    }

    // State snapshot, model half, ending in the snapshot-complete marker.
    // iPhone app Task 72 (ruling 5.6): this session's own view, with its
    // own coalescer, so the sessions already admitted keep what they have
    // pending; the burst goes to this session alone, and a change the
    // burst itself causes reaches every view.
    const QPointer<MirrorView> view(new MirrorView(
        m_mirror, [this, transport](const SessionMessage& message) { sendToPeer(transport, message); },
        this));
    m_peers[transport].view = view;
    view->attach();
    if (!m_peers.contains(transport)) {
        return;
    }
    m_peers[transport].snapshotComplete = true;
    // iPhone app plan Task 34: txPermitted can be true only from here, so a
    // permitted session is sent its capabilities again now.
    publishTxPermitted();
    if (!m_peers.contains(transport)) {
        return;
    }
    // R-R3-16/17: the connect sequence is finished; the deadline stands down.
    if (QTimer* deadline = m_peers[transport].authDeadline) {
        deadline->stop();
    }

    if (!m_deltaFlushTimer->isActive()) {
        m_deltaFlushTimer->start();
    }

    qCInfo(lcStation) << "Session established with" << description;
    emit clientAuthenticated(description);
    const quint64 epoch = peerFor(transport).mediaEpoch;
    if (sharesChanged) {
        emit displayBudgetChanged();
    }
    publishBudgetToChangedSessions();
    if (!m_peers.contains(transport)) {
        return;
    }
    const QPointer<StationServer> admissionGuard(this);
    if (mediaAvailable(epoch)) {
        emit mediaSessionStarted(epoch);
    }
    if (!admissionGuard) {
        return;
    }
    if (m_peers.contains(transport) && telemetryAvailable(epoch)) {
        emit telemetrySessionStarted(epoch);
    }
    if (!admissionGuard) {
        return;
    }
    // Retirement keeps desired Auto but clears operational arming. Resume
    // only once this first admission survives its snapshot/media callbacks;
    // the coordinator retains its applied parameters and readiness gates.
    if (firstMediaSession && m_peers.contains(transport)
        && peerFor(transport).mediaEpoch == epoch && m_radioModel
        && m_radioModel->role() == RadioModel::Role::Local) {
        if (PureSignal* coordinator = m_radioModel->pureSignal()) {
            coordinator->resumeAutomaticCalibrationPreference();
        }
    }
}

// ── Mirror wiring ────────────────────────────────────────────────────────

void StationServer::buildMirror()
{
    if (m_mirrorBuilt || m_radioModel.isNull()) {
        return;
    }
    m_mirrorBuilt = true;

    m_mirror->watch(QByteArray(kRadioKey), m_radioModel.data());
    m_mirror->watch("pureSignalSettings", m_radioModel->pureSignalSettings());
    m_mirror->watch("dspAssets", m_radioModel->dspAssets());
    // R-R3-21 / R-R3-09 (notchControlVersion 1): the Core's notch list. An
    // older app has no object for this key; it records the schema as skew
    // and drops the object and its deltas, as with any newer object.
    m_mirror->watch("notches", m_radioModel->notchModel());
    // R-R3-46 (radioHardwareVersion 1): the Core's step attenuator and
    // preamp. Sent only to a peer at minor 11 (sendToPeer).
    m_mirror->watch(QByteArray(kStepAttKey), m_radioModel->stepAttFacade());
    // R-R3-46 (radioHardwareVersion 2): the Core's Alex antenna settings.
    // Sent only to a peer at minor 11 (sendToPeer).
    m_mirror->watch(QByteArray(kAlexAntennasKey), m_radioModel->alexAntennaFacade());
    // R-R3-46 (radioHardwareVersion 3): the Core's HL2 I/O board, read-only.
    // Sent only to a peer at minor 11 (sendToPeer).
    m_mirror->watch(QByteArray(kIoBoardKey), m_radioModel->ioBoardFacade());
    m_mirror->watch("pureSignal", m_radioModel->pureSignalFacade());
    m_mirror->watch(QByteArray(kTransmitKey), &m_radioModel->transmitModel());
    if (m_radioModel->tunerModel() != nullptr) {
        m_mirror->watch(QByteArray(kTunerKey), m_radioModel->tunerModel());
    }
    // R-R3-47 / R-R3-22 (remotePgxlControlVersion 1,
    // remoteRfKitControlVersion 1): the Core's amplifier and RF-Kit status.
    // Sent only to a peer at minor 11 (sendToPeer).
    m_mirror->watch(QByteArray(kAmplifierKey), m_radioModel->amplifierModel());
    m_mirror->watch(QByteArray(kRfKitKey), m_radioModel->rfKitModel());
    // R-R3-48 (stationTciVersion 1): the Core's station TCI server.
    // Sent only to a peer at minor 11 on a Core that runs one.
    m_mirror->watch(QByteArray(kStationTciKey), m_radioModel->stationTciModel());
    // R-R3-47 / R-R3-22 (accessoryDataVersion 1): the Core's accessory
    // records and settings. Sent only to a peer at minor 11 on a Core that
    // owns its accessories.
    m_mirror->watch(QByteArray(kAccessoryDataKey), m_radioModel->accessoryDataModel());
    // R-R3-47 / R-R3-22 (remotePgxlControlVersion 3, remoteTgxlControlVersion
    // 1): the amp's and tuner's own settings. Sent only to a peer at minor
    // 11 on a Core that owns its accessories.
    m_mirror->watch(QByteArray(kAccessorySettingsKey), m_radioModel->accessorySettingsModel());
    // iPhone app Task 13 (deviceAdminVersion 1): the Core's paired devices.
    // Sent only to a device at minor 11 that declares deviceAuth.
    m_mirror->watch(QByteArray(kDevicesKey), m_devicesFacade.get());
    // iPhone app Task 71 (sessionHolderVersion 1): who is on the Core. Sent
    // only to a view at minor 11 that declared sessionHolder with
    // deviceAuth.
    m_mirror->watch(QByteArray(kConnectedDevicesKey), m_connectedDevices.get());
    // iPhone app plan Task 39 (txStateVersion 1): the transmitter's state
    // and meters. Sent only to a peer at minor 11 that declared remoteTx.
    m_mirror->watch(QByteArray(kTxStateKey), m_transmitState);
    // iPhone app plan Task 25 (vaxVersion 1): the station computer's VAX.
    // Sent only to a peer at minor 11 that declared vax.
    m_mirror->watch(QByteArray(kVaxKey), m_stationVax);
    // iPhone app Task 19 (stationCatalogVersion 1): the Core's catalogue,
    // read again now so the snapshot carries the radio as it is. Sent only
    // to a peer at minor 11 (sendToPeer).
    m_catalog->refresh();
    m_mirror->watch(QByteArray(kCatalogKey), m_catalog.get());
    // R-R3-49 / R-IOS-18 (paProfileVersion 1): the PA Gain profiles, only
    // to a peer that declared paProfiles (sendToPeer).
    m_paProfiles->refresh();
    m_mirror->watch(QByteArray(kPaProfilesKey), m_paProfiles.get());
    m_setupDescription->setRadioContext(m_radioModel->boardCapabilities(),
                                        m_radioModel->hardwareProfile().model,
                                        m_radioModel->currentRadioInfo());
    m_mirror->watch(QByteArray(kSetupDescriptionKey), m_setupDescription.get());
    // Parity Task 19 (recordStreamVersion 1): the Core's spot sources. Sent
    // only to a peer at minor 11 (sendToPeer).
    if (m_radioModel->spotSourceHost() != nullptr) {
        m_mirror->watch(QByteArray(kSpotSourcesKey), m_radioModel->spotSourceHost());
    }
    const QList<PanadapterModel*> pans = m_radioModel->panadapters();
    for (int i = 0; i < pans.size(); ++i) {
        m_mirror->watch(panKey(i), pans.at(i));
    }

    // The third step of ObjectRegistry's three-step, and the one that is
    // easy to forget: the constructor only wires sliceAdded/sliceRemoved,
    // so every slice DaemonApp::start() already created is invisible until
    // this runs. StateMirror::snapshotAll() is not a substitute -- it walks
    // its own watch list and cannot discover an object nobody watched.
    // Called after the objectCreated/objectDestroyed wiring in the
    // constructor, which is the ordering its own doc comment requires.
    m_registry->backfillExistingSlices();
    // iPhone app Task 73: and a marker for each.
    m_markers->backfill();
    // Slice control plan Task 4: and who controls and listens to each.
    m_sliceAccessSet->backfill();
}

bool StationServer::sendCapabilitiesAndSettingsSnapshot(SessionTransport* transport)
{
    // Do not let a queued radio callback target a disconnected or merely
    // handshaking peer, or one turned away. promoteToSession()
    // intentionally calls this while snapshotComplete is still false, so
    // admission is the boundary here rather than readiness of the full
    // mirror snapshot.
    const auto stillAdmitted = [this, transport]() {
        if (transport == nullptr) {
            return false;
        }
        const auto peer = m_peers.constFind(transport);
        return peer != m_peers.cend() && peer->authenticated && !peer->sessionDeviceId.isEmpty();
    };
    if (!stillAdmitted()) {
        return false;
    }

    // Capability exchange (section 7.0 step 4).  On the initial path this
    // remains before every model message; on the late-radio path it updates
    // only identity, board, effective limits, and connection state.
    const StationCapabilities caps = buildCapabilitiesFor(transport);
    if (auto peer = m_peers.find(transport); peer != m_peers.end()) {
        peer->txPermittedSent = caps.txPermitted;   // Task 34
        peer->txWatchPathVersionSent = caps.txWatchPathVersion;
        peer->txRefusalSent = txRefusalOf(caps);
        // iPhone app Task 76: what these capabilities say of the budget, so
        // a later change is published once.
        peer->publishedBudget = budgetEntriesFor(transport);
    }
    send(transport, SessionMessages::capabilities(caps.toUpdates()));
    if (!stillAdmitted()) {
        return false;
    }

    // Station settings are scoped to the radio that is live *now*.  A late
    // radio therefore needs a fresh snapshot even though the mirror objects
    // already exist on the GUI; SettingsProxy merges this authoritative scope
    // into its in-memory cache without touching local GUI storage.
    const QMap<QString, QString> snapshot = m_settingsServer->buildSnapshot(
        m_radioModel.isNull() ? QString() : m_radioModel->currentRadioMac());
    QList<MirrorUpdate> entries;
    entries.reserve(snapshot.size());
    for (auto it = snapshot.cbegin(); it != snapshot.cend(); ++it) {
        entries.append(
            MirrorUpdate{0, it.key().toUtf8(), MirrorWireKind::Utf8, QVariant(it.value())});
    }
    send(transport, SessionMessages::settingsSnapshot(entries));
    return stillAdmitted();
}

// ── Inbound state and settings ───────────────────────────────────────────

void StationServer::handlePropertyWrite(SessionTransport* transport,
                                        const SessionMessage& message)
{
    if (message.objectKey == kSetupDescriptionKey) {
        // Descriptions are station-authored. A peer that did not declare
        // setupDescription must not learn any value through a write result.
        QList<SessionPropertyResult> results;
        QSet<QByteArray> reported;
        for (const MirrorUpdate& update : message.updates) {
            if (reported.contains(update.name)) {
                continue;
            }
            reported.insert(update.name);
            SessionPropertyResult result;
            result.property = update.name;
            result.accepted = false;
            result.reason = QStringLiteral("The Core owns Setup descriptions.");
            results.append(result);
        }
        if (peerFor(transport).agreedMinor >= kDspControlSessionProtocolMinor
            && message.writeId != 0) {
            send(transport,
                 SessionMessages::propertyResult(message.objectKey, message.writeId, results));
        }
        return;
    }
    // iPhone app Task 73 (ruling 5.9): a device writes only its own slices,
    // and nobody writes a marker. Refused before anything is read or
    // applied, with no value of the other device's slice in the answer.
    {
        QString refusal;
        bool withValues = false;
        const QByteArray requester = peerFor(transport).sessionDeviceId;
        if (message.objectKey.startsWith("slice:")) {
            bool ok = false;
            const int sliceId = message.objectKey.mid(6).toInt(&ok);
            if (ok) {
                refusal = changeRefusal(requester, sliceId);
                // Fix wave I2 (ruling 8.11): the transmit slice is frozen
                // while the station device keys it.
                if (refusal.isEmpty()) {
                    refusal = freezeRefusalFor(message).text;
                    // The requester's own slice: its answer carries the
                    // values kept, so the window shows them again.
                    withValues = !refusal.isEmpty();
                }
            }
        } else if (message.objectKey.startsWith("marker:")) {
            const int sliceId = SliceMarkerSet::sliceIdOf(message.objectKey);
            if (m_radioModel && sliceId >= 0 && m_radioModel->sliceOwnership()->isLive(sliceId)) {
                refusal = ownedElsewhereReason(sliceId);
            }
        }
        if (!refusal.isEmpty()) {
            QHash<QByteArray, MirrorUpdate> kept;
            if (withValues) {
                for (const MirrorUpdate& value : m_mirror->snapshot(message.objectKey)) {
                    kept.insert(value.name, value);
                }
            }
            QList<SessionPropertyResult> results;
            QSet<QByteArray> reported;
            for (const MirrorUpdate& update : message.updates) {
                if (reported.contains(update.name)) {
                    continue;
                }
                reported.insert(update.name);
                SessionPropertyResult result;
                result.property = update.name;
                result.accepted = false;
                result.reason = refusal;
                result.hasValue = kept.contains(update.name);
                if (result.hasValue) {
                    result.value = kept.value(update.name);
                }
                results.append(result);
            }
            if (peerFor(transport).agreedMinor >= kDspControlSessionProtocolMinor
                && message.writeId != 0) {
                send(transport,
                     SessionMessages::propertyResult(message.objectKey, message.writeId, results));
            }
            return;
        }
    }

    // iPhone app Task 75 (the several-devices design, 7.1; ruling 6.1): a
    // change that reaches another device's slices is asked first.
    if (handleSharedSetting(transport, message)) {
        return;
    }
    // iPhone app Task 74 (rulings 6.5 and 6.5a): a retune of a slice that
    // leaves its shared receiver's window may be a pan move, asked first,
    // or a take once refused.
    if (handleSliceRetune(transport, message)) {
        tellAppliedNow(false);
        return;
    }
    const QList<SessionPropertyResult> results = applyPropertyWrite(transport, message, true, {});
    // Ruling 7.1a: a change applied at once tells the devices it disturbed.
    tellAppliedNow(std::any_of(results.cbegin(), results.cend(),
                               [](const SessionPropertyResult& r) { return r.accepted; }));
}

QList<SessionPropertyResult> StationServer::applyPropertyWrite(
    SessionTransport* transport, const SessionMessage& message, bool answer,
    const std::function<void(QList<SessionPropertyResult>&)>& adjust)
{
    // Persist accepted PS preferences without replaying transmit operations.
    // This session advertises txPermitted=false until the R4 transmit path.
    // R-R3-49 (parity Task 7): a peer offered transmitSettingsVersion 7
    // changes them as a local window does, applied to the Core's PureSignal
    // at once (arming keys nothing), and only while the radio is off the
    // air; the on-air check is read once for the batch.
    const bool pureSignalSettingsWrite = message.objectKey == "pureSignalSettings";
    const bool pureSignalSettingsLive = pureSignalSettingsWrite
        && pureSignalArmingOffered(transport);
    // Version 13: taken on the air too, from a peer that may change the
    // transmit settings, as the local PureSignal dialog takes it keyed.
    QString pureSignalOnAir;
    if (pureSignalSettingsLive && !takesTransmitSettingsOnAir(transport)) {
        m_radioModel->stationOnAirRefusal(&pureSignalOnAir);
    }
    const QPointer<PureSignal> hydrating = pureSignalSettingsWrite && !pureSignalSettingsLive
        && m_radioModel ? m_radioModel->pureSignal() : nullptr;
    if (hydrating) {
        hydrating->beginSettingsHydration();
    }
    const auto hydrationGuard = qScopeGuard([hydrating]() {
        if (hydrating) {
            hydrating->endSettingsHydration();
        }
    });
    const QList<MirrorUpdate> before = m_mirror->snapshot(message.objectKey);
    QHash<QByteArray, MirrorUpdate> previous;
    for (const auto& value : before) {
        previous.insert(value.name, value);
    }
    QHash<QByteArray, QString> refusals;
    const bool negotiated = peerFor(transport).agreedMinor >= kDspControlSessionProtocolMinor;
    // R-R3-46: only a peer that was offered the object may change it, and
    // only while the Core's controller is behind it.
    const bool stepAttWrite = message.objectKey == kStepAttKey;
    const bool alexWrite = message.objectKey == kAlexAntennasKey;
    QString stepAttRefusal;
    if (stepAttWrite
        && peerFor(transport).agreedMinor < kRadioIdentitySessionProtocolMinor) {
        stepAttRefusal =
            QStringLiteral("Update this app to change the radio's attenuator on this Core.");
    } else if (stepAttWrite
               && (m_radioModel.isNull() || !m_radioModel->stepAttFacade()->isBound())) {
        stepAttRefusal = QStringLiteral("The Core has no attenuator ready.");
    } else if (alexWrite
               && peerFor(transport).agreedMinor < kRadioIdentitySessionProtocolMinor) {
        stepAttRefusal =
            QStringLiteral("Update this app to change the radio's antennas on this Core.");
    } else if (alexWrite && radioHardwareVersion() < 2) {
        stepAttRefusal = QStringLiteral("The Core has no antenna settings ready.");
    } else if (message.objectKey == kIoBoardKey) {
        // R-R3-46: the I/O board's readings are the Core's to report.
        stepAttRefusal = IoBoardHl2Facade::readOnlyReason();
    } else if (message.objectKey == kAmplifierKey) {
        // R-R3-47: the amp's readings are the Core's to report.
        stepAttRefusal = AmplifierModel::readOnlyReason();
    } else if (message.objectKey == kRfKitKey) {
        stepAttRefusal = RfKitModel::readOnlyReason();
    } else if (message.objectKey == kStationTciKey) {
        // R-R3-48: the switch changes only through setStationTci.
        stepAttRefusal = StationTciModel::readOnlyReason();
    } else if (message.objectKey == kSpotSourcesKey) {
        // Parity Task 19: the spot sources change only through spots.*.
        stepAttRefusal = SpotSourceHost::readOnlyReason();
    } else if (message.objectKey == kAccessoryDataKey) {
        // R-R3-47: changed only through its commands.
        stepAttRefusal = AccessoryDataModel::readOnlyReason();
    } else if (message.objectKey == kAccessorySettingsKey) {
        // R-R3-47 / R-R3-22: the devices' own settings change only through
        // their commands, which the Core sends to the device.
        stepAttRefusal = AccessorySettingsModel::readOnlyReason();
    }
    // TX rulings (JJ, 2026-09-30): the attenuator and preamp act on the
    // slice the writer is shown. On a slice it only listens to, which
    // another device controls, they are that controller's to change, as
    // the RX applet greys them. Read once for the batch.
    QString listenedAttRefusal;
    if (stepAttWrite && !m_radioModel.isNull()) {
        const QByteArray writer = peerFor(transport).sessionDeviceId;
        const SliceOwnership* ownership = m_radioModel->sliceOwnership();
        const int shown = writer.isEmpty() ? -1 : ownership->activeRxFor(writer);
        if (shown >= 0 && ownership->isLive(shown) && ownership->isListening(writer, shown)
            && !SliceAccessPolicy::mayChange(*ownership, writer, shown)) {
            listenedAttRefusal = listenerChangeReason(shown);
        }
    }
    // R-R3-47: the RF-Kit switch is the Core's, changed by setRfKitEnabled.
    const bool radioWrite = message.objectKey == QByteArray(kRadioKey);
    const bool receiveOnlyTransmitWrite = message.objectKey == QByteArray(kTransmitKey)
        && !m_radioModel.isNull() && m_radioModel->receiveOnlyStationPolicy();
    // R-R3-49 (parity Task 1): a peer offered transmitSettingsVersion 1 may
    // change a transmit setting on a receive-only Core. Any other peer is
    // refused every `transmit` write, as before. Since version 13 it is
    // taken on the air too, as a local window takes it while transmitting.
    const bool transmitSettingsWrite = receiveOnlyTransmitWrite
        && transmitSettingsOffered(transport);
    // R-R3-25: the tuner's operate, bypass and antenna, and the amplifier's
    // operate, on a receive-only Core.
    const bool receiveOnlyStation = !m_radioModel.isNull()
        && m_radioModel->receiveOnlyStationPolicy();
    // iPhone app plan Task 34: with remote_transmit allow, the same writes
    // are the permitted sessions' (the station transmit gate), and the
    // transmit path waits while another device's holder is on the air
    // (ruling 7.4). A property write never keys: MOX and TUNE are the
    // transmit verbs' (the transmit safety boundary).
    const bool transmitObjectWrite = message.objectKey == QByteArray(kTransmitKey);
    const TxDecision txDecision = txDecisionFor(transport);
    const QByteArray writer = peerFor(transport).sessionDeviceId;
    const bool tunerWrite = message.objectKey == QByteArray(kTunerKey);
    const bool amplifierWrite = message.objectKey == QByteArray(kAmplifierKey);
    // R-R3-49 (parity Task 5): `stepAtt`'s ATT on TX, its value and Force
    // ATT are transmit settings (transmitSettingsVersion 5). A receive-only
    // Core takes them from a peer offered the transmit settings, on the
    // air too since version 13 (the local Setup page takes them keyed).
    // R-IOS-01: the class MirrorPolicy's direction table is keyed by.
    QByteArray outboundClass;
    if (const QObject* target = m_mirror->watchedObject(message.objectKey)) {
        outboundClass = MirrorSchema::shortClassName(target->metaObject()->className());
    }
    QSet<QByteArray> requested;
    for (const MirrorUpdate& update : message.updates) {
        if (requested.contains(update.name)) {
            refusals.insert(update.name, QStringLiteral("This change named the same setting twice."));
            continue;
        }
        requested.insert(update.name);
        // R-R3-49 (parity Task 1): the keying set is refused first, whether
        // or not this Core mirrors the property, on and off the air.
        if (receiveOnlyTransmitWrite
            && (!transmitSettingsWrite || isTransmitKeyingProperty(update.name))) {
            refusals.insert(update.name, QString::fromLatin1(kReceiveOnlyTransmitReason));
            continue;
        }
        const auto known = previous.constFind(update.name);
        if (known == previous.cend() || known->kind != update.kind) {
            refusals.insert(update.name, QStringLiteral("The Core does not have this setting, or not in this form."));
            continue;
        }
        if (message.objectKey.startsWith("slice:") && update.name == "diversityEnabled") {
            const int id = message.objectKey.mid(6).toInt();
            QString refusal = m_radioModel->legacyDiversityRefusal(id, update.value.toBool());
            if (refusal.isEmpty() && update.value.toBool()) {
                refusal = m_radioModel->diversityEligibility(id);
            }
            if (refusal.isEmpty()) { refusal = stationFreezeRefusal(id).text; }
            if (!refusal.isEmpty()) {
                refusals.insert(update.name, refusal);
                continue;
            }
        }
        if (transmitObjectWrite && (update.name == "mox" || update.name == "tune")) {
            refusals.insert(update.name, QString::fromLatin1(kPropertyNeverKeysReason));
            continue;
        }
        // Ruling 7.4 (D60): the transmit path waits while the holder is on
        // the air (the tuner and the amplifier count too).
        {
            TxRefusal onAir = onAirPropertyRefusal(writer, message.objectKey, update.name);
            if (onAir.isEmpty() && ((tunerWrite && isTunerTransmitPathProperty(update.name))
                                    || (amplifierWrite && update.name == "operate"))) {
                onAir = onAirRefusal(writer);
            }
            if (!onAir.isEmpty()) {
                refusals.insert(update.name, onAir.text);
                continue;
            }
        }
        // iPhone app plan Task 77 (ruling 8.4): VOX keys on audio, not on a
        // person, so it follows the holder strictly: arming it from a
        // device that does not hold transmit is refused (another holder is
        // named by the gate below); its client takes transmit first.
        if (transmitObjectWrite && update.name == "voxEnabled" && update.value.toBool()
            && !writer.isEmpty() && txDecision.permitted && m_transmitHolder
            && !m_transmitHolder->isHeldBy(writer)) {
            refusals.insert(update.name, TxRefusals::notHolder().text);
            continue;
        }
        // R-R3-49 (parity Task 1), merged: a receive-only Core's transmit
        // settings from a peer offered them are taken off the air above.
        if (transmitObjectWrite && update.name == "voxEnabled" && update.value.toBool()
            && !writer.isEmpty() && txDecision.permitted && m_radioModel
            && m_radioModel->remoteMicSelection(sessionOwner(peerFor(transport).sessionId), writer)
                == RemoteMicSource::RadioMic) {
            refusals.insert(update.name, remoteRadioVoxReason());
            continue;
        }
        // Fix wave M8: VOX a device arms listens to its microphone line;
        // with no line it could never key, so arming it is refused with the
        // reason (never armed and silent).
        if (transmitObjectWrite && update.name == "voxEnabled" && update.value.toBool()
            && !writer.isEmpty() && txDecision.permitted && !m_radioModel.isNull()
            && m_radioModel->role() == RadioModel::Role::Local
            && !m_radioModel->remoteMicLineOpen(writer)) {
            refusals.insert(update.name, TxRefusals::micNotConnected().text);
            continue;
        }
        if (transmitObjectWrite && !txDecision.permitted && !transmitSettingsWrite) {
            refusals.insert(update.name, txDecision.refusal.text);
            continue;
        }
        if (receiveOnlyStation
            && ((tunerWrite && isTunerTransmitPathProperty(update.name))
                || (amplifierWrite && update.name == "operate"))) {
            // R-R3-25 / R-R3-47: these wait for remote transmit.
            refusals.insert(update.name, AmplifierModel::receiveOnlyOperateReason());
            continue;
        }
        if (!txDecision.permitted
            && ((tunerWrite && isTunerTransmitPathProperty(update.name))
                || (amplifierWrite && update.name == "operate"))) {
            refusals.insert(update.name, txDecision.refusal.text);
            continue;
        }

        if (!stepAttRefusal.isEmpty()) {
            refusals.insert(update.name, stepAttRefusal);
            continue;
        }
        if (!listenedAttRefusal.isEmpty() && isListenedAttProperty(update.name)) {
            refusals.insert(update.name, listenedAttRefusal);
            continue;
        }
        if (stepAttWrite && StepAttenuatorFacade::isTransmitSetting(update.name)) {
            if (receiveOnlyStation && !transmitSettingsOffered(transport)) {
                refusals.insert(update.name, QString::fromLatin1(kReceiveOnlyTransmitReason));
                continue;
            }
            const QString range = m_radioModel->stepAttFacade()
                ->transmitSettingRefusal(update.name, update.value);
            if (!range.isEmpty()) {
                refusals.insert(update.name, range);
                continue;
            }
        }
        if (radioWrite && update.name == "rfKitEnabled") {
            refusals.insert(update.name, QString::fromLatin1(kRfKitSwitchWriteReason));
            continue;
        }
        if (!negotiated && (message.objectKey == "pureSignalSettings"
            || update.name.startsWith("nnr")
            || (update.name == "activeNr" && update.value.toInt() == static_cast<int>(NrSlot::NNR)))) {
            refusals.insert(update.name, QStringLiteral("Update this app to change these settings on this Core."));
            continue;
        }
        if (pureSignalSettingsLive && !pureSignalOnAir.isEmpty()) {
            refusals.insert(update.name, pureSignalOnAir);
            continue;
        }
        if (!outboundClass.isEmpty()
            && !MirrorPolicy::inboundAllowed(outboundClass, update.name)) {
            // `active` has a way in of its own: selecting the slice.
            refusals.insert(update.name,
                            outboundClass == "SliceModel" && update.name == "active"
                                ? SliceModel::activeWriteReason()
                                : QString::fromLatin1(kOutboundWriteReason));
            continue;
        }
        // R-R3-49 (parity Task 2): a transmit setting outside its setter's
        // range is refused with the range, rather than clamped silently.
        if (message.objectKey == QByteArray(kTransmitKey) && !m_radioModel.isNull()) {
            const QString range = m_radioModel->transmitModel()
                .settingRangeRefusal(update.name, update.value);
            if (!range.isEmpty()) {
                refusals.insert(update.name, range);
                continue;
            }
        }
        // iPhone app plan Task 25 (vaxVersion 1): the station computer's
        // VAX, from a peer that declared vax; a level from 0 to 1, and the
        // transmit level only from a device that may transmit (a mute and a
        // receive level apply at once, as the applet's do).
        if (message.objectKey == QByteArray(kVaxKey)) {
            if (vaxVersion() < 1 || !peerDeclares(transport, QByteArrayLiteral("vax"), 1)) {
                refusals.insert(update.name,
                                QStringLiteral("Update this app to change VAX on the "
                                               "Core's computer."));
                continue;
            }
            const QString range = StationVax::levelRefusal(update.name, update.value);
            if (!range.isEmpty()) {
                refusals.insert(update.name, range);
                continue;
            }
            if (StationVax::isTransmitProperty(update.name) && !txDecision.permitted) {
                refusals.insert(update.name, txDecision.refusal.text);
                continue;
            }
        }
        // Ruling 5.7: this session's write. Its changes, here and on other
        // objects, are withheld from its own view and reach every other.
        const MirrorApplyResult result = m_mirror->applyInbound(
            message.objectKey, update.name, update.value, peerFor(transport).view.data());
        if (!result.accepted) {
            refusals.insert(update.name, result.reason);
        } else if (transmitObjectWrite && update.name == "voxEnabled" && !writer.isEmpty()
                   && m_radioModel && m_radioModel->transmitModel().voxEnabled()) {
            // iPhone app plan Task 37: this device armed VOX. It is watched
            // while VOX stays on (it sends keepalives), and VOX goes off
            // with its session, its link or its microphone line.
            if (!m_voxArmedBy.isEmpty() && m_voxArmedBy != writer && m_txWatchdog) {
                m_txWatchdog->setVoxArmed(m_voxArmedBy, false);
            }
            m_voxArmedBy = writer;
            if (m_txWatchdog) {
                // Control logging lane: a new watch measures keepalive
                // gaps afresh.
                if (!m_txWatchdog->isWatching(writer)) {
                    m_controlLog.keepaliveWatchStarted(writer);
                }
                m_txWatchdog->setVoxArmed(writer, true);
            }
            emit voxArmedByChanged(writer);
        }
    }

    // Read once after the WHOLE batch: a later setter may change an earlier
    // property's final value. Qt's successful WRITE invocation alone cannot
    // prove that a validating model accepted the requested configuration.
    const QList<MirrorUpdate> settled = m_mirror->snapshot(message.objectKey);
    QHash<QByteArray, MirrorUpdate> actual;
    for (const auto& value : settled) {
        actual.insert(value.name, value);
    }
    QList<SessionPropertyResult> results;
    QSet<QByteArray> reported;
    for (const auto& update : message.updates) {
        if (reported.contains(update.name)) {
            continue;
        }
        reported.insert(update.name);
        SessionPropertyResult result;
        result.property = update.name;
        result.hasValue = actual.contains(update.name);
        if (result.hasValue) {
            result.value = actual.value(update.name);
        }
        result.reason = refusals.value(update.name);
        const bool sameMap = message.objectKey == QByteArray(kTransmitKey) && result.hasValue
            && sameJsonObject(result.value.value, update.value);
        const bool sameList = alexWrite && result.hasValue
            && sameAntennaListWithout2m(result.value.value, update.value);
        if (result.reason.isEmpty()
            && (!result.hasValue
                || (result.value.value != update.value && !sameMap && !sameList))) {
            // R-R3-46: the attenuator says in plain words why it kept
            // another value (its range, what this radio offers).
            if (stepAttWrite && !m_radioModel.isNull()) {
                result.reason = m_radioModel->stepAttFacade()->settleReason(update.name);
            } else if (alexWrite && !m_radioModel.isNull()) {
                result.reason = m_radioModel->alexAntennaFacade()->settleReason(update.name);
            }
            if (result.reason.isEmpty()) {
                result.reason = QStringLiteral("The Core checked this change and kept the value shown.");
            }
        }
        result.accepted = result.reason.isEmpty();
        results.append(result);
    }
    if (adjust) {
        adjust(results);
    }
    if (answer && negotiated && message.writeId != 0) {
        send(transport, SessionMessages::propertyResult(message.objectKey, message.writeId, results));
    }

    // Inbound setters suppress their notifications to avoid echo loops.
    // Publish their settled side effects, and give legacy peers accepted or
    // clamped readback too. New peers get requested fields in the sequenced
    // result above so an older answer cannot overwrite a newer local edit.
    QList<MirrorUpdate> corrections;
    for (const auto& value : settled) {
        const bool changed = !previous.contains(value.name)
            || !sameSettledValue(previous.value(value.name).value, value.value);
        if ((requested.contains(value.name) && (!negotiated || message.writeId == 0))
            || (changed && !requested.contains(value.name))) {
            corrections.append(value);
        }
    }
    // An older peer was never offered `stepAtt`; it gets nothing back for it.
    const bool olderStepAttPeer = (stepAttWrite || alexWrite)
        && peerFor(transport).agreedMinor < kRadioIdentitySessionProtocolMinor;
    if (!corrections.isEmpty() && !olderStepAttPeer) {
        // A write's side effects can change nnrLimit (turning NNR off or
        // choosing a model clears it), so they are fitted to this peer too.
        SessionMessage delta = SessionMessages::delta(message.objectKey, corrections);
        // R-IOS-13 / R-R3-49: a txEqParaEqData write moves txEqCurve too,
        // which only a peer that declared it is sent. Phone wire batch: a
        // write's side effects can move a declared feature's property (a
        // slice's frequency moves its diversityPattern), which only a peer
        // that declared it is sent.
        if (fitNnrLimitToPeer(delta, peerFor(transport).agreedMinor)
            && fitTxEqCurveToPeer(transport, delta)
            && fitPeerOnlyProperties(transport, delta)
            && fitAdcAttenuatorsToPeer(transport, delta)) {
            send(transport, delta);
        }
    }
    return results;
}

void StationServer::handleSettingsWrite(SessionTransport* transport,
                                        const SessionMessage& message)
{
    if (message.updates.isEmpty()) {
        return;
    }
    const QString key = QString::fromUtf8(message.objectKey);
    // A receive-only Core refuses DSP > Options TX writes exactly as it
    // refuses direct TransmitModel writes (handlePropertyWrite above), and
    // hands back its own value so the remote combo settles on it. R-R3-46:
    // so it does for the transmit side of Hardware Config and the PA pages.
    if (receiveOnlyRefusesKey(transport, key)) {
        const QString reason = QString::fromLatin1(kReceiveOnlyTransmitReason);
        const QVariant restored = m_settings.value(key);
        qCWarning(lcStation) << "Refused remote settings write" << key << ":" << reason;
        send(transport, SessionMessages::settingsReject(key, restored.isValid(),
                                                        restored.toString(), reason));
        return;
    }
    // iPhone app plan Task 34: with remote_transmit allow, a transmit
    // setting is a permitted session's (the station transmit gate).
    if (isReceiveOnlyRefusedKey(key) && !m_radioModel.isNull()
        && !m_radioModel->receiveOnlyStationPolicy()) {
        const TxDecision decision = txDecisionFor(transport);
        if (!decision.permitted) {
            const QVariant restored = m_settings.value(key);
            qCInfo(lcStation) << "Refused remote settings write" << key << ":"
                              << decision.refusal.code;
            send(transport, SessionMessages::settingsReject(key, restored.isValid(),
                                                            restored.toString(),
                                                            decision.refusal.text));
            return;
        }
    }
    // Addendum G-42: a setting the transmit gate reads is changed only by a
    // session with transmit permission, off the air, and only to on or off.
    if (isTransmitGateSettingKey(key)) {
        const QVariant value = message.updates.first().value;
        if (const QString refusal = transmitGateSettingRefusal(transport, key, &value);
            !refusal.isEmpty()) {
            const QVariant restored = m_settings.value(key);
            qCInfo(lcStation) << "Refused remote settings write" << key << ":" << refusal;
            send(transport, SessionMessages::settingsReject(key, restored.isValid(),
                                                            restored.toString(), refusal));
            return;
        }
    }
    // R-R3-49 (parity Task 1): a transmit setting a receive-only Core takes
    // off the air (DSP > Options TX) waits while the radio is on the air,
    // and the Core hands back its own value so the combo settles on it.
    if (const QString onAir = transmitSettingOnAirRefusal(key); !onAir.isEmpty()) {
        const QVariant restored = m_settings.value(key);
        qCWarning(lcStation) << "Refused remote settings write" << key << ":" << onAir;
        send(transport, SessionMessages::settingsReject(key, restored.isValid(),
                                                        restored.toString(), onAir));
        return;
    }
    // R-R3-49 / R-IOS-27 (JJ's ruling): a PA profile key on the air is
    // taken only for the active profile's transmitting band, from the
    // device that holds transmit; the rest is refused, not held.
    {
        const QString value = message.updates.first().value.toString();
        if (const QString pa = paSettingOnAirRefusalFor(transport, key, &value);
            !pa.isEmpty()) {
            const QVariant restored = m_settings.value(key);
            qCWarning(lcStation) << "Refused remote settings write" << key << ":" << pa;
            send(transport, SessionMessages::settingsReject(key, restored.isValid(),
                                                            restored.toString(), pa));
            return;
        }
    }
    // R-R3-49 (parity Task 5): a Power page key the page's own control
    // could not have written is refused, and the Core's value handed back.
    if (const QString range = powerPageKeyValueRefusal(key, message.updates.first().value);
        !range.isEmpty()) {
        const QVariant restored = m_settings.value(key);
        qCWarning(lcStation) << "Refused remote settings write" << key << ":" << range;
        send(transport, SessionMessages::settingsReject(key, restored.isValid(),
                                                        restored.toString(), range));
        return;
    }
    // R-R3-49 (lead's ruling): a calibration value outside its control's
    // range is refused whole, and the Core's value handed back.
    if (const QString range = calibrationKeyValueRefusal(
            key, message.updates.first().value,
            m_radioModel ? m_radioModel->hardwareProfile().model : HPSDRModel::FIRST);
        !range.isEmpty()) {
        const QVariant restored = m_settings.value(key);
        qCWarning(lcStation) << "Refused remote settings write" << key << ":" << range;
        send(transport, SessionMessages::settingsReject(key, restored.isValid(),
                                                        restored.toString(), range));
        return;
    }
    // R-R3-46 / R-R3-49 (review C1): a low-pass edge outside its spinner's
    // range is refused whole, and the Core's value handed back.
    if (const QString range = alexLpfKeyValueRefusal(key, message.updates.first().value);
        !range.isEmpty()) {
        const QVariant restored = m_settings.value(key);
        qCWarning(lcStation) << "Refused remote settings write" << key << ":" << range;
        send(transport, SessionMessages::settingsReject(key, restored.isValid(),
                                                        restored.toString(), range));
        return;
    }
    // An HL2 CL2 frequency outside its box's range, or not a number, is
    // refused whole, and the Core's value handed back.
    if (const QString range = hl2ClockKeyValueRefusal(key, message.updates.first().value);
        !range.isEmpty()) {
        const QVariant restored = m_settings.value(key);
        qCWarning(lcStation) << "Refused remote settings write" << key << ":" << range;
        send(transport, SessionMessages::settingsReject(key, restored.isValid(),
                                                        restored.toString(), range));
        return;
    }
    // D79 (R-IOS-11, R-R3-49): a band plan this Core does not have is
    // refused, and the Core's value handed back.
    if (const QString plan = bandPlanRefusal(key, message.updates.first().value);
        !plan.isEmpty()) {
        const QVariant restored = m_settings.value(key);
        qCWarning(lcStation) << "Refused remote settings write" << key << ":" << plan;
        send(transport, SessionMessages::settingsReject(key, restored.isValid(),
                                                        restored.toString(), plan));
        return;
    }
    // Fix wave I3: a slice's own settings keys are its owner's alone.
    if (const QString refusal = sliceSettingsRefusal(transport, key); !refusal.isEmpty()) {
        const QVariant kept = m_settings.value(key);
        send(transport, SessionMessages::settingsReject(key, kept.isValid(), kept.toString(),
                                                        refusal));
        return;
    }
    // iPhone app Task 75 (link section 8.1): a write that reaches another
    // device is held and asked first.
    if (handleSharedSetting(transport, message)) {
        return;
    }
    // Ruling 7.1a: a change applied at once tells the devices it disturbed.
    tellAppliedNow(applySettingsWrite(transport, message, nullptr));
}

// R-R3-46 / R-R3-49 (review I1): the Filters tab's neighbour rule, run on
// the Core for a low-pass edge any window or the phone wrote, with the same
// code the desktop tab runs (codec::alex::applyAlexLpfEdgeEdit, from Thetis
// setup.cs:15888-15994 [v2.10.3.15]). The rows are read held to their
// ranges, as RadioModel::savedAlexLpfEdges reads them; each neighbour moved
// is stored, which the settings proxy sends to every peer.
void StationServer::applyAlexLpfNeighbourRule(const QString& key)
{
    const std::optional<AlexLpfEdgeKey> edge = parseAlexLpfEdgeKey(key);
    if (!edge || !edge->canonical) {
        return;
    }
    codec::alex::AlexLpfRows rows = codec::alex::AlexLpfEdges::thetisDefaults().rows;
    for (int i = 0; i < codec::alex::kAlexLpfRowCount; ++i) {
        for (const bool isEnd : {false, true}) {
            double& slot = isEnd ? rows[static_cast<size_t>(i)].endMhz
                                 : rows[static_cast<size_t>(i)].startMhz;
            bool ok = false;
            const double saved =
                m_settings.value(alexLpfEdgeKeyFor(*edge, i, isEnd)).toString().toDouble(&ok);
            if (ok) {
                slot = codec::alex::clampAlexLpfEdge(i, isEnd, saved, slot);
            }
        }
    }
    const codec::alex::AlexLpfRow& edited = rows[static_cast<size_t>(edge->row)];
    const double value = edge->isEnd ? edited.endMhz : edited.startMhz;
    const std::vector<codec::alex::AlexLpfEdgeMove> moved =
        codec::alex::applyAlexLpfEdgeEdit(rows, edge->row, edge->isEnd, value);
    for (const codec::alex::AlexLpfEdgeMove& m : moved) {
        m_settings.setValue(alexLpfEdgeKeyFor(*edge, m.row, m.isEnd),
                            QString::number(m.mhz, 'f', 6));
    }
}

QString StationServer::bandPlanRefusal(const QString& key, const QVariant& value) const
{
    // G42: validate the effective TX region before it reaches persistent
    // settings. RadioModel also rejects invalid values already on disk.
    if (key == QLatin1String("BandPlanRegion")) {
        bool ok = false;
        const int region = value.toString().toInt(&ok);
        if (!ok || region < static_cast<int>(safety::Region::Australia)
            || region > static_cast<int>(safety::Region::Germany)) {
            return QStringLiteral("Choose one of this Core's transmit regions.");
        }
        return {};
    }
    if (key != QLatin1String(kBandPlanNameKey) || m_radioModel.isNull()) {
        return {};
    }
    return m_radioModel->bandPlanManager().availablePlans().contains(value.toString())
        ? QString()
        : QStringLiteral("This Core does not have that band plan.");
}

void StationServer::applyBandPlanSetting(const QString& key)
{
    // D79 (R-IOS-11, R-R3-49): the plan a device picks is the station's,
    // as a remote window's View > Band Plan is. The Core's own strip
    // redraws and the catalogue refreshes through planChanged. A removal
    // reads the default, ARRL (US). The key already holds the value, so
    // setActivePlan() writes nothing back.
    if (key != QLatin1String(kBandPlanNameKey) || m_radioModel.isNull()) {
        return;
    }
    m_radioModel->bandPlanManagerMutable().setActivePlan(
        m_settings.value(key, QString::fromLatin1(BandPlanManager::kDefaultPlanName)).toString());
}

QString StationServer::sliceSettingsRefusal(SessionTransport* transport, const QString& key) const
{
    // Slice<N>/... is SliceModel's per-slice, per-band state (Station
    // scope). A slice's NNR keys, hardware/<mac>/slices/<N>/nnr/..., are
    // model-owned and refused to every writer already.
    static const QRegularExpression kSliceKey(QStringLiteral("^Slice(\\d+)/"));
    const QRegularExpressionMatch match = kSliceKey.match(key);
    if (!match.hasMatch() || m_radioModel.isNull()) {
        return {};
    }
    const QByteArray requester = peerFor(transport).sessionDeviceId;
    if (requester.isEmpty()) {
        return {};
    }
    bool ok = false;
    const int sliceId = match.captured(1).toInt(&ok);
    if (!ok) {
        return {};
    }
    // Only the slice's controller (slice control plan Task 2: a listener
    // changes nothing); a slice that is not live has none, so its keys
    // (which would seed the next slice under that id) are nobody's.
    const SliceOwnership* ownership = m_radioModel->sliceOwnership();
    if (ownership->isLive(sliceId)
        && SliceAccessPolicy::mayChange(*ownership, requester, sliceId)) {
        return {};
    }
    // Slice control plan Task 4: a listener's words to a listener that
    // shares slices.
    if (ownership->isLive(sliceId) && ownership->isListening(requester, sliceId)
        && deviceSharesSlices(requester)) {
        return listenerChangeReason(sliceId);
    }
    return ownedElsewhereReason(sliceId);
}

bool StationServer::applySettingsWrite(SessionTransport* transport, const SessionMessage& message,
                                       QString* refusal)
{
    const QString key = QString::fromUtf8(message.objectKey);
    if (key == QLatin1String("BandPlanRegion") || isTransmitGateSettingKey(key)) {
        QString reason = transmitSettingOnAirRefusal(key);
        if (reason.isEmpty()) {
            reason = bandPlanRefusal(key, message.updates.first().value);
        }
        if (!reason.isEmpty()) {
            if (refusal) {
                *refusal = reason;
            } else {
                const QVariant restored = m_settings.value(key);
                send(transport, SessionMessages::settingsReject(
                    key, restored.isValid(), restored.toString(), reason));
            }
            return false;
        }
    }
    const SettingsApplyResult result =
        m_settingsServer->applyInboundWrite(key, message.updates.first().value,
                                            message.originTag);
    if (!result.accepted) {
        qCWarning(lcStation) << "Refused remote settings write" << key << ":"
                             << result.reason;
        if (refusal != nullptr) {
            *refusal = result.reason;
        } else {
            send(transport,
                 SessionMessages::settingsReject(key, result.restoredValue.isValid(),
                                                 result.restoredValue.toString(), result.reason));
        }
        return false;
    }
    // R-R3-46 / R-R3-49 (review I1): a low-pass edge moves its neighbours as
    // the desktop tab's spinners do, stored here so every window and the
    // phone see them, before the radio reads the rows.
    applyAlexLpfNeighbourRule(key);
    // R-R3-21: a DSP > Options RX setting takes effect now, not at the next
    // mode change. R-R3-46: a Hardware Config setting reaches the Core's
    // own controllers (the radio, and their later saves) now too. RadioModel
    // ignores every other key.
    if (!m_radioModel.isNull()) {
        m_radioModel->scheduleRemoteDspOptionsApply(key);
        m_radioModel->scheduleRemoteHardwareApply(key);
        // R-R3-49 / R-IOS-27: a PA change taken on the air reaches the
        // Core's bank and drive now (the PA reload waits for receive).
        m_radioModel->applyPaSettingOnAir(key, m_settings.value(key).toString());
        // R-R3-47 / R-R3-22: an accessory setting (interlock, output limit,
        // tune memory, antenna names, a fault history) reaches the Core's
        // live objects now, not at the next restart.
        m_radioModel->applyRemoteAccessorySetting(key);
        // iPhone plan Task 22 / parity Task 20 (B7.3): a grid square change
        // reaches the Core's FreeDV Reporter list at once.
        m_radioModel->applyRemoteFreedvSetting(key, m_settings);
        // R-R3-49 (parity Task 5): so does an SWR protection setting, to the
        // Core's SwrProtectionController, as the local page's change does.
        m_radioModel->applySwrProtectionSetting(key, m_settings.value(key));
        // R-R3-13 / R-R3-49 (parity Task 15): the Multimeter polling delay
        // sets the Core's meter pump rate at once, as the local page does.
        m_radioModel->applyMeterSetting(key, m_settings.value(key));
        // R-R3-49 / R-R3-21 (parity Task 30, txDisplayVersion 2): a TX
        // Display analyzer setting reaches the Core's TX analyzer at once,
        // on and off the air, as the local page's change does. It changes
        // only the display, never the radio.
        m_radioModel->applyRemoteTxDisplaySetting(key);
        // R-IOS-13 / R-R3-49 (txModMonitorVersion 1): the Mod Monitor's
        // feedback receiver reaches the Core's feedback analyzer at once.
        m_radioModel->applyModMonitorSetting(key, m_settings.value(key));
        // Parity ruling C12: a band's grid dB max or min reaches the Core's
        // pans at once; on that band the new range goes to every window.
        m_radioModel->applyPanGridSetting(key);
        // Level Cal (radioHardwareVersion 12): the meter or display
        // calibration reaches the Core's meter and TCI at once.
        m_radioModel->applyLevelCalibrationSetting(key);
    }
    // D79: the Core's own band plan follows BandPlanName.
    applyBandPlanSetting(key);
    // Addendum G-42: the Core's own Setup page shows a device's change.
    if (!m_radioModel.isNull()) {
        m_radioModel->reportTransmitGateSettingChanged(key);
    }
    return true;
}

void StationServer::handleSettingsRemove(SessionTransport* transport, const SessionMessage& message)
{
    const QString key = QString::fromUtf8(message.objectKey);
    if (isModelOwnedDspSettingsKey(key)) {
        const QVariant value = m_settings.value(key);
        const bool nr3Path =
            key.compare(QLatin1String("Nr3ModelPath"), Qt::CaseInsensitive) == 0;
        // R-R3-21: the notch keys, like Nr3ModelPath, carry their own plain
        // reason; every other model-owned key keeps its existing wire string.
        // R-R3-46: so do the step attenuator and preamp keys.
        const bool plainReason = nr3Path || isModelOwnedNotchSettingsKey(key)
            || isModelOwnedStepAttenuatorSettingsKey(key)
            || isModelOwnedAlexAntennaSettingsKey(key)
            || isCoreOwnedIdentitySettingsKey(key);
        // iPhone app Task 71: to the session that asked (the only one
        // before).
        send(transport, SessionMessages::settingsReject(key, value.isValid(), value.toString(),
            plainReason ? modelOwnedSettingsRefusal(key)
                    : QStringLiteral("Change these settings with their own controls on this Core.")));
        return;
    }
    // A remove would reset a DSP > Options TX setting to its default, so a
    // receive-only Core refuses it exactly as it refuses a write to the same
    // key (handleSettingsWrite above) and hands back its own value (R-R3-21).
    // R-R3-46: the same for a transmit-side hardware key.
    if (receiveOnlyRefusesKey(transport, key)) {
        const QString reason = QString::fromLatin1(kReceiveOnlyTransmitReason);
        const QVariant restored = m_settings.value(key);
        qCWarning(lcStation) << "Refused remote settings remove" << key << ":" << reason;
        send(transport, SessionMessages::settingsReject(key, restored.isValid(),
                                                      restored.toString(), reason));
        return;
    }
    // iPhone app plan Task 34: with remote_transmit allow, a transmit
    // setting is a permitted session's (the station transmit gate).
    if (isReceiveOnlyRefusedKey(key) && !m_radioModel.isNull()
        && !m_radioModel->receiveOnlyStationPolicy()) {
        const TxDecision decision = txDecisionFor(transport);
        if (!decision.permitted) {
            const QVariant restored = m_settings.value(key);
            qCInfo(lcStation) << "Refused remote settings remove" << key << ":"
                              << decision.refusal.code;
            send(transport, SessionMessages::settingsReject(key, restored.isValid(),
                                                            restored.toString(),
                                                            decision.refusal.text));
            return;
        }
    }
    // Addendum G-42: removing it restores off, under the same rule.
    if (isTransmitGateSettingKey(key)) {
        if (const QString refusal = transmitGateSettingRefusal(transport, key, nullptr);
            !refusal.isEmpty()) {
            const QVariant restored = m_settings.value(key);
            qCInfo(lcStation) << "Refused remote settings remove" << key << ":" << refusal;
            send(transport, SessionMessages::settingsReject(key, restored.isValid(),
                                                            restored.toString(), refusal));
            return;
        }
    }
    // R-R3-49 (parity Task 1): the same wait for a remove while the radio
    // is on the air.
    if (const QString onAir = transmitSettingOnAirRefusal(key); !onAir.isEmpty()) {
        const QVariant restored = m_settings.value(key);
        qCWarning(lcStation) << "Refused remote settings remove" << key << ":" << onAir;
        send(transport, SessionMessages::settingsReject(key, restored.isValid(),
                                                      restored.toString(), onAir));
        return;
    }
    // R-R3-49 / R-IOS-27: a PA profile key is never removed on the air.
    if (const QString pa = paSettingOnAirRefusalFor(transport, key, nullptr); !pa.isEmpty()) {
        const QVariant restored = m_settings.value(key);
        qCWarning(lcStation) << "Refused remote settings remove" << key << ":" << pa;
        send(transport, SessionMessages::settingsReject(key, restored.isValid(),
                                                        restored.toString(), pa));
        return;
    }
    // SettingsProxyServer has no remove path of its own: AppSettings::
    // remove() fires the same Task 13 change hook a setValue() does, so
    // the broadcast that reaches every client is produced by the same
    // generic path, with an empty origin tag. Routed through the daemon's
    // own store directly, and gated on the same Station classification
    // applyInboundWrite() enforces so a client cannot reach an
    // OperatorLocal key by removing it instead of writing it.
    if (classifySettingsKey(key) != SettingsScope::Station) {
        qCWarning(lcStation) << "Refused remote settings remove of non-station key" << key;
        return;
    }
    // R-R3-46: as for a write, only the connected radio's hardware keys.
    if (const QString refusal = m_settingsServer->otherRadioRefusal(key); !refusal.isEmpty()) {
        const QVariant value = m_settings.value(key);
        qCWarning(lcStation) << "Refused remote settings remove" << key << ":" << refusal;
        send(transport, SessionMessages::settingsReject(key, value.isValid(), value.toString(),
                                                      refusal));
        return;
    }
    // Fix wave I3: a slice's own settings keys are its owner's alone.
    if (const QString refusal = sliceSettingsRefusal(transport, key); !refusal.isEmpty()) {
        const QVariant kept = m_settings.value(key);
        send(transport, SessionMessages::settingsReject(key, kept.isValid(), kept.toString(),
                                                        refusal));
        return;
    }
    // Fix wave I3: a removal is a write of the default, so it goes through
    // the shared-setting check exactly as a write does.
    if (handleSharedSetting(transport, message)) {
        return;
    }
    applySettingsRemove(message);
    // Ruling 7.1a: a change applied at once tells the devices it disturbed.
    tellAppliedNow(true);
}

void StationServer::applySettingsRemove(const SessionMessage& message)
{
    const QString key = QString::fromUtf8(message.objectKey);
    m_settings.remove(key);
    // Removing a low-pass edge returns it to Thetis's default, which moves
    // its neighbours as a write of that value does.
    applyAlexLpfNeighbourRule(key);
    // R-R3-21: removing a DSP > Options RX setting returns it to its
    // default, which takes effect now as a write does. R-R3-46: so does a
    // Hardware Config setting. R-R3-47: and an accessory setting.
    if (!m_radioModel.isNull()) {
        m_radioModel->scheduleRemoteDspOptionsApply(key);
        m_radioModel->scheduleRemoteHardwareApply(key);
        m_radioModel->applyRemoteAccessorySetting(key);
        // iPhone plan Task 22 / parity Task 20 (B7.3): a grid square change
        // reaches the Core's FreeDV Reporter list at once.
        m_radioModel->applyRemoteFreedvSetting(key, m_settings);
        // R-R3-49 (parity Task 5): the SWR protection default, at once.
        m_radioModel->applySwrProtectionSetting(key, QVariant());
        // R-R3-13 / R-R3-49 (parity Task 15): the default meter pump rate.
        m_radioModel->applyMeterSetting(key, QVariant());
        // R-R3-49 (parity Task 30): the TX Display analyzer default.
        m_radioModel->applyRemoteTxDisplaySetting(key);
        // R-IOS-13 / R-R3-49: the Mod Monitor's feedback receiver, rx1.
        m_radioModel->applyModMonitorSetting(key, QVariant());
        // Parity ruling C12: the band's default grid range.
        m_radioModel->applyPanGridSetting(key);
        // Level Cal: the radio's default meter or display calibration.
        m_radioModel->applyLevelCalibrationSetting(key);
    }
    // D79: removing BandPlanName returns the Core to ARRL (US).
    applyBandPlanSetting(key);
    // Addendum G-42: removing ExtendedTransmit turns it off; the Core's
    // own Setup page shows it.
    if (!m_radioModel.isNull()) {
        m_radioModel->reportTransmitGateSettingChanged(key);
    }
}

// ── Send helpers ─────────────────────────────────────────────────────────

namespace {
constexpr qint64 kSettingsExportBacklogLimit = 1024 * 1024;
constexpr qint64 kSettingsExportIdleMs = 30000;
constexpr qint64 kSettingsExportLifetimeMs = 120000;

bool exportString(const MirrorUpdate& field, const QByteArray& name, QString* value,
                  qsizetype maxBytes)
{
    if (field.ordinal != 0 || field.name != name || field.kind != MirrorWireKind::Utf8
        || field.value.typeId() != QMetaType::QString) return false;
    const QString text = field.value.toString();
    if (text.toUtf8().size() > maxBytes) return false;
    if (value) *value = text;
    return true;
}

bool exportInteger(const MirrorUpdate& field, const QByteArray& name, qint64* value)
{
    if (field.ordinal != 0 || field.name != name || field.kind != MirrorWireKind::Int64
        || field.value.typeId() != QMetaType::LongLong) return false;
    if (value) *value = field.value.toLongLong();
    return true;
}

bool canonicalTransferId(const QString& value)
{
    static const QRegularExpression id(QStringLiteral("^[0-9a-f]{32}$"));
    return id.match(value).hasMatch();
}
} // namespace

bool StationServer::settingsExportEligible(SessionTransport* transport) const
{
    const auto peer = m_peers.constFind(transport);
    return !m_closing && peer != m_peers.cend() && !peer->dropping
        && peer->authenticated && peer->snapshotComplete && peer->sessionId != 0
        && peer->mediaEpoch != 0 && peer->agreedMinor >= kRadioIdentitySessionProtocolMinor
        && peerSeesPairingCode(transport)
        && peerDeclares(transport, QByteArrayLiteral("settingsBackup"), 1)
        && m_radioModel && m_radioModel->role() == RadioModel::Role::Local
        && transport && transport->isOpen();
}

void StationServer::expireSettingsExports()
{
    const qint64 now = settingsExportNow();
    for (auto it = m_settingsExports.begin(); it != m_settingsExports.end();) {
        if (now - it->lastReadMs >= kSettingsExportIdleMs
            || now - it->bornMs >= kSettingsExportLifetimeMs) {
            it = m_settingsExports.erase(it);
        } else {
            ++it;
        }
    }
}

qint64 StationServer::settingsExportNow() const
{
    return m_settingsExportNowForTest ? m_settingsExportNowForTest()
                                      : m_settingsExportClock.elapsed();
}

void StationServer::handleSettingsExport(SessionTransport* transport,
                                         const SessionMessage& message, const QByteArray& wire)
{
    const auto reply = [this, transport, &message](bool accepted, const QString& reason,
                                                    const QList<MirrorUpdate>& values = {}) {
        send(transport, SessionMessages::commandResult(message.commandVerb, message.commandId,
                                                        accepted, reason, {}, values));
    };
    if (!SettingsBackupExportWire::strictEnvelope(wire, false)) {
        reply(false, QStringLiteral("Invalid settings export request."));
        return;
    }
    if (!settingsExportEligible(transport)) {
        reply(false, QStringLiteral("Settings export requires a connected paired device."));
        return;
    }
    expireSettingsExports();
    const quint64 sessionId = peerFor(transport).sessionId;
    const auto stringField = [](const QByteArray& name, const QString& value) {
        return MirrorUpdate{0, name, MirrorWireKind::Utf8, QVariant(value)};
    };
    const auto integerField = [](const QByteArray& name, qint64 value) {
        return MirrorUpdate{0, name, MirrorWireKind::Int64,
                            QVariant(static_cast<qlonglong>(value))};
    };

    if (message.commandVerb == "station.settingsExport.begin") {
        if (!message.arguments.isEmpty()) {
            reply(false, QStringLiteral("Invalid settings export request."));
            return;
        }
        if (m_settingsExports.contains(sessionId)) {
            reply(false, QStringLiteral("A settings export is already in progress."));
            return;
        }
        qint64 heldBytes = 0;
        for (const SettingsExportJob& job : std::as_const(m_settingsExports)) {
            heldBytes += job.source.manifest().byteLength;
        }
        if (m_settingsExports.size() >= 4
            || heldBytes > 64LL * 1024 * 1024 - SettingsBackupTransferSource::kMaxBytes
            || transport->backlogBytes() >= kSettingsExportBacklogLimit) {
            reply(false, QStringLiteral("The Core is busy sending settings. Try again shortly."));
            return;
        }
        QString onAir;
        if (m_radioModel->stationOnAirRefusal(&onAir)) {
            reply(false, onAir);
            return;
        }
        QString error;
        const QByteArray xml = m_settings.exportLocalXml(&error);
        SettingsBackupTransferSource source;
        if (!SettingsBackupTransferSource::create(xml, &source, &error)) {
            reply(false, error);
            return;
        }
        // A caller may reenter during serialization. Never hand its bytes to
        // a replacement or a closing session.
        if (!settingsExportEligible(transport)
            || peerFor(transport).sessionId != sessionId) return;
        if (m_settingsExports.contains(sessionId) || m_settingsExports.size() >= 4) {
            reply(false, QStringLiteral("The Core is busy sending settings. Try again shortly."));
            return;
        }
        const QByteArray id = QUuid::createUuid().toString(QUuid::WithoutBraces)
                                  .remove(QLatin1Char('-')).toLatin1();
        const qint64 now = settingsExportNow();
        const SettingsBackupTransferManifest manifest = source.manifest();
        m_settingsExports.insert(sessionId, SettingsExportJob{std::move(source), id, 0, now, now});
        reply(true, {}, {stringField("transferId", QString::fromLatin1(id)),
                         integerField("byteLength", manifest.byteLength),
                         stringField("sha256", QString::fromLatin1(manifest.sha256.toHex()))});
        return;
    }

    if (message.commandVerb == "station.settingsExport.read") {
        QString id;
        qint64 offset = -1;
        if (message.arguments.size() != 2
            || !exportString(message.arguments.at(0), "transferId", &id, 32)
            || !exportInteger(message.arguments.at(1), "offset", &offset)
            || !canonicalTransferId(id)) {
            reply(false, QStringLiteral("Invalid settings export read request."));
            return;
        }
        auto job = m_settingsExports.find(sessionId);
        if (job == m_settingsExports.end() || job->transferId != id.toLatin1()
            || offset != job->nextOffset || offset < 0) {
            reply(false, QStringLiteral("This settings export is no longer available."));
            return;
        }
        if (transport->backlogBytes() >= kSettingsExportBacklogLimit) {
            m_settingsExports.erase(job);
            reply(false, QStringLiteral("The Core is busy sending settings. Try again shortly."));
            return;
        }
        QByteArray chunk;
        QString error;
        if (!job->source.readChunk(offset, SettingsBackupTransferSource::kMaxChunkBytes,
                                   &chunk, &error)) {
            m_settingsExports.erase(job);
            reply(false, error);
            return;
        }
        const qint64 end = offset + chunk.size();
        const bool final = end == job->source.manifest().byteLength;
        job->nextOffset = end; // advance before a synchronously reentrant send
        job->lastReadMs = settingsExportNow();
        if (final) m_settingsExports.erase(job);
        reply(true, {}, {stringField("transferId", id), integerField("offset", offset),
                         stringField("data", QString::fromLatin1(chunk.toBase64()))});
        return;
    }

    if (message.commandVerb == "station.settingsExport.cancel") {
        QString id;
        if (message.arguments.size() != 1
            || !exportString(message.arguments.at(0), "transferId", &id, 32)
            || !canonicalTransferId(id)) {
            reply(false, QStringLiteral("Invalid settings export cancellation."));
            return;
        }
        const auto job = m_settingsExports.constFind(sessionId);
        if (job != m_settingsExports.cend() && job->transferId == id.toLatin1()) {
            m_settingsExports.remove(sessionId);
        }
        reply(true, {});
        return;
    }
    reply(false, QStringLiteral("The Core does not know this settings export request."));
}

// Coordinated movable Diversity v1. No deferred partial writes: admission,
// both participants and the resource plan are re-read on the model thread.
bool StationServer::handleDiversityControl(SessionTransport* transport, const SessionMessage& message)
{
    if (message.commandVerb != "diversity.setTarget") { return false; }
    if (!m_radioModel) {
        send(transport, SessionMessages::commandResult(message.commandVerb, message.commandId,
            false, QStringLiteral("Update this app and Core to move Diversity between slices."), {}));
        return true;
    }
    const QPointer<StationServer> self(this);
    const QPointer<SessionTransport> recipient(transport);
    const quint64 sessionId = peerFor(transport).sessionId;
    const SessionMessage result =
        m_radioModel->invokeAdmittedDiversityControl(message, this, transport, false);
    // Model publication can retire this server or replace the requesting
    // session synchronously. A result belongs only to that original invoke.
    if (self && recipient && self->hasReplySession(recipient, sessionId)) {
        self->send(recipient, result);
    }
    return true;
}

std::optional<std::pair<QString, QString>>
StationServer::runInvoke(SessionTransport* transport, const SessionMessage& message)
{
    const QPointer<StationServer> self(this);
    const QPointer<SessionCommandDispatcher> dispatcher(m_dispatcher);
    const Peer asker = peerFor(transport);
    if (!hasReplySession(transport, asker.sessionId)) { return std::nullopt; }
    InvokeFrame frame{m_invokeFrame, QPointer<SessionTransport>(transport),
                      {asker.sessionId, message.commandVerb, message.commandId},
                      false, false, std::nullopt};
    struct SavedQuestion {
        QPointer<SessionTransport> transport;
        quint64 sessionId = 0;
        SessionMessage message;
    };
    QList<SavedQuestion> previousQuestions;
    for (const auto& [to, question] : std::as_const(m_heldQuestions)) {
        const quint64 session = peerFor(to).sessionId;
        if (hasReplySession(to, session)) {
            previousQuestions.append({QPointer<SessionTransport>(to), session, question});
        }
    }
    const bool previousHolding = m_holdQuestions;
    m_heldQuestions.clear();
    m_holdQuestions = false;
    // The explicit result-owner override belongs to the signal being
    // delivered, not to a new invoke it happens to receive synchronously.
    auto previousContext = dispatcher->exchangeDispatchContext({});
    m_invokeFrame = &frame;
    m_dispatchingTransport = transport;
    const auto restore = qScopeGuard([this, self, dispatcher, &frame, &previousContext,
                                     previousHolding, &previousQuestions] {
        if (!self) { return; }
        m_invokeFrame = frame.parent;
        if (frame.parent != nullptr && frame.parent->transport
            && hasReplySession(frame.parent->transport, frame.parent->key.sessionId)) {
            m_dispatchingTransport = frame.parent->transport;
            // Re-read negotiated offers; a nested command may change station
            // permission. Restoring identity never rolls that change back.
            previousContext.pureSignalArmingOffered = pureSignalArmingOffered(m_dispatchingTransport);
            previousContext.transmitSettingsOnAir = takesTransmitSettingsOnAir(m_dispatchingTransport);
            previousContext.requesterSharesSlices = peerHasSliceAccess(m_dispatchingTransport);
        } else {
            m_dispatchingTransport = nullptr;
            previousContext.owner.clear();
            previousContext.requester.clear();
            previousContext.requesterSharesSlices = false;
            previousContext.pureSignalArmingOffered = false;
            previousContext.transmitSettingsOnAir = false;
        }
        if (dispatcher) { dispatcher->exchangeDispatchContext(std::move(previousContext)); }
        m_holdQuestions = previousHolding;
        m_heldQuestions.clear();
        for (const SavedQuestion& question : std::as_const(previousQuestions)) {
            if (question.transport && hasReplySession(question.transport, question.sessionId)) {
                m_heldQuestions.append(qMakePair(question.transport.data(), question.message));
            }
        }
    });
    if (answersEveryKeyingCopy(message.commandVerb)) {
        // Register before any admission or model notification can reenter.
        // RemoteKeying replies once per invocation, including waiting copies.
        ++m_keyingReplyCounts[frame.key];
        m_resultRoutes.insert(frame.key, frame.transport);
    }
    // iPhone app Task 72 (ruling 5.8): the command acts for this
    // session, so a DSP-asset job it starts is this device's.
    m_dispatcher->setSessionOwner(sessionOwner(asker.sessionId));
    // R-R3-49 (parity Task 7): whether this peer may arm PureSignal
    // off the air, set per dispatch like the owner (checkpoint join).
    m_dispatcher->setPureSignalArmingOffered(pureSignalArmingOffered(transport));
    // Remote parity on the air (transmitSettingsVersion 13): whether
    // this peer's transmit-setting commands are taken on the air.
    m_dispatcher->setTransmitSettingsOnAir(takesTransmitSettingsOnAir(transport));
    // iPhone app Task 73 (rulings 5.9, 5.10): and for this device,
    // whose slices it may name and whose active slice it sets.
    m_dispatcher->setRequester(asker.sessionDeviceId);
    // Slice control plan Task 4: and whether it shares slices.
    m_dispatcher->setRequesterSharesSlices(peerHasSliceAccess(transport));
    // iPhone app Task 74: a receiver change may be the anchor's
    // (rulings 6.3, 6.4, 6.6) or a take (section 6.4).
    // iPhone app Task 75: a setting that affects every device
    // (the several-devices design, 7.1) is asked first.
    // Fix wave I2 (ruling 8.11): closing the frozen transmit slice,
    // or moving it to another band, waits for the radio's press to
    // end.
    const QString antennaRefusal = radioAntennaRowRefusal(transport, message);
    const TxRefusal frozen = antennaRefusal.isEmpty() ? freezeRefusalFor(message)
                                                         : TxRefusal{};
    if (message.commandVerb == "diversity.setTarget") {
        // Spend this invocation's terminal result before observer-bearing
        // admission/publication; nested invokes keep their own reply frame.
        frame.resultSent = frame.terminalResultSent = true;
        handleDiversityControl(transport, message);
    } else if (!antennaRefusal.isEmpty()) {
        frame.resultSent = frame.terminalResultSent = true;
        consumeKeyingReply(frame.key);
        send(transport, SessionMessages::commandResult(
            message.commandVerb, message.commandId, false, antennaRefusal, {}));
    } else if (!frozen.isEmpty()) {
        frame.resultSent = frame.terminalResultSent = true;
        consumeKeyingReply(frame.key);
        send(transport, SessionMessages::commandResult(
            message.commandVerb, message.commandId, false, frozen.text, {},
            {{0, "refusalCode", MirrorWireKind::Utf8, QString::fromUtf8(frozen.code)},
             {0, "refusalFix", MirrorWireKind::Utf8, QString::fromUtf8(frozen.fix)}}));
    } else if (!handleSharedSetting(transport, message)
        && !handleReceiverCommand(transport, message)) {
        m_dispatcher->dispatch(message);
    }
    if (!self) { return std::nullopt; }
    sendHeldQuestions();
    if (!self) { return std::nullopt; }
    // iPhone app Task 71: a result still owed (it arrives on a later
    // turn), or a PureSignal action's later phases, goes to this
    // session, not to whichever session holds media.
    if (!answersEveryKeyingCopy(message.commandVerb) && frame.transport
        && hasReplySession(frame.transport, frame.key.sessionId)
        && (!frame.resultSent
            || (message.commandVerb.startsWith("ps3.") && !frame.terminalResultSent))) {
        m_resultRoutes.insert(frame.key, frame.transport);
    }
    return frame.pendingEnd;
}

void StationServer::send(SessionTransport* transport, const SessionMessage& message)
{
    if (transport == nullptr) {
        return;
    }
    if (isStationTransport(transport)) {
        deliverToStation(message);
        return;
    }
    logControlResult(transport, message);
    transport->sendText(encodeFor(transport, message));
}

ControlLog::PeerInfo StationServer::controlLogPeer(const Peer& peer)
{
    ControlLog::PeerInfo info;
    info.deviceId = peer.sessionDeviceId.isEmpty() ? peer.deviceId : peer.sessionDeviceId;
    info.signedIn = peer.authenticated;
    info.introduced = peer.introduced;
    info.mediaTunnel = peer.mediaTunnel.get();
    return info;
}

void StationServer::noteControlIn(SessionTransport* transport, const SessionMessage& message)
{
    const auto it = m_peers.constFind(transport);
    if (it != m_peers.cend()) {
        m_controlLog.inbound(transport, controlLogPeer(*it), message);
    }
}

void StationServer::logControlResult(SessionTransport* transport, const SessionMessage& message)
{
    const auto it = m_peers.constFind(transport);
    if (it != m_peers.cend()) {
        m_controlLog.answer(transport, controlLogPeer(*it), message);
    }
}

SessionTransport* StationServer::controlTransportForDevice(const QByteArray& deviceId) const
{
    if (deviceId.isEmpty()) {
        return nullptr;
    }
    for (auto it = m_peers.cbegin(); it != m_peers.cend(); ++it) {
        if (it->sessionDeviceId == deviceId) {
            return it.key();
        }
    }
    return nullptr;
}

void StationServer::controlLogKeepalive(const QByteArray& deviceId,
                                        ControlLog::KeepaliveChannel channel, qint64 ageMs,
                                        bool watched)
{
    SessionTransport* const transport = controlTransportForDevice(deviceId);
    ControlLog::PeerInfo info;
    info.deviceId = deviceId;
    if (transport != nullptr) {
        info = controlLogPeer(*m_peers.constFind(transport));
    }
    m_controlLog.keepalive(deviceId, channel, ageMs, watched, transport, info);
}

bool StationServer::peerSeesPairingCode(SessionTransport* transport) const
{
    const auto peer = m_peers.constFind(transport);
    return peer != m_peers.cend() && peer->authenticated && !peer->signedInWithToken
        && !peer->deviceId.isEmpty();
}

SessionMessage StationServer::withPairingCodeFor(SessionTransport* transport,
                                                 const SessionMessage& message) const
{
    const bool devicesProperties =
        (message.kind == SessionMessageKind::ObjectCreate
         || message.kind == SessionMessageKind::Delta)
        && isDevicesMessage(message);
    const bool pairingOpenResult = message.kind == SessionMessageKind::CommandResult
        && message.commandVerb == "pairing.open";
    if ((!devicesProperties && !pairingOpenResult) || peerSeesPairingCode(transport)) {
        return message;
    }
    // Any other connection (a window signed in with the old pairing token,
    // whatever its hello declares) gets the code blanked.
    SessionMessage blanked = message;
    const QByteArray name = pairingOpenResult ? QByteArrayLiteral("code")
                                              : QByteArray(kPairingCodeProperty);
    for (MirrorUpdate& update : blanked.updates) {
        if (update.name == name) {
            update.value = QVariant(QString());
        }
    }
    return blanked;
}

void StationServer::sendToEveryView(const SessionMessage& message)
{
    // iPhone app Task 72 (ruling 5.8): an object.create, object.destroy or
    // settings.value goes to every view that holds the object or key: each
    // admitted session whose view has attached, fitted to what it
    // negotiated (sendToPeer). A view still in its burst is attached and so
    // receives it too; a connection with no view receives nothing.
    for (const QPointer<MirrorView>& view : attachedViews()) {
        if (!view.isNull()) {
            view->deliver(message);
        }
    }
}

QList<QPointer<MirrorView>> StationServer::attachedViews() const
{
    // Copied: a send can end a session, which erases its peer.
    QList<QPointer<MirrorView>> views;
    for (const Peer& peer : std::as_const(m_peers)) {
        if (!peer.sessionDeviceId.isEmpty() && !peer.view.isNull() && peer.view->isAttached()) {
            views.append(peer.view);
        }
    }
    return views;
}

QString StationServer::sessionOwner(quint64 sessionId)
{
    return sessionId == 0 ? QString() : QStringLiteral("station:%1").arg(sessionId);
}

quint64 StationServer::sessionIdOfOwner(const QString& owner)
{
    static const QString kPrefix = QStringLiteral("station:");
    if (!owner.startsWith(kPrefix)) {
        return 0;
    }
    bool ok = false;
    const quint64 id = QStringView(owner).mid(kPrefix.size()).toULongLong(&ok);
    return ok ? id : 0;
}

StationServer::ResultKey StationServer::resultKeyOf(const SessionMessage& result) const
{
    return ResultKey{sessionIdOfOwner(m_dispatcher->resultOwner()), result.commandVerb,
                     result.commandId};
}

bool StationServer::answersEveryKeyingCopy(const QByteArray& verb)
{
    return verb == "tx.key" || verb == "tx.unkey" || verb == "tx.tune" || verb == "tx.twoTone";
}

bool StationServer::hasReplySession(SessionTransport* transport, quint64 sessionId) const
{
    const Peer* peer = transport != nullptr ? peerPtr(transport) : nullptr;
    return sessionId != 0 && peer != nullptr && peer->sessionId == sessionId
        && (isStationTransport(transport) || (peer->authenticated && !peer->dropping));
}

void StationServer::consumeKeyingReply(const ResultKey& key)
{
    const auto count = m_keyingReplyCounts.find(key);
    if (count == m_keyingReplyCounts.end()) { return; }
    if (--*count == 0) {
        m_keyingReplyCounts.erase(count);
        m_resultRoutes.remove(key);
    }
}

bool StationServer::isLastResult(const SessionMessage& result)
{
    // A PureSignal action answers in phases; its route stays until the
    // completed or failed one. Any other command answers once.
    if (!result.commandVerb.startsWith("ps3.") || !result.accepted) {
        return true;
    }
    for (const MirrorUpdate& update : result.updates) {
        if (update.name == "phase") {
            const QString phase = update.value.toString();
            return phase == QLatin1String("completed") || phase == QLatin1String("failed");
        }
    }
    return true;
}

void StationServer::sendToPeer(SessionTransport* transport, const SessionMessage& original)
{
    if (isStationTransport(transport)) {
        deliverToStation(original);
        return;
    }
    if (transport == nullptr || !m_peers.contains(transport)) {
        return;
    }
    // iPhone app Task 14: the pairing code only to a connection signed in
    // with a paired device's key.
    SessionMessage message = withPairingCodeFor(transport, original);
    // iPhone app Task 73 (ruling 5.6): a device's own slices, and markers
    // for the others to a view with the feature.
    if (!ownershipAllows(transport, message)) {
        return;
    }
    logControlResult(transport, message);
    switch (message.kind) {
    case SessionMessageKind::Schema:
    case SessionMessageKind::ObjectCreate:
    case SessionMessageKind::Delta: {
        // R-IOS-13 / R-R3-49: transmit's txEqCurve only to a peer that
        // declared it; every other peer gets today's transmit. Phone wire
        // batch: a declared feature's properties only to a peer that
        // declared it; every other peer gets today's object.
        if (!fitTxEqCurveToPeer(transport, message)
            || !fitPeerOnlyProperties(transport, message)) {
            return;
        }
        // JJ's ruling of 2026-09-28: the rest of the TCI server's settings
        // only to a peer that declared stationTciSettings.
        if (!fitStationTciSettingsToPeer(transport, message)) {
            return;
        }
        // R-R3-46 / R-R3-11: stepAtt's other-ADC attenuator only to a peer
        // that declared adcAttenuators.
        if (!fitAdcAttenuatorsToPeer(transport, message)) {
            return;
        }
        const auto peer = m_peers.constFind(transport);
        const quint16 minor = peer != m_peers.cend() ? peer->agreedMinor
                                                     : kSessionProtocolMinor;
        // A Core without its controller behind the object does not offer
        // it (radioHardwareVersion 0), so it does not send it either.
        if (isStepAttMessage(message)
            && (minor < kRadioIdentitySessionProtocolMinor || radioHardwareVersion() < 1)) {
            return;
        }
        if (isAlexAntennasMessage(message)
            && (minor < kRadioIdentitySessionProtocolMinor || radioHardwareVersion() < 2)) {
            return;
        }
        if (isIoBoardMessage(message)
            && (minor < kRadioIdentitySessionProtocolMinor || radioHardwareVersion() < 3)) {
            return;
        }
        // R-R3-47: a Core that does not own its accessories does not offer
        // the amplifier and RF-Kit objects, so it does not send them either.
        if ((isAmplifierMessage(message) || isRfKitMessage(message))
            && (minor < kRadioIdentitySessionProtocolMinor
                || accessoryStatusVersion() < 1)) {
            return;
        }
        // R-R3-48: nor the station TCI object without a station server.
        if (isStationTciMessage(message)
            && (minor < kRadioIdentitySessionProtocolMinor || stationTciVersion() < 1)) {
            return;
        }
        // R-R3-47 / R-R3-22: nor the accessory records to an older app, or
        // from a Core that does not own its accessories.
        if (isAccessoryDataMessage(message)
            && (minor < kRadioIdentitySessionProtocolMinor || accessoryDataVersion() < 1)) {
            return;
        }
        // R-R3-47 / R-R3-22: nor the amp's and tuner's own settings.
        if (isAccessorySettingsMessage(message)
            && (minor < kRadioIdentitySessionProtocolMinor
                || (pgxlControlVersion() < 3 && tgxlControlVersion() < 1))) {
            return;
        }
        // iPhone app Task 13: nor the devices object to anyone but a device
        // that declares deviceAuth (today's desktop declares nothing).
        if (isDevicesMessage(message)
            && (minor < kRadioIdentitySessionProtocolMinor
                || !peerDeclares(transport, QByteArrayLiteral("deviceAuth"), 1)
                || deviceAdminVersion() < 1)) {
            return;
        }
        // iPhone app Task 71: nor who is on the Core to anyone but a view at
        // minor 11 that declared sessionHolder with deviceAuth
        // (sessionHolderVersion 1).
        if (isConnectedDevicesMessage(message) && !peerHasSessionHolderVersion(transport)) {
            return;
        }
        // iPhone app plan Task 39: nor the transmit state to anyone but a
        // peer at minor 11 whose hello declared remoteTx (txStateVersion 1).
        if (isTxStateMessage(message)
            && (minor < kRadioIdentitySessionProtocolMinor
                || !peerDeclares(transport, QByteArrayLiteral("remoteTx"), 1)
                || txStateVersion() < 1)) {
            return;
        }
        // iPhone app plan Task 25: nor the station computer's VAX to anyone
        // but a peer that declared vax, on a Core that publishes VAX.
        if (isVaxMessage(message)
            && (minor < kRadioIdentitySessionProtocolMinor
                || !peerDeclares(transport, QByteArrayLiteral("vax"), 1)
                || vaxVersion() < 1)) {
            return;
        }
        // R-R3-49 / R-IOS-18: nor the PA Gain profiles to a peer that did
        // not declare paProfiles.
        if (isPaProfilesMessage(message) && !peerGetsPaProfiles(transport)) {
            return;
        }
        // iPhone app Task 19: nor the catalogue to an older app.
        if (isCatalogMessage(message)
            && (minor < kRadioIdentitySessionProtocolMinor || stationCatalogVersion() < 1)) {
            return;
        }
        if (isSetupDescriptionMessage(message)
            && (minor < kRadioIdentitySessionProtocolMinor
                || !peerDeclares(transport, QByteArrayLiteral("setupDescription"), 1))) {
            return;
        }
        if (isSetupDescriptionMessage(message)
            && message.kind != SessionMessageKind::Schema) {
            SessionMessage fitted = message;
            const int declared = peer->features.value(QByteArrayLiteral("setupDescription"), 0);
            // 16: Hardware's HL2 Options rows. 17: Hardware's Alex-1 low-pass
            // rows (radioHardwareVersion 10). 18: HL2 Options' clock rows
            // (radioHardwareVersion 11). 19: DSP > CFC's band editor
            // (cfcProfile, cfc.setProfile). 20: PA Gain's on-the-air lock
            // per row. 21: CAT & Network's TCI Forget row greys out while
            // Duplicate is off. 22: DSP > Options' RX buffer sizes'
            // on-the-air lock. 23: Hardware > Calibration's Rx1 6m LNA row.
            // 24: Audio > TX Input's Line In Gain in 1.5 dB steps and the
            // Saturn G2's Mic Tip-Ring row.
            const int version = qMin(declared, 24);
            // Version 20: the transmit holder's own PA band stays live.
            const QByteArray deviceId = peerInfoFor(transport).deviceId;
            const bool holdsTransmit = m_transmitHolder && !deviceId.isEmpty()
                && m_transmitHolder->isHeldBy(deviceId);
            // The table describes the supported board's static row shape.
            // A disconnected radio withdraws the live row capability, but a
            // paired peer that negotiated rows keeps this description across
            // a later connection of the same board/SKU. Current radio/MAC
            // checks still govern every row command.
            const bool antennaRowsAvailable =
                peer->features.value(QByteArrayLiteral("radioAntennaRows")) == 1;
            for (MirrorUpdate& update : fitted.updates) {
                if (update.name != "revision") {
                    update.value = SetupDescription::fitCategoryForVersion(
                        update.value.toString(), version, antennaRowsAvailable,
                        holdsTransmit);
                }
            }
            transport->sendText(encodeFor(transport, fitted));
            return;
        }
        // Parity Task 19: nor the spot sources to an older app.
        if (isSpotSourcesMessage(message)
            && (minor < kRadioIdentitySessionProtocolMinor || recordStreamVersion() < 1)) {
            return;
        }
        if (!needsNnrFit(message, minor)) {
            // Most messages carry no NNR field: send them as they are.
            if (worthSendingAfterNnrFit(message)) {
                transport->sendText(encodeFor(transport, message));
            }
            return;
        }
        SessionMessage fitted = message;
        if (fitNnrLimitToPeer(fitted, minor)) {
            transport->sendText(encodeFor(transport, fitted));
        }
        return;
    }
    default:
        transport->sendText(encodeFor(transport, message));
        return;
    }
}

// ── The read-only TX EQ curve (R-IOS-13, R-R3-49) ───────────────────────

bool StationServer::peerGetsPaProfiles(SessionTransport* transport) const
{
    const auto peer = m_peers.constFind(transport);
    return peer != m_peers.cend() && !m_radioModel.isNull()
        && m_radioModel->role() != RadioModel::Role::Remote
        && m_radioModel->paProfileManager() != nullptr
        && peer->agreedMinor >= kRadioIdentitySessionProtocolMinor
        && peerDeclares(transport, QByteArrayLiteral("paProfiles"), 1);
}

QString StationServer::paProfileRefusal(SessionTransport* transport) const
{
    if (m_radioModel.isNull()) {
        return QStringLiteral("This Core cannot change its PA profiles.");
    }
    const QString key = QStringLiteral("hardware/%1/pa/profile/active")
                            .arg(m_radioModel->currentRadioMac());
    if (receiveOnlyRefusesKey(transport, key)) {
        return QString::fromLatin1(kReceiveOnlyTransmitReason);
    }
    if (!m_radioModel->receiveOnlyStationPolicy()) {
        const TxDecision decision = txDecisionFor(transport);
        if (!decision.permitted) {
            return decision.refusal.text;
        }
    }
    return {};
}

bool StationServer::peerGetsTxEqCurve(SessionTransport* transport) const
{
    const auto peer = m_peers.constFind(transport);
    return peer != m_peers.cend() && !m_radioModel.isNull()
        && peer->agreedMinor >= kRadioIdentitySessionProtocolMinor
        && peerDeclares(transport, QByteArrayLiteral("txEqCurve"), 1);
}

int StationServer::txEqCurveVersionFor(SessionTransport* transport) const
{
    if (!peerGetsTxEqCurve(transport)) {
        return 0;
    }
    const auto peer = m_peers.constFind(transport);
    return std::min(2, peer->features.value(QByteArrayLiteral("txEqCurve")));
}

// txEq.setCurve {curveJson} and txEq.resetCurve {}: the curve an app chose,
// or the panel's Reset, as the txEqParaEqData value the dialog would save
// (ParaEqCurve), written as this peer's own property write. The receive-only,
// transmit-permission, holder and on-air rules, the Core's range check and
// the echo rule (ruling 5.7) are that write's; its side-effect delta brings
// this peer the new txEqParaEqData and txEqCurve. The result carries the
// curve the Core kept (`curve`, the txEqCurve form).
void StationServer::handleTxEqCurveCommand(SessionTransport* transport,
                                           const SessionMessage& message)
{
    const auto answer = [this, transport, &message](bool accepted, const QString& reason,
                                                    const QList<MirrorUpdate>& values = {}) {
        send(transport, SessionMessages::commandResult(message.commandVerb, message.commandId,
                                                       accepted, reason, {}, values));
    };
    if (m_radioModel.isNull() || m_radioModel->role() != RadioModel::Role::Local) {
        answer(false, QStringLiteral("The Core cannot change its transmit settings."));
        return;
    }
    const bool reset = message.commandVerb == "txEq.resetCurve";
    ParaEqCurve::TxEqPoints points;
    if (reset) {
        if (!message.arguments.isEmpty()) {
            answer(false, QStringLiteral("The request to reset the TX EQ curve was not understood."));
            return;
        }
        points = ParaEqCurve::resetTxEqPoints(ParaEqCurve::txEqPointsFromParaEqData(
            m_radioModel->transmitModel().txEqParaEqData()));
    } else {
        const bool readable = message.arguments.size() == 1
            && message.arguments.first().name == "curveJson"
            && message.arguments.first().kind == MirrorWireKind::Utf8;
        if (!readable) {
            answer(false, QStringLiteral("The TX EQ curve was not understood."));
            return;
        }
        QString refusal;
        if (!ParaEqCurve::txEqPointsFromCurveJson(message.arguments.first().value.toString(),
                                                  points, &refusal)) {
            answer(false, refusal);
            return;
        }
    }
    const QString data = ParaEqCurve::txEqParaEqDataFromPoints(points);
    if (data.isEmpty()) {
        answer(false, QStringLiteral("The Core could not save this TX EQ curve."));
        return;
    }

    MirrorUpdate update;
    update.name = QByteArrayLiteral("txEqParaEqData");
    update.kind = MirrorWireKind::Utf8;
    update.value = data;
    for (const MirrorUpdate& known : m_mirror->snapshot(QByteArray(kTransmitKey))) {
        if (known.name == update.name) {
            update.ordinal = known.ordinal;
            break;
        }
    }
    // No writeId: the property result is this command's, and the written
    // value comes back in the side-effect delta with the curve.
    const SessionMessage write =
        SessionMessages::propertyWrite(QByteArray(kTransmitKey), {update});
    const QList<SessionPropertyResult> results =
        applyPropertyWrite(transport, write, /*answer=*/false, {});
    QString reason = QStringLiteral("The Core did not change the TX EQ curve.");
    bool accepted = false;
    for (const SessionPropertyResult& result : results) {
        if (result.property == update.name) {
            accepted = result.accepted;
            reason = result.reason;
        }
    }
    if (!accepted) {
        answer(false, reason);
        return;
    }
    answer(true, QString(),
           {{0, "curve", MirrorWireKind::Utf8, m_radioModel->transmitModel().txEqCurve()}});
}

bool StationServer::fitTxEqCurveToPeer(SessionTransport* transport,
                                       SessionMessage& message) const
{
    static const QByteArray kCurve = QByteArrayLiteral("txEqCurve");
    const bool transmit = message.kind == SessionMessageKind::Schema
        ? message.className == "TransmitModel"
        : message.objectKey == QByteArray(kTransmitKey);
    if (!transmit || peerGetsTxEqCurve(transport)) {
        return true;
    }
    switch (message.kind) {
    case SessionMessageKind::Schema:
        message.fields.removeIf([](const SessionSchemaField& field) {
            return field.name == kCurve;
        });
        return true;
    case SessionMessageKind::ObjectCreate:
        message.updates.removeIf([](const MirrorUpdate& update) {
            return update.name == kCurve;
        });
        return true;
    case SessionMessageKind::Delta: {
        const qsizetype before = message.updates.size();
        message.updates.removeIf([](const MirrorUpdate& update) {
            return update.name == kCurve;
        });
        // A delta that carried only the curve is not sent at all.
        return before == 0 || !message.updates.isEmpty();
    }
    default:
        return true;
    }
}

// transmitSettingsVersion 15: cfc.setProfile {profileJson, expectedRevision}.
// The CFC dialog's whole band editor from an app, applied at once as the
// cfcParaEqData value the dialog would save (CfcProfile::encode), written as
// this peer's own property write, so the receive-only, transmit-permission,
// holder and on-air rules and the echo rule are that write's. A revision the
// Core has moved past is refused before anything changes. The side-effect
// delta brings this peer the new cfcParaEqData, its ten-band values and
// cfcProfile; the result carries the profile the Core kept (`profile`).
void StationServer::handleCfcProfileCommand(SessionTransport* transport,
                                            const SessionMessage& message)
{
    const auto answer = [this, transport, &message](bool accepted, const QString& reason,
                                                    const QList<MirrorUpdate>& values = {}) {
        send(transport, SessionMessages::commandResult(message.commandVerb, message.commandId,
                                                       accepted, reason, {}, values));
    };
    if (m_radioModel.isNull() || m_radioModel->role() != RadioModel::Role::Local) {
        answer(false, QStringLiteral("The Core cannot change its transmit settings."));
        return;
    }
    // Permission first, in the order applyPropertyWrite gates a `transmit`
    // write: a peer that may not change the transmit settings is told so,
    // not that its table is stale or which value is out of range.
    if (m_radioModel->receiveOnlyStationPolicy()) {
        if (!transmitSettingsOffered(transport)) {
            answer(false, QString::fromLatin1(kReceiveOnlyTransmitReason));
            return;
        }
    } else {
        const TxDecision decision = txDecisionFor(transport);
        if (!decision.permitted) {
            answer(false, decision.refusal.text);
            return;
        }
    }
    QString profileJson;
    QString expectedRevision;
    bool haveProfile = false;
    bool haveRevision = false;
    for (const MirrorUpdate& argument : message.arguments) {
        if (argument.kind != MirrorWireKind::Utf8) {
            continue;
        }
        if (argument.name == "profileJson") {
            profileJson = argument.value.toString();
            haveProfile = true;
        } else if (argument.name == "expectedRevision") {
            expectedRevision = argument.value.toString();
            haveRevision = true;
        }
    }
    if (message.arguments.size() != 2 || !haveProfile || !haveRevision) {
        answer(false, QStringLiteral("The CFC settings were not understood."));
        return;
    }
    const QString current = m_radioModel->transmitModel().cfcProfile();
    const QString currentRevision = QJsonDocument::fromJson(current.toUtf8()).object()
                                        .value(QStringLiteral("revision")).toString();
    if (expectedRevision != currentRevision) {
        answer(false, QStringLiteral("The CFC settings changed on the Core. "
                                     "Check the new values and try again."));
        return;
    }
    CfcProfile::Profile profile;
    QString refusal;
    if (!CfcProfile::fromPublishedJson(profileJson, profile, &refusal)) {
        answer(false, refusal);
        return;
    }
    const QString data = CfcProfile::encode(profile);
    if (data.isEmpty()) {
        answer(false, QStringLiteral("The Core could not save these CFC settings."));
        return;
    }

    MirrorUpdate update;
    update.name = QByteArrayLiteral("cfcParaEqData");
    update.kind = MirrorWireKind::Utf8;
    update.value = data;
    for (const MirrorUpdate& known : m_mirror->snapshot(QByteArray(kTransmitKey))) {
        if (known.name == update.name) {
            update.ordinal = known.ordinal;
            break;
        }
    }
    // No writeId: the property result is this command's, and the written
    // value comes back in the side-effect delta with the profile.
    const SessionMessage write =
        SessionMessages::propertyWrite(QByteArray(kTransmitKey), {update});
    const QList<SessionPropertyResult> results =
        applyPropertyWrite(transport, write, /*answer=*/false, {});
    QString reason = QStringLiteral("The Core did not change the CFC settings.");
    bool accepted = false;
    for (const SessionPropertyResult& result : results) {
        if (result.property == update.name) {
            accepted = result.accepted;
            reason = result.reason;
        }
    }
    if (!accepted) {
        answer(false, reason);
        return;
    }
    answer(true, QString(),
           {{0, "profile", MirrorWireKind::Utf8, m_radioModel->transmitModel().cfcProfile()}});
}

// ── Properties for a declaring peer only (phone wire batch) ─────────────

bool StationServer::peerGetsFeatureProperties(SessionTransport* transport,
                                              const QByteArray& feature) const
{
    const auto peer = m_peers.constFind(transport);
    return peer != m_peers.cend() && !m_radioModel.isNull()
        && peer->agreedMinor >= kRadioIdentitySessionProtocolMinor
        && peerDeclares(transport, feature, 1);
}

bool StationServer::peerGetsCoreAddresses(SessionTransport* transport) const
{
    return peerGetsFeatureProperties(transport, QByteArrayLiteral("coreAddresses"))
        && peerDeclares(transport, QByteArrayLiteral("deviceAuth"), 1)
        && deviceAdminVersion() >= 1 && peerSeesPairingCode(transport);
}

bool StationServer::fitPeerOnlyProperties(SessionTransport* transport,
                                          SessionMessage& message) const
{
    const qsizetype before = message.updates.size();
    // State remains available without pattern negotiation. Fit the nested
    // pattern as well as the independent SliceModel property.
    if (!peerGetsFeatureProperties(transport, QByteArrayLiteral("diversityPattern"))) {
        for (MirrorUpdate& update : message.updates) {
            if (message.objectKey == "radio" && update.name == "diversityState") {
                QJsonObject state = QJsonDocument::fromJson(update.value.toString().toUtf8()).object();
                if (state.value("live").isObject()) {
                    QJsonObject live = state.value("live").toObject();
                    live.insert("pattern", QJsonValue(QJsonValue::Null));
                    state.insert("live", live);
                    update.value = QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact));
                }
            }
        }
    }
    for (const PeerOnlyProperty& entry : kPeerOnlyProperties) {
        const bool peerGetsIt = entry.deviceKeyOnly
            ? peerGetsCoreAddresses(transport)
            : peerGetsFeatureProperties(transport, QByteArray(entry.feature));
        if (!peerOnlyPropertyApplies(entry, message) || peerGetsIt) {
            continue;
        }
        const QByteArray name(entry.property);
        switch (message.kind) {
        case SessionMessageKind::Schema:
            message.fields.removeIf([&name](const SessionSchemaField& field) {
                return field.name == name;
            });
            break;
        case SessionMessageKind::ObjectCreate:
        case SessionMessageKind::Delta:
            message.updates.removeIf([&name](const MirrorUpdate& update) {
                return update.name == name;
            });
            break;
        default:
            break;
        }
    }
    // A delta that carried only such properties is not sent at all.
    return message.kind != SessionMessageKind::Delta || before == 0
        || !message.updates.isEmpty();
}

RecordBatch StationServer::fitRecordBatchToPeer(SessionTransport* transport,
                                                RecordBatch batch) const
{
    for (const PeerOnlyRecordField& entry : kPeerOnlyRecordFields) {
        if (batch.stream != QLatin1String(entry.stream)
            || peerGetsFeatureProperties(transport, QByteArray(entry.feature))) {
            continue;
        }
        const QString field = QString::fromLatin1(entry.field);
        for (RecordUpsert& upsert : batch.upserts) {
            upsert.fields.remove(field);
        }
    }
    return batch;
}

// ── The Core's TCI server settings (JJ's ruling of 2026-09-28) ───────────

bool StationServer::peerGetsStationTciSettings(SessionTransport* transport) const
{
    const auto peer = m_peers.constFind(transport);
    return peer != m_peers.cend() && stationTciVersion() >= 2
        && peer->agreedMinor >= kRadioIdentitySessionProtocolMinor
        && peerDeclares(transport, QByteArrayLiteral("stationTciSettings"), 1);
}

bool StationServer::fitStationTciSettingsToPeer(SessionTransport* transport,
                                                SessionMessage& message) const
{
    const bool stationTci = message.kind == SessionMessageKind::Schema
        ? message.className == "StationTciModel"
        : message.objectKey == "stationTci";
    if (!stationTci || peerGetsStationTciSettings(transport)) {
        return true;
    }
    const auto isSetting = [](const QByteArray& name) {
        return StationTciModel::setting(name) != nullptr;
    };
    switch (message.kind) {
    case SessionMessageKind::Schema:
        message.fields.removeIf([&](const SessionSchemaField& field) {
            return isSetting(field.name);
        });
        return true;
    case SessionMessageKind::ObjectCreate:
        message.updates.removeIf([&](const MirrorUpdate& update) {
            return isSetting(update.name);
        });
        return true;
    case SessionMessageKind::Delta: {
        const qsizetype before = message.updates.size();
        message.updates.removeIf([&](const MirrorUpdate& update) {
            return isSetting(update.name);
        });
        // A delta that carried only these settings is not sent at all.
        return before == 0 || !message.updates.isEmpty();
    }
    default:
        return true;
    }
}

// ── The other ADC's attenuator (R-R3-46, R-R3-11) ────────────────────────

bool StationServer::peerGetsAdcAttenuators(SessionTransport* transport) const
{
    const auto peer = m_peers.constFind(transport);
    return peer != m_peers.cend() && !m_radioModel.isNull()
        && peer->agreedMinor >= kRadioIdentitySessionProtocolMinor
        && radioHardwareVersion() >= 1
        && peerDeclares(transport, QByteArrayLiteral("adcAttenuators"), 1);
}

bool StationServer::fitAdcAttenuatorsToPeer(SessionTransport* transport,
                                            SessionMessage& message) const
{
    if (!isStepAttMessage(message) || peerGetsAdcAttenuators(transport)) {
        return true;
    }
    const auto added = [](const QByteArray& name) {
        return name == "rx2AttenuationDb" || name == "rx2SliceMask"
            || name == "rx2StepAttEnabled" || name == "rx2AutoAttEnabled"
            || name == "rx2AutoAttUndo" || name == "rx2AutoAttUndoDelayMs"
            || name == "rx2PreampMode";
    };
    switch (message.kind) {
    case SessionMessageKind::Schema:
        message.fields.removeIf([&added](const SessionSchemaField& field) {
            return added(field.name);
        });
        return true;
    case SessionMessageKind::ObjectCreate:
        message.updates.removeIf([&added](const MirrorUpdate& update) {
            return added(update.name);
        });
        return true;
    case SessionMessageKind::Delta: {
        const qsizetype before = message.updates.size();
        message.updates.removeIf([&added](const MirrorUpdate& update) {
            return added(update.name);
        });
        // A delta that carried only these is not sent at all.
        return before == 0 || !message.updates.isEmpty();
    }
    default:
        return true;
    }
}

// ── Slice ownership (iPhone app Task 73) ─────────────────────────────────

bool StationServer::ownershipAllows(SessionTransport* transport,
                                    const SessionMessage& message) const
{
    switch (message.kind) {
    case SessionMessageKind::Schema:
        // A marker's class only to a view with the feature; the access
        // class (slice control plan Task 4) only to a view that shares
        // slices.
        if (message.className == "SliceAccess") {
            return peerHasSliceAccess(transport);
        }
        return message.className != "SliceMarker" || peerHasSessionHolderVersion(transport);
    case SessionMessageKind::ObjectCreate:
    case SessionMessageKind::ObjectDestroy:
    case SessionMessageKind::Delta:
        break;
    default:
        return true;
    }
    // Slice control plan Task 4: who controls and who listens, to a view
    // that shares slices, every slice's.
    if (message.objectKey.startsWith("access:")) {
        return peerHasSliceAccess(transport);
    }
    const bool slice = message.objectKey.startsWith("slice:");
    const bool marker = message.objectKey.startsWith("marker:");
    if ((!slice && !marker) || m_radioModel.isNull()) {
        return true;
    }
    const auto peer = m_peers.constFind(transport);
    if (peer == m_peers.cend()) {
        return false;
    }
    const SliceOwnership* ownership = m_radioModel->sliceOwnership();
    int sliceId = -1;
    if (slice) {
        bool ok = false;
        sliceId = message.objectKey.mid(6).toInt(&ok);
        if (!ok) {
            return false;
        }
    } else {
        sliceId = SliceMarkerSet::sliceIdOf(message.objectKey);
        if (sliceId < 0) {
            return false;
        }
    }
    // One it may see is its slice: its controller or a listener (slice
    // control plan Task 2). A held slice is the station device's. A marker
    // goes to a view with the feature: for one that shares slices, every
    // slice it has not joined; for any other, never the slice of the device
    // it is (the one it is held for, when held).
    if (slice) {
        return SliceAccessPolicy::maySee(*ownership, peer->sessionDeviceId, sliceId);
    }
    return sliceFormsFor(transport, peer->sessionDeviceId, ownership->mark(sliceId),
                         ownership->listenersOf(sliceId))
        .marker;
}

StationServer::SliceForms StationServer::sliceFormsFor(SessionTransport* transport,
                                                       const QByteArray& device,
                                                       const SliceOwnership::Mark& mark,
                                                       const QList<QByteArray>& listeners) const
{
    SliceForms forms;
    // listeners (SliceOwnership::listenersOf) names the controller first.
    forms.slice = !device.isEmpty() && listeners.contains(device);
    if (!peerHasSessionHolderVersion(transport)) {
        return forms;
    }
    forms.marker = peerHasSliceAccess(transport) ? !forms.slice : mark.subject() != device;
    return forms;
}

QString StationServer::sliceHolderWords(const QByteArray& device) const
{
    // Slice control plan Task 17: who holds a slice, in a refusal. The Core
    // only for a slice nobody holds or the station device holds; otherwise
    // the device's name (the operator's own word, ruling 4.3), the plain
    // word for its kind when it has none, and "another device" when the
    // Core knows neither. kindWord reads an empty kind as a computer, so an
    // empty kind is checked first.
    if (device.isEmpty()) {
        return QStringLiteral("the Core");
    }
    const std::optional<ConnectedDevicesFacade::DeviceWords> words =
        m_connectedDevices->describe(device);
    if (words && !words->name.isEmpty()) {
        return words->name;
    }
    if (device == SliceOwnership::stationDevice()) {
        return QStringLiteral("the Core");
    }
    if (words && !words->kind.isEmpty()) {
        const QString kind = DeviceSessionRegistry::kindWord(words->kind).toLower();
        return QStringLiteral("a %1").arg(kind);
    }
    return QStringLiteral("another device");
}

QString StationServer::ownedElsewhereReason(int sliceId) const
{
    // The holder's name is the operator's own word (ruling 4.3), never held
    // to the wording rules; the sentence around it is.
    const QByteArray subject =
        m_radioModel ? m_radioModel->sliceOwnership()->mark(sliceId).subject() : QByteArray();
    const QString owner = sliceHolderWords(subject);
    return QStringLiteral("That slice belongs to %1. It can be changed only there.").arg(owner);
}

QString StationServer::listenerChangeReason(int sliceId) const
{
    // Slice control plan Task 2: a listener's refusal, for a window that
    // listens (Task 4 sends it); older windows keep ownedElsewhereReason.
    // The controller's name is the operator's own word (ruling 4.3), never
    // held to the wording rules; the sentence around it is.
    const QString letter = QString(QChar(QLatin1Char('A').unicode() + sliceId));
    const QByteArray controller =
        m_radioModel ? m_radioModel->sliceOwnership()->mark(sliceId).owner : QByteArray();
    if (m_radioModel && controller.isEmpty()) {
        return QStringLiteral("Nobody controls slice %1. Take control to change it.")
            .arg(letter);
    }
    const QString owner = sliceHolderWords(controller);
    return QStringLiteral("Slice %1 is controlled by %2. Take control to change it.")
        .arg(letter)
        .arg(owner);
}

// ── iPhone app plan Task 34: transmit ────────────────────────────────────

void StationServer::setRemoteTransmitAllowed(bool allowed)
{
    m_txGate.setRemoteTransmitAllowed(allowed);
    if (m_radioModel) {
        // The Core's blanket receive-only policy is the deny setting.
        m_radioModel->setReceiveOnlyStationPolicy(!allowed);
    }
    publishTxPermitted();
}

SessionPeerInfo StationServer::peerInfoFor(SessionTransport* transport) const
{
    SessionPeerInfo info;
    const auto it = m_peers.constFind(transport);
    if (it == m_peers.cend()) {
        return info;
    }
    info.deviceId = it->sessionDeviceId;
    info.declaresRemoteTx = it->agreedMinor >= kRadioIdentitySessionProtocolMinor
        && peerDeclares(transport, QByteArrayLiteral("remoteTx"), 1);
    info.paired = (!it->deviceId.isEmpty() && !it->sessionDeviceId.isEmpty()
                   && !it->sessionDeviceId.startsWith("token:"))
        || (m_tokenSessionsMayTransmitForTest && !it->sessionDeviceId.isEmpty());
    info.snapshotComplete = it->snapshotComplete;
    return info;
}

TxRefusal StationServer::txRefusalOf(const StationCapabilities& caps)
{
    TxRefusal refusal;
    refusal.code = caps.txRefusalCode.toLatin1();
    refusal.text = caps.txRefusalReason;
    refusal.fix = caps.txRefusalFix.toLatin1();
    return refusal;
}

TxDecision StationServer::txDecisionFor(SessionTransport* transport) const
{
    return m_txGate.decide(peerInfoFor(peerKey(transport)));
}

void StationServer::publishTxPermitted()
{
    // A session learns its txPermitted in `capabilities`, sent again (as
    // the display allowance's are, section 5.2) only when the value
    // changed. Not before its snapshot is complete: until then it is false
    // and the first capabilities already said so.
    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        if (!it->authenticated || !it->snapshotComplete) {
            continue;
        }
        // Desktop remote transmit: a peer told why it may not transmit is
        // told again when the reason changes (another holder, say).
        const StationCapabilities caps = buildCapabilitiesFor(it.key());
        const TxRefusal refusal = txRefusalOf(caps);
        if (caps.txPermitted == it->txPermittedSent && refusal == it->txRefusalSent
            && caps.txWatchPathVersion == it->txWatchPathVersionSent) {
            continue;
        }
        it->txPermittedSent = caps.txPermitted;
        it->txWatchPathVersionSent = caps.txWatchPathVersion;
        it->txRefusalSent = refusal;
        send(it.key(), SessionMessages::capabilities(caps.toUpdates()));
    }
}

// ── iPhone app plan Task 37: the watchdog, starvation and VOX ─────────────

void StationServer::followKeyedForWatchdog()
{
    if (!m_txWatchdog || !m_radioModel) {
        return;
    }
    // The key on the air now: a remote device's (its own, its program's,
    // TUNE, two-tone, or a VOX key that is its) is watched; the Core's own
    // keys are not.
    const RadioModel::KeyedBy keyedBy = m_radioModel->keyedBy();
    const bool remoteKey = !keyedBy.isEmpty()
        && keyedBy.deviceId != QByteArray(KeyerIdentity::kStationDeviceId);
    if (!m_watchedKeyedDevice.isEmpty() && (!remoteKey || keyedBy.deviceId != m_watchedKeyedDevice)) {
        // Ended (the device's release clears keyedBy at once, before MOX
        // reads off), or another device's key now.
        const QByteArray was = std::exchange(m_watchedKeyedDevice, QByteArray());
        m_txWatchdog->setKeyed(was, false);
    }
    if (remoteKey) {
        m_watchedKeyedDevice = keyedBy.deviceId;
        // Control logging lane: a new watch measures keepalive gaps afresh.
        if (!m_txWatchdog->isWatching(keyedBy.deviceId)) {
            m_controlLog.keepaliveWatchStarted(keyedBy.deviceId);
        }
        // A VOX key was never answered to the device, so it has no epoch
        // of it: any epoch counts.
        m_txWatchdog->setKeyed(keyedBy.deviceId, true,
                               keyedBy.trigger == QByteArrayLiteral("vox") ? 0 : keyedBy.epoch);
    }
}

void StationServer::disarmVoxArmedBy(const QByteArray& deviceId, const char* why)
{
    if (deviceId.isEmpty() || m_voxArmedBy != deviceId) {
        return;
    }
    m_voxArmedBy.clear();
    if (m_txWatchdog) {
        m_txWatchdog->setVoxArmed(deviceId, false);
    }
    emit voxArmedByChanged({});
    if (m_radioModel && m_radioModel->transmitModel().voxEnabled()) {
        qCInfo(lcStation) << "VOX armed by" << deviceId.toHex().constData() << "turned off:" << why;
        m_radioModel->transmitModel().setVoxEnabled(false);
    }
}

QString StationServer::deviceNameForStop(const QByteArray& deviceId) const
{
    if (m_connectedDevices) {
        if (const auto words = m_connectedDevices->describe(deviceId)) {
            if (!words->name.isEmpty()) {
                return words->name;
            }
        }
    }
    return QStringLiteral("a device");
}

void StationServer::txChannelMessage(quint64 mediaEpoch, const QByteArray& message,
                                     qint64 heldUs)
{
    quint64 sequence = 0;
    quint32 epoch = 0;
    if (!m_txWatchdog || !RemoteTxWatchdog::readChannelKeepalive(message, &sequence, &epoch)) {
        return;
    }
    const QByteArray deviceId = mediaSessionDevice(mediaEpoch);
    if (deviceId.isEmpty()) {
        return;
    }
    // Control logging lane: logged after, as it was watched.
    const bool watched = m_txWatchdog->isWatching(deviceId);
    m_txWatchdog->keepalive(deviceId, sequence, epoch, RemoteTxWatchdog::Path::TxChannel,
                            std::max<qint64>(0, heldUs) / 1000);
    controlLogKeepalive(deviceId, ControlLog::KeepaliveChannel::MediaTx,
                        std::max<qint64>(0, heldUs) / 1000, watched);
}

void StationServer::remoteMicStarved(const QByteArray& deviceId, bool starved)
{
    if (!m_radioModel || m_radioModel->keyedBy().deviceId != deviceId) {
        // Only a key that is this device's is its line's to stop.
        return;
    }
    if (m_radioModel->remoteRadioMicKeyActive(deviceId)) { return; }
    m_starvation.onStarved(deviceId, starved);
}

void StationServer::onTransmitHolderChanged()
{
    const std::optional<TransmitHolder::Holder> holder = m_transmitHolder->holder();
    // Parity Task 32 (JJ's MON ruling, 2026-09-26): while a remote device
    // holds transmit, its MON plays only on that device, so the Core's own
    // outputs leave it out; the station device (a hosting window, or the
    // radio's own PTT) hears it here as before.
    if (m_radioModel && m_radioModel->role() == RadioModel::Role::Local
        && m_radioModel->audioEngine() != nullptr) {
        const bool remoteHolder = holder && holder->source == TransmitHolder::Source::Device
            && holder->deviceId != KeyerIdentity::kStationDeviceId;
        m_radioModel->audioEngine()->setTxMonitorLocal(!remoteHolder);
    }
    if (m_radioModel && m_radioModel->role() == RadioModel::Role::Local) {
        // Ruling 5.11: the holder's active slice is the station-level one.
        const QByteArray id = holder && m_transmitHolder->state() == TransmitHolder::State::Held
            ? holder->deviceId
            : QByteArray();
        if (m_radioModel->sliceOwnership()->transmitHolder() != id) {
            m_radioModel->setTransmitHolder(id);
        }
    }
    m_connectedDevices->refresh();
    publishTxPermitted();
    // Task 77 (ruling 5.4a): TX marks only the holder's slice.
    refreshTxMarks();
    // Task 77 (ruling 8.10): a new holder's transmit slice.
    bindTransmitSliceForHolder();
    // Slice control plan Task 4: each slice's txSelected and onAir.
    if (m_sliceAccessSet) {
        m_sliceAccessSet->refresh();
    }
    // Fix wave I4 (ruling 8.1): txState names the holder for every device.
    if (m_transmitState) {
        TransmitState::Holder published;
        if (holder) {
            if (holder->deviceId == KeyerIdentity::kStationDeviceId) {
                published.deviceId = QString::fromLatin1(KeyerIdentity::kStationDeviceId);
            } else if (const auto words = m_connectedDevices->describe(holder->deviceId)) {
                published.deviceId = words->wireId;
            }
            published.name = holder->name;
            published.shortName = holder->shortName;
            published.kind = holder->kind;
            published.source = holder->source == TransmitHolder::Source::RadioPtt
                                   ? QStringLiteral("radioPtt")
                                   : QStringLiteral("device");
            published.sinceMs = holder->sinceMs;
            published.away = holder->away;
        }
        published.epoch = m_transmitHolder->epoch();
        // Fix wave 2 (the re-review's minor): true whenever every key is
        // refused for a transfer's reasons, so the window matches the
        // refusals: a transfer, a dropped holder's fence, and a transfer
        // that ended with MOX still on (no holder then).
        published.transferring = m_transmitHolder->state() == TransmitHolder::State::Transferring
            || m_transmitHolder->isFenced() || m_transmitHolder->isStopUnconfirmed();
        m_transmitState->setHolder(published);
    }
    // The merge of the trunk into the transmit lane (ruling 9.3): the
    // display budget is split around the holder, so a change of holder, or
    // of its away state, splits it again.
    publishDisplayBudgetCapabilities();
}

void StationServer::watchUnstartedTake(quint64 epoch)
{
    // After the call that took returns: its key has started (the holder is
    // keyed), is still on its way (MOX rising, or a two-tone start walking
    // its settle waits), or never will.
    QTimer::singleShot(0, this, [this, epoch]() {
        if (!m_transmitHolder || m_transmitHolder->epoch() != epoch
            || !m_transmitHolder->isTakeUnstarted()) {
            return;
        }
        const MoxController* mox = m_radioModel ? m_radioModel->moxController() : nullptr;
        const bool moxOn = mox != nullptr && (mox->isMox() || mox->state() != MoxState::Rx);
        const TwoToneController* twoTone =
            m_radioModel ? m_radioModel->twoToneController() : nullptr;
        // Task 77: a Tuner Genius autotune keys once the amplifier is in
        // standby (up to 1.5 s), so its take is still starting meanwhile.
        const bool starting = (twoTone != nullptr && twoTone->isActivationInFlight())
            || (m_radioModel && m_radioModel->isTgxlAutotuneInProgress());
        if (moxOn || starting) {
            QTimer::singleShot(kUnstartedTakeRecheckMs, this,
                               [this, epoch]() { watchUnstartedTake(epoch); });
            return;
        }
        m_transmitHolder->releaseUnstartedTake();
    });
}

void StationServer::releaseTransmitFor(const QByteArray& deviceId, const QString& reason)
{
    m_transmitHolder->release(deviceId, reason);
}

void StationServer::recordTransmitStop(const char* stopReason, const QString& text)
{
    // The first reason for a key wins (TransmitState::recordStop), so a
    // reason recorded here, before its StopAllTx, is not overwritten by the
    // generic `station` one that the stop itself raises.
    if (m_transmitState != nullptr) {
        m_transmitState->recordStop(QByteArray(stopReason), text);
    }
}

QString StationServer::noteHolderStopped(const QByteArray& deviceId, const char* stopReason)
{
    // Only a holder on the air is stopped; the first reason for its key
    // wins (TransmitState::recordStop).
    const std::optional<TransmitHolder::Holder> holder = m_transmitHolder->holder();
    if (m_transmitState == nullptr || !holder || holder->deviceId != deviceId) {
        return QString();
    }
    const bool onAir = holder->keyed || (m_radioModel && m_radioModel->isTransmitting());
    if (!onAir) {
        return QString();
    }
    const QByteArray code(stopReason);
    // A removed device is no longer in the store the holder's words are
    // read from; the key it is on the air with still names it.
    const QString name =
        holder->name.isEmpty() ? m_transmitState->lastKeyedByName() : holder->name;
    const QString text = code == TransmitState::kStopRevoked
        ? TransmitState::revokedText(name)
        : TransmitState::linkLostText(name);
    m_transmitState->recordStop(code, text);
    return text;
}

std::optional<TransmitHolder::Holder> StationServer::onAirHolder() const
{
    // Fix wave 2, Important 1: one notion of a holder on the air for
    // ruling 7.4's refusals, the shared-settings check, the planner's
    // takes and ruling 8.11's freeze. Every keyed holder counts, the
    // station device's own keys (the radio's PTT, the Core's own MOX or
    // TUNE, a Tuner Genius hardware TUNE) as much as a device's.
    if (!m_transmitHolder) {
        return std::nullopt;
    }
    std::optional<TransmitHolder::Holder> holder = m_transmitHolder->holder();
    if (!holder || !holder->keyed) {
        return std::nullopt;
    }
    return holder;
}

TxRefusal StationServer::onAirWords(const TransmitHolder::Holder& holder) const
{
    // D60's words with the holder's short name; "The radio is on the air."
    // after the radio's own PTT took transmit (ruling 7.4), and for the
    // station device's own keys on a Core no desktop hosts (the radio is
    // the only station there). A hosting desktop's own key is named after
    // the desktop (ruling 8.1).
    const bool radio = holder.source == TransmitHolder::Source::RadioPtt
        || (holder.deviceId == KeyerIdentity::kStationDeviceId && m_stationWords.name.isEmpty());
    return TxRefusals::holderOnAir(holder.shortName, radio);
}

void StationServer::setStationDeviceWords(const QString& name, const QString& shortName)
{
    m_stationWords.name = name;
    m_stationWords.shortName = shortName.isEmpty() ? name : shortName;
    if (m_transmitHolder && m_transmitHolder->holder()) {
        onTransmitHolderChanged();
    }
}

TxRefusal StationServer::onAirRefusal(const QByteArray& requester) const
{
    // Ruling 7.4 (D60): while the holder is on the air, a change from any
    // other device waits. The holder's own change, and the Core's own (no
    // requester), are not refused by this rule.
    const std::optional<TransmitHolder::Holder> holder = onAirHolder();
    if (!holder || requester.isEmpty() || requester == holder->deviceId) {
        return {};
    }
    return onAirWords(*holder);
}

int StationServer::stationFrozenSlice() const
{
    if (!m_radioModel) { return -1; }
    const auto holder = onAirHolder();
    const SliceModel* slice = m_radioModel->txBoundSlice();
    return StationSliceFreeze::frozenSlice(
        holder && holder->deviceId == KeyerIdentity::kStationDeviceId,
        slice ? slice->sliceIndex() : -1);
}

TxRefusal StationServer::stationFreezeRefusal(int sliceId) const
{
    const auto holder = onAirHolder();
    return StationSliceFreeze::refusal(sliceId, stationFrozenSlice(),
                                     holder ? onAirWords(*holder) : TxRefusal{});
}

TxRefusal StationServer::freezeRefusalFor(const SessionMessage& message) const
{
    // Ruling 8.11: what would retune, change, move or close the frozen
    // transmit slice. Fix wave 2, Important 3: the XIT too, since it moves
    // the transmitted frequency.
    if (message.kind == SessionMessageKind::PropertyWrite && message.objectKey.startsWith("slice:")) {
        bool ok = false;
        const int sliceId = message.objectKey.mid(6).toInt(&ok);
        if (!ok) {
            return {};
        }
        for (const MirrorUpdate& update : message.updates) {
            const QByteArray& n = update.name;
            if (n == "frequency" || n == "dspMode" || n == "filterLow" || n == "filterHigh"
                || n == "txAntenna" || n == "band" || n == "xitEnabled" || n == "xitHz") {
                return stationFreezeRefusal(sliceId);
            }
        }
        return {};
    }
    if (message.kind == SessionMessageKind::CommandInvoke
        && (message.commandVerb == "removeSlice" || message.commandVerb == "slice.selectBand"
            // Slice control fix wave (whole-branch review, Critical 1):
            // leaving the frozen slice could close it under the key.
            || message.commandVerb == "slice.release"
            || message.commandVerb == "slice.stopListening")) {
        for (const MirrorUpdate& a : message.arguments) {
            if (a.name == "sliceId") {
                return stationFreezeRefusal(static_cast<int>(a.value.toLongLong()));
            }
        }
    }
    return {};
}

TxRefusal StationServer::onAirPropertyRefusal(const QByteArray& requester,
                                              const QByteArray& objectKey,
                                              const QByteArray& property) const
{
    // The transmit path (ruling 7.8): an antenna, receive or transmit (a
    // slice's rxAntenna or txAntenna, the alexAntennas object), and
    // PureSignal (pureSignalSettings, transmit.pureSig).
    const bool transmitPath = objectKey == "alexAntennas" || objectKey == "pureSignalSettings"
        || (objectKey == QByteArray(kTransmitKey) && property == "pureSig")
        || (objectKey.startsWith("slice:") && (property == "rxAntenna" || property == "txAntenna"));
    return transmitPath ? onAirRefusal(requester) : TxRefusal{};
}

QString StationServer::changeRefusal(const QByteArray& requester, int sliceId) const
{
    if (m_radioModel.isNull() || requester.isEmpty()) {
        return {};
    }
    const SliceOwnership* ownership = m_radioModel->sliceOwnership();
    // A slice that is not there is the verb's own refusal ("no longer on
    // the Core"), not this one. Only the controller changes a slice (slice
    // control plan Task 2): a listener is refused like any other device.
    if (!ownership->isLive(sliceId)
        || SliceAccessPolicy::mayChange(*ownership, requester, sliceId)) {
        return {};
    }
    // Slice control plan Task 4: a device that listens and shares slices
    // is told who controls the slice and that it can take control; an
    // older window keeps today's words.
    if (ownership->isListening(requester, sliceId) && deviceSharesSlices(requester)) {
        return listenerChangeReason(sliceId);
    }
    return ownedElsewhereReason(sliceId);
}

bool StationServer::deviceSharesSlices(const QByteArray& device) const
{
    SessionTransport* transport = liveTransportFor(device);
    return transport != nullptr && peerHasSliceAccess(transport);
}

QJsonArray StationServer::listeningOn(const QByteArray& deviceId) const
{
    QJsonArray slices;
    if (m_radioModel.isNull() || deviceId.isEmpty()) {
        return slices;
    }
    for (int sliceId : m_radioModel->sliceOwnership()->ownedBy(deviceId)) {
        const SliceModel* slice = m_radioModel->sliceById(sliceId);
        if (slice == nullptr) {
            continue;
        }
        slices.append(QJsonObject{
            {QStringLiteral("sliceId"), sliceId},
            {QStringLiteral("letter"), QString(QChar(QLatin1Char('A').unicode() + sliceId))},
            {QStringLiteral("band"), static_cast<int>(slice->band())},
            {QStringLiteral("mode"), static_cast<int>(slice->dspMode())},
        });
    }
    return slices;
}

int StationServer::deviceLayoutLimit() const
{
    // The board's own cap, not maxSlices(), which reads 1 while the radio
    // is disconnected.
    const int board = m_radioModel ? m_radioModel->boardCapabilities().maxSlices : 0;
    return board > 0 ? std::min(board, WdspEngine::kMaxSliceChannels)
                     : WdspEngine::kMaxSliceChannels;
}

void StationServer::adoptForLoneDevice()
{
    if (m_radioModel.isNull() || m_radioModel->role() != RadioModel::Role::Local) {
        return;
    }
    const QList<DeviceSessionRegistry::Entry> entries = m_deviceSessions->entries();
    if (entries.size() == 1
        && !m_radioModel->sliceOwnership()->adoptUnowned(entries.first().deviceId).isEmpty()) {
        m_connectedDevices->refresh();
    }
}

bool StationServer::anotherDeviceHoldsAPlace(const QByteArray& deviceId) const
{
    const QList<DeviceSessionRegistry::Entry> entries = m_deviceSessions->entries();
    return std::any_of(entries.cbegin(), entries.cend(),
                       [&deviceId](const DeviceSessionRegistry::Entry& entry) {
                           return entry.deviceId != deviceId;
                       });
}

void StationServer::placeSlicesForAdmission(const QByteArray& deviceId)
{
    if (m_radioModel.isNull() || deviceId.isEmpty()
        || m_radioModel->role() != RadioModel::Role::Local) {
        return;
    }
    SliceOwnership* ownership = m_radioModel->sliceOwnership();
    AppSettings& store = AppSettings::instance();

    // 1. Slices the station device held for it are its own again.
    ownership->returnHeld(deviceId);

    // 2. Its saved slices, where they fit: its old letter when free, else
    //    the lowest free, each with its own settings; what does not fit is
    //    kept in its store and recorded for the notice (Task 74). A token
    //    window has none.
    const QString mac = m_radioModel->currentRadioMac();
    if (!deviceId.startsWith("token:") && !mac.isEmpty()) {
        const QList<SavedSlice> saved = DeviceLayoutStore::load(store, mac, deviceId);
        QList<SavedSlice> notRestored;
        for (const SavedSlice& entry : saved) {
            const int sliceId = m_radioModel->sliceById(entry.id) == nullptr
                    && entry.id < WdspEngine::kMaxSliceChannels
                ? entry.id
                : m_radioModel->lowestFreeSliceId();
            if (sliceId < 0) {
                notRestored.append(entry);
                continue;
            }
            DeviceLayoutStore::writeSliceSettings(store, mac, sliceId, entry.settings);
            const ReceiveSliceState state{sliceId, entry.panKey, entry.frequencyHz, entry.dspMode,
                                          {}, {}};
            QString reason;
            if (m_radioModel->restoreSliceFor(deviceId, sliceId, state, &reason) < 0) {
                qCInfo(lcStation) << "A saved slice did not fit:" << reason;
                DeviceLayoutStore::clearSliceSettings(store, mac, sliceId);
                notRestored.append(entry);
            }
        }
        if (!saved.isEmpty()) {
            DeviceLayoutStore::replace(store, mac, deviceId, notRestored, deviceLayoutLimit());
            m_radioModel->requestSettingsSave();
        }
        if (notRestored.isEmpty()) {
            m_slicesNotRestored.remove(deviceId);
        } else {
            m_slicesNotRestored.insert(deviceId, notRestored);
        }
    }

    // 3. The first device admitted while no other is on the Core adopts
    //    every slice nobody owns (never one held for another device).
    if (!anotherDeviceHoldsAPlace(deviceId)) {
        ownership->adoptUnowned(deviceId);
    }

    // 4. A device that still owns no slice gets one on the station-level
    //    active slice's frequency, on its receiver; with the slice cap full
    //    it starts with none.
    if (ownership->ownedBy(deviceId).isEmpty()) {
        const SliceOwnership::CreatorScope creator(ownership, deviceId);
        if (m_radioModel->addSlice(QString()) < 0) {
            qCInfo(lcStation) << "A device was admitted with no slice: the slice cap is full";
        }
    }
    m_connectedDevices->refresh();
}

bool StationServer::closeSliceFor(int sliceId, const QByteArray& saveFor, SavedSlice* closed,
                                  bool unclaimed)
{
    const QPointer<StationServer> self(this);
    if (m_radioModel.isNull()) {
        return false;
    }
    const SliceModel* slice = m_radioModel->sliceById(sliceId);
    // The Core keeps one slice: its last is never closed for another
    // device's reason. Slice control plan Task 8: a slice nobody is on
    // closes whatever the count (the claims rule).
    if (slice == nullptr
        || (unclaimed ? !m_radioModel->sliceOwnership()->unclaimed().contains(sliceId)
                      : m_radioModel->slices().size() <= 1)) {
        return false;
    }
    SavedSlice saved;
    saved.id = sliceId;
    saved.panKey = slice->panKey();
    saved.frequencyHz = slice->frequency();
    saved.dspMode = slice->dspMode();
    const QString mac = m_radioModel->currentRadioMac();
    // Removing saves the slice's own settings to its keys first.
    if (unclaimed) {
        m_radioModel->closeUnclaimedSlice(sliceId);
    } else {
        m_radioModel->removeSlice(sliceId);
    }
    if (!self || !m_radioModel) return false;
    if (m_radioModel->sliceById(sliceId) != nullptr) {
        return false;
    }
    AppSettings& store = AppSettings::instance();
    if ((!saveFor.isEmpty() || closed != nullptr) && !mac.isEmpty()) {
        saved.settings = DeviceLayoutStore::captureSliceSettings(store, mac, sliceId);
    }
    if (!saveFor.isEmpty() && !mac.isEmpty()) {
        DeviceLayoutStore::append(store, mac, saveFor, saved, deviceLayoutLimit());
    }
    // iPhone app Task 74: a slice another device took is kept by its
    // notice for Take it back, not in the device's saved layout.
    if (closed != nullptr) {
        *closed = saved;
    }
    // The letter's keys go with it, so another device's new slice there
    // starts from defaults (ruling 5.3).
    DeviceLayoutStore::clearSliceSettings(store, mac, sliceId);
    m_radioModel->requestSettingsSave();
    return true;
}

QByteArray StationServer::saveForAbsentSubject(int sliceId) const
{
    if (m_radioModel.isNull()) {
        return {};
    }
    const QByteArray subject = m_radioModel->sliceOwnership()->mark(sliceId).subject();
    if (subject.isEmpty() || subject == SliceOwnership::stationDevice()
        || subject.startsWith("token:") || m_deviceSessions->entry(subject)) {
        return {};
    }
    return subject;
}

void StationServer::saveTakenSlicesFor(const QByteArray& deviceId)
{
    const QPointer<StationServer> self(this);
    if (m_radioModel.isNull() || deviceId.isEmpty()) {
        return;
    }
    if (deviceId.startsWith("token:") || deviceId == SliceOwnership::stationDevice()) {
        // Nothing is saved for these, and no device returns to take back.
        m_confirm->endTakeBacks(deviceId);
        return;
    }
    // Fix wave 3 (the re-review's Minor 2): check it can save before Take
    // it back ends. With no radio connected there is no layout store to
    // save in, so Take it back is kept (it arrives with its notice) and
    // ends, the slice saved, at the end of the device's next away period.
    const QString mac = m_radioModel->currentRadioMac();
    if (mac.isEmpty()) {
        return;
    }
    const QList<ConfirmStep::Notice> taken = m_confirm->endTakeBacks(deviceId);
    if (!self) return;
    AppSettings& store = AppSettings::instance();
    bool saved = false;
    for (const ConfirmStep::Notice& notice : taken) {
        for (const SavedSlice& slice : notice.closed) {
            DeviceLayoutStore::append(store, mac, deviceId, slice, deviceLayoutLimit());
            saved = true;
        }
    }
    if (saved) {
        m_radioModel->requestSettingsSave();
    }
}

void StationServer::releaseDeviceClaims(const QByteArray& deviceId,
                                        std::optional<quint64> awayGeneration)
{
    const QPointer<StationServer> self(this);
    if (m_radioModel.isNull() || deviceId.isEmpty()
        || m_radioModel->role() != RadioModel::Role::Local) {
        return;
    }
    // Slice control plan Task 8: an old expiry never acts on a device that
    // came back (or dropped again since).
    if (awayGeneration && !m_deviceSessions->isCurrentAbsence(deviceId, *awayGeneration)) {
        return;
    }
    // Fix wave: a device gone for good takes its C-Tune pins with it; the
    // receivers it anchored keep their windows, unpinned.
    m_radioModel->clearStreamCtunPinsAnchoredBy(deviceId);
    if (!self || !m_radioModel) return;
    SliceOwnership* ownership = m_radioModel->sliceOwnership();
    // Ruling Q8: the transmit selection of each slice it controlled goes
    // with its control.
    QList<int> controlled = ownership->ownedBy(deviceId);
    controlled += ownership->heldFor(deviceId);
    for (int sliceId : std::as_const(controlled)) {
        clearTransmitSelection(deviceId, sliceId);
        if (!self || !m_radioModel) return;
    }
    // Take-over fix wave (I-2): a device that left keys nothing.
    m_lostTxSlice.remove(deviceId);
    // Approved policy 6: every control and listening claim goes at once.
    // No slice is held for it (Q12).
    const SliceOwnership::ClaimsRemoved removed = ownership->removeClaims(deviceId);
    if (!self || !m_radioModel) return;
    // A slice with nobody left on it closes, the Core's last included; one
    // it controlled is saved for a paired device, whose next admission
    // restores it. A slice others still listen to stays, with no
    // controller, for them.
    const bool token = deviceId.startsWith("token:");
    const bool saves = !token && deviceId != SliceOwnership::stationDevice();
    // A token window alone on the Core keeps today's rule: its slices pass
    // to nobody and stay, so the window signing in again (with the token,
    // or with the key it enrolled) adopts them with their tuning. It cannot
    // be recognised, and nothing is saved for it.
    const bool keptForAdoption = token && !anotherDeviceHoldsAPlace(deviceId);
    // Slice control fix wave (whole-branch review, Critical 1): a slice
    // still on the air (the radio's own PTT on it) is not closed under the
    // key; its close waits for the unkey (closeUnclaimedOrDefer).
    for (int sliceId : removed.releasedControl) {
        if (keptForAdoption || !ownership->unclaimed().contains(sliceId)) {
            continue;
        }
        closeUnclaimedOrDefer(sliceId, saves ? deviceId : QByteArray(), /*saveLayout=*/true);
        if (!self || !m_radioModel) return;
    }
    for (int sliceId : removed.leftListening) {
        if (ownership->unclaimed().contains(sliceId)) {
            closeUnclaimedOrDefer(sliceId, QByteArray(), /*saveLayout=*/false);
            if (!self || !m_radioModel) return;
        }
    }
    m_explicitTxSlice.remove(deviceId);
    m_connectedDevices->refresh();
}

void StationServer::onSliceOwnerChanged(int sliceId, const QByteArray& oldOwner,
                                        const QByteArray& oldHeldFor)
{
    Q_UNUSED(oldOwner);
    Q_UNUSED(oldHeldFor);
    if (m_radioModel.isNull()) {
        return;
    }
    // Fix wave I2: a question naming a slice that changed owner can no
    // longer be proceeded.
    dropQuestionsNaming(sliceId);
    // Slice control plan Task 4 (ruling Q8): a slice that passed from a
    // device is no longer one it took and has not chosen to transmit on.
    const QByteArray owner = m_radioModel->sliceOwnership()->mark(sliceId).owner;
    for (auto it = m_takenNotChosenForTx.begin(); it != m_takenNotChosenForTx.end(); ++it) {
        if (it.key() != owner) {
            it->remove(sliceId);
        }
    }
    // Slice control fix wave, round 2: an explicit transmit choice goes
    // whenever the slice leaves that device's control, however it left
    // (a layout restore, a token release and a revoke as well as take and
    // release).
    {
        const SliceOwnership& ownership = *m_radioModel->sliceOwnership();
        for (auto it = m_explicitTxSlice.begin(); it != m_explicitTxSlice.end();) {
            it = it.value() == sliceId
                    && !SliceAccessPolicy::mayChange(ownership, it.key(), sliceId)
                ? m_explicitTxSlice.erase(it)
                : std::next(it);
        }
    }
    // The markers name the new owner before any view is given one.
    m_markers->refreshOwners();
    // Task 77 (ruling 5.4a): the TX mark follows the owner.
    refreshTxMarks();
    m_connectedDevices->refresh();
    // Ruling 5.8: a change of owner reaches each view as object.destroy of
    // the form it had and object.create of the form it has now (slice
    // control plan Task 4: decided from the forms as they were last sent,
    // so the new controller that already listened is sent nothing).
    onSliceAccessChanged(sliceId);
}

void StationServer::onSliceAccessChanged(int sliceId)
{
    if (m_radioModel.isNull()) {
        return;
    }
    const SliceOwnership* ownership = m_radioModel->sliceOwnership();
    // A slice being made or removed is announced by its own object.create
    // and object.destroy.
    if (!ownership->isLive(sliceId)) {
        return;
    }
    const SliceFormState after{ownership->mark(sliceId), ownership->listenersOf(sliceId)};
    const auto known = m_sliceForms.constFind(sliceId);
    if (known == m_sliceForms.cend()) {
        // Changed before its sliceAdded reached the views: its creation
        // carries the forms as they are now.
        m_sliceForms.insert(sliceId, after);
        return;
    }
    const SliceFormState before = *known;
    if (before.mark == after.mark && before.listeners == after.listeners) {
        return;
    }
    m_sliceForms.insert(sliceId, after);
    if (m_ownerChangesInBurst) {
        // Every view is about to receive its whole burst again.
        return;
    }
    const QByteArray sliceKey = ObjectRegistry::keyForSlice(sliceId);
    const QByteArray markerKey = SliceMarkerSet::keyFor(sliceId);
    const QList<SessionTransport*> transports = m_peers.keys();
    for (SessionTransport* transport : transports) {
        const auto peer = m_peers.constFind(transport);
        if (peer == m_peers.cend() || peer->sessionDeviceId.isEmpty() || peer->view.isNull()
            || !peer->view->isAttached()) {
            continue;
        }
        const QByteArray device = peer->sessionDeviceId;
        // A device that no longer holds a place (revoked a moment ago; its
        // connection ends next) is told nothing more about slices.
        if (!m_deviceSessions->entry(device)) {
            continue;
        }
        const SliceForms had = sliceFormsFor(transport, device, before.mark, before.listeners);
        SliceForms has = sliceFormsFor(transport, device, after.mark, after.listeners);
        // Now: the one it may see (slice control plan Task 2).
        has.slice = SliceAccessPolicy::maySee(*ownership, device, sliceId);
        const QPointer<MirrorView> view = peer->view;
        if (had.slice && !has.slice) {
            send(transport, SessionMessages::objectDestroy(sliceKey, QByteArrayLiteral("SliceModel")));
        }
        if (had.marker && !has.marker) {
            send(transport, SessionMessages::objectDestroy(markerKey, QByteArrayLiteral("SliceMarker")));
        }
        if (!had.slice && has.slice && !view.isNull()) {
            if (const QObject* object = m_mirror->watchedObject(sliceKey)) {
                view->deliver(SessionMessages::objectCreate(
                    sliceKey, MirrorSchema::shortClassName(object->metaObject()->className()),
                    m_mirror->snapshot(sliceKey)));
            }
        }
        if (!had.marker && has.marker && !view.isNull()
            && m_mirror->watchedObject(markerKey) != nullptr) {
            view->deliver(SessionMessages::objectCreate(markerKey, QByteArrayLiteral("SliceMarker"),
                                                        m_mirror->snapshot(markerKey)));
        }
    }
}

// ── Slice control plan Task 4: listening and control ─────────────────────

QString StationServer::wireIdOf(const QByteArray& device) const
{
    if (device == SliceOwnership::stationDevice()) {
        return QString::fromLatin1(SliceOwnership::stationDevice());
    }
    if (const auto words = m_connectedDevices->describe(device)) {
        return words->wireId;
    }
    return device.startsWith("token:") ? QString::fromLatin1(device)
                                       : StationIdentity::toBase64Url(device);
}

bool StationServer::sliceTransmitting(int sliceId) const
{
    if (m_radioModel.isNull() || sliceId < 0) {
        return false;
    }
    // Ruling 8.11: the slice the station freeze holds.
    if (stationFrozenSlice() == sliceId) {
        return true;
    }
    // Slice control fix wave (Critical 1): the slice a transmit move waits
    // to land on (the unkey gate) counts as transmitting, so its control
    // cannot pass before the flag lands there.
    if (const TxSliceArbiter* arbiter = m_radioModel->txSliceArbiter();
        arbiter != nullptr && arbiter->pendingHandoffSliceId() == sliceId) {
        return true;
    }
    const SliceModel* txSlice = m_radioModel->txBoundSlice();
    if (txSlice == nullptr || txSlice->sliceIndex() != sliceId) {
        return false;
    }
    // The transmit slice while a holder is on the air, or while MOX has
    // not yet read off after its key ended.
    if (onAirHolder()) {
        return true;
    }
    const MoxController* mox = m_radioModel->moxController();
    return mox != nullptr && (mox->isMox() || mox->state() != MoxState::Rx);
}

QString StationServer::handOffRefusal(const QByteArray& controller, const QByteArray& taker,
                                      int sliceId) const
{
    const QString letter = QString(QChar(QLatin1Char('A').unicode() + sliceId));
    // Ruling Q7 once passed control only from a controller that stays on
    // as a listener; JJ's wider ruling (2026-09-30) lets every slice be
    // taken. What is left: the Core's own slice with nobody at its desktop,
    // for a taker below sliceAccessVersion 3 (its wire as before).
    if (controller == SliceOwnership::stationDevice()) {
        // Slice control plan Task 8: a slice the Core keeps for a device
        // that is not here may be taken, as an away device's slice may.
        if (!m_radioModel.isNull() && m_radioModel->sliceOwnership()->mark(sliceId).isHeld()) {
            return {};
        }
        // Take-over parity: a hosting desktop that takes its notices is
        // here as a device (Task 10). It stays on as a listener and is
        // told, so control passes from it under the checks below, as from
        // any device.
        // Core-slice take-over (JJ, 2026-09-30): with nobody at the
        // Core's desktop (no hosting desktop takes the station device's
        // notices, as on a headless Core), a device at sliceAccessVersion
        // 3 takes the Core's own slice with no one to ask. A peer below 3
        // is refused as before.
        if (liveTransportFor(controller) == nullptr) {
            SessionTransport* takerTransport = liveTransportFor(taker);
            if (takerTransport != nullptr && peerTakesCoreSlice(takerTransport)) {
                return {};
            }
            return QStringLiteral("Slice %1 is run by the Core itself, so control of it "
                                  "cannot pass to this device.")
                .arg(letter);
        }
    }
    // Core-slice take-over, JJ's wider ruling (2026-09-30): every other
    // slice can be taken; only a slice on the air is refused (the take's
    // own check). A controller that cannot stay on as a listener (a
    // session without the feature, or a device neither here nor away)
    // loses the slice (staysListeningAfterTake).
    return {};
}

bool StationServer::staysListeningAfterTake(const QByteArray& former) const
{
    // The Core's own position stays joined, a hosting desktop or not.
    if (former == SliceOwnership::stationDevice()) {
        return true;
    }
    SessionTransport* transport = liveTransportFor(former);
    if (transport == nullptr) {
        // Slice control plan Task 8: a device away within its 180 s stays
        // joined as a listener and finds itself one when it returns.
        const std::optional<DeviceSessionRegistry::Entry> away = m_deviceSessions->entry(former);
        return away && away->state == DeviceSessionRegistry::State::Away;
    }
    // Ruling Q7: only a session that shares slices can listen to one it
    // does not control.
    return peerHasSliceAccess(transport);
}

void StationServer::clearTransmitSelection(const QByteArray& former, int sliceId)
{
    if (m_radioModel.isNull()) {
        return;
    }
    // Ruling Q8 (b): its remembered transmit choice no longer names it.
    if (!former.isEmpty() && m_chosenTxSlice.value(former, -1) == sliceId) {
        m_chosenTxSlice.remove(former);
    }
    if (!former.isEmpty() && m_explicitTxSlice.value(former, -1) == sliceId) {
        m_explicitTxSlice.remove(former);
    }
    // Ruling Q8 (a), ruling 8.12's close path: while a device holds
    // transmit on this slice, the flag moves to another of the holder's
    // slices; with none, transmit is released to nobody. Never keyed here:
    // a take or a release of a transmitting slice was refused before this.
    // Slice control fix wave (the review's third-holder case): the holder
    // need not be the former controller. A device holding transmit with the
    // flag parked on this slice loses that selection as well, the taker
    // included (taking control grants no transmit). The radio's own PTT
    // keeps ruling 8.11: it transmits where the flag is.
    // TX rulings (JJ, 2026-09-30): that is a Core with no desktop. On a
    // hosting desktop that lost this slice to a take, its footswitch or mic
    // PTT moves the flag to the desktop's active slice once the key is
    // admitted and keys there (radioPttKeyRefusal).
    const SliceModel* txSlice = m_radioModel->txBoundSlice();
    // Take-over fix wave (I-2): the flag stays on this slice when nobody
    // holds transmit (or the radio's own PTT does), so the former
    // controller's next key would land on a slice it no longer controls.
    // Remember it; othersSliceKeyRefusal() reads this.
    if (!former.isEmpty() && txSlice != nullptr && txSlice->sliceIndex() == sliceId) {
        m_lostTxSlice[former].insert(sliceId);
    }
    if (!m_transmitHolder || txSlice == nullptr || txSlice->sliceIndex() != sliceId) {
        return;
    }
    const std::optional<TransmitHolder::Holder> holder = m_transmitHolder->holder();
    if (!holder || holder->deviceId.isEmpty()
        || holder->source == TransmitHolder::Source::RadioPtt) {
        return;
    }
    const QByteArray holderId = holder->deviceId;
    const SliceOwnership* ownership = m_radioModel->sliceOwnership();
    TxSliceArbiter* arbiter = m_radioModel->txSliceArbiter();
    for (SliceModel* slice : m_radioModel->slices()) {
        if (slice != nullptr && slice->sliceIndex() != sliceId && arbiter != nullptr
            && SliceAccessPolicy::mayTransmitOn(*ownership, holderId, slice->sliceIndex())) {
            if (arbiter->requestHandoff(slice->sliceIndex(), holderId)) {
                return;
            }
        }
    }
    qCInfo(lcStation) << "Control of the transmit slice passed; transmit is released";
    m_transmitHolder->release(holderId,
                              QStringLiteral("The device holding transmit no longer controls "
                                             "its transmit slice."));
}

bool StationServer::closeSliceNobodyIsOn(int sliceId)
{
    if (m_radioModel.isNull() || m_radioModel->sliceById(sliceId) == nullptr) {
        return false;
    }
    // Slice control plan Task 7: a slice nobody is on closes whatever the
    // count; the Core may be left with none.
    if (m_radioModel->sliceOwnership()->unclaimed().contains(sliceId)) {
        // Fix wave (whole-branch review, Critical 1): not under a key; the
        // close waits for the unkey.
        return closeUnclaimedOrDefer(sliceId, QByteArray(), /*saveLayout=*/false);
    }
    // Its controller's own close (a release with nobody else on it): the
    // last slice stays for the release to hand to nobody first.
    if (m_radioModel->slices().size() <= 1) {
        return false;
    }
    m_radioModel->removeSlice(sliceId);
    return m_radioModel && m_radioModel->sliceById(sliceId) == nullptr;
}

bool StationServer::closeUnclaimedOrDefer(int sliceId, const QByteArray& saveFor,
                                          bool saveLayout)
{
    if (m_radioModel.isNull() || m_radioModel->sliceById(sliceId) == nullptr) {
        return false;
    }
    // Slice control fix wave (whole-branch review, Critical 1): a slice on
    // the air is never closed under the key. Its close is kept for this
    // incarnation and runs once the radio is in receive, if nobody came
    // back to it meanwhile.
    if (sliceTransmitting(sliceId)) {
        DeferredClose deferred;
        deferred.incarnation = m_radioModel->sliceOwnership()->incarnation(sliceId);
        deferred.saveFor = saveFor;
        deferred.saveLayout = saveLayout;
        m_deferredCloses.insert(sliceId, deferred);
        qCInfo(lcStation) << "A slice nobody is on closes once it stops transmitting:" << sliceId;
        return false;
    }
    m_deferredCloses.remove(sliceId);
    if (saveLayout) {
        return closeSliceFor(sliceId, saveFor, nullptr, /*unclaimed=*/true);
    }
    return m_radioModel->closeUnclaimedSlice(sliceId);
}

void StationServer::scheduleDeferredCloses()
{
    if (m_deferredCloses.isEmpty() || m_deferredClosesQueued) {
        return;
    }
    m_deferredClosesQueued = true;
    QTimer::singleShot(0, this, [this]() {
        m_deferredClosesQueued = false;
        fireDeferredCloses();
    });
}

void StationServer::fireDeferredCloses()
{
    const QPointer<StationServer> self(this);
    if (m_radioModel.isNull() || m_deferredCloses.isEmpty()) {
        return;
    }
    const SliceOwnership* ownership = m_radioModel->sliceOwnership();
    const QList<int> ids = m_deferredCloses.keys();
    for (int sliceId : ids) {
        const auto it = m_deferredCloses.constFind(sliceId);
        if (it == m_deferredCloses.constEnd()) {
            continue;
        }
        const DeferredClose deferred = it.value();
        // Another slice on the id, or someone on it again: nothing to close.
        if (m_radioModel->sliceById(sliceId) == nullptr
            || ownership->incarnation(sliceId) != deferred.incarnation
            || !ownership->unclaimed().contains(sliceId)) {
            m_deferredCloses.remove(sliceId);
            continue;
        }
        if (sliceTransmitting(sliceId)) {
            continue;
        }
        closeUnclaimedOrDefer(sliceId, deferred.saveFor, deferred.saveLayout);
        if (!self || !m_radioModel) return;
    }
}

void StationServer::tellControlTaken(int sliceId, const QByteArray& former,
                                     const QByteArray& taker)
{
    if (m_radioModel.isNull() || former.isEmpty()) {
        return;
    }
    const SliceOwnership* ownership = m_radioModel->sliceOwnership();
    // Slice control plan Task 8: the station device is the former
    // controller of a slice it kept for a device that is not here; nobody
    // there listens (the hold does not join it). Take-over parity: of the
    // hosting desktop's own slice too, which it stays on as a listener,
    // and it is told.
    if (former == SliceOwnership::stationDevice()
        && !ownership->listenersOf(sliceId).contains(former)) {
        return;
    }
    // JJ's wider ruling (2026-09-30): a former controller that could not
    // stay on as a listener lost the slice. It is told nothing it cannot
    // read ("You are still listening" would be wrong); an older window
    // left with no slice ends as ruling 6.10 says.
    if (!ownership->listenersOf(sliceId).contains(former)) {
        endOlderWindowsWithoutSlices({former}, taker);
        return;
    }
    const SliceModel* slice = m_radioModel->sliceById(sliceId);
    if (slice == nullptr) {
        return;
    }
    const QString letter = QString(QChar(QLatin1Char('A').unicode() + sliceId));
    // Take-over parity: one Take it back per device and slice. The taker's
    // own records for the slice, and the former controller's older ones,
    // name a control revision that has passed.
    const auto sameSlice = [sliceId](const ConfirmStep::Notice& kept) {
        return kept.prompt.kind == QLatin1String("controlTaken")
            && kept.prompt.slices && !kept.prompt.slices->isEmpty()
            && kept.prompt.slices->first().toObject().value(QStringLiteral("sliceId")).toInt(-1)
                == sliceId;
    };
    m_confirm->forgetTakeBacks(taker, sameSlice);
    m_confirm->forgetTakeBacks(former, sameSlice);
    ConfirmStep::Notice notice;
    notice.device = former;
    notice.prompt.kind = QStringLiteral("controlTaken");
    // Take-over parity: Take it back is the same take the other way
    // (takeBackControl). A peer below sliceAccessVersion 2 is sent false
    // (sendNotice).
    notice.prompt.takeBack = true;
    notice.prompt.slices = QJsonArray{QJsonObject{
        {QStringLiteral("sliceId"), sliceId},
        {QStringLiteral("letter"), letter},
        {QStringLiteral("frequencyHz"), slice->frequency()},
        {QStringLiteral("mode"), static_cast<int>(slice->dspMode())},
        {QStringLiteral("band"), static_cast<int>(slice->band())},
        {QStringLiteral("incarnation"), static_cast<qint64>(ownership->incarnation(sliceId))},
        {QStringLiteral("controlRevision"),
         static_cast<qint64>(ownership->controlRevision(sliceId))},
    }};
    notice.reason = QStringLiteral("%1 took control of slice %2. You are still listening.")
                        .arg(planDevice(taker).name, letter);
    tellDevice(notice, taker);
}

void StationServer::setMediaEnabled(bool enabled)
{
    // A live session negotiated its capability already. Do not advertise a
    // different contract midway through it.
    if (!hasAuthenticatedSession()) {
        m_mediaEnabled = enabled;
    }
}

void StationServer::setTelemetryEnabled(bool enabled)
{
    if (!hasAuthenticatedSession()) { m_telemetryEnabled = enabled; }
}

bool StationServer::setDisplayBudgetLimits(const DisplayBudgetLimits& limits,
                                           DisplayBudgetReason reason)
{
    if (!limits.isValid()) { return false; }
    if (m_displayBudget) {
        if (*m_displayBudget == limits && reason == m_displayBudgetReason) { return true; }
        const quint32 delta = limits.generation - m_displayBudget->generation;
        if (delta == 0 || delta >= 0x80000000u) { return false; }
    }
    m_displayBudget = limits;
    m_displayBudgetReason = reason;
    // iPhone app Task 76: the new total is split among the media sessions.
    recomputeDisplayBudgetShares();
    const QPointer<StationServer> self(this);
    emit displayBudgetChanged(); // The sender sees new limits before publication.
    // A peer the computed budget does not reach heard of no budget, so a
    // change to it is nothing to that peer (legacy mode exactly).
    if (self) {
        self->publishBudgetToChangedSessions();
    }
    return true;
}

void StationServer::setDisplayBudgetEnforcementEnabled(bool enabled)
{
    m_displayBudgetEnforcementEnabled = enabled;
    publishDisplayBudgetCapabilities();
}

void StationServer::setPs3DisplayAdmissionHandler(Ps3DisplayAdmissionHandler handler)
{
    if (!handler) {
        m_ps3DisplayAdmission = {};
        return;
    }
    m_ps3DisplayAdmission = [handler = std::move(handler)](quint64, bool enabled,
                                                           QString* refusal) {
        return handler(enabled, refusal);
    };
}

void StationServer::setSessionPs3DisplayAdmissionHandler(SessionPs3DisplayAdmissionHandler handler)
{
    m_ps3DisplayAdmission = std::move(handler);
}

void StationServer::publishDisplayBudgetCapabilities()
{
    // iPhone app Task 76: split again (the PureSignal display's charge may
    // have moved) and tell each media session whose budget entries changed.
    if (recomputeDisplayBudgetShares()) {
        emit displayBudgetChanged();
    }
    publishBudgetToChangedSessions();
}

void StationServer::publishBudgetToChangedSessions()
{
    const QList<quint64> epochs = mediaSessionEpochs();
    for (quint64 epoch : epochs) {
        SessionTransport* transport = mediaSessionFor(epoch);
        if (transport == nullptr || !mediaAvailable(epoch)) {
            continue;
        }
        const QByteArray entries = budgetEntriesFor(transport);
        if (entries == peerFor(transport).publishedBudget) {
            continue;
        }
        m_peers[transport].publishedBudget = entries;
        const StationCapabilities caps = buildCapabilitiesFor(transport);
        m_peers[transport].txPermittedSent = caps.txPermitted;   // Task 34
        m_peers[transport].txWatchPathVersionSent = caps.txWatchPathVersion;
        m_peers[transport].txRefusalSent = txRefusalOf(caps);
        send(transport, SessionMessages::capabilities(caps.toUpdates()));
    }
}

QByteArray StationServer::budgetEntriesFor(SessionTransport* transport) const
{
    // Every budget entry the capabilities carry, as one comparable string.
    const StationCapabilities caps = buildCapabilitiesFor(transport);
    QByteArray key;
    key += QByteArray::number(caps.remoteDisplayBudgetVersion);
    if (caps.displayBudget) {
        key += ':' + QByteArray::number(caps.displayBudget->applicationBytesPerSecond);
        key += ':' + QByteArray::number(caps.displayBudget->spectrumSampleUnitsPerSecond);
        key += ':' + QByteArray::number(caps.displayBudget->generation);
    }
    key += caps.remotePs3DisplaySubscribed ? ":ps" : ":-";
    if (caps.displayBudgetReason) {
        key += ':' + QByteArray::number(static_cast<int>(*caps.displayBudgetReason));
    }
    return key;
}

std::optional<DisplayBudgetLimits> StationServer::displayBudgetTotalFor(
    SessionTransport* transport) const
{
    if (m_displayBudget && m_displayBudgetForReasonPeersOnly) {
        // R-R3-08/37: a computed ceiling is only for an app that can be told
        // why it is lowered. An older app keeps legacy mode exactly: no
        // budget, no pacing, no allocation results.
        const auto peer = m_peers.constFind(transport);
        if (peer == m_peers.cend()
            || peer->agreedMinor < kDisplayBudgetReasonSessionProtocolMinor) {
            return std::nullopt;
        }
    }
    return m_displayBudget;
}

int StationServer::displayBudgetSharingCount() const
{
    if (!m_displayBudget || !m_mediaEnabled) {
        return 0;
    }
    int count = 0;
    for (quint64 epoch : mediaSessionEpochs()) {
        SessionTransport* transport = mediaSessionFor(epoch);
        const auto peer = m_peers.constFind(transport);
        if (peer != m_peers.cend() && peer->agreedMinor >= kMediaSessionProtocolMinor
            && displayBudgetTotalFor(transport)) {
            ++count;
        }
    }
    return count;
}

QList<QPair<SessionTransport*, DisplayBudgetShare>> StationServer::splitDisplayBudget(
    std::optional<quint64> ps3Subscriber,
    std::optional<QPair<quint64, quint64>> additionalDemand) const
{
    // iPhone app Task 76 (ruling 9.3): every media session the total
    // reaches shares it, in admission order.
    QList<QPair<SessionTransport*, DisplayBudgetShare>> result;
    if (!m_displayBudget) {
        return result;
    }
    DisplayBudgetSplitInput input;
    input.total = *m_displayBudget;
    input.governorCut = m_displayBudgetReason == DisplayBudgetReason::CoreBusy;
    // Fix wave 2 (Critical 1): every admitted network device counts as
    // asking for at least one useful pan, the governor's own floor pan, so
    // a device that has not subscribed yet (or an older client planning
    // inside its share) keeps at least the smaller of one useful pan and
    // an equal part beside a device asking for the whole total, unless
    // that device is a present holder (rule 1; fix wave 3, design 9.3
    // "What the floor guarantees"). The link carries nothing that says a
    // device is sound only, so every device is floored.
    input.minimumRequest = DisplayLoadGovernor::floorPanCharge();
    QList<SessionTransport*> sharing;
    for (quint64 epoch : mediaSessionEpochs()) {
        SessionTransport* transport = mediaSessionFor(epoch);
        const Peer& peer = m_peers.find(transport).value();
        if (!m_mediaEnabled || peer.agreedMinor < kMediaSessionProtocolMinor
            || !displayBudgetTotalFor(transport)) {
            continue;
        }
        DisplayBudgetSplitDevice device;
        device.id = QByteArray::number(epoch);
        // Fix wave I5 (ruling 9.3): a device's request is its demand, what
        // its displays ask for as subscribed, not its grant, so max-min
        // fair shares (rules 2 and 3) and the holder's whole request (rule
        // 1, with Task 34) mean what they say. Without the media hub's
        // provider every device asks for the whole total, as before.
        if (m_displayDemand) {
            const DisplayBudgetCharge demand =
                m_displayDemand(epoch).value_or(DisplayBudgetCharge{});
            device.request = {demand.applicationBytesPerSecond,
                              demand.spectrumSampleUnitsPerSecond, 0};
        } else {
            device.request = {m_displayBudget->applicationBytesPerSecond,
                              m_displayBudget->spectrumSampleUnitsPerSecond, 0};
        }
        if (additionalDemand && additionalDemand->first == epoch) {
            device.request.applicationBytesPerSecond = std::min(
                kDisplayBudgetJsonSafePositiveLimit,
                device.request.applicationBytesPerSecond + additionalDemand->second);
        }
        device.previous = peer.budgetShare;
        device.previousReason = peer.budgetShareReason;
        input.devices.append(device);
        sharing.append(transport);
    }
    // Task 34's holder (the merge of the trunk into the transmit lane):
    // DisplayBudgetHolderKind::Station for the station device, or ::Device
    // with the holder's media epoch as holderId and holderAway while it is
    // away (it has no media then). Unheld otherwise (rule 3).
    if (const std::optional<TransmitHolder::Holder> holder =
            m_transmitHolder ? m_transmitHolder->holder() : std::nullopt) {
        if (holder->deviceId == KeyerIdentity::kStationDeviceId) {
            input.holderKind = DisplayBudgetHolderKind::Station;
        } else {
            input.holderKind = DisplayBudgetHolderKind::Device;
            input.holderAway = holder->away;
            for (quint64 epoch : mediaSessionEpochs()) {
                if (mediaSessionDevice(epoch) == holder->deviceId) {
                    input.holderId = QByteArray::number(epoch);
                }
            }
            if (input.holderId.isEmpty()) {
                // No media session: nothing to give it (rule 3, as away).
                input.holderAway = true;
            }
        }
    }
    if (ps3Subscriber) {
        input.ps3Subscriber = QByteArray::number(*ps3Subscriber);
    }
    const QList<DisplayBudgetShare> shares = DisplayBudgetSplit::split(input);
    for (qsizetype i = 0; i < sharing.size() && i < shares.size(); ++i) {
        result.append(qMakePair(sharing.at(i), shares.at(i)));
    }
    return result;
}

bool StationServer::recomputeDisplayBudgetShares()
{
    std::optional<quint64> subscriber;
    if (m_radioModel && m_radioModel->pureSignalFacade()->remoteAmpViewSubscribed()) {
        subscriber = m_ps3SubscriberEpoch != 0 ? m_ps3SubscriberEpoch : mediaSessionEpoch();
    }
    const QList<QPair<SessionTransport*, DisplayBudgetShare>> shares =
        splitDisplayBudget(subscriber);
    bool changed = false;
    QSet<SessionTransport*> sharing;
    for (const auto& [transport, share] : shares) {
        Peer& peer = m_peers[transport];
        if (peer.budgetShare != share.limits || peer.budgetShareReason != share.reason) {
            changed = true;
        }
        peer.budgetShare = share.limits;
        peer.budgetShareReason = share.reason;
        sharing.insert(transport);
    }
    // A session the total does not reach has no share.
    for (quint64 epoch : mediaSessionEpochs()) {
        SessionTransport* transport = mediaSessionFor(epoch);
        if (sharing.contains(transport)) {
            continue;
        }
        Peer& peer = m_peers[transport];
        if (peer.budgetShare) {
            changed = true;
        }
        peer.budgetShare.reset();
        peer.budgetShareReason = DisplayBudgetReason::None;
    }
    return changed;
}

std::optional<DisplayBudgetLimits> StationServer::displayBudgetLimitsAsPs3Subscriber(
    quint64 epoch) const
{
    SessionTransport* transport = mediaSessionFor(epoch);
    for (const auto& [sharing, share] : splitDisplayBudget(epoch)) {
        if (sharing == transport) {
            return share.limits;
        }
    }
    return displayBudgetLimits(epoch);
}

std::optional<DisplayBudgetLimits> StationServer::displayBudgetLimitsWithAdditionalDemand(
    quint64 epoch, quint64 applicationBytesPerSecond) const
{
    SessionTransport* transport = mediaSessionFor(epoch);
    std::optional<quint64> subscriber;
    if (m_radioModel && m_radioModel->pureSignalFacade()->remoteAmpViewSubscribed()) {
        subscriber = m_ps3SubscriberEpoch != 0 ? m_ps3SubscriberEpoch : mediaSessionEpoch();
    }
    for (const auto& [sharing, share] :
         splitDisplayBudget(subscriber, qMakePair(epoch, applicationBytesPerSecond))) {
        if (sharing == transport) { return share.limits; }
    }
    return displayBudgetLimits(epoch);
}

SessionTransport* StationServer::mediaSessionFor(quint64 epoch) const
{
    if (epoch == 0) {
        return nullptr;
    }
    for (auto it = m_peers.cbegin(); it != m_peers.cend(); ++it) {
        if (it->mediaEpoch == epoch && it->authenticated) {
            return it.key();
        }
    }
    return nullptr;
}

SessionTransport* StationServer::primaryMediaSession() const
{
    SessionTransport* primary = nullptr;
    quint64 lowest = 0;
    for (auto it = m_peers.cbegin(); it != m_peers.cend(); ++it) {
        if (it->mediaEpoch != 0 && it->authenticated
            && (primary == nullptr || it->mediaEpoch < lowest)) {
            primary = it.key();
            lowest = it->mediaEpoch;
        }
    }
    return primary;
}

QList<quint64> StationServer::mediaSessionEpochs() const
{
    QList<quint64> epochs;
    for (auto it = m_peers.cbegin(); it != m_peers.cend(); ++it) {
        if (it->mediaEpoch != 0 && it->authenticated) {
            epochs.append(it->mediaEpoch);
        }
    }
    std::sort(epochs.begin(), epochs.end());
    return epochs;
}

quint64 StationServer::mediaSessionEpoch() const
{
    SessionTransport* primary = primaryMediaSession();
    return primary != nullptr ? peerFor(primary).mediaEpoch : 0;
}

QByteArray StationServer::mediaSessionDevice(quint64 epoch) const
{
    SessionTransport* transport = mediaSessionFor(epoch);
    return transport != nullptr ? peerFor(transport).sessionDeviceId : QByteArray();
}

bool StationServer::mediaSessionControlsSlice(quint64 epoch, int sliceId) const
{
    // Ruling 9.1: a device's own slices, as SliceOwnership records them;
    // slice control plan Task 2: the controller only.
    const QByteArray device = mediaSessionDevice(epoch);
    if (device.isEmpty() || m_radioModel.isNull()
        || m_radioModel->sliceOwnership() == nullptr) {
        return false;
    }
    return SliceAccessPolicy::mayChange(*m_radioModel->sliceOwnership(), device, sliceId);
}

bool StationServer::mediaSessionHearsSlice(quint64 epoch, int sliceId) const
{
    const QByteArray device = mediaSessionDevice(epoch);
    if (device.isEmpty() || m_radioModel.isNull()
        || m_radioModel->sliceOwnership() == nullptr) {
        return false;
    }
    return SliceAccessPolicy::mayHear(*m_radioModel->sliceOwnership(), device, sliceId);
}

bool StationServer::mediaSessionSeesSlice(quint64 epoch, int sliceId) const
{
    const QByteArray device = mediaSessionDevice(epoch);
    if (device.isEmpty() || m_radioModel.isNull()
        || m_radioModel->sliceOwnership() == nullptr) {
        return false;
    }
    return SliceAccessPolicy::maySee(*m_radioModel->sliceOwnership(), device, sliceId);
}

bool StationServer::mediaAvailableFor(SessionTransport* transport) const
{
    const auto it = m_peers.constFind(transport);
    return m_mediaEnabled && it != m_peers.cend() && it->authenticated && it->mediaEpoch != 0
        && it->snapshotComplete && it->agreedMinor >= kMediaSessionProtocolMinor;
}

bool StationServer::telemetryAvailable() const
{
    return telemetryAvailable(mediaSessionEpoch());
}

bool StationServer::telemetryAvailable(quint64 epoch) const
{
    const auto it = m_peers.constFind(mediaSessionFor(epoch));
    return m_telemetryEnabled && it != m_peers.cend() && it->authenticated
        && it->snapshotComplete && it->agreedMinor >= kStationTelemetrySessionProtocolMinor;
}

bool StationServer::sendTelemetry(const StationTelemetrySnapshot& snapshot,
                                  quint64 expectedEpoch)
{
    if (!telemetryAvailable(expectedEpoch)) { return false; }
    SessionTransport* transport = mediaSessionFor(expectedEpoch);
    SessionMessage message;
    message.kind = SessionMessageKind::StationTelemetry;
    message.telemetry = snapshot;
    // A peer from before host telemetry receives exactly the radio and audio
    // sections it was built for.
    const auto peer = m_peers.constFind(transport);
    if (peer == m_peers.cend() || peer->agreedMinor < kCoreHostTelemetrySessionProtocolMinor) {
        message.telemetry.host = {};
    }
    // Likewise a peer from before receiver load receives exactly the
    // sections it negotiated, without "receivers".
    if (peer == m_peers.cend() || peer->agreedMinor < kReceiverLoadSessionProtocolMinor) {
        message.telemetry.receivers.reset();
        // R-R3-32 (parity Task 6): and without the radio's PA readings and
        // link quality (stationTelemetryVersion 4, minor 11).
        message.telemetry.radio.clearRadioStatus();
        // R-R3-32 (parity Task 14): and without the HL2 link
        // (stationTelemetryVersion 5, minor 11).
        message.telemetry.radio.clearHl2Link();
        message.telemetry.radio.clearRadioDiagnostics();
    }
    // Parity Task 22: the newest measurement, for the support bundle.
    if (const std::optional<QJsonObject> encoded = StationTelemetryCodec::encode(snapshot)) {
        m_lastTelemetry = *encoded;
        m_lastTelemetryAtMs = QDateTime::currentMSecsSinceEpoch();
    }
    const QByteArray wire = SessionMessages::encode(message);
    if (wire.isEmpty()) { return false; }
    transport->sendText(wire);
    return true;
}

bool StationServer::mediaAvailable() const
{
    return mediaAvailable(mediaSessionEpoch());
}

std::optional<IceConfiguration> StationServer::sessionIceConfiguration(quint64 epoch) const
{
    SessionTransport* session = mediaSessionFor(epoch);
    SessionTransport* carrying = session;
    if (const auto* switchable = qobject_cast<const SwitchableTransport*>(session)) {
        carrying = switchable->inner();
    }
    if (const auto* transport = qobject_cast<const DataChannelTransport*>(carrying)) {
        return transport->mediaIceConfiguration();
    }
    // iPhone app plan Task 29 (link section 21.3): a session that came
    // through the service and moved to a direct connection keeps the
    // service's STUN server for its media, and no relay of its own.
    const auto it = m_peers.constFind(session);
    if (it != m_peers.cend() && it->serviceIce) {
        return it->serviceIce->withoutOwnRelay();
    }
    return std::nullopt;
}

bool StationServer::mediaAvailable(quint64 epoch) const
{
    return mediaAvailableFor(mediaSessionFor(epoch));
}

bool StationServer::remoteWidebandAvailable() const
{
    return remoteWidebandAvailable(mediaSessionEpoch());
}

bool StationServer::remoteWidebandAvailable(quint64 epoch) const
{
    const auto it = m_peers.constFind(mediaSessionFor(epoch));
    return mediaAvailable(epoch) && it != m_peers.cend()
        && it->agreedMinor >= kRemoteWidebandSessionProtocolMinor;
}

bool StationServer::remoteAudioStatusAvailable() const
{
    return remoteAudioStatusAvailable(mediaSessionEpoch());
}

bool StationServer::remoteAudioStatusAvailable(quint64 epoch) const
{
    const auto it = m_peers.constFind(mediaSessionFor(epoch));
    return mediaAvailable(epoch) && it != m_peers.cend()
        && it->agreedMinor >= kRemoteAudioStatusSessionProtocolMinor;
}

bool StationServer::spectrumGrantAvailable() const
{
    return spectrumGrantAvailable(mediaSessionEpoch());
}

bool StationServer::spectrumGrantAvailable(quint64 epoch) const
{
    const auto it = m_peers.constFind(mediaSessionFor(epoch));
    return mediaAvailable(epoch) && it != m_peers.cend()
        && it->agreedMinor >= kRemoteSpectrumGrantSessionProtocolMinor;
}

bool StationServer::displayExtrasAvailable() const
{
    return displayExtrasAvailable(mediaSessionEpoch());
}

bool StationServer::displayExtrasAvailable(quint64 epoch) const
{
    // Advertised in the minor-11 capabilities block only, so only a peer
    // that agreed minor 11 was told it may ask.
    const auto it = m_peers.constFind(mediaSessionFor(epoch));
    return mediaAvailable(epoch) && it != m_peers.cend()
        && it->agreedMinor >= kRadioIdentitySessionProtocolMinor
        && displayExtrasVersion() >= 1;
}

bool StationServer::audioQualityAvailable(quint64 epoch) const
{
    // iPhone app plan Task 23: told only to a minor-11 peer that declared
    // audioQuality 1 (StationCapabilities::audioQualityVersion).
    SessionTransport* transport = mediaSessionFor(epoch);
    const auto it = m_peers.constFind(transport);
    return mediaAvailable(epoch) && it != m_peers.cend()
        && it->agreedMinor >= kRadioIdentitySessionProtocolMinor
        && peerDeclares(transport, QByteArrayLiteral("audioQuality"), 1);
}

bool StationServer::remoteTxAvailableForMedia(quint64 epoch) const
{
    SessionTransport* transport = mediaSessionFor(epoch);
    const auto it = m_peers.constFind(transport);
    return mediaAvailable(epoch) && it != m_peers.cend()
        && it->agreedMinor >= kRadioIdentitySessionProtocolMinor
        && peerDeclares(transport, QByteArrayLiteral("remoteTx"), 1);
}

bool StationServer::mediaSessionTxPermitted(quint64 epoch) const
{
    SessionTransport* transport = mediaSessionFor(epoch);
    return transport != nullptr && txDecisionFor(transport).permitted;
}

void StationServer::setDisplayBudgetForReasonPeersOnly(bool reasonPeersOnly)
{
    m_displayBudgetForReasonPeersOnly = reasonPeersOnly;
    recomputeDisplayBudgetShares();
}

std::optional<DisplayBudgetLimits> StationServer::displayBudgetLimits() const
{
    SessionTransport* primary = primaryMediaSession();
    if (primary == nullptr) {
        // What a first session would be given: the whole total.
        return displayBudgetTotalFor(nullptr);
    }
    return displayBudgetLimits(peerFor(primary).mediaEpoch);
}

std::optional<DisplayBudgetLimits> StationServer::displayBudgetLimits(quint64 epoch) const
{
    const auto it = m_peers.constFind(mediaSessionFor(epoch));
    if (it == m_peers.cend()) {
        return std::nullopt;
    }
    return it->budgetShare;
}

DisplayBudgetReason StationServer::displayBudgetShareReason(quint64 epoch) const
{
    const auto it = m_peers.constFind(mediaSessionFor(epoch));
    return it != m_peers.cend() ? it->budgetShareReason : DisplayBudgetReason::None;
}

bool StationServer::displayBudgetAvailable() const
{
    return displayBudgetAvailable(mediaSessionEpoch());
}

bool StationServer::displayBudgetAvailable(quint64 epoch) const
{
    const auto it = m_peers.constFind(mediaSessionFor(epoch));
    return mediaAvailable(epoch) && m_displayBudgetEnforcementEnabled
        && displayBudgetLimits(epoch) && it != m_peers.cend()
        && it->agreedMinor >= kRemoteDisplayBudgetSessionProtocolMinor;
}

bool StationServer::sendMediaControl(const QJsonObject& payload, quint64 expectedEpoch)
{
    if (!mediaAvailable(expectedEpoch)) {
        return false;
    }
    SessionMessage message;
    message.kind = SessionMessageKind::MediaControl;
    message.mediaPayload = payload;
    const QByteArray wire = SessionMessages::encode(message);
    if (wire.isEmpty()) {
        return false;
    }
    mediaSessionFor(expectedEpoch)->sendText(wire);
    return true;
}

// ── Capability descriptor ────────────────────────────────────────────────

void StationServer::setSustainableSliceLimit(int slices)
{
    if (slices < 1) {
        return;
    }
    m_sustainableSliceLimit = slices;
}

int StationServer::accessoryStatusVersion() const
{
    return !m_radioModel.isNull() && m_radioModel->stationAccessoryIdentityEnabled() ? 1 : 0;
}

int StationServer::pgxlControlVersion() const
{
    // 3: the amp's own settings (`accessorySettings` and its verbs), sent
    // by the Core's station controller (R-R3-47 / R-R3-22).
    // 4: setPgxlOperate, scanPgxlLan and setPgxlAddress, R-R3-49 (parity
    // Task 9).
    return accessoryStatusVersion() >= 1 ? 4 : 0;
}

// R-R3-49 (parity Task 1): the one list of transmit settings keys a
// receive-only Core takes while its radio is off the air. Today the
// DSP > Options TX combos (DspOptions<Setting><Mode>Tx); later tasks add
// their keys here.
bool StationServer::isTransmitSettingKeyAcceptedOffAir(const QString& key)
{
    // R-R3-49 (parity Task 5): and Setup > Transmit > Power's SWR Protection
    // and External TX Inhibit keys.
    // R-R3-46 / R-R3-49 (parity Task 6): and Setup > PA's PA profiles and
    // PA forward-power table.
    // R-R3-46 / R-R3-49 (parity Task 13): and Hardware Config's OC transmit
    // pins, OC pin actions, TX Display Cal and Volts/Amps Calibration.
    if (isTransmitDspOptionsKey(key) || isPowerPageTransmitKey(key)
        || isPaPageTransmitKey(key)) {
        return true;
    }
    const QStringList parts = key.toLower().split(QLatin1Char('/'));
    return isOcTransmitPinKey(parts) || isTransmitHardwareKeyTakenOnAir(parts);
}

bool StationServer::isTransmitSettingKeyTakenOnAir(const QString& key)
{
    // R-R3-46 / R-R3-49 (parity Task 13): the keys Thetis changes while
    // transmitting (isTransmitHardwareKeyTakenOnAir).
    // transmitSettingsVersion 11: and "Disable HF PA", which Thetis applies
    // with no MOX check (setup.cs:16750-16754 [v2.10.3.15]
    // chkHFTRRelay_CheckedChanged: console.HFTRRelay = chkHFTRRelay.Checked).
    return key == QLatin1String(RadioModel::kDisableHfPaKey)
        || isTransmitHardwareKeyTakenOnAir(key.toLower().split(QLatin1Char('/')));
}

bool StationServer::transmitSettingsOffered(SessionTransport* transport) const
{
    // Sent only at agreed minor 11 (the minor-11 capabilities block).
    return transport != nullptr
        && peerFor(transport).agreedMinor >= kRadioIdentitySessionProtocolMinor
        && transmitSettingsVersion() >= 1;
}

bool StationServer::receiveOnlyRefusesKey(SessionTransport* transport,
                                          const QString& key) const
{
    if (m_radioModel.isNull() || !m_radioModel->receiveOnlyStationPolicy()
        || !isReceiveOnlyRefusedKey(key)) {
        return false;
    }
    // R-R3-46 / R-R3-49 (parity Task 14): the Alex tab's three transmit
    // high-pass switches, from a peer offered radioHardwareVersion 7, are
    // taken on and off the air (isAlexHpfTransmitSwitchKey).
    if (isAlexHpfTransmitSwitchKey(key)) {
        const auto peer = m_peers.constFind(transport);
        return peer == m_peers.cend()
            || peer->agreedMinor < kRadioIdentitySessionProtocolMinor
            || radioHardwareVersion() < 7;
    }
    // radioHardwareVersion 10: the Alex-1 low-pass rows (isAlexLpfRowKey),
    // likewise on and off the air.
    if (isAlexLpfRowKey(key)) {
        const auto peer = m_peers.constFind(transport);
        return peer == m_peers.cend()
            || peer->agreedMinor < kRadioIdentitySessionProtocolMinor
            || radioHardwareVersion() < 10;
    }
    // R-R3-49 (parity Task 1): a key on the off-air list, from a peer that
    // was offered it, is the on-air check's (transmitSettingOnAirRefusal).
    return !(isTransmitSettingKeyAcceptedOffAir(key) && transmitSettingsOffered(transport));
}

QString StationServer::transmitSettingOnAirRefusal(const QString& key) const
{
    QString reason;
    // Setup description version 22: the DSP > Options RX buffer sizes wait
    // while the radio is on the air, whoever asks, as Thetis greys the
    // whole Buffer Size (IQcomp) group while MOX is on:
    // From Thetis setup.cs:5159 [v2.10.3.15] grpDSPBufferSize.Enabled = !mox;
    // The words are the ones the description's lock shows.
    if (RadioModel::isRxDspBufferSizeKey(key) && m_radioModel
        && m_radioModel->stationOnAirRefusal(nullptr)) {
        return RadioModel::dspBufferOnAirLockedReason();
    }
    // Changing a transmit region, including removing it to restore the
    // default, always waits for RX, regardless of who holds transmit. So
    // do Extended transmit and Prevent transmitting on a different band
    // (addendum G-42: refused while anyone is transmitting).
    if ((key == QLatin1String("BandPlanRegion") || isTransmitGateSettingKey(key))
        && m_radioModel) {
        m_radioModel->stationOnAirRefusal(&reason);
        return reason;
    }
    if (m_radioModel.isNull() || !isTransmitSettingKeyAcceptedOffAir(key)) {
        return reason;
    }
    // R-R3-46 / R-R3-49 (parity Task 13): Thetis has no on-air rule for
    // these; the Core follows it.
    if (isTransmitSettingKeyTakenOnAir(key)) {
        return reason;
    }
    // Trunk merge of remote transmit (join c): the on-air rule is the
    // change's, not the holder's: the OC transmit pins wait while the radio
    // is on the air, whoever holds transmit, as Thetis greys them while MOX
    // is on (setup.cs:21944 [v2.10.3.15] UpdateForHotSwitch). Since
    // transmitSettingsVersion 13 a receive-only Core follows the same rule:
    // the other keys on the list are taken on the air, as a local window
    // takes them (the DSP > Options TX and PA applies wait for receive; the
    // SWR protection keys apply at once).
    if (!isOcTransmitPinKey(key.toLower().split(QLatin1Char('/')))) {
        return reason;
    }
    m_radioModel->stationOnAirRefusal(&reason);
    return reason;
}

QString StationServer::paSettingOnAirRefusalFor(SessionTransport* transport,
                                                const QString& key,
                                                const QString* value) const
{
    if (m_radioModel.isNull()) {
        return {};
    }
    const QByteArray requester = peerInfoFor(transport).deviceId;
    const bool holds = !requester.isEmpty() && m_transmitHolder
        && m_transmitHolder->isHeldBy(requester);
    return m_radioModel->paSettingOnAirRefusal(key, value, holds);
}

bool StationServer::isTransmitGateSettingKey(const QString& key)
{
    return key == QLatin1String(RadioModel::kExtendedTransmitKey)
        || key == QLatin1String(RadioModel::kPreventTxOnDifferentBandKey);
}

QString StationServer::transmitGateSettingRefusal(SessionTransport* transport,
                                                  const QString& key,
                                                  const QVariant* value) const
{
    if (!isTransmitGateSettingKey(key)) {
        return {};
    }
    if (m_radioModel.isNull()) {
        return QStringLiteral("This Core has no radio to transmit with.");
    }
    // JJ's ruling (2026-09-28): changing it needs transmit permission, the
    // station transmit gate's (remote transmit allowed, this device paired
    // and ready, nobody else holding transmit).
    const TxDecision decision = txDecisionFor(transport);
    if (!decision.permitted) {
        return decision.refusal.text;
    }
    // ... and it is refused while anyone is transmitting.
    if (const QString onAir = transmitSettingOnAirRefusal(key); !onAir.isEmpty()) {
        return onAir;
    }
    if (value != nullptr && value->toString() != QLatin1String("True")
        && value->toString() != QLatin1String("False")) {
        return key == QLatin1String(RadioModel::kPreventTxOnDifferentBandKey)
            ? QStringLiteral("Prevent transmitting on a different band is either on or off.")
            : QStringLiteral("Extended transmit is either on or off.");
    }
    return {};
}

int StationServer::bandSelectVersion() const
{
    // The desktop's band button on a slice (RadioModel::onBandButtonClicked),
    // for a band the catalogue's `bands` lists.
    return m_radioModel ? 1 : 0;
}

int StationServer::dspInfoVersion() const
{
    // R-R3-49 / R-R3-21 / R-R3-40 (parity Task 16): the DSP facts come from
    // this Core's own build and channels, so a local radio model.
    return m_radioModel && m_radioModel->role() != RadioModel::Role::Remote ? 1 : 0;
}

int StationServer::recordStreamVersion() const
{
    // R-IOS-25 / R-R3-49 (parity Task 19): the spots and the spot sources
    // are the Core's own, so a local radio model. 2 (spot resolved mode,
    // R-IOS-25): each spot record also carries resolvedMode
    // (SpotSourceHost::spotRecordFields); everything 1 brings is unchanged.
    return m_radioModel && m_radioModel->role() != RadioModel::Role::Remote
            && m_radioModel->spotSourceHost() != nullptr
        ? 2
        : 0;
}

int StationServer::stationFreedvVersion() const
{
    // R-IOS-26 / R-R3-49 (iPhone plan Task 22, parity Task 20): the Core
    // runs FreeDV Reporter itself, with the record streams. 2 (R-IOS-26):
    // each freedvStations record also carries the station's band
    // (FreeDVStationModel::recordFields); everything 1 brings is unchanged.
    return recordStreamVersion() >= 1 && m_radioModel->freeDvReporter() != nullptr
            && m_radioModel->freeDvStationModel() != nullptr
        ? 2
        : 0;
}

int StationServer::supportBundleVersion() const
{
    // R-R3-49 / R-IOS-18 (parity Task 22, iPhone plan Task 25): every Core
    // with a radio model makes its bundle and shares its log.
    return m_radioModel.isNull() ? 0 : 1;
}

int StationServer::txModMonitorVersion() const
{
    // R-IOS-13 / R-R3-49: the Core's own analyzers, on the record streams.
    return recordStreamVersion() >= 1 && m_modMonitor != nullptr ? 1 : 0;
}

int StationServer::mediaReplaceVersion() const
{
    // iPhone app plan Task 29 (R-IOS-16): the media `replace` operation,
    // whenever media is on.
    return m_mediaEnabled ? 1 : 0;
}

bool StationServer::mediaReplaceAvailable(quint64 epoch) const
{
    // Advertised in the minor-11 capabilities block only, as
    // txDisplayAvailable() is.
    const auto it = m_peers.constFind(mediaSessionFor(epoch));
    return mediaAvailable(epoch) && it != m_peers.cend()
        && it->agreedMinor >= kRadioIdentitySessionProtocolMinor && mediaReplaceVersion() >= 1;
}

int StationServer::controlSwitchVersion() const
{
    // iPhone app plan Task 29: every Core of this build moves a session.
    return 1;
}

SessionTransport* StationServer::peerKey(SessionTransport* transport) const
{
    if (transport == nullptr) {
        return nullptr;
    }
    if (m_peers.contains(transport)) {
        return transport;
    }
    for (auto it = m_peers.cbegin(); it != m_peers.cend(); ++it) {
        const auto* switchable = qobject_cast<const SwitchableTransport*>(it.key());
        if (switchable != nullptr && switchable->inner() == transport) {
            return it.key();
        }
    }
    return nullptr;
}

bool StationServer::radioIdleForPathChange() const
{
    // Link section 21.2: not while the radio is keyed, nor while MOX's
    // delay timers run on the way into or out of transmit.
    const MoxController* mox = m_radioModel ? m_radioModel->moxController() : nullptr;
    return mox == nullptr || mox->state() == MoxState::Rx;
}

void StationServer::handlePathTicket(SessionTransport* transport, const SessionMessage& message)
{
    const auto it = m_peers.find(transport);
    if (it == m_peers.end()) {
        return;
    }
    // A verb of the minor-11 block, for an admitted session.
    if (it->agreedMinor < kRadioIdentitySessionProtocolMinor || it->sessionDeviceId.isEmpty()
        || controlSwitchVersion() < 1) {
        send(transport, SessionMessages::commandResult(
            message.commandVerb, message.commandId, false,
            QStringLiteral("The Core does not know this request. Updating the Core may help."),
            {}));
        return;
    }
    // Review Minor 12: no arguments, as session.leave takes none.
    if (!message.arguments.isEmpty()) {
        send(transport, SessionMessages::commandResult(
            message.commandVerb, message.commandId, false,
            QStringLiteral("The request to move this connection was not understood."), {}));
        return;
    }
    if (!radioIdleForPathChange()) {
        send(transport, SessionMessages::commandResult(
            message.commandVerb, message.commandId, false,
            // The words of kPathTransmittingReason (SessionMessages.h),
            // which a device matches.
            QStringLiteral("Not while the radio is transmitting."), {}));
        return;
    }
    // A new ticket replaces the session's last. 32 bytes from the
    // operating system's generator (DeviceAuthenticator::newChallenge).
    it->pathTicket = m_deviceAuth->newChallenge();
    it->pathTicketDeadline = QDeadlineTimer(m_pathTicketLifetimeMs);
    send(transport, SessionMessages::commandResult(
        message.commandVerb, message.commandId, true, QString(), {},
        {{0, "ticket", MirrorWireKind::Utf8, StationIdentity::toBase64Url(it->pathTicket)},
         {1, "expiresInMs", MirrorWireKind::Int64, qlonglong(m_pathTicketLifetimeMs)}}));
}

bool StationServer::txWatchAuthorityCurrent(SessionTransport* transport) const
{
    const auto it = m_peers.constFind(transport);
    const auto* switchable = qobject_cast<const SwitchableTransport*>(transport);
    return it != m_peers.cend() && it->authenticated && it->snapshotComplete
        && !it->dropping && !it->txWatchPathChanging
        && !it->signedInWithToken && !it->deviceId.isEmpty()
        && it->sessionDeviceId == it->deviceId && it->sessionId != 0
        && m_devices->find(it->deviceId).has_value()
        && it->agreedMinor >= kRadioIdentitySessionProtocolMinor
        && peerDeclares(transport, QByteArrayLiteral("remoteTx"), 1)
        && peerDeclares(transport, QByteArrayLiteral("txWatchPath"), 1)
        && txDecisionFor(transport).permitted && isListening()
        && (switchable == nullptr || !switchable->switching())
        && m_txWatchServer != nullptr;
}

bool StationServer::txWatchEligible(SessionTransport* transport) const
{
    const auto* switchable = qobject_cast<const SwitchableTransport*>(transport);
    const SessionTransport* carrying = switchable ? switchable->inner() : transport;
    return txWatchAuthorityCurrent(transport) && m_wsServer != nullptr
        && qobject_cast<const WebSocketTransport*>(carrying) != nullptr;
}

bool StationServer::txWatchRelayEligible(SessionTransport* transport) const
{
    const auto* switchable = qobject_cast<const SwitchableTransport*>(transport);
    const auto* carrying = qobject_cast<const DataChannelTransport*>(
        switchable ? switchable->inner() : transport);
    return txWatchAuthorityCurrent(transport)
        && peerDeclares(transport, QByteArrayLiteral("txWatchRelay"), 1)
        && carrying && carrying->canOpenWatchRelay()
        && !certificatePemPath().isEmpty() && !privateKeyPemPath().isEmpty();
}

bool StationServer::txWatchRelayCapability(SessionTransport* transport) const
{
    if (txWatchRelayEligible(transport)) {
        return true;
    }
    // The grant controls NEW admission. A binding issued before expiry still
    // owns a usable route, including the already attached watch; withdrawing
    // the capability here would make the client close that healthy channel.
    const auto peer = m_peers.constFind(transport);
    const auto* switchable = qobject_cast<const SwitchableTransport*>(transport);
    const auto* carrying = qobject_cast<const DataChannelTransport*>(
        switchable ? switchable->inner() : transport);
    return peer != m_peers.cend() && txWatchAuthorityCurrent(transport)
        && peerDeclares(transport, QByteArrayLiteral("txWatchRelay"), 1)
        && carrying && carrying->hasWatchRelayRoute()
        && m_txWatchServer->hasLiveBinding(transport, peer->txWatchGeneration);
}

bool StationServer::txWatchBindingCurrent(SessionTransport* transport, quint64 sessionId,
                                          const QByteArray& deviceId,
                                          quint64 generation) const
{
    const auto it = m_peers.constFind(transport);
    const auto* switchable = qobject_cast<const SwitchableTransport*>(transport);
    const SessionTransport* carrying = switchable ? switchable->inner() : transport;
    const auto* relay = qobject_cast<const DataChannelTransport*>(carrying);
    const bool routeCurrent = qobject_cast<const WebSocketTransport*>(carrying) != nullptr
        || (peerDeclares(transport, QByteArrayLiteral("txWatchRelay"), 1)
            && relay && relay->hasWatchRelayRoute());
    return it != m_peers.cend() && it->sessionId == sessionId
        && it->deviceId == deviceId && it->txWatchGeneration == generation
        && txWatchAuthorityCurrent(transport) && routeCurrent;
}

void StationServer::handleTxWatchTicket(SessionTransport* transport,
                                        const SessionMessage& message)
{
    const auto answer = [this, transport, &message](bool accepted, const QString& reason,
                                                   const QList<MirrorUpdate>& values = {}) {
        send(transport, SessionMessages::commandResult(message.commandVerb, message.commandId,
                                                       accepted, reason, {}, values));
    };
    if (!message.arguments.isEmpty()) {
        answer(false, QStringLiteral("The watch ticket request was not understood."));
        return;
    }
    if (!txWatchEligible(transport)) {
        answer(false, QStringLiteral("The Core cannot provide a separate transmit watch connection for this device."));
        return;
    }
    const Peer& peer = peerFor(transport);
    const auto ticket = m_txWatchServer->issue(transport, peer.sessionId, peer.deviceId,
                                               peer.txWatchGeneration,
                                               m_deviceAuth->newChallenge());
    if (!ticket) {
        answer(false, QStringLiteral("A transmit watch ticket or path is already in use. Try again shortly."));
        return;
    }
    answer(true, {},
           {{0, "ticket", MirrorWireKind::Utf8, StationIdentity::toBase64Url(ticket->value)},
            {1, "expiresInMs", MirrorWireKind::Int64, qlonglong(ticket->expiresInMs)},
            {2, "path", MirrorWireKind::Utf8, ticket->path}});
}

void StationServer::retirePendingRelayWatch(SessionTransport* primary)
{
    // Remove ownership before close: both DTLS and the relay leg can call
    // back synchronously, and an old callback must never retire a new slot.
    const std::shared_ptr<PendingRelayWatch> pending = m_pendingRelayWatches.take(primary);
    if (!pending) {
        return;
    }
    const QPointer<DataChannelTransport> watch = pending->transport;
    pending->transport = nullptr;
    if (watch) {
        watch->closeLink(QStringLiteral("watch relay ended"));
        if (watch) {
            watch->deleteLater();
        }
    }
    if (pending->leg) {
        pending->leg->close();
        pending->leg.reset();
    }
}

void StationServer::handleTxWatchRelay(SessionTransport* transport,
                                       const SessionMessage& message)
{
    const QByteArray verb = message.commandVerb;
    const quint32 commandId = message.commandId;
    const QPointer<SessionTransport> primary(transport);
    const auto answer = [this, primary, verb, commandId](bool accepted, const QString& reason,
                                                         const QList<MirrorUpdate>& values = {}) {
        if (primary) {
            send(primary, SessionMessages::commandResult(verb, commandId, accepted,
                                                         reason, {}, values));
        }
    };
    // Reject the shape and size before reserving a ticket or making an ICE
    // peer. The wire codec has already parsed JSON, but escaped lone UTF-16
    // surrogates must not be silently replaced in a security-bearing SDP.
    if (message.arguments.size() != 1 || message.arguments.first().ordinal != 0
        || message.arguments.first().name != QByteArrayLiteral("offer")
        || message.arguments.first().kind != MirrorWireKind::Utf8
        || message.arguments.first().value.metaType().id() != QMetaType::QString) {
        answer(false, QStringLiteral("The watch relay offer was not understood."));
        return;
    }
    const QString offer = message.arguments.first().value.toString();
    if (offer.isEmpty() || offer.size() > IMediaTransport::kMaxDescriptionBytes) {
        answer(false, QStringLiteral("The watch relay offer was not understood."));
        return;
    }
    const QByteArray offerBytes = offer.toUtf8();
    if (offerBytes.isEmpty() || offerBytes.size() > IMediaTransport::kMaxDescriptionBytes
        || offerBytes.contains('\0') || QString::fromUtf8(offerBytes) != offer) {
        answer(false, QStringLiteral("The watch relay offer was not understood."));
        return;
    }
    if (!txWatchRelayEligible(transport)) {
        answer(false, QStringLiteral("The Core cannot relay the transmit watch connection for this device."));
        return;
    }
    const auto* switchable = qobject_cast<const SwitchableTransport*>(transport);
    const auto* carrying = qobject_cast<const DataChannelTransport*>(
        switchable ? switchable->inner() : transport);
    const auto grant = carrying->watchRelayGrant();
    if (!grant || m_pendingRelayWatches.contains(transport)) {
        answer(false, QStringLiteral("A transmit watch ticket or path is already in use. Try again shortly."));
        return;
    }
    const Peer peer = peerFor(transport);
    const auto ticket = m_txWatchServer->issue(transport, peer.sessionId, peer.deviceId,
                                               peer.txWatchGeneration,
                                               m_deviceAuth->newChallenge());
    if (!ticket) {
        answer(false, QStringLiteral("A transmit watch ticket or path is already in use. Try again shortly."));
        return;
    }
    auto pending = std::make_shared<PendingRelayWatch>();
    pending->ticket = ticket->value;
    pending->sessionId = peer.sessionId;
    pending->deviceId = peer.deviceId;
    pending->generation = peer.txWatchGeneration;
    pending->commandId = commandId;
    pending->commandVerb = verb;
    pending->lifetime.start();
    m_pendingRelayWatches.insert(transport, pending);

    const std::weak_ptr<PendingRelayWatch> weak(pending);
    const QPointer<StationServer> self(this);
    const auto isCurrent = [self, primary, weak]() {
        const auto held = weak.lock();
        return self && primary && held
            && self->m_pendingRelayWatches.value(primary.data()) == held;
    };
    const auto fail = [self, primary, weak, isCurrent](const QString& reason) {
        if (!isCurrent()) {
            return;
        }
        const auto held = weak.lock();
        const bool mayAnswer = !held->answered
            && self->txWatchBindingCurrent(primary, held->sessionId, held->deviceId,
                                           held->generation);
        self->retirePendingRelayWatch(primary);
        if (!self) {
            return;
        }
        self->m_txWatchServer->retire(primary);
        if (mayAnswer && self && primary
            && self->txWatchBindingCurrent(primary, held->sessionId, held->deviceId,
                                           held->generation)) {
            self->send(primary, SessionMessages::commandResult(
                held->commandVerb, held->commandId, false, reason, {}));
        }
    };
    pending->leg = RelayLeg::createWatch();
    if (!pending->leg) {
        fail(QStringLiteral("The watch relay could not start."));
        return;
    }
    IceConfiguration ice = IceConfiguration::throughRendezvous({}, true, {}, {});
    ice.setRelay(std::nullopt, 1);
    ice.setCandidateSourceFactory(RelayLeg::factoryFor(pending->leg), true);
    auto* watch = new DataChannelTransport(this);
    pending->transport = watch;
    connect(watch, &DataChannelTransport::localDescription, this,
            [self, primary, weak, isCurrent, fail](const QString& sdp, const QString& type) {
        if (!isCurrent()) {
            return;
        }
        const auto held = weak.lock();
        if (held->answered || type != QLatin1String("answer") || sdp.isEmpty()
            || sdp.toUtf8().size() > IMediaTransport::kMaxDescriptionBytes
            || held->lifetime.elapsed() >= TxWatchServer::kTicketLifetimeMs
            || !self->txWatchBindingCurrent(primary, held->sessionId, held->deviceId,
                                            held->generation)) {
            fail(QStringLiteral("The watch relay could not answer in time."));
            return;
        }
        held->answered = true;
        self->send(primary, SessionMessages::commandResult(
            held->commandVerb, held->commandId, true, {}, {},
            {{0, "ticket", MirrorWireKind::Utf8, StationIdentity::toBase64Url(held->ticket)},
             {1, "expiresInMs", MirrorWireKind::Int64,
              qlonglong(TxWatchServer::kTicketLifetimeMs)},
             {2, "path", MirrorWireKind::Utf8, QStringLiteral("relay-dtls-v1")},
             {3, "answer", MirrorWireKind::Utf8, sdp}}));
    });
    connect(watch, &DataChannelTransport::opened, this,
            [self, primary, weak, isCurrent, fail]() {
        if (!isCurrent()) {
            return;
        }
        const auto held = weak.lock();
        if (!held->answered || held->lifetime.elapsed() >= TxWatchServer::kTicketLifetimeMs
            || !self->txWatchBindingCurrent(primary, held->sessionId, held->deviceId,
                                            held->generation)) {
            fail(QStringLiteral("The watch relay could not open in time."));
            return;
        }
        const QPointer<DataChannelTransport> watch = held->transport;
        self->m_pendingRelayWatches.remove(primary);
        held->transport = nullptr;
        held->leg.reset(); // the peer's candidate-source lease retains its leg
        if (self && primary && watch
            && self->txWatchBindingCurrent(primary, held->sessionId, held->deviceId,
                                           held->generation)) {
            self->m_txWatchServer->acceptTransport(watch, QStringLiteral("relay-watch"));
        } else if (watch) {
            if (self && primary) {
                const auto current = self->m_peers.constFind(primary);
                if (current != self->m_peers.cend()
                    && current->txWatchGeneration == held->generation) {
                    self->m_txWatchServer->retire(primary);
                }
            }
            watch->closeLink(QStringLiteral("watch relay ended"));
            if (watch) {
                watch->deleteLater();
            }
        }
    });
    connect(watch, &SessionTransport::closed, this,
            [fail]() { fail(QStringLiteral("The watch relay closed before opening.")); });
    QTimer::singleShot(TxWatchServer::kTicketLifetimeMs, this, [fail]() {
        fail(QStringLiteral("The watch relay could not open in time."));
    });
    DataChannelTransport::Options options;
    options.role = DataChannelTransport::Role::Answerer;
    options.purpose = DataChannelTransport::Purpose::TxWatch;
    options.maxIncomingBytes = DataChannelTransport::kMaxWatchFrameBytes;
    options.certificatePemPath = certificatePemPath();
    options.privateKeyPemPath = privateKeyPemPath();
    options.ice = ice;
    if (!watch->start(options) || !isCurrent() || !pending->transport
        || !watch->acceptDescription(offer, QStringLiteral("offer"))) {
        fail(QStringLiteral("The watch relay offer was not accepted."));
        return;
    }
    if (isCurrent()) {
        pending->leg->open(grant->url, grant->token);
    }
}

void StationServer::handlePathJoin(SessionTransport* transport, const SessionMessage& message)
{
    auto joining = m_peers.find(transport);
    if (joining == m_peers.end()) {
        return;
    }
    const QString refusal = QStringLiteral("The Core did not move the connection here.");
    const auto refuse = [this, transport, &refusal](const char* why) {
        // Never the ticket: only why, for the log.
        qCInfo(lcStation) << "Refused a connection joining a session:" << why;
        dropPeer(transport, refusal, true, /*retryable=*/false,
                 QString::fromLatin1(SessionEndCode::kProtocolError));
    };
    if (joining->authenticated || !joining->helloReceived || joining->agreedMajor == 0
        || joining->mailboxPairing || joining->pairing) {
        refuse("out of turn");
        return;
    }
    bool ticketOk = false;
    const QByteArray ticket = StationIdentity::fromBase64Url(message.pathTicket, &ticketOk);
    // The session the ticket names, compared in constant time across every
    // live ticket, and consumed whether the join is let in or not.
    SessionTransport* sessionKey = nullptr;
    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        if (it->pathTicket.isEmpty()) {
            continue;
        }
        const bool expired = it->pathTicketDeadline.hasExpired();
        bool same = ticketOk && ticket.size() == it->pathTicket.size();
        if (same) {
            // Constant time over the ticket's bytes, as TokenStore compares
            // the token.
            quint8 difference = 0;
            for (qsizetype i = 0; i < ticket.size(); ++i) {
                difference |= static_cast<quint8>(ticket.at(i) ^ it->pathTicket.at(i));
            }
            same = difference == 0;
        }
        if (expired) {
            it->pathTicket.clear();
        }
        if (same && !expired) {
            sessionKey = it.key();
            it->pathTicket.clear();
        }
    }
    joining = m_peers.find(transport);
    auto session = m_peers.find(sessionKey);
    if (sessionKey == nullptr || session == m_peers.end() || sessionKey == transport) {
        refuse("no live ticket");
        return;
    }
    if (!session->authenticated || session->sessionDeviceId.isEmpty()
        || !session->snapshotComplete) {
        refuse("that connection is not signed in");
        return;
    }
    if (joining->agreedMajor != session->agreedMajor
        || joining->agreedMinor != session->agreedMinor) {
        refuse("another link version");
        return;
    }
    // Link section 21.2: a connection through the service joins only a
    // session signed in with a paired device's own key, and only the
    // introduced device's.
    // Review Minor 15: an introduced connection with no device id named
    // (never in production: the rendezvous verified one) joins nothing.
    if (joining->introduced
        && (session->deviceId.isEmpty() || session->signedInWithToken
            || joining->introducedDeviceId.isEmpty()
            || joining->introducedDeviceId != session->deviceId)) {
        refuse("not the introduced device's connection");
        return;
    }
    if (!radioIdleForPathChange()) {
        refuse("the radio is transmitting");
        return;
    }
    auto* joiningSwitchable = qobject_cast<SwitchableTransport*>(transport);
    auto* sessionSwitchable = qobject_cast<SwitchableTransport*>(sessionKey);
    if (joiningSwitchable == nullptr || sessionSwitchable == nullptr
        || sessionSwitchable->switching()) {
        refuse("that connection cannot move now");
        return;
    }
    // Remember the service's ICE settings before the connection that
    // carries them goes (link section 21.3).
    if (const auto* channel = qobject_cast<const DataChannelTransport*>(sessionSwitchable->inner())) {
        session->serviceIce = channel->iceConfiguration();
    }
    // Invalidate before a callback can issue against the old connection,
    // including when the path move later fails.
    session->txWatchPathChanging = true;
    session->txWatchGeneration = ++m_nextTxWatchGeneration;
    const QPointer<StationServer> self(this);
    const QPointer<SessionTransport> joiningGuard(transport);
    const QPointer<SessionTransport> sessionGuard(sessionKey);
    const QPointer<SwitchableTransport> joiningSwitchableGuard(joiningSwitchable);
    const QPointer<SwitchableTransport> sessionSwitchableGuard(sessionSwitchable);
    auto clearPathChanging = qScopeGuard([this, self, sessionKey]() {
        if (!self) { return; }
        auto current = m_peers.find(sessionKey);
        if (current != m_peers.end()) {
            current->txWatchPathChanging = false;
        }
        publishTxPermitted();
    });
    retirePendingRelayWatch(sessionKey);
    m_txWatchServer->retire(sessionKey);
    if (!self || !joiningGuard || !sessionGuard || !joiningSwitchableGuard
        || !sessionSwitchableGuard) {
        return;
    }
    joining = m_peers.find(transport);
    session = m_peers.find(sessionKey);
    if (joining == m_peers.end() || session == m_peers.end()
        || joining->dropping || session->dropping || sessionSwitchableGuard->switching()) {
        return;
    }
    // The joining connection stops being a peer of its own: its connect
    // deadline (the wrapper's child) goes with the wrapper, and the
    // connection itself becomes the session's.
    SessionTransport* connection = joiningSwitchableGuard->takeInner();
    const QPointer<SessionTransport> connectionGuard(connection);
    if (!self || !joiningGuard || !sessionGuard || !joiningSwitchableGuard
        || !sessionSwitchableGuard) {
        return;
    }
    joining = m_peers.find(transport);
    if (joining == m_peers.end()) {
        if (connectionGuard) {
            connectionGuard->closeLink(refusal);
            if (connectionGuard) {
                connectionGuard->deleteLater();
            }
        }
        return;
    }
    disconnect(joiningSwitchableGuard, nullptr, this, nullptr);
    // Control logging lane: its skipped counts and folded answers.
    m_controlLog.closed(transport, controlLogPeer(*joining));
    m_peers.erase(joining);
    joiningSwitchableGuard->deleteLater();
    const bool started = connectionGuard
        && sessionSwitchableGuard->beginStationSwitch(connectionGuard);
    if (!self) {
        return;
    }
    if (!started) {
        if (connectionGuard) {
            connectionGuard->closeLink(refusal);
            if (connectionGuard) {
                connectionGuard->deleteLater();
            }
        }
        qCWarning(lcStation) << "A session could not move to its new connection";
        return;
    }
    if (!sessionGuard) {
        return;
    }
    session = m_peers.find(sessionKey);
    if (session != m_peers.end()) {
        session->description = sessionKey->peerDescription();
    }
    ++m_sessionsMoved;
    qCInfo(lcStation) << "Moved a session to" << sessionKey->peerDescription();
}

int StationServer::controlChannelVersion() const
{
    // R-IOS-16 (Task 28 fix wave, Important 5): a Core answers an
    // introduction with a control channel whose DTLS certificate its
    // identity key binds (link section 20); without a binding it has none
    // to offer.
    return m_certBinding.isEmpty() ? 0 : 1;
}

int StationServer::stationRadiosVersion() const
{
    // R-IOS-18 / R-R3-49 (parity Task 21): only a Core that chooses its own
    // radio (nereusd) offers it.
    return !m_stationRadios.isNull() && m_radioModel
            && m_radioModel->role() != RadioModel::Role::Remote
        ? 1
        : 0;
}

int StationServer::txDisplayVersion() const
{
    // R-R3-49 / A11 (parity Task 28): the transmit display travels on the
    // media display channel and comes from the Core's own TX analyzer.
    // Parity Task 30 (A12): 2, the Core applies a window's Setup > Display
    // > TX Display analyzer settings to that analyzer at once.
    // Parity Task 31 (A11): 3, a subscribe may carry `duplex` (display
    // duplex): that display keeps the receiver while keyed.
    return m_mediaEnabled && m_radioModel && m_radioModel->txDisplayFeed() != nullptr ? 3 : 0;
}

int StationServer::txMonitorAudioVersion() const
{
    // R-IOS-13 / R-R3-49 (parity Task 32): MON travels in a device's own
    // media audio and comes from the Core's own transmitter.
    return m_mediaEnabled && m_radioModel && m_radioModel->role() != RadioModel::Role::Remote
        ? 1
        : 0;
}

bool StationServer::mediaTunnelAvailable(quint64 epoch) const
{
    // Declare support at media start even while this session is on a data
    // channel. A later path move may use the already agreed direct tunnel;
    // mediaTunnelIceConfiguration checks whether that path carries binary.
    SessionTransport* session = mediaSessionFor(epoch);
    const auto it = m_peers.constFind(session);
    return mediaAvailable(epoch) && it != m_peers.cend()
        && it->agreedMinor >= kRadioIdentitySessionProtocolMinor && mediaTunnelVersion() >= 1
        && session != nullptr;
}

std::optional<IceConfiguration> StationServer::mediaTunnelIceConfiguration(quint64 epoch)
{
    if (!mediaTunnelAvailable(epoch) || !mediaSessionFor(epoch)->carriesBinary()) {
        return std::nullopt;
    }
    SessionTransport* session = mediaSessionFor(epoch);
    auto it = m_peers.find(session);
    if (it == m_peers.end()) {
        return std::nullopt;
    }
    if (!it->mediaTunnel) {
        it->mediaTunnel = MediaTunnel::create(session);
    }
    if (!it->mediaTunnel) {
        return std::nullopt;
    }
    return MediaTunnel::iceFor(it->mediaTunnel, mediaStunServer());
}

void StationServer::setMediaStun(const QStringList& urls, const HostFamilies& families)
{
    // Only STUN: a TURN URL (and anything else) never leaves this Core in
    // mediaStunUrls. No credentials ride a STUN URL.
    QStringList kept;
    for (const QString& url : urls) {
        if ((url.startsWith(QLatin1String("stun:")) || url.startsWith(QLatin1String("stuns:")))
            && !url.contains(QLatin1Char('@')) && !url.contains(QLatin1Char('?'))
            && url.size() <= 512 && !kept.contains(url)) {
            kept.append(url);
        }
    }
    const bool changed = kept != m_mediaStunUrls;
    m_mediaStunUrls = kept;
    m_mediaStunFamilies = families;
    if (!changed) {
        return;
    }
    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        if (!it->authenticated || !it->snapshotComplete) {
            continue;
        }
        const StationCapabilities caps = buildCapabilitiesFor(it.key());
        if (caps.mediaDirectVersion < 1) {
            continue;
        }
        it->txPermittedSent = caps.txPermitted;
        it->txWatchPathVersionSent = caps.txWatchPathVersion;
        it->txRefusalSent = txRefusalOf(caps);
        send(it.key(), SessionMessages::capabilities(caps.toUpdates()));
    }
}

std::optional<IceServerAddress> StationServer::mediaStunServer() const
{
    if (m_mediaStunUrls.isEmpty()) {
        return std::nullopt;
    }
    return IceConfiguration::throughRendezvous(m_mediaStunUrls, /*relayAllowed=*/false,
                                               IceConfiguration::localAddressFamilies(),
                                               m_mediaStunFamilies)
        .stunServer();
}

bool StationServer::mediaDirectAvailable(quint64 epoch) const
{
    SessionTransport* session = mediaSessionFor(epoch);
    return session != nullptr && mediaAvailable(epoch) && mediaDirectVersion() >= 1
        && peerDeclares(session, QByteArrayLiteral("mediaDirect"), 1);
}

IceConfiguration StationServer::mediaDirectIceConfiguration() const
{
    return MediaTunnel::directIceFor(mediaStunServer());
}

bool StationServer::txMonitorAudioAvailable(quint64 epoch) const
{
    // As txDisplayAvailable: told only to a peer that agreed minor 11.
    const auto it = m_peers.constFind(mediaSessionFor(epoch));
    return mediaAvailable(epoch) && it != m_peers.cend()
        && it->agreedMinor >= kRadioIdentitySessionProtocolMinor
        && txMonitorAudioVersion() >= 1;
}

int StationServer::remoteIqVersion() const
{
    return m_mediaEnabled && m_radioModel && m_radioModel->role() != RadioModel::Role::Remote
        ? 1 : 0;
}

bool StationServer::remoteIqAvailable(quint64 epoch) const
{
    const auto it = m_peers.constFind(mediaSessionFor(epoch));
    return mediaAvailable(epoch) && it != m_peers.cend()
        && it->agreedMinor >= kRadioIdentitySessionProtocolMinor
        && remoteIqVersion() >= 1;
}

bool StationServer::txDisplayAvailable(quint64 epoch) const
{
    // Advertised in the minor-11 capabilities block only, so only a peer
    // that agreed minor 11 was told it may declare it.
    const auto it = m_peers.constFind(mediaSessionFor(epoch));
    return mediaAvailable(epoch) && it != m_peers.cend()
        && it->agreedMinor >= kRadioIdentitySessionProtocolMinor
        && txDisplayVersion() >= 1;
}

bool StationServer::miniDisplayAvailable(quint64 epoch) const
{
    SessionTransport* session = mediaSessionFor(epoch);
    const auto it = m_peers.constFind(session);
    return mediaAvailable(epoch) && it != m_peers.cend()
        && it->agreedMinor >= kRadioIdentitySessionProtocolMinor
        && peerDeclares(session, QByteArrayLiteral("miniDisplay"), 1);
}

void StationServer::setStationRadios(StationRadios* radios)
{
    if (!m_stationRadios.isNull()) {
        disconnect(m_stationRadios, nullptr, this, nullptr);
    }
    m_stationRadios = radios;
    m_dispatcher->setStationRadios(radios);
    if (radios == nullptr) {
        return;
    }
    if (m_recordFlushTimer == nullptr) {
        m_recordFlushTimer = new QTimer(this);
        m_recordFlushTimer->setSingleShot(true);
        m_recordFlushTimer->setInterval(kDefaultDeltaFlushMs);
        connect(m_recordFlushTimer, &QTimer::timeout, this, &StationServer::flushRecordStreams);
    }
    const QString name = QStringLiteral("stationRadios");
    if (m_recordStreams.find(name) == m_recordStreams.end()) {
        m_recordStreams.emplace(name, std::make_unique<RecordStream>(name, kStationRadiosCapacity));
    }
    connect(radios, &StationRadios::entriesChanged, this, &StationServer::publishStationRadios);
    publishStationRadios();
}

void StationServer::publishStationRadios()
{
    const auto it = m_recordStreams.find(QStringLiteral("stationRadios"));
    if (it == m_recordStreams.end() || m_stationRadios.isNull()) {
        return;
    }
    RecordStream& stream = *it->second;
    const QList<StationRadioEntry> entries = m_stationRadios->entries();
    QSet<QString> now;
    for (const StationRadioEntry& entry : entries) {
        now.insert(entry.id);
    }
    // What went, then what is (each upsert only when it changed).
    QHash<QString, QJsonObject> held;
    for (const RecordUpsert& record : stream.newest(stream.capacity())) {
        if (!now.contains(record.id)) {
            stream.remove(record.id);
        } else {
            held.insert(record.id, record.fields);
        }
    }
    for (const StationRadioEntry& entry : entries) {
        const QJsonObject fields = entry.toFields();
        if (!held.contains(entry.id) || held.value(entry.id) != fields) {
            stream.upsert(entry.id, fields);
        }
    }
    scheduleRecordFlush();
}

void StationServer::publishStationTciClients()
{
    // Parity Task 23: what went, then what is (each upsert only when it
    // changed), as the Core's radios are published.
    const auto it = m_recordStreams.find(QStringLiteral("tciClients"));
    if (it == m_recordStreams.end() || m_radioModel.isNull()) {
        return;
    }
    RecordStream& stream = *it->second;
    const QList<StationTciClient> clients = m_radioModel->stationTciModel()->clients();
    QSet<QString> now;
    for (const StationTciClient& client : clients) {
        now.insert(client.id);
    }
    QHash<QString, QJsonObject> held;
    for (const RecordUpsert& record : stream.newest(stream.capacity())) {
        if (!now.contains(record.id)) {
            stream.remove(record.id);
        } else {
            held.insert(record.id, record.fields);
        }
    }
    for (const StationTciClient& client : clients) {
        const QJsonObject fields = client.toFields();
        if (!held.contains(client.id) || held.value(client.id) != fields) {
            stream.upsert(client.id, fields);
        }
    }
    scheduleRecordFlush();
}

RecordStream* StationServer::recordStreamForTest(const QString& name) const
{
    const auto it = m_recordStreams.find(name);
    return it == m_recordStreams.end() ? nullptr : it->second.get();
}

int StationServer::meterReadingsVersion() const
{
    // R-R3-13 / R-R3-49 (parity Task 15): the pump that fills the slices'
    // ADC and AGC readings runs on a local radio model only.
    return m_radioModel && m_radioModel->sliceMeterPump() != nullptr ? 1 : 0;
}

int StationServer::transmitSettingsVersion() const
{
    // 1: `transmit` writes outside the keying set and the DspOptions*Tx
    // keys, taken off the air and refused on it (R-R3-49, parity Task 1).
    // 2: the TX and Phone/CW applets' settings on `transmit` (tunePower,
    // the VOX level and delay, MON and its level, LEV, EQ, CFC, PROC and
    // its level, AM carrier, DEXP, mic level), tunePowerForTxBand and
    // tuneDrivePowerSource, and setTunePowerForTxBand (parity Task 2).
    // 3: the radio microphone settings (micBoost, micXlr, micTipRing,
    // micBias, micPttDisabled, lineIn, lineInBoost), the Core's TX profiles
    // (activeTxProfile, txProfilesJson), txProfile.select / save / delete
    // and rade.resetVocoder (parity Task 3).
    // 4: the TX EQ (txEqUseLegacy, txEqPreamp, txEqBandsJson,
    // txEqFreqsJson, txEqNc, txEqMp, txEqCtfmode, txEqWintype,
    // txEqParaEqData), CFC (cfcCompressionJson, cfcEqFreqJson,
    // cfcPostEqBandGainJson, cfcPostEqEnabled, cfcPostEqGainDb,
    // cfcPrecompDb, cfcParaEqData), phase rotator, CESSB, leveler and ALC
    // settings; a band array is refused whole (parity Task 4).
    // 5: Setup > Transmit > Power (tuneDrivePowerSource writable,
    // powerByBandJson, tunePowerByBandJson; ATT on TX, its value and Force
    // ATT on `stepAtt`; the SWR Protection and External TX Inhibit keys
    // taken off the air), DEXP/VOX (the DEXP timings, look-ahead,
    // side-channel filter and antiVoxGainDb) and Test > Two-Tone IMD (the
    // two-tone settings) (parity Task 5).
    // 6: Setup > PA (PA Gain's profiles, per-band gains, adjust matrix and
    // max power; the Watt Meter's PA forward-power table): the
    // hardware/<mac>/pa/... and hardware/<mac>/paCalibration/... keys taken
    // off the air and applied to the Core's PA profiles and calibration at
    // once (parity Task 6).
    // 7: PureSignal arming: ps3.single, ps3.automatic, ps3.applyCurrent and
    // ps3.restoreCorrection taken while the radio is off the air, and a
    // pureSignalSettings write applied to the Core's PureSignal at once
    // instead of only kept, refused while the radio is on the air;
    // ps3.twoTone stays with remote transmit (parity Task 7).
    // 8: Setup > Hardware Config's OC Outputs transmit pins (the HF and SWL
    // TX matrices, Reset OC defaults, Reset SWL OC pins), taken off the air
    // and refused on it; the OC pin actions, TX Display Cal and Volts/Amps
    // Calibration, taken on and off the air as Thetis changes them while
    // transmitting. Each applies to the Core's OC matrix or calibration at
    // once (the OC matrix after the radio is back on receive), and the
    // N2ADR switch on the Core's HL2 applies its whole preset off the air
    // (parity Task 13).
    // 9: General Region writes the validated BandPlanRegion ID (0..23),
    // including TX filter edges, on-air refusal and shared confirmation.
    // 10: the mic mute, `transmit.micMuted` (iPhone app plan Task 40),
    // under the same gates as the mic level; muting sets the Core's mic
    // preamp to 0.0 as Thetis's chkMicMute does.
    // 11: Setup > Transmit > Power's "Disable HF PA" (DisableHfPa), taken on
    // and off the air as Thetis applies it, and applied to the Core's
    // connection (the DisablePA bit) and SWR protection at once.
    // 12: General Options' Extended, the Core's ExtendedTransmit setting
    // ("True"/"False", default off), read by the Core's transmit gate;
    // changed only with transmit permission and off the air (addendum
    // G-42). An older peer's ExtendedTxAllowed stays its own and is
    // ignored.
    // 13: remote parity on the air. A local window changes its transmit
    // settings while transmitting (no TX applet, Phone/CW, TX EQ, CFC,
    // PureSignal, Two-Tone or Setup transmit control is greyed under MOX,
    // as in Thetis), so the Core takes them on the air too, from a peer it
    // takes transmit settings from (takesTransmitSettingsOnAir): every
    // `transmit` property but the keying set, `stepAtt`'s ATT on TX
    // settings, the DSP > Options TX, Power and PA keys (the DSP > Options
    // TX and PA applies wait for receive; the SWR protection keys apply at
    // once, as the local page and Thetis apply them), `pureSignalSettings`,
    // and the commands setTunePowerForTxBand, txProfile.save / delete,
    // rade.resetVocoder, the PureSignal arming verbs and tx.twoTonePreset
    // (the transmitter's own commands still the holder's while transmit is
    // held, ruling 7.7).
    // Still off the air, as locally: the OC transmit pins (Thetis greys
    // them under MOX) and General > Region.
    // 14: General Options' Prevent transmitting on a different band, the
    // Core's PreventTxOnDifferentBandToRx setting ("True"/"False", default
    // off), read by the Core's transmit gate, which refuses a key only
    // when the transmitting slice is not its device's active slice and is
    // on a different band from it (Thetis console.cs:29451-29465
    // [v2.10.3.15] refuses only split TX on another band); changed only
    // with transmit permission and off the air, as Extended.
    // 15: the CFC dialog's band editor, transmit's read-only cfcProfile (to
    // a peer that declared cfcProfile 1), and cfc.setProfile, which applies
    // every band's frequency, compression, post-EQ gain and Q, the range,
    // the pre-compression and the post-EQ gain at once against the
    // revision the app last saw, as the peer's own cfcParaEqData write.
    return m_radioModel.isNull() ? 0 : kTransmitSettingsCfcProfileVersion;
}

bool StationServer::takesTransmitSettingsOnAir(SessionTransport* transport) const
{
    // The permission a transmit setting needs, unchanged by the air: a
    // receive-only Core's peer offered the transmit settings, or a session
    // the station transmit gate permits.
    if (m_radioModel.isNull() || transmitSettingsVersion() < kTransmitSettingsOnAirVersion) {
        return false;
    }
    return m_radioModel->receiveOnlyStationPolicy()
        ? transmitSettingsOffered(peerKey(transport))
        : txDecisionFor(transport).permitted;
}

bool StationServer::pureSignalArmingOffered(SessionTransport* transport) const
{
    // R-R3-49 (parity Task 7): offered transmitSettingsVersion 7.
    return transmitSettingsOffered(peerKey(transport)) && transmitSettingsVersion() >= 7;
}

int StationServer::tgxlControlVersion() const
{
    // 2: the tuner's antenna, operate and bypass (setTgxlAntenna,
    // setTgxlOperate, setTgxlBypass), R-R3-49 / R-R3-47.
    // 3: setTgxlOperate on puts the tuner in OPERATE whole (bypass off and
    // operate on, one command), R-R3-49 fix wave.
    // 4: moveTgxlRelay, scanTgxlLan and setTgxlAddress, R-R3-49 (parity
    // Task 8).
    return accessoryStatusVersion() >= 1 ? 4 : 0;
}

int StationServer::rfKitControlVersion() const
{
    // 3: Reset amp error (resetRfKitError), and the Core applies a window's
    // RF-Kit auto-reconnect and poll interval at once (R-R3-47 fix wave).
    // 4: setRfKitOperate, setRfKitAntenna, setRfKitTciMode and
    // setRfKitAddress, R-R3-49 (parity Task 10).
    return accessoryStatusVersion() >= 1 ? 4 : 0;
}

int StationServer::stationTciVersion() const
{
    if (m_radioModel.isNull() || m_radioModel->stationTciController() == nullptr) {
        return 0;
    }
    // Parity Task 23: 2 with the record streams, which carry its apps
    // (`tciClients`); the options and the disconnect come with it.
    return m_recordStreams.find(QStringLiteral("tciClients")) != m_recordStreams.end() ? 2 : 1;
}

int StationServer::accessoryDataVersion() const
{
    return accessoryStatusVersion() >= 1 && !m_radioModel.isNull()
            && m_radioModel->stationAccessoryData() != nullptr
        // 2: the RF-Kit's connection counts (rfkit*), R-R3-49 (parity
        // Task 10). 3: its average response time (rfkitRttAvgMs), group B
        // fix wave (M7).
        ? 3 : 0;
}

int StationServer::radioHardwareVersion() const
{
    if (m_radioModel.isNull() || !m_radioModel->stepAttFacade()->isBound()) {
        return 0;
    }
    if (!m_radioModel->alexAntennaFacade()->isBound()) {
        return 1;
    }
    // 3: the `ioBoard` object and the per-band antenna verb
    // (setAlexRxAntenna), R-R3-46 fix wave. 4: the filter policy verb
    // (setAlexBpfMode), R-R3-46 / R-R3-21, applied through the same
    // `alexAntennas` facade. 5: `rxOutOnTx` (RX bypass on TX) two-way,
    // group B fix wave. 6: the rest of the transmit half two-way (the TX
    // antenna for each band, Block TX on Ant 2 and 3, Ext 1 and Ext 2 on TX
    // and the RX bypass relay override), parity Task 12, and the per-band
    // TX antenna verb (setAlexTxAntenna), parity mini-round.
    //
    // None of them waits for the radio to leave the air, on this Core or in
    // a local window, because Thetis has no such rule: each Setup handler
    // applies at once, the antenna and Block-TX ones through
    // console.AlexAntCtrlEnabled -> UpdateAlexAntSelection(RX1Band, _mox, ...)
    // with tx = _mox, the relay ones by setting Alex's statics.
    // From Thetis setup.cs:13780-13835 [v2.10.3.15] ProcessAlexAntRadioButton
    //   Alex.getAlex().setTxAnt(band, (byte)ant); ... console.AlexAntCtrlEnabled = true;
    // From Thetis setup.cs:18786-18833 [v2.10.3.15] chkBlockTxAnt2/3_CheckedChanged
    //   console.AlexANT2RXOnly = chkBlockTxAnt2.Checked; // G8NJJ_21h
    // From Thetis setup.cs:16520-16544 [v2.10.3.15] chkEXT1OutOnTx / chkEXT2OutOnTx
    //   Alex.Ext1OutOnTx = chkEXT1OutOnTx.Checked;
    // From Thetis setup.cs:17609-17614 [v2.10.3.15] chkDisableRXOut_CheckedChanged
    //   console.RxOutOverride = chkDisableRXOut.Checked;
    // RadioModel sends the TX routing for an antenna change while keyed and
    // holds the relay flags for the next MOX edge (group B fix wave).
    //
    // 7 (parity Task 14): the HL2 I/O board's I2C tool
    // (requestIoBoardI2c) and output pins (setIoBoardOutput), the board's
    // output pins on `ioBoard` (outputs), and the Alex tab's three transmit
    // high-pass switches taken from a window (isAlexHpfTransmitSwitchKey).
    // An I2C write and an output pin are refused while the radio is on the
    // air (they reach the N2ADR filter board in the transmit path); a read
    // and the three switches are not (Thetis sets the switches with no MOX
    // check).
    //
    // 8: the Alex Filters tabs' receive filter rows (hardware/<mac>/alex/hpf,
    // alex/bpf1 and alex2/hpf: each row's Bypass, Start and End; and
    // alex2/master/bypass55MhzBpf), applied to the Core's radio at once
    // (RadioModel::savedAlexHpfEdges through the "alex" reload), on and off
    // the air, as Thetis's per-row setters re-select the high-pass with no
    // MOX check (console.cs:18823-19040 [v2.10.3.15]).
    // 9 (parity ruling C4): the radio's sample rate from a window
    // (setRadioSampleRate), every receiver and the radio's own rate, as a
    // local window's Radio Info change; for a paired device, off the air.
    //
    // 10: the Alex-1 Filters tab's low-pass rows (hardware/<mac>/alex/lpf/
    // <slug>/{start,end}) and 6m/ByPass on RX (alex/master/lpfBypass),
    // through the "alex" reload (RadioModel::savedAlexLpfEdges). The rows
    // are stored for the next selection, the bypass re-selects at once, on
    // and off the air, as Thetis's handlers do (setup.cs:15888-15994,
    // 18832-18835 [v2.10.3.15]). radio's alexLpfBits (the low-pass in use)
    // goes to a peer that declared alexLpf 1.
    //
    // 11: the HL2 clock options (hardware/<mac>/hl2/cl2Enable, cl2FreqMHz
    // and ext10MHz) reach this Core's radio through the "hl2" reload
    // (RadioModel::applyHl2Options -> P1RadioConnection::setHl2Clock), on
    // and off the air, as mi0bot's handlers write the clock chip with no
    // MOX check (mi0bot setup.cs:21732-21756 [@c26a8a4]). A Core below 11
    // stores them without sending them, so a window keeps the rows closed.
    //
    // 12 (Level Cal): resetLevelCalibration, Setup's Reset (the meter and
    // display calibration back to the radio's defaults), and a window's
    // RX1_MeterCalOffsetDb or RX1_DisplayCalOffsetDb reaches the Core's
    // meter and TCI calibration_ex at once, on and off the air, as
    // Thetis's setters and ResetLevelCalibration have no MOX check
    // (console.cs:21089-21122, 46868-46886 [v2.10.3.15]). It also carries
    // startLevelCalibration and cancelLevelCalibration, the Core's run of
    // Thetis CalibrateLevel (console.cs:9856-10232 [v2.10.3.15]) on a
    // slice, whose progress reaches a peer that declared levelCalibration.
    // The number was extended, not raised: no Core shipped 12 without
    // these verbs.
    //
    // 13 (radio codec lane): the Core sends its radio the receive audio
    // (P1's L/R bytes, P2's port 1028 packets), and HL2 Options' Swap audio
    // channels (hardware/<mac>/hl2/swapAudioChannels) reaches it through
    // the "hl2" reload (RadioModel::applyHl2Options ->
    // P1RadioConnection::setHl2SwapAudioChannels), on and off the air, as
    // mi0bot's chkSwapAudioChannels_CheckedChanged has no MOX check
    // (setup.cs:38065 [@c26a8a4]). A Core below 13 stores it without
    // effect, so a window keeps the box closed.
    return m_radioModel->ioBoardFacade()->isBound() ? 13 : 2;
}

QString StationServer::radioAntennaRowRefusal(SessionTransport* transport,
                                              const SessionMessage& invoke) const
{
    if (invoke.commandVerb != "setAlexRxAntennaForRadio"
        && invoke.commandVerb != "setAlexTxAntennaForRadio") {
        return {};
    }
    const auto peer = m_peers.constFind(peerKey(transport));
    if (peer == m_peers.cend() || !peer->authenticated
        || peer->agreedMinor < kRadioIdentitySessionProtocolMinor
        || peer->features.value(QByteArrayLiteral("radioAntennaRows")) != 1) {
        return QStringLiteral("Update this app to change this radio's antenna row on this Core.");
    }
    if (radioHardwareVersion() < 6) {
        return QStringLiteral("The Core has no antenna settings ready.");
    }
    return SessionCommandDispatcher::radioAntennaRowRefusal(invoke, m_radioModel);
}

StationCapabilities StationServer::buildCapabilities() const
{
    // What the primary media session is told (with none, what a first
    // session would be told), as the one session was before Task 71.
    return buildCapabilitiesFor(primaryMediaSession());
}

StationCapabilities StationServer::buildCapabilitiesFor(SessionTransport* transport) const
{
    StationCapabilities caps;
    caps.settingsSchemaVersion = settingsSchemaVersionOf(m_settings);
    // iPhone app Task 76: every admitted session has media and telemetry
    // (the topology note). Before any session, what a first one is told.
    const auto self = m_peers.constFind(transport);
    if (self != m_peers.cend() && self->authenticated
        && self->agreedMinor >= kRadioIdentitySessionProtocolMinor
        && peerDeclares(transport, QByteArrayLiteral("coreBuildInfo"), 1)) {
        const CoreBuildInfo identity{QCoreApplication::applicationVersion(),
                                     BuildIdentity::buildTag()};
        if (!identity.toJson().isEmpty()) caps.coreBuildInfo = identity;
    }
    const bool admitted = transport == nullptr
        || (self != m_peers.cend() && self->authenticated && self->mediaEpoch != 0);
    const bool media = m_mediaEnabled && admitted;
    const bool telemetry = m_telemetryEnabled && admitted;
    if (m_radioModel.isNull()) {
        return caps;
    }

    const BoardCapabilities& board = m_radioModel->boardCapabilities();

    caps.stationName = m_radioModel->name();
    caps.radioModelName = m_radioModel->model();
    caps.firmwareVersion = m_radioModel->version();
    caps.macAddress = m_radioModel->currentRadioMac();
    caps.board = board.board;
    caps.radioConnected = m_radioModel->isConnected();

    // R-R3-46: which radio, how the Core talks to it, and where it is, for a
    // window that negotiated them; an older one gets today's descriptor.
    // The model is the Core's own profile (an operator's model choice
    // included). With no radio yet the profile has no row and the model is
    // not reported; with no radio ever selected neither is the rest.
    {
        const auto peer = m_peers.constFind(transport);
        if (peer != m_peers.cend()
            && peer->agreedMinor >= kRadioIdentitySessionProtocolMinor) {
            caps.radioIdentityEntries = true;
            // R-R3-46 / R-R3-11: 1 once the Core's controller is behind the
            // `stepAtt` object (DaemonApp binds it before the server starts);
            // 2 once its Alex antennas are behind `alexAntennas` too, with
            // the hardware apply step and the I/O board probe; 3 with the
            // read-only `ioBoard` object and the per-band antenna verb; 4
            // with the filter policy verb; 5 with RX bypass on TX two-way;
            // 6 with the rest of the transmit antennas and relays two-way.
            caps.radioHardwareVersion = radioHardwareVersion();
            // R-R3-47 / R-R3-22: 1 on a Core that owns its accessories: the
            // read-only `amplifier` and `rfkit` objects. The Power Genius is 2
            // there: also configurePgxl, disconnectPgxl and
            // setPgxlConnectionSettings.
            caps.remotePgxlControlVersion = pgxlControlVersion();
            // R-R3-47: the RF-Kit is 2 there too: its interface, antenna,
            // tuner and band-follow rows, and configureRfKit,
            // disconnectRfKit and setRfKitEnabled.
            caps.remoteRfKitControlVersion = rfKitControlVersion();
            // R-R3-48: the Core's own station TCI server.
            caps.stationTciVersion = stationTciVersion();
            // R-R3-47 / R-R3-22: the Core's accessory records and settings.
            caps.accessoryDataVersion = accessoryDataVersion();
            caps.accessoryTxVersion = accessoryTxVersion();
            // A radio-bound row needs the complete existing antenna path,
            // an Alex board, and an exactly named connected radio.
            const QString currentMac = m_radioModel->currentRadioMac();
            // R-IOS-26 / R-R3-49: 2 m as its own band, for a peer that
            // declared band2m (any other is sent none, and sees 2 m as GEN).
            caps.band2mVersion = peerKnows2m(transport) ? 1 : 0;
            // iPhone app plan Task 23 (R-IOS-09): the device's own Opus
            // bitrate, for a peer that declared audioQuality 1.
            caps.audioQualityVersion = media
                && peerDeclares(transport, QByteArrayLiteral("audioQuality"), 1) ? 1 : 0;
            // JJ's ruling of 2026-09-28: the Core's TCI server settings,
            // for a peer that declared stationTciSettings 1.
            caps.stationTciSettingsVersion = peerGetsStationTciSettings(transport) ? 1 : 0;
            caps.radioAntennaRowsVersion = peer->features.value(
                    QByteArrayLiteral("radioAntennaRows")) == 1
                && m_radioModel->role() == RadioModel::Role::Local
                && caps.radioHardwareVersion >= 6
                && board.hasAlexFilters
                && m_radioModel->alexAntennaFacade() != nullptr
                && m_radioModel->alexAntennaFacade()->isBound()
                && !currentMac.isEmpty()
                && AppSettings::normalizedRadioMac(currentMac) == currentMac ? 1 : 0;
            // R-IOS-13 / R-R3-49: transmit's read-only txEqCurve, to a peer
            // that declared txEqCurve 1; 2, with txEq.setCurve and
            // txEq.resetCurve, to one that declared 2.
            caps.txEqCurveVersion = txEqCurveVersionFor(transport);
            // R-R3-46 / R-R3-11: stepAtt's other-ADC attenuator, to a peer
            // that declared adcAttenuators 1.
            caps.adcAttenuatorVersion = peerGetsAdcAttenuators(transport) ? 1 : 0;
            // R-R3-49 / R-IOS-18: the PA Gain profiles, to a peer that
            // declared paProfiles 1.
            caps.paProfileVersion = peerGetsPaProfiles(transport) ? 1 : 0;
            // R-R3-47 / R-R3-22: the Tuner Genius's own settings.
            caps.remoteTgxlControlVersion = tgxlControlVersion();
            // iPhone app Task 12 (R-IOS-08): device sign-in by key, last.
            caps.stationIdentityVersion = m_certBinding.isEmpty() ? 0 : 1;
            // iPhone app Task 13: the devices object and its verbs.
            caps.deviceAdminVersion = deviceAdminVersion();
            // iPhone app Task 14: pairing and the pairing window, last.
            caps.pairingVersion = pairingVersion();
            // iPhone app Task 19: the catalogue.
            caps.stationCatalogVersion = stationCatalogVersion();
            caps.setupDescriptionVersion = peerDeclares(
                transport, QByteArrayLiteral("setupDescription"), 1)
                ? qMin(peer->features.value(QByteArrayLiteral("setupDescription")), 24) : 0;
            // iPhone app Task 20: display extras.
            caps.displayExtrasVersion = media ? displayExtrasVersion() : 0;
            // R-R3-49 (parity Task 1): the transmit settings.
            caps.transmitSettingsVersion = transmitSettingsVersion();
            // R-IOS-27, R-IOS-06: slice.selectBand.
            caps.bandSelectVersion = bandSelectVersion();
            // R-R3-13 / R-R3-49 (parity Task 15): the ADC and AGC readings.
            caps.meterReadingsVersion = meterReadingsVersion();
            // R-R3-49 / R-R3-21 / R-R3-40 (parity Task 16): the DSP facts.
            caps.dspInfoVersion = dspInfoVersion();
            // R-IOS-25 / R-R3-49 (parity Task 19): the record streams.
            caps.recordStreamVersion = recordStreamVersion();
            // R-IOS-18 / R-R3-49 (parity Task 21): the Core's radio choice.
            caps.stationRadiosVersion = stationRadiosVersion();
            // G-38: 2 adds station.repairSettings, for a peer that
            // declared settingsHygiene 2; a peer that declared 1 keeps 1.
            caps.settingsHygieneVersion = m_radioModel->role() != RadioModel::Role::Local ? 0
                : peerDeclares(transport, QByteArrayLiteral("settingsHygiene"), 2) ? 2
                : peerDeclares(transport, QByteArrayLiteral("settingsHygiene"), 1) ? 1 : 0;
            caps.settingsBackupVersion = peerSeesPairingCode(transport)
                && peerDeclares(transport, QByteArrayLiteral("settingsBackup"), 1)
                && m_radioModel->role() == RadioModel::Role::Local ? 1 : 0;
            // R-R3-49 / A11 (parity Task 28): the transmit display, with
            // media, appended after the last entry of the minor-11 block.
            caps.txDisplayVersion = media ? txDisplayVersion() : 0;
            // R-IOS-16 (Task 28 fix wave, Important 5): the control
            // session over a data channel through the remote access
            // service, which needs the bound certificate.
            caps.controlChannelVersion = controlChannelVersion();
            // R-IOS-13 / R-R3-49 (parity Task 32): the transmit monitor to
            // the device that holds transmit, with media, appended after
            // controlChannelVersion.
            caps.txMonitorAudioVersion = media ? txMonitorAudioVersion() : 0;
            caps.remoteIqVersion = media ? remoteIqVersion() : 0;
            // R-IOS-26 / R-R3-49 (iPhone plan Task 22, parity Task 20): the
            // Core's FreeDV Reporter, appended after txMonitorAudioVersion.
            caps.stationFreedvVersion = stationFreedvVersion();
            // iPhone app plan Task 29 (R-IOS-16): moving media and the
            // session, and the relay's permission.
            caps.mediaReplaceVersion = media ? mediaReplaceVersion() : 0;
            caps.controlSwitchVersion = controlSwitchVersion();
            caps.relayAllowed = m_relayAllowed;
            // R-R3-49 / R-IOS-18 (parity Task 22, iPhone plan Task 25): the
            // support bundle, the Core's log and its logging categories,
            // appended after relayAllowed.
            caps.supportBundleVersion = supportBundleVersion();
            // Task 29 step 2b: the media tunnel, to a peer with media.
            caps.mediaTunnelVersion = media ? mediaTunnelVersion() : 0;
            caps.mediaRelayRoutingVersion = media ? 1 : 0;
            // The direct media ladder: the direct-only replace and the
            // Core's STUN servers, only to a peer whose hello declared
            // mediaDirect 1 (before coreBuildInfo on the wire); any other
            // peer's capabilities are today's.
            if (media && peerDeclares(transport, QByteArrayLiteral("mediaDirect"), 1)) {
                caps.mediaDirectVersion = mediaDirectVersion();
                caps.mediaStunUrls = m_mediaStunUrls;
            }
            // Level Cal 2: the catalogue's RX2 input control, only to a
            // peer whose hello declared rx2Attenuator 1 (after the direct
            // media ladder and before coreBuildInfo on the wire); any other
            // peer's capabilities are today's.
            caps.rx2AttenuatorVersion =
                peerDeclares(transport, QByteArrayLiteral("rx2Attenuator"), 1) ? 1 : 0;
            // Radio codec lane: the catalogue's radio mic keys, only to a
            // peer whose hello declared radioMic 1 (after
            // rx2AttenuatorVersion and before coreBuildInfo on the wire).
            caps.radioMicVersion =
                peerDeclares(transport, QByteArrayLiteral("radioMic"), 2)
                    && peerDeclares(transport, QByteArrayLiteral("remoteTx"), 1)
                        ? 2 : peerDeclares(transport, QByteArrayLiteral("radioMic"), 1) ? 1 : 0;
            // Shared-input filters, ruling (d): radio's receive low-pass
            // reason and the slice holding it, only to a peer whose hello
            // declared rxFilterLowPass 1 (after radioMicVersion and before
            // coreBuildInfo on the wire).
            caps.rxFilterLowPassVersion =
                peerDeclares(transport, QByteArrayLiteral("rxFilterLowPass"), 1) ? 1 : 0;
            // RADE reason: each slice's radeReason, for a peer that declared
            // radeReason 1 (after rxFilterLowPassVersion and before
            // coreBuildInfo on the wire), the same test that sends it the
            // property (fitPeerOnlyProperties).
            caps.radeReasonVersion =
                peerGetsFeatureProperties(transport, QByteArrayLiteral("radeReason")) ? 1 : 0;
            // R-IOS-13 / R-R3-49: the AM Mod Monitor's readings, appended
            // after remoteIqVersion by StationCapabilities::toUpdates().
            caps.txModMonitorVersion = txModMonitorVersion();
            // Optional and last: old peers retain their exact descriptor.
            caps.miniDisplayVersion = media && peerDeclares(
                transport, QByteArrayLiteral("miniDisplay"), 1) ? 1 : 0;
            // iPhone app Task 71 (ruling 10.1): several devices at once, for
            // a peer that declared sessionHolder with deviceAuth; any other
            // peer is sent no entry, so its capabilities are today's.
            if (peerHoldsSessions(transport)) {
                caps.sessionHolderEntry = true;
                caps.sessionHolderVersion = sessionHolderVersion();
                // Slice control plan Task 4: sharing slices, for a peer
                // that declared sliceAccess as well; any other peer is sent
                // no entry, so its capabilities are today's.
                if (peerDeclares(transport, QByteArrayLiteral("sliceAccess"), 1)) {
                    caps.sliceAccessEntry = true;
                    // Take-over parity: the lower of the Core's version and
                    // the peer's, so a peer that declared 1 reads 1.
                    // Core-slice take-over: 3 likewise.
                    const int declared =
                        peerDeclares(transport, QByteArrayLiteral("sliceAccess"), 3)   ? 3
                        : peerDeclares(transport, QByteArrayLiteral("sliceAccess"), 2) ? 2
                                                                                       : 1;
                    caps.sliceAccessVersion = std::min(sliceAccessVersion(), declared);
                }
            }
            // iPhone app plan Task 34: remote transmit, last, for a peer whose
            // hello declared remoteTx 1; any other peer is sent no entry.
            // iPhone app plan Task 25: the station computer's VAX, last,
            // for a peer whose hello declared vax 1; any other peer is sent
            // no entry.
            if (peerDeclares(transport, QByteArrayLiteral("vax"), 1)) {
                caps.vaxEntry = true;
                caps.vaxVersion = vaxVersion();
            }
            // Phone wire batch: each slice's diversityPattern, after vax,
            // for a peer that declared diversityPattern 1.
            caps.diversityControlVersion =
                peerGetsFeatureProperties(transport, QByteArrayLiteral("diversityControl")) ? 1 : 0;
            caps.diversityPatternVersion =
                peerGetsFeatureProperties(transport, QByteArrayLiteral("diversityPattern")) ? 1 : 0;
            // Phone wire batch: radio's logCategoryList, for a peer that
            // declared logCategoryList 1.
            caps.logCategoryListVersion =
                peerGetsFeatureProperties(transport, QByteArrayLiteral("logCategoryList")) ? 1 : 0;
            // HL2 port part 2: radio's txInhibitReason, for a peer that
            // declared txInhibitReason 1.
            caps.txInhibitReasonVersion =
                peerGetsFeatureProperties(transport, QByteArrayLiteral("txInhibitReason")) ? 1 : 0;
            // Phone wire batch: stationRadios' modelLabel and models, for a
            // peer that declared radioModels 1, on a Core that keeps the
            // radio list.
            if (peerGetsFeatureProperties(transport, QByteArrayLiteral("radioModels"))) {
                caps.radioModelsEntry = true;
                caps.radioModelsVersion = stationRadiosVersion() >= 1 ? 1 : 0;
            }
            // The phone's direct addresses: devices' coreAddresses, for a
            // device signed in with its own key that declared coreAddresses 1.
            caps.coreAddressesVersion = peerGetsCoreAddresses(transport) ? 1 : 0;
            // RADE status: each slice's radeSynced and radeFreqOffsetHz,
            // for a peer that declared radeStatus 1.
            caps.radeStatusVersion =
                peerGetsFeatureProperties(transport, QByteArrayLiteral("radeStatus")) ? 1 : 0;
            // PA on-air gate re-review, Important C: radio's paTransmitBand,
            // for a peer that declared paTransmitBand 1.
            caps.paTransmitBandVersion =
                peerGetsFeatureProperties(transport, QByteArrayLiteral("paTransmitBand")) ? 1 : 0;
            if (peerDeclares(transport, QByteArrayLiteral("remoteTx"), 1)) {
                caps.remoteTxEntry = true;
                caps.remoteTxVersion = remoteTxVersion();
                caps.txWatchPathVersion = (txWatchEligible(transport)
                                           || txWatchRelayCapability(transport)) ? 1 : 0;
                // iPhone app plan Task 39: the `txState` object, with it.
                caps.txStateVersion = txStateVersion();
                // Parity Task 33: the transmit readings, right after it.
                caps.txReadingsVersion = txReadingsVersion();
            }
            const HardwareProfile& profile = m_radioModel->hardwareProfile();
            caps.hpsdrModel = profile.caps != nullptr ? profile.model : HPSDRModel::FIRST;
            const RadioInfo& radio = m_radioModel->currentRadioInfo();
            if (!radio.macAddress.isEmpty()) {
                caps.radioProtocol = static_cast<int>(radio.protocol);
                caps.radioAddress = radio.address.isNull() ? QString()
                                                           : radio.address.toString();
            }
        }
    }

    caps.boardMaxSlices = board.maxSlices > 0 ? board.maxSlices : 1;
    // Plan Task 11: the stream count for the protocol the Core runs (four
    // on Protocol 1), the same number its own stream pool uses.
    caps.userDdcCount = m_radioModel->userStreamCount();
    caps.pureSignalPresent = board.hasPureSignal;

    // EFFECTIVE, not board (parent section 4.5). R2 has no PerfMonitor to
    // compute a sustainable number, so the effective value is whatever an
    // operator configured, clamped to what the radio can actually do --
    // advertising more slices than the board has would be a worse failure
    // than advertising fewer.
    caps.effectiveMaxSlices = m_sustainableSliceLimit > 0
                                  ? std::min(m_sustainableSliceLimit, caps.boardMaxSlices)
                                  : caps.boardMaxSlices;

    // iPhone app plan Task 34 (R-IOS-02): the station transmit gate's answer
    // for this session. False until its snapshot is complete, for a hello
    // without remoteTx, for every session with remote_transmit deny, and
    // while another device holds transmit; sent again when it changes
    // (publishTxPermitted).
    const TxDecision txDecision = txDecisionFor(transport);
    caps.txPermitted = txDecision.permitted;
    // Desktop remote transmit: why not, in the Core's own words (section
    // 18.3), for a peer that declared remoteTx; empty while permitted.
    if (caps.remoteTxEntry && !txDecision.permitted) {
        caps.txRefusalCode = QString::fromLatin1(txDecision.refusal.code);
        caps.txRefusalReason = txDecision.refusal.text;
        caps.txRefusalFix = QString::fromLatin1(txDecision.refusal.fix);
    }
    caps.remoteMediaVersion = media ? 1 : 0;
    caps.remoteWidebandDisplayVersion = media ? 1 : 0;
    caps.remoteAudioStatusVersion = media ? 1 : 0;
    // Parity Task 17 (R-R3-01): version 2 adds the subscribe's
    // `decimation`, applied to the endpoint's engine.
    caps.spectrumGrantVersion = media ? 2 : 0;
    // R-R3-23: lossless audio beside Opus. Advertised with media whatever
    // nereusd.conf audio_lossless says, so a GUI can be told plainly when
    // the Core's own setting refuses it.
    caps.audioProfileVersion = media ? 1 : 0;
    // R-R3-35: the Core answers audio clock probes whenever media is on.
    caps.audioClockVersion = media ? 1 : 0;
    // R-R3-21 / R-R3-08: display frames and those echoes share one Core
    // clock (DaemonMediaController::displayNowNs), whenever media is on.
    caps.displayClockVersion = media ? 1 : 0;
    // R-R3-43: a receiver's own audio on its own stream, whenever media is on.
    caps.receiverAudioVersion = media ? 1 : 0;
    // R-R3-45: the headphones mix on its own stream, whenever media is on.
    caps.headphonesMixVersion = media ? 1 : 0;
    // iPhone app Task 76 (ruling 9.3): this session's own share of the
    // budget, with its own generation and reason.
    const std::optional<DisplayBudgetLimits> budget = transport == nullptr
        ? displayBudgetTotalFor(nullptr)
        : (self != m_peers.cend() ? self->budgetShare : std::nullopt);
    if (media && m_displayBudgetEnforcementEnabled && budget) {
        caps.remoteDisplayBudgetVersion = 1;
        caps.displayBudget = budget;
        // Subscribed for this session: the PureSignal display goes to its
        // subscriber alone (ruling 9.3 item 4).
        const quint64 epoch = self != m_peers.cend() ? self->mediaEpoch : 0;
        const quint64 subscriber = m_ps3SubscriberEpoch != 0 ? m_ps3SubscriberEpoch
                                                             : mediaSessionEpoch();
        caps.remotePs3DisplaySubscribed =
            m_radioModel->pureSignalFacade()->remoteAmpViewSubscribed()
            && (transport == nullptr || epoch == subscriber);
        // R-R3-08/37: only a peer that negotiated the reason receives it; an
        // older one gets exactly the five budget fields it was built for.
        // Task 76 (design ruling 9.3a): the shared reasons only to a peer
        // that declared sessionHolder; another hears coreBusy or none.
        if (self != m_peers.cend()
            && self->agreedMinor >= kDisplayBudgetReasonSessionProtocolMinor) {
            const DisplayBudgetReason reason = self->budgetShareReason;
            caps.displayBudgetReason = peerHoldsSessions(transport)
                ? reason : displayBudgetReasonForOlderDevice(reason);
        }
    }
    caps.remoteCtunVersion = 1;
    // 4 (R-R3-32, parity Task 6): the radio section also carries the Core's
    // PA readings and radio link quality, for a peer at minor 11.
    // 5 (R-R3-32, parity Task 14): and the Core's HL2 link (hl2*).
    // 6: connected radio age, actual UDP base port and observed ADC status.
    caps.stationTelemetryVersion = telemetry ? 6 : 0;
    caps.remoteTgxlConfigVersion = m_radioModel->stationAccessoryIdentityEnabled() ? 1 : 0;
    caps.remoteFourO3AControlVersion = m_radioModel->stationAccessoryIdentityEnabled() ? 1 : 0;
    caps.propertyResultVersion = 1;
    // R-R3-21 / R-R3-09: the Core owns the notch list (the `notches` object
    // and the notch.* commands). Independent of WDSP: the list lives on
    // NotchModel whether or not channels exist. 2 (R-IOS-27, R-IOS-06):
    // also notch.addAtSlice, the desktop's +TNF on a slice.
    caps.notchControlVersion = 2;
#ifdef HAVE_WDSP
    caps.wdspVersion = 210;
    caps.wdspCompatibilityVersion = 1;
    caps.nnrVersion = 1;
    caps.psAlgorithmVersion = 3;
    // 2 (R-R3-21): NR3 models are Core assets (kind 2, selectNr3Model).
    // 3 (R-R3-49, Sub-epic C-1): DspAssetService also sends dfnrRunnable
    // and dfnrModelStatus, whether this Core can run DFNR.
    // 4 (R-R3-49, Sub-epic C-1): and mnrRunnable and mnrStatus, whether
    // this Core can run MNR (a Mac only).
    caps.dspAssetVersion = 4;
    caps.psDisplayVersion = media ? 1 : 0;
#endif

    return caps;
}


// ── Parity Task 19 (R-IOS-25): record streams ────────────────────────────

void StationServer::setUpRecordStreams()
{
    if (m_radioModel.isNull() || m_radioModel->role() == RadioModel::Role::Remote
        || m_radioModel->spotSourceHost() == nullptr || m_radioModel->spotModel() == nullptr) {
        return;
    }
    m_recordFlushTimer = new QTimer(this);
    m_recordFlushTimer->setSingleShot(true);
    m_recordFlushTimer->setInterval(kDefaultDeltaFlushMs);
    connect(m_recordFlushTimer, &QTimer::timeout, this, &StationServer::flushRecordStreams);

    auto spots = std::make_unique<RecordStream>(QStringLiteral("spots"), kSpotsStreamCapacity);
    RecordStream* spotStream = spots.get();
    m_recordStreams.emplace(spotStream->name(), std::move(spots));
    for (const QString& source : SpotSourceHost::stationSources()) {
        const QString name = SpotSourceHost::consoleStream(source);
        m_recordStreams.emplace(name, std::make_unique<RecordStream>(name, kSpotConsoleCapacity));
    }

    SpotModel* model = m_radioModel->spotModel();
    const QPointer<RadioModel> radio(m_radioModel);
    const auto fields = [radio](const SpotData& spot) {
        return SpotSourceHost::spotRecordFields(
            spot, radio ? radio->dxccColorProvider() : nullptr);
    };
    // The spots the Core already holds are the first backlog.
    for (const SpotData& spot : model->spots()) {
        spotStream->upsert(QString::number(spot.index), fields(spot));
    }
    const auto upsert = [this, spotStream, fields](const SpotData& spot) {
        spotStream->upsert(QString::number(spot.index), fields(spot));
        scheduleRecordFlush();
    };
    connect(model, &SpotModel::spotAdded, this, upsert);
    connect(model, &SpotModel::spotUpdated, this, upsert);
    connect(model, &SpotModel::spotRemoved, this, [this, spotStream](int index) {
        spotStream->remove(QString::number(index));
        scheduleRecordFlush();
    });
    connect(model, &SpotModel::spotsCleared, this, [this, spotStream]() {
        spotStream->reset();
        scheduleRecordFlush();
    });
    // iPhone plan Task 22 / parity Task 20 (stationFreedvVersion 1): the
    // Core's FreeDV Reporter list, each station by its session id, with
    // the Core's distance and heading.
    if (stationFreedvVersion() >= 1) {
        FreeDVStationModel* freedv = m_radioModel->freeDvStationModel();
        auto stations = std::make_unique<RecordStream>(QStringLiteral("freedvStations"),
                                                       kFreedvStationsCapacity);
        RecordStream* freedvStream = stations.get();
        m_recordStreams.emplace(freedvStream->name(), std::move(stations));
        const QHash<QString, FreeDVStation> held = freedv->stations();
        for (auto it = held.cbegin(); it != held.cend(); ++it) {
            freedvStream->upsert(it.key(), FreeDVStationModel::recordFields(
                                               it.value(), freedv->messageChangedAtMs(it.key())));
        }
        const QPointer<FreeDVStationModel> model(freedv);
        const auto upsertStation = [this, freedvStream, model](const QString& sid,
                                                               const FreeDVStation& info) {
            freedvStream->upsert(sid, FreeDVStationModel::recordFields(
                                          info, model ? model->messageChangedAtMs(sid) : 0));
            scheduleRecordFlush();
        };
        connect(freedv, &FreeDVStationModel::stationAdded, this, upsertStation);
        connect(freedv, &FreeDVStationModel::stationUpdated, this, upsertStation);
        connect(freedv, &FreeDVStationModel::stationRemoved, this,
                [this, freedvStream](const QString& sid) {
            freedvStream->remove(sid);
            scheduleRecordFlush();
        });
        connect(freedv, &FreeDVStationModel::cleared, this, [this, freedvStream]() {
            freedvStream->reset();
            scheduleRecordFlush();
        });
    }
    // Parity Task 23 (stationTciVersion 2): the apps on the Core's station
    // TCI server, each by the id the Core gave its connection.
    if (m_radioModel->stationTciController() != nullptr) {
        const QString name = QStringLiteral("tciClients");
        m_recordStreams.emplace(name, std::make_unique<RecordStream>(
                                          name, StationTciModel::kClientsCapacity));
        connect(m_radioModel->stationTciModel(), &StationTciModel::clientsChanged, this,
                &StationServer::publishStationTciClients);
        publishStationTciClients();
    }
    connect(m_radioModel->spotSourceHost(), &SpotSourceHost::consoleLine, this,
            [this](const QString& source, const QString& line) {
        const auto it = m_recordStreams.find(SpotSourceHost::consoleStream(source));
        if (it == m_recordStreams.end()) {
            return; // a window's own listener, never the Core's
        }
        it->second->upsert(QString::number(++m_consoleLineId),
                           QJsonObject{{QStringLiteral("line"), line}});
        scheduleRecordFlush();
    });

    // Parity Task 33 (R-R3-49, R-IOS-13): the CFC bar chart's data, one
    // record (id "0") replaced as WDSP produces it. Read every 50 ms, the
    // interval of Thetis's frmCFCConfig timer:
    // From Thetis frmCFCConfig.cs:443-447 [v2.10.3.15] (setTimer):
    //   _timer = new System.Threading.Timer(timerTick, null, 100, 50);
    // only while a peer subscribes and the radio is keyed with CFC on.
    m_recordStreams.emplace(QString::fromLatin1(TransmitState::kCfcStream),
                            std::make_unique<RecordStream>(
                                QString::fromLatin1(TransmitState::kCfcStream), 1));
    m_cfcPollTimer = new QTimer(this);
    m_cfcPollTimer->setInterval(kCfcDisplayPollMs);
    connect(m_cfcPollTimer, &QTimer::timeout, this, &StationServer::pollCfcCompression);
    connect(m_radioModel, &RadioModel::transmittingChanged, this,
            [this](bool) { updateCfcCompressionPolling(); });
    connect(&m_radioModel->transmitModel(), &TransmitModel::cfcEnabledChanged, this,
            [this](bool) { updateCfcCompressionPolling(); });

    // iPhone app plan Task 25 (vaxVersion 1): the VAX meters, one record
    // (id "0"), read 5 times a second only while a peer subscribes: the
    // phone's VAX tool, as the applet reads them only while it is shown.
    m_recordStreams.emplace(QString::fromLatin1(StationVax::kLevelsStream),
                            std::make_unique<RecordStream>(
                                QString::fromLatin1(StationVax::kLevelsStream), 1));
    m_vaxLevelsTimer = new QTimer(this);
    m_vaxLevelsTimer->setInterval(StationVax::kLevelsIntervalMs);
    connect(m_vaxLevelsTimer, &QTimer::timeout, this, &StationServer::pollVaxLevels);
}

void StationServer::scheduleRecordFlush()
{
    if (m_recordFlushTimer != nullptr && !m_recordFlushTimer->isActive()) {
        m_recordFlushTimer->start();
    }
}

void StationServer::flushRecordStreams()
{
    for (auto& [name, stream] : m_recordStreams) {
        Q_UNUSED(name);
        for (const auto& [subscriber, batch] : stream->takePending()) {
            // The subscriber is the peer's connection, still attached
            // (dropPeer unsubscribes it before it goes).
            auto* transport = static_cast<SessionTransport*>(const_cast<void*>(subscriber));
            if (m_peers.contains(transport)) {
                // Phone wire batch: a declared feature's fields only to a
                // peer that declared it.
                send(transport, SessionMessages::recordBatch(fitRecordBatchToPeer(transport, batch)));
            }
        }
    }
    // R-IOS-13 / R-R3-49: what the Mod Monitor waited to send went; its
    // next window of peaks starts.
    if (m_modMonitor) {
        m_modMonitor->flushed();
    }
}

void StationServer::handleRecordsCommand(SessionTransport* transport, const SessionMessage& message)
{
    const auto answer = [this, transport, &message](bool accepted, const QString& reason) {
        send(transport, SessionMessages::commandResult(message.commandVerb, message.commandId,
                                                       accepted, reason, {}));
    };
    if (peerFor(transport).agreedMinor < kRadioIdentitySessionProtocolMinor) {
        answer(false, QStringLiteral("Update this app to see the Core's spots."));
        return;
    }
    const bool subscribe = message.commandVerb == "records.subscribe";
    const QSet<QByteArray> expected = subscribe ? QSet<QByteArray>{"stream", "backlog"}
                                                : QSet<QByteArray>{"stream"};
    QSet<QByteArray> names;
    QString streamName;
    qint64 backlog = 0;
    bool readable = message.arguments.size() == expected.size();
    for (const MirrorUpdate& a : message.arguments) {
        names.insert(a.name);
        if (a.name == "stream") {
            readable = readable && a.kind == MirrorWireKind::Utf8
                && a.value.typeId() == QMetaType::QString;
            streamName = a.value.toString();
        } else if (a.name == "backlog") {
            readable = readable && a.kind == MirrorWireKind::Int64;
            backlog = a.value.toLongLong();
        }
    }
    if (!readable || names != expected || backlog < 0) {
        answer(false, QStringLiteral("The Core could not read this request."));
        return;
    }
    // Parity Task 22: the Core's log comes with supportBundleVersion; every
    // other stream with recordStreamVersion.
    const bool coreLog = streamName == QLatin1String("coreLog");
    if (coreLog ? supportBundleVersion() < 1 : recordStreamVersion() < 1) {
        answer(false, coreLog
                   ? IStationLink::supportBundleUnavailableReason()
                   : QStringLiteral("This Core does not send its spots or console lines."));
        return;
    }
    const auto it = m_recordStreams.find(streamName);
    if (it == m_recordStreams.end()) {
        answer(false, QStringLiteral("The Core does not keep that list."));
        return;
    }
    if (coreLog) {
        // The newest lines first, then a pull while anyone follows it.
        pullCoreLog();
    }
    if (!subscribe) {
        it->second->unsubscribe(transport);
        answer(true, QString());
        // Parity Task 33: the last viewer gone, the Core stops reading.
        updateCfcCompressionPolling();
        updateVaxLevelsPolling();
        if (m_modMonitor) {
            m_modMonitor->subscriptionsChanged();
        }
        return;
    }
    // The answer, then the backlog as a reset: the peer's copy starts from
    // this batch.
    const int wanted = static_cast<int>(std::min<qint64>(backlog, it->second->capacity()));
    const RecordBatch first = it->second->subscribe(transport, wanted);
    answer(true, QString());
    send(transport, SessionMessages::recordBatch(fitRecordBatchToPeer(transport, first)));
    if (m_modMonitor) {
        m_modMonitor->subscriptionsChanged();
    }
    // Parity Task 33: a viewer of the CFC display may start the reads.
    updateCfcCompressionPolling();
    // iPhone app plan Task 25: and a device with the VAX tool open, the
    // VAX meters.
    updateVaxLevelsPolling();
    if (coreLog && m_coreLogTimer != nullptr && !m_coreLogTimer->isActive()) {
        m_coreLogTimer->start();
    }
}

// ── Parity Task 22 (R-R3-49): the Core's log ─────────────────────────────

void StationServer::setUpCoreLogStream()
{
    if (supportBundleVersion() < 1) {
        return;
    }
    if (m_recordFlushTimer == nullptr) {
        m_recordFlushTimer = new QTimer(this);
        m_recordFlushTimer->setSingleShot(true);
        m_recordFlushTimer->setInterval(kDefaultDeltaFlushMs);
        connect(m_recordFlushTimer, &QTimer::timeout, this, &StationServer::flushRecordStreams);
    }
    const QString name = QStringLiteral("coreLog");
    m_recordStreams.emplace(name, std::make_unique<RecordStream>(name, kCoreLogCapacity));
    // Lines start where the log is now: the backlog is read at the first
    // subscribe.
    m_coreLogTimer = new QTimer(this);
    m_coreLogTimer->setInterval(kCoreLogPullMs);
    connect(m_coreLogTimer, &QTimer::timeout, this, [this]() {
        const auto it = m_recordStreams.find(QStringLiteral("coreLog"));
        if (it == m_recordStreams.end() || it->second->subscriberCount() == 0) {
            m_coreLogTimer->stop();
            return;
        }
        pullCoreLog();
    });
}

void StationServer::pullCoreLog()
{
    const auto it = m_recordStreams.find(QStringLiteral("coreLog"));
    if (it == m_recordStreams.end()) {
        return;
    }
    // The sink's newest lines (already free of addresses), each once, with
    // the same secret removal the support bundle applies.
    const QList<LogSinkLine> lines = LogSink::instance().linesSince(m_coreLogSequence);
    if (lines.isEmpty()) {
        return;
    }
    const int from = std::max(0, static_cast<int>(lines.size()) - kCoreLogCapacity);
    QStringList knownSecrets;
    if (m_tokens != nullptr && !m_tokens->token().isEmpty()) {
        knownSecrets.append(m_tokens->token());
    }
    if (m_pairingWindow != nullptr && !m_pairingWindow->currentCode().isEmpty()) {
        knownSecrets.append(m_pairingWindow->currentCode());
    }
    // If a bounded backlog begins inside a PEM block, its first END marker
    // arrives without BEGIN. Withhold the preceding lines too.
    bool inPrivateKey = m_coreLogInPrivateKey;
    if (!inPrivateKey) {
        for (int i = from; i < lines.size(); ++i) {
            const QString& text = lines.at(i).text;
            if (text.contains(QLatin1String("-----BEGIN "))
                && text.contains(QLatin1String("PRIVATE KEY-----"))) { break; }
            if (text.contains(QLatin1String("-----END "))
                && text.contains(QLatin1String("PRIVATE KEY-----"))) {
                inPrivateKey = true;
                break;
            }
        }
    }
    for (int i = from; i < lines.size(); ++i) {
        const QString& text = lines.at(i).text;
        if (text.contains(QLatin1String("-----BEGIN "))
            && text.contains(QLatin1String("PRIVATE KEY-----"))) {
            inPrivateKey = true;
        }
        const QString clean = inPrivateKey
            ? QStringLiteral("[REDACTED PRIVATE KEY]")
            : SupportBundle::sanitizeText(text, knownSecrets);
        if (text.contains(QLatin1String("-----END "))
            && text.contains(QLatin1String("PRIVATE KEY-----"))) {
            inPrivateKey = false;
        }
        it->second->upsert(QString::number(lines.at(i).sequence),
                           QJsonObject{{QStringLiteral("line"), clean}});
    }
    m_coreLogInPrivateKey = inPrivateKey;
    m_coreLogSequence = lines.last().sequence;
    scheduleRecordFlush();
}

int StationServer::txReadingsVersion() const
{
    // Parity Task 33 (R-R3-49, R-R3-32): the raw PA readings come from the
    // Core's own radio model, and the CFC display travels as a record
    // stream, which a Core without record streams does not keep. Version 2
    // adds the Core-scaled PA values from that local radio's raw samples;
    // version 3 (A9) the seven stage readings the container meters show,
    // read from that radio's transmit channel with the other meters.
    return !m_radioModel.isNull() && m_radioModel->role() != RadioModel::Role::Remote
            && m_recordStreams.find(QString::fromLatin1(TransmitState::kCfcStream))
            != m_recordStreams.end()
        ? 3
        : 0;
}

int StationServer::vaxVersion() const
{
    // iPhone app plan Task 25: the station computer's VAX channels, on a
    // Core whose own audio engine publishes VAX devices (nereusd publishes
    // none, R-R3-44), with the record stream that carries the meters.
    if (m_radioModel.isNull() || m_radioModel->role() == RadioModel::Role::Remote
        || m_recordStreams.find(QString::fromLatin1(StationVax::kLevelsStream))
               == m_recordStreams.end()) {
        return 0;
    }
    const AudioEngine* audio = m_radioModel->localAudioDevices();
    return audio != nullptr && audio->vaxOutputsAllowed() ? 1 : 0;
}

bool StationServer::vaxLevelsPollingForTest() const
{
    return m_vaxLevelsTimer != nullptr && m_vaxLevelsTimer->isActive();
}

void StationServer::setVaxLevelReaderForTest(std::function<void(double*, double*)> reader)
{
    m_vaxLevelReader = std::move(reader);
}

void StationServer::updateVaxLevelsPolling()
{
    if (m_vaxLevelsTimer == nullptr) {
        return;
    }
    const auto it = m_recordStreams.find(QString::fromLatin1(StationVax::kLevelsStream));
    const bool wanted = it != m_recordStreams.end() && it->second->subscriberCount() > 0
        && vaxVersion() >= 1;
    if (wanted && !m_vaxLevelsTimer->isActive()) {
        m_lastVaxLevels = {};
        m_vaxLevelsTimer->start();
    } else if (!wanted && m_vaxLevelsTimer->isActive()) {
        m_vaxLevelsTimer->stop();
    }
}

void StationServer::pollVaxLevels()
{
    const auto it = m_recordStreams.find(QString::fromLatin1(StationVax::kLevelsStream));
    if (it == m_recordStreams.end() || m_radioModel.isNull()) {
        return;
    }
    // The meters the applet shows (VaxApplet::pollLevels): each channel's
    // receive level and the transmit level, 0 to 1.
    double rx[StationVax::kChannels] = {};
    double tx = 0.0;
    if (m_vaxLevelReader) {
        m_vaxLevelReader(rx, &tx);
    } else if (AudioEngine* audio = m_radioModel->localAudioDevices()) {
        for (int i = 0; i < StationVax::kChannels; ++i) {
            rx[i] = audio->vaxRxLevel(i + 1);
        }
        tx = audio->vaxTxLevel();
    }
    // Rounded to a thousandth, and sent only when a meter moved.
    const auto rounded = [](double level) {
        return std::round(std::clamp(level, 0.0, 1.0) * 1000.0) / 1000.0;
    };
    QJsonObject levels{
        {QStringLiteral("ch1Level"), rounded(rx[0])},
        {QStringLiteral("ch2Level"), rounded(rx[1])},
        {QStringLiteral("ch3Level"), rounded(rx[2])},
        {QStringLiteral("ch4Level"), rounded(rx[3])},
        {QStringLiteral("txLevel"), rounded(tx)},
    };
    if (levels == m_lastVaxLevels) {
        return;
    }
    m_lastVaxLevels = levels;
    levels.insert(QStringLiteral("atMs"), static_cast<double>(m_deviceSessions->now()));
    it->second->upsert(QString::fromLatin1(StationVax::kLevelsRecordId), levels);
    scheduleRecordFlush();
}

bool StationServer::cfcCompressionPollingForTest() const
{
    return m_cfcPollTimer != nullptr && m_cfcPollTimer->isActive();
}

void StationServer::setCfcDisplayReaderForTest(std::function<bool(double*, int)> reader)
{
    m_cfcDisplayReader = std::move(reader);
}

void StationServer::updateCfcCompressionPolling()
{
    if (m_cfcPollTimer == nullptr) {
        return;
    }
    const auto it = m_recordStreams.find(QString::fromLatin1(TransmitState::kCfcStream));
    // Only while a peer looks at the chart, the radio is on the air and CFC
    // runs: the local dialog reads only while it is shown, and the display
    // has data only while CFC compresses a transmission.
    const bool wanted = it != m_recordStreams.end() && it->second->subscriberCount() > 0
        && !m_radioModel.isNull() && m_radioModel->isTransmitting()
        && m_radioModel->transmitModel().cfcEnabled();
    if (wanted && !m_cfcPollTimer->isActive()) {
        m_cfcPollTimer->start();
    } else if (!wanted && m_cfcPollTimer->isActive()) {
        m_cfcPollTimer->stop();
    }
}

void StationServer::pollCfcCompression()
{
    const auto it = m_recordStreams.find(QString::fromLatin1(TransmitState::kCfcStream));
    if (it == m_recordStreams.end() || m_radioModel.isNull()) {
        return;
    }
    // From Thetis frmCFCConfig.cs:404-407 [v2.10.3.15] (timerTick):
    //   WDSP.GetTXACFCOMPDisplayCompression(WDSP.id(1, 0), ptrCompValues, &ready);
    //   if (ready == 1)
    // A record goes out only when WDSP says there is new data; the window
    // maps the bins to its chart's range as the local dialog does.
    std::array<double, TxChannel::kCfcDisplayBinCount> bins{};
    bool ready = false;
    if (m_cfcDisplayReader) {
        ready = m_cfcDisplayReader(bins.data(), TxChannel::kCfcDisplayBinCount);
    } else if (TxChannel* tx = m_radioModel->txChannel()) {
        ready = tx->getCfcDisplayCompression(bins.data(), TxChannel::kCfcDisplayBinCount);
    }
    if (!ready) {
        return;
    }
    it->second->upsert(QString::fromLatin1(TransmitState::kCfcRecordId),
                       QJsonObject{
                           {QStringLiteral("atMs"),
                            static_cast<double>(m_deviceSessions->now())},
                           {QStringLiteral("binsDbTenths"),
                            TransmitState::encodeCfcBins(bins.data(),
                                                         TxChannel::kCfcDisplayBinCount)},
                       });
    scheduleRecordFlush();
}

void StationServer::setUpModMonitorStreams()
{
    if (m_radioModel.isNull() || recordStreamVersion() < 1) {
        return;
    }
    if (m_recordFlushTimer == nullptr) {
        m_recordFlushTimer = new QTimer(this);
        m_recordFlushTimer->setSingleShot(true);
        m_recordFlushTimer->setInterval(kDefaultDeltaFlushMs);
        connect(m_recordFlushTimer, &QTimer::timeout, this, &StationServer::flushRecordStreams);
    }
    std::array<RecordStream*, 2> streams{};
    for (int source = 0; source < 2; ++source) {
        const QString name = ModMonitorRecord::streamName(source);
        auto stream = std::make_unique<RecordStream>(name, ModMonitorRecord::kCapacity);
        streams[static_cast<std::size_t>(source)] = stream.get();
        m_recordStreams.emplace(name, std::move(stream));
    }
    // The feedback receiver a window chose, kept on the Core
    // (ModMon/FbStream, rx1 by default, the local applet's default).
    m_radioModel->applyModMonitorSetting(QStringLiteral("ModMon/FbStream"),
                                         m_settings.value(QStringLiteral("ModMon/FbStream")));
    m_modMonitor = std::make_unique<ModMonitorPublisher>(
        m_radioModel.data(), streams[0], streams[1], [this]() { scheduleRecordFlush(); });
}

void StationServer::handleModMonitorReset(SessionTransport* transport,
                                          const SessionMessage& message)
{
    const auto answer = [this, transport, &message](bool accepted, const QString& reason) {
        send(transport, SessionMessages::commandResult(message.commandVerb, message.commandId,
                                                       accepted, reason, {}));
    };
    if (peerFor(transport).agreedMinor < kRadioIdentitySessionProtocolMinor) {
        answer(false, QStringLiteral("Update this app to see the Core's modulation monitor."));
        return;
    }
    if (txModMonitorVersion() < 1) {
        answer(false, QStringLiteral("This Core does not send the modulation monitor. "
                                     "Updating the Core may help."));
        return;
    }
    const bool readable = message.arguments.size() == 1
        && message.arguments.first().name == "source"
        && message.arguments.first().kind == MirrorWireKind::Int64;
    const qlonglong source = readable ? message.arguments.first().value.toLongLong() : -1;
    if ((source != 0 && source != 1) || !m_modMonitor->resetSource(static_cast<int>(source))) {
        answer(false, QStringLiteral("The Core could not read this request."));
        return;
    }
    answer(true, QString());
}

} // namespace NereusSDR
