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
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02  J.J. Boyd / KG4VCF. TX letters share the guarded flag
//                Take and select action, with current access and target
//                lifetime checks. AI-assisted via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Signal-routing hub, double-height status-bar layout, and
//                 TitleBar feature-request dialog ported from AetherSDR
//                 (ten9876/AetherSDR, GPLv3) src/gui/MainWindow.{h,cpp} and
//                 src/gui/TitleBar.{h,cpp}. AetherSDR has no per-file
//                 headers; project-level citation per docs/attribution/
//                 HOW-TO-PORT.md rule 6.
//   2026-09-23 - J.J. Boyd (KG4VCF). R3 receiver audio fix wave (R-R3-42,
//                 R-R3-44): m_receiverStopNotices. AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-24 - J.J. Boyd (KG4VCF). R-R3-49 / R-R3-21:
//                 firstRunPromptsBarredForTestRun(). AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-25 - J.J. Boyd (KG4VCF). iPhone app plan, desktop remote
//                 transmit (R-IOS-13, R-R3-42): remoteTransmitReason() and
//                 the TCI transmit forwarder. AI-assisted implementation
//                 via Anthropic Claude Code.
//   2026-09-24 - J.J. Boyd (KG4VCF). R-R3-49 (parity Task 1):
//                 transmitSettingsPermitted() and transmitSettingsReason().
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25 - J.J. Boyd (KG4VCF). R-R3-49 (parity Task 7):
//                 pureSignalArmingPermitted() and pureSignalArmingReason().
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26 - J.J. Boyd (KG4VCF). Remote-window parity Task 18:
//                 refreshNoSliceHints(), wirePanDisplayFlyout(),
//                 refreshClarityBadges(), clarityStreamIndex(),
//                 panLayoutLimitFor(), applySpotModeToSlice(); the pan-0
//                 strip pointer is gone. AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-26 - J.J. Boyd (KG4VCF). Remote-window parity Task 29:
//                m_moxDisplay and its local and remote transmit display
//                sources replace m_txDisplayPanId and the saved receive
//                rate and DDC centre. AI-assisted via Anthropic Claude Code.
//   2026-09-27 - J.J. Boyd (KG4VCF). Remote-window parity Task 31: the
//                display duplex setting, its menu item and its apply.
//                AI-assisted via Anthropic Claude Code.
//   2026-09-27 - J.J. Boyd (KG4VCF). iPhone plan Task 22 / parity Task 20:
//                refreshFreedvReporterAvailability. AI-assisted via
//                Anthropic Claude Code.
//   2026-09-28 - J.J. Boyd (KG4VCF). Parity Task 25: the container filter
//                and band-stack right-clicks. AI-assisted via Anthropic
//                Claude Code.
//   2026-09-28 - J.J. Boyd (KG4VCF). Parity ruling C9: m_cpuRowCycler and
//                refreshCpuRow. AI-assisted via Anthropic Claude Code.
//   2026-09-29 - J.J. Boyd (KG4VCF). Remote parity on the air:
//                transmitSettingsPermitted follows a Core at
//                transmitSettingsVersion 13 on the air. AI-assisted via
//                Anthropic Claude Code.
//   2026-09-29 - HL2 port part 2: the TX badge tooltip and toast name the
//                TX inhibit's reason (the HL2 I/O board's fault code) and
//                the transmit buttons follow the inhibit. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - J.J. Boyd (KG4VCF). Slice control plan Task 14b: a
//                listened flag's "Your volume" (setFlagListenVolume,
//                listenVolumeFor, m_remoteListenVolumes). AI-assisted via
//                Anthropic Claude Code.
//   2026-09-29 - J.J. Boyd (KG4VCF). Slice control plan Task 15:
//                windowRxSlice, refreshRxAppletSlices, sliceShownInWindow.
//                AI-assisted via Anthropic Claude Code.
//   2026-09-29 - J.J. Boyd (KG4VCF). Slice control plan Task 15 fix round
//                1: sliceAccessServer, sliceAccessClient,
//                sliceChangeRefusal (the container buttons' refusal),
//                dropHostingSliceActionsForTest.
//                AI-assisted via Anthropic Claude Code.
//   2026-09-29 - J.J. Boyd (KG4VCF). Slice control plan Task 10: the
//                hosting desktop's slice requests run as the station device
//                (m_hostingSlices, hostingSlices(), selectSliceForWindow,
//                addSliceForWindow, closeSliceForWindow). AI-assisted via
//                Anthropic Claude Code.
//   2026-09-29 - J.J. Boyd (KG4VCF). Slice control plan Task 11:
//                populatePanSlices takes the hosting actions, so a hosting
//                window's empty pans get station-device slices. AI-assisted
//                via Anthropic Claude Code.
//   2026-09-29 - J.J. Boyd (KG4VCF). Slice control plan Task 11 fix:
//                transmitSliceChoiceReason, requestTransmitSlice and
//                refreshFlagTransmitGates, one path for the flag's TX button
//                and the TX applet's letters. AI-assisted via Anthropic
//                Claude Code.
//   2026-09-29 - J.J. Boyd (KG4VCF). Slice control plan Task 16 (rulings
//                U1, U2, U7): windowSharesSlices, windowControlsSlice,
//                windowListensTo, windowPanIds, windowPanFor,
//                rehostSliceView, revealSliceInWindow,
//                reconcileListenPlacements, stopListeningOffWindow,
//                m_listenPlacement, m_pendingRevealSlice. AI-assisted via
//                Anthropic Claude Code.
//   2026-09-30 - J.J. Boyd (KG4VCF). TX rulings (item 3):
//                refreshOverlayAttAccess. AI-assisted via Anthropic Claude
//                Code.
//   2026-09-30 - J.J. Boyd (KG4VCF). TX rulings review:
//                hostingSliceActionsForTest. AI-assisted via Anthropic
//                Claude Code.
//   2026-09-30 - J.J. Boyd (KG4VCF). Desktop listening: listening ends only
//                when the operator ends it; stopListeningOffWindow removed.
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30 - J.J. Boyd (KG4VCF). TX badge take (JJ's ruling): a flag's
//                TX badge takes the slice, then transmit, then makes the
//                slice the TX slice (applyTxBadgeOffer, startTxBadgeTake).
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30 - J.J. Boyd (KG4VCF). Station VOX: a hosting window's VOX is
//                disabled, naming the holder, while another device holds
//                transmit (desktopVoxHolderReason, applyDesktopVoxHolderGate).
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30: Fix wave GUI-I3: applyRemotePureSignalAppletGate. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - VFO flag crash lane: m_vfoWidget removed (Slice A's flag
//                is in m_vfoWidgetsBySlice like every other);
//                m_sliceASpectrumWired. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-10-01 - VFO flag crash lane fix round: createSliceFlag's comment
//                covers Slice A's flag. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
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

#include "gui/widgets/CpuRowCycler.h"
#include <functional>
#include <memory>
#include <map>
#include <utility>
#include <QMainWindow>
#include <QLabel>
#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QPointer>
#include <QTimer>
#include <QHash>
#include <QSet>
#include <QMap>
#include <QVector>

// R1 Task 7: m_topology below is a plain value member (FftTopology has no
// QObject parent to own it through, unlike the pointer members this header
// otherwise forward-declares), so the complete type is needed here rather
// than just in MainWindow.cpp.
#include "core/spectrum/FftTopology.h"
// Remote-daemon R2 Task 20: by value in the constructor overload and in
// m_station below, so it cannot be forward-declared.
#include "core/session/RemoteStationOptions.h"
#include "core/WdspTypes.h"
#include "gui/ReceiveLayoutNotices.h"
#include "gui/ReceiverStopNotices.h"
#include "gui/RemoteReceiverAudioNote.h"
#include "gui/DesktopStationController.h"

class QProgressDialog;
class QSplitter;
class QMenu;
class QDialog;

namespace NereusSDR {

/// Defined in gui/widgets/StatusToast.h. Forward-declared with its fixed
/// underlying type so this header keeps to Qt includes only.
enum class ToastSeverity : int;

class RadioModel;
class MeterItem;
class ContainerButtonDispatcher;
class ConnectionPanel;
class SupportDialog;
class WdspEngine;
class FFTEngine;
class FftEnginePool;
class TxAnalyzer;
class SpectrumWidget;
class SliceModel;
class SliceChooser;
class VfoWidget;
class PanadapterModel;
// Phase 3F Sub-Epic D: forward declarations for the multi-pan layout
// manager. Member m_panStack is introduced (nullptr) in Task 10/11 so the
// +PAN affordance (a dropdown at the time; a drawn icon opening
// PanLayoutDialog since Task B4) and per-chain status indicators can
// guard against not-yet-wired state; Task 12 instantiates m_panStack and
// migrates m_spectrumWidget references.
class PanadapterStack;
class ClarityController;
class ContainerManager;
class MeterWidget;
class MeterPoller;
class TitleBar;
class VaxFirstRunDialog;
class PsForm;
class DiversityDialog;
// Remote-daemon R2 Task 20 fix round 2: createSetupDialog() returns this,
// and it is a slot, so the type appears in a moc-parsed signature.
// Declared here rather than written inline as the elaborated
// `class SetupDialog*` that the older, non-slot wireSetupDialog() uses.
// Whether moc would accept the elaborated form in a return position was
// not tested; the forward declaration is the form that is known to work.
class SetupDialog;
struct CoreSettingsContext;
class RemoteMediaController;
// Phase 3J-2 H1: Tools menu modeless singletons.
class SpotHubDialog;
class FreeDVReporterDialog;

class RxDashboard;
class StationBlock;
class ChromeBarController;
class SystemTile;
class StatusBadge;
class MultiDeviceController;
class HostingSliceActions;
class NoticeCard;
struct SessionMessage;
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
    /// The preset a container's filter right-click edits, or -1 when the
    /// slice's filter is none of its mode's presets.
    static int containerFilterContextSlot(int index, int activePreset, int presetCount);
    /// What a container's band-stack right-click says while band stacking
    /// is not built.
    static QString containerBandStackReason();
    enum class ConnectionStartup { Automatic, Deferred };
    explicit MainWindow(QWidget* parent = nullptr);

    /// Remote-daemon R2 Task 20: the remote-station overload.
    ///
    /// An empty `station.url` is byte-identical to the constructor above
    /// (which delegates here with a default-constructed options struct), so
    /// local direct mode is untouched. A non-empty one makes this window's
    /// RadioModel Role::Remote: it never calls connectToRadio(), owns no
    /// RadioConnection, and drives a daemon over `wss` instead.
    ///
    /// The options are taken by value at construction because
    /// m_radioModel's role has to be decided in the initializer list --
    /// there is no later point at which a RadioModel can change role.
    ///
    /// PRECONDITION for remote use, and it is not optional: the caller must
    /// have already installed a SettingsProxy as AppSettings' remote
    /// backend, and that proxy must still report ready() == false. Several
    /// model constructors seed Station-classified keys if absent, and what
    /// keeps those ship defaults out of the STATION store is entirely that
    /// the proxy is not ready yet and drops the write. See
    /// SettingsProxy.h's "ready()==false is load-bearing beyond this class"
    /// section, which names this task as the one that had to confirm the
    /// ordering. src/main.cpp installs the proxy before constructing this
    /// window, and tst_remote_gui_gating pins that nothing in RadioModel
    /// construction flips ready().
    explicit MainWindow(const RemoteStationOptions& station,
                        QWidget* parent = nullptr,
                        ConnectionStartup startup = ConnectionStartup::Automatic);
    ~MainWindow() override;

    // R-R3-38: construct each immutable-role session before starting it.
    // Only the coordinator uses retirement; ordinary Close still quits.
    void startInitialConnection();
    void retireForSessionSwitch();
    void setConnectionPickerManaged(bool managed);
    /// Present Settings for a saved Core without changing the window's session.
    void openCoreSettings(const QString& targetId);
    /// Presentation facts for this window; no saved-entry or session ownership.
    CoreSettingsContext coreSettingsSnapshot() const;
    void setSavedCoreName(const QString& name);
    RadioModel* radioModel() const { return m_radioModel; }
    // Caller owns the controller and model; local windows only.
    void setDesktopStationController(DesktopStationController* controller);
    void refreshDesktopStationState();
    FftEnginePool* fftEnginePoolForTest() const { return m_fftEnginePool; }
    int miniProducerCountForTest() const { return int(m_miniProducers.size()); }
    // Slice control plan Task 15 fix round 1: drops the hosting slice
    // requests so a test reaches selectSliceForWindow's fallback path.
    void dropHostingSliceActionsForTest();
#ifdef NEREUS_BUILD_TESTS
    // TX rulings review: the hosting slice requests, for a test that
    // checks what a card's close leaves there.
    HostingSliceActions* hostingSliceActionsForTest() const { return m_hostingSlices.get(); }
#endif

    // R-R3-49 / R-R3-21: true in a test run (QStandardPaths test mode, set
    // before main() by tests/TestSandboxInit.cpp), false in the app. A test
    // run never auto-opens a first-run prompt that blocks for a click.
    static bool firstRunPromptsBarredForTestRun();

signals:
    void connectionsRequested();
    void hostedInitialConnectionRequested();
    void setupDialogCreated(NereusSDR::SetupDialog* dialog);

public:
    /// Ruling U1 and U2's reveal (revealSliceInWindow), for tests.
    void revealSliceInWindowForTest(int sliceId) { revealSliceInWindow(sliceId); }

    // ── Phase 3M-0 Task 14 test accessors ────────────────────────────────
    // TX Inhibit no longer has a label of its own. It paints onto the TX
    // badge (prohibition symbol) and raises a toast; see setTxInhibited().
    /// True while an external TX Inhibit is asserted.
    bool isTxInhibited() const noexcept { return m_txInhibited; }

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

    /// The pan-id list a layout template implies. Sole owner of the
    /// template-to-pan-count table, which previously had three copies.
    /// Public (moved from private slots: in Task B1) so the pan-count
    /// table has a direct unit test instead of only being exercised
    /// indirectly through applyPanLayout, which needs a constructed
    /// MainWindow the test harness cannot build.
    static QStringList panIdsForLayout(const QString& layoutId);
    /// Parity Task 18: how many pans Pan Layout and +PAN offer. The slice
    /// limit is RadioModel::maxSlices(), which is the Core's advertised
    /// limit in a remote window and the board's own locally, capped by the
    /// receivers the radio can give separate pans.
    static int panLayoutLimitFor(const RadioModel* model);
    /// Parity Task 18: the mode half of a left-click on spot `spotIndex`
    /// (the widget tunes first): `slice` takes the spot's mode, as in
    /// AetherSDR, unless Auto mode is off in the Spot Hub.
    static void applySpotModeToSlice(RadioModel* model, SliceModel* slice, int spotIndex);
    // Shared startup/operator boundary, exercised without booting MainWindow.
    // Slice control plan Task 11: while this window hosts, `hosting` makes
    // each new slice the station device's (HostingSliceActions::addOnPan),
    // never an unowned one.
    static void populatePanSlices(RadioModel* model, const QStringList& panIds,
                                  bool operatorRequested, bool snapshotReady,
                                  HostingSliceActions* hosting = nullptr);
    /// Slice control plan Task 2: a slice the station device may change as
    /// its own (not one it listens to, nor one it runs held for an absent
    /// device). desktopSliceAllowed is this while the window hosts.
    static bool stationControlsSlice(const RadioModel* model, int sliceId);
    /// Slice control plan Task 11: the slice a hosting window's TX applet
    /// follows: the transmit-bound slice when the station controls it,
    /// else the station's own active slice, else its first slice. Never a
    /// slice it only listens to.
    static SliceModel* stationTransmitSlice(RadioModel* model);

    // Narrow composition seams used by deletion-gap regressions. Runtime
    // call sites use these same helpers so stable-ID lookup cannot diverge
    // between the test and the UI signal path.
    static SliceModel* sliceForAddedIdForTest(RadioModel* model, int sliceId);
    // Follow-up item 3 (R-R3-21): the DSP > NR menu's choice for a slice.
    // Returns the reason to show, in user words, when the slice refused it
    // (empty when accepted). The menu is the one place a menu refusal is
    // shown; a VFO flag click shows its own.
    static QString applyNrMenuChoice(SliceModel* slice, NereusSDR::NrSlot slot);
    // R-R3-49, Sub-epic C-1 (tx-followup-4): the DSP > NR menu's entries
    // (label, slot) in order. BNR is not among them.
    static QList<std::pair<QString, NereusSDR::NrSlot>> nrMenuEntries();
    // R-R3-43 / R-R3-44: the VAX page's note about the Core's receiver
    // streams. receiverAudioNoteFor reads it from the audio status and
    // whether the Core sends receiver streams (None without media).
    // wireReceiverAudioNotePush pushes `source` to every SetupDialog under
    // `dialogRoot` on an audio status change and on a station link change,
    // which is how a capability change arrives (it need not change the
    // audio status). seedReceiverAudioNote gives a dialog the value when
    // Setup opens. Static seams: the constructor and createSetupDialog use
    // them, and a test can reach them without booting a window.
    using ReceiverAudioNoteSource = std::function<RemoteReceiverAudioNote()>;
    static RemoteReceiverAudioNote receiverAudioNoteFor(const RemoteMediaController* media);
    static void wireReceiverAudioNotePush(QObject* dialogRoot, RemoteMediaController* media,
                                          RadioModel* model, ReceiverAudioNoteSource source);
    static void seedReceiverAudioNote(SetupDialog* dialog, const ReceiverAudioNoteSource& source);
    static void applyAntennaChangeForTest(RadioModel* model, int sliceId,
                                          const QString& antennaName);
    // Production composition seam: flags remain per-slice while the RX
    // applet follows the active slice. Exercised without booting a window.
    static void wireAutoAgcVisuals(RadioModel* model, SliceModel* slice,
                                   VfoWidget* flag, class RxApplet* applet);
    static void refreshAutoAgcVisuals(RadioModel* model, SliceModel* slice,
                                      VfoWidget* flag, class RxApplet* applet);
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
    /// 3D Stacked-Trace Spectrum Plan Task 14 fix-forward (recall) + Task 15
    /// fix-forward (save): wires 3D Floor's per-band recall AND save into a
    /// live SpectrumWidget. Recall: pushes the value stored for `pan`'s
    /// current band immediately, then keeps it synced on every
    /// PanadapterModel::bandChanged() crossing. Save: `spectrum`'s
    /// dssFloorDepthChanged (an operator edit through either the overlay
    /// menu or the Setup page) writes back to `pan`'s CURRENT band, guarded
    /// against the recall push above re-triggering itself. Extracted as a
    /// static seam (rather than inlined at the MainWindow constructor call
    /// site) for the same reason as the helpers above: MainWindow needs a
    /// full RadioModel to construct, which no unit-test executable can
    /// afford.
    static void wireDss3DFloorRecallForTest(PanadapterModel* pan,
                                            SpectrumWidget* spectrum);

    // ── TNF operator controls (design sections 7, 7.5 and 10.2) ───────────
    // Public statics rather than file-local helpers: MainWindow needs a full
    // RadioModel (WDSP, audio, network) to construct, which no unit-test
    // executable can afford, so this is the only shape in which the
    // indicator's and the notice's pure behaviours can be tested. All three
    // are called from production code below.

    /// Status-bar TNF light. Struck through in every off state, and
    /// escalated to the amber warning colour once notches exist and are
    /// being bypassed: under maintainer decision D-a the master enable ships
    /// OFF, so the operator's first notch does nothing until they turn it
    /// on, and that state has to be unmistakable rather than a subtle tint.
    static QString tnfIndicatorStyleSheet(bool globalEnabled, int notchCount);

    /// Status-bar TNF light tooltip: how many notches exist, whether they
    /// are doing anything, and that the click toggles them.
    static QString tnfIndicatorTooltip(int notchCount, bool globalEnabled);

    /// The DSP > TNF accelerator. Public and static so the collision test can
    /// read it without an instance; design section 10.2 fixes it in code
    /// because NereusSDR has no shortcut-assignment subsystem to register
    /// with.
    static QKeySequence tnfToggleShortcut();

    /// Operator notice for a rejected notch add. Pure so the wording can be
    /// pinned without standing MainWindow up.
    static QString tnfAddRejectedNotice(const QString& reason);

public slots:
    // ── Phase 3M-0 Task 14 helper slots ──────────────────────────────────
    // Update PA status badge state. Wired by Task 17 to
    // RadioModel::paTrippedChanged.
    void setPaTripped(bool tripped);

    // Update TX Inhibit label visibility. Wired by Task 17 to
    // TxInhibitMonitor::txInhibitedChanged.
    void setTxInhibited(bool inhibited);
    // HL2 port part 2: the TX badge tooltip and toast for the current reason.
    void showTxInhibitReason();

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

    /// Parity Task 18: the Display flyout and the Clarity Re-tune of one
    /// pan's strip act on that pan's own display (they were wired on
    /// pan-0's strip only).
    void wirePanDisplayFlyout(class SpectrumOverlayPanel* panel,
                              class SpectrumWidget* sw, const QString& panId);
    /// Parity Task 18: Clarity tunes the active pan (Setup's display pages
    /// follow the same pan). Each strip's badge shows Clarity's state on
    /// the pan it is tuning and nothing on the others.
    void refreshClarityBadges();
    /// The stream the pan Clarity tunes is fed from; -1 when it has none.
    int clarityStreamIndex() const;

    /// TNF: push the global notch list at EVERY pan (design section 8.1).
    ///
    /// Under D1 the notch list is global, so each pan gets the same vector
    /// and converts it into its own pixel space. Deliberately not the spot
    /// overlay's activeSpectrumWidget()-only shape, which would leave every
    /// secondary pan blank.
    ///
    /// The only Hz-to-MHz conversion site in the TNF stack: NotchModel
    /// stores absolute RF Hz, NotchMarker::freqMhz is MHz, and everything
    /// else (the five interaction signals, setNotchMinWidthHz, the dent
    /// maths) stays in Hz.
    void refreshPanNotchMarkers();

    /// TNF: push the visual-notch (trace dent) toggle at EVERY pan
    /// (design section 8.3).
    ///
    /// Same shape as refreshPanNotchMarkers above, and for the same reason:
    /// the toggle is one global NotchModel flag but each pan owns its own
    /// SpectrumWidget, so it has to reach pans created after startup too.
    /// Armed from PanadapterStack::countChanged and from
    /// NotchModel::visualEnabledChanged.
    void refreshPanVisualNotch();

    /// TNF: push WDSP's minimum notch width at EVERY pan
    /// (design sections 7.2 and 8.3).
    ///
    /// SpectrumWidget cannot pull this: it varies with the filter's
    /// coefficient count and the channel's DSP rate, and neither is visible
    /// from the GUI layer. Without the push every pan keeps the 100 Hz
    /// construction default forever, which silently mis-sizes both the
    /// edge-drag clamp and the dent span the moment the operator changes nc
    /// on the DSP Options page or the radio's sample rate.
    ///
    /// Resolved per pan through that pan's OWN activeSliceIndex, matching
    /// refreshPanStatusOverlays, because a multi-pan layout can host slices
    /// on channels with different rates. Re-arms the per-channel
    /// RxChannel::minNotchWidthChanged follow on every pass
    /// (Qt::UniqueConnection), since the channel a pan resolves to changes
    /// with the slice set.
    void refreshPanNotchMinWidth();

    /// TNF: connect the five per-pan notch interaction signals on EVERY pan.
    ///
    /// Armed from PanadapterStack::countChanged, the same hook
    /// wirePanBadgeHandlers uses, and for the same reason: a pan created by
    /// a layout switch would otherwise never be wired. Not folded into
    /// wireSpectrumForPan, which skips pan-0 by design.
    ///
    /// The handlers below are SLOTS, not lambdas, so Qt::UniqueConnection is
    /// actually honoured on the re-arm.
    void wirePanNotchHandlers();

    /// TNF inbound handlers. Each mutates the single global NotchModel; the
    /// frequencies arrive already resolved in the emitting pan's own
    /// frequency mapping, so nothing here consults an active pan.
    void onNotchCreateRequested(const QString& panId, double freqHz, bool narrow);
    void onPanNotchCreateRequested(double freqHz, bool narrow);
    void onNotchMoveRequested(int id, double newFreqHz);
    void onNotchWidthRequested(int id, double widthHz);
    void onNotchActiveRequested(int id, bool active);
    void onNotchRemoveRequested(int id);

    /// TNF: the +TNF overlay button on pan `panId`. A distinct slot from
    /// onNotchCreateRequested above, which takes an already-resolved
    /// frequency from a panadapter click; this one has to compose the centre
    /// itself from the pan's own slice (design section 7.5).
    ///
    /// A slot, not a lambda, for the same reason the five handlers above are:
    /// ensureOverlayPanels re-runs on every PanadapterStack::countChanged and
    /// Qt6 silently ignores Qt::UniqueConnection on a lambda target, so a
    /// lambda would add one extra notch per layout switch the operator had
    /// ever made.
    void onAddTnfClicked(const QString& panId);

    /// TNF: surface a rejected add. Without this a +TNF press inside the
    /// 10 Hz dedupe window is silently ignored and the button reads as dead.
    void onNotchAddRejected(const QString& reason);
    /// R-R3-21: the Core refused a remote window's notch move, toggle or
    /// delete. The reason is already a plain sentence.
    void onNotchRequestRefused(const QString& reason);
    // Fix wave I3: a receiver refused a noise reducer (NR3 with no model).

    /// TNF: repaint the status-bar light from NotchModel. Driven by every
    /// signal that can change either half of what it shows.
    void refreshTnfIndicator();

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

    /// Parity Task 18 (C8, R-R3-24, R-R3-34): a connected pan with no slice
    /// says so and how to add one. Allowed once the radio is connected and,
    /// in a remote window, once the Core's slices have arrived; before that
    /// an empty pan is only waiting for them.
    void refreshNoSliceHints();

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

    /// Task B5: PanadapterApplet::addSliceRequested / floatRequested both
    /// carry the emitting applet's own panId(), so these forward straight to
    /// RadioModel::addSliceOnPan / PanadapterStack::floatPanadapter with no
    /// activePanId() lookup -- the same "acts on the pan that was clicked"
    /// shape as the three handlers above. Named slots rather than lambdas:
    /// wirePanBadgeHandlers() re-runs on every countChanged, and
    /// Qt::UniqueConnection is silently dropped for lambda targets (see the
    /// comment on the `activated` connect in wirePanBadgeHandlers()), so a
    /// lambda here would re-add itself on every layout change and fire the
    /// add-slice/float once per accumulated connection.
    void onPanAddSliceRequested(const QString& panId);
    void onPanFloatRequested(const QString& panId);

    void onConnectionStateChanged();

    /// Open the radio list / scan panel.
    ///
    /// Remote-daemon R2 Task 20: this is the single choke point for ELEVEN
    /// entry points (the Radio menu's Manage Radios, the status-bar RTT and
    /// station blocks, two context menus, two per-pan "click to connect"
    /// affordances, the auto-connect-failed handler, the disconnect
    /// auto-reopen inside onConnectionStateChanged itself, and two
    /// auto-reconnect fallbacks), which is why the remote gate lives inside
    /// it rather than at any of them. Everything the panel does drives a
    /// LOCAL RadioConnection this process does not own in Role::Remote.
    ///
    /// Fix round 4: this said TEN and enumerated ten. The eleventh, the
    /// disconnect auto-reopen, is the one that fires without a user action,
    /// which is exactly the one an enumeration written from the UI misses.
    void showConnectionPanel();

    /// Open the network + audio diagnostics window.
    ///
    /// Remote-daemon R2 Task 20: extracted from three identical inline
    /// lambdas (status-bar RTT click, audio-pip click, segment context
    /// menu) precisely so the remote gate has one place to live. Every
    /// number the dialog shows comes from this process's RadioConnection
    /// (null in Role::Remote) or its AudioEngine (present but never
    /// started), so on a remote client it is a window of zeroes at best.
    void openNetworkDiagnostics();

    /// Refresh Core session actions and suppress local-only hardware controls.
    /// Called after both mirrored radio state and Core connection activity
    /// change; a reachable Core need not have its radio connected.
    void applyRemoteRoleGating();
    void applyRemotePureSignalAppletGate();

    /// R-R3-46: Radio > Protocol Info in a remote window, from the Core's
    /// description of its radio (name, P1/P2, firmware, MAC, address).
    void showCoreRadioInfo();

    /// Reuse one StationClient/media controller to dial the configured Core.
    void connectToStation();
    void disconnectFromStation();
    void connectionRequestedByOperator();
    void showRemoteConnectionPanel();
    void refreshRemoteConnectionUi();
    /// R-R3-38: places the stop message over the top of the window's
    /// content, keeping the layout under it.
    void placeCoreStopBanner();
    bool transmitControlsPermitted() const;
    /// Desktop remote transmit (R-IOS-13): why this remote window may not
    /// transmit now, in the Core's words when the Core gave them.
    QString remoteTransmitReason() const;
    /// Slice control plan Task 11 fix: why this window may not move
    /// transmit to another slice now (empty when it may). One answer for
    /// the flag's TX button and the TX applet's letters: a hosting window
    /// must hold transmit; a remote window must hold it on the Core.
    QString transmitSliceChoiceReason() const;
    /// Moves transmit to slice `sliceId`: tx.setTxSlice from a remote
    /// window, the arbiter here otherwise. Nothing while the reason above
    /// is not empty.
    void requestTransmitSlice(int sliceId);
    /// Gives every flag's TX button the transmit permission and the reason
    /// above.
    void refreshFlagTransmitGates();
    void applyFlagTransmitGate(VfoWidget* flag) const;
    /// Desktop remote transmit (R-R3-42): the TCI server forwards a
    /// program's transmit to the Core while the Core takes this window's
    /// keys, and not otherwise.
    void refreshTciRemoteTransmit();
    // Group B fix wave: whether BYPS (RX bypass on TX) may change the
    // radio's relay setting, and why not.
    bool rxBypassPermitted() const;
    QString rxBypassUnavailableReason() const;
    /// R-R3-49 (parity Task 1): whether this window may change the transmit
    /// settings that key nothing (RF Power, TX filter, DSP > Options TX):
    /// always in local direct mode; in a remote window while the handshake
    /// is complete, the Core offers transmitSettingsVersion at least
    /// `minVersion`, and its radio is not on the air (RadioModel::isCoreOnAir)
    /// or the Core takes them on the air (kTransmitSettingsOnAirVersion).
    bool transmitSettingsPermitted(int minVersion = 1) const;
    /// Why not, in plain words: the on-the-air reason while the Core's
    /// radio is on the air, otherwise the Core reason
    /// (IStationLink::transmitSettingsUnavailableReason). Empty when
    /// permitted.
    QString transmitSettingsReason(int minVersion = 1) const;
    /// R-R3-49 (parity Task 7): whether this window may arm PureSignal
    /// (PS-A): always in local direct mode; in a remote window with remote
    /// transmit, or on a Core at transmitSettingsVersion 7 while its radio
    /// is off the air.
    bool pureSignalArmingPermitted() const;
    /// Why not: the on-the-air reason on a Core that offers arming, else
    /// the remote transmit reason as before. Empty when permitted.
    QString pureSignalArmingReason() const;
    /// R-R3-21 / R-R3-10: whether the Core's settings can be changed from
    /// this window (always in local direct mode; in a remote window only
    /// while connected and holding the Core's settings snapshot), and the
    /// reason shown while they cannot ("Connect to the Core to change
    /// these." while disconnected, "The Core has not sent its settings."
    /// while connected without them).
    bool stationSettingsAvailable() const;
    QString stationSettingsReason() const;
    // Parity Task 19 (R-IOS-25, B7.2): the Spot Hub's Core settings and the
    // station's spot sources, available or disabled with a reason.
    void refreshSpotHubAvailability();
    // iPhone plan Task 22 / parity Task 20 (R-IOS-26): the FreeDV Reporter
    // dialog's Send QSY, Send and Clear, the Core's in a remote window.
    void refreshFreedvReporterAvailability();

    /// The one place in src/gui that runs `new SetupDialog`.
    ///
    /// Remote-daemon R2 Task 20, fix round 2. Twelve call sites all did the
    /// identical construct / WA_DeleteOnClose / wireSetupDialog() shape.
    ///
    /// R-R3-21 / R-R3-10: never refuses. A remote window opens Setup
    /// connected or not; the dialog is told whether the Core's settings are
    /// available (setStationSettingsAvailable) and disables the Core's pages
    /// and controls until they are. A Core page opened before the Core's
    /// settings first arrive is not built, so no widget reads or writes a
    /// ship default as if it were the Core's. Callers still null-check.
    ///
    /// A slot rather than a plain method so it stays resolvable off
    /// MainWindow::staticMetaObject: MainWindow cannot be constructed in a
    /// unit test, and that name lookup is the only seam the suite has on it.
    SetupDialog* createSetupDialog();
    void showSupportDialog();
    void showAudioDiagnoseDialog();
    void showFeatureRequestDialog();
    void showFeatureRequestDialogImpl();
    /// R-R3-38: the stop message's Check for updates: runs the same
    /// version check and says the result in a notice.
    void checkForUpdates();
    // Phase 3M-4 Task 8: open the modeless PureSignal dialog (Tools menu).
    // Lazy-constructs on first invocation; subsequent calls show + raise the
    // existing instance so geometry persists across opens.
    void openPureSignalDialog();
    // Phase 3J-2 H1: open the modeless Spot Hub / FreeDV Reporter dialogs
    // (Tools menu). Same lazy-construction pattern as openPureSignalDialog;
    // both dialogs are single-instance for the lifetime of MainWindow.
    void openSpotHub();
    void openFreeDVReporter();
    /// Task B4 (bottom-banner + pan-menu epic): +PAN icon click handler.
    /// Also the View > Pan Layout… (Ctrl+L) menu action's target. Gated on
    /// m_radioModel->isConnected(); opens PanLayoutDialog sized to
    /// qMin(BoardCapabilities::maxSlices, RadioModel::userStreamCount())
    /// -- opening a new pan always claims its own DDC, so that ceiling
    /// (not the raw slice count) is what bounds how many pans a board can
    /// actually fill -- and, on accept, applies the selected layout via
    /// applyPanLayout(). Replaces the Phase 3F Sub-Epic D Task 10
    /// showPanMenu() context menu -- its add-slice-on-active-pan and
    /// float-active-pan actions move to each pan's own right-click menu in
    /// Task B5, since both routed through activePanId() and a control
    /// drawn on a pan should target that pan.
    void showPanLayoutDialog();

    /// Apply a pan layout template and reconcile the slices against it.
    ///
    /// Codex review round 3, PR #293. There were three places that applied a
    /// layout: session restore, the View menu, and the +PAN affordance
    /// (a dropdown at the time; a drawn icon since Task B4). Each had its
    /// own copy of the pan-count table, the id list and an add-only slice
    /// loop. Round 2's fix for slices orphaned by a shrinking layout went
    /// into the View-menu copy only, so the defect stayed live through
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
    void populateEmptyPans(bool operatorRequested = false);

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

    // R-R3-21: open Setup at the leaf labelled `label` (as SetupDialog's
    // tree shows it). The menu entries that front an existing Setup page
    // use this; Setup's own local/remote gating applies.
    void openSetupAtPage(const QString& label);
    // R-R3-21: Radio > Antenna Setup. Setup > Hardware Config on its
    // Antenna / ALEX tab (the page's first tab when the radio has no ALEX).
    void openAntennaSetup();
    // R-R3-21: the Diversity dialog, shared by Tools > Diversity, DSP >
    // Diversity and the VFO flag's right-click Diversity entry.
    void openDiversityDialog();

    // R-R3-21 / R-R3-49: a container's band, mode, filter, antenna, tune
    // step and function buttons and its VFO display act on the container's
    // own slice (ContainerWidget::rxSource(), slices A to D), never on the
    // active slice; refreshContainerControls() shows each container its
    // slice's state. A container set to a slice that is not open shows its
    // buttons unavailable, and a click says why and changes nothing.
    void wireContainerControls(class ContainerWidget* container);
    // `only`: just that item (one added while the window runs).
    void refreshContainerControls(MeterItem* only = nullptr);
    void refreshContainer(class ContainerWidget* container, MeterItem* only = nullptr);
    void refreshContainerMeter(class ContainerWidget* container, MeterWidget* meter, MeterItem* only, const QJsonObject& context, bool frequencyOnly = false);
    QString containerSessionId() const;
    int containerControlRxSource(const ContainerWidget* container) const;
    // Tuning: only the VFO display and band items of the containers on
    // `slice` (frequency and band).
    void refreshContainerFrequency(SliceModel* slice);
    void watchContainerItems(QWidget* content);
    void onContainerItemAdded(MeterItem* item);
    void reconcileMiniDisplays();
    void presentMiniFrame(int sliceId, const QVector<float>& traceDbm,
                          const QVector<float>& waterfallDbm, double centreHz,
                          double spanHz, bool transmit, bool advance);
    void clearMiniSlice(int sliceId);
    void watchSlicesForContainers();
    SliceModel* containerSlice(const class ContainerWidget* container) const;
    void onContainerModeClicked(class ContainerWidget* container, int index);
    void onContainerFilterClicked(class ContainerWidget* container, int index);
    /// A container's filter right-click: edit or reset that preset, as the
    /// VFO flag's and RX applet's filter buttons offer. `index` -1 is the
    /// VFO display's, which means the slice's current preset.
    void onContainerFilterContext(class ContainerWidget* container, int index);

    void onContainerAntennaSelected(class ContainerWidget* container, int index);
    void onContainerTuneStepSelected(class ContainerWidget* container, int index);
    void onContainerFrequencyStep(class ContainerWidget* container, int64_t deltaHz);
    void onContainerOtherButtonClicked(class ContainerWidget* container, int buttonId);
    // A click that changed nothing: the reason, as a toast.
    void showContainerButtonReason(const QString& reason);

    // Phase 3P-II Phase 4 Task 97: soft-alert toast when peak forward power
    // exceeds the PGXL cap. R-R3-47 / R-R3-22: the Core computes the alert
    // (StationAccessoryData); this shows it from `accessoryData`.
    void onPowerCapAlertChanged();

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
    /// R-R3-46 / R-R3-11: every pan's receive offset from the ADC of the
    /// stream it shows (slice A's for a pan fed nothing yet).
    void pushSpectrumCalToPans();

private:
#ifdef NEREUS_BUILD_TESTS
    friend class TxLetterTakeWindowAccess;
#endif
    // Parity Task 21 (R-IOS-18, B6.2, B6.3): the Core's radio from a remote
    // window: Setup > This Core opened on what the menu asked for.
    enum class ThisCoreFocus { ChangeRadio, EditRadio, ForgetRadio };
    void openThisCore(ThisCoreFocus focus);
    void addCoreRadioActions(QMenu& menu);
    void refreshCoreRadioActions();
    QString forgetCoreRadioReason() const;
    QString coreRadioAddressText() const;
    QString coreRadioMacText() const;
    QAction* m_actChangeCoreRadio {nullptr};
    QAction* m_actEditCoreRadio {nullptr};
    QAction* m_actForgetCoreRadio {nullptr};
    void ensureRemoteSession();
    // iPhone app plan Task 39 (D14, R-IOS-13): a remote window's transmit
    // meters from the Core's `txState`.
    void wireRemoteTransmitMeters();
    // iPhone app plan Task 25 (R-IOS-18): the VAX applet's "Station
    // computer" section from the Core's `vax` object, shown while the Core
    // sends it, with its meters subscribed only while the applet shows it.
    void wireRemoteStationVax();
    void refreshRemoteStationVax();
    // iPhone app plan Task 78 (R-IOS-02, R-IOS-30): the remote window's
    // screens for several devices on one Core (MultiDeviceController), the
    // bottom banner's holder, the TX pill and applet button that take
    // transmit, the flags' radio freeze, other devices' markers and the
    // empty band's offer of a take.
    void wireRemoteDevices();
    void refreshRemoteDeviceScreens();
    void refreshForeignMarkers();
    void refreshTakeReceiverOffer();
    // Slice control plan Task 13: the bottom RX area's all-slice chooser.
    enum class SliceChooserAction { Listen, TakeControl, Release, StopListening, Select, NewSlice };
    void ensureSliceChooser();
    void openSliceChooser();
    void refreshSliceChooser();
    // Slice control plan Task 14a: a flag's menu action sent as the
    // chooser's request.
    void runFlagAccessAction(SliceChooserAction action, int sliceId);
    void runSliceChooserAction(SliceChooserAction action, int sliceId);
    // Slice control plan Task 14b (ruling U5): a listened flag's "Your
    // volume" and Mute, sent as this device's own listening level
    // (slice.setListenLevel), and the level a flag shows. level is 0..100.
    void setFlagListenVolume(int sliceId, int level, bool muted);
    // Slice control plan Task 10: while this desktop hosts, its slice
    // requests run as the station device through m_hostingSlices, with the
    // checks, questions and slice access a remote device's take. Otherwise
    // RadioModel's own entry points, as before.
    HostingSliceActions* hostingSlices() const;
    void wireHostingSlices();
    bool selectSliceForWindow(int sliceId);
    void addSliceForWindow(const QString& panId);
    void closeSliceForWindow(int sliceId);
    void showHostingQuestion(const SessionMessage& question);
    void showHostingNotice(const SessionMessage& notice);
    void layoutHostingNoticeCards();
    std::pair<int, bool> listenVolumeFor(int sliceId);
    void finishSliceChooserRequest(const QByteArray& verb, bool accepted,
                                   const QString& reason);
    void onPanTakeTransmitRequested(const QString& panId);
    void buildUI();
    void buildMenuBar();
    void buildStatusBar();
    void applyDarkTheme();
    void tryAutoReconnect();
    void wireSliceToSpectrum();

    /// R1 Task 4 fix round 1 (reviewer Finding 2): the one place that sets
    /// both of RadioModel's spectrum view hooks to the same widget --
    /// the concrete m_spectrumWidget (82 Setup-page call sites) and the
    /// abstract m_spectrumSink (RadioModel's own SWR-overlay and
    /// applyClaritySmoothDefaults calls). Before this helper existed the
    /// two calls were convention only: a future call site that wrote
    /// setSpectrumWidget() without the matching setSpectrumSink() would
    /// compile clean and pass every test, and would silently leave the SWR
    /// overlay and Reset-to-Smooth-Defaults acting on a stale widget while
    /// Setup pages kept following the live one. Routing every caller
    /// through here makes that impossible instead of merely undocumented.
    void setSpectrumHooks(SpectrumWidget* sw);

    /// Stream 0's engine. Back-compat accessor for call sites that still
    /// address "the" FFT engine (display settings, Max Bin, auto-zoom).
    ///
    /// R1 Task 6: delegates to m_fftEnginePool, which creates on first use.
    /// Stream 0's engine is always built during buildUI() before any of
    /// these call sites can run, so in practice this never triggers that
    /// creation -- it is a lookup, exactly as the old
    /// m_fftEngines.value(0, nullptr) was. Defined out-of-line in the .cpp:
    /// FftEnginePool is only forward-declared here (m_fftEnginePool is a
    /// pointer member), and calling a method on it needs the complete type.
    FFTEngine* primaryFftEngine() const;

    /// Fix round 1 finding 1 (coordinator spec review): re-reads the four
    /// display AppSettings keys (DisplaySpectrumFps / DisplayFftSize /
    /// DisplayFftWindow / DisplayHzPerBinTarget) into an FftPoolConfig.
    /// Called from ensureStreamWired() immediately before building a
    /// stream that does not exist yet, so a stream created after a live
    /// Setup -> Display change picks up the current value rather than
    /// whatever was last pushed to the pool -- restores the per-stream
    /// freshness the pre-extraction createFftEngineForStream had (it read
    /// these keys inside its own per-engine construction). Deliberately a
    /// MainWindow method, not something FftEnginePool does itself: the
    /// pool must stay settings-agnostic so the Task 9/10 daemon can supply
    /// its own config with no AppSettings dependency in src/core. No-op if
    /// the pool does not exist yet.
    ///
    /// Fix round 2 (coordinator spec review): calls
    /// m_fftEnginePool->setConfigForNewStreams(), NOT setConfig(). The
    /// difference matters here specifically because MainWindow's
    /// auto-zoom lambda calls engine->setFftSize() directly on a single
    /// stream, a deliberate, never-persisted-to-AppSettings override --
    /// setConfig() would retroactively snap that stream's engine back to
    /// the AppSettings baseline the moment ANY new stream appeared. See
    /// FftEnginePool.h's doc comments on both methods for the full
    /// reasoning.
    void refreshFftPoolConfig();


    /// R1 Task 6: wires a pool-provided engine into MainWindow's other
    /// subsystems the first time streamIndex is seen -- the raw I/Q feed
    /// from RadioModel, this stream's initial sample rate, and its own
    /// NoiseFloorTracker. A no-op beyond the lookup on later calls for a
    /// stream that is already wired. Returns nullptr only if the pool
    /// itself is not ready yet or streamIndex is negative.
    ///
    /// Engine creation, reuse, the four display AppSettings-sourced
    /// knobs, and thread parking all moved into FftEnginePool; this is
    /// what is left of the old createFftEngineForStream once that part
    /// is extracted -- the MainWindow-specific wiring the pool has no way
    /// to express (I/Q routing and NoiseFloorTracker are RadioModel- and
    /// MainWindow-owned concerns, not spectrum-engine concerns).
    FFTEngine* ensureStreamWired(int streamIndex);

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

    /// Phase 3F: create the VfoWidget for a slice on the given
    /// SpectrumWidget, push initial state, wire all intent + bidi
    /// signals, and register it in m_vfoWidgetsBySlice. Returns the new
    /// flag (or nullptr). Used at sliceAdded, by wireSliceToSpectrum() for
    /// Slice A, and on panKeyChanged migration and rehost for every slice
    /// (Slice A's included), so the wiring lives in one place. Mirrors AetherSDR's
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

    // Task A8 fix round 1 shipped reapplyHardwarePresenceGates(), a second
    // pass ANDing a live hardware query on top of m_chromeBar's fold
    // decision after every relayout(). Fix round 2 removed it: it only
    // ran from resize/tick call sites, so a signal that fired BETWEEN
    // relayout() calls (plug in a TGXL while folded past rung 2) bypassed
    // it entirely. ChromeBarController::setItemAvailable (see its own doc
    // comment) replaces it -- the presence/DSP-active facts are now
    // reported straight from the signal that changes them
    // (TunerModel::presenceChanged, the rxFilterChainCount capability
    // gate, updatePsaIndicatorVisibility, RxDashboard::badgeAvailabilityChanged),
    // each followed by a relayout() call at that same call site, so there
    // is no window where the fact and the controller's decision disagree.

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

    // Remote-daemon R2 Task 20. MUST be declared before m_radioModel: the
    // initializer list reads it to pick the model's Role, and a member
    // initialised out of a later-declared member is undefined behaviour,
    // not merely a warning.
    RemoteStationOptions m_station;

    RadioModel* m_radioModel{nullptr};
    QPointer<DesktopStationController> m_desktopStationController;
    QPointer<class TakeTransmitDialog> m_desktopTakeDialog;
    QPointer<class StationServer> m_desktopBoundServer;
    // Slice control plan Task 10: bound to m_desktopBoundServer.
    std::unique_ptr<HostingSliceActions> m_hostingSlices;
    QPointer<QDialog> m_hostingQuestionDialog;
    QList<QPointer<NoticeCard>> m_hostingNoticeCards;
    QMetaObject::Connection m_desktopHolderConnection;
    QMetaObject::Connection m_desktopDevicesConnection;
    QMetaObject::Connection m_desktopPresenceConnection;
    QMetaObject::Connection m_desktopOwnershipConnection;
    QMetaObject::Connection m_desktopActiveConnection;
    quint64 m_desktopBindingGeneration{0};
    // Stop clears controller.enabled() before the Host has finished stopping.
    // TCI leaves host mode only after the controller's final lifecycle signal.
    bool m_desktopHostStopConfirmed{true};
    bool desktopHosting() const;
    bool desktopSliceAllowed(int sliceId) const;
    // Slice control plan Task 14a: a slice the hosting window listens to
    // while another device controls it (not one held for an absent device).
    // Its flag shows, read-only, in place of a foreign marker.
    bool desktopListensTo(int sliceId) const;
    // Slice control plan Task 14a: while hosting, each flag's presentation
    // and TX badge. The one place the host's badge rule lives; it runs on
    // every change sliceOnAir depends on (holder, TX slice, pending
    // handoff, MOX state).
    void refreshDesktopFlags();
    SliceModel* activeSliceForWindow() const;
    // Slice control plan Task 15: the slice this window's RX area follows
    // (bottom bar, flag focus, RX applet). A listened slice may be it; the
    // active slice (menus, transmit) never moves with it.
    SliceModel* windowRxSlice() const;
    // Slice control plan Task 15 (ruling U7): the RX applet's tabs, one per
    // slice this window controls or listens to and shows, each saying who
    // controls it; the applet binds windowRxSlice() with its access.
    void refreshRxAppletSlices();
    // TX rulings (JJ, 2026-09-30, item 3): each pan's ATT flyout is held
    // with sliceChangeRefusal()'s reason while the pan's slice is one this
    // window only listens to.
    void refreshOverlayAttAccess();
    bool sliceShownInWindow(int sliceId) const;
    // Slice control plan Task 16 (rulings U1, U2, U7). Whether this window
    // shares slices with other devices (it hosts, or it is a remote window
    // on a Core that reports slice access); whether it controls a slice (a
    // window that shares nothing controls every slice); whether it listens
    // to one another device controls.
    bool windowSharesSlices() const;
    bool windowControlsSlice(int sliceId) const;
    bool windowListensTo(int sliceId) const;
    // This window's pans: the current layout's ids that exist (floating
    // ones included). A pan made only to hold another device's slice is
    // not one of them.
    QStringList windowPanIds() const;
    // The window pan showing `slice`: where this window placed a listened
    // slice, else its pan key, else the window pan that lists it. Empty
    // when this window does not show it.
    QString windowPanFor(const SliceModel* slice) const;
    // Move a slice's flag onto the pan that shows it (spectrumForSlice).
    void rehostSliceView(SliceModel* slice);
    // Ruling U1 and U2: show `sliceId` in this window. A pan that shows it
    // comes forward (a floating one is raised); otherwise it goes to an
    // empty main-window pan, else the window grows to the next layout that
    // fits, else the operator picks a pan. Nothing is added or moved for
    // any other slice.
    void revealSliceInWindow(int sliceId);
    // A placement for a slice this window no longer only listens to is
    // dropped; one it now controls takes the placement as its pan.
    void reconcileListenPlacements();
    // A layout change placed this slice (m_markerOnlyPlacement) and this
    // window still shows it there.
    bool markerOnlyPlacement(int sliceId) const;
    // True when `host` (the pan spectrumForSlice gives the slice) is not a
    // pan of the slice's own: a listened slice this window placed, or one
    // whose own pan is gone. Its demodulator shift then comes from its own
    // stream's centre, never from that pan's view.
    bool hostIsNotSlicesOwnPan(const SliceModel* slice) const;
    // Slice control plan Task 15 fix round 1: the server this window hosts
    // and the Core link it shares slices over (each null when it does not),
    // from which the chooser, the flags and the RX applet say who controls
    // each slice; and why this window may not change a slice, as the RX
    // applet says it (empty when it may). The container buttons ask it.
    class StationServer* sliceAccessServer() const;
    class StationClient* sliceAccessClient() const;
    QString sliceChangeRefusal(int sliceId) const;
    void refreshActiveSlicePresentation();
    bool desktopOwnsTransmit() const;
    // Station VOX (whole-branch review, TX path): in a hosting window,
    // while another device holds transmit, why VOX may not be armed here
    // (that device has the transmitter); empty when it may.
    QString desktopVoxHolderReason() const;
    // The TX applet's VOX button and every Setup's Enable VOX follow it.
    void applyDesktopVoxHolderGate();
    void requestDesktopTransmit(bool tune, bool on);
    // Fix wave (hosting 2-TONE parity): MOX, TUNE and the 2-tone test.
    void requestDesktopKey(DesktopStationController::Key key, bool on);
    void handleDesktopTakeResult(const DesktopStationController::RequestResult& result);
    ConnectionPanel* m_connectionPanel{nullptr};
    SupportDialog* m_supportDialog{nullptr};

    // Remote-daemon R2 Task 20: the wss client, Qt-parented to this window.
    // Null in local direct mode and never constructed there. Declared as a
    // forward-declared pointer so this header stays free of the session
    // stack (StationClient.h drags in the whole message codec).
    class StationClient* m_stationClient{nullptr};
    class RemoteMediaController* m_remoteMedia{nullptr};
    // R-R3-44: this computer's VAX channels, fed from the Core's receiver
    // streams. Remote windows only; deleted right after m_remoteMedia.
    class RemoteVaxRouter* m_remoteVax{nullptr};
    // R-R3-42, R-R3-44 fix wave: one toast per receiver-audio stop across
    // TCI and the VAX channels.
    ReceiverStopNotices m_receiverStopNotices;
    class RemoteTelemetryController* m_remoteTelemetry{nullptr};
    QString m_savedCoreName;
    class RemoteConnectionController* m_remoteConnection{nullptr};
    class RemoteConnectionPanel* m_remoteConnectionPanel{nullptr};
    /// R-R3-38: the stop message over the content of a remote window.
    class CoreStopBanner* m_coreStopBanner{nullptr};
    /// The latest release's version from the release page, or an empty
    /// string when it could not be read. Shared by the issue reporter's
    /// version check and Check for updates (R-R3-38). Not a slot: moc
    /// cannot carry the std::function parameter.
    void fetchLatestReleaseVersion(std::function<void(const QString&)> done);
    /// R-R3-17: forget which link-lost reason was last toasted.
    void clearStationLinkToastMemory();
    // R-R3-34: the receive-layout notice, toasted once per distinct message
    // in a remote window and in a local one.
    ReceiveLayoutNotices m_receiveLayoutNotices;
    // R-R3-17: a failing redial repeats the same reason every backoff step
    // (up to once a minute). Toast each distinct reason once; the Connections
    // window, Core panel and title bar keep showing it persistently. Cleared
    // by a completed handshake or whenever the link goes inactive (an
    // operator disconnect or cancelled retry from any surface).
    QString m_lastStationLinkLostReason;
    QString m_lastReconnectToastReason;
    bool m_stationLinkLostSeen{false};
    bool m_reconnectToastSeen{false};
    bool m_stationDisconnectRequested{false};
    bool m_initialConnectionStarted{false};
    bool m_retiringSession{false};
    bool m_connectionPickerManaged{false};

    // Phase 3M-4 Task 8: PsForm modeless dialog (Tools > PureSignal...).
    // Lazy-constructed on first openPureSignalDialog() call; lives for the
    // lifetime of MainWindow.  Hidden on close, never destroyed.
    PsForm* m_psForm{nullptr};
    // R-R3-21: one Diversity dialog per window, kept across closes. It was
    // a function-local static, shared by every window a session switch
    // built, so the second window reopened a dialog its first window had
    // already destroyed.
    QPointer<DiversityDialog> m_diversityDialog;
    // R-R3-21: every slice's signals the container controls follow.
    QList<QMetaObject::Connection> m_containerSliceConnections;
    struct MiniProducer;
    std::map<int, std::unique_ptr<MiniProducer>> m_miniProducers;
    // R-R3-21 / R-R3-49: maps each container function and band button to
    // its target on the container's own slice.
    std::unique_ptr<ContainerButtonDispatcher> m_containerButtons;
    // TX safety fix round 4 (2026-09-30): a local window's Radio >
    // Disconnect is available while a link is Connected or the lost-link
    // lock holds (the operator's way to lift it and stop recovery).
    bool localDisconnectAvailable() const;
    QAction* m_actPureSignal{nullptr};
    QAction* m_actTxEqualizer{nullptr};
    QAction* m_actDspPureSignal{nullptr};
    // Tools menu developer test entries (antenna switch toast, TX-bound
    // re-route dialog). Kept so applyRemoteRoleGating() can give them the
    // transmit gate in a remote session (R-R3-21, R-R3-25).
    QAction* m_actTestAntennaToast{nullptr};
    QAction* m_actTestTxBoundReRoute{nullptr};

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

    // Task B4: +PAN status-bar icon (AetherSDR MainWindow.cpp:4368-4396
    // [@c6481cb]). Dimmed + retooltipped by updateAddPanButtonState(),
    // called from buildStatusBar() at construction and again on every
    // connectionStateChanged so the affordance reads unavailable before
    // the click rather than no-opping after it (design §8.2).
    QLabel*  m_addPanButton{nullptr};
    void     updateAddPanButtonState();

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
    // R1 Task 6: per-stream engine lifecycle, the four global display
    // AppSettings keys, and the shared FFT thread (formerly m_fftEngines /
    // m_fftThread, plus createFftEngineForStream) now live in
    // FftEnginePool (src/core/spectrum/FftEnginePool.h) -- core work that
    // used to sit in this QWidget. refreshFftPoolConfig() reads the four
    // AppSettings keys, fills an FftPoolConfig, and calls
    // setConfigForNewStreams(); ensureStreamWired() is what invokes it,
    // immediately before building a stream that does not exist yet.
    // (An earlier version of this comment said buildUI() did the reading
    // and that the pool's setConfig() was the setter. Neither is true:
    // buildUI() reads none of those keys, and setConfig() -- which also
    // reconfigures ALREADY-EXISTING engines -- would retroactively stomp
    // a live auto-zoom override every time a new stream appeared, which
    // is exactly why the two setters were split.) Every other call site
    // reaches an engine via primaryFftEngine() or
    // m_fftEnginePool->engineForStream(streamIndex). threadCount defaults
    // to 1 (today's single shared thread); if a 5-stream 1536 kHz bench
    // shows it saturating, raising it is a follow-up needing maintainer
    // sign-off (thread architecture), per design section 4.5a.
    FftEnginePool* m_fftEnginePool{nullptr};

    /// R1 Task 7: consumer-to-stream subscription set of record.
    /// rebuildFftRouting() resolves its pan/slice walk into this, and the
    /// PanadapterStack::panRetired handler wired in buildUI() drops a
    /// retired pan's subscriptions so a later applyTo() cannot resurrect a
    /// mapping for a pan that no longer exists. applyTo() is the only thing
    /// that then writes m_radioModel->fftRouter() itself, rebuilding it
    /// wholesale from whatever this member currently holds. Plain value
    /// member: it is a QMap wrapper with no signals and no heap ownership
    /// question, not a QObject that needs a pointer + parent.
    ///
    /// disconnectPanadapter() also unsubscribes here, but it is NOT the
    /// live teardown path and must not be read as one: it has had no
    /// caller since before this branch (PanadapterStack.h says as much),
    /// and panRetired is what actually fires when a pan goes away. Task 7
    /// added the unsubscribe call into it anyway, so that the function
    /// stays correct if it is ever revived; it is dead code that predates
    /// this work, left alone deliberately rather than deleted here.
    FftTopology m_topology;

    /// One NoiseFloorTracker per stream, fed by that stream's FFT engine.
    /// Auto AGC-T needs the noise floor of the band a slice is actually on;
    /// a single tracker fed from stream 0 would mis-set every other slice.
    QMap<int, class NoiseFloorTracker*> m_streamNoiseFloors;
    /// Remote-window parity Task 29 (A11, R-R3-49): the rise and fall of
    /// the pan hosting the transmitting slice while the radio is keyed
    /// (the transmit display, grid, palette, waterfall levels and red
    /// border), shared by a local and a remote window. Its transmitPanId()
    /// is the pan dispatchFftFrameToPans skips while keyed. Qt-parented to
    /// this window; its source is one of the two below.
    class MoxDisplayController* m_moxDisplay{nullptr};
    // Parity Task 31 (A11, R-R3-49): display duplex (DUP), the window's
    // DisplayDuplex setting and its View menu item.
    bool m_displayDuplexSetting{false};
    QPointer<QAction> m_displayDuplexAction;
    void setDisplayDuplexSetting(bool on);
    void applyDisplayDuplex();
    std::unique_ptr<class LocalTxDisplaySource> m_localTxDisplaySource;
    std::unique_ptr<class RemoteTxDisplaySource> m_remoteTxDisplaySource;

    // PR #212 follow-up: TX-side panadapter source via WDSP analyzer.
    // Source-switched in via the MoxController::moxStateChanged lambda
    // (FFTEngine for RX, TxAnalyzer for TX).  See TxAnalyzer.h header.
    TxAnalyzer*         m_txAnalyzer{nullptr};

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
    /// bottom status bar. Shown only on 2-ADC SKUs. Registered with
    /// m_chromeBar at rung 4 (design §6) so it folds under width pressure;
    /// its rxFilterChainCount>=2 capability gate is reported via
    /// ChromeBarController::setItemAvailable from the currentRadioChanged
    /// handler, not a direct setVisible call.
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
    // Band-stack dots. Head of the bar positionally, ahead of +PAN, but
    // registered at rung 10 so they fold with the other stubs.
    QWidget*    m_bandStackLabel{nullptr};
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
    QPointer<MeterWidget> m_meterWidget;
    MeterPoller* m_meterPoller{nullptr};
    void populateDefaultMeter();

    // Menu DSP actions
    // NR / NB submenus use exclusive QActionGroups; SNB / APF / BIN are
    // single toggle actions that mirror SliceModel state.
    QActionGroup* m_nrGroup   = nullptr;
    QActionGroup* m_nbGroup   = nullptr;
    QAction*      m_anfAction = nullptr;
    QAction*      m_snbAction = nullptr;
    QAction*      m_apfAction = nullptr;
    QAction*      m_binAction = nullptr;
    /// DSP > TNF. Checkable; two-way bound to NotchModel::globalEnabled, so
    /// the status-bar light and this item never disagree.
    QAction*      m_tnfAction = nullptr;

    // Mode menu actions (14 modes: 12 Thetis + NereusSDR-native
    // RADE-U / RADE-L from Phase 3R L3; mutual exclusion via
    // QActionGroup).
    QAction*      m_modeActions[14]  = {};
    QActionGroup* m_modeActionGroup  = nullptr;

    // AGC menu action group (Task 12)
    QActionGroup* m_agcGroup = nullptr;

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
    // Parity ruling C9: a remote window's CPU row, this computer's and the
    // Core's (CpuRowCycler); its source persisted as "CpuRowSource".
    CpuRowCycler m_cpuRowCycler;
    void refreshCpuRow(qint64 elapsedMs);
    // Process-CPU delta state (getrusage). Reset on toggle so the next
    // reading starts fresh rather than reporting accumulated cross-mode delta.
    qint64 m_cpuProcPrevWallUs{0};
    qint64 m_cpuProcPrevUserUs{0};
    qint64 m_cpuProcPrevSysUs{0};
    // System-CPU delta state (host_processor_info). Same reset rule as above.
    quint64 m_cpuSysPrevTotal{0};
    quint64 m_cpuSysPrevIdle{0};
    QVector<int> m_splitterSizesBeforeHide;  // saved splitter sizes for ☰ toggle

    // Status bar safety indicators (Phase 3M-0 Task 14 / sub-PR-8 restyle;
    // reserved safety slots added per design §4.5).
    // TX Inhibit: no widget. Formerly an "INH" pill, dimmed when
    //   TxInhibitMonitor::inhibited() asserts (wired Task 17).
    // m_paStatusBadge:  PA OK (green check) / PA FAULT (red check) StatusBadge.
    // m_txStatusBadge:  TX indicator, solid red when MOX engaged.
    // All four safety badges (TX Inhibit, PA, ADC overload, TX) live in
    // m_safetyGroup's fixed-width slots so an alarm never shifts geometry.
    // TX Inhibit has no widget of its own; it paints onto m_txStatusBadge.
    // m_txInhibited guards the MOX handler from repainting over an active
    // interlock, and the toast is held so it can be dismissed the instant
    // inhibit clears rather than aging out.
    bool                     m_txInhibited{false};
    QPointer<class StatusToast> m_txInhibitToast;
    StatusBadge* m_paStatusBadge{nullptr};
    StatusBadge* m_txStatusBadge{nullptr};
    // Task 78: who holds transmit when it is another device (or the
    // radio), beside the TX badge in the safety group. Hidden otherwise.
    StatusBadge* m_txHolderChip{nullptr};
    MultiDeviceController* m_multiDevice{nullptr};
    // Task 78: this session had a slice of this window's; a later empty
    // band means another device took it.
    bool m_hadSliceThisSession{false};
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
    // Slice control plan Task 13: the chooser (a popup), the request it
    // waits on (the verb, or "addSlice"), and the words for its success.
    QPointer<SliceChooser> m_sliceChooser;
    // TX badge take (JJ's ruling, 2026-09-30): a TX badge click on a slice
    // this window cannot make the TX slice at once takes what it needs, in
    // order: the slice (slice.takeControl, as the flag's Take control),
    // then transmit (tx.take, asked as the Take transmit control asks),
    // then makes the slice the TX slice. Two requests in sequence: a slice
    // take never carries transmit (ruling Q8). Nothing keys.
    enum class TxBadgeStage { None, Slice, Transmit };
    struct TxSliceAction {
        bool offered{false};
        QString toolTip;
        QString heldReason;
        bool enabled{false};
        QString effectiveWords;
    };
    TxSliceAction txSliceAction(int sliceId) const;
    void activateTransmitSlice(int sliceId, bool controlledOnly);
    quint64 txSliceIncarnation(int sliceId) const;
    bool txTakeTargetValid(int sliceId, SliceModel* target, quint64 incarnation,
                           bool requireControl) const;
    // The flag and letter share current eligibility; neither widget is
    // authority for the action or the pending target's Core lifetime.
    void applyTxBadgeOffer(VfoWidget* flag) const;
    // Whether this window holds transmit: the station device's hold in a
    // hosting window, the Core's word in a remote one, always on its own.
    bool windowHoldsTransmit() const;
    void startTxBadgeTake(int sliceId);
    // The Core answered the slice take the badge started.
    void txBadgeSliceAnswered(int sliceId, bool accepted);
    void takeTransmitForTxBadge(int sliceId);
    // A hosting take the badge started ended within its call, or later
    // (DesktopStationController::takeFinished names it).
    void settleTxBadgeHostTake(quint64 takeId);
    // A remote window's badge take after a change on the Core's transmit
    // state: the wait for the holder's change after a slice take ends, a
    // question no longer needed gives way to the take at once, and a
    // granted take ends on its slice.
    void continueTxBadgeTake();
    // Once this window holds transmit through the badge's own take, the
    // slice becomes the TX slice.
    void finishTxBadgeTakeIfHeld();
    void abandonTxBadgeTake();
    // Task 14a: the flag whose menu sent the request in flight (-1: none);
    // it shows the wait and then the Core's answer.
    int m_flagRequestSlice{-1};
    // TX badge take: the slice a badge click is taking, and its stage.
    int m_txBadgeTakeSlice{-1};
    QPointer<SliceModel> m_txBadgeTarget;
    quint64 m_txBadgeIncarnation{0};
    // A flag's slice.takeControl answer can precede its access delta.
    bool m_txBadgeTakingSlice{false};
    TxBadgeStage m_txBadgeTakeStage{TxBadgeStage::None};
    // The badge's own take of transmit: the hosting take's id, the remote
    // tx.take's command id, and whether it was granted. Only that take's
    // grant makes the slice the TX slice.
    quint64 m_txBadgeHostTakeId{0};
    quint32 m_txBadgeCommandId{0};
    bool m_txBadgeGranted{false};
    // Remote case 3: the Core answers slice.takeControl before it sends
    // the change of holder that take made (ruling Q8 moves or frees the
    // former controller's transmit), so the badge waits for that change
    // before it chooses between the question and the take at once.
    bool m_txBadgeAwaitingHolder{false};
    // The take question the badge opened (MultiDeviceController's dialog).
    QPointer<QDialog> m_txBadgeAsk;
    // Bumped whenever a badge take starts or ends, so a queued TX-slice
    // choice from an earlier one does nothing.
    quint64 m_txBadgeSerial{0};
    // Slice control plan Task 16: where this window shows a slice it only
    // listens to, when that slice's own pan key names no pan here (slice id
    // to pan id). Never written to the slice: its pan key belongs to its
    // controller.
    QHash<int, QString> m_listenPlacement;
    // The placements a layout change made (not a reveal): on that pan the
    // slice is its flag and its own edge marker only. Its stream is not
    // subscribed there, and its tunes never move that pan's VFO, view or
    // DDC centre (JJ's desktop listening ruling, 2026-09-30).
    QSet<int> m_markerOnlyPlacement;
    // A remote window's listen or take-control request whose slice is shown
    // once the Core accepts it (-1: none).
    int m_pendingRevealSlice{-1};
    bool m_reconcilingPlacements{false};
    // Task 14b: a remote window's own listening level per listened slice,
    // for the slice incarnation it was set on. The Core applies it and
    // does not publish it back, so the window keeps the value it sent,
    // first seeded from the slice's AF as the Core seeds it.
    struct RemoteListenVolume {
        quint64 incarnation{0};
        int level{100};
        bool muted{false};
    };
    QHash<int, RemoteListenVolume> m_remoteListenVolumes;
    // Task 14b: the hosting desktop's access controller whose level
    // changes for this station refresh its flags (connected once).
    QPointer<QObject> m_listenLevelSource;

    // Phase 3M-4 Task 10: PSA bottom-banner indicator pair (FB + PS labels).
    // Inserted between m_rxDashboard and m_stationBlock per design doc §4 #5
    // (option B).  Visibility gated on caps.hasPureSignal in
    // onConnectionStateChanged().  Wired to PureSignal coordinator + MOX
    // controller from inside the widget (auto-wired via RadioModel).
    PsaIndicatorWidget* m_psaIndicator{nullptr};

    // VFO flag crash lane (2026-09-30): wireSliceToSpectrum's window-wide
    // wiring is done; a Slice A made again gets only its own. (The separate
    // m_vfoWidget pointer to Slice A's flag is gone: it survived Slice A's
    // close in a remote window, and m_vfoWidgetsBySlice is the one owner.)
    bool m_sliceASpectrumWired{false};

    // Phase 3F hotfix 2026-05-27: per-slice VfoWidget tracking. Slice 0
    // (Slice A) is in it like every other slice;
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
    // 3D Stacked-Trace Spectrum Plan Task 22: left-panel display-controls
    // applet, follows the active panadapter via RadioModel::spectrumWidget().
    class DisplayApplet* m_displayApplet{nullptr};
    class EqApplet* m_eqApplet{nullptr};
    class VaxApplet* m_vaxApplet{nullptr};

    // Applets — Tasks 7-10 (NYI shells, hidden until Task 15 Container wiring)
    class DigitalApplet*    m_digitalApplet{nullptr};
    class PureSignalApplet* m_pureSignalApplet{nullptr};
    class ModMonitorApplet* m_modMonApplet{nullptr};   // AM Mod Monitor
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
    // Desktop remote transmit (R-R3-42): the forwarder is installed.
    bool               m_tciRemoteTransmitInstalled{false};
    // R-R3-48: the one TCI switch (this window's server and, on a Core with
    // a station server, the Core's) and the RF-Kit's band follow over this
    // window's server in a local window.
    class TciSwitch*       m_tciSwitch{nullptr};
    class RfKitBandFollow* m_rfKitBandFollow{nullptr};
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

    // Spectrum overlay panels. Parity Task 18: the pan-0-only target the
    // display-settings and Clarity wiring used is gone; each strip is wired
    // to its own pan.
    /// One control strip per pan, keyed by pan id. QPointer because the widget
    /// is parented to its pan's SpectrumWidget and dies with it when a layout
    /// switch retires the pan.
    QHash<QString, QPointer<class SpectrumOverlayPanel>> m_overlayPanels;

    // Parity Task 18: Clarity's badge state, shown on the strip of the pan
    // Clarity tunes, and that pan (the active pan when it last moved).
    bool m_clarityBadgeActive{false};
    bool m_clarityBadgePaused{false};
    QString m_clarityPanId;

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

    // Phase 3P-II Phase 4 Task 97 / R-R3-47: the power-cap alert count this
    // window has already shown (the Core de-bounces; see onPowerCapAlertChanged).
    qint64 m_powerCapAlertSeen{0};
};

} // namespace NereusSDR
