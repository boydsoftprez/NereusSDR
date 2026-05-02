#pragma once

// =================================================================
// src/gui/MainWindow.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Signal-routing hub, double-height status-bar layout, and
//                 TitleBar feature-request dialog ported from AetherSDR
//                 (ten9876/AetherSDR, GPLv3) src/gui/MainWindow.{h,cpp} and
//                 src/gui/TitleBar.{h,cpp}. AetherSDR has no per-file
//                 headers; project-level citation per docs/attribution/
//                 HOW-TO-PORT.md rule 6.
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

#include <QMainWindow>
#include <QLabel>
#include <QAction>
#include <QActionGroup>
#include <QTimer>

class QProgressDialog;
class QThread;
class QSplitter;
class QMenu;

namespace NereusSDR {

class RadioModel;
class ConnectionPanel;
class SupportDialog;
class WdspEngine;
class FFTEngine;
class SpectrumWidget;
class ClarityController;
class ContainerManager;
class MeterWidget;
class MeterPoller;
class TitleBar;
class VaxFirstRunDialog;
class RxDashboard;
class StationBlock;
class MetricLabel;
class StatusBadge;
class AdcOverloadBadge;
class OverflowChip;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // ── Phase 3M-0 Task 14 test accessors ────────────────────────────────
    // Returns the TX Inhibit status-bar label. Visible iff
    // TxInhibitMonitor::inhibited() is true. Wiring to the monitor lands
    // in Task 17 (final integration). Non-null after construction.
    QLabel* txInhibitLabel() const noexcept { return m_txInhibitLabel; }

    // Returns the PA status badge. Variant is On (green) or Tx (red) per
    // RadioModel::paTripped(). Wiring to RadioModel lands in Task 17.
    // Non-null after construction.
    StatusBadge* paStatusBadge() const noexcept { return m_paStatusBadge; }

public slots:
    // ── Phase 3M-0 Task 14 helper slots ──────────────────────────────────
    // Update PA status badge state. Wired by Task 17 to
    // RadioModel::paTrippedChanged.
    void setPaTripped(bool tripped);

    // Update TX Inhibit label visibility. Wired by Task 17 to
    // TxInhibitMonitor::txInhibitedChanged.
    void setTxInhibited(bool inhibited);

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onConnectionStateChanged();
    void showConnectionPanel();
    void showSupportDialog();
    void showAudioDiagnoseDialog();
    void showFeatureRequestDialog();
    void showFeatureRequestDialogImpl();
    // Phase 3Q Sub-PR-4 D.2: right-click context menu on the TitleBar
    // ConnectionSegment. Items: Disconnect / Connect-to-other / Diagnostics /
    // Copy IP / Copy MAC. "Reconnect" omitted — no RadioModel::reconnect() API.
    void showSegmentContextMenu(const QPoint& globalPos);
    // Phase 3Q Sub-PR-7 G.1: right-click context menu on the StationBlock.
    // Items: Disconnect / Edit radio… / Forget radio.
    void showStationContextMenu(const QPoint& globalPos);

private:
    void buildUI();
    void buildMenuBar();
    void buildStatusBar();
    void applyDarkTheme();
    void tryAutoReconnect();
    void wireSliceToSpectrum();

    // Re-runs the right-side strip's progressive-drop logic per design
    // §286-294. Restores all drop candidates first (in case the window
    // grew), then walks the priority order — PA OK → CAT/TCI →
    // PSU/PA → CPU → time — hiding items + their trailing separators
    // until the strip's required width fits the budget. The
    // OverflowChip is updated with the human names of any items that
    // were dropped this pass.
    //
    // Called from resizeEvent + after any visibility toggle of an
    // optional widget (ADC badge appearing, PA voltage row showing on
    // first user_adc0 signal, etc.) since those events change the
    // required width without firing a window resize.
    // force=true bypasses the budget-deadband hysteresis. Pass true when
    // calling from a content-change site (badge visibility toggle, voltage
    // row hidden/shown) — those don't move the strip's available width but
    // do change the required width, so the deadband on width alone would
    // skip the re-evaluation. Resize-driven calls leave force=false.
    void reapplyRightStripDropPriority(bool force = false);

    // CPU usage helpers — return instantaneous percent since the last call.
    // First call after a toggle returns 0 (delta-state reset). The timer
    // applies Thetis-style smoothing on top. Both helpers branch internally
    // on Q_OS_MAC / Q_OS_LINUX / Q_OS_WIN; declared on every platform so
    // the timer wiring in buildStatusBar() doesn't need a platform guard.
    //   process: getrusage(RUSAGE_SELF) on POSIX, GetProcessTimes on Windows
    //   system : host_processor_info on macOS, /proc/stat on Linux,
    //            GetSystemTimes on Windows
    double readProcessCpuPercent();
    double readSystemCpuPercent();
    // Right-click menu on m_cpuMetric — System / App radio choice.
    void onCpuMenuRequested(const QPoint& localPos);

    // Phase 3M-3a-ii Batch 6 (Task 3): one-shot wiring helper called from
    // every SetupDialog construction site.  Connects the dialog's
    // cfcDialogRequested signal to TxApplet::requestOpenCfcDialog so the
    // [Configure CFC bands…] button on Setup → DSP → CFC reuses the same
    // modeless dialog instance owned by the TxApplet.
    void wireSetupDialog(class SetupDialog* dialog);

    // Phase 3O Sub-Phase 11 Task 11b — first-launch / startup rescan
    // hook. Scheduled via QTimer::singleShot(0, ...) from the
    // constructor so it runs after the event loop starts and the UI
    // is fully built. Diffs detected cables against the persisted
    // audio/LastDetectedCables fingerprint and pops the
    // VaxFirstRunDialog in the appropriate scenario.
    void checkVaxFirstRun();

    RadioModel* m_radioModel{nullptr};
    ConnectionPanel* m_connectionPanel{nullptr};
    SupportDialog* m_supportDialog{nullptr};

    // Status bar widgets (double-height AetherSDR design, 46px)
    QLabel* m_connStatusLabel{nullptr};
    QLabel* m_radioModelLabel{nullptr};
    QLabel* m_radioFwLabel{nullptr};
    StationBlock* m_stationBlock{nullptr};    // Sub-PR-7 G.1: radio-name anchor
    QLabel* m_utcTimeLabel{nullptr};
    QTimer* m_clockTimer{nullptr};
    QLabel* m_tnfLabel{nullptr};

    // Wisdom generation dialog (shown on first run)
    QProgressDialog* m_wisdomDialog{nullptr};

    // Spectrum display
    SpectrumWidget*     m_spectrumWidget{nullptr};
    FFTEngine*          m_fftEngine{nullptr};
    QThread*            m_fftThread{nullptr};
    ClarityController*  m_clarityController{nullptr};
    class StepAttenuatorController* m_stepAttController{nullptr};
    // Right-side strip wrapper widget — the inner QWidget hosting the
    // QHBoxLayout that the buildStatusBar() routine populates. Stored
    // as a member so reapplyRightStripDropPriority() can read its
    // available width.
    QWidget* m_chromeBarWidget{nullptr};

    // Hysteresis state for reapplyRightStripDropPriority — without a
    // deadband the function re-evaluates on every resize event, and at
    // boundary widths the show/hide of indicator widgets re-fires
    // resize, looping. Same flash-class issue as RxDashboard's pair
    // stacking. See reapplyRightStripDropPriority() for details.
    int  m_rightStripLastBudget{-1};
    bool m_rightStripSettled{false};

    // Right-side strip drop targets — captured so the drop-priority
    // pass can hide them + their trailing separators in priority order.
    // Each non-separator widget has a paired separator pointer so the
    // pair hides + shows together (no dangling "··" runs).
    QWidget* m_catIndicator{nullptr};
    QLabel*  m_catSep{nullptr};
    QWidget* m_tciIndicator{nullptr};
    QLabel*  m_tciSep{nullptr};
    QLabel*  m_paVoltLabelSep{nullptr};
    QLabel*  m_paStatusBadgeSep{nullptr};
    QLabel*  m_cpuMetricSep{nullptr};
    QWidget* m_timeWidget{nullptr};

    // OverflowChip — "…" pill that surfaces drop-list contents via its
    // hover tooltip. Hidden when the drop list is empty.
    OverflowChip* m_overflowChip{nullptr};

    // (Earlier revisions had a "voltage stack" wrapper holding PSU above
    //  PA. The PSU widget was source-first audited against Thetis 2026-04-30
    //  and removed — Thetis never displays AIN6/supply_volts. The PA volt
    //  label below is the sole supply indicator; it lives directly in the
    //  hbox now with no wrapper.)
    // ADC overload alarm — stacked "ADCx / OVERLOAD" badge living in the
    // right-side strip between PA OK and TX. Width changes consume the
    // strip's right stretch space rather than shifting the STATION
    // anchor (layout-stability rule §278.4). Hidden when no ADC is in
    // overload; setVariant() flips between Warn (yellow) / Tx (red) per
    // Thetis severity rules (ucInfoBar.cs:928 [@501e3f5]).
    AdcOverloadBadge* m_adcOvlBadge{nullptr};
    // Trailing separator paired with the badge — hides + shows together
    // so the strip closes seamlessly when the alarm clears.
    QLabel* m_adcOvlSep{nullptr};
    // 2-second auto-hide timer for the ADC-overload alarm. Mirrors
    // Thetis ucInfoBar._warningTimer: restarts on each overload event,
    // hides the badge when elapsed — independent of the level-decay
    // state tracked in StepAttenuatorController. Source:
    // ucInfoBar.cs:927-932 [@501e3f5]
    QTimer* m_adcOvlHideTimer{nullptr};

    // Re-entrancy guard: prevents centerChanged from firing a second
    // forceHardwareFrequency while frequencyChanged is already retuning the DDC
    bool m_handlingBandJump{false};

    // Task 17: auto-reconnect guard — prevents the background attempt from
    // interfering with a subsequent user-initiated Start Discovery.
    bool m_autoReconnectInProgress{false};

    // Set true at the top of closeEvent (and aboutToQuit). Gates the
    // "auto-open ConnectionPanel on Disconnect" slot — without this,
    // closeEvent's disconnectFromRadio fires connectionStateChanged →
    // ConnectionPanel ctor → startDiscovery, which clears the discovery
    // stop flag and runs a fresh ~5 s NIC walk on the main thread mid-
    // close. Symptom: ⌘Q beach-balls for the full SafeDefault scan time.
    bool m_shuttingDown{false};

    // Container infrastructure (Phase 3G-1)
    ContainerManager* m_containerManager{nullptr};
    QSplitter* m_mainSplitter{nullptr};
    int m_hDelta{0};
    int m_vDelta{0};

    void createDefaultContainers();

    // Phase 3G-6 block 6: dynamic "Edit Container ▸" submenu,
    // populated from ContainerManager::allContainers() and rebuilt
    // whenever a container is added, removed, or retitled. Addresses
    // the block 4 review observation that there was no way to see
    // or manage already-created containers from the menu bar.
    QMenu* m_editContainerMenu{nullptr};
    void rebuildEditContainerSubmenu();
    void resetDefaultLayout();

    // Meter system (Phase 3G-2)
    MeterWidget* m_meterWidget{nullptr};
    MeterPoller* m_meterPoller{nullptr};
    void populateDefaultMeter();

    // Menu DSP actions
    // NR / NB submenus use exclusive QActionGroups; SNB / APF / BIN are
    // single toggle actions that mirror SliceModel state.
    QActionGroup* m_nrGroup   = nullptr;
    QActionGroup* m_nbGroup   = nullptr;
    QAction*      m_snbAction = nullptr;
    QAction*      m_apfAction = nullptr;
    QAction*      m_binAction = nullptr;

    // Mode menu actions (12 modes, mutual exclusion via QActionGroup)
    QAction*      m_modeActions[12]  = {};
    QActionGroup* m_modeActionGroup  = nullptr;

    // AGC menu action group (Task 12)
    QActionGroup* m_agcGroup = nullptr;

    // Dark theme checkable action (Task 12)
    QAction* m_darkThemeAction = nullptr;

    // Radio menu state-aware actions (3Q-9; trimmed in 3Q polish — Discover Now
    // dropped because Manage Radios already exposes a ↻ Scan button).
    QAction* m_actConnect      = nullptr;
    QAction* m_actDisconnect   = nullptr;
    QAction* m_actManageRadios = nullptr;
    QAction* m_actProtocolInfo = nullptr;

    // Status bar members (Task 13 / sub-PR-8 restyle)
    MetricLabel* m_paVoltLabel{nullptr};    // "PA  13.8V"  — MKII-class only (Saturn / G2 / 8000D / 7000DLE / OrionMkII / Anvelina Pro 3); Thetis-faithful — see PSU drop note above
    MetricLabel* m_cpuMetric{nullptr};      // "CPU  19.1%"
    QTimer*      m_cpuTimer{nullptr};

    // CPU usage source — System (whole machine) or App (this process).
    // Thetis equivalent: m_bShowSystemCPUUsage (console.cs:20668), default
    // true. Right-click on m_cpuMetric pops a menu with the two choices,
    // matching Thetis's toolStripDropDownButton_CPU. Persisted as
    // AppSettings "CpuShowSystem" ("True"/"False"). Smoothed reading is
    // updated via 0.8 * prev + 0.2 * new (matches Thetis console.cs:26224).
    bool   m_cpuShowSystem{true};
    double m_cpuSmoothedPct{0.0};
    // Process-CPU delta state (getrusage). Reset on toggle so the next
    // reading starts fresh rather than reporting accumulated cross-mode delta.
    qint64 m_cpuProcPrevWallUs{0};
    qint64 m_cpuProcPrevUserUs{0};
    qint64 m_cpuProcPrevSysUs{0};
    // System-CPU delta state (host_processor_info). Same reset rule as above.
    quint64 m_cpuSysPrevTotal{0};
    quint64 m_cpuSysPrevIdle{0};
    QVector<int> m_splitterSizesBeforeHide;  // saved splitter sizes for ☰ toggle

    // Status bar safety indicators (Phase 3M-0 Task 14 / sub-PR-8 restyle)
    // m_txInhibitLabel — red "TX INHIBIT" pill, hidden by default,
    //   shown when TxInhibitMonitor::inhibited() asserts (wired Task 17).
    // m_paStatusBadge  — PA OK (green ✓) / PA FAULT (red ✓) StatusBadge.
    // m_txStatusBadge  — TX indicator, solid red when MOX engaged.
    QLabel*      m_txInhibitLabel{nullptr};
    StatusBadge* m_paStatusBadge{nullptr};
    StatusBadge* m_txStatusBadge{nullptr};

    // Phase 3Q Sub-PR-6 (F.1): RxDashboard — always-visible RX1 glance surface.
    // Replaces the Phase 3Q-7 m_statusConnInfo / m_statusLiveDot strip (those
    // fields now live in the segment tooltip / NetworkDiagnosticsDialog).
    // Bound to RadioModel::slices().at(0) in buildStatusBar(); rebinds are not
    // needed today (single-slice, RX2 is Phase 3F).
    RxDashboard* m_rxDashboard{nullptr};

    // VFO flag widget (Phase 3E)
    class VfoWidget* m_vfoWidget{nullptr};

    // Applets (Phase 3-UI)
    class RxApplet* m_rxApplet{nullptr};
    // Phase 3M-3a-ii Batch 6: cached so SetupDialog instances can route
    // CfcSetupPage's [Configure CFC bands…] button to the same modeless
    // TxCfcDialog instance owned by TxApplet (m_cfcDialog).
    class TxApplet* m_txApplet{nullptr};
    class PhoneCwApplet* m_phoneCwApplet{nullptr};
    class EqApplet* m_eqApplet{nullptr};
    class VaxApplet* m_vaxApplet{nullptr};

    // Applets — Tasks 7-10 (NYI shells, hidden until Task 15 Container wiring)
    class DigitalApplet*    m_digitalApplet{nullptr};
    class PureSignalApplet* m_pureSignalApplet{nullptr};
    class DiversityApplet*  m_diversityApplet{nullptr};
    class CwxApplet*        m_cwxApplet{nullptr};
    class DvkApplet*        m_dvkApplet{nullptr};
    class CatApplet*        m_catApplet{nullptr};
    class TunerApplet*      m_tunerApplet{nullptr};

    // Spectrum overlay panel
    class SpectrumOverlayPanel* m_overlayPanel{nullptr};

    // Applet panel — scrollable content widget inside Container #0
    class AppletPanelWidget* m_appletPanel{nullptr};

    // Phase 3O Sub-Phase 10 Task 10c: host strip for the menu bar +
    // MasterOutputWidget. Owned by QMainWindow via setMenuWidget().
    TitleBar* m_titleBar{nullptr};
};

} // namespace NereusSDR
