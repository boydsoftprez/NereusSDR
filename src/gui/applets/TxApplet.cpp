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
#include "TxEqDialog.h"
#include "TxCfcDialog.h"
#include "gui/HGauge.h"
#include "gui/StyleConstants.h"
#include "gui/ComboStyle.h"
#include "gui/widgets/DexpPeakMeter.h"
#include "gui/widgets/VfoWidget.h"
#include "core/AudioEngine.h"
#include "core/ControlRanges.h"
#include "core/audio/CompositeTxMicRouter.h"
#include "core/MicProfileManager.h"
#include "core/MoxController.h"
#include "core/PureSignal.h"
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

// R-R3-45: the MON output captions, in one place so a rename is one edit.
// They match the receiver flag's output buttons (VfoWidget), in capitals
// like the flag's other buttons.
QString monitorSpeakersCaption()   { return QStringLiteral("SPEAKERS"); }
QString monitorHeadphonesCaption() { return QStringLiteral("PHONES"); }

// The tooltip a control had before the transmit-permission layer
// (setTransmitPermitted) disabled it, given back when permission returns.
constexpr auto kTransmitSavedTooltip = "TxAppletSavedTransmitTooltip";

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
    static constexpr auto kSavedDescription = "TxAppletSavedTransmitDescription";
    static constexpr auto kSavedEnabled = "TxAppletSavedTransmitEnabled";
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
    vbox->setContentsMargins(4, 2, 4, 2);
    vbox->setSpacing(2);
    outer->addWidget(body);

    // ── 0. Mic-source badge ── read-only label above the gauges ─────────────
    // Phase 3M-1b J.3: shows "PC mic" or "Radio mic" reflecting
    // TransmitModel::micSource (default MicSource::Pc). Read-only; no interaction.
    // Updates on micSourceChanged signal (wired in wireControls()).
    {
        m_micSourceBadge = new QLabel(QStringLiteral("PC mic"), this);
        m_micSourceBadge->setAlignment(Qt::AlignCenter);
        m_micSourceBadge->setFixedHeight(16);
        m_micSourceBadge->setStyleSheet(QStringLiteral(
            "QLabel {"
            " color: %1;"
            " font-size: 9px;"
            " border: 1px solid %2;"
            " border-radius: 2px;"
            " padding: 0px 4px;"
            " background: %3;"
            "}"
        ).arg(Style::kTitleText, Style::kInsetBorder, Style::kInsetBg));
        m_micSourceBadge->setAccessibleName(QStringLiteral("Mic source indicator"));
        m_micSourceBadge->setToolTip(QStringLiteral(
            "Active microphone source: PC mic or Radio mic.\n"
            "Change via Settings > Audio > TX Input."));
        vbox->addWidget(m_micSourceBadge);
    }

    // ── 1. Forward Power gauge — per-SKU scale ──────────────────────────────
    // Default ticks at construction match the Hermes-class 100 W radio
    // (0 / 40 / 80 / 100 / 120 — AetherSDR TxApplet.cpp:71); rescaleFwdGaugeForModel()
    // reapplies a per-SKU range when RadioModel::currentRadioChanged fires
    // (wired in wireControls()).  Bench-reported #167 follow-up: HL2 reading
    // 1-5 W on a 0-120 W scale was a barely-visible sliver; per-SKU scaling
    // gives each radio a properly-sized meter.
    auto* fwdGauge = new HGauge(this);
    fwdGauge->setTitle(QStringLiteral("RF Pwr"));
    fwdGauge->setAccessibleName(QStringLiteral("Forward power gauge"));
    m_fwdPowerGauge = fwdGauge;
    rescaleFwdGaugeForModel(HPSDRModel::FIRST);   // sentinel default until model known
    // 3M-1a (2026-04-27): wired to RadioStatus::powerChanged in
    // wireControls() — the gauge displays radio-reported forward
    // power in watts (scaleFwdPowerWatts'd at the model side).
    // The legacy "Phase 3I-1" NYI marker has been dropped.
    vbox->addWidget(fwdGauge);

    // ── 2. SWR gauge ── 1.0–3.0, redStart 2.5 ───────────────────────────────
    // Ticks: 1 / 1.5 / 2.5 / 3  (AetherSDR TxApplet.cpp:77)
    auto* swrGauge = new HGauge(this);
    // Range and red zone from ControlRanges.h, which the Core's catalogue
    // reads too (iPhone app Task 19).
    swrGauge->setRange(ControlRanges::kSwrGaugeMin, ControlRanges::kSwrGaugeMax);
    swrGauge->setRedStart(ControlRanges::kSwrGaugeRedFrom);
    swrGauge->setYellowStart(ControlRanges::kSwrGaugeRedFrom);
    swrGauge->setTitle(QStringLiteral("SWR"));
    swrGauge->setTickLabels({QStringLiteral("1"), QStringLiteral("1.5"),
                              QStringLiteral("2.5"), QStringLiteral("3")});
    swrGauge->setAccessibleName(QStringLiteral("SWR gauge"));
    m_swrGauge = swrGauge;
    vbox->addWidget(swrGauge);

    // ── 3. RF Power slider row ───────────────────────────────────────────────
    // Label fixedWidth 62, value fixedWidth 22  (AetherSDR TxApplet.cpp:87–104)
    {
        auto* rfSlider = new QSlider(Qt::Horizontal, this);
        rfSlider->setRange(0, 100);
        rfSlider->setValue(100);
        rfSlider->setAccessibleName(QStringLiteral("RF power"));
        rfSlider->setObjectName(QStringLiteral("TxRfPowerSlider"));

        auto* rfValue = new QLabel(QStringLiteral("100"), this);
        rfValue->setFixedWidth(22);
        rfValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rfValue->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextPrimary));

        m_rfPowerSlider = rfSlider;
        m_rfPowerValue  = rfValue;

        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("RF Power:"), this);
        lbl->setFixedWidth(62);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTitleText));
        row->addWidget(lbl);

        rfSlider->setFixedHeight(18);
        rfSlider->setEnabled(true);   // Phase 3M-1a H.3: wired
        // Tooltip set by rescalePowerSlidersForModel() (Issue #175 Task 7) -
        // Thetis-faithful "Transmit Drive - relative value" wording, applied
        // every time the active SKU changes.  No initial setToolTip here.
        row->addWidget(rfSlider, 1);
        row->addWidget(rfValue);

        vbox->addLayout(row);
    }

    // ── 4. Tune Power slider row ─────────────────────────────────────────────
    // Label fixedWidth 62, value fixedWidth 22  (AetherSDR TxApplet.cpp:107–128)
    {
        auto* tunSlider = new QSlider(Qt::Horizontal, this);
        tunSlider->setRange(0, 100);
        tunSlider->setValue(10);
        tunSlider->setAccessibleName(QStringLiteral("Tune power"));

        auto* tunValue = new QLabel(QStringLiteral("10"), this);
        tunValue->setFixedWidth(22);
        tunValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        tunValue->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextPrimary));

        m_tunePwrSlider = tunSlider;
        m_tunePwrValue  = tunValue;

        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Tune Pwr:"), this);
        lbl->setFixedWidth(62);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTitleText));
        row->addWidget(lbl);

        tunSlider->setFixedHeight(18);
        tunSlider->setEnabled(true);  // Phase 3M-1a H.3: wired
        // Tooltip set by rescalePowerSlidersForModel() (Issue #175 Task 7) -
        // Thetis-faithful "Tune and/or 2Tone Drive - relative value" wording,
        // applied every time the active SKU changes.  No initial setToolTip
        // here.
        row->addWidget(tunSlider, 1);
        row->addWidget(tunValue);

        vbox->addLayout(row);
    }

    // ── Button row: TUNE + MOX (50% each) ──────────────────────────────────
    // Positioned above VOX+MON for action-button prominence
    // (docs/superpowers/plans/2026-05-01-ui-polish-right-panel.md §Task 5).
    // ATU + MEM removed (ATU phase, no plan yet; MEM = channel-memory phase).
    // MOX: red active (#cc2222 bg, #ff4444 border, white text)
    // TUNE: red active when tuning, text becomes "TUNING..."
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(2);

        const QString btnStyle = Style::buttonBaseStyle()
            + QStringLiteral("QPushButton { padding: 2px; }");
        const QString redChecked = Style::redCheckedStyle();

        m_tuneBtn = new QPushButton(QStringLiteral("TUNE"), this);
        m_tuneBtn->setCheckable(true);
        m_tuneBtn->setFixedHeight(22);
        m_tuneBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_tuneBtn->setStyleSheet(btnStyle + redChecked);
        m_tuneBtn->setEnabled(true);  // Phase 3M-1a H.3: wired
        m_tuneBtn->setAccessibleName(QStringLiteral("Tune carrier"));
        m_tuneBtn->setToolTip(QStringLiteral("Enable TUNE carrier (single-tone CW)"));
        row->addWidget(m_tuneBtn, 1);

        m_moxBtn = new QPushButton(QStringLiteral("MOX"), this);
        m_moxBtn->setCheckable(true);
        m_moxBtn->setFixedHeight(22);
        m_moxBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_moxBtn->setStyleSheet(btnStyle + redChecked);
        m_moxBtn->setEnabled(true);  // Phase 3M-1a H.3: wired
        m_moxBtn->setAccessibleName(QStringLiteral("MOX transmit"));
        m_moxBtn->setToolTip(QStringLiteral("Manual transmit (MOX)"));
        row->addWidget(m_moxBtn, 1);

        vbox->addLayout(row);

        // Slice control plan Task 11 (U8): which slice transmits, one letter
        // per slice this window controls; the checked one is the transmit
        // slice (refreshTransmitSliceChoices fills it).
        m_txSliceRow = new QHBoxLayout;
        m_txSliceRow->setSpacing(2);
        vbox->addLayout(m_txSliceRow);

        // Fix wave I4: who holds transmit on the Core, in a remote window
        // (setTransmitHolderText). Empty, and so not shown, otherwise.
        m_holderLabel = new QLabel(this);
        m_holderLabel->setObjectName(QStringLiteral("TxHolderLabel"));
        m_holderLabel->setAccessibleName(QStringLiteral("Who holds transmit"));
        m_holderLabel->setWordWrap(true);
        m_holderLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextPrimary));
        m_holderLabel->setVisible(false);
        vbox->addWidget(m_holderLabel);

        // iPhone app plan Task 78 (R-IOS-02, R-IOS-30): beside the line
        // that says another device (or the radio) holds transmit, the way
        // to take it back; the pan's TX pill offers the same.
        m_takeTransmitBtn = new QPushButton(QStringLiteral("Take transmit"), this);
        m_takeTransmitBtn->setObjectName(QStringLiteral("TxTakeTransmitButton"));
        m_takeTransmitBtn->setAccessibleName(QStringLiteral("Take transmit"));
        m_takeTransmitBtn->setToolTip(
            QStringLiteral("Ask to take transmit from the device that has it, so MOX and "
                           "TUNE work here."));
        m_takeTransmitBtn->setVisible(false);
        connect(m_takeTransmitBtn, &QPushButton::clicked, this,
                &TxApplet::takeTransmitRequested);
        vbox->addWidget(m_takeTransmitBtn);
    }

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
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_voxBtn = new QPushButton(QStringLiteral("VOX"), this);
        m_voxBtn->setCheckable(true);
        m_voxBtn->setFixedWidth(48);
        m_voxBtn->setFixedHeight(22);
        m_voxBtn->setStyleSheet(Style::buttonBaseStyle()
                                + Style::greenCheckedStyle());
        m_voxBtn->setAccessibleName(QStringLiteral("VOX voice-operated transmit"));
        m_voxBtn->setObjectName(QStringLiteral("TxVoxButton"));
        m_voxBtn->setToolTip(QStringLiteral(
            "VOX: voice-operated transmit.  Left-click to toggle.\n"
            "Right-click to open the DEXP/VOX setup page."));
        // CustomContextMenu so right-click hits the openSetupRequested slot
        // instead of the default platform menu.
        m_voxBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        row->addWidget(m_voxBtn);

        // Threshold slider-stack: slider on top, DexpPeakMeter strip below.
        auto* voxStackGroup = new QWidget(this);
        auto* voxStackVbox = new QVBoxLayout(voxStackGroup);
        voxStackVbox->setContentsMargins(0, 0, 0, 0);
        voxStackVbox->setSpacing(1);

        m_voxSlider = new QSlider(Qt::Horizontal, voxStackGroup);
        // From Thetis console.Designer.cs:6018-6019 [v2.10.3.13]:
        //   ptbVOX.Maximum = 0; ptbVOX.Minimum = -80;
        m_voxSlider->setRange(-80, 0);
        m_voxSlider->setValue(-20);
        m_voxSlider->setFixedHeight(14);
        m_voxSlider->setStyleSheet(Style::sliderHStyle());
        m_voxSlider->setAccessibleName(QStringLiteral("VOX threshold (dB)"));
        m_voxSlider->setObjectName(QStringLiteral("TxVoxThresholdSlider"));
        voxStackVbox->addWidget(m_voxSlider);

        m_voxPeakMeter = new DexpPeakMeter(voxStackGroup);
        m_voxPeakMeter->setObjectName(QStringLiteral("TxVoxPeakMeter"));
        m_voxPeakMeter->setAccessibleName(QStringLiteral("VOX live mic peak"));
        voxStackVbox->addWidget(m_voxPeakMeter);

        row->addWidget(voxStackGroup, 1);

        m_voxLvlLabel = new QLabel(QStringLiteral("-20 dB"), this);
        m_voxLvlLabel->setStyleSheet(Style::insetValueStyle());
        m_voxLvlLabel->setFixedWidth(38);
        m_voxLvlLabel->setAlignment(Qt::AlignCenter);
        row->addWidget(m_voxLvlLabel);

        m_voxDlySlider = new QSlider(Qt::Horizontal, this);
        // From Thetis setup.designer.cs:45005-45013 [v2.10.3.13]:
        //   udDEXPHold.Maximum = 2000; udDEXPHold.Minimum = 1; (units: ms)
        m_voxDlySlider->setRange(1, 2000);
        m_voxDlySlider->setValue(500);
        m_voxDlySlider->setStyleSheet(Style::sliderHStyle());
        m_voxDlySlider->setAccessibleName(QStringLiteral("VOX hold time (ms)"));
        m_voxDlySlider->setObjectName(QStringLiteral("TxVoxHoldSlider"));
        row->addWidget(m_voxDlySlider, 1);

        m_voxDlyLabel = new QLabel(QStringLiteral("500 ms"), this);
        m_voxDlyLabel->setStyleSheet(Style::insetValueStyle());
        m_voxDlyLabel->setFixedWidth(42);
        m_voxDlyLabel->setAlignment(Qt::AlignCenter);
        row->addWidget(m_voxDlyLabel);

        vbox->addLayout(row);
    }

    // ── 4c. MON toggle button + 4d. Monitor volume slider ─────────────────────
    // Phase 3M-1b J.3: below VOX toggle.
    // MON: checkable, blue border when active (indicates monitor on).
    //   monEnabled does NOT persist — plan §0 row 9 safety: loads OFF always.
    //   Default volume 50 (matches model default 0.5f from Thetis audio.cs:417).
    //
    // Volume slider: 0..100 integer → monitorVolume float 0.0..1.0 (value/100.0f).
    //   Inverse: monitorVolumeChanged(float) → slider position = qRound(v * 100.0f).
    {
        // MON button row
        auto* monRow = new QHBoxLayout;
        monRow->setSpacing(4);

        m_monBtn = new QPushButton(QStringLiteral("MON"), this);
        m_monBtn->setCheckable(true);
        m_monBtn->setChecked(false);  // default: OFF — plan §0 row 9 safety rule
        m_monBtn->setFixedHeight(22);
        m_monBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        // MON button intentionally uses dark-navy/cyan (#001a33 bg / #3399ff border)
        // to distinguish from generic blue toggles (kBlueBg=#0070c0/kBlueBorder=#0090e0).
        // NereusSDR-original one-off — do NOT snap to Style::kBlueBg / kBlueBorder.
        m_monBtn->setStyleSheet(Style::buttonBaseStyle()
            + QStringLiteral("QPushButton:checked {"
                             " background: #001a33;"
                             " border: 1px solid #3399ff;"
                             " color: #ffffff;"
                             "}"));
        m_monBtn->setAccessibleName(QStringLiteral("Monitor enable"));
        // R-R3-21: MON plays your own transmitted audio as it sounds on the
        // air, in the output chosen beside it (R-R3-45).
        m_monBtn->setToolTip(QStringLiteral(
            "Monitor: hear your own transmitted audio as it sounds on the air, "
            "on the output chosen next to MON (SPEAKERS or PHONES).\n"
            "MON is off each time NereusSDR starts, for safety."));
        monRow->addWidget(m_monBtn, 1);

        // R-R3-45: where MON plays, beside MON. An exclusive pair in the
        // LEV / EQ / CFC button family; speakers by default. NereusSDR-native:
        // Thetis mixes MON into its one output.
        const QString outStyle = Style::buttonBaseStyle()
            + Style::greenCheckedStyle();
        m_monSpeakersBtn = new QPushButton(monitorSpeakersCaption(), this);
        m_monSpeakersBtn->setObjectName(QStringLiteral("TxMonitorSpeakersButton"));
        m_monSpeakersBtn->setCheckable(true);
        m_monSpeakersBtn->setChecked(true);
        m_monSpeakersBtn->setFixedHeight(22);
        m_monSpeakersBtn->setStyleSheet(outStyle);
        m_monSpeakersBtn->setAccessibleName(QStringLiteral("Monitor on the speakers"));
        m_monSpeakersBtn->setToolTip(QStringLiteral(
            "Play your transmit monitor on the speakers"));
        monRow->addWidget(m_monSpeakersBtn);

        m_monHeadphonesBtn = new QPushButton(monitorHeadphonesCaption(), this);
        m_monHeadphonesBtn->setObjectName(QStringLiteral("TxMonitorHeadphonesButton"));
        m_monHeadphonesBtn->setCheckable(true);
        m_monHeadphonesBtn->setFixedHeight(22);
        m_monHeadphonesBtn->setStyleSheet(outStyle);
        m_monHeadphonesBtn->setAccessibleName(QStringLiteral("Monitor on the headphones"));
        m_monHeadphonesBtn->setToolTip(QStringLiteral(
            "Play your transmit monitor on the headphones"));
        monRow->addWidget(m_monHeadphonesBtn);

        vbox->addLayout(monRow);

        // Why MON is silent with the headphones chosen and none open: the
        // receiver flag's own words.
        m_monOutputNotice = new QLabel(VfoWidget::headphonesMissingText(), this);
        m_monOutputNotice->setObjectName(QStringLiteral("TxMonitorOutputNotice"));
        m_monOutputNotice->setWordWrap(true);
        m_monOutputNotice->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-size: 10px; }").arg(Style::kAmberText));
        m_monOutputNotice->setVisible(false);
        vbox->addWidget(m_monOutputNotice);

        // Monitor volume slider row
        auto* volRow = new QHBoxLayout;
        volRow->setSpacing(4);

        auto* volLbl = new QLabel(QStringLiteral("Mon Vol:"), this);
        volLbl->setFixedWidth(62);
        volLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTitleText));
        volRow->addWidget(volLbl);

        // Range 0..100 integer; default 50 (model default 0.5f).
        m_monitorVolumeSlider = new QSlider(Qt::Horizontal, this);
        m_monitorVolumeSlider->setRange(0, 100);
        m_monitorVolumeSlider->setValue(50);
        m_monitorVolumeSlider->setFixedHeight(18);
        m_monitorVolumeSlider->setAccessibleName(QStringLiteral("Monitor volume"));
        // R-R3-21: sets the TX monitor's gain (TransmitModel::monitorVolume
        // -> AudioEngine::setTxMonitorVolume), the transmitted audio MON
        // plays in the output chosen beside it (R-R3-45).
        m_monitorVolumeSlider->setToolTip(QStringLiteral(
            "How loud you hear your own transmitted audio while MON is on, "
            "on the output chosen next to MON (SPEAKERS or PHONES). 0 to 100."));
        volRow->addWidget(m_monitorVolumeSlider, 1);

        m_monitorVolumeValue = new QLabel(QStringLiteral("50"), this);
        m_monitorVolumeValue->setFixedWidth(26);
        m_monitorVolumeValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_monitorVolumeValue->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextPrimary));
        volRow->addWidget(m_monitorVolumeValue);

        vbox->addLayout(volRow);
    }

    // ── 4e. TX-processing quick toggles: [LEV] [EQ] [CFC] ──────────────────
    // Phase 3M-3a-i Batch 2 (Task F): introduced the row of 3 (LEV/EQ/PROC).
    // Phase 3M-3a-ii Batch 6 (Task A + F, then post-bench cleanup):
    // PROC was promoted in Batch 6, then dropped here in the cleanup pass —
    // PhoneCwApplet already had a wired PROC button + slider sitting un-wired
    // since 3I-3 (NyiOverlay-marked).  Two PROC controls confused users.
    // Row is now 3 buttons (LEV / EQ / CFC); PROC lives on PhoneCwApplet.
    // All three share the same VOX/MON styling family (compact 22-px-tall,
    // expanding width, green-checked LED look).
    //
    //   LEV  — checkable, bidirectional with TransmitModel::txLevelerOn.
    //   EQ   — checkable, bidirectional with TransmitModel::txEqEnabled.
    //          Left-click toggles.  Right-click → TxEqDialog (3M-3a-i Batch 3).
    //   CFC  — checkable, bidirectional with TransmitModel::cfcEnabled.
    //          Left-click toggles.  Right-click → modeless TxCfcDialog
    //          (10-band per-band CFC editor; mirrors Thetis frmCFCConfig
    //          [v2.10.3.13]).
    //
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(2);

        const QString btnStyle = Style::buttonBaseStyle()
            + Style::greenCheckedStyle();

        m_levBtn = new QPushButton(QStringLiteral("LEV"), this);
        m_levBtn->setCheckable(true);
        m_levBtn->setFixedHeight(22);
        m_levBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_levBtn->setStyleSheet(btnStyle);
        m_levBtn->setAccessibleName(QStringLiteral("TX Leveler enable"));
        m_levBtn->setObjectName(QStringLiteral("TxLevButton"));
        m_levBtn->setToolTip(QStringLiteral(
            "TX Leveler: slow speech-leveling AGC. Improves intelligibility on weak speech."));
        row->addWidget(m_levBtn, 1);

        m_eqBtn = new QPushButton(QStringLiteral("EQ"), this);
        m_eqBtn->setCheckable(true);
        m_eqBtn->setFixedHeight(22);
        m_eqBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_eqBtn->setStyleSheet(btnStyle);
        m_eqBtn->setAccessibleName(QStringLiteral("TX EQ enable"));
        m_eqBtn->setObjectName(QStringLiteral("TxEqButton"));
        m_eqBtn->setToolTip(QStringLiteral(
            "TX 10-band graphic EQ.  Left-click to toggle.\n"
            "Right-click to open the EQ dialog."));
        // Custom context-menu policy so right-click hits a slot (not the
        // default menu).
        m_eqBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        row->addWidget(m_eqBtn, 1);

        m_cfcBtn = new QPushButton(QStringLiteral("CFC"), this);
        m_cfcBtn->setCheckable(true);
        m_cfcBtn->setFixedHeight(22);
        m_cfcBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_cfcBtn->setStyleSheet(btnStyle);
        m_cfcBtn->setAccessibleName(QStringLiteral("Continuous Frequency Compressor enable"));
        m_cfcBtn->setObjectName(QStringLiteral("TxCfcButton"));
        m_cfcBtn->setToolTip(QStringLiteral(
            "CFC: 10-band continuous frequency compressor. Left-click to "
            "toggle. Right-click to open the CFC dialog."));
        // Right-click → modeless TxCfcDialog (mirrors EQ button pattern).
        m_cfcBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        row->addWidget(m_cfcBtn, 1);

        vbox->addLayout(row);
    }

    // ── Profile combo row (full width) ──────────────────────────────────────
    // Tune Mode combo removed (ATU phase, no plan yet).
    // (AetherSDR TxApplet.cpp:131–153)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(2);

        // ── Phase 3M-1c J.1 ─ TX Profile combo ─────────────────────────────────
        // Populated from MicProfileManager via setMicProfileManager().
        // Right-click → emit txProfileMenuRequested (mirrors Thetis
        // comboTXProfile_MouseDown at console.cs:44519-44522 [v2.10.3.13]).
        m_profileCombo = new QComboBox(this);
        m_profileCombo->addItem(QStringLiteral("Default"));
        m_profileCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_profileCombo->setFixedHeight(22);
        applyComboStyle(m_profileCombo);
        m_profileCombo->setAccessibleName(QStringLiteral("TX profile"));
        m_profileCombo->setToolTip(QStringLiteral(
            "TX Profile: left-click to switch.  Right-click to edit "
            "(Setup → Audio → TX Profile)."));
        // Custom context-menu policy so right-click emits
        // customContextMenuRequested instead of the default popup.
        m_profileCombo->setContextMenuPolicy(Qt::CustomContextMenu);
        row->addWidget(m_profileCombo, 1);

        vbox->addLayout(row);
    }

    // ── Plan 4 Cluster C (Task 4 / D2+D3+D9-status): TX BW spinbox row ──────
    // Low/High cutoff spinboxes for the TX bandpass filter.  Bidirectional with
    // TransmitModel::filterLow / filterHigh (wired in wireControls).  Defaults
    // 100 / 2900 Hz from the model (m_filterLow{100} / m_filterHigh{2900}).
    {
        auto* bwRow = new QHBoxLayout;
        bwRow->setSpacing(4);

        // "TX BW" section label — 56 px wide, bold, kTitleText colour, 10 px.
        auto* bwLbl = new QLabel(QStringLiteral("TX BW"), this);
        bwLbl->setFixedWidth(56);
        bwLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; font-weight: bold; }"
        ).arg(Style::kTitleText));
        bwRow->addWidget(bwLbl);

        // "Lo" sub-label
        auto* loLbl = new QLabel(QStringLiteral("Lo"), this);
        loLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 9.5px; }"
        ).arg(Style::kTextSecondary));
        bwRow->addWidget(loLbl);

        // Low-cutoff spinbox — range [0, 5000] Hz
        m_txFilterLowSpin = new QSpinBox(this);
        m_txFilterLowSpin->setRange(0, 5000);
        m_txFilterLowSpin->setSuffix(QStringLiteral(" Hz"));
        m_txFilterLowSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
        m_txFilterLowSpin->setMinimumWidth(72);
        m_txFilterLowSpin->setAccessibleName(QStringLiteral("TX filter low cutoff"));
        m_txFilterLowSpin->setToolTip(QStringLiteral(
            "TX bandpass filter lower cutoff (Hz).  0 Hz for voice SSB modes."));
        bwRow->addWidget(m_txFilterLowSpin);

        // "Hi" sub-label
        auto* hiLbl = new QLabel(QStringLiteral("Hi"), this);
        hiLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 9.5px; }"
        ).arg(Style::kTextSecondary));
        bwRow->addWidget(hiLbl);

        // High-cutoff spinbox — range [200, 10000] Hz
        m_txFilterHighSpin = new QSpinBox(this);
        m_txFilterHighSpin->setRange(200, 10000);
        m_txFilterHighSpin->setSuffix(QStringLiteral(" Hz"));
        m_txFilterHighSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
        m_txFilterHighSpin->setMinimumWidth(72);
        m_txFilterHighSpin->setAccessibleName(QStringLiteral("TX filter high cutoff"));
        m_txFilterHighSpin->setToolTip(QStringLiteral(
            "TX bandpass filter upper cutoff (Hz).  2900 Hz for typical SSB voice."));
        bwRow->addWidget(m_txFilterHighSpin);

        vbox->addLayout(bwRow);

        // D9 status label — orange tint, right-aligned, 9 px bold.
        // Displays e.g. "100-2900 Hz · 2.8k BW" (asymmetric) or "±2900 Hz · 5.8k BW"
        // (symmetric modes).  Refreshed by filterChanged + dspModeChanged.
        m_txFilterStatusLabel = new QLabel(this);
        m_txFilterStatusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_txFilterStatusLabel->setStyleSheet(QStringLiteral(
            // Plan 4 D9 (Cluster E): colour centralised in Style::kTxFilterOverlayLabel.
            "QLabel { color: %1; font-size: 9px; font-weight: bold; }"
        ).arg(QLatin1String(Style::kTxFilterOverlayLabel)));
        m_txFilterStatusLabel->setAccessibleName(QStringLiteral("TX filter status"));
        vbox->addWidget(m_txFilterStatusLabel);
    }

    // ── Button row 3: 2-Tone + PS-A + DUP ───────────────────────────────────
    // Phase 3M-1a H.3: 2-Tone and PS-A are hidden until their owning phases land.
    //   2-Tone: TODO [3M-3]: visible when 2-tone test feature lands.
    //   PS-A:   TODO [3M-4]: visible when PureSignal lands.
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(2);

        // ── Phase 3M-1c J.2 ─ 2-TONE button ────────────────────────────────────
        // Mirrors Thetis chk2TONE_CheckedChanged (console.cs:44728-44760
        // [v2.10.3.13]).  Wired to TwoToneController via
        // setTwoToneController().  The TUN-stop pre-step + 300 ms settle
        // delay live inside TwoToneController::setActive (Phase I.3) so the
        // button itself just dispatches setActive(checked).
        m_twoToneBtn = new QPushButton(QStringLiteral("2-Tone"), this);
        m_twoToneBtn->setCheckable(true);
        m_twoToneBtn->setFixedHeight(22);
        m_twoToneBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_twoToneBtn->setStyleSheet(Style::buttonBaseStyle() + Style::redCheckedStyle());
        m_twoToneBtn->setAccessibleName(QStringLiteral("2-tone test"));
        m_twoToneBtn->setToolTip(QStringLiteral(
            "Continuous or pulsed two-tone IMD test "
            "(configure in Setup → Test → Two-Tone)."));
        row->addWidget(m_twoToneBtn, 1);

        // PS-A: green when checked — #006030/#008040 are a darker green than kGreenBg=#006040.
        // Phase 3M-4 Task 13: un-hidden under capability gating.  Visibility
        // is set by setBoardCapabilities() / MainWindow board-change handler;
        // hidden by default until that fires (matches the Phase 3M-1a comment
        // pattern of hidden-until-capability-known).
        m_psaBtn = new QPushButton(QStringLiteral("PS-A"), this);
        m_psaBtn->setObjectName(QStringLiteral("TxAppletPsaBtn"));
        m_psaBtn->setCheckable(true);
        m_psaBtn->setFixedHeight(22);
        m_psaBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_psaBtn->setStyleSheet(Style::buttonBaseStyle()
            + QStringLiteral("QPushButton:checked {"
                             " background: #006030; border: 1px solid #008040; color: #fff; }"));
        m_psaBtn->setAccessibleName(QStringLiteral("PS-A PureSignal"));
        m_psaBtn->setToolTip(QStringLiteral(
            "Toggle PureSignal auto-calibration. Right-click to open PureSignal..."));
        // Phase 3M-4 bench-fix: default visible so the button shows on every
        // PS-capable board even if MainWindow's capability gate fires after
        // applet construction (timing race observed at bench).  setBoardCapabilities
        // (TxApplet.cpp:1985) hides for caps.hasPureSignal=false (Atlas/HL2).
        row->addWidget(m_psaBtn, 1);

        vbox->addLayout(row);
    }

    // ── SWR protection LED inset ─────────────────────────────────────────────
    // xPA button removed (external-PA hardware-specific phase, no plan yet).
    // SWR Prot LED now occupies its own full-width inset row.
    // Inset: fixedHeight 22, bg #0a0a18, border #1e2e3e (AetherSDR TxApplet.cpp:224–253)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        // Inset container for SWR protection LED (styled like AetherSDR atuInset)
        auto* inset = new QWidget(this);
        inset->setFixedHeight(22);
        inset->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        inset->setObjectName(QStringLiteral("xpaInset"));
        inset->setStyleSheet(QStringLiteral(
            "#xpaInset { background: %1; border: 1px solid %2; border-radius: 3px; }"
            "#xpaInset QLabel { border: none; background: transparent; }"
        ).arg(Style::kInsetBg, Style::kInsetBorder));

        auto* insetLayout = new QHBoxLayout(inset);
        insetLayout->setContentsMargins(4, 0, 4, 0);
        insetLayout->setSpacing(2);

        // 15. SWR protection LED — inactive: #405060, 9px bold
        // Matches AetherSDR makeIndicator() pattern (TxApplet.cpp:22–27)
        m_swrProtLed = new QLabel(QStringLiteral("SWR Prot"), inset);
        m_swrProtLed->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 9px; font-weight: bold; }"
        ).arg(Style::kTextInactive));
        m_swrProtLed->setAlignment(Qt::AlignCenter);
        m_swrProtLed->setAccessibleName(QStringLiteral("SWR protection indicator"));
        insetLayout->addWidget(m_swrProtLed);

        row->addWidget(inset, 1); // full width (xPA removed)
        vbox->addLayout(row);
    }

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
    //   1.  alpha=0.10 EMA on every powerChanged emit — smooths the per-
    //       sample noise without much lag (~10 sample time-constant).
    //   2.  10 Hz QTimer reads the smoothed state into the gauge — keeps
    //       the visible digits stable even when RadioStatus::powerChanged
    //       fires twice per hardware sample (documented in
    //       RadioModel.cpp:572).  Without this throttle the digit
    //       characters update too fast to read.
    connect(&m_model->radioStatus(), &RadioStatus::powerChanged,
            this, [this](double fwdW, double /*revW*/, double swr) {
        constexpr double kAlpha = 0.30;
        m_fwdPowerSmoothedW = kAlpha * fwdW + (1.0 - kAlpha) * m_fwdPowerSmoothedW;
        if (m_swrGauge) {
            m_swrGauge->setValue(swr);
        }
    });
    auto* fwdGaugeRefreshTimer = new QTimer(this);
    fwdGaugeRefreshTimer->setInterval(50);   // 20 Hz UI refresh
    connect(fwdGaugeRefreshTimer, &QTimer::timeout, this, [this]() {
        if (m_fwdPowerGauge) {
            m_fwdPowerGauge->setValue(m_fwdPowerSmoothedW);
        }
    });
    fwdGaugeRefreshTimer->start();

    // Bench-reported #167 follow-up: TxApplet's RF Pwr / SWR HGauges have
    // their own EMA smoothing (m_fwdPowerSmoothedW, alpha=0.30) separate
    // from the MeterPanel's BarItem stack.  A single zero from
    // RadioStatus::powerChanged after un-key only walks the smoothed
    // value down by 30% (e.g. 60 → 42), then no more updates flow so the
    // gauge stays stuck at ~42.  Snap the smoothed state to 0 on MOX
    // falling-edge so the gauge clears instantly — same idea as the
    // BarItem clearSmoothing path in MeterPoller::setInTx.  Subscribed
    // to MoxController::moxStateChanged (the authoritative TX boundary,
    // covers MOX un-key AND TUNE release) rather than orphan
    // TransmitModel::moxChanged signals.
    if (m_model && m_model->moxController()) {
        connect(m_model->moxController(), &MoxController::moxStateChanged,
                this, [this](bool active) {
            if (active) { return; }      // rising-edge: smoothing takes over
            m_fwdPowerSmoothedW = 0.0;
            if (m_fwdPowerGauge) { m_fwdPowerGauge->setValue(0.0); }
            if (m_swrGauge)      { m_swrGauge->setValue(1.0); }
        });
    }
    // R-R3-49 (parity Task 33): a remote window's MoxController never keys;
    // the Core's unkey (its mirrored transmit state) clears the two bars
    // the same way.
    if (m_model && m_model->role() == RadioModel::Role::Remote) {
        connect(m_model, &RadioModel::transmittingChanged, this, [this](bool active) {
            if (active) { return; }
            m_fwdPowerSmoothedW = 0.0;
            if (m_fwdPowerGauge) { m_fwdPowerGauge->setValue(0.0); }
            if (m_swrGauge)      { m_swrGauge->setValue(1.0); }
        });
    }

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
    // Apply the current model's scale at wireControls() time so cold-launch
    // (or reconnect-on-startup) lands a properly-sized gauge + sliders before
    // the first telemetry sample.
    {
        const auto model = m_model->hardwareProfile().model;
        rescaleFwdGaugeForModel(model);
        rescalePowerSlidersForModel(model);
    }

    // ── SWR Prot LED ← SwrProtectionController::highSwrChanged ─────────────
    // SwrProtectionController (Phase 3G-13) emits highSwrChanged(bool) when
    // the radio's SWR-protection state changes. Light the LED amber when
    // high-SWR protection is active; dim it when cleared.
    {
        auto updateSwrProtLed = [this](bool isHigh) {
            if (!m_swrProtLed) { return; }
            const QString color = isHigh ? QStringLiteral("#ffaa00")
                                         : Style::kTextInactive;
            m_swrProtLed->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; font-size: 9px; font-weight: bold; }"
            ).arg(color));
        };
        connect(&m_model->swrProt(),
                &safety::SwrProtectionController::highSwrChanged,
                this, updateSwrProtLed);
        // Initialise to current state.
        updateSwrProtLed(m_model->swrProt().highSwr());
    }

    // ── RF Power slider → TransmitModel::setPower(int) ──────────────────────
    // From Thetis chkMOX_CheckedChanged2 power flow [v2.10.3.13]:
    //   the RF Power slider maps 0–100 to TX drive level.
    //
    // Issue #175 Task 7: label text is now driven by updatePowerSliderLabels()
    // which formats the value in -X.X dB on HL2 and bare integer on every
    // other SKU.  The old direct setText(QString::number(val)) would have
    // bypassed the dB conversion on HL2.
    connect(m_rfPowerSlider, &QSlider::valueChanged, this, [this, &tx](int val) {
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
        if (m_model && m_model->role() == RadioModel::Role::Local) {
            tx.setTuneDrivePowerSource(DrivePowerSource::DriveSlider);
            return;
        }
        // R-R3-49 (group A fix wave, M3): a Core below
        // transmitSettingsVersion 5 refuses tuneDrivePowerSource and takes
        // `power` alone.
        if (!m_powerByBandPermitted) { return; }
        // Symmetric to the tune-slider auto-switch above: touching the RF
        // Power slider restores the tune source to DriveSlider so the
        // setPowerUsingTargetDbm txMode 1 branch reads tx.power() during
        // TUNE.  Last-touched-slider-wins UX. From Thetis console.cs:46553
        // [v2.10.3.13] DrivePowerSource.DRIVE_SLIDER is the canonical default.
        tx.setTuneDrivePowerSource(DrivePowerSource::DriveSlider);
    });

    // Reverse: TransmitModel::powerChanged → slider
    connect(&tx, &TransmitModel::powerChanged, this, [this](int power) {
        QSignalBlocker b(m_rfPowerSlider);
        m_updatingFromModel = true;
        m_rfPowerSlider->setValue(power);
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
    connect(m_tunePwrSlider, &QSlider::valueChanged, this, [this, &tx](int val) {
        updatePowerSliderLabels();
        if (m_updatingFromModel) { return; }
        // R-R3-49 (parity Task 2): a remote window asks the Core, which
        // does what the lines below do for its own transmit band. A drag
        // asks once, on release.
        if (remoteTunePower()) {
            if (!m_tunePwrSlider->isSliderDown()) {
                requestRemoteTunePower(val);
            }
            return;
        }
        if (tx.setTunePowerForTxBand(val)) { return; }
        tx.setTunePowerForBand(m_currentBand, val);
        // When the user touches the tune slider, switch the tune drive
        // source so TUNE actually reads from tunePowerForBand instead of
        // the regular drive slider (the default per Thetis console.cs:46553
        // [v2.10.3.13]).  Without this the tune slider is dead UI: its
        // value persists per-band but the math kernel
        // (TransmitModel::setPowerUsingTargetDbm txMode 1) only consults
        // tunePowerForBand when m_tuneDrivePowerSource == TuneSlider.
        // NereusSDR-spin: Thetis exposes _tuneDrivePowerSource via a
        // separate Setup combo; NereusSDR follows last-touched-slider-wins
        // UX so users don't need to know the enum exists.
        tx.setTuneDrivePowerSource(DrivePowerSource::TuneSlider);
    });

    connect(m_tunePwrSlider, &QSlider::sliderReleased, this, [this]() {
        if (m_updatingFromModel || !remoteTunePower()) { return; }
        requestRemoteTunePower(m_tunePwrSlider->value());
    });

    // Fix round 1 (minor 2): the change answered, the slider shows the
    // Core's value, whichever arrived first, its answer or its next value
    // (or a value it clamped). Not while the operator holds the slider.
    connect(&tx, &TransmitModel::tunePowerForTxBandWriteInFlightChanged,
            this, [this, &tx](bool inFlight) {
        if (inFlight || !remoteTunePower() || m_tunePwrSlider->isSliderDown()) { return; }
        QSignalBlocker b(m_tunePwrSlider);
        m_updatingFromModel = true;
        m_tunePwrSlider->setValue(tx.tunePowerForTxBand());
        updatePowerSliderLabels();
        m_updatingFromModel = false;
    });

    // R-R3-49 (parity Task 2): in a remote window the slider shows the
    // Core's tune power for its transmit band; a local window shows its own
    // transmit band's (PA on-air gate review).
    connect(&tx, &TransmitModel::tunePowerForTxBandChanged,
            this, [this, &tx](int watts) {
        if (!remoteTunePower() && !tx.tuneTxBandKnown()) { return; }
        // Fix wave GUI-I6: the Core's value does not move the slider out
        // from under the operator's hand, nor back to an older value while
        // the operator's change is on its way (the Core's next value, after
        // its answer, shows; a refusal shows the Core's value again).
        if (remoteTunePower()
            && (m_tunePwrSlider->isSliderDown() || tx.tunePowerForTxBandWriteInFlight())) {
            return;
        }
        QSignalBlocker b(m_tunePwrSlider);
        m_updatingFromModel = true;
        m_tunePwrSlider->setValue(watts);
        updatePowerSliderLabels();
        m_updatingFromModel = false;
    });

    // Reverse: TransmitModel::tunePowerByBandChanged → slider (only for
    // current band, and only before the transmit band is known; after that
    // tunePowerForTxBandChanged above repaints it).
    connect(&tx, &TransmitModel::tunePowerByBandChanged,
            this, [this, &tx](Band band, int watts) {
        if (band != m_currentBand || remoteTunePower() || tx.tuneTxBandKnown()) { return; }
        QSignalBlocker b(m_tunePwrSlider);
        m_updatingFromModel = true;
        m_tunePwrSlider->setValue(watts);
        updatePowerSliderLabels();   // Issue #175 Task 7: HL2 dB / non-HL2 int
        m_updatingFromModel = false;
    });

    // ── TUNE button → RadioModel::setTune(bool) ──────────────────────────────
    // G.4 orchestrator: CW mode swap, tone setup, tune-power push.
    // From Thetis console.cs:29978-30157 [v2.10.3.13] chkTUN_CheckedChanged.
    // G8NJJ tell ARIES that tune is active  [original inline comment from console.cs:30153]
    // MW0LGE_22b setupTuneDriveSlider  [original inline comment from console.cs:30155]
    connect(m_tuneBtn, &QPushButton::toggled, this, [this](bool on) {
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
                QSignalBlocker b(m_tuneBtn);
                m_tuneBtn->setChecked(asked);
            }
            m_model->setTune(asked);
        }
    });

    // Reverse: tuneRefused → uncheck TUN button + clear text.
    // From Thetis console.cs:30076 [v2.10.3.13]: guard conditions before
    // chkTUN.Checked = true (connection + power-on checks).
    connect(m_model, &RadioModel::tuneRefused, this, [this](const QString& /*reason*/) {
        if (m_desktopTuneOn) { syncDesktopKeyState(); return; }
        QSignalBlocker b(m_tuneBtn);
        m_updatingFromModel = true;
        m_tuneBtn->setChecked(false);
        m_tuneBtn->setText(QStringLiteral("TUNE"));
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
        connect(m_moxBtn, &QPushButton::toggled, this, [this](bool on) {
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
                    QSignalBlocker b(m_moxBtn);
                    m_moxBtn->setChecked(asked);
                }
                m_model->setMoxFromButton(asked);
            }
        });

        // Reverse: MoxController::moxStateChanged → button checked state.
        // moxStateChanged fires at end of the timer walk (TX fully engaged
        // or fully released), not at setMox() entry — so the button reflects
        // the confirmed state, not the in-progress request.
        connect(mox, &MoxController::moxStateChanged,
                this, [this](bool on) {
            if (m_desktopMoxOn) { syncDesktopKeyState(); return; }
            QSignalBlocker b(m_moxBtn);
            m_updatingFromModel = true;
            m_moxBtn->setChecked(on);
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
        connect(mox, &MoxController::moxRejected,
                this, [this, mox](const QString& /*reason*/) {
            if (m_desktopMoxOn) { syncDesktopKeyState(); return; }
            QSignalBlocker b(m_moxBtn);
            m_updatingFromModel = true;
            m_moxBtn->setChecked(mox->isMox());
            m_updatingFromModel = false;
        });

        // TUNE button checked state driven by manualMoxChanged.
        // manualMoxChanged fires when setTune() sets/clears m_manualMox.
        connect(mox, &MoxController::manualMoxChanged,
                this, [this](bool isManual) {
            if (m_desktopTuneOn) { syncDesktopKeyState(); return; }
            QSignalBlocker b(m_tuneBtn);
            m_updatingFromModel = true;
            m_tuneBtn->setChecked(isManual);
            m_tuneBtn->setText(isManual
                ? QStringLiteral("TUNING...")
                : QStringLiteral("TUNE"));
            m_updatingFromModel = false;
        });
    }

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
    // QTimer driving m_voxPeakMeter (TxApplet::pollVoxMeter).
    // Mirrors the wiring pattern from PhoneCwApplet (Phase 3M-3a-iii Task
    // 15) — relocated here as part of the 2026-05-04 bench polish.

    // ── VOX [ON] toggle ↔ TransmitModel::voxEnabled ──────────────────────────
    if (m_voxBtn) {
        {
            QSignalBlocker b(m_voxBtn);
            m_voxBtn->setChecked(tx.voxEnabled());
        }
        // UI → Model
        connect(m_voxBtn, &QPushButton::toggled, this, [this, &tx](bool on) {
            if (m_updatingFromModel) { return; }
            tx.setVoxEnabled(on);
        });
        // Model → UI
        connect(&tx, &TransmitModel::voxEnabledChanged, this, [this](bool on) {
            m_updatingFromModel = true;
            {
                QSignalBlocker b(m_voxBtn);
                m_voxBtn->setChecked(on);
            }
            m_updatingFromModel = false;
        });
    }

    // ── VOX threshold slider ↔ TransmitModel::voxThresholdDb ─────────────────
    // Range -80..0 dB from console.Designer.cs:6018-6019 [v2.10.3.13]
    // (already set in buildUI).  Default value -20 dB matches Thetis
    // ptbVOX.Value=-20 (console.Designer.cs:6024 [v2.10.3.13]).
    if (m_voxSlider) {
        {
            QSignalBlocker b(m_voxSlider);
            m_voxSlider->setValue(tx.voxThresholdDb());
        }
        if (m_voxLvlLabel) {
            m_voxLvlLabel->setText(QStringLiteral("%1 dB").arg(tx.voxThresholdDb()));
        }
        // UI → Model + label refresh
        connect(m_voxSlider, &QSlider::valueChanged, this, [this, &tx](int dB) {
            if (m_voxLvlLabel) {
                m_voxLvlLabel->setText(QStringLiteral("%1 dB").arg(dB));
            }
            if (m_updatingFromModel) { return; }
            tx.setVoxThresholdDb(dB);
        });
        // Model → UI
        connect(&tx, &TransmitModel::voxThresholdDbChanged, this, [this](int dB) {
            m_updatingFromModel = true;
            {
                QSignalBlocker b(m_voxSlider);
                m_voxSlider->setValue(dB);
            }
            if (m_voxLvlLabel) {
                m_voxLvlLabel->setText(QStringLiteral("%1 dB").arg(dB));
            }
            m_updatingFromModel = false;
        });
    }

    // ── VOX Hold slider ↔ TransmitModel::voxHangTimeMs ───────────────────────
    // Range 1..2000 ms from setup.designer.cs:45005-45013 [v2.10.3.13]
    // (already set in buildUI).  Default 500 ms matches udDEXPHold.Value
    // (setup.designer.cs:45020 [v2.10.3.13]).
    if (m_voxDlySlider) {
        {
            QSignalBlocker b(m_voxDlySlider);
            m_voxDlySlider->setValue(tx.voxHangTimeMs());
        }
        if (m_voxDlyLabel) {
            m_voxDlyLabel->setText(QStringLiteral("%1 ms").arg(tx.voxHangTimeMs()));
        }
        // UI → Model + label refresh
        connect(m_voxDlySlider, &QSlider::valueChanged, this, [this, &tx](int ms) {
            if (m_voxDlyLabel) {
                m_voxDlyLabel->setText(QStringLiteral("%1 ms").arg(ms));
            }
            if (m_updatingFromModel) { return; }
            tx.setVoxHangTimeMs(ms);
        });
        // Model → UI
        connect(&tx, &TransmitModel::voxHangTimeMsChanged, this, [this](int ms) {
            m_updatingFromModel = true;
            {
                QSignalBlocker b(m_voxDlySlider);
                m_voxDlySlider->setValue(ms);
            }
            if (m_voxDlyLabel) {
                m_voxDlyLabel->setText(QStringLiteral("%1 ms").arg(ms));
            }
            m_updatingFromModel = false;
        });
    }

    // ── Right-click on VOX → emit openSetupRequested ─────────────────────────
    // Mirrors PhoneCwApplet's DEXP-button right-click pattern.  MainWindow
    // listens and jumps the SetupDialog to the "DEXP/VOX" leaf page.
    if (m_voxBtn) {
        connect(m_voxBtn, &QPushButton::customContextMenuRequested, this,
                [this](const QPoint&) {
            emit openSetupRequested(QStringLiteral("Transmit"),
                                    QStringLiteral("DEXP/VOX"));
        });
    }

    // ── 100 ms timer driving m_voxPeakMeter ──────────────────────────────────
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

    // ── MON toggle button ↔ TransmitModel::monEnabled ────────────────────────
    // Phase 3M-1b J.3.
    // UI → Model: toggled → setMonEnabled (with m_updatingFromModel guard).
    // Model → UI: monEnabledChanged → update checked state with QSignalBlocker.
    connect(m_monBtn, &QPushButton::toggled, this, [this, &tx](bool on) {
        if (m_updatingFromModel) { return; }
        tx.setMonEnabled(on);
    });

    connect(&tx, &TransmitModel::monEnabledChanged, this, [this](bool on) {
        QSignalBlocker b(m_monBtn);
        m_updatingFromModel = true;
        m_monBtn->setChecked(on);
        m_updatingFromModel = false;
    });

    // ── R-R3-45: MON output ↔ AudioEngine::txMonitorOutput ──────────────────
    // This computer's choice: the audio engine that plays this window's
    // sound. R-R3-49 (parity Task 2): a remote window too, where it is this
    // computer's own routing (a window-scope setting), as in a local window;
    // it follows setTransmitChainSettingsPermitted there.
    {
        if (AudioEngine* engine = m_model->localAudioDevices()) {
            connect(m_monSpeakersBtn, &QPushButton::clicked, this,
                    [this, engine](bool) {
                showMonitorOutput(false);
                engine->setTxMonitorOutput(TxMonitorOutput::Speakers);
            });
            connect(m_monHeadphonesBtn, &QPushButton::clicked, this,
                    [this, engine](bool) {
                showMonitorOutput(true);
                engine->setTxMonitorOutput(TxMonitorOutput::Headphones);
            });
            connect(engine, &AudioEngine::txMonitorOutputChanged, this,
                    [this](TxMonitorOutput output) {
                showMonitorOutput(output == TxMonitorOutput::Headphones);
            });
            connect(engine, &AudioEngine::headphonesAvailableChanged, this,
                    [this](bool available) {
                m_headphonesAvailable = available;
                updateMonitorOutputNotice();
            });
            connect(engine, &AudioEngine::headphonesEnabledChanged, this,
                    [this](bool enabled) {
                m_headphonesEnabled = enabled;
                updateMonitorOutputNotice();
            });
            m_headphonesAvailable = engine->headphonesAvailable();
            m_headphonesEnabled = engine->headphonesEnabled();
            showMonitorOutput(engine->txMonitorOutput() == TxMonitorOutput::Headphones);
        }
    }

    // ── Monitor volume slider ↔ TransmitModel::monitorVolume ─────────────────
    // Phase 3M-1b J.3.
    // UI → Model: slider valueChanged(int) → setMonitorVolume(value / 100.0f).
    // Model → UI: monitorVolumeChanged(float) → slider = qRound(v * 100.0f).
    connect(m_monitorVolumeSlider, &QSlider::valueChanged,
            this, [this, &tx](int val) {
        if (m_updatingFromModel) { return; }
        m_monitorVolumeValue->setText(QString::number(val));
        tx.setMonitorVolume(static_cast<float>(val) / 100.0f);
    });

    connect(&tx, &TransmitModel::monitorVolumeChanged,
            this, [this](float volume) {
        QSignalBlocker b(m_monitorVolumeSlider);
        m_updatingFromModel = true;
        const int uiVal = qRound(volume * 100.0f);
        m_monitorVolumeSlider->setValue(uiVal);
        m_monitorVolumeValue->setText(QString::number(uiVal));
        m_updatingFromModel = false;
    });

    // ── Phase 3M-3a-i Batch 2 (Task F): LEV / EQ quick toggles ──────────────
    //
    // LEV button ↔ TransmitModel::txLevelerOn (bidirectional, echo-guarded).
    if (m_levBtn) {
        connect(m_levBtn, &QPushButton::toggled,
                this, [this, &tx](bool on) {
            if (m_updatingFromModel) { return; }
            tx.setTxLevelerOn(on);
        });
        connect(&tx, &TransmitModel::txLevelerOnChanged,
                this, [this](bool on) {
            QSignalBlocker b(m_levBtn);
            m_updatingFromModel = true;
            m_levBtn->setChecked(on);
            m_updatingFromModel = false;
        });
    }

    // EQ button ↔ TransmitModel::txEqEnabled (bidirectional, echo-guarded).
    // Right-click: placeholder slot for the TxEqDialog launch (Batch 3).
    if (m_eqBtn) {
        connect(m_eqBtn, &QPushButton::toggled,
                this, [this, &tx](bool on) {
            if (m_updatingFromModel) { return; }
            tx.setTxEqEnabled(on);
        });
        connect(&tx, &TransmitModel::txEqEnabledChanged,
                this, [this](bool on) {
            QSignalBlocker b(m_eqBtn);
            m_updatingFromModel = true;
            m_eqBtn->setChecked(on);
            m_updatingFromModel = false;
        });
        // Right-click → open TxEqDialog (Phase 3M-3a-i Batch 3 A.1).
        // Modeless singleton; show + raise + activateWindow so a
        // hidden-but-alive instance is brought forward.
        connect(m_eqBtn, &QPushButton::customContextMenuRequested,
                this, [this](const QPoint& /*pos*/) {
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
    if (m_cfcBtn) {
        connect(m_cfcBtn, &QPushButton::toggled,
                this, [this, &tx](bool on) {
            if (m_updatingFromModel) { return; }
            tx.setCfcEnabled(on);
        });
        connect(&tx, &TransmitModel::cfcEnabledChanged,
                this, [this](bool on) {
            QSignalBlocker b(m_cfcBtn);
            m_updatingFromModel = true;
            m_cfcBtn->setChecked(on);
            m_updatingFromModel = false;
        });
        connect(m_cfcBtn, &QPushButton::customContextMenuRequested,
                this, [this](const QPoint& /*pos*/) {
            requestOpenCfcDialog();
        });
    }

    // ── Mic-source badge ← TransmitModel::micSourceChanged ───────────────────
    // Phase 3M-1b J.3. Read-only: updates badge text on signal, no user interaction.
    // "PC mic" for MicSource::Pc, "Radio mic" for MicSource::Radio, "VAX" for MicSource::Vax.
    connect(&tx, &TransmitModel::micSourceChanged,
            this, [this](MicSource) { refreshMicSourceBadge(); });
    connect(m_model, &RadioModel::remoteMicSourceStateChanged,
            this, &TxApplet::refreshMicSourceBadge);

    // ── Phase 3M-1c J.1 ─ TX Profile combo wiring ────────────────────────────
    // User-driven currentTextChanged → MicProfileManager::setActiveProfile.
    // Guarded with m_updatingFromModel so the rebuildProfileCombo() echo
    // doesn't bounce back into setActiveProfile.
    connect(m_profileCombo, &QComboBox::currentTextChanged,
            this, [this](const QString& name) {
        if (m_updatingFromModel) { return; }
        if (!m_micProfileMgr) { return; }
        if (name.isEmpty()) { return; }
        if (m_model) {
            m_micProfileMgr->setActiveProfile(name, &m_model->transmitModel());
        }
    });

    // Right-click on combo → emit txProfileMenuRequested (Thetis cite at the
    // signal declaration).
    connect(m_profileCombo, &QComboBox::customContextMenuRequested,
            this, [this](const QPoint& /*pos*/) {
        emit txProfileMenuRequested();
    });

    // ── Plan 4 Cluster C (Task 4 / D2+D3): TX BW spinbox wiring ─────────────
    //
    // UI → Model: spinbox valueChanged → setFilterLow / setFilterHigh.
    //             m_updatingFromModel guard prevents echo loops.
    // Model → UI: TransmitModel::filterChanged(int,int) → QSignalBlocker on
    //             both spinboxes, then setValue + refresh status label.
    // Status label refresh helper (shared by filterChanged and dspModeChanged).
    auto refreshFilterStatus = [this]() { refreshTxFilterStatus(); };

    if (m_txFilterLowSpin) {
        connect(m_txFilterLowSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int v) {
            if (m_updatingFromModel) { return; }
            m_model->transmitModel().setFilterLow(v);
        });
    }
    if (m_txFilterHighSpin) {
        connect(m_txFilterHighSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int v) {
            if (m_updatingFromModel) { return; }
            m_model->transmitModel().setFilterHigh(v);
        });
    }

    // Model → spinboxes + status label on filterChanged.
    connect(&tx, &TransmitModel::filterChanged,
            this, [this, refreshFilterStatus](int low, int high) {
        m_updatingFromModel = true;
        if (m_txFilterLowSpin) {
            QSignalBlocker bLo(m_txFilterLowSpin);
            m_txFilterLowSpin->setValue(low);
        }
        if (m_txFilterHighSpin) {
            QSignalBlocker bHi(m_txFilterHighSpin);
            m_txFilterHighSpin->setValue(high);
        }
        m_updatingFromModel = false;
        refreshFilterStatus();
    });

    // Status label refresh on DSP mode change (symmetric ↔ asymmetric format)
    // rides followTransmitSlice's connection (slice control plan Task 11).
    refreshFilterStatus();

    // ── Phase 3M-1c J.2 ─ 2-TONE button wiring ───────────────────────────────
    // toggled → TwoToneController::setActive.  Echo-guarded.
    connect(m_twoToneBtn, &QPushButton::toggled, this, [this](bool on) {
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
            if (self && m_twoToneBtn) {
                const QSignalBlocker blocker(m_twoToneBtn);
                m_twoToneBtn->setChecked(m_twoToneCtrl && m_twoToneCtrl->isActive());
            }
            return;
        }
        if (!m_twoToneCtrl) { return; }
        m_twoToneCtrl->setActive(on);
    });

    // ── iPhone app plan, desktop remote transmit (R-IOS-13) ─────────────────
    // A remote window's own MoxController never keys: MOX, TUNE and 2-TONE
    // light from the Core's state (its `transmitting`, the transmit object's
    // `tune`, PureSignal's two-tone), and a refused press puts them back.
    if (m_model && m_model->role() == RadioModel::Role::Remote) {
        const auto syncFromCore = [this]() {
            if (!m_model) { return; }
            m_updatingFromModel = true;
            if (m_moxBtn) {
                QSignalBlocker b(m_moxBtn);
                m_moxBtn->setChecked(m_model->isTransmitting());
            }
            if (m_tuneBtn) {
                const bool tuning = m_model->transmitModel().isTune();
                QSignalBlocker b(m_tuneBtn);
                m_tuneBtn->setChecked(tuning);
                m_tuneBtn->setText(tuning ? QStringLiteral("TUNING...")
                                          : QStringLiteral("TUNE"));
            }
            if (m_twoToneBtn) {
                const PureSignalSessionFacade* ps = m_model->pureSignalFacade();
                QSignalBlocker b(m_twoToneBtn);
                m_twoToneBtn->setChecked(ps && ps->twoToneOn());
            }
            m_updatingFromModel = false;
        };
        connect(m_model, &RadioModel::transmittingChanged, this, syncFromCore);
        connect(&m_model->transmitModel(), &TransmitModel::tuneChanged, this, syncFromCore);
        connect(m_model, &RadioModel::remoteTransmitRefused, this, syncFromCore);
        if (PureSignalSessionFacade* ps = m_model->pureSignalFacade()) {
            connect(ps, &PureSignalSessionFacade::statusChanged, this, syncFromCore);
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
    // Production always uses RadioModel's one session facade. The local
    // coordinator remains late-bound behind that facade.
    if (m_psaBtn) {
        // Right-click → emit openPureSignalDialogRequested.  Wired
        // unconditionally so the seam exists even when no PureSignal
        // coordinator is bound (test harness verifies the signal emit).
        m_psaBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_psaBtn, &QWidget::customContextMenuRequested, this,
                [this](const QPoint&) {
            emit openPureSignalDialogRequested();
        });

        // The checked state expresses automatic-calibration intent. Turning
        // it off uses the acknowledged Off/reset action.
        connect(m_psaBtn, &QPushButton::toggled, this, [this](bool on) {
            if (m_updatingFromModel || !m_psFacade) {
                return;
            }
            const Ps3Action action = on
                ? Ps3Action::StartAutomatic : Ps3Action::OffReset;
            if (m_psFacade->requestAction(action) == 0) {
                syncPsaFromFacade();
            }
        });

        if (m_model) {
            m_psFacade = m_model->pureSignalFacade();
            if (m_psFacade) {
                connect(m_psFacade, &PureSignalSessionFacade::statusChanged,
                        this, &TxApplet::syncPsaFromFacade);
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
    m_micSourceBadge->setText(source == MicSource::Radio ? QStringLiteral("Radio mic")
        : source == MicSource::Vax ? QStringLiteral("VAX") : QStringLiteral("PC mic"));
    m_micSourceBadge->setToolTip(!reason.isEmpty() ? reason
        : source == MicSource::Radio && !m_model->ownsLocalDsp()
            ? QStringLiteral("Radio microphone at the Core (no microphone stream from this computer).")
            : QStringLiteral("Change microphone source via Settings > Audio > TX Input."));
}

void TxApplet::syncFromModel()
{
    if (!m_model) { return; }

    TransmitModel& tx = m_model->transmitModel();
    MoxController* mox = m_model->moxController();

    m_updatingFromModel = true;

    // RF Power
    {
        QSignalBlocker b(m_rfPowerSlider);
        m_rfPowerSlider->setValue(tx.power());
    }

    // Tune Power for current band
    {
        QSignalBlocker b(m_tunePwrSlider);
        const int tunePwr = shownTunePower(m_currentBand);
        m_tunePwrSlider->setValue(tunePwr);
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
    if (m_voxBtn) {
        QSignalBlocker bv(m_voxBtn);
        m_voxBtn->setChecked(tx.voxEnabled());
    }
    if (m_voxSlider) {
        {
            QSignalBlocker b(m_voxSlider);
            m_voxSlider->setValue(tx.voxThresholdDb());
        }
        if (m_voxLvlLabel) {
            m_voxLvlLabel->setText(QStringLiteral("%1 dB").arg(tx.voxThresholdDb()));
        }
    }
    if (m_voxDlySlider) {
        {
            QSignalBlocker b(m_voxDlySlider);
            m_voxDlySlider->setValue(tx.voxHangTimeMs());
        }
        if (m_voxDlyLabel) {
            m_voxDlyLabel->setText(QStringLiteral("%1 ms").arg(tx.voxHangTimeMs()));
        }
    }

    // MON button state (J.3 Phase 3M-1b)
    // monEnabled intentionally loads as OFF — plan §0 row 9 safety rule.
    if (m_monBtn) {
        QSignalBlocker bm(m_monBtn);
        m_monBtn->setChecked(tx.monEnabled());
    }

    // Monitor volume slider (J.3 Phase 3M-1b)
    // Sync slider position from model; default 0.5f → slider 50.
    if (m_monitorVolumeSlider) {
        QSignalBlocker bvol(m_monitorVolumeSlider);
        const int uiVal = qRound(tx.monitorVolume() * 100.0f);
        m_monitorVolumeSlider->setValue(uiVal);
        m_monitorVolumeValue->setText(QString::number(uiVal));
    }

    // LEV / EQ / CFC button state (3M-3a-i Batch 2 Task F + 3M-3a-ii
    // Batch 6 Task A — CFC button added; PROC moved to PhoneCwApplet
    // 3M-3a-ii post-bench cleanup).
    if (m_levBtn) {
        QSignalBlocker b(m_levBtn);
        m_levBtn->setChecked(tx.txLevelerOn());
    }
    if (m_eqBtn) {
        QSignalBlocker b(m_eqBtn);
        m_eqBtn->setChecked(tx.txEqEnabled());
    }
    if (m_cfcBtn) {
        QSignalBlocker b(m_cfcBtn);
        m_cfcBtn->setChecked(tx.cfcEnabled());
    }

    // TX BW spinboxes + status label (Plan 4 Cluster C Task 4 / D2+D3+D9)
    if (m_txFilterLowSpin) {
        QSignalBlocker bLo(m_txFilterLowSpin);
        m_txFilterLowSpin->setValue(tx.filterLow());
    }
    if (m_txFilterHighSpin) {
        QSignalBlocker bHi(m_txFilterHighSpin);
        m_txFilterHighSpin->setValue(tx.filterHigh());
    }
    refreshTxFilterStatus();

    // Mic-source badge (J.3 Phase 3M-1b; extended to 3-way in Phase 3M-VAX-toggle)
    refreshMicSourceBadge();

    // MOX / TUNE button state
    if (mox) {
        QSignalBlocker bm(m_moxBtn);
        m_moxBtn->setChecked(m_desktopMoxOn ? m_desktopMoxOn() : mox->isMox());

        QSignalBlocker bt(m_tuneBtn);
        const bool isManual = m_desktopTuneOn ? m_desktopTuneOn() : mox->isManualMox();
        m_tuneBtn->setChecked(isManual);
        m_tuneBtn->setText(isManual ? QStringLiteral("TUNING...") : QStringLiteral("TUNE"));
    }

    m_updatingFromModel = false;
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
    if (!m_txSliceRow) { return; }
    for (QPushButton* button : std::as_const(m_txSliceButtons)) {
        m_txSliceRow->removeWidget(button);
        button->deleteLater();
    }
    m_txSliceButtons.clear();
    if (!m_model) { return; }
    const SliceModel* current = transmitSlice();
    const QString reason = m_txSliceUnavailable ? m_txSliceUnavailable() : QString();
    const QString btnStyle = Style::buttonBaseStyle()
        + QStringLiteral("QPushButton { padding: 2px; }") + Style::greenCheckedStyle();
    for (SliceModel* slice : m_model->slices()) {
        if (!slice) { continue; }
        const int id = slice->sliceIndex();
        // U8: only the slices this window controls, never one it listens to.
        if (m_txSliceControlled && !m_txSliceControlled(id)) { continue; }
        auto* button = new QPushButton(slice->sliceLetter(), this);
        button->setObjectName(QStringLiteral("TxSliceButton%1").arg(slice->sliceLetter()));
        button->setCheckable(true);
        button->setChecked(slice == current);
        button->setFixedHeight(20);
        button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        button->setStyleSheet(btnStyle);
        button->setProperty("sliceId", id);
        button->setAccessibleName(QStringLiteral("Transmit on slice %1").arg(slice->sliceLetter()));
        // Disabled, never hidden: the reason is the tooltip.
        const TransmitSliceChoice availability = m_txSliceAvailability
            ? m_txSliceAvailability(id)
            : TransmitSliceChoice{reason.isEmpty(), reason};
        button->setEnabled(availability.enabled);
        button->setToolTip(availability.toolTip.isEmpty()
            ? QStringLiteral("Transmit on slice %1").arg(slice->sliceLetter())
            : availability.toolTip);
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
        m_txSliceRow->addWidget(button, 1);
        m_txSliceButtons.append(button);
    }
}

void TxApplet::syncDesktopKeyState()
{
    if (!m_model || !m_model->moxController()) { return; }
    const QSignalBlocker moxBlock(m_moxBtn);
    const QSignalBlocker tuneBlock(m_tuneBtn);
    m_moxBtn->setChecked(m_desktopMoxOn ? m_desktopMoxOn()
                                       : m_model->moxController()->isMox());
    const bool tuning = m_desktopTuneOn ? m_desktopTuneOn() : m_model->isTune();
    m_tuneBtn->setChecked(tuning);
    m_tuneBtn->setText(tuning ? QStringLiteral("TUNING...") : QStringLiteral("TUNE"));
}

// (Phase 3M-1b J.2 showVoxSettingsPopup removed in 3M-3a-iii Task 16 —
//  the wired VOX surface lives on PhoneCwApplet now, and the per-parameter
//  popup gave way to right-click → Setup → Transmit → DEXP/VOX.)

void TxApplet::rescaleFwdGaugeForModel(HPSDRModel model)
{
    if (!m_fwdPowerGauge) { return; }

    // Per-SKU PA ceiling from HpsdrModel.h paMaxWattsFor().  Bench-reported
    // #167 follow-up: 0-120 W default scale made HL2 (5 W max) and
    // ANAN-G2-1K (1000 W max) both show meaningless bar widths.
    const int maxW   = paMaxWattsFor(model);
    const double red = static_cast<double>(maxW);
    // 20% headroom past the red zone (ControlRanges.h, which the Core's
    // catalogue reads too).
    const double top = red * ControlRanges::kRfPowerGaugeHeadroom;

    m_fwdPowerGauge->setRange(0.0, top);
    m_fwdPowerGauge->setRedStart(red);
    m_fwdPowerGauge->setYellowStart(red);   // no distinct yellow zone

    // Pick five ticks proportional to the new range: 0 / 1/3 / 2/3 / red / top.
    // For HL2 (red=5): 0 / 1.7 / 3.3 / 5 / 6.  For ANAN-100 (red=100):
    // 0 / 33 / 67 / 100 / 120.  For ANAN-G2-1K (red=1000): 0 / 333 / 667 / 1000 / 1200.
    auto fmt = [maxW](double v) {
        return (maxW <= 10) ? QString::number(v, 'f', 1)   // sub-watt resolution for QRP
                            : QString::number(qRound(v));
    };
    m_fwdPowerGauge->setTickLabels({
        fmt(0.0),
        fmt(red / 3.0),
        fmt(2.0 * red / 3.0),
        fmt(red),
        fmt(top),
    });
}

// ── Issue #175 Task 7: per-SKU power-slider rescale + dB labels ─────────────
//
// From mi0bot-Thetis console.cs:2098-2108 [v2.10.3.13-beta2]
//   if (HPSDRHW.HermesLite == Audio.LastRadioHardware ||
//       HPSDRModel.HERMESLITE == HardwareSpecific.Model)     // MI0BOT: Need an early indication of hardware type due to HL2 rx attenuator can be negative
//   {
//       ptbPWR.Maximum = 90;        // MI0BOT: Changes for HL2 only having a 16 step output attenuator
//       ptbPWR.Value = 0;
//       ptbPWR.LargeChange = 6;
//       ptbPWR.SmallChange = 6;
//       ptbTune.Maximum = 99;
//       ptbTune.Value = 0;
//       ptbTune.LargeChange = 3;
//       ptbTune.SmallChange = 3;
//       ...
//   }
//
// HL2: RF Power slider 0..90 step 6 (16-step output attenuator,
//      0.5 dB/step); Tune slider 0..99 step 3 (33 sub-steps).
// Other SKUs: canonical Thetis 0..100 step 1.
//
// MI0BOT: Changes for HL2 only having a 16 step output attenuator
// [original inline comment from mi0bot console.cs:2098]
void TxApplet::rescalePowerSlidersForModel(HPSDRModel model)
{
    if (!m_rfPowerSlider || !m_tunePwrSlider) { return; }

    // Cache for updatePowerSliderLabels() so the formatter sees the same
    // SKU the slider was rescaled for, even when called outside the live
    // currentRadioChanged path (unit tests, headless harness).
    m_powerSliderModel = model;

    const QSignalBlocker rfBlock(m_rfPowerSlider);
    const QSignalBlocker tunBlock(m_tunePwrSlider);

    m_rfPowerSlider->setRange(0, rfPowerSliderMaxFor(model));
    m_rfPowerSlider->setSingleStep(rfPowerSliderStepFor(model));
    m_rfPowerSlider->setPageStep(rfPowerSliderStepFor(model));

    m_tunePwrSlider->setRange(0, tuneSliderMaxFor(model));
    m_tunePwrSlider->setSingleStep(tuneSliderStepFor(model));
    m_tunePwrSlider->setPageStep(tuneSliderStepFor(model));

    // Tooltip - Thetis-faithful on every SKU.  Replaces the previous
    // "RF output power (0-100 W)" / "Tune carrier power for current band
    // (0-100 W)" wording, which contradicted Thetis semantics on every
    // SKU (the slider is a relative drive level, not watts).
    //
    // From Thetis console.resx + mi0bot-Thetis console.resx [v2.10.3.13-beta2]
    //   <data ...><value>Transmit Drive - This is a relative value and is
    //   not meant to imply watts, unless the PA profile is configured
    //   with MAX watts @ 100%</value></data>
    m_rfPowerSlider->setToolTip(QStringLiteral(
        "Transmit Drive - relative value, not watts unless the PA profile "
        "is configured with MAX watts @ 100%"));
    m_tunePwrSlider->setToolTip(QStringLiteral(
        "Tune and/or 2Tone Drive - relative value, not watts unless the "
        "PA profile is configured with MAX watts @ 100%"));

    updatePowerSliderLabels();
}

// From mi0bot-Thetis console.cs:29245-29274 [v2.10.3.13-beta2]
//   if (HardwareSpecific.Model == HPSDRModel.HERMESLITE)       // MI0BOT: HL2 has only 15 output power levels
//   {
//       ...
//       lblPWR.Text = "Drive:  " + ((Math.Round(drv / 6.0) / 2) - 7.5).ToString() + "dB";
//   }
//
// HL2 RF Power label formula:
//   dB = (round(drv/6.0)/2) - 7.5     // 0.5 dB steps in [-7.5, 0]
// HL2 Tune slider label formula (slider 0..99 -> dB -16.5..0):
//   dB = (slider/3 - 33) / 2          // inverse of the persistence formula
//
// MI0BOT: HL2 has only 15 output power levels
// [original inline comment from mi0bot console.cs:29245]
void TxApplet::updatePowerSliderLabels()
{
    if (!m_rfPowerSlider || !m_tunePwrSlider
        || !m_rfPowerValue || !m_tunePwrValue) { return; }

    // Use the cached SKU set by rescalePowerSlidersForModel() so the
    // label format always matches the slider's effective range (HL2
    // 0..90 step 6 / 0..99 step 3, all others 0..100 step 1).
    const HPSDRModel model = m_powerSliderModel;
    const int        rfVal  = m_rfPowerSlider->value();
    const int        tunVal = m_tunePwrSlider->value();

    // The shown values come from HpsdrModel.h (rfPowerShownFor,
    // tuneSliderShownFor: mi0bot's UpdateDriveLabel and UpdateTuneLabel),
    // which the Core's catalogue reads too.
    const int decimals = powerSliderShownDecimalsFor(model);
    m_rfPowerValue->setText(QString::number(rfPowerShownFor(model, rfVal), 'f', decimals));
    m_tunePwrValue->setText(QString::number(tuneSliderShownFor(model, tunVal), 'f', decimals));
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
        QSignalBlocker b(m_tunePwrSlider);
        m_updatingFromModel = true;
        m_tunePwrSlider->setValue(tunePwr);
        // Issue #175 Task 7: HL2 dB / non-HL2 integer — replaces the
        // bare m_tunePwrValue->setText(QString::number(tunePwr)) so HL2
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
// tooltip text and installs it on m_moxBtn. If the mode is an allowed SSB
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
    if (!m_txFilterStatusLabel || !m_model) { return; }
    SliceModel* slice = transmitSlice();
    const DSPMode mode = slice ? slice->dspMode() : DSPMode::USB;
    m_txFilterStatusLabel->setText(m_model->transmitModel().filterDisplayText(mode));
}

void TxApplet::onMoxModeChanged(DSPMode mode)
{
    // Task 16: the receive-only lock sits on top of the tooltip; take it
    // off, change the tooltip under it, and put it back.
    removeReceiveOnlyLock();
    if (m_moxBtn) {
        // Task 16 fix wave 2 (Minor 2): while the transmit-permission layer
        // holds the button, its reason stays visible and the mode's tooltip
        // goes into the tooltip that layer gives back.
        if (m_moxBtn->property(kTransmitSavedTooltip).isValid()) {
            m_moxBtn->setProperty(kTransmitSavedTooltip, tooltipForMode(mode));
        } else {
            m_moxBtn->setToolTip(tooltipForMode(mode));
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
    for (QWidget* control : {static_cast<QWidget*>(m_moxBtn),
                             static_cast<QWidget*>(m_tuneBtn),
                             static_cast<QWidget*>(m_twoToneBtn),
                             static_cast<QWidget*>(m_voxBtn)}) {
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
    if (m_model->transmitLockCoversMox()) {
        lock(m_moxBtn);
    }
    lock(m_tuneBtn);
    lock(m_twoToneBtn);   // MW0LGE_21a
    // TX safety (2026-09-30): a lost radio link leaves VOX alone, as
    // Thetis's power-off does (console.cs:27488-27493 [v2.10.3.15]).
    if (m_model->transmitLockCoversVox()) {
        lock(m_voxBtn);
    }
}

// ── pollVoxMeter — Phase 3M-3a-iii bench polish 2026-05-04 ─────────────────
// 100 ms tick that drives m_voxPeakMeter on the TX right pane.
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
    if (!ch || !m_voxPeakMeter) { return; }

    // VOX peak: linear amplitude → dB → 0..1 over -80..0 range.
    const double linearPeak = ch->getDexpPeakSignal();
    const double voxPeakDb  = (linearPeak > 0.0)
        ? 20.0 * std::log10(linearPeak)
        : -80.0;
    const double voxPeak01  = std::clamp((voxPeakDb + 80.0) / 80.0, 0.0, 1.0);
    m_voxPeakMeter->setSignalLevel(voxPeak01);

    // VOX threshold marker: voxThresholdDb is in -80..0 dB range.
    const int thDb = m_model->transmitModel().voxThresholdDb();
    m_voxPeakMeter->setThresholdMarker(
        std::clamp((thDb + 80.0) / 80.0, 0.0, 1.0));
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
            if (!m_profileCombo) { return; }
            QSignalBlocker blk(m_profileCombo);
            m_updatingFromModel = true;
            const int idx = m_profileCombo->findText(name);
            if (idx >= 0) {
                m_profileCombo->setCurrentIndex(idx);
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
            if (!m_twoToneBtn) { return; }
            QSignalBlocker blk(m_twoToneBtn);
            m_updatingFromModel = true;
            m_twoToneBtn->setChecked(active);
            m_updatingFromModel = false;
        });

        // Sync initial state.
        QSignalBlocker blk(m_twoToneBtn);
        m_updatingFromModel = true;
        m_twoToneBtn->setChecked(m_twoToneCtrl->isActive());
        m_updatingFromModel = false;
    }
}

// ---------------------------------------------------------------------------
// Phase 3M-3a-ii Batch 6 (Task A) — requestOpenCfcDialog
//
// Open (or raise) the modeless TxCfcDialog.  Lazy-creates the dialog on first
// call so the construction cost is only paid when the user actually opens
// CFC settings.  The dialog is parented to this applet's top-level window so
// it floats freely; modal flag is forced false in TxCfcDialog's ctor.  We
// don't deleteLater() the dialog on close — keep it alive across opens for
// fast re-show, mirroring the TxEqDialog singleton pattern.
//
// Public-slot entry point so external surfaces can reuse the same instance:
//   - [CFC] button right-click → customContextMenuRequested → this slot.
//   - CfcSetupPage's [Configure CFC bands…] button → MainWindow connects its
//     openCfcDialogRequested signal to this slot.
//   - Future Tools menu item → connects to this slot.
// ---------------------------------------------------------------------------
void TxApplet::requestOpenCfcDialog()
{
    // R-R3-49 (parity Task 4): opens in a remote window too, greyed with
    // the reason while the Core cannot take a change.
    if (!m_model) { return; }

    if (!m_cfcDialog) {
        QWidget* host = window();
        m_cfcDialog = new TxCfcDialog(
            &m_model->transmitModel(),
            m_model->txChannel(),
            host ? host : static_cast<QWidget*>(this));
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
                    m_cfcDialog, &TxCfcDialog::onStationCommandFinished);
            QPointer<TxCfcDialog> dialog(m_cfcDialog);
            connect(m_model, &RadioModel::stationLinkStateChanged, m_cfcDialog,
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

// R-R3-45: the MON output pair shows the choice; clicking the checked one
// keeps it, as on the receiver flag.
void TxApplet::showMonitorOutput(bool headphones)
{
    if (m_monSpeakersBtn) {
        QSignalBlocker b(m_monSpeakersBtn);
        m_monSpeakersBtn->setChecked(!headphones);
    }
    if (m_monHeadphonesBtn) {
        QSignalBlocker b(m_monHeadphonesBtn);
        m_monHeadphonesBtn->setChecked(headphones);
    }
    updateMonitorOutputNotice();
}

// R-R3-45: with the headphones chosen and none open, say why MON is silent
// in the flag's words: turned on but not opened, or not set up at all.
void TxApplet::updateMonitorOutputNotice()
{
    if (!m_monOutputNotice) {
        return;
    }
    const bool headphones = m_monHeadphonesBtn && m_monHeadphonesBtn->isChecked();
    m_monOutputNotice->setText(m_headphonesEnabled ? VfoWidget::headphonesNotOpenedText()
                                                   : VfoWidget::headphonesMissingText());
    m_monOutputNotice->setVisible(headphones && !m_headphonesAvailable);
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
    apply(m_tuneBtn);
    apply(m_moxBtn);
    // Fix wave 2 (M8): VOX also needs this computer's microphone line.
    gateTransmitControl(m_voxBtn, permitted && m_voxPermitted,
                        permitted ? m_voxReason : reason);
    apply(m_twoToneBtn);
    // Task 16: the receive-only lock back on top (checkpoint join).
    applyReceiveOnlyLock();
}

void TxApplet::setVoxPermitted(bool permitted, const QString& reason)
{
    removeReceiveOnlyLock();
    m_voxPermitted = permitted;
    m_voxReason = reason;
    gateTransmitControl(m_voxBtn, m_transmitPermitted && permitted,
                        m_transmitPermitted ? reason : m_transmitPermissionReason);
    applyReceiveOnlyLock();
}

void TxApplet::setPureSignalArmingPermitted(bool permitted, const QString& unavailableReason)
{
    m_psArmingPermitted = permitted;
    const QString reason = unavailableReason.isEmpty()
        ? tr("Remote transmit controls are not available from this Core.")
        : unavailableReason;
    // Fix wave GUI-I7: the facade's reason off first, so this gate keeps
    // the button's own tooltip to put back.
    removePsaFacadeReason();
    gateTransmitControl(m_psaBtn, permitted, reason);
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
    for (QWidget* control : {static_cast<QWidget*>(m_rfPowerSlider),
                             static_cast<QWidget*>(m_txFilterLowSpin),
                             static_cast<QWidget*>(m_txFilterHighSpin)}) {
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
    for (QWidget* control : {static_cast<QWidget*>(m_tunePwrSlider),
                             static_cast<QWidget*>(m_voxSlider),
                             static_cast<QWidget*>(m_voxDlySlider),
                             static_cast<QWidget*>(m_monBtn),
                             static_cast<QWidget*>(m_monitorVolumeSlider),
                             static_cast<QWidget*>(m_levBtn),
                             static_cast<QWidget*>(m_eqBtn),
                             static_cast<QWidget*>(m_cfcBtn)}) {
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
    return QStringLiteral(
        "This Core does not send the transmit monitor. Updating the Core may help.");
}

void TxApplet::applyMonitorOutputGate()
{
    const bool permitted = m_transmitChainSettingsPermitted && m_monitorOutputPermitted;
    const QString reason = !m_transmitChainSettingsPermitted
        ? (m_transmitChainSettingsReason.isEmpty()
               ? IStationLink::transmitSettingsUnavailableReason()
               : m_transmitChainSettingsReason)
        : m_monitorOutputReason;
    for (QWidget* control : {static_cast<QWidget*>(m_monSpeakersBtn),
                             static_cast<QWidget*>(m_monHeadphonesBtn)}) {
        gateTransmitControl(control, permitted, reason);
    }
}

// R-R3-49 (parity Task 3): the profile combo. The Core applies a pick and
// reports its active profile back; a refused pick shows the Core's again.
void TxApplet::setTxProfilePermitted(bool permitted, const QString& unavailableReason)
{
    m_txProfilePermitted = permitted;
    gateTransmitControl(m_profileCombo, permitted,
                        unavailableReason.isEmpty()
                            ? IStationLink::transmitSettingsUnavailableReason()
                            : unavailableReason);
}

void TxApplet::setTakeTransmitOffered(bool offered, bool holderOnAir)
{
    if (!m_takeTransmitBtn) { return; }
    m_takeTransmitBtn->setVisible(offered);
    m_takeTransmitBtn->setProperty("holderOnAir", offered && holderOnAir);
    m_takeTransmitBtn->setStyleSheet(offered && holderOnAir
        ? QStringLiteral("QPushButton { color: #ff8080; border: 1px solid #ff4444;"
                         " border-radius: 3px; padding: 2px 8px; }")
        : QString());
}

bool TxApplet::takeTransmitOffered() const
{
    return m_takeTransmitBtn && !m_takeTransmitBtn->isHidden();
}

void TxApplet::setTransmitHolderText(const QString& text)
{
    if (!m_holderLabel) { return; }
    m_holderLabel->setText(text);
    m_holderLabel->setVisible(!text.isEmpty());
}

QString TxApplet::transmitHolderText() const
{
    return m_holderLabel ? m_holderLabel->text() : QString();
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
// Mirrors the RxApplet::setBoardCapabilities pattern.  No-op when m_psaBtn
// is null (e.g. during early construction or after teardown).
// ---------------------------------------------------------------------------
void TxApplet::setBoardCapabilities(const NereusSDR::BoardCapabilities& caps)
{
    if (!m_psaBtn) { return; }
    // Fix round 1 (minor 5): with no radio the caps fall back to Unknown.
    // PS-A shows, disabled with its reason, until the board is known; only
    // a known board without PureSignal hides it.
    const bool boardUnknown = caps.board == HPSDRHW::Unknown;
    m_psBoardUnknown = boardUnknown && !caps.hasPureSignal;
    m_psaBtn->setVisible(caps.hasPureSignal || boardUnknown);
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
    if (!m_psaBtn) {
        return;
    }
    const bool automaticIntent = m_psFacade && m_psFacade->settings()
        ? m_psFacade->settings()->autoCalEnabled() : false;
    const QSignalBlocker blocker(m_psaBtn);
    m_updatingFromModel = true;
    m_psaBtn->setChecked(automaticIntent);
    // R-R3-49 (parity Task 7): arming keys nothing, so canArm.
    const bool canArm = !m_psBoardUnknown && m_psFacade && m_psFacade->available()
        && m_psFacade->canArm();
    // Fix wave GUI-I7: greyed by the facade, with the facade's reason. The
    // arming gate's reason, when it is on, stays (its tooltip is set).
    removePsaFacadeReason();
    if (m_psArmingPermitted && !canArm) {
        // Fix round 1 (minor 5): a board not known says it needs one.
        const QString refusal = m_psBoardUnknown || !m_psFacade ? QString()
                                                               : m_psFacade->armingRefusal();
        const QString reason = refusal.isEmpty()
            ? PureSignalSessionFacade::needsRadioReason() : refusal;
        m_psaBtn->setProperty(kPsaFacadeSavedTooltip, m_psaBtn->toolTip());
        m_psaBtn->setProperty(kPsaFacadeSavedDescription, m_psaBtn->accessibleDescription());
        m_psaBtn->setToolTip(reason);
        m_psaBtn->setAccessibleDescription(reason);
    }
    m_psaBtn->setEnabled(m_psArmingPermitted && canArm);
    m_updatingFromModel = false;
}

void TxApplet::removePsaFacadeReason()
{
    if (!m_psaBtn || !m_psaBtn->property(kPsaFacadeSavedTooltip).isValid()) {
        return;
    }
    m_psaBtn->setToolTip(m_psaBtn->property(kPsaFacadeSavedTooltip).toString());
    m_psaBtn->setAccessibleDescription(
        m_psaBtn->property(kPsaFacadeSavedDescription).toString());
    m_psaBtn->setProperty(kPsaFacadeSavedTooltip, QVariant());
    m_psaBtn->setProperty(kPsaFacadeSavedDescription, QVariant());
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
    if (!m_profileCombo) { return; }
    if (!m_micProfileMgr) {
        // No manager → leave the placeholder "Default" item alone.
        return;
    }

    QSignalBlocker blk(m_profileCombo);
    m_updatingFromModel = true;

    const QStringList names = m_micProfileMgr->profileNames();
    const QString active = m_micProfileMgr->activeProfileName();

    m_profileCombo->clear();
    m_profileCombo->addItems(names);

    const int idx = m_profileCombo->findText(active);
    if (idx >= 0) {
        m_profileCombo->setCurrentIndex(idx);
    }

    m_updatingFromModel = false;
}

} // namespace NereusSDR
