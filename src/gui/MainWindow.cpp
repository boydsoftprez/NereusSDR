// =================================================================
// src/gui/MainWindow.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/dsp.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/radio.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-02 — Draft-only edits and inert cached previews by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Atomic container arrangement and reserved chrome by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02  J.J. Boyd / KG4VCF. TX letters share the guarded flag
//                Take and select action, with current access and target
//                lifetime checks. AI-assisted via OpenAI Codex.
//   2026-10-02 - J.J. Boyd (KG4VCF). Clarity grid and waterfall output
//                follow the selected pan and survive original-pan retirement.
//                AI-assisted implementation via OpenAI Codex.
//   2026-10-01  J.J. Boyd / KG4VCF. Opt-in numeric TX-choice timestamps
//                for RX history diagnosis. AI-assisted via OpenAI Codex.
//   2026-10-01  J.J. Boyd / KG4VCF. Remote Max Bin follows the window
//                host of an empty-key primary slice, with receiver ownership
//                guards. AI-assisted via OpenAI Codex.
//   2026-09-30 - J.J. Boyd (KG4VCF). Fix round 2 (minor 3): the disconnect
//                comment says TX applet PS-A shows disabled, not hidden, for
//                the unknown board. AI-assisted via Anthropic Claude Code.
//   2026-09-30 - J.J. Boyd (KG4VCF). Fix round 1 (minor 4): the container's
//                MOX, TUNE and 2-TONE refresh when the window's link to the
//                Core or the Core's waiting for a radio changes.
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30 - J.J. Boyd (KG4VCF). Fix wave, hosting 2-TONE parity: a
//                hosting window's 2-TONE (TX applet and container) asks to
//                take transmit as its MOX and TUNE do (requestDesktopKey).
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30 - J.J. Boyd (KG4VCF). Fix wave GUI-I5: the container's MON
//                and PS-A hooks carry the transmit holder's reason, as the
//                TX applet's do. GUI-I1: abandonTxBadgeTake rejects the
//                badge's open take question. AI-assisted via Anthropic
//                Claude Code.
//   2026-09-30 - J.J. Boyd (KG4VCF). Station VOX: a hosting window's VOX
//                (the TX applet's button, Setup's Enable VOX) is disabled,
//                naming the holder, while another device holds transmit.
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30 - J.J. Boyd (KG4VCF). Shared-input filters (ruling (d)):
//                the CH label's tooltip is the receive low-pass reason.
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30 - J.J. Boyd (KG4VCF). The tooltip comment covers the HL2's
//                high-pass sentence. AI-assisted via Anthropic Claude Code.
//   2026-09-30 - J.J. Boyd (KG4VCF). The filter indicators also refresh on
//                RadioModel::lowPassHoldChanged. AI-assisted via Anthropic
//                Claude Code.
//   2026-09-30 - J.J. Boyd (KG4VCF). Level Cal fix wave: the grid follow
//                guard holds the saved follow at the user's value while a
//                run holds it off. The step attenuator's ceiling is the
//                Core's (BoardCapsTable::stepAttMaxDb). AI-assisted via
//                Anthropic Claude Code.
//   2026-09-29 - J.J. Boyd (KG4VCF). A regained RADE lock repaints the VFO
//                flag from the slice's SNR, which a remote window's Core
//                does not resend when it is unchanged. AI-assisted via
//                Anthropic Claude Code.
//   2026-09-29 - J.J. Boyd (KG4VCF). Remote parity on the air: the
//                transmit settings stay live while the Core's radio is on
//                the air on a Core at transmitSettingsVersion 13, as in a
//                local window; against an older Core they stay disabled
//                with the reason. AI-assisted via Anthropic Claude Code.
//   2026-09-27 - J.J. Boyd (KG4VCF). Parity Task 23 control UI: the TCI
//                 status and log identify the Core and this window.
//                 AI-assisted implementation via OpenAI Codex.
//   2026-09-27 - Task 25: hide the PA-trip slot until its Andromeda/Ganymede
//                 CAT producer is ported. J.J. Boyd (KG4VCF), AI-assisted
//                 via OpenAI Codex.
//   2026-09-24 - J.J. Boyd (KG4VCF). R-R3-49 / R-R3-21: menu items and
//                 status bar items whose feature is not built yet are
//                 hidden through UnbuiltFeatures (local and remote
//                 windows). AI-assisted via Anthropic Claude Code.
//   2026-09-24 - J.J. Boyd (KG4VCF). R-R3-45 fix wave: each slice flag
//                 also learns whether the headphones are turned on.
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24 - J.J. Boyd (KG4VCF). R-R3-45 Task 2: a remote window opens
//                 this computer's headphones when they are enabled, and each
//                 slice flag learns why a receiver on the headphones is
//                 silent on the Core's side. AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-23 - J.J. Boyd (KG4VCF). R-R3-47 / R-R3-22: the Power Genius
//                 gauge conversion moved to the Core side
//                 (PgxlStatusGauges); AmpApplet and Rf2ksApplet read
//                 RadioModel's AmplifierModel and RfKitModel. AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-23 - J.J. Boyd (KG4VCF). R-R3-45: every slice flag learns
//                 whether a headphones output is open, so a receiver on
//                 the headphones with none set up says why it is silent.
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-23 - J.J. Boyd (KG4VCF). R3 receiver audio fix wave (R-R3-42,
//                 R-R3-44): a receiver's audio stopping raises one plain
//                 toast (ReceiverStopNotices), not one from TCI and another
//                 per VAX channel; none for what the window's status
//                 already shows. AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-23 - J.J. Boyd (KG4VCF). R-R3-42: the TCI server asks the Core
//                 for a receiver's audio while an app listens, and its
//                 refusals and stops reach a toast and the TCI log window
//                 in plain words. AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-23 - J.J. Boyd (KG4VCF). R3 Setup fix wave (R-R3-21, R-R3-10):
//                 while connected to a Core whose settings have not
//                 arrived, Setup says "The Core has not sent its
//                 settings." instead of asking to connect. AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-23 - J.J. Boyd (KG4VCF). R-R3-21 / R-R3-10 / R-R3-17: Setup
//                 opens in a remote window whether or not it is connected;
//                 createSetupDialog() and applyRemoteRoleGating() push the
//                 Core's settings availability instead of refusing with a
//                 toast. AI-assisted implementation via Anthropic Claude
//                 Code.
//   2026-09-23 - J.J. Boyd (KG4VCF). R-R3-23: the title-bar master output
//                 reaches this computer's engine through
//                 RadioModel::localAudioDevices(), and a remote window
//                 skips the VAX first-run check. AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-23 - J.J. Boyd (KG4VCF). R-R3-21 / R-R3-09: a Core's refusal of
//                 a remote window's notch move, toggle or delete is shown.
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-23 - J.J. Boyd (KG4VCF). R-R3-21: the RX applet takes the
//                 negotiated transmit permission for its XIT row and TX
//                 passband Shift-click, and the RADE applet for its
//                 profile combo and Reset vocoder. AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-23 - J.J. Boyd (KG4VCF). R-R3-46: Radio > Protocol Info shows
//                 the Core's radio in a remote window. AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-23 - J.J. Boyd (KG4VCF). R-R3-46 / R-R3-21: a remote window's
//                 attenuator controls follow the Core's `stepAtt` object
//                 while the Core offers it, the Core's refusals of an
//                 attenuator edit are shown in user words, and the
//                 overload alarm lights on the Core's overload report.
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-23 - J.J. Boyd (KG4VCF). R-R3-46 / R-R3-11: with a local
//                 radio each band remembers its attenuator and preamp; the
//                 controller follows the transmit slice's band and mode.
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-23 - J.J. Boyd (KG4VCF). R-R3-44: a remote window opens this
//                 computer's VAX outputs and feeds them from the Core's
//                 receiver streams (RemoteVaxRouter); the VAX applet's TX
//                 row takes the transmit permission. AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-22 — J.J. Boyd (KG4VCF). Invoke the Aether-derived pan-stack
//                 shutdown before QWidget destroys its graphics backend.
//                 AI-assisted integration via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Signal-routing hub, double-height status-bar layout, and
//                 TitleBar feature-request dialog ported from AetherSDR
//                 (ten9876/AetherSDR, GPLv3) src/gui/MainWindow.{h,cpp} and
//                 src/gui/TitleBar.{h,cpp}. AetherSDR has no per-file
//                 headers; project-level citation per docs/attribution/
//                 HOW-TO-PORT.md rule 6.
//   2026-09-23 - R-R3-46: Hardware Config availability pushed to the
//                 `alexAntennas` object; antenna refusals shown as toasts. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-23 - R-R3-21: clearing spots moves to Ctrl+Shift+X
//                 (Disconnect keeps Ctrl+Shift+K); Band > HF uses the band buttons' path;
//                 Tools test entries only in developer builds; saved
//                 Multimeter and high-resolution filter settings and the
//                 DXCC country table load at startup. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-47 / R-R3-48: the one TCI switch (TciSwitch),
//                the local RF-Kit band follow, the remote RF-Kit applet's
//                Disconnect or Reconnect through the Core. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-47 / R-R3-22: the power-cap alert is the Core's
//                (StationAccessoryData, in process for a local window)
//                and shown from `accessoryData` in every window; a remote
//                window's antenna names follow the Core's. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-22 / R-R3-47: the Power Genius and RF-Kit applets
//                send a remote window's Disconnect or Reconnect to the
//                Core themselves; the handlers here act on this computer's
//                own connections in a local window only. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-43 / R-R3-44 / R-R3-21: Setup > Audio > VAX is told
//                live whether the Core's receiver streams are Opus, so it
//                can say what that costs digital modes. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-49 / R-R3-21: a test run never auto-opens the
//                blocking Linux audio first-run dialog
//                (firstRunPromptsBarredForTestRun). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-39 (station Task 32): the TX analyzer runs its WDSP
//                calls on the model's transmit lane. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-39 / R-IOS-03: the TX analyzer's rate and frame rate
//                come from TxAnalyzer::applyStationRates, shared with
//                nereusd. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-24: Part C fix wave: the optional device shortName in
//               auth.request, stored with the device. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25 - J.J. Boyd (KG4VCF). R-R3-49, Sub-epic C-1 (tx-followup-3):
//                 the DSP menu's NR list shows DFNR, MNR and BNR always,
//                 disabled with the plain reason while they cannot run.
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-25 - J.J. Boyd (KG4VCF). R-R3-49, Sub-epic C-1 (tx-followup-4):
//                 the DSP menu's NR list no longer offers BNR (operator:
//                 not offered for now); its entries come from
//                 nrMenuEntries(). AI-assisted via Anthropic Claude Code.
//   2026-09-25 - J.J. Boyd (KG4VCF). iPhone app plan, desktop remote
//                 transmit (R-IOS-13, R-R3-42): the remote window's
//                 transmit controls follow the Core's txPermitted with its
//                 reason; a refused press is a toast; the TCI server
//                 forwards a program's transmit to the Core. AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-25 - J.J. Boyd (KG4VCF). iPhone app plan Task 39 (D14,
//                 R-IOS-13): a remote window's transmit meters follow the
//                 Core's `txState`: forward and reflected power and SWR
//                 through the window's RadioStatus (the S-meter's TX needle,
//                 the TX applet's power gauge, the container meters), ALC
//                 and MIC through MeterPoller, the keyed state to the S-meter
//                 and the poller; the meters the Core does not send are
//                 shown disabled with the reason. AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-24 - R-R3-49 (parity Task 1): transmitSettingsPermitted(). The
//                TX applet's RF Power and TX filter, the RX applet's and
//                flag's Shift-click TX passband match and DSP > Options' TX
//                combos follow it; applyRemoteRoleGating runs again when
//                the Core's radio goes on or off the air. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-49 (parity Task 2): the TX applet's Tune Power, VOX
//                level and delay, MON, LEV, EQ and CFC, the Phone/CW
//                applet's mic level, PROC, AM carrier and DEXP, and the
//                container MON button follow transmitSettingsPermitted(2).
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 3): the TX, Phone/CW and RADE
//                applets' profile combos, RADE's Reset vocoder and Setup's
//                versioned transmit settings gates (Audio > TX Input's
//                microphone, Audio > TX Profile) follow
//                transmitSettingsPermitted(3) and (2). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 4): Tools > TX Equalizer opens in a
//                remote window; the TX EQ and CFC dialogs and Setup's
//                version 4 pages (DSP > CFC, AGC/ALC's TX Leveler and ALC)
//                follow transmitSettingsPermitted(4). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 5): Setup's version 5 pages
//                (Transmit > Power, DEXP/VOX, Test > Two-Tone IMD) follow
//                transmitSettingsPermitted(5). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 / R-R3-32 (parity Task 6): Setup > PA follows
//                transmitSettingsPermitted(6); the System tile's PA row and
//                the HW Volts, Amps and Temperature meters read
//                RadioModel::paReadings() (the Core's in a remote window);
//                the TX badge follows RadioModel::txInhibitedChanged.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-IOS-27, R-IOS-06: onAddTnfClicked calls
//                RadioModel::addTnfForSlice, the +TNF add the Core's
//                notch.addAtSlice shares. J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 7): PS-A on the TX applet and the
//                container follows pureSignalArmingPermitted(), which a
//                Core at transmitSettingsVersion 7 offers off the air; the
//                PureSignal menu entries say so. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 8): openSetup's page keys go through
//                SetupDialog::selectNavigationTarget (the Advanced and
//                Interlock entries open their CAT & Network > 4O3A tab);
//                the Tuner Genius applet's Copy diagnostics copies the
//                Core's connection in a remote window. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 9): the Power Genius applet's OPERATE
//                asks the Core in a remote window (never this computer's
//                idle connection), and its Copy diagnostics copies the
//                Core's connection. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 10): the RF-Kit applet's OPERATE and
//                ANT ask the Core in a remote window (never this
//                computer's idle connection); its Copy diagnostics copies
//                the Core's connection there, and a local window's copy
//                gains the operate, interface, reconnect, connected-since
//                and last-poll lines a remote one shows. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-25 - R-R3-49, R-R3-21, R-R3-44 (parity Task 11): the RX
//                applet's XIT row takes no transmit gate; the container
//                Antenna box's TX buttons write the transmit slice's
//                antenna in a remote window with no toast, as the VFO
//                flag's do; the VAX first-run check for new virtual cables
//                runs in a remote window as in a local one. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 / R-R3-46 (parity Task 12): the Alex facade learns
//                whether the Core takes the transmit antennas and relays
//                (radioHardwareVersion 6) and RX bypass on TX (5); Setup >
//                Antenna Control follows it. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (remote-window parity Task 13): the Setup dialog
//                 gets the transmitSettingsVersion 8 gate. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - Receiver and transmit gaps plan, Task 16: the container
//                buttons follow receive only (RadioModel::rxOnlyChanged).
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-R3-13 / R-R3-49 (remote-window parity Task 15): a remote
//                window's ADC and AGC meters follow the Core's
//                meterReadingsVersion. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave: I4 the TX applet says who holds
//               transmit; M8 a VOX arming the Core refuses is a toast.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave 2 (M8): VOX shows disabled with the
//               plain reason while this computer has no microphone line to
//               the Core; the Core's refusal stays the backstop. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26 - R-R3-49 / R-R3-21 (remote-window parity Task 16): DSP > NR
//                offers DFNR and MNR by the station's noise reduction (the
//                Core's in a remote window); a remote pan's minimum notch
//                width is its slice's (the Core's); the filter graphs draw
//                the Core's curve. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-26 - R-R3-01 (parity Task 17 follow-up): RadioModel is given
//                the FFT engine pool, so Rendering > Decimation reaches
//                every pan. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-26 - Remote-window parity Task 18: a pan's BAND flyout changes
//                that pan's slice (B3.1); an empty connected pan says so
//                (C8); every strip's Display flyout (Grid Lines included)
//                and Clarity Re-tune act on their own pan, Clarity tuning
//                the active pan from its own stream; a spot's left-click
//                sets the pan's slice mode as AetherSDR's does
//                (MainWindow_Wiring.cpp:4382-4437 [@1e0718ad]); Pan Layout
//                and +PAN follow RadioModel::maxSlices(). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  iPhone app plan Task 77 (R-IOS-02,
//                                    R-IOS-03, R-IOS-13): every transmit
//                                    control in a remote window follows the
//                                    holder (the transmitter's settings and
//                                    VOX disabled with the reason). AI-
//                                    assisted via Anthropic Claude Code.
//   2026-09-26 - Remote-window parity Task 19 (R-IOS-25): the Spot Hub's
//                starts and stops go through SpotSourceHost; a remote
//                window's cluster, RBN, POTA and PSK Reporter are the
//                Core's; the Spot Hub's Core settings are disabled with
//                Setup's words while there is no Core session. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 - Remote-window parity Task 21 (R-IOS-18, B6.2, B6.3): the
//                Radio menu, the station block and the title bar offer
//                Change radio, Edit radio and Forget radio for the Core's
//                radio (Setup > This Core); the title bar copies the Core's
//                radio's IP and MAC; an unmanaged remote window's Manage
//                Radios opens This Core. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-26 : D79 (R-IOS-11, R-R3-49): View > Band Plan's check
//                follows planChanged, so a remote window's check follows
//                the Core's plan. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-26 : iPhone app plan Task 78 (R-IOS-02, R-IOS-07, R-IOS-30):
//                a remote window shares the Core as a device: the Core's
//                questions and notices (MultiDeviceController), Take
//                transmit from the pan's TX pill and the TX applet, the
//                holder beside the TX badge, other devices' markers, the
//                flags' TX from the Core and the radio's freeze, the empty
//                band's offer of a take, session.leave on quit. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 : iPhone app plan Task 77 fix round 3 (R-IOS-02, R-IOS-03,
//                R-IOS-13): the Power Genius applet's OPERATE also waits
//                while a Tuner Genius cycle runs. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-26 - Remote-window parity Task 29 (A11, R-R3-49, R-R3-12): the
//                MOX lambda's rise and fall moved into MoxDisplayController,
//                which a remote window drives from the Core's txState; this
//                window's pan is the transmit analyzer feed's local viewer.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 - R-IOS-18 / R-IOS-27 / R-R3-08: the FFT pool's display
//                keys and their defaults come from core/ControlRanges.h,
//                the table the Core's catalogue reads. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-27 - Parity Task 31 (A11, R-R3-49): display duplex (DUP), the
//                window's DisplayDuplex setting (off by default as Thetis),
//                View > Display duplex (DUP) and the container DUP button,
//                applied to the MOX display controller, this window's noise
//                blanking rule and a remote window's subscriptions; the TX
//                Display Cal Offset reaches every pan. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-27 - Parity Task 32 (R-IOS-13, R-R3-49): a remote window sends
//                its MON output choice (audio/TxMonitor/Output) to the Core
//                at start and on every change; its MON output pair is shown
//                disabled with the reason on a Core that does not send the
//                transmit monitor. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-27 - iPhone plan Task 22 / parity Task 20 (R-IOS-26, B7.3): the
//                Spot Hub's FreeDV Start / Stop go through the spot source
//                host (the Core's in a remote window); the FreeDV Reporter
//                dialog's QSY and message go to the Core in a remote window,
//                disabled with the reason on a Core that does not run FreeDV
//                Reporter. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-27 - Parity Task 33 (R-R3-49, R-R3-32, R-IOS-13): the CFC
//                dialog's bar chart from the Core's txCfcCompression stream
//                in a remote window (disabled with the reason on a Core that
//                does not send it), the Core's COMP reading for the
//                compression meters; the remote Max Bin source moved to
//                MeterPoller::panMaxBinSource, unchanged. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Parity Task 25: a container's filter right-click (a
//                filter button or the VFO display) edits or resets that
//                preset, as the VFO flag's and RX applet's do; a band-stack
//                right-click says band stacking is not ready yet. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Parity ruling C9: a remote window's System tile CPU row
//                shows this computer's CPU and the Core's, cycling every
//                3 s, pinned from its right-click, held in the warning
//                colour on a reading above 80%. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Parity ruling C13: a remote window's Performance
//                Overlay adds the Core's drops (wireSpectrumForPan).
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - R-R3-49 / R-R3-46: Setup > Transmit > Power's Disable HF PA
//                applied (Thetis DisablePA and hf_tr_relay,
//                transmitSettingsVersion 11). J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-28 - R-R3-46 / R-R3-11: each pan's spectrum takes the
//                 receive offset of the ADC its stream is on
//                 (RadioModel::rxMeterOffsetDbForStream), not slice A's.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 2: the TX badge tooltip and toast name the
//                TX inhibit's reason (the HL2 I/O board's fault code) and
//                the transmit buttons follow the inhibit. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Slice control plan Task 2: the hosting desktop's own
//               slices are the ones SliceAccessPolicy lets the station
//               device change. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-29 - Slice control plan Task 7: no "TX > Slice" notice when
//               the last slice closed and no slice transmits. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Slice control plan Task 8b: a window run with a profile
//               names the profile when it signs in to a Core. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Slice control plan Task 13: the bottom RX area's picker
//               opens the all-slice chooser (SliceChooser): listen in, take
//               control, release, stop listening, select RX and a new slice,
//               each waiting for the Core's answer. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Slice control plan Task 14b (ruling U5): a listened
//               flag's "Your volume" and Mute set this device's own
//               listening level (SliceAccessController::setListenLevel when
//               hosting, slice.setListenLevel from a remote window); the
//               slice's AF and mute are never written from it. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Slice control plan Task 10: while this desktop hosts, its
//               select, new slice, close, listen, stop listening, take
//               control, release and listening level run as the station
//               device (HostingSliceActions), with a remote device's checks,
//               questions and notices. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-29 - Slice control plan Task 11: the TX applet follows the
//               transmit-bound slice (its band, per-band power and every
//               transmit control), offers a letter per slice this window
//               controls (tx.setTxSlice from a remote window), and a hosting
//               window's empty pans get station-device slices. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Slice control plan Task 11 fix: the flag's TX button moves
//               transmit the way the TX applet's letters do (tx.setTxSlice
//               from a remote window) with the same reasons. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Slice control plan Task 15 (rulings U5, U6, U7): the RX
//               applet's tabs list the slices this window controls and the
//               shown slices it listens to, each saying who controls it; a
//               listened tab holds the applet's shared controls with the
//               controller named, offers Take control, and selecting it
//               moves this window's RX (bottom bar, flag focus) without
//               moving the active or transmit slice. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Slice control plan Task 15 fix round 1: the container
//               slice buttons refuse a slice this window listens to with
//               the RX applet's reason (sliceChangeRefusal, both window
//               kinds); the hosting select fallback reaches a listened slice
//               as its RX (setActiveRxFor). J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-29 - Slice control plan Task 16 (rulings U1, U2, U7): a layout
//               change moves only the slices this window controls and stops
//               listening to a listened slice it no longer shows, with a
//               notice; listening to or taking an unseen slice shows it in
//               the main window (an empty pan, else one pan more, else the
//               operator picks); selecting a slice brings its pan forward,
//               a floating one included. The bottom RX area's picker
//               connect moved to buildStatusBar, where the dashboard
//               exists. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-29 - Slice control plan Task 17: a slice's pan key naming a
//               pan this window lacks gets that pan on panKeyChanged, as on
//               sliceAdded, so a remote window's listened slice no longer
//               stays on the active pan it was docked on before its pan key
//               arrived. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-29 - Slice control fix wave (whole-branch review, Minor 1):
//               the unanswered slice request toast drops "yet". J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Take-over parity: the hosting desktop's controlTaken
//               card has Take it back, as a remote window's does. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Take-over fix wave (M-3): the hosting desktop's
//               controlTaken card stays when Take it back may be tried
//               again. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
//   2026-09-30 - Take-over re-review (N-3): a closed hosting card
//               forgets its Take it back record. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-30 - TX rulings (item 3): refreshOverlayAttAccess, each pan's
//               ATT flyout held on a slice this window only listens to.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Desktop listening: a layout change that retires the pan
//               of a slice this window listens to places it on a pan that
//               remains and keeps listening; stopListeningOffWindow and its
//               toast are gone. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-30 - Desktop listening review: a remote window's marker for a
//               slice the station device holds names the hosting desktop
//               (foreignMarkers reads the access entries), refreshed when
//               access or the connected devices change. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Desktop listening placement: a listened slice placed on
//               a remaining pan no longer takes that pan's view. It is
//               placed before the controlled slices rehome, and
//               rebuildFftRouting does not push its stream window onto a
//               pan that already shows something. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-30 - TX badge take (JJ's ruling): a flag's TX badge on a slice
//               this window cannot make the TX slice at once takes the
//               slice (slice.takeControl), then transmit (tx.take, asked as
//               the Take transmit control asks), then makes it the TX
//               slice. Nothing keys. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-30 - TX badge take fix round 1: only the badge's own take
//               (its take id, its tx.take command id) makes the slice the TX
//               slice; the take ends with the link, hosting, a refusal or
//               another request; a slice answer is matched to the flag that
//               asked; a remote badge offers a take only when the Core's
//               refusal is the holder's. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-30 - Desktop listening fix 3: a listened slice a layout change
//               places on a remaining pan is a marker only there
//               (m_markerOnlyPlacement): its stream is not subscribed to
//               that pan, and its hooks draw its flag and its own edge
//               marker, never the pan's VFO, view or DDC centre. On a pan
//               not its own, a slice's demodulator shift comes from its own
//               stream's centre. A reveal is unchanged. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - iPhone app plan Task 25 (R-IOS-18): a remote window's VAX
//                applet gains the "Station computer" section, the Core
//                computer's VAX through the Core's `vax` object, its TX row
//                on the transmit permission and its meters subscribed only
//                while the applet shows it. J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-30 - TX safety: the function buttons follow
//                RadioModel::radioLinkDownChanged, so MOX, TUN and 2TONE
//                are disabled with their reason while the link to the radio
//                is lost. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-30: Fix wave GUI-I3 and GUI-M2: a remote window's PureSignal
//               applet is disabled with the reason below PureSignal 3,
//               not hidden (applyRemotePureSignalAppletGate); the Core
//               radio items show disabled with a reason in a window that
//               runs its own radio. Fix round 1: that reason names the
//               menu item this window has. J.J. Boyd (KG4VCF), AI-
//               assisted via Anthropic Claude Code.
//   2026-09-30 - TX safety: Radio > Disconnect stays enabled while the
//                lost-link lock holds, so the operator can lift it after an
//                automatic recovery stops. J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-30 - The container Power button is removed (maintainer
//                decision): no Power hooks. Radio > Disconnect's enable rule
//                is one helper, localDisconnectAvailable. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - VFO flag crash lane: a flag's edges into its slice use the
//                slice as context; Slice A's flag goes when its slice does,
//                with m_vfoWidgetsBySlice its one owner; a Slice A made
//                again is wired by id on its own pan, window-wide wiring
//                once. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                Code.
//   2026-10-01 - VFO flag crash lane fix round: Slice A's flag follows its
//                pan key and rehost like every other flag; its board-caps
//                and callsign wiring moved into createSliceFlag; Slice A's
//                first wiring names its own pan; slice add and remove are
//                logged. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

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

mw0lge@grange-lane.co.uk
*/
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

/*  wdsp.cs

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013-2017 Warren Pratt, NR0V

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

//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
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
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
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

//=================================================================
// radio.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Copyright (C) 2019-2026  Richard Samphire
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
//=================================================================
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

#include "MainWindow.h"
#include "gui/multidevice/DeviceWords.h"
#include "gui/multidevice/MultiDeviceController.h"
#include "gui/multidevice/NoticeCard.h"
#include "gui/HostingSliceActions.h"
#include "gui/LevelCalGridFollowGuard.h"
#include "core/session/RemoteDevicesState.h"
#include "core/session/PathRacer.h"
#include "ConnectionPanel.h"
#include "NetworkDiagnosticsDialog.h"
#include "OperatorReasonText.h"
#include "RemoteDiagnosticsDialog.h"
#include "RemoteTelemetryController.h"
#include "SupportDialog.h"
#include "AboutDialog.h"
#include "SpectrumWidget.h"
#include "SliceFlagPresentationBinding.h"
// Phase 3F Sub-Epic D Task 10: +PAN bottom-bar dropdown reads slice
// state + drives PanadapterStack layout/float actions.
#include "PanadapterStack.h"
#include "PanadapterApplet.h"
#include "PanLayoutDialog.h"
#include "core/safety/TxRefusal.h"
#include "core/FFTRouter.h"
#include "StyleConstants.h"
#include "models/RadioModel.h"
#include "models/StationTciModel.h"
#include "core/session/IStationLink.h"
#include "core/session/RemoteTransmitClient.h"
#include "models/AccessoryDataModel.h"
#include "models/SliceModel.h"
#include "widgets/VfoWidget.h"
#include "widgets/RxDashboard.h"
#include "SliceChooser.h"
#include "core/session/SliceAccessController.h"
#include "core/session/SliceAccessMirror.h"
#include "widgets/AntennaSwitchToast.h"
#include "widgets/StatusToast.h"
#include "widgets/FilterPolicyDialog.h"
#include "widgets/TxBoundConfirmDialog.h"
#include "core/RxChannel.h"
#include "core/TxChannel.h"  // H.2: setTxChannel wiring
#include "core/ReceiverManager.h"
#include "core/AppSettings.h"
#include "core/ControlRanges.h"
#include "core/BuildIdentity.h"
#include "core/PaTempUnit.h"
#include "core/RadioStatus.h"
#include "core/RadioDiscovery.h"
#include "core/WdspEngine.h"
#include "core/FFTEngine.h"
#include "core/spectrum/FftEnginePool.h"
#include "core/spectrum/FftTopology.h"
#include "core/TxAnalyzer.h"
#include "core/TxDisplayFeed.h"
#include "gui/widgets/GradientPickerWidget.h"
#include "core/NbFamily.h"
#include "core/ClarityController.h"
#include "core/StepAttenuatorController.h"
#include "core/StepAttenuatorFacade.h"
#include "core/accessories/AlexAntennaFacade.h"

#include <array>
#include "core/MoxController.h"  // 3M-1a G.1: F.2 connect (hardwareFlipped → onMoxHardwareFlipped)
#include "core/NoiseFloorTracker.h"
#include "core/BoardCapabilities.h"
#include "core/TxSliceArbiter.h"  // Phase 3F Sub-Epic C Task 9: TX-handoff routing
#include "core/SliceOwnership.h"
#include "core/session/SliceAccessPolicy.h"
#include "core/safety/TransmitHolder.h"
#include "core/session/StationServer.h"
#include "core/session/ConnectedDevicesFacade.h"
#include "core/session/DeviceSessionRegistry.h"
#include "gui/multidevice/TakeTransmitDialog.h"
#include "models/PanadapterModel.h"
#include "models/Band.h"
#include "models/TransmitModel.h"
#include "core/LogCategories.h"
#include "containers/ContainerManager.h"
#include "containers/ContainerContentRegistry.h"
#include "containers/ContainerContentHost.h"
#include "containers/ContainerWorkspaceStore.h"
#include "containers/ContainerSourceAdapter.h"
#include "meters/presets/CompositePresetItem.h"
#include "meters/presets/BarPresetItem.h"
#include <QJsonArray>
#include "containers/ContainerWidget.h"
#include "containers/ContainerButtonDispatcher.h"
#include "containers/ContainerSettingsDialog.h"
#include "meters/MeterWidget.h"
#include "meters/FilterDisplayItem.h"
#include "core/spectrum/SpectrumReducer.h"
#include "meters/MeterItem.h"
#include "meters/ItemGroup.h"
#include "meters/MeterPoller.h"
#include "meters/VfoDisplayItem.h"  // 3M-1c L.3 — TX badge routing
#include "meters/ModeButtonItem.h"      // R-R3-21 container controls
#include "meters/FilterButtonItem.h"
#include "meters/AntennaButtonItem.h"
#include "meters/TuneStepButtonItem.h"
#include "meters/BandButtonItem.h"
#include "meters/OtherButtonItem.h"
#include "models/FilterPresetStore.h"
#include "gui/styles/PopupMenuStyle.h"
#include "gui/widgets/FilterPresetEditDialog.h"
#include "core/SkuUiProfile.h"
// Remote Daemon R2 Task 12: source-selector wiring below (setSourceSelector).
#include "core/meters/SliceMeterPump.h"
#include "applets/AppletPanelWidget.h"
#include "SMeterWidget.h"            // Task 41 (Phase 3P-II): SMeterWidget header wiring
#include "applets/AmpApplet.h"
#include "applets/Rf2ksApplet.h"
#include "applets/AppletVisibilityController.h"
#include "applets/RxApplet.h"
#include "core/PgxlConnection.h"
#include "core/TgxlConnection.h"
#include "core/Rf2ksConnection.h"
#include "core/SmartSdrApiListener.h"
#include "applets/TxApplet.h"
#include "applets/TxEqDialog.h"
// Phase 3J-2 H1: Tools menu modeless singletons (Spot Hub + FreeDV Reporter).
#include "SpotHubDialog.h"
#include "core/SpotSourceHost.h"
#include "core/station/StationRadios.h"
#include "gui/setup/ThisCorePage.h"
#include "FreeDVReporterDialog.h"
// Phase 3F Sub-Epic G T4: bench-minimum Diversity dialog (Tools menu).
#include "DiversityDialog.h"
#include "models/SpotModel.h"
#include "models/SpotModeResolver.h"
#include "models/NotchModel.h"
#include "models/FreeDVStationModel.h"
#include "core/DxccColorProvider.h"
#include "core/FreeDVReporterClient.h"
#include "core/DxClusterClient.h"
#include "core/WsjtxClient.h"
#include "core/SpotCollectorClient.h"
#include "core/PotaClient.h"
#include "core/PskReporterClient.h"
#include "PsForm.h"
#include "AmpViewWindow.h"
#include "PsaIndicatorWidget.h"
#include "core/PureSignal.h"
#include "core/TwoToneController.h"
#include "applets/PhoneCwApplet.h"
#include "applets/RadeApplet.h"
#include "applets/DisplayApplet.h"
#include "applets/EqApplet.h"
#include "applets/VaxApplet.h"
#include "applets/DigitalApplet.h"
#include "applets/PureSignalApplet.h"
#include "applets/ModMonitorApplet.h"
#include "applets/DiversityApplet.h"
#include "applets/CwxApplet.h"
#include "applets/DvkApplet.h"
#include "applets/CatApplet.h"
#include "applets/TunerApplet.h"
// Phase 23: TCI server + applets (guarded so non-WebSocket builds still compile)
#ifdef HAVE_WEBSOCKETS
#  include "applets/TciApplet.h"
#  include "applets/ClientChainApplet.h"
#  include "core/TciServer.h"
#  include "core/TciSwitch.h"
#  include "gui/RemoteTransmitForwarder.h"
#  include "core/RfKitBandFollow.h"
#  include "setup/TciLogWindow.h"  // Phase 3J-1 closeout Item 2 (2026-05-12)
#  include <QWebSocket>
#endif
#include "SpectrumOverlayPanel.h"
#include "SetupDialog.h"
#include "UnbuiltFeatures.h"
// Remote-daemon R2 Task 20: the wss client and the settings backend it
// writes through. Both are used only on the m_station.isRemote() path.
#include "core/security/ClientDeviceIdentity.h"
#include "core/session/StationClient.h"
#include "core/session/TransmitStateFacade.h"
#include "models/RfKitModel.h"
#include "RemoteConnectionController.h"
#include "gui/RemoteMediaController.h"
#include "gui/MoxDisplayController.h"
#include "gui/TxDisplaySource.h"
#include "gui/RemoteVaxRouter.h"
#include "gui/ReceiverStopNotices.h"
#include "core/session/media/IReceiverPcmSink.h"
#include "core/settings/SettingsProxy.h"
#include "setup/DspSetupPages.h"   // NrAnfSetupPage::selectSubtab
#include "setup/DspOptionsPage.h"  // applyPersistedHighResFilter (R-R3-21)
#include "setup/MultimeterPage.h"  // applyPersistedSettings (R-R3-21)
#include "setup/HardwarePage.h"    // showAntennaTab (R-R3-21)
#include "gui/DspAssetDialog.h"
#include "models/PureSignalSettings.h"
#include "core/session/PureSignalSessionFacade.h"
#include "TitleBar.h"
#include "VaxFirstRunDialog.h"
#if defined(Q_OS_LINUX)
#  include "VaxLinuxFirstRunDialog.h"
#endif
#include "widgets/MasterOutputWidget.h"
#include "widgets/StationBlock.h"
#include "widgets/StatusBadge.h"
#include "widgets/AdcOverloadBadge.h"
#include "widgets/OverflowChip.h"
#include "widgets/SystemTile.h"
#include "gui/chrome/ChromeBarController.h"
#include "gui/chrome/ChromeBarItems.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "core/audio/VirtualCableDetector.h"
#include "core/audio/RealtimeAudioPriority.h"

#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QSlider>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QEvent>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QToolTip>
#include <QMenuBar>
#include <QMenu>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QAction>
#include <QSignalBlocker>
#include <QActionGroup>
#include <QStatusBar>
#include <QLabel>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QDateTime>
#include <QPainter>
#include <QPixmap>
#include <QProgressDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>
#include <QThread>
#include <QFile>          // /proc/stat reader for Linux system-CPU path
#include <QPushButton>
#include <QCursor>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVersionNumber>
#include <QPointer>
#include <QElapsedTimer>
#include <QShortcut>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>

// Cross-platform CPU usage readers — see readProcessCpuPercent and
// readSystemCpuPercent below. POSIX side (macOS / Linux) shares
// getrusage for process CPU; macOS adds host_processor_info for system
// CPU; Linux reads /proc/stat; Windows uses GetProcessTimes /
// GetSystemTimes. Each branch is gated by Q_OS_*.
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
#include <sys/resource.h>
#endif
#ifdef Q_OS_MAC
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/processor_info.h>
#endif
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace NereusSDR {

namespace {
// First-run/rescan wants the "relevant" virtual cables for the current
// platform — 3rd-party cables on Windows (BYO), our own NereusSdrVax
// entries on Mac/Linux (native HAL plugin / pipe-source). Centralising
// the platform split here keeps checkVaxFirstRun() focused on
// scenario-selection + dialog wiring.
QVector<DetectedCable> detectedForFirstRun()
{
#if defined(Q_OS_WIN)
    return VirtualCableDetector::scanThirdPartyOnly();
#else
    QVector<DetectedCable> out;
    for (const auto& c : VirtualCableDetector::scan()) {
        if (c.product == VirtualCableProduct::NereusSdrVax) {
            out.push_back(c);
        }
    }
    return out;
#endif
}

// Local tooltips of the Tools menu's two developer test entries. Defined
// once because applyRemoteRoleGating() swaps them for the remote transmit
// reason and has to be able to put them back.
// R-R3-17 / R-R3-21: user words. The entries open AntennaSwitchToast and
// TxBoundConfirmDialog (Phase 3F closeout); neither fires on its own until
// the antenna conflict-detection state machine ships.
QString testAntennaToastToolTip()
{
    return QStringLiteral("Show the antenna switch notice to see how it looks. "
                          "No antenna changes, and antennas do not switch on "
                          "their own.");
}

QString testTxBoundReRouteToolTip()
{
    return QStringLiteral("Show the question asked before the transmit antenna "
                          "moves, to see how it looks. No antenna changes, and "
                          "adding a slice does not ask it.");
}
} // namespace

struct MainWindow::MiniProducer {
    QPointer<SliceModel> slice;
    QList<QPointer<FilterDisplayItem>> items;
    SpectrumReducer trace;
    SpectrumReducer waterfall;
    ReducerConfig geometry;
    ReducerConfig waterfallGeometry;
    int streamIndex{-1};
    quint64 streamEpoch{0};
    int binCount{0};
    QElapsedTimer cadence;
    RadioModel::FilterResponse filterResponse;
    int filterLow{0};
    int filterHigh{0};
    int sampleRate{0};
    DSPMode dspMode{};
    bool filterCached{false};
    QString gradientKey;
    QVector<QColor> customGradient;
    bool gradientTx{false};
    QVector<float> txTrace;
    QVector<float> txWaterfall;
};

MainWindow::MainWindow(QWidget* parent)
    : MainWindow(RemoteStationOptions{}, parent)
{
    // Remote-daemon R2 Task 20: delegates with an empty station, which
    // resolves to Role::Local. Every existing construction site keeps its
    // current behaviour with no change on its end -- the same shape Task 4
    // used for RadioModel's own two constructors.
}

MainWindow::MainWindow(const RemoteStationOptions& station, QWidget* parent,
                       ConnectionStartup startup)
    : QMainWindow(parent)
    , m_station(station)
    , m_radioModel(new RadioModel(m_station.isRemote() ? RadioModel::Role::Remote
                                                       : RadioModel::Role::Local,
                                  this))
{
    connect(m_radioModel, &RadioModel::miniFilterResponseChanged, this,
            [this](int sliceId) {
        const auto found = m_miniProducers.find(sliceId);
        if (found == m_miniProducers.end()) { return; }
        const auto response = m_radioModel->miniFilterResponse(sliceId);
        SliceModel* slice = found->second->slice;
        if (!slice) { return; }
        for (const auto& item : found->second->items) {
            if (item && item->frameAvailable()) {
                item->setRfFilterResponse(response.magnitudesDb,
                    response.startHz, response.stepHz, slice->frequency());
            }
        }
    });
    // A level calibration turns the grid's noise-floor follow off while it
    // runs and puts it back after, for a run started here or one this
    // window's Core runs.
    {
        auto* gridGuard = new LevelCalGridFollowGuard(m_radioModel, this);
        gridGuard->setAccess(
            [this]() {
                SpectrumWidget* w = activeSpectrumWidget();
                return w != nullptr && w->adjustGridMinToNoiseFloor();
            },
            [this](bool on) {
                if (SpectrumWidget* w = activeSpectrumWidget()) {
                    w->setAdjustGridMinToNoiseFloor(on);
                }
            },
            // Level Cal: a quit or a crash mid-run saves the user's value.
            [](std::optional<bool> saved) { SpectrumWidget::setGridFollowSaveHold(saved); });
    }
    // ── Phase 23 (bench fix 2026-05-10): TCI Server BEFORE buildUI ───────────
    // TciApplet + ClientChainApplet are constructed by populateDefaultMeter()
    // (called from buildUI), gated on `if (m_tciServer)`. The original Phase
    // 23 wiring constructed TciServer AFTER buildUI, so the gate evaluated
    // false and the applets never appeared in the right-side panel.
    // RadioModel is already constructed via the member initializer list, so
    // it's safe to instantiate TciServer here. Auto-start is deferred to
    // after buildUI so the indicator + applets are ready to receive signals.
#ifdef HAVE_WEBSOCKETS
    {
        m_tciServer = new TciServer(m_radioModel, this);
        // R-R3-48: the app's one TCI switch drives this server and, on a
        // Core that runs a station server, the Core's too.
        m_tciSwitch = new TciSwitch(m_tciServer, m_radioModel, this);
        if (m_radioModel && m_radioModel->stationTciModel()) {
            connect(m_radioModel->stationTciModel(), &StationTciModel::stateChanged,
                    this, &MainWindow::updateTciIndicator);
            connect(m_radioModel->stationTciModel(), &StationTciModel::clientsChanged,
                    this, &MainWindow::updateTciIndicator);
            connect(m_radioModel, &RadioModel::stationLinkStateChanged,
                    this, &MainWindow::updateTciIndicator);
        }
        connect(m_tciSwitch, &TciSwitch::stationRequestFailed, this,
                [this](const QString& reason) {
            statusBar()->showMessage(OperatorReasonText::forDisplay(reason), 5000);
        });
        // R-R3-48: a local window's RF-Kit follows the band as an app of
        // this server; the band-follow line says whether it does.
        if (m_radioModel && m_radioModel->role() == RadioModel::Role::Local) {
            m_rfKitBandFollow = new RfKitBandFollow(m_radioModel->rfKitModel(), this);
            m_rfKitBandFollow->setServer(m_tciServer);
        }
        connect(m_tciServer, &TciServer::serverStarted,
                this, [this](quint16) { m_tciServerRunning = true;  updateTciIndicator(); });
        connect(m_tciServer, &TciServer::serverStopped,
                this, [this]()        { m_tciServerRunning = false; updateTciIndicator(); });
        connect(m_tciServer, &TciServer::clientConnected,
                this, [this](QWebSocket*) { ++m_tciClientCount; updateTciIndicator(); });
        connect(m_tciServer, &TciServer::clientDisconnected,
                this, [this](QWebSocket*) {
                    if (m_tciClientCount > 0) { --m_tciClientCount; }
                    updateTciIndicator();
                });
        // R-R3-42: what TCI refused (transmit or raw I/Q in a remote window)
        // or why a receiver's audio stopped, in the operator's words. Never
        // on the TCI wire; the applet and the TCI log window show it too.
        connect(m_tciServer, &TciServer::operatorNotice, this,
                [this](const QString& peer, const QString& reason, bool raiseToast) {
            qCInfo(lcConnection) << "TCI notice" << peer << ":" << reason;
            // A receiver's audio stopping is toasted from
            // receiverStopNotice below, which names the receiver.
            if (!raiseToast || ReceiverStopNotices::isReceiverStop(reason)) { return; }
            // iPhone app plan Task 78 (ruling 8.3; the several-devices
            // design, section 12 item 2): a program's key refused because
            // another device holds transmit says so in this window's terms.
            if (m_stationClient && m_stationClient->transmitHeldElsewhere()) {
                showToast(tr("A program using TCI can transmit only while this window has "
                             "transmit. %1")
                              .arg(OperatorReasonText::forDisplay(reason)),
                          ToastSeverity::Warning, 5000);
                return;
            }
            showToast(tr("TCI: %1").arg(OperatorReasonText::forDisplay(reason)),
                      ToastSeverity::Warning, 5000);
        });
        // R-R3-42 fix wave: a receiver's audio stopping is one notice for
        // this computer, however many apps (TCI, VAX channels) hear that
        // receiver; the same reason for another receiver is another notice.
        connect(m_tciServer, &TciServer::receiverStopNotice, this,
                [this](int rx, const QString& reason, bool raiseToast) {
            if (!raiseToast) { return; }
            const QString text = m_receiverStopNotices.toastFor(
                reason, rx, QDateTime::currentMSecsSinceEpoch());
            if (!text.isEmpty()) {
                showToast(text, ToastSeverity::Warning, 5000);
            }
        });
        connect(m_tciServer, &TciServer::txAudioActiveClientChanged,
                this, [this](QWebSocket* owner) {
                    m_tciHasTxClient = (owner != nullptr);
                    updateTciIndicator();
                    // Phase 3J-1 bench fix (2026-05-10): gate the TxWorkerThread
                    // mic-source pump while TCI is providing audio so it does
                    // not race feedTxAudioFromTci's dispatch.  See
                    // TxChannel::m_tciAudioActive doc-comment for the full
                    // narrative on why the two-source race corrupts on-air
                    // audio at the I/Q ring buffer.
                    if (auto* wdsp = m_radioModel ? m_radioModel->wdspEngine() : nullptr) {
                        if (auto* txCh = wdsp->txChannel(WdspEngine::kTxChannelId)) {
                            txCh->setTciAudioActive(owner != nullptr);
                        }
                    }
                });
    }
#endif

    buildUI();
    buildMenuBar();

    // Phase 3O Sub-Phase 10 Task 10c — host the menu bar + master-output
    // controls in a custom TitleBar strip. Must run AFTER buildMenuBar()
    // so the menu is fully populated with actions before we re-parent it.
    //
    // setMenuWidget() hands ownership to QMainWindow and installs the
    // strip at the top of the window. On macOS this also disables Qt's
    // promotion of the menu bar to the native global bar — menus render
    // in-window alongside the master-output controls (explicit design
    // choice, user-approved option D for Sub-Phase 10).
    // R-R3-23: the master output (volume, mute, output device) is this
    // computer's, and drives remote playback in a remote window, so it
    // reaches the engine through localAudioDevices(), not the audited
    // local-DSP accessor.
    m_titleBar = new TitleBar(m_radioModel->localAudioDevices(), this);
    m_titleBar->setMenuBar(menuBar());
    setMenuWidget(m_titleBar);

    // Wire the MasterOutputWidget device picker → AudioEngine so picking
    // an output device rebuilds the speakers bus. Task 10b exposes only
    // the deviceName; we load the rest of the persisted speakers config
    // (sampleRate / channels / bufferSamples / exclusiveMode / etc.) so
    // the user's tuned values are preserved across a device change.  A
    // bare default-constructed AudioDeviceConfig with only deviceName
    // set would otherwise clobber persisted fields, defeating the
    // Setup → Devices page entirely.
    connect(m_titleBar->masterOutput(), &MasterOutputWidget::outputDeviceChanged,
            this, [this](const QString& name) {
        AudioDeviceConfig cfg = AudioDeviceConfig::loadFromSettings(
            QStringLiteral("audio/Speakers"));
        cfg.deviceName = name;
        if (auto* engine = m_radioModel->localAudioDevices()) {
            engine->setSpeakersConfig(cfg);
        }
    });

    // Phase 3O Sub-Phase 10 Task 10d — the 💡 feature-request button
    // now lives inside TitleBar (consolidated from the old featureBar
    // QToolBar). Wire its click signal to the existing slot.
    connect(m_titleBar, &TitleBar::featureRequestClicked,
            this, &MainWindow::showFeatureRequestDialog);

    // ── Phase 3Q Sub-PR-4 D.2: ConnectionSegment wiring ────────────────────
    {
        auto* seg = m_titleBar->connectionSegment();

        // 1. State dot + pulse: driven by connectionStateChanged.
        if (m_radioModel->ownsLocalDsp()) {
            connect(m_radioModel, &RadioModel::connectionStateChanged,
                    seg, &ConnectionSegment::setState);
        }

        // 2. frameTick: forwarded from RadioModel so we never need to
        //    re-wire when m_connection is recreated.
        connect(m_radioModel, &RadioModel::frameReceived,
                seg, &ConnectionSegment::frameTick);

        // 3. 1 Hz rate refresh — polls connection().{tx,rx}ByteRate(1000).
        auto* rateTimer = new QTimer(this);
        rateTimer->setInterval(1000);
        connect(rateTimer, &QTimer::timeout, this, [this, seg]() {
            if (auto* conn = m_radioModel->connection()) {
                // setRates(rxMbps, txMbps) — first arg names the "radio→client"
                // direction (m_rxMbps), second names "client→radio" (m_txMbps).
                // Earlier revisions passed these reversed, which made the ▲/▼
                // glyphs read in radio perspective rather than the client's.
                // Spec §Affordances reads the segment from the operator's
                // (client's) point of view: ▲ = NereusSDR uploading to radio
                // (commands), ▼ = radio downloading to NereusSDR (I/Q).
                seg->setRates(conn->rxByteRate(1000), conn->txByteRate(1000));
            }
        });
        rateTimer->start();

        // 4. RTT — wire from RadioConnection::pingRttMeasured.
        //    Re-wired on every Connecting/Probing transition so the segment
        //    always follows the live connection object (RadioModel recreates
        //    RadioConnection on each connect cycle).
        auto wireRtt = [this, seg]() {
            if (auto* conn = m_radioModel->connection()) {
                connect(conn, &RadioConnection::pingRttMeasured,
                        seg, &ConnectionSegment::setRttMs,
                        Qt::UniqueConnection);
            }
        };
        wireRtt();
        connect(m_radioModel, &RadioModel::connectionStateChanged, this,
                [wireRtt](ConnectionState s) {
                    if (s == ConnectionState::Connecting ||
                        s == ConnectionState::Probing) {
                        wireRtt();
                    }
                });

        // 5. Audio flow-state → ♪ pip color.
        if (auto* engine = m_radioModel->audioEngine()) {
            connect(engine, &AudioEngine::flowStateChanged,
                    seg, &ConnectionSegment::setAudioFlowState);
        }

        // 6. Click affordances.
        // The segment's mousePressEvent (TitleBar.cpp:382-387) routes
        // anywhere-click in the disconnected state to rttClicked so a
        // single signal covers both "click for routes" (when
        // connected) and "click to connect" (when disconnected). Branch
        // here on the live connection state to honor the "Click to
        // connect" affordance the segment paints — without this branch,
        // a disconnected click trapped the user in NetworkDiagnostics
        // instead of opening the connection panel (Codex P2 review
        // against PR #158, MainWindow.cpp:482).
        connect(seg, &ConnectionSegment::rttClicked, this, [this]() {
            if (!m_radioModel->ownsLocalDsp() && m_stationClient
                && m_stationClient->isHandshakeComplete()) {
                m_titleBar->connectionSegment()->showRoutePopup();
                return;
            }
            if (m_connectionPickerManaged) {
                connectionRequestedByOperator();
                return;
            }
            if (!m_radioModel->ownsLocalDsp()) {
                connectionRequestedByOperator();
                return;
            }
            const auto state = m_radioModel->connectionState();
            if (state == ConnectionState::Disconnected
                || state == ConnectionState::LinkLost) {
                showConnectionPanel();
                return;
            }
            // R2 Task 20: was an inline NetworkDiagnosticsDialog construction
            // here and at two other sites. Routed through the named slot so
            // the remote gate has one home instead of three.
            openNetworkDiagnostics();
        });
        connect(seg, &ConnectionSegment::audioPipClicked, this, [this]() {
            // Audio pip click also opens diagnostics — audio section
            // is the most relevant panel for pip trouble-shooting.
            openNetworkDiagnostics();
        });
        connect(seg, &ConnectionSegment::diagnosticsRequested,
                this, &MainWindow::openNetworkDiagnostics);
        connect(seg, &ConnectionSegment::pathsRefreshRequested,
                this, &MainWindow::refreshRemoteConnectionUi);
        connect(seg, &ConnectionSegment::contextMenuRequested,
                this, &MainWindow::showSegmentContextMenu);

        // 7. D.3: Hover tooltip — event filter delivers QHelpEvent.
        seg->setToolTip(QString());   // suppress Qt's own tooltip; we intercept
        seg->setAttribute(Qt::WA_AlwaysShowToolTips, false);
        seg->installEventFilter(this);

        // Seed with current state (Disconnected at launch).
        seg->setState(m_radioModel->connectionState());
    }

    buildStatusBar();
    applyDarkTheme();

    // Wire connection state changes to status bar
    connect(m_radioModel, &RadioModel::connectionStateChanged,
            this, &MainWindow::onConnectionStateChanged);
    // TX safety fix round 3 (2026-09-30): the lost-link lock lifts on the
    // operator's disconnect after the model has already reported
    // Disconnected, so Radio > Disconnect follows the lock itself too.
    connect(m_radioModel, &RadioModel::radioLinkDownChanged, this, [this]() {
        if (m_actDisconnect == nullptr || !m_radioModel->ownsLocalDsp()) {
            return;
        }
        m_actDisconnect->setEnabled(localDisconnectAvailable());
        refreshContainerControls();
    });

    // Issue #118 — show a transient status-bar message when a band-button
    // click short-circuits (locked slice, XVTR without transverter config).
    // Prevents silent failure — the user sees why nothing happened.
    connect(m_radioModel, &RadioModel::bandClickIgnored,
            this, [this](Band /*band*/, const QString& reason) {
        showToast(reason, ToastSeverity::Warning, 3000);
    });

    // Task 33: RadioModel::stopAllTx stopped a transmission. A non-empty
    // message is shown for 10 s, as Thetis's StopAllTx does:
    // From Thetis console.cs:45338-45341 [v2.10.3.15]:
    //   if (!string.IsNullOrEmpty(msg))
    //   {
    //       infoBar.Warning(msg, false, 10000);
    //   }
    connect(m_radioModel, &RadioModel::transmitStopped,
            this, [this](const QString& message) {
        if (!message.isEmpty()) {
            showToast(message, ToastSeverity::Warning, 10000);
        }
    });

    // Phase 3Q Task 10: auto-connect failure / ambiguity surface.
    //
    // autoConnectFailed — the probe timed out or the radio rejected the
    // handshake. Open the ConnectionPanel, highlight the failed target,
    // and post an 8-second status-bar explanation.
    connect(m_radioModel, &RadioModel::autoConnectFailed,
            this, [this](const QString& mac, NereusSDR::ConnectFailure reason) {
        showConnectionPanel();
        if (m_connectionPanel) {
            m_connectionPanel->highlightMac(mac);
        }
        const auto saved = AppSettings::instance().savedRadio(mac);
        const QString name = (saved.has_value() && !saved->info.name.isEmpty())
            ? saved->info.name : mac;
        const QString reasonText =
            (reason == NereusSDR::ConnectFailure::Timeout)
                ? QStringLiteral("isn't reachable from this network")
                : QStringLiteral("returned an error");
        showToast(
            QStringLiteral("Auto-connect target %1 %2. Pick a different radio or update the address.")
                .arg(name, reasonText),
            ToastSeverity::Warning, 8000);
    });

    // autoConnectAmbiguous — multiple radios have AutoConnect = true.
    // The most-recently-connected MAC was chosen; post a 6-second advisory
    // pointing the user at Manage Radios for cleanup.
    connect(m_radioModel, &RadioModel::autoConnectAmbiguous,
            this, [this](int count, const QString& chosenMac) {
        const auto saved = AppSettings::instance().savedRadio(chosenMac);
        const QString name = (saved.has_value() && !saved->info.name.isEmpty())
            ? saved->info.name : chosenMac;
        showToast(
            QStringLiteral("%1 radios marked Auto-connect on launch. Using %2 (most recent). "
                           "Adjust in Manage Radios.")
                .arg(count).arg(name),
            ToastSeverity::Info, 6000);
    });

    // WDSP wisdom progress dialog — shown as a modal window during first-run
    // wisdom generation. Pattern from AetherSDR MainWindow::enableNr2WithWisdom().
    connect(m_radioModel->wdspEngine(), &WdspEngine::wisdomProgress,
            this, [this](int percent, const QString& status) {
        // Create dialog on first progress signal
        if (!m_wisdomDialog && percent < 100) {
            m_wisdomDialog = new QProgressDialog(this);
            m_wisdomDialog->setWindowTitle(QStringLiteral("NereusSDR: FFTW Wisdom"));
            m_wisdomDialog->setLabelText(
                QStringLiteral("Optimizing FFT plans for DSP engine...\n\n"
                               "This only happens on first run."));
            m_wisdomDialog->setRange(0, 100);
            m_wisdomDialog->setValue(0);
            m_wisdomDialog->setCancelButton(nullptr);
            m_wisdomDialog->setAutoClose(false);
            m_wisdomDialog->setMinimumWidth(500);
            m_wisdomDialog->setMinimumDuration(0);
            m_wisdomDialog->setWindowModality(Qt::ApplicationModal);
            m_wisdomDialog->setStyleSheet(QStringLiteral(
                "QProgressDialog { background: #0f0f1a; }"
                "QLabel { color: #c8d8e8; font-size: 13px; }"
                "QProgressBar {"
                "  text-align: center; font-size: 13px;"
                "  font-weight: bold; color: #c8d8e8;"
                "  background: #1a2a3a; border: 1px solid #2e4e6e;"
                "  border-radius: 3px; min-height: 24px;"
                "}"
                "QProgressBar::chunk { background: #00b4d8; }"));
            m_wisdomDialog->show();
        }

        if (m_wisdomDialog) {
            m_wisdomDialog->setValue(percent);
            if (!status.isEmpty() && percent < 100) {
                m_wisdomDialog->setLabelText(
                    QStringLiteral("Optimizing FFT plans for DSP engine...\n\n%1").arg(status));
            }
            if (percent >= 100) {
                m_wisdomDialog->setLabelText(QStringLiteral("FFTW planning complete!"));
                m_wisdomDialog->setValue(100);
                // Auto-close after brief delay
                QTimer::singleShot(800, this, [this]() {
                    if (m_wisdomDialog) {
                        m_wisdomDialog->close();
                        m_wisdomDialog->deleteLater();
                        m_wisdomDialog = nullptr;
                    }
                });
            }
        }
    });

    // R-R3-38: local discovery/auto-connect starts in startInitialConnection,
    // after the coordinator has retired the previous window and installed
    // this one. Standalone construction retains Automatic startup below.

    // Phase 3J-2 + 3R M3: restore each spot client's auto-connect /
    // auto-start state. Sibling to tryAutoReconnect above; deferred via
    // singleShot(0, ...) so the QTcpSocket / QUdpSocket / QWebSocket
    // owned by each client see a fully-spun event loop before any
    // network I/O fires. Each client guards against double-start so
    // re-invocation is harmless (e.g. if a future code path also calls
    // this after a profile switch).
    //
    // 2026-05-18 bench fix: seed the SpectrumWidget per-source visibility
    // mask BEFORE restoring any auto-start state.  Without this, the
    // panadapter overlay mask (m_spotSourceVisible) is empty at startup
    // and the renderer's missing-key fallback (true == visible) leaks
    // every source's spots regardless of the user's "Show on panadapter"
    // checkbox state.  Bench operator hit it with FreeDV: FreeDvAutoStart
    // + Display checkbox unchecked = spots still painting at app start
    // until SpotHub was opened manually.  Six of seven sources share the
    // same symmetry break (PSK Reporter is send-only); fixing it here
    // closes the gap for all of them in one place.  The matching seed
    // inside openSpotHub() is now a defensive guard kept for the case
    // where the spectrum widget races construction; see lines below.
    QTimer::singleShot(0, this, [this] {
        if (activeSpectrumWidget()) {
            activeSpectrumWidget()->loadSpotDisplaySettings();
        }
        if (m_radioModel) {
            m_radioModel->restoreSpotClientAutoStartState();
        }
    });

    // Phase 3O Sub-Phase 11/12 — VAX first-run / startup rescan.
    // The Setup → Audio → VAX page (AudioVaxPage) is now live; users
    // who skip the first-run dialog can reach cable binding via
    // Setup → Audio → VAX at any time.
    // Deferred via singleShot(0, ...) so the UI is fully built first.
    QTimer::singleShot(0, this, &MainWindow::checkVaxFirstRun);

    // Task 23 — auto-open the Linux audio first-run dialog when no backend
    // is detected on this host. The check is deferred via singleShot(0, ...)
    // so the event loop is spinning before the modal dialog is posted.
    // The dialog's Dismiss button (Task 18) sets Audio/LinuxFirstRunSeen=True,
    // so this trigger fires at most once per user installation.
#if defined(Q_OS_LINUX)
    // Remote-daemon R2 Task 20: skipped on a remote client. Its AudioEngine
    // is constructed but never started (Task 4), so linuxBackend() reports
    // None regardless of what the host actually has, and the operator would
    // be shown a first-run dialog about a local sound card that R2 never
    // uses -- with the "seen" flag then set, hiding the real prompt if that
    // same machine is later run in local direct mode.
    //
    // R-R3-49 / R-R3-21: nor in a test run. The dialog is modal (exec()),
    // so a window built by a test on a host with no Linux sound server
    // waited forever for a click.
    if (!firstRunPromptsBarredForTestRun()
        && m_radioModel->ownsLocalDsp()
        && m_radioModel->audioEngine()->linuxBackend() == LinuxAudioBackend::None
        && AppSettings::instance().value(QStringLiteral("Audio/LinuxFirstRunSeen"),
                                          QStringLiteral("False")).toString()
               != QStringLiteral("True")) {
        QTimer::singleShot(0, this, &MainWindow::showAudioDiagnoseDialog);
    }
#endif

    // ── Phase 23: TCI Server instantiation ───────────────────────────────────
    // Phase 23 (bench fix 2026-05-10): TciServer instance is now created in
    // the constructor BEFORE buildUI (above) so populateDefaultMeter's applet
    // gate sees a non-null m_tciServer. Auto-start happens HERE (post-UI) so
    // serverStarted/serverStopped signals find the applets + indicator wired.
#ifdef HAVE_WEBSOCKETS
    if (m_tciServer) {
        auto& s = AppSettings::instance();
        const bool enabled = s.value(QStringLiteral("TciServerEnabled"),
                                     QStringLiteral("False")).toString()
                             == QStringLiteral("True");
        const quint16 port = static_cast<quint16>(
            s.value(QStringLiteral("TciServerPort"),
                    QStringLiteral("50001")).toString().toUShort());
        // Phase 3J-1 closeout Item 1 (2026-05-12): bind-interface dropdown.
        // Default to loopback so a fresh install only exposes TCI to localhost;
        // operator opts into LAN exposure via Setup → CAT & Network → TCI Server.
        const QString bindStr = s.value(QStringLiteral("TciServerBindAddress"),
                                        QStringLiteral("127.0.0.1")).toString();
        QHostAddress bindAddr;
        if (!bindAddr.setAddress(bindStr)) {
            // Malformed AppSettings value — fall back to loopback rather than
            // refusing to start the server.  populateBindAddressCombo() ensures
            // only valid strings get persisted, but a hand-edited XML file
            // shouldn't crash startup.
            bindAddr = QHostAddress(QHostAddress::LocalHost);
        }
        // R-R3-48: through the one switch. At startup this window only
        // applies it here; the Core keeps its own switch.
        if (m_tciSwitch) {
            m_tciSwitch->setSwitch(enabled, port, bindAddr, /*tellCore=*/false);
        } else if (enabled) {
            m_tciServer->start(bindAddr, port);
        }
    }
#endif

    // Defensive save on aboutToQuit. closeEvent is fine for ⌘Q but
    // does NOT run when the process is signaled (SIGTERM from pkill,
    // Activity Monitor force-quit, debugger detach). Without this
    // hook, any container/state change made mid-session is lost on
    // signal-based shutdown. saveState is idempotent so the ⌘Q path
    // (closeEvent → saveState; aboutToQuit → saveState again) is
    // harmless. (Preserved from main PR #13 alongside Phase 3I's
    // singleShot auto-reconnect above.)
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        // Same flush as closeEvent — covers the signal-shutdown path
        // (SIGTERM, force-quit, debugger detach) where closeEvent
        // doesn't run. Idempotent when closeEvent already flushed.
        m_shuttingDown = true;
        if (m_desktopStationController) { m_desktopStationController->stop(); }
        // 2026-05-22 bench-finding: graceful radio disconnect MUST happen
        // before the process tears down so the SendStop frame (run=0
        // CmdHighPriority) actually reaches the wire.  Without this,
        // pkill / SIGTERM / closing the window via the dock all skip
        // P2RadioConnection::disconnect, the radio gateware never sees
        // run=0, and lockups on the G2E require power-cycle.  Call
        // disconnectFromRadio FIRST so its 20 ms flush+sleep runs before
        // any other shutdown cleanup.
        if (m_radioModel) {
            m_radioModel->disconnectFromRadio();
            m_radioModel->flushPendingSettingsSave();
        }
        if (m_containerManager) {
            m_containerManager->saveState();
        }
        // Issue #206 — also flush window geometry on signal-based
        // shutdown (SIGTERM / force-quit). Idempotent with the
        // closeEvent path above.
        saveMainWindowGeometry();
        AppSettings::instance().save();
    });

    // ── Remote-daemon R2 Task 20: the remote gate, then the dial ─────────
    //
    // Last in the constructor on purpose. applyRemoteRoleGating() reaches
    // for m_actConnect / m_actDisconnect / m_actManageRadios /
    // m_actProtocolInfo, which buildMenuBar() creates near the top of this
    // body, and it must run after the initial-enablement block there has
    // already had its say. It is a no-op in local direct mode.
    applyRemoteRoleGating();

    ensureRemoteSession();
    if (startup == ConnectionStartup::Automatic) { startInitialConnection(); }
}

MainWindow::~MainWindow()
{
    m_shuttingDown = true;
    // Retire native hosts while their windows and original construction owners
    // still exist. QObject's later child teardown is too late to reparent a
    // parked native applet safely back into this window.
    if (m_meterPoller) { m_meterPoller->stop(); }
    if (m_radioModel && m_radioModel->meterPoller()==m_meterPoller) { m_radioModel->setMeterPoller(nullptr); }
    if (m_containerManager) {
        auto* registry=m_containerManager->contentRegistry();
        if (m_radioModel && m_radioModel->containerManager()==m_containerManager) { m_radioModel->setContainerManager(nullptr); }
        delete m_containerManager; m_containerManager=nullptr;
        delete registry;
    }
    setDesktopStationController(nullptr);
    // QWidget deletes children after this class's members are destroyed.
    // A toast's destroyed callback edits m_toasts, so retire it while that
    // list is still alive. Otherwise station replacement corrupts freed
    // memory whenever a connection/retry notice is still visible.
    while (!m_toasts.isEmpty()) {
        const QPointer<StatusToast> toast = m_toasts.takeLast();
        if (toast) {
            disconnect(toast, nullptr, this, nullptr);
            delete toast.data();
        }
    }
    if (m_stationClient) {
        // Task 78: quitting is leaving on purpose; the Core frees this
        // window's place (and transmit) now instead of after 3 minutes.
        m_stationClient->leaveSession();
        m_stationClient->disconnectFromStation(QStringLiteral("client shutting down"));
    }
    delete m_remoteTelemetry;
    m_remoteTelemetry = nullptr;
    // Parity Task 29: the MOX display controller (a child, destroyed after
    // the members) lets go of its transmit source before the source and the
    // media controller it draws from go.
    if (m_moxDisplay) {
        m_moxDisplay->setSource(nullptr);
    }
    m_remoteTxDisplaySource.reset();
    m_localTxDisplaySource.reset();
    // Join remote playback before QObject destroys the earlier-created
    // RadioModel child and its speaker AudioEngine.
    delete m_remoteMedia;
    m_remoteMedia = nullptr;
    // R-R3-44: likewise the VAX feeders' workers, which write this
    // computer's VAX outputs on the same engine. The controller is gone, so
    // the router's final releases find nothing to release.
    delete m_remoteVax;
    m_remoteVax = nullptr;
    delete m_remoteConnectionPanel;
    m_remoteConnectionPanel = nullptr;
    delete m_remoteConnection;
    m_remoteConnection = nullptr;
    delete m_stationClient;
    m_stationClient = nullptr;
    if (m_panStack) { m_panStack->prepareShutdown(); }
}

bool MainWindow::desktopHosting() const
{
    return m_radioModel && m_radioModel->role() == RadioModel::Role::Local
        && m_desktopStationController
        && (m_desktopStationController->host() || !m_desktopHostStopConfirmed);
}

bool MainWindow::desktopSliceAllowed(int sliceId) const
{
    return desktopHosting() && stationControlsSlice(m_radioModel, sliceId);
}

bool MainWindow::stationControlsSlice(const RadioModel* model, int sliceId)
{
    if (!model || !model->sliceOwnership() || !model->sliceById(sliceId)) { return false; }
    // Slice control plan Task 2: a slice the station device may change as
    // its own (not one it runs held for an absent device).
    const SliceOwnership& ownership = *model->sliceOwnership();
    return SliceAccessPolicy::mayChange(ownership, SliceOwnership::stationDevice(), sliceId)
        && !ownership.mark(sliceId).isHeld();
}

SliceModel* MainWindow::stationTransmitSlice(RadioModel* model)
{
    if (!model || !model->sliceOwnership()) { return nullptr; }
    if (TxSliceArbiter* arbiter = model->txSliceArbiter()) {
        SliceModel* bound = arbiter->txBoundSlice();
        if (bound && stationControlsSlice(model, bound->sliceIndex())) { return bound; }
    }
    const int chosen = model->sliceOwnership()->activeFor(SliceOwnership::stationDevice());
    if (stationControlsSlice(model, chosen)) { return model->sliceById(chosen); }
    for (SliceModel* slice : model->slices()) {
        if (slice && stationControlsSlice(model, slice->sliceIndex())) { return slice; }
    }
    return nullptr;
}

bool MainWindow::desktopListensTo(int sliceId) const
{
    if (!desktopHosting() || !m_radioModel || !m_radioModel->sliceOwnership()
        || !m_radioModel->sliceById(sliceId) || desktopSliceAllowed(sliceId)) { return false; }
    const SliceOwnership& ownership = *m_radioModel->sliceOwnership();
    return !ownership.mark(sliceId).isHeld()
        && ownership.isListening(SliceOwnership::stationDevice(), sliceId);
}

SliceModel* MainWindow::activeSliceForWindow() const
{
    if (!m_radioModel) { return nullptr; }
    if (!desktopHosting()) { return m_radioModel->activeSlice(); }
    SliceOwnership* ownership = m_radioModel->sliceOwnership();
    if (!ownership) { return nullptr; }
    const int chosen = ownership->activeFor(SliceOwnership::stationDevice());
    if (desktopSliceAllowed(chosen)) { return m_radioModel->sliceById(chosen); }
    for (SliceModel* slice : m_radioModel->slices()) {
        if (slice && desktopSliceAllowed(slice->sliceIndex())) { return slice; }
    }
    return nullptr;
}

SliceModel* MainWindow::windowRxSlice() const
{
    if (!m_radioModel) { return nullptr; }
    if (desktopHosting()) {
        // SliceOwnership::activeRxFor: the station device's receive focus,
        // a controlled slice or one it listens to.
        if (SliceOwnership* ownership = m_radioModel->sliceOwnership()) {
            const int rx = ownership->activeRxFor(SliceOwnership::stationDevice());
            if (desktopSliceAllowed(rx) || desktopListensTo(rx)) {
                return m_radioModel->sliceById(rx);
            }
        }
        return activeSliceForWindow();
    }
    // A remote window that shares slices: the slice whose access entry
    // names this device in activeRx (the Core's selectRx answer).
    if (m_stationClient && m_stationClient->remoteSliceAccessAvailable()) {
        if (const SliceAccessMirror* access = m_stationClient->sliceAccess()) {
            const QString self = access->selfDeviceId();
            for (SliceModel* slice : m_radioModel->slices()) {
                if (!slice) { continue; }
                const std::optional<SliceAccessMirror::Entry> entry =
                    access->entry(slice->sliceIndex());
                if (entry && entry->activeRx.contains(self)) { return slice; }
            }
        }
    }
    return m_radioModel->activeSlice();
}

bool MainWindow::sliceShownInWindow(int sliceId) const
{
    if (!m_panStack) { return true; }
    // Slice control plan Task 16 (ruling U7): shown means on one of this
    // window's pans, not on a pan made only to hold another device's slice.
    return !windowPanFor(m_radioModel ? m_radioModel->sliceById(sliceId) : nullptr).isEmpty();
}

bool MainWindow::windowSharesSlices() const
{
    return desktopHosting() || sliceAccessClient() != nullptr;
}

bool MainWindow::windowControlsSlice(int sliceId) const
{
    if (desktopHosting()) { return desktopSliceAllowed(sliceId); }
    if (StationClient* client = sliceAccessClient()) {
        const SliceAccessMirror* access = client->sliceAccess();
        if (!access) { return true; }
        const std::optional<SliceAccessMirror::Entry> entry = access->entry(sliceId);
        return entry && entry->controllerDeviceId == access->selfDeviceId();
    }
    return true;
}

bool MainWindow::windowListensTo(int sliceId) const
{
    if (desktopHosting()) { return desktopListensTo(sliceId); }
    if (StationClient* client = sliceAccessClient()) {
        const SliceAccessMirror* access = client->sliceAccess();
        if (!access) { return false; }
        const QString self = access->selfDeviceId();
        const std::optional<SliceAccessMirror::Entry> entry = access->entry(sliceId);
        return entry && entry->controllerDeviceId != self && entry->listeners.contains(self);
    }
    return false;
}

QStringList MainWindow::windowPanIds() const
{
    QStringList ids;
    if (!m_panStack) { return ids; }
    for (const QString& id : panIdsForLayout(m_panStack->currentLayoutId())) {
        if (m_panStack->panadapter(id)) { ids << id; }
    }
    return ids;
}

QString MainWindow::windowPanFor(const SliceModel* slice) const
{
    if (!slice || !m_panStack) { return QString(); }
    const QStringList ids = windowPanIds();
    const QString placed = m_listenPlacement.value(slice->sliceIndex());
    if (!placed.isEmpty() && ids.contains(placed)) { return placed; }
    if (ids.contains(slice->panKey())) { return slice->panKey(); }
    for (const QString& id : ids) {
        PanadapterApplet* pan = m_panStack->panadapter(id);
        if (pan && pan->associatedSlices().contains(slice->sliceIndex())) { return id; }
    }
    return QString();
}

void MainWindow::rehostSliceView(SliceModel* slice)
{
    // VFO flag crash lane fix round (2026-10-01): Slice A is rehosted like
    // every other slice; its flag is built by createSliceFlag too.
    if (!slice) { return; }
    const int id = slice->sliceIndex();
    SpectrumWidget* dest = spectrumForSlice(slice);
    VfoWidget* flag = m_vfoWidgetsBySlice.value(id, nullptr);
    if (flag && flag->parentWidget() == dest) { return; }
    if (flag) {
        if (auto* old = qobject_cast<SpectrumWidget*>(flag->parentWidget())) {
            old->removeVfoWidget(id);
        }
    }
    m_vfoWidgetsBySlice.remove(id);
    if (dest) { createSliceFlag(slice, dest); }
}

void MainWindow::revealSliceInWindow(int sliceId)
{
    if (!m_panStack || !m_radioModel) { return; }
    SliceModel* slice = m_radioModel->sliceById(sliceId);
    if (!slice) { return; }
    QString target = windowPanFor(slice);
    if (!target.isEmpty()) {
        // Ruling U2: a pan that shows the slice comes forward, a floating
        // one included. Nothing moves.
        m_panStack->setActivePan(target);
        m_panStack->raiseFloatingPan(target);
        return;
    }
    // Ruling U1: an empty pan in the main window.
    for (const QString& id : windowPanIds()) {
        if (m_panStack->isFloating(id)) { continue; }
        bool empty = true;
        for (SliceModel* other : m_radioModel->slices()) {
            if (other && other != slice && windowPanFor(other) == id) { empty = false; break; }
        }
        if (empty) { target = id; break; }
    }
    if (target.isEmpty()) {
        // The next layout that fits, one pan more, in this window. No
        // slice is added to any other pan it opens.
        static const QHash<int, QString> kGrowTo = {
            {1, QStringLiteral("2v")},
            {2, QStringLiteral("3v")},
            {3, QStringLiteral("2x2")},
            {4, QStringLiteral("3h2")},
        };
        const QStringList before = panIdsForLayout(m_panStack->currentLayoutId());
        const QString grown = kGrowTo.value(before.size());
        const QStringList after = panIdsForLayout(grown);
        if (!grown.isEmpty() && after.size() <= panLayoutLimitFor(m_radioModel)) {
            QStringList floating;
            for (const QString& id : before) {
                if (m_panStack->isFloating(id)) { floating << id; }
            }
            m_panStack->applyLayout(grown, after);
            // applyLayout docks every floating pan; the operator's floating
            // pans go back out.
            for (const QString& id : std::as_const(floating)) {
                m_panStack->floatPanadapter(id);
            }
            // A slice whose flag sat on a pan the new layout retired keeps
            // its flag.
            for (SliceModel* other : m_radioModel->slices()) {
                if (other && other != slice) { rehostSliceView(other); }
            }
            target = after.value(before.size());
        }
    }
    if (target.isEmpty()) {
        // No larger layout fits: the operator picks the pan.
        QMenu* menu = new QMenu(this);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        const QString letter = QString(QChar(QLatin1Char('A').unicode() + std::max(0, sliceId)));
        menu->addSection(tr("Show Slice %1 on").arg(letter));
        for (const QString& id : windowPanIds()) {
            const int number = id.mid(id.lastIndexOf(QLatin1Char('-')) + 1).toInt() + 1;
            QAction* pick = menu->addAction(tr("Pan %1").arg(number));
            connect(pick, &QAction::triggered, this, [this, sliceId, id]() {
                SliceModel* chosen = m_radioModel ? m_radioModel->sliceById(sliceId) : nullptr;
                if (!chosen || !m_panStack || !m_panStack->panadapter(id)) { return; }
                if (windowControlsSlice(sliceId)) {
                    chosen->setPanKey(id);
                } else {
                    m_listenPlacement.insert(sliceId, id);
                    m_markerOnlyPlacement.remove(sliceId);
                    m_panStack->moveSliceToPan(sliceId, id);
                    rehostSliceView(chosen);
                    rebuildFftRouting();
                    refreshDesktopStationState();
                }
                m_panStack->setActivePan(id);
                m_panStack->raiseFloatingPan(id);
            });
        }
        menu->popup(QCursor::pos());
        return;
    }
    if (windowControlsSlice(sliceId)) {
        slice->setPanKey(target);
    } else {
        // The slice's pan key belongs to its controller; this window only
        // places it.
        m_listenPlacement.insert(sliceId, target);
        m_markerOnlyPlacement.remove(sliceId);
        m_panStack->moveSliceToPan(sliceId, target);
        rehostSliceView(slice);
        rebuildFftRouting();
    }
    m_panStack->setActivePan(target);
}

void MainWindow::reconcileListenPlacements()
{
    if (m_reconcilingPlacements || !m_radioModel) { return; }
    m_reconcilingPlacements = true;
    bool changed = false;
    const QList<int> placed = m_listenPlacement.keys();
    for (int id : placed) {
        SliceModel* slice = m_radioModel->sliceById(id);
        const QString pan = m_listenPlacement.value(id);
        if (!slice) {
            m_listenPlacement.remove(id);
            m_markerOnlyPlacement.remove(id);
            changed = true;
            continue;
        }
        if (windowControlsSlice(id)) {
            // Now this window's to control: the placement becomes its pan.
            m_listenPlacement.remove(id);
            // A slice this window controls is no longer only a marker there.
            if (m_markerOnlyPlacement.remove(id) && m_panStack) {
                if (SpectrumWidget* shown = m_panStack->spectrum(pan)) {
                    shown->setEdgeMarkedSlice(id, false);
                }
            }
            changed = true;
            if (m_panStack && m_panStack->panadapter(pan)) { slice->setPanKey(pan); }
            continue;
        }
        if (!windowListensTo(id)) {
            m_listenPlacement.remove(id);
            m_markerOnlyPlacement.remove(id);
            changed = true;
            if (m_panStack && slice->panKey() != pan) {
                if (PanadapterApplet* applet = m_panStack->panadapter(pan)) {
                    applet->removeSlice(id);
                }
            }
        }
    }
    if (changed) { rebuildFftRouting(); }
    m_reconcilingPlacements = false;
}

bool MainWindow::markerOnlyPlacement(int sliceId) const
{
    return m_markerOnlyPlacement.contains(sliceId)
        && !m_listenPlacement.value(sliceId).isEmpty();
}

bool MainWindow::hostIsNotSlicesOwnPan(const SliceModel* slice) const
{
    if (!slice || !m_panStack) { return false; }
    const int id = slice->sliceIndex();
    if (windowControlsSlice(id) || !windowListensTo(id)) { return false; }
    if (m_panStack->panadapter(m_listenPlacement.value(id))) { return true; }
    // Its own pan is gone: spectrumForSlice falls back to the active pan.
    return !slice->panKey().isEmpty() && !m_panStack->panadapter(slice->panKey());
}

StationServer* MainWindow::sliceAccessServer() const
{
    return desktopHosting() && m_desktopStationController
        ? m_desktopStationController->server() : nullptr;
}

StationClient* MainWindow::sliceAccessClient() const
{
    const bool remoteShared = !desktopHosting() && m_stationClient
        && m_stationClient->remoteDevices() && m_stationClient->remoteSliceAccessAvailable();
    return remoteShared ? m_stationClient : nullptr;
}

namespace {

// Who controls each slice, as the chooser and the flags say it.
QHash<int, VfoWidget::SliceAccess> windowSliceAccess(const QList<SliceChooser::Row>& rows)
{
    QHash<int, VfoWidget::SliceAccess> access;
    for (const SliceChooser::Row& row : rows) {
        access.insert(row.sliceId, SliceChooser::flagAccessFor(row));
    }
    return access;
}

QList<SliceChooser::Row> windowSliceRows(RadioModel& model, StationServer* server,
                                         StationClient* client)
{
    if (server) { return SliceChooser::rowsForHostingDesktop(model, *server); }
    if (client) {
        return SliceChooser::rowsForRemoteWindow(model, client->sliceAccess(),
                                                 *client->remoteDevices());
    }
    return {};
}

} // namespace

QString MainWindow::sliceChangeRefusal(int sliceId) const
{
    if (!m_radioModel) { return QString(); }
    // The RX applet's rule: a slice this window listens to (another device,
    // the Core's own window or nobody controls it) is held with that reason.
    const QList<SliceChooser::Row> rows =
        windowSliceRows(*m_radioModel, sliceAccessServer(), sliceAccessClient());
    for (const SliceChooser::Row& row : rows) {
        if (row.sliceId != sliceId) { continue; }
        const VfoWidget::SliceAccess access = SliceChooser::flagAccessFor(row);
        return access.state == VfoWidget::SliceAccess::State::Listening
            ? access.heldReason : QString();
    }
    return QString();
}

void MainWindow::refreshOverlayAttAccess()
{
    if (!m_radioModel || !m_panStack) { return; }
    for (auto it = m_overlayPanels.constBegin(); it != m_overlayPanels.constEnd(); ++it) {
        SpectrumOverlayPanel* panel = it.value();
        if (!panel) { continue; }
        // The pan's own active slice, a listened one included; else the
        // slice the panel resolves.
        int sliceId = -1;
        if (const auto* applet = m_panStack->panadapter(it.key())) {
            sliceId = applet->activeSliceIndex();
        }
        if (sliceId < 0) {
            if (const SliceModel* slice = sliceForPan(it.key())) {
                sliceId = slice->sliceIndex();
            }
        }
        panel->setAttHeldReason(sliceId >= 0 ? sliceChangeRefusal(sliceId) : QString());
    }
}

void MainWindow::refreshRxAppletSlices()
{
    // TX rulings (item 3): the pans' ATT flyouts follow the same access.
    refreshOverlayAttAccess();
    if (!m_rxApplet || !m_radioModel) { return; }
    const bool remoteShared = sliceAccessClient() != nullptr;
    const QList<SliceChooser::Row> rows =
        windowSliceRows(*m_radioModel, sliceAccessServer(), sliceAccessClient());
    const QHash<int, VfoWidget::SliceAccess> access = windowSliceAccess(rows);
    // Ruling U7: a tab for each slice this window controls, and for each
    // slice it listens to that it shows.
    QVector<SliceModel*> tabs;
    for (SliceModel* slice : m_radioModel->slices()) {
        if (!slice) { continue; }
        const int id = slice->sliceIndex();
        bool tab = false;
        if (desktopHosting()) {
            tab = desktopSliceAllowed(id) || (desktopListensTo(id) && sliceShownInWindow(id));
        } else if (remoteShared) {
            for (const SliceChooser::Row& row : std::as_const(rows)) {
                if (row.sliceId != id) { continue; }
                tab = row.controller == SliceChooser::Controller::ThisWindow
                    || (row.listeningHere && sliceShownInWindow(id));
                break;
            }
        } else {
            tab = true;
        }
        if (tab) { tabs.append(slice); }
    }
    SliceModel* rx = windowRxSlice();
    if (rx && !tabs.contains(rx)) {
        rx = desktopHosting() ? activeSliceForWindow() : m_radioModel->activeSlice();
        if (rx && !tabs.contains(rx)) { rx = nullptr; }
    }
    const int rxId = rx ? rx->sliceIndex() : -1;
    // The access first, so a listened slice is never bound with its
    // controls live.
    m_rxApplet->setSliceTabAccess(access);
    m_rxApplet->setSliceAccess(access.value(rxId));
    m_rxApplet->updateSliceButtons(tabs, rxId);
    m_rxApplet->setSlice(rx);
    if (rx) { m_rxApplet->setSliceIndex(rxId); }
    // The bottom bar and the flag focus follow the same slice.
    if (m_rxDashboard && m_rxDashboard->slice() != windowRxSlice()) {
        refreshActiveSlicePresentation();
    }
}

void MainWindow::refreshActiveSlicePresentation()
{
    // Slice control plan Task 15: the bottom bar, the flag focus and the
    // meters follow this window's RX slice, which may be one it listens to;
    // the menu checks below follow the slice it controls.
    SliceModel* rx = windowRxSlice();
    if (m_rxDashboard) {
        m_rxDashboard->bindSlice(rx);
        if (rx) { m_rxDashboard->setSliceLetter(rx->sliceLetter()); }
    }
    if (m_panStack && rx) {
        m_panStack->setActiveSliceOnHostingPan(rx->sliceIndex());
        if (desktopHosting()) {
            for (PanadapterApplet* pan : m_panStack->allApplets()) {
                if (pan && pan->associatedSlices().contains(rx->sliceIndex())) {
                    m_panStack->setActivePan(pan->panId());
                    break;
                }
            }
        }
    }
    if (m_meterPoller && rx) {
        if (RxChannel* channel = m_radioModel->rxChannelForSlice(rx->sliceIndex())) {
            m_meterPoller->setRxChannel(channel);
        }
    }
    SliceModel* slice = activeSliceForWindow();
    if (m_anfAction) {
        const QSignalBlocker block(m_anfAction);
        m_anfAction->setChecked(slice && slice->anfEnabled());
    }
    if (m_nrGroup) {
        for (QAction* action : m_nrGroup->actions()) {
            const QSignalBlocker block(action);
            action->setChecked(slice && action->data().toInt() == int(slice->activeNr()));
        }
    }
    if (m_nbGroup) {
        const NbMode order[] = {NbMode::Off, NbMode::NB, NbMode::NB2};
        const QList<QAction*> actions = m_nbGroup->actions();
        for (int i = 0; i < actions.size() && i < 3; ++i) {
            const QSignalBlocker block(actions[i]);
            actions[i]->setChecked(slice && slice->nbMode() == order[i]);
        }
    }
    const auto syncToggle = [slice](QAction* action, bool on) {
        if (!action) { return; }
        const QSignalBlocker block(action);
        action->setChecked(slice && on);
    };
    syncToggle(m_snbAction, slice && slice->snbEnabled());
    syncToggle(m_apfAction, slice && slice->apfEnabled());
    syncToggle(m_binAction, slice && slice->binauralEnabled());
    if (m_agcGroup) {
        const AGCMode order[] = {AGCMode::Off, AGCMode::Long, AGCMode::Slow,
                                 AGCMode::Med, AGCMode::Fast, AGCMode::Custom};
        const QList<QAction*> actions = m_agcGroup->actions();
        for (int i = 0; i < actions.size() && i < 6; ++i) {
            const QSignalBlocker block(actions[i]);
            actions[i]->setChecked(slice && slice->agcMode() == order[i]);
        }
    }
    const DSPMode modes[] = {DSPMode::LSB, DSPMode::USB, DSPMode::DSB,
                             DSPMode::CWL, DSPMode::CWU, DSPMode::AM,
                             DSPMode::SAM, DSPMode::FM, DSPMode::DIGL,
                             DSPMode::DIGU, DSPMode::DRM, DSPMode::SPEC,
                             DSPMode::RADE_U, DSPMode::RADE_L};
    for (int i = 0; i < 14; ++i) {
        if (!m_modeActions[i]) { continue; }
        const QSignalBlocker block(m_modeActions[i]);
        m_modeActions[i]->setChecked(slice && slice->dspMode() == modes[i]);
    }
}

bool MainWindow::desktopOwnsTransmit() const
{
    StationServer* server = desktopHosting() ? m_desktopStationController->server() : nullptr;
    return server && server->transmitHolder()
        && server->transmitHolder()->isHeldBy(SliceOwnership::stationDevice());
}

void MainWindow::setDesktopStationController(DesktopStationController* controller)
{
    if (!m_radioModel || m_radioModel->role() != RadioModel::Role::Local) { return; }
    if (m_desktopStationController == controller) {
        refreshDesktopStationState();
        return;
    }
    const quint64 bindingGeneration = ++m_desktopBindingGeneration;
    const QPointer<DesktopStationController> replacement(controller);
    const QPointer<DesktopStationController> old = m_desktopStationController;
    m_desktopStationController = nullptr;
    disconnect(m_desktopHolderConnection);
    disconnect(m_desktopOwnershipConnection);
    disconnect(m_desktopActiveConnection);
    if (old) {
        const QPointer<MainWindow> self(this);
        old->stop();
        if (!self || bindingGeneration != m_desktopBindingGeneration) { return; }
        disconnect(old, nullptr, this, nullptr);
    }
    m_desktopHostStopConfirmed = true;
    if (m_desktopTakeDialog) {
        const QPointer<MainWindow> self(this);
        m_desktopTakeDialog->close();
        if (!self || bindingGeneration != m_desktopBindingGeneration) { return; }
    }
    if (controller && !replacement) { refreshDesktopStationState(); return; }
    m_desktopStationController = replacement;
    if (replacement) {
        connect(replacement, &DesktopStationController::hostingStateChanged,
                this, [this](bool enabled) {
            const QPointer<MainWindow> self(this);
            // TX badge take: a take does not outlive hosting.
            if (!enabled) { abandonTxBadgeTake(); }
            m_desktopHostStopConfirmed = !enabled;
            if (!desktopHosting() && m_desktopTakeDialog) { m_desktopTakeDialog->close(); }
            if (!self) { return; }
            refreshDesktopStationState();
        });
        // TX badge take: the badge's take ended after its call.
        connect(replacement, &DesktopStationController::takeFinished, this,
                [this](quint64 takeId, bool held) {
            if (held) {
                settleTxBadgeHostTake(takeId);
            } else if (takeId != 0 && takeId == m_txBadgeHostTakeId) {
                // Not assigned, or replaced by another request: nothing
                // moves, whoever holds transmit now.
                abandonTxBadgeTake();
            }
        });
        connect(replacement, &QObject::destroyed, this, [this] {
            const QPointer<MainWindow> self(this);
            abandonTxBadgeTake();
            m_desktopStationController = nullptr;
            m_desktopHostStopConfirmed = true;
            if (m_desktopTakeDialog) { m_desktopTakeDialog->close(); }
            if (!self) { return; }
            refreshDesktopStationState();
        });
        if (SliceOwnership* ownership = m_radioModel->sliceOwnership()) {
            m_desktopOwnershipConnection = connect(ownership, &SliceOwnership::markChanged,
                this, [this](int, const QByteArray&, const QByteArray&) {
                    refreshDesktopStationState();
                });
            m_desktopActiveConnection = connect(ownership, &SliceOwnership::activeChanged,
                this, [this] { refreshDesktopStationState(); });
        }
    }
    refreshDesktopStationState();
}

void MainWindow::refreshDesktopFlags()
{
    if (!m_radioModel || m_radioModel->role() != RadioModel::Role::Local) { return; }
    const bool hosting = desktopHosting();
    StationServer* server = hosting ? m_desktopStationController->server() : nullptr;
    SliceOwnership* ownership = m_radioModel->sliceOwnership();
    const int txId = m_radioModel->txSliceArbiter()
        ? m_radioModel->txSliceArbiter()->txBoundSliceId() : -1;
    for (auto it = m_vfoWidgetsBySlice.constBegin(); it != m_vfoWidgetsBySlice.constEnd(); ++it) {
        if (VfoWidget* flag = it.value()) {
            const int id = it.key();
            const bool mine = !hosting
                || (ownership && m_radioModel->sliceById(id) && desktopSliceAllowed(id));
            // Slice control plan Task 14a: a slice this window listens to
            // keeps its flag (held read-only by its slice access); its TX
            // badge is red while the slice is on the air.
            const bool listened = hosting && !mine && desktopListensTo(id);
            flag->setStationPresentationAllowed(mine || listened);
            if (hosting) {
                flag->setTxSlice(mine ? (desktopOwnsTransmit() && id == txId)
                                      : (listened && server && server->sliceOnAir(id)));
            } else { flag->setTxSlice(id == txId); }
        }
    }
}

void MainWindow::refreshDesktopStationState()
{
    if (!m_radioModel || m_radioModel->role() != RadioModel::Role::Local) { return; }
    const quint64 bindingGeneration = m_desktopBindingGeneration;
    const bool hosting = desktopHosting();
    StationServer* server = hosting ? m_desktopStationController->server() : nullptr;
    if (m_desktopBoundServer != server) {
        disconnect(m_desktopHolderConnection);
        disconnect(m_desktopDevicesConnection);
        disconnect(m_desktopPresenceConnection);
        m_desktopBoundServer = server;
        // Slice control plan Task 10: the station device's requests go to
        // the server this window hosts now.
        if (m_hostingQuestionDialog) {
            m_hostingQuestionDialog->disconnect(this);
            m_hostingQuestionDialog->close();
        }
        m_hostingSlices.reset();
        if (server) {
            m_hostingSlices = std::make_unique<HostingSliceActions>(server, m_radioModel);
            wireHostingSlices();
        }
        if (server && server->transmitHolder()) {
            m_desktopHolderConnection = connect(server->transmitHolder(),
                &TransmitHolder::changed, this, [this] {
                    const QPointer<MainWindow> self(this);
                    if (m_desktopTakeDialog) { m_desktopTakeDialog->close(); }
                    if (!self) { return; }
                    refreshDesktopStationState();
                });
        }
        if (server && server->connectedDevices()) {
            m_desktopDevicesConnection = connect(server->connectedDevices(),
                &ConnectedDevicesFacade::connectedDevicesChanged, this,
                &MainWindow::refreshForeignMarkers);
        }
        if (server && server->deviceSessions()) {
            m_desktopPresenceConnection = connect(server->deviceSessions(),
                &DeviceSessionRegistry::changed, this,
                &MainWindow::refreshForeignMarkers);
        }
    }
#ifdef HAVE_WEBSOCKETS
    if (m_tciServer && (hosting || m_desktopHostStopConfirmed)) {
        const QPointer<MainWindow> self(this);
        m_tciServer->setDesktopHostMode(hosting);
        if (!self) { return; }
        if (hosting) { m_desktopHostStopConfirmed = false; }
        if (bindingGeneration != m_desktopBindingGeneration) {
            refreshDesktopStationState();
            return;
        }
    }
#endif
    refreshDesktopFlags();
    // Slice control plan Task 15: the RX applet's tabs and bound slice.
    refreshRxAppletSlices();
    if (m_txApplet) {
        if (hosting) {
            m_txApplet->setDesktopKeyHandlers(
                [this](bool on) { requestDesktopTransmit(false, on); },
                [this](bool on) { requestDesktopTransmit(true, on); },
                [this] { return desktopOwnsTransmit() && m_radioModel
                    && m_radioModel->moxController() && m_radioModel->moxController()->isMox(); },
                [this] { return desktopOwnsTransmit() && m_radioModel && m_radioModel->isTune(); });
            // Fix wave (hosting 2-TONE parity): 2-TONE asks as MOX does.
            m_txApplet->setDesktopTwoToneHandler([this](bool on) {
                requestDesktopKey(DesktopStationController::Key::TwoTone, on);
            });
            // Slice control plan Task 11: the TX applet's band, per-band
            // power and every transmit control follow the slice transmit is
            // bound to, never a slice this window only listens to.
            m_txApplet->setTransmitSliceResolver([this]() -> SliceModel* {
                return desktopHosting() ? stationTransmitSlice(m_radioModel) : nullptr;
            });
            // U8: one letter per slice this window controls. A press moves
            // transmit there (the arbiter drops MOX first, ruling 8.10).
            m_txApplet->setTransmitSliceChoices(
                [this](int id) { return desktopSliceAllowed(id); },
                [this](int id) { activateTransmitSlice(id, true); },
                {},
                [this](int id) {
                    const TxSliceAction action = txSliceAction(id);
                    return TxApplet::TransmitSliceChoice{action.enabled, action.effectiveWords};
                });
        } else {
            m_txApplet->setDesktopKeyHandlers({}, {}, {}, {});
            m_txApplet->setDesktopTwoToneHandler({});
            m_txApplet->setTransmitSliceResolver({});
            m_txApplet->setTransmitSliceChoices({}, {}, {});
        }
    }
    // Slice control plan Task 11 fix: the flags' TX buttons follow who
    // holds transmit here, as the letters do.
    refreshFlagTransmitGates();
    // Station VOX: and VOX, as a remote window's does.
    applyDesktopVoxHolderGate();
    refreshActiveSlicePresentation();
    refreshContainerControls();
    for (SetupDialog* dialog : findChildren<SetupDialog*>()) {
        dialog->notifyReceiverSelectionChanged();
    }
    refreshForeignMarkers();
}

QString MainWindow::desktopVoxHolderReason() const
{
    // Station VOX (whole-branch review, TX path): VOX keys for the device
    // that armed it, only while it holds transmit (ruling 8.4). While
    // another device holds transmit, VOX armed here could key only in that
    // device's name, so it is not armed here; the Core refuses its key too.
    if (!desktopHosting()) { return {}; }
    StationServer* server = m_desktopStationController->server();
    TransmitHolder* holder = server ? server->transmitHolder() : nullptr;
    if (holder == nullptr) { return {}; }
    const std::optional<TransmitHolder::Holder> current = holder->holder();
    if (!current || current->deviceId == SliceOwnership::stationDevice()) { return {}; }
    // StationClient::otherHolderReason's words, in a hosting window.
    const QString name = current->name.isEmpty() ? QStringLiteral("Another device")
                                                 : current->name;
    return TxRefusals::otherDeviceHolds(name).text;
}

void MainWindow::applyDesktopVoxHolderGate()
{
    if (!m_radioModel || m_radioModel->role() != RadioModel::Role::Local) { return; }
    const QString reason = desktopVoxHolderReason();
    if (m_txApplet) { m_txApplet->setVoxPermitted(reason.isEmpty(), reason); }
    for (SetupDialog* dialog : findChildren<SetupDialog*>()) {
        dialog->setVoxPermitted(reason.isEmpty(), reason);
    }
}

void MainWindow::requestDesktopTransmit(bool tune, bool on)
{
    requestDesktopKey(tune ? DesktopStationController::Key::Tune
                           : DesktopStationController::Key::Mox, on);
}

void MainWindow::requestDesktopKey(DesktopStationController::Key key, bool on)
{
    if (!desktopHosting()) { return; }
    const QPointer<MainWindow> self(this);
    if (m_desktopTakeDialog) { m_desktopTakeDialog->close(); }
    if (!self || !desktopHosting()) { return; }
    const QPointer<DesktopStationController> controller(m_desktopStationController);
    using Key = DesktopStationController::Key;
    const auto result = key == Key::Tune      ? controller->requestTune(on)
                      : key == Key::TwoTone   ? controller->requestTwoTone(on)
                                              : controller->requestMox(on);
    if (!self || !controller || controller != m_desktopStationController) { return; }
    handleDesktopTakeResult(result);
    if (!self) { return; }
    if (m_txApplet) { m_txApplet->syncDesktopKeyState(); }
    refreshContainerControls();
}

void MainWindow::handleDesktopTakeResult(
    const DesktopStationController::RequestResult& result)
{
    if (result.state == DesktopStationController::RequestState::Refused) {
        // TX badge take: a refused take changes nothing.
        if (m_txBadgeTakeStage == TxBadgeStage::Transmit) { abandonTxBadgeTake(); }
        if (!result.reason.isEmpty()) { showToast(result.reason, ToastSeverity::Info, 3000); }
        return;
    }
    if (result.state != DesktopStationController::RequestState::Ask
        || !result.question || !desktopHosting()) { return; }
    const DesktopStationController::TakeQuestion question = *result.question;
    TakeTransmitDialog::Holder holder;
    holder.name = question.holderName;
    holder.shortName = question.holderShortName;
    holder.onAir = question.holderKeyed;
    auto* dialog = new TakeTransmitDialog(holder, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    if (question.key == DesktopStationController::Key::Tune) {
        dialog->detailLabel()->setText(question.holderKeyed
            ? tr("The other device is on the air. Taking over unkeys it before tuning.")
            : tr("The other device holds transmit. Take it to begin tuning."));
    } else if (question.key == DesktopStationController::Key::TwoTone) {
        // Fix wave (hosting 2-TONE parity).
        dialog->detailLabel()->setText(question.holderKeyed
            ? tr("The other device is on the air. Taking over unkeys it before "
                 "the 2-tone test starts.")
            : tr("The other device holds transmit. Take it to start the 2-tone test."));
    }
    m_desktopTakeDialog = dialog;
    const QPointer<DesktopStationController> controller = m_desktopStationController;
    connect(dialog, &QDialog::accepted, this, [this, controller, question] {
        m_desktopTakeDialog = nullptr;
        if (controller && controller == m_desktopStationController && desktopHosting()) {
            const QPointer<MainWindow> self(this);
            const auto confirmed = controller->confirmTake(question);
            if (!self) { return; }
            handleDesktopTakeResult(confirmed);
            if (!self) { return; }
            // TX badge take: the answer may have ended the badge's take.
            if (question.key == DesktopStationController::Key::Take) {
                settleTxBadgeHostTake(confirmed.takeId);
            }
        }
        if (m_txApplet) { m_txApplet->syncDesktopKeyState(); }
        refreshContainerControls();
    });
    connect(dialog, &QDialog::rejected, this, [this, question] {
        m_desktopTakeDialog = nullptr;
        // TX badge take: cancelled, nothing changes.
        if (question.key == DesktopStationController::Key::Take
            && m_txBadgeTakeStage == TxBadgeStage::Transmit) {
            abandonTxBadgeTake();
        }
        if (m_txApplet) { m_txApplet->syncDesktopKeyState(); }
        refreshContainerControls();
    });
    dialog->open();
}

void MainWindow::startInitialConnection()
{
    if (m_shuttingDown || m_initialConnectionStarted) { return; }
    m_initialConnectionStarted = true;
    if (m_station.isRemote()) {
        connectToStation();
    } else if (desktopHosting()) {
        // Reclaim the Core's radio choice, which may have changed from
        // another device since this window last ran.
        emit hostedInitialConnectionRequested();
    } else {
        m_radioModel->discovery()->startDiscovery();
        QTimer::singleShot(0, this, &MainWindow::tryAutoReconnect);
    }
}

void MainWindow::retireForSessionSwitch()
{
    m_retiringSession = true;
    // closeEvent includes saved geometry, FFT worker retirement and orderly
    // disconnect. Explicitly send it even for an unshown startup window.
    QCloseEvent event;
    closeEvent(&event);
    hide();
}

void MainWindow::setConnectionPickerManaged(bool managed)
{
    m_connectionPickerManaged = managed;
    // R-R3-38: Choose another Core opens Connections, which only a window
    // the picker manages has.
    if (m_coreStopBanner) { m_coreStopBanner->setChooseAnotherCoreAvailable(managed); }
    if (managed && m_actConnect) {
        m_actConnect->setEnabled(true);
        m_actConnect->setToolTip(tr("Choose a Core/radio pair or a radio for this computer"));
    }
    if (m_actManageRadios) {
        m_actManageRadios->setText(managed ? tr("&Connections…") : tr("&Manage Radios…"));
        m_actManageRadios->setToolTip(managed
            ? tr("Choose a Core/radio pair or a radio for this computer")
            : tr("Open the Connection Panel (radio list + ↻ Scan)"));
    }
    refreshCoreRadioActions();
    applyRemoteRoleGating();
}

// ---------------------------------------------------------------------------
// Remote-daemon R2 Task 20: bring up the wss session.
//
// Everything the session needs already exists by the time this runs:
// SettingsProxy is installed as AppSettings' remote backend by src/main.cpp
// BEFORE this window is constructed (see the constructor overload's
// precondition, and SettingsProxy.h's "ready()==false is load-bearing"
// section for why that ordering is not a style choice), and m_radioModel is
// Role::Remote, which StationClient refuses to run without.
// ---------------------------------------------------------------------------
void MainWindow::connectToStation()
{
    if (m_shuttingDown || !m_station.isRemote() || m_radioModel == nullptr) {
        return;
    }

    if (m_stationClient != nullptr && m_stationClient->isConnectionActive()) {
        return;
    }
    ensureRemoteSession();
    if (m_remoteConnection) { m_remoteConnection->connectToStation(); }
}

void MainWindow::wireRemoteStationVax()
{
    if (m_stationClient == nullptr) {
        return;
    }
    connect(m_stationClient, &StationClient::stationVaxAvailabilityChanged, this,
            &MainWindow::refreshRemoteStationVax);
    refreshRemoteStationVax();
}

void MainWindow::refreshRemoteStationVax()
{
    if (m_vaxApplet == nullptr || m_stationClient == nullptr) {
        return;
    }
    m_vaxApplet->setStationVax(m_stationClient->stationVax(),
                               m_stationClient->stationVaxHeld());
    m_stationClient->setStationVaxLevelsWanted(m_vaxApplet->stationLevelsWanted());
}

void MainWindow::wireRemoteTransmitMeters()
{
    // iPhone app plan Task 39 (D14, R-IOS-13): the remote window shows the
    // transmit meters the local window shows, by the same meter items, from
    // the Core's `txState` (MeterPoller::setRemoteTransmitState: power,
    // reflected power and SWR through this window's RadioStatus, which the
    // S-meter's TX needle, the TX applet's power gauge and the container
    // meters already follow; ALC and MIC through the poller). A meter the
    // Core does not send is shown disabled with the reason, never hidden.
    TransmitState* state = m_stationClient ? m_stationClient->transmitState() : nullptr;
    if (state == nullptr || m_radioModel == nullptr) {
        return;
    }
    if (m_meterPoller) {
        // Parity Task 33 follow-up: the COMP reading, txReadingsVersion 1.
        m_meterPoller->setRemoteTxReadingsAvailable([this]() {
            return m_stationClient != nullptr && m_stationClient->txReadingsAvailable();
        });
        // A9 (iPhone app plan Task 39): the seven container stage meters,
        // txReadingsVersion 3.
        m_meterPoller->setRemoteTxStageReadingsAvailable([this]() {
            return m_stationClient != nullptr && m_stationClient->txStageReadingsAvailable();
        });
        m_meterPoller->setRemoteTransmitState(state, [this]() -> QString {
            if (m_stationClient == nullptr || !m_stationClient->isHandshakeComplete()) {
                return tr("Connect to the Core to see transmit meters here.");
            }
            if (m_stationClient->capabilities().txStateVersion < 1) {
                return tr("This Core does not send transmit meters. Update the Core to see "
                          "them here.");
            }
            return {};
        });
    }
    // The S-meter's TX needle follows the Core's keyed state (the local
    // window's follows MoxController's walk).
    connect(state, &TransmitState::stateChanged, this, [this, state]() {
        if (SMeterWidget* sm = m_appletPanel ? m_appletPanel->smeterWidget() : nullptr) {
            sm->setTransmitting(state->keyed());
        }
    });
    // Parity Task 33 follow-up: the compression gauge reads the Core's COMP
    // reading (txState's compressionDb) from a Core at txReadingsVersion 1.
    const auto showCompressionReason = [this]() {
        if (!m_phoneCwApplet || !m_stationClient) {
            return;
        }
        if (m_stationClient->txReadingsAvailable()) {
            m_phoneCwApplet->setCompressionUnavailable(QString());
        } else if (m_stationClient->isHandshakeComplete()) {
            m_phoneCwApplet->setCompressionUnavailable(TransmitState::txReadingNotSentText());
        } else {
            m_phoneCwApplet->setCompressionUnavailable(
                tr("Connect to the Core to see transmit meters here."));
        }
    };
    connect(m_stationClient, &StationClient::handshakeComplete, this, showCompressionReason);
    connect(m_stationClient, &StationClient::stateSnapshotApplied, this, showCompressionReason);
    showCompressionReason();
    // R-R3-49 (parity Task 33): the CFC dialog's bar chart reads the Core's
    // txCfcCompression stream while it is shown; a Core that does not keep
    // it (txReadingsVersion 0) is named as the reason.
    if (m_txApplet) {
        m_txApplet->setStationCfcBarChart([this](bool wanted) {
            if (m_stationClient) {
                m_stationClient->setCfcCompressionWanted(wanted);
            }
        });
        connect(m_stationClient, &StationClient::cfcCompressionReceived, m_txApplet,
                [this](const QList<double>& bins, qint64) {
                    if (m_txApplet) {
                        m_txApplet->applyStationCfcCompression(bins);
                    }
                });
        const auto showCfcReason = [this]() {
            if (m_txApplet && m_stationClient) {
                m_txApplet->setStationCfcBarChartUnavailable(
                    m_stationClient->isHandshakeComplete() && !m_stationClient->txReadingsAvailable()
                        ? TransmitState::txReadingNotSentText()
                        : QString());
            }
        };
        connect(m_stationClient, &StationClient::handshakeComplete, this, showCfcReason);
        connect(m_stationClient, &StationClient::stateSnapshotApplied, this, showCfcReason);
        showCfcReason();
    }
    // Fix wave I4: who holds transmit on the Core, under MOX and TUNE.
    const auto showHolder = [this]() {
        if (m_txApplet && m_stationClient) {
            m_txApplet->setTransmitHolderText(m_stationClient->transmitHolderText());
        }
    };
    connect(state, &TransmitState::holderChanged, this, showHolder);
    // iPhone app plan Task 77: every transmit control follows the holder.
    connect(state, &TransmitState::holderChanged, this, &MainWindow::applyRemoteRoleGating);
    connect(m_stationClient, &StationClient::handshakeComplete, this, showHolder);
    showHolder();
    // Slice control plan Task 11 (U8): the TX applet follows the slice the
    // Core marks for transmit, offers a letter per slice this window
    // controls, and a press asks the Core with tx.setTxSlice (the Core
    // drops MOX before it moves transmit, ruling 8.10).
    if (m_txApplet) {
        m_txApplet->setTransmitSliceResolver([this]() -> SliceModel* {
            if (!m_radioModel) { return nullptr; }
            for (SliceModel* slice : m_radioModel->slices()) {
                if (slice && slice->isTxSlice()) { return slice; }
            }
            return m_radioModel->activeSlice();
        });
        m_txApplet->setTransmitSliceChoices(
            [this](int id) {
                SliceAccessMirror* access = m_stationClient ? m_stationClient->sliceAccess()
                                                            : nullptr;
                return !access || !access->entry(id).has_value() || access->controlledHere(id);
            },
            [this](int id) { activateTransmitSlice(id, true); },
            {},
            [this](int id) {
                const TxSliceAction action = txSliceAction(id);
                return TxApplet::TransmitSliceChoice{action.enabled, action.effectiveWords};
            });
        connect(state, &TransmitState::holderChanged, m_txApplet,
                &TxApplet::refreshTransmitSliceChoices);
        connect(m_stationClient, &StationClient::handshakeComplete, m_txApplet,
                &TxApplet::refreshTransmitSliceChoices);
        if (SliceAccessMirror* access = m_stationClient->sliceAccess()) {
            connect(access, &SliceAccessMirror::changed, m_txApplet,
                    [this](int) {
                        if (m_txApplet) { m_txApplet->refreshTransmitSliceChoices(); }
                        continueTxBadgeTake();
                    });
        }
    }
}

void MainWindow::wireRemoteDevices()
{
    // iPhone app plan Task 78 (R-IOS-02, R-IOS-07, R-IOS-30; the
    // several-devices design, section 12).
    if (m_stationClient == nullptr || m_multiDevice != nullptr) {
        return;
    }
    m_multiDevice = new MultiDeviceController(m_stationClient, this, this);
    connect(m_multiDevice, &MultiDeviceController::refusal, this, [this](const QString& reason) {
        // TX badge take: a take of transmit refused changes nothing.
        if (m_txBadgeTakeStage == TxBadgeStage::Transmit) { abandonTxBadgeTake(); }
        showToast(OperatorReasonText::forDisplay(reason), ToastSeverity::Warning, 5000);
    });
    connect(m_multiDevice, &MultiDeviceController::transmitTaken, this, [this]() {
        showToast(tr("This window has transmit. Press MOX to transmit."), ToastSeverity::Info,
                  4000);
    });
    connect(m_multiDevice, &MultiDeviceController::markersChanged, this,
            &MainWindow::refreshForeignMarkers);
    if (TransmitState* tx = m_stationClient->transmitState()) {
        connect(tx, &TransmitState::holderChanged, this, &MainWindow::refreshRemoteDeviceScreens);
        connect(tx, &TransmitState::stateChanged, this, &MainWindow::refreshRemoteDeviceScreens);
    }
    connect(m_stationClient, &StationClient::transmitTakeAvailabilityChanged, this,
            &MainWindow::refreshRemoteDeviceScreens);
    connect(m_stationClient, &StationClient::sessionEnded, this, [this]() {
        m_hadSliceThisSession = false;
        // TX badge take: a take does not outlive the link.
        abandonTxBadgeTake();
        refreshRemoteDeviceScreens();
    });
    if (m_txApplet) {
        connect(m_txApplet, &TxApplet::takeTransmitRequested, m_multiDevice,
                &MultiDeviceController::askTakeTransmit);
    }
    // The empty band's offer of a take follows this window's slices.
    connect(m_radioModel, &RadioModel::sliceAdded, this, [this]() {
        m_hadSliceThisSession = true;
        refreshTakeReceiverOffer();
    });
    connect(m_radioModel, &RadioModel::sliceRemoved, this,
            &MainWindow::refreshTakeReceiverOffer);
    if (m_panStack) {
        connect(m_panStack, &PanadapterStack::activePanChanged, this,
                &MainWindow::refreshRemoteDeviceScreens);
    }
    refreshRemoteDeviceScreens();
}

void MainWindow::refreshForeignMarkers()
{
    if (m_panStack == nullptr) { return; }
    QVector<SpectrumWidget::ForeignSliceMarker> markers;
    const bool hosting = desktopHosting();
    StationServer* server = hosting ? m_desktopStationController->server() : nullptr;
    SliceOwnership* ownership = hosting && m_radioModel ? m_radioModel->sliceOwnership() : nullptr;
    if (server && ownership && m_radioModel) {
        for (SliceModel* slice : m_radioModel->slices()) {
            if (!slice) { continue; }
            const SliceOwnership::Mark mark = ownership->mark(slice->sliceIndex());
            const QByteArray subject = mark.subject();
            if (subject.isEmpty() || subject == SliceOwnership::stationDevice()) { continue; }
            // Task 14a: a slice this window listens to has its own flag.
            if (desktopListensTo(slice->sliceIndex())) { continue; }
            SpectrumWidget::ForeignSliceMarker marker;
            marker.sliceId = slice->sliceIndex();
            marker.centreHz = slice->frequency();
            marker.filterLowHz = slice->filterLow();
            marker.filterHighHz = slice->filterHigh();
            marker.color = VfoWidget::sliceColor(marker.sliceId);
            marker.letter = slice->sliceLetter();
            if (const auto words = server->connectedDevices()->describe(subject)) {
                marker.ownerName = words->name;
                marker.ownerShortName = words->shortName;
            }
            const auto device = server->deviceSessions()->entry(subject);
            marker.away = mark.isHeld()
                || (device && device->state == DeviceSessionRegistry::State::Away);
            marker.tx = slice->txSliceMarked();
            markers.append(marker);
        }
    } else if (m_stationClient) {
        // A marker for a slice the station device holds names no device;
        // its access entry says so, and the hosting desktop is named.
        markers = MultiDeviceController::foreignMarkers(*m_stationClient->remoteDevices(),
                                                        m_stationClient->sliceAccess());
    }
    for (PanadapterApplet* applet : m_panStack->allApplets()) {
        if (applet && applet->spectrumWidget()) {
            SpectrumWidget* spectrum = applet->spectrumWidget();
            spectrum->setForeignSliceMarkers(markers);
            bool hasOwnSlice = !hosting;
            if (hosting && ownership && m_radioModel) {
                for (SliceModel* slice : m_radioModel->slices()) {
                    if (slice && (desktopSliceAllowed(slice->sliceIndex())
                                  || desktopListensTo(slice->sliceIndex()))
                        && spectrumForSlice(slice) == spectrum) {
                        hasOwnSlice = true;
                        break;
                    }
                }
            }
            spectrum->setOwnSliceMarkerPresentationAllowed(hasOwnSlice);
        }
    }
}

// ── Slice control plan Task 13: the bottom RX area's slice chooser ──────

void MainWindow::ensureSliceChooser()
{
    if (!m_sliceChooser) {
        m_sliceChooser = new SliceChooser(this);
        m_sliceChooser->setWindowFlag(Qt::Popup, true);
        connect(m_sliceChooser, &SliceChooser::closeRequested, m_sliceChooser, &QWidget::hide);
        connect(m_sliceChooser, &SliceChooser::listenRequested, this,
                [this](int id) { runSliceChooserAction(SliceChooserAction::Listen, id); });
        connect(m_sliceChooser, &SliceChooser::takeControlRequested, this,
                [this](int id) { runSliceChooserAction(SliceChooserAction::TakeControl, id); });
        connect(m_sliceChooser, &SliceChooser::releaseRequested, this,
                [this](int id) { runSliceChooserAction(SliceChooserAction::Release, id); });
        connect(m_sliceChooser, &SliceChooser::stopListeningRequested, this,
                [this](int id) { runSliceChooserAction(SliceChooserAction::StopListening, id); });
        connect(m_sliceChooser, &SliceChooser::selectRequested, this,
                [this](int id) { runSliceChooserAction(SliceChooserAction::Select, id); });
        connect(m_sliceChooser, &SliceChooser::newSliceRequested, this,
                [this]() { runSliceChooserAction(SliceChooserAction::NewSlice, -1); });
        // Task 14a: a flag that sent the request shows the answer too.
        connect(m_sliceChooser, &SliceChooser::resultShown, this,
                [this](const QString& words) {
            const int id = m_flagRequestSlice;
            if (id < 0) { return; }
            m_flagRequestSlice = -1;
            if (VfoWidget* flag = m_vfoWidgetsBySlice.value(id)) {
                flag->setSliceAccessPending(QString());
            }
            if (m_rxApplet) { m_rxApplet->setSliceAccessPending(QString()); }
            if (m_txApplet) { m_txApplet->refreshTransmitSliceChoices(); }
            if (!words.isEmpty()) { showToast(words, ToastSeverity::Info, 4000); }
        });
    }
}

void MainWindow::openSliceChooser()
{
    if (!m_rxDashboard || !m_radioModel) {
        return;
    }
    ensureSliceChooser();
    refreshSliceChooser();
    // A wait left from a request that is no longer in flight is cleared.
    m_sliceChooser->reopened();
    m_sliceChooser->adjustSize();
    // Above the picker, inside the window.
    QWidget* picker = m_rxDashboard->chooserButton();
    const QPoint anchor = picker->mapToGlobal(QPoint(0, 0));
    const QSize size = m_sliceChooser->sizeHint().expandedTo(QSize(360, 0));
    m_sliceChooser->resize(size);
    m_sliceChooser->move(anchor.x(), anchor.y() - size.height() - 4);
    m_sliceChooser->show();
    m_sliceChooser->raise();
}

void MainWindow::refreshSliceChooser()
{
    if (!m_radioModel) {
        return;
    }
    QList<SliceChooser::Row> rows;
    bool shared = false;
    StationServer* server = desktopHosting() && m_desktopStationController
        ? m_desktopStationController->server() : nullptr;
    if (server) {
        rows = SliceChooser::rowsForHostingDesktop(*m_radioModel, *server);
        shared = true;
    } else if (m_stationClient && m_stationClient->remoteDevices()) {
        rows = SliceChooser::rowsForRemoteWindow(*m_radioModel, m_stationClient->sliceAccess(),
                                                 *m_stationClient->remoteDevices());
        shared = m_stationClient->remoteSliceAccessAvailable();
    } else {
        rows = SliceChooser::rowsForRemoteWindow(*m_radioModel, nullptr, RemoteDevicesState());
    }
    if (m_rxDashboard && shared) {
        m_rxDashboard->setChooserState(SliceChooser::bannerState(rows));
    }
    if (m_sliceChooser) {
        m_sliceChooser->setInventory(rows);
        m_sliceChooser->setNoRoomForNewSlice(
            m_radioModel->slices().size() >= m_radioModel->sliceCapForDevices());
    }
    // Task 14b: the hosting desktop's own listening level changes (set
    // here, or reset by the Core when a slice is re-made) refresh its flags.
    if (server && server->sliceAccessController()
        && m_listenLevelSource != server->sliceAccessController()) {
        SliceAccessController* levels = server->sliceAccessController();
        m_listenLevelSource = levels;
        connect(levels, &SliceAccessController::listenLevelChanged, this,
                [this](int sliceId, const QByteArray& device) {
            if (device != SliceOwnership::stationDevice()) { return; }
            if (VfoWidget* flag = m_vfoWidgetsBySlice.value(sliceId)) {
                const std::pair<int, bool> volume = listenVolumeFor(sliceId);
                flag->setListenVolume(volume.first, volume.second);
            }
        });
    }
    // Slice control plan Task 14a: each flag says who controls its slice
    // (Unshared everywhere when the Core does not share slices).
    for (auto it = m_vfoWidgetsBySlice.constBegin(); it != m_vfoWidgetsBySlice.constEnd(); ++it) {
        VfoWidget* flag = it.value();
        if (!flag) { continue; }
        // Task 14b: the level "Your volume" shows, set before the access so
        // a flag that starts listening shows it at once.
        if (shared) {
            const std::pair<int, bool> volume = listenVolumeFor(it.key());
            flag->setListenVolume(volume.first, volume.second);
        }
        VfoWidget::SliceAccess access;
        if (shared) {
            for (const SliceChooser::Row& row : rows) {
                if (row.sliceId == it.key()) {
                    access = SliceChooser::flagAccessFor(row);
                    break;
                }
            }
        }
        flag->setSliceAccess(access);
        // TX badge take: what the badge offers follows the slice's access.
        applyFlagTransmitGate(flag);
    }
    // Slice control plan Task 15: the RX applet's tabs say the same.
    refreshRxAppletSlices();
}

void MainWindow::runFlagAccessAction(SliceChooserAction action, int sliceId)
{
    if (!m_radioModel) { return; }
    ensureSliceChooser();
    if (!m_sliceChooser->requestInFlight().isEmpty()) {
        showToast(tr("The Core has not answered the last slice request."),
                  ToastSeverity::Info, 3000);
        return;
    }
    // Pending before the call: the hosting desktop's answer comes at once,
    // inside runSliceChooserAction, through resultShown.
    VfoWidget* flag = m_vfoWidgetsBySlice.value(sliceId);
    m_flagRequestSlice = sliceId;
    if (flag) { flag->setSliceAccessPending(tr("Asking the Core…")); }
    // Task 15: the RX applet's tab menu sends the same requests and waits
    // with the flag.
    if (m_rxApplet) { m_rxApplet->setSliceAccessPending(tr("Asking the Core…")); }
    runSliceChooserAction(action, sliceId);
    if (m_flagRequestSlice == sliceId && !m_sliceChooser->isPending()) {
        // Nothing was sent and nothing answered: do not leave it waiting.
        m_flagRequestSlice = -1;
        if (flag) { flag->setSliceAccessPending(QString()); }
        if (m_rxApplet) { m_rxApplet->setSliceAccessPending(QString()); }
    }
    if (m_txApplet) { m_txApplet->refreshTransmitSliceChoices(); }
}

std::pair<int, bool> MainWindow::listenVolumeFor(int sliceId)
{
    StationServer* server = desktopHosting() && m_desktopStationController
        ? m_desktopStationController->server() : nullptr;
    if (server && server->sliceAccessController()) {
        const SliceAccessController::ListenLevel level =
            server->sliceAccessController()->listenLevel(SliceOwnership::stationDevice(),
                                                         sliceId);
        return { static_cast<int>(std::lround(std::clamp(level.level, 0.0, 1.0) * 100.0)),
                 level.muted };
    }
    const std::optional<SliceAccessMirror::Entry> entry =
        m_stationClient && m_stationClient->sliceAccess()
            ? m_stationClient->sliceAccess()->entry(sliceId) : std::nullopt;
    if (!entry) {
        return { 100, false };
    }
    auto found = m_remoteListenVolumes.find(sliceId);
    if (found == m_remoteListenVolumes.end() || found->incarnation != entry->incarnation) {
        // The Core seeds a new listener's level from the slice's AF.
        RemoteListenVolume seeded;
        seeded.incarnation = entry->incarnation;
        const SliceModel* slice = m_radioModel ? m_radioModel->sliceById(sliceId) : nullptr;
        seeded.level = slice ? std::clamp(slice->afGain(), 0, 100) : 100;
        found = m_remoteListenVolumes.insert(sliceId, seeded);
    }
    return { found->level, found->muted };
}

void MainWindow::setFlagListenVolume(int sliceId, int level, bool muted)
{
    // Slice control plan Task 14b (ruling U5): this device's own level and
    // mute for a slice it listens to. The slice's AF and mute, the
    // controller's audio and every other listener's audio are untouched.
    const int clamped = std::clamp(level, 0, 100);
    // The hosting desktop (Task 10): slice.setListenLevel as the station
    // device; a refusal is toasted by the refused() wiring.
    if (HostingSliceActions* actions = hostingSlices()) {
        actions->setListenLevel(sliceId, clamped / 100.0, muted);
        return;
    }
    if (!m_stationClient || !m_stationClient->sliceAccess()) {
        return;
    }
    const std::optional<SliceAccessMirror::Entry> entry =
        m_stationClient->sliceAccess()->entry(sliceId);
    const quint64 incarnation = entry ? entry->incarnation : 0;
    const IStationLink::CommandOutcome outcome =
        m_stationClient->requestListenLevel(sliceId, incarnation, clamped / 100.0, muted);
    if (!outcome.sent) {
        showToast(outcome.reason, ToastSeverity::Info, 4000);
        return;
    }
    RemoteListenVolume held;
    held.incarnation = incarnation;
    held.level = clamped;
    held.muted = muted;
    m_remoteListenVolumes.insert(sliceId, held);
}

HostingSliceActions* MainWindow::hostingSlices() const
{
    if (!desktopHosting() || !m_hostingSlices || !m_desktopBoundServer
        || m_desktopStationController->server() != m_desktopBoundServer) {
        return nullptr;
    }
    return m_hostingSlices.get();
}

void MainWindow::wireHostingSlices()
{
    HostingSliceActions* actions = m_hostingSlices.get();
    if (!actions) { return; }
    connect(actions, &HostingSliceActions::refused, this, [this](const QString& reason) {
        // The chooser (and a flag's access row, which runs through it)
        // shows its own request's answer.
        if (m_sliceChooser && !m_sliceChooser->requestInFlight().isEmpty()) { return; }
        showToast(OperatorReasonText::forDisplay(reason), ToastSeverity::Info, 4000);
    });
    connect(actions, &HostingSliceActions::question, this, &MainWindow::showHostingQuestion);
    connect(actions, &HostingSliceActions::notice, this, &MainWindow::showHostingNotice);
    // Take-over fix wave (M-3): a controlTaken card goes when control came
    // back or never can now; it stays when Take it back may be tried again.
    connect(actions, &HostingSliceActions::takeBackAnswered, this, [this](qint64 id, bool ended) {
        if (!ended) { return; }
        m_hostingNoticeCards.removeIf([id](const QPointer<NoticeCard>& card) {
            if (card && card->noticeId() == id) {
                card->hide();
                card->deleteLater();
                return true;
            }
            return false;
        });
        layoutHostingNoticeCards();
    });
    connect(actions, &HostingSliceActions::finished, this,
            [this](const QByteArray& verb, int sliceId, bool accepted, const QString& reason) {
        const QPointer<MainWindow> self(this);
        finishSliceChooserRequest(verb == QByteArrayLiteral("addSliceOnPan")
                                      ? QByteArrayLiteral("addSlice") : verb,
                                  accepted, reason);
        if (!self || verb == QByteArrayLiteral("setActiveSliceById")) { return; }
        // Slice control plan Task 16 (ruling U1): a slice the station
        // device now listens to or controls is shown in this window.
        if (accepted && sliceId >= 0
            && (verb == QByteArrayLiteral("slice.listen")
                || verb == QByteArrayLiteral("slice.takeControl"))) {
            revealSliceInWindow(sliceId);
        }
        refreshForeignMarkers();
        refreshDesktopStationState();
        if (!self) { return; }
        // TX badge take: the slice is taken; transmit is next.
        if (verb == QByteArrayLiteral("slice.takeControl")) {
            txBadgeSliceAnswered(sliceId, accepted);
        }
    });
}

void MainWindow::dropHostingSliceActionsForTest()
{
    m_hostingSlices.reset();
}

bool MainWindow::selectSliceForWindow(int sliceId)
{
    if (!m_radioModel) { return false; }
    if (HostingSliceActions* actions = hostingSlices()) {
        // Answered before select() returns (tst_hosting_slice_operations).
        bool accepted = false;
        const QMetaObject::Connection answer = connect(actions, &HostingSliceActions::finished,
            this, [&accepted](const QByteArray& verb, int, bool ok, const QString&) {
                if (verb == QByteArrayLiteral("setActiveSliceById")) { accepted = ok; }
            });
        actions->select(sliceId);
        disconnect(answer);
        // Slice control plan Task 16 (ruling U2): the selected slice's pan
        // comes forward, a floating one included.
        if (accepted) { revealSliceInWindow(sliceId); }
        return accepted;
    }
    if (desktopHosting()) {
        // Slice control plan Task 15 fix round 1: the station device's
        // select as the hosting path runs it (SliceAccessController::
        // selectRx): any live slice it listens to becomes its RX, and one
        // it may change also its active slice.
        SliceOwnership* ownership = m_radioModel->sliceOwnership();
        if (!ownership || !ownership->isLive(sliceId)
            || !ownership->isListening(SliceOwnership::stationDevice(), sliceId)) {
            return false;
        }
        const bool selected =
            m_radioModel->setActiveRxFor(SliceOwnership::stationDevice(), sliceId);
        if (selected) { revealSliceInWindow(sliceId); }
        return selected;
    }
    const bool selected = m_radioModel->setActiveSliceById(sliceId);
    if (selected) { revealSliceInWindow(sliceId); }
    return selected;
}

void MainWindow::addSliceForWindow(const QString& panId)
{
    if (!m_radioModel) { return; }
    if (HostingSliceActions* actions = hostingSlices()) {
        actions->addOnPan(panId);
        return;
    }
    m_radioModel->addSliceOnPan(panId);
}

void MainWindow::closeSliceForWindow(int sliceId)
{
    if (!m_radioModel) { return; }
    if (HostingSliceActions* actions = hostingSlices()) {
        actions->close(sliceId);
        return;
    }
    m_radioModel->removeSlice(sliceId);
}

void MainWindow::showHostingQuestion(const SessionMessage& question)
{
    if (m_hostingQuestionDialog) {
        // A newer question replaces the one still open.
        QDialog* older = m_hostingQuestionDialog;
        m_hostingQuestionDialog = nullptr;
        older->close();
    }
    auto choice = std::make_shared<std::function<qint64()>>();
    QDialog* dialog = MultiDeviceController::questionDialog(question.prompt, this, choice.get());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    m_hostingQuestionDialog = dialog;
    const qint64 id = question.prompt.id;
    const QPointer<QDialog> shown(dialog);
    connect(dialog, &QDialog::accepted, this, [this, shown, id, choice]() {
        if (m_hostingQuestionDialog != shown) { return; }
        m_hostingQuestionDialog = nullptr;
        const qint64 picked = (*choice)();
        if (HostingSliceActions* actions = hostingSlices()) { actions->proceed(id, picked); }
    });
    connect(dialog, &QDialog::rejected, this, [this, shown, id]() {
        if (m_hostingQuestionDialog != shown) { return; }
        m_hostingQuestionDialog = nullptr;
        if (HostingSliceActions* actions = hostingSlices()) { actions->cancel(id); }
    });
    dialog->open();
}

void MainWindow::showHostingNotice(const SessionMessage& notice)
{
    QWidget* host = m_panStack ? m_panStack->panadapter(m_panStack->activePanId()) : nullptr;
    if (!host) {
        if (!notice.reason.isEmpty()) {
            showToast(OperatorReasonText::forDisplay(notice.reason), ToastSeverity::Info, 5000);
        }
        return;
    }
    RemotePrompt prompt;
    prompt.prompt = notice.prompt;
    prompt.reason = notice.reason;
    prompt.receivedAt = QDateTime::currentDateTime();
    // Take-over parity: its own Core always runs Take it back for
    // controlTaken; a card that does not offer it shows it off with why.
    auto* card = new NoticeCard(prompt, host, MultiDeviceController::controlTakeBackOff(prompt, true));
    const QPointer<NoticeCard> guard(card);
    // Take-over fix wave (M-3): a controlTaken card waits for the answer
    // (takeBackAnswered); any other goes at the tap.
    const bool waitsForAnswer = prompt.prompt.kind == QStringLiteral("controlTaken");
    connect(card, &NoticeCard::takeBackRequested, this, [this, guard, waitsForAnswer](qint64 id) {
        if (!waitsForAnswer) {
            m_hostingNoticeCards.removeAll(guard);
            if (guard) { guard->hide(); guard->deleteLater(); }
            layoutHostingNoticeCards();
        }
        if (HostingSliceActions* actions = hostingSlices()) { actions->takeBack(id); }
    });
    connect(card, &NoticeCard::dismissed, this, [this, guard](qint64 id) {
        // Take-over re-review (N-3): its Take it back record goes with it.
        if (HostingSliceActions* actions = hostingSlices()) { actions->forgetNotice(id); }
        m_hostingNoticeCards.removeAll(guard);
        if (guard) { guard->hide(); guard->deleteLater(); }
        layoutHostingNoticeCards();
    });
    m_hostingNoticeCards.append(guard);
    layoutHostingNoticeCards();
}

void MainWindow::layoutHostingNoticeCards()
{
    m_hostingNoticeCards.removeAll(QPointer<NoticeCard>());
    QHash<QWidget*, QList<NoticeCard*>> byHost;
    for (const QPointer<NoticeCard>& card : std::as_const(m_hostingNoticeCards)) {
        byHost[card->parentWidget()].append(card.data());
    }
    for (auto it = byHost.cbegin(); it != byHost.cend(); ++it) {
        MultiDeviceController::stackNoticeCards(it.key(), it.value());
    }
}

void MainWindow::finishSliceChooserRequest(const QByteArray& verb, bool accepted,
                                           const QString& reason)
{
    if (!m_sliceChooser || m_sliceChooser->requestInFlight() != verb) {
        return;
    }
    refreshSliceChooser();
    m_sliceChooser->finishRequest(verb, accepted, reason);
}

void MainWindow::runSliceChooserAction(SliceChooserAction action, int sliceId)
{
    if (!m_radioModel || !m_sliceChooser || !m_sliceChooser->requestInFlight().isEmpty()) {
        return;
    }
    const QString letter = QString(QChar(QLatin1Char('A').unicode() + std::max(0, sliceId)));
    if (action == SliceChooserAction::Select) {
        const bool asked = selectSliceForWindow(sliceId);
        refreshSliceChooser();
        m_sliceChooser->showResult(asked
            ? tr("The bottom RX area now follows slice %1.").arg(letter)
            : tr("Slice %1 cannot be selected here now.").arg(letter));
        return;
    }
    if (action == SliceChooserAction::NewSlice) {
        m_sliceChooser->beginRequest(QByteArrayLiteral("addSlice"),
                                     tr("Asking the Core for a new slice…"),
                                     tr("A new slice is ready."));
        if (m_panStack) {
            addSliceForWindow(m_panStack->activePanId());
        } else {
            m_radioModel->addSlice(QString());
        }
        return;
    }
    QString success;
    switch (action) {
    case SliceChooserAction::Listen:
        success = tr("Listening to slice %1.").arg(letter);
        break;
    case SliceChooserAction::TakeControl:
        // TX badge take: the badge goes on to take transmit, and that
        // take's own outcome speaks.
        if (m_txBadgeTakeStage != TxBadgeStage::Slice || m_txBadgeTakeSlice != sliceId) {
            success = tr("You control slice %1. Tuning is unchanged. Select transmit separately.")
                          .arg(letter);
        }
        break;
    case SliceChooserAction::Release:
        success = tr("You released slice %1.").arg(letter);
        break;
    case SliceChooserAction::StopListening:
        success = tr("You stopped listening to slice %1.").arg(letter);
        break;
    default:
        break;
    }
    // The hosting desktop (Task 10): the station device's own requests, run
    // through the same dispatcher, checks and slice access a remote device's
    // take. The answer arrives on HostingSliceActions::finished, which
    // completes the request.
    if (HostingSliceActions* actions = hostingSlices()) {
        QByteArray verb;
        switch (action) {
        case SliceChooserAction::Listen:        verb = QByteArrayLiteral("slice.listen"); break;
        case SliceChooserAction::TakeControl:   verb = QByteArrayLiteral("slice.takeControl"); break;
        case SliceChooserAction::Release:       verb = QByteArrayLiteral("slice.release"); break;
        case SliceChooserAction::StopListening: verb = QByteArrayLiteral("slice.stopListening"); break;
        default: break;
        }
        if (verb.isEmpty()) { return; }
        m_sliceChooser->beginRequest(verb, tr("Asking the Core…"), success);
        switch (action) {
        case SliceChooserAction::Listen:        actions->listen(sliceId); break;
        case SliceChooserAction::TakeControl:   actions->takeControl(sliceId); break;
        case SliceChooserAction::Release:       actions->release(sliceId); break;
        case SliceChooserAction::StopListening: actions->stopListening(sliceId); break;
        default: break;
        }
        return;
    }
    // A remote window: the Core's slice verbs; the answer comes back on
    // deviceCommandFinished.
    if (!m_stationClient) {
        // A window on its own: its slices are all its own.
        if (action == SliceChooserAction::Release) {
            m_radioModel->removeSlice(sliceId);
            refreshSliceChooser();
            m_sliceChooser->showResult(success);
        }
        return;
    }
    const std::optional<SliceAccessMirror::Entry> entry =
        m_stationClient->sliceAccess() ? m_stationClient->sliceAccess()->entry(sliceId)
                                       : std::nullopt;
    IStationLink::CommandOutcome outcome;
    QByteArray verb;
    const quint64 incarnation = entry ? entry->incarnation : 0;
    const quint64 revision = entry ? entry->controlRevision : 0;
    switch (action) {
    case SliceChooserAction::Listen:
        verb = QByteArrayLiteral("slice.listen");
        outcome = m_stationClient->requestListen(sliceId, incarnation);
        break;
    case SliceChooserAction::TakeControl:
        verb = QByteArrayLiteral("slice.takeControl");
        outcome = m_stationClient->requestTakeControl(sliceId, incarnation, revision);
        break;
    case SliceChooserAction::Release:
        verb = QByteArrayLiteral("slice.release");
        outcome = m_stationClient->requestRelease(sliceId, incarnation, revision);
        break;
    case SliceChooserAction::StopListening:
        verb = QByteArrayLiteral("slice.stopListening");
        outcome = m_stationClient->requestStopListening(sliceId, incarnation);
        break;
    default:
        break;
    }
    if (!outcome.sent) {
        refreshSliceChooser();
        m_sliceChooser->showResult(outcome.reason);
        return;
    }
    m_sliceChooser->beginRequest(verb, tr("Asking the Core…"), success);
    if (action == SliceChooserAction::Listen || action == SliceChooserAction::TakeControl) {
        m_pendingRevealSlice = sliceId;
    }
}

void MainWindow::refreshTakeReceiverOffer()
{
    if (m_panStack == nullptr || m_radioModel == nullptr) {
        return;
    }
    // Only after the Core closed this window's last slice (a take): an
    // empty window that never had one keeps the +RX hint.
    const bool offer = m_stationClient != nullptr && m_stationClient->sessionHolderAvailable()
        && m_hadSliceThisSession && m_radioModel->slices().isEmpty();
    for (PanadapterApplet* applet : m_panStack->allApplets()) {
        if (applet) {
            applet->setTakeReceiverOffered(offer);
        }
    }
}

void MainWindow::refreshRemoteDeviceScreens()
{
    if (m_stationClient == nullptr) {
        return;
    }
    const TransmitState& tx = *m_stationClient->transmitState();
    const bool known = m_stationClient->knowsTransmitHolder();
    const bool elsewhere = m_stationClient->transmitHeldElsewhere();
    const DeviceWords::HolderBadge badge =
        known ? DeviceWords::holderBadge(tx, m_stationClient->thisDeviceWireId())
              : DeviceWords::HolderBadge{};

    // The bottom banner: who holds transmit, beside TX.
    if (m_txHolderChip) {
        m_txHolderChip->setLabel(badge.label);
        switch (badge.tone) {
        case DeviceWords::HolderBadge::Tone::OnAir:
            m_txHolderChip->setVariant(StatusBadge::Variant::Tx);
            break;
        case DeviceWords::HolderBadge::Tone::Away:
            m_txHolderChip->setVariant(StatusBadge::Variant::Warn);
            break;
        case DeviceWords::HolderBadge::Tone::ChangingHands:
        case DeviceWords::HolderBadge::Tone::Listening:
            m_txHolderChip->setVariant(StatusBadge::Variant::Info);
            break;
        }
        m_txHolderChip->setToolTip(badge.toolTip);
        m_txHolderChip->setVisible(badge.shown);
        if (m_chromeBar && m_safetyGroup && m_chromeBarWidget) {
            m_chromeBar->setNaturalWidth(m_safetyGroup, m_safetyGroup->sizeHint().width());
            m_chromeBar->relayout(m_chromeBarWidget->width());
        }
    }

    // Take transmit: the TX applet's button and every pan's TX pill.
    const QString holderName = badge.shown ? badge.label : QString();
    if (m_txApplet) {
        m_txApplet->setTakeTransmitOffered(elsewhere, elsewhere && tx.keyed());
    }
    if (m_panStack) {
        for (PanadapterApplet* applet : m_panStack->allApplets()) {
            if (applet) {
                applet->setTakeTransmitOffered(elsewhere, holderName, elsewhere && tx.keyed());
            }
        }
    }

    // Ruling 8.11: a slice the radio's own PTT transmits on.
    const bool radioKeyed = known && tx.keyed()
        && tx.holderSource() == QStringLiteral("radioPtt");
    for (auto it = m_vfoWidgetsBySlice.constBegin(); it != m_vfoWidgetsBySlice.constEnd(); ++it) {
        if (VfoWidget* flag = it.value()) {
            flag->setInUseByRadio(radioKeyed && it.key() == tx.txSliceId());
        }
    }

    // Notices sit on the active pan's band.
    if (m_multiDevice && m_panStack) {
        m_multiDevice->setNoticeHost(m_panStack->panadapter(m_panStack->activePanId()));
    }
    refreshTakeReceiverOffer();
    // TX badge take: each flag's badge follows who holds transmit, and a
    // take the badge started ends on its slice once transmit is here.
    refreshFlagTransmitGates();
    continueTxBadgeTake();
}

// Parity Task 31 (A11, R-R3-49): display duplex (DUP). The window's
// setting DisplayDuplex, changed from View > Display duplex (DUP) or the
// container DUP button, saved at once.
void MainWindow::setDisplayDuplexSetting(bool on)
{
    if (m_displayDuplexSetting == on) {
        return;
    }
    m_displayDuplexSetting = on;
    MoxDisplayController::saveDisplayDuplex(on);
    applyDisplayDuplex();
}

// The setting to what the pan does (MoxDisplayController: none in a remote
// window whose Core is below txDisplayVersion 3), to this window's own
// noise blanking rule (RadioModel, a window running its own DSP), and to
// the two controls: checked from the setting, disabled with the reason
// when DUP cannot apply.
void MainWindow::applyDisplayDuplex()
{
    if (m_moxDisplay) {
        m_moxDisplay->setDisplayDuplex(m_displayDuplexSetting);
    }
    if (m_radioModel) {
        m_radioModel->setLocalDisplayDuplex(m_displayDuplexSetting);
    }
    const QString reason = MoxDisplayController::displayDuplexUnavailableReason(m_radioModel);
    if (m_displayDuplexAction) {
        const QSignalBlocker block(m_displayDuplexAction);
        m_displayDuplexAction->setChecked(m_displayDuplexSetting);
        m_displayDuplexAction->setEnabled(reason.isEmpty());
        const QString tip = reason.isEmpty()
            ? QStringLiteral("Keep the receiver on the transmitting panadapter while "
                             "transmitting, under the transmit grid and colors.")
            : reason;
        m_displayDuplexAction->setToolTip(tip);
        m_displayDuplexAction->setStatusTip(tip);
    }
    if (m_containerButtons) {
        refreshContainerControls();
    }
}

void MainWindow::ensureRemoteSession()
{
    if (!m_station.isRemote() || !m_radioModel || m_shuttingDown) { return; }
    // A manual reconnect must reuse the same client, model attachment and
    // media controller. Their per-session state is retired by StationClient.
    if (m_stationClient == nullptr) {
        // dynamic_cast, not qobject_cast: AppSettings holds the backend as an
        // ISettingsBackend*, and that interface deliberately is NOT a QObject
        // (see ISettingsBackend.h), so there is no meta-object for qobject_cast
        // to walk. The interface has a virtual destructor, which is what makes
        // this cross-cast well-formed.
        auto* proxy = dynamic_cast<SettingsProxy*>(
            AppSettings::instance().remoteBackend());
        if (proxy == nullptr) {
            // Refusing here rather than dialling with a null proxy: without it
            // every Station-scoped read resolves against this machine's own
            // settings file and the operator drives the daemon with someone
            // else's DSP configuration on screen.
            qCWarning(lcConnection)
                << "Remote station requested but no SettingsProxy is installed as "
                   "the AppSettings backend; refusing to connect. This is a "
                   "programming error in the startup sequence, not a "
                   "configuration problem.";
            return;
        }

        m_stationClient = new StationClient(m_radioModel, proxy, this);
        connect(m_radioModel, &RadioModel::remoteMicSourceStateChanged,
                this, [this]() { applyRemoteRoleGating(); });
        // iPhone app Task 18 (R-IOS-08): this computer's own device key.
        // A Core it paired with is signed in to by key, and a token
        // sign-in to a Core with an identity enrols the key (the link
        // document, section 3.5). The Core lists it by the machine's name,
        // and its short name is the short host name (Part C fix wave).
        // Slice control plan Task 8b: a profile other than the default is
        // its own device, so its name carries the profile.
        m_stationClient->setDeviceIdentity(ClientDeviceIdentity::forThisProfile(),
                                           ClientDeviceIdentity::machineName(
                                               AppSettings::profileOverride()),
                                           ClientDeviceIdentity::machineShortName(
                                               AppSettings::profileOverride()));
        // Slice control plan Task 13: the chooser's answers and inventory.
        connect(m_stationClient, &StationClient::deviceCommandFinished, this,
                [this](const QByteArray& verb, quint32 commandId, bool accepted,
                       const QString& reason, bool awaitingConfirmation) {
                    const QPointer<MainWindow> self(this);
                    // TX badge take: the flag request this answer ends, read
                    // before the chooser clears it.
                    const bool chooserAnswer = m_sliceChooser
                        && m_sliceChooser->requestInFlight() == verb;
                    const int flagSlice = m_flagRequestSlice;
                    finishSliceChooserRequest(verb, accepted, reason);
                    if (!self) { return; }
                    // TX badge take: the badge's own slice take goes on to
                    // transmit; its own tx.take granted makes the slice the
                    // TX slice, refused or cancelled ends it (the refusal
                    // is shown as before).
                    if (!awaitingConfirmation) {
                        if (verb == QByteArrayLiteral("slice.takeControl")) {
                            if (chooserAnswer && flagSlice >= 0
                                && flagSlice == m_txBadgeTakeSlice) {
                                txBadgeSliceAnswered(flagSlice, accepted);
                                if (!self) { return; }
                            }
                        } else if (m_txBadgeTakeStage == TxBadgeStage::Transmit
                                   && m_txBadgeCommandId != 0
                                   && verb == QByteArrayLiteral("tx.take")
                                   && commandId == m_txBadgeCommandId) {
                            m_txBadgeCommandId = 0;
                            if (!accepted) {
                                abandonTxBadgeTake();
                            } else {
                                m_txBadgeGranted = true;
                                finishTxBadgeTakeIfHeld();
                                if (!self) { return; }
                            }
                        } else if (m_txBadgeTakeStage == TxBadgeStage::Transmit
                                   && m_txBadgeCommandId != 0
                                   && verb == QByteArrayLiteral("confirm.cancel")) {
                            abandonTxBadgeTake();
                        }
                    }
                    // Slice control plan Task 16 (ruling U1): a slice this
                    // window now listens to or controls is shown here.
                    if (verb == QByteArrayLiteral("slice.listen")
                        || verb == QByteArrayLiteral("slice.takeControl")) {
                        const int reveal = m_pendingRevealSlice;
                        m_pendingRevealSlice = -1;
                        if (accepted && reveal >= 0) { revealSliceInWindow(reveal); }
                    }
                });
        // A request still waiting when the link drops is not answered.
        connect(m_stationClient, &StationClient::sessionEnded, this, [this](const QString&) {
            // TX badge take: nothing it waits on is answered now.
            abandonTxBadgeTake();
            if (m_sliceChooser) {
                m_sliceChooser->linkLost();
            }
        });
        if (SliceAccessMirror* access = m_stationClient->sliceAccess()) {
            // Slice control plan Task 15 fix round 1: a container's slice
            // buttons follow the change of control, as the flag and tabs do.
            connect(access, &SliceAccessMirror::changed, this,
                    [this](int) {
                refreshSliceChooser();
                refreshContainerControls();
                reconcileListenPlacements();
                // A marker's owner words come from the access entry.
                refreshForeignMarkers();
            });
        }
        if (RemoteDevicesState* devices = m_stationClient->remoteDevices()) {
            connect(devices, &RemoteDevicesState::markersChanged, this,
                    [this]() { refreshSliceChooser(); });
            connect(devices, &RemoteDevicesState::connectedDevicesChanged, this,
                    [this]() {
                refreshSliceChooser();
                // The hosting desktop's name, on a station-held marker.
                refreshForeignMarkers();
            });
        }
        // iPhone app plan Task 39: the Core's transmit meters.
        wireRemoteTransmitMeters();
        // iPhone app plan Task 25: the Core computer's VAX channels.
        wireRemoteStationVax();
        // iPhone app plan Task 78: several devices on one Core.
        wireRemoteDevices();
        m_remoteConnection = new RemoteConnectionController(
            m_stationClient, m_radioModel, m_station, this);
        connect(m_remoteConnection, &RemoteConnectionController::changed,
                this, &MainWindow::refreshRemoteConnectionUi);
        // R-R3-38: when the Core ends this window for good (another app
        // took over, the link versions do not match, or any end the Core
        // marks not retryable), the window stays as it is and a message
        // over its content says why and offers the next steps. Nothing
        // retries by itself; a dropped link still retries as before.
        m_coreStopBanner = new CoreStopBanner(m_remoteConnection, this);
        m_coreStopBanner->setChooseAnotherCoreAvailable(m_connectionPickerManaged);
        m_coreStopBanner->setCheckForUpdatesAvailable(true);
        connect(m_coreStopBanner, &CoreStopBanner::contentChanged,
                this, &MainWindow::placeCoreStopBanner);
        // R-R3-38: place it again when the content area changes size or
        // moves without the window resizing (a dock); see eventFilter.
        // A later setCentralWidget would need this filter moved to the new one.
        if (QWidget* content = centralWidget()) {
            content->installEventFilter(this);
        }
        connect(m_coreStopBanner, &CoreStopBanner::chooseAnotherCoreRequested, this, [this] {
            if (m_shuttingDown || m_retiringSession) { return; }
            if (m_connectionPickerManaged) { emit connectionsRequested(); }
        });
        connect(m_coreStopBanner, &CoreStopBanner::checkForUpdatesRequested,
                this, &MainWindow::checkForUpdates);
        placeCoreStopBanner();
        // R-R3-16 / R-R3-38: Connections opens only after the operator's
        // own Disconnect (Radio > Disconnect, the Connections window, the
        // Core panel), never on link loss or an offline radio: the window
        // retries and says so in its title bar and station block instead.
        // With the picker managing this window that is the Connections
        // window; a --station window has no picker, so it shows the Core
        // panel, which never dials by itself.
        connect(m_remoteConnection, &RemoteConnectionController::operatorDisconnected,
                this, [this] {
            if (m_shuttingDown || m_retiringSession) { return; }
            if (m_connectionPickerManaged) {
                emit connectionsRequested();
                return;
            }
            showRemoteConnectionPanel();
        });
        connect(m_stationClient, &StationClient::connectionActivityChanged,
                this, &MainWindow::applyRemoteRoleGating);
        // R-R3-17: the Connections window and the Core panel disconnect (or
        // cancel a pending retry) through RemoteConnectionController, not
        // disconnectFromStation() below. With a retry pending there is no
        // transport, so sessionEnded never reaches the toast handler. The
        // link stays active through every backoff step, so it goes inactive
        // only when the operator stops it or the Core refuses outright:
        // either way the next failure the operator asks for is news.
        connect(m_stationClient, &StationClient::connectionActivityChanged, this, [this] {
            if (!m_stationClient->isConnectionActive()) {
                clearStationLinkToastMemory();
            }
        });
        connect(m_radioModel, &RadioModel::stationLinkStateChanged,
                this, &MainWindow::applyRemoteRoleGating);
        // R-R3-49 (parity Task 1): the transmit settings grey while the
        // Core's radio is on the air and come back when it stops.
        connect(m_radioModel, &RadioModel::coreOnAirChanged,
                this, &MainWindow::applyRemoteRoleGating);
        m_remoteMedia = new RemoteMediaController(m_stationClient, m_radioModel,
                                               m_panStack, m_stationClient);
        connect(m_remoteMedia, &RemoteMediaController::miniDisplayFrame, this,
                &MainWindow::presentMiniFrame);
        connect(m_remoteMedia, &RemoteMediaController::miniDisplayUnavailable, this,
                &MainWindow::clearMiniSlice);
        // Remote-window parity Task 29 (A11, R-R3-49, R-R3-12): while the
        // Core is keyed the pan hosting the transmitting slice shows the
        // Core's transmit display with the transmit grid, palette,
        // waterfall levels and red border, from the same controller a local
        // window uses; it follows the Core's txState (or, on a Core that
        // sends none, radio.transmitting and the transmit slice).
        m_remoteTxDisplaySource = std::make_unique<RemoteTxDisplaySource>(
            m_remoteMedia, m_stationClient, m_panStack);
        if (m_moxDisplay == nullptr) {
            m_moxDisplay = new MoxDisplayController(m_panStack, m_radioModel, this);
        }
        m_moxDisplay->setSource(m_remoteTxDisplaySource.get());
        m_moxDisplay->followStation(m_stationClient);
        // Parity Task 31 (A11): DUP. The Core keeps the transmitting pan's
        // receive frames for a subscription that says `duplex` (version 3);
        // the menu item and the DUP button follow the Core's version.
        connect(m_moxDisplay, &MoxDisplayController::displayDuplexChanged,
                m_remoteMedia, &RemoteMediaController::setDisplayDuplex);
        connect(m_radioModel, &RadioModel::stationTxDisplayVersionChanged,
                this, &MainWindow::applyDisplayDuplex);
        applyDisplayDuplex();
        m_remoteMedia->setDisplayDuplex(m_moxDisplay->displayDuplex());
        // Parity Task 32 (R-IOS-13, R-R3-49): this computer's MON output
        // choice is where the Core sends MON while this window holds
        // transmit: pushed now and on every change.
        if (AudioEngine* devices = m_radioModel->localAudioDevices()) {
            const auto routeFor = [](TxMonitorOutput output) {
                return output == TxMonitorOutput::Headphones ? TxMonitorRoute::Headphones
                                                             : TxMonitorRoute::Speakers;
            };
            m_remoteMedia->setTxMonitorRoute(routeFor(devices->txMonitorOutput()));
            RemoteMediaController* const media = m_remoteMedia;
            connect(devices, &AudioEngine::txMonitorOutputChanged, media,
                    [media, routeFor](TxMonitorOutput output) {
                media->setTxMonitorRoute(routeFor(output));
            });
        }
        m_remoteTelemetry = new RemoteTelemetryController(
            m_stationClient, m_remoteMedia, this);
        // R-R3-32 (parity Task 6): the Core's PA readings reach this
        // window's model, which every PA surface reads.
        m_remoteTelemetry->setPaReadingsTarget(m_radioModel);
        connect(m_remoteTelemetry, &RemoteTelemetryController::changed, this, [this] {
            if (m_titleBar) {
                refreshRemoteConnectionUi();
            }
        });
        connect(m_stationClient->remoteDevices(), &RemoteDevicesState::coreInfoChanged,
                this, &MainWindow::refreshRemoteConnectionUi);
        connect(m_stationClient, &StationClient::pathChanged,
                this, &MainWindow::refreshRemoteConnectionUi);
        connect(m_remoteMedia, &RemoteMediaController::networkPathChanged,
                this, &MainWindow::refreshRemoteConnectionUi);
        connect(m_remoteMedia, &RemoteMediaController::audioStatusChanged,
                this, &MainWindow::refreshRemoteConnectionUi);
        connect(m_remoteMedia, &RemoteMediaController::errorOccurred, this, [this](const QString& reason) {
            // The raw reason is for the log; the toast says it in user
            // words (R-R3-21, R-R3-23).
            qCWarning(lcConnection) << "Station media:" << reason;
            showToast(tr("Audio and display: %1").arg(OperatorReasonText::forDisplay(reason)),
                      ToastSeverity::Warning, 5000);
        });
        // R-R3-43 / R-R3-44: the VAX page's compressed-audio note follows
        // the quality choice, its fallback and the Core's capabilities while
        // Setup is open.
        wireReceiverAudioNotePush(this, m_remoteMedia, m_radioModel,
                                  [this] { return receiverAudioNoteFor(m_remoteMedia); });
        // Fix wave 2 (M8): VOX follows this computer's microphone line.
        connect(m_remoteMedia, &RemoteMediaController::micLineChanged, this, [this](bool) {
            if (!m_shuttingDown) {
                applyRemoteRoleGating();
            }
        });
        connect(m_remoteMedia, &RemoteMediaController::recoveryRequested,
                m_remoteConnection, &RemoteConnectionController::recoverMediaSession,
                Qt::QueuedConnection);
#ifdef HAVE_WEBSOCKETS
        // R-R3-42: TCI receiver N plays the Core's slice N, asked for while
        // an app listens. The controller is deleted before the TCI server
        // (see ~MainWindow), so the server's final release finds it gone.
        if (m_tciServer) {
            const QPointer<RemoteMediaController> media(m_remoteMedia);
            TciServer::RemoteReceiverAudio source;
            source.request = [media](int sliceId, IReceiverPcmSink* sink) {
                return media ? media->requestReceiverAudio(sliceId, sink)
                             : std::shared_ptr<RemoteTciAudioStage>{};
            };
            source.release = [media](int sliceId, IReceiverPcmSink* sink) {
                if (media) { media->releaseReceiverAudio(sliceId, sink); }
            };
            source.unavailableReason =
                QString::fromLatin1(RemoteMediaController::kReceiverAudioUnavailableReason);
            m_tciServer->setRemoteReceiverAudio(std::move(source));
            TciServer::RemoteIqSource iq;
            iq.available = [media] { return media && media->remoteIqNegotiated(); };
            iq.request = [media](int sliceId) {
                if (media) { media->requestRawIq(sliceId); }
            };
            iq.release = [media](int sliceId) {
                if (media) { media->releaseRawIq(sliceId); }
            };
            m_tciServer->setRemoteIqSource(std::move(iq));
            connect(m_remoteMedia, &RemoteMediaController::rawIqBlock,
                    m_tciServer, &TciServer::receiveRemoteIq);
            connect(m_remoteMedia, &RemoteMediaController::rawIqRate,
                    m_tciServer, &TciServer::setRemoteIqRate);
            connect(m_remoteMedia, &RemoteMediaController::rawIqUnavailable,
                    m_tciServer, &TciServer::remoteIqUnavailable);
            connect(m_stationClient, &StationClient::handshakeComplete,
                    m_tciServer, &TciServer::refreshRemoteIqDemand);
            connect(m_stationClient, &StationClient::stateSnapshotApplied,
                    m_tciServer, &TciServer::refreshRemoteIqDemand);
        }
#endif
        // R-R3-44: VAX in a remote window. This computer's VAX outputs open
        // here (the engine never starts in a remote window), and each VAX
        // channel carries the Core's slice assigned to it, asked for while
        // an app reads the channel (where the platform says) and released
        // otherwise. The router is deleted right after the controller (see
        // ~MainWindow).
        if (AudioEngine* vaxEngine = m_radioModel->localAudioDevices()) {
            vaxEngine->openVaxOutputs();
            // R-R3-45: this computer's headphones open here too when Setup,
            // Audio, Devices has them enabled, as a local start() opens
            // them, so the Core's headphones mix has somewhere to play.
            if (vaxEngine->headphonesEnabled() && !vaxEngine->headphonesAvailable()) {
                vaxEngine->setHeadphonesEnabled(true);
            }
            m_remoteVax = new RemoteVaxRouter(m_radioModel, vaxEngine,
                                              RemoteVaxRouter::coreKeyFor(m_station), this);
            const QPointer<RemoteMediaController> media(m_remoteMedia);
            RemoteVaxRouter::ReceiverAudio vaxSource;
            vaxSource.request = [media](int sliceId, IReceiverPcmSink* sink) {
                if (media) { media->requestReceiverAudio(sliceId, sink); }
            };
            vaxSource.release = [media](int sliceId, IReceiverPcmSink* sink) {
                if (media) { media->releaseReceiverAudio(sliceId, sink); }
            };
            m_remoteVax->setReceiverAudio(std::move(vaxSource));
            connect(m_remoteVax, &RemoteVaxRouter::notice, this,
                    [this](int channel, int sliceId, const QString& reason) {
                qCInfo(lcConnection) << "VAX" << channel << "slice" << sliceId
                                     << "notice:" << reason;
                // R-R3-44 fix wave: one notice per stop, shared with TCI
                // and the other VAX channels carrying the same receiver;
                // none for what the window's own status already says.
                const QString text = m_receiverStopNotices.toastFor(
                    reason, sliceId, QDateTime::currentMSecsSinceEpoch());
                if (!text.isEmpty()) {
                    showToast(text, ToastSeverity::Warning, 5000);
                }
            });
        }

        // R-R3-46 / R-R3-21: an attenuator edit the Core kept at another
        // value (its radio's range, a mode the radio does not offer), or
        // that this window could not send, says why in user words.
        connect(m_stationClient, &StationClient::propertyWriteCompleted, this,
                [this](const QByteArray& objectKey, const QByteArray& property, quint32,
                       bool accepted, const QString& reason) {
            // R-R3-46: so does an antenna edit (Setup > Hardware Config).
            // Fix wave M8: and VOX the Core would not arm (no microphone
            // line from this window yet), whose button drops back.
            const bool voxWrite = objectKey == "transmit" && property == "voxEnabled";
            // Task 78: a write held for the Core's question is not refused.
            if ((objectKey == "stepAtt" || objectKey == "alexAntennas" || voxWrite) && !accepted
                && !reason.isEmpty() && !StationClient::isAwaitingConfirmation(reason)) {
                showToast(OperatorReasonText::forDisplay(reason), ToastSeverity::Warning, 4000);
            }
        });
        if (StepAttenuatorFacade* stepAtt = m_radioModel->stepAttFacade()) {
            connect(stepAtt, &StepAttenuatorFacade::editRejected, this,
                    [this](const QString& reason) {
                showToast(OperatorReasonText::forDisplay(reason), ToastSeverity::Warning, 4000);
            });
        }
        if (AlexAntennaFacade* alex = m_radioModel->alexAntennaFacade()) {
            connect(alex, &AlexAntennaFacade::editRejected, this,
                    [this](const QString& reason) {
                showToast(OperatorReasonText::forDisplay(reason), ToastSeverity::Warning, 4000);
            });
        }

        // Parity Task 18 (C8): an empty pan says so once the Core's slices
        // are known, and stops when the session ends.
        connect(m_stationClient, &StationClient::handshakeComplete,
                this, &MainWindow::refreshNoSliceHints);
        connect(m_stationClient, &StationClient::stateSnapshotApplied,
                this, &MainWindow::refreshNoSliceHints);
        connect(m_stationClient, &StationClient::sessionEnded,
                this, &MainWindow::refreshNoSliceHints);
        connect(m_stationClient, &StationClient::handshakeComplete, this, [this]() {
            qCInfo(lcConnection) << "Station handshake complete:" << m_remoteConnection->endpointText();
            clearStationLinkToastMemory();
            showToast(tr("Connected to the Core at %1").arg(m_remoteConnection->endpointText()),
                      ToastSeverity::Info, 3000);
        });
        const auto explainReceiveLayout = [this] {
            if (!m_stationClient->isHandshakeComplete()) { return; }
            const QString toast = m_receiveLayoutNotices.toastFor(
                m_radioModel->receiveLayoutRestoreState(),
                m_radioModel->receiveLayoutRestoreMessage(), /*viaCore*/ true);
            if (!toast.isEmpty()) {
                showToast(toast, ToastSeverity::Warning, 10000);
            }
        };
        // State/detail arrive as a property bag. Queue evaluation so a paired
        // update is settled before presenting why a saved pan has no receiver.
        connect(m_radioModel, &RadioModel::receiveLayoutRestoreStatusChanged,
                this, explainReceiveLayout, Qt::QueuedConnection);
        connect(m_stationClient, &StationClient::stateSnapshotApplied,
                this, explainReceiveLayout, Qt::QueuedConnection);
        connect(m_stationClient, &StationClient::sessionEnded, this,
                [this](const QString& reason) {
            if (m_stationDisconnectRequested
                || reason == QStringLiteral("operator disconnect")) {
                return;
            }
            qCWarning(lcConnection) << "Station session ended:" << reason;
            // R-R3-38: an end that stops the window for good is said by the
            // stop message over the content, with its buttons; a toast
            // would say the same thing twice.
            if (m_remoteConnection
                && m_remoteConnection->stopNotice() != CoreStopNotice::None) {
                return;
            }
            // R-R3-17: a redial that keeps failing reports the same reason
            // at every backoff step, up to once a minute for as long as the
            // Core stays away. The reason is already shown persistently
            // (Connections window, Core panel, title bar), so toast it once
            // per distinct reason, the same way m_receiveLayoutNotices
            // holds the receive layout notice to once.
            if (m_stationLinkLostSeen && reason == m_lastStationLinkLostReason) {
                return;
            }
            m_stationLinkLostSeen = true;
            m_lastStationLinkLostReason = reason;
            // The operator's ruling of 2026-09-26: a radio change restarts
            // the Core; this window reconnects by itself, so it is not a
            // lost link. The Core's words name the radio.
            if (!m_stationClient->radioChangeReason().isEmpty()) {
                showToast(tr("The Core is changing its radio. This window reconnects by "
                             "itself."),
                          ToastSeverity::Info, 5000);
                return;
            }
            // The raw reason is logged above and compared as text here;
            // only the toast is in user words (R-R3-17, R-R3-21).
            showToast(tr("Link to the Core lost: %1")
                          .arg(OperatorReasonText::forDisplay(reason)),
                      ToastSeverity::Warning, 5000);
        });
        connect(m_stationClient, &StationClient::reconnectScheduled, this,
                [this](int attempt, int delayMs) {
            // Same rule as the link-lost toast above, keyed on the reason
            // that made this retry necessary: the first retry after a new
            // reason is announced, the later ones for that reason are not.
            // sessionEnded() is emitted before reconnectScheduled(), so the
            // reason recorded above is the one this retry answers.
            if (m_reconnectToastSeen
                && m_lastReconnectToastReason == m_lastStationLinkLostReason) {
                return;
            }
            m_reconnectToastSeen = true;
            m_lastReconnectToastReason = m_lastStationLinkLostReason;
            // A radio change said it reconnects already (above).
            if (!m_stationClient->radioChangeReason().isEmpty()) {
                return;
            }
            showToast(tr("Reconnecting to the Core (attempt %1) in %2 s")
                          .arg(attempt).arg((delayMs + 999) / 1000),
                      ToastSeverity::Info, 3000);
        });

        // R-R3-21: the meter update interval is the Core's setting
        // (MultimeterDelayMs, Station scope); the window's meter poller
        // read it at startup, before the Core's settings arrived.
        connect(proxy, &SettingsProxy::snapshotApplied, this, [this](int) {
            MultimeterPage::applyPersistedMeterInterval(m_radioModel);
        });

        // Whole-branch review, Important 2. The three toasts above tell the
        // operator about the LINK. This one tells them about their own EDIT,
        // which nothing did before: while the link is down SettingsProxy still
        // caches a write and value() still returns it, so the Setup control
        // they moved reads back as applied, and the reconnect snapshot then
        // replaces it with the station's own value in silence.
        //
        // Proportionate on purpose: a COUNT here, the key names at warning
        // level in SettingsProxy::applySnapshot(). A remote client that has
        // been offline through a band change can have a dozen of these, and a
        // toast listing "hardware/aa:bb:.../alex/hpf/..." twelve times is
        // noise the operator will learn to dismiss unread.
        connect(proxy, &SettingsProxy::offlineEditsSuperseded, this,
                [this](const QStringList& keys) {
            showToast(tr("%n Core setting(s) you changed while the link was down "
                         "did not stick. See the log for which.", "", keys.size()),
                      ToastSeverity::Warning, 8000);
        });

        // Task 19: the Disconnect side must pass attemptReconnect = false so a
        // deliberate quit does not schedule a surprise redial during teardown.
        connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
            if (m_stationClient != nullptr) {
                m_stationClient->disconnectFromStation(
                    QStringLiteral("client shutting down"));
            }
        });
    }

}

void MainWindow::disconnectFromStation()
{
    if (m_stationClient == nullptr) {
        return;
    }
    m_stationDisconnectRequested = true;
    m_remoteConnection->disconnectFromStation();
    m_stationDisconnectRequested = false;
    clearStationLinkToastMemory();
    showToast(tr("Disconnected from the Core"), ToastSeverity::Info, 3000);
}

void MainWindow::clearStationLinkToastMemory()
{
    m_stationLinkLostSeen = false;
    m_lastStationLinkLostReason.clear();
    m_reconnectToastSeen = false;
    m_lastReconnectToastReason.clear();
}

void MainWindow::connectionRequestedByOperator()
{
    if (m_shuttingDown) { return; }
    if (m_connectionPickerManaged) {
        emit connectionsRequested();
        return;
    }
    if (!m_station.isRemote()) {
        showConnectionPanel();
        return;
    }
    // Explicit clicks can dial. Automatic panel-open callbacks never do.
    if (!m_stationClient || !m_stationClient->isConnectionActive()) {
        connectToStation();
    }
    showRemoteConnectionPanel();
}

void MainWindow::showRemoteConnectionPanel()
{
    if (!m_remoteConnection) { return; }
    if (!m_remoteConnectionPanel) {
        m_remoteConnectionPanel = new RemoteConnectionPanel(m_remoteConnection, this, m_remoteMedia, m_remoteTelemetry);
    }
    m_remoteConnectionPanel->show();
    m_remoteConnectionPanel->raise();
    m_remoteConnectionPanel->activateWindow();
}

void MainWindow::placeCoreStopBanner()
{
    if (!m_coreStopBanner || !m_coreStopBanner->isVisibleTo(this)) { return; }
    const QWidget* content = centralWidget();
    const QRect area = content ? content->geometry() : rect();
    const int width = std::max(0, std::min(560, area.width() - 32));
    m_coreStopBanner->setFixedWidth(width);
    QLayout* const layout = m_coreStopBanner->layout();
    const int height = layout && layout->hasHeightForWidth()
        ? layout->totalHeightForWidth(width) : m_coreStopBanner->sizeHint().height();
    m_coreStopBanner->setFixedHeight(height);
    m_coreStopBanner->move(area.x() + (area.width() - width) / 2, area.y() + 16);
    m_coreStopBanner->raise();
}

void MainWindow::refreshRemoteConnectionUi()
{
    if (!m_remoteConnection) { return; }
    applyRemoteRoleGating();
    if (m_titleBar) {
        auto* segment = m_titleBar->connectionSegment();
        segment->setState(m_remoteConnection->state());
        segment->setRemoteStatusText(m_remoteConnection->statusText());
        segment->setRemoteTelemetryText(m_remoteTelemetry ? m_remoteTelemetry->bannerText() : QString{});
        const bool current = m_remoteConnection->state() == ConnectionState::Connected
            && m_stationClient && m_stationClient->isHandshakeComplete();
        if (current && m_remoteTelemetry) {
            const RemoteTelemetryView& view = m_remoteTelemetry->current();
            const auto number = [](std::optional<double> value) {
                return value ? QString::number(*value, 'f', 1) : QStringLiteral("–");
            };
            const bool megabits = view.coreGuiTotalKbps && *view.coreGuiTotalKbps >= 1000.0;
            const double divisor = megabits ? 1000.0 : 1.0;
            const auto traffic = [divisor, &number](std::optional<double> value) {
                return number(value ? std::optional<double>(*value / divisor) : std::nullopt);
            };
            const QStringList groups{
                (!view.coreGuiRxKbps && !view.coreGuiTxKbps)
                    ? tr("Traffic – kbps")
                    : tr("Traffic ↓%1 ↑%2 %3")
                          .arg(traffic(view.coreGuiRxKbps), traffic(view.coreGuiTxKbps),
                               megabits ? tr("Mbps") : tr("kbps")),
                ConnectionSegment::audioMetricText(
                    view.audioPayloadRxKbps, m_remoteMedia->audioStatus().state),
                view.state == RemoteTelemetryView::State::Current && view.radio.connected
                    ? tr("Radio ↓%1 ↑%2 Mbps")
                          .arg(number(view.radio.rxMbps), number(view.radio.txMbps))
                    : tr("Radio – Mbps"),
                tr("Core RTT %1ms")
                    .arg(view.coreRttMs ? QString::number(*view.coreRttMs) : QStringLiteral("–"))};
            segment->setRemoteMetrics(groups);
            std::optional<NetworkPathSnapshot> control;
            if (SessionTransport* transport = m_stationClient->transport()) {
                control = transport->networkPathSnapshot();
            }
            segment->setRemotePaths(control,
                                    m_remoteMedia ? m_remoteMedia->currentNetworkPath() : std::nullopt);
        } else {
            segment->setRemoteMetrics({});
            segment->setRemotePaths(std::nullopt, std::nullopt);
        }
    }
    if (m_stationBlock) {
        const CoreSettingsContext context = coreSettingsSnapshot();
        const QString reported = context.authenticated ? context.coreName.trimmed() : QString();
        const QString name = !reported.isEmpty() ? reported
            : !m_savedCoreName.isEmpty() ? tr("%1 (last known)").arg(m_savedCoreName)
            : tr("Core name not reported");
        m_stationBlock->setRadioName(name);
        const auto compactPath = [](const std::optional<NetworkPathSnapshot>& path) {
            if (!path) { return tr("path unknown"); }
            const QString kind = path->kind == NetworkPathSnapshot::Kind::Direct ? tr("direct")
                : path->kind == NetworkPathSnapshot::Kind::Relayed ? tr("via relay") : tr("path unknown");
            QHostAddress peer;
            return peer.setAddress(path->remoteAddress)
                ? QStringLiteral("%1 %2").arg(kind, peer.toString()) : kind;
        };
        const bool current = context.authenticated;
        const auto control = current && m_stationClient->transport()
            ? m_stationClient->transport()->networkPathSnapshot() : std::optional<NetworkPathSnapshot>{};
        const auto media = current && m_remoteMedia ? m_remoteMedia->currentNetworkPath() : std::optional<NetworkPathSnapshot>{};
        m_stationBlock->setConnectionLines(current ? tr("Controls %1").arg(compactPath(control))
            : m_remoteConnection->statusText(), current ? tr("Audio/display %1 · %2").arg(compactPath(media),
                m_remoteMedia ? remoteAudioBannerWord(m_remoteMedia->audioStatus().state) : tr("unavailable")) : QString());
        m_stationBlock->setToolTip(name + QLatin1Char('\n') + context.controls
            + QLatin1Char('\n') + context.audioAndDisplay + QLatin1Char('\n')
            + m_remoteConnection->detailText() + QLatin1Char('\n')
            + tr("Radio: %1\nListener: %2\nReached through: %3")
                .arg(context.radio, context.listener, context.reachedThrough));
        if (m_chromeBar && m_chromeBarWidget) {
            m_chromeBar->setNaturalWidth(m_stationBlock, m_stationBlock->sizeHint().width());
            m_chromeBar->relayout(m_chromeBarWidget->width());
        }
    }
}

// Phase 3F Sub-Epic D Task 12: resolve the active pan's SpectrumWidget.
// Used as a backward-compat shim for call sites that still address "the"
// spectrum widget; long-term these should migrate to per-pan addressing.
// Returns nullptr if m_panStack isn't constructed yet (during early init)
// or if the active pan has no widget.
SpectrumWidget* MainWindow::activeSpectrumWidget() const
{
    if (!m_panStack) { return nullptr; }
    auto* applet = m_panStack->panadapter(m_panStack->activePanId());
    return applet ? applet->spectrumWidget() : nullptr;
}

// R1 Task 4 fix round 1 (reviewer Finding 2): the only place that sets
// RadioModel's two spectrum view hooks. Both must always name the same
// widget -- setSpectrumWidget() feeds the 82 Setup-page call sites that
// need the concrete SpectrumWidget API, setSpectrumSink() feeds the
// abstract ISpectrumSink RadioModel's own SWR-overlay and
// applyClaritySmoothDefaults calls use. Routing every caller through here
// instead of two adjacent statements at each call site means a future
// third wiring site cannot forget the second call and silently leave the
// two hooks pointing at different widgets.
void MainWindow::setSpectrumHooks(SpectrumWidget* sw)
{
    if (!m_radioModel) { return; }
    m_radioModel->setSpectrumWidget(sw);
    m_radioModel->setSpectrumSink(sw);
}

// Phase 3F multi-pan: resolve the SpectrumWidget that owns this slice's
// panadapter from the slice's panKey(), falling back to the active pan when
// the key is empty (Slice A pre-seed) or the pan was removed.
// Ported from AetherSDR MainWindow::spectrumForSlice (MainWindow.cpp:14856
// [@6a142807]); AetherSDR uses s->panId() (a string), NereusSDR uses
// s->panKey().
SpectrumWidget* MainWindow::spectrumForSlice(SliceModel* s) const
{
    if (s && m_panStack) {
        // Slice control plan Task 16: where this window placed a slice it
        // only listens to.
        if (auto* sw = m_panStack->spectrum(m_listenPlacement.value(s->sliceIndex()))) {
            return sw;
        }
        if (auto* sw = m_panStack->spectrum(s->panKey())) {
            return sw;
        }
    }
    return activeSpectrumWidget();  // fallback to active pan
}

SliceModel* MainWindow::sliceForAddedIdForTest(RadioModel* model, int sliceId)
{
    return model ? model->sliceById(sliceId) : nullptr;
}

void MainWindow::applyAntennaChangeForTest(RadioModel* model, int sliceId,
                                            const QString& antennaName)
{
    if (SliceModel* slice = sliceForAddedIdForTest(model, sliceId)) {
        slice->setRxAntenna(antennaName);
    }
}

void MainWindow::wireRadeFlagForTest(RadioModel* model, VfoWidget* flag,
                                     int sliceId)
{
    if (!model || !flag) { return; }
    const QPointer<VfoWidget> flagRef(flag);
    connect(model, &RadioModel::radeSyncChanged, flag,
            [flagRef, model, sliceId](int changedSliceId, bool synced) {
        if (!flagRef || changedSliceId != sliceId) { return; }
        if (!synced) {
            flagRef->setRadeSynced(false);
            return;
        }
        // A local decoder's next SNR tick repaints the lock. A remote
        // window's Core sends the SNR only when it moves, so a lock
        // regained at the same SNR repaints from the slice's value here.
        if (SliceModel* slice = model->sliceById(sliceId)) {
            if (!std::isnan(slice->snrDb())) {
                flagRef->setRadeSnrLabel(static_cast<float>(slice->snrDb()));
            }
        }
    });
    connect(model, &RadioModel::radeFreqOffsetChanged, flag,
            [flagRef, sliceId](int changedSliceId, float hz) {
        if (flagRef && changedSliceId == sliceId) {
            flagRef->setRadeFreqOffset(hz);
        }
    });
}

void MainWindow::configureSpectrumForPanForTest(SpectrumWidget* spectrum,
                                                 const QString& panId)
{
    if (!spectrum) { return; }
    bool ok = false;
    const int parsed = panId.startsWith(QStringLiteral("pan-"))
        ? panId.mid(4).toInt(&ok) : 0;
    spectrum->setPanIndex(ok && parsed >= 0 ? parsed : 0);
    spectrum->loadSettings();
}

void MainWindow::wireWidebandExtensionForTest(SpectrumWidget* spectrum,
                                              RadioModel* model,
                                              PanadapterStack* stack,
                                              const QString& panId)
{
    if (!spectrum || !model || !stack) { return; }
    const QPointer<RadioModel> modelRef(model);
    const QPointer<PanadapterStack> stackRef(stack);
    const auto resolve = [modelRef, stackRef, panId]() -> SliceModel* {
        if (!modelRef || !stackRef) { return nullptr; }
        PanadapterApplet* applet = stackRef->panadapter(panId);
        if (!applet) { return nullptr; }
        if (SliceModel* active =
                modelRef->sliceById(applet->activeSliceIndex())) {
            return active;
        }
        for (SliceModel* slice : modelRef->slices()) {
            if (slice
                && applet->associatedSlices().contains(slice->sliceIndex())) {
                return slice;
            }
        }
        return nullptr;
    };
    connect(spectrum, &SpectrumWidget::widebandExtensionStateChanged,
            spectrum, [resolve](bool on) {
        if (SliceModel* slice = resolve()) {
            slice->setWidebandExtensionRequested(on);
        }
    });
    connect(spectrum, &SpectrumWidget::ddcRetuneRequested,
            spectrum, [resolve](double freqHz) {
        if (SliceModel* slice = resolve()) {
            slice->setFrequency(freqHz);
        }
    });
    // Settings restore and rate seeding may have derived the actual state
    // before this bridge existed, so seed the resolved slice immediately.
    if (SliceModel* slice = resolve()) {
        slice->setWidebandExtensionRequested(spectrum->extendedMode());
    }
}

void MainWindow::fanWidebandBinsForTest(PanadapterStack* stack, int adcIndex,
                                        const QVector<float>& bins)
{
    if (!stack) { return; }
    for (PanadapterApplet* applet : stack->allApplets()) {
        if (applet && applet->spectrumWidget()) {
            applet->spectrumWidget()->setWidebandBins(adcIndex, bins);
        }
    }
}

// 3D Stacked-Trace Spectrum Plan Task 14 fix-forward: 3D Floor recall.
// Unlike the other five 3D controls (per panadapter, stored on
// SpectrumWidget itself), 3D Floor is stored per band on PanadapterModel
// because it is anchored to the measured noise floor -- see
// PanadapterModel::dss3DFloorDepthForBand's class-header comment. Pushes
// the value stored for the current band immediately (so app startup, which
// runs before any bandChanged() has fired, is covered too --
// SpectrumWidget::loadSettings() never touches m_dssFloorDepth, so without
// this push the widget would sit at its hardcoded ship default of 6 until
// the operator's next band change), then keeps it synced on every
// PanadapterModel::bandChanged() crossing.
//
// Task 15 fix-forward (coordinator review): the loop above is recall-only.
// Task 14 built PanadapterModel::setDss3DFloorDepthForBand (the SAVE half)
// but nothing ever called it -- confirmed by grep, only its own definition
// and test call sites reference it. An operator dragging 3D Floor via
// either live-editing surface (the Task 13 overlay menu or the Task 15
// Setup page, both of which ultimately call SpectrumWidget::setDssFloorDepth)
// saw the value appear to work, then silently lose it on the next band
// change -- worse than not being per-band at all, since it looks like it
// took effect. Closed below: SpectrumWidget::dssFloorDepthChanged (the new
// Task 15 signal) now also drives a write back into the CURRENT band's
// per-band store, using the exact same "current band" pan->band() the
// recall lambda reads from -- PanadapterModel::setBand() updates m_band
// strictly before emitting bandChanged(), so pan->band() is never stale
// inside either lambda.
//
// recallInProgress guards the save path against the recall push above
// re-triggering itself: pushing the stored value into the widget fires
// dssFloorDepthChanged just like an operator edit would, and without this
// guard that would be indistinguishable from a real edit. In the ordinary
// case PanadapterModel::setDss3DFloorDepthForBand's own early-return
// (`if (slot.dss3DFloorDepth == depth) { return; }`, Task 14) absorbs the
// echo, since a recall for band B, by construction, pushes exactly the
// value already stored for band B -- but SpectrumWidget::setDssFloorDepth
// clamps to [0,24] and PanadapterModel does not, so a stored value outside
// that range (unreachable through either live-editing UI surface today,
// but not through storage itself -- see the mutation-proven test
// recallInProgress_stopsAClampedRecallFromCorruptingStorage_endToEnd in
// tst_dss_persistence.cpp) recalls as the CLAMPED value, `!=` the stored
// one, and WOULD get written straight back over the operator's real
// stored intent without this guard. Same shape as
// Display3DSetupPage::m_updatingFromModel (Task 15): a push driven by "the
// stored value, being recalled" must not be mistaken for "the operator
// changed the value." Heap-allocated (std::shared_ptr, not a stack bool)
// because both lambdas below outlive this function and must share the
// same flag.
void MainWindow::wireDss3DFloorRecallForTest(PanadapterModel* pan,
                                             SpectrumWidget* spectrum)
{
    if (!pan || !spectrum) { return; }

    auto recallInProgress = std::make_shared<bool>(false);

    auto pushRecall = [pan, spectrum, recallInProgress](NereusSDR::Band band) {
        *recallInProgress = true;
        spectrum->setDssFloorDepth(pan->dss3DFloorDepthForBand(band));
        *recallInProgress = false;
    };

    pushRecall(pan->band());
    connect(pan, &PanadapterModel::bandChanged, spectrum,
            [pushRecall](NereusSDR::Band newBand) { pushRecall(newBand); });

    connect(spectrum, &SpectrumWidget::dssFloorDepthChanged, pan,
            [pan, recallInProgress](int depth) {
        if (*recallInProgress) { return; }
        pan->setDss3DFloorDepthForBand(pan->band(), depth);
    });
}

// Phase 3F: create + fully wire a slice's VfoWidget on the given
// SpectrumWidget. Factored out of the sliceAdded handler so the same wiring
// is reused on panKeyChanged migration. Every flag is built here, Slice A's
// included: wireSliceToSpectrum() calls it for Slice A and adds only the
// window-wide wiring (applets, the dBm strip, the pan's first view), and a
// pan move or rehost rebuilds Slice A's flag here like any other.
//
// What's intentionally NOT done here: per-slice rxChannel/CTUN/MaxBin/NR/ANF
// DSP wiring (those hardcode rxChannel(0) today and are the actual Phase 3F
// multi-slice DSP epic). This is "make the flag appear + let the operator
// interact + keep it in sync with its SliceModel".
//
// Mirrors AetherSDR's addVfoWidget()+wireVfoWidget() pair (MainWindow.cpp:
// 11583 + 13968 [@6a142807]).
VfoWidget* MainWindow::createSliceFlag(SliceModel* slice, SpectrumWidget* sw)
{
    if (!slice || !sw || !m_radioModel) { return nullptr; }
    const int sliceIndex = slice->sliceIndex();
    if (m_vfoWidgetsBySlice.contains(sliceIndex)) {
        return m_vfoWidgetsBySlice.value(sliceIndex);
    }

    VfoWidget* newFlag = sw->addVfoWidget(sliceIndex);
    if (!newFlag) { return nullptr; }
    m_vfoWidgetsBySlice.insert(sliceIndex, newFlag);

    // Initial slice state push so the flag paints the right freq/mode/filter
    // on first show (mirrors wireSliceToSpectrum for Slice A).
    newFlag->setSlice(slice);
    newFlag->setFrequency(slice->frequency());
    // Seed the HOSTING widget's VFO marker too, not just the flag's own text.
    // These are separate state: the flag paints the digits, but the pan places
    // the flag from its own VFO frequency. setVfoFrequency was only ever
    // called on activeSpectrumWidget(), so a slice on any other pan left that
    // pan's marker at its 0 Hz default -- the flag was positioned off the left
    // edge and the pan showed the "<| 0.0000" off-screen-VFO chevron instead.
    // Bench-caught 2026-07-26: looked exactly like the flag was never created.
    if (markerOnlyPlacement(sliceIndex)) {
        // A listened slice a layout change placed here: its flag and its own
        // edge marker, never this pan's VFO.
        sw->setEdgeMarkedSlice(sliceIndex, true);
    } else {
        sw->setVfoFrequency(slice->frequency());
    }
    newFlag->setMode(slice->dspMode());

    // AUTO AGC-T on this flag. The toggle and its visual feedback were wired
    // only in wireSliceToSpectrum -- Slice A's path -- so the AUTO button on
    // every other flag did nothing when clicked and never lit. Bench-caught
    // 2026-07-26 on a four-slice layout.
    //
    // Noise floor comes from THIS slice's stream, matching the auto-AGC tick:
    // the visual would otherwise report stream 0's floor next to a threshold
    // computed from the slice's own.
    connect(newFlag, &VfoWidget::autoAgcToggled,
            slice, &SliceModel::setAutoAgcEnabled);
    wireAutoAgcVisuals(m_radioModel, slice, newFlag, m_rxApplet);

    // R-R3-45: speakers or headphones. The route itself is wired in
    // VfoWidget::setSlice; the flag also needs to know whether this
    // computer has a headphones output open.
    if (AudioEngine* engine = m_radioModel->audioEngine()) {
        newFlag->setHeadphonesAvailable(engine->headphonesAvailable());
        connect(engine, &AudioEngine::headphonesAvailableChanged,
                newFlag, &VfoWidget::setHeadphonesAvailable);
        // Turned on but not open: "could not be opened", not "turn on".
        newFlag->setHeadphonesEnabled(engine->headphonesEnabled());
        connect(engine, &AudioEngine::headphonesEnabledChanged,
                newFlag, &VfoWidget::setHeadphonesEnabled);
    }
    // R-R3-45: in a remote window, the Core's side too: a Core that cannot
    // send the headphones mix, or headphones that failed here.
    if (m_remoteMedia) {
        newFlag->setHeadphonesProblem(m_remoteMedia->headphonesProblem());
        connect(m_remoteMedia, &RemoteMediaController::headphonesProblemChanged,
                newFlag, &VfoWidget::setHeadphonesProblem);
    }

    // Remote Daemon R2 Task 12: per-slice S-meter. SliceMeterPump
    // (src/core/meters/, owned by RadioModel) writes this slice's own
    // signalStrengthDbm directly, so the flag can listen to that property's
    // own NOTIFY instead of filtering a shared MeterPoller signal by slice
    // id (the previous sliceSmeterUpdated(int, double) mechanism, which
    // this replaces). Seed the current value immediately so a newly
    // created flag does not show a stale reading until the next poll tick,
    // matching the seeding already done above for frequency/mode/filter/etc.
    if (m_radioModel->role() == RadioModel::Role::Remote) {
        // Core sends independent peak and average readings. MeterPoller
        // selects the applet's current source for every mirrored flag too.
        connect(m_meterPoller, &MeterPoller::remoteSliceLevelUpdated,
                newFlag, [newFlag, id = slice->sliceIndex()](int sliceId, double dbm) {
            if (sliceId == id) { newFlag->setSmeter(dbm); }
        });
    } else {
        connect(slice, &SliceModel::signalStrengthDbmChanged, newFlag, &VfoWidget::setSmeter);
    }
    newFlag->setSmeter(slice->signalStrengthDbm());
    newFlag->setFilter(slice->filterLow(), slice->filterHigh());
    newFlag->setAgcMode(slice->agcMode());
    newFlag->setAfGain(slice->afGain());
    newFlag->setRfGain(slice->rfGain());
    newFlag->setRxAntenna(slice->rxAntenna());
    newFlag->setTxAntenna(slice->txAntenna());
    newFlag->setStepHz(slice->stepHz());
    newFlag->setBoardCapabilities(m_radioModel->boardCapabilities());
    newFlag->setHpsdrSku(m_radioModel->hardwareProfile().model);
    // VFO flag crash lane fix round (2026-10-01): moved here from
    // wireSliceToSpectrum, where only Slice A's first flag had it. A flag
    // rebuilt on a pan move (Slice A's included) keeps following the radio.
    connect(m_radioModel, &RadioModel::currentRadioChanged, newFlag,
            [this, newFlag]() {
        newFlag->setBoardCapabilities(m_radioModel->boardCapabilities());
        newFlag->setHpsdrSku(m_radioModel->hardwareProfile().model);
    });
    // Group B fix wave: BYPS shows and writes the radio's RX bypass on TX
    // through the Alex facade: this computer's AlexController locally, the
    // Core's in a remote window (radioHardwareVersion 5).
    newFlag->setRxBypassActive(m_radioModel->alexAntennaFacade()->rxOutOnTx());
    newFlag->setFilterPresetStore(m_radioModel->filterPresetStore());
    // Phase 3F closeout — give the per-slice VfoWidget the RadioModel pointer
    // so its right-click antenna submenu builds AntennaPickerMenu with live
    // caps + alex + slice (instead of the stub ANT1/ANT2 list).
    newFlag->setRadioModel(m_radioModel);
    // Slice control plan Task 11 fix: the TX button also needs this
    // window to be allowed to move transmit (transmitSliceChoiceReason).
    applyFlagTransmitGate(newFlag);
    newFlag->setRxBypassPermitted(rxBypassPermitted(), rxBypassUnavailableReason());
    wireRadeFlagForTest(m_radioModel, newFlag, sliceIndex);
    if (TxSliceArbiter* arb = m_radioModel->txSliceArbiter()) {
        newFlag->setTxSlice(arb->txBoundSliceId() == sliceIndex);
    }
    // iPhone app plan Task 78 (ruling 5.4a): a remote window's flag shows TX
    // as the Core marks it, only while this window holds transmit on this
    // slice; its arbiter binds nothing.
    if (!m_radioModel->ownsLocalDsp()) {
        newFlag->setTxSlice(slice->isTxSlice());
        const QPointer<SliceModel> tracked(slice);
        connect(slice, &SliceModel::txSliceChanged, newFlag, [this, newFlag, tracked]() {
            if (tracked) { newFlag->setTxSlice(tracked->isTxSlice()); }
            // TX badge take: a listened slice on the air holds the badge.
            applyFlagTransmitGate(newFlag);
        });
        refreshRemoteDeviceScreens();
    }
    // Which slice is selected decides this flag's marker colours on the pan.
    // Seeded here because the selection can predate the flag (a slice that
    // migrated pans, or one selected before its flag was built); the
    // RadioModel::activeSliceChanged fan-out in buildUI(), next to the TX
    // badge's, keeps it current after that.
    newFlag->setActiveSlice(m_radioModel->activeSlice() == slice);

    // --- Intent signals (Sub-Epic C T9 + Sub-Epic E T4 mirror) ---
    // Slice control plan Task 11 fix: the TX applet's letters' path, so a
    // remote window asks the Core (tx.setTxSlice) instead of moving its own
    // model's arbiter, which binds nothing.
    connect(newFlag, &VfoWidget::txHandoffRequested, this,
            [this](int idx) { activateTransmitSlice(idx, false); });
    // TX badge take (JJ's ruling, 2026-09-30).
    connect(newFlag, &VfoWidget::txTakeRequested, this,
            [this](int idx) { activateTransmitSlice(idx, false); });
    // Phase 3F Sub-Epic I closeout, defect G2: route to the slice's DDC
    // stream. This used to write SliceModel::setSampleRateHz, which stopped
    // reaching the wire once buildStreamConfigsForCodec began sourcing the
    // rate from the allocator, so the menu did nothing. requestSliceSampleRate
    // also resolves by slice ID rather than list position, which is what
    // VfoWidget actually carries.
    connect(newFlag, &VfoWidget::sampleRateRequested, this,
            [this](int sliceId, int hz) {
        if (!m_radioModel) { return; }
        m_radioModel->requestSliceSampleRate(sliceId, hz);
    });
    connect(newFlag, &VfoWidget::filterPolicyRequested, this,
            [this](int chainIdx) {
        if (!m_radioModel) { return; }
        FilterPolicyDialog dlg(chainIdx, m_radioModel, this);
        dlg.exec();
    });
    connect(newFlag, &VfoWidget::removeSliceRequested, this,
            [this](int idx) { closeSliceForWindow(idx); });
    connect(newFlag, &VfoWidget::diversityRequested,
            this, &MainWindow::openDiversityDialog);
    // Phase 3F (Bug 2): the floating ✕ close button emits closeRequested.
    // Wire it to removeSlice so operators can dismiss a flag they opened.
    // (removeSliceRequested above is the right-click-menu path; this is the
    // dedicated button. Both land on RadioModel::removeSlice, which refuses
    // to remove the last remaining slice.)
    connect(newFlag, &VfoWidget::closeRequested, this,
            [this](int idx) { closeSliceForWindow(idx); });
    // Slice control plan Task 14a: the flag's access actions are the
    // chooser's requests (the close button of a listened flag is Stop
    // listening).
    connect(newFlag, &VfoWidget::takeControlRequested, this,
            [this](int idx) { runFlagAccessAction(SliceChooserAction::TakeControl, idx); });
    connect(newFlag, &VfoWidget::releaseRequested, this,
            [this](int idx) { runFlagAccessAction(SliceChooserAction::Release, idx); });
    connect(newFlag, &VfoWidget::stopListeningRequested, this,
            [this](int idx) { runFlagAccessAction(SliceChooserAction::StopListening, idx); });
    // Task 14b: "Your volume" and Mute on a listened flag set this device's
    // own listening level, never the slice's AF or mute.
    connect(newFlag, &VfoWidget::listenVolumeRequested, this,
            [this](int idx, int level, bool muted) { setFlagListenVolume(idx, level, muted); });
    // Phase 3F (Bug 3): clicking this flag activates its slice so the RX
    // applet, the pan the flag sits on, and every other active-slice surface
    // follow it. Every flag runs through here, Slice A's included, since the
    // flag-path unification made createSliceFlag the one builder. Mirrors
    // AetherSDR's VfoWidget::sliceActivationRequested -> setActiveSlice
    // (wireVfoWidget, MainWindow.cpp:14076 [@6a142807]).
    //
    // setActiveSliceById, not setActiveSlice: VfoWidget carries the value
    // createSliceFlag stamped from SliceModel::sliceIndex(), a stable slice
    // ID, while setActiveSlice indexes m_slices positionally. With A(0) B(1)
    // C(2), closing B leaves C at id 2 / position 1, and the unconverted call
    // asked for position 2 of a two-element list -- so clicking flag C
    // selected nothing at all.
    connect(newFlag, &VfoWidget::sliceActivationRequested, this,
            [this](int sliceId) { selectSliceForWindow(sliceId); });
    // Phase 3F closeout — AntennaPickerMenu selection forwards to
    // SliceModel::setRxAntenna. Sub-Epic E Task 5 consumer wire-up.
    connect(newFlag, &VfoWidget::antennaChangeRequested, this,
            [this](int idx, const QString& antName) {
        applyAntennaChangeForTest(m_radioModel, idx, antName);
    });

    // --- VfoWidget -> SliceModel (user click propagates to model) ---
    //
    // VFO flag crash lane (2026-09-30): every flag-to-slice edge below has
    // the SLICE as its context object, so Qt drops it when the slice is
    // freed. These used `this` (the window) as context while capturing the
    // raw slice, so a flag that outlived its slice -- Slice A's, closed from
    // the Core in a remote window -- wrote the freed SliceModel on its next
    // click: JJ's crash in SliceModel::lockedChanged from the lock button.
    connect(newFlag, &VfoWidget::frequencyChanged, slice,
            [slice](double hz) { slice->setFrequency(hz); });
    connect(newFlag, &VfoWidget::modeChanged, slice,
            [slice](DSPMode mode) { slice->setDspMode(mode); });
    connect(newFlag, &VfoWidget::filterChanged, slice,
            [slice](int low, int high) { slice->setFilter(low, high); });
    connect(newFlag, &VfoWidget::agcModeChanged, slice,
            [slice](AGCMode mode) { slice->setAgcMode(mode); });
    connect(newFlag, &VfoWidget::afGainChanged, slice,
            [slice](int gain) { slice->setAfGain(gain); });
    connect(newFlag, &VfoWidget::rfGainChanged, slice,
            [slice](int gain) { slice->setRfGain(gain); });
    connect(newFlag, &VfoWidget::rxAntennaChanged, slice,
            [slice](const QString& ant) { slice->setRxAntenna(ant); });
    connect(newFlag, &VfoWidget::txAntennaChanged, slice,
            [slice](const QString& ant) { slice->setTxAntenna(ant); });

    // --- SliceModel -> VfoWidget (model updates repaint the flag) ---
    //
    // R-R3-30: every handler here is connected with the flag as its context
    // (wireSliceFlagStatePresentation), so removing or rehoming the flag
    // retires it. With MainWindow as the context, each rehome through
    // panKeyChanged added one more frequency, mode and filter handler, and a
    // deleted flag's handlers stayed live. The hooks below are the host-side
    // half; the binding repaints the flag before calling them.
    SliceFlagHostHooks hostHooks;
    hostHooks.frequencyChanged = [this, slice](double hz) {
        if (m_handlingBandJump) { return; }
        // Keep the hosting pan's VFO marker on this slice as it tunes.
        // Resolved per-call rather than captured, because a slice can migrate
        // to another pan and the flag follows it there.
        SpectrumWidget* host = spectrumForSlice(slice);
        if (!host) { return; }
        if (markerOnlyPlacement(slice->sliceIndex())) {
            // A listened slice a layout change placed here moves only its
            // own flag and edge marker: never this pan's VFO, view or DDC
            // centre, and never its own demodulator shift, which
            // bindSliceToStream set from its own stream.
            host->setEdgeMarkedSlice(slice->sliceIndex(), true);
            host->refreshSliceFlags();
            return;
        }
        // On a pan that is not the slice's own, the demodulator shift comes
        // from the slice's own stream, never from this pan's view: that pan
        // shows another stream, and its geometry would put the slice off
        // frequency for the device that controls it.
        const bool foreignHost = hostIsNotSlicesOwnPan(slice);

        // Band jump: the slice moved outside what this pan is showing, so the
        // pan has to follow it. Restored after the flag-path unification
        // dropped the handler that carried it -- nothing set m_handlingBandJump
        // afterwards, so a slice tuned to another band left its pan behind.
        //
        // Per pan now, and using this slice's own WDSP channel rather than the
        // hardcoded rxChannel(0) the single-pan version used. The DDC retune
        // itself lives in RadioModel::bindSliceToStream (Sub-Epic I) and is not
        // duplicated here; this is the DISPLAY half.
        const double center = host->centerFrequency();
        const double halfBw = host->bandwidth() / 2.0;
        const bool offScreen = (hz < center - halfBw) || (hz > center + halfBw);
        if (!m_radioModel->ownsLocalDsp()) {
            // Core alone moves the DDC and applies the demodulator shift.
            // Following a mirrored VFO must not echo an explicit pan gesture.
            if (!host->ctunEnabled() || offScreen) {
                host->setDisplayWindowPreservingHistory(hz, host->bandwidth());
            }
            host->setVfoFrequency(hz);
            return;
        }
        if (!host->ctunEnabled() || offScreen) {
            m_handlingBandJump = true;
            const bool wasCtun = host->ctunEnabled();
            if (m_radioModel->receiverManager()) {
                m_radioModel->receiverManager()->setDdcFrequencyLocked(false);
            }
            host->setCenterFrequency(hz);
            host->setDdcCenterFrequency(hz);
            // Phase 3F Sub-Epic J Task 11: resolved through RadioModel's
            // accessor rather than wdspEngine()->rxChannel() directly --
            // src/gui/ no longer reaches into WdspEngine for a channel.
            // The model and WDSP together (R-R3-49): the pan now sits on
            // the slice, so its offset is zero in both halves.
            m_radioModel->applySliceStreamCentre(
                slice, foreignHost ? m_radioModel->streamCentreHz(slice->streamIndex()) : hz);
            if (wasCtun && m_radioModel->receiverManager()) {
                m_radioModel->receiverManager()->setDdcFrequencyLocked(true);
            }
            m_handlingBandJump = false;
        } else {
            // CTUN, still on-screen: the DDC stays put and WDSP shifts.
            // Written to the model and WDSP together (R-R3-49), so the
            // slice's shiftOffsetHz names the centre the demodulator uses.
            m_radioModel->applySliceStreamCentre(
                slice, foreignHost ? m_radioModel->streamCentreHz(slice->streamIndex()) : center);
        }
        host->setVfoFrequency(hz);
    };
    // Mode and filter drive the flag's LABELS and this pan's PASSBAND.
    //
    // The passband half was lost when the two flag-wiring paths were unified:
    // wireSliceToSpectrum had `activeSpectrumWidget()->setFilterOffset(low,
    // high)` and `setTxMode(mode)`, and the shared path only ever set the
    // flag's text -- so the shaded passband stopped following the filter and
    // never flipped to the other side of the dial on a USB/LSB change.
    // Bench-caught immediately, 2026-07-26.
    //
    // Resolved through spectrumForSlice per call, so the passband lands on the
    // pan hosting this slice rather than on whichever pan is active.
    hostHooks.modeChanged = [this, slice](DSPMode mode) {
        // The pan's passband and TX mode belong to the pan's VFO, which a
        // slice placed there only as a marker never takes.
        if (markerOnlyPlacement(slice->sliceIndex())) { return; }
        if (SpectrumWidget* host = spectrumForSlice(slice)) {
            // TX filter overlay maps audio Hz to the right sideband from this.
            host->setTxMode(mode);
            // Re-push the passband: the sideband, and therefore the sign of
            // the offsets, changes with the mode.
            host->setFilterOffset(slice->filterLow(), slice->filterHigh());
        }
    };
    hostHooks.filterChanged = [this, slice](int low, int high) {
        if (markerOnlyPlacement(slice->sliceIndex())) { return; }
        if (SpectrumWidget* host = spectrumForSlice(slice)) {
            host->setFilterOffset(low, high);
        }
    };
    wireSliceFlagStatePresentation(slice, newFlag, std::move(hostHooks));


    // ---- Moved from wireSliceToSpectrum (Slice A's private path) ----
    //
    // These were wired only for Slice A, so on every other flag Mute, BIN,
    // SQL, the AGC-T slider, Pan, RIT/XIT, NR, NB, SNB, ANF, APF, Lock and
    // the setup shortcuts did nothing at all: the flag never reached the
    // model, so the per-slice model->WDSP work could not help them.
    // createSliceFlag is now the single place a flag is wired.
    connect(newFlag, &VfoWidget::rxBypassToggled,
            m_radioModel->alexAntennaFacade(), &AlexAntennaFacade::setRxOutOnTx);
    connect(m_radioModel->alexAntennaFacade(), &AlexAntennaFacade::rxOutOnTxChanged,
            newFlag, &VfoWidget::setRxBypassActive);
    connect(slice, &SliceModel::lastRadeRxCallsignChanged,
            newFlag, &VfoWidget::setRadeCallsign);
    // VFO flag crash lane fix round (2026-10-01): the seed of the cached
    // callsign, moved here from wireSliceToSpectrum (its 2026-05-11 bench
    // comment there gives the reason): a slice that already holds a decoded
    // callsign paints it on the flag's first show.
    newFlag->setRadeCallsign(slice->lastRadeRxCallsign());
    wireSliceFlagPresentation(slice, newFlag);
    connect(slice, &SliceModel::nbModeChanged, newFlag, &VfoWidget::setNbMode);
    newFlag->setNbMode(slice->nbMode());   // initial sync
    connect(newFlag, &VfoWidget::txFilterMatchRequested, this,
            [this](int audioLow, int audioHigh) {
        // R-R3-49 (parity Task 1): the TX passband is a transmit setting.
        if (!transmitSettingsPermitted()) {
            showToast(transmitSettingsReason(), ToastSeverity::Info, 3000);
            return;
        }
        m_radioModel->transmitModel().setFilterLow(audioLow);
        m_radioModel->transmitModel().setFilterHigh(audioHigh);
    });
    connect(newFlag, &VfoWidget::nbModeCycled, slice, [slice] {
        slice->setNbMode(NereusSDR::cycleNbMode(slice->nbMode()));
    });
    connect(newFlag, &VfoWidget::anfChanged, slice, [slice](bool on) {
        slice->setAnfEnabled(on);
    });
    connect(newFlag, &VfoWidget::nr2Changed, slice, [slice](bool on) {
        // NR2 = EMNR in Thetis naming. Toggle: NR2→active clears any other slot.
        slice->setActiveNr(on ? NereusSDR::NrSlot::NR2 : NereusSDR::NrSlot::Off);
    });
    connect(newFlag, &VfoWidget::snbChanged, slice, [slice](bool on) {
        slice->setSnbEnabled(on);
    });
    connect(newFlag, &VfoWidget::apfChanged, slice, [slice](bool on) {
        slice->setApfEnabled(on);
    });
    connect(newFlag, &VfoWidget::apfTuneHzChanged, slice, [slice](int hz) {
        slice->setApfTuneHz(hz);
    });
    connect(newFlag, &VfoWidget::muteChanged, slice, [slice](bool v) {
        slice->setMuted(v);
    });
    connect(newFlag, &VfoWidget::panChanged, slice, [slice](double p) {
        slice->setAudioPan(p);
    });
    connect(newFlag, &VfoWidget::squelchEnabledChanged, slice, [slice](bool v) {
        slice->setSsqlEnabled(v);
    });
    connect(newFlag, &VfoWidget::squelchThreshChanged, slice, [slice](int v) {
        slice->setSsqlThresh(static_cast<double>(v));
    });
    connect(newFlag, &VfoWidget::agcThreshChanged, slice, [slice](int v) {
        slice->setAgcThreshold(v);
    });
    connect(newFlag, &VfoWidget::binauralChanged, slice, [slice](bool v) {
        slice->setBinauralEnabled(v);
    });
    connect(newFlag, &VfoWidget::quickModeRequested, slice, [slice](int index) {
        // Quick-mode buttons: 0=USB, 1=CW, 2=DIG (matching AetherSDR defaults)
        static constexpr DSPMode kQuickModes[] = {DSPMode::USB, DSPMode::CWU, DSPMode::DIGU};
        if (index >= 0 && index < 3) {
            slice->setDspMode(kQuickModes[index]);
        }
    });
    connect(newFlag, &VfoWidget::openSetupRequested, slice, [this, slice]() {
        if (desktopHosting()) {
            if (!desktopSliceAllowed(slice->sliceIndex())) { return; }
            if (!selectSliceForWindow(slice->sliceIndex())) { return; }
        }
        auto* dialog = createSetupDialog();
        if (dialog == nullptr) {
            return;  // the gate refused and has already said why
        }
        dialog->selectPage(QStringLiteral("AGC/ALC"));
        dialog->show();
    });
    connect(newFlag, &VfoWidget::openNbSetupRequested, slice, [this, slice]() {
        if (desktopHosting()) {
            if (!desktopSliceAllowed(slice->sliceIndex())) { return; }
            if (!selectSliceForWindow(slice->sliceIndex())) { return; }
        }
        auto* dialog = createSetupDialog();
        if (dialog == nullptr) {
            return;  // the gate refused and has already said why
        }
        dialog->selectPage(QStringLiteral("NB/SNB"));
        dialog->show();
    });
    connect(newFlag, &VfoWidget::openNrSetupForSliceRequested, this,
            [this](NereusSDR::NrSlot slot, int sliceId) {
        if (!m_radioModel->sliceById(sliceId)) {
            return;
        }
        if (desktopHosting()) {
            if (!desktopSliceAllowed(sliceId)) { return; }
            if (!selectSliceForWindow(sliceId)) {
                return;
            }
        }
        auto* dialog = createSetupDialog();
        if (dialog == nullptr) {
            return;  // the gate refused and has already said why
        }
        dialog->selectPage(QStringLiteral("NR/ANF"));
        // Deep-link to the sub-tab matching the NR slot the user clicked
        // (Task 18 polish 2026-04-23 — previously always opened NR1).
        if (auto* nrPage = dialog->findChild<NrAnfSetupPage*>()) {
            nrPage->selectSubtab(slot, sliceId);
        }
        dialog->show();
    });
    connect(newFlag, &VfoWidget::openNnrModelsRequested, this, [this](int sliceId) {
        if (!m_radioModel->sliceById(sliceId)) {
            return;
        }
        auto* dialog = new DspAssetDialog(m_radioModel, DspAssetKind::NnrModel, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    });
    connect(newFlag, &VfoWidget::ritEnabledChanged, slice, [slice](bool on) {
        slice->setRitEnabled(on);
    });
    connect(newFlag, &VfoWidget::ritHzChanged, slice, [slice](int hz) {
        slice->setRitHz(hz);
    });
    connect(newFlag, &VfoWidget::xitEnabledChanged, slice, [slice](bool on) {
        slice->setXitEnabled(on);
    });
    connect(newFlag, &VfoWidget::xitHzChanged, slice, [slice](int hz) {
        slice->setXitHz(hz);
    });
    // From Thetis console.cs:29034-29038 [v2.10.3.15]: a left click on the
    // step display (WheelTune_MouseDown) calls ChangeTuneStepUp, which wraps
    // from the last tune_step_list entry back to the first.
    // changeTuneStepUp emits stepHzChanged, which updates this flag's STEP
    // label through the stepHzChanged connection made earlier in createSliceFlag.
    connect(newFlag, &VfoWidget::stepCycleRequested, slice, [slice]() {
        slice->changeTuneStepUp();
    });
    connect(newFlag, &VfoWidget::lockChanged, slice, [slice](bool locked) {
        slice->setLocked(locked);
    });
    if (desktopHosting()) {
        SliceOwnership* ownership = m_radioModel->sliceOwnership();
        const bool mine =
            SliceAccessPolicy::mayChange(*ownership, SliceOwnership::stationDevice(), sliceIndex)
            && !ownership->mark(sliceIndex).isHeld();
        // Task 14a: a slice this window listens to shows its flag too.
        const bool listened = !mine && desktopListensTo(sliceIndex);
        newFlag->setStationPresentationAllowed(mine || listened);
        newFlag->setTxSlice(mine && desktopOwnsTransmit()
            && m_radioModel->txSliceArbiter()
            && m_radioModel->txSliceArbiter()->txBoundSliceId() == sliceIndex);
    }
    connect(slice, &SliceModel::frequencyChanged, newFlag,
            [this](double) { if (desktopHosting()) { refreshForeignMarkers(); } });
    connect(slice, &SliceModel::filterChanged, newFlag,
            [this](int, int) { if (desktopHosting()) { refreshForeignMarkers(); } });
    connect(slice, &SliceModel::txSliceChanged, newFlag,
            [this](bool) { if (desktopHosting()) { refreshForeignMarkers(); } });
    refreshForeignMarkers();
    // Task 14a: the new flag says who controls its slice.
    refreshSliceChooser();
    return newFlag;
}

// R1 Task 6: out-of-line because FftEnginePool is only forward-declared in
// MainWindow.h (m_fftEnginePool is a pointer member); this .cpp includes the
// full "core/spectrum/FftEnginePool.h", so the method call resolves here.
FFTEngine* MainWindow::primaryFftEngine() const
{
    return m_fftEnginePool ? m_fftEnginePool->engineForStream(0) : nullptr;
}

// Fix round 1 finding 1 (coordinator spec review): re-reads the four
// display AppSettings keys and pushes them to the pool. This is the ONLY
// thing MainWindow does with those keys now -- the clamping / fallback
// logic is unchanged from what createFftEngineForStream used to do inline,
// just moved here so it can run more than once. Called from
// ensureStreamWired() immediately before a stream that does not exist yet
// is built, so a stream created after a live Setup -> Display change picks
// up the CURRENT value rather than whatever was true the first time this
// ran. Deliberately NOT inside FftEnginePool itself: the pool must stay
// settings-agnostic (design section 9.4a) so the Task 9/10 daemon can
// supply its own config with no AppSettings dependency in src/core at all.
//
// Fix round 2 (coordinator spec review): calls setConfigForNewStreams(),
// NOT setConfig(). setConfig() retroactively re-applies to every existing
// engine, which is fine for a caller with nothing else that can diverge an
// engine from it, but AppSettings is not the only source of truth for
// fftSize here -- the auto-zoom lambda below calls engine->setFftSize()
// directly on one stream's engine and deliberately never persists that
// override. Calling setConfig() from here would have silently snapped
// every OTHER, already-zoomed stream's engine back to the AppSettings
// baseline the instant any new stream appeared (e.g. RX1 zoomed to FFT
// 32768, user enables RX2: RX1's panadapter would silently jump back to
// the 4096 baseline with a visible replan pause, with zero user action on
// that pan). setConfigForNewStreams() only affects the stream about to be
// built, leaving every other engine's live state alone.
void MainWindow::wireAutoAgcVisuals(RadioModel* model, SliceModel* slice,
                                   VfoWidget* flag, RxApplet* applet)
{
    if (!model || !slice || !flag) { return; }
    const auto refresh = [modelRef = QPointer<RadioModel>(model),
                          sliceRef = QPointer<SliceModel>(slice),
                          flagRef = QPointer<VfoWidget>(flag),
                          appletRef = QPointer<RxApplet>(applet)] {
        if (!modelRef || !sliceRef || !flagRef) { return; }
        refreshAutoAgcVisuals(modelRef, sliceRef, flagRef, appletRef);
    };
    connect(slice, &SliceModel::autoAgcEnabledChanged, flag, refresh);
    connect(slice, &SliceModel::autoAgcOffsetChanged, flag, refresh);
    connect(slice, &SliceModel::stationAutoAgcNoiseFloorChanged, flag, refresh);
    connect(model, &RadioModel::connectionStateChanged, flag, refresh);
    connect(model, &RadioModel::activeSliceChanged, flag, refresh);
    refresh();
}

void MainWindow::refreshAutoAgcVisuals(RadioModel* model, SliceModel* slice,
                                      VfoWidget* flag, RxApplet* applet)
{
    if (!slice || !model) { return; }
    float floor = -200.0f;
    bool valid = false;
    if (model->role() == RadioModel::Role::Remote) {
        floor = static_cast<float>(slice->stationAutoAgcNoiseFloorDbm());
        valid = model->isConnected() && slice->stationAutoAgcNoiseFloorValid();
    } else if (const auto* tracker = model->noiseFloorTrackerForSlice(slice)) {
        floor = tracker->noiseFloor();
        valid = tracker->isGood();
    }
    if (flag) {
        flag->updateAgcAutoVisuals(slice->autoAgcEnabled(), floor,
                                   slice->autoAgcOffset(), valid);
    }
    if (applet && model->activeSlice() == slice) {
        applet->updateAgcAutoVisuals(slice->autoAgcEnabled(), floor,
                                       slice->autoAgcOffset(), valid);
    }
}

void MainWindow::refreshFftPoolConfig()
{
    if (!m_fftEnginePool) { return; }
    auto& s = AppSettings::instance();
    FftPoolConfig cfg;
    // The keys and their defaults come from ControlRanges.h, the table
    // Setup > Display and the Core's catalogue read.
    cfg.fps = qBound(1,
        s.value(QLatin1String(ControlRanges::kDisplaySpectrumFpsKey),
                QString::number(ControlRanges::kDisplaySpectrumFpsDefault)).toString().toInt(),
        60);
    cfg.fftSize = s.value(QLatin1String(ControlRanges::kDisplayFftSizeKey),
                          QString::number(ControlRanges::kDisplayFftSizeDefault))
                      .toString().toInt();
    // Fallback default matches FFTEngine's own constructor default
    // (WindowFunction::BlackmanHarris4 == 1).  The pre-extraction code
    // computed this by querying a freshly constructed engine's
    // windowFunction(); there is no throwaway engine to query here now
    // that engine construction lives inside the pool, so the equivalent
    // literal is used directly.
    static_assert(ControlRanges::kDisplayFftWindowDefault
                  == static_cast<int>(WindowFunction::BlackmanHarris4));
    const int defaultWin = ControlRanges::kDisplayFftWindowDefault;
    cfg.windowType = qBound(0,
        s.value(QLatin1String(ControlRanges::kDisplayFftWindowKey),
                QString::number(defaultWin)).toString().toInt(),
        static_cast<int>(WindowFunction::Count) - 1);
    cfg.hzPerBinTarget = s.value(QLatin1String(ControlRanges::kDisplayHzPerBinTargetKey),
                                 QString::number(ControlRanges::kDisplayHzPerBinTargetDefault))
                             .toString().toDouble();
    m_fftEnginePool->setConfigForNewStreams(cfg);
}

// ── Phase 3F Sub-Epic I Task 8: one FFTEngine per DDC stream ────────────────
//
// The panadapter belongs to the DDC, not to the sub-receiver: ChannelMaster
// holds a single `volatile long run_pan` per `_rcvr` alongside
// `audio[cmMAXSubRcvr]` (cmaster.h:75-82 [v2.10.3.15]).  So slices that share
// a DDC share one spectrum and appear as separate flags on it, and the engine
// pool is sized by stream, not by slice.
//
// R1 Task 6: engine creation/reuse, the four display AppSettings-sourced
// knobs, and thread parking moved into FftEnginePool. This function is what
// is left in MainWindow: the pool has no notion of RadioModel or of this
// stream's NoiseFloorTracker, so wiring both stays here, run once per
// stream the first time it is seen.
//
// Fix round 1 finding 1: refreshFftPoolConfig() runs here, before
// requesting the engine, and ONLY for a stream that does not exist yet --
// not on every call. Pre-extraction, createFftEngineForStream read
// AppSettings inside its own per-engine construction, so a stream built
// after a live Setup -> Display change picked up the current value. The
// pool's setConfig() is otherwise a one-time snapshot with no other call
// site (by design -- see FftEnginePool.h: the pool itself must stay
// settings-agnostic so the Task 9/10 daemon can supply its own config), so
// without this refresh here, every stream after the first would silently
// run on whatever was captured back at buildUI() time.
FFTEngine* MainWindow::ensureStreamWired(int streamIndex)
{
    if (!m_fftEnginePool) { return nullptr; }
    const bool isNewStream = !m_fftEnginePool->streams().contains(streamIndex);
    if (isNewStream) {
        refreshFftPoolConfig();
    }
    FFTEngine* engine = m_fftEnginePool->engineForStream(streamIndex);
    if (!engine || !isNewStream) { return engine; }

    // Sample rate is not one of FftEnginePool's four global display knobs
    // (DisplaySpectrumFps / DisplayFftSize / DisplayFftWindow /
    // DisplayHzPerBinTarget): it is per-stream and radio-connection-driven,
    // not a Setup -> Display setting, so it stays here. 768 kHz is the P2
    // default; RadioModel::wireSampleRateChanged and the streamCentreChanged
    // handler below update it to the actual wire rate on each connect
    // (P1=192k, P2=768k).
    engine->setSampleRate(768000.0);

    // Raw I/Q for this stream -> this engine.  The context object is the
    // ENGINE, not MainWindow, deliberately: RadioModel emits
    // rawIqDataForStream from the Connection thread (Lever 2, 2026-05-24,
    // RadioModel.cpp Step 2a), and an engine-scoped connection resolves to a
    // queued delivery straight onto the FFT thread.  Routing through a
    // MainWindow-scoped lambda instead would put every I/Q packet through the
    // main thread's event loop (~3200/s per stream at 768 kHz), silently
    // undoing that fix.  The index filter costs one compare on the FFT
    // thread; the alternative -- reading the pool's engines from the
    // Connection thread -- would need synchronisation AND lose Qt's
    // automatic disconnect-on-destroy, which is what makes this safe at
    // shutdown.
    connect(m_radioModel, &RadioModel::rawIqDataForStream, engine,
            [engine, streamIndex](int idx, const QVector<float>& samples) {
        if (idx != streamIndex) { return; }
        engine->feedIQ(samples);
    });

    // One NoiseFloorTracker per stream, fed by that stream's own FFT.
    //
    // Auto AGC-T derives its threshold from the noise floor, so a slice must
    // measure the band it is actually on. There used to be a single tracker
    // fed only by primaryFftEngine(), i.e. stream 0 -- fine while one slice
    // existed, but it would set a 20m slice's threshold from 40m's noise
    // floor once auto-AGC ran for every slice.
    auto* nf = new NoiseFloorTracker;
    m_streamNoiseFloors.insert(streamIndex, nf);
    if (m_radioModel) { m_radioModel->setStreamNoiseFloorTracker(streamIndex, nf); }
    connect(engine, &FFTEngine::fftReady, this,
            [nf](int, const QVector<float>& binsDbm) {
        static constexpr float kFrameIntervalMs = 33.0f;
        nf->feed(binsDbm, kFrameIntervalMs);
    });

    // Parity Task 18: Clarity reads the stream of the pan it tunes.
    connect(engine, &FFTEngine::fftReady, this,
            [this, streamIndex](int, const QVector<float>& binsDbm) {
        if (m_clarityController && streamIndex == clarityStreamIndex()) {
            m_clarityController->feedBins(binsDbm);
        }
    });

    return engine;
}

void MainWindow::applyStreamWindowToPan(const QString& panId, int streamIndex)
{
    if (!m_panStack) { return; }
    const auto it = m_streamWindows.constFind(streamIndex);
    if (it == m_streamWindows.constEnd()) { return; }
    SpectrumWidget* sw = m_panStack->spectrum(panId);
    if (!sw) { return; }
    sw->setDdcCenterFrequency(it->centreHz);
    if (it->sampleRateHz > 0) {
        sw->setSampleRate(static_cast<double>(it->sampleRateHz));
    }

    // Move the DISPLAY window onto the stream too, not just the DDC centre.
    //
    // setDdcCenterFrequency only tells the widget where the DDC sits for
    // bin-to-frequency mapping; the visible span is separate state, and a pan
    // created after startup keeps SliceModel's 14.225 MHz default. Bench-caught
    // 2026-07-26 on a 2v layout: pan-1 was correctly subscribed to a 7.265 MHz
    // stream while still displaying 14.2258 MHz, which
    //   - made visibleBinRange() select bins entirely outside the stream, so
    //     the waterfall rendered saturated (solid red), and
    //   - put the slice's flag at an x position far off the left edge, so the
    //     pan looked like it had no flag at all.
    //
    // Both symptoms are the same missing line. Recentre, preserving the pan's
    // current span so an operator's zoom is not thrown away -- only a pan that
    // has never been placed is actually moved, because pan-0 already sits on
    // its stream and this is a no-op there.
    // Span is clamped to what this pan may show. Preserving an arbitrarily
    // wider one would reintroduce the same failure at the edges: a pan left at
    // the 192 kHz default over a 48 kHz DDC has three quarters of its window
    // outside the data, which is exactly the out-of-range saturation this is
    // fixing.
    //
    // Through setDisplayWindowClamped, so the ceiling is the extended one when
    // extended view is allowed and the DDC rate otherwise. The clamp used to
    // be written out here against the stream width, which is the DDC rate and
    // nothing else, so binding a slice to a stream collapsed a restored
    // extended zoom right back onto the DDC. It was the third copy of that
    // same clamp on this branch, each found in a different review round; the
    // shared helper is what stops a fourth. Codex, PR #318.
    sw->setDisplayWindowClamped(it->centreHz, sw->bandwidth());
}

// Phase 3F: WIDE badge fan-out. See RadioModel::panBypassState for the
// routing (pan -> slices -> stream -> ADC -> BpfEffective) and for the
// per-cause reason strings.
//
// Every pan is refreshed on every pass, not just the ones that changed. A
// bypass is a property of the CHAIN, so one slice crossing a band edge can
// flip the badge on pans that do not host it and never saw an event of
// their own. Rechecking all of them is one query per pan against
// single-digit slice and pan counts.
void MainWindow::refreshPanWideBadges()
{
    if (!m_panStack || !m_radioModel) { return; }
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        const RadioModel::PanBypassState st =
            m_radioModel->panBypassState(applet->associatedSlices());
        applet->setWideBpf(st.bypassed, st.reason);
    }
}

// Phase 3F: status-overlay fan-out. Sibling of refreshPanWideBadges above,
// and deliberately the same shape: ask the model per pan, push the answer at
// that pan, never reach into the widget tree.
//
// Which slice a pan shows is its OWN activeSliceIndex, not the globally
// active slice -- per Sub-Epic E plan Task 2 Step 4. A pan hosting several
// slices has one of them active (PanadapterApplet::addSlice seeds it,
// removeSlice re-picks), and a global read would make every pan on a
// multi-pan layout paint identical text, which is the one thing a per-pan
// overlay exists not to do.
//
// updateStatusOverlay had zero callers before this, so every pan painted the
// widget's construction-time placeholders -- slice "A", "0.000", "USB",
// "CH 0" -- on a live radio.
void MainWindow::refreshPanStatusOverlays()
{
    if (!m_panStack || !m_radioModel) { return; }
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        const int sliceId = applet->activeSliceIndex();
        SliceModel* slice = m_radioModel->sliceById(sliceId);
        // A pan with no slices keeps whatever it last painted rather than
        // being blanked: the operator is mid-drag between pans and a flash
        // to placeholder text reads as a fault.
        if (!slice) { continue; }
        const int chain = m_radioModel->sliceChainIndex(sliceId);
        applet->updateStatusOverlay(slice, chain);

        // Extended-pan wings read one ADC's wideband bins, and
        // widebandSpectrumReady fans every ADC at every pan, so the pan has
        // to be told which one is its own.
        //
        // The PHYSICAL ADC, not the chain. An earlier version of this passed
        // `chain` on the grounds that it is the number RadioModel hands
        // setWidebandEnabled. That is true and it is still the wrong number
        // to paint by: chainForStream deliberately folds ADC1 onto chain 0 on
        // a board with more ADCs than preselector banks (ANAN-100D,
        // ANAN-200D), while widebandSpectrumReady carries the physical index
        // straight off the wire. On those boards a pan fed by ADC1 drew
        // ADC0's survey in its wings either side of a perfectly correct DDC
        // island. Found by Codex on PR #318.
        //
        // -1 (slice bound to no stream) leaves the last answer alone rather
        // than snapping the wings to ADC0.
        const int adc = m_radioModel->sliceAdcIndex(sliceId);
        if (adc >= 0 && applet->spectrumWidget()) {
            applet->spectrumWidget()->setWidebandAdcIndex(adc);
        }
    }
}

void MainWindow::wirePanStatusOverlayTriggers()
{
    if (!m_panStack) { return; }
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        connect(applet, &PanadapterApplet::activeSliceChanged,
                this, &MainWindow::refreshPanStatusOverlays,
                Qt::UniqueConnection);
    }
}

// TNF: notch-marker fan-out. Fourth sibling of refreshPanWideBadges,
// refreshPanStatusOverlays and wirePanBadgeHandlers, on the same hook and
// the same shape: ask the model once, push the answer at every pan.
//
// Every pan is refreshed on every pass because the notch list is GLOBAL
// (design D1): a notch added from one pan is a notch on all of them. This is
// deliberately not the spot overlay's activeSpectrumWidget() push, which
// binds one widget forever and would leave every secondary pan blank.
//
// This is the ONLY Hz-to-MHz conversion site in the TNF stack. NotchModel
// stores absolute RF Hz; NotchMarker::freqMhz is MHz; the five interaction
// signals coming back the other way are Hz again.
void MainWindow::refreshPanNotchMarkers()
{
    if (!m_panStack || !m_radioModel) { return; }
    NotchModel* notches = m_radioModel->notchModel();
    if (!notches) { return; }

    QVector<SpectrumWidget::NotchMarker> markers;
    markers.reserve(notches->notches().size());
    // `auto` here: the element type is obvious from notches(), and this
    // stays correct whichever scope the Notch value type is declared in.
    for (const auto& n : notches->notches()) {
        SpectrumWidget::NotchMarker m;
        m.id      = n.id;
        m.freqMhz = n.centerHz / 1.0e6;
        m.widthHz = n.widthHz;
        m.active  = n.active;
        markers.append(m);
    }

    const bool globalOn = notches->globalEnabled();
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        SpectrumWidget* sw = applet->spectrumWidget();
        if (!sw) { continue; }
        sw->setNotchMarkers(markers);
        sw->setNotchGlobalEnabled(globalOn);
    }
}

// TNF: visual-notch fan-out (design section 8.3). See the declaration for why
// every pan is refreshed on every pass.
void MainWindow::refreshPanVisualNotch()
{
    if (!m_panStack || !m_radioModel) { return; }
    NotchModel* notches = m_radioModel->notchModel();
    if (!notches) { return; }
    const bool on = notches->visualEnabled();
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        if (SpectrumWidget* sw = applet->spectrumWidget()) {
            sw->setVisualNotchEnabled(on);
        }
    }
}

// TNF: minimum-notch-width fan-out (design sections 7.2 and 8.3).
//
// WDSP recomputes the minimum on every read as
// 1600.0 / (nc / 256) * (rate / 48000): the wintype-0 arm of
// min_notch_width (third_party/wdsp/src/nbp.c:88), which is the arm that
// governs because nbp0 is created with wintype 0 (RXA.c:103). So it moves
// whenever the filter size or the channel rate does. Thetis has the same
// problem and
// solves it the same way, re-reading through UpdateMinimumNotchWidthRX and
// firing MinimumRXNotchWidthChangedHandlers (console.cs:48787-48818
// [v2.10.3.15]) from the DSP-options apply path at console.cs:39052-39053.
//
// A pan with no resolvable channel keeps whatever it last had rather than
// being reset: the alternative is a visible dent-width flicker every time the
// operator drags a slice between pans.
void MainWindow::refreshPanNotchMinWidth()
{
    if (!m_panStack || !m_radioModel) { return; }
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        SpectrumWidget* sw = applet->spectrumWidget();
        if (!sw) { continue; }
        // R-R3-49 (parity Task 16): a remote window has no channel of its
        // own; its slice carries the Core's channel's minimum (dspInfoVersion
        // 1), 0 until the Core says, which keeps what the pan last had.
        if (m_radioModel->role() == RadioModel::Role::Remote) {
            SliceModel* slice = m_radioModel->sliceById(applet->activeSliceIndex());
            if (!slice) { continue; }
            connect(slice, &SliceModel::minNotchWidthHzChanged,
                    this, &MainWindow::refreshPanNotchMinWidth,
                    Qt::UniqueConnection);
            if (slice->minNotchWidthHz() > 0.0) {
                sw->setNotchMinWidthHz(slice->minNotchWidthHz());
            }
            continue;
        }
        // Through RadioModel, not WdspEngine: scripts/verify-no-gui-dsp-
        // access.py fails the build on a bare rxChannel() from src/gui/.
        RxChannel* ch = m_radioModel->rxChannelForSlice(applet->activeSliceIndex());
        if (!ch) { continue; }
        // Re-armed every pass because the channel a pan resolves to changes
        // with the slice set. UniqueConnection makes the repeat a no-op, and
        // a destroyed channel drops its own connections.
        connect(ch, &RxChannel::minNotchWidthChanged,
                this, &MainWindow::refreshPanNotchMinWidth,
                Qt::UniqueConnection);
        sw->setNotchMinWidthHz(ch->minNotchWidthHz());
    }
}

void MainWindow::wirePanNotchHandlers()
{
    if (!m_panStack) { return; }
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        SpectrumWidget* sw = applet->spectrumWidget();
        if (!sw) { continue; }
        // The pan's own slice selection decides which channel's minimum
        // notch width it draws with, so a slice switch has to re-resolve it.
        connect(applet, &PanadapterApplet::activeSliceChanged,
                this, &MainWindow::refreshPanNotchMinWidth,
                Qt::UniqueConnection);
        // UniqueConnection requires a member slot. Qt rejects a lambda
        // here (and asserts in Debug), leaving notch-create disconnected.
        // Resolve the emitting pan at delivery, never through activeSlice.
        connect(sw, &SpectrumWidget::notchCreateRequested,
                this, &MainWindow::onPanNotchCreateRequested,
                Qt::UniqueConnection);
        connect(sw, &SpectrumWidget::notchMoveRequested,
                this, &MainWindow::onNotchMoveRequested,
                Qt::UniqueConnection);
        // Flush the coalesced notch push the moment a drag ends, so the
        // final position is exact rather than up to one coalescing window
        // stale. RadioModel::scheduleNotchEditPush explains the window.
        connect(sw, &SpectrumWidget::notchDragFinished,
                m_radioModel, &RadioModel::commitPendingNotchEdits,
                Qt::UniqueConnection);
        connect(sw, &SpectrumWidget::notchWidthRequested,
                this, &MainWindow::onNotchWidthRequested,
                Qt::UniqueConnection);
        connect(sw, &SpectrumWidget::notchActiveRequested,
                this, &MainWindow::onNotchActiveRequested,
                Qt::UniqueConnection);
        connect(sw, &SpectrumWidget::notchRemoveRequested,
                this, &MainWindow::onNotchRemoveRequested,
                Qt::UniqueConnection);
    }
}

void MainWindow::onPanNotchCreateRequested(double freqHz, bool narrow)
{
    SpectrumWidget* sw = qobject_cast<SpectrumWidget*>(sender());
    if (!sw || !m_panStack) { return; }
    for (PanadapterApplet* applet : m_panStack->allApplets()) {
        if (applet && applet->spectrumWidget() == sw) {
            onNotchCreateRequested(applet->panId(), freqHz, narrow);
            return;
        }
    }
}

void MainWindow::onNotchCreateRequested(const QString& panId, double freqHz,
                                       bool narrow)
{
    if (!m_radioModel) { return; }
    // Narrow is the Shift-held add. Both widths live on NotchModel because
    // they are Thetis constants (console.cs:40268-40269 [v2.10.3.15]).
    //
    // The slice comes from the pan that emitted the signal, not from
    // activeSlice(): clicking a pan activates it in PanadapterStack without
    // necessarily changing the active slice, so on two pans running different
    // filter sizes the clamp would resolve against the wrong channel. Standing
    // rule: a control drawn on a pan targets that pan. Codex review of PR #313.
    m_radioModel->addNotchForSlice(
        sliceForPan(panId), freqHz,
        narrow ? NotchModel::kNarrowNotchWidthHz
               : NotchModel::kDefaultNotchWidthHz);
}

void MainWindow::onNotchMoveRequested(int id, double newFreqHz)
{
    if (!m_radioModel || !m_radioModel->notchModel()) { return; }
    m_radioModel->notchModel()->setCenter(id, newFreqHz);
}

void MainWindow::onNotchWidthRequested(int id, double widthHz)
{
    if (!m_radioModel || !m_radioModel->notchModel()) { return; }
    m_radioModel->notchModel()->setWidth(id, widthHz);
}

void MainWindow::onNotchActiveRequested(int id, bool active)
{
    if (!m_radioModel || !m_radioModel->notchModel()) { return; }
    m_radioModel->notchModel()->setActive(id, active);
}

void MainWindow::onNotchRemoveRequested(int id)
{
    if (!m_radioModel || !m_radioModel->notchModel()) { return; }
    m_radioModel->notchModel()->removeNotch(id);
}

// The +TNF button on one pan's control strip. Distinct from
// onNotchCreateRequested above because a panadapter click already knows its
// frequency and this does not: the centre is composed from the pan's own
// slice, so a strip drawn on pan-2 notches pan-2's signal.
//
// From Thetis console.cs:40313-40331 [v2.10.3.15], TNFAdd(rx): VFO, plus RIT,
// shifted into the sideband, then AddNotch. The arithmetic lives in
// NotchModel::tnfAddCenterHz (design sections 7.5 and 10.2, which keeps the
// Thetis-derived maths out of the AetherSDR-registered overlay panel), and the
// admin-busy guard upstream repeats at console.cs:40315 is already enforced
// inside NotchModel::addNotch (console.cs:40224), so the reject path is the
// model's.
void MainWindow::onAddTnfClicked(const QString& panId)
{
    if (!m_radioModel) { return; }
    SliceModel* slice = sliceForPan(panId);
    if (!m_radioModel->notchModel() || !slice) { return; }
    // The centre and width live in RadioModel::addTnfForSlice, which the
    // Core's notch.addAtSlice runs too (R-IOS-27, R-IOS-06).
    m_radioModel->addTnfForSlice(slice);
}

// A rejected add is not a failure worth an error badge, but it must not be
// silent either: pressing +TNF twice on the same signal lands inside the 10 Hz
// dedupe window (console.cs:40259-40260 [v2.10.3.15]) and would otherwise do
// nothing at all, which reads as a dead button.
void MainWindow::onNotchAddRejected(const QString& reason)
{
    showToast(tnfAddRejectedNotice(OperatorReasonText::forDisplay(reason)),
              ToastSeverity::Warning, 3000);
}

// R-R3-21: a remote window's move, toggle or delete the Core refused (a notch
// another window already removed, or the Core's own TNF page mid-edit). The
// window has already put the Core's list back; this says why.
void MainWindow::onNotchRequestRefused(const QString& reason)
{
    showToast(OperatorReasonText::forDisplay(reason), ToastSeverity::Warning, 3000);
}

// R-R3-49, Sub-epic C-1 (tx-followup-4): the DSP > NR menu's entries, in
// order. BNR (NVIDIA) is not offered (operator, 2026-09-25); NrSlot::BNR
// keeps its value 6 so no saved or sent selection moves.
QList<std::pair<QString, NereusSDR::NrSlot>> MainWindow::nrMenuEntries()
{
    using Slot = NereusSDR::NrSlot;
    return {
        { QStringLiteral("&Off"),  Slot::Off  },
        { QStringLiteral("NR&1"),  Slot::NR1  },
        { QStringLiteral("NR&2"),  Slot::NR2  },
        { QStringLiteral("NR&3"),  Slot::NR3  },
        { QStringLiteral("NR&4"),  Slot::NR4  },
        { QStringLiteral("&DFNR"), Slot::DFNR },
        { QStringLiteral("&NNR"),  Slot::NNR  },
        { QStringLiteral("&MNR"),  Slot::MNR  },
    };
}

// Fix wave I3 (R-R3-21): a noise reducer a receiver would not turn on, for
// example NR3 on a Core with no NR3 model. The receiver is unchanged; the
// menu says why, in local and remote windows alike. Follow-up item 3: only
// for a choice made from this menu; a VFO flag click says why at the flag.
QString MainWindow::applyNrMenuChoice(SliceModel* slice, NereusSDR::NrSlot slot)
{
    if (!slice) {
        return {};
    }
    slice->setActiveNr(slot);
    if (slice->activeNr() == slot || slice->nnrLastError().isEmpty()) {
        return {};
    }
    return OperatorReasonText::forDisplay(slice->nnrLastError());
}

// Repaint the status-bar TNF light. Both halves of what it shows can move
// independently: the master enable from this label, the DSP menu or TCI, and
// the count from any pan's panadapter or the MNF settings page.
void MainWindow::refreshTnfIndicator()
{
    if (!m_tnfLabel || !m_radioModel) { return; }
    NotchModel* notches = m_radioModel->notchModel();
    if (!notches) { return; }
    const bool on    = notches->globalEnabled();
    const int  count = static_cast<int>(notches->notches().size());
    m_tnfLabel->setStyleSheet(tnfIndicatorStyleSheet(on, count));
    m_tnfLabel->setToolTip(tnfIndicatorTooltip(count, on));
}

// Phase 3F: badge-click fan-out. Third sibling of refreshPanWideBadges and
// wirePanStatusOverlayTriggers above, on the same hook and for the same
// reason: a pan that comes into existence after startup has to be wired
// without anyone remembering to wire it.
//
// The connects name member slots rather than lambdas on purpose. This runs
// again on every countChanged, so it has to be idempotent, and
// Qt::UniqueConnection is silently ignored for lambda targets in Qt6 -- a
// lambda here would stack one extra connection per layout switch and open
// that many FilterPolicyDialogs on a single click.
// ---------------------------------------------------------------------------
// ensureOverlayPanels — one control strip per panadapter.
//
// The strip used to be a single instance parented to pan-0's SpectrumWidget,
// so every other pan had no BAND / ANT / Display / +RX at all, and the one
// button that did exist had to guess which pan the operator meant. A control
// drawn on a pan acts on that pan: each strip now owns its panId and emits it,
// the same shape as AetherSDR's SpectrumOverlayMenu (SpectrumOverlayMenu.cpp:
// 292-315 [@c6481cbf], where +RX emits addRxClicked(m_panId)).
//
// Idempotent and re-armed from PanadapterStack::countChanged, so pans created
// by a layout switch or an Add Panadapter get their strip by construction
// rather than by remembering. Panels for pans that went away are dropped --
// the widget itself dies with its parent SpectrumWidget.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// wireSpectrumForPan — mouse interaction for one panadapter.
//
// Every SpectrumWidget signal used to be connected exactly once, to whatever
// activeSpectrumWidget() returned during startup -- pan-0's widget. The connect
// binds the OBJECT, not the expression, so it never re-resolved: on any other
// pan, click-to-tune, filter-edge drag, pan drag, zoom-replan and the dBm strip
// were all inert. The flag stayed live because it is wired per slice, which is
// why tuning appeared to work only with the pointer over the flag's digits.
//
// Resolves the slice per call through sliceForPan, so each pan drives its own
// slice, and uses that slice's WDSP channel rather than the hardcoded
// rxChannel(0) the single-pan path used.
// ---------------------------------------------------------------------------
// Push the live connection state into every pan's spectrum widget. Safe to
// call at any time; a pan created later is seeded by wireSpectrumForPan.
void MainWindow::pushConnectionStateToPans()
{
    if (!m_radioModel || !m_panStack) { return; }
    const ConnectionState st = m_radioModel->connectionState();
    const bool live = m_radioModel->isConnected();
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        if (SpectrumWidget* sw = m_panStack->spectrum(applet->panId())) {
            sw->setConnectionState(st);
            // Clearing stale history on disconnect was pan-0 only, so every
            // other pan kept painting the waterfall it had when the radio went
            // away -- indistinguishable from live data.
            if (!live) { sw->clearWaterfallHistory(); }
        }
    }
    refreshNoSliceHints();
}

void MainWindow::refreshNoSliceHints()
{
    if (!m_radioModel || !m_panStack) { return; }
    const bool slicesKnown = m_radioModel->ownsLocalDsp()
        || (m_stationClient && m_stationClient->isHandshakeComplete());
    const bool allowed = m_radioModel->isConnected() && slicesKnown;
    for (PanadapterApplet* applet : m_panStack->allApplets()) {
        if (applet) { applet->setNoSliceHintAllowed(allowed); }
    }
}

void MainWindow::onPanTakeTransmitRequested(const QString& panId)
{
    Q_UNUSED(panId)
    if (m_multiDevice) {
        m_multiDevice->askTakeTransmit();
    }
}

void MainWindow::wireSpectrumForPan(SpectrumWidget* sw, const QString& panId)
{
    if (!sw || !m_radioModel) { return; }

    // iPhone app plan Task 78: other devices' slices on this pan, and what a
    // click on one's label says.
    connect(sw, &SpectrumWidget::foreignMarkerClicked, this,
            [this](int, const QString& explanation) {
        showToast(explanation, ToastSeverity::Info, 4000);
    });
    refreshForeignMarkers();

    configureSpectrumForPanForTest(sw, panId);

    // Parity ruling C13: in a remote window the Performance Overlay shows
    // the Core's drops too, headed as the Core's.
    if (!m_radioModel->ownsLocalDsp()) {
        const QPointer<MainWindow> self(this);
        sw->setCorePerfLinesProvider([self]() -> QStringList {
            if (!self || !self->m_remoteTelemetry) { return {}; }
            return RemoteTelemetryController::performanceOverlayLines(
                self->m_remoteTelemetry->current());
        });
    }

    // Seed the new pan with the CURRENT connection state. Without this a pan
    // created after connect sits at the Disconnected default until the next
    // state change, and its disconnected guard swallows every mouse press.
    sw->setConnectionState(m_radioModel->connectionState());

    // Band plan too: setBandPlanManager was another activeSpectrumWidget()
    // one-shot, so the band-segment bar ("PHONE General" and friends) drew on
    // pan-0 only and every other pan showed a bare spectrum.
    sw->setBandPlanManager(&m_radioModel->bandPlanManagerMutable());

    // Right-click a spot on THIS pan to remove it. Was pan-0 only, so spots
    // could be dismissed from one pan and were inert on every other.
    if (SpotModel* spots = m_radioModel->spotModel()) {
        connect(sw, &SpectrumWidget::spotRemoveRequested,
                spots, &SpotModel::removeSpot);
    }

    // Hovering a spot on any pan drives the Spot Hub highlight.
    if (m_spotHubDialog) {
        connect(sw, &SpectrumWidget::spotHoverIndexChanged,
                m_spotHubDialog.data(),
                &SpotHubDialog::setHoveredPanadapterSpot);
    }

    // Clicking a disconnected pan opens the connection panel, from any pan.
    connect(sw, &SpectrumWidget::disconnectedClickRequest,
            this, &MainWindow::connectionRequestedByOperator, Qt::UniqueConnection);

    // CTUN max-bin offset follows THIS pan's slice rather than the globally
    // active one, so a max-bin readout on a background pan is not measured
    // against a slice sitting on some other pan's stream.
    connect(sw, &SpectrumWidget::ddcCenterFrequencyChanged, this,
            [this, panId](double ddcCenter) {
        if (!m_radioModel || !m_radioModel->wdspEngine()) { return; }
        if (SliceModel* s = sliceForPan(panId)) {
            m_radioModel->wdspEngine()->setMaxBinSliceOffsetHz(
                /*disp=*/0, s->frequency() - ddcCenter);
        }
    });

    // NOT wired here: bandwidthChangeRequested. Zoom itself already works on
    // every pan (SpectrumWidget narrows its own visible bin range), but the
    // auto-replan that keeps bins-per-pixel constant across zoom levels runs
    // against primaryFftEngine() -- one engine, pan-0's. Routing it per pan
    // needs the FFT engine looked up by the pan's stream, which is a bigger
    // change than this function. Until then a deep zoom on another pan stays
    // visually correct but does not gain FFT resolution.
}

// ---------------------------------------------------------------------------
// wireSpectrumSliceControls: the four spectrum controls that act on a slice.
//
// Bench 2026-07-28: "when I click to tune it always tunes flag A, not the last
// selected." Proven with per-hop instrumentation rather than by reading: a
// synthetic frequencyClicked on pan-0 fired exactly one handler, the copy in
// wireSliceToSpectrum, and sliceForPan was never called in the whole session.
//
// These four used to exist twice. wireSpectrumForPan held the per-pan version,
// and ensureOverlayPanels skips that function for pan-0 (its spot, connection
// and MaxBin hooks are wired elsewhere in this file and would double), so pan-0
// ran an older copy in wireSliceToSpectrum whose lambdas closed over
//
//     SliceModel* slice = m_radioModel->activeSlice();
//
// by value. connect() binds the object, and the lambda binds the pointer, so
// that copy was Slice A for the life of the session. On the single pan almost
// every operator uses, click-to-tune, the filter-edge drag, the pan drag and
// the CTUN toggle all drove Slice A and never consulted the pan at all. The
// per-pan active slice this epic added was correct and simply unread.
//
// One home now, called for every pan including pan-0, so the two cannot drift
// apart again. verify-no-captured-slice-spectrum-wiring.py fails the build if
// any of the four is connected to a sender other than `sw`; it is listed in
// the pre-commit hook and in CI's compliance job beside
// verify-no-gui-dsp-access.py, which guards the same class of mistake one
// layer down.
//
// Every handler resolves its slice through sliceForPan(panId) on each signal.
// That is the whole fix: resolving late is what lets the answer change when
// the operator selects a different flag.
// ---------------------------------------------------------------------------
void MainWindow::wireSpectrumSliceControls(SpectrumWidget* sw,
                                           const QString& panId)
{
    if (!sw || !m_radioModel) { return; }

    // Click on the spectrum tunes this pan's slice.
    connect(sw, &SpectrumWidget::frequencyClicked, this,
            [this, panId](double hz) {
        if (SliceModel* s = sliceForPan(panId)) { s->setFrequency(hz); }
    });

    // Parity Task 18: a left-click on a spot. The widget has already tuned
    // this pan's slice to it (frequencyClicked above); then the slice takes
    // the spot's mode, as AetherSDR does.
    // From AetherSDR src/gui/MainWindow_Wiring.cpp:4382-4437 [@1e0718ad].
    // Its Memory branch (NereusSDR has no memory spots yet), its TCI spot
    // notice (NereusSDR's TCI keeps no spots) and its FlexRadio
    // "spot trigger" command have no counterpart here.
    connect(sw, &SpectrumWidget::spotTriggered, this,
            [this, panId](int spotIndex) {
        applySpotModeToSlice(m_radioModel, sliceForPan(panId), spotIndex);
    });

    // Drag a filter edge on this pan.
    connect(sw, &SpectrumWidget::filterEdgeDragged, this,
            [this, panId](int low, int high) {
        if (SliceModel* s = sliceForPan(panId)) { s->setFilter(low, high); }
    });

    // Drag the pan. Non-CTUN moves the VFO; CTUN pins the DDC to the pan centre
    // and offsets WDSP so audio stays on the VFO (Thetis radio.cs:1417 --
    // SetRXAShiftFreq receives +(freq - center)).
    connect(sw, &SpectrumWidget::centerChanged, this,
            [this, panId, sw](double centerHz) {
        if (m_handlingBandJump) { return; }
        SliceModel* s = sliceForPan(panId);
        if (!s) { return; }
        if (!sw->ctunEnabled()) {
            s->setFrequency(centerHz);
            return;
        }
        // RemoteMediaController sends an authenticated centre command and
        // waits for Core's geometry. No client-local hardware/DSP mutation.
        if (!m_radioModel->ownsLocalDsp()) { return; }
        const int stream = s->streamIndex();
        if (stream >= 0 && m_radioModel->receiverManager()) {
            m_radioModel->receiverManager()->forceHardwareFrequency(
                stream, static_cast<quint64>(centerHz));
        }
        sw->setDdcCenterFrequency(centerHz);
        // Re-shift the WHOLE stream, not just this pan's slice. Addressing the
        // dragged slice's own channel fixed the hardcoded rxChannel(0), but a
        // shared DDC window still has one centre and N slices at their own
        // offsets inside it, so a co-host on the same stream kept the offset it
        // had before the drag and demodulated the wrong signal with its flag
        // still reading right.
        //
        // From Thetis radio.cs:1417 [v2.10.3.15]: SetRXAShiftFreq receives
        // +(freq - center). reshiftSlicesOnStream applies that per member.
        m_radioModel->reshiftSlicesOnStream(stream, centerHz);
    });

    // CTUN toggle restores this pan's whole stream rather than channel 0.
    //
    // Unpinning does not retune the DDC, so the members are still offset from
    // an unmoved centre; zeroing the shift would drop the demodulator onto the
    // DDC centre. Restore against where the DDC actually sits (the drag above
    // bypasses the allocator via forceHardwareFrequency and writes the centre
    // into this widget), and let the next VFO move settle the offsets to zero
    // through the now-unpinned allocator.
    // R-R3-21 (fix wave M3): a container's Peak button lights at once when
    // peak hold changes on its slice's pan, from the overlay or elsewhere.
    connect(sw, &SpectrumWidget::peakHoldEnabledChanged, this,
            [this](bool) { refreshContainerControls(); });
    connect(sw, &SpectrumWidget::ctunEnabledChanged, this,
            [this, panId, sw](bool enabled) {
        // R-R3-21: a container's CTUN button lights from its slice's pan.
        refreshContainerControls();
        if (!m_radioModel->ownsLocalDsp()) { return; }
        if (m_radioModel->receiverManager()) {
            m_radioModel->receiverManager()->setDdcFrequencyLocked(enabled);
        }
        if (enabled) { return; }
        if (SliceModel* s = sliceForPan(panId)) {
            m_radioModel->reshiftSlicesOnStream(s->streamIndex(),
                                                sw->ddcCenterFrequency());
        }
    });
}

void MainWindow::ensureOverlayPanels()
{
    if (!m_panStack || !m_radioModel) { return; }

    // Remote Daemon R2 Task 12: this used to call refreshMeterPollerSlices()
    // here too (belt-and-braces alongside the sliceAdded/sliceRemoved
    // connects removed from wirePanStatusOverlayTriggers's neighbourhood).
    // SliceMeterPump re-reads RadioModel::slices() on every poll tick, so
    // there is nothing to push from here either.

    // Drop entries whose pan (and therefore whose parent widget) is gone.
    for (auto it = m_overlayPanels.begin(); it != m_overlayPanels.end(); ) {
        if (it.value().isNull() || !m_panStack->panadapter(it.key())) {
            it = m_overlayPanels.erase(it);
        } else {
            ++it;
        }
    }

    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        const QString panId = applet->panId();
        if (m_overlayPanels.contains(panId)) { continue; }

        SpectrumWidget* sw = m_panStack->spectrum(panId);
        if (!sw) { continue; }

        // Same one-shot-per-pan guard as the strip below: this loop only runs
        // for pans that have no entry yet, so the interaction connects cannot
        // stack on a layout switch.
        //
        // The slice controls go to EVERY pan, pan-0 included. pan-0 used to be
        // excluded from the whole of wireSpectrumForPan and left running an
        // older copy of these four in wireSliceToSpectrum, whose lambdas held
        // a captured Slice A pointer -- which is the 2026-07-28 bench defect
        // where click-to-tune always retuned flag A. Those copies are gone, so
        // this is the only wiring for them and nothing doubles.
        wireSpectrumSliceControls(sw, panId);
        wireWidebandExtensionForTest(sw, m_radioModel, m_panStack, panId);

        // Parity Task 31 (A11): the keyed trace's calibration
        // (SpectrumWidget::displayCalOffsetDb; Thetis display.cs:4820-4850
        // [v2.10.3.15], RX1Offset): Setup > Calibration's TX Display Cal
        // Offset, and for display duplex the preamp half of the receive
        // calibration and the TX attenuator offset (applyKeyedDisplayOffsets
        // keeps every pan current).
        {
            const CalibrationController& cal = m_radioModel->calibrationController();
            sw->setTxDisplayCalOffsetDb(static_cast<float>(cal.txDisplayOffsetDb()));
            sw->setRxPreampOffsetDb(static_cast<float>(m_radioModel->rxPreampOffsetDb()));
            if (m_stepAttController) {
                sw->setTxAttenuatorOffsetDb(
                    static_cast<float>(m_stepAttController->txAttenuatorOffsetDb()));
            }
            const QPointer<SpectrumWidget> guard(sw);
            connect(&cal, &CalibrationController::changed, sw, [this, guard]() {
                if (!guard.isNull() && m_radioModel) {
                    guard->setTxDisplayCalOffsetDb(static_cast<float>(
                        m_radioModel->calibrationController().txDisplayOffsetDb()));
                }
            });
        }

        if (panId != QStringLiteral("pan-0")) {
            // Still pan-0-excluded: its spot, connection and MaxBin hooks are
            // wired one-shot elsewhere in this file (the activeSpectrumWidget()
            // connects near the spot / ConnectionPanel / MaxBin sections), so
            // running them again here would double every one. Unifying those
            // the way the slice controls just were is a separate change with
            // its own regression surface.
            wireSpectrumForPan(sw, panId);
        }

        auto* panel = new SpectrumOverlayPanel(sw);
        panel->setPanId(panId);
        panel->setSliceResolver([this, panId]() {
            return sliceForPan(panId);
        });
        panel->move(4, 4);
        panel->show();
        m_overlayPanels.insert(panId, panel);
        // Parity Task 18: this strip's Display flyout and Clarity Re-tune act
        // on this pan.
        wirePanDisplayFlyout(panel, sw, panId);

        // Phase 3O Sub-Phase 9 Task 9.2c — bind the VAX Ch combo to the model.
        panel->setRadioModel(m_radioModel);
        // TX rulings (item 3): held from the start on a listened slice.
        refreshOverlayAttAccess();
        connect(applet, &PanadapterApplet::activeSliceChanged, panel,
                [this, panel](const QString&, int) {
            panel->bindToPanSlice();
            refreshOverlayAttAccess();   // TX rulings (item 3)
        });

        // Phase 3P-I-a T18 — board caps drive the antenna combos, and are
        // re-pushed on every radio swap.
        panel->setBoardCapabilities(m_radioModel->boardCapabilities());
        connect(m_radioModel, &RadioModel::currentRadioChanged, panel,
                [this, panel]() {
            panel->setBoardCapabilities(m_radioModel->boardCapabilities());
        });

        // +RX adds a slice on the pan this strip belongs to. The id comes from
        // the signal, so this never consults an "active" pan.
        connect(panel, &SpectrumOverlayPanel::addRxClicked, this,
                [this](const QString& id) {
            if (!m_radioModel || id.isEmpty()) { return; }
            addSliceForWindow(id);
        });

        // +TNF adds a notch on the slice THIS pan is showing, at the frequency
        // the operator is actually listening to. The composition lives in
        // onAddTnfClicked; a named slot, so re-arming this loop on a layout
        // switch cannot stack duplicate connections.
        connect(panel, &SpectrumOverlayPanel::addTnfClicked, this,
                &MainWindow::onAddTnfClicked, Qt::UniqueConnection);

        // R-R3-21: the zoom buttons zoom the pan they are drawn on, through
        // the same path as its Ctrl+wheel zoom. They emitted signals nothing
        // received. Step from AetherSDR src/gui/SpectrumWidget.cpp:2168-2169
        // [@1e0718ad]: emitZoom(1.5) out, emitZoom(1.0 / 1.5) in.
        connect(panel, &SpectrumOverlayPanel::zoomOut, sw, [sw]() { sw->zoomBy(1.5); });
        connect(panel, &SpectrumOverlayPanel::zoomIn, sw, [sw]() { sw->zoomBy(1.0 / 1.5); });
        connect(panel, &SpectrumOverlayPanel::zoomSegment, sw, &SpectrumWidget::zoomToSegment);
        connect(panel, &SpectrumOverlayPanel::zoomBand, sw, &SpectrumWidget::zoomToBand);

        // Band clicks act on this pan's active slice rather than the globally
        // active one, for the same reason (#118 fixed the mode-vs-frequency
        // half of this; the pan-targeting half arrives with per-pan strips).
        //
        // sliceIndex() is a stable slice ID, so it goes through
        // setActiveSliceById rather than the positional setActiveSlice --
        // otherwise a band click on a pan whose slice had a higher id than the
        // list is long (any mid-list removal) left the active slice where it
        // was and onBandButtonClicked retuned the wrong slice.
        //
        // Parity Task 18 (B3.1): the band change names this pan's slice. In a
        // remote window setActiveSliceById only asks the Core, so the active
        // slice has not moved when the band click runs; acting on "the
        // active slice" retuned whichever slice was active, often on another
        // pan. A pan with no slice has nothing to change.
        connect(panel, &SpectrumOverlayPanel::bandSelected, this,
                [this, panId](const QString& name, double, const QString&) {
            if (!m_radioModel) { return; }
            SliceModel* s = sliceForPan(panId);
            if (s == nullptr) { return; }
            selectSliceForWindow(s->sliceIndex());
            m_radioModel->onBandButtonClicked(s, bandFromName(name));
        });
    }
    // Parity Task 18 (C8): a pan created after connect says so when empty.
    refreshNoSliceHints();
    refreshClarityBadges();
}

// Parity Task 18: one pan's Display flyout and Clarity Re-tune, on that pan.
// The connects are the ones B8 Tasks 20-24 made for pan-0's strip, moved here
// so every strip gets them and each reaches its own SpectrumWidget (the
// colour-scheme one used to look up the active pan at click time).
void MainWindow::wirePanDisplayFlyout(SpectrumOverlayPanel* panel, SpectrumWidget* sw,
                                      const QString& panId)
{
    if (!panel || !sw) { return; }

    // B8 Task 20: WF Gain, WF Black Level and the colour scheme.
    connect(panel, &SpectrumOverlayPanel::wfColorGainChanged,
            sw, &SpectrumWidget::setWfColorGain);
    connect(panel, &SpectrumOverlayPanel::wfBlackLevelChanged,
            sw, &SpectrumWidget::setWfBlackLevel);
    connect(panel, &SpectrumOverlayPanel::colorSchemeChanged, sw, [sw](int idx) {
        // colorSchemeChanged carries a raw combo index (int); setWfColorScheme
        // takes the WfColorScheme enum, so adapt with a bounds-checked cast.
        const int schemeCount = static_cast<int>(WfColorScheme::Count);
        sw->setWfColorScheme(static_cast<WfColorScheme>(qBound(0, idx, schemeCount - 1)));
    });
    // B8 Task 21: Cursor Freq.
    connect(panel, &SpectrumOverlayPanel::cursorFreqVisibleChanged,
            sw, &SpectrumWidget::setCursorFreqVisible);
    // B8 Task 22: Fill Color. B8 fix-up: Fill Alpha.
    connect(panel, &SpectrumOverlayPanel::fillColorChanged,
            sw, &SpectrumWidget::setFillColor);
    connect(panel, &SpectrumOverlayPanel::fillAlphaChanged,
            sw, &SpectrumWidget::setFillAlpha);
    // Parity Task 18: Grid Lines changed only its own label.
    panel->setGridVisible(sw->gridEnabled());
    connect(panel, &SpectrumOverlayPanel::gridVisibleChanged,
            sw, &SpectrumWidget::setGridEnabled);
    // B8 Task 24: "More Display Options" opens Setup > Display, whose pages
    // act on the active pan (setSpectrumHooks), so this pan becomes it.
    connect(panel, &SpectrumOverlayPanel::openSetupRequested, this,
            [this, panId](const QString& page) {
        if (m_panStack) { m_panStack->setActivePan(panId); }
        auto* dialog = createSetupDialog();
        if (dialog == nullptr) {
            return;  // the gate refused and has already said why
        }
        dialog->selectPage(page);
        dialog->show();
    });
    // Clarity tunes the active pan; Re-tune on this pan's strip makes this
    // pan the one it tunes, then estimates afresh.
    connect(panel, &SpectrumOverlayPanel::clarityRetuneRequested, this,
            [this, panId]() {
        if (m_panStack) { m_panStack->setActivePan(panId); }
        if (m_clarityController) { m_clarityController->retuneNow(); }
    });
}

void MainWindow::refreshClarityBadges()
{
    for (auto it = m_overlayPanels.constBegin(); it != m_overlayPanels.constEnd(); ++it) {
        SpectrumOverlayPanel* panel = it.value();
        if (!panel) { continue; }
        const bool tuned = it.key() == m_clarityPanId;
        panel->setClarityStatus(tuned && m_clarityBadgeActive,
                                tuned && m_clarityBadgePaused);
    }
}

int MainWindow::clarityStreamIndex() const
{
    if (!m_panStack) { return -1; }
    SliceModel* slice = sliceForPan(m_clarityPanId.isEmpty() ? m_panStack->activePanId()
                                                              : m_clarityPanId);
    return slice ? slice->streamIndex() : -1;
}

// The slice this pan hosts: its own active slice if it has one, else the first
// slice associated with it. Returns nullptr for a pan with no slices.
SliceModel* MainWindow::sliceForPan(const QString& panId) const
{
    if (!m_panStack || !m_radioModel) { return nullptr; }
    auto* applet = m_panStack->panadapter(panId);
    if (!applet) { return nullptr; }
    const int active = applet->activeSliceIndex();
    if (active >= 0) {
        if (!desktopHosting() || desktopSliceAllowed(active)) {
            if (SliceModel* s = m_radioModel->sliceById(active)) { return s; }
        }
    }
    if (desktopHosting()) {
        if (SliceModel* chosen = activeSliceForWindow()) {
            if (applet->associatedSlices().contains(chosen->sliceIndex())) {
                return chosen;
            }
        }
    }
    for (SliceModel* s : m_radioModel->slices()) {
        if (s && applet->associatedSlices().contains(s->sliceIndex())
            && (!desktopHosting() || desktopSliceAllowed(s->sliceIndex()))) {
            return s;
        }
    }
    return nullptr;
}

void MainWindow::wirePanBadgeHandlers()
{
    if (!m_panStack) { return; }
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        connect(applet, &PanadapterApplet::wideBadgeClicked,
                this, &MainWindow::onPanWideBadgeClicked,
                Qt::UniqueConnection);
        connect(applet, &PanadapterApplet::chainTagClicked,
                this, &MainWindow::onPanChainTagClicked,
                Qt::UniqueConnection);
        connect(applet, &PanadapterApplet::txBadgeClicked,
                this, &MainWindow::onPanTxBadgeClicked,
                Qt::UniqueConnection);
        // Task 78: the TX pill that offers Take transmit.
        connect(applet, &PanadapterApplet::takeTransmitRequested,
                this, &MainWindow::onPanTakeTransmitRequested,
                Qt::UniqueConnection);
        // Task B5: add-slice / float, both carrying the applet's own panId().
        // Member-pointer targets, same reasoning as the three connects above:
        // this function re-runs on every countChanged.
        connect(applet, &PanadapterApplet::addSliceRequested,
                this, &MainWindow::onPanAddSliceRequested,
                Qt::UniqueConnection);
        connect(applet, &PanadapterApplet::floatRequested,
                this, &MainWindow::onPanFloatRequested,
                Qt::UniqueConnection);
        // Phase 3F: clicking a pan makes it the active pan. Straight to the
        // stack's setter, exactly as AetherSDR MainWindow.cpp:12964 [@6a142807]
        // does it:
        //   connect(applet, &PanadapterApplet::activated,
        //           m_panStack, &PanadapterStack::setActivePan);
        // Member-pointer target on both ends, so Qt::UniqueConnection is
        // actually honoured here (it is silently dropped for lambdas -- see
        // RadioModel.cpp:5666), which matters because this re-runs on every
        // countChanged.
        connect(applet, &PanadapterApplet::activated,
                m_panStack, &PanadapterStack::setActivePan,
                Qt::UniqueConnection);
    }
}

int MainWindow::panChainIndex(const QString& panId) const
{
    if (!m_panStack || !m_radioModel) { return -1; }
    auto* applet = m_panStack->panadapter(panId);
    if (!applet) { return -1; }

    const bool hosting = desktopHosting();
    SliceModel* slice = hosting ? sliceForPan(panId) : nullptr;
    if (hosting && !slice) { return -1; }
    const int chain = m_radioModel->sliceChainIndex(
        slice ? slice->sliceIndex() : applet->activeSliceIndex());
    if (chain >= 0) { return chain; }

    // The slice is bound to no stream, so the model has no chain to give.
    // updateStatusOverlay holds the CH tag at its last known-good value in
    // exactly this case rather than claiming chain 0, so following it keeps
    // the dialog on the chain the operator can see. statusChainIndex() is the
    // read-back 0896b4f3 added for the overlay.
    return applet->statusChainIndex();
}

void MainWindow::onPanWideBadgeClicked(const QString& panId)
{
    onPanChainTagClicked(panId, -1);
}

// chainIdx is what the CH tag was painting when it was clicked; it is passed
// so the two entry points stay one function, but the live resolution wins.
// The tag is refreshed from the same call panChainIndex makes, so they agree
// in every case except a stale repaint, and the live answer is the honest one
// to open a dialog on.
void MainWindow::onPanChainTagClicked(const QString& panId, int chainIdx)
{
    if (!m_radioModel) { return; }
    if (desktopHosting() && !sliceForPan(panId)) { return; }

    int chain = panChainIndex(panId);
    if (chain < 0) { chain = chainIdx; }
    if (chain < 0) { return; }

    FilterPolicyDialog dlg(chain, m_radioModel, this);
    dlg.exec();
}

// The TX pill hands the transmitter to the slice THIS pan is showing, which
// is the pan's own activeSliceIndex -- a slice ID, so it goes through
// RadioModel::requestTxHandoffToSlice, which converts to the list position
// TxSliceArbiter indexes by and drops MOX before flipping. The MOX drop stays
// the arbiter's; nothing here reproduces or bypasses it.
//
// Reachable only while this pan's slice already holds TX, because
// SpectrumStatusOverlay paints and hit-tests the pill on m_txBound. The
// handoff is therefore a no-op today, and is wired to the correct target so
// that it stays correct if the pill is ever given an unlit clickable state --
// a visual decision, not one to make here.
void MainWindow::onPanTxBadgeClicked(const QString& panId)
{
    if (!m_panStack || !m_radioModel) { return; }
    if (!desktopHosting()) {
        if (auto* applet = m_panStack->panadapter(panId)) {
            m_radioModel->requestTxHandoffToSlice(applet->activeSliceIndex());
        }
        return;
    }
    SliceModel* slice = sliceForPan(panId);
    if (!slice || !desktopOwnsTransmit()) { return; }
    m_radioModel->requestTxHandoffToSlice(slice->sliceIndex());
}

// Task B5: straight forwarders. panId is the emitting applet's own id (see
// PanadapterApplet::buildContextMenu), never resolved through
// m_panStack->activePanId() -- see MainWindow.h for why that distinction
// matters here.
void MainWindow::onPanAddSliceRequested(const QString& panId)
{
    addSliceForWindow(panId);
}

void MainWindow::onPanFloatRequested(const QString& panId)
{
    if (m_panStack) { m_panStack->floatPanadapter(panId); }
}

void MainWindow::wireSliceStatusOverlayTriggers(SliceModel* slice)
{
    if (!slice) { return; }

    connect(slice, &SliceModel::chainIndexChanged,
            this, &MainWindow::refreshPanWideBadges, Qt::UniqueConnection);
    connect(slice, &SliceModel::streamIndexChanged,
            this, &MainWindow::refreshPanWideBadges, Qt::UniqueConnection);

    // Connect the NOTIFY signal of every property the overlay reads, resolved
    // through the metaobject from the list PanadapterApplet publishes. Naming
    // the signals here instead would put the trigger set in a second place
    // that has to be remembered -- which is how updateStatusOverlay came to
    // have no callers at all.
    const QMetaObject* sliceMeta = slice->metaObject();
    const int refreshIdx =
        MainWindow::staticMetaObject.indexOfSlot("refreshPanStatusOverlays()");
    if (refreshIdx < 0) {
        // Renaming the slot without updating this string would otherwise
        // strand every overlay silently -- the exact failure this change
        // exists to fix. tst_pan_status_overlay pins the lookup so it cannot
        // reach a release, and this keeps a debug build loud if it ever does.
        qWarning("MainWindow: refreshPanStatusOverlays() slot not found; "
                 "per-pan status overlays will not follow slice state");
        return;
    }
    const QMetaMethod refresh = MainWindow::staticMetaObject.method(refreshIdx);

    for (const QByteArray& name : PanadapterApplet::statusOverlaySliceProperties()) {
        const int propIdx = sliceMeta->indexOfProperty(name.constData());
        if (propIdx < 0) { continue; }
        const QMetaProperty prop = sliceMeta->property(propIdx);
        if (!prop.hasNotifySignal()) { continue; }
        connect(slice, prop.notifySignal(), this, refresh, Qt::UniqueConnection);
    }
}

void MainWindow::rebuildFftRouting()
{
    if (!m_radioModel) { return; }

    // The WIDE badges ride this pass rather than getting their own connects
    // at each of the four call sites. The trigger set is identical -- any
    // change to which slices feed which pan through which stream moves both
    // answers -- so hanging the refresh here means every present and future
    // topology trigger reaches the badge by construction. Same reasoning the
    // Alex republish uses for hanging off bpfStateChanged (RadioModel.cpp:
    // 596-601). Ahead of the router guard on purpose: the badge is a model
    // question and stays correct with no FFTRouter present.
    refreshPanWideBadges();

    // The status overlay rides the same pass, for the same reason: every
    // topology trigger (slice add / remove, pan migration, stream rebind,
    // chain reassignment) moves which slice a pan shows and which chain feeds
    // it, so hanging the refresh here means present and future triggers reach
    // the overlay by construction rather than by remembering. The per-slice
    // frequency / mode triggers and each pan's activeSliceChanged are wired
    // separately, since those move the overlay without moving the topology.
    refreshPanStatusOverlays();

    auto* router = m_radioModel->fftRouter();
    if (!router) { return; }

    // R1 Task 7: the router-mutation half of this rebuild -- turning a
    // resolved subscription set into FFTRouter calls, wholesale rather than
    // as an incremental edit -- now lives in FftTopology (src/core/spectrum/
    // FftTopology.h), core state a headless daemon can drive from
    // remote-endpoint ids instead of pan ids. What stays here is the walk
    // that resolves which pan each live slice actually feeds: it needs
    // PanadapterStack (a QWidget) and SliceModel, neither of which core may
    // depend on (tests/tst_core_has_no_gui_includes.cpp).
    //
    // Snapshot the pre-rebuild subscriptions first so a brand-new one can
    // be told apart from one that merely survived. Only new ones get the
    // stream window pushed: streamBindingsChanged fires on every bind, so
    // on every VFO tick, and re-pushing the allocator's centre each time
    // would yank a CTUN pan back after an operator pan-drag (that path
    // retunes the DDC through forceHardwareFrequency without going through
    // the allocator).
    // Fix round 1 (coordinator spec review, finding 1): a QHash<QString,
    // int> here misfired the isNewSubscription gate below the moment a pan
    // could legitimately carry more than one stream at once -- whichever
    // stream was not the single remembered one looked "new" on every pass,
    // re-triggering applyStreamWindowToPan every time instead of once.
    // Restored to the list-membership form this had before FftTopology
    // existed, just sourced from m_topology.subscriptions() instead of
    // router->receiversForPan(panId).
    QHash<QString, QList<int>> before;
    for (const SpectrumSubscription& sub : m_topology.subscriptions()) {
        before[sub.consumerId].append(sub.streamIndex);
    }

    // Wholesale rebuild, not an incremental edit. A pan can host several
    // slices, and unsubscribing one pan at a time here (rather than
    // clearing every live pan up front) would let a slice's migration
    // silently strand its co-hosted neighbours' subscriptions on whatever
    // they last held. Binding signals also fire before sliceAdded (plan
    // discovery item 7), so only a rebuild-from-model consumer is safe.
    if (m_panStack) {
        // PanadapterStack::allApplets (PanadapterStack.h:70) and
        // PanadapterApplet::panId (PanadapterApplet.h:65) both already exist.
        for (auto* applet : m_panStack->allApplets()) {
            if (!applet) { continue; }
            m_topology.unsubscribe(applet->panId());
        }
    }

    for (SliceModel* slice : m_radioModel->slices()) {
        if (!slice) { continue; }
        const int stream = slice->streamIndex();
        if (stream < 0) { continue; }          // unbound slice feeds nothing
        // A listened slice a layout change placed is a marker only on that
        // pan: its stream is not subscribed there, so its frames never draw
        // there and its DDC moves never re-centre that pan. The pan keeps
        // the stream and the centre of the slices that own its view.
        if (markerOnlyPlacement(slice->sliceIndex())) { continue; }

        // Resolving the pan is not just slice->panKey(). Slice A never has
        // one: RadioModel::addSlice stamps panKey only when it is given one
        // and connectToRadio calls the no-argument overload
        // (RadioModel.cpp:4137), so Slice A's panKey is permanently empty.
        // Skipping empties (as the Task 9 spec had it) would leave pan 0
        // with no subscription and no trace at all.
        //
        // Fall back to the pan that actually hosts the slice, recorded by
        // the sliceAdded handler through PanadapterApplet::addSlice. That
        // record is stable; activePanId() is not, and using it alone would
        // migrate Slice A's subscription (and darken pan 0) the moment the
        // operator made another pan active. activePanId() stays as the last
        // resort, matching spectrumForSlice (MainWindow.cpp:880).
        if (!m_panStack) { continue; }
        // Slice control plan Task 16: a listened slice this window placed
        // feeds the pan it was placed on.
        QString panId = m_listenPlacement.value(slice->sliceIndex());
        if (panId.isEmpty() || !m_panStack->panadapter(panId)) { panId = slice->panKey(); }
        if (panId.isEmpty() || !m_panStack->panadapter(panId)) {
            panId.clear();
            for (auto* applet : m_panStack->allApplets()) {
                if (applet
                    && applet->associatedSlices().contains(slice->sliceIndex())) {
                    panId = applet->panId();
                    break;
                }
            }
        }
        if (panId.isEmpty()) { panId = m_panStack->activePanId(); }
        if (panId.isEmpty()) { continue; }

        const bool isNewSubscription = !before.value(panId).contains(stream);
        // subscribe() adds idempotently -- a pan can legitimately carry more
        // than one stream at once (fix round 1, finding 1), and repeating an
        // already-held (panId, stream) pair is a no-op -- so two slices
        // sharing a stream and a pan collapse to the one subscription
        // applyTo() below actually applies.
        m_topology.subscribe(panId, stream);
        if (isNewSubscription) {
            applyStreamWindowToPan(panId, stream);
        }
    }

    m_topology.applyTo(*router);
}

void MainWindow::dispatchFftFrameToPans(int streamIndex,
                                        const QVector<float>& binsLinear,
                                        double windowEnb,
                                        double dbmOffset)
{
    if (m_radioModel && m_panStack
        && m_radioModel->role() != RadioModel::Role::Remote) {
        // From Thetis MeterManager.cs:43362-43363,44463-44477 [@3759d096]
        // [v2.10.3.15]: each receiver has its own 1024-pixel, 30-fps
        // RF window at twice MaxFilterWidth (console.cs:13221 = 10000 Hz).
        constexpr double kMiniRxSpanHz = 20'000.0;
        constexpr int kMiniPixels = FilterDisplayItem::kSpectrumPixels;
        constexpr int kMiniFramePeriodMs = 1000 / 30;
        for (auto& [sliceId, producer] : m_miniProducers) {
            SliceModel* slice = producer->slice;
            if (!slice || slice->streamIndex() != streamIndex
                || (m_radioModel->isTransmitting()
                    && m_radioModel->txBoundSlice() == slice)) {
                continue;
            }
            const double centreHz = slice->frequency();
            const double sourceHz = m_radioModel->streamCentreHz(streamIndex);
            const double rateHz = m_radioModel->streamSampleRateHz(streamIndex);
            if (!m_radioModel->streamActive(streamIndex) || !std::isfinite(sourceHz)
                || !std::isfinite(rateHz) || rateHz <= 0.0
                || centreHz - kMiniRxSpanHz / 2.0 < sourceHz - rateHz / 2.0
                || centreHz + kMiniRxSpanHz / 2.0 > sourceHz + rateHz / 2.0) {
                clearMiniSlice(sliceId);
                continue;
            }
            ReducerConfig config;
            config.pixels = kMiniPixels;
            config.centreHz = centreHz;
            config.spanHz = kMiniRxSpanHz;
            config.streamCentreHz = sourceHz;
            config.sampleRateHz = rateHz;
            ReducerConfig waterfallConfig = config;
            SpectrumWidget* analyzerSettings = nullptr;
            for (PanadapterApplet* applet : m_panStack->allApplets()) {
                if (!applet || !applet->spectrumWidget()) { continue; }
                if (!analyzerSettings) { analyzerSettings = applet->spectrumWidget(); }
                if (applet->activeSliceIndex() == sliceId) {
                    analyzerSettings = applet->spectrumWidget();
                    break;
                }
            }
            // A slice can share a pan or have no dedicated pan. Thetis still
            // copies the RX analyzer's settings into that slice's MiniSpec.
            if (analyzerSettings) {
                config.detector = static_cast<SpectrumDetectorMode>(
                    analyzerSettings->spectrumDetector());
                config.averageMode = int(analyzerSettings->spectrumAveraging());
                config.averageAlpha = analyzerSettings->spectrumAverageAlpha();
                waterfallConfig.detector = static_cast<SpectrumDetectorMode>(
                    analyzerSettings->waterfallDetector());
                waterfallConfig.averageMode = int(analyzerSettings->waterfallAveraging());
                waterfallConfig.averageAlpha = analyzerSettings->waterfallAverageAlpha();
            }
            const bool changed = producer->streamIndex != streamIndex
                || producer->streamEpoch != slice->streamEpoch()
                || producer->binCount != binsLinear.size()
                || producer->geometry.centreHz != config.centreHz
                || producer->geometry.spanHz != config.spanHz
                || producer->geometry.streamCentreHz != config.streamCentreHz
                || producer->geometry.sampleRateHz != config.sampleRateHz
                || producer->geometry.detector != config.detector
                || producer->geometry.averageMode != config.averageMode
                || producer->geometry.averageAlpha != config.averageAlpha
                || producer->waterfallGeometry.detector != waterfallConfig.detector
                || producer->waterfallGeometry.averageMode != waterfallConfig.averageMode
                || producer->waterfallGeometry.averageAlpha != waterfallConfig.averageAlpha;
            if (changed) {
                producer->trace.clearAveraging();
                producer->waterfall.clearAveraging();
                producer->cadence.invalidate();
                clearMiniSlice(sliceId);
                producer->geometry = config;
                producer->waterfallGeometry = waterfallConfig;
                producer->streamIndex = streamIndex;
                producer->streamEpoch = slice->streamEpoch();
                producer->binCount = binsLinear.size();
            }
            if (producer->cadence.isValid()
                && producer->cadence.elapsed() < kMiniFramePeriodMs) { continue; }
            const auto [firstBin, lastBin] = SpectrumReducer::visibleBinRange(
                binsLinear.size(), config);
            if (lastBin < firstBin) {
                clearMiniSlice(sliceId);
                continue;
            }
            producer->trace.setConfig(config);
            producer->waterfall.setConfig(waterfallConfig);
            QVector<float> trace;
            QVector<float> waterfall;
            producer->trace.reduce(binsLinear, windowEnb, dbmOffset, trace);
            producer->waterfall.reduce(binsLinear, windowEnb, dbmOffset, waterfall);
            if (trace.size() != kMiniPixels || waterfall.size() != kMiniPixels) {
                clearMiniSlice(sliceId);
                continue;
            }
            producer->cadence.restart();
            presentMiniFrame(sliceId, trace, waterfall, centreHz,
                             kMiniRxSpanHz, false, true);
        }
    }
    if (!m_panStack || !m_radioModel) { return; }
    auto* router = m_radioModel->fftRouter();
    if (!router) { return; }

    // FFTRouter is the topology oracle rather than a signal hop:
    // pansForReceiver is public and unit-tested, and routing through its
    // own signal would add a queued hop on the render path for no gain.
    // One stream can feed N pans (different zoom levels of the same I/Q),
    // which is the AetherSDR overlay model the router was designed for.
    for (const QString& panId : router->pansForReceiver(streamIndex)) {
        // A pan showing the transmit spectrum must not also be fed its
        // receiver. On the HERMES class this never triggers during
        // PureSignal transmit because the radio has stopped streaming and
        // no frame arrives at all; on the ORION class RX1 keeps its DDC
        // right through transmit, so without this the two traces would
        // alternate frame by frame on one widget. Cleared on MOX fall.
        // Remote-window parity Task 29: MoxDisplayController owns the id.
        if (m_moxDisplay && !m_moxDisplay->transmitPanId().isEmpty()
            && panId == m_moxDisplay->transmitPanId()) {
            continue;
        }
        if (SpectrumWidget* sw = m_panStack->spectrum(panId)) {
            // R-R3-46 / R-R3-11: the receive offset of this stream's ADC
            // (unchanged values return early in setDbmCalOffset).
            sw->setDbmCalOffset(
                static_cast<float>(m_radioModel->rxMeterOffsetDbForStream(streamIndex)));
            sw->updateSpectrumLinear(streamIndex, binsLinear,
                                     windowEnb, dbmOffset);
        }
    }
}

void MainWindow::pushSpectrumCalToPans()
{
    if (!m_radioModel) {
        return;
    }
    // R-R3-46 / R-R3-11: each pan the router feeds takes its stream's ADC
    // offset; a pan it feeds nothing (or no router yet) keeps slice A's.
    QSet<SpectrumWidget*> placed;
    if (m_panStack) {
        if (auto* router = m_radioModel->fftRouter()) {
            for (int stream = 0; stream < 5; ++stream) {
                const auto offset =
                    static_cast<float>(m_radioModel->rxMeterOffsetDbForStream(stream));
                for (const QString& panId : router->pansForReceiver(stream)) {
                    if (SpectrumWidget* sw = m_panStack->spectrum(panId)) {
                        if (!placed.contains(sw)) {
                            sw->setDbmCalOffset(offset);
                            placed.insert(sw);
                        }
                    }
                }
            }
        }
    }
    if (SpectrumWidget* active = activeSpectrumWidget(); active && !placed.contains(active)) {
        active->setDbmCalOffset(static_cast<float>(m_radioModel->rxMeterOffsetDb()));
    }
}

// Phase 3F Sub-Epic D Task 16: disconnect-before-removal for safe pan teardown.
// AetherSDR issue #242: deleting a widget with active connections to lambdas
// can race with queued signal delivery and crash. Disconnect first, then
// remove.
void MainWindow::disconnectPanadapter(const QString& panId)
{
    if (!m_panStack) { return; }
    auto* applet = m_panStack->panadapter(panId);
    if (!applet) { return; }

    if (auto* sw = applet->spectrumWidget()) {
        sw->disconnect(this);
    }
    applet->disconnect(this);

    // R1 Task 7: m_topology is now what rebuildFftRouting() rebuilds the
    // router from, so a destroyed pan has to leave it too -- otherwise the
    // very next rebuild pass would resurrect this panId's mapping (applyTo()
    // only knows a consumer is gone if unsubscribe() was called; it has no
    // way to learn a widget was destroyed on its own). The direct router
    // removal stays alongside it for the reason it was already here:
    // disconnect-before-removal needs the fan-out to stop in this same
    // tick, not just by the next rebuild.
    m_topology.unsubscribe(panId);
    if (m_radioModel) {
        if (auto* router = m_radioModel->fftRouter()) {
            router->removePan(panId);
        }
    }
}

// Issue #206 — main-window geometry persistence. Qt's saveGeometry()
// returns a versioned QByteArray that already encodes position, size,
// AND window state (Normal/Maximized/FullScreen) plus screen identity
// for multi-monitor setups. We base64-encode it so AppSettings (which
// stores QString values) can round-trip the blob unmodified.
void MainWindow::saveMainWindowGeometry()
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("MainWindowGeometry"),
               QString::fromLatin1(saveGeometry().toBase64()));
}

bool MainWindow::restoreMainWindowGeometry()
{
    auto& s = AppSettings::instance();
    const QString blob = s.value(QStringLiteral("MainWindowGeometry")).toString();
    if (blob.isEmpty()) {
        return false;
    }
    const QByteArray bytes = QByteArray::fromBase64(blob.toLatin1());
    if (bytes.isEmpty() || !restoreGeometry(bytes)) {
        return false;
    }

    // Multi-screen safety: if the restored frame's center sits outside
    // every connected screen (monitor disconnected since last save),
    // fall back to the centered 1280×800 default rather than parking
    // the window offscreen where the user can't reach it.
    const QPoint center = frameGeometry().center();
    if (!QGuiApplication::screenAt(center)) {
        resize(1280, 800);
        if (auto* primary = QGuiApplication::primaryScreen()) {
            const QRect avail = primary->availableGeometry();
            move(avail.center() - QPoint(width() / 2, height() / 2));
        }
        // Drop any saved Maximized/FullScreen bit so we don't immediately
        // re-maximize onto a screen layout that no longer matches.
        setWindowState(Qt::WindowNoState);
        return false;
    }

    return true;
}

void MainWindow::buildUI()
{
    // Title: name, version, then whatever else the operator needs to tell
    // this window apart from another one.
    //
    // Issue #100 added the active profile, so two instances against
    // different radios are distinguishable. The build tag is the same idea
    // for the same reason: a smoke build under test has to be
    // distinguishable from a release and from another worktree's build.
    // Standing rule (JJ, KG4VCF, 2026-07-30), earned when a session spent
    // real time proving by pgrep which binary was on screen.
    //
    // The tag is empty on release artifacts, which are built from a tag ref,
    // so their title stays exactly as it was. main() fills BuildIdentity in
    // from a header regenerated on every build, so the sha here is the sha
    // that was compiled, not the one that was current at the last configure.
    QString title = QStringLiteral("NereusSDR %1").arg(NEREUSSDR_VERSION);

    const QString buildTag = BuildIdentity::buildTag();
    if (!buildTag.isEmpty()) {
        title += QStringLiteral(" · %1").arg(buildTag);
    }

    // Profile stays last: it is per-run operator state, whereas the build
    // tag is a property of the binary, and the binary identity reads better
    // next to the version it belongs to.
    const QString profile = AppSettings::profileOverride();
    if (!profile.isEmpty()) {
        title += QStringLiteral(" [%1]").arg(profile);
    }

    setWindowTitle(title);
    setMinimumSize(800, 600);
    resize(1280, 800);

    // Issue #206 — restore last session's window position, size, and
    // maximized/fullscreen state. The 1280×800 above stays as the
    // first-launch fallback; restoreMainWindowGeometry() returns false
    // when no saved blob exists or the blob is corrupted, in which
    // case Qt centers the default size on the primary screen as before.
    restoreMainWindowGeometry();

    // --- Main QSplitter: spectrum (left) + container panel (right) ---
    // AetherSDR pattern: right panel is a proper layout element, not an overlay.
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->setChildrenCollapsible(false);
    m_mainSplitter->setHandleWidth(3);
    m_mainSplitter->setStyleSheet(QStringLiteral(
        "QSplitter::handle { background: #203040; }"));

    // Left side: spectrum + zoom bar
    auto* spectrumPane = new QWidget(m_mainSplitter);
    spectrumPane->setMinimumWidth(400);
    auto* layout = new QVBoxLayout(spectrumPane);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Phase 3F Sub-Epic D Task 12: replace the single SpectrumWidget with
    // a PanadapterStack. PanadapterStack's constructor pre-creates
    // "pan-0" containing a PanadapterApplet whose embedded SpectrumWidget
    // becomes the new single-pan default. activeSpectrumWidget() resolves
    // to that widget for backward-compat call sites.
    m_panStack = new PanadapterStack(spectrumPane);
    m_panStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_panStack, 1);

    SpectrumWidget* const initialSpectrum = activeSpectrumWidget();
    if (initialSpectrum) {
        configureSpectrumForPanForTest(initialSpectrum,
                                       QStringLiteral("pan-0"));
    }

    // Task 13 wires per-pan rebinding when the active pan changes; for
    // now we just log/no-op.  Future polish: re-attach overlay panel,
    // peak-detector, spot bridges, etc. to the new active pan's widget.
    // Point the Setup dialog's Display pages at the pan the operator is on.
    //
    // Bench report 2026-07-30 (JJ, KG4VCF): "the second-N panadapters seem
    // not to honour the settings for display, pan 1 does, seems no way to
    // adjust the others."
    //
    // RadioModel::m_spectrumWidget is the single hook every Display setup
    // page pushes through (82 call sites across DisplaySetupPages,
    // AppearanceSetupPages and SpectrumPeaksPage). It was assigned once
    // during wiring from activeSpectrumWidget() and never again, so it kept
    // whichever widget happened to be active at startup for the whole
    // session. Every display control in Setup therefore acted on one pan and
    // silently did nothing for the others.
    //
    // Following the active pan is the smallest thing that makes the controls
    // reachable at all, and it suits the per-pan model: SpectrumWidget
    // already persists every display preference under
    // settingsKey(key, m_panIndex), so the pans genuinely have their own
    // settings and Setup just needs to say which one it means. Selecting a
    // pan and opening Setup now adjusts that pan.
    //
    // Not an activePanId indirection of the kind that rule forbids: this is
    // a global dialog choosing a target, not a control drawn on one pan
    // reaching sideways into another. A per-pan selector inside Setup, the
    // way Thetis splits RX1 and RX2 display settings, is the fuller answer.
    connect(m_panStack, &PanadapterStack::activePanChanged, this,
            [this](const QString& panId) {
        if (!m_radioModel || !m_panStack) { return; }
        if (SpectrumWidget* sw = m_panStack->spectrum(panId)) {
            setSpectrumHooks(sw);
        }
    });

    // Phase 3F: the status overlay's non-slice trigger. countChanged fires
    // from addPanadapter / removePanadapter, including the ones applyLayout
    // makes when the operator switches template, so this catches every pan
    // that comes into existence after startup -- which is every pan except
    // pan-0. Re-arming is safe: the connects inside are UniqueConnection.
    connect(m_panStack, &PanadapterStack::countChanged, this, [this](int) {
        wirePanStatusOverlayTriggers();
        wirePanBadgeHandlers();
        wirePanNotchHandlers();
        ensureOverlayPanels();
        refreshPanStatusOverlays();
        refreshPanNotchMarkers();
        refreshPanVisualNotch();
        refreshPanNotchMinWidth();
    });

    // R1 Task 7 fix round 1 (coordinator spec review, finding 2):
    // applyLayout()'s orphan-retirement loop destroys a pan on a layout
    // shrink without going through MainWindow::disconnectPanadapter (which
    // has no caller today), so nothing told m_topology that panId was
    // gone. Left alone, the next rebuildFftRouting() -> applyTo() call
    // would resurrect that pan's stale mapping into the router. Pre-dates
    // FftTopology and is mostly benign today (the pan-dispatch loop
    // null-checks m_panStack->spectrum(panId) before touching it), but a
    // router entry for a pan that no longer exists is still wrong state,
    // so close it now that m_topology exists to keep in sync.
    connect(m_panStack, &PanadapterStack::panRetired, this,
            [this](const QString& panId) { m_topology.unsubscribe(panId); });

    // A stream's physical ADC can move without its stream index or its folded
    // chain index moving with it, so nothing that watches slice properties
    // notices. The extended pan's wings are keyed on that physical ADC and
    // would otherwise keep painting the old one until an unrelated overlay
    // event happened past. Codex, PR #318.
    connect(m_radioModel, &RadioModel::streamAdcRoutingChanged, this,
            &MainWindow::refreshPanStatusOverlays);

    // TNF: the notch list is global, so one connect per NotchModel signal
    // repaints every pan. refreshPanNotchMarkers takes no arguments; Qt
    // drops the extra ones from notchAdded / notchChanged / notchRemoved /
    // globalEnabledChanged.
    if (NotchModel* notches = m_radioModel->notchModel()) {
        connect(notches, &NotchModel::notchAdded,
                this, &MainWindow::refreshPanNotchMarkers);
        connect(notches, &NotchModel::notchChanged,
                this, &MainWindow::refreshPanNotchMarkers);
        connect(notches, &NotchModel::notchRemoved,
                this, &MainWindow::refreshPanNotchMarkers);
        connect(notches, &NotchModel::notchesReset,
                this, &MainWindow::refreshPanNotchMarkers);
        connect(notches, &NotchModel::globalEnabledChanged,
                this, &MainWindow::refreshPanNotchMarkers);
        // Design section 8.3: the visual-notch toggle is model state, so it
        // gets a model trigger as well as the pan-count one above.
        connect(notches, &NotchModel::visualEnabledChanged,
                this, &MainWindow::refreshPanVisualNotch);
    }

    // TNF: the minimum notch width comes off an RxChannel, and channels open
    // when the pool does. Both of these run long after the pans exist, so
    // they are what gets the real value onto a pan that started on the 100 Hz
    // construction default. refreshPanNotchMinWidth arms the per-channel
    // follow itself, so this only has to cover channels coming into being.
    connect(m_radioModel, &RadioModel::sliceAdded, this,
            [this](int) { refreshPanNotchMinWidth(); });
    connect(m_radioModel, &RadioModel::sliceRemoved, this,
            [this](int) { refreshPanNotchMinWidth(); });
    connect(m_radioModel, &RadioModel::connectionStateChanged, this,
            [this](NereusSDR::ConnectionState) { refreshPanNotchMinWidth(); });

    // Remote Daemon R2 Task 12: the S-meter poller's slice list used to be
    // pushed here on every sliceAdded/sliceRemoved (refreshMeterPollerSlices,
    // removed). SliceMeterPump (src/core/meters/) re-reads RadioModel::
    // slices() itself on every poll tick instead, so there is nothing left
    // to refresh from a slice-lifecycle signal.
    wirePanStatusOverlayTriggers();
    wirePanBadgeHandlers();
    wirePanNotchHandlers();
    // Seed pan-0 with whatever NotchModel::restoreFromSettings() already
    // loaded in the RadioModel constructor. The layout restore below fires
    // countChanged and re-runs both of these for the pans it creates.
    refreshPanNotchMarkers();
    refreshPanVisualNotch();
    refreshPanNotchMinWidth();

    // ── Bench 2026-07-28: click-to-tune always tuned flag A ────────────────
    //
    // The pan a slice lives on has to follow the operator's selection.
    // RadioModel::activeSlice() (global) and
    // PanadapterApplet::activeSliceIndex() (per pan) are independent, and
    // only the global one had a writer once addSlice had seeded the pan --
    // so a pan latched onto the first slice added to it and stayed there.
    // MainWindow::sliceForPan reads the per-pan value, which is what
    // click-to-tune, the filter-edge drag, the CH tag and the pan TX pill
    // all act on, so selecting flag B moved every global surface and left
    // all four still driving A. This is the missing writer.
    //
    // Direction is one way, pan follows global, and deliberately so:
    //
    //   * Every route the operator has for selecting a slice (a VfoWidget
    //     flag press, an RxApplet slice tab, a band button, TCI) already
    //     lands on RadioModel::setActiveSlice, so following it here covers
    //     all of them in one wire instead of one per entry point.
    //   * The reverse -- a pan's own re-pick promoting itself to the global
    //     active slice -- is NOT wired. PanadapterApplet::removeSlice
    //     re-picks silently when a pan loses its active slice, and letting a
    //     background pan's bookkeeping steal the RX applet, the container
    //     S-meter and the DSP menu out from under the operator is a change
    //     to what they see, not to what tunes. RadioModel::removeSlice
    //     already re-seats the global active slice on its own when the
    //     removed one held it.
    //
    // Only the pan hosting the slice moves; setActiveSliceOnHostingPan is
    // where that rule lives. activeSliceChanged carries a LIST POSITION, so
    // the id is resolved through activeSlice()->sliceIndex() rather than
    // used as it arrives.
    connect(m_radioModel, &RadioModel::activeSliceChanged, this,
            [this](int) {
        if (desktopHosting()) { return; }
        if (!m_panStack || !m_radioModel) { return; }
        if (SliceModel* active = m_radioModel->activeSlice()) {
            m_panStack->setActiveSliceOnHostingPan(active->sliceIndex());
        }
    });

    // Phase 3F Sub-Epic D Task 15: restore persisted pan layout + splitter
    // sizes. Reads PanLayoutId from AppSettings (default "1") and asks the
    // stack to materialise that many pans before restoring per-splitter
    // QByteArray state. Operators get their last layout back on launch.
    {
        auto& s = AppSettings::instance();
        const QString restoredLayout = s.value(QStringLiteral("PanLayoutId"),
                                                QStringLiteral("1")).toString();
        // Shares the template table with applyPanLayout, but deliberately not
        // the rest of it: this runs at startup, before a radio is connected
        // and before any slice exists, so there is nothing to rehome and the
        // slice add-loop would manufacture a slice pre-connect.
        // (Codex review round 3, PR #293.)
        m_panStack->applyLayout(restoredLayout, panIdsForLayout(restoredLayout));
        m_panStack->restoreSplitterState();

        // ...and finish the job once there IS a radio. Skipping the slice
        // add-loop above is right, but nothing used to pick it up afterwards,
        // so a persisted 2v layout came back with pan-1 permanently dead: no
        // trace, no waterfall, a 0.0000 flag, and no way forward except
        // noticing you have to add Slice B by hand (bench, 2026-08-01,
        // J.J. Boyd KG4VCF).
        //
        // Queued so it lands after the connect handlers that size the stream
        // pool and bind Slice A have run; addSliceOnPan needs a sized pool to
        // bind what it creates.
        connect(m_radioModel, &RadioModel::connectionStateChanged, this,
                [this](ConnectionState s) {
            if (s != ConnectionState::Connected) { return; }
            QMetaObject::invokeMethod(this, [this]() { populateEmptyPans(); },
                                      Qt::QueuedConnection);
        });
    }

    // Phase 3F Sub-Epic E Task 3: the badge clicks are armed for every pan
    // from the countChanged hook above, alongside the status-overlay
    // triggers. See wirePanBadgeHandlers.

    // Left overlay panel (SpectrumOverlayPanel) — child of the active
    // pan's SpectrumWidget. Construction is deferred when the active
    // pan has no widget (shouldn't happen because the stack ctor
    // creates pan-0, but be defensive).
    // One strip per pan, created here for the pans that exist at startup and
    // re-armed from the countChanged hook for every pan created later.
    // Parity Task 18: each strip's display controls are wired to its own pan
    // there (wirePanDisplayFlyout); nothing is wired to pan-0's strip alone.
    ensureOverlayPanels();

    // ── 2026-05-12 bench fix: SpotModel → SpectrumWidget bridge ───────────
    //
    // The missing bridge — every spot ingestion path (Cluster / RBN /
    // WSJT-X / SpotCollector / POTA / FreeDV / PSK Reporter) lands in
    // RadioModel's per-source `on<Source>SpotReceived` slot which calls
    // `m_spotModel->applySpotStatus(...)`.  SpotModel then emits
    // spotAdded / spotUpdated / spotRemoved / spotsCleared.
    //
    // But upstream AetherSDR's `refreshSpots()` lambda on MainWindow —
    // the thing that translates SpotData into SpectrumWidget::SpotMarker
    // and calls `setSpotMarkers(...)` to repaint the panadapter overlay —
    // never carried over in the port.  Result: SpotTableModel (the Spot
    // List tab) fills correctly because it has its own per-source
    // wireClient lambda; the panadapter, however, has been blind to
    // every spot since Phase 3J-2 shipped.
    //
    // This block rebuilds the marker vector on every SpotModel change
    // (full rebuild, not delta — the typical spot map is <500 entries
    // with a 30-min lifetime).  DxCC-aware coloring uses the same
    // DxccColorProvider the SpotTableModel queries.
    if (auto* spotModel = m_radioModel->spotModel()) {
        auto refreshSpots = [this]() {
            if (!activeSpectrumWidget() || !m_radioModel) { return; }
            auto* spotModel = m_radioModel->spotModel();
            if (!spotModel) { return; }
            auto* dxccColor = m_radioModel->dxccColorProvider();

            const auto& spots = spotModel->spots();
            QVector<SpectrumWidget::SpotMarker> markers;
            markers.reserve(spots.size());
            for (auto it = spots.constBegin(); it != spots.constEnd(); ++it) {
                const SpotData& s = it.value();
                SpectrumWidget::SpotMarker m;
                m.index            = s.index;
                m.callsign         = s.callsign;
                // Prefer the RX (heard-on) frequency so cluster spots
                // land on the actual TX freq of the DX station.  Fall
                // back to txFreqMhz when only one is populated (e.g.
                // POTA / RBN sometimes ship rxFreqMhz only).
                m.freqMhz          = (s.rxFreqMhz > 0.0)
                                         ? s.rxFreqMhz
                                         : s.txFreqMhz;
                m.color            = s.color;
                m.mode             = s.mode;
                m.source           = s.source;
                m.spotterCallsign  = s.spotterCallsign;
                m.comment          = s.comment;
                m.timestampMs      = s.timestamp.isValid()
                                         ? s.timestamp.toMSecsSinceEpoch()
                                         : QDateTime::currentMSecsSinceEpoch();
                if (dxccColor && dxccColor->isEnabled()
                    && !m.callsign.isEmpty() && m.freqMhz > 0.0) {
                    m.dxccColor = dxccColor->colorForSpot(
                        m.callsign, m.freqMhz, m.mode);
                }
                markers.append(m);
            }
            activeSpectrumWidget()->setSpotMarkers(markers);
        };

        connect(spotModel, &SpotModel::spotAdded,
                this, [refreshSpots](const SpotData&) { refreshSpots(); });
        connect(spotModel, &SpotModel::spotUpdated,
                this, [refreshSpots](const SpotData&) { refreshSpots(); });
        connect(spotModel, &SpotModel::spotRemoved,
                this, [refreshSpots](int) { refreshSpots(); });
        connect(spotModel, &SpotModel::spotsCleared,
                this, refreshSpots);
        connect(spotModel, &SpotModel::spotsRefreshed,
                this, refreshSpots);

        // 2026-05-12 bench fix (Gap #3 follow-on).  Right-click → Remove
        // Spot on the panadapter emits spotRemoveRequested(idx) → purge
        // the spot from SpotModel → spotRemoved fires → refreshSpots
        // above repaints the overlay without it.
        connect(activeSpectrumWidget(), &SpectrumWidget::spotRemoveRequested,
                spotModel, &SpotModel::removeSpot);
    }

    // Zoom slider bar below spectrum
    auto* zoomBar = new QSlider(Qt::Horizontal, spectrumPane);
    zoomBar->setRange(1, 768);
    zoomBar->setValue(768);
    zoomBar->setFixedHeight(20);
    zoomBar->setToolTip(QStringLiteral("Zoom: drag to adjust spectrum bandwidth"));
    zoomBar->setStyleSheet(QStringLiteral(
        "QSlider { background: #0a0a14; }"
        "QSlider::groove:horizontal { background: #1a2a3a; height: 6px; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #00b4d8; width: 14px; margin: -4px 0; border-radius: 7px; }"));
    layout->addWidget(zoomBar);
    connect(zoomBar, &QSlider::valueChanged, this, [this](int val) {
        double bwHz = val * 1000.0;
        activeSpectrumWidget()->setFrequencyRange(activeSpectrumWidget()->centerFrequency(), bwHz);
        emit activeSpectrumWidget()->bandwidthChangeRequested(bwHz);
    });

    m_mainSplitter->addWidget(spectrumPane);

    // Right side: Container #0 will be added by ContainerManager
    setCentralWidget(m_mainSplitter);

    // --- Container Infrastructure (Phase 3G-1) ---
    m_containerManager = new ContainerManager(spectrumPane, m_mainSplitter, this);
    auto* contentRegistry = new ContainerContentRegistry(this);
    auto* workspaceStore = new ContainerWorkspaceStore(AppSettings::instance(), this);
    m_containerManager->setWorkspaceAdapter(workspaceStore, contentRegistry);
    m_containerManager->setTransmitting(m_radioModel->isTransmitting());
    connect(m_radioModel,&RadioModel::transmittingChanged,m_containerManager,&ContainerManager::setTransmitting);
    connect(m_containerManager, &ContainerManager::workspaceError, this, [this](const QString& error) {
        showToast(tr("Container layout is read-only: %1").arg(error), ToastSeverity::Warning, 6000);
    });
    // Adopt legacy state exactly once. Existing document placements, including
    // hidden/moved applets, are authoritative on every subsequent startup.
    if (workspaceStore->loadError().isEmpty() && !AppSettings::instance().contains("ContainerWorkspace")) {
        WorkspaceDocument workspace = workspaceStore->snapshot();
        if (workspace.containers.isEmpty()) {
            ContainerDocument main; main.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            main.name = tr("Main Panel"); main.layout = ContentLayout::VerticalStack;
            workspace.mainContainerId = main.id; workspace.containers.append(main);
        }
        // Legacy floating containers stored their actual top-level rectangle
        // separately from the child container record. Adopt it into the document
        // once so that stale legacy keys never compete on later startups.
        for (auto& c : workspace.containers) {
            if (c.dockMode!=DockMode::Floating) { continue; }
            const auto fields=c.config.value("floatingGeometry").toString().split(',');
            if(fields.size()!=4) { continue; }
            bool ok[4]; int rect[4]; for(int i=0;i<4;++i) { rect[i]=fields[i].toInt(&ok[i]); }
            if(ok[0] && ok[1] && ok[2] && ok[3] && rect[2]>0 && rect[3]>0) { c.geometry=QRect(rect[0],rect[1],rect[2],rect[3]); }
        }
        QSet<QString> placed;
        for (const auto& c : workspace.containers) { for (const auto& e : c.contents) { placed.insert(e.typeId); } }
        for (auto& c : workspace.containers) {
            if (c.id != workspace.mainContainerId) { continue; }
            for (const auto& descriptor : contentRegistry->descriptors()) {
                if (!descriptor.singleton || placed.contains(descriptor.typeId)) { continue; }
                auto entry = contentRegistry->makeEntry(descriptor.typeId);
                if (descriptor.typeId == "applet:s_meter") { c.contents.prepend(entry); }
                else if (descriptor.typeId == "applet:mod_monitor") { c.contents.insert(qMin(1,c.contents.size()),entry); }
                else { c.contents.append(entry); }
            }
        }
        // Project old per-applet floating intent into committed shells, retaining
        // original flags/geometry and a stable return location as migration data.
        QVector<ContainerDocument> shells;
        for (auto& c : workspace.containers) {
            for (int i = c.contents.size()-1; i >= 0; --i) {
                auto entry = c.contents[i];
                if (!entry.typeId.startsWith("applet:") || !entry.config.value("floating").toBool()) { continue; }
                entry.returnLocation = ReturnLocation{c.id, i>0?c.contents[i-1].id:QString(), i+1<c.contents.size()?c.contents[i+1].id:QString(),{}};
                ContainerDocument shell; shell.id = QUuid::createUuid().toString(QUuid::WithoutBraces); shell.name = entry.name;
                shell.layout = ContentLayout::VerticalStack; shell.dockMode = DockMode::Floating; shell.popOutShell = true;
                shell.config["legacyAppletFloatGeometry"] = entry.config.value("floatGeometry");
                shell.contents.append(entry); shells.append(shell); c.contents.removeAt(i);
            }
        }
        workspace.containers += shells;
        m_containerManager->commitWorkspace(workspace, workspace.revision);
    }
    connect(m_containerManager, &ContainerManager::workspaceReconciled, this, [this] {
        if (m_shuttingDown) { return; }
        for (ContainerWidget* c : m_containerManager->allContainers()) {
            watchContainerItems(c->content());
            if (m_meterPoller) { if (auto* host = m_containerManager->contentHost(c->id())) {
                for (const auto& row : host->entryRows()) {
                    if (row.widget && row.widget == m_containerManager->contentRegistry()->singletonView("applet:s_meter")) { m_meterPoller->setSMeterContext(row.context); }
                }
            } }
        }
        refreshContainerControls();
    });

    // Phase 3P-I-a T17 — push board caps into every container so
    // AntennaButtonItems gate their click handler on hasAlex. Re-runs
    // when the active radio changes (currentRadioChanged) and also fires
    // when a new container is added (containerAdded). Without this,
    // freshly-created or restored containers keep the default
    // m_hasAlex=true and would allow clicks on HL2/Atlas.
    auto pushCapsToAllContainers = [this]() {
        const auto caps = m_radioModel->boardCapabilities();
        for (ContainerWidget* c : m_containerManager->allContainers()) {
            c->setBoardCapabilities(caps);
        }
    };
    connect(m_radioModel, &RadioModel::currentRadioChanged, this,
            pushCapsToAllContainers);

    // Per-SKU power-meter rescale.  Bench-reported #167 follow-up: the
    // top MeterPanel BarItem stack ships with a 0-120 W default that
    // makes HL2 (5 W) a sliver and ANAN-G2-1K (1000 W) saturate.  When
    // the active radio changes, ask every MeterWidget to rescale its
    // PowerBar / PowerScale pair to the new SKU's PA ceiling.  Same
    // paMaxWattsFor() helper TxApplet uses for its RF Pwr HGauge so
    // both meter surfaces share a single source of truth.
    auto rescaleAllPowerMeters = [this]() {
        const HPSDRModel m = m_radioModel->hardwareProfile().model;
        const int maxW     = paMaxWattsFor(m);
        if (m_meterPoller) { m_meterPoller->rescalePowerMeters(maxW); }
        for (ContainerWidget* c : m_containerManager->allContainers()) {
            for (MeterWidget* mw : c->findChildren<MeterWidget*>()) {
                mw->rescalePowerMeters(maxW);
            }
        }
    };
    connect(m_radioModel, &RadioModel::currentRadioChanged, this,
            rescaleAllPowerMeters);

    // Phase 3F Sub-Epic D Task 11: gate the bottom-bar CH 1 stacked
    // indicator widget on whether the connected radio drives a second RX
    // filter chain. buildStatusBar() ships chain1Widget hidden by default;
    // this slot toggles it on/off as the user switches radios.
    //
    // Defect D4: this used to read adcCount, which names the wrong thing. The
    // indicator reports a CHAIN's band-pass state, and ANAN-100D / ANAN-200D
    // have two ADCs behind one filter bank: the setAlex2HPF model list at
    // Thetis console.cs:15435-15443 [v2.10.3.15] never hands them a second
    // filter word. Showing CH 1 there offered the operator a second chain to
    // reason about, and a Filter Policy override to set on it, that the radio
    // does not have.
    // Upstream inline attribution preserved verbatim (console.cs:15441):
    //   HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
    // Registered with m_chromeBar at rung 4 (design §6); the >=2 fact is
    // reported via setItemAvailable, not a direct setVisible call, per
    // ChromeBarController::setItemAvailable's own doc comment.
    auto updateChain1Visibility = [this]() {
        if (!m_chain1IndicatorWidget) { return; }
        const auto caps = m_radioModel->boardCapabilities();
        if (m_chromeBar && m_chromeBarWidget) {
            m_chromeBar->setItemAvailable(m_chain1IndicatorWidget,
                                          caps.rxFilterChainCount >= 2);
            m_chromeBar->relayout(m_chromeBarWidget->width());
        }
    };
    connect(m_radioModel, &RadioModel::currentRadioChanged, this,
            updateChain1Visibility);

    // One view of local hardware state or Core telemetry drives both CH labels
    // and WIDE badges. A remote client never consults its inert Alex controller.
    auto refreshFilterIndicators = [this]() {
        for (int adc = 0; adc < 2; ++adc) {
            const auto& state = m_radioModel->filterChainState(adc);
            auto* lbl = findChild<QLabel*>(
                QStringLiteral("chainIndicator%1").arg(adc));
            if (!lbl) { continue; }
            const bool available = m_radioModel->filterChainStateAvailable(adc);
            lbl->setText(available ? state.reasonText
                : (m_radioModel->isConnected() ? tr("awaiting Core") : tr("offline")));
            // Shared-input filters, ruling (d): which slice holds the
            // receive low-pass on this input and what that costs the
            // slices below it, and on the HL2 which slice needs the
            // broadcast-band high-pass off. Empty when neither applies.
            lbl->setToolTip(available ? state.lowPassReason : QString());
            const QString color =
                (available && state.effective == AlexController::BpfEffective::Filtered)
                    ? Style::kGreenText
                    : Style::kAmberWarn;
            lbl->setStyleSheet(
                QStringLiteral("color: %1; font-size: 9px; font-weight: bold;")
                    .arg(color));

            // reasonText ranges from "idle" to
            // "BYPASS (multi-band: 160m + 80m + 40m + 20m + 10m)", so the
            // owning chain widget's sizeHint (registered with m_chromeBar
            // at rung 4) just went stale. Report it so folding stays a
            // pure function of width rather than overflowing on the next
            // band change (final-fix-wave finding 1).
            if (m_chromeBar && m_chromeBarWidget) {
                QWidget* chainWidget = (adc == 0) ? m_chain0IndicatorWidget
                                                   : m_chain1IndicatorWidget;
                if (chainWidget) {
                    m_chromeBar->setNaturalWidth(
                        chainWidget, chainWidget->sizeHint().width());
                    m_chromeBar->relayout(m_chromeBarWidget->width());
                }
            }
        }
        refreshPanWideBadges();
    };
    connect(m_radioModel, &RadioModel::filterStateChanged,
            this, refreshFilterIndicators);
    // The CH tooltip and the WIDE reason carry the low-pass hold, which
    // has its own notifier.
    connect(m_radioModel, &RadioModel::lowPassHoldChanged,
            this, refreshFilterIndicators);
    connect(m_radioModel, &RadioModel::connectionStateChanged,
            this, refreshFilterIndicators);
    refreshFilterIndicators();

    // R-R3-21 / R-R3-49: the container buttons' targets. The transmit
    // gate, the panadapter of a slice and this computer's VAX outputs live
    // on this window; the rest on RadioModel. (No Power button: maintainer
    // decision 2026-09-30.)
    {
        ContainerButtonDispatcher::Hooks hooks;
        hooks.desktopHosting = [this] { return desktopHosting(); };
        // Slice control plan Task 15 fix round 1: a slice this window
        // listens to refuses the slice buttons with the RX applet's reason.
        hooks.sliceRefusal = [this](int sliceId) { return sliceChangeRefusal(sliceId); };
        hooks.desktopMoxOn = [this] {
            return desktopOwnsTransmit() && m_radioModel->moxController()
                && m_radioModel->moxController()->isMox();
        };
        hooks.desktopTuneOn = [this] {
            return desktopOwnsTransmit() && m_radioModel->isTune();
        };
        hooks.requestDesktopMox = [this](bool on) { requestDesktopTransmit(false, on); };
        hooks.requestDesktopTune = [this](bool on) { requestDesktopTransmit(true, on); };
        // Fix wave (hosting 2-TONE parity): 2TONE asks as MOX does.
        hooks.requestDesktopTwoTone = [this](bool on) {
            requestDesktopKey(DesktopStationController::Key::TwoTone, on);
        };
        hooks.transmitPermitted = [this] { return transmitControlsPermitted(); };
        // R-R3-49 (parity Task 2): MON is a transmit setting (version 2).
        // R-R3-49 (parity Task 7): PS-A arms PureSignal (version 7).
        // Fix wave GUI-I5: with the holder rule applyRemoteRoleGating gives
        // the TX applet's MON and PS-A (iPhone app plan Task 77, rulings
        // 7.7 and 8.4): while another device holds transmit they are shown
        // disabled with the Core's holder reason; a reason already shown
        // (the setting not taken, the radio on the air) stays.
        const auto otherHolder = [this] {
            return m_stationClient && m_stationClient->knowsTransmitHolder()
                ? m_stationClient->otherHolderReason()
                : QString();
        };
        hooks.transmitSettingsPermitted = [this, otherHolder] {
            return otherHolder().isEmpty() && transmitSettingsPermitted(2);
        };
        hooks.transmitSettingsReason = [this, otherHolder] {
            const QString holder = otherHolder();
            return holder.isEmpty() || !transmitSettingsPermitted(2) ? transmitSettingsReason(2)
                                                                     : holder;
        };
        hooks.pureSignalArmingPermitted = [this, otherHolder] {
            return otherHolder().isEmpty() && pureSignalArmingPermitted();
        };
        hooks.pureSignalArmingReason = [this, otherHolder] {
            const QString holder = otherHolder();
            return holder.isEmpty() || !pureSignalArmingPermitted() ? pureSignalArmingReason()
                                                                    : holder;
        };
        hooks.remoteTransmitReason =
            tr("Remote transmit controls are not available from this Core.");
        // Desktop remote transmit: the Core's own reason when it gave one.
        hooks.remoteTransmitReasonNow = [this] { return remoteTransmitReason(); };
        hooks.spectrumFor = [this](SliceModel* s) { return spectrumForSlice(s); };
        // Parity Task 31 (A11): DUP, the window's DisplayDuplex setting, as
        // View > Display duplex (DUP) sets it.
        hooks.displayDuplexOn = [this] { return m_displayDuplexSetting; };
        hooks.setDisplayDuplex = [this](bool on) { setDisplayDuplexSetting(on); };
        hooks.displayDuplexReason = [this] {
            return MoxDisplayController::displayDuplexUnavailableReason(m_radioModel);
        };
        // R-R3-49: VAX 1 / VAX 2 open and close this computer's VAX
        // outputs, as Setup > Audio > VAX does (live in a remote window).
        hooks.vaxDevices = m_radioModel->localAudioDevices();
        // Fix wave M3: VAX 1 / VAX 2 light at once when Setup > Audio > VAX
        // (or anything else) opens or closes this computer's VAX output.
        if (hooks.vaxDevices) {
            connect(hooks.vaxDevices, &AudioEngine::vaxBusOpenChanged, this,
                    [this](int) { refreshContainerControls(); });
        }
        m_containerButtons = std::make_unique<ContainerButtonDispatcher>(
            m_radioModel, std::move(hooks));
    }

    connect(m_containerManager, &ContainerManager::containerAdded, this,
            [this](const QString& id) {
        if (auto* c = m_containerManager->container(id)) {
            c->setBoardCapabilities(m_radioModel->boardCapabilities());
            wireContainerControls(c);
        }
    });
    connect(m_containerManager, &ContainerManager::containerRemoved, this,
            [this](const QString&) { reconcileMiniDisplays(); });
    // R-R3-21: every slice's state reaches the containers set to it.
    connect(m_radioModel, &RadioModel::sliceAdded, this,
            [this](int) { watchSlicesForContainers(); });
    connect(m_radioModel, &RadioModel::sliceRemoved, this,
            [this](int) { watchSlicesForContainers(); });
    connect(m_radioModel, &RadioModel::transmittingChanged, this,
            [this](bool) {
        for (const auto& [sliceId, producer] : m_miniProducers) {
            producer->txTrace.clear();
            producer->txWaterfall.clear();
            clearMiniSlice(sliceId);
        }
        reconcileMiniDisplays();
    });
    // The function buttons' global targets: they light from the target.
    {
        const auto refresh = [this]() { refreshContainerControls(); };
        connect(&m_radioModel->transmitModel(), &TransmitModel::monEnabledChanged,
                this, refresh);
        connect(m_radioModel, &RadioModel::tuneRefused, this, refresh);
        connect(m_radioModel, &RadioModel::connectionStateChanged, this, refresh);
        // Desktop remote transmit: a remote window's MOX, TUNE and 2-TONE
        // light from the Core's state.
        connect(m_radioModel, &RadioModel::transmittingChanged, this, refresh);
        connect(m_radioModel, &RadioModel::remoteTransmitRefused, this, refresh);
        connect(&m_radioModel->transmitModel(), &TransmitModel::tuneChanged, this, refresh);
        // Task 16: receive only disables TUN, MOX and 2TONE.
        connect(m_radioModel, &RadioModel::rxOnlyChanged, this, refresh);
        // HL2 port part 2: so does a TX inhibit, with its reason.
        connect(m_radioModel, &RadioModel::txInhibitedChanged, this, refresh);
        connect(m_radioModel, &RadioModel::txInhibitReasonChanged, this, refresh);
        // TX safety (2026-09-30): and a lost radio link, until it is back.
        connect(m_radioModel, &RadioModel::radioLinkDownChanged, this, refresh);
        // Fix round 1 (minor 4): the link-down words follow the window's
        // link to the Core and the Core's waiting for a radio.
        connect(m_radioModel, &RadioModel::stationLinkStateChanged, this, refresh);
        connect(m_radioModel, &RadioModel::stationRadioWaitingChanged, this, refresh);
        if (MoxController* mox = m_radioModel->moxController()) {
            connect(mox, &MoxController::moxStateChanged, this, refresh);
            connect(mox, &MoxController::moxRejected, this, refresh);
            connect(mox, &MoxController::manualMoxChanged, this, refresh);
        }
        if (TwoToneController* twoTone = m_radioModel->twoToneController()) {
            connect(twoTone, &TwoToneController::twoToneActiveChanged, this, refresh);
        }
        if (PureSignalSessionFacade* ps = m_radioModel->pureSignalFacade()) {
            connect(ps, &PureSignalSessionFacade::statusChanged, this, refresh);
            if (PureSignalSettings* settings = ps->settings()) {
                connect(settings, &PureSignalSettings::autoCalEnabledChanged, this, refresh);
            }
        }
        if (NotchModel* notches = m_radioModel->notchModel()) {
            connect(notches, &NotchModel::globalEnabledChanged, this, refresh);
        }
    }
    watchSlicesForContainers();

    // Create the MeterPoller BEFORE restoreState / populateDefaultMeter
    // so the meterReadyForPolling signal fires into a live poller as
    // each container's MeterWidget is materialized. Previously the
    // poller was created later and only the panel container's
    // m_meterWidget was registered manually — every user-created
    // container's meter sat orphaned and bars never received setValue()
    // calls, the root of the "BarMeter not drawing" symptom.
    m_meterPoller = new MeterPoller(this);
    // Task 3.1: expose MeterPoller via RadioModel so MultimeterPage can
    // apply live interval + averaging-window changes without a MainWindow
    // round-trip.  Non-owning; RadioModel stores the pointer only.
    m_radioModel->setMeterPoller(m_meterPoller);
    // R-R3-32 (parity Task 6): the HW Volts, Amps and Temperature meters
    // read the one PA reading source (the Core's in a remote window).
    m_meterPoller->setPaReadingsModel(m_radioModel);
    // Task 3.2: expose ContainerManager via RadioModel so MultimeterPage
    // can broadcast unit-mode changes to all live MeterItems.
    m_radioModel->setContainerManager(m_containerManager);
    const auto cachedMaxBin = MeterPoller::panMaxBinSourceForSlice([this](const SliceModel* slice) -> SpectrumWidget* {
        if (!slice || !m_panStack || slice->streamIndex() < 0 || markerOnlyPlacement(slice->sliceIndex())) { return nullptr; }
        PanadapterApplet* pan=m_panStack->panadapter(windowPanFor(slice));
        SliceModel* displayed=pan ? m_radioModel->sliceById(pan->activeSliceIndex()) : nullptr;
        if (!displayed || displayed->streamIndex()!=slice->streamIndex() || displayed->streamEpoch()!=slice->streamEpoch()) { return nullptr; }
        return pan->spectrumWidget();
    });
    m_meterPoller->setSessionIdSource([this] { return containerSessionId(); });
    m_meterPoller->setRxReadingSource([this, cachedMaxBin](const QJsonObject& context, int binding) {
        const bool remote = m_radioModel->role() == RadioModel::Role::Remote;
        const bool ready = m_radioModel->isConnected() && (!remote || (m_stationClient && m_stationClient->isHandshakeComplete()));
        const bool extended = !remote || (m_stationClient && m_stationClient->capabilities().meterReadingsVersion >= 1);
        return ContainerSourceAdapter::reading(m_radioModel, context, windowRxSlice(), binding, ready, extended, cachedMaxBin, containerSessionId());
    });
    m_meterPoller->rescalePowerMeters(paMaxWattsFor(m_radioModel->hardwareProfile().model));
    m_containerManager->setPreviewPoller(m_meterPoller);
    connect(m_containerManager,&ContainerManager::previewPresentationRequested,this,[this](MeterWidget* meter,const QJsonObject& context){
        for(auto* item:meter->items()) {
            const auto source=item->property("containerSourceContext");
            refreshContainerMeter(nullptr,meter,item,source.isValid()?source.toJsonObject():context);
        }
    });
    connect(m_containerManager, &ContainerManager::meterContextReady, m_meterPoller, &MeterPoller::setTargetContext);
    connect(m_containerManager, &ContainerManager::meterReadyForPolling, this, [this](MeterWidget* meter) {
        if (!meter || !m_meterPoller) { return; }
        if (!m_containerManager->workspaceStore()) { m_meterPoller->addTarget(meter); }
        meter->rescalePowerMeters(paMaxWattsFor(m_radioModel->hardwareProfile().model));
    });
    // Shared GUI cadence also advances clocks/MMIO and previews while offline;
    // cache readiness above keeps every disconnected radio reading unavailable.
    m_meterPoller->start();

    m_containerManager->restoreState();
    for (auto* c : m_containerManager->allContainers()) {
        wireContainerControls(c);
        if (auto* host = m_containerManager->contentHost(c->id())) {
            for (MeterWidget* meter : host->meterSurfaces()) { m_meterPoller->setTargetContext(meter, host->sourceContext(meter)); }
        }
    }
    if (m_containerManager->containerCount() == 0 && !m_containerManager->workspaceStore()) {
        createDefaultContainers();
    }
    // Always populate the panel container's content (meters + applets).
    // On first run, createDefaultContainers() creates the shell; on restore,
    // restoreState() recreates the shell but content is lost. This ensures
    // the applet panel is always populated regardless of restore path.
    populateDefaultMeter();
    m_containerManager->restoreSplitterState();

    // Phase 3P-I-a T17 — initial push. `containerAdded` fires during
    // restoreState() but the content (and any AntennaButtonItems) are
    // installed after the signal by populateDefaultMeter() or the saved
    // content factory. Do a one-shot sweep here so the final items
    // pick up the startup board capabilities.
    pushCapsToAllContainers();

    // R-R3-21: Setup > Multimeter (average, unit, decimal, history
    // duration) and DSP > Options (high-resolution filter graph) reached
    // the meters only when their Setup page was opened, so a restart lost
    // them until then. Apply the saved values to the restored meters now.
    MultimeterPage::applyPersistedSettings(m_radioModel);
    DspOptionsPage::applyPersistedHighResFilter(m_radioModel);
    // R-R3-49 (parity Task 16): a remote window's filter graphs draw the
    // Core's curve as it arrives.
    connect(m_radioModel, &RadioModel::coreFilterResponseChanged, this,
            [this]() { DspOptionsPage::applyCoreFilterResponse(m_radioModel); });

    // R-R3-21: the DXCC country table the spot colouring resolves against.
    // cty.dat is bundled as the ":/cty.dat" resource (resources.qrc), as
    // AetherSDR loads it at startup (MainWindow.cpp:1469 [@1e0718ad]);
    // without it every spot resolved to no country.
    if (DxccColorProvider* dxcc = m_radioModel->dxccColorProvider()) {
        if (!dxcc->loadCtyDat()) {
            qCWarning(lcSpots) << "DXCC country table (:/cty.dat) did not load;"
                               << "spots will not be colored by country";
        }
    }

    // Default splitter sizes on first run: ~80% spectrum, ~20% panel
    if (!AppSettings::instance().contains(QStringLiteral("MainSplitterSizes"))) {
        m_mainSplitter->setSizes({1024, 256});
    }

    // Wire spectrum display to SliceModel (values come from persisted state,
    // no longer hardcoded). Connection is deferred to wireSliceToSpectrum()
    // which runs after RadioModel creates slice 0.
    connect(m_radioModel, &RadioModel::sliceAdded, this, [this](int index) {
        if (index == 0) {
            wireSliceToSpectrum();
        }
    });

    // Push restored slice state into spectrum + VFO views once
    // RadioModel::loadSliceState() completes.
    //
    // wireSliceToSpectrum() above runs at sliceAdded() time — BEFORE
    // loadSliceState() runs inside connectToRadio.  At that earlier moment
    // the slice still holds its pre-restore default values, so the
    // spectrum widget's m_ddcCenterHz, center freq, and VFO freq were
    // seeded with the default (typically 14.225 MHz / 20m).  After
    // loadSliceState() restores the persisted band/freq/mode/filter,
    // SliceModel::frequencyChanged is gated by qFuzzyCompare and by the
    // CTUN-shift branch in wireSliceToSpectrum's frequencyChanged lambda
    // (the offScreen path that calls setDdcCenterFrequency only fires
    // when persisted freq is OUTSIDE default ±halfBw).  Without an
    // explicit re-push, m_ddcCenterHz stays at the default and spectrum
    // bin labels point to the wrong band of RF until the first dial-
    // tune crosses the offScreen threshold.
    //
    // Mirrors Thetis txtVFOAFreq_LostFocus's unconditional Display.VFOA
    // / Display.CentreFreqRX1 push at console.cs:31272 + 15378
    // [v2.10.3.13], invoked from chkPower_CheckedChanged at
    // console.cs:27204 [v2.10.3.13] as the explicit "push state to
    // display" step on power-on.
    connect(m_radioModel, &RadioModel::sliceStateRestored, this,
            [this](int index) {
        if (index != 0 || !activeSpectrumWidget()) {
            return;
        }
        SliceModel* slice = m_radioModel->activeSlice();
        if (!slice) {
            return;
        }
        const double freq = slice->frequency();
        activeSpectrumWidget()->setCenterFrequency(freq);
        activeSpectrumWidget()->setDdcCenterFrequency(freq);
        activeSpectrumWidget()->setVfoFrequency(freq);
        activeSpectrumWidget()->setFilterOffset(slice->filterLow(), slice->filterHigh());
        if (VfoWidget* flag = m_vfoWidgetsBySlice.value(slice->sliceIndex())) {
            flag->setFrequency(freq);
            flag->setMode(slice->dspMode());
            flag->setFilter(slice->filterLow(), slice->filterHigh());
        }
    });

    // The dashboard follows the ACTIVE slice, not slice 0. It was pinned to
    // id 0 and never rebound, so after multi-pan landed (#312) an operator
    // working Slice B was shown Slice A's mode, filter, AGC and NR as
    // current. See design §4.2.
    auto rebindDashboard = [this]() {
        if (!m_rxDashboard || !m_radioModel) { return; }
        // Slice control plan Task 15: this window's RX slice, which may be
        // one it listens to.
        SliceModel* s = windowRxSlice();
        m_rxDashboard->bindSlice(s);
        // Slice control plan Task 13: the picker's words and an open
        // chooser follow every change of this window's slices.
        refreshSliceChooser();
        if (!s) { return; }
        // Use SliceModel::sliceLetter(), do NOT derive the letter here.
        // It is already derived from sliceIndex() upstream. It previously
        // returned a stored member defaulting to 'A', so every slice
        // reported 'A' and three call sites mislabelled their slices; see
        // the comment at SliceModel.h:503. Deriving locally would
        // reintroduce a second source of truth for the same fact.
        m_rxDashboard->setSliceLetter(s->sliceLetter());
    };
    connect(m_radioModel, &RadioModel::sliceAdded, this,
            [rebindDashboard](int) { rebindDashboard(); });
    connect(m_radioModel, &RadioModel::activeSliceChanged, this,
            [rebindDashboard]() { rebindDashboard(); });
    rebindDashboard();

    // Slice control plan Task 13: a new slice the chooser asked for is
    // answered by the slice or by the refusal. The picker's own connection
    // lives in buildStatusBar(), where the dashboard is made (Task 16: it
    // sat here, before the dashboard existed, and never connected).
    connect(m_radioModel, &RadioModel::sliceAdded, this, [this](int) {
        finishSliceChooserRequest(QByteArrayLiteral("addSlice"), true, QString());
    });
    connect(m_radioModel, &RadioModel::sliceAddRejected, this, [this](const QString& reason) {
        // A hosting add's answer is HostingSliceActions::finished (Task 10).
        if (m_hostingSlices && m_hostingSlices->invoking()) { return; }
        finishSliceChooserRequest(QByteArrayLiteral("addSlice"), false, reason);
    });
    connect(m_radioModel, &RadioModel::sliceRemoved, this, [this](int) { refreshSliceChooser(); });
    if (SliceOwnership* ownership = m_radioModel->sliceOwnership()) {
        connect(ownership, &SliceOwnership::markChanged, this,
                [this](int, const QByteArray&, const QByteArray&) {
            refreshSliceChooser();
            reconcileListenPlacements();
        });
        connect(ownership, &SliceOwnership::listenersChanged, this, [this](int) {
            refreshSliceChooser();
            reconcileListenPlacements();
            // Task 14a: a slice this window starts or stops listening to
            // gains or loses its flag.
            if (desktopHosting()) { refreshDesktopStationState(); }
        });
    }

    // Phase 3F: the status overlay's per-slice triggers. Topology changes
    // reach the overlay through rebuildFftRouting, but a retune or a mode
    // change moves no topology at all, so those need their own wire -- and
    // they are the ones an operator exercises on every VFO detent.
    //
    // Resolved by id, not list position: sliceAdded carries the stable slice
    // id (RadioModel.cpp addSlice hands out the lowest free one), which is a
    // position only until the operator removes a slice from the middle.
    connect(m_radioModel, &RadioModel::sliceAdded, this,
            [this](int sliceId) {
        wireSliceStatusOverlayTriggers(m_radioModel->sliceById(sliceId));
        refreshPanStatusOverlays();
    });
    // Slices that already exist. RadioModel creates Slice A at connect, so on
    // a reconnect this loop is what re-arms it; sliceAdded has already fired.
    for (SliceModel* existing : m_radioModel->slices()) {
        wireSliceStatusOverlayTriggers(existing);
    }

    // Phase 3F Sub-Epic D Task 13: bind newly-created slices to a pan
    // and register pan-to-receiver routing in the FFTRouter.
    //
    // RadioModel::addSlice stamps the requested pan id on the SliceModel via
    // setPanKey() (and the transitional initialPanId property) so this handler
    // knows where to dock the slice. If the slice was added by some other path
    // (no pan key), we fall back to the active pan. If the requested pan does
    // not exist yet (e.g. Add Panadapter ran between addSliceOnPan emit and
    // this slot), we create it on the fly via PanadapterStack::addPanadapter.
    connect(m_radioModel, &RadioModel::sliceAdded, this,
            [this](int sliceIndex) {
        if (!m_panStack) { return; }
        SliceModel* slice = sliceForAddedIdForTest(m_radioModel, sliceIndex);
        if (!slice) { return; }

        const QString panKey = slice->panKey();
        const QString targetPan = panKey.isEmpty()
                                      ? m_panStack->activePanId()
                                      : panKey;

        auto* applet = m_panStack->panadapter(targetPan);
        if (!applet) {
            applet = m_panStack->addPanadapter(targetPan);
        }
        if (applet) {
            applet->addSlice(sliceIndex);
        }

        // Phase 3F Sub-Epic I Task 9: re-derive the whole topology instead
        // of mapping this one pan. The old call keyed the router on
        // slice->ddcIndex(), which is the hardware DDC number (2..6 on
        // Saturn-class), while the FFTEngine pool is keyed on stream index
        // (0..userDdcCount-1) -- plan invariant 3. It also ran too early for
        // Slice A: the pool is not sized until connect, so Slice A's
        // addSlice-time bind is a no-op and its streamIndex is still -1 here.
        // The streamBindingsChanged rebuild picks it up once it binds.
        rebuildFftRouting();

        // Phase 3F hotfix 2026-05-27: create a per-slice VfoWidget so
        // operators can see + interact with the new slice.  The existing
        // wireSliceToSpectrum() above runs for slice 0 (Slice A) and
        // creates Slice A's flag; Slice B+ never had a flag, so multi-slice
        // was invisible.  We add the UI surface here and wire the same
        // intent signals (Sub-Epic C Task 9 + Sub-Epic E Task 4) plus
        // the bidirectional freq/mode/filter/AGC/gain/antenna/step state
        // signals so the new flag stays in sync with its SliceModel and
        // user clicks on the new flag propagate back to the model.
        //
        // What's intentionally NOT done here: per-slice rxChannel/CTUN/
        // MaxBin/NR/ANF DSP wiring (those hardcode rxChannel(0) today
        // and are the actual Phase 3F multi-slice DSP epic).  This hotfix
        // is "make the flag appear + let the operator interact"; the
        // DSP-side per-slice fanout is the next sub-epic.
        // VFO flag crash lane fix round (2026-10-01): Slice A's flag is
        // still built by wireSliceToSpectrum() (the index==0 handler at the
        // top of this file), but Slice A no longer returns here before the
        // pan-move handler below. A remote window is sent a slice before
        // its pan key, so a Slice A made again on another pan was docked on
        // the active pan and, with no handler, stayed there.
        qCInfo(lcReceiver).noquote()
            << QStringLiteral("Slice %1 added in this window").arg(QChar(u'A' + sliceIndex));
        if (sliceIndex != 0) {
            if (m_vfoWidgetsBySlice.contains(sliceIndex)) {
                return;
            }
            // Phase 3F: route the new flag to the SpectrumWidget that hosts the
            // slice's pan (resolved from slice->panKey() by spectrumForSlice),
            // NOT the active pan. This is Bug 1's fix: previously the flag landed
            // on the active pan and stacked on pan-0 instead of the pan showing
            // the slice's band. Ported from AetherSDR's sliceAdded path
            // (MainWindow.cpp:11581 [@6a142807]).
            SpectrumWidget* sw = spectrumForSlice(slice);
            if (!sw) { return; }
            createSliceFlag(slice, sw);
        }

        // Phase 3F: migrate the flag when the slice's owning pan changes.
        // Remove the VfoWidget from every pan's SpectrumWidget, then re-add
        // + re-wire it on the new pan resolved by spectrumForSlice(). This
        // is the panKeyChanged half of Bug 1's fix (the sliceAdded path
        // above handles initial placement). Ported from AetherSDR's
        // panIdChanged migration (MainWindow.cpp:11560 [@6a142807]).
        connect(slice, &SliceModel::panKeyChanged, this,
                [this, slice](const QString& newPanKey) {
            const int idx = slice->sliceIndex();
            // The pan-to-slice association has to move with the flag. Without
            // this the pan the slice left kept listing it in
            // associatedSlices() and could keep it as that pan's active
            // slice, so the old pan went on tuning, and painting the CH tag
            // for, a slice now shown somewhere else. Ahead of the flag
            // rebuild's early return below so the association is corrected on
            // any path that reaches this handler.
            if (m_panStack) {
                // Slice control plan Task 16: a slice this window placed
                // stays where it was placed when its controller moves it.
                const QString placed = m_listenPlacement.value(idx);
                const QString dest =
                    !placed.isEmpty() && m_panStack->panadapter(placed) ? placed : newPanKey;
                // Slice control plan Task 17: a pan key naming a pan this
                // window does not have gets that pan, as sliceAdded gives
                // it. A remote window is sent a slice before its pan key,
                // so sliceAdded docked it on the active pan; left there, a
                // slice this window only listens to sat on the operator's
                // pan and read as shown, so a listen never placed it.
                if (!dest.isEmpty() && !m_panStack->panadapter(dest)) {
                    m_panStack->addPanadapter(dest);
                }
                m_panStack->moveSliceToPan(idx, dest);
            }
            // VFO flag crash lane fix round (2026-10-01): Slice A moves too
            // (the early return that sat here for it is gone), and a flag
            // already on the slice's pan is left alone rather than rebuilt.
            SpectrumWidget* dest = spectrumForSlice(slice);
            VfoWidget* flag = m_vfoWidgetsBySlice.value(idx, nullptr);
            if (!flag || flag->parentWidget() != dest) {
                if (m_panStack) {
                    for (auto* applet : m_panStack->allApplets()) {
                        if (applet && applet->spectrumWidget()) {
                            applet->spectrumWidget()->removeVfoWidget(idx);
                        }
                    }
                }
                m_vfoWidgetsBySlice.remove(idx);
                if (dest) { createSliceFlag(slice, dest); }
            }
            // Phase 3F Sub-Epic I Task 9: the slice now feeds a different
            // pan, so the FFT topology has to follow. Unconditional: the
            // model changed even when the destination pan does not exist
            // yet and no flag could be built.
            rebuildFftRouting();
        });
    });

    // Phase 3F Sub-Epic D Task 13: detach a removed slice index from
    // every pan. The associatedSlices set is small (single-digit) so a
    // linear scan across all applets is fine.
    connect(m_radioModel, &RadioModel::sliceRemoved, this,
            [this](int sliceIndex) {
        if (m_panStack) {
            for (auto* applet : m_panStack->allApplets()) {
                if (applet) { applet->removeSlice(sliceIndex); }
            }
        }
        // Phase 3F hotfix 2026-05-27: remove the per-slice VfoWidget
        // when its slice goes away.
        //
        // VFO flag crash lane (2026-09-30): Slice A's flag too. It was
        // skipped here on the grounds that Slice A never goes, but a Core
        // closes Slice A in a remote window (the claims rule, or its X on a
        // listened flag), and the flag it left behind kept live buttons
        // wired to the freed slice: a lock click crashed in
        // SliceModel::lockedChanged, and a Slice A made again found this
        // stale flag in m_vfoWidgetsBySlice and was never given one of its
        // own, so it did not paint. Every flag is built by createSliceFlag
        // and goes here; m_vfoWidgetsBySlice is its one owner.
        //
        // Phase 3F (Bug 1 follow-up): the flag may live on a non-active pan
        // now that flags route to spectrumForSlice(). Remove from EVERY pan's
        // SpectrumWidget (removeVfoWidget on a pan that doesn't host it is a
        // no-op) instead of just the active pan, which would leak the flag.
        qCInfo(lcReceiver).noquote()
            << QStringLiteral("Slice %1 removed from this window").arg(QChar(u'A' + sliceIndex));
        if (m_vfoWidgetsBySlice.contains(sliceIndex)) {
            VfoWidget* victim = m_vfoWidgetsBySlice.take(sliceIndex);
            if (victim) {
                bool removed = false;
                if (m_panStack) {
                    for (auto* applet : m_panStack->allApplets()) {
                        if (applet && applet->spectrumWidget()) {
                            applet->spectrumWidget()->removeVfoWidget(sliceIndex);
                            removed = true;
                        }
                    }
                }
                if (!removed) {
                    // Same orphan hazard as removeVfoWidget: the flag's
                    // floating buttons belong to the SpectrumWidget, so they
                    // outlive a bare delete and stay painted on the pan.
                    victim->destroyFloatingButtons();
                    victim->deleteLater();
                }
            }
        }
        // Phase 3F Sub-Epic I Task 9: the removed slice may have been the
        // last one feeding its pan, or the last one on its stream. Rebuild
        // from the surviving slice set rather than unsubscribing this pan,
        // which would also drop any co-hosted slices still showing on it.
        rebuildFftRouting();
    });

    // Phase 3F Sub-Epic F Task 6: route wideband bins from RadioModel into
    // the active pan's SpectrumWidget.  RadioModel emits one
    // widebandSpectrumReady per ADC per assembled frame; we forward the
    // bins to the currently-active SpectrumWidget which silently stores
    // them per ADC.  Per-pan ADC routing (so extended-pan views on the
    // correct ADC's bins) lands in Sub-Epic F polish (T7-T10).
    connect(m_radioModel, &RadioModel::widebandSpectrumReady, this,
            [this](int adcIdx, const QVector<float>& dbmBins) {
        fanWidebandBinsForTest(m_panStack, adcIdx, dbmBins);
    });

    // Create the FFT engine pool (spectrum thread from architecture).
    // Sample rate starts at P2 default (768k); RadioModel::wireSampleRateChanged
    // updates it to the actual wire rate on each connect (P1=192k, P2=768k).
    //
    // R1 Task 6: per-stream engine lifecycle, the four global display
    // AppSettings keys, and the shared FFT thread now live in FftEnginePool
    // (src/core/spectrum/FftEnginePool.h) instead of this QWidget.
    //
    // Fix round 1 finding 1: this used to load the config here, once, with
    // no other call site ever refreshing it -- so a stream built later (a
    // second DDC claimed after the user changed Setup -> Display) came up
    // on whatever was captured at this exact moment, not the live value.
    // Pre-extraction, createFftEngineForStream read AppSettings inside its
    // own per-engine construction, so every new stream was current as of
    // ITS OWN creation. refreshFftPoolConfig() restores that: it is called
    // from ensureStreamWired() immediately before building any stream that
    // does not exist yet, stream 0 here included, so there is nothing left
    // to do in this function beyond creating the pool itself. Only stream
    // 0's engine is built here: the SKU's stream count is not known until
    // connect, and the rest are built on demand as the allocator claims
    // their DDCs (see the streamCentreChanged handler below).
    m_fftEnginePool = new FftEnginePool(this);
    ensureStreamWired(0);

    // Linear-power frame -> every pan subscribed to its stream.  One
    // connection for the whole pool (fftFrameReady carries the stream index),
    // replacing the per-engine connect that used to live inside
    // createFftEngineForStream.  dispatchFftFrameToPans consults the
    // FFTRouter and pushes the frame to every pan subscribed to that stream.
    // fftReadyLinear/fftFrameReady carry the raw |X[k]|² bins plus windowEnb +
    // dbmOffset metadata so the detector + avenger pipeline reproduces the
    // legacy fftReady dBm output (FFTEngine.cpp:348 [v2.10.3.13]) at
    // display-pixel resolution.  fftReady (full-bin dBm) stays a separate,
    // per-engine signal for chrome / AGC consumers (ClarityController,
    // NoiseFloorTracker) wired below; those stay on the primary engine
    // because each is a single global consumer.
    connect(m_fftEnginePool, &FftEnginePool::fftFrameReady,
            this, &MainWindow::dispatchFftFrameToPans);

    connect(m_radioModel, &RadioModel::wireSampleRateChanged,
            this, [this](double rateHz) {
        // Every stream on a P1 radio shares the wire rate, and on P2 the
        // per-stream rate published by streamCentreChanged is derived from
        // the same value, so the whole pool follows this signal.
        if (m_fftEnginePool) {
            for (int streamIndex : m_fftEnginePool->streams()) {
                FFTEngine* engine = m_fftEnginePool->engineForStream(streamIndex);
                if (!engine) { continue; }
                QMetaObject::invokeMethod(engine, [engine, rateHz]() {
                    engine->setSampleRate(rateHz);
                });
            }
        }
        if (activeSpectrumWidget()) {
            activeSpectrumWidget()->setSampleRate(rateHz);
            // Phase 3G-12: preserve the user's current zoom level across
            // sample rate changes. Only reset the visible span if the
            // current bandwidth would now exceed the new DDC sample rate
            // (in which case we clamp to full-span).
            // Against the EXTENDED ceiling, not the DDC rate.
            //
            // A span past the DDC rate is a legitimate state now: it is the
            // one and only trigger for extended mode. Clamping to rateHz here
            // meant any reconnect or rate change silently collapsed an
            // extended view back onto the DDC, and the operator's zoom was
            // gone with no wings and no explanation. setDisplayWindowClamped
            // reads the DDC rate when extended view is switched off, so the
            // ordinary case is unchanged. Codex, PR #318.
            activeSpectrumWidget()->setDisplayWindowClamped(
                activeSpectrumWidget()->centerFrequency(),
                activeSpectrumWidget()->bandwidth());
        }
    });
    // Spectrum/waterfall FPS — load persisted value (default 30), apply to
    // BOTH the FFT engine (row production cadence) and the SpectrumWidget
    // display timer (paint cadence) so the two stay locked.  Without this
    // load, FFTEngine defaulted to 30 and SpectrumWidget defaulted to its
    // own 30 fps constructor value, but Setup -> Display -> Spectrum
    // changes did not survive restart.  Persistence write side is in
    // SpectrumDefaultsPage::pushFps.
    //
    // R1 Task 6: the engine half of this (setOutputFps), plus the persisted
    // FFT size / window / Hz-per-bin target that used to follow it, now live
    // in FftEnginePool's config (set above) so every pooled engine comes up
    // on the same display knobs, including ones created later.  Only the
    // SpectrumWidget half is left here.
    {
        const int persistedFps = qBound(1,
            AppSettings::instance().value(
                QLatin1String(ControlRanges::kDisplaySpectrumFpsKey),
                QString::number(ControlRanges::kDisplaySpectrumFpsDefault)).toString().toInt(),
            60);
        if (activeSpectrumWidget()) {
            activeSpectrumWidget()->setDisplayFps(persistedFps);
        }
    }

    // 2026-05-25/26 KG4VCF bench fix (elevate the spectrum FFT thread to
    // USER_INTERACTIVE so compile workers can't win the time-slice race
    // against live spectrum production -- see
    // src/core/audio/RealtimeAudioPriority.h) now lives inside
    // FftEnginePool itself: it owns the thread(s), so it elevates each one
    // the moment it starts rather than MainWindow reaching in via
    // primaryFftEngine() as connection context.  Per-engine cleanup on
    // stream removal (removeStream()'s deleteLater()) and the raw I/Q feed
    // (wired in ensureStreamWired() above) are the only two lifecycle
    // pieces still on this side of the pool boundary.

    // ── PR #212 follow-up bench fix (J.J. KG4VCF, 2026-05-07) ────────────
    // TxAnalyzer drives the panadapter from the WDSP TX siphon during MOX.
    // Created here on the main thread; MOX-aware source-switch is wired
    // below in the MoxController connect block.  See TxAnalyzer.h header
    // for the design rationale and Thetis source-first cite map.
    // R2 remote clients own no local DSP and receive no media. Keep the TX
    // analyzer on the local role so a mirrored station MOX state cannot start
    // a local WDSP analyzer or suppress a receive pan on the client.
    if (m_radioModel->ownsLocalDsp()) {
        // R-R3-39: its WDSP calls run on the transmit lane, not here.
        m_txAnalyzer = new TxAnalyzer(TxAnalyzer::kTxDispId, this,
                                      m_radioModel->transmitLane());
        // TX dsp_rate 96 kHz and Thetis's 15 fps; nereusd (DaemonApp) gives
        // its analyzer the same set-up.
        m_txAnalyzer->applyStationRates();
        // Phase 3M-5d: expose TxAnalyzer on RadioModel so Setup Display TX
        // page can reach it without depending on MainWindow.
        m_radioModel->setTxAnalyzer(m_txAnalyzer);
        if (TxDisplayFeed* feed = m_radioModel->txDisplayFeed()) {
            const auto showMiniTx = [this, feed](const QVector<float>& dbm,
                                                  bool waterfall) {
                if (!m_radioModel || !m_radioModel->isTransmitting()
                    || !feed->miniReady()) { return; }
                SliceModel* slice = m_radioModel->txBoundSlice();
                if (!slice) { return; }
                auto it = m_miniProducers.find(slice->sliceIndex());
                if (it == m_miniProducers.end() || it->second->slice != slice) { return; }
                MiniProducer& producer = *it->second;
                (waterfall ? producer.txWaterfall : producer.txTrace) = dbm;
                if (producer.txTrace.size() != 1024 || producer.txWaterfall.size() != 1024) {
                    return;
                }
                const TxDisplayView view = feed->miniView();
                presentMiniFrame(slice->sliceIndex(), producer.txTrace,
                                 producer.txWaterfall, view.centreHz(),
                                 view.spanHz(), true, waterfall);
            };
            connect(feed, &TxDisplayFeed::miniTraceReady, this,
                    [showMiniTx](const QVector<float>& dbm) { showMiniTx(dbm, false); });
            connect(feed, &TxDisplayFeed::miniWaterfallReady, this,
                    [showMiniTx](const QVector<float>& dbm) { showMiniTx(dbm, true); });
        }
    }
    // Filter passband + n_pix get re-applied on every MOX-up edge in the
    // MoxController connect block below — the active slice's mode/filter
    // and the SpectrumWidget's laid-out width aren't known yet at ctor
    // time.

    // Phase 3G-8: expose view hooks on RadioModel so Display setup pages can
    // reach the renderer / FFT engine without depending on MainWindow.
    // R1 Task 4 fix round 1: setSpectrumHooks sets both of RadioModel's
    // spectrum pointers from one cached widget; see its declaration in
    // MainWindow.h and the activePanChanged handler earlier in this file
    // for the matching re-wire when the active pan changes.
    setSpectrumHooks(activeSpectrumWidget());
    m_radioModel->setFftEngine(primaryFftEngine());
    m_radioModel->setFftEnginePool(m_fftEnginePool);

    // Phase 3F Sub-Epic I Task 8: follow each stream's DDC centre + rate.
    //
    // RadioModel::bindSliceToStream emits this whenever the allocator claims
    // or moves a DDC.  The pans showing the stream must recentre with it, or
    // SpectrumWidget::visibleBinRange maps the incoming bins against a stale
    // window.  Also cached, because at emit time the router may not yet know
    // which pan shows the stream (see m_streamWindows in the header).
    connect(m_radioModel, &RadioModel::streamCentreChanged, this,
            [this](int streamIndex, double centreHz, int sampleRateHz) {
        m_streamWindows.insert(streamIndex, StreamWindow{centreHz, sampleRateHz});
        // This signal is emitted exactly when the allocator claims or moves
        // a DDC, which is the only way a stream starts producing I/Q, so it
        // is also the right moment to build that stream's engine. No-op
        // after the first time.
        if (FFTEngine* engine = ensureStreamWired(streamIndex)) {
            QMetaObject::invokeMethod(engine, [engine, sampleRateHz]() {
                engine->setSampleRate(static_cast<double>(sampleRateHz));
            }, Qt::QueuedConnection);
        }
        if (m_radioModel) {
            if (auto* router = m_radioModel->fftRouter()) {
                for (const QString& panId : router->pansForReceiver(streamIndex)) {
                    applyStreamWindowToPan(panId, streamIndex);
                }
            }
        }
    });

    // Phase 3F Sub-Epic I Task 9: any change to which slices sit on which
    // stream re-derives the FFT topology. This is the authoritative trigger:
    // it fires from bindSliceToStream AFTER SliceModel::streamIndex is set,
    // including for Slice A's deferred bind at connect (the pool is not
    // sized when Slice A is created, so its addSlice-time bind is a no-op
    // and the sliceAdded rebuild sees streamIndex == -1).
    connect(m_radioModel, &RadioModel::streamBindingsChanged, this,
            [this](int, const QVector<int>&) { rebuildFftRouting(); });

    // Sub-epic E: flush the rewind ring buffer when the radio disconnects so
    // a new session starts with a clean history. AetherSDR's clearDisplay()
    // did this implicitly; NereusSDR has no equivalent single-call reset, so
    // we plumb the connection-state signal through here. See
    // docs/architecture/phase3g-rx-epic-e-waterfall-scrollback-plan.md task 4.
    connect(m_radioModel, &RadioModel::connectionStateChanged, activeSpectrumWidget(),
            [this]() {
        if (!m_radioModel->isConnected() && activeSpectrumWidget()) {
            activeSpectrumWidget()->clearWaterfallHistory();
        }
    });

    // Phase 3Q-8: clicking the spectrum while disconnected opens ConnectionPanel.
    connect(activeSpectrumWidget(), &SpectrumWidget::disconnectedClickRequest,
            this, &MainWindow::connectionRequestedByOperator, Qt::UniqueConnection);

    // Wire BandPlanManager → SpectrumWidget so the bandplan strip renders on launch.
    activeSpectrumWidget()->setBandPlanManager(&m_radioModel->bandPlanManagerMutable());

    // Phase 3G-9b: no first-launch auto-apply of smooth defaults. Per
    // user decision 2026-04-15, the out-of-box waterfall stays on
    // WfColorScheme::Default; ClarityBlue is reachable only via the
    // "Reset to Smooth Defaults" button on SpectrumDefaultsPage or by
    // manually selecting "Clarity Blue" from the Waterfall Defaults combo.

    // --- Phase 3G-13: Step attenuator + ADC overload ---
    m_stepAttController = new StepAttenuatorController(this);
    m_radioModel->setStepAttController(m_stepAttController);
    // Parity Task 31 (A11): Thetis Display.TXAttenuatorOffset and
    // Display.RX1PreampOffset reach every pan (the keyed trace with display
    // duplex adds the first and leaves out the second, RX1Offset).
    {
        const auto pushKeyedOffsets = [this]() {
            if (!m_panStack || !m_radioModel || !m_stepAttController) { return; }
            for (PanadapterApplet* applet : m_panStack->allApplets()) {
                SpectrumWidget* pan = applet ? applet->spectrumWidget() : nullptr;
                if (!pan) { continue; }
                pan->setTxAttenuatorOffsetDb(
                    static_cast<float>(m_stepAttController->txAttenuatorOffsetDb()));
                pan->setRxPreampOffsetDb(static_cast<float>(m_radioModel->rxPreampOffsetDb()));
            }
        };
        connect(m_stepAttController, &StepAttenuatorController::txAttenuatorOffsetChanged,
                this, pushKeyedOffsets);
        connect(m_radioModel, &RadioModel::rxMeterOffsetChanged, this, pushKeyedOffsets);
        pushKeyedOffsets();
    }
    // R-R3-46 / R-R3-11: each band remembers its attenuator and preamp with
    // a local radio, as through the Core: the controller follows slice A's
    // receive band (Thetis rx1_band) and the transmit slice's band and mode
    // for ATT-on-TX, and sends a band's restored values to the radio. A
    // remote window is not wired (the Core does it).
    m_radioModel->followReceiveSliceWithStepAttenuator();

    // 3M-1a G.1 / F.2: MoxController::hardwareFlipped → StepAttenuatorController.
    // Both objects are now live; RadioModel owns MoxController, MainWindow owns
    // StepAttenuatorController. Wire here where both sides are accessible.
    // Qt::QueuedConnection documents cross-component intent (both main-thread)
    // and ensures the slot body runs after the emit call stack unwinds.
    // F.2 connect note: this is the connect deferred from StepAttenuatorController.h
    // line 257 ("The connect() call wiring this slot to MoxController::hardwareFlipped
    // is deferred to Task G.1").
    // From Thetis console.cs:29546-29576 [v2.10.3.13] — ATT-on-TX in HdwMOXChanged.
    // Inline attribution tags preserved verbatim from the cited range:
    //MW0LGE [2.9.0.7] added option to always apply 31 att from setup form when not in ps  [console.cs:29561]
    //[2.10.3.6]MW0LGE att_fixes  [original inline comment from console.cs:29567]
    //[2.10.3.6]MW0LGE att_fixes NOTE: this will eventually call Display.TXAttenuatorOffset with the value  [console.cs:29568]
    // Display.TXAttenuatorOffset = 0; //[2.10.3.6]MW0LGE att_fixes  [console.cs:29576]
    if (MoxController* mox = m_radioModel->moxController()) {
        connect(mox, &MoxController::hardwareFlipped,
                m_stepAttController, &StepAttenuatorController::onMoxHardwareFlipped,
                Qt::QueuedConnection);

        // H.1 (Phase 3M-1a): SpectrumWidget MOX overlay.
        // Wire MoxController::moxStateChanged → SpectrumWidget::setMoxOverlay.
        // From Thetis display.cs:1569-1593 [v2.10.3.13] Display.MOX setter:
        // the flag drives grid pen selection (tx_vgrid_pen red vs rx grey).
        // In 3M-1a we render a 3 px red border tint; full grid recolouring
        // is deferred to 3M-3.
        // Qt::QueuedConnection: MoxController and SpectrumWidget both live on
        // the main thread but a queued connection is used to match the deferred
        // pattern established for the hardwareFlipped connect above.
        // Revive merge (3M-5): the overlay is now set inside the TX-display
        // lambda below rather than bound here.
        //
        // Binding it here was wrong in two ways once multi-pan landed. It
        // resolved activeSpectrumWidget() ONCE, at startup, so the overlay
        // kept going to whichever pan was active at construction even after
        // the operator selected another. And "active pan" is the wrong pan
        // regardless: the red border, the TX palette and the TX threshold
        // path all key off m_moxOverlay, so they belong on the pan hosting
        // the TRANSMITTING slice. Both facts have to agree about which
        // widget that is, so one lambda owns both.

        // Remote-window parity Task 29 (A11, R-R3-49): the MOX-aware
        // panadapter source switch (PR #212 follow-up and the 3M-5 revive)
        // lives in MoxDisplayController, which a remote window drives too.
        // This window's own pan is a viewer of the transmit analyzer's feed
        // (TxDisplayFeed, local, so it governs the analyzer's window); the
        // rise and fall follow MoxController::moxStateChanged with the
        // TX-bound slice, queued as before so they run after the feed has
        // started the analyzer on the same edge.
        if (m_txAnalyzer && m_radioModel->txDisplayFeed()) {
            m_localTxDisplaySource = std::make_unique<LocalTxDisplaySource>(
                m_radioModel->txDisplayFeed());
            m_moxDisplay = new MoxDisplayController(m_panStack, m_radioModel, this);
            m_moxDisplay->setSource(m_localTxDisplaySource.get());
            m_moxDisplay->followLocalRadio();
        }
        // Parity Task 31: DUP from the window's setting.
        applyDisplayDuplex();

        // ── Phase 3M-4 Task 12: SpectrumWidget IMD overlay state wiring ──────
        // From Thetis display.cs:5008 [v2.10.3.13] show condition:
        //   show_imd_measurements = local_mox && _testing_imd
        //                           && _show_imd_measurements && displayduplex;
        // local_mox is already wired above. Wire the other two flags from
        // their authoritative coordinators:
        //   testing_imd            <- TwoToneController::twoToneActiveChanged
        //                              (mirrors Thetis Display.TestingIMD,
        //                              display.cs:296-302 [v2.10.3.13])
        //   show_imd_measurements  <- PureSignal::show2ToneMeasurementsChanged
        //                              (mirrors Thetis Display.ShowIMDMeasurments,
        //                              display.cs:304-311 [v2.10.3.13])
        // displayduplex is the window's DUP (parity Task 31), set on the
        // transmitting pan by MoxDisplayController; off by default as Thetis,
        // so the overlay shows during two-tone only with DUP on.
        if (activeSpectrumWidget()) {
            activeSpectrumWidget()->setShowIMDMeasurements(
                AppSettings::instance().value(
                    QStringLiteral("puresignal/showTwoToneMeasurements"), false).toBool());
            if (auto* tt = m_radioModel->twoToneController()) {
                connect(tt, &TwoToneController::twoToneActiveChanged,
                        activeSpectrumWidget(), &SpectrumWidget::setTestingIMD);
            }
            // Phase 3M-4 bench-fix: PureSignal coordinator is late-bound
            // during WDSP-init — at MainWindow build time it's typically
            // nullptr, so the connection below never gets made and the
            // IMD overlay show condition stays at m_showIMDMeasurements=
            // false even after the user checks chkShow2ToneMeasurements.
            // Subscribe to RadioModel::pureSignalCoordinatorReady so we
            // re-attempt the connect when the coordinator becomes available.
            // Mirrors the pattern in PureSignalApplet.cpp:118-123 +
            // PsaIndicatorWidget bench-fix.
            auto wireSpectrumToPs = [this](PureSignal* ps) {
                if (!ps || !activeSpectrumWidget()) { return; }
                connect(ps, &PureSignal::show2ToneMeasurementsChanged,
                        activeSpectrumWidget(),
                        &SpectrumWidget::setShowIMDMeasurements,
                        Qt::UniqueConnection);
            };
            wireSpectrumToPs(m_radioModel->pureSignal());
            connect(m_radioModel, &RadioModel::pureSignalCoordinatorReady,
                    this, [wireSpectrumToPs](PureSignal* ps) {
                        wireSpectrumToPs(ps);
                    });
        }

        // ── 3M-1c Phase L.3: VFO TX badge routing ─────────────────────────────
        //
        // MoxController::moxChanged(rx, oldMox, newMox) → VfoDisplayItem
        // setTransmitting on every VfoDisplayItem hosted by the app.  The rx
        // semantic (Thetis console.cs:29677 [v2.10.3.13]) is:
        //   rx==1  → VFO-A (TX comes off VFO-A in 3M-1; default in NereusSDR)
        //   rx==2  → VFO-B (only when RX2 enabled AND VFOBTX — neither
        //                    plumbed in NereusSDR today)
        //
        // Lookup strategy: walk every container's MeterWidget and update
        // every VfoDisplayItem found.  This is coarse but correct for 3M-1
        // (one VFO instance) — when RX2 lands (3F multi-pan), upgrade to
        // per-VfoDisplayItem item-name routing so VFO-B gets rx==2 only.
        //
        // The G.2 routing test (tst_vfo_display_item_tx_badge.cpp) demonstrates
        // the canonical lambda shape that this code mirrors at production scale.
        // TODO [3F]: split routing per-item so RX2's VFO-B instance only
        // updates on rx==2.
        connect(mox, &MoxController::moxChanged, this,
                [this](int rx, bool /*oldMox*/, bool newMox) {
            if (!m_containerManager) { return; }
            // 3M-1: only rx==1 is ever emitted (default RX2/VFOBTX both
            // false in MoxController), so the broadcast fires the same set
            // of items.  Filter on rx==1 to leave the door open for the
            // 3F upgrade without changing the connect site.
            if (rx != 1) { return; }
            for (ContainerWidget* c : m_containerManager->allContainers()) {
                if (!c) { continue; }
                auto* mw = qobject_cast<MeterWidget*>(c->content());
                if (!mw) { continue; }
                for (MeterItem* item : mw->items()) {
                    if (auto* vfo = qobject_cast<VfoDisplayItem*>(item)) {
                        vfo->setTransmitting(newMox);
                    }
                }
            }
        }, Qt::QueuedConnection);
    }

    // --- Phase 3G-9c: Clarity adaptive display tuning ---
    m_clarityController = new ClarityController(this);
    m_radioModel->setClarityController(m_clarityController);

    // Restore enabled state from AppSettings + sync the clarityActive
    // flag on SpectrumWidget so legacy AGC knows to stand down.
    {
        auto& s = AppSettings::instance();
        // Ship default 2026-04-30: Clarity ON for fresh installs. Auto-tuning
        // the noise floor is the better first-launch experience than asking
        // the user to find and toggle the setting themselves.
        bool clarityOn = s.value(QStringLiteral("ClarityEnabled"), QStringLiteral("True"))
                            .toString() == QStringLiteral("True");
        m_clarityController->setEnabled(clarityOn);
        activeSpectrumWidget()->setClarityActive(clarityOn);
    }

    // Feed FFT bins to Clarity (auto-queued: spectrum thread → main).
    // ClarityController holds one adaptive-display state, so it tracks one
    // stream rather than whichever stream last produced a frame. Parity
    // Task 18: that stream is the one feeding the pan Clarity tunes (the
    // active pan), connected per stream in ensureStreamWired(); it was stream
    // 0 whichever pan Clarity was tuning. A remote window's feed is the
    // Core's noise floor for that same pan (RemoteMediaController).

    // ── NoiseFloorTracker for Auto AGC-T ────────────────────────────────
    auto* nfTracker = new NoiseFloorTracker;
    m_radioModel->setNoiseFloorTracker(nfTracker);

    connect(primaryFftEngine(), &FFTEngine::fftReady,
            this, [nfTracker](int /*rxId*/, const QVector<float>& binsDbm) {
        static constexpr float kFrameIntervalMs = 33.0f;
        nfTracker->feed(binsDbm, kFrameIntervalMs);
    });

    // Max Bin detector: feed FFTEngine dBm bins into WdspEngine's NereusSDR-native
    // Max Bin pipeline.  See WdspEngine::setupMaxBinDetector for the algorithm
    // cite and the divergence rationale (WDSP analyzer not wired; FFTEngine
    // uses raw FFTW3 directly; NereusSDR runs the same Thetis algorithm against
    // the dBm bins emitted here).
    //
    // Algorithm from Thetis wdsp/analyzer.c:800-822 [@501e3f5].
    //
    // Primary engine only: setupMaxBinDetector is called with disp=0 (single
    // display channel), so the detector reads stream 0's bins.
    if (auto* eng = (m_radioModel ? m_radioModel->wdspEngine() : nullptr)) {
        connect(primaryFftEngine(), &FFTEngine::fftReady,
                eng,               &WdspEngine::onSpectrumBinsForMaxBin);
    }

    // 2026-05-22 bench fix: MaxBin meter accuracy. The raw FFT bin path
    // above reads ~12-17 dB below what the spectrum visually displays
    // because the spectrum runs the bins through a detector + invEnb
    // window-normalization + avenger time-smoothing pipeline that
    // reconstructs window-spread integrated power. After every spectrum
    // render frame, push the slice's passband peak (from m_renderedPixels,
    // post detector + avenger) into the MaxBin detector so the analog
    // S-meter reads what the operator actually sees on the trace.
    if (activeSpectrumWidget() && m_radioModel) {
        connect(activeSpectrumWidget(), &SpectrumWidget::spectrumFrameRendered,
                this, [this]() {
            auto* eng = m_radioModel ? m_radioModel->wdspEngine() : nullptr;
            if (!eng || !activeSpectrumWidget()) { return; }
            const double dbm = activeSpectrumWidget()->peakDbmInSlicePassband();
            if (dbm > -400.0) {
                eng->setMaxBinDbmFromSpectrum(/*disp=*/0, dbm);
            }
        });
    }

    // Fast-attack triggers — deferred until slice exists
    // From Thetis v2.10.3.13 display.cs:905 — freq change triggers fast attack
    // From Thetis v2.10.3.13 display.cs:880 — mode change triggers fast attack
    // Connected in wireSliceToSpectrum() where activeSlice() is guaranteed non-null
    // From Thetis v2.10.3.13 display.cs:890 — OnAttenuatorDataChanged
    if (m_stepAttController) {
        connect(m_stepAttController, &StepAttenuatorController::attenuationChanged,
                this, [nfTracker](int /*dB*/) {
            nfTracker->triggerFastAttack();
        });
    }

    // Periodic visual update: auto-AGC timer → refresh NF visuals on both widgets
    if (m_radioModel->autoAgcTimer()) {
        connect(m_radioModel->autoAgcTimer(), &QTimer::timeout, this, [this]() {
            for (SliceModel* slice : m_radioModel->slices()) {
                refreshAutoAgcVisuals(m_radioModel, slice,
                    m_vfoWidgetsBySlice.value(slice->sliceIndex()), m_rxApplet);
            }
        });
    }

    // TX pause: MOX signal → ClarityController
    connect(&m_radioModel->transmitModel(), &TransmitModel::moxChanged,
            m_clarityController, &ClarityController::setTransmitting);

    // Plan 4 D9 (Cluster E): TX filter audio range → spectrum overlay.
    // TransmitModel::filterChanged carries (low, high) audio Hz; SpectrumWidget
    // converts to IQ-space at draw time using m_txMode (set below via slice).
    if (activeSpectrumWidget()) {
        connect(&m_radioModel->transmitModel(), &TransmitModel::filterChanged,
                activeSpectrumWidget(), &SpectrumWidget::setTxFilterRange);

        // Initial sync from current TransmitModel state.
        const auto& txModel = m_radioModel->transmitModel();
        activeSpectrumWidget()->setTxFilterRange(txModel.filterLow(), txModel.filterHigh());
    }

    // Clarity → SpectrumWidget threshold update + clarityActive flag.
    // Issue #230 fix: write the render-active mirror, not the
    // persistent user fields — Clarity is runtime state per Thetis's
    // AGC pattern (display.cs:6584 [v2.10.3.13] uses
    // _RX1waterfallPreviousMinValue, a runtime field separate from
    // waterfall_low_threshold).  The previous setWfLow/HighThreshold
    // calls were silently overwriting the user's saved thresholds via
    // scheduleSettingsSave() on every Clarity tick.
    connect(m_clarityController, &ClarityController::waterfallThresholdsChanged,
            this, [this](float low, float high) {
        // PR #212 follow-up bench fix (KG4VCF, 2026-05-10): suppress
        // Clarity threshold updates while MOX is active.  Clarity tracks
        // RX noise floor and would otherwise re-enable itself with
        // RX-tuned thresholds during TX, defeating the
        // setClarityActive(false) call in the MOX-rise lambda.
        //
        // Kept through the 3M-5 revive merge, re-applied on top of the
        // issue-#230 shape: the threshold write now goes to the runtime
        // mirror via setClarityWaterfallThresholds rather than to the
        // persisted setWfLow/HighThreshold pair. The MOX gate is still
        // needed and is independent of that change -- #230 stopped Clarity
        // clobbering SAVED thresholds, this stops it running at all during
        // transmit.
        MoxController* mox = m_radioModel ? m_radioModel->moxController()
                                          : nullptr;
        if (mox && mox->isMox()) {
            return;
        }
        // Parity Task 29: a remote window's Core keyed (no local MOX).
        if (m_moxDisplay && m_moxDisplay->isKeyed()) {
            return;
        }
        if (SpectrumWidget* sw = activeSpectrumWidget()) {
            sw->setClarityActive(true);
            sw->setClarityWaterfallThresholds(low, high);
        }
    });

    // Clarity → SpectrumWidget NF-aware grid (Task 2.9).
    // NereusSDR-original — no Thetis equivalent.
    // noiseFloorChanged fires after EWMA smoothing but before the deadband
    // gate so the grid tracks the floor at every cadence tick.
    // The window owns both output routes; retiring the initial pane must
    // neither disconnect them nor leave its grid receiving another pane's floor.
    connect(m_clarityController, &ClarityController::noiseFloorChanged,
            this, [this](float nf) {
        if (SpectrumWidget* sw = activeSpectrumWidget()) {
            sw->onNoiseFloorChanged(nf);
        }
    });

    // Task 2.10: per-band NF priming — settle detector.
    // NereusSDR-original — no Thetis equivalent.
    //
    // On each noiseFloorChanged tick, keep a 2-second sliding window of NF
    // samples. When variance drops below 1 dB for a sustained window of ≥30
    // samples (≈ 15 s / cadence-0.5s = 30 ticks), save the current floor to
    // the panadapter's per-band NF slot so the next band-switch can snap
    // instantly instead of cold-starting from zero.
    {
        struct NFHistoryEntry { qint64 t; float value; };
        struct SettleState {
            QList<NFHistoryEntry> history;
        };
        auto settle = QSharedPointer<SettleState>::create();

        PanadapterModel* pan0 = m_radioModel->panadapters().isEmpty()
                                ? nullptr
                                : m_radioModel->panadapters().first();
        if (pan0) {
            connect(m_clarityController, &ClarityController::noiseFloorChanged,
                    this, [pan0, settle](float nf) {
                const qint64 now = QDateTime::currentMSecsSinceEpoch();
                settle->history.append({now, nf});

                // Trim to 2-second window.
                const qint64 cutoff = now - 2000;
                while (!settle->history.isEmpty() && settle->history.first().t < cutoff) {
                    settle->history.removeFirst();
                }

                // Compute variance when we have ≥30 samples (~30 cadence ticks).
                if (settle->history.size() >= 30) {
                    float sum = 0.0f;
                    for (const auto& e : std::as_const(settle->history)) { sum += e.value; }
                    const float mean = sum / static_cast<float>(settle->history.size());
                    float sqSum = 0.0f;
                    for (const auto& e : std::as_const(settle->history)) {
                        const float d = e.value - mean;
                        sqSum += d * d;
                    }
                    const float variance = sqSum / static_cast<float>(settle->history.size());

                    if (variance < 1.0f) {
                        // NereusSDR-original — no Thetis equivalent.
                        // NF settled within 1 dB variance over 2s; save for this band.
                        pan0->setBandNFEstimate(pan0->band(), nf);
                    }
                }
            });

            // Task 2.10: band-change → prime ClarityController EWMA with stored NF.
            // NereusSDR-original — no Thetis equivalent.
            //
            // PanadapterModel::bandChanged fires when the pan center crosses a band
            // boundary. snapToFloor() seeds the EWMA (m_smoothedFloor) and emits
            // waterfallThresholdsChanged immediately so the waterfall snaps to the
            // remembered state rather than cold-starting from an uninitialized floor.
            // NaN is ignored by snapToFloor (band with no stored data is a no-op).
            connect(pan0, &PanadapterModel::bandChanged,
                    this, [this, pan0](NereusSDR::Band newBand) {
                // NereusSDR-original — no Thetis equivalent.
                // Prime estimator with last-seen NF for this band to eliminate
                // cold-start visual jump after band change.
                const float storedNF = pan0->bandNFEstimate(newBand);
                m_clarityController->snapToFloor(storedNF);
            });

            // NF fast-attack triggers — From Thetis display.cs:879-905
            // [v2.10.3.13]:
            //   if (rx == 1) FastAttackNoiseFloorRX1 = true;  // band change
            //   if (Math.Abs(oldFreq - newFreq) > 0.5)         // freq jump
            //       FastAttackNoiseFloorRX1 = true;
            // While in fast-attack state SpectrumWidget renders the NF
            // line/box/text in gray to signal the smoothed estimate is
            // still settling.  Auto-clear is internal to the setter (see
            // SpectrumWidget::setNoiseFloorFastAttack — 1000ms timer
            // matching Thetis display.cs:5906 minimum delay).
            if (activeSpectrumWidget()) {
                connect(pan0, &PanadapterModel::bandChanged,
                        this, [this](NereusSDR::Band) {
                    activeSpectrumWidget()->setNoiseFloorFastAttack(true);
                });
            }

            // 3D Stacked-Trace Spectrum Plan Task 14 fix-forward: 3D Floor
            // recall, same bandChanged block as the ClarityController
            // priming and NF fast-attack connections directly above, so 3D
            // Floor arrives at the same time as the rest of the per-band
            // state a band change already recalls. Routed through the
            // static seam below (same shape as wireWidebandExtensionForTest
            // etc.) so the unit test exercises the exact connect() call
            // this constructor makes, not a parallel test-only copy of it.
            wireDss3DFloorRecallForTest(pan0, activeSpectrumWidget());
        }
    }

    // Slice freq-jump > 0.5 MHz fast-attack trigger — Thetis display.cs:905
    // [v2.10.3.13]: if (Math.Abs(oldFreq - newFreq) > 0.5) FastAttack = true.
    // Smaller jumps (in-band tuning) don't shift the noise floor enough to
    // warrant resetting the smoothed estimate.
    //
    // Stores the last-trigger frequency as a QObject dynamic property on
    // the slice itself — Qt cleans it up when the slice is destroyed, and
    // the same wiring works for slices added later via RadioModel::sliceAdded.
    if (activeSpectrumWidget()) {
        // 500 kHz threshold matches Thetis display.cs:905: > 0.5 MHz.
        constexpr double kFastAttackFreqJumpHz = 500000.0;
        constexpr const char* kLastFreqProp = "nfLastFastAttackFreq";

        auto subscribeSlice = [this](SliceModel* slice) {
            if (!slice) { return; }
            slice->setProperty(kLastFreqProp, slice->frequency());
            connect(slice, &SliceModel::frequencyChanged, this,
                    [this, slice](double freq) {
                const double last =
                    slice->property(kLastFreqProp).toDouble();
                if (std::abs(last - freq) > kFastAttackFreqJumpHz) {
                    activeSpectrumWidget()->setNoiseFloorFastAttack(true);
                }
                slice->setProperty(kLastFreqProp, freq);
            });
        };
        for (SliceModel* slice : m_radioModel->slices()) {
            subscribeSlice(slice);
        }
        connect(m_radioModel, &RadioModel::sliceAdded, this,
                [this, subscribeSlice](int index) {
            subscribeSlice(sliceForAddedIdForTest(m_radioModel, index));
        });
    }

    connect(m_radioModel, &RadioModel::settingsSaveErrorChanged, this,
            [this](const QString& reason) {
        if (!reason.isEmpty()) {
            // A remote window's save error is the Core's text; shown in
            // user words, logged raw (R-R3-21). Local text is plain and
            // passes through unchanged.
            showToast(OperatorReasonText::forDisplay(reason), ToastSeverity::Error, 10000);
        }
    });
    // Phase 3F Sub-Epic C Task 8: toast on slice-add rejection.
    // RadioModel::addSliceOnPan() and addSlice() emit sliceAddRejected(reason)
    // when the slice limit blocks a +RX click (e.g. "Hermes Lite 2 supports
    // a maximum of 1 slice"). Surface that for 4 seconds so the operator
    // sees why the click did nothing.
    connect(m_radioModel, &RadioModel::sliceAddRejected, this,
            [this](const QString& reason) {
        // A hosting add's refusal is HostingSliceActions::refused (Task 10).
        if (m_hostingSlices && m_hostingSlices->invoking()) { return; }
        // A remote window's refusal is the Core's text; shown in user words.
        showToast(OperatorReasonText::forDisplay(reason), ToastSeverity::Warning, 4000);
    });
    // R-R3-34: a local window (no Core) closes slices a smaller board cannot
    // host when it connects (RadioModel::closeSlicesPastChannelLimit) and
    // says so on the receive-layout restore status. A remote window toasts
    // that status from its station wiring; this is the same notice through
    // the same toast for a window with no Core, so a closure is never silent.
    if (!m_station.isRemote()) {
        connect(m_radioModel, &RadioModel::receiveLayoutRestoreStatusChanged, this,
                [this] {
            const QString toast = m_receiveLayoutNotices.toastFor(
                m_radioModel->receiveLayoutRestoreState(),
                m_radioModel->receiveLayoutRestoreMessage(), /*viaCore*/ false);
            if (!toast.isEmpty()) {
                showToast(toast, ToastSeverity::Warning, 10000);
            }
        }, Qt::QueuedConnection);
        // Each connect is told its own closures, even one worded exactly as
        // the last connect's.
        connect(m_radioModel, &RadioModel::connectionStateChanged, this,
                [this](ConnectionState state) {
            if (state == ConnectionState::Disconnected) {
                m_receiveLayoutNotices.forget();
            }
        });
    }
    // L1 (R-R3-47, R-R3-22, R-R3-48): the Core refused an accessory request
    // (amp, tuner, RF-Kit, interlock, fault history, station TCI). Its own
    // route, so a slice-only listener never hears it; the same toast.
    // Follow-up 3: a refusal the page that sent it shows is not toasted too.
    connect(m_radioModel, &RadioModel::accessoryRequestRefused, this,
            [this](const QString&, const QString& reason, bool shownOnPage) {
        if (!shownOnPage) {
            showToast(OperatorReasonText::forDisplay(reason), ToastSeverity::Warning, 4000);
        }
    });

    // Phase 3F Sub-Epic I closeout, defect F4.
    //
    // The operator turned the knob to somewhere no DDC can reach. The VFO has
    // already snapped back to the last frequency that bound, so the message
    // has to explain the snap rather than talk about adding a slice, which is
    // what the rejection used to say. 6 s: it names a frequency the operator
    // needs time to read.
    connect(m_radioModel, &RadioModel::sliceRetuneRejected, this,
            [this](int, const QString& reason) {
        showToast(OperatorReasonText::forDisplay(reason), ToastSeverity::Warning, 6000);
    });

    // Phase 3F Sub-Epic I closeout, defect F3.
    //
    // The 1-ADC HERMES class drops every extra receiver the moment PureSignal
    // transmits or diversity engages. That is what Thetis does and it stays,
    // but Thetis says nothing about it either, so on the bench a slice simply
    // stopped producing audio with no explanation. Same surface as the
    // rejection message above; 6 s because it names slice letters the
    // operator has to map back to their flags.
    //
    // This is the notice that produced the 2026-07-30 bench report: it fires
    // on MOX with PureSignal running, which is exactly when the operator most
    // needs the PureSignal indicator and the TX badge it used to cover. It is
    // a toast for that reason and must stay one.
    connect(m_radioModel, &RadioModel::streamsSuspended, this,
            [this](const QVector<int>& streams, const QString& reason) {
        if (streams.isEmpty()) {
            // Everything is back. Take the notice down rather than leaving a
            // stale one on screen for its full timeout: on unkey the streams
            // return in well under six seconds.
            if (m_suspendToast) { m_suspendToast->close(); }
            return;
        }
        m_suspendToast = showToast(reason, ToastSeverity::Warning, 6000);
    });

    // Phase 3F closeout — Sub-Epic E Task 6 consumer wire-up.
    // antennaAutoSwitched(sliceIdx, oldAnt, newAnt) is emitted when an
    // AlexController conflict-policy re-route moves a slice off its old
    // antenna onto a new one. MainWindow constructs an AntennaSwitchToast
    // anchored to the MainWindow bottom-right corner, 8 s auto-dismiss,
    // UNDO button logs (real undo wires when the conflict-detection state
    // machine lands).
    connect(m_radioModel, &RadioModel::antennaAutoSwitched, this,
            [this](int sliceIdx, const QString& oldAnt, const QString& newAnt) {
        const QString msg = QStringLiteral("Slice %1 moved from %2 to %3.")
                                .arg(QChar(QLatin1Char('A' + sliceIdx)),
                                     oldAnt, newAnt);
        auto* toast = new AntennaSwitchToast(msg, this);
        toast->setAttribute(Qt::WA_DeleteOnClose);
        const QRect mwGeom = frameGeometry();
        toast->move(mwGeom.right() - toast->width() - 20,
                     mwGeom.bottom() - toast->height() - 50);
        toast->show();
        connect(toast, &AntennaSwitchToast::undoRequested, this,
                [sliceIdx, oldAnt]() {
            qCInfo(lcContainer) << "AntennaSwitchToast: undo requested for slice"
                                 << sliceIdx << "(would revert to" << oldAnt
                                 << ")  -  real undo wires when conflict-detection lands";
        });
    });

    // Phase 3F closeout — Sub-Epic E Task 7 consumer wire-up.
    // txBoundReRouteRequested(proposedAntenna, existingAntenna) opens a
    // modal TxBoundConfirmDialog with three outcomes (Cancelled /
    // UseExistingAntenna / ConfirmReroute). Today we log the outcome;
    // outcome routing to AlexController lands when the conflict-detection
    // state machine in addSliceOnPan ships in a follow-up.
    connect(m_radioModel, &RadioModel::txBoundReRouteRequested, this,
            [this](const QString& proposed, const QString& existing) {
        if (!m_radioModel) { return; }
        TxBoundConfirmDialog dlg(proposed, existing,
                                  m_radioModel->slices(), this);
        dlg.exec();
        qCInfo(lcContainer) << "TxBoundConfirmDialog: outcome="
                             << int(dlg.outcome())
                             << "(0=Cancelled, 1=UseExistingAntenna, 2=ConfirmReroute)";
    });

    // Phase 3F Sub-Epic C Task 10: TxSliceArbiter state → UI updates.
    // When the TX-bound slice flips (via VfoWidget badge click in T9, or
    // any future programmatic path), update the matching VfoWidget badge
    // and post a 2-second "TX > Slice X" status toast.
    //
    // Sub-Epic D expands this single-VfoWidget update to iterate the full
    // per-pan flag collection (one VfoWidget per slice on multi-pan
    // layouts).  Phase 3F hotfix 2026-05-27 made that fanout live: every
    // VfoWidget tracked in m_vfoWidgetsBySlice (Slice A + any Slice B+
    // flags auto-created on sliceAdded) now gets its TX badge updated
    // to reflect the arbiter's current bound slice.
    if (TxSliceArbiter* arb = m_radioModel->txSliceArbiter()) {
        connect(arb, &TxSliceArbiter::txBoundSliceChanged, this,
                [this](int oldId, int newId) {
            // Slice control plan Task 14a: while hosting, the badge follows
            // refreshDesktopFlags' rule (a listened slice on the air is red
            // too); this used to set its own rule over it.
            if (desktopHosting()) {
                refreshDesktopFlags();
            } else {
                for (auto it = m_vfoWidgetsBySlice.constBegin();
                     it != m_vfoWidgetsBySlice.constEnd(); ++it) {
                    if (VfoWidget* flag = it.value()) {
                        flag->setTxSlice(flag->sliceIndex() == newId);
                    }
                }
            }
            // oldId < 0 is the arbiter's initial bind (TxSliceArbiter::
            // syncToSliceList), not an operator handoff: the transmitter
            // did not move, it acquired its first home when the first slice
            // appeared. Announcing "TX > Slice A" on every connect would be
            // noise about something that did not happen.
            if (oldId < 0) { return; }
            // Slice control plan Task 7: the last slice closed; there is
            // no transmit slice to name.
            if (newId < 0) { return; }
            showToast(QStringLiteral("TX > Slice %1")
                          .arg(QChar(QLatin1Char('A' + newId))),
                      ToastSeverity::Info, 2000);
        });
        // A handoff waiting for the unkey gate counts its slice as on the
        // air (StationServer::sliceTransmitting).
        connect(arb, &TxSliceArbiter::pendingHandoffChanged, this,
                [this](int) { if (desktopHosting()) { refreshDesktopFlags(); } });
    }
    // The TX slice stays on the air until MOX reads off after its key ends.
    if (MoxController* mox = m_radioModel->moxController()) {
        connect(mox, &MoxController::moxStateChanged, this,
                [this](bool) { if (desktopHosting()) { refreshDesktopFlags(); } });
    }

    // Same fan-out for the selected slice, which colours the markers on the
    // pans: the selected slice draws in its full colour, every other slice
    // darker (SpectrumWidget::sliceMarkerGeometry). Every flag on every pan
    // is told, not only the ones on the pan hosting the selection, because a
    // pan that does not host it draws all of its slices darker. The flag
    // invalidates its own pan's cached overlay when its state flips.
    //
    // activeSliceChanged carries a list position, so the selection is
    // resolved through activeSlice()->sliceIndex(), as the handler that
    // fronts the selected flag does.
    connect(m_radioModel, &RadioModel::activeSliceChanged, this, [this](int) {
        const SliceModel* active = m_radioModel->activeSlice();
        const int activeId = active ? active->sliceIndex() : -1;
        for (auto it = m_vfoWidgetsBySlice.constBegin();
             it != m_vfoWidgetsBySlice.constEnd(); ++it) {
            if (VfoWidget* flag = it.value()) {
                flag->setActiveSlice(flag->sliceIndex() == activeId);
            }
        }
    });

    // MOX transition fast-attack trigger — Thetis display.cs:889-892:
    //   if (rx == 1) FastAttackNoiseFloorRX1 = true;
    // Fires on both RX→TX and TX→RX transitions; the buffer-clear pulse on
    // either edge resets the noise-floor settling window.
    if (activeSpectrumWidget()) {
        connect(&m_radioModel->transmitModel(), &TransmitModel::moxChanged,
                this, [this](bool) {
            activeSpectrumWidget()->setNoiseFloorFastAttack(true);
        });
    }

    // When Clarity pauses or is disabled, let legacy AGC resume.
    connect(m_clarityController, &ClarityController::pausedChanged,
            activeSpectrumWidget(), [this](bool paused) {
        if (paused) {
            activeSpectrumWidget()->setClarityActive(false);
        }
    });

    // Clarity ↔ each pan's strip. Parity Task 18: every strip shows the
    // badge for the pan Clarity tunes (the active pan) and nothing on the
    // others, and each strip's Display flyout and Re-tune act on its own pan
    // (wirePanDisplayFlyout, from ensureOverlayPanels). Before, only pan-0's
    // strip was wired, and its display controls reached whichever pan was
    // active.
    connect(m_clarityController, &ClarityController::waterfallThresholdsChanged,
            this, [this](float, float) {
        m_clarityBadgeActive = true;
        m_clarityBadgePaused = false;
        refreshClarityBadges();
    });
    connect(m_clarityController, &ClarityController::pausedChanged,
            this, [this](bool paused) {
        m_clarityBadgeActive = m_clarityController->isEnabled();
        m_clarityBadgePaused = paused;
        refreshClarityBadges();
    });
    // Clarity follows the active pan: the pan it leaves goes back to its own
    // waterfall levels, and the pan it arrives at is estimated afresh
    // rather than given the last pan's floor.
    m_clarityPanId = m_panStack ? m_panStack->activePanId() : QString();
    connect(m_panStack, &PanadapterStack::activePanChanged, this,
            [this](const QString& panId) {
        if (panId == m_clarityPanId) { return; }
        if (SpectrumWidget* left = m_panStack->spectrum(m_clarityPanId)) {
            left->setClarityActive(false);
        }
        m_clarityPanId = panId;
        if (m_clarityController->isEnabled()) {
            if (SpectrumWidget* arrived = m_panStack->spectrum(panId)) {
                arrived->setClarityActive(!m_clarityController->isPaused());
            }
            m_clarityController->retuneNow();
        }
        refreshClarityBadges();
    });

    // Wire: zoom changes -> auto-replan FFT size to maintain constant
    // bins-per-pixel across zoom levels.  NereusSDR-original (Thetis
    // does not auto-replan on zoom; the user manually picks FFT size).
    //
    // Math: the slider's "FFT size at full DDC bandwidth" baseline
    // implies a target K = baseline / displayWidth bins per pixel.
    // To maintain K as bwHz narrows (zoom in), the FFT size must scale
    // inversely with bwHz:
    //   targetSize = baseline * sampleRate / bwHz
    //
    // Cap at kAutoZoomMaxFftSize = 65536.  Set well below kMaxFftSize
    // (262144) to bound the buffer-fill pause on every replan: at
    // 768 kHz DDC, 65536 fills in 85 ms (barely perceptible).  262144
    // would take 340 ms (jarring) and create a multi-frame avenger
    // ghost in the waterfall as the smoothed state crosses fftSize
    // resolutions.  Users who want larger FFTs explicitly opt in via
    // the slider (one-time pause they chose); auto-zoom won't push
    // above the cap automatically.
    //
    // Floor at the slider baseline (we never replan BELOW the user's
    // chosen value).
    //
    // Hysteresis: only replan when computed/current is outside
    // [0.66, 1.5].  Avoids replan thrash on smooth zoom drag.
    //
    // Phase 3F Sub-Epic I Task 8: the primary engine, because this lambda is
    // wired to pan 0's SpectrumWidget (the signal is per-pan). Per-pan
    // auto-zoom on secondary pans is Phase 3F follow-up work.
    constexpr int kAutoZoomMaxFftSize = 65536;
    connect(activeSpectrumWidget(), &SpectrumWidget::bandwidthChangeRequested,
            this, [this, kAutoZoomMaxFftSize](double bwHz) {
        FFTEngine* engine = primaryFftEngine();
        if (!engine || !activeSpectrumWidget()) { return; }
        const double sampleRate = activeSpectrumWidget()->sampleRate();
        if (sampleRate <= 0.0 || bwHz <= 0.0) { return; }

        const int baseline = engine->fftSizeBaseline();

        // Hz/bin override (Option 3 from the 2026-05-08 design).  When the
        // user has set a non-zero target Hz/bin in
        // Setup → Display → Spectrum Defaults, the auto-zoom formula
        // becomes zoom-INDEPENDENT:
        //   targetSize = sampleRate / hzPerBinTarget
        // The FFT delivers the requested resolution at any zoom — useful
        // for hunting narrow features (CW, digital).  Floor at baseline
        // still applies, so the FFT slider remains a minimum-FFT-size
        // knob.  When hzPerBinTarget == 0 we use the original
        // bins-in-window default (constant K = baseline).
        const double hzPerBinTarget = engine->hzPerBinTarget();
        double desired;
        if (hzPerBinTarget > 0.0) {
            desired = sampleRate / hzPerBinTarget;
        } else {
            const double scale = sampleRate / bwHz;        // 1.0 at full bw
            desired = static_cast<double>(baseline) * scale;
        }

        // Round up to next power of 2.
        int targetSize = 1024;
        while (targetSize < desired && targetSize < kAutoZoomMaxFftSize) {
            targetSize *= 2;
        }
        // Floor at baseline (slider's choice always honoured), then cap at
        // auto-zoom max.  When baseline > cap (user explicitly picked a
        // larger size via the slider), baseline wins and auto-zoom is a
        // no-op for that range.
        targetSize = (std::max)(targetSize, baseline);
        targetSize = (std::min)(targetSize, (std::max)(baseline, kAutoZoomMaxFftSize));

        // Hysteresis: only replan if outside [current * 2/3, current * 3/2].
        const int currentSize = engine->fftSize();
        if (currentSize > 0) {
            const double ratio = static_cast<double>(targetSize)
                                 / static_cast<double>(currentSize);
            if (ratio > 0.66 && ratio < 1.5) {
                return;  // small enough change to ignore
            }
        }

        engine->setFftSize(targetSize);
    });

    // R1 Task 6: no explicit thread start here anymore -- FftEnginePool
    // starts each worker thread itself the moment it is first needed
    // (inside createEngine(), when a bucket's thread does not exist yet),
    // which already happened above when ensureStreamWired(0) built stream
    // 0's engine.

    // --- Meter Poller (Phase 3G-2) ---
    // Poller was created earlier so the meterReadyForPolling signal
    // could catch container restore + populateDefaultMeter emits.
    // m_meterWidget is registered automatically via that signal —
    // the call here is a defensive belt only and dedupes inside
    // MeterPoller::addTarget.
    if (m_meterWidget) {
        m_meterPoller->addTarget(m_meterWidget);
    }

    // Wire RxChannel to poller when WDSP finishes initializing.
    // RadioModel's initializedChanged handler creates the RxChannel, but
    // it was registered AFTER this connection (RadioModel registers during
    // onConnectionStateChanged, not buildUI). Qt fires in registration order,
    // so we defer by one event loop pass to ensure RxChannel exists.
    connect(m_radioModel->wdspEngine(), &WdspEngine::initializedChanged,
            this, [this](bool ok) {
        if (!ok) {
            // R-R3-21: the filter displays hold channel 0's RxChannel for the
            // high-resolution curve; WdspEngine has already destroyed it, so
            // rebind them to none (rxChannelForSlice(0) is null now).
            DspOptionsPage::applyPersistedHighResFilter(m_radioModel);
            return;
        }
        QTimer::singleShot(0, this, [this]() {
            // R-R3-21: bind the new channel 0 to the filter displays with
            // the saved high-resolution setting, as opening DSP > Options
            // used to be the only way to do.
            DspOptionsPage::applyPersistedHighResFilter(m_radioModel);
            // Phase 3F Sub-Epic J Task 11: RadioModel::rxChannelForSlice()
            // replaces the direct wdspEngine()->rxChannel() reach. Still
            // channel 0 here on purpose -- this is the boot-time seed, before
            // any slice but A exists, and the activeSliceChanged handler
            // below immediately supersedes it once other slices are added.
            RxChannel* rxCh = m_radioModel->rxChannelForSlice(0);
            if (rxCh) {
                m_meterPoller->setRxChannel(rxCh);
                m_meterPoller->start();
                qCDebug(lcMeter) << "MeterPoller started on RxChannel 0";
            } else {
                qCWarning(lcMeter) << "MeterPoller: RxChannel 0 still null after WDSP init";
            }

            // H.2 (Phase 3M-1a): wire TxChannel to MeterPoller for TX meters.
            // TxChannel is valid after WDSP initialization via createTxChannel().
            // Guard: txChannel() is null before initialization; null guard in
            // pollTxMeters() handles the case where it isn't set yet.
            if (TxChannel* txCh = m_radioModel->txChannel()) {
                m_meterPoller->setTxChannel(txCh);
                qCDebug(lcMeter) << "MeterPoller: TxChannel wired for TX meters";
            }
        });
    });

    // Phase 3F Sub-Epic J Task 4: the container S-meter is attached to no
    // flag, so it must show the active slice, not always slice A. The
    // WDSP-init binding above is only the seed for the first slice --
    // MeterPoller::pollSMeter() (which drives the analog SMeterWidget
    // installed as AppletPanelWidget's fixed header, i.e. the widget this
    // wiring targets) reads m_rxChannel exclusively and that pointer never
    // moved after the seed, so the container meter showed RxChannel 0
    // whatever the operator was working. The per-flag mini S-meters do not
    // have this bug: they already resolve their own channel per slice, now
    // via SliceMeterPump (src/core/meters/, Remote Daemon R2 Task 12;
    // formerly MeterPoller::pollSliceSMeters()) reading wdspEngine()->
    // rxChannel(slice->sliceIndex()) directly on RadioModel, so they are
    // untouched here.
    connect(m_radioModel, &RadioModel::activeSliceChanged, this,
            [this](int) { refreshActiveSlicePresentation(); });

    // NereusSDR (R-R3-13): a local LinkLost keeps the RX channels alive, so
    // the poller cannot tell from the channel that no reading exists. Only
    // a Connected link carries receive readings; every other state shows
    // "--" on the container meters and the analog S-meter header.
    connect(m_radioModel, &RadioModel::connectionStateChanged, m_meterPoller,
            [poller = m_meterPoller](ConnectionState state) {
        poller->setLocalRxReadingAvailable(state == ConnectionState::Connected);
    });
    m_meterPoller->setLocalRxReadingAvailable(
        m_radioModel->connectionState() == ConnectionState::Connected);

    // H.2 (Phase 3M-1a): wire MoxController::moxStateChanged → MeterPoller::setInTx.
    // Switches the poll set between RX meters (TX off) and TX meters (TX on).
    // From Thetis dsp.cs:995-1050 [v2.10.3.13] CalculateTXMeter dispatch.
    // Qt::QueuedConnection: ensures the flip happens at the start of the next
    // event loop tick rather than mid-poll, matching Thetis's timer dispatch.
    if (MoxController* mox = m_radioModel->moxController()) {
        connect(mox, &MoxController::moxStateChanged,
                m_meterPoller, &MeterPoller::setInTx,
                Qt::QueuedConnection);

        // ── Phase 3M-1b K.2: MOX rejection → toast ──────────────────────────
        // moxRejected fires when BandPlanGuard::checkMoxAllowed() rejects a
        // setMox(true) request (wrong mode, out-of-band freq, cross-band TX).
        // Presented for 3 seconds, matching the bandClickIgnored pattern.
        // The toast is transient: it clears automatically and does not affect
        // bottom-bar layout or persistence.
        connect(mox, &MoxController::moxRejected,
                this, [this](const QString& reason) {
            showToast(reason, ToastSeverity::Warning, 3000);
        });
    }
    // Desktop remote transmit (R-IOS-13): the Core refused a remote
    // window's MOX, TUNE or two-tone press; shown as a local refusal is.
    connect(m_radioModel, &RadioModel::remoteTransmitRefused,
            this, [this](const QString& reason) {
        showToast(reason, ToastSeverity::Warning, 3000);
    });

    // ── Phase 3M-0 Task 17: safety controller → status-bar wiring ────────────
    //
    // Wire PA Fwd/Rev/SWR telemetry into MeterPoller's cache so PA power
    // meter items (MeterPoller::setRadioStatus) receive live data as
    // RadioStatus::powerChanged fires. Called here (after poller creation)
    // so the connection outlives the poller's lifetime.
    m_meterPoller->setRadioStatus(&m_radioModel->radioStatus());

    // Ganymede PA-trip badge: RadioModel::paTrippedChanged → setPaTripped.
    // setPaTripped() was added in Task 14 and updates m_paStatusBadge text
    // and colour atomically.
    connect(m_radioModel, &RadioModel::paTrippedChanged,
            this, &MainWindow::setPaTripped);

    // TX Inhibit pill: TxInhibitMonitor::txInhibitedChanged → setTxInhibited.
    // setTxInhibited() was added in Task 14 and toggles m_txInhibitLabel
    // visibility. The Source parameter is ignored by the UI slot (the pill
    // is binary: visible or hidden).
    //
    // R-R3-49 (parity Task 6): through RadioModel::txInhibitedChanged, which
    // follows this window's own monitor locally and the Core's mirrored
    // `txInhibited` in a remote window, so both show the radio's inhibit.
    connect(m_radioModel, &RadioModel::txInhibitedChanged,
            this, &MainWindow::setTxInhibited);
    // HL2 port part 2: a new reason while inhibited (a new fault code).
    connect(m_radioModel, &RadioModel::txInhibitReasonChanged,
            this, [this](const QString&) { showTxInhibitReason(); });
    setTxInhibited(m_radioModel->isTxInhibited());
}

void MainWindow::rebuildEditContainerSubmenu()
{
    if (!m_editContainerMenu) { return; }
    m_editContainerMenu->clear();

    if (!m_containerManager) {
        auto* none = m_editContainerMenu->addAction(
            QStringLiteral("(no containers)"));
        none->setEnabled(false);
        return;
    }

    const QList<ContainerWidget*> all = m_containerManager->allContainers();
    if (all.isEmpty()) {
        auto* none = m_editContainerMenu->addAction(
            QStringLiteral("(no containers)"));
        none->setEnabled(false);
        return;
    }

    // Alphabetical by title so the menu order is predictable even
    // when containers are created in different orders.
    QList<ContainerWidget*> sorted = all;
    std::sort(sorted.begin(), sorted.end(),
              [](ContainerWidget* a, ContainerWidget* b) {
        const QString na = a->notes().isEmpty()
            ? a->id().left(8) : a->notes();
        const QString nb = b->notes().isEmpty()
            ? b->id().left(8) : b->notes();
        return na.localeAwareCompare(nb) < 0;
    });

    for (ContainerWidget* c : sorted) {
        const QString label = c->notes().isEmpty()
            ? (QStringLiteral("(unnamed) ") + c->id().left(8))
            : c->notes();
        QAction* act = m_editContainerMenu->addAction(label);
        const QString id = c->id();
        connect(act, &QAction::triggered, this, [this, id]() {
            if (!m_containerManager) { return; }
            ContainerWidget* target = m_containerManager->container(id);
            if (!target) { return; }
            ContainerSettingsDialog dialog(target, this, m_containerManager);
            dialog.exec();
        });
    }
}

void MainWindow::resetDefaultLayout()
{
    if (!m_containerManager) { return; }
    if (auto* store=m_containerManager->workspaceStore()) {
        WorkspaceDocument document=store->snapshot();
        ContainerDocument main; main.id=document.mainContainerId; main.name=tr("Main Panel"); main.layout=ContentLayout::VerticalStack;
        for (const auto& c : document.containers) { for (const auto& entry : c.contents) {
            if (entry.typeId.startsWith("applet:")) { auto retained=entry; retained.returnLocation.reset(); main.contents.append(retained); }
        } }
        document.containers={main};
        m_containerManager->commitWorkspace(document,document.revision); return;
    }

    // Destroy every non-panel container. Collect IDs first because
    // destroyContainer mutates the underlying map.
    const QList<ContainerWidget*> all = m_containerManager->allContainers();
    ContainerWidget* panel = m_containerManager->panelContainer();
    QStringList toRemove;
    for (ContainerWidget* c : all) {
        if (c == panel) { continue; }
        toRemove.append(c->id());
    }
    for (const QString& id : toRemove) {
        m_containerManager->destroyContainer(id);
    }

    // Also wipe the panel container's MeterWidget items and rebuild
    // from factories. Before this, a persisted item payload (e.g. a
    // bar-style S-Meter saved by an earlier build) would survive
    // Reset because only the non-panel containers were destroyed —
    // Container #0's MeterWidget was left untouched, so the stale
    // items reloaded every launch and Reset felt like a no-op for
    // the main meter column.
    if (m_meterWidget) {
        m_meterWidget->clearItems();

        ItemGroup* smeter = ItemGroup::createSMeterPreset(
            MeterBinding::SignalAvg, QStringLiteral("S-Meter"), m_meterWidget);
        smeter->installInto(m_meterWidget, 0.0f, 0.0f, 1.0f, 0.45f);
        delete smeter;

        ItemGroup* pwrSwr = ItemGroup::createPowerSwrPreset(
            QStringLiteral("Power/SWR"), m_meterWidget);
        pwrSwr->installInto(m_meterWidget, 0.0f, 0.45f, 1.0f, 0.40f);
        delete pwrSwr;

        ItemGroup* alc = ItemGroup::createAlcPreset(m_meterWidget);
        alc->installInto(m_meterWidget, 0.0f, 0.85f, 1.0f, 0.15f);
        delete alc;
    }

    rebuildEditContainerSubmenu();
    qCInfo(lcContainer) << "Reset default layout: removed"
                         << toRemove.size() << "non-panel containers"
                         << "and rebuilt panel meter defaults";
}

void MainWindow::createDefaultContainers()
{
    // Container #0: panel-docked right side (AetherSDR pattern).
    // Placeholder content in 3G-1, replaced by AppletPanel in 3G-AP.
    ContainerWidget* c0 = m_containerManager->createContainer(1, DockMode::PanelDocked);
    c0->setNotes(QStringLiteral("Main Panel"));
    c0->setNoControls(false);

    qCDebug(lcContainer) << "Created default Container #0 (panel-docked):" << c0->id();
}

void MainWindow::populateDefaultMeter()
{
    ContainerWidget* c0 = m_containerManager->panelContainer();
    if (!c0) {
        qCWarning(lcContainer) << "No panel container for meter widget";
        return;
    }

    // Guard: don't repopulate if real content (AppletPanelWidget) already exists.
    // The container constructor creates a placeholder QLabel("Container") which
    // we need to replace. Check if content is already an AppletPanelWidget.
    if (qobject_cast<AppletPanelWidget*>(c0->content()) != nullptr) {
        return;
    }

    // Main constructs each applet once. The hidden legacy panel retains stable
    // S-meter lookup and connections; document hosts own the displayed views.
    m_appletPanel = new AppletPanelWidget(this);
    m_appletPanel->setManagedWorkspace(true);
    m_appletPanel->setArrangeController(m_containerManager->arrangeController());
    m_appletPanel->hide();
    auto* panel = m_appletPanel;

    // Task 41 (Phase 3P-II): wire the SMeterWidget (installed by the
    // AppletPanelWidget constructor) into MeterPoller.  WdspEngine is
    // needed for getRxaSignalPeak() and getMaxBinDbm() (MaxBin mode).
    if (m_meterPoller) {
        if (SMeterWidget* sm = m_appletPanel->smeterWidget()) {
            m_meterPoller->setSMeter(sm);
        }
        m_meterPoller->setWdspEngine(m_radioModel->wdspEngine());

        if (m_radioModel->role() == RadioModel::Role::Remote) {
            // Parity Task 33: Max Bin is measured here, from the slice's
            // own pan, as the local window measures it (panMaxBinSource).
            m_meterPoller->setRemoteRadioModel(m_radioModel,
                [this]() {
                    return m_stationClient && m_stationClient->isHandshakeComplete();
                },
                MeterPoller::panMaxBinSourceForSlice([this](const SliceModel* slice) -> SpectrumWidget* {
                    if (!slice || !m_panStack || slice->streamIndex() < 0
                        || markerOnlyPlacement(slice->sliceIndex())) {
                        return nullptr;
                    }
                    // 2026-10-02 KG4VCF, Codex: restore the TX carry's inherited
                    // resolver. Empty Core keys use this slice's actual host,
                    // never the active pan or another receiver's retained trace.
                    PanadapterApplet* pan = m_panStack->panadapter(windowPanFor(slice));
                    SliceModel* displayed = pan
                        ? m_radioModel->sliceById(pan->activeSliceIndex()) : nullptr;
                    if (!displayed || displayed->streamIndex() != slice->streamIndex()
                        || displayed->streamEpoch() != slice->streamEpoch()) {
                        return nullptr;
                    }
                    return pan->spectrumWidget();
                }));
            // R-R3-13 / R-R3-49 (parity Task 15): the ADC and AGC meters
            // read the Core's slice readings when it sends them.
            m_meterPoller->setRemoteMeterReadingsAvailable([this]() {
                return m_stationClient
                    && m_stationClient->capabilities().meterReadingsVersion >= 1;
            });
            // Remote models never initialize a local RxChannel, so the
            // WDSP-ready callback cannot start this timer for them.
            m_meterPoller->start();
        }

        // RX meter cal offset source (Thetis-faithful port).
        // RadioModel::rxMeterOffsetDb() returns RXPreampOffset(1) +
        // RXCalibrationOffset(1) per Thetis console.cs:21040 [v2.10.3.13].
        // The callable captures m_radioModel by raw pointer (lives for
        // the lifetime of MainWindow); pollSMeter / poll invoke it
        // once per tick.  Without this wire-up the WDSP S-meter readings
        // sit in raw ADC dBFS instead of at-antenna dBm.
        m_meterPoller->setRxOffsetSource([rm = m_radioModel]() -> double {
            return rm ? rm->rxMeterOffsetDb() : 0.0;
        });
    }

    // Remote Daemon R2 Task 12 step 7: give SliceMeterPump the SAME
    // rxMode()-driven source selector pollSMeter() uses for the analog
    // widget above, or the per-flag level bars and the analog needle
    // diverge by the 3-15 dB MeterPoller.cpp's own pollSMeter() comment
    // records (SignalPeak vs SignalAverage on a typical SSB signal) --
    // this is the change Task 16's bench row exists to eyeball: set the
    // analog S-Meter to Peak and to MaxBin and the flag bar must agree
    // with the needle. Queries m_appletPanel->smeterWidget() fresh on
    // every call rather than capturing the SMeterWidget* once, the same
    // "re-read live state each tick" shape the rxOffsetSource lambda
    // above uses for m_radioModel. sliceMeterPump() is null on a
    // Role::Remote model (Task 12 step 4b); the guard below is what keeps
    // this a no-op there instead of a null dereference.
    if (SliceMeterPump* pump = m_radioModel->sliceMeterPump()) {
        pump->setSourceSelector([this]() -> SliceMeterPump::MeterSource {
            SMeterWidget* sm = m_appletPanel ? m_appletPanel->smeterWidget() : nullptr;
            if (!sm) { return SliceMeterPump::MeterSource::SignalAverage; }
            switch (sm->rxMode()) {
            case SMeterWidget::RxMode::SMeter:
            case SMeterWidget::RxMode::SMeterPeak:
                return SliceMeterPump::MeterSource::SignalPeak;
            case SMeterWidget::RxMode::MaxBin:
                return SliceMeterPump::MeterSource::MaxBin;
            case SMeterWidget::RxMode::SignalAverage:
                break;
            }
            return SliceMeterPump::MeterSource::SignalAverage;
        });
    }

    // 2026-05-22 spectrum-calibration fix (Fix 1 from the S-meter / spectrum
    // alignment research). FFTEngine bins ship in raw dBFS (window-coherent
    // gain compensated against the digital I/Q full-scale reference). The
    // S-meter path adds the same RXPreampOffset + RXCalibrationOffset chain
    // Thetis applies at console.cs:21040 to land at antenna-referenced dBm,
    // but the spectrum path never got that calibration step. Result: the
    // S-meter and the spectrum trace lived in different reference frames
    // (38 dB gap on the bench at preamp Off; 18 dB at preamp On). Forward
    // the SAME rxMeterOffsetDb value to SpectrumWidget::setDbmCalOffset so
    // both views share the antenna reference. Per the calibration research,
    // carriers will then agree to ~1 dB between meter and spectrum. Noise
    // floor will still differ by 10*log10(NBP_BW/bin_BW) which is physics
    // (S-meter is passband-integrated; spectrum is per-bin) and matches
    // Thetis behavior.
    //
    // R-R3-46 / R-R3-11: each pan takes the offset of the ADC its stream is
    // on (pushSpectrumCalToPans), so a pan on the other ADC reads that ADC's
    // attenuator, not slice A's.
    if (activeSpectrumWidget() && m_radioModel) {
        connect(m_radioModel, &RadioModel::rxMeterOffsetChanged,
                this, [this](double) { pushSpectrumCalToPans(); });
        connect(m_radioModel, &RadioModel::rxAdcMeterOffsetsChanged,
                this, &MainWindow::pushSpectrumCalToPans);
        // Initial push so the cal lands at startup before any controller
        // change. setStepAttController already calls the recompute lambda
        // once on attach, but that may have run before this connect was
        // wired -- push the current value here defensively.
        pushSpectrumCalToPans();
    }

    // Refresh MaxBin's CTUN slice offset whenever the DDC center moves.
    // The frequencyChanged lambda in wireSliceToSpectrum pushes the
    // offset on slice retune, but a spectrum pan moves the DDC NCO
    // without moving the slice -- without this hook, MaxBin's scan
    // window stays at the OLD DDC-relative bin range until the next
    // slice retune (or CTUN toggle).  Observable as "MaxBin meter
    // drifts off the carrier when I pan the panadapter."
    if (activeSpectrumWidget()) {
        connect(activeSpectrumWidget(), &SpectrumWidget::ddcCenterFrequencyChanged,
                this, [this](double ddcCenter) {
            if (!m_radioModel) { return; }
            auto* eng = m_radioModel->wdspEngine();
            if (!eng) { return; }
            SliceModel* slice = activeSliceForWindow();
            if (!slice) { return; }
            eng->setMaxBinSliceOffsetHz(/*disp=*/0,
                                        slice->frequency() - ddcCenter);
        });
    }

    // Task 43 (Phase 3P-II): PGXL-aware power scale + TX meter feed.
    //
    // Four permanent connects (RadioModel persists across radio connects):
    //
    // 1. ampMetersChanged: when PGXL is OPERATE, forward the amp's
    //    forward-power/SWR readings to the SMeterWidget TX display.
    //    RadioModel::ampMetersChanged is fired by PgxlConnection on each
    //    statusUpdated containing "peakfwd" and "swr" keys.
    //
    // 2. RadioStatus::powerChanged: when PGXL is absent or STANDBY, use the
    //    radio's own PA telemetry (barefoot or Aurora) for the TX display.
    //    fwd is forward power in watts; the third argument (swr) is used.
    //
    // 3. amplifierChanged: snap the power scale to 2 kW on PGXL connect.
    //
    // 4. ampStateChanged: re-evaluate scale whenever OPERATE/STANDBY toggles.
    //    When returning to STANDBY (amp present but not OPERATE), the scale
    //    reverts to barefoot by passing hasAmplifier()=false to setPowerScale.
    if (SMeterWidget* sm = m_appletPanel->smeterWidget()) {
        // Connect 1: PGXL amp meters (OPERATE path).
        connect(m_radioModel, &RadioModel::ampMetersChanged, this,
                [this, sm](float fwd, float swr) {
            if (m_radioModel->hasAmplifier() && m_radioModel->ampOperate()) {
                sm->setTxMeters(fwd, swr);
            }
        });

        // Connect 2: radio barefoot/Aurora TX meters (STANDBY or no amp).
        // RadioStatus::powerChanged carries (fwdWatts, revWatts, swr); we
        // take fwdWatts (arg 1) and swr (arg 3) to match setTxMeters() signature.
        //
        // 2026-05-25 KG4VCF bench fix: gate on isAnyExternalAmpInOperate
        // (cross-vendor) instead of just PGXL state.  RadioStatus::powerChanged
        // fires much faster than RF-Kit's 1 Hz REST poll, so when RF-Kit
        // (without PGXL) is in OPERATE, the radio's barefoot reading was
        // overwriting the RF-Kit amp reading at ~20 Hz.  Now barefoot only
        // feeds when no external amp is amplifying.
        connect(&m_radioModel->radioStatus(), &RadioStatus::powerChanged,
                this, [this, sm](double fwd, double /*rev*/, double swr) {
            if (!m_radioModel->isAnyExternalAmpInOperate()) {
                sm->setTxMeters(static_cast<float>(fwd), static_cast<float>(swr));
            }
        });

        // Connect 3: PGXL connect event snaps scale to 2 kW.
        connect(m_radioModel, &RadioModel::amplifierChanged, this,
                [sm](bool present) {
            sm->setPowerScale(/*maxWatts=*/0, present);
        });

        // Connect 4: OPERATE/STANDBY state change re-evaluates scale.
        // When STANDBY: hasAmplifier()=true but we want barefoot scale,
        // so pass false (treat as absent) until OPERATE resumes.
        //
        // 2026-05-25 KG4VCF bench fix: use the cross-vendor predicate so
        // PGXL going STANDBY does not flip the SMeterWidget back to
        // barefoot scale if RF-Kit is still amplifying (and vice versa).
        connect(m_radioModel, &RadioModel::ampStateChanged, this,
                [this, sm]() {
            sm->setPowerScale(/*maxWatts=*/0,
                              m_radioModel->isAnyExternalAmpInOperate());
        });

        // Connect 5: 2026-05-20 bench fix -- SMeterWidget::setTransmitting
        // was implemented but never wired. m_transmitting stayed false so
        // updateNeedleTarget() always fell through to the RX dBm path,
        // even when ampMetersChanged was feeding watts via setTxMeters.
        // The needle therefore showed an RX S-meter reading during TX
        // even though PGXL was clearly delivering power. Wire MoxController
        // so the needle switches to the TX-power scale on key, returns to
        // RX scale on unkey. moxStateChanged fires at end of walk so the
        // switch lines up with carrier-on-air.
        if (MoxController* mox = m_radioModel->moxController()) {
            connect(mox, &MoxController::moxStateChanged,
                    sm, &SMeterWidget::setTransmitting);
        }
    }

    // Phase 3P-III review fix C1: wire the production SMeterWidget to the
    // cross-vendor RF-Kit aggregate signals.
    //
    // The displayed SMeterWidget is constructed via the QWidget-only ctor in
    // AppletPanelWidget, so the SMeterWidget(RadioModel*, QWidget*) overload +
    // connectToRadioModel() added in Task 13 are dead code in production.
    // externalAmpOperateChanged and externalAmpFwdSwrUpdated fire correctly
    // from RadioModel but the displayed widget never subscribed, leaving the
    // 2 kW scale switch silent on RF-Kit OPERATE and the TX needle frozen.
    //
    // Wire here, immediately after the analogous PGXL SMeterWidget block,
    // so all SMeterWidget signal connections live in one place. PGXL paths
    // (Connect 1..5 above) remain unchanged; these two add RF-Kit on top.
    if (SMeterWidget* sm = m_appletPanel->smeterWidget()) {
        // RF-Kit Connect A: snap 2 kW scale on RF-Kit OPERATE.
        // externalAmpOperateChanged is now transition-gated (fix I2) so this
        // fires only when the amp enters or leaves OPERATE, not every poll.
        //
        // 2026-05-25 KG4VCF bench fix: use the cross-vendor predicate so
        // RF-Kit going STANDBY does not flip back to barefoot scale if
        // PGXL is still amplifying.
        connect(m_radioModel, &RadioModel::externalAmpOperateChanged, this,
                [this, sm](bool /*inOp*/) {
            sm->setPowerScale(/*maxWatts=*/0,
                              m_radioModel->isAnyExternalAmpInOperate());
        });

        // RF-Kit Connect B: feed RF-Kit forward power + SWR to the TX needle.
        // externalAmpFwdSwrUpdated carries watts (int) and SWR (float);
        // setTxMeters takes (float fwdPower, float swr).
        //
        // 2026-05-25 KG4VCF bench fix: gate so a STANDBY amp doesn't push
        // 0 W into the needle while the radio's barefoot reading is also
        // wanting the meter.  Only fire when the amp is actually amplifying.
        //
        // The gate is RF-Kit's own OPERATE state, not the cross-vendor
        // isAnyExternalAmpInOperate(): this signal carries RF-Kit telemetry
        // exclusively, so the cross-vendor form let a PGXL in OPERATE pass
        // RF-Kit /power polls through from an RF2K-S sitting in STANDBY,
        // clobbering the live PGXL reading with RF-Kit's 0 W once per poll.
        // Codex review, PR #291.
        connect(m_radioModel, &RadioModel::externalAmpFwdSwrUpdated, this,
                [this, sm](int fwd, float swr) {
            if (m_radioModel->isRfKitInOperate()) {
                sm->setTxMeters(static_cast<float>(fwd), swr);
            }
        });
    }

    // Connect 5: Phase 3P-II Phase 4 Task 97 -- PGXL power cap soft-alert.
    // Fires a 5-second status-bar toast when peak forward power exceeds the
    // cap configured in Setup -> Peripherals -> PGXL Advanced -> Hardware.
    // De-bounced: one toast per exceedance event (re-arms below cap).
    // R-R3-47 / R-R3-22: the alert is computed where the amp is (the Core,
    // or this window's own model in process) and shown from the
    // `accessoryData` object, so every connected window sees it.
    m_powerCapAlertSeen = m_radioModel->accessoryDataModel()->powerCapAlertCount();
    connect(m_radioModel->accessoryDataModel(), &AccessoryDataModel::powerCapChanged,
            this, &MainWindow::onPowerCapAlertChanged);

    // Phase 3P-II review fix C2: surface TX interlock decisions to the
    // operator via 5-second status-bar toasts.  Without these connections
    // Block mode silently gates TX with no operator feedback.
    if (TxInterlockPolicy* policy = m_radioModel->txInterlockPolicy()) {
        connect(policy, &TxInterlockPolicy::warned,
                this, &MainWindow::onTxInterlockWarning);
        connect(policy, &TxInterlockPolicy::denied,
                this, &MainWindow::onTxInterlockDenial);
    }

    // RxApplet — Tier 1 wired to SliceModel (slice attached in wireSliceToSpectrum)
    m_rxApplet = new RxApplet(nullptr, m_radioModel, nullptr);
    panel->addApplet(m_rxApplet);

    // Phase 3P-I-a T16 — push board caps into RxApplet so ANT buttons
    // hide on HL2/Atlas. Matches the VFO Flag wiring below (T15).
    // B3: also push SKU profile so AntennaPopupBuilder knows rxOnlyLabels.
    m_rxApplet->setBoardCapabilities(m_radioModel->boardCapabilities());
    m_rxApplet->setHpsdrSku(m_radioModel->hardwareProfile().model);
    connect(m_radioModel, &RadioModel::currentRadioChanged, m_rxApplet,
            [this]() {
        m_rxApplet->setBoardCapabilities(m_radioModel->boardCapabilities());
        m_rxApplet->setHpsdrSku(m_radioModel->hardwareProfile().model);
    });

    // ── Phase 3F (Bug 3): per-slice RX applet + active-slice switching ──────
    //
    // Clicking a slice tab in the RX applet, or clicking a VfoWidget flag,
    // routes to RadioModel::setActiveSlice. RadioModel::activeSliceChanged
    // then rebinds the RX applet to the new active slice, refreshes the tab
    // row highlight, and updates the static badge. The slice tab row is also
    // refreshed whenever the slice list changes (add/remove). Workflow ported
    // from AetherSDR (RxApplet::sliceActivationRequested ->
    // MainWindow::setActiveSlice at MainWindow.cpp:3277, and the
    // setActiveSliceInternal rebind at MainWindow.cpp:12132 [@6a142807]).
    //
    // setActiveSliceById for the same reason the flag path uses it:
    // updateSliceButtons keys each tab's button-group id to the slice's own
    // sliceIndex() rather than its list position, so the id has to be
    // converted rather than indexed with.
    connect(m_rxApplet, &RxApplet::sliceActivationRequested, this,
            [this](int sliceId) { selectSliceForWindow(sliceId); });
    // R-R3-49 (parity Task 1): a Shift-click that could not also set the TX
    // passband says why, as the VFO flag's does.
    connect(m_rxApplet, &RxApplet::transmitSettingRefused, this,
            [this](const QString& reason) {
        showToast(reason, ToastSeverity::Info, 3000);
    });

    auto refreshSliceTabs = [this]() {
        if (m_rxApplet && m_radioModel) {
            if (desktopHosting()) { refreshDesktopStationState(); return; }
            // Slice control plan Task 15: the tabs, their access and the
            // bound slice come from one place.
            refreshRxAppletSlices();
        }
    };

    connect(m_radioModel, &RadioModel::activeSliceChanged, this,
            [this](int) {
        if (!m_radioModel) { return; }
        if (desktopHosting()) { refreshDesktopStationState(); return; }
        // Rebind the RX applet to this window's RX slice, refresh its badge
        // and the tab row highlight (Slice control plan Task 15).
        refreshRxAppletSlices();
    });
    // Slice control plan Task 15: a listened tab's Take control, Release and
    // Stop listening run as the flag's menu does, with its wait and answer.
    connect(m_rxApplet, &RxApplet::takeControlRequested, this,
            [this](int id) { runFlagAccessAction(SliceChooserAction::TakeControl, id); });
    connect(m_rxApplet, &RxApplet::releaseRequested, this,
            [this](int id) { runFlagAccessAction(SliceChooserAction::Release, id); });
    connect(m_rxApplet, &RxApplet::stopListeningRequested, this,
            [this](int id) { runFlagAccessAction(SliceChooserAction::StopListening, id); });
    // The station device's receive focus moves on a listened tab or flag
    // without moving the active slice, so nothing else signals it.
    if (SliceOwnership* ownership = m_radioModel->sliceOwnership()) {
        connect(ownership, &SliceOwnership::activeRxChanged, this,
                [this](const QByteArray& device) {
            if (device != SliceOwnership::stationDevice() || !desktopHosting()) { return; }
            refreshRxAppletSlices();
            refreshActiveSlicePresentation();
        });
    }

    // Keep the tab row in sync with the slice population.
    connect(m_radioModel, &RadioModel::sliceAdded, this,
            [refreshSliceTabs](int) { refreshSliceTabs(); });
    connect(m_radioModel, &RadioModel::sliceRemoved, this,
            [refreshSliceTabs](int) { refreshSliceTabs(); });

    // DisplayApplet, 3D Stacked-Trace Spectrum Plan Task 22. Sits
    // immediately after RxApplet in the panel add order. Follows the
    // active panadapter via RadioModel::spectrumWidget() /
    // spectrumWidgetChanged; no further wiring needed here.
    m_displayApplet = new DisplayApplet(m_radioModel, nullptr);
    panel->addApplet(m_displayApplet);

    // TxApplet — NYI shell (Phase 3I-1)
    // 3M-3a-ii Batch 6: cache pointer in m_txApplet so SetupDialog
    // instances can wire CfcSetupPage's [Configure CFC bands…] button
    // through to TxApplet::requestOpenCfcDialog.
    auto* txApplet = new TxApplet(m_radioModel, nullptr);
    m_txApplet = txApplet;
    panel->addApplet(txApplet);

    // ── 3M-1c Phase L: hand TxApplet the controllers it needs ──────────────
    //
    // L.1 — MicProfileManager (J.1 setter): drives the TX Profile combo
    // population + active-profile mirror + "Default" seed surfacing.  The
    // pointer is obtained from RadioModel (constructed in the ctor; per-MAC
    // scope is set inside connectToRadio).  Pre-connect, the manager is
    // unscoped and the combo simply stays at the placeholder "Default" item
    // (rebuildProfileCombo() no-ops when no manager is set).
    //
    // L.2 — TwoToneController (J.2 setter): drives the 2-TONE button toggle
    // round-trip.  The controller's setActive(true) refuses with a
    // qCWarning when m_powerOn is false, so pre-connect button presses are
    // safely rejected.
    //
    // L (J.4) — txProfileMenuRequested signal: a right-click on the profile
    // combo opens SetupDialog at "TX Profile".  Lambda-construct a fresh
    // SetupDialog each time (matches the 7 other "open setup" sites in
    // MainWindow.cpp at lines 1283 / 2824 / 2834 / 2846 / 3029 / 3428).
    if (m_radioModel) {
        txApplet->setMicProfileManager(m_radioModel->micProfileManager());
        txApplet->setTwoToneController(m_radioModel->twoToneController());
    }
    connect(txApplet, &TxApplet::txProfileMenuRequested, this, [this]() {
        auto* dialog = createSetupDialog();
        if (dialog == nullptr) {
            return;  // the gate refused and has already said why
        }
        dialog->selectPage(QStringLiteral("TX Profile"));
        dialog->show();
    });

    // ── Phase 3M-4 Task 13: PS-A right-click → open PsForm ─────────────────
    // Mirrors Thetis chkFWCATUBypass_MouseDown (console.cs:46149-46152
    // [v2.10.3.13]).  PsForm is the same singleton dialog opened from the
    // Tools / DSP menu and from PureSignalApplet right-click handlers.
    connect(txApplet, &TxApplet::openPureSignalDialogRequested,
            this, &MainWindow::openPureSignalDialog);

    // Phase 3M-4 Task 13 — capability-gated PS-A visibility.  Push initial
    // caps + keep them in sync on RadioModel::currentRadioChanged.  TxApplet
    // hides m_psaBtn when caps.hasPureSignal == false (HL2 / Atlas).
    txApplet->setBoardCapabilities(m_radioModel->boardCapabilities());
    connect(m_radioModel, &RadioModel::currentRadioChanged, txApplet,
            [this, txApplet]() {
        txApplet->setBoardCapabilities(m_radioModel->boardCapabilities());
    });

    // Phase 3M-4 Task 13 — late-bound coordinator handoff.  TxApplet
    // already self-subscribes inside its ctor (see TxApplet.cpp wireControls
    // PS-A block), so no explicit connect needed here.  Still: push the
    // current coordinator at startup in case it's already live (test path).
    if (PureSignal* ps = m_radioModel->pureSignal()) {
        txApplet->setPureSignal(ps);
    }

    // Slice control plan Task 11: TxApplet follows the transmit-bound
    // slice's band itself (TxApplet::followTransmitSlice), so the per-band
    // Tune Power and power sliders read that slice's band, never the
    // panadapter's or a listened slice's.

    // PhoneCwApplet — Phone + CW pages, NYI
    m_phoneCwApplet = new PhoneCwApplet(m_radioModel, nullptr);
    panel->addApplet(m_phoneCwApplet);
    // R-R3-21: the compression gauge shows the meters' Compression reading.
    if (m_meterPoller) {
        connect(m_meterPoller, &MeterPoller::txMeterReading, m_phoneCwApplet,
                [applet = m_phoneCwApplet](int bindingId, double value) {
            if (bindingId == MeterBinding::TxComp) {
                applet->setCompressionReading(value);
            }
        });
    }

    // RadeApplet — Phase 3R L2.  Sits alongside PhoneCwApplet but is
    // visible only when the active slice's mode is DSPMode::RADE_U or
    // DSPMode::RADE_L.  The initial mode is set in the dspModeChanged
    // lambda below; for the default startup mode (USB) the applet
    // starts hidden.
    m_radeApplet = new RadeApplet(m_radioModel, nullptr);
    panel->addApplet(m_radeApplet);
    m_radeApplet->setVisible(false);

    // Ghost applets — hidden per docs/superpowers/plans/2026-05-01-ui-polish-right-panel.md §Task 6.
    // These applets are entirely placeholder-only today (no wired controls).
    // Showing them is misleading — users click e.g. "Equalizer" and nothing happens.
    // Uncomment each when its feature phase ships (one-line re-enable).
    //
    // m_eqApplet = new EqApplet(m_radioModel, nullptr);           // TODO 3I-3: TX/RX EQ wiring
    // panel->addApplet(m_eqApplet);

    // VaxApplet — per-VAX-channel gain + mute + level meters
    // (Phase 3O Sub-Phase 9 Task 9.2b).
    m_vaxApplet = new VaxApplet(m_radioModel,
                                m_radioModel->audioEngine(), nullptr);
    panel->addApplet(m_vaxApplet);
    // iPhone app plan Task 25: in a remote window, the Core computer's VAX
    // below this computer's own; its meters only while the applet shows.
    connect(m_vaxApplet, &VaxApplet::stationLevelsWantedChanged, this, [this](bool wanted) {
        if (m_stationClient != nullptr) {
            m_stationClient->setStationVaxLevelsWanted(wanted);
        }
    });
    refreshRemoteStationVax();

    // Phase 3M-4 Task 13 — PureSignalApplet quick-access surface.
    //
    // Constructed unconditionally and added to the right panel, but
    // visibility is gated on caps.hasPureSignal in onConnectionStateChanged
    // (HL2 / Atlas hide the applet entirely; G2-class boards show it).
    // Right-click on every PureSignalApplet control opens PsForm via the
    // openPureSignalDialogRequested signal, which MainWindow forwards to
    // openPureSignalDialog (same singleton dialog as Tools / DSP menu).
    m_pureSignalApplet = new PureSignalApplet(m_radioModel, nullptr);
    panel->addApplet(m_pureSignalApplet);
    connect(m_pureSignalApplet,
            &PureSignalApplet::openPureSignalDialogRequested,
            this, &MainWindow::openPureSignalDialog);
    // Initial visibility from current board caps; tracked thereafter via
    // onConnectionStateChanged (where hasPureSignal is also gated on the
    // PSA bottom-banner indicator).
    m_pureSignalApplet->setVisible(
        m_radioModel->boardCapabilities().hasPureSignal);

    // AM Mod Monitor (NereusSDR-original): peak-reading +/- modulation
    // meters, flashers, carrier lamp and envelope scope, fed by the TX
    // I/Q tap or the PureSignal feedback receiver.  Visibility is a plain
    // user preference (View > Containers > Applets).
    m_modMonApplet = new ModMonitorApplet(m_radioModel, nullptr);
    panel->insertApplet(0, m_modMonApplet);   // directly below the S-Meter

    // Phase 23: TCI applets — live in Container #0 below the existing applets.
    // Visibility is now managed by AppletVisibilityController below
    // (registered as ids "Tci" + "ClientChain", keys AppletTciVisible +
    // AppletClientChainVisible). Legacy keys TciApplet_Visible /
    // ClientChainApplet_Visible from earlier versions become orphans on
    // upgrade; existing users get TCI applets back to default-visible.
#ifdef HAVE_WEBSOCKETS
    if (m_tciServer) {
        m_tciApplet = new TciApplet(m_tciServer, nullptr);
        m_tciApplet->setStationContext(m_tciSwitch, m_radioModel);
        panel->addApplet(m_tciApplet);
        connect(m_tciApplet, &TciApplet::setupRequested,
                this, &MainWindow::openTciSetupPage);
        // showClientsRequested: scroll/show the ClientChainApplet.
        // ClientChainApplet is constructed immediately below, so capture
        // by pointer — the lambda runs only after full construction.
        connect(m_tciApplet, &TciApplet::showClientsRequested,
                this, [this]() {
                    if (m_clientChainApplet && m_appletVis) {
                        m_appletVis->setVisible(
                            QStringLiteral("ClientChain"), true);
                        m_clientChainApplet->raise();
                    }
                });

        m_clientChainApplet = new ClientChainApplet(m_tciServer, nullptr);
        m_clientChainApplet->setStationModel(m_radioModel);
        panel->addApplet(m_clientChainApplet);
    }
#endif

    // Phase 3P-II Task 20: AmpApplet (PGXL telemetry + OPERATE toggle).
    // Added to the panel alongside the other applets. Signal routing to
    // PgxlConnection is wired in onConnectionStateChanged() so every
    // radio-connect gets a fresh binding without double-connects.
    m_ampApplet = new AmpApplet(m_radioModel, nullptr);
    panel->addApplet(m_ampApplet);

    // Phase 3P-II Task 20: TunerApplet (TGXL controls + relay bars).
    // Was previously commented out ("TODO ATU phase"). Now constructed
    // with the TunerModel* owned by RadioModel so it tracks TGXL state
    // from construction time.
    // Phase 3P-II Phase 4 Task 89: pass RadioModel's shared TuneMemoryStore
    // so saves from the context menu are visible in TgxlAdvancedPage and vice versa.
    m_tunerApplet = new TunerApplet(m_radioModel,
                                    m_radioModel->tunerModel(),
                                    nullptr,
                                    m_radioModel->tuneMemoryStore());
    panel->addApplet(m_tunerApplet);

    // 2026-05-20 bench fix: rescale TunerApplet's fwd-power bar when
    // PGXL comes into the chain. TunerApplet defaults to 0-200 W
    // (barefoot) which pegs out the moment PGXL pushes its amplified
    // ~450-2000 W through the tuner. Mirror the same amplifierChanged
    // + ampStateChanged wires we use for the SMeterWidget above.
    if (m_tunerApplet) {
        // Initial scale: match current PGXL state at construction time.
        const bool amplifyingNow =
            m_radioModel->hasAmplifier() && m_radioModel->ampOperate();
        m_tunerApplet->setPowerScale(/*maxWatts=*/0, amplifyingNow);

        // PGXL connect/disconnect snaps the scale to 2 kW or back to
        // barefoot. maxWatts=0 means "use the standard barefoot/PGXL
        // range from TunerApplet::setPowerScale defaults".
        connect(m_radioModel, &RadioModel::amplifierChanged, this,
                [this](bool present) {
            if (m_tunerApplet) {
                m_tunerApplet->setPowerScale(/*maxWatts=*/0, present);
            }
        });

        // OPERATE/STANDBY edges re-evaluate scale. STANDBY -> barefoot
        // until OPERATE resumes (pass amplifying=false to drop the
        // 2 kW scale back to 200 W).
        connect(m_radioModel, &RadioModel::ampStateChanged, this,
                [this]() {
            if (m_tunerApplet) {
                const bool amplifying = m_radioModel->hasAmplifier()
                                        && m_radioModel->ampOperate();
                m_tunerApplet->setPowerScale(/*maxWatts=*/0, amplifying);
            }
        });
    }

    // Phase 3P-III Task 14: RF-Kit RF2K-S applet.
    // Constructed unconditionally alongside the other peripheral applets.
    // Visibility is gated on rfKitEnabled() via the AppletVisibilityController
    // availability axis (set below, live-updated via rfKitEnabledChanged).
    //
    // Data-flow and context-menu signals are wired once here in buildUI()
    // because rfKitConnection() returns the same permanent object for the
    // app lifetime (RadioModel creates it in its ctor, never destroys it).
    // This differs from the AmpApplet/TunerApplet pattern (wired in
    // onConnectionStateChanged) because PGXL/TGXL use PGX-specific
    // auto-connect logic gated on fourO3AEnabled; the RF-Kit connects
    // independently at startup when rfKitEnabled is true.
    m_rfKitApplet = new Rf2ksApplet(m_radioModel, nullptr);
    panel->addApplet(m_rfKitApplet);

    {
        // Connection -> applet data flow.
        Rf2ksConnection* rfKitConn = m_radioModel->rfKitConnection();
        if (rfKitConn) {
            // R-R3-47 / R-R3-22: power, OPERATE, the connection dot, the
            // name and version, and (Task 3) the tuner and antenna rows come
            // from RadioModel's RfKitModel, which Rf2ksApplet reads itself
            // (the Core's `rfkit` object in a remote window).

            // Applet -> connection (antenna click, operate toggle).
            // R-R3-49 (parity Task 10): a remote window's applet asks the
            // Core itself (setRfKitAntenna, setRfKitOperate); this
            // computer's RF-Kit connection is local only.
            connect(m_rfKitApplet, &Rf2ksApplet::antennaRequested,
                    this, [this](RfKitAntenna::Type type, int number) {
                // Group B fix wave (M5): refused on the air, as the applet
                // and a remote window are.
                if (m_radioModel->role() == RadioModel::Role::Remote
                    || m_radioModel->stationOnAirRefusal(nullptr)) {
                    return;
                }
                if (Rf2ksConnection* conn = m_radioModel->rfKitConnection()) {
                    conn->setActiveAntenna(type, number);
                }
            });
            connect(m_rfKitApplet, &Rf2ksApplet::operateToggled,
                    this, [this](bool wantOperate) {
                if (m_radioModel->role() == RadioModel::Role::Remote
                    || m_radioModel->stationOnAirRefusal(nullptr)) {   // M5
                    return;
                }
                Rf2ksConnection* conn = m_radioModel->rfKitConnection();
                if (!conn) { return; }
                conn->setOperateMode(wantOperate
                    ? QStringLiteral("OPERATE")
                    : QStringLiteral("STANDBY"));
            });
        }

        // Context menu signals (always wired regardless of connection state).
        connect(m_rfKitApplet, &Rf2ksApplet::navigationRequested,
                this, &MainWindow::openSetup);

        connect(m_rfKitApplet, &Rf2ksApplet::connectionToggleRequested,
                this, [this]() {
            // R-R3-22 / R-R3-47: a remote window's applet asks the Core
            // itself (disconnectRfKit, configureRfKit) and shows the
            // answer on its own line; this computer's connection is local
            // only.
            if (m_radioModel->role() == RadioModel::Role::Remote) {
                return;
            }
            Rf2ksConnection* conn = m_radioModel->rfKitConnection();
            if (!conn) { return; }
            if (conn->isConnected()) {
                conn->disconnect();
            } else {
                // Per-radio peripherals refactor (2026-05-26): host/port
                // live under hardware/<mac>/peripherals/.  When no radio
                // is connected, peripheralValue() returns the default and
                // the empty-host gate below makes this a safe no-op.
                const QString host = m_radioModel->peripheralValue(
                    QStringLiteral("RfKit_ManualIp"));
                const quint16 port = static_cast<quint16>(
                    m_radioModel->peripheralValue(
                        QStringLiteral("RfKit_ManualPort"),
                        QStringLiteral("8080")).toUInt());
                if (!host.isEmpty()) {
                    conn->connectToAmp(host, port);
                }
            }
        });

        connect(m_rfKitApplet, &Rf2ksApplet::diagnosticsCopyRequested,
                this, [this]() {
            // R-R3-49 (parity Task 10): a remote window copies the Core's
            // connection (the mirrored `rfkit` object and accessoryData's
            // rfkit counters), not this computer's idle one.
            if (m_radioModel->role() == RadioModel::Role::Remote) {
                QGuiApplication::clipboard()->setText(
                    Rf2ksApplet::coreDiagnosticsText(m_radioModel));
                return;
            }
            Rf2ksConnection* conn = m_radioModel->rfKitConnection();
            QString diag;
            diag += QStringLiteral("RF-Kit RF2K-S diagnostics\n");
            if (conn) {
                diag += QStringLiteral("Host: %1:%2\n")
                            .arg(conn->peerAddress()).arg(conn->peerPort());
                diag += QStringLiteral("Version: %1\n")
                            .arg(conn->softwareVersion());
                diag += QStringLiteral("Polls OK/failed: %1/%2\n")
                            .arg(conn->pollsSucceeded())
                            .arg(conn->pollsFailed());
                diag += QStringLiteral("RTT avg: %1 ms\n")
                            .arg(conn->rttAvgLast10Ms());
                // R-R3-49 (parity Task 10): the lines a remote window's copy
                // shows from the Core's counters.
                const auto time = [](qint64 ms) {
                    return ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms).toString(Qt::ISODate)
                                  : QStringLiteral("--");
                };
                diag += QStringLiteral("Operate: %1\nInterface: %2\n")
                            .arg(conn->operateMode() == QStringLiteral("OPERATE")
                                     ? QStringLiteral("Yes") : QStringLiteral("No"),
                                 conn->operationalInterface().isEmpty()
                                     ? QStringLiteral("--") : conn->operationalInterface());
                diag += QStringLiteral("Reconnects: %1\n").arg(conn->reconnectAttempts());
                diag += QStringLiteral("Connected since: %1\n")
                            .arg(time(conn->connectedSinceMs()));
                diag += QStringLiteral("Last poll: %1\n").arg(time(conn->lastPollMs()));
            } else {
                diag += QStringLiteral("(connection unavailable)\n");
            }
            QGuiApplication::clipboard()->setText(diag);
        });
    }

    // R-R3-47 / R-R3-22: a remote window's antenna names are the Core's.
    if (m_radioModel->role() == RadioModel::Role::Remote) {
        connect(m_radioModel->accessoryDataModel(), &AccessoryDataModel::labelsChanged, this,
                [this] {
            const AccessoryDataModel* data = m_radioModel->accessoryDataModel();
            const QStringList tgxl = data->tgxlAntennaLabels();
            if (m_tunerApplet) {
                for (int i = 0; i < tgxl.size(); ++i) {
                    m_tunerApplet->onAntennaLabelChanged(i + 1, tgxl.at(i));
                }
            }
            const QStringList rfkit = data->rfkitAntennaLabels();
            if (m_rfKitApplet) {
                for (int i = 0; i < rfkit.size(); ++i) {
                    m_rfKitApplet->setAntennaLabel(i + 1, rfkit.at(i));
                }
            }
        });
    }

    // Antenna labels: load operator-set labels from AppSettings at startup.
    // Keys: RfKit_Ant1_Label .. RfKit_Ant4_Label (stored by RfKitPage.cpp).
    // If a key is absent or empty the applet already shows "ANT N" by default.
    for (int i = 1; i <= 4; ++i) {
        const QString label = AppSettings::instance()
            .value(QStringLiteral("RfKit_Ant%1_Label").arg(i)).toString();
        if (!label.isEmpty()) {
            m_rfKitApplet->setAntennaLabel(i, label);
        }
    }

    // ── Applet visibility controller (Containers > Applets + ☰ menus) ──
    // NereusSDR-original. Backs the show/hide menu surfaces.
    //
    // Registered applets get a checkable menu entry in Containers > Applets
    // AND in the right-side panel's ☰ banner menu. Add new entries here as
    // additional applets ship.
    //
    // RadeApplet caveat: its visibility is mode-driven (auto-shown when
    // the active slice is in DSPMode::RADE_U/_L; hidden otherwise) via
    // the dspModeChanged lambda further down. The menu toggle here is a
    // user override that lasts until the next mode change repopulates
    // visibility. Acceptable for v1; tighter integration is a follow-up.
    m_appletVis = new AppletVisibilityController(this);
    m_appletVis->setWorkspaceAdapter(m_containerManager->workspaceStore(), m_containerManager->contentRegistry());
    connect(m_appletVis, &AppletVisibilityController::persistenceFailed, this, [this](const QString& error) {
        showToast(tr("Container layout could not be saved: %1").arg(error), ToastSeverity::Warning, 6000);
    });

    m_appletsById[QStringLiteral("Rx")]         = m_rxApplet;
    m_appletsById[QStringLiteral("Display")]    = m_displayApplet;
    m_appletsById[QStringLiteral("Tx")]         = m_txApplet;
    m_appletsById[QStringLiteral("PhoneCw")]    = m_phoneCwApplet;
    m_appletsById[QStringLiteral("Rade")]       = m_radeApplet;
    m_appletsById[QStringLiteral("Vax")]        = m_vaxApplet;
    m_appletsById[QStringLiteral("PureSignal")] = m_pureSignalApplet;
    m_appletsById[QStringLiteral("ModMon")]     = m_modMonApplet;
    m_appletsById[QStringLiteral("Amp")]        = m_ampApplet;
    m_appletsById[QStringLiteral("Tuner")]      = m_tunerApplet;
    m_appletsById[QStringLiteral("RfKit")]      = m_rfKitApplet;
#ifdef HAVE_WEBSOCKETS
    if (m_tciApplet) {
        m_appletsById[QStringLiteral("Tci")]        = m_tciApplet;
    }
    if (m_clientChainApplet) {
        m_appletsById[QStringLiteral("ClientChain")] = m_clientChainApplet;
    }
#endif

    // Display names match each applet's appletTitle() — keep in sync if
    // an applet's title changes.
    //
    // PureSignal defaults to visible. The existing onConnectionStateChanged
    // handler still calls m_pureSignalApplet->setVisible(caps.hasPureSignal)
    // on radio connect, which can hide the inner widget on HL2/Atlas; the
    // menu entry stays available for users who want to force-show or
    // permanently hide it. Defaulting to true here means new G2 users see
    // PS immediately without having to discover the menu toggle.
    m_appletVis->registerApplet(QStringLiteral("Rx"),
                                QStringLiteral("RX"),           true);
    m_appletVis->registerApplet(QStringLiteral("Display"),
                                QStringLiteral("Display"),      true);
    m_appletVis->registerApplet(QStringLiteral("Tx"),
                                QStringLiteral("TX"),           true);
    m_appletVis->registerApplet(QStringLiteral("PhoneCw"),
                                QStringLiteral("Phone / CW"),   true);
    // RADE: defaultVisible=true (user pref). Actual visibility is gated
    // on the active slice's mode via the availability axis — the
    // dspModeChanged lambda below calls setAvailable(true) only when
    // mode is DSPMode::RADE_U/_L. Initial availability set to false here
    // since the default startup mode is USB; the mode lambda fires
    // shortly after to correct it if needed.
    m_appletVis->registerApplet(QStringLiteral("Rade"),
                                QStringLiteral("RADE"),         true);
    m_appletVis->registerApplet(QStringLiteral("Vax"),
                                QStringLiteral("VAX"),          true);
    m_appletVis->registerApplet(QStringLiteral("PureSignal"),
                                QStringLiteral("PureSignal"),   true);
    m_appletVis->registerApplet(QStringLiteral("ModMon"),
                                QStringLiteral("AM Mod Monitor"), true);
    m_appletVis->registerApplet(QStringLiteral("Amp"),
                                QStringLiteral("Power Genius"), true);
    m_appletVis->registerApplet(QStringLiteral("Tuner"),
                                QStringLiteral("Tuner Genius"), true);
    m_appletVis->registerApplet(QStringLiteral("RfKit"),
                                QStringLiteral("RF-Kit RF2K-S"), true);
#ifdef HAVE_WEBSOCKETS
    if (m_tciApplet) {
        m_appletVis->registerApplet(QStringLiteral("Tci"),
                                    QStringLiteral("TCI Server"),  true);
    }
    if (m_clientChainApplet) {
        m_appletVis->registerApplet(QStringLiteral("ClientChain"),
                                    QStringLiteral("TCI Clients"), true);
    }
#endif

    // Capability gates: applets that depend on an external feature flag
    // get their availability set here. When availability is false, the
    // applet is hidden AND its menu entries are greyed out. The user's
    // persisted visibility preference is preserved across availability
    // changes (so re-enabling 4O3A pops the applet back if the user
    // wanted it visible).
    const bool fourO3AOn = m_radioModel && m_radioModel->fourO3AEnabled();
    m_appletVis->setAvailable(QStringLiteral("Amp"),   fourO3AOn);
    m_appletVis->setAvailable(QStringLiteral("Tuner"), fourO3AOn);
    // RADE: available only in RADE_U / RADE_L modes. Startup mode is
    // USB, so initial availability=false. The dspModeChanged lambda
    // below updates this on every mode change.
    m_appletVis->setAvailable(QStringLiteral("Rade"),  false);

    // RF-Kit RF2K-S: available only when the master toggle is enabled.
    // Default OFF; live-updated via rfKitEnabledChanged below.
    const bool rfKitOn = m_radioModel && m_radioModel->rfKitEnabled();
    m_appletVis->setAvailable(QStringLiteral("RfKit"), rfKitOn);

    // Apply initial visibility state from the controller (in case
    // AppSettings already had values from a prior session).
    // Uses effective visibility (user pref AND available).
    for (const QString& id : m_appletVis->registeredIds()) {
        if (m_appletsById.value(id, nullptr)) {
            m_containerManager->contentRegistry()->setAvailable(ContainerContentRegistry::appletTypeForVisibilityId(id), m_appletVis->isAvailable(id));
        }
    }

    // Pump future EFFECTIVE-visibility changes from the controller into
    // the panel. effectiveVisibilityChanged fires when either the user
    // toggle or the availability gate flips the net visibility, so we
    // catch both menu clicks and external capability changes (e.g. 4O3A).
    connect(m_appletVis, &AppletVisibilityController::effectiveVisibilityChanged,
            this, [this](const QString& id, bool effective) {
        Q_UNUSED(effective);
        if (m_containerManager && m_containerManager->contentRegistry()) {
            m_containerManager->contentRegistry()->setAvailable(ContainerContentRegistry::appletTypeForVisibilityId(id), m_appletVis->isAvailable(id));
            m_containerManager->reconcileWorkspace(m_containerManager->workspaceStore()->snapshot());
        }
    });

    // Live-track 4O3A master toggle so Amp/Tuner availability updates
    // without an app restart. RadioModel::setFourO3AEnabled emits the
    // signal whenever the persisted value changes.
    if (m_radioModel) {
        connect(m_radioModel, &RadioModel::fourO3AEnabledChanged,
                this, [this](bool enabled) {
            if (!m_appletVis) { return; }
            m_appletVis->setAvailable(QStringLiteral("Amp"),   enabled);
            m_appletVis->setAvailable(QStringLiteral("Tuner"), enabled);
        });

        // Phase 3P-III Task 14: live-track RF-Kit master toggle so the
        // RfKit applet availability updates without an app restart.
        connect(m_radioModel, &RadioModel::rfKitEnabledChanged,
                this, [this](bool enabled) {
            if (!m_appletVis) { return; }
            m_appletVis->setAvailable(QStringLiteral("RfKit"), enabled);
        });
    }

    // ── Banner ☰ menu on AppletPanelWidget ──────────────────────────────
    if (m_appletVis && m_appletPanel) {
        m_bannerAppletsMenu = new QMenu(this);

        for (const QString& id : m_appletVis->registeredIds()) {
            QAction* act = m_bannerAppletsMenu->addAction(
                m_appletVis->displayName(id));
            act->setCheckable(true);
            act->setChecked(m_appletVis->isVisible(id));
            // Grey out the entry when the applet is currently unavailable
            // (e.g. Amp/Tuner when 4O3A is disabled). The check state still
            // reflects the user preference so re-enabling restores it.
            act->setEnabled(m_appletVis->isAvailable(id));
            // User-visible tooltip — plain English.
            act->setToolTip(QStringLiteral("Show or hide the %1 applet")
                            .arg(m_appletVis->displayName(id)));

            connect(act, &QAction::toggled, this, [this, id](bool checked) {
                if (m_appletVis) { m_appletVis->setVisible(id, checked); }
            });
            m_bannerAppletActions.insert(id, act);
        }

        // Sync banner checkmarks when state changes elsewhere (top menu).
        connect(m_appletVis, &AppletVisibilityController::visibilityChanged,
                this, [this](const QString& id, bool visible) {
            if (auto* act = m_bannerAppletActions.value(id, nullptr)) {
                QSignalBlocker block(act);
                act->setChecked(visible);
            }
        });

        // Grey/un-grey banner entries when an applet's availability
        // changes (e.g. 4O3A master toggle flipped).
        connect(m_appletVis, &AppletVisibilityController::availabilityChanged,
                this, [this](const QString& id, bool available) {
            if (auto* act = m_bannerAppletActions.value(id, nullptr)) {
                act->setEnabled(available);
            }
        });

        m_appletPanel->setBannerMenu(m_bannerAppletsMenu);
    }

    // Ghost applets: constructed but not added to the panel or the Containers menu
    // until their feature phases ship. Uncomment the construction + addContainerToggle
    // call (in buildMenuBar) together when the feature lands.
    //
    // m_digitalApplet    = new DigitalApplet(m_radioModel, nullptr);    // TODO 3-VAX
    // m_diversityApplet  = new DiversityApplet(m_radioModel, nullptr);  // TODO 3F (multi-RX)
    // m_cwxApplet        = new CwxApplet(m_radioModel, nullptr);        // TODO 3M-2 (CW TX)
    // m_dvkApplet        = new DvkApplet(m_radioModel, nullptr);        // TODO 3M-1 (DVK)
    // m_catApplet        = new CatApplet(m_radioModel, nullptr);        // TODO 3J/3K/3-VAX

    // Detach the analog singleton before the old header wrapper is disposed.
    panel->clearHeaderWidget();
    auto* registry = m_containerManager->contentRegistry();
    registry->attachSingleton("applet:s_meter", panel->smeterWidget());
    for (const auto& descriptor : registry->descriptors()) {
        if (descriptor.singleton && descriptor.typeId != "applet:s_meter") { registry->setAvailable(descriptor.typeId, false, tr("The live applet is not available in this session")); }
    }
    for (const QString& id : m_appletVis->registeredIds()) {
        const QString type = ContainerContentRegistry::appletTypeForVisibilityId(id);
        registry->setAvailable(type, m_appletVis->isAvailable(id));
        registry->attachSingleton(type, m_appletsById.value(id));
    }
    if (auto* host = m_containerManager->contentHost(c0->id())) { host->setBannerMenu(m_bannerAppletsMenu); }
    if (!m_containerManager->storageError().isEmpty()) {
        showToast(tr("Container layout is read-only: %1").arg(m_containerManager->storageError()), ToastSeverity::Warning, 6000);
    }
    qCDebug(lcMeter) << "Installed default meter layout: S-Meter + Power/SWR + ALC";
    qCDebug(lcContainer) << "Container #0: Meters + RxApplet + TxApplet + PhoneCwApplet + VaxApplet + TciApplet + ClientChainApplet";
}

void MainWindow::buildMenuBar()
{
    // =========================================================================
    // FILE
    // =========================================================================
    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));

    {
        QAction* settingsAction = fileMenu->addAction(QStringLiteral("&Settings..."),
            this, [this]() {
                auto* dialog = createSetupDialog();
                if (dialog == nullptr) {
                    return;  // the gate refused and has already said why
                }
                dialog->show();
            });
        settingsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
        settingsAction->setMenuRole(QAction::NoRole);  // Keep in File menu, don't let macOS move it
        settingsAction->setToolTip(QStringLiteral("Open application settings"));
    }

    {
        // R-R3-21: each entry opens the Setup page that already does the
        // job. TX and mic profiles are one set (MicProfileManager), edited
        // on Setup > Audio > TX Profile; settings import and export live on
        // Setup > Diagnostics > Export / Import.
        QMenu* profilesMenu = fileMenu->addMenu(QStringLiteral("&Profiles"));
        QAction* txProfilesAction = profilesMenu->addAction(QStringLiteral("&TX Profiles..."));
        txProfilesAction->setToolTip(QStringLiteral("Open Setup > Audio > TX Profile"));
        connect(txProfilesAction, &QAction::triggered, this,
                [this]() { openSetupAtPage(QStringLiteral("TX Profile")); });
        QAction* micProfilesAction = profilesMenu->addAction(QStringLiteral("&Mic Profiles..."));
        micProfilesAction->setToolTip(QStringLiteral("Open Setup > Audio > TX Profile"));
        connect(micProfilesAction, &QAction::triggered, this,
                [this]() { openSetupAtPage(QStringLiteral("TX Profile")); });
        profilesMenu->addSeparator();
        QAction* importAction = profilesMenu->addAction(QStringLiteral("&Import..."));
        importAction->setToolTip(QStringLiteral("Open Setup > Diagnostics > Export / Import"));
        connect(importAction, &QAction::triggered, this,
                [this]() { openSetupAtPage(QStringLiteral("Export / Import")); });
        QAction* exportAction = profilesMenu->addAction(QStringLiteral("&Export..."));
        exportAction->setToolTip(QStringLiteral("Open Setup > Diagnostics > Export / Import"));
        connect(exportAction, &QAction::triggered, this,
                [this]() { openSetupAtPage(QStringLiteral("Export / Import")); });
    }

    fileMenu->addSeparator();

    fileMenu->addAction(QStringLiteral("&Quit"), QKeySequence(Qt::CTRL | Qt::Key_Q),
                        qApp, &QApplication::quit);

    // =========================================================================
    // RADIO
    // =========================================================================
    // ── Radio menu — 3Q-9: role-based items with state-aware enablement ──────
    QMenu* radioMenu = menuBar()->addMenu(QStringLiteral("&Radio"));

    // Connect (⌘K) — reconnects to the last-used radio. Greyed out when there
    // is no actionable target (currently connected, or no lastConnected MAC,
    // or the lastConnected MAC isn't in saved radios). Manage Radios is the
    // ONLY menu item whose job is to open the Connection Panel; Connect is
    // strictly a one-click reconnect.
    m_actConnect = radioMenu->addAction(QStringLiteral("&Connect"),
        QKeySequence(Qt::CTRL | Qt::Key_K),
        this, [this]() {
            if (m_connectionPickerManaged) {
                connectionRequestedByOperator();
                return;
            }
            if (!m_radioModel->ownsLocalDsp()) {
                connectToStation();
                return;
            }
            if (m_radioModel->isConnected()) {
                return;
            }
            AppSettings& s = AppSettings::instance();
            const QString lastMac = s.lastConnected();
            const auto saved = s.savedRadio(lastMac);
            if (!saved.has_value()) {
                return;  // enablement should have prevented this
            }
            // Unicast probe targeted at the saved IP — cleaner than a
            // broadcast scan + radioDiscovered listener: no leaked listeners
            // when the radio doesn't reply, and works across VPN tunnels
            // that drop broadcast traffic. Phase 3Q-2 wired probeAddress;
            // this menu item now uses it directly. (Earlier broadcast-listen
            // implementation leaked a connect-on-mac-match listener that
            // would auto-reconnect to LOCAL radio on later scans even after
            // the user explicitly disconnected — bug reported 2026-04-30.)
            RadioDiscovery* disc = m_radioModel->discovery();
            QMetaObject::Connection* connPtr = new QMetaObject::Connection;
            QMetaObject::Connection* failPtr = new QMetaObject::Connection;
            auto cleanup = [connPtr, failPtr]() {
                QObject::disconnect(*connPtr);
                delete connPtr;
                QObject::disconnect(*failPtr);
                delete failPtr;
            };
            *connPtr = connect(disc, &RadioDiscovery::radioDiscovered,
                this, [this, lastMac, cleanup](const RadioInfo& found) {
                    if (found.macAddress != lastMac) {
                        return;  // probe reply for a different radio — wait
                    }
                    if (m_radioModel->isConnected()) {
                        cleanup();
                        return;
                    }
                    cleanup();
                    RadioInfo ri = found;
                    HPSDRModel mo = AppSettings::instance().modelOverride(ri.macAddress);
                    if (mo != HPSDRModel::FIRST) {
                        ri.modelOverride = mo;
                    }
                    m_radioModel->connectToRadio(ri);
                });
            *failPtr = connect(disc, &RadioDiscovery::probeFailed,
                this, [cleanup, lastMac](const QHostAddress&, quint16) {
                    qCInfo(lcConnection) << "Connect: probe failed for"
                                         << lastMac;
                    cleanup();
                });
            disc->probeAddress(saved->info.address, saved->info.port);
        });
    m_actConnect->setToolTip(QStringLiteral(
        "Reconnect to the last-used radio (grayed out when there's nothing to reconnect to)"));

    // Disconnect (⌘⇧K) — disabled while disconnected.
    m_actDisconnect = radioMenu->addAction(QStringLiteral("&Disconnect"),
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K),
        this, [this]() {
            if (!m_radioModel->ownsLocalDsp()) {
                disconnectFromStation();
                return;
            }
            m_radioModel->disconnectFromRadio();
        });
    m_actDisconnect->setToolTip(QStringLiteral("Disconnect from the current radio"));

    radioMenu->addSeparator();

    // Manage Radios — always enabled; sole purpose is to open the panel
    // (which has its own ↻ Scan button for fresh broadcast discovery).
    // Parity Task 21 (R-IOS-18): in a remote window that is not managed by
    // the Connections picker, the Core's radios are on Setup > This Core
    // (the Connection panel is this computer's radios, which a remote
    // window does not run).
    m_actManageRadios = radioMenu->addAction(QStringLiteral("&Manage Radios…"),
        this, [this]() {
            if (!m_connectionPickerManaged && m_radioModel != nullptr
                && !m_radioModel->ownsLocalDsp()) {
                openThisCore(ThisCoreFocus::ChangeRadio);
                return;
            }
            showConnectionPanel();
        });
    m_actManageRadios->setToolTip(QStringLiteral(
        "Open the Connection Panel (radio list + ↻ Scan)"));
    // Parity Task 21 (R-IOS-18, B6.2; the operator's "Option A"): a remote
    // window's Core's radio, beside Connections. A window running its own
    // radio changes it in the Connection panel.
    m_actChangeCoreRadio = radioMenu->addAction(tr("Change radio…"), this, [this]() {
        openThisCore(ThisCoreFocus::ChangeRadio);
    });
    m_actEditCoreRadio = radioMenu->addAction(tr("Edit radio…"), this, [this]() {
        openThisCore(ThisCoreFocus::EditRadio);
    });
    m_actForgetCoreRadio = radioMenu->addAction(tr("Forget radio"), this, [this]() {
        openThisCore(ThisCoreFocus::ForgetRadio);
    });
    radioMenu->setToolTipsVisible(true);
    // GUI-M2 (fix wave): a window running its own radio shows these
    // disabled, with the reason pointing to the Connection panel, never
    // hidden (refreshCoreRadioActions).
    refreshCoreRadioActions();
    connect(radioMenu, &QMenu::aboutToShow, this, &MainWindow::refreshCoreRadioActions);

    radioMenu->addSeparator();

    {
        // R-R3-21: Setup > Hardware Config, on its Antenna / ALEX tab.
        QAction* antennaSetupAction = radioMenu->addAction(QStringLiteral("&Antenna Setup…"));
        antennaSetupAction->setToolTip(
            QStringLiteral("Open Setup > Hardware Config > Antenna / ALEX"));
        connect(antennaSetupAction, &QAction::triggered, this, &MainWindow::openAntennaSetup);
    }
    {
        QAction* transvertersAction = radioMenu->addAction(QStringLiteral("Trans&verters…"));
        transvertersAction->setEnabled(false);
        // R-R3-49: hidden until transverters are built.
        UnbuiltFeatures::hideUnlessBuilt(transvertersAction, UnbuiltFeature::Transverters);
    }

    radioMenu->addSeparator();

    // Protocol Info — disabled while disconnected; shows a QMessageBox with
    // the connected radio's protocol, firmware, and address info.
    m_actProtocolInfo = radioMenu->addAction(QStringLiteral("&Protocol Info"),
        this, [this]() {
            // Remote-daemon R2: isConnected() is storage-backed and true on
            // a remote client, whose connection() is permanently null, so
            // the second half of this test is load-bearing and not merely
            // defensive. A remote model never reaches it (the branch just
            // below), and applyRemoteRoleGating() only enables this QAction
            // once the Core has described its radio -- but QAction::trigger()
            // ignores enablement. The gate is not the guard.
            if (!m_radioModel->ownsLocalDsp()) {
                // R-R3-46: a remote window shows the Core's radio, from
                // what the Core reported. Same five lines as local mode;
                // anything an older Core does not report says so.
                showCoreRadioInfo();
                return;
            }
            if (!m_radioModel->isConnected() || !m_radioModel->connection()) {
                return;
            }
            RadioInfo info = m_radioModel->connection()->radioInfo();
            const QString proto =
                info.protocol == ProtocolVersion::Protocol2
                    ? QStringLiteral("P2") : QStringLiteral("P1");
            const QString msg =
                QStringLiteral("Radio:    %1\nProtocol: %2\nFirmware: %3\nMAC:      %4\nIP:       %5")
                    .arg(info.displayName())
                    .arg(proto)
                    .arg(info.firmwareVersion)
                    .arg(info.macAddress, info.address.toString());
            QMessageBox::information(this, QStringLiteral("Protocol Info"), msg);
        });
    m_actProtocolInfo->setToolTip(QStringLiteral(
        "Show connected radio protocol, firmware, and address details"));

    // Initial enablement (before any connectionStateChanged fires). Connect
    // is enabled only when there's a last-used radio in saved entries — i.e.
    // when "reconnect" actually has a target.
    {
        AppSettings& s = AppSettings::instance();
        const QString lastMac = s.lastConnected();
        const bool hasReconnectTarget =
            !lastMac.isEmpty() && s.savedRadio(lastMac).has_value();
        m_actConnect->setEnabled(m_connectionPickerManaged || hasReconnectTarget);
    }
    m_actDisconnect->setEnabled(false);
    m_actProtocolInfo->setEnabled(false);

    // =========================================================================
    // VIEW
    // =========================================================================
    QMenu* viewMenu = menuBar()->addMenu(QStringLiteral("&View"));

    // Phase 3F Sub-Epic D Task 14: live Pan Layout / Add slice / Float
    // entries. Replace the NYI submenu and disabled Add/Remove placeholders
    // with the working stack actions. The bottom-bar +PAN icon (a drawn
    // pixmap since Task B4, not a dropdown menu) is kept as the operator-
    // facing primary and shares showPanLayoutDialog() with the menu action
    // below it; these menu items are for operators who prefer the menubar
    // / keyboard shortcuts.
    {
        QAction* panLayoutAct = viewMenu->addAction(QStringLiteral("Pan &Layout…"));
        panLayoutAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));
        panLayoutAct->setToolTip(QStringLiteral(
            "Pick a panadapter layout template"));
        // Delegates to showPanLayoutDialog() rather than duplicating dialog
        // construction here: this call site used to build its own
        // PanLayoutDialog inline with no isConnected() guard, so with no
        // radio it opened a dialog gated on a stale maxSlices()-only
        // fallback (final-fix-wave finding 12). showPanLayoutDialog() is
        // already connection-gated and DDC-axis-correct (finding 2); the
        // bottom-bar +PAN icon uses the same method.
        connect(panLayoutAct, &QAction::triggered, this,
                &MainWindow::showPanLayoutDialog);
    }

    {
        QAction* addSliceAct = viewMenu->addAction(
            QStringLiteral("&Add slice on active pan"));
        addSliceAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
        // Fix wave 1 follow-up: no count here. The limit is known only once
        // a radio sizes the stream pool (and, in a remote window, it is the
        // Core's), and a refused add already names it (sliceCapReason).
        addSliceAct->setToolTip(QStringLiteral(
            "Create a new slice on the active panadapter"));
        connect(addSliceAct, &QAction::triggered, this, [this]() {
            if (m_panStack && m_radioModel) {
                addSliceForWindow(m_panStack->activePanId());
            }
        });
    }

    {
        QAction* floatAct = viewMenu->addAction(
            QStringLiteral("&Float active pan…"));
        floatAct->setToolTip(QStringLiteral(
            "Detach the active panadapter into its own floating window"));
        connect(floatAct, &QAction::triggered, this, [this]() {
            if (m_panStack) {
                m_panStack->floatPanadapter(m_panStack->activePanId());
            }
        });
    }

    viewMenu->addSeparator();

    // From AetherSDR MainWindow.cpp:4098-4130 [@0cd4559]
    {
        QMenu* bandPlanMenu = viewMenu->addMenu(QStringLiteral("&Band Plan"));

        const int savedBpSize = AppSettings::instance()
                                    .value(QStringLiteral("BandPlanFontSize"),
                                           QStringLiteral("6"))
                                    .toInt();

        QActionGroup* bpGroup = new QActionGroup(bandPlanMenu);
        bpGroup->setExclusive(true);
        struct BpOption { const char* label; int pt; };
        const BpOption bpModes[] = {
            { "&Off",    0  },
            { "&Small",  6  },
            { "&Medium", 10 },
            { "&Large",  12 },
            { "&Huge",   16 },
        };
        for (const auto& opt : bpModes) {
            QAction* a = bandPlanMenu->addAction(QString::fromUtf8(opt.label));
            a->setCheckable(true);
            a->setChecked(opt.pt == savedBpSize);
            bpGroup->addAction(a);
            const int pt = opt.pt;
            connect(a, &QAction::triggered, this, [this, pt]() {
                if (activeSpectrumWidget()) {
                    activeSpectrumWidget()->setBandPlanFontSize(pt);
                }
                AppSettings::instance().setValue(QStringLiteral("BandPlanFontSize"),
                                                 QString::number(pt));
            });
        }

        bandPlanMenu->addSeparator();

        QActionGroup* planGroup = new QActionGroup(bandPlanMenu);
        planGroup->setExclusive(true);
        const auto& mgr = m_radioModel->bandPlanManager();
        const QString activePlan = mgr.activePlanName();
        for (const QString& name : mgr.availablePlans()) {
            QAction* a = bandPlanMenu->addAction(name);
            a->setCheckable(true);
            a->setChecked(name == activePlan);
            planGroup->addAction(a);
            connect(a, &QAction::triggered, this, [this, name]() {
                m_radioModel->bandPlanManagerMutable().setActivePlan(name);
            });
        }
        // D79 (R-IOS-11, R-R3-49): the check follows the plan however it
        // changes (a remote window following the Core's plan included).
        connect(&m_radioModel->bandPlanManagerMutable(), &BandPlanManager::planChanged,
                planGroup, [this, planGroup]() {
                    const QString active = m_radioModel->bandPlanManager().activePlanName();
                    for (QAction* action : planGroup->actions()) {
                        action->setChecked(action->text() == active);
                    }
                });
    }

    {
        QMenu* displayModeMenu = viewMenu->addMenu(QStringLiteral("&Display Mode"));
        // R-R3-49: hidden until display modes are built.
        UnbuiltFeatures::hideUnlessBuilt(displayModeMenu->menuAction(),
                                         UnbuiltFeature::DisplayMode);
    }

    // Parity Task 31 (A11, R-R3-49): display duplex (DUP), JJ's ruling Q5
    // (2026-09-26): off by default as Thetis's _display_duplex
    // (console.cs:15390-15395 [v2.10.3.15]), saved with this window
    // (DisplayDuplex), as Thetis saves chkRX2SR with the console's check
    // boxes (console.cs:3278-3288 [v2.10.3.15], addControlState). The
    // container DUP button is the same setting.
    {
        m_displayDuplexSetting = MoxDisplayController::savedDisplayDuplex();
        m_displayDuplexAction = viewMenu->addAction(QStringLiteral("Display duplex (DUP)"));
        m_displayDuplexAction->setObjectName(QStringLiteral("actionDisplayDuplex"));
        m_displayDuplexAction->setCheckable(true);
        m_displayDuplexAction->setChecked(m_displayDuplexSetting);
        connect(m_displayDuplexAction, &QAction::toggled, this,
                [this](bool on) { setDisplayDuplexSetting(on); });
        applyDisplayDuplex();
    }

    {
        QMenu* uiScaleMenu = viewMenu->addMenu(QStringLiteral("&UI Scale"));
        QActionGroup* scaleGroup = new QActionGroup(this);
        scaleGroup->setExclusive(true);
        const struct { const char* label; bool isDefault; } scales[] = {
            { "&75%",  false },
            { "&100%", true  },
            { "&125%", false },
            { "&150%", false },
            { "&175%", false },
            { "&200%", false },
        };
        for (const auto& s : scales) {
            QAction* a = uiScaleMenu->addAction(QString::fromUtf8(s.label));
            a->setCheckable(true);
            a->setEnabled(false);
            if (s.isDefault) { a->setChecked(true); }
            scaleGroup->addAction(a);
        }
        // R-R3-49: hidden until UI scaling is built.
        UnbuiltFeatures::hideUnlessBuilt(uiScaleMenu->menuAction(), UnbuiltFeature::UiScale);
    }

    // Phase 23 "View > Network Applets" submenu removed: TCI Server and
    // TCI Clients are now driven by AppletVisibilityController, accessible
    // via Containers > Applets and the panel's ☰ banner menu. Two
    // independent controls (old direct-setVisible + new controller path)
    // would drift out of sync. CAT + MIDI greyed placeholders deferred to
    // their feature phases (3K-1 / 3K-3) — re-add at that time wired
    // through the controller.

    viewMenu->addSeparator();

    {
        QAction* minimalAction = viewMenu->addAction(QStringLiteral("&Minimal Mode"));
        minimalAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
        minimalAction->setEnabled(false);
        // R-R3-49: hidden until minimal mode is built.
        UnbuiltFeatures::hideUnlessBuilt(minimalAction, UnbuiltFeature::MinimalMode);
    }

    // 2026-05-26 KG4VCF perf instrumentation: toggle the in-spectrum
    // perf overlay (paint/gap/fft/overlay timings + audio underruns
    // + UDP drops + memory pressure).  Persisted via AppSettings
    // "ShowPerfOverlay" by SpectrumWidget::setShowPerfOverlay; the
    // menu item just exposes the toggle.
    {
        QAction* perfAction = viewMenu->addAction(
            QStringLiteral("&Performance Overlay"));
        perfAction->setCheckable(true);
        perfAction->setChecked(activeSpectrumWidget() && activeSpectrumWidget()->showPerfOverlay());
        perfAction->setToolTip(QStringLiteral(
            "Show paint/gap/fft/overlay timings + audio underruns + UDP drops"
            " + memory pressure in a corner of the spectrum panel."
            "  Useful for diagnosing jitter under system load."));
        connect(perfAction, &QAction::toggled, this, [this](bool on) {
            if (activeSpectrumWidget()) {
                activeSpectrumWidget()->setShowPerfOverlay(on);
            }
        });
    }

    viewMenu->addSeparator();

    {
        QAction* kbAction = viewMenu->addAction(QStringLiteral("&Keyboard Shortcuts..."));
        kbAction->setEnabled(false);
        // R-R3-49: hidden until keyboard shortcut editing is built.
        UnbuiltFeatures::hideUnlessBuilt(kbAction, UnbuiltFeature::Keyboard);
    }

    // =========================================================================
    // DSP
    // =========================================================================
    QMenu* dspMenu = menuBar()->addMenu(QStringLiteral("&DSP"));

    // ── NR submenu — full slot bank, mutual exclusion via QActionGroup ─────
    // Mirrors VfoWidget's NR bank. Every filter is always listed (R-R3-49,
    // Sub-epic C-1: never hidden); DFNR and MNR are disabled, with the
    // plain reason as the tooltip, while they cannot run (RadioModel::
    // nrCannotRunReason: the Core's word, mirrored, in a remote window).
    // BNR (NVIDIA) is not offered (operator, 2026-09-25); see nrMenuEntries.
    {
        QMenu* nrMenu = dspMenu->addMenu(QStringLiteral("&NR"));
        nrMenu->setToolTipsVisible(true);
        m_nrGroup = new QActionGroup(this);
        m_nrGroup->setExclusive(true);

        using Slot = NereusSDR::NrSlot;
        for (const auto& nr : nrMenuEntries()) {
            Slot slot = nr.second;
            QAction* a = nrMenu->addAction(nr.first,
                this, [this, slot]() {
                    SliceModel* slice = activeSliceForWindow();
                    if (!slice) { return; }
                    const QString refused = applyNrMenuChoice(slice, slot);
                    // Fix wave I3: a refused choice leaves the check on the
                    // reducer the receiver still runs.
                    if (slice->activeNr() != slot) {
                        for (QAction* action : m_nrGroup->actions()) {
                            QSignalBlocker blocker(action);
                            action->setChecked(action->data().toInt()
                                               == static_cast<int>(slice->activeNr()));
                        }
                    }
                    if (!refused.isEmpty()) {
                        showToast(refused, ToastSeverity::Warning, 3000);
                    }
                });
            a->setData(static_cast<int>(slot));
            a->setCheckable(true);
            m_nrGroup->addAction(a);
        }
        connect(nrMenu, &QMenu::aboutToShow, this, [this]() {
            const SliceModel* slice = activeSliceForWindow();
            for (QAction* action : m_nrGroup->actions()) {
                const NrSlot slot = static_cast<NrSlot>(action->data().toInt());
                QSignalBlocker blocker(action);
                action->setChecked(slice && slice->activeNr() == slot);
                const QString cannot = m_radioModel->nrCannotRunReason(slot);
                action->setEnabled(slice && cannot.isEmpty()
                                   && (slot != NrSlot::NNR || slice->nnrAvailable()));
                action->setToolTip(cannot.isEmpty() ? action->text().remove(QLatin1Char('&'))
                                                    : cannot);
            }
        });
        nrMenu->setToolTipsVisible(true);
    }

    // ── NB submenu — Off/NB/NB2 mutual exclusion ───────────────────────────
    // Maps to SliceModel::setNbMode(NbMode). Mirrors VfoWidget's cycling
    // NB button (Off → NB → NB2 → Off) but as discrete menu items.
    {
        QMenu* nbMenu = dspMenu->addMenu(QStringLiteral("N&B"));
        m_nbGroup = new QActionGroup(this);
        m_nbGroup->setExclusive(true);

        using Mode = NereusSDR::NbMode;
        const struct { const char* label; Mode mode; } nbModes[] = {
            { "&Off",  Mode::Off },
            { "&NB",   Mode::NB  },
            { "NB&2",  Mode::NB2 },
        };
        for (const auto& nb : nbModes) {
            Mode mode = nb.mode;
            QAction* a = nbMenu->addAction(QString::fromUtf8(nb.label),
                this, [this, mode]() {
                    SliceModel* slice = activeSliceForWindow();
                    if (slice) { slice->setNbMode(mode); }
                });
            a->setCheckable(true);
            m_nbGroup->addAction(a);
        }
    }

    // ── Single-toggle DSP actions ──────────────────────────────────────────
    // ANF now goes through SliceModel::anfEnabled (Phase 3F Sub-Epic J
    // Task 3) and is kept in sync below. SNB / APF / BIN go through
    // SliceModel and are synced too.
    {
        QAction* anfAction = dspMenu->addAction(QStringLiteral("&ANF"));
        m_anfAction = anfAction;
        anfAction->setCheckable(true);
        connect(anfAction, &QAction::toggled, this, [this](bool on) {
            // A control attached to no flag targets the active slice, which
            // is whichever flag the operator last clicked. Resolved at
            // invocation, not captured, so it follows focus.
            if (SliceModel* slice = activeSliceForWindow()) {
                slice->setAnfEnabled(on);
            }
        });

        // Reflect the active slice when focus moves, without re-emitting
        // toggled back into the handler above.
        connect(m_radioModel, &RadioModel::activeSliceChanged, this,
                [this, anfAction](int) {
            if (SliceModel* slice = activeSliceForWindow()) {
                QSignalBlocker block(anfAction);
                anfAction->setChecked(slice->anfEnabled());
            }
        });
    }

    m_snbAction = dspMenu->addAction(QStringLiteral("&SNB"));
    m_snbAction->setCheckable(true);
    connect(m_snbAction, &QAction::toggled, this, [this](bool on) {
        SliceModel* slice = activeSliceForWindow();
        if (slice) { slice->setSnbEnabled(on); }
    });

    m_apfAction = dspMenu->addAction(QStringLiteral("AP&F"));
    m_apfAction->setCheckable(true);
    connect(m_apfAction, &QAction::toggled, this, [this](bool on) {
        SliceModel* slice = activeSliceForWindow();
        if (slice) { slice->setApfEnabled(on); }
    });

    m_binAction = dspMenu->addAction(QStringLiteral("B&IN"));
    m_binAction->setCheckable(true);
    connect(m_binAction, &QAction::toggled, this, [this](bool on) {
        SliceModel* slice = activeSliceForWindow();
        if (slice) { slice->setBinauralEnabled(on); }
    });

    // TNF: enable or bypass every notch at once. Global rather than per-slice
    // because the notch list itself is global (design decision D1), which is
    // also how Thetis models it: TNFActive is a single flag despite the
    // per-rx command shape (console.cs:52317-52326 [v2.10.3.15], where GetMNF
    // is documented "mnf enabled globally").
    m_tnfAction = dspMenu->addAction(QStringLiteral("&TNF"));
    m_tnfAction->setCheckable(true);
    m_tnfAction->setShortcut(tnfToggleShortcut());
    m_tnfAction->setToolTip(
        QStringLiteral("Enable or bypass all tunable notch filters"));
    if (NotchModel* notches = m_radioModel->notchModel()) {
        m_tnfAction->setChecked(notches->globalEnabled());
        connect(m_tnfAction, &QAction::toggled,
                notches, &NotchModel::setGlobalEnabled);
        // The blocker is what keeps one operator gesture from reaching the
        // model twice: setChecked re-emits toggled, which would write the
        // model again.
        connect(notches, &NotchModel::globalEnabledChanged,
                m_tnfAction, [this](bool on) {
            QSignalBlocker block(m_tnfAction);
            m_tnfAction->setChecked(on);
        });
    }

    dspMenu->addSeparator();

    {
        // AGC submenu — checkable exclusive via QActionGroup.
        // AGCMode enum from WdspTypes.h: Off=0, Long=1, Slow=2, Med=3, Fast=4, Custom=5
        // From Thetis dsp.cs AGCMode — all 6 modes wired to SliceModel::setAgcMode().
        QMenu* agcMenu = dspMenu->addMenu(QStringLiteral("&AGC"));
        m_agcGroup = new QActionGroup(this);
        m_agcGroup->setExclusive(true);

        const struct { const char* label; AGCMode mode; } agcModes[] = {
            { "&Off",    AGCMode::Off    },
            { "&Long",   AGCMode::Long   },
            { "&Slow",   AGCMode::Slow   },
            { "&Med",    AGCMode::Med    },
            { "&Fast",   AGCMode::Fast   },
            { "&Custom", AGCMode::Custom },
        };
        for (const auto& agc : agcModes) {
            AGCMode agcMode = agc.mode;
            QAction* a = agcMenu->addAction(QString::fromUtf8(agc.label),
                this, [this, agcMode]() {
                    SliceModel* slice = activeSliceForWindow();
                    if (slice) { slice->setAgcMode(agcMode); }
                });
            a->setCheckable(true);
            m_agcGroup->addAction(a);
        }

        // Sync AGC checked state when SliceModel changes
        connect(m_radioModel, &RadioModel::sliceAdded, this, [this](int index) {
            if (index != 0) { return; }
            SliceModel* slice = m_radioModel->activeSlice();
            if (!slice) { return; }
            connect(slice, &SliceModel::agcModeChanged, this, [this](AGCMode mode) {
                QList<QAction*> acts = m_agcGroup->actions();
                const AGCMode agcOrder[] = {
                    AGCMode::Off, AGCMode::Long, AGCMode::Slow,
                    AGCMode::Med, AGCMode::Fast, AGCMode::Custom
                };
                for (int i = 0; i < acts.size() && i < 6; ++i) {
                    acts[i]->setChecked(agcOrder[i] == mode);
                }
            });
        });
    }

    // ── Sync NR / NB / SNB / APF / BIN checked state from slice 0 ──────────
    // Mirrors the AGC sync pattern above. Wired via sliceAdded so the
    // connection survives slice teardown/re-create. ANF is not synced here:
    // its check state follows activeSliceChanged instead (wired beside the
    // action's creation above), since it must track whichever slice is
    // active, not just slice 0.
    connect(m_radioModel, &RadioModel::sliceAdded, this, [this](int index) {
        if (index != 0) { return; }
        SliceModel* slice = m_radioModel->activeSlice();
        if (!slice) { return; }

        // Each action carries its stable enum; menu ordering is presentation only.
        auto syncNr = [this](NereusSDR::NrSlot slot) {
            for (QAction* action : m_nrGroup->actions()) {
                QSignalBlocker blocker(action);
                action->setChecked(action->data().toInt() == static_cast<int>(slot));
            }
        };
        syncNr(slice->activeNr());
        connect(slice, &SliceModel::activeNrChanged, this, syncNr);

        // NB submenu sync — three actions in m_nbGroup: Off, NB, NB2.
        const NereusSDR::NbMode nbOrder[] = {
            NereusSDR::NbMode::Off, NereusSDR::NbMode::NB, NereusSDR::NbMode::NB2,
        };
        auto syncNb = [this, nbOrder](NereusSDR::NbMode mode) {
            QList<QAction*> acts = m_nbGroup->actions();
            for (int i = 0; i < acts.size() && i < 3; ++i) {
                QSignalBlocker b(acts[i]);
                acts[i]->setChecked(nbOrder[i] == mode);
            }
        };
        syncNb(slice->nbMode());
        connect(slice, &SliceModel::nbModeChanged, this, syncNb);

        // Single-toggle initial sync.
        { QSignalBlocker b(m_snbAction); m_snbAction->setChecked(slice->snbEnabled()); }
        { QSignalBlocker b(m_apfAction); m_apfAction->setChecked(slice->apfEnabled()); }
        { QSignalBlocker b(m_binAction); m_binAction->setChecked(slice->binauralEnabled()); }

        connect(slice, &SliceModel::snbEnabledChanged, this, [this](bool v) {
            QSignalBlocker b(m_snbAction);
            m_snbAction->setChecked(v);
        });
        connect(slice, &SliceModel::apfEnabledChanged, this, [this](bool v) {
            QSignalBlocker b(m_apfAction);
            m_apfAction->setChecked(v);
        });
        connect(slice, &SliceModel::binauralEnabledChanged, this, [this](bool v) {
            QSignalBlocker b(m_binAction);
            m_binAction->setChecked(v);
        });
    });

    dspMenu->addSeparator();

    {
        QAction* eqAction = dspMenu->addAction(QStringLiteral("&Equalizer..."));
        eqAction->setEnabled(false);
        // R-R3-49: hidden until the receive equalizer is built.
        UnbuiltFeatures::hideUnlessBuilt(eqAction, UnbuiltFeature::Equalizer);
    }
    {
        // Phase 3M-4 Task 8: wire DSP > PureSignal... to the modeless dialog.
        // Both this entry and Tools > PureSignal... below open the same
        // singleton dialog (DSP for discoverability under the existing
        // DSP-feature menu, Tools per the per-task plan §8.4).
        m_actDspPureSignal = dspMenu->addAction(QStringLiteral("&PureSignal..."));
        m_actDspPureSignal->setToolTip(
            QStringLiteral("Open the PureSignal pre-distortion control dialog."));
        connect(m_actDspPureSignal, &QAction::triggered,
                this, &MainWindow::openPureSignalDialog);
    }
    {
        // R-R3-21: the same dialog as Tools > Diversity.
        QAction* divAction = dspMenu->addAction(QStringLiteral("&Diversity..."));
        divAction->setToolTip(QStringLiteral(
            "Open the Diversity dialog (Slice A: enable, phase, gain)."));
        connect(divAction, &QAction::triggered, this, &MainWindow::openDiversityDialog);
    }

    // =========================================================================
    // BAND
    // =========================================================================
    QMenu* bandMenu = menuBar()->addMenu(QStringLiteral("&Band"));

    {
        QMenu* hfMenu = bandMenu->addMenu(QStringLiteral("&HF"));
        // R-R3-21: each entry goes through the band buttons' path, so the
        // band's saved frequency, mode and filter come back (and a first
        // visit uses the band's seed). It used to set the listed frequency
        // directly, skipping the per-band memory. Entries are named by band
        // only: a band's entry restores its saved frequency, so a listed
        // frequency would not match where it lands.
        const struct { const char* label; Band band; } hfBands[] = {
            { "160m",             Band::Band160m },
            { "80m",              Band::Band80m  },
            { "60m",              Band::Band60m  },
            { "40m",              Band::Band40m  },
            { "30m",              Band::Band30m  },
            { "20m",              Band::Band20m  },
            { "17m",              Band::Band17m  },
            { "15m",              Band::Band15m  },
            { "12m",              Band::Band12m  },
            { "10m",              Band::Band10m  },
            { "6m",               Band::Band6m   },
            // R-IOS-26: 2 m is its own band, after 6 m in Thetis's HF band
            // group (MeterManager.cs GetBandGroupFromBand [v2.10.3.15]).
            { "2m",               Band::Band2m   },
        };
        for (const auto& entry : hfBands) {
            const Band band = entry.band;
            hfMenu->addAction(QString::fromUtf8(entry.label), this, [this, band]() {
                if (m_radioModel) { m_radioModel->onBandButtonClicked(band); }
            });
        }
    }

    {
        QMenu* vhfMenu = bandMenu->addMenu(QStringLiteral("&VHF"));
        // R-R3-49: hidden until transverters are built.
        UnbuiltFeatures::hideUnlessBuilt(vhfMenu->menuAction(), UnbuiltFeature::Transverters);
    }

    // R-R3-21: GEN and WWV go through the band buttons' path, like the HF
    // entries above, so the band's saved frequency, mode and filter come
    // back (a first visit uses the band's seed). GEN was an empty submenu
    // and WWV set 10.0 MHz directly, skipping the per-band memory.
    {
        QAction* genAction = bandMenu->addAction(QStringLiteral("&GEN"));
        genAction->setToolTip(QStringLiteral("General coverage"));
        connect(genAction, &QAction::triggered, this, [this]() {
            if (m_radioModel) { m_radioModel->onBandButtonClicked(Band::GEN); }
        });
    }

    bandMenu->addAction(QStringLiteral("&WWV"), this, [this]() {
        if (m_radioModel) { m_radioModel->onBandButtonClicked(Band::WWV); }
    });

    bandMenu->addSeparator();

    {
        QAction* bandStackAction = bandMenu->addAction(QStringLiteral("Band &Stacking..."));
        bandStackAction->setEnabled(false);
        // R-R3-49: hidden until band stacking is built.
        UnbuiltFeatures::hideUnlessBuilt(bandStackAction, UnbuiltFeature::BandStack);
    }

    // =========================================================================
    // MODE
    // =========================================================================
    QMenu* modeMenu = menuBar()->addMenu(QStringLiteral("&Mode"));

    // 12 Thetis-faithful modes + the NereusSDR-native RADE-U / RADE-L
    // entries (Phase 3R L3).  Display order: LSB, USB, DSB, CWL, CWU,
    // AM, SAM, FM, DIGL, DIGU, DRM, SPEC, RADE-U, RADE-L.  Maps to
    // DSPMode enum values from WdspTypes.h.
    // From Thetis dsp.cs DSPMode enum — enum values used directly, not indices.
    // RADE-U / RADE-L are NereusSDR-native entries (DSPMode::RADE_U = 12,
    // DSPMode::RADE_L = 13; not WDSP modes; routes the slice through
    // RadeChannel).  Like USB/LSB, RADE has upper/lower sideband
    // variants with mirrored 1700 Hz passbands.
    const struct { const char* label; DSPMode mode; } modes[] = {
        { "LSB",    DSPMode::LSB    },
        { "USB",    DSPMode::USB    },
        { "DSB",    DSPMode::DSB    },
        { "CWL",    DSPMode::CWL    },
        { "CWU",    DSPMode::CWU    },
        { "AM",     DSPMode::AM     },
        { "SAM",    DSPMode::SAM    },
        { "FM",     DSPMode::FM     },
        { "DIGL",   DSPMode::DIGL   },
        { "DIGU",   DSPMode::DIGU   },
        { "DRM",    DSPMode::DRM    },
        { "SPEC",   DSPMode::SPEC   },
        { "RADE-U", DSPMode::RADE_U },  // Phase 3R L3, NereusSDR-native upper
        { "RADE-L", DSPMode::RADE_L },  // Phase 3R L3, NereusSDR-native lower
    };

    m_modeActionGroup = new QActionGroup(this);
    m_modeActionGroup->setExclusive(true);

    for (int i = 0; i < 14; ++i) {
        DSPMode mode = modes[i].mode;
        QAction* act = modeMenu->addAction(QString::fromUtf8(modes[i].label),
                                           this, [this, mode]() {
            SliceModel* slice = activeSliceForWindow();
            if (slice) { slice->setDspMode(mode); }
        });
        act->setCheckable(true);
        m_modeActionGroup->addAction(act);
        m_modeActions[i] = act;
    }

    // Sync checked mode action when SliceModel reports a mode change.
    // Connection is deferred until slice 0 is available (sliceAdded signal).
    connect(m_radioModel, &RadioModel::sliceAdded, this, [this](int index) {
        if (index != 0) { return; }
        SliceModel* slice = m_radioModel->activeSlice();
        if (!slice) { return; }
        connect(slice, &SliceModel::dspModeChanged, this, [this](DSPMode mode) {
            const DSPMode displayOrder[] = {
                DSPMode::LSB, DSPMode::USB, DSPMode::DSB, DSPMode::CWL,
                DSPMode::CWU, DSPMode::AM,  DSPMode::SAM,  DSPMode::FM,
                DSPMode::DIGL, DSPMode::DIGU, DSPMode::DRM, DSPMode::SPEC,
                DSPMode::RADE_U,  // Phase 3R L3, index 12
                DSPMode::RADE_L,  // Phase 3R L3, index 13
            };
            for (int i = 0; i < 14; ++i) {
                if (m_modeActions[i]) {
                    m_modeActions[i]->setChecked(displayOrder[i] == mode);
                }
            }
        });
    });

    // Menu checks follow this window's active slice, including a station
    // selection that changes while the TX holder keeps global active fixed.
    connect(m_radioModel, &RadioModel::sliceAdded, this, [this](int index) {
        SliceModel* slice = m_radioModel->sliceById(index);
        if (!slice) { return; }
        auto refresh = [this] {
            if (desktopHosting()) { refreshActiveSlicePresentation(); }
        };
        connect(slice, &SliceModel::activeNrChanged, this, refresh);
        connect(slice, &SliceModel::nbModeChanged, this, refresh);
        connect(slice, &SliceModel::anfEnabledChanged, this, refresh);
        connect(slice, &SliceModel::snbEnabledChanged, this, refresh);
        connect(slice, &SliceModel::apfEnabledChanged, this, refresh);
        connect(slice, &SliceModel::binauralEnabledChanged, this, refresh);
        connect(slice, &SliceModel::agcModeChanged, this, refresh);
        connect(slice, &SliceModel::dspModeChanged, this, refresh);
        if (desktopHosting()) { refreshActiveSlicePresentation(); }
    });

    // =========================================================================
    // CONTAINERS
    // =========================================================================
    QMenu* containersMenu = menuBar()->addMenu(QStringLiteral("Contai&ners"));

    {
        // New Container: creates a floating container with a fresh MeterWidget,
        // then opens the settings dialog so the user can pick a preset or add items.
        // From Thetis setup.cs:24358 — btnAddRX1Container_Click → AddMeterContainer(1, false)
        QAction* newContAction = containersMenu->addAction(QStringLiteral("&New Container..."));
        connect(newContAction, &QAction::triggered, this, [this]() {
            if (!m_containerManager) { return; }

            ContainerWidget* c = m_containerManager->createContainer(1, DockMode::Floating);
            if (!c) { return; }
            c->setNotes(QStringLiteral("Meter"));

            // Give it a MeterWidget as content (replaces the default placeholder label)
            if (!m_containerManager->workspaceStore()) {
                MeterWidget* meter = new MeterWidget(); c->setContent(meter);
            }

            // Open settings dialog so user can configure it
            ContainerSettingsDialog dialog(c, this, m_containerManager);
            if (dialog.exec() == QDialog::Rejected) {
                // User cancelled — destroy the container
                m_containerManager->destroyContainer(c->id());
            }
        });
    }
    {
        // Phase 3G-6 block 6 commit 45: dynamic "Edit Container ▸"
        // submenu populated from ContainerManager::allContainers().
        // Replaces the old static "Container Settings..." action that
        // could only edit Container #0. Rebuilds on
        // containerAdded / containerRemoved / containerTitleChanged
        // signals so menu entries stay in sync with the live
        // container set.
        m_editContainerMenu = containersMenu->addMenu(
            QStringLiteral("&Edit Container"));
        rebuildEditContainerSubmenu();
        if (m_containerManager) {
            connect(m_containerManager, &ContainerManager::containerAdded,
                    this, [this](const QString&) { rebuildEditContainerSubmenu(); });
            connect(m_containerManager, &ContainerManager::containerRemoved,
                    this, [this](const QString&) { rebuildEditContainerSubmenu(); });
            connect(m_containerManager, &ContainerManager::containerTitleChanged,
                    this, [this](const QString&, const QString&) {
                rebuildEditContainerSubmenu();
            });
        }
    }
    {
        // Phase 3G-6 block 6 commit 46: Reset Default Layout — now
        // functional. Destroys every non-panel container and
        // rebuilds the submenu.
        QAction* resetAction = containersMenu->addAction(QStringLiteral("&Reset Default Layout"));
        connect(resetAction, &QAction::triggered, this,
                &MainWindow::resetDefaultLayout);
    }

    containersMenu->addSeparator();

    // ── Containers > Applets section ─────────────────────────────────────
    // Show/hide toggles for each currently-wired applet. Backed by
    // m_appletVis (AppletVisibilityController). Two-way sync with the
    // ☰ menu on AppletPanelWidget happens via the controller's
    // visibilityChanged signal.
    //
    // Predecessor: dead lambda was disabled in 25597df because its 7
    // entries were all ghost applets. The new section ships only
    // currently-wired applets. Add new entries here as additional
    // applets ship (default visible per design §5.2).
    if (m_appletVis) {
        // Section header. addSection is the idiomatic Qt API; falls back
        // gracefully on platforms where it renders as a plain label.
        containersMenu->addSection(QStringLiteral("Applets"));

        for (const QString& id : m_appletVis->registeredIds()) {
            QAction* act = containersMenu->addAction(
                m_appletVis->displayName(id));
            act->setCheckable(true);
            act->setChecked(m_appletVis->isVisible(id));
            // Grey out when the applet is currently unavailable (e.g.
            // Amp/Tuner when 4O3A is disabled). Check state still
            // reflects the user preference.
            act->setEnabled(m_appletVis->isAvailable(id));
            // User-visible tooltip — plain English, no source cites.
            act->setToolTip(QStringLiteral("Show or hide the %1 applet")
                            .arg(m_appletVis->displayName(id)));

            connect(act, &QAction::toggled, this, [this, id](bool checked) {
                if (m_appletVis) { m_appletVis->setVisible(id, checked); }
            });
            m_topMenuAppletActions.insert(id, act);
        }

        // Sync checkmark when the controller's state changes (e.g. via
        // the banner ☰ menu in Task 6). QSignalBlocker prevents
        // recursive toggle.
        connect(m_appletVis, &AppletVisibilityController::visibilityChanged,
                this, [this](const QString& id, bool visible) {
            if (auto* act = m_topMenuAppletActions.value(id, nullptr)) {
                QSignalBlocker block(act);
                act->setChecked(visible);
            }
        });

        // Grey/un-grey top-menu entries when an applet's availability
        // changes (e.g. 4O3A master toggle flipped in Setup).
        connect(m_appletVis, &AppletVisibilityController::availabilityChanged,
                this, [this](const QString& id, bool available) {
            if (auto* act = m_topMenuAppletActions.value(id, nullptr)) {
                act->setEnabled(available);
            }
        });
    }

    // =========================================================================
    // TOOLS
    // =========================================================================
    QMenu* toolsMenu = menuBar()->addMenu(QStringLiteral("&Tools"));

    // Phase 3J-2 H1: Spot Hub (DX cluster / RBN / POTA / WSJT-X / FreeDV /
    // PSK Reporter). Modeless singleton dialog; lazy-constructed in
    // openSpotHub() with all 7 clients + SpotModel + DxccColorProvider
    // injected from RadioModel.
    {
        QAction* spotHubAction = toolsMenu->addAction(QStringLiteral("Spot &Hub..."));
        spotHubAction->setObjectName(QStringLiteral("actSpotHub"));
        spotHubAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")));
        spotHubAction->setToolTip(QStringLiteral(
            "Open the Spot Hub dialog (DX cluster, RBN, POTA, WSJT-X, "
            "FreeDV Reporter, PSK Reporter, Spot Collector)."));
        connect(spotHubAction, &QAction::triggered, this, &MainWindow::openSpotHub);
    }

    // Phase 3J-2 H1: FreeDV Reporter live station map.
    // Modeless singleton dialog; lazy-constructed in openFreeDVReporter()
    // with FreeDVStationModel + FreeDVReporterClient from RadioModel.
    {
        QAction* fdvAction = toolsMenu->addAction(QStringLiteral("&FreeDV Reporter..."));
        fdvAction->setObjectName(QStringLiteral("actFreeDVReporter"));
        fdvAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+R")));
        fdvAction->setToolTip(QStringLiteral(
            "Open the FreeDV Reporter dialog (live stations on qso.freedv.org)."));
        connect(fdvAction, &QAction::triggered, this, &MainWindow::openFreeDVReporter);
    }

    toolsMenu->addSeparator();

    // TX Equalizer: modeless singleton dialog (Phase 3M-3a-i Batch 3 A.1).
    {
        QAction* txEqAction = toolsMenu->addAction(QStringLiteral("TX &Equalizer..."));
        m_actTxEqualizer = txEqAction;
        txEqAction->setObjectName(QStringLiteral("toolsTxEqualizer"));
        txEqAction->setToolTip(QStringLiteral(
            "Open the 10-band TX EQ dialog (preamp + 10 band gains + center frequencies)."));
        connect(txEqAction, &QAction::triggered, this, [this]() {
            // R-R3-49 (parity Task 4): opens in a remote window too; the
            // dialog greys itself with the reason while the Core cannot
            // take a change (TxEqDialog::setSettingsPermitted).
            TxEqDialog* dlg = TxEqDialog::instance(m_radioModel, this);
            dlg->show();
            dlg->raise();
            dlg->activateWindow();
        });
    }

    // Phase 3M-4 Task 8: PureSignal — modeless singleton dialog.
    // Same target as DSP > PureSignal... above; this entry per design doc
    // §4 #3 ("Tools > PureSignal..." for higher discoverability than the
    // DSP-buried path).
    {
        m_actPureSignal = toolsMenu->addAction(QStringLiteral("&PureSignal..."));
        m_actPureSignal->setToolTip(QStringLiteral(
            "Open the PureSignal pre-distortion control dialog."));
        connect(m_actPureSignal, &QAction::triggered,
                this, &MainWindow::openPureSignalDialog);
    }

    // Phase 3F Sub-Epic G T4: Diversity dialog (bench minimum).
    // Lazy-singleton: one dialog instance per RadioModel, kept alive
    // across close so the dialog's own state survives a re-open
    // (SliceModel persistence handles real settings round-trip in T2).
    {
        QAction* divDlgAct = toolsMenu->addAction(QStringLiteral("&Diversity..."));
        divDlgAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+D")));
        divDlgAct->setToolTip(QStringLiteral(
            "Open the Diversity dialog (Slice A: enable, phase, gain)."));
        connect(divDlgAct, &QAction::triggered, this, &MainWindow::openDiversityDialog);
    }

    toolsMenu->addSeparator();

    {
        QAction* cwxAction = toolsMenu->addAction(QStringLiteral("C&WX..."));
        cwxAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_X));
        cwxAction->setEnabled(false);
        // R-R3-49: hidden until CWX is built.
        UnbuiltFeatures::hideUnlessBuilt(cwxAction, UnbuiltFeature::Cwx);
    }
    {
        QAction* memAction = toolsMenu->addAction(QStringLiteral("&Memory Manager..."));
        memAction->setEnabled(false);
        // R-R3-49: hidden until memories are built.
        UnbuiltFeatures::hideUnlessBuilt(memAction, UnbuiltFeature::Memories);
    }
    {
        QAction* catAction = toolsMenu->addAction(QStringLiteral("&CAT Control..."));
        catAction->setEnabled(false);
        // R-R3-49: hidden until CAT is built.
        UnbuiltFeatures::hideUnlessBuilt(catAction, UnbuiltFeature::Cat);
    }
    {
        // Phase 23: TCI Server action — enabled, opens Setup → TCI Server.
        QAction* tciAction = toolsMenu->addAction(QStringLiteral("&TCI Server..."));
        tciAction->setToolTip(QStringLiteral("Open TCI Server Setup"));
        connect(tciAction, &QAction::triggered, this, &MainWindow::openTciSetupPage);
    }
    {
        // R-R3-21: Setup > Audio > VAX, where the VAX channels are set up.
        QAction* daxAction = toolsMenu->addAction(QStringLiteral("&VAX Audio..."));
        daxAction->setToolTip(QStringLiteral("Open Setup > Audio > VAX"));
        connect(daxAction, &QAction::triggered, this,
                [this]() { openSetupAtPage(QStringLiteral("VAX")); });
    }
    {
        QAction* midiAction = toolsMenu->addAction(QStringLiteral("&MIDI Mapping..."));
        midiAction->setEnabled(false);
        // R-R3-49: hidden until MIDI control is built.
        UnbuiltFeatures::hideUnlessBuilt(midiAction, UnbuiltFeature::Midi);
    }

    toolsMenu->addSeparator();

    {
        QAction* netDiagAction = toolsMenu->addAction(QStringLiteral("&Network Diagnostics..."));
        connect(netDiagAction, &QAction::triggered,
                this, &MainWindow::openNetworkDiagnostics);
    }
    toolsMenu->addAction(QStringLiteral("&Support Bundle..."), this,
                         &MainWindow::showSupportDialog);

    // Phase 3F closeout — operator-visible test entries for Sub-Epic E
    // consumer surfaces. Lets the user verify the toast and TX-bound
    // re-route dialog render correctly without needing to trigger a real
    // antenna conflict. Removed when full conflict-detection state machine
    // lands and the test surfaces become unnecessary.
    //
    // R-R3-21 / R-R3-25: in a remote session both would fake an antenna
    // event the Core never had, so applyRemoteRoleGating() gives them the
    // transmit gate, and each handler refuses the same way the TX
    // Equalizer entry's does. Local direct mode is unchanged.
    //
    // R-R3-21: developer builds only. A release build (no smoke-build tag;
    // see BuildIdentity.h) shows no test entries; m_actTestAntennaToast and
    // m_actTestTxBoundReRoute then stay null, which every user null-checks.
    if (!BuildIdentity::buildTag().isEmpty()) {
        toolsMenu->addSeparator();
        {
            QAction* testToastAct = toolsMenu->addAction(
                QStringLiteral("Test antenna switch &toast"));
            m_actTestAntennaToast = testToastAct;
            testToastAct->setObjectName(QStringLiteral("toolsTestAntennaSwitchToast"));
            testToastAct->setToolTip(testAntennaToastToolTip());
            connect(testToastAct, &QAction::triggered, this, [this]() {
                if (!transmitControlsPermitted()) { return; }
                if (m_radioModel) {
                    m_radioModel->emitAntennaAutoSwitched(
                        0, QStringLiteral("ANT1"), QStringLiteral("ANT2"));
                }
            });
        }
        {
            QAction* testReRouteAct = toolsMenu->addAction(
                QStringLiteral("Test TX-bound &re-route dialog"));
            m_actTestTxBoundReRoute = testReRouteAct;
            testReRouteAct->setObjectName(QStringLiteral("toolsTestTxBoundReRoute"));
            testReRouteAct->setToolTip(testTxBoundReRouteToolTip());
            connect(testReRouteAct, &QAction::triggered, this, [this]() {
                if (!transmitControlsPermitted()) { return; }
                if (m_radioModel) {
                    m_radioModel->requestTxBoundReRoute(
                        QStringLiteral("ANT2"), QStringLiteral("ANT1"));
                }
            });
        }
    }

    // =========================================================================
    // HELP
    // =========================================================================
    QMenu* helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));

    {
        QAction* gettingStartedAction = helpMenu->addAction(QStringLiteral("&Getting Started"));
        gettingStartedAction->setEnabled(false);
        // R-R3-49: hidden until the help pages are built.
        UnbuiltFeatures::hideUnlessBuilt(gettingStartedAction, UnbuiltFeature::Help);
    }
    {
        QAction* helpAction = helpMenu->addAction(QStringLiteral("&NereusSDR Help"));
        helpAction->setEnabled(false);
        UnbuiltFeatures::hideUnlessBuilt(helpAction, UnbuiltFeature::Help);
    }
    {
        QAction* dataModesAction = helpMenu->addAction(QStringLiteral("Understanding &Data Modes"));
        dataModesAction->setEnabled(false);
        UnbuiltFeatures::hideUnlessBuilt(dataModesAction, UnbuiltFeature::Help);
    }

    helpMenu->addSeparator();

    {
        // R-R3-21: the release notes the About dialog links to.
        QAction* whatsNewAction = helpMenu->addAction(QStringLiteral("What's &New"));
        whatsNewAction->setToolTip(QStringLiteral("Open the NereusSDR release notes"));
        connect(whatsNewAction, &QAction::triggered, this, []() {
            QDesktopServices::openUrl(AboutDialog::releaseNotesUrl());
        });
    }

    helpMenu->addSeparator();

#if defined(Q_OS_LINUX)
    // PipeWire / pactl diagnostic dialog — Linux-only feature.
    helpMenu->addAction(QStringLiteral("&Diagnose audio backend…"),
                        this, &MainWindow::showAudioDiagnoseDialog);

    helpMenu->addSeparator();
#endif

    helpMenu->addAction(QStringLiteral("&About NereusSDR"), this, [this]() {
        AboutDialog dlg(this);
        dlg.exec();
    });

    // Phase 3J-2 H1: Ctrl+Shift+X clears all rows in SpotModel. Mirrors the
    // "Clear All Spots" button on SpotHubDialog's Display tab so the user
    // can wipe stale spots without opening the dialog. Application-scoped
    // QShortcut so it fires regardless of which child widget has focus.
    // R-R3-21: it was Ctrl+Shift+K, which Radio > Disconnect owns; Qt fires
    // neither owner of an ambiguous chord. X for "clear", free in every
    // menu (tst_controls_that_work checks every shortcut is unique).
    {
        auto* clearSpotsShortcut = new QShortcut(
            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_X), this);
        clearSpotsShortcut->setObjectName(QStringLiteral("clearSpotsShortcut"));
        clearSpotsShortcut->setContext(Qt::ApplicationShortcut);
        connect(clearSpotsShortcut, &QShortcut::activated, this, [this]() {
            if (m_radioModel && m_radioModel->spotModel()) {
                m_radioModel->spotModel()->clear();
            }
        });
    }
}

// Reserved safety slot dim helper (design §4.5). Static so both
// buildStatusBar()'s construction-time state and setTxInhibited() (a
// separately-wired Task 17 slot outside buildStatusBar()) can drive the
// same badge through the same opacity-only state change -- a plain
// setVisible() no longer represents "inactive" once a badge lives in a
// permanently allocated slot.
void MainWindow::dimSafetyBadge(QWidget* w, bool active)
{
    auto* fx = qobject_cast<QGraphicsOpacityEffect*>(w->graphicsEffect());
    if (!fx) {
        fx = new QGraphicsOpacityEffect(w);
        w->setGraphicsEffect(fx);
    }
    fx->setOpacity(active ? 1.0 : 0.14);
}

// Task B4 (design §8.2): AetherSDR's connection gate on the +PAN affordance
// is a silent early return, which reads as a dead click. Ours dims the icon
// and names the reason in the tooltip BEFORE the click, so unavailability
// is visible ahead of time rather than discovered by clicking and getting
// nothing. Called at construction (buildStatusBar) and on every
// connectionStateChanged transition.
void MainWindow::updateAddPanButtonState()
{
    if (!m_addPanButton) { return; }
    const bool connected = m_radioModel && m_radioModel->isConnected();
    m_addPanButton->setEnabled(connected);
    auto* fx = qobject_cast<QGraphicsOpacityEffect*>(
        m_addPanButton->graphicsEffect());
    if (!fx) {
        fx = new QGraphicsOpacityEffect(m_addPanButton);
        m_addPanButton->setGraphicsEffect(fx);
    }
    fx->setOpacity(connected ? 1.0 : 0.35);
    m_addPanButton->setToolTip(connected
        ? tr("Change panadapter layout")
        : tr("Connect a radio to change pan layout"));
}

// ── TNF operator controls (design sections 7, 7.5, 10.2) ──────────────────
//
// Four pure statics behind the status-bar light, the DSP-menu chord and the
// rejected-add notice. Static because MainWindow boots WDSP, the audio engine
// and the discovery thread, so nothing that needs an instance can be tested.

QString MainWindow::tnfIndicatorStyleSheet(bool globalEnabled, int notchCount)
{
    // ON reads accent cyan, matching AetherSDR's status-bar TNF light
    // (MainWindow_Wiring.cpp:3306-3311 [@c6481cbf], #00b4d8 on / #404858
    // off).
    //
    // The OFF half diverges from upstream, deliberately. AetherSDR's notch
    // list mirrors radio state and defaults its global flag ON, so its off
    // state is a rare, transient thing. Ours ships OFF (maintainer decision
    // D-a, matching Thetis's unchecked chkTNF and WDSP's master run 0 at
    // RXA.c:87), which means the operator's very first notch does nothing
    // until they find this switch. A dim grey label sitting in a row of
    // three permanently dim NYI labels does not communicate that, so:
    //
    //   off, no notches  -> dim #404858 + struck through. Idle, matched to
    //                       the CWX / DVK / FDX siblings but visibly a
    //                       toggle rather than a stub.
    //   off, notches set -> amber + struck through. Notches exist and are
    //                       being bypassed; that is the D-a hazard and it
    //                       gets the palette's warning colour.
    //   on               -> accent cyan, no strike, whatever the count.
    const bool bypassing = (!globalEnabled && notchCount > 0);
    const QString color = globalEnabled
                              ? QString::fromLatin1(Style::kAccent)
                              : (bypassing ? QString::fromLatin1(Style::kAmberWarn)
                                           : QStringLiteral("#404858"));
    const QString decoration = globalEnabled ? QStringLiteral("none")
                                             : QStringLiteral("line-through");
    return QStringLiteral("QLabel { color: %1; font-weight: bold; "
                          "font-size: 11px; text-decoration: %2; }")
        .arg(color, decoration);
}

QString MainWindow::tnfIndicatorTooltip(int notchCount, bool globalEnabled)
{
    // Shape from AetherSDR buildTnfTooltip (MainWindowHelpers.cpp:233-247
    // [@c6481cbf]): name the feature, say how many notches exist, say the
    // click toggles them. Upstream renders an HTML table of every notch;
    // ours stays a single plain line because the Settings > DSP > MNF table
    // (design section 9) is where the per-notch list lives.
    if (notchCount <= 0) {
        return QStringLiteral("Tunable Notch Filter: no notches. "
                              "Click to toggle all notches.");
    }
    return QStringLiteral("Tunable Notch Filter: %1 notch%2, %3. "
                          "Click to toggle all notches.")
        .arg(notchCount)
        .arg(notchCount == 1 ? QString() : QStringLiteral("es"),
             globalEnabled ? QStringLiteral("enabled")
                           : QStringLiteral("bypassed"));
}

QKeySequence MainWindow::tnfToggleShortcut()
{
    // Design section 10.2: KeyboardSetupPages.cpp is a 100% NYI stub, no
    // ShortcutManager or registerAction exists in src/, and every shipped
    // shortcut is a plain QAction::setShortcut in this file. AetherSDR
    // registers "tnf_toggle" with an empty default sequence
    // (MainWindow_Shortcuts.cpp:1093 [@c6481cbf]) precisely because it HAS a
    // manager to bind it later; we ship a fixed chord instead. Building the
    // assignment subsystem is a separate epic and explicitly out of scope.
    //
    // Ctrl+Shift+N (maintainer decision D-f), consistent with the existing
    // Ctrl+Shift+S and Ctrl+Shift+R chords and verified unclaimed.
    return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N);
}

QString MainWindow::tnfAddRejectedNotice(const QString& reason)
{
    // NotchModel's reject reasons are already operator-legible sentences
    // ("A notch already exists within 10 Hz"), so this only names what was
    // refused. Without it a +TNF press inside the dedupe window is entirely
    // silent (plan correction 16).
    // A reason that is already a sentence keeps its own full stop.
    const bool sentence = reason.endsWith(QLatin1Char('.')) || reason.endsWith(QLatin1Char('!'))
        || reason.endsWith(QLatin1Char('?'));
    return sentence ? QStringLiteral("Notch not added: %1").arg(reason)
                    : QStringLiteral("Notch not added: %1.").arg(reason);
}

void MainWindow::buildStatusBar()
{
    // AetherSDR double-height status bar (46px fixed height, 3-section layout)
    QStatusBar* sb = statusBar();
    sb->setFixedHeight(46);
    sb->setSizeGripEnabled(false);
    sb->setStyleSheet(QStringLiteral(
        "QStatusBar { background: #0a0a14; border-top: 1px solid #203040; }"
        "QStatusBar::item { border: none; }"));

    // Wrapper widget for the full-width custom layout. Stored as a
    // member so resizeEvent can read its width for m_chromeBar->relayout().
    m_chromeBarWidget = new QWidget(sb);
    m_chromeBarWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QWidget* barWidget = m_chromeBarWidget;   // local alias keeps existing code below tidy
    QHBoxLayout* hbox = new QHBoxLayout(barWidget);
    hbox->setContentsMargins(6, 0, 6, 0);
    hbox->setSpacing(6);

    // Helper: styled separator dot
    auto makeSep = [&]() -> QLabel* {
        auto* sep = new QLabel(QStringLiteral(" · "), barWidget);
        sep->setStyleSheet(QStringLiteral("QLabel { color: #304050; font-size: 18px; }"));
        return sep;
    };

    // ── Left section ──────────────────────────────────────────────────────────

    // Band Stack: three grey circles (NYI — clickable placeholder). Added to
    // m_placeholderGroup below rather than hbox directly, so it folds
    // together with TNF/CWX/DVK/FDX at rung 10 (design §6) instead of
    // sitting at its own fixed early position.
    auto* bandStackLabel = new QLabel(barWidget);
    bandStackLabel->setFixedSize(10, 22);
    {
        QPixmap pm(10, 22);
        pm.fill(Qt::transparent);
        QPainter painter(&pm);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QColor(0x40, 0x48, 0x58));
        painter.setPen(Qt::NoPen);
        for (int i = 0; i < 3; ++i) {
            painter.drawEllipse(0, i * 7, 9, 6);
        }
        painter.end();
        bandStackLabel->setPixmap(pm);
    }
    bandStackLabel->setToolTip(QStringLiteral("Band stack"));
    bandStackLabel->setCursor(Qt::PointingHandCursor);
    bandStackLabel->setObjectName(QStringLiteral("statusBandStackDots"));

    // +PAN icon. From AetherSDR MainWindow.cpp:4368-4396 [@c6481cb]: a jagged
    // spectrum polyline with a plus in the upper right. An icon reads as a
    // control where a text pill reads as a label, and the trace says what
    // kind of thing it adds.
    auto* panBtn = new QLabel(barWidget);
    panBtn->setObjectName(QStringLiteral("addPanButton"));
    panBtn->setAccessibleName(tr("Add panadapter"));
    QPixmap pm(36, 28);
    {
        pm.fill(Qt::transparent);
        QPainter pp(&pm);
        pp.setRenderHint(QPainter::Antialiasing);
        const QColor stroke(255, 255, 255, 210);
        pp.setPen(QPen(stroke, 1.6));
        const QPointF pts[] = {
            { 0, 22}, { 1, 21}, { 2, 22}, { 3, 19}, { 4, 22},
            { 5, 21}, { 6, 18}, { 7, 12}, { 8, 17}, { 9, 22},
            {10, 21}, {11, 22}, {12, 16}, {13, 22},
            {14, 21}, {15, 19}, {16, 22},
            {17, 20}, {18, 12}, {19,  4}, {20, 11}, {21, 21},
            {22, 22}, {23, 21}, {24, 17}, {25, 22},
            {26, 21}, {27, 22}, {28, 18}, {29, 22}, {30, 22}
        };
        pp.drawPolyline(pts, sizeof(pts) / sizeof(pts[0]));
        pp.setPen(QPen(stroke, 2.2));
        pp.drawLine(30, 4, 30, 14);
        pp.drawLine(25, 9, 35, 9);
        pp.end();
    }
    panBtn->setPixmap(pm);
    panBtn->setCursor(Qt::PointingHandCursor);
    panBtn->installEventFilter(this);
    panBtn->setProperty("isAddPanButton", true);
    // Band-stack dots lead the bar, ahead of +PAN, as they did before the
    // bottom-banner epic. Folding them is rung 10's job; where they sit is
    // this layout's job, and the two are independent.
    hbox->addWidget(bandStackLabel);
    hbox->addWidget(panBtn);
    m_addPanButton = panBtn;
    m_bandStackLabel = bandStackLabel;
    updateAddPanButtonState();

    // Phase 3F Sub-Epic D Task 11: per-chain (ADC) BPF state indicators
    // in the bottom status bar. Two stacked labels per chain ("CH N"
    // header + reasonText body), wired to AlexController::bpfStateChanged
    // below so the body text + colour reflect the live per-ADC BPF
    // state. CH 0 is always shown; CH 1 is shown only when the
    // connected radio's BoardCapabilities reports rxFilterChainCount >= 2
    // (gated in the currentRadioChanged handler below).
    auto makeChainIndicator = [&](int adc) -> QWidget* {
        auto* w  = new QWidget(barWidget);
        auto* vl = new QVBoxLayout(w);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(0);

        auto* topLbl = new QLabel(QStringLiteral("CH %1").arg(adc), w);
        topLbl->setStyleSheet(
            QStringLiteral("color: %1; font-size: 10px; font-weight: bold;")
                .arg(Style::kTextScale));
        vl->addWidget(topLbl);

        auto* botLbl = new QLabel(QStringLiteral("idle"), w);
        botLbl->setObjectName(QStringLiteral("chainIndicator%1").arg(adc));
        botLbl->setStyleSheet(
            QStringLiteral("color: %1; font-size: 9px; font-weight: bold;")
                .arg(Style::kTextInactive));
        vl->addWidget(botLbl);

        return w;
    };

    auto* chain0Widget = makeChainIndicator(0);
    hbox->addWidget(chain0Widget);
    m_chain0IndicatorWidget = chain0Widget;
    auto* chain1Widget = makeChainIndicator(1);
    chain1Widget->setVisible(false);  // shown only on 2-ADC SKUs
    hbox->addWidget(chain1Widget);
    m_chain1IndicatorWidget = chain1Widget;

    // Panel toggle (☰) — wired to QSplitter right pane visibility
    auto* panelToggleLabel = new QLabel(QStringLiteral("☰"), barWidget);
    panelToggleLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #8aa8c0; font-weight: bold; font-size: 16px; }"));
    panelToggleLabel->setToolTip(QStringLiteral("Toggle container panel"));
    panelToggleLabel->setCursor(Qt::PointingHandCursor);
    hbox->addWidget(panelToggleLabel);

    // Wire ☰ click: toggle QSplitter right pane (widget index 1) visibility.
    // When hiding: save sizes so we can restore them. When showing: restore.
    connect(panelToggleLabel, &QLabel::linkActivated, this, [](const QString&){});
    // QLabel doesn't emit click directly — install event filter via lambda via
    // a helper QObject. Use mousePressEvent via event filter on the label.
    panelToggleLabel->installEventFilter(this);
    // We need to store the label pointer to recognise it in eventFilter.
    // Use a property to mark it.
    panelToggleLabel->setProperty("isPanelToggle", true);

    // TNF light. Click toggles every notch at once; colour and tooltip follow
    // NotchModel::globalEnabled.
    // From AetherSDR MainWindow.cpp:4422-4436 [@c6481cbf] (indicator label +
    // tooltip refresh on list changes) and MainWindow_Wiring.cpp:3306-3311
    // [@c6481cbf] (globalEnabledChanged drives the stylesheet).
    //
    // Not gated on isConnected the way AetherSDR's is: under design decision
    // D3 the notch list is persisted client-side operator state, not a mirror
    // of radio state, so it is meaningful before a radio is attached.
    m_tnfLabel = new QLabel(QStringLiteral("TNF"), barWidget);
    m_tnfLabel->setCursor(Qt::PointingHandCursor);
    m_tnfLabel->setProperty("isTnfToggle", true);
    m_tnfLabel->installEventFilter(this);
    // TNF is NOT part of m_placeholderGroup. It was an inert NYI label when
    // the bottom-banner epic classified it as fold-last chrome, but the
    // tunable-notch-filter work that landed on main (#313) made it a live
    // toggle that turns amber when notches exist and are being bypassed.
    // That is an operationally meaningful warning, so it is registered as
    // its own item and folds with live state, not with the stubs.
    hbox->addWidget(m_tnfLabel);

    if (NotchModel* notches = m_radioModel->notchModel()) {
        // Named slot, not a lambda: Qt::UniqueConnection is silently ignored
        // for lambda targets in Qt6, and every one of these five signals can
        // fire while the bar is being rebuilt.
        connect(notches, &NotchModel::globalEnabledChanged, this,
                &MainWindow::refreshTnfIndicator, Qt::UniqueConnection);
        connect(notches, &NotchModel::notchAdded, this,
                &MainWindow::refreshTnfIndicator, Qt::UniqueConnection);
        connect(notches, &NotchModel::notchRemoved, this,
                &MainWindow::refreshTnfIndicator, Qt::UniqueConnection);
        connect(notches, &NotchModel::notchesReset, this,
                &MainWindow::refreshTnfIndicator, Qt::UniqueConnection);
        connect(notches, &NotchModel::notchAddRejected, this,
                &MainWindow::onNotchAddRejected, Qt::UniqueConnection);
        connect(notches, &NotchModel::notchRequestRefused, this,
                &MainWindow::onNotchRequestRefused, Qt::UniqueConnection);
        // Seed from whatever restoreFromSettings already loaded.
        refreshTnfIndicator();
    }

    // CWX
    auto* cwxLabel = new QLabel(QStringLiteral("CWX"), barWidget);
    cwxLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #404858; font-weight: bold; font-size: 11px; }"));
    cwxLabel->setToolTip(QStringLiteral("CW keyer"));
    cwxLabel->setCursor(Qt::PointingHandCursor);
    cwxLabel->setObjectName(QStringLiteral("statusCwxLabel"));

    // DVK
    auto* dvkLabel = new QLabel(QStringLiteral("DVK"), barWidget);
    dvkLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #404858; font-weight: bold; font-size: 11px; }"));
    dvkLabel->setToolTip(QStringLiteral("Digital voice keyer"));
    dvkLabel->setCursor(Qt::PointingHandCursor);
    dvkLabel->setObjectName(QStringLiteral("statusDvkLabel"));

    // FDX
    auto* fdxLabel = new QLabel(QStringLiteral("FDX"), barWidget);
    fdxLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #404858; font-weight: bold; font-size: 11px; }"));
    fdxLabel->setToolTip(QStringLiteral("Full duplex"));
    fdxLabel->setCursor(Qt::PointingHandCursor);
    fdxLabel->setObjectName(QStringLiteral("statusFdxLabel"));
    // R-R3-49: each of the three is hidden until its feature is built. The
    // group folds as one; it is taken off the bar below when none is left.
    UnbuiltFeatures::hideUnlessBuilt(cwxLabel, UnbuiltFeature::Cwx);
    UnbuiltFeatures::hideUnlessBuilt(dvkLabel, UnbuiltFeature::Voice);
    UnbuiltFeatures::hideUnlessBuilt(fdxLabel, UnbuiltFeature::Fdx);

    // Rung 10, last resort (design §6). Grouped so the ladder folds them as
    // one unit rather than dribbling them out one label at a time. Each
    // label is reparented here; the group owns them.
    //
    // Two members that used to be here are deliberately NOT:
    //
    //   bandStackLabel  -- it leads the bar, ahead of +PAN, and moving it
    //   into this group silently reordered the left section. Restored to
    //   its original position below; it still folds at rung 10 with the
    //   rest of the stubs, because rung governs visibility and the layout
    //   governs position, which are independent.
    //
    //   m_tnfLabel      -- no longer a stub. The tunable-notch-filter work
    //   on main (#313) made it a live toggle that turns amber when notches
    //   exist and are being bypassed, so it folds later than the stubs.
    m_placeholderGroup = new QWidget(barWidget);
    auto* phRow = new QHBoxLayout(m_placeholderGroup);
    phRow->setContentsMargins(0, 0, 0, 0);
    phRow->setSpacing(6);
    phRow->addWidget(cwxLabel);
    phRow->addWidget(dvkLabel);
    phRow->addWidget(fdxLabel);
    hbox->addWidget(m_placeholderGroup);

    // Trailing separator, paired with m_placeholderGroup so it folds
    // alongside the group (design §6) instead of dangling on its own.
    m_placeholderSep = makeSep();
    hbox->addWidget(m_placeholderSep);

    // Design §4.1: the old left-section model+firmware pair
    // (radioInfoWidget / m_radioModelLabel / m_radioFwLabel) is retired.
    // It had no click affordance and sat in the banner's unprotected left
    // section, so its width changes were what shoved its neighbours.
    // Radio identity now renders once, on StationBlock's second row
    // (Task A4's setHardwareLine), wired from onConnectionStateChanged()
    // below. m_connStatusLabel's legacy alias is retired with it — grep
    // confirms nothing else in the tree reads it.

    // ── Phase 3Q Sub-PR-6 (F.1): RxDashboard ────────────────────────────────
    // Replaces the Phase 3Q-7 verbose connection-info strip (those fields now
    // live in the segment tooltip / NetworkDiagnosticsDialog).
    // Follows the ACTIVE slice (Task A5's rebindDashboard lambda, wired
    // below to RadioModel::sliceAdded / activeSliceChanged) rather than a
    // fixed slice(0); no slice exists yet at construction time so there is
    // nothing to bind here. When disconnected the badges show placeholder
    // "—" until the slice receives live values from the radio.
    m_rxDashboard = new RxDashboard(barWidget);
    hbox->addWidget(m_rxDashboard);
    // Slice control plan Task 13: the picker opens the chooser.
    connect(m_rxDashboard, &RxDashboard::chooserRequested, this, [this]() {
        if (m_sliceChooser && m_sliceChooser->isVisible()) {
            m_sliceChooser->hide();
        } else {
            openSliceChooser();
        }
    });
    // R-R3-21: a badge click opens the VFO flag tab holding that setting,
    // on the flag of the slice the dashboard describes.
    connect(m_rxDashboard, &RxDashboard::badgeClicked, this,
            [this](RxDashboard::Badge badge) {
        SliceModel* slice = m_rxDashboard ? m_rxDashboard->slice() : nullptr;
        VfoWidget* flag = slice ? m_vfoWidgetsBySlice.value(slice->sliceIndex()) : nullptr;
        if (!flag) { return; }
        VfoWidget::Tab tab = VfoWidget::Tab::Mode;
        switch (badge) {
        case RxDashboard::Badge::Mode:
        case RxDashboard::Badge::Filter:  tab = VfoWidget::Tab::Mode;  break;
        case RxDashboard::Badge::Agc:
        case RxDashboard::Badge::Squelch: tab = VfoWidget::Tab::Audio; break;
        case RxDashboard::Badge::Nr:
        case RxDashboard::Badge::Nb:
        case RxDashboard::Badge::Apf:     tab = VfoWidget::Tab::Dsp;   break;
        }
        flag->showTab(tab);
        flag->raise();
    });

    // ── Phase 3M-4 Task 10: PSA bottom-banner pair (FB + PS) ──────────────────
    // Source-first port of Thetis ucInfoBar.cs:820-1098 [v2.10.3.13].
    // The widget auto-wires to RadioModel's PureSignal coordinator and
    // MoxController on construction; click signals route back to
    // PureSignal::setInvertRedBlue / setHideFeedback below.
    //
    // Phase 3M-4 bench-fix: visibility is gated on
    //   caps.hasPureSignal && pureSignal->isAutoCalEnabled()
    // (NereusSDR-specific UX: hide the banner unless the user has
    // explicitly armed PS-A; reduces clutter for non-PS workflows on
    // PS-capable boards).  updatePsaIndicatorVisibility() centralises the
    // condition; called from autoCalEnabledChanged + connection-state +
    // pureSignalCoordinatorReady (late-bind seam, Task 13).
    m_psaIndicator = new PsaIndicatorWidget(m_radioModel, barWidget);
    m_psaIndicator->setVisible(false);
    connect(m_psaIndicator, &PsaIndicatorWidget::invertRedBlueRequested, this, [this]() {
        auto& settings = AppSettings::instance();
        const bool inverted = settings.value("InvertRedBluePsa", "False").toString() != "True";
        settings.setValue("InvertRedBluePsa", inverted ? "True" : "False");
        m_psaIndicator->setInvertRedBlue(inverted);
        if (PureSignal* ps = m_radioModel->pureSignal()) {
            ps->setInvertRedBlue(inverted);
        }
        for (SetupDialog* dialog : findChildren<SetupDialog*>()) {
            dialog->reloadFeedbackPreferences();
        }
    });
    connect(m_psaIndicator, &PsaIndicatorWidget::hideFeedbackToggleRequested, this, [this]() {
        auto& settings = AppSettings::instance();
        const bool hidden = settings.value("HideFeedbackLevel", "False").toString() != "True";
        settings.setValue("HideFeedbackLevel", hidden ? "True" : "False");
        m_psaIndicator->setHideFeedback(hidden);
        if (PureSignal* ps = m_radioModel->pureSignal()) {
            ps->setHideFeedback(hidden);
        }
        for (SetupDialog* dialog : findChildren<SetupDialog*>()) {
            dialog->reloadFeedbackPreferences();
        }
    });
    connect(m_radioModel->pureSignalSettings(), &PureSignalSettings::autoCalEnabledChanged,
            this, &MainWindow::updatePsaIndicatorVisibility);
    connect(m_radioModel->pureSignalFacade(), &PureSignalSessionFacade::statusChanged,
            this, &MainWindow::updatePsaIndicatorVisibility);
    hbox->addWidget(m_psaIndicator);

    // ── Stretch ───────────────────────────────────────────────────────────────
    hbox->addStretch(1);

    // ── Center section: STATION — radio-name anchor (Sub-PR-7 G.1) ───────────
    // The old cyan "STATION: NereusSDR" box is replaced by a StationBlock that
    // shows the connected radio's name. Click → opens ConnectionPanel. Right-
    // click → Disconnect / Edit radio… / Forget radio. Disconnected appearance:
    // dashed-red border + italic "Click to connect" placeholder.
    // The StationCallsign AppSettings key is preserved on disk for a potential
    // future operator-callsign surface; it is no longer shown in status chrome.
    m_stationBlock = new StationBlock(barWidget);
    connect(m_stationBlock, &StationBlock::clicked,
            this, &MainWindow::connectionRequestedByOperator);
    connect(m_stationBlock, &StationBlock::contextMenuRequested,
            this, &MainWindow::showStationContextMenu);
    // Update the block's name on connection state changes.
    connect(m_radioModel, &RadioModel::currentRadioChanged, this,
            [this](const NereusSDR::RadioInfo& info) {
        if (!m_radioModel->ownsLocalDsp()) {
            refreshRemoteConnectionUi();
            return;
        }
        const bool connected =
            (m_radioModel->connectionState() == ConnectionState::Connected);
        m_stationBlock->setRadioName(connected ? info.name : QString());
        // setRadioName(QString()) also clears the hardware row
        // (StationBlock.cpp:57-59), so sizeHint may have changed either
        // way. Report it (final-fix-wave finding 3) so the fold budget
        // does not keep charging the connected width after this label
        // shrinks back to "Click to connect".
        if (m_chromeBar && m_chromeBarWidget) {
            m_chromeBar->setNaturalWidth(
                m_stationBlock, m_stationBlock->sizeHint().width());
            m_chromeBar->relayout(m_chromeBarWidget->width());
        }
    });
    connect(m_radioModel, &RadioModel::connectionStateChanged, this,
            [this](ConnectionState s) {
        if (!m_radioModel->ownsLocalDsp()) {
            refreshRemoteConnectionUi();
            return;
        }
        if (s != ConnectionState::Connected) {
            m_stationBlock->setRadioName(QString());
            if (m_chromeBar && m_chromeBarWidget) {
                m_chromeBar->setNaturalWidth(
                    m_stationBlock, m_stationBlock->sizeHint().width());
                m_chromeBar->relayout(m_chromeBarWidget->width());
            }
        }
    });

    // ── ADC Overload alarm: lives in the reserved safety group ─────────────
    // Earlier revisions of the Phase 3Q chrome work parked the alarm
    // between the dashboard and STATION block. That violated layout-
    // stability rule §278.4 ("STATION sits between two flex:1 spacers.
    // Activity in the middle or right sections never moves it.")
    // because the label's text width grew when overload fired and
    // pushed STATION sideways. The alarm is now an AdcOverloadBadge built
    // further down as one of the four permanently-allocated 50 px safety
    // slots (design doc §4.5: INH / PA / OVL / TX) -- an inactive slot
    // dims rather than collapsing, so its appearance changes opacity, not
    // width, and nothing else on the bar moves when it lights up.

    hbox->addWidget(m_stationBlock);

    // ── Stretch ───────────────────────────────────────────────────────────────
    hbox->addStretch(1);

    // ── Right section: indicators ────────────────────────────────────────────

    // Helper lambda: create a stacked indicator pair (top label + bottom label)
    // Returns the widget; sets topLbl/botLbl via out-params for wiring.
    auto makeIndicator = [&](const QString& top, const QString& bottom,
                              QLabel** outTop = nullptr, QLabel** outBot = nullptr) -> QWidget* {
        QWidget* w = new QWidget(barWidget);
        w->setMinimumWidth(60);
        QVBoxLayout* vl = new QVBoxLayout(w);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(0);
        auto* topLbl = new QLabel(top, w);
        topLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: #607080; font-size: 11px; }"));
        auto* botLbl = new QLabel(bottom, w);
        botLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: #404858; font-size: 11px; }"));
        vl->addWidget(topLbl);
        vl->addWidget(botLbl);
        if (outTop) { *outTop = topLbl; }
        if (outBot) { *outBot = botLbl; }
        return w;
    };

    // CAT Serial — NYI until Phase 3K; kept as static indicator, no live signal
    m_catIndicator = makeIndicator(QStringLiteral("CAT"), QStringLiteral("Off"));
    m_catIndicator->setObjectName(QStringLiteral("statusCatIndicator"));
    hbox->addWidget(m_catIndicator);
    m_catSep = makeSep();
    hbox->addWidget(m_catSep);

    // TCI — Phase 23: capture bottom label for updateTciIndicator() + install
    // event filter for click-to-Setup navigation.
    m_tciIndicator = makeIndicator(QStringLiteral("TCI"), QStringLiteral("Off"),
                                   nullptr, &m_tciIndicatorBotLabel);
    m_tciIndicator->installEventFilter(this);
    hbox->addWidget(m_tciIndicator);
    m_tciSep = makeSep();
    hbox->addWidget(m_tciSep);

    // ── System tile: PA telemetry + CPU, merged (design §4.3) ────────────
    // Earlier revisions showed only a single "PSU" widget driven by the
    // supply_volts (AIN6) channel.  Source-first audit against Thetis
    // [v2.10.3.13] proved that channel is never displayed in Thetis —
    // computeHermesDCVoltage() exists but has zero callers, and the only
    // voltage status indicator (toolStripStatusLabel_Volts) reads
    // _MKIIPAVolts which is convertToVolts(getUserADC0()) — i.e. the PA
    // drain voltage on AIN3.  On a G2 / 8000D / 7000DLE the PA drain IS
    // the supply voltage minus a small drop, so this single number
    // covers what the user wants to know.
    //
    // The HL2 fork (mi0bot) extends this slot for HermesLite by reusing
    // the volts label to show FPGA on-die temperature (HL2 has no PA
    // volts ADC, but does carry a temperature ADC value in the C&C
    // exciter_power AIN5 field).  See mi0bot console.cs:26758-26762
    // [v2.10.3.13-beta2 @c26a8a4]:
    //   if (HardwareSpecific.Model == HPSDRModel.HERMESLITE)
    //   {
    //       toolStripStatusLabel_Volts.Text = String.Format("{0:#0.0}C", _MKIIHL2Temp);
    //       ...
    //   }
    //
    // Bottom-banner cleanup Task A3 merged the 2-row PA stack (PA-V over
    // PA-T, mutually exclusive per board) with the standalone CPU
    // MetricLabel into one 2-row SystemTile: row one carries whichever PA
    // reading(s) the board publishes (both share row one when a board
    // publishes both, rather than evicting CPU per design §4.3), row two
    // is always CPU. The tile itself never hides — a board with neither
    // PA reading still shows CPU-only, matching design §4.3's degenerate
    // case ("shows a CPU-only tile, not an empty one").
    m_systemTile = new SystemTile(barWidget);
    hbox->addWidget(m_systemTile);
    m_systemTileSep = makeSep();
    hbox->addWidget(m_systemTileSep);

    // Phase 3P-II Task 21: TGXL presence chip. Registered with m_chromeBar
    // at rung 2 (design §6), so it folds under width pressure, but
    // presence is not a fold concept -- it is reported to the controller
    // via setItemAvailable, straight from the signal that changes it, per
    // ChromeBarController::setItemAvailable's own doc comment. Hidden
    // (available=false) until TunerModel::presenceChanged fires true;
    // text reflects operate/bypass/standby state via stateChanged.
    m_tgxlChip = new QLabel(QStringLiteral("TGXL"), barWidget);
    m_tgxlChip->setStyleSheet(QStringLiteral(
        "QLabel { background:#1a3a5a; border:1px solid #205070; "
        "padding:1px 8px; border-radius:3px; color:#88e0ff; }"));
    m_tgxlChip->setVisible(false);
    hbox->addWidget(m_tgxlChip);

    connect(m_radioModel->tunerModel(), &TunerModel::presenceChanged,
            this, [this](bool present) {
        if (!m_chromeBar || !m_chromeBarWidget) { return; }
        m_chromeBar->setItemAvailable(m_tgxlChip, present);
        m_chromeBar->relayout(m_chromeBarWidget->width());
    });
    connect(m_radioModel->tunerModel(), &TunerModel::stateChanged,
            this, [this]() {
        TunerModel* t = m_radioModel->tunerModel();
        QString s = t->isOperate()
                    ? (t->isBypass() ? QStringLiteral("BYPS")
                                     : QStringLiteral("OPER"))
                    : QStringLiteral("SBY");
        m_tgxlChip->setText(QStringLiteral("TGXL ") + s);
        // TGXL / TGXL OPER / TGXL BYPS / TGXL SBY are different widths
        // (Task A8 fix round 1 finding 4); report the new one.
        if (m_chromeBar && m_chromeBarWidget) {
            m_chromeBar->setNaturalWidth(m_tgxlChip,
                                         m_tgxlChip->sizeHint().width());
            m_chromeBar->relayout(m_chromeBarWidget->width());
        }
    });

    // Helper: SystemTile's content just changed width (a reading gained or
    // lost digits, a row appeared/disappeared). Report the new width to
    // m_chromeBar and let it re-decide — content-change sites call
    // setNaturalWidth then relayout() instead of the old force-refresh
    // drop-priority pattern (design §5.2).
    auto refreshChromeBarForSystemTile = [this]() {
        if (!m_chromeBar || !m_chromeBarWidget || !m_systemTile) { return; }
        m_chromeBar->setNaturalWidth(m_systemTile,
                                     m_systemTile->sizeHint().width());
        m_chromeBar->relayout(m_chromeBarWidget->width());
    };

    // R-R3-32 / R-R3-46 (parity Task 6): the PA row reads the one PA
    // reading source, RadioModel::paReadings(): this window's own radio, or
    // in a remote window the Core's (station telemetry version 4). A reading
    // that is absent (none on this board, not reported yet, disconnected,
    // or the Core's telemetry out of date) clears its half of the row; it
    // is never shown as 0.
    //
    // 2026-05-25 KG4VCF G2E bench finding: ANAN-G2E (HermesC10) firmware
    // leaves user_adc0 (AIN3 / status bytes 53-54) dark (0.1 V against an
    // actual 13.4 V supply), so the G2E row shows supply_volts (AIN6 /
    // bytes 45-46) labelled "PSU"; on the other MKII boards user_adc0 IS
    // the PA drain sense, which is what the "PA" label means
    // (RadioModel::paRowVolts). The choice reads the model at each update,
    // when it is settled.
    //
    // 2026-08-03 KG4VCF G2E bench finding: status frames can be parsed
    // before RadioModel reaches Connected, and the connection suppresses
    // re-emitting an unchanged value, so a steady supply would never reach
    // a listener bound late. paReadings() reads the connection's cached
    // values, and RadioModel re-announces them on every connection state
    // change, so the first reading is never missed.
    auto refreshPaRow = [this, refreshChromeBarForSystemTile]() {
        // R-R3-32: a remote window's readings are the Core's, and say so.
        m_systemTile->setPaSourceNote(m_radioModel->paReadingsFromCore()
                                          ? tr("From the Core") : QString());
        const RadioModel::PaRowVolts row = m_radioModel->paRowVolts();
        // Task 3.6: ANAN-8000DLE user preference gate. For ANAN-8000D
        // radios, consult the "Show volts/amps in title bar" AppSettings key
        // (default true). Other MKII-class boards (7000DLE, AnvelinaPro3)
        // have no such checkbox, so the gate is always open.
        const bool is8000D = (m_radioModel->hardwareProfile().model == HPSDRModel::ANAN8000D);
        const bool showVolts = !is8000D ||
            AppSettings::instance().value(
                QStringLiteral("HardwareAnan8000DleShowVoltsAmps"),
                QStringLiteral("True")).toString() == QStringLiteral("True");
        if (row.volts && showVolts) {
            m_systemTile->setPaLabel(row.supply ? QStringLiteral("PSU") : QStringLiteral("PA"));
            m_systemTile->setPaVolts(*row.volts);
        } else {
            m_systemTile->clearPaVolts();
        }
        // PA temperature (HL2 publishes it via the handlePaTelemetry HL2
        // branch). The label formatting respects
        // PaTempUnitNotifier::currentUnit().
        const RadioModel::PaReadings readings = m_radioModel->paReadings();
        if (readings.paTemperatureCelsius) {
            m_systemTile->setPaTempCelsius(*readings.paTemperatureCelsius);
        } else {
            m_systemTile->clearPaTemp();
        }
        refreshChromeBarForSystemTile();
    };
    connect(m_radioModel, &RadioModel::paReadingsChanged, this, refreshPaRow);
    refreshPaRow();

    // Live re-format on °C / °F toggle without waiting for the next
    // telemetry sample. There is no toggle(); flip explicitly, matching
    // the SystemTile::paTempClicked handler just below.
    connect(&PaTempUnitNotifier::instance(),
            &PaTempUnitNotifier::unitChanged, this,
            [this, refreshChromeBarForSystemTile](PaTempUnit) {
        m_systemTile->refreshPaRow();
        refreshChromeBarForSystemTile();
    });
    connect(m_systemTile, &SystemTile::paTempClicked, this, []() {
        const PaTempUnit cur = PaTempUnitNotifier::currentUnit();
        PaTempUnitNotifier::setUnit(cur == PaTempUnit::Celsius
                                        ? PaTempUnit::Fahrenheit
                                        : PaTempUnit::Celsius);
    });

    // ── sub-PR-8: CPU, now SystemTile's row two ──────────────────────────
    // Replaces the old standalone CPU MetricLabel.
    m_systemTile->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_systemTile, &QWidget::customContextMenuRequested,
            this, &MainWindow::onCpuMenuRequested);
    // No whole-tile tooltip set here (unlike the old m_cpuMetric): the
    // merge means m_systemTile's tooltip is already owned by
    // SystemTile::refreshPaRow(), which sets it to the °C/°F click hint
    // whenever a temperature reading is present and clears it otherwise.
    // Setting a second, competing tooltip here would just get clobbered
    // by the next PA reading, unpredictably.

    // TX Inhibit has no slot of its own any more.
    //
    // It used to be an "INH" pill dimmed to 14%. Two problems, both found on
    // a bench: the abbreviation meant nothing to the operator, and the pill
    // carried a 1 px #ff6060 border that survived the dimming while its text
    // did not, so the safety corner sat there showing an empty alarm-red
    // outline while nothing was wrong. That is the opposite of what the
    // corner is for.
    //
    // Inhibit is a property OF transmit, not a peer indicator beside it, so
    // it now paints onto the TX badge itself: a prohibition symbol replaces
    // the TX dot and a toast names the reason at the moment it asserts. See
    // setTxInhibited(). That reclaims a whole reserved slot as well.

    // ── sub-PR-8: PA Status StatusBadge ──────────────────────────────────
    // Variant::On (green ✓ PA OK) / Variant::Tx (red ✓ PA FAULT).
    // Driven by RadioModel::paTripped(); setPaTripped() flips the variant.
    // Signal wiring lands in Task 17 (same as the original QLabel).
    m_paStatusBadge = new StatusBadge(barWidget);
    m_paStatusBadge->setObjectName(QStringLiteral("paStatusBadge"));
    // SVG-backed icon — earlier revisions used the U+2713 CHECK MARK
    // glyph, which renders inconsistently across the SF Mono / Menlo /
    // monospace fallback chain (boxed or kerned wrong on platforms
    // without SF Mono installed). The SVG is rendered at 14 logical
    // px and tinted with the variant's foreground color.
    m_paStatusBadge->setSvgIcon(QStringLiteral(":/icons/badge-check.svg"));
    m_paStatusBadge->setLabel(QStringLiteral("PA"));
    m_paStatusBadge->setVariant(StatusBadge::Variant::On);
    m_paStatusBadge->setToolTip(tr("PA status: OK"));

    // ── ADC overload alarm: reserved slot between PA and TX ──────────────
    // Dimmed by default; shown at full opacity when StepAttenuatorController
    // emits an overload event, dimmed again 2 s after the latest event by
    // the timer below.
    //
    // Source-first port of Thetis pollOverloadSyncSeqErr + ucInfoBar.Warning
    // [@501e3f5]:
    //   console.cs:21323        adc_names[] = { "ADC0", "ADC1", "ADC2" }
    //   console.cs:21359-21389  per-ADC level counter; level>0 → warn,
    //                            any level>3 → red_warning
    //   ucInfoBar.cs:911-933    Warning(msg, red_warning, show_duration):
    //                            ForeColor = red ? Red : Yellow;
    //                            Visible=true; _warningTimer.Start()
    m_adcOvlBadge = new AdcOverloadBadge(barWidget);
    m_adcOvlBadge->setObjectName(QStringLiteral("adcOvlBadge"));
    dimSafetyBadge(m_adcOvlBadge, false);

    // Auto-hide timer mirrors Thetis ucInfoBar._warningTimer — single-shot
    // 2000 ms, restarts on each overload event so a single hit keeps the
    // alarm visible for the full 2 s even after the per-ADC level decays.
    // Source: ucInfoBar.cs:927-932 + console.cs:21388 show_duration=2000
    // [@501e3f5].
    m_adcOvlHideTimer = new QTimer(this);
    m_adcOvlHideTimer->setSingleShot(true);
    m_adcOvlHideTimer->setInterval(2000);
    connect(m_adcOvlHideTimer, &QTimer::timeout, this, [this]() {
        if (m_adcOvlBadge) { dimSafetyBadge(m_adcOvlBadge, false); }
        // No relayout() needed: m_safetyGroup is registered with
        // m_chromeBar as one rung-0 item at its pinned 4x50 px sizeHint
        // (design §4.5), so dimming a badge inside it never changes the
        // group's own required width (finding routed from Task A6).
    });

    // R-R3-46 / R-R3-21: the alarm for a set of per-ADC levels, from this
    // window's controller (local) or the Core's report (remote, below).
    const auto showAdcOverload = [this](const std::array<OverloadLevel, 3>& levels) {
        // Thetis adc_names table — console.cs:21323 [@501e3f5]
        static const char* const kAdcNames[3] = { "ADC0", "ADC1", "ADC2" };

        // Build the alarm state: which ADCs are firing, plus highest
        // severity. Thetis console.cs:21359-21389 [@501e3f5] —
        // red_warning is any level > 3; our levelToSeverity() maps that
        // to OverloadLevel::Red.
        bool anyRed = false;
        QString shownAdcs;
        QString tip;
        for (int i = 0; i < 3; ++i) {
            const OverloadLevel lvl = levels[static_cast<std::size_t>(i)];
            if (lvl == OverloadLevel::None) { continue; }
            if (lvl == OverloadLevel::Red) { anyRed = true; }
            if (!shownAdcs.isEmpty()) { shownAdcs += QStringLiteral("/"); }
            shownAdcs += QString::number(i);
            if (!tip.isEmpty()) { tip += QStringLiteral("\n"); }
            tip += QStringLiteral("%1: overload").arg(
                QString::fromLatin1(kAdcNames[i]));
        }

        if (shownAdcs.isEmpty()) {
            // No ADC currently above level 0 — let the 2 s auto-hide
            // timer expire so a just-cleared overload stays visible
            // for the remainder of its window. Matches Thetis.
            return;
        }

        m_adcOvlBadge->setAdcs(shownAdcs);
        // ucInfoBar.cs:928 [@501e3f5] — red_warning ? Red : Yellow.
        m_adcOvlBadge->setVariant(anyRed ? AdcOverloadBadge::Variant::Tx
                                         : AdcOverloadBadge::Variant::Warn);
        m_adcOvlBadge->setToolTip(tip);
        dimSafetyBadge(m_adcOvlBadge, true);

        // No relayout() needed here either -- same reasoning as the
        // auto-hide timer above.

        // Restart auto-hide — Thetis: _warningTimer.Stop(); .Start();
        // (ucInfoBar.cs:927+932 [@501e3f5]).
        m_adcOvlHideTimer->start();
    };
    connect(m_stepAttController, &StepAttenuatorController::overloadStatusChanged,
            this, [this, showAdcOverload](int /*adc*/, OverloadLevel /*level*/) {
        showAdcOverload({m_stepAttController->overloadLevel(0),
                         m_stepAttController->overloadLevel(1),
                         m_stepAttController->overloadLevel(2)});
    });
    // A remote window's controller has no radio. The alarm follows the
    // Core's overload report instead: the `stepAtt` object's ADC0 and ADC1
    // levels (0 none, 1 yellow, 2 red), which change when the Core's own
    // levels do, as the controller's signal above does.
    if (!m_radioModel->ownsLocalDsp()) {
        if (StepAttenuatorFacade* stepAtt = m_radioModel->stepAttFacade()) {
            const auto fromWire = [](int level) {
                return level >= 2 ? OverloadLevel::Red
                     : level == 1 ? OverloadLevel::Yellow : OverloadLevel::None;
            };
            const auto fromCore = [stepAtt, fromWire, showAdcOverload](int) {
                showAdcOverload({fromWire(stepAtt->overloadAdc0()),
                                 fromWire(stepAtt->overloadAdc1()), OverloadLevel::None});
            };
            connect(stepAtt, &StepAttenuatorFacade::overloadAdc0Changed, this, fromCore);
            connect(stepAtt, &StepAttenuatorFacade::overloadAdc1Changed, this, fromCore);
        }
    }

    // ── sub-PR-8: Canonical TX StatusBadge ───────────────────────────────
    // Solid red (Variant::Tx) when MoxController emits moxStateChanged(true).
    // Dim (Variant::Off) at rest. No flash per design spec.
    m_txStatusBadge = new StatusBadge(barWidget);
    m_txStatusBadge->setObjectName(QStringLiteral("txStatusBadge"));
    // SVG-backed icon — see PA badge note above for rationale. The dot
    // shape matches the U+25CF BLACK CIRCLE glyph it replaces.
    m_txStatusBadge->setSvgIcon(QStringLiteral(":/icons/badge-dot.svg"));
    m_txStatusBadge->setLabel(QStringLiteral("TX"));
    m_txStatusBadge->setVariant(StatusBadge::Variant::Off);
    m_txStatusBadge->setToolTip(tr("Receive (MOX off)"));

    // ── Reserved safety slots (design §4.5) ──────────────────────────────
    // Every slot is permanently allocated. Only the badge inside changes.
    // The old code inserted the overload badge BETWEEN the PA and TX badges
    // and made it visible on overload, so TX slid sideways at the exact
    // moment something went wrong. Reserving the slot fixes that: an alarm
    // now lights up in a pixel the operator has already learned.
    m_safetyGroup = new QWidget(barWidget);
    m_safetyGroup->setObjectName(QStringLiteral("safetyGroup"));
    m_safetyGroup->setStyleSheet(QStringLiteral(
        "QWidget#safetyGroup { border-left: 1px solid #203040; }"));
    auto* safetyRow = new QHBoxLayout(m_safetyGroup);
    safetyRow->setContentsMargins(8, 0, 0, 0);
    safetyRow->setSpacing(6);

    auto addSlot = [&](QWidget* badge, int widthPx = kSafetySlotWidthPx) {
        auto* slot = new QWidget(m_safetyGroup);
        slot->setObjectName(QStringLiteral("safetySlot"));
        slot->setFixedWidth(widthPx);
        auto* sl = new QHBoxLayout(slot);
        sl->setContentsMargins(0, 0, 0, 0);
        sl->addWidget(badge);
        badge->setParent(slot);
        safetyRow->addWidget(slot);
    };

    addSlot(m_paStatusBadge);
    // The trip input is specific to an unported Andromeda/Ganymede CAT
    // producer. Hide the whole reserved slot until that hardware path exists.
    UnbuiltFeatures::hideUnlessBuilt(m_paStatusBadge->parentWidget(),
                                    UnbuiltFeature::GanymedeTrip);
    // The alarm gets a slot sized to its own content, not to its
    // neighbours. PA and TX stay narrow and learnable by position.
    addSlot(m_adcOvlBadge, kOverloadSlotWidthPx);
    addSlot(m_txStatusBadge);
    // iPhone app plan Task 78 (the several-devices design, section 12
    // item 2): who holds transmit when it is not this window, beside TX.
    // Sized to its words; shown only while another device (or the radio)
    // holds transmit, so the fixed slots before it never move.
    m_txHolderChip = new StatusBadge(m_safetyGroup);
    m_txHolderChip->setObjectName(QStringLiteral("txHolderBadge"));
    m_txHolderChip->setVariant(StatusBadge::Variant::Info);
    m_txHolderChip->setVisible(false);
    safetyRow->addWidget(m_txHolderChip);
    hbox->addWidget(m_safetyGroup);

    // Wire TX badge to MoxController. MoxController lives on m_radioModel;
    // both are created before buildStatusBar() runs.
    if (MoxController* mox = m_radioModel->moxController()) {
        // Qt::UniqueConnection is not supported for lambda connects — this
        // connect runs once at construction so no deduplication is needed.
        connect(mox, &MoxController::moxStateChanged, this, [this](bool tx) {
            // While inhibited the badge is showing the prohibition symbol and
            // must keep showing it; a MOX transition underneath must not
            // repaint over an active interlock.
            if (m_txInhibited) { return; }
            m_txStatusBadge->setSvgIcon(QStringLiteral(":/icons/badge-dot.svg"));
            m_txStatusBadge->setVariant(tx ? StatusBadge::Variant::Tx
                                           : StatusBadge::Variant::Off);
            m_txStatusBadge->setToolTip(tx ? tr("Transmitting (MOX engaged)")
                                           : tr("Receive (MOX off)"));
        });
    }

    // ── OverflowChip: surfaces folded-item contents when the strip is tight
    // Sits at the right end of the strip; the "…" appears whenever
    // m_chromeBar has folded >= 1 item to fit the bar width. Hidden
    // (unavailable) when nothing is folded. The clock lives on TitleBar
    // now (Task A7), not here.
    m_overflowChip = new OverflowChip(barWidget);
    hbox->addWidget(m_overflowChip);

    // ── CPU usage timer ──────────────────────────────────────────────────────
    // Two sources, user-toggleable via right-click on m_systemTile:
    //   System  (default) — host_processor_info / whole-machine CPU,
    //                       mirrors Thetis _total_cpu_usage PerformanceCounter.
    //   App     — getrusage(RUSAGE_SELF), this process only,
    //                       mirrors Thetis Common.ProcessCPUUsage.
    // 1 s tick rate matches Thetis cpu_meter_delay (console.cs:20102).
    // Smoothed value via 0.8/0.2 mix per Thetis console.cs:26224.
    // Restore persisted toggle (default System per Thetis default).
    // Wired on every supported platform — readSystemCpuPercent /
    // readProcessCpuPercent are cross-platform (macOS / Linux / Windows).
    // The context-menu policy + connect are wired above, next to
    // m_systemTile's construction.
    m_cpuShowSystem = (AppSettings::instance()
                          .value(QStringLiteral("CpuShowSystem"),
                                 QStringLiteral("True"))
                          .toString() == QStringLiteral("True"));
    // Parity ruling C9: a remote window's CPU row source.
    m_cpuRowCycler.setSource(CpuRowCycler::sourceFromKey(
        AppSettings::instance().value(QStringLiteral("CpuRowSource"),
                                      QStringLiteral("Cycle")).toString()));

    m_cpuTimer = new QTimer(this);
    connect(m_cpuTimer, &QTimer::timeout, this, [this]() {
        const double pct = m_cpuShowSystem ? readSystemCpuPercent()
                                           : readProcessCpuPercent();
        // Thetis smoothing: smoothed = smoothed*0.8 + new*0.2
        m_cpuSmoothedPct = m_cpuSmoothedPct * 0.8 + pct * 0.2;
        if (m_systemTile) {
            refreshCpuRow(m_cpuTimer->interval());
            // CPU's digit count varies (0-100%), so its row can change
            // width tick to tick. Cheap: relayout() no-ops unless the
            // computed rung actually changes (ChromeBarController::relayout).
            if (m_chromeBar && m_chromeBarWidget) {
                m_chromeBar->setNaturalWidth(m_systemTile,
                                             m_systemTile->sizeHint().width());
                m_chromeBar->relayout(m_chromeBarWidget->width());
            }
        }
    });
    // Task 3.6: restore persisted rate (default 1 Hz = 1000 ms interval).
    {
        const int savedHz = AppSettings::instance().value(
            QStringLiteral("GeneralCpuMeterUpdateRateHz"), 1).toInt();
        const int clampedHz = qBound(1, savedHz, 30);
        m_cpuTimer->start(1000 / clampedHz);
    }

    // ── Layout authority (design §5) ───────────────────────────────────
    // One controller replaces RxDashboard's internal ladder, the old
    // right-strip drop-priority pass, and Qt's squeeze in the left section.
    // The rung assignments live in registerChromeBarItems so they can be
    // tested without constructing MainWindow; do not inline them here.
    m_chromeBar = new ChromeBarController(this);

    ChromeBarWidgets bar;
    bar.panButton        = panBtn;
    bar.panelToggle      = panelToggleLabel;
    bar.stationBlock     = m_stationBlock;
    bar.safetyGroup      = m_safetyGroup;
    bar.psaIndicator     = m_psaIndicator;
    bar.overflowChip     = m_overflowChip;
    bar.systemTile       = m_systemTile;
    bar.systemTileSep    = m_systemTileSep;
    bar.tgxlChip         = m_tgxlChip;
    bar.catIndicator     = m_catIndicator;
    bar.catSep           = m_catSep;
    bar.tciIndicator     = m_tciIndicator;
    bar.tciSep           = m_tciSep;
    bar.chain0           = m_chain0IndicatorWidget;
    // chain1 is always constructed (below), never null; single-ADC SKUs
    // are gated via setItemAvailable on rxFilterChainCount, not by
    // omitting this widget.
    bar.chain1           = m_chain1IndicatorWidget;
    bar.rxDashRow        = m_rxDashboard;
    bar.placeholderGroup = m_placeholderGroup;
    bar.placeholderSep   = m_placeholderSep;
    bar.bandStackLabel   = m_bandStackLabel;
    bar.tnfLabel         = m_tnfLabel;
    for (int rung = 5; rung <= 9; ++rung) {
        bar.pillByRung[rung] = m_rxDashboard->badgeForRung(rung);
    }

    registerChromeBarItems(*m_chromeBar, bar);

    // registerChromeBarItems just measured w.rxDashRow's raw sizeHint(),
    // which at this point in construction happens to equal tag + mode +
    // filter + AGC (the four badges visible before any slice binds) --
    // wrong on two counts: it double-counts AGC (registered separately at
    // rung 9 above) and it would go stale the moment any pill's
    // visibility changes. Override with the pill-independent residual
    // immediately (final-fix-wave finding 5).
    if (m_rxDashboard) {
        m_chromeBar->setNaturalWidth(m_rxDashboard,
                                     m_rxDashboard->residualWidth());
    }

    // Items that start unavailable until their owning signal says
    // otherwise (Task A8 fix round 1, findings 1-3). No radio has
    // connected and no slice has bound yet at this point in construction,
    // so nothing is known to be present, armed or DSP-active. Without
    // this, availability defaults to true (addItem's default) and the
    // FIRST relayout() -- which always runs a full pass, since
    // m_foldedThrough starts at -1 -- would force-show a blank PSA
    // indicator and stray "TGXL" / "CH 1" tiles, and RxDashboard's four
    // toggle pills would pop up empty on every cold launch.
    m_chromeBar->setItemAvailable(m_psaIndicator, false);
    m_chromeBar->setItemAvailable(m_tgxlChip, false);
    m_chromeBar->setItemAvailable(m_chain1IndicatorWidget, false);
    m_chromeBar->setItemAvailable(m_overflowChip, false);
    // R-R3-49: items whose feature is not built yet never show. Reported as
    // availability, the controller's one side channel, so the fold ladder
    // neither shows them nor charges their width.
    m_chromeBar->setItemAvailable(m_bandStackLabel,
                                  UnbuiltFeatures::isBuilt(UnbuiltFeature::BandStack));
    m_chromeBar->setItemAvailable(m_placeholderGroup,
                                  UnbuiltFeatures::isBuilt(UnbuiltFeature::Cwx)
                                      || UnbuiltFeatures::isBuilt(UnbuiltFeature::Voice)
                                      || UnbuiltFeatures::isBuilt(UnbuiltFeature::Fdx));
    m_chromeBar->setItemAvailable(m_catIndicator,
                                  UnbuiltFeatures::isBuilt(UnbuiltFeature::Cat));
    for (int rung = 5; rung <= 8; ++rung) {  // SQL, APF, NB, NR
        m_chromeBar->setItemAvailable(m_rxDashboard->badgeForRung(rung), false);
    }
    // AGC (rung 9) has no off state and keeps the default available=true.

    // RxDashboard's on*Changed handlers report DSP-active state (and
    // hence width, since StatusBadge::setLabel changes minimum width
    // live) through this signal instead of calling setVisible directly
    // (RxDashboard.h doc comment).
    connect(m_rxDashboard, &RxDashboard::badgeAvailabilityChanged,
            this, [this](int rung, bool available) {
        if (!m_chromeBar || !m_chromeBarWidget) { return; }
        StatusBadge* badge = m_rxDashboard->badgeForRung(rung);
        if (!badge) { return; }
        m_chromeBar->setItemAvailable(badge, available);
        m_chromeBar->setNaturalWidth(badge, badge->sizeHint().width());
        m_chromeBar->relayout(m_chromeBarWidget->width());
    });

    // The slice tag, mode or filter content changed, so the residual
    // registered above is stale (final-fix-wave finding 5). Mirrors the
    // badgeAvailabilityChanged handler just above, minus the availability
    // half -- the row itself never folds.
    connect(m_rxDashboard, &RxDashboard::residualWidthChanged, this, [this]() {
        if (!m_chromeBar || !m_chromeBarWidget || !m_rxDashboard) { return; }
        m_chromeBar->setNaturalWidth(m_rxDashboard,
                                     m_rxDashboard->residualWidth());
        m_chromeBar->relayout(m_chromeBarWidget->width());
    });

    // m_overflowChip is registered with m_chromeBar at rung 0 (final-fix-
    // wave finding 4), so the controller is the sole writer of its
    // visibility; setDroppedItems() no longer calls setVisible() itself.
    // This handler feeds it content AND reports the resulting
    // available/unavailable fact back through setItemAvailable, same
    // shape as updatePsaIndicatorVisibility(). The nested relayout() call
    // is safe: the chip's width can only ever push the required rung UP
    // (never down), so once its availability flips true the folded set
    // stays a superset and the chain settles in one extra pass; the
    // reverse direction (labels empty -> chip goes unavailable) is
    // self-consistent at rung 0, which never has anything folded, so no
    // pass beyond that one is triggered either.
    connect(m_chromeBar, &ChromeBarController::foldStateChanged, this,
            [this](const QStringList& labels) {
        m_overflowChip->setDroppedItems(labels);
        if (m_chromeBar && m_chromeBarWidget) {
            m_chromeBar->setItemAvailable(m_overflowChip, !labels.isEmpty());
            m_chromeBar->relayout(m_chromeBarWidget->width());
        }
    });

    // Add the full-width bar widget to the status bar.
    //
    // Deliberately addWidget rather than addPermanentWidget: the bar takes
    // the full width with stretch 1, so as a permanent widget it would
    // leave a showMessage no room to render and the notice would be lost
    // silently. Nothing calls showMessage any more for exactly that
    // reason; notices go through showToast() below. If you are about to
    // add a showMessage here, it will blank this entire bar. Use
    // showToast().
    sb->addWidget(barWidget, 1);
}

// ── Transient notices ────────────────────────────────────────────────────────
//
// Bench report 2026-07-30 (JJ, KG4VCF): pressing TUNE with PureSignal
// active replaced the whole bottom bar with a single line of text for
// six seconds. Root cause is not the message, it is the surface:
// QStatusBar::showMessage hides every non-permanent widget while a
// message is up, and buildStatusBar adds the entire bar as one such
// widget. So any notice cost the operator the CH pill, the PureSignal
// indicator, the radio name, CAT and TCI state, the PA and TX badges,
// and the clock (which lived on this bar at the time; it has since
// moved to TitleBar, Task A7), all at once, mid-transmit.
//
// The notices themselves are worth keeping. They move here instead.
StatusToast* MainWindow::showToast(const QString& message,
                                   ToastSeverity severity,
                                   int timeoutMs)
{
    if (m_shuttingDown || message.isEmpty()) { return nullptr; }

    // Drop dead entries first so a repeat check and the restack below
    // both see only live toasts.
    m_toasts.removeIf([](const QPointer<StatusToast>& t) { return t.isNull(); });

    // A repeat of something already on screen restarts its countdown
    // rather than stacking a second copy. Several of these fire from
    // signals that can repeat while the condition persists, and a column
    // of identical toasts is worse than the message it replaced.
    for (const QPointer<StatusToast>& existing : m_toasts) {
        if (existing && existing->message() == message) {
            existing->refresh(timeoutMs);
            return existing;
        }
    }

    auto* toast = new StatusToast(message, severity, timeoutMs, this);
    connect(toast, &QObject::destroyed, this, [this]() {
        m_toasts.removeIf([](const QPointer<StatusToast>& t) { return t.isNull(); });
        restackToasts();
    });
    m_toasts.append(toast);
    toast->show();
    restackToasts();
    return toast;
}

void MainWindow::restackToasts()
{
    // Bottom-right, newest nearest the bar, growing upward. Offset clears
    // the status bar itself so a toast never covers the thing it was
    // introduced to stop covering.
    const QRect geom = frameGeometry();
    const int rightEdge = geom.right() - 20;
    int bottom = geom.bottom() - 60;

    for (auto it = m_toasts.crbegin(); it != m_toasts.crend(); ++it) {
        StatusToast* toast = *it;
        if (!toast) { continue; }
        toast->move(rightEdge - toast->width(), bottom - toast->height());
        bottom -= toast->height() + 8;
    }
}

// ── Phase 3M-0 Task 14 / sub-PR-8: PA trip badge update ──────────────────────
// Called by Task 17 wiring when RadioModel::paTrippedChanged fires.
// Flips the StatusBadge variant (On = green ✓ PA OK; Tx = red ✓ PA FAULT)
// and updates the tooltip atomically.
void MainWindow::setPaTripped(bool tripped)
{
    if (!m_paStatusBadge) { return; }
    if (tripped) {
        m_paStatusBadge->setVariant(StatusBadge::Variant::Tx);
        m_paStatusBadge->setToolTip(tr("PA status: FAULT (the PA tripped and MOX dropped)"));
    } else {
        m_paStatusBadge->setVariant(StatusBadge::Variant::On);
        m_paStatusBadge->setToolTip(tr("PA status: OK"));
    }
}

// ── Phase 3M-0 Task 14: TX Inhibit label state ───────────────────────────────
// Called by Task 17 wiring when TxInhibitMonitor::txInhibitedChanged fires.
// The "INH" pill lives in a permanently allocated safety slot (design
// §4.5) -- dims to 14% opacity when inactive rather than hiding, so this
// never resizes the slot. setVisible() would be a no-op here in the wrong
// direction: the label stays Qt-visible at all times once inside its slot.
// HL2 port part 2: the badge's tooltip and the toast name the reason when
// it is more than the external input (the HL2 I/O board's fault code,
// RadioModel::txInhibitReason), and follow it when the code changes.
void MainWindow::showTxInhibitReason()
{
    if (!m_txStatusBadge || !m_txInhibited) { return; }
    const QString reason = m_radioModel ? m_radioModel->txInhibitReason() : QString();
    if (m_txInhibitToast) {
        m_txInhibitToast->close();
        m_txInhibitToast = nullptr;
    }
    if (reason.isEmpty()) {
        m_txStatusBadge->setToolTip(
            tr("Transmit blocked by an external TX Inhibit signal."));
        m_txInhibitToast = showToast(
            tr("Transmit blocked: external TX Inhibit asserted."),
            ToastSeverity::Error, 8000);
        return;
    }
    m_txStatusBadge->setToolTip(tr("Transmit blocked. %1").arg(reason));
    m_txInhibitToast = showToast(tr("Transmit blocked. %1").arg(reason),
                                 ToastSeverity::Error, 8000);
}

void MainWindow::setTxInhibited(bool inhibited)
{
    if (!m_txStatusBadge) { return; }
    if (m_txInhibited == inhibited) { return; }
    m_txInhibited = inhibited;

    if (inhibited) {
        // A prohibition symbol over TX, not a separate "INH" pill. Inhibit is
        // a property of transmit, so it belongs on the transmit indicator;
        // and the pill's abbreviation meant nothing to an operator who had
        // not read the source (bench report, 2026-08-03).
        m_txStatusBadge->setSvgIcon(QStringLiteral(":/icons/badge-prohibited.svg"));
        m_txStatusBadge->setVariant(StatusBadge::Variant::Tx);
        // The symbol says transmit is blocked; the toast says why, once, at
        // the moment it happens. Error severity because an interlock is
        // actively refusing the operator, which is what that level means.
        // Held so it can be taken down the instant inhibit clears rather
        // than aging out and leaving a stale notice on screen.
        showTxInhibitReason();
        return;
    }

    // Cleared. Hand the badge back to whatever MOX currently says, so the
    // operator does not have to key up to get a truthful indicator again.
    if (m_txInhibitToast) {
        m_txInhibitToast->close();
        m_txInhibitToast = nullptr;
    }
    const bool tx = m_radioModel && m_radioModel->moxController()
                    && m_radioModel->moxController()->isMox();
    m_txStatusBadge->setSvgIcon(QStringLiteral(":/icons/badge-dot.svg"));
    m_txStatusBadge->setVariant(tx ? StatusBadge::Variant::Tx
                                   : StatusBadge::Variant::Off);
    m_txStatusBadge->setToolTip(tx ? tr("Transmitting (MOX engaged)")
                                   : tr("Receive (MOX off)"));
}

// ── Phase 23: TCI indicator update + Setup navigation ────────────────────────
//
// updateTciIndicator() — 4 states per design doc §8.4.
// Reads m_tciServerRunning / m_tciClientCount / m_tciHasTxClient (all updated
// by TciServer signal lambdas) and applies color + text + tooltip.
//
// Colors:
//   Off:       #404858 (dim grey)
//   On:        #6f6    (green)
//   On · N:    #6cf    (cyan)
//   On · N TX: #ec6    (orange)
void MainWindow::updateTciIndicator()
{
    if (!m_tciIndicatorBotLabel) { return; }

    QString text;
    QString color;
    QString tooltip;

#ifdef HAVE_WEBSOCKETS
    const quint16 port = m_tciServer ? m_tciServer->port() : 50001;
    const QString addr = QStringLiteral("127.0.0.1:%1").arg(port);
#else
    const QString addr = QStringLiteral("127.0.0.1:50001");
#endif

    if (!m_tciServerRunning) {
        text    = QStringLiteral("Off");
        color   = QStringLiteral("#404858");
        tooltip = QStringLiteral("TCI Server stopped. Click to open Setup.");
    } else if (m_tciClientCount == 0) {
        text    = QStringLiteral("On");
        color   = QStringLiteral("#6f6");
        tooltip = QStringLiteral("TCI Server listening on %1. No clients.").arg(addr);
    } else if (!m_tciHasTxClient) {
        text    = QStringLiteral("On · %1").arg(m_tciClientCount);
        color   = QStringLiteral("#6cf");
        tooltip = QStringLiteral("TCI Server listening on %1. %2 client%3 connected.")
                      .arg(addr)
                      .arg(m_tciClientCount)
                      .arg(m_tciClientCount == 1 ? QString{} : QStringLiteral("s"));
    } else {
        text    = QStringLiteral("On · %1 ▸TX").arg(m_tciClientCount);
        color   = QStringLiteral("#ec6");
        QString txPeer;
#ifdef HAVE_WEBSOCKETS
        if (m_tciServer) {
            txPeer = m_tciServer->activeTxClientPeer();
        }
#endif
        tooltip = QStringLiteral("TCI Server listening on %1. %2 client%3. %4 holds TX audio.")
                      .arg(addr)
                      .arg(m_tciClientCount)
                      .arg(m_tciClientCount == 1 ? QString{} : QStringLiteral("s"))
                      .arg(txPeer.isEmpty() ? QStringLiteral("A client") : txPeer);
    }

    // A remote window can have a local TCI server and a Core station
    // server at the same time. Name each in the bottom indicator.
    const IStationLink* stationLink = m_radioModel ? m_radioModel->stationLink() : nullptr;
    const StationTciModel* stationTci = m_radioModel ? m_radioModel->stationTciModel() : nullptr;
    if (stationLink && stationLink->stationTciAvailable() && stationTci) {
        const int coreClients = stationLink->stationTciServerAvailable()
            ? stationTci->clients().size() : 0;
        text += stationTci->listening()
            ? QStringLiteral(" | Core %1").arg(coreClients)
            : QStringLiteral(" | Core off");
        tooltip = QStringLiteral("This window: %1\nThe Core's TCI server: %2")
            .arg(tooltip, stationTci->listening()
                ? QStringLiteral("listening on port %1 with %2 apps")
                      .arg(stationTci->port()).arg(coreClients)
                : QStringLiteral("not listening"));
    }

    m_tciIndicatorBotLabel->setText(text);
    m_tciIndicatorBotLabel->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 11px; }").arg(color));
    if (m_tciIndicator) {
        m_tciIndicator->setToolTip(tooltip);
        // Bottom row text ranges from "Off" to "On · 3 ▸TX" -- a real
        // width change (Task A8 fix round 1 finding 4).
        if (m_chromeBar && m_chromeBarWidget) {
            m_chromeBar->setNaturalWidth(m_tciIndicator,
                                         m_tciIndicator->sizeHint().width());
            m_chromeBar->relayout(m_chromeBarWidget->width());
        }
    }
}

// openTciSetupPage() — open Setup dialog at "TCI Server" page.
// Pattern-matched from the many other "open setup" sites in MainWindow.cpp
// (e.g. vfoWidget::openSetupRequested, m_overlayPanel::openSetupRequested).
void MainWindow::openTciSetupPage()
{
    auto* dialog = createSetupDialog();
    if (dialog == nullptr) {
        return;  // the gate refused and has already said why
    }
    dialog->selectPage(QStringLiteral("TCI Server"));
    dialog->show();
}

// openSetup(pageKey) -- Phase 3P-II Phase 4 Task 90
//
// Generic navigation entry point wired to applet right-click menus.
// Maps a well-known key string to a SetupDialog tree label and opens the
// dialog at that page. If the key is not recognised, logs a warning and
// opens the dialog at the default page (first leaf).
//
// Key -> tree label mapping (Option 1 per plan Task 90):
//   "pgxlAdvanced"  -> "PGXL Advanced"
//   "tgxlAdvanced"  -> "TGXL Advanced"
//   "pgxlInterlock" -> "PGXL Interlock"
//   "peripherals"   -> "4O3A" (General tab contains Peripherals)
//
// Pattern matches openTciSetupPage(): fresh SetupDialog with WA_DeleteOnClose
// so geometry is not preserved across opens (consistent with all other Setup
// entry points in this file).
void MainWindow::openSetup(const QString& pageKey)
{
    auto* dialog = createSetupDialog();
    if (dialog == nullptr) {
        return;  // the gate refused and has already said why
    }

    // R-R3-49 (parity Task 8): the key-to-page map lives in SetupDialog. The
    // "PGXL Advanced", "TGXL Advanced" and "PGXL Interlock" tree labels this
    // map named were folded into CAT & Network > 4O3A, so those entries
    // opened Setup's first page; they now open their 4O3A tab.
    if (!dialog->selectNavigationTarget(pageKey)) {
        qWarning("MainWindow::openSetup: unknown pageKey '%s' -- opening at default page",
                 qUtf8Printable(pageKey));
    }
    dialog->show();
    dialog->raise();
}

// ── R-R3-21: container Mode / Filter / Antenna / Tune Step / VFO display ────
//
// Each ButtonBoxItem is a port of a Thetis MeterManager.cs button box; its
// click did nothing past ContainerWidget's relay (only the band buttons were
// connected). They act through the same SliceModel setters the VFO flag and
// the RX applet use, so a remote window changes the Core's slice exactly as
// those do. R-R3-49 (the unfinished-controls plan, Task 3): each acts on the
// container's own slice (ContainerWidget::rxSource(), slices A to D), as
// Thetis's meter acts on its own RX (MeterManager.cs:9424 [v2.10.3.15]
// passes the meter's RX with every click), and the function and band
// buttons go through ContainerButtonDispatcher.

namespace {

// A panel can contain a header meter and additional meter widgets in its
// scroll body. All of their visible FilterDisplays share the slice producer.
QList<MeterWidget*> contentMeters(QWidget* content)
{
    if (!content) { return {}; }
    QList<MeterWidget*> meters;
    if (auto* meter = qobject_cast<MeterWidget*>(content)) { meters.append(meter); }
    for (MeterWidget* meter : content->findChildren<MeterWidget*>()) {
        if (!meters.contains(meter)) { meters.append(meter); }
    }
    return meters;
}

} // namespace

void MainWindow::reconcileMiniDisplays()
{
    if (m_shuttingDown || !m_containerManager || !m_radioModel) { return; }
    std::map<int, QList<QPointer<FilterDisplayItem>>> wanted;
    for (ContainerWidget* container : m_containerManager->allContainers()) {
        // A floating form can hide its owner without setting the child's
        // explicit hidden flag. isVisible() includes that ancestor state.
        if (!container || !container->isVisible()) { continue; }
        for (MeterWidget* meter : contentMeters(container->content())) {
            const QVariant routed = meter->property("containerSourceContext");
            if (!meter->isVisible()) { continue; }
            QVector<MeterItem*> descendants = meter->items();
            for (MeterItem* root : meter->items()) { if (auto* face = qobject_cast<CompositePresetItem*>(root)) { descendants += face->internalItems(); } }
            for (MeterItem* base : descendants) {
                const QVariant entryContext = base->property("containerSourceContext");
                SliceModel* slice = ContainerSourceAdapter::slice(m_radioModel,
                    entryContext.isValid()?entryContext.toJsonObject():(routed.isValid()?routed.toJsonObject():QJsonObject{{"sliceId",container->rxSource()-1}}),
                    windowRxSlice(), containerSessionId());
                if (!slice || slice->streamIndex() < 0) { continue; }
                auto* item = qobject_cast<FilterDisplayItem*>(base);
                if (item && !item->property("containerUnsupportedSource").toBool() && meter->shouldRender(item)
                    && item->displayMode() != FilterDisplayItem::DisplayMode::None) {
                    wanted[slice->sliceIndex()].append(item);
                }
            }
        }
    }
    for (auto it = m_miniProducers.begin(); it != m_miniProducers.end();) {
        if (wanted.count(it->first) == 0) {
            for (const QPointer<FilterDisplayItem>& item : it->second->items) {
                if (item) { item->clearFrame(); }
            }
            it = m_miniProducers.erase(it);
        } else {
            ++it;
        }
    }
    for (auto& [sliceId, items] : wanted) {
        auto& producer = m_miniProducers[sliceId];
        if (!producer) { producer = std::make_unique<MiniProducer>(); }
        SliceModel* slice = m_radioModel->sliceById(sliceId);
        if (producer->slice != slice) {
            for (const auto& item : producer->items) { if (item) { item->clearFrame(); } }
            producer->trace.clearAveraging();
            producer->waterfall.clearAveraging();
            producer->cadence.invalidate();
            producer->filterCached = false;
            producer->txTrace.clear();
            producer->txWaterfall.clear();
            producer->slice = slice;
        }
        for (const auto& former : producer->items) {
            if (former && !items.contains(former)) { former->clearFrame(); }
        }
        producer->items = items;
    }
    if (m_remoteMedia) {
        QSet<int> slices;
        for (const auto& [sliceId, producer] : m_miniProducers) {
            if (!producer->items.isEmpty()) { slices.insert(sliceId); }
        }
        m_remoteMedia->setMiniDisplaySlices(slices);
        const bool highResolution = AppSettings::instance().value(
            QStringLiteral("DspOptionsHighResFilterCharacteristics"),
            QStringLiteral("False")).toString() == QLatin1String("True");
        m_radioModel->setMiniFilterResponseSlices(highResolution ? slices : QSet<int>{});
    } else if (TxDisplayFeed* feed = m_radioModel->txDisplayFeed()) {
        const SliceModel* tx = m_radioModel->txBoundSlice();
        feed->setLocalMiniDemand(tx && wanted.count(tx->sliceIndex()) != 0);
    }
}

void MainWindow::clearMiniSlice(int sliceId)
{
    auto found = m_miniProducers.find(sliceId);
    if (found == m_miniProducers.end()) { return; }
    found->second->txTrace.clear();
    found->second->txWaterfall.clear();
    for (const auto& item : found->second->items) {
        if (item) { item->clearFrame(); }
    }
}

void MainWindow::presentMiniFrame(int sliceId, const QVector<float>& traceDbm,
                                  const QVector<float>& waterfallDbm,
                                  double centreHz, double spanHz,
                                  bool transmit, bool advance)
{
    auto found = m_miniProducers.find(sliceId);
    if (found == m_miniProducers.end() || !m_radioModel) { return; }
    SliceModel* slice = m_radioModel->sliceById(sliceId);
    if (!slice || found->second->slice != slice) { clearMiniSlice(sliceId); return; }
    const double rxCentre = slice->frequency();
    const double txCentre = transmit ? double(m_radioModel->txFrequencyForSlice(slice)) : rxCentre;
    const auto& tx = m_radioModel->transmitModel();
    QVector<double> notches;
    if (NotchModel* model = m_radioModel->notchModel()) {
        for (const Notch& notch : model->notches()) {
            if (notch.active && notch.centerHz >= centreHz - spanHz / 2.0
                && notch.centerHz <= centreHz + spanHz / 2.0) {
                notches.append(notch.centerHz);
            }
        }
    }
    MiniProducer& producer = *found->second;
    SpectrumWidget* settingsWidget = nullptr;
    if (m_panStack) {
        for (PanadapterApplet* applet : m_panStack->allApplets()) {
            if (applet && applet->activeSliceIndex() == sliceId
                && applet->spectrumWidget()) {
                settingsWidget = applet->spectrumWidget();
                break;
            }
        }
    }
    // Thetis's CUSTOM branch indexes the source's 101-colour picker table
    // (MeterManager.cs:34321-34375). Cache the table by encoded setting so
    // the 30 fps presenter never rebuilds a widget on every frame.
    const QString gradientKey = transmit
        ? (settingsWidget ? settingsWidget->txWfGradient() : QString())
        : AppSettings::instance().value(QStringLiteral("DisplayWfCustomStops")).toString();
    if (producer.customGradient.isEmpty() || producer.gradientKey != gradientKey
        || producer.gradientTx != transmit) {
        GradientPickerWidget picker;
        if (!gradientKey.isEmpty()) { picker.setEncodedText(gradientKey); }
        producer.customGradient = picker.colorTable(101);
        producer.gradientKey = gradientKey;
        producer.gradientTx = transmit;
    }
    const bool highResolution = AppSettings::instance().value(
        QStringLiteral("DspOptionsHighResFilterCharacteristics"),
        QStringLiteral("False")).toString() == QLatin1String("True");
    if (highResolution && m_radioModel->role() != RadioModel::Role::Remote) {
        if (!producer.filterCached || producer.filterLow != slice->filterLow()
            || producer.filterHigh != slice->filterHigh()
            || producer.sampleRate != slice->sampleRateHz()
            || producer.dspMode != slice->dspMode()) {
            producer.filterResponse = {};
            m_radioModel->filterResponseForStation(sliceId, true,
                                                  &producer.filterResponse, nullptr);
            producer.filterLow = slice->filterLow();
            producer.filterHigh = slice->filterHigh();
            producer.sampleRate = slice->sampleRateHz();
            producer.dspMode = slice->dspMode();
            producer.filterCached = true;
        }
    } else if (highResolution) {
        producer.filterResponse = m_radioModel->miniFilterResponse(sliceId);
    } else {
        producer.filterResponse = {};
    }
    for (const auto& item : found->second->items) {
        if (!item) { continue; }
        item->setWaterfallLowColour(transmit && settingsWidget
            ? settingsWidget->txWfLowColor() : QColor(Qt::black));
        item->setCustomWaterfallGradient(producer.customGradient,
                                         producer.customGradient);
        item->presentFrame(traceDbm, waterfallDbm, centreHz, spanHz,
                           transmit, advance);
        item->setRfMarkers(rxCentre + slice->filterLow(),
                           rxCentre + slice->filterHigh(),
                           txCentre + tx.filterLow(), txCentre + tx.filterHigh(),
                           notches);
        if (highResolution && !producer.filterResponse.magnitudesDb.isEmpty()) {
            item->bindRxChannel(nullptr);
            item->setRfFilterResponse(producer.filterResponse.magnitudesDb,
                                      producer.filterResponse.startHz,
                                      producer.filterResponse.stepHz, rxCentre);
        } else {
            item->setRfFilterResponse({}, 0.0, 0.0, rxCentre);
        }
    }
}

void MainWindow::wireContainerControls(ContainerWidget* c)
{
    if (!c || c->property("mainControlsWired").toBool()) { return; }
    c->setProperty("mainControlsWired",true);
    c->installEventFilter(this);
    // Issue #118: the band buttons, now on the container's own slice.
    connect(c, &ContainerWidget::bandClicked, this, [this, c](int idx) {
        if (!m_containerButtons) { return; }
        showContainerButtonReason(m_containerButtons->clickBand(idx, containerControlRxSource(c)));
    });
    connect(c, &ContainerWidget::modeClicked, this,
            [this, c](int index) { onContainerModeClicked(c, index); });
    connect(c, &ContainerWidget::filterClicked, this,
            [this, c](int index) { onContainerFilterClicked(c, index); });
    connect(c, &ContainerWidget::filterContextRequested, this,
            [this, c](int index) { onContainerFilterContext(c, index); });
    connect(c, &ContainerWidget::vfoFilterContextRequested, this,
            [this, c]() { onContainerFilterContext(c, -1); });
    // From Thetis MeterManager.cs:13216 and :14593 [v2.10.3.15]: a band
    // button's or the VFO display's right-click opens console.PopupBandstack
    // (console.cs:48529). Band stacking is not built in NereusSDR (the Band
    // menu's entry waits for it too), so the right-click says so.
    connect(c, &ContainerWidget::bandStackRequested, this,
            [this](int) { showContainerButtonReason(containerBandStackReason()); });
    connect(c, &ContainerWidget::antennaSelected, this,
            [this, c](int index) { onContainerAntennaSelected(c, index); });
    connect(c, &ContainerWidget::tuneStepSelected, this,
            [this, c](int index) { onContainerTuneStepSelected(c, index); });
    connect(c, &ContainerWidget::frequencyChangeRequested, this,
            [this, c](int64_t deltaHz) { onContainerFrequencyStep(c, deltaHz); });
    connect(c, &ContainerWidget::otherButtonClicked, this,
            [this, c](int buttonId) { onContainerOtherButtonClicked(c, buttonId); });
    connect(c, &ContainerWidget::unavailableButtonClicked,
            this, &MainWindow::showContainerButtonReason);
    connect(c, &ContainerWidget::rxSourceChanged, this,
            [this, c](int) { refreshContainer(c); reconcileMiniDisplays(); });
    // An item added later (a preset, Container settings > Apply) gets the
    // saved meter settings and the slice's state as it arrives; one refresh
    // now covers restored containers.
    connect(c, &ContainerWidget::contentChanged, this,
            [this](QWidget* content) {
        watchContainerItems(content);
        refreshContainerControls();
    });
    watchContainerItems(c->content());
    refreshContainerControls();
}

void MainWindow::watchContainerItems(QWidget* content)
{
    QList<AppletPanelWidget*> panels;
    if (content) { panels=content->findChildren<AppletPanelWidget*>(); }
    if (auto* panel=qobject_cast<AppletPanelWidget*>(content)) { panels.prepend(panel); }
    for (AppletPanelWidget* panel : panels) {
        connect(panel, &AppletPanelWidget::headerWidgetChanged,
                this, &MainWindow::watchContainerItems, Qt::UniqueConnection);
        connect(panel, &AppletPanelWidget::panelWidgetAdded,
                this, &MainWindow::watchContainerItems, Qt::UniqueConnection);
    }
    for (MeterWidget* meter : contentMeters(content)) {
        meter->installEventFilter(this);
        for (MeterItem* root : meter->items()) {
            if (root->property("mainContainerWired").toBool()) { continue; }
            root->setProperty("mainContainerWired", true);
            onContainerItemAdded(root);
            QVector<MeterItem*> targets{root};
            if (auto* face = qobject_cast<CompositePresetItem*>(root)) { targets += face->internalItems(); }
            for (auto* c : m_containerManager->allContainers()) {
                if (!contentMeters(c->content()).contains(meter)) { continue; }
                for (MeterItem* target : targets) { c->wireInteractiveItem(target); }
                break;
            }
        }

        connect(meter, &MeterWidget::itemAdded, this,
                &MainWindow::onContainerItemAdded, Qt::UniqueConnection);
        connect(meter, &MeterWidget::itemRemoved, this,
                &MainWindow::reconcileMiniDisplays, Qt::UniqueConnection);
        connect(meter, &MeterWidget::displayVisibilityChanged, this,
                &MainWindow::reconcileMiniDisplays, Qt::UniqueConnection);
        for (MeterItem* item : meter->items()) {
            if (auto* mini = qobject_cast<FilterDisplayItem*>(item)) {
                connect(mini, &FilterDisplayItem::displayModeChanged, this,
                        &MainWindow::reconcileMiniDisplays, Qt::UniqueConnection);
            }
        }
    }
    reconcileMiniDisplays();
}

void MainWindow::onContainerItemAdded(MeterItem* item)
{
    if (!item) { return; }
    if (auto* mini = qobject_cast<FilterDisplayItem*>(item)) {
        connect(mini, &FilterDisplayItem::displayModeChanged, this,
                &MainWindow::reconcileMiniDisplays, Qt::UniqueConnection);
    }
    // R-R3-21: Setup > Multimeter's unit, decimal and history duration and
    // DSP > Options' high-resolution filter graph, which otherwise reached
    // a new item only when those Setup pages next opened.
    MultimeterPage::applyPersistedSettingsTo(item);
    DspOptionsPage::applyPersistedHighResFilterTo(m_radioModel, item);
    refreshContainerControls(item);
    reconcileMiniDisplays();
}

QString MainWindow::containerSessionId() const
{
    if (!m_radioModel) { return {}; }
    if (m_radioModel->role() == RadioModel::Role::Remote) {
        return m_stationClient ? QString::fromLatin1(m_stationClient->stationIdentityFingerprint().toHex()) : QString();
    }
    return m_radioModel->currentRadioMac();
}

int MainWindow::containerControlRxSource(const ContainerWidget* c) const
{
    if (!c) { return 0; }
    const QVariant routed = c->property("containerDispatchContext");
    if (!routed.isValid()) { return c->rxSource(); }
    SliceModel* source = ContainerSourceAdapter::slice(m_radioModel, routed.toJsonObject(), windowRxSlice(), containerSessionId());
    return source ? source->sliceIndex()+1 : 0;
}
SliceModel* MainWindow::containerSlice(const ContainerWidget* c) const
{
    return c && m_containerButtons ? m_containerButtons->sliceFor(containerControlRxSource(c)) : nullptr;
}

void MainWindow::watchSlicesForContainers()
{
    // Fix wave M4: sliceAdded / sliceRemoved during teardown reach here too.
    if (m_shuttingDown) { return; }
    for (const QMetaObject::Connection& conn : std::as_const(m_containerSliceConnections)) {
        QObject::disconnect(conn);
    }
    m_containerSliceConnections.clear();
    if (!m_radioModel) { return; }
    const auto refresh = [this]() { refreshContainerControls(); };
    for (SliceModel* slice : m_radioModel->slices()) {
        if (!slice) { continue; }
        // Tuning changes only what the VFO displays and band buttons of
        // the containers on that slice show; the rest are left alone on
        // every tuning step.
        m_containerSliceConnections
            << connect(slice, &SliceModel::frequencyChanged, this,
                       [this, slice]() { refreshContainerFrequency(slice); })
            << connect(slice, &SliceModel::frequencyChanged, this,
                       [this, slice]() { clearMiniSlice(slice->sliceIndex()); })
            << connect(slice, &SliceModel::streamIndexChanged, this,
                       [this, slice]() {
                           clearMiniSlice(slice->sliceIndex());
                           reconcileMiniDisplays();
                       })
            << connect(slice, &SliceModel::dspModeChanged, this, refresh)
            << connect(slice, &SliceModel::filterChanged, this, refresh)
            << connect(slice, &SliceModel::stepHzChanged, this, refresh)
            << connect(slice, &SliceModel::rxAntennaChanged, this, refresh)
            << connect(slice, &SliceModel::txAntennaChanged, this, refresh)
            << connect(slice, &SliceModel::anfEnabledChanged, this, refresh)
            << connect(slice, &SliceModel::snbEnabledChanged, this, refresh)
            << connect(slice, &SliceModel::mutedChanged, this, refresh)
            << connect(slice, &SliceModel::binauralEnabledChanged, this, refresh);
    }
    refreshContainerControls();
}

void MainWindow::refreshContainerControls(MeterItem* only)
{
    // Fix wave M4: the refresh connections (the global targets, sliceAdded
    // and sliceRemoved, each slice's changes) can fire while the window is
    // torn down, after m_containerButtons is destroyed and before the
    // children that emit are.
    if (m_shuttingDown) { return; }
    if (!m_containerManager || !m_radioModel || !m_containerButtons) { return; }
    for (ContainerWidget* c : m_containerManager->allContainers()) {
        if (!c) { continue; }
        refreshContainer(c, only);
    }
    reconcileMiniDisplays();
}

void MainWindow::refreshContainer(ContainerWidget* c, MeterItem* only)
{
    if (!c) { return; }
    for (MeterWidget* meter : contentMeters(c->content())) {
        const QVariant context = meter->property("containerSourceContext");
        for (MeterItem* root : meter->items()) {
            if (only && root != only && !root->findChildren<MeterItem*>().contains(only)) { continue; }
            const QVariant entryContext = root->property("containerSourceContext");
            refreshContainerMeter(c, meter, root, entryContext.isValid() ? entryContext.toJsonObject() :
                (context.isValid() ? context.toJsonObject() : QJsonObject{{"sliceId", c->rxSource()-1}}));
        }
    }
}
void MainWindow::refreshContainerMeter(ContainerWidget* c, MeterWidget* meter, MeterItem* only, const QJsonObject& context)
{
    if (m_shuttingDown || !meter || !m_radioModel || !m_containerButtons) { return; }
    const bool unsupported = only && only->property("containerUnsupportedSource").toBool();
    SliceModel* source = unsupported || (!c && !m_radioModel->isConnected()) ? nullptr : ContainerSourceAdapter::slice(m_radioModel, context, windowRxSlice(), containerSessionId());
    const int rxSource = source ? source->sliceIndex()+1 : 0;
    SliceModel* slice = m_containerButtons->sliceFor(rxSource);
    QVector<MeterItem*> items;
    for (MeterItem* root : meter->items()) {
        if (only && root != only && !root->findChildren<MeterItem*>().contains(only)) { continue; }
        items.append(root);
        if (auto* face = qobject_cast<CompositePresetItem*>(root)) { items += face->internalItems(); }
    }
    if (items.isEmpty()) { return; }
    // Snapshot actual child presentation rather than dirtying equal model pushes.
    const auto signature = [](MeterItem* item) {
        QString value = item->serialize();
        if (auto* box = qobject_cast<ButtonBoxItem*>(item)) {
            for (int i=0;i<box->buttonCount();++i) { value += QString::number(box->isButtonAvailable(i))+box->buttonUnavailableReason(i)+box->button(i).text+QString::number(box->button(i).on); }
        }
        if (auto* band = qobject_cast<BandButtonItem*>(item)) { value += QString::number(band->activeBand()); }
        if (auto* mode = qobject_cast<ModeButtonItem*>(item)) { value += QString::number(mode->activeMode()); }
        if (auto* vfo = qobject_cast<VfoDisplayItem*>(item)) { value += QString::number(vfo->frequency())+vfo->unavailableText(); }
        if (auto* other = qobject_cast<OtherButtonItem*>(item)) {
            for (int i=0;i<other->buttonCount();++i) { value += QString::number(other->buttonState(static_cast<OtherButtonItem::ButtonId>(i))); }
        }
        return value;
    };
    QHash<MeterItem*, QString> before;
    for (MeterItem* item : items) { before[item] = signature(item); }
    const auto finishPresentation = [&] {
        bool dirty = false;
        for (MeterItem* item : items) {
            if (before.value(item) == signature(item)) { continue; }
            dirty = true;
            if (auto* face = qobject_cast<CompositePresetItem*>(item->parent())) { face->markPresentationDirty(); }
        }
        if (dirty) {
            for (MeterItem* root : meter->items()) {
                if (items.contains(root)) { meter->invalidatePresentation(root); }
            }
        }
    };
    // Function buttons and band buttons, whatever the slice (the function
    // buttons' global targets do not need it; the dispatcher reports a
    // slice that is not open on the ones that do).
    const auto applyAlways = [&](MeterItem* item) {
        if (auto* other = qobject_cast<OtherButtonItem*>(item)) {
            m_containerButtons->apply(other, rxSource);
            return true;
        }
        if (auto* band = qobject_cast<BandButtonItem*>(item)) {
            m_containerButtons->applyBand(band, rxSource);
            return true;
        }
        return false;
    };

    if (!slice) {
        // Fix wave M2 (R-R3-49, R-R3-21): a slice that is not open (never
        // opened, or closed while this container was set to it) shows
        // none of its last state. The buttons light nothing and say why
        // when clicked; the VFO display says the slice is not open.
        const QString notOpen = unsupported ? only->property("unsupportedSourceReason").toString() :
            tr("%1 is not open").arg(ContainerWidget::sliceNameForRxSource(rxSource));
        const auto applyNoSlice = [&](MeterItem* item) {
            if (auto* face = qobject_cast<CompositePresetItem*>(item)) { face->setUnavailableText(notOpen); }
            if (applyAlways(item)) { return; }
            if (auto* box = qobject_cast<ButtonBoxItem*>(item)) {
                m_containerButtons->applySliceAvailability(box, rxSource);
            }
            if (auto* mode = qobject_cast<ModeButtonItem*>(item)) {
                mode->setActiveMode(-1);
            } else if (auto* filter = qobject_cast<FilterButtonItem*>(item)) {
                for (int i = 0; i < 10; ++i) { filter->setFilterLabel(i, QString()); }
                filter->setActiveFilter(-1);
            } else if (auto* step = qobject_cast<TuneStepButtonItem*>(item)) {
                step->setActiveStep(-1);
            } else if (auto* ant = qobject_cast<AntennaButtonItem*>(item)) {
                ant->setActiveRxAntenna(-1);
                ant->setActiveTxAntenna(-1);
            } else if (auto* vfo = qobject_cast<VfoDisplayItem*>(item)) {
                vfo->setUnavailableText(notOpen);
            }
        };
        {
            for (MeterItem* item : items) { applyNoSlice(item); }
        }
        finishPresentation();
        return;
    }

    const QString modeName = SliceModel::modeName(slice->dspMode());
    int modeIndex = -1;
    for (int i = 0; ; ++i) {
        const QString label = ModeButtonItem::modeLabel(i);
        if (label.isEmpty()) { break; }
        if (label == modeName) { modeIndex = i; break; }
    }

    QList<FilterPreset> presets;
    if (FilterPresetStore* store = m_radioModel->filterPresetStore()) {
        presets = store->presetsForMode(slice->dspMode());
    }
    int filterIndex = -1;
    for (int i = 0; i < presets.size(); ++i) {
        // RxApplet::updateFilterButtons tolerance: 50 Hz per edge.
        if (qAbs(slice->filterLow() - presets[i].low) <= 50
            && qAbs(slice->filterHigh() - presets[i].high) <= 50) {
            filterIndex = i;
            break;
        }
    }

    int stepIndex = -1;
    for (int i = 0; TuneStepButtonItem::stepHz(i) != 0; ++i) {
        if (TuneStepButtonItem::stepHz(i) == slice->stepHz()) { stepIndex = i; break; }
    }

    const auto antIndex = [](const QString& ant) {
        if (ant == QLatin1String("ANT1")) { return 0; }
        if (ant == QLatin1String("ANT2")) { return 1; }
        if (ant == QLatin1String("ANT3")) { return 2; }
        return -1;
    };

    const int widthHz = std::abs(slice->filterHigh() - slice->filterLow());
    const QString filterText = widthHz >= 1000
        ? QStringLiteral("%1k").arg(widthHz / 1000.0, 0, 'f', widthHz % 1000 == 0 ? 0 : 1)
        : QString::number(widthHz);
    const Band band = bandFromFrequency(slice->frequency());

    const auto applyTo = [&](MeterItem* item) {
        if (auto* bar = qobject_cast<BarPresetItem*>(item)) { bar->setAboveS9Frequency(slice->frequency() > 30000000.0); }
        if (auto* face = qobject_cast<CompositePresetItem*>(item)) {
            face->setUnavailableText({}); face->setFrequency(qint64(std::llround(slice->frequency())));
            face->setModeLabel(modeName); face->setBandLabel(bandLabel(band));
            face->setAboveS9Frequency(slice->frequency() > 30000000.0);
        }
        if (applyAlways(item)) { return; }
        if (auto* box = qobject_cast<ButtonBoxItem*>(item)) {
            m_containerButtons->applySliceAvailability(box, rxSource);
        }
        if (auto* mode = qobject_cast<ModeButtonItem*>(item)) {
            mode->setActiveMode(modeIndex);
        } else if (auto* filter = qobject_cast<FilterButtonItem*>(item)) {
            // F1..F10 show the mode's presets. A mode with fewer presets
            // leaves the rest blank (a click there does nothing,
            // onContainerFilterClicked), not the last mode's names.
            for (int i = 0; i < 10; ++i) {
                QString label;
                if (i < presets.size()) {
                    label = presets[i].name.isEmpty()
                        ? QStringLiteral("F%1").arg(i + 1) : presets[i].name;
                }
                filter->setFilterLabel(i, label);
            }
            filter->setActiveFilter(filterIndex);
        } else if (auto* step = qobject_cast<TuneStepButtonItem*>(item)) {
            step->setActiveStep(stepIndex);
        } else if (auto* ant = qobject_cast<AntennaButtonItem*>(item)) {
            ant->setActiveRxAntenna(antIndex(slice->rxAntenna()));
            ant->setActiveTxAntenna(antIndex(slice->txAntenna()));
        } else if (auto* vfo = qobject_cast<VfoDisplayItem*>(item)) {
            vfo->setUnavailableText(QString());
            vfo->setFrequency(static_cast<int64_t>(std::llround(slice->frequency())));
            vfo->setModeLabel(modeName);
            vfo->setFilterLabel(filterText);
            vfo->setBandLabel(bandLabel(band));
        }
    };
    {
        for (MeterItem* item : items) { applyTo(item); }
    }
    finishPresentation();
}

void MainWindow::refreshContainerFrequency(SliceModel* slice)
{
    if (m_shuttingDown || !slice) { return; }
    refreshContainerControls();
}

void MainWindow::onContainerOtherButtonClicked(ContainerWidget* c, int buttonId)
{
    if (!c || !m_containerButtons) { return; }
    const QString reason = m_containerButtons->click(
        static_cast<OtherButtonItem::ButtonId>(buttonId), containerControlRxSource(c));
    showContainerButtonReason(reason);
    // Targets with no change signal of their own (peak hold, VAX) light
    // from this refresh.
    refreshContainerControls();
}

void MainWindow::showContainerButtonReason(const QString& reason)
{
    if (reason.isEmpty()) { return; }
    showToast(reason, ToastSeverity::Warning, 3000);
}

void MainWindow::onContainerModeClicked(ContainerWidget* c, int index)
{
    SliceModel* slice = containerSlice(c);
    if (!slice) { return; }
    // From Thetis MeterManager.cs:10277 [v2.10.3.15] (clsModeButtonBox.setMode):
    //   if (abortForLockedVFO()) return;
    if (slice->locked()) { return; }
    const QString label = ModeButtonItem::modeLabel(index);
    if (label.isEmpty()) { return; }
    slice->setDspMode(SliceModel::modeFromName(label));
}

void MainWindow::onContainerFilterClicked(ContainerWidget* c, int index)
{
    SliceModel* slice = containerSlice(c);
    FilterPresetStore* store = m_radioModel ? m_radioModel->filterPresetStore() : nullptr;
    if (!slice || !store) { return; }
    // From Thetis MeterManager.cs:7952 [v2.10.3.15] (clsFilterButtonBox.MouseUp):
    //   Filter f = (Filter)((int)Filter.F1 + index); console.RX1Filter = f;
    // F1..F10 are the mode's presets, as the RX applet and the VFO flag
    // apply them. Var1 / Var2 (index 10, 11) have no NereusSDR filter yet.
    const QList<FilterPreset> presets = store->presetsForMode(slice->dspMode());
    if (index < 0 || index >= presets.size()) { return; }
    slice->setFilter(presets[index].low, presets[index].high);
}

QString MainWindow::containerBandStackReason()
{
    return tr("Band stacking is not ready.");
}

int MainWindow::containerFilterContextSlot(int index, int activePreset, int presetCount)
{
    const int slot = index >= 0 ? index : activePreset;
    return slot >= 0 && slot < presetCount ? slot : -1;
}

void MainWindow::onContainerFilterContext(ContainerWidget* c, int index)
{
    SliceModel* slice = containerSlice(c);
    FilterPresetStore* store = m_radioModel ? m_radioModel->filterPresetStore() : nullptr;
    if (!slice || !store) { return; }
    // From Thetis MeterManager.cs:7937-7945 [v2.10.3.15] (clsFilterButtonBox
    // right-click) and :14607 (the VFO display): console.PopupFilterContextMenu,
    // whose menu configures or resets the receiver's filters
    // (console.cs:39843-39888). NereusSDR's filter buttons edit presets one
    // at a time (the VFO flag's and RX applet's filter right-click, Stage
    // C2), so a container's filter right-click offers the same two items
    // for the preset it names.
    const DSPMode mode = slice->dspMode();
    const QList<FilterPreset> presets = store->presetsForMode(mode);
    int active = -1;
    for (int i = 0; i < presets.size(); ++i) {
        // RxApplet::updateFilterButtons tolerance: 50 Hz per edge.
        if (qAbs(slice->filterLow() - presets[i].low) <= 50
            && qAbs(slice->filterHigh() - presets[i].high) <= 50) {
            active = i;
            break;
        }
    }
    const int slot = containerFilterContextSlot(index, active, static_cast<int>(presets.size()));
    if (slot < 0) {
        showContainerButtonReason(index < 0
            ? tr("This slice's filter is not one of this mode's presets, so there is no "
                 "preset to edit.")
            : tr("This mode has no preset on that button."));
        return;
    }
    QMenu menu(this);
    menu.setStyleSheet(QString::fromLatin1(kPopupMenu));
    QAction* editAct = menu.addAction(tr("Edit this preset…"));
    QAction* resetAct = menu.addAction(tr("Reset this preset"));
    QAction* chosen = menu.exec(QCursor::pos());
    if (chosen == editAct) {
        auto* dlg = new FilterPresetEditDialog(store, mode, slot, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->exec();
    } else if (chosen == resetAct) {
        store->resetPreset(mode, slot);
    }
}

void MainWindow::onContainerAntennaSelected(ContainerWidget* c, int index)
{
    SliceModel* slice = containerSlice(c);
    if (!slice) { return; }
    // From Thetis MeterManager.cs:9974-9985 [v2.10.3.15] (clsAntennaButtonBox.MouseUp):
    //   if (index >= 0 && index <= 2) setRXAntenna(index, _rx1_band);
    //   if (index >= 3 && index <= 4) // ignore xvtr for now
    //       setAuxAntenna(index, _rx1_band, index == 3, index == 4, index == 5);
    //   if (index >= 6 && index <= 8)
    //       setTXAntenna(index - 6, _tx_band);//[2.10.3.9]MW0LGE fix, was using _rx1_band
    //   if (index == 9) toggleTxRxAnt();
    // The slice setters route to AlexController for the slice's band, as
    // the VFO flag's and RX applet's antenna pickers do (RadioModel's
    // rxAntennaChanged / txAntennaChanged handlers). Rx/Tx (index 9) has
    // no NereusSDR counterpart yet.
    if (index >= 0 && index <= 2) {
        slice->setRxAntenna(QStringLiteral("ANT%1").arg(index + 1));
    } else if (index >= 3 && index <= 4) {
        // Buttons 3-4 carry the radio's receive-only input labels
        // (AntennaButtonItem::setHpsdrSku); RadioModel maps the label back.
        const SkuUiProfile sku = skuUiProfileFor(m_radioModel->hardwareProfile().model);
        slice->setRxAntenna(sku.rxOnlyLabels[static_cast<size_t>(index - 3)]);
    } else if (index >= 6 && index <= 8) {
        // R-R3-49, R-R3-21 (parity Task 11): the transmit slice's txAntenna,
        // as the VFO flag's TX antenna button writes it: a slice setting,
        // written in a remote window too (the Core's slice follows), not
        // tied to the transmit permission.
        SliceModel* tx = m_radioModel->txBoundSlice();
        (tx ? tx : slice)->setTxAntenna(QStringLiteral("ANT%1").arg(index - 5));
    }
}

void MainWindow::onContainerTuneStepSelected(ContainerWidget* c, int index)
{
    SliceModel* slice = containerSlice(c);
    const int hz = TuneStepButtonItem::stepHz(index);
    if (!slice || hz <= 0) { return; }
    // From Thetis MeterManager.cs:8242 [v2.10.3.15] (clsTunestepButtons.MouseUp):
    //   _console.TuneStepIndex = index;
    slice->setStepHz(hz);
}

void MainWindow::onContainerFrequencyStep(ContainerWidget* c, int64_t deltaHz)
{
    SliceModel* slice = containerSlice(c);
    if (!slice && c && m_containerButtons) {
        // Fix wave M2: the wheel says why it does nothing, as a click does.
        showContainerButtonReason(ContainerButtonDispatcher::noSliceReason(containerControlRxSource(c)));
        return;
    }
    if (!slice || deltaHz == 0) { return; }
    // SliceModel::setFrequency refuses a locked slice itself.
    slice->setFrequency(slice->frequency() + static_cast<double>(deltaHz));
}

void MainWindow::openSetupAtPage(const QString& label)
{
    auto* dialog = createSetupDialog();
    if (dialog == nullptr) {
        return;  // the gate refused and has already said why
    }
    dialog->selectPage(label);
    dialog->show();
    dialog->raise();
}

void MainWindow::openAntennaSetup()
{
    auto* dialog = createSetupDialog();
    if (dialog == nullptr) {
        return;
    }
    // selectPage realizes the lazily built page before it returns.
    dialog->selectPage(QStringLiteral("Hardware Config"));
    if (auto* hw = dialog->findChild<HardwarePage*>()) {
        hw->showAntennaTab();
    }
    dialog->show();
    dialog->raise();
}

// Phase 3F Sub-Epic G T4: Diversity dialog (bench minimum). Lazy, one per
// window, kept alive across close so its own state survives a re-open
// (SliceModel persistence handles the real settings round-trip).
void MainWindow::openDiversityDialog()
{
    if (!m_diversityDialog) {
        m_diversityDialog = new DiversityDialog(m_radioModel, this);
        m_diversityDialog->setAttribute(Qt::WA_DeleteOnClose, false);
    }
    m_diversityDialog->show();
    m_diversityDialog->raise();
    m_diversityDialog->activateWindow();
}

// Phase 3P-II Phase 4 Task 97: PGXL power cap soft-alert toast.
//
// Fires a 5-second toast when peak forward power exceeds the
// operator-configured PGXL cap.  De-bounced: one toast per exceedance event
// (re-armed when fwd drops back below the cap threshold so a subsequent
// exceedance fires a fresh toast).
//
// Design reference:
//   docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-plan.md
//   Task 97 / design ss5.6.2 "TX power cap: soft alert only".
//
// R-R3-47 / R-R3-22: the rule (PGXL_PowerCapEnabled, PGXL_PowerCapW,
// one alert per exceedance) moved to StationAccessoryData::onForwardPower,
// which runs where the amp is. The window shows the alert when the
// object's alert count moves while the output is over the limit; the count
// it saw when the object first arrived is not an alert of its own.
void MainWindow::onPowerCapAlertChanged()
{
    const AccessoryDataModel* data = m_radioModel->accessoryDataModel();
    const qint64 count = data->powerCapAlertCount();
    if (count == m_powerCapAlertSeen) {
        return;
    }
    const bool fresh = count > m_powerCapAlertSeen;
    m_powerCapAlertSeen = count;
    if (!fresh || !data->powerCapExceeded() || data->powerCapAlertText().isEmpty()) {
        return;
    }
    showToast(data->powerCapAlertText(), ToastSeverity::Error, 5000);
}

// ── Phase 3P-II review fix C2: TX interlock warning/denial toasts ────────────
// Both slots display a 5-second notice so the operator knows why TX was
// warned or blocked.  The distinction: warning allows TX to proceed; denial
// means MOX was rejected.  Pattern mirrors onAmpMetersForPowerCap above
// (showToast + qCWarning(lcMeter)).
//
// These are the strongest argument for not using QStatusBar::showMessage
// here: both fire during transmit, and blanking the bottom bar would take
// the PA and TX badges away at the exact moment they matter.
void MainWindow::onTxInterlockWarning(const QString& reason)
{
    const QString msg = QString("TX interlock warning: %1").arg(reason);
    showToast(msg, ToastSeverity::Warning, 5000);
    qCWarning(lcMeter) << msg;
}

void MainWindow::onTxInterlockDenial(const QString& reason)
{
    const QString msg = QString("TX interlock blocked: %1").arg(reason);
    showToast(msg, ToastSeverity::Error, 5000);
    qCWarning(lcMeter) << msg;
}

// ── Task 3.6: CPU meter rate ─────────────────────────────────────────────────
// Live-applies the CPU meter update interval from GeneralOptionsPage spinbox.
// Restarts m_cpuTimer with the new period. hz is clamped to [1, 30] so a
// zero or negative value from a misconfigured spinbox cannot stop the timer.
void MainWindow::setCpuTimerIntervalHz(int hz)
{
    if (!m_cpuTimer) { return; }
    const int clamped = qBound(1, hz, 30);
    m_cpuTimer->setInterval(1000 / clamped);
}

// ── Task 3.6: ANAN-8000DLE volts/amps visibility ────────────────────────────
// Called when the "Show volts/amps in title bar" checkbox on Hardware →
// Radio Info is toggled.  Only has visible effect for ANAN-8000D radios
// (SystemTile's PA row is already hidden for non-MKII boards by the
// hardware gate in buildStatusBar(); this gives the user an additional
// opt-out).
void MainWindow::setVoltsAmpsVisible(bool visible)
{
    if (!m_systemTile) { return; }
    // SystemTile::clearPaVolts() clears only the volts flag; if a
    // temperature reading is also present (HL2-class row-sharing, design
    // §4.3) it keeps the tile's PA row alive, same intent as the old
    // refreshPaStackVisibility() symmetry. Idempotent, so no isVisible()
    // guard is needed the way the old m_paVoltLabel check had.
    if (!visible) {
        m_systemTile->clearPaVolts();
        if (m_chromeBar && m_chromeBarWidget) {
            m_chromeBar->setNaturalWidth(m_systemTile,
                                         m_systemTile->sizeHint().width());
            m_chromeBar->relayout(m_chromeBarWidget->width());
        }
    }
    // Note: toggling back to visible=true does not force-show the reading;
    // the next userAdc0Changed will re-show it. This avoids showing a stale
    // "—" value before the first ADC reading arrives.
}

// ---------------------------------------------------------------------------
// Phase 3M-3a-ii Batch 6 (Task 3) — wireSetupDialog
//
// Centralized helper called from every SetupDialog construction site.  Wires
// the dialog's cfcDialogRequested signal (forwarded from CfcSetupPage's
// [Configure CFC bands…] button) to TxApplet::requestOpenCfcDialog so the
// modeless TxCfcDialog instance owned by the TxApplet is reused.
//
// Pre-condition: m_txApplet is set (TxApplet is created during early UI
// build-out, well before any of the user-triggered SetupDialog opens).
// ---------------------------------------------------------------------------
void MainWindow::wireSetupDialog(SetupDialog* dialog)
{
    if (!dialog) { return; }
    // R-R3-21: Appearance > Meter Styles changes the S-meter on screen.
    const auto sMeter = [this]() {
        return m_appletPanel ? m_appletPanel->smeterWidget() : nullptr;
    };
    connect(dialog, &SetupDialog::sMeterFaceChanged, this, [sMeter](int face) {
        if (SMeterWidget* sm = sMeter()) {
            sm->setFaceStyle(static_cast<SMeterWidget::FaceStyle>(face));
        }
    });
    connect(dialog, &SetupDialog::sMeterPeakHoldChanged, this, [sMeter](bool on) {
        if (SMeterWidget* sm = sMeter()) { sm->setPeakHoldEnabled(on); }
    });
    connect(dialog, &SetupDialog::sMeterPeakDecayChanged, this, [sMeter](const QString& rate) {
        if (SMeterWidget* sm = sMeter()) { sm->setPeakDecayRate(rate); }
    });
    // ...and the other way: a right-click change on the S-meter shows on
    // the page while Setup is open.
    if (SMeterWidget* sm = sMeter()) {
        connect(sm, &SMeterWidget::settingsChanged, dialog, &SetupDialog::reloadMeterStyles);
    }
    connect(dialog, &SetupDialog::connectionsRequested,
            this, &MainWindow::connectionRequestedByOperator);
    connect(dialog, &SetupDialog::coreConnectionDetailsRequested,
            this, &MainWindow::showRemoteConnectionPanel);
    connect(dialog, &SetupDialog::coreDiagnosticsRequested,
            this, &MainWindow::openNetworkDiagnostics);
    if (m_txApplet) {
        connect(dialog, &SetupDialog::cfcDialogRequested,
                m_txApplet, &TxApplet::requestOpenCfcDialog);
    }
    // Phase 3P-II Phase 4 Task 95: propagate TGXL antenna label edits from
    // Setup -> Network -> TGXL Advanced -> Antenna Labels to the TunerApplet
    // antenna buttons so they update live without restarting.
    if (m_tunerApplet) {
        connect(dialog, &SetupDialog::tgxlAntennaLabelChanged,
                m_tunerApplet, &TunerApplet::onAntennaLabelChanged);
    }
    // Task 3.6: CPU meter rate live-apply.
    connect(dialog, &SetupDialog::cpuMeterRateChanged,
            this,   &MainWindow::setCpuTimerIntervalHz);
    connect(dialog, &SetupDialog::hideFeedbackLevelChanged, this, [this](bool hidden) {
        if (m_psaIndicator) { m_psaIndicator->setHideFeedback(hidden); }
    });
    connect(dialog, &SetupDialog::invertRedBluePsaChanged, this, [this](bool inverted) {
        if (m_psaIndicator) { m_psaIndicator->setInvertRedBlue(inverted); }
    });
    // Task 3.6: ANAN-8000DLE volts/amps live-apply.
    connect(dialog, &SetupDialog::anan8000DleVoltsAmpsChanged,
            this,   &MainWindow::setVoltsAmpsVisible);

#ifdef HAVE_WEBSOCKETS
    // Phase 3J-1 review P2.4: live-wire Setup → Network → TCI Server enable
    // checkbox to the running TciServer.  Previously the checkbox only wrote
    // AppSettings; the server required a manual restart to pick up the change.
    // Now toggling the checkbox immediately starts or stops the server.
    //
    // Auto-connect (QueuedConnection for cross-dialog safety): when `on` is
    // true, start on the persisted port; when false, stop.  If the server is
    // already in the requested state the calls are no-ops (double-start
    // returns false; stop() on a non-running server returns immediately).
    if (m_tciServer) {
        connect(dialog, &SetupDialog::tciServerEnableToggled,
                this, [this](bool on, quint16 port) {
                    if (on) {
                        // Re-read bind address from AppSettings so the start
                        // honors whatever the operator last picked in the
                        // bind-interface dropdown.  CatTciServerPage persists
                        // TciServerBindAddress on every combo change.
                        auto& s = AppSettings::instance();
                        const QString bindStr = s.value(
                            QStringLiteral("TciServerBindAddress"),
                            QStringLiteral("127.0.0.1")).toString();
                        QHostAddress bindAddr;
                        if (!bindAddr.setAddress(bindStr)) {
                            bindAddr = QHostAddress(QHostAddress::LocalHost);
                        }
                        // R-R3-48: this window's server and the Core's.
                        m_tciSwitch->setSwitch(true, port, bindAddr);
                    } else {
                        m_tciSwitch->setSwitch(false, port, QHostAddress(QHostAddress::LocalHost));
                    }
                });
        // Phase 3J-1 closeout Item 1 (2026-05-12): live-restart on bind /
        // port change.  CatTciServerPage emits this whenever the operator
        // picks a different interface from the dropdown or edits the port
        // spinbox.  If the server is running, restart it in place; if
        // stopped, the new values are already in AppSettings for next
        // start.  Mirrors the enable-toggled pattern above.
        connect(dialog, &SetupDialog::tciServerBindOrPortChanged,
                this, [this](const QString& bindStr, quint16 port) {
                    QHostAddress bindAddr;
                    if (!bindAddr.setAddress(bindStr)) {
                        bindAddr = QHostAddress(QHostAddress::LocalHost);
                    }
                    // R-R3-48: restarts this window's server if it runs,
                    // and gives the Core the new port while the switch is on.
                    m_tciSwitch->setPortOrBind(port, bindAddr);
                });
        // Phase 3J-1 closeout Item 2 (2026-05-12): "Show Log..." button.
        // The window is owned by MainWindow (lazy-constructed) so it
        // outlives this dialog closing -- WSJT-X sessions can take an
        // hour to settle and the operator wants the log window pinned.
        connect(dialog, &SetupDialog::tciShowLogRequested,
                this,   &MainWindow::showTciLogWindow);

        // Phase 3J-1 bench fix (2026-05-11): forward the TciServer reference
        // into the dialog so CatTciServerPage's Server group box title +
        // Status label update live as clients connect/disconnect and the
        // server starts/stops.  Mirrors Thetis Setup.cs:9491-9494
        // [v2.10.3.13] — TCIClientsConnectedChange updates `grpTCIServer.Text`.
        dialog->setTciServer(m_tciServer);
    }
#endif // HAVE_WEBSOCKETS
}

#ifdef HAVE_WEBSOCKETS
// Phase 3J-1 closeout Item 2 (2026-05-12): lazy-construct the TciLogWindow
// on first request, connect it to TciServer::messageLogged, and show it.
// Subsequent clicks just raise the existing window.  The window is owned
// by MainWindow so it survives Setup dialog close/reopen.
//
// Qt::QueuedConnection on the signal hookup ensures the TciServer's
// emit-side never blocks while the log view processes the entry --
// important during a busy WSJT-X session where ~10 frames/sec arrive
// from each direction.
void MainWindow::showTciLogWindow()
{
    if (!m_tciServer) {
        return;  // No-op in builds without HAVE_WEBSOCKETS or before init.
    }
    if (!m_tciLogWindow) {
        m_tciLogWindow = new TciLogWindow(this);
        connect(m_tciServer, &TciServer::messageLogged,
                m_tciLogWindow, [log = m_tciLogWindow](const QString& direction,
                                                       const QString& peer,
                                                       const QString& line, qint64 atMs) {
            log->appendEntry(direction, QStringLiteral("This window: %1").arg(peer),
                             line, atMs);
        },
                Qt::QueuedConnection);
        // R-R3-42: the operator's notices, in plain words, between the
        // wire lines (never sent to an app).
        connect(m_tciServer, &TciServer::operatorNotice, m_tciLogWindow,
                [log = m_tciLogWindow](const QString& peer, const QString& reason, bool) {
            log->appendEntry(QStringLiteral("note"),
                             QStringLiteral("This window: %1").arg(peer),
                             OperatorReasonText::forDisplay(reason),
                             QDateTime::currentMSecsSinceEpoch());
        }, Qt::QueuedConnection);
        // The Core sends its connected-app records, including each app's
        // latest command. Show changes beside this window's live wire log.
        if (m_radioModel && m_radioModel->role() == RadioModel::Role::Remote
            && m_radioModel->stationTciModel()) {
            auto* station = m_radioModel->stationTciModel();
            connect(station, &StationTciModel::clientsChanged, m_tciLogWindow,
                    [station, log = m_tciLogWindow,
                     seen = QHash<QString, QString>{}]() mutable {
                QHash<QString, QString> now;
                for (const StationTciClient& client : station->clients()) {
                    now.insert(client.id, client.lastCommand);
                    if (!client.lastCommand.isEmpty()
                        && seen.value(client.id) != client.lastCommand) {
                        log->appendEntry(QStringLiteral("in"),
                                         QStringLiteral("The Core: %1").arg(client.address),
                                         client.lastCommand, QDateTime::currentMSecsSinceEpoch());
                    }
                }
                seen = now;
            });
            const StationTciModel* state = station;
            m_tciLogWindow->appendEntry(
                QStringLiteral("note"), QStringLiteral("The Core"),
                state->listening() ? QStringLiteral("TCI server listening on port %1")
                                         .arg(state->port())
                                   : QStringLiteral("TCI server is not listening"),
                QDateTime::currentMSecsSinceEpoch());
        }
    }
    m_tciLogWindow->show();
    m_tciLogWindow->raise();
    m_tciLogWindow->activateWindow();
}
#else
void MainWindow::showTciLogWindow() {}  // no-op in non-WebSocket builds
#endif // HAVE_WEBSOCKETS

void MainWindow::wireSliceToSpectrum()
{
    // VFO flag crash lane (2026-09-30): Slice A by its id. This runs on
    // sliceAdded(0), which a remote window sees again when its Core closes
    // Slice A and makes it again; by then the active slice is another one,
    // and reading activeSlice() here re-wired that slice and gave the new
    // Slice A no flag. The pan is the one hosting Slice A (the active pan
    // when it has no pan key yet), as for every other slice's flag.
    SliceModel* slice = m_radioModel->sliceById(0);
    SpectrumWidget* host = slice ? spectrumForSlice(slice) : nullptr;
    if (!slice || !host) {
        return;
    }
    // The window-wide wiring below (applets, the dBm strip, the pan's first
    // view) is done once; a Slice A made again gets only its own wiring, so
    // nothing is connected twice and the pan's rate and zoom are left as
    // they are.
    const bool firstWiring = !m_sliceASpectrumWired;
    m_sliceASpectrumWired = true;

    // Set initial spectrum display. Phase 3G-12: preserve the user's
    // persisted zoom level if present. SpectrumWidget::loadSettings()
    // has already read "DisplayBandwidth" from AppSettings into
    // m_bandwidthHz by this point. If the loaded value is sensible
    // (between 10 kHz and the DDC sample rate), keep it; otherwise
    // fall back to the full-span default (768 kHz = sample rate).
    const double freq = slice->frequency();
    if (firstWiring) {
        host->setDdcCenterFrequency(freq);
        // Rate BEFORE the span, so the extended ceiling below is computed against
        // a real DDC rate rather than the widget's construction default.
        host->setSampleRate(768000.0);

        // Same ceiling as setDisplayWindowClamped, spelled out here because the
        // FALLBACK differs: an out-of-range stored value goes to the full-span
        // 768 kHz default rather than to the ceiling, and there is a 10 kHz floor
        // that only applies to a restored value. The ceiling is the shared part.
        //
        // The upper bound is that extended ceiling, not a hardcoded 768 kHz.
        //
        // saveSettings can now record a span above the DDC rate, because that is
        // exactly what extended view is, and this test rejected every one of them
        // and fell back to full span. So an extended zoom was always discarded at
        // startup even though it had been persisted correctly. maxZoomOutBandwidth
        // Hz returns the DDC rate when extended view is off, which is what the
        // old literal meant to say. Codex, PR #318.
        const double loadedBw = host->bandwidth();
        const double ceiling  = host->maxZoomOutBandwidthHz();
        const double initialBw = (loadedBw >= 10000.0 && loadedBw <= ceiling)
                                 ? loadedBw : 768000.0;
        host->setFrequencyRange(freq, initialBw);
        host->setVfoFrequency(freq);
        host->setFilterOffset(slice->filterLow(), slice->filterHigh());
        host->setStepSize(slice->stepHz());
    }

    // --- Create floating VFO flag widget (AetherSDR pattern) ---
    // Slice A's flag is built and wired by createSliceFlag, exactly like every
    // other slice's. This used to construct it by hand and then wire 67
    // connects to it, while createSliceFlag wired far fewer -- two paths, one
    // incomplete, which is why 23 flag controls were dead on B/C/D. One path
    // now, so the two cannot drift again.
    VfoWidget* vfo = createSliceFlag(slice, host);
    if (!vfo) { return; }

    // Mode-driven applet surfaces. These are SINGLE global widgets (one RADE
    // applet, one PhoneCw applet), so they follow the ACTIVE slice's mode and
    // stay here rather than moving into the per-flag path.
    //
    // Restored after the flag-path unification deleted the handler that
    // carried them alongside the flag's own setMode: switching to CW or FM
    // stopped changing the PhoneCw page, and RADE stopped revealing its
    // applet. The passband and TX-mode half of that handler now lives in
    // createSliceFlag, per pan.
    connect(slice, &SliceModel::dspModeChanged, this, [this](DSPMode mode) {
        // Phase 3R L2: RADE applet shows for either RADE sideband, IN ADDITION
        // to PhoneCwApplet -- bench feedback showed PhoneCw hosts the mic gain
        // slider, which RADE TX still needs. Routed through the visibility
        // controller so the wrapper and the menu entry agree, and the user's
        // persisted preference survives the mode change.
        const bool isRade = (mode == DSPMode::RADE_U
                             || mode == DSPMode::RADE_L);
        if (m_appletVis) {
            m_appletVis->setAvailable(QStringLiteral("Rade"), isRade);
        }
        if (m_phoneCwApplet) {
            m_phoneCwApplet->setVisible(true);  // always visible
            switch (mode) {
                case DSPMode::CWL:
                case DSPMode::CWU:
                    m_phoneCwApplet->showPage(1);  // CW page
                    break;
                case DSPMode::FM:
                    m_phoneCwApplet->showPage(2);  // FM page
                    break;
                default:
                    m_phoneCwApplet->showPage(0);  // Phone page
                                                   // (incl. RADE_U / RADE_L)
                    break;
            }
        }
    });
    vfo->setSlice(slice);
    vfo->setFrequency(freq);
    vfo->setMode(slice->dspMode());
    vfo->setFilter(slice->filterLow(), slice->filterHigh());
    vfo->setAgcMode(slice->agcMode());
    vfo->setAfGain(slice->afGain());
    vfo->setRfGain(slice->rfGain());
    vfo->setRxAntenna(slice->rxAntenna());
    vfo->setTxAntenna(slice->txAntenna());
    vfo->setStepHz(slice->stepHz());

    // Phase 3P-I-a T15 — push board caps into VFO Flag so ANT buttons
    // hide on HL2/Atlas.
    // Phase 3P-I-b T9 — also push HPSDRModel so the BYPS button gates
    // correctly (ANAN10/ANAN8000D/G2/G2_1K suppress it despite hasRxBypassRelay).
    vfo->setBoardCapabilities(m_radioModel->boardCapabilities());
    vfo->setHpsdrSku(m_radioModel->hardwareProfile().model);
    vfo->setRxBypassActive(m_radioModel->alexAntennaFacade()->rxOutOnTx());
    // Phase 3F closeout — give Slice A's VfoWidget the RadioModel pointer so
    // contextMenuEvent builds AntennaPickerMenu instead of the stub fallback.
    vfo->setRadioModel(m_radioModel);
    applyFlagTransmitGate(vfo);
    vfo->setRxBypassPermitted(rxBypassPermitted(), rxBypassUnavailableReason());
    // VFO flag crash lane fix round (2026-10-01): the currentRadioChanged
    // board-caps connect moved into createSliceFlag, so a Slice A flag
    // rebuilt on a pan move keeps it.

    // Phase 3F Sub-Epic C Task 9: VFO TX badge click → arbiter handoff.
    // VfoWidget emits txHandoffRequested(sliceIndex); MainWindow forwards to
    // RadioModel::txSliceArbiter()->requestHandoff(), which drops MOX before
    // flipping the TX-bound slice (RF-safe). Sub-Epic D wires the matching
    // reverse path (txBoundSliceChanged then updates all flag badges) in T10.

    // Phase 3F Sub-Epic E Task 4: VfoWidget context-menu intent signals.
    // Routes to RadioModel::requestSliceSampleRate / FilterPolicyDialog /
    // RadioModel::removeSlice. Phase 3F closeout: antennaChangeRequested is
    // now live, fired by AntennaPickerMenu (Sub-Epic E Task 5 consumer wire).
    //
    // Phase 3F Sub-Epic I closeout, defect G2: same rewire as the secondary
    // flags in createSliceFlag. The rate belongs to the DDC stream, so the
    // request goes to the stream rather than into a per-slice property
    // nothing downstream reads.
    // Phase 3F closeout — AntennaPickerMenu pick forwards to SliceModel::setRxAntenna.

    // Phase 3P-I-b T9: VFO BYPS button ↔ AlexController::rxOutOnTx.
    // Group B fix wave: through the Alex facade, the Core's in a remote
    // window (see createSliceFlag).
    // VFO flag crash lane fix round (2026-10-01): the rxOutOnTxChanged ->
    // setRxBypassActive connect that sat here duplicated createSliceFlag's,
    // so each change reached Slice A's flag twice. createSliceFlag's stays.

    // Stage C2: wire FilterPresetStore so VFO flag filter buttons use user overrides.
    vfo->setFilterPresetStore(m_radioModel->filterPresetStore());

    // 2026-05-11 bench: wire EOO-decoded RADE speaker callsign to the
    // VFO flag SNR row so "<call> ● <snr>dB" replaces "RADE ● <snr>dB"
    // whenever we have a decoded callsign for the current slice.  Sticky
    // until next decode replaces it; SliceModel clears the field when
    // setDspMode leaves RADE_U/RADE_L (A + D semantics per bench design
    // 2026-05-11).  Seed the current cached value once so a slice that
    // already holds a decoded callsign (e.g. from before the user opened
    // a panadapter container) paints correctly on first show.
    // VFO flag crash lane fix round (2026-10-01): the seed is in
    // createSliceFlag now, so a rebuilt Slice A flag gets it too.

    // --- Slice → spectrum display ---

    // VFO frequency change → move VFO marker
    // In CTUN mode (SmartSDR-style): pan stays fixed, VFO moves within it.
    // In traditional mode: pan follows VFO (auto-scroll handled in setVfoFrequency).
    // Band changes (large jumps) always recenter regardless of mode.


    // Task 42 (Phase 3P-II): reconfigure the Max Bin detector whenever the
    // IF passband changes so the passband-strongest-bin reading follows the
    // active filter window.
    //
    // 100 ms QTimer::singleShot debounce: rapid filter edge drags (e.g.
    // VFO flag drag) would otherwise call SetupDetectMaxBin on every
    // intermediate sample, which re-initialises the WDSP analyzer DSP
    // block at display-interrupt rate and wastes CPU.
    //
    // WdspEngine::setupMaxBinDetector wraps Thetis Console/dsp.cs:846-847
    // [@501e3f5] SetupDetectMaxBin; display channel = 0 (single panadapter).
    // Sample rate: primaryFftEngine()->sampleRate() at call time, which
    // reflects the currently active DDC bandwidth.  Primary engine because
    // the detector is set up with disp=0 (single display channel).
    // Frame rate: primaryFftEngine()->outputFps() * 1.1 matches Thetis
    //   console.cs:51150 [@501e3f5]: (int)Math.Max(1, _display_fps * 1.1f).
    connect(slice, &SliceModel::filterChanged, this, [this, slice](int low, int high) {
        // VFO flag crash lane: the slice is tracked, not held raw. A slice
        // closed within the 100 ms (a Core closing Slice A in a remote
        // window) has no detector setup left to do; the window stays the
        // timer's context for its own lifetime.
        QTimer::singleShot(100, this, [this, slice = QPointer<SliceModel>(slice), low, high]() {
            if (!slice) { return; }
            FFTEngine* fft = primaryFftEngine();
            if (!m_radioModel || !fft) { return; }
            WdspEngine* eng = m_radioModel->wdspEngine();
            if (!eng) { return; }
            const double rate = fft->sampleRate();
            const int fps = qMax(1, static_cast<int>(fft->outputFps() * 1.1f));
            eng->setupMaxBinDetector(/*disp=*/0, /*ss=*/0, /*LO=*/0,
                                     rate,
                                     static_cast<double>(low),
                                     static_cast<double>(high),
                                     /*tauSeconds=*/0.5,
                                     fps);
            // Re-sync the CTUN slice offset after every setup call.  Filter
            // changes don't move the slice, but they re-run setupMaxBinDetector
            // and we want the offset to be authoritative against the current
            // slice freq vs DDC center -- not whatever stale offset was last
            // pushed by frequencyChanged.  Without this re-sync, a filter
            // change immediately after a CTUN tune could leave the detector
            // pointing at the wrong bins until the user nudges the VFO again.
            const double ddcCenter = activeSpectrumWidget()->ddcCenterFrequency();
            const double sliceFreq = slice->frequency();
            eng->setMaxBinSliceOffsetHz(/*disp=*/0, sliceFreq - ddcCenter);
        });
    });

    // Plan 4 D9 (Cluster E): initial TX mode push so the overlay has the right
    // IQ-space sign convention before the first paint.
    //
    // VFO flag crash lane fix round (2026-10-01): this block, the XIT push,
    // the dBm strip and the CTUN lock below name Slice A's own pan (`host`,
    // or spectrumForSlice at call time), as the view seed and the flag do,
    // instead of the active pan.
    if (firstWiring) {
        host->setTxMode(slice->dspMode());
        // Initial XIT offset push + signal wires below so the TX overlay
        // centers on the actual TX frequency (RX VFO + XIT) rather than the
        // RX VFO alone.  Codex review feedback on PR #166.
        const int initialXitOffset = slice->xitEnabled() ? slice->xitHz() : 0;
        host->setTxVfoOffsetHz(initialXitOffset);
    }

    // XIT-enabled toggle and XIT-Hz changes both feed the spectrum's TX
    // overlay center.  When enabled flips off, the offset goes to zero;
    // when on, the offset tracks xitHz.
    auto pushXitOffset = [this, slice]() {
        SpectrumWidget* sw = spectrumForSlice(slice);
        if (!sw) { return; }
        sw->setTxVfoOffsetHz(slice->xitEnabled() ? slice->xitHz() : 0);
    };
    connect(slice, &SliceModel::xitEnabledChanged, this,
            [pushXitOffset](bool /*enabled*/) { pushXitOffset(); });
    connect(slice, &SliceModel::xitHzChanged, this,
            [pushXitOffset](int /*hz*/) { pushXitOffset(); });








    // --- SliceModel → VfoWidget: RIT/XIT inbound (S1.8a stubs) ---




    // --- SliceModel → VfoWidget: DSP tab inbound (S1.8b) ---

    // Sub-epic C-1: NR bank sync — VfoWidget::setSlice also connects activeNrChanged
    // via onActiveNrChanged for the full 7-button bank; this redundant connection is
    // removed to avoid double-firing. setSlice handles both initial sync and updates.
    // (Legacy setNr2Enabled call removed here — onActiveNrChanged covers NR2.)




    // --- VFO flag → slice ---




    // Shift+click on a filter preset on the flag — snap TX passband to
    // match the RX preset's audio Hz range (Thetis-style alignment shortcut).






    // NB cycling — nbModeCycled fires on user click; cycle the mode through
    // Off → NB → NB2 → Off via SliceModel. SliceModel's nbModeChanged feeds
    // back to setNbMode() (wired in the inbound block above).
    // From Thetis console.cs:43513 [v2.10.3.13].

    // NR/ANF → RxChannel directly (not SliceModel properties)

    // --- VfoWidget → SliceModel: DSP tab outbound (S1.8b) ---

    // --- SliceModel → VfoWidget: Audio tab inbound (S1.8c stubs) ---

    // --- VfoWidget → SliceModel: Audio tab outbound (S1.8c stubs) ---

    // --- VfoWidget → MainWindow: open Setup dialog to AGC/ALC page ---

    // --- VfoWidget → Setup → DSP → NB/SNB page (right-click on NB or SNB).
    // Mirrors Thetis chkNB_MouseDown / chkDSPNB2_MouseDown (console.cs:44447
    // [v2.10.3.13]) which call ShowSetupTab(NB_Tab).

    // --- VfoWidget → Setup → DSP → NR/ANF page (Task 18, Sub-epic C-1).
    // Emitted from DspParamPopup "More Settings…" on any NR bank button.
    // Mirrors Thetis chkNR_MouseDown (console.cs [v2.10.3.13]) which calls
    // ShowSetupTab(NR_Tab). Sub-tab selection per NrSlot is deferred to Task 17.

    // --- VfoWidget AUTO button → SliceModel auto-AGC toggle ---

    // --- SliceModel auto-AGC state → update visuals on both widgets ---

    // --- Noise floor fast-attack triggers (slice is guaranteed non-null here) ---
    {
        auto* nfTracker = m_radioModel->noiseFloorTracker();
        if (nfTracker) {
            // From Thetis v2.10.3.13 display.cs:905 — freq change > 0.5
            connect(slice, &SliceModel::frequencyChanged,
                    this, [nfTracker](double /*hz*/) {
                nfTracker->triggerFastAttack();
            });
            // From Thetis v2.10.3.13 display.cs:880 — mode change
            connect(slice, &SliceModel::dspModeChanged,
                    this, [nfTracker](NereusSDR::DSPMode /*mode*/) {
                nfTracker->triggerFastAttack();
            });
        }
    }

    // --- VfoWidget → SliceModel: RIT/XIT outbound (S1.8a stubs) ---




    // --- VfoWidget → SliceModel: STEP cycle (S1.8a — wires to live setStepHz) ---

    // --- VfoWidget → SliceModel: lock state (S1.8a — verifying edge exists) ---

    // --- SliceModel → VfoWidget: lock state inbound (S1.8a review — I3) ---
    // Without this edge, programmatic changes to SliceModel::locked (e.g. from
    // a future CAT/TCI command) would not be reflected in either lock button.

    // Phase 3F (Bug 2): wire Slice A's floating ✕ close button to removeSlice.
    // Slice A's close button is hidden by VfoWidget (last-slice invariant), so
    // this can only fire if a future change un-hides it; removeSlice refuses
    // to remove the final slice regardless, so this is safe and consistent
    // with the secondary-flag wiring in createSliceFlag().


    // The four spectrum controls that act on a slice (click-to-tune, filter-
    // edge drag, pan drag, CTUN toggle) used to be connected here, to
    // activeSpectrumWidget(), with lambdas capturing the `slice` above by
    // value. That pointer is Slice A and never moved, so on pan-0 all four
    // drove Slice A for the life of the session however many times the
    // operator selected another flag. Bench-caught 2026-07-28.
    //
    // They now live in wireSpectrumSliceControls, which ensureOverlayPanels
    // runs for every pan including this one, and which resolves its target
    // through sliceForPan(panId) on each signal instead of capturing it.
    // verify-no-captured-slice-spectrum-wiring.py keeps them from coming back.

    // --- dBm range strip → PanadapterModel (per-band grid storage + AppSettings) ---
    if (firstWiring) {
        connect(host, &SpectrumWidget::dbmRangeChangeRequested,
                this, [this](float minDbm, float maxDbm) {
            if (m_radioModel && !m_radioModel->panadapters().isEmpty()) {
                PanadapterModel* pan = m_radioModel->panadapters().first();
                pan->setdBmFloor(static_cast<int>(minDbm));
                pan->setdBmCeiling(static_cast<int>(maxDbm));
            }
        });

        // Set initial lock state
        m_radioModel->receiverManager()->setDdcFrequencyLocked(
            host->ctunEnabled());
    }

    // Position the VFO flag
    host->updateVfoPositions();

    // Remote Daemon R2 Task 12: the "S-meter -> VfoWidget level bar" connect
    // that used to live here (MeterPoller::smeterUpdated -> vfo->setSmeter)
    // is deleted, not merely moved -- it was redundant with, and always
    // immediately overwritten within the same poll() tick by,
    // createSliceFlag()'s own per-slice wiring below `vfo` (this slice IS
    // Slice A; wireSliceToSpectrum obtains `vfo` from createSliceFlag()
    // above). That wiring now connects SliceModel::signalStrengthDbmChanged
    // directly, which already covers Slice A -- see createSliceFlag()'s
    // comment for the full reasoning and for why the old sliceSmeterUpdated
    // filter-by-id mechanism this also used to duplicate is gone too.

    // --- Wire RxApplet to active slice ---
    // Once: a Slice A made again is bound by the window's RX slice refresh
    // (refreshSliceChooser), which follows the slice this window receives.
    if (firstWiring && m_rxApplet) {
        m_rxApplet->setSlice(slice);
        refreshAutoAgcVisuals(m_radioModel, slice,
                    m_vfoWidgetsBySlice.value(slice->sliceIndex()), m_rxApplet);

        // AUTO button toggle → SliceModel
        connect(m_rxApplet, &RxApplet::autoAgcToggled, this, [this](bool enabled) {
            if (SliceModel* active = activeSliceForWindow()) {
                active->setAutoAgcEnabled(enabled);
            }
        });

        // Right-click AGC-T slider → open Setup dialog to AGC/ALC page
        connect(m_rxApplet, &RxApplet::openSetupRequested, this, [this]() {
            auto* dialog = createSetupDialog();
            if (dialog == nullptr) {
                return;  // the gate refused and has already said why
            }
            dialog->selectPage(QStringLiteral("AGC/ALC"));
            dialog->show();
        });

        // RxApplet openNbSetupRequested wiring removed 2026-04-22 —
        // RxApplet no longer hosts any NB controls (strict Thetis parity).
        // VfoWidget::openNbSetupRequested above handles the NB→Setup hop.
    }

    // --- PhoneCwApplet → Setup → Transmit → DEXP/VOX page (Phase 3M-3a-iii Task 15).
    // Right-click on the DEXP [ON] button on the Phone tab opens the
    // SetupDialog and jumps to the DexpVoxPage leaf (Task 14).  Mirrors
    // the SpeechProcessorPage cross-link pattern (TransmitSetupPages.h:201).
    // (VOX-button right-click moved to TxApplet 2026-05-04 with the rest
    // of the VOX surface — see the TxApplet connect just below.)
    if (firstWiring && m_phoneCwApplet) {
        connect(m_phoneCwApplet, &PhoneCwApplet::openSetupRequested, this,
                [this](const QString& /*category*/, const QString& page) {
            auto* dialog = createSetupDialog();
            if (dialog == nullptr) {
                return;  // the gate refused and has already said why
            }
            dialog->selectPage(page);
            dialog->show();
            dialog->raise();
        });
    }

    // --- TxApplet → Setup → Transmit → DEXP/VOX page (3M-3a-iii bench polish 2026-05-04).
    // Right-click on the VOX button (relocated from PhoneCwApplet) opens
    // the same DexpVoxPage leaf.  Same lambda body as the PhoneCwApplet
    // connect above.
    if (firstWiring && m_txApplet) {
        connect(m_txApplet, &TxApplet::openSetupRequested, this,
                [this](const QString& /*category*/, const QString& page) {
            auto* dialog = createSetupDialog();
            if (dialog == nullptr) {
                return;  // the gate refused and has already said why
            }
            dialog->selectPage(page);
            dialog->show();
            dialog->raise();
        });
    }

    // --- Wire overlay Band flyout to RadioModel band-click handler (#118) ---
    // The signal still carries legacy (name, freqHz, mode) args for
    // backwards-compat with SpectrumOverlayPanel's kBands table, but the
    // handler now owns seed/restore policy — only the name is consulted.
    // Previously this lambda called setFrequency and silently discarded
    // the mode arg, which was the #118 reproducer (80m click moved VFO
    // but left mode stale).
    // Wired per strip in ensureOverlayPanels() now, so a band click acts on the
    // pan it was clicked on rather than on whichever slice happens to be
    // active. The handler there keeps this one's semantics (name only; the
    // legacy freqHz / mode args stay for SpectrumOverlayPanel's kBands table).

    // Accessory frequency/mode propagation belongs to RadioModel's TX-bound
    // slice wiring, so local and headless Core use the same owner (R-R3-22).

    // Phase 3P-II review fix C1: keep TunerApplet m_currentBand in sync so
    // right-click Save/Recall/Clear actions always address the actual current
    // (antenna, band) slot rather than the Band::Band20m default.
    if (m_tunerApplet) {
        connect(slice, &SliceModel::bandChanged,
                m_tunerApplet, &TunerApplet::setBand);
        // Seed with the slice's current band so the first context-menu open
        // before any band crossing is already correct.
        m_tunerApplet->setBand(bandFromFrequency(slice->frequency()));
    }
}

// ── CPU usage source toggle ──────────────────────────────────────────────────
// Right-click menu on m_systemTile — System / App radio choice.
// Mirrors Thetis's toolStripDropDownButton_CPU with systemToolStripMenuItem
// and thetisOnlyToolStripMenuItem (console.cs:44230-44247). Persists the
// choice in AppSettings under "CpuShowSystem".
void MainWindow::onCpuMenuRequested(const QPoint& localPos)
{
    if (!m_systemTile) { return; }

    QMenu menu(this);
    QAction* sysAct = menu.addAction(tr("System"));
    sysAct->setCheckable(true);
    sysAct->setChecked(m_cpuShowSystem);
    QAction* appAct = menu.addAction(tr("App (NereusSDR)"));
    appAct->setCheckable(true);
    appAct->setChecked(!m_cpuShowSystem);

    // Parity ruling C9: a remote window's row shows this computer's CPU and
    // the Core's; cycle between them, or pin either.
    QAction* cycleAct = nullptr;
    QAction* thisAct = nullptr;
    QAction* coreAct = nullptr;
    if (m_radioModel && !m_radioModel->ownsLocalDsp()) {
        menu.addSeparator();
        const CpuRowCycler::Source source = m_cpuRowCycler.source();
        cycleAct = menu.addAction(tr("Cycle"));
        cycleAct->setCheckable(true);
        cycleAct->setChecked(source == CpuRowCycler::Source::Cycle);
        thisAct = menu.addAction(tr("This computer"));
        thisAct->setCheckable(true);
        thisAct->setChecked(source == CpuRowCycler::Source::ThisComputer);
        coreAct = menu.addAction(tr("Core"));
        coreAct->setCheckable(true);
        coreAct->setChecked(source == CpuRowCycler::Source::Core);
    }

    QAction* chosen = menu.exec(m_systemTile->mapToGlobal(localPos));
    if (!chosen) { return; }

    if (chosen == cycleAct || chosen == thisAct || chosen == coreAct) {
        const CpuRowCycler::Source source = chosen == thisAct ? CpuRowCycler::Source::ThisComputer
            : chosen == coreAct ? CpuRowCycler::Source::Core
                                : CpuRowCycler::Source::Cycle;
        m_cpuRowCycler.setSource(source);
        AppSettings::instance().setValue(QStringLiteral("CpuRowSource"),
                                         CpuRowCycler::sourceKey(source));
        refreshCpuRow(0);
        return;
    }

    const bool newSys = (chosen == sysAct);
    if (newSys == m_cpuShowSystem) { return; }

    m_cpuShowSystem = newSys;
    AppSettings::instance().setValue(
        QStringLiteral("CpuShowSystem"),
        newSys ? QStringLiteral("True") : QStringLiteral("False"));

    // Reset delta state and smoothing so the next reading starts cleanly.
    m_cpuSmoothedPct = 0.0;
    m_cpuProcPrevWallUs = 0;
    m_cpuProcPrevUserUs = 0;
    m_cpuProcPrevSysUs = 0;
    m_cpuSysPrevTotal = 0;
    m_cpuSysPrevIdle = 0;
    // SystemTile's CPU row does start with a "—" placeholder
    // (SystemTile.cpp constructor), but setCpuPercent() only takes a
    // numeric value and there's no way to ask for that placeholder again
    // once the timer is running; 0% is the closest equivalent to a
    // reset-to-placeholder display until the next timer tick.
    m_systemTile->setCpuPercent(0.0);
    refreshCpuRow(0);
}

void MainWindow::refreshCpuRow(qint64 elapsedMs)
{
    if (!m_systemTile) { return; }
    if (!m_radioModel || m_radioModel->ownsLocalDsp()) {
        m_systemTile->setCpuPercent(m_cpuSmoothedPct);
        return;
    }
    // Parity ruling C9: in a remote window, this computer's CPU and the
    // Core's, each labelled, cycling, pinned or held on a hot reading.
    m_cpuRowCycler.setThisComputer(m_cpuSmoothedPct);
    QString reason = QStringLiteral("The Core's CPU reading is not current.");
    std::optional<double> core;
    if (m_remoteTelemetry) {
        core = RemoteTelemetryController::coreCpuPercent(m_remoteTelemetry->current(),
                                                         m_cpuShowSystem, &reason);
    }
    m_cpuRowCycler.setCore(core, reason);
    m_cpuRowCycler.advance(elapsedMs);
    m_systemTile->setCpuRow(m_cpuRowCycler.row());
}

double MainWindow::readProcessCpuPercent()
{
    // Per-platform "process CPU time since boot" readers — return user +
    // kernel time consumed by this process expressed in microseconds.
    // POSIX (macOS / Linux) uses getrusage; Windows uses GetProcessTimes
    // and converts the FILETIME tick counter (100 ns) to microseconds.
    qint64       userUs = 0;
    qint64       sysUs  = 0;
    const qint64 nowUs  = QDateTime::currentMSecsSinceEpoch() * 1000LL;

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    struct rusage ru{};
    if (getrusage(RUSAGE_SELF, &ru) != 0) { return 0.0; }
    auto toUs = [](const struct timeval& tv) -> qint64 {
        return static_cast<qint64>(tv.tv_sec) * 1'000'000LL
             + static_cast<qint64>(tv.tv_usec);
    };
    userUs = toUs(ru.ru_utime);
    sysUs  = toUs(ru.ru_stime);
#elif defined(Q_OS_WIN)
    FILETIME ftCreation{}, ftExit{}, ftKernel{}, ftUser{};
    if (!GetProcessTimes(GetCurrentProcess(),
                         &ftCreation, &ftExit, &ftKernel, &ftUser)) {
        return 0.0;
    }
    auto fileTimeToUs = [](const FILETIME& ft) -> qint64 {
        ULARGE_INTEGER u{};
        u.LowPart  = ft.dwLowDateTime;
        u.HighPart = ft.dwHighDateTime;
        return static_cast<qint64>(u.QuadPart / 10);  // 100 ns -> µs
    };
    userUs = fileTimeToUs(ftUser);
    sysUs  = fileTimeToUs(ftKernel);
#else
    return 0.0;
#endif

    if (m_cpuProcPrevWallUs == 0) {
        // First-call sentinel — capture baseline, return 0 this round.
        m_cpuProcPrevWallUs = nowUs;
        m_cpuProcPrevUserUs = userUs;
        m_cpuProcPrevSysUs  = sysUs;
        return 0.0;
    }

    const qint64 wallDelta = nowUs - m_cpuProcPrevWallUs;
    if (wallDelta <= 0) { return 0.0; }

    const qint64 cpuDelta = (userUs - m_cpuProcPrevUserUs)
                          + (sysUs  - m_cpuProcPrevSysUs);

    m_cpuProcPrevWallUs = nowUs;
    m_cpuProcPrevUserUs = userUs;
    m_cpuProcPrevSysUs  = sysUs;

    return 100.0 * static_cast<double>(cpuDelta)
                 / static_cast<double>(wallDelta);
}

double MainWindow::readSystemCpuPercent()
{
    // Per-platform "system CPU time since boot" readers. The CPU usage
    // formula is the same across all three: percent = 100 * (1 - dIdle / dTotal).
    // What differs is how each OS exposes the underlying tick counters.
    //
    // - macOS: host_processor_info(PROCESSOR_CPU_LOAD_INFO) → per-CPU
    //   tick counters; sum across cores.
    // - Linux: /proc/stat first line "cpu  user nice system idle iowait
    //   irq softirq steal guest guest_nice" — total = sum, idle = the
    //   `idle` field (not iowait, matching `top`/`htop` convention).
    // - Windows: GetSystemTimes → idle/kernel/user as FILETIMEs (100 ns).
    //   Note kernel time on Windows *includes* idle, so total = kernel +
    //   user; the percent formula above still holds.
    quint64 totalNow = 0;
    quint64 idleNow  = 0;

#if defined(Q_OS_MAC)
    natural_t                 cpuCount = 0;
    processor_info_array_t    info     = nullptr;
    mach_msg_type_number_t    numInfo  = 0;

    if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO,
                            &cpuCount, &info, &numInfo) != KERN_SUCCESS) {
        return 0.0;
    }

    auto* cpus = reinterpret_cast<processor_cpu_load_info_t>(info);
    for (natural_t i = 0; i < cpuCount; ++i) {
        for (int s = 0; s < CPU_STATE_MAX; ++s) {
            totalNow += cpus[i].cpu_ticks[s];
        }
        idleNow += cpus[i].cpu_ticks[CPU_STATE_IDLE];
    }

    vm_deallocate(mach_task_self(),
                  reinterpret_cast<vm_address_t>(info),
                  static_cast<vm_size_t>(numInfo) * sizeof(integer_t));
#elif defined(Q_OS_LINUX)
    QFile f(QStringLiteral("/proc/stat"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { return 0.0; }
    const QByteArray line = f.readLine();
    f.close();

    // Tokenize on whitespace; first token is "cpu", remaining are tick
    // counts. Empty entries from the doubled space after "cpu" get filtered.
    const QList<QByteArray> rawParts = line.split(' ');
    QList<quint64> vals;
    vals.reserve(10);
    for (int i = 1; i < rawParts.size() && vals.size() < 10; ++i) {
        if (rawParts[i].isEmpty()) { continue; }
        bool ok = false;
        const quint64 v = rawParts[i].toULongLong(&ok);
        if (ok) { vals.append(v); }
    }
    if (vals.size() < 4) { return 0.0; }
    for (auto v : vals) { totalNow += v; }
    idleNow = vals[3];   // idle field; iowait NOT counted as idle (top convention)
#elif defined(Q_OS_WIN)
    FILETIME ftIdle{}, ftKernel{}, ftUser{};
    if (!GetSystemTimes(&ftIdle, &ftKernel, &ftUser)) { return 0.0; }
    auto fileTimeToTicks = [](const FILETIME& ft) -> quint64 {
        ULARGE_INTEGER u{};
        u.LowPart  = ft.dwLowDateTime;
        u.HighPart = ft.dwHighDateTime;
        return static_cast<quint64>(u.QuadPart);
    };
    const quint64 idle   = fileTimeToTicks(ftIdle);
    const quint64 kernel = fileTimeToTicks(ftKernel);   // includes idle
    const quint64 user   = fileTimeToTicks(ftUser);
    totalNow = kernel + user;
    idleNow  = idle;
#else
    return 0.0;
#endif

    if (m_cpuSysPrevTotal == 0) {
        // First-call sentinel — capture baseline, return 0 this round.
        m_cpuSysPrevTotal = totalNow;
        m_cpuSysPrevIdle  = idleNow;
        return 0.0;
    }

    const quint64 totalDelta = totalNow - m_cpuSysPrevTotal;
    const quint64 idleDelta  = idleNow  - m_cpuSysPrevIdle;
    m_cpuSysPrevTotal = totalNow;
    m_cpuSysPrevIdle  = idleNow;

    if (totalDelta == 0) { return 0.0; }
    return 100.0 * (1.0 - static_cast<double>(idleDelta)
                              / static_cast<double>(totalDelta));
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);

    // Update axis-lock positions for overlay-docked containers
    if (m_mainSplitter && m_containerManager) {
        // Use the spectrum pane (first splitter child) as reference
        QWidget* spectrumPane = m_mainSplitter->widget(0);
        if (spectrumPane) {
            m_hDelta = spectrumPane->width();
            m_vDelta = spectrumPane->height();
            m_containerManager->updateDockedPositions(m_hDelta, m_vDelta);
        }
    }

    // Single layout authority for the banner (design §5). One relayout()
    // call per resize; no re-measure mid-decision, no deadband. Presence/
    // DSP-active facts (TGXL, CH1, PSA, RX pills) are reported to the
    // controller via setItemAvailable at the signal that changes them,
    // not re-derived here -- see ChromeBarController::setItemAvailable.
    if (m_chromeBar && m_chromeBarWidget) {
        m_chromeBar->relayout(m_chromeBarWidget->width());
    }
    placeCoreStopBanner();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (!m_shuttingDown
        && (qobject_cast<ContainerWidget*>(watched)
            || qobject_cast<MeterWidget*>(watched))
        && (event->type() == QEvent::Show || event->type() == QEvent::Hide
            || event->type() == QEvent::ParentChange)) {
        QTimer::singleShot(0, this, &MainWindow::reconcileMiniDisplays);
    }
    // R-R3-38: the stop message sits over the content area, which moves
    // and resizes on its own when a dock opens or closes; follow it so the
    // message never sits over a dock. Observe only.
    if (m_coreStopBanner && watched == centralWidget()
     && (event->type() == QEvent::Resize || event->type() == QEvent::Move)) {
        placeCoreStopBanner();
    }

    // Phase 3Q Sub-PR-4 D.3: TitleBar ConnectionSegment hover tooltip.
    // The segment has installEventFilter(this) in the D.2 wiring block.
    // We intercept QHelpEvent (ToolTip) and delegate to RadioModel for the
    // formatted multi-line string so the segment stays a thin paint layer.
    if (m_titleBar && watched == m_titleBar->connectionSegment()
     && event->type() == QEvent::ToolTip) {
        auto* helpEvent = static_cast<QHelpEvent*>(event);
        QToolTip::showText(helpEvent->globalPos(),
                           m_remoteConnection ? m_remoteConnection->detailText()
                               + (m_remoteTelemetry ? QLatin1Char('\n') + m_remoteTelemetry->detailText() : QString{})
                                              : m_radioModel->buildConnectionTooltip(),
                           m_titleBar->connectionSegment());
        return true;
    }

    // Phase 23: m_tciIndicator click → open Setup → TCI Server.
    // The indicator is a QWidget (not a QLabel) so we match by pointer identity.
    if (event->type() == QEvent::MouseButtonPress) {
        if (watched == m_tciIndicator) {
            openTciSetupPage();
            return true;  // event consumed
        }
    }

    // Handle ☰ panel toggle click — label has property "isPanelToggle"
    if (event->type() == QEvent::MouseButtonPress) {
        auto* label = qobject_cast<QLabel*>(watched);
        if (label && label->property("isPanelToggle").toBool()) {
            // Toggle QSplitter right pane (index 1) visibility.
            // Save/restore sizes so spectrum expands when panel is hidden.
            if (!m_mainSplitter || m_mainSplitter->count() < 2) {
                return QMainWindow::eventFilter(watched, event);
            }
            QWidget* rightPane = m_mainSplitter->widget(1);
            if (rightPane->isVisible()) {
                // Hide: save current sizes, then collapse right to 0
                m_splitterSizesBeforeHide = m_mainSplitter->sizes();
                rightPane->hide();
                label->setStyleSheet(QStringLiteral(
                    "QLabel { color: #404858; font-weight: bold; font-size: 16px; }"));
            } else {
                // Show: restore saved sizes (or default 80/20 if none saved)
                rightPane->show();
                if (!m_splitterSizesBeforeHide.isEmpty()) {
                    m_mainSplitter->setSizes(m_splitterSizesBeforeHide);
                } else {
                    m_mainSplitter->setSizes({1024, 256});
                }
                label->setStyleSheet(QStringLiteral(
                    "QLabel { color: #8aa8c0; font-weight: bold; font-size: 16px; }"));
            }
            return true;  // event consumed
        }
    }

    // Task B4: +PAN icon click; label has property "isAddPanButton".
    // updateAddPanButtonState() disables the label while disconnected, so
    // this fires only when a layout change is actually possible; the
    // showPanLayoutDialog() body re-checks the same guard defensively.
    // Left button only (final-fix-wave finding 12): a bare
    // QEvent::MouseButtonPress check fires on right-click and middle-
    // click too, which is not how every other clickable icon on this bar
    // behaves (compare StationBlock::mousePressEvent's explicit button
    // check).
    if (watched->property("isAddPanButton").toBool()
        && event->type() == QEvent::MouseButtonPress
        && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
        showPanLayoutDialog();
        return true;
    }

    // Status-bar TNF light: click toggles every notch at once.
    // From AetherSDR MainWindow_Shortcuts.cpp:612-614 [@c6481cbf], which
    // flips the model flag straight from the indicator's mouse press. The
    // DSP > TNF menu item follows through globalEnabledChanged, so either
    // surface can originate the flip and neither echoes it back.
    if (event->type() == QEvent::MouseButtonPress) {
        auto* label = qobject_cast<QLabel*>(watched);
        if (label && label->property("isTnfToggle").toBool()) {
            NotchModel* notches =
                m_radioModel ? m_radioModel->notchModel() : nullptr;
            if (notches) {
                notches->setGlobalEnabled(!notches->globalEnabled());
            }
            return true;  // event consumed
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::applyDarkTheme()
{
    setStyleSheet(QStringLiteral(
        "QMainWindow { background: #0f0f1a; }"
        "QMenuBar {"
        "  background: #1a2a3a;"
        "  color: #c8d8e8;"
        "  border-bottom: 1px solid #203040;"
        "}"
        "QMenuBar::item:selected { background: #00b4d8; }"
        "QMenu {"
        "  background: #1a2a3a;"
        "  color: #c8d8e8;"
        "  border: 1px solid #203040;"
        "}"
        "QMenu::item:selected { background: #00b4d8; }"
        "QLabel { color: #c8d8e8; }"
        "QStatusBar {"
        "  background: #1a2a3a;"
        "  color: #8090a0;"
        "  border-top: 1px solid #203040;"
        "}"));
}

void MainWindow::showConnectionPanel()
{
    if (m_shuttingDown) { return; }
    if (m_connectionPickerManaged) {
        emit connectionsRequested();
        return;
    }
    // ── Remote-daemon R2 Task 20 ─────────────────────────────────────────
    // This is the local radio panel, including its automatic reopen paths.
    // Explicit operator connection gestures use connectionRequestedByOperator.
    // Never turn this remote guard into a dial: retained radio-name updates
    // during manual session teardown can invoke it and undo Disconnect.
    if (m_radioModel != nullptr && !m_radioModel->ownsLocalDsp()) {
        qCInfo(lcConnection)
            << "Connection panel suppressed: this window is driving a remote "
               "station, which owns the radio. Use --station to change it.";
        return;
    }

    if (!m_connectionPanel) {
        m_connectionPanel = new ConnectionPanel(m_radioModel, this);
        m_connectionPanel->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_connectionPanel, &QObject::destroyed, this, [this]() {
            m_connectionPanel = nullptr;
        });
    }
    m_connectionPanel->show();
    m_connectionPanel->raise();
    m_connectionPanel->activateWindow();
}

// ---------------------------------------------------------------------------
// Remote-daemon R2 Task 20: the two remaining halves of the remote gate.
// ---------------------------------------------------------------------------

void MainWindow::openNetworkDiagnostics()
{
    if (m_radioModel != nullptr && !m_radioModel->ownsLocalDsp()) {
        if (!m_remoteTelemetry) { showRemoteConnectionPanel(); return; }
        auto* dlg = new RemoteDiagnosticsDialog(m_remoteTelemetry, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
        return;
    }

    auto* dlg = new NetworkDiagnosticsDialog(
        m_radioModel, m_radioModel->audioEngine(), this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void MainWindow::setSavedCoreName(const QString& name)
{
    if (m_savedCoreName == name) { return; }
    m_savedCoreName = name;
    refreshRemoteConnectionUi();
}

CoreSettingsContext MainWindow::coreSettingsSnapshot() const
{
    CoreSettingsContext context;
    context.connectionDetailsAvailable = m_remoteConnection != nullptr;
    context.diagnosticsAvailable = m_radioModel != nullptr;
    context.audioAvailable = m_remoteMedia != nullptr;
    context.stationSettingsAvailable = stationSettingsAvailable();
    context.stationSettingsReason = stationSettingsReason();
    context.listener = tr("Not reported");
    context.controls = tr("Path unavailable");
    context.audioAndDisplay = tr("Path unavailable");
    context.reachedThrough = tr("Not known");
    if (!m_stationClient) { return context; }
    context.epoch = m_stationClient->sessionEpoch(); // Host replaces this source stamp with a UI epoch.
    context.pairedIdentity = m_stationClient->stationIdentityFingerprint();
    context.authenticated = m_stationClient->isHandshakeComplete() && m_stationClient->signedInWithDeviceKey();
    if (!context.authenticated) { return context; }
    context.coreName = m_stationClient->remoteDevices()->coreInfo().stationLabel;
    context.radio = m_remoteConnection ? m_remoteConnection->radioText() : tr("Radio unknown");
    const auto controls = m_stationClient->transport()
        ? m_stationClient->transport()->networkPathSnapshot() : std::optional<NetworkPathSnapshot>{};
    context.controls = ConnectionSegment::routeText(controls);
    context.audioAndDisplay = ConnectionSegment::routeText(m_remoteMedia
        ? m_remoteMedia->currentNetworkPath() : std::optional<NetworkPathSnapshot>{});
    const int rank = m_stationClient->pathRank();
    if (rank == PathRacer::ThisNetwork) { context.reachedThrough = tr("This network"); }
    else if (rank == PathRacer::Direct) { context.reachedThrough = tr("A direct address"); }
    else if (rank >= PathRacer::ServiceDirect) { context.reachedThrough = tr("Remote access introduction"); }
    // A diagnostic socket/ICE peer is never reusable listener evidence.
    const QUrl listener = m_stationClient->connectedUrl();
    QHostAddress numeric;
    if (rank >= PathRacer::ThisNetwork && rank <= PathRacer::Direct
        && listener.scheme() == QStringLiteral("wss")
        && listener.port() > 0 && listener.port() <= 65535 && numeric.setAddress(listener.host())) {
        context.listener = numeric.toString();
    }
    return context;
}

void MainWindow::openCoreSettings(const QString& targetId)
{
    SetupDialog* dialog = createSetupDialog();
    if (!dialog) { return; }
    // createSetupDialog emits setupDialogCreated before returning, so the host
    // installs its canonical store/context and lazy page binder before inspection.
    dialog->inspectCoreTarget(targetId);
    dialog->show();
    dialog->raise();
}

SetupDialog* MainWindow::createSetupDialog()
{
    // R-R3-21 / R-R3-10: Setup opens in every state. A remote window that is
    // disconnected, or still waiting for the Core's settings, keeps this
    // computer's own settings usable; the Core's settings stay disabled
    // with a reason until they arrive (SetupDialog::setStationSettingsAvailable).
    // That replaces the old refusal toast. What the refusal protected is
    // still protected: a Core page is not built before the Core's settings
    // arrive, so it cannot show (or write) this computer's ship defaults,
    // and SettingsProxy sends nothing while it is not ready. In local
    // direct mode the settings are always available and nothing changes.
    auto* dialog = new SetupDialog(m_radioModel, this);
    dialog->setReceiverSelector([this] { return activeSliceForWindow(); },
                                [this] { return desktopHosting(); });
    if (m_radioModel && m_radioModel->sliceOwnership()) {
        SliceOwnership* ownership = m_radioModel->sliceOwnership();
        connect(ownership, &SliceOwnership::activeChanged, dialog,
                [dialog] { dialog->notifyReceiverSelectionChanged(); });
        connect(ownership, &SliceOwnership::markChanged, dialog,
                [dialog] { dialog->notifyReceiverSelectionChanged(); });
    }
    dialog->setTransmitPermitted(transmitControlsPermitted(),
        tr("Remote transmit controls are not available from this Core."));
    // R-R3-49 (parity Task 1): the transmit settings that key nothing.
    dialog->setTransmitSettingsPermitted(transmitSettingsPermitted(),
                                         transmitSettingsReason());
    if (!m_radioModel || !m_radioModel->ownsLocalDsp()) {
        // R-R3-49 (parity Tasks 2 and 3): the settings later versions
        // brought (Audio > TX Input's microphone, Audio > TX Profile).
        // R-R3-49 (parity Task 6): and version 6, Setup > PA.
        // R-R3-49 (parity Task 13): and version 8, Hardware Config's OC
        // transmit pins, pin actions and transmit calibration.
        // And version 11, Transmit > Power's "Disable HF PA".
        for (const int version : {2, 3, 4, 5, 6, 8, 11}) {
            dialog->setTransmitSettingsPermitted(transmitSettingsPermitted(version),
                                                 transmitSettingsReason(version), version);
        }
    }
    // Fix wave 2 (M8): Enable VOX needs this computer's microphone line.
    if (m_remoteMedia != nullptr && !m_remoteMedia->micLineOpen()) {
        dialog->setVoxPermitted(false, TxRefusals::micNotConnected().text);
    }
    if (m_stationClient && m_stationClient->remoteTransmit()) {
        const auto* source = m_stationClient->remoteTransmit();
        if (!source->micSourceSettled() || source->acceptedMicSource() == RemoteMicSource::RadioMic) {
            dialog->setVoxPermitted(false,
                !source->micSourceSettled() ? source->micSourceReason() : remoteRadioVoxReason());
        }
    }
    // Station VOX: a hosting window's, while another device holds transmit.
    if (const QString holderReason = desktopVoxHolderReason(); !holderReason.isEmpty()) {
        dialog->setVoxPermitted(false, holderReason);
    }
    dialog->setCoreAudioSources(m_remoteMedia, m_remoteTelemetry);
    dialog->setCoreAudioContext(coreSettingsSnapshot());
    dialog->setStationSettingsAvailable(stationSettingsAvailable(), stationSettingsReason());
    seedReceiverAudioNote(dialog, [this] { return receiverAudioNoteFor(m_remoteMedia); });
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    wireSetupDialog(dialog);
    emit setupDialogCreated(dialog);
    return dialog;
}

bool MainWindow::stationSettingsAvailable() const
{
    // setupDialogAllowedForCurrentBackend(): true in local direct mode (no
    // settings proxy); in a remote window, true only while the session is
    // ready and holds the Core's settings snapshot.
    return (m_radioModel != nullptr && m_radioModel->ownsLocalDsp())
        || setupDialogAllowedForCurrentBackend();
}

QString MainWindow::stationSettingsReason() const
{
    // R3 Setup fix wave (final review I2): true to the state. Connected to
    // a Core that has not sent its settings (still arriving, or an empty
    // profile it never marked), "Connect to the Core" would be wrong.
    if (m_stationClient != nullptr && m_stationClient->isConnectionActive()) {
        return tr("The Core has not sent its settings.");
    }
    return tr("Connect to the Core to change these.");
}

// ── Parity Task 21 (R-IOS-18): the Core's radio from a remote window ──────

QString MainWindow::coreRadioAddressText() const
{
    return m_stationClient != nullptr && m_stationClient->isConnectionActive()
        ? m_stationClient->capabilities().radioAddress
        : QString();
}

QString MainWindow::coreRadioMacText() const
{
    return m_stationClient != nullptr && m_stationClient->isConnectionActive()
        ? m_stationClient->capabilities().macAddress
        : QString();
}

QString MainWindow::forgetCoreRadioReason() const
{
    // The Core's own radio cannot be forgotten while it runs it (the
    // Core's rule); This Core forgets the others.
    for (const StationRadioEntry& radio : m_radioModel->stationRadios()) {
        if (radio.inUse) {
            return StationRadios::inUseReason();
        }
    }
    return {};
}

void MainWindow::addCoreRadioActions(QMenu& menu)
{
    menu.addAction(tr("Change radio…"), this, [this]() {
        openThisCore(ThisCoreFocus::ChangeRadio);
    });
    menu.addAction(tr("Edit radio…"), this, [this]() {
        openThisCore(ThisCoreFocus::EditRadio);
    });
    QAction* forget = menu.addAction(tr("Forget radio"), this, [this]() {
        openThisCore(ThisCoreFocus::ForgetRadio);
    });
    const QString why = forgetCoreRadioReason();
    forget->setEnabled(why.isEmpty());
    forget->setToolTip(why);
}

void MainWindow::refreshCoreRadioActions()
{
    if (m_actForgetCoreRadio == nullptr || m_radioModel == nullptr) {
        return;
    }
    // GUI-M2 (fix wave): the Core's radio items, in a window that runs its
    // own radio, wait with the reason.
    if (m_radioModel->ownsLocalDsp()) {
        // Fix round 1 (minor 3): the reason names the item this window has
        // (a window the connection picker manages calls it Connections).
        const QString local = m_connectionPickerManaged
            ? tr("This computer runs its own radio. Change it in Radio > Connections.")
            : tr("This computer runs its own radio. Change it in the Connection panel "
                 "(Radio > Manage Radios).");
        for (QAction* a : {m_actChangeCoreRadio, m_actEditCoreRadio, m_actForgetCoreRadio}) {
            if (a == nullptr) {
                continue;
            }
            a->setEnabled(false);
            a->setToolTip(local);
        }
        return;
    }
    for (QAction* a : {m_actChangeCoreRadio, m_actEditCoreRadio}) {
        if (a != nullptr) {
            a->setEnabled(true);
            a->setToolTip(QString());
        }
    }
    const QString why = forgetCoreRadioReason();
    m_actForgetCoreRadio->setEnabled(why.isEmpty());
    m_actForgetCoreRadio->setToolTip(why);
}

void MainWindow::openThisCore(ThisCoreFocus focus)
{
    auto* dialog = createSetupDialog();
    if (dialog == nullptr) {
        return;
    }
    // selectPage realizes the lazily built page before it returns.
    dialog->selectPage(QStringLiteral("This Core"));
    if (auto* page = dialog->findChild<ThisCorePage*>()) {
        if (focus != ThisCoreFocus::ChangeRadio) {
            page->selectCoreRadio();
        }
        if (focus == ThisCoreFocus::EditRadio) {
            page->modelCombo()->setFocus();
        } else if (focus == ThisCoreFocus::ForgetRadio) {
            page->forgetButton()->setFocus();
        }
    }
    dialog->show();
    dialog->raise();
}

void MainWindow::refreshSpotHubAvailability()
{
    // Parity Task 19 (R-IOS-25, B7.2): a window running its own radio runs
    // every source itself. A remote window's Spot Hub edits the Core's
    // settings (Setup's words while there is no Core session) and asks the
    // Core to run the station's sources.
    if (!m_spotHubDialog || m_radioModel == nullptr) {
        return;
    }
    if (m_radioModel->ownsLocalDsp()) {
        m_spotHubDialog->setStationSettingsAvailable(true, QString());
        m_spotHubDialog->setStationSourcesAvailable(true, QString());
        m_spotHubDialog->setStationFreedvAvailable(true, QString());
        return;
    }
    const bool settings = stationSettingsAvailable();
    m_spotHubDialog->setStationSettingsAvailable(settings, stationSettingsReason());
    const bool sources = m_stationClient != nullptr && m_stationClient->spotSourcesAvailable();
    m_spotHubDialog->setStationSourcesAvailable(
        sources, settings ? IStationLink::spotSourcesUnavailableReason() : stationSettingsReason());
    // iPhone plan Task 22 / parity Task 20: FreeDV Reporter is the Core's.
    const bool freedv = m_stationClient != nullptr && m_stationClient->stationFreedvAvailable();
    m_spotHubDialog->setStationFreedvAvailable(
        freedv, IStationLink::stationFreedvUnavailableReason());
}

void MainWindow::refreshFreedvReporterAvailability()
{
    // iPhone plan Task 22 / parity Task 20 (R-IOS-26): in a remote window
    // the FreeDV Reporter dialog's requests go to the Core.
    if (!m_freeDVReporterDialog || m_radioModel == nullptr) {
        return;
    }
    if (m_radioModel->ownsLocalDsp()) {
        m_freeDVReporterDialog->setCoreRequestsAvailable(true, QString());
        return;
    }
    const bool freedv = m_stationClient != nullptr && m_stationClient->stationFreedvAvailable();
    m_freeDVReporterDialog->setCoreRequestsAvailable(
        freedv, stationSettingsAvailable() ? IStationLink::stationFreedvUnavailableReason()
                                           : stationSettingsReason());
}

RemoteReceiverAudioNote MainWindow::receiverAudioNoteFor(const RemoteMediaController* media)
{
    // Only a remote window has remote media, and only a Core that sends
    // receiver streams feeds VAX from it.
    if (media == nullptr) {
        return RemoteReceiverAudioNote::None;
    }
    return remoteReceiverAudioNote(media->audioStatus(), media->receiverAudioNegotiated());
}

void MainWindow::wireReceiverAudioNotePush(QObject* dialogRoot, RemoteMediaController* media,
                                           RadioModel* model, ReceiverAudioNoteSource source)
{
    if (dialogRoot == nullptr || !source) {
        return;
    }
    auto push = [root = QPointer<QObject>(dialogRoot), source] {
        if (!root) {
            return;
        }
        const RemoteReceiverAudioNote note = source();
        for (SetupDialog* dialog : root->findChildren<SetupDialog*>()) {
            dialog->setReceiverAudioNote(note);
        }
    };
    if (media != nullptr) {
        QObject::connect(media, &RemoteMediaController::audioStatusChanged, dialogRoot, push);
    }
    // receiverAudioNegotiated() is not part of the audio status: a
    // capability change (or the session ending) reaches the window as a
    // station link change, which need not change the status.
    if (model != nullptr) {
        QObject::connect(model, &RadioModel::stationLinkStateChanged, dialogRoot, push);
    }
}

void MainWindow::seedReceiverAudioNote(SetupDialog* dialog, const ReceiverAudioNoteSource& source)
{
    if (dialog != nullptr && source) {
        dialog->setReceiverAudioNote(source());
    }
}

bool MainWindow::transmitControlsPermitted() const
{
    // Desktop remote transmit (R-IOS-13): a remote window's controls work
    // while the Core takes its keys (remoteTxVersion) and permits it now.
    return m_radioModel && (m_radioModel->ownsLocalDsp()
        || (m_stationClient && m_stationClient->isHandshakeComplete()
            && m_stationClient->remoteTransmitAvailable()
            && m_stationClient->capabilities().txPermitted));
}

QString MainWindow::remoteTransmitReason() const
{
    // The Core's own sentence (link section 18.3) when it takes this
    // window's keys and gave one; otherwise today's words.
    if (m_stationClient && m_stationClient->remoteTransmitAvailable()
        && !m_stationClient->capabilities().txRefusalReason.isEmpty()) {
        return m_stationClient->capabilities().txRefusalReason;
    }
    return tr("Remote transmit controls are not available from this Core.");
}

QString MainWindow::transmitSliceChoiceReason() const
{
    // Slice control plan Task 11 fix: the TX applet's letters and every
    // flag's TX button give the same answer. A hosting window moves
    // transmit while the station device holds it; a remote window asks the
    // Core, which takes tx.setTxSlice only from its holder.
    if (desktopHosting()) {
        return desktopOwnsTransmit() ? QString() : TxRefusals::notHolder().text;
    }
    if (m_radioModel && !m_radioModel->ownsLocalDsp() && m_stationClient) {
        if (!m_stationClient->sessionHolderAvailable()
            || !m_stationClient->remoteTransmitAvailable()) {
            return tr("This Core does not offer moving transmit between slices to this app.");
        }
        return m_stationClient->holdsTransmitHere() ? QString() : TxRefusals::notHolder().text;
    }
    return {};
}

void MainWindow::requestTransmitSlice(int sliceId)
{
    if (!m_radioModel || !transmitSliceChoiceReason().isEmpty()) {
        return;
    }
    if (!m_radioModel->ownsLocalDsp()) {
        if (m_stationClient) { m_stationClient->requestTxSlice(sliceId); }
        return;
    }
    // The arbiter drops MOX before it moves transmit (ruling 8.10).
    if (m_radioModel->txSliceArbiter()) {
        m_radioModel->requestTxHandoffToSlice(sliceId);
    }
}

void MainWindow::applyFlagTransmitGate(VfoWidget* flag) const
{
    if (!flag) { return; }
    const bool transmit = transmitControlsPermitted();
    const QString choice = transmitSliceChoiceReason();
    flag->setTransmitPermitted(transmit && choice.isEmpty(),
                               !transmit ? remoteTransmitReason() : choice);
    applyTxBadgeOffer(flag);
}

bool MainWindow::windowHoldsTransmit() const
{
    if (desktopHosting()) { return desktopOwnsTransmit(); }
    if (m_radioModel && !m_radioModel->ownsLocalDsp() && m_stationClient) {
        return m_stationClient->holdsTransmitHere();
    }
    return true;
}

MainWindow::TxSliceAction MainWindow::txSliceAction(int id) const
{
    TxSliceAction offer;
    if (!m_radioModel || !m_radioModel->sliceById(id)) { return offer; }
    const VfoWidget::SliceAccess access = windowSliceAccess(
        windowSliceRows(*m_radioModel, sliceAccessServer(), sliceAccessClient())).value(id);
    const bool listening = access.state == VfoWidget::SliceAccess::State::Listening;
    // Whether this window may take transmit, and from whom (empty when
    // nobody holds it: the take is at once).
    bool takesTransmit = false;
    QString holderName;
    // Why this window cannot take transmit now, when that is all that
    // holds the badge (a hosting take refused while transmit changes hands).
    QString takeRefusal;
    bool sliceTakes = false;
    bool onAir = false;
    if (desktopHosting()) {
        StationServer* server = m_desktopStationController->server();
        TransmitHolder* holder = server ? server->transmitHolder() : nullptr;
        const TransmitHolder::TakeAnswer answer = holder && !desktopOwnsTransmit()
            ? holder->askTake(SliceOwnership::stationDevice())
            : TransmitHolder::TakeAnswer{};
        if (holder && !desktopOwnsTransmit()
            && answer.verdict == TransmitHolder::TakeVerdict::Refuse) {
            takeRefusal = answer.refusal.text;
        } else if (holder && !desktopOwnsTransmit()) {
            takesTransmit = true;
            if (const std::optional<TransmitHolder::Holder> current = holder->holder()) {
                TakeTransmitDialog::Holder shown;
                shown.name = current->name;
                shown.shortName = current->shortName;
                holderName = TakeTransmitDialog::shortNameOf(shown);
            }
        }
        sliceTakes = hostingSlices() != nullptr;
        onAir = server && server->sliceOnAir(id);
    } else if (m_radioModel && !m_radioModel->ownsLocalDsp() && m_stationClient) {
        // The Core takes this window's keys and tx.take. Only a refusal
        // that is the holder's (another device holds or is on the air) is
        // one a take answers; any other (receive only, not paired, not
        // ready) holds the badge with the Core's words, as before.
        const bool elsewhere = m_stationClient->transmitHeldElsewhere();
        const QString code = m_stationClient->capabilities().txRefusalCode;
        const bool holderRefusal = code == QLatin1String(TxRefusals::kOtherDeviceHolds)
            || code == QLatin1String(TxRefusals::kHolderOnAir);
        if (m_stationClient->transmitTakeAvailable() && !m_stationClient->holdsTransmitHere()
            && (transmitControlsPermitted() || (elsewhere && holderRefusal))) {
            takesTransmit = true;
            if (elsewhere) {
                holderName = TakeTransmitDialog::shortNameOf(
                    TakeTransmitDialog::fromTransmitState(*m_stationClient->transmitState()));
            }
        }
        sliceTakes = m_stationClient->remoteSliceAccessAvailable();
        const SliceModel* slice = m_radioModel->sliceById(id);
        onAir = slice && slice->isTxSlice() && m_stationClient->knowsTransmitHolder()
            && m_stationClient->transmitState()->keyed();
    }
    const bool holds = windowHoldsTransmit();
    if (listening) {
        // Case 3: a slice another device controls. The refusals keep their
        // words: on the air, and the Core's own refusal of the take.
        if (onAir) {
            offer.heldReason = SliceAccessController::takeWhileTransmittingWords(id);
        } else if (!access.takeHeldReason.isEmpty()) {
            offer.heldReason = access.takeHeldReason;
        } else if (sliceTakes && !holds && !takeRefusal.isEmpty()) {
            offer.heldReason = takeRefusal;
        } else if (sliceTakes && (holds || takesTransmit)) {
            offer.offered = true;
            offer.toolTip = !holds && !holderName.isEmpty()
                ? tr("Take control of this slice, then take transmit from %1")
                      .arg(holderName)
                : tr("Take control of this slice and make it the TX slice");
        }
    } else if (!holds && !takeRefusal.isEmpty() && windowControlsSlice(id)) {
        // Case 2 while the take is refused for now: held with its reason.
        offer.heldReason = takeRefusal;
    } else if (!holds && takesTransmit && windowControlsSlice(id)) {
        // Case 2: this window's slice, and it does not hold transmit.
        offer.offered = true;
        offer.toolTip = holderName.isEmpty()
            ? tr("Take transmit and make this the TX slice")
            : tr("Take transmit from %1 and make this the TX slice").arg(holderName);
    }
    const bool permitted = transmitControlsPermitted() && transmitSliceChoiceReason().isEmpty();
    QString reason = !transmitControlsPermitted() ? remoteTransmitReason()
        : (!transmitSliceChoiceReason().isEmpty() ? transmitSliceChoiceReason() : access.heldReason);
    const bool pending = m_flagRequestSlice == id && m_sliceChooser && m_sliceChooser->isPending();
    const bool radioPtt = m_stationClient && m_stationClient->knowsTransmitHolder()
        && m_stationClient->transmitState()->keyed()
        && m_stationClient->transmitState()->holderSource() == QStringLiteral("radioPtt")
        && m_stationClient->transmitState()->txSliceId() == id;
    if (!offer.heldReason.isEmpty()) { reason = offer.heldReason; }
    else if (offer.offered && pending) { reason = tr("Asking the Core…"); }
    else if (radioPtt) { reason = VfoWidget::inUseByRadioText(); }
    offer.enabled = !radioPtt && ((offer.offered && !pending && (!permitted || listening))
                                  || (permitted && !listening));
    offer.effectiveWords = offer.enabled ? offer.toolTip : reason;
    return offer;
}

void MainWindow::applyTxBadgeOffer(VfoWidget* flag) const
{
    if (!flag) { return; }
    const TxSliceAction action = txSliceAction(flag->sliceIndex());
    flag->setTxBadgeOffer({action.offered, action.toolTip, action.heldReason});
}

void MainWindow::activateTransmitSlice(int sliceId, bool controlledOnly)
{
    // A delivered old letter intent must still name a controlled slice.
    // Flags retain their Take control path for a listened slice.
    if (controlledOnly && !windowControlsSlice(sliceId)) { return; }
    const TxSliceAction action = txSliceAction(sliceId);
    if (!action.enabled) {
        if (desktopHosting() && !action.heldReason.isEmpty()) {
            showToast(action.heldReason, ToastSeverity::Info, 3000);
        }
        return;
    }
    if (action.offered) { startTxBadgeTake(sliceId); }
    else {
        abandonTxBadgeTake();
        requestTransmitSlice(sliceId);
    }
}

quint64 MainWindow::txSliceIncarnation(int sliceId) const
{
    if (desktopHosting() && m_radioModel->sliceOwnership()) {
        return m_radioModel->sliceOwnership()->incarnation(sliceId);
    }
    if (StationClient* client = sliceAccessClient()) {
        if (const auto entry = client->sliceAccess()->entry(sliceId)) { return entry->incarnation; }
    }
    return 0;
}

bool MainWindow::txTakeTargetValid(int sliceId, SliceModel* target, quint64 incarnation,
                                  bool requireControl) const
{
    return target && m_radioModel && m_radioModel->sliceById(sliceId) == target
        && txSliceIncarnation(sliceId) == incarnation
        && (!requireControl || windowControlsSlice(sliceId));
}

void MainWindow::startTxBadgeTake(int sliceId)
{
    // A new click replaces a take still waiting.
    abandonTxBadgeTake();
    // The old hosting dialog's rejected handler must run before the new
    // take enters Transmit; otherwise it abandons the replacement intent.
    const QPointer<MainWindow> self(this);
    if (m_desktopTakeDialog) { m_desktopTakeDialog->close(); }
    if (!self) { return; }
    const TxSliceAction action = txSliceAction(sliceId);
    if (!m_radioModel || !action.offered || !action.enabled) { return; }
    m_txBadgeTakeSlice = sliceId;
    m_txBadgeTarget = m_radioModel->sliceById(sliceId);
    m_txBadgeIncarnation = txSliceIncarnation(sliceId);
    m_txBadgeTakingSlice = !windowControlsSlice(sliceId);
    if (windowControlsSlice(sliceId)) {
        takeTransmitForTxBadge(sliceId);
        return;
    }
    // Case 3: the slice first, as the flag's Take control asks for it. Its
    // answer (txBadgeSliceAnswered) goes on to transmit.
    m_txBadgeTakeStage = TxBadgeStage::Slice;
    runFlagAccessAction(SliceChooserAction::TakeControl, sliceId);
    if (m_txBadgeTakeStage == TxBadgeStage::Slice && m_txBadgeTakeSlice == sliceId
        && (!m_sliceChooser
            || m_sliceChooser->requestInFlight() != QByteArrayLiteral("slice.takeControl"))) {
        // Nothing was sent (another request still waits, and says so).
        abandonTxBadgeTake();
    }
}

void MainWindow::txBadgeSliceAnswered(int sliceId, bool accepted)
{
    if (m_txBadgeTakeStage != TxBadgeStage::Slice || sliceId != m_txBadgeTakeSlice) { return; }
    if (!accepted) {
        // The flag shows the Core's refusal; nothing else changes.
        abandonTxBadgeTake();
        return;
    }
    // A remote window: the former controller transmitting on this slice
    // loses it with the slice (ruling Q8), and the Core's word of that
    // follows its answer. Wait for it, so the question is asked only of a
    // holder that still holds.
    if (!desktopHosting() && m_stationClient && m_stationClient->transmitHeldElsewhere()
        && m_stationClient->transmitState()->txSliceId() == sliceId) {
        m_txBadgeTakeStage = TxBadgeStage::Transmit;
        m_txBadgeAwaitingHolder = true;
        // Past this, the take goes on with what the Core last said.
        constexpr int kTxBadgeHolderWaitMs = 1000;
        const quint64 serial = m_txBadgeSerial;
        QTimer::singleShot(kTxBadgeHolderWaitMs, this, [this, serial, sliceId]() {
            if (serial != m_txBadgeSerial || !m_txBadgeAwaitingHolder) { return; }
            m_txBadgeAwaitingHolder = false;
            takeTransmitForTxBadge(sliceId);
        });
        return;
    }
    takeTransmitForTxBadge(sliceId);
}

void MainWindow::continueTxBadgeTake()
{
    if (m_txBadgeTakeStage != TxBadgeStage::Transmit || !m_stationClient) { return; }
    const bool elsewhere = m_stationClient->transmitHeldElsewhere();
    const int sliceId = m_txBadgeTakeSlice;
    if (m_txBadgeAwaitingHolder) {
        if (elsewhere && m_stationClient->transmitState()->txSliceId() == sliceId) { return; }
        m_txBadgeAwaitingHolder = false;
        takeTransmitForTxBadge(sliceId);
        return;
    }
    if (m_txBadgeAsk && m_txBadgeCommandId == 0 && !elsewhere) {
        // Nobody else holds transmit now: the question is not needed, and
        // the take goes on at once (or is already done).
        QDialog* ask = m_txBadgeAsk.data();
        m_txBadgeAsk.clear();
        disconnect(ask, nullptr, this, nullptr);
        const QPointer<MainWindow> self(this);
        ask->reject();
        if (!self || m_txBadgeTakeStage != TxBadgeStage::Transmit
            || m_txBadgeTakeSlice != sliceId) {
            return;
        }
        takeTransmitForTxBadge(sliceId);
        return;
    }
    finishTxBadgeTakeIfHeld();
}

void MainWindow::takeTransmitForTxBadge(int sliceId)
{
    if (!txTakeTargetValid(sliceId, m_txBadgeTarget, m_txBadgeIncarnation, false)) {
        abandonTxBadgeTake();
        return;
    }
    m_txBadgeTakeSlice = sliceId;
    m_txBadgeTakeStage = TxBadgeStage::Transmit;
    if (windowHoldsTransmit()) {
        m_txBadgeGranted = true;
        finishTxBadgeTakeIfHeld();
        return;
    }
    if (desktopHosting()) {
        // The Take transmit question the TX applet's MOX asks, keying
        // nothing (DesktopStationController::Key::Take).
        const QPointer<MainWindow> self(this);
        if (m_desktopTakeDialog) { m_desktopTakeDialog->close(); }
        if (!self || !desktopHosting() || m_txBadgeTakeStage != TxBadgeStage::Transmit) {
            return;
        }
        const QPointer<DesktopStationController> controller(m_desktopStationController);
        const DesktopStationController::RequestResult result = controller->requestTakeTransmit();
        if (!self) { return; }
        m_txBadgeHostTakeId = result.takeId;
        handleDesktopTakeResult(result);
        if (!self) { return; }
        settleTxBadgeHostTake(result.takeId);
        return;
    }
    if (!m_stationClient || !m_multiDevice) {
        abandonTxBadgeTake();
        return;
    }
    const quint64 serial = m_txBadgeSerial;
    if (m_stationClient->transmitHeldElsewhere()) {
        // The pan's TAKE TX pill's question; its answer sends tx.take.
        m_multiDevice->askTakeTransmit();
        QDialog* ask = m_multiDevice->openDialog();
        if (!ask) {
            // Refused before asking (changing hands); the refusal says why.
            abandonTxBadgeTake();
            return;
        }
        m_txBadgeAsk = ask;
        // After the controller's own handler, which sends tx.take.
        connect(ask, &QDialog::accepted, this, [this, serial]() {
            if (serial != m_txBadgeSerial || m_txBadgeTakeStage != TxBadgeStage::Transmit) {
                return;
            }
            m_txBadgeAsk.clear();
            m_txBadgeCommandId = m_multiDevice ? m_multiDevice->lastTakeCommandId() : 0;
            if (m_txBadgeCommandId == 0) { abandonTxBadgeTake(); }
        });
        connect(ask, &QDialog::rejected, this, [this, serial]() {
            if (serial == m_txBadgeSerial && m_txBadgeTakeStage == TxBadgeStage::Transmit) {
                abandonTxBadgeTake();
            }
        });
        return;
    }
    // Nobody holds transmit: the Core takes it at once.
    m_txBadgeCommandId = m_stationClient->requestTakeTransmit(false, 0, false);
    if (m_txBadgeCommandId == 0) {
        abandonTxBadgeTake();
    }
}

void MainWindow::settleTxBadgeHostTake(quint64 takeId)
{
    if (m_txBadgeTakeStage != TxBadgeStage::Transmit || takeId == 0
        || takeId != m_txBadgeHostTakeId) {
        return;
    }
    // Still waiting: on the question, or on the Core's grant.
    if (m_desktopStationController && m_desktopStationController->takeInFlight() == takeId) {
        return;
    }
    if (!desktopHosting() || !desktopOwnsTransmit()) {
        // Refused, not assigned, or replaced by another request.
        abandonTxBadgeTake();
        return;
    }
    m_txBadgeGranted = true;
    finishTxBadgeTakeIfHeld();
}

void MainWindow::finishTxBadgeTakeIfHeld()
{
    if (m_txBadgeTakeStage != TxBadgeStage::Transmit || !m_txBadgeGranted
        || !windowHoldsTransmit()) {
        return;
    }
    const int sliceId = m_txBadgeTakeSlice;
    if (!txTakeTargetValid(sliceId, m_txBadgeTarget, m_txBadgeIncarnation, false)) {
        abandonTxBadgeTake();
        return;
    }
    // The Core answers slice.takeControl before publishing control. A
    // flag's Case 3 waits for that word; a letter never takes slice control.
    if (m_txBadgeTakingSlice && !windowControlsSlice(sliceId)) { return; }
    m_txBadgeTakingSlice = false;
    const QPointer<SliceModel> target = m_txBadgeTarget;
    const quint64 incarnation = m_txBadgeIncarnation;
    abandonTxBadgeTake();
    const quint64 serial = m_txBadgeSerial;
    // Queued: the Core binds a new holder's transmit slice as the take
    // completes (a slice it took is not chosen for it, ruling Q8), and the
    // choice made here must come after that binding, not before it.
    QTimer::singleShot(0, this, [this, sliceId, serial, target, incarnation]() {
        if (serial != m_txBadgeSerial
            || !txTakeTargetValid(sliceId, target, incarnation, true)
            || !windowHoldsTransmit()) {
            return;
        }
        // Case 1 from here: the arbiter drops MOX before it moves transmit
        // (ruling 8.10); a remote window asks the Core with tx.setTxSlice.
        requestTransmitSlice(sliceId);
    });
}

void MainWindow::abandonTxBadgeTake()
{
    m_txBadgeTakeSlice = -1;
    m_txBadgeTarget.clear();
    m_txBadgeIncarnation = 0;
    m_txBadgeTakingSlice = false;
    m_txBadgeTakeStage = TxBadgeStage::None;
    m_txBadgeHostTakeId = 0;
    m_txBadgeCommandId = 0;
    m_txBadgeGranted = false;
    m_txBadgeAwaitingHolder = false;
    // Fix wave GUI-I1: the take question the badge opened goes with the
    // take, so its Take can no longer send tx.take for a take abandoned.
    // After the serial moves on, so its own rejected handler does nothing.
    const QPointer<QDialog> ask = m_txBadgeAsk;
    m_txBadgeAsk.clear();
    ++m_txBadgeSerial;
    if (ask && ask->isVisible()) {
        ask->reject();
    }
}

void MainWindow::refreshFlagTransmitGates()
{
    if (m_txApplet) { m_txApplet->refreshTransmitSliceChoices(); }
    for (VfoWidget* flag : m_vfoWidgetsBySlice) {
        applyFlagTransmitGate(flag);
    }
}

bool MainWindow::rxBypassPermitted() const
{
    // Group B fix wave: a local window's BYPS writes this computer's
    // AlexController; a remote window's the Core's, from radioHardwareVersion 5.
    return m_radioModel && (m_radioModel->ownsLocalDsp()
        || (m_stationClient && m_stationClient->remoteRxBypassOnTxAvailable()));
}

QString MainWindow::rxBypassUnavailableReason() const
{
    if (rxBypassPermitted() || !m_stationClient) {
        return {};
    }
    return m_stationClient->rxBypassOnTxUnavailableReason();
}

bool MainWindow::transmitSettingsPermitted(int minVersion) const
{
    // R-R3-49 (parity Task 1): the Core decides; this only says whether a
    // change is worth sending. Remote parity on the air: a Core at
    // transmitSettingsVersion 13 takes them while its radio is on the air,
    // as a local window does; an older Core refuses them then, so they
    // wait (shown disabled with the reason) rather than snap back.
    return m_radioModel && (m_radioModel->ownsLocalDsp()
        || (m_stationClient && m_stationClient->isHandshakeComplete()
            && m_stationClient->transmitSettingsAvailable(minVersion)
            && (m_stationClient->transmitSettingsAvailable(kTransmitSettingsOnAirVersion)
                || !m_radioModel->isCoreOnAir())));
}

QString MainWindow::transmitSettingsReason(int minVersion) const
{
    if (transmitSettingsPermitted(minVersion)) {
        return QString();
    }
    if (m_radioModel && m_radioModel->isCoreOnAir()) {
        return RadioModel::onAirReason();
    }
    return IStationLink::transmitSettingsUnavailableReason();
}

bool MainWindow::pureSignalArmingPermitted() const
{
    // R-R3-49 (parity Task 7): arming keys nothing, so a Core at
    // transmitSettingsVersion 7 takes it while its radio is off the air.
    return transmitControlsPermitted() || transmitSettingsPermitted(7);
}

QString MainWindow::pureSignalArmingReason() const
{
    if (pureSignalArmingPermitted()) {
        return QString();
    }
    if (m_stationClient && m_stationClient->pureSignalArmingOffered()) {
        return transmitSettingsReason(7);
    }
    return remoteTransmitReason();
}

void MainWindow::refreshTciRemoteTransmit()
{
#ifdef HAVE_WEBSOCKETS
    if (!m_tciServer || !m_stationClient || m_radioModel == nullptr
        || m_radioModel->ownsLocalDsp()) {
        return;
    }
    const bool wanted = m_stationClient->remoteTransmitAvailable() && !m_shuttingDown;
    if (wanted == m_tciRemoteTransmitInstalled) {
        return;
    }
    m_tciRemoteTransmitInstalled = wanted;
    m_tciServer->setRemoteTransmit(
        wanted ? remoteTransmitForwarder(m_stationClient->remoteTransmit(), m_remoteMedia)
               : TciServer::RemoteTransmit{});
#endif
}

// GUI-I3 (fix wave): on a remote window the PureSignal applet follows the
// Core's radio. It hides only when that radio has no PureSignal hardware
// (as a local HL2 or Atlas hides it); a radio that has it while the Core
// has not advertised PureSignal 3 shows the applet disabled, with the
// reason, as the PureSignal menu items do.
void MainWindow::applyRemotePureSignalAppletGate()
{
    if (!m_pureSignalApplet) {
        return;
    }
    const bool linked = m_stationClient && m_stationClient->isHandshakeComplete();
    const bool ps3Supported = linked
        && m_stationClient->capabilities().psAlgorithmVersion == 3;
    const bool hardware = linked && m_radioModel->boardCapabilities().hasPureSignal;
    m_pureSignalApplet->setEnabled(ps3Supported);
    m_pureSignalApplet->setToolTip(ps3Supported
        ? QString()
        : tr("The connected Core has not advertised PureSignal 3."));
    m_pureSignalApplet->setVisible(hardware);
}

void MainWindow::applyRemoteRoleGating()
{
    if (m_radioModel == nullptr || m_radioModel->ownsLocalDsp()) {
        return;  // local direct mode: nothing here runs, by construction
    }

    // Core session activity is independent of the mirrored radio state:
    // an authenticated Core with an offline radio must still be disconnectable.
    const bool active = m_stationClient != nullptr
        && m_stationClient->isConnectionActive();
    const bool transmitPermitted = transmitControlsPermitted();
    const QString transmitReason = remoteTransmitReason();
    // Desktop remote transmit (R-R3-42): TCI programs key through the Core.
    refreshTciRemoteTransmit();
    // R-R3-49 (parity Task 1): the transmit settings that key nothing, live
    // while the Core takes them and its radio is off the air.
    bool settingsPermitted = transmitSettingsPermitted();
    QString settingsReason = transmitSettingsReason();
    // R-R3-49 (parity Task 2): the applets' other transmit settings came
    // with transmitSettingsVersion 2.
    bool chainPermitted = transmitSettingsPermitted(2);
    QString chainReason = transmitSettingsReason(2);
    // R-R3-49 (parity Task 3): the TX profiles, the radio microphone
    // settings and RADE's Reset vocoder came with transmitSettingsVersion 3.
    bool profilePermitted = transmitSettingsPermitted(3);
    QString profileReason = transmitSettingsReason(3);
    // R-R3-49 (parity Task 4): the TX EQ and CFC dialogs, Setup > DSP >
    // CFC and AGC/ALC's TX Leveler and ALC came with version 4.
    bool processingPermitted = transmitSettingsPermitted(4);
    QString processingReason = transmitSettingsReason(4);
    // Fix wave 2 (M8): VOX listens to this computer's microphone line to
    // the Core; without one it is shown disabled with the reason (the
    // Core's refusal of arming it stays the backstop).
    bool voxLine = m_remoteMedia == nullptr || m_remoteMedia->micLineOpen();
    QString voxReason = voxLine ? QString() : TxRefusals::micNotConnected().text;
    if (m_stationClient && m_stationClient->remoteTransmit()) {
        const auto* source = m_stationClient->remoteTransmit();
        if (!source->micSourceSettled() || source->acceptedMicSource() == RemoteMicSource::RadioMic) {
            voxLine = false;
            voxReason = !source->micSourceSettled() ? source->micSourceReason() : remoteRadioVoxReason();
        }
    }
    // iPhone app plan Task 77 (rulings 7.7, 8.4): while another device
    // holds transmit, the transmitter's settings are its own, and VOX
    // follows the holder: every transmit control here is shown disabled
    // with the Core's reason, never hidden. The Core refuses them anyway.
    const QString holderReason = m_stationClient && m_stationClient->knowsTransmitHolder()
        ? m_stationClient->otherHolderReason()
        : QString();
    const auto settingsAt = [this, &holderReason](int version) {
        return holderReason.isEmpty() && transmitSettingsPermitted(version);
    };
    const auto settingsReasonAt = [this, &holderReason](int version) {
        return holderReason.isEmpty() || !transmitSettingsPermitted(version)
            ? transmitSettingsReason(version)
            : holderReason;
    };
    if (m_stationClient && m_stationClient->knowsTransmitHolder()) {
        // A reason already shown (the radio on the air) stays.
        if (!holderReason.isEmpty()) {
            if (settingsPermitted) { settingsPermitted = false; settingsReason = holderReason; }
            if (chainPermitted) { chainPermitted = false; chainReason = holderReason; }
            if (profilePermitted) { profilePermitted = false; profileReason = holderReason; }
            if (processingPermitted) { processingPermitted = false; processingReason = holderReason; }
        }
        if (voxLine && !m_stationClient->holdsTransmitHere()) {
            voxLine = false;
            voxReason = holderReason.isEmpty() ? TxRefusals::notHolder().text : holderReason;
        }
    }
    TxEqDialog::setSettingsPermitted(processingPermitted, processingReason);
    if (m_txApplet) {
        m_txApplet->setTransmitPermitted(transmitPermitted, transmitReason);
        m_txApplet->setVoxPermitted(voxLine, voxReason);
        m_txApplet->setTransmitSettingsPermitted(settingsPermitted, settingsReason);
        m_txApplet->setTransmitChainSettingsPermitted(chainPermitted, chainReason);
        // Parity Task 32: the MON output pair needs a Core that sends MON.
        m_txApplet->setMonitorOutputPermitted(
            m_stationClient == nullptr
                || m_stationClient->capabilities().txMonitorAudioVersion >= 1,
            TxApplet::monitorOutputUnavailableReason());
        m_txApplet->setTxProfilePermitted(profilePermitted, profileReason);
        m_txApplet->setTxProcessingPermitted(processingPermitted, processingReason);
        // R-R3-49 (group A fix wave, M3): the RF Power slider's per-band
        // and drive-source writes came with version 5.
        m_txApplet->setPowerByBandPermitted(settingsAt(5));
        // R-R3-49 (parity Task 7): PS-A arms PureSignal (version 7).
        m_txApplet->setPureSignalArmingPermitted(
            holderReason.isEmpty() && pureSignalArmingPermitted(),
            holderReason.isEmpty() || !pureSignalArmingPermitted() ? pureSignalArmingReason()
                                                                   : holderReason);
    }
    if (m_phoneCwApplet) {
        m_phoneCwApplet->setTransmitPermitted(transmitPermitted, transmitReason);
        m_phoneCwApplet->setTransmitSettingsPermitted(chainPermitted, chainReason);
        m_phoneCwApplet->setTxProfilePermitted(profilePermitted, profileReason);
    }
    // R-R3-44: the VAX applet's TX row (VAX as the microphone).
    if (m_vaxApplet) {
        m_vaxApplet->setTransmitPermitted(transmitPermitted, transmitReason);
        // iPhone app plan Task 25: the Core computer's TX row, which the
        // Core takes only from a device that may transmit.
        m_vaxApplet->setStationTransmitPermitted(transmitPermitted, transmitReason);
    }
    // R-R3-49 (parity Task 1): the RX applet's TX passband Shift-click.
    // Its XIT row is a slice setting and takes no gate (parity Task 11).
    if (m_rxApplet) {
        m_rxApplet->setTransmitSettingsPermitted(settingsPermitted, settingsReason);
    }
    // R-R3-21: the RADE applet's profile combo writes the TX mic profile.
    if (m_radeApplet) {
        m_radeApplet->setTransmitPermitted(transmitPermitted, transmitReason);
        m_radeApplet->setTxProfilePermitted(profilePermitted, profileReason);
    }
    // Group B fix wave: BYPS follows whether the Core takes RX bypass on
    // TX (radioHardwareVersion 5), not the transmit permission.
    const bool rxBypass = rxBypassPermitted();
    const QString rxBypassReason = rxBypassUnavailableReason();
    for (VfoWidget* flag : m_vfoWidgetsBySlice) {
        if (flag) {
            applyFlagTransmitGate(flag);
            flag->setRxBypassPermitted(rxBypass, rxBypassReason);
        }
    }
    // R-R3-21: the container function buttons: the transmit ones are
    // unavailable with this reason, Power follows the Core session.
    refreshContainerControls();
    // R-R3-21 / R-R3-10: the Core's settings availability too. Pushed on
    // every link change: StationClient marks the settings not ready before
    // it reports a lost or closed session, and ready (with the snapshot)
    // before it reports the session established.
    const bool stationAvailable = stationSettingsAvailable();
    for (SetupDialog* dialog : findChildren<SetupDialog*>()) {
        dialog->setTransmitPermitted(transmitPermitted, transmitReason);
        dialog->setVoxPermitted(voxLine, voxReason);
        dialog->setTransmitSettingsPermitted(settingsPermitted, settingsReason);
        dialog->setTransmitSettingsPermitted(chainPermitted, chainReason, 2);
        dialog->setTransmitSettingsPermitted(profilePermitted, profileReason, 3);
        dialog->setTransmitSettingsPermitted(processingPermitted, processingReason, 4);
        // R-R3-49 (parity Task 5): Transmit > Power, DEXP/VOX and Test >
        // Two-Tone IMD came with version 5.
        dialog->setTransmitSettingsPermitted(settingsAt(5), settingsReasonAt(5), 5);
        // R-R3-49 (parity Task 6): Setup > PA came with version 6.
        dialog->setTransmitSettingsPermitted(settingsAt(6), settingsReasonAt(6), 6);
        // R-R3-49 (parity Task 13): Hardware Config's OC transmit pins, pin
        // actions and transmit calibration came with version 8.
        dialog->setTransmitSettingsPermitted(settingsAt(8), settingsReasonAt(8), 8);
        // Transmit > Power's "Disable HF PA" came with version 11.
        dialog->setTransmitSettingsPermitted(settingsAt(11), settingsReasonAt(11), 11);
        dialog->setStationSettingsAvailable(stationAvailable, stationSettingsReason());
    }
    // Parity Task 19 (B7.2): and the Spot Hub's Core settings and sources.
    refreshSpotHubAvailability();
    // iPhone plan Task 22: and the FreeDV Reporter dialog's requests.
    refreshFreedvReporterAvailability();
    if (m_actTxEqualizer) {
        // R-R3-49 (parity Task 4): opens whatever the Core says; the dialog
        // shows why it is greyed.
        m_actTxEqualizer->setEnabled(true);
        m_actTxEqualizer->setToolTip(tr("Open the TX equalizer."));
    }
    // The Tools menu's developer test entries fake an antenna switch and a
    // TX-bound re-route that nothing on the Core stands behind, so they
    // follow the same gate and give the same reason.
    if (m_actTestAntennaToast) {
        m_actTestAntennaToast->setEnabled(transmitPermitted);
        m_actTestAntennaToast->setToolTip(transmitPermitted
            ? testAntennaToastToolTip() : transmitReason);
    }
    if (m_actTestTxBoundReRoute) {
        m_actTestTxBoundReRoute->setEnabled(transmitPermitted);
        m_actTestTxBoundReRoute->setToolTip(transmitPermitted
            ? testTxBoundReRouteToolTip() : transmitReason);
    }
    if (m_tunerApplet) {
        m_tunerApplet->setTransmitPermitted(transmitPermitted, transmitReason);
        m_tunerApplet->setStationConnected(
            m_stationClient && m_stationClient->isHandshakeComplete());
    }
    for (QAction* action : {m_actPureSignal, m_actDspPureSignal}) {
        if (!action) { continue; }
        const bool ps3Supported = m_stationClient && m_stationClient->isHandshakeComplete()
            && m_stationClient->capabilities().psAlgorithmVersion == 3;
        // R-R3-49 (parity Task 7): a Core at transmitSettingsVersion 7 takes
        // calibration from here off the air; the two-tone test still waits
        // for remote transmit.
        const bool armingOffered = m_stationClient && m_stationClient->pureSignalArmingOffered();
        action->setEnabled(ps3Supported);
        action->setToolTip(!ps3Supported
            ? tr("The connected Core has not advertised PureSignal 3.")
            : armingOffered
                ? tr("PureSignal 3 settings, calibration, saved corrections and diagnostics. The two-tone test is not available from this Core.")
                : tr("PureSignal 3 settings, saved corrections and diagnostics. Remote transmit controls are not available from this Core."));
    }
    if (m_pureSignalApplet) {
        applyRemotePureSignalAppletGate();
    }
    if (m_actConnect != nullptr) {
        m_actConnect->setEnabled(m_connectionPickerManaged || (m_station.isRemote() && !active));
        m_actConnect->setToolTip(m_connectionPickerManaged
            ? tr("Choose a Core/radio pair or a radio for this computer")
            : tr("Connect to the configured Core"));
    }
    if (m_actDisconnect != nullptr) {
        m_actDisconnect->setEnabled(active);
        m_actDisconnect->setToolTip(
            tr("Disconnect from Core and stop automatic connection attempts"));
    }
    if (m_actManageRadios != nullptr) {
        // Parity Task 21 (R-IOS-18): a window started for one Core manages
        // that Core's radios on Setup > This Core.
        m_actManageRadios->setEnabled(true);
        m_actManageRadios->setToolTip(
            m_connectionPickerManaged
                ? tr("Choose a Core/radio pair or a radio for this computer")
                : tr("See and change the Core's radio (Setup > This Core)"));
    }
    // R-R3-46 / R-R3-21: the attenuator, preamp and auto-attenuate
    // controls (RX applet, Setup > General > Options) follow the Core's
    // `stepAtt` object; they are usable only while the Core takes this
    // window's edits, and otherwise say why in user words.
    if (StepAttenuatorFacade* stepAtt = m_radioModel->stepAttFacade()) {
        const bool hardware = m_stationClient != nullptr
            && m_stationClient->remoteRadioHardwareAvailable();
        stepAtt->setWindowAvailability(hardware, hardware ? QString()
            : m_stationClient != nullptr
                ? OperatorReasonText::forDisplay(m_stationClient->radioHardwareUnavailableReason())
                : tr("Connect to the Core to change the attenuator and preamp."));
    }
    // R-R3-46: Setup > Hardware Config's receive settings go through the
    // Core when it offers them (radioHardwareVersion 2); the page follows
    // this and otherwise says why in user words.
    if (AlexAntennaFacade* alex = m_radioModel->alexAntennaFacade()) {
        const bool hardwareConfig = m_stationClient != nullptr
            && m_stationClient->remoteHardwareConfigAvailable();
        alex->setWindowAvailability(hardwareConfig, hardwareConfig ? QString()
            : m_stationClient != nullptr
                ? OperatorReasonText::forDisplay(m_stationClient->hardwareConfigUnavailableReason())
                : tr("Connect to the Core to change the radio's hardware settings."));
        // R-R3-49 / R-R3-46 (parity Task 12): the transmit antennas and
        // relays go through the Core from radioHardwareVersion 6, RX bypass
        // on TX from 5, whatever the transmit permission says; a local
        // window's own controller always takes them.
        const bool localWindow = m_radioModel->ownsLocalDsp();
        const bool txAntennas = localWindow
            || (m_stationClient != nullptr && m_stationClient->remoteTransmitAntennasAvailable());
        alex->setTransmitEditAvailability(
            txAntennas, txAntennas ? QString()
                : m_stationClient != nullptr
                    ? OperatorReasonText::forDisplay(
                          m_stationClient->transmitAntennasUnavailableReason())
                    : tr("Connect to the Core to change the radio's hardware settings."),
            rxBypassPermitted(), OperatorReasonText::forDisplay(rxBypassUnavailableReason()));
    }
    // R-R3-46: Protocol Info shows the Core's radio (showCoreRadioInfo(),
    // never connection(), which a remote model does not have) once the
    // Core has described it.
    if (m_actProtocolInfo != nullptr) {
        const bool described = m_stationClient != nullptr
            && m_stationClient->isHandshakeComplete();
        m_actProtocolInfo->setEnabled(described);
        m_actProtocolInfo->setToolTip(described
            ? tr("Show the Core's radio: its name, firmware, MAC and network address")
            : tr("Unavailable until this window is connected to the Core."));
    }
}

// R-R3-46: Radio > Protocol Info in a remote window. The radio is the Core's,
// as its capabilities describe it; the lines match local mode's dialog.
void MainWindow::showCoreRadioInfo()
{
    if (m_radioModel == nullptr || m_stationClient == nullptr
        || !m_stationClient->isHandshakeComplete()) {
        return;
    }
    const StationCapabilities& caps = m_stationClient->capabilities();
    const RadioInfo& info = m_radioModel->currentRadioInfo();
    const QString notReported = tr("not reported by the Core");
    const QString name = !caps.stationName.isEmpty() ? caps.stationName
        : (!m_radioModel->model().isEmpty() ? m_radioModel->model() : notReported);
    const QString proto = caps.radioProtocol == 2 ? QStringLiteral("P2")
        : caps.radioProtocol == 1 ? QStringLiteral("P1") : notReported;
    const QString firmware = info.firmwareVersion > 0
        ? QString::number(info.firmwareVersion)
        : (!caps.firmwareVersion.isEmpty() ? caps.firmwareVersion : notReported);
    const QString mac = info.macAddress.isEmpty() ? notReported : info.macAddress;
    const QString address = info.address.isNull() ? notReported : info.address.toString();
    const QString msg =
        QStringLiteral("Radio:    %1\nProtocol: %2\nFirmware: %3\nMAC:      %4\nIP:       %5")
            .arg(name, proto, firmware, mac, address);
    QMessageBox::information(this, QStringLiteral("Protocol Info"), msg);
}

// Phase 3Q Sub-PR-4 D.2 — right-click context menu on the TitleBar
// ConnectionSegment. "Reconnect" is intentionally absent: RadioModel has no
// public reconnect() API (tryAutoReconnect() is private to MainWindow and
// starts a full probe + discovery cycle, which is not appropriate to invoke
// from a context menu that the user might trigger mid-session). The user can
// use "Connect to other radio…" to re-select the same radio.
void MainWindow::showSegmentContextMenu(const QPoint& globalPos)
{
    QMenu menu(this);
    if (!m_radioModel->ownsLocalDsp()) {
        menu.setToolTipsVisible(true);
        menu.addAction(m_actConnect);
        menu.addAction(m_actDisconnect);
        menu.addAction(tr("Core connection details..."),
                       this, &MainWindow::showRemoteConnectionPanel);
        // Parity Task 21 (R-IOS-18, B6.2): the Core's radio.
        menu.addSeparator();
        addCoreRadioActions(menu);
        // B6.3: the Core's radio's address, as a local window copies its own.
        menu.addSeparator();
        const QString ip = coreRadioAddressText();
        const QString mac = coreRadioMacText();
        QAction* copyIp = menu.addAction(tr("Copy IP address"), this, [ip]() {
            QGuiApplication::clipboard()->setText(ip);
        });
        QAction* copyMac = menu.addAction(tr("Copy MAC address"), this, [mac]() {
            QGuiApplication::clipboard()->setText(mac);
        });
        const QString none = tr("The Core has not reported its radio.");
        copyIp->setEnabled(!ip.isEmpty());
        copyIp->setToolTip(ip.isEmpty() ? none : QString());
        copyMac->setEnabled(!mac.isEmpty());
        copyMac->setToolTip(mac.isEmpty() ? none : QString());
        menu.exec(globalPos);
        return;
    }

    // R2 Task 20: Disconnect is gated the same way the Radio menu's is.
    // Harmless today (teardownConnection() returns early on the null
    // connection a remote model always has), but an enabled control that
    // does nothing is exactly what this gate exists to remove, and its
    // sibling below is already gated through showConnectionPanel().
    QAction* disconnectAction = menu.addAction(tr("Disconnect"), this, [this]() {
        m_radioModel->disconnectFromRadio();
    });
    if (m_radioModel != nullptr && !m_radioModel->ownsLocalDsp()) {
        disconnectAction->setEnabled(false);
        disconnectAction->setToolTip(
            tr("Unavailable: this window is driving a remote Core. "
               "The Core owns the radio connection."));
    }
    menu.addAction(tr("Connect to other radio…"), this, [this]() {
        showConnectionPanel();
    });
    menu.addSeparator();
    // R2 Task 20: third of the three former inline constructions, now the
    // named slot so the remote gate covers this one too.
    menu.addAction(tr("Network diagnostics…"), this,
                   &MainWindow::openNetworkDiagnostics);
    menu.addSeparator();
    menu.addAction(tr("Copy IP address"), this, [this]() {
        QGuiApplication::clipboard()->setText(m_radioModel->connectionIpText());
    });
    menu.addAction(tr("Copy MAC address"), this, [this]() {
        QGuiApplication::clipboard()->setText(m_radioModel->connectionMacText());
    });

    menu.exec(globalPos);
}

void MainWindow::showStationContextMenu(const QPoint& globalPos)
{
    // Only show when connected — StationBlock only emits contextMenuRequested
    // in connected appearance, but guard here defensively.
    if (m_radioModel->ownsLocalDsp()
        && m_radioModel->connectionState() != ConnectionState::Connected) {
        return;
    }

    QMenu menu(this);
    if (!m_radioModel->ownsLocalDsp()) {
        menu.setToolTipsVisible(true);
        menu.addAction(m_actConnect);
        menu.addAction(m_actDisconnect);
        menu.addAction(tr("Core connection details..."),
                       this, &MainWindow::showRemoteConnectionPanel);
        // Parity Task 21 (R-IOS-18, B6.2): the Core's radio.
        menu.addSeparator();
        addCoreRadioActions(menu);
        menu.exec(globalPos);
        return;
    }

    // Fix round 4: all three entries below are gated the way
    // showSegmentContextMenu()'s Disconnect already is. That sibling gained
    // its gate in Task 20 and this one did not, so a remote client got a
    // fully live station menu offering three actions that between them
    // return early, open a panel that is itself suppressed, and delete a
    // saved radio this window is not connected to.
    //
    // Each is harmless in the sense of not crashing -- disconnectFromRadio()
    // returns early on the null connection a remote model always has,
    // showConnectionPanel() is gated inside itself, and the MAC these read
    // off connection() is empty so forgetRadio() is skipped. The standard
    // the sibling's own comment sets is not "does it crash", it is: "an
    // enabled control that does nothing is exactly what this gate exists to
    // remove".
    const bool localDsp =
        (m_radioModel != nullptr) && m_radioModel->ownsLocalDsp();
    const QString remoteWhy =
        tr("Unavailable: this window is driving a remote Core. "
           "The Core owns the radio connection.");

    QAction* disconnectAction = menu.addAction(tr("Disconnect"), this, [this]() {
        m_radioModel->disconnectFromRadio();
    });

    // "Edit radio…" — open ConnectionPanel so the user can edit the currently
    // connected radio's settings (model override, etc.). The panel pre-selects
    // by highlighted MAC when available; if not connected, user clicks the row.
    QAction* editAction = menu.addAction(tr("Edit radio…"), this, [this]() {
        showConnectionPanel();
        if (m_connectionPanel) {
            const QString mac =
                m_radioModel->connection()
                    ? m_radioModel->connection()->radioInfo().macAddress
                    : QString();
            if (!mac.isEmpty()) {
                m_connectionPanel->highlightMac(mac);
            }
        }
    });

    QAction* forgetAction = menu.addAction(tr("Forget radio"), this, [this]() {
        const QString mac =
            m_radioModel->connection()
                ? m_radioModel->connection()->radioInfo().macAddress
                : QString();
        m_radioModel->disconnectFromRadio();
        if (!mac.isEmpty()) {
            AppSettings::instance().forgetRadio(mac);
        }
    });

    if (!localDsp) {
        disconnectAction->setEnabled(false);
        disconnectAction->setToolTip(remoteWhy);
        editAction->setEnabled(false);
        editAction->setToolTip(
            tr("Unavailable: this window was started for one Core with "
               "--station, so the radio list cannot change it."));
        forgetAction->setEnabled(false);
        forgetAction->setToolTip(
            tr("Unavailable: this window was started for one Core with "
               "--station, so the radio list cannot change it."));
    }

    menu.exec(globalPos);
}

void MainWindow::showSupportDialog()
{
    if (!m_supportDialog) {
        m_supportDialog = new SupportDialog(m_radioModel, this);
        m_supportDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_supportDialog, &QObject::destroyed, this, [this]() {
            m_supportDialog = nullptr;
        });
    }
    m_supportDialog->show();
    m_supportDialog->raise();
    m_supportDialog->activateWindow();
}

// Phase 3M-4 Task 8: open the modeless PureSignal dialog (Tools >
// PureSignal... and DSP > PureSignal...).  Lazy-constructs on the first
// call; subsequent calls show + raise the existing instance so geometry
// persists across opens.  Source-first port of Thetis console.cs:43099-
// 43104 linearityToolStripMenuItem_Click [v2.10.3.13]:
//
//   if (psform == null) psform = new PSForm(this);
//   psform.Show();
//   psform.Focus();
//
// NereusSDR mirrors via raise()+activateWindow() instead of Focus().
void MainWindow::openPureSignalDialog()
{
    if (!m_psForm) {
        // PureSignal coordinator is owned by RadioModel; pass it directly so
        // the dialog can wire signal/slot bindings even before connect.
        // RadioModel is the owner; we keep a non-owning pointer so the
        // dialog tolerates RadioModel-less startup (covered by tst_psform
        // construction-time test).
        PureSignal* coordinator =
            (m_radioModel ? m_radioModel->pureSignal() : nullptr);
        m_psForm = new PsForm(m_radioModel, coordinator, this);
        connect(m_psForm, &PsForm::showTwoToneMeasurementsChanged,
                this, [this](bool shown) {
            if (auto* spectrum = activeSpectrumWidget()) {
                spectrum->setShowIMDMeasurements(shown);
            }
        });
    }
    m_psForm->show();
    m_psForm->raise();
    m_psForm->activateWindow();
}

// Phase 3J-2 H1: open the modeless SpotHubDialog.
//
// Lazy-constructs on first invocation, wiring all 7 spot-ingest clients
// + SpotModel + DxccColorProvider from RadioModel (Task H2 made these
// accessible via getters). Subsequent calls show + raise the existing
// instance so geometry, table sort state, and per-tab settings persist
// across opens. QPointer guards the pointer in case the dialog is ever
// deleted by some external path; lazy reconstruction is then automatic.
//
// Mirrors the modeless-singleton pattern at AetherSDR
// src/gui/MainWindow.cpp openDxClusterDialog() [@0cd4559].
void MainWindow::openSpotHub()
{
    if (!m_radioModel) { return; }
    if (!m_spotHubDialog) {
        m_spotHubDialog = new SpotHubDialog(
            m_radioModel->dxCluster(),
            m_radioModel->rbn(),
            m_radioModel->wsjtx(),
            m_radioModel->spotCollector(),
            m_radioModel->pota(),
            m_radioModel->freeDvReporter(),
            m_radioModel->pskReporter(),
            m_radioModel->spotModel(),
            m_radioModel->spotTableModel(),
            m_radioModel->dxccColorProvider(),
            this);
        // Bridge spotsClearedAll (Display tab's "Clear All Spots" button)
        // to SpotModel::clear so the global QShortcut and the dialog
        // button share one truth-source.
        // Parity Task 19 (R-IOS-25): through the spot source host, which
        // clears this window's spots and, in a remote window, the Core's.
        connect(m_spotHubDialog.data(), &SpotHubDialog::spotsClearedAll,
                m_radioModel->spotSourceHost(), &SpotSourceHost::clearAllSpots);
        // Spot List double-click tuneRequested(double Mhz) drives the
        // active slice. SliceModel::setFrequency takes Hz (double), so
        // multiply by 1e6 to convert MHz to Hz.
        connect(m_spotHubDialog.data(), &SpotHubDialog::tuneRequested,
                this, [this](double freqMhz) {
                    if (auto* slice = activeSliceForWindow()) {
                        slice->setFrequency(freqMhz * 1.0e6);
                    }
                });
        // Phase 3J-2 + 3R M2: Display tab knob round-trip.
        // SpotHubDialog F4 writes every knob change to AppSettings and
        // emits settingsChanged. SpectrumWidget::loadSpotDisplaySettings
        // pulls the new values back out and pushes them into the spot
        // overlay setters in one go. Mirrors AetherSDR's refreshSpots
        // lambda (src/models/RadioModel.cpp [@0cd4559]) but the
        // NereusSDR shape lives on the widget so the test seam is local
        // (see tst_spothub_display_knobs).
        connect(m_spotHubDialog.data(), &SpotHubDialog::settingsChanged,
                this, [this] {
                    if (activeSpectrumWidget()) {
                        activeSpectrumWidget()->loadSpotDisplaySettings();
                    }
                });
        // Defensive re-seed.  The primary seed runs at MainWindow startup
        // (see the QTimer::singleShot(0, ...) lambda earlier in this file
        // that pairs loadSpotDisplaySettings + restoreSpotClientAutoStartState),
        // which guarantees the panadapter mask is populated before any
        // auto-started spot client emits its first spot.  This call is kept
        // as a belt-and-suspenders idempotent re-seed for the (rare) case
        // where the spectrum widget was not yet available at startup but
        // is now -- loadSpotDisplaySettings re-reads AppSettings and the
        // setter is no-op when the value is unchanged, so the cost is
        // bounded and the behaviour is correct either way.
        if (activeSpectrumWidget()) {
            activeSpectrumWidget()->loadSpotDisplaySettings();
        }

        // Phase 3R K-bench (bench feedback): wire the FreeDV tab's
        // Start / Stop button signals to the actual client lifecycle.
        // Previously the button emitted freedvStartRequested /
        // freedvStopRequested but nothing handled the Stop side, so
        // clicking Stop appeared to do nothing.
        //
        // iPhone plan Task 22 / parity Task 20 (R-IOS-26): through the spot
        // source host, which sets the saved identity before it connects and,
        // in a remote window, asks the Core, which runs FreeDV Reporter.
        if (SpotSourceHost* host = m_radioModel->spotSourceHost()) {
            connect(m_spotHubDialog.data(), &SpotHubDialog::freedvStartRequested, host,
                    &SpotSourceHost::startFreedvReporter);
            connect(m_spotHubDialog.data(), &SpotHubDialog::freedvStopRequested, host,
                    &SpotSourceHost::stopFreedvReporter);
        }

        // 2026-05-12 bench fix: wire the remaining 10 SpotHubDialog
        // lifecycle signals to their respective client methods.  Without
        // these connects the per-tab Connect / Start / Stop buttons in
        // SpotHubDialog emit signals into the void — the FreeDV pair
        // above was wired but DX Cluster, RBN, WSJT-X, SpotCollector,
        // and POTA buttons all silently no-op'd.
        //
        // Parity Task 19 (R-IOS-25): the starts and stops moved into the
        // spot source host (the same client calls, and the PSK Reporter
        // Start that arms the client's reporting interval after setting
        // the freshly checked identity). In a remote window it sends the
        // DX cluster, RBN, POTA and PSK Reporter buttons to the Core, which
        // runs them; WSJT-X and SpotCollector listen on this computer.
        if (SpotSourceHost* host = m_radioModel->spotSourceHost()) {
            SpotHubDialog* hub = m_spotHubDialog.data();
            connect(hub, &SpotHubDialog::connectRequested, host, &SpotSourceHost::connectCluster);
            connect(hub, &SpotHubDialog::disconnectRequested, host,
                    &SpotSourceHost::disconnectCluster);
            connect(hub, &SpotHubDialog::rbnConnectRequested, host, &SpotSourceHost::connectRbn);
            connect(hub, &SpotHubDialog::rbnDisconnectRequested, host,
                    &SpotSourceHost::disconnectRbn);
            connect(hub, &SpotHubDialog::wsjtxStartRequested, host, &SpotSourceHost::startWsjtx);
            connect(hub, &SpotHubDialog::wsjtxStopRequested, host, &SpotSourceHost::stopWsjtx);
            connect(hub, &SpotHubDialog::spotCollectorStartRequested, host,
                    &SpotSourceHost::startSpotCollector);
            connect(hub, &SpotHubDialog::spotCollectorStopRequested, host,
                    &SpotSourceHost::stopSpotCollector);
            connect(hub, &SpotHubDialog::potaStartRequested, host, &SpotSourceHost::startPota);
            connect(hub, &SpotHubDialog::potaStopRequested, host, &SpotSourceHost::stopPota);
            connect(hub, &SpotHubDialog::pskStartRequested, host,
                    &SpotSourceHost::startPskReporter);
            connect(hub, &SpotHubDialog::pskStopRequested, host,
                    &SpotSourceHost::stopPskReporter);
            hub->setSourceHost(host);
        }
        refreshSpotHubAvailability();

        // 2026-05-12 bench fix: Save & Propagate writes User/GridSquare
        // to AppSettings but the FreeDVStationModel only reads its
        // m_ourGrid once at RadioModel construction.  Without this
        // forward the Reporter dialog's Distance + Hdg columns stay
        // zeroed until app restart.  Push the new grid into the model
        // on every save so distance/heading recompute live.
        connect(m_spotHubDialog.data(), &SpotHubDialog::identitySaved,
                this, [this](const QString& /*call*/,
                             const QString& grid,
                             const QString& /*msg*/) {
                    if (auto* sm = m_radioModel
                            ? m_radioModel->freeDvStationModel()
                            : nullptr) {
                        if (!grid.isEmpty()) {
                            sm->setOurGridSquare(grid);
                        }
                    }
                });

        // 2026-05-12 bench fix (Gap #6 — spot list ↔ panadapter hover sync).
        // Bidirectional: panadapter hover highlights the Spot List row,
        // Spot List hover paints a halo on the matching panadapter
        // label.  Lazy-wired here because both widgets are needed; the
        // dialog is constructed on first open.
        if (activeSpectrumWidget()) {
            connect(activeSpectrumWidget(), &SpectrumWidget::spotHoverIndexChanged,
                    m_spotHubDialog.data(),
                    &SpotHubDialog::setHoveredPanadapterSpot);
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::spotListHoverChanged,
                    activeSpectrumWidget(),
                    &SpectrumWidget::setHoverSpotIndexExternal);
        }
    }
    m_spotHubDialog->show();
    m_spotHubDialog->raise();
    m_spotHubDialog->activateWindow();
}

// Phase 3J-2 H1: open the modeless FreeDVReporterDialog.
//
// Lazy-constructs on first invocation, wiring FreeDVStationModel +
// FreeDVReporterClient from RadioModel. Same singleton + show / raise
// pattern as openSpotHub.
//
// Wires three downstream connections:
//   qsyRequested -> FreeDVReporterClient::requestQSY (network QSY)
//   messageSendRequested -> FreeDVReporterClient::updateMessage
//   tuneRequested -> active SliceModel::setFrequency (local QSY)
//
// The dialog already calls setAttribute(Qt::WA_DeleteOnClose, false)
// in its own ctor so close + reopen preserves state.
void MainWindow::openFreeDVReporter()
{
    if (!m_radioModel) { return; }
    if (!m_freeDVReporterDialog) {
        m_freeDVReporterDialog = new FreeDVReporterDialog(
            m_radioModel->freeDvStationModel(),
            m_radioModel->freeDvReporter(),
            this);
        // QSY: dialog -> reporter client -> network broadcast.
        // iPhone plan Task 22 / parity Task 20 (R-IOS-26): in a remote
        // window the list is the Core's, so the request names the row's
        // callsign and the Core sends it (freedv.sendQsy).
        connect(m_freeDVReporterDialog.data(), &FreeDVReporterDialog::qsyRequested, this,
                [this](const QString& sid, quint64 freqHz, const QString& message) {
            if (m_radioModel == nullptr) {
                return;
            }
            SpotSourceHost* host = m_radioModel->spotSourceHost();
            if (host != nullptr && host->forwardsStationSources()) {
                const QString callsign =
                    m_radioModel->freeDvStationModel()->stationBySid(sid).callsign;
                host->requestFreedvQsy(callsign, static_cast<qint64>(freqHz));
                return;
            }
            if (FreeDVReporterClient* client = m_radioModel->freeDvReporter()) {
                client->requestQSY(sid, freqHz, message);
            }
        });
        // Message update: dialog -> reporter client (the Core's in a remote
        // window, freedv.setMessage).
        if (SpotSourceHost* host = m_radioModel->spotSourceHost()) {
            connect(m_freeDVReporterDialog.data(), &FreeDVReporterDialog::messageSendRequested,
                    host, &SpotSourceHost::sendFreedvMessage);
            // A refused request shows as the Core's other refusals do.
            connect(host, &SpotSourceHost::sourceRefused, this,
                    [this](const QString& source, const QString& reason) {
                if (source == SpotSourceHost::kFreedvReporter && m_freeDVReporterDialog
                    && m_freeDVReporterDialog->isVisible() && !reason.isEmpty()) {
                    showToast(reason, ToastSeverity::Warning, 5000);
                }
            });
        }
        // Local QSY: dialog -> active slice tune. tuneRequested signature
        // is quint64 Hz so no MHz conversion needed.
        connect(m_freeDVReporterDialog.data(),
                &FreeDVReporterDialog::tuneRequested,
                this, [this](quint64 freqHz) {
                    if (auto* slice = activeSliceForWindow()) {
                        slice->setFrequency(static_cast<double>(freqHz));
                    }
                });

        // Phase 3R K-bench (bench feedback): wire active slice VFO ->
        // dialog so the Band/Exact-freq filter actually tracks. Push
        // current value immediately + on every frequencyChanged.
        if (auto* slice = activeSliceForWindow()) {
            m_freeDVReporterDialog->setActiveFrequency(
                static_cast<quint64>(slice->frequency()));
            connect(slice, &SliceModel::frequencyChanged,
                    m_freeDVReporterDialog.data(),
                    [this](double hz) {
                        if (m_freeDVReporterDialog) {
                            m_freeDVReporterDialog->setActiveFrequency(
                                static_cast<quint64>(hz));
                        }
                    });
        }
    }
    refreshFreedvReporterAvailability();
    m_freeDVReporterDialog->show();
    m_freeDVReporterDialog->raise();
    m_freeDVReporterDialog->activateWindow();
}

// panIdsForLayout(): synthesize a "pan-0" .. "pan-(N-1)" id list sized to
// a layout template's pan count. Two callers: applyPanLayout() below,
// reached from PanLayoutDialog's thumbnail-grid accept path (Task B3/B4),
// and the launch-time restoredLayout call near the top of this file.
//
// This used to also carry a 20-line doc block for showPanMenu(), the old
// +PAN dropdown that built the same table inline across three sections
// (add-slice on the active pan / layout / float the active pan).
// showPanMenu() was replaced by PanLayoutDialog's thumbnail grid
// (Task B3), and its two per-pan actions moved to each pan's own
// right-click menu so they carry that pan's own id instead of routing
// through activePanId() (Task B5, design doc §8.5); neither reads this
// function's five-template comment any more, and the table itself has
// grown to the current nine layouts (design doc §8.3).
// Codex review round 3, PR #293. The template-to-pan-count table had three
// copies; this is the only one now.
QStringList MainWindow::panIdsForLayout(const QString& layoutId)
{
    // One table, so a new layout is a one-line addition here and a branch in
    // PanadapterStack::applyLayout, rather than a chain of ternaries that
    // silently defaults new ids to 2. Counts match design §8.3.
    static const QHash<QString, int> kPanCount = {
        {QStringLiteral("1"),   1},
        {QStringLiteral("2v"),  2},
        {QStringLiteral("2h"),  2},
        {QStringLiteral("2h1"), 3},
        {QStringLiteral("12h"), 3},
        {QStringLiteral("3v"),  3},
        {QStringLiteral("2x2"), 4},
        {QStringLiteral("4v"),  4},
        {QStringLiteral("3h2"), 5},
    };
    const int needed = kPanCount.value(layoutId, 1);
    QStringList ids;
    ids.reserve(needed);
    for (int i = 0; i < needed; ++i) {
        ids << QStringLiteral("pan-%1").arg(i);
    }
    return ids;
}

// Codex review round 3, PR #293. See MainWindow.h for why this exists.
void MainWindow::applyPanLayout(const QString& layoutId)
{
    if (!m_panStack) { return; }
    if (m_station.isRemote()
        && (!m_stationClient || !m_stationClient->isHandshakeComplete())) {
        showToast(tr("Wait until this window has the Core's settings before changing "
                     "the pan layout."),
                  ToastSeverity::Info, 3000);
        return;
    }

    const QStringList ids = panIdsForLayout(layoutId);

    // Slice control plan Task 16 (ruling U7): a window that shares slices
    // moves only the slices it controls, and a slice it only listens to is
    // placed where this window shows it. Read before the layout changes, so
    // the pans a listened slice was shown on are the ones it had.
    const bool shared = m_radioModel && windowSharesSlices();
    RadioModel::PanScope scope;
    if (shared) {
        for (SliceModel* slice : m_radioModel->slices()) {
            if (!slice) { continue; }
            const int id = slice->sliceIndex();
            if (windowControlsSlice(id)) {
                scope.controlled.insert(id);
            } else if (windowListensTo(id)) {
                const QString shownOn = windowPanFor(slice);
                if (!shownOn.isEmpty()) { scope.listenedOn.insert(id, shownOn); }
            }
        }
    }

    qCInfo(lcContainer) << "Pan layout: applying" << layoutId << "with ids" << ids;
    m_panStack->applyLayout(layoutId, ids);

    if (!m_radioModel) { return; }

    if (shared) {
        // Listening ends only when the operator ends it (Stop listening,
        // Release or a close). A listened slice whose pan the layout
        // retired moves onto a pan that remains, the way a controlled slice
        // is rehomed, and keeps listening. Only this window's placement
        // changes; the slice's pan for its controller stays put. Placed
        // before the controlled slices rehome, so the FFT routing pass
        // their pan change triggers already finds it on its placed pan and
        // leaves that pan's view where the operator had it.
        const QString survivor = ids.value(0);
        for (int off : m_radioModel->listenedOffPans(ids, scope)) {
            scope.listenedOn.insert(off, survivor);
            m_listenPlacement.insert(off, survivor);
            // Only its flag and its own edge marker on that pan: never its
            // stream, its view or its VFO (JJ's ruling, 2026-09-30).
            m_markerOnlyPlacement.insert(off);
            m_panStack->moveSliceToPan(off, survivor);
        }
        // Controlled slices rehome as before; nothing else moves, and no
        // stream or DDC is touched.
        const int rehomed = m_radioModel->rehomeSlicesToPans(ids, &scope);
        if (rehomed > 0) {
            qCInfo(lcContainer) << "Layout: rehomed" << rehomed
                                << "controlled slice(s) onto" << ids.value(0);
        }
        // Flags on a pan the layout retired move to a pan that remains.
        for (SliceModel* slice : m_radioModel->slices()) {
            if (!slice) { continue; }
            const QString placed = m_listenPlacement.value(slice->sliceIndex());
            if (!placed.isEmpty() && !ids.contains(placed)) {
                m_listenPlacement.remove(slice->sliceIndex());
                m_markerOnlyPlacement.remove(slice->sliceIndex());
            }
            rehostSliceView(slice);
        }
        // A slice placed only as a marker is never the pan's active slice
        // ahead of a slice this window controls there (PanadapterApplet::
        // addSlice makes the first slice added active on an empty pan).
        if (PanadapterApplet* kept = m_panStack->panadapter(survivor)) {
            if (markerOnlyPlacement(kept->activeSliceIndex())) {
                QList<int> hosted = kept->associatedSlices().values();
                std::sort(hosted.begin(), hosted.end());
                for (int id : std::as_const(hosted)) {
                    if (windowControlsSlice(id) && !markerOnlyPlacement(id)) {
                        kept->setActiveSliceIndex(id);
                        break;
                    }
                }
            }
        }
        m_radioModel->spreadSlicesOntoEmptyPans(ids, &scope);
        // populatePanSlices' rule, counting only what this window shows.
        if (m_radioModel->ownsLocalDsp()
            || (m_stationClient && m_stationClient->isHandshakeComplete())) {
            for (const QString& emptyPan : m_radioModel->pansWithoutSlices(ids, &scope)) {
                if (HostingSliceActions* hosting = hostingSlices()) {
                    hosting->addOnPan(emptyPan);
                } else {
                    m_radioModel->addSliceOnPan(emptyPan);
                }
            }
        }
        rebuildFftRouting();
        refreshSliceChooser();
        refreshForeignMarkers();
        return;
    }

    // Shrink first. Slices left on panes applyLayout just deleted would keep a
    // dangling panKey, lose their VFO widget, and hold a DDC, a stream and
    // audio the operator can no longer reach. Running this before the grow
    // step also means the occupancy question below sees the settled answer.
    const int rehomed = m_radioModel->rehomeSlicesToPans(ids);
    if (rehomed > 0) {
        qCInfo(lcContainer) << "Layout: rehomed" << rehomed
                            << "slice(s) onto" << ids.value(0);
    }

    // Grow. Every pan wants a slice so that it has a VfoWidget and an RX
    // applet entry (Phase 3F bench fix 2026-06-03).
    //
    // Asked as an occupancy question rather than as
    // `for (i = slices().size(); i < target; ++i)`. That form assumed the
    // slice COUNT is the first unoccupied pan index, which stops being true
    // the moment slices co-host or get rehomed: shrinking a 2x2 to one pane
    // puts all four slices on pan-0, and expanding back then saw
    // existing == target, added nothing, and left three panes empty.
    // (Codex review round 4, PR #293.)
    //
    // No maxSlices arithmetic here: addSliceOnPan enforces the cap itself and
    // emits sliceAddRejected with an operator-facing reason when it cannot,
    // so restating it would be a second copy of that policy.
    //
    // Spread before creating. After a shrink every slice is co-hosted on
    // pan-0, so the slices these empty pans need already exist. Creating new
    // ones instead spends the maxSlices budget filling one pan and leaves the
    // rest empty with a surplus slice in the model. (Codex review round 5.)
    const int spread = m_radioModel->spreadSlicesOntoEmptyPans(ids);
    if (spread > 0) {
        qCInfo(lcContainer) << "Layout: spread" << spread
                            << "co-hosted slice(s) onto empty pans";
    }

    // Whatever is still empty after the surplus has been used up genuinely
    // needs a new slice.
    populateEmptyPans(true);
}

// ---------------------------------------------------------------------------
// populateEmptyPans — every pan needs a slice to be worth anything
//
// A pan with no slice has no VfoWidget, no RX applet entry, and no stream
// feeding it: it renders as an empty box with a 0.0000 flag.
//
// Called from two places, and the second one is why this is a function.
// applyPanLayout calls it because a layout change can add panes. The connect
// handler calls it because the startup layout restore deliberately does NOT
// (MainWindow.cpp, the PanLayoutId block): at startup no radio is connected
// and the stream pool is unsized, so manufacturing slices there would bind
// nothing. Correct as far as it goes, but nothing finished the job once a
// radio did connect.
//
// Bench-caught 2026-08-01 (J.J. Boyd, KG4VCF): quit with a 2v layout, relaunch,
// connect, and the second pan is permanently dead until the operator notices
// they have to add Slice B by hand. The log gives it away by omission, with no
// "Pan layout: applying" line anywhere in the session.
//
// addSliceOnPan enforces the maxSlices cap itself and emits sliceAddRejected
// with an operator-facing reason, so there is no cap arithmetic here.
// ---------------------------------------------------------------------------
void MainWindow::populateEmptyPans(bool operatorRequested)
{
    if (!m_radioModel || !m_panStack) { return; }

    // The pans that actually exist, not panIdsForLayout's template. After a
    // restore those are the same, but reading the live stack means a pan
    // created by any other route is covered too.
    QStringList ids;
    for (const PanadapterApplet* applet : m_panStack->allApplets()) {
        if (applet) { ids << applet->panId(); }
    }

    populatePanSlices(m_radioModel, ids, operatorRequested,
                      m_stationClient && m_stationClient->isHandshakeComplete(),
                      hostingSlices());
}

void MainWindow::populatePanSlices(RadioModel* model, const QStringList& panIds,
                                  bool operatorRequested, bool snapshotReady,
                                  HostingSliceActions* hosting)
{
    if (!model) { return; }
    // Restoring a client layout is never permission to create station slices.
    // Capabilities report radio connectivity before slice snapshot hydration.
    if (!model->ownsLocalDsp() && (!operatorRequested || !snapshotReady)) { return; }
    for (const QString& emptyPan : model->pansWithoutSlices(panIds)) {
        // Slice control plan Task 11: a hosting window's new slice is the
        // station device's, asked for the way its +RX asks.
        if (hosting) {
            hosting->addOnPan(emptyPan);
        } else {
            model->addSliceOnPan(emptyPan);
        }
    }
}

// Task B4: replaces the showPanMenu() context menu. Its layout section
// listed ids as bare strings and is superseded by PanLayoutDialog's
// thumbnail grid (Task B3); its two per-pan actions (add slice on active
// pan, float active pan) move to each pan's own right-click menu in
// Task B5, since both routed through activePanId() and a control drawn on
// a pan should target that pan, not "whichever one is active."
void MainWindow::showPanLayoutDialog()
{
    if (!m_radioModel || !m_radioModel->isConnected()) {
        return;
    }
    // Gate on the DDC-derived ceiling, not raw maxSlices: opening a NEW
    // pan always claims its own DDC (SliceStreamAllocator::placeSlice,
    // preferOwnStream=true; see the ruling comment at
    // SliceStreamAllocator.cpp:81-86), so a board like HL2 (maxSlices=5,
    // userDdcCount=2) can never fill more than 2 independent pans even
    // though it can host 5 slices total. Gating on maxSlices alone showed
    // tiles the board could paint but never fill (final-fix-wave finding 2).
    // userStreamCount() is the one stream count (plan Task 11): it knows the
    // protocol (four on Protocol 1) and, on a remote window, the Core's.
    const int maxPanCount = panLayoutLimitFor(m_radioModel);
    const QString boardName = m_radioModel->name();
    PanLayoutDialog dlg(maxPanCount,
                        m_panStack ? m_panStack->currentLayoutId()
                                   : QStringLiteral("1"),
                        boardName, this);
    if (dlg.exec() == QDialog::Accepted && !dlg.selectedLayout().isEmpty()) {
        applyPanLayout(dlg.selectedLayout());
    }
}

// Parity Task 18: the mode half of a spot's left-click, from AetherSDR
// src/gui/MainWindow_Wiring.cpp:4382-4437 [@1e0718ad]: a spot that is gone
// or a slice that is not there does nothing; "Auto mode" off (the Spot Hub's
// Display tab, SpotAutoSwitchMode, on by default as in AetherSDR) leaves the
// mode; otherwise the slice takes the spot's mode when it differs.
void MainWindow::applySpotModeToSlice(RadioModel* model, SliceModel* slice, int spotIndex)
{
    if (!model || !model->spotModel()) { return; }
    const auto& spots = model->spotModel()->spots();
    const auto it = spots.find(spotIndex);
    if (it == spots.end()) { return; }
    if (AppSettings::instance().value(QStringLiteral("SpotAutoSwitchMode"),
                                      QStringLiteral("True")).toString()
        != QStringLiteral("True")) {
        return;
    }
    if (!slice) { return; }
    const std::optional<DSPMode> mode = SpotModeResolver::dspModeForSpot(*it);
    if (mode && *mode != slice->dspMode()) {
        slice->setDspMode(*mode);
    }
}

// Parity Task 18: the slice limit is RadioModel::maxSlices(), the Core's
// advertised (effective) limit in a remote window and the board's own
// locally; the board's raw BoardCapabilities::maxSlices ignored what the Core
// said it can sustain.
int MainWindow::panLayoutLimitFor(const RadioModel* model)
{
    if (model == nullptr) { return 1; }
    return qMin(model->maxSlices(), model->userStreamCount());
}

// Phase 3M-4 bench-fix: PSA bottom-banner indicator visibility
// gated on (caps.hasPureSignal && PureSignal::isAutoCalEnabled).
// Centralised so onConnectionStateChanged + autoCalEnabledChanged +
// pureSignalCoordinatorReady can all share one truth-source.
//
// m_psaIndicator is registered with m_chromeBar at rung 0 so its width
// (two QLabel minimumWidth pins, ~154 px) is counted in the fold budget
// on every PS-capable, PS-armed board (Task A8 fix round 1 finding 2).
// The armed fact itself is reported via setItemAvailable, not a direct
// setVisible call, per ChromeBarController::setItemAvailable's own doc
// comment.
void MainWindow::updatePsaIndicatorVisibility()
{
    if (!m_psaIndicator) { return; }
    const bool caps = m_radioModel && m_radioModel->isConnected()
        && m_radioModel->pureSignalFacade()->available();
    const bool armed = m_radioModel && m_radioModel->pureSignalSettings()->autoCalEnabled();
    if (m_chromeBar && m_chromeBarWidget) {
        m_chromeBar->setItemAvailable(m_psaIndicator, caps && armed);
        m_chromeBar->relayout(m_chromeBarWidget->width());
    }
}

// R-R3-49 / R-R3-21: the rule PortAudioBus::portAudioBarredForTestRun
// applies to audio devices, applied to first-run prompts. Test mode is
// switched on before main() in every test binary (tests/TestSandboxInit.cpp)
// and never in the app, so the app's behaviour is unchanged.
bool MainWindow::firstRunPromptsBarredForTestRun()
{
#ifdef NEREUS_BUILD_TESTS
    return QStandardPaths::isTestModeEnabled();
#else
    return false;
#endif
}

void MainWindow::showAudioDiagnoseDialog()
{
#if defined(Q_OS_LINUX)
    AudioEngine* eng = m_radioModel->audioEngine();
    if (!eng) {
        return;
    }
    auto* dlg = new VaxLinuxFirstRunDialog(eng, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
#endif
}

void MainWindow::onConnectionStateChanged()
{
    // Phase 3Q-8: forward state to the spectrum widgets for the disconnect
    // overlay.
    //
    // EVERY pan, not just the active one. SpectrumWidget::mousePressEvent
    // opens with a disconnected guard that emits disconnectedClickRequest()
    // and returns, so a widget left at its Disconnected default swallows every
    // press: no click-to-tune, no filter-edge drag, no pan drag. Only
    // mouseMoveEvent is ungated, which is why a second pan still tracked the
    // cursor readout and looked alive while being completely unclickable --
    // and why tuning appeared to work only with the pointer over the flag,
    // which is a separate widget with its own handlers.
    pushConnectionStateToPans();

    // Task B4: +PAN dims (and its tooltip explains why) on every
    // connection-state transition, connected or not.
    updateAddPanButtonState();

    if (m_radioModel->isConnected()) {
        // ── Remote-daemon R2: isConnected() no longer implies a connection ──
        //
        // Before this branch, RadioModel::isConnected() was `m_connection &&
        // m_connection->isConnected()`, so the `if` above had ALSO tested
        // connection() for null and everything below could dereference it.
        // Task 3 made it storage-backed (`m_connectionState == Connected`,
        // RadioModel.cpp) so a client that owns no RadioConnection can report
        // Connected, and Task 18's applyStationCapabilities() sets exactly
        // that state whenever the station holds the radio -- which is the
        // premise of R2, not an edge case. Every dereference below was
        // therefore a null dereference on the FIRST state change a remote
        // client sees, taking the whole slot with it.
        //
        // Guarded on connection() rather than on role(): a null pointer is
        // the actual precondition, and testing it restores the invariant the
        // `if` above used to carry. The `else` branch of this same function
        // has always tested it this way.
        RadioConnection* const conn = m_radioModel->connection();

        // Board widget top line: show model code ("Saturn") not marketing name
        // ("ANAN-G2 (Saturn)") — the marketing name truncates at status-bar widths.
        // boardCodeName() returns the HPSDRHW enum label which is short and unambiguous.
        {
            // With no connection of our own, the station's radioModelName is
            // the identity we have. Same source the line below already uses
            // for firmware: model state populated by applyStationCapabilities,
            // not the connection.
            const QString code = conn != nullptr
                ? QString::fromLatin1(boardCodeName(conn->radioInfo().boardType))
                : m_radioModel->model();
            m_stationBlock->setHardwareLine(
                code, QStringLiteral("v%1").arg(m_radioModel->version()));
            // The second row's text (and hence StationBlock's sizeHint)
            // just changed (Task A8 fix round 1 finding 4).
            if (m_chromeBar && m_chromeBarWidget) {
                m_chromeBar->setNaturalWidth(
                    m_stationBlock, m_stationBlock->sizeHint().width());
                m_chromeBar->relayout(m_chromeBarWidget->width());
            }
        }

        // Phase 3Q-6/D.1: setRadio() removed — radio identity moves to the
        // STATION block (sub-PR-7). Segment state is already driven by
        // connectionStateChanged → ConnectionSegment::setState (see D.2 wiring
        // block in the constructor).

        // RxDashboard follows the ACTIVE slice (Task A5's rebindDashboard
        // lambda, wired to RadioModel::sliceAdded / activeSliceChanged
        // below), not a fixed slice(0), so no per-connect rebind is needed
        // here specifically -- the rebind already happens on sliceAdded.
        // (Connection details moved to segment tooltip / NetworkDiagnosticsDialog.)

        // Everything from here to the closing brace reads the LOCAL radio
        // connection: the step attenuator drives it directly, and `caps`
        // comes from the board byte the connection reported (which is NOT
        // the same as m_radioModel->boardCapabilities() -- a saved model
        // override moves the latter and not the former, so this stays on
        // the connection for local direct mode).
        //
        // A remote client has no local step attenuator to wire and no
        // local PureSignal coordinator to arm; both live on the station.
        // Skipping the block is the correct behaviour there, not merely
        // the safe one.
        if (conn != nullptr) {
            // Wire step attenuator controller to the live radio connection
            // and set max attenuation from board capabilities.
            // From Thetis console.cs ucInfoBar Warning() + SetupForm attenuator init.
            m_stepAttController->setRadioConnection(conn);
            const auto& caps = BoardCapsTable::forBoard(
                conn->radioInfo().boardType);
            // Level Cal fix wave: the same ceiling the Core gives the board
            // (DaemonApp::applyStepAttenuatorConnection and the RX applet,
            // BoardCapsTable::stepAttMaxDb): 61 dB on the Alex boards, the
            // board row's own maximum otherwise.
            {
                const auto& modelCaps = m_radioModel->boardCapabilities();
                m_stepAttController->setMaxAttenuation(
                    BoardCapsTable::stepAttMaxDb(modelCaps.board, modelCaps.hasAlexFilters));
            }
            // Wire HPSDR-board flag — Atlas/Metis kit uses preamp save/restore on
            // MOX rather than per-band TX ATT (Thetis console.cs:29548 [v2.10.3.13]:
            //   if (HardwareSpecific.Model == HPSDRModel.HPSDR) { ... }).
            m_stepAttController->setIsHpsdrBoard(
                conn->radioInfo().boardType == HPSDRHW::Atlas);

            // Phase 3M-4 Task 13: gate PureSignalApplet + TxApplet [PS-A] on
            // the same board capability.  PureSignalApplet hides itself; the
            // TxApplet [PS-A] button hides via its setBoardCapabilities slot.
            if (m_pureSignalApplet) {
                m_pureSignalApplet->setVisible(caps.hasPureSignal);
            }
            if (m_txApplet) {
                m_txApplet->setBoardCapabilities(caps);
            }
            // P1 full-parity §4.1: gate AutoAttMode::Adaptive on per-step
            // calibration support.  Must be set BEFORE loadSettings() so a
            // persisted "Adaptive" string is clamped to Classic when the
            // connected board lacks the feature.
            m_stepAttController->setHasStepAttenuatorCal(caps.hasStepAttenuatorCal);
            // Before loadSettings: the stored preamp modes move to the ten
            // Thetis modes by the label this board shows (the RX applet's
            // preamp combo uses the same board and Alex flag).
            m_stepAttController->setBoardIdentity(
                conn->radioInfo().boardType,
                m_radioModel->hardwareProfile().model,
                caps.hasAlexFilters);
            // R-R3-46: select slice A's band first, because
            // loadSettings restores the per-band slot for the current band
            // (DaemonApp::applyStepAttenuatorConnection does the same).
            m_radioModel->syncStepAttenuatorToReceiveSlice();
            m_stepAttController->loadSettings(conn->radioInfo().macAddress);
        } else if (m_pureSignalApplet) {
            applyRemotePureSignalAppletGate();
        }

        // Phase 3M-4 Task 10 + bench-fix: PSA bottom-banner indicator is
        // gated on caps.hasPureSignal AND pureSignal->isAutoCalEnabled().
        // Boards without PS support (HL2 / Atlas) hide the FB+PS pair
        // entirely; PS-capable boards (Hermes II / Angelia / Orion /
        // Saturn / G2) show it only when the user has armed PS-A.
        // updatePsaIndicatorVisibility centralises the condition.
        //
        // Hoisted out of the connection-guarded block above: it reads
        // m_radioModel->boardCapabilities() and pureSignal(), never the
        // connection, and a remote client must still get the hide (its
        // pureSignal() is null, so the condition is false).
        //
        // The hoist moved it from before that block to after it. That is
        // observably a no-op in local direct mode: nothing the block does
        // -- step-attenuator wiring, PureSignalApplet visibility,
        // TxApplet board capabilities -- is read by
        // updatePsaIndicatorVisibility, and nothing it does is read by the
        // block. Checked both directions rather than assumed.
        updatePsaIndicatorVisibility();

        // Phase 3Q Task 5 — auto-close: 1 s after connect, accept() the panel if open.
        // Fires on transitions TO Connected only (not on repeated Connected emits).
        if (m_connectionPanel && m_connectionPanel->isVisible()) {
            QTimer::singleShot(1000, this, [this]() {
                if (m_connectionPanel && m_connectionPanel->isVisible()
                    && m_radioModel->isConnected()) {
                    m_connectionPanel->accept();
                }
            });
        }

        // Per-radio peripherals refactor (2026-05-26): the PGXL / TGXL
        // auto-connect-on-Connected block previously lived here in
        // MainWindow but read GLOBAL AppSettings keys.  The lifecycle
        // (gated on the per-MAC FourO3A flag) now lives in
        // RadioModel::applyPeripheralsForCurrentMac(), which is driven
        // from onConnectionStateChanged so MainWindow doesn't need to
        // touch the peripheral wires here.

        // Phase 3P-II Task 20: wire AmpApplet controls to PgxlConnection.
        // operateToggled: translate bool to "operate"/"standby" command string.
        // statusUpdated: fan the k=v map into AmpApplet setter slots.
        // These connects are made on every radio-connect. Qt lambda
        // connects do not support UniqueConnection, so guard with a flag
        // to avoid stacking connections across reconnects. The flag is
        // instance-local; resetFlag is intentional (first time = wire).
        if (m_ampApplet && !m_ampAppletWired) {
            m_ampAppletWired = true;

            connect(m_ampApplet, &AmpApplet::operateToggled,
                    this, [this](bool wantOperate) {
                // R-R3-49 (parity Task 9): a remote window's applet asks the
                // Core itself (setPgxlOperate); this computer's Power Genius
                // connection is local only.
                // Group B fix wave (M5): refused on the air, as the applet
                // and a remote window are. Task 77 fix round 3: and while a
                // Tuner Genius cycle runs (pgxlSwitchRefusal).
                if (m_radioModel->role() == RadioModel::Role::Remote
                    || m_radioModel->pgxlSwitchRefusal(nullptr, /*standbyRequested=*/!wantOperate)) {
                    return;
                }
                // Bench-fix 2026-05-19: pcap stream 11 (.19 PowerGeniusDesktop
                // -> .235 PGXL :9008) shows the actually-used wire command
                // for OPERATE is `operate=1` (key=value), not bare `operate`.
                // PGXL rejected `operate` / `standby` with error 50000016
                // every click.
                QString reason;
                m_radioModel->setPgxlOperateForStation(wantOperate, &reason);
            });

            // R-R3-47 / R-R3-22: the gauges no longer come from here. The
            // Power Genius conversion (dBm peak forward power to W, signed
            // return loss to an SWR ratio, the transmit-only gate on those
            // two latched values, temperature, drain current, mains volts
            // and the efficiency label) moved to the Core side as
            // applyPgxlStatus() (src/core/PgxlStatusGauges.cpp), which feeds
            // RadioModel's AmplifierModel; AmpApplet reads that model, here
            // in-process and in a remote window as the Core's `amplifier`
            // object.

            // Phase 3P-II Phase 4 Task 88: track PGXL connected state for the
            // context menu Disconnect/Reconnect label.
            connect(m_radioModel->pgxlConnection(), &PgxlConnection::connected,
                    this, [this]() { m_ampApplet->setPgxlConnected(true); });
            connect(m_radioModel->pgxlConnection(), &PgxlConnection::disconnected,
                    this, [this]() { m_ampApplet->setPgxlConnected(false); });

            // Phase 3P-II Phase 4 Task 88: context menu right-click signals.

            // connectionToggleRequested: disconnect or reconnect PGXL.
            connect(m_ampApplet, &AmpApplet::connectionToggleRequested,
                    this, [this]() {
                // R-R3-22 / R-R3-47: a remote window's applet asks the Core
                // itself (disconnectPgxl, configurePgxl); this computer's
                // Power Genius connection is local only.
                if (m_radioModel->role() == RadioModel::Role::Remote) {
                    return;
                }
                PgxlConnection* pgxl = m_radioModel->pgxlConnection();
                if (!pgxl) { return; }
                if (pgxl->isConnected()) {
                    pgxl->disconnect();
                } else {
                    // PR #279 review #5 (2026-05-23): use the same
                    // PGXL_ManualIp / PGXL_ManualPort keys that
                    // auto-connect at line ~6331 + the Setup ->
                    // CAT & Network -> PGXL page persist.  The
                    // earlier reads of obsolete PGXL_IpAddress /
                    // PGXL_Port (default 50001) returned empty
                    // strings on every install that had only ever
                    // written the canonical keys, so the AmpApplet
                    // context-menu Connect did nothing.
                    //
                    // Per-radio peripherals refactor (2026-05-26):
                    // the keys are scoped under
                    // hardware/<mac>/peripherals/.  Default 9008
                    // matches the AppSettings.h documented default.
                    const QString ip = m_radioModel->peripheralValue(
                        QStringLiteral("PGXL_ManualIp"));
                    const quint16 port = static_cast<quint16>(
                        m_radioModel->peripheralValue(
                            QStringLiteral("PGXL_ManualPort"),
                            QStringLiteral("9008")).toUInt());
                    if (!ip.isEmpty()) {
                        pgxl->connectToPgxl(ip, port);
                    }
                }
            });

            // diagnosticsCopyRequested: build a brief diagnostic string and copy to clipboard.
            connect(m_ampApplet, &AmpApplet::diagnosticsCopyRequested,
                    this, [this]() {
                // R-R3-49 (parity Task 9): a remote window copies the Core's
                // connection (the mirrored `amplifier` object and
                // accessoryData's pgxl counters), not this computer's idle
                // socket.
                if (m_radioModel->role() == RadioModel::Role::Remote) {
                    QGuiApplication::clipboard()->setText(
                        AmpApplet::coreDiagnosticsText(m_radioModel));
                    return;
                }
                PgxlConnection* pgxl = m_radioModel->pgxlConnection();
                const QString text = QStringLiteral(
                    "PGXL Diagnostics\n"
                    "Connected: %1\n"
                    "IP: %2\n"
                ).arg(pgxl && pgxl->isConnected() ? QStringLiteral("Yes") : QStringLiteral("No"))
                 .arg(pgxl ? pgxl->peerAddress() : QStringLiteral("--"));
                QGuiApplication::clipboard()->setText(text);
            });

            // Phase 3P-II Phase 4 Task 90: wire navigationRequested to openSetup().
            connect(m_ampApplet, &AmpApplet::navigationRequested,
                    this, &MainWindow::openSetup);
        }

        // Phase 3P-II Phase 4 Task 89: wire TunerApplet context menu signals to
        // TgxlConnection. buildUI() runs once at startup, so no deduplication
        // guard is needed. Qt::UniqueConnection is intentionally NOT used here:
        // Qt6 silently no-ops UniqueConnection when the slot is a lambda (it
        // requires a pointer-to-member-function of a QObject subclass), so
        // all four connects below would have been dead on arrival.
        if (m_tunerApplet) {
            // Track TGXL connected state for Disconnect/Reconnect label.
            connect(m_radioModel->tgxlConnection(), &TgxlConnection::connected,
                    this, [this]() { m_tunerApplet->setTgxlConnected(true); });
            connect(m_radioModel->tgxlConnection(), &TgxlConnection::disconnected,
                    this, [this]() { m_tunerApplet->setTgxlConnected(false); });

            // connectionToggleRequested: disconnect or reconnect TGXL.
            connect(m_tunerApplet, &TunerApplet::connectionToggleRequested,
                    this, [this]() {
                TgxlConnection* tgxl = m_radioModel->tgxlConnection();
                if (!tgxl) { return; }
                if (tgxl->isConnected()) {
                    tgxl->disconnect();
                } else {
                    // Per-radio peripherals refactor (2026-05-26): keys
                    // scoped under hardware/<mac>/peripherals/.
                    const QString ip = m_radioModel->peripheralValue(
                        QStringLiteral("TGXL_ManualIp"));
                    const quint16 port = static_cast<quint16>(
                        m_radioModel->peripheralValue(
                            QStringLiteral("TGXL_ManualPort"),
                            QStringLiteral("9010")).toUInt());
                    if (!ip.isEmpty()) {
                        tgxl->connectToTgxl(ip, port);
                    }
                }
            });

            // diagnosticsCopyRequested: build diagnostic string and copy to clipboard.
            connect(m_tunerApplet, &TunerApplet::diagnosticsCopyRequested,
                    this, [this]() {
                // R-R3-49 (parity Task 8): a remote window copies the Core's
                // connection (the mirrored `tuner` object and accessoryData's
                // tgxl counters), not this computer's idle socket.
                if (m_radioModel->role() == RadioModel::Role::Remote) {
                    QGuiApplication::clipboard()->setText(
                        TunerApplet::coreDiagnosticsText(m_radioModel));
                    return;
                }
                TgxlConnection* tgxl = m_radioModel->tgxlConnection();
                const QString text = QStringLiteral(
                    "TGXL Diagnostics\n"
                    "Connected: %1\n"
                    "IP: %2\n"
                ).arg(tgxl && tgxl->isConnected() ? QStringLiteral("Yes") : QStringLiteral("No"))
                 .arg(tgxl ? tgxl->peerAddress() : QStringLiteral("--"));
                QGuiApplication::clipboard()->setText(text);
            });

            // Phase 3P-II Phase 4 Task 90: wire navigationRequested to openSetup().
            connect(m_tunerApplet, &TunerApplet::navigationRequested,
                    this, &MainWindow::openSetup,
                    Qt::UniqueConnection);
        }
    } else {
        // No explicit hardware-line reset needed here: StationBlock clears
        // its own second row automatically whenever setRadioName(QString())
        // runs (Task A4), which the connectionStateChanged handler wired in
        // buildStatusBar() already does on every non-Connected transition.

        // Save step attenuator settings before disconnecting.
        if (m_radioModel->connection()) {
            m_stepAttController->saveSettings(m_radioModel->connection()->radioInfo().macAddress);
        }

        // Disconnect step attenuator from radio
        m_stepAttController->setRadioConnection(nullptr);

        // Phase 3M-4 Task 10 + bench-fix: hide the PSA indicator on
        // disconnect.  Re-evaluated via updatePsaIndicatorVisibility on
        // next reconnect (which now also checks PureSignal::isAutoCalEnabled).
        updatePsaIndicatorVisibility();
        // Phase 3M-4 Task 13: hide PureSignalApplet on disconnect and push
        // the unknown board to TxApplet [PS-A].  Same lifetime model as the
        // PSA indicator above.  Re-evaluation happens on next reconnect via
        // the connected-branch gating block.
        if (m_pureSignalApplet) {
            m_pureSignalApplet->setVisible(false);
        }
        if (m_txApplet) {
            // Push the unknown-board defaults (hasPureSignal == false).
            // Fix round 1 (minor 5): [PS-A] then shows disabled with its
            // reason until the board is known; only a known board without
            // PureSignal hides it.  RadioModel::boardCapabilities() returns the
            // unknown-board fallback when m_hardwareProfile.caps is null
            // (RadioModel.cpp:1016 [v2.10.3.13] equivalent).
            m_txApplet->setBoardCapabilities(m_radioModel->boardCapabilities());
        }

        // Phase 3Q Sub-PR-6 (F.1): RxDashboard shows placeholder "—" when
        // disconnected automatically (slice values reset to defaults). No
        // per-disconnect update needed here.
        // (The "last connected" breadcrumb moved to the segment tooltip in D.2.)

        // Phase 3Q Task 5 — auto-open: on disconnect (after having been connected),
        // open the ConnectionPanel so the user can reconnect.
        // Guard: m_autoReconnectInProgress suppresses the panel during background
        // auto-reconnect (Task 17); m_shuttingDown suppresses it during ⌘Q so
        // ConnectionPanel's ctor doesn't restart discovery mid-close (would
        // beach-ball the close path for the full SafeDefault scan window — see
        // [shutdown-trace] log analysis 2026-05-02). The very first state read
        // at startup is Disconnected which should not open the panel either —
        // the radio-name check below handles that case.
        // The panel itself is non-modal (show/raise), matching the current pattern.
        // R-R3-16: local models only. A remote window opens Connections on
        // the operator's own Disconnect (RemoteConnectionController::
        // operatorDisconnected); a Disconnected state it reaches through
        // link loss or an offline radio at the Core opens nothing.
        if (!m_autoReconnectInProgress && !m_shuttingDown
            && m_radioModel->ownsLocalDsp()) {
            // Only open if we were previously connected (transition from Connected,
            // not the initial Disconnected state at startup). We detect this by
            // checking if the model has ever reported a radio name — set on connect.
            if (!m_radioModel->name().isEmpty()) {
                showConnectionPanel();
            }
        }
    }

    // 3Q-9 (post-feedback simplification): Connect is "reconnect to last".
    // Greyed out when (a) we're already connected, OR (b) there's no
    // last-used radio in saved entries to reconnect to. Manage Radios is
    // the only way to pick a different radio.
    //
    // Use the model's authoritative connectionState (3Q-1) rather than
    // RadioModel::isConnected() — the latter dereferences m_connection
    // which can briefly disagree during teardown (m_connectionState
    // already Disconnected but m_connection->isConnected() still true
    // until the worker-thread teardown finishes). Without this, a
    // Radio→Disconnect would leave Connect greyed forever.
    if (m_actConnect && m_actDisconnect && m_actProtocolInfo) {
        const bool connected =
            (m_radioModel->connectionState() == ConnectionState::Connected);
        AppSettings& s = AppSettings::instance();
        const QString lastMac = s.lastConnected();
        const bool hasReconnectTarget =
            !connected
            && !lastMac.isEmpty()
            && s.savedRadio(lastMac).has_value();
        m_actConnect->setEnabled(m_connectionPickerManaged || hasReconnectTarget);
        // TX safety fix round 3 (2026-09-30): Disconnect also stays
        // available while the lost-link lock holds, Disconnected included
        // (an automatic recovery that stopped), since it is what lifts it.
        m_actDisconnect->setEnabled(localDisconnectAvailable());
        m_actProtocolInfo->setEnabled(connected);
    }

    // Remote actions follow Core session activity; protocol/discovery remain
    // local-only even when the mirrored radio reports Connected.
    applyRemoteRoleGating();
    refreshRemoteConnectionUi();
    // R-R3-21: the container transmit buttons follow the connection state
    // set just above.
    refreshContainerControls();
}

bool MainWindow::localDisconnectAvailable() const
{
    return m_radioModel->connectionState() == ConnectionState::Connected
        || m_radioModel->isRadioLinkDown();
}

// Phase 3I Task 17 / Phase 3Q Task 10 — auto-reconnect on launch.
//
// Logic:
//   1. Collect ALL saved radios with autoConnect = true. If none, open the
//      ConnectionPanel (Phase 3Q polish — design §6.1 cold-launch flow) so
//      the user has a one-click path to a saved radio or to Add Manually.
//   2. Pick the target MAC:
//      - Single autoConnect entry → use it directly.
//      - Multiple entries → most-recently-connected MAC wins (radios/lastConnected);
//        a one-time status-bar warning is posted via RadioModel::autoConnectAmbiguous.
//   3a. pinToMac=true  → run a Fast-profile discovery, connect when the same MAC
//      is seen. A 3-second kill timer fires if the MAC never appears; on timeout
//      the ConnectionPanel opens with the target row highlighted and a status-bar
//      message explains why.  RadioModel's m_autoConnectInProgress flag ensures
//      the RadioConnection::connectFailed signal (if the radio replies but fails)
//      also surfaces via RadioModel::autoConnectFailed → MainWindow lambdas.
//   3b. pinToMac=false → direct connect to saved IP. Arm m_autoConnectInProgress
//      so RadioConnection::connectFailed is forwarded as autoConnectFailed.
//
// m_autoReconnectInProgress gates the disconnect auto-open in onConnectionStateChanged
// so the background probe does not flash the ConnectionPanel while in flight.
void MainWindow::tryAutoReconnect()
{
    if (m_shuttingDown || !m_radioModel->ownsLocalDsp()) { return; }
    AppSettings& s = AppSettings::instance();

    // --- Step 1: Collect all autoConnect-flagged saved radios ---
    const QList<SavedRadio> allSaved = s.savedRadios();
    QStringList autoMacs;
    for (const SavedRadio& sr : allSaved) {
        if (sr.autoConnect) {
            autoMacs << sr.info.macAddress;
        }
    }
    if (autoMacs.isEmpty()) {
        // Phase 3Q polish: cold-launch panel auto-open. Design §6.1 — when
        // there's no auto-connect target, surface the radio list so the
        // user has a one-click path to either connect (saved radio shown)
        // or add one (empty list). Non-modal so the app remains usable
        // around the panel. Skipped when an auto-connect attempt is in
        // flight (the auto-reconnect path handles its own panel open via
        // the failure handler in Task 10).
        showConnectionPanel();
        return;
    }

    // --- Step 2: Pick the target MAC (most-recently-connected wins) ---
    const QString lastMac = s.lastConnected();
    QString chosenMac = autoMacs.first();
    if (autoMacs.size() > 1) {
        if (autoMacs.contains(lastMac)) {
            chosenMac = lastMac;
        }
        // Warn once — notifyAutoConnectAmbiguous emits the signal; the
        // MainWindow lambda wired in buildUI surfaces it as a status-bar message.
        m_radioModel->notifyAutoConnectAmbiguous(autoMacs.size(), chosenMac);
    } else if (chosenMac != lastMac && !lastMac.isEmpty()) {
        // Single autoConnect entry but it's not the most recently connected.
        // Still proceed — the user may have switched their autoConnect flag.
    }

    const auto saved = s.savedRadio(chosenMac);
    if (!saved.has_value()) {
        return;  // Shouldn't happen — entry disappeared between the two reads.
    }

    qCInfo(lcConnection) << "Auto-reconnect: attempting" << chosenMac
                         << "at" << saved->info.address.toString()
                         << "(pinToMac=" << saved->pinToMac << ")";

    if (saved->pinToMac) {
        // Use Fast profile — ~480ms per NIC, shorter than the SafeDefault
        // that the user's manual Start Discovery uses.
        RadioDiscovery* disc = m_radioModel->discovery();
        disc->setProfile(DiscoveryProfile::Fast);
        m_autoReconnectInProgress = true;

        // Arm the RadioModel flag so RadioConnection::connectFailed (which fires
        // if the radio replies but fails the handshake) is forwarded as
        // RadioModel::autoConnectFailed. This covers the pinToMac discovery-found
        // but connect-failed path; the timeout path below handles unreachable.
        m_radioModel->setAutoConnectInProgress(true, chosenMac);

        // Listen for a radio that matches our chosen MAC
        // Both the discovery callback and its timeout own this handle. A
        // station switch may destroy the window before either fires.
        auto connPtr = std::make_shared<QMetaObject::Connection>();
        *connPtr = connect(disc, &RadioDiscovery::radioDiscovered,
            this, [this, chosenMac, connPtr](const RadioInfo& found) {
            if (m_shuttingDown) { return; }
            if (found.macAddress != chosenMac) {
                return;
            }
            if (m_radioModel->isConnected()) {
                return; // User beat us to it
            }
            qCInfo(lcConnection) << "Auto-reconnect: MAC found —"
                                 << found.displayName()
                                 << found.address.toString();
            QObject::disconnect(*connPtr);
            m_autoReconnectInProgress = false;
            // Note: do NOT disarm m_radioModel->setAutoConnectInProgress here —
            // connectToRadio runs asynchronously and we want connectFailed to
            // still be forwarded if the handshake itself fails. RadioModel's
            // onConnectionStateChanged(Connected) disarms on success;
            // wireConnectionSignals' connectFailed handler disarms on failure.
            RadioInfo ri = found;
            HPSDRModel mo = AppSettings::instance().modelOverride(ri.macAddress);
            if (mo != HPSDRModel::FIRST) {
                ri.modelOverride = mo;
            }
            m_radioModel->connectToRadio(ri);
        });

        // Kick off the Fast-profile discovery
        disc->startDiscovery();

        // 3-second hard timeout — if the MAC never appears, the radio is
        // unreachable on this network. Open the panel + post a status message.
        QTimer::singleShot(3000, this, [this, chosenMac, connPtr]() {
            if (!m_autoReconnectInProgress) {
                // Already connected (discovery lambda cleaned up) — nothing to do.
                return;
            }
            qCInfo(lcConnection) << "Auto-reconnect: 3-second timeout — radio not found";
            // Disconnect the listener so it doesn't fire on later scans
            QObject::disconnect(*connPtr);
            m_autoReconnectInProgress = false;
            // Disarm RadioModel flag — the Timeout path is surfaced here directly
            // (not via connectFailed, which only fires after a reply is received).
            m_radioModel->setAutoConnectInProgress(false);
            // Stop the discovery pass we started; restore SafeDefault profile
            // so the user's next manual scan uses the full timing.
            m_radioModel->discovery()->stopDiscovery();
            m_radioModel->discovery()->setProfile(DiscoveryProfile::SafeDefault);
            // Phase 3Q Task 10: surface the failure — open panel + toast.
            const QString name = AppSettings::instance()
                .savedRadio(chosenMac)
                .value_or(SavedRadio{})
                .info.name;
            const QString displayName = name.isEmpty() ? chosenMac : name;
            showConnectionPanel();
            if (m_connectionPanel) {
                m_connectionPanel->highlightMac(chosenMac);
            }
            showToast(
                QStringLiteral("Auto-connect target %1 isn't reachable from this network. "
                               "Pick a different radio or update the address.")
                    .arg(displayName),
                ToastSeverity::Warning, 8000);
        });
    } else {
        // No MAC pinning — direct connect to saved IP address.
        // Arm m_autoConnectInProgress so that RadioConnection::connectFailed
        // is forwarded as RadioModel::autoConnectFailed → MainWindow lambdas.
        if (!m_radioModel->isConnected()) {
            m_radioModel->setAutoConnectInProgress(true, chosenMac);
            // Load persisted model override for auto-reconnect (Phase 3I-RP)
            RadioInfo ri = saved->info;
            HPSDRModel mo = AppSettings::instance().modelOverride(ri.macAddress);
            if (mo != HPSDRModel::FIRST) {
                ri.modelOverride = mo;
            }
            m_radioModel->connectToRadio(ri);
        }
    }
}

// =============================================================================
// Phase 3O Sub-Phase 11 Task 11b — VAX first-run / rescan hook
// =============================================================================
//
// Called once from the constructor via QTimer::singleShot(0, ...). Decides
// whether to show the VaxFirstRunDialog based on audio/FirstRunComplete
// and a SHA-256 diff of the current detected-cable set against the stored
// audio/LastDetectedCables fingerprint. The fingerprint is refreshed on
// every launch regardless of whether the dialog shows, so uninstall +
// reinstall of the same cable doesn't flag it as "new" forever.
//
// NereusSDR-original; no Thetis equivalent.
void MainWindow::checkVaxFirstRun()
{
    // R-R3-44 (parity Task 11): runs in a remote window as in a local one.
    // The VAX outputs are this computer's in both (a remote window feeds
    // them from the Core's receiver streams), and audio/FirstRunComplete,
    // the cable fingerprint and audio/Vax<ch>/DeviceName are this
    // computer's settings, never the Core's, so a later local window reads
    // what the operator answered here.
    auto& s = AppSettings::instance();
    const bool firstRunDone =
        (s.value(QStringLiteral("audio/FirstRunComplete"),
                 QStringLiteral("False")).toString() == QStringLiteral("True"));

    // Platform-specific scan — see detectedForFirstRun() in the anonymous
    // namespace at the top of this file for the platform split rationale.
    const QVector<DetectedCable> detected = detectedForFirstRun();

    // Always refresh the stored fingerprint so a cable being removed +
    // later reinstalled doesn't permanently re-flag itself as "new".
    const QString newCsv = VirtualCableDetector::fingerprintCsv(detected);
    const QString lastCsv = s.value(QStringLiteral("audio/LastDetectedCables"),
                                    QString()).toString();
    s.setValue(QStringLiteral("audio/LastDetectedCables"), newCsv);
    s.save();

    FirstRunScenario scenario;
    QVector<DetectedCable> payload;

    if (!firstRunDone) {
#if defined(Q_OS_WIN)
        scenario = detected.isEmpty() ? FirstRunScenario::WindowsNoCables
                                       : FirstRunScenario::WindowsCablesFound;
#elif defined(Q_OS_MAC)
        scenario = FirstRunScenario::MacNative;
#else
        scenario = FirstRunScenario::LinuxNative;
#endif
        payload = detected;
    } else {
        // First-run already complete — only pop the dialog if NEW cables
        // have appeared since the last launch.
        const auto fresh = VirtualCableDetector::diffNewCables(detected, lastCsv);
        if (fresh.isEmpty()) {
            return;
        }
        scenario = FirstRunScenario::RescanNewCables;
        payload = fresh;
    }

    auto* dlg = new VaxFirstRunDialog(scenario, payload, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    // "Apply suggested" / "Apply to VAX 3 & 4" — user accepted the
    // recommended bindings. Log-but-ignore any AudioEngine wiring failure;
    // the design interview explicitly settled that we still mark the
    // first-run complete so the user isn't re-ambushed on next launch.
    connect(dlg, &VaxFirstRunDialog::applySuggested, this,
            [this](const QVector<QPair<int, QString>>& bindings) {
        // This computer's VAX outputs, live in a remote window too
        // (R-R3-44), so not the audited local-DSP accessor.
        auto* engine = m_radioModel->localAudioDevices();
        if (!engine) {
            qCWarning(lcAudio)
                << "VAX first-run: applySuggested with no AudioEngine; "
                   "bindings dropped" << bindings.size();
            return;
        }

        // Remap dialog-suggested bindings onto the first VAX slots
        // whose audio/Vax<ch>/DeviceName is unset. VaxFirstRunDialog::
        // computeSuggestedBindings always numbers its payload starting
        // at VAX 1 regardless of scenario, with an explicit comment
        // that MainWindow is responsible for skipping slots the user
        // has already assigned. Applying the dialog's channel numbers
        // verbatim — the previous revision — clobbered existing slot-1
        // ..N mappings under FirstRunScenario::RescanNewCables. We
        // apply the same rule unconditionally since it is a no-op for
        // WindowsCablesFound (all four DeviceName keys are empty on a
        // fresh install, so remap resolves to the same 1..N order).
        //
        auto& settings = AppSettings::instance();
        int slot = 1;
        for (const auto& b : bindings) {
            while (slot <= 4) {
                const QString key = QStringLiteral("audio/Vax%1/DeviceName")
                                        .arg(slot);
                if (settings.value(key, QString()).toString().isEmpty()) {
                    break;
                }
                ++slot;
            }
            if (slot > 4) {
                qCWarning(lcAudio)
                    << "VAX first-run: no unassigned slots remain;"
                    << "dropping cable" << b.second;
                break;
            }
            AudioDeviceConfig cfg;
            cfg.deviceName = b.second;
            engine->setVaxConfig(slot, cfg);
            engine->setVaxEnabled(slot, true);
            ++slot;
        }
    });

    // Sub-Phase 12: wire "Customize…" / "Why do I need this?" → Setup → VAX.
    // Opens (or raises) the Setup dialog and navigates to Audio → VAX.
    connect(dlg, &VaxFirstRunDialog::openSetupAudioPage, this,
            [this](const QString& pageLabel) {
        auto* dialog = createSetupDialog();
        if (dialog == nullptr) {
            return;  // the gate refused and has already said why
        }
        dialog->selectPage(pageLabel);
        dialog->show();
    });

    connect(dlg, &VaxFirstRunDialog::openInstallUrl, this,
            [](const QString& url) {
        QDesktopServices::openUrl(QUrl(url));
    });

    // Persist audio/FirstRunComplete on Accepted only (Apply / Skip /
    // Got-it). Rejected covers Escape, window-close, and Customize — none
    // of those should silence the dialog on next launch.
    connect(dlg, &QDialog::finished, this, [](int result) {
        if (result == QDialog::Accepted) {
            auto& settings = AppSettings::instance();
            settings.setValue(QStringLiteral("audio/FirstRunComplete"),
                              QStringLiteral("True"));
            settings.save();
        }
    });

    dlg->show();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Mark shutting-down BEFORE anything else so the "auto-open
    // ConnectionPanel on Disconnect" slot below doesn't re-trigger
    // discovery via ConnectionPanel's ctor while teardown runs.
    m_shuttingDown = true;
    if (m_desktopStationController) { m_desktopStationController->stop(); }
    m_autoReconnectInProgress = false;
    if (m_stationClient) {
        m_stationDisconnectRequested = true;
        m_stationClient->disconnectFromStation(QStringLiteral("client shutting down"));
    }

    // Capture GUI-local DSP dialog preferences while these windows still
    // exist and before the final AppSettings flush. QObject destruction is
    // later than that flush, so destructor-only geometry saves miss shutdown.
    for (auto* dialog : findChildren<AmpViewWindow*>()) {
        dialog->close();
    }
    for (auto* dialog : findChildren<DspAssetDialog*>()) {
        dialog->close();
    }
    if (m_psForm) {
        m_psForm->close();
    }

    // Force-run any pending coalesced slice save BEFORE we tear anything
    // down. The 500 ms debounce in RadioModel::scheduleSettingsSave can't
    // fire while this handler runs synchronously (event loop blocked on
    // QThread::wait below); without this flush the user's last AF / step /
    // freq / lock / RIT change is silently dropped on close.
    m_radioModel->flushPendingSettingsSave();

    // Stop discovery to prevent new signals during shutdown
    m_radioModel->discovery()->stopDiscovery();

    // Stop the FFT engine pool's worker thread(s) before anything else below
    // touches display state.  FftEnginePool's destructor quits and waits on
    // every worker thread before deleting the engines parked on it (see
    // FftEnginePool.cpp), so deleting it here -- at the same point in the
    // sequence the old m_fftThread->quit()/wait() ran -- gives the rest of
    // this teardown the same guarantee the old code had: no FFT engine is
    // still producing frames while saveSettings() / disconnect / container
    // teardown run below.  primaryFftEngine() safely returns nullptr once
    // this pointer is cleared, and every remaining call site already
    // null-guards it.
    m_radioModel->setFftEnginePool(nullptr);
    delete m_fftEnginePool;
    m_fftEnginePool = nullptr;

    // Save display settings before shutdown
    if (m_panStack) {
        for (PanadapterApplet* applet : m_panStack->allApplets()) {
            if (applet && applet->spectrumWidget()) {
                applet->spectrumWidget()->saveSettings();
            }
        }
    }

    // Tear down connection (sends stop command, closes sockets, joins thread)
    m_radioModel->disconnectFromRadio();

    // Save container layout
    if (m_containerManager) {
        m_containerManager->saveState();
    }

    // Phase 3F Sub-Epic D Task 15: persist pan layout id + per-splitter
    // sizes. PanadapterStack::saveSplitterState writes PanLayoutId +
    // PanLayoutSplitter_* keys to AppSettings; the matching restore runs
    // during MainWindow init.
    if (m_panStack) {
        m_panStack->saveSplitterState();
        // Any pan still popped out, while there is still a flush coming.
        // ~PanadapterStack saves too, but it runs after the
        // AppSettings::save() below and setValue only writes an in-memory map
        // whose owner has a defaulted destructor, so the teardown save alone
        // never reached the disk. Codex, PR #318.
        m_panStack->saveFloatingGeometry();
    }

    // Issue #206 — persist window geometry + maximized/fullscreen
    // state. Captured BEFORE close so the saved blob reflects the
    // user-visible state, not Qt's mid-teardown geometry.
    saveMainWindowGeometry();

    AppSettings::instance().save();
    event->accept();

    // Ask Qt for an orderly exit from the event loop. Previously called
    // std::exit(0) which runs C++ static destructors before Qt's thread
    // cleanup — that caused QThreadStoragePrivate::finish to fire a qWarning
    // against a destructed QRegularExpression in the PII-redaction message
    // handler, segfaulting every close (~100 diagnostic reports in one day).
    if (!m_retiringSession) { QCoreApplication::quit(); }
}

// =============================================================================
// Phase 3G-14: AI-Assisted Issue Reporter
// Ported from AetherSDR TitleBar::showFeatureRequestDialog() /
// showFeatureRequestDialogImpl()
// =============================================================================

void MainWindow::fetchLatestReleaseVersion(std::function<void(const QString&)> done)
{
    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl(QStringLiteral(
        "https://api.github.com/repos/boydsoftprez/NereusSDR/releases/latest")));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NereusSDR"));
    auto* reply = nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [reply, nam, done = std::move(done)] {
        reply->deleteLater();
        nam->deleteLater();
        QString latest;
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            latest = doc.object().value(QStringLiteral("tag_name")).toString();
            if (latest.startsWith(QLatin1Char('v'))) {
                latest = latest.mid(1);
            }
        }
        done(latest);
    });
}

void MainWindow::checkForUpdates()
{
    fetchLatestReleaseVersion([this](const QString& latest) {
        const QString current = QCoreApplication::applicationVersion();
        const QVersionNumber latestVer = QVersionNumber::fromString(latest);
        if (latestVer.isNull()) {
            showToast(tr("Could not check for updates. Check this computer's internet "
                         "connection and try again."), ToastSeverity::Warning, 6000);
            return;
        }
        if (QVersionNumber::fromString(current) < latestVer) {
            showToast(tr("NereusSDR %1 is available. This computer has %2.")
                          .arg(latest, current), ToastSeverity::Info, 8000);
            return;
        }
        showToast(tr("This computer has the newest NereusSDR, %1.").arg(current),
                  ToastSeverity::Info, 6000);
    });
}

void MainWindow::showFeatureRequestDialog()
{
    // Version check gate: warn if not on latest release before filing
    fetchLatestReleaseVersion([this](const QString& latest) {
        if (!latest.isEmpty()) {
            QVersionNumber latestVer = QVersionNumber::fromString(latest);
            QVersionNumber currentVer = QVersionNumber::fromString(
                QCoreApplication::applicationVersion());
            if (!latestVer.isNull() && currentVer < latestVer) {
                auto answer = QMessageBox::warning(this,
                    QStringLiteral("Outdated Version"),
                    QStringLiteral(
                        "<p>You are running <b>v%1</b> but <b>v%2</b> is available.</p>"
                        "<p>Your issue may already be fixed in the latest release. "
                        "Please update before filing a bug report.</p>"
                        "<p>Continue anyway?</p>")
                        .arg(QCoreApplication::applicationVersion(), latest),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                if (answer != QMessageBox::Yes) {
                    return;
                }
            }
        }
        // Proceed to show the issue dialog
        showFeatureRequestDialogImpl();
    });
}

void MainWindow::showFeatureRequestDialogImpl()
{
    static const QString kPrompt = QStringLiteral(
        "IMPORTANT: before doing anything else, fetch the complete list of open\n"
        "issues by reading pages sequentially until you get fewer than 100 results:\n"
        "  Page 1: https://github.com/boydsoftprez/NereusSDR/issues?state=open&per_page=100&page=1\n"
        "  Page 2: https://github.com/boydsoftprez/NereusSDR/issues?state=open&per_page=100&page=2\n"
        "  ... continue until a page returns fewer than 100 issues.\n"
        "Do NOT rely on cached or training data for the issue list.\n\n"
        "Also fetch CLAUDE.md fresh (do not use cached versions):\n"
        "  https://raw.githubusercontent.com/boydsoftprez/NereusSDR/main/CLAUDE.md\n\n"
        "I want to report an issue or request a feature for NereusSDR, a cross-platform\n"
        "Qt6/C++20 SDR console for OpenHPSDR radios (ANAN, Hermes Lite 2, etc.). It uses\n"
        "the OpenHPSDR Protocol 1 and Protocol 2 over UDP, with client-side DSP via WDSP.\n\n"
        "DUPLICATE CHECK: this is mandatory. Search the fetched issue list for keywords\n"
        "related to my description below. Check titles AND bodies. If you find an existing\n"
        "issue that covers the same thing, STOP and tell me:\n"
        "  > Duplicate found: #<number>: <title>\n"
        "  > I recommend adding a +1 reaction and a comment describing your use case.\n"
        "Do NOT write a new issue if a duplicate exists.\n\n"
        "If no duplicate exists, determine whether my description is a BUG REPORT or a\n"
        "FEATURE REQUEST, then write a GitHub issue using the appropriate format below.\n"
        "Use GitHub-flavored Markdown formatting (headers, code blocks, bullet points).\n\n"
        "FOR FEATURE REQUESTS include:\n"
        "1. A clear, concise title (imperative mood)\n"
        "2. ## What: what the feature does from the user's perspective\n"
        "3. ## Why: what problem it solves\n"
        "4. ## How Other Clients Do It: how Thetis, PowerSDR, SparkSDR, etc. handle this\n"
        "5. ## Suggested Behavior: specific UX, what the user clicks, sees, what happens.\n"
        "   Reference NereusSDR UI elements (AppletPanel, VfoWidget, RxApplet, SetupDialog, etc.)\n"
        "6. ## Protocol Hints: relevant OpenHPSDR commands, or \"Unknown, needs research\"\n"
        "7. ## Acceptance Criteria: 3-5 bullet points defining done vs not-done\n\n"
        "FOR BUG REPORTS include:\n"
        "1. A clear title describing the broken behavior\n"
        "2. ## What happened: describe the incorrect behavior\n"
        "3. ## What I expected: describe the correct behavior\n"
        "4. ## Steps to reproduce: numbered steps to trigger the bug\n"
        "5. ## Environment: OS, radio model, protocol version, firmware version if relevant\n"
        "6. ## Suggested fix: if you have an idea what's wrong, describe it\n\n"
        "Suggest appropriate labels from: enhancement, bug, documentation,\n"
        "help wanted, good first issue, question\n\n"
        "Here is my idea or bug report:\n\n"
        "[Describe your feature or bug here in plain English]");

    // Reuse existing dialog if still open
    static QPointer<QDialog> sDlg;
    if (sDlg) {
        sDlg->raise();
        sDlg->activateWindow();
        return;
    }

    auto* dlg = new QDialog(this);
    sDlg = dlg;
    dlg->setWindowTitle(QStringLiteral("AI-Assisted Issue Reporter"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setStyleSheet(QStringLiteral("QDialog { background: #0f0f1a; }"));
    dlg->setMinimumWidth(620);

    auto* vbox = new QVBoxLayout(dlg);
    vbox->setSpacing(8);
    vbox->setContentsMargins(16, 16, 16, 16);

    auto* header = new QLabel(QStringLiteral(
        "<h3 style='color:#c8d8e8;'>AI-Assisted Issue Reporter</h3>"
        "<p style='color:#8090a0;'>Use any AI assistant to write a detailed bug report or feature request.</p>"
        "<ol style='color:#c8d8e8;'>"
        "<li><b>Choose your AI</b> below; the prompt is copied to your clipboard</li>"
        "<li><b>Paste the prompt</b> into the AI chat</li>"
        "<li><b>Describe your idea</b>: edit the [bracketed] section</li>"
        "<li><b>Copy the AI's output</b> and click <b>Submit Your Idea</b></li>"
        "</ol>"));
    header->setWordWrap(true);
    vbox->addWidget(header);

    // Status label — shows after provider selected
    auto* statusLabel = new QLabel;
    statusLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #20c060; font-size: 11px; font-weight: bold; }"));
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->hide();
    vbox->addWidget(statusLabel);

    // AI provider buttons
    const QString btnStyle = QStringLiteral(
        "QPushButton { background: #1a2a3a; border: 1px solid #304050; "
        "border-radius: 3px; color: #c8d8e8; font-size: 12px; font-weight: bold; "
        "padding: 6px 12px; }"
        "QPushButton:hover { background: #203040; }");

    auto* btnRow1 = new QHBoxLayout;
    struct Provider { const char* name; const char* url; };
    static constexpr Provider providers[] = {
        {"Claude",     "https://claude.ai/new"},
        {"ChatGPT",    "https://chat.openai.com/"},
        {"Gemini",     "https://gemini.google.com/"},
        {"Grok",       "https://grok.x.ai/"},
        {"Perplexity", "https://www.perplexity.ai/"},
    };
    for (const auto& p : providers) {
        auto* btn = new QPushButton(QString::fromUtf8(p.name), dlg);
        btn->setStyleSheet(btnStyle);
        btn->setAutoDefault(false);
        QString url = QString::fromUtf8(p.url);
        connect(btn, &QPushButton::clicked, dlg, [url, statusLabel] {
            QApplication::clipboard()->setText(kPrompt);
            QDesktopServices::openUrl(QUrl(url));
            statusLabel->setText(QStringLiteral(
                "Prompt copied to clipboard. Paste it into the AI, "
                "then come back and click Submit Your Idea"));
            statusLabel->show();
        });
        btnRow1->addWidget(btn);
    }
    vbox->addLayout(btnRow1);

    vbox->addSpacing(8);

    // Submit / Report / Close
    auto* btnRow2 = new QHBoxLayout;

    auto* submitBtn = new QPushButton(QStringLiteral("Submit Your Idea"), dlg);
    submitBtn->setAutoDefault(false);
    submitBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #00b4d8; color: #0f0f1a; font-weight: bold; "
        "border-radius: 4px; padding: 8px 20px; font-size: 13px; }"
        "QPushButton:hover { background: #00c8f0; }"));
    connect(submitBtn, &QPushButton::clicked, dlg, [dlg] {
        QDesktopServices::openUrl(QUrl(QStringLiteral(
            "https://github.com/boydsoftprez/NereusSDR/issues/new?template=feature_request.yml")));
        QTimer::singleShot(500, dlg, &QDialog::close);
    });
    btnRow2->addWidget(submitBtn);

    auto* bugBtn = new QPushButton(QStringLiteral("Report a Bug"), dlg);
    bugBtn->setAutoDefault(false);
    bugBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #cc4040; color: #ffffff; font-weight: bold; "
        "border-radius: 4px; padding: 8px 20px; font-size: 13px; }"
        "QPushButton:hover { background: #dd5050; }"));
    connect(bugBtn, &QPushButton::clicked, dlg, [dlg] {
        QDesktopServices::openUrl(QUrl(QStringLiteral(
            "https://github.com/boydsoftprez/NereusSDR/issues/new?template=bug_report.yml")));
        QTimer::singleShot(500, dlg, &QDialog::close);
    });
    btnRow2->addWidget(bugBtn);

    auto* closeBtn = new QPushButton(QStringLiteral("Close"), dlg);
    closeBtn->setAutoDefault(false);
    closeBtn->setStyleSheet(btnStyle);
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);
    btnRow2->addWidget(closeBtn);
    vbox->addLayout(btnRow2);

    // Copy prompt to clipboard on first open
    QApplication::clipboard()->setText(kPrompt);

    dlg->show();
}

} // namespace NereusSDR
