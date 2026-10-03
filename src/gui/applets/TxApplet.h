#pragma once

// =================================================================
// src/gui/applets/TxApplet.h  (NereusSDR)
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
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Layout pattern from AetherSDR `src/gui/TxApplet.{h,cpp}`.
//                 Wiring deferred to Phase 3M.
//   2026-04-26 — Phase 3M-1a H.3: TUNE/MOX/Tune-Power/RF-Power deep-wired.
//                 Out-of-phase controls hidden with TODO comments.
//                 syncFromModel() activates. setCurrentBand(Band) added for
//                 tune-power slider sync on band change.
//   2026-04-28 — Phase 3M-1b J.2: VOX toggle button added below Tune Power.
//                 Checkable, green border when active. Bidirectional with
//                 TransmitModel::voxEnabled (default false; does NOT persist).
//                 Right-click opens VoxSettingsPopup with 3 sliders for
//                 threshold/gain/hang-time.
//                 (REMOVED 2026-05-03 — see 3M-3a-iii Task 16 entry below.)
//   2026-04-28 — Phase 3M-1b J.3: MON toggle button + monitor volume slider
//                 added below VOX. Bidirectional with TransmitModel::monEnabled
//                 and TransmitModel::monitorVolume (default 0.5f). Mic-source
//                 badge added above the gauges (read-only, "PC mic"/"Radio mic").
//   2026-04-28 — Phase 3M-1b K.2: MOX button tooltip override on DSP mode change.
//                 tooltipForMode(DSPMode) returns a static tooltip string that
//                 reflects the deferred-phase reason for CW and AM/FM/SAM/DSB/DRM,
//                 or the normal "Manual transmit (MOX)" for allowed modes.
//                 onMoxModeChanged(DSPMode) slot wired to SliceModel::dspModeChanged
//                 via RadioModel in wireControls(). Closes Phase K.
//   2026-04-28 — Phase 3M-1b (relocation): Mic Gain slider row (J.1) removed.
//                 Relocated to PhoneCwApplet (#5 slot) per JJ feedback.
//                 PhoneCwApplet now owns micGainDb wiring and mic level gauge.
//   2026-04-29 — Phase 3M-1c J.1+J.2: TX Profile combo wired to MicProfileManager
//                 (left-click selects, right-click → Setup → TX Profile via
//                 txProfileMenuRequested signal); 2-TONE button wired to
//                 TwoToneController (mirrors Thetis chk2TONE_CheckedChanged at
//                 console.cs:44728-44760 [v2.10.3.13]).  Both bind via raw-
//                 pointer setters that Phase L's MainWindow wiring populates.
//   2026-04-30 — Phase 3M-3a-ii Batch 6 (Task F + A): PROC button enabled and
//                 bidirectionally wired to TransmitModel::cpdrOn (mirrors WDSP
//                 SetTXACompressorRun via TxChannel; see Thetis frmCFCConfig
//                 dynamics + console.cs:36430 cpdrOn global state).  CFC button
//                 added next to PROC: bidirectional with TransmitModel::cfcEnabled,
//                 right-click opens TxCfcDialog (modeless, mirrors TxEqDialog
//                 launch pattern).  requestOpenCfcDialog() public slot exposed so
//                 CfcSetupPage's [Configure CFC bands…] button can route through
//                 the same dialog instance.
//   2026-04-30 — Phase 3M-3a-ii post-bench cleanup (Batch 6 H): PROC button
//                 removed from TxApplet (was a duplicate — PhoneCwApplet had an
//                 un-wired PROC button + slider since 3I-3 NyiOverlay-marked).
//                 PROC wiring moved to PhoneCwApplet; row drops to [LEV][EQ][CFC].
//   2026-05-02 — Plan 4 Cluster C (Task 4 / D2+D3+D9-status): TX BW spinbox
//                 row + status label added.  See TxApplet.cpp for details.
//   2026-05-03 — Phase 3M-3a-iii Task 16: VOX toggle button removed from TxApplet
//                 (was a duplicate — PhoneCwApplet now owns the canonical VOX
//                 surface as part of the 3M-3a-iii Phone-tab DEXP/VOX wiring).
//                 Same dedup pattern as 3M-3a-ii PROC cleanup.  VoxSettingsPopup
//                 widget retired alongside (no remaining callers).
//   2026-05-04 — Phase 3M-3a-iii bench polish: VOX row relocated from
//                 PhoneCwApplet (Phone tab Control #10) back to TxApplet as
//                 a full row directly under TUNE/MOX (Option B).  Operators
//                 want the VOX engage surface next to MOX/TUNE on the right
//                 pane where they engage TX, not buried on the Phone tab.
//                 The full row moves as a unit: [VOX btn][threshold slider
//                 + DexpPeakMeter strip][threshold value label][Hold slider]
//                 [Hold value label].  Right-click on VOX still opens
//                 Setup → Transmit → DEXP/VOX (mirrors PhoneCwApplet's DEXP
//                 button pattern).  100 ms VOX peak-meter poller moves with
//                 the row.  DEXP row stays on PhoneCwApplet — only VOX moves.
//   2026-09-22 — Routed the PS-A toggle through RadioModel's shared
//                 PureSignalSessionFacade for local and remote sessions.
//   2026-09-24 : R-R3-45: Speakers / Headphones choice for MON on the MON
//                 row, with a plain notice when the headphones are chosen
//                 and not open. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-24 : R-R3-49 (parity Task 1): setTransmitSettingsPermitted.
//                 RF Power and the TX filter low and high follow the
//                 transmit settings gate in a remote window; the keying
//                 controls keep setTransmitPermitted. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-24 : R-R3-49 (parity Task 2): setTransmitChainSettingsPermitted
//                 for Tune Power, the VOX level and delay, MON, its level
//                 and output, LEV, EQ and CFC. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-25 : R-R3-49 (parity Task 3): setTxProfilePermitted for the
//                 profile combo, which picks the Core's profiles in a
//                 remote window. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-25 : R-R3-49 (parity Task 4): setTxProcessingPermitted for the
//                 CFC dialog; the EQ and CFC right-clicks open their
//                 dialogs in a remote window. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-25 : R-R3-49 (parity Task 7): setPureSignalArmingPermitted for
//                 PS-A, which arms PureSignal on the Core from a remote
//                 window off the air. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-25 : Task 16 fix wave: followActiveSliceMode (M3) and the
//                 transmit permission's reason kept for the receive-only
//                 lock (M6). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
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
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 11 (Q15, U8):
//                setTransmitSliceResolver (the TX band, per-band power and
//                the MOX mode follow the transmit slice, never a listened
//                one) and the transmit-slice letter row
//                (setTransmitSliceChoices). AI-assisted via Anthropic Claude
//                Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 11 fix: holds
//                the transmit slice's band while transmitting (m_txBand).
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Fix round 1 (minor 5): m_psBoardUnknown,
//                PS-A shown disabled while the board is not known.
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

#include "AppletWidget.h"
#include <QPointer>
#include <QList>
#include <functional>
#include <QString>
#include "models/Band.h"
#include "core/BoardCapabilities.h"  // setBoardCapabilities slot
#include "core/HpsdrModel.h"   // HPSDRModel for rescaleFwdGaugeForModel
#include "core/WdspTypes.h"

class QHBoxLayout;
class QPushButton;
class QSlider;
class QComboBox;
class QLabel;
class QSpinBox;
class QTimer;

namespace NereusSDR {

class HGauge;
class MicProfileManager;
class PureSignal;
class PureSignalSessionFacade;
class TwoToneController;
class TxCfcDialog;
class DexpPeakMeter;
class SliceModel;

// TxApplet — transmit controls panel.
//
// Layout (AetherSDR TxApplet.cpp pattern):
//  0.  Mic-source badge     — read-only label "PC mic"/"Radio mic" [J.3 Phase 3M-1b]
//  1.  Forward Power gauge  — HGauge 0–120 W, red > 100 W
//  2.  SWR gauge            — HGauge 1.0–3.0, red > 2.5
//  3.  RF Power slider row  — label(62) + slider + value(22)
//  4a. Tune Power slider row
//  4a. TUNE + MOX button row (50% each)
//  4b. VOX row [3M-3a-iii bench polish 2026-05-04]
//      [VOX btn 48px] [Threshold slider + DexpPeakMeter stack][-N dB inset]
//      [Hold slider 1..2000 ms][N ms inset]
//      Bidirectional with TransmitModel::voxEnabled / voxThresholdDb /
//      voxHangTimeMs.  Right-click on VOX → openSetupRequested("Transmit",
//      "DEXP/VOX").  Relocated from PhoneCwApplet Phone tab Control #10.
//  4c. MON toggle button    — checkable, blue border on active [J.3 Phase 3M-1b]
//      Bidirectional with TransmitModel::monEnabled (default false).
//  4d. Monitor volume slider — 0..100 → monitorVolume 0.0..1.0 [J.3 Phase 3M-1b]
//      Default 50 (matches model default 0.5f from Thetis audio.cs:417).
//  5.  MOX button           — checkable, red when active
//  6.  TUNE button          — checkable, red + "TUNING..." when active
//  7.  ATU button           — checkable
//  8.  MEM button           — checkable
//  9.  TX Profile combo     — "Default" item
// 10.  Tune mode combo
// 11.  2-Tone test button   — hidden until Phase 3M-3 (out-of-phase)
// 12.  PS-A toggle          — hidden until Phase 3M-4 (out-of-phase)
// 13.  DUP (full duplex)    — checkable
// 14.  xPA indicator button — checkable
// 15.  SWR protection LED   — QLabel indicator
//
// NOTE: Mic Gain slider was here (Row 3b, J.1) but was relocated to
//       PhoneCwApplet (#5 slot) per JJ feedback (2026-04-28 relocation).
//
// Phase 3M-1a H.3: TUNE/MOX/Tune-Power/RF-Power are deep-wired.
// Phase 3M-1b J.3: MON toggle + volume slider + mic-source badge wired.
// Phase 3M-3a-iii bench polish (2026-05-04): VOX row relocated from
//   PhoneCwApplet (Phone tab #10) back to TxApplet — operators wanted the
//   VOX engage surface next to MOX/TUNE on the right pane.  Full row moves
//   as a unit including the live DexpPeakMeter strip + 100 ms poller.
// Out-of-phase controls (2-Tone, PS-A) are hidden.
class TxApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit TxApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId() const override { return QStringLiteral("TX"); }
    QString appletTitle() const override { return QStringLiteral("TX"); }
    void syncFromModel() override;

    // Called by MainWindow when band changes so the Tune Power slider
    // can reflect the stored per-band tune power.
    // Phase 3M-1a H.3.
    void setCurrentBand(Band band);

    // Rescale the RF Pwr HGauge ticks + redzone for the connected SKU's
    // PA ceiling.  Called from currentRadioChanged subscriber wired in
    // wireControls().  HPSDRModel::FIRST is a safe sentinel default
    // (yields 100 W ceiling).  Bench-reported #167 follow-up.
    void rescaleFwdGaugeForModel(HPSDRModel model);

    // K.2: MOX button tooltip override based on current DSP mode.
    // Public static so tests can call it directly without constructing a full
    // TxApplet instance. Returns the rejection reason for deferred modes
    // (CW → 3M-2, AM/FM/SAM/DSB/DRM → 3M-3) or the normal tooltip otherwise.
    static QString tooltipForMode(DSPMode mode);

    // ── Phase 3M-1c J.1: TX Profile combo wiring ────────────────────────────
    // MainWindow (Phase L) injects MicProfileManager via this setter so the
    // combo populates from manager.profileNames() and round-trips selection
    // via setActiveProfile.  Pass nullptr to clear (e.g. on radio
    // disconnect).  TxApplet does NOT take ownership.
    void setMicProfileManager(MicProfileManager* mgr);

    // ── Phase 3M-1c J.2: 2-TONE button wiring ───────────────────────────────
    // MainWindow (Phase L) injects TwoToneController via this setter.  The
    // button's clicked-state drives controller.setActive(checked); the
    // controller's twoToneActiveChanged signal mirrors back into the button.
    void setTwoToneController(TwoToneController* controller);

    // ── Phase 3M-3a-ii Batch 6 (Task A): TxCfcDialog launch ─────────────────
    // Public slot so external surfaces (CfcSetupPage's [Configure CFC bands…]
    // button, Tools menu, etc.) can route to the same modeless dialog
    // instance owned by this applet.  Lazy-creates m_cfcDialog on first call,
    // then show()+raise()+activateWindow().  Safe to call when m_model is
    // null (no-op).
public slots:
    void requestOpenCfcDialog();

    // ── Phase 3M-4 Task 13: capability-gated PS-A button visibility ────────
    // Called by MainWindow on RadioModel::currentRadioChanged (and once at
    // startup) so the [PS-A] toggle hides on boards without PureSignal
    // support (HL2 / Atlas) and shows on PS-capable boards (G2-class /
    // Hermes II / Angelia / Orion / Saturn).  Mirrors the existing
    // RxApplet::setBoardCapabilities pattern.
    void setBoardCapabilities(const NereusSDR::BoardCapabilities& caps);

    // ── Phase 3M-4 Task 13: late-bound PureSignal coordinator wiring ───────
    // PureSignal is constructed by RadioModel inside the WDSP-init lambda
    // AFTER the TxApplet exists.  TxApplet listens to
    // RadioModel::pureSignalCoordinatorReady to wire the [PS-A] toggle
    // when the coordinator becomes available.  Tests call this slot
    // directly with their own coordinator instance.
    void setPureSignal(NereusSDR::PureSignal* coordinator);

    // Remote-station presentation gate. MainWindow supplies true only after
    // the station handshake completes and its negotiated capabilities permit
    // transmit. This changes widget availability only: it deliberately does
    // not alter TransmitModel, station settings, or the displayed state of an
    // already-authoritative control.
    void setTransmitPermitted(bool permitted, const QString& unavailableReason = QString());
    // Fix wave 2 (M8): VOX listens to this computer's microphone line to
    // the Core. While the window has none, the VOX button is shown disabled
    // with `reason` (on top of setTransmitPermitted); the Core's refusal
    // stays the backstop. Always true in a local window.
    void setVoxPermitted(bool permitted, const QString& reason = QString());
    // R-R3-49 (parity Task 1): the transmit settings that key nothing (RF
    // Power, TX filter low and high). In a remote window MainWindow
    // supplies true while the Core takes them (transmitSettingsVersion)
    // and its radio is off the air, and the reason otherwise. Widget
    // availability only, as setTransmitPermitted.
    void setTransmitSettingsPermitted(bool permitted,
                                      const QString& unavailableReason = QString());
    // R-R3-49 (parity Task 2): the rest of this applet's transmit settings
    // (Tune Power, the VOX level and delay, MON and its level, the MON
    // output pair, LEV, EQ, CFC), live while the Core takes them
    // (transmitSettingsVersion 2) and its radio is off the air. The VOX
    // button, TUNE, MOX, 2-Tone, PS-A and the profile stay on
    // setTransmitPermitted.
    void setTransmitChainSettingsPermitted(bool permitted,
                                           const QString& unavailableReason = QString());
    // R-R3-49 (parity Task 3): the profile combo. In a remote window it
    // lists the Core's profiles and selects through the Core
    // (transmitSettingsVersion 3), live while its radio is off the air.
    void setTxProfilePermitted(bool permitted,
                               const QString& unavailableReason = QString());
    // Remote-window parity Task 32 (R-IOS-13, R-R3-49): the MON output pair
    // (SPEAKERS / PHONES) in a remote window. MainWindow supplies false with
    // monitorOutputUnavailableReason() on a Core below txMonitorAudioVersion
    // 1, where this computer's choice would reach nothing; on top of
    // setTransmitChainSettingsPermitted. MON itself stays on that gate.
    void setMonitorOutputPermitted(bool permitted,
                                   const QString& unavailableReason = QString());
    static QString monitorOutputUnavailableReason();
    // R-R3-49 (group A fix wave, M3): whether an RF Power move also writes
    // the tune drive source (tuneDrivePowerSource on the link), which a Core
    // takes from transmitSettingsVersion 5. MainWindow supplies false for an
    // older Core, which would refuse it on every move. Always true locally.
    // The per-band power is never written from a window: the Core owns it.
    void setPowerByBandPermitted(bool permitted) { m_powerByBandPermitted = permitted; }
    // R-R3-49 (parity Task 4): the CFC dialog (transmitSettingsVersion 4).
    // The EQ and CFC right-clicks open their dialogs in any window; this
    // greys the CFC dialog with the reason while a remote window cannot
    // change it (the TX EQ dialog takes TxEqDialog::setSettingsPermitted).
    void setTxProcessingPermitted(bool permitted,
                                  const QString& unavailableReason = QString());
    // The CFC dialog, once a right-click or Setup has built it.
    TxCfcDialog* cfcDialog() const { return m_cfcDialog; }
    // R-R3-49 (parity Task 33): a remote window's CFC bar chart comes from
    // the Core. `setWanted` asks for (true) or lets go of (false) the Core's
    // CFC display while the dialog is shown; applyStationCfcCompression
    // hands the dialog each reading; setStationCfcBarChartUnavailable says
    // why there is none (empty when there is).
    void setStationCfcBarChart(std::function<void(bool)> setWanted);
    void applyStationCfcCompression(const QList<double>& binsDb);
    void setStationCfcBarChartUnavailable(const QString& reason);
    // Parity Task 33: the RF Pwr and SWR bars, for a test.
    HGauge* fwdPowerGauge() const { return m_fwdPowerGauge; }
    HGauge* swrGauge() const { return m_swrGauge; }
    // R-R3-49 (parity Task 7): PS-A arms PureSignal and keys nothing. It
    // follows this gate (not setTransmitPermitted): a remote window whose
    // Core offers arming (transmitSettingsVersion 7) uses it while the
    // Core's radio is off the air; otherwise it is greyed with the reason.
    // Always true locally.
    void setPureSignalArmingPermitted(bool permitted,
                                      const QString& unavailableReason = QString());
public:

    // ── Test accessors ──────────────────────────────────────────────────────
    // Always-on (no NEREUS_BUILD_TESTS guard) — same convention as
    // TestTwoTonePage (matches AudioTxInputPage / RxApplet patterns).
    QComboBox*   profileCombo()      const { return m_profileCombo; }
    QPushButton* twoToneButton()     const { return m_twoToneBtn; }
    // Task 16: the keying buttons receive only disables.
    QPushButton* moxButton()         const { return m_moxBtn; }
    QPushButton* tuneButton()        const { return m_tuneBtn; }
    // Borrowed desktop host routes key requests through its holder gate.
    // Empty handlers restore direct-local behavior.
    void setDesktopKeyHandlers(std::function<void(bool)> mox,
                               std::function<void(bool)> tune,
                               std::function<bool()> moxOn,
                               std::function<bool()> tuneOn);
    void syncDesktopKeyState();
    // Fix wave (hosting 2-TONE parity): a hosting window's 2-TONE press
    // goes through the holder gate's take question. Empty: direct-local.
    void setDesktopTwoToneHandler(std::function<void(bool)> request);
    /// Slice control plan Task 11 (Q15): the slice this window transmits
    /// on. The TX band, the per-band RF power, the MOX mode tooltip and the
    /// TX filter status follow it, never a slice this window only listens
    /// to (Thetis TXBand follows the transmit VFO, console.cs:35753
    /// [v2.10.3.15]). Empty: the radio's TX-bound slice, else the slice
    /// flagged for transmit, else the active slice.
    void setTransmitSliceResolver(std::function<SliceModel*()> resolver);
    SliceModel* transmitSlice() const;
    /// The band the TX applet's per-band controls read and write now.
    Band currentBand() const { return m_currentBand; }
    /// Slice control plan Task 11 (U8): one letter button per slice this
    /// window controls (`controlled`); the checked one transmits. A press
    /// calls `choose` with the slice id (a move while keyed unkeys first,
    /// ruling 8.10). `unavailableReason` non-empty shows the row disabled
    /// with that reason. Empty functions: every slice, the radio's own
    /// handoff (RadioModel::requestTxHandoffToSlice), always available.
    void setTransmitSliceChoices(std::function<bool(int)> controlled,
                                 std::function<void(int)> choose,
                                 std::function<QString()> unavailableReason = {});
    /// Rebuilds the letter row (a slice came or went, control changed).
    void refreshTransmitSliceChoices();
    QList<QPushButton*> transmitSliceButtons() const { return m_txSliceButtons; }
    QPushButton* voxButton()         const { return m_voxBtn; }
    // Issue #175 Task 7: HL2 slider rescale + dB label test access.
    QSlider*     rfPowerSlider()    const noexcept { return m_rfPowerSlider; }
    /// Fix wave I4: a remote window's line under MOX and TUNE saying who
    /// holds transmit on the Core ("Grant's iPhone holds transmit.",
    /// "... holds transmit and is away.", "Transmit is changing hands.").
    /// Empty hides it.
    void setTransmitHolderText(const QString& text);
    /// iPhone app plan Task 78: the Take transmit button under the holder
    /// line, while another device (or the radio) holds transmit and this
    /// window can take it. Outlined red while the holder is on the air.
    void setTakeTransmitOffered(bool offered, bool holderOnAir);
    bool takeTransmitOffered() const;
    QPushButton* takeTransmitButton() const { return m_takeTransmitBtn; }
    QString transmitHolderText() const;
    QSlider*     tunePowerSlider()  const noexcept { return m_tunePwrSlider; }
    QLabel*      rfPowerLabel()     const noexcept { return m_rfPowerValue; }
    QLabel*      tunePowerLabel()   const noexcept { return m_tunePwrValue; }

    // ── Issue #175 Task 7: per-SKU power-slider rescale + dB labels ─────────
    // mi0bot-Thetis HL2 parity helpers.
    //
    // rescalePowerSlidersForModel(): adjusts RF Power and Tune Power slider
    // ranges/steps for the given SKU (HL2 → 0..90/6 and 0..99/3, all others
    // → canonical Thetis 0..100/1) and rewrites both slider tooltips with
    // Thetis-faithful "Transmit Drive - relative value" wording.  Wired to
    // RadioModel::currentRadioChanged in wireControls() alongside the
    // existing rescaleFwdGaugeForModel() call.
    //
    // updatePowerSliderLabels(): rewrites the inset value labels using the
    // mi0bot HL2 dB formula on HL2 (RF: (round(drv/6.0)/2)-7.5; Tune:
    // (slider/3.0-33.0)/2.0) or the bare integer slider value on every
    // other SKU.  Called from rescalePowerSlidersForModel() and from each
    // slider's valueChanged signal.
    //
    // From mi0bot-Thetis console.cs:2098-2108 (slider Max/step) +
    // console.cs:29245-29274 (label formula) [v2.10.3.13-beta2].
    void rescalePowerSlidersForModel(HPSDRModel m);
    void updatePowerSliderLabels();

signals:
    /// Task 78: the Take transmit button was clicked.
    void takeTransmitRequested();
    // ── Phase 3M-1c J.1: right-click on TX Profile combo ────────────────────
    // Mirrors Thetis comboTXProfile_MouseDown (console.cs:44519-44522
    // [v2.10.3.13]):
    //     if (e.Button == MouseButtons.Right) {
    //         SetupForm.Show();
    //         SetupForm.ActivateTXProfileTab();
    //     }
    //
    // MainWindow (Phase L) connects this to a slot opening SetupDialog at
    // the "TX Profile" page.  The signal carries no payload.
    void txProfileMenuRequested();

    // ── Phase 3M-3a-iii bench polish (2026-05-04) ──────────────────────────
    /// Right-click on the VOX button opens Setup → Transmit → DEXP/VOX.
    /// Mirrors PhoneCwApplet::openSetupRequested (kept on PhoneCwApplet for
    /// the DEXP row).  MainWindow listens and jumps the SetupDialog to the
    /// requested leaf page.  `category` is informational (currently always
    /// "Transmit"); `page` is the SetupDialog leaf-item label (currently
    /// always "DEXP/VOX").
    void openSetupRequested(const QString& category, const QString& page);

    // ── Phase 3M-4 Task 13: PS-A right-click → open PsForm ─────────────────
    // Mirrors Thetis chkFWCATUBypass_MouseDown (console.cs:46149-46152
    // [v2.10.3.13]):
    //   if (IsRightButton(e)) linearityToolStripMenuItem_Click(null, EventArgs.Empty);
    // MainWindow connects this signal to openPureSignalDialog (Tools →
    // PureSignal…); the same singleton dialog instance is opened by the
    // [PS-A] button right-click and the matching control on PureSignalApplet.
    void openPureSignalDialogRequested();

private slots:
    /// Phase 3M-3a-iii bench polish (2026-05-04): 100 ms timer slot — pulls
    /// live VOX peak from TxChannel::getDexpPeakSignal(), maps it to 0..1
    /// and pushes to m_voxPeakMeter; pulls voxThresholdDb from TM and pushes
    /// the threshold marker line.  Continuous (NOT MOX-gated) since
    /// GetDEXPPeakSignal is the live DEXP detector envelope, not the
    /// TX-pipeline meter.  Cadence matches Thetis UpdateNoiseGate
    /// Task.Delay(100) at console.cs:25347 [v2.10.3.13].
    void pollVoxMeter();

private:
    void buildUI();
    void wireControls();  // called after buildUI() — attaches signals/slots
    void syncPsaFromFacade();
    // Fix wave GUI-I7: put back PS-A's tooltip under the facade's reason.
    void removePsaFacadeReason();
    // R-R3-45: show the MON output choice (true = headphones) without
    // writing it back, then refresh the notice.
    void showMonitorOutput(bool headphones);
    void updateMonitorOutputNotice();
    // K.2: slot called when SliceModel::dspModeChanged fires (via RadioModel).
    // Updates m_moxBtn->setToolTip(tooltipForMode(mode)).
    void onMoxModeChanged(DSPMode mode);

    // Task 16: receive only disables MOX (in every mode, fix wave I3), TUNE,
    // 2-Tone and VOX with its reason, as Thetis console.RXOnly does
    // (console.cs:15318-15324 [v2.10.3.15]). It sits on top of the remote
    // transmit-permission gate: remove it, change the layer below, put it
    // back.
    void removeReceiveOnlyLock();
    void applyReceiveOnlyLock();
    // Task 16 fix wave (M3), slice control plan Task 11: follow the
    // transmit slice's mode for the MOX tooltip and the TX filter status,
    // and its frequency for the TX band; the connections are the current
    // slice's.
    void followTransmitSlice();
    void refreshTxFilterStatus();
    // Connects each slice's txSliceChanged once (Qt::UniqueConnection).
    void watchTransmitFlags();
    void onSliceTransmitFlagChanged(bool isTx);
    QMetaObject::Connection m_moxModeConnection;
    QMetaObject::Connection m_txFreqConnection;
    QPointer<SliceModel> m_followedTxSlice;
    // Slice control plan Task 11 fix: the followed transmit slice's band,
    // held while transmitting (Thetis's _tx_band under its MOX gate).
    Band m_txBand{Band::Band20m};
    bool m_txBandKnown{false};

    // Canonical TX band derived from the transmit slice's frequency.  This
    // is the band the radio actually transmits on (RadioModel.cpp:903-905
    // uses the same expression for the TX wire path).  Distinct from
    // m_currentBand, which tracks UI state (since slice control plan Task
    // 11 it follows the transmit slice's band through followTransmitSlice,
    // and setCurrentBand can still set it directly), so it is not the
    // storage key for per-band TX state.  Falls back to
    // m_currentBand when the transmit slice is unavailable (early bootstrap
    // or after disconnection).
    Band txBand() const;

    // ── J.1: combo refresh helpers ──────────────────────────────────────────
    // Rebuild combo entries from m_micProfileMgr->profileNames(), preserving
    // the currently-active profile selection where possible.  Does nothing
    // when the manager is null.  Uses QSignalBlocker to suppress the
    // currentTextChanged echo that would otherwise call back into the model.
    void rebuildProfileCombo();

    // ── J.1/J.2: non-owning controller pointers ─────────────────────────────
    MicProfileManager* m_micProfileMgr{nullptr};
    TwoToneController* m_twoToneCtrl{nullptr};

    // 0. Mic-source badge (J.3 Phase 3M-1b) — read-only label above the gauges.
    QLabel*  m_micSourceBadge = nullptr;
    QLabel*  m_holderLabel = nullptr;   // fix wave I4
    QPushButton* m_takeTransmitBtn = nullptr;  // Task 78
    // 1. Forward Power gauge
    HGauge*  m_fwdPowerGauge  = nullptr;
    // 2. SWR gauge
    HGauge*  m_swrGauge       = nullptr;
    // EMA smoothing state for fwd-power gauge (Thetis-style envelope detector
    // not yet ported; this is a simple alpha=0.25 exponential-moving-average
    // to keep the displayed value calm — RadioStatus::powerChanged fires
    // ~twice per hardware sample, which makes raw values visibly jittery).
    double   m_fwdPowerSmoothedW{0.0};
    // 3. RF Power
    QSlider* m_rfPowerSlider  = nullptr;
    QLabel*  m_rfPowerValue   = nullptr;
    // 4. Tune Power
    QSlider* m_tunePwrSlider  = nullptr;
    QLabel*  m_tunePwrValue   = nullptr;
    // ── 4b. VOX row (3M-3a-iii bench polish 2026-05-04) ───────────────────
    // Relocated from PhoneCwApplet (Phone tab Control #10).  Layout:
    //   [VOX btn 48px] [Threshold slider + DexpPeakMeter stack][-N dB inset]
    //   [Hold slider 1..2000 ms][N ms inset]
    // Threshold range -80..0 dB matches Thetis ptbVOX
    // (console.Designer.cs:6018-6019 [v2.10.3.13]).
    // Hold range 1..2000 ms matches Thetis udDEXPHold
    // (setup.designer.cs:45005-45013 [v2.10.3.13]).
    // Bidirectional with TransmitModel::voxEnabled / voxThresholdDb /
    // voxHangTimeMs.  Right-click on the VOX button → openSetupRequested.
    QPushButton*    m_voxBtn{nullptr};
    QSlider*        m_voxSlider{nullptr};         // VOX threshold (-80..0 dB)
    QLabel*         m_voxLvlLabel{nullptr};
    QSlider*        m_voxDlySlider{nullptr};      // Hold time (1..2000 ms)
    QLabel*         m_voxDlyLabel{nullptr};
    DexpPeakMeter*  m_voxPeakMeter{nullptr};
    // 100 ms QTimer driving m_voxPeakMeter.  Cadence matches Thetis
    // UpdateNoiseGate Task.Delay(100) at console.cs:25347 [v2.10.3.13].
    QTimer*         m_voxMeterTimer{nullptr};
    // 4c. MON toggle (J.3 Phase 3M-1b) — bidirectional with TransmitModel::monEnabled
    QPushButton* m_monBtn     = nullptr;
    // 4d. Monitor volume slider (J.3 Phase 3M-1b) — 0..100 → monitorVolume 0.0..1.0
    //     Default 50 (matches model default 0.5f from Thetis audio.cs:417).
    QSlider*     m_monitorVolumeSlider = nullptr;
    QLabel*      m_monitorVolumeValue  = nullptr;
    // R-R3-45: where MON plays, beside the MON button. Exclusive pair
    // bound to AudioEngine::txMonitorOutput (local window only), plus the
    // plain notice shown when the headphones are chosen and not open.
    QPushButton* m_monSpeakersBtn   = nullptr;
    QPushButton* m_monHeadphonesBtn = nullptr;
    QLabel*      m_monOutputNotice  = nullptr;
    bool         m_headphonesAvailable = false;
    bool         m_headphonesEnabled   = false;
    // 4e. TX-processing quick toggles — row of 3 (3M-3a-ii post-bench cleanup
    //     drops the duplicate PROC button; PROC lives on PhoneCwApplet which
    //     already had a wired button + slider sitting un-wired since 3I-3):
    //     LEV: bidirectional ↔ TransmitModel.txLevelerOn (green-checked style)
    //     EQ:  bidirectional ↔ TransmitModel.txEqEnabled (green-checked style)
    //          right-click → TxEqDialog (3M-3a-i Batch 3)
    //     CFC: bidirectional ↔ TransmitModel.cfcEnabled (3M-3a-ii Batch 6 Task A)
    //          right-click → TxCfcDialog (modeless, mirrors EQ launch pattern)
    QPushButton* m_levBtn     = nullptr;
    QPushButton* m_eqBtn      = nullptr;
    QPushButton* m_cfcBtn     = nullptr;
    // ── 3M-3a-ii Batch 6 (Task A): modeless CFC dialog instance ─────────────
    // Lazy-created on first right-click of [CFC] or first call to
    // requestOpenCfcDialog().  Lives until applet (parent window) is destroyed.
    TxCfcDialog* m_cfcDialog  = nullptr;
    // Parity Task 33: a remote window's CFC chart source and its note.
    std::function<void(bool)> m_stationCfcBarChart;
    QString m_stationCfcBarChartReason;
    // 5. MOX
    QPushButton* m_moxBtn     = nullptr;
    // 6. TUNE
    QPushButton* m_tuneBtn    = nullptr;
    // 7. TX Profile combo
    QComboBox*   m_profileCombo = nullptr;
    // 8. 2-Tone test
    QPushButton* m_twoToneBtn = nullptr;
    // 9. PS-A (Phase 3M-4 Task 13)
    QPushButton* m_psaBtn     = nullptr;
    // Shared session facade owned by RadioModel. setPureSignal() changes its
    // local coordinator only for the established standalone test seam.
    QPointer<NereusSDR::PureSignalSessionFacade> m_psFacade;
    // ── Plan 4 Cluster C (Task 4 / D2+D3+D9-status): TX BW spinbox row ─────────
    // Low/High cutoff spinboxes (Hz) — bidirectional with TransmitModel::filterLow
    // and filterHigh via the filterChanged(int,int) signal.
    QSpinBox*    m_txFilterLowSpin{nullptr};
    QSpinBox*    m_txFilterHighSpin{nullptr};
    // Status label below the spinbox row — shows "100-2900 Hz · 2.8k BW" or
    // "±2900 Hz · 5.8k BW" depending on whether the active slice mode is
    // symmetric.  Orange tint (#ffaa70) matches the future Style::kTxFilterOverlayLabel
    // constant (Cluster E will add it to StyleConstants.h).
    QLabel*      m_txFilterStatusLabel{nullptr};

    // 10. SWR protection LED (wired to SwrProtectionController::highSwrChanged)
    QLabel*      m_swrProtLed = nullptr;
    // Removed NYI members (re-add when their phases ship):
    //   m_atuBtn      — ATU phase (no plan yet)
    //   m_memBtn      — channel-memory phase
    //   m_tuneModeCombo — ATU phase (no plan yet)
    //   m_dupBtn      — full-duplex audio routing phase
    //   m_xpaBtn      — external-PA hardware-specific phase

    // Current band — used to resolve per-band tune power.
    // Updated by setCurrentBand() when PanadapterModel::bandChanged fires.
    Band m_currentBand{Band::Band20m};

    // Issue #175 Task 7: cached SKU for the slider/label formatter.  Set by
    // rescalePowerSlidersForModel() so updatePowerSliderLabels() formats in
    // the SKU that the slider was last rescaled for, not whatever the live
    // RadioModel happens to report (the two diverge during unit tests where
    // the test calls rescalePowerSlidersForModel directly without rebuilding
    // the radio profile).  Default HPSDRModel::FIRST is the same sentinel
    // used by rescaleFwdGaugeForModel(HPSDRModel::FIRST) at construction.
    HPSDRModel m_powerSliderModel{HPSDRModel::FIRST};

    // Flag preventing echo loops between the model and the UI.
    bool m_updatingFromModel{false};
    std::function<void(bool)> m_desktopMoxRequest;
    std::function<void(bool)> m_desktopTuneRequest;
    std::function<bool()> m_desktopMoxOn;
    std::function<bool()> m_desktopTuneOn;
    std::function<void(bool)> m_desktopTwoToneRequest;
    std::function<SliceModel*()> m_transmitSliceResolver;
    // Slice control plan Task 11 (U8): the transmit-slice letter row.
    QHBoxLayout* m_txSliceRow = nullptr;
    QList<QPushButton*> m_txSliceButtons;
    std::function<bool(int)> m_txSliceControlled;
    std::function<void(int)> m_txSliceChoose;
    std::function<QString()> m_txSliceUnavailable;

    // Defaults to local-direct behaviour. Remote MainWindow wiring replaces it
    // after handshake/capability evaluation.
    bool m_transmitPermitted{true};
    bool m_transmitSettingsPermitted{true};
    bool m_transmitChainSettingsPermitted{true};
    QString m_transmitChainSettingsReason;
    bool m_monitorOutputPermitted{true};  // R-R3-49 (parity Task 32)
    QString m_monitorOutputReason;
    void applyMonitorOutputGate();
    bool m_txProfilePermitted{true};
    bool m_txProcessingPermitted{true};   // R-R3-49 (parity Task 4)
    bool m_powerByBandPermitted{true};    // R-R3-49 (group A fix wave, M3)
    bool m_psArmingPermitted{true};       // R-R3-49 (parity Task 7)
    // Fix round 1 (minor 5): the board is not known (no radio, so the caps
    // fall back to Unknown) and does not say it has PureSignal: PS-A shows,
    // disabled, with PureSignalSessionFacade::needsRadioReason().
    bool m_psBoardUnknown{false};
    QString m_txProcessingReason;
    // R-R3-49 (parity Task 2): a remote window's Tune Power slider asks the
    // Core (setTunePowerForTxBand) and shows the Core's tunePowerForTxBand.
    bool remoteTunePower() const;
    int  shownTunePower(Band band) const;
    void requestRemoteTunePower(int watts);
    // The words the transmit-permission gate shows while it holds; the
    // receive-only lock names them beside its own (Task 16 fix wave, M6).
    QString m_transmitPermissionReason;
    bool m_voxPermitted{true};      // fix wave 2 (M8)
    QString m_voxReason;
};

} // namespace NereusSDR
