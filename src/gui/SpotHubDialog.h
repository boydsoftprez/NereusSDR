// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - SpotHub dialog: 9-tab strip aggregating six spot-ingest
// clients (Cluster / RBN / WSJT-X / SpotCollector / POTA / FreeDV /
// PSK Reporter), the cross-source Spot List table, and the Display
// preferences page.
//
// Ported from AetherSDR src/gui/DxClusterDialog.{h,cpp} [@0cd4559]
// (shell only; per-tab content lands in Phase 3J-2 Tasks F2-F4).
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task F1. Initial shell
//                                    port. Constructor wires the same
//                                    six client pointers AetherSDR
//                                    upstream takes, but the trailing
//                                    `RadioModel* radioModel` argument
//                                    is replaced with NereusSDR's
//                                    `SpotModel* spots` (the
//                                    TCI-keyed spot sink from Phase
//                                    3J-2 Task D1); routing of
//                                    tuneRequested() into RadioModel
//                                    happens in MainWindow when the
//                                    dialog is instantiated (Phase F
//                                    wire-up). Each build*Tab() is a
//                                    stub that adds a placeholder
//                                    QLabel; per-source tab content
//                                    lands in F2 (Cluster / RBN /
//                                    WSJT-X / SpotCollector / POTA /
//                                    FreeDV / PSK Reporter), F3 (Spot
//                                    List), and F4 (Display).
//                                    Replaces upstream's
//                                    HAVE_WEBSOCKETS-gated FreeDvClient
//                                    with the always-built
//                                    FreeDVReporterClient (NereusSDR
//                                    Task B5) - same Engine.IO /
//                                    Socket.IO contract, just no
//                                    compile-time gating. Adds a PSK
//                                    Reporter tab between FreeDV and
//                                    Spot List (NereusSDR has the
//                                    PskReporterClient from Phase
//                                    3J-2 Task B6; AetherSDR upstream
//                                    has no PSK Reporter tab).
//                                    AI tooling: Anthropic Claude
//                                    Code.
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task F2. Per-source
//                                    tab content (uniform template):
//                                    connection-control grid +
//                                    auto-start toggle + start/stop
//                                    button + status label +
//                                    raw-event console. Cluster /
//                                    RBN / WSJT-X / SpotCollector /
//                                    POTA / FreeDV port verbatim
//                                    from AetherSDR
//                                    `DxClusterDialog.cpp:637-1596
//                                    [@0cd4559]`; PSK Reporter is
//                                    NereusSDR-native (no upstream)
//                                    and uses the same uniform shape
//                                    with `pskCallEdit` /
//                                    `pskGridEdit` identity inputs.
//                                    AppSettings keys preserved
//                                    verbatim except for the
//                                    NereusSDR-only `PskReporter*`
//                                    family. Member declarations
//                                    moved from `DxClusterDialog`
//                                    private section into
//                                    `SpotHubDialog` private section
//                                    in one consolidated block; each
//                                    sub-group is delimited by the
//                                    same source-tab comment. AI
//                                    tooling: Anthropic Claude Code.
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task F3. Spot List
//                                    tab content. Port of AetherSDR
//                                    `DxClusterDialog.cpp:1599-1717
//                                    [@0cd4559]`: QTableView bound
//                                    to BandFilterProxy(SpotTableModel)
//                                    + band filter row + bottom
//                                    spot-count + Clear button +
//                                    double-click to emit
//                                    tuneRequested(double).
//                                    Three NereusSDR divergences:
//                                    (1) band filters become
//                                    QPushButton "pills" (checkable)
//                                    instead of upstream QCheckBoxes
//                                    to match the SpotHub dialog's
//                                    pill aesthetic; (2) band list
//                                    extends from upstream's 11
//                                    (160m..6m) to 12 (160m..2m) so
//                                    every band the SpotTableModel
//                                    can produce gets a control;
//                                    (3) adds a second row of 7
//                                    source-filter pills (DX / RBN /
//                                    JT / COL / POT / FDR / PSK) on
//                                    top of the band-filter row,
//                                    driving the new
//                                    BandFilterProxy::setSourceVisible.
//                                    Three new member pointers
//                                    (m_spotTableModel,
//                                    m_spotProxyModel, m_spotTable)
//                                    join the existing per-source
//                                    block. objectName() keys
//                                    pinned for the smoke-test
//                                    harness. AI tooling: Anthropic
//                                    Claude Code.
//   2026-05-11  J.J. Boyd / KG4VCF  Post-3J-2 UX fix. Adds a
//                                    Settings tab at index 0 as the
//                                    single source of operator
//                                    identity (callsign + grid +
//                                    FreeDV status message). Save
//                                    button writes to canonical
//                                    User/Callsign + User/GridSquare
//                                    + FreeDvReporter/Message keys
//                                    and propagates to the per-source
//                                    legacy keys
//                                    (DxClusterCallsign / RbnCallsign
//                                    / PskReporterCallsign /
//                                    FreeDvReporter/Callsign /
//                                    FreeDvReporter/GridSquare).
//                                    Per-source tabs (Cluster / RBN /
//                                    PSK Reporter) now fall back to
//                                    User/Callsign when their
//                                    per-source key is empty. FreeDV
//                                    tab gains an identity-status
//                                    label (green = set, yellow = no
//                                    identity). Closes the
//                                    user-reported "FreeDV Reporter
//                                    not connecting" bug: the auto-
//                                    start used to fire with an empty
//                                    callsign, producing anonymous
//                                    connection attempts. AI tooling:
//                                    Anthropic Claude Code.
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task F4. Display tab
//                                    content. Folds AetherSDR's
//                                    standalone
//                                    `src/gui/SpotSettingsDialog.{h,cpp}
//                                    [@0cd4559]` into the Display
//                                    tab of SpotHubDialog (the
//                                    upstream standalone dialog is
//                                    retired in favour of one
//                                    consolidated control surface).
//                                    Two-column layout: LEFT 8 stat
//                                    blocks (Total Spots / Unique
//                                    Callsigns / Active Sources /
//                                    cty.dat entries / ADIF QSOs /
//                                    DXCC entities / New DXCC / New
//                                    bands) plus a red "Clear All
//                                    Spots" button at the bottom;
//                                    RIGHT all knobs from upstream
//                                    SpotSettingsDialog (Spots and
//                                    Memories toggles + Levels /
//                                    Position / Font Size / Spot
//                                    Lifetime sliders + Override
//                                    Colors toggle + color swatch +
//                                    Override Background two toggles
//                                    + color swatch + Background
//                                    Opacity slider). Every knob
//                                    change writes to AppSettings
//                                    (verbatim key names from
//                                    upstream: IsSpotsEnabled,
//                                    SpotsMaxLevel,
//                                    SpotsStartingHeightPercentage,
//                                    SpotFontSize,
//                                    DxClusterSpotLifetimeSec,
//                                    IsSpotsOverrideColorsEnabled,
//                                    IsSpotsOverrideBackgroundColors
//                                    Enabled,
//                                    IsSpotsOverrideToAutoBackground
//                                    ColorEnabled,
//                                    SpotsOverrideColor,
//                                    SpotsOverrideBgColor,
//                                    SpotsBackgroundOpacity,
//                                    IsMemorySpotsEnabled) and
//                                    emits settingsChanged() so
//                                    MainWindow can refresh the
//                                    spectrum spot overlay live.
//                                    The Clear All Spots button
//                                    calls SpotModel::clear() and
//                                    emits spotsClearedAll(). NereusSDR
//                                    divergence from upstream: stat
//                                    blocks LHS replaces upstream's
//                                    single "Total Spots" label
//                                    (upstream `:272-276`); the
//                                    additional Unique Callsigns /
//                                    Active Sources / cty.dat /
//                                    ADIF / DXCC entities / NewDxcc
//                                    / NewBands labels read live
//                                    from the SpotTableModel +
//                                    DxccColorProvider (Tasks D2 +
//                                    C4) and are NereusSDR-native.
//                                    objectName() keys pinned for
//                                    the smoke-test harness. AI
//                                    tooling: Anthropic Claude Code.

#pragma once

#include <QDialog>

class QTabWidget;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QLabel;
class QCheckBox;
class QPlainTextEdit;
class QTableView;

namespace NereusSDR {

class DxClusterClient;
class WsjtxClient;
class SpotCollectorClient;
class PotaClient;
class FreeDVReporterClient;
class PskReporterClient;
class SpotModel;
class SpotTableModel;
class BandFilterProxy;
class DxccColorProvider;

// From AetherSDR src/gui/DxClusterDialog.h:79-215 [@0cd4559]
//
// SpotHubDialog: top-level QDialog containing a QTabWidget with
// nine tabs in upstream order (Cluster / RBN / WSJT-X /
// SpotCollector / POTA / FreeDV / PSK Reporter / Spot List /
// Display). The dialog is the modeless control surface for the
// six spot-ingest clients; each source has its own tab with
// connect/disconnect controls, a status LED, and a per-source
// console. The Spot List tab shows the merged 8-column table
// across all sources (NereusSDR SpotTableModel + BandFilterProxy
// from Phase 3J-2 Task D2). The Display tab holds DXCC
// color-coding configuration and global spot rendering toggles.
//
// F1 ships the SHELL only. Each `build<Source>Tab()` adds a
// placeholder QLabel. F2 (per-source tabs), F3 (Spot List),
// and F4 (Display) build the content.
class SpotHubDialog : public QDialog {
    Q_OBJECT

public:
    explicit SpotHubDialog(DxClusterClient* clusterClient,
                           DxClusterClient* rbnClient,
                           WsjtxClient* wsjtxClient,
                           SpotCollectorClient* spotCollectorClient,
                           PotaClient* potaClient,
                           FreeDVReporterClient* freedvClient,
                           PskReporterClient* pskClient,
                           SpotModel* spotModel,
                           SpotTableModel* spotTableModel,
                           DxccColorProvider* dxccProvider,
                           QWidget* parent = nullptr);

    // 2026-05-12 bench fix (Gap #6).  Spot List ↔ panadapter hover
    // sync.  setHoveredPanadapterSpot(spotIdx) is called from
    // MainWindow when the user mouses over a spot label on the
    // panadapter — finds the row in the Spot List matching that
    // SpotModel::SpotData::index (by callsign + freq + source) and
    // selects it.  -1 clears.  Companion signal
    // spotListHoverChanged(spotIdx) lives in the signals block below.
public slots:
    void setHoveredPanadapterSpot(int spotIdx);

public:

signals:
    // 2026-05-12 bench fix (Gap #6 companion).  Fired when the user
    // mouses over a Spot List row.  spotIdx is the matching
    // SpotModel::SpotData::index (resolved by callsign+freq+source);
    // -1 means "no row under cursor."  MainWindow forwards this to
    // SpectrumWidget::setHoverSpotIndexExternal for the halo paint.
    void spotListHoverChanged(int spotIdx);

    // Forwarded from per-source tab controls in F2.
    void settingsChanged();
    void connectRequested(const QString& host, quint16 port, const QString& callsign);
    void disconnectRequested();
    void rbnConnectRequested(const QString& host, quint16 port, const QString& callsign);
    void rbnDisconnectRequested();
    void wsjtxStartRequested(const QString& address, quint16 port);
    void wsjtxStopRequested();
    void spotCollectorStartRequested(quint16 port);
    void spotCollectorStopRequested();
    void potaStartRequested(int intervalSec);
    void potaStopRequested();
    void freedvStartRequested();
    void freedvStopRequested();
    // 2026-05-12 bench fix (review P1 / P2 — PR #238): carry the
    // freshly-validated callsign + grid from the PSK tab so the
    // MainWindow handler can apply them to the live PskReporterClient
    // via setIdentity() BEFORE arming the auto-send timer.  Previously
    // pskStartRequested was no-arg and RadioModel held a stale
    // identity (the one set at construction from PskReporter/* slash
    // keys), so a user who only filled in the PSK tab and clicked
    // Start would emit IPFIX datagrams with empty receiver fields.
    void pskStartRequested(const QString& callsign, const QString& gridSquare);
    void pskStopRequested();
    // Forwarded from the Spot List click-to-tune + the spot overlay.
    void tuneRequested(double freqMhz);
    // Forwarded from the Display tab's "Clear All Spots" button.
    void spotsClearedAll();

    // 2026-05-12 bench fix: emitted after the Settings tab's Save &
    // Propagate button writes User/Callsign / User/GridSquare /
    // FreeDvReporter/Message to AppSettings.  MainWindow listens so
    // it can push the new grid into FreeDVStationModel::setOurGridSquare
    // — otherwise the dialog's distance/heading columns stay zeroed
    // because the station model's m_ourGrid never updates without
    // an explicit setter call.
    void identitySaved(const QString& callsign,
                       const QString& gridSquare,
                       const QString& message);

private:
    // NereusSDR-native Settings tab (first position) for central
    // operator identity. Post-3J-2 UX fix.
    void buildSettingsTab(QTabWidget* tabs);

    // Shell-only stubs in F1. F2-F4 fill these out.
    void buildClusterTab(QTabWidget* tabs);
    void buildRbnTab(QTabWidget* tabs);
    void buildWsjtxTab(QTabWidget* tabs);
    void buildSpotCollectorTab(QTabWidget* tabs);
    void buildPotaTab(QTabWidget* tabs);
    void buildFreeDvTab(QTabWidget* tabs);
    void buildPskTab(QTabWidget* tabs);
    void buildSpotListTab(QTabWidget* tabs);
    void buildDisplayTab(QTabWidget* tabs);

    // Held by pointer; ownership stays with the caller (RadioModel /
    // MainWindow in production, the test fixture in unit tests).
    DxClusterClient*      m_clusterClient{nullptr};
    DxClusterClient*      m_rbnClient{nullptr};
    WsjtxClient*          m_wsjtxClient{nullptr};
    SpotCollectorClient*  m_spotCollectorClient{nullptr};
    PotaClient*           m_potaClient{nullptr};
    FreeDVReporterClient* m_freedvClient{nullptr};
    PskReporterClient*    m_pskClient{nullptr};
    SpotModel*            m_spotModel{nullptr};
    SpotTableModel*       m_spotTableModel{nullptr};
    DxccColorProvider*    m_dxccProvider{nullptr};

    // Settings tab (NereusSDR-native, post-3J-2 UX fix). Held so the
    // Save button slot can re-read the QLineEdit text and write to
    // AppSettings + propagate to live FreeDVReporterClient +
    // PskReporterClient identity. The error label surfaces validation
    // failures (empty callsign / invalid Maidenhead grid); the saved
    // label flashes "Saved" briefly after a successful Save.
    QLineEdit*      m_settingsCallEdit{nullptr};
    QLineEdit*      m_settingsGridEdit{nullptr};
    QLineEdit*      m_settingsFreedvMsgEdit{nullptr};
    QPushButton*    m_settingsSaveBtn{nullptr};
    QLabel*         m_settingsErrorLabel{nullptr};
    QLabel*         m_settingsSavedLabel{nullptr};
    QLabel*         m_settingsCurrentLabel{nullptr};

    // FreeDV tab identity-status label (NereusSDR-native, post-3J-2).
    // Green when identity is configured; yellow warning when missing
    // and the user needs to visit the Settings tab.
    QLabel*         m_freedvIdentityLabel{nullptr};

    // From AetherSDR src/gui/DxClusterDialog.h:141-199 [@0cd4559].
    // Per-source member pointers needed by F2 tab builders. Each
    // sub-block matches one tab; objectName() values are set in the
    // .cpp to make the widgets discoverable in the smoke tests.

    // Cluster tab
    QLineEdit*      m_hostEdit{nullptr};
    QSpinBox*       m_portSpin{nullptr};
    QLineEdit*      m_callEdit{nullptr};
    QPushButton*    m_connectBtn{nullptr};
    QPushButton*    m_autoConnectBtn{nullptr};
    QLabel*         m_statusLabel{nullptr};
    QPlainTextEdit* m_console{nullptr};
    QLineEdit*      m_cmdEdit{nullptr};
    QPushButton*    m_sendBtn{nullptr};

    // RBN tab
    QLineEdit*      m_rbnHostEdit{nullptr};
    QSpinBox*       m_rbnPortSpin{nullptr};
    QLineEdit*      m_rbnCallEdit{nullptr};
    QPushButton*    m_rbnConnectBtn{nullptr};
    QPushButton*    m_rbnAutoConnectBtn{nullptr};
    QLabel*         m_rbnStatusLabel{nullptr};
    QPlainTextEdit* m_rbnConsole{nullptr};
    QLineEdit*      m_rbnCmdEdit{nullptr};
    QPushButton*    m_rbnSendBtn{nullptr};

    // WSJT-X tab
    QLineEdit*      m_wsjtxAddrEdit{nullptr};
    QSpinBox*       m_wsjtxPortSpin{nullptr};
    QPushButton*    m_wsjtxStartBtn{nullptr};
    QPushButton*    m_wsjtxAutoStartBtn{nullptr};
    QLabel*         m_wsjtxStatusLabel{nullptr};
    QPlainTextEdit* m_wsjtxConsole{nullptr};
    QCheckBox*      m_wsjtxFilterCQ{nullptr};
    QCheckBox*      m_wsjtxFilterPOTA{nullptr};
    QCheckBox*      m_wsjtxFilterCallingMe{nullptr};
    QPushButton*    m_wsjtxColorCQ{nullptr};
    QPushButton*    m_wsjtxColorPOTA{nullptr};
    QPushButton*    m_wsjtxColorCallingMe{nullptr};
    QPushButton*    m_wsjtxColorDefault{nullptr};

    // SpotCollector tab
    QSpinBox*       m_scPortSpin{nullptr};
    QPushButton*    m_scStartBtn{nullptr};
    QPushButton*    m_scAutoStartBtn{nullptr};
    QLabel*         m_scStatusLabel{nullptr};
    QPlainTextEdit* m_scConsole{nullptr};

    // POTA tab
    QSpinBox*       m_potaIntervalSpin{nullptr};
    QPushButton*    m_potaStartBtn{nullptr};
    QPushButton*    m_potaAutoStartBtn{nullptr};
    QLabel*         m_potaStatusLabel{nullptr};
    QPlainTextEdit* m_potaConsole{nullptr};

    // FreeDV tab
    QPushButton*    m_freedvStartBtn{nullptr};
    QPushButton*    m_freedvAutoStartBtn{nullptr};
    QLabel*         m_freedvStatusLabel{nullptr};
    QPlainTextEdit* m_freedvConsole{nullptr};

    // PSK Reporter tab (NereusSDR-native, no upstream).
    QLineEdit*      m_pskCallEdit{nullptr};
    QLineEdit*      m_pskGridEdit{nullptr};
    QPushButton*    m_pskStartBtn{nullptr};
    QPushButton*    m_pskAutoStartBtn{nullptr};
    QLabel*         m_pskStatusLabel{nullptr};
    QPlainTextEdit* m_pskConsole{nullptr};

    // Spot List tab (F3). From AetherSDR DxClusterDialog.h:200-209
    // [@0cd4559] but the underlying types come from NereusSDR's
    // standalone src/models/ port (Task D2) and the SpotTableModel
    // backs all sources rather than just the Cluster tab.
    //
    // 2026-05-12 bench fix: m_spotTableModel moved to the section
    // above (declared next to m_spotModel) — its ownership now lives
    // on RadioModel so spots populate from app start.  Keeping only
    // the dialog-owned proxy + view here.
    BandFilterProxy* m_spotProxyModel{nullptr};
    QTableView*     m_spotTable{nullptr};

    // Display tab (F4). LEFT-column stat blocks are NereusSDR-native
    // additions; RIGHT-column knobs port verbatim from AetherSDR
    // src/gui/SpotSettingsDialog.{h,cpp} [@0cd4559]. The stat label
    // pointers are held so the dialog can refresh them when the
    // table model + DxccColorProvider emit count-change signals.
    QLabel*      m_statTotalSpots{nullptr};
    QLabel*      m_statUniqueCallsigns{nullptr};
    QLabel*      m_statActiveSources{nullptr};
    QLabel*      m_statCtyDatEntries{nullptr};
    QLabel*      m_statAdifQsos{nullptr};
    QLabel*      m_statDxccEntities{nullptr};
    QLabel*      m_statNewDxcc{nullptr};
    QLabel*      m_statNewBands{nullptr};
};

} // namespace NereusSDR
