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
#include <QPointer>
#include <QTimer>
#include <QHash>
#include <QMap>
#include <QVector>

class QProgressDialog;
class QThread;
class QSplitter;
class QMenu;

namespace NereusSDR {

/// Defined in gui/widgets/StatusToast.h. Forward-declared with its fixed
/// underlying type so this header keeps to Qt includes only.
enum class ToastSeverity : int;

class RadioModel;
class ConnectionPanel;
class SupportDialog;
class WdspEngine;
class FFTEngine;
class SpectrumWidget;
class SliceModel;
class VfoWidget;
// Phase 3F Sub-Epic D: forward declarations for the multi-pan layout
// manager. Member m_panStack is introduced (nullptr) in Task 10/11 so the
// +PAN dropdown menu and per-chain status indicators can guard against
// not-yet-wired state; Task 12 instantiates m_panStack and migrates
// m_spectrumWidget references.
class PanadapterStack;
class ClarityController;
class ContainerManager;
class MeterWidget;
class MeterPoller;
class TitleBar;
class VaxFirstRunDialog;
class PsForm;
// Phase 3J-2 H1: Tools menu modeless singletons.
class SpotHubDialog;
class FreeDVReporterDialog;

class RxDashboard;
class StationBlock;
class ChromeBarController;
class SystemTile;
class MetricLabel;
class StatusBadge;
class AdcOverloadBadge;
class OverflowChip;
class PsaIndicatorWidget;
class AppletVisibilityController;
class AppletWidget;

// Phase 23: TCI server + applets forward declarations (all inside NereusSDR
// namespace — TciServer only exists when HAVE_WEBSOCKETS is defined but we
// forward-declare unconditionally; m_tciServer is nullptr in non-WebSocket builds).
class TciServer;
class TciApplet;
class ClientChainApplet;
// Phase 3J-1 closeout Item 2 (2026-05-12): TciLogWindow viewer.
class TciLogWindow;

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

    /// Phase 3F Sub-Epic D Task 12: backward-compat accessor that returns
    /// the SpectrumWidget owned by the currently-active pan (via
    /// m_panStack->panadapter(activePanId())->spectrumWidget()).
    /// Returns nullptr during early init before m_panStack is constructed,
    /// or if the active pan has no widget. Long-term migration target:
    /// callers should thread through per-pan PanadapterApplet rather than
    /// reaching for the active pan.
    SpectrumWidget* activeSpectrumWidget() const;

    /// The SpectrumWidget that hosts slice `s`'s panadapter, resolved from
    /// the slice's panKey(). Falls back to the active pan's widget when the
    /// slice has no pan key or the pan no longer exists. Phase 3F multi-pan
    /// flag routing hub; mirrors AetherSDR MainWindow::spectrumForSlice
    /// (MainWindow.cpp:14856 [@6a142807]).
    SpectrumWidget* spectrumForSlice(SliceModel* s) const;

    // Narrow composition seams used by deletion-gap regressions. Runtime
    // call sites use these same helpers so stable-ID lookup cannot diverge
    // between the test and the UI signal path.
    static SliceModel* sliceForAddedIdForTest(RadioModel* model, int sliceId);
    static void applyAntennaChangeForTest(RadioModel* model, int sliceId,
                                          const QString& antennaName);
    static void wireRadeFlagForTest(RadioModel* model, VfoWidget* flag,
                                    int sliceId);
    static void configureSpectrumForPanForTest(SpectrumWidget* spectrum,
                                                const QString& panId);
    static void wireWidebandExtensionForTest(SpectrumWidget* spectrum,
                                             RadioModel* model,
                                             PanadapterStack* stack,
                                             const QString& panId);
    static void fanWidebandBinsForTest(PanadapterStack* stack, int adcIndex,
                                       const QVector<float>& bins);

public slots:
    // ── Phase 3M-0 Task 14 helper slots ──────────────────────────────────
    // Update PA status badge state. Wired by Task 17 to
    // RadioModel::paTrippedChanged.
    void setPaTripped(bool tripped);

    // Update TX Inhibit label visibility. Wired by Task 17 to
    // TxInhibitMonitor::txInhibitedChanged.
    void setTxInhibited(bool inhibited);

    // Task 3.6: live-apply CPU meter update rate from GeneralOptionsPage spinbox.
    // hz is clamped to [1, 30]. Restarts m_cpuTimer with the new interval.
    void setCpuTimerIntervalHz(int hz);

    // Task 3.6: live-apply ANAN-8000DLE volts/amps visibility preference.
    // Called when the "Show volts/amps in title bar" checkbox changes.
    // Only has visible effect when the connected radio is an ANAN-8000D
    // (the SystemTile PA row is already auto-hidden for non-MKII boards).
    void setVoltsAmpsVisible(bool visible);

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    /// Phase 3F: repaint every pan's status overlay from the slice that pan
    /// is showing (its own activeSliceIndex), with the ADC chain resolved
    /// through RadioModel::sliceChainIndex so the CH tag agrees with the WIDE
    /// pill beside it.
    ///
    /// A slot rather than a plain method because the per-slice triggers are
    /// connected through QMetaMethod, driven by
    /// PanadapterApplet::statusOverlaySliceProperties, so that adding an
    /// overlay field never means remembering to add a connect here.
    ///
    /// Every pan is refreshed on every pass, matching refreshPanWideBadges.
    /// Cheap: single-digit pans, and each overlay setter drops a no-op write
    /// before it repaints, so a VFO detent repaints exactly the one pan whose
    /// frequency actually moved.
    void refreshPanStatusOverlays();

    /// Phase 3F: connect the status-overlay badge clicks on EVERY pan.
    ///
    /// Armed from PanadapterStack::countChanged, the same hook
    /// wirePanStatusOverlayTriggers uses, so pans created after startup by a
    /// layout switch or an Add Panadapter action are wired too -- which is
    /// every pan except pan-0. Before this, MainWindow connected the three
    /// signals for the single applet it could name at construction, so on a
    /// multi-pan layout the badges painted correctly everywhere (00ab9522,
    /// 0896b4f3) and responded nowhere else.
    ///
    /// Idempotent, because re-arming on every countChanged would otherwise
    /// stack duplicate connections and open one dialog per layout switch the
    /// operator had ever made. The handlers below are SLOTS, not lambdas,
    /// specifically so Qt::UniqueConnection actually dedups: Qt6 silently
    /// no-ops UniqueConnection when the target is a lambda (see the notes at
    /// the SpotModel and applet-visibility connect sites).
    void wirePanBadgeHandlers();

    /// Give every panadapter its own control strip (+RX / BAND / ANT /
    /// Display). Idempotent and re-armed from PanadapterStack::countChanged,
    /// so pans created later get one by construction. Each strip carries its
    /// own panId and its controls act on that pan.
    void ensureOverlayPanels();

    /// Push the live slice-id list to MeterPoller so every flag's S-meter bar
    /// is fed. Must be called on slice add/remove, not just on pan-count
    /// change: a slice added to an existing pan moves no pan count.
    void refreshMeterPollerSlices();

    /// Wire one panadapter's spot, connection and MaxBin hooks. Every one of
    /// these used to be connected once to pan-0's widget, leaving other pans
    /// inert. The four controls that TARGET A SLICE live in
    /// wireSpectrumSliceControls below, because pan-0 needs those and does not
    /// come through here.
    void wireSpectrumForPan(class SpectrumWidget* sw, const QString& panId);

    /// Wire the four spectrum controls that act on a slice: click-to-tune,
    /// filter-edge drag, pan drag, CTUN toggle.
    ///
    /// Split out of wireSpectrumForPan for the bench defect of 2026-07-28,
    /// where click-to-tune retuned Slice A however many times the operator
    /// selected another flag. ensureOverlayPanels deliberately skips
    /// wireSpectrumForPan for pan-0 (its spot / connection / MaxBin hooks are
    /// wired elsewhere and would double), so pan-0 was left running an older
    /// copy of these four in wireSliceToSpectrum whose lambdas captured
    /// RadioModel::activeSlice() by value at connect time. That pointer is
    /// Slice A and never moved, so on the one pan almost every operator uses,
    /// none of the four ever consulted the pan's active slice at all.
    ///
    /// Called for EVERY pan, pan-0 included, and the sole home for these four
    /// signals: verify-no-captured-slice-spectrum-wiring.py fails the build if
    /// any of them is connected to a sender other than this function's `sw`.
    /// Each handler resolves its target through sliceForPan(panId) at signal
    /// time rather than capturing it, which is what makes it follow the
    /// operator's selection.
    void wireSpectrumSliceControls(class SpectrumWidget* sw,
                                   const QString& panId);

    /// Push the live connection state into every pan's spectrum widget.
    /// SpectrumWidget::mousePressEvent returns early when it believes the
    /// radio is disconnected, so a widget that never receives this is inert to
    /// every mouse press.
    void pushConnectionStateToPans();

    /// The slice a pan hosts -- its own active slice if it has one, else the
    /// first slice associated with it. nullptr when the pan has no slices.
    SliceModel* sliceForPan(const QString& panId) const;

    /// Phase 3F: WIDE pill / CH tag both open FilterPolicyDialog on the chain
    /// feeding the CLICKED pan, and the TX pill asks the arbiter to hand the
    /// transmitter to that pan's active slice.
    ///
    /// Each takes the pan id rather than assuming the active pan: an operator
    /// looking at a WIDE pill on pan 1 is asking about pan 1's chain whether
    /// or not pan 1 is the pan with focus.
    void onPanWideBadgeClicked(const QString& panId);
    void onPanChainTagClicked(const QString& panId, int chainIdx);
    void onPanTxBadgeClicked(const QString& panId);

    void onConnectionStateChanged();
    void showConnectionPanel();
    void showSupportDialog();
    void showAudioDiagnoseDialog();
    void showFeatureRequestDialog();
    void showFeatureRequestDialogImpl();
    // Phase 3M-4 Task 8: open the modeless PureSignal dialog (Tools menu).
    // Lazy-constructs on first invocation; subsequent calls show + raise the
    // existing instance so geometry persists across opens.
    void openPureSignalDialog();
    // Phase 3J-2 H1: open the modeless Spot Hub / FreeDV Reporter dialogs
    // (Tools menu). Same lazy-construction pattern as openPureSignalDialog;
    // both dialogs are single-instance for the lifetime of MainWindow.
    void openSpotHub();
    void openFreeDVReporter();
    /// Phase 3F Sub-Epic D Task 10: +PAN dropdown handler.
    /// Builds a context menu with three sections (add slice / pick layout
    /// template / float active pan), driven by RadioModel::slices() /
    /// maxSlices() and (when wired by Task 12) m_panStack.
    void showPanMenu();

    /// Apply a pan layout template and reconcile the slices against it.
    ///
    /// Codex review round 3, PR #293. There were three places that applied a
    /// layout: session restore, the View menu, and the +PAN dropdown. Each
    /// had its own copy of the pan-count table, the id list and an add-only
    /// slice loop. Round 2's fix for slices orphaned by a shrinking layout
    /// went into the View-menu copy only, so the defect stayed live through
    /// +PAN, which is the one operators actually use.
    ///
    /// One function now owns the whole sequence, so a later fix cannot land
    /// in one path and miss two. The part that carries real logic,
    /// RadioModel::rehomeSlicesToPans, is tested there; MainWindow is not
    /// constructible in the harness, so what is left here is plumbing.
    void applyPanLayout(const QString& layoutId);

    /// Give every pan that has no slice one, so it gets a VFO flag, an RX
    /// applet entry and a stream. Called from applyPanLayout and again at
    /// connect, because the startup layout restore cannot do it: no radio,
    /// no stream pool. See the definition for the bench defect where a
    /// persisted multi-pan layout came back with a permanently dead pane.
    void populateEmptyPans();

    /// The pan-id list a layout template implies. Sole owner of the
    /// template-to-pan-count table, which previously had three copies.
    static QStringList panIdsForLayout(const QString& layoutId);
    // Phase 3M-4 bench-fix: gate m_psaIndicator visibility on
    // caps.hasPureSignal && PureSignal::isAutoCalEnabled.  Called from
    // PureSignal::autoCalEnabledChanged + RadioModel::pureSignalCoordinator-
    // Ready + onConnectionStateChanged.
    void updatePsaIndicatorVisibility();
    // Phase 3Q Sub-PR-4 D.2: right-click context menu on the TitleBar
    // ConnectionSegment. Items: Disconnect / Connect-to-other / Diagnostics /
    // Copy IP / Copy MAC. "Reconnect" omitted — no RadioModel::reconnect() API.
    void showSegmentContextMenu(const QPoint& globalPos);
    // Phase 3Q Sub-PR-7 G.1: right-click context menu on the StationBlock.
    // Items: Disconnect / Edit radio… / Forget radio.
    void showStationContextMenu(const QPoint& globalPos);
    // Phase 23: update m_tciIndicator bottom label + tooltip for the 4 states
    // (Off / On / On·N / On·N ▸TX).  Connected to TciServer signals.
    void updateTciIndicator();
    // Phase 23: open Setup dialog at "TCI Server" page.  Wired to
    // tciAction triggered + m_tciIndicator click + TciApplet::setupRequested.
    void openTciSetupPage();

    // Phase 3P-II Phase 4 Task 90: generic navigation entry point for applet
    // right-click menus. Maps a pageKey string to a SetupDialog tree label:
    //   "pgxlAdvanced"  -> "PGXL Advanced"
    //   "tgxlAdvanced"  -> "TGXL Advanced"
    //   "pgxlInterlock" -> "PGXL Interlock"
    //   "peripherals"   -> "Peripherals"
    // Unknown keys are logged and ignored (current page unchanged).
    void openSetup(const QString& pageKey);

    // Phase 3P-II Phase 4 Task 97: soft-alert toast when peak forward power
    // exceeds the PGXL cap.  Connected to RadioModel::ampMetersChanged.
    // De-bounced: only one toast per exceedance event (re-arms when fwd drops
    // back below cap).  Uses QStatusBar::showMessage (5-second duration).
    // Guards: PGXL_PowerCapEnabled == "True" and fwd > PGXL_PowerCapW.
    void onAmpMetersForPowerCap(float fwd, float swr);

    // Phase 3P-II review fix C2: show TX interlock warning/denial on the
    // status bar so bench rows 28/29/31 are visible to the operator.
    // Connected to TxInterlockPolicy::warned / denied in buildUI().
    void onTxInterlockWarning(const QString& reason);
    void onTxInterlockDenial(const QString& reason);

    /// Phase 3F Sub-Epic I Task 8: fan one stream's FFT frame out to every
    /// pan subscribed to it. Connected to every pooled FFTEngine's
    /// fftReadyLinear; the streamIndex argument is the engine's receiver id.
    void dispatchFftFrameToPans(int streamIndex,
                                const QVector<float>& binsLinear,
                                double windowEnb,
                                double dbmOffset);

private:
    void buildUI();
    void buildMenuBar();
    void buildStatusBar();
    void applyDarkTheme();
    void tryAutoReconnect();
    void wireSliceToSpectrum();

    /// Stream 0's engine. Back-compat accessor for call sites that still
    /// address "the" FFT engine (display settings, Max Bin, auto-zoom).
    FFTEngine* primaryFftEngine() const { return m_fftEngines.value(0, nullptr); }

    /// Build and register one FFTEngine for `streamIndex`, configured as
    /// the old single-engine path was, moved onto the shared FFT thread.
    /// Returns the existing engine if one is already registered.
    ///
    /// Called on demand rather than once per pool slot: engines subscribe
    /// to the shared RadioModel::rawIqDataForStream and filter by index, so
    /// an engine for a stream that is never claimed would still take (and
    /// discard) a queued event for every packet of every OTHER stream.
    /// Building one only when the allocator actually claims the DDC keeps
    /// that cost at zero for the single-stream case. Sizing off
    /// streamPoolSize() at construction would not work anyway: the pool is
    /// unsized until RadioModel::configureStreamPool runs at connect.
    FFTEngine* createFftEngineForStream(int streamIndex);

    /// Push the stream's cached DDC centre + sample rate onto one pan's
    /// SpectrumWidget so visibleBinRange maps its bins against the right
    /// window. No-op for a stream we have never seen a centre for.
    void applyStreamWindowToPan(const QString& panId, int streamIndex);

    /// Re-derive the entire pan-to-stream topology from the current slice
    /// set. Cheap (one pass) and called on slice add / remove / migration
    /// / pan change. Chosen over incremental edits because a pan can host
    /// several slices, so no single change maps onto one subscription.
    void rebuildFftRouting();

    /// Phase 3F: light the WIDE pill on every pan fed by a bypassed RX
    /// preselector chain, and clear it on the rest. One
    /// RadioModel::panBypassState query per pan; the decision (and the
    /// operator-facing reason) lives there, not here.
    void refreshPanWideBadges();

    /// Phase 3F: arm the status-overlay triggers that are not per-slice --
    /// each pan's own activeSliceChanged. Idempotent (Qt::UniqueConnection),
    /// so it can be re-run whenever the pan set changes; a layout switch
    /// destroys and rebuilds applets, and a pan created after startup would
    /// otherwise never be wired.
    void wirePanStatusOverlayTriggers();

    /// Phase 3F: the ADC chain feeding `panId`, or -1 when it resolves to
    /// none.
    ///
    /// Deliberately the SAME resolution refreshPanStatusOverlays paints with
    /// -- the pan's own activeSliceIndex through
    /// RadioModel::sliceChainIndex -- so the dialog a badge opens is on the
    /// chain the CH tag beside it is showing. The shipped pan-0 handler used
    /// slices.first()->chainIndex() instead, which was wrong twice over: the
    /// first slice rather than the clicked pan's, and through a SliceModel
    /// property with no production writer. Both errors return 0, so every
    /// click on every pan opened chain 0.
    int panChainIndex(const QString& panId) const;

    /// Phase 3F: connect one slice's overlay triggers. Drives the connects
    /// off PanadapterApplet::statusOverlaySliceProperties through the
    /// metaobject rather than naming signals here, so the trigger set has a
    /// single definition and cannot silently fall behind the fields
    /// updateStatusOverlay paints.
    void wireSliceStatusOverlayTriggers(SliceModel* slice);

    /// Phase 3F: create the VfoWidget for a secondary slice (B+) on the
    /// given SpectrumWidget, push initial state, wire all intent + bidi
    /// signals, and register it in m_vfoWidgetsBySlice. Returns the new
    /// flag (or nullptr). Used both at sliceAdded and on panKeyChanged
    /// migration so the wiring lives in one place. Slice A keeps its own
    /// dedicated path in wireSliceToSpectrum(). Mirrors AetherSDR's
    /// addVfoWidget()+wireVfoWidget() pair (MainWindow.cpp:11583 +
    /// 13968 [@6a142807]).
    class VfoWidget* createSliceFlag(SliceModel* slice, SpectrumWidget* sw);

    /// Phase 3F Sub-Epic D Task 16: clean disconnect-before-removal for pans
    /// (AetherSDR issue #242 pattern - avoids lambda crashes during teardown).
    /// Caller should immediately follow with m_panStack->removePanadapter(panId).
    void disconnectPanadapter(const QString& panId);

    // Issue #206 — persist the main window's position, size, and
    // maximized/fullscreen state across launches. Stored in AppSettings
    // under MainWindowGeometry / MainWindowState (base64 of Qt's native
    // QByteArray blobs). restoreMainWindowGeometry() returns true if a
    // valid saved geometry was applied, false otherwise (first launch
    // or corrupted blob); buildUI() uses the return to decide whether
    // to keep the 1280×800 default. Multi-screen safety clamp: if the
    // restored geometry sits entirely outside every connected screen
    // (monitor disconnected since last save), fall back to centering
    // on the primary screen at the default size.
    void saveMainWindowGeometry();
    bool restoreMainWindowGeometry();

    // Re-apply the true hardware/board-capability gate on top of
    // m_chromeBar's fold decision, for the items that carry BOTH a fold
    // rung (design §6) AND an independent presence gate the controller
    // cannot express.
    //
    // ChromeBarController::addItem's contract says it is the sole writer
    // of visibility for every item it registers, and relayout() re-asserts
    // setVisible() for every registered item on any rung change -- not
    // just the item whose own state changed. m_tgxlChip and
    // m_chain1IndicatorWidget are registered (they are explicitly on the
    // §6 ladder, rungs 2 and 4) but ALSO already have a pre-existing,
    // independent visibility owner: TunerModel::presenceChanged and the
    // rxFilterChainCount capability gate respectively. Without this second
    // pass, any resize that changes the fold rung force-shows a "TGXL" or
    // "CH 1" tile that has no hardware behind it, and nothing else
    // corrects it afterwards (TunerModel never emits presenceChanged(false),
    // and the capability gate only re-fires on the next currentRadioChanged).
    // Called immediately after every m_chromeBar->relayout() call.
    void reapplyHardwarePresenceGates();

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
    // Right-click menu on m_systemTile — System / App radio choice.
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

    // Phase 3M-4 Task 8: PsForm modeless dialog (Tools > PureSignal...).
    // Lazy-constructed on first openPureSignalDialog() call; lives for the
    // lifetime of MainWindow.  Hidden on close, never destroyed.
    PsForm* m_psForm{nullptr};
    QAction* m_actPureSignal{nullptr};

    // Phase 3J-2 H1: Tools > Spot Hub... and Tools > FreeDV Reporter...
    // modeless singleton dialogs. Lazy-constructed on first
    // openSpotHub() / openFreeDVReporter() call; lives for the lifetime
    // of MainWindow. QPointer guards against the QDialog being deleted
    // out from under MainWindow (Qt::WA_DeleteOnClose is left at the
    // default false in the dialogs themselves so close-then-reopen
    // preserves geometry / table state). Both members are accessed by
    // the H1 test seam below.
    QPointer<SpotHubDialog>        m_spotHubDialog;
    QPointer<FreeDVReporterDialog> m_freeDVReporterDialog;

    // Status bar widgets (double-height AetherSDR design, 46px)
    //
    // Design §4.1: the left-section model+firmware pair (formerly
    // m_radioModelLabel / m_radioFwLabel, plus the m_connStatusLabel alias
    // to the model label) is retired. Both had no click affordance and sat
    // in the banner's unprotected left section, so their width changes were
    // what shoved neighbours. Radio identity now renders once, on
    // StationBlock's second row via setHardwareLine() (Task A4), driven
    // from onConnectionStateChanged().
    StationBlock* m_stationBlock{nullptr};    // Sub-PR-7 G.1: radio-name anchor
    QLabel* m_tnfLabel{nullptr};

    // Wisdom generation dialog (shown on first run)
    QProgressDialog* m_wisdomDialog{nullptr};

    // Spectrum display
    //
    // Phase 3F Sub-Epic D Task 12: the single m_spectrumWidget has been
    // removed and replaced by m_panStack (PanadapterStack), which owns
    // 1..N PanadapterApplet instances, each containing its own
    // SpectrumWidget. Existing call sites that still need a single
    // SpectrumWidget* go through activeSpectrumWidget() below, which
    // resolves to m_panStack->panadapter(activePanId())->spectrumWidget().
    // The accessor returns nullptr during early init before m_panStack
    // is constructed, so callers must null-guard.
    PanadapterStack*    m_panStack{nullptr};

    // Phase 3F Sub-Epic I Task 8: one FFTEngine per DDC stream, keyed by
    // stream index. Before this there was a single FFTEngine(0) wired at
    // construction to activeSpectrumWidget(), which resolves to pan 0
    // permanently, so no secondary pan ever received a frame.
    //
    // One engine per STREAM, not per slice: the panadapter belongs to the
    // DDC (ChannelMaster `_rcvr.run_pan`, cmaster.h:79 [v2.10.3.15]), so
    // slices sharing a DDC share its spectrum and appear as separate flags
    // on it.
    //
    // All engines share m_fftThread. If a 5-stream 1536 kHz bench shows
    // the thread saturating, splitting to one thread per engine is a
    // follow-up needing maintainer sign-off (thread architecture).
    QMap<int, FFTEngine*> m_fftEngines;

    /// One NoiseFloorTracker per stream, fed by that stream's FFT engine.
    /// Auto AGC-T needs the noise floor of the band a slice is actually on;
    /// a single tracker fed from stream 0 would mis-set every other slice.
    QMap<int, class NoiseFloorTracker*> m_streamNoiseFloors;
    QThread*            m_fftThread{nullptr};

    /// Last centre + sample rate RadioModel published for each stream, kept
    /// so a pan that subscribes AFTER the stream was centred still learns
    /// where its bins sit. RadioModel::bindSliceToStream emits
    /// streamCentreChanged BEFORE SliceModel::streamIndex is updated and
    /// before sliceAdded (plan discovery item 7), so at emit time the router
    /// does not yet know which pan shows the stream and the direct push in
    /// the streamCentreChanged handler reaches nobody. rebuildFftRouting
    /// replays the cached value onto each pan as it (re)subscribes.
    /// Without it SpectrumWidget::visibleBinRange maps a second pan's bins
    /// against its ctor-default 14.225 MHz / 768 kHz window.
    struct StreamWindow {
        double centreHz{0.0};
        int    sampleRateHz{0};
    };
    QHash<int, StreamWindow> m_streamWindows;
    ClarityController*  m_clarityController{nullptr};
    class StepAttenuatorController* m_stepAttController{nullptr};
    /// Phase 3F Sub-Epic D Task 11: CH 1 stacked-indicator widget in the
    /// bottom status bar. Shown only on 2-ADC SKUs (gated by
    /// BoardCapabilities::adcCount in the currentRadioChanged handler).
    /// Also registered with m_chromeBar at rung 4 (design §6) so it folds
    /// under width pressure; see reapplyHardwarePresenceGates() for why
    /// that registration needs a second, narrower gate on top.
    QWidget*            m_chain1IndicatorWidget{nullptr};
    /// CH 0's stacked-indicator widget, captured the same way as CH 1 so
    /// it can be registered with m_chromeBar (chain0 shares rung 4).
    /// Always shown; single-ADC and multi-ADC SKUs alike have a chain 0.
    QWidget*            m_chain0IndicatorWidget{nullptr};

    // Right-side strip wrapper widget — the inner QWidget hosting the
    // QHBoxLayout that buildStatusBar() populates. Stored as a member so
    // resizeEvent can read its available width for m_chromeBar->relayout().
    QWidget* m_chromeBarWidget{nullptr};

    // Single layout authority for the banner (design §5). Replaces both
    // the old right-strip drop-priority ladder (30 px deadband
    // hysteresis) and RxDashboard's internal 3-stage ladder. One rung
    // table, one relayout() call per resize, no re-measure mid-decision.
    // Item-to-rung wiring lives in registerChromeBarItems() (ChromeBarItems.h)
    // rather than inline here, so it is testable without constructing
    // MainWindow — see tests/tst_chrome_bar_items.cpp.
    ChromeBarController* m_chromeBar{nullptr};
    // Merged PA telemetry + CPU tile (design §4.3). Replaces the old
    // m_paStackWidget / m_paVoltLabel / m_paTempLabel / m_cpuMetric quartet.
    SystemTile* m_systemTile{nullptr};
    QLabel*     m_systemTileSep{nullptr};
    // Rung-10 group: band-stack dots + TNF/CWX/DVK/FDX, wrapped in one
    // widget so the ladder folds them together instead of dribbling them
    // out one label at a time (design §6, "last resort").
    QWidget*    m_placeholderGroup{nullptr};
    QLabel*     m_placeholderSep{nullptr};

    // Right-side strip items — captured so they can be registered with
    // m_chromeBar. Each non-separator widget has a paired separator
    // pointer so the pair hides + shows together (no dangling "··" runs).
    QWidget* m_catIndicator{nullptr};
    QLabel*  m_catSep{nullptr};
    QWidget* m_tciIndicator{nullptr};
    QLabel*  m_tciSep{nullptr};

    // OverflowChip — "…" pill that surfaces drop-list contents via its
    // hover tooltip. Hidden when the drop list is empty. Now driven by
    // m_chromeBar's foldStateChanged signal rather than a direct call
    // from the old right-strip drop-priority pass.
    OverflowChip* m_overflowChip{nullptr};

    // (Earlier revisions had a "voltage stack" wrapper holding PSU above
    //  PA. The PSU widget was source-first audited against Thetis 2026-04-30
    //  and removed — Thetis never displays AIN6/supply_volts. The PA volt
    //  label below is the sole supply indicator; it lives directly in the
    //  hbox now with no wrapper.)
    // ADC overload alarm: "ADCx / OVERLOAD" badge living in its own
    // reserved slot inside m_safetyGroup (design §4.5), between the PA
    // and TX slots. Dimmed when no ADC is in overload; setVariant()
    // flips between Warn (yellow) / Tx (red) per Thetis severity rules
    // (ucInfoBar.cs:928 [@501e3f5]).
    AdcOverloadBadge* m_adcOvlBadge{nullptr};
    // 2-second auto-hide timer for the ADC-overload alarm. Mirrors
    // Thetis ucInfoBar._warningTimer: restarts on each overload event,
    // dims the badge when elapsed — independent of the level-decay
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

    // Mode menu actions (14 modes: 12 Thetis + NereusSDR-native
    // RADE-U / RADE-L from Phase 3R L3; mutual exclusion via
    // QActionGroup).
    QAction*      m_modeActions[14]  = {};
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

    // Status bar members (Task 13 / sub-PR-8 restyle; merged into
    // SystemTile per design §4.3 in the bottom-banner cleanup).
    //
    // PA telemetry + CPU now share one two-row tile (m_systemTile) instead
    // of a separate PA stack (PA-V over PA-T) plus a standalone CPU
    // MetricLabel. Row one is PA voltage (MKII-class boards — Saturn / G2 /
    // 8000D / 7000DLE / OrionMkII / Anvelina Pro 3; Thetis-faithful via
    // convertToVolts) and/or PA temperature (HL2 today; future
    // PureSignal-feedback boards may surface a real temp source via
    // Phase 3M-4) — both share row one when a board publishes both rather
    // than evicting CPU. Row two is always CPU. The PA row is also
    // click-to-toggle °C / °F via PaTempUnitNotifier when it carries a
    // temperature reading (SystemTile::paTempClicked). Source-of-truth
    // value lives in RadioStatus::paTemperatureCelsius (always °C);
    // display formatting happens at paint time via PaTempUnitNotifier::format.
    QTimer*      m_cpuTimer{nullptr};

    // CPU usage source — System (whole machine) or App (this process).
    // Thetis equivalent: m_bShowSystemCPUUsage (console.cs:20668), default
    // true. Right-click on m_systemTile pops a menu with the two choices,
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

    // Bench-fix 2026-05-19: debounce token for within-band PGXL frequency push.
    // frequencyChanged fires on every tune-wheel click; we coalesce into one
    // setBand call per 200 ms burst. The token (timestamp in ms) lets the
    // QTimer::singleShot callback drop stale invocations.
    qint64 m_pgxlBandPushTokenMs{0};

    // Status bar safety indicators (Phase 3M-0 Task 14 / sub-PR-8 restyle;
    // reserved safety slots added per design §4.5).
    // m_txInhibitLabel: "INH" pill, dimmed by default, full opacity when
    //   TxInhibitMonitor::inhibited() asserts (wired Task 17).
    // m_paStatusBadge:  PA OK (green check) / PA FAULT (red check) StatusBadge.
    // m_txStatusBadge:  TX indicator, solid red when MOX engaged.
    // All four safety badges (TX Inhibit, PA, ADC overload, TX) live in
    // m_safetyGroup's fixed-width slots so an alarm never shifts geometry.
    QLabel*      m_txInhibitLabel{nullptr};
    StatusBadge* m_paStatusBadge{nullptr};
    StatusBadge* m_txStatusBadge{nullptr};
    // Reserved safety slot group. Registered at rung 0 (never folds).
    QWidget* m_safetyGroup{nullptr};
    // Inactive slots dim rather than collapse, so geometry never moves
    // (design §4.5). Shared by buildStatusBar()'s construction-time state
    // and setTxInhibited() -- both toggle a safety-group badge's active
    // state and must agree on how "inactive" is represented.
    static void dimSafetyBadge(QWidget* w, bool active);

    // Phase 3Q Sub-PR-6 (F.1): RxDashboard — always-visible glance surface
    // for the ACTIVE slice's RX state. Replaces the Phase 3Q-7
    // m_statusConnInfo / m_statusLiveDot strip (those fields now live in
    // the segment tooltip / NetworkDiagnosticsDialog).
    // Task A5 (2026-08-02 bottom-banner cleanup): rebound on every
    // RadioModel::sliceAdded / activeSliceChanged so it follows whichever
    // slice is active, not a fixed slice(0) -- see the rebindDashboard
    // lambda in buildStatusBar().
    RxDashboard* m_rxDashboard{nullptr};

    // Phase 3M-4 Task 10: PSA bottom-banner indicator pair (FB + PS labels).
    // Inserted between m_rxDashboard and m_stationBlock per design doc §4 #5
    // (option B).  Visibility gated on caps.hasPureSignal in
    // onConnectionStateChanged().  Wired to PureSignal coordinator + MOX
    // controller from inside the widget (auto-wired via RadioModel).
    PsaIndicatorWidget* m_psaIndicator{nullptr};

    // VFO flag widget (Phase 3E)
    class VfoWidget* m_vfoWidget{nullptr};

    // Phase 3F hotfix 2026-05-27: per-slice VfoWidget tracking. Slice 0
    // (Slice A) maps to the existing m_vfoWidget for backward compat;
    // additional slices created via +PAN / Ctrl+R get their own VfoWidget
    // via SpectrumWidget::addVfoWidget(N) wired in the sliceAdded handler.
    // Without this hash, multi-slice was invisible: the slice landed in
    // the model and the codec recomputed the DDC assignment, but no flag
    // appeared for the new slice and operators had no way to interact
    // with it.
    QHash<int, class VfoWidget*> m_vfoWidgetsBySlice;

    // Applets (Phase 3-UI)
    class AmpApplet*   m_ampApplet{nullptr};
    bool               m_ampAppletWired{false};  // guards one-time AmpApplet signal connects
    class RxApplet* m_rxApplet{nullptr};
    // Phase 3M-3a-ii Batch 6: cached so SetupDialog instances can route
    // CfcSetupPage's [Configure CFC bands…] button to the same modeless
    // TxCfcDialog instance owned by TxApplet (m_cfcDialog).
    class TxApplet* m_txApplet{nullptr};
    class PhoneCwApplet* m_phoneCwApplet{nullptr};
    // Phase 3R L2 — RADE-mode applet, visible only when the active slice
    // is in DSPMode::RADE_U or DSPMode::RADE_L.  Sits alongside
    // PhoneCwApplet in the panel stack and is shown/hidden in the same
    // dspModeChanged lambda.
    class RadeApplet* m_radeApplet{nullptr};
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

    // Phase 3P-III Task 14: RF-Kit RF2K-S applet.
    class Rf2ksApplet*      m_rfKitApplet{nullptr};

    // Phase 23: TCI server + applets.
    // m_tciServer is nullptr in non-WebSocket builds (HAVE_WEBSOCKETS not defined).
    TciServer*         m_tciServer{nullptr};
    TciApplet*         m_tciApplet{nullptr};
    ClientChainApplet* m_clientChainApplet{nullptr};

    // Phase 3J-1 closeout Item 2 (2026-05-12): TCI message log viewer.
    // Lazy-constructed on the first "Show Log..." click from the Setup
    // dialog; persistent thereafter for the lifetime of MainWindow so the
    // window survives Setup close/reopen.  Connected to
    // TciServer::messageLogged via Qt::QueuedConnection so emit-side never
    // blocks the server.  Nullptr until first show.
    TciLogWindow* m_tciLogWindow{nullptr};
    void showTciLogWindow();

    // Bottom label of the TCI indicator tile — captured from makeIndicator()
    // so updateTciIndicator() can change color + text without a findChild scan.
    QLabel* m_tciIndicatorBotLabel{nullptr};

    // Live connection-count cache for updateTciIndicator().  Updated from
    // clientConnected / clientDisconnected signals.
    int  m_tciClientCount{0};
    bool m_tciServerRunning{false};
    bool m_tciHasTxClient{false};

    /// Live notification toasts, newest last. Bench report 2026-07-30:
    /// QStatusBar::showMessage hides every non-permanent widget for the
    /// life of the message, and the whole bottom bar is one such widget
    /// (see buildStatusBar), so a TUNE with PureSignal active blanked the
    /// CH pill, PS indicator, radio name, CAT/TCI state, PA/TX badges and
    /// the clock for six seconds. Notices moved off the bar to here.
    ///
    /// QPointer because each toast deletes itself on close, by timer or
    /// by click, without telling us first.
    QList<QPointer<class StatusToast>> m_toasts;



    /// Show a transient notice without disturbing the bottom bar.
    /// Repeats of a message already on screen restart that toast's
    /// countdown instead of stacking a duplicate beneath it.
    ///
    /// Returns the toast so a caller whose condition can end early can
    /// dismiss it rather than leaving a stale notice up for its full
    /// timeout. Hold it by QPointer: it deletes itself on close.
    StatusToast* showToast(const QString& message,
                           ToastSeverity severity,
                           int timeoutMs);

    /// The suspended-streams notice, kept so it can be taken down the
    /// moment the streams come back instead of aging out.
    QPointer<class StatusToast> m_suspendToast;

    /// Re-stack live toasts bottom-right, newest nearest the bar.
    /// Called on show, on close, and on move/resize.
    void restackToasts();

    // Spectrum overlay panel
    /// Pan-0's strip. Kept as a stable target for the display-settings and
    /// clarity wiring, which is still global rather than per-pan.
    class SpectrumOverlayPanel* m_overlayPanel{nullptr};

    /// One control strip per pan, keyed by pan id. QPointer because the widget
    /// is parented to its pan's SpectrumWidget and dies with it when a layout
    /// switch retires the pan.
    QHash<QString, QPointer<class SpectrumOverlayPanel>> m_overlayPanels;

    // Applet panel — scrollable content widget inside Container #0
    class AppletPanelWidget* m_appletPanel{nullptr};

    // Applet visibility controller (NereusSDR-original) — backs the
    // Containers > Applets top menu and the panel banner ☰ menu.
    // Constructed in the layout-build path after the panel is wired.
    AppletVisibilityController* m_appletVis{nullptr};
    QHash<QString, AppletWidget*> m_appletsById;
    QHash<QString, QAction*> m_topMenuAppletActions;
    QMenu* m_bannerAppletsMenu{nullptr};
    QHash<QString, QAction*> m_bannerAppletActions;

    // Phase 3O Sub-Phase 10 Task 10c: host strip for the menu bar +
    // MasterOutputWidget. Owned by QMainWindow via setMenuWidget().
    TitleBar* m_titleBar{nullptr};

    // Phase 3P-II Task 21: TGXL status bar chip.
    // Shown when TunerModel::presenceChanged fires true; hidden otherwise.
    // Text is "TGXL" / "TGXL OPER" / "TGXL BYPS" / "TGXL SBY".
    QLabel* m_tgxlChip{nullptr};

    // Phase 3P-II Phase 4 Task 97: de-bounce flag for power cap soft-alert
    // toast.  Set true when the toast fires; reset false when fwd drops back
    // below the cap so a new exceedance event re-arms.
    bool m_powerCapToastShown{false};
};

} // namespace NereusSDR
