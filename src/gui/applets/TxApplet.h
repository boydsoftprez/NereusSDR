#pragma once

// =================================================================
// src/gui/applets/TxApplet.h  (Longpath)
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
// Modification history (Longpath):
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
//   Longpath is also GPLv3. Attribution follows GPLv3 §5 requirements.
// =================================================================

#include "AppletWidget.h"
#include <QPointer>
#include "models/Band.h"
#include "core/BoardCapabilities.h"  // setBoardCapabilities slot
#include "core/HpsdrModel.h"   // HPSDRModel for rescaleFwdGaugeForModel
#include "core/WdspTypes.h"

class QPushButton;
class QSlider;
class QComboBox;
class QLabel;
class QSpinBox;
class QTimer;

namespace Longpath {

class HGauge;
class MicProfileManager;
class PureSignal;
class TwoToneController;
class TxCfcDialog;
class DexpPeakMeter;

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

    QString appletId() const override { return QStringLiteral("Tx"); }
    QString appletTitle() const override { return QStringLiteral("TX"); }
    void syncFromModel() override;

    // Der ⚙-Knopf in der geteilten Kopfleiste (GridCellWidget) oeffnet
    // denselben Popup wie der bestehende ⚙-Knopf im Applet selbst
    // (showFinePopup) — zwei Wege zum selben Ziel, bis der interne
    // Knopf in einem eigenen Schritt entfernt wird.
    bool hasExtendedSettings() const override { return true; }
    void openExtendedSettings() override { showFinePopup(); }

    // Called by MainWindow when band changes so the Tune Power slider
    // can reflect the stored per-band tune power.
    // Phase 3M-1a H.3.
    void setCurrentBand(Band band);

    // Code review, 2026-09-13: the MOX-tooltip (K.2) and TX-filter-status-
    // label connections to SliceModel::dspModeChanged were only ever made
    // once, in wireControls(), against whichever slice m_model->activeSlice()
    // returned at construction time -- never re-bound when the operator
    // switches the active slice (RadioModel::activeSliceChanged), so both
    // stayed wired to the wrong (or, since TxApplet is built before any
    // slice exists, no) slice for the rest of the session. Call this
    // whenever the active slice changes; MainWindow does so on
    // RadioModel::activeSliceChanged, mirroring how it already re-binds
    // RxApplet via RxApplet::setSlice().
    void rebindActiveSlice();

    // Rescale the RF Pwr HGauge ticks + redzone for the connected SKU's
    // PA ceiling.  Called from currentRadioChanged subscriber wired in
    // wireControls().  HPSDRModel::FIRST is a safe sentinel default
    // (yields 100 W ceiling).  Bench-reported #167 follow-up.
    void rescaleFwdGaugeForModel(HPSDRModel model);

    // ── Der Verstaerker verschiebt die Skala ─────────────────────────
    //
    // Bis 2026-08-18 stand das nur an der analogen S-Meter-Anzeige im
    // Panelkopf (SMeterWidget::setPowerScale): mit PGXL oder RF-Kit in
    // OPERATE sprang sie auf 2 kW, waehrend die Leistungsanzeige HIER
    // weiter auf der Barfussskala des Geraets stand und bei jedem
    // Sendevorgang am Anschlag klebte.
    //
    // OE5SOS, 2026-08-18: „Die 2-kW-Skala fuer PGXL und RF-Kit geht mit
    // ihnen — sie gehoert zur Leistungsanzeige, nicht zum
    // Empfangszeiger."
    //
    // `hasAmplifier` heisst „verstaerkt gerade", nicht „ist
    // angeschlossen": ein PGXL in STANDBY reicht die Leistung des
    // Geraets durch, und dann ist die Barfussskala die richtige.
    // Wortgleich mit TunerApplet::setPowerScale, damit die drei
    // Anzeigen nicht auseinanderlaufen koennen.
    void setPowerScale(int maxWatts, bool hasAmplifier);

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
    void setBoardCapabilities(const Longpath::BoardCapabilities& caps);

    // ── Phase 3M-4 Task 13: late-bound PureSignal coordinator wiring ───────
    // PureSignal is constructed by RadioModel inside the WDSP-init lambda
    // AFTER the TxApplet exists.  TxApplet listens to
    // RadioModel::pureSignalCoordinatorReady to wire the [PS-A] toggle
    // when the coordinator becomes available.  Tests call this slot
    // directly with their own coordinator instance.
    void setPureSignal(Longpath::PureSignal* coordinator);
public:

    // ── Test accessors ──────────────────────────────────────────────────────
    // Always-on (no LONGPATH_BUILD_TESTS guard) — same convention as
    // TestTwoTonePage (matches AudioTxInputPage / RxApplet patterns).
    QComboBox*   profileCombo()      const { return m_profileCombo; }
    QPushButton* twoToneButton()     const { return m_twoToneBtn; }
    // Issue #175 Task 7: HL2 slider rescale + dB label test access.
    QSlider*     rfPowerSlider()    const noexcept { return m_rfPowerSlider; }
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
    void buildFinePopup();
    void showFinePopup();
    void wireControls();  // called after buildUI() — attaches signals/slots
    // K.2: slot called when SliceModel::dspModeChanged fires (via RadioModel).
    // Updates m_moxBtn->setToolTip(tooltipForMode(mode)).
    void onMoxModeChanged(DSPMode mode);
    // Promoted out of wireControls()'s local lambda (2026-09-13) so
    // rebindActiveSlice() can call it too. Reads m_model->activeSlice()
    // fresh each call -- shared by TransmitModel::filterChanged and the
    // active slice's dspModeChanged (symmetric vs. asymmetric format).
    void refreshTxFilterStatusLabel();

    // Canonical TX band derived from the active slice's frequency.  This
    // is the band the radio actually transmits on (RadioModel.cpp:903-905
    // uses the same expression for the TX wire path).  Distinct from
    // m_currentBand, which tracks UI state and is fed by both
    // PanadapterModel::bandChanged AND SliceModel::frequencyChanged from
    // MainWindow — m_currentBand can drift to the panadapter band when
    // the user pans without retuning the slice (CTUN), so it is NOT safe
    // to use as the storage key for per-band TX state.  Falls back to
    // m_currentBand when the active slice is unavailable (early bootstrap
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
    // Fusszeile "PC mic": Quelle, und beim Ausfall des PC-Mikrofons die
    // Warnung (2026-11-20, Entwurf A). Die eine Stelle, die den Text setzt.
    void aktualisiereMikroAbzeichen();
    // 1. Forward Power gauge
    HGauge*  m_fwdPowerGauge  = nullptr;
    // 2. SWR gauge
    HGauge*  m_swrGauge       = nullptr;
    // EMA smoothing state for fwd-power gauge (Thetis-style envelope detector
    // not yet ported; this is a simple alpha=0.25 exponential-moving-average
    // to keep the displayed value calm — RadioStatus::powerChanged fires
    // ~twice per hardware sample, which makes raw values visibly jittery).
    double   m_fwdPowerSmoothedW{0.0};
    /// Spitzenwerthalt waehrend einer Sendung (PEP), siehe
    /// wireControls(): die Flex schickt FWDPWR nicht in jedem Paket,
    /// und Sprache hat keine konstante Huellkurve.
    bool     m_inTx{false};
    double   m_fwdPeakW{0.0};
    /// Bis wann nach dem Loesen keine Leistungswerte angenommen
    /// werden (Uhrzeit in ms). Der Messwertstrom der Flex laeuft
    /// dem Loesen hinterher, siehe wireControls().
    qint64   m_fwdSperreBisMs{0};
    // 3. RF Power
    QSlider* m_rfPowerSlider  = nullptr;
    QLabel*  m_rfPowerValue   = nullptr;
    // 4. Tune Power
    QSlider* m_tunePwrSlider  = nullptr;
    QLabel*  m_tunePwrValue   = nullptr;
    /// Nur am Flex-Sendeweg sichtbar, siehe zeigeFlexMikRegler().
    QSlider*     m_flexMikSlider = nullptr;
    QLabel*      m_flexMikValue  = nullptr;
    QHBoxLayout* m_flexMikRow    = nullptr;
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
    // Late-bound PureSignal coordinator pointer.  Set by setPureSignal()
    // (test seam) or by RadioModel::pureSignalCoordinatorReady on connect.
    // Null until WDSP-init lambda fires; the m_psaBtn::toggled lambda
    // null-guards on this so pre-coordinator clicks are safely no-op'd.
    //
    // PR #212 follow-up bench fix (J.J. KG4VCF, 2026-05-07): converted from
    // raw pointer to QPointer to fix EXC_BAD_ACCESS crash on radio-disconnect
    // teardown.  RadioModel destroys PureSignal during disconnect, then
    // emits pureSignalCoordinatorReady(nullptr) to clear bindings via
    // setPureSignal(nullptr).  setPureSignal's `disconnect(m_ps, nullptr,
    // this, nullptr)` call dereferenced the freed PureSignal pointer.
    // QPointer auto-nulls on QObject destruction, so the m_ps null-guard
    // at setPureSignal:2010 now properly skips the redundant disconnect
    // (Qt auto-disconnects on sender destruction anyway).
    QPointer<Longpath::PureSignal> m_ps;
    // ── Plan 4 Cluster C (Task 4 / D2+D3+D9-status): TX BW spinbox row ─────────
    // Low/High cutoff spinboxes (Hz) — bidirectional with TransmitModel::filterLow
    // and filterHigh via the filterChanged(int,int) signal.
    QSpinBox*    m_txFilterLowSpin{nullptr};
    QSpinBox*    m_txFilterHighSpin{nullptr};
    // Status label below the spinbox row — shows "100-2900 Hz · 2.8k BW" or
    // "±2900 Hz · 5.8k BW" depending on whether the active slice mode is
    // symmetric.  Orange tint (#ffaa70) matches the future Style::kTxFilterOverlayLabel
    // constant (Cluster E will add it to StyleConstants.h).
    // ── Die Sendebandbreite zum Durchklicken (2026-10-16) ────────────
    //
    // Betreiber: „nur das ich anklicken kann die unterschiedlichen
    // bandbreiten durchklicken kann". Bis dahin stand sie hier nur als
    // Zustand — der Kommentar in buildUI sagte es ausdruecklich.
    //
    // Ein flacher Knopf statt einer Reihe von Knoepfen: die
    // Applet-Spalte ist schmal, und sechs Stufen nebeneinander waeren
    // abgeschnitten. Linksklick geht zur naechsten Stufe und laeuft
    // um, Rechtsklick oeffnet die ganze Liste.


    QPushButton* m_txFilterStatusBtn{nullptr};

public:
    struct TxBandbreite {
        const char* name;   ///< „2.8k"
        int lowHz;
        int highHz;
    };

    /// Die Stufen, schmal nach breit. 3.6k ist die Breite aus Martins
    /// ESSB-Profil, 5.0k die von Zeus („eSSB Wide": lowCutHz 40,
    /// highCutHz 5000), 8.0k die gewuenschte breiteste — darueber
    /// hinaus laesst der Feinblatt-Spinner ohnehin nichts zu
    /// (setRange(200, 10000)).
    static const TxBandbreite kTxBandbreiten[6];

    /// Welche Stufe passt zu low/high? -1, wenn keine — dann ist eine
    /// Breite von Hand gesetzt, und der naechste Klick geht auf die
    /// naechstgroessere Stufe, statt sie stillschweigend zu verwerfen.
    ///
    /// Oeffentlich und statisch, weil die Regel ohne Fenster zu
    /// pruefen sein soll; der Knopf ist nur der Ausloeser.
    static int stufeFuer(int lowHz, int highHz);
    static int naechsteStufe(int lowHz, int highHz);

    /// Fuer den Pruefstand: der Knopf, der die Bandbreite zeigt und
    /// durchschaltet.
    QPushButton* txFilterButtonForTest() const { return m_txFilterStatusBtn; }

private:
    void setzeBandbreite(int index);
    void zeigeBandbreitenMenue(const QPoint& pos);

    // Live connections to whichever slice is currently active, re-made by
    // rebindActiveSlice() on every RadioModel::activeSliceChanged so they
    // never point at a stale slice (2026-09-13 code review fix).
    QMetaObject::Connection m_activeSliceModeConn;
    QMetaObject::Connection m_activeSliceFilterConn;

    // 10. SWR protection LED (wired to SwrProtectionController::highSwrChanged)
    QLabel*      m_swrProtLed = nullptr;

    // ── Umbau 2026-09-02: das Feinblatt hinter dem Zahnrad ──────────────────
    // Traegt Profil, TX-Bandbreite, VOX-Schwelle/-Haltezeit und
    // Mithoerlautstaerke. Es sind dieselben Bauteile wie vorher, nur an
    // einem anderen Elternteil — objectNames und Verdrahtung unveraendert.
    QLabel*      m_profileEcho = nullptr;   // Profilname in der Fusszeile
    QPushButton* m_moreBtn    = nullptr;
    QWidget*     m_finePopup  = nullptr;
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
    HPSDRModel m_fwdGaugeModel{HPSDRModel::FIRST};

    /// Steht die Leistungsanzeige gerade auf der Verstaerkerskala?
    /// rescaleFwdGaugeForModel muss das wissen: es rechnet die Skala
    /// aus der PA-Decke des GERAETS, und waehrend ein Verstaerker
    /// arbeitet, waere das die falsche Antwort. Ein Bandwechsel bei
    /// laufendem PGXL haette die Anzeige sonst zurueck auf Barfuss
    /// gesetzt.
    bool m_ampScaleActive{false};

    /// Das Geraet, fuer das die Leistungsanzeige zuletzt skaliert
    /// wurde. Eigenes Feld, NICHT m_powerSliderModel: das gehoert den
    /// Reglern und wird von rescalePowerSlidersForModel gesetzt. Beide
    /// werden heute zusammen gerufen, aber „setPowerScale(false) stellt
    /// wieder her, was eine ANDERE Funktion sich gemerkt hat" ist die
    /// Sorte Kopplung, die genau so lange haelt, bis jemand eine der
    /// beiden einzeln ruft.

    // Flag preventing echo loops between the model and the UI.
    bool m_updatingFromModel{false};
};

} // namespace Longpath
