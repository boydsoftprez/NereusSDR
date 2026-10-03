// =================================================================
// src/core/session/StationClient.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 18.
// See StationClient.h for the connect sequence, the two directions of the
// property mirror, the echo guard, and the caller's ordering
// preconditions.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30  J.J. Boyd / KG4VCF  Fix wave GUI-I6: a Tune Power change
//                                    is marked on its way until the Core
//                                    answers it (TransmitModel).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Inbound sibling fix round 4: the
//                                    paired CFC curve cancels the unsent
//                                    scalar edits only when the new curve
//                                    decodes (setCfcParaEqData projects
//                                    nothing otherwise);
//                                    unresolvedDeltaCancelRuleNames()
//                                    checks the table's names against the
//                                    schemas. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Inbound sibling fix round 3: a delta
//                                    cancels the unsent edits its causes
//                                    define, named per cause in
//                                    kDeltaCancelRules (dspMode -> both
//                                    filter edges, per Thetis
//                                    console.cs:34513 [v2.10.3.15];
//                                    lineInBoost -> lineInGain, per
//                                    console.cs:40930-40932 [v2.10.3.15];
//                                    the paired CFC curve and scalars),
//                                    whatever the
//                                    values. Only the dspMode row steps
//                                    the edges back before its setter
//                                    runs. Replaces round 2's value-
//                                    inferred cancel and generic step-
//                                    back. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Inbound sibling fix round 2: a
//                                    cancelled edit falls back to its
//                                    write in flight (whose answer then
//                                    applies), else to the delta's value,
//                                    else to the last value the Core sent
//                                    (m_coreValues); unsent edits step
//                                    back before a delta's setters run, so
//                                    a cancelled edge is never saved as a
//                                    LastFilter; a sibling behind the same
//                                    notifier is cancelled with it. AI-
//                                    assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Inbound sibling fix round 1: a delta
//                                    whose side effect moves an edit the
//                                    window has not sent cancels that edit
//                                    (its hold and its coalesced write)
//                                    and its own value applies, following
//                                    Thetis's per-mode filter edges
//                                    (console.cs:34513, 34766-34768
//                                    [v2.10.3.15]). A sent edit, and a
//                                    Core answer's side effect, still put
//                                    the operator's value back. AI-
//                                    assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  RADE reason: the hello declares
//                                    radeReason 1, and each slice's
//                                    radeReason applies as the Core sent it.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Inbound sibling lane: a pending write
//                                    keeps the operator's value, and an
//                                    inbound apply that moves it as a side
//                                    effect (a Core mode change rewriting
//                                    the filter edges) puts it back
//                                    (restoreOperatorValues), so the flush
//                                    sends the operator's value, not the
//                                    Core's. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Shared-input filters (ruling (d)): the
//                                    hello declares rxFilterLowPass 1, so
//                                    the CH label, WIDE badge and Filter
//                                    Policy dialog show the Core's receive
//                                    low-pass reason. AI-assisted via
//                                    Anthropic Claude Code.
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
//                                    tunnel alone for the fallback.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  The direct media ladder: the hello
//                                    declares mediaDirect 1; media takes
//                                    the Core's STUN (mediaStunServer), and
//                                    mediaDirectIceConfiguration makes a
//                                    direct-only replacement. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  The heartbeat does not count missed
//                                    pongs before the snapshot-complete
//                                    marker. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  The older-Core reason for the 2 m band
//                                    no longer says "yet". AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Parity ruling C4: setRadioSampleRate
//                                    (radioHardwareVersion 9). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-29: The Core's TCI server settings (JJ's ruling of 2026-09-28,
//               stationTciSettingsVersion 1). J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  Task 24: negotiated remote Settings
//                                    Validation refresh and reply lifetime.
//                                    AI-assisted implementation via Codex.
//   2026-08-08  J.J. Boyd / KG4VCF  Remote daemon R2 Task 18: the GUI half
//                                    of the wss session. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-08-08  J.J. Boyd / KG4VCF  Remote daemon R2 Task 19: link loss,
//                                    daemon restart and reconnect. AI-
//                                    assisted transformation via Anthropic
//                                    Claude Code.
//   2026-08-09  J.J. Boyd / KG4VCF  Whole-branch review, Important 4:
//                                    an entry-less settings.value is a
//                                    removal, not an empty string.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-08-09  J.J. Boyd / KG4VCF  Remote daemon R2: the five
//                                    IStationLink verbs, the pending-
//                                    command map that routes a station
//                                    refusal to an operator-facing
//                                    signal, attach/detach against the
//                                    RadioModel, and the two inbound
//                                    paths re-pointed off removeSlice().
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-40: station telemetry carries
//                                    each receiver's processing load only
//                                    for a peer that negotiated minor 11
//                                    and stationTelemetryVersion 3.
//                                    AI-assisted implementation via
//                                    Anthropic Claude Code.
//                                    Later the same day: the runtime NNR
//                                    limit arrives as SliceModel nnrLimit,
//                                    and the operator's retry is sent as
//                                    nnr.tryAgain, both from minor 11.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-21 / R-R3-09: the window's
//                                    NotchModel mirrors a notchControlVersion
//                                    Core's list and sends notch.* requests.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-46 / R-R3-11: the window holds a
//                                    radioHardwareVersion Core's `stepAtt`
//                                    object; its edits pass an edit gate.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-46 / R-R3-21: why the window's
//                                    attenuator edits cannot reach the
//                                    Core, for the window's controls.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-23 - R-R3-46: the `alexAntennas` object at
//                 radioHardwareVersion 2, its edit gate and reason, and the
//                 requestIoBoardProbe verb. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-23 - R-R3-47 / R-R3-22: the Core's read-only `amplifier` and
//                 `rfkit` objects, applied as plain state, and whether they
//                 are live. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-23 - R-R3-46 fix wave (radioHardwareVersion 3): the read-only
//                 `ioBoard` object, one band's antenna at a time
//                 (setAlexRxAntenna), and the window's OC pin matrix copy
//                 reloaded when the Core's OC settings arrive. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-47 / R-R3-48: remoteRfKitControlVersion 2 (the
//                 configureRfKit, disconnectRfKit and setRfKitEnabled
//                 requests; rfKitEnabled applied as plain state), and
//                 stationTciVersion 1 (the `stationTci` object and the
//                 setStationTci request). J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-24 - R-R3-47 / R-R3-22: remotePgxlControlVersion 2, the
//                 configurePgxl, disconnectPgxl and setPgxlConnectionSettings
//                 requests. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-24 - R-R3-47 / R-R3-22: accessoryDataVersion 1 (the
//                 `accessoryData` object and the setTxInterlockPolicy,
//                 setPgxlPowerCap and clearAccessoryFaults requests). J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - iPhone app Task 4 (R-IOS-01): the highest link major
//                 shared with the station's hello, or a plain-words
//                 departure without retrying; the station's declared
//                 features. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-24 - R-R3-47 / R-R3-22: remotePgxlControlVersion 3 and
//                 remoteTgxlControlVersion 1 (the `accessorySettings` object
//                 and the amp's and tuner's own settings requests); their
//                 refusals go to the Advanced pages, not the slice toast.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-47 / R-R3-22 / R-R3-48: every accessory request's
//                 refusal (Power Genius, Tuner Genius, RF-Kit, interlock,
//                 fault history, station TCI, 4O3A switch) goes to
//                 accessoryRequestRefused, never the slice toast. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-47: remoteRfKitControlVersion 3 (the resetRfKitError
//                 request, the RF-Kit page's settings from a remote window);
//                 a Core setting's change is reported to the window's pages
//                 (stationSettingChanged); whether the Core keeps a TCI
//                 switch is read from this link's settings snapshot only.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-46 / R-R3-21: radioHardwareVersion 4, the filter
//                 policy request (setAlexBpfMode) and its plain reason.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - Lane B takes integration (R-IOS-01, R-R3-21): a refusal
//                with no reason says the Core refused it. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R3 completion carry, review I1 (R-R3-21, R-R3-38,
//                R-IOS-01): the stop message reads the Core's takeover and
//                version reasons through SessionEndReasons, and a version
//                this app refuses itself records the same end. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-49 / R-R3-47: remoteTgxlControlVersion 2 (the
//                Tuner Genius's antenna, operate and bypass requests). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-49 fix wave: the radio's `transmitting` applied as
//                plain state. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-24 - R-R3-49 fix wave: tgxlOperateAppliesWhole
//                (remoteTgxlControlVersion 3). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49, Sub-epic C-1: a slice refuses DFNR while the
//                Core's mirrored dfnrRunnable is false. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49, Sub-epic C-1: and MNR while the Core's mirrored
//                mnrRunnable is false, and BNR (in no build). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24: Part C fix wave: the optional device shortName in
//               auth.request, stored with the device. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25 - iPhone app plan, desktop remote transmit (R-IOS-13,
//                R-R3-42): the hello declares remoteTx 1; the transmit
//                verbs go out three times each through RemoteTransmitClient
//                and their answers come back to it. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 37 (R-IOS-13): tx.keepalive once
//                each on the session (RemoteTransmitClient's keepalive when
//                no "tx" data channel is open); its answers are dropped.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 39 (D14, R-IOS-13): the Core's
//               `txState` object (TransmitState, txStateVersion 1), read-only,
//               for the window's transmit meters. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: merge of Tasks 38 and 39: transmitTimeOutAvailable() (a
//               Core sending txStateVersion 1 has the transmit time-out).
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-24 - R-R3-49 (parity Task 1): transmitSettingsAvailable
//                (transmitSettingsVersion), and the radio's `transmitting`
//                cleared when the session ends. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-49 (parity Task 2): requestTunePowerForTxBand
//                (transmitSettingsVersion 2), the Core's tunePowerForTxBand
//                and tuneDrivePowerSource applied as plain state, and a
//                refused Tune Power change shows the Core's value again.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 3): the TX profile requests and
//                requestRadeResetVocoder (transmitSettingsVersion 3); a
//                refused profile request shows the Core's profile again.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-32 / R-R3-46 / R-R3-49 (parity Task 6): the radio's
//                PA readings and link quality kept only from a Core at
//                minor 11 and stationTelemetryVersion 4; the Core's
//                `txInhibited` applied as plain state; the Core's PA
//                profiles and PA table reloaded in the window as their keys
//                arrive. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-25 - R-R3-21, R-IOS-27: the window's NotchModel is told the
//                Core's notchControlVersion, so its +TNF sends
//                notch.addAtSlice to a version 2 Core. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 7): PureSignal arming offered to the
//                window by a Core at transmitSettingsVersion 7 (the
//                facade's canArm, which also follows the Core's on-air
//                state). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 8): moveTgxlRelay, scanTgxlLan and
//                setTgxlAddress (remoteTgxlControlVersion 4); the scan's
//                answer goes to RadioModel::reportStationTgxlLanScan.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 9): setPgxlOperate, scanPgxlLan and
//                setPgxlAddress (remotePgxlControlVersion 4); the scan's
//                answer goes to RadioModel::reportStationPgxlLanScan.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 10): setRfKitOperate,
//                setRfKitAntenna, setRfKitTciMode and setRfKitAddress
//                (remoteRfKitControlVersion 4). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 / R-R3-46 (parity Task 12): the transmit antennas
//                and relays from radioHardwareVersion 6
//                (remoteTransmitAntennasAvailable). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 / R-R3-46 (parity mini-round): one band's TX
//                antenna at a time (setAlexTxAntenna) from
//                radioHardwareVersion 6. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-26 - R-R3-46 / R-R3-32 (parity Task 14): requestIoBoardI2c and
//                setIoBoardOutput (radioHardwareVersion 7) with their
//                answers routed to the model; the HL2 link fields kept only
//                from a Core at stationTelemetryVersion 5. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 - Parity Task 14 follow-up (R-R3-46): a refused I2C request
//                or output pin is shown once, by the tab that asked, not
//                also through the general refusal notice. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-R3-13 / R-R3-49 (parity Task 15): the Core's ADC and
//                AGC slice readings applied as plain state, and
//                meterReadingsVersion read from its capabilities. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
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
//   2026-09-26 - R-R3-49 / R-R3-21 / R-R3-40 (parity Task 16): the Core's
//                noise reduction, DSP Options apply time and minimum notch
//                widths applied as plain state; dsp.filterResponse sent and
//                its answer handed to the model. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-R3-01 (parity Task 17): spectrumDecimationAvailable
//                (spectrumGrantVersion 2). J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-26 - Parity Task 18 (B3.1): the window's band buttons send
//                slice.selectBand for a named slice to a Core at
//                bandSelectVersion 1. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
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
//   2026-09-26: Task 77 fix wave, M2: the holder line for the radio says
//               how to get transmit back. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 78 (R-IOS-02, R-IOS-07, R-IOS-30):
//               sessionHolder 1 on a key sign-in; RemoteDevicesState fed
//               from connectedDevices, devices, markers, confirm.request and
//               notice; tx.take, the questions' answers, Take it back and
//               session.leave; a held change's "Waiting for you to confirm."
//               is not shown as a refusal. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26 - Parity Task 21 (R-IOS-18): the Core's `stationRadios`
//                stream subscribed and applied; the station radio verbs,
//                their refusals shown on This Core. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-26 - Parity Task 19 (R-IOS-25): the Core's `spots` and
//                spotConsole:<source> streams subscribed after the snapshot
//                and applied to the window's model, the `spotSources`
//                object, and the spots.* verbs. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 28 (R-IOS-16):
//               connectThroughService(), its retries through the service,
//               and sessionIceConfiguration(). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27 - iPhone plan Task 22 / parity Task 20 (R-IOS-26): the
//                Core's freedvStations stream and FreeDV Reporter console
//                subscribed with stationFreedvVersion 1, and the freedv.*
//                verbs. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-27: iPhone app plan Task 29 (R-IOS-16): the path race, moving
//               the session to a better path (session.pathTicket,
//               path.join), the attempt record's service path. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 - Parity Task 33 (R-R3-49, R-R3-32): txReadingsAvailable();
//                the Core's txCfcCompression stream while this window shows
//                the CFC bar chart (setCfcCompressionWanted,
//                cfcCompressionReceived); the window's model holds its
//                `txState` copy. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 step 2b (R-IOS-16, R-IOS-08): the
//               web relay's leg (RelayLeg) and its per-connection candidate
//               sources; the computer's own proxy settings (SystemProxy).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27: R-IOS-13 / R-R3-49 (txModMonitorVersion 1): the window's
//               Mod Monitor subscribes to the Core's txAmModulation or
//               txAmModulationFeedback stream while shown, again after each
//               snapshot, and sends txModMonitor.reset. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-28: iPhone app plan Task 78 items 3 and 7 (G-53): session.held
//               read and answered (answerHeld), the takenOver end's name,
//               id and time kept for the stop panel and Take it back. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28: iPhone app plan Task 25: the Core's device verbs for the
//               This Core page (requestDeviceAdmin). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: The hello declares radioModels 1, so Manage Radios offers
//               each radio the Core's own model list. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-29: addendum G-42: transmitSettingsPermitted and
//               transmitPermissionReason. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-29 - R-IOS-13 / R-R3-49: the txEqCurve a window did not
//                declare is not counted as schema skew. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-28: R-R3-46 / R-R3-11: the hello declares adcAttenuators 1;
//               without adcAttenuatorVersion every slice reads the Core's
//               attenuationDb. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-29: R-R3-46 / R-R3-11: adcAttenuatorsAvailable() for the Setup
//               RX2 row. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-29 - The hello declares radeStatus 1 and each slice takes the
//                Core's radeSynced and radeFreqOffsetHz, so the window's VFO
//                flag shows RADE sync and offset. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-29: HL2 port part 2: the hello declares txInhibitReason 1, and
//               the Core's reason applies as observed state.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - R-R3-46 / R-R3-49: the hello declares alexLpf 1 and radio
//                takes the Core's alexLpfBits, so the Alex-1 Filters tab's
//                lamps show the Core's low-pass (radioHardwareVersion 10).
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - transmitSettingsVersion 15: requestCfcProfile
//                (cfc.setProfile). J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-29 - R-R3-49 / R-IOS-27: holdsTransmitHere() answers the
//                IStationLink query, and a holder change reaches the
//                window's model (reportTransmitHolderChanged). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28: slice control and shared listening plan Task 4: the hello
//               declares sliceAccess with sessionHolder; the `access:<id>`
//               objects are held by no model object until Task 5. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28: slice control and shared listening plan Task 5: the
//               `access:<id>` objects go to SliceAccessMirror, which marks
//               a listened slice read-only; remoteSliceAccessAvailable and
//               slice.listen, slice.stopListening, slice.takeControl and
//               slice.release, answered on deviceCommandFinished; a change
//               held back on a listened slice is sliceAccessHeld. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 14b: requestListenLevel sends
//               slice.setListenLevel for a listened flag's "Your volume".
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 11: requestTxSlice sends
//               tx.setTxSlice for the TX applet's transmit-slice letters,
//               answered on deviceCommandFinished. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 17: a test's token bench link
//               declares sliceAccess with sessionHolder when
//               setTokenSliceAccessForTest asks. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-30: take-over parity: the hello declares sliceAccess 2 (Take
//               it back on controlTaken); controlTakeBackAvailable().
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: take-over fix wave (M-3): a controlTaken card stays when
//               its Take it back was refused and may be tried again.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: core-slice take-over: the hello declares sliceAccess 3,
//               and the slice access mirror learns whether the Core's own
//               slice may be taken. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-01: tune-ended lane: a tuneEnded notice ends the Tuner Genius
//               tune this window asked for (RemoteTransmitClient), so TUNE
//               and its keepalives do not stay on. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

// 2026-10-01: Authenticated Core address inventory and reconnect learning.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex. NereusSDR-original.

#include "core/session/NetworkTrouble.h"
#include "core/session/SystemProxy.h"
#include "core/session/StationClient.h"
#include "core/session/RemoteStationOptions.h"
#include "core/session/BandLinkFit.h"
#include "core/session/ModMonitorRecord.h"

#include "core/AppSettings.h"
#include "core/CfcProfile.h"
#include "core/session/SettingsHygieneWire.h"
#include "core/session/SettingsBackupExportWire.h"
#include "core/SettingsHygiene.h"
#include "core/station/StationRadios.h"
#include "core/FaultLog.h"
#include "core/safety/TxRefusal.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/DeviceAuthenticator.h"
#include "core/security/StationIdentity.h"
#include "core/MicProfileManager.h"
#include "core/session/MirrorPolicy.h"
#include "core/session/ObjectRegistry.h"
#include "core/session/DataChannelTransport.h"
#include "core/session/IceConfiguration.h"
#include "core/session/RendezvousDialer.h"
#include "core/session/RendezvousWire.h"
#include "core/session/RelayLeg.h"
#include "core/session/SessionEndReasons.h"
#include "core/session/SessionTransport.h"
#include "core/session/SwitchableTransport.h"
#include "core/session/TxWatchClient.h"
#include "core/session/media/IMediaTransport.h"
#include "core/session/MediaTunnel.h"
#include "core/session/StationVaxFacade.h"
#include "core/session/TransmitStateFacade.h"
#include "core/session/RemoteDevicesState.h"
#include "core/session/SliceAccessMirror.h"
#include "core/settings/SettingsProxy.h"
#include "models/AmplifierModel.h"
#include "models/NotchModel.h"
#include "models/RfKitModel.h"
#include "models/StationTciModel.h"
#include "models/AccessoryDataModel.h"
#include "models/AccessorySettingsModel.h"

#include <QAuthenticator>
#include <QHostAddress>
#include <QNetworkInterface>
#include "models/PanadapterModel.h"
#include "models/PureSignalSettings.h"
#include "core/dsp/DspAssetService.h"
#include "DspCommandValues.h"
#include "PureSignalSessionFacade.h"
#include "core/StepAttenuatorFacade.h"
#include "core/accessories/AlexAntennaFacade.h"
#include "core/IoBoardHl2Facade.h"
#include "core/SpotSourceHost.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include "models/TunerModel.h"

#include <QCryptographicHash>
#include <QHostAddress>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSslCertificate>
#include <QSslError>
#include <QStringList>
#include <QTimer>
#include <QWebSocket>

#include <algorithm>
#include <array>
#include <cmath>

namespace NereusSDR {

namespace {
Q_LOGGING_CATEGORY(lcStationClient, "nereus.stationclient")

constexpr const char* kRadioKey = "radio";
constexpr const char* kTransmitKey = "transmit";
constexpr const char* kTunerKey = "tuner";
constexpr const char* kPanKeyPrefix = "pan:";
constexpr const char* kSliceKeyPrefix = "slice:";

QString peerNameForThisProcess()
{
    return QStringLiteral("NereusSDR GUI");
}

// Named readLocal... rather than localSettingsSchemaVersion() deliberately:
// StationClient has a member accessor of that exact name, which would
// silently win name lookup inside every member function and turn the
// initialisation below into "m_localSettingsSchema = m_localSettingsSchema".
// The compiler caught it as an unused-function warning; the behaviour it
// was hiding was a schema comparison against a permanent zero.
qint32 readLocalSettingsSchemaVersion()
{
    // BY NAME: both ends read the value stored under this literal key in
    // their OWN store. AppSettings::ensureSettingsAtVersion() is what
    // writes it, and it classifies OperatorLocal (SettingsScope.cpp has no
    // rule for it, and the default is OperatorLocal), so it never travels
    // in the settings snapshot and each side really does read its own.
    return static_cast<qint32>(
        AppSettings::instance()
            .value(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("0"))
            .toString()
            .toInt());
}

/// Colon-separated uppercase SHA-256 hex pairs, byte-for-byte the form
/// CertificateStore::fingerprintSha256() prints. Computed here from
/// QSslCertificate::digest() rather than OpenSSL's X509_digest: both hash
/// the same DER encoding, so the strings match, and the client half has no
/// reason to link libcrypto.
QString formatFingerprint(const QByteArray& sha256Digest)
{
    QString out;
    out.reserve(sha256Digest.size() * 3);
    for (int i = 0; i < sha256Digest.size(); ++i) {
        if (i > 0) {
            out.append(QLatin1Char(':'));
        }
        out.append(QString::number(static_cast<quint8>(sha256Digest[i]), 16)
                       .rightJustified(2, QLatin1Char('0'))
                       .toUpper());
    }
    return out;
}

int idFromKey(const QByteArray& objectKey, const char* prefix)
{
    const QByteArray p(prefix);
    if (!objectKey.startsWith(p)) {
        return -1;
    }
    bool ok = false;
    const int id = objectKey.mid(p.size()).toInt(&ok);
    return ok ? id : -1;
}

QByteArray skewKey(const QByteArray& className, const QByteArray& property)
{
    return className + '.' + property;
}

/// RAII for m_applyingInbound. Save/restore rather than hardcoding false
/// on exit, for the same reason StateMirror::ApplyingGuard does: an apply
/// can reach a setter whose side effects lead back into another apply, and
/// an inner call clearing the flag would leak the outer call's remaining
/// notifies straight back to the station as echoes.
class InboundGuard {
public:
    explicit InboundGuard(bool& flag) : m_flag(flag), m_previous(flag) { m_flag = true; }
    ~InboundGuard() { m_flag = m_previous; }
    InboundGuard(const InboundGuard&) = delete;
    InboundGuard& operator=(const InboundGuard&) = delete;

private:
    bool& m_flag;
    bool m_previous;
};

/// R-R3-17. True when `host` is an address literal in a private or
/// link-local range: IPv4 10/8, 172.16/12, 192.168/16, 169.254/16; IPv6
/// fc00::/7, fe80::/10. A host NAME is never classified: resolving it here
/// would make a pure mapping depend on the network it is diagnosing.
bool isLocalNetworkAddress(const QString& host)
{
    QString literal = host.trimmed();
    if (literal.startsWith(QLatin1Char('[')) && literal.endsWith(QLatin1Char(']'))) {
        literal = literal.mid(1, literal.size() - 2);
    }
    const QHostAddress address(literal);
    if (address.isNull()) {
        return false;
    }
    static const QList<QPair<QHostAddress, int>> kLocalSubnets = {
        QHostAddress::parseSubnet(QStringLiteral("10.0.0.0/8")),
        QHostAddress::parseSubnet(QStringLiteral("172.16.0.0/12")),
        QHostAddress::parseSubnet(QStringLiteral("192.168.0.0/16")),
        QHostAddress::parseSubnet(QStringLiteral("169.254.0.0/16")),
        QHostAddress::parseSubnet(QStringLiteral("fc00::/7")),
        QHostAddress::parseSubnet(QStringLiteral("fe80::/10")),
    };
    for (const QPair<QHostAddress, int>& subnet : kLocalSubnets) {
        if (address.isInSubnet(subnet)) {
            return true;
        }
    }
    return false;
}

// R-R3-38: reads a session end the Core marked not retryable into what the
// window offers next. The link carries only a reason and the retryable
// flag (station link section 12.4), so the two ends that need their own
// buttons are told apart by the reason, worded and read in one place
// (SessionEndReasons). OperatorReasonText words the same reasons for
// display. Any other reason still ends the session and is not retried; the
// window then shows it as a plain refusal with the Core's reason.
// iPhone app Task 18 (R-IOS-08, R-IOS-17): the end's code chooses the
// kind (the link document, section 12.4). A Core that sends no code is an
// older one, and its reason's words are read instead (SessionEndReasons).
StationEndReport stationEndReport(const QString& reason, const QString& code)
{
    StationEndReport report;
    report.kind = StationEndReport::Kind::Refused;
    report.reason = reason;
    report.code = code;

    const SessionEndReasons::Parsed parsed = SessionEndReasons::read(code, reason);
    switch (parsed.kind) {
    case SessionEndReasons::Parsed::Kind::TakenOver:
        report.kind = StationEndReport::Kind::TakenOver;
        report.takenOverBy = parsed.otherAppAddress;
        break;
    case SessionEndReasons::Parsed::Kind::VersionRefused:
        report.kind = StationEndReport::Kind::VersionRefused;
        report.coreMajor = parsed.coreMajor;
        report.appMajor = parsed.appMajor;
        break;
    case SessionEndReasons::Parsed::Kind::DeviceRemoved:
        report.kind = StationEndReport::Kind::DeviceRemoved;
        break;
    case SessionEndReasons::Parsed::Kind::PairingRequired:
        report.kind = StationEndReport::Kind::PairingRequired;
        break;
    case SessionEndReasons::Parsed::Kind::IdentityChanged:
        report.kind = StationEndReport::Kind::IdentityChanged;
        break;
    case SessionEndReasons::Parsed::Kind::Other:
        break;
    }
    return report;
}

// iPhone app Task 18: this computer's device block for a sign-in over
// this connection (the link document, section 3.5): its key signs the
// transcript of this connection's challenge, the Core's certificate and
// the Core's key.
SessionDeviceBlock deviceBlockFor(const ClientDeviceIdentity& identity, const QString& name,
                                  const QString& shortName, const QByteArray& challenge,
                                  const QByteArray& certSha256, const QByteArray& stationSpki)
{
    const QByteArray deviceSpki = identity.publicKeySpki();
    return SessionDeviceBlock{
        StationIdentity::toBase64Url(identity.fingerprint()),
        StationIdentity::toBase64Url(deviceSpki),
        name,
        QString::fromLatin1(ClientDeviceIdentity::kKind),
        StationIdentity::toBase64Url(identity.sign(
            DeviceAuthenticator::transcript(challenge, certSha256, stationSpki, deviceSpki))),
        // Part C fix wave: outside the signed transcript, as `name` is.
        shortName,
    };
}

// The Core's key from its hello, when it is a usable one.
QByteArray stationSpkiOf(const SessionMessage& hello)
{
    if (!hello.stationIdentity) {
        return {};
    }
    bool ok = false;
    const QByteArray spki = StationIdentity::fromBase64Url(hello.stationIdentity->publicKey, &ok);
    return ok && StationIdentity::isP256Spki(spki) ? spki : QByteArray();
}

// True when `binding` (the hello's certBinding, base64url) is `spki`'s
// signature over this connection's certificate.
bool bindingHolds(const QByteArray& spki, const QString& binding, const QByteArray& certSha256)
{
    bool ok = false;
    const QByteArray signature = StationIdentity::fromBase64Url(binding, &ok);
    return ok && certSha256.size() == 32 && !spki.isEmpty()
        && StationIdentity::verify(spki, StationIdentity::certBindingMessage(certSha256),
                                   signature);
}

} // namespace

QString StationClient::connectionFailureReason(QAbstractSocket::SocketError error,
                                               const QString& errorText,
                                               const QString& host,
                                               bool macOs)
{
    // Keyed on the socket error enum AND its text. Qt reports EHOSTUNREACH
    // as NetworkError with the text "Host unreachable"; some paths carry
    // the platform's own "No route to host". HostNotFoundError is a name
    // lookup failure, not this case, and "Network unreachable" (also a
    // NetworkError) means this Mac has no route at all, which Local
    // Network privacy does not produce.
    // A proxy that demands a login: NereusSDR has none to give it.
    const QString networkWords = NetworkTrouble::wordsForSocketError(error);
    if (!networkWords.isEmpty()) {
        return networkWords;
    }
    const bool hostUnreachable =
        error == QAbstractSocket::NetworkError
        && (errorText.contains(QLatin1String("Host unreachable"), Qt::CaseInsensitive)
            || errorText.contains(QLatin1String("No route to host"), Qt::CaseInsensitive));
    if (!macOs || !hostUnreachable || !isLocalNetworkAddress(host)) {
        return errorText;
    }
    return QStringLiteral(
               "Can't reach the Core at %1. If this Mac is on the same network as "
               "the Core, macOS may be blocking NereusSDR from your local network: "
               "allow it in System Settings, Privacy & Security, Local Network, "
               "then press Connect.")
        .arg(host);
}

StationClient::StationClient(RadioModel* radioModel, SettingsProxy* settingsProxy,
                             QObject* parent, const QList<quint16>& supportedMajors, SessionPurpose purpose)
    : QObject(parent)
    , m_sessionPurpose(purpose)
    , m_radioModel(radioModel)
    , m_settingsProxy(settingsProxy)
    , m_supportedMajors(supportedMajors.isEmpty() ? LinkVersion::supportedMajors()
                                                  : supportedMajors)
{
    // Oldest first and each once, as the hello sends them.
    std::sort(m_supportedMajors.begin(), m_supportedMajors.end());
    m_supportedMajors.erase(std::unique(m_supportedMajors.begin(), m_supportedMajors.end()),
                            m_supportedMajors.end());
    // iPhone app plan Task 39: the Core's transmit state, unbound (a mirror).
    m_transmitState = new TransmitState(this);
    // Parity Task 33: pages that show the Core's transmit readings (PA
    // Values) find it through the window's model.
    if (!m_radioModel.isNull()) {
        m_radioModel->setStationTransmitState(m_transmitState);
    }
    // iPhone app plan Task 25: the Core computer's VAX, unbound (a mirror).
    m_stationVax = new StationVax(this);
    // iPhone app plan Task 78: who else is on the Core.
    m_remoteDevices = new RemoteDevicesState(this);
    // Slice control plan Task 5: who controls and who listens to each
    // slice; it marks a slice this window only listens to read-only.
    m_sliceAccess = new SliceAccessMirror(m_radioModel.data(), m_remoteDevices, this);
    if (!m_radioModel.isNull()) {
        // A slice request the model held back for a listened slice.
        connect(m_radioModel.data(), &RadioModel::sliceRequestHeldForListener, this,
                [this](int sliceId, const QString& reason) { emit sliceAccessHeld(sliceId, reason); });
    }
    connect(m_transmitState, &TransmitState::holderChanged, this,
            &StationClient::transmitTakeAvailabilityChanged);
    m_localSettingsSchema = readLocalSettingsSchemaVersion();
    if (m_localSettingsSchema == 0) {
        // The observable trace of CoreInit::initialize() not having run
        // yet. See StationClient.h's ordering preconditions: installing
        // the settings backend before the migrations means this machine's
        // own migrated keys get pushed up to the station.
        qCWarning(lcStationClient)
            << "SettingsSchemaVersion is unset. StationClient is being constructed"
            << "before CoreInit::initialize() ran its settings migrations, which is"
            << "the ordering that leaks local migrated keys to the station.";
    }

    if (radioModel != nullptr && radioModel->role() != RadioModel::Role::Remote) {
        qCWarning(lcStationClient)
            << "StationClient was given a Role::Local RadioModel. Its capability"
            << "apply and connection-state entry points refuse on a local model,"
            << "so this session will connect and then drive nothing.";
    }

    // The seam RadioModel routes its five slice-mutating entry points
    // through in Role::Remote (RadioModel.h, attachStation). Attached for
    // this object's whole life rather than per session -- see the
    // IStationLink block in this class's header for why that is the state
    // with fewer ways to be wrong. Harmless on a Role::Local model, which
    // never consults the link at all.
    if (radioModel != nullptr) {
        radioModel->attachStation(this);
        radioModel->setStationDevices(m_remoteDevices);
        // R-R3-49 / R-IOS-27: pages that follow who holds transmit (the
        // PA Gain page's on-the-air lock) hear it through the model.
        connect(this, &StationClient::transmitTakeAvailabilityChanged, radioModel,
                &RadioModel::reportTransmitHolderChanged);
    }

    // iPhone app plan, desktop remote transmit (R-IOS-13, R-R3-42): this
    // window transmits through the Core (link section 18.6), so its hello
    // says so and the Core answers with txPermitted and remoteTxVersion.
    m_declaredFeatures.insert(QByteArrayLiteral("remoteTx"), 1);
    // G-38: 2 adds Repair invalid settings (station.repairSettings).
    m_declaredFeatures.insert(QByteArrayLiteral("settingsHygiene"), 2);
    m_declaredFeatures.insert(QByteArrayLiteral("coreBuildInfo"), 1);
    m_declaredFeatures.insert(QByteArrayLiteral("coreAddresses"), 1);
    m_declaredFeatures.insert(QByteArrayLiteral("settingsBackup"), 1);
    m_declaredFeatures.insert(QByteArrayLiteral("radioAntennaRows"), 1);
    // R-IOS-26 / R-R3-49: this window knows 2 m as its own band (Band 27).
    m_declaredFeatures.insert(QByteArray(BandLinkFit::kFeature), 1);
    // Manage Radios offers the models the Core accepts for each radio's
    // board, from the Core's own list (stationRadios' models, radioModels 1),
    // never a guess from the model alone.
    m_declaredFeatures.insert(QByteArrayLiteral("radioModels"), 1);
    // JJ's ruling of 2026-09-28: this window shows and changes the rest of
    // the Core's TCI server settings (stationTciSettingsVersion 1).
    m_declaredFeatures.insert(QByteArrayLiteral("stationTciSettings"), 1);
    // R-R3-46 / R-R3-11: this window shows and sets the other ADC's own
    // attenuator for the slices on it (stepAtt rx2AttenuationDb,
    // rx2SliceMask; adcAttenuatorVersion 1).
    m_declaredFeatures.insert(QByteArrayLiteral("adcAttenuators"), 1);
    // The VFO flag's RADE row shows the Core's decoder sync and frequency
    // offset (radeStatusVersion 1), as a local window's flag does.
    m_declaredFeatures.insert(QByteArrayLiteral("radeStatus"), 1);
    // RADE reason: the VFO flag's RADE row says why the Core's RADE slice
    // has no working decoder (radeReasonVersion 1), as a local window's does.
    m_declaredFeatures.insert(QByteArrayLiteral("radeReason"), 1);
    // HL2 port part 2: this window shows why the Core's transmit is held
    // off (radio's txInhibitReason; txInhibitReasonVersion 1).
    m_declaredFeatures.insert(QByteArrayLiteral("txInhibitReason"), 1);
    // The Alex-1 Filters tab's lamps show the low-pass the Core's radio is
    // using (radio's alexLpfBits, radioHardwareVersion 10).
    m_declaredFeatures.insert(QByteArrayLiteral("alexLpf"), 1);
    // Shared-input filters, ruling (d): the CH label, WIDE badge and Filter
    // Policy dialog say which slice holds the Core's receive low-pass
    // (radio's rxFilter0LowPassReason and rxFilter0LowPassSlice;
    // rxFilterLowPassVersion 1).
    m_declaredFeatures.insert(QByteArrayLiteral("rxFilterLowPass"), 1);
    // PA on-air gate re-review, Important C: the PA pages open and lock
    // the row the Core holds on the air (paTransmitBandVersion 1).
    m_declaredFeatures.insert(QByteArrayLiteral("paTransmitBand"), 1);
    // The direct media ladder: this window takes the Core's STUN list
    // (mediaStunUrls) and asks for a direct-only media replacement
    // (mediaDirectVersion 1).
    m_declaredFeatures.insert(QByteArrayLiteral("mediaDirect"), 1);
    // Level Cal: Setup's calibration shows the Core's run as it goes
    // (radio's levelCal* properties, radioHardwareVersion 12).
    m_declaredFeatures.insert(QByteArrayLiteral("levelCalibration"), 1);
    // iPhone app plan Task 25 (R-IOS-18): the VAX applet's "Station
    // computer" section shows the Core computer's VAX channels (the `vax`
    // object); this window's own VAX channels stay its own (R-R3-44).
    m_declaredFeatures.insert(QByteArrayLiteral("vax"), 1);
    m_settingsBackupReplyTimer = new QTimer(this);
    m_settingsBackupReplyTimer->setSingleShot(true);
    connect(m_settingsBackupReplyTimer, &QTimer::timeout, this, [this]() {
        finishSettingsBackupExport(false, QStringLiteral("The Core did not answer the settings export in time."),
                                   {}, true);
    });
    m_settingsBackupOverallTimer = new QTimer(this);
    m_settingsBackupOverallTimer->setSingleShot(true);
    connect(m_settingsBackupOverallTimer, &QTimer::timeout, this, [this]() {
        finishSettingsBackupExport(false, QStringLiteral("The settings export took too long."),
                                   {}, true);
    });
    m_declaredFeatures.insert(QByteArrayLiteral("miniDisplay"), 1);
    // Each transmit verb goes out as the same command three times (the
    // copies rule); the Core acts on the first and answers every copy.
    m_remoteTransmit = new RemoteTransmitClient(
        [this](const QByteArray& verb, const QList<MirrorUpdate>& arguments) -> quint32 {
            if (!remoteTransmitAvailable()) {
                return 0;
            }
            const quint32 id = m_nextCommandId++;
            if (m_nextCommandId == 0) {
                ++m_nextCommandId;
            }
            const SessionMessage command = SessionMessages::commandInvoke(verb, id, arguments);
            for (int copy = 0; copy < RemoteTransmitClient::kCopies; ++copy) {
                send(command);
            }
            return id;
        },
        this);
    // iPhone app plan Task 37 (R-IOS-13): the Core's watchdog hears from
    // this window every 100 ms while it transmits or has VOX armed; on the
    // session a keepalive goes once (a lost one is overtaken by the next).
    m_remoteTransmit->setSessionKeepalive([this](quint64 sequence, quint32 epoch) {
        if (!remoteTransmitAvailable()) {
            return false;
        }
        const quint32 id = m_nextCommandId++;
        if (m_nextCommandId == 0) {
            ++m_nextCommandId;
        }
        send(SessionMessages::commandInvoke(
            QByteArrayLiteral("tx.keepalive"), id,
            {MirrorUpdate{0, QByteArrayLiteral("sequence"), MirrorWireKind::Int64,
                          QVariant(static_cast<qint64>(sequence))},
             MirrorUpdate{0, QByteArrayLiteral("epoch"), MirrorWireKind::Int64,
                          QVariant(static_cast<qint64>(epoch))}}));
        return true;
    });
    m_remoteTransmit->setAuxiliaryKeepalive([this](quint64 sequence, quint32 epoch) {
        TxWatchClient* watch = m_directWatch.data();
        if (!transmitWatchReady() || watch == nullptr
            || watch->generation() != m_directWatchGeneration) {
            return false;
        }
        const QPointer<StationClient> self(this);
        const quint32 sessionEpoch = m_sessionEpoch;
        const bool sent = watch->sendKeepalive(sequence, epoch);
        return self && self->m_sessionEpoch == sessionEpoch
            && self->m_directWatch == watch && self->transmitWatchReady() && sent;
    });
    if (radioModel != nullptr) {
        connect(radioModel, &RadioModel::transmittingChanged, m_remoteTransmit,
                &RemoteTransmitClient::setCoreTransmitting);
        // Task 37: VOX armed (the Core's, mirrored) keeps the keepalive
        // going while the window streams unkeyed.
        // Fix wave M7: a stop the Core records ends this window's key on,
        // even one the mirrored `transmitting` never showed.
        // Fix wave 2: queued, so the whole update (stopSerial and the
        // stopEpoch after it, and keyed) is applied before it is judged.
        connect(
            m_transmitState, &TransmitState::stopChanged, m_remoteTransmit,
            [this]() {
                m_remoteTransmit->coreStopped(
                    m_transmitState->stopSerial(), m_transmitState->keyed(),
                    static_cast<quint32>(m_transmitState->stopEpoch()));
            },
            Qt::QueuedConnection);
        // Fix wave M6: only VOX this window armed (its own write, not the
        // Core's value for another device's).
        connect(&radioModel->transmitModel(), &TransmitModel::voxEnabledChanged, this,
                [this](bool on) {
                    const bool armedHere = on && (m_voxArmedHere || !m_applyingInbound);
                    if (armedHere == m_voxArmedHere) {
                        return;
                    }
                    m_voxArmedHere = armedHere;
                    m_remoteTransmit->setVoxArmed(armedHere);
                    emit voxArmedHereChanged(armedHere);
                });
        // A refused press is shown in the Core's words, where a local
        // refusal shows (MainWindow's toast; the buttons follow the Core).
        const QPointer<RadioModel> model(radioModel);
        connect(m_remoteTransmit, &RemoteTransmitClient::refused, this,
                [model](const QString& reason, const QString&, const QString&) {
                    if (model) { model->reportRemoteTransmitRefused(reason); }
                });
    }

    m_outboundMirror = new StateMirror(this);
    connect(m_outboundMirror, &StateMirror::propertiesChanged, this,
            [this](const QByteArray& objectKey, const QList<MirrorUpdate>& updates) {
                // The echo guard, checked FIRST, before this handler asks
                // anything else about the change -- exactly where
                // StateMirror::onWatchedPropertyChanged checks its own.
                // Before the first snapshot is complete, local changes have
                // no authenticated station state to merge with.  A later
                // schema burst on an established session is different: keep
                // genuine operator edits coalesced while its snapshot owns
                // the wire, then flush them after SnapshotComplete.
                if (m_applyingInbound || (!m_forwardLocalChanges && !m_handshakeComplete)) {
                    return;
                }
                QObject* object = m_objects.value(objectKey).data();
                if (object == nullptr) {
                    return;
                }
                const QByteArray className =
                    MirrorSchema::shortClassName(object->metaObject()->className());
                // Slice control plan Task 5: a slice this window only
                // listens to sends nothing; the Core refuses a listener's
                // every write. Its setters hold a change back before it
                // gets here; what is left (the pan it is shown on) is this
                // window's own.
                if (const auto* slice = qobject_cast<const SliceModel*>(object);
                    slice != nullptr && slice->isReadOnlyListener()) {
                    return;
                }
                for (const MirrorUpdate& update : updates) {
                    // OUTBOUND is MirrorPolicy-gated: this is the exact
                    // direction that table describes, so a property the
                    // station would refuse is dropped here rather than
                    // sent and argued about.
                    if (!MirrorPolicy::inboundAllowed(className, update.name)) {
                        continue;
                    }
                    m_outboundCoalescer.update(objectKey, update);
                    if (propertyResultsAvailable()) {
                        // The operator's value, held until the Core
                        // answers this write (restoreOperatorValues).
                        // A write of it already on its way stays known
                        // (inFlightWriteId), so a delta that cancels
                        // this newer edit can fall back to it.
                        auto& writes = m_pendingWrites[objectKey];
                        PendingWrite next{0, update, m_nextPendingWriteOrder++, 0, MirrorUpdate{}};
                        if (const auto previous = writes.constFind(update.name);
                            previous != writes.cend()) {
                            if (previous->writeId != 0) {
                                next.inFlightWriteId = previous->writeId;
                                next.inFlightValue = previous->value;
                            } else {
                                next.inFlightWriteId = previous->inFlightWriteId;
                                next.inFlightValue = previous->inFlightValue;
                            }
                        }
                        writes.insert(update.name, next);
                    }
                }
            });

    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(m_heartbeatIntervalMs);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &StationClient::onHeartbeatTick);

    m_writeFlushTimer = new QTimer(this);
    m_writeFlushTimer->setInterval(kDefaultWriteFlushMs);
    connect(m_writeFlushTimer, &QTimer::timeout, this, &StationClient::onWriteFlushTick);

    // R-R3-16/17: owned and single-shot like m_reconnectTimer, so an
    // operator Disconnect or a completed handshake can always stop it.
    m_handshakeDeadlineTimer = new QTimer(this);
    m_handshakeDeadlineTimer->setSingleShot(true);
    connect(m_handshakeDeadlineTimer, &QTimer::timeout,
            this, &StationClient::onHandshakeDeadline);

    // Task 19: the automatic-reconnect timer. Owned (parented to this,
    // dies with it), single-shot (armed fresh by scheduleReconnect() for
    // each attempt rather than ticking repeatedly), and stoppable from
    // anywhere that holds `this` -- never static QTimer::singleShot. See
    // the class comment's link-loss section.
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &StationClient::onReconnectTimeout);
    m_directWatchClock.start();
    m_directWatchRetryTimer = new QTimer(this);
    m_directWatchRetryTimer->setSingleShot(true);
    connect(m_directWatchRetryTimer, &QTimer::timeout, this,
            &StationClient::requestWatchAttempt);
    m_directWatchTicketTimer = new QTimer(this);
    m_directWatchTicketTimer->setSingleShot(true);
    connect(m_directWatchTicketTimer, &QTimer::timeout, this, [this]() {
        if (m_watchIsRelay && (m_watchPreparing || m_directWatchTicketId != 0
                               || (m_directWatch && !m_directWatch->isReady()))) {
            const QPointer<StationClient> self(this);
            retireDirectWatch();
            if (self) { retryDirectWatch(QStringLiteral("watch attachment timed out")); }
            return;
        }
        if (m_directWatchTicketId == 0) { return; }
        m_directWatchTicketId = 0;
        m_directWatchTicketGeneration = 0;
        m_directWatchTicketSessionEpoch = 0;
        retryDirectWatch(QStringLiteral("watch ticket reply timed out"));
    });

    // iPhone app plan Task 27: an address that is not the last to try gets
    // kCachedAddressOpenTimeoutMs to open before the next one is tried.
    // iPhone app plan Task 29 (link section 21.3): looking for a better
    // path, and the bound on moving to one.
    m_upgradeScheduleMs = QList<int>(PathRacer::kUpgradeRetryMs.cbegin(),
                                     PathRacer::kUpgradeRetryMs.cend());
    m_upgradeTimer = new QTimer(this);
    m_upgradeTimer->setSingleShot(true);
    connect(m_upgradeTimer, &QTimer::timeout, this, &StationClient::startUpgradeRace);
    m_upgradeDeadline = new QTimer(this);
    m_upgradeDeadline->setSingleShot(true);
    m_upgradeDeadline->setInterval(SwitchableTransport::kSwitchDeadlineMs);
    connect(m_upgradeDeadline, &QTimer::timeout, this, [this] {
        abandonUpgrade(QStringLiteral("the Core did not give a ticket in time"), true);
    });

    m_openTimer = new QTimer(this);
    m_openTimer->setSingleShot(true);
    m_openTimer->setInterval(kCachedAddressOpenTimeoutMs);
    connect(m_openTimer, &QTimer::timeout, this, [this] {
        if (!m_transportOpened) {
            advanceDialPlan(StationConnectionAttempt::Outcome::TimedOut);
        }
    });

    // ── The settings proxy's OUTBOUND half ───────────────────────────────
    //
    // SettingsProxy.h's own contract says it emits these "for a live
    // session (Task 18) to relay over the wire", and until this connect
    // existed neither signal had a consumer anywhere in src/. The
    // operator-visible shape of that gap: a remote GUI's Setup change
    // updates the optimistic cache, appears to take, never reaches the
    // station, and is silently reverted by the next snapshot.
    //
    // Origin tag read at emit time rather than captured, because
    // handleSettingsSnapshot() assigns it after the handshake and these
    // connects are made in the constructor.
    if (m_settingsProxy != nullptr) {
        connect(m_settingsProxy, &SettingsProxy::outboundWriteRequested, this,
                [this](const QString& key, const QVariant& value) {
                    if (m_settingsProxy.isNull()) {
                        return;
                    }
                    send(SessionMessages::settingsWrite(key, value.toString(),
                                                        m_settingsProxy->localOriginTag()));
                });
        connect(m_settingsProxy, &SettingsProxy::outboundRemoveRequested, this,
                [this](const QString& key) {
                    send(SessionMessages::settingsRemove(key));
                });
    }
    if (m_sessionPurpose == SessionPurpose::RenameOnly) {
        m_declaredFeatures.clear();
    }
}

QByteArray StationClient::deviceIdentityFingerprint() const
{
    return m_deviceIdentity ? m_deviceIdentity->fingerprint() : QByteArray();
}

StationClient::~StationClient()
{
    // The model holds this pointer non-owningly and outlives this object
    // in the ordinary GUI teardown, so leaving it attached would leave
    // RadioModel routing operator clicks into freed memory. QPointer, so
    // the reverse order (model destroyed first) needs no special case.
    if (!m_radioModel.isNull()) {
        m_radioModel->detachStation();
    }
}

// ── Connecting ───────────────────────────────────────────────────────────

void StationClient::connectToStation(const QUrl& url, const QString& token,
                                     const QString& expectedFingerprint,
                                     bool allowUnpinned,
                                     const QByteArray& stationIdentityFingerprint)
{
    ++m_connectionRequestGeneration;
    // Fix round 1, Important 2. Every OTHER entry into connectToStation()
    // cancels a pending retry as a side effect of reaching attachTransport()
    // (which does this too, unconditionally, for the case where a caller
    // reconnects by hand while a backoff wait is still counting down). The
    // empty-fingerprint refusal below returns BEFORE ever reaching
    // dialStation() and therefore attachTransport(), so without this
    // explicit stop here, a retry armed by an earlier failed dial survived
    // an unrelated refused attempt at a DIFFERENT station untouched, and
    // would go on to silently redial the FIRST station once its backoff
    // elapsed -- an operator who tried station B and was told the
    // connection was refused would, moments later, find the GUI connected
    // to station A instead, unprompted. Stopped here, unconditionally,
    // before any other logic runs, so it also covers the refusal path.
    m_reconnectTimer->stop();

    // FIRST error wins for the rest of this connection attempt. A pinning
    // refusal is followed immediately by the socket errors it causes
    // ("the host name did not match", "remote host closed"), and reporting
    // the last of those to the operator would name a consequence instead
    // of the cause. Cleared here so a later attempt starts clean.
    m_lastError.clear();

    // iPhone app Task 18: a paired Core is trusted by its identity key,
    // which is checked at its hello, so it needs no pin here.
    if (expectedFingerprint.isEmpty() && !allowUnpinned && stationIdentityFingerprint.isEmpty()) {
        m_lastError = QStringLiteral(
            "No station certificate fingerprint to pin. Refusing to connect: an "
            "unpinned self-signed certificate authenticates nothing.");
        qCWarning(lcStationClient) << m_lastError;
        emit sessionEnded(m_lastError);
        emit connectionActivityChanged();
        return;
    }

    // A DELIBERATE, fresh attempt that is actually going to DIAL -- the
    // very first connect, or an operator manually reconnecting after
    // giving up -- always starts the backoff over. Moved below the
    // refusal check (fix round 1, Important 2): resetting it for a call
    // that never reaches dialStation() would silently reset the schedule
    // of a DIFFERENT, still-relevant retry sequence that happens to be
    // between attempts (timer briefly inactive, mid-redial) at the exact
    // moment an unrelated refusal runs. dialStation()'s own redial entry,
    // onReconnectTimeout(), deliberately does not reset this at all.
    m_reconnectAttempts = 0;
    // Task 28: a connection by address is not retried through the service.
    stopServiceDial();
    m_serviceServers.clear();
    m_serviceStationId.clear();

    // iPhone app plan Task 27: the Core's last good addresses first, then
    // the one asked for.
    // Fix wave I1: never for a Core saved with "connect without a
    // certificate fingerprint", whose token must go only where the operator
    // pointed it.
    m_dialPlan.clear();
    if (!allowUnpinned) {
        for (const QUrl& cached : std::as_const(m_cachedAddresses)) {
            if (cached.isValid() && cached != url && !m_dialPlan.contains(cached)) {
                m_dialPlan.append(cached);
            }
        }
    }
    if (!url.isEmpty()) {
        m_dialPlan.append(url);
    }
    m_planToken = token;
    // iPhone app plan Task 29 (R-IOS-16; link section 21.1): a paired Core
    // is raced: every address at once (IPv6 first), and the internet
    // service beside them. A Core trusted by its pin keeps the addresses
    // one after another.
    if (!stationIdentityFingerprint.isEmpty() && !allowUnpinned) {
        m_raceMode = true;
        m_lastUrl = url;
        m_lastFingerprint = expectedFingerprint;
        m_lastAllowUnpinned = false;
        m_stationIdentity = stationIdentityFingerprint;
        m_pinRequired = false;
        startRace();
        return;
    }
    m_raceMode = false;
    startDialPlan();
    dialStation(m_dialPlan.first(), token, expectedFingerprint, allowUnpinned,
                stationIdentityFingerprint);
}

void StationClient::setCachedAddresses(const QList<QUrl>& addresses)
{
    m_cachedAddresses = addresses;
    // An authenticated devices delta can add direct listeners while the
    // session is on the service. Existing retry/upgrade races use this plan.
    if (m_handshakeComplete && m_raceMode && !m_lastAllowUnpinned) {
        m_dialPlan.clear();
        for (const QUrl& address : addresses) {
            if (address.isValid() && !m_dialPlan.contains(address)) { m_dialPlan.append(address); }
        }
        if (!m_lastUrl.isEmpty() && !m_dialPlan.contains(m_lastUrl)) { m_dialPlan.append(m_lastUrl); }
    }
}

void StationClient::connectThroughService(const QList<QUrl>& servers,
                                          const QString& stationRendezvousId,
                                          const QByteArray& stationIdentityFingerprint)
{
    // As connectToStation(): a fresh attempt cancels a pending retry and
    // starts the backoff over.
    m_reconnectTimer->stop();
    m_lastError.clear();
    if (servers.isEmpty() || !RendezvousWire::isRendezvousId(stationRendezvousId)
        || stationIdentityFingerprint.isEmpty() || !m_deviceIdentity
        || !m_deviceIdentity->isValid()) {
        m_lastError = QStringLiteral(
            "This computer can reach the Core from anywhere only once it is paired with it.");
        qCWarning(lcStationClient) << "Not connecting through the remote access service:"
                                   << "no paired Core or no device key";
        emit sessionEnded(m_lastError);
        emit connectionActivityChanged();
        return;
    }
    m_reconnectAttempts = 0;
    stopRace();
    m_raceMode = false;
    m_dialPlan.clear();
    m_lastUrl.clear();
    m_planToken.clear();
    m_lastFingerprint.clear();
    m_lastAllowUnpinned = false;
    m_serviceServers = servers;
    m_serviceStationId = stationRendezvousId;
    m_stationIdentity = stationIdentityFingerprint;
    startDialPlan();
    dialThroughService();
}

void StationClient::stopServiceDial()
{
    m_serviceDialing = false;
    if (m_serviceDialer) {
        RendezvousDialer* dialer = m_serviceDialer;
        m_serviceDialer = nullptr;
        dialer->disconnect(this);
        dialer->cancel();
        dialer->deleteLater();
    }
}

void StationClient::dialThroughService()
{
    stopServiceDial();
    m_lastError.clear();
    // The attempt record: through the service, until the connection shows
    // it went through the relay or the web relay (step 2b: "through the
    // internet service", no longer "direct", which named the service's
    // host as if it were the Core's).
    StationConnectionAttempt::Try attempt;
    attempt.path = StationConnectionAttempt::Path::Service;
    attempt.address = m_serviceServers.first().host();
    m_attempt.tries.append(attempt);
    emit connectionAttemptChanged();

    auto* dialer = new RendezvousDialer(this);
    if (m_serviceDialDeadlineMs > 0) {
        dialer->setDialDeadlineMs(m_serviceDialDeadlineMs);
    }
    m_serviceDialer = dialer;
    m_serviceDialing = true;
    connect(dialer, &RendezvousDialer::ready, this, [this, dialer](DataChannelTransport* transport) {
        if (m_serviceDialer != dialer) {
            transport->closeLink(QStringLiteral("replaced"));
            transport->deleteLater();
            return;
        }
        m_serviceDialer = nullptr;
        m_serviceDialing = false;
        dialer->deleteLater();
        const std::optional<MediaIcePath> path = transport->selectedPath();
        if (path && (path->relayed() || path->viaLoopbackShim()) && !m_attempt.tries.isEmpty()) {
            m_attempt.tries.last().path = path->viaLoopbackShim()
                ? StationConnectionAttempt::Path::WebRelay
                : StationConnectionAttempt::Path::Relay;
            emit connectionAttemptChanged();
        }
        qCInfo(lcStationClient) << "Connected to the Core through the remote access service"
                                << (path && path->viaLoopbackShim() ? "(web relay)"
                                    : path && path->relayed()       ? "(relayed)"
                                                                    : "(direct)");
        m_pathRank = PathRacer::rankForPath(path);
        // Trusted by the identity key alone: the hello's binding is checked
        // against the certificate the Core presented in DTLS before
        // anything is sent (handleHello()).
        m_pinRequired = false;
        m_transportOpened = true;
        attachTransport(transport, QString());
    });
    connect(dialer, &RendezvousDialer::failed, this, [this, dialer](const QString& reason) {
        if (m_serviceDialer != dialer) {
            return;
        }
        m_serviceDialer = nullptr;
        m_serviceDialing = false;
        dialer->deleteLater();
        recordOutcome(StationConnectionAttempt::Outcome::NoAnswer);
        m_lastError = reason;
        qCInfo(lcStationClient) << "Could not connect through the remote access service";
        // As a failed first connect by address: reported, then retried.
        emit sessionEnded(reason);
        scheduleReconnect();
        emit connectionActivityChanged();
    });
    dialer->dial(m_serviceServers, m_serviceStationId, m_deviceIdentity);
    emit connectionActivityChanged();
}

std::optional<IceConfiguration> StationClient::sessionIceConfiguration() const
{
    const auto* transport = qobject_cast<const DataChannelTransport*>(this->transport());
    if (transport != nullptr) {
        return transport->mediaIceConfiguration();
    }
    // iPhone app plan Task 29 (link section 21.3): a session that came
    // through the service and moved to a direct connection keeps the
    // service's STUN server for its media, with no relay of its own.
    return m_serviceIce ? std::optional<IceConfiguration>(m_serviceIce->withoutOwnRelay())
                        : std::nullopt;
}

bool StationClient::mediaTunnelAvailable() const
{
    const SwitchableTransport* session = sessionTransport();
    return m_handshakeComplete && m_capabilities.mediaTunnelVersion >= 1 && mediaAvailable()
        && session != nullptr;
}

std::optional<IceConfiguration> StationClient::mediaTunnelIceConfiguration()
{
    if (!mediaTunnelAvailable() || !sessionTransport()->carriesBinary()) {
        return std::nullopt;
    }
    if (!m_mediaTunnel) {
        m_mediaTunnel = MediaTunnel::create(sessionTransport());
    }
    if (!m_mediaTunnel) {
        return std::nullopt;
    }
    return MediaTunnel::iceFor(m_mediaTunnel, mediaStunServer());
}

std::optional<IceServerAddress> StationClient::mediaStunServer() const
{
    // The direct media ladder (link section 21): the Core's own STUN list
    // (mediaStunUrls) first. libjuice has no TLS, so only a stun: entry is
    // usable.
    for (const QString& url : m_capabilities.mediaStunUrls) {
        if (auto stun = IceConfiguration::parseStunUrl(url)) {
            return stun;
        }
    }
    // Then the STUN server of this session's connection through the
    // service, and failing that the last one a session through the service
    // used. Kept in memory only, never saved.
    if (const auto ice = sessionIceConfiguration(); ice && ice->stunServer()) {
        m_lastServiceStun = ice->stunServer();
    }
    return m_lastServiceStun;
}

bool StationClient::mediaDirectAvailable() const
{
    return m_handshakeComplete && m_capabilities.mediaDirectVersion >= 1 && mediaAvailable();
}

IceConfiguration StationClient::mediaDirectIceConfiguration() const
{
    return MediaTunnel::directIceFor(mediaStunServer());
}

std::optional<IceConfiguration> StationClient::mediaTunnelOnlyIceConfiguration()
{
    // The direct media ladder's fallback: the same tunnel, alone.
    if (!mediaTunnelIceConfiguration()) {
        return std::nullopt;
    }
    return MediaTunnel::tunnelIceFor(m_mediaTunnel);
}

SwitchableTransport* StationClient::sessionTransport() const
{
    return qobject_cast<SwitchableTransport*>(m_transport);
}

SessionTransport* StationClient::transport() const
{
    if (const auto* switchable = qobject_cast<const SwitchableTransport*>(m_transport)) {
        return switchable->inner();
    }
    return m_transport;
}

void StationClient::setCachedAddressOpenTimeoutMs(int ms)
{
    m_openTimer->setInterval(std::max(1, ms));
}

void StationClient::startDialPlan()
{
    m_dialIndex = 0;
    m_attempt = StationConnectionAttempt{};
    m_attempt.started = QDateTime::currentDateTimeUtc();
    m_connectedUrl.clear();
    emit connectionAttemptChanged();
}

void StationClient::recordOutcome(StationConnectionAttempt::Outcome outcome)
{
    if (m_attempt.tries.isEmpty()
        || m_attempt.tries.last().outcome != StationConnectionAttempt::Outcome::Trying) {
        return;
    }
    m_attempt.tries.last().outcome = outcome;
    emit connectionAttemptChanged();
}

bool StationClient::advanceDialPlan(StationConnectionAttempt::Outcome outcome)
{
    m_openTimer->stop();
    if (m_dialPlan.isEmpty() || m_dialIndex + 1 >= m_dialPlan.size()
        || m_lastUrl != m_dialPlan.at(m_dialIndex)) {
        return false;
    }
    recordOutcome(outcome);
    ++m_dialIndex;
    qCInfo(lcStationClient) << "Trying the Core's next address";
    dialStation(m_dialPlan.at(m_dialIndex), m_planToken, m_lastFingerprint, m_lastAllowUnpinned,
                m_stationIdentity);
    return true;
}

void StationClient::dialStation(const QUrl& url, const QString& token,
                                const QString& expectedFingerprint, bool allowUnpinned,
                                const QByteArray& stationIdentityFingerprint)
{
    // First error wins for THIS attempt -- see connectToStation()'s own
    // clear. A redial from onReconnectTimeout() is a new attempt and gets
    // its own clean slate too.
    m_lastError.clear();

    // A PIN THAT IS CONFIGURED IS A PIN THAT MUST BE CHECKED, and a scheme
    // with no TLS under it cannot check one. RemoteStationOptions::
    // isValidStationUrl accepts ws:// for a loopback bench run and its
    // rejection message advertises it, while connectToStation() above
    // refuses only an EMPTY fingerprint. So an operator who had pinned a
    // fingerprint correctly and typed ws:// got no TLS, therefore no
    // sslErrors, therefore no pin comparison anywhere, and then
    // handleHello() put the shared pre-shared token on the wire in
    // cleartext. Checked BEFORE the latch below so a refused URL is not
    // left behind for onReconnectTimeout() to redial.
    if (url.scheme().compare(QLatin1String("wss"), Qt::CaseInsensitive) != 0
        && !expectedFingerprint.isEmpty()) {
        m_lastError =
            QStringLiteral("Refusing to connect to %1: a station certificate "
                           "fingerprint is pinned, but \"%2\" carries no TLS, so "
                           "there is nothing to compare the fingerprint against "
                           "and the pairing token would travel in cleartext. Use "
                           "wss://.")
                .arg(url.toString(QUrl::RemovePassword), url.scheme());
        qCWarning(lcStationClient) << m_lastError;
        // Emitted directly rather than through endSession(), matching
        // connectToStation()'s own empty-fingerprint refusal: no session
        // has been attached yet, so endSession()'s m_sessionActive guard
        // would swallow this and the caller would hear nothing at all.
        emit sessionEnded(m_lastError);
        emit connectionActivityChanged();
        return;
    }

    // iPhone app Task 18: the same for a paired Core. Its certificate
    // binding is what proves it, and without TLS there is no certificate
    // to check the binding against.
    if (url.scheme().compare(QLatin1String("wss"), Qt::CaseInsensitive) != 0
        && !stationIdentityFingerprint.isEmpty()) {
        m_lastError = QStringLiteral("This computer connects to a Core it paired with only "
                                     "over a secure address beginning with wss://.");
        qCWarning(lcStationClient) << m_lastError << url.toString(QUrl::RemovePassword);
        emit sessionEnded(m_lastError);
        emit connectionActivityChanged();
        return;
    }

    // Whether this attempt owes a certificate comparison before it may
    // send the token. The conjunction is the SAME one the sslErrors bypass
    // below has always used: opting out requires both an explicit
    // allowUnpinned and no configured pin. attachTransport() turns this
    // into the per-attach m_pinSatisfied.
    //
    // iPhone app Task 18: a paired Core owes none. Its identity key and
    // certificate binding are checked at its hello instead, so a Core
    // whose certificate changed is still recognised by its key.
    m_pinRequired = stationIdentityFingerprint.isEmpty()
                    && !(expectedFingerprint.isEmpty() && allowUnpinned);
    m_stationIdentity = stationIdentityFingerprint;

    // Latched (Task 19) so a later automatic retry can redial identically.
    // Parent design section 13: "onReconnectTimeout slot with latched host
    // and port."
    m_lastUrl = url;
    m_lastFingerprint = expectedFingerprint;
    m_lastAllowUnpinned = allowUnpinned;

    auto* socket = new QWebSocket();
    // Capped before the socket is ever opened. A pinned certificate proves
    // WHO the station is, not that it will behave; see
    // kMaxIncomingMessageBytes for how the number was derived from the
    // connect-time settings snapshot.
    auto* transport = new WebSocketTransport(socket, kMaxIncomingMessageBytes);
    // Task 19 fix: captured so the two lambdas below can tell a STALE
    // socket's asynchronous signal apart from the current one's. See the
    // class comment's link-loss section for the defect this closes --
    // the Task 18 review found it and graded it Minor only because
    // nothing called connectToStation() (and now dialStation()) twice on
    // one client before this task's automatic reconnect existed to do
    // exactly that.
    const QPointer<SessionTransport> transportGuard(transport);
    const QString pinned = expectedFingerprint.toUpper();

    connect(socket, &QWebSocket::sslErrors, this,
            [this, socket, pinned, allowUnpinned, transportGuard](const QList<QSslError>& errors) {
                // A stale socket's error must not act on whatever session
                // is CURRENT by the time it arrives -- the same guard
                // onTransportClosed() uses, applied here because these
                // two lambdas are connected to the QWebSocket, not the
                // SessionTransport wrapping it, so attachTransport()'s
                // disconnect(stale, ...) release does not reach them.
                if (transportGuard.isNull() || transportGuard.data() != this->transport()) {
                    return;
                }
                if (pinned.isEmpty() && allowUnpinned) {
                    socket->ignoreSslErrors(errors);
                    return;
                }
                // The comparison itself now lives in ensurePinSatisfied(),
                // which this path shares with the connected() handler
                // below and with handleHello()'s gate. Keeping it here as
                // well as there is what makes a MISMATCH surface at the
                // earliest possible moment -- mid-handshake, before the
                // socket is even usable -- while the other two callers are
                // what make the comparison happen AT ALL on a handshake
                // that produced no errors to report.
                if (!ensurePinSatisfied()) {
                    return;
                }
                // The pinned fingerprint IS the identity check (parent
                // design section 10.5), so chain-trust errors that are
                // wholly explained by self-signing are ignored. Validity
                // dates are NOT: an expired certificate on a pinned
                // fingerprint still means somebody's clock is wrong, and
                // silently accepting it hides that.
                QList<QSslError> ignorable;
                for (const QSslError& error : errors) {
                    if (error.error() == QSslError::CertificateExpired
                        || error.error() == QSslError::CertificateNotYetValid) {
                        continue;
                    }
                    ignorable.append(error);
                }
                socket->ignoreSslErrors(ignorable);
            });

    // THE fix for the second reachable instance of the pinning gap.
    // QWebSocket::sslErrors fires ONLY when the handshake produced errors,
    // so a handshake the client's own trust store already accepts -- a
    // corporate or antivirus MITM root, or a genuine DV certificate issued
    // for a dynamic-DNS station name -- never reached the comparison at
    // all. That is precisely the case pinning exists for. connected()
    // fires on every successful handshake, error-free ones included, and
    // by then sslConfiguration().peerCertificate() is populated.
    //
    // This is belt to handleHello()'s braces rather than a replacement for
    // it: Qt emits connected() before it delivers any frame on the same
    // socket, so this normally runs first, but the token is gated on
    // m_pinSatisfied at the one place it is actually sent, so the ordering
    // does not have to be relied upon.
    connect(socket, &QWebSocket::connected, this, [this, transportGuard]() {
        if (transportGuard.isNull() || transportGuard.data() != this->transport()) {
            return;
        }
        // iPhone app plan Task 27: this address answered.
        m_transportOpened = true;
        m_openTimer->stop();
        ensurePinSatisfied();
    });

    // A proxy that demands a login: NereusSDR gives it none (JJ's ruling of
    // 2026-09-28), so the socket fails next; its reason says why, whichever
    // of the error and the close comes first.
    connect(socket, &QWebSocket::proxyAuthenticationRequired, this,
            [this, transportGuard](const QNetworkProxy&, QAuthenticator*) {
                if (transportGuard.isNull() || transportGuard.data() != this->transport()) {
                    return;
                }
                if (m_lastError.isEmpty()) {
                    m_lastError = NetworkTrouble::proxyNeedsLoginWords();
                }
            });
    connect(socket, &QWebSocket::errorOccurred, this,
            [this, socket, transportGuard, host = url.host()](QAbstractSocket::SocketError error) {
                if (transportGuard.isNull() || transportGuard.data() != this->transport()) {
                    return;
                }
                // First error wins -- see dialStation()'s clear above. The
                // operator reason may name a recovery action (R-R3-17); the
                // raw socket error still goes to the log below.
                if (m_lastError.isEmpty()) {
                    m_lastError = connectionFailureReason(error, socket->errorString(), host);
                }
                qCWarning(lcStationClient)
                    << "Station connection error:" << socket->errorString();
                // iPhone app plan Task 27: an address that never opened
                // gives way to the next one this attempt has.
                if (!m_transportOpened
                    && advanceDialPlan(StationConnectionAttempt::Outcome::NoAnswer)) {
                    return;
                }
                recordOutcome(m_transportOpened ? StationConnectionAttempt::Outcome::Failed
                                                : StationConnectionAttempt::Outcome::NoAnswer);
                // A refused or unreachable station emits this and may never
                // emit disconnected at all, so waiting for a close would
                // leave the caller with no signal whatsoever. endSession()
                // is idempotent per attach, so a socket error DURING a live
                // session that is followed by a real close still reports
                // once. A socket-level error is retry-eligible (Task 19):
                // this is the "station is down, or restarting" case
                // automatic reconnect exists for.
                endSession(m_lastError, /*attemptReconnect=*/true);
            });

    attachTransport(transport, token);

    // iPhone app plan Task 27: this address, in the attempt's record (after
    // the attach, which ends the address before it silently); one that is
    // not the last to try gets a short time to open.
    m_transportOpened = false;
    StationConnectionAttempt::Try attempted;
    attempted.path = StationConnectionAttempt::pathFor(url);
    const QString host = url.host().contains(QLatin1Char(':'))
        ? QLatin1Char('[') + url.host() + QLatin1Char(']')
        : url.host();
    attempted.address = url.port() > 0 ? QStringLiteral("%1:%2").arg(host).arg(url.port()) : host;
    m_attempt.tries.append(attempted);
    emit connectionAttemptChanged();
    if (m_dialIndex + 1 < m_dialPlan.size() && m_dialPlan.at(m_dialIndex) == url) {
        m_openTimer->start();
    } else {
        m_openTimer->stop();
    }
    // Task 29 step 2b (options survey B.5): the computer's own proxy
    // settings (SystemProxy), for a network that reaches out only through
    // one. The Core's identity check at its hello is unchanged.
    socket->setProxy(SystemProxy::forUrl(url));
    socket->open(url);
}

// The one place the pinned fingerprint is compared, called from three:
// the sslErrors handler (earliest possible refusal on a handshake that
// reported problems), the connected() handler (every OTHER handshake,
// which is the case the old code never checked at all), and handleHello()
// immediately before the token would go out (the gate that makes the
// property hold regardless of which of the other two ran, or whether
// either did).
//
// Idempotent and cheap after the first success: m_pinSatisfied latches.
//
// A refusal here is a SECURITY refusal, not a transient link failure, so
// it is never retry-eligible. Redialing the same latched station that is
// presenting the same wrong certificate cannot converge, and retrying
// forever against an active MITM is worse than stopping.
bool StationClient::ensurePinSatisfied()
{
    if (m_pinSatisfied) {
        return true;
    }

    auto* wsTransport = qobject_cast<WebSocketTransport*>(transport());
    QWebSocket* socket = wsTransport != nullptr ? wsTransport->socket() : nullptr;
    // iPhone app Task 18: read through the transport, which is where the
    // device sign-in reads the same certificate (a WebSocketTransport
    // digests its socket's peer certificate, exactly as this did).
    const QByteArray peerDigest =
        m_transport != nullptr ? m_transport->peerCertificateSha256() : QByteArray();

    if (peerDigest.isEmpty()) {
        // A pin is configured and the link cannot produce a certificate to
        // check it against. dialStation() refuses a non-TLS scheme up
        // front, so reaching this means something stranger: a transport
        // that is not a WebSocketTransport at all, or a wss socket whose
        // peer certificate is somehow absent. Refuse rather than fall
        // through, because falling through is what sends the token.
        m_lastError = QStringLiteral(
            "Refusing to authenticate: a station certificate fingerprint is "
            "pinned, but this link presented no certificate to compare it "
            "against.");
        qCWarning(lcStationClient) << m_lastError;
        // endSession FIRST, abort second, for the same reason
        // disconnectFromStation() documents for closeLink(): abort()
        // emits errorOccurred SYNCHRONOUSLY, that handler calls
        // endSession(attemptReconnect = true), and endSession is
        // once-per-attach -- so aborting first let the socket error win
        // the race and schedule a reconnect for a refusal this function
        // has just declared non-retryable. Observed in the log as
        // "Scheduling reconnect attempt 1" immediately after a fingerprint
        // mismatch.
        endSession(m_lastError, /*attemptReconnect=*/false);
        if (socket != nullptr) {
            socket->abort();
        }
        return false;
    }

    const QString pinned = m_lastFingerprint.toUpper();
    const QString actual = formatFingerprint(peerDigest);
    if (actual != pinned) {
        // Fix wave I1: another computer at an address that is not the last
        // this attempt has (a cached address given to something else since)
        // gives way to the next, before the token was sent, as
        // refuseStation() does for identityChanged. The redial retires this
        // transport, so its later socket signals are ignored.
        if (advanceDialPlan(StationConnectionAttempt::Outcome::NotThisCore)) {
            qCInfo(lcStationClient)
                << "Another certificate answered at a saved address of the Core";
            return false;
        }
        m_lastError = QStringLiteral("Station certificate fingerprint does not match the saved pin.");
        qCWarning(lcStationClient) << m_lastError;
        // See the ordering note above: endSession, then abort.
        endSession(m_lastError, /*attemptReconnect=*/false);
        if (socket != nullptr) {
            socket->abort();
        } else if (m_transport != nullptr) {
            m_transport->closeLink(m_lastError);
        }
        return false;
    }

    m_pinSatisfied = true;
    return true;
}

void StationClient::startSession(SessionTransport* transport, const QString& token,
                                 const QString& expectedFingerprint,
                                 const QByteArray& stationIdentityFingerprint)
{
    ++m_connectionRequestGeneration;
    // A pin this caller states is a pin this session owes, exactly as on
    // the dial path. With no fingerprint (the default, and every adopted
    // transport in the tree today) there is nothing to compare and nothing
    // to require. Set BEFORE attachTransport(), which is what turns it
    // into this attach's m_pinSatisfied, and latched into
    // m_lastFingerprint because ensurePinSatisfied() reads it from there.
    m_pinRequired = !expectedFingerprint.isEmpty() && stationIdentityFingerprint.isEmpty();
    m_stationIdentity = stationIdentityFingerprint;

    // Fix round 1, Minor 3. Without this, a client that once dialed via
    // connectToStation() (latching m_lastUrl to something real) and LATER
    // runs a startSession()-based seam session -- this suite's own pattern
    // for the manual-reconnect half of the link-loss narrative, and a
    // legitimate production sequence too if a caller ever mixes the two
    // entry points -- would keep the STALE latch from the earlier real
    // dial. A retry-eligible close of the SEAM session would then
    // scheduleReconnect() find m_lastUrl still valid and silently redial
    // the earlier, unrelated target. Invalidating it here is what makes
    // the class comment's claim -- "nothing to redial" for a
    // startSession()-based session -- actually true rather than true only
    // for a client that has never dialed at all.
    //
    // m_lastFingerprint is the exception among the three: it is not a
    // redial target, it is what ensurePinSatisfied() compares against, so
    // it takes this call's own value rather than being blanked. With the
    // default empty argument that is byte-identical to the clear this
    // replaced.
    m_lastUrl.clear();
    // iPhone app plan Task 27: nothing to redial, so no addresses either.
    m_dialPlan.clear();
    // An adopted transport owns no paired race target either. Cancel any
    // prior race and retire its route before this session takes ownership.
    stopRace();
    m_raceMode = false;
    m_serviceRoute = ServiceRoute{};
    // Task 28: nor a route through the service.
    stopServiceDial();
    m_serviceServers.clear();
    m_serviceStationId.clear();
    m_openTimer->stop();
    m_lastFingerprint = expectedFingerprint;
    m_lastAllowUnpinned = false;
    attachTransport(transport, token);
}

void StationClient::attachTransport(SessionTransport* transport, const QString& token)
{
    if (transport == nullptr) {
        return;
    }
    const QPointer<StationClient> watchSelf(this);
    const QPointer<SessionTransport> requestedTransport(transport);
    retireDirectWatch();
    if (!watchSelf) { return; }
    m_directWatchDeclared = false;
    m_watchRelayDeclared = false;

    // A directly adopted replacement link needs the same retirement as a
    // redial. Otherwise the previous authentication/snapshot flags remain
    // true until the new Hello arrives, admitting work into an unverified
    // session. Retire its media and mirror state, preserving the existing
    // contract that a deliberate redial emits no sessionEnded notification.
    if (m_sessionActive) {
        const quint32 priorEpoch = m_sessionEpoch;
        endSession(QStringLiteral("replaced by a newer session"), false, false);
        if (!watchSelf || m_sessionEpoch != priorEpoch || m_sessionActive) {
            if (requestedTransport && (!watchSelf || requestedTransport != m_transport)) {
                requestedTransport->closeLink(QStringLiteral("superseded during reconnect"));
                if (requestedTransport) requestedTransport->deleteLater();
            }
            return;
        }
    }

    // RELEASE THE OLD LINK FIRST. Overwriting m_transport without this was
    // a reconnect defect waiting for Task 19: WebSocketTransport::closeLink
    // closes ASYNCHRONOUSLY, so "heartbeat timeout, reconnect from the
    // slot, the old socket's disconnected arrives a moment later" drove the
    // BRAND NEW session to Disconnected, stopped both timers and called
    // setReady(false). Every reconnect also leaked a WebSocketTransport and
    // its QWebSocket, still connected to onTransportText.
    //
    // Disconnecting every signal from the old transport to this object
    // FIRST is what makes the subsequent closeLink() safe: the close it
    // provokes can no longer reach onTransportClosed(). onTransportClosed()
    // additionally ignores anything that is not the current transport (see
    // there), so the two protections are independent -- the same
    // erase-then-look-up discipline StationServer::dropPeer already uses.
    if (m_transport != nullptr && m_transport != transport) {
        SessionTransport* stale = m_transport;
        m_transport = nullptr;
        disconnect(stale, nullptr, this, nullptr);
        stale->closeLink(QStringLiteral("replaced by a newer session"));
        stale->deleteLater();
    }

    // Task 19: a fresh attach -- whether from a manual reconnect or from
    // onReconnectTimeout()'s own redial -- supersedes any retry this
    // client might independently have pending (e.g. a caller reconnecting
    // by hand, via startSession(), while a connectToStation()-originated
    // backoff wait was still counting down). Stopping here, unconditionally,
    // is what keeps that stray timer from firing a redial on top of a
    // session that is already re-establishing.
    m_reconnectTimer->stop();

    // iPhone app plan Task 29 (link section 21.2): every session runs on a
    // SwitchableTransport, which can move it to a better path without
    // ending it.
    if (qobject_cast<SwitchableTransport*>(transport) == nullptr) {
        transport = new SwitchableTransport(transport, SwitchableTransport::Side::Client,
                                            kMaxIncomingMessageBytes, this);
    }
    transport->setParent(this);
    m_transport = transport;
    m_token = token;
    m_pathSwitches = 0;
    m_serviceIce.reset();
    m_mediaTunnel.reset();
    m_mediaTunnelInUse = false;
    m_stationRendezvousId.clear();
    // R-R3-38: a new link starts with no end recorded, so the report never
    // describes an older one.
    m_lastEndReport = StationEndReport{};
    m_pingsAwaitingPong = 0;
    m_linkUp = false;
    m_sessionActive = true;
    // iPhone app Task 4: nothing the previous station declared carries
    // over; the new station's hello says it again.
    m_agreedMajor = 0;
    m_stationFeatures.clear();
    m_enrollingIdentity.clear();

    // These are optional, remote-only fields.  A fresh peer may predate
    // them, in which case its snapshot cannot overwrite a status received
    // from the previous station.  Do this at the attach boundary rather
    // than on a same-session re-seed, whose status remains authoritative.
    if (m_radioModel != nullptr) {
        m_radioModel->applyStationReceiveLayoutStatus("receiveLayoutRestoreState", {});
        m_radioModel->applyStationReceiveLayoutStatus("receiveLayoutRestoreMessage", {});
    }

    // Per-attach, never carried across one. A reconnect re-dials and gets
    // a fresh TLS handshake, possibly against a different certificate, so
    // a pin satisfied by the PREVIOUS session says nothing about this one.
    // The caller (dialStation or startSession) has already set
    // m_pinRequired for this attempt.
    m_pinSatisfied = !m_pinRequired;

    // Task 19: a new epoch for every attach, including the first (so the
    // first session is epoch 1; 0 means "never attached"). See
    // sessionEpoch()'s doc comment.
    ++m_sessionEpoch;
    // The old watch was folded before this new logical primary epoch.
    m_retiredWatchTelemetry = {};
    m_cancelledSettingsBackupBegin.reset();
    m_hygieneValidateId = 0;
    m_hygieneValidateMac.clear();
    m_hygieneValidateDirty = false;
    m_hygieneMutations.clear();
    if (!m_radioModel.isNull()) {
        m_radioModel->settingsHygiene().setRemoteUnavailable(
            QStringLiteral("Not connected to the Core."));
    }
    m_settingsSnapshotThisLink = false;
    m_lastTelemetrySequence = 0;
    m_lastTelemetrySampleElapsedMs = -1;
    m_capabilities.remoteDisplayBudgetVersion = 0;
    m_capabilities.radioAntennaRowsVersion = 0;
    m_capabilities.band2mVersion = 0;
    m_capabilities.coreBuildInfo.reset();
    m_capabilities.displayBudget.reset();
    m_capabilities.displayBudgetReason.reset();
    m_capabilities.remotePs3DisplaySubscribed = false;

    // These three describe THIS session. Carrying them across a reconnect
    // would let a difference the station has since fixed keep showing up
    // in a diagnostic Task 20's bench is meant to trust.
    m_schemaOnlyOnStation.clear();
    m_schemaOnlyLocal.clear();
    m_unapplied.clear();
    m_unheldDeltaKeys.clear();
    m_pendingStationSchemas.clear();

    const quint32 epoch = m_sessionEpoch;
    connect(transport, &SessionTransport::textReceived, this,
            [this, transport, epoch](const QByteArray& wire) {
        // Disconnecting does not cancel already queued deliveries. A delayed
        // observation or snapshot from a replaced transport cannot become part
        // of its successor, even if a later allocation reuses the address.
        if (m_transport == transport && m_sessionEpoch == epoch) {
            onTransportText(wire);
        }
    });
    connect(transport, &SessionTransport::pongReceived, this,
            [this]() { m_pingsAwaitingPong = 0; });
    connect(transport, &SessionTransport::closed, this, &StationClient::onTransportClosed);

    // The heartbeat deliberately does NOT start here. A wss dial can take
    // seconds, and a heartbeat counting missed pongs across a socket that
    // has not finished connecting reports a slow dial as a dead station.
    // It starts on the first inbound frame instead (onTransportText), which
    // is the station's own Hello and therefore proof the link carries
    // traffic in both directions.
    //
    // R-R3-16/17: which left the wait BEFORE that frame unbounded. The
    // 2026-09-23 incident sat there for 8.7 minutes: Core's event loop was
    // blocked, so the TLS and WebSocket upgrade and Core's Hello all waited
    // on it, and nothing on this side was counting. The handshake deadline
    // covers exactly that window and the rest of the connect sequence. It
    // is armed here rather than on QWebSocket::connected because the
    // upgrade itself is part of what stalls, and it runs until the
    // snapshot-complete marker, the point the session is usable.
    if (m_handshakeDeadlineMs > 0) {
        m_handshakeDeadlineTimer->start(m_handshakeDeadlineMs);
    } else {
        m_handshakeDeadlineTimer->stop();
    }
    emit connectionActivityChanged();
}

void StationClient::disconnectFromStation(const QString& reason, bool attemptReconnect)
{
    ++m_connectionRequestGeneration;
    // Not reconnecting (the operator's Disconnect, a permanent end): no
    // radio change is being waited out any more.
    if (!attemptReconnect) {
        m_radioChangeReason.clear();
    }
    // Task 19: cancel a PENDING retry even when no session is active at
    // all -- the backoff-wait state has m_sessionActive already false
    // (the session it was about already ended), so endSession()'s own
    // guard below would skip the block that normally stops this timer.
    // This is the other half of what makes the timer genuinely
    // cancellable rather than merely stoppable-from-inside-a-live-session:
    // parent design section 13, "ICE restart and operator-initiated
    // disconnect both need [cancellability]."
    if (m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
    }
    // Task 28: an attempt through the service still making its connection.
    stopServiceDial();
    // Task 29: a race, or a look for a better path, still running.
    stopRace();

    // endSession FIRST, closeLink second. A transport can deliver its
    // closed() signal synchronously (the in-process one does, and nothing
    // forbids it), and endSession() is once-per-attach, so closing first
    // let the close handler's generic "link closed" win the race and the
    // caller was told that instead of "heartbeat timeout" -- the specific
    // reason, thrown away by ordering alone. endSession() touches no
    // transport, so running it first is safe.
    const QPointer<StationClient> self(this);
    const QPointer<SessionTransport> endingTransport(m_transport);
    const quint32 endingEpoch = m_sessionEpoch;
    endSession(reason, attemptReconnect);
    if (!self || m_sessionEpoch != endingEpoch || m_transport != endingTransport) return;
    if (endingTransport) {
        endingTransport->closeLink(reason);
    }
    if (!self || m_sessionEpoch != endingEpoch || m_transport != endingTransport) return;
    // A cancelled backoff has no active session for endSession() to retire.
    emit connectionActivityChanged();
}

void StationClient::onTransportClosed()
{
    // Ignore a close from a transport this client has already moved on
    // from. sender() is null when this is called directly rather than
    // through the signal, which is a legitimate internal path.
    if (sender() != nullptr && sender() != m_transport) {
        return;
    }
    m_settingsSnapshotThisLink = false;   // rework follow-up 4
    // iPhone app plan Task 27: an address that closed before it ever opened
    // gives way to the next one this attempt has.
    if (!m_transportOpened && advanceDialPlan(StationConnectionAttempt::Outcome::NoAnswer)) {
        return;
    }
    // Task 19: a plain transport close with no station-sent reason is
    // exactly the case automatic reconnect exists for -- "kill the
    // daemon" (a clean TCP close) is the bench scenario the parent task
    // brief opens with, and it looks exactly like this: no SessionEnd
    // message (nothing was left alive to send one), just the socket going
    // away. Retry-eligible.
    endSession(m_lastError.isEmpty() ? QStringLiteral("link closed") : m_lastError,
              /*attemptReconnect=*/true);
}

// The single place a session ends, so sessionEnded() fires AT MOST ONCE
// per attach no matter which of the six paths got here (peer close, socket
// error, heartbeat timeout, station SessionEnd, version refusal, auth
// refusal). Not "exactly once" (fix round 1, Minor 7): an attach that is
// SUPERSEDED by a fresh attachTransport() before its own endSession() ever
// runs is released silently, with no sessionEnded for it at all -- see
// the header's own note on this method for the covering test.
//
// The previous shape emitted only `if (m_handshakeComplete)`, which swallowed
// the two failures that matter most:
//
//   - A failed INITIAL connect (station down, wrong port, TLS refused)
//     produced no signal at all, for a full 40 to 60 second heartbeat
//     window. v0.5.1 shipped "connection state stuck Connected on failed
//     initial connect"; this is the same bug class, so it gets a named
//     mechanism rather than a gate that happens to be true on the paths
//     someone tested.
//   - The client's own heartbeat timeout, because disconnectFromStation()
//     cleared m_handshakeComplete before the close handler read it.
//
// Task 19 extends this with `attemptReconnect` and the mirror teardown
// below. See the class comment's link-loss section for the full
// contract this enforces: the mirror registry does not survive a link
// loss, RadioModel's own state does (retained, not reset -- design doc
// section 13), and only a closure that WANTS a retry re-arms the
// automatic reconnect timer.
void StationClient::endSession(const QString& reason, bool attemptReconnect,
                               bool reportSessionEnd)
{
    if (!m_sessionActive) {
        return;  // already reported for this attach
    }
    // Retire the operation now, but notify only as this teardown returns.
    // A completion handler may destroy this client or reconnect it. Emitting
    // before clearing mirrors/proxy/coalescer let that new session inherit
    // the old one, because the outer cleanup then correctly refused to touch
    // the replacement. The guard owns no client pointer and runs last.
    const QPointer<RadioModel> exportModel(m_radioModel);
    const quint32 exportOperation = m_settingsBackupExport
        ? m_settingsBackupExport->operationId : 0;
    m_settingsBackupExport.reset();
    m_settingsBackupReplyTimer->stop();
    m_settingsBackupOverallTimer->stop();
    const auto exportCompletion = qScopeGuard([exportModel, exportOperation]() {
        if (exportModel && exportOperation != 0) {
            exportModel->reportStationSettingsBackupExportFinished(
                exportOperation, false, QStringLiteral("The Core connection ended."), {});
        }
    });
    m_sessionActive = false;
    m_handshakeComplete = false;
    m_authenticated = false;
    m_cancelledSettingsBackupBegin.reset();
    const QPointer<StationClient> watchSelf(this);
    retireDirectWatch();
    if (!watchSelf) { return; }
    // iPhone app plan Task 29: a look for a better path belongs to the
    // session that ended.
    m_upgradeTimer->stop();
    abandonUpgrade(QString(), /*reschedule=*/false);
    if (m_upgradeRacer) {
        PathRacer* racer = m_upgradeRacer;
        m_upgradeRacer = nullptr;
        racer->disconnect(this);
        racer->cancel();
        racer->deleteLater();
    }
    // iPhone app plan Task 27: an address still being tried ended here (the
    // connect deadline, a refusal, the operator).
    m_openTimer->stop();
    recordOutcome(m_transportOpened ? StationConnectionAttempt::Outcome::Failed
                                    : StationConnectionAttempt::Outcome::NoAnswer);

    // Follow-up N2: a radio change the Core never comes back from. The
    // reading holds through the backoff's steps; the failed redial after
    // the longest wait drops it, so this failure is what the window shows
    // (status, detail and toast). The radio-change end itself carries the
    // reason just recorded and keeps it.
    if (!m_radioChangeReason.isEmpty() && attemptReconnect && reconnectBackoffExhausted()
        && reason != m_radioChangeReason) {
        m_radioChangeReason.clear();
    }

    m_capabilities.coreBuildInfo.reset();
    m_signedInWithDeviceKey = false;
    m_capabilities.radioAntennaRowsVersion = 0;
    m_capabilities.band2mVersion = 0;
    m_enrolledDeviceKey = false;
    m_hygieneValidateId = 0;
    m_hygieneValidateMac.clear();
    m_hygieneValidateDirty = false;
    m_hygieneMutations.clear();
    if (m_radioModel) {
        m_radioModel->settingsHygiene().setRemoteUnavailable(
            QStringLiteral("Not connected to the Core."));
    }
    // Task 78: who else was on the Core was this session's.
    m_declaredSessionHolder = false;
    m_remoteDevices->clear();
    // Slice control plan Task 5: so were its access objects. The slices keep
    // their read-only marks until the next session's objects arrive.
    m_sliceAccess->clear();
    m_sliceAccess->setCoreSliceTakeable(false);
    if (m_radioModel) {
        m_radioModel->setStationMayCloseLastSlice(false);
    }
    m_forwardLocalChanges = false;
    m_pendingWrites.clear();
    m_coreValues.clear();
    // Desktop remote transmit: the Core unkeys this device when the link
    // drops and never keys it again by itself; nothing of it is kept.
    refreshRemoteTransmit();
    if (m_radioModel) {
        m_radioModel->dspAssets()->resetSession();
        m_radioModel->dspAssets()->setRemoteNr3ModelsSupported(false);
        m_radioModel->pureSignalFacade()->resetSession();
        // The window keeps the Core's last notch list; unanswered requests
        // and held edits belong to the retired session.
        if (m_radioModel->notchModel()) {
            m_radioModel->notchModel()->resetSession();
        }
    }
    m_linkUp = false;
    m_heartbeatTimer->stop();
    m_writeFlushTimer->stop();
    m_handshakeDeadlineTimer->stop();
    // Results from the retired session can no longer arrive. Keeping its
    // unanswered commands would suppress completions for fresh requests
    // after reconnect (including the 4O3A master and C-Tune controls).
    m_pendingCommands.clear();
    if (!m_radioModel.isNull()) {
        // Fix wave GUI-I6: nothing of the retired session is on its way.
        m_radioModel->transmitModel().setTunePowerForTxBandWriteInFlight(false);
    }
    m_controlTakeBacks.clear();
    m_pendingPs3Display.reset();

    // Fix round 1, Important 1: disconnect the dead transport's signals to
    // this object. Without this, m_transport stays fully wired
    // (onTransportText, onTransportClosed, the pongReceived lambda) for
    // the entire stale window even though the session it belonged to has
    // just ended -- onTransportText() has no m_sessionActive gate of its
    // own (only the heartbeat-start check does), so a frame arriving late
    // on this SAME transport is dispatched in full, and a buffered
    // SnapshotComplete would silently re-set m_handshakeComplete /
    // m_everConnected / m_forwardLocalChanges with no sessionEnded ever
    // firing for the attach that just ended. Exactly the case this
    // subsystem exists to prevent: a link that resumes after a heartbeat
    // timeout, delivering a frame that was already in flight when the
    // timeout was declared. Mirrors attachTransport()'s identical
    // disconnect for a superseded transport. Deliberately does NOT null
    // m_transport: disconnectFromStation()'s subsequent closeLink() call
    // is a direct call on the object itself, not a signal delivery, and is
    // unaffected by severing its signals TO this object; the pointer stays
    // valid until the next attachTransport() releases it the same way a
    // superseded transport is released.
    if (m_transport != nullptr) {
        disconnect(m_transport, nullptr, this, nullptr);
    }

    // Task 19 step 2: mirror teardown. All three describe THIS session and
    // none may survive it:
    //
    //   - m_objects (mirroredObjectKeys()/mirroredObject()) is the wire-key
    //     -> live-object registry. Its own doc comment says "Task 19 tears
    //     this down on link loss." Clearing it is what makes handleDelta()
    //     drop every further inbound frame for an unknown key (there is
    //     nothing to route it to) rather than silently keep applying
    //     traffic from a session that no longer exists, and what makes
    //     mirroredObjectKeys() correctly read empty while disconnected.
    //   - m_outboundMirror->unwatchAll() stops the outbound watcher.
    //     m_forwardLocalChanges (already false above) already prevents any
    //     local change from being forwarded while disconnected, so this is
    //     not independently load-bearing for "drops writes" -- it is
    //     hygiene: a long disconnect should not leave stale QMetaObject
    //     connections to objects that may be destroyed by some other path
    //     before reconnect, and it is the symmetric counterpart to the
    //     re-watching handleCapabilities()/resolveOrCreate() do on the
    //     next attach.
    //   - m_outboundCoalescer.clear() IS load-bearing: without it, a local
    //     edit that was marked dirty but never reached a flush before the
    //     link died would sit pending, and once the NEXT session's write
    //     flush timer resumes, onWriteFlushTick() would re-resolve it
    //     against the (by then reconnected) live model and send it as a
    //     property.write -- telling the fresh station its own
    //     just-applied value back, and doing so under the guise of a
    //     "fresh snapshot" that is supposed to have no leftover cruft from
    //     the session before it.
    //
    // RadioModel's own state -- SliceModel::frequency() and the rest -- is
    // deliberately NOT touched here. No removeSlice() call, no reset to a
    // default. Section 13: "the client retains last-known state". A
    // reconnect ADOPTS the retained SliceModel objects under the station's
    // ids (resolveOrCreate(), unchanged by this task), which is what lets
    // a GUI holding a raw pointer to one survive a reconnect unchanged.
    m_objects.clear();
    m_outboundMirror->unwatchAll();
    m_outboundCoalescer.clear();
    // iPhone app plan Task 39: nothing is on the air as far as this window
    // can tell once its Core is gone; the last stop stays until the next
    // snapshot replaces it.
    m_transmitState->clearStationValues();
    // iPhone app plan Task 25: the Core computer's VAX leaves the applet
    // until the next snapshot sends it again.
    m_stationVax->clearStationValues();
    if (m_stationVaxHeld) {
        m_stationVaxHeld = false;
        emit stationVaxAvailabilityChanged();
    }
    // Parity Task 19: the Core's spot sources read off, and its spots leave
    // this window, until the next snapshot sends them again.
    if (!m_radioModel.isNull()) {
        if (SpotSourceHost* spotSources = m_radioModel->spotSourceHost()) {
            spotSources->clearStationValues();
        }
        m_radioModel->clearStationRecords();
        // Parity Task 22: the Core's log and bundle wait for the next
        // session.
        m_radioModel->noteStationSupportAvailabilityChanged();
    }
    // R-IOS-13 / R-R3-49: that session's Mod Monitor subscription ended
    // with it; the window's choice of source stays for the next.
    m_modMonitorStream.clear();

    if (!m_settingsProxy.isNull()) {
        m_settingsProxy->setReady(false);
    }
    if (!m_radioModel.isNull()) {
        // Fixed tuner telemetry is an admission snapshot, unlike the
        // retained slice model.  Once a remote session is inactive it must
        // not appear to have a live TGXL.  Keep the endpoint as a reconnect
        // draft, but clear device identity, live state and meters through
        // TunerModel's observational station-state adapter; this never
        // opens a local socket or issues an RF command.
        if (m_radioModel->role() == RadioModel::Role::Remote) {
            // 4O3A state is station-owned admission state, not retained
            // client display state.  Do this before a replacement snapshot
            // can arrive, so a disconnected station is never presented as
            // still listening on this machine.
            m_radioModel->clearRemoteFourO3AState();
            // R-R3-49: likewise the Core's transmit state, so a Core that
            // does not send `transmitting` never inherits "on the air".
            m_radioModel->clearRemoteTransmittingState();
            // R-R3-46 (parity Task 14): an I2C or output pin request still
            // waiting on the Core gets no answer now.
            m_radioModel->failStationIoBoardRequests(
                QStringLiteral("The link to the Core closed before the radio answered."));
            // Parity Task 16: likewise a filter curve request.
            m_radioModel->failStationFilterResponse();
            if (TunerModel* const tuner = m_radioModel->tunerModel()) {
                TunerModel::StationConnectionState disconnected;
                disconnected.configuredHost = tuner->configuredHost();
                disconnected.configuredPort = static_cast<quint16>(
                    qBound(0, tuner->configuredPort(), 65535));
                disconnected.phase = TunerModel::ConnectionPhase::Disconnected;
                tuner->setStationConnectionState(disconnected);
            }
        }
        // The remote accessory controls derive their enabled state from
        // StationClient's negotiated session state.  ConnectionState alone
        // does not change for every close/retry path, so publish this
        // boundary explicitly after the availability predicates are false.
        m_radioModel->reportStationLinkStateChanged();
        m_radioModel->setStationConnectionState(ConnectionState::Disconnected);
        m_radioModel->clearStationFilterState();
        m_radioModel->clearStationBandOutputs();
        m_radioModel->clearStationAlexLpf();
        m_radioModel->clearStationLevelCal();
        for (SliceModel* slice : m_radioModel->slices()) {
            slice->setStationAutoAgcNoiseFloor(slice->stationAutoAgcNoiseFloorDbm(), false,
                                              slice->stationAutoAgcNoiseFloorGeneration());
        }
    }
    emit mediaSessionEnded(m_sessionEpoch);
    emit telemetrySessionEnded(m_sessionEpoch);
    if (reportSessionEnd) {
        emit sessionEnded(reason);
    }

    // Task 19 step 3: automatic reconnect. Only for a closure that WANTS
    // one, and only when there is something to redial -- a
    // startSession()-based session (every non-TLS test in this suite, and
    // the production protocol-seam path) never latches a URL, so
    // scheduleReconnect() is simply never reached for it. See the class
    // comment for why "the daemon spoke with an explicit reason" (version
    // refusal, auth refusal, preemption, peer-limit refusal --
    // attemptReconnect false on all of those call sites) is deliberately
    // NOT retried.
    // A paired race may have no configured URL: its authenticated learned
    // listeners and/or service route still provide something to redial.
    const bool pairedRaceRoute = m_raceMode && m_stationIdentity.size() == 32
        && !m_lastAllowUnpinned && (!m_dialPlan.isEmpty() || !m_serviceRoute.servers.isEmpty());
    if (attemptReconnect && (m_lastUrl.isValid() || pairedRaceRoute)) {
        scheduleReconnect();
    }
    emit connectionActivityChanged();
}

// ── Heartbeat ────────────────────────────────────────────────────────────

void StationClient::setHeartbeatIntervalMs(int ms)
{
    m_heartbeatIntervalMs = ms;
    if (ms <= 0) {
        qCWarning(lcStationClient)
            << "Heartbeat disabled. A station that dies without closing the TCP "
               "connection will not be detected.";
        m_heartbeatTimer->stop();
        return;
    }
    m_heartbeatTimer->setInterval(ms);
    // m_linkUp, not m_transport: see attachTransport() for why the
    // heartbeat waits for the first inbound frame.
    if (m_linkUp) {
        m_heartbeatTimer->start();
    }
}

void StationClient::setMaxMissedPongs(int misses)
{
    m_maxMissedPongs = misses < 1 ? 1 : misses;
}

// ── Handshake deadline (R-R3-16/17) ─────────────────────────────────────

QString StationClient::handshakeDeadlineReason()
{
    return QStringLiteral("The Core did not finish connecting.");
}

void StationClient::setHandshakeDeadlineMs(int ms)
{
    m_handshakeDeadlineMs = ms;
    if (ms < 1) {
        qCWarning(lcStationClient)
            << "Handshake deadline disabled. A station that accepts the connection "
               "but never finishes connecting will be waited on indefinitely.";
    }
}

void StationClient::onHandshakeDeadline()
{
    if (!m_sessionActive || m_handshakeComplete) {
        return;  // stopped too late to matter; nothing is waiting
    }
    qCWarning(lcStationClient) << "Station did not finish connecting within"
                               << m_handshakeDeadlineMs << "ms; closing the link";

    // Recorded as the reason before anything below can race a socket error
    // into m_lastError (first error wins for this attempt, see
    // dialStation()). It is what the link-lost toast and the Core
    // connection status show.
    const QString reason = handshakeDeadlineReason();
    m_lastError = reason;

    // A stalled station is the "no reason from the daemon, just silence"
    // case the heartbeat timeout already treats as retry-eligible, so the
    // next attempt follows the normal backoff. A handshake that stalls
    // every time therefore slows down step by step rather than hammering
    // a Core that is already struggling: nothing here resets the schedule.
    //
    // Hold the transport across the call: disconnectFromStation() ends the
    // session first and then closes the link, and a QWebSocket still in its
    // TLS or upgrade phase does not close on a close() request. Abort it so
    // Core sees the connection go and can retire whatever it built for it.
    const QPointer<SessionTransport> transport(this->transport());
    disconnectFromStation(reason, /*attemptReconnect=*/true);
    if (auto* ws = qobject_cast<WebSocketTransport*>(transport.data())) {
        if (QWebSocket* socket = ws->socket();
            socket != nullptr && socket->state() != QAbstractSocket::ConnectedState) {
            socket->abort();
        }
    }
}

int StationClient::effectiveHeartbeatIntervalMs() const
{
    if (m_heartbeatIntervalMs <= 0) {
        return m_heartbeatIntervalMs;
    }
    const bool relayed = m_pathRank == PathRacer::ServiceRelayed || m_pathRank == PathRacer::Floor
        || m_mediaTunnelInUse;
    return relayed ? std::min(m_heartbeatIntervalMs, kRelayedHeartbeatIntervalMs)
                   : m_heartbeatIntervalMs;
}

void StationClient::setMediaTunnelInUse(bool inUse)
{
    if (m_mediaTunnelInUse == inUse) {
        return; // setInterval restarts a running timer: only on a change
    }
    m_mediaTunnelInUse = inUse;
    if (m_heartbeatIntervalMs > 0) {
        m_heartbeatTimer->setInterval(effectiveHeartbeatIntervalMs());
    }
}

void StationClient::onHeartbeatTick()
{
    if (m_transport == nullptr) {
        m_heartbeatTimer->stop();
        return;
    }
    // Step 2b: the cadence follows the path (a move or a settled pair may
    // have changed it since the last tick).
    m_heartbeatTimer->setInterval(effectiveHeartbeatIntervalMs());
    // Before the snapshot-complete marker a missed pong is not counted. The
    // station sends its whole snapshot in order ahead of any pong, so on a
    // slow link (the web relay under load) the pong can arrive later than
    // kMaxMissedPongs relayed intervals while the snapshot is still coming.
    // Counting it then declared a live link dead and dialled again (a
    // second relay join from each end, tst_relay_session). The handshake
    // deadline already bounds this window: it runs until the marker and
    // ends a connect that stalls. The ping is still sent; only a pong
    // counts once the session is up (StationServer.h, heartbeat section).
    if (!m_handshakeComplete && m_handshakeDeadlineTimer->isActive()) {
        m_transport->ping();
        return;
    }
    if (m_pingsAwaitingPong >= m_maxMissedPongs) {
        qCWarning(lcStationClient)
            << "Station missed" << m_pingsAwaitingPong
            << "consecutive pongs; declaring the link dead";
        emit stationHeartbeatTimeout();
        // Task 19 step 3a: the case that motivated pulling the heartbeat
        // into R2 at all -- a peer that stops responding WITHOUT closing.
        // Retry-eligible: there is no daemon-sent reason here, just
        // silence, the same as a plain transport close.
        disconnectFromStation(QStringLiteral("heartbeat timeout"), /*attemptReconnect=*/true);
        return;
    }
    ++m_pingsAwaitingPong;
    m_transport->ping();
}

// ── Reconnect (Task 19) ──────────────────────────────────────────────────

void StationClient::setReconnectBackoffUnitMs(int ms)
{
    m_reconnectBackoffUnitMs = ms > 0 ? ms : 1;
}

bool StationClient::isReconnectPending() const
{
    return m_reconnectTimer->isActive();
}

namespace {
// scheduleReconnect()'s schedule, in units of m_reconnectBackoffUnitMs; see
// its comment for where the numbers come from. File scope so that
// reconnectBackoffExhausted() reads the same ceiling.
constexpr int kReconnectBackoffSteps[] = { 1, 2, 5, 10, 30, 60 };
constexpr int kReconnectBackoffStepCount =
    static_cast<int>(sizeof(kReconnectBackoffSteps) / sizeof(kReconnectBackoffSteps[0]));
} // namespace

bool StationClient::reconnectBackoffExhausted() const
{
    // m_reconnectAttempts counts the retries scheduled since the schedule
    // last started over; the last step is the ceiling, so once as many
    // retries as there are steps have been scheduled, one of them waited
    // the ceiling.
    return m_reconnectAttempts >= kReconnectBackoffStepCount;
}

void StationClient::scheduleReconnect()
{
    if (m_sessionPurpose == SessionPurpose::RenameOnly) { return; }
    // Same schedule as PgxlConnection.cpp:30's kBackoffSec and
    // TgxlConnection.cpp:30's kTgxlBackoffSec ({1, 2, 5, 10, 30, 60} in
    // both, verified against this tree), reused for consistency with an
    // already-shipped, human-reviewed choice. NOT reused: that class's
    // static QTimer::singleShot mechanism -- see the class comment's
    // link-loss section for why parent design section 13 calls that out
    // by name as the thing not to copy. Scaled by m_reconnectBackoffUnitMs
    // (production default 1000, i.e. real seconds) rather than exposed as
    // a raw ms table, so a test can shrink the whole schedule
    // proportionally with one setter instead of duplicating six numbers.
    const int idx = std::min(m_reconnectAttempts, kReconnectBackoffStepCount - 1);
    const int delayMs = kReconnectBackoffSteps[idx] * m_reconnectBackoffUnitMs;
    ++m_reconnectAttempts;

    qCInfo(lcStationClient) << "Scheduling reconnect attempt" << m_reconnectAttempts
                            << "in" << delayMs << "ms";
    // Fix round 1, Minor 8: start BEFORE emitting. A directly-connected
    // slot that reacts to reconnectScheduled() by cancelling (e.g. an
    // operator's own "stop retrying" control) must see an ALREADY-ARMED
    // timer to cancel; emitting first would let such a slot's
    // isReconnectPending() read false and its own stop() call be
    // overridden a moment later by the start() below.
    m_reconnectTimer->start(delayMs);
    emit reconnectScheduled(m_reconnectAttempts, delayMs);
}

void StationClient::onReconnectTimeout()
{
    // Task 29: a paired Core is raced again.
    if (m_raceMode && m_stationIdentity.size() == 32 && !m_lastAllowUnpinned
        && (!m_dialPlan.isEmpty() || !m_serviceRoute.servers.isEmpty())) {
        startRace();
        return;
    }
    // Task 28: a session through the service is retried through it.
    if (!m_serviceServers.isEmpty()) {
        startDialPlan();
        dialThroughService();
        return;
    }
    if (!m_lastUrl.isValid()) {
        // Defensive: disconnectFromStation() and attachTransport() both
        // stop this timer unconditionally, so a fired-with-nothing-to-
        // redial timeout should be unreachable. Not treated as a bug if
        // it somehow happens -- just nothing to do.
        emit connectionActivityChanged();
        return;
    }
    // iPhone app plan Task 27: every retry runs the same addresses again,
    // the Core's last good one first.
    if (!m_dialPlan.isEmpty()) {
        startDialPlan();
        dialStation(m_dialPlan.first(), m_planToken, m_lastFingerprint, m_lastAllowUnpinned,
                    m_stationIdentity);
        return;
    }
    dialStation(m_lastUrl, m_token, m_lastFingerprint, m_lastAllowUnpinned, m_stationIdentity);
}

// ── Inbound dispatch ─────────────────────────────────────────────────────

void StationClient::onTransportText(const QByteArray& wire)
{
    // First frame from the station is proof the link carries traffic in
    // both directions, which is the point at which a missed-pong count
    // starts meaning something. See attachTransport().
    if (!m_linkUp) {
        m_linkUp = true;
        if (m_heartbeatIntervalMs > 0 && m_sessionActive) {
            m_heartbeatTimer->setInterval(effectiveHeartbeatIntervalMs());
            m_heartbeatTimer->start();
        }
    }

    SessionMessage message;
    if (!SessionMessages::decode(wire, &message)) {
        if (m_settingsBackupExport || m_cancelledSettingsBackupBegin) {
            // Decode failure may concern this operation, but arbitrary text
            // mentioning its verb is not a message identity. Inspect only
            // the actual envelope fields, independent of JSON key ordering.
            const QJsonObject envelope = QJsonDocument::fromJson(wire).object();
            const QJsonValue id = envelope.value(QStringLiteral("id"));
            const quint32 expected = m_settingsBackupExport
                ? m_settingsBackupExport->expectedCommandId
                : m_cancelledSettingsBackupBegin->first;
            if (envelope.value(QStringLiteral("type")) == QJsonValue(QStringLiteral("command.result"))
                && id.isDouble() && id.toDouble() == static_cast<double>(expected)) {
                m_cancelledSettingsBackupBegin.reset();
                finishSettingsBackupExport(false, QStringLiteral("Malformed settings export reply."), {}, true);
                return;
            }
        }
        qCWarning(lcStationClient) << "Undecodable message from station; ignoring";
        return;
    }
    if (message.kind == SessionMessageKind::CommandResult
        && ((m_settingsBackupExport
             && message.commandId == m_settingsBackupExport->expectedCommandId)
            || (m_cancelledSettingsBackupBegin
                && message.commandId == m_cancelledSettingsBackupBegin->first))
        && !SettingsBackupExportWire::strictEnvelope(wire, true)) {
        m_cancelledSettingsBackupBegin.reset();
        finishSettingsBackupExport(false, QStringLiteral("Malformed settings export reply."), {}, true);
        return;
    }

    switch (message.kind) {
    case SessionMessageKind::StationTelemetry:
        if (telemetryAvailable()
            && message.telemetry.sequence > m_lastTelemetrySequence
            && message.telemetry.sampledElapsedMs >= m_lastTelemetrySampleElapsedMs) {
            m_lastTelemetrySequence = message.telemetry.sequence;
            m_lastTelemetrySampleElapsedMs = message.telemetry.sampledElapsedMs;
            // Only a Core that negotiated host telemetry may supply it.
            if (m_agreedMinor < kCoreHostTelemetrySessionProtocolMinor
                || m_capabilities.stationTelemetryVersion < 2) {
                message.telemetry.host = {};
            }
            // Only a Core that negotiated receiver load may supply it.
            if (m_agreedMinor < kReceiverLoadSessionProtocolMinor
                || m_capabilities.stationTelemetryVersion < 3) {
                message.telemetry.receivers.reset();
            }
            // R-R3-32 (parity Task 6): and the radio's PA readings and link
            // quality only from one that negotiated version 4.
            if (m_agreedMinor < kReceiverLoadSessionProtocolMinor
                || m_capabilities.stationTelemetryVersion < 4) {
                message.telemetry.radio.clearRadioStatus();
            }
            // R-R3-32 (parity Task 14): and the HL2 link only from one that
            // negotiated version 5.
            if (m_agreedMinor < kReceiverLoadSessionProtocolMinor
                || m_capabilities.stationTelemetryVersion < 5) {
                message.telemetry.radio.clearHl2Link();
            }
            if (m_agreedMinor < kReceiverLoadSessionProtocolMinor
                || m_capabilities.stationTelemetryVersion < 6) {
                message.telemetry.radio.clearRadioDiagnostics();
            }
            emit telemetryReceived(message.telemetry, m_sessionEpoch);
        }
        break;
    case SessionMessageKind::MediaControl:
        if (mediaAvailable()) {
            emit mediaControlReceived(message.mediaPayload, m_sessionEpoch);
        }
        break;
    case SessionMessageKind::Hello:
        handleHello(message);
        break;
    case SessionMessageKind::AuthResult:
        handleAuthResult(message);
        break;
    case SessionMessageKind::Capabilities:
        handleCapabilities(message);
        break;
    case SessionMessageKind::SettingsSnapshot:
        handleSettingsSnapshot(message);
        break;
    case SessionMessageKind::Schema:
        handleSchema(message);
        break;
    case SessionMessageKind::ObjectCreate:
        handleObjectCreate(message);
        break;
    case SessionMessageKind::ObjectDestroy:
        handleObjectDestroy(message);
        break;
    case SessionMessageKind::Delta:
        handleDelta(message);
        break;
    case SessionMessageKind::PropertyResult:
        if (propertyResultsAvailable()) {
            handlePropertyResult(message);
        }
        break;
    case SessionMessageKind::SnapshotComplete: {
        const bool firstSnapshot = !m_handshakeComplete;
        // BEFORE anything below, and in particular before
        // m_forwardLocalChanges goes true: this marker is the FIRST moment
        // the station's full object set is known, and it is the only
        // moment at which "the station did not name this slice" means
        // "the station does not have this slice". See
        // reconcileSlicesAgainstStation().
        reconcileSlicesAgainstStation();
        m_handshakeComplete = true;
        // R-R3-16/17: the connect sequence finished inside its deadline.
        m_handshakeDeadlineTimer->stop();
        // The heartbeat counts missed pongs from here (onHeartbeatTick);
        // pings sent during the snapshot are not held against the link.
        if (firstSnapshot) {
            m_pingsAwaitingPong = 0;
        }
        if (m_radioModel) {
            // R-R3-49 (parity Task 7): and arming off the air from a Core
            // at transmitSettingsVersion 7.
            m_radioModel->pureSignalFacade()->setRemoteCapabilities(
                m_agreedMinor >= kDspControlSessionProtocolMinor && m_capabilities.psAlgorithmVersion == 3,
                m_capabilities.txPermitted, pureSignalArmingOffered());
            m_radioModel->dspAssets()->setRemoteNr3ModelsSupported(remoteNr3ModelsAvailable());
        }
        if (m_radioModel) {
            m_radioModel->setStationFilterSnapshotReady();
            m_radioModel->reportStationLinkStateChanged();
        }
        // Task 19: this is a PROVEN success, the moment isStale() (once it
        // has ever been true) goes false again, and the only place that
        // resets the reconnect backoff on the strength of an actually
        // working session rather than merely a deliberate new attempt
        // (connectToStation() resets it too, but for a DIFFERENT reason --
        // see its own comment). Without this reset, a session that
        // survived for hours after a rocky initial connect would have its
        // NEXT, unrelated drop start retrying at whatever the ORIGINAL
        // struggle's backoff had climbed to, possibly the 60 s ceiling,
        // rather than at the first, fast step.
        //
        // R-R3-28: when media was negotiated, the handshake alone no longer
        // proves the session works. Media that fails after every good
        // handshake would otherwise retry at the first step forever (the
        // "Explicit remaining boundary" in the R3 media-recovery evidence).
        // The reset then waits for noteMediaEstablished(); without media it
        // happens here, as it always has.
        m_everConnected = true;
        if (!mediaAvailable()) {
            m_reconnectAttempts = 0;
        }
        // Only now: everything that moved before this point was the
        // station's own burst landing, and forwarding any of it would tell
        // the station its own state back.
        m_forwardLocalChanges = m_sessionPurpose == SessionPurpose::Ordinary;
        if (m_forwardLocalChanges) { m_writeFlushTimer->start(); }
        refreshRemoteTransmit();
        // Parity Task 19 (R-IOS-25): the Core's spots and its spot sources'
        // console lines, each backlog first. Each (re)connect starts from
        // the Core's newest records.
        if (m_sessionPurpose == SessionPurpose::Ordinary && spotSourcesAvailable()) {
            if (!m_radioModel.isNull()) {
                // Fix wave, I3: the spots only; the radio list's own
                // subscription replaces it.
                m_radioModel->clearStationSpots();
            }
            const auto subscribe = [this](const QString& stream, int backlog) {
                invokeCommand("records.subscribe",
                              {MirrorUpdate{0, "stream", MirrorWireKind::Utf8, QVariant(stream)},
                               MirrorUpdate{0, "backlog", MirrorWireKind::Int64,
                                            QVariant(static_cast<qlonglong>(backlog))}});
            };
            subscribe(QStringLiteral("spots"), 500);
            for (const QString& source : SpotSourceHost::recordStreamSources()) {
                subscribe(SpotSourceHost::consoleStream(source), 200);
            }
            // iPhone plan Task 22 / parity Task 20 (stationFreedvVersion
            // 1): the Core's FreeDV Reporter list and console.
            if (stationFreedvAvailable()) {
                if (!m_radioModel.isNull()) {
                    m_radioModel->clearStationFreedv();
                }
                subscribe(SpotSourceHost::consoleStream(SpotSourceHost::kFreedvReporter), 200);
                subscribe(QStringLiteral("freedvStations"), 1000);
            }
            // Parity Task 21 (R-IOS-18): the Core's radios, for This Core.
            if (stationRadiosAvailable()) {
                subscribe(QStringLiteral("stationRadios"), 64);
            }
            // Parity Task 23 (stationTciVersion 2): the apps on the Core's
            // TCI server, for the TCI applets, page and log.
            if (stationTciServerAvailable()) {
                subscribe(QStringLiteral("tciClients"), StationTciModel::kClientsCapacity);
            }
        }
        // Parity Task 22 (R-R3-49): the Core's log follows again for the
        // viewers that hold it, and the support controls learn the session.
        if (!m_radioModel.isNull()) {
            m_radioModel->noteStationSupportAvailabilityChanged();
        }
        // Parity Task 33: the CFC bar chart shown across a reconnect.
        if (m_cfcCompressionWanted && txReadingsAvailable()) {
            sendCfcCompressionSubscription(true);
        }
        // iPhone app plan Task 25: the VAX meters shown across a reconnect.
        if (m_stationVaxLevelsWanted && stationVaxAvailable()) {
            sendStationVaxLevelsSubscription(true);
        }
        // R-IOS-13 / R-R3-49: the Mod Monitor's stream, when the window
        // shows it; a new snapshot is a new subscription.
        m_modMonitorStream.clear();
        if (!m_radioModel.isNull()) {
            m_radioModel->clearStationModMonitor();
        }
        syncModMonitorSubscription();
        if (firstSnapshot) {
            qCInfo(lcStationClient) << "Session established with" << m_capabilities.stationName;
            // iPhone app plan Task 27: where the Core was reached, tried
            // first next time.
            m_openTimer->stop();
            if (m_raceMode) {
                // Task 29: where the race reached the Core, a direct
                // address, is the one to try first next time; a session
                // through the service leaves the list as it is.
                if (m_raceWinnerUrl.isValid()) {
                    m_connectedUrl = m_raceWinnerUrl;
                    m_cachedAddresses.removeAll(m_raceWinnerUrl);
                    m_cachedAddresses.prepend(m_raceWinnerUrl);
                }
            } else if (m_lastUrl.isValid()) {
                m_connectedUrl = m_lastUrl;
                m_cachedAddresses.removeAll(m_lastUrl);
                m_cachedAddresses.prepend(m_lastUrl);
            }
            recordOutcome(StationConnectionAttempt::Outcome::Connected);
            m_radioChangeReason.clear();
            // Task 29 (link section 21.1): the winner reached
            // snapshot.complete; every other rung stops, and one better
            // than the winner, if ready, is moved to at once (21.3).
            std::optional<PathRacer::Ready> standby;
            if (m_racer) {
                standby = m_racer->takeStandby();
                m_racer->finish();
                syncAttemptFromRace(m_racer);
                m_racer->disconnect(this);
                m_racer->deleteLater();
                m_racer = nullptr;
            }
            // Review Minor 7: the rank from the pair the connection
            // settled on; a standby no better than that is let go.
            refreshPathRank();
            if (standby && standby->rank >= m_pathRank) {
                if (standby->transport) {
                    standby->transport->closeLink(QStringLiteral("not needed"));
                    standby->transport->deleteLater();
                }
                standby.reset();
            }
            m_upgradeAttempt = 0;
            const QPointer<StationClient> self(this);
            emit handshakeComplete();
            if (!self) {
                return;
            }
            if (standby && standby->transport) {
                beginUpgrade(*standby);
            } else {
                scheduleUpgrade(/*advance=*/false);
            }
        }
        // iPhone app plan Task 78: sharing the Core as a device is known
        // once the handshake completes.
        if (!m_radioModel.isNull()) {
            m_radioModel->setStationMayCloseLastSlice(sessionHolderAvailable());
        }
        m_remoteDevices->setSelfDeviceId(thisDeviceWireId());
        // Slice control plan Task 5: every slice the snapshot named is
        // marked from the access objects it sent (none: not read-only).
        m_sliceAccess->setSelfDeviceId(thisDeviceWireId());
        m_sliceAccess->setCoreSliceTakeable(coreSliceTakeAvailable());
        m_sliceAccess->refreshSlices();
        emit transmitTakeAvailabilityChanged();
        refreshSettingsHygiene();
        const QPointer<StationClient> watchSelf(this);
        emit stateSnapshotApplied();
        if (watchSelf) {
            watchSelf->requestWatchAttempt();
        }
        break;
    }
    case SessionMessageKind::CommandResult:
        handleCommandResult(message);
        break;
    // Parity Task 19 (R-IOS-25): a stream this window subscribed to.
    case SessionMessageKind::RecordBatch:
        // iPhone app plan Task 25: the Core computer's VAX meters.
        if (message.recordBatch.stream == QLatin1String(StationVax::kLevelsStream)) {
            for (const RecordUpsert& u : message.recordBatch.upserts) {
                double rx[StationVax::kChannels] = {};
                for (int i = 0; i < StationVax::kChannels; ++i) {
                    rx[i] = u.fields.value(QStringLiteral("ch%1Level").arg(i + 1)).toDouble();
                }
                m_stationVax->setStationLevels(
                    rx, u.fields.value(QStringLiteral("txLevel")).toDouble());
            }
            break;
        }
        // Parity Task 33: the Core's CFC display, its one record.
        if (message.recordBatch.stream == QLatin1String(TransmitState::kCfcStream)) {
            for (const RecordUpsert& u : message.recordBatch.upserts) {
                const QList<double> bins =
                    TransmitState::decodeCfcBins(u.fields.value(QStringLiteral("binsDbTenths")).toString());
                if (!bins.isEmpty()) {
                    emit cfcCompressionReceived(
                        bins, static_cast<qint64>(u.fields.value(QStringLiteral("atMs")).toDouble()));
                }
            }
            break;
        }
        if ((spotSourcesAvailable() || stationRadiosAvailable() || stationTciServerAvailable() || txModMonitorAvailable())
            && !m_radioModel.isNull()) {
            m_radioModel->applyStationRecordBatch(message.recordBatch);
        }
        break;
    case SessionMessageKind::SettingsValue:
        handleSettingsValue(message);
        break;
    // iPhone app plan Task 78 item 7 (G-53; the link document, section 5.1
    // step 4): the Core is full and asks which device this window takes
    // the place of. Sent only to a window that declared sessionHolder 1
    // and deviceAuth 1. The Core pauses its connect deadline while it
    // asks, and so does this window.
    case SessionMessageKind::SessionHeld:
        if (m_declaredSessionHolder) {
            m_handshakeDeadlineTimer->stop();
            m_remoteDevices->setHeld(RemoteDevicesState::parseHeld(
                message.heldDevices, message.heldRevision, message.placeTaken,
                message.placeFreed, m_takeBackDeviceId));
        }
        break;
    // iPhone app plan Task 78: the Core's question and its notices, sent
    // only to a device with sessionHolderVersion 1.
    case SessionMessageKind::ConfirmRequest:
        if (m_declaredSessionHolder) {
            m_remoteDevices->setQuestion(message.prompt, message.reason);
        }
        break;
    case SessionMessageKind::Notice:
        // Tune-ended lane: the Core ended this window's tuner tune before
        // its carrier keyed; nothing else tells the transmit client.
        if (message.prompt.kind == QLatin1String("tuneEnded")) {
            m_remoteTransmit->tunerTuneEnded();
        }
        if (m_declaredSessionHolder) {
            m_remoteDevices->addNotice(message.prompt, message.reason);
        }
        break;
    case SessionMessageKind::SettingsReject:
        handleSettingsReject(message);
        break;
    case SessionMessageKind::SessionEnd:
        qCWarning(lcStationClient) << "Station ended the session:" << message.reason
                                   << (message.retryable ? "(retryable)" : "(permanent)");
        m_lastError = message.reason;
        // R-R3-38: an end that will not fix itself is what the window shows
        // and offers buttons for; a retryable one retries as before.
        if (!message.retryable) {
            m_lastEndReport = stationEndReport(message.reason, message.endCode);
            // Task 78 item 3 (G-53): who took this window's place, and when.
            if (m_lastEndReport.kind == StationEndReport::Kind::TakenOver) {
                m_lastEndReport.takenOverByName = message.takenOverBy;
                m_lastEndReport.takenOverById = message.takenOverById;
                if (message.secondsAgo) {
                    m_lastEndReport.endedAt =
                        QDateTime::currentDateTime().addSecs(-std::max<qint64>(0, *message.secondsAgo));
                }
                m_takeBackDeviceId = message.takenOverById;
            }
        }
        // The operator's ruling of 2026-09-26: the Core changes its radio by
        // restarting its run. This end is a reconnect, not a failure; the
        // window says so until it is back (radioChangeReason).
        if (message.retryable
            && message.endCode == QLatin1String(SessionEndCode::kRadioChanging)) {
            m_radioChangeReason = message.reason;
        } else {
            // Follow-up N2: any other end from the Core is shown as itself.
            m_radioChangeReason.clear();
        }
        // The station's own classification, not this end's guess at one
        // and not a match against its English prose. Every station-sent
        // refusal used to take disconnectFromStation()'s default of false,
        // so "Station is at its concurrent-connection limit" -- a cap the
        // header explicitly sizes to be hit BY a reconnecting client --
        // permanently disarmed automatic reconnect. See
        // SessionMessage::retryable and the classification argued at each
        // StationServer call site.
        disconnectFromStation(message.reason, message.retryable);
        break;
    default:
        qCWarning(lcStationClient) << "Ignoring station message of client-only kind:"
                                   << SessionMessages::kindName(message.kind);
        break;
    }
}

void StationClient::handleHello(const SessionMessage& message)
{
    // Parent design section 7.0's version policy, applied from this side
    // too rather than trusting the station to have applied it: a station
    // several majors ahead may not even recognise this client's Hello.
    //
    // iPhone app spec D23 and D39 (R-IOS-01): the highest major both ends
    // support. An older station's hello has no `majors`, which decodes as
    // [its major]. Sharing none (two or more apart), this client leaves
    // without sending its hello or its token, without retrying, and with
    // the station's own wording.
    const std::optional<quint16> agreed =
        LinkVersion::agreeMajor(m_supportedMajors, message.supportedMajors);
    if (!agreed) {
        m_lastError =
            SessionEndReasons::versionRefused(message.supportedMajors, m_supportedMajors);
        qCWarning(lcStationClient) << "No link major shared with the station (it supports"
                                   << message.supportedMajors << "; this client supports"
                                   << m_supportedMajors << "):" << m_lastError;
        // R-R3-38: this app refused the Core, for good, for the same
        // reason the Core would have refused it: the same end as the
        // Core's refusal, so the window shows the same version notice
        // whichever side finds the mismatch.
        m_lastEndReport = stationEndReport(
            m_lastError, QString::fromLatin1(SessionEndCode::kLinkVersion));
        disconnectFromStation(m_lastError);
        return;
    }
    m_agreedMajor = *agreed;
    m_stationFeatures = message.features;

    m_agreedMinor = std::min(kSessionProtocolMinor, message.protocolMinor);

    // Schema version skew, caught BY NAME at handshake -- see
    // readLocalSettingsSchemaVersion() for what "by name" means. Reported
    // rather than refused: this version governs the shape of each side's
    // OWN local settings file, not the wire contract, and section 7.0's
    // refusal rule is about the protocol major alone.
    m_stationSettingsSchema = message.settingsSchemaVersion;
    m_settingsSchemaSkew = m_stationSettingsSchema != m_localSettingsSchema;
    if (m_settingsSchemaSkew) {
        qCWarning(lcStationClient)
            << "Settings schema skew: this client is at" << m_localSettingsSchema
            << "the station is at" << m_stationSettingsSchema
            << "-- station settings may not round-trip as expected";
    }

    // THE GATE. This is the one line on which the pre-shared token leaves
    // this process, so this is where the pin has to have been checked --
    // not in whichever handler happened to fire, and not only on a
    // handshake that reported errors. ensurePinSatisfied() has normally
    // already latched by now (connected() precedes any inbound frame), so
    // in the ordinary case this costs one bool test; when it has not, it
    // does the comparison here rather than letting the secret out.
    //
    // iPhone app Task 18: a paired Core is trusted by its identity key
    // instead of the pin; signIn() checks it before anything is sent.
    if (m_stationIdentity.isEmpty() && !ensurePinSatisfied()) {
        return;
    }

    signIn(message);
}

void StationClient::setDeviceIdentity(std::shared_ptr<const ClientDeviceIdentity> identity,
                                      const QString& deviceName, const QString& shortName)
{
    if (m_deviceIdentity != identity && m_sessionActive) {
        const QPointer<StationClient> watchSelf(this);
        retireDirectWatch();
        if (!watchSelf) { return; }
        m_directWatchDeclared = false;
        m_watchRelayDeclared = false;
    }
    m_deviceIdentity = std::move(identity);
    m_deviceName = deviceName;
    m_deviceShortName = shortName;
    // A client that cannot sign in by key does not say it can.
    if (m_deviceIdentity && m_deviceIdentity->isValid()) {
        m_declaredFeatures.insert(QByteArrayLiteral("deviceAuth"), 1);
    } else {
        m_declaredFeatures.remove(QByteArrayLiteral("deviceAuth"));
        if (m_deviceIdentity) {
            qCWarning(lcStationClient) << "This computer's device key is unavailable:"
                                       << m_deviceIdentity->lastError();
        }
    }
}

void StationClient::refuseStation(const QString& reason, StationEndReport::Kind kind,
                                  const QString& code)
{
    // iPhone app plan Task 27: another computer at a cached address (its
    // address given to something else since) gives way to the next address
    // this attempt has, before anything was sent.
    if (kind == StationEndReport::Kind::IdentityChanged
        && advanceDialPlan(StationConnectionAttempt::Outcome::NotThisCore)) {
        qCInfo(lcStationClient) << "Another computer answered at a saved address of the Core";
        return;
    }
    recordOutcome(kind == StationEndReport::Kind::IdentityChanged
                      ? StationConnectionAttempt::Outcome::NotThisCore
                      : StationConnectionAttempt::Outcome::Failed);
    m_lastError = reason;
    qCWarning(lcStationClient) << "Not signing in to the Core:" << reason;
    // Recorded before the end is reported, so the window reads it.
    StationEndReport report;
    report.kind = kind;
    report.reason = reason;
    report.code = code;
    m_lastEndReport = report;
    disconnectFromStation(reason);
}

bool StationClient::verifyStationIdentity(const SessionMessage& hello,
                                          const QByteArray& expected,
                                          QByteArray* stationSpki, QByteArray* certSha256)
{
    const QString identityCode = QString::fromLatin1(SessionEndCode::kIdentityChanged);
    const QByteArray spki = stationSpkiOf(hello);
    // A Core that shows no identity, or another one, is not the Core this
    // computer paired with: never trusted silently (the link document,
    // section 12.4, identityChanged).
    if (spki.isEmpty() || StationIdentity::fingerprintOf(spki) != expected) {
        refuseStation(QStringLiteral("The Core at this address is not the Core this computer "
                                     "paired with, so this computer did not connect. If the "
                                     "Core was set up again, forget it here and pair again."),
                      StationEndReport::Kind::IdentityChanged, identityCode);
        return false;
    }
    const QByteArray certificate =
        m_transport != nullptr ? m_transport->peerCertificateSha256() : QByteArray();
    if (!bindingHolds(spki, hello.stationIdentity->certBinding, certificate)) {
        refuseStation(QStringLiteral("The Core's certificate is not signed by the Core this "
                                     "computer paired with, so this computer did not connect."),
                      StationEndReport::Kind::IdentityChanged, identityCode);
        return false;
    }
    *stationSpki = spki;
    *certSha256 = certificate;
    return true;
}

bool StationClient::signIn(const SessionMessage& hello)
{
    if (m_sessionPurpose == SessionPurpose::RenameOnly && m_stationIdentity.size() != 32) {
        refuseStation(QStringLiteral("Rename requires an already paired Core identity."),
                      StationEndReport::Kind::Refused, QString());
        return false;
    }
    const QPointer<StationClient> guardedSelf(this);
    const quint32 guardedEpoch = m_sessionEpoch;
    const auto admissionGuard = m_admissionGuard;
    if (admissionGuard && !admissionGuard()) {
        if (guardedSelf && m_sessionEpoch == guardedEpoch) {
            refuseStation(QStringLiteral("The rename session is no longer permitted."),
                          StationEndReport::Kind::Refused, QString());
        }
        return false;
    }
    if (!guardedSelf || m_sessionEpoch != guardedEpoch) { return false; }
    const bool keyUsable = m_deviceIdentity && m_deviceIdentity->isValid();
    bool challengeOk = false;
    const QByteArray challenge = StationIdentity::fromBase64Url(hello.challenge, &challengeOk);
    const bool challengeUsable =
        challengeOk && challenge.size() == DeviceAuthenticator::kChallengeBytes;

    if (!m_stationIdentity.isEmpty()) {
        // A paired Core: its key and certificate binding, then this
        // computer's own key. The token is never sent to it.
        QByteArray stationSpki;
        QByteArray certificate;
        if (!verifyStationIdentity(hello, m_stationIdentity, &stationSpki, &certificate)) {
            return false;
        }
        // iPhone app plan Task 29 (link section 21.1): the Core's
        // rendezvous id, which a window saves to reach it through the
        // service.
        m_stationRendezvousId = RendezvousWire::rendezvousId(stationSpki);
        if (!keyUsable) {
            refuseStation(QStringLiteral("This computer's own key could not be read, so it "
                                         "cannot sign in to the Core it paired with."),
                          StationEndReport::Kind::Refused, QString());
            return false;
        }
        if (!challengeUsable || !stationDeclares(QByteArrayLiteral("deviceAuth"), 1)) {
            refuseStation(QStringLiteral("The Core at this address did not offer the sign-in "
                                         "this computer paired for, so this computer did not "
                                         "connect."),
                          StationEndReport::Kind::IdentityChanged,
                          QString::fromLatin1(SessionEndCode::kIdentityChanged));
            return false;
        }
        // iPhone app plan Task 78 (the several-devices design, ruling
        // 10.1): signed in with its own key, this window knows its device
        // id as the Core sends it, so it shares the Core as a device: it
        // is asked before a change reaches another device, told what
        // another device did, and may take transmit.
        QHash<QByteArray, int> features = m_declaredFeatures;
        const auto* relayPrimary = qobject_cast<const DataChannelTransport*>(transport());
        m_watchRelayDeclared = m_sessionPurpose == SessionPurpose::Ordinary && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
            && certificate.size() == 32 && relayPrimary != nullptr
            && relayPrimary->canOpenWatchRelay();
        m_directWatchDeclared = m_sessionPurpose == SessionPurpose::Ordinary && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
            && certificate.size() == 32
            && (qobject_cast<WebSocketTransport*>(transport()) != nullptr
                || m_watchRelayDeclared);
        if (m_directWatchDeclared) {
            features.insert(QByteArrayLiteral("txWatchPath"), 1);
        }
        if (m_watchRelayDeclared) {
            features.insert(QByteArrayLiteral("txWatchRelay"), 1);
        }
        if (m_declaresSessionHolder) {
            features.insert(QByteArrayLiteral("sessionHolder"), 1);
            // Slice control plan Task 4: listening to and taking another
            // device's slice (the rest of the window's side is Task 5).
            // Take-over parity: 2, Take it back on controlTaken.
            if (m_sessionPurpose == SessionPurpose::Ordinary) {
                features.insert(QByteArrayLiteral("sliceAccess"), m_sliceAccessDeclared);
            }
        }
        m_declaredSessionHolder = m_declaresSessionHolder;
        send(SessionMessages::hello(m_agreedMajor, kSessionProtocolMinor, m_localSettingsSchema,
                                    peerNameForThisProcess(), m_supportedMajors, features));
        // Hello delivery can synchronously retire a temporary rename's authority.
        // Check again at the admission boundary, before any auth.request goes out.
        if (!guardedSelf || m_sessionEpoch != guardedEpoch) { return false; }
        if (m_sessionPurpose == SessionPurpose::RenameOnly && admissionGuard && !admissionGuard()) {
            if (guardedSelf && m_sessionEpoch == guardedEpoch) {
                refuseStation(QStringLiteral("The rename session is no longer permitted."),
                              StationEndReport::Kind::Refused, QString());
            }
            return false;
        }
        if (!guardedSelf || m_sessionEpoch != guardedEpoch) { return false; }
        send(SessionMessages::authRequest(
            QString(), deviceBlockFor(*m_deviceIdentity, m_deviceName, m_deviceShortName,
                                      challenge, certificate, stationSpki)));
        m_signedInWithDeviceKey = true;
        m_enrolledDeviceKey = false;
        return true;
    }

    // A Core trusted by its pin. When it has an identity and the pin was
    // actually checked, this computer's key is enrolled with the token in
    // the same sign-in (the link document, section 3.5), and the Core is
    // trusted by that identity from then on. A Core with no identity, a
    // bench link with no pin, or a certificate the identity does not bind
    // gets the token alone, as before.
    SessionDeviceBlock block;
    m_enrollingIdentity.clear();
    if (keyUsable && !m_token.isEmpty() && m_pinRequired && m_pinSatisfied && challengeUsable
        && stationDeclares(QByteArrayLiteral("deviceAuth"), 1)) {
        const QByteArray stationSpki = stationSpkiOf(hello);
        const QByteArray certificate =
            m_transport != nullptr ? m_transport->peerCertificateSha256() : QByteArray();
        if (!stationSpki.isEmpty()
            && bindingHolds(stationSpki, hello.stationIdentity->certBinding, certificate)) {
            block = deviceBlockFor(*m_deviceIdentity, m_deviceName, m_deviceShortName,
                                   challenge, certificate, stationSpki);
            m_enrollingIdentity = StationIdentity::fingerprintOf(stationSpki);
        } else {
            qCWarning(lcStationClient) << "The Core's identity does not match its certificate;"
                                       << "signing in with the token alone.";
        }
    }
    // A token sign-in does not know the id the Core numbers it by, so it
    // does not share the Core as a device (Task 78). A window test's
    // bench link says it does (setTokenSessionHolderForTest).
    QHash<QByteArray, int> features = m_declaredFeatures;
    m_directWatchDeclared = false;
    m_watchRelayDeclared = false;
    m_declaredSessionHolder = false;
    if (!m_tokenSessionHolderIdForTest.isEmpty() && m_declaresSessionHolder
        && features.contains(QByteArrayLiteral("deviceAuth"))) {
        features.insert(QByteArrayLiteral("sessionHolder"), 1);
        if (m_tokenSliceAccessForTest) {
            features.insert(QByteArrayLiteral("sliceAccess"), m_sliceAccessDeclared);
        }
        m_declaredSessionHolder = true;
    }
    send(SessionMessages::hello(m_agreedMajor, kSessionProtocolMinor, m_localSettingsSchema,
                                peerNameForThisProcess(), m_supportedMajors, features));
    send(m_enrollingIdentity.isEmpty() ? SessionMessages::authRequest(m_token)
                                       : SessionMessages::authRequest(m_token, block));
    // The pairing token, even when this sign-in enrols the key: the Core
    // counts this session as a token sign-in (fix wave, I5).
    m_signedInWithDeviceKey = false;
    m_enrolledDeviceKey = false;
    return true;
}

bool StationClient::stationDeclares(const QByteArray& feature, int minVersion) const
{
    if (!m_stationFeatures.contains(feature)) {
        return false;
    }
    return m_stationFeatures.value(feature) >= minVersion;
}

void StationClient::handleAuthResult(const SessionMessage& message)
{
    if (!message.accepted) {
        m_lastError = message.reason;
        m_enrollingIdentity.clear();
        // Follow-up N2: the Core answered the redial with a refusal; that is
        // the reading now, not the radio change.
        m_radioChangeReason.clear();
        qCWarning(lcStationClient) << "Station refused authentication:" << message.reason
                                   << message.endCode;
        // iPhone app Task 18: a refusal that will not fix itself (a removed
        // or unpaired device, a retired token) is what the window shows,
        // chosen by its code.
        if (!message.retryable) {
            m_lastEndReport = stationEndReport(message.reason, message.endCode);
        }
        // A WRONG TOKEN stays permanent; a RATE-LIMITED refusal does not.
        // The station's rate limiter is global rather than per-peer, so
        // somebody else's five bad guesses inside 60 s refuse the operator
        // too (TokenStore.h:44-48). Treated as permanent, that was a
        // stranger being able to lock the operator out of their own
        // station until they noticed and reconnected by hand. Retrying is
        // safe here precisely BECAUSE the wrong-token case is not retried:
        // this client only comes back when the station said the condition
        // was temporary.
        disconnectFromStation(message.reason, message.retryable);
        return;
    }
    m_authenticated = true;
    // iPhone app Task 18: the token sign-in enrolled this computer's key.
    // The Core is trusted by its identity from now on, a redial included.
    if (!m_enrollingIdentity.isEmpty()) {
        const QByteArray learned = m_enrollingIdentity;
        m_enrollingIdentity.clear();
        m_stationIdentity = learned;
        // Follow-up N1: still a token session, but this computer is paired;
        // the window says to reconnect rather than to find a paired device.
        m_enrolledDeviceKey = true;
        qCInfo(lcStationClient) << "This computer's key is paired with the Core; it signs in"
                                << "by key from now on.";
        emit stationIdentityLearned(learned);
    }
}

void StationClient::handleCapabilities(const SessionMessage& message)
{
    // Task 78 item 7: capabilities means the Core let this window in; a
    // fifth-device question, if one was asked, is over.
    m_remoteDevices->clearHeld();
    m_takeBackDeviceId.clear();
    const QPointer<StationClient> self(this);
    const quint32 epoch = m_sessionEpoch;
    const auto previousBudget = remoteDisplayBudgetLimits();
    const bool previousPs3 = remotePs3DisplaySubscribed();
    const DisplayBudgetReason previousReason = remoteDisplayBudgetReason();
    StationCapabilities incoming = StationCapabilities::fromUpdates(message.updates);
    // A complete descriptor is authoritative for this authenticated epoch.
    // Malformed, partial or stale updates cannot turn a known cap into the
    // legacy fallback. A fresh attach clears it before accepting a new peer.
    if (m_capabilities.displayBudget) {
        bool retain = !incoming.displayBudget;
        if (incoming.displayBudget) {
            const quint32 delta = incoming.displayBudget->generation
                - m_capabilities.displayBudget->generation;
            retain = delta >= 0x80000000u
                || (delta == 0 && *incoming.displayBudget != *m_capabilities.displayBudget);
        }
        if (retain) {
            incoming.remoteDisplayBudgetVersion = m_capabilities.remoteDisplayBudgetVersion;
            incoming.displayBudget = m_capabilities.displayBudget;
            incoming.remotePs3DisplaySubscribed = m_capabilities.remotePs3DisplaySubscribed;
            incoming.displayBudgetReason = m_capabilities.displayBudgetReason;
        }
    }
    m_capabilities = incoming;
    if (m_settingsBackupExport && m_capabilities.settingsBackupVersion < 1) {
        finishSettingsBackupExport(false, QStringLiteral("The Core stopped offering settings export."),
                                   {}, true);
        if (!self || m_sessionEpoch != epoch) return;
    }
    if (!directWatchEligible() && !relayWatchEligible(false)) {
        retireDirectWatch();
        if (!self || m_sessionEpoch != epoch) { return; }
    }
    refreshRemoteTransmit();
    // iPhone app plan Task 78: a device that shares the Core accepts the
    // Core closing its last slice (a take), and shows an empty band that
    // offers a take (the several-devices design, section 12).
    if (!m_radioModel.isNull()) {
        m_radioModel->setStationMayCloseLastSlice(sessionHolderAvailable());
    }
    m_remoteDevices->setSelfDeviceId(thisDeviceWireId());
    m_sliceAccess->setSelfDeviceId(thisDeviceWireId());
    // Core-slice take-over: Take control of the Core's own slice follows
    // the Core's sliceAccessVersion.
    m_sliceAccess->setCoreSliceTakeable(coreSliceTakeAvailable());
    emit transmitTakeAvailabilityChanged();
    if (!self || m_sessionEpoch != epoch) { return; }
    if (m_handshakeComplete) {
        requestWatchAttempt();
        if (!self || m_sessionEpoch != epoch) { return; }
    }

    if (m_capabilities.effectiveMaxSlices < m_capabilities.boardMaxSlices) {
        qCInfo(lcStationClient)
            << "Station is limiting slices to" << m_capabilities.effectiveMaxSlices
            << "of the board's" << m_capabilities.boardMaxSlices
            << "-- a daemon capacity decision, not a radio limit";
    }

    if (m_radioModel.isNull()) {
        if (previousBudget != remoteDisplayBudgetLimits()
            || previousPs3 != remotePs3DisplaySubscribed()
            || previousReason != remoteDisplayBudgetReason()) {
            emit displayBudgetChanged();
        }
        return;
    }
    // The step that makes three earlier tasks mean anything: identity,
    // board capabilities, the EFFECTIVE slice limit, userDdcCount, and the
    // connection state, all through one production entry point.
    m_radioModel->applyStationCapabilities(m_capabilities);
    if (!self || m_sessionEpoch != epoch || !m_sessionActive || !m_radioModel) { return; }
    // A station may update its advertised optional capabilities after the
    // initial snapshot.  Once the session is established, this can change
    // whether the typed remote TGXL controls are available without a radio
    // connection-state transition.
    if (m_handshakeComplete) {
        m_radioModel->pureSignalFacade()->setRemoteCapabilities(
            m_agreedMinor >= kDspControlSessionProtocolMinor && m_capabilities.psAlgorithmVersion == 3,
            m_capabilities.txPermitted, pureSignalArmingOffered());
        if (!self || m_sessionEpoch != epoch || !m_sessionActive || !m_radioModel) { return; }
        m_radioModel->dspAssets()->setRemoteNr3ModelsSupported(remoteNr3ModelsAvailable());
        if (!self || m_sessionEpoch != epoch || !m_sessionActive || !m_radioModel) { return; }
        m_radioModel->reportStationLinkStateChanged();
        if (!self || m_sessionEpoch != epoch || !m_sessionActive || !m_radioModel) { return; }
    }

    // The singletons exist from RadioModel's own construction, so they can
    // be mapped and watched as soon as capabilities land, rather than
    // waiting for an object.create that will never come for them (the
    // daemon watches them directly; only slices have a lifecycle).
    const QByteArray radioKey(kRadioKey);
    m_objects.insert(radioKey, m_radioModel.data());
    watchForOutbound(radioKey, m_radioModel.data());

    m_objects.insert("pureSignal", m_radioModel->pureSignalFacade());
    m_objects.insert("dspAssets", m_radioModel->dspAssets());
    m_radioModel->pureSignalFacade()->setRemoteRequestHandler(
        [self](Ps3Action action, const QVariantMap& arguments) -> quint32 {
        if (!self || !self->propertyResultsAvailable() || self->m_capabilities.psAlgorithmVersion != 3) {
            return 0;
        }
        const auto values = dspCommandValues(arguments);
        return values ? self->invokeCommand(PureSignalSessionFacade::actionVerb(action), *values) : 0;
    });
    disconnect(m_radioModel->pureSignalFacade(), &PureSignalSessionFacade::displaySubscriptionRequested,
               this, nullptr);
    connect(m_radioModel->pureSignalFacade(), &PureSignalSessionFacade::displaySubscriptionRequested,
            this, [this](bool enabled) {
        if (remoteDisplayBudgetLimits()) { emit ps3DisplaySubscriptionRequested(enabled); }
        else { requestPs3DisplaySubscription(enabled); }
    });
    m_radioModel->dspAssets()->setRemoteRequestHandler(
        [self](const QByteArray& verb, const QVariantMap& arguments) -> quint32 {
        if (!self || !self->m_handshakeComplete
            || self->m_agreedMinor < kDspControlSessionProtocolMinor
            || self->m_capabilities.dspAssetVersion < 1 || !verb.startsWith("dspAssets.")) {
            return 0;
        }
        // R-R3-21: NR3 models need a dspAssetVersion 2 Core. An older Core
        // would refuse them anyway; not sending keeps its answer predictable.
        if (self->m_capabilities.dspAssetVersion < 2
            && (verb == "dspAssets.selectNr3Model"
                || (verb == "dspAssets.beginImport"
                    && arguments.value(QStringLiteral("kind")).toLongLong()
                           == static_cast<qlonglong>(DspAssetKind::Nr3Model)))) {
            return 0;
        }
        const auto values = dspCommandValues(arguments);
        return values ? self->invokeCommand(verb, *values) : 0;
    });
    m_objects.insert("pureSignalSettings", m_radioModel->pureSignalSettings());
    watchForOutbound("pureSignalSettings", m_radioModel->pureSignalSettings());

    // R-R3-21 / R-R3-09: a Core with notchControlVersion owns the notch
    // list. The window mirrors it and asks for every change; it never
    // writes or restores the Core's Notch* settings. Against an older Core
    // nothing changes: the key is not registered, so its object (if any)
    // is dropped, and the model keeps today's settings path.
    if (NotchModel* notches = m_radioModel->notchModel()) {
        const bool mirrored = m_agreedMinor >= kDspControlSessionProtocolMinor
            && m_capabilities.notchControlVersion >= 1;
        if (mirrored) {
            notches->setMirrorMode(true);
            // R-R3-21, R-IOS-27: version 2 lets the window's +TNF send
            // notch.addAtSlice (RadioModel::addTnfForSlice).
            notches->setRemoteControlVersion(m_capabilities.notchControlVersion);
            notches->setRemoteRequestHandler(
                [self](const QByteArray& verb, const QVariantMap& arguments) -> quint32 {
                if (!self || !self->remoteNotchControlAvailable() || !verb.startsWith("notch.")) {
                    return 0;
                }
                const auto values = dspCommandValues(arguments);
                return values ? self->invokeCommand(verb, *values) : 0;
            });
            m_objects.insert("notches", notches);
            watchForOutbound("notches", notches);
        } else {
            m_objects.remove("notches");
            m_outboundMirror->unwatch("notches");
            notches->setRemoteRequestHandler({});
            notches->setRemoteControlVersion(0);
            notches->setMirrorMode(false);
        }
    }

    // R-R3-46 / R-R3-11: a Core with radioHardwareVersion 1 mirrors its step
    // attenuator and preamp as `stepAtt` and applies the window's edits
    // through its own controller. The window's edits pass only while that
    // holds; against an older Core the key is not registered, so its object
    // (if any) is dropped and nothing is sent.
    if (StepAttenuatorFacade* stepAtt = m_radioModel->stepAttFacade()) {
        stepAtt->setEditGate([self](QString* reason) {
            const bool allowed = self
                && (self->m_applyingInbound || self->remoteRadioHardwareAvailable());
            if (!allowed && reason) {
                *reason = self ? self->radioHardwareUnavailableReason()
                               : QStringLiteral("Connect to the Core to change the attenuator "
                                                "and preamp.");
            }
            return allowed;
        });
        if (m_agreedMinor >= kRadioIdentitySessionProtocolMinor
            && m_capabilities.radioHardwareVersion >= 1) {
            m_objects.insert("stepAtt", stepAtt);
            watchForOutbound("stepAtt", stepAtt);
        } else {
            m_objects.remove("stepAtt");
            m_outboundMirror->unwatch("stepAtt");
        }
        // R-R3-46 / R-R3-11: a Core that does not send the other ADC's
        // attenuator leaves every slice on attenuationDb, as before.
        if (m_capabilities.adcAttenuatorVersion < 1) {
            stepAtt->applyRemoteProperty(QByteArrayLiteral("rx2SliceMask"), 0);
        }
    }

    // R-R3-46: a Core with radioHardwareVersion 2 mirrors its Alex antenna
    // settings as `alexAntennas` and applies the window's receive edits
    // through its own AlexController. Same gate shape as `stepAtt`.
    if (AlexAntennaFacade* alex = m_radioModel->alexAntennaFacade()) {
        alex->setEditGate([self](QString* reason) {
            const bool allowed = self
                && (self->m_applyingInbound || self->remoteHardwareConfigAvailable());
            if (!allowed && reason) {
                *reason = self ? self->hardwareConfigUnavailableReason()
                               : QStringLiteral("Connect to the Core to change the radio's "
                                                "hardware settings.");
            }
            return allowed;
        });
        if (m_agreedMinor >= kRadioIdentitySessionProtocolMinor
            && m_capabilities.radioHardwareVersion >= 2) {
            m_objects.insert("alexAntennas", alex);
            watchForOutbound("alexAntennas", alex);
        } else {
            m_objects.remove("alexAntennas");
            m_outboundMirror->unwatch("alexAntennas");
        }
        // R-R3-46 fix wave (radioHardwareVersion 3): one band's antenna at a
        // time, so a list built before the Core changed another band cannot
        // put that band back. A version 2 Core gets today's whole list.
        if (m_agreedMinor >= kRadioIdentitySessionProtocolMinor
            && m_capabilities.radioHardwareVersion >= 3) {
            alex->setBandEditSender([self](Band band, int antenna, bool rxOnly, QString* reason) {
                if (!self) {
                    return false;
                }
                const CommandOutcome outcome = self->requestAlexRxAntenna(band, antenna, rxOnly);
                if (!outcome.sent && reason) {
                    *reason = outcome.reason;
                }
                return outcome.sent;
            });
        } else {
            alex->setBandEditSender({});
        }
        // Parity mini-round (radioHardwareVersion 6): one band's TX antenna
        // at a time too (setAlexTxAntenna), so the grid cannot put back a
        // TX antenna the Core changed on another band.
        if (m_agreedMinor >= kRadioIdentitySessionProtocolMinor
            && m_capabilities.radioHardwareVersion >= 6) {
            alex->setTxBandEditSender([self](Band band, int antenna, QString* reason) {
                if (!self) {
                    return false;
                }
                const CommandOutcome outcome = self->requestAlexTxAntenna(band, antenna);
                if (!outcome.sent && reason) {
                    *reason = outcome.reason;
                }
                return outcome.sent;
            });
        } else {
            alex->setTxBandEditSender({});
        }
    }

    // R-R3-46 fix wave (radioHardwareVersion 3): the Core's HL2 I/O board,
    // read-only; its values go into the window's own board, which Setup's
    // HL2 I/O board tab shows.
    if (IoBoardHl2Facade* ioBoard = m_radioModel->ioBoardFacade()) {
        if (m_agreedMinor >= kRadioIdentitySessionProtocolMinor
            && m_capabilities.radioHardwareVersion >= 3) {
            m_objects.insert("ioBoard", ioBoard);
            watchForOutbound("ioBoard", ioBoard);
        } else {
            m_objects.remove("ioBoard");
            m_outboundMirror->unwatch("ioBoard");
            // Follow-up item 5: the window's board shows no Core's board
            // it cannot follow (a previous Core's readings would stay).
            ioBoard->clearRemoteValues();
        }
    }

    const QByteArray transmitKey(kTransmitKey);
    m_objects.insert(transmitKey, &m_radioModel->transmitModel());
    watchForOutbound(transmitKey, &m_radioModel->transmitModel());

    if (m_radioModel->tunerModel() != nullptr) {
        const QByteArray tunerKey(kTunerKey);
        m_objects.insert(tunerKey, m_radioModel->tunerModel());
        watchForOutbound(tunerKey, m_radioModel->tunerModel());
    }

    // R-R3-47 / R-R3-22: the Core's Power Genius and RF-Kit status, read
    // only. Registered against a Core that offers them; otherwise the key
    // is not held and an object for it is dropped as skew. Nothing on
    // either is ever written back.
    const struct {
        const char* key;
        QObject* object;
        bool offered;
    } accessories[] = {
        { "amplifier", m_radioModel->amplifierModel(),
          m_agreedMinor >= kRadioIdentitySessionProtocolMinor
              && m_capabilities.remotePgxlControlVersion >= 1 },
        { "rfkit", m_radioModel->rfKitModel(),
          m_agreedMinor >= kRadioIdentitySessionProtocolMinor
              && m_capabilities.remoteRfKitControlVersion >= 1 },
        // R-R3-48: the Core's station TCI server.
        { "stationTci", m_radioModel->stationTciModel(),
          m_agreedMinor >= kRadioIdentitySessionProtocolMinor
              && m_capabilities.stationTciVersion >= 1 },
        // R-R3-47 / R-R3-22: the Core's accessory records and settings.
        { "accessoryData", m_radioModel->accessoryDataModel(),
          m_agreedMinor >= kRadioIdentitySessionProtocolMinor
              && m_capabilities.accessoryDataVersion >= 1 },
        // R-R3-47 / R-R3-22: the amp's and tuner's own settings.
        { "accessorySettings", m_radioModel->accessorySettingsModel(),
          m_agreedMinor >= kRadioIdentitySessionProtocolMinor
              && (m_capabilities.remotePgxlControlVersion >= 3
                  || m_capabilities.remoteTgxlControlVersion >= 1) },
    };
    for (const auto& accessory : accessories) {
        const QByteArray key(accessory.key);
        if (accessory.object != nullptr && accessory.offered) {
            m_objects.insert(key, accessory.object);
            watchForOutbound(key, accessory.object);
        } else {
            m_objects.remove(key);
            m_outboundMirror->unwatch(key);
        }
    }

    // Parity Task 19 (recordStreamVersion 1): the Core's spot sources, read
    // only; never written back. Against a Core that does not send them the
    // key is not held and the station's sources read off.
    if (SpotSourceHost* spotSources = m_radioModel->spotSourceHost()) {
        if (m_agreedMinor >= kRadioIdentitySessionProtocolMinor
            && m_capabilities.recordStreamVersion >= 1) {
            m_objects.insert(QByteArrayLiteral("spotSources"), spotSources);
        } else {
            m_objects.remove(QByteArrayLiteral("spotSources"));
            spotSources->clearStationValues();
        }
    }

    // iPhone app plan Task 39 (txStateVersion 1): the Core's transmit state
    // and meters, read only; never written back. Against a Core that does
    // not send it the key is not held and the window's copy reads idle.
    if (m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.txStateVersion >= 1) {
        m_objects.insert(QByteArrayLiteral("txState"), m_transmitState);
    } else {
        m_objects.remove(QByteArrayLiteral("txState"));
        m_transmitState->clearStationValues();
    }

    // iPhone app plan Task 25 (vaxVersion 1): the Core computer's VAX. Its
    // levels and mutes a window changes through the object (sent on as
    // property writes); against a Core that does not send it the key is
    // not held and the copy reads idle.
    const bool vaxHeld = stationVaxAvailable();
    if (vaxHeld) {
        m_objects.insert(QByteArrayLiteral("vax"), m_stationVax);
        watchForOutbound(QByteArrayLiteral("vax"), m_stationVax);
    } else {
        m_objects.remove(QByteArrayLiteral("vax"));
        m_outboundMirror->unwatch(QByteArrayLiteral("vax"));
        m_stationVax->clearStationValues();
    }
    if (vaxHeld != m_stationVaxHeld) {
        m_stationVaxHeld = vaxHeld;
        emit stationVaxAvailabilityChanged();
    }

    const QList<PanadapterModel*> pans = m_radioModel->panadapters();
    for (int i = 0; i < pans.size(); ++i) {
        const QByteArray key = QByteArray(kPanKeyPrefix) + QByteArray::number(i);
        m_objects.insert(key, pans.at(i));
        watchForOutbound(key, pans.at(i));
    }
    if (previousBudget != remoteDisplayBudgetLimits()
        || previousPs3 != remotePs3DisplaySubscribed()
        || previousReason != remoteDisplayBudgetReason()) {
        emit displayBudgetChanged();
    }
}

void StationClient::handleSettingsSnapshot(const SessionMessage& message)
{
    if (m_settingsProxy.isNull()) {
        return;
    }
    QMap<QString, QString> data;
    for (const MirrorUpdate& entry : message.updates) {
        data.insert(QString::fromUtf8(entry.name), entry.value.toString());
    }

    const bool firstSnapshot = !m_settingsProxy->hasReceivedSnapshot();
    m_settingsProxy->applySnapshot(data);
    m_settingsSnapshotThisLink = true;
    m_coreKeepsTciSwitch = data.contains(QStringLiteral("StationTci_Enabled"));
    // R-R3-46: the window's copy of the Core's OC pin matrix follows the
    // Core's settings (RadioModel::scheduleRemoteOcReload, coalesced).
    if (!m_radioModel.isNull()) {
        for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
            m_radioModel->scheduleRemoteOcReload(it.key());
        }
        // R-R3-46 (parity Task 6): and its PA profiles and PA table, once.
        m_radioModel->scheduleRemotePaReload(QString());
        // Follow-up 6: pages showing the Core's settings re-read them.
        m_radioModel->reportStationSettingChanged(QString());
    }

    // Ready only NOW, never earlier. SliceModel, NotchModel,
    // FilterPresetStore and TciServer all do contains()-then-seed against
    // Station-classified prefixes in their constructors, and the only
    // thing stopping them writing ship defaults into the STATION's store
    // is that writes are dropped while not ready (SettingsProxy.h's own
    // "record, not fix" section). Those constructors ran when the
    // RadioModel this class was handed was built, which is necessarily
    // before now.
    m_settingsProxy->setReady(true);
    m_settingsProxy->setLocalOriginTag(
        QStringLiteral("client-%1").arg(reinterpret_cast<quintptr>(this), 0, 16));

    if (firstSnapshot && !m_radioModel.isNull()) {
        // Task 15 handoff: FaultLog loads its ring buffer in its
        // constructor, which on a remote client runs long before any
        // station settings exist, so both of RadioModel's instances come
        // up empty and stay that way forever. reload() was made public for
        // exactly this call and had zero callers until now. Noticing this
        // by hand needs PGXL or TGXL hardware, which is why it is wired
        // here rather than left to a bench to find.
        if (m_radioModel->pgxlFaultLog() != nullptr) {
            m_radioModel->pgxlFaultLog()->reload();
        }
        if (m_radioModel->tgxlFaultLog() != nullptr) {
            m_radioModel->tgxlFaultLog()->reload();
        }
        // R-R3-47: and the RF-Kit's. A Core that offers `accessoryData`
        // then sends its live lists, which replace these.
        if (m_radioModel->rfkitFaultLog() != nullptr) {
            m_radioModel->rfkitFaultLog()->reload();
        }
    }
}

void StationClient::handleSettingsValue(const SessionMessage& message)
{
    if (m_settingsProxy.isNull()) {
        return;
    }
    // An EMPTY entry list means the key is GONE from the station's store,
    // the same absence convention handleSettingsReject below already
    // reads (whole-branch review, Important 4). Before this, the daemon
    // sent a real entry holding "" for a removal and this method cached
    // it, so the client reported contains() true and value(key, default)
    // "" for a key the station did not have.
    const QString key = QString::fromUtf8(message.objectKey);
    if (key == QLatin1String("StationTci_Enabled")) {
        m_coreKeepsTciSwitch = !message.updates.isEmpty();   // rework follow-up 4
    }
    if (message.updates.isEmpty()) {
        m_settingsProxy->applyRemoteRemoval(key);
    } else {
        m_settingsProxy->applyRemoteValue(key, message.updates.first().value,
                                          message.originTag);
    }
    // R-R3-46: a changed cell of the Core's OC pin matrix reloads the
    // window's copy, so its next save cannot send a stale cell back.
    if (!m_radioModel.isNull()) {
        m_radioModel->scheduleRemoteOcReload(key);
        // R-R3-46 (parity Task 6): likewise the PA profiles and PA table.
        m_radioModel->scheduleRemotePaReload(key);
        // Follow-up 6: another window's (or the Core's) change reaches the
        // pages that show it.
        m_radioModel->reportStationSettingChanged(key);
    }
    if (key.startsWith(QStringLiteral("hardware/"))) {
        refreshSettingsHygiene();
    }
}

void StationClient::handleSettingsReject(const SessionMessage& message)
{
    if (m_settingsProxy.isNull()) {
        return;
    }
    // An EMPTY entry list means the station has nothing for this key
    // either: proven-unset, which SettingsProxy::applyRejection()
    // distinguishes from a restored empty string via an invalid QVariant.
    const QVariant restored =
        message.updates.isEmpty() ? QVariant() : message.updates.first().value;
    m_settingsProxy->applyRejection(QString::fromUtf8(message.objectKey), restored);
    // R-R3-46: a refused OC cell settles the window's copy on the Core's.
    if (!m_radioModel.isNull()) {
        m_radioModel->scheduleRemoteOcReload(QString::fromUtf8(message.objectKey));
        // R-R3-46 (parity Task 6): a refused PA write settles on the Core's.
        m_radioModel->scheduleRemotePaReload(QString::fromUtf8(message.objectKey));
    }
    // Task 78: a write held for a question is not a refusal to show.
    if (!message.reason.isEmpty() && m_radioModel && !isAwaitingConfirmation(message.reason)) {
        m_radioModel->reportStationSliceCommandRejected(message.reason);
    }
}

// ── The mirror, inbound ──────────────────────────────────────────────────

void StationClient::handleSchema(const SessionMessage& message)
{
    // Schema messages open a full snapshot burst, including a same-session
    // reseed after Core discovers its radio. Pause writes until its marker.
    m_forwardLocalChanges = false;
    // Task 18 step 8: schema skew caught by NAME comparison at handshake.
    // MirrorSchema's ordinals are dense and per-class, so two builds that
    // declare different property sets assign DIFFERENT ordinals to the
    // same names -- which is why every MirrorUpdate carries its name as
    // well, and why comparing names is the check that actually means
    // something. A property present on one side and absent on the other is
    // reported, not fatal: the overlap still round-trips correctly, and
    // refusing the whole session over one unknown property would make
    // every future property addition a flag day.
    const QMetaObject* mo = nullptr;
    QSet<QByteArray> stationNames;
    for (const SessionSchemaField& field : message.fields) {
        stationNames.insert(field.name);
    }

    // Resolve the class through an object of it we already hold, if any:
    // MirrorSchema is keyed on QMetaObject and this class has no static
    // name-to-metaobject table (and should not grow one -- the mirrored
    // allowlist already lives in MirrorSchema).
    for (auto it = m_objects.cbegin(); it != m_objects.cend(); ++it) {
        QObject* object = it.value().data();
        if (object == nullptr) {
            continue;
        }
        if (MirrorSchema::shortClassName(object->metaObject()->className())
            == message.className) {
            mo = object->metaObject();
            break;
        }
    }
    if (mo == nullptr) {
        // Nothing of this class exists here YET. A slice's schema always
        // arrives before its first object.create, so recording every
        // station name as skew here would report the entire SliceModel
        // property table as missing on a client that in fact declares all
        // of it. Defer instead, and run the real comparison the moment an
        // instance exists (handleObjectCreate).
        m_pendingStationSchemas.insert(message.className, stationNames);
        return;
    }

    compareSchema(message.className, stationNames, mo);
}

void StationClient::compareSchema(const QByteArray& className,
                                  const QSet<QByteArray>& stationNames,
                                  const QMetaObject* mo)
{
    const MirrorSchema& local = MirrorSchema::forMetaObject(mo);
    QSet<QByteArray> localNames;
    for (const MirrorProperty& prop : local.properties()) {
        localNames.insert(prop.name);
    }

    int onlyStation = 0;
    int onlyLocal = 0;
    for (const QByteArray& name : stationNames) {
        if (!localNames.contains(name)) {
            m_schemaOnlyOnStation.insert(skewKey(className, name));
            ++onlyStation;
        }
    }
    for (const QByteArray& name : localNames) {
        // R-IOS-13 / R-R3-49: the Core sends transmit's txEqCurve only to a
        // peer that declared txEqCurve; for this window its absence is the
        // wire it asked for, not skew.
        if (className == "TransmitModel" && name == "txEqCurve"
            && !m_declaredFeatures.contains(QByteArrayLiteral("txEqCurve"))) {
            continue;
        }
        if (!stationNames.contains(name)) {
            // A property the Core sends only to a peer that declared its
            // link feature is expected to be missing when this client did
            // not declare it (MirrorPolicy::featureGates); that is not skew.
            const MirrorPolicy::FeatureGate* gate =
                MirrorPolicy::featureGateFor(className, name);
            if (gate != nullptr
                && m_declaredFeatures.value(QByteArray(gate->feature)) < gate->minVersion) {
                continue;
            }
            m_schemaOnlyLocal.insert(skewKey(className, name));
            ++onlyLocal;
        }
    }
    // THIS class's counts, not the accumulated set sizes. Logging the set
    // totals made every class after the first look like it had inherited
    // the previous one's differences.
    if (onlyStation > 0 || onlyLocal > 0) {
        qCWarning(lcStationClient)
            << "Mirror schema skew for" << className << "-- station-only:" << onlyStation
            << "local-only:" << onlyLocal;
    }
}

QObject* StationClient::resolveOrCreate(const QByteArray& objectKey,
                                        const QByteArray& className)
{
    if (QObject* existing = m_objects.value(objectKey).data()) {
        return existing;
    }
    if (m_radioModel.isNull()) {
        return nullptr;
    }
    // iPhone app Task 18: a Core sends its `devices` object to a client
    // that declares deviceAuth (the link document, section 7.1). This
    // window does not show the list, so it holds no object for it.
    if (objectKey == QByteArrayLiteral("devices")) {
        return nullptr;
    }
    const int sliceId = idFromKey(objectKey, kSliceKeyPrefix);
    if (sliceId < 0) {
        qCWarning(lcStationClient)
            << "Station named an object this client cannot construct:" << objectKey
            << className;
        return nullptr;
    }

    // A slice this client already holds under the station's id is ADOPTED,
    // not refused and not duplicated. This is the ordinary case rather than
    // an edge: RadioModel's own construction path can leave a slice behind
    // (connectToRadio seeds Slice A locally, and a reconnect finds whatever
    // the previous session built), and the id space is the STATION's, so an
    // existing local slice under that id is the same slice by definition.
    SliceModel* slice = m_radioModel->sliceById(sliceId);
    if (slice == nullptr) {
        // Ids are the station's, not minted here -- see
        // RadioModel::addSliceWithStationId for why a locally minted id
        // drifts from the station's after any mid-list removal, and what
        // that costs.
        if (m_radioModel->addSliceWithStationId(sliceId) != sliceId) {
            return nullptr;
        }
        slice = m_radioModel->sliceById(sliceId);
    }
    if (slice == nullptr) {
        return nullptr;
    }
    m_objects.insert(objectKey, slice);
    watchForOutbound(objectKey, slice);
    return slice;
}

// Whole-branch review, Important 1.
//
// endSession() deliberately RETAINS RadioModel's slices across a link
// loss (design doc section 13: "the client retains last-known state"), and
// resolveOrCreate() adopts, on reattach, only the ids the station actually
// NAMES. Nothing anywhere closed the other half of that: a slice the
// station no longer has was never adopted, never watched, never mirrored,
// and never removed either -- it simply stayed on screen, and every edit
// the operator made to it went nowhere. m_forwardLocalChanges gates the
// forwarder on session state, not on membership, so the ghost did not even
// produce a refusal to log.
//
// Reachable by an ordinary restart, not a contrived one: slice_count is a
// real key in packaging/nereusd.conf.sample, DaemonConfig parses it, and
// DaemonApp::createConfiguredSlices clamps it to min(requested, the
// board's cap) and additionally stops early when the allocator refuses. A
// daemon that served two slices and comes back with one is a config edit
// or a smaller board away.
//
// WHEN this runs is the whole of the design. It cannot run per
// object.create (a burst that has delivered slice 0 but not yet slice 1
// would reap slice 1 for not having arrived), it cannot run on
// Capabilities (which precedes the burst entirely), and it must not run
// on link loss (the station may well come back with the same set, and
// section 13's retention is what lets a GUI keep its pointers). The
// snapshot-complete marker is the one point at which the station has
// finished naming everything it has, which is exactly the question this
// asks.
//
// Removal goes through the same RadioModel::removeSliceWithStationId()
// that handleObjectDestroy() uses, under the same inbound guard, so a
// reap is indistinguishable downstream from an explicit destroy. NOT
// removeSlice(), which on a Role::Remote model sends a removeSlice verb
// to the station (RadioModel.h) -- reaping locally must not ask the
// station to remove slices it has already told this client it does not
// have. Two of the removal body's own invariants carry through unchanged
// and are relied on here:
// it refuses to remove the last remaining slice (so a station reporting
// zero slices leaves the client with one rather than an empty model), and
// it hands TX off before removing a TX-bound victim.
void StationClient::reconcileSlicesAgainstStation()
{
    if (m_radioModel.isNull()) {
        return;
    }
    // A COPY: removeSlice() mutates the list this iterates.
    const QList<SliceModel*> held = m_radioModel->slices();
    QList<int> reaped;
    for (SliceModel* slice : held) {
        if (slice == nullptr) {
            continue;
        }
        const int sliceId = slice->sliceIndex();
        const QByteArray key = QByteArray(kSliceKeyPrefix) + QByteArray::number(sliceId);
        if (m_objects.contains(key)) {
            continue;
        }
        // Symmetric with handleObjectDestroy(): drop the wire registry
        // entry and the outbound watch first, then remove the object.
        // Both are no-ops for a slice that was never adopted, and both
        // are correct for one adopted by an EARLIER session whose
        // registry entry endSession() already cleared.
        m_objects.remove(key);
        m_outboundMirror->unwatch(key);
        {
            InboundGuard guard(m_applyingInbound);
            m_radioModel->removeSliceWithStationId(sliceId);
        }
        // Confirmed, not assumed. removeSlice() returns void and refuses
        // silently when the victim is the LAST remaining slice, so a
        // station that somehow reported none would otherwise be logged as
        // a reap that did not happen. Not reachable through configuration
        // (DaemonConfig refuses slice_count below 1), which is why this
        // is a check rather than a branch with behaviour behind it.
        if (m_radioModel->sliceById(sliceId) == nullptr) {
            reaped.append(sliceId);
        } else {
            qCWarning(lcStationClient)
                << "Station does not have slice" << sliceId
                << "but it could not be removed locally; RadioModel always keeps at"
                << "least one slice. It stays on screen, unmirrored.";
        }
    }
    if (!reaped.isEmpty()) {
        QStringList ids;
        ids.reserve(reaped.size());
        for (int id : reaped) {
            ids.append(QString::number(id));
        }
        qCInfo(lcStationClient)
            << "Station no longer has slice(s)" << ids.join(QLatin1String(", "))
            << "-- removed locally so no control is left pointing at a slice"
            << "the station cannot act on";
    }
}

void StationClient::handleObjectCreate(const SessionMessage& message)
{
    // iPhone app plan Task 78: who else is on the Core and their slices,
    // plain state for the window's screens; no model object stands for
    // them here.
    if (RemoteDevicesState::holdsKey(message.objectKey)) {
        m_pendingStationSchemas.remove(message.className);
        m_remoteDevices->applyObject(message.objectKey, message.updates);
        return;
    }
    // Slice control plan Task 5: who controls and who listens to a slice.
    if (SliceAccessMirror::holdsKey(message.objectKey)) {
        m_pendingStationSchemas.remove(message.className);
        m_sliceAccess->applyObject(message.objectKey, message.updates);
        return;
    }
    QObject* target = resolveOrCreate(message.objectKey, message.className);
    if (target == nullptr) {
        return;
    }

    // The deferred half of the schema comparison: this is the first
    // instance of a class whose schema arrived before anything of that
    // class existed here. See handleSchema().
    const auto pending = m_pendingStationSchemas.find(message.className);
    if (pending != m_pendingStationSchemas.end()) {
        compareSchema(message.className, pending.value(), target->metaObject());
        m_pendingStationSchemas.erase(pending);
    }

    applyUpdates(target, message.objectKey, message.updates);
}

void StationClient::handleObjectDestroy(const SessionMessage& message)
{
    if (RemoteDevicesState::holdsKey(message.objectKey)) {
        m_remoteDevices->destroyObject(message.objectKey);
        return;
    }
    if (SliceAccessMirror::holdsKey(message.objectKey)) {
        m_sliceAccess->destroyObject(message.objectKey);
        return;
    }
    const int sliceId = idFromKey(message.objectKey, kSliceKeyPrefix);
    m_objects.remove(message.objectKey);
    m_pendingWrites.remove(message.objectKey);
    m_coreValues.remove(message.objectKey);
    m_outboundMirror->unwatch(message.objectKey);
    if (sliceId >= 0 && !m_radioModel.isNull()) {
        InboundGuard guard(m_applyingInbound);
        // removeSliceWithStationId, NOT removeSlice. On a Role::Remote
        // model removeSlice() now SENDS a removeSlice verb (RadioModel.h),
        // so calling it here would bounce the station's own destroy
        // straight back at the station as a fresh command.
        m_radioModel->removeSliceWithStationId(sliceId);
    }
}

void StationClient::handleDelta(const SessionMessage& message)
{
    if (RemoteDevicesState::holdsKey(message.objectKey)) {
        m_remoteDevices->applyObject(message.objectKey, message.updates);
        return;
    }
    if (SliceAccessMirror::holdsKey(message.objectKey)) {
        m_sliceAccess->applyObject(message.objectKey, message.updates);
        return;
    }
    QObject* target = m_objects.value(message.objectKey).data();
    if (target == nullptr) {
        // Once per object per session: a newer Core's object this client
        // does not hold (notches on an older app) changes often.
        if (!m_unheldDeltaKeys.contains(message.objectKey)
            && message.objectKey != QByteArrayLiteral("devices")) {
            m_unheldDeltaKeys.insert(message.objectKey);
            qCWarning(lcStationClient) << "Delta for an object this client does not hold:"
                                       << message.objectKey;
        }
        return;
    }
    QList<MirrorUpdate> current;
    QList<MirrorUpdate> held;
    const auto pending = m_pendingWrites.value(message.objectKey);
    for (const auto& value : message.updates) {
        if (!pending.contains(value.name)) {
            current.append(value);
        } else {
            // Skipped while the operator's write holds it, unless this
            // delta's side effects cancel an unsent hold
            // (restoreOperatorValues): then this value applies.
            held.append(value);
        }
    }
    applyUpdates(target, message.objectKey, current, SideEffectRule::Delta, held);
    // What the Core holds, applied or not: a later cancel falls back to it.
    for (const auto& value : held) {
        m_coreValues[message.objectKey].insert(value.name, value);
    }
}

void StationClient::handlePropertyResult(const SessionMessage& message)
{
    QObject* target = m_objects.value(message.objectKey).data();
    if (!target || message.writeId == 0) {
        return;
    }
    auto pending = m_pendingWrites.find(message.objectKey);
    if (pending == m_pendingWrites.end()) {
        return;
    }
    QList<MirrorUpdate> acceptedValues;
    QList<SessionPropertyResult> currentResults;
    for (const auto& result : message.propertyResults) {
        if (!pending->contains(result.property)
            || pending->value(result.property).writeId != message.writeId) {
            // The answer to a write a newer unsent edit has superseded:
            // that edit still wins, but the write is no longer on its way,
            // and its value is now what the Core holds.
            auto superseded = pending->find(result.property);
            if (superseded != pending->end() && superseded->writeId == 0
                && superseded->inFlightWriteId == message.writeId) {
                superseded->inFlightWriteId = 0;
                superseded->inFlightValue = MirrorUpdate{};
                if (result.hasValue) {
                    m_coreValues[message.objectKey].insert(result.property, result.value);
                }
            }
            continue;
        }
        pending->remove(result.property);
        if (result.hasValue) {
            acceptedValues.append(result.value);
        }
        currentResults.append(result);
    }
    if (pending->isEmpty()) {
        m_pendingWrites.erase(pending);
    }
    applyUpdates(target, message.objectKey, acceptedValues);
    for (const auto& result : currentResults) {
        if (auto* slice = qobject_cast<SliceModel*>(target);
            slice && (result.property.startsWith("nnr") || result.property == "activeNr")) {
            InboundGuard guard(m_applyingInbound);
            slice->reportNnrEditResult(result.reason);
        }
        emit propertyWriteCompleted(message.objectKey, result.property, message.writeId,
                                    result.accepted, result.reason);
    }
}

namespace {

// What a change from elsewhere does to the window's unsent edits, stated
// per cause rather than inferred from values. When a delta applies
// `cause` to an object of `className` and moves it, every unsent edit of
// a `dependents` property on that object is cancelled
// (StationClient::cancelUnsentEdit): the cause's setter defines those
// properties, so an edit made before it has no place after it.
struct DeltaCancelRule {
    const char* className;
    const char* cause;
    // Up to five, unused slots null.
    std::array<const char*, 5> dependents;
    // The cause's setter saves the dependents' current values somewhere
    // of their own before it replaces them, so they step back to the
    // Core's before it runs and a cancelled edit is never saved.
    bool stepBack;
    // When set, the cause defines its dependents only for a new value
    // (decoded) this accepts; otherwise for every new value.
    bool (*defines)(const QVariant& decoded) = nullptr;
};

// setCfcParaEqData projects only a curve that decodes onto the scalars;
// a curve that does not decode leaves them as they are.
bool cfcCurveDecodes(const QVariant& decoded)
{
    CfcProfile::Profile profile;
    return CfcProfile::decode(decoded.toString(), profile);
}

constexpr DeltaCancelRule kDeltaCancelRules[] = {
    // Filter edges are per-mode state. SetRX1Mode loads the new mode's
    // own last filter:
    //   RX1Filter = rx1_filters[(int)new_mode].LastFilter;
    // From Thetis console.cs:34513 [v2.10.3.15] (in SetRX1Mode, :33957),
    // and SetRX1Filter saves the edges as the current mode's
    // (console.cs:34766-34768 [v2.10.3.15]). SliceModel::setDspMode saves
    // the current edges as the old mode's LastFilter before loading the
    // new mode's, so the edges step back first.
    { "SliceModel", "dspMode", { { "filterLow", "filterHigh" } }, true },
    // The line-in boost sets the line-in gain index:
    //   var lineboost = Array.IndexOf(lineinboost, line_in_boost.ToString());
    //   NetworkIO.SetLineBoost(lineboost);
    // From Thetis console.cs:40930-40932 [v2.10.3.15]
    // (TransmitModel::setLineInBoost -> setLineInGain).
    { "TransmitModel", "lineInBoost", { { "lineInGain" } }, false },
    // NereusSDR's paired CFC curve: a curve that decodes projects onto
    // the five scalars, and each scalar re-encodes the curve
    // (TransmitModel::setCfcParaEqData, updatePairedCfc,
    // updatePairedCfcArray).
    { "TransmitModel", "cfcParaEqData",
      { { "cfcPrecompDb", "cfcPostEqGainDb", "cfcEqFreqJson", "cfcCompressionJson",
          "cfcPostEqBandGainJson" } }, false, &cfcCurveDecodes },
    { "TransmitModel", "cfcPrecompDb", { { "cfcParaEqData" } }, false },
    { "TransmitModel", "cfcPostEqGainDb", { { "cfcParaEqData" } }, false },
    { "TransmitModel", "cfcEqFreqJson", { { "cfcParaEqData" } }, false },
    { "TransmitModel", "cfcCompressionJson", { { "cfcParaEqData" } }, false },
    { "TransmitModel", "cfcPostEqBandGainJson", { { "cfcParaEqData" } }, false },
};

} // namespace

QList<QByteArray> StationClient::unresolvedDeltaCancelRuleNames()
{
    QList<QByteArray> unresolved;
    for (const DeltaCancelRule& rule : kDeltaCancelRules) {
        const QByteArray className(rule.className);
        const QMetaObject* meta = nullptr;
        if (className == "SliceModel") {
            meta = &SliceModel::staticMetaObject;
        } else if (className == "TransmitModel") {
            meta = &TransmitModel::staticMetaObject;
        }
        if (meta == nullptr || !MirrorSchema::isMirrorable(meta)
            || MirrorSchema::shortClassName(meta->className()) != className) {
            unresolved.append(className);
            continue;
        }
        const MirrorSchema& schema = MirrorSchema::forMetaObject(meta);
        const auto check = [&](const char* name) {
            if (schema.byName(QByteArray(name)) == nullptr) {
                unresolved.append(className + '.' + name);
            }
        };
        check(rule.cause);
        for (const char* name : rule.dependents) {
            if (name == nullptr) {
                break;
            }
            check(name);
        }
    }
    return unresolved;
}

void StationClient::applyUpdates(QObject* target, const QByteArray& objectKey,
                                 const QList<MirrorUpdate>& updates, SideEffectRule rule,
                                 const QList<MirrorUpdate>& heldValues)
{
    if (updates.isEmpty()) {
        return;
    }
    const MirrorSchema& schema = MirrorSchema::forObject(target);
    const QByteArray className =
        MirrorSchema::shortClassName(target->metaObject()->className());

    // ONE guard around the whole batch, not one per property: a single
    // setter can move several properties as a side effect (SliceModel::
    // setDspMode rewrites both filter edges), and those have to be
    // suppressed too.
    InboundGuard guard(m_applyingInbound);

    // A delta from elsewhere cancels the unsent edits its causes define
    // (kDeltaCancelRules). Which ones is decided before any setter runs:
    // the rules whose cause this delta carries and the window holds at a
    // different value, and their dependents with an unsent edit here.
    QList<const MirrorProperty*> toCancel;
    if (rule == SideEffectRule::Delta) {
        const auto pending = m_pendingWrites.value(objectKey);
        QList<const MirrorProperty*> toStepBack;
        for (const DeltaCancelRule& cancelRule : kDeltaCancelRules) {
            if (className != cancelRule.className) {
                continue;
            }
            const MirrorProperty* cause = schema.byName(cancelRule.cause);
            const auto carried = std::find_if(
                updates.cbegin(), updates.cend(),
                [&](const MirrorUpdate& u) { return u.name == cancelRule.cause; });
            if (cause == nullptr || carried == updates.cend()) {
                continue;
            }
            const QVariant next = MirrorSchema::decode(*cause, carried->value);
            if (MirrorSchema::decode(*cause, schema.read(*cause, target)) == next) {
                continue;
            }
            if (cancelRule.defines != nullptr && !cancelRule.defines(next)) {
                continue;
            }
            for (const char* name : cancelRule.dependents) {
                if (name == nullptr) {
                    break;
                }
                const auto write = pending.constFind(QByteArray(name));
                const MirrorProperty* dependent = schema.byName(name);
                if (write == pending.cend() || write->writeId != 0 || dependent == nullptr
                    || toCancel.contains(dependent)) {
                    continue;
                }
                toCancel.append(dependent);
                if (cancelRule.stepBack) {
                    toStepBack.append(dependent);
                }
            }
        }
        // Back to the write still in flight, else to the Core's value.
        const auto core = m_coreValues.value(objectKey);
        for (const MirrorProperty* prop : toStepBack) {
            const PendingWrite write = pending.value(prop->name);
            if (write.inFlightWriteId != 0) {
                applyOne(target, *prop, write.inFlightValue);
            } else if (core.contains(prop->name)) {
                applyOne(target, *prop, core.value(prop->name));
            }
        }
    }

    QSet<QByteArray> applied;
    for (const MirrorUpdate& update : updates) {
        applied.insert(update.name);
        m_coreValues[objectKey].insert(update.name, update);
        const MirrorProperty* prop = schema.byName(update.name);
        if (prop == nullptr) {
            // A property this build does not declare. Already recorded by
            // handleSchema when the schema arrived; recorded again here
            // because a slice's schema is compared before any instance of
            // it exists.
            m_schemaOnlyOnStation.insert(skewKey(className, update.name));
            continue;
        }
        if (!applyOne(target, *prop, update)) {
            const QByteArray key = skewKey(className, update.name);
            if (!m_unapplied.contains(key)) {
                m_unapplied.insert(key);
                // ONCE per (class, property), not per delta: an S-meter
                // property that cannot land would otherwise produce a log
                // line ten times a second.
                qCWarning(lcStationClient)
                    << "No way to apply station value for" << key
                    << "-- this property will read stale on this client";
            }
        }
    }

    for (const MirrorProperty* prop : toCancel) {
        cancelUnsentEdit(objectKey, target, *prop, heldValues);
    }

    // Still under the guard: putting the operator's values back is not a
    // change of theirs to send again.
    restoreOperatorValues(objectKey, applied);
}

void StationClient::restoreOperatorValues(const QByteArray& appliedKey,
                                          const QSet<QByteArray>& applied)
{
    // The guard suppresses the observer, not the change. A Core value a
    // setter applies can move a DIFFERENT property as a side effect
    // (SliceModel::setDspMode rewrites both filter edges; a TransmitModel
    // CFC scalar re-encodes the paired curve). When that property holds
    // an operator edit the Core has not answered, the flush would read
    // the moved value and send the Core's state back as the operator's
    // choice. So the operator's value goes back now: it wins until the
    // Core answers that write, and the window shows what it will send.
    // A property the Core's message named keeps the Core's value (only an
    // object.create names one with a pending write; a delta skips them,
    // and a result ends the hold before it applies). An unsent edit a
    // delta's cause defines was already cancelled (kDeltaCancelRules).
    struct Held {
        quint64 order;
        QByteArray key;
        QByteArray name;
    };
    QList<Held> held;
    for (auto object = m_pendingWrites.cbegin(); object != m_pendingWrites.cend(); ++object) {
        for (auto write = object->cbegin(); write != object->cend(); ++write) {
            held.append(Held{write->order, object.key(), write.key()});
        }
    }
    std::sort(held.begin(), held.end(),
              [](const Held& a, const Held& b) { return a.order < b.order; });
    for (const Held& h : held) {
        QObject* object = m_objects.value(h.key).data();
        auto writes = m_pendingWrites.find(h.key);
        if (object == nullptr || writes == m_pendingWrites.end()) {
            continue;
        }
        auto write = writes->find(h.name);
        if (write == writes->end()) {
            continue;
        }
        const MirrorSchema& schema = MirrorSchema::forObject(object);
        const MirrorProperty* prop = schema.byName(h.name);
        if (prop == nullptr) {
            continue;
        }
        const QVariant live = schema.read(*prop, object);
        if (!live.isValid()) {
            continue;
        }
        if (h.key == appliedKey && applied.contains(h.name)) {
            write->value.value = live;
            continue;
        }
        if (live == write->value.value) {
            continue;
        }
        const MirrorUpdate operatorValue = write->value;
        applyOne(object, *prop, operatorValue);
    }
}

void StationClient::cancelUnsentEdit(const QByteArray& objectKey, QObject* object,
                                     const MirrorProperty& prop,
                                     const QList<MirrorUpdate>& heldValues)
{
    auto writes = m_pendingWrites.find(objectKey);
    if (writes == m_pendingWrites.end()) {
        return;
    }
    auto write = writes->find(prop.name);
    if (write == writes->end()) {
        return;
    }
    m_outboundCoalescer.remove(objectKey, prop.ordinal);
    if (write->inFlightWriteId != 0) {
        // An earlier write of this property is still on its way. Back to
        // that state: its value shows and its answer applies, so the
        // window ends where the Core does (the Core excludes the writer
        // from its own change).
        write->writeId = write->inFlightWriteId;
        write->value = write->inFlightValue;
        write->inFlightWriteId = 0;
        write->inFlightValue = MirrorUpdate{};
        applyOne(object, prop, write->value);
        return;
    }
    writes->erase(write);
    if (writes->isEmpty()) {
        m_pendingWrites.erase(writes);
    }
    // The delta's own value for it, else the last value the Core sent.
    for (const MirrorUpdate& value : heldValues) {
        if (value.name == prop.name) {
            applyOne(object, prop, value);
            return;
        }
    }
    const auto core = m_coreValues.value(objectKey);
    if (core.contains(prop.name)) {
        applyOne(object, prop, core.value(prop.name));
    }
}

bool StationClient::applyOne(QObject* target, const MirrorProperty& prop,
                             const MirrorUpdate& update)
{
    const MirrorSchema& schema = MirrorSchema::forObject(target);
    const QByteArray className =
        MirrorSchema::shortClassName(target->metaObject()->className());

    // CONSTANT properties are object identity, not state. sliceIndex is
    // the case that matters, and it was already consumed: it is what
    // resolveOrCreate() minted the slice under.
    if (prop.isConstant) {
        return true;
    }

    // Strategy 1: a real Q_PROPERTY WRITE.
    // A lock prevents GUI-originated tuning, never an authoritative station
    // snapshot/delta. Apply without temporarily unlocking observable state.
    if (className == "SliceModel" && prop.name == "frequency") {
        auto* slice = qobject_cast<SliceModel*>(target);
        const QVariant frequency = MirrorSchema::decode(prop, update.value);
        return slice && frequency.isValid() && slice->applyStationFrequency(frequency.toDouble());
    }
    if (prop.isWritable) {
        return schema.write(prop, target, update.value);
    }

    const QVariant native = MirrorSchema::decode(prop, update.value);
    if (!native.isValid()) {
        return false;
    }

    // Radio connectivity can change while the authenticated station link
    // stays up. The handshake seeds this state from capabilities, but its
    // read-only property deltas need the same explicit remote-state writer.
    // RadioModel's generic inbound hook is deliberately a command boundary.
    if (target == m_radioModel.data() && className == QByteArrayLiteral("RadioModel")
        && prop.name == QByteArrayLiteral("connected")
        && m_radioModel->role() == RadioModel::Role::Remote) {
        m_capabilities.radioConnected = native.toBool();
        m_radioModel->setStationConnectionState(m_capabilities.radioConnected
            ? ConnectionState::Connected : ConnectionState::Disconnected);
        return true;
    }

    // Strategy 2: the model's own inbound hook -- but ONLY where that hook
    // is a genuine STATE APPLY, never where it is a COMMAND SENDER.
    //
    // This distinction is not fussiness, it is a direction error the
    // allowlist exists to make structurally impossible. applyMirroredValue
    // is the DAEMON's inbound path: "a remote peer is asking this model to
    // do something." Inbound on a CLIENT the same message means the
    // opposite: "the station reports this is now true." Feeding a state
    // report into a command sender inverts the link.
    //
    // TunerModel is the live case. Its hook answers isOperate / isBypass /
    // antennaA by calling setOperate() / setBypass() / setAntennaA(), each
    // of which forwards a command to a bound TgxlConnection. Two things
    // went wrong before this allowlist:
    //
    //   - Those three setters no-op when no tuner is bound and the hook
    //     still returns success, so on a client isOperate and isBypass
    //     reported as APPLIED, changed nothing, read stale, and never
    //     entered m_unapplied -- defeating the accessor Task 20's bench is
    //     meant to trust.
    //   - It was inert only because a remote client has no TgxlConnection.
    //     Bind one and every inbound tuner delta from the station becomes
    //     an outbound tuner COMMAND from the client.
    //
    // Fixed HERE rather than in TunerModel::applyMirroredValue, which was
    // the other option the review offered. That hook's accept-with-no-tuner
    // behaviour is deliberate and tested: tst_mirror_inbound's
    // tunerOperateAndBypassRouteThroughTheHookToTheRealCommandSlots pins it
    // with the rationale that the mirror is a REMOTE CLICK and must not
    // diverge from what a local TunerApplet click does, which is also a
    // silent no-op with no tuner attached. Changing it would overturn a
    // documented Task 8 decision to fix a problem that only exists on the
    // client, so the client is where it is fixed.
    //
    // So the client consults the hook only for pairs proven to be a plain
    // state apply: SliceModel's signal readings call plain telemetry
    // setters, including the separate R3 peak and average readings.
    // Adding a pair here means having read the hook
    // body and confirmed it writes state rather than sending a command.
    static const QSet<QByteArray> kClientStateApplyHooks = {
        // R-R3-22: RadioModel assigns these only in Role::Remote; it never
        // calls the listener-owning setter or persists a GUI-local setting.
        QByteArrayLiteral("RadioModel.fourO3AEnabled"),
        QByteArrayLiteral("RadioModel.fourO3AListening"),
        QByteArrayLiteral("RadioModel.fourO3AListenerError"),
        // R-R3-47: likewise the Core's RF-Kit switch.
        QByteArrayLiteral("RadioModel.rfKitEnabled"),
        // R-R3-49: likewise the Core's transmit state; it never keys here.
        QByteArrayLiteral("RadioModel.transmitting"),
        // R-R3-49 (parity Task 6): likewise the Core's TX inhibit.
        QByteArrayLiteral("RadioModel.txInhibited"),
        // Parity Task 16: likewise the Core's DSP facts.
        QByteArrayLiteral("RadioModel.dspOptionsLastApplyMs"),
        // Fix wave (M2): likewise the Core's waiting reason.
        QByteArrayLiteral("RadioModel.stationRadioWaiting"),
        // Parity Task 22: likewise the Core's logging categories.
        QByteArrayLiteral("RadioModel.logCategories"),
        // HL2 port part 2: likewise the Core's TX inhibit reason.
        QByteArrayLiteral("RadioModel.txInhibitReason"),
        // PA on-air gate re-review: likewise the Core's on-air PA row.
        QByteArrayLiteral("RadioModel.paTransmitBand"),
        QByteArrayLiteral("SliceModel.minNotchWidthHz"),
        QByteArrayLiteral("SliceModel.signalStrengthDbm"),
        QByteArrayLiteral("SliceModel.signalPeakDbm"),
        QByteArrayLiteral("SliceModel.signalAverageDbm"),
        // Parity Task 15: likewise the Core's ADC and AGC readings.
        QByteArrayLiteral("SliceModel.adcPeakDbfs"),
        QByteArrayLiteral("SliceModel.adcAverageDbfs"),
        QByteArrayLiteral("SliceModel.agcGainDb"),
        QByteArrayLiteral("SliceModel.agcPeakDb"),
        QByteArrayLiteral("SliceModel.agcAverageDb"),
        QByteArrayLiteral("SliceModel.stationAutoAgcNoiseFloorDbm"),
        QByteArrayLiteral("SliceModel.stationAutoAgcNoiseFloorValid"),
        QByteArrayLiteral("SliceModel.stationAutoAgcNoiseFloorGeneration"),
        QByteArrayLiteral("SliceModel.streamCtunPinned"),
        QByteArrayLiteral("SliceModel.streamEpoch"),
    };
    if (kClientStateApplyHooks.contains(skewKey(className, prop.name))) {
        QString hookReason;
        const bool invoked = QMetaObject::invokeMethod(
            target, "applyMirroredValue", Qt::DirectConnection,
            Q_RETURN_ARG(QString, hookReason), Q_ARG(QByteArray, prop.name),
            Q_ARG(QVariant, native));
        if (invoked && hookReason.isEmpty()) {
            return true;
        }
    }

    // Strategy 3: the client-side adapter, for properties whose hook
    // refusal is correct on the daemon and wrong here.
    return applyClientOnlyProperty(target, className, prop.name, native);
}

bool StationClient::applyClientOnlyProperty(QObject* target, const QByteArray& className,
                                            const QByteArray& propertyName,
                                            const QVariant& native)
{
    if (target == m_radioModel.data() && className == "RadioModel") {
        if (propertyName == "settingsSaveError") {
            m_radioModel->applyStationSettingsSaveError(native.toString());
            return true;
        }
        if (m_radioModel->applyStationReceiveLayoutStatus(propertyName, native.toString())) {
            return true;
        }
        if (m_radioModel->applyStationBandOutputsValue(propertyName, native)) {
            return true;
        }
        if (m_radioModel->applyStationAlexLpfValue(propertyName, native)) {
            return true;
        }
        if (m_radioModel->applyStationLevelCalValue(propertyName, native)) {
            return true;
        }
        return m_radioModel->applyStationFilterValue(propertyName, native);
    }
    if (className == "PureSignalSessionFacade") {
        auto* facade = qobject_cast<PureSignalSessionFacade*>(target);
        return facade && facade->applyRemoteProperty(propertyName, native);
    }
    if (className == "DspAssetService") {
        auto* assets = qobject_cast<DspAssetService*>(target);
        return assets && assets->applyRemoteProperty(propertyName, native);
    }
    if (className == "NotchModel") {
        auto* notches = qobject_cast<NotchModel*>(target);
        return notches && notches->applyRemoteProperty(propertyName, native);
    }
    if (className == "StepAttenuatorFacade") {
        auto* stepAtt = qobject_cast<StepAttenuatorFacade*>(target);
        return stepAtt && stepAtt->applyRemoteProperty(propertyName, native);
    }
    if (className == "AlexAntennaFacade") {
        auto* alex = qobject_cast<AlexAntennaFacade*>(target);
        return alex && alex->applyRemoteProperty(propertyName, native);
    }
    if (className == "IoBoardHl2Facade") {
        auto* ioBoard = qobject_cast<IoBoardHl2Facade*>(target);
        return ioBoard && ioBoard->applyRemoteProperty(propertyName, native);
    }
    if (className == "PureSignalSettings") {
        auto* settings = qobject_cast<PureSignalSettings*>(target);
        return settings && settings->applyStationDiagnostic(propertyName, native);
    }
    if (className == "TunerModel") {
        auto* tuner = qobject_cast<TunerModel*>(target);
        return tuner != nullptr && tuner->applyStationValue(propertyName, native);
    }
    // R-R3-49 (parity Task 2): the Core's tune power for its transmit band
    // and tune drive source, plain state; they change only by command.
    if (className == "TransmitModel") {
        auto* tx = qobject_cast<TransmitModel*>(target);
        return tx != nullptr && tx->applyStationValue(propertyName, native);
    }
    // R-R3-47 / R-R3-22: plain state applies; never a command to an amp.
    // iPhone app plan Task 39: a plain state apply; the Core's transmitter
    // is never changed from here.
    if (className == "TransmitState") {
        auto* state = qobject_cast<TransmitState*>(target);
        return state != nullptr && state->applyStationValue(propertyName, native);
    }
    if (className == "AmplifierModel") {
        auto* amp = qobject_cast<AmplifierModel*>(target);
        return amp != nullptr && amp->applyStationValue(propertyName, native);
    }
    if (className == "RfKitModel") {
        auto* rfKit = qobject_cast<RfKitModel*>(target);
        return rfKit != nullptr && rfKit->applyStationValue(propertyName, native);
    }
    // Parity Task 19: a plain state apply; the sources change only by
    // command.
    if (className == "SpotSourceHost") {
        auto* spotSources = qobject_cast<SpotSourceHost*>(target);
        return spotSources != nullptr && spotSources->applyStationValue(propertyName, native);
    }
    // iPhone app plan Task 25: the Core computer's VAX slices, device names
    // and transmit slice, plain state (its levels and mutes are writable
    // and land through their setters).
    if (className == "StationVax") {
        auto* vax = qobject_cast<StationVax*>(target);
        return vax != nullptr && vax->applyStationValue(propertyName, native);
    }
    // R-R3-48: a plain state apply; the switch changes only by command.
    if (className == "StationTciModel") {
        auto* tci = qobject_cast<StationTciModel*>(target);
        return tci != nullptr && tci->applyStationValue(propertyName, native);
    }
    // R-R3-47 / R-R3-22: a plain state apply; changes only by command.
    if (className == "AccessoryDataModel") {
        auto* data = qobject_cast<AccessoryDataModel*>(target);
        return data != nullptr && data->applyStationValue(propertyName, native);
    }
    // R-R3-47 / R-R3-22: a plain state apply; changes only by command.
    if (className == "AccessorySettingsModel") {
        auto* settings = qobject_cast<AccessorySettingsModel*>(target);
        return settings != nullptr && settings->applyStationValue(propertyName, native);
    }
    if (className != "SliceModel") {
        return false;
    }
    auto* slice = qobject_cast<SliceModel*>(target);
    if (slice == nullptr) {
        return false;
    }
    if (propertyName.startsWith("nnr")) {
        return slice->applyStationNnrDiagnostic(propertyName, native);
    }

    // SliceModel::active and ::txSlice have no WRITE, and their
    // applyMirroredValue correctly REFUSES on the daemon: a remote peer
    // must go through the setActiveSliceById verb and through
    // TxSliceArbiter respectively, or it would bypass the exclusivity
    // those two exist to maintain. Inbound on a client the direction is
    // reversed -- the arbiter has already spoken and this is its answer
    // arriving -- so refusing here would leave a remote operator unable to
    // see which slice is active or which one transmits, both of which the
    // R2 demo names explicitly (design addendum section 2).
    if (propertyName == "active") {
        slice->setActive(native.toBool());
        // ...and move RadioModel's own m_activeSlice with it. The flag
        // alone was not enough: activeSlice() is repointed only by
        // RadioModel::setActiveSlice(), so before this, every
        // activeSlice()-reading surface on a remote GUI (the container
        // S-meter, the RX applet, the DSP menu, the band buttons) stayed
        // stranded on whichever slice was created first, no matter which
        // one the station reported active.
        //
        // Only on the TRUE edge. The false edge is the slice being stood
        // down, and its partner true edge -- which arrives in the same
        // batch, in either order -- is what does the repointing;
        // setActiveSlice() clears the outgoing slice's flag itself.
        if (native.toBool() && !m_radioModel.isNull()) {
            m_radioModel->applyStationActiveSlice(slice->sliceIndex());
        }
        return true;
    }
    if (propertyName == "txSlice") {
        slice->setTxSlice(native.toBool());
        return true;
    }
    // radeStatusVersion 1: the Core's RADE decoder sync and offset. Read
    // only on the wire (no WRITE), set here as the Core reported them; the
    // window's RadioModel carries them to the VFO flag and the RADE applet.
    if (propertyName == "radeSynced") {
        slice->setRadeSynced(native.toBool());
        return true;
    }
    if (propertyName == "radeFreqOffsetHz") {
        slice->setRadeFreqOffsetHz(native.toDouble());
        return true;
    }
    // radeReasonVersion 1: why the Core's RADE slice has no working decoder.
    // Read only on the wire, set here as the Core worded it.
    if (propertyName == "radeReason") {
        slice->setRadeReason(native.toString());
        return true;
    }
    if (propertyName == "band") {
        // Derived from frequency by SliceModel itself, on this client
        // exactly as on the daemon, so the frequency delta in the same
        // batch already produced it. Accepted rather than counted as a
        // gap, because nothing is actually missing.
        return true;
    }
    return false;
}

// ── The mirror, outbound ─────────────────────────────────────────────────

void StationClient::watchForOutbound(const QByteArray& objectKey, QObject* object)
{
    if (object == nullptr) {
        return;
    }
    if (auto* settings = qobject_cast<PureSignalSettings*>(object)) {
        QPointer<StationClient> self(this);
        settings->setEditGate([self](QString* reason) {
            const bool allowed = self && (self->m_applyingInbound
                || (self->propertyResultsAvailable() && self->m_capabilities.psAlgorithmVersion == 3));
            if (!allowed && reason) {
                *reason = QStringLiteral("The station does not support PS3 settings.");
            }
            return allowed;
        });
    }
    m_outboundMirror->watch(objectKey, object);
    if (auto* slice = qobject_cast<SliceModel*>(object)) {
        const QPointer<StationClient> owner(this);
        // Slice control plan Task 5: the Core's own state is applied to a
        // listened slice; only a change this window makes is held back.
        // (RadioModel announces a held change, sliceRequestHeldForListener.)
        slice->setStationApplyProbe([owner]() { return owner && owner->m_applyingInbound; });
        slice->setNnrSettingsApplier([owner](const NnrSettings& requested, QString* reason)
                                       -> std::optional<NnrSettings> {
            if (owner && (owner->m_applyingInbound || owner->nnrControlAvailable())) {
                return requested;
            }
            if (reason) { *reason = QStringLiteral("This station session does not support NNR controls."); }
            return std::nullopt;
        });
        slice->setNrSelectionApplier([owner](NrSlot requested, QString* reason) {
            if (owner && owner->m_applyingInbound) {
                return true;
            }
            if (requested == NrSlot::NNR && !(owner && owner->nnrControlAvailable())) {
                if (reason) { *reason = QStringLiteral("This station session does not support NNR."); }
                return false;
            }
            // Fix wave I3 (R-R3-21): the Core said it has no usable NR3
            // model (mirrored nr3Runnable), so NR3 cannot run there.
            DspAssetService* assets = owner && owner->m_radioModel
                ? owner->m_radioModel->dspAssets() : nullptr;
            if (requested == NrSlot::NR3 && assets && !assets->nr3Runnable()) {
                if (reason) {
                    *reason = assets->nr3ModelStatus().isEmpty()
                        ? QStringLiteral("NR3 cannot run on this Core: no NR3 model file was found.")
                        : assets->nr3ModelStatus();
                }
                return false;
            }
            // R-R3-49, Sub-epic C-1: likewise DFNR (mirrored dfnrRunnable).
            if (requested == NrSlot::DFNR && assets && !assets->dfnrRunnable()) {
                if (reason) {
                    *reason = assets->dfnrModelStatus().isEmpty()
                        ? QStringLiteral("DFNR cannot run on this Core.")
                        : assets->dfnrModelStatus();
                }
                return false;
            }
            // And MNR (the Core's mirrored mnrRunnable: it runs only on a
            // Mac Core) and BNR (in no build), in the model's words; and
            // DFNR or MNR on a Core too old to say (trunk merge, parity
            // Task 16's rule).
            if ((requested == NrSlot::DFNR || requested == NrSlot::MNR
                 || requested == NrSlot::BNR) && owner
                && owner->m_radioModel) {
                const QString cannot = owner->m_radioModel->nrCannotRunReason(requested);
                if (!cannot.isEmpty()) {
                    if (reason) {
                        *reason = cannot;
                    }
                    return false;
                }
            }
            return true;
        });
        // R-R3-40: the operator's retry goes to the station. A station
        // echo (choosing the model the station reports) is not a retry.
        disconnect(slice, &SliceModel::nnrRetryRequested, this, nullptr);
        connect(slice, &SliceModel::nnrRetryRequested, this, [this, slice]() {
            if (m_applyingInbound) {
                return;
            }
            const auto outcome = requestNnrRetry(slice->sliceIndex());
            if (!outcome.sent) {
                slice->reportNnrEditResult(outcome.reason);
            }
        });
        disconnect(slice, &SliceModel::nnrDiagnosticsRequested, this, nullptr);
        connect(slice, &SliceModel::nnrDiagnosticsRequested, this,
                [this, slice](int testMode, int outputMode) {
            const auto outcome = requestNnrDiagnostics(slice->sliceIndex(), testMode, outputMode);
            if (!outcome.sent) { slice->reportNnrEditResult(outcome.reason); }
        });
    }
}

void StationClient::onWriteFlushTick()
{
    // handleSchema() pauses the wire for a full snapshot burst.  Preserve
    // coalesced operator edits until SnapshotComplete reopens this gate.
    if (!m_forwardLocalChanges) {
        return;
    }
    const QList<QPair<QByteArray, QList<MirrorUpdate>>> pending = m_outboundCoalescer.flush();
    for (const auto& batch : pending) {
        QObject* object = m_objects.value(batch.first).data();
        if (object == nullptr) {
            // Unwatched or destroyed since it was marked dirty. Dropped
            // rather than sent, the same call StateMirror::
            // flushCoalescedDeltas makes for the same situation.
            continue;
        }
        const MirrorSchema& schema = MirrorSchema::forObject(object);
        QList<MirrorUpdate> resolved;
        resolved.reserve(batch.second.size());
        for (const MirrorUpdate& pendingUpdate : batch.second) {
            // Re-read the LIVE value rather than sending what the
            // coalescer stored: an inbound apply can have moved the
            // property again since it was marked dirty (the guard
            // suppresses the observer, not the change), and sending the
            // stale one would tell the station to undo its own value.
            // Where the Core answers writes, an inbound SIDE EFFECT on a
            // pending property has already been undone by
            // restoreOperatorValues(), so the live value is the
            // operator's.
            const MirrorProperty* prop = schema.byName(pendingUpdate.name);
            if (prop == nullptr) {
                continue;
            }
            const QVariant live = schema.read(*prop, object);
            if (!live.isValid()) {
                continue;
            }
            resolved.append(MirrorUpdate{ prop->ordinal, prop->name, prop->kind, live });
        }
        if (!resolved.isEmpty()) {
            quint32 writeId = 0;
            if (propertyResultsAvailable()) {
                writeId = m_nextPropertyWriteId++;
                if (m_nextPropertyWriteId == 0) {
                    ++m_nextPropertyWriteId;
                }
                auto& writes = m_pendingWrites[batch.first];
                for (const auto& update : resolved) {
                    // The value sent is the operator's: an inbound side
                    // effect since the edit was put back by
                    // restoreOperatorValues(). It is held until this
                    // write's answer.
                    auto entry = writes.find(update.name);
                    if (entry == writes.end()) {
                        writes.insert(update.name,
                                      PendingWrite{writeId, update, m_nextPendingWriteOrder++, 0, MirrorUpdate{}});
                    } else {
                        entry->writeId = writeId;
                        entry->value = update;
                        entry->inFlightWriteId = 0;
                        entry->inFlightValue = MirrorUpdate{};
                    }
                }
            }
            send(SessionMessages::propertyWrite(batch.first, resolved, writeId));
        }
    }
}

// ── Commands and send ────────────────────────────────────────────────────

quint32 StationClient::invokeCommand(const QByteArray& verb,
                                     const QList<MirrorUpdate>& arguments)
{
    if ((m_sessionPurpose == SessionPurpose::RenameOnly
         && verb != QByteArrayLiteral("station.rename") && verb != QByteArrayLiteral("session.leave"))
        || m_transport == nullptr || !m_authenticated) {
        return 0;
    }
    const quint32 id = m_nextCommandId++;
    if (m_nextCommandId == 0) {
        ++m_nextCommandId;
    }
    send(SessionMessages::commandInvoke(verb, id, arguments));
    return id;
}

// ── IStationLink: the operator's clicks leaving this process ─────────────
//
// Every one of the five is the same three steps: build the arguments
// SessionCommandDispatcher's handler for that verb reads by name, hand
// them to invokeCommand(), and remember what the command was about so its
// result can be reported to a human. Nothing here touches RadioModel:
// applying the change on the way out is precisely the defect this seam
// closes, and the daemon's answer arrives on the ordinary mirror path.

namespace {

// The argument shapes are SessionCommandDispatcher's, read by NAME out of
// the CommandInvoke's `arguments` list (which reuses MirrorUpdate as a
// generic {name, kind, value} triple -- SessionMessage::arguments' own doc
// comment). `ordinal` is not consulted by any handler, so 0 throughout.
MirrorUpdate intArgument(const QByteArray& name, int value)
{
    return MirrorUpdate{ 0, name, MirrorWireKind::Int64,
                         QVariant(static_cast<qlonglong>(value)) };
}

MirrorUpdate stringArgument(const QByteArray& name, const QString& value)
{
    return MirrorUpdate{ 0, name, MirrorWireKind::Utf8, QVariant(value) };
}

MirrorUpdate boolArgument(const QByteArray& name, bool value)
{
    return MirrorUpdate{ 0, name, MirrorWireKind::Bool, QVariant(value) };
}

MirrorUpdate doubleArgument(const QByteArray& name, double value)
{
    return MirrorUpdate{ 0, name, MirrorWireKind::Float64, QVariant(value) };
}

// R-R3-47 / R-R3-22: the amp's and tuner's own settings verbs, whose
// refusals the Advanced pages show (RadioModel::accessoryRequestRefused).
// L1 (R-R3-47, R-R3-22, R-R3-48): what an accessory request the Core
// refused was about, for RadioModel::accessoryRequestRefused; empty for
// every other verb. "pgxl" and "tgxl" (the amp's and tuner's connection,
// output limit and own settings), "rfkit", "interlock", "tci" (the
// station TCI server), "4o3a" (the 4O3A switch) and, for a fault history,
// the device it names ("faults" for any other).
QString accessoryRefusalDevice(const QByteArray& verb, const QString& faultsDevice)
{
    if (verb == "setPgxlName" || verb == "setPgxlHardware" || verb == "setPgxlNetwork"
        || verb == "savePgxlSettings" || verb == "readPgxlSettings"
        || verb == "setPgxlPowerCap" || verb == "configurePgxl" || verb == "disconnectPgxl"
        || verb == "setPgxlConnectionSettings" || verb == "setPgxlOperate"
        || verb == "amp.operate" || verb == "amp.standby"
        || verb == "scanPgxlLan" || verb == "setPgxlAddress") {
        return QStringLiteral("pgxl");
    }
    if (verb == "setTgxlName" || verb == "setTgxlNetwork" || verb == "saveTgxlSettings"
        || verb == "readTgxlSettings" || verb == "configureTgxl" || verb == "disconnectTgxl"
        || verb == "setTgxlAntenna" || verb == "setTgxlOperate" || verb == "setTgxlBypass"
        || verb == "moveTgxlRelay" || verb == "scanTgxlLan" || verb == "setTgxlAddress"
        || verb == "tuner.tune" || verb == "tuner.operate" || verb == "tuner.bypass"
        || verb == "tuner.antenna") {
        return QStringLiteral("tgxl");
    }
    if (verb == "configureRfKit" || verb == "disconnectRfKit" || verb == "setRfKitEnabled"
        || verb == "resetRfKitError" || verb == "setRfKitOperate" || verb == "setRfKitAntenna"
        || verb == "setRfKitTciMode" || verb == "setRfKitAddress"
        || verb == "rfkit.operate" || verb == "rfkit.standby" || verb == "rfkit.antenna") {
        return QStringLiteral("rfkit");
    }
    if (verb == "setTxInterlockPolicy") {
        return QStringLiteral("interlock");
    }
    if (verb == "setStationTci" || verb == "setStationTciOptions"
        || verb == "setStationTciSettings" || verb == "disconnectStationTciClient") {
        return QStringLiteral("tci");
    }
    if (verb == "setFourO3AEnabled") {
        return QStringLiteral("4o3a");
    }
    if (verb == "clearAccessoryFaults") {
        if (faultsDevice == QLatin1String("pgxl") || faultsDevice == QLatin1String("tgxl")
            || faultsDevice == QLatin1String("rfkit")) {
            return faultsDevice;
        }
        return QStringLiteral("faults");
    }
    return {};
}

} // namespace

StationClient::CommandOutcome StationClient::sendCommand(const QByteArray& verb, int sliceId,
                                                         const QList<MirrorUpdate>& arguments,
                                                         const QString& action)
{
    const quint32 id = invokeCommand(verb, arguments);
    if (id == 0) {
        // invokeCommand()'s own two refusals: no transport, or a transport
        // that has not finished authenticating. Both are the same thing to
        // an operator -- the station is not reachable right now -- and
        // both are ordinary rather than exceptional, because this is the
        // state a remote GUI sits in before its first handshake and again
        // for the whole of a reconnect backoff.
        return CommandOutcome{
            false,
            QStringLiteral("The station session is not established, so %1 was not sent.")
                .arg(action)
        };
    }
    PendingCommand pending;
    pending.verb = verb;
    pending.sliceId = sliceId;
    if (verb == "clearAccessoryFaults") {
        for (const MirrorUpdate& argument : arguments) {
            if (argument.name == "device") {
                pending.faultsDevice = argument.value.toString();
            }
        }
    }
    if (verb.startsWith("spots.")) {
        for (const MirrorUpdate& argument : arguments) {
            if (argument.name == "source") {
                pending.spotSource = argument.value.toString();
            }
        }
    }
    // iPhone plan Task 22: a FreeDV Reporter request's refusal is shown
    // where the FreeDV Reporter controls are, as a source's is.
    if (verb.startsWith("freedv.")) {
        pending.spotSource = SpotSourceHost::kFreedvReporter;
    }
    if ((verb == "requestStreamCtunPinned" || verb == "requestStreamCentre")
        && !m_radioModel.isNull()) {
        if (SliceModel* slice = m_radioModel->sliceById(sliceId)) {
            pending.streamEpoch = slice->streamEpoch();
        }
        if (verb == "requestStreamCtunPinned") {
            for (const MirrorUpdate& argument : arguments) {
                if (argument.name == "pinned" && argument.value.typeId() == QMetaType::Bool) {
                    pending.requestedPin = argument.value.toBool();
                    break;
                }
            }
        }
    }
    m_pendingCommands.insert(id, pending);
    return CommandOutcome{ true, QString(), id };
}

StationClient::CommandOutcome StationClient::requestAddSlice(const QString& initialPanId)
{
    return sendCommand("addSlice", -1, { stringArgument("initialPanId", initialPanId) },
                       QStringLiteral("the request for a new slice"));
}

StationClient::CommandOutcome StationClient::requestAddSliceOnPan(const QString& panId)
{
    return sendCommand("addSliceOnPan", -1, { stringArgument("panId", panId) },
                       QStringLiteral("the request for a new slice"));
}

StationClient::CommandOutcome StationClient::requestRemoveSlice(int sliceId)
{
    return sendCommand("removeSlice", sliceId, { intArgument("sliceId", sliceId) },
                       QStringLiteral("the request to close this slice"));
}

StationClient::CommandOutcome StationClient::requestActiveSlice(int sliceId)
{
    return sendCommand("setActiveSliceById", sliceId, { intArgument("sliceId", sliceId) },
                       QStringLiteral("the request to make slice %1 active").arg(sliceId));
}

bool StationClient::bandSelectAvailable() const
{
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.bandSelectVersion >= 1;
}

StationClient::CommandOutcome StationClient::requestSelectBand(int sliceId, int band)
{
    if (!bandSelectAvailable()) {
        return IStationLink::requestSelectBand(sliceId, band);
    }
    if (band == static_cast<int>(Band::Band2m) && !station2mAvailable()) {
        return {false, station2mUnavailableReason()};
    }
    return sendCommand("slice.selectBand", sliceId,
                       { intArgument("sliceId", sliceId), intArgument("band", band) },
                       QStringLiteral("the band change"));
}

bool StationClient::remoteSliceAccessAvailable() const
{
    // Slice control plan Task 5: the Core sends sliceAccessVersion only to
    // a window that declared sliceAccess with sessionHolder.
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.sliceAccessVersion >= 1;
}

bool StationClient::controlTakeBackAvailable() const
{
    // Take-over parity: the Core sends the lower of its sliceAccessVersion
    // and the one this window declared.
    return remoteSliceAccessAvailable() && m_capabilities.sliceAccessVersion >= 2;
}

bool StationClient::coreSliceTakeAvailable() const
{
    // Core-slice take-over: the Core sends the lower of its
    // sliceAccessVersion and the one this window declared.
    return remoteSliceAccessAvailable() && m_capabilities.sliceAccessVersion >= 3;
}

QString StationClient::coreSliceTakeUnavailableReason(QChar letter)
{
    // The words a Core below sliceAccessVersion 3 refuses the take with
    // (StationServer::handOffRefusal).
    return QStringLiteral("Slice %1 is run by the Core itself, so control of it cannot pass to "
                          "this device.")
        .arg(letter);
}

QString StationClient::controlTakeBackUnavailableReason()
{
    return QStringLiteral("This Core cannot give control back from here. Updating the Core may "
                          "help.");
}

namespace {

MirrorUpdate countArgument(const QByteArray& name, quint64 value)
{
    // An incarnation or a control revision: a non-negative i64 on the wire.
    return MirrorUpdate{ 0, name, MirrorWireKind::Int64,
                         QVariant(static_cast<qlonglong>(value)) };
}

} // namespace

StationClient::CommandOutcome StationClient::requestListen(int sliceId, quint64 incarnation)
{
    if (!remoteSliceAccessAvailable()) {
        return IStationLink::requestListen(sliceId, incarnation);
    }
    return sendCommand("slice.listen", sliceId,
                       { intArgument("sliceId", sliceId), countArgument("incarnation", incarnation) },
                       QStringLiteral("the request to listen to this slice"));
}

StationClient::CommandOutcome StationClient::requestStopListening(int sliceId,
                                                                  quint64 incarnation)
{
    if (!remoteSliceAccessAvailable()) {
        return IStationLink::requestStopListening(sliceId, incarnation);
    }
    return sendCommand("slice.stopListening", sliceId,
                       { intArgument("sliceId", sliceId), countArgument("incarnation", incarnation) },
                       QStringLiteral("the request to stop listening to this slice"));
}

StationClient::CommandOutcome StationClient::requestTakeControl(int sliceId, quint64 incarnation,
                                                                quint64 controlRevision)
{
    if (!remoteSliceAccessAvailable()) {
        return IStationLink::requestTakeControl(sliceId, incarnation, controlRevision);
    }
    return sendCommand("slice.takeControl", sliceId,
                       { intArgument("sliceId", sliceId), countArgument("incarnation", incarnation),
                         countArgument("controlRevision", controlRevision) },
                       QStringLiteral("the request to take control of this slice"));
}

StationClient::CommandOutcome StationClient::requestRelease(int sliceId, quint64 incarnation,
                                                            quint64 controlRevision)
{
    if (!remoteSliceAccessAvailable()) {
        return IStationLink::requestRelease(sliceId, incarnation, controlRevision);
    }
    return sendCommand("slice.release", sliceId,
                       { intArgument("sliceId", sliceId), countArgument("incarnation", incarnation),
                         countArgument("controlRevision", controlRevision) },
                       QStringLiteral("the request to release this slice"));
}

StationClient::CommandOutcome StationClient::requestListenLevel(int sliceId, quint64 incarnation,
                                                                double level, bool muted)
{
    if (!remoteSliceAccessAvailable()) {
        return IStationLink::requestListenLevel(sliceId, incarnation, level, muted);
    }
    const double clamped = std::isfinite(level) ? std::clamp(level, 0.0, 1.0) : 0.0;
    return sendCommand("slice.setListenLevel", sliceId,
                       { intArgument("sliceId", sliceId), countArgument("incarnation", incarnation),
                         doubleArgument("level", clamped), boolArgument("muted", muted) },
                       QStringLiteral("the change to your volume for this slice"));
}

StationClient::CommandOutcome StationClient::requestSliceSampleRate(int sliceId, int rateHz)
{
    return sendCommand(
        "requestSliceSampleRate", sliceId,
        { intArgument("sliceId", sliceId), intArgument("rateHz", rateHz) },
        QStringLiteral("the sample-rate change to %1 kHz").arg(rateHz / 1000));
}

StationClient::CommandOutcome StationClient::requestStreamCtunPinned(int sliceId, bool pinned)
{
    if (!remoteCtunAvailable()) {
        return { false, QStringLiteral("The station does not support remote C-Tune.") };
    }
    return sendCommand("requestStreamCtunPinned", sliceId,
                       { intArgument("sliceId", sliceId), boolArgument("pinned", pinned) },
                       QStringLiteral("the C-Tune pin change"));
}

StationClient::CommandOutcome StationClient::requestStreamCentre(int sliceId, double centreHz)
{
    if (!remoteCtunAvailable()) {
        return { false, QStringLiteral("The station does not support remote C-Tune.") };
    }
    return sendCommand("requestStreamCentre", sliceId,
                       { intArgument("sliceId", sliceId), doubleArgument("centreHz", centreHz) },
                       QStringLiteral("the C-Tune center change"));
}

StationClient::CommandOutcome StationClient::requestConfigureTgxl(const QString& host, quint16 port)
{
    if (!remoteTgxlConfigAvailable()) {
        return { false, QStringLiteral("The station does not support remote TGXL configuration.") };
    }
    return sendCommand("configureTgxl", -1,
                       { stringArgument("host", host), intArgument("port", port) },
                       QStringLiteral("the TGXL configuration"));
}

bool StationClient::propertyResultsAvailable() const
{
    return m_handshakeComplete && m_agreedMinor >= kDspControlSessionProtocolMinor
        && m_capabilities.propertyResultVersion > 0;
}

StationClient::CommandOutcome StationClient::requestApplyNnrModels(quint32 revision)
{
    if (!nnrControlAvailable() || m_capabilities.dspAssetVersion < 1) {
        return {false, QStringLiteral("The station does not support NNR model application.")};
    }
    return sendCommand("nnr.applyModelSelection", -1, {intArgument("revision", revision)},
        QStringLiteral("the NNR model reconnect"));
}

bool StationClient::remoteNr3ModelsAvailable() const
{
    return m_handshakeComplete && m_agreedMinor >= kDspControlSessionProtocolMinor
        && m_capabilities.dspAssetVersion >= 2;
}

bool StationClient::remoteNotchControlAvailable() const
{
    return m_handshakeComplete && m_agreedMinor >= kDspControlSessionProtocolMinor
        && m_capabilities.notchControlVersion >= 1;
}

bool StationClient::remoteRadioHardwareAvailable() const
{
    return propertyResultsAvailable() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.radioHardwareVersion >= 1;
}

QString StationClient::radioHardwareUnavailableReason() const
{
    if (remoteRadioHardwareAvailable()) {
        return {};
    }
    if (!m_handshakeComplete) {
        return QStringLiteral("Connect to the Core to change the attenuator and preamp.");
    }
    return QStringLiteral("This Core cannot change its radio's attenuator for this "
                          "app. Updating the Core may help.");
}

bool StationClient::remoteHardwareConfigAvailable() const
{
    return propertyResultsAvailable() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.radioHardwareVersion >= 2;
}

QString StationClient::hardwareConfigUnavailableReason() const
{
    if (remoteHardwareConfigAvailable()) {
        return {};
    }
    if (!m_handshakeComplete) {
        return QStringLiteral("Connect to the Core to change the radio's hardware settings.");
    }
    return QStringLiteral("This Core cannot change its radio's hardware settings for this "
                          "app. Updating the Core may help.");
}

bool StationClient::remoteRxBypassOnTxAvailable() const
{
    return remoteHardwareConfigAvailable() && m_capabilities.radioHardwareVersion >= 5;
}

QString StationClient::rxBypassOnTxUnavailableReason() const
{
    if (remoteRxBypassOnTxAvailable()) {
        return {};
    }
    if (!remoteHardwareConfigAvailable()) {
        return hardwareConfigUnavailableReason();
    }
    return QStringLiteral("This Core cannot switch its receive bypass on transmit for this "
                          "app. Updating the Core may help.");
}

bool StationClient::remoteTransmitAntennasAvailable() const
{
    return remoteHardwareConfigAvailable() && m_capabilities.radioHardwareVersion >= 6;
}

QString StationClient::transmitAntennasUnavailableReason() const
{
    if (remoteTransmitAntennasAvailable()) {
        return {};
    }
    if (!remoteHardwareConfigAvailable()) {
        return hardwareConfigUnavailableReason();
    }
    return QStringLiteral("This Core cannot change its radio's transmit antennas for this "
                          "app. Updating the Core may help.");
}

StationClient::CommandOutcome StationClient::requestAlexRxAntenna(Band band, int antenna,
                                                                  bool rxOnly)
{
    if (!remoteHardwareConfigAvailable() || m_capabilities.radioHardwareVersion < 3) {
        return {false, hardwareConfigUnavailableReason()};
    }
    if (band == Band::Band2m && !station2mAvailable()) {
        return {false, station2mUnavailableReason()};
    }
    const bool radioBound = m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.radioAntennaRowsVersion == 1;
    const QString mac = m_radioModel ? m_radioModel->currentRadioMac() : QString();
    if (radioBound && (mac.isEmpty() || AppSettings::normalizedRadioMac(mac) != mac)) {
        return {false, QStringLiteral("The Core's connected radio is not ready for this antenna change.")};
    }
    QList<MirrorUpdate> arguments;
    if (radioBound) {
        arguments.append(stringArgument("mac", mac));
    }
    arguments.append({intArgument("band", static_cast<int>(band)),
                      intArgument("antenna", antenna), boolArgument("rxOnly", rxOnly)});
    return sendCommand(radioBound ? "setAlexRxAntennaForRadio" : "setAlexRxAntenna", -1,
                       arguments,
                       QStringLiteral("the antenna change"));
}

StationClient::CommandOutcome StationClient::requestAlexTxAntenna(Band band, int antenna)
{
    if (!remoteTransmitAntennasAvailable()) {
        return {false, transmitAntennasUnavailableReason()};
    }
    if (band == Band::Band2m && !station2mAvailable()) {
        return {false, station2mUnavailableReason()};
    }
    const bool radioBound = m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.radioAntennaRowsVersion == 1;
    const QString mac = m_radioModel ? m_radioModel->currentRadioMac() : QString();
    if (radioBound && (mac.isEmpty() || AppSettings::normalizedRadioMac(mac) != mac)) {
        return {false, QStringLiteral("The Core's connected radio is not ready for this antenna change.")};
    }
    QList<MirrorUpdate> arguments;
    if (radioBound) {
        arguments.append(stringArgument("mac", mac));
    }
    arguments.append({intArgument("band", static_cast<int>(band)),
                      intArgument("antenna", antenna)});
    return sendCommand(radioBound ? "setAlexTxAntennaForRadio" : "setAlexTxAntenna", -1,
                       arguments,
                       QStringLiteral("the antenna change"));
}

bool StationClient::filterPolicyEditAvailable() const
{
    return remoteHardwareConfigAvailable() && m_capabilities.radioHardwareVersion >= 4;
}

QString StationClient::filterPolicyUnavailableReason() const
{
    if (filterPolicyEditAvailable()) {
        return {};
    }
    if (!m_handshakeComplete) {
        return QStringLiteral("Connect to the Core to change the filter policy.");
    }
    return QStringLiteral("This Core cannot change its filter policy for this app. "
                          "Updating the Core may help.");
}

StationClient::CommandOutcome StationClient::requestFilterPolicy(int chain, int mode)
{
    if (!filterPolicyEditAvailable()) {
        return {false, filterPolicyUnavailableReason()};
    }
    return sendCommand("setAlexBpfMode", -1,
                       { intArgument("chain", chain), intArgument("mode", mode) },
                       QStringLiteral("the filter policy change"));
}

bool StationClient::radioHardwareAvailable(int minVersion) const
{
    return remoteHardwareConfigAvailable()
        && m_capabilities.radioHardwareVersion >= std::max(minVersion, 1);
}

StationClient::CommandOutcome StationClient::requestIoBoardI2c(int bus, int address, int reg,
                                                              bool write, int value)
{
    if (!radioHardwareAvailable(7)) {
        return {false, remoteHardwareConfigAvailable() ? ioBoardI2cUnavailableReason()
                                                       : hardwareConfigUnavailableReason()};
    }
    return sendCommand("requestIoBoardI2c", -1,
                       { intArgument("bus", bus), intArgument("address", address),
                         intArgument("register", reg), boolArgument("write", write),
                         intArgument("value", value) },
                       QStringLiteral("the I2C request"));
}

bool StationClient::radioSampleRateAvailable() const
{
    return radioHardwareAvailable(9);
}

StationClient::CommandOutcome StationClient::requestRadioSampleRate(int rateHz)
{
    if (!radioSampleRateAvailable()) {
        return IStationLink::requestRadioSampleRate(rateHz);
    }
    return sendCommand("setRadioSampleRate", -1, { intArgument("rateHz", rateHz) },
                       QStringLiteral("the sample-rate change to %1 kHz").arg(rateHz / 1000));
}

bool StationClient::levelCalibrationResetAvailable() const
{
    return radioHardwareAvailable(12);
}

StationClient::CommandOutcome StationClient::requestResetLevelCalibration()
{
    if (!levelCalibrationResetAvailable()) {
        return IStationLink::requestResetLevelCalibration();
    }
    return sendCommand("resetLevelCalibration", -1, {},
                       QStringLiteral("the level calibration reset"));
}

bool StationClient::levelCalibrationRunAvailable() const
{
    return radioHardwareAvailable(12);
}

bool StationClient::rx2PreampModeAvailable() const
{
    return radioHardwareAvailable(12);
}

StationClient::CommandOutcome StationClient::requestStartLevelCalibration(float levelDbm,
                                                                          double frequencyHz,
                                                                          int sliceId)
{
    if (!levelCalibrationRunAvailable()) {
        return IStationLink::requestStartLevelCalibration(levelDbm, frequencyHz, sliceId);
    }
    return sendCommand("startLevelCalibration", -1,
                       { doubleArgument("levelDbm", levelDbm),
                         doubleArgument("frequencyHz", frequencyHz),
                         intArgument("sliceId", sliceId) },
                       QStringLiteral("the level calibration"));
}

StationClient::CommandOutcome StationClient::requestCancelLevelCalibration()
{
    if (!levelCalibrationRunAvailable()) {
        return IStationLink::requestCancelLevelCalibration();
    }
    return sendCommand("cancelLevelCalibration", -1, {},
                       QStringLiteral("the level calibration cancel"));
}

bool StationClient::dspInfoAvailable() const
{
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.dspInfoVersion >= 1;
}

bool StationClient::spotSourcesAvailable() const
{
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.recordStreamVersion >= 1;
}

StationClient::CommandOutcome StationClient::requestSpotSource(const QByteArray& verb,
                                                               const QString& source,
                                                               const QString& text)
{
    if (!spotSourcesAvailable()) {
        return {false, stationLinkReady() ? spotSourcesUnavailableReason()
                                          : QStringLiteral("Not connected to the Core, so the "
                                                           "spot source request was not sent.")};
    }
    // iPhone plan Task 22: an older Core does not run FreeDV Reporter.
    if (source == SpotSourceHost::kFreedvReporter && !stationFreedvAvailable()) {
        return {false, stationFreedvUnavailableReason()};
    }
    QList<MirrorUpdate> arguments;
    if (verb != "spots.clearAll") {
        arguments.append(stringArgument("source", source));
    }
    if (verb == "spots.sendCommand") {
        arguments.append(stringArgument("text", text));
    }
    return sendCommand(verb, -1, arguments, QStringLiteral("the spot source request"));
}

bool StationClient::supportBundleAvailable() const
{
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.supportBundleVersion >= 1;
}

StationClient::CommandOutcome StationClient::requestSupportBundle()
{
    if (!supportBundleAvailable()) {
        return {false, stationLinkReady() ? supportBundleUnavailableReason()
                                          : QStringLiteral("Not connected to the Core, so its "
                                                           "support bundle was not asked for.")};
    }
    return sendCommand("support.collect", -1, {}, QStringLiteral("the support bundle request"));
}

StationClient::CommandOutcome StationClient::requestLogCategories(const QString& categories)
{
    if (!supportBundleAvailable()) {
        return {false, stationLinkReady() ? supportBundleUnavailableReason()
                                          : QStringLiteral("Not connected to the Core, so the "
                                                           "logging change was not sent.")};
    }
    return sendCommand("support.setLogCategories", -1,
                       { stringArgument("categories", categories) },
                       QStringLiteral("the logging change"));
}

void StationClient::requestCoreLog(bool follow)
{
    if (!supportBundleAvailable()) {
        return;
    }
    QList<MirrorUpdate> arguments{
        MirrorUpdate{0, "stream", MirrorWireKind::Utf8, QVariant(QStringLiteral("coreLog"))}};
    if (follow) {
        arguments.append(MirrorUpdate{
            0, "backlog", MirrorWireKind::Int64,
            QVariant(static_cast<qlonglong>(RadioModel::kStationCoreLogLines))});
    }
    invokeCommand(follow ? "records.subscribe" : "records.unsubscribe", arguments);
}

bool StationClient::txReadingsAvailable() const
{
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.txStateVersion >= 1 && m_capabilities.txReadingsVersion >= 1;
}

bool StationClient::txStageReadingsAvailable() const
{
    return txReadingsAvailable() && m_capabilities.txReadingsVersion >= 3;
}

void StationClient::setCfcCompressionWanted(bool wanted)
{
    if (m_cfcCompressionWanted == wanted) {
        return;
    }
    m_cfcCompressionWanted = wanted;
    if (txReadingsAvailable()) {
        sendCfcCompressionSubscription(wanted);
    }
}

bool StationClient::stationVaxAvailable() const
{
    return m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.vaxVersion >= 1;
}

void StationClient::setStationVaxLevelsWanted(bool wanted)
{
    if (m_stationVaxLevelsWanted == wanted) {
        return;
    }
    m_stationVaxLevelsWanted = wanted;
    if (stationVaxAvailable() && isHandshakeComplete()) {
        sendStationVaxLevelsSubscription(wanted);
    }
}

void StationClient::sendStationVaxLevelsSubscription(bool subscribe)
{
    const MirrorUpdate stream{0, "stream", MirrorWireKind::Utf8,
                              QVariant(QString::fromLatin1(StationVax::kLevelsStream))};
    if (subscribe) {
        invokeCommand("records.subscribe",
                      {stream, MirrorUpdate{0, "backlog", MirrorWireKind::Int64,
                                            QVariant(static_cast<qlonglong>(1))}});
    } else {
        invokeCommand("records.unsubscribe", {stream});
    }
}

void StationClient::sendCfcCompressionSubscription(bool subscribe)
{
    const MirrorUpdate stream{0, "stream", MirrorWireKind::Utf8,
                              QVariant(QString::fromLatin1(TransmitState::kCfcStream))};
    if (subscribe) {
        invokeCommand("records.subscribe",
                      {stream, MirrorUpdate{0, "backlog", MirrorWireKind::Int64,
                                            QVariant(static_cast<qlonglong>(1))}});
    } else {
        invokeCommand("records.unsubscribe", {stream});
    }
}

bool StationClient::stationFreedvAvailable() const
{
    return spotSourcesAvailable() && m_capabilities.stationFreedvVersion >= 1;
}

StationClient::CommandOutcome StationClient::requestFreedv(const QByteArray& verb,
                                                           const QVariantMap& args)
{
    if (!stationFreedvAvailable()) {
        return {false, stationLinkReady() ? stationFreedvUnavailableReason()
                                          : QStringLiteral("Not connected to the Core, so the "
                                                           "FreeDV Reporter request was not "
                                                           "sent.")};
    }
    QList<MirrorUpdate> arguments;
    if (verb == "freedv.setMessage") {
        arguments.append(stringArgument("text", args.value(QStringLiteral("text")).toString()));
    } else if (verb == "freedv.sendQsy") {
        arguments.append(
            stringArgument("callsign", args.value(QStringLiteral("callsign")).toString()));
        arguments.append(MirrorUpdate{
            0, "frequencyHz", MirrorWireKind::Int64,
            QVariant(static_cast<qlonglong>(args.value(QStringLiteral("frequencyHz")).toLongLong()))});
    } else if (verb == "freedv.setHidden") {
        arguments.append(boolArgument("on", args.value(QStringLiteral("on")).toBool()));
    } else {
        return {false, QStringLiteral("This app does not know that FreeDV Reporter request.")};
    }
    return sendCommand(verb, -1, arguments, QStringLiteral("the FreeDV Reporter request"));
}

bool StationClient::txModMonitorAvailable() const
{
    return spotSourcesAvailable() && m_capabilities.txModMonitorVersion >= 1;
}

void StationClient::setModMonitorSource(int source)
{
    m_modMonitorSource = source == 0 || source == 1 ? source : -1;
    syncModMonitorSubscription();
}

void StationClient::syncModMonitorSubscription()
{
    const QString wanted = txModMonitorAvailable()
        ? ModMonitorRecord::streamName(m_modMonitorSource)
        : QString();
    if (wanted == m_modMonitorStream) {
        return;
    }
    const auto streamArgument = [](const QString& stream) {
        return MirrorUpdate{0, "stream", MirrorWireKind::Utf8, QVariant(stream)};
    };
    if (!m_modMonitorStream.isEmpty() && stationLinkReady()) {
        invokeCommand("records.unsubscribe", {streamArgument(m_modMonitorStream)});
    }
    // The readings of a source the window no longer watches leave it.
    if (!m_radioModel.isNull()) {
        m_radioModel->clearStationModMonitor();
    }
    m_modMonitorStream = wanted;
    if (!wanted.isEmpty()) {
        invokeCommand("records.subscribe",
                      {streamArgument(wanted),
                       MirrorUpdate{0, "backlog", MirrorWireKind::Int64,
                                    QVariant(static_cast<qlonglong>(ModMonitorRecord::kCapacity))}});
    }
}

StationClient::CommandOutcome StationClient::requestModMonitorReset(int source)
{
    if (!txModMonitorAvailable()) {
        return {false, stationLinkReady() ? modMonitorUnavailableReason()
                                          : QStringLiteral("Not connected to the Core, so the "
                                                           "reset was not sent.")};
    }
    if (source != 0 && source != 1) {
        return {false, QStringLiteral("This app does not know that monitor source.")};
    }
    return sendCommand("txModMonitor.reset", -1,
                       {MirrorUpdate{0, "source", MirrorWireKind::Int64,
                                     QVariant(static_cast<qlonglong>(source))}},
                       QStringLiteral("the modulation monitor reset"));
}

bool StationClient::stationRadiosAvailable() const
{
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.stationRadiosVersion >= 1;
}

bool StationClient::settingsHygieneAvailable() const
{
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.settingsHygieneVersion >= 1 && m_settingsSnapshotThisLink;
}

bool StationClient::settingsRepairAvailable() const
{
    return settingsHygieneAvailable() && m_capabilities.settingsHygieneVersion >= 2;
}

bool StationClient::settingsBackupExportAvailable() const
{
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.settingsBackupVersion >= 1 && m_settingsSnapshotThisLink
        && signedInWithDeviceKey();
}

StationClient::CommandOutcome StationClient::requestSettingsBackupExport()
{
    if (!settingsBackupExportAvailable()) {
        return {false, QStringLiteral("This Core does not offer paired settings export to this app.")};
    }
    if (m_settingsBackupExport || m_cancelledSettingsBackupBegin) {
        return {false, QStringLiteral("A settings export is already in progress.")};
    }
    const quint32 id = m_nextCommandId++;
    if (m_nextCommandId == 0) ++m_nextCommandId;
    SettingsBackupExportPending pending;
    pending.operationId = id;
    pending.expectedCommandId = id;
    pending.sessionEpoch = m_sessionEpoch;
    pending.expectedVerb = "station.settingsExport.begin";
    m_settingsBackupExport.emplace(std::move(pending));
    m_settingsBackupReplyTimer->start(10000);
    m_settingsBackupOverallTimer->start(120000);
    // Pending is installed first: an in-process transport may answer in send().
    send(SessionMessages::commandInvoke("station.settingsExport.begin", id, {}));
    return {true, {}, id};
}

void StationClient::cancelSettingsBackupExport(quint32 operationId)
{
    if (m_settingsBackupExport
        && (operationId == 0 || operationId == m_settingsBackupExport->operationId)) {
        finishSettingsBackupExport(false, QStringLiteral("Settings export canceled."), {}, true);
    }
}

void StationClient::finishSettingsBackupExport(bool accepted, const QString& reason,
                                               const QByteArray& xml, bool cancelRemote)
{
    if (!m_settingsBackupExport) return;
    const QPointer<StationClient> self(this);
    const QPointer<RadioModel> model(m_radioModel);
    const SettingsBackupExportPending pending = std::move(*m_settingsBackupExport);
    m_settingsBackupExport.reset();
    m_settingsBackupReplyTimer->stop();
    m_settingsBackupOverallTimer->stop();
    if (cancelRemote && pending.transferId.isEmpty()
        && pending.expectedVerb == "station.settingsExport.begin"
        && pending.sessionEpoch == m_sessionEpoch && stationLinkReady()) {
        m_cancelledSettingsBackupBegin = qMakePair(pending.expectedCommandId, pending.sessionEpoch);
        QTimer::singleShot(10000, this, [this, id = pending.expectedCommandId,
                                          epoch = pending.sessionEpoch]() {
            if (m_cancelledSettingsBackupBegin
                && *m_cancelledSettingsBackupBegin == qMakePair(id, epoch)) {
                m_cancelledSettingsBackupBegin.reset();
            }
        });
    }
    if (cancelRemote && !pending.transferId.isEmpty()
        && pending.sessionEpoch == m_sessionEpoch && stationLinkReady()) {
        const quint32 id = m_nextCommandId++;
        if (m_nextCommandId == 0) ++m_nextCommandId;
        send(SessionMessages::commandInvoke("station.settingsExport.cancel", id,
             {{0, "transferId", MirrorWireKind::Utf8,
               QString::fromLatin1(pending.transferId)}}));
        if (!self) return;
    }
    if (model) model->reportStationSettingsBackupExportFinished(
        pending.operationId, accepted, reason, accepted ? xml : QByteArray());
}

void StationClient::requestNextSettingsBackupChunk()
{
    if (!m_settingsBackupExport || !settingsBackupExportAvailable()
        || m_settingsBackupExport->sessionEpoch != m_sessionEpoch) {
        finishSettingsBackupExport(false, QStringLiteral("The Core connection changed."));
        return;
    }
    const qint64 offset = m_settingsBackupExport->assembler.expectedOffset();
    const quint32 id = m_nextCommandId++;
    if (m_nextCommandId == 0) ++m_nextCommandId;
    m_settingsBackupExport->expectedCommandId = id;
    m_settingsBackupExport->expectedVerb = "station.settingsExport.read";
    const QByteArray transferId = m_settingsBackupExport->transferId;
    m_settingsBackupReplyTimer->start(10000);
    send(SessionMessages::commandInvoke("station.settingsExport.read", id,
         {{0, "transferId", MirrorWireKind::Utf8, QString::fromLatin1(transferId)},
          {0, "offset", MirrorWireKind::Int64, QVariant(static_cast<qlonglong>(offset))}}));
}

void StationClient::handleSettingsBackupExportResult(const SessionMessage& message)
{
    if (!m_settingsBackupExport || message.commandId != m_settingsBackupExport->expectedCommandId
        || message.commandVerb != m_settingsBackupExport->expectedVerb
        || m_settingsBackupExport->sessionEpoch != m_sessionEpoch) {
        if (m_settingsBackupExport
            && message.commandId == m_settingsBackupExport->expectedCommandId) {
            finishSettingsBackupExport(false, QStringLiteral("Malformed settings export reply."), {}, true);
        }
        return;
    }
    m_settingsBackupReplyTimer->stop();
    if (!message.accepted) {
        finishSettingsBackupExport(false,
            message.reason.isEmpty() ? QStringLiteral("The Core refused settings export.")
                                     : message.reason);
        return;
    }
    const auto& values = message.updates;
    const auto textAt = [&values](int index, const QByteArray& name, qsizetype max,
                                  QString* out) -> bool {
        if (index >= values.size()) return false;
        const MirrorUpdate& v = values.at(index);
        if (v.ordinal != 0 || v.name != name || v.kind != MirrorWireKind::Utf8
            || v.value.typeId() != QMetaType::QString) return false;
        const QString s = v.value.toString();
        if (s.toUtf8().size() > max) return false;
        *out = s;
        return true;
    };
    const auto intAt = [&values](int index, const QByteArray& name, qint64* out) -> bool {
        if (index >= values.size()) return false;
        const MirrorUpdate& v = values.at(index);
        if (v.ordinal != 0 || v.name != name || v.kind != MirrorWireKind::Int64
            || v.value.typeId() != QMetaType::LongLong) return false;
        *out = v.value.toLongLong();
        return true;
    };
    const auto fail = [this]() {
        finishSettingsBackupExport(false, QStringLiteral("Malformed settings export reply."), {}, true);
    };
    if (!message.affectedKeys.isEmpty() || !message.reason.isEmpty() || values.size() != 3) {
        fail(); return;
    }
    if (message.commandVerb == "station.settingsExport.begin") {
        QString id, digest;
        qint64 length = 0;
        static const QRegularExpression idPattern(QStringLiteral("^[0-9a-f]{32}$"));
        static const QRegularExpression digestPattern(QStringLiteral("^[0-9a-f]{64}$"));
        if (!textAt(0, "transferId", 32, &id)
            || !intAt(1, "byteLength", &length)
            || !textAt(2, "sha256", 64, &digest)
            || !idPattern.match(id).hasMatch() || !digestPattern.match(digest).hasMatch()) {
            fail(); return;
        }
        SettingsBackupTransferManifest manifest{length, QByteArray::fromHex(digest.toLatin1())};
        QString error;
        if (!m_settingsBackupExport->assembler.begin(manifest, &error)) {
            fail(); return;
        }
        m_settingsBackupExport->transferId = id.toLatin1();
        const quint32 epoch = m_sessionEpoch;
        const quint32 operation = m_settingsBackupExport->operationId;
        QTimer::singleShot(0, this, [this, epoch, operation]() {
            if (m_settingsBackupExport && m_sessionEpoch == epoch
                && m_settingsBackupExport->operationId == operation) {
                requestNextSettingsBackupChunk();
            }
        });
        return;
    }
    QString id, data;
    qint64 offset = -1;
    constexpr qsizetype kEncodedMax = ((SettingsBackupTransferSource::kMaxChunkBytes + 2) / 3) * 4;
    if (!textAt(0, "transferId", 32, &id)
        || !intAt(1, "offset", &offset)
        || !textAt(2, "data", kEncodedMax, &data)
        || id.toLatin1() != m_settingsBackupExport->transferId
        || offset != m_settingsBackupExport->assembler.expectedOffset()) {
        fail(); return;
    }
    const QByteArray encoded = data.toLatin1();
    const QByteArray chunk = QByteArray::fromBase64(encoded);
    if (encoded.isEmpty() || chunk.isEmpty() || chunk.size() > SettingsBackupTransferSource::kMaxChunkBytes
        || chunk.toBase64() != encoded || QString::fromLatin1(encoded) != data) {
        fail(); return;
    }
    QString error;
    if (!m_settingsBackupExport->assembler.acceptChunk(offset, chunk, &error)) {
        fail(); return;
    }
    QByteArray xml;
    if (m_settingsBackupExport->assembler.completedPayload(&xml)) {
        if (!AppSettings::validateLocalXml(xml, &error)) {
            finishSettingsBackupExport(false, error, {}, true);
        } else {
            finishSettingsBackupExport(true, {}, xml);
        }
        return;
    }
    const quint32 epoch = m_sessionEpoch;
    const quint32 operation = m_settingsBackupExport->operationId;
    QTimer::singleShot(0, this, [this, epoch, operation]() {
        if (m_settingsBackupExport && m_sessionEpoch == epoch
            && m_settingsBackupExport->operationId == operation) {
            requestNextSettingsBackupChunk();
        }
    });
}

StationClient::CommandOutcome StationClient::requestSettingsHygiene(
    const QByteArray& verb, const QString& mac)
{
    if (!settingsHygieneAvailable()) {
        return {false, settingsHygieneUnavailableReason()};
    }
    const QString target = AppSettings::normalizedRadioMac(mac);
    if (target.isEmpty() || target != AppSettings::normalizedRadioMac(m_radioModel->currentRadioMac())) {
        return {false, QStringLiteral("No current radio matches this Settings Validation request.")};
    }
    if (verb != "station.validateSettings" && verb != "station.forgetSettings"
        && verb != "station.repairSettings") {
        return {false, QStringLiteral("This app does not know that Settings Validation request.")};
    }
    if (verb == "station.repairSettings" && !settingsRepairAvailable()) {
        return {false, settingsRepairUnavailableReason()};
    }
    if ((verb == "station.forgetSettings" || verb == "station.repairSettings")
        && !signedInWithDeviceKey()) {
        return {false, StationRadios::pairedDeviceReason()};
    }
    if (verb != "station.validateSettings") {
        QString onAir;
        if (m_radioModel->stationOnAirRefusal(&onAir)) { return {false, onAir}; }
    }
    if (verb == "station.validateSettings" && m_hygieneValidateId != 0) {
        m_hygieneValidateDirty = true;
        return {true, {}, m_hygieneValidateId};
    }
    const quint32 id = invokeCommand(verb, {stringArgument("mac", target)});
    if (id == 0) { return {false, QStringLiteral("Not connected to the Core.")}; }
    if (verb == "station.validateSettings") {
        m_hygieneValidateId = id;
        m_hygieneValidateEpoch = m_sessionEpoch;
        m_hygieneValidateMac = target;
        m_hygieneValidateDirty = false;
    } else {
        m_hygieneMutations.insert(id, {m_sessionEpoch, target});
    }
    return {true, {}, id};
}

void StationClient::refreshSettingsHygiene()
{
    if (m_radioModel.isNull()) { return; }
    if (!settingsHygieneAvailable() || m_radioModel->currentRadioMac().isEmpty()) {
        m_radioModel->settingsHygiene().setRemoteUnavailable(
            m_radioModel->currentRadioMac().isEmpty()
                ? QStringLiteral("Connect a radio on the Core to validate its settings.")
                : settingsHygieneUnavailableReason());
        return;
    }
    const QPointer<StationClient> self(this);
    m_radioModel->settingsHygiene().setRemoteUnavailable(
        QStringLiteral("Checking this radio's settings on the Core."));
    if (!self || m_radioModel.isNull()) { return; }
    requestSettingsHygiene("station.validateSettings", m_radioModel->currentRadioMac());
}

void StationClient::handleSettingsHygieneResult(const SessionMessage& message)
{
    const QPointer<StationClient> self(this);
    const bool validation = message.commandVerb == "station.validateSettings";
    const bool matchingValidation = validation && m_hygieneValidateId == message.commandId
        && m_hygieneValidateEpoch == m_sessionEpoch;
    const auto mutation = m_hygieneMutations.take(message.commandId);
    const bool matchingMutation = !validation && mutation.first == m_sessionEpoch
        && !mutation.second.isEmpty();
    if (!matchingValidation && !matchingMutation) { return; }
    const QString target = validation ? m_hygieneValidateMac : mutation.second;
    const bool dirty = m_hygieneValidateDirty;
    if (matchingValidation) {
        m_hygieneValidateId = 0;
        m_hygieneValidateDirty = false;
        m_hygieneValidateMac.clear();
    }
    if (!m_radioModel.isNull() && settingsHygieneAvailable()
        && target == AppSettings::normalizedRadioMac(m_radioModel->currentRadioMac())) {
        const auto reply = message.accepted ? SettingsHygieneWire::decode(message.updates)
                                            : std::nullopt;
        if (reply && reply->mac == target) {
            m_radioModel->settingsHygiene().replaceRemoteIssues(reply->issues);
        } else {
            m_radioModel->settingsHygiene().setRemoteUnavailable(
                message.accepted ? QStringLiteral("The Core sent invalid Settings Validation details.")
                                 : message.reason);
        }
        m_radioModel->reportStationCommandFinished(message.commandId, message.accepted,
            message.accepted ? QString() : message.reason);
        if (!self) { return; }
    }
    emit commandResponse(message);
    if (!self) { return; }
    emit commandResult(message.commandId, message.accepted, message.reason);
    if (!self) { return; }
    if (matchingValidation && dirty) { refreshSettingsHygiene(); }
}

bool StationClient::deviceAdminAvailable() const
{
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.deviceAdminVersion >= 1 && signedInWithDeviceKey();
}

bool StationClient::pairingAvailable() const
{
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.pairingVersion >= 1 && signedInWithDeviceKey();
}

StationClient::CommandOutcome StationClient::requestDeviceAdmin(const QByteArray& verb,
                                                                const QString& id)
{
    const bool pairing = verb == "pairing.open" || verb == "pairing.close";
    if (pairing ? !pairingAvailable() : !deviceAdminAvailable()) {
        if (!stationLinkReady()) {
            return {false, QStringLiteral("Not connected to the Core, so the request was not "
                                          "sent.")};
        }
        return {false, signedInWithDeviceKey() ? deviceAdminUnavailableReason()
                                               : pairedDeviceAdminReason()};
    }
    QList<MirrorUpdate> arguments;
    if (verb == "devices.revoke") {
        arguments.append(stringArgument("id", id));
    }
    return sendCommand(verb, -1, arguments, QStringLiteral("the device request"));
}

StationClient::CommandOutcome StationClient::requestStationRadio(const QByteArray& verb,
                                                                 const QString& mac, int model)
{
    if (!stationRadiosAvailable()) {
        return {false, stationLinkReady() ? stationRadiosUnavailableReason()
                                          : QStringLiteral("Not connected to the Core, so the "
                                                           "radio request was not sent.")};
    }
    QList<MirrorUpdate> arguments;
    if (verb != "station.rescanRadios") {
        arguments.append(stringArgument("mac", mac));
    }
    if (verb == "station.setRadioModel") {
        arguments.append(intArgument("model", model));
    }
    return sendCommand(verb, -1, arguments, QStringLiteral("the radio request"));
}

StationClient::CommandOutcome StationClient::requestFilterResponse(int sliceId,
                                                                  bool highResolution)
{
    if (!dspInfoAvailable()) {
        return {false, filterResponseUnavailableReason()};
    }
    return sendCommand("dsp.filterResponse", sliceId,
                       { intArgument("sliceId", sliceId),
                         boolArgument("highResolution", highResolution) },
                       QStringLiteral("the filter curve request"));
}

StationClient::CommandOutcome StationClient::requestIoBoardOutput(int pin, bool on)
{
    if (!radioHardwareAvailable(7)) {
        return {false, remoteHardwareConfigAvailable() ? ioBoardI2cUnavailableReason()
                                                       : hardwareConfigUnavailableReason()};
    }
    return sendCommand("setIoBoardOutput", -1,
                       { intArgument("pin", pin), boolArgument("on", on) },
                       QStringLiteral("the I/O board output change"));
}

StationClient::CommandOutcome StationClient::requestIoBoardProbe()
{
    if (!remoteHardwareConfigAvailable()) {
        return {false, hardwareConfigUnavailableReason()};
    }
    return sendCommand("requestIoBoardProbe", -1, {},
                       QStringLiteral("the I/O board probe"));
}

bool StationClient::nnrControlAvailable() const
{
    return propertyResultsAvailable() && m_capabilities.nnrVersion > 0;
}

StationClient::CommandOutcome StationClient::requestNnrDiagnostics(int sliceId, int testMode, int outputMode)
{
    if (!nnrControlAvailable()) {
        return {false, QStringLiteral("The station does not support NNR diagnostics.")};
    }
    return sendCommand("nnr.setDiagnostics", sliceId,
        {intArgument("sliceId", sliceId), intArgument("testMode", testMode), intArgument("outputMode", outputMode)},
        QStringLiteral("the NNR diagnostic change"));
}

bool StationClient::nnrRetryAvailable() const
{
    return nnrControlAvailable() && m_agreedMinor >= kNnrLimitSessionProtocolMinor;
}

StationClient::CommandOutcome StationClient::requestNnrRetry(int sliceId)
{
    if (!nnrRetryAvailable()) {
        return {false, QStringLiteral("This station cannot try noise reduction again. "
                                      "Update the station software.")};
    }
    return sendCommand("nnr.tryAgain", sliceId, {intArgument("sliceId", sliceId)},
        QStringLiteral("trying noise reduction again"));
}

StationClient::CommandOutcome StationClient::requestConfigurePgxl(const QString& host, quint16 port)
{
    if (!remotePgxlControlAvailable()) {
        return { false, QStringLiteral("The station does not support remote PGXL configuration.") };
    }
    return sendCommand("configurePgxl", -1,
                       { stringArgument("host", host), intArgument("port", port) },
                       QStringLiteral("the Power Genius address"));
}

StationClient::CommandOutcome StationClient::requestDisconnectPgxl()
{
    if (!remotePgxlControlAvailable()) {
        return { false, QStringLiteral("The station does not support remote PGXL configuration.") };
    }
    return sendCommand("disconnectPgxl", -1, {}, QStringLiteral("the Power Genius disconnect"));
}

StationClient::CommandOutcome StationClient::requestPgxlConnectionSettings(bool autoReconnect,
                                                                           int keepaliveSec,
                                                                           int pingSec)
{
    if (!remotePgxlControlAvailable()) {
        return { false, QStringLiteral("The station does not support remote PGXL configuration.") };
    }
    return sendCommand("setPgxlConnectionSettings", -1,
                       { boolArgument("autoReconnect", autoReconnect),
                         intArgument("keepaliveSec", keepaliveSec),
                         intArgument("pingSec", pingSec) },
                       QStringLiteral("the Power Genius connection settings"));
}

// R-R3-47 / R-R3-22 (remoteRfKitControlVersion 2): the Core's RF-Kit.
StationClient::CommandOutcome StationClient::requestConfigureRfKit(const QString& host, quint16 port)
{
    if (!remoteRfKitControlAvailable()) {
        return { false, QStringLiteral("This Core does not offer RF-Kit amplifier setup to this app.") };
    }
    return sendCommand("configureRfKit", -1,
                       { stringArgument("host", host), intArgument("port", port) },
                       QStringLiteral("the RF-Kit amplifier address"));
}

bool StationClient::rfKitSettingsAvailable() const
{
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.remoteRfKitControlVersion >= 3;
}

StationClient::CommandOutcome StationClient::requestResetRfKitError()
{
    if (!rfKitSettingsAvailable()) {
        return IStationLink::requestResetRfKitError();
    }
    return sendCommand("resetRfKitError", -1, {},
                       QStringLiteral("the RF-Kit amplifier's error reset"));
}

StationClient::CommandOutcome StationClient::requestDisconnectRfKit()
{
    if (!remoteRfKitControlAvailable()) {
        return { false, QStringLiteral("This Core does not offer RF-Kit amplifier setup to this app.") };
    }
    return sendCommand("disconnectRfKit", -1, {}, QStringLiteral("the RF-Kit amplifier disconnect"));
}

StationClient::CommandOutcome StationClient::requestRfKitEnabled(bool enabled)
{
    if (!remoteRfKitControlAvailable()) {
        return { false, QStringLiteral("This Core does not offer RF-Kit amplifier setup to this app.") };
    }
    return sendCommand("setRfKitEnabled", -1, { boolArgument("enabled", enabled) },
                       QStringLiteral("the RF-Kit amplifier switch"));
}

// R-R3-49 (parity Task 10): a Core below remoteRfKitControlVersion 4 is not
// asked; the window says why in its own words.
StationClient::CommandOutcome StationClient::requestRfKitOperate(bool on)
{
    if (!rfKitFullControlAvailable()) {
        return IStationLink::requestRfKitOperate(on);
    }
    return sendCommand("setRfKitOperate", -1, { boolArgument("on", on) },
                       QStringLiteral("the RF-Kit amplifier operate"));
}

StationClient::CommandOutcome StationClient::requestRfKitAntenna(int port)
{
    if (!rfKitFullControlAvailable()) {
        return IStationLink::requestRfKitAntenna(port);
    }
    return sendCommand("setRfKitAntenna", -1, { intArgument("port", port) },
                       QStringLiteral("the RF-Kit amplifier antenna"));
}

StationClient::CommandOutcome StationClient::requestRfKitTciMode()
{
    if (!rfKitFullControlAvailable()) {
        return IStationLink::requestRfKitTciMode();
    }
    return sendCommand("setRfKitTciMode", -1, {},
                       QStringLiteral("the RF-Kit amplifier TCI mode"));
}

StationClient::CommandOutcome StationClient::requestRfKitAddress(const QString& host, int port)
{
    if (!rfKitFullControlAvailable()) {
        return IStationLink::requestRfKitAddress(host, port);
    }
    return sendCommand("setRfKitAddress", -1,
                       { stringArgument("host", host), intArgument("port", port) },
                       QStringLiteral("the RF-Kit amplifier address"));
}

// R-R3-48 (stationTciVersion 1): the one TCI switch and port.
StationClient::CommandOutcome StationClient::requestStationTci(bool enabled, quint16 port)
{
    if (!stationTciAvailable()) {
        return { false, QStringLiteral("This Core has no TCI server.") };
    }
    return sendCommand("setStationTci", -1,
                       { boolArgument("enabled", enabled), intArgument("port", port) },
                       QStringLiteral("the Core's TCI server switch"));
}

// Parity Task 23 (stationTciVersion 2): the Core's station TCI server's
// options and apps.
bool StationClient::stationTciServerAvailable() const
{
    return stationTciAvailable() && m_capabilities.stationTciVersion >= 2;
}

StationClient::CommandOutcome StationClient::requestStationTciOptions(bool emulateExpertSdr3,
                                                                      bool emulateSunSdr2Pro,
                                                                      bool cwluBecomesCw,
                                                                      bool sendInitialState)
{
    if (!stationTciServerAvailable()) {
        return { false, stationTciServerUnavailableReason() };
    }
    return sendCommand("setStationTciOptions", -1,
                       { boolArgument("emulateExpertSdr3", emulateExpertSdr3),
                         boolArgument("emulateSunSdr2Pro", emulateSunSdr2Pro),
                         boolArgument("cwluBecomesCw", cwluBecomesCw),
                         boolArgument("sendInitialState", sendInitialState) },
                       QStringLiteral("the Core's TCI server settings"));
}

bool StationClient::stationTciSettingsAvailable() const
{
    // JJ's ruling of 2026-09-28: a Core that told this window
    // stationTciSettingsVersion 1 (it declares stationTciSettings).
    return stationTciServerAvailable() && m_capabilities.stationTciSettingsVersion >= 1;
}

StationClient::CommandOutcome StationClient::requestStationTciSetting(const QByteArray& name,
                                                                      const QVariant& value)
{
    if (!stationTciSettingsAvailable()) {
        return { false, stationTciServerUnavailableReason() };
    }
    const StationTciModel::Setting* setting = StationTciModel::setting(name);
    if (setting == nullptr) {
        return { false, QStringLiteral("The Core's TCI server has no such setting.") };
    }
    return sendCommand("setStationTciSettings", -1,
                       { setting->kind == StationTciModel::Setting::Kind::Bool
                             ? boolArgument(name, value.toBool())
                             : intArgument(name, value.toInt()) },
                       QStringLiteral("the Core's TCI server settings"));
}

StationClient::CommandOutcome StationClient::requestDisconnectStationTciClient(const QString& id)
{
    if (!stationTciServerAvailable()) {
        return { false, stationTciServerUnavailableReason() };
    }
    return sendCommand("disconnectStationTciClient", -1,
                       { MirrorUpdate{0, "id", MirrorWireKind::Utf8, QVariant(id)} },
                       QStringLiteral("an app on the Core's TCI server"));
}

// R-R3-47 / R-R3-22 (accessoryDataVersion 1): the Core's accessory records
// and settings.
StationClient::CommandOutcome StationClient::requestTxInterlockPolicy(int mode, int graceMs,
                                                                      bool swrGateEnabled,
                                                                      double swrGateMax)
{
    if (!accessoryDataAvailable()) {
        return IStationLink::requestTxInterlockPolicy(mode, graceMs, swrGateEnabled, swrGateMax);
    }
    return sendCommand("setTxInterlockPolicy", -1,
                       { intArgument("mode", mode), intArgument("graceMs", graceMs),
                         boolArgument("swrGateEnabled", swrGateEnabled),
                         doubleArgument("swrGateMax", swrGateMax) },
                       QStringLiteral("the transmit interlock"));
}

StationClient::CommandOutcome StationClient::requestPgxlPowerCap(bool enabled, int watts)
{
    if (!accessoryDataAvailable()) {
        return IStationLink::requestPgxlPowerCap(enabled, watts);
    }
    return sendCommand("setPgxlPowerCap", -1,
                       { boolArgument("enabled", enabled), intArgument("watts", watts) },
                       QStringLiteral("the Power Genius output limit"));
}

StationClient::CommandOutcome StationClient::requestClearAccessoryFaults(const QString& device)
{
    if (!accessoryDataAvailable()) {
        return IStationLink::requestClearAccessoryFaults(device);
    }
    return sendCommand("clearAccessoryFaults", -1, { stringArgument("device", device) },
                       QStringLiteral("the fault history"));
}

// R-R3-47 / R-R3-22 (remotePgxlControlVersion 3, remoteTgxlControlVersion
// 1): the amp's and tuner's own settings. A Core that did not offer them is
// not asked; the window says why.
StationClient::CommandOutcome StationClient::requestPgxlName(const QString& name)
{
    if (!pgxlDeviceSettingsAvailable()) {
        return IStationLink::requestPgxlName(name);
    }
    return sendCommand("setPgxlName", -1, { stringArgument("name", name) },
                       QStringLiteral("the Power Genius name"));
}

StationClient::CommandOutcome StationClient::requestPgxlHardware(const QString& setting,
                                                                 const QString& value)
{
    if (!pgxlDeviceSettingsAvailable()) {
        return IStationLink::requestPgxlHardware(setting, value);
    }
    if (setting == QLatin1String("ledIntensity")) {
        return sendCommand("setPgxlHardware", -1, { intArgument("ledIntensity", value.toInt()) },
                           QStringLiteral("the Power Genius hardware setting"));
    }
    return sendCommand("setPgxlHardware", -1, { stringArgument(setting.toUtf8(), value) },
                       QStringLiteral("the Power Genius hardware setting"));
}

StationClient::CommandOutcome StationClient::requestPgxlNetwork(bool dhcp, const QString& address,
                                                                const QString& netmask,
                                                                const QString& gateway)
{
    if (!pgxlDeviceSettingsAvailable()) {
        return IStationLink::requestPgxlNetwork(dhcp, address, netmask, gateway);
    }
    return sendCommand("setPgxlNetwork", -1,
                       { boolArgument("dhcp", dhcp), stringArgument("address", address),
                         stringArgument("netmask", netmask),
                         stringArgument("gateway", gateway) },
                       QStringLiteral("the Power Genius network settings"));
}

StationClient::CommandOutcome StationClient::requestPgxlSaveAndRestart()
{
    if (!pgxlDeviceSettingsAvailable()) {
        return IStationLink::requestPgxlSaveAndRestart();
    }
    return sendCommand("savePgxlSettings", -1, {},
                       QStringLiteral("the Power Genius Save & Reboot"));
}

StationClient::CommandOutcome StationClient::requestPgxlReadSettings()
{
    if (!pgxlDeviceSettingsAvailable()) {
        return IStationLink::requestPgxlReadSettings();
    }
    return sendCommand("readPgxlSettings", -1, {},
                       QStringLiteral("the request for the Power Genius settings"));
}

StationClient::CommandOutcome StationClient::requestTgxlName(const QString& name)
{
    if (!tgxlDeviceSettingsAvailable()) {
        return IStationLink::requestTgxlName(name);
    }
    return sendCommand("setTgxlName", -1, { stringArgument("name", name) },
                       QStringLiteral("the Tuner Genius name"));
}

StationClient::CommandOutcome StationClient::requestTgxlNetwork(bool dhcp, const QString& address,
                                                                const QString& netmask,
                                                                const QString& gateway)
{
    if (!tgxlDeviceSettingsAvailable()) {
        return IStationLink::requestTgxlNetwork(dhcp, address, netmask, gateway);
    }
    return sendCommand("setTgxlNetwork", -1,
                       { boolArgument("dhcp", dhcp), stringArgument("address", address),
                         stringArgument("netmask", netmask),
                         stringArgument("gateway", gateway) },
                       QStringLiteral("the Tuner Genius network settings"));
}

StationClient::CommandOutcome StationClient::requestTgxlSaveAndRestart()
{
    if (!tgxlDeviceSettingsAvailable()) {
        return IStationLink::requestTgxlSaveAndRestart();
    }
    return sendCommand("saveTgxlSettings", -1, {},
                       QStringLiteral("the Tuner Genius Save & Reboot"));
}

StationClient::CommandOutcome StationClient::requestTgxlReadSettings()
{
    if (!tgxlDeviceSettingsAvailable()) {
        return IStationLink::requestTgxlReadSettings();
    }
    return sendCommand("readTgxlSettings", -1, {},
                       QStringLiteral("the request for the Tuner Genius settings"));
}

// R-R3-49 / R-R3-47 (remoteTgxlControlVersion 2): the Tuner Genius's
// antenna, operate and bypass. A Core that did not offer them is not asked.
StationClient::CommandOutcome StationClient::requestTgxlAntenna(int port)
{
    if (!tgxlControlAvailable()) {
        return IStationLink::requestTgxlAntenna(port);
    }
    return sendCommand("setTgxlAntenna", -1, { intArgument("port", port) },
                       QStringLiteral("the Tuner Genius antenna"));
}

StationClient::CommandOutcome StationClient::requestTgxlOperate(bool on)
{
    if (!tgxlControlAvailable()) {
        return IStationLink::requestTgxlOperate(on);
    }
    return sendCommand("setTgxlOperate", -1, { boolArgument("on", on) },
                       QStringLiteral("the Tuner Genius operate switch"));
}

StationClient::CommandOutcome StationClient::requestTgxlBypass(bool on)
{
    if (!tgxlControlAvailable()) {
        return IStationLink::requestTgxlBypass(on);
    }
    return sendCommand("setTgxlBypass", -1, { boolArgument("on", on) },
                       QStringLiteral("the Tuner Genius bypass"));
}

// R-R3-49 (parity Task 8): a Core below remoteTgxlControlVersion 4 is not
// asked; the window says why in its own words.
StationClient::CommandOutcome StationClient::requestTgxlRelayMove(int relay, int direction)
{
    if (!tgxlFullControlAvailable()) {
        return IStationLink::requestTgxlRelayMove(relay, direction);
    }
    return sendCommand("moveTgxlRelay", -1,
                       { intArgument("relay", relay), intArgument("direction", direction) },
                       QStringLiteral("the Tuner Genius relay"));
}

StationClient::CommandOutcome StationClient::requestTgxlLanScan()
{
    if (!tgxlFullControlAvailable()) {
        return IStationLink::requestTgxlLanScan();
    }
    return sendCommand("scanTgxlLan", -1, {}, QStringLiteral("the Tuner Genius scan"));
}

StationClient::CommandOutcome StationClient::requestTgxlAddress(const QString& host, int port)
{
    if (!tgxlFullControlAvailable()) {
        return IStationLink::requestTgxlAddress(host, port);
    }
    return sendCommand("setTgxlAddress", -1,
                       { stringArgument("host", host), intArgument("port", port) },
                       QStringLiteral("the Tuner Genius address"));
}

// R-R3-49 (parity Task 9): a Core below remotePgxlControlVersion 4 is not
// asked; the window says why in its own words.
StationClient::CommandOutcome StationClient::requestPgxlOperate(bool on)
{
    if (m_capabilities.accessoryTxVersion >= 1) {
        return sendCommand(on ? "amp.operate" : "amp.standby", -1, {},
                           QStringLiteral("the Power Genius operate"));
    }
    if (!pgxlFullControlAvailable()) {
        return IStationLink::requestPgxlOperate(on);
    }
    return sendCommand("setPgxlOperate", -1, { boolArgument("on", on) },
                       QStringLiteral("the Power Genius operate"));
}

StationClient::CommandOutcome StationClient::requestPgxlLanScan()
{
    if (!pgxlFullControlAvailable()) {
        return IStationLink::requestPgxlLanScan();
    }
    return sendCommand("scanPgxlLan", -1, {}, QStringLiteral("the Power Genius scan"));
}

StationClient::CommandOutcome StationClient::requestPgxlAddress(const QString& host, int port)
{
    if (!pgxlFullControlAvailable()) {
        return IStationLink::requestPgxlAddress(host, port);
    }
    return sendCommand("setPgxlAddress", -1,
                       { stringArgument("host", host), intArgument("port", port) },
                       QStringLiteral("the Power Genius address"));
}

// R-R3-49 (parity Task 2): the TX applet's Tune Power slider. A Core
// below transmitSettingsVersion 2 is not asked.
StationClient::CommandOutcome StationClient::requestTunePowerForTxBand(int watts)
{
    if (!transmitSettingsAvailable(2)) {
        return IStationLink::requestTunePowerForTxBand(watts);
    }
    const CommandOutcome outcome =
        sendCommand("setTunePowerForTxBand", -1, { intArgument("watts", watts) },
                    QStringLiteral("the tune power"));
    // Fix wave GUI-I6: on its way until the Core answers it.
    if (outcome.sent && !m_radioModel.isNull()) {
        m_radioModel->transmitModel().setTunePowerForTxBandWriteInFlight(true);
    }
    return outcome;
}

// transmitSettingsVersion 15: the CFC dialog's band editor, applied by the
// Core at once against the revision the window last saw. A Core below 15
// is not asked; the dialog writes the CFC properties one by one instead.
StationClient::CommandOutcome StationClient::requestCfcProfile(const QString& profileJson,
                                                               const QString& expectedRevision)
{
    if (!transmitSettingsAvailable(kTransmitSettingsCfcProfileVersion)) {
        return IStationLink::requestCfcProfile(profileJson, expectedRevision);
    }
    return sendCommand("cfc.setProfile", -1,
                       { stringArgument("profileJson", profileJson),
                         stringArgument("expectedRevision", expectedRevision) },
                       QStringLiteral("the CFC settings"));
}

// R-R3-49 (parity Task 3): the TX profile combos, Setup > Audio > TX
// Profile and the RADE applet's Reset vocoder. A Core below
// transmitSettingsVersion 3 is not asked.
StationClient::CommandOutcome StationClient::requestTxProfileSelect(const QString& name)
{
    if (!transmitSettingsAvailable(3)) {
        return IStationLink::requestTxProfileSelect(name);
    }
    return sendCommand("txProfile.select", -1, { stringArgument("name", name) },
                       QStringLiteral("the transmit profile"));
}

StationClient::CommandOutcome StationClient::requestTxProfileSave(const QString& name)
{
    if (!transmitSettingsAvailable(3)) {
        return IStationLink::requestTxProfileSave(name);
    }
    return sendCommand("txProfile.save", -1, { stringArgument("name", name) },
                       QStringLiteral("the transmit profile"));
}

StationClient::CommandOutcome StationClient::requestTxProfileDelete(const QString& name)
{
    if (!transmitSettingsAvailable(3)) {
        return IStationLink::requestTxProfileDelete(name);
    }
    return sendCommand("txProfile.delete", -1, { stringArgument("name", name) },
                       QStringLiteral("the transmit profile"));
}

StationClient::CommandOutcome StationClient::requestRadeResetVocoder()
{
    if (!transmitSettingsAvailable(3)) {
        return IStationLink::requestRadeResetVocoder();
    }
    return sendCommand("rade.resetVocoder", -1, {}, QStringLiteral("the RADE vocoder reset"));
}

StationClient::CommandOutcome StationClient::requestDisconnectTgxl()
{
    if (!remoteTgxlConfigAvailable()) {
        return { false, QStringLiteral("The station does not support remote TGXL configuration.") };
    }
    return sendCommand("disconnectTgxl", -1, {}, QStringLiteral("the TGXL disconnect"));
}

StationClient::CommandOutcome StationClient::requestFourO3AEnabled(bool enabled)
{
    if (!remoteFourO3AControlAvailable()) {
        return { false, QStringLiteral("The station does not support remote 4O3A control.") };
    }
    return sendCommand("setFourO3AEnabled", -1, { boolArgument("enabled", enabled) },
                       QStringLiteral("the 4O3A master change"));
}

bool StationClient::remoteTransmitAvailable() const
{
    return m_sessionPurpose == SessionPurpose::Ordinary && stationLinkReady()
        && m_agreedMinor >= kRadioIdentitySessionProtocolMinor && m_capabilities.remoteTxVersion >= 1;
}

bool StationClient::directWatchReady() const
{
    return !m_watchIsRelay && m_directWatch != nullptr && m_directWatch->isReady()
        && m_directWatch->generation() == m_directWatchGeneration
        && directWatchEligible();
}

bool StationClient::transmitWatchReady() const
{
    return m_directWatch != nullptr && m_directWatch->isReady()
        && m_directWatch->generation() == m_directWatchGeneration
        && (m_watchIsRelay ? relayWatchEligible(false) : directWatchEligible());
}

bool StationClient::directWatchEligible() const
{
    const auto* direct = qobject_cast<const WebSocketTransport*>(transport());
    return remoteTransmitAvailable() && m_capabilities.txPermitted
        && m_capabilities.txWatchPathVersion == 1 && m_directWatchDeclared
        && m_signedInWithDeviceKey && m_stationIdentity.size() == 32
        && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && !m_upgrade && (sessionTransport() == nullptr || !sessionTransport()->switching())
        && direct != nullptr && direct->isOpen()
        && direct->peerCertificateSha256().size() == 32
        && m_connectedUrl.isValid() && m_connectedUrl.scheme() == QLatin1String("wss")
        && !m_connectedUrl.host().isEmpty()
        && !m_connectedUrl.authority(QUrl::FullyEncoded).contains('@');
}

bool StationClient::relayWatchEligible(bool newAdmission) const
{
    const auto* relay = qobject_cast<const DataChannelTransport*>(transport());
    return remoteTransmitAvailable() && m_capabilities.txPermitted
        && m_capabilities.txWatchPathVersion == 1 && m_directWatchDeclared
        && m_watchRelayDeclared && m_signedInWithDeviceKey
        && m_stationIdentity.size() == 32
        && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && !m_upgrade && (sessionTransport() == nullptr || !sessionTransport()->switching())
        && relay != nullptr && relay->peerCertificateSha256().size() == 32
        && (newAdmission ? relay->canOpenWatchRelay() : relay->hasWatchRelayRoute());
}

void StationClient::retireDirectWatch()
{
    ++m_directWatchGeneration;
    m_watchPreparing = false;
    m_watchIsRelay = false;
    m_directWatchTicketId = 0;
    m_directWatchTicketGeneration = 0;
    m_directWatchTicketSessionEpoch = 0;
    if (m_directWatchRetryTimer) { m_directWatchRetryTimer->stop(); }
    if (m_directWatchTicketTimer) { m_directWatchTicketTimer->stop(); }
    const QPointer<TxWatchClient> watch = m_directWatch;
    m_directWatch = nullptr;
    if (watch) {
        // Detach first, then fold once while the final attempt is still
        // readable. Its close callbacks cannot add these bytes again.
        const AuxiliaryWatchTelemetry final = watch->telemetry();
        m_retiredWatchTelemetry.receivedPayloadBytes += final.receivedPayloadBytes;
        m_retiredWatchTelemetry.submittedPayloadBytes += final.submittedPayloadBytes;
    }
    const QPointer<DataChannelTransport> pending = m_pendingWatchRelayPeer;
    m_pendingWatchRelayPeer = nullptr;
    const std::shared_ptr<RelayLeg> leg = std::move(m_watchRelayLeg);
    if (pending) {
        pending->disconnect(this);
        pending->setParent(nullptr);
        pending->closeLink(QStringLiteral("watch retired"));
        if (pending) { pending->deleteLater(); }
    }
    if (watch != nullptr) {
        // A replacement must not deliver an old close into any callback.
        watch->disconnect();
        watch->close();
        if (watch) { watch->deleteLater(); }
    }
    if (leg) {
        leg->disconnect(this);
        leg->close();
    }
}

void StationClient::retryDirectWatch(const QString& reason)
{
    if ((!directWatchEligible() && !relayWatchEligible(true))
        || m_watchPreparing || m_pendingWatchRelayPeer
        || m_directWatchTicketId != 0 || m_directWatch != nullptr) {
        return;
    }
    qCInfo(lcStationClient).noquote()
        << "Independent transmit watch unavailable:" << reason
        << "ordinary primary/media keepalives continue";
    const qint64 since = m_directWatchClock.elapsed() - m_lastDirectWatchRequestMs;
    m_directWatchRetryTimer->start(static_cast<int>(qMax<qint64>(1000, 1000 - since)));
}

void StationClient::requestWatchAttempt()
{
    if (m_sessionPurpose == SessionPurpose::RenameOnly) { return; }
    if (directWatchEligible()) {
        requestDirectWatchTicket();
    } else if (relayWatchEligible(true)) {
        startRelayWatch();
    }
}

void StationClient::startRelayWatch()
{
    if (!relayWatchEligible(true) || m_watchPreparing || m_pendingWatchRelayPeer
        || m_directWatchTicketId != 0 || m_directWatch != nullptr
        || m_directWatchRetryTimer->isActive()) {
        return;
    }
    const qint64 now = m_directWatchClock.elapsed();
    const qint64 since = now - m_lastDirectWatchRequestMs;
    if (since < 1000) {
        m_directWatchRetryTimer->start(static_cast<int>(1000 - since));
        return;
    }
    const auto* primary = qobject_cast<const DataChannelTransport*>(transport());
    if (primary == nullptr || !primary->watchRelayGrant()) { return; }
    const DataChannelTransport::WatchRelayGrant grant = *primary->watchRelayGrant();
    m_lastDirectWatchRequestMs = now;
    m_watchIsRelay = true;
    m_watchPreparing = true;
    const quint64 generation = m_directWatchGeneration;
    const quint32 sessionEpoch = m_sessionEpoch;
    const QPointer<StationClient> self(this);
    auto leg = RelayLeg::createWatch();
    if (!leg) {
        retireDirectWatch();
        if (self) { retryDirectWatch(QStringLiteral("watch relay could not start")); }
        return;
    }
    m_watchRelayLeg = leg;
    auto* peer = new DataChannelTransport(this);
    m_pendingWatchRelayPeer = peer;
    connect(peer, &DataChannelTransport::localDescription, this,
            [this, peer, generation, sessionEpoch](const QString& sdp, const QString& type) {
        handleRelayWatchOffer(sdp, type, peer, generation, sessionEpoch);
    });
    const auto failed = [this, peer, generation, sessionEpoch]() {
        if (m_pendingWatchRelayPeer != peer || m_directWatchGeneration != generation
            || m_sessionEpoch != sessionEpoch) { return; }
        const QPointer<StationClient> self(this);
        retireDirectWatch();
        if (self) { retryDirectWatch(QStringLiteral("watch relay connection failed")); }
    };
    connect(peer, &DataChannelTransport::failed, this,
            [failed](const QString&) { failed(); });
    connect(peer, &SessionTransport::closed, this, failed);
    connect(leg.get(), &RelayLeg::ended, this,
            [this, leg, generation, sessionEpoch](const QString&, const QString&) {
        if (m_watchRelayLeg != leg || m_directWatchGeneration != generation
            || m_sessionEpoch != sessionEpoch) { return; }
        const QPointer<StationClient> self(this);
        retireDirectWatch();
        if (self) { retryDirectWatch(QStringLiteral("watch relay ended")); }
    });
    m_directWatchTicketGeneration = generation;
    m_directWatchTicketSessionEpoch = sessionEpoch;
    m_directWatchTicketTimer->start(10000); // bounded SDP preparation
    leg->open(grant.url, grant.token);
    if (!self || self->m_directWatchGeneration != generation
        || self->m_pendingWatchRelayPeer != peer) { return; }
    IceConfiguration ice = IceConfiguration::throughRendezvous(
        {}, true, IceConfiguration::localAddressFamilies(), {});
    ice.setRelay(std::nullopt, 1);
    ice.setCandidateSourceFactory(RelayLeg::factoryFor(leg), true);
    DataChannelTransport::Options options;
    options.role = DataChannelTransport::Role::Offerer;
    options.purpose = DataChannelTransport::Purpose::TxWatch;
    options.maxIncomingBytes = DataChannelTransport::kMaxWatchFrameBytes;
    options.ice = ice;
    if (!peer->start(options) && self && self->m_pendingWatchRelayPeer == peer) {
        self->retireDirectWatch();
        if (self) { self->retryDirectWatch(QStringLiteral("watch DTLS offer could not start")); }
    }
}

void StationClient::handleRelayWatchOffer(const QString& sdp, const QString& type,
                                           DataChannelTransport* peer, quint64 generation,
                                           quint32 sessionEpoch)
{
    if (!m_watchPreparing || m_pendingWatchRelayPeer != peer
        || m_directWatchGeneration != generation || m_sessionEpoch != sessionEpoch) {
        return;
    }
    const QByteArray bytes = sdp.toUtf8();
    if (!relayWatchEligible(true) || type != QLatin1String("offer")
        || bytes.isEmpty() || bytes.size() > IMediaTransport::kMaxDescriptionBytes
        || bytes.contains('\0')) {
        const QPointer<StationClient> self(this);
        retireDirectWatch();
        if (self) { retryDirectWatch(QStringLiteral("watch DTLS offer was unavailable")); }
        return;
    }
    m_watchPreparing = false;
    const quint32 id = m_nextCommandId++;
    if (m_nextCommandId == 0) { ++m_nextCommandId; }
    m_directWatchTicketId = id;
    m_directWatchTicketGeneration = generation;
    m_directWatchTicketSessionEpoch = sessionEpoch;
    m_directWatchTicketTimer->start(10000); // Core ticket lifetime starts at dispatch
    send(SessionMessages::commandInvoke(
        QByteArrayLiteral("tx.watchRelay"), id,
        {MirrorUpdate{0, QByteArrayLiteral("offer"), MirrorWireKind::Utf8, QVariant(sdp)}}));
}

void StationClient::requestDirectWatchTicket()
{
    if (!directWatchEligible() || m_directWatchTicketId != 0 || m_directWatch != nullptr
        || m_directWatchRetryTimer->isActive()) {
        return;
    }
    const qint64 now = m_directWatchClock.elapsed();
    const qint64 since = now - m_lastDirectWatchRequestMs;
    if (since < 1000) {
        m_directWatchRetryTimer->start(static_cast<int>(1000 - since));
        return;
    }
    m_lastDirectWatchRequestMs = now;
    const quint32 id = m_nextCommandId++;
    if (m_nextCommandId == 0) { ++m_nextCommandId; }
    m_directWatchTicketId = id;
    m_directWatchTicketGeneration = m_directWatchGeneration;
    m_directWatchTicketSessionEpoch = m_sessionEpoch;
    m_directWatchTicketTimer->start(10000);
    send(SessionMessages::commandInvoke(QByteArrayLiteral("tx.watchTicket"), id, {}));
}

void StationClient::bindWatchClient(TxWatchClient* watch, quint64 generation,
                                    quint32 sessionEpoch)
{
    connect(watch, &TxWatchClient::ready, this,
            [this, watch, generation, sessionEpoch](quint64 reportedGeneration) {
        if (m_directWatch != watch || m_directWatchGeneration != generation
            || m_sessionEpoch != sessionEpoch || reportedGeneration != generation
            || !(m_watchIsRelay ? relayWatchEligible(false) : directWatchEligible())) {
            return;
        }
        if (m_watchIsRelay) { m_directWatchTicketTimer->stop(); }
        qCInfo(lcStationClient) << "Independent transmit watch is ready";
    });
    connect(watch, &TxWatchClient::closed, this,
            [this, watch, generation, sessionEpoch](quint64 reportedGeneration,
                                                    const QString& reason) {
        if (m_directWatch != watch || m_directWatchGeneration != generation
            || m_sessionEpoch != sessionEpoch || reportedGeneration != generation) {
            return;
        }
        const QPointer<StationClient> self(this);
        retireDirectWatch();
        if (self) { retryDirectWatch(reason); }
    });
}

void StationClient::handleDirectWatchTicket(const SessionMessage& message)
{
    if (message.commandId == 0 || message.commandId != m_directWatchTicketId
        || m_directWatchTicketGeneration != m_directWatchGeneration
        || m_directWatchTicketSessionEpoch != m_sessionEpoch) {
        return;
    }
    m_directWatchTicketId = 0;
    m_directWatchTicketGeneration = 0;
    m_directWatchTicketSessionEpoch = 0;
    m_directWatchTicketTimer->stop();
    if (!directWatchEligible()) { return; }
    if (!message.accepted) {
        retryDirectWatch(QStringLiteral("Core did not issue a watch ticket"));
        return;
    }
    QString encoded;
    QString path;
    qint64 expires = -1;
    bool ticketSeen = false;
    bool pathSeen = false;
    bool expirySeen = false;
    bool valid = message.updates.size() == 3;
    for (const MirrorUpdate& update : message.updates) {
        if (update.name == "ticket" && !ticketSeen && update.kind == MirrorWireKind::Utf8
            && update.value.typeId() == QMetaType::QString) {
            ticketSeen = true;
            encoded = update.value.toString();
        } else if (update.name == "path" && !pathSeen && update.kind == MirrorWireKind::Utf8
                   && update.value.typeId() == QMetaType::QString) {
            pathSeen = true;
            path = update.value.toString();
        } else if (update.name == "expiresInMs" && !expirySeen
                   && update.kind == MirrorWireKind::Int64
                   && update.value.typeId() == QMetaType::LongLong) {
            expirySeen = true;
            expires = update.value.toLongLong();
        } else {
            valid = false;
        }
    }
    bool canonical = false;
    QByteArray rawTicket = StationIdentity::fromBase64Url(encoded, &canonical);
    valid = valid && ticketSeen && pathSeen && expirySeen && canonical
        && rawTicket.size() == 32 && encoded.size() == 43
        && path == QStringLiteral("/tx-watch/v1") && expires == 10000;
    if (!valid) {
        rawTicket.fill('\0');
        retryDirectWatch(QStringLiteral("Core watch ticket reply was malformed"));
        return;
    }
    const QByteArray actualPin = transport()->peerCertificateSha256();
    const QUrl authority = m_connectedUrl;
    const quint64 generation = m_directWatchGeneration;
    const quint32 sessionEpoch = m_sessionEpoch;
    auto* watch = new TxWatchClient(this);
    m_directWatch = watch;
    bindWatchClient(watch, generation, sessionEpoch);
    const QPointer<StationClient> self(this);
    const bool opened = watch->openDirect(authority, actualPin, rawTicket, generation);
    rawTicket.fill('\0');
    if (self && !opened && self->m_directWatch == watch) {
        self->retireDirectWatch();
        if (self) {
            self->retryDirectWatch(QStringLiteral("direct watch socket did not open"));
        }
    }
}

void StationClient::handleRelayWatchResult(const SessionMessage& message)
{
    if (!m_watchIsRelay || m_watchPreparing || !m_pendingWatchRelayPeer
        || message.commandId == 0 || message.commandId != m_directWatchTicketId
        || m_directWatchTicketGeneration != m_directWatchGeneration
        || m_directWatchTicketSessionEpoch != m_sessionEpoch) {
        return;
    }
    m_directWatchTicketId = 0;
    if (!relayWatchEligible(false) || m_directWatchTicketTimer->remainingTime() <= 0
        || !message.accepted) {
        const QPointer<StationClient> self(this);
        retireDirectWatch();
        if (self) { retryDirectWatch(QStringLiteral("Core watch relay reply was unavailable")); }
        return;
    }
    QString encoded;
    QString path;
    QString answer;
    qint64 expires = -1;
    bool ticketSeen = false;
    bool pathSeen = false;
    bool answerSeen = false;
    bool expirySeen = false;
    bool valid = message.updates.size() == 4;
    for (const MirrorUpdate& update : message.updates) {
        if (update.name == "ticket" && !ticketSeen && update.kind == MirrorWireKind::Utf8
            && update.value.typeId() == QMetaType::QString) {
            ticketSeen = true;
            encoded = update.value.toString();
        } else if (update.name == "path" && !pathSeen && update.kind == MirrorWireKind::Utf8
                   && update.value.typeId() == QMetaType::QString) {
            pathSeen = true;
            path = update.value.toString();
        } else if (update.name == "answer" && !answerSeen && update.kind == MirrorWireKind::Utf8
                   && update.value.typeId() == QMetaType::QString) {
            answerSeen = true;
            answer = update.value.toString();
        } else if (update.name == "expiresInMs" && !expirySeen
                   && update.kind == MirrorWireKind::Int64
                   && update.value.typeId() == QMetaType::LongLong) {
            expirySeen = true;
            expires = update.value.toLongLong();
        } else {
            valid = false;
        }
    }
    bool canonical = false;
    QByteArray rawTicket = StationIdentity::fromBase64Url(encoded, &canonical);
    const QByteArray answerBytes = answer.toUtf8();
    valid = valid && ticketSeen && pathSeen && answerSeen && expirySeen && canonical
        && encoded.size() == 43 && rawTicket.size() == 32
        && path == QStringLiteral("relay-dtls-v1") && expires == 10000
        && !answerBytes.isEmpty() && answerBytes.size() <= IMediaTransport::kMaxDescriptionBytes
        && !answerBytes.contains('\0');
    if (!valid) {
        rawTicket.fill('\0');
        const QPointer<StationClient> self(this);
        retireDirectWatch();
        if (self) { retryDirectWatch(QStringLiteral("Core watch relay reply was malformed")); }
        return;
    }
    QPointer<DataChannelTransport> peer = m_pendingWatchRelayPeer;
    const quint64 generation = m_directWatchGeneration;
    const quint32 sessionEpoch = m_sessionEpoch;
    const QPointer<StationClient> self(this);
    const bool accepted = peer->acceptDescription(answer, QStringLiteral("answer"));
    if (!self) {
        rawTicket.fill('\0');
        return;
    }
    if (!accepted || !peer || m_pendingWatchRelayPeer != peer
        || m_directWatchGeneration != generation || m_sessionEpoch != sessionEpoch
        || !relayWatchEligible(false) || m_directWatchTicketTimer->remainingTime() <= 0) {
        rawTicket.fill('\0');
        if (m_pendingWatchRelayPeer == peer && m_directWatchGeneration == generation) {
            retireDirectWatch();
            if (self) { retryDirectWatch(QStringLiteral("watch DTLS answer was refused")); }
        }
        return;
    }
    const QByteArray actualPin = transport()->peerCertificateSha256();
    peer->disconnect(this);
    m_pendingWatchRelayPeer = nullptr;
    auto* watch = new TxWatchClient(this);
    m_directWatch = watch;
    bindWatchClient(watch, generation, sessionEpoch);
    const bool opened = watch->openRelay(peer, actualPin, rawTicket, generation);
    rawTicket.fill('\0');
    if (self && !opened && self->m_directWatch == watch
        && self->m_directWatchGeneration == generation) {
        self->retireDirectWatch();
        if (self) { self->retryDirectWatch(QStringLiteral("watch relay socket did not open")); }
    }
}

void StationClient::refreshRemoteTransmit()
{
    if (m_remoteTransmit != nullptr) {
        m_remoteTransmit->setAvailable(remoteTransmitAvailable());
    }
}

void StationClient::handleCommandResult(const SessionMessage& message)
{
    if (m_cancelledSettingsBackupBegin
        && message.commandId == m_cancelledSettingsBackupBegin->first) {
        const quint32 epoch = m_cancelledSettingsBackupBegin->second;
        m_cancelledSettingsBackupBegin.reset();
        if (epoch == m_sessionEpoch && stationLinkReady() && message.accepted
            && message.commandVerb == "station.settingsExport.begin"
            && message.updates.size() == 3) {
            const MirrorUpdate& field = message.updates.at(0);
            const QString id = field.value.toString();
            static const QRegularExpression idPattern(QStringLiteral("^[0-9a-f]{32}$"));
            if (field.ordinal == 0 && field.name == "transferId"
                && field.kind == MirrorWireKind::Utf8
                && field.value.typeId() == QMetaType::QString
                && idPattern.match(id).hasMatch()) {
                const quint32 cancelId = m_nextCommandId++;
                if (m_nextCommandId == 0) ++m_nextCommandId;
                send(SessionMessages::commandInvoke("station.settingsExport.cancel", cancelId,
                     {{0, "transferId", MirrorWireKind::Utf8, id}}));
            }
        }
        return;
    }
    if ((m_settingsBackupExport
         && message.commandId == m_settingsBackupExport->expectedCommandId)
        || message.commandVerb.startsWith("station.settingsExport.")) {
        handleSettingsBackupExportResult(message);
        return;
    }
    if (message.commandVerb == "tx.watchRelay") {
        handleRelayWatchResult(message);
        return;
    }
    if (message.commandVerb == "tx.watchTicket") {
        handleDirectWatchTicket(message);
        return;
    }
    if (message.commandVerb == "station.validateSettings"
        || message.commandVerb == "station.forgetSettings"
        || message.commandVerb == "station.repairSettings") {
        handleSettingsHygieneResult(message);
        return;
    }
    // R-R3-21: the app shows a refusal in user words (OperatorReasonText),
    // so the Core's own text is kept here, each time, as it arrived.
    if (!message.accepted) {
        qCInfo(lcStationClient).noquote()
            << "Station refused" << QString::fromUtf8(message.commandVerb)
            << "command" << message.commandId << ":" << message.reason;
    }
    // Desktop remote transmit: the transmit verbs' answers (every copy's)
    // go to the window's transmit client, which shows a refusal once, in
    // the words a local refusal uses; none takes the generic routes below.
    // Task 37: a keepalive's answer changes nothing; a refused one is
    // logged above and goes nowhere else (ten a second while keyed).
    if (message.commandVerb == "tx.keepalive") {
        return;
    }
    // iPhone app plan Task 29 (link section 21.2): the ticket this window
    // asked for to move the session. Never logged beyond the outcome.
    if (message.commandVerb == "session.pathTicket") {
        if (message.commandId == m_pathTicketCommandId && m_pathTicketCommandId != 0) {
            onPathTicket(message);
        }
        return;
    }
    // iPhone app plan Task 78 (the several-devices design, section 7.3): a
    // change held for a question is answered "Waiting for you to confirm."
    // with `phase` `needsConfirmation`; the question follows. It is not a
    // refusal to show, so it takes none of the refusal routes below.
    bool awaiting = false;
    for (const MirrorUpdate& value : message.updates) {
        if (value.name == "phase"
            && value.value.toString() == QStringLiteral("needsConfirmation")) {
            awaiting = !message.accepted;
        }
    }
    // Slice control plan Task 5: the four slice access verbs are answered
    // where the window's several-devices refusals are shown; Task 14b adds
    // a listened slice's own volume (slice.setListenLevel), Task 11 the
    // transmit slice's choice (tx.setTxSlice).
    if (message.commandVerb == "tx.take" || message.commandVerb == "confirm.proceed"
        || message.commandVerb == "confirm.cancel" || message.commandVerb == "notice.takeBack"
        || message.commandVerb == "session.leave" || message.commandVerb == "slice.listen"
        || message.commandVerb == "slice.stopListening"
        || message.commandVerb == "slice.takeControl" || message.commandVerb == "slice.release"
        || message.commandVerb == "slice.setListenLevel"
        || message.commandVerb == "tx.setTxSlice") {
        m_pendingCommands.remove(message.commandId);
        if (const auto kept = m_controlTakeBacks.constFind(message.commandId);
            kept != m_controlTakeBacks.cend()) {
            const qint64 noticeId = kept.value();
            m_controlTakeBacks.erase(kept);
            // Take-over fix wave (M-3): the card goes when control came
            // back or never can now; a refusal that may be tried again
            // (the slice transmits) leaves it.
            bool retry = false;
            if (!message.accepted) {
                for (const RemotePrompt& notice : m_remoteDevices->notices()) {
                    if (notice.prompt.id != noticeId || !notice.prompt.slices
                        || notice.prompt.slices->isEmpty()) {
                        continue;
                    }
                    const int sliceId = notice.prompt.slices->first().toObject()
                                            .value(QStringLiteral("sliceId")).toInt(-1);
                    const std::optional<SliceAccessMirror::Entry> now =
                        m_sliceAccess ? m_sliceAccess->entry(sliceId) : std::nullopt;
                    retry = controlTakeBackMayBeTriedAgain(
                        notice.prompt, message.reason,
                        now ? static_cast<qint64>(now->incarnation) : -1,
                        now ? static_cast<qint64>(now->controlRevision) : -1);
                    break;
                }
            }
            if (!retry) {
                m_remoteDevices->dismissNotice(noticeId);
            }
        }
        emit deviceCommandFinished(message.commandVerb, message.commandId, message.accepted,
                                   message.reason, awaiting);
        return;
    }
    if (awaiting) {
        m_pendingCommands.remove(message.commandId);
        return;
    }
    // Parity Task 19: the window's own record subscriptions (sent after
    // each snapshot); their answer is only logged above, and a batch
    // follows an accepted one.
    if (message.commandVerb == "records.subscribe"
        || message.commandVerb == "records.unsubscribe") {
        m_pendingCommands.remove(message.commandId);
        return;
    }
    if (message.commandVerb == "tx.key" || message.commandVerb == "tx.unkey"
        || message.commandVerb == "tx.tune" || message.commandVerb == "tx.tunerTune"
        || message.commandVerb == "tx.twoTone") {
        const QPointer<StationClient> self(this);
        if (m_remoteTransmit != nullptr) {
            m_remoteTransmit->commandFinished(message.commandId, message.commandVerb,
                                              message.accepted, message.reason, message.updates);
        }
        if (!self) { return; }
        emit commandResponse(message);
        if (!self) { return; }
        emit commandResult(message.commandId, message.accepted, message.reason);
        return;
    }
    if (message.commandVerb == "ps3.subscribeDisplay" && m_pendingPs3Display
        && m_pendingPs3Display->first == message.commandId) {
        const bool enabled = m_pendingPs3Display->second;
        m_pendingPs3Display.reset();
        const QPointer<StationClient> self(this);
        const quint32 epoch = m_sessionEpoch;
        emit ps3DisplaySubscriptionFinished(message.commandId, enabled,
                                             message.accepted, message.reason);
        if (!self || m_sessionEpoch != epoch || !m_sessionActive) { return; }
    }
    // Taken, not read: an id is answered exactly once, and leaving the
    // entry behind would grow this map for the life of the session.
    // A result for an id this client does not hold is not an error worth
    // refusing -- it is what a second CommandResult, or a result for a
    // command sent through the generic invokeCommand() rather than the
    // five typed verbs, looks like -- so the signal below still fires and
    // only the operator-facing routing is skipped.
    const PendingCommand pending = m_pendingCommands.take(message.commandId);

    // Fix wave GUI-I6: a Tune Power change answered; another may still be
    // on its way. Before the refusal below shows the Core's value.
    if (pending.verb == "setTunePowerForTxBand" && !m_radioModel.isNull()) {
        bool newerTunePower = false;
        for (auto it = m_pendingCommands.cbegin(); it != m_pendingCommands.cend(); ++it) {
            if (it.value().verb == pending.verb) {
                newerTunePower = true;
                break;
            }
        }
        m_radioModel->transmitModel().setTunePowerForTxBandWriteInFlight(newerTunePower);
    }

    const bool isFourO3ACommand = pending.verb == "setFourO3AEnabled";
    bool newerFourO3ACommand = false;
    if (isFourO3ACommand) {
        for (auto it = m_pendingCommands.cbegin(); it != m_pendingCommands.cend(); ++it) {
            if (it.value().verb == pending.verb) {
                newerFourO3ACommand = true;
                break;
            }
        }
        if (!newerFourO3ACommand && !m_radioModel.isNull()) {
            m_radioModel->reportStationFourO3ACommandFinished(message.accepted, message.reason);
        }
    }

    // notch.* refusals are shown by NotchModel itself (notchAddRejected /
    // notchRequestRefused), in the words the window uses for a local one.
    // requestIoBoardI2c / setIoBoardOutput refusals are shown by the HL2
    // Options tab that asked (reportStationIoBoardResult below), once;
    // routing them here as well showed the same refusal twice.
    const bool ioBoardRequest = pending.verb == "requestIoBoardI2c"
        || pending.verb == "setIoBoardOutput";
    // Parity Task 16: the filter graph asks for its curve by itself; a
    // refusal (no receiver yet) leaves it on the passband, with no notice.
    const bool filterResponseRequest = pending.verb == "dsp.filterResponse";
    // Parity Task 19: a spot source's refusal is shown on its Spot Hub tab;
    // a record subscription's is only logged (the window asks by itself).
    const bool spotRequest = pending.verb.startsWith("spots.")
        || pending.verb.startsWith("freedv.");
    // Parity Task 21: a radio request's refusal is shown on This Core.
    const bool radioRequest = pending.verb == "station.selectRadio"
        || pending.verb == "station.rescanRadios" || pending.verb == "station.setRadioModel"
        || pending.verb == "station.forgetRadio";
    if (radioRequest && !message.accepted && !m_radioModel.isNull()) {
        m_radioModel->reportStationRadioRefused(
            message.reason.isEmpty()
                ? QStringLiteral("The Core refused the request without giving a reason.")
                : message.reason);
    }
    const bool recordRequest = pending.verb.startsWith("records.");
    // Parity Task 22: the support bundle's answer and a logging change's
    // refusal go to the Support dialog, which asked.
    const bool supportRequest = pending.verb.startsWith("support.");
    if (pending.verb == "support.collect" && !m_radioModel.isNull()) {
        QByteArray bundle;
        for (const MirrorUpdate& value : message.updates) {
            if (value.name == "bundle" && value.kind == MirrorWireKind::Utf8) {
                bundle = QByteArray::fromBase64(value.value.toString().toLatin1());
            }
        }
        const QPointer<StationClient> self(this);
        m_radioModel->reportStationSupportBundle(message.commandId, message.accepted,
                                                 message.reason, bundle);
        if (!self) { return; }
    } else if (pending.verb == "support.setLogCategories" && !message.accepted
               && !m_radioModel.isNull()) {
        m_radioModel->reportStationLogCategoriesRefused(
            message.reason.isEmpty()
                ? QStringLiteral("The Core refused the request without giving a reason.")
                : message.reason);
    }
    if (spotRequest && !message.accepted && !m_radioModel.isNull()) {
        if (SpotSourceHost* spotSources = m_radioModel->spotSourceHost()) {
            spotSources->reportStationRefusal(pending.spotSource,
                message.reason.isEmpty()
                    ? QStringLiteral("The Core refused the request without giving a reason.")
                    : message.reason);
        }
    }
    if (!message.accepted && !m_radioModel.isNull() && !ioBoardRequest
        && !filterResponseRequest && !spotRequest && !recordRequest && !radioRequest
        && !supportRequest
        && !message.commandVerb.startsWith("ps3.") && !message.commandVerb.startsWith("dspAssets.")
        && !message.commandVerb.startsWith("notch.")) {
        // The station's OWN reason, relayed verbatim. Wording a refusal
        // here instead would put this client's guess in front of an
        // operator for a decision the daemon made -- and on a bench that
        // is indistinguishable from the click having silently done
        // nothing, which is the shape of the defect this round closes.
        const QString reason = message.reason.isEmpty()
            ? QStringLiteral("The Core refused the request without giving a reason.")
            : message.reason;
        // requestSliceSampleRate is the one verb whose refusal already has
        // a slice-scoped signal locally (sliceRetuneRejected, which
        // MainWindow toasts for 6 s because it names a frequency), so it
        // keeps using it. `pending.verb` rather than message.commandVerb
        // so an unrecognised or absent echo cannot misroute; the two agree
        // on every path SessionCommandDispatcher produces.
        if (pending.verb.startsWith("nnr.")) {
            if (auto* slice = m_radioModel->sliceById(pending.sliceId)) {
                slice->reportNnrEditResult(reason);
            }
        } else if (pending.verb == "requestSliceSampleRate") {
            m_radioModel->reportStationRetuneRejected(pending.sliceId, reason);
        } else if (const QString device = accessoryRefusalDevice(pending.verb,
                                                                 pending.faultsDevice);
                   !device.isEmpty()) {
            // L1 (R-R3-47, R-R3-22, R-R3-48): an accessory refusal has its
            // own route (the pages that sent it show it; MainWindow says
            // it), never the slice one.
            m_radioModel->reportStationAccessoryRefusal(device, reason, message.commandId);
        } else {
            m_radioModel->reportStationSliceCommandRejected(reason);
        }
        // R-R3-49 (parity Task 2): a refused Tune Power change leaves the
        // Core's value; the slider shows it again.
        if (pending.verb == "setTunePowerForTxBand") {
            m_radioModel->transmitModel().reportTunePowerForTxBandRefused();
        }
        // R-R3-49 (parity Task 3): a refused profile request leaves the
        // Core's profile; every profile combo shows it again.
        if (pending.verb.startsWith("txProfile.")) {
            if (MicProfileManager* profiles = m_radioModel->micProfileManager()) {
                profiles->reportStationRequestRefused();
            }
        }
        // R-R3-46 fix wave: a refused band antenna leaves the window's
        // values as the Core's; the Setup tab that showed the click re-reads.
        if (pending.verb == "setAlexRxAntenna" || pending.verb == "setAlexTxAntenna"
            || pending.verb == "setAlexRxAntennaForRadio"
            || pending.verb == "setAlexTxAntennaForRadio") {
            if (AlexAntennaFacade* alex = m_radioModel->alexAntennaFacade()) {
                alex->reportBandEditRefused();
            }
        }
    }

    const bool isCtunCommand = pending.verb == "requestStreamCtunPinned"
        || pending.verb == "requestStreamCentre";
    bool newerCtunCommand = false;
    if (isCtunCommand) {
        for (auto it = m_pendingCommands.cbegin(); it != m_pendingCommands.cend(); ++it) {
            const PendingCommand& candidate = it.value();
            if (candidate.verb == pending.verb && candidate.sliceId == pending.sliceId
                && candidate.streamEpoch == pending.streamEpoch) {
                newerCtunCommand = true;
                break;
            }
        }
        if (!newerCtunCommand) {
            if (pending.verb == "requestStreamCtunPinned") {
                emit streamCtunPinFinished(pending.sliceId, pending.streamEpoch,
                                           pending.requestedPin, message.accepted);
            } else {
                emit streamCentreFinished(pending.sliceId, pending.streamEpoch,
                                          message.accepted);
            }
        }
    }

    if (message.commandVerb.startsWith("notch.") && m_radioModel && m_radioModel->notchModel()) {
        const auto values = dspCommandValues(message.updates);
        m_radioModel->notchModel()->receiveRemoteResult(message.commandId, message.commandVerb,
            message.accepted, message.reason, values.value_or(QVariantMap{}));
    }
    if (message.commandVerb.startsWith("dspAssets.") && m_radioModel) {
        if (const auto values = dspCommandValues(message.updates)) {
            m_radioModel->dspAssets()->receiveRemoteResult(message.commandId, message.commandVerb,
                message.accepted, message.reason, *values);
        }
    }
    if (message.commandVerb.startsWith("ps3.") && m_radioModel) {
        if (const auto values = dspCommandValues(message.updates)) {
            const QString state = values->value("phase").toString();
            Ps3ActionPhase phase = Ps3ActionPhase::Failed;
            if (message.accepted && state == "pending") {
                phase = Ps3ActionPhase::Pending;
            } else if (message.accepted && state == "completed") {
                phase = Ps3ActionPhase::Completed;
            } else if (message.accepted && state == "accepted") {
                phase = Ps3ActionPhase::Accepted;
            }
            m_radioModel->pureSignalFacade()->receiveRemoteActionResult(message.commandId,
                message.commandVerb, phase, message.reason, *values);
        }
    }
    // R-R3-46 (parity Task 14): the Core's answer to an I2C request (a
    // read's bytes in `value`) or an output pin change, to the model,
    // which hands it to the tab that asked.
    if (ioBoardRequest && !m_radioModel.isNull()) {
        std::optional<qint64> value;
        for (const MirrorUpdate& update : message.updates) {
            if (update.name == "value" && update.kind == MirrorWireKind::Int64) {
                value = update.value.toLongLong();
            }
        }
        const QPointer<StationClient> self(this);
        m_radioModel->reportStationIoBoardResult(message.commandId, message.accepted,
                                                 message.reason, value);
        if (!self) { return; }
    }
    // R-R3-49 (parity Task 16): the Core's filter curve, to the model.
    if (filterResponseRequest && !m_radioModel.isNull()) {
        double startHz = 0.0;
        double stepHz = 0.0;
        QString json;
        for (const MirrorUpdate& update : message.updates) {
            if (update.name == "startHz" && update.kind == MirrorWireKind::Float64) {
                startHz = update.value.toDouble();
            } else if (update.name == "stepHz" && update.kind == MirrorWireKind::Float64) {
                stepHz = update.value.toDouble();
            } else if (update.name == "magnitudesDbJson" && update.kind == MirrorWireKind::Utf8) {
                json = update.value.toString();
            }
        }
        const QPointer<StationClient> self(this);
        m_radioModel->reportStationFilterResponse(message.commandId, message.accepted,
                                                  message.reason, startHz, stepHz, json);
        if (!self) { return; }
    }
    // R-R3-49 (parity Task 8): the Core's Scan LAN answer, to the window's
    // scan dialog (a refusal is also routed as an accessory refusal above).
    // R-R3-49 (parity Task 9): the same for the Power Genius's scan.
    if ((pending.verb == "scanTgxlLan" || pending.verb == "scanPgxlLan")
        && !m_radioModel.isNull()) {
        QString devicesJson;
        for (const MirrorUpdate& value : message.updates) {
            if (value.name == "devicesJson" && value.kind == MirrorWireKind::Utf8) {
                devicesJson = value.value.toString();
            }
        }
        const QPointer<StationClient> self(this);
        if (pending.verb == "scanTgxlLan") {
            m_radioModel->reportStationTgxlLanScan(message.commandId, message.accepted,
                                                   message.reason, devicesJson);
        } else {
            m_radioModel->reportStationPgxlLanScan(message.commandId, message.accepted,
                                                   message.reason, devicesJson);
        }
        if (!self) { return; }
    }
    // R-R3-22 fix wave: every result by its id, so a sender (the amp
    // applets, the TCI switch) clears its own pending request and shows
    // only its own refusal; it also ends a page's claim on an accepted
    // accessory request (follow-up 3). The refusal's words are the same the routing above used.
    if (!m_radioModel.isNull()) {
        const QPointer<StationClient> self(this);
        const QString finishedReason = message.accepted ? QString()
            : message.reason.isEmpty()
                ? QStringLiteral("The Core refused the request without giving a reason.")
                : message.reason;
        m_radioModel->reportStationCommandFinished(message.commandId, message.accepted,
                                                   finishedReason);
        if (!self) { return; }
    }
    emit commandResponse(message);
    emit commandResult(message.commandId, message.accepted, message.reason);
}

void StationClient::send(const SessionMessage& message)
{
    if (m_sessionPurpose == SessionPurpose::RenameOnly
        && message.kind != SessionMessageKind::Hello && message.kind != SessionMessageKind::AuthRequest
        && !(message.kind == SessionMessageKind::CommandInvoke
             && (message.commandVerb == QByteArrayLiteral("station.rename")
                 || message.commandVerb == QByteArrayLiteral("session.leave")))) {
        return;
    }
    if (m_transport == nullptr) {
        return;
    }
    // R-IOS-26 / R-R3-49: a Core without 2 m reads the per-band lists and
    // maps without their 2 m entry.
    const QByteArray wire = SessionMessages::encode(message);
    m_transport->sendText(station2mAvailable() ? wire : BandLinkFit::forStationWithout2m(wire));
}

bool StationClient::station2mAvailable() const
{
    return m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.band2mVersion >= 1;
}

QString StationClient::band2mUnavailableReason() const
{
    return station2mAvailable() ? QString() : station2mUnavailableReason();
}

QString StationClient::station2mUnavailableReason()
{
    return QStringLiteral("This Core does not have the 2 m band. Updating the Core adds it.");
}

bool StationClient::remoteCtunAvailable() const
{
    return m_sessionActive && m_authenticated && m_handshakeComplete
        && m_transport && m_transport->isOpen()
        && m_agreedMinor >= kRemoteCtunSessionProtocolMinor
        && m_capabilities.remoteCtunVersion >= 1;
}

bool StationClient::remoteTgxlConfigAvailable() const
{
    return m_sessionActive && m_authenticated && m_handshakeComplete
        && m_transport && m_transport->isOpen()
        && m_agreedMinor >= kRemoteTgxlConfigSessionProtocolMinor
        && m_capabilities.remoteTgxlConfigVersion >= 1;
}

bool StationClient::stationLinkReady() const
{
    return m_sessionActive && m_authenticated && m_handshakeComplete
        && m_transport && m_transport->isOpen();
}

bool StationClient::transmitTimeOutAvailable() const
{
    // The time-out (Task 38) and txState (Task 39) ship together; a Core
    // that sends txStateVersion 1 counts the time-out's time left.
    return stationLinkReady() && m_capabilities.txStateVersion >= 1;
}

bool StationClient::adcAttenuatorsAvailable() const
{
    // R-R3-46 / R-R3-11: stepAtt carries rx2AttenuationDb only with this.
    return stationLinkReady() && m_capabilities.adcAttenuatorVersion >= 1;
}

bool StationClient::remoteAmplifierStatusAvailable() const
{
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.remotePgxlControlVersion >= 1;
}

bool StationClient::remotePgxlControlAvailable() const
{
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.remotePgxlControlVersion >= 2;
}

bool StationClient::remoteRfKitStatusAvailable() const
{
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.remoteRfKitControlVersion >= 1;
}

bool StationClient::remoteRfKitControlAvailable() const
{
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.remoteRfKitControlVersion >= 2;
}

bool StationClient::accessoryDataAvailable() const
{
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.accessoryDataVersion >= 1;
}

bool StationClient::pgxlDeviceSettingsAvailable() const
{
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.remotePgxlControlVersion >= 3;
}

bool StationClient::tgxlDeviceSettingsAvailable() const
{
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.remoteTgxlControlVersion >= 1;
}

// R-R3-49 / R-R3-47: the Tuner Genius's antenna, operate and bypass.
bool StationClient::tgxlAutotuneAvailable() const
{
    // iPhone app plan Task 77: tx.tunerTune, remoteTxVersion 2.
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.remoteTxVersion >= 2 && m_remoteTransmit != nullptr
        && m_remoteTransmit->available();
}

bool StationClient::tgxlControlAvailable() const
{
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.remoteTgxlControlVersion >= 2;
}

bool StationClient::transmitSettingsAvailable(int minVersion) const
{
    // R-R3-49 (parity Task 1): the Core takes this window's transmit
    // settings while its radio is off the air.
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.transmitSettingsVersion >= std::max(minVersion, 1);
}

bool StationClient::transmitSettingsPermitted() const
{
    // Addendum G-42: the Core's own verdict for this session (the station
    // transmit gate), sent again whenever it changes (publishTxPermitted).
    return isHandshakeComplete() && remoteTransmitAvailable() && m_capabilities.txPermitted;
}

QString StationClient::transmitPermissionReason() const
{
    if (transmitSettingsPermitted()) {
        return {};
    }
    if (remoteTransmitAvailable() && !m_capabilities.txRefusalReason.isEmpty()) {
        return m_capabilities.txRefusalReason;
    }
    return IStationLink::transmitPermissionReason();
}

bool StationClient::pureSignalArmingOffered() const
{
    // R-R3-49 (parity Task 7): the Core takes this window's PureSignal
    // arming while its radio is off the air. Read at the handshake's end
    // and when capabilities change, so not through stationLinkReady().
    return m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.transmitSettingsVersion >= 7;
}

bool StationClient::tgxlOperateAppliesWhole() const
{
    // R-R3-49 fix wave: a Core at 3 applies setTgxlOperate on whole.
    return tgxlControlAvailable() && m_capabilities.remoteTgxlControlVersion >= 3;
}

bool StationClient::rfKitFullControlAvailable() const
{
    // R-R3-49 (parity Task 10): setRfKitOperate, setRfKitAntenna,
    // setRfKitTciMode, setRfKitAddress.
    return remoteRfKitControlAvailable() && m_capabilities.remoteRfKitControlVersion >= 4;
}

bool StationClient::rfKitCountersAvailable() const
{
    // R-R3-49 (parity Task 10): accessoryData's rfkit* counters.
    return accessoryDataAvailable() && m_capabilities.accessoryDataVersion >= 2;
}

bool StationClient::rfKitResponseTimeAvailable() const
{
    // Group B fix wave (M7): accessoryData's rfkitRttAvgMs.
    return accessoryDataAvailable() && m_capabilities.accessoryDataVersion >= 3;
}

bool StationClient::pgxlFullControlAvailable() const
{
    // R-R3-49 (parity Task 9): setPgxlOperate, scanPgxlLan, setPgxlAddress.
    return remotePgxlControlAvailable() && m_capabilities.remotePgxlControlVersion >= 4;
}

bool StationClient::tgxlFullControlAvailable() const
{
    // R-R3-49 (parity Task 8): moveTgxlRelay, scanTgxlLan, setTgxlAddress.
    return tgxlControlAvailable() && m_capabilities.remoteTgxlControlVersion >= 4;
}

bool StationClient::stationTciAvailable() const
{
    return stationLinkReady() && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.stationTciVersion >= 1;
}

int StationClient::coreStationTciStored() const
{
    // The Core's station switch is its StationTci_Enabled station setting
    // (StationTciController), which reaches this window in the settings
    // snapshot; absent there, the Core has none yet.
    // Rework follow-up 4: this link's snapshot, not one from a Core this
    // window used before.
    if (m_settingsProxy.isNull() || !m_settingsSnapshotThisLink) {
        return -1;
    }
    return m_coreKeepsTciSwitch ? 1 : 0;
}

bool StationClient::coreServesTciOnThisComputer() const
{
    // What the Core last said it offers, kept while the link is down: the
    // Core keeps its server running whether or not this window is there.
    if (m_agreedMinor < kRadioIdentitySessionProtocolMinor
        || m_capabilities.stationTciVersion < 1) {
        return false;
    }
    if (m_coreOnThisComputerForTest >= 0) {
        return m_coreOnThisComputerForTest == 1;
    }
    const QString host = m_lastUrl.host();
    if (host.isEmpty()) {
        return false;
    }
    if (host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    const QHostAddress address(host);
    if (address.isNull()) {
        return false;
    }
    if (address.isLoopback()) {
        return true;
    }
    const auto local = QNetworkInterface::allAddresses();
    for (const QHostAddress& mine : local) {
        if (mine.isEqual(address, QHostAddress::ConvertV4MappedToIPv4)) {
            return true;
        }
    }
    return false;
}

bool StationClient::remoteFourO3AControlAvailable() const
{
    return m_sessionActive && m_authenticated && m_handshakeComplete
        && m_transport && m_transport->isOpen()
        && m_agreedMinor >= kRemoteFourO3AControlSessionProtocolMinor
        && m_capabilities.remoteFourO3AControlVersion >= 1;
}

bool StationClient::telemetryAvailable() const
{
    return m_sessionActive && m_authenticated && m_handshakeComplete
        && m_transport && m_transport->isOpen()
        && m_agreedMinor >= kStationTelemetrySessionProtocolMinor
        && m_capabilities.stationTelemetryVersion >= 1;
}

std::optional<SessionTransportTelemetry> StationClient::transportTelemetry() const
{
    if (!m_sessionActive || !m_handshakeComplete || !m_transport) { return std::nullopt; }
    return m_transport->telemetry();
}

std::optional<AuxiliaryWatchTelemetry> StationClient::auxiliaryWatchTelemetry() const
{
    if (!m_sessionActive || !m_handshakeComplete || !m_transport) { return std::nullopt; }
    AuxiliaryWatchTelemetry total = m_retiredWatchTelemetry;
    if (m_directWatch) {
        const AuxiliaryWatchTelemetry current = m_directWatch->telemetry();
        total.receivedPayloadBytes += current.receivedPayloadBytes;
        total.submittedPayloadBytes += current.submittedPayloadBytes;
    }
    return total;
}

bool StationClient::mediaAvailable() const
{
    return m_sessionPurpose == SessionPurpose::Ordinary && m_sessionActive && m_authenticated && m_handshakeComplete
        && m_transport && m_transport->isOpen()
        && m_agreedMinor >= kMediaSessionProtocolMinor
        && m_capabilities.remoteMediaVersion >= 1;
}

bool StationClient::remoteWidebandAvailable() const
{
    return mediaAvailable() && m_agreedMinor >= kRemoteWidebandSessionProtocolMinor
        && m_capabilities.remoteWidebandDisplayVersion >= 1;
}

bool StationClient::remoteAudioStatusAvailable() const
{
    return mediaAvailable() && m_agreedMinor >= kRemoteAudioStatusSessionProtocolMinor
        && m_capabilities.remoteAudioStatusVersion >= 1;
}

bool StationClient::spectrumGrantAvailable() const
{
    return mediaAvailable() && m_agreedMinor >= kRemoteSpectrumGrantSessionProtocolMinor
        && m_capabilities.spectrumGrantVersion >= 1;
}

bool StationClient::spectrumDecimationAvailable() const
{
    return spectrumGrantAvailable() && m_capabilities.spectrumGrantVersion >= 2;
}

std::optional<DisplayBudgetLimits> StationClient::remoteDisplayBudgetLimits() const
{
    if (!mediaAvailable() || m_agreedMinor < kRemoteDisplayBudgetSessionProtocolMinor
        || m_capabilities.remoteDisplayBudgetVersion < 1) { return std::nullopt; }
    return m_capabilities.displayBudget;
}

DisplayBudgetReason StationClient::remoteDisplayBudgetReason() const
{
    if (!remoteDisplayBudgetLimits() || m_agreedMinor < kDisplayBudgetReasonSessionProtocolMinor
        || !m_capabilities.displayBudgetReason) {
        return DisplayBudgetReason::None;
    }
    return *m_capabilities.displayBudgetReason;
}

bool StationClient::remotePs3DisplaySubscribed() const
{
    return remoteDisplayBudgetLimits() && m_capabilities.remotePs3DisplaySubscribed;
}

quint32 StationClient::requestPs3DisplaySubscription(bool enabled)
{
    if (!mediaAvailable() || m_capabilities.psDisplayVersion < 1) { return 0; }
    if (!remoteDisplayBudgetLimits()) {
        return invokeCommand("ps3.subscribeDisplay", {{0, "enabled", MirrorWireKind::Bool, enabled}});
    }
    // One acknowledged transition at a time. Arm its identity before either
    // notification or transport can reenter; no guessed release on timeout.
    if (m_pendingPs3Display) { return 0; }
    const quint32 id = m_nextCommandId++;
    if (m_nextCommandId == 0) { ++m_nextCommandId; }
    m_pendingPs3Display = qMakePair(id, enabled);
    const QPointer<StationClient> self(this);
    const quint32 epoch = m_sessionEpoch;
    emit ps3DisplaySubscriptionStarted(id, enabled);
    if (!self || m_sessionEpoch != epoch || !mediaAvailable()
        || !m_pendingPs3Display || m_pendingPs3Display->first != id) { return 0; }
    send(SessionMessages::commandInvoke("ps3.subscribeDisplay", id,
        {{0, "enabled", MirrorWireKind::Bool, enabled}}));
    return id;
}

void StationClient::noteMediaEstablished(quint32 expectedEpoch)
{
    // R-R3-28. The deferred half of the SnapshotComplete reset: with media
    // negotiated, this is the proven success. Epoch-scoped so a ready from
    // a retired peer cannot reset a newer session's schedule.
    if (expectedEpoch == 0 || expectedEpoch != m_sessionEpoch || !mediaAvailable()) {
        return;
    }
    m_reconnectAttempts = 0;
}

bool StationClient::sendMediaControl(const QJsonObject& payload, quint32 expectedEpoch)
{
    if (!mediaAvailable() || expectedEpoch != m_sessionEpoch) {
        return false;
    }
    SessionMessage message;
    message.kind = SessionMessageKind::MediaControl;
    message.mediaPayload = payload;
    const QByteArray wire = SessionMessages::encode(message);
    if (wire.isEmpty()) {
        return false;
    }
    m_transport->sendText(wire);
    return true;
}

QList<QByteArray> StationClient::mirroredObjectKeys() const
{
    QList<QByteArray> keys;
    keys.reserve(m_objects.size());
    for (auto it = m_objects.cbegin(); it != m_objects.cend(); ++it) {
        if (!it.value().isNull()) {
            keys.append(it.key());
        }
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

QObject* StationClient::mirroredObject(const QByteArray& objectKey) const
{
    return m_objects.value(objectKey).data();
}

QString StationClient::thisDeviceWireId() const
{
    if (!m_tokenSessionHolderIdForTest.isEmpty() && !m_signedInWithDeviceKey) {
        return m_tokenSessionHolderIdForTest;
    }
    return m_deviceIdentity ? StationIdentity::toBase64Url(m_deviceIdentity->fingerprint())
                            : QString();
}

QString StationClient::transmitHolderText() const
{
    // Fix wave I4 (the several-devices design, ruling 8.1).
    if (m_transmitState == nullptr || !isHandshakeComplete()
        || capabilities().txStateVersion < 2) {
        return {};
    }
    const TransmitState& tx = *m_transmitState;
    if (tx.holderTransferring()) {
        // Fix wave 2: with nobody holding, a transfer ended with MOX still
        // reading on (TxRefusals::stopNotConfirmed's words).
        return tx.holderDeviceId().isEmpty()
                   ? QStringLiteral("The radio did not confirm it stopped transmitting.")
                   : QStringLiteral("Transmit is changing hands.");
    }
    if (tx.holderDeviceId().isEmpty()) {
        return {};
    }
    const QString self = thisDeviceWireId();
    if (!self.isEmpty() && tx.holderDeviceId() == self) {
        return QStringLiteral("This computer holds transmit.");
    }
    // Task 77 fix wave, M2 (ruling 8.1): the radio keeps transmit after its
    // own PTT until a device takes it; it never lets go. Task 78: this
    // window takes it with Take transmit.
    if (tx.holderSource() == QStringLiteral("radioPtt")
        || tx.holderKind() == QStringLiteral("station")) {
        return QStringLiteral("The radio has the transmitter. Take it from this window to "
                              "transmit.");
    }
    const QString name = tx.holderName().isEmpty() ? QStringLiteral("Another device")
                                                   : tx.holderName();
    // Task 78: a window that can take transmit says how.
    if (transmitTakeAvailable()) {
        return tx.holderAway()
                   ? QStringLiteral("%1 holds transmit and is away. Take transmit to use MOX "
                                    "and TUNE here.")
                         .arg(name)
                   : QStringLiteral("%1 holds transmit. Take transmit to use MOX and TUNE "
                                    "here.")
                         .arg(name);
    }
    // Fix wave 2: the name and why this window's MOX and TUNE are refused
    // until the holder lets go (until taking transmit is built, Task 77).
    return tx.holderAway()
               ? QStringLiteral("%1 holds transmit and is away. MOX and TUNE here wait until "
                                "it lets go.")
                     .arg(name)
               : QStringLiteral("%1 holds transmit. MOX and TUNE here wait until it lets go.")
                     .arg(name);
}

bool StationClient::knowsTransmitHolder() const
{
    // A receive-only Core has no device holding transmit: its transmit
    // settings stay this window's to change off the air (R-R3-49), whoever
    // its own keys named.
    return m_transmitState != nullptr && isHandshakeComplete()
        && capabilities().txStateVersion >= 2 && remoteTransmitAvailable()
        && capabilities().txRefusalCode != QString::fromLatin1(TxRefusals::kStationReceiveOnly);
}

bool StationClient::holdsTransmitHere() const
{
    if (!knowsTransmitHolder()) {
        return false;
    }
    const QString self = thisDeviceWireId();
    return !self.isEmpty() && m_transmitState->holderDeviceId() == self;
}

QString StationClient::otherHolderReason() const
{
    if (!knowsTransmitHolder() || m_transmitState->holderDeviceId().isEmpty()
        || holdsTransmitHere()) {
        return {};
    }
    const QString name = m_transmitState->holderName().isEmpty()
        ? QStringLiteral("Another device")
        : m_transmitState->holderName();
    return TxRefusals::otherDeviceHolds(name).text;
}

// ── iPhone app plan Task 78: several devices on one Core ───────────────────

bool StationClient::sessionHolderAvailable() const
{
    return m_declaredSessionHolder && isHandshakeComplete()
        && m_agreedMinor >= kRadioIdentitySessionProtocolMinor
        && m_capabilities.sessionHolderVersion >= 1;
}

bool StationClient::transmitTakeAvailable() const
{
    // The link document, section 18.9: tx.take needs sessionHolderVersion 1
    // and came with remoteTxVersion 2; a Core that does not take this
    // window's keys has nothing to take.
    return sessionHolderAvailable() && remoteTransmitAvailable()
        && m_capabilities.remoteTxVersion >= 2 && knowsTransmitHolder();
}

bool StationClient::transmitHeldElsewhere() const
{
    return transmitTakeAvailable() && !m_transmitState->holderDeviceId().isEmpty()
        && !holdsTransmitHere();
}

quint32 StationClient::requestTakeTransmit(bool shown, qint64 holderEpoch, bool shownKeyed)
{
    if (!transmitTakeAvailable()) {
        return 0;
    }
    QList<MirrorUpdate> args;
    if (shown) {
        args.append(MirrorUpdate{0, QByteArrayLiteral("holderEpoch"), MirrorWireKind::Int64,
                                 QVariant(holderEpoch)});
        args.append(MirrorUpdate{0, QByteArrayLiteral("shownKeyed"), MirrorWireKind::Bool,
                                 QVariant(shownKeyed)});
    }
    return invokeCommand(QByteArrayLiteral("tx.take"), args);
}

quint32 StationClient::requestTxSlice(int sliceId)
{
    // tx.setTxSlice came with remoteTxVersion 1 (iPhone app plan Task 34);
    // the Core answers notHolder while this window does not hold transmit.
    if (!sessionHolderAvailable() || !remoteTransmitAvailable() || sliceId < 0) {
        return 0;
    }
    return invokeCommand(
        QByteArrayLiteral("tx.setTxSlice"),
        {MirrorUpdate{0, QByteArrayLiteral("sliceId"), MirrorWireKind::Int64,
                      QVariant(static_cast<qint64>(sliceId))}});
}

quint32 StationClient::proceedQuestion(qint64 id, qint64 choice)
{
    if (!sessionHolderAvailable()) {
        return 0;
    }
    m_remoteDevices->closeQuestion(id);
    return invokeCommand(
        QByteArrayLiteral("confirm.proceed"),
        {MirrorUpdate{0, QByteArrayLiteral("id"), MirrorWireKind::Int64, QVariant(id)},
         MirrorUpdate{0, QByteArrayLiteral("choice"), MirrorWireKind::Int64, QVariant(choice)}});
}

quint32 StationClient::cancelQuestion(qint64 id)
{
    if (!sessionHolderAvailable()) {
        m_remoteDevices->closeQuestion(id);
        return 0;
    }
    m_remoteDevices->closeQuestion(id);
    return invokeCommand(
        QByteArrayLiteral("confirm.cancel"),
        {MirrorUpdate{0, QByteArrayLiteral("id"), MirrorWireKind::Int64, QVariant(id)}});
}

quint32 StationClient::takeBackNotice(qint64 id)
{
    // Take-over fix wave (M-3): a controlTaken card waits for the answer.
    bool controlTaken = false;
    for (const RemotePrompt& notice : m_remoteDevices->notices()) {
        if (notice.prompt.id == id) {
            controlTaken = notice.prompt.kind == QLatin1String("controlTaken");
            break;
        }
    }
    if (!controlTaken || !sessionHolderAvailable()) {
        m_remoteDevices->dismissNotice(id);
    }
    if (!sessionHolderAvailable()) {
        return 0;
    }
    const quint32 commandId = invokeCommand(
        QByteArrayLiteral("notice.takeBack"),
        {MirrorUpdate{0, QByteArrayLiteral("id"), MirrorWireKind::Int64, QVariant(id)}});
    if (controlTaken) {
        if (commandId == 0) {
            m_remoteDevices->dismissNotice(id);
        } else {
            m_controlTakeBacks.insert(commandId, id);
        }
    }
    return commandId;
}

bool StationClient::controlTakeBackMayBeTriedAgain(const SessionPrompt& notice,
                                                   const QString& reason, qint64 incarnationNow,
                                                   qint64 revisionNow)
{
    if (notice.kind != QLatin1String("controlTaken") || !notice.slices
        || notice.slices->isEmpty() || incarnationNow < 0
        || reason == QLatin1String("That can no longer be taken back.")) {
        return false;
    }
    const QJsonObject entry = notice.slices->first().toObject();
    return entry.value(QStringLiteral("incarnation")).toInteger(-1) == incarnationNow
        && entry.value(QStringLiteral("controlRevision")).toInteger(-1) == revisionNow;
}

bool StationClient::answerHeld(const QString& deviceId)
{
    const std::optional<RemoteHeldList> held = m_remoteDevices->held();
    if (!held || m_transport == nullptr) {
        return false;
    }
    send(SessionMessages::sessionTakeover(deviceId, held->revision));
    m_remoteDevices->clearHeld();
    // A choice resumes the connect: the Core lets this window in, or asks
    // again with a newer list. Declining ends the session from the Core.
    if (!deviceId.isEmpty() && m_handshakeDeadlineMs > 0) {
        m_handshakeDeadlineTimer->start(m_handshakeDeadlineMs);
    }
    return true;
}

quint32 StationClient::leaveSession()
{
    if (!sessionHolderAvailable()) {
        return 0;
    }
    return invokeCommand(QByteArrayLiteral("session.leave"), {});
}

bool StationClient::isAwaitingConfirmation(const QString& reason)
{
    // The one reason the Core gives a held change (the several-devices
    // design, section 7.3; the link document, section 7.5).
    return reason == QStringLiteral("Waiting for you to confirm.");
}

// ── iPhone app plan Task 29: the race and moving to a better path ─────────
//
// The link document, section 21. A paired Core is raced: every address the
// window has for it, and the internet service, at once (PathRacer). The
// first path whose hello proves the Core wins and the session signs in
// there; once it has reached snapshot.complete a better path, if one is
// ready or found later, takes the session over (SwitchableTransport),
// without a sign-in or a snapshot.

bool StationClient::helloProvesPairedCore(const SessionMessage& hello,
                                          const QByteArray& certSha256,
                                          const QByteArray& expectedIdentity,
                                          QString* rendezvousId)
{
    if (hello.kind != SessionMessageKind::Hello || expectedIdentity.isEmpty()) {
        return false;
    }
    const QByteArray spki = stationSpkiOf(hello);
    if (spki.isEmpty() || StationIdentity::fingerprintOf(spki) != expectedIdentity) {
        return false;
    }
    if (!bindingHolds(spki, hello.stationIdentity->certBinding, certSha256)) {
        return false;
    }
    if (rendezvousId != nullptr) {
        *rendezvousId = RendezvousWire::rendezvousId(spki);
    }
    return true;
}

PathRacer* StationClient::newRacer(bool upgrade)
{
    auto* racer = new PathRacer(this);
    const QByteArray identity = m_stationIdentity;
    racer->setVetter([identity](const SessionMessage& hello, SessionTransport* transport) {
        return transport != nullptr
            && helloProvesPairedCore(hello, transport->peerCertificateSha256(), identity);
    });
    // Review Minor 8: an upgrade knows its bar before any address is
    // listed, so none that cannot beat it is dialled.
    if (upgrade) {
        racer->setBetterThan(m_pathRank);
    }
    racer->addDirectUrls(m_dialPlan, kMaxIncomingMessageBytes);
    const ServiceRoute& route = m_serviceRoute;
    const int controlVersion = route.currentControlChannelVersion
        ? route.currentControlChannelVersion() : route.controlChannelVersion;
    const bool routeUsable = !route.servers.isEmpty()
        && RendezvousWire::isRendezvousId(route.rendezvousId) && m_deviceIdentity
        && m_deviceIdentity->isValid();
    const QString serviceHost = route.servers.isEmpty() ? QString() : route.servers.first().host();
    if (routeUsable && controlVersion == 0) {
        // Link section 21.1: a Core whose last session declared no control
        // channel does not answer through the service; not started.
        if (!upgrade) {
            racer->addNote(PathRacer::PathKind::Service, serviceHost,
                           PathRacer::Outcome::CoreTooOld,
                           QString::fromLatin1(RendezvousDialer::kCoreTooOldReason));
        }
    } else if (routeUsable && (!upgrade || m_pathRank > PathRacer::ServiceDirect)) {
        // An upgrade looks for a path without the relay (link 21.3).
        const bool allowRelay = !upgrade && route.relayAllowed;
        auto* rung = new RendezvousPathRung(route.servers, route.rendezvousId, m_deviceIdentity,
                                            allowRelay);
        if (m_serviceDialDeadlineMs > 0) {
            rung->setDialDeadlineMs(m_serviceDialDeadlineMs);
        }
        if (m_serviceAnswerDeadlineMs > 0) {
            rung->setAnswerDeadlineMs(m_serviceAnswerDeadlineMs);
        }
        // Review Minor 5: a Core whose last session declared the control
        // channel is slow or offline when it does not answer, not old.
        rung->setCoreAnswersIntroductions(controlVersion >= 1);
        racer->addRung(rung);
        if (!upgrade && !route.relayAllowed) {
            racer->addNote(PathRacer::PathKind::Relay, serviceHost,
                           PathRacer::Outcome::RelayOff,
                           QStringLiteral("The Core has the relay turned off."));
        }
    }
    return racer;
}

void StationClient::startRace()
{
    if (m_sessionPurpose == SessionPurpose::Ordinary && m_candidateSource) {
        const quint64 request = m_connectionRequestGeneration;
        const QPointer<StationClient> self(this);
        const CandidateSource source = m_candidateSource;
        const auto candidates = source();
        if (!self || request != m_connectionRequestGeneration) { return; }
        if (!candidates) {
            const auto reasonSource = m_candidateUnavailableReasonSource;
            const QString suppliedReason = reasonSource ? reasonSource() : QString();
            if (!self || request != m_connectionRequestGeneration) { return; }
            const QString reason = suppliedReason.isEmpty()
                ? QStringLiteral("The saved Core changed; choose it again to connect.") : suppliedReason;
            const bool active = m_sessionActive;
            disconnectFromStation(reason);
            m_dialPlan.clear();
            m_serviceRoute = {};
            m_raceMode = false;
            m_lastError = reason;
            if (!active) { emit sessionEnded(reason); }
            return;
        }
        m_cachedAddresses = candidates->addresses;
        m_serviceRoute = candidates->service;
        m_dialPlan.clear();
        for (const QUrl& address : m_cachedAddresses) {
            if (RemoteStationOptions::isValidStationUrl(address.toString()) && !m_dialPlan.contains(address)) {
                m_dialPlan.append(address);
            }
        }
        if (!m_lastUrl.isEmpty() && !m_dialPlan.contains(m_lastUrl)) { m_dialPlan.append(m_lastUrl); }
    }
    stopRace();
    m_lastError.clear();
    startDialPlan();
    m_raceWinnerUrl.clear();
    m_raceWinnerLine = -1;
    m_raceLines.clear();
    m_pathRank = -1;
    PathRacer* racer = newRacer(/*upgrade=*/false);
    m_racer = racer;
    connect(racer, &PathRacer::linesChanged, this, [this, racer] {
        if (m_racer == racer) {
            syncAttemptFromRace(racer);
        }
    });
    connect(racer, &PathRacer::won, this, [this, racer](const PathRacer::Ready& ready) {
        if (m_racer == racer) {
            adoptRaceWinner(ready);
        }
    });
    connect(racer, &PathRacer::failed, this, [this, racer](const QString& reason) {
        if (m_racer == racer) {
            onRaceFailed(reason);
        }
    });
    qCInfo(lcStationClient) << "Racing every path to the Core";
    racer->start();
    emit connectionActivityChanged();
}

void StationClient::stopRace()
{
    if (m_racer) {
        PathRacer* racer = m_racer;
        m_racer = nullptr;
        racer->disconnect(this);
        racer->cancel();
        racer->deleteLater();
    }
    if (m_upgradeRacer) {
        PathRacer* racer = m_upgradeRacer;
        m_upgradeRacer = nullptr;
        racer->disconnect(this);
        racer->cancel();
        racer->deleteLater();
    }
    m_upgradeTimer->stop();
    abandonUpgrade(QString(), /*reschedule=*/false);
}

void StationClient::syncAttemptFromRace(const PathRacer* racer)
{
    if (racer == nullptr) {
        return;
    }
    const QList<PathRacer::Line> lines = racer->lines();
    m_raceLines = lines;
    QList<StationConnectionAttempt::Try> tries;
    for (int i = 0; i < lines.size(); ++i) {
        const PathRacer::Line& line = lines.at(i);
        StationConnectionAttempt::Try attempt;
        switch (line.kind) {
        case PathRacer::PathKind::ThisNetwork:
            attempt.path = StationConnectionAttempt::Path::ThisNetwork;
            break;
        case PathRacer::PathKind::Direct:
            attempt.path = StationConnectionAttempt::Path::Direct;
            break;
        case PathRacer::PathKind::Service:
            attempt.path = StationConnectionAttempt::Path::Service;
            break;
        case PathRacer::PathKind::Relay:
            attempt.path = StationConnectionAttempt::Path::Relay;
            break;
        case PathRacer::PathKind::WebRelay:
            attempt.path = StationConnectionAttempt::Path::WebRelay;
            break;
        }
        attempt.address = line.address;
        using O = StationConnectionAttempt::Outcome;
        switch (line.outcome) {
        case PathRacer::Outcome::Trying: attempt.outcome = O::Trying; break;
        case PathRacer::Outcome::Ready:
            attempt.outcome = i == m_raceWinnerLine && m_handshakeComplete ? O::Connected
                                                                             : O::Trying;
            break;
        case PathRacer::Outcome::NoAnswer: attempt.outcome = O::NoAnswer; break;
        case PathRacer::Outcome::TimedOut: attempt.outcome = O::TimedOut; break;
        case PathRacer::Outcome::NotThisCore: attempt.outcome = O::NotThisCore; break;
        case PathRacer::Outcome::Failed: attempt.outcome = O::Failed; break;
        case PathRacer::Outcome::Stopped: attempt.outcome = O::AnotherPathFirst; break;
        case PathRacer::Outcome::RelayOff: attempt.outcome = O::RelayOff; break;
        case PathRacer::Outcome::CoreTooOld: attempt.outcome = O::CoreTooOld; break;
        case PathRacer::Outcome::WebRelayEnded:
            attempt.outcome = O::WebRelayEnded;
            attempt.reason = line.reason;
            break;
        }
        tries.append(attempt);
    }
    m_attempt.tries = tries;
    emit connectionAttemptChanged();
}

void StationClient::adoptRaceWinner(const PathRacer::Ready& ready)
{
    if (ready.transport.isNull()) {
        return;
    }
    // The winner's line, for the record at snapshot.complete.
    for (int i = 0; i < m_raceLines.size(); ++i) {
        if (m_raceLines.at(i).outcome == PathRacer::Outcome::Ready
            && m_raceLines.at(i).address == ready.address) {
            m_raceWinnerLine = i;
        }
    }
    m_raceWinnerUrl = ready.url;
    m_pathRank = ready.rank;
    m_pinRequired = false;
    m_transportOpened = true;
    SessionTransport* transport = ready.transport;
    attachTransport(transport, m_planToken);
    qCInfo(lcStationClient) << "Signing in to the Core at" << ready.address;
    // The Core's hello, which the race read to vet this path: the session
    // reads it first, as if it had just arrived.
    const QPointer<StationClient> self(this);
    onTransportText(ready.hello);
    if (self && m_racer) {
        syncAttemptFromRace(m_racer);
    }
}

void StationClient::onRaceFailed(const QString& reason)
{
    PathRacer* racer = m_racer;
    if (racer != nullptr) {
        syncAttemptFromRace(racer);
        m_racer = nullptr;
        racer->disconnect(this);
        racer->deleteLater();
    }
    m_lastError = reason;
    qCInfo(lcStationClient) << "No path reached the Core";
    // As a failed first connect: reported, then retried (link 12.4).
    emit sessionEnded(reason);
    scheduleReconnect();
    emit connectionActivityChanged();
}

bool StationClient::canMovePathNow() const
{
    if (m_sessionPurpose == SessionPurpose::RenameOnly || !m_handshakeComplete || m_capabilities.controlSwitchVersion < 1
        || sessionTransport() == nullptr || sessionTransport()->switching()) {
        return false;
    }
    // Link section 21.2: not while this window is keyed or has VOX armed,
    // nor while the Core is on the air (the Core also refuses while MOX's
    // delay timers run).
    if (m_remoteTransmit != nullptr && m_remoteTransmit->keepaliveRunning()) {
        return false;
    }
    if (!m_radioModel.isNull() && m_radioModel->isTransmitting()) {
        return false;
    }
    return true;
}

bool StationClient::moveSessionForTest(SessionTransport* next, int rank, const QUrl& url)
{
    if (next == nullptr || !canMovePathNow() || m_upgrade) {
        return false;
    }
    PathRacer::Ready ready;
    ready.transport = next;
    ready.rank = rank;
    ready.url = url;
    ready.kind = PathRacer::PathKind::Direct;
    ready.address = next->peerDescription();
    beginUpgrade(ready);
    return true;
}

void StationClient::scheduleUpgrade(bool advance)
{
    m_upgradeTimer->stop();
    if (m_sessionPurpose == SessionPurpose::RenameOnly || !m_raceMode || m_pathRank <= PathRacer::ThisNetwork || !m_handshakeComplete
        || m_capabilities.controlSwitchVersion < 1 || m_upgradeScheduleMs.isEmpty()) {
        return;
    }
    if (advance) {
        ++m_upgradeAttempt;
    }
    const int index = std::min(m_upgradeAttempt, static_cast<int>(m_upgradeScheduleMs.size()) - 1);
    m_upgradeTimer->start(m_upgradeScheduleMs.at(index));
}

void StationClient::refreshPathRank()
{
    if (m_pathRank != PathRacer::ServiceRelayed && m_pathRank != PathRacer::ServiceDirect
        && m_pathRank != PathRacer::Floor) {
        return;
    }
    const auto* channel = qobject_cast<const DataChannelTransport*>(transport());
    const std::optional<MediaIcePath> path =
        channel != nullptr ? channel->selectedPath() : std::nullopt;
    if (!path) {
        return;
    }
    const int rank = PathRacer::rankForPath(path);
    if (rank == m_pathRank) {
        return;
    }
    qCInfo(lcStationClient) << "The connection through the service settled on rank" << rank;
    m_pathRank = rank;
    // The record names the path the session runs on.
    const StationConnectionAttempt::Path now = rank == PathRacer::Floor
        ? StationConnectionAttempt::Path::WebRelay
        : rank == PathRacer::ServiceRelayed ? StationConnectionAttempt::Path::Relay
                                            : StationConnectionAttempt::Path::Service;
    for (StationConnectionAttempt::Try& attempt : m_attempt.tries) {
        if (attempt.outcome == StationConnectionAttempt::Outcome::Connected
            && (attempt.path == StationConnectionAttempt::Path::Relay
                || attempt.path == StationConnectionAttempt::Path::Service
                || attempt.path == StationConnectionAttempt::Path::WebRelay)) {
            attempt.path = now;
        }
    }
    emit connectionAttemptChanged();
}

void StationClient::startUpgradeRace()
{
    if (m_upgradeRacer || m_upgrade) {
        return;
    }
    // Review Minor 7: look only for what beats the path in use now.
    refreshPathRank();
    if (m_pathRank <= PathRacer::ThisNetwork) {
        return;
    }
    if (!canMovePathNow()) {
        // Link 21.3: the schedule waits for the unkey (and for a move
        // already under way); look again at the first step's interval.
        if (m_handshakeComplete && m_pathRank > PathRacer::ThisNetwork) {
            m_upgradeTimer->start(m_upgradeScheduleMs.value(0, 5000));
        }
        return;
    }
    PathRacer* racer = newRacer(/*upgrade=*/true);
    m_upgradeRacer = racer;
    connect(racer, &PathRacer::won, this, [this, racer](const PathRacer::Ready& ready) {
        if (m_upgradeRacer != racer) {
            if (ready.transport) {
                ready.transport->closeLink(QStringLiteral("not needed"));
                ready.transport->deleteLater();
            }
            return;
        }
        m_upgradeRacer = nullptr;
        racer->finish();
        racer->disconnect(this);
        racer->deleteLater();
        beginUpgrade(ready);
    });
    connect(racer, &PathRacer::failed, this, [this, racer](const QString&) {
        if (m_upgradeRacer != racer) {
            return;
        }
        m_upgradeRacer = nullptr;
        racer->disconnect(this);
        racer->deleteLater();
        scheduleUpgrade(/*advance=*/true);
    });
    qCInfo(lcStationClient) << "Looking for a better path to the Core";
    racer->start();
}

void StationClient::beginUpgrade(PathRacer::Ready ready)
{
    if (ready.transport.isNull()) {
        scheduleUpgrade(/*advance=*/true);
        return;
    }
    if (!canMovePathNow() || m_upgrade) {
        ready.transport->closeLink(QStringLiteral("not now"));
        ready.transport->deleteLater();
        scheduleUpgrade(/*advance=*/false);
        return;
    }
    const QPointer<StationClient> watchSelf(this);
    retireDirectWatch();
    if (!watchSelf) { return; }
    ready.transport->setParent(this);
    // The Core may close the new connection at its connect deadline, or on
    // a refused join; either gives the move up.
    connect(ready.transport, &SessionTransport::closed, this, [this, transport = ready.transport] {
        if (m_upgrade && m_upgrade->transport == transport && m_pathTicketCommandId != 0) {
            abandonUpgrade(QStringLiteral("the new connection closed"), true);
        }
    });
    m_upgrade = ready;
    m_pathTicketCommandId = invokeCommand(QByteArrayLiteral("session.pathTicket"), {});
    if (m_pathTicketCommandId == 0) {
        abandonUpgrade(QStringLiteral("no session to move"), false);
        return;
    }
    m_upgradeDeadline->start();
    qCInfo(lcStationClient) << "Moving the session to" << ready.address;
}

void StationClient::abandonUpgrade(const QString& why, bool reschedule)
{
    m_upgradeDeadline->stop();
    m_pathTicketCommandId = 0;
    if (!m_upgrade) {
        return;
    }
    const PathRacer::Ready upgrade = *m_upgrade;
    m_upgrade.reset();
    if (upgrade.transport) {
        upgrade.transport->disconnect(this);
        upgrade.transport->closeLink(QStringLiteral("move given up"));
        upgrade.transport->deleteLater();
    }
    if (!why.isEmpty()) {
        qCInfo(lcStationClient) << "Not moving the session:" << why;
    }
    if (reschedule) {
        scheduleUpgrade(/*advance=*/true);
    }
    retryDirectWatch(QStringLiteral("watch path move ended"));
}

void StationClient::onPathTicket(const SessionMessage& result)
{
    m_upgradeDeadline->stop();
    m_pathTicketCommandId = 0;
    if (!m_upgrade || m_upgrade->transport.isNull()) {
        abandonUpgrade(QStringLiteral("the new connection is gone"), true);
        return;
    }
    QString ticket;
    for (const MirrorUpdate& value : result.updates) {
        if (value.name == "ticket" && value.kind == MirrorWireKind::Utf8) {
            ticket = value.value.toString();
        }
    }
    SwitchableTransport* switchable = sessionTransport();
    if (!result.accepted || ticket.isEmpty() || switchable == nullptr || !canMovePathNow()) {
        // Review Minor 14 (and its re-review): a refusal for now (the Core
        // on the air, or this window keyed or VOX armed) is not a failed
        // look, so the schedule stays on its current step; anything else
        // is, and advances it.
        const bool forNow = !canMovePathNow()
            || (!result.accepted
                && result.reason == QLatin1String(kPathTransmittingReason));
        abandonUpgrade(result.accepted ? QStringLiteral("this connection cannot move now")
                                       : result.reason,
                       /*reschedule=*/false);
        scheduleUpgrade(/*advance=*/!forNow);
        return;
    }
    const PathRacer::Ready upgrade = *m_upgrade;
    m_upgrade.reset();
    SessionTransport* next = upgrade.transport;
    next->disconnect(this);
    // Link section 21.2 step 3: this window's hello on the new connection,
    // as at a sign-in, then path.join in place of auth.request.
    QHash<QByteArray, int> features = m_declaredFeatures;
    if (m_declaredSessionHolder) {
        features.insert(QByteArrayLiteral("sessionHolder"), 1);
    }
    next->sendText(SessionMessages::encode(
        SessionMessages::hello(m_agreedMajor, kSessionProtocolMinor, m_localSettingsSchema,
                               peerNameForThisProcess(), m_supportedMajors, features)));
    next->sendText(SessionMessages::encode(SessionMessages::pathJoin(ticket)));
    // The ICE settings of the connection through the service this session
    // may be leaving, for its media afterwards (link 21.3).
    const auto* leaving = qobject_cast<const DataChannelTransport*>(transport());
    const std::optional<IceConfiguration> leavingIce =
        leaving != nullptr ? leaving->iceConfiguration() : std::nullopt;
    const int rank = upgrade.rank;
    const QUrl url = upgrade.url;
    const QString address = upgrade.address;
    const PathRacer::PathKind kind = upgrade.kind;
    connect(switchable, &SwitchableTransport::switched, this,
            [this, switchable, rank, url, address, kind, leavingIce] {
        disconnect(switchable, &SwitchableTransport::switched, this, nullptr);
        disconnect(switchable, &SwitchableTransport::switchFailed, this, nullptr);
        if (leavingIce && !m_serviceIce) {
            m_serviceIce = leavingIce;
        }
        m_pathRank = rank;
        ++m_pathSwitches;
        if (url.isValid()) {
            m_connectedUrl = url;
            m_cachedAddresses.removeAll(url);
            m_cachedAddresses.prepend(url);
        }
        // The record: the path left, and the one the session runs on now.
        for (StationConnectionAttempt::Try& attempt : m_attempt.tries) {
            if (attempt.outcome == StationConnectionAttempt::Outcome::Connected) {
                attempt.outcome = StationConnectionAttempt::Outcome::MovedOn;
            }
        }
        StationConnectionAttempt::Try moved;
        moved.path = kind == PathRacer::PathKind::ThisNetwork
            ? StationConnectionAttempt::Path::ThisNetwork
            : kind == PathRacer::PathKind::Direct ? StationConnectionAttempt::Path::Direct
            : kind == PathRacer::PathKind::Relay  ? StationConnectionAttempt::Path::Relay
            : kind == PathRacer::PathKind::WebRelay ? StationConnectionAttempt::Path::WebRelay
                                                  : StationConnectionAttempt::Path::Service;
        moved.address = address;
        moved.outcome = StationConnectionAttempt::Outcome::Connected;
        m_attempt.tries.append(moved);
        qCInfo(lcStationClient) << "The session moved to" << address << "rank" << rank;
        m_upgradeAttempt = 0;
        const QPointer<StationClient> self(this);
        emit connectionAttemptChanged();
        emit pathChanged();
        if (self) {
            scheduleUpgrade(/*advance=*/false);
            requestWatchAttempt();
        }
    });
    connect(switchable, &SwitchableTransport::switchFailed, this,
            [this, switchable](const QString& why) {
        disconnect(switchable, &SwitchableTransport::switched, this, nullptr);
        disconnect(switchable, &SwitchableTransport::switchFailed, this, nullptr);
        qCInfo(lcStationClient) << "The session stayed where it was:" << why;
        scheduleUpgrade(/*advance=*/true);
        retryDirectWatch(QStringLiteral("watch path move failed"));
    });
    if (!switchable->beginClientSwitch(next)) {
        disconnect(switchable, &SwitchableTransport::switched, this, nullptr);
        disconnect(switchable, &SwitchableTransport::switchFailed, this, nullptr);
        next->closeLink(QStringLiteral("move given up"));
        next->deleteLater();
        scheduleUpgrade(/*advance=*/true);
        retryDirectWatch(QStringLiteral("watch path move failed"));
    }
}

// ── iPhone app plan Task 27: the attempt record ────────────────────────────

bool StationConnectionAttempt::connected() const
{
    for (const Try& attempt : tries) {
        if (attempt.outcome == Outcome::Connected) {
            return true;
        }
    }
    return false;
}

QString StationConnectionAttempt::pathText(Path path)
{
    switch (path) {
    case Path::ThisNetwork:
        return QStringLiteral("this network");
    case Path::Direct:
        return QStringLiteral("direct");
    case Path::Relay:
        return QStringLiteral("relay");
    case Path::Service:
        return QStringLiteral("through the internet service");
    case Path::WebRelay:
        return QStringLiteral("web relay");
    }
    return {};
}

QString StationConnectionAttempt::outcomeText(Outcome outcome)
{
    switch (outcome) {
    case Outcome::Trying:
        return QStringLiteral("still trying");
    case Outcome::Connected:
        return QStringLiteral("connected");
    case Outcome::NoAnswer:
        return QStringLiteral("no answer");
    case Outcome::TimedOut:
        return QStringLiteral("no answer in time");
    case Outcome::NotThisCore:
        return QStringLiteral("another computer answered");
    case Outcome::Failed:
        return QStringLiteral("did not connect");
    case Outcome::AnotherPathFirst:
        return QStringLiteral("another path connected first");
    case Outcome::RelayOff:
        return QStringLiteral("the Core has the relay turned off");
    case Outcome::CoreTooOld:
        return QStringLiteral("this Core can't be reached through the internet service; "
                              "updating the Core may help");
    case Outcome::MovedOn:
        return QStringLiteral("connected, then moved to a better path");
    case Outcome::WebRelayEnded:
        return QStringLiteral("the web relay ended the connection");
    }
    return {};
}

QString StationConnectionAttempt::summary() const
{
    if (tries.isEmpty()) {
        return {};
    }
    QStringList parts;
    for (const Try& attempt : tries) {
        // Step 2b: a line with words of its own (the web relay's, section
        // 12.4 of the rendezvous document) says them, without the end stop.
        QString words = attempt.reason;
        if (words.endsWith(QLatin1Char('.'))) {
            words.chop(1);
        }
        parts.append(QStringLiteral("%1 (%2): %3")
                         .arg(pathText(attempt.path), attempt.address,
                              words.isEmpty() ? outcomeText(attempt.outcome) : words));
    }
    return QStringLiteral("Tried ") + parts.join(QStringLiteral("; ")) + QLatin1Char('.');
}

StationConnectionAttempt::Path StationConnectionAttempt::pathFor(const QUrl& url)
{
    const QString host = url.host();
    if (host.endsWith(QLatin1String(".local"), Qt::CaseInsensitive)
        || host.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0) {
        return Path::ThisNetwork;
    }
    QHostAddress address(host);
    if (address.isNull()) {
        return Path::Direct;
    }
    if (address.isLoopback()) {
        return Path::ThisNetwork;
    }
    bool mapped = false;
    const quint32 ipv4 = address.toIPv4Address(&mapped);
    if (mapped) {
        address = QHostAddress(ipv4);
    }
    for (const QNetworkInterface& interface : QNetworkInterface::allInterfaces()) {
        if (!(interface.flags() & QNetworkInterface::IsUp)
            || !(interface.flags() & QNetworkInterface::IsRunning)) {
            continue;
        }
        for (const QNetworkAddressEntry& entry : interface.addressEntries()) {
            if (entry.prefixLength() >= 0
                && address.isInSubnet(entry.ip(), entry.prefixLength())) {
                return Path::ThisNetwork;
            }
        }
    }
    return Path::Direct;
}

} // namespace NereusSDR
