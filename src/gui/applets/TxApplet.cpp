// =================================================================
// src/gui/applets/TxApplet.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs — chkMOX_CheckedChanged2
//   (29311-29678 [v2.10.3.13]) and chkTUN_CheckedChanged (29978-30157
//   [v2.10.3.13]); original licence from Thetis source is included below.
//
// Layout from AetherSDR TxApplet.{h,cpp} (GPLv3, see AetherSDR attribution
// block below).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-23 - A TX Keying copy can hide its slice indicators.
//                 Brian Sutton, AI-assisted via xAI Grok.
//   2026-10-21 - Copies of a row carry the gates, follow the Core's 2-TONE
//                and ask to take transmit; a preview of a row is inert.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-20 - A row's wiring ends with whichever goes first, the row or
//                the applet (connectRow), so a container closing a copy no
//                longer leaves the model writing to it; one power refresh
//                timer for every row. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-10-20 - The rows as independent container content: each row's wiring
//                moves into a wireXSect() method the applet's own row and a
//                container's copy share, the gates and state pushes reach the
//                copies, and a copy gets the column's margins.
//                Brian Sutton, AI-assisted via OpenCode.
//   2026-10-14 - The 2-Tone tooltip names the page as the Settings tree
//                does: Settings > Test > Two-Tone IMD. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-10-12 - Seed and resync remote TX indicators from Core mirrors.
//                J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-12 - Fit live inset threshold/hold readouts to the actual
//                 slider range and resolved workspace font/style metrics.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-12 - Shared accessory display readings and independent legal
//                 marker / display scale by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-10-09 - PC mic status for the transmit badge (native audio plan
//                Task 20; R-AUD-24, R-AUD-09, R-AUD-13). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-10-02  J.J. Boyd / KG4VCF. TX letters share the guarded flag
//                Take and select action, with current access and target
//                lifetime checks. AI-assisted via OpenAI Codex.
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Layout pattern from AetherSDR `src/gui/TxApplet.{h,cpp}`.
//                 Wiring deferred to Phase 3M.
//   2026-04-26 — Phase 3M-1a H.3: deep-wired TUNE/MOX/Tune-Power/RF-Power.
//                 Out-of-phase controls (2-Tone, PS-A) hidden.
//                 syncFromModel() implemented. setCurrentBand(Band) added.
//   2026-04-28 — Phase 3M-1b J.2: VOX toggle button added below Tune Power.
//                 Checkable, green border when active. Bidirectional with
//                 TransmitModel::voxEnabled. Right-click opens VoxSettingsPopup.
//                 (REMOVED 2026-05-03 in 3M-3a-iii Task 16 — see entry below.)
//   2026-04-28 — Phase 3M-1b J.3: MON toggle button + monitor volume slider
//                 added below VOX. Bidirectional with TransmitModel::monEnabled
//                 and monitorVolume (default 0.5f, Thetis audio.cs:417). Mic-source
//                 badge added above the gauges ("PC mic"/"Radio mic"), driven by
//                 TransmitModel::micSourceChanged. Phase J complete.
//   2026-04-28 — Phase 3M-1b (relocation): Mic Gain slider row (J.1) removed from
//                 TxApplet. Relocated to PhoneCwApplet (#5 slot) per JJ feedback.
//   2026-04-28 — Phase 3M-1b K.2: tooltipForMode(DSPMode) helper + onMoxModeChanged
//                 slot implemented. Wired to SliceModel::dspModeChanged via
//                 RadioModel active-slice accessor so the MOX button tooltip
//                 reflects the rejection reason for CW/AM/FM/etc. modes.
//                 Closes Phase K.
//   2026-04-30 — Phase 3M-3a-ii Batch 6 (Task F + A): PROC button enabled and
//                 bidirectionally wired to TransmitModel::cpdrOn.  CFC button
//                 added next to PROC, bidirectional with cfcEnabled, right-
//                 click opens modeless TxCfcDialog (single instance kept alive
//                 across opens — same pattern as TxEqDialog).
//                 requestOpenCfcDialog() public slot exposed so CfcSetupPage's
//                 [Configure CFC bands…] button routes to the same dialog
//                 instance via a MainWindow-side signal connection.
//   2026-05-02 — Plan 4 Cluster C (Task 4 / D2+D3+D9-status): TX BW spinbox
//                 row added below Profile combo — Lo/Hi QSpinBox pair
//                 bidirectional with TransmitModel::filterLow/filterHigh;
//                 orange status label shows filter description text.
//                 Wired via filterChanged(int,int) + dspModeChanged refresh.
//                 syncFromModel() extended to seed spinboxes + status label.
//   2026-05-04 — Phase 3M-3a-iii bench polish: VOX row relocated from
//                 PhoneCwApplet Phone tab (Control #10) back to TxApplet as
//                 a full row directly under TUNE/MOX (Option B - full row).
//                 VOX button + threshold slider + DexpPeakMeter strip + Hold
//                 slider all move as a unit, plus the 100 ms peak-meter
//                 poller (now TxApplet::pollVoxMeter) and the right-click
//                 → Setup → Transmit → DEXP/VOX signal handling.  DEXP row
//                 stays on PhoneCwApplet — only VOX moves.
//   2026-09-22 — Routed the PS-A toggle through RadioModel's shared
//                 PureSignalSessionFacade for local and remote sessions.
//   2026-09-24 : R-R3-45: Speakers / Headphones choice for MON on the MON
//                 row, with a plain notice when the headphones are chosen
//                 and not open. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-24 - iPhone app Task 19 (R-IOS-06): the SWR gauge's range and
//                 the RF power gauge's headroom come from ControlRanges.h,
//                 which the Core's catalogue reads too. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-24 : R-R3-49 (parity Task 1): setTransmitSettingsPermitted.
//                 RF Power and the TX filter low and high follow the
//                 transmit settings gate in a remote window; the keying
//                 controls keep setTransmitPermitted. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-24 : R-R3-49 (parity Task 2): Tune Power, the VOX level and
//                 delay, MON, its level and output pair, LEV, EQ and CFC
//                 follow setTransmitChainSettingsPermitted in a remote
//                 window. Its Tune Power slider asks the Core
//                 (setTunePowerForTxBand) and shows the Core's value; the
//                 MON output pair routes this computer's monitor audio in a
//                 remote window too. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-25 : R-R3-49 (parity Task 3): the profile combo follows
//                 setTxProfilePermitted; in a remote window its manager
//                 mirrors the Core's profiles. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-25 : R-R3-49 (parity Task 4): the EQ and CFC right-clicks open
//                 their dialogs in a remote window; setTxProcessingPermitted
//                 greys the CFC dialog with the reason. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-25 : R-R3-49 (group A fix wave, M3): setPowerByBandPermitted.
//                 An RF Power move writes the band slot and the tune drive
//                 source only where the Core takes them (version 5).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 : Receiver and transmit gaps plan, Task 7: the MOX button
//                 keys through RadioModel::setMoxFromButton (a manual key,
//                 Thetis chkMOX_Click). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-25 : R-R3-49 (parity Task 7): PS-A follows
//                 setPureSignalArmingPermitted and the facade's canArm, no
//                 longer the keying gate. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-25 : Receiver and transmit gaps plan, Task 16: receive only
//                 disables MOX (outside SPEC and DRM), TUNE, 2-Tone and VOX
//                 with its reason (console.RXOnly, console.cs:15312-15334
//                 [v2.10.3.15]). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-25 : Task 16 fix wave: MOX disabled in every mode (I3), the
//                 MOX tooltip and lock follow the active slice (M3), and
//                 the lock names the remote transmit reason too (M6).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-09-25 : Task 16 fix wave 2: a mode or slice change while the
//                 transmit-permission layer holds MOX changes the tooltip
//                 it gives back, not the reason shown. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-25 - iPhone app plan, desktop remote transmit (R-IOS-13): in a
//                 remote window MOX, TUNE and 2-TONE show the Core's state
//                 and 2-TONE goes through RadioModel::setTwoTone. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave: I4 the line saying who holds
//               transmit. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave 2 (M8): VOX shows disabled with the
//               plain reason while this computer has no microphone line to
//               the Core; the Core's refusal stays the backstop. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  iPhone app plan Task 78 (R-IOS-02,
//                R-IOS-30): a Take transmit button under the holder line.
//                AI-assisted via Anthropic Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  the RF Power and Tune labels read
//                rfPowerShownFor / tunePowerShownFor (HpsdrModel.h), which
//                the Core's catalogue reads too (R-IOS-06, R-IOS-27). The
//                labels are unchanged. AI-assisted via Anthropic Claude
//                Code.
//   2026-09-27  J.J. Boyd / KG4VCF  the HL2 RF Power and Tune labels
//                snap and round a value between steps as mi0bot's
//                UpdateDriveLabel and UpdateTuneLabel do (R-IOS-06,
//                R-IOS-27). AI-assisted via Anthropic Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  Remote-window parity Task 32 (R-IOS-13,
//                R-R3-49): setMonitorOutputPermitted, the MON output pair
//                in a remote window on a Core that does not send the
//                transmit monitor. AI-assisted via Anthropic Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  Remote-window parity Task 33 (R-R3-49,
//                R-IOS-13): the CFC dialog's bar chart from the Core's
//                stream (setStationCfcBarChart); in a remote window the RF
//                Pwr and SWR bars fall at the Core's unkey as a local
//                window's do at its own. AI-assisted via Anthropic Claude
//                Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Addendum G-42 item 4: the tooltip for a
//                mode that cannot transmit matches the refusal's words.
//                AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 2: MOX, TUN, 2TONE and VOX are disabled
//                with the reason while a TX inhibit holds (the HL2 I/O
//                board's fault code, say), as Thetis's TXInhibit setter
//                does. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                Code.
//   2026-09-29 - Setup publication (CFC band editor): in a remote window
//                the CFC dialog sends its band table as the Core's
//                cfc.setProfile command when the Core takes it, and hears
//                that command's answer. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  setCurrentBand no longer recalls PWR
//                and a local window's RF Power slider no longer saves the
//                band slot: RadioModel does both (applyTransmitBand,
//                drivePowerScroll). AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  PA on-air gate review: the local Tune
//                Power slider writes and shows the transmit band's tune
//                power (Thetis ptbTune_Scroll, console.cs:46618
//                [v2.10.3.15]), not the pan band's. AI-assisted via
//                Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 11 (Q15, U8):
//                the TX band, per-band power, MOX mode tooltip and TX filter
//                status follow the transmit slice (setTransmitSliceResolver,
//                followTransmitSlice), never a listened slice; the
//                transmit-slice letter row. AI-assisted via Anthropic Claude
//                Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 11 fix: ports
//                Thetis's MOX gate on the TX band: the band (and the power
//                the slider recalls and writes) holds while transmitting.
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  TX rulings (item 1): a remote window's
//                MOX and TUNE press toggles against its own key
//                (RadioModel::moxPressAsksOn, tunePressAsksOn).
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  TX safety: a lost radio link locks
//                MOX, TUN and 2TONE with its reason until the link is back
//                (RadioModel::radioLinkDownChanged); VOX stays as Thetis's
//                power-off leaves it (console.cs:27488-27493 [v2.10.3.15]).
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Fix wave GUI-I6: a remote window's
//                Tune Power slider ignores the Core's value while held or
//                while its change is on its way.
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  TX-parity-linkdown (fix wave): the
//                lock follows connectionStateChanged too, so a remote
//                window whose Core has no radio locks MOX, TUN and 2TONE.
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Fix round 1 (minor 2): the remote
//                Tune Power slider shows the Core's value when its change
//                is answered, whatever arrived first. AI-assisted via
//                Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Fix round 1 (minor 5): PS-A shows,
//                disabled with its reason, while the board is not known;
//                only a known board without PureSignal hides it.
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Fix round 1 (minor 4): the link-down
//                words follow the window's link to the Core and the Core's
//                waiting for a radio. AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Fix wave, hosting 2-TONE parity: a
//                hosting window's 2-TONE asks to take transmit, as MOX and
//                TUNE do (setDesktopTwoToneHandler). AI-assisted via
//                Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Fix wave GUI-I7: PS-A greyed by the
//                PureSignal facade carries the facade's reason.
//                AI-assisted via Anthropic Claude Code.
//   2026-10-06  J.J. Boyd / KG4VCF  R-SPK-21: the mic source badge points
//                to Settings > Audio > Microphone, the new name of TX Input.
//                AI-assisted via Anthropic Claude Code.
//   2026-10-08  J.J. Boyd (KG4VCF): Qt 6.11.0 Cocoa accessibility cache
//               repair on a live item view (QtCocoaAccessibleTableCache).
//               AI-assisted via Anthropic Claude Code.
//   2026-10-08  J.J. Boyd / KG4VCF  Issue #357: syncDesktopKeyState's
//                fallback reads the transmit model's TUNE, which the
//                release press clears, as the hosting window's handler
//                does. AI-assisted via Anthropic Claude Code.
//   2026-10-12  J.J. Boyd (KG4VCF): approved 6 px workspace-look TX
//               panel row spacing. AI-assisted via OpenAI Codex.
//   2026-10-13 - Present the window-owned DisplayDuplex preference beside PS-A.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation
//                 via OpenAI Codex.
//   2026-10-15  J.J. Boyd (KG4VCF): rows change after a real model reset
//               (resetItemsBeforeRowChange), so accessibility cannot read a
//               deleted cell. AI-assisted via Anthropic Claude Code.
//   2026-10-18  J.J. Boyd (KG4VCF): the RF Power and Tune Power sliders no
//               longer change the tune drive source; Setup > Transmit >
//               Power alone chooses it, as in Thetis (setPowerByBandPermitted
//               removed with the write it gated). applyDriveSourceGate grays
//               the slider TUNE or the two-tone test is not using, from
//               Thetis setupTuneDriveSlider, grayed where Thetis hides.
//               AI-assisted via Anthropic Claude Code.
//   2026-10-21  J.J. Boyd (KG4VCF): the drive source gate sets a grayed
//               power slider's accessible description to its reason, as
//               the transmit gates do. AI-assisted via Anthropic Claude Code.
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

// =================================================================
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   Layout pattern from AetherSDR `src/gui/TxApplet.{h,cpp}`.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
// =================================================================

// TxApplet — TX control panel.
// Phase 3M-1a H.3: TUNE/MOX/Tune-Power/RF-Power deep-wired.
// Phase 3M-1b J.3: MON toggle + monitor volume slider + mic-source badge wired.
// Phase 3M-3a-iii bench polish (2026-05-04): VOX row relocated back from
//   PhoneCwApplet — full row [VOX btn][threshold + peak strip][Hold slider].
// Out-of-phase controls (2-Tone, PS-A) hidden.
//
// Control inventory:
//  0.  Mic-source badge  — read-only "PC mic"/"Radio mic"  [WIRED — 3M-1b J.3]
//  1.  Fwd Power gauge   — HGauge 0–120 W, redStart 100 W
//  2.  SWR gauge         — HGauge 1.0–3.0, redStart 2.5
//  3.  RF Power slider   + label + value  [WIRED — 3M-1a H.3]
//  4.  Tune Power slider + label + value  [WIRED — 3M-1a H.3]
//      (Mic Gain slider relocated to PhoneCwApplet — 2026-04-28 relocation)
//  4a. TUNE + MOX button row [WIRED — 3M-1a H.3]
//  4b. VOX row — [VOX btn][Threshold slider + DexpPeakMeter strip][-N dB]
//      [Hold slider 1..2000 ms][N ms]  [WIRED — 3M-3a-iii bench polish]
//      Bidirectional with voxEnabled / voxThresholdDb / voxHangTimeMs.
//      Right-click VOX → openSetupRequested("Transmit", "DEXP/VOX").
//      Relocated from PhoneCwApplet Phone tab Control #10.
//  4c. MON toggle button — checkable, blue:checked style  [WIRED — 3M-1b J.3]
//      Bidirectional with TransmitModel::monEnabled (default false).
//  4d. Monitor volume slider — 0..100 → monitorVolume 0.0..1.0  [WIRED — 3M-1b J.3]
//      Default 50 (model default 0.5f, Thetis audio.cs:417).
//  5.  MOX button        — checkable, red:checked style  [WIRED — 3M-1a H.3]
//  6.  TUNE button       — checkable, red:checked + "TUNING..." text  [WIRED — 3M-1a H.3]
//  7.  ATU button        — checkable (NYI — 3M-2/3M-3)
//  8.  MEM button        — checkable (NYI — 3M-2/3M-3)
//  9.  TX Profile combo  — (NYI — 3M-3)
// 10.  Tune mode combo   — (NYI — 3M-3)
// 11.  2-Tone test       — HIDDEN until Phase 3M-3
// 12.  PS-A toggle       — HIDDEN until Phase 3M-4
// 13.  DUP               — checkable (NYI — 3M-3)
// 14.  xPA indicator     — checkable (NYI — 3M-3)
// 15.  SWR protection LED — QLabel indicator (NYI — 3M-3)

#include "TxApplet.h"
#include "gui/QtCocoaAccessibleTableCache.h"
#include "TxEqDialog.h"
#include "TxCfcDialog.h"
#include "TxChainSection.h"
#include "TxProcessingSection.h"
#include "TxFilterSection.h"
#include "TxKeyingSection.h"
#include "TxMonitorSection.h"
#include "TxPowerSection.h"
#include "TxVoxSection.h"
#include "gui/HGauge.h"
#include "gui/StyleConstants.h"
#include "gui/ComboStyle.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/widgets/DexpPeakMeter.h"
#include "gui/widgets/InsetValueLabelFit.h"
#include "gui/widgets/VfoWidget.h"
#include "core/AudioEngine.h"
#include "core/ControlRanges.h"
#include "core/audio/CompositeTxMicRouter.h"
#include "core/MicProfileManager.h"
#include "core/MoxController.h"
#include "core/PureSignal.h"
#include "models/MainPowerMeter.h"
#include "gui/applets/AccessoryReadouts.h"
#include "gui/instruments/BarInstrument.h"
#include "core/RadioStatus.h"
#include "core/session/IStationLink.h"
#include "core/session/RemoteTransmitClient.h"
#include "core/session/StationCapabilities.h"
#include "core/session/PureSignalSessionFacade.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "core/TxSliceArbiter.h"
#include "models/PureSignalSettings.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <QComboBox>
#include <QContextMenuEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

constexpr int kWorkspaceTxRowSpacing = 6;
// The margins the applet's own column gives its rows: 4 px at the sides,
// 2 px above and below.  A copy of a row in a container has no column of
// its own, so it gives itself these margins itself.
constexpr int kTxColumnMarginH = 4;
constexpr int kTxColumnMarginV = 2;

// The tooltip a control had before the transmit-permission layer
// (setTransmitPermitted) disabled it, given back when permission returns.
constexpr auto kTransmitSavedTooltip = "TxAppletSavedTransmitTooltip";
constexpr auto kTransmitSavedEnabled = "TxAppletSavedTransmitEnabled";
constexpr auto kTransmitSavedDescription = "TxAppletSavedTransmitDescription";

// Disable a control with `reason` as its tooltip, remembering what it had,
// or put back what it had. Shared by the keying gate and the transmit
// settings gate, which hold disjoint controls. No model state is written.
// Fix wave GUI-I7: PS-A's own tooltip while the facade's reason shows.
constexpr auto kPsaFacadeSavedTooltip = "TxAppletPsaFacadeSavedTooltip";
constexpr auto kPsaFacadeSavedDescription = "TxAppletPsaFacadeSavedDescription";

void gateTransmitControl(QWidget* control, bool permitted, const QString& reason)
{
    if (!control) { return; }

    static constexpr auto kSavedTooltip = kTransmitSavedTooltip;
    static constexpr auto kSavedDescription = kTransmitSavedDescription;
    static constexpr auto kSavedEnabled = kTransmitSavedEnabled;
    if (!permitted) {
        if (!control->property(kSavedTooltip).isValid()) {
            control->setProperty(kSavedTooltip, control->toolTip());
            control->setProperty(kSavedDescription, control->accessibleDescription());
            control->setProperty(kSavedEnabled, control->isEnabled());
        }
        control->setEnabled(false);
        control->setToolTip(reason);
        control->setAccessibleDescription(reason);
        return;
    }

    if (control->property(kSavedTooltip).isValid()) {
        control->setEnabled(control->property(kSavedEnabled).toBool());
        control->setToolTip(control->property(kSavedTooltip).toString());
        control->setAccessibleDescription(
            control->property(kSavedDescription).toString());
        control->setProperty(kSavedTooltip, QVariant());
        control->setProperty(kSavedDescription, QVariant());
        control->setProperty(kSavedEnabled, QVariant());
    }
}

// The power sliders' own tooltips (rescalePowerSlidersForModel).
// From Thetis console.resx + mi0bot-Thetis console.resx [v2.10.3.13-beta2]
constexpr auto kRfPowerTooltip =
    "Transmit Drive - relative value, not watts unless the PA profile "
    "is configured with MAX watts @ 100%";
constexpr auto kTunePowerTooltip =
    "Tune and/or 2Tone Drive - relative value, not watts unless the "
    "PA profile is configured with MAX watts @ 100%";

// The drive source gate's enabled state, tooltip and accessible
// description for a power slider: the description is the gate's reason,
// as gateTransmitControl sets it, and empty while the slider is live.
// While a transmit permission gate holds the slider (a remote window that
// may not change it), the gate's saved state takes them, so the slider
// comes back as the source says when permission returns.
void setDriveSourceEnabled(QWidget* control, bool enabled, const QString& tooltip,
                           const QString& reason)
{
    if (!control) { return; }
    const QString description = enabled ? QString() : reason;
    if (control->property(kTransmitSavedTooltip).isValid()) {
        control->setProperty(kTransmitSavedEnabled, enabled);
        control->setProperty(kTransmitSavedTooltip, tooltip);
        control->setProperty(kTransmitSavedDescription, description);
        return;
    }
    control->setEnabled(enabled);
    control->setToolTip(tooltip);
    control->setAccessibleDescription(description);
}

// A preview of a row (ContentRenderMode::Preview): it shows the row but can
// change nothing.  Disabled, so neither a click nor a key toggles it, and
// every control takes no focus, so the keyboard never reaches one.
void makeRowInert(QWidget* row)
{
    row->setEnabled(false);
    row->setFocusPolicy(Qt::NoFocus);
    for (QWidget* child : row->findChildren<QWidget*>()) {
        child->setFocusPolicy(Qt::NoFocus);
    }
}

} // namespace

TxApplet::TxApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();
    wireControls();
    if (model && model->role() == RadioModel::Role::Remote) {
        setTransmitPermitted(false);
        setTransmitSettingsPermitted(false);
        setTransmitChainSettingsPermitted(false);
        setTxProfilePermitted(false);
        setTxProcessingPermitted(false);
    }
}

void TxApplet::buildUI()
{
    // Outer layout: zero margins (title bar flush to edges)
    // Body: padded content — matches AetherSDR TxApplet.cpp outer/inner pattern
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* body = new QWidget(this);
    body->setStyleSheet(QStringLiteral("background: %1;").arg(Style::kPanelBg));
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(kTxColumnMarginH, kTxColumnMarginV,
                            kTxColumnMarginH, kTxColumnMarginV);
    vbox->setSpacing(WorkspacePreview::enabled() ? kWorkspaceTxRowSpacing : 2);
    outer->addWidget(body);

    // ── 0. Mic-source badge ── read-only label above the gauges ─────────────
    // Phase 3M-1b J.3: shows "PC mic" or "Radio mic" reflecting
    // TransmitModel::micSource (default MicSource::Pc). Read-only; no interaction.
    // Updates on micSourceChanged signal (wired in wireControls()).
    {
        m_micSourceBadge = new QLabel(QStringLiteral("PC mic"), this);
        m_micSourceBadge->setAlignment(Qt::AlignCenter);
        m_micSourceBadge->setFixedHeight(16);
        m_micSourceBadge->setStyleSheet(micSourceBadgeStyle(false));
        m_micSourceBadge->setAccessibleName(QStringLiteral("Mic source indicator"));
        m_micSourceBadge->setToolTip(QStringLiteral(
            "Active microphone source: PC mic or Radio mic.\n"
            "Change via Settings > Audio > Microphone."));
        vbox->addWidget(m_micSourceBadge);
    }

    // ── 1-4. The forward-power and SWR gauges and the RF Power and Tune Power
    // slider rows — all moved to TxPowerSection. The section is given this
    // column's own row spacing so the rows inside it keep the same gap they
    // had here, and the model's power meter so the gauge is scaled the way it
    // was scaled when the rows were built.
    m_powerSection = new TxPowerSection(
        body, vbox->spacing(), m_model ? m_model->mainPowerMeter() : nullptr,
        txRowMargins());
    vbox->addWidget(m_powerSection);

    // ── Button row: TUNE + MOX (50% each) ──────────────────────────────────
    // Positioned above VOX+MON for action-button prominence
    // (docs/superpowers/plans/2026-05-01-ui-polish-right-panel.md §Task 5).
    // ATU + MEM removed (ATU phase, no plan yet; MEM = channel-memory phase).
    // MOX: red active (#cc2222 bg, #ff4444 border, white text)
    // TUNE: red active when tuning, text becomes "TUNING..."
    // ── 4a/4b. The TUNE and MOX buttons, the transmit-slice letter row, the
    // label that says who holds transmit, and the Take transmit button — all
    // moved to TxKeyingSection. The section is given this column's own row
    // spacing so the rows inside it keep the same gap they had here.
    m_keyingSection = new TxKeyingSection(body, vbox->spacing(), txRowMargins());
    vbox->addWidget(m_keyingSection);
    // The Take-transmit press is this applet's own signal, so it is wired
    // here rather than inside the section.
    connect(m_keyingSection->takeTransmitButton(), &QPushButton::clicked, this,
            &TxApplet::takeTransmitRequested);

    // ── 4b. VOX row (3M-3a-iii bench polish 2026-05-04) ───────────────────────
    // Relocated from PhoneCwApplet (Phone tab Control #10).  Operators
    // wanted the VOX engage surface next to MOX/TUNE on the right pane
    // where they engage TX, not buried on the Phone tab.  Full row moves
    // as a unit including the live DexpPeakMeter strip + 100 ms poller.
    //
    // Layout (Option B - full row):
    //   [VOX btn 48px] | { Threshold slider top, DexpPeakMeter strip
    //   below } | [-20 dB inset] | [Hold slider 1..2000 ms] | [500 ms inset]
    //
    // Threshold slider range -80..0 dB matches Thetis ptbVOX
    // (console.Designer.cs:6018-6019 [v2.10.3.13]).  Hold slider range
    // 1..2000 ms matches Thetis udDEXPHold (setup.designer.cs:45005-45013
    // [v2.10.3.13]).  Default values mirror Thetis ptbVOX.Value=-20
    // (console.Designer.cs:6024) and udDEXPHold.Value=500
    // (setup.designer.cs:45020).
    //
    // Slider-stack: a small QVBoxLayout wraps the threshold slider (top)
    // and DexpPeakMeter (below) so the live mic peak strip sits directly
    // under the threshold knob — matches Thetis picVOX placement.
    // ── 4b. VOX row: the VOX toggle, the threshold slider with the live
    // DexpPeakMeter strip, the hold-time slider and their inset readouts —
    // all moved to TxVoxSection. The section is given this column's own
    // row spacing so the rows inside it keep the same gap they had here.
    m_voxSection = new TxVoxSection(body, vbox->spacing(), txRowMargins());
    vbox->addWidget(m_voxSection);

    // ── 4c. MON toggle button + 4d. Monitor volume slider ─────────────────────
    // ── 4c/4d. MON row: the MON toggle, the SPEAKERS / PHONES output pair,
    // the notice that says why MON is silent, and the monitor volume
    // slider — all moved to TxMonitorSection. The section is given this
    // column's own row spacing so the rows inside it keep the same gap
    // they had here.
    m_monitorSection = new TxMonitorSection(body, vbox->spacing(), txRowMargins());
    vbox->addWidget(m_monitorSection);

    // ── 4e. The LEV, EQ and CFC buttons and the TX Profile combo — all
    // moved to TxChainSection. The section is given this column's own row
    // spacing so the rows inside it keep the same gap they had here.
    m_chainSection = new TxChainSection(body, vbox->spacing(), txRowMargins());
    vbox->addWidget(m_chainSection);

    // ── 4f. The TX bandpass filter cutoff spinboxes and the filter status
    // label — all moved to TxFilterSection. The section is given this
    // column's own row spacing so the rows inside it keep the same gap they
    // had here.
    m_filterSection = new TxFilterSection(body, vbox->spacing(), txRowMargins());
    vbox->addWidget(m_filterSection);

    // ── 4g. The 2-Tone, PS-A and DUP buttons and the SWR protection LED —
    // all moved to TxProcessingSection. The section is given this column's
    // own row spacing so the rows inside it keep the same gap they had here.
    m_processingSection = new TxProcessingSection(body, vbox->spacing(), txRowMargins());
    vbox->addWidget(m_processingSection);
    // The DUP press raises this applet's own signal, so it is wired here
    // rather than inside the section.

    vbox->addStretch();
}

// ── Phase 3M-1a H.3 wiring ──────────────────────────────────────────────────
//
// wireControls: called once after buildUI(). Attaches signal/slot connections
// between the four wired controls and the model layer.
//
// Pattern follows the NereusSDR "GUI↔Model sync, no feedback loops" rule:
//   - Use QSignalBlocker (or m_updatingFromModel) to prevent echo loops.
//   - Model setters emit signals → RadioConnection sends protocol commands.
//   - UI state changes → model setters → emit back to update other UI.
void TxApplet::wireControls()
{
    if (!m_model) {
        return;
    }

    TransmitModel& tx = m_model->transmitModel();

    wirePowerSection(m_powerSection);
    wireKeyingSection(m_keyingSection);

    // The slider TUNE or the two-tone test is not using is grayed; Thetis
    // reruns setupTuneDriveSlider on each source change and on TUN and 2TONE
    // (console.cs:46651, 46673, 30206, 44832 [v2.10.3.15]).
    connect(&tx, &TransmitModel::tuneDrivePowerSourceChanged,
            this, [this](DrivePowerSource) { applyDriveSourceGate(); });
    connect(&tx, &TransmitModel::twoToneDrivePowerSourceChanged,
            this, [this](DrivePowerSource) { applyDriveSourceGate(); });
    connect(&tx, &TransmitModel::tuneChanged,
            this, [this](bool) { applyDriveSourceGate(); });
    connect(&tx, &TransmitModel::twoToneActiveChanged,
            this, [this](bool) { applyDriveSourceGate(); });

    // ── K.2: MOX button tooltip override ← SliceModel::dspModeChanged ─────────
    // Phase 3M-1b K.2: update MOX button tooltip when DSP mode changes.
    // For modes that are deferred to a later phase (CW → 3M-2, AM/FM → 3M-3)
    // the tooltip explains why MOX won't engage, matching the moxRejected reason.
    // Wired here (wireControls) rather than syncFromModel because the active
    // slice can change after construction.
    if (m_model) {
        // Task 16 fix wave (M3): the MOX tooltip, and the receive-only lock
        // over it, follow the active slice when it changes, not only the
        // slice that was active here.
        // Slice control plan Task 11 (Q15): the transmit slice, not the
        // active one; re-followed whenever either may have moved.
        followTransmitSlice();
        refreshTransmitSliceChoices();
        connect(m_model, &RadioModel::activeSliceChanged,
                this, [this](int) { followTransmitSlice(); });
        if (TxSliceArbiter* arbiter = m_model->txSliceArbiter()) {
            connect(arbiter, &TxSliceArbiter::txBoundSliceChanged, this, [this](int, int) {
                followTransmitSlice();
                refreshTransmitSliceChoices();
            });
        }
        // A remote window's flag arrives mirrored from the Core onto the
        // slice, so each slice's own flag change re-follows too.
        watchTransmitFlags();
        connect(m_model, &RadioModel::sliceAdded, this, [this](int) {
            watchTransmitFlags();
            followTransmitSlice();
            refreshTransmitSliceChoices();
        });
        connect(m_model, &RadioModel::sliceRemoved, this, [this](int) {
            followTransmitSlice();
            refreshTransmitSliceChoices();
        });
        // Task 16: receive only turning on or off, or its reason changing
        // (a radio with no transmitter).
        connect(m_model, &RadioModel::rxOnlyChanged, this, [this](bool) {
            removeReceiveOnlyLock();
            applyReceiveOnlyLock();
        });
        // HL2 port part 2: a TX inhibit locks the same buttons, with its
        // reason (the HL2 I/O board's fault code, say), as Thetis's
        // TXInhibit setter disables them (console.cs:15341-15363
        // [v2.10.3.15]):
        //   chkTUN.Enabled = !_tx_inhibit;
        //   chk2TONE.Enabled = !_tx_inhibit; //MW0LGE_21a
        //   chkVOX.Enabled = !_tx_inhibit;
        const auto relock = [this]() {
            removeReceiveOnlyLock();
            applyReceiveOnlyLock();
        };
        connect(m_model, &RadioModel::txInhibitedChanged, this, relock);
        connect(m_model, &RadioModel::txInhibitReasonChanged, this, relock);
        // TX safety (2026-09-30): and a lost radio link locks MOX, TUN and
        // 2TONE until it is back, as Thetis's power-off on loss of sync
        // disables them (console.cs:27488-27493 [v2.10.3.15]).
        connect(m_model, &RadioModel::radioLinkDownChanged, this, relock);
        // TX-parity-linkdown (fix wave): a remote window's Core losing or
        // regaining its radio (RadioModel::transmitLinkDown) the same way.
        connect(m_model, &RadioModel::connectionStateChanged, this, relock);
        // Fix round 1 (minor 4): its words follow the window's link to the
        // Core and the Core's waiting for a radio.
        connect(m_model, &RadioModel::stationLinkStateChanged, this, relock);
        connect(m_model, &RadioModel::stationRadioWaitingChanged, this, relock);
        applyReceiveOnlyLock();
    }

    // ── 4b. VOX row wiring (3M-3a-iii bench polish 2026-05-04) ────────────────
    //
    // Bidirectional binds for [VOX] toggle + threshold/hold sliders, plus
    // right-click → openSetupRequested("Transmit", "DEXP/VOX") and a 100 ms
    // QTimer driving the VOX peak meter (TxApplet::pollVoxMeter).
    // Mirrors the wiring pattern from PhoneCwApplet (Phase 3M-3a-iii Task
    // 15) — relocated here as part of the 2026-05-04 bench polish.

    wireVoxSection(m_voxSection);

    // ── 100 ms timer driving the VOX peak meter ──────────────────────────────────
    // Cadence matches Thetis UpdateNoiseGate Task.Delay(100) at
    // console.cs:25347 [v2.10.3.13].  Stops automatically when the applet
    // is destroyed (parented to `this`).  Continuous (NOT MOX-gated) since
    // GetDEXPPeakSignal is the live DEXP detector envelope, not the
    // TX-pipeline meter.
    m_voxMeterTimer = new QTimer(this);
    m_voxMeterTimer->setInterval(100);
    connect(m_voxMeterTimer, &QTimer::timeout,
            this, &TxApplet::pollVoxMeter);
    m_voxMeterTimer->start();

    // ── MON row (TxMonitorSection) ───────────────────────────────────────────
    // The wiring lives in wireMonitorSection() so an independent copy of the
    // row, made for a container, is wired exactly the same way.
    wireMonitorSection(m_monitorSection);

    wireChainSection(m_chainSection);

    // ── Mic-source badge ← TransmitModel::micSourceChanged ───────────────────
    // Phase 3M-1b J.3. Read-only: updates badge text on signal, no user interaction.
    // "PC mic" for MicSource::Pc, "Radio mic" for MicSource::Radio, "VAX" for MicSource::Vax.
    connect(&tx, &TransmitModel::micSourceChanged,
            this, [this](MicSource) { refreshMicSourceBadge(); });
    connect(m_model, &RadioModel::remoteMicSourceStateChanged,
            this, &TxApplet::refreshMicSourceBadge);
    // Native audio plan Task 20 (R-AUD-24): the PC mic's status.
    connect(m_model, &RadioModel::pcMicStatusChanged,
            this, &TxApplet::refreshMicSourceBadge);

    wireFilterSection(m_filterSection);
    // ── iPhone app plan, desktop remote transmit (R-IOS-13) ─────────────────
    // A remote window's own MoxController never keys: MOX, TUNE and 2-TONE
    // light from the Core's state (its `transmitting`, the transmit object's
    // `tune`, PureSignal's two-tone), and a refused press puts them back.
    if (m_model && m_model->role() == RadioModel::Role::Remote) {
        connect(m_model, &RadioModel::transmittingChanged, this, &TxApplet::syncRemoteIndicators);
        connect(&m_model->transmitModel(), &TransmitModel::tuneChanged,
                this, &TxApplet::syncRemoteIndicators);
        connect(m_model, &RadioModel::remoteTransmitRefused, this, &TxApplet::syncRemoteIndicators);
        if (PureSignalSessionFacade* ps = m_model->pureSignalFacade()) {
            connect(ps, &PureSignalSessionFacade::statusChanged, this, &TxApplet::syncRemoteIndicators);
        }
    }

    // ── Phase 3M-4 / WDSP 2.10: PS-A button wiring ──────────────────────────
    // Source-first port of Thetis chkFWCATUBypass:
    //   - Left-click toggle drives PureSignal::setAutoCalEnabled (mirrors
    //     chkFWCATUBypass_Click, console.cs:36762 [v2.10.3.13]).
    //   - Right-click opens PsForm via openPureSignalDialogRequested (mirrors
    //     chkFWCATUBypass_MouseDown, console.cs:46149-46152 [v2.10.3.13]:
    //       if (IsRightButton(e)) linearityToolStripMenuItem_Click(null,
    //                                                              EventArgs.Empty);
    //     ).
    //
    // The button's own connections live in wireProcessingSection() so an
    // independent copy of the row, made for a container, is wired the same
    // way.  The PureSignal facade wiring below is applet-wide, not per row.
    wireProcessingSection(m_processingSection);

    // Production always uses RadioModel's one session facade. The local
    // coordinator remains late-bound behind that facade.
    if (m_processingSection->psaButton()) {
        if (m_model) {
            m_psFacade = m_model->pureSignalFacade();
            if (m_psFacade) {
                connect(m_psFacade, &PureSignalSessionFacade::statusChanged,
                        this, &TxApplet::syncPsaFromFacade);
                connect(m_psFacade, &PureSignalSessionFacade::actionResult,
                        this, [this](quint32, Ps3ActionPhase phase, const QString&,
                                     const QVariantMap&) {
                    if (phase == Ps3ActionPhase::Failed) {
                        syncPsaFromFacade();
                    }
                });
                if (PureSignalSettings* settings = m_psFacade->settings()) {
                    connect(settings, &PureSignalSettings::autoCalEnabledChanged,
                            this, &TxApplet::syncPsaFromFacade);
                }
            }
            if (PureSignal* ps = m_model->pureSignal()) {
                setPureSignal(ps);
            }
            connect(m_model, &RadioModel::pureSignalCoordinatorReady, this,
                    &TxApplet::setPureSignal);
        }
        syncPsaFromFacade();
    }
    
    // ── Initial sync from model ──────────────────────────────────────────────
    syncFromModel();
}

// Native audio plan Task 20 (R-AUD-24): the badge's normal look, or the
// area's amber warning look while the PC mic is missing or busy.
QString TxApplet::micSourceBadgeStyle(bool amber)
{
    return QStringLiteral(
        "QLabel {"
        " color: %1;"
        " font-size: 9px;"
        " border: 1px solid %2;"
        " border-radius: 2px;"
        " padding: 0px 4px;"
        " background: %3;"
        "}"
    ).arg(amber ? Style::kAmberText : Style::kTitleText,
          amber ? Style::kAmberBorder : Style::kInsetBorder,
          amber ? Style::kAmberBg : Style::kInsetBg);
}

void TxApplet::refreshMicSourceBadge()
{
    if (!m_model || !m_micSourceBadge) { return; }
    MicSource source = m_model->transmitModel().micSource();
    QString reason;
    if (!m_model->ownsLocalDsp()) {
        auto* tx = m_model->stationLink() ? m_model->stationLink()->remoteTransmit() : nullptr;
        if (tx) {
            source = tx->acceptedMicSource() == RemoteMicSource::RadioMic ? MicSource::Radio
                : source == MicSource::Vax ? MicSource::Vax : MicSource::Pc;
            if (!tx->micSourceSettled()) { reason = tx->micSourceReason(); }
        } else if (source == MicSource::Radio) {
            source = MicSource::Pc;
        }
    }
    QString text = source == MicSource::Radio ? QStringLiteral("Radio mic")
        : source == MicSource::Vax ? QStringLiteral("VAX") : QStringLiteral("PC mic");
    QString tip = !reason.isEmpty() ? reason
        : source == MicSource::Radio && !m_model->ownsLocalDsp()
            ? QStringLiteral("Radio microphone at the Core (no microphone stream from this computer).")
            : QStringLiteral("Change microphone source via Settings > Audio > Microphone.");

    // Native audio plan Task 20 (R-AUD-24, R-AUD-09, settled call 35): a
    // PC mic that is not connected, or held by another program, turns the
    // badge amber and says so; it clears by itself when the device returns.
    // R-AUD-13: a playing PC mic's tooltip names the device in use. Other
    // failures (and a remote source the Core has not settled) keep the
    // badge above. Display only: keying stays on R-R3-36's rule.
    bool missing = false;
    if (source == MicSource::Pc && reason.isEmpty()) {
        const AudioRoleStatus mic = m_model->pcMicStatus();
        const QString device = !mic.chosenName.isEmpty() ? mic.chosenName
            : QStringLiteral("The microphone");
        const QString rest = QStringLiteral(
            " Transmit audio is silent until it comes back; NereusSDR never switches to "
            "another mic by itself. Change the source in Settings > Audio > Microphone.");
        if (mic.state == AudioRoleState::Silent && mic.reason == AudioRoleReason::NotConnected) {
            missing = true;
            text = QStringLiteral("PC mic not connected");
            tip = device + QStringLiteral(" is not connected.") + rest;
        } else if (mic.state == AudioRoleState::Silent && mic.reason == AudioRoleReason::InUse) {
            missing = true;
            text = QStringLiteral("PC mic in use by another program");
            tip = device + QStringLiteral(" is in use by another program.") + rest;
        } else if ((mic.state == AudioRoleState::Playing
                    || mic.state == AudioRoleState::PlayingOnDefault)
                   && !mic.playingName.isEmpty()) {
            tip = QStringLiteral("PC mic: ") + mic.playingName;
        }
    }
    m_micSourceBadge->setText(text);
    m_micSourceBadge->setToolTip(tip);
    if (missing != m_micSourceBadgeAmber) {
        m_micSourceBadgeAmber = missing;
        m_micSourceBadge->setStyleSheet(micSourceBadgeStyle(missing));
    }
}

void TxApplet::syncRemoteIndicators()
{
    if (!m_model || m_model->role() != RadioModel::Role::Remote) { return; }

    const bool wasUpdating = m_updatingFromModel;
    m_updatingFromModel = true;
    {
        const bool tuning = m_model->transmitModel().isTune();
        for (TxKeyingSection* s : allKeyingSections()) {
            if (s->moxButton()) {
                QSignalBlocker b(s->moxButton());
                s->moxButton()->setChecked(m_model->isTransmitting());
            }
            if (s->tuneButton()) {
                QSignalBlocker b(s->tuneButton());
                s->tuneButton()->setChecked(tuning);
                s->tuneButton()->setText(tuning ? QStringLiteral("TUNING...") : QStringLiteral("TUNE"));
            }
        }
    }
    {
        // Every copy of the 2-TONE button, not just the applet's own.
        const PureSignalSessionFacade* ps = m_model->pureSignalFacade();
        const bool twoTone = ps && ps->twoToneOn();
        for (TxProcessingSection* s : allProcessingSections()) {
            if (!s->twoToneButton()) { continue; }
            QSignalBlocker b(s->twoToneButton());
            s->twoToneButton()->setChecked(twoTone);
        }
    }
    m_updatingFromModel = wasUpdating;
}

void TxApplet::syncFromModel()
{
    if (!m_model) { return; }

    TransmitModel& tx = m_model->transmitModel();
    MoxController* mox = m_model->moxController();

    // The guard is saved and restored, not just set and cleared, so a sync
    // that happens inside another one (a copy made while the applet is
    // syncing) cannot clear the outer sync's guard early.
    const bool wasUpdating = m_updatingFromModel;
    m_updatingFromModel = true;

    // RF Power
    for (TxPowerSection* s : allPowerSections()) {
        QSignalBlocker b(s->rfPowerSlider());
        s->rfPowerSlider()->setValue(tx.power());
    }

    // Tune Power for current band
    {
        const int tunePwr = shownTunePower(m_currentBand);
        for (TxPowerSection* s : allPowerSections()) {
            QSignalBlocker b(s->tunePowerSlider());
            s->tunePowerSlider()->setValue(tunePwr);
        }
    }

    // Issue #175 Task 7: refresh both labels through the central
    // formatter (HL2 dB / non-HL2 int).  Replaces the two inline
    // QString::number() calls that bypassed HL2 dB conversion above.
    updatePowerSliderLabels();

    // VOX [ON] toggle + Threshold slider + Hold slider
    // (3M-3a-iii bench polish 2026-05-04 — relocated from PhoneCwApplet).
    // Bidirectional sync to TransmitModel::voxEnabled / voxThresholdDb /
    // voxHangTimeMs.  Note: voxEnabled is NEVER persisted (safety: VOX
    // always starts OFF), but we still pull whatever the current model
    // state is so any other UI surface (e.g. Setup → DEXP/VOX page) stays
    // in agreement.
    for (TxVoxSection* s : allVoxSections()) {
        if (s->button()) {
            QSignalBlocker bv(s->button());
            s->button()->setChecked(tx.voxEnabled());
        }
        if (s->thresholdSlider()) {
            {
                QSignalBlocker b(s->thresholdSlider());
                s->thresholdSlider()->setValue(tx.voxThresholdDb());
            }
            if (s->thresholdLabel()) {
                s->thresholdLabel()->setText(QStringLiteral("%1 dB").arg(tx.voxThresholdDb()));
            }
        }
        if (s->holdSlider()) {
            {
                QSignalBlocker b(s->holdSlider());
                s->holdSlider()->setValue(tx.voxHangTimeMs());
            }
            if (s->holdLabel()) {
                s->holdLabel()->setText(QStringLiteral("%1 ms").arg(tx.voxHangTimeMs()));
            }
        }
    }

    // MON button state (J.3 Phase 3M-1b)
    // monEnabled intentionally loads as OFF — plan §0 row 9 safety rule.
    for (TxMonitorSection* s : allMonitorSections()) {
        QSignalBlocker bm(s->monButton());
        s->monButton()->setChecked(tx.monEnabled());
    }

    // Monitor volume slider (J.3 Phase 3M-1b)
    // Sync slider position from model; default 0.5f → slider 50.
    {
        const int uiVal = qRound(tx.monitorVolume() * 100.0f);
        for (TxMonitorSection* s : allMonitorSections()) {
            QSignalBlocker bvol(s->volumeSlider());
            s->volumeSlider()->setValue(uiVal);
            s->volumeValue()->setText(QString::number(uiVal));
        }
    }

    // LEV / EQ / CFC button state (3M-3a-i Batch 2 Task F + 3M-3a-ii
    // Batch 6 Task A — CFC button added; PROC moved to PhoneCwApplet
    // 3M-3a-ii post-bench cleanup).
    for (TxChainSection* s : allChainSections()) {
        if (s->levButton()) {
            QSignalBlocker b(s->levButton());
            s->levButton()->setChecked(tx.txLevelerOn());
        }
        if (s->eqButton()) {
            QSignalBlocker b(s->eqButton());
            s->eqButton()->setChecked(tx.txEqEnabled());
        }
        if (s->cfcButton()) {
            QSignalBlocker b(s->cfcButton());
            s->cfcButton()->setChecked(tx.cfcEnabled());
        }
    }

    // TX BW spinboxes + status label (Plan 4 Cluster C Task 4 / D2+D3+D9)
    for (TxFilterSection* s : allFilterSections()) {
        if (s->filterLowSpin()) {
            QSignalBlocker bLo(s->filterLowSpin());
            s->filterLowSpin()->setValue(tx.filterLow());
        }
        if (s->filterHighSpin()) {
            QSignalBlocker bHi(s->filterHighSpin());
            s->filterHighSpin()->setValue(tx.filterHigh());
        }
    }
    refreshTxFilterStatus();

    // Mic-source badge (J.3 Phase 3M-1b; extended to 3-way in Phase 3M-VAX-toggle)
    refreshMicSourceBadge();

    // MOX / TUNE button state
    if (m_model->role() == RadioModel::Role::Remote) {
        syncRemoteIndicators();
    } else if (mox) {
        const bool moxOn = m_desktopMoxOn ? m_desktopMoxOn() : mox->isMox();
        const bool isManual = m_desktopTuneOn ? m_desktopTuneOn() : mox->isManualMox();
        for (TxKeyingSection* s : allKeyingSections()) {
            QSignalBlocker bm(s->moxButton());
            s->moxButton()->setChecked(moxOn);

            QSignalBlocker bt(s->tuneButton());
            s->tuneButton()->setChecked(isManual);
            s->tuneButton()->setText(isManual ? QStringLiteral("TUNING...") : QStringLiteral("TUNE"));
        }
    }

    m_updatingFromModel = wasUpdating;
}
void TxApplet::setDesktopKeyHandlers(std::function<void(bool)> mox,
                                     std::function<void(bool)> tune,
                                     std::function<bool()> moxOn,
                                     std::function<bool()> tuneOn)
{
    m_desktopMoxRequest = std::move(mox);
    m_desktopTuneRequest = std::move(tune);
    m_desktopMoxOn = std::move(moxOn);
    m_desktopTuneOn = std::move(tuneOn);
    syncFromModel();
}

void TxApplet::setDesktopTwoToneHandler(std::function<void(bool)> request)
{
    m_desktopTwoToneRequest = std::move(request);
}

void TxApplet::setTransmitSliceResolver(std::function<SliceModel*()> resolver)
{
    m_transmitSliceResolver = std::move(resolver);
    followTransmitSlice();
    refreshTransmitSliceChoices();
}

void TxApplet::setTransmitSliceChoices(std::function<bool(int)> controlled,
                                       std::function<void(int)> choose,
                                       std::function<QString()> unavailableReason,
                                       std::function<TransmitSliceChoice(int)> availability)
{
    m_txSliceControlled = std::move(controlled);
    m_txSliceChoose = std::move(choose);
    m_txSliceUnavailable = std::move(unavailableReason);
    m_txSliceAvailability = std::move(availability);
    refreshTransmitSliceChoices();
}

void TxApplet::refreshTransmitSliceChoices()
{
    // Every copy of the keying rows, not just the applet's own.
    for (TxKeyingSection* s : allKeyingSections()) {
        refreshTransmitSliceChoicesIn(s);
    }
}

void TxApplet::refreshTransmitSliceChoicesIn(TxKeyingSection* s)
{
    if (!s) { return; }
    s->clearTransmitSliceButtons();
    if (!m_model) { return; }
    const SliceModel* current = transmitSlice();
    const QString reason = m_txSliceUnavailable ? m_txSliceUnavailable() : QString();
    for (SliceModel* slice : m_model->slices()) {
        if (!slice) { continue; }
        const int id = slice->sliceIndex();
        // U8: only the slices this window controls, never one it listens to.
        if (m_txSliceControlled && !m_txSliceControlled(id)) { continue; }
        const TransmitSliceChoice availability = m_txSliceAvailability
            ? m_txSliceAvailability(id)
            : TransmitSliceChoice{reason.isEmpty(), reason};
        const QString toolTip = availability.toolTip.isEmpty()
            ? QStringLiteral("Transmit on slice %1").arg(slice->sliceLetter())
            : availability.toolTip;
        QPushButton* button = s->addTransmitSliceButton(
            slice->sliceLetter(), id, slice == current, availability.enabled, toolTip);
        connect(button, &QPushButton::clicked, this, [this, button, id](bool) {
            // The model's answer checks the row; the press alone does not.
            if (button) { button->setChecked(transmitSlice()
                                             && transmitSlice()->sliceIndex() == id); }
            if (m_txSliceChoose) {
                m_txSliceChoose(id);
            } else if (m_model) {
                // Ruling 8.10: the arbiter drops MOX before it moves the flag.
                m_model->requestTxHandoffToSlice(id);
            }
        });
    }
}

void TxApplet::syncDesktopKeyState()
{
    if (!m_model || !m_model->moxController()) { return; }
    const bool moxOn = m_desktopMoxOn ? m_desktopMoxOn()
                                       : m_model->moxController()->isMox();
    // Issue #357: the TUNE the release press clears, not isTune(), which
    // holds until the radio is back on receive.
    const bool tuning = m_desktopTuneOn ? m_desktopTuneOn()
                                        : m_model->transmitModel().isTune();
    for (TxKeyingSection* s : allKeyingSections()) {
        const QSignalBlocker moxBlock(s->moxButton());
        const QSignalBlocker tuneBlock(s->tuneButton());
        s->moxButton()->setChecked(moxOn);
        s->tuneButton()->setChecked(tuning);
        s->tuneButton()->setText(tuning ? QStringLiteral("TUNING...") : QStringLiteral("TUNE"));
    }
}

// (Phase 3M-1b J.2 showVoxSettingsPopup removed in 3M-3a-iii Task 16 —
//  the wired VOX surface lives on PhoneCwApplet now, and the per-parameter
//  popup gave way to right-click → Setup → Transmit → DEXP/VOX.)

void TxApplet::rescaleFwdGaugeForModel(HPSDRModel model)
{
    if (!m_powerSection) { return; }
    // Every copy of the power rows, not just the applet's own.
    for (TxPowerSection* s : allPowerSections()) {
        s->rescaleFwdGaugeForModel(
            model, m_model ? m_model->mainPowerMeter() : nullptr);
    }
}

void TxApplet::refreshPowerReadouts()
{
    if (!m_model || !m_powerSection) { return; }
    // Every copy of the power rows, not just the applet's own.
    for (TxPowerSection* s : allPowerSections()) {
        s->refreshPowerReadouts(
            m_model->mainPowerMeter(), m_model->isTransmitting(),
            m_model->hardwareProfile().model);
    }
}

void TxApplet::rescalePowerSlidersForModel(HPSDRModel model)
{
    for (TxPowerSection* s : allPowerSections()) {
        s->rescalePowerSlidersForModel(model);
    }
    // The drive source gate sets the sliders' tooltips (kRfPowerTooltip,
    // kTunePowerTooltip), with its reason while a slider is grayed.
    applyDriveSourceGate();
}

// ---------------------------------------------------------------------------
// The power slider TUNE or the two-tone test is not using is grayed.
//
// From Thetis console.cs:46677-46709 [v2.10.3.15] setupTuneDriveSlider:
//   if (_tuneDrivePowerSource != DrivePowerSource.TUNE_SLIDER && _2ToneDrivePowerSource != DrivePowerSource.TUNE_SLIDER)
//   {
//       // hide it
//       ptbTune.Visible = false;
//       lblTune.Visible = false;
//   }
//   ...
//   if (chkTUN.Checked)
//   {
//       ptbPWR.Enabled = _tuneDrivePowerSource == DrivePowerSource.DRIVE_SLIDER;
//       ptbTune.Enabled = _tuneDrivePowerSource == DrivePowerSource.TUNE_SLIDER;
//   }
//   else if (chk2TONE.Checked)
//   {
//       ptbPWR.Enabled = _2ToneDrivePowerSource == DrivePowerSource.DRIVE_SLIDER;
//       ptbTune.Enabled = _2ToneDrivePowerSource == DrivePowerSource.TUNE_SLIDER;
//   }
//   else
//   {
//       ptbPWR.Enabled = true;
//       ptbTune.Enabled = true;
//   }
// Thetis calls it from the source setters (console.cs:46651, 46673), and
// chkTUN_CheckedChanged and chk2TONE_CheckedChanged (console.cs:30206,
// 44832). NereusSDR keeps the Tune slider on screen, grayed with its reason,
// where Thetis hides it (JJ, 2026-10-18: disabled, never hidden).
// ---------------------------------------------------------------------------
void TxApplet::applyDriveSourceGate()
{
    const QList<TxPowerSection*> sections = allPowerSections();
    if (sections.isEmpty()) { return; }
    const QString rfTooltip = QString::fromLatin1(kRfPowerTooltip);
    const QString tuneTooltip = QString::fromLatin1(kTunePowerTooltip);
    if (!m_model) {
        for (TxPowerSection* s : sections) {
            setDriveSourceEnabled(s->rfPowerSlider(), true, rfTooltip, QString());
            setDriveSourceEnabled(s->tunePowerSlider(), true, tuneTooltip, QString());
        }
        return;
    }
    const TransmitModel& tx = m_model->transmitModel();
    const DrivePowerSource tuneSource = tx.tuneDrivePowerSource();
    const DrivePowerSource twoToneSource = tx.twoToneDrivePowerSource();

    bool rfOn = true;
    bool tuneOn = true;
    QString rfReason;
    QString tuneReason;
    const QString tuneSetup = tr("TUNE's drive source is chosen in "
                                 "Setup > Transmit > Power.");
    const QString twoToneSetup = tr("The two-tone test's drive source is "
                                    "chosen in Setup > Test > Two-Tone IMD.");
    if (tx.isTune()) {
        rfOn = tuneSource == DrivePowerSource::DriveSlider;
        tuneOn = tuneSource == DrivePowerSource::TuneSlider;
        rfReason = tr("TUNE is not using this slider.") + QLatin1Char(' ') + tuneSetup;
        tuneReason = rfReason;
    } else if (tx.isTwoToneActive()) {
        rfOn = twoToneSource == DrivePowerSource::DriveSlider;
        tuneOn = twoToneSource == DrivePowerSource::TuneSlider;
        rfReason = tr("The two-tone test is not using this slider.")
                   + QLatin1Char(' ') + twoToneSetup;
        tuneReason = rfReason;
    }
    // [original inline comment from console.cs:46681]
    // hide it
    // Grayed here, not hidden: neither TUNE nor the two-tone test uses it.
    if (tuneOn && tuneSource != DrivePowerSource::TuneSlider
        && twoToneSource != DrivePowerSource::TuneSlider) {
        tuneOn = false;
        tuneReason = tr("Neither TUNE nor the two-tone test uses this slider.")
                     + QLatin1Char(' ') + tuneSetup;
    }

    // Every copy of the power rows, not just the applet's own.
    for (TxPowerSection* s : sections) {
        setDriveSourceEnabled(s->rfPowerSlider(), rfOn,
                              rfOn ? rfTooltip : rfTooltip + QStringLiteral("\n\n") + rfReason,
                              rfReason);
        setDriveSourceEnabled(s->tunePowerSlider(), tuneOn,
                              tuneOn ? tuneTooltip
                                     : tuneTooltip + QStringLiteral("\n\n") + tuneReason,
                              tuneReason);
    }
}

void TxApplet::updatePowerSliderLabels()
{
    for (TxPowerSection* s : allPowerSections()) {
        s->updatePowerSliderLabels();
    }
}

// Canonical TX band — derived from the active slice's frequency (which
// is what RadioModel.cpp:903-905 uses to compose the TX wire byte).
// Falls back to m_currentBand when:
//   - m_model is null (early bootstrap — TxApplet exists before
//     RadioModel pointer wired up; see TxApplet ctor parent),
//   - activeSlice() is null (no slice yet — first launch before
//     addSlice fires, or post-disconnect cleanup state).
// In both fallback cases m_currentBand is the best information we have
// and is what the pre-fix code already used.
Band TxApplet::txBand() const
{
    if (!m_model) { return m_currentBand; }
    SliceModel* slice = transmitSlice();
    if (!slice) { return m_currentBand; }
    // Slice control plan Task 11 fix: the band followTransmitSlice holds
    // (Thetis's _tx_band), which a retune under MOX does not change.
    if (m_txBandKnown && slice == m_followedTxSlice.data()) { return m_txBand; }
    return bandFromFrequency(slice->frequency());
}

SliceModel* TxApplet::transmitSlice() const
{
    if (m_transmitSliceResolver) { return m_transmitSliceResolver(); }
    if (!m_model) { return nullptr; }
    if (TxSliceArbiter* arbiter = m_model->txSliceArbiter()) {
        if (SliceModel* bound = arbiter->txBoundSlice()) { return bound; }
    }
    // A remote window's flag is mirrored from the Core onto the slice.
    for (SliceModel* slice : m_model->slices()) {
        if (slice && slice->isTxSlice()) { return slice; }
    }
    return m_model->activeSlice();
}

void TxApplet::setCurrentBand(Band band)
{
    // No same-band early-return: the bootstrap call from
    // MainWindow.cpp:1578 (`txApplet->setCurrentBand(pan0->band())`) fires
    // with band == m_currentBand-default (Band20m) when the panadapter
    // also opens on 20m, and we still need that call to push the loaded
    // per-band slider values into the UI on first paint.  Re-running the
    // sync on identical input is idempotent — QSlider::setValue same-value
    // is a no-op, and TransmitModel::setPower / setTunePowerForBand have
    // their own same-value early-returns — so the cost is negligible and
    // the win is correct first-paint behaviour after loadFromSettings.
    m_currentBand = band;

    if (!m_model) { return; }

    // Update the Tune Power slider to reflect the per-band stored value.
    {
        const int tunePwr = shownTunePower(band);
        m_updatingFromModel = true;
        for (TxPowerSection* s : allPowerSections()) {
            QSignalBlocker b(s->tunePowerSlider());
            s->tunePowerSlider()->setValue(tunePwr);
        }
        // Issue #175 Task 7: HL2 dB / non-HL2 integer — replaces the
        // bare m_powerSection->tunePowerLabel()->setText(QString::number(tunePwr)) so HL2
        // operators see the dB-attenuator label rather than a raw 0..99
        // slider integer.
        updatePowerSliderLabels();
        m_updatingFromModel = false;
    }

    // The RF Power slider is not recalled here. RadioModel loads the
    // transmit band's stored power into PWR on a transmit band change and
    // at connect (applyTransmitBand, the Thetis TXBand setter port at
    // console.cs:17511-17545 [v2.10.3.15]), and powerChanged paints the
    // slider. A second recall here would load the band twice, and on a
    // panadapter-only band change (CTUN) would load the wrong band.
}

// ── Phase 3M-1b K.2: tooltipForMode ──────────────────────────────────────────
//
// Returns a MOX button tooltip string matching the active DSP mode.
// For modes deferred to a later phase, the tooltip explains why MOX won't
// engage, matching the reason string emitted by moxRejected (K.2) and the
// rejection reason from BandPlanGuard::checkMoxAllowed (K.1).
//
// Mode categories:
//   Allowed (LSB/USB/DIGL/DIGU): normal "Manual transmit (MOX)" tooltip.
//   CW (CWL/CWU):                CW TX deferred to Phase 3M-2.
//   FM, DRM:                     not yet transmit modes (FM waits on 3M-3b).
//   SPEC:                        Never a TX mode.
//
// This helper is static so TxApplet tests can call it directly without
// constructing a full TxApplet instance.
// ---------------------------------------------------------------------------
// static
QString TxApplet::tooltipForMode(DSPMode mode)
{
    switch (mode) {
    case DSPMode::LSB:
    case DSPMode::USB:
    case DSPMode::DIGL:
    case DSPMode::DIGU:
    case DSPMode::AM:
    case DSPMode::SAM:
    case DSPMode::DSB:
        return QStringLiteral("Manual transmit (MOX)");

    // R-R3-17 / R-R3-21: the same user words BandPlanGuard refuses with.
    // CW transmit is Phase 3M-2; FM transmit waits on pre-emphasis (Phase
    // 3M-3b). DRM names DRM.
    case DSPMode::CWL:
    case DSPMode::CWU:
        return QStringLiteral("CW transmit is not available on this Core");

    case DSPMode::FM:
        return QStringLiteral("FM transmit is not available on this Core");

    case DSPMode::DRM:
        return QStringLiteral("DRM transmit is not available on this Core");

    case DSPMode::SPEC:
    default:
        return QStringLiteral("This mode cannot transmit.");
    }
}

// ---------------------------------------------------------------------------
// onMoxModeChanged — update MOX button tooltip when DSP mode changes.
//
// Wired to SliceModel::dspModeChanged (via RadioModel active-slice accessor)
// in wireControls(). Calls tooltipForMode(mode) to get the appropriate
// tooltip text and installs it on m_keyingSection->moxButton(). If the mode is an allowed SSB
// mode the tooltip reverts to the normal "Manual transmit (MOX)".
// ---------------------------------------------------------------------------
// Wires the active slice's dspModeChanged to onMoxModeChanged, dropping the
// previous slice's connection, and sets the tooltip from its mode.
// Slice control plan Task 11 (Q15): the TX band follows the transmit slice's
// frequency, as Thetis sets TXBand from the transmit VFO:
// From Thetis console.cs:35753 [v2.10.3.15]
//     TXBand = BandByFreq(VFOBFreq, tx_xvtr_index, current_region);
// and its setter recalls the band's power and tune power:
// From Thetis console.cs:17542 [v2.10.3.15]
//     // initialisting, becase it is irrelevent, old_band will = value at this point MW0LGE
//     ptbTune.LimitValue = limitTunePower_by_band[(int)value]; //MW0LGE_22b
//     PWR = power_by_band[(int)value];
//     TunePWR = tunePower_by_band[(int)value]; //MW0LGE_22b
// setCurrentBand is that recall (tune power by m_currentBand, RF power by
// txBand()). Slice control plan Task 11 fix: the setter's MOX gate is
// ported to the retune path. While this window's model says the radio
// transmits (MOX, TUNE or two-tone; a remote window's is the Core's), a
// retune of the transmit slice leaves m_txBand and the recall alone:
// From Thetis console.cs:17517-17518 [v2.10.3.15]
//     //[2.10.3.6]MW0LGE no band change on TX fix
//     if (MOX) return;
// From Thetis console.cs:6512-6513 [v2.10.3.15]
//     //[2.10.3.6]MW0LGE no band change on TX fix
//     if (MOX) return;
// Nothing re-evaluates on the unkey; the next retune carries the band, as
// in Thetis. A move of the binding is not gated: a keyed move unkeys
// first (ruling 8.10).
void TxApplet::followTransmitSlice()
{
    SliceModel* slice = transmitSlice();
    const bool moved = slice != m_followedTxSlice.data();
    disconnect(m_moxModeConnection);
    m_moxModeConnection = {};
    disconnect(m_txFreqConnection);
    m_txFreqConnection = {};
    m_followedTxSlice = slice;
    refreshTxFilterStatus();
    if (!slice) {
        m_txBandKnown = false;
        return;
    }
    if (moved || !m_txBandKnown) {
        m_txBand = bandFromFrequency(slice->frequency());
        m_txBandKnown = true;
    }
    m_moxModeConnection = connect(slice, &SliceModel::dspModeChanged,
                                  this, [this](DSPMode mode) {
        onMoxModeChanged(mode);
        refreshTxFilterStatus();
    });
    m_txFreqConnection = connect(slice, &SliceModel::frequencyChanged,
                                 this, [this](double hz) {
        if (m_model && m_model->isTransmitting()) { return; }
        const Band band = bandFromFrequency(hz);
        m_txBand = band;
        m_txBandKnown = true;
        if (band != m_currentBand) { setCurrentBand(band); }
    });
    onMoxModeChanged(slice->dspMode());
    if (moved) {
        setCurrentBand(bandFromFrequency(slice->frequency()));
    }
}

void TxApplet::watchTransmitFlags()
{
    if (!m_model) { return; }
    for (SliceModel* slice : m_model->slices()) {
        if (slice) {
            connect(slice, &SliceModel::txSliceChanged, this,
                    &TxApplet::onSliceTransmitFlagChanged, Qt::UniqueConnection);
        }
    }
}

void TxApplet::onSliceTransmitFlagChanged(bool)
{
    followTransmitSlice();
    refreshTransmitSliceChoices();
}

void TxApplet::refreshTxFilterStatus()
{
    if (!m_model) { return; }
    SliceModel* slice = transmitSlice();
    const DSPMode mode = slice ? slice->dspMode() : DSPMode::USB;
    const QString text = m_model->transmitModel().filterDisplayText(mode);
    for (TxFilterSection* s : allFilterSections()) {
        if (s->filterStatusLabel()) {
            s->filterStatusLabel()->setText(text);
        }
    }
}

void TxApplet::onMoxModeChanged(DSPMode mode)
{
    // Task 16: the receive-only lock sits on top of the tooltip; take it
    // off, change the tooltip under it, and put it back.
    removeReceiveOnlyLock();
    for (TxKeyingSection* s : allKeyingSections()) {
        if (!s->moxButton()) { continue; }
        // Task 16 fix wave 2 (Minor 2): while the transmit-permission layer
        // holds the button, its reason stays visible and the mode's tooltip
        // goes into the tooltip that layer gives back.
        if (s->moxButton()->property(kTransmitSavedTooltip).isValid()) {
            s->moxButton()->setProperty(kTransmitSavedTooltip, tooltipForMode(mode));
        } else {
            s->moxButton()->setToolTip(tooltipForMode(mode));
        }
    }
    applyReceiveOnlyLock();
}

// ---------------------------------------------------------------------------
// Task 16: receive only.
//
// From Thetis console.cs:15318-15324 [v2.10.3.15] (RXOnly setter):
//   if (_rx1_dsp_mode != DSPMode.SPEC &&
//       _rx1_dsp_mode != DSPMode.DRM &&
//       chkPower.Checked)
//       chkMOX.Enabled = !_rx_only;
//   chkTUN.Enabled = !_rx_only;
//   chk2TONE.Enabled = !_rx_only; // MW0LGE_21a
//   chkVOX.Enabled = !_rx_only;
// Disabled, with the reason as the tooltip (the operator, 2026-09-25: a
// control that cannot run is shown disabled with its reason). The keying
// gate refuses every key whatever the buttons show (MoxController::setRxOnly).
// MOX is disabled in every mode, SPEC and DRM included, where Thetis leaves
// it alone (RadioModel::receiveOnlyDisablesMoxButton says why; fix wave I3).
// Under a remote window's transmit gate both reasons show (M6).
// ---------------------------------------------------------------------------
namespace {
constexpr auto kRxOnlySavedTooltip = "TxAppletRxOnlySavedTooltip";
constexpr auto kRxOnlySavedDescription = "TxAppletRxOnlySavedDescription";
constexpr auto kRxOnlySavedEnabled = "TxAppletRxOnlySavedEnabled";
}

void TxApplet::removeReceiveOnlyLock()
{
    for (QWidget* control : rxOnlyLockedControls()) {
        if (!control || !control->property(kRxOnlySavedTooltip).isValid()) {
            continue;
        }
        control->setEnabled(control->property(kRxOnlySavedEnabled).toBool());
        control->setToolTip(control->property(kRxOnlySavedTooltip).toString());
        control->setAccessibleDescription(
            control->property(kRxOnlySavedDescription).toString());
        control->setProperty(kRxOnlySavedTooltip, QVariant());
        control->setProperty(kRxOnlySavedDescription, QVariant());
        control->setProperty(kRxOnlySavedEnabled, QVariant());
    }
}

void TxApplet::applyReceiveOnlyLock()
{
    // HL2 port part 2: receive only or a TX inhibit (transmitButtonsLocked).
    if (!m_model || !m_model->transmitButtonsLocked()) {
        return;
    }
    const QString reason = m_model->transmitLockReasonAlongside(
        m_transmitPermitted ? QString() : m_transmitPermissionReason);
    const auto lock = [&reason](QWidget* control) {
        if (!control || control->property(kRxOnlySavedTooltip).isValid()) {
            return;
        }
        control->setProperty(kRxOnlySavedTooltip, control->toolTip());
        control->setProperty(kRxOnlySavedDescription, control->accessibleDescription());
        control->setProperty(kRxOnlySavedEnabled, control->isEnabled());
        control->setEnabled(false);
        control->setToolTip(reason);
        control->setAccessibleDescription(reason);
    };
    for (TxKeyingSection* s : allKeyingSections()) {
        if (m_model->transmitLockCoversMox()) {
            lock(s->moxButton());
        }
        lock(s->tuneButton());
    }
    for (TxProcessingSection* s : allProcessingSections()) {
        lock(s->twoToneButton());   // MW0LGE_21a
    }
    // TX safety (2026-09-30): a lost radio link leaves VOX alone, as
    // Thetis's power-off does (console.cs:27488-27493 [v2.10.3.15]).
    if (m_model->transmitLockCoversVox()) {
        for (TxVoxSection* s : allVoxSections()) {
            lock(s->button());
        }
    }
}

// The processing rows' widgets, now owned by TxProcessingSection.
QPushButton* TxApplet::twoToneButton() const
{
    return m_processingSection ? m_processingSection->twoToneButton() : nullptr;
}

// The TX chain rows' widgets, now owned by TxChainSection.
QComboBox* TxApplet::profileCombo() const
{
    return m_chainSection ? m_chainSection->profileCombo() : nullptr;
}

// The keying rows' widgets, now owned by TxKeyingSection.
QPushButton* TxApplet::moxButton() const
{
    return m_keyingSection ? m_keyingSection->moxButton() : nullptr;
}

QPushButton* TxApplet::tuneButton() const
{
    return m_keyingSection ? m_keyingSection->tuneButton() : nullptr;
}

QPushButton* TxApplet::takeTransmitButton() const
{
    return m_keyingSection ? m_keyingSection->takeTransmitButton() : nullptr;
}

QList<QPushButton*> TxApplet::transmitSliceButtons() const
{
    return m_keyingSection ? m_keyingSection->transmitSliceButtons() : QList<QPushButton*>();
}

// The power rows' widgets, now owned by TxPowerSection.
BarInstrument* TxApplet::fwdPowerGauge() const
{
    return m_powerSection ? m_powerSection->fwdPowerGauge() : nullptr;
}

BarInstrument* TxApplet::swrGauge() const
{
    return m_powerSection ? m_powerSection->swrGauge() : nullptr;
}

QSlider* TxApplet::rfPowerSlider() const noexcept
{
    return m_powerSection ? m_powerSection->rfPowerSlider() : nullptr;
}

QSlider* TxApplet::tunePowerSlider() const noexcept
{
    return m_powerSection ? m_powerSection->tunePowerSlider() : nullptr;
}

QLabel* TxApplet::rfPowerLabel() const noexcept
{
    return m_powerSection ? m_powerSection->rfPowerLabel() : nullptr;
}

QLabel* TxApplet::tunePowerLabel() const noexcept
{
    return m_powerSection ? m_powerSection->tunePowerLabel() : nullptr;
}

// The VOX row's widgets, now owned by TxVoxSection. The applet keeps the
// model wiring, the 100 ms peak poller and the permission gates; these
// reach the section's widgets for it.
QPushButton* TxApplet::voxButton() const
{
    return m_voxSection ? m_voxSection->button() : nullptr;
}

QSlider* TxApplet::voxThresholdSlider() const
{
    return m_voxSection ? m_voxSection->thresholdSlider() : nullptr;
}

QLabel* TxApplet::voxThresholdLabel() const
{
    return m_voxSection ? m_voxSection->thresholdLabel() : nullptr;
}

QSlider* TxApplet::voxHoldSlider() const
{
    return m_voxSection ? m_voxSection->holdSlider() : nullptr;
}

QLabel* TxApplet::voxHoldLabel() const
{
    return m_voxSection ? m_voxSection->holdLabel() : nullptr;
}

// ── pollVoxMeter — Phase 3M-3a-iii bench polish 2026-05-04 ─────────────────
// 100 ms tick that drives the VOX peak meter on the TX right pane.
//
//   VOX peak (linear amplitude from TxChannel::getDexpPeakSignal()):
//     • 20 * log10(linear) → dB.
//     • Map -80..0 dB → 0..1 normalized (range matches Thetis ptbVOX scale
//       per console.Designer.cs:6018-6019 [v2.10.3.13]).
//     • Threshold marker pulled from TransmitModel::voxThresholdDb() and
//       mapped through the same -80..0 → 0..1 transform.
//
// Continuous (NOT MOX-gated) since GetDEXPPeakSignal is the live DEXP
// detector envelope, not the TX-pipeline meter.  Same rationale as the
// PhoneCwApplet::pollDexpMeters comment — see that file for the full
// narrative.  Relocated from PhoneCwApplet as part of the 2026-05-04 bench
// polish (VOX row moved to TxApplet under TUNE/MOX).
void TxApplet::pollVoxMeter()
{
    if (!m_model) { return; }
    TxChannel* ch = m_model->txChannel();
    if (!ch || !m_voxSection) { return; }

    // VOX peak: linear amplitude → dB → 0..1 over -80..0 range.
    const double linearPeak = ch->getDexpPeakSignal();
    const double voxPeakDb  = (linearPeak > 0.0)
        ? 20.0 * std::log10(linearPeak)
        : -80.0;
    const double voxPeak01  = std::clamp((voxPeakDb + 80.0) / 80.0, 0.0, 1.0);
    for (TxVoxSection* s : allVoxSections()) {
        s->setPeakLevel(voxPeak01);
    }

    // VOX threshold marker: voxThresholdDb is in -80..0 dB range.
    const int thDb = m_model->transmitModel().voxThresholdDb();
    for (TxVoxSection* s : allVoxSections()) {
        s->setThresholdMarker(
            std::clamp((thDb + 80.0) / 80.0, 0.0, 1.0));
    }
}

// ---------------------------------------------------------------------------
// Phase 3M-1c J.1 — setMicProfileManager
//
// Inject the per-MAC MicProfileManager.  Wires:
//   - manager.profileListChanged → rebuildProfileCombo (set membership change)
//   - manager.activeProfileChanged → combo selection update (programmatic)
// ---------------------------------------------------------------------------
void TxApplet::setMicProfileManager(MicProfileManager* mgr)
{
    if (m_micProfileMgr == mgr) { return; }

    if (m_micProfileMgr) {
        disconnect(m_micProfileMgr, nullptr, this, nullptr);
    }

    m_micProfileMgr = mgr;

    if (m_micProfileMgr) {
        // List changes → rebuild combo entries.
        connect(m_micProfileMgr, &MicProfileManager::profileListChanged,
                this, &TxApplet::rebuildProfileCombo);
        // Active changes → select the named entry without triggering a
        // setActiveProfile callback (m_updatingFromModel guards that).
        connect(m_micProfileMgr, &MicProfileManager::activeProfileChanged,
                this, [this](const QString& name) {
            m_updatingFromModel = true;
            for (TxChainSection* s : allChainSections()) {
                if (!s->profileCombo()) { continue; }
                QSignalBlocker blk(s->profileCombo());
                const int idx = s->profileCombo()->findText(name);
                if (idx >= 0) {
                    s->profileCombo()->setCurrentIndex(idx);
                }
            }
            m_updatingFromModel = false;
        });
    }

    rebuildProfileCombo();
}

// ---------------------------------------------------------------------------
// Phase 3M-1c J.2 — setTwoToneController
//
// Inject the TwoToneController.  Wires the controller's
// twoToneActiveChanged signal so the button visually mirrors the
// authoritative state (covers the BandPlanGuard rejection clean-up
// from Phase I.5).
// ---------------------------------------------------------------------------
void TxApplet::setDisplayDuplexState(bool checked, bool available, const QString& tooltip)
{
    m_dupChecked = checked;
    m_dupAvailable = available;
    m_dupTooltip = tooltip;
    for (TxProcessingSection* s : allProcessingSections()) {
        const QSignalBlocker block(s->dupButton());
        s->dupButton()->setChecked(checked);
        s->dupButton()->setEnabled(available);
        s->dupButton()->setToolTip(tooltip);
    }
}

void TxApplet::setTwoToneController(TwoToneController* controller)
{
    if (m_twoToneCtrl == controller) { return; }

    if (m_twoToneCtrl) {
        disconnect(m_twoToneCtrl, nullptr, this, nullptr);
    }

    m_twoToneCtrl = controller;

    if (m_twoToneCtrl) {
        connect(m_twoToneCtrl, &TwoToneController::twoToneActiveChanged,
                this, [this](bool active) {
            if (m_model && m_model->role() == RadioModel::Role::Remote) {
                syncRemoteIndicators();
                return;
            }
            for (TxProcessingSection* s : allProcessingSections()) {
                if (!s->twoToneButton()) { continue; }
                QSignalBlocker blk(s->twoToneButton());
                m_updatingFromModel = true;
                s->twoToneButton()->setChecked(active);
                m_updatingFromModel = false;
            }
        });

        // Remote indicators keep the Core facade as their authority.
        if (m_model && m_model->role() == RadioModel::Role::Remote) {
            syncRemoteIndicators();
            return;
        }

        // Sync initial state.
        for (TxProcessingSection* s : allProcessingSections()) {
            QSignalBlocker blk(s->twoToneButton());
            m_updatingFromModel = true;
            s->twoToneButton()->setChecked(m_twoToneCtrl->isActive());
            m_updatingFromModel = false;
        }
    }
}

// ---------------------------------------------------------------------------
// Phase 3M-3a-ii Batch 6 (Task A) — requestOpenCfcDialog
//
// Open (or raise) the modeless TxCfcDialog.  Lazy-creates the dialog on first
// call so the construction cost is only paid when the user actually opens
// CFC settings. The modeless dialog is owned by this retained applet, so
// returning it from a disposable floating container preserves the editor. We
// don't deleteLater() the dialog on close — keep it alive across opens for
// fast re-show, mirroring the TxEqDialog singleton pattern.
//
// Public-slot entry point so external surfaces can reuse the same instance:
//   - [CFC] button right-click → customContextMenuRequested → this slot.
//   - CfcSetupPage's [Configure CFC bands…] button → MainWindow connects its
//     openCfcDialogRequested signal to this slot.
//   - Future Tools menu item → connects to this slot.
// ---------------------------------------------------------------------------
TxCfcDialog* TxApplet::cfcDialog() const
{
    return m_cfcDialog.data();
}

void TxApplet::requestOpenCfcDialog()
{
    // R-R3-49 (parity Task 4): opens in a remote window too, greyed with
    // the reason while the Core cannot take a change.
    if (!m_model) { return; }

    if (!m_cfcDialog) {
        m_cfcDialog = new TxCfcDialog(
            &m_model->transmitModel(),
            m_model->txChannel(),
            this);
        // Setup publication (CFC band editor): a remote window sends the
        // whole table to a Core that takes it, and hears the answer. The
        // link is looked up on every call, so a dialog built before the
        // window reached a Core sends whole once it has.
        {
            QPointer<RadioModel> model(m_model);
            m_cfcDialog->setStationProfileSender(
                [model] {
                    IStationLink* link = model ? model->stationLink() : nullptr;
                    return link && link->transmitSettingsAvailable(
                                       kTransmitSettingsCfcProfileVersion);
                },
                [model](const QString& profileJson, const QString& expectedRevision) {
                    TxCfcDialog::StationProfileSend result;
                    IStationLink* link = model ? model->stationLink() : nullptr;
                    if (!link) {
                        result.reason = IStationLink::transmitSettingsUnavailableReason();
                        return result;
                    }
                    const IStationLink::CommandOutcome outcome =
                        link->requestCfcProfile(profileJson, expectedRevision);
                    result.sent = outcome.sent;
                    result.reason = outcome.reason;
                    result.commandId = outcome.commandId;
                    return result;
                });
            connect(m_model, &RadioModel::stationCommandFinished,
                    m_cfcDialog.data(), &TxCfcDialog::onStationCommandFinished);
            QPointer<TxCfcDialog> dialog(m_cfcDialog);
            connect(m_model, &RadioModel::stationLinkStateChanged, m_cfcDialog.data(),
                    [model, dialog] {
                        if (!dialog) { return; }
                        const IStationLink* link = model ? model->stationLink() : nullptr;
                        dialog->onStationLinkChanged(link && link->stationLinkReady());
                    });
        }
    } else {
        // Connection may have come up since the dialog was created.
        // Refresh the TxChannel pointer so the bar chart timer can poll WDSP.
        m_cfcDialog->setTxChannel(m_model->txChannel());
    }
    m_cfcDialog->setSettingsPermitted(m_txProcessingPermitted, m_txProcessingReason);
    // Parity Task 33: a remote window's chart reads the Core's stream.
    if (m_stationCfcBarChart) {
        m_cfcDialog->setStationBarChart(m_stationCfcBarChart);
        m_cfcDialog->setBarChartUnavailable(m_stationCfcBarChartReason);
    }
    m_cfcDialog->show();
    m_cfcDialog->raise();
    m_cfcDialog->activateWindow();
}

void TxApplet::setStationCfcBarChart(std::function<void(bool)> setWanted)
{
    m_stationCfcBarChart = std::move(setWanted);
    if (m_cfcDialog && m_stationCfcBarChart) {
        m_cfcDialog->setStationBarChart(m_stationCfcBarChart);
    }
}

void TxApplet::applyStationCfcCompression(const QList<double>& binsDb)
{
    if (m_cfcDialog) {
        m_cfcDialog->applyStationCompression(binsDb);
    }
}

void TxApplet::setStationCfcBarChartUnavailable(const QString& reason)
{
    m_stationCfcBarChartReason = reason;
    if (m_cfcDialog) {
        m_cfcDialog->setBarChartUnavailable(reason);
    }
}

void TxApplet::setTxProcessingPermitted(bool permitted, const QString& unavailableReason)
{
    m_txProcessingPermitted = permitted;
    m_txProcessingReason = permitted
        ? QString()
        : (unavailableReason.isEmpty() ? IStationLink::transmitSettingsUnavailableReason()
                                       : unavailableReason);
    if (m_cfcDialog) {
        m_cfcDialog->setSettingsPermitted(m_txProcessingPermitted, m_txProcessingReason);
    }
}

// ---------------------------------------------------------------------------
// Remote-station transmit-permission presentation
//
// The Core remains the authority for transmit refusal and unwind. This gate
// exists so a remote operator never receives a live-looking TX control before
// the completed handshake explicitly grants that capability. Do not clear or
// write any model state here: model-to-view updates must remain authoritative.
// ---------------------------------------------------------------------------
void TxApplet::setTransmitPermitted(bool permitted, const QString& unavailableReason)
{
    // Task 16: the receive-only lock goes back on top afterwards.
    removeReceiveOnlyLock();
    m_transmitPermitted = permitted;
    const QString reason = unavailableReason.isEmpty()
        ? tr("Transmit controls are unavailable until the Core confirms "
             "transmit permission.")
        : unavailableReason;
    m_transmitPermissionReason = reason;   // Task 16 fix wave (M6)

    const auto apply = [permitted, &reason](QWidget* control) {
        gateTransmitControl(control, permitted, reason);
    };

    // R-R3-49 (parity Task 1): RF Power and the TX filter low and high
    // follow setTransmitSettingsPermitted; parity Task 2: Tune Power, the
    // VOX level and delay, MON, LEV, EQ and CFC follow
    // setTransmitChainSettingsPermitted; parity Task 3: the profile combo
    // follows setTxProfilePermitted; parity Task 7: PS-A follows
    // setPureSignalArmingPermitted. This gate keeps the rest.
    for (TxKeyingSection* s : allKeyingSections()) {
        apply(s->tuneButton());
        apply(s->moxButton());
    }
    // Fix wave 2 (M8): VOX also needs this computer's microphone line.
    for (TxVoxSection* s : allVoxSections()) {
        gateTransmitControl(s->button(), permitted && m_voxPermitted,
                            permitted ? m_voxReason : reason);
    }
    for (TxProcessingSection* s : allProcessingSections()) {
        apply(s->twoToneButton());
    }
    // Task 16: the receive-only lock back on top (checkpoint join).
    applyReceiveOnlyLock();
}

void TxApplet::setVoxPermitted(bool permitted, const QString& reason)
{
    removeReceiveOnlyLock();
    m_voxPermitted = permitted;
    m_voxReason = reason;
    for (TxVoxSection* s : allVoxSections()) {
        gateTransmitControl(s->button(), m_transmitPermitted && permitted,
                            m_transmitPermitted ? reason : m_transmitPermissionReason);
    }
    applyReceiveOnlyLock();
}

void TxApplet::setPureSignalArmingPermitted(bool permitted, const QString& unavailableReason)
{
    m_psArmingPermitted = permitted;
    const QString reason = unavailableReason.isEmpty()
        ? tr("Remote transmit controls are not available from this Core.")
        : unavailableReason;
    m_psArmingReason = reason;
    // Fix wave GUI-I7: the facade's reason off first, so this gate keeps
    // the button's own tooltip to put back.
    removePsaFacadeReason();
    for (TxProcessingSection* s : allProcessingSections()) {
        gateTransmitControl(s->psaButton(), permitted, reason);
    }
    syncPsaFromFacade();
    applyReceiveOnlyLock();
}

// R-R3-49 (parity Task 1): the transmit settings that key nothing. In a
// remote window they are live while the Core takes them and its radio is
// off the air; the Core refuses a change that races a key anyway.
void TxApplet::setTransmitSettingsPermitted(bool permitted, const QString& unavailableReason)
{
    m_transmitSettingsPermitted = permitted;
    const QString reason = unavailableReason.isEmpty()
        ? IStationLink::transmitSettingsUnavailableReason()
        : unavailableReason;
    m_transmitSettingsReason = reason;
    for (QWidget* control : transmitSettingsGatedControls()) {
        gateTransmitControl(control, permitted, reason);
    }
}

// R-R3-49 (parity Task 2): the rest of this applet's transmit settings. The
// Core takes them off the air (transmitSettingsVersion 2) and shows its
// values back; the MON output pair is this computer's own routing.
void TxApplet::setTransmitChainSettingsPermitted(bool permitted,
                                                 const QString& unavailableReason)
{
    m_transmitChainSettingsPermitted = permitted;
    const QString reason = unavailableReason.isEmpty()
        ? IStationLink::transmitSettingsUnavailableReason()
        : unavailableReason;
    m_transmitChainSettingsReason = reason;
    for (QWidget* control : chainSettingsGatedControls()) {
        gateTransmitControl(control, permitted, reason);
    }
    for (QWidget* control : monitorChainControls()) {
        gateTransmitControl(control, permitted, reason);
    }
    // Parity Task 32: the MON output pair also needs a Core that sends MON.
    applyMonitorOutputGate();
}

// Remote-window parity Task 32 (R-IOS-13, R-R3-49): where MON plays is this
// computer's choice, sent to the Core, which carries MON in this window's
// own audio while it holds transmit. A Core that does not send MON has
// nothing to route, so the pair is shown disabled with the reason; MON
// itself still turns the Core's monitor on.
void TxApplet::setMonitorOutputPermitted(bool permitted, const QString& unavailableReason)
{
    m_monitorOutputPermitted = permitted;
    m_monitorOutputReason = unavailableReason.isEmpty() ? monitorOutputUnavailableReason()
                                                        : unavailableReason;
    applyMonitorOutputGate();
}

QString TxApplet::monitorOutputUnavailableReason()
{
    return TxMonitorSection::outputUnavailableReason();
}

void TxApplet::applyMonitorOutputGate()
{
    const bool permitted = m_transmitChainSettingsPermitted && m_monitorOutputPermitted;
    const QString reason = !m_transmitChainSettingsPermitted
        ? (m_transmitChainSettingsReason.isEmpty()
               ? IStationLink::transmitSettingsUnavailableReason()
               : m_transmitChainSettingsReason)
        : m_monitorOutputReason;
    for (QWidget* control : monitorOutputControls()) {
        gateTransmitControl(control, permitted, reason);
    }
}

// R-R3-49 (parity Task 3): the profile combo. The Core applies a pick and
// reports its active profile back; a refused pick shows the Core's again.
void TxApplet::setTxProfilePermitted(bool permitted, const QString& unavailableReason)
{
    m_txProfilePermitted = permitted;
    m_txProfileReason = unavailableReason.isEmpty()
        ? IStationLink::transmitSettingsUnavailableReason()
        : unavailableReason;
    for (QWidget* control : profileGatedControls()) {
        gateTransmitControl(control, permitted, m_txProfileReason);
    }
}

void TxApplet::setTakeTransmitOffered(bool offered, bool holderOnAir)
{
    m_takeTransmitOffered = offered;
    m_holderOnAir = holderOnAir;
    for (TxKeyingSection* s : allKeyingSections()) {
        if (!s->takeTransmitButton()) { continue; }
        s->takeTransmitButton()->setVisible(offered && !s->sliceIndicatorsHidden());
        s->takeTransmitButton()->setProperty("holderOnAir", offered && holderOnAir);
        s->takeTransmitButton()->setStyleSheet(offered && holderOnAir
            ? QStringLiteral("QPushButton { color: #ff8080; border: 1px solid #ff4444;"
                             " border-radius: 3px; padding: 2px 8px; }")
            : QString());
    }
}

bool TxApplet::takeTransmitOffered() const
{
    return m_keyingSection->takeTransmitButton() && !m_keyingSection->takeTransmitButton()->isHidden();
}

void TxApplet::setTransmitHolderText(const QString& text)
{
    m_transmitHolderText = text;
    for (TxKeyingSection* s : allKeyingSections()) {
        if (!s->holderLabel()) { continue; }
        s->holderLabel()->setText(text);
        s->holderLabel()->setVisible(!text.isEmpty() && !s->sliceIndicatorsHidden());
    }
}

QString TxApplet::transmitHolderText() const
{
    return m_keyingSection->holderLabel() ? m_keyingSection->holderLabel()->text() : QString();
}

bool TxApplet::remoteTunePower() const
{
    return m_model && m_model->role() == RadioModel::Role::Remote;
}

int TxApplet::shownTunePower(Band band) const
{
    if (!m_model) { return 0; }
    const TransmitModel& tx = m_model->transmitModel();
    // The transmit band's tune power once it is known (PA on-air gate
    // review; Thetis shows TunePWR, the transmit band's).
    return (remoteTunePower() || tx.tuneTxBandKnown()) ? tx.tunePowerForTxBand()
                                                       : tx.tunePowerForBand(band);
}

void TxApplet::requestRemoteTunePower(int watts)
{
    if (!m_model) { return; }
    TransmitModel& tx = m_model->transmitModel();
    IStationLink* link = m_model->stationLink();
    if (!m_transmitChainSettingsPermitted || !link
        || !link->requestTunePowerForTxBand(watts).sent) {
        // Not asked: the slider shows the Core's value again.
        tx.reportTunePowerForTxBandRefused();
    }
}

// ---------------------------------------------------------------------------
// Phase 3M-4 Task 13 — setBoardCapabilities
//
// Hide / show the [PS-A] button based on caps.hasPureSignal.  Called by
// MainWindow on RadioModel::currentRadioChanged (and once on startup).
// Mirrors the RxApplet::setBoardCapabilities pattern.  No-op when m_processingSection->psaButton()
// is null (e.g. during early construction or after teardown).
// ---------------------------------------------------------------------------
void TxApplet::setBoardCapabilities(const NereusSDR::BoardCapabilities& caps)
{
    if (!m_processingSection->psaButton()) { return; }
    // Fix round 1 (minor 5): with no radio the caps fall back to Unknown.
    // PS-A shows, disabled with its reason, until the board is known; only
    // a known board without PureSignal hides it.
    const bool boardUnknown = caps.board == HPSDRHW::Unknown;
    m_psBoardUnknown = boardUnknown && !caps.hasPureSignal;
    for (TxProcessingSection* s : allProcessingSections()) {
        s->psaButton()->setVisible(caps.hasPureSignal || boardUnknown);
    }
    syncPsaFromFacade();
}

// ---------------------------------------------------------------------------
// Phase 3M-4 Task 13 — setPureSignal (late-bound coordinator)
//
// Retain the established test/late-bind slot while keeping one facade per
// RadioModel. The facade owns coordinator signal wiring and session state.
// ---------------------------------------------------------------------------
void TxApplet::setPureSignal(NereusSDR::PureSignal* coordinator)
{
    if (m_psFacade) {
        m_psFacade->setCoordinator(coordinator);
    }
    syncPsaFromFacade();
}

void TxApplet::syncPsaFromFacade()
{
    // Every copy of the processing rows, not just the applet's own.
    for (TxProcessingSection* s : allProcessingSections()) {
        syncPsaFromFacadeIn(s);
    }
}

void TxApplet::syncPsaFromFacadeIn(TxProcessingSection* s)
{
    if (!s || !s->psaButton()) {
        return;
    }
    const bool automaticIntent = m_psFacade && m_psFacade->settings()
        ? m_psFacade->settings()->autoCalEnabled() : false;
    const QSignalBlocker blocker(s->psaButton());
    m_updatingFromModel = true;
    s->psaButton()->setChecked(automaticIntent);
    // R-R3-49 (parity Task 7): arming keys nothing, so canArm.
    const bool canArm = !m_psBoardUnknown && m_psFacade && m_psFacade->available()
        && m_psFacade->canArm();
    // Fix wave GUI-I7: greyed by the facade, with the facade's reason. The
    // arming gate's reason, when it is on, stays (its tooltip is set).
    removePsaFacadeReasonIn(s);
    if (m_psArmingPermitted && !canArm) {
        // Fix round 1 (minor 5): a board not known says it needs one.
        const QString refusal = m_psBoardUnknown || !m_psFacade ? QString()
                                                               : m_psFacade->armingRefusal();
        const QString reason = refusal.isEmpty()
            ? PureSignalSessionFacade::needsRadioReason() : refusal;
        s->psaButton()->setProperty(kPsaFacadeSavedTooltip, s->psaButton()->toolTip());
        s->psaButton()->setProperty(kPsaFacadeSavedDescription, s->psaButton()->accessibleDescription());
        s->psaButton()->setToolTip(reason);
        s->psaButton()->setAccessibleDescription(reason);
    }
    s->psaButton()->setEnabled(m_psArmingPermitted && canArm);
    m_updatingFromModel = false;
}

void TxApplet::removePsaFacadeReason()
{
    // Every copy of the processing rows, not just the applet's own.
    for (TxProcessingSection* s : allProcessingSections()) {
        removePsaFacadeReasonIn(s);
    }
}

void TxApplet::removePsaFacadeReasonIn(TxProcessingSection* s)
{
    if (!s || !s->psaButton() || !s->psaButton()->property(kPsaFacadeSavedTooltip).isValid()) {
        return;
    }
    s->psaButton()->setToolTip(s->psaButton()->property(kPsaFacadeSavedTooltip).toString());
    s->psaButton()->setAccessibleDescription(
        s->psaButton()->property(kPsaFacadeSavedDescription).toString());
    s->psaButton()->setProperty(kPsaFacadeSavedTooltip, QVariant());
    s->psaButton()->setProperty(kPsaFacadeSavedDescription, QVariant());
}

// ---------------------------------------------------------------------------
// Phase 3M-1c J.1 — rebuildProfileCombo
//
// Rebuild combo entries from m_micProfileMgr->profileNames().  Preserves
// the active-profile selection where possible; otherwise the manager's
// activeProfileName() is selected.  No-op when manager is null.
// ---------------------------------------------------------------------------
void TxApplet::rebuildProfileCombo()
{
    if (!m_micProfileMgr) {
        // No manager → leave the placeholder "Default" item alone.
        return;
    }

    const QStringList names = m_micProfileMgr->profileNames();
    const QString active = m_micProfileMgr->activeProfileName();

    m_updatingFromModel = true;
    for (TxChainSection* s : allChainSections()) {
        if (!s->profileCombo()) { continue; }
        QSignalBlocker blk(s->profileCombo());
        resetItemsBeforeRowChange(s->profileCombo());
        s->profileCombo()->clear();
        s->profileCombo()->addItems(names);

        const int idx = s->profileCombo()->findText(active);
        if (idx >= 0) {
            s->profileCombo()->setCurrentIndex(idx);
        }
    }

    m_updatingFromModel = false;
}

// ---------------------------------------------------------------------------
// The MON row's wiring, for any instance of it.
//
// The applet owns the MON row, and a container may hold an independent copy
// of it (createMonitorSection). Both are wired here, so both show and set
// the same model state, and both follow the same permission gates.
// ---------------------------------------------------------------------------
void TxApplet::wireMonitorSection(TxMonitorSection* s)
{
    if (!s || !m_model) {
        return;
    }
    TransmitModel& tx = m_model->transmitModel();
    // ── MON row (TxMonitorSection) ↔ TransmitModel / AudioEngine ─────────────
    // Phase 3M-1b J.3.
    // UI → Model: toggled → setMonEnabled (with m_updatingFromModel guard).
    // Model → UI: monEnabledChanged → update checked state with QSignalBlocker.
    auto* monBtn = s->monButton();
    auto* monSpeakersBtn = s->speakersButton();
    auto* monHeadphonesBtn = s->headphonesButton();
    auto* monitorVolumeSlider = s->volumeSlider();
    auto* monitorVolumeValue = s->volumeValue();

    connectRow(s, monBtn, &QPushButton::toggled, [this, &tx](bool on) {
        if (m_updatingFromModel) { return; }
        tx.setMonEnabled(on);
    });

    connectRow(s, &tx, &TransmitModel::monEnabledChanged, [this, monBtn](bool on) {
        QSignalBlocker b(monBtn);
        m_updatingFromModel = true;
        monBtn->setChecked(on);
        m_updatingFromModel = false;
    });

    // ── R-R3-45: MON output ↔ AudioEngine::txMonitorOutput ──────────────────
    // This computer's choice: the audio engine that plays this window's
    // sound. R-R3-49 (parity Task 2): a remote window too, where it is this
    // computer's own routing (a window-scope setting), as in a local window;
    // it follows setTransmitChainSettingsPermitted there.
    {
        if (AudioEngine* engine = m_model->localAudioDevices()) {
            connectRow(s, monSpeakersBtn, &QPushButton::clicked,
                    [engine, s](bool) {
                s->showMonitorOutput(false);
                engine->setTxMonitorOutput(TxMonitorOutput::Speakers);
            });
            connectRow(s, monHeadphonesBtn, &QPushButton::clicked,
                    [engine, s](bool) {
                s->showMonitorOutput(true);
                engine->setTxMonitorOutput(TxMonitorOutput::Headphones);
            });
            connectRow(s, engine, &AudioEngine::txMonitorOutputChanged,
                    [s](TxMonitorOutput output) {
                s->showMonitorOutput(output == TxMonitorOutput::Headphones);
            });
            connectRow(s, engine, &AudioEngine::headphonesAvailableChanged,
                    [s](bool available) {
                s->setHeadphonesAvailable(available);
            });
            connectRow(s, engine, &AudioEngine::headphonesEnabledChanged,
                    [s](bool enabled) {
                s->setHeadphonesEnabled(enabled);
            });
            s->setHeadphonesAvailable(engine->headphonesAvailable());
            s->setHeadphonesEnabled(engine->headphonesEnabled());
            s->showMonitorOutput(engine->txMonitorOutput() == TxMonitorOutput::Headphones);
        }
    }

    // ── Monitor volume slider ↔ TransmitModel::monitorVolume ─────────────────
    // Phase 3M-1b J.3.
    // UI → Model: slider valueChanged(int) → setMonitorVolume(value / 100.0f).
    // Model → UI: monitorVolumeChanged(float) → slider = qRound(v * 100.0f).
    connectRow(s, monitorVolumeSlider, &QSlider::valueChanged,
            [this, &tx, monitorVolumeValue](int val) {
        if (m_updatingFromModel) { return; }
        monitorVolumeValue->setText(QString::number(val));
        tx.setMonitorVolume(static_cast<float>(val) / 100.0f);
    });

    connectRow(s, &tx, &TransmitModel::monitorVolumeChanged,
            [this, monitorVolumeSlider, monitorVolumeValue](float volume) {
        QSignalBlocker b(monitorVolumeSlider);
        m_updatingFromModel = true;
        const int uiVal = qRound(volume * 100.0f);
        monitorVolumeSlider->setValue(uiVal);
        monitorVolumeValue->setText(QString::number(uiVal));
        m_updatingFromModel = false;
    });
}

// The gap the applet's own column puts between its rows, so an independent
// copy of a row keeps the same spacing.
int TxApplet::txRowSpacing() const
{
    return WorkspacePreview::enabled() ? kWorkspaceTxRowSpacing : 2;
}

QMargins TxApplet::txRowMargins() const
{
    return QMargins();
}

QMargins TxApplet::txCopyMargins() const
{
    return QMargins(kTxColumnMarginH, kTxColumnMarginV, kTxColumnMarginH, kTxColumnMarginV);
}

// An independent copy of the MON row, for a container that holds the monitor
// controls on their own. The applet wires it and keeps it, so the
// transmit-permission gates reach it like they reach the applet's own.
TxMonitorSection* TxApplet::createMonitorSection(QWidget* parent, bool preview)
{
    auto* s = new TxMonitorSection(parent, txRowSpacing(), txCopyMargins());
    if (preview) {
        makeRowInert(s);
        return s;
    }
    wireMonitorSection(s);
    m_extraMonitorSections.append(s);
    // The copy belongs to the container, which owns it and may destroy it
    // when the operator closes the container or removes the item.  The
    // applet's timers walk this list, so a copy that is gone must not stay
    // in it.  The QPointer drops it on its own; this keeps the list from
    // growing as copies come and go.
    const QPointer<TxMonitorSection> guard(s);
    connect(s, &QObject::destroyed, this, [this, guard]{
        m_extraMonitorSections.removeAll(guard);
    });
    // The copy starts where the model is, not where the applet's own row was
    // when it was built.
    syncSectionInstances();
    return s;
}

// The controls the transmit-chain gate covers: the MON toggle and the
// monitor volume slider, in the applet's own row and in every copy.
QList<QWidget*> TxApplet::monitorChainControls() const
{
    QList<QWidget*> controls;
    // allMonitorSections skips a copy that is gone.
    for (const TxMonitorSection* s : allMonitorSections()) {
        controls << s->monButton()
                 << s->volumeSlider();
    }
    return controls;
}

// The controls the monitor-output gate covers: the SPEAKERS / PHONES pair,
// in the applet's own row and in every copy.
QList<QWidget*> TxApplet::monitorOutputControls() const
{
    QList<QWidget*> controls;
    // allMonitorSections skips a copy that is gone.
    for (const TxMonitorSection* s : allMonitorSections()) {
        controls << s->speakersButton()
                 << s->headphonesButton();
    }
    return controls;
}

// ---------------------------------------------------------------------------
// The TxPowerSection row's wiring, for any instance of it.
//
// The applet owns the row, and a container may hold an independent copy of
// it (createPowerSection). Both are wired here, so both show and set the same
// model state, and both follow the same permission gates.
// ---------------------------------------------------------------------------
void TxApplet::wirePowerSection(TxPowerSection* s)
{
    if (!s || !m_model) {
        return;
    }
    TransmitModel& tx = m_model->transmitModel();
    MoxController* mox = m_model->moxController();
    // ── Forward-power gauge ← RadioStatus::powerChanged ─────────────────────
    // 3M-1a (2026-04-27): wire the previously-NYI fwd-power gauge to the
    // radio's reported forward power.  Pipeline:
    //   P2RadioConnection → paTelemetryUpdated(fwdRaw,...)
    //     → RadioModel handler scaleFwdPowerWatts() → m_radioStatus.setForwardPower(W)
    //     → RadioStatus::powerChanged(fwd, rev, swr)  ← we listen here
    //
    // Two-stage de-jitter (Phase 3I-1 will replace with the proper Thetis
    // peak-decay filter):
    //   1.  alpha=0.30 EMA on every powerChanged emit — smooths the per-
    //       sample noise without much lag (~10 sample time-constant).
    //   2.  20 Hz QTimer reads the smoothed state into the gauge — keeps
    //       the visible digits stable even when RadioStatus::powerChanged
    //       fires twice per hardware sample (documented in
    //       RadioModel.cpp:572).  Without this throttle the digit
    //       characters update too fast to read.
    connectRow(s, &m_model->radioStatus(), &RadioStatus::powerChanged,
            [this, s](double fwdW, double /*revW*/, double /*swr*/) {
        if (!m_model->mainPowerMeter()->onAmplifier()) {
            constexpr double kAlpha = 0.30;
            s->setSmoothedForwardPowerW(kAlpha * fwdW + (1.0 - kAlpha) * s->smoothedForwardPowerW());
        }
    });
    // One refresh for every power row: refreshPowerReadouts walks the
    // applet's own row and every copy, so a copy starts no timer of its own.
    if (s == m_powerSection) {
        auto* fwdGaugeRefreshTimer = new QTimer(this);
        fwdGaugeRefreshTimer->setInterval(50);   // 20 Hz UI refresh
        connect(fwdGaugeRefreshTimer, &QTimer::timeout,
                this, &TxApplet::refreshPowerReadouts);
        fwdGaugeRefreshTimer->start();
    }

    // The model's power signals reach every power row through one handler
    // each, made with the applet's own row: it resets every row's smoothed
    // reading and then refreshes them all once, rather than once per row.
    if (s == m_powerSection) {
        MainPowerMeter* meter = m_model->mainPowerMeter();
        const auto resetSmoothed = [this](double watts) {
            for (TxPowerSection* row : allPowerSections()) {
                row->setSmoothedForwardPowerW(watts);
            }
        };
        connect(meter, &MainPowerMeter::sourceChanged, this, [this, meter, resetSmoothed] {
            if (!meter->onAmplifier()) {
                resetSmoothed(meter->forwardW());
            }
            refreshPowerReadouts();
        });
        const auto refreshAmplifier = [this, meter] {
            if (meter->onAmplifier()) { refreshPowerReadouts(); }
        };
        connect(meter, &MainPowerMeter::readingChanged, this, refreshAmplifier);
        connect(meter, &MainPowerMeter::driveChanged, this, refreshAmplifier);
        connect(meter, &MainPowerMeter::scaleChanged, this, [this] {
            rescaleFwdGaugeForModel(m_model->hardwareProfile().model);
            refreshPowerReadouts();
        });
        // Bench #167: a lone zero after unkey left the EMA stuck above zero.
        // Keep the explicit reset; the new rows then show their no-reading dashes.
        if (mox) {
            connect(mox, &MoxController::moxStateChanged, this, [this, resetSmoothed](bool active) {
                if (!active) { resetSmoothed(0.0); }
                refreshPowerReadouts();
            });
        }
        // R-R3-49 parity Task 33: the remote controller never keys; the Core's
        // mirrored transmit state clears the rows and drive note instead.
        connect(m_model, &RadioModel::transmittingChanged, this, [this, resetSmoothed](bool active) {
            if (!active) { resetSmoothed(0.0); }
            refreshPowerReadouts();
        });

        // Per-SKU RF Pwr gauge rescale.  Bench-reported #167 follow-up: the
        // 0-120 W default scale made HL2 (5 W) and ANAN-G2-1K (1000 W) both
        // unreadable.  Subscribe to currentRadioChanged so the gauge ticks
        // and red-zone redraw whenever the active radio changes.
        //
        // Issue #175 Task 7: also rescale the RF Power and Tune Power sliders
        // (HL2 → 0..90/6 and 0..99/3 with -X.X dB labels via mi0bot formulae;
        //  every other SKU → canonical 0..100/1 with bare integer labels).
        connect(m_model, &RadioModel::currentRadioChanged, this,
                [this](const NereusSDR::RadioInfo&) {
            const auto model = m_model->hardwareProfile().model;
            rescaleFwdGaugeForModel(model);
            rescalePowerSlidersForModel(model);
        });
    }
    // Apply the current model's scale at wireControls() time so cold-launch
    // (or reconnect-on-startup) lands a properly-sized gauge + sliders before
    // the first telemetry sample.
    {
        const auto model = m_model->hardwareProfile().model;
        rescaleFwdGaugeForModel(model);
        rescalePowerSlidersForModel(model);
    }


    // ── RF Power slider → TransmitModel::setPower(int) ──────────────────────
    // From Thetis chkMOX_CheckedChanged2 power flow [v2.10.3.13]:
    //   the RF Power slider maps 0–100 to TX drive level.
    //
    // Issue #175 Task 7: label text is now driven by updatePowerSliderLabels()
    // which formats the value in -X.X dB on HL2 and bare integer on every
    // other SKU.  The old direct setText(QString::number(val)) would have
    // bypassed the dB conversion on HL2.
    connectRow(s, s->rfPowerSlider(), &QSlider::valueChanged, [this, &tx](int val) {
        updatePowerSliderLabels();
        if (m_updatingFromModel) { return; }
        tx.setPower(val);
        // The per-band slot (Thetis ptbPWR_Scroll, console.cs:28682-28693
        // [v2.10.3.15]: `power_by_band[(int)_tx_band] = ptbPWR.Value;`) has
        // one writer: RadioModel on the radio's side, which saves PWR into
        // its transmit band on powerChanged (drivePowerScroll). A local
        // window leaves it there; a remote window sends the power setting
        // alone and the Core saves it. Writing the slot here too could pick
        // a different band (this applet's active slice) from the one that
        // transmits.
        //
        // The slider leaves the tune drive source alone: Thetis ptbPWR_Scroll
        // never writes _tuneDrivePowerSource, which only the Setup radios set
        // (setup.cs:22902-22915 [v2.10.3.15], radUseDriveSliderTune /
        // radUseTuneSliderTune / radUseFixedDriveTune_CheckedChanged).
    });

    // Reverse: TransmitModel::powerChanged → slider
    connectRow(s, &tx, &TransmitModel::powerChanged, [this, s](int power) {
        QSignalBlocker b(s->rfPowerSlider());
        m_updatingFromModel = true;
        s->rfPowerSlider()->setValue(power);
        updatePowerSliderLabels();   // Issue #175 Task 7: HL2 dB / non-HL2 int
        m_updatingFromModel = false;
    });


    // ── Tune Power slider → TransmitModel::setTunePowerForTxBand ────────────
    // Per-band tune power, ported from Thetis console.cs:12094 [v2.10.3.13]:
    //   private int[] tunePower_by_band;
    // The slider writes and shows the transmit band's slot, as Thetis
    // ptbTune_Scroll does (PA on-air gate review):
    // From Thetis console.cs:46618 [v2.10.3.15]
    //   tunePower_by_band[(int)_tx_band] = ptbTune.Value;
    // m_currentBand (the pan or slice band) is used only before RadioModel
    // knows the transmit band.
    //
    // Issue #175 Task 7: label text routed through updatePowerSliderLabels()
    // for the HL2 (slider/3.0 - 33.0)/2.0 dB conversion.
    connectRow(s, s->tunePowerSlider(), &QSlider::valueChanged, [this, s, &tx](int val) {
        updatePowerSliderLabels();
        if (m_updatingFromModel) { return; }
        // R-R3-49 (parity Task 2): a remote window asks the Core, which
        // does what the lines below do for its own transmit band. A drag
        // asks once, on release.
        if (remoteTunePower()) {
            if (!s->tunePowerSlider()->isSliderDown()) {
                requestRemoteTunePower(val);
            }
            return;
        }
        // The tune drive source stays as Setup > Transmit > Power set it:
        // Thetis ptbTune_Scroll (console.cs:46607-46630 [v2.10.3.15]) writes
        // tunePower_by_band and never _tuneDrivePowerSource.
        if (tx.setTunePowerForTxBand(val)) { return; }
        tx.setTunePowerForBand(m_currentBand, val);
    });

    connectRow(s, s->tunePowerSlider(), &QSlider::sliderReleased, [this, s]() {
        if (m_updatingFromModel || !remoteTunePower()) { return; }
        requestRemoteTunePower(s->tunePowerSlider()->value());
    });

    // Fix round 1 (minor 2): the change answered, the slider shows the
    // Core's value, whichever arrived first, its answer or its next value
    // (or a value it clamped). Not while the operator holds the slider.
    connectRow(s, &tx, &TransmitModel::tunePowerForTxBandWriteInFlightChanged,
            [this, s, &tx](bool inFlight) {
        if (inFlight || !remoteTunePower() || s->tunePowerSlider()->isSliderDown()) { return; }
        QSignalBlocker b(s->tunePowerSlider());
        m_updatingFromModel = true;
        s->tunePowerSlider()->setValue(tx.tunePowerForTxBand());
        updatePowerSliderLabels();
        m_updatingFromModel = false;
    });

    // R-R3-49 (parity Task 2): in a remote window the slider shows the
    // Core's tune power for its transmit band; a local window shows its own
    // transmit band's (PA on-air gate review).
    connectRow(s, &tx, &TransmitModel::tunePowerForTxBandChanged,
            [this, s, &tx](int watts) {
        if (!remoteTunePower() && !tx.tuneTxBandKnown()) { return; }
        // Fix wave GUI-I6: the Core's value does not move the slider out
        // from under the operator's hand, nor back to an older value while
        // the operator's change is on its way (the Core's next value, after
        // its answer, shows; a refusal shows the Core's value again).
        if (remoteTunePower()
            && (s->tunePowerSlider()->isSliderDown() || tx.tunePowerForTxBandWriteInFlight())) {
            return;
        }
        QSignalBlocker b(s->tunePowerSlider());
        m_updatingFromModel = true;
        s->tunePowerSlider()->setValue(watts);
        updatePowerSliderLabels();
        m_updatingFromModel = false;
    });

    // Reverse: TransmitModel::tunePowerByBandChanged → slider (only for
    // current band, and only before the transmit band is known; after that
    // tunePowerForTxBandChanged above repaints it).
    connectRow(s, &tx, &TransmitModel::tunePowerByBandChanged,
            [this, s, &tx](Band band, int watts) {
        if (band != m_currentBand || remoteTunePower() || tx.tuneTxBandKnown()) { return; }
        QSignalBlocker b(s->tunePowerSlider());
        m_updatingFromModel = true;
        s->tunePowerSlider()->setValue(watts);
        updatePowerSliderLabels();   // Issue #175 Task 7: HL2 dB / non-HL2 int
        m_updatingFromModel = false;
    });

}

// ---------------------------------------------------------------------------
// The TxFilterSection row's wiring, for any instance of it.
//
// The applet owns the row, and a container may hold an independent copy of
// it (createFilterSection). Both are wired here, so both show and set the same
// model state, and both follow the same permission gates.
// ---------------------------------------------------------------------------
void TxApplet::wireFilterSection(TxFilterSection* s)
{
    if (!s || !m_model) {
        return;
    }
    TransmitModel& tx = m_model->transmitModel();
    // ── Plan 4 Cluster C (Task 4 / D2+D3): TX BW spinbox wiring ─────────────
    //
    // UI → Model: spinbox valueChanged → setFilterLow / setFilterHigh.
    //             m_updatingFromModel guard prevents echo loops.
    // Model → UI: TransmitModel::filterChanged(int,int) → QSignalBlocker on
    //             both spinboxes, then setValue + refresh status label.
    // Status label refresh helper (shared by filterChanged and dspModeChanged).
    auto refreshFilterStatus = [this]() { refreshTxFilterStatus(); };

    if (s->filterLowSpin()) {
        connectRow(s, s->filterLowSpin(), QOverload<int>::of(&QSpinBox::valueChanged),
                [this](int v) {
            if (m_updatingFromModel) { return; }
            m_model->transmitModel().setFilterLow(v);
        });
    }
    if (s->filterHighSpin()) {
        connectRow(s, s->filterHighSpin(), QOverload<int>::of(&QSpinBox::valueChanged),
                [this](int v) {
            if (m_updatingFromModel) { return; }
            m_model->transmitModel().setFilterHigh(v);
        });
    }

    // Model → spinboxes + status label on filterChanged.
    connectRow(s, &tx, &TransmitModel::filterChanged,
            [this, s, refreshFilterStatus](int low, int high) {
        m_updatingFromModel = true;
        if (s->filterLowSpin()) {
            QSignalBlocker bLo(s->filterLowSpin());
            s->filterLowSpin()->setValue(low);
        }
        if (s->filterHighSpin()) {
            QSignalBlocker bHi(s->filterHighSpin());
            s->filterHighSpin()->setValue(high);
        }
        m_updatingFromModel = false;
        refreshFilterStatus();
    });

    // Status label refresh on DSP mode change (symmetric ↔ asymmetric format)
    // rides followTransmitSlice's connection (slice control plan Task 11).
    refreshFilterStatus();

}

// ---------------------------------------------------------------------------
// The TxChainSection row's wiring, for any instance of it.
//
// The applet owns the row, and a container may hold an independent copy of
// it (createChainSection). Both are wired here, so both show and set the same
// model state, and both follow the same permission gates.
// ---------------------------------------------------------------------------
void TxApplet::wireChainSection(TxChainSection* s)
{
    if (!s || !m_model) {
        return;
    }
    TransmitModel& tx = m_model->transmitModel();
    // ── Phase 3M-3a-i Batch 2 (Task F): LEV / EQ quick toggles ──────────────
    //
    // LEV button ↔ TransmitModel::txLevelerOn (bidirectional, echo-guarded).
    if (s->levButton()) {
        connectRow(s, s->levButton(), &QPushButton::toggled,
                [this, &tx](bool on) {
            if (m_updatingFromModel) { return; }
            tx.setTxLevelerOn(on);
        });
        connectRow(s, &tx, &TransmitModel::txLevelerOnChanged,
                [this, s](bool on) {
            QSignalBlocker b(s->levButton());
            m_updatingFromModel = true;
            s->levButton()->setChecked(on);
            m_updatingFromModel = false;
        });
    }

    // EQ button ↔ TransmitModel::txEqEnabled (bidirectional, echo-guarded).
    // Right-click: placeholder slot for the TxEqDialog launch (Batch 3).
    if (s->eqButton()) {
        connectRow(s, s->eqButton(), &QPushButton::toggled,
                [this, &tx](bool on) {
            if (m_updatingFromModel) { return; }
            tx.setTxEqEnabled(on);
        });
        connectRow(s, &tx, &TransmitModel::txEqEnabledChanged,
                [this, s](bool on) {
            QSignalBlocker b(s->eqButton());
            m_updatingFromModel = true;
            s->eqButton()->setChecked(on);
            m_updatingFromModel = false;
        });
        // Right-click → open TxEqDialog (Phase 3M-3a-i Batch 3 A.1).
        // Modeless singleton; show + raise + activateWindow so a
        // hidden-but-alive instance is brought forward.
        connectRow(s, s->eqButton(), &QPushButton::customContextMenuRequested,
                [this](const QPoint& /*pos*/) {
            // R-R3-49 (parity Task 4): opens in a remote window too; the
            // dialog greys itself with the reason while the Core cannot
            // take a change (TxEqDialog::setSettingsPermitted).
            if (!m_model) { return; }
            TxEqDialog* dlg = TxEqDialog::instance(m_model, this);
            dlg->show();
            dlg->raise();
            dlg->activateWindow();
        });
    }


    // ── Phase 3M-3a-ii Batch 6 (Task A): CFC button ↔ TransmitModel::cfcEnabled
    // Right-click opens the modeless TxCfcDialog (lazy-create singleton kept
    // alive for fast re-opens — same pattern as TxEqDialog).
    if (s->cfcButton()) {
        connectRow(s, s->cfcButton(), &QPushButton::toggled,
                [this, &tx](bool on) {
            if (m_updatingFromModel) { return; }
            tx.setCfcEnabled(on);
        });
        connectRow(s, &tx, &TransmitModel::cfcEnabledChanged,
                [this, s](bool on) {
            QSignalBlocker b(s->cfcButton());
            m_updatingFromModel = true;
            s->cfcButton()->setChecked(on);
            m_updatingFromModel = false;
        });
        connectRow(s, s->cfcButton(), &QPushButton::customContextMenuRequested,
                [this](const QPoint& /*pos*/) {
            requestOpenCfcDialog();
        });
    }
    // ── Phase 3M-1c J.1 ─ TX Profile combo wiring ────────────────────────────
    // User-driven currentTextChanged → MicProfileManager::setActiveProfile.
    // Guarded with m_updatingFromModel so the rebuildProfileCombo() echo
    // doesn't bounce back into setActiveProfile.
    connectRow(s, s->profileCombo(), &QComboBox::currentTextChanged,
            [this](const QString& name) {
        if (m_updatingFromModel) { return; }
        if (!m_micProfileMgr) { return; }
        if (name.isEmpty()) { return; }
        if (m_model) {
            m_micProfileMgr->setActiveProfile(name, &m_model->transmitModel());
        }
    });

    // Right-click on combo → emit txProfileMenuRequested (Thetis cite at the
    // signal declaration).
    connectRow(s, s->profileCombo(), &QComboBox::customContextMenuRequested,
            [this](const QPoint& /*pos*/) {
        emit txProfileMenuRequested();
    });

}

// ---------------------------------------------------------------------------
// Independent copies of the power, filter and chain rows, for containers that
// hold them on their own.  The applet makes each copy and wires it, so the
// copy shows and sets the same state as the applet's own row.
// ---------------------------------------------------------------------------
TxPowerSection* TxApplet::createPowerSection(QWidget* parent, bool preview)
{
    auto* s = new TxPowerSection(parent, txRowSpacing(),
                               m_model ? m_model->mainPowerMeter() : nullptr,
                               txCopyMargins());
    if (preview) {
        makeRowInert(s);
        return s;
    }
    wirePowerSection(s);
    m_extraPowerSections.append(s);
    // The copy belongs to the container, which owns it and may destroy it
    // when the operator closes the container or removes the item.  The
    // applet's timers walk this list, so a copy that is gone must not stay
    // in it.  The QPointer drops it on its own; this keeps the list from
    // growing as copies come and go.
    const QPointer<TxPowerSection> guard(s);
    connect(s, &QObject::destroyed, this, [this, guard]{
        m_extraPowerSections.removeAll(guard);
    });
    // The copy starts where the model is, not where the applet's own row was
    // when it was built.
    syncSectionInstances();
    return s;
}

TxFilterSection* TxApplet::createFilterSection(QWidget* parent, bool preview)
{
    auto* s = new TxFilterSection(parent, txRowSpacing(), txCopyMargins());
    if (preview) {
        makeRowInert(s);
        return s;
    }
    wireFilterSection(s);
    m_extraFilterSections.append(s);
    // The copy belongs to the container, which owns it and may destroy it
    // when the operator closes the container or removes the item.  The
    // applet's timers walk this list, so a copy that is gone must not stay
    // in it.  The QPointer drops it on its own; this keeps the list from
    // growing as copies come and go.
    const QPointer<TxFilterSection> guard(s);
    connect(s, &QObject::destroyed, this, [this, guard]{
        m_extraFilterSections.removeAll(guard);
    });
    // The copy starts where the model is, not where the applet's own row was
    // when it was built.
    syncSectionInstances();
    return s;
}

TxChainSection* TxApplet::createChainSection(QWidget* parent, bool preview)
{
    auto* s = new TxChainSection(parent, txRowSpacing(), txCopyMargins());
    if (preview) {
        makeRowInert(s);
        return s;
    }
    wireChainSection(s);
    m_extraChainSections.append(s);
    // The copy belongs to the container, which owns it and may destroy it
    // when the operator closes the container or removes the item.  The
    // applet's timers walk this list, so a copy that is gone must not stay
    // in it.  The QPointer drops it on its own; this keeps the list from
    // growing as copies come and go.
    const QPointer<TxChainSection> guard(s);
    connect(s, &QObject::destroyed, this, [this, guard]{
        m_extraChainSections.removeAll(guard);
    });
    // The copy starts where the model is, not where the applet's own row was
    // when it was built.
    syncSectionInstances();
    return s;
}

TxVoxSection* TxApplet::createVoxSection(QWidget* parent, bool preview)
{
    auto* s = new TxVoxSection(parent, txRowSpacing(), txCopyMargins());
    if (preview) {
        makeRowInert(s);
        return s;
    }
    wireVoxSection(s);
    m_extraVoxSections.append(s);
    // The copy belongs to the container, which owns it and may destroy it
    // when the operator closes the container or removes the item.  The
    // applet's timers walk this list, so a copy that is gone must not stay
    // in it.  The QPointer drops it on its own; this keeps the list from
    // growing as copies come and go.
    const QPointer<TxVoxSection> guard(s);
    connect(s, &QObject::destroyed, this, [this, guard]{
        m_extraVoxSections.removeAll(guard);
    });
    // The copy starts where the model is, not where the applet's own row was
    // when it was built.
    syncSectionInstances();
    return s;
}

TxProcessingSection* TxApplet::createProcessingSection(QWidget* parent, bool preview)
{
    auto* s = new TxProcessingSection(parent, txRowSpacing(), txCopyMargins());
    if (preview) {
        makeRowInert(s);
        return s;
    }
    wireProcessingSection(s);
    m_extraProcessingSections.append(s);
    // The copy belongs to the container, which owns it and may destroy it
    // when the operator closes the container or removes the item.  The
    // applet's timers walk this list, so a copy that is gone must not stay
    // in it.  The QPointer drops it on its own; this keeps the list from
    // growing as copies come and go.
    const QPointer<TxProcessingSection> guard(s);
    connect(s, &QObject::destroyed, this, [this, guard]{
        m_extraProcessingSections.removeAll(guard);
    });
    // The copy starts where the model is, not where the applet's own row was
    // when it was built.
    syncSectionInstances();
    return s;
}

TxKeyingSection* TxApplet::createKeyingSection(QWidget* parent, bool preview)
{
    auto* s = new TxKeyingSection(parent, txRowSpacing(), txCopyMargins());
    if (preview) {
        makeRowInert(s);
        return s;
    }
    wireKeyingSection(s);
    // The Take-transmit press is this applet's own signal, as the applet's
    // own row's is (buildUI).
    if (s->takeTransmitButton()) {
        connect(s->takeTransmitButton(), &QPushButton::clicked, this,
                &TxApplet::takeTransmitRequested);
    }
    m_extraKeyingSections.append(s);
    // The copy belongs to the container, which owns it and may destroy it
    // when the operator closes the container or removes the item.  The
    // applet's timers walk this list, so a copy that is gone must not stay
    // in it.  The QPointer drops it on its own; this keeps the list from
    // growing as copies come and go.
    const QPointer<TxKeyingSection> guard(s);
    connect(s, &QObject::destroyed, this, [this, guard]{
        m_extraKeyingSections.removeAll(guard);
    });
    refreshTransmitSliceChoicesIn(s);
    // The copy starts where the model is, not where the applet's own row was
    // when it was built.
    syncSectionInstances();
    return s;
}

// Every instance of the monitor rows: the applet's own, and the independent
// copies made for containers.
QList<TxMonitorSection*> TxApplet::allMonitorSections() const
{
    QList<TxMonitorSection*> sections;
    if (m_monitorSection) {
        sections << m_monitorSection;
    }
    for (const QPointer<TxMonitorSection>& copy : m_extraMonitorSections) {
        if (copy) {
            sections << copy.data();
        }
    }
    return sections;
}

// Every instance of the filter rows: the applet's own, and the independent
// copies made for containers.
QList<TxFilterSection*> TxApplet::allFilterSections() const
{
    QList<TxFilterSection*> sections;
    if (m_filterSection) {
        sections << m_filterSection;
    }
    for (const QPointer<TxFilterSection>& copy : m_extraFilterSections) {
        if (copy) {
            sections << copy.data();
        }
    }
    return sections;
}

// Every instance of the chain rows: the applet's own, and the independent
// copies made for containers.
QList<TxChainSection*> TxApplet::allChainSections() const
{
    QList<TxChainSection*> sections;
    if (m_chainSection) {
        sections << m_chainSection;
    }
    for (const QPointer<TxChainSection>& copy : m_extraChainSections) {
        if (copy) {
            sections << copy.data();
        }
    }
    return sections;
}

// Every instance of the VOX rows: the applet's own, and the independent
// copies made for containers.
QList<TxVoxSection*> TxApplet::allVoxSections() const
{
    QList<TxVoxSection*> sections;
    if (m_voxSection) {
        sections << m_voxSection;
    }
    for (const QPointer<TxVoxSection>& copy : m_extraVoxSections) {
        if (copy) {
            sections << copy.data();
        }
    }
    return sections;
}

// Every instance of the processing rows: the applet's own, and the independent
// copies made for containers.
QList<TxProcessingSection*> TxApplet::allProcessingSections() const
{
    QList<TxProcessingSection*> sections;
    if (m_processingSection) {
        sections << m_processingSection;
    }
    for (const QPointer<TxProcessingSection>& copy : m_extraProcessingSections) {
        if (copy) {
            sections << copy.data();
        }
    }
    return sections;
}

// Every instance of the keying rows: the applet's own, and the independent
// copies made for containers.
QList<TxKeyingSection*> TxApplet::allKeyingSections() const
{
    QList<TxKeyingSection*> sections;
    if (m_keyingSection) {
        sections << m_keyingSection;
    }
    for (const QPointer<TxKeyingSection>& copy : m_extraKeyingSections) {
        if (copy) {
            sections << copy.data();
        }
    }
    return sections;
}

// Every instance of the power rows: the applet's own, and the independent
// copies made for containers.
QList<TxPowerSection*> TxApplet::allPowerSections() const
{
    QList<TxPowerSection*> sections;
    if (m_powerSection) {
        sections << m_powerSection;
    }
    for (const QPointer<TxPowerSection>& copy : m_extraPowerSections) {
        if (copy) {
            sections << copy.data();
        }
    }
    return sections;
}

// The controls the receive-only lock covers: the transmit keys, the 2-TONE
// button and the VOX toggle, in the applet's own rows and in every copy.
QList<QWidget*> TxApplet::rxOnlyLockedControls() const
{
    QList<QWidget*> controls;
    for (const TxKeyingSection* s : allKeyingSections()) {
        controls << s->moxButton()
                 << s->tuneButton();
    }
    for (const TxProcessingSection* s : allProcessingSections()) {
        controls << s->twoToneButton();
    }
    for (const TxVoxSection* s : allVoxSections()) {
        controls << s->button();
    }
    return controls;
}

// The controls the transmit-settings gate covers: the RF Power slider and the
// TX filter cutoffs, in the applet's own rows and in every copy of them.
QList<QWidget*> TxApplet::transmitSettingsGatedControls() const
{
    QList<QWidget*> controls;
    for (const TxPowerSection* s : allPowerSections()) {
        controls << s->rfPowerSlider();
    }
    for (const TxFilterSection* s : allFilterSections()) {
        controls << s->filterLowSpin()
                 << s->filterHighSpin();
    }
    return controls;
}

// The controls the transmit-chain-settings gate covers: the Tune Power slider,
// the VOX sliders, the LEV / EQ / CFC buttons, the MON toggle and the
// monitor volume slider, in the applet's own rows and in every copy of them.
QList<QWidget*> TxApplet::chainSettingsGatedControls() const
{
    QList<QWidget*> controls;
    for (const TxPowerSection* s : allPowerSections()) {
        controls << s->tunePowerSlider();
    }
    for (const TxVoxSection* s : allVoxSections()) {
        controls << s->thresholdSlider()
                 << s->holdSlider();
    }
    for (const TxChainSection* s : allChainSections()) {
        controls << s->levButton()
                 << s->eqButton()
                 << s->cfcButton();
    }
    return controls;
}

// The controls the TX-profile gate covers: the profile combo, in the applet's
// own row and in every copy of it.
QList<QWidget*> TxApplet::profileGatedControls() const
{
    QList<QWidget*> controls;
    for (const TxChainSection* s : allChainSections()) {
        controls << s->profileCombo();
    }
    return controls;
}

// ---------------------------------------------------------------------------
// The VOX row's wiring, for any instance of it.
//
// The applet owns the row, and a container may hold an independent copy of
// it (createVoxSection).  Both are wired here, so both show and set the same
// model state, and both follow the same permission gates.
// ---------------------------------------------------------------------------
void TxApplet::wireVoxSection(TxVoxSection* s)
{
    if (!s || !m_model) {
        return;
    }
    TransmitModel& tx = m_model->transmitModel();
    // ── VOX [ON] toggle ↔ TransmitModel::voxEnabled ──────────────────────────
    if (s->button()) {
        {
            QSignalBlocker b(s->button());
            s->button()->setChecked(tx.voxEnabled());
        }
        // UI → Model
        connectRow(s, s->button(), &QPushButton::toggled, [this, &tx](bool on) {
            if (m_updatingFromModel) { return; }
            tx.setVoxEnabled(on);
        });
        // Model → UI
        connectRow(s, &tx, &TransmitModel::voxEnabledChanged, [this, s](bool on) {
            m_updatingFromModel = true;
            {
                QSignalBlocker b(s->button());
                s->button()->setChecked(on);
            }
            m_updatingFromModel = false;
        });
    }

    // ── VOX threshold slider ↔ TransmitModel::voxThresholdDb ─────────────────
    // Range -80..0 dB from console.Designer.cs:6018-6019 [v2.10.3.13]
    // (already set in buildUI).  Default value -20 dB matches Thetis
    // ptbVOX.Value=-20 (console.Designer.cs:6024 [v2.10.3.13]).
    if (s->thresholdSlider()) {
        {
            QSignalBlocker b(s->thresholdSlider());
            s->thresholdSlider()->setValue(tx.voxThresholdDb());
        }
        if (s->thresholdLabel()) {
            s->thresholdLabel()->setText(QStringLiteral("%1 dB").arg(tx.voxThresholdDb()));
        }
        // UI → Model + label refresh
        connectRow(s, s->thresholdSlider(), &QSlider::valueChanged, [this, s, &tx](int dB) {
            if (s->thresholdLabel()) {
                s->thresholdLabel()->setText(QStringLiteral("%1 dB").arg(dB));
            }
            if (m_updatingFromModel) { return; }
            tx.setVoxThresholdDb(dB);
        });
        // Model → UI
        connectRow(s, &tx, &TransmitModel::voxThresholdDbChanged, [this, s](int dB) {
            m_updatingFromModel = true;
            {
                QSignalBlocker b(s->thresholdSlider());
                s->thresholdSlider()->setValue(dB);
            }
            if (s->thresholdLabel()) {
                s->thresholdLabel()->setText(QStringLiteral("%1 dB").arg(dB));
            }
            m_updatingFromModel = false;
        });
    }

    // ── VOX Hold slider ↔ TransmitModel::voxHangTimeMs ───────────────────────
    // Range 1..2000 ms from setup.designer.cs:45005-45013 [v2.10.3.13]
    // (already set in buildUI).  Default 500 ms matches udDEXPHold.Value
    // (setup.designer.cs:45020 [v2.10.3.13]).
    if (s->holdSlider()) {
        {
            QSignalBlocker b(s->holdSlider());
            s->holdSlider()->setValue(tx.voxHangTimeMs());
        }
        if (s->holdLabel()) {
            s->holdLabel()->setText(QStringLiteral("%1 ms").arg(tx.voxHangTimeMs()));
        }
        // UI → Model + label refresh
        connectRow(s, s->holdSlider(), &QSlider::valueChanged, [this, s, &tx](int ms) {
            if (s->holdLabel()) {
                s->holdLabel()->setText(QStringLiteral("%1 ms").arg(ms));
            }
            if (m_updatingFromModel) { return; }
            tx.setVoxHangTimeMs(ms);
        });
        // Model → UI
        connectRow(s, &tx, &TransmitModel::voxHangTimeMsChanged, [this, s](int ms) {
            m_updatingFromModel = true;
            {
                QSignalBlocker b(s->holdSlider());
                s->holdSlider()->setValue(ms);
            }
            if (s->holdLabel()) {
                s->holdLabel()->setText(QStringLiteral("%1 ms").arg(ms));
            }
            m_updatingFromModel = false;
        });
    }

    // ── Right-click on VOX → emit openSetupRequested ─────────────────────────
    // Mirrors PhoneCwApplet's DEXP-button right-click pattern.  MainWindow
    // listens and jumps the SetupDialog to the "DEXP/VOX" leaf page.
    if (s->button()) {
        connectRow(s, s->button(), &QPushButton::customContextMenuRequested,
                [this](const QPoint&) {
            emit openSetupRequested(QStringLiteral("Transmit"),
                                    QStringLiteral("DEXP/VOX"));
        });
    }

}

// ---------------------------------------------------------------------------
// The processing rows' wiring, for any instance of them.
//
// The applet owns the rows, and a container may hold an independent copy of
// them (createProcessingSection).  Both are wired here, so both show and set
// the same model state, and both follow the same permission gates.
// ---------------------------------------------------------------------------
void TxApplet::wireProcessingSection(TxProcessingSection* s)
{
    if (!s || !m_model) {
        return;
    }

    // The DUP press raises this applet's own signal, so it is wired here
    // rather than inside the section.
    connect(s->dupButton(), &QPushButton::clicked, this,
            &TxApplet::displayDuplexRequested);

    // ── SWR Prot LED ← SwrProtectionController::highSwrChanged ─────────────
    // SwrProtectionController (Phase 3G-13) emits highSwrChanged(bool) when
    // the radio's SWR-protection state changes. Light the LED amber when
    // high-SWR protection is active; dim it when cleared.
    {
        auto updateSwrProtLed = [s](bool isHigh) {
            if (!s->swrProtectionLed()) { return; }
            const QString color = isHigh ? QStringLiteral("#ffaa00")
                                         : Style::kTextInactive;
            s->swrProtectionLed()->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; font-size: 9px; font-weight: bold; }"
            ).arg(color));
        };
        connectRow(s, &m_model->swrProt(),
                &safety::SwrProtectionController::highSwrChanged,
                updateSwrProtLed);
        // Initialise to current state.
        updateSwrProtLed(m_model->swrProt().highSwr());
    }


    // ── Phase 3M-1c J.2 ─ 2-TONE button wiring ───────────────────────────────
    // toggled → TwoToneController::setActive.  Echo-guarded.
    connectRow(s, s->twoToneButton(), &QPushButton::toggled, [this, s](bool on) {
        if (m_updatingFromModel) { return; }
        // Desktop remote transmit: a remote window asks the Core.
        if (m_model && m_model->remoteTransmitRouted()) {
            m_model->setTwoTone(on);
            return;
        }
        // Fix wave (hosting 2-TONE parity): a hosting window asks to take
        // transmit first. The button shows the test's own state after.
        if (m_desktopTwoToneRequest) {
            const QPointer<TxApplet> self(this);
            m_desktopTwoToneRequest(on);
            if (self && s->twoToneButton()) {
                const QSignalBlocker blocker(s->twoToneButton());
                s->twoToneButton()->setChecked(m_twoToneCtrl && m_twoToneCtrl->isActive());
            }
            return;
        }
        if (!m_twoToneCtrl) { return; }
        m_twoToneCtrl->setActive(on);
    });


    // ── Phase 3M-4 / WDSP 2.10: PS-A button wiring ──────────────────────────
    // Source-first port of Thetis chkFWCATUBypass:
    //   - Left-click toggle drives PureSignal::setAutoCalEnabled (mirrors
    //     chkFWCATUBypass_Click, console.cs:36762 [v2.10.3.13]).
    //   - Right-click opens PsForm via openPureSignalDialogRequested (mirrors
    //     chkFWCATUBypass_MouseDown, console.cs:46149-46152 [v2.10.3.13]:
    //       if (IsRightButton(e)) linearityToolStripMenuItem_Click(null,
    //                                                              EventArgs.Empty);
    //     ).
    //
    // Production always uses RadioModel's one session facade. The local
    // coordinator remains late-bound behind that facade.
    if (s->psaButton()) {
        // Right-click → emit openPureSignalDialogRequested.  Wired
        // unconditionally so the seam exists even when no PureSignal
        // coordinator is bound (test harness verifies the signal emit).
        s->psaButton()->setContextMenuPolicy(Qt::CustomContextMenu);
        connectRow(s, s->psaButton(), &QWidget::customContextMenuRequested,
                [this](const QPoint&) {
            emit openPureSignalDialogRequested();
        });

        // The checked state expresses automatic-calibration intent. Turning
        // it off uses the acknowledged Off/reset action.
        connectRow(s, s->psaButton(), &QPushButton::toggled, [this](bool on) {
            if (m_updatingFromModel || !m_psFacade) {
                return;
            }
            const Ps3Action action = on
                ? Ps3Action::StartAutomatic : Ps3Action::OffReset;
            if (m_psFacade->requestAction(action) == 0) {
                syncPsaFromFacade();
            }
        });

    }
}

// ---------------------------------------------------------------------------
// The keying rows' wiring, for any instance of them.
//
// The applet owns the rows, and a container may hold an independent copy of
// them (createKeyingSection).  Both are wired here, so both show and set the
// same model state, and both follow the same permission gates.
// ---------------------------------------------------------------------------
void TxApplet::wireKeyingSection(TxKeyingSection* s)
{
    if (!s || !m_model) {
        return;
    }
    MoxController* mox = m_model->moxController();

    // ── TUNE button → RadioModel::setTune(bool) ──────────────────────────────
    // G.4 orchestrator: CW mode swap, tone setup, tune-power push.
    // From Thetis console.cs:29978-30157 [v2.10.3.13] chkTUN_CheckedChanged.
    // G8NJJ tell ARIES that tune is active  [original inline comment from console.cs:30153]
    // MW0LGE_22b setupTuneDriveSlider  [original inline comment from console.cs:30155]
    connectRow(s, s->tuneButton(), &QPushButton::toggled, [this, s](bool on) {
        if (m_updatingFromModel) { return; }
        if (!m_model) { return; }
        // R-R3-21: the button's text and checked state are written only by
        // the Tune state and refusal handlers below (manualMoxChanged,
        // tuneRefused). Writing "TUNING..." here, after setTune returned,
        // painted a refused press as tuning: the refusal had already reset
        // the button inside this call.
        if (m_desktopTuneRequest) {
            const QPointer<TxApplet> self(this);
            m_desktopTuneRequest(on);
            if (self) { syncDesktopKeyState(); }
        } else {
            // TX rulings (item 1): a remote window's press keys after a
            // release even while the Core's TUNE still reads on, and the
            // button shows what it asked until the Core's state arrives.
            const bool asked = m_model->tunePressAsksOn(on);
            if (asked != on) {
                QSignalBlocker b(s->tuneButton());
                s->tuneButton()->setChecked(asked);
            }
            m_model->setTune(asked);
        }
    });

    // Reverse: tuneRefused → uncheck TUN button + clear text.
    // From Thetis console.cs:30076 [v2.10.3.13]: guard conditions before
    // chkTUN.Checked = true (connection + power-on checks).
    connectRow(s, m_model, &RadioModel::tuneRefused, [this, s](const QString& /*reason*/) {
        if (m_desktopTuneOn) { syncDesktopKeyState(); return; }
        QSignalBlocker b(s->tuneButton());
        m_updatingFromModel = true;
        s->tuneButton()->setChecked(false);
        s->tuneButton()->setText(QStringLiteral("TUNE"));
        m_updatingFromModel = false;
    });

    // ── MOX button → RadioModel::setMoxFromButton(bool) ─────────────────────
    // B.5 setter: drives state machine through RX→TX or TX→RX transitions.
    // From Thetis console.cs:29311-29678 [v2.10.3.13] chkMOX_CheckedChanged2.
    // //[2.10.1.0]MW0LGE changed  [original inline comment from console.cs:29355]
    // //MW0LGE [2.9.0.7]  [original inline comment from console.cs:29400, 29561]
    // //[2.10.3.6]MW0LGE att_fixes  [original inline comment from console.cs:29567-29568, 29659]
    if (mox) {
        // Receiver and transmit gaps plan, Task 7: the MOX button is a manual
        // key (Thetis chkMOX_Click, console.cs:29730-29747 [v2.10.3.15]);
        // RadioModel::setMoxFromButton also turns TUN and two-tone off on
        // the way off, as chkMOX_Click does.
        connectRow(s, s->moxButton(), &QPushButton::toggled, [this, s](bool on) {
            if (m_updatingFromModel) { return; }
            if (m_desktopMoxRequest) {
                const QPointer<TxApplet> self(this);
                m_desktopMoxRequest(on);
                if (self) { syncDesktopKeyState(); }
            } else {
                // TX rulings (item 1): a remote window's press keys after a
                // release even while the Core's confirmation is on its way,
                // and the button shows what it asked until the Core's state
                // arrives.
                const bool asked = m_model->moxPressAsksOn(on);
                if (asked != on) {
                    QSignalBlocker b(s->moxButton());
                    s->moxButton()->setChecked(asked);
                }
                m_model->setMoxFromButton(asked);
            }
        });

        // Reverse: MoxController::moxStateChanged → button checked state.
        // moxStateChanged fires at end of the timer walk (TX fully engaged
        // or fully released), not at setMox() entry — so the button reflects
        // the confirmed state, not the in-progress request.
        connectRow(s, mox, &MoxController::moxStateChanged,
                [this, s](bool on) {
            if (m_desktopMoxOn) { syncDesktopKeyState(); return; }
            QSignalBlocker b(s->moxButton());
            m_updatingFromModel = true;
            s->moxButton()->setChecked(on);
            m_updatingFromModel = false;
        });

        // Reverse, the rejection half: MoxController::moxRejected → button
        // follows the state that actually holds.
        //
        // setMox(true) returns on rejection without advancing anything, so
        // moxStateChanged never fires -- and the connect immediately above
        // was the ONLY thing that unchecked this button. The user's own
        // press left it checked and red, reading "transmitting", with the
        // radio in RX.
        //
        // Pre-existing locally, where rejections are occasional (band-plan
        // and TX-interlock refusals). Remote-daemon R2 is what made it
        // worth fixing: on a Role::Remote model EVERY press is refused, so
        // the button was not occasionally wrong, it was permanently wrong.
        //
        // isMox() rather than a literal false: setMox() only ever rejects
        // a TX-on request, but reading the controller keeps the button
        // following the authority instead of an assumption about which
        // requests can be refused.
        connectRow(s, mox, &MoxController::moxRejected,
                [this, s, mox](const QString& /*reason*/) {
            if (m_desktopMoxOn) { syncDesktopKeyState(); return; }
            QSignalBlocker b(s->moxButton());
            m_updatingFromModel = true;
            s->moxButton()->setChecked(mox->isMox());
            m_updatingFromModel = false;
        });

        // TUNE button checked state driven by manualMoxChanged.
        // manualMoxChanged fires when setTune() sets/clears m_manualMox.
        connectRow(s, mox, &MoxController::manualMoxChanged,
                [this, s](bool isManual) {
            if (m_desktopTuneOn) { syncDesktopKeyState(); return; }
            QSignalBlocker b(s->tuneButton());
            m_updatingFromModel = true;
            s->tuneButton()->setChecked(isManual);
            s->tuneButton()->setText(isManual
                ? QStringLiteral("TUNING...")
                : QStringLiteral("TUNE"));
            m_updatingFromModel = false;
        });
    }

}

// ---------------------------------------------------------------------------
// Push the state the applet holds into every row.
//
// A copy of a row is made after the applet has already pushed the model
// state into its own row, so a copy made on its own would start blank.  The
// applet calls this when it makes one, so the copy starts where the model
// is and not where the applet was when it was built.
// ---------------------------------------------------------------------------
void TxApplet::syncSectionInstances()
{
    if (!m_model) { return; }

    // The radio's scale for the power gauge and sliders, before their
    // values: wirePowerSection rescales the rows the applet already lists,
    // which a copy being wired is not yet one of.
    {
        const auto model = m_model->hardwareProfile().model;
        rescaleFwdGaugeForModel(model);
        rescalePowerSlidersForModel(model);
    }

    // The model state: the power sliders, the VOX row, MON and the monitor
    // volume, the LEV / EQ / CFC buttons, the filter cutoffs and the
    // TUNE / MOX buttons.
    syncFromModel();

    // The TX Profile combo's entries.
    rebuildProfileCombo();

    // Who holds transmit, and the offer to take it.
    setTransmitHolderText(m_transmitHolderText);
    setTakeTransmitOffered(m_takeTransmitOffered, m_holderOnAir);

    // The full-duplex button.
    setDisplayDuplexState(m_dupChecked, m_dupAvailable, m_dupTooltip);

    // The 2-Tone button's own state.
    if (m_twoToneCtrl) {
        for (TxProcessingSection* s : allProcessingSections()) {
            const QSignalBlocker blk(s->twoToneButton());
            m_updatingFromModel = true;
            s->twoToneButton()->setChecked(m_twoToneCtrl->isActive());
            m_updatingFromModel = false;
        }
    }

    // The PS-A button, which the PureSignal facade owns.
    syncPsaFromFacade();

    // Every gate, last, so a copy starts as gated as the applet's own row.
    reapplyGates();
}

// ---------------------------------------------------------------------------
// Apply every gate again from the state its setter kept.
//
// The gates are applied when the window learns what it may do, usually before
// the operator places a row in a container, so a copy made later has missed
// them.  Each setter is idempotent on the controls it already holds, so this
// changes nothing on the applet's own rows and gates a new copy the same way,
// with the same reasons.
// ---------------------------------------------------------------------------
void TxApplet::reapplyGates()
{
    // The MOX tooltip for the transmit slice's mode, which the gates below
    // keep to give back.
    if (SliceModel* slice = transmitSlice()) {
        onMoxModeChanged(slice->dspMode());
    }
    // PS-A shows or hides as the applet's own does (setBoardCapabilities).
    if (m_processingSection && m_processingSection->psaButton()) {
        const bool shown = !m_processingSection->psaButton()->isHidden();
        for (TxProcessingSection* s : allProcessingSections()) {
            if (s->psaButton()) {
                s->psaButton()->setVisible(shown);
            }
        }
    }
    // MOX, TUNE, 2-TONE and VOX, with the receive-only lock on top.
    setTransmitPermitted(m_transmitPermitted, m_transmitPermissionReason);
    setVoxPermitted(m_voxPermitted, m_voxReason);
    setTransmitSettingsPermitted(m_transmitSettingsPermitted, m_transmitSettingsReason);
    // Also the MON output pair (applyMonitorOutputGate).
    setTransmitChainSettingsPermitted(m_transmitChainSettingsPermitted,
                                      m_transmitChainSettingsReason);
    setTxProfilePermitted(m_txProfilePermitted, m_txProfileReason);
    setPureSignalArmingPermitted(m_psArmingPermitted, m_psArmingReason);
    // The power slider TUNE or the two-tone test is not using is grayed in a
    // new copy too: the copy joins the list after it is wired.
    applyDriveSourceGate();
}

} // namespace NereusSDR
