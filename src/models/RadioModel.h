#pragma once

// =================================================================
// src/models/RadioModel.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-03 - Diversity atomic reentry and slice-close/hydration lifetime
//                 fences, J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-09-30 - Fix round 1 (minor 4): transmitLinkDownReason picks the
//                 link-down words by state (the window's link to the Core,
//                 the Core without a radio, the radio's link). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - RADE reason: RadeStartFault, beginRadeStart/endRadeStart,
//                 refreshRadeReasons and radeStartReason, so a RADE slice
//                 with no working decoder says why on its radeReason.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Shared-input filters (rulings (c) and (d)):
//                 rxFilter0LowPassReason and rxFilter0LowPassSlice, declared
//                 last; lowPassHoldReason; the counted slices and 6m/ByPass
//                 on RX as last applied. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-30 - Shared-input filters, follow-up: receiverVfoHzBySlot.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - lowPassHoldReason names the slices the HL2's N2ADR
//                 broadcast-band high-pass is off for (JJ's ruling).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - m_hl2NoPinsSliceId: the slice the HL2 filter board is off
//                 for (JJ's ruling). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-30 - lowPassHoldChanged: rxFilter0LowPassReason and
//                 rxFilter0LowPassSlice notify on it, not on
//                 filterStateChanged, so a hold alone sends a peer without
//                 rxFilterLowPass no delta. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-30 - Radio codec: connectMicCodecSignals and its test seam;
//                 the radio speaker output tap. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Radio codec: the connect-load seam locks the mic source
//                 on radioMicSelectable; orionMicPanelAvailable and the HL2
//                 add-on note. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-29 - Radio Status PTT source: a remote window reads a key from
//                 a device that does not hold transmit as Remote, as the
//                 Core's window does; both keep the key's source through a
//                 RADE end-of-over tail; a radio with no station running
//                 reads the key's own source (TCI, CAT, PTT, VOX).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - RADE end-of-over callsigns: endOfOverTailActive /
//                 endOfOverTailChanged, startRadeEndOfOverTail and
//                 onEndOfOverTailChanged (FreeDV's end-of-over frame after
//                 an operator's release). J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
//   2026-09-21 — Multi-slice RADE RX ownership and lifetime orchestration by
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 OpenAI Codex.
//   2026-09-22 : R-R3-36 Task 5 by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code. PC microphone session demand
//                 (setPcCaptureAllowed / pcCaptureRequired and a
//                 LocalSession capture lease). NereusSDR-original; no
//                 Thetis logic.
//   2026-09-22 : R-R3-36 Task 7 by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code. PC-microphone MOX admission
//                 (pcCaptureGatesKeying / pcCaptureReady) and input-loss
//                 release. NereusSDR-original; no Thetis logic.
//   2026-09-22 : R-R3-36 fix wave by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code. Tune and two-tone keying read at
//                 check time (m_tuneKeyInFlight, m_generatedKeyLive).
//                 NereusSDR-original; no Thetis logic.
//   2026-09-23 : R-R3-40 by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code. Per-receiver DSP load snapshot
//                 (ReceiverDspLoad, receiverDspLoad) and the stamped I/Q
//                 feed to RxDspWorker. NereusSDR-original; no Thetis logic.
//                 Later the same day: one periodic sampler
//                 (m_dspLoadSampler, m_dspLoadTimer) owns the intervals and
//                 receiverDspLoad returns its cached snapshot.
//   2026-09-23 : R-R3-23 / R-R3-16 by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code. localAudioDevices(), the audio
//                 engine handed out uncounted for this computer's own
//                 devices, and the setNameForTest seam. NereusSDR-original;
//                 no Thetis logic.
//   2026-09-23 - R-R3-46: alexAntennaFacade(),
//                 scheduleRemoteHardwareApply(), requestIoBoardProbe(). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-23 - R-R3-46 fix wave: ioBoardFacade(), the `ioBoard` object;
//                 scheduleRemoteOcReload(); rxMeterOffsetDb() is 0 on a
//                 Remote model. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-23 - R-R3-47 / R-R3-22: amplifierModel() and rfKitModel(), the
//                 Power Genius and RF-Kit status objects. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-23 : R-R3-44 by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code. setRemoteVaxChannelStore(): where a remote
//                 window's slices keep their VAX channel. NereusSDR-original;
//                 no Thetis logic.
//   2026-09-24 - R-R3-47 / R-R3-48: RF-Kit station verbs,
//                stationTciModel() / stationTciController(), rfKitEnabled
//                read-only. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-47 / R-R3-22: the Core's accessory records and
//                settings (`accessoryData`): RF-Kit fault log, the model's
//                own connection counters, the interlock, power-cap and
//                fault-history commands, and a window's setting changes
//                reaching the Core's live objects. NereusSDR-original.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-22 / R-R3-47: setStationBind (the Core's station
//                listeners on the station network only) and the FlexRadio
//                beacon following the 4O3A switch. NereusSDR-original.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-47 / R-R3-22: the Power Genius's and Tuner Genius's
//                own settings for a window (`accessorySettings`, the
//                ...ForStation device-settings requests) and a route of
//                their own for the Core's refusals (accessoryRequestRefused).
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-24 - R-R3-47 fix wave: resetRfKitErrorForStation; the station
//                TCI objects owned by std::unique_ptr; accessory refusals
//                say whether the page that sent them shows them. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-49: the Network Watchdog setting applied where the
//                radio is (setNetworkWatchdogEnabled / applyNetworkWatchdog).
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-49 / R-R3-47: the Tuner Genius's antenna, operate
//                and bypass for a remote window (remoteTgxlControlVersion
//                2), refused while the radio is on the air. NereusSDR-
//                original. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-24 - R-R3-49 fix wave: `transmitting`, the Core's real MOX
//                (MoxController, any source, through the TX to RX
//                handover), Core to window. NereusSDR-original. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-49 (parity Task 1): stationOnAirRefusal, the Core's
//                one on-the-air refusal; isCoreOnAir / coreOnAirChanged in
//                a window; the TX half of the remote DSP > Options apply.
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-24 - R-R3-49 (parity Task 2): setTunePowerForTxBandForStation,
//                refreshTransmitTuneBand and wireTransmitChainForTest.
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 3): the TX profile commands
//                (selectTxProfileForStation, saveTxProfileForStation,
//                deleteTxProfileForStation), resetRadeVocoderForStation,
//                scopeTxProfiles and the Core's profiles published on
//                `transmit`; a window's profile manager mirrors them.
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-25 - R-R3-32 / R-R3-46 / R-R3-49 (parity Task 6): paReadings,
//                the one PA reading source for every window (the Core's in
//                a remote window, applyCorePaReadings); the Core's TX
//                inhibit mirrored as `txInhibited`. NereusSDR-original.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 5): applySwrProtectionSetting, the
//                SWR protection settings applied to the live controller
//                when they change, not only at start. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-R3-13 / R-R3-49 (parity Task 15): applyMeterSetting, the
//                Multimeter polling delay applied to the meter pump when a
//                window changes it. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-26 - R-R3-49 / R-R3-21 / R-R3-40 (parity Task 16): the DSP
//                facts a window reads from its Core: noiseReductionMethods,
//                dspOptionsLastApplyMs, each slice's minNotchWidthHz and
//                the filter curve (filterResponseForStation,
//                coreFilterResponse). NereusSDR-original. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-R3-49 (trunk merge of parity Tasks 16 to 18): one noise
//                reduction availability source. noiseReductionMethods and
//                noiseReductionUnavailableReason dropped for
//                nrCannotRunReason (DspAssetService), which in a remote
//                window gives the "does not say" reason on a Core below
//                dspAssetVersion 3 (DFNR) or 4 (MNR). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-21: before a pool is sized, the slice-limit refusal
//                names the Core only on a Core (NereusSDR in a window with
//                no Core); stale slice-limit comments corrected.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-39: receive DSP off the event loop. A local model
//                owns the receive lane (DspControlThread); the live sample
//                rate change runs there (setSampleRateLiveAsync,
//                sampleRateChangeFinished) and the calls into RxDspWorker
//                that blocked the event loop became lane jobs. NereusSDR-
//                original. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-25 - R-R3-39: transmit DSP off the event loop. A local model
//                owns the transmit lane too; the TX channel stays on this
//                thread and posts its WDSP calls there; MoxController's
//                keying steps reach it through wireTxChannelKeying
//                (setRunningAsync). NereusSDR-original. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01 - Related to #300: scope keying connections and interlock
//                failsafes to the current TX channel/session. NereusSDR-
//                original. J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-09-25 - Task 33 (R-IOS-03): stopTransmitNow (the emergency
//                stop, NereusSDR-original), stopAllTx (ported from Thetis
//                console.cs StopAllTx), transmitStopped, onMoxRxReady.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49, Sub-epic C-1: the Core's DFNR availability
//                (DspAssetService dfnrRunnable / dfnrModelStatus), set at
//                start and when a channel's first DFNR load fails; DFNR is
//                refused, and turned off on a slice, while it cannot run.
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-25 - R-R3-49, Sub-epic C-1: the Core's MNR availability
//                (DspAssetService mnrRunnable / mnrStatus) and BNR's
//                build-wide reason; both are refused, and turned off on a
//                slice, while they cannot run. NereusSDR-original. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - Receiver and transmit gaps plan, Task 7: setMoxFromButton
//                (Thetis chkMOX_Click, console.cs:29730-29747 [v2.10.3.15])
//                for the MOX buttons, and setMox, the TCI trx shim, now keys
//                through MoxController::onTciPtt with PttMode::Tci under the
//                PollPTT rules instead of always acting. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-24 - Receiver and transmit gaps plan, Task 7 fix wave:
//                applyTxKeyBlock (TX inhibit and the PA trip gate every
//                key, console.cs:15341-15363 and 25470 [v2.10.3.15]); setMox
//                passes every TCI release on; teardown unkeys; the MOX
//                button completes a pending TUN-off before keying.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 73 (R-IOS-02): SliceOwnership (whose
//                each slice is, each owner's active slice and the
//                station-level one), owners in the restart manifest, a
//                device's slice restored at its old frequency and letter,
//                and the FreeDV Reporter frequency following the
//                station-level active slice. NereusSDR-original. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 34 (R-IOS-02, R-IOS-03): the
//                unkey-confirmed gate (UnkeyGate, unkeyGate()), which the TX
//                slice arbiter's handoff while keyed waits for; TUNE asks the
//                keying gate before it starts (admitStationKey); the MOX
//                check names its refusal codes (stationReceiveOnly,
//                micNotReady); stopAllTx's MOX = false goes to the
//                controller. NereusSDR-original. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 35 (R-IOS-13): who is keyed
//                (keyedBy, naming the transmit holder) and the keying
//                epoch; setTune for a remote device's key; PttSource::Remote
//                while a device is keyed. NereusSDR-original. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 36 (R-IOS-13): the station's
//                microphone source follows a remote device (RemoteMicFeed)
//                while it transmits or has VOX armed; PttSource::Vox for a
//                device's VOX key. NereusSDR-original. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - iPhone app plan, desktop remote transmit (R-IOS-13,
//                R-R3-42): in a remote window MOX, TUNE and two-tone go to
//                the Core through the transmit verbs (setTwoTone added),
//                never the window's own MoxController; the Core's refusal
//                is reported (remoteTransmitRefused). NereusSDR-original.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 37 (R-IOS-13): remoteMicDeviceChanged,
//                so the Core turns off VOX a device armed when its
//                microphone line closes. NereusSDR-original. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 38 (R-IOS-04, D29): the transmit
//                time-out (TxTimeOutTimer, Thetis TimeOutTimerManager)
//                with its limit for whoever is keyed, timeOutRemainingSeconds
//                and the timeOut stop reason; timeOutTimer from console.cs.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 74 (R-IOS-02, R-IOS-30): each
//                slice's receiver reported to the anchors;
//                moveStreamWindowFor and moveSlicesToStream for a confirmed
//                pan move; the allocator and the slice cap readable.
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-25 - R-IOS-27, R-IOS-06: addTnfForSlice, the desktop's +TNF
//                in one place, and addTnfFromStation, the same add for a
//                device's notch.addAtSlice. NereusSDR-original. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-21, R-IOS-27: addTnfForSlice on a remote window sends
//                notch.addAtSlice to a notchControlVersion 2 Core.
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 7): pureSignalOperationPermitted, a
//                receive-only Core lets a window arm PureSignal off the
//                air. NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 8): moveTgxlRelayForStation,
//                scanTgxlLanForStation and setTgxlAddressForStation
//                (remoteTgxlControlVersion 4), and the window's LAN scan
//                answer (reportStationTgxlLanScan). NereusSDR-original.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 9): setPgxlOperateForStation,
//                scanPgxlLanForStation and setPgxlAddressForStation
//                (remotePgxlControlVersion 4), and the window's LAN scan
//                answer (reportStationPgxlLanScan). NereusSDR-original.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 10): setRfKitOperateForStation,
//                setRfKitAntennaForStation, setRfKitTciModeForStation and
//                setRfKitAddressForStation (remoteRfKitControlVersion 4).
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity mini-round, the operator's rulings a to
//                c): refuseLocalAccessorySwitchOnAir; Scan LAN and the
//                saved amp and tuner addresses go ahead on the air.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - Receiver and transmit gaps plan, Task 16: receive only
//                (Thetis console.RXOnly, console.cs:15312-15334 and
//                setup.cs:6479 [v2.10.3.15]): setRxOnly / applyRxOnlySetting
//                / isRxOnly / rxOnlyChanged, a Core setting, and the HL2
//                receive-only kit always receive only. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - Task 16 fix wave: MOX disabled by receive only in every
//                mode (I3), rxOnlyReasonAlongside and
//                transmitBlockReasonAlongside (M6, M2). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-R3-46 / R-R3-32 (parity Task 14): requestIoBoardI2c,
//                setIoBoardOutput, refreshIoBoardOutputs (HL2 Options' I2C
//                tool and Pin Control, local and through the Core), and
//                hl2LinkFigures / applyCoreHl2LinkFigures. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave C2: the microphone lines by
//               device (openRemoteMicLine, closeRemoteMicLine, per-device
//               priming and VOX), one writer at a time (remoteMicWriter),
//               VOX following the holder. J.J. Boyd (KG4VCF), with AI-
//               assisted implementation via Anthropic Claude Code.
//   2026-09-26 - R-R3-01 (parity Task 17 follow-up): fftEnginePool view
//                hook, so Rendering > Decimation reaches every pan.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  iPhone app plan Task 77 (R-IOS-02,
//                                    R-IOS-03, R-IOS-13):
//                                    startTgxlAutotuneFor,
//                                    cancelTgxlAutotuneFor and a device's
//                                    cycle; the arbiter's owner lookups. AI-
//                                    assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Task 77 fix wave (I3, I4): a device's
//                                    autotune refused on the air and ended
//                                    unkeyed when MOX came on during its
//                                    standby wait; the amplifier's restore
//                                    waits for receive. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Task 77 fix round 2 (R-IOS-02,
//                                    R-IOS-03, R-IOS-13): the Power Genius
//                                    is switched only with nothing keyed or
//                                    pending and one command in flight; a
//                                    key's RF waits at the RF-flow gate
//                                    while it changes over (stopped after
//                                    1.5 s); every autotune refused on the
//                                    air. AI-assisted via Anthropic Claude
//                                    Code.
//   2026-09-26 - Parity Task 19 (R-IOS-25): SpotSourceHost starts and
//                stops the spot sources (the restore moved there; the Core
//                runs the station's with restoreStationSpotSources, a remote
//                window its own WSJT-X and SpotCollector); a remote window
//                shows the Core's spots stream beside its own
//                (applyStationRecordBatch). J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-26 - Parity Task 21 (R-IOS-18): a remote window's copy of the
//                Core's radios (stationRadios, stationRadiosChanged) and the
//                Core's refusals of a radio request (stationRadioRefused).
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 - iPhone app plan Task 78 (R-IOS-02, R-IOS-30):
//                setStationMayCloseLastSlice and stationDevices. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 - iPhone app plan Task 77 fix round 3 (R-IOS-02, R-IOS-03,
//                R-IOS-13): pgxlSwitchRefusal (the Power Genius waits while
//                a Tuner Genius cycle runs), an error reply or a fault ends
//                the amplifier's changeover, and the stop's words say how to
//                transmit without it. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-26 - Parity Task 28 (R-R3-49, A11): setTxAnalyzer also makes
//                the transmit display's feed (txDisplayFeed), which starts
//                and stops the TX analyzer on the MOX edge and holds its
//                view for every viewer. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-27 - Parity Task 30 (R-R3-49, R-R3-21, A12):
//                applyRemoteTxDisplaySetting, a window's Setup > Display >
//                TX Display analyzer setting applied to the Core's TX
//                analyzer at once; stationTxDisplayVersion, the Core's
//                txDisplayVersion as a remote window last heard it. J.J.
//                Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 - Parity Task 31 (A11, R-R3-49): display duplex and noise
//                blanking. With DUP on at the key (this window's, or on a
//                Core the transmit holder's), NB and NB2 go off on the
//                transmit slice while keyed and come back at the unkey, as
//                Thetis's UIMOXChangedTrue / False do. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-27 - R-R3-49: on Protocol 2 each slice comes back at the rate
//                saved for its band at connect (savedSliceSampleRates,
//                applySavedSliceSampleRates), and a rate change saves it.
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-27 - iPhone plan Task 22 / parity Task 20 (R-IOS-26):
//                clearStationFreedv(). NereusSDR-original. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 - R-R3-49: publishSliceAudioView() hands AudioEngine each
//                slice's mute, output route and VAX channel on every add,
//                remove, layout restore and change, so the audio thread
//                never walks m_slices. NereusSDR-original. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 - Parity Task 33 (R-R3-49, R-R3-32): paRawAdc() and
//                paRawAdcChanged, the radio's raw forward and reflected
//                power readings the Core sends in txState; a remote window's
//                stationTransmitState() and stationTxReadingsVersion().
//   2026-09-27 - R-IOS-13 / R-R3-49 (txModMonitorVersion 1): the AM Mod
//                Monitor in a remote window. setAmModTxTapEnabled (the
//                Core feeds its TX analyzer only for a watching device),
//                applyModMonitorSetting (the Core's feedback receiver,
//                ModMon/FbStream), and a remote window's copy of the
//                Core's readings (stationModMonitorSnapshot).
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-28 - R-R3-49 / R-R3-46: Setup > Transmit > Power's Disable HF PA
//                applied (Thetis DisablePA and hf_tr_relay,
//                transmitSettingsVersion 11). J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-28 - R-R3-46 / R-R3-49: the Alex Filters tabs' receive filter rows
//                (per-row bypass and edges, Alex-2 master bypass) select the
//                receive high-pass as Thetis's setAlexHPF /
//                setBPF1ForOrionIISaturn / setAlex2HPF do (radioHardwareVersion
//                8). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Phone wire batch: logCategoryList, the Support dialog's
//                categories with their labels (logCategoryListVersion 1).
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-29 - Remote parity on the air: the five station transmit
//                commands take `takenOnAir` (transmitSettingsVersion 13).
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 1: the HL2 TX buffer latency and PTT hang
//                (bank 17) are the saved HL2 options, as mi0bot
//                setup.cs:21236-21248 [@c26a8a4] sends them. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 1: rebindIoBoardSlice feeds the HL2 I/O
//                board poll the TX VFO's mode and frequency. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - R-R3-49 / R-IOS-18: PA Gain's profiles for a remote client
//                 (paProfileActionForStation; the page's ids, plain tooltips,
//                 the adjust tooltip's stray %, and the Default profile found
//                 by its real name after a delete). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-29 - R-R3-49 (found bug): Log Volts/Amps to VALog.txt works:
//                 the controller reads the box (logVoltsAmps), the station's
//                 RadioModel logs through VoltsAmpsLog (Thetis console.cs
//                 LogVA). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-09-29 - HL2 port part 2: the HL2 I/O board's fault register holds
//                 transmit off (mi0bot console.cs UpdateIOBoard 25876-25885
//                 [@c26a8a4]); txInhibitReason names it on every window, and
//                 the transmit buttons are disabled with the reason while any
//                 TX inhibit holds, as Thetis's TXInhibit setter does
//                 (console.cs:15341-15363 [v2.10.3.15]). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 2: expectIoBoardRead and
//                 updateIoBoardPollingPause, the I/O board poll's pause
//                 while the I2C tool waits on a read (mi0bot
//                 SetI2CPollingPause [@c26a8a4]). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-29 - PA on-air gate re-review, item 5: TUNE-on drive, the
//                 first-MOX seed (seedInitialAudioVolume) and the two-tone
//                 start read the held transmit band (driveTxBand), as
//                 Thetis reads _tx_band. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-29 - PA on-air gate re-review: paTransmitBand, the PA row the
//                 Core holds on the air (paTransmitBandVersion 1), so a
//                 remote window opens and locks the Core's row. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - PA on-air gate review: paOnAirBandIndex is the transmit
//                 band (driveTxBand), held while keyed as Thetis's
//                 _adjustingBand is; transmitBandChanged tells the PA page
//                 and the station's PA publish. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Slice control plan Task 3: setActiveRxFor, each
//                device's active receive slice among the slices it has
//                joined. NereusSDR-original. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Slice control plan Task 5: a remote window holds back a
//                slice request for a slice it only listens to (close, band,
//                sample rate, C-Tune pin and center, NNR diagnostics) and
//                announces it, and every held change of a slice, as
//                sliceRequestHeldForListener. NereusSDR-original. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Slice control fix wave (Important 4): txSliceSelected,
//                emitted when the Core's own window selects a transmit
//                slice (requestTxHandoffToSlice), so the session server
//                records an explicit choice. NereusSDR-original. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Slice control plan Task 16: PanScope scoped rehome,
//                spread and occupancy plus listenedOffPans, so a layout
//                change moves only slices this window controls.
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-29 - Level Cal: rxDisplayCalOffsetDb, applyLevelCalibrationSetting,
//                resetLevelCalibration, levelCalibrationResetAvailable,
//                requestResetLevelCalibration and levelCalibrationChanged.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Level Cal: the ten preamp offsets (rx1_preamp_offset,
//                rx2_preamp_offset, console.cs:1999-2019 [v2.10.3.15]),
//                RX1's saved under RX1_PreampOffsetsDb.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Level Cal: the calibration run as a Core procedure
//                (LevelCalibrationService), its progress as
//                levelCalRunning / levelCalPercent / levelCalMessage /
//                levelCalSucceeded, and the start and cancel calls a
//                local and a remote window both make.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Level Cal: rx2PreampModeAvailable, whether a slice on the
//                other ADC can change RX2's own preamp mode.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - TX rulings (item 1): moxPressAsksOn and tunePressAsksOn, a
//                remote window's press toggles against its own key.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - RADE threads: every RADE slice decodes at once, each on its
//                own decoder thread fed by the DSP thread (m_radeRxRoutes
//                replaces the single RADE target and its main-thread
//                decode and resample). Only the TX slice's channel encodes
//                (refreshRadeTxSelection). Removing a slice destroys its
//                RadeChannel. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-30 - RADE gaps: publishRadeModeSlices hands the DSP worker the
//                slices in RADE mode, on every mode change, slice list
//                change and worker attach, so a RADE slice with no route
//                yet plays silence, not its sideband. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30 - TX safety: isRadioLinkDown, radioLinkDownChanged and
//                transmitLockCoversVox; a lost radio link locks MOX, TUN
//                and 2TONE until it is back (console.cs:27488-27493
//                [v2.10.3.15]). J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-30 - TX safety: retireConnectionForRecovery keeps the lost-link
//                lock through an automatic recovery; disconnectFromRadio
//                (the operator's) lifts it. J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-30 - TGXL tune lane (JJ's ruling): m_tgxlTakePending and
//                m_tgxlTakeGeneration, the take the tuner's front-panel
//                TUNE asks for (ruling 8.9). J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-10-01 - TGXL tune lane fix round: m_tgxlAutotuneTunerPress,
//                m_tgxlPendingTunerPress, the outstanding autotune and
//                tgxlIsPeer. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-10-01 - TGXL tune lane round 2: m_tgxlAnswers (TgxlAnswerTracker)
//                replaces the single outstanding-autotune flag. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01 - TGXL tune lane round 3: m_tgxlLinkEpoch and the per-tune
//                frame and echo counts. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-10-01 - Tune-ended lane: TunerTuneEnd and tunerTuneEndedReason,
//                the words a device's autotune that ends before its carrier
//                keyed is told; cancelTgxlAutotuneFor takes the reason.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01 - Diversity lane: diversityTargetSlice(), the slice diversity
//                runs for, for the Diversity dialog. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
// =================================================================

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#include "core/ConnectionState.h"
#include "core/audio/CaptureSupervisor.h"
#include "core/ReceiveLayoutStore.h"
#include "core/spectrum/WidebandSpectrumCache.h"
#include "core/PgxlConnection.h"
#include "core/TgxlAnswerTracker.h"
#include "core/Rf2ksConnection.h"
#include "core/TgxlConnection.h"
#include "core/FaultLog.h"
#include "core/AmModulationAnalyzer.h"
#include "core/session/IStationLink.h"
#include "core/station/StationRadios.h"
// Remote-daemon R2 Task 18: the handshake descriptor applyStationCapabilities()
// takes by const reference. A by-value struct member of a public method
// signature, so a forward declaration would not do.
#include "core/session/StationCapabilities.h"
#include "core/spectrum/ISpectrumSink.h"
#include "core/TxInterlockPolicy.h"
#include "core/TuneMemoryStore.h"
#include "models/TunerModel.h"
#include "models/ReceiverDspLoadSampler.h"
#include "core/dsp/NnrLoadGovernor.h"
#include <QElapsedTimer>
#include "Band.h"
#include "BandPlanManager.h"
#include "SliceModel.h"
#include "PanadapterModel.h"
#include "MeterModel.h"
#include "TransmitModel.h"
#include "core/Hl2OptionsModel.h"
#include "core/OcMatrix.h"
#include "core/IoBoardHl2.h"
#include "core/HermesLiteBandwidthMonitor.h"
#include "core/RadioStatus.h"
#include "core/SettingsHygiene.h"
#include "core/SliceStreamAllocator.h"
#include "core/accessories/AlexController.h"
#include "core/accessories/ApolloController.h"
#include "core/accessories/PennyLaneController.h"
#include "core/CalibrationController.h"
#include "core/RadioDiscovery.h"
#include "core/RadioConnection.h"
#include "core/HardwareProfile.h"
#include "core/codec/CodecContext.h"  // SliceConfig (Phase 3F Sub-Epic B Task 16)
#include "core/DdcAssignment.h"       // DdcAssignment (Phase 3F Sub-Epic B Task 16)
#include "core/StationNetwork.h"  // StationBind (R-R3-22 station listeners)
#include "core/SkuUiProfile.h"  // issue #257 — setLastBandForTest passes the SKU into refreshAntennasFromAlex
#include "core/safety/SwrProtectionController.h"
#include "core/safety/TxInhibitMonitor.h"
#include "core/safety/BandPlanGuard.h"
#include "core/safety/TxTimeOutTimer.h"

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QPointer>
#include <QMap>
#include <QSet>     // Phase 3F: panBypassState takes PanadapterApplet::associatedSlices
#include <QString>
#include <QList>
#include <QThread>
#include <QPointer>
#include <QVariant> // Remote Daemon R2 Task 8: applyMirroredValue(name, value)

#include <limits>   // 2026-05-22 NaN sentinel for m_lastEmittedRxMeterOffsetDb

// 3M-1a G.1: TxMicRouter is a plain (non-QObject) strategy interface.
// Include required directly so unique_ptr destructor is available here.
#include "core/TxMicRouter.h"
// (Phase 3M-1c L.4 added a `core/audio/MicReBlocker.h` include for the
//  unique_ptr<MicReBlocker> destructor.  The TX pump architecture
//  redesign (2026-04-29) deleted MicReBlocker; replaced with
//  TxWorkerThread which drives TxChannel directly.)
#include <algorithm>  // std::clamp (used by computeWireDriveForTest)
#include "core/session/RemoteMicSource.h"
#include <atomic>     // AM Mod Monitor flags
#include <array>      // std::array (HL2 temp averaging ring)
#include <functional> // R-R3-21 DSP > Options apply observer (test seam)
#include <vector>
#include <memory>  // std::unique_ptr
#include <mutex>   // R-R3-39 RxWorkerTarget
#include <optional>
#include <vector>

namespace NereusSDR { class VoltsAmpsLog; }

namespace NereusSDR {

class AppSettings;
class StationServer;
class StationSliceOwnershipPolicy;
class SessionTransport;
struct SessionMessage;
enum class PreampMode;

class ReceiverManager;
class RemoteDevicesState;
class AudioEngine;
class MasterMixAudioTap;
class WdspEngine;
class RxDspWorker;
class DspControlThread;
class NoiseFloorTracker;
// Remote Daemon R2 Task 12: per-slice S-meter QTimer (src/core/meters/).
// Owned directly (constructed in the constructor body, Role::Local only);
// see wdspEngine()'s neighbouring accessor below and RadioModel's own
// constructor for the role gate.
class SliceMeterPump;
// Phase 3F Sub-Epic F Task 5: per-ADC wideband FFT engine. Forward decl
// here; included in RadioModel.cpp so we don't pull fftw3.h into every
// translation unit that touches RadioModel.h.
class WidebandFftEngine;
// 3M-1a G.1: forward declarations for TX-side components.
class CatService;
class MoxController;
struct KeyerIdentity;
class TxChannel;
// Phase 3F Sub-Epic J Task 11: forward decl for rxChannelForSlice()'s
// return type (see below, near txChannel()).
class RxChannel;
// Phase 3F Sub-Epic C: TX-slice arbiter (single-TX invariant + RF-safe handoff).
class TxSliceArbiter;
class UnkeyGate;
// 3M-1b L.1: forward declarations for mic-source strategy objects.
class PcMicSource;
class RadioMicSource;
class VaxTxMicSource;  // VAX TX consumer (added 2026-05-06).
class CompositeTxMicRouter;
// 3M-1c L.1 / L.2: forward declarations for the MicProfileManager bank
// (chunk F) + the TwoToneController activation orchestrator (chunk I).
class MicProfileManager;
class TwoToneController;
// 3M-4 Task 7: PureSignal coordinator (cal lifecycle, MOX integration,
// auto-attention, polling, save/restore, two-tone wiring).
class PureSignal;
class PureSignalSettings;
struct Ps3RoutingSnapshot;
class DspAssetService;
class PureSignalSessionFacade;
class StepAttenuatorFacade;
class AlexAntennaFacade;
class LanDiscovery;  // group B fix wave (M2): the running scans
class IoBoardHl2Facade;
class PsccPump;
// Phase 4 Agent 4A of issue #167: PaProfileManager forward declaration.
// RadioModel owns the per-MAC PA gain profile bank (parallel to
// MicProfileManager); the active profile is passed by reference to
// TransmitModel::setPowerUsingTargetDbm at every drive-slider /
// TUNE / two-tone callsite.
class PaProfileManager;
// 3M-1c TX pump architecture redesign — TxWorkerThread.
class TxWorkerThread;
// iPhone app plan Task 36: the remote microphone ring.
class RemoteMicFeed;
class TxDisplayFeed;
class TransmitState;
// Stage C2 filter preset editor — user-override layer over Thetis defaults.
class FilterPresetStore;

// Phase 3J-2 H2: spot-system forward declarations. RadioModel owns the
// seven spot-ingest clients (DxCluster, RBN, WSJT-X, SpotCollector,
// POTA, FreeDV Reporter, PSK Reporter), the three view models
// (SpotModel, FreeDVStationModel, RxDecodeModel), and the
// DxccColorProvider. Each client's spotReceived(DxSpot) signal lands
// in a per-source adapter slot that builds the QMap<QString,QString>
// kvs SpotModel::applySpotStatus expects.
class DxClusterClient;
class WsjtxClient;
class SpotCollectorClient;
class PotaClient;
class FreeDVReporterClient;
class FreeDVRadeReporterBridge;
class PskReporterClient;
class SpotSourceHost;
struct RecordBatch;

class DxccColorProvider;
class SpotModel;
class SpotTableModel;
class FreeDVStationModel;
class RxDecodeModel;
// TNF (design section 5): the canonical notch store, owned by RadioModel
// alongside SpotModel. One list shared by every slice (design D1) because
// notch centres are absolute RF Hz, so a 20 m notch is inherently inert on a
// 40 m slice.
class NotchModel;
struct DxSpot;
struct FreeDVStation;

// Phase 3R Task I5: forward declaration for the RadeChannel codec wrapper.
// RadioModel does not own the channel (J2 / J3 create one per slice as
// mode flips to RADE), but exposes wireRadeChannel(sliceId, channel, slice)
// to attach the channel's snrChanged / syncChanged / rxTextDecoded
// signals into the slot graph.
class RadeChannel;

// Phase 3R K-bench forward decl: Resampler is used by RadioModel to
// upsample RadeChannel's 24 kHz baseband output to the radio's TX
// I/Q wire rate before m_connection->sendTxIq.  Lives in core/Resampler.h.
class Resampler;
class StationTgxlController;
class StationPgxlController;
class StationRfKitController;
class StationTciController;
class StationTciModel;
class SliceOwnership;
class RfKitBandFollow;
class AmplifierModel;
class RfKitModel;
class AccessoryDataModel;
class AccessorySettingsModel;
class StationAccessoryData;
class ConnectionDiagnostics;

// RadioModel is the central data model for a connected radio.
// It owns the RadioConnection (on a worker thread), ReceiverManager,
// and all sub-models. It routes signals between components.
//
// Thread architecture:
//   Main thread: RadioModel, ReceiverManager, all sub-models, GUI,
//                AudioEngine (timer-driven QAudioSink drain)
//   Connection thread: RadioConnection (sockets, protocol I/O)
//   DSP thread:  RxDspWorker — runs RxChannel::processIq → fexchange2;
//                kept off main because WDSP fexchange2 with bfo=1 can
//                block on Sem_OutReady and would otherwise freeze the
//                Qt event loop, deadlocking against wdspmain.
class RadioModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString settingsSaveError READ settingsSaveError NOTIFY settingsSaveErrorChanged)
    Q_PROPERTY(QString receiveLayoutRestoreState READ receiveLayoutRestoreState NOTIFY receiveLayoutRestoreStatusChanged)
    Q_PROPERTY(QString receiveLayoutRestoreMessage READ receiveLayoutRestoreMessage NOTIFY receiveLayoutRestoreStatusChanged)
    Q_PROPERTY(QString name        READ name        NOTIFY infoChanged)
    Q_PROPERTY(QString model       READ model       NOTIFY infoChanged)
    Q_PROPERTY(QString version     READ version     NOTIFY infoChanged)
    Q_PROPERTY(bool    connected   READ isConnected NOTIFY connectionStateChanged)
    // R-R3-47: the station's RF-Kit switch, Core to window only. A window
    // changes it with the setRfKitEnabled command; a raw write is refused.
    Q_PROPERTY(bool rfKitEnabled READ rfKitEnabled NOTIFY rfKitEnabledChanged)
    // R-R3-22: station-owned configuration/status; changes use a typed verb.
    Q_PROPERTY(bool fourO3AEnabled READ fourO3AEnabled NOTIFY fourO3AStatusChanged)
    Q_PROPERTY(bool fourO3AListening READ fourO3AListening NOTIFY fourO3AStatusChanged)
    Q_PROPERTY(QString fourO3AListenerError READ fourO3AListenerError NOTIFY fourO3AStatusChanged)
    // Station-owned preselector telemetry, never remotely writable.
    Q_PROPERTY(int rxFilter0Mode READ rxFilter0Mode NOTIFY filterStateChanged)
    Q_PROPERTY(int rxFilter0Effective READ rxFilter0Effective NOTIFY filterStateChanged)
    Q_PROPERTY(int rxFilter0Band READ rxFilter0Band NOTIFY filterStateChanged)
    Q_PROPERTY(QString rxFilter0Reason READ rxFilter0Reason NOTIFY filterStateChanged)
    Q_PROPERTY(int rxFilter1Mode READ rxFilter1Mode NOTIFY filterStateChanged)
    Q_PROPERTY(int rxFilter1Effective READ rxFilter1Effective NOTIFY filterStateChanged)
    Q_PROPERTY(int rxFilter1Band READ rxFilter1Band NOTIFY filterStateChanged)
    Q_PROPERTY(QString rxFilter1Reason READ rxFilter1Reason NOTIFY filterStateChanged)
    // R-R3-49: the Core's radio is keyed, Core to window only. True from
    // the moment MoxController starts a key (its MOX button, a hardware
    // PTT, CAT, TCI, TUNE or two-tone) until its TX to RX handover ends.
    Q_PROPERTY(bool transmitting READ isTransmitting NOTIFY transmittingChanged)
    // R-R3-49 (parity Task 6, carried from gaps Task 13): the Core's TX
    // inhibit (TxInhibitMonitor::inhibited()), Core to window only, so a
    // remote window's TX badge shows the Core's inhibit.
    Q_PROPERTY(bool txInhibited READ isTxInhibited NOTIFY txInhibitedChanged)
    // Plan Task 14 fix wave (R-R3-49): the band-output (OC) byte the Core's
    // connection composed, the band it was chosen for and whether the
    // transmitter was keyed. Station-owned, never remotely writable. Every
    // window's band-output displays show these, never a byte of their own.
    Q_PROPERTY(int bandOutputsByte READ bandOutputsByte NOTIFY bandOutputsChanged)
    Q_PROPERTY(int bandOutputsBand READ bandOutputsBand NOTIFY bandOutputsChanged)
    Q_PROPERTY(bool bandOutputsKeyed READ bandOutputsKeyed NOTIFY bandOutputsChanged)
    // R-R3-49 / R-R3-21 / R-R3-40 (remote-window parity Task 16, dspInfoVersion
    // 1): how long the Core's last DSP Options apply took, in ms (0 before
    // any). Core to window only. Which noise reduction the Core runs comes
    // from DspAssetService (dfnrRunnable, mnrRunnable), the one source.
    Q_PROPERTY(qint64 dspOptionsLastApplyMs READ dspOptionsLastApplyMs
                   NOTIFY dspOptionsLastApplyMsChanged)
    // Fix wave after parity Tasks 19 and 21 (M2, R-IOS-18): why the Core
    // has no radio (it waits for a choice, for its chosen radio to appear,
    // or for a radio another program holds), in plain words; empty while it
    // has one or is connecting one. Set by nereusd's DaemonApp; Core to
    // window only. This Core shows it.
    Q_PROPERTY(QString stationRadioWaiting READ stationRadioWaiting
                   NOTIFY stationRadioWaitingChanged)
    // Remote-window parity Task 22 (R-R3-49, supportBundleVersion 1): the
    // Core's enabled logging categories, their ids joined by commas in the
    // Support dialog's order. On a local model this process's LogManager;
    // a remote window holds the Core's (applyMirroredValue) and changes it
    // with support.setLogCategories. Core to window only.
    Q_PROPERTY(QString logCategories READ logCategories NOTIFY logCategoriesChanged)
    // Phone wire batch (logCategoryListVersion 1): every logging category
    // the Support dialog lists, with the label its checkbox shows
    // (LogManager::categoryListJson), so a device names a category it was
    // not built with. Fixed for the life of the process: CONSTANT, sent in
    // the snapshot only, and only to a peer that declared logCategoryList.
    // Declared last so every earlier property keeps its wire ordinal.
    Q_PROPERTY(QString logCategoryList READ logCategoryList CONSTANT)
    // HL2 port part 2 (txInhibitReasonVersion 1): why the Core's transmit
    // is held off, in plain words, when the reason is more than the plain
    // TX inhibit: the HL2 I/O board's "I/O Board: Fault Code N" (mi0bot
    // console.cs:25876-25885 [@c26a8a4]). Empty otherwise. Core to window
    // only, and only to a peer that declared txInhibitReason. Declared
    // after logCategoryList so every earlier property keeps its wire ordinal.
    Q_PROPERTY(QString txInhibitReason READ txInhibitReason NOTIFY txInhibitReasonChanged)
    // The Alex-1 low-pass filter bits (Thetis SetAlexLPFBits: 0x01 20m,
    // 0x02 40m, 0x04 80m, 0x08 160m, 0x10 6m, 0x20 10m, 0x40 15m) the Core's
    // connection last selected, or -1 before any. Station-owned, never
    // remotely writable; every window's Alex tab lamp shows it. Sent only
    // to a peer that declared alexLpf. Declared after txInhibitReason so
    // every earlier property keeps its wire ordinal.
    Q_PROPERTY(int alexLpfBits READ alexLpfBits NOTIFY alexLpfBitsChanged)
    // PA on-air gate re-review, Important C (paTransmitBandVersion 1): the
    // PA row the Core holds on the air (paOnAirBandIndex, Thetis
    // _adjustingBand), -1 when its transmit band has no PA values. Core to
    // window only, and only to a peer that declared paTransmitBand; a
    // window opens and locks this row, not its own slice's. Declared last
    // so every earlier property keeps its wire ordinal.
    Q_PROPERTY(int paTransmitBand READ paTransmitBand NOTIFY paTransmitBandChanged)
    // Level Cal (levelCalibrationVersion 1): the Core's calibration run.
    // Running, its percent done, the sentence it ended with (empty while
    // it runs or before one ran) and whether it finished. Core to window
    // only, and only to a peer that declared levelCalibration. Declared
    // last so every earlier property keeps its wire ordinal.
    Q_PROPERTY(bool levelCalRunning READ levelCalRunning NOTIFY levelCalStateChanged)
    Q_PROPERTY(int levelCalPercent READ levelCalPercent NOTIFY levelCalStateChanged)
    Q_PROPERTY(QString levelCalMessage READ levelCalMessage NOTIFY levelCalStateChanged)
    Q_PROPERTY(bool levelCalSucceeded READ levelCalSucceeded NOTIFY levelCalStateChanged)
    // Shared-input filters, ruling (d) 2026-09-30: why the receive low-pass
    // on chain 0's input is held for one slice, and that slice's id (-1 when
    // none is). Peer-only, to a peer that declared rxFilterLowPass 1
    // (rxFilterLowPassVersion 1). Declared last so every earlier property
    // keeps its wire ordinal. They notify on lowPassHoldChanged, not
    // filterStateChanged: a peer without the feature has both removed from
    // a delta (StationServer::fitPeerOnlyProperties), and a delta left with
    // nothing is not sent, so a hold alone reaches that peer as nothing, as
    // before the feature (link document, section 17).
    Q_PROPERTY(QString rxFilter0LowPassReason READ rxFilter0LowPassReason NOTIFY lowPassHoldChanged)
    Q_PROPERTY(int rxFilter0LowPassSlice READ rxFilter0LowPassSlice NOTIFY lowPassHoldChanged)
    Q_PROPERTY(QString diversityState READ diversityState NOTIFY diversityStateChanged)


public:
    // The CW pitch the APF centre is offset from: APF freq = CWPitch +
    // tuneOffset. From Thetis setup.cs:17071 [v2.10.3.13]; CW pitch default
    // 600 Hz from Thetis console.cs. Setup > DSP > CW's APF Center Freq
    // uses the same value (R-R3-21).
    static constexpr int kApfCwPitchHz = 600;

    // Remote-daemon R2 Task 4: which side of the wire this model's DSP
    // lives on. Local (default; unchanged behavior) owns a
    // RadioConnection and drives WdspEngine + AudioEngine from
    // connectToRadio() exactly as it always has. Remote never enters
    // connectToRadio()'s body -- see the early return there -- so it
    // holds no socket and starts neither WdspEngine nor AudioEngine; it
    // is the shape a GUI-only process needs to drive a daemon-owned radio
    // over the wire (R2 Task 18's wss session) instead of dialing one up
    // locally.
    enum class Role { Local, Remote };

    explicit RadioModel(QObject* parent = nullptr);
    // Remote-daemon R2 Task 4: explicit-role overload. Two constructors
    // sharing a defaulted trailing QObject* stay unambiguous as long as
    // the new one's first parameter is Role, so every existing
    // RadioModel(parent) / RadioModel() call site (MainWindow.cpp,
    // DaemonApp.cpp, every test) keeps resolving to the constructor above
    // and keeps defaulting to Role::Local without any change on its end.
    RadioModel(Role role, QObject* parent = nullptr);
    ~RadioModel() override;

    // Remote-daemon R2 Task 4.
    Role role() const { return m_role; }
    void setReceiveOnlyStationPolicy(bool receiveOnly);
    bool receiveOnlyStationPolicy() const { return m_receiveOnlyStationPolicy; }

    // R-R3-36: PC microphone session demand. While a local radio
    // connection is up, PC capture is allowed and the mic source is Pc,
    // this model holds one LocalSession capture lease on its AudioEngine
    // (acquired after AudioEngine::start() returns, released when the
    // source leaves Pc and in teardown after the TX worker has stopped).
    // Default true; DaemonApp sets false before connecting so nereusd
    // never starts the capture helper. A Role::Remote model never holds a
    // lease. Owner thread only.
    void setPcCaptureAllowed(bool allowed);
    bool pcCaptureAllowed() const { return m_pcCaptureAllowed; }

    // R-R3-44: Role::Remote only. Every slice of a remote model keeps its
    // VAX channel through this store (SliceModel::setVaxChannelStore) and
    // never writes the Core's Slice<N>/VaxChannel: a remote window's VAX
    // channels are this computer's. Until one is set a pick is kept on the
    // slice alone. Owner thread.
    void setRemoteVaxChannelStore(SliceModel::VaxChannelStore store);
    // True when the mic source is Pc and local keying would read PC
    // capture (a Role::Local model with PC capture allowed).
    bool pcCaptureRequired() const;

    // Remote-daemon R2 Task 4: non-owning attach point for the
    // control-plane station-link seam (core/session/IStationLink.h). A
    // later task's StateMirror uses this to push slice/meter/status state
    // to, and apply command verbs from, a remote GUI when
    // role() == Remote. Held the same non-owning way m_spectrumSink is:
    // whoever constructs the link owns its lifetime, RadioModel never
    // allocates or deletes it.
    void attachStation(NereusSDR::IStationLink* link) { m_station = link; }
    IStationLink* stationLink() const { return m_station; }
    bool requestMicSource(MicSource desired, std::function<void()> accepted = {});
    QString micSourceChangeReason(MicSource desired) const;
    bool pcCaptureGatesKeyingForTest() const { return pcCaptureGatesKeying(); }

    void detachStation() { m_station = nullptr; }
    void reportStationLinkStateChanged();

    // ── Remote-daemon R2 Task 18: the production handshake entry points ──
    //
    // Before this task the ONLY way anything outside RadioModel could move
    // m_connectionState was setConnectionStateForTest(), a test seam
    // (above). Every production transition ran off a RadioConnection
    // signal, which a Role::Remote model does not have and never will --
    // connectToRadio() early-returns for Remote (Task 4). Without a
    // production entry point, Task 3's storage-backed isConnected() is
    // never true on a remote client, maxSlices() keeps returning its
    // disconnected default of 1, and roughly fourteen GUI sites keep
    // reading disconnected while a real radio is being driven.
    //
    // Both methods are Role::Remote ONLY, and refuse (no-op plus a warning)
    // on a Role::Local model. A Local model's connection state is derived
    // from its own socket and must keep exactly one writer; letting a
    // second one in "just in case" is how the two silently disagree.

    /// Apply what the daemon advertised at handshake (StationServer's
    /// Capabilities message) and, if it reports a live radio, drive this
    /// model to Connected.
    ///
    /// Writes station identity (name / model / version / MAC / board type
    /// and therefore boardCapabilities()), the EFFECTIVE slice limit and
    /// userDdcCount, and finally the connection state. In that order,
    /// deliberately: connectionStateChanged() is what wakes every GUI site
    /// that then reads maxSlices() and boardCapabilities(), so the state
    /// change has to be last or those sites re-read stale values.
    ///
    /// Deliberately does NOT call configureStreamPool(). Task 5 made that
    /// call a no-op for Role::Remote precisely so a capability apply
    /// cannot re-arm a local stream allocator the daemon is the only one
    /// placing slices against; this method must not route around that by
    /// reaching configureStreamPoolImpl() instead.
    ///
    /// `caps.radioConnected == false` (an authenticated daemon whose radio
    /// is powered off or unreachable) applies identity and limits but
    /// leaves the state Disconnected: every slice control a Connected
    /// client offers would otherwise reach a RadioModel that cannot act.
    ///
    /// R-R3-46: the stored radio info (MAC, board, name, firmware, and the
    /// Core's protocol and address when it reports them) and the hardware
    /// profile follow the Core's radio: the Core's own model wins when it
    /// matches the board (so an ANAN-8000DLE or ANAN-G2 1K keeps its own
    /// row), an unknown board resolves to Unknown and never to Hermes
    /// (profileForStation()). currentRadioChanged is emitted once per
    /// identity change, after the profile is set and after the connection
    /// state, as a local connect emits it after Connected.
    void applyStationCapabilities(const NereusSDR::StationCapabilities& caps);
    int stationRemoteIqVersion() const { return m_stationRemoteIqVersion; }

    /// Drive the connection lifecycle from the session directly. Task 18
    /// uses it for the close side (a preempted or refused session goes back
    /// to Disconnected); Task 19 owns the link-loss and reconnect
    /// transitions built on it.
    void setStationConnectionState(ConnectionState s);

    /// The EFFECTIVE user DDC count the station advertised, 0 before any
    /// handshake. Deliberately NOT the same read as
    /// boardCapabilities().userDdcCount: that is the board's own number,
    /// and parent design section 4.5 is explicit that a client gates on
    /// what the DAEMON can sustain, which on the Pi 4 floor can be fewer.
    /// The two agree today only because R2 builds no PerfMonitor to
    /// narrow anything. Meaningful for Role::Remote only.
    int stationUserDdcCount() const { return m_stationUserDdcCount; }

    /// The number of receive streams (independent DDCs) slices get on this
    /// radio: the one stream count every reader uses (plan Task 11).
    /// Role::Remote: what the Core advertised (stationUserDdcCount()).
    /// Otherwise BoardCapsTable::userDdcCountFor(boardCapabilities(), the
    /// protocol in use): the connected radio's protocol once one has been
    /// chosen, the board row's own protocol before that (a test-primed
    /// board). Protocol 1 gives at most four. On Protocol 2 a receiver
    /// count the radio reported in discovery caps it
    /// (RadioInfo::reportedReceivers; 0 keeps the board row's count).
    int userStreamCount() const;

    /// The ReceiverManager ceiling connectToRadio sets for `info` and a
    /// stream pool of `poolStreams`. Protocol 1: the radio's count as
    /// discovery left it (unchanged). Protocol 2: never below the pool, so
    /// every stream has a receiver behind it; with a report the pool is
    /// already within it, and without one (the table fallback, or a saved
    /// radio's default of 4) the pool's own size is the need.
    static int receiverPoolCeiling(const RadioInfo& info, int poolStreams);

    /// The ceiling setActiveRxCountLive clamps a requested receiver count
    /// to: BoardCapsTable::effectiveReceiverCount for the board profile and
    /// the connected radio's report (1 before a board is known).
    int maxActiveRxCount() const;

    /// Create a slice under the id the STATION chose rather than minting
    /// one locally. Role::Remote only.
    ///
    /// RadioModel::addSlice() mints the lowest id not currently in use, and
    /// removeSlice() never renumbers survivors, so a daemon holding slices
    /// {0, 2} cannot be reproduced on a client that only has addSlice():
    /// replaying create(0) then create(2) would produce {0, 1}. Every wire
    /// message naming a slice carries SliceModel::sliceIndex() (parent
    /// design section 7.1's general rule), so a client whose ids drifted
    /// from the station's would silently address the wrong slice on every
    /// command verb it sent afterwards.
    ///
    /// Returns the id on success, or -1 if the id is negative, already in
    /// use here, or this is a Role::Local model.
    int addSliceWithStationId(int sliceId, const QString& initialPanId = QString());

    // ── Remote-daemon R2: the station's answers coming back ─────────────
    //
    // The three entry points below are the INBOUND half of the command
    // routing added alongside them (see the Role::Remote branches in
    // addSlice / addSliceOnPan / removeSlice / setActiveSliceById /
    // requestSliceSampleRate). They exist because the outbound half turns
    // those five entry points into wire verbs on a Role::Remote model, so
    // the session can no longer reach the local bodies through them: a
    // StationClient calling removeSlice() to apply the daemon's OWN
    // object.destroy would bounce that destroy straight back at the
    // daemon as a fresh removeSlice command.
    //
    // Same shape and same reasoning as addSliceWithStationId above, which
    // was already the inbound half of creation for exactly this reason.

    /// Remove the slice the STATION has destroyed. Role::Remote only.
    ///
    /// The inbound twin of addSliceWithStationId: this is the daemon's
    /// object.destroy landing, not an operator asking for a removal, so
    /// it runs the local removal body rather than sending a verb.
    void removeSliceWithStationId(int sliceId);
    /// iPhone app plan Task 78 (the several-devices design, section 12): a
    /// remote window that shares the Core as a device accepts the Core
    /// closing its last slice (another device took its receiver) and
    /// shows an empty band; any other window keeps at least one slice, as
    /// before. Role::Remote only; set by StationClient per session.
    void setStationMayCloseLastSlice(bool may) { m_stationMayCloseLastSlice = may; }
    bool stationMayCloseLastSlice() const { return m_stationMayCloseLastSlice; }
    /// Task 78: the window's copy of who else is on the Core (set by the
    /// StationClient that feeds it; null in a local window).
    void setStationDevices(NereusSDR::RemoteDevicesState* devices);
    NereusSDR::RemoteDevicesState* stationDevices() const;

    /// Adopt the station's choice of active slice. Role::Remote only.
    ///
    /// SliceModel::active alone is not enough. StationClient applies the
    /// mirrored `active` flag onto each SliceModel, but RadioModel's own
    /// m_activeSlice pointer is moved only by setActiveSlice(), so
    /// without this every activeSlice()-reading surface on a remote GUI
    /// (the container S-meter, the RX applet, the DSP menu, the band
    /// buttons) stays stranded on whichever slice was created first.
    /// Silently ignores an id this client does not hold.
    void applyStationActiveSlice(int sliceId);

    /// The station refused a slice command this client sent, with its own
    /// reason. Role::Remote only.
    ///
    /// Routed to sliceAddRejected, the channel MainWindow already toasts
    /// (MainWindow.cpp, the sliceAddRejected connect). A remote refusal
    /// that reached no operator-facing signal would be indistinguishable
    /// from the silent divergence this whole round exists to close: the
    /// click does nothing and nothing says why.
    void reportStationSliceCommandRejected(const QString& reason);

    /// R-R3-47 / R-R3-22 / R-R3-48: the Core refused an accessory request,
    /// with its own reason. `device` says what it was about: "pgxl",
    /// "tgxl", "rfkit", "interlock", "tci", "4o3a", or for a fault history
    /// the device it names ("faults" for any other). Role::Remote only.
    /// Routed to accessoryRequestRefused, which MainWindow toasts and the
    /// pages that sent the request show, never to the slice toast.
    void reportStationAccessoryRefusal(const QString& device, const QString& reason,
                                       quint32 commandId = 0);
    /// Follow-up 3 / rework part 5: `page` sent request `commandId` and
    /// shows the Core's refusal of it itself. The refusal counts as shown
    /// (accessoryRequestRefused's shownOnPage, so MainWindow does not toast
    /// it too) only if the page still exists and is visible when it
    /// arrives; otherwise it is toasted. Claims end when the command
    /// completes or the link to the Core drops. 0 is ignored.
    void noteAccessoryRequestShownOnPage(quint32 commandId, QObject* page);
    /// Follow-up 6: one of the Core's station settings changed (`key`), or
    /// a whole snapshot arrived (empty). Role::Remote only; emits
    /// stationSettingChanged for the pages that show those settings.
    void reportStationSettingChanged(const QString& key);

    /// R-R3-22 fix wave: the Core answered command `commandId` (the id in
    /// IStationLink::CommandOutcome). `reason` is the Core's words for a
    /// refusal, empty when accepted. Role::Remote only. Routed to
    /// stationCommandFinished for a sender that waits on its own command.
    /// Ends any page's claim on the command (noteAccessoryRequestShownOnPage).
    void reportStationCommandFinished(quint32 commandId, bool accepted, const QString& reason);
    /// Remote role only: a paired Core settings export completed or failed.
    /// coreXml is populated only after length, digest and XML validation.
    void reportStationSettingsBackupExportFinished(quint32 operationId, bool accepted,
                                                    const QString& reason,
                                                    const QByteArray& coreXml);
    /// R-R3-49 (parity Task 8): the Core answered a window's scanTgxlLan
    /// (`devicesJson` the answer's JSON array, empty on a refusal).
    /// Role::Remote only. Routed to stationTgxlLanScanFinished.
    void reportStationTgxlLanScan(quint32 commandId, bool accepted, const QString& reason,
                                  const QString& devicesJson);
    /// R-R3-49 (parity Task 9): the same for a window's scanPgxlLan.
    /// Routed to stationPgxlLanScanFinished.
    void reportStationPgxlLanScan(quint32 commandId, bool accepted, const QString& reason,
                                  const QString& devicesJson);

    /// The station refused a sample-rate change, with its own reason.
    /// Role::Remote only. Routed to sliceRetuneRejected, which carries the
    /// slice id and is separately toasted by MainWindow, because that is
    /// the signal a rate or retune refusal already uses locally.
    void reportStationRetuneRejected(int sliceId, const QString& reason);

    // ── Remote-daemon R2 Task 20: the local-DSP capability gate ─────────
    //
    // The single authority a GUI surface asks before reaching for anything
    // whose implementation only exists when the DSP runs in THIS process.
    // src/gui does not re-derive this from role(): one predicate means one
    // place to widen when a later phase (R4's TX path) makes part of it
    // true for a remote client too.
    bool ownsLocalDsp() const { return m_role == Role::Local; }

    // R-R3-49: the Network Watchdog (Setup > General > Options) is a radio
    // setting, applied where the radio is. setNetworkWatchdogEnabled saves
    // it (a remote window's save goes to the Core) and applies it to this
    // model's own radio, if it has one. applyNetworkWatchdog applies a value
    // without saving it: the Core calls it when a window's change arrives.
    // The connect path applies the saved value before the radio starts.
    // Default on, as Thetis: setup.designer.cs:8434 [v2.10.3.15]
    //   this.chkNetworkWDT.Checked = true;
    static constexpr bool kNetworkWatchdogDefault = true;
    static bool networkWatchdogSetting();
    void setNetworkWatchdogEnabled(bool enabled);
    void applyNetworkWatchdog(bool enabled);

    // R-R3-49 (parity Task 5): Setup > Transmit > Power's SWR Protection
    // group (SwrProtectionEnabled, SwrProtectionLimit,
    // SwrTuneProtectionEnabled, TunePowerSwrIgnore, WindBackPowerSwr),
    // applied to this model's SwrProtectionController at once, as Thetis
    // applies each box when it changes. `value` is the saved string; an
    // invalid QVariant (the key removed) applies the default the
    // constructor reads. The local page calls it after saving; the Core
    // calls it for a window's accepted change (StationServer). False for
    // any other key.
    bool applySwrProtectionSetting(const QString& key, const QVariant& value);
    // R-R3-13 / R-R3-49 (remote-window parity Task 15): Setup > Display >
    // Multimeter's polling delay (MultimeterDelayMs) sets this model's
    // SliceMeterPump rate at once, clamped to the pump's [10, 2000] ms, as
    // Thetis applies udDisplayMeterDelay when it changes. `value` is the
    // saved string; an invalid QVariant (the key removed) applies the 100 ms
    // default the pump's constructor reads. The Core calls it for a
    // window's accepted change (StationServer); the local page sets the
    // pump itself. False for any other key or with no pump.
    bool applyMeterSetting(const QString& key, const QVariant& value);
    // R-R3-49 / R-R3-21 (remote-window parity Task 30): one of Setup >
    // Display > TX Display's nine analyzer keys (TxAnalyzer::isSettingsKey)
    // reaches this model's TX analyzer at once, keyed or not, through the
    // setter the local page calls (TxAnalyzer::reloadSetting). The Core
    // calls it for a window's accepted write or removal (StationServer).
    // Any other key, or no analyzer, does nothing.
    void applyRemoteTxDisplaySetting(const QString& key);
    // Parity ruling C12: a per-band grid dB max or min (DisplayGridMax_ /
    // DisplayGridMin_, the Core's settings) changed; every pan re-reads
    // that band (PanadapterModel::applyStationGridSetting). The Core calls
    // it for a window's accepted write or removal (StationServer), so its
    // pan on that band takes the new range and the window sees it; a
    // remote window's model calls it for each Core setting that arrives.
    void applyPanGridSetting(const QString& key);
    static bool isSwrProtectionSettingKey(const QString& key);

    // Setup > Transmit > Power's "Disable HF PA" (DisableHfPa, Thetis
    // chkHFTRRelay -> console.HFTRRelay), applied at once: the connection's
    // DisablePA bit (RadioConnection::setPaDisabled) and the SWR
    // protection's pass (SwrProtectionController::setHfPaDisabled). Thetis
    // offers the box on every radio except the Hermes and the Atlas kit
    // (hfPaSwitchAvailable); on those it is off. The local page calls it
    // after saving, the connect path on connect, and the Core for a
    // change to the Core's settings, from a window or the Core itself
    // (StationServer). A model with no radio of its own (a remote window)
    // does nothing.
    static constexpr const char* kDisableHfPaKey = "DisableHfPa";
    static bool hfPaSwitchAvailable(HPSDRModel model) noexcept;
    static QString hfPaSwitchUnavailableReason();
    // Setup > Audio > TX Input's radio mic groups (radio codec lane).
    // Thetis greys out the whole ORION mic panel (Tip/Ring, PTT, Bias) on
    // the Red Pitaya (orionMicPanelAvailable); the desktop's Orion group and
    // the Setup description's Orion rows are shown disabled with this
    // reason. The Hermes Lite 2 takes the radio mic only with its AK4951
    // audio add-on board, which the gateware cannot report, so Radio Mic
    // stays open there with radioMicAddOnNote beside it.
    static bool orionMicPanelAvailable(HPSDRModel model) noexcept;
    static QString orionMicPanelUnavailableReason();
    static QString radioMicAddOnNote();
    // Alex-1 Filters' "6m/ByPass on RX" on a radio Thetis hides it on
    // (codec::alex::lpfBypassAvailable): the desktop's box and the Setup
    // description's row are shown disabled with this reason.
    static QString lpfBypassUnavailableReason();
    // Reads this computer's saved value.
    void applyDisableHfPaSetting();
    // `value` is the saved string; an invalid QVariant (the key removed)
    // applies the default, off.
    void applyDisableHfPaSetting(const QVariant& value);
    // Pushes the saved HL2 options the radio takes on the wire (Band Volts,
    // Disable PS Sync, TX buffer latency, PTT hang, reset on Ethernet
    // disconnect, External 10 MHz, Enable CL2, CL2 frequency) to a P1
    // connection. Nothing without one.
    void applyHl2Options();

    // Task 13: External TX Inhibit (Setup > Transmit > Power, grpExtTXInhibit)
    // is a Core setting: the gate sits where the radio is. The setters save
    // (a remote window's save goes to the Core) and apply to this model's
    // own TxInhibitMonitor; the Core applies a window's change through
    // StationServer. Default off, as Thetis: console.cs:15336-15337
    // [v2.10.3.15] _useTxInhibit = false, _reverseTxInhibit = false.
    // From Thetis setup.cs:16660-16667 [v2.10.3.15]
    // (chkTXInhibit_CheckedChanged / chkTXInhibitReverse_CheckedChanged).
    void setUseTxInhibit(bool on);
    void setReverseTxInhibit(bool on);

    // Task 16 (receiver and transmit gaps plan): receive only, Thetis
    // console.RXOnly (console.cs:15312-15334 [v2.10.3.15]) and Setup's
    // chkGeneralRXOnly (setup.cs:6479 [v2.10.3.15]).
    //
    // A Core setting ("RxOnly", SettingsScope::Station): the gate sits where
    // the radio is. setRxOnly saves it (a remote window's save goes to the
    // Core) and applies it to this model; applyRxOnlySetting applies a value
    // without saving it (the Core, when a window's change arrives). A remote
    // window re-reads it when the Core's copy changes
    // (stationSettingChanged), so its controls show the Core's state.
    //
    // isRxOnly() is the effective state: the setting, or a radio with no
    // transmitter (BoardCapabilities::isRxOnlySku, the HL2 receive-only
    // kit). The kit always runs receive only; that is NereusSDR's own rule,
    // since mi0bot-Thetis has no kit model, only the operator's toggle.
    // MoxController holds the gate (setRxOnly); rxOnlyChanged tells Setup
    // and the transmit buttons.
    static bool rxOnlySetting();

    // JJ's ruling (2026-09-28, addendum G-42): Thetis's Extended
    // (chkExtended, console.Extended; console.cs:6780 and :6818
    // [v2.10.3.15] return true from CheckValidTXFreq and
    // checkValidTXFreq_local while it is on) as one Core setting,
    // "ExtendedTransmit" (SettingsScope::Station), default off. The old
    // per-computer "ExtendedTxAllowed" is never read. The transmit gate
    // reads this at every key; StationServer takes a device's change only
    // with transmit permission and off the air.
    static constexpr const char* kExtendedTransmitKey = "ExtendedTransmit";
    static bool extendedTransmitSetting();
    // JJ's ruling (2026-09-29): Thetis's "Prevent TX'ing on a different
    // band to the RX band" (chkPreventTXonDifferentBandToRX,
    // _preventTXonDifferentBandToRXband; console.cs:20843 and
    // :29451-29465 [v2.10.3.15]) as one Core setting under Thetis's key
    // name (SettingsScope::Station), default off. The transmit gate reads
    // it at every key: when the transmitting slice is not its device's
    // active slice, its band is compared with the active slice's (Thetis
    // compares split TX with RX; NereusSDR has no split).
    // StationServer takes a device's change only with transmit permission
    // and off the air.
    static constexpr const char* kPreventTxOnDifferentBandKey = "PreventTxOnDifferentBandToRx";
    static bool preventTxOnDifferentBandSetting();
    /// A local window, or the Core a device changed it on, tells the pages
    /// that show it (transmitGateSettingChanged).
    void reportTransmitGateSettingChanged(const QString& key);

    void setRxOnly(bool on);
    void applyRxOnlySetting(bool on);
    bool isRxOnly() const noexcept { return m_rxOnlyEffective; }
    bool isRxOnlyForced() const noexcept { return m_rxOnlyForced; }
    // The plain words a refused key and a disabled button show.
    QString rxOnlyReason() const;
    static QString rxOnlyForcedReason();
    // True when receive only disables the MOX button: in every mode.
    // Thetis leaves chkMOX.Enabled alone in SPEC and DRM
    // (console.cs:15318-15321); NereusSDR does not (see the definition).
    bool receiveOnlyDisablesMoxButton() const;
    // Task 16 fix wave (M6): the words for a transmit control that receive
    // only blocks, together with `otherReason` (a remote window's missing
    // transmit, say) when that blocks it too, so turning off the one named
    // never leaves the control blocked for a reason not shown. The kit's
    // reason stands alone: nothing else lets that radio transmit. Without
    // receive only, `otherReason` as it is.
    QString rxOnlyReasonAlongside(const QString& otherReason) const;
    // Task 16 fix wave (M2): the same for a transmit block MoxController
    // holds (receive only, TX inhibit, a PA trip): its reason, joined with
    // `otherReason` the same way; empty when neither applies.
    QString transmitBlockReasonAlongside(const QString& otherReason) const;

    // Sub-components
    RadioConnection*  connection()       { return m_connection; }
    const RadioConnection* connection() const { return m_connection; }
    RadioDiscovery*   discovery()        { return m_discovery; }
    ReceiverManager*  receiverManager()  { noteLocalDspHandOut("receiverManager"); return m_receiverManager; }
    AudioEngine*      audioEngine()      { noteLocalDspHandOut("audioEngine");     return m_audioEngine;     }
    WdspEngine*       wdspEngine()       { noteLocalDspHandOut("wdspEngine");      return m_wdspEngine;      }

    // R-R3-23 / R-R3-36: this computer's own sound devices. The same
    // AudioEngine audioEngine() returns, handed out WITHOUT bumping the
    // local-DSP audit below. A remote window's engine is not inert for this
    // purpose: it drives this computer's speakers for remote playback, and
    // its capture supervisor opens this computer's microphone for Test Mic,
    // and (R-R3-44) it opens this computer's VAX outputs, which the Core's
    // receiver streams feed. So a Setup page that only picks those devices
    // is working, not dead, and must not be caught by the SetupDialog gate.
    //
    // Allowed only where that is true. Check 3 of
    // scripts/verify-no-gui-dsp-access.py lists the files that may call it
    // (the audio backend strip helper in SetupDialog, the Devices and TX
    // Input pages, MainWindow's title-bar wiring, RemoteMediaController)
    // and fails on any other caller, so a new use has to come through that
    // list and say why the object it reaches is live on a remote model.
    AudioEngine*      localAudioDevices() { return m_audioEngine; }

    // ── Remote-daemon R2 Task 20: the reach-through audit ────────────────
    //
    // These three accessors are silent on a Role::Remote model in a way a
    // test cannot see by running the code. They are constructed
    // unconditionally (see this class's constructor initializer list), so
    // on a remote model they hand back a real but inert object: the
    // caller's writes land nowhere, the connects it makes never fire, and
    // nothing is observably wrong until an operator notices a control that
    // does not work. A null-dereference assertion cannot catch that shape
    // at all. Counting the hand-outs can, which is why the counter exists
    // rather than a comment asking people to be careful.
    //
    // ---- connection() is NOT the easy case this used to claim ----
    //
    // An earlier version of this block set connection() up as the
    // reassuring contrast: nullptr on a remote model, "so an unguarded
    // caller crashes and any sweep that merely executes the path finds
    // it". That is false, and it was false when it was written.
    //
    // Nothing in this tree executes the path. The one caller that mattered,
    // MainWindow::onConnectionStateChanged(), is in a class the test suite
    // cannot construct -- it boots WDSP, the audio engine and the discovery
    // thread, and three separate test banners say so
    // (tst_notch_hit_test.cpp, tst_mainwindow_status_bar_safety.cpp,
    // tst_pan_active_slice_sync.cpp). It sat there dereferencing a null
    // connection() on the first state change any remote client produced,
    // and the whole R2 acceptance run went past it.
    //
    // The reasoning that produced the false claim is worth naming, because
    // it is easy to repeat: the audit was designed around accessors that
    // fail SILENTLY, on the premise that a loud one would be caught by
    // running code. The premise needs code that runs. The single place it
    // had to hold is the single place nothing does.
    //
    // So connection() is not covered by the counter AND not covered by
    // execution. What covers it is that every caller tests it for null,
    // which used to come free -- isConnected() was `m_connection &&
    // m_connection->isConnected()` before Task 3 made it storage-backed,
    // so testing isConnected() tested the pointer too. It does not any
    // more. If you are writing a branch gated on isConnected(), the
    // pointer test is now yours to write.
    //
    // ---- KNOWN BLIND SPOT: rxChannelForSlice() is NOT counted ----
    //
    // An earlier version of this comment claimed these three were the ONLY
    // reach-through a running test cannot see. That was wrong, and the
    // exception matters because the tree is actively migrating toward it.
    //
    // rxChannelForSlice() (below, and defined in RadioModel.cpp) forwards
    // to WdspEngine::rxChannel(). On a remote model m_wdspEngine is
    // non-null but has no channels, so the forward returns nullptr, every
    // call site's `if (RxChannel* ch = ...)` guard swallows it, and the
    // control silently does nothing. It is the same silent shape as the
    // three above, arrived at through a null rather than an inert object,
    // and it is not counted here.
    //
    // It is not counted DELIBERATELY, not by oversight. Routing it through
    // noteLocalDspHandOut() was tried and rejected: two of its seven
    // src/gui call sites run at page-construction time
    // (setup/DspOptionsPage.cpp's buildUI, and MnfSetupPage's constructor
    // via refreshMinNotchWidth), so the SetupDialog gate would disable DSP
    // > Options and MNF outright. Both pages exist mainly to edit
    // Station-scoped settings that must round-trip to the daemon; each
    // reaches for a channel for one incidental local binding. Disabling
    // them would cost a remote operator two working settings pages to
    // silence one dead widget on each. Recorded rather than fixed.
    //
    // The consequence to know before adding a call site: reaching DSP
    // through rxChannelForSlice() in a Setup page produces a page that
    // stays ENABLED on a remote client and silently does nothing.
    // setup/DspOptionsPage.cpp is already in that state. If a future
    // change makes those two pages tolerable to disable, route the wrapper
    // and delete this paragraph.
    //
    // That consequence is ENFORCED, not merely written down here. Check 2
    // of scripts/verify-no-gui-dsp-access.py holds a per-file inventory of
    // every rxChannelForSlice() call under src/gui/ and fails on a call in
    // a file that is not listed, or on a count that moved in one that is.
    // It runs in CI and in the pre-commit hook, so a new call site has to
    // come through that inventory and read this block on the way past. Fix
    // round 2, Important 1.
    //
    // Armed for Role::Remote only, so this is not a general accessor tally
    // -- it means specifically "a remote model gave away something local".
    // That is also what makes it thread-safe without an atomic: a
    // Role::Remote model starts no RxDspWorker, no TxWorkerThread, no
    // AudioEngine device thread and no RadioConnection thread (Task 4's
    // connectToRadio() early return), and Task 18 keeps the whole session
    // stack on this object's own thread, so every armed call is on the
    // same thread. On Role::Local, where those threads DO exist, the
    // counter is never written at all.

    /// How many times this model handed out a live local-DSP object while
    /// in Role::Remote. Always 0 on a Role::Local model.
    int localDspHandOutCount() const { return m_localDspHandOuts; }

    /// Which accessors did it, spelled exactly as the accessor is named
    /// ("audioEngine", "receiverManager", "wdspEngine"), so a failing
    /// assertion says what to go and gate rather than only that something
    /// leaked.
    QSet<QByteArray> localDspHandOutNames() const { return m_localDspHandOutNames; }

    /// Clear the audit so a caller can attribute hand-outs to one narrow
    /// window.
    ///
    /// No production caller. SetupDialog::realizePage() deliberately uses a
    /// before/after difference instead, so that a page factory which
    /// re-enters realizePage() cannot zero the outer page's tally on its
    /// way through; its own comment says as much. This stays for tests that
    /// want a clean window and for a future caller that has no re-entrancy
    /// to worry about.
    void resetLocalDspHandOutAudit()
    {
        m_localDspHandOuts = 0;
        m_localDspHandOutNames.clear();
    }

    // Remote Daemon R2 Task 12: non-null only when role() == Role::Local.
    // Constructed and start()ed in the constructor; never rebuilt. Unlike
    // meterPoller()/containerManager() just above, there is no matching
    // setSliceMeterPump(): those are GUI objects MainWindow builds and
    // hands in non-owning after the fact, while RadioModel builds and owns
    // this one itself (Qt-parented), so the headless daemon gets it with
    // no GUI construction step at all.
    SliceMeterPump* sliceMeterPump() const { return m_sliceMeterPump; }

    /// Phase 3F Sub-Epic F Task 5: per-ADC wideband FFT engine accessor.
    /// Returns nullptr if adcIndex out of range (valid: 0 or 1).  Used by
    /// SpectrumWidget and bench rigs to inspect the wideband FFT pipeline
    /// without going through the widebandSpectrumReady signal hop.
    NereusSDR::WidebandFftEngine* widebandFftEngine(int adc) const {
        return (adc >= 0 && adc < 2) ? m_widebandFftEngines[adc] : nullptr;
    }

    /// Configured Thetis/local RF geometry, not a measured P2 sample rate.
    /// An unsupported/offline source or invalid rate has no offer.
    std::optional<double> widebandAdcRateHz(int adc) const;
    /// Owner-thread snapshots. Retired capture/configuration tokens are
    /// checked on reads as well as publication, including before queued
    /// retirement notifications have reached this model.
    std::optional<WidebandSourceDescriptor> widebandSourceDescriptor(int adc) const;
    std::optional<WidebandSpectrumFrame> latestWidebandSpectrum(int adc) const;

    /// R1 Task 11: injectable target for the wideband FFT dispatch hop
    /// wired inside wireConnectionSignals (P2RadioConnection::
    /// widebandFrameReadyForGeneration -> WidebandFftEngine::computeFft). That hop has
    /// always landed on RadioModel's own thread via an auto-connection --
    /// fine for the GUI, where RadioModel lives on the main thread and the
    /// entire point is getting the FFT off the P2 connection thread onto
    /// the thread that is idle between paint events. nereusd's DaemonApp
    /// has no such spare thread of its own: its RadioModel lives on the
    /// daemon's single Qt event-loop thread, which carries other daemon
    /// responsibilities too, so it supplies a dedicated QThread here
    /// instead (DaemonApp::widebandThread(), started in start() and
    /// quit()+wait()'d in stop()).
    ///
    /// Call BEFORE any P2 connection exists (i.e. before connectToRadio())
    /// so the very first wideband frame already targets the new thread --
    /// Qt actually re-resolves an auto-connection's direct-vs-queued
    /// dispatch on every emit based on the context object's CURRENT
    /// thread(), so in practice this is safe to call at any time, but
    /// "before connect" keeps the call order easy to reason about.
    ///
    /// `dispatchThread` is NOT owned by RadioModel -- the caller keeps it
    /// alive and must quit()+wait() it before this RadioModel is
    /// destroyed (see DaemonApp::stop()'s ordering comment). Passing
    /// nullptr is a no-op: it does NOT reset to any particular thread, it
    /// just leaves the current target alone. Never called at all (the GUI
    /// path), m_widebandDispatchContext stays on whatever thread
    /// constructed this RadioModel -- byte-for-byte the same target the
    /// old `this`-context connection used.
    void setWidebandDispatchThread(QThread* dispatchThread) {
        if (!dispatchThread) {
            return;
        }
        m_widebandDispatchContext.moveToThread(dispatchThread);
    }

    // OC matrix — single instance shared between the OC Outputs UI and the
    // codec layer (P1/P2 buildCodecContext). Loaded per-MAC at connect time.
    // Phase 3P-D Task 3.
    const OcMatrix& ocMatrix()        const { return m_ocMatrix; }
    OcMatrix&       ocMatrixMutable()       { return m_ocMatrix; }

    // HL2 I/O board model — single instance; non-null on any HL2 connection.
    // Pushed into P1RadioConnection::setIoBoard() at connect time so the
    // codec layer can dequeue I2C transactions.  Phase 3P-E Task 2.
    const IoBoardHl2& ioBoard()        const { return m_ioBoard; }
    IoBoardHl2&       ioBoardMutable()       { return m_ioBoard; }

    // HL2 Options model — 9 HL2-specific behavior knobs (mi0bot tpHL2Options).
    // Loaded per-MAC at connect time, mirrors OcMatrix ownership pattern.
    // Phase 3L commit #9.  Wire-format emission deferred to a follow-up PR.
    const Hl2OptionsModel& hl2Options()        const { return m_hl2Options; }
    Hl2OptionsModel&       hl2OptionsMutable()       { return m_hl2Options; }

    // HL2 bandwidth monitor — single instance; pushed into P1RadioConnection
    // via setBandwidthMonitor() at connect time when hasBandwidthMonitor.
    // Phase 3P-E Task 3.
    const HermesLiteBandwidthMonitor& bwMonitor()        const { return m_bwMonitor; }
    HermesLiteBandwidthMonitor&       bwMonitorMutable()       { return m_bwMonitor; }

    // Live PA telemetry and PTT state — single instance owned here.
    // Setters called by connection layer on each status packet.
    // Backed by Phase 3P-H Task 1 RadioStatus model.
    // Phase 3P-H Task 2.
    const RadioStatus& radioStatus()        const { return m_radioStatus; }
    RadioStatus&       radioStatus()              { return m_radioStatus; }

    // R-R3-32 / R-R3-46 (parity Task 6): the radio's PA readings, the one
    // source every window's PA row, Radio Status, PA Values and the HW
    // Volts, Amps and Temperature meters read. A local window takes them
    // from its own connection and RadioStatus; a remote window holds the
    // Core's (applyCorePaReadings, from station telemetry version 4). Each
    // is absent when the radio has none, has not reported it, or (in a
    // remote window) the Core's telemetry is out of date: never a 0
    // standing in for "unknown".
    struct PaReadings {
        std::optional<double> paVolts;              // PA drain volts (user ADC0)
        std::optional<double> supplyVolts;          // supply volts
        std::optional<double> paCurrentAmps;        // PA current
        std::optional<double> paTemperatureCelsius; // PA temperature
        bool operator==(const PaReadings&) const = default;
    };
    PaReadings paReadings() const;
    // True in a remote window, whose readings come from the Core.
    bool paReadingsFromCore() const { return m_role == Role::Remote; }
    // The PA row's volts, as the System tile and Radio Status show them:
    // the supply volts on the ANAN-G2E, whose user ADC0 is dark (2026-05-25
    // G2E bench finding), the PA drain volts on every other board.
    struct PaRowVolts {
        std::optional<double> volts;
        bool supply = false;   // true: supply volts ("PSU"), else PA volts ("PA")
    };
    PaRowVolts paRowVolts() const;
    // Remote window only: the Core's latest readings, or all absent when
    // its telemetry is out of date or the session ended.
    void applyCorePaReadings(const PaReadings& readings);

    // R-R3-49 (parity Task 6): the radio's TX inhibit. Local: this model's
    // TxInhibitMonitor. Remote: the Core's, as the window last heard it.
    bool isTxInhibited() const;
    // HL2 port part 2: the plain words for a TX inhibit whose reason is more
    // than the plain inhibit (the HL2 I/O board's fault code); empty
    // otherwise. Remote: the Core's, as the window last heard it.
    QString txInhibitReason() const;
    // HL2 port part 2: the transmit buttons are locked, disabled with a
    // reason, while receive only or a TX inhibit holds. Thetis's TXInhibit
    // setter disables MOX, TUN, 2TONE and VOX (console.cs:15341-15363
    // [v2.10.3.15]) as its RXOnly setter does.
    bool transmitButtonsLocked() const;
    // Whether that lock covers MOX: always for a TX inhibit; for receive
    // only, receiveOnlyDisablesMoxButton.
    bool transmitLockCoversMox() const;
    // The words a locked transmit button shows, joined with `otherReason`
    // as rxOnlyReasonAlongside does; `otherReason` as it is when unlocked.
    QString transmitLockReasonAlongside(const QString& otherReason) const;
    // TX safety (2026-09-30): whether the lock covers VOX. Receive only and
    // a TX inhibit disable it; a lost radio link does not, as Thetis's
    // power-off leaves chkVOX alone (console.cs:27488-27493 [v2.10.3.15]).
    bool transmitLockCoversVox() const;
    // TX safety (2026-09-30): the link to the radio is lost and not yet
    // back. The lock holds from LinkLost through the Disconnected wait of
    // an automatic recovery (retireConnectionForRecovery) and the rebuilt
    // link's Connecting and Probing, a rebuilt link's own Disconnected
    // included, until a link reaches Connected or the operator disconnects
    // (disconnectFromRadio). Every key is refused
    // (MoxController::setRadioLinkDown) and MOX, TUN and 2TONE are locked
    // with radioLinkDownReason(). Always false on a remote window, whose
    // Core refuses the key. A radio change (the hosted replace, the Core's
    // switchRadio) is the operator's and counts as their disconnect: it
    // restarts the window or the Core run, so a new radio starts unlocked.
    bool isRadioLinkDown() const { return m_radioLinkDown; }
    static QString radioLinkDownReason();
    // TX-parity-linkdown (fix wave): the link-down lock transmitButtonsLocked,
    // transmitLockCoversMox and transmitLockReasonAlongside use: a lost
    // link here, or, on a remote window, a Core not connected to its radio.
    bool transmitLinkDown() const;
    // Fix round 1 (minor 4): the words for that lock, by state. A lost
    // link here, or the Core's link to its radio lost or being rebuilt:
    // radioLinkDownReason(). On a remote window whose own link to the Core
    // is down: "Not connected to the Core." On a remote window whose Core
    // waits for a radio (stationRadioWaiting): "The Core has no radio
    // ready."
    QString transmitLinkDownReason() const;

    // ── Remote-window parity Task 16 (R-R3-49, R-R3-21, R-R3-40) ──────────
    // Which noise reduction runs is nrCannotRunReason's (DspAssetService,
    // the one source). A remote window on a Core below dspAssetVersion 3
    // (DFNR) or 4 (MNR) is not told, and shows them disabled with this.
    static QString noiseReductionNotSaidReason();
    // The Core's dspAssetVersion as a remote window last heard it (0 on a
    // local model, or before the Core says).
    int stationDspAssetVersion() const { return m_stationDspAssetVersion; }
    // The Core's dspInfoVersion as a remote window last heard it (0 on a
    // local model, or before the Core says).
    int stationDspInfoVersion() const { return m_stationDspInfoVersion; }
    // Remote-window parity Task 30: the Core's txDisplayVersion as a remote
    // window last heard it (0 on a local model, or before the Core says).
    // At 2 the Core applies Setup > Display > TX Display's analyzer
    // settings from this window.
    int stationTxDisplayVersion() const { return m_stationTxDisplayVersion; }
    void setStationTxDisplayVersion(int version);
    // Remote-window parity Task 33 (R-R3-49, R-R3-32): the Core's
    // txReadingsVersion as a remote window last heard it (0 on a local
    // model, or before the Core says). At 1 the Core's `txState` carries
    // forwardAdcRaw and reflectedAdcRaw and the Core keeps the
    // txCfcCompression stream.
    int stationTxReadingsVersion() const { return m_stationTxReadingsVersion; }
    void setStationTxReadingsVersion(int version);
    // A remote window's copy of the Core's `txState` (StationClient owns it
    // and sets it here), for pages that show the Core's transmit readings:
    // PA Values. Null on a local model.
    TransmitState* stationTransmitState() const;
    void setStationTransmitState(TransmitState* state);

    // Parity Task 33: the radio's raw forward and reflected power readings
    // (the ADC counts RadioConnection::paTelemetryUpdated carries), as the
    // last sample reported them, transmitting or not. The Core sends them in
    // `txState` (forwardAdcRaw, reflectedAdcRaw); PA Values shows them.
    struct PaRawAdc {
        quint16 forward = 0;
        quint16 reflected = 0;
        bool reported = false;
        bool operator==(const PaRawAdc&) const = default;
    };
    PaRawAdc paRawAdc() const { return m_paRawAdc; }
    // How long the last DSP Options apply took (dspChangeMeasured), in ms;
    // the Core's in a remote window. 0 before any.
    qint64 dspOptionsLastApplyMs() const;

    // The filter graph's high-resolution curve for a slice's receiver: the
    // magnitudes (dB, 0 at the peak) at startHz + k * stepHz, k from 0, as
    // RxChannel::filterResponseBins gives them.
    struct FilterResponse {
        double startHz = 0.0;
        double stepHz = 0.0;
        QVector<double> magnitudesDb;
    };
    // Core: the curve for `sliceId`'s receiver (dsp.filterResponse). With
    // `highResolution` false no curve is wanted and an empty one is taken.
    // False with `reason` when there is no such slice or receiver.
    bool filterResponseForStation(int sliceId, bool highResolution, FilterResponse* out,
                                  QString* reason) const;
    // The JSON text of `magnitudesDb` (rounded to 0.001 dB) and back.
    static QString filterResponseToJson(const QVector<double>& magnitudesDb);
    static std::optional<QVector<double>> filterResponseFromJson(const QString& json);
    // Remote window: the filter graph wants the Core's curve (Setup > DSP >
    // Options > High-resolution filter characteristics on). While it does,
    // the curve is fetched and fetched again when the slice's filter, mode
    // or rate changes; coreFilterResponse() holds the latest.
    void setCoreFilterResponseWanted(bool wanted);
    const FilterResponse& coreFilterResponse() const { return m_coreFilterResponse; }
    // Container minis retain a response per owning slice; a late reply for
    // one slice cannot replace another slice's curve.
    void setMiniFilterResponseSlices(const QSet<int>& sliceIds);
    FilterResponse miniFilterResponse(int sliceId) const;
    // Why a remote window cannot draw its Core's curve, or empty.
    QString coreFilterResponseUnavailableReason() const;
    // Remote window: the Core answered dsp.filterResponse `commandId`.
    void reportStationFilterResponse(quint32 commandId, bool accepted, const QString& reason,
                                     double startHz, double stepHz, const QString& json);
    // Remote window: capabilities or the link changed.
    void setStationDspInfoVersion(int version);
    void setStationDspAssetVersion(int version);
    // Remote window: the link closed with a curve request unanswered.
    void failStationFilterResponse();
    // Core: each slice's minNotchWidthHz follows its receiver's channel.
    void refreshSliceMinNotchWidths();

    // Settings hygiene validation — single instance owned here.
    // Call validate() after each successful connect.
    // Phase 3P-H Task 2.
    const SettingsHygiene& settingsHygiene()        const { return m_settingsHygiene; }
    SettingsHygiene&       settingsHygiene()              { return m_settingsHygiene; }

    // Alex antenna controller — per-band TX/RX/RX-only antenna assignment.
    // Loaded per-MAC at connect time. Backs Antenna Control sub-sub-tab UI
    // (AntennaAlexAntennaControlTab — Phase 3P-F Task 3).
    const AlexController& alexController()        const { return m_alexController; }
    AlexController&       alexControllerMutable()       { return m_alexController; }

    // Reads local hardware policy in direct mode, Core telemetry in remote mode.
    const AlexController::AlexAdcState& filterChainState(int chain) const;
    bool filterChainStateAvailable(int chain) const;
    bool applyStationFilterValue(const QByteArray& name, const QVariant& value);
    void clearStationFilterState();
    void setStationFilterSnapshotReady();
    int rxFilter0Mode() const { return static_cast<int>(filterChainState(0).mode); }
    int rxFilter0Effective() const { return static_cast<int>(filterChainState(0).effective); }
    int rxFilter0Band() const { return static_cast<int>(filterChainState(0).currentBpfBand); }
    QString rxFilter0Reason() const { return filterChainState(0).reasonText; }
    int rxFilter1Mode() const { return static_cast<int>(filterChainState(1).mode); }
    int rxFilter1Effective() const { return static_cast<int>(filterChainState(1).effective); }
    int rxFilter1Band() const { return static_cast<int>(filterChainState(1).currentBpfBand); }
    QString rxFilter1Reason() const { return filterChainState(1).reasonText; }
    // Shared-input filters, ruling (d): the low-pass reason and the slice
    // that forces it, chain 0 (the receive low-pass is on ADC0's input).
    QString rxFilter0LowPassReason() const { return filterChainState(0).lowPassReason; }
    int rxFilter0LowPassSlice() const { return filterChainState(0).lowPassSlice; }

    // ── Plan Task 14 fix wave (R-R3-49): the band outputs on the wire ──────
    //
    // Thetis's Setup shows the bits UpdateExtCtrl returned:
    //   From Thetis console.cs:29104-29107 [v2.10.3.15]
    //     if (penny_ext_ctrl_enabled) //MW0LGE_21k
    //     {
    //         int bits = Penny.getPenny().UpdateExtCtrl(lo_band, lo_bandb, _mox, _tuning, SetupForm.TestIMD, chkExternalPA.Checked); //MW0LGE_21j
    //         if (!IsSetupFormNull) SetupForm.UpdateOCLedStrip(_mox, bits);
    // Here the connection reports the byte it composed into the packet that
    // carries it (RadioConnection::bandOutputsComposed), and a remote window
    // receives the Core's through the station link (link section 7.1). The
    // HL2 I/O tab, the HL2 options tab and the OC Outputs tab show these.
    //
    // bandOutputsByte: the 7 OC bits. bandOutputsBand: a Band index, or -1
    // before anything has been composed. bandOutputsKnown: false until a
    // byte has been reported (locally) or all three values have arrived
    // from the Core (remotely); a display shows no pins until then, since a
    // Core that predates these properties never sends them.
    int  bandOutputsByte()  const { return m_bandOutputsByte; }
    int  bandOutputsBand()  const { return m_bandOutputsBand; }
    bool bandOutputsKeyed() const { return m_bandOutputsKeyed; }
    bool bandOutputsKnown() const;
    // Remote role: one of the three values from the Core. False for any
    // other name or an out-of-range value.
    bool applyStationBandOutputsValue(const QByteArray& name, const QVariant& value);
    // Remote role: the session ended; nothing is known until the next one.
    void clearStationBandOutputs();
    // Test seam: stands in for the connection's report (local role).
    void reportBandOutputsForTest(quint8 ocByte, int band, bool keyed)
    {
        onBandOutputsComposed(ocByte, band, keyed);
    }
    // Test seam: the production connection -> model report, for a test that
    // injects a connection (injectConnectionForTest does no wiring).
    void wireBandOutputsReportForTest() { connectBandOutputsReport(); }
    // Test seam: the production HL2 options -> connection push, for a test
    // that injects a connection.
    void wireHl2OptionsForTest() { connectHl2OptionsToConnection(); }

    // The Alex-1 low-pass filter bits the Core's connection selected (see
    // the Q_PROPERTY), or -1 before any.
    int alexLpfBits() const noexcept { return m_alexLpfBits; }
    // Remote role: the value from the Core. False for any other name or an
    // out-of-range value.
    bool applyStationAlexLpfValue(const QByteArray& name, const QVariant& value);
    // Remote role: the session ended; nothing is known until the next one.
    void clearStationAlexLpf();
    // Test seam: stands in for the connection's report (local role).
    void reportAlexLpfBitsForTest(quint8 bits) { onAlexLpfBitsComposed(bits); }

    // ── Phase 3F: per-panadapter RX preselector bypass state (WIDE badge) ────
    // NereusSDR-original; no upstream port. Design doc
    // 2026-05-26-phase3f-multi-pan-multi-slice-design.md §16.4.
    //
    // WIDE means one thing: the RX preselector chain feeding this pan is
    // bypassed on the wire right now. It reports an effect, not a cause;
    // the cause is named in `reason` (§16.4.3, §16.4.4).
    struct PanBypassState {
        bool    bypassed {false};
        QString reason;   ///< operator-facing sentence; empty unless bypassed
    };

    /// Resolve the WIDE state for the slices one panadapter is showing.
    ///
    /// Routing, per §16.4.2:
    ///     pan -> its slices -> their stream -> that stream's ADC -> effective
    ///
    /// The answer is per chain, not global: on a 2-chain SKU with the bypass
    /// on chain 1 only, pans on chain 0 come back clear. That is the whole
    /// point of the badge -- it tells the operator WHICH of their receivers
    /// is exposed. A pan with no slices, or whose slices have not bound a
    /// stream, feeds off nothing and reports nothing.
    ///
    /// Takes slice indices (the keys PanadapterApplet::associatedSlices
    /// hands out) rather than SliceModel pointers so callers never have to
    /// resolve the model themselves.
    PanBypassState panBypassState(const QSet<int>& sliceIndices) const;

    /// The ADC chain feeding one slice, or -1 when it is on none.
    ///
    ///     slice -> its stream -> that stream's ADC
    ///
    /// The single resolver for that hop. panBypassState calls it to decide
    /// the WIDE pill, and MainWindow calls it to paint the CH tag sitting
    /// beside that pill, so the two cannot report different chains for one
    /// pan. Do not reach for SliceModel::chainIndex() instead: nothing in
    /// production writes it, so it answers 0 for every slice.
    ///
    /// Takes a slice ID (see sliceById), not a list position -- the same key
    /// PanadapterApplet::associatedSlices holds. Returns -1 for an unknown id
    /// and for a slice that has not bound a stream; an unbound slice feeds
    /// off nothing, which is not the same as being on chain 0.
    int sliceChainIndex(int sliceId) const;

    /// The filter chain feeding one DDC stream, or -1 when the stream index
    /// is not a stream. sliceChainIndex is the by-slice front end of this;
    /// republishAlexAdcSlices and bypassReasonForAdc take the stream form
    /// because they are already iterating streams.
    ///
    /// CHAIN, not ADC, and the distinction is load-bearing (defect D4). A
    /// chain is one preselector bank plus the ADC behind it (design §16.1.1),
    /// and a board can have more ADCs than chains: ANAN-100D and ANAN-200D
    /// are both NetworkIO.SetRxADC(2) yet neither appears in the setAlex2HPF
    /// model list at Thetis console.cs:15435-15443 [v2.10.3.15], so both
    /// their ADCs sit behind one filter bank. On such a board a stream really
    /// can be routed to ADC1 and is still behind chain 0, so it is folded
    /// onto chain 0 here. That is the physical truth on a one-chain board,
    /// not a workaround: with one bank in front of both ADCs, every slice's
    /// range has to be counted against that one bank.
    int chainForStream(int stream) const;

    /// The PHYSICAL ADC behind a stream, with none of chainForStream's fold.
    ///
    /// Use these two and not the chain pair whenever the question is about the
    /// ADC itself rather than the preselector in front of it. The wideband
    /// display is the case that forced the split: widebandSpectrumReady
    /// carries the physical index off the wire, so keying the paint off a
    /// chain index made an extended pan on ADC1 render ADC0's survey on any
    /// board where the fold applies (ANAN-100D, ANAN-200D). Codex, PR #318.
    ///
    /// Same -1 conventions as the chain pair: not a stream, unknown slice, or
    /// a slice that has not bound a stream.
    int adcForStream(int stream) const;
    int sliceAdcIndex(int sliceId) const;

    /// Effective demand is the OR of local slice requests and active endpoint
    /// owners. Physical ADC capture and filter-chain bypass are independent.
    bool widebandActiveForChain(int chainIdx) const;
    bool widebandActiveForChainForTest(int chainIdx) const {
        return widebandActiveForChain(chainIdx);
    }
    void reconcileWidebandDemand();

    /// Owner-thread, Core-only leases for remote display endpoints. Acquisition
    /// is inactive and binds the actual SliceModel, not a reusable numeric ID.
    /// Zero is invalid. Deactivate when wings are hidden; release when the
    /// endpoint retires. Slice removal and radio loss invalidate its token.
    using WidebandDemandToken = quint64;
    WidebandDemandToken acquireWidebandDemand(int sliceId);
    bool setWidebandDemandActive(WidebandDemandToken token, bool active);
    void releaseWidebandDemand(WidebandDemandToken token);

    /// Operator-facing sentence naming WHY the given chain is bypassed.
    /// One string per cause, per design doc §16.4.4. Public so the Filter
    /// Policy dialog can show the same wording the badge tooltip carries.
    QString bypassReasonForAdc(int adc,
                               const AlexController::AlexAdcState& st) const;

    /// Shared-input filters, ruling (d): the sentence saying which slice
    /// sets the receive low-pass on the input (`top`) and which counted
    /// slices it holds to it (`held`). AlexAdcState::lowPassReason. On the
    /// HL2, `highPassOff` names the slices the N2ADR broadcast-band
    /// high-pass is off for; `top` may then be null when none is held.
    QString lowPassHoldReason(const SliceModel* top,
                              const QList<const SliceModel*>& held,
                              const QList<const SliceModel*>& highPassOff = {}) const;

    // Band-plan overlay manager — loaded once on construction from bundled
    // Qt resource JSON files. Active plan persists in AppSettings under
    // "BandPlanName". Phase 3G RX Epic sub-epic D.
    const BandPlanManager& bandPlanManager()        const { return m_bandPlanManager; }
    BandPlanManager&       bandPlanManagerMutable()       { return m_bandPlanManager; }

    // Apollo PA + ATU + LPF accessory controller — present/filter/tuner enable flags.
    // Loaded per-MAC at connect time. Setup UI deferred (Phase 3P-F Task 5a).
    const ApolloController& apolloController()        const { return m_apolloController; }
    ApolloController&       apolloControllerMutable()       { return m_apolloController; }

    // PennyLane / Penelope external-control master toggle.
    // Loaded per-MAC at connect time. OC bitmask logic lives in OcMatrix (Phase 3P-D).
    // Setup UI deferred (Phase 3P-F Task 5b).
    const PennyLaneController& pennyLaneController()        const { return m_pennyLaneController; }
    PennyLaneController&       pennyLaneControllerMutable()       { return m_pennyLaneController; }

    // Calibration controller — HPSDR NCO correction factor, level offsets, LNA
    // offsets, TX display cal, PA current sens/offset. Loaded per-MAC at connect.
    // Backs CalibrationTab UI and P2RadioConnection::hzToPhaseWord(). Phase 3P-G.
    const CalibrationController& calibrationController()        const { return m_calController; }
    CalibrationController&       calibrationControllerMutable()       { return m_calController; }

    // Phase 3M-0 Task 17: safety controller accessors.
    // SwrProtectionController and TxInhibitMonitor are QObject-owned by RadioModel.
    // BandPlanGuard is a plain value class (no Qt parent).
    safety::SwrProtectionController& swrProt() noexcept { return m_swrProt; }
    const safety::SwrProtectionController& swrProt() const noexcept { return m_swrProt; }
    safety::TxInhibitMonitor& txInhibit() noexcept { return m_txInhibit; }
    const safety::TxInhibitMonitor& txInhibit() const noexcept { return m_txInhibit; }
    safety::BandPlanGuard& bandPlan() noexcept { return m_bandPlan; }
    const safety::BandPlanGuard& bandPlan() const noexcept { return m_bandPlan; }

    // Sub-models
    MeterModel&       meterModel()       { return m_meterModel; }
    // Inert model-owned CAT service; policy-ready lifecycle callers start it.
    CatService* catService() const { return m_catService; }
    TransmitModel&    transmitModel()    { return m_transmitModel; }

    // Slice management (client-side — radio has no slice concept)
    QList<SliceModel*> slices() const { return m_slices; }

    /// The slice whose SliceModel::sliceIndex() equals `sliceId`, or nullptr.
    ///
    /// Slice ids are stable for the life of a slice and are NOT list
    /// positions: addSlice hands out the lowest free id and removeSlice does
    /// not renumber the survivors, so the two diverge after any mid-list
    /// removal. The id doubles as the slice's WDSP RX channel id.
    /// For positional access, index slices() directly.
    SliceModel* sliceById(int sliceId) const;

    /// The slice diversity runs for, by its stable id (slice A), or nullptr
    /// while that slice is closed. The one owner RadioModel itself uses
    /// (diversityActive, reconcileExternalDiversityRoute), published so a
    /// diversity surface edits the same slice instead of picking its own.
    SliceModel* diversityTargetSlice() const;

    /// R-R3-40: the DSP load of the slice with this ID over the latest
    /// ReceiverDspLoadSampler::kSampleIntervalMs interval, or nullopt when
    /// there is no such slice or no WDSP channel for it (always nullopt on a
    /// remote model), and for a new slice until its second sample (the
    /// first only seeds the baseline). Returns the snapshot the periodic
    /// sampler cached;
    /// reading changes nothing, so any number of readers see the same
    /// values. Main thread only.
    std::optional<ReceiverDspLoad> receiverDspLoad(int sliceId) const;

    /// R-R3-40: take one load sample of every slice now (the sampler timer
    /// calls this every kSampleIntervalMs on a local model). Main thread.
    void sampleReceiverDspLoad();

    /// R-R3-40: one check of every receiver running NNR (the sampler timer
    /// calls this right after sampleReceiverDspLoad, local role only). A
    /// receiver that cannot keep up steps back one level (NnrLoadGovernor):
    /// the Core asks its WDSP channel for the limit without the DSP lock and
    /// sets SliceModel::nnrLimit. The saved NNR choice is never changed.
    /// nowMs is a monotonic clock. Main thread.
    void governNnrLoad(qint64 nowMs);

    /// Test seam for governNnrLoad: the same check with the given loads in
    /// place of receiverDspLoad (a missing slice, or nullopt, is not
    /// measured).
    void governNnrLoadForTest(qint64 nowMs, const QHash<int, std::optional<double>>& loads);

    SliceModel* activeSlice() const { return m_activeSlice; }

    /// Hand the transmitter to the slice with this ID, RF-safely.
    ///
    /// Takes a slice ID (see sliceById), not a list position, and passes it
    /// straight on: TxSliceArbiter::requestHandoff matches it against
    /// SliceModel::sliceIndex(), the stable id every per-slice UI surface
    /// carries (PanadapterApplet::activeSliceIndex() is resolved through
    /// sliceById by the status-overlay refresh). So a mid-list removal that
    /// makes ids and positions diverge still picks the right slice: with
    /// A(0) B(1) C(2), removing B leaves C at id 2 / position 1, and a
    /// handoff to 2 reaches C.
    ///
    /// Returns false without moving anything when the id resolves to no
    /// slice, or when there is no arbiter. Delegates the MOX drop to
    /// TxSliceArbiter::requestHandoff rather than reproducing it: that
    /// sequence is the whole reason the arbiter owns this.
    ///
    /// Factored out of the MainWindow badge handler so both the pan TX badge
    /// and a test can reach it without standing up a MainWindow, the same way
    /// requestSliceSampleRate is.
    /// Slice control fix wave (Important 4): an accepted request emits
    /// txSliceSelected, the Core's own window's explicit transmit choice.
    bool requestTxHandoffToSlice(int sliceId);

    /// Phase 3F: hardware-capped user-facing slice count. Reads BoardCapabilities.maxSlices
    /// for the currently connected SKU. Returns 1 when disconnected (safe default).
    /// See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §2.
    int maxSlices() const;

    // ── Phase 3F Sub-Epic I: DDC stream pool ────────────────────────────
    //
    // A stream is one hardware DDC plus its ReceiverManager receiver, its
    // FFTEngine, its panadapter window, and its noise blanker. Streams are
    // opened once at connect and reused, mirroring Thetis CreateRadio
    // (cmaster.cs:516 [v2.10.3.15]) and deskhpsdr's create-all loop
    // (radio.c:1259 [@f3d857c]). Neither upstream opens a WDSP channel at
    // runtime.
    //
    // Slices bind many-to-one: several whose frequencies fall inside one
    // window share it, differing only by shift offset. An idle stream costs
    // memory; its DDC stays out of the ddcEnable bitmask so the radio never
    // streams it.

    /// Size the pool to the connected SKU and clear all bindings.
    void configureStreamPool(int userDdcCount, int maxSlices, int defaultRateHz);

    /// Open the WDSP RX channel pool: one channel per slice the SKU allows.
    ///
    /// `poolSize` is BoardCapabilities::maxSlices; values below 1 are treated
    /// as 1, and anything above WdspEngine::kMaxSliceChannels is CLAMPED, not
    /// honoured. The clamp is the collision guard: WDSP's channel table is one
    /// global array (channel.c:29) and OpenChannel overwrites whatever sits at
    /// the id without closing it, so a pool that ran past kMaxSliceChannels
    /// would silently take over kTxChannelId and orphan the TXA's thread. A SKU
    /// that wants a bigger pool has to raise the reserved block, which moves TX
    /// and PS with it.
    ///
    /// Already-open channels are left alone, so this is idempotent and safe to
    /// re-run on reconnect.
    ///
    /// Called from the WdspEngine::initializedChanged lambda in
    /// connectToRadio(), after Slice A's channel has its state.
    void openRxChannelPool(int poolSize, int inputBufferSize,
                           int inputSampleRateHz);

    /// Switch on the WDSP channel of every slice that currently holds a stream.
    ///
    /// A pooled channel is opened stopped (WDSP `initial state = 0`, Thetis
    /// ChannelMaster/cmaster.c:80 [v2.10.3.15]) and RxChannel::processIq
    /// memsets its output to silence until setActive(true). Called at the tail
    /// of openRxChannelPool so a reconnect, which re-binds every slice before
    /// WDSP has any channels, still comes back with all of them audible.
    void activateBoundSliceChannels();

    /// Reconcile every OPEN pool channel with the current notch set.
    ///
    /// Design section 6.3. Runs at the tail of openRxChannelPool because
    /// activateSliceChannel is dead as a hook for Slice A: connectToRadio's
    /// WDSP-init lambda activates channel 0 before it opens the pool, and
    /// activateSliceChannel early-returns on an already-active channel.
    /// Covers reconnect for free, since teardownConnection destroys every
    /// channel.
    ///
    /// Reconciles channels that no slice is bound to yet as well: a notch
    /// database is created inert (third_party/wdsp/src/RXA.c:87) and this is
    /// where the run flag lands.
    void syncNotchesToAllChannels();

    /// Switch on one slice's WDSP channel, pushing its demodulation state
    /// first. No-op when the slice has no stream, has no channel yet, or is
    /// already live (Slice A, which connectToRadio activates after the full
    /// state push).
    ///
    /// Mirrors Thetis's receiver-enable order: push the DSPRX state, then
    /// SetChannelState(ch, 1, 0) (console.cs:37359-37361 [v2.10.3.15]).
    void activateSliceChannel(SliceModel* slice);

    /// Stop a slice's WDSP channel running. The channel object stays open for
    /// whichever slice takes the id next. Thetis's disable half:
    /// SetChannelState(ch, 0, 0) (console.cs:37398-37400 [v2.10.3.15]).
    void deactivateSliceChannel(int sliceId);

    /// Run the allocator for every slice that currently has no stream.
    ///
    /// Connect-time step: Slice A is created before the pool is sized, so its
    /// addSlice-time bind was a no-op, and after a teardown every slice is
    /// unbound (see releaseStreamBindings).
    void bindUnboundSlices();

    /// Push every stream's current slice set to the DSP worker.
    ///
    /// Phase 3F Sub-Epic I closeout, defect F1. connectToRadio sizes the pool
    /// and binds every slice BEFORE wireConnectionSignals constructs
    /// m_dspWorker, so each of those binds published into a null pointer and
    /// the freshly-built worker started life knowing only its constructor's
    /// seed ({stream 0: [slice 0]}). Anything on a non-zero stream then
    /// demodulated nothing until the operator happened to retune it. Called
    /// from wireConnectionSignals once the worker exists.
    void republishAllStreamBindings();

    /// Drop every slice's stream binding and idle the whole pool.
    ///
    /// Phase 3F Sub-Epic I closeout, defect F1. Teardown destroys the DSP
    /// worker but the slices kept their streamIndex, so the reconnect bind
    /// loop in connectToRadio (guarded on streamIndex() < 0) skipped them all
    /// and nothing was ever republished to the new worker. Deactivating the
    /// allocator's streams here keeps its bookkeeping consistent with the
    /// slices that just became unbound, rather than leaving live streams that
    /// no slice claims.
    void releaseStreamBindings();

    int streamPoolSize() const;
    int activeStreamCount() const;

    /// SliceModel::sliceIndex() of every slice currently bound to a stream,
    /// ascending. These double as WDSP channel ids (Sub-Epic I invariant:
    /// WDSP RX channel id == slice index).
    QVector<int> slicesOnStream(int streamIndex) const;

    /// Recompute every slice's shift oscillator against a new stream centre.
    ///
    /// A shared DDC window has one centre and N slices sitting at their own
    /// offsets inside it. When the centre moves (a CTUN drag, a band jump),
    /// each member's shift is (frequency - newCentreHz). Missing a co-host
    /// leaves it demodulating the wrong signal while its flag still reads
    /// the right number.
    ///
    /// Deliberately does NOT move the allocator's own centre: this is called
    /// from the CTUN drag, which retunes the DDC through
    /// ReceiverManager::forceHardwareFrequency precisely because it is
    /// bypassing the allocator's placement policy. Callers that DO own the
    /// placement (bindSliceToStream) already write the shift themselves.
    void reshiftSlicesOnStream(int streamIndex, double newCentreHz);

    /// Commit one slice's offset from the centre the local pan says its
    /// stream sits on, to the model and to WDSP together.
    ///
    /// The local tune path in MainWindow (the pan following a band jump, and
    /// a CTUN tune inside the pan) used to write RxChannel::setShiftFrequency
    /// straight from the widget's centre, leaving SliceModel::shiftOffsetHz
    /// on whatever the allocator had last placed. The demodulator and the
    /// model then described different centres, and anything reading the
    /// model (TCI's dds and if, the notch origin) reported the wrong one.
    /// This writes shiftOffsetHz = frequency - streamCentreHz, then pushes the
    /// composed shift and the notch origin from the same centre through
    /// pushNotchOrigin, so the two halves cannot disagree.
    ///
    /// From Thetis radio.cs:1419 [v2.10.3.15]: SetRXAShiftFreq receives
    /// +(freq - center).
    void applySliceStreamCentre(SliceModel* slice, double streamCentreHz);

    /// Phase 3F Sub-Epic I Task 7b: hardware DDC currently routed to
    /// `streamIndex`, or -1 when that stream is idle (or no codec has run).
    /// This is the codec's choice, republished; every slice on the stream
    /// reports the same number through SliceModel::ddcIndex().
    int ddcForStream(int streamIndex) const;

    /// Phase 3F Sub-Epic I Task 10: change a DDC stream's sample rate.
    ///
    /// The rate IS the window width, so widening lets more slices share this
    /// stream and narrowing can push slices out of it. Every slice bound to
    /// the stream is therefore re-run through the allocator afterwards: an
    /// evicted slice migrates to a free DDC rather than being left aliased
    /// on a window that no longer contains it.
    ///
    /// Protocol 1 carries ONE rate for the whole radio in C&C bank 0
    /// (P1RadioConnection::composeCcBank0 takes a single sampleRate and
    /// encodes it as srBits), so on a P1 connection this applies the rate to
    /// every active stream. Protocol 2 carries a per-DDC rate in
    /// DdcAssignment::rate[], which the codecs populate per stream, so there
    /// it applies only to the stream named.
    bool setStreamSampleRate(int streamIndex, int rateHz);
    /// Fix wave after the several-devices group review (a rate proceed
    /// closes only after the change succeeds): the same change with the
    /// slices in `closing` set aside in the plan. `close` runs for each of
    /// them only once the change is certain (the plan holds and, on
    /// Protocol 1, the radio took the new rate), before the plan commits.
    /// Refused, it closes nothing. Local only.
    bool setStreamSampleRateClosing(int streamIndex, int rateHz, const QSet<int>& closing,
                                    const std::function<void(int)>& close);

    /// Phase 3F Sub-Epic I closeout, defect G2: the operator picked a sample
    /// rate on one slice's VFO flag.
    ///
    /// Takes a slice ID (see sliceById), not a list position, because that is
    /// what VfoWidget carries. Resolves the slice to its DDC stream and hands
    /// off to setStreamSampleRate, so the request necessarily resolves to a
    /// stream-wide rate: co-hosted slices share one DDC and cannot hold
    /// different widths. Unknown or unbound slices are ignored.
    ///
    /// This is the body of MainWindow's sampleRateRequested handler, factored
    /// out so both flag-wiring sites share it and so it is reachable from a
    /// test without a MainWindow.
    ///
    /// Remote-daemon R2: on a Role::Remote model this SENDS the
    /// requestSliceSampleRate verb and returns, applying nothing locally.
    /// Deliberately no local pre-check on the id: the daemon owns the
    /// slice list and its refusal reason is the one the operator needs,
    /// relayed onto sliceRetuneRejected by
    /// reportStationRetuneRejected().
    void requestSliceSampleRate(int sliceId, int rateHz);

    /// Parity ruling C4: Setup > Hardware > Radio Info's sample rate, from
    /// any window. A local model changes the whole radio at once
    /// (setSampleRateLiveAsync: every receiver and the radio's own rate,
    /// which new receivers take). A remote model sends the Core the same
    /// change (verb setRadioSampleRate, radioHardwareVersion 9); on an older
    /// Core it sends each of this window's receivers' own rate request
    /// (requestSliceSampleRate), lowest id first, as before. A refusal
    /// comes back as sliceRetuneRejected(-1, reason).
    void requestRadioSampleRate(int rateHz);
    /// Parity ruling C4: true when requestRadioSampleRate reaches every
    /// receiver and the radio's own rate: always on a local model, and on a
    /// remote one whose Core offers setRadioSampleRate.
    bool radioSampleRateReachesEveryReceiver() const;
    /// Parity ruling C4, the Core's half of setRadioSampleRate: the local
    /// window's change (setSampleRateLiveAsync), with `onFinished(ok)`
    /// called once when it has finished (at once, ok, for the rate the
    /// radio is at; ok false with no connection or WDSP not ready).
    void changeRadioSampleRate(int rateHz, std::function<void(bool)> onFinished);
    /// requestSliceSampleRate on the Core with `closing` closed through
    /// `close` only once the change is certain (setStreamSampleRateClosing).
    void requestSliceSampleRateClosing(int sliceId, int rateHz, const QSet<int>& closing,
                                       const std::function<void(int)>& close);

    /// Set the station-owned C-Tune pin for the bound stream named by a
    /// slice. Remote roles send the typed station command and never mutate
    /// their mirror optimistically. Returns whether the local operation, or
    /// remote send, was accepted.
    bool requestStreamCtunPinned(int sliceId, bool pinned);

    /// Move the bound stream's DDC centre explicitly. All cohosts must remain
    /// inside the target window or the request is refused without changes.
    bool requestStreamCentre(int sliceId, double centreHz);

    /// iPhone app Task 74 (rulings 6.4, 6.5, 6.7): moves a receiver's
    /// window on a confirmed pan move and places again every slice it no
    /// longer covers (but `exemptSliceId`); returns those that found no
    /// receiver, for the caller to close. Local only.
    QList<int> moveStreamWindowFor(int stream, double centreHz, int exemptSliceId);
    /// iPhone app Task 74 (ruling 6.6): takes these slices to `stream`,
    /// claimed at `centreHz` when free. Local only.
    bool moveSlicesToStream(const QList<int>& sliceIds, int stream, double centreHz);
    /// iPhone app Task 74: the placement policy's state, read to plan a pan
    /// move or a take on a copy before anything changes.
    const NereusSDR::SliceStreamAllocator& streamAllocator() const { return m_streamAllocator; }
    /// iPhone app Task 74 (ruling 6.9): the slice cap every device shares.
    int sliceCapForDevices() const { return sliceChannelLimit(); }

    /// iPhone app Task 75 (the several-devices design, ruling 7.3): what a
    /// sample-rate change on `sliceId`'s receiver would do, simulated with
    /// today's plan (planStreamSampleRateChange) and changing nothing. A
    /// slice the plan would refuse and `mayClose` allows (another device's)
    /// is set aside as closing and the plan run again without it; any other
    /// refused slice refuses the whole change, as today (`refused`, with
    /// `refusedSliceId`). Local only.
    struct SampleRateReach {
        bool refused = true;
        int refusedSliceId = -1;
        /// The receiver the change is on, and whether it is the radio's
        /// (Protocol 1).
        int stream = -1;
        bool radioWide = false;
        int fromRateHz = 0;
        QList<int> changes;
        QList<int> moves;
        QList<int> closes;
    };
    SampleRateReach planSampleRateReach(int sliceId, int rateHz,
                                        const std::function<bool(int)>& mayClose) const;

    /// End the C-Tune pins of the receivers `device` anchors and project the
    /// cleared value to every cohost. Fix wave after the several-devices
    /// group review (ruling 4.8 keeps a device's pans): StationServer calls
    /// this when a device leaves for good (session.leave, a token window's
    /// end, the end of its 180 s, revocation), not when a session drops, so
    /// a device coming back keeps its pins.
    void clearStreamCtunPinsAnchoredBy(const QByteArray& device);

    /// Push a slice's just-restored per-band sample rate onto its DDC.
    ///
    /// Codex review round 7, PR #293. SliceModel::restoreFromSettings reads
    /// the persisted per-band SampleRate and calls setSampleRateHz, which is
    /// a plain property setter: it moves the number the VFO menu displays
    /// and nothing else. The rate the receiver, codec and wire actually run
    /// at only changes through requestSliceSampleRate. So returning to a
    /// band you had left at 384 kHz showed 384 kHz while the DDC stayed on
    /// whatever the previous band was using, and the saved preference was in
    /// effect never restored.
    ///
    /// Called after restoreFromSettings rather than inside it: the restore
    /// sets the frequency first, which completes the allocator rebind, and
    /// the rate transaction has to run against the stream the slice ended up
    /// on. SliceModel also holds no RadioModel handle by design, and the
    /// rate is a stream-wide transaction rather than a slice property.
    void applyRestoredSampleRate(SliceModel* slice);

    /// Slice control plan Task 5: on a Remote model, true (and announced)
    /// when slice `sliceId` is one this window only listens to, so a
    /// request for it is held back instead of sent.
    bool holdSliceRequestForListener(int sliceId);

    /// R-R3-49: each slice's saved per-band sample rate for the band it is
    /// on (SliceModel::savedSampleRateHz), keyed by slice id; slices with
    /// none saved are left out. connectToRadio reads it before it binds the
    /// slices, because a bind makes a slice adopt its stream's rate and a
    /// later save would then write that over the saved one.
    QHash<int, int> savedSliceSampleRates() const;

    /// R-R3-49: Protocol 2 only. Puts each bound slice's saved rate from
    /// `saved` on its DDC through requestSliceSampleRate, the same path a
    /// window's rate change takes (allocator, WDSP channel rate, DDC
    /// assignment). A rate the board does not allow for `proto` is logged and
    /// skipped, leaving the slice at the connect rate, as resolveSampleRate
    /// does for the radio-wide key. Protocol 1 carries one rate for the whole
    /// radio, so it keeps the radio-wide key and this does nothing.
    /// connectToRadio calls it once WDSP is up and the connection and codec
    /// exist.
    void applySavedSliceSampleRates(const QHash<int, int>& saved,
                                    NereusSDR::ProtocolVersion proto);

    /// True when one sample rate covers the whole radio rather than one DDC.
    ///
    /// Protocol 1 encodes the rate as srBits in C&C bank 0
    /// (P1RadioConnection::composeCcBank0 takes a single sampleRate), so every
    /// stream shares it and setStreamSampleRate fans a change across all of
    /// them. Protocol 2 carries a per-DDC rate in DdcAssignment::rate[]. UI
    /// that offers the rate from a per-slice surface has to disclose the P1
    /// scope rather than imply a private rate. False when disconnected.
    bool sampleRateIsRadioWide() const;

    /// Sample rates the connected radio accepts, ascending; empty when
    /// disconnected.
    ///
    /// Thin wrapper over SampleRateCatalog::allowedSampleRates with the live
    /// protocol, board capabilities and SKU. Rate pickers must filter through
    /// this: P1 saturates srBits at 3 for anything >= 384 kHz, so offering a
    /// P2-only rate on a P1 board would leave the client configured for a
    /// width the radio is not sending.
    QVector<int> allowedStreamSampleRates() const;

    // Phase 3F bench fix 2026-06-03: optional initialPanId is stamped on the
    // new SliceModel as a dynamic property BEFORE sliceAdded() emits, so the
    // MainWindow handler can route the VfoWidget to the owning pan. Passing
    // an empty string preserves the legacy single-pan behaviour.
    /// Returns the new slice's id — the lowest not currently in use, which
    /// is also its WDSP RX channel id and its A-E display letter. Returns
    /// -1 if the allocator refused to place it, in which case nothing is
    /// added: see the rollback in the definition. Also returns -1, adding
    /// nothing, when the slices already fill the ceiling the stream pool
    /// was sized with; sliceAddRejected then carries the same cap reason
    /// addSliceOnPan() gives (sliceCapReason).
    ///
    /// Remote-daemon R2: on a Role::Remote model this SENDS the addSlice
    /// verb and always returns -1, because the id is the STATION's to
    /// mint and is not knowable here yet. The slice appears later, under
    /// the station's id, through addSliceWithStationId(). A caller that
    /// needs the id must wait for sliceAdded rather than read the return.
    ///
    /// Whether the slice gets its own receiver window or shares an existing
    /// one is DERIVED from `initialPanId`, not passed in: a pan with no
    /// other slices is new and needs its own, a pan that already has slices
    /// is a host and sharing it is the point. It was briefly a caller-
    /// supplied flag; every caller that forgot it silently reintroduced the
    /// coupled-pan defect, so the decision lives with the data it depends on.
    int addSlice(const QString& initialPanId = QString());

    /// Takes a slice ID (see sliceById), not a list position. sliceRemoved
    /// carries the same id.
    ///
    /// Remote-daemon R2: on a Role::Remote model this SENDS the
    /// removeSlice verb and removes nothing locally; the slice goes away
    /// when the station's object.destroy arrives at
    /// removeSliceWithStationId(). The session's own inbound path must
    /// call that one, never this.
    void removeSlice(int sliceId);

    /// Slice control plan Task 7 (Local role): closes the slice only when
    /// SliceOwnership reports it unclaimed (no controller, nobody
    /// listening, not held for anyone), whatever the slice count, the
    /// Core's last slice included, and saves its settings as a removal
    /// does. Returns whether it closed. Zero slices is a valid idle Core.
    bool closeUnclaimedSlice(int sliceId);

    /// Slice control plan Task 7: whether a slice is bound for transmit.
    /// With none (a Core with no slice) every key is refused.
    bool hasTransmitSlice() const;

    /// NOTE: still a LIST POSITION, unlike sliceById / removeSlice above.
    /// This API remains positional for internal list navigation only.
    ///
    /// Prefer setActiveSliceById below for anything driven by a UI surface:
    /// every per-slice widget carries the stable id, not the position.
    void setActiveSlice(int index);

    /// Make the slice with this ID the active one.
    ///
    /// UI surfaces that select a slice all carry the stable slice ID
    /// (VfoWidget::sliceActivationRequested emits what createSliceFlag
    /// stamped from SliceModel::sliceIndex(); RxApplet::updateSliceButtons
    /// keys its button group the same way), while setActiveSlice above
    /// indexes m_slices positionally. Ids and positions diverge after any
    /// mid-list removal, because removeSlice does not renumber survivors.
    /// With A(0) B(1) C(2), closing B leaves C at id 2 / position 1: the
    /// unconverted call asked for position 2 of a two-element list and
    /// selected nothing at all, so clicking flag C did nothing.
    ///
    /// Returns false, changing nothing, when the id resolves to no slice.
    /// Leaving the previous active slice in place matters: every
    /// active-slice surface (container S-meter, RX applet, DSP menu) would
    /// otherwise be stranded on nullptr by one stale click.
    ///
    /// Remote-daemon R2: on a Role::Remote model this SENDS the
    /// setActiveSliceById verb and flips nothing locally. The return then
    /// means "the request left this client", not "the slice is now
    /// active": the daemon applies it and its answer arrives back as an
    /// ordinary mirrored `active` delta. False still means nothing
    /// changed, for the two reasons a remote client can know on its own:
    /// the id is not in its mirrored slice list, or there is no station
    /// to ask.
    bool setActiveSliceById(int sliceId);

    // ── iPhone app Task 73 (R-IOS-02): several devices on one Core ───────
    //
    // Whose each slice is (the several-devices design, sections 5.1 to 5.7).
    // On a Local model every slice has an owner mark here; a slice's
    // `active` means its owner's active slice, and activeSlice() is the
    // station-level active slice (ruling 5.11): the transmit holder's while
    // transmit is held, otherwise the most recent choice by any owner. With
    // no owners (a desktop window on its own) every slice has the same
    // owner, none, and this is exactly the one active slice of before.
    SliceOwnership* sliceOwnership() const { return m_sliceOwnership; }

    /// `owner` makes one of its own slices its active slice (ruling 5.10).
    /// False, changing nothing, when the slice is not `owner`'s. Local only.
    bool setActiveSliceByIdFor(const QByteArray& owner, int sliceId);

    /// Slice control plan Task 3: `device` makes one of the slices it has
    /// joined (controlled or listened) its active receive slice
    /// (SliceOwnership::activeRxFor). When it controls the slice this is
    /// also its active slice, as setActiveSliceByIdFor; a listened slice
    /// never moves a slice's `active` or the station-level active slice.
    /// False, changing nothing, when `device` has not joined it. Local only.
    bool setActiveRxFor(const QByteArray& device, int sliceId);

    /// The device holding transmit, empty for none (Task 34 calls this).
    /// While one holds it, its active slice is the station-level one
    /// (ruling 5.11). Local only.
    void setTransmitHolder(const QByteArray& holder);

    // ── Parity Task 31 (A11, R-R3-49): display duplex (DUP) ─────────────
    //
    // Thetis turns noise blanking off while keyed with DUP on and restores
    // it at the unkey (console.cs:29179 and :29213 [v2.10.3.15],
    // UIMOXChangedTrue / UIMOXChangedFalse). Here the DUP that counts is
    // the transmit holder's when a device holds transmit (a remote window,
    // told to the Core by its media subscriptions), otherwise this
    // window's own. Local only: a remote window's model follows the Core.
    /// This window's DUP (the window's DisplayDuplex setting as it applies).
    void setLocalDisplayDuplex(bool on);
    bool localDisplayDuplex() const { return m_localDisplayDuplex; }
    /// A remote device's DUP (DaemonMediaController, from its subscriptions).
    void setDeviceDisplayDuplex(const QByteArray& deviceId, bool on);
    /// The DUP the next key and unkey read: the holder's, else this window's.
    bool transmitDisplayDuplex() const;

    /// iPhone app plan Task 35 (R-IOS-13): who is keyed. While MOX is on it
    /// names the transmit holder (a device's key, a program's key through
    /// its window, and VOX all name the device that holds transmit; the
    /// Core's own keys name the station device, kind "station"), how it
    /// keyed and the key's epoch. Empty deviceId while unkeyed. The Core
    /// sets it (StationServer, through RemoteKeying); a desktop on its own
    /// leaves it empty.
    struct KeyedBy {
        QByteArray deviceId;
        QString deviceName;
        QString deviceKind;
        /// A device's trigger ("screen", "headset", "bluetooth",
        /// "actionButton", "tci"), "tune" or "twoTone" for those keys,
        /// "vox", or for the station device's own keys "radioPtt", "cat"
        /// or "station".
        QByteArray trigger;
        /// The keying epoch: advances with every key, so each key has its
        /// own.
        quint32 epoch{0};

        bool isEmpty() const { return deviceId.isEmpty(); }
        bool operator==(const KeyedBy& other) const = default;
    };
    KeyedBy keyedBy() const { return m_keyedBy; }
    /// Local only. Also shows PttSource::Remote in RadioStatus while a
    /// device other than the station is keyed.
    void setKeyedBy(const KeyedBy& keyedBy);
    /// The next keying epoch (each call advances it; never 0).
    quint32 advanceKeyingEpoch();
    quint32 keyingEpoch() const { return m_keyingEpoch; }

    // ── iPhone app plan Task 36 (R-IOS-13): the remote microphone ──────
    //
    // The station's microphone source follows a remote device while it
    // transmits: the transmit pump takes the device's microphone line
    // (remoteMicFeed()) instead of the operator's configured source while
    //   - the device is keyed (keyedBy() names it and MOX is on: its own
    //     key, its program's key, or a VOX key attributed to it),
    //   - it has VOX armed (setRemoteMicVoxArmed: VOX on and its session
    //     permitted to transmit), or
    //   - its key is waiting for the buffer to fill (setRemoteMicPriming).
    // Otherwise the operator's source applies, and every change empties
    // the ring, so at unkey nothing of the device's audio is left. Local
    // only; the Core's media controller names the device and drives the
    // priming and VOX inputs.

    //
    // Fix wave C2: several devices may carry a line at once (one media
    // controller each). Exactly one line writes the ring at a time, the
    // writer (remoteMicWriter()): the keyed device's while MOX is on and it
    // has a line; otherwise the device whose key is waiting for its buffer;
    // otherwise the VOX device. A change of writer empties the ring, so one
    // device's audio is never mixed into another's transmission.

    /// The ring the transmit pump pulls (null on a remote window's model).
    void setRemoteMicSelection(const QString& owner, const QByteArray& device, RemoteMicSource source);
    RemoteMicSource remoteMicSelection(const QString& owner, const QByteArray& device) const;
    void forgetRemoteMicSession(const QString& owner);
    void beginRemoteRadioKeyAttempt(const QString& owner, const QByteArray& device, quint32 commandId);
    void finishRemoteRadioKeyAttempt(const QString& owner, const QByteArray& device,
                                    quint32 commandId, quint32 acceptedEpoch);
    bool remoteRadioMicKeyActive(const QByteArray& device) const;
    RemoteMicFeed* remoteMicFeed() const { return m_remoteMicFeed.get(); }
    /// `deviceId`'s media carries a microphone line now (opened once per
    /// media connection that carries one; closed as often).
    void openRemoteMicLine(const QByteArray& deviceId);
    void closeRemoteMicLine(const QByteArray& deviceId);
    bool remoteMicLineOpen(const QByteArray& deviceId) const;
    /// `deviceId`'s key is waiting for its line's buffer to fill.
    void setRemoteMicPriming(const QByteArray& deviceId, bool priming);
    /// `deviceId` has VOX armed (VOX on, its session may transmit, and VOX
    /// was not armed by another device).
    void setRemoteMicVoxArmed(const QByteArray& deviceId, bool armed);
    /// The pump takes a remote device's microphone now.
    bool remoteMicInUse() const { return m_remoteMicInUse; }
    /// The one line that writes the ring now, or empty (none, or a key
    /// whose line was lost mid-key: silence).
    QByteArray remoteMicWriter() const { return m_remoteMicWriter; }
    /// The device whose microphone VOX listens to now (its VOX key is its
    /// own), or empty: the transmit holder when it has VOX armed and a
    /// line, else the one device with VOX armed and a line.
    QByteArray remoteVoxDevice() const;

    /// The lowest slice id not in use (the next letter a new slice takes),
    /// or -1 when every id with a channel is in use.
    int lowestFreeSliceId() const;

    /// Ruling 5.2 step 2: a device's saved slice made again for `owner`, on
    /// `sliceId` (which must be free), at `state`'s frequency and mode, its
    /// pan key `state.panKey`, after loading whatever settings that id's
    /// keys hold. It joins a receiver window that covers it or claims a
    /// free receiver, as any new slice does. Returns the id, or -1 with
    /// `reason` (the slice cap, or no receiver) when it does not fit.
    /// Local only.
    int restoreSliceFor(const QByteArray& owner, int sliceId, const ReceiveSliceState& state,
                        QString* reason = nullptr);

    /// Phase 3F Sub-Epic C Task 7: AetherSDR-faithful slice creation entry
    /// point.  Creates a new SliceModel (delegates to addSlice) and tags it
    /// with the supplied pan id as a dynamic property for Sub-Epic D wiring.
    /// Enforces the slice cap addSlice() does (sliceChannelLimit()) and
    /// emits sliceAddRejected with sliceCapReason() on overflow.
    /// Pattern from AetherSDR MainWindow.cpp:6849-6859 [@0cd4559a]
    /// (+RX button handler).
    ///
    /// Remote-daemon R2: on a Role::Remote model this SENDS the
    /// addSliceOnPan verb and creates nothing locally. The slice cap
    /// check is deliberately skipped on that path: the station
    /// owns the slice list and enforces its own cap, with its own reason,
    /// which comes back through reportStationSliceCommandRejected().
    Q_INVOKABLE void addSliceOnPan(const QString& panId);

    /// Re-point any slice whose pan is not in `livePanIds` at the first one
    /// that is. Returns how many moved.
    ///
    /// Codex review, PR #293. Shrinking the pan layout deleted the omitted
    /// PanadapterApplets, but the slice-side loop in MainWindow only ever
    /// added (`for (i = existing; i < target; ++i)`), so with existing >
    /// target its body never ran. Slices whose panKey named a deleted pane
    /// were left pointing at nothing: their VFO widgets went with the pane,
    /// re-expanding did not re-associate them because nothing emitted
    /// panKeyChanged, and they went on holding a DDC, a stream and audio.
    ///
    /// Rehomes rather than removes. A slice carries the operator's frequency,
    /// mode, filter and DSP state, and throwing that away as a side effect of
    /// picking a smaller layout is destructive and was never asked for.
    ///
    /// An empty `livePanIds` is a no-op: there is nowhere to move to, and
    /// leaving a slice on a stale pan id beats pointing it at an empty string
    /// that no pane will ever match.
    ///
    /// Lives here rather than in the MainWindow lambda that has the defect,
    /// because MainWindow is not constructible in the test harness and logic
    /// put there cannot be tested at all.
    /// Slice control and listening, layout change rule: which slices one
    /// window may place, and where it shows the ones it only listens to.
    ///
    /// A slice's pan key is shared by every device (it mirrors both ways),
    /// so a window that rehomes or spreads a slice it does not control moves
    /// that slice for the device that does. A scoped call moves only
    /// `controlled` slices and counts a pan as occupied when a controlled
    /// slice has its key or a listened slice is placed there (`listenedOn`,
    /// slice id to this window's pan id). Every other slice is invisible to
    /// the scoped calls. A null scope keeps the unscoped behavior, which is
    /// what a window with no other devices has always had.
    struct PanScope {
        QSet<int> controlled;
        QHash<int, QString> listenedOn;
    };

    int rehomeSlicesToPans(const QStringList& livePanIds,
                           const PanScope* scope = nullptr);

    /// The listened slices in `scope` whose placement is not one of
    /// `panIds`, in slice order. A layout change that returns any of these
    /// has retired the pan showing them. Listening ends only when the
    /// operator ends it, so the caller places each one on a pan that
    /// remains. Nothing about the slice itself changes here.
    QList<int> listenedOffPans(const QStringList& panIds, const PanScope& scope) const;

    /// Which of `panIds` currently host no slice, in the order given.
    ///
    /// Codex review round 4, PR #293, and a regression rehomeSlicesToPans
    /// created. The layout handler decided which pans to populate with
    /// `for (i = slices().size(); i < target; ++i)`, which assumes the slice
    /// COUNT is the first unoccupied pan index. Rehoming breaks that: shrink
    /// a 2x2 to one pane and all four slices land on pan-0, so expanding back
    /// has existing == target == 4, the loop adds nothing, and three panes
    /// come up with no VFO and no RX entry. Co-hosting slices by hand, or
    /// removing a non-final slice id, breaks the same assumption.
    ///
    /// Occupancy is the question the caller is actually asking, so it is the
    /// question answered here. Co-hosted slices count once: a pan with three
    /// slices on it is occupied, not three-times occupied.
    QStringList pansWithoutSlices(const QStringList& panIds,
                                  const PanScope* scope = nullptr) const;

    /// Slices currently living on `panId`, optionally skipping one.
    ///
    /// `except` exists for the add path: addSlice appends the new slice to
    /// m_slices before binding it, so asking "does this pan already have
    /// slices" would otherwise always answer yes and count the newcomer
    /// itself. That distinction decides whether the slice opens a new
    /// receiver or joins an existing one, so getting it wrong is the whole
    /// difference between two independent pans and two views of one.
    QVector<SliceModel*> slicesOnPan(const QString& panId,
                                     const SliceModel* except = nullptr) const;

    /// Fix wave I4 (several-devices ruling 5.12): a pan is a device plus a
    /// pan key. Whether `panId` already holds a slice of `owner`'s (the
    /// SliceOwnership owner), skipping `except`. An empty `owner` counts
    /// every slice on the key, as before several devices. Decides whether a
    /// new slice there opens a new pan, on the Core as in addSliceImpl.
    bool panHasSlicesFor(const QString& panId, const QByteArray& owner,
                         const SliceModel* except = nullptr) const;

    /// Move surplus co-hosted slices onto pans in `panIds` that have none.
    /// Returns how many moved.
    ///
    /// Codex review round 5, PR #293, and a regression pansWithoutSlices
    /// created. After a 2x2 shrinks to one pane every slice sits on pan-0;
    /// expanding again finds three empty pans, and creating a slice for each
    /// spends the maxSlices budget on NEW slices while four co-hosted ones sit
    /// idle. On a five-slice radio that fills pan-1, hits the cap, and leaves
    /// pan-2 and pan-3 empty with a surplus fifth slice in the model.
    ///
    /// The slices needed are already there, so they are moved before any are
    /// made. Only genuinely surplus ones move: a pan holding a single slice is
    /// never raided, or expanding would just relocate the hole.
    int spreadSlicesOntoEmptyPans(const QStringList& panIds,
                                  const PanScope* scope = nullptr);

    /// Phase 3F closeout — public helper for invoking the antennaAutoSwitched
    /// signal from operator surfaces (Tools menu "Test antenna switch toast"
    /// entry) and from future conflict-detection logic in AlexController.
    /// Plain helper avoids the Qt private-signal access dance for callers.
    Q_INVOKABLE void emitAntennaAutoSwitched(int sliceIndex,
                                              const QString& oldAntenna,
                                              const QString& newAntenna);

    /// Phase 3F closeout — Sub-Epic E Task 7 consumer wire surface. Emits
    /// txBoundReRouteRequested(proposedAntenna, existingAntenna). Today this
    /// is invoked only from the Tools menu test entry that exercises the
    /// TxBoundConfirmDialog surface; real emission from addSliceOnPan when
    /// the slice-add would force a TX-bound chain re-route lands when the
    /// conflict-detection state machine ships.
    Q_INVOKABLE void requestTxBoundReRoute(const QString& proposedAntenna,
                                            const QString& existingAntenna);

    // Band-button click handler. Routes both SpectrumOverlayPanel::bandSelected
    // and ContainerWidget::bandClicked through one code path. On first
    // visit to `band`, applies BandDefaults::seedFor(band) and persists;
    // on subsequent visits, restores last-used per-band state via the
    // 3G-10 Stage 2 persistence already on SliceModel.
    //
    // Same-band click is a no-op. XVTR with no seed and no persisted
    // state is a logged no-op. Locked slices freeze frequency (mode still
    // changes, matching Thetis lock semantics).
    //
    // Acts on activeSlice(). No-op if active slice is null.
    //
    // Issue #118.
    void onBandButtonClicked(NereusSDR::Band band);

    // R-R3-21: the same, on `slice` (a container's own slice, which need
    // not be the active one). No-op if `slice` is null.
    //
    // Parity Task 18 (B3.1): in a remote window this sends slice.selectBand
    // for `slice` to a Core at bandSelectVersion 1 (a grid band), and the
    // Core runs this same function on its own slice.
    void onBandButtonClicked(SliceModel* slice, NereusSDR::Band band);

    // Panadapter management (client-side)
    QList<PanadapterModel*> panadapters() const { return m_panadapters; }
    int addPanadapter();
    void removePanadapter(int index);

    // View hooks: non-owning pointers to the primary spectrum widget and
    // FFT engine so setup pages (Phase 3G-8+) can call renderer/FFT
    // setters without depending on MainWindow. Wired by MainWindow after
    // constructing each view. Not owned, not lifetime-tracked — MainWindow
    // outlives both.
    class SpectrumWidget* spectrumWidget() const { return m_spectrumWidget; }
    void setSpectrumWidget(class SpectrumWidget* w) {
        if (m_spectrumWidget == w) { return; }
        m_spectrumWidget = w;
        emit spectrumWidgetChanged(w);
    }
signals:
    // 3D Stacked-Trace Spectrum Plan Task 22: lets a surface that follows
    // the active panadapter (DisplayApplet) rebind when MainWindow
    // repoints this view hook, instead of only ever reading it once at
    // construction.
    void spectrumWidgetChanged(class SpectrumWidget* w);
public:

    // R1 Task 4: the abstract counterpart of m_spectrumWidget above.
    // RadioModel's own DSP-facing calls (SwrProtectionController overlay
    // push, applyClaritySmoothDefaults) go through this pointer instead of
    // the concrete widget, so RadioModel.cpp needs no "gui/" include.
    // m_spectrumWidget stays alongside it, untouched, because 82 Setup
    // page call sites (DisplaySetupPages, AppearanceSetupPages,
    // SpectrumPeaksPage; see MainWindow.cpp's activePanChanged handler
    // comment) reach the renderer through spectrumWidget() for far more
    // than the ISpectrumSink surface covers. Both members are set from the
    // same SpectrumWidget* at every call site in MainWindow.cpp, so they
    // always name the same object; only the static type callers see
    // differs.
    NereusSDR::ISpectrumSink* spectrumSink() const { return m_spectrumSink; }
    void setSpectrumSink(NereusSDR::ISpectrumSink* sink) { m_spectrumSink = sink; }
    class FFTEngine* fftEngine() const { return m_fftEngine; }
    void setFftEngine(class FFTEngine* e) { m_fftEngine = e; }
    // Parity Task 17 follow-up (R-R3-01): every pan's engine, so Setup >
    // Display > Rendering > Decimation applies to every pan, not only
    // stream 0's. Non-owning; MainWindow sets it beside setFftEngine and
    // clears it before the pool goes.
    class FftEnginePool* fftEnginePool() const { return m_fftEnginePool; }
    void setFftEnginePool(class FftEnginePool* pool) { m_fftEnginePool = pool; }
    // Phase 3M-5d: Setup → Display → TX page reaches the TX analyzer the
    // same way it reaches the FFT engine.  Non-owning pointer wired by
    // MainWindow at construction.
    class TxAnalyzer* txAnalyzer() const { return m_txAnalyzer; }
    // Task 28 (R-R3-49, A11): also makes the transmit display's feed over
    // it (TxDisplayFeed), the one owner of its view and its start and stop
    // on the MOX edge; null clears both.
    void setTxAnalyzer(class TxAnalyzer* a);
    /// Task 28: the transmit display's feed, owned here; null without a TX
    /// analyzer.
    TxDisplayFeed* txDisplayFeed() const { return m_txDisplayFeed.get(); }
    class ClarityController* clarityController() const { return m_clarityController; }
    void setClarityController(class ClarityController* c) { m_clarityController = c; }
    class StepAttenuatorController* stepAttController() const { return m_stepAttController; }
    // Phase 4 Agent 4A of issue #167 — also propagates to TransmitModel
    // so the ATT-on-TX-on-power-change safety gate inside
    // setPowerUsingTargetDbm can call ctrl->setAttOnTxValue(31) when the
    // gate fires (Thetis console.cs:46740-46748 [v2.10.3.13] [2.10.3.5]MW0LGE).
    // Implementation in RadioModel.cpp.
    void setStepAttController(class StepAttenuatorController* c);
    /// R-R3-46 / R-R3-11: the step attenuator follows the receive band of
    /// slice A (slice 0, Thetis rx1_band), with a local radio and on the
    /// Core (DaemonApp): on every band change of that slice its band's
    /// attenuation and preamp are restored and sent to the radio
    /// (setBandRestoreToRadio), as Thetis's RX1Band setter does
    /// (console.cs:17325 [v2.10.3.15]). The ATT-on-TX value and the CW
    /// check on MOX follow the transmit-bound slice's band and mode (Thetis
    /// _tx_band and the TX DSP mode). Wires once; a no-op on a Remote model
    /// (the Core restores and sends) and without a controller.
    void followReceiveSliceWithStepAttenuator();
    /// R-R3-46: hand the controller slice A's band and the transmit slice's
    /// band and mode now. Called before loadSettings on connect, which
    /// restores the per-band slot for the controller's current band.
    void syncStepAttenuatorToReceiveSlice();
    /// R-R3-46 / R-R3-11: hand the controller which ADC slice A is on, the
    /// other ADC in use and the band of the slice controlling it (the
    /// lowest-numbered slice there), and whether diversity links the two.
    void syncStepAttenuatorAdcRouting();
    /// R-R3-46: the step attenuator and preamp as the mirrored `stepAtt`
    /// object. A Local model binds it to its controller (the Core's, or a
    /// local window's, where nothing reads it); a Remote model leaves it
    /// unbound and it holds the Core's values.
    StepAttenuatorFacade* stepAttFacade() const { return m_stepAttFacade; }
    /// R-R3-46 (radioHardwareVersion 2): the Alex antenna settings as the
    /// mirrored `alexAntennas` object. A Local model binds it to its own
    /// AlexController; a Remote model leaves it unbound and it holds the
    /// Core's values (and the window's Hardware Config availability).
    AlexAntennaFacade* alexAntennaFacade() const { return m_alexAntennaFacade; }
    /// R-R3-46 (radioHardwareVersion 3): the HL2 I/O board's detected state,
    /// hardware version and registers as the read-only mirrored `ioBoard`
    /// object. A Local model binds it to its own IoBoardHl2; a Remote model
    /// writes the Core's values into its own IoBoardHl2, which Setup's HL2
    /// I/O board tab shows.
    IoBoardHl2Facade* ioBoardFacade() const { return m_ioBoardFacade; }

    /// R-R3-46: a remote window's copy of the Core's OC pin matrix is
    /// reloaded from the Core's settings when one of its keys arrives
    /// (`key` is the settings key), coalesced, so the window never saves a
    /// stale cell back over a newer Core value. A no-op on a Local model.
    void scheduleRemoteOcReload(const QString& key);
    // R-R3-46 / R-R3-49 (parity Task 6): in a remote window, a key of the
    // Core's radio's PA profiles (hardware/<mac>/pa/...) or PA forward-power
    // table (hardware/<mac>/paCalibration/...) reloads the window's copies,
    // coalesced; an empty key reloads them whatever changed.
    void scheduleRemotePaReload(const QString& key);

    /// R-R3-46: ask the radio's HL2 I/O board to identify itself (three
    /// I2C reads). Locally the P1 connection enqueues them; a remote window
    /// asks the Core. `sent` is false, with a plain reason, when nothing
    /// was asked.
    struct IoBoardProbeOutcome {
        bool sent = false;
        QString reason;
    };
    IoBoardProbeOutcome requestIoBoardProbe();

    /// Remote-window parity Task 14 (R-R3-46, radioHardwareVersion 7): HL2
    /// Options' I2C Control tool. One read or write on the radio's I2C bus
    /// (bus 1, the HL2's daughterboard bus; address 0 to 0x7F; register and
    /// write value 0 to 255). Locally the transaction joins the I/O board's
    /// queue, which the HL2's P1 connection sends one per C&C frame; a
    /// remote window asks the Core (requestIoBoardI2c), which does the
    /// same. `done` is called exactly once, perhaps before this returns:
    /// a read with `ok` true and the four bytes the radio returned in
    /// `value` (C1 << 24 | C2 << 16 | C3 << 8 | C4, so the register's own
    /// byte is value & 0xFF), or false with ioBoardNoAnswerReason() once
    /// kIoBoardI2cAnswerMs pass after the read went out unanswered; a write
    /// with `ok` true once it is queued. A write is refused while the radio
    /// is on the air (onAirReason()), a read is not.
    struct IoBoardI2cRequest {
        int bus = 1;
        int address = 0;
        int reg = 0;
        bool write = false;
        int value = 0;
    };
    using IoBoardI2cDone =
        std::function<void(bool ok, qint64 value, const QString& reason)>;
    void requestIoBoardI2c(const IoBoardI2cRequest& request, IoBoardI2cDone done);
    /// Parity Task 14: Pin Control on HL2 Options. Sets output `pin` (0 to
    /// 7) of the I/O board on or off (the output register, 169), then reads
    /// the register back into `outputs`, as mi0bot's strip click does.
    /// Needs the board detected; refused on the air. A remote window asks
    /// the Core (setIoBoardOutput). `done(ok, 0, reason)` is called once.
    void setIoBoardOutput(int pin, bool on, IoBoardI2cDone done);
    /// Parity Task 14: read the I/O board's output register back, as
    /// mi0bot does when HL2 Options is entered. Needs the board detected;
    /// nothing is reported (the answer lands in the register mirror and
    /// `ioBoard`'s outputs).
    void refreshIoBoardOutputs();
    /// The words for an I2C read the radio did not answer in time.
    static QString ioBoardNoAnswerReason();
    /// mi0bot gives an I2C read 20 one-millisecond polls after its first
    /// to be answered (setup.cs btnI2CRead_MouseDown [@c26a8a4]).
    static constexpr int kIoBoardI2cAnswerMs = 21;
    /// Remote window: the Core answered requestIoBoardI2c or
    /// setIoBoardOutput `commandId` (`value` the read's bytes, when sent).
    void reportStationIoBoardResult(quint32 commandId, bool accepted, const QString& reason,
                                    std::optional<qint64> value);
    /// Remote window: the link closed; every request still waiting on the
    /// Core is answered with `reason`.
    void failStationIoBoardRequests(const QString& reason);

    /// R-R3-32 (parity Task 14): the Hermes Lite 2 link, the one source the
    /// HL2 I/O tab's bandwidth monitor, Radio Status's Connection Quality
    /// card and Diagnostics > Connection Quality read. A local window reads
    /// its own bandwidth monitor (bwMonitor()); a remote window holds the
    /// Core's (applyCoreHl2LinkFigures, from station telemetry version 5),
    /// each absent when the Core's radio has no monitor or the Core's
    /// telemetry is out of date. The throttle event count is this window's
    /// own monitor's: the Core does not send it.
    struct Hl2LinkFigures {
        std::optional<double> rxBytesPerSecond;
        std::optional<double> txBytesPerSecond;
        std::optional<bool> throttled;
        std::optional<qint64> sequenceGaps;
        std::optional<int> throttleEvents;
        bool operator==(const Hl2LinkFigures&) const = default;
    };
    Hl2LinkFigures hl2LinkFigures() const;
    /// True in a remote window, whose HL2 link figures come from the Core.
    bool hl2LinkFiguresFromCore() const { return m_role == Role::Remote; }
    /// Remote window only: the Core's latest figures, or all absent.
    void applyCoreHl2LinkFigures(const Hl2LinkFigures& figures);
    NoiseFloorTracker* noiseFloorTracker() const { return m_noiseFloorTracker; }
    void setNoiseFloorTracker(NoiseFloorTracker* t) { m_noiseFloorTracker = t; }

    /// Register the tracker measuring one DDC stream's band.
    ///
    /// Auto AGC-T derives its threshold from the noise floor, so a slice has
    /// to measure the band it is on. With one shared tracker (stream 0's), a
    /// 20m slice would take its threshold from 40m's noise floor.
    void setStreamNoiseFloorTracker(int streamIndex, NoiseFloorTracker* t) {
        if (t) { m_streamNoiseFloors.insert(streamIndex, t); }
    }
    void clearStreamNoiseFloorTracker(int streamIndex, NoiseFloorTracker* expected) {
        if (m_streamNoiseFloors.value(streamIndex, nullptr) == expected) {
            m_streamNoiseFloors.remove(streamIndex);
        }
    }

    /// The tracker for this slice's stream, falling back to the global one
    /// when the slice is unbound or its stream has no tracker yet.
    NoiseFloorTracker* noiseFloorTrackerForSlice(const SliceModel* s) const {
        if (s) {
            const int stream = s->streamIndex();
            if (stream >= 0) {
                if (NoiseFloorTracker* t = m_streamNoiseFloors.value(stream, nullptr)) {
                    return t;
                }
            }
        }
        return m_noiseFloorTracker;
    }
    // Task 3.1: MeterPoller view hook so MultimeterPage can apply live
    // polling-interval and averaging-window changes without a MainWindow
    // round-trip.  Non-owning; MainWindow calls setMeterPoller() after
    // creating MeterPoller (see MainWindow.cpp construction block).
    class MeterPoller* meterPoller() const { return m_meterPoller; }
    void setMeterPoller(class MeterPoller* p) { m_meterPoller = p; }
    // Task 3.2: ContainerManager view hook so MultimeterPage can broadcast
    // unit-mode changes to all live MeterItems via forEachMeterItem().
    // Non-owning; MainWindow calls setContainerManager() after creating
    // ContainerManager (same pattern as setMeterPoller above).
    class ContainerManager* containerManager() const { return m_containerManager; }
    void setContainerManager(class ContainerManager* cm) { m_containerManager = cm; }
    QTimer* autoAgcTimer() const { return m_autoAgcTimer; }

    // 3M-1a G.1: expose MoxController so MainWindow can wire
    // StepAttenuatorController::onMoxHardwareFlipped (F.2 connect) after
    // both objects exist.  Non-owning; lifetime is RadioModel's lifetime.
    // Master design §5.1.1; pre-code review §1.6.
    MoxController* moxController() const { return m_moxController; }

    // R-R3-49: the radio is keyed or still handing back to receive
    // (MoxController::isMox(), or its state is not Rx). A remote window
    // holds the Core's value as it last heard it.
    bool isTransmitting() const;

    // RADE end-of-over callsigns: the radio is sending FreeDV's end-of-over
    // frame after an operator's release (MoxController's end-of-over tail).
    // The Core's value; TransmitState sends it as txEnding.
    bool endOfOverTailActive() const;
    // A release that has not yet dropped the hardware (MoxController::
    // isReleasing): Stop All TX and the time-out still act during it.
    bool transmitReleaseInProgress() const;
    // Whether an unkey now may send the RADE end-of-over tail: the Core's
    // own release (not after one of its stops, the RF gate still open), not
    // TUNE or two-tone, and the TX-bound slice in RADE. The tail also needs
    // the slice's running RADE channel and the TX worker.
    bool radeEndOfOverTailPermitted() const;

    // R-R3-49 (parity Task 1): the Core's one on-the-air refusal. True,
    // with "The radio is on the air. Try again when it stops." in `reason`,
    // while the radio is keyed (MoxController from any source, through its
    // TX to RX handover; or the transmit model's MOX latch), TUNE is on, or
    // the two-tone test runs. False otherwise. Every change a window asks
    // the Core for that keys nothing is checked here before it is applied.
    bool stationOnAirRefusal(QString* reason) const;
    // The sentence stationOnAirRefusal gives, for a window's own gate.
    static QString onAirReason();
    // R-R3-49 / R-IOS-27 (JJ's ruling, follow Thetis): the PA Gain page's
    // on-the-air lock (Thetis OnMoxChangeHandler, setup.cs:23826-23834
    // [v2.10.3.15]). While the radio is on the air only the transmitting
    // band's gain, drive-step adjust, max power and use-max change, and
    // only from the device that holds transmit.
    static QString paOnAirLockedReason();   // "Can't change while transmitting."
    static QString paHolderOnlyReason();    // "Only the device that is transmitting ..."
    // The DSP > Options RX buffer sizes' on-the-air lock (Thetis greys
    // grpDSPBufferSize while MOX is on, setup.cs:5159 [v2.10.3.15]); the
    // same words as PA Gain's lock.
    static QString dspBufferOnAirLockedReason(); // "Can't change while transmitting."
    // True for the four DSP > Options RX buffer size keys
    // (DspOptionsBufferSize{Phone,Fm,Cw,Dig}Rx).
    static bool isRxDspBufferSizeKey(const QString& key);
    // The PA band the radio transmits on (Thetis _adjustingBand): the
    // transmit slice's band, else the last band, when it is 160 m..6 m or
    // XVTR; -1 when that band has no PA values. A remote window whose Core
    // sends paTransmitBand takes the Core's row.
    int paOnAirBandIndex() const;
    // The paTransmitBand property: paOnAirBandIndex().
    int paTransmitBand() const;
    // True while the PA Gain page's on-the-air lock holds: MOX (the
    // controller's or the transmit model's), TUNE or the two-tone test.
    // Unlike stationOnAirRefusal it ends when MOX drops, not after the
    // controller's TX to RX handover: Thetis gates these edits on
    // console.MOX alone (setup.cs:24210-24222 [v2.10.3.15]).
    bool paOnAirNow() const;
    // Why an on-the-air PA edit is refused, or empty when it is taken (and
    // empty off the air). `profileAction`: select, new, copy, delete or
    // reset. `band`: the PA row the edit changes. `requesterHoldsTransmit`:
    // the device asking holds transmit.
    QString paOnAirEditRefusal(bool profileAction, int band, bool requesterHoldsTransmit) const;
    // The same rule for a window's raw PA profile keys
    // (hardware/<mac>/pa/profile/...): on the air only a change to the
    // active profile's transmitting band, from the device that holds
    // transmit, is taken; the list, the active name, another profile, a
    // remove (`value` null) and any other band are refused. Empty off the
    // air and for every other key.
    QString paSettingOnAirRefusal(const QString& key, const QString* value,
                                  bool requesterHoldsTransmit) const;
    // After such a change was stored: the Core's bank takes it at once and
    // the drive follows it, as applyPaEditOnAir does for the verbs. Off the
    // air (and for any other key) it does nothing: the PA reload does it.
    void applyPaSettingOnAir(const QString& key, const QString& value);
    // iPhone app plan Task 77 fix round 3: the Power Genius's OPERATE and
    // STANDBY also wait while a Tuner Genius cycle runs (the Core's own
    // cycle, from its standby wait to its restore, or the tuner reporting
    // a sweep; a remote window sees the Core's tuner). True, with the
    // words, when refused: on the air (onAirReason()) first, then tuning
    // (tunerTuningReason()).
    // Task 77 fix round 4: and while the amplifier is still switching from
    // an earlier command (ampStillSwitchingReason()), except a standby
    // (`standbyRequested`) while operate=1 is unconfirmed: the way out of
    // an amplifier that took operate=1 and never reports operating.
    bool pgxlSwitchRefusal(QString* reason, bool standbyRequested = false) const;
    // "The amplifier is still switching. Try again in a moment."
    static QString ampStillSwitchingReason();
    // Task 77 fix round 4: operate=1 was sent and its state is not reported
    // yet (a window's OPERATE button then sends standby).
    bool ampOperateUnconfirmed() const;
    // True while a Tuner Genius cycle runs (see pgxlSwitchRefusal).
    bool pgxlSwitchWaitsForTuner() const;
    // The words for an amplifier switch refused while the tuner tunes.
    static QString tunerTuningReason();
    // Parity mini-round (the operator's ruling c, 2026-09-25): a local
    // window's own amp or tuner switch (`device` "pgxl", "tgxl" or
    // "rfkit"), checked by stationOnAirRefusal. Its buttons are greyed by
    // isCoreOnAir(), which can already read false while this still refuses
    // (the hand-back to receive after MOX, the transmit model's MOX latch).
    // True when refused: the click then goes out on accessoryRequestRefused
    // with the reason a remote window gets from its Core, which MainWindow
    // shows the same way, never a silent drop. False otherwise, and always
    // false in a remote window, which asks its Core instead.
    // Task 77 fix round 4: `standbyRequested` for a Power Genius STANDBY
    // (see pgxlSwitchRefusal).
    bool refuseLocalAccessorySwitchOnAir(const QString& device, bool standbyRequested = false);

    // R-R3-49 (parity Task 7): PureSignal's operational permission. True
    // on a station, a receive-only Core included (a window arms PureSignal
    // there off the air; arming keys nothing, and the correction runs only
    // while the radio transmits). False in a remote window, which asks its
    // Core instead.
    bool pureSignalOperationPermitted() const;

    // R-R3-49 (parity Task 1): in a remote window, the Core's radio is on
    // the air as the Core last reported it: its `transmitting`, the
    // mirrored transmit model's TUNE, or PureSignal's two-tone. Nothing
    // here keys; a window greys what waits while this is true.
    bool isCoreOnAir() const;
    // R-R3-49: the window's copy of the Core's `transmitting` goes back to
    // false when the session ends, so a Core that does not send it never
    // inherits an old "on the air". Its TX inhibit and paTransmitBand go
    // with it.
    void clearRemoteTransmittingState();

    // Phase 3F Sub-Epic C: TX-slice arbiter (single-TX invariant + RF-safe
    // handoff). Owned by RadioModel (Qt parent), wired to slice list +
    // MoxController during construction. MAC injected + load() driven on
    // every currentRadioChanged emit; save() runs from teardownConnection.
    // Used by the upcoming VfoWidget TX-badge click handoff path and any
    // future code that needs the authoritative TX-bound slice index.
    TxSliceArbiter* txSliceArbiter() const { return m_txSliceArbiter; }
    /// iPhone app plan Task 34 (R-IOS-03): the unkey-confirmed gate (never
    /// null). The arbiter's handoff while keyed and a transfer of transmit
    /// (TransmitHolder) unkey through it.
    UnkeyGate* unkeyGate() const { return m_unkeyGate; }

    // The slice bound to the transmitter — the source of every transmit
    // frequency. NOT activeSlice(), which is only the slice the operator is
    // looking at; in multi-slice those diverge, and taking the transmit
    // frequency from the wrong one puts the PA on the wrong band (and, via
    // the Alex low-pass, behind the wrong filter).
    //
    // Thetis draws the same distinction: its VFO A arm is guarded by
    // `!chkVFOBTX.Checked` so it stands down when VFO B is transmitting
    // (console.cs:31889-31893 [v2.10.3.15]), and the VFO B handler assigns
    // tx_dds_freq_mhz itself in that case (console.cs:32866-32869).
    //
    // Returns nullptr when no arbiter binding resolves. TX-global callers
    // must fail safely rather than substituting listening/UI state.
    SliceModel* txBoundSlice() const;

    /// The RF carrier a slice actually transmits on: its dial frequency
    /// plus XIT when XIT is enabled. Returns 0 for a null slice and clamps
    /// at 0 rather than wrapping.
    ///
    /// Public because the TX display has to centre on the same number the
    /// transmitter uses. Centring on slice->frequency() instead drew the
    /// trace, waterfall and TX filter overlay around the RX VFO while the
    /// radio transmitted at the shifted carrier (Codex, PR #317). Anything
    /// that needs "where is this slice transmitting" should come here
    /// rather than re-derive the offset.
    quint64 txFrequencyForSlice(const SliceModel* slice) const;

    // Phase 3F Sub-Epic D Task 13: NereusSDR-original FFT fan-out router.
    // Wires receiverId -> N pans so a single DDC FFT pipeline can feed
    // multiple zoom levels of the same I/Q data. MainWindow registers
    // pan-to-receiver mappings on sliceAdded; the per-receiver FFTEngine
    // fan-out pump is wired in Sub-Epic E / F polish (the routing table
    // is correct as soon as the mappings are populated).
    class FFTRouter* fftRouter() const { return m_fftRouter; }

    // 3M-1c Phase L.1: expose MicProfileManager so MainWindow / SetupDialog
    // can hand the per-MAC profile bank to TxApplet (J.1 setter) and
    // TxProfileSetupPage (J.3 ctor).  Non-owning; lifetime is RadioModel's
    // lifetime.  See header §3M-1c L.1 for the construction + connect flow.
    MicProfileManager* micProfileManager() const { return m_micProfileMgr; }

    // Phase 4 Agent 4A of issue #167: expose PaProfileManager so the future
    // PaGainByBandPage (Phase 6 Agent 6A) and tests can hand the per-MAC
    // profile bank around.  Non-owning; lifetime is RadioModel's lifetime.
    // Constructed once in the RadioModel ctor; setMacAddress + load() are
    // called per-connect inside connectToRadio() (mirrors MicProfileManager
    // wiring at lines ~1191).  Active profile is passed by reference to
    // TransmitModel::setPowerUsingTargetDbm at every callsite.
    PaProfileManager* paProfileManager() const { return m_paProfileManager; }

    // 3M-1c Phase L.2: expose TwoToneController so MainWindow can hand it to
    // TxApplet (J.2 setter) for the 2-TONE button + status mirror.
    // Non-owning; lifetime is RadioModel's lifetime.
    TwoToneController* twoToneController() const { return m_twoToneController; }

    // 3M-4 Task 7: expose PureSignal coordinator so PsForm, PureSignalApplet,
    // TxApplet [PS-A], and PsaIndicatorWidget can subscribe to its
    // Q_PROPERTY signals (cal lifecycle, MOX integration, FB level updates).
    // Non-owning view; RadioModel owns via std::unique_ptr.
    // Cited in design §8 + plan §Task 7.  Created lazily inside the WDSP-init
    // lambda once m_txChannel + m_psFeedbackChannel are live.  Returns nullptr
    // before that point (and after teardown).
    PureSignal* pureSignal() const { return m_pureSignal.get(); }
    PureSignalSettings* pureSignalSettings() const { return m_pureSignalSettings; }
    DspAssetService* dspAssets() const { return m_dspAssets; }
    PureSignalSessionFacade* pureSignalFacade() const { return m_pureSignalFacade; }
    Ps3RoutingSnapshot pureSignalRoutingSnapshot() const;
    bool applyNnrModelSelection(quint32 revision, QString* reason = nullptr);

    // Stage C2: expose FilterPresetStore so RxApplet, VfoWidget, and
    // FilterPresetsSetupPage can read/write user-customised presets.
    // Constructed once in RadioModel ctor; lifetime is RadioModel's lifetime.
    FilterPresetStore* filterPresetStore() const { return m_filterPresetStore; }

    // ── Phase 3J-2 H2: spot-system accessors ────────────────────────────────
    // RadioModel owns the seven spot-ingest clients, three view models, and
    // the DxccColorProvider as std::unique_ptr members. Each accessor returns
    // a non-owning pointer; lifetime is RadioModel's lifetime. MainWindow
    // (H1) consumes these to instantiate SpotHubDialog + FreeDVReporterDialog
    // with shared model pointers, and the M3 follow-up task wires the
    // `<Source>/AutoConnect` AppSettings keys to actually start each client.
    //
    // Constructed in RadioModel ctor with identity / endpoint defaults from
    // AppSettings; startConnection() is NOT called at construction time.
    SpotModel*            spotModel()           const { return m_spotModel.get(); }
    // 2026-05-12 bench fix: moved from SpotHubDialog ownership so the
    // table stays populated from app start regardless of whether the
    // dialog is open.  Spots from auto-connected sources were
    // previously dropped on the floor until the user opened Tools →
    // Spot Hub for the first time (the SpotTableModel didn't exist
    // before then), forcing a manual disconnect+reconnect to repopulate.
    SpotTableModel*       spotTableModel()      const { return m_spotTableModel.get(); }
    FreeDVStationModel*   freeDvStationModel()  const { return m_freeDvStationModel.get(); }
    RxDecodeModel*        rxDecodeModel()       const { return m_rxDecodeModel.get(); }
    DxccColorProvider*    dxccColorProvider()   const { return m_dxccColorProvider.get(); }
    DxClusterClient*      dxCluster()           const { return m_dxCluster.get(); }
    DxClusterClient*      rbn()                 const { return m_rbn.get(); }
    WsjtxClient*          wsjtx()               const { return m_wsjtx.get(); }
    SpotCollectorClient*  spotCollector()       const { return m_spotCollector.get(); }
    PotaClient*           pota()                const { return m_pota.get(); }
    FreeDVReporterClient* freeDvReporter()      const { return m_freeDvReporter.get(); }
    PskReporterClient*    pskReporter()         const { return m_pskReporter.get(); }
    // Parity Task 19 (R-IOS-25): the spot sources' starts and stops.
    SpotSourceHost*       spotSourceHost()      const { return m_spotSourceHost.get(); }

    /// Parity Task 19 (R-IOS-25): a remote window applies the Core's
    /// `spots` stream (the Core's cluster, RBN, POTA and PSK Reporter
    /// spots) into its SpotModel and Spot List, beside its own WSJT-X and
    /// SpotCollector spots, and the Core's spotConsole:<source> streams
    /// into the Spot Hub's consoles. A reset replaces the Core's spots.
    void applyStationRecordBatch(const RecordBatch& batch);
    /// The session ended: the Core's spots and its radio list leave this
    /// window.
    void clearStationRecords();
    /// The Core's spots only (a `spots` reset, a new snapshot). Never the
    /// Core's radio list, which has its own stream (fix wave, I3).
    void clearStationSpots();
    /// The Core's radio list only.
    void clearStationRadios();
    /// iPhone plan Task 22 / parity Task 20: the Core's FreeDV Reporter
    /// list only (a remote window's; a new subscription or the session's
    /// end).
    void clearStationFreedv();

    /// Parity Task 21 (R-IOS-18): a remote window's copy of the Core's
    /// radios (the `stationRadios` stream), the Core's radio first.
    QList<StationRadioEntry> stationRadios() const;
    /// Parity Task 21: the Core refused a radio request (Change radio, Scan
    /// again, Edit radio, Forget radio); This Core shows the reason.
    void reportStationRadioRefused(const QString& reason);
    /// Fix wave (M3): the Core is changing its radio (nereusd's DaemonApp).
    /// Keying is refused until the change ends, so nothing keyed is torn
    /// down by it.
    void setStationRadioChangeUnderway(bool underway) { m_stationRadioChangeUnderway = underway; }
    /// Fix wave (M2): the Core's waiting reason (see the property).
    QString stationRadioWaiting() const { return m_stationRadioWaiting; }
    void setStationRadioWaiting(const QString& reason);

    /// Remote-window parity Task 22 (R-R3-49): see the property.
    QString logCategories() const;
    /// Phone wire batch: see the property. This process's categories.
    QString logCategoryList() const;
    /// A remote window's copy of the Core's log (the `coreLog` stream),
    /// oldest first, at most kStationCoreLogLines. Followed only while a
    /// viewer holds it (the Support dialog, Setup > Diagnostics > Logs).
    static constexpr int kStationCoreLogLines = 200;
    QStringList stationCoreLog() const { return m_stationCoreLog; }
    void addStationCoreLogViewer();
    void removeStationCoreLogViewer();
    /// Reads the Core's log again (a new subscription and its backlog).
    void refreshStationCoreLog();
    /// Why a remote window cannot show or change the Core's log or collect
    /// its bundle (empty when it can).
    QString stationSupportUnavailableReason() const;
    /// The link's support availability moved (capabilities, a new session).
    void noteStationSupportAvailabilityChanged();
    /// The Core answered support.collect `commandId` (`bundle` its ZIP).
    void reportStationSupportBundle(quint32 commandId, bool accepted, const QString& reason,
                                    const QByteArray& bundle);
    /// The Core refused support.setLogCategories; the dialog shows why.
    void reportStationLogCategoriesRefused(const QString& reason);
    bool stationRadioChangeUnderway() const { return m_stationRadioChangeUnderway; }

    // ── TNF (design section 8.1): the canonical notch store ─────────────────
    //
    // Constructed in the RadioModel ctor and restored from AppSettings there,
    // before any WDSP channel exists, so the openRxChannelPool-tail reconcile
    // (section 6.3) always has the full list to install. Non-owning pointer;
    // lifetime is RadioModel's. Consumed by the TCI rx_nf_enable repoint
    // (section 6.4), the +TNF button and status-bar light (section 7), and
    // MnfSetupPage (section 9).
    NotchModel*           notchModel()          const { return m_notchModel.get(); }

    // ── Phase 3J-2 + 3R M3: spot-client auto-start state restore ────────────
    //
    // Reads each per-source AutoConnect / AutoStart key from AppSettings
    // and, when True, calls the corresponding start method with the
    // persisted identity / port / interval params. Designed to be called
    // once at launch from MainWindow (sibling to tryAutoReconnect for the
    // radio connection itself).
    //
    // Keys consulted (all flat PascalCase, matching SpotHubDialog F2):
    //   DxClusterAutoConnect   -> connectToCluster(host, port, callsign)
    //   RbnAutoConnect         -> same shape, different host default
    //   WsjtxAutoStart         -> startListening(address, port)
    //   SpotCollectorAutoStart -> startListening(port)
    //   PotaAutoStart          -> startPolling(intervalSec)
    //   FreeDvAutoStart        -> startConnection() (identity / URL
    //                              already plumbed by RadioModel ctor)
    //   PskReporterAutoStart   -> no-op (PSK Reporter is send-only)
    //
    // Safe to call multiple times. Each client's start method already
    // guards against double-start.
    void restoreSpotClientAutoStartState();
    /// Parity Task 19 (R-IOS-25): the Core (nereusd) starts the station's
    /// sources (DX cluster, RBN, POTA, PSK Reporter) whose Auto-Connect or
    /// Auto-Start is on, with no window. A window's restore above starts
    /// every source when it runs its own radio, and only its own WSJT-X and
    /// SpotCollector listeners in a remote window.
    void restoreStationSpotSources();

    // ── Phase 3R Task I5: RadeChannel slot-graph wiring ─────────────────────
    //
    // wireRadeChannel attaches a freshly-created RadeChannel into RadioModel's
    // slot graph. Phase J calls this from createRadeChannel (J2) / mode-swap
    // (J3) after constructing the channel.
    //
    // The channel's per-channel signals (snrChanged / syncChanged /
    // rxTextDecoded) do not carry a slice ID; the wiring adapts each through
    // a captured-sliceId lambda so the receiving RadioModel slots know which
    // slice to apply the event to.
    //
    // Routing:
    //   RadeChannel::snrChanged       -> onRadeSnrChanged     -> SliceModel::setSnrDb
    //                                                         -> radeSnrChanged re-emit
    //   RadeChannel::syncChanged      -> onRadeSyncChanged    -> radeSyncChanged
    //                                                            (only on transition)
    //   RadeChannel::rxTextDecoded    -> onRadeTextDecoded    -> RxDecodeModel::addDecode
    //
    // Null channel or null slice is a safe no-op.
    void wireRadeChannel(int sliceId, NereusSDR::RadeChannel* channel,
                         NereusSDR::SliceModel* slice);

    // RADE reason (2026-09-30), NereusSDR-original. How the last attempt to
    // start a slice's RADE decoder ended, kept while the slice stays in RADE
    // so its radeReason can say why it is silent. Main thread only.
    enum class RadeStartFault {
        None,          // started, or no attempt failed
        CreateFailed,  // WdspEngine::createRadeChannel gave no channel
        StartFailed,   // RadeChannel::start returned false
        ModelMissing,  // start refused: the configured model file is absent
        NotRunning,    // admission refused: the receiver is not running
    };
    // Every path that makes and starts a RADE decoder (SliceModel's mode
    // change, the restored owner's admission, the WDSP-init path) brackets
    // the attempt: begin clears the slice's last fault and holds the
    // reasons, so the route's own refreshes in between never show a
    // passing state; end records how it ended and refreshes them.
    void beginRadeStart(int sliceId);
    void endRadeStart(int sliceId, RadeStartFault fault);
    // Sets each slice's radeReason from its mode, its route and its last
    // fault (radeStartReason). Local role only; a remote window mirrors the
    // Core's. Runs from publishRadeModeSlices, so every slice-list change,
    // mode change and route change refreshes it.
    void refreshRadeReasons();
    // The plain sentence for one slice; empty when it decodes, is not in
    // RADE, or its receiver is not running.
    QString radeStartReason(const SliceModel* slice) const;

    // Reads the latest RADE sync state for the given slice ID. Returns
    // false when the slice has no recorded sync state (e.g. RADE was
    // never wired for that slice). Surface for the future Phase L
    // RadeApplet status indicator + status-bar SYNC badge.
    bool radeSynced(int sliceId) const;

    // 3M-1a G.1: expose TxChannel view so TxApplet and G.4 TUNE function
    // can call setTuneTone / setRunning without depending on WdspEngine.
    // Non-owning; WdspEngine owns the channel. Null until WDSP initializes.
    // Master design §5.1.1; pre-code review §2.5.
    // TxChannel::setConnection() + setMicRouter() inject the production loop
    // pointers in the WDSP-init lambda (see connectToRadio). The 5 ms QTimer
    // in TxChannel drives fexchange2 → sendTxIq (SPSC ring) while running.
    // Wired by 3M-1a Task G.1 (bench fix: TUNE carrier now reaches the radio).
    TxChannel* txChannel() const { return m_txChannel; }

    // ── AM Mod Monitor (NereusSDR-original) ───────────────────────────────
    /// source 0 = TX I/Q leaving WDSP, 1 = PureSignal feedback receiver.
    AmModulationAnalyzer* amModulationAnalyzer(int source) const;
    /// Receiver stream index the feedback analyzer listens to (HL2: 1).
    void setAmModFeedbackStream(int streamIndex);
    int  amModFeedbackStream() const { return m_amModFbStream.load(); }
    /// The applet sets this when its PA-feedback source is selected so the
    /// connection-thread I/Q fork only pays for the analysis when wanted.
    void setAmModFeedbackWanted(bool wanted);
    bool amModFeedbackWanted() const { return m_amModFbWanted.load(); }
    /// R-IOS-13 / R-R3-49: whether the TX I/Q tap feeds the TX analyzer.
    /// True by default (a local window's applet reads it whenever it is
    /// shown); the Core turns it off and on for a watching device
    /// (ModMonitorPublisher), so with nobody watching the analyzer costs
    /// nothing. Takes effect on the live transmit channel at once.
    void setAmModTxTapEnabled(bool enabled);
    bool amModTxTapEnabled() const { return m_amModTxTapEnabled.load(); }
    /// The Core applies a window's ModMon/FbStream (the feedback receiver
    /// the Mod Monitor's PA FB source listens to), or its removal (rx1),
    /// at once. Ignores every other key.
    void applyModMonitorSetting(const QString& key, const QVariant& value);
    /// A remote window's copy of the Core's Mod Monitor readings for a
    /// source (0 TX I/Q, 1 PA feedback), from the txAmModulation and
    /// txAmModulationFeedback streams; nullopt while the Core sends none
    /// (nobody keyed in AM, SAM or DSB, or not subscribed).
    std::optional<AmModulationAnalyzer::Snapshot> stationModMonitorSnapshot(int source) const;
    /// The Core's Mod Monitor readings leave this window.
    void clearStationModMonitor();

    // Phase 3F Sub-Epic J Task 11: the one place GUI code may resolve a
    // slice's WDSP channel. Mirrors txChannel()'s shape. Added so MainWindow
    // and the Setup pages can stop calling wdspEngine()->rxChannel()
    // directly -- that direct reach is what let ANF-on-slice-B toggle slice
    // A (this same sub-epic, Task 1). RadioModel stays the only layer that
    // touches WdspEngine::rxChannel(); everything under src/gui/ goes
    // through this accessor instead (enforced by
    // scripts/verify-no-gui-dsp-access.py).
    // Returns nullptr if WDSP has not initialised yet or sliceIndex has no
    // channel.
    RxChannel* rxChannelForSlice(int sliceIndex) const;

    // Phase 3P-II: PGXL / TGXL / Tuner accessors.
    // PgxlConnection and TgxlConnection are QObject children of RadioModel
    // (constructed once in the ctor with parent=this). TunerModel is likewise
    // a QObject child that binds its connection once in the ctor.
    // All three accessors return non-null pointers from construction time.
    PgxlConnection* pgxlConnection() { return m_pgxlConnection; }
    Rf2ksConnection* rfKitConnection() const { return m_rfKitConnection.get(); }
    TgxlConnection* tgxlConnection() { return m_tgxlConnection; }
    TunerModel*     tunerModel()     { return m_tunerModel;     }
    // R-R3-47 / R-R3-22: the Power Genius XL and RF-Kit RF2K-S status, as
    // the Core mirrors them (`amplifier`, `rfkit`). Non-null from
    // construction. A Local model binds them to its own connections; a
    // Remote model's hold the Core's values only.
    AmplifierModel* amplifierModel() const { return m_amplifierModel; }
    RfKitModel*     rfKitModel()     const { return m_rfKitModel; }
    // R-R3-48: the Core's station TCI server as the `stationTci` object.
    // Non-null from construction; filled only on a Core that runs one
    // (enableStationTci), and from the Core's values in a remote window.
    StationTciModel* stationTciModel() const { return m_stationTciModel; }
    // R-R3-48: the Core's station TCI server (nullptr outside the Core).
    StationTciController* stationTciController() const { return m_stationTci.get(); }
    // R-R3-47: the Core's RF-Kit controller (nullptr outside the Core).
    StationRfKitController* stationRfKitController() const { return m_stationRfKit; }
    // SmartSDR API server on TCP 4992. Owned by RadioModel; lifetime matches.
    // Used by MainWindow to push slice/transmit state so PGXL/TGXL pull the
    // current band/freq via the SmartSDR API rather than from a stale cache.
    class SmartSdrApiListener* smartSdrListener() { return m_smartSdrListener; }

    // Live toggle for the 4O3A master switch (Settings -> CAT & Network ->
    // 4O3A -> General tab).  Starts or stops the TCP 4992 listener
    // without requiring an app restart.  Persisted automatically via
    // AppSettings key "FourO3A_Enabled".  Default OFF on first run.
    //
    // When false: TCP 4992 not bound, PGXL/TGXL auto-connect skipped,
    // and the detail tabs (PowerGenius XL / Tuner Genius XL / Diagnostics)
    // are disabled in the Setup UI.
    //
    // When true: listener starts (if not already running), AppSettings
    // persisted, FourO3APage updates its enabled state.
    void setFourO3AEnabled(bool enabled);
    bool fourO3AEnabled() const;
    bool fourO3AListening() const;
    QString fourO3AListenerError() const;
    bool setFourO3AEnabledForStation(bool enabled, QString* reason);
    void clearRemoteFourO3AState();
    void reportStationFourO3ACommandFinished(bool accepted, const QString& reason)
    { emit stationFourO3ACommandFinished(accepted, reason); }

    // R3 station-owned accessory lifecycle. Installed by DaemonApp before
    // radio startup, independently of the temporary receive-only policy.
    void enableStationAccessoryIdentity();
    bool stationAccessoryIdentityEnabled() const { return m_stationTgxl != nullptr; }
    bool configureTgxlForStation(const QString& host, quint16 port, QString* reason);
    bool disconnectTgxlForStation(QString* reason);
    // R-R3-47 / R-R3-22: the Core's Power Genius XL, as the tuner's above.
    // configure saves the address for the Core's radio and starts
    // identifying what answers there; it is paired only once admitted.
    bool configurePgxlForStation(const QString& host, quint16 port, QString* reason);
    bool disconnectPgxlForStation(QString* reason);
    /// Saves and applies the amp's connection settings on the Core:
    /// automatic retry, keepalive seconds (1 to 3600) and ping seconds
    /// (0 turns it off, up to 3600).
    bool setPgxlConnectionSettingsForStation(bool autoReconnect, int keepaliveSec,
                                             int pingSec, QString* reason);
    // R-R3-49 (parity Task 9, remotePgxlControlVersion 4): a window's
    // OPERATE or STANDBY. The Core sends the local applet's own line,
    // `operate=1` or `operate=0`, through its PgxlConnection; the amp's
    // report returns on AmplifierModel. It keys nothing. Refused on a Core
    // that does not own its accessories, while the radio is on the air, and
    // while the Core is not connected to the amp; nothing is sent then.
    bool setPgxlOperateForStation(bool on, QString* reason);
    // A window's Scan LAN: as scanTgxlLanForStation, for Power Genius
    // announcements (StationPgxlController::expectedProduct()). Nothing is
    // sent to any device.
    bool scanPgxlLanForStation(std::function<void(const QString& devicesJson)> done,
                               QString* reason);
    // A Host or Port typed on a window's Peripherals row without Connect:
    // saves PGXL_ManualIp and PGXL_ManualPort for the Core's radio without
    // dialling, with configurePgxl's address checks and reasons. A blank
    // host is saved (group B fix wave, I1), as a local window's blank Host
    // stops auto-connect. It switches nothing, so it goes ahead on the air
    // (parity mini-round, rulings a and b). The `amplifier` object's
    // configured address follows while the Core is not connecting or
    // connected.
    bool setPgxlAddressForStation(const QString& host, int port, QString* reason);
    // R-R3-47 / R-R3-22: the Core's RF-Kit RF2K-S. configure saves the
    // address for the Core's radio and starts identifying what answers
    // there; the amp is admitted once its /info names an RF2K-S.
    bool configureRfKitForStation(const QString& host, quint16 port, QString* reason);
    bool disconnectRfKitForStation(QString* reason);
    /// I4 (R-R3-47, remoteRfKitControlVersion 3): a window's Reset amp
    /// error, sent to the admitted amp as the local page sends it.
    bool resetRfKitErrorForStation(QString* reason);
    /// The station's RF-Kit switch, from a window's command.
    bool setRfKitEnabledForStation(bool enabled, QString* reason);
    // R-R3-49 (parity Task 10, remoteRfKitControlVersion 4): a window's
    // RF-Kit OPERATE or STANDBY, ANT 1 to 4 and "Set amp to TCI mode". The
    // Core's admitted amp gets the request the local applet or page sends
    // (StationRfKitController). None keys anything. Refused on a Core that
    // does not own its accessories, while the radio is on the air, and
    // while the Core is not connected to the amp; nothing is sent then. An
    // antenna outside 1 to 4, or one the amp lists as disabled or does not
    // list, is refused too.
    bool setRfKitOperateForStation(bool on, QString* reason);
    bool setRfKitAntennaForStation(int port, QString* reason);
    bool setRfKitTciModeForStation(QString* reason);
    // A Host and Port saved from a window's RF-Kit page: saves
    // RfKit_ManualIp and RfKit_ManualPort for the Core's radio without
    // dialling, with configureRfKit's address checks and reasons (the
    // RF-Kit switch is not an address check: saving dials nothing). A
    // blank host is saved (group B fix wave, I1), as a local window's
    // blank Host stops auto-connect. It switches nothing, so it goes ahead
    // on the air (parity mini-round, rulings a and b). The `rfkit` object's
    // configured address follows while the Core is not connecting or
    // connected.
    bool setRfKitAddressForStation(const QString& host, int port, QString* reason);

    // R-R3-48: the Core runs the app's TCI server on the station network
    // (DaemonApp, before radio startup). `bindOverride` is nereusd.conf's
    // station_bind (older name station_tci_bind); empty picks this
    // computer's address on the radio's subnet once the radio connects.
    // Once setStationBind has run, its rule wins.
    void enableStationTci(const QString& bindOverride);
    /// The station's TCI switch and port, from a window's command.
    bool setStationTciForStation(bool enabled, int port, QString* reason);
    /// Parity Task 23 (stationTciVersion 2): the station server's four
    /// options, from a window's setStationTciOptions.
    /// JJ's ruling of 2026-09-28 (stationTciSettingsVersion 1): the rest
    /// of the station server's settings, from a window's or an app's
    /// setStationTciSettings (StationTciController::setSettings).
    bool setStationTciSettingsForStation(const QVariantMap& changes, QString* reason);
    bool setStationTciOptionsForStation(bool emulateExpertSdr3, bool emulateSunSdr2Pro,
                                        bool cwluBecomesCw, bool sendInitialState,
                                        QString* reason);
    /// Parity Task 23: closes one app on the station server (its
    /// `tciClients` id), from a window's disconnectStationTciClient.
    bool disconnectStationTciClientForStation(const QString& id, QString* reason);

    // R-R3-22 / R-R3-47 / R-R3-48: the Core's station network (DaemonApp,
    // before radio startup). The SmartSDR API listener on TCP 4992, the
    // Power Genius and Tuner Genius discovery sockets and the station TCI
    // server then all accept connections there only, by one rule
    // (StationNetwork::StationBind): `bindOverride` is nereusd.conf's
    // station_bind (older name station_tci_bind); empty follows the radio's
    // subnet, with 127.0.0.1 alone until a radio connects; a new radio
    // address moves every listener. A desktop window never calls this and
    // binds as it always has. The FlexRadio beacon then announces the
    // station address.
    void setStationBind(const QString& bindOverride);
    /// The rule as it stands (radio address included); unset in a desktop window.
    std::optional<StationNetwork::StationBind> stationBind() const { return m_stationBind; }
    /// Test seam: this computer's address entries for the station rule.
    void setStationInterfaceEntriesForTest(const QList<QNetworkAddressEntry>& entries);
    static constexpr int kPgxlKeepaliveMinSec = 1;
    static constexpr int kPgxlKeepaliveMaxSec = 3600;
    static constexpr int kPgxlPingMaxSec = 3600;

    // Phase 3P-III RF-Kit RF2K-S master toggle.
    // Persisted per-MAC under hardware/<mac>/peripherals/RfKit_Enabled.
    // Default OFF on first run. When true the Rf2ksApplet and Setup tab
    // become active; when false the REST poller is idle and the applet
    // is hidden. No-op when not connected (no MAC scope to write under).
    void setRfKitEnabled(bool enabled);
    // Pushes RfKit_AutoReconnect + RfKit_PollIntervalMs from AppSettings into
    // the live connection. Called before every connectToAmp().
    void applyRfKitOperatorSettings();
    bool rfKitEnabled() const;

    // ── Per-radio peripherals scope (RF-Kit / 4O3A / PGXL / TGXL) ────────
    //
    // The four external-amp accessories used to read GLOBAL AppSettings
    // keys so they fired on every radio regardless of which one was
    // connected.  As of this refactor each accessory's enable + connection
    // info is scoped under hardware/<mac>/peripherals/<key>.
    //
    // Storage format:
    //   hardware/<mac>/peripherals/RfKit_Enabled       "True" | "False"
    //   hardware/<mac>/peripherals/RfKit_ManualIp      string
    //   hardware/<mac>/peripherals/RfKit_ManualPort    int (string-encoded)
    //   hardware/<mac>/peripherals/FourO3A_Enabled     "True" | "False"
    //   hardware/<mac>/peripherals/PGXL_ManualIp       string
    //   hardware/<mac>/peripherals/PGXL_ManualPort     int (string-encoded)
    //   hardware/<mac>/peripherals/TGXL_ManualIp       string
    //   hardware/<mac>/peripherals/TGXL_ManualPort     int (string-encoded)
    //
    // peripheralValue(key, default):
    //   Returns the per-MAC value when connected, defaultValue otherwise.
    //   Use this for every read of the keys listed above.
    //
    // setPeripheralValue(key, value):
    //   Writes the per-MAC value when connected.  When NOT connected this
    //   is a no-op + qCWarning(lcConnection) so a Setup page that fires
    //   before a radio is selected can't silently lose data.  Setup pages
    //   should gray themselves out via connectionStateChanged().
    //
    // currentRadioMac():
    //   Returns m_lastRadioInfo.macAddress when connected, empty otherwise.
    //   Setup pages use this to populate the "Editing peripherals for ..."
    //   banner.  Tests reach for setLastRadioInfoForTest() to drive the
    //   per-MAC scope without a live RadioConnection.
    QString peripheralValue(const QString& key,
                            const QString& defaultValue = QString{}) const;
    void    setPeripheralValue(const QString& key, const QString& value);
    QString currentRadioMac() const;

    bool hasAmplifier() const { return m_hasAmplifier; }
    bool ampOperate()  const  { return m_ampOperate; }

    // ── iPhone app plan Task 77 fix round 2: the Power Genius changeover ──
    //
    // The Core sends the amplifier operate=0 or operate=1 (an autotune's
    // standby, the restore after it) only while MOX reads receive, no key
    // is pending and no earlier command is still unconfirmed; otherwise
    // the restore stays owed and is retried at each unkey and release.
    //
    // A key pending beyond this model's own view (a take running, a
    // device's key waiting for its microphone, a device's two-tone
    // settling): the Core's session server installs it. Unset: none.
    using AmpKeyPendingFn = std::function<bool()>;
    void setAmpKeyPendingProbe(AmpKeyPendingFn probe) { m_ampKeyPending = std::move(probe); }
    /// Scoped review (addendum G-42): the sentence for "another device
    /// holds transmit" on a Core, empty while none does (or on a desktop
    /// with no session server). The session server installs it and tells
    /// the pages when the holder changes (transmitHolderChanged). A Core's
    /// own window reads it before changing a setting the Core takes only
    /// from the holder, such as Extended transmit.
    using OtherDeviceHoldsRefusalFn = std::function<QString()>;
    void setOtherDeviceHoldsRefusal(OtherDeviceHoldsRefusalFn probe)
    { m_otherDeviceHoldsRefusal = std::move(probe); }
    QString otherDeviceHoldsRefusal() const
    { return m_otherDeviceHoldsRefusal ? m_otherDeviceHoldsRefusal() : QString(); }
    void reportTransmitHolderChanged() { emit transmitHolderChanged(); }
    /// The amplifier is changing over: from any operate=0 or operate=1
    /// written to it (PgxlConnection::operateCommanded) until its status
    /// reports the commanded state. Always false with no Power Genius
    /// connected. While it is, a key holds its RF at the RF-flow gate (a
    /// third condition beside txReady and interlockGranted); a key still
    /// held kAmpChangeoverBoundMs after the command is stopped with
    /// ampNotSwitchedText(), never let through.
    bool ampChangingOver() const;
    static constexpr int kAmpChangeoverBoundMs = 1500;
    /// The stop's words (plain, for the operator and the device): what
    /// happened and how to transmit without the amplifier.
    static QString ampNotSwitchedText();
    /// Task 77 fix round 3: a Tuner Genius cycle that ended unkeyed because
    /// the amplifier did not go to standby for it (or was put back in
    /// operate during the wait).
    static QString ampNotStandbyForTuneText();
    /// Tune-ended lane (2026-10-01): why a device's autotune ended before
    /// its carrier keyed, where no refusal of its own says it.
    enum class TunerTuneEnd {
        TunerDisconnected,   // the Tuner Genius link dropped
        TunerStopped,        // the tuner let go of the tune first
        CarrierNotStarted,   // the tune carrier was refused, no words given
        TransmitTaken,       // another device or the radio's PTT took transmit
        LinkLost,            // the device's session ended (its link dropped or it left)
        NoReasonGiven,       // the backstop: an end that named no reason
    };
    /// The words of each, for the device's tuneEnded notice.
    static QString tunerTuneEndedReason(TunerTuneEnd end);
    /// Task 77 fix round 4: the amplifier reported operating, commanded by
    /// nobody, while the radio transmitted (a fault clearing by itself, or
    /// its front panel): the key is stopped with these words.
    static QString ampOperatedUnderKeyText();
    static constexpr char kAmpOperatedUnderKeyStopCode[] = "ampOperatedUnderKey";
    /// Task 77 fix round 4: a SmartSDR `amplifier set <key>=<value>` for
    /// the amp model `model` ("PowerGeniusXL" or "TunerGeniusXL"), relayed
    /// to its own connection. A Power Genius operate=1 is held while the
    /// radio's RF may flow (tgxlRfFlowing) and sent once it reads receive;
    /// operate=0 goes at once (standby is the safe direction).
    void forwardAmplifierSet(const QString& model, const QString& key, const QString& value);
    bool isRelayedPgxlOperateHeld() const { return m_relayedPgxlOperateHeld; }
    /// The stop reason code lastTransmitStopReason() carries for it.
    static constexpr char kAmpNotSwitchedStopCode[] = "ampNotSwitched";
    /// An owed amplifier restore, sent now if nothing is pending. The
    /// session server calls it when a take or a device's pending key ends.
    void retryOwedAmpRestore();

    // Cross-vendor "is any external amp currently amplifying?" predicate.
    // True if PGXL is connected + in OPERATE OR if the RF-Kit RF2K-S is in
    // OPERATE.  Used by MainWindow's SMeterWidget wiring to decide whether
    // to feed the TX needle from the radio's barefoot meters or from an
    // external amp's telemetry.  Phase 3P-III bench fix 2026-05-25 KG4VCF:
    // without this gate, PGXL Connect 2 (radio TX power) was overwriting
    // RF-Kit Connect B (amp forward power) at the radio's higher emit rate.
    bool isAnyExternalAmpInOperate() const;

    // RF-Kit-only counterpart to the predicate above: true when the RF2K-S
    // is connected AND reporting OPERATE, ignoring PGXL entirely.
    //
    // Needed because externalAmpFwdSwrUpdated carries RF-Kit telemetry
    // exclusively, so gating that feed on the cross-vendor predicate let a
    // PGXL in OPERATE wave through RF-Kit /power polls from an amp sitting
    // in STANDBY, overwriting the live PGXL meter with RF-Kit's 0 W.  A
    // per-source feed needs a per-source gate.  Codex review, PR #291.
    bool isRfKitInOperate() const;

    // Phase 3P-II Task 86: TxInterlockPolicy -- NereusSDR-native TX gate.
    // Constructed once in the ctor (Qt parent-ownership). Non-null from
    // construction time. Shared with PgxlInterlockPage (non-owning read/write)
    // and MoxController (non-owning gate via setInterlockPolicy).
    TxInterlockPolicy* txInterlockPolicy() { return m_txInterlockPolicy; }

    // Phase 3P-II Phase 4 Task 89: TuneMemoryStore -- shared per-(antenna,band)
    // TGXL relay position cache. Constructed once in the ctor (Qt parent-ownership).
    // Non-null from construction time. Shared by TgxlAdvancedPage (non-owning
    // view/edit) and TunerApplet (non-owning save/recall from context menu).
    TuneMemoryStore* tuneMemoryStore() { return m_tuneMemoryStore; }

    // Phase 3P-II Phase 4 Task 94: FaultLog -- shared PGXL / TGXL fault ring buffers.
    // Constructed once in the ctor (Qt parent-ownership). Non-null from construction
    // time. RadioModel captures PGXL FAULT state transitions into m_pgxlFaultLog.
    // Shared (non-owning) with PgxlAdvancedPage and TgxlAdvancedPage so their
    // Fault History tables reflect live captures.
    FaultLog* pgxlFaultLog() { return m_pgxlFaultLog; }
    FaultLog* tgxlFaultLog() { return m_tgxlFaultLog; }
    // R-R3-47: the RF-Kit's faults (a lost link, a refused identity, an
    // interface error), key RfKit_FaultHistory.
    FaultLog* rfkitFaultLog() { return m_rfkitFaultLog; }

    // R-R3-47 / R-R3-22: the Power Genius and Tuner Genius connection
    // counters. A Local model's are bound to its own connections for its
    // whole life; a Remote model's hold the Core's counters. Non-null
    // from construction.
    ConnectionDiagnostics* pgxlDiagnostics() const { return m_pgxlDiagnostics; }
    ConnectionDiagnostics* tgxlDiagnostics() const { return m_tgxlDiagnostics; }

    // R-R3-47 / R-R3-22: the Core's accessory records and settings, as the
    // Core mirrors them (`accessoryData`). Non-null from construction. A
    // Local model keeps it current (StationAccessoryData); a Remote model
    // holds the Core's values and feeds them into its fault logs, counters,
    // interlock policy and tune memory so the pages show the Core's.
    AccessoryDataModel* accessoryDataModel() const { return m_accessoryDataModel; }
    // nullptr in a Remote model.
    StationAccessoryData* stationAccessoryData() const { return m_stationAccessoryData; }

    // R-R3-47 / R-R3-22: the three commands (accessoryDataVersion 1). The
    // policy's enforcement stays in MoxController; a change keys nothing.
    bool setTxInterlockPolicyForStation(int mode, int graceMs, bool swrGateEnabled,
                                        double swrGateMax, QString* reason);
    bool setPgxlPowerCapForStation(bool enabled, int watts, QString* reason);
    bool clearAccessoryFaultsForStation(const QString& device, QString* reason);
    /// A window changed a station setting (StationServer, after the write
    /// is stored): the Core's interlock policy, output limit, tune memory,
    /// antenna names, fault logs and RF-Kit auto-reconnect and poll
    /// interval follow it at once. Other keys ignored.
    void applyRemoteAccessorySetting(const QString& key);
    /// iPhone plan Task 22 / parity Task 20 (B7.3): a window's grid square
    /// write reaches the Core's FreeDV Reporter list (distance and heading)
    /// at once. Other keys: no change.
    void applyRemoteFreedvSetting(const QString& key, AppSettings& settings);

    // R-R3-47 / R-R3-22: the Power Genius's and Tuner Genius's own settings
    // (`accessorySettings`, remotePgxlControlVersion 3 and
    // remoteTgxlControlVersion 1). Non-null from construction. On the Core
    // the station controllers keep it current; a Remote model holds the
    // Core's values for the Advanced pages.
    AccessorySettingsModel* accessorySettingsModel() const { return m_accessorySettingsModel; }
    // A window's request, sent by the Core as the local Advanced page's own
    // device command (StationDeviceSettings). True when it left for the
    // device; the answer arrives on accessorySettingsModel().
    bool setPgxlNameForStation(const QString& name, QString* reason);
    /// `setting` is "biasMode" (utf8 ClassA or ClassAB), "fanMode" (utf8
    /// Auto, Quiet or Continuous) or "ledIntensity" (0 to 100).
    bool setPgxlHardwareForStation(const QString& setting, const QVariant& value,
                                   QString* reason);
    bool setPgxlNetworkForStation(bool dhcp, const QString& address, const QString& netmask,
                                  const QString& gateway, QString* reason);
    bool savePgxlSettingsForStation(QString* reason);
    bool readPgxlSettingsForStation(QString* reason);
    bool setTgxlNameForStation(const QString& name, QString* reason);
    bool setTgxlNetworkForStation(bool dhcp, const QString& address, const QString& netmask,
                                  const QString& gateway, QString* reason);
    bool saveTgxlSettingsForStation(QString* reason);
    bool readTgxlSettingsForStation(QString* reason);
    // R-R3-49 / R-R3-47 (remoteTgxlControlVersion 2): a window switches the
    // Core's Tuner Genius antenna (port 1 to 3), operate and bypass through
    // the Core's own TunerModel, the local applet's command slots. Refused,
    // with nothing sent to the tuner, while the radio is on the air (MOX,
    // TUNE or two-tone), with no Tuner Genius admitted, and for the antenna
    // on a tuner with no antenna switch or a port outside 1 to 3. True when
    // the command left for the tuner; the tuner's report comes back on the
    // mirrored `tuner` object.
    bool setTgxlAntennaForStation(int port, QString* reason);
    bool setTgxlOperateForStation(bool on, QString* reason);
    bool setTgxlBypassForStation(bool on, QString* reason);
    // R-R3-49 (parity Task 8, remoteTgxlControlVersion 4): a window's
    // mouse-wheel nudge of one matching relay (`relay` 0 C1, 1 L, 2 C2;
    // `direction` -1 or 1), sent through the Core's own TunerModel as the
    // local applet's `tune relay=<relay> move=<move>` line. It keys
    // nothing. Refused as the switches above are, and for other values.
    bool moveTgxlRelayForStation(int relay, int direction, QString* reason);
    // A window's Scan LAN: the Core listens for Tuner Genius announcements
    // for the local Scan LAN dialog's own window (kTgxlLanScanWindowMs) and
    // calls `done` once with a JSON array of {"address","port","model",
    // "serial","nickname"}. Refused on a Core that does not own its
    // accessories; `done` is not called then. It only listens, so it goes
    // ahead on the air (parity mini-round, rulings a and b). Nothing is
    // sent to any device.
    using TgxlLanScanDone = std::function<void(const QString& devicesJson)>;
    bool scanTgxlLanForStation(TgxlLanScanDone done, QString* reason);
    static constexpr int kTgxlLanScanWindowMs = 3000;
    // A Host or Port typed on a window's Peripherals row without Connect:
    // saves TGXL_ManualIp and TGXL_ManualPort for the Core's radio without
    // dialling, with configureTgxl's address checks and reasons. A blank
    // host is saved (group B fix wave, I1), as a local window's blank Host
    // stops auto-connect. It switches nothing, so it goes ahead on the air
    // (parity mini-round, rulings a and b). The `tuner` object's configured
    // address follows while the Core is not connecting or connected.
    bool setTgxlAddressForStation(const QString& host, int port, QString* reason);
    // R-R3-49 (parity Task 2, transmitSettingsVersion 2): a window's Tune
    // Power slider. Sets the tune power for the band the Core transmits on
    // and the tune drive source to the tune slider, as the local slider
    // does. Refused, changing nothing, while the radio is on the air and
    // outside the tune power range.
    /// Remote parity on the air (transmitSettingsVersion 13): with
    /// `takenOnAir` (the peer may change the transmit settings) these five
    /// are taken while the radio is on the air, as the local controls are;
    /// otherwise they wait for it to stop, as before.
    bool setTunePowerForTxBandForStation(int watts, QString* reason, bool takenOnAir = false);
    // R-R3-49 (parity Task 3, transmitSettingsVersion 3): a window's TX
    // profile combos and Setup > Audio > TX Profile. Select applies the
    // profile as the local combo does; save stores the Core's current
    // transmit settings under the name, overwriting only that name, as the
    // local Save does; delete removes it, never the last one. Each is
    // refused, changing nothing, while the radio is on the air, and select
    // and delete for a name the Core does not have. The Core's active
    // profile and list come back on `transmit` (activeTxProfile,
    // txProfilesJson). None keys the radio.
    bool selectTxProfileForStation(const QString& name, QString* reason,
                                   bool takenOnAir = false);
    bool saveTxProfileForStation(const QString& name, QString* reason,
                                 bool takenOnAir = false);
    bool deleteTxProfileForStation(const QString& name, QString* reason,
                                   bool takenOnAir = false);
    // R-R3-49 / R-IOS-18 (paProfileVersion 1): the PA Gain page's profile
    // actions and edits, for a paProfile verb, through the Core's own
    // PaProfileManager as the local page uses it. Each is refused, changing
    // nothing, while the radio is on the air, for a value outside the
    // page's own control, and for a name the rules refuse. None keys the
    // radio. The Core's `paProfiles` object follows the bank.
    enum class PaProfileAction {
        Select, New, Copy, Delete, Reset, SetGain, SetAdjust, SetMaxPower, SetUseMax
    };
    struct PaProfileRequest {
        PaProfileAction action = PaProfileAction::Select;
        QString name;
        int band = -1;     // Band 0 (160m) .. 13 (XVTR)
        int step = -1;     // drive step 0 (10%) .. 8 (90%)
        double value = 0.0;
        bool on = false;
        // The device asking holds transmit (paOnAirEditRefusal).
        bool requesterHoldsTransmit = false;
    };
    bool paProfileActionForStation(const PaProfileRequest& request, QString* reason);
    // R-R3-49 / R-IOS-27: after a PA edit taken on the air, what Thetis
    // does: a gain re-applies the drive; an adjust moves the drive (or the
    // tune power) to the step being adjusted (setup.cs:24210-24222
    // [v2.10.3.15]). Max power and use-max: nothing more.
    void applyPaEditOnAir(PaProfileAction action, int step);
    // R-R3-49 (parity Task 3): the RADE applet's Reset vocoder. Clears the
    // RADE transmit vocoder of the active slice's RADE channel
    // (RadeChannel::resetTx), as the local button does. Keys nothing.
    // Refused while the radio is on the air and with no RADE channel.
    bool resetRadeVocoderForStation(QString* reason, bool takenOnAir = false);
    // R-R3-49 (parity Task 3): scope the TX profile bank to a radio's MAC and
    // load it, as a connect does (empty: no radio), then publish the
    // profiles on `transmit`. The Core only; a window mirrors the Core's.
    void scopeTxProfiles(const QString& mac);

    // Phase 3G-9b: one-shot profile that sets the 7 smooth-default recipe
    // values on SpectrumWidget. Called from the constructor exactly once
    // on first launch (gated by AppSettings key "DisplayProfileApplied").
    // Also callable on demand via the "Reset to Smooth Defaults" button
    // on SpectrumDefaultsPage, in which case it unconditionally applies
    // regardless of the gate.
    //
    // See docs/architecture/waterfall-tuning.md for the rationale behind
    // each value.
    void applyClaritySmoothDefaults();

    // Radio info
    QString name() const { return m_name; }
    QString model() const { return m_model; }
    QString version() const { return m_version; }

    // Remote Daemon R2 Task 8: StateMirror::applyInbound()'s hook for the
    // three RadioModel properties above with no Q_PROPERTY WRITE (name,
    // model, version -- see MirrorPolicy.cpp). `connected` is deliberately
    // NOT handled here: Task 3 re-points it at m_connectionState and Task
    // 18 drives it through the production setConnectionState() entry point
    // it adds, so this builds no second path into it and simply falls
    // through to the same generic refusal as the other three. All four are
    // hardware identity or connection-lifecycle state RadioModel only ever
    // learns from the connected radio (or Task 18's session) itself; there
    // is no legitimate remote-write path for any of them, so this always
    // refuses.
    Q_INVOKABLE QString applyMirroredValue(const QByteArray& propertyName,
                                           const QVariant& value);

    const HardwareProfile& hardwareProfile() const { return m_hardwareProfile; }

    /// The radio this model is (or was last) connected to: a local model's
    /// discovered radio, or on a Role::Remote model the Core's radio as its
    /// capabilities describe it (R-R3-46). Kept across a disconnect, like
    /// the MAC it carries. The same value currentRadioChanged carries.
    const NereusSDR::RadioInfo& currentRadioInfo() const { return m_lastRadioInfo; }

    // Returns the BoardCapabilities for the current (or last) board.
    // Falls back to the Unknown board caps when no radio has ever connected.
    // Phase 3P-A Task 15: exposes caps so RxApplet can set slider range at
    // construction time, not only after a connection is established.
    const BoardCapabilities& boardCapabilities() const;

    // ── RX meter calibration offset (Thetis-faithful port) ────────────────
    //
    // Returns the cumulative dB offset applied to WDSP S-meter readings
    // (RXA_S_PK, RXA_S_AV) and MaxBin readings before display.  Without
    // this offset, raw WDSP meter values are in ADC dBFS rather than
    // antenna dBm.
    //
    // Ported from Thetis console.cs:21040 [v2.10.3.13]:
    //   public float RXOffset(int rx) {
    //       return RXPreampOffset(rx) + RXCalibrationOffset(rx);
    //   }
    //
    // RXPreampOffset (console.cs:20989) selects between attenuator_data
    // (when step-att enabled) and preamp_offset[mode] (when disabled).
    //
    // RXCalibrationOffset (console.cs:21022) sums per-radio meter cal +
    // XVTR + 6m offsets.  NereusSDR applies the per-radio meter cal
    // (defaults from rxMeterCalOffsetDefaultFor() and the user override
    // AppSettings key RX1_MeterCalOffsetDb) and the RX1 6 m LNA gain offset
    // (rx6mGainOffsetDb); the XVTR offset rides a future transverter epic.
    //
    // Consumed by MeterPoller::pollSMeter and MeterPoller::poll for the
    // SignalPeak / SignalAvg / SIGNAL_MAX_BIN bindings only; matches
    // Thetis console.cs:46824 + :46881 [v2.10.3.13] where +offset is
    // applied to those exact reading types.  ADC_PK / ADC_AV / AGC_PK /
    // AGC_AV / AGC_GAIN do NOT take the offset (Thetis line 46831-46835
    // omit +offset for the same reason).
    //
    // R-R3-46: 0 on a Remote model; the Core's readings and spectrum frames
    // already carry the Core's offset.
    double rxMeterOffsetDb() const;
    // Parity Task 31 (A11): the preamp half of rxMeterOffsetDb(), Thetis
    // RXPreampOffset(1) (console.cs:21029-21037 [v2.10.3.15]); the rest is
    // the receive calibration. 0 on a Remote model.
    double rxPreampOffsetDb() const;
    // The RX1 6 m LNA gain offset's part of rxMeterOffsetDb(), Thetis
    // RX1_6mGainOffset: minus Setup > Calibration's Rx1 6m LNA on 6 m, when
    // the radio has an Alex and the 6 m LNA is in circuit, else 0. 0 on a
    // Remote model.
    double rx6mGainOffsetDb() const;
    // R-R3-46 / R-R3-11: the receive offset of one ADC, Thetis
    // RXOffset(rx) (console.cs:21080-21083 [v2.10.3.15]) for the receiver
    // whose attenuator that ADC carries. Slice A's ADC (and any ADC while
    // diversity links them, or an ADC not in use) is RX1's:
    // rxMeterOffsetDb(). The other ADC in use is RX2's: its own attenuator
    // (or, with the step attenuator off, its preamp,
    // rx2_preamp_offset[rx2_preamp_mode]) plus the receive calibration,
    // whose meter cal Thetis starts from the same per-radio value
    // (console.cs:999). The RX2 XVTR and 6 m LNA terms are not ported, as
    // RX1's XVTR term is not. 0 on a Remote model.
    double rxMeterOffsetDbForAdc(int adc) const;
    // The preamp half of rxMeterOffsetDbForAdc (RXPreampOffset(rx)).
    double rxPreampOffsetDbForAdc(int adc) const;
    // The offset for what a slice hears, and for a stream's spectrum: the
    // ADC it is on (sliceAdcIndex / adcForStream).
    double rxMeterOffsetDbForSlice(int sliceId) const;
    double rxMeterOffsetDbForStream(int stream) const;
    // The meter cal term of the receive calibration: Setup's value
    // (RX1_MeterCalOffsetDb) or the radio's factory default.
    double rxMeterCalOffsetDb() const;
    // Level Cal: the RX1 display calibration (RX1_DisplayCalOffsetDb) or
    // the radio's factory default, Thetis RX1DisplayCalOffset
    // (console.cs:21113-21122 [v2.10.3.15]). As in Thetis it reaches TCI
    // calibration_ex only (TCIServer.cs:1160-1176 [v2.10.3.15]); the
    // panadapter follows the meter cal (console.cs:12305-12311
    // [v2.10.3.15], rxMeterOffsetDb), so this never moves it.
    double rxDisplayCalOffsetDb() const;
    // Level Cal: the connected model's own saved meter or display
    // calibration (RxMeterCalOffsetDbByRadio / RxDisplayCalOffsetDbByRadio,
    // Thetis rx_meter_cal_offset_by_radio and rx_display_cal_offset_by_radio,
    // console.cs:196-197 [v2.10.3.15]); nullopt reads the factory default.
    // The setters change only the connected model's entry.
    std::optional<double> rxMeterCalOverrideDb() const;
    std::optional<double> rxDisplayCalOverrideDb() const;
    void setRxMeterCalOverrideDb(std::optional<double> db);
    void setRxDisplayCalOverrideDb(std::optional<double> db);
    // Level Cal: `key` is RX1_MeterCalOffsetDb or RX1_DisplayCalOffsetDb
    // (just written or removed): refresh the meter offset and emit
    // levelCalibrationChanged, as Thetis's setters fire their changed
    // handlers (console.cs:21091-21122 [v2.10.3.15]). Returns false for any
    // other key. The Core calls it for a window's write too.
    bool applyLevelCalibrationSetting(const QString& key);
    // Level Cal Reset, Thetis ResetLevelCalibration (console.cs:46868-46886
    // [v2.10.3.15]): the meter and display offsets return to the radio's
    // defaults (both keys removed). Nothing else changes.
    void resetLevelCalibration();
    // Whether this window can reset the level calibration: always locally,
    // in a remote window when its Core offers resetLevelCalibration.
    bool levelCalibrationResetAvailable() const;
    // The one call both windows make for Setup's Reset. Empty when it was
    // done (locally) or sent (remote); otherwise the reason it was not.
    QString requestResetLevelCalibration();
    // Level Cal, Thetis CalibrateLevel (console.cs:9856-10232
    // [v2.10.3.15]) run by the Core on one slice (Thetis RX1 and VFO A;
    // -1 the active slice). Whether this window can start one: always
    // locally, in a remote window when its Core offers
    // startLevelCalibration.
    bool levelCalibrationRunAvailable() const;
    // Whether a slice on the other ADC can change RX2's own preamp mode
    // (Thetis RX2PreampMode): always locally, in a remote window when its
    // Core carries stepAtt's rx2PreampMode (radioHardwareVersion 12).
    bool rx2PreampModeAvailable() const;
    // The calls both windows make. Empty when it started or was sent;
    // otherwise the reason it was not. A refusal the Core sends later
    // arrives as levelCalibrationRefused.
    QString requestStartLevelCalibration(float levelDbm, double frequencyHz, int sliceId);
    // Level Cal 2: the slice the hosting desktop's Start names. -1 (the
    // station's active slice) when the station may change that one (its
    // own, or one nobody controls or listens to); else the desktop's own
    // active slice; else -1 with *refusal set to the ownership words, and
    // the desktop's Start is disabled with them. -1 and no refusal in a
    // remote window, which names its own slice (requestStartLevelCalibration).
    int levelCalHostSlice(QString* refusal = nullptr) const;
    QString requestCancelLevelCalibration();
    bool levelCalRunning() const;
    int levelCalPercent() const;
    QString levelCalMessage() const;
    bool levelCalSucceeded() const;
    // Remote window: a levelCal* value from the Core. False on a local
    // model or for any other name.
    bool applyStationLevelCalValue(const QByteArray& name, const QVariant& value);
    // Remote window: the session ended, nothing is known about a run.
    void clearStationLevelCal();
    // The Core's run (created on first use). Null on a Remote model.
    class LevelCalibrationService* levelCalibrationServiceForTest();
    // Recompute rxMeterOffsetDb() and emit rxMeterOffsetChanged if it moved.
    void refreshRxMeterOffset();
    // Level Cal: moves a one-value calibration of an earlier build to the
    // connected model's entry. True when a key moved.
    bool foldLegacyLevelCal();
    void writeLevelCalOverride(bool meter, std::optional<double> db);
    // Level Cal: the receive offset of each preamp setting, Thetis
    // rx1_preamp_offset[] (console.cs:1999-2009 [v2.10.3.15]), which
    // CalibrateLevel measures (console.cs:10026-10140 [v2.10.3.15]). RX1's
    // ten are saved under RX1_PreampOffsetsDb as ten values at three
    // decimals separated by '|' (Thetis saves them the same way,
    // console.cs:3202-3203 [v2.10.3.15]); absent, or not ten numbers, they
    // read Thetis's defaults. RX2's are held while the program runs, as
    // Thetis never saves rx2_preamp_offset. Setting one refreshes the meter.
    float rx1PreampOffsetDbFor(PreampMode mode) const;
    float rx2PreampOffsetDbFor(PreampMode mode) const;
    void setRx1PreampOffsetDb(PreampMode mode, float db);
    void setRx2PreampOffsetDb(PreampMode mode, float db);
    // Parity Task 31 (A11): the display's calibration while keyed, Thetis
    // RX1Offset (display.cs:4820-4850 [v2.10.3.15]) for the transmitting
    // receiver: the TX Display Cal Offset, plus with display duplex on the
    // receive calibration (rxMeterOffsetDb() less the preamp) and the TX
    // attenuator offset (StepAttenuatorController::txAttenuatorOffsetDb).
    // The Core calibrates a remote window's frames with it. 0 on a Remote
    // model.
    double keyedDisplayOffsetDb(bool displayDuplex) const;

signals:
    void stationLinkStateChanged();
    void filterStateChanged();
    // rxFilter0LowPassReason / rxFilter0LowPassSlice changed (the chain's
    // lowPassReason and lowPassSlice). Separate from filterStateChanged so
    // the hold is published on its own; a display of the filter state
    // listens to both.
    void lowPassHoldChanged();
    // bandOutputsByte / bandOutputsBand / bandOutputsKeyed / bandOutputsKnown
    // changed.
    void bandOutputsChanged();
    // alexLpfBits() changed.
    void alexLpfBitsChanged();
    // levelCalRunning / levelCalPercent / levelCalMessage /
    // levelCalSucceeded changed.
    void levelCalStateChanged();
    // Remote window: the Core refused a start this window sent.
    void levelCalibrationRefused(const QString& reason);
    // Emitted when rxMeterOffsetDb() changes (model swap, preamp change,
    // step-att enable/disable, attenuator dB change, or AppSettings
    // RX1_MeterCalOffsetDb override).  MeterPoller connects this to
    // refresh its cached offset value.
    void rxMeterOffsetChanged(double db);
    // R-R3-46 / R-R3-11: an ADC's offset (rxMeterOffsetDbForAdc) may have
    // moved: the other ADC's attenuator or preamp, or which ADC carries
    // which attenuator. Emitted with rxMeterOffsetChanged too.
    void rxAdcMeterOffsetsChanged();
    // Level Cal: the meter or display calibration changed (a write, a
    // Reset, or in a remote window the Core's copy). TCI sends
    // calibration_ex on it.
    void levelCalibrationChanged();

public:

    bool isConnected() const;

    // ── Phase 3Q sub-PR-3: NetworkDiagnosticsDialog text accessors ───────────
    // Each returns an em-dash placeholder ("—") when disconnected.
    // The monotonic connection age is started by setConnectionState() on
    // Connected and invalidated on every retirement, independent of a
    // remote client's telemetry session.
    // In a remote window it is the Core's radio connection age (station
    // telemetry version 6, applyCoreConnectionAge), counting on between
    // samples, and absent while the Core's measurements are not current.
    std::optional<qint64> connectionAgeMs() const;
    QString connectionUptimeText() const;     // "14m 32s" / "—"
    /// Remote role only: the Core's connection age from its latest current
    /// telemetry sample, or absent. Local windows ignore it.
    void applyCoreConnectionAge(std::optional<qint64> ageMs);
    QString connectedRadioName() const;       // RadioInfo.name / "—"
    QString connectionProtocolText() const;   // "1" or "2" / "—"
    QString connectionFirmwareText() const;   // "v27" / "—"
    QString connectionIpText() const;         // "192.168.x.y : port" / "—"
    QString connectionMacText() const;        // "AA:BB:CC:DD:EE:FF" / "—"
    int     connectionSampleRateHz() const;   // 0 if disconnected
    QString connectionSampleRateText() const; // "192 kHz" / "—"

    // Task 1.6 — Sample-rate live-apply coordinator.
    //
    // Changes the sample rate of the active radio connection without
    // disconnecting.  The sequence is:
    //   1. Quiesce the DSP worker (stop I/Q feed into RxDspWorker).
    //   2. Notify AudioEngine of the impending change (pauseInput hook).
    //   3. Rebuild all WDSP channels with the new rate.
    //   4. Update the hardware:
    //      - P1: stop + re-arm EP6 sender with new rate + start.
    //      - P2: send updated CmdRx/CmdTx (already contains new rate).
    //   5. Update RxDspWorker buffer sizes to match the new rate.
    //   6. Reconnect the I/Q feed into RxDspWorker (resume DSP worker).
    //   7. Notify AudioEngine (resumeInput hook).
    //   8. Persist the new rate, update m_connectionSampleRateHz, and
    //      emit wireSampleRateChanged(newRateHz).
    //
    // Returns elapsed milliseconds for the whole operation.  Returns -1
    // if no connection is active or WDSP is not initialized.
    //
    // Must be called on the main thread.
    //
    // Caveats:
    //   - P1 restart is untested on live hardware (design §5C risk note).
    //     A brief audio dropout (one buffer interval) is expected on P1;
    //     P2 is glitch-free in practice.
    //   - TxWorkerThread is stopped before TX channel rebuild and restarted
    //     after.  If MOX is asserted during the change, MOX is silently
    //     dropped.  Callers should ensure MOX is off before calling.
    //   - dspChangeMeasured(qint64) signal (Task 1.8) is emitted on completion.
    //     The elapsed time is also returned synchronously.
    //
    // R-R3-39: the steps run on the receive lane now (setSampleRateLiveAsync);
    // this form waits for them in a local event loop. Tests only; production
    // callers use setSampleRateLiveAsync.
    qint64 setSampleRateLive(int newRateHz,
                             bool reconcileDiversity = true);

    // R-R3-39: the same change without waiting. Returns at once; the twelve
    // steps (Thetis setup.cs:7003-7159 [v2.10.3.13]) run in their order as
    // one receive-lane barrier, and sampleRateChangeFinished(rate, ok)
    // follows exactly once for each change it starts (ok false when there
    // is no connection or WDSP is not ready, or when a later request
    // replaced this one before it started). A request for the rate the
    // radio is at, or is already changing to, starts nothing and emits
    // nothing. A request made while a change runs starts when it finishes.
    // The allocator state, the published sizes and the slices' rates change
    // at once; each channel's rate when the lane re-rates it (step 6, as
    // before); the wire rate (connectionSampleRateHz,
    // wireSampleRateChanged) at the end.
    void setSampleRateLiveAsync(int rateHz, bool reconcileDiversity = true);

    // R-R3-39: the receive lane every RX WDSP call runs on (null on a
    // remote model).
    DspControlThread* receiveLane() const { return m_rxLane.get(); }
    // R-R3-39: the transmit lane every TX WDSP call runs on (null on a
    // remote model). The desktop's TxAnalyzer runs its calls here too.
    DspControlThread* transmitLane() const { return m_txLane.get(); }

    // Task 1.7 — Active-RX-count live-apply coordinator.
    //
    // Enables or disables the secondary receiver (RX2) without disconnecting.
    // The sequence mirrors setSampleRateLive() (Task 1.6):
    //   1. Quiesce the DSP worker (stop I/Q feed into RxDspWorker).
    //   2. Pause AudioEngine.
    //   3. Create/destroy WDSP RX channels to match the new count.
    //   4. Update ReceiverManager DDC mapping (activate/deactivate receivers).
    //   5. Update the hardware:
    //      - P1: update m_activeRxCount in P1RadioConnection so the next
    //            bank-0 C&C frame encodes the correct nrx bits, then issue
    //            a stop+prime+start cycle so the radio re-arms EP6 with the
    //            new per-frame slot count.  The static parseEp6Frame already
    //            accepts numRx as a parameter; m_activeRxCount in the instance
    //            is used on every parse call, so updating it is sufficient —
    //            no MetisFrameParser rework required (MetisFrameParser does not
    //            exist as a separate class; parsing is in P1RadioConnection).
    //      - P2: setActiveReceiverCount() already sends sendCmdRx() when
    //            running, which updates DDC enable bits in the hardware.
    //   6. Reconnect DSP worker I/Q feed (resume DSP worker).
    //   7. Resume AudioEngine.
    //   8. Persist the new count per-MAC, update m_connectionActiveRxCount, and
    //      emit activeRxCountChanged(newCount).
    //
    // Returns elapsed milliseconds.  Returns -1 if no connection is active or
    // WDSP is not initialized.  Returns 0 if newCount == current count
    // (idempotent).
    //
    // Must be called on the main thread.
    //
    // Note on P1 MetisFrameParser: the plan (design §5D) flagged a potential
    // need to rework MetisFrameParser to handle mid-stream RX-count changes.
    // Investigation found that no separate MetisFrameParser class exists —
    // EP6 parsing is in P1RadioConnection::parseEp6Frame(frame, numRx, ...)
    // which accepts numRx as a parameter on every call and reads
    // m_activeRxCount from the instance.  There is no per-receiver cache to
    // invalidate.  Strategy A (full live-apply, both protocols) is therefore
    // possible without any parser rework.
    qint64 setActiveRxCountLive(int newCount);

    // Returns the active-RX count last pushed to hardware (0 when disconnected).
    int connectionActiveRxCount() const { return m_connectionActiveRxCount; }

    // Task 4.2 — Per-mode DSP-Options live-apply (called from DspOptionsPage).
    //
    // Reads the per-mode AppSettings keys for forMode and calls
    // RxChannel::onModeChanged() (+ TxChannel::onModeChanged()) if WDSP is
    // initialized and a channel exists.  No-op if disconnected or uninitialized.
    //
    // Emits dspChangeMeasured(elapsedMs) if a WDSP channel rebuild occurred.
    //
    // DspOptionsPage calls this when a combo changes and the combo's mode
    // matches the current slice mode (design Section 4B: live-apply if same
    // mode, persist-only otherwise — applies on next mode-switch).
    //
    // Must be called on the main thread.
    void rebuildDspOptionsForMode(DSPMode forMode);

    // R-R3-21: a remote window's accepted DSP > Options RX write.
    //
    // The Core's StationServer calls this after it accepts a settings write
    // or remove of a key from a remote window. RX per-mode keys
    // (DspOptions{BufferSize,FilterSize,FilterType}{Phone,Cw,Dig,Fm}Rx)
    // queue their mode group; every other key is ignored. After
    // kDspOptionsApplyCoalesceMs the queued groups are applied once: each
    // slice whose current mode is in a queued group re-runs the mode-change
    // apply (RxChannel::onModeChanged) on its own channel, so a buffer or
    // filter change takes effect without a mode change. A burst of keys
    // yields one apply per slice.
    //
    // R-R3-49 (parity Task 1): TX per-mode keys
    // (DspOptions{BufferSize,FilterSize,FilterType}{Phone,Dig,Fm}Tx) queue
    // their group too, and when the TX-bound slice's mode is in it the TX
    // channel re-runs its mode-change apply (TxChannel::onModeChanged), the
    // local page's own TX apply (rebuildDspOptionsForMode). It sets the TX
    // channel's buffer and filter only; it never keys.
    //
    // Local operation never calls this: DspOptionsPage applies its own
    // edits through rebuildDspOptionsForMode. No-op on a remote-role model.
    // Must be called on the main thread.
    void scheduleRemoteDspOptionsApply(const QString& key);

    // R-R3-46: the Core's hardware apply step. The Core's StationServer
    // calls this after it accepts a settings write or remove of a key from
    // a remote window, beside scheduleRemoteDspOptionsApply. Keys under
    // hardware/<connected MAC>/ that a live controller holds in memory
    // queue that controller's reload: oc/ (the OC pin matrix the codec
    // reads each frame), hl2IoBoard/n2adrFilter (the HL2 N2ADR filter
    // board's pins in that matrix), cal/ (the frequency calibration the P2
    // codec reads each command) and hl2/ (the HL2 options). After
    // kHardwareApplyCoalesceMs the queued reloads run once, so the Core's
    // own copy follows the saved settings and its later saves keep them.
    // Every other key is ignored. No-op on a remote-role model. Must be
    // called on the main thread.
    void scheduleRemoteHardwareApply(const QString& key);

    // Plan Task 14 and its fix wave (R-R3-49): the Alex tab's high-pass
    // switches (Setup > Hardware > Alex, hardware/<mac>/alex/master/...)
    // reach the Core's own radio. Reads the saved values for the connected
    // radio and hands them to the connection. Called on connect, by the Alex
    // tab after it saves one, and by scheduleRemoteHardwareApply when a
    // remote window's change arrives. A model with no radio of its own (a
    // remote window) does nothing: its save goes to the Core, which applies
    // it there.
    void applyAlexHpfSwitchSettings();
    // The Alex tab's receive filter rows saved for `mac` (the high-pass
    // ladder, the band-pass bank and the Alex-2 bank: each row's edges and
    // bypass, and the Alex-2 master bypass), each default Thetis's.
    static codec::alex::AlexHpfEdges savedAlexHpfEdges(const QString& mac);
    const codec::alex::AlexHpfEdges& alexHpfEdges() const noexcept { return m_alexHpfEdges; }
    // The Alex tab's low-pass filter rows saved for `mac` (each row's start
    // and end), each default Thetis's.
    static codec::alex::AlexLpfEdges savedAlexLpfEdges(const QString& mac);
    const codec::alex::AlexLpfEdges& alexLpfEdges() const noexcept { return m_alexLpfEdges; }
    // Task 14's name for the same apply, kept for its callers.
    void applyHpfBypassOnTxSetting() { applyAlexHpfSwitchSettings(); }

    // Test-only: observe each reload the coalesced hardware apply makes,
    // by name ("oc", "n2adr", "cal", "hl2").
    void setHardwareApplyObserverForTest(std::function<void(const QString&)> observer)
    {
        m_hardwareApplyObserverForTest = std::move(observer);
    }

    // Test-only: observe each slice apply the coalesced flush makes, as
    // (slice index, the slice's mode). Called before the RxChannel apply,
    // so it reports the target even when no WDSP channel exists.
    void setDspOptionsApplyObserverForTest(std::function<void(int, DSPMode)> observer)
    {
        m_dspOptionsApplyObserverForTest = std::move(observer);
    }
    // Test-only: observe each TX apply the coalesced flush makes, with the
    // TX-bound slice's mode. Called before the TxChannel apply, so it
    // reports the target even when no TX channel exists.
    void setDspOptionsTxApplyObserverForTest(std::function<void(DSPMode)> observer)
    {
        m_dspOptionsTxApplyObserverForTest = std::move(observer);
    }
    // R-IOS-13 test-only: observe each key's TX DSP options apply
    // (applyTxDspOptionsBeforeKey), with the TX-bound slice's mode, called
    // after the TxChannel apply.
    void setTxKeyDspOptionsObserverForTest(std::function<void(DSPMode)> observer)
    {
        m_txKeyDspOptionsObserverForTest = std::move(observer);
    }

    // Phase 3Q Sub-PR-4 D.3: Hover tooltip for the TitleBar ConnectionSegment.
    // Returns a multi-line string with radio name, uptime, IP, MAC, protocol,
    // firmware, sample rate, and live throughput. Disconnected state returns a
    // short invitation to connect. Owned by RadioModel so the segment stays a
    // thin presentation layer.
    QString buildConnectionTooltip() const;

    // Phase 3Q-1: single source of truth for the connection lifecycle state.
    // UI components (TitleBar, ConnectionPanel, status bar, spectrum overlay)
    // read this instead of deriving state from RadioConnection directly.
    ConnectionState connectionState() const { return m_connectionState; }
    // Nereus session ownership: connect may pump a nested wisdom event loop.
    // Its owner must keep this model alive until the synchronous call returns.
    bool localConnectionSetupActive() const { return m_localConnectionSetupActive; }

    // Test-only: allow tests to drive transitions without standing up
    // a fake RadioConnection. Production transitions go through the
    // private setConnectionState() called from connection signals.
    void setConnectionStateForTest(ConnectionState s) { setConnectionState(s); }

    // Test-only: walk the FULL Connected/Disconnected handler so tests
    // can drive the peripherals lifecycle (applyPeripheralsForCurrentMac
    // / teardownPeripherals) without standing up a RadioConnection.
    // Mirrors the production signal-driven path -- equivalent to wiring
    // a fake RadioConnection and emitting its connectionStateChanged
    // signal, but without the QObject overhead.
    void onConnectionStateChangedForTest(ConnectionState s) {
        onConnectionStateChanged(s);
    }

    // Test-only: targeted seams for the peripherals lifecycle.  Tests
    // that don't want the full onConnectionStateChanged side-effects
    // (settings-hygiene validation, currentRadioChanged emit, etc.)
    // can drive applyPeripheralsForCurrentMac / teardownPeripherals
    // directly after setting the connection state via
    // setConnectionStateForTest + setLastRadioInfoForTest.
    void applyPeripheralsForTest() { applyPeripheralsForCurrentMac(); }
    // The first-MOX audioVolume seed, which connectToRadio runs only with a
    // live WDSP TxChannel.
    void seedInitialAudioVolumeForTest() { seedInitialAudioVolume(); }
    void teardownPeripheralsForTest() { teardownPeripherals(); }
    // R-R3-22: the FlexRadio beacon as connectToRadio leaves it, in a mode
    // that sends nothing (FlexRadioDiscoveryBroadcaster::setNoSendForTesting),
    // and whether it runs.
    void configureFlexBeaconForTest();
    bool flexBeaconRunningForTest() const;
    class FlexRadioDiscoveryBroadcaster* flexBroadcasterForTest() const { return m_flexBroadcaster; }

#ifdef NEREUS_BUILD_TESTS
public:
    friend class TstRadioModelKeyingReconnect;
    // R-R3-16: the radio name a local model learns on connect
    // (m_name = info.displayName()). MainWindow's automatic Connections
    // open on a Disconnected state keys on a non-empty name, and a test has
    // no other way to give a local model one without a live connection.
    void setNameForTest(const QString& name) { m_name = name; }
    // RADE end-of-over callsigns: a TX worker without a started pump (the
    // test drives tickForTest), wired as the connect path wires its own.
    void installTxWorkerForTest(std::unique_ptr<TxWorkerThread> worker);
    TxWorkerThread* txWorkerMutableForTest() const { return m_txWorker.get(); }
    // The 24 -> 48 kHz RADE TX resampler (null until the first modem block).
    const Resampler* radeTxResamplerForTest() const { return m_radeTxResampler.get(); }
    // The FreeDV reporter bridge fed by radeSyncChanged / radeSnrChanged.
    FreeDVRadeReporterBridge* radeReporterBridgeForTest() const { return m_radeReporterBridge.get(); }

    // Test-only: inject board caps without a live radio connection.
    // Mirrors P1RadioConnection::setBoardForTest pattern.
    void runAutoAgcTickForTest() { updateAutoAgc(); }
    void setBoardForTest(HPSDRHW board) {
        // Plan Task 15: profileForRadio, as connectToRadio builds it.
        m_hardwareProfile = ::NereusSDR::profileForRadio(
            board, defaultModelForBoard(board));
        m_calController.setHardwareModel(m_hardwareProfile.model);
        applyRxOnly();   // Task 16: the kit runs receive only
    }

    // Phase 3P-I-a T14 — test-only hooks. Allow tests to inject a mock
    // RadioConnection, simulate band crossings, trigger the Connected
    // state handler, and override board capabilities. Production code
    // must never use these.
    // Remote-daemon R2 Task 3: also drive m_connectionState, not just the
    // pointer. isConnected() is storage-backed as of this task, and this
    // seam does no signal wiring at all (no connectToRadio(), so
    // connectionStateChanged never reaches onConnectionStateChanged), so a
    // plain pointer assignment would leave m_connectionState stuck at
    // Disconnected regardless of what the injected mock's own
    // isConnected() reports. 24 test files across the suite rely on this
    // seam standing in for a real connection; two of them (tst_p2_ddc_
    // assignment_marshalling.cpp, tst_p2_ddc_mask_ownership.cpp) drive the
    // isConnected() gate on the P2 DDC wire push (RadioModel.cpp:15412)
    // through it, so keeping the two in lockstep here is what keeps those
    // 24 sites working rather than just the pointer-consuming ones.
    void wireWidebandConnectionForTest() { wireWidebandConnection(); }
    // The production ReceiverManager -> connection pushes, for a test that
    // injects a connection (injectConnectionForTest does no wiring).
    void wireReceiverManagerHardwarePushesForTest() { wireReceiverManagerHardwarePushes(); }
    void injectConnectionForTest(RadioConnection* conn) {
        m_connection = conn;
        setConnectionState(conn != nullptr ? ConnectionState::Connected
                                            : ConnectionState::Disconnected);
    }
    // Install a real PureSignal coordinator without the full WDSP/connect
    // pipeline so codec-context tests can distinguish the auto-cal preference
    // from the cmd-state machine's effective PSEnabled state. RadioModel owns
    // the returned coordinator, matching production lifetime.
    PureSignal* installPureSignalForTest(TxChannel* tx);
    // Group B fix wave: the unkey wiring connectToRadio makes, against the
    // channel wireTransmitChainForTest set, and an observer called where
    // PureSignal hears the radio is going back to receive (the TX drain's
    // request, since Task 33's Thetis unkey order: wireTxChannelKeying).
    void wireTxaFlushedForTest() { wireTxChannelKeying(); }
    void setTxaFlushedPureSignalObserverForTest(std::function<void()> observer)
    {
        m_txaFlushedPureSignalObserverForTest = std::move(observer);
    }
    // Phase 3F Sub-Epic I closeout, defect F1: attach a DSP worker without
    // standing up the connection / DSP-thread pipeline, so a test can
    // reproduce connectToRadio's real ordering (pool sized and slices bound
    // FIRST, worker constructed second) and assert the bindings still reach
    // it. Non-owning, exactly like the production m_dspWorker.
    void attachDspWorkerForTest(RxDspWorker* w);
    // The same, with `thread` registered as the worker's running thread the
    // way the connect path registers m_dspThread, so the receive lane
    // quiesces the worker from the lane (runOnDspWorkerFromLane) exactly as
    // it does in production. Pass nullptrs to detach.
    void attachDspWorkerOnThreadForTest(RxDspWorker* w, QThread* thread);
    // R-R3-39: waits until every job on the receive lane has run and then
    // delivers the answers they queued for this thread. False on timeout.
    bool waitForReceiveLaneForTest(int timeoutMs = 600000);
    // R-R3-49: republishes every slice's audio view (publishSliceAudioView).
    void publishSliceAudioViewForTest() { publishSliceAudioView(); }
    // R-R3-39: the same for the transmit lane.
    bool waitForTransmitLaneForTest(int timeoutMs = 600000);
    // R-R3-39: wires MoxController's txReady and txaFlushed to an injected TX
    // channel (injectTxChannelForTest) exactly as the connect path does.
    void wireTxChannelKeyingForTest() { wireTxChannelKeying(); }
    // Phase 3F Sub-Epic I closeout, defect F3: force the radio-state inputs
    // the codec branches on, so the PureSignal and diversity branches are
    // reachable without standing up a connection, a WDSP engine and a
    // PureSignal coordinator. Sticky once set; production code must never
    // call this.
    // Isolate route-lifecycle tests from board/resource admission. Never a production caller.
    void setDiversityAdmissionBypassForTest() { m_diversityAdmissionBypassForTest = true; }
    void setDdcContextForTest(bool mox, bool puresignalRun, bool diversity) {
        m_ddcCtxForTest    = true;
        m_ddcCtxMoxForTest = mox;
        m_ddcCtxPsForTest  = puresignalRun;
        m_ddcCtxDivForTest = diversity;
    }
    // B6 — XIT: allow tests to trigger wireSliceSignals() directly after
    // injecting a mock connection, mirroring what wireConnectionSignals() does
    // when a real radio connects.
    void wireSliceSignalsForTest() { wireSliceSignals(m_activeSlice); }
    // iPhone app Task 73: the frequency the FreeDV Reporter lists (the
    // station-level active slice's), whether or not it is connected.
    quint64 freedvWantedFrequencyHzForTest() const { return m_freedvWantedHz; }
    // The transmit-frequency derivation the push and both TUNE arms share.
    // setTune() itself is unreachable from a unit test (it requires a live
    // connection AND an audio engine, console.cs:30035-30043 [v2.10.3.15]'s
    // PowerOn guard), so the shared derivation is what gets pinned.
    quint64 txFrequencyForSliceForTest(const SliceModel* s) const {
        return txFrequencyForSlice(s);
    }
    void installBandPlanMoxCheckForTest() { installBandPlanMoxCheck(); }
    // Issue #182 — invoke the mic_ptt_disabled wiring helper directly so
    // tst_radio_model_mic_ptt_wire can verify the signal/slot bind + prime
    // path without spinning up the full wireConnectionSignals pipeline.
    void wireMicPttDisabledForTest() { connectMicPttDisabledSignal(); }
    // Radio codec lane: the mic codec wiring alone, for an injected connection.
    void wireMicCodecForTest() { connectMicCodecSignals(); }
    void wireRadioSpeakerOutputForTest() { connectRadioSpeakerOutput(); }
    void unwireRadioSpeakerOutputForTest() { disconnectRadioSpeakerOutput(); }
    // Task 13: wire the injected connection's user digital inputs to the
    // TX inhibit monitor, and undo it, without the full connect pipeline.
    void wireTxInhibitInputForTest() { connectTxInhibitInput(); }
    void teardownTxInhibitInputForTest() { m_txInhibit.detachRadioInput(); }
    /// iPhone app Task 75: band tracking (the per-band antenna switch) for
    /// every slice, on a model with no connection; see crossBandForSlice.
    void enableBandTrackingForTest();
    /// The band whose receive antenna is kept on the relay, if any.
    std::optional<NereusSDR::Band> keptReceiveAntennaBandForTest() const {
        return m_keptRxAntennaBand;
    }
    void setLastBandForTest(NereusSDR::Band b) {
        const bool cross = (b != m_lastBand);
        m_lastBand = b;
        if (cross) {
            applyAlexAntennaForBand(b);
            // Mirror the production T10 path so tests catch regressions
            // in the slice-label refresh (see RadioModel.cpp frequencyChanged
            // handler for the canonical version).
            //
            // Issue #257: production now passes the SkuUiProfile so the
            // RX-only label slot wins over the ANT* default. Mirror that
            // here so band-cross tests exercise the same path.
            if (m_activeSlice) {
                const NereusSDR::SkuUiProfile sku =
                    NereusSDR::skuUiProfileFor(m_hardwareProfile.model);
                m_activeSlice->refreshAntennasFromAlex(m_alexController, b, &sku);
            }
        }
    }
    void onConnectedForTest() {
        applyAlexAntennaForBand(m_lastBand);
    }
    // Phase 3P-I-b (T6): expose with isTx parameter for composition tests.
    void applyAlexAntennaForBandForTest(NereusSDR::Band band, bool isTx) {
        applyAlexAntennaForBand(band, isTx);
    }
    void setCapsForTest(bool hasAlex) {
        m_testCapsOverride  = true;
        m_testCapsHasAlex   = hasAlex;
        m_testCapsIsRxOnly  = false;  // reset sibling so combined state is unambiguous
    }
    // 3M-1a G.2: inject isRxOnlySku without a live HermesLiteRxOnly board.
    // (HermesLiteRxOnly has no HPSDRModel entry so setBoardForTest cannot
    // reach its caps via the normal profileForModel path.)
    // Resets m_testCapsHasAlex so chaining setCapsForTest + setCapsRxOnlyForTest
    // in the same fixture does not silently combine both flags.
    void setCapsRxOnlyForTest(bool isRxOnly) {
        m_testCapsOverride  = true;
        m_testCapsHasAlex   = false;  // reset sibling so combined state is unambiguous
        m_testCapsIsRxOnly  = isRxOnly;
        applyRxOnly();   // Task 16: as a connect to that board would
    }
    // 3M-1b I.1: inject hasMicJack without a live radio board.
    // HL2 sets hasMicJack=false; all other boards set true (default).
    // Does not reset other test-cap flags — compose with setCapsRxOnlyForTest
    // if a combined cap override is needed (each flag is independent).
    void setCapsHasMicJackForTest(bool hasMicJack) {
        m_testCapsOverride     = true;
        m_testCapsHasMicJack   = hasMicJack;
    }
    // Issue #177 — drive the tune-off settle delay synchronously in tests.
    // Production default is 100 ms (mirrors `await Task.Delay(100)` at Thetis
    // console.cs:30107 [v2.10.3.13]).  Setting this to 0 makes completeTuneOff
    // schedule on the next event loop iteration, so QCoreApplication::processEvents
    // can drive the deferred completion synchronously.
    void setTuneOffSettleMsForTest(int ms) noexcept { m_tuneOffSettleMs = ms; }
    bool tuneOffPendingForTest()          const noexcept { return m_pendingTuneOff; }

    // 3M-1b I.3: inject HPSDRHW board type to select the per-family Radio Mic
    // group box in AudioTxInputPage without a live radio connection.
    // Does not reset other test-cap flags — independent of hasMicJack.
    void setCapsHwForTest(HPSDRHW hw) {
        m_testCapsOverride = true;
        m_testCapsHw       = hw;
    }
    // Emit currentRadioChanged with a default-constructed RadioInfo for test use.
    // Use this to simulate a reconnect when testing signal-driven visibility updates.
    void emitCurrentRadioChangedForTest() {
        emit currentRadioChanged(NereusSDR::RadioInfo{});
    }
    NereusSDR::Band lastBand() const { return m_lastBand; }

    // Phase 3F: expose the codec's input array so tests can assert what the
    // per-board codec is actually handed (SliceConfig::txBound in
    // particular, which is the OR of SliceModel::isTxSlice across the slices
    // sharing a DDC stream). Production callers reach the same builder
    // through requestDdcAssignment.
    std::array<NereusSDR::SliceConfig, 5> buildStreamConfigsForCodecForTest() const {
        return buildStreamConfigsForCodec();
    }

    // Companion to the above for the other half of the codec's inputs.
    // Added with the D2 fix (CodecContext::adcCtrl was never seeded on
    // Protocol 2), so a test can assert the seed without standing up a
    // connection to observe it through a wire frame.
    NereusSDR::CodecContext currentCodecContextForTest() const {
        return currentCodecContext();
    }

    // Publish a hand-built assignment through the real production path.
    //
    // Added with the D1 fix. The per-stream ADC now reaches the model in
    // exactly one way: a codec composes a DdcAssignment and this function
    // decodes its adcCtrl bytes. A test that wants a slice on chain 1 has to
    // go through here or it is seeding a field nothing reads, which is the
    // defect D1 was.
    //
    // Deliberately the whole function rather than a setter for m_streamAdc.
    // A narrow ADC setter could drift from what publishDdcAssignment
    // actually writes and the suite would never notice; routing through the
    // real body means the decode (NereusSDR::adcForDdc) is under test too.
    //
    // For tests that want the codec's own answer instead of a hand-built
    // one, inject a codec and use requestDdcAssignment: that is the fuller
    // path and it is what tst_alex_per_adc_bpf_wire drives.
    void publishDdcAssignmentForTest(const NereusSDR::DdcAssignment& a) {
        publishDdcAssignment(a);
    }

    // Per-radio peripherals scope: tests pin m_lastRadioInfo without
    // standing up a fake RadioConnection so peripheralValue / setPeripheralValue
    // can resolve their per-MAC scope.  Production code populates this via
    // the connection-thread handshake.
    void setLastRadioInfoForTest(const NereusSDR::RadioInfo& info) {
        m_lastRadioInfo = info;
    }

    // Codex review round 7, PR #293 — drive the I/Q tap fork directly.
    // Production reaches it from the iqDataForReceiver lambda installed in
    // wireConnectionSignals, which needs DSP threads, an RxDspWorker and a
    // live connection. Same on*ForTest pattern as handlePaTelemetryForTest.
    void forkIqToTapsForTest(int receiverIndex, const QVector<float>& samples) {
        forkIqToTaps(receiverIndex, samples);
    }

    // P1 full-parity §3.4 test hook — invoke the per-sample PA telemetry
    // handler directly without spinning up the full wireConnectionSignals
    // pipeline (which constructs DSP threads and the RxDspWorker).  Mirrors
    // the existing on*ForTest pattern (setConnectionStateForTest /
    // onConnectedForTest / setLastBandForTest).  Production code reaches
    // the same handler via the lambda installed in wireConnectionSignals.
    // R-R3-49: Calibration's Volts/Amps log, to point it at a test file.
    VoltsAmpsLog* voltsAmpsLogForTest() const { return m_voltsAmpsLog; }
    void handlePaTelemetryForTest(quint16 fwdRaw, quint16 revRaw,
                                  quint16 exciterRaw, quint16 userAdc0Raw,
                                  quint16 userAdc1Raw, quint16 supplyRaw) {
        // The test seam injects telemetry as if the radio were
        // transmitting — bypass the MOX gate that handlePaTelemetry
        // applies in production (which forces TX-domain readings to 0
        // when MoxController state != Tx so late samples don't refill
        // the meters after un-key).  Tests calling this seam mean
        // "behave as if in TX"; flipping the flag lets the routing
        // pipeline run exactly as it would on a live transmit sample.
        m_forceTxForTest = true;
        handlePaTelemetry(fwdRaw, revRaw, exciterRaw,
                          userAdc0Raw, userAdc1Raw, supplyRaw);
        m_forceTxForTest = false;
    }

    // P1 full-parity §3.5 test seam — pure-function counterpart of the
    // percent-to-wire-byte SWR-foldback formula inlined at every
    // setTxDrive call site (voice powerChanged lambda, TUNE-engage,
    // TUNE-restore).  Tests assert against this helper to verify the
    // formula in isolation; production callsites use the same three-line
    // expression (see RadioModel.cpp).  A regression in the helper is a
    // regression in the inlined production code by construction.
    //
    // Source: mi0bot NetworkIO.cs:209-211 [v2.10.3.14-beta1]
    //   int i = (int)(255 * f * _swr_protect);   // f normalised 0..1,
    //                                            // _swr_protect ≤ 1.0
    static int computeWireDriveForTest(int powerPct, float swrProtectFactor) {
        const float f          = std::clamp(powerPct / 100.0f, 0.0f, 1.0f);
        const float swrProtect = std::clamp(swrProtectFactor, 0.0f, 1.0f);
        return std::clamp(int(255.0f * f * swrProtect), 0, 255);
    }

    // 3M-1b L.1 test seams: expose raw pointers into the mic-source strategy
    // objects so ownership, threading, and lifecycle tests can inspect state
    // without coupling to production API surfaces.
    // All three return nullptr before the first connectToRadio() / after
    // teardownConnection() — exactly the lifecycle the tests verify.
    const PcMicSource*           pcMicSourceForTest()          const { return m_pcMicSource.get(); }
    const RadioMicSource*        radioMicSourceForTest()        const { return m_radioMicSource.get(); }
    const CompositeTxMicRouter*  compositeMicRouterForTest()   const { return m_compositeMicRouter.get(); }

    // 3M-1c TX pump architecture redesign test seam: returns the unique_ptr's
    // raw pointer to the TxWorkerThread.  Returns nullptr before the first
    // connectToRadio() (m_txWorker is constructed inside the WDSP-init lambda
    // once m_audioEngine and m_txChannel are both live) and after
    // teardownConnection() resets it.  Used by tst_radio_model_3m1b_ownership
    // to verify worker construction/destruction follows the documented
    // lifecycle.
    // Test-only accessor — do not use in production code.
    const TxWorkerThread* txWorkerForTest() const { return m_txWorker.get(); }
    // Phase 3M-1c TX pump v3 — TxMicSource is constructed alongside
    // TxWorkerThread; allow tests to verify the pre/post-connect ownership.
    const class TxMicSource* txMicSourceForTest() const { return m_txMicSource.get(); }

    // 3M-1b L.3 test seam: simulate connectToRadio()'s loadFromSettings +
    // HL2 force-Pc sequence without a live radio connection.
    // Call setCapsHasMicJackForTest(bool) first to inject the board caps,
    // then call this to run the exact same two-step sequence as
    // connectToRadio(): loadFromSettings(mac) → setMicSourceLocked(!hasMicJack).
    // After this call, transmitModel().micSource() and isMicSourceLocked()
    // reflect the HL2 (or non-HL2) post-connect state.
    void simulateConnectLoadForTest(const QString& mac) {
        m_transmitModel.loadFromSettings(mac);
        m_transmitModel.setMicSourceLocked(!boardCapabilities().radioMicSelectable());
    }

    // Release the lock, mirroring teardownConnection()'s setMicSourceLocked(false).
    // Use between simulated reconnects in the same test.
    void simulateDisconnectForTest() {
        m_transmitModel.setMicSourceLocked(false);
    }

    // Phase 4 Agent 4A of issue #167 — test seam to inject a non-owning
    // TxChannel pointer so the drive-slider / TUNE rewrite tests can spy on
    // setTxFixedGain() without standing up the full WdspEngine pipeline.
    // Production code never calls this — m_txChannel is wired by the
    // WDSP-init lambda inside connectToRadio() (see "createTxChannel(kTxChannelId)"
    // around RadioModel.cpp:1514).
    void injectTxChannelForTest(class TxChannel* ch) { m_txChannel = ch; }
    void bindTxEqProfileChannelForTest(TxChannel* ch) { bindTxEqProfileChannel(ch); }
    void bindCfcProfileChannelForTest(TxChannel* ch) { bindCfcProfileChannel(ch); }
    void replayCfcProfileForTest() { replayCfcProfile(); }

    // R-R3-49 (parity Task 2): inject `channel` and run the Core's transmit
    // chain wiring (TransmitModel to TxChannel, MON to the audio engine)
    // that connectToRadio() runs once WDSP is up, so a test can check a
    // setting reached the TX channel's own state. No WDSP channel, no RF.
    void wireTransmitChainForTest(class TxChannel* channel);

    // Phase 4 Agent 4A of issue #167 — test seam to inject the HPSDRModel
    // hardware profile directly. setBoardForTest(HPSDRHW::OrionMKII) maps
    // through defaultModelForBoard() to ORIONMKII (the *first* model
    // matching that board), but K2GX's regression specifically pins
    // ANAN8000D values; this seam lets tests pick the exact HPSDRModel.
    //
    // v0.4.1 hotfix: routes through applyHpsdrModel() so tests get the
    // same TransmitModel + ReceiverManager fan-out as production
    // connectToRadio.  Without this, the test seam drifts from
    // production and tests miss regressions in the ReceiverManager
    // push (root cause of the v0.4.0 PureSignal-broken-on-Hermes bug).
    void setHpsdrModelForTest(HPSDRModel m) {
        applyHpsdrModel(m);
    }

    // Stand in an exact capability-table row (for invariants over every row
    // of BoardCapsTable::all(), including rows no HPSDRModel resolves to).
    void setBoardRowForTest(const BoardCapabilities& caps) {
        m_testWidebandCaps = caps;
        reconcileWidebandDemand();
    }

    // Synthetic routing topology for ADC-versus-filter-chain regressions.
    // Copies the selected profile; it does not change any production SKU.
    void setWidebandTopologyForTest(int adcCount, int widebandAdcs, int chains) {
        m_testWidebandCaps = boardCapabilities();
        m_testWidebandCaps->adcCount = adcCount;
        m_testWidebandCaps->widebandAdcs = widebandAdcs;
        m_testWidebandCaps->rxFilterChainCount = chains;
        reconcileWidebandDemand();
    }

    // Remote-daemon R2 Task 5 test seam: sizes m_streamAllocator via the
    // same body configureStreamPool uses, bypassing that method's
    // Role::Remote guard (see configureStreamPool's definition). A test
    // proving bindSliceToStream's OWN role guard blocks a mutation needs
    // a sized-but-Remote pool to do it against -- an unsized pool already
    // makes bindSliceToStream return false through the pre-existing
    // "pool not sized yet" path, which would pass even with no Task 5
    // guard at all. See task-5-controller-notes.md.
    //
    // Review fix round 1, finding 1: this MUST stay inside the
    // NEREUS_BUILD_TESTS block and never be reachable from a production
    // build. Sizing a Role::Remote model's pool is exactly the state that
    // makes bindSliceToStream's guard destructive rather than merely
    // inert -- see that guard's comment in RadioModel.cpp for the full
    // mechanism. When this method lived in the always-compiled public
    // section, it was one accidental call site away from putting a
    // shipping GUI binary into that state.
    void configureStreamPoolForTest(int userDdcCount, int maxSlices,
                                    int defaultRateHz) {
        configureStreamPoolImpl(userDdcCount, maxSlices, defaultRateHz);
    }
#endif

    // TUN state, exported for H.3 UI polling and for issue #177 tests.
    // True between setTune(true) and the completion of the corresponding
    // setTune(false) → completeTuneOff() chain.
    // Cite: Thetis console.cs:30010 [v2.10.3.13] — _tuning = true (read by
    // many UI/meter/PA paths in console.cs).
    //
    // Lives OUTSIDE the NEREUS_BUILD_TESTS block because production code
    // (TunerApplet::onTuneClicked at TunerApplet.cpp:183) uses it to gate
    // local-tune carrier engage on hardware-TUNE entry.  Prior placement
    // inside the test block compiled green on Linux (-DNEREUS_BUILD_TESTS=ON
    // in CI) but broke macOS / Windows where the option defaults OFF.
    bool isTune() const noexcept { return m_isTuning; }

    // Connection
    void connectToRadio(const RadioInfo& info);
    // Same selected radio, retaining live receiver state and active selection.
    void connectToRadioPreservingSlices(const RadioInfo& info);
    // The operator's disconnect (and an application shutdown). It also lifts
    // the lost-link key lock, even when the connection is already gone.
    void disconnectFromRadio();
    // TX safety fix round 2 (2026-09-30): automatic recovery's retire (the
    // Core's retire-and-reconnect, the hosted GUI's retry). Same teardown as
    // disconnectFromRadio, but a lost-link key lock stays set through the
    // Disconnected wait and the rebuilt link's Connecting and Probing; it
    // lifts when a link reaches Connected.
    void retireConnectionForRecovery();
#ifdef NEREUS_BUILD_TESTS
    // Instance-local loopback transport setup, applied before the connection
    // moves to its worker. No global port overrides or production callers.
    void configureP2TransportForTest(quint16 outboundBase, quint16 inputRoleBase,
                                     int firstIqMs, int establishedMs) {
        m_testP2OutboundBase = outboundBase;
        m_testP2InputBase = inputRoleBase;
        m_testP2FirstIqMs = firstIqMs;
        m_testP2EstablishedMs = establishedMs;
    }
    // R-R3-49 (parity Task 8): a shorter Tuner Genius LAN scan window, so
    // a test does not wait the dialog's three seconds.
    void setTgxlLanScanWindowMsForTest(int ms) { m_tgxlLanScanWindowMs = ms; }
    // R-R3-49 (parity Task 9): the same for the Power Genius scan.
    void setPgxlLanScanWindowMsForTest(int ms) { m_pgxlLanScanWindowMs = ms; }
#endif

    // Phase 3Q Task 10: arm / disarm the auto-connect-in-progress flag.
    // Called by MainWindow::tryAutoReconnect() before and after the probe.
    // When armed, RadioModel::wireConnectionSignals wires RadioConnection::connectFailed
    // to emit autoConnectFailed(mac, reason) and then disarms automatically.
    void setAutoConnectInProgress(bool inProgress, const QString& chosenMac = {}) {
        m_autoConnectInProgress = inProgress;
        m_autoConnectChosenMac  = inProgress ? chosenMac : QString{};
    }

    // Phase 3Q Task 10: called by MainWindow when multiple saved radios have
    // autoConnect = true. Emits autoConnectAmbiguous so the MainWindow lambda
    // can post the status-bar warning without the caller reaching into our signals.
    void notifyAutoConnectAmbiguous(int count, const QString& chosenMac) {
        emit autoConnectAmbiguous(count, chosenMac);
    }

    // ── Phase 3M-0 Task 6: Ganymede PA-trip live state ───────────────────────
    // G8NJJ: handlers for Ganymede 500W PA protection
    // From Thetis Andromeda/Andromeda.cs:914-948 [v2.10.3.13]
    // (CATHandleAmplifierTripMessage + GanymedeResetPressed).

    /// True iff a Ganymede PA trip is currently latched.
    /// From Thetis Andromeda/Andromeda.cs:914-920 [v2.10.3.13] (CATHandleAmplifierTripMessage).
    bool paTripped() const noexcept { return m_paTripped; }

    /// Apply a Ganymede CAT trip message. tripState != 0 latches the trip,
    /// 0 clears. As a safety side-effect, latching also drops MOX
    /// (Andromeda.cs:920 [v2.10.3.13]: `if (_ganymede_pa_issue && MOX) MOX = false`).
    void handleGanymedeTrip(int tripState);

    /// Clear the trip latch. Mirrors GanymedeResetPressed().
    /// Cite: Andromeda/Andromeda.cs (GanymedeResetPressed function) [v2.10.3.13].
    void resetGanymedePa();

    /// Setter for GanymedePresent capability. When set to false while a
    /// trip is latched, clears the trip (the radio no longer reports a PA).
    /// From Thetis Andromeda/Andromeda.cs:855-866 [v2.10.3.13] (GanymedePresent setter). //G8NJJ
    void setGanymedePresent(bool present);

public slots:
    /// Flush any coalesced notch edit immediately. Called when a notch drag
    /// ends so the committed position is exact rather than up to one
    /// coalescing window stale. See scheduleNotchEditPush.
    void commitPendingNotchEdits();

    /// The single creation route for every notch. Resolves the minimum
    /// realisable width from THE GIVEN SLICE's channel and clamps to it, so a
    /// notch is never stored or drawn at a width WDSP will silently widen.
    ///
    /// Codex review of PR #313 found three separate add routes with three
    /// different behaviours: the panadapter gesture clamped against
    /// activeSlice() rather than the pan that was clicked, and the +TNF button
    /// and the settings-page Add button did not clamp at all. Every route goes
    /// through here now, and `slice` is always the slice the operator acted
    /// on, per the standing rule that a control drawn on a pan targets that
    /// pan.
    ///
    /// Returns the new notch id, or -1 if the model refused it.
    ///
    /// R-R3-21: on a remote window whose NotchModel mirrors the Core, this
    /// sends notch.add for `slice` instead and returns -1; the Core clamps
    /// the width to its own receiver, and its id arrives with its list.
    int addNotchForSlice(SliceModel* slice, double centerHz, double widthHz);

    /// The +TNF button for `slice`: a notch of NotchModel::kDefaultNotchWidthHz
    /// at NotchModel::tnfAddCenterHz of the slice's demodulated frequency and
    /// filter (Thetis TNFAdd, console.cs:40313-40331 [v2.10.3.15]), through
    /// addNotchForSlice. The desktop's button and the Core's
    /// notch.addAtSlice both come here, so the centre is composed once.
    /// Returns what addNotchForSlice returns; -1 with no slice or no list.
    /// R-R3-21, R-IOS-27: on a remote window whose Core offers
    /// notchControlVersion 2 or more, it sends notch.addAtSlice for the
    /// slice instead (the Core's slice decides the centre) and returns -1.
    int addTnfForSlice(SliceModel* slice);
    /// The +TNF centre for `slice`, in Hz.
    static double tnfCentreHzFor(const SliceModel& slice);

public:
    // ── R-R3-21 / R-R3-09: the Core's notch commands ────────────────────────
    // A remote window changes the Core's list only through these, one notch
    // at a time, so no window can replace another's notches. Each returns
    // true when applied; otherwise `reason` is a plain refusal. The Core's
    // NotchModel fans every change out to every receiver as a local edit
    // does. Add refusals read as "Notch not added: <reason>." on the window,
    // so they carry no final period; the others are whole sentences.
    bool addNotchFromStation(int sliceId, double centreHz, double widthHz,
                             int* id, QString* reason);
    /// R-IOS-27, R-IOS-06: notch.addAtSlice, the desktop's +TNF for a
    /// device (addTnfForSlice on the Core's slice), with notch.add's
    /// refusals.
    bool addTnfFromStation(int sliceId, int* id, QString* reason);
    bool moveNotchFromStation(int id, double centreHz, double widthHz, QString* reason);
    bool setNotchActiveFromStation(int id, bool active, QString* reason);
    bool deleteNotchFromStation(int id, QString* reason);
    quint32 notchListRevision() const;

public slots:

    // ── Phase 3M-1a Task F.1: MoxController::hardwareFlipped fan-out ───────────
    // Slot connected to MoxController::hardwareFlipped(bool isTx).
    // Fans out hardware-flip side-effects to AlexController + RadioConnection
    // in Thetis HdwMOXChanged step order (pre-code review §2.3):
    //   1. applyAlexAntennaForBand(currentBand, isTx)  — §2.3 step 8
    //   2. m_connection->setMox(isTx)                  — §2.3 / §1.4 step 12
    //   3. m_connection->setTrxRelay(isTx)             — §2.3 step 10
    //
    // Must be under public slots: so Qt's auto-connection queues this correctly
    // when the emitting object (MoxController) lives on a different thread.
    // G.1's connect() call uses Qt::QueuedConnection so the slot body runs on
    // RadioModel's thread; steps 2+3 then marshal to the connection thread via
    // QMetaObject::invokeMethod (see implementation).
    void onMoxHardwareFlipped(bool isTx);

    // Task 33: MoxController::rxReady (after ptt_out_delay) turns the
    // receiver MOX stopped back on, as Thetis does after HdwMOXChanged and
    // ptt_out_delay (console.cs:29678-29680 [v2.10.3.15]).
    void onMoxRxReady();

    // iPhone app plan Task 38: Thetis's console.cs timeOutTimer, the
    // time-out's callback ("MOX" or "PING", with the limit that fired).
    void onTxTimeOut(const QString& which, int limitSeconds);

    // ── Task 33 (R-IOS-03, remote design §12.1): stopping transmission ─────
    //
    // stopTransmitNow: the emergency stop. Closes the TX channel's RF gate
    // and hands MOX off and the T/R relay off to the connection's thread
    // before it returns; it makes no WDSP call and waits for no lane. Until
    // the next key begins (MoxController::txAboutToBegin), no keying step
    // queued before it (a hardware flip, txReady, an interlock grant) can
    // key the radio again. It does not change the MOX, TUNE or two-tone
    // state: the caller clears those (stopAllTx does) and their normal
    // unkey then finishes on the lanes. It never waits for a RADE
    // end-of-over tail. `reason` goes to the log only.
    void stopTransmitNow(const QString& reason);

    // stopAllTx: ported from Thetis console.cs StopAllTx
    // (console.cs:45324-45342 [v2.10.3.15]). When MOX, manual MOX, TUNE or
    // two-tone is on: stopTransmitNow(message), then MOX, manual MOX, TUNE
    // and two-tone off, and transmitStopped(message) once. Otherwise it does
    // nothing. A held PTT then does not key again until it is released
    // (MoxController::latchStopAllTx).
    void stopAllTx(const QString& message = QString());

public:
    // ── iPhone app plan Task 38 (R-IOS-04, D29): the transmit time-out ──
    //
    // TxTimeOutTimer (Thetis TimeOutTimerManager) on a model with its own
    // radio (Local). The limit is the one for whoever is keyed now
    // (keyedBy().deviceKind):
    //   - the station and computers: Thetis's own MOX time-out (default
    //     off, 180 s) and ping time-out (default off, 180 s, 8.8.8.8);
    //   - phones and tablets: RemoteMoxTimeOutEnabled (default on) and
    //     RemoteMoxTimeOutSeconds (default 180), no ping time-out.
    // All seven keys are the Core's (Station scope) and are read at every
    // tick, so a change applies at once, counted from key-down. When it
    // fires, stopAllTx("MOX Time Out Timer") (or "PING ...") and the stop
    // reason timeOut with the limit.
    static constexpr bool kRemoteMoxTimeOutDefault = true;
    /// The limits for a key by a device of `deviceKind` ("phone",
    /// "tablet", "computer", "station" or empty), read from the settings.
    static TxTimeOutTimer::Settings txTimeOutSettingsFor(const QString& deviceKind);
    /// Whole seconds before the MOX time-out stops the transmission, or -1
    /// when no time-out applies (unkeyed, the limit off for this key, or a
    /// model without its own radio).
    int timeOutRemainingSeconds() const;
    /// The time-out itself (null on a remote window's model). Tests drive
    /// its clock and tick.
    TxTimeOutTimer* txTimeOutTimer() const { return m_txTimeOut; }

    /// Why the Core last stopped a transmission on its own. Task 38 sets
    /// code "timeOut" with `which` ("mox" or "ping") and the limit in
    /// seconds; the transmit state (Task 39) reads it.
    struct TransmitStopReason {
        QByteArray code;
        QByteArray which;
        int limitSeconds{-1};

        bool operator==(const TransmitStopReason& other) const = default;
    };
    TransmitStopReason lastTransmitStopReason() const { return m_lastTransmitStopReason; }

public slots:

    // ── Phase 3M-1a Task G.4: TUN function orchestrator ─────────────────────
    // Activate / release the TUNE function.
    //
    // Orchestrates all TUN side-effects across the model:
    //   TUN-on:  save DSP mode + power; swap CW→LSB/USB if needed;
    //            set tune tone; push tune power; drive MoxController.
    //   TUN-off: drive MoxController; release tone; restore DSP mode,
    //            power, and meter mode.
    //
    // Coordinates with:
    //   - MoxController::setTune(bool) for MOX state machine + flags.
    //   - TxChannel::setTuneTone(bool, freqHz, mag) for WDSP gen1 PostGen.
    //   - SliceModel::setDspMode() for CW→LSB/USB swap and restore.
    //   - TransmitModel::tunePowerForBand() + m_connection->setTxDrive()
    //     for per-band tune power push and restore.
    //
    // Power-on guard: emits tuneRefused(reason) and returns without any
    // state change if the radio is not connected (matching Thetis
    // console.cs:29983-29991 [v2.10.3.13] MessageBox "Power must be on").
    //
    // Meter mode save/restore: Thetis saves current_meter_tx_mode and
    // restores it on TUN-off (console.cs:30011-30015 [v2.10.3.13]).
    // NereusSDR's MeterModel does not yet expose a TX-mode selector (that
    // is H.3 territory); this method saves and restores m_transmitModel.power()
    // as the "slider power" position instead.  The full meter-mode lock
    // (switch to FORWARD_POWER display) is deferred to H.3 or 3M-1b when
    // MeterModel gains a setTxDisplayMode() setter.
    //
    // Inline attribution preserved from Thetis:
    //   //MW0LGE_21k9d  [original inline comment from console.cs:29980]
    //   //MW0LGE_21a    [original inline comment from console.cs:29997]
    //   //MW0LGE_22b    [original inline comment from console.cs:30033]
    //   //MW0LGE_21k8   [original inline comment from console.cs:30086]
    //   //MW0LGE_21j    [original inline comment from console.cs:30136]
    //
    // Cite: Thetis console.cs:29978-30157 [v2.10.3.13] — chkTUN_CheckedChanged.
    void setTune(bool on);
    // iPhone app plan Task 35 (R-IOS-13): TUNE for `keyer`, a remote
    // device's key: the keying gate is asked for that device (a person's key
    // on unheld transmit takes it) and the tune's MOX key is that device's.
    // setTune(false) ends it as any TUNE ends.
    void setTune(bool on, const KeyerIdentity& keyer);

    // TGXL autotune orchestration (NereusSDR-native, no Thetis source).
    //
    // Bench-driven on 2026-05-20: TGXL refuses to run its relay sweep when
    // PGXL is in OPERATE -- the amp is amplifying the radio's tune carrier
    // and TGXL can't calibrate against an amplified signal. The operator
    // workflow with real FlexRadio is: put PGXL in STANDBY, run TGXL
    // autotune at radio tunepower (~10-25 W), then re-arm PGXL to OPERATE.
    //
    // This method orchestrates that sequence:
    //   1. Save current PGXL operate-state (m_pgxlSavedOperate)
    //   2. Send `operate=0` to PGXL if it was operating
    //   3. Engage local CW tune carrier via setTune(true). Thetis-faithful
    //      `chkTUN_CheckedChanged` already swaps rfpower -> tunepower for
    //      the cycle (console.cs:30075 [v2.10.3.13]).
    //   4. After 200 ms settle, send `autotune` to TGXL on :9010 (unless
    //      fromHardware=true, in which case TGXL is already running its
    //      own internal cycle and we only need to provide the carrier).
    //   5. Wait for TGXL tuning=0 -> drop carrier (handled by the
    //      TunerApplet tuningChanged path that calls setTune(false)).
    //   6. On carrier drop, if m_pgxlSavedOperate was true, send
    //      `operate=1` to restore PGXL.
    //
    // fromHardware: true when the cycle was initiated by a TGXL hardware
    // TUNE button (we received `transmit tune on` via LAN PTT). Skips
    // step 4 because TGXL is already running its own sweep internally.
    void startTgxlAutotune(bool fromHardware);

    // iPhone app plan Task 77 (controller addition): the same autotune for a
    // remote device (tx.tunerTune), its tune carrier keyed as `keyer`
    // through the keying gate, so the holder rules and the transmit
    // watchdog apply to it as to the device's tx.tune. False, with the
    // reason in plain words, when no Tuner Genius is connected, a cycle is
    // already running, or transmit is blocked; nothing changes then.
    bool startTgxlAutotuneFor(const KeyerIdentity& keyer, QString* reason);
    /// Ends `deviceId`'s autotune cycle (its carrier, and the amplifier
    /// back to operate). False when no cycle of that device runs.
    /// Tune-ended lane: `unkeyedReason`, the words the device is told when
    /// the cycle ends before its carrier keyed (tgxlAutotuneEnded); empty
    /// for the device's own stop, which its answer already tells it.
    bool cancelTgxlAutotuneFor(const QByteArray& deviceId,
                               const QString& unkeyedReason = QString());
    bool isTgxlAutotuneInProgress() const { return m_tgxlAutotuneInProgress; }
    /// The device the running autotune keys for; empty for the Core's own
    /// (its Tuner page, a Tuner Genius hardware TUNE) or when none runs.
    QByteArray tgxlAutotuneDeviceId() const
    {
        return m_tgxlAutotuneInProgress ? m_tgxlAutotuneDeviceId : QByteArray();
    }

    // ── Phase 3J-1 follow-up: TCI Q_INVOKABLE shims (bench wire-up) ──────────
    //
    // TciProtocol calls into RadioModel by *method name string* via
    // QMetaObject::invokeMethod(...).  For Qt to resolve those names, the
    // methods must be marked Q_INVOKABLE (or be slots, or Q_PROPERTY
    // READ/WRITE — Q_INVOKABLE is the explicit choice here).
    //
    // Phase 6 wired the call sites in TciProtocol.cpp but added the matching
    // Q_INVOKABLE shims only on TestMockRadioModel.  The matrix runner asserts
    // byte-for-byte parity against the mock, so all 80+ matrix rows pass — but
    // when a real client (WSJT-X / ESDR3 / SunSDR) connects against the live
    // RadioModel, every set/query silently no-ops because the meta-object has
    // no entry under those names.
    //
    // This block adds the WSJT-X minimum: PTT (trx), VFO (vfo), mode
    // (modulation), and split_enable.  Subsequent commits will fill the long
    // tail (DSP toggles, AGC, SQL, RIT/XIT, balance, audio configs,
    // calibration).
    //
    // Signatures MUST match the Q_ARG / Q_RETURN_ARG types at each call site
    // in src/core/TciProtocol.cpp:
    //   handleVfo       (line 1086 set / 1104 query)
    //   handleModulation(line 1236 query / 1275 set)
    //   handleTrx       (line 1365 set  / 1380 query)
    //   handleSplit     (line 1416 set  / 1430 query)
    //
    // Connection type: TciProtocol invokes with Qt::DirectConnection (test
    // thread) but the runtime TciServer pumps from the main thread (same
    // thread as RadioModel), so DirectConnection is fine for production too.

    /// Set MOX (PTT) for TCI trx: the TCI keying source.  With a
    /// MoxController this is MoxController::onTciPtt (PttMode::Tci), written
    /// only when it changes the MOX state, as handleTrxMessage writes TCIPTT
    /// only when MOX != bMox (TCIServer.cs:3671-3672 [v2.10.3.15]); without
    /// one it falls back to the TransmitModel latch.
    /// From Thetis TCIServer.cs:3594-3689 [v2.10.3.15]: handleTrxMessage.
    Q_INVOKABLE void setMox(bool on);

    /// The MOX button (TxApplet and the container MOX button).  Ports
    /// Thetis chkMOX_Click (console.cs:29730-29747 [v2.10.3.15]):
    /// MoxController::onMoxButton keys or unkeys with the manual key, and
    /// on the way off TUN and two-tone are turned off as chkMOX_Click does
    /// (first, keeping the manual key until their own ends; see the .cpp).
    void setMoxFromButton(bool on);

    /// TX rulings (JJ, 2026-09-30, item 1): what a press of the MOX button
    /// asks, given the way the button toggled (`toggledOn`). A local window
    /// asks what the button shows. A remote window toggles against its own
    /// key, as the desktop's button does: its key down (or waiting) is
    /// unkeyed; a press after it let go keys, even while the Core's
    /// `transmitting` still reads true for the key it let go; otherwise the
    /// button's way (a lit button for a key not its own is let go).
    bool moxPressAsksOn(bool toggledOn) const;
    /// The same for the TUNE button.
    bool tunePressAsksOn(bool toggledOn) const;

    /// iPhone app plan, desktop remote transmit (R-IOS-13): the 2-TONE
    /// buttons (TxApplet and the container). A local window starts or stops
    /// its TwoToneController; a remote window asks the Core (tx.twoTone).
    void setTwoTone(bool on);

    /// A remote window whose Core takes its keys: MOX, TUNE and two-tone go
    /// to the Core through the transmit verbs (RemoteTransmitClient) and the
    /// window's own MoxController keys nothing. False in a local window.
    bool remoteTransmitRouted() const;

    /// The Core refused this remote window's press, in the Core's words.
    void reportRemoteTransmitRefused(const QString& reason);

    /// Query MOX (PTT).  Returns the current MOX latch state.
    /// From Thetis TCIServer.cs:3555-3558 [v2.10.3.13] — sendMOX.
    Q_INVOKABLE bool mox() const;

    /// Set VFO frequency for receiver `rx`, channel `chan` (0=A, 1=B).
    /// NereusSDR has one frequency per slice; `chan==1` (VFO B) is silently
    /// ignored because the second VFO concept maps to a separate slice, not
    /// to a per-slice secondary frequency.
    /// From Thetis TCIServer.cs:3719-3793 [v2.10.3.13] — handleVfo, set path.
    Q_INVOKABLE void setVfoHz(int rx, int chan, qint64 hz);

    /// Query VFO frequency for receiver `rx`, channel `chan`.  Returns
    /// the slice frequency regardless of `chan` (see setVfoHz note).
    /// From Thetis TCIServer.cs:3793-3833 [v2.10.3.13] — handleVfo, query path.
    Q_INVOKABLE qint64 vfoHz(int rx, int chan) const;
    /// The frequency of the VFO that transmits, for TCI's "CW becomes CWU
    /// above 10 MHz" (Thetis TCIServer.cs:4003-4025 [v2.10.3.15] tests the
    /// TX VFO): the transmitting slice's frequency, or the first slice's
    /// when none is bound to transmit.
    Q_INVOKABLE qint64 transmitVfoHz() const;

    /// The centre of the stream receiver `rx` sits on: its frequency minus
    /// its offset from that centre (SliceModel::shiftOffsetHz). What TCI's
    /// dds line carries, as Thetis's sendDDS reads CentreFrequency /
    /// CentreRX2Frequency (TCIServer.cs:2402-2410 [v2.10.3.15]). 0 when no
    /// such slice exists. Task 12, R-R3-49.
    Q_INVOKABLE qint64 ddsHz(int rx) const;

    /// Receiver `rx`'s RIT offset in Hz, 0 while RIT is off. Thetis folds
    /// udRIT into RXOsc (console.cs:31457-31458 [v2.10.3.15]), so TCI's if
    /// line carries it. Task 12, R-R3-49.
    Q_INVOKABLE int ritHzForRx(int rx) const;

    /// Set demodulation mode for receiver `rx`.  `modeStr` is uppercase
    /// (LSB, USB, CWL, CWU, AM, FM, DIGL, DIGU, etc.).
    /// CWbecomesCWUabove10mhz transform from [2.10.3.6]MW0LGE fixes #365
    /// (TCIServer.cs:3868-3895) is DEFERRED — `cw` maps to CWL until VFOATX /
    /// VFOBTX state plumbing arrives.
    //[2.10.3.6]MW0LGE fixes #365  [original inline tag from TCIServer.cs:3868]
    /// From Thetis TCIServer.cs:3835-3942 [v2.10.3.13] — handleModulation, set.
    Q_INVOKABLE void setMode(int rx, QString modeStr);

    /// Query demodulation mode for receiver `rx`.  Returns uppercase name.
    /// From Thetis TCIServer.cs:3942-3954 [v2.10.3.13] — handleModulation, query.
    Q_INVOKABLE QString mode(int rx) const;

    /// Query split-TX state.  Always returns false: Phase 3F deletes the
    /// `setSplit` stub per design §3 ("split is replaced with XIT for plus
    /// or minus 10 kHz tuning offset, or addSliceOnPan to create a second
    /// slice for full retune"). The query stays so TciProtocol's init burst
    /// can still emit `split_enable:rx,false;` for wire-protocol stability
    /// with WSJT-X / N1MM / Log4OM clients ("Split Operation: None/Fake It"
    /// is the supported configuration).
    /// See `docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md`
    /// §3 ("VFO A/B / split: not implemented").
    Q_INVOKABLE bool split(int rx) const;

    // ── Phase 3J-1 closeout Item 3 (2026-05-12): TCI Q_INVOKABLE long tail ──
    //
    // ~56 additional shims that TciProtocol calls via QMetaObject::invokeMethod.
    // Without these the matrix test (against TestMockRadioModel) passes but
    // ESDR3 / N1MM / Log4OM / SunSDR-native clients hit silent no-ops on the
    // production RadioModel.  Each shim is documented with what it does
    // semantically (mock parity) and what underlying state it writes (real
    // model side).  Stubs are explicitly labeled "stub until <feature> lands".

    // VFO lock — routes to SliceModel::locked.  TCI carries two-chan-per-rx
    // semantics from Thetis (VFOALock + VFOBLock); NereusSDR collapses them
    // because per-slice VFO B isn't modeled.  Both chan==0 and chan==1
    // read/write the same slice-level locked flag.
    Q_INVOKABLE void setVfoLock(int rx, int chan, bool locked);
    Q_INVOKABLE bool vfoLock(int rx, int chan) const;
    Q_INVOKABLE void setLock(int rx, bool locked);
    Q_INVOKABLE bool lock(int rx) const;

    // Mute — routes to SliceModel::muted (per-slice) and a new RadioModel
    // member m_globalMute (global).  Global mute is broadcast-only state;
    // when global mute is on, all slices' audio is suppressed downstream.
    Q_INVOKABLE void setGlobalMute(bool on);
    Q_INVOKABLE bool globalMute() const;
    Q_INVOKABLE void setRxMute(int rx, bool on);
    Q_INVOKABLE bool rxMute(int rx) const;

    // Filter — routes to SliceModel::filterLow / filterHigh.  setFilterBand
    // sets BOTH cutoffs atomically.
    Q_INVOKABLE void setFilterBand(int rx, int lowHz, int highHz);
    Q_INVOKABLE int  filterLow(int rx) const;
    Q_INVOKABLE int  filterHigh(int rx) const;

    // AGC mode — routes to SliceModel::agcMode (enum).  Mock uses uppercase
    // strings: "OFF" / "LONG" / "SLOW" / "MED" / "FAST" / "CUSTOM".
    Q_INVOKABLE void    setAgcMode(int rx, const QString& mode);
    Q_INVOKABLE QString agcMode(int rx) const;

    // AGC gain (threshold) — routes to SliceModel::agcThreshold (-20..120).
    Q_INVOKABLE void setAgcGain(int rx, int gain);
    Q_INVOKABLE int  agcGain(int rx) const;

    // Squelch — routes to SliceModel::ssqlEnabled / ssqlThresh.  TCI level
    // is int (-140..0 dBm); SliceModel::ssqlThresh is double in same units.
    Q_INVOKABLE void setSqlEnable(int rx, bool on);
    Q_INVOKABLE bool sqlEnable(int rx) const;
    Q_INVOKABLE void setSqlLevel(int rx, int level);
    Q_INVOKABLE int  sqlLevel(int rx) const;

    // RIT / XIT — routes to SliceModel::ritEnabled/ritHz/xitEnabled/xitHz on
    // the active slice.  Thetis treats these as radio-global (single VFO
    // pair); NereusSDR collapses to active-slice for symmetry with mode/mox.
    Q_INVOKABLE void setRitEnable(bool on);
    Q_INVOKABLE bool ritEnable() const;
    Q_INVOKABLE void setRitOffset(int hz);
    Q_INVOKABLE int  ritOffset() const;
    Q_INVOKABLE void setXitEnable(bool on);
    Q_INVOKABLE bool xitEnable() const;
    Q_INVOKABLE void setXitOffset(int hz);
    Q_INVOKABLE int  xitOffset() const;

    // RX balance / audio pan — routes to SliceModel::audioPan.  TCI uses
    // double in [-1, 1]; SliceModel matches.  chan arg is ignored (single
    // pan per slice, not per VFO).
    Q_INVOKABLE void   setRxBalance(int rx, int chan, double balance);
    Q_INVOKABLE double rxBalance(int rx, int chan) const;

    // CTUN — per-slice stub (no CTUN model state yet; spectrum-widget owns
    // the interaction mode).  Stored in m_tciStubRxCtun, set-and-read only
    // until a CTUN model lands.
    Q_INVOKABLE void setRxCtun(int rx, bool on);
    Q_INVOKABLE bool rxCtun(int rx) const;

    // ── DSP toggles (NbMode + activeNr based) ────────────────────────────
    // setRxNb: maps bool to NbMode (true -> last-non-None mode; false -> None).
    Q_INVOKABLE void setRxNb(int rx, bool on);
    Q_INVOKABLE bool rxNb(int rx) const;
    // setRxNr: maps (bool, int nrIndex) to activeNr enum slot.
    Q_INVOKABLE void setRxNr(int rx, bool on, int nrIndex);
    Q_INVOKABLE bool rxNr(int rx) const;
    Q_INVOKABLE int  rxNrIndex(int rx) const;
    // setRxAnf / rxAnf: routes to SliceModel::anfEnabled (Phase 3F Sub-Epic J
    // Task 1 added ANF as its own Q_PROPERTY, independent of the activeNr
    // slot enum).  Previously this pair stubbed ANF state into
    // m_tciStubRxApf -- the APF array -- so toggling ANF via TCI silently
    // flipped APF's stored bit too, and neither one touched real WDSP ANF.
    // Sub-Epic J Task 10 (rx_volume) closeout fixed the routing.
    Q_INVOKABLE void setRxAnf(int rx, bool on);
    Q_INVOKABLE bool rxAnf(int rx) const;

    // setRxBin / rxBin: routes to SliceModel::binauralEnabled.
    // setRxApf / rxApf: routes to SliceModel::apfEnabled.
    // Both are per-receiver upstream (handleRxBinEnable TCIServer.cs:1854-1869,
    // handleRxApfEnable TCIServer.cs:1870-1894 [v2.10.3.15]) and both already
    // had SliceModel properties wired to RxChannel, but Phase 3F chip
    // task_c1e6fbad found these two shims still storing into private arrays
    // and reading straight back out, so a TCI client could set either one, be
    // told it took effect, and change nothing in the DSP chain.
    Q_INVOKABLE void setRxBin(int rx, bool on);
    Q_INVOKABLE bool rxBin(int rx) const;
    Q_INVOKABLE void setRxApf(int rx, bool on);
    Q_INVOKABLE bool rxApf(int rx) const;

    // NOT a stub since TNF section 6.4: these read and write the NotchModel
    // master enable.  Global despite the per-rx command shape, exactly as
    // Thetis GetMNF is (console.cs:52317-52330 [v2.10.3.15]).
    Q_INVOKABLE void setRxNf(int rx, bool on);
    Q_INVOKABLE bool rxNf(int rx) const;

    // ── Per-slice AF gain (rx_volume: query source) ──────────────────────
    // Distinct from afLinear() below: afLinear is the single radio-global
    // master volume slider (Thetis console AF field, handleVolume /
    // "volume:" line).  afGain(rx) is the per-receiver AF gain (Thetis
    // RX0Gain/RX1Gain/RX2Gain, handleRxVolume / "rx_volume:" lines), routed
    // to SliceModel::afGain (WDSP RXA panel gain1 -- see the afGainChanged
    // connect in the constructor).  Getter-only: TciProtocol has no
    // rx_volume set/query dispatch case today (only the init burst reads
    // this), matching the calibration getters above.  See
    // TciProtocol.cpp's buildInitialRadioStateLines rx_volume block for the
    // receiver -> slice id mapping this feeds and its active-slice fallback.
    Q_INVOKABLE int afGain(int rx) const;

    // ── Volume (linear int) ──────────────────────────────────────────────
    // setAfLinear: TCI sends 0..32767; we store and let the audio path read.
    // monLinear: TX monitor volume; same range.  These are NereusSDR-global
    // (not per-slice) -- matches Thetis console.cs handleAFVolume.
    Q_INVOKABLE void setAfLinear(int v);
    Q_INVOKABLE int  afLinear() const;
    Q_INVOKABLE void setMonLinear(int v);
    Q_INVOKABLE int  monLinear() const;

    // ── IQ sample rate ───────────────────────────────────────────────────
    // setIqSampleRate: TCI echoes the rate back per Thetis pattern; the
    // radio hardware doesn't actually change rate from a TCI command.
    Q_INVOKABLE void setIqSampleRate(int sr);
    Q_INVOKABLE int  iqSampleRate() const;

    // ── Audio stream config ──────────────────────────────────────────────
    // Per-client state lives in TciClientSession; TciServer intercepts these
    // commands BEFORE the invokeMethod fires, so the production shims here
    // are dead-code parity with the mock.  Kept for symmetry + matrix-test
    // compatibility.  They store last-seen value but no other side effect.
    Q_INVOKABLE void    setAudioSampleRate(int sr);
    Q_INVOKABLE int     audioSampleRate() const;
    Q_INVOKABLE void    setAudioStreamSampleType(const QString& t);
    Q_INVOKABLE QString audioStreamSampleType() const;
    Q_INVOKABLE void    setAudioStreamChannels(int n);
    Q_INVOKABLE int     audioStreamChannels() const;
    Q_INVOKABLE void    setAudioStreamSamples(int n);
    Q_INVOKABLE int     audioStreamSamples() const;

    // ── TX profile (via MicProfileManager) ───────────────────────────────
    // setTxProfile: name lookup against the operator's profile library.
    // txProfilesList: enumerates installed profiles.
    Q_INVOKABLE void        setTxProfile(const QString& name);
    Q_INVOKABLE QString     txProfile() const;
    Q_INVOKABLE QStringList txProfilesList() const;

    // ── Calibration (TCI calibration_ex) ─────────────────────────────────
    // calibrationMeter is rxMeterCalOffsetDb() and calibrationDisplay is
    // rxDisplayCalOffsetDb(), for either rx: NereusSDR keeps one receive
    // calibration, which Thetis starts RX2 from too (console.cs:999
    // [v2.10.3.15]). The XVTR, 6 m and TX display terms still return 0.0.
    Q_INVOKABLE double calibrationMeter(int rx) const;
    Q_INVOKABLE double calibrationDisplay(int rx) const;
    Q_INVOKABLE double calibrationXvtr(int rx) const;
    Q_INVOKABLE double calibrationSixMeter(int rx) const;
    Q_INVOKABLE double calibrationTxDisplay(int rx) const;

    // ── Phase 3J-1 closeout (2026-05-22): init-burst live-state shims ───────
    //
    // Added so TciProtocol::buildInitialRadioStateLines can read live
    // RadioModel state instead of emitting the Phase 4 Task 4.2 hardcoded
    // placeholders.  Each shim is a thin Q_INVOKABLE wrapper over existing
    // state or trivial derivation -- no new member variables, no behavior
    // changes.  Architectural divergences (where NereusSDR's storage model
    // differs from Thetis Console) are documented in each shim header AND
    // mirrored in the TciProtocol.cpp call site so reviewers see the same
    // story from both sides.

    /// "RX2 enabled" -- derived from connectionActiveRxCount >= 2.
    /// Thetis console.cs:37278 [v2.10.3.15] backs RX2Enabled with the
    /// rx2_enabled member (chkRX2.Checked).  NereusSDR uses the active-
    /// receiver count (set by RadioModel::setActiveRxCountLive) as the
    /// authoritative source.
    Q_INVOKABLE bool rx2Enabled() const;

    /// TX monitor enable -- forwards to TransmitModel::monEnabled().
    /// Thetis console.cs:18656-18663 [v2.10.3.15] -- MON = chkMON.Checked.
    /// NereusSDR's m_transmitModel.m_monEnabled defaults false and is never
    /// persisted (safety: MON loads OFF always, matching Thetis audio.cs:406).
    Q_INVOKABLE bool monEnabled() const;

    /// Tune state -- m_isTuning (latched true between setTune(true) and
    /// completeTuneOff()).  Thetis console.cs:18677-18684 [v2.10.3.15] --
    /// TUN = chkTUN.Checked.  Semantically identical.
    ///
    /// Separate from the existing isTune() accessor (which is noexcept and
    /// cannot be Q_INVOKABLE).  Same backing field.
    Q_INVOKABLE bool tune() const;

    /// Power-on -- forwards to isConnected().
    ///
    /// Architectural divergence from Thetis console.cs:19799-19803
    /// [v2.10.3.15] PowerOn = chkPower.Checked.  In Thetis PowerOn and
    /// connection state are SEPARATE concepts: a user can "power off" the
    /// radio while remaining connected.  NereusSDR has no such mode -- the
    /// connection IS the power switch.  TCI clients see powerOn = true
    /// while connected, false while disconnected (no "soft off" state).
    Q_INVOKABLE bool powerOn() const;

    /// DIGL click-tune offset -- active slice's diglOffsetHz.
    ///
    /// Architectural divergence from Thetis console.cs:14693-14749
    /// [v2.10.3.15] DIGLClickTuneOffset (radio-global private member,
    /// default 2210 Hz).  NereusSDR stores per-slice on SliceModel
    /// (m_diglOffsetHz, default 0 Hz per SliceModel.h:928).  For TCI we
    /// expose the active slice's value as the radio's "current" DIGL
    /// offset; falls back to 0 when no slice is active (pre-connect probe).
    Q_INVOKABLE int diglOffset() const;

    /// DIGU click-tune offset -- active slice's diguOffsetHz.
    ///
    /// Same Thetis-vs-NereusSDR divergence as diglOffset.  Thetis
    /// console.cs:14658-14691 [v2.10.3.15] -- DIGUClickTuneOffset
    /// (radio-global, default 1500 Hz).  NereusSDR per-slice
    /// (m_diguOffsetHz, default 0 Hz per SliceModel.h:929).
    Q_INVOKABLE int diguOffset() const;

    // ── Phase 3R Task I5: RadeChannel signal-graph slots ────────────────────
    //
    // Public slots so Qt's auto-connection queues them correctly when the
    // emitting RadeChannel ever moves to a worker thread (J2/J3 currently
    // keep it on the main thread; the public-slot declaration is forward-
    // safe regardless).
    //
    // All three accept an int sliceId so a single RadioModel can route
    // multiple per-slice RadeChannels through one set of slots. The
    // wireRadeChannel helper captures the slice ID in a lambda at wire
    // time and adapts the channel's per-channel signals into these.

    // Forwards a decoded callsign (+ optional grid) into RxDecodeModel as
    // a single decode row. mode="RADE", source="rade_text". Grid is
    // appended to the payload only when non-empty (I4 Option B does not
    // carry grid; the field is present for future text-channel revs).
    void onRadeTextDecoded(int sliceId, const QString& callsign,
                           const QString& grid);

    // Tracks per-slice RADE decoder sync state. Emits radeSyncChanged
    // only on actual transitions: repeated identical values collapse to
    // a single emit (de-duplication keeps the future status-bar
    // indicator from flickering on repeated sync=true reports from the
    // codec).
    void onRadeSyncChanged(int sliceId, bool synced);

    // Forwards the codec's SNR estimate to the slice's snrDb property
    // (D5 added SliceModel::setSnrDb). Cast-up float->double at the
    // boundary; the slice setter no-ops on identical numeric values
    // so repeated identical SNR updates do not spam UI repaint.
    void onRadeSnrChanged(int sliceId, float snrDb);

    // 2026-05-12 bench: throttled FreeDV Reporter freq publish.  See
    // m_freedvFreqDwellTimer member declaration for the policy.  Called
    // from the slice.frequencyChanged subscriber at line ~4945 in
    // RadioModel.cpp.  Caller passes the new VFO Hz; this method either
    // publishes immediately (initial baseline / band-jump fast-path /
    // MOX force) or restarts the dwell timer for a deferred publish.
    void publishFreedvFrequencyDwelled(quint64 hz);
    /// Fix wave (several-devices ruling 5.11): the slice whose frequency
    /// the FreeDV Reporter lists: the first slice in RADE mode, else the
    /// station-level active slice.
    SliceModel* freedvReportedSlice() const;
    /// Lists freedvReportedSlice()'s frequency when it changed.
    void refreshFreedvReportedFrequency();
    // Force-publish the current pending freq right now and reset the
    // dwell.  Called from MoxController::txAboutToBegin so a TX engage
    // never leaves the reporter showing a stale freq.
    void flushFreedvFrequencyDwell();

    // Phase 3J-1 closeout follow-up (2026-05-12): FreeDV Reporter is a
    // dashboard for FreeDV / RADE operators.  Our station should be
    // visible there only when we're actually using RADE (RADE_U or
    // RADE_L) -- not when we're on SSB / WSJT-X / CW.  Mirrors
    // freedv-gui's connect-and-hide-when-not-on-FreeDV behavior; we stay
    // connected so we can still see other FreeDV stations and report
    // their decodes via sendRxReport, but our own row stays hidden on
    // the public dashboard unless we're TX-capable in RADE.
    //
    // Wired on:
    //   - active slice's dspModeChanged (mode switch during operation)
    //   - active slice swap (different slice becomes active)
    //   - FreeDVReporterClient::connected (initial state after connect)
    void updateFreedvReporterVisibility();

signals:
    void diversityStateChanged(const QString& state);
    void infoChanged();
    // Task 33: stopAllTx stopped a transmission. A non-empty message is for
    // the operator (MainWindow shows it for 10 s, as Thetis's
    // infoBar.Warning(msg, false, 10000)).
    void transmitStopped(QString message);
    // iPhone app plan Task 38: the Core stopped a transmission for a
    // reason of its own (lastTransmitStopReason()); emitted after
    // transmitStopped.
    void transmitStopReasonRaised(const QByteArray& code, int limitSeconds);
    // iPhone app plan Task 35: keyedBy() changed.
    void keyedByChanged();
    // iPhone app plan Task 36: remoteMicInUse() changed.
    void remoteMicInUseChanged(bool inUse);
    // iPhone app plan Task 37 (fix wave C2): a device's microphone line
    // opened or closed. The Core turns off the VOX a device armed when its
    // line closes.
    void remoteMicLinesChanged();
    // Fix wave C2: remoteMicWriter() changed.
    void remoteMicWriterChanged(const QByteArray& deviceId);
    // Phase 3Q-1: parametrized — state passed so UI consumers can act without
    // a secondary RadioModel::connectionState() read under race conditions.
    // Existing no-arg slot connections (ConnectionPanel, MainWindow, SpectrumWidget)
    // remain valid: Qt discards excess signal args when slot arity is lower.
    void connectionStateChanged(NereusSDR::ConnectionState newState);
    // Explicit owner/operator intent, including a stop while already offline.
    void radioDisconnectRequested();
    // Emitted when the on-air sample rate for the current connection is
    // known. MainWindow reacts by updating FFTEngine + SpectrumWidget so
    // bin math matches the wire rate (P1=192k, P2=768k).
    void wireSampleRateChanged(double rateHz);
    // R-R3-39: a change setSampleRateLiveAsync started has finished (ok) or
    // could not run.
    void sampleRateChangeFinished(int rateHz, bool ok);
    // Task 1.7: emitted after setActiveRxCountLive() successfully applies
    // the new receiver count to both hardware and WDSP channels.
    void activeRxCountChanged(int newCount);

    // Fires when the 4O3A master toggle flips (Setup → CAT & Network →
    // 4O3A → General). Consumers (e.g., MainWindow's applet visibility
    // wiring) react to grey out / hide the Amplifier and Tuner applets
    // when 4O3A is off.
    void fourO3AEnabledChanged(bool enabled);
    void fourO3AStatusChanged();
    void stationFourO3ACommandFinished(bool accepted, const QString& reason);
    // Fires when the RF-Kit master toggle flips (Setup -> CAT & Network ->
    // RF-Kit -> General). Consumers (e.g. MainWindow applet visibility)
    // react to show/hide the RF2K-S applet.
    void rfKitEnabledChanged(bool enabled);
    // R-R3-49: isTransmitting() changed.
    void transmittingChanged(bool transmitting);
    // RADE end-of-over callsigns: endOfOverTailActive() changed.
    void endOfOverTailChanged(bool active);
    // R-R3-49 (parity Task 1): isCoreOnAir() changed.
    void coreOnAirChanged(bool onAir);
    // R-R3-49: the Core's transmit band (Thetis _tx_band, m_txBand) changed
    // or became known or unknown. It holds while keyed, so paOnAirBandIndex
    // follows this, not a slice's band.
    void transmitBandChanged();
    // R-R3-32 (parity Task 6): paReadings() changed.
    void paReadingsChanged();
    // paTransmitBand() changed (paTransmitBandVersion 1).
    void paTransmitBandChanged(int band);
    // Parity Task 33: paRawAdc() changed.
    void paRawAdcChanged();
    // Parity Task 33: stationTxReadingsVersion() changed.
    void stationTxReadingsVersionChanged();
    // R-R3-49 (parity Task 6): isTxInhibited() changed.
    void txInhibitedChanged(bool inhibited);
    // HL2 port part 2: txInhibitReason() changed.
    void txInhibitReasonChanged(const QString& reason);
    // Fires on each transition to Connected with the RadioInfo of the live
    // connection. HardwarePage (Phase 3I) listens to this to repopulate
    // sub-tabs with per-radio fields.
    void currentRadioChanged(const NereusSDR::RadioInfo& info);

    // ── Phase 3M-4 Task 13: late-bound PureSignal coordinator handoff ──────
    // Fires when m_pureSignal is created (post-WDSP-init) or torn down.
    // Carries the live PureSignal* (nullptr on disconnect).  Subscribers
    // (PureSignalApplet, TxApplet [PS-A]) re-wire their controls when the
    // coordinator becomes available.
    //
    // The coordinator does not exist at MainWindow construction time
    // (RadioModel::pureSignal() returns nullptr until connectToRadio()'s
    // WDSP-init lambda fires, see RadioModel.cpp:1884 [v2.10.3.13]).  This
    // signal is the late-binding seam.  Tests call
    // emit pureSignalCoordinatorReady(...) directly to inject a test-owned
    // coordinator into the applet wiring.
    void pureSignalCoordinatorReady(NereusSDR::PureSignal* coordinator);
    // iPhone app Part A fix wave (R-IOS-01): the station's receive-only
    // policy changed. Readiness that depends on it (PureSignal's
    // canActuate, which the session facade publishes) follows at once
    // instead of on the coordinator's next status poll.
    void receiveOnlyStationPolicyChanged(bool receiveOnly);
    void settingsSaveErrorChanged(const QString& reason);
    void sliceAdded(int index);
    void sliceRemoved(int index);
    /// Slice control fix wave (Important 4): the Core's own window chose
    /// `sliceId` for transmit (requestTxHandoffToSlice accepted it). Never
    /// emitted for a binding the transmitter gets by itself.
    void txSliceSelected(int sliceId);
    // A restored shared slice may have changed many preferences silently.
    // Consumers must publish a complete snapshot, not duplicate sliceAdded.
    void receiveLayoutHydrated();
    void receiveLayoutRestoreStatusChanged();

    /// Phase 3F Sub-Epic F Task 5: emitted after the per-ADC wideband FFT
    /// completes.  adcIndex is 0 or 1; dbmBins is 32768 raw dB entries (kOutputBins
    /// from WidebandFftEngine).  SpectrumWidget consumes this in extended
    /// pan rendering; visual paint wires in Sub-Epic F polish (T7-T10).
    void widebandSpectrumReady(int adcIndex, QVector<float> dbmBins);
    void widebandSourceChanged(int adcIndex);
    void widebandSpectrumAvailable(int adcIndex, quint32 sourceGeneration);
    // Phase 3F Sub-Epic C Task 7: emitted when addSliceOnPan or addSlice
    // rejects a request because the slice limit (sliceChannelLimit()) has
    // been reached; `reason` is sliceCapReason()'s words. MainWindow shows
    // it as a toast.
    void sliceAddRejected(QString reason);
    /// R-R3-47 / R-R3-22: the Core refused a request for an accessory's own
    /// settings (`device` "pgxl" or "tgxl"); `reason` is the Core's words.
    /// Parity mini-round (ruling c): a local window's own switch refused on
    /// the air arrives here too, with the same words
    /// (refuseLocalAccessorySwitchOnAir).
    void accessoryRequestRefused(const QString& device, const QString& reason,
                                 bool shownOnPage);
    /// Remote window: a Core station setting changed (empty: a snapshot).
    void stationSettingChanged(const QString& key);
    /// A setting the transmit gate reads (ExtendedTransmit) changed on
    /// this model's own settings: a local window's edit, or a device's
    /// edit the Core took. A remote window hears stationSettingChanged.
    void transmitGateSettingChanged(const QString& key);
    /// Scoped review (addendum G-42): who holds transmit on this Core
    /// changed (otherDeviceHoldsRefusal may read differently).
    void transmitHolderChanged();
    /// R-R3-22 fix wave: see reportStationCommandFinished. The one
    /// per-command result signal: the amp applets' pending requests and
    /// the TCI switch's request wait both match their own id here (an
    /// accessory refusal also arrives on accessoryRequestRefused).
    void stationCommandFinished(quint32 commandId, bool accepted, const QString& reason);
    void stationSettingsBackupExportFinished(quint32 operationId, bool accepted,
                                             const QString& reason,
                                             const QByteArray& coreXml);
    /// R-R3-49 (parity Task 8): see reportStationTgxlLanScan.
    /// R-R3-49 (parity Task 9): see reportStationPgxlLanScan.
    void stationPgxlLanScanFinished(quint32 commandId, bool accepted, const QString& reason,
                                    const QString& devicesJson);
    void stationTgxlLanScanFinished(quint32 commandId, bool accepted, const QString& reason,
                                    const QString& devicesJson);

    /// Phase 3F Sub-Epic I closeout, defect F4: the operator retuned a slice
    /// to a frequency no DDC can reach, and the frequency has been rolled
    /// back to the last one that bound. Distinct from sliceAddRejected
    /// because that one talks about adding a slice, which is not what
    /// happened -- the operator turned the knob.
    ///
    /// After this fires, the slice's frequency, stream binding and shift
    /// offset all agree again. `reason` is plain English, ready for a status
    /// bar, and names the frequency the slice stayed on.
    void sliceRetuneRejected(int sliceIndex, const QString& reason);
    /// Slice control plan Task 5: a remote window held back a change to
    /// slice `sliceId` (one of its setters, or a slice request) because it
    /// only listens to that slice; nothing was sent. `reason` is the Core's
    /// listener words (SliceModel::readOnlyListenerReason()).
    void sliceRequestHeldForListener(int sliceId, const QString& reason);
    /// Parity Task 21 (R-IOS-18): the Core's radios changed (a remote
    /// window), or the Core refused a radio request.
    void stationRadiosChanged();
    void stationRadioRefused(const QString& reason);

    /// Phase 3F Sub-Epic I: a stream's slice set changed. Consumers rebuild
    /// FFT routing; RadioModel republishes the set to RxDspWorker.
    void streamBindingsChanged(int streamIndex, const QVector<int>& sliceIndices);

    /// Phase 3F Sub-Epic I closeout, defect F3: streams that still host slices
    /// but that the per-board codec left without a DDC, so the radio has
    /// stopped streaming them. Emitted on transitions only (both into and out
    /// of the suspended state; an empty list means everything is back).
    ///
    /// This happens legitimately on the 1-ADC HERMES class, where Thetis
    /// collapses to a single synced pair whenever PureSignal transmits or
    /// diversity engages (console.cs:8448-8456 [v2.10.3.15]). The behaviour is
    /// upstream-faithful; the silence around it was not, which is what this
    /// signal fixes. `reason` is plain English, ready for a status bar.
    void streamsSuspended(const QVector<int>& streamIndices, const QString& reason);

    /// Phase 3F Sub-Epic I: a stream was activated or retuned; its FFTEngine
    /// and panadapter window must follow.
    void streamCentreChanged(int streamIndex, double centreHz, int sampleRateHz);

    /// A stream's PHYSICAL ADC moved, without necessarily anything else
    /// moving with it.
    ///
    /// An antenna or codec-state change can shift a stream from ADC0 to ADC1
    /// while it keeps its stream index, and on a one-chain board its folded
    /// chain index stays 0 too, so no slice property changes and nothing that
    /// watches slice properties re-reads the routing. Anything keyed on the
    /// physical ADC (the extended pan's wideband wings) has to listen here.
    /// Codex, PR #318.
    void streamAdcRoutingChanged();

    /// iPhone app Task 75 (the several-devices design, ruling 5.11a, D61):
    /// slice `sliceId` crossed a band edge and the receive antenna stayed on
    /// `antenna` because `listeners` (other devices, by id) listen through
    /// it. Local only.
    void receiveAntennaKept(int sliceId, const QString& antenna,
                            const QList<QByteArray>& listeners);

    /// Phase 3F Sub-Epic I: emitted whenever the slice or stream set changes
    /// such that the per-board codec must recompute the DDC assignment.
    /// Observation hook; invokeCodecDdcAssignment does the work. Task 7b: it
    /// no longer no-ops while disconnected; only the wire push is gated on
    /// a connection, because the client-side mapping has to be correct
    /// before the first packet arrives.
    void ddcAssignmentRequested();

    // Phase 3F closeout — Sub-Epic E Task 6 consumer wire. Emitted when
    // AlexController auto-switches an antenna due to a conflict-policy
    // re-route (Sub-Epic E Tasks 11-13 will fill in the real detection;
    // for now the signal surface exists so MainWindow can wire
    // AntennaSwitchToast). Carries the slice that moved plus the old and
    // new antenna names. UNDO action is consumer-defined.
    void antennaAutoSwitched(int sliceIndex, QString oldAntenna,
                              QString newAntenna);
    // Phase 3F closeout — Sub-Epic E Task 7 consumer wire. Emitted when
    // adding a slice would force a TX-bound chain re-route to a different
    // antenna. Consumer (MainWindow) opens TxBoundConfirmDialog. Real
    // emission from addSliceOnPan lands when the conflict-policy state
    // machine ships in a follow-up.
    void txBoundReRouteRequested(QString proposedAntenna,
                                  QString existingAntenna);
    void activeSliceChanged(int index);

    // Remote-daemon R2 Task 11. activeSliceChanged above is a LIST
    // POSITION (see setActiveSlice's own doc comment: "still a LIST
    // POSITION, unlike sliceById / removeSlice"), which diverges from the
    // stable per-slice id after any mid-list removal -- exactly the trap
    // setActiveSliceById exists to avoid for callers, except this signal
    // itself had no id-based counterpart for LISTENERS. Fires alongside
    // activeSliceChanged, from the same three sites, carrying the newly-
    // active slice's SliceModel::sliceIndex() (its stable id) instead of
    // its position; -1 when activeSliceChanged also reports -1 (no active
    // slice). R2 does not consume this yet -- Task 11 adds it because the
    // positional signal is a live trap for any future id-based listener,
    // not because anything in this plan wires to it today. The positional
    // signal is unchanged and unaffected; existing GUI code keeps binding
    // to it exactly as before.
    void activeSliceIdChanged(int sliceId);

    // Emitted once at the end of loadSliceState() after the slice has been
    // restored from AppSettings. Mirrors Thetis console.cs:27204 [v2.10.3.13]
    // chkPower_CheckedChanged calling txtVFOAFreq_LostFocus() as the
    // explicit "push state to display" step at power-on. Listeners
    // (MainWindow, SpectrumWidget bridge) push the now-correct slice
    // freq/mode/filter into views, since the wireSliceToSpectrum() seed
    // ran with the slice's pre-restore default values.
    void sliceStateRestored(int index);
    // Issue #153 sub-bug 2 — diagnostic + test observation hook.
    // Emitted by pushTxModeAndBandpass() when a TX-bound slice exists,
    // BEFORE the queued setter dispatch to TxWorkerThread.  Carries the
    // slice's current DSPMode + audio-space filter cutoffs.  Tests use
    // it as a proxy for "push helper triggered with X"; production code
    // can wire it into diagnostic logging.
    void txModeAndBandpassPushed(NereusSDR::DSPMode mode,
                                 int audioLowHz, int audioHighHz);
    void panadapterAdded(int index);
    void panadapterRemoved(int index);

    // Raw interleaved I/Q for spectrum display (tapped before WDSP processing)
    void rawIqData(const QVector<float>& interleavedIQ);

    // Phase 3F Sub-Epic I Task 8: stream-tagged companion to rawIqData,
    // which is kept for existing single-slice subscribers. MainWindow's
    // per-stream FFTEngine pool subscribes to this one so each DDC's bins
    // reach its own engine (and from there its own panadapter).
    // Emitted from the same Connection-thread fork as rawIqData.
    void rawIqDataForStream(int streamIndex, const QVector<float>& samples);

    // Phase 3Q-6: forwarded from the active RadioConnection::frameReceived()
    // so TitleBar::ConnectionSegment can pulse its activity LED without
    // holding a reference to a connection that may be recreated on reconnect.
    // Re-emitted from wireConnectionSignals() for every new connection.
    void frameReceived();

    // Emitted when onBandButtonClicked short-circuits in a user-visible way
    // (locked slice, XVTR no-seed). MainWindow connects this to the status
    // bar so the user learns why their band click did nothing — prevents
    // silent failure. `reason` is a one-line human-readable message.
    // Issue #118.
    void bandClickIgnored(NereusSDR::Band band, QString reason);

    // Phase 3M-0 Task 6: Ganymede PA-trip live state.
    // Emitted whenever the trip latch changes (true = tripped, false = clear).
    // From Thetis Andromeda/Andromeda.cs:914-920 [v2.10.3.13]
    // (CATHandleAmplifierTripMessage). G8NJJ: handlers for Ganymede 500W PA protection.
    void paTrippedChanged(bool tripped);

    // Task 16: the effective receive-only state or its reason changed
    // (isRxOnly, isRxOnlyForced, rxOnlyReason).
    void rxOnlyChanged(bool on);

    // TX safety (2026-09-30): isRadioLinkDown changed.
    void radioLinkDownChanged(bool down);

    // Task 1.8: DSP rebuild elapsed time signal.
    // Emitted whenever a live DSP change (sample rate, active RX count,
    // DSP-Options buffer/filter changes) completes. The argument is the
    // elapsed wall-clock milliseconds for the rebuild. Used by
    // DspOptionsPage's "Time to last change" readout.
    void dspChangeMeasured(qint64 elapsedMs);
    // Remote-window parity Task 16: stationDspInfoVersion() changed (the
    // filter curve's availability follows it).
    void stationDspInfoVersionChanged();
    // Remote-window parity Task 30: stationTxDisplayVersion() changed.
    void stationTxDisplayVersionChanged();
    // nrCannotRunReason may have changed for a reason DspAssetService's own
    // signals do not carry: whether the Core says (stationDspAssetVersion).
    void nrAvailabilityChanged();
    // Remote-window parity Task 16: dspOptionsLastApplyMs() changed.
    void dspOptionsLastApplyMsChanged(qint64 elapsedMs);
    // Fix wave (M2): stationRadioWaiting() changed.
    void stationRadioWaitingChanged(const QString& reason);
    // Remote-window parity Task 22: logCategories() changed.
    void logCategoriesChanged(const QString& categories);
    // Remote-window parity Task 22: stationCoreLog() changed.
    void stationCoreLogChanged();
    // Remote-window parity Task 22: the link's support availability moved.
    void stationSupportAvailabilityChanged();
    // Remote-window parity Task 22: the Core answered support.collect.
    void stationSupportBundleFinished(quint32 commandId, bool accepted, const QString& reason,
                                      const QByteArray& bundle);
    // Remote-window parity Task 22: the Core refused a category change.
    void stationLogCategoriesRefused(const QString& reason);
    // Remote-window parity Task 16: coreFilterResponse() changed.
    void coreFilterResponseChanged();
    void miniFilterResponseChanged(int sliceId);

    // Phase 3Q Task 10: auto-connect failure signals.
    //
    // autoConnectFailed — emitted when an auto-connect-on-launch attempt fails
    // (RadioConnection::connectFailed fires while m_autoConnectInProgress is set).
    // `mac`    — the saved-radio MAC key that was attempted.
    // `reason` — typed failure code (Timeout is the most common: radio unreachable).
    // MainWindow reacts by opening the ConnectionPanel and posting a status-bar message.
    void autoConnectFailed(const QString& mac, NereusSDR::ConnectFailure reason);

    // autoConnectAmbiguous — emitted when tryAutoReconnect finds more than one
    // saved radio with autoConnect = true. The most-recently-connected MAC wins;
    // MainWindow surfaces a one-time status-bar warning pointing to Manage Radios.
    // `count`      — total number of autoConnect-flagged radios.
    // `chosenMac`  — the MAC selected (most recently connected).
    void autoConnectAmbiguous(int count, const QString& chosenMac);

    // ── Phase 3M-1a Task G.4: TUNE refused ──────────────────────────────────
    // Emitted when setTune(true) is called but the power-on guard fires
    // (radio not connected / audio engine not active).
    // Cite: Thetis console.cs:29983-29991 [v2.10.3.13] — MessageBox "Power must be on".
    // NereusSDR equivalent: emit signal; UI reacts with a toast or status bar message.
    // Subscribers should uncheck the TUN button and display `reason` to the user.
    void tuneRefused(const QString& reason);

    /// iPhone app plan, desktop remote transmit (R-IOS-13): the Core
    /// refused a MOX, TUNE or two-tone press from this remote window (or its
    /// release). Shown as a local refusal is; the buttons follow the Core.
    void remoteTransmitRefused(const QString& reason);
    void remoteMicSourceStateChanged();

    // ── Plan 4 D8: per-profile TX filter relay signal ─────────────────────────
    //
    // Intermediate signal that carries the 3-arg filter request (audio Hz + mode)
    // from the main-thread lambda (subscribed to TransmitModel::filterChanged)
    // across to TxChannel::requestFilterChange on the audio thread.
    //
    // TransmitModel lives on the main thread; TxChannel lives on TxWorkerThread
    // after RadioModel's moveToThread call.  A direct lambda-connect from
    // TransmitModel::filterChanged → m_txChannel lambda would fire on the main
    // thread (because TransmitModel is the sender and its thread is main).
    // Routing through this intermediate signal ensures Qt auto-connection
    // selects QueuedConnection for TxChannel::requestFilterChange, which runs
    // the slot on TxWorkerThread where the debounce timer is live.
    //
    // NereusSDR-original glue (no Thetis equivalent needed).
    void txFilterRequest(int audioLowHz, int audioHighHz, NereusSDR::DSPMode mode);

    // ── Phase 3R Task I5: RadeChannel slot-graph re-emit signals ─────────────
    //
    // Re-emitted after RadioModel internalises the corresponding
    // RadeChannel signal. UI consumers (RadeApplet, status bar SYNC
    // badge, future SNR readout in VfoWidget) subscribe here rather
    // than to the per-channel RadeChannel directly, so they survive
    // mode-swap / channel-rebuild cycles without re-wiring.
    //
    // radeSyncChanged fires only on actual transitions (de-duplicated
    // by onRadeSyncChanged via m_radeSyncedSlices). radeSnrChanged
    // fires on every onRadeSnrChanged invocation: no de-dup here,
    // the slice setter's NaN-aware short-circuit handles repaint thrash.
    void radeSyncChanged(int sliceId, bool synced);
    void radeSnrChanged(int sliceId, float snrDb);

    // Phase 3R Task L2: RADE carrier-frequency offset re-emit. Wired
    // alongside the I5 trio for the RadeApplet freq-offset readout.
    // Sent on every codec tick while locked, changed or not
    // (RadeChannel::processIq), and re-emitted here without de-dup.
    void radeFreqOffsetChanged(int sliceId, float hz);

    // Phase 3P-II: PGXL amplifier presence / state / meter signals.
    // amplifierChanged: fires once on the first statusUpdated from PgxlConnection
    //   (m_hasAmplifier transitions false -> true). present=true only.
    // ampStateChanged: fires whenever m_ampOperate changes (OPERATE-family vs not).
    // ampMetersChanged: fires on each statusUpdated that carries peakfwd + swr keys.
    //   fwd is forward power in watts (dBm input converted: watts = 10^(dbm/10)/1000).
    //   swr is the SWR ratio (return-loss dB input: ratio = 10^(-rl/20), clamped >= 1.0).
    void amplifierChanged(bool present);
    void ampStateChanged();
    void ampMetersChanged(float fwd, float swr);
    /// iPhone app plan Task 77 fix round 2: a Tuner Genius autotune cycle
    /// ended (keyed or not); `deviceId` is the device it was for, empty for
    /// the Core's own.
    /// Round 4: `unkeyedReason`, when it ended without keying for the
    /// amplifier's sake, the words for its device; empty otherwise.
    void tgxlAutotuneEnded(const QByteArray& deviceId, const QString& unkeyedReason);
    /// iPhone app plan Task 77 fix round 3: pgxlSwitchWaitsForTuner()
    /// may have changed (a cycle started or ended, the tuner's sweep).
    /// Round 4: also when an amplifier changeover starts or ends.
    void pgxlSwitchWaitChanged();

    // Phase 3P-III Task 13: cross-vendor external-amp aggregator signals.
    // Both PgxlConnection state transitions and Rf2ksConnection::operateModeUpdated
    // feed externalAmpOperateChanged so SMeterWidget and any other consumer can
    // subscribe once rather than per-brand.
    //
    // externalAmpOperateChanged(bool inOperate):
    //   true  when any connected external amp enters OPERATE.
    //   false when all external amps leave OPERATE (or disconnect).
    //
    // externalAmpFwdSwrUpdated(int forwardW, float swr):
    //   fired for every RF-Kit power snapshot so consumers can feed the TX
    //   needle without knowing which amp brand supplied the reading.
    void externalAmpOperateChanged(bool inOperate);
    void externalAmpFwdSwrUpdated(int forwardW, float swr);

private slots:
    void onConnectionStateChanged(NereusSDR::ConnectionState state);
    void noteOfflineReceiverPropertyEdit();

    // ── #202 deep-fix: Audio.RadioVolume setter analogue ─────────────────────
    //
    // Mirrors Thetis audio.cs:262-271 [v2.10.3.13]:
    //   public static double RadioVolume {
    //       set {
    //           radio_volume = value;
    //           NetworkIO.SetOutputPower((float)(value * 1.02));   // wire byte
    //           cmaster.CMSetTXOutputLevel();                       // IQ scalar
    //       }
    //   }
    //
    // Connected to TransmitModel::audioVolumeChanged so every call to
    // setPowerUsingTargetDbm (drive slider, TUNE-on, TUN-off restore,
    // two-tone) and any future audio_volume mutator pumps the wire byte +
    // IQ scalar uniformly.  Also re-pumped on TransmitModel::
    // swrProtectFactorChanged (mirrors console.cs:26102-26109 [v2.10.3.13]
    // `Audio.RadioVolume = Audio.RadioVolume` re-emission when SWRProtect
    // changes mid-TX).
    //
    // Wire byte composition is byte-for-byte equivalent to Thetis
    // NetworkIO.cs:201-211 [v2.10.3.13]:
    //   if (f < 0.0) f = 0.0F;
    //   if (f >= 1.0) f = 1.0F;
    //   int i = (int)(255 * f * _swr_protect);
    // IQ scalar mirrors cmaster.cs:1115-1119 [v2.10.3.13]:
    //   double level = Audio.RadioVolume * Audio.HighSWRScale;
    // where HighSWRScale is set to 1.0 once at console.cs:29194 and never
    // reassigned anywhere in baseline Thetis — effectively no-op.
    void pumpAudioVolume(double audioVolume);

    /// Recompute the drive byte through the NORMAL (non-tune) power path.
    ///
    /// Ports the restore mi0bot performs on every MOX-to-TX transition, at
    /// console.cs:30272 [v2.10.3.13-beta2] inside chkMOX_CheckedChanged2's
    /// `if (tx)` branch:
    ///
    ///   if (!chkTUN.Checked && !chk2TONE.Checked) ptbPWR_Scroll(this, EventArgs.Empty);
    ///   //MW0LGE_22b need this here as we may have adjusted power via tune slider when not in mox
    ///
    /// `ptbPWR_Scroll` calls setPowerFromDriveSlider (console.cs:47601-47607),
    /// which is SetPowerUsingTargetDBM with bFromTune=false. Without it a
    /// preceding TUNE leaves its drive value in place for the next normal
    /// transmit. On the HL2 that value is 0, because the mi0bot carve-out at
    /// console.cs:47660-47673 deliberately zeroes the drive byte for tune
    /// powers at or below 51 and carries the level in the post-gen tone
    /// magnitude instead, so a TUNE silences every following SSB transmit.
    void restoreNormalTxDrive();
    /// Thetis ptbPWR_Scroll's power path (console.cs:28682-28692
    /// [v2.10.3.15]): SetPowerUsingTargetDBM with bFromTune=false and no
    /// TUNE or two-tone guard, so the transmit model's own tx mode picks
    /// the tune or two-tone drive source while either runs. The drive half
    /// of drivePowerScroll.
    void applyDriveSliderPower();
    /// All of Thetis ptbPWR_Scroll (console.cs:28682-28693 [v2.10.3.15]):
    /// applyDriveSliderPower, then the per-band save
    /// `power_by_band[(int)_tx_band] = ptbPWR.Value;` in every tx mode and
    /// with or without a radio. The PWR setter (console.cs:18437-18448) runs
    /// it, so TransmitModel::powerChanged does too. Only a model that owns
    /// its radio saves; a remote window gets the Core's saved values.
    void drivePowerScroll();

    // ── Phase 3J-2 H2: per-source spot-adapter slots ────────────────────────
    //
    // Each ingest client emits spotReceived(DxSpot); the adapter slot
    // translates that into the QMap<QString,QString> kvs shape
    // SpotModel::applySpotStatus expects (TCI-style sink). Per-source
    // lifetime and color defaults are read from AppSettings under the
    // <Source>SpotLifetimeSec / <Source>SpotColor key family.
    //
    // The WSJT-X adapter is special: it also pushes to RxDecodeModel so the
    // "what my radio just heard" feed tracks live decodes (NereusSDR design;
    // freedv-gui has no equivalent feed). WsjtxClient does not have a
    // separate decodeReceived signal; the single spotReceived signal is the
    // source for both sinks.
    void onClusterSpotReceived(const NereusSDR::DxSpot& spot);
    void onRbnSpotReceived(const NereusSDR::DxSpot& spot);
    void onWsjtxSpotReceived(const NereusSDR::DxSpot& spot);
    void onSpotCollectorSpotReceived(const NereusSDR::DxSpot& spot);
    void onPotaSpotReceived(const NereusSDR::DxSpot& spot);
    void onFreeDvReporterSpotReceived(const NereusSDR::DxSpot& spot);
    void onPskReporterSpotReceived(const NereusSDR::DxSpot& spot);

    // Phase 3P-II Task 19: PGXL status update handler.
    // Called on every statusUpdated from PgxlConnection. On first call sets
    // m_hasAmplifier and emits amplifierChanged(true). Parses the "state" key
    // to update m_ampOperate and emits ampStateChanged() on transition. Parses
    // "peakfwd" (dBm) and "swr" (return-loss dB) and emits ampMetersChanged.
    void onPgxlStatus(const QMap<QString, QString>& kvs);

    // Phase 3P-II Task 62: runs the amplifierCreate + flexradioPair +
    // enableKeepalive sequence once PgxlConnection reports connected.
    // Reads PGXL_PairAttempt / PGXL_FlexAmpSlice / PGXL_TxAnt / PGXL_AntMap
    // from AppSettings. Serial is "NereusSDR-<macAddress>".
    void onPgxlConnected();

    // Phase 3P-II Phase 4 Task 96: auto-recall TGXL tune memory when the
    // TX-bound slice crosses a band boundary. Connected to
    // SliceModel::bandChanged from addSlice(). Fires only when
    // TGXL_AutoTuneMemoryRecall == "True" and a stored entry exists for
    // (activeAntenna, newBand).  Falls back to issuing "tune start" per
    // design bench-caveat (absolute relay-write API not yet confirmed).
    void onSliceBandChanged(SliceModel* source, NereusSDR::Band band);

private:
    // Fix round 1 (minor 4): a remote window whose own link to the Core
    // is down (no link, or stationLinkReady() false).
    bool remoteCoreLinkDown() const;
    void updateAutoAgc();

    // RADE end-of-over callsigns (MoxController::setEndOfOverTail). Starts
    // the tail when this release is an operator's and the transmitter runs
    // RADE: queues the end-of-over frame, carrying the station callsign
    // FreeDV Reporter uses, on the TX-bound slice's RADE channel and arms
    // the TX worker's drained notice. False (no tail) otherwise.
    bool startRadeEndOfOverTail();
    // Fix wave (RADE EOO): after the EOO's samples leave the channel (now,
    // or once the decoder releases the codec): flush the 24 -> 48 kHz
    // stage and arm the TX worker's drained notice.
    void finishRadeEndOfOverTailQueue();
    // The TX worker's RADE connections: the path latch on MOX-on and the
    // tail's drained notice. Called where the worker is created.
    void wireTxWorkerRade(TxWorkerThread* worker);
    // The tail ended (sent, timed out, stopped or cut by a new key).
    void onEndOfOverTailChanged(bool active);
    // At every unkey's drain: the RADE TX audio the over left (the worker's
    // queue, the 24 -> 48 kHz resampler, the channel's encoder state).
    void dropRadeTxAudio();
    // While a tail runs: the TX slice's dspModeChanged and the arbiter's
    // txBoundSliceChanged, each ending it.
    QMetaObject::Connection m_endOfOverTailModeWatch;
    // Set around teardownConnection's unkey: a disconnect sends no tail,
    // and does not wait for the send ring (G-05).
    bool m_refuseEndOfOverTail{false};
    QMetaObject::Connection m_endOfOverTailSliceWatch;

    // R-R3-49 / R-R3-47: false, with the reason, when a window may not
    // switch the Core's Tuner Genius now (not the Core's tuner, the radio
    // on the air, or no tuner admitted).
    bool stationTgxlControlAllowed(QString* reason) const;
    // R-R3-49 (parity Task 9): the Power Genius's OPERATE gate: a Core that
    // owns its accessories, off the air, connected to the amp.
    bool stationPgxlControlAllowed(QString* reason, bool standbyRequested = false) const;
    // R-R3-49 (parity Task 10): the RF-Kit's gate before its controller's
    // own: a Core that owns its accessories, off the air.
    bool stationRfKitControlAllowed(QString* reason) const;
    // R-R3-49 (parity Tasks 8 and 9): the Core's own Scan LAN, one
    // LanDiscovery child named `objectName` that keeps the announcements of
    // `products` for `windowMs` and calls `done` once with the JSON array.
    // Group B fix wave (M2): one scan per `objectName` at a time; a
    // request while it listens joins it and gets the same answer.
    void startStationLanScan(const QString& objectName, const QStringList& products,
                             int windowMs, std::function<void(const QString&)> done);
    // R-R3-49 (parity Task 2): the transmit band for tunePowerForTxBand,
    // and the Core's transmit chain wiring (moved from connectToRadio()).
    void refreshTransmitTuneBand();
    // The transmit slice's band, else m_lastBand: the band refreshTransmitTuneBand
    // hands the TUNE path and applyTransmitBand.
    Band transmitSliceBand() const;
    // Port of Thetis's TXBand setter (console.cs:17511-17545 [v2.10.3.15]):
    // on a transmit band change, saves PWR into the old band's slot and
    // loads the new band's stored power into PWR. `initializing` is the
    // connect-time call after the per-band store loads. Local role only.
    void applyTransmitBand(Band band, bool initializing);
    // m_txBand once known, else transmitSliceBand(): the band the drive math
    // reads and saves PWR to (Thetis _tx_band).
    Band driveTxBand() const;
    // The first-MOX audioVolume seed: PWR's drive, pushed once the TxChannel
    // exists so the first key is not silent (txSetup in connectToRadio).
    void seedInitialAudioVolume();
    // R-R3-49 (parity Task 3): the Core's MicProfileManager's active profile
    // and list onto `transmit` (activeTxProfile, txProfilesJson).
    void publishTxProfiles();
    // R-R3-49 (parity Task 3): a window's profile manager mirrors the Core's
    // profiles and asks the Core through the station link.
    void mirrorTxProfilesFromStation();
    void wireMicAndMonitorToTransmit();
    void wireTransmitProcessingChain();

    // Phase 3Q-1: drives the RadioModel-level connection state machine.
    // Guards against redundant transitions (no emit if state unchanged).
    void setConnectionState(ConnectionState s);

    // ── Per-radio peripherals lifecycle ────────────────────────────────────
    // Drive the RF-Kit / 4O3A / PGXL / TGXL connections from the connection
    // state machine.  applyPeripheralsForCurrentMac() runs after the radio
    // reports Connected (and m_lastRadioInfo.macAddress is populated);
    // teardownPeripherals() runs on Disconnected / LinkLost.
    //
    // migratePeripheralGlobalsIfNeeded() is a one-shot that folds the legacy
    // GLOBAL RfKit_* / FourO3A_Enabled / PGXL_Manual* / TGXL_Manual* keys
    // into the currently connected radio's hardware/<mac>/peripherals/
    // scope on the FIRST Connected event after this code lands.  Subsequent
    // launches see PeripheralsMigrationDone="True" and skip.
    void applyPeripheralsForCurrentMac();
    void teardownPeripherals();
    void migratePeripheralGlobalsIfNeeded();

    // v0.4.1 hotfix: single point that fans the connected hardware
    // HPSDRModel out to every sub-model that needs it.  Updates
    // m_hardwareProfile, then pushes the model into TransmitModel
    // (issue #175 HL2 mi0bot polymorphic-clamp setup) AND
    // ReceiverManager (drives per-board codec dispatch in
    // applyPureSignalDdcConfig — without this fan-out, the codec
    // sees the default HPSDRModel::HPSDR enum, falls through its
    // model switch's default branch, and emits an empty PsDdcConfig
    // → PsccPump never activates → PureSignal correction never
    // lands).  Called from connectToRadio() and the test-only
    // setHpsdrModelForTest() seam so production and tests stay in
    // sync. Plan Task 15: the board is the radio's own (RadioInfo::boardType)
    // so the HL2 receive-only kit keeps its row under the HL2 model
    // (profileForRadio); the test seam passes Unknown, which is
    // profileForModel(m) exactly.
    void applyHpsdrModel(HPSDRModel m, HPSDRHW board = HPSDRHW::Unknown);

    // Pushes AlexController's per-band antenna state to the connection.
    // Full port of Thetis HPSDR/Alex.cs:310-413 UpdateAlexAntSelection.
    // Phase 3P-I-b (T6): adds isTx branch, Ext1/Ext2OnTx mapping, xvtrActive
    // gating, and rxOutOverride clamp. MOX coupling and Aries clamp deferred
    // to Phase 3M-1 (TX bring-up). isTx defaults to false so existing callers
    // are unaffected.
    //
    // Source: Thetis HPSDR/Alex.cs:310-413 [@501e3f5].
    void applyAlexAntennaForBand(NereusSDR::Band band, bool isTx = false);
    // Reconciles the TX-bound slice's stored antenna intent into the
    // per-band Alex state. Called at every authority boundary (slice edit,
    // TX handoff, and immediately before MOX routing).
    void applyTxAntennaFromBoundSlice();

    void connectToRadioImpl(const RadioInfo& info, bool preserveSlices);
    void wireConnectionSignals(int wdspInSize);
    // The ReceiverManager -> RadioConnection hardware pushes (live slots,
    // receiver count, per-slot frequency). Part of wireConnectionSignals;
    // separate so a test can install exactly the production wiring.
    void wireReceiverManagerHardwarePushes();
    void wireWidebandConnection();
    /// Wire one slice's property changes to its OWN WDSP channel and to the
    /// radio. Call for every slice, not just the active one: this used to
    /// read m_activeSlice and run once, leaving 65 per-slice DSP handlers
    /// (AGC, filter, mode, NB, SNB, APF, RIT/XIT, squelch, mute, pan and the
    /// whole NR parameter set) wired for Slice A alone -- and every one of
    /// them writing rxChannel(0). Idempotent per slice: the handlers use
    /// Qt::UniqueConnection-safe member targets or are wired once at
    /// addSlice time.
    void wireSliceSignals(SliceModel* slice);
    /// Band tracking's crossing for `slice` (m_lastBand, the per-band
    /// antenna switch and the slice's antenna labels); iPhone app Task 75
    /// keeps the receive antenna while another device listens (ruling
    /// 5.11a).
    void crossBandForSlice(SliceModel* slice, Band newBand);
    bool receiveAntennaDiffers(Band a, Band b) const;
    QString receiveAntennaLabel(Band band) const;
    QList<QByteArray> devicesListeningThroughRelay(const SliceModel* tuner) const;
    void wireBandTrackingForTest(SliceModel* slice);

    /// The transmitter's RADE passband snap: entering RADE_U/RADE_L sets the
    /// transmit filter to 650..2350 Hz; leaving RADE restores 100..3900 Hz
    /// only when the filter is still exactly the RADE passband. Callers own
    /// the "is this the TX-bound slice" check.
    void snapTransmitFilterForMode(DSPMode mode);

    /// Connect NotchModel's mutation signals to the per-channel WDSP
    /// fan-out. Called once from the ctor; NotchModel outlives every
    /// connection.
    void wireNotchModel();

    /// Push the full notch state at one channel: the list, the master run
    /// flag, the auto-increase flag and the NBP tune frequency. `channelId`
    /// is also the slice index (Sub-Epic I invariant), which is how the
    /// hosting stream's centre is resolved for the tune frequency
    /// (design section 4.1).
    void syncNotchesToChannel(RxChannel* ch, int channelId);

    /// Every WDSP RX channel that currently backs a slice. The fan-out
    /// target set for a live notch mutation (design section 6.3).
    QVector<RxChannel*> sliceRxChannels() const;

    /// Design section 6.2: our list position IS the WDSP notch index, so a
    /// count divergence is a correctness bug. Detect and recover with a full
    /// resync rather than assert, which a release build compiles out.
    void reconcileNotchCount(RxChannel* ch);

    // Recomputes the transmit frequency from the TX-bound slice and pushes it
    // at the connection. The single place that answers "what frequency is the
    // radio transmitting on", so the Alex TX low-pass, the TX NCO and the
    // drive-level band gate cannot disagree about it.
    //
    // Mirrors Thetis UpdateTXDDSFreq(), which likewise recomputes from
    // tx_dds_freq_mhz and fans out to setAlexLPF(..., true) and
    // NetworkIO.VFOfreq(0, tx_dds_freq_mhz, 1) together
    // (console.cs:15464-15485 [v2.10.3.15]).
    //
    // XIT is included and RIT is not, per Thetis console.cs:31782-31784
    // [v2.10.3.15]: udXIT lands on tx_freq, udRIT on rx_freq.
    void pushTxFrequencyFromTxSlice();

    // The total WDSP shift for a slice: the allocator's offset from its
    // hosting stream's centre, plus RIT, plus the per-mode DIG click-tune
    // offset. Five sites push the shift (bindSliceToStream,
    // activateSliceChannel, reshiftSlicesOnStream,
    // commitStreamSampleRateChange and the RIT/DIG lambda in
    // wireSliceSignals) and they used to disagree about which terms belonged
    // in it, so each clobbered the others'. Toggling RIT on a shifted slice
    // threw away the stream offset; retuning with RIT on threw away the RIT.
    //
    // Reads slice->shiftOffsetHz(), so every caller must commit the stream
    // term to the model before calling.
    // See docs/architecture/2026-07-28-tunable-notch-filter-design.md 4.4.
    double composedShiftHz(const SliceModel* slice) const;
    /// Shift derived from an EXPLICIT stream centre, so it cannot disagree
    /// with the NOTCHDB::tunefreq written alongside it. Design section 4.1.
    double composedShiftHz(const SliceModel* slice, double streamCentreHz) const;
    /// The ONLY writer of the notch RF origin. Writes tunefreq and shift
    /// together from one centre; WDSP sums them (nbp.c:192).
    void pushNotchOrigin(SliceModel* slice, RxChannel* ch, double streamCentreHz);

    /// Coalesces notch edits during a drag. RXANBPEditNotch runs a full
    /// UpdateNBPFilters (an FFT per partition at nc=4096, plus a bpsnba
    /// recalculation) and swaps the masks under the DSP lock, so pushing one
    /// per mouse-move costs ~50 filter redesigns per second PER CHANNEL.
    /// Thetis does push per move (console.cs:49967 [v2.10.3.15]) but for one
    /// notch on one channel; multi-pan multiplies that by the channel count.
    /// The marker is drawn from the model and does not wait for this, so the
    /// only thing rate-limited is the DSP redesign.
    void scheduleNotchEditPush(int id);
    void flushNotchEditPush();

    QTimer* m_notchEditTimer{nullptr};
    QSet<int> m_pendingNotchEdits;

    // R-R3-21: coalesces remote DSP > Options RX writes into one apply per
    // slice. See scheduleRemoteDspOptionsApply.
    void flushRemoteDspOptionsApply();

    QTimer* m_dspOptionsApplyTimer{nullptr};
    QSet<QString> m_pendingDspOptionsGroups;

    // R-R3-46: coalesces remote hardware settings writes into one reload
    // per controller. See scheduleRemoteHardwareApply.
    void flushRemoteHardwareApply();

    QTimer* m_hardwareApplyTimer{nullptr};
    QSet<QString> m_pendingHardwareReloads;
    std::function<void(const QString&)> m_hardwareApplyObserverForTest;
    std::function<void(int, DSPMode)> m_dspOptionsApplyObserverForTest;
    // R-R3-49 (parity Task 1): the TX groups queued by a window's
    // DspOptions<Setting><Mode>Tx write, and the test observer.
    QSet<QString> m_pendingDspOptionsTxGroups;
    std::function<void(DSPMode)> m_dspOptionsTxApplyObserverForTest;
    std::function<void(DSPMode)> m_txKeyDspOptionsObserverForTest;
    // Group B fix wave: the test observer of PureSignal hearing the unkey
    // on the main thread (wireTxChannelKeying).
    // Group A follow-up (group B fix wave): the parametric TX EQ's pushes,
    // coalesced to Thetis's 100 ms tick (eqform.cs:3613-3614 [v2.10.3.15]).
    static constexpr int kTxEqPushCoalesceMs = 100;
    QTimer* m_txEqPushTimer{nullptr};
    // Group B fix wave: applies a held DSP > Options TX change and a held
    // PA profile or calibration reload once the on-air rule clears.
    void releaseHeldOnAirWork();
    std::function<void()> m_txaFlushedPureSignalObserverForTest;

    // The connect-time DDC seed, factored out of the wireSliceSignals
    // singleShot so it can be driven without a live connection. Commands the
    // centre of whichever stream hosts `slice`, then re-seeds the TX NCO.
    // See docs/architecture/2026-07-28-tunable-notch-filter-design.md 4.5.
    void seedConnectFrequency(SliceModel* slice);

    // The frequency a slice would actually transmit on: its dial plus XIT.
    //
    // One answer for three callers, because they had drifted apart. The
    // transmit-frequency push folded XIT in; both TUNE arms read the raw
    // dial. Keying TUNE with XIT set therefore put the carrier somewhere the
    // transmit chain had not been told about, and with XIT straddling a
    // filter edge that included the Alex transmit low-pass.
    //
    // From Thetis console.cs:31774-31783 [v2.10.3.15]
    //   double tx_freq = freq;
    //   ...
    //   if (chkXIT.Checked) tx_freq += (int)udXIT.Value * 0.000001;
    // The TUNE offsets are applied to that same tx_freq afterwards
    // (console.cs:31845-31860 [v2.10.3.15]) before it becomes
    // tx_dds_freq_mhz at console.cs:31891, so TUNE transmits on the
    // XIT-shifted frequency too.
    //
    // Returns 0 for a null slice, and clamps at 0 rather than wrapping.
    void teardownConnection();

    // Derives the 16-digit dashed FlexRadio-style serial number from the
    // radio's MAC address. SHA-256(mac + salt) -> first 8 bytes -> uint64 ->
    // mod 10^16 -> "XXXX-XXXX-XXXX-XXXX". Used by both onPgxlConnected() and
    // connectToRadio() (FlexRadio discovery beacon) so the serial matches in
    // both contexts. If PGXL_FlexRadioSerial is set in AppSettings, returns
    // that override instead of the derived value.
    QString derivedFlexSerial(const QString& mac) const;

    // Issue #182 — wire TransmitModel::micPttDisabledChanged →
    // RadioConnection::setMicPTTDisabled and prime the connection with the
    // current model value once.  Extracted so the connect() can be exercised
    // in isolation by tst_radio_model_mic_ptt_wire without needing to spin
    // up the full DSP-thread pipeline that wireConnectionSignals starts.
    void connectMicPttDisabledSignal();
    // Radio codec lane: TransmitModel's mic boost, line in, XLR, tip/ring
    // and bias reach the connection on connect and on every change (Thetis
    // SetMicGain and the Setup mic panel). Called from wireConnectionSignals.
    void connectMicCodecSignals();
    // Radio codec (2026-09-30): the station's program to the radio's own
    // speaker out (AudioEngine::setRadioOutputTap into
    // RadioConnection::pushRadioAudio), for a connection that carries it.
    // Called from wireConnectionSignals; teardownConnection removes it
    // before the connection goes.
    void connectRadioSpeakerOutput();
    void disconnectRadioSpeakerOutput();
    // Task 13: the radio's user digital inputs reach TxInhibitMonitor
    // (PollTXInhibit, console.cs:25849-25887 [v2.10.3.15]). Called from
    // wireConnectionSignals.
    void connectTxInhibitInput();
    // Plan Task 14 fix wave: the connection's composed band outputs reach
    // bandOutputsByte (onBandOutputsComposed). Called from
    // wireConnectionSignals.
    void connectBandOutputsReport();
    // Pushes the saved HL2 options to a P1 connection now and again each
    // time they change (applyHl2Options). Nothing without one.
    void connectHl2OptionsToConnection();
    void onBandOutputsComposed(quint8 ocByte, int band, bool keyed);
    void onAlexLpfBitsComposed(quint8 bits);
    void resetBandOutputs();

    // Issue #177 — deferred completion of the TUN-off path.
    //
    // Called from the rxReady → settle-timer slot wired in the constructor.
    // Performs everything that used to run synchronously inside setTune(false)
    // EXCEPT the MoxController::setTune(false) call: gen1 OFF, DSP-mode
    // restore (CWL/CWU), tune-power restore through the dBm path, TX VFO
    // un-offset, and the m_isTuning / m_pendingTuneOff state clears.
    //
    // Cite: Thetis console.cs:30106-30148 [v2.10.3.13] — chkTUN_CheckedChanged
    // TUN-off branch.  Thetis runs the equivalent block AFTER
    // chkMOX.Checked = false (which is synchronous and blocks ~30 ms inside
    // chkMOX_CheckedChanged2) and AFTER `await Task.Delay(100)`.  In NereusSDR
    // this method is invoked from a QTimer::singleShot(m_tuneOffSettleMs)
    // chained off MoxController::rxReady, so the same total ~130 ms gap
    // separates the user's click from gen1 going off.
    void completeTuneOff();

    // Task 7 fix wave, I2: pushes TX inhibit (TxInhibitMonitor) and the PA
    // trip (paTripped()) into MoxController, whose gates refuse every key
    // while either is set and unkey an active transmission; with either set
    // it also turns TUN and two-tone off. Thetis TXInhibit setter
    // (console.cs:15341-15363 [v2.10.3.15]) and _ganymede_pa_issue
    // (console.cs:25470, 29364-29371).
    void applyTxKeyBlock();

    // Task 16: recompute isRxOnly from the setting and the board, push it
    // to MoxController through applyTxKeyBlock, and emit rxOnlyChanged on
    // a change.
    void applyRxOnly();

    // P1 full-parity §3.4 — per-sample PA telemetry handler.
    // Applies per-board ADC→watts scaling (scaleFwdPowerWatts /
    // scaleRevPowerWatts / scalePaVolts / scalePaAmps), routes the FWD
    // reading through CalibrationController::calibratedFwdPowerWatts()
    // (Thetis console.cs:6691-6724 CalibratedPAPower [v2.10.3.13]) and
    // publishes the calibrated values to RadioStatus + SwrProtectionController.
    //
    // Wired by wireConnectionSignals to RadioConnection::paTelemetryUpdated
    // via a thin forwarding lambda.  Extracted from that lambda so the test
    // hook handlePaTelemetryForTest can drive it directly without spinning
    // up the full wireConnectionSignals DSP-thread pipeline.
    void handlePaTelemetry(quint16 fwdRaw, quint16 revRaw, quint16 exciterRaw,
                           quint16 userAdc0Raw, quint16 userAdc1Raw,
                           quint16 supplyRaw);
    void saveSliceState(SliceModel* slice);
    void scheduleSettingsSave(SliceModel* slice = nullptr);
    void wireNnrSettings(SliceModel* slice);
    void applyNnrStateToChannel(SliceModel* slice, RxChannel* channel);
    // R-R3-40: the runtime NNR limit (see governNnrLoad).
    void governNnrLoadWith(qint64 nowMs,
                           const std::function<std::optional<double>(int)>& load);
    void applyNnrLimit(SliceModel* slice, NnrLimit limit);
    void clearNnrLimit(SliceModel* slice);
    // Follow-up item 1 (R-R3-21): the plain reason NR3 cannot run here, and
    // the rule that a slice never holds NR3 while this Core has no usable
    // NR3 model (NR off, the reason set). Local role only.
    QString nr3CannotRunReason() const;
    void turnOffNr3WithoutModel(SliceModel* slice);
    // R-R3-49, Sub-epic C-1: DFNR's counterparts. The reason this Core
    // cannot run DFNR (no DFNR in the build, the model missing, or failed
    // to load); a slice holding DFNR the Core cannot run turns it off.
    static QString dfnrCannotRunReason(bool modelMissing);
    void turnOffDfnrWithoutModel(SliceModel* slice);
    // A channel's RxChannel::dfnrUnavailable (queued from the receive lane).
    void onRxChannelDfnrUnavailable(bool modelMissing);
#ifdef NEREUS_BUILD_TESTS
public:
    // R-R3-49: what a channel's failed first DFNR load does to the model.
    void reportDfnrUnavailableForTest(bool modelMissing)
    {
        onRxChannelDfnrUnavailable(modelMissing);
    }
private:
#endif
public:
    // R-R3-49, Sub-epic C-1: the plain reasons MNR and BNR cannot run, for
    // the Core's refusals and the VFO flag's disabled MNR button. MNR runs
    // only on a Mac (a Core built without it sends this as mnrStatus); BNR
    // is in no build and has no control (tx-followup-4), so its reason is
    // only the refusal's, which holds everywhere.
    static QString mnrCannotRunReason();
    static QString bnrCannotRunReason();
    // Why the noise filter in `slot` cannot run for this model's receivers,
    // in plain words, or empty when it can. DFNR and MNR follow this
    // model's DspAssetService (the Core's, mirrored, in a remote window);
    // BNR is in no build. Every other filter returns empty.
    QString nrCannotRunReason(NrSlot slot) const;
    // The same for a flag with no model: this build alone decides.
    static QString nrCannotRunInThisBuildReason(NrSlot slot);
    // Whether this build can run BNR (never, today: HAVE_BNR is not set).
    static constexpr bool bnrBuilt()
    {
#ifdef HAVE_BNR
        return true;
#else
        return false;
#endif
    }
private:
    // A slice holding MNR or BNR the Core cannot run turns it off, with the
    // reason, as turnOffDfnrWithoutModel does. Local role only.
    void turnOffNrThatCannotRun(SliceModel* slice);

public:
    // Force-run any pending coalesced slice save synchronously. Call this
    // from app-quit paths (MainWindow::closeEvent, aboutToQuit) and at the
    // top of teardownConnection() so the 500 ms debounce in
    // scheduleSettingsSave() can't swallow the user's last AF / step / freq
    // tweak when they immediately close the app. No-op when nothing's
    // pending. Idempotent — calling repeatedly is safe.
    void flushPendingSettingsSave();
    /// iPhone app Task 73: the coalesced settings save, for a store the
    /// Core's session server changed (a device's saved slices).
    void requestSettingsSave() { scheduleSettingsSave(); }
    QString settingsSaveError() const { return m_settingsSaveError; }
    // Release preflight: prove that the live receiver layout was staged and
    // committed before a handover may destroy this model. Ordinary periodic
    // saves retain their existing retry behavior.
    bool saveForStationHandover(QString* error);
    // Daemon startup calls this after its initial receiver seed, immediately
    // before station ingress can mutate a pending/protected layout.
    void beginStationHandoverEditTracking();
    void applyStationSettingsSaveError(const QString& reason);
    // R-R3-34: seed a validated local layout before any radio/DSP resources
    // exist. Preserves shared QObject identities, descriptor order and IDs.
    // Does not admit streams/decoders or enable persistence. The startup owner
    // must complete resource admission before permitting settings writeback.
    bool hydrateReceiveLayout(const QString& radioMac,
                              const ReceiveLayoutStore::LoadResult& layout,
                              QString* error = nullptr);
    bool receiveLayoutPendingAdmission() const { return m_receiveLayoutPendingAdmission; }
    std::optional<int> restoredRadeReceiveOwner() const { return m_restoredRadeReceiveOwner; }
    // Daemon startup opts into per-radio membership persistence. An empty MAC
    // waits for first discovery; a repeated selected MAC never reloads edits.
    void prepareReceiveLayout(const QString& radioMac);
    bool receiveLayoutOverridesConfiguredCount() const { return m_receiveLayoutOverridesCount; }
    QString receiveLayoutRestoreState() const { return m_receiveLayoutRestoreState; }
    QString receiveLayoutRestoreMessage() const { return m_receiveLayoutRestoreMessage; }
    bool applyStationReceiveLayoutStatus(const QByteArray& property, const QString& value);
    // Called after the radio/worker startup boundary (or the existing primed
    // daemon fixture). Binding and any required RADE owner must be accepted.
    void completeReceiveLayoutStartup();
    bool setNnrDiagnosticMode(int sliceId, int testMode, int outputMode,
                              QString* reason = nullptr);

    // Restore a slice's persisted state from AppSettings.  Public so unit
    // tests can drive it without spinning up the full connectToRadio()
    // pipeline.  Production callers: connectToRadio() at RadioModel.cpp
    // line ~1377 — fires once per session per slice on Connected. Emits
    // sliceStateRestored(index) on completion (see comment on the signal).
    void loadSliceState(SliceModel* slice);

    // Issue #153 sub-bug 2 — push the TX-bound slice's DSPMode plus the
    // TransmitModel's positive audio-space filter cutoffs to TxChannel.
    // No-op if no TX binding resolves.
    //
    // SliceModel filter bounds are RX/IQ-space and signed for LSB-family
    // modes, so they are deliberately not a TX bandpass source.
    //
    // Read happens on RadioModel's main thread; the TxChannel setter
    // call is queued to TxWorkerThread via QMetaObject::invokeMethod
    // (receiver=m_txChannel) so the receiver-thread invariant holds —
    // mirrors the F.1 / F.2 / H.1 wires inside connectToRadio's txSetup
    // lambda.  Emits txModeAndBandpassPushed(mode, audioLow, audioHigh)
    // before the queued dispatch as a test/diagnostic observation hook
    // (fires even when m_txChannel is null so test fixtures can drive
    // the helper without standing up the full TX pipeline).
    //
    // Wire targets (set up inside the txSetup lambda + wireSliceSignals):
    //   - createTxChannel success → pushTxModeAndBandpass (initial seed)
    //   - SliceModel::dspModeChanged → pushTxModeAndBandpass
    //   - MoxController::txAboutToBegin → pushTxModeAndBandpass
    //
    // Source-of-truth: Thetis SetTXFilters at console.cs:8091 +
    // CurrentDSPMode setter at radio.cs:2670-2696 [v2.10.3.13], wired
    // into the mode-change handler at console.cs:33937 [v2.10.3.13].
    // The MOX-engage trigger is NereusSDR's belt-and-suspenders re-seed
    // (Thetis seeds at mode-change only; we additionally re-seed at
    // MOX-engage so prior TUN-state desync cannot starve SSB MOX).
    void pushTxModeAndBandpass();
    // R-IOS-13 (2026-09-27): apply the TX-bound slice's DSP > Options (TX
    // buffer, filter size and type) to the TX channel. Runs at every key
    // (MoxController::txAboutToBegin, before the hardware flip and the TX
    // channel's start), so no key path transmits at the channel's open
    // sizes; a no-op when they are applied already.
    void applyTxDspOptionsBeforeKey();
    void installBandPlanMoxCheck();
    bool receiveOnlyTxOperationsBlocked() const;

    // ── Phase 3F Sub-Epic B Task 16: multi-slice codec glue ─────────────────
    // Build the 5-element codec input array. Phase 3F Sub-Epic I Task 7b:
    // indexed by DDC STREAM, not by slice. Slot [st] is live when the
    // allocator reports stream `st` active; frequencyHz is the stream's
    // window CENTRE (the DDC tunes there; slices sit at shift offsets inside
    // it) and the per-slice flags are folded across slicesOnStream(st).
    // Indexing by slice handed two co-hosted slices two different DDCs,
    // contradicting the sharing model they were bound under.
    // NereusSDR-original; no Thetis equivalent (Thetis builds UpdateDDCs
    // inputs inline in console.cs:8186-8538 [v2.10.3.15]).
    std::array<NereusSDR::SliceConfig, 5> buildStreamConfigsForCodec() const;

    // Phase 3F Sub-Epic I Task 7b: run the per-board codec over the current
    // stream set and return its DdcAssignment. Pure: no wire I/O, no model
    // mutation, safe to call while disconnected. Split out of
    // invokeCodecDdcAssignment so the mapping is testable without a socket.
    //
    // Codec source: the RadioConnection owns the codec and is authoritative
    // whenever a connection object exists; ReceiverManager holds the same
    // non-owning pointer (wired at connect, cleared in reset()) and is the
    // fallback when it does not.
    //
    // std::nullopt when no codec has been selected yet, which is NOT the same
    // fact as an assignment whose streamDdc entries are all -1 and must not be
    // spelled the same way. Bench report 2026-07-31 (JJ, KG4VCF): this used to
    // return an all-idle assignment for "nobody has been asked", and
    // publishDdcAssignment cannot tell that apart from a codec answering that
    // the radio has stopped every stream. connectToRadio binds the slice pool
    // before it installs the codec, so every connect published a fabricated
    // "no DDCs anywhere", deactivated the receiver it had activated forty
    // lines earlier, and dropped every I/Q packet until the operator moved the
    // VFO. See tst_connect_routes_first_iq.
    std::optional<NereusSDR::DdcAssignment> computeDdcAssignment() const;

    /// Phase 3F Sub-Epic I closeout, defect F3: single read of the radio-state
    /// codec inputs (MOX / PureSignal / diversity), so computeDdcAssignment and
    /// describeSuspendedStreams cannot disagree about them.
    NereusSDR::CodecContext currentCodecContext() const;

    /// Whether the radio is running the diversity DDC pair right now.
    /// Extracted from currentCodecContext so republishAlexAdcSlices reads the
    /// same answer the codec branched on, including under the
    /// setDdcContextForTest seam. Two reads of this would be two chances for
    /// the filter decision and the DDC map to disagree about the same
    /// transmit-critical state.
    bool diversityActive() const;
    bool diversityPairAssigned() const;
    QString diversityState() const;
    QString diversityStateForPeer(bool includePattern) const;
    quint64 diversityStateRevision() const { return m_diversityStateRevision; }
    /// Fixed station requester, synchronous on a Local model's thread.
    /// Does not create a host, listener, network identity or session.
    SessionMessage invokeDiversityAsStationDevice(const SessionMessage& invoke);
private:
    friend class StationServer;
    friend class StationSliceOwnershipPolicy;
    // Raw coordinator: only after the shared admitted transaction checks.
    bool setDiversityTarget(int sliceId, QString* reason = nullptr);
    SessionMessage invokeAdmittedDiversityControl(const SessionMessage& invoke,
        StationServer* server, SessionTransport* transport, bool stationEntry);
    QString diversityControlRefusal(const QByteArray& requester, int sliceId,
                                    const StationServer* server) const;
    // Set/cleared only by the canonical Core server, never by a GUI caller.
    QPointer<StationServer> m_diversityStationServer;
public:
    QString diversityEligibility(int sliceId, QString* code = nullptr) const;
    QString legacyDiversityRefusal(int sliceId, bool enabled) const;
    void publishDiversityState(bool structural = true);
    void wireDiversitySlice(SliceModel* slice);
    std::optional<NereusSDR::DdcAssignment> computeDdcAssignmentForContext(
        const NereusSDR::CodecContext& ctx) const;

    /// Reconcile the process-wide WDSP slot and the DSP worker's paired raw-DDC
    /// route against one complete codec assignment. This is the sole start
    /// owner, which keeps source selection and hardware publication atomic from
    /// the model's point of view.
    void reconcileExternalDiversityRoute(
        const NereusSDR::DdcAssignment& assignment);

    /// Resolve the stable target's primary DDC plus the assignment's DDC0 sync
    /// partner. Returns false when PureSignal owns the pair or the codec did not
    /// publish two equal-rate diversity legs. The sync partner need not also
    /// appear in ddcEnable; Hermes-class Thetis assignments enable DDC0 and
    /// activate DDC1 through syncEnable alone.
    bool resolveExternalDiversitySources(
        const NereusSDR::DdcAssignment& assignment,
        const SliceModel* target, int& primaryDdc, int& secondaryDdc) const;

    /// Apply the target's current phase/gain rotation to an already-created
    /// external-diversity slot.
    void configureExternalDiversityRotation(const SliceModel* target);

    // Phase 3F Sub-Epic I Task 7b: publish a computed assignment onto the
    // client-side model. Routes each stream's hardware DDC to its logical
    // receiver via ReceiverManager::setDdcMapping, stamps every slice with
    // the DDC of the stream hosting it, and reconciles ReceiverManager's
    // per-stream active flag against slicesOnStream(). No wire I/O, so it
    // runs whether or not a connection exists.
    void publishDdcAssignment(const NereusSDR::DdcAssignment& assignment);

    // Drive the per-board codec's applyDdcAssignment(), forward the result to
    // P2RadioConnection (P1 wire integration deferred to Sub-Epic C), then
    // publish the mapping client-side. The wire push is gated on an actual
    // connection; the client-side publish is not.
    void invokeCodecDdcAssignment();

    /// Plain-English sentence naming the affected slice letters and why they
    /// lost their receiver. Empty when nothing is suspended.
    QString describeSuspendedStreams(const QVector<int>& streams) const;

    /// Publish one I/Q frame to the two taps: rawIqDataForStream always,
    /// rawIqData only for stream zero.
    ///
    /// Codex review round 7, PR #293. The untagged rawIqData signal
    /// predates multi-stream, and its only subscriber (TciServer) still
    /// hardcodes receiver 0. Forwarding every stream through it fed a
    /// single TCI IQ client frames from unrelated frequencies under one
    /// header. Named rather than left inline so the rule has somewhere to
    /// be stated and somewhere to be tested.
    void forkIqToTaps(int receiverIndex, const QVector<float>& samples);

    // ── Phase 3F Sub-Epic I: slice-to-stream binding ───────────────────────

    /// Run the allocator for `slice` at `frequencyHz` and apply the result
    /// (stream binding, shift offset, stream centre, codec recompute).
    /// Returns false and emits sliceAddRejected when the hardware has no
    /// room. Returns false silently when the pool has not been sized yet
    /// (disconnected): there is no DDC to bind to, and a slice with
    /// streamIndex() < 0 is unbound and feeds nothing. Also returns false
    /// silently for a Role::Remote model (remote-daemon R2 Task 5): the
    /// daemon owns the allocator, so this model must never place, retune
    /// or evict a stream of its own.
    /// `preferOwnStream` is forwarded to SliceStreamAllocator::placeSlice on a
    /// first bind, and says the caller wants an independent window rather than
    /// the cheapest placement. Set by the +PAN path; see that header for why a
    /// pan and a slice want different answers. Ignored on a retune, which
    /// already owns a stream.
    bool bindSliceToStream(SliceModel* slice, double frequencyHz,
                           bool preferOwnStream = false, int requiredStream = -1);

    /// Mirror a stream's liveness into ReceiverManager's active-receiver set,
    /// which is what decides whether that hardware DDC's samples are forwarded
    /// or dropped. Called from bindSliceToStream on both edges. Idempotent.
    /// See the definition for the bench defect that showed the two were never
    /// connected.
    void syncReceiverToStream(int streamIndex, bool live);

    /// Push the current slice set for `streamIndex` to RxDspWorker and emit
    /// streamBindingsChanged. Called after every bind / unbind.
    void republishStreamBindings(int streamIndex);
    bool streamCtunPinned(int streamIndex) const;
    void setStreamCtunPinned(int streamIndex, bool pinned);
    quint64 streamEpoch(int streamIndex) const;
    void setStreamEpoch(int streamIndex, quint64 epoch);
    void claimStreamEpoch(int streamIndex);
    void retireStream(int streamIndex);
    // iPhone app Task 74: claim a stream, or move a live one, centred on
    // `centreHz` (bindSliceToStream's NewStream / RetunedStream arm).
    void activateStreamAt(int streamIndex, double centreHz);

    /// Emit ddcAssignmentRequested and drive the per-board codec recompute.
    void requestDdcAssignment();

    /// Phase 3F: group the live slices by the ADC their stream sits on, hand
    /// each group to AlexController::notifySlicesOnAdc, and push the resulting
    /// per-chain band-pass decision at the connection.
    ///
    /// Closes the gap CT1IQI reported on PR #293: the per-ADC analysis existed
    /// but had no producer and no consumer, so the wire took its HPF from
    /// whichever receiver was retuned last and a second slice on another band
    /// made the first one deaf.
    void republishAlexAdcSlices();

    /// Plan Task 14: tell the connection the VFO frequency of the slice each
    /// hardware receiver slot serves, so the OC outputs take their band from
    /// a VFO (Thetis BandByFreq(VFOAFreq)) and not from a DDC centre, which
    /// differs under CTUN. Where several slices share a slot, the lowest
    /// slice letter speaks for it, as VFO A does in Thetis. Runs on the same
    /// triggers as republishAlexAdcSlices.
    void republishReceiverVfoFrequencies();
    QVector<quint64> receiverVfoHzBySlot() const;

    /// Phase 3F Sub-Epic I closeout, defect H1: put the DSP side of the pool
    /// back in step with the allocator after anything moves a stream's rate
    /// or moves a slice between streams.
    ///
    /// Two halves of one geometry, both derived from the stream's rate
    /// through the single bufferSizeForRate() in the tree:
    ///   * RxDspWorker's accumulator drain threshold for that stream, and
    ///   * SetInputSamplerate / SetInputBuffsize on the WDSP channel of every
    ///     slice bound to it.
    ///
    /// This is ChannelMaster's SetXcmInrate, split across the two objects
    /// NereusSDR keeps the state in:
    ///   From Thetis cmaster.c:461,473-475 [v2.10.3.15]
    ///     pcm->xcm_insize[in_id] = getbuffsize (rate);
    ///     for (i = 0; i < pcm->cmSubRCVR; i++) {
    ///         SetInputSamplerate (chid (in_id, i), rate);
    ///         SetInputBuffsize (chid (in_id, i), pcm->xcm_insize[in_id]);
    ///     }
    ///
    /// The two must never disagree while a drain can run: fexchange2 copies
    /// ch[channel].in_size samples out of the buffer it is handed
    /// (iobuffs.c:532-536 [WDSP v1.29]) and ignores any count we pass, so a
    /// drain threshold below the channel's in_size reads past the end of the
    /// accumulator. Rather than order the two writes (the safe order inverts
    /// between widening and narrowing), this quiesces the I/Q feed for the
    /// duration exactly as setSampleRateLive steps 2 and 10 do, so no drain
    /// can observe a half-applied geometry at all.
    ///
    /// Cheap and side-effect-free when nothing is out of step, which is every
    /// call on a single-rate radio.
    void applyStreamDspGeometry();

    /// Re-run the codec after a MOX, effective PureSignal, or diversity
    /// transition through the same complete request used by slice binding.
    /// Protocol 2 therefore has one full DdcAssignment wire owner; Protocol 1
    /// keeps its legacy PsDdcConfig wire path while this request publishes
    /// the client-side assignment.
    void refreshDdcAssignmentForRadioState();

    /// Prevent new paired feeds, then stop and destroy WDSP external-diversity
    /// slot 0. The worker clear is synchronous when it lives on the DSP thread,
    /// so no queued raw-I/Q delivery can process against a torn-down slot.
    void stopExternalDiversityRoute();

    /// Streams the codec left without a DDC while slices are still bound to
    /// them. Empty in steady state.
    QVector<int> suspendedStreams() const { return m_suspendedStreams; }

    /// Current allocator geometry for a live stream. These are read-only
    /// production accessors for daemon media consumers; they deliberately
    /// forward the same allocator values exposed by the older test seams
    /// below, rather than deriving geometry from a slice frequency.
    double streamCentreHz(int streamIndex) const {
        return m_streamAllocator.streamCentreHz(streamIndex);
    }
    int streamSampleRateHz(int streamIndex) const {
        return m_streamAllocator.streamSampleRateHz(streamIndex);
    }
    /// Rate activateStreamAt assigns when it claims a previously idle DDC.
    /// Also used by confirmation planners that must predict its window.
    int newStreamSampleRateHz() const {
        return m_connectionSampleRateHz > 0 ? m_connectionSampleRateHz : m_streamDefaultRateHz;
    }
    bool streamActive(int streamIndex) const {
        return m_streamAllocator.isStreamActive(streamIndex);
    }

    /// Phase 3F Sub-Epic I closeout, defect F4 test seam: a stream's window
    /// centre, so a test can reconstruct the frequency WDSP is actually
    /// demodulating (centre + the slice's shift offset) and require it to
    /// match what the VFO reads.
    double streamCentreHzForTest(int streamIndex) const {
        return m_streamAllocator.streamCentreHz(streamIndex);
    }
    int streamSampleRateHzForTest(int streamIndex) const {
        return m_streamAllocator.streamSampleRateHz(streamIndex);
    }
    bool streamActiveForTest(int streamIndex) const {
        return m_streamAllocator.isStreamActive(streamIndex);
    }

    /// TNF Task 1 test seam (design doc 4.5): runs the connect-time DDC seed
    /// without a live connection, so a test can assert the quantity it
    /// commands.
    void seedConnectFrequencyForTest(SliceModel* slice) {
        seedConnectFrequency(slice);
    }

private:
    /// Remote-daemon R2 Task 11. Emits BOTH activeSliceChanged(index) (the
    /// existing positional signal, unchanged) and activeSliceIdChanged(id)
    /// (new), resolving `index` against m_slices to find the id. `index`
    /// of -1 (no active slice, the removeSlice() case when the list would
    /// otherwise be left empty) reports -1 on both signals rather than
    /// resolving anything. The three existing `emit activeSliceChanged(...)`
    /// call sites (addSlice, removeSlice, setActiveSlice) all route through
    /// this instead, so the two signals can never disagree about which
    /// slice is active. Fix round 1 review finding: this was originally
    /// placed under the WRONG access specifier (a `public:` label two
    /// sections above this one re-opens visibility without a matching
    /// `private:` in between it and where this was first declared) --
    /// moved here, under a genuine `private:`, so no external caller can
    /// announce an active-slice change without m_activeSlice having
    /// actually moved.
    void emitActiveSliceChanged(int index);

    struct PlannedSlicePlacement {
        int sliceId{-1};
        int previousStream{-1};
        SliceStreamAllocator::Placement placement;
        int resolvedRateHz{0};
    };

    struct StreamRateChangePlan {
        SliceStreamAllocator allocator;
        QVector<PlannedSlicePlacement> slices;
    };

    // iPhone app Task 75 (ruling 7.3): `excluded` slices are left out of
    // the simulation, as though already closed (a stream only they held is
    // free); `rejectedSliceId`, when given, names the slice whose refusal
    // made the plan fail (-1 for any other failure).
    std::optional<StreamRateChangePlan>
    planStreamSampleRateChange(int streamIndex, int rateHz,
                               const QSet<int>& excluded = {},
                               int* rejectedSliceId = nullptr) const;

    void commitStreamSampleRateChange(const StreamRateChangePlan& plan);

    /// Remote-daemon R2 Task 18: shared body of addSlice() and
    /// addSliceWithStationId(). `requestedId` below 0 means "mint the
    /// lowest id not currently in use", which is exactly what addSlice()
    /// has always done and remains its only behaviour; any other value is
    /// used verbatim, and checking it for collision first is the caller's
    /// job (addSliceWithStationId does).
    // R-R3-49: hands AudioEngine what rxBlockReady needs of each slice (its
    // mute, output route and VAX channel), one atomic word per slice id,
    // and marks every other id absent. Main thread. Called on every add,
    // remove and layout restore and on each slice's mute / route / VAX
    // change, so the audio thread never walks m_slices or touches a
    // SliceModel the main thread may be deleting.
    void publishSliceAudioView();
    // RADE gaps (2026-09-30): the slices in RADE mode (bit n for id n), and
    // their hand-off to the DSP worker (RxDspWorker::setRadeModeSlices).
    // Main thread; called from publishSliceAudioView, on each slice's mode
    // change, and when a worker is attached.
    quint32 radeModeSliceMask() const;
    void publishRadeModeSlices();
    int addSliceImpl(int requestedId, const QString& initialPanId,
                     const ReceiveSliceState* restoreSeed = nullptr,
                     bool bindRestored = false);

    /// iPhone app Task 73: every slice's `active` from its owner's active
    /// slice, and activeSlice() moved to the station-level one (emitting
    /// the active-slice signals when it moves). Local only.
    void applyActiveSlices();

    /// The operator's reason for a refused add at the slice cap:
    /// "<radio> supports a maximum of <cap> slices" ("1 slice" for one), or,
    /// before a radio has sized the stream pool, "The Core supports a
    /// maximum of ..." on a Core and "NereusSDR supports a maximum of ..."
    /// in a window with no Core. One wording for addSliceOnPan() and addSlice(), and so
    /// for the session verbs that relay them (Phase 3F design section 3).
    QString sliceCapReason(int cap) const;

    /// Slice ids below this have a WDSP channel: the stream pool's ceiling
    /// clamped to WdspEngine::kMaxSliceChannels, or that absolute ceiling
    /// before any pool is sized.
    int sliceChannelLimit() const;

    /// The operator sentence for a slice closed because the board cannot
    /// host its id (R-R3-34: explicit, with a way to recover).
    QString closedSliceSentence(const SliceModel* slice, int channelLimit) const;

    /// Adds Slice A on pan-0 at constructor defaults, bound to its own
    /// stream, when no slice 0 exists. Used before every remaining slice is
    /// closed or refused, so the radio always keeps one receiver.
    void installReceiveFallbackSlice();

    /// At connect, outside startup admission of a saved layout: closes every
    /// slice whose id has no WDSP channel on this board, highest id first,
    /// and reports them through the receive-layout restore status.
    void closeSlicesPastChannelLimit();

    /// Remote-daemon R2: shared body of removeSlice() and
    /// removeSliceWithStationId(), for the same reason addSliceImpl above
    /// is shared. removeSlice() now sends a verb on a Role::Remote model,
    /// so the session's own inbound destroy needs a way past that branch
    /// to the removal itself.
    /// `mayCloseLast` (slice control plan Task 7): the claims rule's close
    /// of an unclaimed slice, which may leave the Core with no slice.
    void removeSliceImpl(int sliceId, bool persist = true, bool mayCloseLast = false,
                         bool requireUnclaimed = false);
    void bindReceiveLayoutSlices();
    bool activateRestoredRadeReceiveOwner(QString* error);
    void setReceiveLayoutRestoreStatus(const QString& state, const QString& message);
    bool captureReceiveLayout(QString* error);

    // RADE threads (2026-09-30): one receive route per RADE slice. The
    // channel decodes on its own thread; the DSP worker feeds it and plays
    // its speech (RxDspWorker::setRadeRxRoute). `serial` tells a route from
    // the one that replaced it, so a destroyed channel retires only its own.
    struct RadeRxRoute {
        quint64 serial{0};
        QPointer<RadeChannel> channel;
        QPointer<SliceModel> slice;
    };

    quint64 installRadeRxRoute(int sliceId, RadeChannel* channel,
                               SliceModel* slice);
    void retireRadeRxRoute(int sliceId, quint64 serial);
    void queueRadeRxRoute(int sliceId, RadeChannel* channel);
    // Only the TX slice's channel encodes and is handed to the TX worker;
    // the TX slice's channel stops decoding while keyed. `keyed` is the MOX
    // edge's own value when one is being handled, else mox().
    void refreshRadeTxSelection(bool keyed);
    void attachRadeRxWorker(RxDspWorker* worker);
    bool canAdmitRadeSlice(int sliceId, const SliceModel* slice) const;

    /// Remote-daemon R2 Task 5: the actual sizing body, shared by
    /// configureStreamPool (gated on Role::Local) and
    /// configureStreamPoolForTest (unconditional). See both definitions.
    void configureStreamPoolImpl(int userDdcCount, int maxSlices,
                                 int defaultRateHz);

    // Sub-components (owned, main thread)
    RadioDiscovery*  m_discovery{nullptr};
    ReceiverManager* m_receiverManager{nullptr};
    AudioEngine*     m_audioEngine{nullptr};
    WdspEngine*      m_wdspEngine{nullptr};
    // Remote Daemon R2 Task 12: constructed only for Role::Local (see the
    // constructor body); stays nullptr for Role::Remote.
    SliceMeterPump*  m_sliceMeterPump{nullptr};

    // Connection (owned, lives on m_connThread)
    RadioConnection* m_connection{nullptr};
    QThread*         m_connThread{nullptr};

    // I/Q DSP worker (owned, lives on m_dspThread). Fed by a queued
    // connection from ReceiverManager::iqDataForReceiverStamped (R-R3-40).
    RxDspWorker*     m_dspWorker{nullptr};

    // R-R3-39: the receive lane (local role only), and what its barriers
    // park: the DSP worker and its thread, set while the worker runs and
    // cleared (after the lane is drained) before either is deleted.
    std::unique_ptr<DspControlThread> m_rxLane;
    // R-R3-39: the transmit lane (local role only).
    std::unique_ptr<DspControlThread> m_txLane;
    // MoxController's txReady / txaFlushed to m_txChannel (connect path).
    void wireTxChannelKeying();
    void disconnectTxChannelKeying();
    QList<QMetaObject::Connection> m_txKeyingConnections;
    QPointer<TxChannel> m_txKeyingChannel;
    quint64 m_txKeyingGeneration{0};
    struct RxWorkerTarget {
        std::mutex mutex;
        RxDspWorker* worker{nullptr};
        QThread* thread{nullptr};
    };
    std::shared_ptr<RxWorkerTarget> m_rxWorkerTarget{std::make_shared<RxWorkerTarget>()};
    void setRxWorkerTarget(RxDspWorker* worker, QThread* thread);
    // Runs `fn` on the DSP worker's thread and waits for it, from the lane
    // (what the BlockingQueuedConnection calls did from the event loop).
    // Runs it at once when the worker's thread is not running.
    void runOnDspWorkerFromLane(RxDspWorker* worker, const std::function<void()>& fn);
    // True when `worker` is the running DSP worker and its thread is not
    // the caller's.
    bool dspWorkerThreadRunning(RxDspWorker* worker) const;
    // Connects a new RX channel's lane signals (NNR diagnostics and late
    // refusals, DSP-options timing) to the slice that owns it.
    void wireRxChannelLaneSignals(int channelId);
    // Reads the AGC top back (on the lane) after an AGC-T set and puts it in
    // the slice's RF gain. m_agcReadbackSerial keeps only the newest
    // readback per slice.
    void syncRfGainFromAgcTop(SliceModel* slice, RxChannel* channel);
    QHash<const SliceModel*, quint64> m_agcReadbackSerial;
    // The live rate change (setSampleRateLiveAsync).
    struct SampleRateRequest {
        int rateHz{0};
        bool reconcileDiversity{true};
        std::function<void(bool)> onFinished;
    };
    bool m_sampleRateChangeInFlight{false};
    // An applyStreamDspGeometry that arrived while a change ran.
    bool m_streamGeometryPending{false};
    int m_sampleRateTargetHz{0};
    std::vector<std::function<void(bool)>> m_sampleRateInFlightCallbacks;
    std::optional<SampleRateRequest> m_pendingSampleRateChange;
    quint64 m_sampleRateChangeGeneration{0};
    void requestSampleRateChange(SampleRateRequest request);
    void startSampleRateChange(SampleRateRequest request);
    void finishSampleRateChange(int rateHz, bool reconcileDiversity,
                                bool restartExternalDiversity, qint64 elapsedMs);
    bool canChangeSampleRateLive() const;
    // Ends an in-flight change at teardown: its callers hear ok=false.
    void abandonSampleRateChange();

    // R-R3-40: per-slice DSP load snapshots, refreshed every
    // ReceiverDspLoadSampler::kSampleIntervalMs by m_dspLoadTimer (local
    // role only). Main thread only.
    ReceiverDspLoadSampler m_dspLoadSampler;
    QTimer* m_dspLoadTimer{nullptr};
    // R-R3-40: the NNR step-back decision, its monotonic clock, and the
    // slices whose NNR readback is refreshed once their WDSP worker has run
    // two blocks after a step (slice id -> worker block count at the step).
    NnrLoadGovernor m_nnrGovernor;
    QElapsedTimer m_nnrGovernorClock;
    QHash<int, qint64> m_nnrLimitReadbackPending;
    QThread*         m_dspThread{nullptr};
    QHash<int, RadeRxRoute> m_radeRxRoutes;
    quint64 m_nextRadeRxRouteSerial{0};

    // Sub-models
    MeterModel    m_meterModel;
    TransmitModel m_transmitModel;

    // Phase 3M-0 Task 17: PA safety controllers.
    // Declared AFTER m_transmitModel so the ingest lambda can read
    // m_transmitModel.isTune() safely at any point post-construction.
    // SwrProtectionController and TxInhibitMonitor are QObject children
    // (parent=this); BandPlanGuard is a plain value class.
    safety::SwrProtectionController m_swrProt{this};
    safety::TxInhibitMonitor        m_txInhibit{this};
    safety::BandPlanGuard           m_bandPlan;

    // OC matrix — per-band × per-pin × {RX,TX} bit assignments.
    // Owned here so both OcOutputsTab UI and P1/P2 codec layer read
    // the same instance. MAC and load() are called on connect.
    // Phase 3P-D Task 3.
    OcMatrix      m_ocMatrix;

    // HL2 Options model — 9 HL2-specific behavior knobs.  Owned here
    // so the Hl2OptionsTab and (eventually) the P1 codec wire-format
    // layer share one instance.  MAC and load() are called on connect.
    // Phase 3L commit #9.
    Hl2OptionsModel m_hl2Options;
    // Hl2OptionsModel::changed -> applyHl2Options, while a P1 connection is
    // up (connectHl2OptionsToConnection).
    QMetaObject::Connection m_hl2OptionsConnection;

    // HL2 I/O board model — owns I2C queue and register mirror.
    // Shared with P1RadioConnection::setIoBoard() at connect time.
    // Phase 3P-E Task 2.
    IoBoardHl2    m_ioBoard;

    // HL2 LAN PHY bandwidth monitor — owns byte-rate + throttle state.
    // Pushed into P1RadioConnection::setBandwidthMonitor() at connect time.
    // Phase 3P-E Task 3.
    HermesLiteBandwidthMonitor m_bwMonitor;

    // Live PA telemetry + PTT state from status packets.
    // Phase 3P-H Task 2.
    RadioStatus m_radioStatus;

    // HL2 temperature averaging ring (only populated when model ==
    // HPSDRModel::HERMESLITE). HL2 firmware overloads the C&C
    // exciter_power AIN5 field to carry on-die FPGA temperature ADC
    // counts; we mirror mi0bot's 100-sample averaging window before
    // publishing to RadioStatus to suppress per-frame noise.
    //
    // Port of mi0bot console.cs:24917-24985 + 25069-25082
    // [v2.10.3.13-beta2 @c26a8a4]:
    //   private ConcurrentQueue<int> _tempQueue = new ConcurrentQueue<int>();   // MI0BOT: HL2 temperature
    //   ...
    //   _tempQueue.Enqueue(NetworkIO.getExciterPower());
    //   while (_tempQueue.Count > 100 && nTries < 100) //  MI0BOT: HL2 temperature, keep max 100 in the queue
    //       _tempQueue.TryDequeue(out int tmp);
    //   ...
    //   float tempAverage = _tempQueue.Count > 0 ? (float)_tempQueue.Average() : 0;     // MI0BOT: HL2 temperature
    std::array<quint16, 100> m_hl2TempRing{};
    int m_hl2TempCount{0};   // 0..100 — slots filled
    int m_hl2TempHead{0};    // next slot to write

    // Settings hygiene — validated against caps at connect time.
    // Phase 3P-H Task 2.
    SettingsHygiene m_settingsHygiene;

    // Alex antenna controller — per-band TX/RX/RX-only port assignment.
    // MAC and load() are called on connect, matching OcMatrix ownership pattern.
    // Phase 3P-F Task 3.
    AlexController m_alexController;
    std::array<AlexController::AlexAdcState, 2> m_stationFilterStates{};
    std::array<unsigned, 2> m_stationFilterFields{};
    bool m_stationFilterSnapshotReady{false};
    // Plan Task 14 fix wave: the band outputs on the wire (bandOutputsByte).
    // m_bandOutputsFields: bits 0-2 = byte, band, keyed received (remote);
    // all three set by a local report.
    int      m_bandOutputsByte{0};
    int      m_bandOutputsBand{-1};
    bool     m_bandOutputsKeyed{false};
    unsigned m_bandOutputsFields{0};

    // Phase 3F Sub-Epic F Task 5: per-ADC WidebandFftEngine instances.
    // Indexed by adcIndex (0 or 1). Constructed in the RadioModel ctor with
    // a default 122.88 MHz ADC sample rate. Owned via QObject parent.
    std::array<NereusSDR::WidebandFftEngine*, 2> m_widebandFftEngines{};
    struct WidebandDemandOwner {
        QPointer<SliceModel> slice;
        bool active{false};
    };
    struct WidebandDemandState {
        std::array<bool, WidebandSpectrumCache::kMaxSources> capture{};
        std::array<bool, 2> bypass{}; // AlexController's two filter-chain slots.
    };
    struct WidebandDemandRoute { int adc; int chain; };
    std::optional<WidebandDemandRoute> widebandDemandRoute(const SliceModel* slice) const;
    WidebandDemandState widebandDemandState() const;
    void retireWidebandDemand();
    QHash<WidebandDemandToken, WidebandDemandOwner> m_widebandDemands;
    WidebandDemandToken m_lastWidebandDemandToken{0};
    bool m_widebandDemandRetiring{false};
    bool m_reconcilingWidebandDemand{false};
    bool m_widebandDemandDirty{false};
    WidebandSpectrumCache m_widebandSpectrumCache;
    std::array<std::shared_ptr<const std::atomic<quint64>>, 2> m_widebandCaptureEpochs{};
    void invalidateWidebandSpectrum(int adc);

    // R1 Task 11: connection-context anchor for the wideband FFT dispatch
    // hop inside wireConnectionSignals -- a bare QObject used only for its
    // thread affinity, never for signals/slots of its own. Qt resolves an
    // AutoConnection's direct-vs-queued dispatch PER EMIT based on this
    // object's CURRENT thread() versus the emitting thread, so
    // setWidebandDispatchThread()'s moveToThread() retargets the hop even
    // for a connection made before the move. Deliberately NOT parented to
    // `this`: QObject::moveToThread() refuses to move an object that has
    // a parent. Starts life on whatever thread constructs this
    // RadioModel -- identical to the pre-Task-11 `this` context it
    // replaces in wireConnectionSignals.
    QObject m_widebandDispatchContext;
    std::atomic<quint64> m_widebandConnectionEpoch {1};

    // Band-plan overlay manager — app-global, loaded once from Qt resources.
    // Phase 3G RX Epic sub-epic D.
    BandPlanManager m_bandPlanManager;

    // Apollo PA + ATU + LPF accessory state (present/filter/tuner enable bools).
    // MAC and load() are called on connect. Phase 3P-F Task 5a.
    ApolloController m_apolloController;

    // PennyLane external-control master toggle. Composes with OcMatrix (Phase 3P-D).
    // MAC and load() are called on connect. Phase 3P-F Task 5b.
    PennyLaneController m_pennyLaneController;

    // Calibration controller — HPSDR NCO correction factor, level offsets, PA current.
    // MAC and load() are called on connect. Backs CalibrationTab UI and
    // P2RadioConnection::hzToPhaseWord(). Phase 3P-G.
    CalibrationController m_calController;
    // R-R3-49: Calibration's Volts/Amps log (parented to this).
    VoltsAmpsLog* m_voltsAmpsLog{nullptr};

    // Slices and panadapters (client-managed)
    QList<SliceModel*> m_slices;
    QList<QPointer<SliceModel>> m_sliceRemovalsInFlight;
    // ID-keyed consumers must finish an old removal before seeing the next
    // object with that ID. Construction/ownership still complete synchronously.
    struct SlicePublication {
        QPointer<SliceModel> object;
        int id{-1};
        quint64 incarnation{0};
        bool constructing{true};
        bool ready{false};
        bool published{false};
        quint64 serial{0}; // Exact pending record, including allocator address reuse.
    };
    QHash<SliceModel*, SlicePublication> m_slicePublications;
    QHash<int, int> m_sliceRemovalBarriers;
    quint64 m_nextSlicePublicationSerial{0};
    bool currentSlicePublication(SliceModel* slice) const;
    void publishReadySlices(int id);

    QList<PanadapterModel*> m_panadapters;
    SliceModel* m_activeSlice{nullptr};
    // iPhone app Task 73: whose each slice is. Qt-parented to this model.
    SliceOwnership* m_sliceOwnership{nullptr};
    // Parity Task 31: DUP, this window's and each remote device's, and the
    // transmit slice's noise blanking saved at the key.
    bool m_localDisplayDuplex{false};
    QSet<QByteArray> m_deviceDisplayDuplex;
    QPointer<SliceModel> m_nbSavedSlice;
    NereusSDR::NbMode m_nbSavedMode{NereusSDR::NbMode::Off};
    // iPhone app plan Task 35.
    KeyedBy m_keyedBy;
    // The Radio Status page: the last key's trigger and whether a device
    // keyed it, kept through a RADE end-of-over tail (Local only).
    QString m_radioStatusKeyTrigger;
    bool m_radioStatusKeyFromDevice{false};
    // Remote: a refresh of the page's key is queued for the delta in.
    bool m_radioStatusPttRefreshQueued{false};
    // iPhone app plan Task 38: the transmit time-out (Local only; Qt
    // parent this) and the last reason the Core stopped a transmission.
    TxTimeOutTimer* m_txTimeOut{nullptr};
    TransmitStopReason m_lastTransmitStopReason;
    quint32 m_keyingEpoch{0};
    // iPhone app plan Task 36: the remote microphone ring (Local only; it
    // outlives the transmit pump, which holds a plain pointer to it) and
    // what puts it in use.
    std::unique_ptr<RemoteMicFeed> m_remoteMicFeed;
    // Fix wave C2: every device with a line open (a count per device, one
    // per media connection), the device priming, the devices with VOX
    // armed, and the one writer.
    QHash<QByteArray, int> m_remoteMicLines;
    QByteArray m_remoteMicPrimingDevice;
    QSet<QByteArray> m_remoteMicVoxArmed;
    QByteArray m_remoteMicWriter;
    bool m_remoteMicInUse{false};
    // The device keyed on its line: if the line goes away mid-key, the
    // ring stays the source (silence) until that key ends, so a remote key
    // never falls back to the station's own microphone.
    QByteArray m_remoteMicKeyedDevice;
    struct RemoteMicSelection { QByteArray device; RemoteMicSource source; };
    QHash<QString, RemoteMicSelection> m_remoteMicSelections;
    struct RemoteRadioKey { QString owner; QByteArray device; quint32 commandId{0}; quint32 epoch{0}; };
    std::optional<RemoteRadioKey> m_remoteRadioCandidate;
    std::optional<RemoteRadioKey> m_remoteRadioKey;
    void setRemoteRadioMicActive(bool active);
    bool m_remoteRadioMicActive{false};
    // Set while setTune(true, keyer) runs: the keyer TUNE asks and keys for.
    const KeyerIdentity* m_tuneKeyer{nullptr};
    // iPhone app Task 73 (ruling 5.11): the frequency the FreeDV Reporter
    // lists, the station-level active slice's; published when connected.
    quint64 m_freedvWantedHz{0};

    // View hooks (non-owning, set by MainWindow). Phase 3G-8 + 3G-9c +
    // 3M-5d (m_txAnalyzer).
    // Phase 3F Sub-Epic I: which DDC stream hosts which slice. Pure policy;
    // sized by configureStreamPool at connect, empty (and therefore
    // bind-refusing) while disconnected.
    NereusSDR::SliceStreamAllocator m_streamAllocator;
    QVector<bool> m_streamCtunPinned;
    QVector<quint64> m_streamEpoch;
    quint64 m_nextStreamEpoch{0};

    // Rate handed to configureStreamPool, used when a stream is claimed
    // before m_connectionSampleRateHz has been set (Slice A binds during
    // connectToRadio, before wireConnectionSignals records the wire rate).
    int m_streamDefaultRateHz{192000};

    // Phase 3F Sub-Epic I closeout, defect H1: the drain size last published
    // to RxDspWorker for each stream, keyed by stream index. Seeded by
    // configureStreamPool to bufferSizeForRate(defaultRateHz), which is
    // exactly what the worker's global default already is, so a pool that
    // never leaves its connect rate is recognised as already in step and
    // applyStreamDspGeometry stays a no-op. Main thread only.
    QHash<int, int> m_streamInSizePushed;

    // Phase 3F Sub-Epic I Task 7b: the codec's last per-stream DDC choice,
    // indexed by stream. -1 = that stream is idle, so an emptied stream
    // leaves no stale DDC behind. Backs ddcForStream().
    std::array<int, 5> m_streamDdc{{-1, -1, -1, -1, -1}};

    // Process-wide WDSP pdiv[0] lifecycle shadow. Main-thread-only: the worker
    // route itself is published/cleared synchronously on m_dspThread before
    // this state changes.
    static constexpr int kExternalDiversityId = 0;
    int m_diversityTargetSliceId{-1};
    bool m_diversityCommitting{false};
#ifdef NEREUS_BUILD_TESTS
    bool m_diversityAdmissionBypassForTest{false};
#endif
    quint64 m_diversityStateRevision{1};
    QByteArray m_diversityStateShape;
    bool m_externalDiversityRouteActive{false};
    // R-R3-39: bumped by every route start and stop, so a lane answer about
    // an older start is ignored.
    quint64 m_externalDiversityRouteGeneration{0};
    int m_externalDiversityPrimaryDdc{-1};
    int m_externalDiversitySecondaryDdc{-1};
    int m_externalDiversityChunkSize{0};

    // Defect D1: the ADC that same assignment routed each stream to, decoded
    // from its adcCtrl bytes. Backs chainForStream(), which is what the Alex
    // per-chain filter decision groups by.
    //
    // Held here rather than read back out of ReceiverManager, even though
    // publishDdcAssignment mirrors it there too. ReceiverConfig only exists
    // for a receiver that has been created, and connectToRadio creates one
    // per stream, so a RadioModel driven without a connection (every unit
    // test, and the pre-connect model state) has no ReceiverConfig to answer
    // from and would silently report ADC0 for everything. That is precisely
    // the failure mode D1 was: an ADC field that always says 0 and a wire
    // that says otherwise.
    //
    // 0 rather than -1 for an idle stream: chainForStream returns -1 for
    // "not on a chain" off the stream index itself, and a stream with no DDC
    // has no ADC to name.
    std::array<int, 5> m_streamAdc{{0, 0, 0, 0, 0}};

    // Phase 3F Sub-Epic I closeout, defect F3: last-published set of streams
    // that host slices but have no DDC. Change-gates the streamsSuspended
    // emit so it fires on transitions rather than on every codec run.
    QVector<int> m_suspendedStreams;

    // Phase 3F Sub-Epic I closeout, defect F4: guards the rollback
    // setFrequency in the retune handler from re-entering the allocator.
    // Everyone else still sees the rolled-back frequencyChanged.
    bool m_rollingBackFrequency{false};

    // Phase 3F Sub-Epic J Task 6: guards the nbModeChanged mirror from
    // re-entering itself. The blanker is per-DDC, not per-slice (Thetis
    // cmaster.h:74-82 [v2.10.3.15]), so a change on one slice writes
    // setNbMode on every co-host; each of those emits its own
    // nbModeChanged, which would otherwise walk the stream again.
    bool m_mirroringNbMode{false};
    // Re-entrancy guard for the NB1 / NB2 detailed-tuning mirror, which
    // spreads one slice's blanker tuning across every co-host on the same
    // DDC. Separate from m_mirroringNbMode: the two mirrors run off different
    // signals and a shared flag would let one suppress the other. Shared
    // across the five tuning knobs is fine, because each mirror only ever
    // writes the same property it fired on, so they never nest.
    bool m_mirroringNbTuning{false};

    // Phase 3F Sub-Epic I closeout, defect F4: the allocator's own words for
    // the last rejected placement, handed to the retune handler so its
    // status-bar line can explain what the hardware ran out of.
    QString m_lastPlacementRejectReason;

    // Phase 3F Sub-Epic I closeout, defect F3: injected via
    // setDdcContextForTest. Off in production; currentCodecContext() reads
    // MoxController / PureSignal / the slice diversity flag as before.
    bool m_ddcCtxForTest{false};
    bool m_ddcCtxMoxForTest{false};
    bool m_ddcCtxPsForTest{false};
    bool m_ddcCtxDivForTest{false};

    // View hooks (non-owning, set by MainWindow). Phase 3G-8 + 3G-9c.
    class SpectrumWidget*     m_spectrumWidget{nullptr};
    // R1 Task 4: abstract twin of m_spectrumWidget above, see the
    // spectrumSink() comment. Same object, different static type.
    NereusSDR::ISpectrumSink* m_spectrumSink{nullptr};
    class FFTEngine*          m_fftEngine{nullptr};
    class FftEnginePool*      m_fftEnginePool{nullptr};
    class TxAnalyzer*         m_txAnalyzer{nullptr};
    std::unique_ptr<TxDisplayFeed> m_txDisplayFeed;
    class ClarityController*  m_clarityController{nullptr};
    class StepAttenuatorController* m_stepAttController{nullptr};
    // Level Cal: Thetis rx2_preamp_offset[] (console.cs:2011-2019
    // [v2.10.3.15]), never saved; NaN reads the default.
    std::array<float, 10> m_rx2PreampOffsetDb{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    // R-R3-46: followReceiveSliceWithStepAttenuator() has wired its connects.
    bool m_stepAttFollowsSlices{false};

    // 2026-05-22 spectrum-calibration fix: cache of the last rxMeterOffsetDb
    // value we emitted via rxMeterOffsetChanged. NaN sentinel forces the
    // first call to compare unequal so subscribers always get the initial
    // value (matches the MoxController NaN-sentinel pattern). Updated only
    // by setStepAttController's recompute lambda; rxMeterOffsetDb itself
    // stays const and recomputes on every call.
    mutable double m_lastEmittedRxMeterOffsetDb{std::numeric_limits<double>::quiet_NaN()};

    // Radio info
    QString m_name;
    QString m_model;
    QString m_version;
    HardwareProfile m_hardwareProfile;

    // Phase 3Q-1: RadioModel-level connection state machine.
    // Drives UI (TitleBar, ConnectionPanel, status bar, spectrum overlay).
    ConnectionState m_connectionState{ConnectionState::Disconnected};
    bool m_localConnectionSetupActive{false};

    // Remote-daemon R2 Task 4: set once at construction (see the Role
    // constructor overload above), never mutated afterward.
    Role m_role{Role::Local};
    bool m_receiveOnlyStationPolicy{false};

    // R-R3-36: PC microphone session demand (see setPcCaptureAllowed).
    // m_pcCaptureSessionActive is true from AudioEngine::start() in the
    // local connect path until teardown has stopped the TX worker.
    void updatePcCaptureDemand();
    // R-R3-36 Task 6: TransmitModel's pcMic* fields project the AudioEngine
    // TX input config and their setters forward to it (constructor only).
    void wirePcMicConfigProjection();
    // R-R3-36 Task 7: pcCaptureRequired() less the keying that does not
    // read the PC microphone (TCI audio, Tune, two-tone). Used by the MOX
    // pre-check and the input-loss release only; the session demand stays
    // on pcCaptureRequired().
    bool pcCaptureGatesKeying() const;
    bool generatedKeyInFlight() const;
    // iPhone app plan Task 36: recomputes remoteMicInUse() and puts the
    // ring in or out of use.
    void updateRemoteMicSource();
    bool pcCaptureReady() const;
    void onCaptureStatusChanged(const CaptureSupervisor::Status& status);
    // R-R3-36: true only across the m_moxController->setTune(true) call in
    // setTune(), so the MOX pre-check recognises Tune's own key without
    // trusting m_manualMox, which a refused Tune leaves set.
    bool m_tuneKeyInFlight{false};
    // R-R3-36: the key-up now live came from Tune or two-tone (set from
    // moxChanging, cleared by any unkey).
    bool m_generatedKeyLive{false};
    bool m_pcCaptureAllowed{true};
    SliceModel::VaxChannelStore m_remoteVaxChannelStore;  // R-R3-44
    bool m_pcCaptureSessionActive{false};
    CaptureSupervisor::Lease m_pcCaptureLease;

    // Remote-daemon R2 Task 20: the reach-through audit described on
    // localDspHandOutCount() above. Written only while m_role ==
    // Role::Remote, which is what makes plain ints safe here (see that
    // comment for the thread argument).
    int              m_localDspHandOuts{0};
    QSet<QByteArray> m_localDspHandOutNames;

    // Bumps the audit above. Returns immediately on Role::Local so the
    // three hot accessors cost one predictable branch there and nothing
    // else. Deliberately NOT const: the accessors it serves are non-const.
    void noteLocalDspHandOut(const char* accessor)
    {
        if (m_role == Role::Local) {
            return;
        }
        ++m_localDspHandOuts;
        m_localDspHandOutNames.insert(QByteArray(accessor));
    }

    // Remote-daemon R2 Task 4: non-owning; see attachStation()/
    // detachStation() above.
    NereusSDR::IStationLink* m_station{nullptr};
    // iPhone app plan Task 78.
    bool m_stationMayCloseLastSlice{false};
    QPointer<QObject> m_stationDevices;

    // Remote-daemon R2 Task 18: the EFFECTIVE slice limit the station
    // advertised (parent design section 4.5 -- what the DAEMON can
    // sustain, never the board's own BoardCapabilities::maxSlices), and
    // its user DDC count. 0 means no handshake has been applied yet.
    // Read by maxSlices() for Role::Remote only; a Role::Local model
    // never sets or reads either, so local direct mode is untouched.
    int m_stationMaxSlices{0};
    int m_stationUserDdcCount{0};
    int m_stationRemoteIqVersion{0};

    // One model-owned monotonic age for local uptime and V6 telemetry. The
    // connection identity fence also retires age if a pointer is replaced
    // without a state edge (including the test injection seam).
    QElapsedTimer m_connectionStartedAt;
    QPointer<RadioConnection> m_connectionAgeOwner;
    bool m_connectionAgeHadOwner{false};

    // Phase 3Q sub-PR-3: sample rate as last pushed to the wire.
    // Written from the wireSampleRateChanged path in connectToRadio().
    // connectionSampleRateHz() / connectionSampleRateText() read this.
    int m_connectionSampleRateHz{0};

    // Task 1.7: active-RX count last pushed to the wire (0 = disconnected).
    // Updated by setActiveRxCountLive() after hardware reconfiguration completes.
    // Also written by connectToRadio() via the resolveActiveRxCount() call.
    int m_connectionActiveRxCount{0};

    // Reconnect state
    RadioInfo m_lastRadioInfo;
    bool m_intentionalDisconnect{false};

    // I/Q accumulator and per-batch buffer sizes now live in
    // RxDspWorker (src/models/RxDspWorker.h) so the DSP thread owns
    // its own state and the main thread never touches it.

    // Per-slice-per-band persistence: tracks which band the VFO is currently
    // on so the coalesced scheduleSettingsSave() timer writes to the right
    // per-band slot. From Thetis console.cs:45312 handleBSFChange
    // [@501e3f5] — bandstack state is recalled via band-button
    // press, not via VFO tune, so this lambda only tracks; it does NOT
    // save or restore at the boundary.
    Band m_lastBand{Band::Band20m};
    // Thetis's _tx_band (console.cs:17511 [v2.10.3.15]): the band PWR was
    // last loaded for, which drivePowerScroll saves to. Unknown until
    // applyTransmitBand first runs.
    Band m_txBand{Band::Band20m};
    bool m_txBandKnown{false};
    /// iPhone app Task 75 (ruling 5.11a): the band whose receive antenna the
    /// relay keeps while another device listens through it; empty when the
    /// relay follows m_lastBand as always.
    std::optional<Band> m_keptRxAntennaBand;
    bool m_bandTrackingForTest{false};

    // Settings save coalescing: a save is wanted (m_settingsSaveScheduled),
    // and scheduleSettingsSave's 500 ms timer is running.
    bool m_settingsSaveScheduled{false};
    bool m_settingsSaveTimerArmed{false};
    bool m_receiveLayoutHydrating{false};
    bool m_stationHandoverTrackSuppressedReceiverEdits{false};
    bool m_stationHandoverSuppressedReceiverEdits{false};
    bool m_receiveLayoutPendingAdmission{false};
    bool m_receiveLayoutManaged{false};
    bool m_receiveLayoutOverridesCount{false};
    // Slice ids the saved layout hydrated, until admission completes: a
    // slice refused at admission is "restored" only if its id is here.
    QSet<int> m_receiveLayoutHydratedIds;
    // Slices closed at connect because the board cannot host their ids,
    // for the one run of admission that reports them. Kept so a later
    // admission step does not report an accepted restore over it.
    QString m_sliceClosureNotice;
    bool m_receiveLayoutProtected{false};
    QString m_receiveLayoutMac;
    std::optional<int> m_restoredRadeReceiveOwner;
    // RADE reason: each RADE slice's last failed decoder start, and the
    // open beginRadeStart brackets.
    QHash<int, RadeStartFault> m_radeStartFaults;
    int m_radeStartHolds{0};
    QString m_receiveLayoutRestoreState;
    QString m_receiveLayoutRestoreMessage;
    bool m_settingsRetryScheduled{false};
    QString m_settingsSaveError;
    QSet<int> m_dirtySettingsSliceIds;
    // Phase 3P-I-a — dirty flag for AlexController persistence.
    // AlexController::antennaChanged can fire 14× during load(); the
    // flag + scheduleSettingsSave() timer coalesces them into a single
    // write at flush time. Set from the antennaChanged/blockTxChanged
    // handlers in wireSlice<Slot>, cleared by saveSliceState().
    bool m_alexControllerDirty{false};
    // Phase 3F re-entrancy guard for republishAlexAdcSlices().
    // republishAlexAdcSlices feeds AlexController::notifySlicesOnAdc, which
    // recomputes and can emit bpfStateChanged, which is wired back to
    // republishAlexAdcSlices so an operator override or a wideband toggle
    // reaches the wire on its own trigger. The re-entry lands between the
    // ADC0 and ADC1 notifications, so the inner pass would compose ADC1
    // from state the outer pass has not refreshed yet. The guard drops the
    // nested call; the outer one finishes the loop and pushes once, from
    // fully-updated state.
    bool m_republishingAlexBpf{false};
    // Plan Task 14 re-review N4: the Alex tab's two receive-side bypass
    // switches as last applied (applyAlexHpfSwitchSettings), so
    // republishAlexAdcSlices can report the bypass they put on Alex0.
    bool m_alexHpfBypassSwitch{false};
    bool m_alexDisable6mLnaOnRxSwitch{false};
    // Task 14 follow-up 2: the three keyed switches as last applied, for the
    // bypass they put on Alex0 while keyed. Defaults are the connection's
    // (RadioConnection.h, m_hpfBypassOnTx / m_hpfBypassOnPs /
    // m_disable6mLnaOnTx), which are Thetis's.
    bool m_alexHpfBypassOnTxSwitch{false};
    bool m_alexHpfBypassOnPsSwitch{true};
    bool m_alexDisable6mLnaOnTxSwitch{true};
    // The Alex tab's receive filter rows as last applied
    // (applyAlexHpfSwitchSettings): the chain decisions select each chain's
    // high-pass from them, as the connection does.
    codec::alex::AlexHpfEdges m_alexHpfEdges{codec::alex::AlexHpfEdges::thetisDefaults()};
    codec::alex::AlexLpfEdges m_alexLpfEdges{codec::alex::AlexLpfEdges::thetisDefaults()};
    // Shared-input filters, ruling (d): 6m/ByPass on RX as last applied
    // (applyAlexHpfSwitchSettings). On, the receive low-pass is the 6 m
    // filter for every slice and no slice holds it.
    bool m_alexLpfBypassSwitch{false};
    // Shared-input filters, ruling (c): the slice ids republishAlexAdcSlices
    // counted on each chain (away and unbound slices left out), for
    // bypassReasonForAdc's range names.
    std::array<QList<int>, 2> m_alexCountedSliceIds{};
    // JJ's ruling of 2026-09-30: on the HL2, the slice whose band has no
    // N2ADR pins set when the pins sent come to 0x00 (hl2ReceivePins
    // boardOff), for bypassReasonForAdc. -1 otherwise.
    int m_hl2NoPinsSliceId{-1};
    // alexLpfBits(): -1 until the connection (or the Core) reports one.
    int m_alexLpfBits{-1};
    // Level Cal: the Core's run (local role), created on first use.
    class LevelCalibrationService* levelCalibrationService();
    class LevelCalibrationService* m_levelCalService{nullptr};
    // A remote window's copy of the Core's run.
    bool m_stationLevelCalRunning{false};
    int m_stationLevelCalPercent{0};
    QString m_stationLevelCalMessage;
    bool m_stationLevelCalSucceeded{false};
    // The start this window sent, whose refusal it reports.
    quint32 m_levelCalStartCommandId{0};

#ifdef NEREUS_BUILD_TESTS
    std::optional<BoardCapabilities> m_testWidebandCaps;
    quint16 m_testP2OutboundBase {0};
    quint16 m_testP2InputBase {0};
    int m_testP2FirstIqMs {2000};
    int m_testP2EstablishedMs {3000};
    bool     m_testCapsOverride{false};
    bool     m_testCapsHasAlex{false};
    bool     m_testCapsIsRxOnly{false};              // 3M-1a G.2: injected via setCapsRxOnlyForTest
    bool     m_testCapsHasMicJack{true};             // 3M-1b I.1: injected via setCapsHasMicJackForTest
    HPSDRHW  m_testCapsHw{HPSDRHW::Unknown};        // 3M-1b I.3: injected via setCapsHwForTest
#endif

    // Test-only override for the handlePaTelemetry MOX gate.
    // Toggled true by handlePaTelemetryForTest() before the call and false
    // after, simulating "the radio just sent us a transmit sample" without
    // requiring the full MoxController state machine to be driven into
    // MoxState::Tx.  Always false in production code paths.
    // Bench-reported #167 follow-up.
    bool     m_forceTxForTest{false};

    // Phase 3M-0 Task 6: Ganymede PA-trip live state.
    // From Thetis Andromeda/Andromeda.cs:914 [v2.10.3.13] (_ganymede_pa_issue volatile bool).
    // G8NJJ: handlers for Ganymede 500W PA protection
    bool m_paTripped{false};
    // Task 16: receive only (see isRxOnly).
    bool m_rxOnlySetting{false};
    bool m_rxOnlyEffective{false};
    bool m_rxOnlyForced{false};
    // TX safety (2026-09-30): see isRadioLinkDown.
    bool m_radioLinkDown{false};
    // From Thetis Andromeda/Andromeda.cs:854-866 [v2.10.3.13] (_ganymedePresent / GanymedePresent setter).
    bool m_ganymedePresent{false};

    // Phase 3Q Task 10: auto-connect failure path.
    // Set by MainWindow::tryAutoReconnect() before starting the probe;
    // cleared (to false / empty) on success OR failure so that a subsequent
    // user-initiated Connect does not trip the failure handler.
    bool    m_autoConnectInProgress{false};
    QString m_autoConnectChosenMac;

    // AGC bidirectional sync guard — prevents infinite feedback loop between
    // agcThresholdChanged and rfGainChanged handlers.
    // From Thetis console.cs:45960-46006 — bidirectional sync pattern.
    bool m_syncingAgc{false};

    // From Thetis v2.10.3.13 console.cs:46057 — tmrAutoAGC (500ms interval)
    QTimer* m_autoAgcTimer{nullptr};
    NoiseFloorTracker* m_noiseFloorTracker{nullptr};
    QMap<int, NoiseFloorTracker*> m_streamNoiseFloors;
    // Task 3.1 view hook — non-owning, set by MainWindow.
    class MeterPoller*      m_meterPoller{nullptr};
    // Task 3.2 view hook — non-owning, set by MainWindow.
    class ContainerManager* m_containerManager{nullptr};

    // ── 3M-1a G.4: TUN state save/restore ───────────────────────────────────
    // Fields that preserve pre-TUN state across the setTune(true)/setTune(false)
    // pair so TUN-off can restore exactly what TUN-on changed.
    //
    // m_savedTxDspMode: DSP mode before the CW→LSB/USB swap.
    //   Cite: Thetis console.cs:30042 [v2.10.3.13] — old_dsp_mode = ...CurrentDSPMode.
    //   Default USB (matches SliceModel default). Used only when old_dsp_mode
    //   was CWL or CWU; restored unconditionally on TUN-off.
    DSPMode m_savedTxDspMode{DSPMode::USB};
    // Stable slice identity paired with m_savedTxDspMode. Listening focus
    // may move while the asynchronous TUN-off sequence is settling.
    int m_savedTxDspSliceId{-1};
    //
    // m_savedPowerPct: power slider value (0-100) before the tune-power push.
    //   Cite: Thetis console.cs:30033 [v2.10.3.13] — PreviousPWR = ptbPWR.Value.
    //   //MW0LGE_22b  [original inline comment from console.cs:30033]
    //   Saved and restored only under the FIXED tune source
    //   (console.cs:30094-30104 and 30180-30185 [v2.10.3.15]); see
    //   m_tuneSetFixedPwr.
    // Default 100 matches TransmitModel::m_power default (TransmitModel.h).
    // G.4 fixup: changed from 50 (initial value mismatch with TransmitModel);
    // harmless after the cold-off guard in setTune(false) but kept for hygiene.
    int m_savedPowerPct{100};
    // m_tuneSetFixedPwr: TUN-on took the FIXED branch (PWR limit off, PWR set
    //   to the tune power), so TUN-off turns the limit back on and restores
    //   m_savedPowerPct. Cleared by that restore.
    bool m_tuneSetFixedPwr{false};
    //
    // m_isTuning: True while TUN is engaged (between setTune(true) and
    //   setTune(false)).  Used as the idempotent guard at the top of
    //   setTune(false) — prevents a cold-off (no prior setTune(true)) from
    //   restoring stale saved state over the user's actual settings.  Also
    //   exported for H.3 UI polling.
    //   Cite: Thetis console.cs:30010 [v2.10.3.13] — _tuning = true.
    bool m_isTuning{false};

    // m_lastAudioVolume: cache of the most recent value emitted by
    //   TransmitModel::audioVolumeChanged.  Used by the swrProtectFactorChanged
    //   re-pump path (mirrors Thetis console.cs:26108 [v2.10.3.13]
    //   `Audio.RadioVolume = Audio.RadioVolume` self-assign re-emission when
    //   SWRProtect changes mid-TX).  Updated only by pumpAudioVolume.
    double m_lastAudioVolume{0.0};

    // ── Issue #177 fix — Thetis-faithful TUN-off ordering ────────────────────
    //
    // m_pendingTuneOff: latched true at the START of the setTune(false) path,
    //   cleared inside completeTuneOff().  setTune(false) now only kicks off
    //   the MoxController TX→RX walk; the rest (gen1 off, mode restore, drive
    //   restore, VFO restore) runs from completeTuneOff() AFTER MoxController
    //   emits rxReady AND an additional 100 ms settle elapses.
    //
    //   This mirrors Thetis console.cs:30106-30109 [v2.10.3.13]:
    //     chkMOX.Checked = false;        // synchronously walks TX→RX (~30 ms)
    //     await Task.Delay(100);
    //     radio.GetDSPTX(0).TXPostGenRun = 0;
    //
    //   Without the deferral, gen1 was killed at T+0 while the WDSP TX channel
    //   was still pumping fexchange0 (setRunning(false) does not fire until
    //   txaFlushed at T+10 ms).  The hard step at gen1's output produced a
    //   filter-ringing transient through the 31-stage TXA chain that briefly
    //   exceeded steady-state amplitude on the wire.  Combined with the wire
    //   drive byte staying at TUNE level for one EP2 frame after MOX-off
    //   (round-robin priority bank0 > bank10), this produced an RF spike past
    //   the radio's spec at high tune-slider settings.  Issue #177.
    bool m_pendingTuneOff{false};

    // m_tuneOffSettleMs: explicit 100 ms wait between MoxController::rxReady
    //   and completeTuneOff().  Mirrors `await Task.Delay(100)` at Thetis
    //   console.cs:30107 [v2.10.3.13].  Tests override via the *ForTest seam.
    int m_tuneOffSettleMs{100};

    // ── 3M-1a G.1: TX-side integration ──────────────────────────────────────
    // Master design §5.1.1; pre-code review §1.6 + §2.5.

    // MOX state machine — lives on the main thread (QTimers must be on
    // the event loop of the thread they fire on; RadioModel is main-thread).
    // Owned by RadioModel (Qt parent = this, set in constructor).
    // Wired: hardwareFlipped(bool) → onMoxHardwareFlipped(bool)
    //                              → StepAttenuatorController::onMoxHardwareFlipped
    //        txReady()             → m_txChannel->setRunning(true)
    //        txaFlushed()          → m_txChannel->setRunning(false)
    // From Thetis console.cs:29311-29678 [v2.10.3.13] — chkMOX_CheckedChanged2.
    //
    // Inline attribution tags preserved verbatim from the cited range:
    //[2.10.1.0]MW0LGE changed  [original inline comment from console.cs:29355]
    //MW0LGE [2.9.0.7]  [original inline comment from console.cs:29400]
    //[2.10.3.6]MW0LGE att_fixes  [original inline comment from console.cs:29561-29576]
    // Thread.Sleep(space_mox_delay); // default 0 // from PSDR MW0LGE  [console.cs:29603]
    //[2.10.3.6]MW0LGE att_fixes  [original inline comment from console.cs:29647-29659]
    MoxController* m_moxController{nullptr};

    // Stable WDSP RX identity stopped at MOX entry. Release restores this
    // exact channel even if listening focus changes before key-up.
    int m_moxStoppedRxChannel{-1};
    // Group B fix wave: whether onMoxHardwareFlipped last put the Alex
    // relays on the TX routing, and for which band; an antenna changed
    // meanwhile is applied on that routing, as Thetis applies it with
    // tx = _mox.
    bool m_alexRoutingTx{false};
    NereusSDR::Band m_alexRoutingTxBand{NereusSDR::Band::Band20m};

    // Phase 3F Sub-Epic C: TX-slice arbiter (single-TX invariant + RF-safe
    // handoff). QObject child of RadioModel (Qt parent ownership). Wired
    // to &m_slices + m_moxController in the constructor body, fed MAC +
    // load() on every currentRadioChanged emit. See txSliceArbiter()
    // accessor and docs/architecture/2026-05-26-phase3f-sub-epic-c-tx-arbiter-lifecycle-plan.md
    // Task 6.
    TxSliceArbiter* m_txSliceArbiter{nullptr};

    // Qt child, stopped before transmit/model retirement.
    CatService* m_catService{nullptr};
    UnkeyGate* m_unkeyGate{nullptr};   // Task 34, Qt-parented to this

    // Phase 3F Sub-Epic D Task 13: receiver -> pan FFT fan-out router.
    // QObject child of RadioModel. Constructed in the ctor body after
    // m_txSliceArbiter; the per-receiver FFTEngine pump and pan FFT
    // subscriber wiring lands in Sub-Epic E / F polish.
    class FFTRouter* m_fftRouter{nullptr};

    // Phase 3J-1 closeout Item 3 (2026-05-12): TCI Q_INVOKABLE long-tail
    // state.  See setGlobalMute / setAfLinear / setIqSampleRate / etc. for
    // semantics.  Defaults chosen to match TestMockRadioModel initial
    // values so the production path passes the existing matrix tests.
    //
    // Per-slice stub state for DSP toggles SliceModel doesn't yet expose
    // as Q_PROPERTYs: rxCtun.  Sized to the max RX count
    // NereusSDR supports today (4 for the four-DDC SKUs); the setter clamps
    // the index so an out-of-range slice silently no-ops.
    // rxNf left this set in TNF section 6.4: it is the global notch master
    // enable and now reads and writes NotchModel::globalEnabled.
    static constexpr int kTciStubSliceMax = 4;
    bool        m_tciGlobalMute{false};
    // AF / MON volume fallback defaults match the live-source defaults
    // they back-fill (AudioEngine::m_masterVolume{0.5f} and
    // TransmitModel::m_monitorVolume{0.5f} both = 50 linear).  Live
    // sources are non-null on a constructed RadioModel, so these stubs
    // are only read in degenerate test paths -- but using 50 instead of
    // 0 means a future change that drops the live-source guard won't
    // silently emit `volume:-60.0;` (full mute) on first client connect.
    // PR #279 review P1 (2026-05-22).
    int         m_tciAfLinear{50};
    int         m_tciMonLinear{50};
    // iqSampleRate fallback: prefers live connectionSampleRateHz(); this
    // stub is only read pre-connect.  Use the canonical HPSDR P2 baseline
    // (192 kHz, matches SampleRateCatalog::kDefaultSampleRate) so first
    // client connect doesn't see iq_samplerate:0; which real TCI clients
    // reject.  PR #279 review P1 (2026-05-22).
    int         m_tciIqSampleRate{192000};
    // Audio-stream config: per-client semantics live in TciClientSession;
    // these mirror "last value any client sent" for matrix-test parity.
    int         m_tciAudioSampleRate{48000};
    int         m_tciAudioStreamChannels{2};
    int         m_tciAudioStreamSamples{2048};
    // audioStreamSampleType: Thetis TCI wire-format tokens (audio_stream
    // sample-type field) are all lower-case in golden captures
    // ("float32" / "int16" / "int24" / "int32").  Default "Float32"
    // (capital F) made the first-connect frame non-canonical.
    // PR #279 review P1 (2026-05-22).
    QString     m_tciAudioStreamSampleType{QStringLiteral("float32")};
    // Per-slice DSP toggle stubs (set-and-read only; not wired to WDSP).
    // BIN and APF used to live here too; Phase 3F chip task_c1e6fbad routed
    // them to SliceModel::binauralEnabled / apfEnabled, which are wired to
    // RxChannel, and deleted their arrays rather than leaving state nothing
    // reads. NF followed them out in TNF section 6.4, onto
    // NotchModel::globalEnabled -- see the setRxNf comment in RadioModel.cpp.
    std::array<bool, kTciStubSliceMax> m_tciStubRxCtun{};

    // Non-owning view of the WDSP TX channel (WdspEngine::kTxChannelId,
    // == WDSP.id(1, 0)).
    // WdspEngine owns the channel via m_txChannels. This pointer is valid only
    // after m_wdspEngine->initializedChanged fires and createTxChannel(kTxChannelId) is
    // called inside the initializedChanged lambda. null before that.
    // Callers must guard: if (m_txChannel) { ... }.
    // Thread safety: read only from the main thread. WDSP TX processing happens
    // on the DSP thread (m_dspThread), but the run-flag mutations called here
    // (setRunning / setTuneTone) are non-realtime control-path calls that are
    // safe to call from the main thread per the WDSP API contract.
    // From Thetis dsp.cs:926-944 [v2.10.3.13] — WDSP.id(1, 0) = channel 1.
    TxChannel* m_txChannel{nullptr};
    QPointer<TxChannel> m_txEqProfileChannel;
    QList<QMetaObject::Connection> m_txEqProfileConnections;
    void bindTxEqProfileChannel(TxChannel* channel);
    void replayTxEqProfile();
    QPointer<TxChannel> m_cfcProfileChannel;
    QList<QMetaObject::Connection> m_cfcProfileConnections;
    void bindCfcProfileChannel(TxChannel* channel);
    void replayCfcProfile();

    // AM Mod Monitor analyzers: [0] TX I/Q tap, [1] PS feedback receiver.
    std::unique_ptr<AmModulationAnalyzer> m_amModTx;
    std::unique_ptr<AmModulationAnalyzer> m_amModFb;
    std::atomic<int>  m_amModFbStream{1};
    std::atomic<bool> m_amModFbWanted{false};
    std::atomic<bool> m_amModMoxOn{false};
    std::atomic<bool> m_amModTxTapEnabled{true};
    // R-IOS-13 / R-R3-49: a remote window's copy of the Core's readings.
    std::array<std::optional<AmModulationAnalyzer::Snapshot>, 2> m_stationModMonitor;

    // TX mic source — strategy interface for silence (3M-1a) or real mic (3M-1b).
    // Owned by RadioModel via unique_ptr. NullMicSource for 3M-1a; replaced with
    // PcMicSource / RadioMicSource in 3M-1b per user preference and board caps.
    // Not a QObject — no thread affinity. pullSamples() is called from whatever
    // thread drives the TX I/Q production loop; for 3M-1a (TUNE carrier via WDSP
    // gen1 PostGen) it is never actually invoked, since gen1 overwrites the input.
    // Master design §5.2 (3M-1a NullMicSource; 3M-1b concrete sources).
    std::unique_ptr<TxMicRouter> m_txMicRouter;

    // 3M-1b L.1: concrete mic-source objects owned by RadioModel.
    // Constructed in connectToRadio() after m_connection is live (so
    // PcMicSource has AudioEngine and RadioMicSource has a valid connection
    // pointer). Destroyed in teardownConnection() in reverse-construction order
    // (composite first, then radio, then pc) to avoid dangling raw pointers
    // inside CompositeTxMicRouter.
    //
    // When null (before first connect or after disconnect):
    //   m_txChannel->setMicRouter() is called with nullptr via teardownConnection,
    //   matching the G.1 convention for nulling injection pointers on teardown.
    //
    // PcMicSource does NOT inherit QObject — no Qt parent. AudioEngine lifetime
    // is RadioModel's lifetime, so the non-owning AudioEngine* is always valid
    // while m_pcMicSource is alive.
    //
    // RadioMicSource IS a QObject but its parent is set to nullptr here because
    // RadioModel manages its lifetime via unique_ptr. This matches the convention
    // used by TxChannel (non-owning view, managed externally).
    //
    // Plan: 3M-1b Task L.1. Pre-code review §0.3 + master design §5.2.4.
    std::unique_ptr<PcMicSource>           m_pcMicSource;
    // Radio codec (2026-09-30): the audio engine's radio output tap,
    // forwarding to the connection (connectRadioSpeakerOutput).
    std::unique_ptr<MasterMixAudioTap>     m_radioSpeakerTap;
    std::unique_ptr<RadioMicSource>        m_radioMicSource;
    // VAX TX consumer (added 2026-05-06, eager-borg-d64bed).  Pulls
    // audio from /nereussdr-vax-tx shared memory via AudioEngine and
    // is registered with m_compositeMicRouter via setVaxSource().
    // Reset before m_compositeMicRouter on teardown — see notes
    // around teardownConnection().
    std::unique_ptr<VaxTxMicSource>        m_vaxTxMicSource;
    std::unique_ptr<CompositeTxMicRouter>  m_compositeMicRouter;

    // ── 3M-1c Phase L: cross-cutting ownership ──────────────────────────────
    //
    // L.1 — MicProfileManager (chunk F).  QObject child of RadioModel so the
    // dtor cleans it up automatically.  Constructed once in the RadioModel
    // ctor; setMacAddress + load() are called per-connect inside
    // connectToRadio(); setMacAddress("") is called in teardownConnection so
    // mutators silently no-op while no radio is selected.
    MicProfileManager* m_micProfileMgr{nullptr};

    // Phase 4 Agent 4A of issue #167 — PaProfileManager.  QObject child of
    // RadioModel; mirrors m_micProfileMgr lifecycle exactly.  Constructed
    // once in the ctor; setMacAddress + load(connectedModel) are called
    // per-connect inside connectToRadio().  The active profile is read at
    // every drive-slider / TUNE callsite via paProfileManager()->activeProfile()
    // and passed by reference to TransmitModel::setPowerUsingTargetDbm.
    PaProfileManager* m_paProfileManager{nullptr};
    //
    // L.2 — TwoToneController (chunk I).  QObject child of RadioModel.
    // Construction-time deps that DON'T require a live connection
    // (TransmitModel, MoxController, SliceModel) are wired in the RadioModel
    // ctor; setTxChannel(...) is called inside the WDSP-init lambda once
    // m_txChannel is live.  setTxChannel(nullptr) is called in teardown.
    TwoToneController* m_twoToneController{nullptr};

    // 3M-4 Task 7: PureSignal coordinator.  Owned via unique_ptr (NOT a
    // raw QObject child) so the destructor can drain the polling timers
    // before the WdspEngine / TxChannel pointers are torn down — the
    // QObject child-deletion path doesn't guarantee that ordering.  See
    // PureSignal.h for the design.  Constructed inside the WDSP-init
    // lambda alongside TwoToneController; reset() in teardown.
    PureSignalSettings* m_pureSignalSettings{nullptr};
    DspAssetService* m_dspAssets{nullptr};
    PureSignalSessionFacade* m_pureSignalFacade{nullptr};
    StepAttenuatorFacade* m_stepAttFacade{nullptr};
    AlexAntennaFacade* m_alexAntennaFacade{nullptr};
    IoBoardHl2Facade* m_ioBoardFacade{nullptr};
    // R-R3-46: coalesces a remote window's OC matrix reloads.
    QTimer* m_remoteOcReloadTimer{nullptr};
    std::unique_ptr<PureSignal> m_pureSignal;

    // 3M-4 Task 17 chunk C: pscc() driver — pairs per-DDC IQ streams
    // (PS-feedback on DDC0, TX-monitor on DDC1) into paired blocks for
    // calcc.  Without this driver, calcc never runs and info[16] stays
    // at zero.  See PsccPump.h for the architectural narrative.  Owned
    // via unique_ptr alongside m_pureSignal so destruction ordering is
    // explicit (drain pump before TxChannel goes away).
    std::unique_ptr<PsccPump> m_psccPump;
    //
    // (Phase 3M-1c L.4 introduced a `std::unique_ptr<MicReBlocker>` here
    //  to bridge AudioEngine 720-sample emits to TxChannel 256-sample
    //  pushes.  The TX pump architecture redesign (2026-04-29) deleted
    //  MicReBlocker entirely; replaced with TxWorkerThread below.)
    //
    // 3M-1c TX pump architecture redesign — dedicated QThread that
    // drives the TX DSP pump off the main thread.  Mirrors Thetis's
    // `cm_main` worker-thread pattern (cmbuffs.c:151-168 [v2.10.3.13]):
    // QTimer-driven 5 ms tick, pulls 256-sample mic blocks via
    // AudioEngine::pullTxMic, calls TxChannel::driveOneTxBlock(samples,
    // 256).  Constructed inside the WDSP-init lambda once m_txChannel
    // is live; TxChannel is moveToThread'd to the worker before
    // startPump().  Teardown: stopPump() → quit() + wait() → move
    // TxChannel back to main thread → reset.  See plan §5.2.
    std::unique_ptr<TxWorkerThread> m_txWorker;

    // Phase 3M-1c TX pump v3 — TxMicSource (Thetis Inbound/cm_main port).
    // Constructed inside the WDSP-init lambda alongside TxWorkerThread,
    // wired into both the worker (consumer) and the connection (producer).
    // Teardown order: stopPump (worker exits) → micSource->stop (already
    // happens inside stopPump, but reset only after the worker is torn
    // down so the consumer side is fully disconnected).
    std::unique_ptr<class TxMicSource> m_txMicSource;

    // Phase 3R K-bench: RADE TX 24 -> txSampleRate upsampler.  Lazily
    // constructed on first txModemReady arrival once we know the
    // connection's txSampleRate() (P1 = 48 kHz; P2 = 192 kHz; per
    // RadioConnection.h:408 default + P2RadioConnection.h:265).
    // m_radeTxResamplerHwRate stores the rate the resampler was
    // built against so a reconnect at a different rate triggers a
    // rebuild rather than silently producing wrong-rate audio.
    //
    // m_radeTxMonoScratch / m_radeTxIqScratch are reused per emission
    // to avoid per-call allocations.  Both are sized once on first use.
    std::unique_ptr<Resampler> m_radeTxResampler;
    int                        m_radeTxResamplerHwRate{0};
    std::vector<float>         m_radeTxMonoScratch;
    std::vector<float>         m_radeTxIqScratch;

    // Phase 3R K-bench (bench feedback): the RADE RX speakers-side
    // upsamplers (24 -> 48 kHz, one per leg) moved to each RADE route on
    // the DSP worker (RxDspWorker::RadeRxRoute) with the RADE threads work.

    // Stage C2 — filter preset user-override store.
    // Constructed in RadioModel ctor; QObject child so dtor cleans up.
    FilterPresetStore* m_filterPresetStore{nullptr};

    // ── Phase 3J-2 H2: spot-system ownership ────────────────────────────────
    //
    // RadioModel becomes the wiring hub for Phase 3J-2's spot system. View
    // models are constructed first (the adapter slots below depend on
    // m_spotModel + m_rxDecodeModel + m_freeDvStationModel being live), then
    // the ingest clients. Each client emits spotReceived(DxSpot); a per-
    // source adapter slot translates that into the kvs map
    // SpotModel::applySpotStatus expects.
    //
    // None of the clients start their network I/O at construction time;
    // startConnection() / startListening() / startPolling() is the M3
    // follow-up task. H2 only wires the in-process signal graph.
    std::unique_ptr<SpotModel>            m_spotModel;
    std::unique_ptr<SpotTableModel>       m_spotTableModel;
    std::unique_ptr<FreeDVStationModel>   m_freeDvStationModel;
    std::unique_ptr<RxDecodeModel>        m_rxDecodeModel;
    std::unique_ptr<DxccColorProvider>    m_dxccColorProvider;

    std::unique_ptr<DxClusterClient>      m_dxCluster;      // DX cluster (DxSpider / AR-Cluster / CC-Cluster)
    std::unique_ptr<DxClusterClient>      m_rbn;            // Reverse Beacon Network (RBN-suffixed spotter)
    std::unique_ptr<WsjtxClient>          m_wsjtx;
    std::unique_ptr<SpotCollectorClient>  m_spotCollector;
    std::unique_ptr<PotaClient>           m_pota;
    std::unique_ptr<FreeDVReporterClient> m_freeDvReporter;
    std::unique_ptr<PskReporterClient>    m_pskReporter;
    // Parity Task 19 (R-IOS-25): starts, stops and follows the clients
    // above (all but FreeDV Reporter), locally and on the Core; the
    // mirrored `spotSources` object.
    std::unique_ptr<SpotSourceHost>       m_spotSourceHost;
    // Parity Task 19: in a remote window, the Core's spot ids (the `spots`
    // stream's record ids) and the SpotModel index each is shown under,
    // beside this window's own WSJT-X and SpotCollector spots.
    QHash<QString, int>                   m_stationSpotIndex;
    // Parity Task 21: the Core's radios, by stream id, in the Core's order.
    QList<StationRadioEntry>              m_stationRadioEntries;

    // TNF (design section 5): notch store. Persisted globally rather than
    // per-MAC (design D3) because a notch tracks a QRM source at the
    // operator's location and band, not a property of the radio.
    std::unique_ptr<NotchModel>           m_notchModel;

    // Phase 3R-bridge: drives the freedv-gui-style RADE "sync-only"
    // rx_report upload (empty callsign, "RADEV1" mode, 1 Hz) into
    // m_freeDvReporter. Ported from freedv-gui src/main.cpp:
    // 1971-1996 [@77e793a]. Path A (callsign-decoded via EOO) is
    // separately driven by RadioModel::onRadeTextDecoded.
    std::unique_ptr<FreeDVRadeReporterBridge> m_radeReporterBridge;

    // Monotonic index passed to SpotModel::applySpotStatus on every adapter
    // dispatch. Increments once per emitted spot regardless of source.
    int m_nextSpotIndex{0};

    // ── Phase 3R Task I5: per-slice RADE sync state cache ───────────────────
    //
    // Latest RADE decoder sync state per slice ID, updated by
    // onRadeSyncChanged on every transition. radeSynced(sliceId) reads
    // this; a missing key reads as false (slice never had RADE wired).
    //
    // Stored on RadioModel rather than SliceModel so future UI surfaces
    // can iterate sync state across all slices without recursing into
    // each slice's WDSP channel pointer. The dedup logic in
    // onRadeSyncChanged also lives here for the same reason.
    QHash<int, bool> m_radeSyncedSlices;

    // 2026-05-12 bench: per-slice timestamp of the most recent sync
    // FALLING edge (true -> false transition).  Used by onRadeSyncChanged
    // to debounce the "clear cached speaker callsign on sync rise"
    // behaviour: only count a rising edge as a "new transmission /
    // new speaker" event if sync was down for >= kRadeSyncDropClearDebounceMs.
    // Brief flickers (< debounce) keep the previous over's callsign on
    // the VFO flag.  Per bench design refinement 2026-05-12 (option B
    // debounce-by-sync-loss-duration).
    QHash<int, QDateTime> m_radeSyncDropAt;
    static constexpr int kRadeSyncDropClearDebounceMs = 2000;

    // 2026-07-27 (ANAN-G2E lockup): discovery quiet period after any
    // teardown.  Must outlast (a) the radio's stop-transition settling
    // (observed death window: up to ~1 s after run=0 in the 2026-07-27 TZSP
    // captures) and (b) the gateware's ~2 s C&C deadman edge
    // (Hermes.v:398-414, HW_TIMEOUT at 250e6 cycles @ 125 MHz), so the first
    // probe a stopped radio hears arrives with its state machines fully
    // settled.  Thetis's post-stop behaviour is total silence; 3 s of quiet
    // approximates that without making the reopened panel feel dead.
    static constexpr std::chrono::milliseconds kPostDisconnectScanQuietMs{3000};

    // 2026-05-12 bench: FreeDV Reporter freq-publish throttle.
    //
    // Spinning the VFO would otherwise fire a Socket.IO freq_change
    // event on every sub-Hz movement (mouse wheel cadence) -- the
    // qso.freedv.org server gets DoS'd and other operators see the
    // dashboard flicker.  Throttle policy (per JJ bench design
    // 2026-05-12):
    //
    //   1. Trailing dwell: restart a single-shot timer on every freq
    //      change; only publish when the timer expires
    //      (kFreedvFreqDwellMs = 7000 ms).  Spinning across a band
    //      publishes exactly once, 7 s after the user stops.
    //   2. Band-jump fast-path: if the new freq is >= kFreedvFreqJumpHz
    //      (100 kHz) from the last *published* freq, bypass the dwell
    //      and publish immediately -- band changes don't lag.
    //   3. MOX force-publish: TX engage flushes any pending dwell so
    //      the reporter never shows "TXing on stale freq."
    //
    // Driven by publishFreedvFrequencyDwelled() called from
    // SliceModel::frequencyChanged.  m_freedvLastPublishedHz is the
    // baseline for the band-jump comparison.  Initial publish on
    // Socket.IO ACK bypasses this and uses setFrequency() directly so
    // the first packet establishes the baseline.
    QTimer*  m_freedvFreqDwellTimer{nullptr};
    quint64  m_freedvLastPublishedHz{0};
    quint64  m_freedvPendingHz{0};
    static constexpr int     kFreedvFreqDwellMs = 7000;
    static constexpr quint64 kFreedvFreqJumpHz  = 100'000;

    // Phase 3P-II Task 19: PGXL / TGXL / Tuner ownership.
    // All three are QObject children of RadioModel (parent=this, constructed
    // once in the ctor). Raw pointer pattern follows m_moxController et al.
    PgxlConnection* m_pgxlConnection{nullptr};
    TgxlConnection* m_tgxlConnection{nullptr};
    StationTgxlController* m_stationTgxl{nullptr};
    // R-R3-49 (parity Task 8): scanTgxlLanForStation's listening window.
    int m_tgxlLanScanWindowMs{kTgxlLanScanWindowMs};
    // R-R3-49 (parity Task 9): scanPgxlLanForStation's listening window,
    // the local dialog's (kTgxlLanScanWindowMs, the same three seconds).
    int m_pgxlLanScanWindowMs{kTgxlLanScanWindowMs};
    // Group B fix wave (M2): the scan listening for each device (keyed by
    // the listener's object name) and the requests waiting for its answer.
    struct StationLanScan {
        QPointer<LanDiscovery> discovery;
        std::vector<std::function<void(const QString&)>> waiting;
    };
    QHash<QString, StationLanScan> m_stationLanScans;
    StationPgxlController* m_stationPgxl{nullptr};
    TunerModel*     m_tunerModel{nullptr};
    AmplifierModel* m_amplifierModel{nullptr};
    RfKitModel*     m_rfKitModel{nullptr};
    // R-R3-47 / R-R3-48: the Core's RF-Kit controller, station TCI server
    // and its state, and the RF-Kit's band follow over that server.
    StationRfKitController* m_stationRfKit{nullptr};
    StationTciModel*        m_stationTciModel{nullptr};
    // M6: owned here and destroyed first in ~RadioModel (they hold this
    // model's slices and receivers), not through Qt parenting.
    std::unique_ptr<StationTciController> m_stationTci;
    std::unique_ptr<RfKitBandFollow>      m_rfKitBandFollow;
    // Follow-up 3: accessory requests whose refusal their page shows.
    QHash<quint32, QPointer<QObject>> m_pageShownAccessoryRequests;

    // Phase 3P-III: RF-Kit RF2K-S connection. unique_ptr with Qt parent=this
    // so destruction order is deterministic and QObject hierarchy is intact.
    // Constructed once in the ctor; non-null from that point.
    std::unique_ptr<Rf2ksConnection> m_rfKitConnection;

    // Phase 3P-III review fix I2: last-seen RF-Kit operate state, used to gate
    // externalAmpOperateChanged so the cross-vendor signal fires only on actual
    // transitions, not on every 1 Hz REST poll. Initialized false (STANDBY).
    bool m_lastRfKitInOperate{false};

    // Amplifier presence and operate-state cache (driven by onPgxlStatus).
    bool m_hasAmplifier{false};
    bool m_ampOperate{false};

    // TGXL autotune orchestration state (NereusSDR-native).
    // Event-driven flow (mirrors the FlexAPI interlock handshake pattern):
    //   1. startTgxlAutotune() snapshots PGXL state into m_pgxlSavedOperate
    //      and sets m_tgxlAutotuneInProgress + m_pgxlStandbyPending
    //   2. Send `operate=0` to PGXL (if it was operating)
    //   3. Wait for ampStateChanged(false) confirmation (PGXL transitioned
    //      to STANDBY), then call continueTgxlAutotuneAfterStandby()
    //   4. Engage local TUN carrier; set m_awaitingInterlockForAutotune
    //   5. Wait for interlockGranted from SmartSdrApiListener (TGXL has
    //      now received S0|interlock state=TRANSMITTING and knows PTT is
    //      live), then send `autotune` to TGXL on :9010
    //   6. On tuningChanged(false) -> drop carrier -> manualMoxChanged(false)
    //      -> restore PGXL to m_pgxlSavedOperate state
    //
    // m_tgxlAutotuneFromHardware: true if TGXL initiated (LAN PTT). Skips
    //   the `autotune` cmd because TGXL is already sweeping. We also skip
    //   the interlockGranted wait in that case.
    // m_awaitingInterlockForAutotune: true between continueTgxlAutotune-
    //   AfterStandby and the interlockGranted handler. Gates the autotune
    //   command on TRANSMITTING actually being broadcast so TGXL sees PTT
    //   before it gets the sweep command (previously a 200 ms fixed timer
    //   raced the interlock chain and caused first-press "no PTT in"
    //   aborts on cold caches).
    // 1500 ms failsafe in case PGXL never confirms standby (e.g. amp
    //   disconnected / unresponsive) -- proceed anyway and log a warning.
    // 1500 ms failsafe also on the interlockGranted wait, for the same
    //   degraded-amp case (e.g. amp disconnected mid-cycle before ACK).
    bool m_pgxlSavedOperate{false};
    bool m_tgxlAutotuneInProgress{false};
    bool m_pgxlStandbyPending{false};
    bool m_tgxlAutotuneFromHardware{false};
    // Task 77: the device a remote autotune keys for, and the connection
    // it asked on (KeyerIdentity::session); empty for the Core's own
    // cycles.
    QByteArray m_tgxlAutotuneDeviceId;
    QString m_tgxlAutotuneSession;
    /// Task 77: the tuner reported tuning during a device's cycle.
    bool m_tgxlDeviceCycleSawTuning{false};
    /// Task 77: how long a device's cycle waits for the tuner to start its
    /// sweep once the carrier is up (TunerApplet's short watchdog, 3 s).
    static constexpr int kTgxlDeviceCycleStartMs = 3000;
    static constexpr int kTgxlCarrierReadyMs = 3000;
    static constexpr int kTgxlRfSettleMs = 150;
    // Completion ownership: actual channel, its latest run request and
    // the tune cycle that requested it. The completion is always queued.
    QPointer<TxChannel> m_tgxlCarrierChannel;
    quint64 m_tgxlCarrierSequence{0};
    quint64 m_tgxlCarrierCycle{0};
    bool m_tgxlCarrierReady{false};
    bool m_tgxlSettlePending{false};
    bool m_tgxlCommandSent{false};
    /// Task 77: the cycle ended (or never keyed): the amplifier's state
    /// restored, the flags cleared.
    /// Task 77 fix round 4: `unkeyedReason`, the words for a cycle that ends
    /// without keying because of the amplifier (carried on tgxlAutotuneEnded
    /// to the device whose cycle it was).
    /// Tune-ended lane: every end before the carrier keyed names its own
    /// reason; RemoteKeying fills in a backstop for one that does not.
    void finishTgxlAutotuneCycle(const QString& unkeyedReason = QString());
    /// Task 77 fix round 4: counts cycles, so a failsafe timer left from an
    /// ended cycle never acts on a new one.
    quint64 m_tgxlCycleGeneration{0};
    /// TGXL tune lane (JJ's ruling, 2026-09-30): the tuner's front-panel
    /// TUNE is taking transmit from another device (ruling 8.9), for the
    /// cycle of this generation; its carrier keys when the take ends.
    bool m_tgxlTakePending{false};
    quint64 m_tgxlTakeGeneration{0};
    /// TGXL tune lane fix round (2026-10-01): this cycle is the tuner's own
    /// front-panel TUNE (its `transmit tune on`, from its address, not an
    /// answer to our autotune); only such a cycle takes transmit.
    bool m_tgxlAutotuneTunerPress{false};
    /// Set by the LAN PTT handler for the one startTgxlAutotune call it
    /// makes; beginTgxlAutotune takes it.
    bool m_tgxlPendingTunerPress{false};
    /// TGXL tune lane round 2: everything this computer sent the tuner
    /// that it may answer with its own `transmit tune on/off` (each
    /// `autotune`, each tune=1/0 broadcast), counted; a tune on that
    /// answers none of them is the tuner's own TUNE.
    TgxlAnswerTracker m_tgxlAnswers;
    /// Round 3: counts :9010 connects; `autotune` entries carry it.
    quint64 m_tgxlLinkEpoch{0};
    /// Round 3: the tune=1 frames sent and the echoes answered in the
    /// current tune, for the answer log.
    bool m_tgxlLastTuneSent{false};
    int m_tgxlTuneFramesSent{0};
    int m_tgxlTuneEchoes{0};
    /// Whether `peer` (a :4992 client) is the connected Tuner Genius.
    bool tgxlIsPeer(const QHostAddress& peer) const;
    /// Monotonic milliseconds for m_tgxlAnswers.
    static qint64 tgxlAnswerNowMs();
    /// Task 77 fix round 4: a relayed operate=1 held while RF may flow.
    bool m_relayedPgxlOperateHeld{false};
    void retryHeldRelayedPgxlOperate();
    /// Task 77 fix wave (I3, I4): the amplifier is never switched between
    /// standby and operate while RF flows. A restore owed while MOX is on
    /// or still walking back to receive waits here and is sent once MOX
    /// reads receive (or carried into the next cycle's saved state).
    bool m_pgxlRestoreWhenUnkeyed{false};
    /// MOX is on, or its walk has not reached receive.
    bool tgxlRfFlowing() const;
    /// Sends operate=1 to the Power Genius (the restore itself).
    void sendPgxlOperateRestore();
    /// Task 77 fix round 2: operate=0 or operate=1 from this model (its own
    /// command, so the owed restore survives it).
    void sendPgxlOperate(bool operate);
    /// Task 77 fix round 2: a key may be about to start (a PTT source
    /// down, two-tone running or settling, the session server's probe).
    bool ampKeyPending() const;
    /// Task 77 fix round 2: the amplifier may be switched now.
    bool ampSwitchAllowed() const;
    AmpKeyPendingFn m_ampKeyPending;
    OtherDeviceHoldsRefusalFn m_otherDeviceHoldsRefusal;
    /// The unconfirmed operate command (true: operate=1), and when it was
    /// written. Empty while the amplifier is not changing over.
    std::optional<bool> m_ampCommandedOperate;
    QElapsedTimer m_ampCommandClock;
    /// Set while this model writes its own operate command.
    bool m_ampOwnCommand{false};
    /// Task 77 fix round 3: the sequence of the unconfirmed operate
    /// command; an error reply to it ends the changeover.
    /// Task 77 fix round 3, round 4: every operate command written and not
    /// yet confirmed, oldest first; an error reply removes its own, and the
    /// changeover ends only when none is left.
    struct AmpCommand {
        quint32 seq{0};
        bool operate{false};
    };
    QList<AmpCommand> m_ampPendingCommands;
    void onPgxlOperateCommanded(bool operate, quint32 seq);
    void onPgxlReplyRefused(quint32 seq);
    /// Task 77 fix round 3: sets m_tgxlAutotuneInProgress and announces it.
    void setTgxlAutotuneInProgress(bool running);
    void endAmpChangeover();
    /// The RF-flow gate's third condition: a key's RF waits here.
    bool m_rfHeldForAmp{false};
    QTimer* m_ampHoldTimer{nullptr};
    /// Opens the RF gate (TxChannel::setRunningAsync(true)) once txReady
    /// and the interlock have both come, unless the amplifier is changing
    /// over; then the RF is held until it has.
    void openTxRfGate();
    void onAmpHoldDeadline();
    /// The cycle itself (startTgxlAutotune's body before Task 77). Returns
    /// the refusal when it did not start (empty when it started, or was a
    /// hardware echo of a running cycle).
    QString beginTgxlAutotune(bool fromHardware);
    bool m_awaitingInterlockForAutotune{false};
    void continueTgxlAutotuneAfterStandby();
    void onTgxlRfGateOpened(TxChannel* channel, quint64 sequence);
    bool tgxlCarrierEligible(quint64 cycle, TxChannel* channel, quint64 sequence) const;
    void scheduleTgxlAutotune();
    void sendTgxlAutotuneCmd(quint64 cycle, TxChannel* channel, quint64 sequence);
    void armTgxlSweepStartWatchdog();

    // RF-flow gate state (NereusSDR-native, deck item #3).
    //
    // When MoxController::txReady fires (rfDelay elapsed, radio ready to
    // TX), we normally call TxChannel::setRunning(true) which causes the
    // audio pump to start feeding samples to the radio. With an external
    // amp like PGXL in the chain, this is too early: PGXL needs to ACK
    // PTT_REQUESTED and switch its relays from bypass to amp path BEFORE
    // the carrier arrives, or it sees ~250 ms of carrier through bypass
    // into a (possibly unmatched) antenna and intermittently trips its
    // own SWR protection.
    //
    // The fix: defer setRunning(true) until interlockGranted fires
    // (TRANSMITTING was just broadcast to all amps; PGXL has ACKed or
    // the 500 ms lenient grant has fired). The lambda in the ctor reads
    // this flag and calls setRunning(true) when the grant arrives.
    //
    // 1500 ms failsafe (same budget as the autotune gate): if interlock-
    // Granted doesn't fire, start the audio anyway so the operator isn't
    // stuck with a silent TX.
    bool m_awaitingInterlockForTx{false};
    // RF-flow gate two-condition tracker (deck item #3, ordering fix
    // 2026-05-20 21:19): TxChannel::setRunning(true) needs BOTH
    //   (a) MoxController::txReady fired (radio is in TX mode), and
    //   (b) SmartSdrApiListener::interlockGranted fired (amp in
    //       TRANSMITTING state, relays switched to amp path).
    // Whichever signal fires SECOND triggers setRunning. We track each
    // independently because Qt event-queue ordering can race; we cannot
    // rely on one always firing before the other when amp ACK is fast.
    // m_txReadyReceived is set in the txReady wire, cleared on TX-off
    // (moxStateChanged(false)). m_awaitingInterlockForTx is set in the
    // txAboutToBegin wire (BEFORE PTT_REQUESTED is sent so the gate is
    // armed before any interlockGranted can fire) and cleared by the
    // grant handler.
    bool m_txReadyReceived{false};

    // Task 33: set by stopTransmitNow, cleared when the next key begins
    // (MoxController::txAboutToBegin). While set, a keying step queued
    // before the stop (hardwareFlipped(true), txReady, an interlock grant)
    // does not key the radio.
    bool m_transmitStopHold{false};
    // Task 33: the TX channel drain the TX→RX walk is waiting for.
    quint64 m_pendingTxDrainSequence{0};

    // Phase 3P-II Task 86: TxInterlockPolicy -- NereusSDR-native TX gate.
    // Qt parent-ownership (parent=this); non-null from construction time.
    TxInterlockPolicy* m_txInterlockPolicy{nullptr};

    // Phase 3P-II Phase 4 Task 89: TuneMemoryStore -- shared per-(antenna,band)
    // TGXL relay position cache. Qt parent-ownership (parent=this); non-null from
    // construction time. Shared (non-owning) with TgxlAdvancedPage and TunerApplet.
    TuneMemoryStore* m_tuneMemoryStore{nullptr};

    // Phase 3P-II Phase 4 Task 94: FaultLog ring buffers for PGXL and TGXL.
    // Qt parent-ownership (parent=this); non-null from construction time.
    // Shared (non-owning) with PgxlAdvancedPage and TgxlAdvancedPage.
    FaultLog* m_pgxlFaultLog{nullptr};
    FaultLog* m_tgxlFaultLog{nullptr};
    // R-R3-47 / R-R3-22: see the accessors.
    FaultLog* m_rfkitFaultLog{nullptr};
    ConnectionDiagnostics* m_pgxlDiagnostics{nullptr};
    ConnectionDiagnostics* m_tgxlDiagnostics{nullptr};
    AccessoryDataModel* m_accessoryDataModel{nullptr};
    AccessorySettingsModel* m_accessorySettingsModel{nullptr};
    StationAccessoryData* m_stationAccessoryData{nullptr};

    // Phase 3P-II Phase 4 Task 94: last known PGXL state string.
    // Tracks "previous state" so we capture only on FAULT *transitions*
    // (not on every repeated FAULT status push).
    QString m_lastPgxlState;

    // FlexRadio UDP 4992 discovery beacon. Owned by RadioModel (Qt parent=this).
    // Constructed once in the ctor; configured and started in connectToRadio()
    // once m_lastRadioInfo.macAddress is known; stopped in teardownConnection().
    // Allows PGXL/TGXL to auto-discover NereusSDR in their FlexRadio dropdown
    // without any manual IP entry.
    class FlexRadioDiscoveryBroadcaster* m_flexBroadcaster{nullptr};
    // R-R3-22: connectToRadio configured the beacon for this radio. It
    // runs only while this is set, 4O3A is on and PGXL_BroadcastDiscovery
    // is True (updateFlexBeacon).
    bool m_flexBeaconConfigured{false};
    void updateFlexBeacon();
    // R-R3-22 / R-R3-47: set by setStationBind on the Core only.
    std::optional<StationNetwork::StationBind> m_stationBind;
    void applyStationBind();

    // Passive SmartSDR API listener on TCP 4992. Bench-recon stub: logs every
    // line PGXL sends so we can design the response layer in a follow-up.
    // Phase 3P-II follow-up: replace with a full SmartSDR API server.
    class SmartSdrApiListener* m_smartSdrListener{nullptr};
    bool m_remoteFourO3AEnabled{false};
    // R-R3-47: the Core's RF-Kit switch as a remote window last heard it.
    bool m_remoteRfKitEnabled{false};
    // R-R3-49: the last isTransmitting() announced (the Core's), and the
    // Core's value as a remote window last heard it.
    bool m_transmitting{false};
    bool m_remoteTransmitting{false};
    // R-R3-49 (parity Task 6): the Core's TX inhibit as a remote window
    // last heard it.
    bool m_remoteTxInhibited{false};
    // HL2 port part 2: txInhibitReason(): this model's (refreshed from its
    // TxInhibitMonitor) or, on a remote window, the Core's.
    QString m_txInhibitReason;
    void refreshTxInhibitReason();
    static QString ioBoardFaultReason(quint8 code);
    // On the Core: emit paTransmitBandChanged when paOnAirBandIndex moved.
    void announcePaTransmitBand();
    // PA on-air gate re-review, Important C: the Core's paTransmitBand as a
    // remote window last heard it, and whether it has (an older Core never
    // sends it). On the Core, the value last announced.
    int m_stationPaTransmitBand{-1};
    bool m_stationPaTransmitBandKnown{false};
    int m_announcedPaTransmitBand{-2};
    // Remote-window parity Task 16: the Core's dspInfoVersion and
    // dspAssetVersion as a remote window last heard them; the last DSP
    // Options apply time; the filter curve.
    int m_stationDspInfoVersion{0};
    // Remote-window parity Task 30: the Core's txDisplayVersion.
    int m_stationTxDisplayVersion{0};
    // Remote-window parity Task 33: the Core's txReadingsVersion and the
    // window's copy of its `txState`; the radio's last raw PA readings.
    int m_stationTxReadingsVersion{0};
    QPointer<TransmitState> m_stationTransmitState;
    PaRawAdc m_paRawAdc;
    int m_stationDspAssetVersion{0};
    qint64 m_dspOptionsLastApplyMs{0};
    bool m_stationRadioChangeUnderway{false};
    QString m_stationRadioWaiting;
    // Remote-window parity Task 22: the Core's logging categories and log.
    QString m_remoteLogCategories;
    QStringList m_stationCoreLog;
    int m_stationCoreLogViewers{0};
    FilterResponse m_coreFilterResponse;
    bool m_coreFilterResponseWanted{false};
    bool m_coreFilterResponseDirty{false};
    quint32 m_coreFilterResponseCommand{0};
    QList<QMetaObject::Connection> m_coreFilterResponseSliceConns;
    void requestCoreFilterResponse();
    void watchCoreFilterResponseSlice();
    SliceModel* coreFilterResponseSlice() const;
    struct MiniFilterResponseState {
        FilterResponse response;
        quint64 serial{0}; // changes when demand is removed and later recreated
        quint32 command{0};
        bool dirty{false};
        QList<QMetaObject::Connection> connections;
    };
    QHash<int, MiniFilterResponseState> m_miniFilterResponses;
    friend class TestMiniFilterResponseLifecycle;
    quint64 m_nextMiniFilterResponseSerial{0};
    quint64 m_miniFilterDemandSetEpoch{0};
    void requestMiniFilterResponse(int sliceId);
    // R-R3-32 (parity Task 6): the Core's PA readings in a remote window,
    // and in a local one whether a telemetry sample has reported the PA
    // current and the PA temperature since connect.
    PaReadings m_corePaReadings;
    // R-R3-32 (parity Task 14): the Core's HL2 link in a remote window.
    Hl2LinkFigures m_coreHl2LinkFigures;
    // Remote role: the Core's connection age at m_coreConnectionAgeClock's
    // start (applyCoreConnectionAge).
    std::optional<qint64> m_coreConnectionAgeMs;
    QElapsedTimer m_coreConnectionAgeClock;
    // R-R3-46 (parity Task 14): HL2 Options' I2C reads waiting on the
    // radio (local: the Core's own and a local window's), and a remote
    // window's I2C and output pin requests waiting on the Core.
    struct PendingIoBoardRead {
        quint64 id = 0;
        quint8 address = 0;
        quint8 reg = 0;
        bool sent = false;
        IoBoardI2cDone done;
    };
    QList<PendingIoBoardRead> m_pendingIoBoardReads;
    quint64 m_nextIoBoardReadId{1};
    bool m_ioBoardToolWired{false};
    QHash<quint32, IoBoardI2cDone> m_stationIoBoardRequests;
    // Why an I2C transaction cannot reach the radio now, or empty.
    QString ioBoardI2cUnreachableReason() const;
    void wireIoBoardTool();
    void enqueueIoBoardTxn(quint8 address, quint8 reg, bool write, quint8 value);
    void finishIoBoardRead(quint64 id, bool ok, qint64 value, const QString& reason);
    // A read the tool waits on (address, register), answered to `done`;
    // holds the I/O board poll's pause until it is answered or gives up.
    void expectIoBoardRead(quint8 address, quint8 reg, IoBoardI2cDone done);
    // The poll pauses while any read above is outstanding (mi0bot
    // SetI2CPollingPause).
    void updateIoBoardPollingPause();
    bool m_paCurrentReported{false};
    bool m_paTemperatureReported{false};
    // R-R3-46 (parity Task 6): the remote window's PA reload.
    QTimer* m_remotePaReloadTimer{nullptr};
    void reloadRemotePaState();
    // R-R3-49 (parity Task 1): isCoreOnAir() as last announced.
    bool m_coreOnAir{false};
    void updateCoreOnAir();
    // The Radio Status page's PTT source (RadioStatus::activePttSource):
    // in a local window from this model's key, in a remote window from the
    // Core's mirrored `txState`, both through pttSourceForKey.
    void refreshRadioStatusPtt();
    QMetaObject::Connection m_stationTransmitStateConnection;
    bool m_remoteFourO3AListening{false};
    QString m_remoteFourO3AListenerError;
    QTimer* m_accessoryBandTimer{nullptr};
    QMetaObject::Connection m_accessoryFrequencyConnection;
    QMetaObject::Connection m_accessoryModeConnection;
    // The HL2 I/O board poll's TX VFO (rebindIoBoardSlice).
    QMetaObject::Connection m_ioBoardFrequencyConnection;
    QMetaObject::Connection m_ioBoardModeConnection;
    void rebindIoBoardSlice();
    QMetaObject::Connection m_accessoryBandConnection;
    void rebindAccessorySlice();
    void publishAccessoryBand();
};

} // namespace NereusSDR
