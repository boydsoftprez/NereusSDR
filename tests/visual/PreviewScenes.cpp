// no-port-check: NereusSDR-original visual preview tool; scene table.
// =================================================================
// tests/visual/PreviewScenes.cpp  (NereusSDR)
// =================================================================
//
// See PreviewScenes.h. Every baseline scene shows the current app as it
// is; nothing here changes how a widget looks. Transmit appearance comes
// from display-only seams (MoxDisplayController, SpectrumWidget,
// SMeterWidget, MeterPoller); nothing calls MoxController::setMox or
// reaches a RadioConnection (a Deferred-startup window has none, and the
// transmit scene refuses to run if one exists).
//
// Modification history (NereusSDR):
//   2026-10-26 - container-parity-* round 2: the text overlays carry
//                 token text (a new item starts empty, as Thetis's does),
//                 the short-title container's entry is named "Signal",
//                 the token picker opens where Qt places it (no forced
//                 move off the screen), and the web image notes say the
//                 picture's caption is its own. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - container-parity-* scenes: the items and editors the
//                 container item parity branch changed (button grid
//                 editors, mode grid, VFO display and its screens, clock,
//                 text overlay and LED, S-meter and tuning dials, web image
//                 pan background, artwork options, signal text, TX and RX
//                 sections, a long container title). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - The add-content scenes find the first Longpath row by
//                 typeId and the dial by "S-Meter dial" (picker names under
//                 headings). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-12 - Shared accessory display readings and independent legal
//                 marker / display scale by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-10-07 - Written for the workspace visual review. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-07 - Proposed workspace header scenes (header-always,
//                 header-reveal-idle, header-reveal-shown, header-hidden,
//                 arrange-on) with the Main Panel top crop. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-07 - Proposed four-edge workspace scenes (edge-right,
//                 edge-left, edge-top-bottom, edge-all-four, edge-all-four-clean,
//                 free-container, floating-container, drag-preview-left,
//                 drag-preview-between). J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-07 - Longpath instrument scenes (instruments-sheet,
//                 mixed-container, mixed-container-reveal) with region
//                 provenance. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - Workspace profile scenes (profile-ssb, profile-dx,
//                 profile-contest, profile-ft8, profile-rail-rest,
//                 profile-rail-open, profile-manage-dialog,
//                 profile-new-menu) with slice states and rail
//                 measurements. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - Whole-image provenance regions on every sidecar; coverage
//                 scenes (clean-spectrum, cw-digital, tx-processing,
//                 edit-add-content, edit-source-select, edit-properties,
//                 four-rx-look, setup-look-*). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Named regions for each pan plot, the overlays on it, the
//                 pan separators and each VFO flag; fed readings name the
//                 poller's reading-consumer path. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Round 2: plain-English plates, setup-look-* removed
//                 (look-dialog-setup-* covers them), clean-spectrum-reveal,
//                 full-content strip details, EdgeDropOverlay proposed, TX
//                 profiles seeded for tx-processing. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - edit-source-select shows the source chip's menu open
//                 (its own --menu image) in place of its plate;
//                 edit-properties notes the declared property controls;
//                 preview factories take the render mode. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Review C: the declared-property group and property_*
//                 controls are tagged proposed; the source menu note says
//                 it was opened programmatically. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The edge values note the right strip's 0 px padding and
//                 3 px handle. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - Every grab clears the item views' hover row first: Qt
//                 takes it from the desktop pointer when a view scrolls,
//                 even in a window that is never on screen. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - controls-sheet: today, preview now and Longpath controls
//                 side by side (ControlsSheet.cpp). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - strip-fit-today and strip-fit-preview: applet minimum
//                 widths against the Main Panel viewport. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - controls-sheet--indicators, --lit-buttons and --sizes:
//                 choice sheets for JJ (ControlsSheet.cpp). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - controls-sheet--lit-green and --scrollbars: green and
//                 scroll bar thickness choices (ControlsSheet.cpp).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Setup page scenes centre the page's tree item and keep
//                 the pointer's hover out of the tree, and Radio Status
//                 holds its uptime, so they are the same every run.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Window images carry applet_fit for every container
//                 holding applets. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - Folded receiver flag scenes (flag-fold--short-pans,
//                 flag-fold--two-band, flag-fold--two-band-look,
//                 flag-fold--states). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - Scenes for every remaining dialog, the standard dialogs and
//                 the remote set on a never-dialled remote window. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - dial-addable--* scenes: added copies of the SIGNAL
//                 STRENGTH dial, each fed for its own receiver. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - dial-addable--amp, --face and the --plate-b variants of
//                 --two-band and --tx (SMeterWidget's test-only plate
//                 limit). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-08 - colour-choice--status and colour-choice--data: the status
//                 and data colour choices (ControlsSheet.cpp). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - JJ's decisions are the product: decided--status and
//                 decided--data replace the colour-choice scenes, and the
//                 --plate-b dial variants are gone (the plate limit is
//                 unconditional). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - Window and dialog grabs dump every visible widget's
//                 rectangle (<name>.geometry.tsv). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - decided--controls: JJ's decided control look through the
//                 product path. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - menu-*, menubar-*, popup-*, toast-* and tooltip-* scenes:
//                 every menu, popup, toast and tooltip opened through the
//                 product's own path and grabbed while open; tooltips placed
//                 for the default 16x16 pointer (profile-rail-open too).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - The sample spots no longer re-set the notch markers to
//                 refresh the GPU overlay: setSpotMarkers marks it dirty
//                 itself now. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-09 - menu-pan-flyout-ant and menu-pan-flyout-att: the toolbar's
//                 ANT and ATT flyouts, opened as the BAND one is. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - Window scenes wait for a quiet layout (waitForQuietLayout)
//                 before the freeze and before the grab, not only a fixed
//                 time, and note a pan resized after its feed. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - menu-freedv-reporter stops the dialog's own timers after
//                 the member-timer cut, so its idle sweep cannot remove the
//                 sample station in a scene slowed past 30 s by load.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - Setup's later tabs (dialog-setup-<page>--<tab>, the HL2
//                 ones on a posed Hermes Lite 2) and --scrolled step scenes
//                 for every page and tab that scrolls. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-09 - dialog-setup-hardware-config poses its board (a Saturn)
//                 as the tab scenes do. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-09 - D25: Setup size scenes, dialog-setup-<page>[--<tab>]--min at
//                 the dialog's minimum 820 x 600 for the pages that rearrange
//                 to fit, and --wide at 1440 x 900 for Antenna / ALEX and OC
//                 Outputs; a --scrolled scene steps a table frame that scrolls
//                 sideways inside a page that scrolls down. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - Lit-state scenes posed after the window is shown and the
//                 live pass has run: dialog-lit-operate (OPERATE on in the
//                 tuner, PGXL and RF2K-S applets), dialog-lit-antenna (a
//                 tuner antenna selected), dialog-lit-psa-on (PS-A checked)
//                 and dialog-lit-ps-badge (the PureSignal badge), all
//                 through setters and injected status lines; nothing
//                 connects or starts discovery. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-09 - cw-digital seeds the TX profiles (the Phone / CW mic
//                 profile list was empty) and puts its CW page plate in the
//                 applet's own layout, above its first row, instead of over
//                 its level meter. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-09 - strip-scroll scenes: a Receive strip, the RX applet over
//                 GPU meters, unscrolled and scrolled, in both looks, with
//                 where each meter's native window sits. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - A --strip-<edge>-full grab waits until the grown strip's
//                 size stops changing (cw-digital at laptop@1.25 caught it
//                 one pixel short in some runs). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-10 - No --scrolled scene for Hardware Config > Calibration: at
//                 the dialog's own size its groups now always sit side by
//                 side with nothing to scroll (D25, SetupFit::FitScrollArea).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-10 - shootOpenPopup() places a popup again from the size it
//                 settles at: the DSP popups with sliders grew after they
//                 were placed, in some runs, and ran past the window's
//                 lower edge at laptop-1280x800@1. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================
//   2026-10-11 - Headers are shown or hidden: header-always is header-shown,
//                 arrange-on mixes a kept and a hidden header and notes each
//                 eye button's rect, mixed-container-reveal is
//                 mixed-container-hidden; header-reveal-idle,
//                 header-reveal-shown and clean-spectrum-reveal are gone.
//                 The look-off scenes, baseline-header-reveal among them,
//                 are unchanged. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-11 - arrange-drag-edge-free and arrange-drag-edge-floating: a
//                 free container and a floating window held over an edge of
//                 the main window while arranging, with the landing box
//                 shown. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-11 - floating-container-pinned: the floating window with its
//                 header's pin clicked. Both floating scenes record
//                 stays_on_top. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-11 - A scrolled Setup scene whose page fits says so in the
//                 context (nothingToScroll) as well as failing. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 - flag-tab--* scenes: each tab of the open slice flag, in
//                 today's look and the workspace look. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-12 - accessory-readouts and accessory-readouts-classic: the
//                 tuner, Power Genius and RF2K-S panels with forward power
//                 as one large bar and the rest as slim rows, in four
//                 states, with the workspace look on and off. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-14 - Scenes for the manual figures: baseline-connected (one
//                 pan, the TX applet under RX, "You control" on slice A's
//                 flag, the radio named in the status bar, TX profiles
//                 seeded), dialog-setup-tx-profile-active (Default
//                 active), dialog-setup-digital-modes-full and
//                 dialog-remote-setup-tci-server (the dialog grown until
//                 the page needs no scrolling; the Core's TCI group shows
//                 only in a remote window). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-15 - dialog-remote-setup-tci-server poses the Core's station
//                 link (tests/fakes/FakeStationLink.h) and its TCI server
//                 values, so the Core's group shows them, enabled.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-10-15 - baseline-connected-tx: the same window with the Main
//                 Panel's entries above the TX applet hidden, so the whole
//                 applet shows. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.

#include "models/MainPowerMeter.h"
#include "models/AmplifierModel.h"
#include "gui/instruments/PowerReadingDescriptor.h"
#include "gui/instruments/NeedleInstrument.h"
#include "gui/meters/ItemGroup.h"
#include "gui/meters/presets/CompositePresetItem.h"
#include "gui/applets/SectionParts.h"
#include "gui/containers/ContentPropertyEditor.h"
#include "gui/meters/AntennaButtonItem.h"
#include "gui/meters/ClockItem.h"
#include "gui/meters/DialItem.h"
#include "gui/meters/FilterButtonItem.h"
#include "gui/meters/SignalTextItem.h"
#include "gui/meters/TextOverlayItem.h"
#include "gui/meters/VfoDisplayItem.h"
#include "gui/meters/WebImageItem.h"
#include "gui/meters/presets/SignalTextPresetItem.h"
#include <QLinearGradient>
#include <QTemporaryDir>
#include <QToolButton>
#include <QUrl>
#include "PreviewScenes.h"

#include <QAction>
#include <QApplication>
#include <QColorDialog>
#include <QDateTime>
#include <QGridLayout>
#include <QHostAddress>
#include <QInputDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QTabBar>
#include <QTabWidget>
#include <QTest>
#include <QUuid>
#include <QCoreApplication>
#include <QDialog>
#include <QElapsedTimer>
#include <QFile>
#include <QEnterEvent>
#include <QHoverEvent>
#include <QLabel>
#include <QFontInfo>
#include <QFontMetrics>
#include <QGroupBox>
#include <QMenu>
#include <QCheckBox>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QTimeZone>
#include <QToolTip>
#include <QMetaObject>
#include <QPointer>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QWindow>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStatusBar>
#include <QTimer>
#include <QTreeWidget>
#include <QAbstractItemView>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHash>
#include <QJsonDocument>
#include <QListWidget>
#include <QSet>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QHelpEvent>
#include <QMenuBar>
#include <QMetaProperty>
#include <QMouseEvent>
#include <QScreen>
#include <QStackedWidget>
#include <QTableView>
#include <QWidgetAction>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>

#include "ControlsSheet.h"
#include "PreviewData.h"
#include "core/AppSettings.h"
#include "core/ControlRanges.h"
#include "core/RadioDiscovery.h"
#include "core/audio/VirtualCableDetector.h"
#include "core/dsp/DspAssetValidation.h"
#include "core/session/RemoteDevicesState.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationClient.h"
#include "gui/AboutDialog.h"
#include "gui/AddCustomRadioDialog.h"
#include "gui/AmpViewWindow.h"
#include "gui/ConnectionPanel.h"
#include "gui/ConnectionSelector.h"
#include "gui/CoreTargetEditor.h"
#include "gui/CoreTargetStore.h"
#include "gui/DspAssetDialog.h"
#include "gui/FreeDVReporterDialog.h"
#include "gui/GuiSessionCoordinator.h"
#include "gui/LanScanDialog.h"
#include "gui/NetworkDiagnosticsDialog.h"
#include "gui/PgxlSaveRebootDialog.h"
#include "gui/RemoteConnectionController.h"
#include "gui/RemoteDiagnosticsDialog.h"
#include "gui/SpotHubDialog.h"
#include "gui/StationStartupSelection.h"
#include "gui/SupportDialog.h"
#include "gui/VaxFirstRunDialog.h"
#include "gui/containers/MmioEndpointsDialog.h"
#include "gui/containers/MmioVariablePickerPopup.h"
#include "gui/multidevice/ConfirmChangeDialog.h"
#include "gui/multidevice/ReplaceDeviceDialog.h"
#include "gui/multidevice/TakeReceiverDialog.h"
#include "gui/multidevice/TakeTransmitDialog.h"
#include "gui/setup/AppearanceSetupPages.h"
#include "gui/setup/HardwarePage.h"
#include "gui/setup/TciLogWindow.h"
#include "gui/widgets/FilterPolicyDialog.h"
#include "gui/widgets/FilterPresetEditDialog.h"
#include "gui/widgets/TxBoundConfirmDialog.h"
#include "models/Band.h"
#include "core/WdspTypes.h"
#include "core/meters/SliceMeterPump.h"
#include "gui/DiversityDialog.h"
#include "gui/MainWindow.h"
#include "gui/MoxDisplayController.h"
#include "gui/PanLayoutDialog.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanadapterStack.h"
#include "gui/PsForm.h"
#include "gui/SMeterWidget.h"
#include "gui/SetupDialog.h"
#include "gui/SetupPage.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/SpectrumWidget.h"
#include "gui/TitleBar.h"
#include "gui/UnbuiltFeatures.h"
#include "gui/applets/CwxApplet.h"
#include "gui/applets/DigitalApplet.h"
#include "gui/applets/DvkApplet.h"
#include "gui/applets/EqApplet.h"
#include "gui/applets/PhoneCwApplet.h"
#include "gui/applets/TxApplet.h"
#include "gui/applets/AmpApplet.h"
#include "gui/applets/Rf2ksApplet.h"
#include "gui/applets/TunerApplet.h"
#include "gui/PsaIndicatorWidget.h"
#include "core/TgxlConnection.h"
#include "models/TunerModel.h"
#include <QHBoxLayout>
#include <QSignalBlocker>
#include "gui/applets/TxCfcDialog.h"
#include "gui/applets/TxEqDialog.h"
#include "gui/containers/ContainerManager.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerTypes.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/FloatingContainer.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/EdgeDockStrip.h"
#include "gui/containers/WorkspaceHeader.h"
#include "gui/containers/WorkspaceProfiles.h"
#include "gui/containers/WorkspaceProfilesDialog.h"
#include "gui/instruments/BarInstrument.h"
#include "gui/instruments/FrequencyInstrument.h"
#include "gui/instruments/InstrumentFooter.h"
#include "gui/instruments/LongpathContent.h"
#include "gui/instruments/NeedleInstrument.h"
#include "gui/instruments/SMeterDialContent.h"
#include "gui/widgets/BandwidthFilterPane.h"
#include "gui/meters/BandButtonItem.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "core/MicProfileManager.h"
#include "gui/meters/ModeButtonItem.h"
#include "gui/widgets/ProfileRail.h"
#include "gui/widgets/ProfileRailDock.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "core/FreeDVStation.h"
#include "core/TxInterlockPolicy.h"
#include "core/dsp/DspAssetService.h"
#include "core/session/IStationLink.h"
#include "core/StationTciController.h"
#include "models/StationTciModel.h"
#include "fakes/FakeStationLink.h"
#include "gui/applets/RxApplet.h"
#include "gui/widgets/AntennaSwitchToast.h"
#include "gui/widgets/RxDashboard.h"
#include "gui/widgets/StationBlock.h"
#include "gui/SliceChooser.h"
#include "gui/chrome/ChromeBarController.h"
#include "gui/widgets/StatusToast.h"
#include "gui/widgets/SystemTile.h"
#include "models/FreeDVStationModel.h"

namespace NereusSDR::Preview {

namespace {

constexpr int kSettleMs = 900;
constexpr int kDialogSettleMs = 900;
constexpr int kFinalSettleMs = 400;
constexpr int kNeedleSettleMaxMs = 4000;

// Levels the transmit scene poses.
constexpr float kTxForwardWatts = 85.0f;
constexpr float kTxSwr = 1.25f;
constexpr float kTxMicDb = -12.0f;
// The transmit display on pan 1: slice A's carrier, the default transmit
// filter (TransmitModel 100..2900 Hz) and illustrative levels.
constexpr double kTwoBandCarrierHz = 7185000.0;
constexpr int kTxFilterLowHz = 100;
constexpr int kTxFilterHighHz = 2900;
constexpr double kTxPeakDbm = -12.0;
constexpr double kTxFloorDbm = -100.0;
constexpr quint32 kTxSeed = 0x7E11u;

// Displayed noise floors per band (the median of the settled trace), as a
// live pan reads them on the default -60 to -115 dBm scale.
constexpr double kNoise80mDbm = -100.0;
constexpr double kNoise40mDbm = -104.0;
constexpr double kNoise20mDbm = -110.0;
constexpr double kNoise15mDbm = -114.0;

// ---- small helpers ---------------------------------------------------------

PanadapterApplet* appletFor(MainWindow* window, const QString& panId)
{
    for (PanadapterApplet* applet : window->findChildren<PanadapterApplet*>()) {
        if (applet->panId() == panId) {
            return applet;
        }
    }
    return nullptr;
}

SpectrumWidget* spectrumFor(MainWindow* window, const QString& panId)
{
    PanadapterApplet* applet = appletFor(window, panId);
    return applet != nullptr ? applet->spectrumWidget() : nullptr;
}

SliceModel* sliceOnPan(MainWindow* window, const QString& panId)
{
    PanadapterApplet* applet = appletFor(window, panId);
    if (applet == nullptr || window->radioModel() == nullptr) {
        return nullptr;
    }
    return window->radioModel()->sliceById(applet->activeSliceIndex());
}

QString panIdAt(int index)
{
    return QStringLiteral("pan-%1").arg(index);
}

// Takes the desktop pointer out of a capture. When an item view scrolls
// (QTreeWidget::setCurrentItem, say), Qt sets its hover row from
// QCursor::pos(), the real pointer, even in a window that is never on
// screen. Wherever the pointer happened to sit over the window's (unseen)
// rect, that row came out highlighted, so the Setup tree differed from run
// to run. A HoverLeave clears the row. No scene grabbed here hovers an
// item view on purpose (the controls sheet does, through its own grab).
void clearPointerHover(QWidget* root)
{
    QList<QAbstractItemView*> views = root->findChildren<QAbstractItemView*>();
    if (auto* self = qobject_cast<QAbstractItemView*>(root)) {
        views.append(self);
    }
    for (QAbstractItemView* view : views) {
        QHoverEvent leave(QEvent::HoverLeave, QPointF(-1, -1), QPointF(-1, -1), QPointF(-1, -1));
        QCoreApplication::sendEvent(view->viewport(), &leave);
    }
}

QImage grabWidget(QWidget* widget)
{
    clearPointerHover(widget);
    return widget->grab().toImage();
}

QRect deviceRect(const QRect& logical, double dpr)
{
    return QRect(static_cast<int>(std::lround(logical.x() * dpr)),
                 static_cast<int>(std::lround(logical.y() * dpr)),
                 static_cast<int>(std::lround(logical.width() * dpr)),
                 static_cast<int>(std::lround(logical.height() * dpr)));
}

QString shapeName(SignalShape shape)
{
    switch (shape) {
    case SignalShape::Usb: return QStringLiteral("USB");
    case SignalShape::Lsb: return QStringLiteral("LSB");
    case SignalShape::Cw:  return QStringLiteral("CW");
    case SignalShape::Ft8: return QStringLiteral("FT8");
    }
    return QString();
}

// ---- pan and slice content ------------------------------------------------

struct PanPose {
    QString panId;
    double sliceHz{0.0};
    DSPMode mode{DSPMode::USB};
    int filterLo{150};
    int filterHi{2850};
    double sMeterDbm{-140.0};
    PanSpectrum spectrum;
};

PanSpectrum twentyMetres(const QString& panId)
{
    PanSpectrum s;
    s.panId = panId;
    s.noiseDbm = kNoise20mDbm;
    s.seed = 20;
    s.signalList = {
        {14195000.0, -88.0, SignalShape::Usb},
        {14210000.0, -95.0, SignalShape::Usb},
        {14225000.0, -81.0, SignalShape::Usb},
        {14243000.0, -79.0, SignalShape::Usb},
        {14262000.0, -100.0, SignalShape::Usb},
        {14290000.0, -90.0, SignalShape::Usb},
    };
    return s;
}

PanSpectrum fortyMetresPhone(const QString& panId)
{
    PanSpectrum s;
    s.panId = panId;
    s.noiseDbm = kNoise40mDbm;
    s.seed = 40;
    s.signalList = {
        {7160000.0, -78.0, SignalShape::Lsb},
        {7172000.0, -92.0, SignalShape::Lsb},
        {7185000.0, -73.0, SignalShape::Lsb},
        {7198000.0, -85.0, SignalShape::Lsb},
        {7215000.0, -101.0, SignalShape::Lsb},
    };
    return s;
}

QVector<PanPose> twoBandPoses()
{
    PanPose a;
    a.panId = panIdAt(0);
    a.sliceHz = 7185000.0;
    a.mode = DSPMode::LSB;
    a.filterLo = -2850;
    a.filterHi = -150;
    a.sMeterDbm = -73.0;
    a.spectrum = fortyMetresPhone(a.panId);
    PanPose b;
    b.panId = panIdAt(1);
    b.sliceHz = 14225000.0;
    b.mode = DSPMode::USB;
    b.filterLo = 150;
    b.filterHi = 2850;
    b.sMeterDbm = -81.0;
    b.spectrum = twentyMetres(b.panId);
    return {a, b};
}

QVector<PanPose> fourRxPoses()
{
    PanPose a;
    a.panId = panIdAt(0);
    a.sliceHz = 3790000.0;
    a.mode = DSPMode::LSB;
    a.filterLo = -2850;
    a.filterHi = -150;
    a.sMeterDbm = -84.0;
    a.spectrum.panId = a.panId;
    a.spectrum.noiseDbm = kNoise80mDbm;
    a.spectrum.seed = 80;
    a.spectrum.signalList = {
        {3760000.0, -90.0, SignalShape::Lsb},
        {3775000.0, -97.0, SignalShape::Lsb},
        {3790000.0, -84.0, SignalShape::Lsb},
        {3805000.0, -79.0, SignalShape::Lsb},
    };

    PanPose b;
    b.panId = panIdAt(1);
    b.sliceHz = 7074000.0;
    b.mode = DSPMode::DIGU;
    b.filterLo = 100;
    b.filterHi = 3100;
    b.spectrum.panId = b.panId;
    b.spectrum.noiseDbm = kNoise40mDbm;
    b.spectrum.seed = 74;
    // 14 FT8 tones between 7.0742 and 7.0768 MHz, -95 to -70 dBm.
    constexpr int kTones = 14;
    constexpr double kFirstToneHz = 7074200.0;
    constexpr double kLastToneHz = 7076800.0;
    const double levels[kTones] = {-88, -95, -76, -91, -70, -84, -93,
                                   -79, -86, -73, -90, -82, -94, -77};
    double strongest = -1000.0;
    for (int i = 0; i < kTones; ++i) {
        const double hz = kFirstToneHz + (kLastToneHz - kFirstToneHz) * i / (kTones - 1);
        b.spectrum.signalList.append({hz, levels[i], SignalShape::Ft8});
        strongest = std::max(strongest, levels[i]);
    }
    b.sMeterDbm = strongest;

    PanPose c;
    c.panId = panIdAt(2);
    c.sliceHz = 14030000.0;
    c.mode = DSPMode::CWU;
    c.filterLo = 400;
    c.filterHi = 900;
    c.sMeterDbm = -76.0;
    c.spectrum.panId = c.panId;
    c.spectrum.noiseDbm = kNoise20mDbm;
    c.spectrum.seed = 14;
    c.spectrum.signalList = {
        {14012000.0, -88.0, SignalShape::Cw},
        {14021000.0, -101.0, SignalShape::Cw},
        {14030000.0, -76.0, SignalShape::Cw},
        {14038000.0, -93.0, SignalShape::Cw},
    };

    PanPose d;
    d.panId = panIdAt(3);
    d.sliceHz = 21300000.0;
    d.mode = DSPMode::USB;
    d.filterLo = 150;
    d.filterHi = 2850;
    d.sMeterDbm = -83.0;
    d.spectrum.panId = d.panId;
    d.spectrum.noiseDbm = kNoise15mDbm;
    d.spectrum.seed = 21;
    d.spectrum.signalList = {
        {21285000.0, -96.0, SignalShape::Usb},
        {21300000.0, -83.0, SignalShape::Usb},
        {21318000.0, -91.0, SignalShape::Usb},
    };
    return {a, b, c, d};
}

QVector<PanPose> defaultPoses()
{
    PanPose a;
    a.panId = panIdAt(0);
    a.sliceHz = 14225000.0;
    a.mode = DSPMode::USB;
    a.filterLo = 150;
    a.filterHi = 2850;
    a.sMeterDbm = -81.0;
    a.spectrum = twentyMetres(a.panId);
    return {a};
}

// Tunes each pan's slice. Returns false (with ctx.failure) on a missing pan.
bool tuneSlices(SceneContext& ctx, const QVector<PanPose>& poses, int activePanIndex)
{
    for (const PanPose& pose : poses) {
        SliceModel* slice = sliceOnPan(ctx.window, pose.panId);
        if (slice == nullptr) {
            ctx.failure = QStringLiteral("no slice on %1").arg(pose.panId);
            return false;
        }
        slice->setDspMode(pose.mode);
        slice->setFrequency(pose.sliceHz);
        slice->setFilter(pose.filterLo, pose.filterHi);
    }
    if (SliceModel* active = sliceOnPan(ctx.window, poses.value(activePanIndex).panId)) {
        ctx.model->setActiveSlice(active->sliceIndex());
    }
    pump(kSettleMs);
    return true;
}

// Holds every live readout and timer still. Runs after the scene's own
// state is set and before the data is fed.
void freeze(SceneContext& ctx)
{
    if (auto* poller = ctx.window->findChild<MeterPoller*>()) {
        poller->stop();
        ctx.frozen.append(QStringLiteral("meter-poller: stopped"));
    }
    // The RadioModel-owned pump writes every slice's meter readings (and
    // clears them to "no reading" with no DSP running).
    if (SliceMeterPump* pump = ctx.model->sliceMeterPump()) {
        pump->stop();
        ctx.frozen.append(QStringLiteral("slice-meter-pump: stopped"));
    }
    for (const QJsonValue& v : stopTimersUnder(ctx.window)) {
        ctx.stoppedTimers.append(v);
    }
    for (QWidget* top : QApplication::topLevelWidgets()) {
        if (top != ctx.window && top->isVisible()) {
            for (const QJsonValue& v : stopTimersUnder(top)) {
                ctx.stoppedTimers.append(v);
            }
        }
    }
    for (const QJsonValue& v : freezeReadouts(ctx.window)) {
        ctx.frozen.append(v);
    }
}

// Every S-meter: the main window's and any in a floating container.
QList<SMeterWidget*> allMeters(SceneContext& ctx)
{
    QList<SMeterWidget*> meters = ctx.window->findChildren<SMeterWidget*>();
    for (QWidget* top : QApplication::topLevelWidgets()) {
        if (top != ctx.window && top->isVisible()) {
            meters += top->findChildren<SMeterWidget*>();
        }
    }
    return meters;
}

// Sets every slice flag's meter and every S-meter needle (to the active
// pan's level).
void poseMeters(SceneContext& ctx, const QVector<PanPose>& poses, int activePanIndex)
{
    for (const PanPose& pose : poses) {
        if (SliceModel* slice = sliceOnPan(ctx.window, pose.panId)) {
            slice->setSignalStrengthDbm(pose.sMeterDbm);
        }
    }
    for (SMeterWidget* meter : allMeters(ctx)) {
        // An added dial copy reads its own receiver (feedDials), never the
        // active pan's level.
        if (meter->settingsStore() == SMeterWidget::SettingsStore::Instance) {
            continue;
        }
        meter->setLevel(static_cast<float>(poses.value(activePanIndex).sMeterDbm));
    }
}

// Waits for every S-meter needle to reach its target.
void waitForNeedle(SceneContext& ctx)
{
    const QList<SMeterWidget*> meters = allMeters(ctx);
    if (meters.isEmpty()) {
        ctx.notes.append(QStringLiteral("no SMeterWidget found"));
        return;
    }
    auto settled = [&meters]() {
        for (SMeterWidget* meter : meters) {
            if (std::abs(meter->testNeedleFraction() - meter->testNeedleTarget()) > 1e-6f) {
                return false;
            }
        }
        return true;
    };
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < kNeedleSettleMaxMs && !settled()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    if (!settled()) {
        ctx.unfrozen.append(QJsonObject{
            {QStringLiteral("item"), QStringLiteral("s-meter needle")},
            {QStringLiteral("region"), QStringLiteral("right panel, S-Meter")},
            {QStringLiteral("reason"), QStringLiteral("needle did not reach its target in time")}});
    }
}

void feedSpectra(SceneContext& ctx, const QVector<PanPose>& poses)
{
    for (const PanPose& pose : poses) {
        SpectrumWidget* sw = spectrumFor(ctx.window, pose.panId);
        if (sw == nullptr) {
            ctx.failure = QStringLiteral("no spectrum on %1").arg(pose.panId);
            return;
        }
        const SpectrumCheck c = feedPan(sw, pose.spectrum);
        QJsonArray sigs;
        for (const SignalSpec& s : pose.spectrum.signalList) {
            sigs.append(QJsonObject{{QStringLiteral("hz"), s.hz},
                                    {QStringLiteral("dbm"), s.dbm},
                                    {QStringLiteral("shape"), shapeName(s.shape)}});
        }
        const QJsonObject check{
            {QStringLiteral("pan"), c.panId},
            {QStringLiteral("target_hz"), c.targetHz},
            {QStringLiteral("target_dbm"), c.targetDbm},
            {QStringLiteral("displayed_dbm"), std::round(c.displayedDbm * 100.0) / 100.0},
            {QStringLiteral("within_1db"), std::abs(c.displayedDbm - c.targetDbm) <= 1.0},
            {QStringLiteral("centre_hz"), c.centreHz},
            {QStringLiteral("span_hz"), c.spanHz},
            {QStringLiteral("ddc_centre_hz"), c.ddcCentreHz},
            {QStringLiteral("sample_rate_hz"), c.sampleRateHz},
            {QStringLiteral("ref_level_dbm"), sw->refLevel()},
            {QStringLiteral("dynamic_range_db"), sw->dynamicRange()},
            {QStringLiteral("waterfall_rows_pushed"), c.waterfallRows},
            {QStringLiteral("waterfall_low_dbm"), sw->wfActiveLowThreshold()},
            {QStringLiteral("waterfall_high_dbm"), sw->wfActiveHighThreshold()},
            {QStringLiteral("waterfall_agc"), sw->wfAgcEnabled()},
            {QStringLiteral("noise_target_dbm"), c.noiseTargetDbm},
            {QStringLiteral("noise_displayed_median_dbm"), std::round(c.noiseDisplayedDbm * 100.0) / 100.0},
            {QStringLiteral("noise_bin_mean_offset_db"), std::round(c.noiseOffsetDb * 100.0) / 100.0},
            {QStringLiteral("waterfall_row_period_ms"), c.rowPeriodMs},
            {QStringLiteral("signals"), sigs}};
        ctx.checks.append(check);
        qInfo("check %s: target %.0f Hz %.1f dBm, displayed %.2f dBm (centre %.0f span %.0f ddc %.0f rate %.0f, "
              "waterfall %.1f..%.1f dBm, noise median %.1f for %.1f)",
              qPrintable(c.panId), c.targetHz, c.targetDbm, c.displayedDbm, c.centreHz,
              c.spanHz, c.ddcCentreHz, c.sampleRateHz, sw->wfActiveLowThreshold(),
              sw->wfActiveHighThreshold(), c.noiseDisplayedDbm, c.noiseTargetDbm);
    }
}

void addCrop(QVector<CropRect>& crops, const QString& name, QWidget* window, QWidget* w, double dpr)
{
    // A widget in another top-level (the floating Main Panel) is not in
    // this image.
    if (w == nullptr || !w->isVisible() || !window->isAncestorOf(w)) {
        return;
    }
    const QRect logical(w->mapTo(window, QPoint(0, 0)), w->size());
    const QRect clipped = logical.intersected(QRect(QPoint(0, 0), window->size()));
    if (clipped.isEmpty()) {
        return;
    }
    crops.append({name, clipped, deviceRect(clipped, dpr)});
}

VfoWidget* flagForSlice(MainWindow* window, SliceModel* slice)
{
    for (VfoWidget* flag : window->findChildren<VfoWidget*>()) {
        if (flag->sliceForTest() == slice && flag->isVisible()) {
            return flag;
        }
    }
    return nullptr;
}

// One line per visible widget under `root`: region, a path of class
// names with sibling indexes, and the rectangle relative to `root`.
void dumpGeometry(QWidget* root, const QString& region, QStringList& lines)
{
    if (root == nullptr) {
        lines << region + QStringLiteral(" <missing>");
        return;
    }
    std::function<void(QWidget*, const QString&)> walk = [&](QWidget* w, const QString& path) {
        if (!w->isVisible()) {
            return;
        }
        const QRect r(w->mapTo(root, QPoint(0, 0)), w->size());
        lines << QStringLiteral("%1 %2 %3,%4 %5x%6")
                     .arg(region, path)
                     .arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
        int index = 0;
        for (QObject* o : w->children()) {
            if (!o->isWidgetType()) {
                continue;
            }
            auto* child = static_cast<QWidget*>(o);
            walk(child, path + QLatin1Char('/') + QString::fromLatin1(child->metaObject()->className())
                            + QLatin1Char('[') + QString::number(index) + QLatin1Char(']'));
            ++index;
        }
    };
    walk(root, QString::fromLatin1(root->metaObject()->className()));
}

// Capture resolved widget fonts as evidence alongside the raster image.
// Painted fonts and explicit rich spans have separate source/test coverage.
QJsonArray visibleTextFonts(QWidget* root)
{
    QJsonArray rows;
    QList<QWidget*> widgets = root->findChildren<QWidget*>();
    widgets.prepend(root);
    for (QWidget* widget : widgets) {
        if (!widget->isVisible()) {
            continue;
        }
        QString text;
        if (auto* label = qobject_cast<QLabel*>(widget)) {
            text = label->text();
        } else if (auto* button = qobject_cast<QAbstractButton*>(widget)) {
            text = button->text();
        } else if (auto* edit = qobject_cast<QLineEdit*>(widget)) {
            text = edit->text();
        } else if (auto* combo = qobject_cast<QComboBox*>(widget)) {
            text = combo->currentText();
        } else if (auto* group = qobject_cast<QGroupBox*>(widget)) {
            text = group->title();
        } else if (auto* tabs = qobject_cast<QTabBar*>(widget)) {
            for (int i = 0; i < tabs->count(); ++i) {
                text += tabs->tabText(i) + QLatin1Char(' ');
            }
        }
        if (text.isEmpty()) {
            continue;
        }
        const QFontInfo info(widget->font());
        const QFontMetrics metrics(widget->font(), widget);
        rows.append(QJsonObject{
            {QStringLiteral("class"), QString::fromLatin1(widget->metaObject()->className())},
            {QStringLiteral("object"), widget->objectName()},
            {QStringLiteral("text"), text},
            {QStringLiteral("resolved_px"), info.pixelSize()},
            {QStringLiteral("font_height"), metrics.height()},
            {QStringLiteral("rect"), QJsonArray{widget->width(), widget->height()}},
        });
    }
    return rows;
}

// The control kind a widget is, for masking a diff against the controls
// a look restyles; "other" for everything else.
QString controlKind(const QWidget* w)
{
    if (w->inherits("QScrollBar")) {
        return QStringLiteral("scrollbar");
    }
    if (w->inherits("QAbstractSlider")) {
        return QStringLiteral("slider");
    }
    if (w->inherits("QCheckBox")) {
        return QStringLiteral("checkbox");
    }
    if (w->inherits("QRadioButton")) {
        return QStringLiteral("radio");
    }
    if (w->inherits("QAbstractButton")) {
        return QStringLiteral("button");
    }
    if (w->inherits("QComboBox")) {
        return QStringLiteral("combo");
    }
    if (w->inherits("QLineEdit") || w->inherits("QAbstractSpinBox")) {
        return QStringLiteral("field");
    }
    if (w->inherits("QTabBar")) {
        return QStringLiteral("tabbar");
    }
    if (w->inherits("QTabWidget")) {
        return QStringLiteral("tabwidget");
    }
    if (w->inherits("QGroupBox")) {
        return QStringLiteral("groupbox");
    }
    return QStringLiteral("other");
}

// Every visible widget under `root`, root included, as tab-separated
// lines: path, class, object name, control kind, x, y, width, height (the
// rectangle in root's logical coordinates). Two runs of a scene compare
// line for line for moved or resized widgets.
QStringList widgetGeometry(QWidget* root)
{
    QStringList lines;
    if (root == nullptr) {
        return lines;
    }
    std::function<void(QWidget*, const QString&)> walk = [&](QWidget* w, const QString& path) {
        if (!w->isVisible()) {
            return;
        }
        const QRect r(w->mapTo(root, QPoint(0, 0)), w->size());
        lines << QStringLiteral("%1\t%2\t%3\t%4\t%5\t%6\t%7\t%8")
                     .arg(path, QString::fromLatin1(w->metaObject()->className()), w->objectName(),
                          controlKind(w))
                     .arg(r.x())
                     .arg(r.y())
                     .arg(r.width())
                     .arg(r.height());
        int index = 0;
        for (QObject* o : w->children()) {
            if (!o->isWidgetType()) {
                continue;
            }
            auto* child = static_cast<QWidget*>(o);
            if (!child->isWindow()) {
                walk(child, path + QLatin1Char('/') + QString::number(index));
            }
            ++index;
        }
    };
    walk(root, QStringLiteral("0"));
    return lines;
}

// ---- provenance regions ----------------------------------------------------

// The dynamic property a widget's provenance is tagged with; untagged
// widgets inherit from their nearest tagged ancestor, and an image root
// with no tag is retained.
constexpr auto kProvenanceProperty = "nereusPreviewProvenance";
// Optional region name and content type for a tagged widget.
constexpr auto kRegionNameProperty = "nereusPreviewRegionName";
constexpr auto kRegionTypeProperty = "nereusPreviewRegionType";
// Set on a widget that gets a region of its own even when its provenance
// matches its parent's (pan plots, the overlays on them, pan separators
// and VFO flags), so the retained-pixel proof can find them by name.
constexpr auto kForceRegionProperty = "nereusPreviewForceRegion";

QString provRetained() { return QStringLiteral("retained"); }
QString provLongpath() { return QStringLiteral("longpath-port"); }
QString provProposed() { return QStringLiteral("proposed"); }
QString provNotWired() { return QStringLiteral("exists-not-wired"); }
QString provNotImplemented() { return QStringLiteral("not-implemented"); }

void tagProvenance(QWidget* w, const QString& provenance, bool overwrite = true)
{
    if (w != nullptr && (overwrite || !w->property(kProvenanceProperty).isValid())) {
        w->setProperty(kProvenanceProperty, provenance);
    }
}

// Provenance by class. Container chrome stays retained (so containers in a
// proposed strip read as retained); the proposed headers, strips, splitter
// handles and profile chrome are proposed; the ported Longpath widgets are
// longpath-port.
QString classProvenance(QWidget* w)
{
    if (dynamic_cast<FrequencyInstrument*>(w) != nullptr || dynamic_cast<NeedleInstrument*>(w) != nullptr
        || dynamic_cast<BarInstrument*>(w) != nullptr || dynamic_cast<BandwidthFilterPane*>(w) != nullptr
        || dynamic_cast<ProfileRail*>(w) != nullptr) {
        return provLongpath();
    }
    if (dynamic_cast<EdgeDockStrip*>(w) != nullptr || dynamic_cast<WorkspaceGroupHeader*>(w) != nullptr
        || dynamic_cast<WorkspaceEntryHeader*>(w) != nullptr || dynamic_cast<WorkspaceCornerGrip*>(w) != nullptr
        || dynamic_cast<ProfileRailDock*>(w) != nullptr || dynamic_cast<WorkspaceProfilesDialog*>(w) != nullptr
        || w->objectName() == QLatin1String("profileRailPopover")
        || w->objectName() == QLatin1String("edgeDropOverlay")) {
        return provProposed();
    }
    // The property editor's declared-property group and controls
    // (ContentPropertyEditor, preview only); the rest of Container Settings
    // stays retained.
    if (w->objectName() == QLatin1String("declaredProperties")
        || w->objectName().startsWith(QLatin1String("property_"))) {
        return provProposed();
    }
    if (auto* handle = dynamic_cast<QSplitterHandle*>(w);
        handle != nullptr && dynamic_cast<EdgeDockSplitter*>(handle->splitter()) != nullptr) {
        return provProposed();
    }
    if (dynamic_cast<ContainerWidget*>(w) != nullptr) {
        return provRetained();
    }
    return {};
}

void forceRegion(QWidget* w, const QString& name)
{
    if (!w->property(kRegionNameProperty).isValid()) {
        w->setProperty(kRegionNameProperty, name);
    }
    w->setProperty(kForceRegionProperty, true);
}

// Names the panadapter regions under `root`: `pan-<n>-plot` for each pan's
// SpectrumWidget (the widget's own rect: spectrum, frequency scale and
// waterfall), `flag-<letter>` for each VFO flag on it, `pan-<n>-<object
// name or class>` for every other widget painted over the plot (the pan
// toolbar, corner buttons, status badges, hints; children of the
// SpectrumWidget or of its PanadapterApplet), and `pan-separator-<k>` for
// the splitter handles between pans. All keep the provenance they inherit.
void tagPanRegions(QWidget* root)
{
    QList<PanadapterApplet*> applets = root->findChildren<PanadapterApplet*>();
    if (auto* self = qobject_cast<PanadapterApplet*>(root)) {
        applets.prepend(self);
    }
    for (PanadapterApplet* applet : applets) {
        SpectrumWidget* plot = applet->spectrumWidget();
        if (plot == nullptr) {
            continue;
        }
        const QString pan = applet->panId();
        forceRegion(plot, pan + QStringLiteral("-plot"));
        QList<QWidget*> over;
        for (QObject* o : plot->children()) {
            if (auto* w = qobject_cast<QWidget*>(o)) {
                over.append(w);
            }
        }
        for (QObject* o : applet->children()) {
            auto* w = qobject_cast<QWidget*>(o);
            if (w != nullptr && w != plot && w->geometry().intersects(plot->geometry())) {
                over.append(w);
            }
        }
        QHash<QString, int> seen;
        for (QWidget* w : over) {
            if (w->isWindow()) {
                continue;
            }
            if (auto* flag = qobject_cast<VfoWidget*>(w)) {
                SliceModel* slice = flag->sliceForTest();
                const QChar letter = slice != nullptr ? slice->sliceLetter() : QChar(QLatin1Char('?'));
                forceRegion(flag, QStringLiteral("flag-%1").arg(letter));
                continue;
            }
            const QString what = w->objectName().isEmpty() ? QString::fromLatin1(w->metaObject()->className())
                                                             : w->objectName();
            const int n = seen.value(what, 0);
            seen.insert(what, n + 1);
            forceRegion(w, n == 0 ? QStringLiteral("%1-%2").arg(pan, what)
                                  : QStringLiteral("%1-%2-%3").arg(pan, what).arg(n + 1));
        }
    }
    QList<PanadapterStack*> stacks = root->findChildren<PanadapterStack*>();
    int separator = 0;
    for (PanadapterStack* stack : stacks) {
        for (QSplitterHandle* handle : stack->findChildren<QSplitterHandle*>()) {
            if (!handle->isVisibleTo(root)) {
                continue;
            }
            forceRegion(handle, QStringLiteral("pan-separator-%1").arg(separator++));
        }
    }
}

// Tags every widget under `root` by class (never over a scene's own tag),
// and each container entry widget with its entry id, content type and,
// for Longpath content, longpath-port.
void tagTree(QWidget* root)
{
    QList<QWidget*> all = root->findChildren<QWidget*>();
    all.prepend(root);
    for (QWidget* w : all) {
        const QString p = classProvenance(w);
        if (!p.isEmpty()) {
            tagProvenance(w, p, false);
        }
    }
    tagPanRegions(root);
    QList<ContainerContentHost*> hosts = root->findChildren<ContainerContentHost*>();
    if (auto* self = qobject_cast<ContainerContentHost*>(root)) {
        hosts.prepend(self);
    }
    for (ContainerContentHost* host : hosts) {
        const ContainerDocument doc = host->captureDocument();
        for (const auto& row : host->entryRows()) {
            if (row.widget == nullptr) {
                continue;
            }
            QString typeId;
            for (const ContentEntry& e : doc.contents) {
                if (e.id == row.entryId) {
                    typeId = e.typeId;
                }
            }
            if (!row.widget->property(kRegionNameProperty).isValid()) {
                row.widget->setProperty(kRegionNameProperty, row.entryId);
                row.widget->setProperty(kRegionTypeProperty, typeId);
            }
            if (typeId.startsWith(QLatin1String("longpath."))) {
                tagProvenance(row.widget, provLongpath(), false);
            }
        }
    }
}

QJsonArray regionRectJson(const QRect& r)
{
    return QJsonArray{r.x(), r.y(), r.width(), r.height()};
}

QRect regionRectFrom(const QJsonValue& v)
{
    const QJsonArray a = v.toArray();
    return QRect(a.at(0).toInt(), a.at(1).toInt(), a.at(2).toInt(), a.at(3).toInt());
}

// Appends the regions of `w` (and its subtree) to `out`. Rects are in
// `root` coordinates moved by `offset`; `clip` is in root coordinates.
void walkRegions(QWidget* root, QWidget* w, const QString& inherited, int parent, const QRect& clip,
                 const QPoint& offset, QJsonArray& out)
{
    const QRect rect(w == root ? QPoint(0, 0) : w->mapTo(root, QPoint(0, 0)), w->size());
    const QRect visible = rect.intersected(clip);
    if (visible.isEmpty()) {
        return;
    }
    const QString own = w->property(kProvenanceProperty).toString();
    const QString provenance = own.isEmpty() ? inherited : own;
    int index = parent;
    if (w == root || provenance != inherited || w->property(kForceRegionProperty).toBool()) {
        index = int(out.size());
        QString name = w->property(kRegionNameProperty).toString();
        if (name.isEmpty()) {
            name = w->objectName();
        }
        if (name.isEmpty()) {
            name = QString::fromLatin1(w->metaObject()->className());
        }
        QString type = w->property(kRegionTypeProperty).toString();
        if (type.isEmpty()) {
            type = w->property("containerFactoryType").toString();
        }
        const int depth = parent < 0 ? 0 : out.at(parent).toObject().value(QStringLiteral("depth")).toInt() + 1;
        out.append(QJsonObject{{QStringLiteral("name"), name},
                               {QStringLiteral("provenance"), provenance},
                               {QStringLiteral("rect"), regionRectJson(rect.translated(offset))},
                               {QStringLiteral("visible_rect"), regionRectJson(visible.translated(offset))},
                               {QStringLiteral("type"), type},
                               {QStringLiteral("widget_class"), QString::fromLatin1(w->metaObject()->className())},
                               {QStringLiteral("parent"), parent},
                               {QStringLiteral("depth"), depth}});
    }
    for (QObject* o : w->children()) {
        auto* child = qobject_cast<QWidget*>(o);
        if (child == nullptr || child->isWindow() || !child->isVisibleTo(root)) {
            continue;
        }
        walkRegions(root, child, provenance, index, visible, offset, out);
    }
}

// The regions of the top-level `top` painted at `offset` into an image of
// `imageSize`, appended to `out`.
void appendRegions(QJsonArray& out, QWidget* top, const QPoint& offset, const QSize& imageSize)
{
    tagTree(top);
    // A root that is not a top-level (a detail image of a strip's body)
    // inherits from its nearest tagged ancestor, as it would in the window.
    QString own = top->property(kProvenanceProperty).toString();
    for (QWidget* up = top->parentWidget(); own.isEmpty() && up != nullptr && !top->isWindow();
         up = up->parentWidget()) {
        own = up->property(kProvenanceProperty).toString();
        if (own.isEmpty()) {
            own = classProvenance(up);
        }
    }
    const QRect clip = QRect(-offset, imageSize).intersected(top->rect());
    walkRegions(top, top, own.isEmpty() ? provRetained() : own, -1, clip, QPoint(0, 0) + offset, out);
}

QString regionUnits()
{
    return QStringLiteral(
        "rect and visible_rect are [x, y, width, height] in logical px from this image's top-left; "
        "device px = logical px x dpr");
}

QString regionsContract()
{
    return QStringLiteral(
        "regions: the top-most provenance regions covering the whole image, in paint order (a parent "
        "before its children, siblings bottom to top; composited top-levels after the window). Each "
        "widget carries nereusPreviewProvenance (retained, longpath-port, proposed, exists-not-wired, "
        "not-implemented) or inherits it from its nearest tagged ancestor; an image root with no tag "
        "is retained. A region is written for the root and for each visible widget whose provenance "
        "differs from its parent's, and for each named panadapter part whatever its provenance: pan-<n>-plot "
        "(widget_class NereusSDR::SpectrumWidget, the pan's SpectrumWidget rect taken from the widget, not "
        "computed: spectrum, frequency scale and waterfall), flag-<letter> (VfoWidget, the VFO flag of that "
        "slice), pan-<n>-<object name or class> (each other widget over the plot: pan toolbar, corner "
        "buttons, status badges, hints; a repeated name gets -2, -3) and pan-separator-<k> (k from 0 over the visible handles) (splitter handles between pans); the plot "
        "minus the regions inside it is the plot area alone. A point's provenance is that of the last region whose visible_rect "
        "contains it. Fields: name (entry id, object name or class), provenance, rect (the widget), "
        "visible_rect (clipped to its ancestors and the image; regions with no visible area are "
        "omitted), type (content type id for container content, else empty), widget_class, parent "
        "(index of the enclosing region, -1 for an image root), depth (0 for a root)");
}

// Sets the whole-image regions of `img`, grabbed from `root`.
void attachRegions(CapturedImage& img, QWidget* root)
{
    QJsonArray regions;
    appendRegions(regions, root, QPoint(0, 0), img.logicalSize);
    img.extra[QStringLiteral("regions")] = regions;
    img.extra[QStringLiteral("region_units")] = regionUnits();
    img.extra[QStringLiteral("regions_contract")] = regionsContract();
}

// The regions of `regions` inside `crop` (logical px of the source image),
// moved to the crop's top-left and scaled by `scale`.
QJsonArray cropRegions(const QJsonArray& regions, const QRect& crop, int scale)
{
    QJsonArray out;
    QHash<int, int> remap;
    const auto place = [&crop, scale](const QRect& r) {
        const QRect m = r.translated(-crop.topLeft());
        return QRect(m.x() * scale, m.y() * scale, m.width() * scale, m.height() * scale);
    };
    for (int i = 0; i < regions.size(); ++i) {
        QJsonObject r = regions.at(i).toObject();
        const QRect visible = regionRectFrom(r.value(QStringLiteral("visible_rect"))).intersected(crop);
        if (visible.isEmpty()) {
            continue;
        }
        remap.insert(i, int(out.size()));
        r[QStringLiteral("rect")] = regionRectJson(place(regionRectFrom(r.value(QStringLiteral("rect")))));
        r[QStringLiteral("visible_rect")] = regionRectJson(place(visible));
        r[QStringLiteral("parent")] = remap.value(r.value(QStringLiteral("parent")).toInt(), -1);
        out.append(r);
    }
    return out;
}

void appendCropImages(SceneContext& ctx, const CapturedImage& full, const QString& name);

// Grabs the whole window and, at the crop profile, its detail crops.
void captureWindow(SceneContext& ctx, const QString& name)
{
    if (ctx.beforeGrab) {
        ctx.beforeGrab(ctx.window);
        pump(kFinalSettleMs);
    }
    CapturedImage full;
    full.name = name;
    full.kind = QStringLiteral("window");
    full.image = grabWidget(ctx.window);
    full.logicalSize = ctx.window->size();
    if (!ctx.geometryDump.isEmpty()) {
        QStringList lines;
        auto* manager = ctx.window->findChild<ContainerManager*>();
        dumpGeometry(manager != nullptr ? manager->panelContainer() : nullptr,
                     QStringLiteral("right-panel"), lines);
        SpectrumWidget* pan = spectrumFor(ctx.window, panIdAt(0));
        dumpGeometry(pan != nullptr ? pan->findChild<SpectrumOverlayPanel*>() : nullptr,
                     QStringLiteral("pan1-toolbar"), lines);
        QFile f(ctx.geometryDump);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            f.write(lines.join(QLatin1Char('\n')).toUtf8());
            f.write("\n");
        } else {
            ctx.notes.append(QStringLiteral("could not write the geometry dump"));
        }
    }
    full.geometry = widgetGeometry(ctx.window);
    full.dpr = full.image.devicePixelRatio();
    if (ctx.crops) {
        SliceModel* sliceA = ctx.model->sliceById(0);
        addCrop(full.crops, QStringLiteral("flag"), ctx.window, flagForSlice(ctx.window, sliceA), full.dpr);
        auto* manager = ctx.window->findChild<ContainerManager*>();
        addCrop(full.crops, QStringLiteral("right-panel"), ctx.window,
                manager != nullptr ? manager->panelContainer() : nullptr, full.dpr);
        addCrop(full.crops, QStringLiteral("top-strip"), ctx.window,
                ctx.window->findChild<TitleBar*>(), full.dpr);
        addCrop(full.crops, QStringLiteral("status-bar"), ctx.window, ctx.window->statusBar(), full.dpr);
        SpectrumWidget* pan1 = spectrumFor(ctx.window, panIdAt(0));
        addCrop(full.crops, QStringLiteral("pan1-toolbar"), ctx.window,
                pan1 != nullptr ? pan1->findChild<SpectrumOverlayPanel*>() : nullptr, full.dpr);
        if (ctx.extraCrops) {
            ctx.extraCrops(ctx, full.crops, full.dpr);
        }
    }
    full.extra[QStringLiteral("applet_fit")] = measureAppletFit(ctx.window);
    full.extra[QStringLiteral("visible_text_fonts")] = visibleTextFonts(ctx.window);
    attachRegions(full, ctx.window);
    ctx.images.append(full);
    appendCropImages(ctx, full, name);
}

// Each crop of `full` as its own image, plus a 3x nearest-neighbour copy.
void appendCropImages(SceneContext& ctx, const CapturedImage& full, const QString& name)
{
    for (const CropRect& crop : full.crops) {
        CapturedImage c;
        c.name = name + QStringLiteral("--") + crop.name;
        c.kind = QStringLiteral("crop");
        c.image = full.image.copy(crop.device);
        c.image.setDevicePixelRatio(full.dpr);
        c.logicalSize = crop.logical.size();
        c.dpr = full.dpr;
        c.crops = {crop};
        c.extra[QStringLiteral("source")] = name;
        if (full.extra.contains(QStringLiteral("regions"))) {
            c.extra[QStringLiteral("regions")] =
                cropRegions(full.extra.value(QStringLiteral("regions")).toArray(), crop.logical, 1);
            c.extra[QStringLiteral("region_units")] = regionUnits();
            c.extra[QStringLiteral("regions_contract")] = regionsContract();
        }
        ctx.images.append(c);
        CapturedImage big;
        big.name = c.name + QStringLiteral("@3x");
        big.kind = QStringLiteral("crop-3x");
        big.image = c.image.scaled(c.image.width() * 3, c.image.height() * 3,
                                   Qt::IgnoreAspectRatio, Qt::FastTransformation);
        big.image.setDevicePixelRatio(full.dpr);
        big.logicalSize = crop.logical.size() * 3;
        big.dpr = full.dpr;
        big.crops = {crop};
        big.extra[QStringLiteral("source")] = name;
        big.extra[QStringLiteral("enlargement")] = QStringLiteral("3x nearest-neighbour");
        if (full.extra.contains(QStringLiteral("regions"))) {
            big.extra[QStringLiteral("regions")] =
                cropRegions(full.extra.value(QStringLiteral("regions")).toArray(), crop.logical, 3);
            big.extra[QStringLiteral("region_units")] = regionUnits();
            big.extra[QStringLiteral("regions_contract")] = regionsContract();
        }
        ctx.images.append(big);
    }
}

void recordStandardNotShown(SceneContext& ctx)
{
    const QString reason = QStringLiteral(
        "needs a device setting that starts discovery or a device connection");
    for (const QString& item : {QStringLiteral("applet:amp (Power Genius / Amplifier)"),
                                QStringLiteral("applet:tuner (Tuner Genius)"),
                                QStringLiteral("applet:RfKit (RF-Kit)")}) {
        ctx.notShown.append(QJsonObject{{QStringLiteral("item"), item},
                                        {QStringLiteral("reason"), reason}});
    }
}

// Waits until the window and every visible top-level stop moving (see
// waitForQuietLayout) and records it in the sidecar when they never did.
void settleLayout(SceneContext& ctx, const QString& stage)
{
    QList<QWidget*> roots{ctx.window};
    for (QWidget* top : QApplication::topLevelWidgets()) {
        if (top != ctx.window && top->isVisible()) {
            roots.append(top);
        }
    }
    const LayoutQuiet quiet = waitForQuietLayout(roots);
    if (!quiet.quiet) {
        ctx.unfrozen.append(QJsonObject{
            {QStringLiteral("item"), QStringLiteral("layout")},
            {QStringLiteral("region"), stage},
            {QStringLiteral("reason"), QStringLiteral("still moving after %1 ms: %2")
                 .arg(quiet.waitedMs).arg(quiet.pending.join(QStringLiteral(", ")))}});
    }
}

// Each posed pan's SpectrumWidget size (empty for a missing pan).
QVector<QSize> panSizes(SceneContext& ctx, const QVector<PanPose>& poses)
{
    QVector<QSize> sizes;
    for (const PanPose& pose : poses) {
        SpectrumWidget* sw = spectrumFor(ctx.window, pose.panId);
        sizes.append(sw != nullptr ? sw->size() : QSize());
    }
    return sizes;
}

// A pan resized after its feed shows a waterfall rebuilt from its history
// at the new size, not the rows as fed; the sidecar says so.
void noteResizedPans(SceneContext& ctx, const QVector<PanPose>& poses, const QVector<QSize>& fedSizes)
{
    const QVector<QSize> now = panSizes(ctx, poses);
    for (int i = 0; i < poses.size() && i < fedSizes.size(); ++i) {
        if (now.at(i) != fedSizes.at(i)) {
            ctx.unfrozen.append(QJsonObject{
                {QStringLiteral("item"), QStringLiteral("%1 spectrum and waterfall").arg(poses.at(i).panId)},
                {QStringLiteral("region"), poses.at(i).panId},
                {QStringLiteral("reason"), QStringLiteral("resized from %1x%2 at the feed to %3x%4 before the grab")
                     .arg(fedSizes.at(i).width()).arg(fedSizes.at(i).height())
                     .arg(now.at(i).width()).arg(now.at(i).height())}});
        }
    }
}

// The common main-window pose: tune, scene hook, freeze, meters, data.
void runWindowScene(SceneContext& ctx, const QVector<PanPose>& poses, int activePanIndex,
                    const std::function<void(SceneContext&)>& beforeFreeze = {},
                    const std::function<void(SceneContext&)>& afterFeed = {})
{
    recordStandardNotShown(ctx);
    if (!tuneSlices(ctx, poses, activePanIndex)) {
        return;
    }
    if (beforeFreeze) {
        beforeFreeze(ctx);
        if (!ctx.failure.isEmpty()) {
            return;
        }
        pump(kSettleMs);
    }
    // The freeze stops every timer and the feed writes each pan at its
    // size, so both wait for the layout to stop moving. After a fixed wait
    // alone, a loaded machine could freeze SpectrumWidget's waterfall
    // history timer mid-resize and feed a pan at a size it then left.
    settleLayout(ctx, QStringLiteral("before the freeze"));
    freeze(ctx);
    poseMeters(ctx, poses, activePanIndex);
    feedSpectra(ctx, poses);
    if (!ctx.failure.isEmpty()) {
        return;
    }
    const QVector<QSize> fedSizes = panSizes(ctx, poses);
    if (afterFeed) {
        afterFeed(ctx);
    }
    waitForNeedle(ctx);
    pump(kFinalSettleMs);
    settleLayout(ctx, QStringLiteral("before the grab"));
    noteResizedPans(ctx, poses, fedSizes);
}

ContainerWidget* mainPanel(SceneContext& ctx)
{
    auto* manager = ctx.window->findChild<ContainerManager*>();
    return manager != nullptr ? manager->panelContainer() : nullptr;
}

void sceneHeaderMode(SceneContext& ctx, HeaderMode mode, const QString& name)
{
    runWindowScene(ctx, twoBandPoses(), 0, [mode](SceneContext& c) {
        ContainerWidget* panel = mainPanel(c);
        if (panel == nullptr) {
            c.failure = QStringLiteral("no Main Panel container");
            return;
        }
        // The context menu's Header > Always / Reveal / Hidden path.
        emit panel->headerModeRequested(mode);
    });
    if (ctx.failure.isEmpty()) {
        captureWindow(ctx, name);
    }
}

// ---- proposed workspace headers -------------------------------------------

// The Main Panel's top band, where the group and first entry headers sit.
constexpr int kPanelTopCropPx = 120;

void addPanelTopCrop(SceneContext& ctx, QVector<CropRect>& crops, double dpr)
{
    ContainerWidget* panel = mainPanel(ctx);
    if (panel == nullptr || !panel->isVisible() || !ctx.window->isAncestorOf(panel)) {
        return;
    }
    const QRect logical(panel->mapTo(ctx.window, QPoint(0, 0)),
                        QSize(panel->width(), qMin(panel->height(), kPanelTopCropPx)));
    const QRect clipped = logical.intersected(QRect(QPoint(0, 0), ctx.window->size()));
    if (!clipped.isEmpty()) {
        crops.append({QStringLiteral("panel-top"), clipped, deviceRect(clipped, dpr)});
    }
}

QJsonObject proposedHeaderValues()
{
    return QJsonObject{
        {QStringLiteral("group_header_height_px"), WorkspaceGroupHeader::kHeight},
        {QStringLiteral("group_header_background"), QStringLiteral(
            "titleBarStyle(): #26262b / #1c1c20 / #141417, bottom #0d0d0f, top rgba(255,255,255,18), top radius 9")},
        {QStringLiteral("group_header_margins"), QStringLiteral("2,0,4,0, spacing 4")},
        {QStringLiteral("stripe"), QStringLiteral("3 px #d8a55f")},
        {QStringLiteral("grip"), QStringLiteral("U+22EE U+22EE, 11 px #7e7e85 (hover #dcdce1), hit 14x18")},
        {QStringLiteral("title"), QStringLiteral("capsFont 9 px DemiBold #828288")},
        {QStringLiteral("source_chip"), QStringLiteral(
            "mono 9 px slice colour on #16161c, 1 px slice colour at 60%, radius 4, height 13")},
        {QStringLiteral("buttons"), QStringLiteral(
            "16x14, glyph #7e7e85, hover #26262b radius 7: eye (drawn, open when kept shown, crossed out "
            "when hidden), pop out, options, hide")},
        {QStringLiteral("plate"), QStringLiteral("glassPanelFill #17171b..#0e0e10, 1 px #1f1f23, radius 10")},
        {QStringLiteral("entry_header_height_px"), WorkspaceEntryHeader::kHeight},
        {QStringLiteral("entry_header"), QStringLiteral(
            "transparent, bottom hairline #1f1f23, grip 10 px #58585e (hover #a6a6ac), "
            "title capsFont 9 #7e7e85, options 14x12")},
        {QStringLiteral("content_left_gutter_px"), 0},
        {QStringLiteral("hidden"), QStringLiteral(
            "zero extent; drawn over the top of the content only while arranging")},
        {QStringLiteral("arrange"), QStringLiteral(
            "header bottom border #4a7ba8; 1 px dashed #4a7ba8 at 60% outline; 12x12 dot corner #7e7e85; "
            "4 px inside edge zones; Ctrl+Shift+A; not saved")},
    };
}

// One proposed header scene: the two-band data with the workspace look,
// the Main Panel in `mode`, `arrange` (before the data is fed), then `pose`
// (after it).
void sceneProposedHeader(SceneContext& ctx, HeaderMode mode, const QString& name,
                         const std::function<void(SceneContext&, ContainerWidget*)>& pose = {},
                         const std::function<void(SceneContext&)>& arrange = {})
{
    ctx.provenance = QStringLiteral("proposed");
    ctx.proposedValues = proposedHeaderValues();
    ctx.extraCrops = addPanelTopCrop;
    runWindowScene(ctx, twoBandPoses(), 0, [mode, arrange](SceneContext& c) {
        ContainerWidget* panel = mainPanel(c);
        if (panel == nullptr) {
            c.failure = QStringLiteral("no Main Panel container");
            return;
        }
        // The eye button's and the body menu's Keep Header Showing path.
        emit panel->headerModeRequested(mode);
        if (arrange) {
            arrange(c);
        }
    }, [pose](SceneContext& c) {
        if (pose) {
            pose(c, mainPanel(c));
        }
    });
    if (!ctx.failure.isEmpty()) {
        return;
    }
    if (ContainerWidget* panel = mainPanel(ctx)) {
        ctx.notes.append(QStringLiteral("Main Panel header extent %1 px, header visible %2, arrange %3")
                             .arg(panel->headerExtent())
                             .arg(panel->chromeVisible() ? QStringLiteral("yes") : QStringLiteral("no"))
                             .arg(panel->arrangeActive() ? QStringLiteral("on") : QStringLiteral("off")));
    }
    // Where each drawn header's eye button is, in window logical px.
    if (auto* manager = ctx.window->findChild<ContainerManager*>()) {
        for (ContainerWidget* container : manager->allContainers()) {
            WorkspaceGroupHeader* header = container->workspaceHeader();
            if (header == nullptr || !header->isVisible() || !ctx.window->isAncestorOf(header)) {
                continue;
            }
            QAbstractButton* eye = header->keepShownButton();
            const QPoint at = eye->mapTo(ctx.window, QPoint(0, 0));
            ctx.notes.append(QStringLiteral("eye button of \"%1\": %2,%3 %4x%5, %6")
                                 .arg(header->title())
                                 .arg(at.x()).arg(at.y()).arg(eye->width()).arg(eye->height())
                                 .arg(header->keepShown() ? QStringLiteral("open (header kept shown)")
                                                          : QStringLiteral("crossed out (header hidden)")));
        }
    }
    captureWindow(ctx, name);
}

// ---- dialogs ---------------------------------------------------------------

template <typename T>
T* visibleTopLevel()
{
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (auto* t = qobject_cast<T*>(w); t != nullptr && t->isVisible()) {
            return t;
        }
    }
    return nullptr;
}

// ---- proposed four-edge workspace -----------------------------------------

QString slug(const QString& text);

struct EdgeEntry {
    QString typeId;
    // FreeCanvas placement in container pixels; empty for a stack.
    QRectF rect;
    // A legacy record for the item's own settings (button columns).
    QString legacyRecord;
    // Source context keys merged into the entry's context (rxSource).
    QJsonObject context;
    // Keys merged into the entry's config (a Longpath binding).
    QJsonObject config;
};

// One row of buttons: every button in its own column, at `heightRatio`,
// as Container Settings sets them for a short strip.
template <typename Item>
QString singleRowRecord(float heightRatio)
{
    Item item;
    item.setColumns(qMax(1, item.buttonCount()));
    item.setHeightRatio(heightRatio);
    return item.serialize();
}

// Adds a container to the committed workspace, as the Containers menu and
// the settings dialog do: a draft with registry entries, committed.
ContainerDocument makeSceneContainer(ContainerManager* manager, const QString& name, DockEdge edge,
                                     ContentLayout layout, const QVector<EdgeEntry>& entries)
{
    ContainerDocument d;
    d.id = QStringLiteral("preview-") + slug(name);
    d.name = name;
    d.layout = layout;
    d.dockMode = DockMode::PanelDocked;
    d.edge = edge;
    d.header = HeaderMode::Shown;
    for (const EdgeEntry& e : entries) {
        ContentEntry entry = manager->contentRegistry()->makeEntry(e.typeId);
        entry.id = d.id + QStringLiteral("-") + slug(e.typeId);
        // A second entry of the same type gets a numbered id.
        for (int n = 2; std::any_of(d.contents.cbegin(), d.contents.cend(),
                                    [&entry](const ContentEntry& c) { return c.id == entry.id; }); ++n) {
            entry.id = d.id + QStringLiteral("-") + slug(e.typeId) + QStringLiteral("-%1").arg(n);
        }
        for (auto it = e.config.constBegin(); it != e.config.constEnd(); ++it) {
            entry.config.insert(it.key(), it.value());
        }
        if (!e.legacyRecord.isEmpty()) {
            entry.config.insert(QStringLiteral("legacyRecord"), e.legacyRecord);
        }
        for (auto it = e.context.constBegin(); it != e.context.constEnd(); ++it) {
            entry.context.insert(it.key(), it.value());
        }
        if (!e.rect.isNull()) {
            entry.extensions[QStringLiteral("freeCanvasRect")] =
                QJsonArray{e.rect.x(), e.rect.y(), e.rect.width(), e.rect.height()};
        }
        d.contents.append(entry);
    }
    return d;
}

QString addSceneContainer(SceneContext& ctx, const QString& name, DockEdge edge,
                          ContentLayout layout, const QVector<EdgeEntry>& entries)
{
    auto* manager = ctx.window->findChild<ContainerManager*>();
    if (manager == nullptr || manager->workspaceStore() == nullptr || manager->contentRegistry() == nullptr) {
        ctx.failure = QStringLiteral("no container workspace");
        return {};
    }
    WorkspaceDocument draft = manager->workspaceStore()->snapshot();
    const ContainerDocument d = makeSceneContainer(manager, name, edge, layout, entries);
    draft.containers.append(d);
    if (manager->commitWorkspace(draft, draft.revision).status != CommitStatus::Saved) {
        ctx.failure = QStringLiteral("could not add the %1 container").arg(name);
        return {};
    }
    return d.id;
}

// Free canvas rows for a 112 px strip: a meter view is at least 49 px
// tall, and the canvas keeps 20 px above and below, so the 82 px under an
// Always header scrolls by 7 px (recorded in the Task 4 report).
constexpr double kEndRowHeightPx = 49.0;

// "Meters": the Thetis S-Meter preset and the Power/SWR face side by side.
QVector<EdgeEntry> metersEntries()
{
    return {{QStringLiteral("meter.sMeter"), QRectF(0, 0, 420, kEndRowHeightPx)},
            {QStringLiteral("meter.powerSwr"), QRectF(440, 0, 420, kEndRowHeightPx)}};
}

QString addMetersContainer(SceneContext& ctx, DockEdge edge)
{
    return addSceneContainer(ctx, QStringLiteral("Meters"), edge, ContentLayout::FreeCanvas, metersEntries());
}

// "Band": the band and mode button boxes side by side, one row each.
QVector<EdgeEntry> bandEntries()
{
    return {{QStringLiteral("BANDBTNS"), QRectF(0, 0, 840, kEndRowHeightPx),
             singleRowRecord<BandButtonItem>(0.4f)},
            {QStringLiteral("MODEBTNS"), QRectF(860, 0, 520, kEndRowHeightPx),
             singleRowRecord<ModeButtonItem>(0.4f)}};
}

QString addBandContainer(SceneContext& ctx, DockEdge edge)
{
    return addSceneContainer(ctx, QStringLiteral("Band"), edge, ContentLayout::FreeCanvas, bandEntries());
}

// "Levels": two bar meters stacked, a container that reads well on a side.
QVector<EdgeEntry> levelsEntries()
{
    return {{QStringLiteral("meter.signalMaxBin"), {}}, {QStringLiteral("meter.comp"), {}}};
}

QString addLevelsContainer(SceneContext& ctx, DockEdge edge)
{
    return addSceneContainer(ctx, QStringLiteral("Levels"), edge, ContentLayout::VerticalStack, levelsEntries());
}

QJsonObject proposedEdgeValues()
{
    return QJsonObject{
        {QStringLiteral("layout"), QStringLiteral(
            "vertical splitter [top | middle | bottom], middle [left | spectrum | right]; "
            "top and bottom span the width; an empty strip takes no space")},
        {QStringLiteral("strip_padding_px"), EdgeDockStrip::kPaddingPx},
        {QStringLiteral("strip_padding_right_px"), EdgeDockStrip::kRightPaddingPx},
        {QStringLiteral("strip_gap_px"), EdgeDockStrip::kGapPx},
        {QStringLiteral("strip_background"), QStringLiteral("app-bg")},
        {QStringLiteral("handle"), QStringLiteral(
            "6 px (3 px on the spectrum side of the right strip, today's splitter handle), "
            "app-bg, 1 px centre line border role, accent role under the pointer")},
        {QStringLiteral("thickness_left_right_px"), EdgeDockStrip::kSideThicknessPx},
        {QStringLiteral("thickness_top_bottom_px"), EdgeDockStrip::kEndThicknessPx},
        {QStringLiteral("thickness_top_bottom_default"), QStringLiteral(
            "max(112, tallest container's preferred height incl. header + 2 x padding); "
            "scrolls only when made smaller")},
        {QStringLiteral("thickness_side_minimum"), QStringLiteral(
            "widest container minimum + 2 x padding")},
        {QStringLiteral("thickness_right_first_run"), QStringLiteral(
            "today's Main Panel width (the 3 px-handle splitter's 1024/256 split, "
            "the panel raised to its minimum), no padding, until dragged or saved")},
        {QStringLiteral("free_and_floating_background"), QStringLiteral(
            "opaque glass plate (panel-top to panel-bottom); floating shell panel role")},
        {QStringLiteral("target_band"), QStringLiteral("6 px at each edge, accent at 35%")},
        {QStringLiteral("drop_preview"), QStringLiteral(
            "accent at 22% fill, 1 px #3d78b0, radius 6; 28 px between containers")},
        {QStringLiteral("grid"), QStringLiteral("8 px, off by default, Alt places freely")},
    };
}

void addStripCrops(SceneContext& ctx, QVector<CropRect>& crops, double dpr)
{
    auto* area = ctx.window->findChild<EdgeDockArea*>();
    if (area == nullptr) {
        return;
    }
    addCrop(crops, QStringLiteral("strip-left"), ctx.window, area->strip(DockEdge::Left), dpr);
    addCrop(crops, QStringLiteral("strip-top"), ctx.window, area->strip(DockEdge::Top), dpr);
    addCrop(crops, QStringLiteral("strip-bottom"), ctx.window, area->strip(DockEdge::Bottom), dpr);
}

void noteStrips(SceneContext& ctx)
{
    auto* area = ctx.window->findChild<EdgeDockArea*>();
    if (area == nullptr) {
        ctx.notes.append(QStringLiteral("no EdgeDockArea: the workspace preview switch is off"));
        return;
    }
    const QPair<DockEdge, QString> edges[] = {{DockEdge::Left, QStringLiteral("left")},
                                              {DockEdge::Right, QStringLiteral("right")},
                                              {DockEdge::Top, QStringLiteral("top")},
                                              {DockEdge::Bottom, QStringLiteral("bottom")}};
    for (const auto& [edge, name] : edges) {
        EdgeDockStrip* strip = area->strip(edge);
        QStringList titles;
        for (QWidget* w : strip->containers()) {
            if (auto* c = qobject_cast<ContainerWidget*>(w); c != nullptr && !c->isHidden()) {
                QString scroll;
                if (auto* area = c->contentHolder()->findChild<QScrollArea*>()) {
                    scroll = QStringLiteral(", viewport %1x%2, body min %3x%4")
                                 .arg(area->viewport()->width())
                                 .arg(area->viewport()->height())
                                 .arg(area->widget() ? area->widget()->minimumSizeHint().width() : -1)
                                 .arg(area->widget() ? area->widget()->minimumSizeHint().height() : -1);
                }
                titles.append(QStringLiteral("%1 (container %5x%6, minimum width %7, content %2x%3%4)")
                                  .arg(c->notes().isEmpty() ? c->id() : c->notes())
                                  .arg(c->contentHolder()->width())
                                  .arg(c->contentHolder()->height())
                                  .arg(scroll)
                                  .arg(c->width()).arg(c->height())
                                  .arg(qMax(c->minimumWidth(), c->minimumSizeHint().width())));
            }
        }
        ctx.notes.append(QStringLiteral("%1 strip: %2, %3x%4 [%5]")
                             .arg(name, strip->isVisible() ? QStringLiteral("shown") : QStringLiteral("empty"))
                             .arg(strip->width()).arg(strip->height())
                             .arg(titles.join(QStringLiteral(", "))));
    }
}

// One proposed edge scene: the two-band data with the workspace look, every
// container's header in `header` (Shown unless a scene says otherwise),
// `arrange` before the data and `pose` after.
void sceneProposedEdges(SceneContext& ctx, const QString& name,
                        const std::function<void(SceneContext&)>& arrange,
                        const std::function<void(SceneContext&)>& pose = {},
                        HeaderMode header = HeaderMode::Shown)
{
    ctx.provenance = QStringLiteral("proposed");
    ctx.proposedValues = proposedEdgeValues();
    ctx.extraCrops = addStripCrops;
    runWindowScene(ctx, twoBandPoses(), 0, [arrange, header](SceneContext& c) {
        if (mainPanel(c) == nullptr || c.window->findChild<EdgeDockArea*>() == nullptr) {
            c.failure = QStringLiteral("no Main Panel container or edge area");
            return;
        }
        if (arrange) {
            arrange(c);
        }
        if (!c.failure.isEmpty()) {
            return;
        }
        auto* manager = c.window->findChild<ContainerManager*>();
        for (ContainerWidget* container : manager->allContainers()) {
            emit container->headerModeRequested(header);
        }
    }, [pose](SceneContext& c) {
        if (pose) {
            pose(c);
        }
    });
    if (!ctx.failure.isEmpty()) {
        return;
    }
    noteStrips(ctx);
    captureWindow(ctx, name);
}

// The visible window a floating container is in, or nullptr.
QWidget* floatingWindowOf(ContainerManager* manager, const QString& id)
{
    ContainerWidget* container = manager != nullptr ? manager->container(id) : nullptr;
    if (container == nullptr || !container->isFloating()) {
        return nullptr;
    }
    QWidget* top = container->window();
    return top->isVisible() ? top : nullptr;
}

// ---- Longpath instruments (workspace preview) ------------------------------

// The fixed instrument values the sheet shows.
constexpr double kSheetFrequencyHz = 14225000.0;
constexpr double kSheetOtherHz = 7185000.0;
constexpr double kSheetSignalDbm = -73.0;
constexpr double kSheetSignalPeakDbm = -68.0;
constexpr double kSheetTxWatts = 85.0;
constexpr double kSheetSwr = 1.25;
// A realistic ALC level just under Thetis's red zone (which starts at
// 0 dB). Fixed: the sheet has no transmitter.
constexpr double kSheetAlcDb = -3.0;
constexpr int kSheetFilterLowHz = 150;
constexpr int kSheetFilterHighHz = 2850;
constexpr int kSheetTracePoints = 160;
constexpr int kSheetMarginPx = 16;
constexpr int kSheetGapPx = 16;
constexpr int kSheetLabelPx = 18;
constexpr int kSheetRowGapPx = 14;
// The needle hub crop: this far either side of the pivot column, and
// this far above and below the footer's top edge.
constexpr int kHubHalfWidthPx = 60;
constexpr int kHubAbovePx = 60;
constexpr int kHubBelowPx = 12;

QJsonArray rectJson(const QRect& r)
{
    return QJsonArray{r.x(), r.y(), r.width(), r.height()};
}

// A fixed passband trace over the pane's 10 kHz span, centred on the VFO:
// a deterministic noise floor near -118 dBm, a USB voice hump from +200 to
// +2700 Hz, and a carrier at +1200 Hz.
QVector<float> sheetTrace(int spanHz)
{
    QVector<float> trace(kSheetTracePoints);
    for (int i = 0; i < kSheetTracePoints; ++i) {
        const double offsetHz = -spanHz / 2.0 + spanHz * i / (kSheetTracePoints - 1.0);
        double dbm = -118.0 + 2.5 * std::sin(i * 1.7) + 1.5 * std::sin(i * 0.37);
        if (offsetHz >= 200.0 && offsetHz <= 2700.0) {
            dbm = std::max(dbm, -86.0 + 6.0 * std::sin(offsetHz / 350.0) + 2.0 * std::sin(i * 2.3));
        }
        if (std::abs(offsetHz - 1200.0) < spanHz / double(kSheetTracePoints)) {
            dbm = -62.0;
        }
        trace[i] = static_cast<float>(dbm);
    }
    return trace;
}

QSize minimumOf(const QWidget* w)
{
    return w->minimumSizeHint().expandedTo(w->minimumSize());
}

struct SheetRow {
    QString key;        // crop and region name stem
    QString title;
    std::function<QWidget*(QWidget* parent)> make;
    int column{0};
};

// instruments-sheet: every ported widget at its preferred and minimum
// size, on its own top-level window (not the MainWindow).
void sceneInstrumentsSheet(SceneContext& ctx)
{
    ctx.provenance = QStringLiteral("longpath-port");
    auto frequency = [](FrequencyInstrument::Form form) {
        return [form](QWidget* parent) -> QWidget* {
            auto* f = new FrequencyInstrument(parent);
            f->setForm(form);
            f->setFrequency(kSheetFrequencyHz);
            f->setOtherFrequency(kSheetOtherHz);
            f->setActiveIsThis(true);
            return f;
        };
    };
    const QVector<SheetRow> rows = {
        {QStringLiteral("frequency"), QStringLiteral("FrequencyInstrument, NumberOnly, 14.225.000 (other 7.185.000)"),
         frequency(FrequencyInstrument::Form::NumberOnly), 0},
        {QStringLiteral("frequency-bandstrip"), QStringLiteral("FrequencyInstrument, BandStrip"),
         frequency(FrequencyInstrument::Form::BandStrip), 0},
        {QStringLiteral("frequency-flatarc"), QStringLiteral("FrequencyInstrument, FlatArc"),
         frequency(FrequencyInstrument::Form::FlatArc), 0},
        {QStringLiteral("filter"), QStringLiteral("BandwidthFilterPane, RX A USB 150..2850, fixed trace"),
         [](QWidget* parent) -> QWidget* {
             auto* p = new BandwidthFilterPane(parent);
             p->setLabel(QStringLiteral("RX A"));
             p->setAccent(QColor::fromRgb(ControlRanges::sliceColour(0)));
             p->setHasFrequency(true);
             p->setVfoFrequency(kSheetFrequencyHz);
             p->setFilter(kSheetFilterLowHz, kSheetFilterHighHz);
             p->setTrace(sheetTrace(p->spanHz()));
             return p;
         }, 0},
        {QStringLiteral("bar-swr"), QStringLiteral("BarInstrument, TX: SWR 1.25"),
         [](QWidget* parent) -> QWidget* {
             auto* b = new BarInstrument(parent);
             b->setPrimary(MeterBinding::TxSwr);
             b->onReading(MeterBinding::TxSwr, kSheetSwr);
             return b;
         }, 0},
        {QStringLiteral("bar-alc"), QStringLiteral("BarInstrument, TX: ALC -3 dB (fixed sheet value)"),
         [](QWidget* parent) -> QWidget* {
             auto* b = new BarInstrument(parent);
             // TX: ALC carries Thetis's ALC scale (ReadingSource.cpp). A
             // refusal would leave the unbound face, which must not stand
             // in for ALC, so the sidecar records it.
             b->setProperty("sheetBindRefused", !b->setPrimary(MeterBinding::TxAlc));
             b->onReading(MeterBinding::TxAlc, kSheetAlcDb);
             return b;
         }, 0},
        {QStringLiteral("needle-signal"), QStringLiteral("NeedleInstrument, RX: Signal Peak -73 dBm, peak -68"),
         [](QWidget* parent) -> QWidget* {
             auto* n = new NeedleInstrument(parent);
             n->setPrimary(MeterBinding::SignalPeak);
             n->onReading(MeterBinding::SignalPeak, kSheetSignalPeakDbm);
             n->onReading(MeterBinding::SignalPeak, kSheetSignalDbm);
             return n;
         }, 1},
        {QStringLiteral("needle-txpower"), QStringLiteral("NeedleInstrument, TX: Forward Power 85 W"),
         [](QWidget* parent) -> QWidget* {
             auto* n = new NeedleInstrument(parent);
             n->setPrimary(MeterBinding::TxPower);
             n->onReading(MeterBinding::TxPower, kSheetTxWatts);
             return n;
         }, 1},
    };

    auto sheet = std::make_unique<QWidget>();
    sheet->setObjectName(QStringLiteral("instrumentsSheet"));
    sheet->setWindowTitle(QStringLiteral("Longpath instruments"));
    sheet->setAttribute(Qt::WA_DontShowOnScreen, true);
    sheet->setAutoFillBackground(true);
    struct Placed {
        const SheetRow* row;
        QLabel* label;
        QWidget* preferred;
        QWidget* minimum;
    };
    QVector<Placed> placed;
    for (const SheetRow& row : rows) {
        auto* label = new QLabel(sheet.get());
        label->setStyleSheet(QStringLiteral("color: #828288; font-size: 10px;"));
        placed.append({&row, label, row.make(sheet.get()), row.make(sheet.get())});
    }
    sheet->resize(400, 400);
    sheet->show();
    pump(kFinalSettleMs);

    // Sizes come from the shown widgets (FrequencyInstrument's minimum
    // counts its A/B row only when it is shown).
    int columnX[2] = {kSheetMarginPx, kSheetMarginPx};
    int columnY[2] = {kSheetMarginPx, kSheetMarginPx};
    int column0Width = 0;
    for (const Placed& p : placed) {
        const QSize pref = p.preferred->sizeHint();
        const QSize min = minimumOf(p.minimum);
        column0Width = p.row->column == 0 ? qMax(column0Width, pref.width() + kSheetGapPx + min.width())
                                          : column0Width;
    }
    columnX[1] = kSheetMarginPx + column0Width + 2 * kSheetGapPx;
    int right = 0;
    int bottom = 0;
    for (Placed& p : placed) {
        const int c = p.row->column;
        const QSize pref = p.preferred->sizeHint();
        const QSize min = minimumOf(p.minimum);
        p.label->setText(QStringLiteral("%1   preferred %2x%3 | minimum %4x%5")
                             .arg(p.row->title).arg(pref.width()).arg(pref.height())
                             .arg(min.width()).arg(min.height()));
        p.label->adjustSize();
        p.label->move(columnX[c], columnY[c]);
        const int y = columnY[c] + kSheetLabelPx;
        p.preferred->setGeometry(QRect(QPoint(columnX[c], y), pref));
        p.minimum->setGeometry(QRect(QPoint(columnX[c] + pref.width() + kSheetGapPx, y), min));
        columnY[c] = y + qMax(pref.height(), min.height()) + kSheetRowGapPx;
        right = qMax(right, qMax(p.minimum->geometry().right(), p.label->geometry().right()));
        bottom = qMax(bottom, columnY[c]);
    }
    sheet->resize(right + kSheetMarginPx, bottom + kSheetMarginPx - kSheetRowGapPx);
    pump(kFinalSettleMs);
    for (const QJsonValue& v : stopTimersUnder(sheet.get())) {
        ctx.stoppedTimers.append(v);
    }

    CapturedImage img;
    img.name = QStringLiteral("instruments-sheet");
    img.kind = QStringLiteral("sheet");
    img.image = grabWidget(sheet.get());
    img.logicalSize = sheet->size();
    img.dpr = img.image.devicePixelRatio();
    QJsonArray regions;
    for (const Placed& p : placed) {
        for (const auto& [w, kind] : {std::pair<QWidget*, QString>{p.preferred, QStringLiteral("preferred")},
                                      std::pair<QWidget*, QString>{p.minimum, QStringLiteral("minimum")}}) {
            regions.append(QJsonObject{
                {QStringLiteral("name"), p.row->key + QLatin1Char('-') + kind},
                {QStringLiteral("provenance"), QStringLiteral("longpath-port")},
                {QStringLiteral("class"), QString::fromLatin1(w->metaObject()->className())},
                {QStringLiteral("size"), kind},
                {QStringLiteral("rect"), rectJson(w->geometry())}});
        }
        regions.append(QJsonObject{
            {QStringLiteral("name"), p.row->key + QStringLiteral("-label")},
            {QStringLiteral("provenance"), QStringLiteral("proposed")},
            {QStringLiteral("rect"), rectJson(p.label->geometry())}});
        if (p.minimum->property("sheetBindRefused").toBool()) {
            ctx.notes.append(QStringLiteral("%1: BarInstrument::setPrimary(TxAlc) refused it; "
                                            "the bar shows its unbound face, not ALC").arg(p.row->key));
        }
    }
    // The per-instrument list (with its size column) predates the
    // whole-image regions.
    img.extra[QStringLiteral("sheet_regions")] = regions;
    tagProvenance(sheet.get(), provProposed());
    attachRegions(img, sheet.get());
    img.extra[QStringLiteral("window")] = QStringLiteral("its own top-level QWidget, not the MainWindow");
    if (ctx.crops) {
        const auto cropOf = [&](const QString& name, const QRect& logical) {
            const QRect clipped = logical.intersected(QRect(QPoint(0, 0), sheet->size()));
            if (!clipped.isEmpty()) {
                img.crops.append({name, clipped, deviceRect(clipped, img.dpr)});
            }
        };
        for (const Placed& p : placed) {
            if (p.row->key == QLatin1String("frequency")) {
                cropOf(QStringLiteral("frequency-digits"), p.preferred->geometry());
                cropOf(QStringLiteral("frequency-digits-min"), p.minimum->geometry());
            } else if (p.row->key == QLatin1String("needle-signal")) {
                for (const auto& [w, suffix] : {std::pair<QWidget*, QString>{p.preferred, QString()},
                                                std::pair<QWidget*, QString>{p.minimum, QStringLiteral("-min")}}) {
                    auto* footer = w->findChild<InstrumentFooter*>();
                    const int footerTop = footer != nullptr ? footer->y() : w->height();
                    const QRect hub(w->x() + w->width() / 2 - kHubHalfWidthPx,
                                    w->y() + footerTop - kHubAbovePx,
                                    2 * kHubHalfWidthPx, kHubAbovePx + kHubBelowPx);
                    cropOf(QStringLiteral("needle-hub") + suffix, hub);
                }
            }
        }
    }
    ctx.images.append(img);
    appendCropImages(ctx, img, img.name);
    ctx.notes.append(QStringLiteral("sheet %1x%2; instruments built directly with fixed values, "
                                    "no MeterPoller or slice binding")
                         .arg(sheet->width()).arg(sheet->height()));
    ctx.notes.append(QStringLiteral("bar-alc: TX: ALC shows a fixed sheet value of %1 dB on Thetis's "
                                    "ALC scale (-30..+12 dB, red from 0 dB); not a live reading")
                         .arg(kSheetAlcDb));
}

// "Station": Longpath instruments mixed with Thetis meter faces, slice A.
QVector<EdgeEntry> stationEntries()
{
    const QJsonObject sliceA{{QStringLiteral("rxSource"), 1}};
    return {{QString::fromLatin1(LongpathContent::kFrequencyType), {}, {}, sliceA},
            {QString::fromLatin1(LongpathContent::kFilterType), {}, {}, sliceA},
            {QStringLiteral("meter.sMeter"), {}, {}, sliceA},
            {QString::fromLatin1(LongpathContent::kNeedleType), {}, {}, sliceA},
            {QStringLiteral("meter.crossNeedle"), {}, {}, sliceA},
            {QString::fromLatin1(LongpathContent::kBarType), {}, {}, sliceA}};
}

QString addStationContainer(SceneContext& ctx, DockEdge edge)
{
    return addSceneContainer(ctx, QStringLiteral("Station"), edge, ContentLayout::VerticalStack, stationEntries());
}

// After the data is fed (the poller and timers are stopped): one reading
// through MeterPoller::readingUpdated for each Longpath needle, as the
// poller would send for slice A, and one trace refresh for each filter
// pane from the fed pan.
void poseStation(SceneContext& ctx, const QString& id)
{
    auto* manager = ctx.window->findChild<ContainerManager*>();
    ContainerContentHost* host = manager != nullptr ? manager->contentHost(id) : nullptr;
    auto* poller = ctx.window->findChild<MeterPoller*>();
    if (host == nullptr || poller == nullptr) {
        ctx.failure = QStringLiteral("no Station host or MeterPoller");
        return;
    }
    for (NeedleInstrument* needle : host->findChildren<NeedleInstrument*>()) {
        const QJsonObject context = needle->property("containerSourceContext").toJsonObject();
        emit poller->readingUpdated(context, MeterBinding::SignalPeak, kSheetSignalPeakDbm);
        emit poller->readingUpdated(context, MeterBinding::SignalPeak, kSheetSignalDbm);
        ctx.notes.append(QStringLiteral("Longpath needle: readingUpdated SignalPeak -68 then -73 dBm"));
    }
    for (BandwidthFilterPane* pane : host->findChildren<BandwidthFilterPane*>()) {
        for (QTimer* timer : pane->findChildren<QTimer*>()) {
            QMetaObject::invokeMethod(timer, "timeout");
        }
        ctx.notes.append(QStringLiteral("Longpath filter pane: one trace refresh from the fed pan"));
    }
    ctx.notes.append(QStringLiteral("Longpath bar (TX: SWR): no reading in receive, shows its dash"));
}

QString regionProvenance(const QString& typeId)
{
    return typeId.startsWith(QLatin1String("longpath.")) ? QStringLiteral("longpath-port")
                                                        : QStringLiteral("retained");
}

// The sidecar regions of the Station container: each entry's widget rect,
// the headers and the strips, in window coordinates.
QJsonArray stationRegions(SceneContext& ctx, const QString& id)
{
    QJsonArray regions;
    auto* manager = ctx.window->findChild<ContainerManager*>();
    ContainerWidget* container = manager->container(id);
    ContainerContentHost* host = manager->contentHost(id);
    auto windowRect = [&ctx](QWidget* w, const QRect& local) {
        return QRect(w->mapTo(ctx.window, local.topLeft()), local.size());
    };
    QRect viewport;
    if (auto* area = container->contentHolder()->findChild<QScrollArea*>()) {
        viewport = windowRect(area->viewport(), area->viewport()->rect());
    } else {
        viewport = windowRect(host, host->rect());
    }
    const ContainerDocument doc = host->captureDocument();
    for (const ContentEntry& entry : doc.contents) {
        const QRect rect = windowRect(host, host->entryBoundary(entry.id));
        QJsonObject r{{QStringLiteral("name"), entry.id},
                      {QStringLiteral("type"), entry.typeId},
                      {QStringLiteral("provenance"), regionProvenance(entry.typeId)},
                      {QStringLiteral("rect"), rectJson(rect)},
                      {QStringLiteral("visible_rect"), rectJson(rect.intersected(viewport))}};
        for (const auto& row : host->entryRows()) {
            if (row.entryId == entry.id && row.widget != nullptr) {
                r.insert(QStringLiteral("widget_class"),
                         QString::fromLatin1(row.widget->metaObject()->className()));
            }
        }
        regions.append(r);
        if (WorkspaceEntryHeader* header = host->entryHeader(entry.id); header != nullptr && header->isVisible()) {
            regions.append(QJsonObject{{QStringLiteral("name"), entry.id + QStringLiteral("-header")},
                                       {QStringLiteral("provenance"), QStringLiteral("proposed")},
                                       {QStringLiteral("rect"), rectJson(windowRect(header, header->rect()))}});
        }
    }
    if (WorkspaceGroupHeader* header = container->workspaceHeader(); header != nullptr && header->isVisible()) {
        regions.append(QJsonObject{{QStringLiteral("name"), id + QStringLiteral("-group-header")},
                                   {QStringLiteral("provenance"), QStringLiteral("proposed")},
                                   {QStringLiteral("rect"), rectJson(windowRect(header, header->rect()))}});
    }
    regions.append(QJsonObject{{QStringLiteral("name"), id + QStringLiteral("-viewport")},
                               {QStringLiteral("provenance"), QStringLiteral("proposed")},
                               {QStringLiteral("rect"), rectJson(viewport)}});
    if (auto* area = ctx.window->findChild<EdgeDockArea*>()) {
        const QPair<DockEdge, QString> edges[] = {{DockEdge::Left, QStringLiteral("strip-left")},
                                                  {DockEdge::Right, QStringLiteral("strip-right")}};
        for (const auto& [edge, name] : edges) {
            if (EdgeDockStrip* strip = area->strip(edge); strip != nullptr && strip->isVisible()) {
                regions.append(QJsonObject{{QStringLiteral("name"), name},
                                           {QStringLiteral("provenance"), QStringLiteral("proposed")},
                                           {QStringLiteral("rect"), rectJson(windowRect(strip, strip->rect()))}});
            }
        }
    }
    return regions;
}

void sceneMixedContainer(SceneContext& ctx, const QString& name, HeaderMode header)
{
    auto stationId = std::make_shared<QString>();
    sceneProposedEdges(ctx, name, [stationId](SceneContext& c) {
        // Controller resolution: the Station container in the left strip;
        // the Main Panel stays right.
        *stationId = addStationContainer(c, DockEdge::Left);
    }, [stationId](SceneContext& c) {
        poseStation(c, *stationId);
        pump(kFinalSettleMs);
    }, header);
    if (!ctx.failure.isEmpty() || stationId->isEmpty()) {
        return;
    }
    const QJsonArray regions = stationRegions(ctx, *stationId);
    for (CapturedImage& img : ctx.images) {
        if (img.name == name) {
            img.extra[QStringLiteral("entry_regions")] = regions;
            img.extra[QStringLiteral("region_provenance")] = QStringLiteral(
                "longpath-port: Longpath instruments; retained: Thetis meter faces; "
                "proposed: headers, strips and the container viewport");
        }
    }
}

void captureDialog(SceneContext& ctx, QWidget* dialog, const QString& name)
{
    if (dialog == nullptr) {
        ctx.failure = QStringLiteral("%1: the dialog did not open").arg(name);
        return;
    }
    pump(kDialogSettleMs);
    for (const QJsonValue& v : stopTimersUnder(dialog)) {
        ctx.stoppedTimers.append(v);
    }
    pump(kFinalSettleMs);
    if (ctx.beforeGrab) {
        ctx.beforeGrab(dialog);
        pump(kFinalSettleMs);
    }
    CapturedImage img;
    img.name = name;
    img.kind = QStringLiteral("dialog");
    img.image = grabWidget(dialog);
    img.logicalSize = dialog->size();
    img.dpr = img.image.devicePixelRatio();
    img.extra[QStringLiteral("widget_class")] = QString::fromLatin1(dialog->metaObject()->className());
    img.extra[QStringLiteral("window_title")] = dialog->windowTitle();
    img.extra[QStringLiteral("visible_text_fonts")] = visibleTextFonts(dialog);
    img.geometry = widgetGeometry(dialog);
    attachRegions(img, dialog);
    ctx.images.append(img);
}

// Opens a modal dialog through `open` (which blocks in exec()), grabs it from
// a deferred timer, then rejects it so `open` returns.
template <typename T>
void captureModal(SceneContext& ctx, const QString& name, const std::function<void()>& open)
{
    bool done = false;
    QTimer::singleShot(0, ctx.window, [&ctx, &done, name] {
        QElapsedTimer t;
        t.start();
        T* dialog = nullptr;
        while (dialog == nullptr && t.elapsed() < 5000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            dialog = visibleTopLevel<T>();
        }
        captureDialog(ctx, dialog, name);
        if (dialog != nullptr) {
            dialog->reject();
        }
        done = true;
    });
    open();
    if (!done) {
        ctx.failure = QStringLiteral("%1: the modal dialog never opened").arg(name);
    }
}

void prepareForDialog(SceneContext& ctx)
{
    // Dialog scenes run on the two-band window so slice-bound pages have a
    // tuned slice; the window itself is not captured.
    tuneSlices(ctx, twoBandPoses(), 0);
    if (auto* poller = ctx.window->findChild<MeterPoller*>()) {
        poller->stop();
    }
}

QString slug(const QString& text)
{
    QString s = text.toLower();
    s.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    s.remove(QRegularExpression(QStringLiteral("^-+|-+$")));
    return s;
}

struct SetupLeaf {
    int index{-1};
    QString label;
    QString category;
    bool hidden{false};
};

SetupDialog* openSetupDialog(MainWindow* window)
{
    SetupDialog* created = nullptr;
    const auto conn = QObject::connect(window, &MainWindow::setupDialogCreated,
                                       [&created](SetupDialog* d) { created = d; });
    QMetaObject::invokeMethod(window, "openSetupAtPage", Qt::DirectConnection,
                              Q_ARG(QString, QString()));
    QObject::disconnect(conn);
    if (created == nullptr) {
        created = visibleTopLevel<SetupDialog>();
    }
    return created;
}

QVector<SetupLeaf> setupLeaves(SetupDialog* dialog)
{
    QVector<SetupLeaf> leaves;
    auto* tree = dialog->findChild<QTreeWidget*>();
    if (tree == nullptr) {
        return leaves;
    }
    const QStringList labels = dialog->pageLabelsForTest();
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* category = tree->topLevelItem(i);
        for (int j = 0; j < category->childCount(); ++j) {
            QTreeWidgetItem* leaf = category->child(j);
            const int index = leaf->data(0, Qt::UserRole).toInt();
            if (index < 0 || index >= labels.size()) {
                continue;
            }
            leaves.append({index, labels.at(index), category->text(0),
                           leaf->isHidden() || category->isHidden()});
        }
    }
    return leaves;
}

QTreeWidgetItem* treeItemForIndex(QTreeWidget* tree, int index)
{
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* category = tree->topLevelItem(i);
        for (int j = 0; j < category->childCount(); ++j) {
            if (category->child(j)->data(0, Qt::UserRole).toInt() == index) {
                return category->child(j);
            }
        }
    }
    return nullptr;
}

// ---- workspace profiles (workspace preview) --------------------------------

// The fixed "last saved" time the preview's profiles show.
QDateTime profileClock()
{
    return QDateTime(QDate(2026, 10, 7), QTime(14, 32), QTimeZone::utc());
}

QJsonObject rxSource(int rx)
{
    return QJsonObject{{QStringLiteral("rxSource"), rx}};
}

// A container bound to one slice: the header's source chip names it.
ContainerDocument onSlice(ContainerDocument d, int rx)
{
    d.config.insert(QStringLiteral("rxSource"), rx);
    return d;
}

// The DX top strip: room for the needles without taking half the window.
constexpr int kDxTopStripPx = 190;

// The four seeded profiles, in rail order (S, D, C, F).
enum class SceneProfile { Ssb, Dx, Contest, Ft8 };

QString profileName(SceneProfile p)
{
    switch (p) {
    case SceneProfile::Ssb: return QStringLiteral("SSB");
    case SceneProfile::Dx: return QStringLiteral("DX");
    case SceneProfile::Contest: return QStringLiteral("Contest");
    case SceneProfile::Ft8: return QStringLiteral("FT8");
    }
    return {};
}

struct SeededProfiles {
    QMap<SceneProfile, QString> ids;
    // Containers to pose after the data is fed: needles fed for slice A
    // (as MeterPoller would), filter panes given one trace refresh.
    QMap<SceneProfile, QStringList> needleHosts;
    QMap<SceneProfile, QStringList> filterHosts;
    QMap<SceneProfile, QString> description;
};

void placeMain(WorkspaceDocument& d, DockEdge edge)
{
    for (ContainerDocument& c : d.containers) {
        if (c.id == d.mainContainerId) {
            c.dockMode = DockMode::PanelDocked;
            c.edge = edge;
            c.visible = true;
        }
    }
}

// Commits `base` edited by `edit`, every container's header `header`.
bool commitArrangement(SceneContext& ctx, const WorkspaceDocument& base, HeaderMode header,
                       const std::function<void(WorkspaceDocument&, ContainerManager*)>& edit)
{
    auto* manager = ctx.window->findChild<ContainerManager*>();
    WorkspaceDocument draft = base;
    draft.revision = manager->workspaceStore()->snapshot().revision;
    edit(draft, manager);
    for (ContainerDocument& c : draft.containers) {
        c.header = header;
    }
    if (manager->commitWorkspace(draft, draft.revision).status != CommitStatus::Saved) {
        return false;
    }
    pump(kSettleMs);
    return true;
}

// Seeds SSB, DX, Contest and FT8 through the same calls the rail and the
// manage dialog make (rename, save, New > From Current Arrangement), then
// switches to `show`.
// `dxSignalType` replaces the DX top strip's two Longpath needles (empty:
// the needles).
bool seedProfiles(SceneContext& ctx, SceneProfile show, SeededProfiles& seeded,
                  const QString& dxSignalType = {})
{
    WorkspaceProfiles* profiles = ctx.window->workspaceProfiles();
    auto* manager = ctx.window->findChild<ContainerManager*>();
    if (profiles == nullptr || manager == nullptr || manager->workspaceStore() == nullptr) {
        ctx.failure = QStringLiteral("no workspace profiles: the workspace preview switch is off");
        return false;
    }
    if (profiles->ids().size() != 1) {
        ctx.failure = QStringLiteral("expected the first-run Default profile only");
        return false;
    }
    profiles->setClock(&profileClock);
    const WorkspaceDocument base = manager->workspaceStore()->snapshot();

    // SSB: Main Panel right, the Station mixed container left, headers hidden.
    if (!commitArrangement(ctx, base, HeaderMode::Hidden, [](WorkspaceDocument& d, ContainerManager* m) {
            placeMain(d, DockEdge::Right);
            d.containers.append(makeSceneContainer(m, QStringLiteral("Station"), DockEdge::Left,
                                                   ContentLayout::VerticalStack, stationEntries()));
        })) {
        ctx.failure = QStringLiteral("could not commit the SSB arrangement");
        return false;
    }
    const QString first = profiles->activeId();
    if (!profiles->rename(first, QStringLiteral("SSB")).ok() || !profiles->save().ok()) {
        ctx.failure = QStringLiteral("could not rename and save SSB");
        return false;
    }
    seeded.ids[SceneProfile::Ssb] = first;
    seeded.needleHosts[SceneProfile::Ssb] = {QStringLiteral("preview-station")};
    seeded.filterHosts[SceneProfile::Ssb] = {QStringLiteral("preview-station")};
    seeded.description[SceneProfile::Ssb] = QStringLiteral(
        "Main Panel right; Station (Longpath frequency, filter, needle and bar with Thetis "
        "S-Meter and cross needle, slice A) left; every header Hidden");

    const auto create = [&](SceneProfile p, HeaderMode header,
                            const std::function<void(WorkspaceDocument&, ContainerManager*)>& edit) {
        if (!commitArrangement(ctx, base, header, edit)) {
            ctx.failure = QStringLiteral("could not commit the %1 arrangement").arg(profileName(p));
            return false;
        }
        const auto r = profiles->createFromCurrent(profileName(p));
        if (!r.ok()) {
            ctx.failure = QStringLiteral("could not create %1: %2").arg(profileName(p), r.error);
            return false;
        }
        seeded.ids[p] = r.id;
        return true;
    };

    // DX: Main Panel right; a Frequency (Longpath) per slice left, with
    // source chips; signal needles for A and B on top; Always.
    const QString dxSignal = dxSignalType.isEmpty() ? QString::fromLatin1(LongpathContent::kNeedleType)
                                                    : dxSignalType;
    if (!create(SceneProfile::Dx, HeaderMode::Shown, [dxSignal](WorkspaceDocument& d, ContainerManager* m) {
            placeMain(d, DockEdge::Right);
            const QString freq = QString::fromLatin1(LongpathContent::kFrequencyType);
            const QString needle = dxSignal;
            d.containers.append(onSlice(makeSceneContainer(m, QStringLiteral("Frequency A"), DockEdge::Left,
                                                   ContentLayout::VerticalStack, {{freq, {}, {}, rxSource(1)}}), 1));
            d.containers.append(onSlice(makeSceneContainer(m, QStringLiteral("Frequency B"), DockEdge::Left,
                                                   ContentLayout::VerticalStack, {{freq, {}, {}, rxSource(2)}}), 2));
            d.containers.append(onSlice(makeSceneContainer(m, QStringLiteral("Signal A"), DockEdge::Top,
                                                   ContentLayout::VerticalStack, {{needle, {}, {}, rxSource(1)}}), 1));
            d.containers.append(onSlice(makeSceneContainer(m, QStringLiteral("Signal B"), DockEdge::Top,
                                                   ContentLayout::VerticalStack, {{needle, {}, {}, rxSource(2)}}), 2));
            d.arrangement.strips[DockEdge::Top].thickness = kDxTopStripPx;
        })) {
        return false;
    }
    if (dxSignalType.isEmpty()) {
        seeded.needleHosts[SceneProfile::Dx] = {QStringLiteral("preview-signal-a"), QStringLiteral("preview-signal-b")};
        seeded.description[SceneProfile::Dx] = QStringLiteral(
            "Main Panel right; Frequency A and Frequency B (Longpath, source chips RX A / RX B) "
            "left; Signal A and Signal B (Longpath needles) top; every header Always");
    } else {
        seeded.description[SceneProfile::Dx] = QStringLiteral(
            "Main Panel right; Frequency A and Frequency B (Longpath, source chips RX A / RX B) "
            "left; Signal A and Signal B (%1) top; every header Always").arg(dxSignalType);
    }

    // Contest: all four strips, dense, Always.
    if (!create(SceneProfile::Contest, HeaderMode::Shown, [](WorkspaceDocument& d, ContainerManager* m) {
            placeMain(d, DockEdge::Right);
            d.containers.append(makeSceneContainer(m, QStringLiteral("Meters"), DockEdge::Top,
                                                   ContentLayout::FreeCanvas, metersEntries()));
            d.containers.append(makeSceneContainer(m, QStringLiteral("Levels"), DockEdge::Left,
                                                   ContentLayout::VerticalStack, levelsEntries()));
            d.containers.append(onSlice(makeSceneContainer(
                m, QStringLiteral("Filter A"), DockEdge::Left, ContentLayout::VerticalStack,
                {{QString::fromLatin1(LongpathContent::kFilterType), {}, {}, rxSource(1)}}), 1));
            d.containers.append(makeSceneContainer(m, QStringLiteral("Band"), DockEdge::Bottom,
                                                   ContentLayout::FreeCanvas, bandEntries()));
        })) {
        return false;
    }
    seeded.filterHosts[SceneProfile::Contest] = {QStringLiteral("preview-filter-a")};
    seeded.description[SceneProfile::Contest] = QStringLiteral(
        "Meters top, Levels and Filter A (Longpath filter passband, slice A) left, Main Panel "
        "right, Band bottom; every header Always");

    // FT8: a narrow right strip with only slice B's frequency and filter;
    // the Main Panel moves to the left strip; Hidden.
    if (!create(SceneProfile::Ft8, HeaderMode::Hidden, [](WorkspaceDocument& d, ContainerManager* m) {
            placeMain(d, DockEdge::Left);
            d.containers.append(onSlice(makeSceneContainer(
                m, QStringLiteral("FT8"), DockEdge::Right, ContentLayout::VerticalStack,
                {{QString::fromLatin1(LongpathContent::kFrequencyType), {}, {}, rxSource(2)},
                 {QString::fromLatin1(LongpathContent::kFilterType), {}, {}, rxSource(2)}}), 2));
            d.arrangement.strips[DockEdge::Right].thickness = EdgeDockStrip::kSideThicknessPx;
        })) {
        return false;
    }
    seeded.filterHosts[SceneProfile::Ft8] = {QStringLiteral("preview-ft8")};
    seeded.description[SceneProfile::Ft8] = QStringLiteral(
        "FT8 (Longpath frequency and filter passband, slice B) alone in a narrow right strip; "
        "Main Panel in the left strip; every header Hidden");

    const auto r = profiles->switchTo(seeded.ids.value(show));
    if (!r.ok()) {
        ctx.failure = QStringLiteral("could not switch to %1: %2").arg(profileName(show), r.error);
        return false;
    }
    pump(kSettleMs);
    return true;
}

// The peak a fed RX needle holds above the slice's level (slice A's -68
// over -73 dBm, as the Station scenes pose it).
constexpr double kFedPeakAboveDb = 5.0;

// Feeds every Longpath needle and bar under `root` bound to RX: Signal Peak
// the reading MeterPoller would send for its slice: the slice's posed
// signal level, after a peak kFedPeakAboveDb above it. Display-only: the
// readingUpdated signal the instruments listen to, no radio or Core.
// The seam a fed reading went through. Each Longpath needle and bar
// registers its context with MeterPoller::addReadingConsumer on creation;
// the poller is stopped (freeze), so the scene stands in for its timer
// frame and emits readingUpdated for the registered context.
QString consumerSeam(MeterPoller* poller)
{
    return QStringLiteral("MeterPoller::readingUpdated to a registered reading consumer "
                          "(addReadingConsumer; poller stopped, %1 consumers; display-only)")
        .arg(poller->readingConsumerCountForTest());
}

void feedSliceInstruments(SceneContext& ctx, QWidget* root)
{
    auto* poller = ctx.window->findChild<MeterPoller*>();
    if (poller == nullptr || root == nullptr) {
        return;
    }
    QList<QWidget*> instruments;
    for (NeedleInstrument* n : root->findChildren<NeedleInstrument*>()) {
        instruments.append(n);
    }
    for (BarInstrument* b : root->findChildren<BarInstrument*>()) {
        instruments.append(b);
    }
    for (QWidget* w : instruments) {
        if (!w->property("longpathPrimaryBinding").isValid()
            || w->property("longpathPrimaryBinding").toInt() != MeterBinding::SignalPeak) {
            continue;
        }
        const QJsonObject context = w->property("containerSourceContext").toJsonObject();
        // The host resolves rxSource (1-based) to sliceId (0-based).
        const int sliceId = context.contains(QStringLiteral("sliceId"))
                                ? context.value(QStringLiteral("sliceId")).toInt()
                                : context.value(QStringLiteral("rxSource")).toInt() - 1;
        SliceModel* slice = sliceId >= 0 ? ctx.model->sliceById(sliceId) : nullptr;
        if (slice == nullptr) {
            continue;
        }
        const int rx = sliceId + 1;
        const double level = slice->signalStrengthDbm();
        const QString unavailable = poller->bindingUnavailableReason(MeterBinding::SignalPeak);
        if (!unavailable.isEmpty()) {
            ctx.notes.append(QStringLiteral("RX: Signal Peak unavailable (%1); the instrument shows no value")
                                 .arg(unavailable));
        }
        emit poller->readingUpdated(context, MeterBinding::SignalPeak, level + kFedPeakAboveDb);
        emit poller->readingUpdated(context, MeterBinding::SignalPeak, level);
        const QString entry = w->property(kRegionNameProperty).toString().isEmpty()
                                  ? w->property("containerEntryId").toString()
                                  : w->property(kRegionNameProperty).toString();
        ctx.fedReadings.append(QJsonObject{
            {QStringLiteral("instrument"), QString::fromLatin1(w->metaObject()->className())},
            {QStringLiteral("entry"), entry},
            {QStringLiteral("slice"), QString(QChar('A' + rx - 1))},
            {QStringLiteral("binding"), QStringLiteral("RX: Signal Peak")},
            {QStringLiteral("values_dbm"), QJsonArray{level + kFedPeakAboveDb, level}},
            {QStringLiteral("seam"), consumerSeam(poller)}});
    }
}

void poseProfile(SceneContext& ctx, const SeededProfiles& seeded, SceneProfile p)
{
    auto* manager = ctx.window->findChild<ContainerManager*>();
    auto* poller = ctx.window->findChild<MeterPoller*>();
    for (const QString& id : seeded.needleHosts.value(p)) {
        ContainerContentHost* host = manager->contentHost(id);
        if (host == nullptr || poller == nullptr) {
            continue;
        }
        feedSliceInstruments(ctx, host);
        ctx.notes.append(QStringLiteral("%1 Longpath needles: readingUpdated RX: Signal Peak for each "
                                        "needle's slice, as MeterPoller sends (see fed_readings)").arg(id));
    }
    for (const QString& id : seeded.filterHosts.value(p)) {
        ContainerContentHost* host = manager->contentHost(id);
        if (host == nullptr) {
            continue;
        }
        for (BandwidthFilterPane* pane : host->findChildren<BandwidthFilterPane*>()) {
            for (QTimer* timer : pane->findChildren<QTimer*>()) {
                QMetaObject::invokeMethod(timer, "timeout");
            }
            ctx.notes.append(QStringLiteral("%1 Longpath filter pane: one trace refresh from the fed pan").arg(id));
        }
    }
    if (p == SceneProfile::Ssb) {
        ctx.notes.append(QStringLiteral("Station Longpath bar (TX: SWR): no reading in receive, shows its dash"));
    }
}

QJsonArray sliceStates(SceneContext& ctx)
{
    QJsonArray out;
    for (SliceModel* s : ctx.model->slices()) {
        out.append(QJsonObject{{QStringLiteral("slice"), QString(QChar('A' + s->sliceIndex()))},
                               {QStringLiteral("frequency_hz"), s->frequency()},
                               {QStringLiteral("mode"), SliceModel::modeName(s->dspMode())},
                               {QStringLiteral("filter_low_hz"), s->filterLow()},
                               {QStringLiteral("filter_high_hz"), s->filterHigh()}});
    }
    return out;
}

QLabel* titleLabel(TitleBar* bar)
{
    for (QLabel* label : bar->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly)) {
        if (label->text() == QStringLiteral("NereusSDR")) {
            return label;
        }
    }
    return nullptr;
}

// Logical-px measurements of the rail against the network readout and
// the title, in window coordinates.
QJsonObject railMeasure(SceneContext& ctx)
{
    ProfileRailDock* dock = ctx.window->profileRailDock();
    auto* bar = ctx.window->findChild<TitleBar*>();
    QLabel* title = bar != nullptr ? titleLabel(bar) : nullptr;
    if (dock == nullptr || title == nullptr) {
        return QJsonObject{{QStringLiteral("error"), QStringLiteral("no rail dock or title")}};
    }
    QWidget* readout = bar->connectionSegment();
    const int readoutRight = readout->mapTo(ctx.window, QPoint(readout->width(), 0)).x();
    const int textWidth = title->fontMetrics().horizontalAdvance(title->text());
    const int titleTextLeft = title->mapTo(ctx.window, QPoint((title->width() - textWidth) / 2, 0)).x();
    QPushButton* badge = dock->collapsedBadge();
    const QRect badgeRect(badge->mapTo(ctx.window, QPoint(0, 0)), badge->size());
    const int openWidth = dock->isOpen() ? dock->popover()->width() : dock->popover()->sizeHint().width();
    const int openLeft = badgeRect.x() + badgeRect.width() - openWidth;
    QJsonArray barChildren;
    for (QWidget* w : bar->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
        if (w->isVisible()) {
            barChildren.append(QStringLiteral("%1 %2x%3 at x %4")
                                   .arg(QString::fromLatin1(w->metaObject()->className()))
                                   .arg(w->width()).arg(w->height())
                                   .arg(w->mapTo(ctx.window, QPoint(0, 0)).x()));
        }
    }
    return QJsonObject{
        {QStringLiteral("window_width"), ctx.window->width()},
        {QStringLiteral("collapsed_badge"), rectJson(badgeRect)},
        {QStringLiteral("collapsed_width"), badgeRect.width()},
        {QStringLiteral("collapsed_height"), badgeRect.height()},
        {QStringLiteral("open_width"), openWidth},
        {QStringLiteral("open_height"), dock->isOpen() ? dock->popover()->height()
                                                       : dock->popover()->sizeHint().height()},
        {QStringLiteral("open_left"), openLeft},
        {QStringLiteral("network_readout_right"), readoutRight},
        {QStringLiteral("title_text_left"), titleTextLeft},
        {QStringLiteral("free_readout_to_title"), titleTextLeft - readoutRight},
        {QStringLiteral("free_readout_to_badge"), badgeRect.x() - readoutRight},
        {QStringLiteral("open_covers_readout_px"), qMax(0, readoutRight - openLeft)},
        {QStringLiteral("title_bar_children"), barChildren},
    };
}

QJsonArray railRegions(SceneContext& ctx)
{
    QJsonArray regions;
    ProfileRailDock* dock = ctx.window->profileRailDock();
    if (dock == nullptr) {
        return regions;
    }
    QPushButton* badge = dock->collapsedBadge();
    regions.append(QJsonObject{
        {QStringLiteral("name"), QStringLiteral("profile-rail-collapsed")},
        {QStringLiteral("provenance"), QStringLiteral("proposed")},
        {QStringLiteral("note"), QStringLiteral("NereusSDR wrapper; Longpath pill style")},
        {QStringLiteral("rect"), rectJson(QRect(badge->mapTo(ctx.window, QPoint(0, 0)), badge->size()))}});
    if (dock->isOpen()) {
        regions.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("profile-rail-popover")},
            {QStringLiteral("provenance"), QStringLiteral("proposed")},
            {QStringLiteral("rect"), rectJson(dock->popover()->geometry())}});
        ProfileRail* rail = dock->rail();
        regions.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("profile-rail")},
            {QStringLiteral("provenance"), QStringLiteral("longpath-port")},
            {QStringLiteral("rect"), rectJson(QRect(rail->mapTo(ctx.window, QPoint(0, 0)), rail->size()))}});
    }
    return regions;
}

// Paints a separate top-level (menu, tooltip, dialog) into a window image
// where it sits on screen.
// Where `top` sits in `window`. A top-level moved after it was shown with
// WA_DontShowOnScreen keeps a stale mapToGlobal, so `byGeometry` reads the
// window geometry instead (profile-rail-open's placed tooltip).
QPoint topOffset(QWidget* window, QWidget* top, bool byGeometry)
{
    if (byGeometry) {
        return top->geometry().topLeft() - window->geometry().topLeft();
    }
    return top->mapToGlobal(QPoint(0, 0)) - window->mapToGlobal(QPoint(0, 0));
}

void compositeOver(CapturedImage& img, QWidget* window, QWidget* top, bool byGeometry = false)
{
    if (top == nullptr || !top->isVisible()) {
        return;
    }
    const QImage over = grabWidget(top);
    const QPoint at = topOffset(window, top, byGeometry);
    QPainter painter(&img.image);
    painter.drawImage(QRect(at, top->size()), over);
}

QWidget* visibleToolTip()
{
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (w->isVisible() && w->inherits("QTipLabel")) {
            return w;
        }
    }
    return nullptr;
}

void addTopStripCropImage(SceneContext& ctx, const QString& name)
{
    // At the crop profile captureWindow already cut it.
    if (ctx.crops) {
        return;
    }
    for (const CapturedImage& img : ctx.images) {
        if (img.name != name) {
            continue;
        }
        CapturedImage full = img;
        full.crops.clear();
        addCrop(full.crops, QStringLiteral("top-strip"), ctx.window, ctx.window->findChild<TitleBar*>(), full.dpr);
        appendCropImages(ctx, full, name);
        return;
    }
}

void annotateProfileImages(SceneContext& ctx, const QString& name, const SeededProfiles& seeded,
                           SceneProfile shown)
{
    QJsonObject profiles;
    for (auto it = seeded.ids.constBegin(); it != seeded.ids.constEnd(); ++it) {
        profiles.insert(profileName(it.key()), it.value());
    }
    const QJsonArray slices = sliceStates(ctx);
    const QJsonObject measure = railMeasure(ctx);
    const QJsonArray regions = railRegions(ctx);
    for (CapturedImage& img : ctx.images) {
        if (img.name != name && img.extra.value(QStringLiteral("source")).toString() != name) {
            continue;
        }
        img.extra[QStringLiteral("slice_states")] = slices;
        img.extra[QStringLiteral("active_profile")] = profileName(shown);
        img.extra[QStringLiteral("arrangement")] = seeded.description.value(shown);
        img.extra[QStringLiteral("profiles")] = profiles;
        img.extra[QStringLiteral("profile_saved_at")] = profileClock().toString(Qt::ISODate);
        img.extra[QStringLiteral("rail_measure")] = measure;
        img.extra[QStringLiteral("rail_regions")] = regions;
        img.extra[QStringLiteral("region_provenance")] = QStringLiteral(
            "longpath-port: the profile rail (fanned out) and the Longpath instruments; proposed: "
            "profiles, the collapsed badge wrapper, the popover, arrangements, the dialog");
    }
}

// One profile scene: the two-band station, profiles seeded, `shown`
// active, the rail at rest unless `pose` changes it.
// Paints `tops` into the window image `name` and cuts its crops again
// from the composited image.
void compositeAndRecrop(SceneContext& ctx, const QString& name, const QList<QWidget*>& tops,
                        bool byGeometry = false)
{
    for (int i = ctx.images.size() - 1; i >= 0; --i) {
        if (ctx.images[i].extra.value(QStringLiteral("source")).toString() == name) {
            ctx.images.removeAt(i);
        }
    }
    for (CapturedImage& img : ctx.images) {
        if (img.name != name) {
            continue;
        }
        QJsonArray regions = img.extra.value(QStringLiteral("regions")).toArray();
        for (QWidget* top : tops) {
            compositeOver(img, ctx.window, top, byGeometry);
            if (top != nullptr && top->isVisible()) {
                appendRegions(regions, top, topOffset(ctx.window, top, byGeometry), img.logicalSize);
            }
        }
        img.extra[QStringLiteral("regions")] = regions;
        const CapturedImage full = img;
        appendCropImages(ctx, full, name);
        return;
    }
}

// The scene runs inside the event loop, so processEvents never runs the
// deleteLater() of the badges the rail dropped on its last rebuild; live,
// the loop does on return. Hide them so they are not painted under the
// current badges. (Flushing every deferred delete instead also drops what
// the poses feed, so it is limited to the rail.)
void hideStaleRailBadges(ProfileRail* rail)
{
    QSet<QPushButton*> current;
    for (const QString& id : rail->badges()) {
        current.insert(rail->badgeButton(id));
    }
    for (QPushButton* b : rail->findChildren<QPushButton*>(QStringLiteral("profileRailBadge"),
                                                          Qt::FindDirectChildrenOnly)) {
        if (!current.contains(b)) {
            b->hide();
        }
    }
}

void sceneProfile(SceneContext& ctx, const QString& name, SceneProfile shown,
                  const std::function<void(SceneContext&)>& pose = {},
                  const std::function<void(SceneContext&)>& afterCapture = {},
                  bool topStripCrop = false, const QString& dxSignalType = {})
{
    ctx.provenance = QStringLiteral("proposed");
    ctx.extraCrops = addStripCrops;
    auto seeded = std::make_shared<SeededProfiles>();
    runWindowScene(ctx, twoBandPoses(), 0, [seeded, shown, dxSignalType](SceneContext& c) {
        seedProfiles(c, shown, *seeded, dxSignalType);
    }, [seeded, shown, pose](SceneContext& c) {
        poseProfile(c, *seeded, shown);
        if (pose) {
            pose(c);
        }
        pump(kFinalSettleMs);
    });
    if (!ctx.failure.isEmpty()) {
        return;
    }
    noteStrips(ctx);
    captureWindow(ctx, name);
    if (afterCapture) {
        afterCapture(ctx);
    }
    if (topStripCrop) {
        addTopStripCropImage(ctx, name);
    }
    annotateProfileImages(ctx, name, *seeded, shown);
}

// The display-only transmit state of slice A (baseline-tx), before the
// freeze.
void poseTxBeforeFreeze(SceneContext& c)
{
    SliceModel* sliceA = c.model->sliceById(0);
    if (auto* mox = c.window->findChild<MoxDisplayController*>()) {
        mox->setKeyed(true, sliceA != nullptr ? sliceA->sliceIndex() : 0);
        c.notes.append(QStringLiteral("MoxDisplayController::setKeyed(true, slice A)"));
    } else {
        c.notShown.append(QJsonObject{
            {QStringLiteral("item"), QStringLiteral("MoxDisplayController keyed overlay")},
            {QStringLiteral("reason"), QStringLiteral(
                "a Deferred-startup local window builds no MoxDisplayController "
                "(it needs the TX analyzer)")}});
    }
    if (SpectrumWidget* sw = spectrumFor(c.window, panIdAt(0))) {
        sw->setTxActiveForTest(true);
        c.notes.append(QStringLiteral("SpectrumWidget::setTxActiveForTest(true) on pan-0"));
    }
    if (auto* poller = c.window->findChild<MeterPoller*>()) {
        poller->stop();
        poller->setInTx(true);
        c.notes.append(QStringLiteral("MeterPoller::stop(), setInTx(true)"));
    }
}

// The transmit display and S-meter levels, after the data is fed.
void poseTxAfterFeed(SceneContext& c)
{
    // Pan 1 transmits: the transmit trace and waterfall a local
    // window's LocalTxDisplaySource would draw, slice A's LSB at
    // the default transmit filter 100..2900 Hz.
    if (SpectrumWidget* sw = spectrumFor(c.window, panIdAt(0))) {
        feedTxPan(sw, kTwoBandCarrierHz, false, kTxFilterLowHz, kTxFilterHighHz,
                  kTxPeakDbm, kTxFloorDbm, kTxSeed);
        c.notes.append(QStringLiteral(
            "pan-0 transmit display: updateSpectrumFromTxPixels and pushTxWaterfallRow, "
            "LSB voice 100..2900 Hz, peak %1 dBm over %2 dBm").arg(kTxPeakDbm).arg(kTxFloorDbm));
    }
    for (SMeterWidget* meter : allMeters(c)) {
        meter->setTransmitting(true);
        meter->setTxMeters(kTxForwardWatts, kTxSwr);
        meter->setMicMeters(kTxMicDb, 0.0f, kTxMicDb, -30.0f);
        c.notes.append(QStringLiteral("SMeterWidget: transmitting, 85 W, SWR 1.25, mic -12 dB"));
    }
    c.notShown.append(QJsonObject{
        {QStringLiteral("item"), QStringLiteral("ALC -2 dB")},
        {QStringLiteral("reason"), QStringLiteral(
            "SMeterWidget has no ALC input and the default panel has no ALC meter")}});
}

// ---- coverage scenes (workspace preview) -----------------------------------

// The ALC level the transmit scenes pose (baseline-tx records it as not
// shown on the default panel).
constexpr double kTxAlcDb = -2.0;
// A not-implemented plate: its width and its gap below what it explains.
constexpr int kPlateWidthPx = 270;
constexpr int kPlateGapPx = 6;
// Container Settings is sized to the window less this much.
constexpr int kDialogInsetPx = 80;
// The free container's clearance from the dB scale and the time scale.
constexpr int kFreeScaleClearancePx = 72;
constexpr int kFreeBottomClearancePx = 24;
// The placeholder per-MAC scope tx-processing seeds TX profiles under,
// in the throwaway test profile (a locally administered address).
constexpr auto kPreviewTxProfileScope = "02:00:00:00:00:00";
// At most this many window growths for a strip's full-content image.
constexpr int kFullStripGrowSteps = 4;
// Settles at most to let a grown strip's size stop changing.
constexpr int kFullStripStableSteps = 6;

// Applets the source has but MainWindow does not construct (their lines
// are commented out in MainWindow.cpp). Each is plain UI over NyiOverlay
// markers with no model, network or device access, so the preview builds
// them standalone as container content, tagged exists-not-wired.
constexpr auto kPreviewCwxType = "preview.cwx";
constexpr auto kPreviewDvkType = "preview.dvk";
constexpr auto kPreviewDigitalType = "preview.digital";
constexpr auto kPreviewEqType = "preview.eq";

// Every slice's state on every image of the scene.
void annotateSlices(SceneContext& ctx)
{
    const QJsonArray slices = sliceStates(ctx);
    for (CapturedImage& img : ctx.images) {
        img.extra[QStringLiteral("slice_states")] = slices;
    }
}

void addAllStripCrops(SceneContext& ctx, QVector<CropRect>& crops, double dpr)
{
    addStripCrops(ctx, crops, dpr);
    if (auto* area = ctx.window->findChild<EdgeDockArea*>()) {
        addCrop(crops, QStringLiteral("strip-right"), ctx.window, area->strip(DockEdge::Right), dpr);
    }
}

// Takes `typeIds` out of the Main Panel, so a singleton can be placed in
// another container (its "Move").
void takeFromMain(WorkspaceDocument& d, const QStringList& typeIds)
{
    for (ContainerDocument& c : d.containers) {
        if (c.id == d.mainContainerId) {
            c.contents.removeIf([&typeIds](const ContentEntry& e) { return typeIds.contains(e.typeId); });
        }
    }
}

bool registerPreviewApplets(SceneContext& ctx)
{
    auto* manager = ctx.window->findChild<ContainerManager*>();
    if (manager == nullptr || manager->contentRegistry() == nullptr || manager->workspaceStore() == nullptr) {
        ctx.failure = QStringLiteral("no container workspace");
        return false;
    }
    ContainerContentRegistry* registry = manager->contentRegistry();
    RadioModel* model = ctx.model;
    using Make = std::function<QWidget*(RadioModel*, QWidget*)>;
    const auto add = [registry, model](const char* typeId, const QString& title, const Make& make) {
        const QString id = QString::fromLatin1(typeId);
        if (registry->hasWidgetFactory(id)) {
            return;
        }
        registry->registerWidgetFactory(id, {id, title, false, true, {}},
            [model, make](const ContentEntry&, QWidget* parent, ContentRenderMode) -> QWidget* {
                QWidget* w = make(model, parent);
                tagProvenance(w, provNotWired());
                return w;
            });
    };
    add(kPreviewCwxType, QStringLiteral("CWX (not wired)"),
        [](RadioModel* m, QWidget* p) -> QWidget* { return new CwxApplet(m, p); });
    add(kPreviewDvkType, QStringLiteral("DVK (not wired)"),
        [](RadioModel* m, QWidget* p) -> QWidget* { return new DvkApplet(m, p); });
    add(kPreviewDigitalType, QStringLiteral("Digital (not wired)"),
        [](RadioModel* m, QWidget* p) -> QWidget* { return new DigitalApplet(m, p); });
    add(kPreviewEqType, QStringLiteral("EQ (not wired)"),
        [](RadioModel* m, QWidget* p) -> QWidget* { return new EqApplet(m, p); });
    ctx.notes.append(QStringLiteral(
        "CwxApplet, DvkApplet, DigitalApplet and EqApplet: constructed standalone through preview "
        "widget factories (MainWindow leaves them commented out); exists-not-wired"));
    return true;
}

bool commitScene(SceneContext& ctx, HeaderMode header,
                 const std::function<void(WorkspaceDocument&, ContainerManager*)>& edit)
{
    auto* manager = ctx.window->findChild<ContainerManager*>();
    if (manager == nullptr || manager->workspaceStore() == nullptr || manager->contentRegistry() == nullptr) {
        ctx.failure = QStringLiteral("no container workspace: the workspace preview switch is off");
        return false;
    }
    if (!commitArrangement(ctx, manager->workspaceStore()->snapshot(), header, edit)) {
        ctx.failure = QStringLiteral("could not commit the scene arrangement");
        return false;
    }
    return true;
}

QJsonObject bindingConfig(int binding)
{
    return QJsonObject{{QString::fromLatin1(LongpathContent::kBindingKey), binding}};
}

// A plate where a control does not exist in this build. `message` is the
// plain-English text on the plate; `reason` (the technical cause) goes to
// the sidecar notes, never onto the plate.
QLabel* addNotImplementedPlate(SceneContext& ctx, QWidget* near, const QString& what, const QString& message,
                               const QString& reason, bool below = true)
{
    QWidget* window = ctx.window;
    ctx.notes.append(QStringLiteral("not-implemented plate \"%1\": %2").arg(message, reason));
    auto* plate = new QLabel(message, window);
    plate->setObjectName(QStringLiteral("previewNotImplementedPlate"));
    plate->setWordWrap(true);
    plate->setStyleSheet(QStringLiteral(
        "QLabel { background: #1c1c20; color: #c8c8cd; border: 1px dashed #7e7e85; "
        "border-radius: 6px; padding: 6px 8px; font-size: 11px; }"));
    plate->setFixedWidth(below ? kPlateWidthPx : qMin(kPlateWidthPx, near->width() - 2 * kPlateGapPx));
    plate->adjustSize();
    QPoint at = below ? near->mapTo(window, QPoint(0, near->height() + kPlateGapPx))
                      : near->mapTo(window, QPoint(kPlateGapPx, kPlateGapPx));
    at.setX(qBound(0, at.x(), window->width() - plate->width()));
    at.setY(qBound(0, at.y(), window->height() - plate->height()));
    plate->move(at);
    plate->raise();
    plate->show();
    tagProvenance(plate, provNotImplemented());
    plate->setProperty(kRegionNameProperty, QStringLiteral("plate: %1").arg(what));
    return plate;
}

// clean-spectrum: one pan, no docked containers, one free container with
// a Longpath frequency and needle for slice A, header hidden.
void sceneCleanSpectrum(SceneContext& ctx)
{
    ctx.provenance = provProposed();
    ctx.proposedValues = proposedEdgeValues();
    ctx.extraCrops = addAllStripCrops;
    const QString id = QStringLiteral("preview-instruments");
    runWindowScene(ctx, defaultPoses(), 0, [id](SceneContext& c) {
        if (!commitScene(c, HeaderMode::Hidden, [](WorkspaceDocument& d, ContainerManager* m) {
                for (ContainerDocument& cd : d.containers) {
                    if (cd.id == d.mainContainerId) {
                        cd.visible = false;
                    }
                }
                d.containers.append(onSlice(makeSceneContainer(
                    m, QStringLiteral("Instruments"), DockEdge::Right, ContentLayout::VerticalStack,
                    {{QString::fromLatin1(LongpathContent::kFrequencyType), {}, {}, rxSource(1)},
                     {QString::fromLatin1(LongpathContent::kNeedleType), {}, {}, rxSource(1)}}), 1));
            })) {
            return;
        }
        auto* manager = c.window->findChild<ContainerManager*>();
        ContainerWidget* box = manager->container(id);
        if (box == nullptr) {
            c.failure = QStringLiteral("the Instruments container was not created");
            return;
        }
        // Options > Place Free, then moved by its header to the lower right.
        emit box->placeFreeRequested();
        pump(kSettleMs);
        QWidget* pane = box->parentWidget();
        if (pane == nullptr) {
            c.failure = QStringLiteral("the Instruments container was not placed free");
            return;
        }
        WorkspaceDocument draft = manager->workspaceStore()->snapshot();
        for (auto& d : draft.containers) {
            if (d.id == id) {
                const QSize size = d.geometry.isValid() ? d.geometry.size() : box->size();
                d.geometry = QRect(QPoint(pane->width() - size.width() - kFreeScaleClearancePx,
                                          pane->height() - size.height() - kFreeBottomClearancePx),
                                   size);
                c.notes.append(QStringLiteral("Instruments placed free at %1,%2 %3x%4 in a %5x%6 pane")
                                   .arg(d.geometry.x()).arg(d.geometry.y())
                                   .arg(size.width()).arg(size.height())
                                   .arg(pane->width()).arg(pane->height()));
            }
        }
        if (manager->commitWorkspace(draft, draft.revision).status != CommitStatus::Saved) {
            c.failure = QStringLiteral("could not move the Instruments container");
        }
    }, [id](SceneContext& c) {
        // MeterPoller polls Longpath instruments on their own; with no
        // radio the needle gets slice A's reading through the poller's
        // signal, as it would live.
        auto* manager = c.window->findChild<ContainerManager*>();
        feedSliceInstruments(c, manager->contentHost(id));
        pump(kFinalSettleMs);
    });
    if (!ctx.failure.isEmpty()) {
        return;
    }
    ctx.notes.append(QStringLiteral("Main Panel hidden (visible false); Instruments: Place Free, Header Hidden"));
    noteStrips(ctx);
    captureWindow(ctx, QStringLiteral("clean-spectrum"));
    annotateSlices(ctx);
}

// How far the scroll areas in `strip` (its own and each container's) are
// scrolled past their viewports along the strip's axis, in logical px.
int stripOverflow(EdgeDockStrip* strip)
{
    const bool vertical = strip->orientation() == Qt::Vertical;
    int overflow = 0;
    for (QScrollArea* sa : strip->findChildren<QScrollArea*>()) {
        if (sa->widget() == nullptr || !sa->isVisibleTo(strip)) {
            continue;
        }
        overflow += vertical ? sa->verticalScrollBar()->maximum() : sa->horizontalScrollBar()->maximum();
    }
    return overflow;
}

// `<scene>--strip-<edge>-full`: each strip that scrolls, rendered with its
// whole content showing. The window is grown along the strip's axis until
// nothing in the strip scrolls, the strip is grabbed, and the window is
// put back; the window image (taken before) stays as the window looks.
void captureStripsFull(SceneContext& ctx, const QString& scene)
{
    auto* area = ctx.window->findChild<EdgeDockArea*>();
    if (area == nullptr) {
        return;
    }
    const QPair<DockEdge, QString> edges[] = {{DockEdge::Left, QStringLiteral("strip-left")},
                                              {DockEdge::Right, QStringLiteral("strip-right")},
                                              {DockEdge::Top, QStringLiteral("strip-top")},
                                              {DockEdge::Bottom, QStringLiteral("strip-bottom")}};
    const QSize original = ctx.window->size();
    for (const auto& [edge, edgeName] : edges) {
        EdgeDockStrip* strip = area->strip(edge);
        if (strip == nullptr || !strip->isVisible()) {
            continue;
        }
        const int before = stripOverflow(strip);
        if (before <= 0) {
            continue;
        }
        const bool vertical = strip->orientation() == Qt::Vertical;
        QSize grown = original;
        int overflow = before;
        for (int step = 0; step < kFullStripGrowSteps && overflow > 0; ++step) {
            grown += vertical ? QSize(0, overflow) : QSize(overflow, 0);
            ctx.window->resize(grown);
            pump(kSettleMs);
            overflow = stripOverflow(strip);
        }
        // The strip's width follows its content's minimum a pass after the
        // window grows (a scroll bar that went away gives its width back),
        // so a single settle can catch it one pixel short at a fractional
        // scale. Wait until two settles in a row leave it the same size.
        for (int step = 0; step < kFullStripStableSteps; ++step) {
            const QSize seen = strip->size();
            pump(kSettleMs);
            if (strip->size() == seen) {
                break;
            }
        }
        overflow = stripOverflow(strip);
        const QString name = scene + QStringLiteral("--") + edgeName + QStringLiteral("-full");
        CapturedImage img;
        img.name = name;
        img.kind = QStringLiteral("detail");
        img.image = grabWidget(strip);
        img.logicalSize = strip->size();
        img.dpr = img.image.devicePixelRatio();
        img.extra[QStringLiteral("widget_class")] = QString::fromLatin1(strip->metaObject()->className());
        img.extra[QStringLiteral("full_content")] = QJsonObject{
            {QStringLiteral("scrolled_px_in_window"), before},
            {QStringLiteral("window_grown_to"), QJsonArray{grown.width(), grown.height()}},
            {QStringLiteral("still_scrolled_px"), overflow},
            {QStringLiteral("method"), QStringLiteral(
                 "the window grown along the strip's axis until no scroll area in the strip scrolls, "
                 "the strip grabbed, the window restored")}};
        attachRegions(img, strip);
        ctx.images.append(img);
        ctx.notes.append(QStringLiteral("%1: %2 px scrolled in the window; grabbed with the window at %3x%4 "
                                        "(%5 px still scrolled)")
                             .arg(name).arg(before).arg(grown.width()).arg(grown.height()).arg(overflow));
        ctx.window->resize(original);
        pump(kSettleMs);
    }
}

// cw-digital: slice A 14.030 CW and slice B 7.074 DIGU.
QVector<PanPose> cwDigitalPoses()
{
    const QVector<PanPose> four = fourRxPoses();
    PanPose a = four.at(2);
    a.panId = panIdAt(0);
    a.spectrum.panId = a.panId;
    PanPose b = four.at(1);
    b.panId = panIdAt(1);
    b.spectrum.panId = b.panId;
    return {a, b};
}

QJsonObject seedTxProfiles(SceneContext& ctx);
QSize growSetupToFit(SceneContext& ctx, SetupDialog* d, const QString& where);

void sceneCwDigital(SceneContext& ctx)
{
    ctx.provenance = provProposed();
    ctx.proposedValues = proposedEdgeValues();
    ctx.extraCrops = addAllStripCrops;
    QJsonObject txProfileSeed;
    runWindowScene(ctx, cwDigitalPoses(), 0, [&txProfileSeed](SceneContext& c) {
        if (!registerPreviewApplets(c)) {
            return;
        }
        if (!commitScene(c, HeaderMode::Shown, [](WorkspaceDocument& d, ContainerManager* m) {
                placeMain(d, DockEdge::Left);
                takeFromMain(d, {QStringLiteral("applet:PHCW")});
                d.containers.append(makeSceneContainer(
                    m, QStringLiteral("CW and Digital"), DockEdge::Right, ContentLayout::VerticalStack,
                    {{QStringLiteral("applet:PHCW"), {}},
                     {QString::fromLatin1(kPreviewCwxType), {}},
                     {QString::fromLatin1(kPreviewDvkType), {}},
                     {QString::fromLatin1(kPreviewDigitalType), {}}}));
            })) {
            return;
        }
        auto* phcw = c.window->findChild<PhoneCwApplet*>();
        if (phcw == nullptr) {
            c.failure = QStringLiteral("no PhoneCwApplet");
            return;
        }
        phcw->showPage(1);
        c.notes.append(QStringLiteral("Main Panel in the left strip; Phone / CW moved to the right strip, "
                                      "PhoneCwApplet::showPage(1) (its CW page)"));
        // The Phone page's mic profile list reads MicProfileManager, as the
        // TX applet's profile selector does in tx-processing.
        txProfileSeed = seedTxProfiles(c);
    }, [](SceneContext& c) {
        // The CW page is not built in this build: showPage(1) falls back to
        // the Phone page, so a plate says so above the applet's first row,
        // in the applet's own layout, where it covers no control.
        if (UnbuiltFeatures::isBuilt(UnbuiltFeature::Cwx)) {
            return;
        }
        auto* phcw = c.window->findChild<PhoneCwApplet*>();
        auto* root = qobject_cast<QBoxLayout*>(phcw->layout());
        if (root == nullptr) {
            c.failure = QStringLiteral("the Phone / CW applet has no box layout for the plate");
            return;
        }
        QLabel* plate = addNotImplementedPlate(c, phcw, QStringLiteral("Phone / CW, CW page"),
                                               QStringLiteral("CW page: not built yet in NereusSDR"),
                                               QStringLiteral("the Phone / CW applet's CW page is gated by "
                                                              "UnbuiltFeature::Cwx; showPage(1) shows the "
                                                              "Phone page"),
                                               false);
        auto* holder = new QWidget(phcw);
        auto* box = new QVBoxLayout(holder);
        box->setContentsMargins(kPlateGapPx, kPlateGapPx, kPlateGapPx, kPlateGapPx);
        plate->setParent(holder);
        plate->setMinimumWidth(0);
        plate->setMaximumWidth(QWIDGETSIZE_MAX);
        box->addWidget(plate);
        root->insertWidget(0, holder);
        plate->show();
        holder->show();
        pump(kFinalSettleMs);
        c.notes.append(QStringLiteral("Phone / CW CW page: not built (UnbuiltFeature::Cwx); the applet shows "
                                      "its Phone page and a not-implemented plate sits above its first row, "
                                      "in its own layout"));
    });
    if (!ctx.failure.isEmpty()) {
        return;
    }
    // The applet's combo that lists the seeded profiles is its mic profile list.
    MicProfileManager* profiles = ctx.model->micProfileManager();
    auto* phcw = ctx.window->findChild<PhoneCwApplet*>();
    if (profiles != nullptr && phcw != nullptr) {
        QString listed = QStringLiteral("no list shows the active profile");
        for (QComboBox* combo : phcw->findChildren<QComboBox*>()) {
            if (!profiles->activeProfileName().isEmpty()
                && combo->findText(profiles->activeProfileName()) >= 0) {
                listed = QStringLiteral("%1 profile(s), \"%2\"").arg(combo->count()).arg(combo->currentText());
            }
        }
        ctx.notes.append(QStringLiteral("Phone / CW mic profile list: %1").arg(listed));
    }
    noteStrips(ctx);
    captureWindow(ctx, QStringLiteral("cw-digital"));
    for (CapturedImage& img : ctx.images) {
        if (img.name == QLatin1String("cw-digital") && !txProfileSeed.isEmpty()) {
            img.extra[QStringLiteral("tx_profile_seed")] = txProfileSeed;
        }
    }
    captureStripsFull(ctx, QStringLiteral("cw-digital"));
    annotateSlices(ctx);
}

// After the data is fed: each Longpath TX instrument gets the reading
// MeterPoller::publishGlobalReading would send every context in transmit.
void poseLongpathTx(SceneContext& ctx, const QStringList& hostIds)
{
    auto* manager = ctx.window->findChild<ContainerManager*>();
    auto* poller = ctx.window->findChild<MeterPoller*>();
    if (manager == nullptr || poller == nullptr) {
        ctx.failure = QStringLiteral("no ContainerManager or MeterPoller");
        return;
    }
    const QHash<int, double> values{{MeterBinding::TxPower, kTxForwardWatts},
                                    {MeterBinding::TxSwr, kTxSwr},
                                    {MeterBinding::TxAlc, kTxAlcDb}};
    for (const QString& id : hostIds) {
        ContainerContentHost* host = manager->contentHost(id);
        if (host == nullptr) {
            ctx.failure = QStringLiteral("no %1 container").arg(id);
            return;
        }
        QList<QWidget*> instruments;
        for (NeedleInstrument* n : host->findChildren<NeedleInstrument*>()) {
            instruments.append(n);
        }
        for (BarInstrument* b : host->findChildren<BarInstrument*>()) {
            instruments.append(b);
        }
        for (QWidget* w : instruments) {
            const int binding = w->property("longpathPrimaryBinding").toInt();
            const QJsonObject context = w->property("containerSourceContext").toJsonObject();
            emit poller->readingUpdated(context, binding, values.value(binding));
            ctx.fedReadings.append(QJsonObject{
                {QStringLiteral("instrument"), QString::fromLatin1(w->metaObject()->className())},
                {QStringLiteral("entry"), id},
                {QStringLiteral("context"), QStringLiteral("TX (every context)")},
                {QStringLiteral("binding"), binding},
                {QStringLiteral("values"), QJsonArray{values.value(binding)}},
                {QStringLiteral("seam"), consumerSeam(poller)}});
            ctx.notes.append(QStringLiteral("%1 %2: readingUpdated binding %3 = %4, as MeterPoller "
                                            "publishes a transmit reading to every context")
                                 .arg(id, QString::fromLatin1(w->metaObject()->className()))
                                 .arg(binding).arg(values.value(binding)));
        }
    }
}

// The TX profile selector reads MicProfileManager, which is scoped by the
// radio's MAC and stays empty until a radio connects. The scene gives it a
// placeholder scope in this process's throwaway test profile and runs its
// own first-launch load(), which writes "Default" (active) and the factory
// profiles there, as the first connect would. No radio, Core or real
// settings are reached.
QJsonObject seedTxProfiles(SceneContext& ctx)
{
    MicProfileManager* profiles = ctx.model->micProfileManager();
    if (profiles == nullptr || !profiles->profileNames().isEmpty()) {
        return {};
    }
    profiles->setMacAddress(QString::fromLatin1(kPreviewTxProfileScope));
    profiles->load();
    ctx.notes.append(QStringLiteral("TX profiles seeded in the test profile (see tx_profile_seed)"));
    return QJsonObject{
        {QStringLiteral("scope"), QString::fromLatin1(kPreviewTxProfileScope)},
        {QStringLiteral("method"), QStringLiteral(
             "MicProfileManager::setMacAddress(placeholder) then load() (its first-launch seed) in the "
             "isolated test profile; no radio connected")},
        {QStringLiteral("profiles"), QJsonArray::fromStringList(profiles->profileNames())},
        {QStringLiteral("active"), profiles->activeProfileName()}};
}

void sceneTxProcessing(SceneContext& ctx)
{
    // Display-only transmit state, as baseline-tx. Refuse to run if a
    // connection exists: these seams must never reach a radio.
    if (ctx.model->connection() != nullptr) {
        ctx.failure = QStringLiteral("a RadioConnection exists; refusing the transmit scene");
        return;
    }
    ctx.provenance = provProposed();
    ctx.proposedValues = proposedEdgeValues();
    ctx.extraCrops = addAllStripCrops;
    const QStringList meterHosts = {QStringLiteral("preview-tx-power"), QStringLiteral("preview-swr"),
                                    QStringLiteral("preview-alc")};
    QJsonObject txProfileSeed;
    runWindowScene(ctx, twoBandPoses(), 0, [&txProfileSeed](SceneContext& c) {
        if (!registerPreviewApplets(c)) {
            return;
        }
        if (!commitScene(c, HeaderMode::Shown, [](WorkspaceDocument& d, ContainerManager* m) {
                placeMain(d, DockEdge::Left);
                takeFromMain(d, {QStringLiteral("applet:TX"), QStringLiteral("applet:pure_signal"),
                                 QStringLiteral("applet:mod_monitor")});
                d.containers.append(makeSceneContainer(
                    m, QStringLiteral("TX Processing"), DockEdge::Right, ContentLayout::VerticalStack,
                    {{QStringLiteral("applet:TX"), {}},
                     {QString::fromLatin1(kPreviewEqType), {}},
                     {QStringLiteral("applet:pure_signal"), {}},
                     {QStringLiteral("applet:mod_monitor"), {}}}));
                const QString needle = QString::fromLatin1(LongpathContent::kNeedleType);
                const QString bar = QString::fromLatin1(LongpathContent::kBarType);
                d.containers.append(makeSceneContainer(m, QStringLiteral("TX Power"), DockEdge::Top,
                    ContentLayout::VerticalStack, {{needle, {}, {}, {}, bindingConfig(MeterBinding::TxPower)}}));
                d.containers.append(makeSceneContainer(m, QStringLiteral("SWR"), DockEdge::Top,
                    ContentLayout::VerticalStack, {{needle, {}, {}, {}, bindingConfig(MeterBinding::TxSwr)}}));
                d.containers.append(makeSceneContainer(m, QStringLiteral("ALC"), DockEdge::Top,
                    ContentLayout::VerticalStack, {{bar, {}, {}, {}, bindingConfig(MeterBinding::TxAlc)}}));
                d.arrangement.strips[DockEdge::Top].thickness = kDxTopStripPx;
            })) {
            return;
        }
        c.notes.append(QStringLiteral("Main Panel in the left strip; TX, PureSignal and Mod Monitor moved "
                                      "to the right strip with EQ; TX Power, SWR and ALC on top"));
        txProfileSeed = seedTxProfiles(c);
        poseTxBeforeFreeze(c);
    }, [meterHosts](SceneContext& c) {
        poseTxAfterFeed(c);
        poseLongpathTx(c, meterHosts);
        pump(kFinalSettleMs);
    });
    if (!ctx.failure.isEmpty()) {
        return;
    }
    auto* tx = ctx.window->findChild<TxApplet*>();
    if (tx == nullptr) {
        ctx.failure = QStringLiteral("no TxApplet");
        return;
    }
    ctx.notes.append(QStringLiteral("TxApplet TX profile selector: %1")
                         .arg(tx->profileCombo() != nullptr && tx->profileCombo()->isVisible()
                                  ? QStringLiteral("visible, \"%1\"").arg(tx->profileCombo()->currentText())
                                  : QStringLiteral("not visible")));
    noteStrips(ctx);
    captureWindow(ctx, QStringLiteral("tx-processing"));
    for (CapturedImage& img : ctx.images) {
        if (img.name == QLatin1String("tx-processing") && !txProfileSeed.isEmpty()) {
            img.extra[QStringLiteral("tx_profile_seed")] = txProfileSeed;
        }
    }
    captureStripsFull(ctx, QStringLiteral("tx-processing"));
    // The CFC and TX EQ dialogs over the same state, alone.
    auto* action = ctx.window->findChild<QAction*>(QStringLiteral("toolsTxEqualizer"));
    if (action == nullptr) {
        ctx.failure = QStringLiteral("no toolsTxEqualizer action");
        return;
    }
    action->trigger();
    TxEqDialog* eq = visibleTopLevel<TxEqDialog>();
    captureDialog(ctx, eq, QStringLiteral("tx-processing--tx-eq"));
    if (eq != nullptr) {
        eq->close();
    }
    tx->requestOpenCfcDialog();
    TxCfcDialog* cfc = visibleTopLevel<TxCfcDialog>();
    captureDialog(ctx, cfc, QStringLiteral("tx-processing--cfc"));
    if (cfc != nullptr) {
        cfc->close();
    }
    annotateSlices(ctx);
}

QListWidgetItem* rowContaining(QListWidget* list, const QString& text)
{
    for (int i = 0; list != nullptr && i < list->count(); ++i) {
        if (list->item(i)->text().contains(text, Qt::CaseInsensitive)) {
            return list->item(i);
        }
    }
    return nullptr;
}

QJsonArray listTexts(QListWidget* list)
{
    QJsonArray out;
    for (int i = 0; list != nullptr && i < list->count(); ++i) {
        out.append(list->item(i)->text());
    }
    return out;
}

// What the entry property editor shows: its labels, the Source combo and
// whether the advanced group is enabled.
void noteEditor(SceneContext& ctx, ContainerSettingsDialog* dialog)
{
    QStringList labels;
    for (QLabel* l : dialog->findChildren<QLabel*>()) {
        if (l->isVisible() && !l->text().isEmpty()) {
            labels.append(l->text());
        }
    }
    ctx.notes.append(QStringLiteral("dialog labels: %1").arg(labels.join(QStringLiteral(" | "))));
    if (auto* source = dialog->findChild<QComboBox*>(QStringLiteral("contentSlice"))) {
        QStringList items;
        for (int i = 0; i < source->count(); ++i) {
            items.append(source->itemText(i));
        }
        ctx.notes.append(QStringLiteral("Source combo (contentSlice): %1, effectively %2, items [%3], current \"%4\"")
                             .arg(source->isVisible() ? QStringLiteral("shown") : QStringLiteral("not shown"))
                             .arg(source->isEnabled() ? QStringLiteral("enabled") : QStringLiteral("disabled"))
                             .arg(items.join(QStringLiteral(", ")), source->currentText()));
    } else {
        ctx.notes.append(QStringLiteral("no Source combo (contentSlice) in the dialog"));
    }
}

// The Station container (slice A on its chip) in the left strip, the Main
// Panel right, and Containers > Edit Container open on Station over the
// window; `select` poses the dialog. With `composite` off the window
// image leaves the dialog out (it would cover what the scene shows) and
// the dialog has its own image only.
// `menu`, when the pose sets it, is a popup open over the window: it is
// composited into the window image and has its own image (`name--menu`).
void sceneEditDialog(SceneContext& ctx, const QString& name,
                     const std::function<void(SceneContext&, ContainerSettingsDialog*)>& select,
                     bool composite = true, const std::shared_ptr<QPointer<QMenu>>& menu = {})
{
    ctx.provenance = provProposed();
    ctx.proposedValues = proposedEdgeValues();
    ctx.extraCrops = addAllStripCrops;
    const QString id = QStringLiteral("preview-station");
    auto dialog = std::make_shared<QPointer<ContainerSettingsDialog>>();
    runWindowScene(ctx, twoBandPoses(), 0, [](SceneContext& c) {
        commitScene(c, HeaderMode::Shown, [](WorkspaceDocument& d, ContainerManager* m) {
            placeMain(d, DockEdge::Right);
            d.containers.append(onSlice(makeSceneContainer(m, QStringLiteral("Station"), DockEdge::Left,
                                                           ContentLayout::VerticalStack, stationEntries()), 1));
        });
    }, [id, dialog, select](SceneContext& c) {
        poseStation(c, id);
        if (!c.failure.isEmpty()) {
            return;
        }
        auto* manager = c.window->findChild<ContainerManager*>();
        ContainerWidget* station = manager->container(id);
        // The Containers > Edit Container path, shown instead of exec()'d
        // so the grab needs no nested loop.
        *dialog = new ContainerSettingsDialog(station, c.window, manager);
        (*dialog)->selectDraftContainer(id);
        (*dialog)->show();
        pump(kDialogSettleMs);
        // Its default size is larger than the window; sized down to the
        // window as a user would (never below its minimum).
        const QSize fit = (*dialog)->size().boundedTo(c.window->size() - QSize(kDialogInsetPx, kDialogInsetPx))
                              .expandedTo((*dialog)->minimumSizeHint());
        c.notes.append(QStringLiteral("Container Settings default %1x%2, shown at %3x%4 to fit the %5x%6 window")
                           .arg((*dialog)->width()).arg((*dialog)->height())
                           .arg(fit.width()).arg(fit.height())
                           .arg(c.window->width()).arg(c.window->height()));
        (*dialog)->resize(fit);
        pump(kFinalSettleMs);
        (*dialog)->move(c.window->mapToGlobal(c.window->rect().center()) - (*dialog)->rect().center());
        select(c, dialog->data());
        pump(kFinalSettleMs);
        for (const QJsonValue& v : stopTimersUnder(dialog->data())) {
            c.stoppedTimers.append(v);
        }
    });
    if (!ctx.failure.isEmpty()) {
        return;
    }
    noteStrips(ctx);
    captureWindow(ctx, name);
    const bool menuOpen = menu && *menu;
    if (composite) {
        compositeAndRecrop(ctx, name, {dialog->data()});
    } else {
        ctx.notes.append(QStringLiteral("the dialog is not composited into the window image; see %1--dialog")
                             .arg(name));
        if (menuOpen) {
            compositeAndRecrop(ctx, name, {menu->data()});
        }
    }
    if (menuOpen) {
        captureDialog(ctx, menu->data(), name + QStringLiteral("--menu"));
        (*menu)->close();
    }
    captureDialog(ctx, dialog->data(), name + QStringLiteral("--dialog"));
    annotateSlices(ctx);
    if (*dialog) {
        (*dialog)->reject();
        (*dialog)->deleteLater();
    }
}

void sceneEditAddContent(SceneContext& ctx)
{
    sceneEditDialog(ctx, QStringLiteral("edit-add-content"), [](SceneContext& c, ContainerSettingsDialog* d) {
        auto* available = d->findChild<QListWidget*>(QStringLiteral("containerAvailableContents"));
        // The first Longpath instrument, by typeId: the list names them
        // under the "Longpath instruments" heading, not with a suffix.
        QListWidgetItem* longpath = nullptr;
        for (int i = 0; available != nullptr && i < available->count(); ++i) {
            if (available->item(i)->data(Qt::UserRole).toString().startsWith(QLatin1String("longpath."))) {
                longpath = available->item(i);
                break;
            }
        }
        if (longpath == nullptr) {
            c.failure = QStringLiteral("no Longpath entry in the Available list");
            return;
        }
        available->setCurrentItem(longpath);
        available->scrollToItem(longpath, QAbstractItemView::PositionAtCenter);
        c.notes.append(QStringLiteral("Available list (containerAvailableContents), first Longpath row "
                                      "selected and centred: %1")
                           .arg(QString::fromUtf8(QJsonDocument(listTexts(available)).toJson(QJsonDocument::Compact))));
    });
}

void sceneEditSourceSelect(SceneContext& ctx)
{
    // The Station header's source menu opened programmatically, as a chip
    // click opens it, over the window as ContainerWidget shows it. The dialog has its own image.
    auto menu = std::make_shared<QPointer<QMenu>>();
    sceneEditDialog(ctx, QStringLiteral("edit-source-select"), [menu](SceneContext& c, ContainerSettingsDialog* d) {
        auto* contents = d->findChild<QListWidget*>(QStringLiteral("containerDraftContents"));
        QListWidgetItem* frequency = rowContaining(contents, QStringLiteral("Frequency"));
        if (frequency == nullptr) {
            c.failure = QStringLiteral("no Frequency row in the Station contents");
            return;
        }
        contents->setCurrentItem(frequency);
        pump(kFinalSettleMs);
        noteEditor(c, d);
        auto* manager = c.window->findChild<ContainerManager*>();
        ContainerWidget* station = manager->container(QStringLiteral("preview-station"));
        WorkspaceGroupHeader* header = station != nullptr ? station->workspaceHeader() : nullptr;
        WorkspaceSourceChip* chip = header != nullptr ? header->sourceChip() : nullptr;
        if (chip == nullptr || !chip->isVisible()) {
            c.failure = QStringLiteral("no visible source chip on the Station header");
            return;
        }
        // The menu the chip's click opens (ContainerWidget::showSourceMenu),
        // shown with popup() so the grab needs no nested loop.
        *menu = new QMenu(station);
        station->populateSourceMenu(**menu);
        tagProvenance(menu->data(), provProposed());
        (*menu)->popup(chip->mapToGlobal(QPoint(0, chip->height())));
        pump(kFinalSettleMs);
        if (!(*menu)->isVisible()) {
            c.failure = QStringLiteral("the source menu did not show");
            return;
        }
        QStringList items;
        for (QAction* a : (*menu)->actions()) {
            if (a->isSeparator()) {
                items.append(QStringLiteral("-"));
                continue;
            }
            items.append(QStringLiteral("%1%2%3").arg(a->text(),
                a->isChecked() ? QStringLiteral(" (checked)") : QString(),
                a->isEnabled() ? QString() : QStringLiteral(" (disabled)")));
        }
        c.notes.append(QStringLiteral("source chip \"%1\": menu opened programmatically, as a chip click "
                                      "opens it, below the chip, items [%2]. "
                                      "A choice commits the container source as Container Settings' "
                                      "Slice combo does").arg(chip->text(), items.join(QStringLiteral(", "))));
    }, false, menu);
}

void sceneEditProperties(SceneContext& ctx)
{
    sceneEditDialog(ctx, QStringLiteral("edit-properties"), [](SceneContext& c, ContainerSettingsDialog* d) {
        auto* contents = d->findChild<QListWidget*>(QStringLiteral("containerDraftContents"));
        QListWidgetItem* needle = rowContaining(contents, QStringLiteral("Needle"));
        if (needle == nullptr) {
            c.failure = QStringLiteral("no Needle row in the Station contents");
            return;
        }
        contents->setCurrentItem(needle);
        pump(kFinalSettleMs);
        c.notes.append(QStringLiteral("Station contents: %1, \"%2\" selected")
                           .arg(QString::fromUtf8(QJsonDocument(listTexts(contents)).toJson(QJsonDocument::Compact)),
                                needle->text()));
        noteEditor(c, d);
        QStringList properties;
        for (QWidget* w : d->findChildren<QWidget*>()) {
            if (w->objectName().startsWith(QStringLiteral("property_")) && w->isVisible()) {
                QString value;
                if (auto* combo = qobject_cast<QComboBox*>(w)) {
                    value = combo->currentText();
                } else if (auto* check = qobject_cast<QCheckBox*>(w)) {
                    value = check->isChecked() ? QStringLiteral("on") : QStringLiteral("off");
                } else if (auto* edit = qobject_cast<QLineEdit*>(w)) {
                    value = edit->text();
                }
                properties.append(QStringLiteral("%1 \"%2\"").arg(w->objectName(), value));
            }
        }
        c.notes.append(QStringLiteral("declared property controls: %1")
                           .arg(properties.isEmpty() ? QStringLiteral("none shown") : properties.join(QStringLiteral(", "))));
    });
}

// ---- added copies of the SIGNAL STRENGTH dial (workspace preview) ----------

// The added dial copies in the window (never the Main Panel's dial).
QList<SMeterWidget*> dialCopies(SceneContext& ctx)
{
    QList<SMeterWidget*> out;
    for (SMeterWidget* meter : allMeters(ctx)) {
        if (meter->settingsStore() == SMeterWidget::SettingsStore::Instance) {
            out.append(meter);
        }
    }
    return out;
}

// The id of the container holding `w`, less the "preview-" prefix.
QString holderName(QWidget* w)
{
    for (QWidget* p = w; p != nullptr; p = p->parentWidget()) {
        if (auto* c = qobject_cast<ContainerWidget*>(p)) {
            QString id = c->id();
            id.remove(QStringLiteral("preview-"));
            return id;
        }
    }
    return QStringLiteral("unhoused");
}

// Feeds every added dial the reading MeterPoller would send for its own
// receiver: the slice's posed level after a peak kFedPeakAboveDb above it,
// or no reading when the receiver its source names is closed. The poller
// is stopped (freeze); readingUpdated reaches the dial's registered
// reading consumer. Display-only: no radio or Core.
void feedDials(SceneContext& ctx)
{
    auto* poller = ctx.window->findChild<MeterPoller*>();
    if (poller == nullptr) {
        ctx.failure = QStringLiteral("no MeterPoller");
        return;
    }
    for (SMeterWidget* dial : dialCopies(ctx)) {
        const QJsonObject context = dial->property("containerSourceContext").toJsonObject();
        const int sliceId = context.contains(QStringLiteral("sliceId"))
                                ? context.value(QStringLiteral("sliceId")).toInt()
                                : context.value(QStringLiteral("rxSource")).toInt() - 1;
        SliceModel* slice = sliceId >= 0 ? ctx.model->sliceById(sliceId) : nullptr;
        QJsonObject fed{{QStringLiteral("instrument"), QStringLiteral("SMeterWidget (added copy)")},
                        {QStringLiteral("entry"), dial->property("containerEntryId").toString()},
                        {QStringLiteral("binding"), QStringLiteral("RX: Signal Peak (RX mode Signal)")},
                        {QStringLiteral("seam"), consumerSeam(poller)}};
        if (slice == nullptr) {
            emit poller->readingUpdated(context, MeterBinding::SignalPeak, kNoMeterReadingDbm);
            fed.insert(QStringLiteral("slice"), QStringLiteral("closed (source %1)")
                                                    .arg(QString::fromUtf8(QJsonDocument(context).toJson(QJsonDocument::Compact))));
            fed.insert(QStringLiteral("values_dbm"), QJsonArray{kNoMeterReadingDbm});
            ctx.fedReadings.append(fed);
            continue;
        }
        const double level = slice->signalStrengthDbm();
        emit poller->readingUpdated(context, MeterBinding::SignalPeak, level + kFedPeakAboveDb);
        emit poller->readingUpdated(context, MeterBinding::SignalPeak, level);
        fed.insert(QStringLiteral("slice"), QString(QChar('A' + sliceId)));
        fed.insert(QStringLiteral("values_dbm"), QJsonArray{level + kFedPeakAboveDb, level});
        ctx.fedReadings.append(fed);
    }
}

// The transmit readings for every added dial through its reading consumer
// (85 W, SWR 1.25, mic -12 dB, as poseTxAfterFeed poses the Main Panel's
// dial directly). Display-only.
void feedDialsTx(SceneContext& ctx)
{
    auto* poller = ctx.window->findChild<MeterPoller*>();
    if (poller == nullptr) {
        ctx.failure = QStringLiteral("no MeterPoller");
        return;
    }
    for (SMeterWidget* dial : dialCopies(ctx)) {
        const QJsonObject context = dial->property("containerSourceContext").toJsonObject();
        emit poller->readingUpdated(context, MeterBinding::TxPower, kTxForwardWatts);
        emit poller->readingUpdated(context, MeterBinding::TxSwr, kTxSwr);
        emit poller->readingUpdated(context, MeterBinding::TxMic, kTxMicDb);
        ctx.fedReadings.append(QJsonObject{
            {QStringLiteral("instrument"), QStringLiteral("SMeterWidget (added copy)")},
            {QStringLiteral("entry"), dial->property("containerEntryId").toString()},
            {QStringLiteral("binding"), QStringLiteral("TX: Power, SWR, Mic")},
            {QStringLiteral("values"), QJsonArray{kTxForwardWatts, kTxSwr, kTxMicDb}},
            {QStringLiteral("seam"), consumerSeam(poller)}});
    }
}

// A crop of each added dial, named for its container.
void addDialCrops(SceneContext& ctx, QVector<CropRect>& crops, double dpr)
{
    QStringList used;
    for (SMeterWidget* dial : dialCopies(ctx)) {
        QString name = QStringLiteral("dial-") + holderName(dial);
        for (int n = 2; used.contains(name); ++n) {
            name = QStringLiteral("dial-%1-%2").arg(holderName(dial)).arg(n);
        }
        used.append(name);
        addCrop(crops, name, ctx.window, dial, dpr);
    }
}

void addStripAndDialCrops(SceneContext& ctx, QVector<CropRect>& crops, double dpr)
{
    addStripCrops(ctx, crops, dpr);
    addDialCrops(ctx, crops, dpr);
}

void noteDials(SceneContext& ctx)
{
    for (SMeterWidget* dial : dialCopies(ctx)) {
        ctx.notes.append(QStringLiteral("dial copy %1 in %2: %3x%4, RX reads %5")
                             .arg(dial->property("containerEntryId").toString(), holderName(dial))
                             .arg(dial->width()).arg(dial->height())
                             .arg(dial->sUnitsText()));
    }
    ctx.notes.append(QStringLiteral("added dial copies follow RadioModel::transmittingChanged; a scene "
                                    "cannot key, so a transmitting pose sets it display-only"));
}

// One dial scene: the two-band data, `edit` committed (every header
// `header`), the dials fed, then `pose`.
void sceneDialWindow(SceneContext& ctx, const QString& name, HeaderMode header,
                     const std::function<void(WorkspaceDocument&, ContainerManager*)>& edit,
                     const std::function<void(SceneContext&)>& beforeFreeze = {},
                     const std::function<void(SceneContext&)>& pose = {})
{
    ctx.provenance = provProposed();
    ctx.proposedValues = proposedEdgeValues();
    ctx.extraCrops = addStripAndDialCrops;
    runWindowScene(ctx, twoBandPoses(), 0, [header, edit, beforeFreeze](SceneContext& c) {
        if (!commitScene(c, header, edit)) {
            return;
        }
        if (beforeFreeze) {
            beforeFreeze(c);
        }
    }, [pose](SceneContext& c) {
        feedDials(c);
        if (pose) {
            pose(c);
        }
        pump(kFinalSettleMs);
    });
    if (!ctx.failure.isEmpty()) {
        return;
    }
    noteStrips(ctx);
    noteDials(ctx);
    captureWindow(ctx, name);
    annotateSlices(ctx);
}

void sceneDialTwoBand(SceneContext& ctx, const QString& name)
{
    sceneProfile(ctx, name, SceneProfile::Dx, [](SceneContext& c) {
        feedDials(c);
        c.extraCrops = addStripAndDialCrops;
        noteDials(c);
    }, {}, false, QString::fromLatin1(SMeterDialContent::kDialType));
}

// Station (as mixed-container) with a dial after the Thetis S-meter bar.
void sceneDialMixed(SceneContext& ctx)
{
    sceneDialWindow(ctx, QStringLiteral("dial-addable--mixed"), HeaderMode::Shown,
                    [](WorkspaceDocument& d, ContainerManager* m) {
        placeMain(d, DockEdge::Right);
        QVector<EdgeEntry> entries = stationEntries();
        entries.insert(3, {QString::fromLatin1(SMeterDialContent::kDialType), {}, {}, rxSource(1)});
        d.containers.append(makeSceneContainer(m, QStringLiteral("Station"), DockEdge::Left,
                                               ContentLayout::VerticalStack, entries));
    }, {}, [](SceneContext& c) { poseStation(c, QStringLiteral("preview-station")); });
}

// A dial for RX B in a narrow right strip with B's frequency, as the FT8
// profile's strip; the Main Panel left; headers Hidden.
void sceneDialNarrow(SceneContext& ctx)
{
    sceneDialWindow(ctx, QStringLiteral("dial-addable--narrow"), HeaderMode::Hidden,
                    [](WorkspaceDocument& d, ContainerManager* m) {
        placeMain(d, DockEdge::Left);
        d.containers.append(onSlice(makeSceneContainer(
            m, QStringLiteral("Dial B"), DockEdge::Right, ContentLayout::VerticalStack,
            {{QString::fromLatin1(LongpathContent::kFrequencyType), {}, {}, rxSource(2)},
             {QString::fromLatin1(SMeterDialContent::kDialType), {}, {}, rxSource(2)}}), 2));
        d.arrangement.strips[DockEdge::Right].thickness = EdgeDockStrip::kSideThicknessPx;
    });
}

// Dials for RX A and RX B on top while slice A transmits (display-only).
void sceneDialTx(SceneContext& ctx, const QString& name)
{
    sceneDialWindow(ctx, name, HeaderMode::Shown,
                    [](WorkspaceDocument& d, ContainerManager* m) {
        placeMain(d, DockEdge::Right);
        const QString dial = QString::fromLatin1(SMeterDialContent::kDialType);
        d.containers.append(onSlice(makeSceneContainer(m, QStringLiteral("Signal A"), DockEdge::Top,
                                                       ContentLayout::VerticalStack, {{dial, {}, {}, rxSource(1)}}), 1));
        d.containers.append(onSlice(makeSceneContainer(m, QStringLiteral("Signal B"), DockEdge::Top,
                                                       ContentLayout::VerticalStack, {{dial, {}, {}, rxSource(2)}}), 2));
        d.arrangement.strips[DockEdge::Top].thickness = kDxTopStripPx;
    }, poseTxBeforeFreeze, [](SceneContext& c) {
        poseTxAfterFeed(c);
        feedDialsTx(c);
    });
}

// A Power Genius XL status report as RadioModel takes it from the amp's
// connection (RadioModel::onPgxlStatus), with no device and no connection:
// display-only. The radio is not keyed (MoxController stays on receive).
void reportPgxlStatus(SceneContext& c, const QMap<QString, QString>& kvs)
{
    using StatusMap = QMap<QString, QString>;
    if (!QMetaObject::invokeMethod(c.model, "onPgxlStatus", Qt::DirectConnection,
                                   Q_ARG(StatusMap, kvs))) {
        c.failure = QStringLiteral("RadioModel::onPgxlStatus not reachable");
        return;
    }
    QStringList pairs;
    for (auto it = kvs.cbegin(); it != kvs.cend(); ++it) {
        pairs.append(it.key() + QLatin1Char('=') + it.value());
    }
    c.notes.append(QStringLiteral("RadioModel::onPgxlStatus(%1) (no device)").arg(pairs.join(QLatin1Char(' '))));
}

// The power the amp scene's Power Genius XL reports: 1200 W (60.79 dBm),
// return loss 20 dB (SWR 1.22).
constexpr auto kAmpPeakFwdDbm = "60.7918";
constexpr auto kAmpReturnLossDb = "-20";

// A dial for RX A while slice A transmits through a Power Genius XL in
// OPERATE (display-only): the copy on the amplifier's 2 kW scale with its
// watts, as the Main Panel's dial.
void sceneDialAmp(SceneContext& ctx)
{
    sceneDialWindow(ctx, QStringLiteral("dial-addable--amp"), HeaderMode::Shown,
                    [](WorkspaceDocument& d, ContainerManager* m) {
        placeMain(d, DockEdge::Right);
        const QString dial = QString::fromLatin1(SMeterDialContent::kDialType);
        d.containers.append(onSlice(makeSceneContainer(m, QStringLiteral("Signal A"), DockEdge::Top,
                                                       ContentLayout::VerticalStack, {{dial, {}, {}, rxSource(1)}}), 1));
        d.arrangement.strips[DockEdge::Top].thickness = kDxTopStripPx;
    }, poseTxBeforeFreeze, [](SceneContext& c) {
        poseTxAfterFeed(c);
        feedDialsTx(c);
        // The amp reports after the radio's own 85 W, as it does on air.
        reportPgxlStatus(c, {{QStringLiteral("state"), QStringLiteral("OPERATE")},
                             {QStringLiteral("peakfwd"), QString::fromLatin1(kAmpPeakFwdDbm)},
                             {QStringLiteral("swr"), QString::fromLatin1(kAmpReturnLossDb)}});
        for (SMeterWidget* dial : dialCopies(c)) {
            c.notes.append(QStringLiteral("dial copy %1: 2 kW scale %2, TX value %3 W")
                               .arg(dial->property("containerEntryId").toString())
                               .arg(dial->txScaleIs2kWForTesting() ? QStringLiteral("on") : QStringLiteral("off"))
                               .arg(dial->testTxValue(), 0, 'f', 0));
        }
    });
}

// Two dials side by side: "Setup face" has no face of its own and follows
// Setup's Meter Styles face, set to Blackface the way the page sets it (the
// page writes SMeter_FaceStyle; MainWindow::wireSetupDialog passes it to the
// Main Panel's dial); "Own face" has VU Amber picked from its own menu.
void sceneDialFace(SceneContext& ctx)
{
    sceneDialWindow(ctx, QStringLiteral("dial-addable--face"), HeaderMode::Shown,
                    [](WorkspaceDocument& d, ContainerManager* m) {
        placeMain(d, DockEdge::Right);
        const QString dial = QString::fromLatin1(SMeterDialContent::kDialType);
        const QJsonObject own{{QLatin1String(SMeterDialContent::kFaceKey),
                               SMeterWidget::faceStyleKey(SMeterWidget::FaceStyle::VuAmber)}};
        d.containers.append(onSlice(makeSceneContainer(m, QStringLiteral("Setup face"), DockEdge::Top,
                                                       ContentLayout::VerticalStack, {{dial, {}, {}, rxSource(1)}}), 1));
        d.containers.append(onSlice(makeSceneContainer(m, QStringLiteral("Own face"), DockEdge::Top,
                                                       ContentLayout::VerticalStack, {{dial, {}, {}, rxSource(2), own}}), 2));
        d.arrangement.strips[DockEdge::Top].thickness = kDxTopStripPx;
    }, {}, [](SceneContext& c) {
        MeterStylesPage page(c.model);
        auto* combo = page.findChild<QComboBox*>(QStringLiteral("sMeterFaceCombo"));
        SMeterWidget* mainPanelDial = nullptr;
        for (SMeterWidget* meter : allMeters(c)) {
            if (meter->settingsStore() == SMeterWidget::SettingsStore::Global) {
                mainPanelDial = meter;
            }
        }
        if (combo == nullptr || mainPanelDial == nullptr) {
            c.failure = QStringLiteral("no Meter Styles face combo or no Main Panel dial");
            return;
        }
        QObject::connect(&page, &MeterStylesPage::sMeterFaceChanged, mainPanelDial, [mainPanelDial](int face) {
            mainPanelDial->setFaceStyle(static_cast<SMeterWidget::FaceStyle>(face));
        });
        combo->setCurrentIndex(combo->findData(static_cast<int>(SMeterWidget::FaceStyle::Blackface)));
        c.notes.append(QStringLiteral("Setup > Appearance > Meter Styles: Face %1 (SMeter_FaceStyle=%2)")
                           .arg(combo->currentText(), AppSettings::instance().value(QStringLiteral("SMeter_FaceStyle")).toString()));
        for (SMeterWidget* dial : dialCopies(c)) {
            c.notes.append(QStringLiteral("dial copy %1: face %2, follows Setup %3")
                               .arg(dial->property("containerEntryId").toString(),
                                    SMeterWidget::faceStyleLabel(dial->faceStyle()),
                                    dial->followsSetupFace() ? QStringLiteral("yes") : QStringLiteral("no")));
        }
    });
}

// A dial for RX A beside one whose source is RX C, which the two-band
// station has not opened.
void sceneDialClosed(SceneContext& ctx)
{
    sceneDialWindow(ctx, QStringLiteral("dial-addable--closed"), HeaderMode::Shown,
                    [](WorkspaceDocument& d, ContainerManager* m) {
        placeMain(d, DockEdge::Right);
        const QString dial = QString::fromLatin1(SMeterDialContent::kDialType);
        d.containers.append(onSlice(makeSceneContainer(m, QStringLiteral("Signal A"), DockEdge::Top,
                                                       ContentLayout::VerticalStack, {{dial, {}, {}, rxSource(1)}}), 1));
        d.containers.append(onSlice(makeSceneContainer(m, QStringLiteral("Signal C"), DockEdge::Top,
                                                       ContentLayout::VerticalStack, {{dial, {}, {}, rxSource(3)}}), 3));
        d.arrangement.strips[DockEdge::Top].thickness = kDxTopStripPx;
    });
}

// Container Settings' Available list with the dial's row selected, and a
// crop of the list from the dialog image.
void sceneDialAddContent(SceneContext& ctx)
{
    const QString name = QStringLiteral("dial-addable--add-content");
    auto listRect = std::make_shared<QRect>();
    sceneEditDialog(ctx, name, [listRect](SceneContext& c, ContainerSettingsDialog* d) {
        auto* available = d->findChild<QListWidget*>(QStringLiteral("containerAvailableContents"));
        QListWidgetItem* dial = rowContaining(available, QStringLiteral("S-Meter dial"));
        if (dial == nullptr) {
            c.failure = QStringLiteral("no S-Meter dial row in the Available list");
            return;
        }
        available->setCurrentItem(dial);
        available->scrollToItem(dial, QAbstractItemView::PositionAtCenter);
        *listRect = QRect(available->mapTo(d, QPoint(0, 0)), available->size());
        c.notes.append(QStringLiteral("Available list (containerAvailableContents), \"%1\" selected and centred: %2")
                           .arg(dial->text(),
                                QString::fromUtf8(QJsonDocument(listTexts(available)).toJson(QJsonDocument::Compact))));
    });
    if (!ctx.failure.isEmpty() || !ctx.crops || listRect->isEmpty()) {
        return;
    }
    const QString dialogName = name + QStringLiteral("--dialog");
    for (const CapturedImage& img : ctx.images) {
        if (img.name != dialogName) {
            continue;
        }
        CapturedImage full = img;
        full.crops.clear();
        const QRect clipped = listRect->intersected(QRect(QPoint(0, 0), img.logicalSize));
        full.crops.append({QStringLiteral("available-list"), clipped, deviceRect(clipped, img.dpr)});
        appendCropImages(ctx, full, dialogName);
        return;
    }
}

// four-rx-look: four compact frequencies left, four Longpath S-meter bars
// bottom, the Main Panel right.
void sceneFourRxLook(SceneContext& ctx)
{
    ctx.provenance = provProposed();
    ctx.proposedValues = proposedEdgeValues();
    ctx.extraCrops = addAllStripCrops;
    runWindowScene(ctx, fourRxPoses(), 2, [](SceneContext& c) {
        commitScene(c, HeaderMode::Shown, [](WorkspaceDocument& d, ContainerManager* m) {
            placeMain(d, DockEdge::Right);
            const QString freq = QString::fromLatin1(LongpathContent::kFrequencyType);
            const QString bar = QString::fromLatin1(LongpathContent::kBarType);
            for (int rx = 1; rx <= 4; ++rx) {
                const QString letter(QChar('A' + rx - 1));
                d.containers.append(onSlice(makeSceneContainer(
                    m, QStringLiteral("Frequency %1").arg(letter), DockEdge::Left, ContentLayout::VerticalStack,
                    {{freq, {}, {}, rxSource(rx)}}), rx));
            }
            for (int rx = 1; rx <= 4; ++rx) {
                const QString letter(QChar('A' + rx - 1));
                d.containers.append(onSlice(makeSceneContainer(
                    m, QStringLiteral("S-Meter %1").arg(letter), DockEdge::Bottom, ContentLayout::VerticalStack,
                    {{bar, {}, {}, rxSource(rx), bindingConfig(MeterBinding::SignalPeak)}}), rx));
            }
        });
    }, [](SceneContext& c) {
        auto* area = c.window->findChild<EdgeDockArea*>();
        feedSliceInstruments(c, area != nullptr ? area->strip(DockEdge::Bottom) : nullptr);
        pump(kFinalSettleMs);
    });
    if (!ctx.failure.isEmpty()) {
        return;
    }
    ctx.notes.append(QStringLiteral(
        "S-Meter A to D (Longpath bars, RX: Signal Peak): each fed its slice's posed level through "
        "MeterPoller::readingUpdated, as the poller sends with a radio (see fed_readings)"));
    noteStrips(ctx);
    captureWindow(ctx, QStringLiteral("four-rx-look"));
    annotateSlices(ctx);
}


// ---- live scroll bars (controls-sheet--scrollbars) ----

QJsonObject barFacts(QScrollArea* area)
{
    QScrollBar* v = area->verticalScrollBar();
    return QJsonObject{
        {QStringLiteral("area_class"), QString::fromLatin1(area->metaObject()->className())},
        {QStringLiteral("area_parent"), area->parentWidget() != nullptr
             ? QString::fromLatin1(area->parentWidget()->metaObject()->className()) : QString()},
        {QStringLiteral("area_size"), QJsonArray{area->width(), area->height()}},
        {QStringLiteral("viewport_width"), area->viewport()->width()},
        {QStringLiteral("vertical_bar_visible"), v->isVisible()},
        {QStringLiteral("vertical_bar_width"), v->isVisible() ? v->width() : 0},
        {QStringLiteral("vertical_bar_class"), QString::fromLatin1(v->metaObject()->className())},
        {QStringLiteral("pm_scroll_bar_extent"), v->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, v)},
    };
}

// The Main Panel's scroll areas in the live window.
QJsonArray mainPanelBars(MainWindow* window)
{
    QJsonArray out;
    auto* manager = window->findChild<ContainerManager*>();
    QWidget* panel = manager != nullptr ? manager->panelContainer() : nullptr;
    if (panel == nullptr) {
        return out;
    }
    for (QScrollArea* area : panel->findChildren<QScrollArea*>()) {
        if (area->isVisible() && area->widget() != nullptr) {
            out.append(barFacts(area));
        }
    }
    return out;
}

// The first Setup page, in tree order, whose page scroll area shows a
// vertical bar. Leaves the dialog open on that page; returns its area.
QScrollArea* setupPageWithBar(SetupDialog* dialog, QJsonObject& facts)
{
    auto* tree = dialog->findChild<QTreeWidget*>();
    if (tree == nullptr) {
        return nullptr;
    }
    for (const SetupLeaf& leaf : setupLeaves(dialog)) {
        if (leaf.hidden) {
            continue;
        }
        QTreeWidgetItem* item = treeItemForIndex(tree, leaf.index);
        if (item == nullptr) {
            continue;
        }
        tree->setCurrentItem(item);
        pump(kFinalSettleMs);
        for (SetupPage* page : dialog->findChildren<SetupPage*>()) {
            if (!page->isVisible()) {
                continue;
            }
            for (QScrollArea* area : page->findChildren<QScrollArea*>()) {
                if (area->isVisible() && area->parentWidget() == page && area->verticalScrollBar()->isVisible()) {
                    facts = barFacts(area);
                    facts.insert(QStringLiteral("page"), leaf.category + QStringLiteral(" / ") + leaf.label);
                    return area;
                }
            }
        }
    }
    return nullptr;
}

// Today's look, then the Setup page again with the preview switch on and
// the preview's scoped pass over the dialog (WorkspacePreview::themeSubtree,
// what themeOnShow runs on a dialog). The switch ends off.
QJsonObject liveScrollBars(SceneContext& ctx)
{
    QJsonObject out{{QStringLiteral("main_panel_today"), mainPanelBars(ctx.window)}};
    SetupDialog* dialog = openSetupDialog(ctx.window);
    if (dialog == nullptr) {
        out.insert(QStringLiteral("setup_error"), QStringLiteral("Setup did not open"));
        return out;
    }
    QJsonObject today;
    QScrollArea* area = setupPageWithBar(dialog, today);
    if (area == nullptr) {
        out.insert(QStringLiteral("setup_error"), QStringLiteral("no Setup page shows a vertical bar"));
    } else {
        out.insert(QStringLiteral("setup_today"), today);
        WorkspacePreview::setEnabledForTest(true);
        WorkspacePreview::themeSubtree(dialog);
        pump(kFinalSettleMs);
        QJsonObject preview = barFacts(area);
        preview.insert(QStringLiteral("page"), today.value(QStringLiteral("page")));
        out.insert(QStringLiteral("setup_preview_switch_on"), preview);
        WorkspacePreview::setEnabledForTest(false);
    }
    dialog->close();
    pump(kFinalSettleMs);
    return out;
}

// ---- every other dialog (inventory 5.2) and the standard dialogs (5.5) ----

// Longer than the 250 ms Network Diagnostics and 1000 ms Remote Diagnostics
// refresh ticks.
constexpr int kMemberTimerCutWaitMs = 1300;
constexpr auto kFrozenNetworkUptime = "14m 32s";
// The Spot Hub tabs are shown one by one; each settles before its grab.
constexpr int kTabSettleMs = 300;
// Wisdom progress as WdspEngine reports a 4096-point plan
// (estimateWisdomPercent, WdspEngine.cpp:187-211: step 7 of 14).
constexpr int kWisdomSamplePercent = 50;
// The TCI log's sample lines start at this local time on the review date.
constexpr int kTciLogYear = 2026;
constexpr int kTciLogMonth = 10;
constexpr int kTciLogDay = 8;
constexpr int kTciLogHour = 14;
constexpr int kTciLogMinute = 32;
constexpr int kTciLogSecond = 5;
constexpr int kTciLogLineGapMs = 37;
constexpr quint16 kTgxlPort = 9010;
constexpr quint16 kPgxlPort = 9008;

// Member QTimers are not children, so stopTimersUnder() cannot reach them:
// for `waitMs` (longer than the timer's interval), every QTimer whose
// timeout reaches `target` is cut as it fires and stopped.
void cutTimersInto(SceneContext& ctx, QObject* target, int waitMs)
{
    class Cut : public QObject
    {
    public:
        Cut(QObject* target, QJsonArray* stopped) : m_target(target), m_stopped(stopped) {}
        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (event->type() == QEvent::Timer) {
                if (auto* timer = qobject_cast<QTimer*>(watched);
                    timer != nullptr && timer->isActive()
                    && QObject::disconnect(timer, &QTimer::timeout, m_target, nullptr)) {
                    m_stopped->append(QJsonObject{
                        {QStringLiteral("parent"),
                         QStringLiteral("member timer feeding %1")
                             .arg(QString::fromLatin1(m_target->metaObject()->className()))},
                        {QStringLiteral("name"), timer->objectName()},
                        {QStringLiteral("interval_ms"), timer->interval()}});
                    timer->stop();
                }
            }
            return false;
        }

    private:
        QObject* m_target;
        QJsonArray* m_stopped;
    } cut(target, &ctx.stoppedTimers);
    qApp->installEventFilter(&cut);
    pump(waitMs);
    qApp->removeEventFilter(&cut);
}

// Sets the value label beside a field label ("Uptime" or "Uptime:") in any
// grid of `root`. False when there is no such field.
bool pinValueBeside(QWidget* root, const QString& field, const QString& value)
{
    for (QGridLayout* grid : root->findChildren<QGridLayout*>()) {
        for (int i = 0; i < grid->count(); ++i) {
            auto* label = qobject_cast<QLabel*>(grid->itemAt(i)->widget());
            if (label == nullptr) {
                continue;
            }
            QString text = label->text().trimmed();
            if (text.endsWith(QLatin1Char(':'))) {
                text.chop(1);
            }
            if (text != field) {
                continue;
            }
            int row = 0;
            int column = 0;
            int rowSpan = 0;
            int columnSpan = 0;
            grid->getItemPosition(i, &row, &column, &rowSpan, &columnSpan);
            QLayoutItem* beside = grid->itemAtPosition(row, column + columnSpan);
            if (auto* v = beside != nullptr ? qobject_cast<QLabel*>(beside->widget()) : nullptr) {
                v->setText(value);
                return true;
            }
        }
    }
    return false;
}

// Opens a dialog through a MainWindow slot and grabs the one it shows.
template <typename T>
T* openThroughSlot(SceneContext& ctx, MainWindow* window, const char* slot)
{
    if (!QMetaObject::invokeMethod(window, slot, Qt::DirectConnection)) {
        ctx.failure = QStringLiteral("MainWindow has no slot %1").arg(QString::fromLatin1(slot));
        return nullptr;
    }
    return visibleTopLevel<T>();
}

void noteNotShown(SceneContext& ctx, const QString& item, const QString& reason)
{
    ctx.notShown.append(QJsonObject{{QStringLiteral("item"), item},
                                    {QStringLiteral("reason"), reason}});
}

// The sample remote device lists (tst_multi_device_screens.cpp:1248-1282
// and :1305-1360 build the same shapes).
RemoteHeldList sampleHeldList()
{
    const auto entry = [](const QString& id, const QString& name, const QString& state,
                          bool replaceable) {
        RemoteHeldEntry e;
        e.device.deviceId = id;
        e.device.name = name;
        e.device.shortName = name.section(QLatin1Char(' '), -1);
        e.device.state = state;
        e.device.connectedForSeconds = 3900;
        e.device.lastActivitySeconds = 420;
        e.device.awayForSeconds = state == QLatin1String("away") ? 95 : 0;
        e.device.transmittingForSeconds = state == QLatin1String("transmitting") ? 130 : 0;
        RemoteDeviceSlice slice;
        slice.sliceId = 1;
        slice.letter = QStringLiteral("B");
        slice.band = static_cast<int>(Band::Band20m);
        slice.frequencyHz = 14074000.0;
        if (state == QLatin1String("transmitting")) {
            e.device.holdsTransmit = true;
            e.device.transmittingOn = slice;
        } else if (state == QLatin1String("listening")) {
            e.device.listeningOn = {slice};
        }
        e.replaceable = replaceable;
        return e;
    };
    RemoteHeldList held;
    held.revision = 3;
    held.placeFreedSecondsAgo = 200;
    held.receivedAt = QDateTime(QDate(kTciLogYear, kTciLogMonth, kTciLogDay),
                                QTime(kTciLogHour, kTciLogMinute, kTciLogSecond));
    held.entries = {entry(QStringLiteral("a"), QStringLiteral("Jo's iPad"), QStringLiteral("away"), true),
                    entry(QStringLiteral("h"), QStringLiteral("Shack Mac mini"), QStringLiteral("listening"), false),
                    entry(QStringLiteral("l"), QStringLiteral("Den MacBook"), QStringLiteral("listening"), true),
                    entry(QStringLiteral("t"), QStringLiteral("Jo's iPhone"), QStringLiteral("transmitting"), true)};
    return held;
}

QJsonObject sampleSlice(int id, const QString& letter, double hz, const QString& device)
{
    return QJsonObject{{QStringLiteral("sliceId"), id}, {QStringLiteral("letter"), letter},
                       {QStringLiteral("frequencyHz"), hz}, {QStringLiteral("mode"), 1},
                       {QStringLiteral("deviceName"), device}};
}

SessionPrompt sampleTakeReceiverPrompt()
{
    SessionPrompt take;
    take.id = 8;
    take.kind = QStringLiteral("takeReceiver");
    QJsonObject txSlice = sampleSlice(1, QStringLiteral("B"), 14074000.0, QStringLiteral("Jo's iPhone"));
    txSlice[QStringLiteral("txSlice")] = true;
    take.choices = QJsonArray{
        QJsonObject{{QStringLiteral("choice"), 0}, {QStringLiteral("streamIndex"), 0},
                    {QStringLiteral("takeable"), false},
                    {QStringLiteral("why"), QStringLiteral("Your panadapter already uses this receiver.")},
                    {QStringLiteral("slices"),
                     QJsonArray{sampleSlice(0, QStringLiteral("A"), 7074000.0, QStringLiteral("Shack MacBook"))}},
                    {QStringLiteral("devices"), QJsonArray{QJsonObject{
                         {QStringLiteral("name"), QStringLiteral("Shack MacBook")},
                         {QStringLiteral("state"), QStringLiteral("listening")}}}}},
        QJsonObject{{QStringLiteral("choice"), 1}, {QStringLiteral("streamIndex"), 1},
                    {QStringLiteral("takeable"), true},
                    {QStringLiteral("slices"), QJsonArray{txSlice}},
                    {QStringLiteral("devices"), QJsonArray{QJsonObject{
                         {QStringLiteral("name"), QStringLiteral("Jo's iPhone")},
                         {QStringLiteral("state"), QStringLiteral("listening")}}}}}};
    return take;
}

SessionPrompt sampleSharedSettingPrompt()
{
    SessionPrompt shared;
    shared.id = 7;
    shared.kind = QStringLiteral("sharedSetting");
    shared.change = QJsonObject{{QStringLiteral("label"), QStringLiteral("Attenuator, ADC 1")},
                                {QStringLiteral("from"), QStringLiteral("0 dB")},
                                {QStringLiteral("to"), QStringLiteral("20 dB")}};
    QJsonObject slice = sampleSlice(1, QStringLiteral("B"), 14074000.0, QString());
    slice.remove(QStringLiteral("deviceName"));
    slice[QStringLiteral("streamIndex")] = 1;
    slice[QStringLiteral("effect")] = QStringLiteral("changes");
    shared.affected = QJsonArray{QJsonObject{
        {QStringLiteral("deviceName"), QStringLiteral("Jo's iPhone")},
        {QStringLiteral("state"), QStringLiteral("listening")},
        {QStringLiteral("slices"), QJsonArray{slice}}}};
    return shared;
}

RadioInfo sampleRadio(const QString& name, const QString& mac, const QString& ip, HPSDRHW board,
                      ProtocolVersion protocol, int firmware)
{
    RadioInfo info;
    info.name = name;
    info.macAddress = mac;
    info.address = QHostAddress(ip);
    info.boardType = board;
    info.protocol = protocol;
    info.firmwareVersion = firmware;
    info.adcCount = RadioInfo::adcCountForBoard(board);
    info.maxReceivers = RadioInfo::maxReceiversForBoard(board);
    return info;
}

QList<ConnectionTargetRow> sampleConnectionRows()
{
    // The row shapes and words GuiConnectionController::refresh builds
    // (GuiConnectionController.cpp:420-475) and lanCoreRow (:581-633).
    QList<ConnectionTargetRow> rows;
    rows.append({QStringLiteral("local"), ConnectionTargetKind::LocalRadio,
                 QStringLiteral("This computer's Core"), QStringLiteral("Choose a local radio"),
                 QStringLiteral("This computer"), QStringLiteral("Available"), true, false, false});
    rows.append({QStringLiteral("radio:00:1C:C0:4E:21:0A"), ConnectionTargetKind::LocalRadio,
                 QStringLiteral("ANAN-G2"), QStringLiteral("This computer's Core"),
                 QStringLiteral("192.168.1.20"), QStringLiteral("Available"), true, true, true});
    rows.append({QStringLiteral("radio:00:1C:C0:7B:90:3C"), ConnectionTargetKind::LocalRadio,
                 QStringLiteral("Hermes Lite 2"), QStringLiteral("This computer's Core"),
                 QStringLiteral("192.168.1.31"), QStringLiteral("Saved / not seen"), true, true, true});
    rows.append({QStringLiteral("saved:hilltop"), ConnectionTargetKind::SavedCore,
                 QStringLiteral("K1ABC Hilltop Core"), QStringLiteral("ANAN-7000DLE"),
                 QStringLiteral("192.168.1.40:4433"), QStringLiteral("Saved, ready to connect"),
                 true, true, true});
    ConnectionTargetRow lan{QStringLiteral("lan:shack"), ConnectionTargetKind::LanCore,
                            QStringLiteral("Shack Core (advertised)"),
                            QStringLiteral("ANAN-G2 (advertised online)"),
                            QStringLiteral("192.168.1.52:4433"), QStringLiteral("Not paired"),
                            false, false, false};
    lan.pairable = true;
    rows.append(lan);
    return rows;
}

SavedCoreTarget sampleSavedCore()
{
    SavedCoreTarget target;
    target.id = QStringLiteral("hilltop");
    target.label = QStringLiteral("K1ABC Hilltop Core");
    target.connection.url = QStringLiteral("wss://192.168.1.40:4433");
    target.connection.token = QStringLiteral("q7Lw2Rk9Tz4Hn8Vb");
    target.connection.fingerprint =
        QStringLiteral("3F:9A:C1:57:0B:E2:48:D6:91:7C:25:AE:63:F0:1D:84:"
                       "B9:42:6E:D3:08:7A:CF:15:92:E4:3B:60:D8:21:7F:A5");
    target.lastRadioName = QStringLiteral("ANAN-7000DLE");
    return target;
}

// The sample TCI exchange a WSJT-X session starts with.
void appendSampleTciLines(TciLogWindow* log)
{
    const qint64 start = QDateTime(QDate(kTciLogYear, kTciLogMonth, kTciLogDay),
                                   QTime(kTciLogHour, kTciLogMinute, kTciLogSecond))
                             .toMSecsSinceEpoch();
    const QString peer = QStringLiteral("This window: 127.0.0.1:52144");
    const QList<QPair<QString, QString>> lines = {
        {QStringLiteral("out"), QStringLiteral("protocol:ExpertSDR3,1.9")},
        {QStringLiteral("out"), QStringLiteral("device:ANAN-G2")},
        {QStringLiteral("out"), QStringLiteral("receive_only:false")},
        {QStringLiteral("out"), QStringLiteral("trx_count:2")},
        {QStringLiteral("out"), QStringLiteral("vfo_limits:10000,61440000")},
        {QStringLiteral("out"), QStringLiteral("modulations_list:AM,SAM,LSB,USB,CW,NFM,DIGL,DIGU")},
        {QStringLiteral("out"), QStringLiteral("ready")},
        {QStringLiteral("in"), QStringLiteral("vfo:0,0")},
        {QStringLiteral("out"), QStringLiteral("vfo:0,0,14074000")},
        {QStringLiteral("in"), QStringLiteral("modulation:0")},
        {QStringLiteral("out"), QStringLiteral("modulation:0,DIGU")},
        {QStringLiteral("in"), QStringLiteral("audio_start:0")},
    };
    qint64 at = start;
    for (const auto& line : lines) {
        log->appendEntry(line.first, peer, line.second, at);
        at += kTciLogLineGapMs;
    }
}

// The setup tree poses the setup page scenes use: the page's item
// centred, no hover row from the pointer.
void poseSetupTree(SetupDialog* dialog, const QString& label)
{
    auto* tree = dialog->findChild<QTreeWidget*>();
    if (tree == nullptr) {
        return;
    }
    pump(kFinalSettleMs);
    if (tree->currentItem() != nullptr && tree->currentItem()->text(0) == label) {
        tree->scrollToItem(tree->currentItem(), QAbstractItemView::PositionAtCenter);
    }
    tree->viewport()->setAttribute(Qt::WA_Hover, false);
    QEvent leave(QEvent::Leave);
    QCoreApplication::sendEvent(tree->viewport(), &leave);
}

// A remote window built as tests/tst_unbuilt_features.cpp:147 builds it:
// replace(selection, false) makes the window without starting its
// connection (tst_preview_dialog_seams proves no dial and no socket).
MainWindow* openNeverDialledRemoteWindow(SceneContext& ctx, GuiSessionCoordinator& sessions)
{
    const StationStartupSelection core{{QStringLiteral("ws://127.0.0.1:4433"), {}, {}, true},
                                       QStringLiteral("core")};
    if (!sessions.replace(core, false)) {
        ctx.failure = QStringLiteral("the remote window was not built");
        return nullptr;
    }
    MainWindow* remote = sessions.window();
    if (remote == nullptr) {
        ctx.failure = QStringLiteral("the remote window was not built");
        return nullptr;
    }
    remote->setAttribute(Qt::WA_DontShowOnScreen, true);
    remote->setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
    remote->resize(ctx.size);
    remote->show();
    pump(kFinalSettleMs);
    ctx.notes.append(QStringLiteral(
        "remote window: GuiSessionCoordinator::replace(ws://127.0.0.1:4433, startConnection false); "
        "never dialled"));
    return remote;
}

// The remote set: opens one surface on a never-dialled remote window.
void runRemoteDialogScene(SceneContext& ctx, const QString& name,
                          const std::function<QWidget*(SceneContext&, MainWindow*)>& open)
{
    prepareForDialog(ctx);
    RadioDiscovery holdOff;
    holdOff.holdOffScans(std::chrono::minutes{5});
    GuiSessionCoordinator sessions;
    MainWindow* remote = openNeverDialledRemoteWindow(ctx, sessions);
    if (remote != nullptr) {
        QWidget* dialog = open(ctx, remote);
        if (ctx.failure.isEmpty()) {
            captureDialog(ctx, dialog, name);
        }
        for (StationClient* client : remote->findChildren<StationClient*>()) {
            if (client->isConnectionActive() || client->isHandshakeComplete()) {
                ctx.failure = QStringLiteral("%1: the remote window's connection started").arg(name);
            }
        }
    }
    sessions.shutdown();
    RadioDiscovery::clearHoldOffForTest();
}

// One standard dialog, parented to the main window and then to a themed
// dialog (Support, which sets its own stylesheet at SupportDialog.cpp:324).
void runStandardDialogScene(SceneContext& ctx, const QString& name, const QString& source,
                            const std::function<std::unique_ptr<QDialog>(QWidget*)>& make)
{
    prepareForDialog(ctx);
    ctx.notes.append(QStringLiteral("built in the scene from the app's own words (%1), "
                                    "the Qt widget dialog, never a native one").arg(source));
    {
        std::unique_ptr<QDialog> dialog = make(ctx.window);
        dialog->show();
        captureDialog(ctx, dialog.get(), name + QStringLiteral("--main-window"));
        dialog->close();
    }
    if (!ctx.failure.isEmpty()) {
        return;
    }
    auto* themed = openThroughSlot<SupportDialog>(ctx, ctx.window, "showSupportDialog");
    if (themed == nullptr) {
        ctx.failure = QStringLiteral("%1: the Support dialog did not open").arg(name);
        return;
    }
    std::unique_ptr<QDialog> dialog = make(themed);
    dialog->show();
    captureDialog(ctx, dialog.get(), name + QStringLiteral("--themed-dialog"));
    dialog->close();
    ctx.notes.append(QStringLiteral("--themed-dialog is parented to the Support dialog"));
}

std::unique_ptr<QDialog> makeMessageBox(QWidget* parent, QMessageBox::Icon icon, const QString& title,
                                        const QString& text, QMessageBox::StandardButtons buttons,
                                        QMessageBox::StandardButton defaultButton)
{
    auto box = std::make_unique<QMessageBox>(icon, title, text, buttons, parent);
    box->setOption(QMessageBox::Option::DontUseNativeDialog, true);
    if (defaultButton != QMessageBox::NoButton) {
        box->setDefaultButton(defaultButton);
    }
    return box;
}

void appendInventoryDialogScenes(QVector<Scene>& scenes)
{
    // D22: Tools > Network Diagnostics on the local window.
    scenes.append({QStringLiteral("dialog-network-diagnostics"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        auto* dialog = openThroughSlot<NetworkDiagnosticsDialog>(ctx, ctx.window, "openNetworkDiagnostics");
        if (dialog == nullptr) {
            ctx.failure = QStringLiteral("Network Diagnostics did not open");
            return;
        }
        cutTimersInto(ctx, dialog, kMemberTimerCutWaitMs);
        if (!pinValueBeside(dialog, QStringLiteral("Uptime"), QString::fromLatin1(kFrozenNetworkUptime))) {
            ctx.failure = QStringLiteral("no Uptime value in Network Diagnostics");
            return;
        }
        ctx.notes.append(QStringLiteral("uptime held at %1; the 250 ms refresh timer is cut")
                             .arg(QString::fromLatin1(kFrozenNetworkUptime)));
        captureDialog(ctx, dialog, QStringLiteral("dialog-network-diagnostics"));
    }});

    // D18: Spot Hub through MainWindow::openSpotHub, one image per tab.
    scenes.append({QStringLiteral("dialog-spot-hub"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        auto* hub = openThroughSlot<SpotHubDialog>(ctx, ctx.window, "openSpotHub");
        auto* tabs = hub != nullptr ? hub->findChild<QTabWidget*>() : nullptr;
        if (tabs == nullptr) {
            ctx.failure = QStringLiteral("Spot Hub did not open with its tabs");
            return;
        }
        for (int i = 0; i < tabs->count() && ctx.failure.isEmpty(); ++i) {
            tabs->setCurrentIndex(i);
            pump(kTabSettleMs);
            captureDialog(ctx, hub, QStringLiteral("dialog-spot-hub--") + slug(tabs->tabText(i)));
        }
        ctx.notes.append(QStringLiteral("%1 tabs; no spot client is started (auto-start settings are "
                                        "absent in the isolated profile)").arg(tabs->count()));
    }});

    // D19: FreeDV Reporter with no client.
    scenes.append({QStringLiteral("dialog-freedv-reporter"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        FreeDVReporterDialog dialog(ctx.model->freeDvStationModel(), nullptr, ctx.window);
        dialog.show();
        captureDialog(ctx, &dialog, QStringLiteral("dialog-freedv-reporter"));
        dialog.close();
    }});

    // D24: Help > Support.
    scenes.append({QStringLiteral("dialog-support"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        captureDialog(ctx, openThroughSlot<SupportDialog>(ctx, ctx.window, "showSupportDialog"),
                      QStringLiteral("dialog-support"));
        noteNotShown(ctx, QStringLiteral("dialog-feature-request (D41)"),
                     QStringLiteral("opening it copies its prompt to the clipboard "
                                    "(MainWindow.cpp:18384), and the preview runs on the cocoa "
                                    "platform, so it would overwrite the operator's clipboard"));
    }});

    // D25: Help > About NereusSDR (MainWindow.cpp:11719, exec'd there).
    scenes.append({QStringLiteral("dialog-about"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        AboutDialog dialog(ctx.window);
        dialog.show();
        captureDialog(ctx, &dialog, QStringLiteral("dialog-about"));
        dialog.close();
    }});

    // Lit states posed after show: each host is shown, the live pass runs,
    // and only then does the state change, through the applet's setter or
    // a status line injected into a never-connected TgxlConnection. No
    // toggle signal is let through, nothing connects or starts discovery.
    const auto litHost = [](SceneContext& ctx, const QString& title) {
        prepareForDialog(ctx);
        auto host = std::make_unique<QWidget>(ctx.window, Qt::Tool);
        host->setObjectName(QStringLiteral("LitStatesHost"));
        host->setWindowTitle(title);
        auto* row = new QHBoxLayout(host.get());
        row->setContentsMargins(8, 8, 8, 8);
        row->setSpacing(8);
        return host;
    };
    const auto showThenPose = [](QWidget* host, const std::function<void()>& pose) {
        host->adjustSize();
        host->show();
        pump(kDialogSettleMs);
        pose();
        QCoreApplication::processEvents();
    };
    scenes.append({QStringLiteral("dialog-lit-operate"), QStringLiteral("2v"), true,
                   [litHost, showThenPose](SceneContext& ctx) {
        // The models outlive the host, which owns the applets.
        TgxlConnection tgxl;
        TunerModel tuner;
        tuner.bindConnection(&tgxl);
        auto host = litHost(ctx, QStringLiteral("OPERATE on"));
        auto* tunerApplet = new TunerApplet(nullptr, &tuner, host.get());
        auto* amp = new AmpApplet(nullptr, host.get());
        auto* rf2ks = new Rf2ksApplet(nullptr, host.get());
        for (QWidget* w : {static_cast<QWidget*>(tunerApplet), static_cast<QWidget*>(amp),
                           static_cast<QWidget*>(rf2ks)}) {
            w->setMinimumWidth(260);
            host->layout()->addWidget(w);
        }
        showThenPose(host.get(), [&] {
            tgxl.injectLineForTesting(QStringLiteral("V1.2.17"));
            tgxl.injectLineForTesting(QStringLiteral("S0|state state=1 bypass=0"));
            amp->setState(QStringLiteral("OPERATE"));
            rf2ks->setOperateMode(QStringLiteral("OPERATE"));
        });
        ctx.notes.append(QStringLiteral("TgxlConnection status state=1 bypass=0, AmpApplet::setState(OPERATE), "
                                        "Rf2ksApplet::setOperateMode(OPERATE), after show"));
        captureDialog(ctx, host.get(), QStringLiteral("dialog-lit-operate"));
        host->close();
    }});

    // The tuner and amplifier panels' readouts: forward power as one large
    // bar, reflected power, SWR and temperature as slim rows. The three
    // real panels side by side at 260 px, fed through their public setters
    // and a never-connected TgxlConnection, in four states. Nothing
    // connects and nothing keys. accessory-readouts wears the workspace
    // look; accessory-readouts-classic is the same with the look off, which
    // is what the everyday launcher shows.
    const auto accessoryReadouts = [litHost, showThenPose](SceneContext& ctx, const QString& scene) {
        struct State {
            const char* slug;
            const char* title;
            bool keyed;
            double fwdPeak;   // shown first, so the large bar holds it as its peak
            double fwd;
            int rfKitReflected;   // the RF2K-S reports it; the other two work it out
            double swr;
            double temp;
        };
        static const State kStates[] = {
            {"keyed", "Keyed", true, 1310.0, 1236.0, 4, 1.12, 41.0},
            {"high-swr", "High SWR", true, 800.0, 800.0, 159, 2.61, 44.0},
            {"not-keyed", "Not keyed", false, 0.0, 0.0, 0, 1.0, 38.0},
            {"largest", "Largest values", true, 2000.0, 2000.0, 1447, 12.4, 100.0},
        };
        for (const State& st : kStates) {
            // The models outlive the host, which owns the applets.
            TgxlConnection tgxl;
            TunerModel tuner;
            tuner.bindConnection(&tgxl);
            auto host = litHost(ctx, QString::fromLatin1(st.title));
            auto* tunerApplet = new TunerApplet(nullptr, &tuner, host.get());
            auto* amp = new AmpApplet(nullptr, host.get());
            auto* rf2ks = new Rf2ksApplet(nullptr, host.get());
            for (QWidget* w : {static_cast<QWidget*>(tunerApplet), static_cast<QWidget*>(amp),
                               static_cast<QWidget*>(rf2ks)}) {
                w->setFixedWidth(260);
                host->layout()->addWidget(w);
                host->layout()->setAlignment(w, Qt::AlignTop);
            }
            showThenPose(host.get(), [&] {
                tgxl.injectLineForTesting(QStringLiteral("V1.2.17"));
                tgxl.injectLineForTesting(QStringLiteral("S0|state state=1 bypass=0"));
                tunerApplet->setPowerScale(/*maxWatts=*/0, /*hasAmplifier=*/true);
                amp->setMainsVoltage(240);
                amp->setDrainCurrent(st.keyed ? 38.0f : 0.0f);
                amp->setMeff(QStringLiteral("ACTIVE"));
                amp->setState(st.keyed ? QStringLiteral("TRANSMIT_A") : QStringLiteral("OPERATE"));
                rf2ks->setConnectedState(true);
                rf2ks->setNicknameAndVersion(QStringLiteral("RF2K-S"), QStringLiteral("G190C217"));
                rf2ks->setOperateMode(QStringLiteral("OPERATE"));
                RfKitPowerSnapshot snap;
                snap.swr = static_cast<float>(st.swr);
                snap.temperatureC = static_cast<float>(st.temp);
                snap.voltageV = 53.0f;
                snap.currentA = st.keyed ? 31.5f : 0.0f;
                for (const double watts : {st.fwdPeak, st.fwd}) {
                    tunerApplet->updateMeters(static_cast<float>(watts), static_cast<float>(st.swr));
                    amp->setFwdPower(static_cast<float>(watts));
                    amp->setSwr(static_cast<float>(st.swr));
                    snap.forwardW = static_cast<int>(watts);
                    snap.reflectedW = st.rfKitReflected;
                    rf2ks->setPower(snap);
                }
                amp->setTemp(static_cast<float>(st.temp));
            });
            ctx.notes.append(QStringLiteral("%1: forward %2 W (after %3 W), SWR %4, %5 C, %6; through "
                                            "TunerApplet::updateMeters, AmpApplet::setState / setFwdPower / "
                                            "setSwr / setTemp and Rf2ksApplet::setPower, after show")
                                 .arg(QString::fromLatin1(st.slug))
                                 .arg(st.fwd).arg(st.fwdPeak).arg(st.swr).arg(st.temp)
                                 .arg(st.keyed ? QStringLiteral("keyed") : QStringLiteral("not keyed")));
            captureDialog(ctx, host.get(), QStringLiteral("%1--%2").arg(scene, QString::fromLatin1(st.slug)));
            host->close();
            if (!ctx.failure.isEmpty()) {
                return;
            }
        }
    };
    scenes.append({QStringLiteral("accessory-readouts"), QStringLiteral("2v"), true,
                   [accessoryReadouts](SceneContext& ctx) {
        accessoryReadouts(ctx, QStringLiteral("accessory-readouts"));
    }, true});
    scenes.append({QStringLiteral("accessory-readouts-classic"), QStringLiteral("2v"), true,
                   [accessoryReadouts](SceneContext& ctx) {
        accessoryReadouts(ctx, QStringLiteral("accessory-readouts-classic"));
    }});
    // Real main-meter consumers on a Remote-shaped model; no connection or
    // key command. Extra overrange pose proves legal-marker/top independence.
    const auto mainPowerScene = [litHost, showThenPose](SceneContext& ctx, const QString& scene, bool containers) {
        struct PowerState { const char* name; bool operate; bool keyed; double forward; double drive; };
        const PowerState states[] = {{"operate-keyed",true,true,1236,42},
            {"standby-keyed",false,true,98,98},{"operate-idle",true,false,0,0},
            {"overrange",true,true,2300,42}};
        for (const PowerState& st : states) {
            RadioModel radio(RadioModel::Role::Remote);
            radio.setBoardForTest(HPSDRHW::Saturn);
            radio.currentRadioChanged(RadioInfo{});
            MeterPoller poller;
            poller.setRadioStatus(&radio.radioStatus());
            poller.rescalePowerMeters(100);
            poller.setMainPowerMeter(radio.mainPowerMeter());
            auto host = litHost(ctx, QString::fromLatin1(st.name));
            QVector<MeterWidget*> meterWidgets;
            auto* dial = new SMeterWidget(host.get());
            dial->setFixedSize(260,130);
            MainPowerMeter* meter = radio.mainPowerMeter();
            const auto scale = [dial,meter] { dial->setPowerDisplayScale(meter->visualLimitW(),meter->scaleTopW()); };
            const auto reading = [dial,meter] {
                dial->setTxMeters(static_cast<float>(meter->forwardW()),static_cast<float>(meter->swr()));
            };
            QObject::connect(meter,&MainPowerMeter::scaleChanged,dial,scale);
            QObject::connect(meter,&MainPowerMeter::readingChanged,dial,reading);
            QObject::connect(&radio,&RadioModel::transmittingChanged,dial,&SMeterWidget::setTransmitting);
            scale(); reading();
            QLayout* dialLayout = host->layout();
            if (!containers) {
                auto* tx = new TxApplet(&radio,host.get());
                tx->setFixedWidth(260);
                host->layout()->addWidget(tx);
                host->layout()->setAlignment(tx,Qt::AlignTop);
            } else {
                auto* content = new QWidget(host.get());
                auto* contentLayout = new QVBoxLayout(content);
                contentLayout->setContentsMargins(0,0,0,0);
                auto* topRow = new QWidget(content);
                auto* topLayout = new QHBoxLayout(topRow);
                auto* bottomRow = new QWidget(content);
                auto* bottomLayout = new QHBoxLayout(bottomRow);
                contentLayout->addWidget(topRow);
                contentLayout->addWidget(bottomRow);
                host->layout()->addWidget(content);
                dialLayout = topLayout;
                auto* column = new QWidget(topRow);
                auto* layout = new QVBoxLayout(column);
                layout->setContentsMargins(0,0,0,0);
                auto* legacy = new MeterWidget(column);
                legacy->setFixedSize(260,220);
                ItemGroup* group = ItemGroup::createPowerSwrPreset(QStringLiteral("Power"),legacy);
                for (MeterItem* item : group->items()) { legacy->addItem(item); }
                poller.addTarget(legacy);
                meterWidgets.append(legacy);
                layout->addWidget(legacy);
                auto* bar = new BarInstrument(column);
                bar->setFixedSize(260,100);
                bar->setPrimary(MeterBinding::TxPower);
                const auto barScale = [bar,meter] { bar->setPrimaryScale(powerReadingDescriptor(meter->visualLimitW(),meter->scaleTopW())); };
                QObject::connect(meter,&MainPowerMeter::scaleChanged,bar,barScale); barScale();
                QObject::connect(meter,&MainPowerMeter::readingChanged,bar,[bar](double power,double) { bar->onReading(MeterBinding::TxPower,power); });
                layout->addWidget(bar);
                topLayout->addWidget(column);
                topLayout->setAlignment(column,Qt::AlignTop);
                for (const CompositePresetItem::Face face : {CompositePresetItem::Face::PowerSwr,CompositePresetItem::Face::Cross,CompositePresetItem::Face::Anan}) {
                    auto* widget = new MeterWidget(bottomRow);
                    widget->setFixedSize(face==CompositePresetItem::Face::PowerSwr?260:360,300);
                    auto* item = new CompositePresetItem(face,widget);
                    item->setRect(0,0,1,1);
                    widget->addItem(item); poller.addTarget(widget);
                    meterWidgets.append(widget);
                    bottomLayout->addWidget(widget);
                    bottomLayout->setAlignment(widget,Qt::AlignTop);
                }
            }
            dialLayout->addWidget(dial);
            dialLayout->setAlignment(dial,Qt::AlignTop);
            showThenPose(host.get(),[&] {
                poller.setInTx(st.keyed);
                radio.applyMirroredValue("transmitting",st.keyed);
                radio.radioStatus().setPowerReadings(st.drive,0,1.12);
                radio.amplifierModel()->applyStationValue("forwardPowerW",st.forward);
                radio.amplifierModel()->applyStationValue("swr",1.12);
                radio.amplifierModel()->applyStationValue("present",true);
                radio.amplifierModel()->applyStationValue("operate",st.operate);
                // Real radio samples keep its existing EMA moving; vary by
                // a hundredth, below whole-watt display resolution.
                if (!st.operate && st.keyed) {
                    for (int i=0;i<40;++i) { radio.radioStatus().setPowerReadings(st.drive+(i%2)*.01,0,1.12); }
                }
                // Advance the real display dynamics deterministically. This
                // preview has no live poller or radio transport.
                for (MeterWidget* widget : meterWidgets) {
                    for (int step = 0; step <= 120; ++step) {
                        widget->advanceMeters(step * 50);
                    }
                }
            });
            ctx.notes.append(QStringLiteral("1500 W is a legal display marker; %1 W is retained unclamped.").arg(st.forward));
            captureDialog(ctx,host.get(),QStringLiteral("%1--%2").arg(scene,QString::fromLatin1(st.name)));
            host->close(); if (!ctx.failure.isEmpty()) { return; }
        }
    };
    for (const QString& name : {QStringLiteral("main-power-row"),QStringLiteral("main-power-row-classic"),
                               QStringLiteral("main-meter-amplifier"),QStringLiteral("main-meter-amplifier-classic")}) {
        scenes.append({name,QStringLiteral("2v"),true,[mainPowerScene,name](SceneContext& ctx) {
            mainPowerScene(ctx,name,name.startsWith(QStringLiteral("main-meter-amplifier")));
        },!name.endsWith(QStringLiteral("-classic"))});
    }
    scenes.append({QStringLiteral("dialog-lit-antenna"), QStringLiteral("2v"), true,
                   [litHost, showThenPose](SceneContext& ctx) {
        TgxlConnection tgxl;
        TunerModel tuner;
        tuner.bindConnection(&tgxl);
        auto host = litHost(ctx, QStringLiteral("Antenna selected"));
        auto* tunerApplet = new TunerApplet(nullptr, &tuner, host.get());
        tunerApplet->setMinimumWidth(260);
        host->layout()->addWidget(tunerApplet);
        showThenPose(host.get(), [&] {
            // The antenna row shows for a TGXL with the switch, reached
            // here as a station state: model fields only, no connection
            // (TunerModel.cpp:39-80).
            TunerModel::StationConnectionState station;
            station.phase = TunerModel::ConnectionPhase::Connected;
            tuner.setStationConnectionState(station);
            tgxl.injectLineForTesting(QStringLiteral("V1.2.17"));
            tgxl.injectLineForTesting(QStringLiteral("S0|state state=1 bypass=0 antA=1 3way=1"));
        });
        ctx.notes.append(QStringLiteral("TgxlConnection status antA=1 (ANT 2), after show"));
        captureDialog(ctx, host.get(), QStringLiteral("dialog-lit-antenna"));
        host->close();
    }});
    scenes.append({QStringLiteral("dialog-lit-psa-on"), QStringLiteral("2v"), true,
                   [litHost, showThenPose](SceneContext& ctx) {
        RadioModel radio;
        auto host = litHost(ctx, QStringLiteral("PS-A on"));
        auto* tx = new TxApplet(&radio, host.get());
        host->layout()->addWidget(tx);
        auto* psa = tx->findChild<QPushButton*>(QStringLiteral("TxAppletPsaBtn"));
        if (psa == nullptr) {
            ctx.failure = QStringLiteral("no TxAppletPsaBtn");
            return;
        }
        psa->setVisible(true);
        showThenPose(host.get(), [psa] {
            const QSignalBlocker block(psa);
            psa->setChecked(true);
        });
        ctx.notes.append(QStringLiteral("TxAppletPsaBtn checked with its signals blocked, after show"));
        captureDialog(ctx, host.get(), QStringLiteral("dialog-lit-psa-on"));
        host->close();
    }});
    scenes.append({QStringLiteral("dialog-lit-ps-badge"), QStringLiteral("2v"), true,
                   [litHost, showThenPose](SceneContext& ctx) {
        auto host = litHost(ctx, QStringLiteral("PureSignal badge"));
        auto* badge = new PsaIndicatorWidget(nullptr, host.get());
        host->layout()->addWidget(badge);
        showThenPose(host.get(), [badge] {
            badge->setPsEnabled(true);
            badge->setMox(false);
        });
        ctx.notes.append(QStringLiteral("PsaIndicatorWidget::setPsEnabled(true), setMox(false), after show"));
        captureDialog(ctx, host.get(), QStringLiteral("dialog-lit-ps-badge"));
        host->close();
    }});

    // D3: Add custom radio (ConnectionPanel.cpp:1185); Probe is never pressed.
    scenes.append({QStringLiteral("dialog-add-custom-radio"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        AddCustomRadioDialog dialog(ctx.window);
        dialog.show();
        captureDialog(ctx, &dialog, QStringLiteral("dialog-add-custom-radio"));
        dialog.close();
    }});

    // D12: NNR models (MainWindow.cpp:5202).
    scenes.append({QStringLiteral("dialog-dsp-assets"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        DspAssetDialog dialog(ctx.model, DspAssetKind::NnrModel, ctx.window);
        dialog.show();
        captureDialog(ctx, &dialog, QStringLiteral("dialog-dsp-assets"));
        dialog.close();
    }});

    // D11: AmpView as the inventory opens it (no model, no PureSignal).
    scenes.append({QStringLiteral("dialog-amp-view"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        AmpViewWindow dialog(nullptr, nullptr, ctx.window);
        dialog.show();
        captureDialog(ctx, &dialog, QStringLiteral("dialog-amp-view"));
        dialog.close();
    }});

    // D31: Edit this preset (MainWindow.cpp:13873) for USB, first preset.
    scenes.append({QStringLiteral("dialog-filter-preset-edit"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        FilterPresetEditDialog dialog(ctx.model->filterPresetStore(), DSPMode::USB, 0, ctx.window);
        dialog.show();
        captureDialog(ctx, &dialog, QStringLiteral("dialog-filter-preset-edit"));
        dialog.close();
    }});

    // D30: Filter policy for chain 0 (MainWindow.cpp:6518, exec'd there).
    scenes.append({QStringLiteral("dialog-filter-policy"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        FilterPolicyDialog dialog(0, ctx.model, ctx.window);
        dialog.show();
        captureDialog(ctx, &dialog, QStringLiteral("dialog-filter-policy"));
        dialog.close();
    }});

    // D28: TX-bound re-route (MainWindow.cpp:8859) with the window's slices.
    scenes.append({QStringLiteral("dialog-tx-bound-confirm"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        TxBoundConfirmDialog dialog(QStringLiteral("ANT2"), QStringLiteral("ANT1"),
                                    ctx.model->slices(), ctx.window);
        dialog.show();
        captureDialog(ctx, &dialog, QStringLiteral("dialog-tx-bound-confirm"));
        dialog.close();
    }});

    // D32 to D35: the multi-device questions with sample values.
    scenes.append({QStringLiteral("dialog-take-transmit"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        TakeTransmitDialog::Holder holder{QStringLiteral("Jo's iPhone"), QStringLiteral("iPhone"),
                                          true, false, 0};
        TakeTransmitDialog dialog(holder, ctx.window);
        dialog.show();
        captureDialog(ctx, &dialog, QStringLiteral("dialog-take-transmit"));
        dialog.close();
    }});
    scenes.append({QStringLiteral("dialog-take-receiver"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        TakeReceiverDialog dialog(sampleTakeReceiverPrompt(), ctx.window);
        dialog.show();
        captureDialog(ctx, &dialog, QStringLiteral("dialog-take-receiver"));
        dialog.close();
    }});
    scenes.append({QStringLiteral("dialog-confirm-change"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        ConfirmChangeDialog dialog(sampleSharedSettingPrompt(), ctx.window);
        dialog.show();
        captureDialog(ctx, &dialog, QStringLiteral("dialog-confirm-change"));
        dialog.close();
    }});
    scenes.append({QStringLiteral("dialog-replace-device"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        ReplaceDeviceDialog dialog(sampleHeldList(), ctx.window);
        dialog.show();
        captureDialog(ctx, &dialog, QStringLiteral("dialog-replace-device"));
        dialog.close();
    }});

    // D26: VAX first run, the macOS screen and the Windows cables screen.
    scenes.append({QStringLiteral("dialog-vax-first-run"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        {
            VaxFirstRunDialog dialog(FirstRunScenario::MacNative, {}, ctx.window);
            dialog.show();
            captureDialog(ctx, &dialog, QStringLiteral("dialog-vax-first-run"));
            dialog.close();
        }
        const QVector<DetectedCable> cables = {
            {VirtualCableProduct::VbCable, QStringLiteral("CABLE Input (VB-Audio Virtual Cable)"), false, 0},
            {VirtualCableProduct::VbCable, QStringLiteral("CABLE Output (VB-Audio Virtual Cable)"), true, 0},
            {VirtualCableProduct::VbCableA, QStringLiteral("CABLE-A Input (VB-Audio Cable A)"), false, 1},
            {VirtualCableProduct::VbCableA, QStringLiteral("CABLE-A Output (VB-Audio Cable A)"), true, 1},
        };
        VaxFirstRunDialog dialog(FirstRunScenario::WindowsCablesFound, cables, ctx.window);
        dialog.show();
        captureDialog(ctx, &dialog, QStringLiteral("dialog-vax-first-run--cables-found"));
        dialog.close();
    }});

    // D37: PGXL save and reboot (PgxlAdvancedPage.cpp:997).
    scenes.append({QStringLiteral("dialog-pgxl-save-reboot"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        PgxlSaveRebootDialog dialog(ctx.window);
        dialog.show();
        captureDialog(ctx, &dialog, QStringLiteral("dialog-pgxl-save-reboot"));
        dialog.close();
    }});

    // D15, D16: MMIO endpoints and the variable picker; both list saved
    // endpoints only.
    scenes.append({QStringLiteral("dialog-mmio-endpoints"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        MmioEndpointsDialog dialog(ctx.window);
        dialog.show();
        captureDialog(ctx, &dialog, QStringLiteral("dialog-mmio-endpoints"));
        dialog.close();
    }});
    scenes.append({QStringLiteral("dialog-mmio-variable-picker"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        MmioVariablePickerPopup dialog(QUuid(), QString(), ctx.window);
        dialog.show();
        captureDialog(ctx, &dialog, QStringLiteral("dialog-mmio-variable-picker"));
        dialog.close();
    }});

    // D36: the TCI log with a sample WSJT-X exchange at a fixed time.
    scenes.append({QStringLiteral("dialog-tci-log"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        TciLogWindow dialog(ctx.window);
        appendSampleTciLines(&dialog);
        dialog.show();
        captureDialog(ctx, &dialog, QStringLiteral("dialog-tci-log"));
        dialog.close();
    }});

    // D5 to D7: the connection selector with sample rows, the Core editor
    // and Add by code, parented to the selector as GuiConnectionController
    // parents them (GuiConnectionController.cpp:896, :1136).
    scenes.append({QStringLiteral("dialog-connection-selector"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        ConnectionSelector selector;
        selector.setTargets(sampleConnectionRows());
        selector.setCurrentConnection(QStringLiteral("This computer's Core, with no radio connected"),
                                      QString(), false, false);
        selector.setDiscoveryStatus(QStringLiteral("LAN discovery is active."));
        selector.setSelectedKey(QStringLiteral("radio:00:1C:C0:4E:21:0A"));
        selector.show();
        captureDialog(ctx, &selector, QStringLiteral("dialog-connection-selector"));
        selector.close();
    }});
    scenes.append({QStringLiteral("dialog-core-target-editor"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        ConnectionSelector selector;
        CoreTargetEditor dialog(sampleSavedCore(), &selector);
        dialog.show();
        captureDialog(ctx, &dialog, QStringLiteral("dialog-core-target-editor"));
        dialog.close();
    }});
    scenes.append({QStringLiteral("dialog-add-core-by-code"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        ConnectionSelector selector;
        AddCoreByCodeDialog dialog(QString(), &selector);
        dialog.show();
        captureDialog(ctx, &dialog, QStringLiteral("dialog-add-core-by-code"));
        dialog.close();
    }});

    // D29: the FFTW wisdom dialog from the builder the wisdomProgress
    // handler uses, at a sample step.
    scenes.append({QStringLiteral("dialog-wisdom-progress"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        std::unique_ptr<QProgressDialog> dialog(MainWindow::buildWisdomProgressDialog(ctx.window));
        dialog->show();
        dialog->setValue(kWisdomSamplePercent);
        dialog->setLabelText(QStringLiteral("Optimizing FFT plans for DSP engine...\n\n%1")
                                 .arg(QStringLiteral("Planning COMPLEX FORWARD  FFT size 4096")));
        captureDialog(ctx, dialog.get(), QStringLiteral("dialog-wisdom-progress"));
        dialog->close();
    }});

    // D2: the connection panel, discovery skipped by its seam, with two
    // sample radios fed to its discovery slot.
    scenes.append({QStringLiteral("dialog-connection-panel"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        ConnectionPanel::setDiscoveryStartSkippedForTest(true);
        auto* panel = openThroughSlot<ConnectionPanel>(ctx, ctx.window, "showConnectionPanel");
        if (panel != nullptr) {
            panel->onRadioDiscovered(sampleRadio(QStringLiteral("ANAN-G2"), QStringLiteral("00:1C:C0:4E:21:0A"),
                                                 QStringLiteral("192.168.1.20"), HPSDRHW::Saturn,
                                                 ProtocolVersion::Protocol2, 27));
            panel->onRadioDiscovered(sampleRadio(QStringLiteral("Hermes Lite 2"),
                                                 QStringLiteral("00:1C:C0:7B:90:3C"),
                                                 QStringLiteral("192.168.1.31"), HPSDRHW::HermesLite,
                                                 ProtocolVersion::Protocol1, 73));
            ctx.notes.append(QStringLiteral("discovery skipped (ConnectionPanel::"
                                            "setDiscoveryStartSkippedForTest); two sample radios; the "
                                            "15 s last-seen refresh does not fire before the grab"));
        }
        captureDialog(ctx, panel, QStringLiteral("dialog-connection-panel"));
        ConnectionPanel::setDiscoveryStartSkippedForTest(false);
    }});

    // D4: the 4O3A LAN scan, no scan by its seam, two sample devices, then
    // the scan's own finish.
    scenes.append({QStringLiteral("dialog-lan-scan"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        LanScanDialog::setDiscoveryStartSkippedForTest(true);
        {
            LanScanDialog dialog(ctx.window);
            const auto add = [&dialog](const QString& model, const QString& ip, quint16 port,
                                       const QString& version, const QString& serial,
                                       const QString& nickname) {
                QMetaObject::invokeMethod(&dialog, "onDeviceDiscovered", Qt::DirectConnection,
                                          Q_ARG(QString, model), Q_ARG(QString, ip), Q_ARG(quint16, port),
                                          Q_ARG(QString, version), Q_ARG(QString, serial),
                                          Q_ARG(QString, nickname));
            };
            add(QStringLiteral("TunerGeniusXL"), QStringLiteral("192.168.1.60"), kTgxlPort,
                QStringLiteral("1.2.11"), QStringLiteral("1520-1104-8823"), QStringLiteral("Shack TGXL"));
            add(QStringLiteral("PowerGeniusXL"), QStringLiteral("192.168.1.61"), kPgxlPort,
                QStringLiteral("3.8.6"), QStringLiteral("4220-0317-1192"), QStringLiteral("Shack PGXL"));
            QMetaObject::invokeMethod(&dialog, "onScanFinished", Qt::DirectConnection);
            dialog.show();
            ctx.notes.append(QStringLiteral("scan skipped (LanScanDialog::setDiscoveryStartSkippedForTest); "
                                            "two sample devices, then the scan's finish"));
            captureDialog(ctx, &dialog, QStringLiteral("dialog-lan-scan"));
            dialog.close();
        }
        LanScanDialog::setDiscoveryStartSkippedForTest(false);
    }});

    // The one non-native colour dialog: Display > Spectrum's Fill colour.
    scenes.append({QStringLiteral("dialog-overlay-colour"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        SetupDialog* setup = openSetupDialog(ctx.window);
        if (setup == nullptr) {
            ctx.failure = QStringLiteral("Setup did not open");
            return;
        }
        setup->selectPage(QStringLiteral("Spectrum"));
        QCoreApplication::processEvents();
        QPushButton* fill = nullptr;
        for (QPushButton* b : setup->findChildren<QPushButton*>()) {
            if (b->property("nereusSetupId").toString()
                == QLatin1String("display.spectrum.fillColor")) {
                fill = b;
                break;
            }
        }
        if (fill == nullptr) {
            ctx.failure = QStringLiteral("no fill colour button");
            setup->close();
            return;
        }
        captureModal<QColorDialog>(ctx, QStringLiteral("dialog-overlay-colour"), [fill] { fill->click(); });
        setup->close();
        ctx.notes.append(QStringLiteral("the file dialogs and the other colour dialogs are native on "
                                        "macOS and are not captured"));
    }});

    // ---- standard dialogs ----
    scenes.append({QStringLiteral("dialog-msgbox-information"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        runStandardDialogScene(ctx, QStringLiteral("dialog-msgbox-information"),
                               QStringLiteral("TxProfileSetupPage.cpp:484-489"), [](QWidget* parent) {
            return makeMessageBox(parent, QMessageBox::Information, QStringLiteral("Cannot Delete Profile"),
                                  QStringLiteral("It is not possible to delete the last remaining TX profile"),
                                  QMessageBox::Ok, QMessageBox::NoButton);
        });
    }});
    scenes.append({QStringLiteral("dialog-msgbox-warning"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        runStandardDialogScene(ctx, QStringLiteral("dialog-msgbox-warning"),
                               QStringLiteral("GeneralOptionsPage.cpp:551-555"), [](QWidget* parent) {
            return makeMessageBox(parent, QMessageBox::Warning, QStringLiteral("Warning: Enable Transmit?"),
                                  QStringLiteral("Unchecking Receive Only may cause damage to your hardware. "
                                                 "Are you sure you want to enable transmit?"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        });
    }});
    scenes.append({QStringLiteral("dialog-msgbox-question"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        runStandardDialogScene(ctx, QStringLiteral("dialog-msgbox-question"),
                               QStringLiteral("TxProfileSetupPage.cpp:441-445"), [](QWidget* parent) {
            return makeMessageBox(parent, QMessageBox::Question, QStringLiteral("Overwrite TX Profile"),
                                  QStringLiteral("A profile named \"ESSB 2.8k\" already exists.  Overwrite?"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::NoButton);
        });
    }});
    scenes.append({QStringLiteral("dialog-inputdialog-text"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        runStandardDialogScene(ctx, QStringLiteral("dialog-inputdialog-text"),
                               QStringLiteral("TxProfileSetupPage.cpp:416-422"), [](QWidget* parent) {
            auto input = std::make_unique<QInputDialog>(parent);
            input->setInputMode(QInputDialog::TextInput);
            input->setWindowTitle(QStringLiteral("Save TX Profile"));
            input->setLabelText(QStringLiteral("New profile name:"));
            input->setTextEchoMode(QLineEdit::Normal);
            input->setTextValue(QStringLiteral("ESSB 2.8k"));
            return std::unique_ptr<QDialog>(std::move(input));
        });
    }});

    // ---- the remote set, on a never-dialled remote window ----
    scenes.append({QStringLiteral("dialog-remote-diagnostics"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        runRemoteDialogScene(ctx, QStringLiteral("dialog-remote-diagnostics"),
                             [](SceneContext& c, MainWindow* remote) -> QWidget* {
            auto* dialog = openThroughSlot<RemoteDiagnosticsDialog>(c, remote, "openNetworkDiagnostics");
            if (dialog != nullptr) {
                cutTimersInto(c, dialog, kMemberTimerCutWaitMs);
            }
            return dialog;
        });
    }});
    scenes.append({QStringLiteral("dialog-remote-connection"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        runRemoteDialogScene(ctx, QStringLiteral("dialog-remote-connection"),
                             [](SceneContext& c, MainWindow* remote) -> QWidget* {
            return openThroughSlot<RemoteConnectionPanel>(c, remote, "showRemoteConnectionPanel");
        });
        noteNotShown(ctx, QStringLiteral("popup-connection-route (C1)"),
                     QStringLiteral("a popup; the menus and popups task captures it"));
    }});
    // Setup's This Core page; not named dialog-setup-* (those names are the
    // local window's setup page scenes).
    scenes.append({QStringLiteral("dialog-remote-setup-this-core"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        runRemoteDialogScene(ctx, QStringLiteral("dialog-remote-setup-this-core"),
                             [](SceneContext& c, MainWindow* remote) -> QWidget* {
            const QString page = QStringLiteral("This Core");
            if (!QMetaObject::invokeMethod(remote, "openSetupAtPage", Qt::DirectConnection,
                                           Q_ARG(QString, page))) {
                c.failure = QStringLiteral("MainWindow has no slot openSetupAtPage");
                return nullptr;
            }
            auto* setup = visibleTopLevel<SetupDialog>();
            if (setup != nullptr) {
                poseSetupTree(setup, page);
            }
            return setup;
        });
    }});
    // The manual's TCI Server figure: this window's server and the Core's
    // TCI server, which the page shows only in a remote window
    // (CatTciServerPage::refreshCoreGroup), in one image: the dialog grown
    // until the page needs no scrolling. The window is never dialed; the
    // Core's station link is posed instead (Test::FakeStationLink, ready,
    // with stationTciVersion 2 and stationTciSettingsVersion 1), with the
    // Core's server on, listening on the Core's default port and every
    // other value at the readers' defaults (StationTciModel::State). The
    // window's own StationClient is attached again afterwards.
    scenes.append({QStringLiteral("dialog-remote-setup-tci-server"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        Test::FakeStationLink coreLink;
        QPointer<RadioModel> posedModel;
        QPointer<StationClient> ownLink;
        runRemoteDialogScene(ctx, QStringLiteral("dialog-remote-setup-tci-server"),
                             [&coreLink, &posedModel, &ownLink](SceneContext& c,
                                                                MainWindow* remote) -> QWidget* {
            RadioModel* model = remote->radioModel();
            if (model == nullptr || model->stationTciModel() == nullptr) {
                c.failure = QStringLiteral("the remote window has no RadioModel station TCI model");
                return nullptr;
            }
            posedModel = model;
            ownLink = dynamic_cast<StationClient*>(model->stationLink());
            model->attachStation(&coreLink);
            StationTciModel::State core;
            core.enabled = true;
            core.port = StationTciController::kDefaultPort;
            core.listening = true;
            model->stationTciModel()->setState(core);
            c.notes.append(QStringLiteral("Core link posed: Test::FakeStationLink (ready, station TCI "
                                          "server and settings available), StationTciModel on, "
                                          "port %1, listening, other values at their defaults")
                               .arg(core.port));
            const QString page = QStringLiteral("TCI Server");
            if (!QMetaObject::invokeMethod(remote, "openSetupAtPage", Qt::DirectConnection,
                                           Q_ARG(QString, page))) {
                c.failure = QStringLiteral("MainWindow has no slot openSetupAtPage");
                return nullptr;
            }
            auto* setup = visibleTopLevel<SetupDialog>();
            if (setup == nullptr) {
                return nullptr;
            }
            poseSetupTree(setup, page);
            pump(kDialogSettleMs);
            growSetupToFit(c, setup, QStringLiteral("Setup > CAT & Network > TCI Server (remote window)"));
            return c.failure.isEmpty() ? setup : nullptr;
        });
        if (posedModel != nullptr && posedModel->stationLink() == &coreLink) {
            if (ownLink != nullptr) {
                posedModel->attachStation(ownLink.data());
            } else {
                posedModel->detachStation();
            }
        }
    }});
}


// ---- menus, popups, toasts and tooltips (workspace visual review Task 24) ----
//
// Every surface opens through the product's own path on the two-band
// window: a context-menu or help event, a click, a mouse event, or the
// signal the product connects to the menu. A deferred timer grabs it while
// it is open (QMenu::exec blocks in its own loop) and then closes it, so no
// action is ever triggered and nothing changes. Images:
//   <name>              the base (window or dialog) with the popup painted in
//   <name>--popup       the popup alone
//   <name>--sub-<item>  the same with that submenu open (one level deep)

constexpr int kPopupWaitMs = 5000;
// How often shootOpenPopup places a popup again after its size changed.
constexpr int kPopupPlacePasses = 4;
constexpr int kPopupGraceMs = 5000;
// How long profile-rail-open keeps its tooltip up (QTipLabel's own timer
// is about 10 s).
constexpr int kRailTipHoldMs = 600000;

struct PopupShot {
    // The popup, when it is not QApplication::activePopupWidget().
    std::function<QWidget*()> find;
    // Where the product asks for the popup (global): the click point, the
    // trigger's corner, or QCursor::pos() for which the gallery gives the
    // trigger. Unset: where Qt put it.
    std::optional<QPoint> anchor;
    // The base image: the window unless set (a dialog).
    QWidget* base{nullptr};
    // Painted under the popup (a dialog over the window).
    QList<QWidget*> under;
    bool submenus{true};
    // Not opening is recorded as not_shown with this reason instead of
    // failing the scene.
    QString optionalReason;
    QString provenance;
};

QJsonArray menuItemsJson(QMenu* menu)
{
    QJsonArray items;
    for (QAction* a : menu->actions()) {
        if (!a->isVisible()) {
            continue;
        }
        if (a->isSeparator()) {
            items.append(a->text().isEmpty() ? QStringLiteral("-") : QStringLiteral("-- %1 --").arg(a->text()));
            continue;
        }
        QString t = a->text();
        if (a->menu() != nullptr) {
            t += QStringLiteral(" >");
        }
        if (a->isCheckable()) {
            t += a->isChecked() ? QStringLiteral(" (checked)") : QStringLiteral(" (unchecked)");
        }
        if (!a->isEnabled()) {
            t += QStringLiteral(" (disabled)");
        }
        items.append(t);
    }
    return items;
}

// Where `w` is on the desktop, from window geometry: a popup window that
// never reaches the screen (WA_DontShowOnScreen) keeps a stale platform
// position, so mapToGlobal would put every menu at 0,0.
QPoint globalOf(QWidget* w)
{
    QWidget* win = w->window();
    return win->geometry().topLeft() + (w == win ? QPoint(0, 0) : w->mapTo(win, QPoint(0, 0)));
}

QPoint offsetIn(QWidget* base, QWidget* w)
{
    return globalOf(w) - globalOf(base);
}

// compositeOver with window-geometry positions.
void paintOver(CapturedImage& img, QWidget* base, QWidget* top)
{
    if (top == nullptr || !top->isVisible()) {
        return;
    }
    const QImage over = grabWidget(top);
    QPainter painter(&img.image);
    painter.drawImage(QRect(offsetIn(base, top), top->size()), over);
}

// Paints `tops` into a grab of `base` where they sit on screen, then adds
// `surface` alone.
void appendPopupImages(SceneContext& ctx, const QString& name, QWidget* base, const QList<QWidget*>& tops,
                       QWidget* surface)
{
    CapturedImage img;
    img.name = name;
    img.kind = base == ctx.window ? QStringLiteral("window") : QStringLiteral("dialog");
    img.image = grabWidget(base);
    img.logicalSize = base->size();
    img.dpr = img.image.devicePixelRatio();
    attachRegions(img, base);
    QJsonArray regions = img.extra.value(QStringLiteral("regions")).toArray();
    for (QWidget* top : tops) {
        if (top == nullptr || !top->isVisible() || top == base) {
            continue;
        }
        paintOver(img, base, top);
        appendRegions(regions, top, offsetIn(base, top), img.logicalSize);
    }
    img.extra[QStringLiteral("regions")] = regions;
    QJsonObject facts;
    if (surface != nullptr) {
        facts[QStringLiteral("surface_class")] = QString::fromLatin1(surface->metaObject()->className());
        facts[QStringLiteral("surface_object")] = surface->objectName();
        facts[QStringLiteral("surface_rect")] = regionRectJson(QRect(offsetIn(base, surface), surface->size()));
        if (auto* menu = qobject_cast<QMenu*>(surface)) {
            facts[QStringLiteral("menu_items")] = menuItemsJson(menu);
        }
        if (auto* label = qobject_cast<QLabel*>(surface)) {
            facts[QStringLiteral("text")] = label->text();
        }
    }
    for (auto it = facts.constBegin(); it != facts.constEnd(); ++it) {
        img.extra[it.key()] = it.value();
    }
    ctx.images.append(img);
    if (surface == nullptr) {
        return;
    }
    CapturedImage alone;
    alone.name = name + QStringLiteral("--popup");
    alone.kind = QStringLiteral("popup");
    alone.image = grabWidget(surface);
    alone.logicalSize = surface->size();
    alone.dpr = alone.image.devicePixelRatio();
    attachRegions(alone, surface);
    for (auto it = facts.constBegin(); it != facts.constEnd(); ++it) {
        alone.extra[it.key()] = it.value();
    }
    alone.extra[QStringLiteral("source")] = name;
    ctx.images.append(alone);
}

// The first item a pointer would land on, highlighted as under the pointer.
void highlightFirst(QMenu* menu)
{
    for (QAction* a : menu->actions()) {
        if (a->isSeparator() || !a->isVisible() || !a->isEnabled() || a->menu() != nullptr
            || qobject_cast<QWidgetAction*>(a) != nullptr) {
            continue;
        }
        menu->setActiveAction(a);
        return;
    }
}

// Every visible popup window but tooltips.
QSet<QWidget*> visiblePopups()
{
    QSet<QWidget*> out;
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (w->isVisible() && w->windowType() == Qt::Popup && !w->inherits("QTipLabel")) {
            out.insert(w);
        }
    }
    return out;
}

// The popup that opened since `before`: the active popup when there is
// one, else a new visible popup window.
QWidget* newPopup(const QSet<QWidget*>& before)
{
    QWidget* active = QApplication::activePopupWidget();
    if (active != nullptr && !before.contains(active)) {
        return active;
    }
    QWidget* found = nullptr;
    for (QWidget* w : visiblePopups()) {
        if (!before.contains(w) && (found == nullptr || qobject_cast<QMenu*>(found) == nullptr)) {
            found = w;
        }
    }
    return found;
}

void closePopups()
{
    for (int i = 0; i < 16; ++i) {
        QWidget* p = QApplication::activePopupWidget();
        if (p == nullptr) {
            const QSet<QWidget*> open = visiblePopups();
            if (open.isEmpty()) {
                return;
            }
            p = *open.cbegin();
        }
        p->close();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
}

// The top-left that puts `top` at `anchor` inside `base`: pushed left (or,
// for a submenu, flipped to the left of its menu at `flipX`) and up where it
// would run past the base's edge, as Qt keeps a popup on its screen.
QPoint placeInBase(QWidget* top, const QPoint& anchor, QWidget* base, bool submenu, int flipX = 0)
{
    const QRect r(globalOf(base), base->size());
    QPoint p = anchor;
    if (p.x() + top->width() > r.x() + r.width()) {
        p.setX(submenu ? flipX - top->width() : r.x() + r.width() - top->width());
    }
    if (p.y() + top->height() > r.y() + r.height()) {
        p.setY(r.y() + r.height() - top->height());
    }
    p.setX(std::max(p.x(), r.x()));
    p.setY(std::max(p.y(), r.y()));
    return p;
}

QPoint below(QWidget* w)
{
    return globalOf(w) + QPoint(0, w->height());
}

QPoint globalAt(QWidget* w, const QPoint& local)
{
    return globalOf(w) + local;
}

void shootOpenPopup(SceneContext& ctx, const QString& name, QWidget* top, const PopupShot& shot)
{
    QWidget* base = shot.base != nullptr ? shot.base : ctx.window;
    for (const QJsonValue& v : stopTimersUnder(top)) {
        ctx.stoppedTimers.append(v);
    }
    // Where the product asks for the popup, and where Qt put it.
    const QPoint asked = shot.anchor.value_or(top->pos());
    const QPoint qtAt = top->pos() - globalOf(base);
    top->move(placeInBase(top, asked, base, false));
    tagProvenance(top, shot.provenance.isEmpty() ? provRetained() : shot.provenance, false);
    auto* menu = qobject_cast<QMenu*>(top);
    if (menu != nullptr) {
        highlightFirst(menu);
    }
    pump(kFinalSettleMs);
    // A popup can still change size after it is shown: with the workspace
    // look on, the live pass raises a slider thinner than the decided
    // handle when the slider is shown, and the popup's layout grows the
    // popup on a later turn of the event loop. Placed before that, a popup
    // near the base's lower edge ran past it by the pixels it grew (the
    // DSP popups with sliders at laptop-1280x800@1, 5 px), or not, by how
    // the events fell. So it is placed again from the size it settled at,
    // until the place holds.
    for (int pass = 0; pass < kPopupPlacePasses; ++pass) {
        const QPoint at = placeInBase(top, asked, base, false);
        if (at == top->pos()) {
            break;
        }
        top->move(at);
        pump(kFinalSettleMs);
    }
    {
        const QPoint shown = top->pos() - globalOf(base);
        if (shown != qtAt) {
            ctx.notes.append(QStringLiteral("%1: placed at %2,%3 of the base, where the product asks for it "
                                            "and kept inside the base (Qt had kept it on the desktop screen, "
                                            "at %4,%5)")
                                 .arg(name).arg(shown.x()).arg(shown.y()).arg(qtAt.x()).arg(qtAt.y()));
        }
    }
    QList<QWidget*> tops = shot.under;
    tops.append(top);
    appendPopupImages(ctx, name, base, tops, top);
    if (menu == nullptr || !shot.submenus) {
        return;
    }
    QSet<QString> used;
    for (QAction* a : menu->actions()) {
        QMenu* sub = a->menu();
        if (sub == nullptr || !a->isVisible()) {
            continue;
        }
        QString s = slug(a->text());
        for (int n = 2; used.contains(s); ++n) {
            s = slug(a->text()) + QStringLiteral("-%1").arg(n);
        }
        used.insert(s);
        if (!a->isEnabled()) {
            ctx.notes.append(QStringLiteral("%1: submenu \"%2\" is disabled; not opened").arg(name, a->text()));
            continue;
        }
        menu->setActiveAction(a);
        pump(kFinalSettleMs);
        if (!sub->isVisible()) {
            ctx.notes.append(QStringLiteral("%1: submenu \"%2\" did not open (it has no items)").arg(name, a->text()));
            continue;
        }
        // Beside its item, as Qt places it on a screen, inside the base.
        sub->move(placeInBase(sub, globalOf(menu) + menu->actionGeometry(a).topRight(), base, true,
                              globalOf(menu).x()));
        pump(kFinalSettleMs);
        for (const QJsonValue& v : stopTimersUnder(sub)) {
            ctx.stoppedTimers.append(v);
        }
        tagProvenance(sub, shot.provenance.isEmpty() ? provRetained() : shot.provenance, false);
        highlightFirst(sub);
        pump(kFinalSettleMs);
        QList<QWidget*> subTops = tops;
        subTops.append(sub);
        appendPopupImages(ctx, name + QStringLiteral("--sub-") + s, base, subTops, sub);
        sub->hide();
        pump(kFinalSettleMs);
    }
}

// Opens a popup through `open` (which may block in QMenu::exec) and grabs
// it from a deferred timer. False when it did not open.
bool shootPopup(SceneContext& ctx, const QString& name, const std::function<void()>& open,
                const PopupShot& shot = {})
{
    if (!ctx.failure.isEmpty()) {
        return false;
    }
    enum class State { Waiting, Shot, None };
    auto state = std::make_shared<State>(State::Waiting);
    const QSet<QWidget*> before = visiblePopups();
    QTimer::singleShot(0, ctx.window, [&ctx, name, shot, state, before] {
        QElapsedTimer t;
        t.start();
        QWidget* top = nullptr;
        while (top == nullptr && t.elapsed() < kPopupWaitMs) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            top = shot.find ? shot.find() : newPopup(before);
            if (top != nullptr && !top->isVisible()) {
                top = nullptr;
            }
        }
        if (top == nullptr) {
            *state = State::None;
            // Whatever opened late is closed so a blocking exec returns.
            for (QWidget* w : visiblePopups()) {
                if (!before.contains(w)) {
                    w->close();
                }
            }
            return;
        }
        shootOpenPopup(ctx, name, top, shot);
        if (shot.find && top->isVisible() && !top->isWindow()) {
            top->hide();
        }
        closePopups();
        *state = State::Shot;
    });
    open();
    QElapsedTimer t;
    t.start();
    while (*state == State::Waiting && t.elapsed() < kPopupWaitMs + kPopupGraceMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    closePopups();
    pump(kFinalSettleMs);
    if (*state == State::Shot) {
        return true;
    }
    if (!shot.optionalReason.isEmpty()) {
        noteNotShown(ctx, name, shot.optionalReason);
        return false;
    }
    if (ctx.failure.isEmpty()) {
        ctx.failure = QStringLiteral("%1: the popup did not open").arg(name);
    }
    return false;
}

void sendContextMenu(QWidget* w, const QPoint& local)
{
    QContextMenuEvent event(QContextMenuEvent::Mouse, local, w->mapToGlobal(local));
    QCoreApplication::sendEvent(w, &event);
}

void sendMouse(QWidget* w, QEvent::Type type, const QPoint& local, Qt::MouseButton button,
               Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QMouseEvent event(type, QPointF(local), QPointF(w->mapTo(w->window(), local)), QPointF(w->mapToGlobal(local)),
                      button, buttons, modifiers);
    QCoreApplication::sendEvent(w, &event);
}

QPoint middleOf(QWidget* w)
{
    return QPoint(w->width() / 2, w->height() / 2);
}

// Where Qt puts a tooltip for its default 16x16 pointer at `pointer`
// (QTipLabel::placeTip's offset and screen-edge rules). QTipLabel offsets by
// the platform pointer's size; on macOS that is NSCursor.currentSystemCursor,
// the desktop's pointer shape at that moment, so the tip moved from launch
// to launch. The gallery places it for the default pointer instead.
QPoint arrowTipPosition(QWidget* tip, const QPoint& pointer)
{
    const QScreen* screen = QGuiApplication::screenAt(pointer);
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    const QSize cursor(16, 16);
    QPoint offset(2, cursor.height());
    if (cursor.height() > 2 * tip->height()) {
        offset = QPoint(cursor.width() / 2, 0);
    }
    QPoint p = pointer + offset;
    const QRect s = screen->geometry();
    if (p.x() + tip->width() > s.x() + s.width()) {
        p.rx() -= 4 + tip->width();
    }
    if (p.y() + tip->height() > s.y() + s.height()) {
        p.ry() -= 24 + tip->height();
    }
    if (p.y() < s.y()) {
        p.setY(s.y());
    }
    if (p.x() + tip->width() > s.x() + s.width()) {
        p.setX(s.x() + s.width() - tip->width());
    }
    if (p.x() < s.x()) {
        p.setX(s.x());
    }
    if (p.y() + tip->height() > s.y() + s.height()) {
        p.setY(s.y() + s.height() - tip->height());
    }
    return p;
}

QString placeTipForArrow(QWidget* tip, const QPoint& pointer)
{
    const QPoint qtAt = tip->pos();
    const QPoint at = arrowTipPosition(tip, pointer);
    tip->move(at);
    return QStringLiteral("tooltip offset from the pointer: Qt %1,%2 (desktop pointer size), shown %3,%4 "
                          "(default 16x16 pointer)")
        .arg(qtAt.x() - pointer.x()).arg(qtAt.y() - pointer.y())
        .arg(at.x() - pointer.x()).arg(at.y() - pointer.y());
}

// A tooltip shown by `show` (a help event or the product's mouse path) with
// the pointer at `pointer` (global), grabbed over the window.
bool shootToolTip(SceneContext& ctx, const QString& name, const QPoint& pointer, const std::function<void()>& show,
                  const QString& expectText = {})
{
    if (!ctx.failure.isEmpty()) {
        return false;
    }
    QToolTip::hideText();
    pump(kFinalSettleMs);
    show();
    pump(kFinalSettleMs);
    QWidget* tip = visibleToolTip();
    auto* label = qobject_cast<QLabel*>(tip);
    if (tip == nullptr || (!expectText.isEmpty() && (label == nullptr || !label->text().contains(expectText)))) {
        ctx.failure = QStringLiteral("%1: the tooltip did not show (showing: %2)")
                          .arg(name, label != nullptr ? label->text() : QStringLiteral("nothing"));
        return false;
    }
    ctx.notes.append(QStringLiteral("%1: %2").arg(name, placeTipForArrow(tip, pointer)));
    pump(kFinalSettleMs);
    appendPopupImages(ctx, name, ctx.window, {tip}, tip);
    QToolTip::hideText();
    pump(kFinalSettleMs);
    return true;
}

// Refuses a toast's close until the grab is done: its dismiss timer is a
// member QTimer (no parent), so stopTimersUnder cannot reach it.
class HoldOpen final : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Close) {
            event->ignore();
            return true;
        }
        return QObject::eventFilter(watched, event);
    }
};

template <typename T>
T* newestVisible(const QSet<QWidget*>& before)
{
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (!before.contains(w) && w->isVisible()) {
            if (auto* t = qobject_cast<T*>(w)) {
                return t;
            }
        }
    }
    return nullptr;
}

QSet<QWidget*> topLevelSet()
{
    const QWidgetList list = QApplication::topLevelWidgets();
    return QSet<QWidget*>(list.cbegin(), list.cend());
}

template <typename T>
bool shootToast(SceneContext& ctx, const QString& name, const QString& how, const std::function<void()>& raise)
{
    if (!ctx.failure.isEmpty()) {
        return false;
    }
    const QSet<QWidget*> before = topLevelSet();
    raise();
    T* toast = newestVisible<T>(before);
    if (toast == nullptr) {
        ctx.failure = QStringLiteral("%1: the toast did not show").arg(name);
        return false;
    }
    HoldOpen hold;
    toast->installEventFilter(&hold);
    ctx.stoppedTimers.append(QJsonObject{
        {QStringLiteral("parent"), QString::fromLatin1(toast->metaObject()->className())},
        {QStringLiteral("name"), QStringLiteral("dismiss timer (member QTimer)")},
        {QStringLiteral("held"), QStringLiteral("its close is refused until after the grab")}});
    tagProvenance(toast, provRetained(), false);
    pump(kFinalSettleMs);
    ctx.notes.append(QStringLiteral("%1: %2").arg(name, how));
    appendPopupImages(ctx, name, ctx.window, {toast}, toast);
    toast->removeEventFilter(&hold);
    toast->close();
    pump(kFinalSettleMs);
    return true;
}

// The two-band window, posed and frozen, then `shoot`.
void runPopupScene(SceneContext& ctx, const std::function<void(SceneContext&)>& shoot,
                   const std::function<void(SceneContext&)>& beforeFreeze = {})
{
    runWindowScene(ctx, twoBandPoses(), 0, beforeFreeze);
    if (!ctx.failure.isEmpty()) {
        return;
    }
    shoot(ctx);
}

VfoWidget* flagA(SceneContext& ctx)
{
    return flagForSlice(ctx.window, sliceOnPan(ctx.window, panIdAt(0)));
}

template <typename T>
T* namedChild(QWidget* root, const QString& name)
{
    return root != nullptr ? root->findChild<T*>(name) : nullptr;
}

QPushButton* buttonWithText(QWidget* root, const QString& text)
{
    if (root == nullptr) {
        return nullptr;
    }
    for (QPushButton* b : root->findChildren<QPushButton*>()) {
        if (b->text() == text) {
            return b;
        }
    }
    return nullptr;
}

// Opens the flag tab holding `inner` (a click on its tab, as a user opens it).
bool openFlagTabFor(SceneContext& ctx, VfoWidget* flag, QWidget* inner)
{
    QWidget* page = inner;
    QStackedWidget* stack = nullptr;
    while (page != nullptr && page->parentWidget() != nullptr && page->parentWidget() != flag) {
        if (auto* s = qobject_cast<QStackedWidget*>(page->parentWidget())) {
            stack = s;
            break;
        }
        page = page->parentWidget();
    }
    if (stack == nullptr) {
        return true;
    }
    const int index = stack->indexOf(page);
    QList<QPushButton*> tabs;
    for (QPushButton* b : flag->findChildren<QPushButton*>(QString(), Qt::FindDirectChildrenOnly)) {
        if (b->isCheckable() && b->toolTip().startsWith(QLatin1String("Show/hide"))) {
            tabs.append(b);
        }
    }
    if (index < 0 || index >= tabs.size()) {
        ctx.failure = QStringLiteral("no flag tab button for page %1").arg(index);
        return false;
    }
    if (!stack->isVisible() || stack->currentIndex() != index) {
        tabs.at(index)->click();
    }
    pump(kFinalSettleMs);
    ctx.notes.append(QStringLiteral("flag tab \"%1\" opened by a click on its tab")
                         .arg(tabs.at(index)->text().isEmpty() ? tabs.at(index)->objectName() : tabs.at(index)->text()));
    return inner->isVisible();
}

// Every readable property of `o`, so a scene can show it changed nothing.
QJsonObject propertySnapshot(QObject* o)
{
    QJsonObject out;
    const QMetaObject* mo = o->metaObject();
    for (int i = 0; i < mo->propertyCount(); ++i) {
        const QMetaProperty p = mo->property(i);
        if (!p.isReadable()) {
            continue;
        }
        out.insert(QString::fromLatin1(p.name()), QJsonValue::fromVariant(p.read(o)));
    }
    return out;
}

// One DSP popup (C34 to C40): the flag's DSP tab, a right-click on the
// button, as live; every slice property is the same afterwards.
void scenePopupDsp(SceneContext& ctx, const QString& name, const QString& button)
{
    runPopupScene(ctx, [name, button](SceneContext& c) {
        VfoWidget* flag = flagA(c);
        QPushButton* btn = buttonWithText(flag, button);
        if (btn == nullptr) {
            c.failure = QStringLiteral("no %1 button on the flag").arg(button);
            return;
        }
        if (!openFlagTabFor(c, flag, btn)) {
            if (c.failure.isEmpty()) {
                c.failure = QStringLiteral("the %1 button is not shown").arg(button);
            }
            return;
        }
        SliceModel* slice = flag->sliceForTest();
        const QJsonObject before = propertySnapshot(slice);
        if (!btn->isEnabled()) {
            // VfoWidget offers no quick controls for a filter that cannot
            // run here; the window with the disabled button carries the
            // record.
            noteNotShown(c, name, QStringLiteral("%1 is disabled because it cannot run on this computer (\"%2\"), "
                                                 "and VfoWidget offers no quick controls for it")
                                      .arg(button, btn->toolTip()));
            appendPopupImages(c, name, c.window, {}, nullptr);
            return;
        }
        // Where DspParamPopup::showAt puts it (Qt's position, kept inside
        // the window).
        shootPopup(c, name, [btn] { emit btn->customContextMenuRequested(middleOf(btn)); });
        const QJsonObject after = propertySnapshot(slice);
        if (before != after) {
            QStringList changed;
            for (auto it = before.constBegin(); it != before.constEnd(); ++it) {
                if (after.value(it.key()) != it.value()) {
                    changed.append(it.key());
                }
            }
            c.failure = QStringLiteral("%1 changed slice properties: %2").arg(name, changed.join(QStringLiteral(", ")));
            return;
        }
        c.notes.append(QStringLiteral("%1: right-click on %2 (customContextMenuRequested, as live); all %3 slice "
                                      "properties the same before and after")
                           .arg(name, button).arg(before.size()));
    });
}

// The spectrum of pan 1 (slice A, 40 m).
SpectrumWidget* panOne(SceneContext& ctx)
{
    return spectrumFor(ctx.window, panIdAt(0));
}

// Opens the flyout of pan 1's toolbar button `label` (a child panel, not a
// popup), grabs it and closes it. The flyout is the largest widget the
// click showed. The ANT button shows only on a board with antenna choices:
// the toolbar takes the posed board's capabilities, as MainWindow passes
// them on currentRadioChanged.
void shootPanFlyout(SceneContext& c, const QString& name, const QString& label)
{
    SpectrumWidget* s = panOne(c);
    auto* panel = s != nullptr ? s->findChild<SpectrumOverlayPanel*>() : nullptr;
    QPushButton* button = buttonWithText(panel, label);
    if (button != nullptr && !button->isVisible() && panel != nullptr) {
        panel->setBoardCapabilities(c.model->boardCapabilities());
        c.notes.append(QStringLiteral("SpectrumOverlayPanel::setBoardCapabilities(posed board), "
                                      "as MainWindow does on currentRadioChanged"));
        pump(kSettleMs);
    }
    if (button == nullptr || !button->isVisible()) {
        c.failure = QStringLiteral("no %1 button on pan 1").arg(label);
        return;
    }
    QSet<QWidget*> shown;
    for (QWidget* w : s->findChildren<QWidget*>()) {
        if (w->isVisible()) {
            shown.insert(w);
        }
    }
    button->click();
    pump(kFinalSettleMs);
    QWidget* flyout = nullptr;
    for (QWidget* w : s->findChildren<QWidget*>()) {
        if (!w->isVisible() || shown.contains(w)) {
            continue;
        }
        if (flyout == nullptr || w->width() * w->height() > flyout->width() * flyout->height()) {
            flyout = w;
        }
    }
    if (flyout == nullptr) {
        c.failure = QStringLiteral("the %1 flyout did not show").arg(label);
        return;
    }
    appendPopupImages(c, name, c.window, {}, flyout);
    button->click();
    pump(kFinalSettleMs);
}

// x of `hz` across the widget, from the displayed centre and bandwidth (the
// plot spans the widget less its dBm strip; close enough to aim at).
int approxX(SpectrumWidget* s, double hz)
{
    const double left = s->centerFrequency() - s->bandwidth() / 2.0;
    return int(std::lround((hz - left) / s->bandwidth() * s->width()));
}

qint64 sampleSpotTime(int minute)
{
    return QDateTime(QDate(2026, 10, 8), QTime(14, minute), QTimeZone::utc()).toMSecsSinceEpoch();
}

// Sample spots as a spot source would push them (no spot client exists):
// three single labels and seven at one frequency, so four overflow the
// three label rows into a cluster badge.
void placeSampleSpots(SceneContext& ctx, SpectrumWidget* s)
{
    const double c = s->centerFrequency();
    const double bw = s->bandwidth();
    QVector<SpectrumWidget::SpotMarker> spots;
    auto add = [&spots](int index, const QString& call, double hz, const QString& mode, const QString& source,
                        const QString& spotter, const QString& comment, int minute) {
        SpectrumWidget::SpotMarker m;
        m.index = index;
        m.callsign = call;
        m.freqMhz = hz / 1.0e6;
        m.mode = mode;
        m.source = source;
        m.spotterCallsign = spotter;
        m.comment = comment;
        m.timestampMs = sampleSpotTime(minute);
        spots.append(m);
    };
    const double single1 = std::round((c - bw * 0.34) / 100.0) * 100.0;
    const double single2 = std::round((c - bw * 0.22) / 100.0) * 100.0;
    const double single3 = std::round((c + bw * 0.14) / 100.0) * 100.0;
    const double cluster = std::round((c + bw * 0.30) / 100.0) * 100.0;
    add(1, QStringLiteral("K1AAA"), single1, QStringLiteral("SSB"), QStringLiteral("Cluster"),
        QStringLiteral("W2BBB"), QStringLiteral("59 in NH"), 12);
    add(2, QStringLiteral("VE3CCC"), single2, QStringLiteral("SSB"), QStringLiteral("RBN"),
        QStringLiteral("N4DDD"), QString(), 18);
    add(3, QStringLiteral("G4EEE"), single3, QStringLiteral("SSB"), QStringLiteral("POTA"),
        QStringLiteral("KG4FFF"), QStringLiteral("K-1234"), 25);
    const QStringList calls = {QStringLiteral("DL1GGG"), QStringLiteral("F5HHH"), QStringLiteral("EA3III"),
                               QStringLiteral("I2JJJ"), QStringLiteral("OH1KKK"), QStringLiteral("SP5LLL"),
                               QStringLiteral("JA1MMM")};
    for (int i = 0; i < calls.size(); ++i) {
        add(10 + i, calls.at(i), cluster + i * 50.0, QStringLiteral("SSB"), QStringLiteral("Cluster"),
            QStringLiteral("W1NNN"), QString(), 20 + i);
    }
    s->setSpotMarkers(spots);
    pump(kFinalSettleMs);
    grabWidget(s);
    pump(kFinalSettleMs);
    ctx.notes.append(QStringLiteral("sample spots set on pan 1 through SpectrumWidget::setSpotMarkers (no spot "
                                    "client constructed or started): %1 labels, %2 cluster badge(s)")
                         .arg(s->spotClickRectsForTest().size()).arg(s->spotClustersForTest().size()));
    for (const auto& cl : s->spotClustersForTest()) {
        ctx.notes.append(QStringLiteral("cluster badge at %1,%2 %3x%4 of pan 1's spectrum (widget %5x%6)")
                             .arg(cl.rect.x()).arg(cl.rect.y()).arg(cl.rect.width()).arg(cl.rect.height())
                             .arg(s->width()).arg(s->height()));
    }
}

// One notch on pan 1 through setNotchMarkers (display only; the notch model
// and the DSP are not touched).
int placeSampleNotch(SceneContext& ctx, SpectrumWidget* s)
{
    SpectrumWidget::NotchMarker n;
    n.id = 1;
    const double hz = std::round((s->centerFrequency() + s->bandwidth() * 0.06) / 10.0) * 10.0;
    n.freqMhz = hz / 1.0e6;
    n.widthHz = 200.0;
    n.active = true;
    s->setNotchMarkers({n});
    pump(kFinalSettleMs);
    ctx.notes.append(QStringLiteral("sample notch at %1 MHz, 200 Hz, through SpectrumWidget::setNotchMarkers "
                                    "(display only; no notch model or DSP change)")
                         .arg(n.freqMhz, 0, 'f', 6));
    return approxX(s, hz);
}

// Moves the pointer along the plot row `y` around `x0` until the notch's
// tooltip shows; the pointer's spot, or (-1, -1).
QPoint findNotchPointer(SpectrumWidget* s, int x0, int y)
{
    for (int dx = 0; dx <= 60; ++dx) {
        for (int sign : {1, -1}) {
            const QPoint at(x0 + sign * dx, y);
            sendMouse(s, QEvent::MouseMove, at, Qt::NoButton, Qt::NoButton);
            auto* tip = qobject_cast<QLabel*>(visibleToolTip());
            if (tip != nullptr && tip->text().contains(QLatin1String("Width:"))) {
                return at;
            }
        }
    }
    return QPoint(-1, -1);
}

QPoint rectMiddle(const QRect& r)
{
    return r.center();
}

// The dialog over the window, sized and centred as sceneEditDialog does.
ContainerSettingsDialog* openContainerSettings(SceneContext& ctx)
{
    auto* manager = ctx.window->findChild<ContainerManager*>();
    ContainerWidget* panel = mainPanel(ctx);
    if (manager == nullptr || panel == nullptr) {
        ctx.failure = QStringLiteral("no main panel container");
        return nullptr;
    }
    auto* dialog = new ContainerSettingsDialog(panel, ctx.window, manager);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->selectDraftContainer(panel->id());
    dialog->show();
    pump(kDialogSettleMs);
    const QSize fit = dialog->size().boundedTo(ctx.window->size() - QSize(kDialogInsetPx, kDialogInsetPx))
                          .expandedTo(dialog->minimumSizeHint());
    dialog->resize(fit);
    pump(kFinalSettleMs);
    dialog->move(ctx.window->mapToGlobal(ctx.window->rect().center()) - dialog->rect().center());
    pump(kFinalSettleMs);
    for (const QJsonValue& v : stopTimersUnder(dialog)) {
        ctx.stoppedTimers.append(v);
    }
    ctx.notes.append(QStringLiteral("Container Settings for the main panel, shown (not exec'd) at %1x%2 over the "
                                    "window, as the Containers > Edit Container path opens it")
                         .arg(dialog->width()).arg(dialog->height()));
    return dialog;
}

// One menu bar menu open, the menu bar in the window (the Windows and Linux
// arrangement).
// The window's menu bar: TitleBar::setMenuBar took it into the title bar,
// so QMainWindow::menuBar() would make a new, empty one.
QMenuBar* titleMenuBar(MainWindow* window)
{
    for (QMenuBar* bar : window->findChildren<QMenuBar*>()) {
        if (!bar->actions().isEmpty()) {
            return bar;
        }
    }
    return nullptr;
}

void sceneMenuBar(SceneContext& ctx, const QString& name, const QString& title)
{
    runPopupScene(ctx, [name, title](SceneContext& c) {
        QMenuBar* bar = titleMenuBar(c.window);
        if (bar == nullptr) {
            c.failure = QStringLiteral("no menu bar with menus");
            return;
        }
        QAction* menuAction = nullptr;
        for (QAction* a : bar->actions()) {
            if (QString(a->text()).remove(QLatin1Char('&')) == title) {
                menuAction = a;
            }
        }
        if (menuAction == nullptr || bar->isNativeMenuBar()) {
            c.failure = QStringLiteral("no in-window menu \"%1\" (actions %2, native %3)")
                            .arg(title).arg(bar->actions().size()).arg(bar->isNativeMenuBar());
            return;
        }
        if (!bar->isVisible()) {
            // Native, Qt hid the widget; in the window it is shown, as on
            // Windows and Linux.
            bar->show();
            pump(kFinalSettleMs);
        }
        if (!bar->isVisible()) {
            c.failure = QStringLiteral("the in-window menu bar is not visible (parent %1 visible %2)")
                            .arg(bar->parentWidget() ? QString::fromLatin1(bar->parentWidget()->metaObject()->className())
                                                     : QStringLiteral("none"))
                            .arg(bar->parentWidget() && bar->parentWidget()->isVisible());
            return;
        }
        c.notes.append(QStringLiteral(
            "menu bar in the window: QMenuBar::setNativeMenuBar(false) on the gallery window, so it sits in "
            "the title bar where MainWindow puts it (TitleBar::setMenuBar(menuBar()), MainWindow.cpp:1452, "
            "already done at construction). This is the Windows and Linux look; macOS draws its own menu "
            "bar, which Qt does not paint"));
        shootPopup(c, name, [bar, menuAction] { bar->setActiveAction(menuAction); },
                   {.anchor = globalAt(bar, bar->actionGeometry(menuAction).bottomLeft() + QPoint(0, 1))});
    }, [](SceneContext& c) {
        if (QMenuBar* bar = titleMenuBar(c.window)) {
            bar->setNativeMenuBar(false);
        }
    });
}

// Closes any toast a refused trigger raised, so the next image is clean.
void closeNewToasts(SceneContext& ctx)
{
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (w->isVisible() && qobject_cast<StatusToast*>(w) != nullptr) {
            ctx.notes.append(QStringLiteral("a toast was raised and closed: \"%1\"")
                                 .arg(w->findChild<QLabel*>() ? w->findChild<QLabel*>()->text() : QString()));
            w->close();
        }
    }
    pump(kFinalSettleMs);
}

// The connection tooltip's uptime, pinned (tooltip-connection).
constexpr auto kPinnedUptime = "connected 0m 05s";

// While it lives, every QTimer whose timeout reaches one of `targets` is cut
// as it fires and stopped, as cutTimersInto does for a fixed wait.
class TimerHold : public QObject
{
public:
    TimerHold(QList<QObject*> targets, QJsonArray* stopped)
        : m_targets(std::move(targets)), m_stopped(stopped)
    {
        qApp->installEventFilter(this);
    }
    ~TimerHold() override { qApp->removeEventFilter(this); }
    TimerHold(const TimerHold&) = delete;
    TimerHold& operator=(const TimerHold&) = delete;

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() != QEvent::Timer) {
            return false;
        }
        auto* timer = qobject_cast<QTimer*>(watched);
        if (timer == nullptr || !timer->isActive()) {
            return false;
        }
        for (QObject* target : m_targets) {
            if (QObject::disconnect(timer, &QTimer::timeout, target, nullptr)) {
                m_stopped->append(QJsonObject{
                    {QStringLiteral("parent"),
                     QStringLiteral("member timer feeding %1")
                         .arg(QString::fromLatin1(target->metaObject()->className()))},
                    {QStringLiteral("name"), timer->objectName()},
                    {QStringLiteral("interval_ms"), timer->interval()}});
                timer->stop();
            }
        }
        return false;
    }

private:
    QList<QObject*> m_targets;
    QJsonArray* m_stopped;
};

void recordMenuNotShown(SceneContext& ctx)
{
    for (const auto& [item, reason] : std::initializer_list<std::pair<const char*, const char*>>{
             {"menu: TunerApplet (C45)", "the applet needs a device setting that starts discovery or a device connection"},
             {"menu: AmpApplet (C46)", "the applet needs a device setting that starts discovery or a device connection"},
             {"menu: Rf2ksApplet (C47)", "the applet needs a device setting that starts discovery or a device connection"}}) {
        noteNotShown(ctx, QString::fromLatin1(item), QString::fromLatin1(reason));
    }
}

void appendMenuScenes(QVector<Scene>& scenes)
{
    auto add = [&scenes](const QString& name, const std::function<void(SceneContext&)>& run, bool previewOnly = false) {
        scenes.append({name, QStringLiteral("2v"), false, run, previewOnly});
    };

    // ---- title bar and status bar ----
    add(QStringLiteral("popup-connection-route"), [](SceneContext& ctx) {
        // C1: ConnectionSegment::showRoutePopup, the segment's own opener.
        runPopupScene(ctx, [](SceneContext& c) {
            auto* segment = c.window->findChild<ConnectionSegment*>();
            if (segment == nullptr) {
                c.failure = QStringLiteral("no connection segment");
                return;
            }
            shootPopup(c, QStringLiteral("popup-connection-route"), [segment] { segment->showRoutePopup(); },
                       {.anchor = below(segment),
                        .optionalReason = QStringLiteral("the route popup opens only while the segment is Connected")});
        });
    });
    add(QStringLiteral("menu-speaker"), [](SceneContext& ctx) {
        // C2: the speaker button's right-click (MasterOutputWidget::onSpeakerContextMenu).
        runPopupScene(ctx, [](SceneContext& c) {
            auto* speaker = namedChild<QPushButton>(c.window, QStringLiteral("speakerBtn"));
            if (speaker == nullptr) {
                c.failure = QStringLiteral("no speaker button");
                return;
            }
            c.notes.append(QStringLiteral("menu-speaker lists this computer's audio outputs, so its items "
                                          "differ from machine to machine"));
            shootPopup(c, QStringLiteral("menu-speaker"),
                       [speaker] { emit speaker->customContextMenuRequested(middleOf(speaker)); },
                       {.anchor = globalAt(speaker, middleOf(speaker))});
        });
    });
    add(QStringLiteral("menu-cpu-tile"), [](SceneContext& ctx) {
        // C3: the CPU tile's right-click (MainWindow::onCpuMenuRequested).
        runPopupScene(ctx, [](SceneContext& c) {
            auto* tile = c.window->findChild<SystemTile*>();
            if (tile == nullptr) {
                c.failure = QStringLiteral("no CPU tile");
                return;
            }
            shootPopup(c, QStringLiteral("menu-cpu-tile"),
                       [tile] { emit tile->customContextMenuRequested(middleOf(tile)); },
                       {.anchor = globalAt(tile, middleOf(tile))});
        });
    });
    add(QStringLiteral("menu-connection-segment"), [](SceneContext& ctx) {
        // C4: ConnectionSegment::contextMenuRequested (MainWindow::showSegmentContextMenu).
        runPopupScene(ctx, [](SceneContext& c) {
            auto* segment = c.window->findChild<ConnectionSegment*>();
            if (segment == nullptr) {
                c.failure = QStringLiteral("no connection segment");
                return;
            }
            const QPoint at = globalAt(segment, QPoint(12, segment->height()));
            shootPopup(c, QStringLiteral("menu-connection-segment"),
                       [segment, at] { emit segment->contextMenuRequested(at); }, {.anchor = at});
        });
    });
    add(QStringLiteral("menu-station"), [](SceneContext& ctx) {
        // C5: StationBlock::contextMenuRequested (MainWindow::showStationContextMenu).
        runPopupScene(ctx, [](SceneContext& c) {
            auto* station = c.window->findChild<StationBlock*>();
            if (station == nullptr) {
                c.failure = QStringLiteral("no station block");
                return;
            }
            const QPoint at = globalAt(station, QPoint(8, station->height()));
            shootPopup(c, QStringLiteral("menu-station"), [station, at] { emit station->contextMenuRequested(at); },
                       {.anchor = at,
                        .optionalReason = QStringLiteral("the station menu opens only while connected")});
        });
    });
    add(QStringLiteral("popup-slice-chooser"), [](SceneContext& ctx) {
        // C6: the picker's chooserRequested (MainWindow::openSliceChooser).
        runPopupScene(ctx, [](SceneContext& c) {
            auto* dashboard = c.window->findChild<RxDashboard*>();
            if (dashboard == nullptr) {
                c.failure = QStringLiteral("no receiver dashboard");
                return;
            }
            shootPopup(c, QStringLiteral("popup-slice-chooser"), [dashboard] { emit dashboard->chooserRequested(); });
            noteNotShown(c, QStringLiteral("menu-reveal-slice (C7)"),
                         QStringLiteral("MainWindow::revealSliceInWindow shows its menu only for a slice this "
                                        "window does not show while every pan is taken and no larger layout fits "
                                        "the radio's pan limit; the two-band window always has a layout that fits"));
        });
    });

    // ---- workspace and containers ----
    add(QStringLiteral("menu-banner-applets"), [](SceneContext& ctx) {
        // C8 on the applets banner button: C17 (AppletPanelWidget) with the
        // switch off, C14 (ContainerContentHost) with it on.
        runPopupScene(ctx, [](SceneContext& c) {
            QPushButton* button = nullptr;
            for (const QString& id : {QStringLiteral("containerAppletsMenu"), QStringLiteral("appletPanelBannerButton")}) {
                for (QPushButton* b : c.window->findChildren<QPushButton*>(id)) {
                    if (button == nullptr && b->isVisible() && b->menu() != nullptr) {
                        button = b;
                    }
                }
            }
            if (button == nullptr) {
                c.failure = QStringLiteral("no visible applets banner button");
                return;
            }
            c.notes.append(QStringLiteral("menu-banner-applets: the menu of %1 (QPushButton::showMenu, as a click)")
                               .arg(button->objectName()));
            shootPopup(c, QStringLiteral("menu-banner-applets"), [button] { button->showMenu(); },
                       {.anchor = below(button)});
        });
    });
    add(QStringLiteral("menu-container-filter"), [](SceneContext& ctx) {
        // C9: the main panel's VFO filter right-click
        // (ContainerWidget::vfoFilterContextRequested -> onContainerFilterContext).
        runPopupScene(ctx, [](SceneContext& c) {
            ContainerWidget* panel = mainPanel(c);
            if (panel == nullptr) {
                c.failure = QStringLiteral("no main panel container");
                return;
            }
            // The VFO display's filter right-click names the slice's own
            // preset; when the posed filter is not a preset the product shows
            // a reason instead, so the first filter button's right-click
            // (the same menu for preset 0) is used.
            const bool viaVfo = shootPopup(c, QStringLiteral("menu-container-filter"),
                       [panel] { emit panel->vfoFilterContextRequested(); },
                       {.anchor = globalAt(panel, QPoint(40, 80)), .optionalReason = QStringLiteral("-")});
            if (viaVfo) {
                c.notes.append(QStringLiteral("menu-container-filter: vfoFilterContextRequested (the VFO display's "
                                              "filter right-click)"));
                return;
            }
            c.notShown.removeLast();
            closeNewToasts(c);
            c.notes.append(QStringLiteral("menu-container-filter: the posed filter is not one of this mode's "
                                          "presets, so the VFO display's right-click shows a reason; shown "
                                          "through filterContextRequested(0), the first filter button's "
                                          "right-click"));
            shootPopup(c, QStringLiteral("menu-container-filter"),
                       [panel] { emit panel->filterContextRequested(0); },
                       {.anchor = globalAt(panel, QPoint(40, 80))});
        });
    });
    add(QStringLiteral("menu-container-context"), [](SceneContext& ctx) {
        // C12: a right-click on the main panel container.
        runPopupScene(ctx, [](SceneContext& c) {
            ContainerWidget* panel = mainPanel(c);
            if (panel == nullptr) {
                c.failure = QStringLiteral("no main panel container");
                return;
            }
            shootPopup(c, QStringLiteral("menu-container-context"),
                       [panel] { sendContextMenu(panel, QPoint(40, 60)); },
                       {.anchor = globalAt(panel, QPoint(40, 60))});
        });
    });
    add(QStringLiteral("menu-container-settings"), [](SceneContext& ctx) {
        // C18 to C20 in Container Settings: Add (with its submenus), Presets
        // and Move to container.
        runPopupScene(ctx, [](SceneContext& c) {
            QPointer<ContainerSettingsDialog> dialog = openContainerSettings(c);
            if (!dialog) {
                return;
            }
            PopupShot over;
            over.base = dialog.data();
            auto overAt = [&over](QWidget* w) {
                PopupShot o = over;
                o.anchor = below(w);
                return o;
            };
            QPushButton* addButton = buttonWithText(dialog, QStringLiteral("+"));
            QPushButton* presets = buttonWithText(dialog, QStringLiteral("Presets…"));
            if (addButton == nullptr || presets == nullptr) {
                c.failure = QStringLiteral("no Add or Presets button in Container Settings");
            } else {
                shootPopup(c, QStringLiteral("menu-container-settings--add"), [addButton] { addButton->click(); }, overAt(addButton));
                shootPopup(c, QStringLiteral("menu-container-settings--presets"), [presets] { presets->click(); }, overAt(presets));
            }
            auto* move = namedChild<QPushButton>(dialog, QStringLiteral("moveContent"));
            if (move != nullptr && move->isVisible() && move->isEnabled()) {
                PopupShot moveAt = overAt(move);
                moveAt.optionalReason = QStringLiteral(
                    "Move to container lists the draft's other containers; with none to move to its menu "
                    "is empty and QMenu::popup shows nothing");
                shootPopup(c, QStringLiteral("menu-container-settings--move"), [move] { move->click(); }, moveAt);
            } else {
                noteNotShown(c, QStringLiteral("menu-container-settings--move (C18)"),
                             QStringLiteral("Move to container is shown only for a selected content row in a "
                                            "workspace with another container"));
            }
            if (dialog) {
                dialog->reject();
            }
        });
    });
    add(QStringLiteral("menu-container-options"), [](SceneContext& ctx) {
        // C10: the workspace header's options button (switch on only).
        runPopupScene(ctx, [](SceneContext& c) {
            ContainerWidget* panel = mainPanel(c);
            WorkspaceGroupHeader* header = panel != nullptr ? panel->workspaceHeader() : nullptr;
            if (header == nullptr) {
                c.failure = QStringLiteral("no workspace header on the main panel");
                return;
            }
            shootPopup(c, QStringLiteral("menu-container-options"), [header] { emit header->optionsClicked(); },
                       {.anchor = globalAt(header, QPoint(header->width(), header->height())),
                        .provenance = provProposed()});
        });
    }, true);
    add(QStringLiteral("menu-content-host"), [](SceneContext& ctx) {
        // C15/C16: an entry header's options button in the main panel
        // (switch on only), and the same items on the entry's right-click.
        runPopupScene(ctx, [](SceneContext& c) {
            ContainerWidget* panel = mainPanel(c);
            WorkspaceEntryHeader* entry = nullptr;
            if (panel != nullptr) {
                for (WorkspaceEntryHeader* h : panel->findChildren<WorkspaceEntryHeader*>()) {
                    if (entry == nullptr && h->isVisible()) {
                        entry = h;
                    }
                }
            }
            if (entry == nullptr) {
                c.failure = QStringLiteral("no visible entry header in the main panel");
                return;
            }
            c.notes.append(QStringLiteral("entry \"%1\"").arg(entry->title()));
            shootPopup(c, QStringLiteral("menu-content-host"), [entry] { emit entry->optionsClicked(); },
                       {.anchor = globalAt(entry, QPoint(entry->width(), entry->height())),
                        .provenance = provProposed()});
            shootPopup(c, QStringLiteral("menu-content-host--context"),
                       [entry] { sendContextMenu(entry, QPoint(entry->width() / 3, entry->height() / 2)); },
                       {.anchor = globalAt(entry, QPoint(entry->width() / 3, entry->height() / 2)),
                        .provenance = provProposed()});
            noteNotShown(c, QStringLiteral("menu: canvas entry (C13)"),
                         QStringLiteral("needs a container in the free canvas layout; it offers the same "
                                        "addEntryActions items as the entry menus shown here"));
        });
    }, true);
    add(QStringLiteral("menu-profile-rail"), [](SceneContext& ctx) {
        // C22: a right-click on a badge of the fanned-out rail (switch on only).
        sceneProfile(ctx, QStringLiteral("menu-profile-rail--window"), SceneProfile::Ssb, [](SceneContext& c) {
            ProfileRailDock* dock = c.window->profileRailDock();
            dock->open();
            hideStaleRailBadges(dock->rail());
            pump(kFinalSettleMs);
        }, [](SceneContext& c) {
            ProfileRailDock* dock = c.window->profileRailDock();
            WorkspaceProfiles* profiles = c.window->workspaceProfiles();
            QPushButton* badge = profiles->ids().size() > 1 ? dock->rail()->badgeButton(profiles->ids().at(1)) : nullptr;
            if (badge == nullptr) {
                c.failure = QStringLiteral("no second badge in the rail");
                return;
            }
            shootPopup(c, QStringLiteral("menu-profile-rail"),
                       [badge] { emit badge->customContextMenuRequested(middleOf(badge)); },
                       {.anchor = globalAt(badge, middleOf(badge)),
                        .provenance = provProposed()});
        });
    }, true);

    // ---- spectrum ----
    add(QStringLiteral("menu-spectrum-context"), [](SceneContext& ctx) {
        // C24 (a right-click on a spot label) and C23 (on a notch).
        runPopupScene(ctx, [](SceneContext& c) {
            SpectrumWidget* s = panOne(c);
            placeSampleSpots(c, s);
            QPoint spot(-1, -1);
            for (const auto& hit : s->spotClickRectsForTest()) {
                if (spot.x() < 0 && hit.markerIndex == 0) {
                    spot = rectMiddle(hit.rect);
                }
            }
            if (spot.x() < 0) {
                c.failure = QStringLiteral("the K1AAA spot label was not placed");
                return;
            }
            shootPopup(c, QStringLiteral("menu-spectrum-context--spot"), [s, spot] {
                sendMouse(s, QEvent::MouseButtonPress, spot, Qt::RightButton, Qt::RightButton);
            }, {.anchor = globalAt(s, spot)});
            const int x = placeSampleNotch(c, s);
            const QPoint notch = findNotchPointer(s, x, s->height() / 5);
            QToolTip::hideText();
            if (notch.x() < 0) {
                c.failure = QStringLiteral("the notch was not found under the pointer");
                return;
            }
            shootPopup(c, QStringLiteral("menu-spectrum-context--notch"), [s, notch] {
                sendMouse(s, QEvent::MouseButtonPress, notch, Qt::RightButton, Qt::RightButton);
            }, {.anchor = globalAt(s, notch)});
        });
    });
    add(QStringLiteral("menu-spot-cluster"), [](SceneContext& ctx) {
        // C25: a click on the cluster badge (SpectrumWidget::showSpotClusterPopup).
        runPopupScene(ctx, [](SceneContext& c) {
            SpectrumWidget* s = panOne(c);
            placeSampleSpots(c, s);
            if (s->spotClustersForTest().isEmpty()) {
                c.failure = QStringLiteral("no cluster badge");
                return;
            }
            const QPoint at = rectMiddle(s->spotClustersForTest().constFirst().rect);
            shootPopup(c, QStringLiteral("menu-spot-cluster"), [s, at] {
                sendMouse(s, QEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton);
            }, {.anchor = globalAt(s, at)});
        });
    });
    add(QStringLiteral("popup-spectrum-overlay"), [](SceneContext& ctx) {
        // C26: a right-click on the plot away from spots and notches.
        runPopupScene(ctx, [](SceneContext& c) {
            SpectrumWidget* s = panOne(c);
            const QPoint at(s->width() * 2 / 3, s->height() / 5);
            shootPopup(c, QStringLiteral("popup-spectrum-overlay"), [s, at] {
                sendMouse(s, QEvent::MouseButtonPress, at, Qt::RightButton, Qt::RightButton);
            }, {.anchor = globalAt(s, at)});
        });
    });
    add(QStringLiteral("menu-pan-flyout-band"), [](SceneContext& ctx) {
        // C27: the BAND flyout of pan 1's toolbar (a child panel, not a popup).
        runPopupScene(ctx, [](SceneContext& c) {
            shootPanFlyout(c, QStringLiteral("menu-pan-flyout-band"), QStringLiteral("BAND"));
        });
    });
    add(QStringLiteral("menu-pan-flyout-ant"), [](SceneContext& ctx) {
        // The ANT flyout of pan 1's toolbar: RX and TX antenna rows.
        runPopupScene(ctx, [](SceneContext& c) {
            shootPanFlyout(c, QStringLiteral("menu-pan-flyout-ant"), QStringLiteral("ANT"));
        });
    });
    add(QStringLiteral("menu-pan-flyout-att"), [](SceneContext& ctx) {
        // The ATT flyout of pan 1's toolbar: the step attenuator check box and level.
        runPopupScene(ctx, [](SceneContext& c) {
            shootPanFlyout(c, QStringLiteral("menu-pan-flyout-att"), QStringLiteral("ATT"));
        });
    });
    add(QStringLiteral("menu-panadapter"), [](SceneContext& ctx) {
        // C28: a right-click on pan 1's applet (PanadapterApplet::contextMenuEvent).
        runPopupScene(ctx, [](SceneContext& c) {
            PanadapterApplet* applet = appletFor(c.window, panIdAt(0));
            if (applet == nullptr) {
                c.failure = QStringLiteral("no applet for pan 1");
                return;
            }
            shootPopup(c, QStringLiteral("menu-panadapter"),
                       [applet] { sendContextMenu(applet, QPoint(applet->width() / 2, applet->height() / 3)); },
                       {.anchor = globalAt(applet, QPoint(applet->width() / 2, applet->height() / 3))});
        });
    });
    add(QStringLiteral("menu-smeter"), [](SceneContext& ctx) {
        // C29: a right-click on the Main Panel's SIGNAL STRENGTH dial.
        runPopupScene(ctx, [](SceneContext& c) {
            SMeterWidget* meter = nullptr;
            for (SMeterWidget* m : allMeters(c)) {
                if (meter == nullptr && m->isVisible() && m->settingsStore() != SMeterWidget::SettingsStore::Instance) {
                    meter = m;
                }
            }
            if (meter == nullptr) {
                c.failure = QStringLiteral("no visible Main Panel dial");
                return;
            }
            shootPopup(c, QStringLiteral("menu-smeter"), [meter] { sendContextMenu(meter, middleOf(meter)); },
                       {.anchor = globalAt(meter, middleOf(meter))});
        });
    });
    add(QStringLiteral("menu-dial"), [](SceneContext& ctx) {
        // sMeter.dial: an added SIGNAL STRENGTH dial's right-click (switch on only).
        sceneProfile(ctx, QStringLiteral("menu-dial--window"), SceneProfile::Dx, [](SceneContext& c) {
            feedDials(c);
        }, [](SceneContext& c) {
            const QList<SMeterWidget*> copies = dialCopies(c);
            SMeterWidget* dial = nullptr;
            for (SMeterWidget* d : copies) {
                if (dial == nullptr && d->isVisible()) {
                    dial = d;
                }
            }
            if (dial == nullptr) {
                c.failure = QStringLiteral("no visible added dial");
                return;
            }
            c.notes.append(QStringLiteral("added dial in %1").arg(holderName(dial)));
            // The dials' member timers (peak reset every 10 s from
            // construction, peak decay, needle animation) are cut as they
            // fire while the menu and its submenus are taken, so a reset
            // landing between two images cannot move the peak.
            QList<QObject*> held;
            for (SMeterWidget* d : copies) {
                held.append(d);
            }
            TimerHold hold(held, &c.stoppedTimers);
            shootPopup(c, QStringLiteral("menu-dial"), [dial] { sendContextMenu(dial, middleOf(dial)); },
                       {.anchor = globalAt(dial, middleOf(dial)),
                        .provenance = provProposed()});
        }, false, QString::fromLatin1(SMeterDialContent::kDialType));
    }, true);

    // ---- receiver flag ----
    add(QStringLiteral("menu-vfo-rx-antenna"), [](SceneContext& ctx) {
        // C30: a click on the flag's RX antenna button.
        runPopupScene(ctx, [](SceneContext& c) {
            auto* btn = namedChild<QPushButton>(flagA(c), QStringLiteral("m_rxAntBtn"));
            if (btn == nullptr || !btn->isVisible()) {
                c.failure = QStringLiteral("no RX antenna button on the flag");
                return;
            }
            shootPopup(c, QStringLiteral("menu-vfo-rx-antenna"), [btn] { btn->click(); }, {.anchor = below(btn)});
        });
    });
    add(QStringLiteral("menu-vfo-tx-antenna"), [](SceneContext& ctx) {
        // C31: a click on the flag's TX antenna button.
        runPopupScene(ctx, [](SceneContext& c) {
            auto* btn = namedChild<QPushButton>(flagA(c), QStringLiteral("m_txAntBtn"));
            if (btn == nullptr || !btn->isVisible()) {
                c.failure = QStringLiteral("no TX antenna button on the flag");
                return;
            }
            shootPopup(c, QStringLiteral("menu-vfo-tx-antenna"), [btn] { btn->click(); }, {.anchor = below(btn)});
        });
    });
    add(QStringLiteral("menu-vfo-filter-preset"), [](SceneContext& ctx) {
        // C32: a right-click on the flag's selected filter preset.
        runPopupScene(ctx, [](SceneContext& c) {
            VfoWidget* flag = flagA(c);
            QPushButton* preset = nullptr;
            for (QPushButton* b : flag->findChildren<QPushButton*>()) {
                if (b->property("filterLow").isValid() && (preset == nullptr || b->isChecked())) {
                    if (preset == nullptr || !preset->isChecked()) {
                        preset = b;
                    }
                }
            }
            if (preset == nullptr || !openFlagTabFor(c, flag, preset)) {
                if (c.failure.isEmpty()) {
                    c.failure = QStringLiteral("no filter preset button shown on the flag");
                }
                return;
            }
            shootPopup(c, QStringLiteral("menu-vfo-filter-preset"),
                       [preset] { emit preset->customContextMenuRequested(middleOf(preset)); },
                       {.anchor = globalAt(preset, middleOf(preset))});
        });
    });
    add(QStringLiteral("menu-vfo-context"), [](SceneContext& ctx) {
        // C33: a right-click on the flag.
        runPopupScene(ctx, [](SceneContext& c) {
            VfoWidget* flag = flagA(c);
            shootPopup(c, QStringLiteral("menu-vfo-context"),
                       [flag] { sendContextMenu(flag, QPoint(flag->width() / 2, 12)); },
                       {.anchor = globalAt(flag, QPoint(flag->width() / 2, 12))});
        });
    });
    for (const auto& [scene, button] : std::initializer_list<std::pair<const char*, const char*>>{
             {"popup-dsp-nr1", "NR1"}, {"popup-dsp-nr2", "NR2"}, {"popup-dsp-nr3", "NR3"},
             {"popup-dsp-nr4", "NR4"}, {"popup-dsp-dfnr", "DFNR"}, {"popup-dsp-mnr", "MNR"},
             {"popup-dsp-nnr", "NNR"}}) {
        const QString name = QString::fromLatin1(scene);
        const QString label = QString::fromLatin1(button);
        add(name, [name, label](SceneContext& ctx) { scenePopupDsp(ctx, name, label); });
    }

    // ---- applets ----
    auto rxApplet = [](SceneContext& c) -> RxApplet* {
        RxApplet* applet = c.window->findChild<RxApplet*>();
        if (applet == nullptr || !applet->isVisible()) {
            c.failure = QStringLiteral("the RX applet is not shown");
            return nullptr;
        }
        return applet;
    };
    add(QStringLiteral("menu-rx-filter-preset"), [rxApplet](SceneContext& ctx) {
        // C43: a right-click on the RX applet's selected filter preset.
        runPopupScene(ctx, [rxApplet](SceneContext& c) {
            RxApplet* applet = rxApplet(c);
            QPushButton* preset = nullptr;
            if (applet != nullptr) {
                for (QPushButton* b : applet->findChildren<QPushButton*>()) {
                    if (b->isVisible() && b->isCheckable() && b->contextMenuPolicy() == Qt::CustomContextMenu
                        && b->objectName().isEmpty() && (preset == nullptr || (b->isChecked() && !preset->isChecked()))) {
                        preset = b;
                    }
                }
            }
            if (preset == nullptr) {
                if (c.failure.isEmpty()) {
                    c.failure = QStringLiteral("no filter preset button on the RX applet");
                }
                return;
            }
            c.notes.append(QStringLiteral("preset \"%1\"").arg(preset->text()));
            for (const char* item : {"menu: RX applet RX antenna (C41)", "menu: RX applet TX antenna (C42)"}) {
                noteNotShown(c, QString::fromLatin1(item),
                             QStringLiteral("the RX applet hides its antenna buttons for this window's board "
                                            "capabilities (RxApplet::setBoardCapabilities: no Alex or fewer than "
                                            "three antenna inputs with no radio connected); the same antenna "
                                            "menus are shown from the flag (menu-vfo-rx-antenna, "
                                            "menu-vfo-tx-antenna)"));
            }
            shootPopup(c, QStringLiteral("menu-rx-filter-preset"),
                       [preset] { emit preset->customContextMenuRequested(middleOf(preset)); },
                       {.anchor = globalAt(preset, middleOf(preset))});
        });
    });
    add(QStringLiteral("menu-rx-slice"), [rxApplet](SceneContext& ctx) {
        // C44: the RX applet's slice badge right-click (RxApplet::showSliceMenu).
        runPopupScene(ctx, [rxApplet](SceneContext& c) {
            RxApplet* applet = rxApplet(c);
            if (applet == nullptr) {
                return;
            }
            QLabel* badge = nullptr;
            for (QLabel* l : applet->findChildren<QLabel*>()) {
                if (badge == nullptr && l->contextMenuPolicy() == Qt::CustomContextMenu && l->isVisible()) {
                    badge = l;
                }
            }
            recordMenuNotShown(c);
            if (badge == nullptr) {
                noteNotShown(c, QStringLiteral("menu-rx-slice (C44)"), QStringLiteral("no slice badge is shown"));
                appendPopupImages(c, QStringLiteral("menu-rx-slice"), c.window, {}, nullptr);
                return;
            }
            const bool shown = shootPopup(c, QStringLiteral("menu-rx-slice"),
                       [badge] { emit badge->customContextMenuRequested(middleOf(badge)); },
                       {.anchor = globalAt(badge, middleOf(badge)),
                        .optionalReason = QStringLiteral(
                            "RxApplet::showSliceMenu shows nothing when the slice has no access actions "
                            "(an unshared slice on a window that is not hosting)")});
            if (!shown) {
                // The window carries the not-shown records (C44 to C47).
                appendPopupImages(c, QStringLiteral("menu-rx-slice"), c.window, {}, nullptr);
            }
        });
    });

    // ---- dialogs ----
    add(QStringLiteral("menu-freedv-reporter"), [](SceneContext& ctx) {
        // C50 (the dialog's menu bar, in the window as on Windows and Linux)
        // and C51 (a row's right-click), with one sample station in the
        // station list (no reporter client).
        runPopupScene(ctx, [](SceneContext& c) {
            FreeDVStationModel* stations = c.model->freeDvStationModel();
            FreeDVStation st;
            st.sid = QStringLiteral("preview-1");
            st.callsign = QStringLiteral("K1AAA");
            st.gridSquare = QStringLiteral("FN42");
            st.version = QStringLiteral("2.0.0");
            st.frequencyHz = 14236000;
            st.txMode = QStringLiteral("RADEV1");
            st.status = QStringLiteral("Active");
            const QDateTime at(QDate(2026, 10, 8), QTime(14, 30), QTimeZone::utc());
            st.lastUpdate = at;
            st.connectTime = at;
            stations->onStationAdded(st.sid, st);
            auto* dialog = new FreeDVReporterDialog(stations, nullptr, c.window);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            QMenuBar* bar = dialog->findChild<QMenuBar*>();
            if (bar != nullptr) {
                bar->setNativeMenuBar(false);
            }
            dialog->show();
            pump(kDialogSettleMs);
            cutTimersInto(c, dialog, kMemberTimerCutWaitMs);
            // The dialog's own timers (its 30 s idle sweep among them) are
            // children, which the cut above only reaches when they fire in
            // its window. The sweep removes the sample station, whose last
            // update is fixed in the past, so a scene that ran past 30 s
            // under load shot no row and failed (exit 4).
            for (const QJsonValue& v : stopTimersUnder(dialog)) {
                c.stoppedTimers.append(v);
            }
            c.notes.append(QStringLiteral("one sample station added through FreeDVStationModel::onStationAdded "
                                          "(no reporter client); the dialog's menu bar shown in the window "
                                          "(setNativeMenuBar(false)), as on Windows and Linux"));
            PopupShot over;
            over.base = dialog;
            if (bar == nullptr || !bar->isVisible()) {
                c.failure = QStringLiteral("no in-window FreeDV Reporter menu bar");
            } else {
                for (QAction* a : bar->actions()) {
                    const QString name = QStringLiteral("menu-freedv-reporter--bar-") + slug(a->text());
                    PopupShot at = over;
                    at.anchor = globalAt(bar, bar->actionGeometry(a).bottomLeft() + QPoint(0, 1));
                    shootPopup(c, name, [bar, a] { bar->setActiveAction(a); }, at);
                }
            }
            auto* table = dialog->findChild<QTableView*>();
            if (table != nullptr && table->model() != nullptr && table->model()->rowCount() > 0) {
                const QRect row = table->visualRect(table->model()->index(0, 1));
                const QPoint at = row.center();
                PopupShot rowAt = over;
                rowAt.anchor = globalAt(table->viewport(), at);
                shootPopup(c, QStringLiteral("menu-freedv-reporter--row"),
                           [table, at] { emit table->customContextMenuRequested(at); }, rowAt);
            } else if (c.failure.isEmpty()) {
                c.failure = QStringLiteral("the sample station has no row");
            }
            noteNotShown(c, QStringLiteral("menu: Setup > VAX Auto-detect (C49)"),
                         QStringLiteral("the Auto-detect button is a hidden legacy widget on Setup > VAX "
                                        "(AudioVaxPage.cpp, setVisible(false)), so its menu cannot open"));
            noteNotShown(c, QStringLiteral("menu: ConnectionPanel context (C48)"),
                         QStringLiteral("ConnectionPanel starts discovery when it is built (ConnectionPanel.cpp, "
                                        "disc->startDiscovery())"));
            dialog->close();
            stations->onStationRemoved(st.sid);
        });
    });

    // ---- menu bar (in the window, the Windows and Linux arrangement) ----
    for (const char* title : {"File", "Radio", "View", "DSP", "Band", "Mode", "Containers", "Tools", "Help"}) {
        const QString t = QString::fromLatin1(title);
        const QString name = QStringLiteral("menubar-") + slug(t);
        add(name, [name, t](SceneContext& ctx) { sceneMenuBar(ctx, name, t); });
    }

    // ---- toasts ----
    add(QStringLiteral("toast-info"), [](SceneContext& ctx) {
        runPopupScene(ctx, [](SceneContext& c) {
            auto* applet = c.window->findChild<RxApplet*>();
            if (applet == nullptr) {
                c.failure = QStringLiteral("no RX applet");
                return;
            }
            shootToast<StatusToast>(c, QStringLiteral("toast-info"),
                QStringLiteral("Info through MainWindow::showToast: RxApplet::transmitSettingRefused with "
                               "IStationLink::transmitSettingsUnavailableReason() (MainWindow.cpp, Info, 3000 ms)"),
                [applet] { emit applet->transmitSettingRefused(IStationLink::transmitSettingsUnavailableReason()); });
        });
    });
    add(QStringLiteral("toast-warning"), [](SceneContext& ctx) {
        runPopupScene(ctx, [](SceneContext& c) {
            TxInterlockPolicy* policy = c.model->txInterlockPolicy();
            if (policy == nullptr) {
                c.failure = QStringLiteral("no TX interlock policy");
                return;
            }
            shootToast<StatusToast>(c, QStringLiteral("toast-warning"),
                QStringLiteral("Warning through MainWindow::showToast: TxInterlockPolicy::warned with its "
                               "amplifier reason (TxInterlockPolicy.cpp) -> onTxInterlockWarning"),
                [policy] { emit policy->warned(QStringLiteral("Amplifier present but not in OPERATE")); });
        });
    });
    add(QStringLiteral("toast-error"), [](SceneContext& ctx) {
        runPopupScene(ctx, [](SceneContext& c) {
            TxInterlockPolicy* policy = c.model->txInterlockPolicy();
            if (policy == nullptr) {
                c.failure = QStringLiteral("no TX interlock policy");
                return;
            }
            shootToast<StatusToast>(c, QStringLiteral("toast-error"),
                QStringLiteral("Error through MainWindow::showToast: TxInterlockPolicy::denied with its "
                               "amplifier reason (TxInterlockPolicy.cpp) -> onTxInterlockDenial"),
                [policy] { emit policy->denied(QStringLiteral("Amplifier present but not in OPERATE")); });
        });
    });
    add(QStringLiteral("toast-antenna-switch"), [](SceneContext& ctx) {
        // T2: Tools > Test antenna switch toast when the build has it, else
        // the same RadioModel::emitAntennaAutoSwitched it calls.
        runPopupScene(ctx, [](SceneContext& c) {
            auto* action = c.window->findChild<QAction*>(QStringLiteral("toolsTestAntennaSwitchToast"));
            const bool viaAction = action != nullptr && action->isVisible() && action->isEnabled();
            shootToast<AntennaSwitchToast>(c, QStringLiteral("toast-antenna-switch"),
                viaAction ? QStringLiteral("Tools > Test antenna switch toast (its action triggered)")
                          : QStringLiteral("RadioModel::emitAntennaAutoSwitched(0, ANT1, ANT2), as the Tools test item calls it"),
                [model = c.model, action, viaAction] {
                    if (viaAction) {
                        action->trigger();
                    } else {
                        model->emitAntennaAutoSwitched(0, QStringLiteral("ANT1"), QStringLiteral("ANT2"));
                    }
                });
        });
    });

    // ---- tooltips ----
    add(QStringLiteral("tooltip-generic"), [](SceneContext& ctx) {
        // TT1: a widget's own tooltip through a help event (QWidget::event
        // shows it as a hover does), on the flag's RX antenna button.
        runPopupScene(ctx, [](SceneContext& c) {
            auto* btn = namedChild<QPushButton>(flagA(c), QStringLiteral("m_rxAntBtn"));
            if (btn == nullptr) {
                c.failure = QStringLiteral("no RX antenna button on the flag");
                return;
            }
            const QPoint local = middleOf(btn);
            const QPoint global = globalAt(btn, local);
            shootToolTip(c, QStringLiteral("tooltip-generic"), global, [btn, local, global] {
                QHelpEvent help(QEvent::ToolTip, local, global);
                QCoreApplication::sendEvent(btn, &help);
            }, btn->toolTip().left(12));
        });
    });
    add(QStringLiteral("tooltip-spectrum"), [](SceneContext& ctx) {
        // TT2 to TT4: the pointer over a spot label, the cluster badge and a
        // notch (SpectrumWidget::mouseMoveEvent).
        runPopupScene(ctx, [](SceneContext& c) {
            SpectrumWidget* s = panOne(c);
            placeSampleSpots(c, s);
            QPoint spot(-1, -1);
            for (const auto& hit : s->spotClickRectsForTest()) {
                if (spot.x() < 0 && hit.markerIndex == 0) {
                    spot = rectMiddle(hit.rect);
                }
            }
            if (spot.x() < 0 || s->spotClustersForTest().isEmpty()) {
                c.failure = QStringLiteral("the sample spots were not placed");
                return;
            }
            shootToolTip(c, QStringLiteral("tooltip-spectrum--spot"), globalAt(s, spot), [s, spot] {
                sendMouse(s, QEvent::MouseMove, spot, Qt::NoButton, Qt::NoButton);
            }, QStringLiteral("K1AAA"));
            // Any point of the badge the hover path answers.
            QPoint clusterAt(-1, -1);
            const QRect badge = s->spotClustersForTest().constFirst().rect;
            for (int y = badge.top(); y <= badge.bottom() && clusterAt.x() < 0; ++y) {
                for (int x = badge.left(); x <= badge.right() && clusterAt.x() < 0; x += 2) {
                    QToolTip::hideText();
                    sendMouse(s, QEvent::MouseMove, QPoint(x, y), Qt::NoButton, Qt::NoButton);
                    auto* tip = qobject_cast<QLabel*>(visibleToolTip());
                    if (tip != nullptr && tip->text().contains(QLatin1String("at this freq"))) {
                        clusterAt = QPoint(x, y);
                    }
                }
            }
            QToolTip::hideText();
            if (clusterAt.x() >= 0) {
                shootToolTip(c, QStringLiteral("tooltip-spectrum--cluster"), globalAt(s, clusterAt), [s, clusterAt] {
                    sendMouse(s, QEvent::MouseMove, clusterAt, Qt::NoButton, Qt::NoButton);
                }, QStringLiteral("at this freq"));
            } else {
                noteNotShown(c, QStringLiteral("tooltip: spot cluster badge (TT3)"),
                             QStringLiteral("no point of the cluster badge (%1,%2 %3x%4) answers the hover path: "
                                            "SpectrumWidget::mouseMoveEvent looks for spots only above the "
                                            "spectrum's height (my < layout.spectrum.height())")
                                 .arg(badge.x()).arg(badge.y()).arg(badge.width()).arg(badge.height()));
            }
            const int x = placeSampleNotch(c, s);
            const QPoint notch = findNotchPointer(s, x, s->height() / 5);
            if (notch.x() < 0) {
                c.failure = QStringLiteral("the notch was not found under the pointer");
                return;
            }
            shootToolTip(c, QStringLiteral("tooltip-spectrum--notch"), globalAt(s, notch), [s, notch] {
                sendMouse(s, QEvent::MouseMove, notch, Qt::NoButton, Qt::NoButton);
            }, QStringLiteral("Width:"));
        });
    });
    add(QStringLiteral("tooltip-connection"), [](SceneContext& ctx) {
        // TT5: a help event on the connection segment (MainWindow's event
        // filter shows RadioModel::buildConnectionTooltip).
        runPopupScene(ctx, [](SceneContext& c) {
            auto* segment = c.window->findChild<ConnectionSegment*>();
            if (segment == nullptr) {
                c.failure = QStringLiteral("no connection segment");
                return;
            }
            noteNotShown(c, QStringLiteral("tooltip: unavailable meter reading (TT6)"),
                         QStringLiteral("MeterWidget::event shows it on a meter item of a meter container whose "
                                        "reading another radio session owns; the two-band window has no meter "
                                        "container (its panels hold widgets, not meter items) in either look, "
                                        "and adding one would change the workspace"));
            const QPoint local(segment->width() / 3, segment->height() / 2);
            const QPoint global = globalAt(segment, local);
            shootToolTip(c, QStringLiteral("tooltip-connection"), global, [&c, segment, local, global] {
                QHelpEvent help(QEvent::ToolTip, local, global);
                QCoreApplication::sendEvent(segment, &help);
                // The uptime counts from the posed connection
                // (RadioModel::connectionUptimeText), so it changes with the
                // run's timing; it is pinned, as pinValueBeside pins Uptime
                // fields in dialogs.
                auto* tip = qobject_cast<QLabel*>(visibleToolTip());
                if (tip == nullptr) {
                    return;
                }
                static const QRegularExpression uptime(QStringLiteral("connected (\\d+h )?(\\d+m )?\\d+s"));
                QString text = tip->text();
                const QRegularExpressionMatch m = uptime.match(text);
                if (m.hasMatch()) {
                    c.notes.append(QStringLiteral("tooltip-connection: uptime \"%1\" pinned to \"%2\"")
                                       .arg(m.captured(0), QString::fromLatin1(kPinnedUptime)));
                    text.replace(m.capturedStart(0), m.capturedLength(0), QString::fromLatin1(kPinnedUptime));
                    QToolTip::showText(global, text, segment);
                }
            });
        });
    });
    add(QStringLiteral("tooltip-vfo"), [](SceneContext& ctx) {
        // TT7 is the tooltip a refused NR click shows at the button
        // (VfoWidget::onNrSelectionRefused, only with a click in flight).
        // Offline no click is refused: NR3 has a usable model (a click
        // turns it on) and DFNR, which cannot run here, is disabled. The
        // same plain reason shows as the disabled DFNR button's tooltip,
        // which is shown instead.
        runPopupScene(ctx, [](SceneContext& c) {
            VfoWidget* flag = flagA(c);
            QPushButton* dfnr = buttonWithText(flag, QStringLiteral("DFNR"));
            if (dfnr == nullptr) {
                c.failure = QStringLiteral("no DFNR button on the flag");
                return;
            }
            if (!openFlagTabFor(c, flag, dfnr)) {
                if (c.failure.isEmpty()) {
                    c.failure = QStringLiteral("the DFNR button is not shown");
                }
                return;
            }
            noteNotShown(c, QStringLiteral("tooltip: refused NR selection (TT7)"),
                         QStringLiteral("shown only when a click on NR3 or DFNR is refused; offline NR3 has a usable "
                                        "model, so a click would turn it on, and DFNR is disabled while it cannot "
                                        "run. The disabled DFNR button's tooltip, with the same reason, is shown"));
            const QPoint local = middleOf(dfnr);
            const QPoint global = globalAt(dfnr, local);
            shootToolTip(c, QStringLiteral("tooltip-vfo"), global, [dfnr, local, global] {
                QHelpEvent help(QEvent::ToolTip, local, global);
                QCoreApplication::sendEvent(dfnr, &help);
            }, dfnr->toolTip().left(12));
            c.notes.append(QStringLiteral("tooltip-vfo: the DFNR button (%1) hovered through a help event")
                               .arg(dfnr->isEnabled() ? QStringLiteral("enabled") : QStringLiteral("disabled")));
        });
    });
}

} // namespace

void pump(int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
}

// ---- folded receiver flags (workspace visual review Task 26) ----------------

VfoWidget* flagOnPan(SceneContext& ctx, int panIndex)
{
    return flagForSlice(ctx.window, sliceOnPan(ctx.window, panIdAt(panIndex)));
}

// Folds the flag of the slice on pan `panIndex` the way a click on its
// letter does (setCollapsed), with `pose` applied to it first.
void foldFlagOnPan(SceneContext& ctx, int panIndex,
                   const std::function<void(VfoWidget*)>& pose = {})
{
    VfoWidget* flag = flagOnPan(ctx, panIndex);
    if (flag == nullptr) {
        ctx.failure = QStringLiteral("no flag on %1").arg(panIdAt(panIndex));
        return;
    }
    if (pose) {
        pose(flag);
    }
    flag->setCollapsed(true);
    ctx.notes.append(QStringLiteral("%1: flag folded").arg(panIdAt(panIndex)));
}

// A crop around a folded flag: the flag, its frequency label and the
// marker under it.
constexpr int kFoldCropPadPx = 24;

void addFoldCrop(SceneContext& ctx, QVector<CropRect>& crops, double dpr, const QString& name,
                 VfoWidget* flag)
{
    if (flag == nullptr || flag->parentWidget() == nullptr) {
        return;
    }
    const QRect area = flag->occupiedRegion().boundingRect()
                           .adjusted(-kFoldCropPadPx, -kFoldCropPadPx / 6,
                                     kFoldCropPadPx, kFoldCropPadPx);
    const QRect logical(flag->parentWidget()->mapTo(ctx.window, area.topLeft()), area.size());
    const QRect clipped = logical.intersected(QRect(QPoint(0, 0), ctx.window->size()));
    if (!clipped.isEmpty()) {
        crops.append({name, clipped, deviceRect(clipped, dpr)});
    }
}

void addFoldStateCrops(SceneContext& ctx, QVector<CropRect>& crops, double dpr)
{
    addFoldCrop(ctx, crops, dpr, QStringLiteral("fold-right-of-marker"), flagOnPan(ctx, 0));
    addFoldCrop(ctx, crops, dpr, QStringLiteral("fold-left-of-marker"), flagOnPan(ctx, 1));
    addFoldCrop(ctx, crops, dpr, QStringLiteral("fold-tx-lit"), flagOnPan(ctx, 2));
    addFoldCrop(ctx, crops, dpr, QStringLiteral("fold-locked"), flagOnPan(ctx, 3));
}

void sceneFlagFoldTwoBand(SceneContext& ctx, const QString& name)
{
    runWindowScene(ctx, twoBandPoses(), 0, {}, [](SceneContext& c) {
        foldFlagOnPan(c, 0);
        pump(kSettleMs);
    });
    if (ctx.failure.isEmpty()) {
        captureWindow(ctx, name);
    }
}

// The open flag with one of its tabs showing (0 audio, 1 DSP, 2 mode,
// 3 X/RIT, 4 VAX), opened by a click on the tab as a user opens it.
void sceneFlagTab(SceneContext& ctx, const QString& name, int tab)
{
    runWindowScene(ctx, defaultPoses(), 0, {}, [tab](SceneContext& c) {
        VfoWidget* flag = flagA(c);
        if (flag == nullptr) {
            c.failure = QStringLiteral("no flag on %1").arg(panIdAt(0));
            return;
        }
        QList<QPushButton*> tabs;
        for (QPushButton* b : flag->findChildren<QPushButton*>(QString(), Qt::FindDirectChildrenOnly)) {
            if (b->isCheckable() && b->toolTip().startsWith(QLatin1String("Show/hide"))) {
                tabs.append(b);
            }
        }
        if (tab < 0 || tab >= tabs.size()) {
            c.failure = QStringLiteral("no flag tab %1").arg(tab);
            return;
        }
        tabs.at(tab)->click();
        pump(kFinalSettleMs);
        c.notes.append(QJsonObject{{QStringLiteral("flag_tab"), tab},
                                   {QStringLiteral("flag_size"),
                                    QStringLiteral("%1x%2").arg(flag->width()).arg(flag->height())}});
    });
    if (ctx.failure.isEmpty()) {
        captureWindow(ctx, name);
    }
}

// ---- a strip container scrolled over its meters -------------------------------

// JJ's left strip: the RX applet with GPU meters below it, in one stack.
// The RX applet is a singleton, so it leaves wherever it was placed.
QString addReceiveStrip(SceneContext& ctx)
{
    auto* manager = ctx.window->findChild<ContainerManager*>();
    if (manager == nullptr || manager->workspaceStore() == nullptr || manager->contentRegistry() == nullptr) {
        ctx.failure = QStringLiteral("no container workspace");
        return {};
    }
    WorkspaceDocument draft = manager->workspaceStore()->snapshot();
    for (auto& c : draft.containers) {
        c.contents.erase(std::remove_if(c.contents.begin(), c.contents.end(),
                                        [](const ContentEntry& e) { return e.typeId == QLatin1String("applet:rx"); }),
                         c.contents.end());
    }
    const ContainerDocument d = makeSceneContainer(manager, QStringLiteral("Receive"), DockEdge::Left,
        ContentLayout::VerticalStack,
        {{QStringLiteral("applet:rx"), {}}, {QStringLiteral("meter.mic"), {}}, {QStringLiteral("meter.alc"), {}},
         {QStringLiteral("meter.mic"), {}}, {QStringLiteral("meter.alc"), {}}, {QStringLiteral("meter.mic"), {}},
         {QStringLiteral("meter.alc"), {}}});
    draft.containers.append(d);
    if (manager->commitWorkspace(draft, draft.revision).status != CommitStatus::Saved) {
        ctx.failure = QStringLiteral("could not add the Receive container");
        return {};
    }
    return d.id;
}

QScrollArea* receiveStripScroll(SceneContext& ctx)
{
    auto* manager = ctx.window->findChild<ContainerManager*>();
    ContainerContentHost* host = manager ? manager->contentHost(QStringLiteral("preview-receive")) : nullptr;
    return host ? host->findChild<QScrollArea*>() : nullptr;
}

// Where Qt placed each meter's native window against the widget it
// belongs to and the scroll viewport that should clip it.
void noteMeterWindows(SceneContext& ctx, QScrollArea* scroll, const QString& when)
{
    QWidget* viewport = scroll->viewport();
    const QRect clip(viewport->mapTo(ctx.window, QPoint()), viewport->size());
    for (MeterWidget* m : scroll->findChildren<MeterWidget*>()) {
        const QRect widget(m->mapTo(ctx.window, QPoint()), m->size());
        QJsonObject note{{QStringLiteral("when"), when},
                         {QStringLiteral("meter"), m->objectName().isEmpty() ? QStringLiteral("MeterWidget") : m->objectName()},
                         {QStringLiteral("widget_in_window"), rectJson(widget)},
                         {QStringLiteral("viewport_in_window"), rectJson(clip)},
                         {QStringLiteral("visible_part"), rectJson(widget.intersected(clip))},
                         {QStringLiteral("native"), m->testAttribute(Qt::WA_NativeWindow)}};
        if (QWindow* w = m->windowHandle()) {
            note.insert(QStringLiteral("native_window_geometry"), rectJson(w->geometry()));
            note.insert(QStringLiteral("native_parent"), w->parent() ? w->parent()->objectName() : QStringLiteral("(none)"));
            note.insert(QStringLiteral("native_mask_empty"), w->mask().isEmpty());
            // The widget that owns the native parent window, and how far
            // up the meter it sits.
            int depth = 0;
            for (QWidget* a = m->parentWidget(); a != nullptr; a = a->parentWidget(), ++depth) {
                if (a->windowHandle() != nullptr && a->windowHandle() == w->parent()) {
                    note.insert(QStringLiteral("native_parent_widget"),
                                QStringLiteral("%1 '%2' %3 levels up%4%5")
                                    .arg(QString::fromLatin1(a->metaObject()->className()), a->objectName())
                                    .arg(depth + 1)
                                    .arg(a == viewport ? QStringLiteral(" (the scroll viewport)") : QString(),
                                         a == scroll->widget() ? QStringLiteral(" (the scrolled body)") : QString()));
                    break;
                }
            }
        }
        // Every ancestor up to the window: which own a native window, and
        // where that window sits against its parent.
        QJsonArray chain;
        for (QWidget* a = m->parentWidget(); a != nullptr; a = a->parentWidget()) {
            QString line = QStringLiteral("%1 '%2'").arg(QString::fromLatin1(a->metaObject()->className()), a->objectName());
            if (a == viewport) { line += QStringLiteral(" [viewport]"); }
            if (a == scroll->widget()) { line += QStringLiteral(" [body]"); }
            if (a->internalWinId() != 0) {
                line += QStringLiteral(" native");
                if (a->testAttribute(Qt::WA_NativeWindow)) { line += QStringLiteral(" (asked)"); }
                if (a->windowHandle() != nullptr) {
                    const QRect g = a->windowHandle()->geometry();
                    line += QStringLiteral(" at %1,%2 %3x%4").arg(g.x()).arg(g.y()).arg(g.width()).arg(g.height());
                }
            }
            chain.append(line);
            if (a->isWindow()) { break; }
        }
        note.insert(QStringLiteral("ancestors"), chain);
        ctx.notes.append(note);
    }
}

// ---- the connected window of the manual's overview (baseline-connected) -----

namespace {

// The firmware the status bar's hardware line shows for the posed radio
// (the connection panel scene's ANAN-G2, sampleRadio).
constexpr int kConnectedFirmware = 27;

// The TX applet straight under RX in the Main Panel, where the manual's
// overview shows it; by default it sits below Mod Monitor, off the first
// screen.
bool moveTxUnderRx(SceneContext& ctx)
{
    auto* manager = ctx.window->findChild<ContainerManager*>();
    if (manager == nullptr || manager->workspaceStore() == nullptr) {
        ctx.failure = QStringLiteral("no container workspace");
        return false;
    }
    WorkspaceDocument draft = manager->workspaceStore()->snapshot();
    bool moved = false;
    for (ContainerDocument& c : draft.containers) {
        if (c.id != draft.mainContainerId) {
            continue;
        }
        int rx = -1;
        int tx = -1;
        for (int i = 0; i < c.contents.size(); ++i) {
            if (c.contents.at(i).typeId == QLatin1String("applet:rx")) {
                rx = i;
            } else if (c.contents.at(i).typeId == QLatin1String("applet:TX")) {
                tx = i;
            }
        }
        if (rx < 0 || tx < 0) {
            ctx.failure = QStringLiteral("the Main Panel has no RX or no TX applet");
            return false;
        }
        const ContentEntry entry = c.contents.takeAt(tx);
        if (tx < rx) {
            --rx;
        }
        c.contents.insert(rx + 1, entry);
        moved = true;
    }
    if (!moved) {
        ctx.failure = QStringLiteral("no Main Panel container");
        return false;
    }
    if (manager->commitWorkspace(draft, draft.revision).status != CommitStatus::Saved) {
        ctx.failure = QStringLiteral("could not move the TX applet under RX");
        return false;
    }
    ctx.notes.append(QStringLiteral("Main Panel: applet:TX moved to just after applet:rx "
                                    "(ContainerManager::commitWorkspace)"));
    return true;
}

// The status bar's station block names the radio, as a local connect
// does (MainWindow's currentRadioChanged and hardware-line handlers), then
// the chrome bar lays out at its new natural width. The model's own
// currentRadioChanged is not emitted: its listeners load per-radio state.
void nameConnectedRadio(SceneContext& ctx)
{
    auto* station = ctx.window->findChild<StationBlock*>();
    if (station == nullptr) {
        ctx.failure = QStringLiteral("no StationBlock");
        return;
    }
    const RadioInfo radio = sampleRadio(QStringLiteral("ANAN-G2"), QStringLiteral("00:1C:C0:4E:21:0A"),
                                        QStringLiteral("192.168.1.20"), HPSDRHW::Saturn,
                                        ProtocolVersion::Protocol2, kConnectedFirmware);
    station->setRadioName(radio.name);
    station->setHardwareLine(QString::fromLatin1(boardCodeName(radio.boardType)),
                             QStringLiteral("v%1").arg(radio.firmwareVersion));
    auto* chrome = ctx.window->findChild<ChromeBarController*>();
    if (chrome != nullptr && station->parentWidget() != nullptr) {
        chrome->setNaturalWidth(station, station->sizeHint().width());
        chrome->relayout(station->parentWidget()->width());
    }
    ctx.notes.append(QStringLiteral("StationBlock::setRadioName(\"%1\"), setHardwareLine(\"%2\", \"v%3\"); "
                                    "ChromeBarController::setNaturalWidth and relayout%4")
                         .arg(radio.name, QString::fromLatin1(boardCodeName(radio.boardType)))
                         .arg(radio.firmwareVersion)
                         .arg(chrome != nullptr ? QString() : QStringLiteral(" (no ChromeBarController found)")));
}

// Slice A's flag says this window controls it, the words SliceChooser
// gives a flag whose row this window controls (MainWindow's slice access
// pass). Set after the freeze, so nothing refreshes it before the grab.
void showYouControl(SceneContext& ctx)
{
    SliceModel* sliceA = ctx.model->sliceById(0);
    VfoWidget* flag = flagForSlice(ctx.window, sliceA);
    if (flag == nullptr) {
        ctx.failure = QStringLiteral("no flag for slice A");
        return;
    }
    SliceChooser::Row row;
    row.sliceId = 0;
    row.controller = SliceChooser::Controller::ThisWindow;
    row.activeHere = true;
    flag->setSliceAccess(SliceChooser::flagAccessFor(row));
    ctx.notes.append(QStringLiteral("slice A flag: VfoWidget::setSliceAccess(SliceChooser::flagAccessFor(row "
                                    "controlled by this window)), \"%1\"")
                         .arg(flag->accessLineText()));
}

// The Main Panel's entries above the TX applet (the S-meter and RX, once
// moveTxUnderRx has run) hidden through the workspace (ContentEntry::visible),
// so the whole TX applet fits in the panel.
bool hideMainPanelAboveTx(SceneContext& ctx)
{
    auto* manager = ctx.window->findChild<ContainerManager*>();
    if (manager == nullptr || manager->workspaceStore() == nullptr) {
        ctx.failure = QStringLiteral("no container workspace");
        return false;
    }
    WorkspaceDocument draft = manager->workspaceStore()->snapshot();
    QStringList hidden;
    for (ContainerDocument& c : draft.containers) {
        if (c.id != draft.mainContainerId) {
            continue;
        }
        for (ContentEntry& entry : c.contents) {
            if (entry.typeId == QLatin1String("applet:TX")) {
                break;
            }
            if (entry.visible) {
                entry.visible = false;
                hidden.append(entry.typeId);
            }
        }
    }
    if (hidden.isEmpty()) {
        ctx.failure = QStringLiteral("nothing above the TX applet in the Main Panel");
        return false;
    }
    if (manager->commitWorkspace(draft, draft.revision).status != CommitStatus::Saved) {
        ctx.failure = QStringLiteral("could not hide the entries above the TX applet");
        return false;
    }
    ctx.notes.append(QStringLiteral("Main Panel: %1 hidden above applet:TX "
                                    "(ContentEntry::visible, ContainerManager::commitWorkspace)")
                         .arg(hidden.join(QStringLiteral(", "))));
    return true;
}

// baseline-connected's state before the freeze; with txOnly, the Main
// Panel's entries above the TX applet are hidden too.
void poseConnected(SceneContext& c, bool txOnly)
{
    if (!moveTxUnderRx(c)) {
        return;
    }
    if (txOnly && !hideMainPanelAboveTx(c)) {
        return;
    }
    nameConnectedRadio(c);
    // The TX applet's profile selector lists them, "Default" active.
    const QJsonObject seed = seedTxProfiles(c);
    if (!seed.isEmpty()) {
        c.notes.append(QJsonObject{{QStringLiteral("tx_profile_seed"), seed}});
    }
}

} // namespace

// ---- container item parity (PR 677, workspace preview) ---------------------
//
// The container-parity-* scenes show the items and editors the container
// item parity branch changed, built through the real paths: the containers
// are committed to the workspace as the settings dialog commits them, the
// items come from ContainerContentRegistry::makeEntry (a new item, with
// Thetis's defaults) or from a record a saved layout holds, and the editors
// are the ones Containers > Edit Container shows for the selected entry.
// Readings reach the items through the same seams the other scenes use
// (MeterWidget::updateMeterValue, MeterPoller::readingUpdated); nothing
// connects, keys or fetches from the network.

namespace {

// The strip widths these scenes give their containers.
constexpr int kParitySideStripPx = 420;
constexpr int kParityNarrowStripPx = 260;
constexpr int kParityTopStripPx = 230;
constexpr int kParityRoomyTopStripPx = 300;
// A Detailed VFO display face with room for its three columns.
constexpr int kParityRoomyFaceHeightPx = 240;
// The VFO display's long press (VfoDisplayItem::kLongPressMs) plus a margin.
constexpr int kParityLongPressMs = 1300;
// A VFO display screen closes once more than five whole seconds pass.
constexpr int kParityScreenCloseMaxMs = 9000;
// The signal level fed to readings (slice A's posed level).
constexpr double kParitySignalDbm = -73.0;
// Fed readings: how many, and the pause between them.
constexpr int kParityFeedCount = 12;
constexpr int kParityFeedGapMs = 40;
// The history wedge feed: levels swing this far either side of the level.
constexpr int kParityHistorySteps = 40;
constexpr double kParityHistorySwingDb = 12.0;
// A tooltip composited under a disabled row, this far from its left edge.
constexpr int kParityTipInsetPx = 8;
// The fixed local time every clock shows.
QDateTime parityClockTime()
{
    return QDateTime(QDate(2026, 10, 7), QTime(14, 32, 10));
}

// One item of a parity container.
struct ParityItem {
    QString typeId;
    // The entry name: the editor's list row and the entry header.
    QString name;
    // A saved record (the item's look before this branch); empty for a
    // new item, which makeEntry gives Thetis's defaults.
    QString legacyRecord;
    // Keys merged into the entry config; a "properties" object is merged
    // into the face's own properties, key by key.
    QJsonObject config;
    // true: "properties" replaces the face's properties (a saved face).
    bool replaceProperties{false};
};

struct ParityBox {
    QString name;
    DockEdge edge{DockEdge::Left};
    QVector<ParityItem> items;
    // The slice the container follows (1 = slice A).
    int rx{1};
};

// What the commit built: container ids and, per container, entry ids.
struct ParityBuilt {
    QStringList containers;
    QVector<QStringList> entries;
};

ContainerDocument parityContainer(ContainerManager* manager, const ParityBox& box)
{
    QVector<EdgeEntry> entries;
    for (const ParityItem& item : box.items) {
        entries.append({item.typeId, {}, item.legacyRecord, rxSource(box.rx), {}});
    }
    ContainerDocument d = onSlice(makeSceneContainer(manager, box.name, box.edge,
                                                     ContentLayout::VerticalStack, entries), box.rx);
    for (int i = 0; i < box.items.size() && i < d.contents.size(); ++i) {
        const ParityItem& item = box.items.at(i);
        ContentEntry& entry = d.contents[i];
        if (!item.name.isEmpty()) {
            entry.name = item.name;
        }
        for (auto it = item.config.constBegin(); it != item.config.constEnd(); ++it) {
            if (it.key() == QLatin1String("properties") && !item.replaceProperties) {
                QJsonObject merged = entry.config.value(it.key()).toObject();
                const QJsonObject change = it.value().toObject();
                for (auto c = change.constBegin(); c != change.constEnd(); ++c) {
                    merged.insert(c.key(), c.value());
                }
                entry.config.insert(it.key(), merged);
            } else {
                entry.config.insert(it.key(), it.value());
            }
        }
    }
    return d;
}

// The two-band window with `boxes` committed beside the Main Panel (right),
// every header shown; `afterFeed` runs once the spectra are fed.
bool runParityWindow(SceneContext& ctx, const QVector<ParityBox>& boxes, const QMap<DockEdge, int>& thickness,
                     ParityBuilt& built, const std::function<void(SceneContext&)>& afterFeed = {})
{
    ctx.provenance = provProposed();
    runWindowScene(ctx, twoBandPoses(), 0, [&boxes, &thickness, &built](SceneContext& c) {
        commitScene(c, HeaderMode::Shown, [&boxes, &thickness, &built](WorkspaceDocument& d, ContainerManager* m) {
            placeMain(d, DockEdge::Right);
            for (const ParityBox& box : boxes) {
                const ContainerDocument container = parityContainer(m, box);
                built.containers.append(container.id);
                QStringList ids;
                for (const ContentEntry& entry : container.contents) {
                    ids.append(entry.id);
                }
                built.entries.append(ids);
                d.containers.append(container);
            }
            for (auto it = thickness.constBegin(); it != thickness.constEnd(); ++it) {
                d.arrangement.strips[it.key()].thickness = it.value();
            }
        });
    }, [&afterFeed](SceneContext& c) {
        if (afterFeed) {
            afterFeed(c);
        }
        pump(kFinalSettleMs);
    });
    if (!ctx.failure.isEmpty()) {
        return false;
    }
    noteStrips(ctx);
    return true;
}

ContainerManager* parityManager(SceneContext& ctx)
{
    return ctx.window->findChild<ContainerManager*>();
}

// Every item of a type in one container, with the meter surface it is on.
template <typename T>
QVector<QPair<MeterWidget*, T*>> parityItems(SceneContext& ctx, const QString& containerId)
{
    QVector<QPair<MeterWidget*, T*>> out;
    ContainerContentHost* host = parityManager(ctx)->contentHost(containerId);
    if (host == nullptr) {
        return out;
    }
    for (MeterWidget* meter : host->meterSurfaces()) {
        for (MeterItem* item : meter->items()) {
            if (auto* t = qobject_cast<T*>(item)) {
                out.append({meter, t});
            }
        }
    }
    return out;
}

CapturedImage parityGrab(QWidget* widget, const QString& name, const QString& kind)
{
    CapturedImage img;
    img.name = name;
    img.kind = kind;
    img.image = grabWidget(widget);
    img.logicalSize = widget->size();
    img.dpr = img.image.devicePixelRatio();
    img.extra[QStringLiteral("widget_class")] = QString::fromLatin1(widget->metaObject()->className());
    img.extra[QStringLiteral("visible_text_fonts")] = visibleTextFonts(widget);
    img.geometry = widgetGeometry(widget);
    attachRegions(img, widget);
    return img;
}

// The whole window, without the standard crops: where the containers sit.
void captureParityWindow(SceneContext& ctx, const QString& name)
{
    for (const QJsonValue& v : stopTimersUnder(ctx.window)) {
        ctx.stoppedTimers.append(v);
    }
    CapturedImage img = parityGrab(ctx.window, name, QStringLiteral("window"));
    img.extra[QStringLiteral("applet_fit")] = measureAppletFit(ctx.window);
    ctx.images.append(img);
}

// One container as the window shows it (`name`), and, when its content
// scrolls, the whole content (`name`-content).
void captureParityContainer(SceneContext& ctx, const QString& name, const QString& containerId)
{
    ContainerManager* manager = parityManager(ctx);
    ContainerWidget* container = manager->container(containerId);
    ContainerContentHost* host = manager->contentHost(containerId);
    if (container == nullptr || host == nullptr || !container->isVisible()) {
        ctx.failure = QStringLiteral("%1: container %2 is not shown").arg(name, containerId);
        return;
    }
    pump(kFinalSettleMs);
    CapturedImage img = parityGrab(container, name, QStringLiteral("container"));
    const int minimumWidth = qMax(container->minimumWidth(), container->minimumSizeHint().width());
    img.extra[QStringLiteral("container_minimum_width")] = minimumWidth;
    QJsonArray rows;
    const ContainerDocument doc = host->captureDocument();
    for (const ContentEntry& entry : doc.contents) {
        const QRect r = host->entryBoundary(entry.id);
        rows.append(QJsonObject{{QStringLiteral("entry"), entry.id},
                                {QStringLiteral("type"), entry.typeId},
                                {QStringLiteral("name"), entry.name},
                                {QStringLiteral("rect_in_content"), rectJson(r)}});
    }
    img.extra[QStringLiteral("entries")] = rows;
    ctx.images.append(img);
    QString scroll;
    auto* area = container->contentHolder()->findChild<QScrollArea*>();
    const bool scrolls = area != nullptr && area->verticalScrollBar()->isVisible();
    if (scrolls) {
        ctx.images.append(parityGrab(host, name + QStringLiteral("-content"), QStringLiteral("container-content")));
        scroll = QStringLiteral(", content scrolls (%1 px in a %2 px view); %3-content shows all of it")
                     .arg(host->height()).arg(area->viewport()->height()).arg(name);
    }
    ctx.notes.append(QStringLiteral("%1: container \"%2\" %3x%4, minimum width %5%6")
                         .arg(name, container->notes().isEmpty() ? containerId : container->notes())
                         .arg(container->width()).arg(container->height()).arg(minimumWidth).arg(scroll));
}

// Containers > Edit Container on `containerId`, shown (not exec()'d) and
// sized to the window as a user would, over the window's middle.
ContainerSettingsDialog* openParityDialog(SceneContext& ctx, const QString& containerId)
{
    ContainerManager* manager = parityManager(ctx);
    ContainerWidget* container = manager->container(containerId);
    if (container == nullptr) {
        ctx.failure = QStringLiteral("no container %1 for the settings dialog").arg(containerId);
        return nullptr;
    }
    auto* dialog = new ContainerSettingsDialog(container, ctx.window, manager);
    dialog->selectDraftContainer(containerId);
    dialog->show();
    pump(kDialogSettleMs);
    const QSize fit = dialog->size().boundedTo(ctx.window->size() - QSize(kDialogInsetPx, kDialogInsetPx))
                          .expandedTo(dialog->minimumSizeHint());
    dialog->resize(fit);
    pump(kFinalSettleMs);
    dialog->move(ctx.window->mapToGlobal(ctx.window->rect().center()) - dialog->rect().center());
    pump(kFinalSettleMs);
    return dialog;
}

void closeParityDialog(ContainerSettingsDialog* dialog)
{
    if (dialog != nullptr) {
        dialog->reject();
        dialog->deleteLater();
    }
}

// Selects the entry in the dialog's contents list and returns the editor
// it shows.
ContentPropertyEditor* selectParityEntry(SceneContext& ctx, ContainerSettingsDialog* dialog, const QString& entryId)
{
    auto* contents = dialog->findChild<QListWidget*>(QStringLiteral("containerDraftContents"));
    QListWidgetItem* row = nullptr;
    for (int i = 0; contents != nullptr && i < contents->count(); ++i) {
        if (contents->item(i)->data(Qt::UserRole).toString() == entryId) {
            row = contents->item(i);
        }
    }
    if (row == nullptr) {
        ctx.failure = QStringLiteral("no row for entry %1 in the settings dialog").arg(entryId);
        return nullptr;
    }
    contents->setCurrentItem(row);
    pump(kFinalSettleMs);
    for (ContentPropertyEditor* editor : dialog->findChildren<ContentPropertyEditor*>()) {
        if (editor->isVisible()) {
            return editor;
        }
    }
    ctx.failure = QStringLiteral("no property editor shown for entry %1").arg(entryId);
    return nullptr;
}

// The scroll area the editor sits in, scrolled so `w` is in view.
void scrollParityEditorTo(QWidget* editor, QWidget* w)
{
    for (QWidget* p = editor->parentWidget(); p != nullptr; p = p->parentWidget()) {
        if (auto* area = qobject_cast<QScrollArea*>(p)) {
            area->ensureWidgetVisible(w, 0, 40);
            pump(kFinalSettleMs);
            return;
        }
    }
}

// The editor at its full height (not clipped to the dialog's scroll area).
CapturedImage parityEditorImage(SceneContext& ctx, ContentPropertyEditor* editor, const QString& name,
                                const QString& entryName)
{
    for (const QJsonValue& v : stopTimersUnder(editor)) {
        ctx.stoppedTimers.append(v);
    }
    CapturedImage img = parityGrab(editor, name, QStringLiteral("editor"));
    QStringList labels;
    for (QLabel* l : editor->findChildren<QLabel*>()) {
        if (l->isVisibleTo(editor) && !l->text().isEmpty()) {
            labels.append(l->text() + (l->isEnabled() ? QString() : QStringLiteral(" (disabled)")));
        }
    }
    img.extra[QStringLiteral("entry")] = entryName;
    img.extra[QStringLiteral("labels")] = QJsonArray::fromStringList(labels);
    ctx.notes.append(QStringLiteral("%1: the property editor of \"%2\" at its full height, %3x%4")
                         .arg(name, entryName).arg(editor->width()).arg(editor->height()));
    return img;
}

void captureParityEditor(SceneContext& ctx, ContentPropertyEditor* editor, const QString& name,
                         const QString& entryName)
{
    ctx.images.append(parityEditorImage(ctx, editor, name, entryName));
}

// A tooltip as the pointer over `anchor` raises it, placed under the row
// at the editor's left edge so the editor image can carry it.
QWidget* showParityTip(QWidget* editor, QWidget* anchor, const QString& text)
{
    QToolTip::hideText();
    pump(kFinalSettleMs);
    const QPoint at(globalOf(editor).x() + kParityTipInsetPx, globalOf(anchor).y() + anchor->height() + 2);
    QToolTip::showText(at, text, anchor);
    pump(kFinalSettleMs);
    QWidget* tip = visibleToolTip();
    if (tip != nullptr) {
        tip->move(at);
        pump(kFinalSettleMs);
    }
    return tip;
}

// The editor with each disabled control's tooltip painted under it, one
// image per control.
void captureParityEditorTips(SceneContext& ctx, ContentPropertyEditor* editor, const QString& name,
                             const QString& entryName, const QStringList& objectNames)
{
    for (const QString& objectName : objectNames) {
        auto* anchor = editor->findChild<QWidget*>(objectName);
        if (anchor == nullptr) {
            ctx.failure = QStringLiteral("%1: no control %2 in the editor").arg(name, objectName);
            return;
        }
        const QString text = anchor->toolTip();
        QWidget* tip = showParityTip(editor, anchor, text);
        CapturedImage img = parityEditorImage(ctx, editor, name + QStringLiteral("-") + slug(objectName),
                                              entryName);
        if (tip == nullptr) {
            ctx.notes.append(QStringLiteral("%1: the tooltip of %2 did not show; its text is \"%3\"")
                                 .arg(img.name, objectName, text));
        } else {
            paintOver(img, editor, tip);
        }
        img.extra[QStringLiteral("tooltip_of")] = objectName;
        img.extra[QStringLiteral("tooltip_text")] = text;
        img.extra[QStringLiteral("control_enabled")] = anchor->isEnabled();
        ctx.notes.append(QStringLiteral("%1: %2 is %3, tooltip \"%4\" painted under its row")
                             .arg(img.name, objectName,
                                  anchor->isEnabled() ? QStringLiteral("enabled") : QStringLiteral("disabled"), text));
        ctx.images.append(img);
        QToolTip::hideText();
        pump(kFinalSettleMs);
    }
}

// Pushes `value` to `bindings` on every meter surface of a container, as
// the poller's updates would arrive.
void feedParityReadings(SceneContext& ctx, const QString& containerId, const QList<int>& bindings, double value)
{
    ContainerContentHost* host = parityManager(ctx)->contentHost(containerId);
    if (host == nullptr) {
        return;
    }
    for (int i = 0; i < kParityFeedCount; ++i) {
        for (MeterWidget* meter : host->meterSurfaces()) {
            for (int binding : bindings) {
                meter->updateMeterValue(binding, value);
            }
        }
        pump(kParityFeedGapMs);
    }
    QJsonArray ids;
    for (int binding : bindings) {
        ids.append(binding);
    }
    ctx.fedReadings.append(QJsonObject{{QStringLiteral("container"), containerId},
                                       {QStringLiteral("bindings"), ids},
                                       {QStringLiteral("value"), value},
                                       {QStringLiteral("count"), kParityFeedCount},
                                       {QStringLiteral("seam"), QStringLiteral("MeterWidget::updateMeterValue")}});
}

// Every clock in a container (building blocks and clock faces) reads the
// fixed time.
void fixParityClocks(SceneContext& ctx, const QString& containerId)
{
    QList<ClockItem*> clocks;
    for (const auto& [meter, clock] : parityItems<ClockItem>(ctx, containerId)) {
        clocks.append(clock);
    }
    for (const auto& [meter, face] : parityItems<CompositePresetItem>(ctx, containerId)) {
        if (face->clockDisplay() != nullptr) {
            clocks.append(face->clockDisplay());
        }
    }
    for (ClockItem* clock : clocks) {
        clock->setTimeSource([] { return parityClockTime(); });
    }
    ContainerContentHost* host = parityManager(ctx)->contentHost(containerId);
    for (MeterWidget* meter : host != nullptr ? host->meterSurfaces() : QVector<MeterWidget*>{}) {
        for (MeterItem* item : meter->items()) {
            meter->invalidatePresentation(item);
        }
        meter->update();
    }
    pump(kFinalSettleMs);
    ctx.frozen.append(QJsonObject{{QStringLiteral("item"), QStringLiteral("clocks in %1").arg(containerId)},
                                  {QStringLiteral("value"), QStringLiteral("local time 2026-10-07 14:32:10 "
                                                                           "(ClockItem::setTimeSource)")},
                                  {QStringLiteral("count"), int(clocks.size())}});
}

// A record of `Item` as constructed, before any new-item default: the
// look a layout saved before this branch loads with.
template <typename Item>
QString savedRecord()
{
    Item item;
    return item.serialize();
}

// ---- 1 and 11: button grid editors -----------------------------------------

void sceneParityButtonGrids(SceneContext& ctx)
{
    const QString name = QStringLiteral("container-parity-button-grids");
    ParityBuilt built;
    const QVector<ParityBox> boxes = {
        {QStringLiteral("Mode buttons"), DockEdge::Left, {{QStringLiteral("MODEBTNS")}}},
        {QStringLiteral("Tune step buttons"), DockEdge::Left, {{QStringLiteral("TUNESTEPBTNS")}}},
        {QStringLiteral("Antenna buttons"), DockEdge::Left, {{QStringLiteral("ANTENNABTNS")}}},
        {QStringLiteral("Filter buttons"), DockEdge::Left, {{QStringLiteral("FILTERBTNS")}}},
        {QStringLiteral("Other buttons"), DockEdge::Left, {{QStringLiteral("OTHERBTNS")}}},
    };
    if (!runParityWindow(ctx, boxes, {{DockEdge::Left, kParitySideStripPx}}, built)) {
        return;
    }
    captureParityWindow(ctx, name);
    const QStringList keys = {QStringLiteral("modes"), QStringLiteral("steps"), QStringLiteral("antennas"),
                              QStringLiteral("filters"), QStringLiteral("other")};
    for (int i = 0; i < keys.size(); ++i) {
        captureParityContainer(ctx, name + QStringLiteral("--") + keys.at(i), built.containers.at(i));
    }
    for (int i = 0; i < keys.size() && ctx.failure.isEmpty(); ++i) {
        ContainerSettingsDialog* dialog = openParityDialog(ctx, built.containers.at(i));
        ContentPropertyEditor* editor = dialog != nullptr
            ? selectParityEntry(ctx, dialog, built.entries.at(i).value(0)) : nullptr;
        if (editor != nullptr) {
            if (i == 0) {
                captureDialog(ctx, dialog, name + QStringLiteral("--dialog"));
            }
            const QString image = name + QStringLiteral("--editor-") + keys.at(i);
            captureParityEditor(ctx, editor, image, boxes.at(i).name);
            if (keys.at(i) == QLatin1String("antennas")) {
                // XVTR input and Rx/Tx swap (AntennaButtonItem::kXvtrButton,
                // kRxTxButton) wait for features that are not built.
                captureParityEditorTips(ctx, editor, image, boxes.at(i).name,
                                        {QStringLiteral("antennaVisible_%1").arg(AntennaButtonItem::kXvtrButton),
                                         QStringLiteral("antennaVisible_%1").arg(AntennaButtonItem::kRxTxButton)});
            } else if (keys.at(i) == QLatin1String("filters")) {
                captureParityEditorTips(ctx, editor, image, boxes.at(i).name,
                                        {QStringLiteral("filterVisible_%1").arg(FilterButtonItem::kVar1Button),
                                         QStringLiteral("filterVisible_%1").arg(FilterButtonItem::kVar2Button)});
            }
        }
        closeParityDialog(dialog);
        pump(kFinalSettleMs);
    }
    annotateSlices(ctx);
}

// ---- 2: mode grid ------------------------------------------------------------

// A mode grid saved before the twelve-mode order: the old ten buttons, all
// shown, no order marker (ModeButtonItem::deserialize remaps it and starts
// SPEC and DRM hidden).
QString savedModeGridRecord()
{
    ModeButtonItem item;
    QStringList parts = item.serialize().split(QLatin1Char('|')).mid(0, 9);
    parts.append(QString::number((1u << 10) - 1u));
    return parts.join(QLatin1Char('|'));
}

void sceneParityModeGrid(SceneContext& ctx)
{
    const QString name = QStringLiteral("container-parity-mode-grid");
    ParityBuilt built;
    const QVector<ParityBox> boxes = {
        {QStringLiteral("Modes"), DockEdge::Left,
         {{QStringLiteral("MODEBTNS"), QStringLiteral("Mode buttons (new)")},
          {QStringLiteral("MODEBTNS"), QStringLiteral("Mode buttons (saved layout)"), savedModeGridRecord()},
          {QStringLiteral("meter.contest"), QStringLiteral("Contest controls (new)")}}},
    };
    if (!runParityWindow(ctx, boxes, {{DockEdge::Left, kParitySideStripPx}}, built)) {
        return;
    }
    const QString id = built.containers.value(0);
    fixParityClocks(ctx, id);
    captureParityWindow(ctx, name);
    captureParityContainer(ctx, name + QStringLiteral("--grids"), id);
    ContainerSettingsDialog* dialog = openParityDialog(ctx, id);
    const QString savedEntry = built.entries.value(0).value(1);
    ContentPropertyEditor* editor = dialog != nullptr ? selectParityEntry(ctx, dialog, savedEntry) : nullptr;
    if (editor == nullptr) {
        closeParityDialog(dialog);
        return;
    }
    captureParityEditor(ctx, editor, name + QStringLiteral("--editor-saved"), QStringLiteral("Mode buttons (saved layout)"));
    // Tick SPEC and DRM in Visible modes, then Apply, as a user would.
    QStringList ticked;
    for (int i = 0; i < ModeButtonItem::kModeCount; ++i) {
        const QString label = ModeButtonItem::modeLabel(i);
        if (label != QLatin1String("SPEC") && label != QLatin1String("DRM")) {
            continue;
        }
        auto* check = editor->findChild<QCheckBox*>(QStringLiteral("modeVisible_%1").arg(i));
        if (check == nullptr) {
            ctx.failure = QStringLiteral("no Visible modes row for %1").arg(label);
            closeParityDialog(dialog);
            return;
        }
        ctx.notes.append(QStringLiteral("saved grid before the tick: %1 %2").arg(label,
            check->isChecked() ? QStringLiteral("ticked") : QStringLiteral("not ticked")));
        check->setChecked(true);
        ticked.append(label);
        pump(kFinalSettleMs);
    }
    const CommitResult applied = dialog->applyDraft();
    pump(kSettleMs);
    ctx.notes.append(QStringLiteral("ticked %1 on the saved grid and pressed Apply: %2")
                         .arg(ticked.join(QStringLiteral(" and ")),
                              applied.status == CommitStatus::Saved ? QStringLiteral("saved")
                                                                    : QStringLiteral("not saved")));
    if (applied.status != CommitStatus::Saved) {
        ctx.failure = QStringLiteral("Apply did not save the ticked modes");
        closeParityDialog(dialog);
        return;
    }
    editor = selectParityEntry(ctx, dialog, savedEntry);
    if (editor != nullptr) {
        captureParityEditor(ctx, editor, name + QStringLiteral("--editor-saved-spec-drm"),
                            QStringLiteral("Mode buttons (saved layout)"));
    }
    closeParityDialog(dialog);
    pump(kSettleMs);
    fixParityClocks(ctx, id);
    captureParityContainer(ctx, name + QStringLiteral("--grids-spec-drm"), id);
    ctx.notes.append(QStringLiteral("the Contest face's grid has no Visible modes row (its properties leave "
                                    "out \"modes\"); it shows its ten modes"));
    annotateSlices(ctx);
}

// ---- 3: VFO display ------------------------------------------------------------

// Sends a press and release (or a held press) to `meter` at `pos`.
void parityClick(MeterWidget* meter, const QPointF& pos, int holdMs = 0)
{
    QMouseEvent press(QEvent::MouseButtonPress, pos, meter->mapToGlobal(pos), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(meter, &press);
    pump(holdMs > 0 ? holdMs : 20);
    QMouseEvent release(QEvent::MouseButtonRelease, pos, meter->mapToGlobal(pos), Qt::LeftButton, Qt::NoButton,
                        Qt::NoModifier);
    QCoreApplication::sendEvent(meter, &release);
    pump(kFinalSettleMs);
}

// A point of the Detailed layout of a one-column VFO display face, from
// the Thetis fractions renderVfoDisplay's boxes use (x of the width times
// kVfoSingleXMulti 2.07, y of the height).
QPointF parityVfoPoint(MeterWidget* meter, MeterItem* face, double fx, double fy)
{
    const QRectF r(face->x() * meter->width(), face->y() * meter->height(),
                   face->itemWidth() * meter->width(), face->itemHeight() * meter->height());
    return QPointF(r.left() + r.width() * 2.07 * fx, r.top() + r.height() * fy);
}

void waitParityScreenClosed(VfoDisplayItem* vfo, int slice)
{
    QElapsedTimer t;
    t.start();
    while (vfo->screenFor(slice) != VfoDisplayItem::Screen::Vfo && t.elapsed() < kParityScreenCloseMaxMs) {
        pump(200);
    }
}

void sceneParityVfoDisplay(SceneContext& ctx, bool roomy)
{
    const QString name = roomy ? QStringLiteral("container-parity-vfo-display-roomy")
                               : QStringLiteral("container-parity-vfo-display");
    QJsonObject all{{QStringLiteral("properties"),
                     QJsonObject{{QStringLiteral("displayMode"), int(VfoDisplayItem::DisplayMode::AllSlices)}}}};
    QJsonObject source;
    if (roomy) {
        QJsonObject p = all.value(QStringLiteral("properties")).toObject();
        p.insert(QStringLiteral("faceHeight"), kParityRoomyFaceHeightPx);
        all.insert(QStringLiteral("properties"), p);
        source.insert(QStringLiteral("properties"), QJsonObject{{QStringLiteral("faceHeight"), kParityRoomyFaceHeightPx}});
    }
    ParityBuilt built;
    const QVector<ParityBox> boxes = {
        {QStringLiteral("VFO display, slice A"), DockEdge::Top,
         {{QStringLiteral("meter.vfoDisplay"), QStringLiteral("VFO display (new)"), {}, source}}},
        {QStringLiteral("VFO display, all slices"), DockEdge::Top,
         {{QStringLiteral("meter.vfoDisplay"), QStringLiteral("VFO display, all slices"), {}, all}}},
    };
    if (!runParityWindow(ctx, boxes, {{DockEdge::Top, roomy ? kParityRoomyTopStripPx : kParityTopStripPx}}, built)) {
        return;
    }
    captureParityWindow(ctx, name);
    captureParityContainer(ctx, name + QStringLiteral("--slice-a"), built.containers.value(0));
    captureParityContainer(ctx, name + QStringLiteral("--all-slices"), built.containers.value(1));
    for (const auto& [meter, face] : parityItems<CompositePresetItem>(ctx, built.containers.value(0))) {
        ctx.notes.append(QStringLiteral("slice A face: %1x%2 px on its meter, layout %3, face height %4")
                             .arg(int(face->itemWidth() * meter->width())).arg(int(face->itemHeight() * meter->height()))
                             .arg(face->vfoDisplay() != nullptr && face->vfoDisplay()->layout() == VfoDisplayItem::Layout::Thetis
                                      ? QStringLiteral("Detailed") : QStringLiteral("Compact"))
                             .arg(face->configuration().value(QStringLiteral("faceHeight")).toInt()));
    }
    if (roomy) {
        annotateSlices(ctx);
        return;
    }
    // The editor of the new face.
    if (ContainerSettingsDialog* dialog = openParityDialog(ctx, built.containers.value(0))) {
        if (ContentPropertyEditor* editor = selectParityEntry(ctx, dialog, built.entries.value(0).value(0))) {
            captureParityEditor(ctx, editor, name + QStringLiteral("--editor"), QStringLiteral("VFO display (new)"));
        }
        closeParityDialog(dialog);
        pump(kFinalSettleMs);
    }
    if (!ctx.failure.isEmpty()) {
        return;
    }
    // Each screen of the slice A face, opened as a click (a long press for
    // the tune step) opens it; each closes on its own after five seconds.
    const auto faces = parityItems<CompositePresetItem>(ctx, built.containers.value(0));
    if (faces.isEmpty() || faces.first().second->vfoDisplay() == nullptr) {
        ctx.failure = QStringLiteral("no VFO display face in the slice A container");
        return;
    }
    MeterWidget* meter = faces.first().first;
    CompositePresetItem* face = faces.first().second;
    VfoDisplayItem* vfo = face->vfoDisplay();
    struct ScreenShot {
        QString key;
        VfoDisplayItem::Screen screen;
        double fx;
        double fy;
        int holdMs;
    };
    // From Thetis MeterManager.cs renderVfoDisplay boxes (kVfoHitBoxes):
    // mode 0.008-0.078, filter 0.250-0.332, band 0.333-0.480 across, all
    // 0.555-0.900 down; the kHz digits 0.346-0.416 at 0.30.
    const QVector<ScreenShot> shots = {
        {QStringLiteral("band"), VfoDisplayItem::Screen::Band, 0.40, 0.70, 0},
        {QStringLiteral("mode"), VfoDisplayItem::Screen::Mode, 0.04, 0.70, 0},
        {QStringLiteral("filter"), VfoDisplayItem::Screen::Filter, 0.29, 0.70, 0},
        {QStringLiteral("tune-step"), VfoDisplayItem::Screen::TuneStep, 0.346 + (0.416 - 0.346) / 3.0 * 2.5, 0.30,
         kParityLongPressMs},
    };
    const int sliceA = 0;
    for (const ScreenShot& shot : shots) {
        waitParityScreenClosed(vfo, sliceA);
        parityClick(meter, parityVfoPoint(meter, face, shot.fx, shot.fy), shot.holdMs);
        meter->update();
        pump(kFinalSettleMs);
        if (vfo->screenFor(sliceA) != shot.screen) {
            ctx.notes.append(QStringLiteral("the %1 screen did not open from a %2 at %3,%4 of the face")
                                 .arg(shot.key, shot.holdMs > 0 ? QStringLiteral("long press") : QStringLiteral("click"))
                                 .arg(shot.fx).arg(shot.fy));
            ctx.failure = QStringLiteral("the VFO display's %1 screen did not open").arg(shot.key);
            return;
        }
        captureParityContainer(ctx, name + QStringLiteral("--screen-") + shot.key, built.containers.value(0));
    }
    annotateSlices(ctx);
}

// ---- 4: clock ----------------------------------------------------------------

QString centeredClockRecord()
{
    ClockItem item(nullptr, false);
    item.applyNewItemDefaults();
    item.setLayout(ClockItem::Layout::NereusSDR);
    return item.serialize();
}

void sceneParityClock(SceneContext& ctx)
{
    const QString name = QStringLiteral("container-parity-clock");
    ParityBuilt built;
    const QVector<ParityBox> boxes = {
        {QStringLiteral("Clocks"), DockEdge::Left,
         {{QStringLiteral("CLOCK"), QStringLiteral("Clock (new, Large time)")},
          {QStringLiteral("CLOCK"), QStringLiteral("Clock (new, Centered)"), centeredClockRecord()},
          {QStringLiteral("CLOCK"), QStringLiteral("Clock (saved layout)"), savedRecord<ClockItem>()},
          {QStringLiteral("meter.clock"), QStringLiteral("Clock face (new)")}}},
    };
    if (!runParityWindow(ctx, boxes, {{DockEdge::Left, kParitySideStripPx}}, built)) {
        return;
    }
    const QString id = built.containers.value(0);
    fixParityClocks(ctx, id);
    captureParityWindow(ctx, name);
    captureParityContainer(ctx, name + QStringLiteral("--clocks"), id);
    ContainerSettingsDialog* dialog = openParityDialog(ctx, id);
    if (dialog == nullptr) {
        return;
    }
    const QStringList names = {QStringLiteral("Clock (new, Large time)"), QStringLiteral("Clock (new, Centered)"),
                               QStringLiteral("Clock (saved layout)"), QStringLiteral("Clock face (new)")};
    const QStringList keys = {QStringLiteral("large-time"), QStringLiteral("centered"), QStringLiteral("saved"),
                              QStringLiteral("face")};
    for (int i = 0; i < keys.size() && ctx.failure.isEmpty(); ++i) {
        if (ContentPropertyEditor* editor = selectParityEntry(ctx, dialog, built.entries.value(0).value(i))) {
            captureParityEditor(ctx, editor, name + QStringLiteral("--editor-") + keys.at(i), names.at(i));
        }
    }
    closeParityDialog(dialog);
    annotateSlices(ctx);
}

// ---- 5 and 10: text overlay and LED -------------------------------------------

// The text a user typed into Text 1, so the overlay has something to show:
// a new text overlay starts with no text, as Thetis's does (clsTextOverlay,
// MeterManager.cs:20144 [v2.10.3.15]: _text_1 = ""), and draws only its
// back panel. The slice tokens read the posed slices.
constexpr char kParityOverlayNewText[] = "%sliceA_freq% %sliceA_mode%";
constexpr char kParityOverlaySavedText[] = "Slice A %sliceA_band% %sliceA_filter%";

// A text overlay record with Text 1 typed in: a new item (Thetis's
// defaults) or one as a saved layout holds it.
QString parityOverlayRecord(bool newItem, const QString& text)
{
    TextOverlayItem item;
    if (newItem) {
        item.applyNewItemDefaults();
    }
    item.setText1(text);
    return item.serialize();
}

void sceneParityOverlayLed(SceneContext& ctx)
{
    const QString name = QStringLiteral("container-parity-overlay-led");
    ParityBuilt built;
    const QVector<ParityBox> boxes = {
        {QStringLiteral("Overlay and LED"), DockEdge::Left,
         {{QStringLiteral("TEXTOVERLAY"), QStringLiteral("Text overlay (new)"),
           parityOverlayRecord(true, QString::fromLatin1(kParityOverlayNewText))},
          {QStringLiteral("TEXTOVERLAY"), QStringLiteral("Text overlay (saved layout)"),
           parityOverlayRecord(false, QString::fromLatin1(kParityOverlaySavedText))},
          {QStringLiteral("LED"), QStringLiteral("LED (new)")}}},
    };
    if (!runParityWindow(ctx, boxes, {{DockEdge::Left, kParitySideStripPx}}, built)) {
        return;
    }
    const QString id = built.containers.value(0);
    fixParityClocks(ctx, id);
    ctx.notes.append(QStringLiteral("text overlays: Text 1 typed in, new \"%1\", saved layout \"%2\"; "
                                    "the LED has no condition, so it shows its false look (Thetis "
                                    "_false_colour 192,192,192, a flat square)")
                         .arg(QString::fromLatin1(kParityOverlayNewText), QString::fromLatin1(kParityOverlaySavedText)));
    for (const auto& [meter, overlay] : parityItems<TextOverlayItem>(ctx, id)) {
        ctx.notes.append(QStringLiteral("text overlay \"%1\" shows \"%2\"").arg(overlay->text1(), overlay->resolvedText1()));
    }
    captureParityWindow(ctx, name);
    captureParityContainer(ctx, name + QStringLiteral("--items"), id);
    ContainerSettingsDialog* dialog = openParityDialog(ctx, id);
    if (dialog == nullptr) {
        return;
    }
    // The text overlay's editor, then its "%" token picker open.
    if (ContentPropertyEditor* editor = selectParityEntry(ctx, dialog, built.entries.value(0).value(0))) {
        captureParityEditor(ctx, editor, name + QStringLiteral("--editor-overlay"), QStringLiteral("Text overlay (new)"));
        auto* picker = editor->findChild<QToolButton*>(QStringLiteral("overlayTokenPicker1"));
        QMenu* menu = picker != nullptr ? picker->menu() : nullptr;
        if (menu == nullptr) {
            ctx.failure = QStringLiteral("no \"%\" token picker on the text overlay editor");
        } else {
            scrollParityEditorTo(editor, picker);
            // The menu InstantPopup opens, shown with popup() so the grab
            // needs no nested loop. popup() keeps it on the screen as the
            // tool button's own popup does; it is not moved after.
            menu->popup(globalOf(picker) + QPoint(0, picker->height()));
            pump(kFinalSettleMs);
            appendPopupImages(ctx, name + QStringLiteral("--token-picker"), dialog, {menu}, menu);
            const QRect screen = picker->screen() != nullptr ? picker->screen()->geometry() : QRect();
            const QRect placed = menu->geometry();
            ctx.notes.append(QStringLiteral("token picker: %1 tokens, menu %2x%3 at %4,%5; screen %6x%7, "
                                            "menu inside the screen: %8")
                                 .arg(menu->actions().size()).arg(placed.width()).arg(placed.height())
                                 .arg(placed.x()).arg(placed.y()).arg(screen.width()).arg(screen.height())
                                 .arg(screen.contains(placed) ? QStringLiteral("yes") : QStringLiteral("no")));
            menu->close();
            pump(kFinalSettleMs);
        }
    }
    // The LED's editor, then with a condition it cannot work out.
    if (ctx.failure.isEmpty()) {
        if (ContentPropertyEditor* editor = selectParityEntry(ctx, dialog, built.entries.value(0).value(2))) {
            captureParityEditor(ctx, editor, name + QStringLiteral("--editor-led"), QStringLiteral("LED (new)"));
            auto* condition = editor->findChild<QLineEdit*>(QStringLiteral("ledCondition"));
            if (condition == nullptr) {
                ctx.failure = QStringLiteral("no Condition row on the LED editor");
            } else {
                const QString bad = QStringLiteral("%swr% >= (2");
                condition->setText(bad);
                emit condition->textEdited(bad);
                pump(kFinalSettleMs);
                captureParityEditor(ctx, editor, name + QStringLiteral("--editor-led-bad-condition"),
                                    QStringLiteral("LED (new)"));
                ctx.notes.append(QStringLiteral("LED condition typed: \"%1\"").arg(bad));
            }
        }
    }
    closeParityDialog(dialog);
    annotateSlices(ctx);
}

// ---- 6: S-meter dial (and the tuning dial's saved look) ---------------------------

// Feeds every added S-meter dial a swinging level, so a dial that keeps
// history has a wedge to draw, then the slice's level.
void feedParityDials(SceneContext& ctx)
{
    auto* poller = ctx.window->findChild<MeterPoller*>();
    if (poller == nullptr) {
        ctx.failure = QStringLiteral("no MeterPoller");
        return;
    }
    const QList<SMeterWidget*> dials = dialCopies(ctx);
    for (int step = 0; step < kParityHistorySteps; ++step) {
        const double swing = kParityHistorySwingDb * std::sin(step * 0.6);
        for (SMeterWidget* dial : dials) {
            const QJsonObject context = dial->property("containerSourceContext").toJsonObject();
            emit poller->readingUpdated(context, MeterBinding::SignalPeak, kParitySignalDbm + swing);
        }
        pump(kParityFeedGapMs);
    }
    for (SMeterWidget* dial : dials) {
        const QJsonObject context = dial->property("containerSourceContext").toJsonObject();
        emit poller->readingUpdated(context, MeterBinding::SignalPeak, kParitySignalDbm);
    }
    ctx.fedReadings.append(QJsonObject{
        {QStringLiteral("instrument"), QStringLiteral("SMeterWidget (added copies)")},
        {QStringLiteral("binding"), QStringLiteral("RX: Signal Peak")},
        {QStringLiteral("values_dbm"), QStringLiteral("%1 readings swinging %2 dB about %3, then %3")
                                           .arg(kParityHistorySteps).arg(kParityHistorySwingDb).arg(kParitySignalDbm)},
        {QStringLiteral("seam"), consumerSeam(poller)}});
}

QString savedTuningDialRecord()
{
    DialItem item;
    item.applyNewItemDefaults();
    item.setThetisLook(false);
    return item.serialize();
}

void sceneParitySMeterDial(SceneContext& ctx)
{
    const QString name = QStringLiteral("container-parity-smeter-dial");
    const QString dial = QString::fromLatin1(SMeterDialContent::kDialType);
    const QString dark = QString::fromLatin1(SMeterDialContent::kDarkModeKey);
    const QString history = QString::fromLatin1(SMeterDialContent::kShowHistoryKey);
    ParityBuilt built;
    const QVector<ParityBox> boxes = {
        {QStringLiteral("Dial"), DockEdge::Top, {{dial, QStringLiteral("S-meter dial (new)")}}},
        {QStringLiteral("Dial, dark mode"), DockEdge::Top,
         {{dial, QStringLiteral("S-meter dial, dark mode"), {}, QJsonObject{{dark, true}}}}},
        {QStringLiteral("Dial, history"), DockEdge::Top,
         {{dial, QStringLiteral("S-meter dial, history"), {}, QJsonObject{{history, true}}}}},
        {QStringLiteral("Dial, dark and history"), DockEdge::Top,
         {{dial, QStringLiteral("S-meter dial, dark mode and history"), {}, QJsonObject{{dark, true}, {history, true}}}}},
        {QStringLiteral("Tuning dials"), DockEdge::Left,
         {{QStringLiteral("DIAL"), QStringLiteral("Tuning dial (new)")},
          {QStringLiteral("DIAL"), QStringLiteral("Tuning dial (saved look)"), savedTuningDialRecord()}}},
    };
    if (!runParityWindow(ctx, boxes, {{DockEdge::Top, kParityTopStripPx}, {DockEdge::Left, kParitySideStripPx}},
                         built, [](SceneContext& c) { feedParityDials(c); })) {
        return;
    }
    captureParityWindow(ctx, name);
    const QStringList keys = {QStringLiteral("default"), QStringLiteral("dark"), QStringLiteral("history"),
                              QStringLiteral("dark-history"), QStringLiteral("tuning-dials")};
    for (int i = 0; i < keys.size(); ++i) {
        captureParityContainer(ctx, name + QStringLiteral("--") + keys.at(i), built.containers.value(i));
    }
    noteDials(ctx);
    ctx.notes.append(QStringLiteral("an S-meter dial has no new-item default: a new copy and a saved copy "
                                    "both draw with dark mode and history off (absent keys)"));
    if (ContainerSettingsDialog* dialog = openParityDialog(ctx, built.containers.value(3))) {
        if (ContentPropertyEditor* editor = selectParityEntry(ctx, dialog, built.entries.value(3).value(0))) {
            captureParityEditor(ctx, editor, name + QStringLiteral("--editor"),
                                QStringLiteral("S-meter dial, dark mode and history"));
        }
        closeParityDialog(dialog);
    }
    annotateSlices(ctx);
}

// ---- 7: web image background ----------------------------------------------------

// A fixed picture for the web image item: bands of colour with a grid and
// a caption, so it is plain where it is drawn.
QImage parityWebPicture()
{
    QImage img(640, 360, QImage::Format_ARGB32);
    QPainter p(&img);
    QLinearGradient sky(0, 0, 0, img.height());
    sky.setColorAt(0.0, QColor(0x1c, 0x3f, 0x6e));
    sky.setColorAt(0.6, QColor(0x5a, 0x86, 0xb8));
    sky.setColorAt(1.0, QColor(0xd9, 0x9a, 0x4e));
    p.fillRect(img.rect(), sky);
    p.setPen(QPen(QColor(255, 255, 255, 60), 1));
    for (int x = 0; x < img.width(); x += 40) {
        p.drawLine(x, 0, x, img.height());
    }
    for (int y = 0; y < img.height(); y += 40) {
        p.drawLine(0, y, img.width(), y);
    }
    QFont f = p.font();
    f.setPixelSize(28);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(255, 255, 255, 200));
    p.drawText(img.rect(), Qt::AlignCenter, QStringLiteral("Web image (preview picture)"));
    return img;
}

void sceneParityWebImage(SceneContext& ctx)
{
    const QString name = QStringLiteral("container-parity-web-image");
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("web-image.png"));
    if (!dir.isValid() || !parityWebPicture().save(path, "PNG")) {
        ctx.failure = QStringLiteral("could not write the web image picture");
        return;
    }
    const QString url = QUrl::fromLocalFile(path).toString();
    ParityBuilt built;
    const QVector<ParityBox> boxes = {
        {QStringLiteral("Web image"), DockEdge::Left, {{QStringLiteral("WEBIMAGE"), QStringLiteral("Web image (new)")}}},
    };
    // The item reads a local file (no network): WebImageItem::setUrl loads a
    // file URL at once, and a live item on Pan 1 hands its picture to that
    // pan's spectrum (WebImageBackgrounds, MainWindow).
    if (!runParityWindow(ctx, boxes, {{DockEdge::Left, kParitySideStripPx}}, built, [&built, &url](SceneContext& c) {
            for (const auto& [meter, web] : parityItems<WebImageItem>(c, built.containers.value(0))) {
                web->setUrl(url);
                web->setTargetPan(0);
                meter->update();
                c.notes.append(QStringLiteral("web image: url a local PNG (no network), Pan 1; image %1, "
                                              "shows background %2")
                                   .arg(web->currentImage().isNull() ? QStringLiteral("none") : QStringLiteral("loaded"))
                                   .arg(web->showsBackground() ? QStringLiteral("yes") : QStringLiteral("no")));
            }
            pump(kSettleMs);
        })) {
        return;
    }
    SpectrumWidget* pan = spectrumFor(ctx.window, panIdAt(0));
    if (pan == nullptr) {
        ctx.failure = QStringLiteral("no pan 1 spectrum");
        return;
    }
    if (!pan->hasBackgroundImageOverride()) {
        ctx.failure = QStringLiteral("pan 1 has no web image background");
        return;
    }
    captureParityWindow(ctx, name);
    captureParityContainer(ctx, name + QStringLiteral("--item"), built.containers.value(0));
    QWidget* applet = appletFor(ctx.window, panIdAt(0));
    QWidget* panWidget = applet != nullptr ? applet : pan;
    ctx.notes.append(QStringLiteral("pan 1 background: own background %1, opacity %2%; with it off the "
                                    "web image is drawn at full strength (Thetis shows it undimmed), and the words over "
                                    "the trace are the preview picture's own caption")
                         .arg(pan->backgroundEnabled() ? QStringLiteral("on") : QStringLiteral("off"))
                         .arg(pan->backgroundOpacity()));
    ctx.images.append(parityGrab(panWidget, name + QStringLiteral("--pan"), QStringLiteral("pan")));
    // The pan's own background setting on: its opacity applies.
    const bool wasOn = pan->backgroundEnabled();
    pan->setBackgroundEnabled(true);
    pump(kSettleMs);
    ctx.notes.append(QStringLiteral("then the pan's own background on (opacity %1%)").arg(pan->backgroundOpacity()));
    ctx.images.append(parityGrab(panWidget, name + QStringLiteral("--pan-background-on"), QStringLiteral("pan")));
    pan->setBackgroundEnabled(wasOn);
    pump(kSettleMs);
    if (ContainerSettingsDialog* dialog = openParityDialog(ctx, built.containers.value(0))) {
        if (ContentPropertyEditor* editor = selectParityEntry(ctx, dialog, built.entries.value(0).value(0))) {
            captureParityEditor(ctx, editor, name + QStringLiteral("--editor"), QStringLiteral("Web image (new)"));
        }
        closeParityDialog(dialog);
    }
    annotateSlices(ctx);
}

// ---- 8: ANAN dark face and magic eye bezel -------------------------------------

void sceneParityArtworkOptions(SceneContext& ctx)
{
    const QString name = QStringLiteral("container-parity-artwork-options");
    ParityBuilt built;
    const QVector<ParityBox> boxes = {
        {QStringLiteral("Faces"), DockEdge::Left,
         {{QStringLiteral("meter.ananMulti"), QStringLiteral("ANAN multi meter (new)")},
          {QStringLiteral("meter.magicEye"), QStringLiteral("Magic eye face (new)")},
          {QStringLiteral("MAGICEYE"), QStringLiteral("Magic eye (new)")}}},
    };
    if (!runParityWindow(ctx, boxes, {{DockEdge::Left, kParitySideStripPx}}, built)) {
        return;
    }
    const QString id = built.containers.value(0);
    captureParityWindow(ctx, name);
    captureParityContainer(ctx, name + QStringLiteral("--faces"), id);
    ContainerSettingsDialog* dialog = openParityDialog(ctx, id);
    if (dialog == nullptr) {
        return;
    }
    const QStringList names = {QStringLiteral("ANAN multi meter (new)"), QStringLiteral("Magic eye face (new)"),
                               QStringLiteral("Magic eye (new)")};
    const QStringList keys = {QStringLiteral("anan"), QStringLiteral("eye-face"), QStringLiteral("eye")};
    const QStringList controls = {QStringLiteral("darkMode"), QStringLiteral("builtInBezel"),
                                  QStringLiteral("builtInBezel")};
    for (int i = 0; i < keys.size() && ctx.failure.isEmpty(); ++i) {
        ContentPropertyEditor* editor = selectParityEntry(ctx, dialog, built.entries.value(0).value(i));
        if (editor == nullptr) {
            break;
        }
        const QString image = name + QStringLiteral("--editor-") + keys.at(i);
        captureParityEditor(ctx, editor, image, names.at(i));
        captureParityEditorTips(ctx, editor, image, names.at(i), {controls.at(i)});
    }
    closeParityDialog(dialog);
    annotateSlices(ctx);
}

// ---- 9: signal text ----------------------------------------------------------

QString savedSignalFaceRecord(QJsonObject& properties)
{
    SignalTextPresetItem face;
    properties = face.configuration();
    return face.serialize();
}

void sceneParitySignalText(SceneContext& ctx)
{
    const QString name = QStringLiteral("container-parity-signal-text");
    QJsonObject savedFaceProperties;
    const QString savedFace = savedSignalFaceRecord(savedFaceProperties);
    ParityItem savedFaceItem{QStringLiteral("meter.signalText"), QStringLiteral("Signal text face (saved layout)"),
                             savedFace, QJsonObject{{QStringLiteral("properties"), savedFaceProperties}}, true};
    ParityBuilt built;
    const QVector<ParityBox> boxes = {
        {QStringLiteral("Signal text"), DockEdge::Left,
         {{QStringLiteral("SIGNALTEXT"), QStringLiteral("Signal text (new)")},
          {QStringLiteral("SIGNALTEXT"), QStringLiteral("Signal text (saved layout)"), savedRecord<SignalTextItem>()},
          {QStringLiteral("meter.signalText"), QStringLiteral("Signal text face (new)")},
          savedFaceItem}},
    };
    const QList<int> bindings = {MeterBinding::SignalPeak, MeterBinding::SignalAvg, MeterBinding::SignalMaxBin};
    if (!runParityWindow(ctx, boxes, {{DockEdge::Left, kParitySideStripPx}}, built,
                         [&built, &bindings](SceneContext& c) {
            feedParityReadings(c, built.containers.value(0), bindings, kParitySignalDbm);
        })) {
        return;
    }
    const QString id = built.containers.value(0);
    captureParityWindow(ctx, name);
    captureParityContainer(ctx, name + QStringLiteral("--items"), id);
    ContainerSettingsDialog* dialog = openParityDialog(ctx, id);
    if (dialog == nullptr) {
        return;
    }
    if (ContentPropertyEditor* editor = selectParityEntry(ctx, dialog, built.entries.value(0).value(0))) {
        captureParityEditor(ctx, editor, name + QStringLiteral("--editor"), QStringLiteral("Signal text (new)"));
        auto* units = editor->findChild<QComboBox*>(QStringLiteral("signalUnits"));
        if (units == nullptr) {
            ctx.failure = QStringLiteral("no Units row on the signal text editor");
        } else {
            QStringList items;
            for (int i = 0; i < units->count(); ++i) {
                items.append(units->itemText(i));
            }
            ctx.notes.append(QStringLiteral("Units (signalUnits): current \"%1\", items [%2]")
                                 .arg(units->currentText(), items.join(QStringLiteral(", "))));
            scrollParityEditorTo(editor, units);
            const QSet<QWidget*> before = visiblePopups();
            units->showPopup();
            pump(kFinalSettleMs);
            QWidget* popup = newPopup(before);
            appendPopupImages(ctx, name + QStringLiteral("--units"), dialog, {popup}, popup);
            if (popup == nullptr) {
                ctx.notes.append(QStringLiteral("the Units list did not open offscreen; see the editor image"));
            }
            units->hidePopup();
            pump(kFinalSettleMs);
        }
    }
    if (ctx.failure.isEmpty()) {
        if (ContentPropertyEditor* editor = selectParityEntry(ctx, dialog, built.entries.value(0).value(2))) {
            captureParityEditor(ctx, editor, name + QStringLiteral("--editor-face"),
                                QStringLiteral("Signal text face (new)"));
        }
    }
    closeParityDialog(dialog);
    annotateSlices(ctx);
}

// ---- 12: TX and RX section items -------------------------------------------------

// A section type with at least three parts, its built order and its
// hide keys, from the properties it declares.
struct ParitySection {
    QString typeId;
    QStringList order;
    QStringList labels;
};

ParitySection paritySection(ContainerContentRegistry* registry, const QStringList& candidates)
{
    for (const QString& typeId : candidates) {
        for (const ContentProperty& p : registry->declaredProperties(typeId)) {
            if (p.kind != ContentProperty::Kind::Order || p.choices.size() < 3) {
                continue;
            }
            ParitySection s;
            s.typeId = typeId;
            for (const ContentPropertyChoice& choice : p.choices) {
                s.order.append(choice.value.toObject().value(QStringLiteral("id")).toString());
                s.labels.append(choice.label);
            }
            return s;
        }
    }
    return {};
}

// The section with its second part hidden and its parts in reverse order
// (SectionParts::effectiveOrder keeps a shared line together).
QJsonObject paritySectionEdit(const ParitySection& s)
{
    QStringList reversed = s.order;
    std::reverse(reversed.begin(), reversed.end());
    return QJsonObject{{SectionParts::hideKey(s.order.value(1)), true},
                       {SectionParts::orderKey(), QJsonArray::fromStringList(reversed)}};
}

void sceneParitySections(SceneContext& ctx)
{
    const QString name = QStringLiteral("container-parity-sections");
    ContainerContentRegistry* registry = parityManager(ctx) != nullptr ? parityManager(ctx)->contentRegistry() : nullptr;
    if (registry == nullptr) {
        ctx.failure = QStringLiteral("no content registry: the workspace preview switch is off");
        return;
    }
    const ParitySection tx = paritySection(registry, {QStringLiteral("tx.power"), QStringLiteral("tx.processing"),
                                                      QStringLiteral("tx.chain"), QStringLiteral("tx.keying"),
                                                      QStringLiteral("tx.monitor"), QStringLiteral("tx.vox"),
                                                      QStringLiteral("tx.filter")});
    const ParitySection rx = paritySection(registry, {QStringLiteral("rx.agc"), QStringLiteral("rx.filter"),
                                                      QStringLiteral("rx.mode"), QStringLiteral("rx.antennas"),
                                                      QStringLiteral("rx.attenuator"), QStringLiteral("rx.squelch"),
                                                      QStringLiteral("rx.rit"), QStringLiteral("rx.xit"),
                                                      QStringLiteral("rx.step"), QStringLiteral("rx.pan")});
    if (tx.typeId.isEmpty() || rx.typeId.isEmpty()) {
        ctx.failure = QStringLiteral("no TX or RX section type with three or more parts");
        return;
    }
    ctx.notes.append(QStringLiteral("TX section %1, parts [%2]; second part hidden, order reversed")
                         .arg(tx.typeId, tx.labels.join(QStringLiteral(", "))));
    ctx.notes.append(QStringLiteral("RX section %1, parts [%2]; second part hidden, order reversed")
                         .arg(rx.typeId, rx.labels.join(QStringLiteral(", "))));
    ParityBuilt built;
    const QVector<ParityBox> boxes = {
        {QStringLiteral("TX section"), DockEdge::Left,
         {{tx.typeId, QStringLiteral("TX section (as built)")},
          {tx.typeId, QStringLiteral("TX section (hidden and reordered)"), {}, paritySectionEdit(tx)}}},
        {QStringLiteral("RX section"), DockEdge::Left,
         {{rx.typeId, QStringLiteral("RX section (as built)")},
          {rx.typeId, QStringLiteral("RX section (hidden and reordered)"), {}, paritySectionEdit(rx)}}},
    };
    if (!runParityWindow(ctx, boxes, {{DockEdge::Left, kParitySideStripPx}}, built)) {
        return;
    }
    captureParityWindow(ctx, name);
    captureParityContainer(ctx, name + QStringLiteral("--tx"), built.containers.value(0));
    captureParityContainer(ctx, name + QStringLiteral("--rx"), built.containers.value(1));
    const QStringList keys = {QStringLiteral("tx"), QStringLiteral("rx")};
    for (int i = 0; i < keys.size() && ctx.failure.isEmpty(); ++i) {
        if (ContainerSettingsDialog* dialog = openParityDialog(ctx, built.containers.value(i))) {
            if (ContentPropertyEditor* editor = selectParityEntry(ctx, dialog, built.entries.value(i).value(1))) {
                captureParityEditor(ctx, editor, name + QStringLiteral("--editor-") + keys.at(i),
                                    boxes.at(i).items.at(1).name);
            }
            closeParityDialog(dialog);
            pump(kFinalSettleMs);
        }
    }
    annotateSlices(ctx);
}

// ---- 13: container header with a long default name -----------------------------------

void sceneParityLongTitle(SceneContext& ctx)
{
    const QString name = QStringLiteral("container-parity-long-title");
    ParityBuilt built;
    const QVector<ParityBox> boxes = {
        {QStringLiteral("Signal max bin bar"), DockEdge::Left, {{QStringLiteral("meter.signalMaxBin")}}},
        // A one-entry container's header shows its entry's name
        // (ContainerManager::applyWorkspaceHeader), so the entry is named.
        {QStringLiteral("Signal"), DockEdge::Left, {{QStringLiteral("meter.signalMaxBin"), QStringLiteral("Signal")}}},
    };
    if (!runParityWindow(ctx, boxes, {{DockEdge::Left, kParityNarrowStripPx}}, built,
                         [&built](SceneContext& c) {
            for (const QString& id : built.containers) {
                feedParityReadings(c, id, {MeterBinding::SignalMaxBin}, kParitySignalDbm);
            }
        })) {
        return;
    }
    captureParityWindow(ctx, name);
    captureParityContainer(ctx, name + QStringLiteral("--long"), built.containers.value(0));
    captureParityContainer(ctx, name + QStringLiteral("--short"), built.containers.value(1));
    if (auto* area = ctx.window->findChild<EdgeDockArea*>()) {
        EdgeDockStrip* strip = area->strip(DockEdge::Left);
        ctx.notes.append(QStringLiteral("left strip asked for %1 px, is %2 px wide (minimum %3)")
                             .arg(kParityNarrowStripPx).arg(strip->width())
                             .arg(qMax(strip->minimumWidth(), strip->minimumSizeHint().width())));
    }
    ContainerManager* manager = parityManager(ctx);
    for (const QString& id : built.containers) {
        ContainerWidget* container = manager->container(id);
        WorkspaceGroupHeader* header = container != nullptr ? container->workspaceHeader() : nullptr;
        if (header == nullptr) {
            continue;
        }
        QLabel* title = nullptr;
        for (QLabel* l : header->findChildren<QLabel*>()) {
            if (l->isVisible() && !l->text().isEmpty()) {
                title = l;
            }
        }
        ctx.notes.append(QStringLiteral("header of \"%1\": %2 px wide, title label %3 px wide showing \"%4\" "
                                        "(full text needs %5 px)")
                             .arg(header->title()).arg(header->width())
                             .arg(title != nullptr ? title->width() : -1)
                             .arg(title != nullptr ? title->text() : QString())
                             .arg(title != nullptr ? title->fontMetrics().horizontalAdvance(header->title()) : -1));
    }
    annotateSlices(ctx);
}

// The container-parity-* scenes (workspace preview switch on).
void appendContainerParityScenes(QVector<Scene>& scenes)
{
    const auto add = [&scenes](const QString& name, const std::function<void(SceneContext&)>& run) {
        scenes.append({name, QStringLiteral("2v"), false, run, true});
    };
    add(QStringLiteral("container-parity-button-grids"), sceneParityButtonGrids);
    add(QStringLiteral("container-parity-mode-grid"), sceneParityModeGrid);
    add(QStringLiteral("container-parity-vfo-display"), [](SceneContext& ctx) { sceneParityVfoDisplay(ctx, false); });
    add(QStringLiteral("container-parity-vfo-display-roomy"),
        [](SceneContext& ctx) { sceneParityVfoDisplay(ctx, true); });
    add(QStringLiteral("container-parity-clock"), sceneParityClock);
    add(QStringLiteral("container-parity-overlay-led"), sceneParityOverlayLed);
    add(QStringLiteral("container-parity-smeter-dial"), sceneParitySMeterDial);
    add(QStringLiteral("container-parity-web-image"), sceneParityWebImage);
    add(QStringLiteral("container-parity-artwork-options"), sceneParityArtworkOptions);
    add(QStringLiteral("container-parity-signal-text"), sceneParitySignalText);
    add(QStringLiteral("container-parity-sections"), sceneParitySections);
    add(QStringLiteral("container-parity-long-title"), sceneParityLongTitle);
}

} // namespace

QVector<Scene> fixedScenes()
{
    QVector<Scene> scenes;

    scenes.append({QStringLiteral("baseline-default"), QStringLiteral("1"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, defaultPoses(), 0);
        if (ctx.failure.isEmpty()) { captureWindow(ctx, QStringLiteral("baseline-default")); }
    }});

    scenes.append({QStringLiteral("baseline-two-band"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, twoBandPoses(), 0);
        if (ctx.failure.isEmpty()) { captureWindow(ctx, QStringLiteral("baseline-two-band")); }
    }});

    scenes.append({QStringLiteral("baseline-four-rx"), QStringLiteral("2x2"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, fourRxPoses(), 2);
        if (ctx.failure.isEmpty()) { captureWindow(ctx, QStringLiteral("baseline-four-rx")); }
    }});

    scenes.append({QStringLiteral("baseline-tx"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        // Display-only transmit state on slice A. Refuse to run if a
        // connection exists: these seams must never reach a radio.
        if (ctx.model->connection() != nullptr) {
            ctx.failure = QStringLiteral("a RadioConnection exists; refusing the transmit scene");
            return;
        }
        runWindowScene(ctx, twoBandPoses(), 0, poseTxBeforeFreeze, poseTxAfterFeed);
        if (ctx.failure.isEmpty()) { captureWindow(ctx, QStringLiteral("baseline-tx")); }
    }});

    // The manual's overview (desktop-overview, -vfo and -tx): one pan on
    // slice A, receiving, with the TX applet in view, "You control" on the
    // flag and the radio named in the status bar. Nothing connects or keys.
    scenes.append({QStringLiteral("baseline-connected"), QStringLiteral("1"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, {twoBandPoses().first()}, 0,
                       [](SceneContext& c) { poseConnected(c, false); }, showYouControl);
        if (ctx.failure.isEmpty()) { captureWindow(ctx, QStringLiteral("baseline-connected")); }
    }});

    // The manual's TX figure: the same window with the S-meter and RX
    // hidden from the Main Panel, so the whole TX applet (down to TX BW)
    // is in view.
    scenes.append({QStringLiteral("baseline-connected-tx"), QStringLiteral("1"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, {twoBandPoses().first()}, 0,
                       [](SceneContext& c) { poseConnected(c, true); }, showYouControl);
        if (ctx.failure.isEmpty()) { captureWindow(ctx, QStringLiteral("baseline-connected-tx")); }
    }});

    scenes.append({QStringLiteral("baseline-arrange"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, twoBandPoses(), 0, [](SceneContext& c) {
            auto* manager = c.window->findChild<ContainerManager*>();
            if (manager == nullptr || manager->arrangeController() == nullptr) {
                c.failure = QStringLiteral("no ContainerArrangeController");
                return;
            }
            c.notes.append(QStringLiteral(
                "The current app has no separate arrange mode: ContainerManager sets its "
                "ContainerArrangeController on every content host whenever a workspace store "
                "exists, so the entry grips show in every baseline scene."));
        });
        if (ctx.failure.isEmpty()) { captureWindow(ctx, QStringLiteral("baseline-arrange")); }
    }});

    scenes.append({QStringLiteral("baseline-header-always"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneHeaderMode(ctx, HeaderMode::Shown, QStringLiteral("baseline-header-always"));
    }});
    scenes.append({QStringLiteral("baseline-header-reveal"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneHeaderMode(ctx, HeaderMode::Reveal, QStringLiteral("baseline-header-reveal"));
    }});
    scenes.append({QStringLiteral("baseline-header-hidden"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneHeaderMode(ctx, HeaderMode::Hidden, QStringLiteral("baseline-header-hidden"));
    }});

    scenes.append({QStringLiteral("baseline-floating"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, twoBandPoses(), 0, [](SceneContext& c) {
            auto* manager = c.window->findChild<ContainerManager*>();
            ContainerWidget* panel = mainPanel(c);
            if (manager == nullptr || panel == nullptr) {
                c.failure = QStringLiteral("no Main Panel container");
                return;
            }
            manager->floatContainer(panel->id());
        });
        if (!ctx.failure.isEmpty()) { return; }
        captureWindow(ctx, QStringLiteral("baseline-floating"));
        auto* floating = visibleTopLevel<FloatingContainer>();
        if (floating == nullptr) {
            ctx.failure = QStringLiteral("the Main Panel did not float");
            return;
        }
        CapturedImage img;
        img.name = QStringLiteral("baseline-floating--floating");
        img.kind = QStringLiteral("floating");
        img.image = grabWidget(floating);
        img.logicalSize = floating->size();
        img.dpr = img.image.devicePixelRatio();
        img.extra[QStringLiteral("window_title")] = floating->windowTitle();
        attachRegions(img, floating);
        ctx.images.append(img);
    }});

    // ---- proposed workspace headers (workspace preview switch on) ----
    scenes.append({QStringLiteral("header-shown"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedHeader(ctx, HeaderMode::Shown, QStringLiteral("header-shown"));
    }, true});
    scenes.append({QStringLiteral("header-hidden"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedHeader(ctx, HeaderMode::Hidden, QStringLiteral("header-hidden"));
    }, true});
    scenes.append({QStringLiteral("arrange-on"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        // The Main Panel keeps its header (open eye); a Meters container
        // in the top strip hides its own (crossed-out eye), so Arrange
        // draws that one over the top of its content.
        sceneProposedHeader(ctx, HeaderMode::Shown, QStringLiteral("arrange-on"),
                            [](SceneContext& c, ContainerWidget*) {
            if (auto* manager = c.window->findChild<ContainerManager*>()) {
                manager->setArrangeMode(true);
                c.notes.append(QStringLiteral("ContainerManager::setArrangeMode(true)"));
            }
        }, [](SceneContext& c) {
            const QString id = addMetersContainer(c, DockEdge::Top);
            auto* manager = c.window->findChild<ContainerManager*>();
            ContainerWidget* meters = id.isEmpty() ? nullptr : manager->container(id);
            if (meters == nullptr) {
                c.failure = QStringLiteral("the Meters container was not created");
                return;
            }
            emit meters->headerModeRequested(HeaderMode::Hidden);
            c.notes.append(QStringLiteral("Main Panel: header shown; Meters (top strip): header hidden"));
        });
    }, true});

    // ---- proposed four-edge workspace (workspace preview switch on) ----
    scenes.append({QStringLiteral("edge-right"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedEdges(ctx, QStringLiteral("edge-right"), {});
    }, true});
    scenes.append({QStringLiteral("edge-left"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedEdges(ctx, QStringLiteral("edge-left"), [](SceneContext& c) {
            // Options > Move to Edge > Left.
            emit mainPanel(c)->moveToEdgeRequested(DockEdge::Left);
        });
    }, true});
    scenes.append({QStringLiteral("edge-top-bottom"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedEdges(ctx, QStringLiteral("edge-top-bottom"), [](SceneContext& c) {
            addMetersContainer(c, DockEdge::Top);
            addBandContainer(c, DockEdge::Bottom);
        });
    }, true});
    scenes.append({QStringLiteral("edge-all-four"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedEdges(ctx, QStringLiteral("edge-all-four"), [](SceneContext& c) {
            addMetersContainer(c, DockEdge::Top);
            addBandContainer(c, DockEdge::Bottom);
            addLevelsContainer(c, DockEdge::Left);
        });
    }, true});
    scenes.append({QStringLiteral("edge-all-four-clean"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        // edge-all-four with every container's header hidden: no group or
        // entry header shows and none takes room.
        sceneProposedEdges(ctx, QStringLiteral("edge-all-four-clean"), [](SceneContext& c) {
            addMetersContainer(c, DockEdge::Top);
            addBandContainer(c, DockEdge::Bottom);
            addLevelsContainer(c, DockEdge::Left);
            c.notes.append(QStringLiteral("every container: Header Hidden"));
        }, {}, HeaderMode::Hidden);
    }, true});
    // ---- folded receiver flags (Task 26) ----
    // edge-all-four's short pans with both flags folded; the zoom row sits
    // clear of the button column.
    scenes.append({QStringLiteral("flag-fold--short-pans"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedEdges(ctx, QStringLiteral("flag-fold--short-pans"), [](SceneContext& c) {
            addMetersContainer(c, DockEdge::Top);
            addBandContainer(c, DockEdge::Bottom);
            addLevelsContainer(c, DockEdge::Left);
        }, [](SceneContext& c) {
            foldFlagOnPan(c, 0);
            foldFlagOnPan(c, 1);
            pump(kSettleMs);
        });
    }, true});
    // The two-band layout with flag A folded and flag B open, in today's
    // look and with the workspace preview switch on (the flag is the same).
    scenes.append({QStringLiteral("flag-fold--two-band"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneFlagFoldTwoBand(ctx, QStringLiteral("flag-fold--two-band"));
    }});
    scenes.append({QStringLiteral("flag-fold--two-band-look"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneFlagFoldTwoBand(ctx, QStringLiteral("flag-fold--two-band-look"));
    }, true});
    // Each tab of the open flag, in today's look and the workspace look:
    // the tab opened by a click on its tab, nothing else touched.
    for (const bool look : {false, true}) {
        static const char* const kFlagTabs[] = {"audio", "dsp", "mode", "xrit", "vax"};
        for (int tab = 0; tab < 5; ++tab) {
            const QString name = QStringLiteral("flag-tab--%1%2")
                .arg(QLatin1String(kFlagTabs[tab]), look ? QStringLiteral("-look") : QString());
            scenes.append({name, QStringLiteral("1"), false, [name, tab](SceneContext& ctx) {
                sceneFlagTab(ctx, name, tab);
            }, look});
        }
    }
    // Font B review: display-only AUTO/NF state in both matching surfaces.
    scenes.append({QStringLiteral("font-minimum--auto-info"), QStringLiteral("1"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, defaultPoses(), 0, {}, [](SceneContext& c) {
            VfoWidget* flag = flagA(c);
            if (flag != nullptr) {
                flag->showTab(VfoWidget::Tab::Audio);
                flag->updateAgcAutoVisuals(true, -98.0f, 20.0);
            }
            if (RxApplet* rx = c.window->findChild<RxApplet*>()) {
                rx->updateAgcAutoVisuals(true, -98.0f, 20.0);
            }
            pump(kFinalSettleMs);
        });
        if (ctx.failure.isEmpty()) {
            captureWindow(ctx, QStringLiteral("font-minimum--auto-info"));
        }
    }, true});
    // Four folded flags: A right of its marker (LSB), B left of its marker
    // (DIGU), C as the TX slice (the flag's display setter only), D locked
    // (the flag's display setter only). Crops of each at the crop profile.
    scenes.append({QStringLiteral("flag-fold--states"), QStringLiteral("2x2"), false,
                   [](SceneContext& ctx) {
        ctx.extraCrops = addFoldStateCrops;
        ctx.notShown.append(QJsonObject{
            {QStringLiteral("item"), QStringLiteral("open flag with the pointer over its letter")},
            {QStringLiteral("reason"), QStringLiteral(
                "an offscreen grab draws no pointer, and the letter badge has no hover style: "
                "the pointer turns to a pointing hand and nothing else changes")}});
        runWindowScene(ctx, fourRxPoses(), 2, {}, [](SceneContext& c) {
            foldFlagOnPan(c, 0);
            foldFlagOnPan(c, 1);
            foldFlagOnPan(c, 2, [](VfoWidget* f) { f->setTxSlice(true); });
            foldFlagOnPan(c, 3, [](VfoWidget* f) { f->setLocked(true); });
            pump(kSettleMs);
        });
        if (ctx.failure.isEmpty()) {
            captureWindow(ctx, QStringLiteral("flag-fold--states"));
        }
    }});
    // ---- Longpath instruments as container content (switch on) ----
    scenes.append({QStringLiteral("instruments-sheet"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneInstrumentsSheet(ctx); }, true});
    scenes.append({QStringLiteral("mixed-container"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneMixedContainer(ctx, QStringLiteral("mixed-container"), HeaderMode::Shown);
    }, true});
    scenes.append({QStringLiteral("mixed-container-hidden"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        // mixed-container with every header hidden.
        sceneMixedContainer(ctx, QStringLiteral("mixed-container-hidden"), HeaderMode::Hidden);
    }, true});
    // ---- workspace profiles (switch on) ----
    scenes.append({QStringLiteral("profile-ssb"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneProfile(ctx, QStringLiteral("profile-ssb"), SceneProfile::Ssb); },
                   true});
    scenes.append({QStringLiteral("profile-dx"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneProfile(ctx, QStringLiteral("profile-dx"), SceneProfile::Dx); },
                   true});
    scenes.append({QStringLiteral("profile-contest"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProfile(ctx, QStringLiteral("profile-contest"), SceneProfile::Contest);
    }, true});
    scenes.append({QStringLiteral("profile-ft8"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneProfile(ctx, QStringLiteral("profile-ft8"), SceneProfile::Ft8); },
                   true});
    scenes.append({QStringLiteral("profile-rail-rest"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        // SSB with the rail at rest: the active badge only.
        sceneProfile(ctx, QStringLiteral("profile-rail-rest"), SceneProfile::Ssb, {}, {}, true);
    }, true});
    scenes.append({QStringLiteral("profile-rail-open"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        // The fan-out through the dock's open() seam, the pointer over the
        // D badge (hover state, its remove cross) and its tooltip.
        auto tip = std::make_shared<QPointer<QWidget>>();
        sceneProfile(ctx, QStringLiteral("profile-rail-open"), SceneProfile::Ssb, [tip](SceneContext& c) {
            ProfileRailDock* dock = c.window->profileRailDock();
            WorkspaceProfiles* profiles = c.window->workspaceProfiles();
            dock->open();
            hideStaleRailBadges(dock->rail());
            pump(kFinalSettleMs);
            QPushButton* d = profiles->ids().size() > 1 ? dock->rail()->badgeButton(profiles->ids().at(1)) : nullptr;
            if (d == nullptr) {
                c.failure = QStringLiteral("no D badge in the rail");
                return;
            }
            const QPointF local(d->width() / 2.0, d->height() / 2.0);
            d->setAttribute(Qt::WA_UnderMouse, true);
            QEnterEvent enter(local, d->mapTo(d->window(), local), d->mapToGlobal(local));
            QCoreApplication::sendEvent(d, &enter);
            d->update();
            // The pointer's spot; Qt offsets the tooltip from it as it does live.
            // QTipLabel hides itself after about 10 s, which a loaded run
            // can pass before the composite; it is held for the scene.
            // In window-geometry coordinates, as the composite reads them.
            const QPoint pointer = globalOf(d) + local.toPoint();
            QToolTip::showText(pointer, d->toolTip(), d, QRect(), kRailTipHoldMs);
            pump(kFinalSettleMs);
            *tip = visibleToolTip();
            // Qt offsets the tip by the desktop's current pointer size, which
            // changed from launch to launch; placed for the default pointer.
            if (*tip) {
                c.notes.append(QStringLiteral("profile-rail-open: %1")
                                   .arg(placeTipForArrow(tip->data(), pointer)));
                pump(kFinalSettleMs);
            }
            c.notes.append(*tip ? QStringLiteral("pointer over the D badge: hover state set, Enter sent, "
                                                 "tooltip \"%1\" composited where it shows").arg(d->toolTip())
                                : QStringLiteral("pointer over the D badge: hover state set; the tooltip did "
                                                 "not show (background-only process), not composited"));
        }, [tip](SceneContext& c) {
            if (*tip) {
                compositeAndRecrop(c, QStringLiteral("profile-rail-open"), {tip->data()}, true);
            }
        }, true);
    }, true});
    scenes.append({QStringLiteral("profile-new-menu"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        // The + in the fanned-out rail: the New menu, as MainWindow shows it.
        auto menu = std::make_shared<QPointer<QMenu>>();
        sceneProfile(ctx, QStringLiteral("profile-new-menu"), SceneProfile::Ssb, [menu](SceneContext& c) {
            ProfileRailDock* dock = c.window->profileRailDock();
            dock->open();
            hideStaleRailBadges(dock->rail());
            pump(kFinalSettleMs);
            QWidget* plus = dock->rail()->plusButton();
            *menu = WorkspaceProfilesDialog::createNewMenu(c.window->workspaceProfiles(), c.window);
            tagProvenance(menu->data(), provProposed());
            (*menu)->popup(plus->mapToGlobal(QPoint(0, plus->height())));
            pump(kFinalSettleMs);
            if (!(*menu)->isVisible()) {
                c.failure = QStringLiteral("the New menu did not show");
            }
        }, [menu](SceneContext& c) {
            if (!*menu) {
                return;
            }
            compositeAndRecrop(c, QStringLiteral("profile-new-menu"), {menu->data()});
            captureDialog(c, menu->data(), QStringLiteral("profile-new-menu--menu"));
            (*menu)->close();
        });
    }, true});
    scenes.append({QStringLiteral("profile-manage-dialog"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        // Containers > Manage Workspace Profiles over the SSB window, and the
        // dialog alone.
        sceneProfile(ctx, QStringLiteral("profile-manage-dialog"), SceneProfile::Ssb, [](SceneContext& c) {
            auto* action = c.window->findChild<QAction*>(QStringLiteral("containersManageProfiles"));
            if (action == nullptr) {
                c.failure = QStringLiteral("no containersManageProfiles action");
                return;
            }
            action->trigger();
            pump(kDialogSettleMs);
            auto* dialog = c.window->findChild<WorkspaceProfilesDialog*>();
            if (dialog == nullptr) {
                c.failure = QStringLiteral("the manage dialog did not open");
                return;
            }
            dialog->move(c.window->mapToGlobal(c.window->rect().center()) - dialog->rect().center());
            pump(kFinalSettleMs);
        }, [](SceneContext& c) {
            auto* dialog = c.window->findChild<WorkspaceProfilesDialog*>();
            compositeAndRecrop(c, QStringLiteral("profile-manage-dialog"), {dialog});
            captureDialog(c, dialog, QStringLiteral("profile-manage-dialog--dialog"));
            if (dialog != nullptr) {
                dialog->close();
            }
        });
    }, true});
    scenes.append({QStringLiteral("free-container"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedEdges(ctx, QStringLiteral("free-container"), [](SceneContext& c) {
            const QString id = addLevelsContainer(c, DockEdge::Left);
            if (id.isEmpty()) {
                return;
            }
            // Options > Place Free.
            auto* manager = c.window->findChild<ContainerManager*>();
            emit manager->container(id)->placeFreeRequested();
            // Moved by its header to the lower right of the 20 m waterfall,
            // clear of every VFO flag and of the dB and time scales.
            constexpr int kScaleClearancePx = 72;
            constexpr int kBottomClearancePx = 24;
            ContainerWidget* levels = manager->container(id);
            QWidget* pane = levels != nullptr ? levels->parentWidget() : nullptr;
            if (pane == nullptr) {
                c.failure = QStringLiteral("the Levels container was not placed free");
                return;
            }
            WorkspaceDocument draft = manager->workspaceStore()->snapshot();
            for (auto& d : draft.containers) {
                if (d.id == id) {
                    // The committed free size; the widget may not have taken it yet.
                    const QSize size = d.geometry.isValid() ? d.geometry.size() : levels->size();
                    d.geometry = QRect(QPoint(pane->width() - size.width() - kScaleClearancePx,
                                              pane->height() - size.height() - kBottomClearancePx),
                                       size);
                    c.notes.append(QStringLiteral("Levels moved to %1,%2 %3x%4 in a %5x%6 pane")
                                       .arg(d.geometry.x()).arg(d.geometry.y())
                                       .arg(size.width()).arg(size.height())
                                       .arg(pane->width()).arg(pane->height()));
                }
            }
            if (manager->commitWorkspace(draft, draft.revision).status != CommitStatus::Saved) {
                c.failure = QStringLiteral("could not move the Levels container");
            }
        }, [](SceneContext& c) {
            auto* manager = c.window->findChild<ContainerManager*>();
            ContainerWidget* levels = manager->container(QStringLiteral("preview-levels"));
            if (levels == nullptr) {
                c.failure = QStringLiteral("the Levels container was not created");
                return;
            }
            c.notes.append(QStringLiteral("Levels: Place Free, header shown"));
        });
    }, true});
    // floating-container shows the window as it is popped out, its pin off;
    // floating-container-pinned clicks the header's pin first.
    const auto floatingScene = [](const QString& name, bool pinned) {
        return [name, pinned](SceneContext& ctx) {
            sceneProposedEdges(ctx, name, [pinned](SceneContext& c) {
                const QString id = addLevelsContainer(c, DockEdge::Left);
                if (id.isEmpty()) {
                    return;
                }
                auto* manager = c.window->findChild<ContainerManager*>();
                manager->floatContainer(id);
                if (!pinned) {
                    return;
                }
                ContainerWidget* levels = manager->container(id);
                auto* pin = levels != nullptr
                    ? levels->findChild<QAbstractButton*>(QStringLiteral("workspacePin")) : nullptr;
                if (pin == nullptr) {
                    c.failure = QStringLiteral("the floating Levels header has no pin");
                    return;
                }
                pin->click();
                c.notes.append(QStringLiteral("Levels: the header's pin clicked, window kept on top"));
            });
            if (!ctx.failure.isEmpty()) {
                return;
            }
            auto* floating = visibleTopLevel<FloatingContainer>();
            if (floating == nullptr) {
                ctx.failure = QStringLiteral("the Levels container did not float");
                return;
            }
            if (ctx.beforeGrab) {
                ctx.beforeGrab(floating);
                pump(kFinalSettleMs);
            }
            CapturedImage img;
            img.name = name + QStringLiteral("--floating");
            img.kind = QStringLiteral("floating");
            img.image = grabWidget(floating);
            img.logicalSize = floating->size();
            img.dpr = img.image.devicePixelRatio();
            img.extra[QStringLiteral("window_title")] = floating->windowTitle();
            img.extra[QStringLiteral("frameless")] =
                floating->windowFlags().testFlag(Qt::FramelessWindowHint);
            img.extra[QStringLiteral("stays_on_top")] =
                floating->windowFlags().testFlag(Qt::WindowStaysOnTopHint);
            attachRegions(img, floating);
            ctx.images.append(img);
        };
    };
    scenes.append({QStringLiteral("floating-container"), QStringLiteral("2v"), false,
                   floatingScene(QStringLiteral("floating-container"), false), true});
    scenes.append({QStringLiteral("floating-container-pinned"), QStringLiteral("2v"), false,
                   floatingScene(QStringLiteral("floating-container-pinned"), true), true});
    scenes.append({QStringLiteral("drag-preview-left"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedEdges(ctx, QStringLiteral("drag-preview-left"), {}, [](SceneContext& c) {
            auto* manager = c.window->findChild<ContainerManager*>();
            manager->setArrangeMode(true);
            manager->showDropPreviewForTest(DockEdge::Left, 0);
            c.notes.append(QStringLiteral(
                "Arrange on; ContainerManager::showDropPreviewForTest(Left, 0)"));
        });
    }, true});
    scenes.append({QStringLiteral("drag-preview-between"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedEdges(ctx, QStringLiteral("drag-preview-between"), [](SceneContext& c) {
            addLevelsContainer(c, DockEdge::Right);
        }, [](SceneContext& c) {
            auto* manager = c.window->findChild<ContainerManager*>();
            manager->setArrangeMode(true);
            manager->showDropPreviewForTest(DockEdge::Right, 1);
            c.notes.append(QStringLiteral(
                "Arrange on; ContainerManager::showDropPreviewForTest(Right, 1)"));
        });
    }, true});
    // A drag held over an edge while arranging. The pointer is not in a
    // capture.
    scenes.append({QStringLiteral("arrange-drag-edge-free"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedEdges(ctx, QStringLiteral("arrange-drag-edge-free"), [](SceneContext& c) {
            const QString id = addLevelsContainer(c, DockEdge::Left);
            if (id.isEmpty()) {
                return;
            }
            // Options > Place Free.
            emit c.window->findChild<ContainerManager*>()->container(id)->placeFreeRequested();
        }, [](SceneContext& c) {
            auto* manager = c.window->findChild<ContainerManager*>();
            ContainerWidget* levels = manager->container(QStringLiteral("preview-levels"));
            QWidget* pane = levels != nullptr ? levels->parentWidget() : nullptr;
            if (pane == nullptr || !levels->isOverlayDocked()) {
                c.failure = QStringLiteral("the Levels container was not placed free");
                return;
            }
            manager->setArrangeMode(true);
            pump(kSettleMs);
            // A held drag: the header is pressed a quarter of the way
            // along, which is what keeps the workspace from putting the
            // container back at its saved place, and the container is put
            // where a pointer in the left edge zone has it, part of it past
            // the pane's edge (a drag does not hold a container inside the
            // pane). The pointer of a window that is not on screen maps to
            // no edge zone, so the landing box comes from the preview seam.
            QWidget* header = levels->workspaceHeader();
            if (header == nullptr || !header->isVisible()) {
                c.failure = QStringLiteral("the Levels container has no header to drag");
                return;
            }
            constexpr int kPointerFromEdgePx = 12;
            constexpr int kDragTopPx = 96;
            const QPoint grab(header->width() / 4, header->height() / 2);
            QTest::mousePress(header, Qt::LeftButton, Qt::NoModifier, grab);
            if (!levels->geometryInteractionActive()) {
                c.failure = QStringLiteral("pressing the Levels header began no drag");
                return;
            }
            levels->move(kPointerFromEdgePx - grab.x(), kDragTopPx);
            levels->raise();
            manager->showDropPreviewForTest(DockEdge::Left, 0);
            c.notes.append(QStringLiteral(
                "Arrange on; Levels free, its header pressed and held, the container moved to %1,%2 "
                "of the pane as a drag to the left edge has it; "
                "ContainerManager::showDropPreviewForTest(Left, 0)")
                               .arg(levels->x()).arg(levels->y()));
        });
        // The held drag is given up: nothing is committed.
        QTest::keyClick(ctx.window, Qt::Key_Escape);
    }, true});
    scenes.append({QStringLiteral("arrange-drag-edge-floating"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedEdges(ctx, QStringLiteral("arrange-drag-edge-floating"), [](SceneContext& c) {
            const QString id = addLevelsContainer(c, DockEdge::Left);
            if (id.isEmpty()) {
                return;
            }
            c.window->findChild<ContainerManager*>()->floatContainer(id);
        }, [](SceneContext& c) {
            auto* manager = c.window->findChild<ContainerManager*>();
            auto* area = c.window->findChild<EdgeDockArea*>();
            manager->setArrangeMode(true);
            pump(kSettleMs);
            QWidget* floating = floatingWindowOf(manager, QStringLiteral("preview-levels"));
            if (floating == nullptr || area == nullptr) {
                c.failure = QStringLiteral("the Levels container did not float");
                return;
            }
            // Where the drag has it: the window's header, held near its
            // right end, under a pointer in the right edge zone of the main
            // window and above the middle of the Main Panel, so the landing
            // box is over the Main Panel.
            constexpr int kPointerFromEdgePx = 12;
            const QPoint kHeaderGrabPx(floating->width() - 96, 12);
            // By window geometry: a top-level shown with WA_DontShowOnScreen
            // keeps a stale mapToGlobal (see topOffset).
            const QPoint inWindow = area->mapTo(
                c.window, QPoint(area->width() - kPointerFromEdgePx, area->height() / 4));
            floating->move(c.window->geometry().topLeft() + inWindow - kHeaderGrabPx);
            manager->showDropPreviewForTest(DockEdge::Right, 0);
            c.notes.append(QStringLiteral(
                "Arrange on; the Levels window moved so its header is at %1,%2 of the main window, "
                "in the right edge zone; ContainerManager::showDropPreviewForTest(Right, 0). "
                "The window is its own image, arrange-drag-edge-floating--floating.")
                               .arg(inWindow.x()).arg(inWindow.y()));
        });
        if (!ctx.failure.isEmpty()) {
            return;
        }
        QWidget* floating = floatingWindowOf(ctx.window->findChild<ContainerManager*>(),
                                             QStringLiteral("preview-levels"));
        if (floating == nullptr) {
            ctx.failure = QStringLiteral("the Levels container did not float");
            return;
        }
        if (ctx.beforeGrab) {
            ctx.beforeGrab(floating);
            pump(kFinalSettleMs);
        }
        // The window is a top-level of its own: painted over the main
        // window image where it is, and written as an image of its own.
        compositeAndRecrop(ctx, QStringLiteral("arrange-drag-edge-floating"), {floating}, true);
        CapturedImage img;
        img.name = QStringLiteral("arrange-drag-edge-floating--floating");
        img.kind = QStringLiteral("floating");
        img.image = grabWidget(floating);
        img.logicalSize = floating->size();
        img.dpr = img.image.devicePixelRatio();
        img.extra[QStringLiteral("window_title")] = floating->windowTitle();
        img.extra[QStringLiteral("position_in_main_window")] =
            QJsonArray{topOffset(ctx.window, floating, true).x(),
                       topOffset(ctx.window, floating, true).y()};
        attachRegions(img, floating);
        ctx.images.append(img);
    }, true});

    // ---- coverage scenes (switch on) ----
    scenes.append({QStringLiteral("clean-spectrum"), QStringLiteral("1"), false,
                   [](SceneContext& ctx) { sceneCleanSpectrum(ctx); }, true});
    scenes.append({QStringLiteral("cw-digital"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneCwDigital(ctx); }, true});
    scenes.append({QStringLiteral("tx-processing"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneTxProcessing(ctx); }, true});
    scenes.append({QStringLiteral("edit-add-content"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneEditAddContent(ctx); }, true});
    scenes.append({QStringLiteral("edit-source-select"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneEditSourceSelect(ctx); }, true});
    scenes.append({QStringLiteral("edit-properties"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneEditProperties(ctx); }, true});
    scenes.append({QStringLiteral("four-rx-look"), QStringLiteral("2x2"), false,
                   [](SceneContext& ctx) { sceneFourRxLook(ctx); }, true});
    // ---- added SIGNAL STRENGTH dial copies (switch on) ----
    scenes.append({QStringLiteral("dial-addable--two-band"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneDialTwoBand(ctx, QStringLiteral("dial-addable--two-band")); },
                   true});
    scenes.append({QStringLiteral("dial-addable--mixed"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneDialMixed(ctx); }, true});
    scenes.append({QStringLiteral("dial-addable--narrow"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneDialNarrow(ctx); }, true});
    scenes.append({QStringLiteral("dial-addable--tx"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneDialTx(ctx, QStringLiteral("dial-addable--tx")); }, true});
    scenes.append({QStringLiteral("dial-addable--amp"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneDialAmp(ctx); }, true});
    scenes.append({QStringLiteral("dial-addable--face"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneDialFace(ctx); }, true});
    scenes.append({QStringLiteral("dial-addable--closed"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneDialClosed(ctx); }, true});
    scenes.append({QStringLiteral("dial-addable--add-content"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneDialAddContent(ctx); }, true});

    // ---- controls: today, preview now and Longpath side by side ----
    // Starts in today's look (switch off): the sheet switches the looks
    // itself and returns to today's.
    // Applet minimum widths against the Main Panel viewport: today's
    // window, and the preview's right strip as JJ runs it.
    scenes.append({QStringLiteral("strip-fit-today"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, twoBandPoses(), 0);
        if (!ctx.failure.isEmpty()) { return; }
        const QJsonObject fit = measurePanelFit(ctx, QStringLiteral("today"));
        captureWindow(ctx, QStringLiteral("strip-fit-today"));
        appendPanelFitCrop(ctx, QStringLiteral("strip-fit-today"), fit);
    }});
    scenes.append({QStringLiteral("strip-fit-preview"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        auto fit = std::make_shared<QJsonObject>();
        sceneProposedEdges(ctx, QStringLiteral("strip-fit-preview"), {}, [fit](SceneContext& c) {
            *fit = measurePanelFit(c, QStringLiteral("preview now"));
        });
        if (ctx.failure.isEmpty()) {
            appendPanelFitCrop(ctx, QStringLiteral("strip-fit-preview"), *fit);
        }
    }, true});

    scenes.append({QStringLiteral("controls-sheet"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, twoBandPoses(), 0);
        if (ctx.failure.isEmpty()) { sceneControlsSheet(ctx); }
    }});
    // ---- decided status and data colours ----
    // The decided status tones on the real title bar (with the pulse's two
    // steps), badges, the TCI Server page and Network Diagnostics.
    scenes.append({QStringLiteral("decided--status"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, twoBandPoses(), 0);
        if (ctx.failure.isEmpty()) { sceneStatusColoursDecided(ctx); }
    }, true});
    // The decided data colours on the two-band window with the Thetis bar strips
    // (top: S-Meter preset's SIGNALAVG and Power/SWR; left: SIGNALMAXBIN
    // and COMP), a Longpath needle on slice A and the Main Panel.
    scenes.append({QStringLiteral("decided--data"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, twoBandPoses(), 0, [](SceneContext& c) {
            addMetersContainer(c, DockEdge::Top);
            if (c.failure.isEmpty()) { addLevelsContainer(c, DockEdge::Left); }
            if (c.failure.isEmpty()) {
                addSceneContainer(c, QStringLiteral("Needle"), DockEdge::Left, ContentLayout::VerticalStack,
                                  {{QString::fromLatin1(LongpathContent::kNeedleType), {}, {}, rxSource(1)}});
            }
        }, [](SceneContext& c) {
            feedSliceInstruments(c, c.window);
            // Display-only readings for the bar rows, repeated so the
            // bars' attack smoothing settles on them.
            constexpr double kAvgDbm = -73.0;
            constexpr double kMaxBinDbm = -68.0;
            constexpr double kCompDb = -8.0;
            constexpr double kCompPeakDb = -4.0;
            constexpr int kSettleUpdates = 40;
            constexpr qint64 kFrozenClockMs = 1'000'000'000'000;
            constexpr qint64 kStepMs = 100;
            for (MeterWidget* m : c.window->findChildren<MeterWidget*>()) {
                for (int i = 0; i < kSettleUpdates; ++i) {
                    m->updateMeterValue(MeterBinding::SignalAvg, kAvgDbm);
                    m->updateMeterValue(MeterBinding::SignalMaxBin, kMaxBinDbm);
                    m->updateMeterValue(MeterBinding::TxComp, kCompDb);
                    m->updateMeterValue(MeterBinding::TxCompPeak, kCompPeakDb);
                    // The Thetis bar strips smooth on the frame clock: step
                    // it by hand on a time base far past the real one, so a
                    // later real frame never moves the settled marker.
                    m->advanceMeters(kFrozenClockMs + i * kStepMs);
                }
            }
            c.fedReadings.append(QJsonObject{
                {QStringLiteral("instrument"), QStringLiteral("MeterWidget bar rows")},
                {QStringLiteral("values"), QJsonObject{{QStringLiteral("SignalAvg_dbm"), kAvgDbm},
                                                       {QStringLiteral("SignalMaxBin_dbm"), kMaxBinDbm},
                                                       {QStringLiteral("TxComp_db"), kCompDb},
                                                       {QStringLiteral("TxCompPeak_db"), kCompPeakDb}}},
                {QStringLiteral("seam"), QStringLiteral("MeterWidget::updateMeterValue (display only)")}});
            pump(kSettleMs);
        });
        if (ctx.failure.isEmpty()) { sceneDataColoursDecided(ctx); }
    }, true});
    // JJ's left strip, the RX applet over GPU meters,
    // unscrolled (the meters must show their scales) and scrolled part way,
    // in today's look and the workspace look.
    for (const bool look : {false, true}) {
        for (const int scrollPx : {0, 70}) {
            const QString name = QStringLiteral("strip-scroll--%1-%2")
                .arg(look ? QStringLiteral("workspace") : QStringLiteral("today"),
                     scrollPx == 0 ? QStringLiteral("unscrolled") : QStringLiteral("scrolled"));
            scenes.append({name, QStringLiteral("2v"), false, [name, scrollPx](SceneContext& ctx) {
                ctx.extraCrops = addStripCrops;
                runWindowScene(ctx, twoBandPoses(), 0, [](SceneContext& c) { addReceiveStrip(c); },
                               [scrollPx](SceneContext& c) {
                    QScrollArea* scroll = receiveStripScroll(c);
                    if (scroll == nullptr) {
                        c.failure = QStringLiteral("no Receive strip scroll area");
                        return;
                    }
                    noteMeterWindows(c, scroll, QStringLiteral("before scrolling"));
                    QScrollBar* bar = scroll->verticalScrollBar();
                    bar->setValue(qMin(scrollPx, bar->maximum()));
                    pump(kSettleMs);
                    c.notes.append(QJsonObject{{QStringLiteral("scroll_value"), bar->value()},
                                               {QStringLiteral("scroll_maximum"), bar->maximum()}});
                    noteMeterWindows(c, scroll, QStringLiteral("after scrolling"));
                });
                if (ctx.failure.isEmpty()) {
                    captureWindow(ctx, name);
                }
            }, look});
        }
    }
    // JJ's decided sliders, indicators, lit buttons, radii and scroll bars
    // through the product path (ControlsSheet.cpp).
    scenes.append({QStringLiteral("decided--controls"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, twoBandPoses(), 0);
        if (ctx.failure.isEmpty()) { sceneControlsDecided(ctx); }
    }, true});
    scenes.append({QStringLiteral("controls-sheet--indicators"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, twoBandPoses(), 0);
        if (ctx.failure.isEmpty()) { sceneIndicatorChoices(ctx); }
    }});
    scenes.append({QStringLiteral("controls-sheet--lit-buttons"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, twoBandPoses(), 0);
        if (ctx.failure.isEmpty()) { sceneLitButtons(ctx); }
    }});
    scenes.append({QStringLiteral("controls-sheet--lit-green"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, twoBandPoses(), 0);
        if (ctx.failure.isEmpty()) { sceneLitGreenChoices(ctx); }
    }});
    scenes.append({QStringLiteral("controls-sheet--scrollbars"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, twoBandPoses(), 0);
        if (ctx.failure.isEmpty()) {
            // The sheet first, with the switch off; then the live bars,
            // which turn the switch on for the Setup page and off again.
            sceneScrollBarChoices(ctx, QJsonObject());
            const QJsonObject live = liveScrollBars(ctx);
            for (CapturedImage& c : ctx.images) {
                if (c.name == QLatin1String("controls-sheet--scrollbars")) {
                    c.extra.insert(QStringLiteral("live_bars"), live);
                }
            }
        }
    }});
    scenes.append({QStringLiteral("controls-sheet--sizes"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, twoBandPoses(), 0);
        if (ctx.failure.isEmpty()) { sceneControlSizes(ctx); }
    }});

    // ---- fixed dialogs ----
    scenes.append({QStringLiteral("dialog-tx-eq"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        auto* action = ctx.window->findChild<QAction*>(QStringLiteral("toolsTxEqualizer"));
        if (action == nullptr) {
            ctx.failure = QStringLiteral("no toolsTxEqualizer action");
            return;
        }
        action->trigger();
        captureDialog(ctx, visibleTopLevel<TxEqDialog>(), QStringLiteral("dialog-tx-eq"));
    }});
    scenes.append({QStringLiteral("dialog-tx-cfc"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        auto* tx = ctx.window->findChild<TxApplet*>();
        if (tx == nullptr) {
            ctx.failure = QStringLiteral("no TxApplet");
            return;
        }
        tx->requestOpenCfcDialog();
        captureDialog(ctx, visibleTopLevel<TxCfcDialog>(), QStringLiteral("dialog-tx-cfc"));
    }});
    scenes.append({QStringLiteral("dialog-container-settings"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        auto* manager = ctx.window->findChild<ContainerManager*>();
        ContainerWidget* panel = mainPanel(ctx);
        if (manager == nullptr || panel == nullptr) {
            ctx.failure = QStringLiteral("no Main Panel container");
            return;
        }
        // The Containers > Edit Container path (MainWindow.cpp:11138), shown
        // instead of exec()'d so the grab needs no nested loop.
        ContainerSettingsDialog dialog(panel, ctx.window, manager);
        dialog.show();
        captureDialog(ctx, &dialog, QStringLiteral("dialog-container-settings"));
        dialog.reject();
    }});
    scenes.append({QStringLiteral("dialog-pan-layout"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        captureModal<PanLayoutDialog>(ctx, QStringLiteral("dialog-pan-layout"), [&ctx] {
            QMetaObject::invokeMethod(ctx.window, "showPanLayoutDialog", Qt::DirectConnection);
        });
    }});
    scenes.append({QStringLiteral("dialog-pure-signal"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        QMetaObject::invokeMethod(ctx.window, "openPureSignalDialog", Qt::DirectConnection);
        captureDialog(ctx, visibleTopLevel<PsForm>(), QStringLiteral("dialog-pure-signal"));
    }});
    scenes.append({QStringLiteral("dialog-diversity"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        QMetaObject::invokeMethod(ctx.window, "openDiversityDialog", Qt::DirectConnection);
        captureDialog(ctx, visibleTopLevel<DiversityDialog>(), QStringLiteral("dialog-diversity"));
    }});
    appendContainerParityScenes(scenes);
    appendInventoryDialogScenes(scenes);
    appendMenuScenes(scenes);
    return scenes;
}

// Setup > Diagnostics > Radio Status shows the time since the bench
// connection came up, which moved by a second between runs. Its clock is
// a QTimer member of the page (not a child, so stopTimersUnder() cannot
// reach it): cut every timer that ticks into the page as it fires, cut the
// model's connection-state tick, then show a fixed time.
constexpr auto kUptimeReadoutId = "diagnostics.radioStatus.uptime";
constexpr auto kFrozenUptime = "14m 32s";
// Longer than the page's 1000 ms uptime tick.
constexpr int kUptimeCutWaitMs = 1300;

void freezeRadioStatusUptime(SceneContext& ctx, QWidget* dialog)
{
    QLabel* uptime = nullptr;
    for (QLabel* label : dialog->findChildren<QLabel*>()) {
        if (label->property("nereusSetupId").toString() == QLatin1String(kUptimeReadoutId)) {
            uptime = label;
        }
    }
    if (uptime == nullptr) {
        return;
    }
    QWidget* page = uptime->parentWidget();
    while (page != nullptr && !page->inherits("NereusSDR::RadioStatusPage")) {
        page = page->parentWidget();
    }
    if (page == nullptr) {
        return;
    }
    class TimerCut : public QObject
    {
    public:
        explicit TimerCut(QObject* target) : m_target(target) {}
        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (event->type() == QEvent::Timer) {
                if (auto* timer = qobject_cast<QTimer*>(watched);
                    timer != nullptr && QObject::disconnect(timer, &QTimer::timeout, m_target, nullptr)) {
                    timer->stop();
                }
            }
            return false;
        }

    private:
        QObject* m_target;
    } cut(page);
    qApp->installEventFilter(&cut);
    pump(kUptimeCutWaitMs);
    qApp->removeEventFilter(&cut);
    QObject::disconnect(ctx.model, &RadioModel::connectionStateChanged, page, nullptr);
    uptime->setText(QString::fromLatin1(kFrozenUptime));
    ctx.notes.append(QStringLiteral("radio-status uptime held at %1").arg(QString::fromLatin1(kFrozenUptime)));
}

// ---- Setup's other tabs and scrolled pages ---------------------------------

namespace {

// One later tab of a Setup page (the page scenes show each page on its
// first tab). `path` names the tab from the outside in: a tab inside a tab
// is {outer, inner}. The board is the identity the model is posed as (a
// model value; nothing connects): the HL2 tabs show only for a board with
// the HL2 I/O board (HardwarePage::onCurrentRadioChanged).
struct SetupTabSpec {
    QString page;   // the page scene's stem: dialog-setup-<page>
    QStringList path;
    HPSDRHW board{HPSDRHW::Saturn};
};

QVector<SetupTabSpec> setupTabSpecs()
{
    const QString cores = QStringLiteral("your-cores");
    const QString hardware = QStringLiteral("hardware-config");
    const QString nr = QStringLiteral("nr-anf");
    const QString amps = QStringLiteral("4o3a");
    const QString rfKit = QStringLiteral("rf-kit");
    const QString alex = QStringLiteral("Antenna / ALEX");
    const QString oc = QStringLiteral("OC Outputs");
    const QString hl2Oc = QStringLiteral("Hermes Lite Control");
    const HPSDRHW hl2 = HPSDRHW::HermesLite;
    return {
        // CoresSetupPage.cpp:157-159 (a QTabBar over a QStackedWidget).
        {cores, {QStringLiteral("Addresses")}},
        {cores, {QStringLiteral("Radio")}},
        {cores, {QStringLiteral("Devices")}},
        // HardwarePage.cpp:161-168, shown for the posed board (:301-334).
        {hardware, {alex}},
        {hardware, {alex, QStringLiteral("Alex-1 Filters")}},   // AntennaAlexTab.cpp:137
        {hardware, {alex, QStringLiteral("Alex-2 Filters")}},   // AntennaAlexTab.cpp:156
        {hardware, {oc}},
        {hardware, {oc, QStringLiteral("SWL")}},                // OcOutputsTab.cpp:161
        {hardware, {QStringLiteral("Calibration")}},
        {hardware, {hl2Oc}, hl2},                               // HardwarePage.cpp:314-315
        {hardware, {hl2Oc, QStringLiteral("SWL")}, hl2},
        {hardware, {QStringLiteral("HL2 Options")}, hl2},
        {hardware, {QStringLiteral("HL2 I/O")}, hl2},
        // DspSetupPages.cpp:995-1722 (NR1 is the first tab).
        {nr, {QStringLiteral("NR2")}},
        {nr, {QStringLiteral("NR3")}},
        {nr, {QStringLiteral("NR4")}},
        {nr, {QStringLiteral("DFNR")}},
        {nr, {QStringLiteral("MNR")}},
        {nr, {QStringLiteral("NNR")}},
        {nr, {QStringLiteral("ANF")}},
        // FourO3APage.cpp:124-129 (a local window).
        {amps, {QStringLiteral("PowerGenius XL")}},
        {amps, {QStringLiteral("Tuner Genius XL")}},
        // RfKitPage.cpp:98-99.
        {rfKit, {QStringLiteral("RF2K-S")}},
    };
}

// The pages and tabs whose content runs past the dialog: each gets a
// --scrolled scene stepping through it. An empty path is the page's
// first tab. Measured with the dialog at its own size (the same at both
// dialog profiles); a scene fails if its page no longer overflows.
struct SetupScrollSpec {
    QString page;   // the page scene's stem
    QStringList path;
    HPSDRHW board{HPSDRHW::Saturn};
};

QVector<SetupScrollSpec> setupScrollSpecs()
{
    const QString hardware = QStringLiteral("hardware-config");
    const QString alex = QStringLiteral("Antenna / ALEX");
    const QString oc = QStringLiteral("OC Outputs");
    const QString hl2Oc = QStringLiteral("Hermes Lite Control");
    const QString swl = QStringLiteral("SWL");
    const HPSDRHW hl2 = HPSDRHW::HermesLite;
    return {
        {QStringLiteral("general-options"), {}},
        {QStringLiteral("pa-gain"), {}},
        {QStringLiteral("digital-modes"), {}},
        {QStringLiteral("spectrum-defaults"), {}},
        {QStringLiteral("waterfall-defaults"), {}},
        {QStringLiteral("power"), {}},
        {QStringLiteral("cat-options"), {}},
        {QStringLiteral("tcp-ip-cat"), {}},
        {QStringLiteral("tci-server"), {}},
        {QStringLiteral("4o3a"), {}},
        {QStringLiteral("4o3a"), {QStringLiteral("PowerGenius XL")}},
        {QStringLiteral("4o3a"), {QStringLiteral("Tuner Genius XL")}},
        {QStringLiteral("remote-access"), {}},
        {hardware, {alex, QStringLiteral("Alex-1 Filters")}},
        {hardware, {alex, QStringLiteral("Alex-2 Filters")}},
        {hardware, {oc}},
        // No Calibration entry: under D25 a row that fits side by side
        // without a vertical scroll bar is side by side, and at the
        // dialog's own size the tab's two columns do, with nothing left to
        // scroll (SetupFit::FitScrollArea). It used to stack, and so
        // scroll, only when a scroll bar happened to show first. The --min
        // scene still shows it stacked at 820 x 600.
        {hardware, {hl2Oc}, hl2},
        {hardware, {QStringLiteral("HL2 I/O")}, hl2},
        {QStringLiteral("nr-anf"), {QStringLiteral("NNR")}},
    };
}

// The pages and tabs that rearrange to fit the Settings window (JJ's
// decision D25), shown at another dialog size: `--min` at the dialog's
// minimum (SetupDialog.cpp:344-345, 820 x 600), where their groups stack,
// and `--wide` at 1440 x 900, where Antenna / ALEX's and OC Outputs' groups
// go back side by side.
struct SetupSizeSpec {
    QString page;   // the page scene's stem
    QStringList path;
    HPSDRHW board{HPSDRHW::Saturn};
    QString suffix; // "--min" or "--wide"
    QSize size;
};

constexpr QSize kSetupMinimumSize{820, 600};
constexpr QSize kSetupWideSize{1440, 900};

QVector<SetupSizeSpec> setupSizeSpecs()
{
    const QString hardware = QStringLiteral("hardware-config");
    const QString alex = QStringLiteral("Antenna / ALEX");
    const QString alex1 = QStringLiteral("Alex-1 Filters");
    const QString alex2 = QStringLiteral("Alex-2 Filters");
    const QString oc = QStringLiteral("OC Outputs");
    const QString hl2Oc = QStringLiteral("Hermes Lite Control");
    const QString swl = QStringLiteral("SWL");
    const HPSDRHW saturn = HPSDRHW::Saturn;
    const HPSDRHW hl2 = HPSDRHW::HermesLite;
    const QString min = QStringLiteral("--min");
    const QString wide = QStringLiteral("--wide");
    return {
        {hardware, {}, saturn, min, kSetupMinimumSize},
        {hardware, {alex}, saturn, min, kSetupMinimumSize},
        {hardware, {alex, alex1}, saturn, min, kSetupMinimumSize},
        {hardware, {alex, alex2}, saturn, min, kSetupMinimumSize},
        {hardware, {oc}, saturn, min, kSetupMinimumSize},
        {hardware, {oc, swl}, saturn, min, kSetupMinimumSize},
        {hardware, {QStringLiteral("Calibration")}, saturn, min, kSetupMinimumSize},
        {hardware, {hl2Oc}, hl2, min, kSetupMinimumSize},
        {hardware, {hl2Oc, swl}, hl2, min, kSetupMinimumSize},
        {hardware, {QStringLiteral("HL2 Options")}, hl2, min, kSetupMinimumSize},
        {hardware, {QStringLiteral("HL2 I/O")}, hl2, min, kSetupMinimumSize},
        {QStringLiteral("pa-gain"), {}, saturn, min, kSetupMinimumSize},
        {QStringLiteral("dexp-vox"), {}, saturn, min, kSetupMinimumSize},
        {QStringLiteral("export-import"), {}, saturn, min, kSetupMinimumSize},
        {QStringLiteral("startup-preferences"), {}, saturn, min, kSetupMinimumSize},
        {QStringLiteral("radio-status"), {}, saturn, min, kSetupMinimumSize},
        {QStringLiteral("nr-anf"), {QStringLiteral("DFNR")}, saturn, min, kSetupMinimumSize},
        {QStringLiteral("nr-anf"), {QStringLiteral("MNR")}, saturn, min, kSetupMinimumSize},
        {QStringLiteral("4o3a"), {}, saturn, min, kSetupMinimumSize},
        {QStringLiteral("rf-kit"), {QStringLiteral("RF2K-S")}, saturn, min, kSetupMinimumSize},
        {hardware, {alex}, saturn, wide, kSetupWideSize},
        {hardware, {alex, alex1}, saturn, wide, kSetupWideSize},
        {hardware, {alex, alex2}, saturn, wide, kSetupWideSize},
        {hardware, {oc}, saturn, wide, kSetupWideSize},
        {hardware, {oc, swl}, saturn, wide, kSetupWideSize},
    };
}

// The pages a manual figure shows whole, in one image: the dialog keeps
// its width and grows down until the page's own scroll area has nothing
// left to scroll. `name` is the scene's full name after dialog-setup-.
struct SetupFullSpec {
    QString page;   // the page scene's stem
    QString name;
};

QVector<SetupFullSpec> setupFullSpecs()
{
    return {
        // desktop-modes-audio: the VAX 1 to 4 cards and the TCI audio settings.
        {QStringLiteral("digital-modes"), QStringLiteral("digital-modes-full")},
    };
}

// At most this many growths before a page that still scrolls fails.
constexpr int kSetupGrowSteps = 6;

QString setupTabStem(const QStringList& path)
{
    QString stem;
    for (const QString& label : path) {
        stem += QStringLiteral("--") + slug(label);
    }
    return stem;
}

// The test radios the connection panel scene shows (sampleRadio), one per
// posed board.
RadioInfo posedRadio(HPSDRHW board)
{
    if (board == HPSDRHW::HermesLite) {
        return sampleRadio(QStringLiteral("Hermes Lite 2"), QStringLiteral("00:1C:C0:7B:90:3C"),
                           QStringLiteral("192.168.1.31"), HPSDRHW::HermesLite,
                           ProtocolVersion::Protocol1, 73);
    }
    return sampleRadio(QStringLiteral("ANAN-G2"), QStringLiteral("00:1C:C0:4E:21:0A"),
                       QStringLiteral("192.168.1.20"), HPSDRHW::Saturn, ProtocolVersion::Protocol2, 27);
}

// Selects each tab of `path` in turn, outside in: a QTabWidget, or a bare
// QTabBar (Your Cores), that is showing and holds a tab with that text.
bool selectSetupTabs(SceneContext& ctx, QWidget* dialog, const QStringList& path)
{
    for (const QString& label : path) {
        bool found = false;
        for (QTabWidget* tabs : dialog->findChildren<QTabWidget*>()) {
            if (found || !tabs->isVisible()) {
                continue;
            }
            for (int i = 0; i < tabs->count() && !found; ++i) {
                if (tabs->tabText(i) != label) {
                    continue;
                }
                if (!tabs->isTabVisible(i)) {
                    ctx.failure = QStringLiteral("Setup tab %1 is hidden").arg(label);
                    return false;
                }
                tabs->setCurrentIndex(i);
                found = true;
            }
        }
        for (QTabBar* bar : dialog->findChildren<QTabBar*>()) {
            if (found || !bar->isVisible() || qobject_cast<QTabWidget*>(bar->parentWidget()) != nullptr) {
                continue;
            }
            for (int i = 0; i < bar->count() && !found; ++i) {
                if (bar->tabText(i) == label && bar->isTabVisible(i)) {
                    bar->setCurrentIndex(i);
                    found = true;
                }
            }
        }
        if (!found) {
            ctx.failure = QStringLiteral("no Setup tab %1 is showing").arg(label);
            return false;
        }
        pump(kTabSettleMs);
    }
    return true;
}

// No hover on any scroll bar in the dialog from the real pointer, the way
// the page tree keeps none (poseSetupTree).
void clearScrollBarHover(QWidget* dialog)
{
    for (QScrollBar* bar : dialog->findChildren<QScrollBar*>()) {
        bar->setAttribute(Qt::WA_Hover, false);
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(bar, &leave);
    }
}

bool barOverflows(const QScrollBar* bar)
{
    return bar->isVisible() && bar->maximum() > 0;
}

// The scroll areas that scroll the shown page or tab: every visible
// QScrollArea in the dialog with a bar showing, less any inside another
// one. (The page tree is a QTreeWidget, not a QScrollArea.)
QVector<QScrollArea*> overflowingAreas(QWidget* dialog)
{
    QVector<QScrollArea*> all;
    for (QScrollArea* area : dialog->findChildren<QScrollArea*>()) {
        if (area->isVisible()
            && (barOverflows(area->verticalScrollBar()) || barOverflows(area->horizontalScrollBar()))) {
            all.append(area);
        }
    }
    QVector<QScrollArea*> outer;
    for (QScrollArea* area : all) {
        bool inside = false;
        for (QScrollArea* other : all) {
            inside = inside || (other != area && other->isAncestorOf(area));
        }
        if (!inside) {
            outer.append(area);
        }
    }
    return outer;
}

// One viewport per step, the next step starting this much before the end
// of the last (logical px).
constexpr int kScrollOverlapPx = 40;
// The band under each scrolled image that says which step it is.
constexpr int kStepCaptionPx = 22;
constexpr int kStepCaptionFontPx = 12;
constexpr int kStepCaptionInsetPx = 8;
constexpr QRgb kStepCaptionFill = qRgb(0x2a, 0x2a, 0x30);
constexpr QRgb kStepCaptionText = qRgb(0xe8, 0xe8, 0xec);

// The scroll positions after the top: one viewport less the overlap at a
// time, the last one the bottom (or the right end).
QVector<int> scrollSteps(const QScrollBar* bar)
{
    QVector<int> steps;
    const int page = bar->pageStep();
    const int stride = std::max(page - kScrollOverlapPx, std::max(1, page / 2));
    for (int at = stride;; at += stride) {
        const int pos = std::min(at, bar->maximum());
        steps.append(pos);
        if (pos >= bar->maximum()) {
            break;
        }
    }
    return steps;
}

// The grab with a band under it naming the step.
QImage withStepCaption(const QImage& grab, const QString& text)
{
    const double dpr = grab.devicePixelRatio();
    const int band = static_cast<int>(std::lround(kStepCaptionPx * dpr));
    QImage out(grab.width(), grab.height() + band, QImage::Format_ARGB32_Premultiplied);
    out.fill(kStepCaptionFill);
    out.setDevicePixelRatio(dpr);
    QPainter p(&out);
    p.drawImage(QPointF(0, 0), grab);
    QFont font = QApplication::font();
    font.setPixelSize(kStepCaptionFontPx);
    p.setFont(font);
    p.setPen(QColor(kStepCaptionText));
    const double logicalWidth = grab.width() / dpr;
    const double logicalHeight = grab.height() / dpr;
    p.drawText(QRectF(kStepCaptionInsetPx, logicalHeight, logicalWidth - 2 * kStepCaptionInsetPx,
                      kStepCaptionPx),
               Qt::AlignVCenter | Qt::AlignLeft, text);
    p.end();
    return out;
}

// Opens Setup on the page at `index`, posed as every page scene is: the
// item centred in the tree, no hover row, Radio Status's uptime held.
SetupDialog* openSetupPageScene(SceneContext& ctx, int index)
{
    prepareForDialog(ctx);
    SetupDialog* d = openSetupDialog(ctx.window);
    if (d == nullptr) {
        ctx.failure = QStringLiteral("Setup did not open");
        return nullptr;
    }
    auto* tree = d->findChild<QTreeWidget*>();
    QTreeWidgetItem* item = tree != nullptr ? treeItemForIndex(tree, index) : nullptr;
    if (item == nullptr) {
        ctx.failure = QStringLiteral("no tree item for page %1").arg(index);
        return nullptr;
    }
    tree->setCurrentItem(item);
    // setCurrentItem() scrolls the tree only as far as it must, from
    // wherever a pending layout left it, so the offset varied by run.
    // Settle, then centre the item: the same tree scroll every run.
    pump(kFinalSettleMs);
    tree->scrollToItem(item, QAbstractItemView::PositionAtCenter);
    // Qt sends the tree a synthetic enter from the real pointer,
    // wherever it is on screen, when the dialog shows, and one row
    // stayed hovered in some runs: no hover in the page tree.
    tree->viewport()->setAttribute(Qt::WA_Hover, false);
    QEvent leave(QEvent::Leave);
    QCoreApplication::sendEvent(tree->viewport(), &leave);
    freezeRadioStatusUptime(ctx, d);
    return d;
}

// The page at `index` on the tab `path` (empty: its first tab), with the
// model posed as `board`. Hardware Config is told the posed radio, as a
// connect tells it (HardwarePage::onCurrentRadioChanged), so its tab strip
// is the board's.
SetupDialog* openSetupTabScene(SceneContext& ctx, int index, const QString& page, const QStringList& path,
                               HPSDRHW board)
{
    if (board != HPSDRHW::Saturn) {
        ctx.model->setBoardForTest(board);
    }
    SetupDialog* d = openSetupPageScene(ctx, index);
    if (d == nullptr) {
        return nullptr;
    }
    if (page == QLatin1String("Hardware Config")) {
        auto* hardware = d->findChild<HardwarePage*>();
        if (hardware == nullptr) {
            ctx.failure = QStringLiteral("no HardwarePage");
            return nullptr;
        }
        const RadioInfo radio = posedRadio(board);
        hardware->onCurrentRadioChanged(radio);
        pump(kFinalSettleMs);
        ctx.notes.append(QStringLiteral("board posed as %1 (RadioModel::setBoardForTest); Hardware Config "
                                        "told the radio %2 through HardwarePage::onCurrentRadioChanged; "
                                        "nothing connects")
                             .arg(board == HPSDRHW::HermesLite ? QStringLiteral("Hermes Lite 2")
                                                               : QStringLiteral("Saturn (ANAN-G2)"),
                                  radio.name));
    }
    if (!selectSetupTabs(ctx, d, path)) {
        return nullptr;
    }
    clearScrollBarHover(d);
    return d;
}

// Grabs each step of the page's (or tab's) own scroll area after the top,
// named <name>-1 ... <name>-N, each with a band naming its step.
void captureSetupScrollSteps(SceneContext& ctx, SetupDialog* d, const QString& name, const QString& where)
{
    // A table frame that scrolls sideways (SetupFit::SidewaysFrame) is
    // stepped across with the page, not counted as the page's own area.
    const auto isSidewaysFrame = [](const QScrollArea* a) {
        return a->property("nereusSetupScrollsSideways").toBool();
    };
    QVector<QScrollArea*> areas = overflowingAreas(d);
    QScrollArea* sidewaysFrame = nullptr;
    if (areas.size() > 1) {
        QVector<QScrollArea*> pages;
        for (QScrollArea* a : areas) {
            if (isSidewaysFrame(a)) {
                sidewaysFrame = a;
            } else {
                pages.append(a);
            }
        }
        areas = pages;
    }
    if (areas.size() != 1) {
        QStringList names;
        for (const QScrollArea* a : areas) {
            names << QStringLiteral("%1 \"%2\" in %3")
                         .arg(QString::fromLatin1(a->metaObject()->className()), a->objectName(),
                              QString::fromLatin1(a->parentWidget() != nullptr
                                                      ? a->parentWidget()->metaObject()->className()
                                                      : "none"));
        }
        ctx.failure = QStringLiteral("%1: %2 overflowing scroll areas, expected 1 (%3)")
                          .arg(where).arg(areas.size()).arg(names.join(QStringLiteral(", ")));
        ctx.nothingToScroll = areas.isEmpty();
        return;
    }
    QScrollArea* area = areas.first();
    QScrollBar* vertical = area->verticalScrollBar();
    QScrollBar* horizontal = area->horizontalScrollBar();
    // A table too wide for the window scrolls sideways inside its own frame
    // (SetupFit::SidewaysFrame), inside the page or beside its scroll
    // area, while the page scrolls down: step the frame across as the
    // page would be.
    if (!barOverflows(horizontal)) {
        for (QScrollArea* frame : d->findChildren<QScrollArea*>()) {
            if (frame != area && frame->isVisible() && area->isAncestorOf(frame) && isSidewaysFrame(frame)
                && barOverflows(frame->horizontalScrollBar())) {
                sidewaysFrame = frame;
            }
        }
        if (sidewaysFrame != nullptr) {
            horizontal = sidewaysFrame->horizontalScrollBar();
        }
    }
    // Every position the page can show, row by row: the top row across,
    // then each step down across. A page that scrolls one way only steps
    // that way; (0, 0) is the page scene itself.
    QVector<int> downs{0};
    QVector<int> rights{0};
    if (barOverflows(vertical)) {
        downs += scrollSteps(vertical);
    }
    if (barOverflows(horizontal)) {
        rights += scrollSteps(horizontal);
    }
    QVector<QPair<int, int>> steps;
    for (int down : downs) {
        for (int right : rights) {
            if (down != 0 || right != 0) {
                steps.append({down, right});
            }
        }
    }
    auto offsetText = [](const QString& axis, const QScrollBar* bar) {
        return QStringLiteral("%1 %2 of %3 px").arg(axis).arg(bar->value()).arg(bar->maximum());
    };
    for (int i = 0; i < steps.size(); ++i) {
        vertical->setValue(steps.at(i).first);
        horizontal->setValue(steps.at(i).second);
        clearScrollBarHover(d);
        captureDialog(ctx, d, QStringLiteral("%1-%2").arg(name).arg(i + 1));
        if (!ctx.failure.isEmpty()) {
            return;
        }
        QStringList offsets;
        if (barOverflows(vertical)) {
            offsets << offsetText(QStringLiteral("down"), vertical);
        }
        if (barOverflows(horizontal)) {
            offsets << offsetText(QStringLiteral("right"), horizontal);
        }
        const QString caption = QStringLiteral("%1: step %2 of %3, scrolled %4")
                                    .arg(where).arg(i + 1).arg(steps.size())
                                    .arg(offsets.join(QStringLiteral(", ")));
        CapturedImage& img = ctx.images.last();
        img.image = withStepCaption(img.image, caption);
        img.extra[QStringLiteral("scroll_step")] = QJsonObject{
            {QStringLiteral("step"), i + 1},
            {QStringLiteral("steps"), static_cast<int>(steps.size())},
            {QStringLiteral("down"), vertical->value()},
            {QStringLiteral("down_maximum"), barOverflows(vertical) ? vertical->maximum() : 0},
            {QStringLiteral("down_page_step"), vertical->pageStep()},
            {QStringLiteral("right"), horizontal->value()},
            {QStringLiteral("right_maximum"), barOverflows(horizontal) ? horizontal->maximum() : 0},
            {QStringLiteral("right_page_step"), horizontal->pageStep()},
            {QStringLiteral("overlap_px"), kScrollOverlapPx},
            {QStringLiteral("scroll_content_class"),
             QString::fromLatin1(area->widget() != nullptr ? area->widget()->metaObject()->className() : "")},
            {QStringLiteral("caption"), caption},
            {QStringLiteral("caption_band_px"), kStepCaptionPx}};
    }
    ctx.notes.append(QStringLiteral("%1: %2 scroll steps after the top, through the page's own scroll area")
                         .arg(where).arg(steps.size()));
}

// Grows the dialog down by what its page still scrolls, until nothing
// scrolls down. Returns the size it started at, or an empty size (with
// ctx.failure set) when it could not.
QSize growSetupToFit(SceneContext& ctx, SetupDialog* d, const QString& where)
{
    const QSize start = d->size();
    for (int i = 0; i < kSetupGrowSteps; ++i) {
        int more = 0;
        for (QScrollArea* area : overflowingAreas(d)) {
            if (barOverflows(area->verticalScrollBar())) {
                more = std::max(more, area->verticalScrollBar()->maximum());
            }
        }
        if (more == 0) {
            if (d->size() == start) {
                ctx.failure = QStringLiteral("%1: nothing scrolls at %2 x %3, so the page scene shows it whole")
                                  .arg(where).arg(start.width()).arg(start.height());
                ctx.nothingToScroll = true;
                return {};
            }
            ctx.notes.append(QStringLiteral("%1: the dialog grown from %2 x %3 to %4 x %5, where nothing scrolls")
                                 .arg(where).arg(start.width()).arg(start.height())
                                 .arg(d->width()).arg(d->height()));
            return start;
        }
        const QSize want(d->width(), d->height() + more);
        d->resize(want);
        pump(kFinalSettleMs);
        if (d->size() != want) {
            ctx.failure = QStringLiteral("%1: the dialog is %2 x %3, not %4 x %5")
                              .arg(where).arg(d->width()).arg(d->height())
                              .arg(want.width()).arg(want.height());
            return {};
        }
    }
    ctx.failure = QStringLiteral("%1: still scrolls after %2 growths").arg(where).arg(kSetupGrowSteps);
    return {};
}

} // namespace

QVector<Scene> setupPageScenes(MainWindow* window)
{
    QVector<Scene> scenes;
    SetupDialog* dialog = openSetupDialog(window);
    if (dialog == nullptr) {
        return scenes;
    }
    const QVector<SetupLeaf> leaves = setupLeaves(dialog);
    QHash<QString, int> labelCounts;
    for (const SetupLeaf& leaf : leaves) {
        labelCounts[leaf.label] += 1;
    }
    for (const SetupLeaf& leaf : leaves) {
        const QString stem = labelCounts.value(leaf.label) > 1
            ? slug(leaf.category) + QLatin1Char('-') + slug(leaf.label)
            : slug(leaf.label);
        const QString name = QStringLiteral("dialog-setup-") + stem;
        const int index = leaf.index;
        const bool hidden = leaf.hidden;
        const QString label = leaf.label;
        const QString category = leaf.category;
        scenes.append({name, QStringLiteral("2v"), true,
                       [name, index, hidden, label, category](SceneContext& ctx) {
            // Hardware Config's tab set is the board's: pose the board
            // (a Saturn, as the tab scenes do) rather than take whatever
            // tab set the page was last built with.
            SetupDialog* d = label == QLatin1String("Hardware Config")
                ? openSetupTabScene(ctx, index, label, {}, HPSDRHW::Saturn)
                : openSetupPageScene(ctx, index);
            if (d == nullptr) {
                return;
            }
            ctx.notes.append(QStringLiteral("Setup page %1 / %2 (registry index %3)%4")
                                 .arg(category, label).arg(index)
                                 .arg(hidden ? QStringLiteral(", hidden in the tree") : QString()));
            captureDialog(ctx, d, name);
        }});
    }
    // Each page's later tabs, then the pages and tabs that scroll
    // (inventory 1.4 and 5.3).
    QHash<QString, SetupLeaf> leafByStem;
    for (const SetupLeaf& leaf : leaves) {
        leafByStem.insert(labelCounts.value(leaf.label) > 1
                              ? slug(leaf.category) + QLatin1Char('-') + slug(leaf.label)
                              : slug(leaf.label),
                          leaf);
    }
    auto where = [](const SetupLeaf& leaf, const QStringList& path) {
        return (QStringList{QStringLiteral("Setup"), leaf.category, leaf.label} + path)
            .join(QStringLiteral(" > "));
    };
    for (const SetupTabSpec& spec : setupTabSpecs()) {
        if (!leafByStem.contains(spec.page)) {
            continue;
        }
        const SetupLeaf leaf = leafByStem.value(spec.page);
        const QString name = QStringLiteral("dialog-setup-") + spec.page + setupTabStem(spec.path);
        const QString title = where(leaf, spec.path);
        scenes.append({name, QStringLiteral("2v"), true, [name, leaf, spec, title](SceneContext& ctx) {
            SetupDialog* d = openSetupTabScene(ctx, leaf.index, leaf.label, spec.path, spec.board);
            if (d == nullptr) {
                return;
            }
            ctx.notes.append(QStringLiteral("%1 (registry index %2)").arg(title).arg(leaf.index));
            captureDialog(ctx, d, name);
        }});
    }
    for (const SetupScrollSpec& spec : setupScrollSpecs()) {
        if (!leafByStem.contains(spec.page)) {
            continue;
        }
        const SetupLeaf leaf = leafByStem.value(spec.page);
        const QString name = QStringLiteral("dialog-setup-") + spec.page + setupTabStem(spec.path)
            + QStringLiteral("--scrolled");
        const QString title = where(leaf, spec.path);
        scenes.append({name, QStringLiteral("2v"), true, [name, leaf, spec, title](SceneContext& ctx) {
            SetupDialog* d = openSetupTabScene(ctx, leaf.index, leaf.label, spec.path, spec.board);
            if (d == nullptr) {
                return;
            }
            ctx.notes.append(QStringLiteral("%1 (registry index %2)").arg(title).arg(leaf.index));
            captureSetupScrollSteps(ctx, d, name, title);
        }});
    }
    for (const SetupSizeSpec& spec : setupSizeSpecs()) {
        if (!leafByStem.contains(spec.page)) {
            continue;
        }
        const SetupLeaf leaf = leafByStem.value(spec.page);
        const QString name = QStringLiteral("dialog-setup-") + spec.page + setupTabStem(spec.path) + spec.suffix;
        const QString title = where(leaf, spec.path);
        scenes.append({name, QStringLiteral("2v"), true, [name, leaf, spec, title](SceneContext& ctx) {
            SetupDialog* d = openSetupTabScene(ctx, leaf.index, leaf.label, spec.path, spec.board);
            if (d == nullptr) {
                return;
            }
            d->resize(spec.size);
            pump(kFinalSettleMs);
            clearScrollBarHover(d);
            if (d->size() != spec.size) {
                ctx.failure = QStringLiteral("%1: the dialog is %2 x %3, not %4 x %5")
                                  .arg(title).arg(d->width()).arg(d->height())
                                  .arg(spec.size.width()).arg(spec.size.height());
                return;
            }
            ctx.notes.append(QStringLiteral("%1 (registry index %2) at %3 x %4")
                                 .arg(title).arg(leaf.index).arg(spec.size.width()).arg(spec.size.height()));
            captureDialog(ctx, d, name);
        }});
    }
    // The manual's figures: pages shown whole, and TX Profile with an
    // active profile.
    for (const SetupFullSpec& spec : setupFullSpecs()) {
        if (!leafByStem.contains(spec.page)) {
            continue;
        }
        const SetupLeaf leaf = leafByStem.value(spec.page);
        const QString name = QStringLiteral("dialog-setup-") + spec.name;
        const QString title = where(leaf, {});
        scenes.append({name, QStringLiteral("2v"), true, [name, leaf, title](SceneContext& ctx) {
            SetupDialog* d = openSetupPageScene(ctx, leaf.index);
            if (d == nullptr) {
                return;
            }
            const QSize start = growSetupToFit(ctx, d, title);
            if (!ctx.failure.isEmpty()) {
                return;
            }
            clearScrollBarHover(d);
            ctx.notes.append(QStringLiteral("%1 (registry index %2)").arg(title).arg(leaf.index));
            captureDialog(ctx, d, name);
            if (ctx.failure.isEmpty()) {
                ctx.images.last().extra[QStringLiteral("grown_to_fit")] = QJsonObject{
                    {QStringLiteral("from"), QJsonArray{start.width(), start.height()}},
                    {QStringLiteral("to"), QJsonArray{d->width(), d->height()}}};
            }
        }});
    }
    if (leafByStem.contains(QStringLiteral("tx-profile"))) {
        const SetupLeaf leaf = leafByStem.value(QStringLiteral("tx-profile"));
        const QString name = QStringLiteral("dialog-setup-tx-profile-active");
        const QString title = where(leaf, {});
        scenes.append({name, QStringLiteral("2v"), true, [name, leaf, title](SceneContext& ctx) {
            // The profiles a first connect writes, "Default" active, in this
            // process's throwaway test profile (seedTxProfiles).
            const QJsonObject seed = seedTxProfiles(ctx);
            if (seed.isEmpty()) {
                ctx.failure = QStringLiteral("%1: the TX profiles could not be seeded").arg(title);
                return;
            }
            SetupDialog* d = openSetupPageScene(ctx, leaf.index);
            if (d == nullptr) {
                return;
            }
            ctx.notes.append(QStringLiteral("%1 (registry index %2), active profile \"%3\"")
                                 .arg(title).arg(leaf.index)
                                 .arg(seed.value(QStringLiteral("active")).toString()));
            captureDialog(ctx, d, name);
            if (ctx.failure.isEmpty()) {
                ctx.images.last().extra[QStringLiteral("tx_profile_seed")] = seed;
            }
        }});
    }
    dialog->close();
    return scenes;
}

} // namespace NereusSDR::Preview
