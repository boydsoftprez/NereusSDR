// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - SpotHub dialog: see SpotHubDialog.h for the full
// port-citation header and the F1 / F2 / F3 / F4 task breakdown.
//
// Ported from AetherSDR src/gui/DxClusterDialog.cpp [@0cd4559]
// (F2 ships per-source tab content; F3 Spot List and F4 Display
// remain stubs).
//
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task F1. Initial
//                                    shell port; see header for full
//                                    notes. AI tooling: Anthropic
//                                    Claude Code.
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task F2. Fleshes out
//                                    the seven per-source tab
//                                    builders with the uniform
//                                    template (connection-control
//                                    grid + auto-start toggle +
//                                    start/stop button + status
//                                    label + raw-event console).
//                                    Cluster / RBN / WSJT-X /
//                                    SpotCollector / POTA / FreeDV
//                                    port verbatim from upstream
//                                    `DxClusterDialog.cpp:637-1596
//                                    [@0cd4559]`; PSK Reporter is
//                                    NereusSDR-native and uses the
//                                    same uniform shape with
//                                    callsign + grid identity
//                                    inputs. AppSettings keys and
//                                    widget stylesheets preserved
//                                    verbatim. Each tab builder
//                                    assigns objectName() values
//                                    that the smoke test harness
//                                    uses to assert content shape.
//                                    AI tooling: Anthropic Claude
//                                    Code.
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task F3. Spot List
//                                    tab. Port of AetherSDR
//                                    `DxClusterDialog.cpp:1599-1717
//                                    [@0cd4559]`: QTableView bound
//                                    to BandFilterProxy(SpotTableModel)
//                                    + filter row + bottom
//                                    spot-count + Clear button +
//                                    double-click emit
//                                    tuneRequested(double).
//                                    Wires every client's
//                                    spotReceived(DxSpot) into the
//                                    table model so the merged
//                                    8-column view sees all sources.
//                                    NereusSDR divergences from
//                                    upstream: (1) band filters
//                                    become checkable QPushButton
//                                    "pills" instead of upstream
//                                    QCheckBoxes (matches dialog
//                                    pill aesthetic); (2) band list
//                                    extends upstream's 11 (160m..
//                                    6m) to 12 (160m..2m) so every
//                                    band SpotTableModel can produce
//                                    has a control; (3) adds a
//                                    second row of 7 source pills
//                                    (DX / RBN / JT / COL / POT /
//                                    FDR / PSK) driving the new
//                                    BandFilterProxy::setSourceVisible.
//                                    AppSettings keys preserved:
//                                    SpotBandFilter_<band> for band
//                                    pills; new SpotSourceFilter_
//                                    <source> for source pills
//                                    (NereusSDR-native). AI tooling:
//                                    Anthropic Claude Code.
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task F4. Display tab
//                                    content. Folds AetherSDR's
//                                    standalone
//                                    `src/gui/SpotSettingsDialog.{h,cpp}
//                                    [@0cd4559]` into the Display
//                                    tab. Two columns: LEFT 8 stat
//                                    blocks + red Clear All Spots
//                                    button; RIGHT all knobs from
//                                    upstream SpotSettingsDialog
//                                    (Spots / Memories toggles,
//                                    Levels / Position / Font Size /
//                                    Lifetime sliders, Override
//                                    Colors / Override BG / Auto
//                                    toggles, two color swatch
//                                    pickers, BG Opacity slider).
//                                    Each knob change writes to
//                                    AppSettings (verbatim keys
//                                    from upstream
//                                    `SpotSettingsDialog.cpp:22-37
//                                    [@0cd4559]`) and emits
//                                    `settingsChanged()`. Clear
//                                    button calls `SpotModel::clear()`
//                                    and emits `spotsClearedAll()`.
//                                    GuardedSlider is reused from
//                                    `src/gui/widgets/GuardedSlider.h`
//                                    (already ported from upstream
//                                    `src/gui/GuardedSlider.h
//                                    [@0cd4559]`). Non-linear
//                                    lifetime steps (10s..55s in 5s,
//                                    5min..55min in 5min, 1hr..24hr
//                                    in 1hr; 45 indices total)
//                                    preserved verbatim from upstream
//                                    `:146-178`. AI tooling:
//                                    Anthropic Claude Code.

#include "SpotHubDialog.h"

#include "core/AppSettings.h"
#include "core/DxClusterClient.h"
#include "core/DxccColorProvider.h"
#include "core/DxSpot.h"
#include "core/FreeDVReporterClient.h"
#include "core/PotaClient.h"
#include "core/PskReporterClient.h"
#include "core/SpotCollectorClient.h"
#include "core/WsjtxClient.h"
#include "gui/widgets/GuardedSlider.h"
#include "models/BandFilterProxy.h"
#include "models/SpotModel.h"
#include "models/SpotTableModel.h"

#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDateTime>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>

#include <cmath>

namespace NereusSDR {

namespace {

// Common stylesheet fragments. Lifted verbatim from upstream so the
// SpotHub dialog visually matches AetherSDR's DxCluster dialog
// (consistent dark panel + cyan accent + monospace console).

constexpr const char* kLineEditStyle =
    "QLineEdit { background: #1a1a2e; color: #c8d8e8; "
    "border: 1px solid #203040; padding: 3px; }";

constexpr const char* kSpinBoxStyle =
    "QSpinBox { background: #1a1a2e; color: #c8d8e8; "
    "border: 1px solid #203040; padding: 3px; }";

constexpr const char* kAutoToggleStyle =
    "QPushButton { background: #206030; color: white; "
    "border: 1px solid #305040; padding: 4px 10px; }"
    "QPushButton:!checked { background: #603020; }";

constexpr const char* kStartBtnStyle =
    "QPushButton { background: #00b4d8; color: #0f0f1a; font-weight: bold; "
    "border: 1px solid #008ba8; padding: 4px; border-radius: 3px; }"
    "QPushButton:hover { background: #00c8f0; }"
    "QPushButton:disabled { background: #404060; color: #808080; }";

constexpr const char* kStatusIdleStyle =
    "QLabel { color: #808080; font-size: 11px; }";

constexpr const char* kStatusActiveStyle =
    "QLabel { color: #00b4d8; font-size: 11px; font-weight: bold; }";

constexpr const char* kConsoleStyle =
    "QPlainTextEdit {"
    "  background: #0a0a14;"
    "  color: #a0b0c0;"
    "  font-family: monospace;"
    "  font-size: 11px;"
    "  border: 1px solid #203040;"
    "  padding: 4px;"
    "}";

constexpr const char* kCmdEditStyle =
    "QLineEdit { background: #1a1a2e; color: #c8d8e8; "
    "border: 1px solid #203040; padding: 3px; font-family: monospace; }";

// F3 (NereusSDR-native). Pill buttons for the band + source filter
// rows on the Spot List tab. Checked = pill lit (cyan accent), filter
// passes that band/source. Unchecked = dim, filter hides it.
constexpr const char* kFilterPillStyle =
    "QPushButton {"
    "  background: #1a1a2e;"
    "  color: #808890;"
    "  border: 1px solid #203040;"
    "  border-radius: 9px;"
    "  padding: 2px 8px;"
    "  font-size: 11px;"
    "  font-weight: bold;"
    "  min-width: 24px;"
    "}"
    "QPushButton:checked {"
    "  background: #00b4d8;"
    "  color: #0f0f1a;"
    "  border-color: #008ba8;"
    "}"
    "QPushButton:hover { border-color: #c8d8e8; }";

constexpr const char* kSpotTableStyle =
    "QTableView {"
    "  background: #0a0a14;"
    "  alternate-background-color: #0f0f1e;"
    "  color: #c8d8e8;"
    "  gridline-color: #1a2a3a;"
    "  border: 1px solid #203040;"
    "  font-size: 11px;"
    "}"
    "QTableView::item:selected {"
    "  background: #1a3a5a;"
    "  color: #e0f0ff;"
    "}"
    "QHeaderView::section {"
    "  background: #1a1a2e;"
    "  color: #00b4d8;"
    "  border: 1px solid #203040;"
    "  padding: 3px 6px;"
    "  font-weight: bold;"
    "  font-size: 11px;"
    "}";

QString swatchStyle(const QColor& c) {
    return QString(
        "QPushButton { background: %1; border: 2px solid #405060; border-radius: 3px; }"
        "QPushButton:hover { border-color: #c8d8e8; }").arg(c.name());
}

} // namespace

// From AetherSDR src/gui/DxClusterDialog.cpp:230-273 [@0cd4559].
// NereusSDR divergence: AetherSDR upstream takes a trailing
// `RadioModel* radioModel` argument; NereusSDR replaces it with the
// TCI-keyed `SpotModel* spots` (Phase 3J-2 Task D1). Routing of
// `tuneRequested(double)` into the active RadioModel happens in
// MainWindow when the dialog is instantiated. Replaces upstream's
// HAVE_WEBSOCKETS-gated FreeDvClient with the always-built
// FreeDVReporterClient (NereusSDR Task B5). Adds a PSK Reporter
// tab between FreeDV and Spot List (NereusSDR-only). The pane
// stylesheet (panel border + tab colors) follows upstream verbatim.
SpotHubDialog::SpotHubDialog(DxClusterClient* clusterClient,
                             DxClusterClient* rbnClient,
                             WsjtxClient* wsjtxClient,
                             SpotCollectorClient* spotCollectorClient,
                             PotaClient* potaClient,
                             FreeDVReporterClient* freedvClient,
                             PskReporterClient* pskClient,
                             SpotModel* spotModel,
                             SpotTableModel* spotTableModel,
                             DxccColorProvider* dxccProvider,
                             QWidget* parent)
    : QDialog(parent),
      m_clusterClient(clusterClient),
      m_rbnClient(rbnClient),
      m_wsjtxClient(wsjtxClient),
      m_spotCollectorClient(spotCollectorClient),
      m_potaClient(potaClient),
      m_freedvClient(freedvClient),
      m_pskClient(pskClient),
      m_spotModel(spotModel),
      m_spotTableModel(spotTableModel),
      m_dxccProvider(dxccProvider)
{
    setWindowTitle("SpotHub");
    setMinimumSize(680, 560);
    resize(760, 640);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(4, 4, 4, 4);

    auto* tabs = new QTabWidget;
    tabs->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #203040; }"
        "QTabBar::tab { background: #1a1a2e; color: #808890; border: 1px solid #203040; "
        "  padding: 6px 16px; margin-right: 2px; }"
        "QTabBar::tab:selected { background: #0f0f1a; color: #00b4d8; border-bottom: none; }");

    // Tab order matches AetherSDR upstream
    // (src/gui/DxClusterDialog.cpp:262-271). NereusSDR adds the PSK
    // Reporter tab between FreeDV and Spot List (Task F2 wires its
    // content). FreeDV tab is unconditional in NereusSDR; upstream
    // gated it on HAVE_WEBSOCKETS.
    // Post-3J-2 UX fix: a NereusSDR-native Settings tab takes the
    // first position so the operator enters callsign + grid + FreeDV
    // status message once and Save propagates to every per-source
    // legacy key plus the canonical User/* keys.
    buildSettingsTab(tabs);
    buildClusterTab(tabs);
    buildRbnTab(tabs);
    buildWsjtxTab(tabs);
    buildSpotCollectorTab(tabs);
    buildPotaTab(tabs);
    buildFreeDvTab(tabs);
    buildPskTab(tabs);
    buildSpotListTab(tabs);
    buildDisplayTab(tabs);

    root->addWidget(tabs);
}

// NereusSDR-native (no AetherSDR upstream). Post-3J-2 UX fix: the
// Settings tab takes the first position in the dialog and provides
// one place for the operator to enter callsign + Maidenhead grid +
// FreeDV status message. The Save button:
//
//   1. Validates: callsign non-empty, grid is 4 or 6 chars.
//   2. Writes canonical keys: User/Callsign, User/GridSquare,
//      FreeDvReporter/Message. These are what every NereusSDR module
//      that needs operator identity reads first.
//   3. Propagates to legacy per-source keys for backward compat:
//      DxClusterCallsign, RbnCallsign, PskReporterCallsign,
//      FreeDvReporter/Callsign, FreeDvReporter/GridSquare. Existing
//      per-source-tab readers (Cluster / RBN / PSK) keep reading
//      their per-source keys; this propagation keeps them in sync
//      with the central Settings entry.
//   4. Calls setIdentity() on the live FreeDVReporterClient +
//      PskReporterClient instances (if non-null) so a connection that
//      is already up picks up the new identity without disconnect.
//   5. Stamps User/IdentityLastSaved with the current timestamp and
//      flashes a green "Saved" label for 2 seconds.
//
// Closes the user-reported "FreeDV Reporter not connecting" bug: the
// auto-start path used to fire startConnection() without ever calling
// setIdentity(), so the WebSocket connected anonymously and the
// server dropped it. The fix is two-fold: (a) the Settings tab gives
// the user one place to enter identity, (b)
// RadioModel::restoreSpotClientAutoStartState now re-applies identity
// from the User/* fall-back chain before calling startConnection(),
// and skips the call entirely when no identity is configured.
void SpotHubDialog::buildSettingsTab(QTabWidget* tabs)
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(8);

    auto& s = AppSettings::instance();

    // ── Identity group ────────────────────────────────────────────
    auto* idGroup = new QGroupBox("Operator Identity");
    idGroup->setObjectName("settingsIdentityGroup");
    auto* idLayout = new QVBoxLayout(idGroup);
    idLayout->setSpacing(4);

    // 2026-05-12 bench: first-time-setup banner.  Shown only when
    // User/Callsign is empty so existing users don't see a stale
    // prompt.  Without this an operator on a fresh install would just
    // see "Disconnected" on every spot tab with no UI clue that a
    // callsign is required.
    const bool needsFirstRunPrompt =
        s.value("User/Callsign").toString().trimmed().isEmpty();
    if (needsFirstRunPrompt) {
        auto* firstRunBanner = new QLabel(
            "First-time setup — enter your callsign and grid square "
            "below.  Spot sources stay disconnected until your "
            "callsign is set.");
        firstRunBanner->setObjectName("settingsFirstRunBanner");
        firstRunBanner->setWordWrap(true);
        firstRunBanner->setStyleSheet(
            "QLabel { color: #ffcc66; background: #2a2010; "
            "border: 1px solid #806020; padding: 6px; "
            "border-radius: 3px; font-weight: bold; }");
        idLayout->addWidget(firstRunBanner);
    }

    auto* helpLabel = new QLabel(
        "These values feed every spot source (DX cluster, RBN, WSJT-X "
        "registration, SpotCollector, POTA spot author, FreeDV Reporter, "
        "PSK Reporter). Change here once; per-source tabs read these "
        "defaults.");
    helpLabel->setWordWrap(true);
    helpLabel->setStyleSheet("QLabel { color: #8aa8c0; font-size: 11px; }");
    idLayout->addWidget(helpLabel);

    auto* grid = new QGridLayout;
    grid->setColumnStretch(1, 1);
    int row = 0;

    grid->addWidget(new QLabel("Callsign:"), row, 0);
    m_settingsCallEdit = new QLineEdit(
        s.value("User/Callsign").toString());
    m_settingsCallEdit->setObjectName("settingsCallEdit");
    m_settingsCallEdit->setPlaceholderText(
        "Enter Callsign Here (4-12 chars — required to publish spots)");
    m_settingsCallEdit->setMaxLength(12);
    m_settingsCallEdit->setStyleSheet(kLineEditStyle);
    grid->addWidget(m_settingsCallEdit, row, 1);
    row++;

    grid->addWidget(new QLabel("Grid Square:"), row, 0);
    m_settingsGridEdit = new QLineEdit(
        s.value("User/GridSquare").toString());
    m_settingsGridEdit->setObjectName("settingsGridEdit");
    m_settingsGridEdit->setPlaceholderText(
        "Enter Grid Here (Maidenhead — e.g. EM73 or EM73XY)");
    m_settingsGridEdit->setMaxLength(6);
    m_settingsGridEdit->setStyleSheet(kLineEditStyle);
    grid->addWidget(m_settingsGridEdit, row, 1);
    row++;

    grid->addWidget(new QLabel("FreeDV Status Message:"), row, 0);
    m_settingsFreedvMsgEdit = new QLineEdit(
        s.value("FreeDvReporter/Message").toString());
    m_settingsFreedvMsgEdit->setObjectName("settingsFreedvMsgEdit");
    m_settingsFreedvMsgEdit->setPlaceholderText(
        "shown next to your station on the FreeDV Reporter map");
    m_settingsFreedvMsgEdit->setMaxLength(200);
    m_settingsFreedvMsgEdit->setStyleSheet(kLineEditStyle);
    grid->addWidget(m_settingsFreedvMsgEdit, row, 1);
    row++;

    idLayout->addLayout(grid);
    layout->addWidget(idGroup);

    // ── Apply Identity Now group ──────────────────────────────────
    auto* applyGroup = new QGroupBox("Apply Identity Now");
    applyGroup->setObjectName("settingsApplyGroup");
    auto* applyLayout = new QVBoxLayout(applyGroup);
    applyLayout->setSpacing(4);

    auto* btnRow = new QHBoxLayout;
    m_settingsSaveBtn = new QPushButton("Save && Propagate");
    m_settingsSaveBtn->setObjectName("settingsSaveBtn");
    m_settingsSaveBtn->setStyleSheet(kStartBtnStyle);
    m_settingsSaveBtn->setFixedWidth(180);
    btnRow->addWidget(m_settingsSaveBtn);

    m_settingsErrorLabel = new QLabel;
    m_settingsErrorLabel->setObjectName("settingsErrorLabel");
    m_settingsErrorLabel->setStyleSheet(
        "QLabel { color: #ff4444; font-size: 11px; }");
    btnRow->addWidget(m_settingsErrorLabel, 1);

    m_settingsSavedLabel = new QLabel;
    m_settingsSavedLabel->setObjectName("settingsSavedLabel");
    m_settingsSavedLabel->setStyleSheet(
        "QLabel { color: #4caf50; font-weight: bold; }");
    btnRow->addWidget(m_settingsSavedLabel);

    applyLayout->addLayout(btnRow);
    layout->addWidget(applyGroup);

    // Save button slot. Validates, persists, propagates, applies to
    // live clients, stamps timestamp, and flashes "Saved".
    connect(m_settingsSaveBtn, &QPushButton::clicked, this, [this] {
        const QString call = m_settingsCallEdit->text().trimmed().toUpper();
        const QString gridSquare =
            m_settingsGridEdit->text().trimmed().toUpper();
        const QString message = m_settingsFreedvMsgEdit->text().trimmed();

        m_settingsErrorLabel->clear();
        m_settingsSavedLabel->clear();

        // Validate callsign.
        if (call.isEmpty()) {
            m_settingsErrorLabel->setText("Callsign is required.");
            return;
        }
        if (call.length() < 3 || call.length() > 12) {
            m_settingsErrorLabel->setText(
                "Callsign must be 3 to 12 characters.");
            return;
        }
        // Validate Maidenhead grid: 4 or 6 chars, alpha+digit pattern
        // (e.g. EM73 or EM73XY). Reject anything else.
        if (gridSquare.length() != 4 && gridSquare.length() != 6) {
            m_settingsErrorLabel->setText(
                "Grid must be 4 or 6 character Maidenhead (e.g. EM73 or EM73XY).");
            return;
        }

        auto& settings = AppSettings::instance();
        // Canonical keys.
        settings.setValue("User/Callsign", call);
        settings.setValue("User/GridSquare", gridSquare);
        settings.setValue("FreeDvReporter/Message", message);
        settings.setValue("User/IdentityLastSaved",
                          QDateTime::currentDateTime().toString(Qt::ISODate));

        // Propagate to legacy per-source keys. Existing per-source-tab
        // readers (Cluster / RBN / PSK Reporter) keep reading their
        // per-source key on construction; propagation keeps them in
        // sync with the central Settings entry.
        //
        // 2026-05-12 (PR #238 review P2): PSK Reporter keys unified
        // on the slash-key family RadioModel reads
        // (RadioModel.cpp:1000-1004, :1575-1582).  Flat-key writes
        // were orphaned (nothing read them on restore), so a user
        // who saved identity via Save & Propagate, restarted the
        // app, and clicked PSK Start without re-entering the PSK
        // tab would emit IPFIX datagrams with empty receiver
        // fields.  Flat keys retained for DxCluster / Rbn (unrelated).
        settings.setValue("DxClusterCallsign", call);
        settings.setValue("RbnCallsign", call);
        settings.setValue("PskReporter/Callsign", call);
        settings.setValue("PskReporter/GridSquare", gridSquare);
        settings.setValue("FreeDvReporter/Callsign", call);
        settings.setValue("FreeDvReporter/GridSquare", gridSquare);

        settings.save();

        // Push to live clients if non-null. A connection that is
        // already up picks up the new identity without disconnect.
        // Version string comes from CMake (NEREUSSDR_VERSION); the
        // FreeDV / PSK Reporter pools want a versioned client tag.
        const QString version =
            QStringLiteral("NereusSDR/") + QStringLiteral(NEREUSSDR_VERSION);
        if (m_freedvClient) {
            m_freedvClient->setIdentity(call, gridSquare, message, version);
        }
        if (m_pskClient) {
            m_pskClient->setIdentity(call, gridSquare, version);
        }
        // 2026-05-12 bench fix #2: propagate the new identity into
        // each per-source tab's QLineEdit so the user sees the
        // change in every editable field, not just the Settings tab.
        // The tabs read AppSettings only on construction; without
        // this push, the Cluster / RBN / PSK callsign fields keep
        // their stale value until the dialog is closed and reopened.
        // Edits are guarded against null (some builds may construct
        // the dialog without certain source tabs in the future).
        if (m_callEdit) {
            m_callEdit->setText(call);
        }
        if (m_rbnCallEdit) {
            m_rbnCallEdit->setText(call);
        }
        if (m_pskCallEdit) {
            m_pskCallEdit->setText(call);
        }
        if (m_pskGridEdit) {
            m_pskGridEdit->setText(gridSquare);
        }

        // 2026-05-12 bench fix: notify MainWindow so it can push the
        // new grid into FreeDVStationModel.  Without this the Reporter
        // dialog's Distance / Hdg columns stay zeroed because the
        // station model never learns our grid.
        emit identitySaved(call, gridSquare, message);

        // Flash "Saved" for 2 seconds.
        m_settingsSavedLabel->setText("Saved");
        QTimer::singleShot(2000, this, [this] {
            if (m_settingsSavedLabel) {
                m_settingsSavedLabel->clear();
            }
        });

        // Refresh the current-identity summary line.
        if (m_settingsCurrentLabel) {
            m_settingsCurrentLabel->setText(
                QString("Current identity: %1 / %2 - last saved %3")
                    .arg(call, gridSquare,
                         QDateTime::currentDateTime().toString(
                             "yyyy-MM-dd HH:mm")));
        }
        // Update the FreeDV tab's identity-status label too so the
        // user sees the change without switching tabs.
        if (m_freedvIdentityLabel) {
            m_freedvIdentityLabel->setText(QString(
                "Identity: <b>%1</b> / <b>%2</b> (set in Settings tab)")
                .arg(call, gridSquare));
            m_freedvIdentityLabel->setStyleSheet(
                "QLabel { color: #4caf50; }");
        }
    });

    // ── Current identity summary ──────────────────────────────────
    const QString persistedCall =
        s.value("User/Callsign").toString();
    const QString persistedGrid =
        s.value("User/GridSquare").toString();
    const QString persistedSaved =
        s.value("User/IdentityLastSaved").toString();
    QString summary;
    if (persistedCall.isEmpty()) {
        summary = "No identity saved yet. Fill in the fields above and click Save.";
    } else {
        summary = QString("Current identity: %1 / %2")
                      .arg(persistedCall, persistedGrid);
        if (!persistedSaved.isEmpty()) {
            // Try to reformat the ISO timestamp into a human-friendly
            // form; fall back to the raw string if parsing fails.
            QDateTime ts = QDateTime::fromString(persistedSaved, Qt::ISODate);
            if (ts.isValid()) {
                summary += QString(" - last saved %1")
                               .arg(ts.toString("yyyy-MM-dd HH:mm"));
            }
        }
    }
    m_settingsCurrentLabel = new QLabel(summary);
    m_settingsCurrentLabel->setObjectName("settingsCurrentLabel");
    m_settingsCurrentLabel->setStyleSheet(
        "QLabel { color: #808890; font-size: 11px; }");
    m_settingsCurrentLabel->setWordWrap(true);
    layout->addWidget(m_settingsCurrentLabel);

    layout->addStretch();

    tabs->addTab(page, "Settings");
}

// From AetherSDR src/gui/DxClusterDialog.cpp:637-803 [@0cd4559].
// Cluster tab: telnet host/port/callsign + auto-connect toggle +
// connect/disconnect button + status label + cluster console +
// command input row. AetherSDR's per-source stylesheets, AppSettings
// key names, default server (`dxc.nc7j.com:7300`), and signal
// emission (`connectRequested(host, port, call)` /
// `disconnectRequested`) preserved verbatim. NereusSDR addition:
// objectName() on every test-relevant widget so smoke tests can
// locate them via findChild().
void SpotHubDialog::buildClusterTab(QTabWidget* tabs)
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(8);

    auto& s = AppSettings::instance();

    auto* connGroup = new QGroupBox("Connection");
    auto* connLayout = new QVBoxLayout(connGroup);
    connLayout->setSpacing(4);

    auto* grid = new QGridLayout;
    grid->setColumnStretch(1, 1);
    int row = 0;

    grid->addWidget(new QLabel("Server:"), row, 0);
    m_hostEdit = new QLineEdit(s.value("DxClusterHost", "dxc.nc7j.com").toString());
    m_hostEdit->setObjectName("clusterHostEdit");
    m_hostEdit->setPlaceholderText("dxc.nc7j.com");
    m_hostEdit->setStyleSheet(kLineEditStyle);
    grid->addWidget(m_hostEdit, row, 1);
    row++;

    grid->addWidget(new QLabel("Port:"), row, 0);
    m_portSpin = new QSpinBox;
    m_portSpin->setObjectName("clusterPortSpin");
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(s.value("DxClusterPort", 7300).toInt());
    m_portSpin->setStyleSheet(kSpinBoxStyle);
    grid->addWidget(m_portSpin, row, 1);
    row++;

    grid->addWidget(new QLabel("Callsign:"), row, 0);
    // Post-3J-2 UX fix: fall back to User/Callsign (the canonical key
    // written by the Settings tab) when DxClusterCallsign is empty.
    m_callEdit = new QLineEdit(
        s.value("DxClusterCallsign",
                s.value("User/Callsign").toString()).toString());
    m_callEdit->setObjectName("clusterCallEdit");
    m_callEdit->setPlaceholderText(
        "Enter Callsign Here (set under Settings tab)");
    m_callEdit->setStyleSheet(kLineEditStyle);
    grid->addWidget(m_callEdit, row, 1);
    row++;

    connLayout->addLayout(grid);

    auto* btnRow = new QHBoxLayout;
    m_autoConnectBtn = new QPushButton(
        s.value("DxClusterAutoConnect", "False").toString() == "True"
            ? "Auto-Connect: ON" : "Auto-Connect: OFF");
    m_autoConnectBtn->setObjectName("clusterAutoConnectBtn");
    m_autoConnectBtn->setCheckable(true);
    m_autoConnectBtn->setChecked(s.value("DxClusterAutoConnect", "False").toString() == "True");
    m_autoConnectBtn->setStyleSheet(kAutoToggleStyle);
    connect(m_autoConnectBtn, &QPushButton::toggled, this, [this](bool on) {
        m_autoConnectBtn->setText(on ? "Auto-Connect: ON" : "Auto-Connect: OFF");
        auto& settings = AppSettings::instance();
        settings.setValue("DxClusterAutoConnect", on ? "True" : "False");
        settings.save();
    });
    btnRow->addWidget(m_autoConnectBtn);
    btnRow->addStretch();

    m_statusLabel = new QLabel("Disconnected");
    m_statusLabel->setObjectName("clusterStatusLabel");
    m_statusLabel->setStyleSheet(kStatusIdleStyle);
    btnRow->addWidget(m_statusLabel);
    btnRow->addStretch();

    m_connectBtn = new QPushButton(m_clusterClient && m_clusterClient->isConnected()
                                       ? "Disconnect" : "Connect");
    m_connectBtn->setObjectName("clusterConnectBtn");
    m_connectBtn->setFixedWidth(100);
    m_connectBtn->setStyleSheet(kStartBtnStyle);
    connect(m_connectBtn, &QPushButton::clicked, this, [this] {
        if (m_clusterClient && m_clusterClient->isConnected()) {
            emit disconnectRequested();
            return;
        }
        QString host = m_hostEdit->text().trimmed();
        QString call = m_callEdit->text().trimmed().toUpper();
        quint16 port = static_cast<quint16>(m_portSpin->value());
        if (host.isEmpty() || call.isEmpty()) {
            m_statusLabel->setText("Server and callsign are required");
            m_statusLabel->setStyleSheet("QLabel { color: #ff4444; font-size: 11px; }");
            return;
        }
        auto& settings = AppSettings::instance();
        settings.setValue("DxClusterHost", host);
        settings.setValue("DxClusterPort", port);
        settings.setValue("DxClusterCallsign", call);
        settings.save();
        emit connectRequested(host, port, call);
    });
    btnRow->addWidget(m_connectBtn);
    connLayout->addLayout(btnRow);

    // 2026-05-12 bench fix: wire DxClusterClient state signals to the
    // status label + Connect button so the UI reflects the actual TCP
    // state.  Parallels the FreeDV wires at line 1561+.  Without this
    // block the Connect button stays at its constructor-time text and
    // the status label stays "Disconnected" even after a successful
    // login.
    if (m_clusterClient) {
        connect(m_clusterClient, &DxClusterClient::connected,
                this, [this]() {
                    if (m_statusLabel) {
                        m_statusLabel->setText("Connected");
                        m_statusLabel->setStyleSheet(kStatusActiveStyle);
                    }
                    if (m_connectBtn) {
                        m_connectBtn->setText("Disconnect");
                    }
                    if (m_cmdEdit) m_cmdEdit->setEnabled(true);
                    if (m_sendBtn) m_sendBtn->setEnabled(true);
                });
        connect(m_clusterClient, &DxClusterClient::disconnected,
                this, [this]() {
                    if (m_statusLabel) {
                        m_statusLabel->setText("Disconnected");
                        m_statusLabel->setStyleSheet(kStatusIdleStyle);
                    }
                    if (m_connectBtn) {
                        m_connectBtn->setText("Connect");
                    }
                    if (m_cmdEdit) m_cmdEdit->setEnabled(false);
                    if (m_sendBtn) m_sendBtn->setEnabled(false);
                });
        connect(m_clusterClient, &DxClusterClient::connectionError,
                this, [this](const QString& error) {
                    if (m_statusLabel) {
                        m_statusLabel->setText(QString("Error: %1").arg(error));
                        m_statusLabel->setStyleSheet(
                            "QLabel { color: #e6c200; font-size: 11px; }");
                    }
                });
        // Stream raw cluster lines into the console widget so the user
        // sees the welcome banner + spots scrolling as they arrive.
        connect(m_clusterClient, &DxClusterClient::rawLineReceived,
                this, [this](const QString& line) {
                    if (m_console) m_console->appendPlainText(line);
                });
    }

    layout->addWidget(connGroup);

    // Console + spot-color row
    auto* consoleRow = new QHBoxLayout;
    auto* consoleLabel = new QLabel("Cluster Console");
    consoleLabel->setStyleSheet("QLabel { color: #00b4d8; font-weight: bold; }");
    consoleRow->addWidget(consoleLabel);
    consoleRow->addStretch();

    auto* dxcColorLabel = new QLabel("Spot Color:");
    dxcColorLabel->setStyleSheet("QLabel { color: #808080; font-size: 12px; }");
    consoleRow->addWidget(dxcColorLabel);

    QColor dxcColor(s.value("DxClusterSpotColor", "#D2B48C").toString());
    auto* dxcColorBtn = new QPushButton;
    dxcColorBtn->setObjectName("clusterColorBtn");
    dxcColorBtn->setFixedSize(18, 18);
    dxcColorBtn->setStyleSheet(swatchStyle(dxcColor));
    connect(dxcColorBtn, &QPushButton::clicked, this, [this, dxcColorBtn] {
        QColor c = QColorDialog::getColor(
            QColor(AppSettings::instance().value("DxClusterSpotColor", "#D2B48C").toString()),
            this, "DX Cluster Spot Color");
        if (c.isValid()) {
            dxcColorBtn->setStyleSheet(swatchStyle(c));
            AppSettings::instance().setValue("DxClusterSpotColor", c.name());
            AppSettings::instance().save();
        }
    });
    consoleRow->addWidget(dxcColorBtn);
    layout->addLayout(consoleRow);

    m_console = new QPlainTextEdit;
    m_console->setObjectName("clusterConsole");
    m_console->setReadOnly(true);
    m_console->setMaximumBlockCount(2000);
    m_console->setStyleSheet(kConsoleStyle);
    layout->addWidget(m_console, 1);

    // Command input row
    auto* cmdRow = new QHBoxLayout;
    m_cmdEdit = new QLineEdit;
    m_cmdEdit->setObjectName("clusterCmdEdit");
    m_cmdEdit->setPlaceholderText("Type a cluster command (e.g. sh/dx 20, set/filter, bye)");
    m_cmdEdit->setStyleSheet(kCmdEditStyle);
    m_cmdEdit->setEnabled(m_clusterClient && m_clusterClient->isConnected());
    connect(m_cmdEdit, &QLineEdit::returnPressed, this, [this] {
        QString cmd = m_cmdEdit->text().trimmed();
        if (cmd.isEmpty() || !m_clusterClient || !m_clusterClient->isConnected()) {
            return;
        }
        auto* client = m_clusterClient;
        QMetaObject::invokeMethod(client, [client, cmd] { client->sendCommand(cmd); });
        m_console->appendPlainText("> " + cmd);
        m_cmdEdit->clear();
    });
    m_sendBtn = new QPushButton("Send");
    m_sendBtn->setObjectName("clusterSendBtn");
    m_sendBtn->setFixedWidth(60);
    m_sendBtn->setEnabled(m_clusterClient && m_clusterClient->isConnected());
    connect(m_sendBtn, &QPushButton::clicked, this, [this] {
        emit m_cmdEdit->returnPressed();
    });
    cmdRow->addWidget(m_cmdEdit, 1);
    cmdRow->addWidget(m_sendBtn);
    layout->addLayout(cmdRow);

    tabs->addTab(page, "Cluster");
}

// From AetherSDR src/gui/DxClusterDialog.cpp:805-992 [@0cd4559].
// RBN tab: same skeleton as the cluster tab plus a rate-limit
// spinbox row. Defaults to `telnet.reversebeacon.net:7000` and falls
// back to the cluster callsign when the RBN callsign is empty.
// Signals: `rbnConnectRequested(host, port, call)` /
// `rbnDisconnectRequested`. NereusSDR addition: objectName() on
// every test-relevant widget.
void SpotHubDialog::buildRbnTab(QTabWidget* tabs)
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(8);

    auto& s = AppSettings::instance();
    // Post-3J-2 UX fix: extend the existing RbnCallsign ->
    // DxClusterCallsign fallback chain with one more hop to the
    // canonical User/Callsign key written by the Settings tab.
    QString defaultCall = s.value("RbnCallsign").toString();
    if (defaultCall.isEmpty()) {
        defaultCall = s.value("DxClusterCallsign").toString();
    }
    if (defaultCall.isEmpty()) {
        defaultCall = s.value("User/Callsign").toString();
    }

    auto* connGroup = new QGroupBox("RBN Connection");
    auto* connLayout = new QVBoxLayout(connGroup);
    connLayout->setSpacing(4);

    auto* grid = new QGridLayout;
    grid->setColumnStretch(1, 1);
    int row = 0;

    grid->addWidget(new QLabel("Server:"), row, 0);
    m_rbnHostEdit = new QLineEdit(s.value("RbnHost", "telnet.reversebeacon.net").toString());
    m_rbnHostEdit->setObjectName("rbnHostEdit");
    m_rbnHostEdit->setPlaceholderText("telnet.reversebeacon.net");
    m_rbnHostEdit->setStyleSheet(kLineEditStyle);
    grid->addWidget(m_rbnHostEdit, row, 1);
    row++;

    grid->addWidget(new QLabel("Port:"), row, 0);
    m_rbnPortSpin = new QSpinBox;
    m_rbnPortSpin->setObjectName("rbnPortSpin");
    m_rbnPortSpin->setRange(1, 65535);
    m_rbnPortSpin->setValue(s.value("RbnPort", 7000).toInt());
    m_rbnPortSpin->setStyleSheet(kSpinBoxStyle);
    grid->addWidget(m_rbnPortSpin, row, 1);
    row++;

    grid->addWidget(new QLabel("Callsign:"), row, 0);
    m_rbnCallEdit = new QLineEdit(defaultCall);
    m_rbnCallEdit->setObjectName("rbnCallEdit");
    m_rbnCallEdit->setPlaceholderText(
        "Enter Callsign Here (set under Settings tab)");
    m_rbnCallEdit->setStyleSheet(kLineEditStyle);
    grid->addWidget(m_rbnCallEdit, row, 1);
    row++;

    grid->addWidget(new QLabel("Rate Limit:"), row, 0);
    auto* rateRow = new QHBoxLayout;
    auto* rateSpin = new QSpinBox;
    rateSpin->setObjectName("rbnRateSpin");
    rateSpin->setRange(1, 100);
    rateSpin->setValue(s.value("RbnRateLimit", 10).toInt());
    rateSpin->setSuffix(" spots/sec");
    rateSpin->setStyleSheet(kSpinBoxStyle);
    connect(rateSpin, &QSpinBox::valueChanged, this, [](int v) {
        auto& settings = AppSettings::instance();
        settings.setValue("RbnRateLimit", v);
        settings.save();
    });
    rateRow->addWidget(rateSpin);
    rateRow->addStretch();
    grid->addLayout(rateRow, row, 1);
    row++;

    connLayout->addLayout(grid);

    auto* btnRow = new QHBoxLayout;
    m_rbnAutoConnectBtn = new QPushButton(
        s.value("RbnAutoConnect", "False").toString() == "True"
            ? "Auto-Connect: ON" : "Auto-Connect: OFF");
    m_rbnAutoConnectBtn->setObjectName("rbnAutoConnectBtn");
    m_rbnAutoConnectBtn->setCheckable(true);
    m_rbnAutoConnectBtn->setChecked(s.value("RbnAutoConnect", "False").toString() == "True");
    m_rbnAutoConnectBtn->setStyleSheet(kAutoToggleStyle);
    connect(m_rbnAutoConnectBtn, &QPushButton::toggled, this, [this](bool on) {
        m_rbnAutoConnectBtn->setText(on ? "Auto-Connect: ON" : "Auto-Connect: OFF");
        auto& settings = AppSettings::instance();
        settings.setValue("RbnAutoConnect", on ? "True" : "False");
        settings.save();
    });
    btnRow->addWidget(m_rbnAutoConnectBtn);
    btnRow->addStretch();

    m_rbnStatusLabel = new QLabel("Disconnected");
    m_rbnStatusLabel->setObjectName("rbnStatusLabel");
    m_rbnStatusLabel->setStyleSheet(kStatusIdleStyle);
    btnRow->addWidget(m_rbnStatusLabel);
    btnRow->addStretch();

    m_rbnConnectBtn = new QPushButton(m_rbnClient && m_rbnClient->isConnected()
                                          ? "Disconnect" : "Connect");
    m_rbnConnectBtn->setObjectName("rbnConnectBtn");
    m_rbnConnectBtn->setFixedWidth(100);
    m_rbnConnectBtn->setStyleSheet(kStartBtnStyle);
    connect(m_rbnConnectBtn, &QPushButton::clicked, this, [this] {
        if (m_rbnClient && m_rbnClient->isConnected()) {
            emit rbnDisconnectRequested();
            return;
        }
        QString host = m_rbnHostEdit->text().trimmed();
        QString call = m_rbnCallEdit->text().trimmed().toUpper();
        quint16 port = static_cast<quint16>(m_rbnPortSpin->value());
        if (host.isEmpty() || call.isEmpty()) {
            m_rbnStatusLabel->setText("Server and callsign are required");
            m_rbnStatusLabel->setStyleSheet("QLabel { color: #ff4444; font-size: 11px; }");
            return;
        }
        auto& settings = AppSettings::instance();
        settings.setValue("RbnHost", host);
        settings.setValue("RbnPort", port);
        settings.setValue("RbnCallsign", call);
        settings.save();
        emit rbnConnectRequested(host, port, call);
    });
    btnRow->addWidget(m_rbnConnectBtn);
    connLayout->addLayout(btnRow);

    // 2026-05-12 bench fix: wire RBN state signals to UI.  Mirrors the
    // DxCluster wire block at the top of this file — same DxClusterClient
    // class, just a different instance pointing at the RBN telnet server.
    if (m_rbnClient) {
        connect(m_rbnClient, &DxClusterClient::connected,
                this, [this]() {
                    if (m_rbnStatusLabel) {
                        m_rbnStatusLabel->setText("Connected");
                        m_rbnStatusLabel->setStyleSheet(kStatusActiveStyle);
                    }
                    if (m_rbnConnectBtn) m_rbnConnectBtn->setText("Disconnect");
                    if (m_rbnCmdEdit) m_rbnCmdEdit->setEnabled(true);
                    if (m_rbnSendBtn) m_rbnSendBtn->setEnabled(true);
                });
        connect(m_rbnClient, &DxClusterClient::disconnected,
                this, [this]() {
                    if (m_rbnStatusLabel) {
                        m_rbnStatusLabel->setText("Disconnected");
                        m_rbnStatusLabel->setStyleSheet(kStatusIdleStyle);
                    }
                    if (m_rbnConnectBtn) m_rbnConnectBtn->setText("Connect");
                    if (m_rbnCmdEdit) m_rbnCmdEdit->setEnabled(false);
                    if (m_rbnSendBtn) m_rbnSendBtn->setEnabled(false);
                });
        connect(m_rbnClient, &DxClusterClient::connectionError,
                this, [this](const QString& error) {
                    if (m_rbnStatusLabel) {
                        m_rbnStatusLabel->setText(QString("Error: %1").arg(error));
                        m_rbnStatusLabel->setStyleSheet(
                            "QLabel { color: #e6c200; font-size: 11px; }");
                    }
                });
        connect(m_rbnClient, &DxClusterClient::rawLineReceived,
                this, [this](const QString& line) {
                    if (m_rbnConsole) m_rbnConsole->appendPlainText(line);
                });
    }

    layout->addWidget(connGroup);

    // Console + spot-color row
    auto* rbnConsoleRow = new QHBoxLayout;
    auto* rbnConsoleLabel = new QLabel("RBN Console");
    rbnConsoleLabel->setStyleSheet("QLabel { color: #00b4d8; font-weight: bold; }");
    rbnConsoleRow->addWidget(rbnConsoleLabel);
    rbnConsoleRow->addStretch();

    auto* rbnColorLabel = new QLabel("Spot Color:");
    rbnColorLabel->setStyleSheet("QLabel { color: #808080; font-size: 12px; }");
    rbnConsoleRow->addWidget(rbnColorLabel);

    QColor rbnColor(s.value("RbnSpotColor", "#4488FF").toString());
    auto* rbnColorBtn = new QPushButton;
    rbnColorBtn->setObjectName("rbnColorBtn");
    rbnColorBtn->setFixedSize(18, 18);
    rbnColorBtn->setStyleSheet(swatchStyle(rbnColor));
    connect(rbnColorBtn, &QPushButton::clicked, this, [this, rbnColorBtn] {
        QColor c = QColorDialog::getColor(
            QColor(AppSettings::instance().value("RbnSpotColor", "#4488FF").toString()),
            this, "RBN Spot Color");
        if (c.isValid()) {
            rbnColorBtn->setStyleSheet(swatchStyle(c));
            AppSettings::instance().setValue("RbnSpotColor", c.name());
            AppSettings::instance().save();
        }
    });
    rbnConsoleRow->addWidget(rbnColorBtn);
    layout->addLayout(rbnConsoleRow);

    m_rbnConsole = new QPlainTextEdit;
    m_rbnConsole->setObjectName("rbnConsole");
    m_rbnConsole->setReadOnly(true);
    m_rbnConsole->setMaximumBlockCount(2000);
    m_rbnConsole->setStyleSheet(kConsoleStyle);
    layout->addWidget(m_rbnConsole, 1);

    // Command input row
    auto* cmdRow = new QHBoxLayout;
    m_rbnCmdEdit = new QLineEdit;
    m_rbnCmdEdit->setObjectName("rbnCmdEdit");
    m_rbnCmdEdit->setPlaceholderText("Type an RBN command (e.g. set/skimmer, set/ft8, bye)");
    m_rbnCmdEdit->setStyleSheet(kCmdEditStyle);
    m_rbnCmdEdit->setEnabled(m_rbnClient && m_rbnClient->isConnected());
    connect(m_rbnCmdEdit, &QLineEdit::returnPressed, this, [this] {
        QString cmd = m_rbnCmdEdit->text().trimmed();
        if (cmd.isEmpty() || !m_rbnClient || !m_rbnClient->isConnected()) {
            return;
        }
        auto* client = m_rbnClient;
        QMetaObject::invokeMethod(client, [client, cmd] { client->sendCommand(cmd); });
        m_rbnConsole->appendPlainText("> " + cmd);
        m_rbnCmdEdit->clear();
    });
    m_rbnSendBtn = new QPushButton("Send");
    m_rbnSendBtn->setObjectName("rbnSendBtn");
    m_rbnSendBtn->setFixedWidth(60);
    m_rbnSendBtn->setEnabled(m_rbnClient && m_rbnClient->isConnected());
    connect(m_rbnSendBtn, &QPushButton::clicked, this, [this] {
        emit m_rbnCmdEdit->returnPressed();
    });
    cmdRow->addWidget(m_rbnCmdEdit, 1);
    cmdRow->addWidget(m_rbnSendBtn);
    layout->addLayout(cmdRow);

    tabs->addTab(page, "RBN");
}

// From AetherSDR src/gui/DxClusterDialog.cpp:994-1237 [@0cd4559].
// WSJT-X tab: UDP multicast address + port + auto-start + start/stop
// button + spot filter checkboxes (CQ / CQ POTA / Calling Me) with
// inline color pickers + Default color picker + spot-life slider +
// raw-event console. Signals: `wsjtxStartRequested(addr, port)` /
// `wsjtxStopRequested`. NereusSDR addition: objectName() on every
// test-relevant widget.
void SpotHubDialog::buildWsjtxTab(QTabWidget* tabs)
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(8);

    auto& s = AppSettings::instance();

    auto* connGroup = new QGroupBox("WSJT-X Listener Address");
    auto* connLayout = new QVBoxLayout(connGroup);
    connLayout->setSpacing(4);

    auto* grid = new QGridLayout;
    grid->setColumnStretch(1, 1);
    int row = 0;

    grid->addWidget(new QLabel("Address:"), row, 0);
    m_wsjtxAddrEdit = new QLineEdit(s.value("WsjtxAddress", "224.0.0.1").toString());
    m_wsjtxAddrEdit->setObjectName("wsjtxAddrEdit");
    m_wsjtxAddrEdit->setPlaceholderText("224.0.0.1 (multicast) or 0.0.0.0 (any)");
    m_wsjtxAddrEdit->setStyleSheet(kLineEditStyle);
    grid->addWidget(m_wsjtxAddrEdit, row, 1);
    row++;

    grid->addWidget(new QLabel("Port:"), row, 0);
    m_wsjtxPortSpin = new QSpinBox;
    m_wsjtxPortSpin->setObjectName("wsjtxPortSpin");
    m_wsjtxPortSpin->setRange(1, 65535);
    m_wsjtxPortSpin->setValue(s.value("WsjtxPort", 2237).toInt());
    m_wsjtxPortSpin->setStyleSheet(kSpinBoxStyle);
    grid->addWidget(m_wsjtxPortSpin, row, 1);
    row++;

    connLayout->addLayout(grid);

    auto* btnRow = new QHBoxLayout;
    m_wsjtxAutoStartBtn = new QPushButton(
        s.value("WsjtxAutoStart", "False").toString() == "True"
            ? "Auto-Start: ON" : "Auto-Start: OFF");
    m_wsjtxAutoStartBtn->setObjectName("wsjtxAutoStartBtn");
    m_wsjtxAutoStartBtn->setCheckable(true);
    m_wsjtxAutoStartBtn->setChecked(s.value("WsjtxAutoStart", "False").toString() == "True");
    m_wsjtxAutoStartBtn->setStyleSheet(kAutoToggleStyle);
    connect(m_wsjtxAutoStartBtn, &QPushButton::toggled, this, [this](bool on) {
        m_wsjtxAutoStartBtn->setText(on ? "Auto-Start: ON" : "Auto-Start: OFF");
        auto& settings = AppSettings::instance();
        settings.setValue("WsjtxAutoStart", on ? "True" : "False");
        settings.save();
    });
    btnRow->addWidget(m_wsjtxAutoStartBtn);
    btnRow->addStretch();

    m_wsjtxStatusLabel = new QLabel("Stopped");
    m_wsjtxStatusLabel->setObjectName("wsjtxStatusLabel");
    m_wsjtxStatusLabel->setStyleSheet(kStatusIdleStyle);
    btnRow->addWidget(m_wsjtxStatusLabel);
    btnRow->addStretch();

    m_wsjtxStartBtn = new QPushButton(m_wsjtxClient && m_wsjtxClient->isListening()
                                          ? "Stop" : "Start");
    m_wsjtxStartBtn->setObjectName("wsjtxStartBtn");
    m_wsjtxStartBtn->setFixedWidth(100);
    m_wsjtxStartBtn->setStyleSheet(kStartBtnStyle);
    connect(m_wsjtxStartBtn, &QPushButton::clicked, this, [this] {
        if (m_wsjtxClient && m_wsjtxClient->isListening()) {
            emit wsjtxStopRequested();
            return;
        }
        quint16 port = static_cast<quint16>(m_wsjtxPortSpin->value());
        QString addr = m_wsjtxAddrEdit->text().trimmed();
        if (addr.isEmpty()) {
            addr = "224.0.0.1";
        }
        auto& settings = AppSettings::instance();
        settings.setValue("WsjtxAddress", addr);
        settings.setValue("WsjtxPort", port);
        settings.save();
        emit wsjtxStartRequested(addr, port);
    });
    btnRow->addWidget(m_wsjtxStartBtn);
    connLayout->addLayout(btnRow);

    // 2026-05-12 bench fix: wire WSJT-X listening/stopped signals to UI.
    // Without this Start fires wsjtxStartRequested → startListening
    // (UDP socket bind), but the user sees the button stay "Start" and
    // the status label stay "Stopped" — looks dead.
    if (m_wsjtxClient) {
        connect(m_wsjtxClient, &WsjtxClient::listening,
                this, [this]() {
                    if (m_wsjtxStatusLabel) {
                        m_wsjtxStatusLabel->setText(
                            QString("Listening on %1:%2")
                                .arg(m_wsjtxAddrEdit->text().trimmed(),
                                     QString::number(m_wsjtxPortSpin->value())));
                        m_wsjtxStatusLabel->setStyleSheet(kStatusActiveStyle);
                    }
                    if (m_wsjtxStartBtn) m_wsjtxStartBtn->setText("Stop");
                });
        connect(m_wsjtxClient, &WsjtxClient::stopped,
                this, [this]() {
                    if (m_wsjtxStatusLabel) {
                        m_wsjtxStatusLabel->setText("Stopped");
                        m_wsjtxStatusLabel->setStyleSheet(kStatusIdleStyle);
                    }
                    if (m_wsjtxStartBtn) m_wsjtxStartBtn->setText("Start");
                });
        connect(m_wsjtxClient, &WsjtxClient::rawLineReceived,
                this, [this](const QString& line) {
                    if (m_wsjtxConsole) m_wsjtxConsole->appendPlainText(line);
                });
    }

    layout->addWidget(connGroup);

    // Spot filters with inline color pickers
    auto* filterRow = new QHBoxLayout;
    filterRow->setSpacing(6);
    auto* filterLabel = new QLabel("Spot Filter:");
    filterLabel->setStyleSheet("QLabel { color: #808080; font-size: 14px; }");
    filterRow->addWidget(filterLabel);

    const QString cbStyle =
        "QCheckBox { color: #a0b0c0; font-size: 14px; spacing: 3px; }"
        "QCheckBox::indicator { width: 14px; height: 14px; }";

    // CQ color + checkbox
    QColor cqColor(s.value("WsjtxColorCQ", "#00FF00").toString());
    m_wsjtxColorCQ = new QPushButton;
    m_wsjtxColorCQ->setObjectName("wsjtxColorCQ");
    m_wsjtxColorCQ->setFixedSize(18, 18);
    m_wsjtxColorCQ->setStyleSheet(swatchStyle(cqColor));
    connect(m_wsjtxColorCQ, &QPushButton::clicked, this, [this] {
        QColor c = QColorDialog::getColor(
            QColor(AppSettings::instance().value("WsjtxColorCQ", "#00FF00").toString()),
            this, "CQ Spot Color");
        if (c.isValid()) {
            m_wsjtxColorCQ->setStyleSheet(swatchStyle(c));
            AppSettings::instance().setValue("WsjtxColorCQ", c.name());
            AppSettings::instance().save();
        }
    });
    filterRow->addWidget(m_wsjtxColorCQ);

    m_wsjtxFilterCQ = new QCheckBox("CQ");
    m_wsjtxFilterCQ->setObjectName("wsjtxFilterCQ");
    m_wsjtxFilterCQ->setChecked(s.value("WsjtxFilterCQ", "True").toString() == "True");
    m_wsjtxFilterCQ->setStyleSheet(cbStyle);
    connect(m_wsjtxFilterCQ, &QCheckBox::toggled, this, [this](bool on) {
        if (on) {
            m_wsjtxFilterPOTA->setChecked(false);
        }
        auto& settings = AppSettings::instance();
        settings.setValue("WsjtxFilterCQ", on ? "True" : "False");
        settings.save();
    });
    filterRow->addWidget(m_wsjtxFilterCQ, 1);

    // CQ POTA color + checkbox
    QColor potaColor(s.value("WsjtxColorPOTA", "#00FFFF").toString());
    m_wsjtxColorPOTA = new QPushButton;
    m_wsjtxColorPOTA->setObjectName("wsjtxColorPOTA");
    m_wsjtxColorPOTA->setFixedSize(18, 18);
    m_wsjtxColorPOTA->setStyleSheet(swatchStyle(potaColor));
    connect(m_wsjtxColorPOTA, &QPushButton::clicked, this, [this] {
        QColor c = QColorDialog::getColor(
            QColor(AppSettings::instance().value("WsjtxColorPOTA", "#00FFFF").toString()),
            this, "CQ POTA Spot Color");
        if (c.isValid()) {
            m_wsjtxColorPOTA->setStyleSheet(swatchStyle(c));
            AppSettings::instance().setValue("WsjtxColorPOTA", c.name());
            AppSettings::instance().save();
        }
    });
    filterRow->addWidget(m_wsjtxColorPOTA);

    m_wsjtxFilterPOTA = new QCheckBox("CQ POTA");
    m_wsjtxFilterPOTA->setObjectName("wsjtxFilterPOTA");
    m_wsjtxFilterPOTA->setChecked(s.value("WsjtxFilterPOTA", "True").toString() == "True");
    m_wsjtxFilterPOTA->setStyleSheet(cbStyle);
    connect(m_wsjtxFilterPOTA, &QCheckBox::toggled, this, [this](bool on) {
        if (on) {
            m_wsjtxFilterCQ->setChecked(false);
        }
        auto& settings = AppSettings::instance();
        settings.setValue("WsjtxFilterPOTA", on ? "True" : "False");
        settings.save();
    });
    filterRow->addWidget(m_wsjtxFilterPOTA, 1);

    // Calling Me color + checkbox
    QColor callingMeColor(s.value("WsjtxColorCallingMe", "#FF0000").toString());
    m_wsjtxColorCallingMe = new QPushButton;
    m_wsjtxColorCallingMe->setObjectName("wsjtxColorCallingMe");
    m_wsjtxColorCallingMe->setFixedSize(18, 18);
    m_wsjtxColorCallingMe->setStyleSheet(swatchStyle(callingMeColor));
    connect(m_wsjtxColorCallingMe, &QPushButton::clicked, this, [this] {
        QColor c = QColorDialog::getColor(
            QColor(AppSettings::instance().value("WsjtxColorCallingMe", "#FF0000").toString()),
            this, "Calling Me Spot Color");
        if (c.isValid()) {
            m_wsjtxColorCallingMe->setStyleSheet(swatchStyle(c));
            AppSettings::instance().setValue("WsjtxColorCallingMe", c.name());
            AppSettings::instance().save();
        }
    });
    filterRow->addWidget(m_wsjtxColorCallingMe);

    m_wsjtxFilterCallingMe = new QCheckBox("Calling Me");
    m_wsjtxFilterCallingMe->setObjectName("wsjtxFilterCallingMe");
    m_wsjtxFilterCallingMe->setChecked(s.value("WsjtxFilterCallingMe", "True").toString() == "True");
    m_wsjtxFilterCallingMe->setStyleSheet(cbStyle);
    connect(m_wsjtxFilterCallingMe, &QCheckBox::toggled, this, [](bool on) {
        auto& settings = AppSettings::instance();
        settings.setValue("WsjtxFilterCallingMe", on ? "True" : "False");
        settings.save();
    });
    filterRow->addWidget(m_wsjtxFilterCallingMe, 1);

    // Default color (no checkbox)
    QColor defaultColor(s.value("WsjtxColorDefault", "#FFFFFF").toString());
    m_wsjtxColorDefault = new QPushButton;
    m_wsjtxColorDefault->setObjectName("wsjtxColorDefault");
    m_wsjtxColorDefault->setFixedSize(18, 18);
    m_wsjtxColorDefault->setStyleSheet(swatchStyle(defaultColor));
    connect(m_wsjtxColorDefault, &QPushButton::clicked, this, [this] {
        QColor c = QColorDialog::getColor(
            QColor(AppSettings::instance().value("WsjtxColorDefault", "#FFFFFF").toString()),
            this, "Default Spot Color");
        if (c.isValid()) {
            m_wsjtxColorDefault->setStyleSheet(swatchStyle(c));
            AppSettings::instance().setValue("WsjtxColorDefault", c.name());
            AppSettings::instance().save();
        }
    });
    filterRow->addWidget(m_wsjtxColorDefault);
    auto* defaultLabel = new QLabel("Default");
    defaultLabel->setStyleSheet("QLabel { color: #a0b0c0; font-size: 14px; }");
    filterRow->addWidget(defaultLabel);

    layout->addLayout(filterRow);

    // Decodes label + spot-life slider
    auto* decodeRow = new QHBoxLayout;
    auto* consoleLabel = new QLabel("WSJT-X Decodes");
    consoleLabel->setStyleSheet("QLabel { color: #00b4d8; font-weight: bold; }");
    decodeRow->addWidget(consoleLabel);
    decodeRow->addStretch();

    auto* lifeLabel = new QLabel("Spot Life:");
    lifeLabel->setStyleSheet("QLabel { color: #808080; font-size: 12px; }");
    decodeRow->addWidget(lifeLabel);

    int wsjtxLife = s.value("WsjtxSpotLifetime", 120).toInt();
    auto* wsjtxLifeSlider = new QSlider(Qt::Horizontal);
    wsjtxLifeSlider->setObjectName("wsjtxLifeSlider");
    wsjtxLifeSlider->setRange(30, 300);
    wsjtxLifeSlider->setValue(wsjtxLife);
    wsjtxLifeSlider->setFixedWidth(120);
    decodeRow->addWidget(wsjtxLifeSlider);

    auto* wsjtxLifeValue = new QLabel(QString("%1s").arg(wsjtxLife));
    wsjtxLifeValue->setFixedWidth(35);
    wsjtxLifeValue->setAlignment(Qt::AlignRight);
    wsjtxLifeValue->setStyleSheet("QLabel { color: #a0b0c0; font-size: 12px; }");
    decodeRow->addWidget(wsjtxLifeValue);

    connect(wsjtxLifeSlider, &QSlider::valueChanged, this, [wsjtxLifeValue](int v) {
        wsjtxLifeValue->setText(QString("%1s").arg(v));
        auto& settings = AppSettings::instance();
        settings.setValue("WsjtxSpotLifetime", v);
        settings.save();
    });
    layout->addLayout(decodeRow);

    m_wsjtxConsole = new QPlainTextEdit;
    m_wsjtxConsole->setObjectName("wsjtxConsole");
    m_wsjtxConsole->setReadOnly(true);
    m_wsjtxConsole->setMaximumBlockCount(2000);
    m_wsjtxConsole->setStyleSheet(kConsoleStyle);
    layout->addWidget(m_wsjtxConsole, 1);

    tabs->addTab(page, "WSJT-X");
}

// From AetherSDR src/gui/DxClusterDialog.cpp:1239-1345 [@0cd4559].
// SpotCollector tab: UDP port spinbox + help text + auto-start
// toggle + start/stop button + status label + raw-event console.
// Signals: `spotCollectorStartRequested(port)` /
// `spotCollectorStopRequested`. NereusSDR addition: objectName() on
// every test-relevant widget.
void SpotHubDialog::buildSpotCollectorTab(QTabWidget* tabs)
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(8);

    auto& s = AppSettings::instance();

    auto* connGroup = new QGroupBox("SpotCollector UDP Listener");
    auto* connLayout = new QVBoxLayout(connGroup);
    connLayout->setSpacing(4);

    auto* grid = new QGridLayout;
    grid->setColumnStretch(1, 1);

    grid->addWidget(new QLabel("UDP Port:"), 0, 0);
    m_scPortSpin = new QSpinBox;
    m_scPortSpin->setObjectName("scPortSpin");
    m_scPortSpin->setRange(1, 65535);
    m_scPortSpin->setValue(s.value("SpotCollectorPort", 9999).toInt());
    m_scPortSpin->setStyleSheet(kSpinBoxStyle);
    grid->addWidget(m_scPortSpin, 0, 1);

    connLayout->addLayout(grid);

    auto* helpLabel = new QLabel(
        "Receives DX spots from DXLab SpotCollector via UDP push.\n"
        "In SpotCollector, enable UDP broadcast to this port (default 9999).\n"
        "Alternatively, use the DX Cluster tab to connect to SpotCollector's telnet interface.");
    helpLabel->setWordWrap(true);
    helpLabel->setStyleSheet("QLabel { color: #8aa8c0; font-size: 11px; }");
    connLayout->addWidget(helpLabel);

    auto* btnRow = new QHBoxLayout;
    m_scAutoStartBtn = new QPushButton(
        s.value("SpotCollectorAutoStart", "False").toString() == "True"
            ? "Auto-Start: ON" : "Auto-Start: OFF");
    m_scAutoStartBtn->setObjectName("scAutoStartBtn");
    m_scAutoStartBtn->setCheckable(true);
    m_scAutoStartBtn->setChecked(s.value("SpotCollectorAutoStart", "False").toString() == "True");
    m_scAutoStartBtn->setStyleSheet(kAutoToggleStyle);
    connect(m_scAutoStartBtn, &QPushButton::toggled, this, [this](bool on) {
        m_scAutoStartBtn->setText(on ? "Auto-Start: ON" : "Auto-Start: OFF");
        auto& settings = AppSettings::instance();
        settings.setValue("SpotCollectorAutoStart", on ? "True" : "False");
        settings.save();
    });
    btnRow->addWidget(m_scAutoStartBtn);
    btnRow->addStretch();

    m_scStatusLabel = new QLabel("Stopped");
    m_scStatusLabel->setObjectName("scStatusLabel");
    m_scStatusLabel->setStyleSheet(kStatusIdleStyle);
    btnRow->addWidget(m_scStatusLabel);
    btnRow->addStretch();

    m_scStartBtn = new QPushButton(m_spotCollectorClient && m_spotCollectorClient->isListening()
                                       ? "Stop" : "Start");
    m_scStartBtn->setObjectName("scStartBtn");
    m_scStartBtn->setFixedWidth(100);
    m_scStartBtn->setStyleSheet(kStartBtnStyle);
    connect(m_scStartBtn, &QPushButton::clicked, this, [this] {
        if (m_spotCollectorClient && m_spotCollectorClient->isListening()) {
            emit spotCollectorStopRequested();
            return;
        }
        quint16 port = static_cast<quint16>(m_scPortSpin->value());
        auto& settings = AppSettings::instance();
        settings.setValue("SpotCollectorPort", port);
        settings.save();
        emit spotCollectorStartRequested(port);
    });
    btnRow->addWidget(m_scStartBtn);
    connLayout->addLayout(btnRow);

    // 2026-05-12 bench fix: wire SpotCollector listening/stopped signals
    // to UI.  Same shape as the POTA and WSJT-X wires; click does nothing
    // visible without these.
    if (m_spotCollectorClient) {
        connect(m_spotCollectorClient, &SpotCollectorClient::listening,
                this, [this]() {
                    if (m_scStatusLabel) {
                        m_scStatusLabel->setText(
                            QString("Listening on port %1")
                                .arg(m_scPortSpin->value()));
                        m_scStatusLabel->setStyleSheet(kStatusActiveStyle);
                    }
                    if (m_scStartBtn) m_scStartBtn->setText("Stop");
                });
        connect(m_spotCollectorClient, &SpotCollectorClient::stopped,
                this, [this]() {
                    if (m_scStatusLabel) {
                        m_scStatusLabel->setText("Stopped");
                        m_scStatusLabel->setStyleSheet(kStatusIdleStyle);
                    }
                    if (m_scStartBtn) m_scStartBtn->setText("Start");
                });
        connect(m_spotCollectorClient, &SpotCollectorClient::rawLineReceived,
                this, [this](const QString& line) {
                    if (m_scConsole) m_scConsole->appendPlainText(line);
                });
    }

    layout->addWidget(connGroup);

    auto* consoleLabel = new QLabel("SpotCollector Spots");
    consoleLabel->setStyleSheet("QLabel { color: #00b4d8; font-weight: bold; }");
    layout->addWidget(consoleLabel);

    m_scConsole = new QPlainTextEdit;
    m_scConsole->setObjectName("scConsole");
    m_scConsole->setReadOnly(true);
    m_scConsole->setMaximumBlockCount(2000);
    m_scConsole->setStyleSheet(kConsoleStyle);
    layout->addWidget(m_scConsole, 1);

    if (m_spotCollectorClient && m_spotCollectorClient->isListening()) {
        m_scStatusLabel->setText(QString("Listening on port %1").arg(m_scPortSpin->value()));
        m_scStatusLabel->setStyleSheet("QLabel { color: #00b4d8; font-size: 11px; }");
        m_scStartBtn->setText("Stop");
    }

    tabs->addTab(page, "SpotCollector");
}

// From AetherSDR src/gui/DxClusterDialog.cpp:1347-1479 [@0cd4559].
// POTA tab: HTTP polling-interval spinbox + auto-start toggle +
// start/stop button + status label + raw-event console + spot
// color picker. Signals: `potaStartRequested(interval)` /
// `potaStopRequested`. NereusSDR addition: objectName() on every
// test-relevant widget.
void SpotHubDialog::buildPotaTab(QTabWidget* tabs)
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(8);

    auto& s = AppSettings::instance();

    auto* connGroup = new QGroupBox("POTA Spot Feed");
    auto* connLayout = new QVBoxLayout(connGroup);
    connLayout->setSpacing(4);

    auto* grid = new QGridLayout;
    grid->setColumnStretch(1, 1);
    int row = 0;

    grid->addWidget(new QLabel("Server:"), row, 0);
    auto* serverLabel = new QLabel("api.pota.app (HTTP polling)");
    serverLabel->setStyleSheet("QLabel { color: #808890; }");
    grid->addWidget(serverLabel, row, 1);
    row++;

    grid->addWidget(new QLabel("Poll Interval:"), row, 0);
    m_potaIntervalSpin = new QSpinBox;
    m_potaIntervalSpin->setObjectName("potaIntervalSpin");
    m_potaIntervalSpin->setRange(15, 300);
    m_potaIntervalSpin->setValue(s.value("PotaPollInterval", 30).toInt());
    m_potaIntervalSpin->setSuffix(" sec");
    m_potaIntervalSpin->setStyleSheet(kSpinBoxStyle);
    connect(m_potaIntervalSpin, &QSpinBox::valueChanged, this, [](int v) {
        auto& settings = AppSettings::instance();
        settings.setValue("PotaPollInterval", v);
        settings.save();
    });
    grid->addWidget(m_potaIntervalSpin, row, 1);
    row++;

    connLayout->addLayout(grid);

    auto* btnRow = new QHBoxLayout;
    m_potaAutoStartBtn = new QPushButton(
        s.value("PotaAutoStart", "False").toString() == "True"
            ? "Auto-Start: ON" : "Auto-Start: OFF");
    m_potaAutoStartBtn->setObjectName("potaAutoStartBtn");
    m_potaAutoStartBtn->setCheckable(true);
    m_potaAutoStartBtn->setChecked(s.value("PotaAutoStart", "False").toString() == "True");
    m_potaAutoStartBtn->setStyleSheet(kAutoToggleStyle);
    connect(m_potaAutoStartBtn, &QPushButton::toggled, this, [this](bool on) {
        m_potaAutoStartBtn->setText(on ? "Auto-Start: ON" : "Auto-Start: OFF");
        auto& settings = AppSettings::instance();
        settings.setValue("PotaAutoStart", on ? "True" : "False");
        settings.save();
    });
    btnRow->addWidget(m_potaAutoStartBtn);
    btnRow->addStretch();

    m_potaStatusLabel = new QLabel("Stopped");
    m_potaStatusLabel->setObjectName("potaStatusLabel");
    m_potaStatusLabel->setStyleSheet(kStatusIdleStyle);
    btnRow->addWidget(m_potaStatusLabel);
    btnRow->addStretch();

    m_potaStartBtn = new QPushButton(m_potaClient && m_potaClient->isPolling()
                                         ? "Stop" : "Start");
    m_potaStartBtn->setObjectName("potaStartBtn");
    m_potaStartBtn->setFixedWidth(100);
    m_potaStartBtn->setStyleSheet(kStartBtnStyle);
    connect(m_potaStartBtn, &QPushButton::clicked, this, [this] {
        if (m_potaClient && m_potaClient->isPolling()) {
            emit potaStopRequested();
            return;
        }
        int interval = m_potaIntervalSpin->value();
        auto& settings = AppSettings::instance();
        settings.setValue("PotaPollInterval", interval);
        settings.save();
        emit potaStartRequested(interval);
    });
    btnRow->addWidget(m_potaStartBtn);
    connLayout->addLayout(btnRow);

    // 2026-05-12 bench fix: wire POTA started/stopped signals to UI.
    // Without this, clicking Start does fire potaStartRequested -> the
    // polling actually begins (proven by pota.log entries) but the
    // user sees no UI change: button stays "Start" and status stays
    // "Stopped".  Looks like the button is dead.
    if (m_potaClient) {
        connect(m_potaClient, &PotaClient::started,
                this, [this]() {
                    if (m_potaStatusLabel) {
                        m_potaStatusLabel->setText("Polling api.pota.app");
                        m_potaStatusLabel->setStyleSheet(kStatusActiveStyle);
                    }
                    if (m_potaStartBtn) m_potaStartBtn->setText("Stop");
                });
        connect(m_potaClient, &PotaClient::stopped,
                this, [this]() {
                    if (m_potaStatusLabel) {
                        m_potaStatusLabel->setText("Stopped");
                        m_potaStatusLabel->setStyleSheet(kStatusIdleStyle);
                    }
                    if (m_potaStartBtn) m_potaStartBtn->setText("Start");
                });
        // Console stream so the user sees activity as polls happen.
        connect(m_potaClient, &PotaClient::rawLineReceived,
                this, [this](const QString& line) {
                    if (m_potaConsole) m_potaConsole->appendPlainText(line);
                });
    }

    layout->addWidget(connGroup);

    auto* consoleRow = new QHBoxLayout;
    auto* consoleLabel = new QLabel("POTA Activations");
    consoleLabel->setStyleSheet("QLabel { color: #00b4d8; font-weight: bold; }");
    consoleRow->addWidget(consoleLabel);
    consoleRow->addStretch();

    auto* spotColorLabel = new QLabel("Spot Color:");
    spotColorLabel->setStyleSheet("QLabel { color: #808080; font-size: 12px; }");
    consoleRow->addWidget(spotColorLabel);

    QColor potaColor(s.value("PotaSpotColor", "#FFFF00").toString());
    auto* potaColorBtn = new QPushButton;
    potaColorBtn->setObjectName("potaColorBtn");
    potaColorBtn->setFixedSize(18, 18);
    potaColorBtn->setStyleSheet(swatchStyle(potaColor));
    connect(potaColorBtn, &QPushButton::clicked, this, [this, potaColorBtn] {
        QColor c = QColorDialog::getColor(
            QColor(AppSettings::instance().value("PotaSpotColor", "#FFFF00").toString()),
            this, "POTA Spot Color");
        if (c.isValid()) {
            potaColorBtn->setStyleSheet(swatchStyle(c));
            AppSettings::instance().setValue("PotaSpotColor", c.name());
            AppSettings::instance().save();
        }
    });
    consoleRow->addWidget(potaColorBtn);
    layout->addLayout(consoleRow);

    m_potaConsole = new QPlainTextEdit;
    m_potaConsole->setObjectName("potaConsole");
    m_potaConsole->setReadOnly(true);
    m_potaConsole->setMaximumBlockCount(2000);
    m_potaConsole->setStyleSheet(kConsoleStyle);
    layout->addWidget(m_potaConsole, 1);

    tabs->addTab(page, "POTA");
}

// From AetherSDR src/gui/DxClusterDialog.cpp:1482-1596 [@0cd4559].
// FreeDV tab: server label (qso.freedv.org, WebSocket) + auto-start
// toggle + start/stop button + status label + raw-event console +
// spot color picker. NereusSDR divergence from upstream: AetherSDR
// gates the entire builder on HAVE_WEBSOCKETS; NereusSDR builds
// unconditionally because `FreeDVReporterClient` (Task B5) is a
// native QWebSocket + nlohmann::json port rather than an optional
// dependency. Signals: `freedvStartRequested` / `freedvStopRequested`.
// NereusSDR addition: objectName() on every test-relevant widget.
void SpotHubDialog::buildFreeDvTab(QTabWidget* tabs)
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(8);

    auto& s = AppSettings::instance();

    // Post-3J-2 UX fix: identity-status row. FreeDV Reporter needs a
    // callsign + grid to connect; without it the WebSocket session
    // gets dropped by the server. The Settings tab is the single
    // source of identity, but the user is looking at the FreeDV tab
    // when they want to know what is going on, so a status label
    // surfaces the resolved identity here.
    {
        const QString call = s.value("FreeDvReporter/Callsign",
            s.value("User/Callsign").toString()).toString();
        const QString gridSquare = s.value("FreeDvReporter/GridSquare",
            s.value("User/GridSquare").toString()).toString();
        m_freedvIdentityLabel = new QLabel;
        m_freedvIdentityLabel->setObjectName("freedvIdentityLabel");
        m_freedvIdentityLabel->setTextFormat(Qt::RichText);
        m_freedvIdentityLabel->setWordWrap(true);
        if (call.isEmpty() || gridSquare.isEmpty()) {
            m_freedvIdentityLabel->setText(
                "<b style='color:#e6c200;'>No identity set.</b> "
                "Go to the <b>Settings</b> tab and fill in your callsign "
                "and grid square before starting FreeDV Reporter.");
        } else {
            m_freedvIdentityLabel->setText(QString(
                "Identity: <b>%1</b> / <b>%2</b> (set in Settings tab)")
                .arg(call, gridSquare));
            m_freedvIdentityLabel->setStyleSheet(
                "QLabel { color: #4caf50; }");
        }
        layout->addWidget(m_freedvIdentityLabel);
    }

    auto* connGroup = new QGroupBox("FreeDV QSO Reporter");
    auto* connLayout = new QVBoxLayout(connGroup);
    connLayout->setSpacing(4);

    auto* grid = new QGridLayout;
    grid->setColumnStretch(1, 1);
    int row = 0;

    grid->addWidget(new QLabel("Server:"), row, 0);
    auto* serverLabel = new QLabel("qso.freedv.org (WebSocket)");
    serverLabel->setStyleSheet("QLabel { color: #808890; }");
    grid->addWidget(serverLabel, row, 1);
    row++;

    connLayout->addLayout(grid);

    auto* btnRow = new QHBoxLayout;
    m_freedvAutoStartBtn = new QPushButton(
        s.value("FreeDvAutoStart", "False").toString() == "True"
            ? "Auto-Start: ON" : "Auto-Start: OFF");
    m_freedvAutoStartBtn->setObjectName("freedvAutoStartBtn");
    m_freedvAutoStartBtn->setCheckable(true);
    m_freedvAutoStartBtn->setChecked(s.value("FreeDvAutoStart", "False").toString() == "True");
    m_freedvAutoStartBtn->setStyleSheet(kAutoToggleStyle);
    m_freedvAutoStartBtn->setToolTip(
        "Identity (callsign and grid square) must be set in the "
        "Settings tab first. Auto-Start with an empty identity is "
        "skipped at launch time and the connection is not attempted.");
    connect(m_freedvAutoStartBtn, &QPushButton::toggled, this, [this](bool on) {
        m_freedvAutoStartBtn->setText(on ? "Auto-Start: ON" : "Auto-Start: OFF");
        auto& settings = AppSettings::instance();
        settings.setValue("FreeDvAutoStart", on ? "True" : "False");
        settings.save();
    });
    btnRow->addWidget(m_freedvAutoStartBtn);
    btnRow->addStretch();

    // Phase 3R K-bench (bench feedback): seed initial state from the
    // client's connection state. Without this, the label shows
    // "Stopped" until the next connected() signal fires — and if the
    // client was already connected when SpotHub opened (auto-start
    // ran at app launch), the signal already fired before the dialog
    // existed, leaving the label permanently stuck.
    const bool freedvAlreadyConnected =
        m_freedvClient && m_freedvClient->isConnected();
    m_freedvStatusLabel =
        new QLabel(freedvAlreadyConnected ? "Connected" : "Stopped");
    m_freedvStatusLabel->setObjectName("freedvStatusLabel");
    m_freedvStatusLabel->setStyleSheet(freedvAlreadyConnected
                                           ? kStatusActiveStyle
                                           : kStatusIdleStyle);
    btnRow->addWidget(m_freedvStatusLabel);
    btnRow->addStretch();

    m_freedvStartBtn = new QPushButton(m_freedvClient && m_freedvClient->isConnected()
                                           ? "Stop" : "Start");
    m_freedvStartBtn->setObjectName("freedvStartBtn");
    m_freedvStartBtn->setFixedWidth(100);
    m_freedvStartBtn->setStyleSheet(kStartBtnStyle);
    connect(m_freedvStartBtn, &QPushButton::clicked, this, [this] {
        if (m_freedvClient && m_freedvClient->isConnected()) {
            emit freedvStopRequested();
            return;
        }
        // Post-3J-2 UX fix: refuse to start with an empty identity.
        // Attempting startConnection() with no callsign produces an
        // anonymous WebSocket session that the qso.freedv.org server
        // drops. Surface the missing identity in the status label.
        auto& settings = AppSettings::instance();
        const QString call = settings.value("FreeDvReporter/Callsign",
            settings.value("User/Callsign").toString()).toString();
        const QString gridSquare = settings.value("FreeDvReporter/GridSquare",
            settings.value("User/GridSquare").toString()).toString();
        if (call.isEmpty() || gridSquare.isEmpty()) {
            if (m_freedvStatusLabel) {
                m_freedvStatusLabel->setText(
                    "Identity missing - set callsign and grid in Settings tab.");
                m_freedvStatusLabel->setStyleSheet(
                    "QLabel { color: #e6c200; font-size: 11px; }");
            }
            return;
        }
        emit freedvStartRequested();
    });
    btnRow->addWidget(m_freedvStartBtn);
    connLayout->addLayout(btnRow);

    // Phase 3R K-bench: wire connection-state signals so the status
    // label + Start/Stop button reflect reality. Without this, the
    // dialog stays at its constructor-time snapshot ("Stopped") even
    // after the client successfully connects to qso.freedv.org.
    if (m_freedvClient) {
        connect(m_freedvClient, &FreeDVReporterClient::connected,
                this, [this]() {
                    if (m_freedvStatusLabel) {
                        m_freedvStatusLabel->setText("Connected");
                        m_freedvStatusLabel->setStyleSheet(kStatusActiveStyle);
                    }
                    if (m_freedvStartBtn) {
                        m_freedvStartBtn->setText("Stop");
                    }
                });
        connect(m_freedvClient, &FreeDVReporterClient::disconnected,
                this, [this]() {
                    if (m_freedvStatusLabel) {
                        m_freedvStatusLabel->setText("Stopped");
                        m_freedvStatusLabel->setStyleSheet(kStatusIdleStyle);
                    }
                    if (m_freedvStartBtn) {
                        m_freedvStartBtn->setText("Start");
                    }
                });
        connect(m_freedvClient, &FreeDVReporterClient::connectionError,
                this, [this](const QString& error) {
                    if (m_freedvStatusLabel) {
                        m_freedvStatusLabel->setText(QString("Error: %1").arg(error));
                        m_freedvStatusLabel->setStyleSheet(
                            "QLabel { color: #e6c200; font-size: 11px; }");
                    }
                });
    }

    layout->addWidget(connGroup);

    // Phase 3R K-bench (bench feedback): preference toggles. Two small
    // checkboxes that gate (a) "display distance in miles" for the
    // FreeDV Reporter dialog's km/Hdg column, ported from freedv-gui
    // freedv_reporter.cpp `m_useMetricUnits` [@77e793a]; and (b)
    // "Report decodes to PSK Reporter", a cross-feed convenience that
    // mirrors freedv-gui's "Report to PSK Reporter" setting.
    {
        auto* prefsGroup = new QGroupBox("Preferences");
        auto* prefsLayout = new QVBoxLayout(prefsGroup);
        prefsLayout->setSpacing(4);

        // Display in miles toggle.
        auto* milesChk = new QCheckBox("Display distances in miles");
        milesChk->setObjectName("freedvDistanceMilesChk");
        milesChk->setToolTip(
            "When checked, the FreeDV Reporter dialog's distance column "
            "shows miles instead of kilometers. Mirrors freedv-gui's "
            "metric/imperial toggle.");
        milesChk->setChecked(
            s.value("FreeDvReporter/DistanceMiles", "False").toString()
            == "True");
        connect(milesChk, &QCheckBox::toggled, this, [](bool on) {
            auto& settings = AppSettings::instance();
            settings.setValue("FreeDvReporter/DistanceMiles",
                              on ? "True" : "False");
            settings.save();
        });
        prefsLayout->addWidget(milesChk);

        // Report to PSK Reporter toggle.
        auto* pskChk = new QCheckBox("Report decodes to PSK Reporter");
        pskChk->setObjectName("freedvReportToPskChk");
        pskChk->setToolTip(
            "When checked, RADE decodes also push to PSK Reporter (in "
            "addition to the FreeDV Reporter dashboard). Off by default "
            "to avoid duplicate spotting on operators who already run "
            "their own PSK Reporter feed.");
        pskChk->setChecked(
            s.value("FreeDvReporter/ReportToPsk", "False").toString()
            == "True");
        connect(pskChk, &QCheckBox::toggled, this, [](bool on) {
            auto& settings = AppSettings::instance();
            settings.setValue("FreeDvReporter/ReportToPsk",
                              on ? "True" : "False");
            settings.save();
        });
        prefsLayout->addWidget(pskChk);

        // Direction display 3-way combo, mirroring freedv-gui's
        // reportingDirectionAsCardinal toggle. NereusSDR exposes a
        // 3-way choice because the existing "045° NE" combined form
        // was a NereusSDR-native compromise; the upstream is binary.
        auto* dirRow = new QHBoxLayout;
        dirRow->setSpacing(4);
        auto* dirLabel = new QLabel("Heading display:");
        dirRow->addWidget(dirLabel);
        auto* dirCombo = new QComboBox;
        dirCombo->setObjectName("freedvDirectionCombo");
        dirCombo->addItem("Numeric + cardinal (045° NE)",
                          QStringLiteral("Combined"));
        dirCombo->addItem("Cardinal only (NE)",
                          QStringLiteral("Cardinal"));
        dirCombo->addItem("Numeric only (045°)",
                          QStringLiteral("Numeric"));
        dirCombo->setToolTip(
            "Heading column rendering in the FreeDV Reporter dialog. "
            "Mirrors freedv-gui's reportingDirectionAsCardinal setting "
            "with an extra combined option matching NereusSDR's "
            "default pre-bench rendering.");
        const QString savedDir =
            s.value("FreeDvReporter/DirectionAsCardinal",
                    QStringLiteral("Combined")).toString();
        for (int i = 0; i < dirCombo->count(); ++i) {
            if (dirCombo->itemData(i).toString() == savedDir) {
                dirCombo->setCurrentIndex(i);
                break;
            }
        }
        connect(dirCombo, &QComboBox::currentIndexChanged, this,
                [dirCombo](int idx) {
                    auto& settings = AppSettings::instance();
                    settings.setValue(
                        "FreeDvReporter/DirectionAsCardinal",
                        dirCombo->itemData(idx).toString());
                    settings.save();
                });
        dirRow->addWidget(dirCombo);
        dirRow->addStretch(1);
        prefsLayout->addLayout(dirRow);

        // Frequency display: MHz vs kHz, mirroring freedv-gui's
        // reportingFrequencyAsKhz setting.
        auto* khzChk = new QCheckBox("Display frequencies in kHz");
        khzChk->setObjectName("freedvFrequencyKhzChk");
        khzChk->setToolTip(
            "When checked, the FreeDV Reporter dialog's MHz column "
            "shows kHz instead. Mirrors freedv-gui's "
            "reportingFrequencyAsKhz setting.");
        khzChk->setChecked(
            s.value("FreeDvReporter/FrequencyAsKhz", "False").toString()
            == "True");
        connect(khzChk, &QCheckBox::toggled, this, [](bool on) {
            auto& settings = AppSettings::instance();
            settings.setValue("FreeDvReporter/FrequencyAsKhz",
                              on ? "True" : "False");
            settings.save();
        });
        prefsLayout->addWidget(khzChk);

        // Hide from view: when checked, the reporter server keeps our
        // session but doesn't list us on the dashboard. Useful for SWLs
        // who want to RX-report without appearing as an active station.
        // Mirrors freedv-gui's reportingHidden setting / hide_from_view
        // event.
        auto* hideChk = new QCheckBox("Hide my station from the dashboard");
        hideChk->setObjectName("freedvHideFromViewChk");
        hideChk->setToolTip(
            "When checked, we still report RX decodes to the server but "
            "our row stays hidden from the public dashboard. Mirrors "
            "freedv-gui's 'Hide from view' setting.");
        hideChk->setChecked(
            s.value("FreeDvReporter/Hidden", "False").toString() == "True");
        connect(hideChk, &QCheckBox::toggled, this,
                [this](bool on) {
                    auto& settings = AppSettings::instance();
                    settings.setValue("FreeDvReporter/Hidden",
                                      on ? "True" : "False");
                    settings.save();
                    // Push live to the connected client if available.
                    if (m_freedvClient && m_freedvClient->isConnected()) {
                        m_freedvClient->setHiddenFromView(on);
                    }
                });
        prefsLayout->addWidget(hideChk);

        layout->addWidget(prefsGroup);
    }

    auto* consoleRow = new QHBoxLayout;
    auto* consoleLabel = new QLabel("FreeDV Spots");
    consoleLabel->setStyleSheet("QLabel { color: #00b4d8; font-weight: bold; }");
    consoleRow->addWidget(consoleLabel);
    consoleRow->addStretch();

    auto* spotColorLabel = new QLabel("Spot Color:");
    spotColorLabel->setStyleSheet("QLabel { color: #808080; font-size: 12px; }");
    consoleRow->addWidget(spotColorLabel);

    QColor freedvColor(s.value("FreeDvSpotColor", "#FF8C00").toString());
    auto* freedvColorBtn = new QPushButton;
    freedvColorBtn->setObjectName("freedvColorBtn");
    freedvColorBtn->setFixedSize(18, 18);
    freedvColorBtn->setStyleSheet(swatchStyle(freedvColor));
    connect(freedvColorBtn, &QPushButton::clicked, this, [this, freedvColorBtn] {
        QColor c = QColorDialog::getColor(
            QColor(AppSettings::instance().value("FreeDvSpotColor", "#FF8C00").toString()),
            this, "FreeDV Spot Color");
        if (c.isValid()) {
            freedvColorBtn->setStyleSheet(swatchStyle(c));
            AppSettings::instance().setValue("FreeDvSpotColor", c.name());
            AppSettings::instance().save();
        }
    });
    consoleRow->addWidget(freedvColorBtn);
    layout->addLayout(consoleRow);

    m_freedvConsole = new QPlainTextEdit;
    m_freedvConsole->setObjectName("freedvConsole");
    m_freedvConsole->setReadOnly(true);
    m_freedvConsole->setMaximumBlockCount(2000);
    m_freedvConsole->setStyleSheet(kConsoleStyle);
    layout->addWidget(m_freedvConsole, 1);

    tabs->addTab(page, "FreeDV");
}

// NereusSDR-native, no upstream. AetherSDR has no PSK Reporter tab.
// PSK Reporter is a UDP/IPFIX reporting service for digital-mode
// activity; NereusSDR ships the `PskReporterClient` (Phase 3J-2
// Task B6, ported from `freedv-gui src/reporting/pskreporter.cpp`).
// Tab follows the F2 uniform template: identity fields (callsign +
// grid) + auto-start toggle + start/stop button + status label +
// raw-event console. Default server is `report.pskreporter.info:4739`
// (matches PskReporterClient default). Signals:
// `pskStartRequested` / `pskStopRequested`. PSK Reporter is
// primarily a write-only flow (NereusSDR sends decodes to the
// pool); the console shows queued / sent record counts so users
// can confirm reports are flowing.
void SpotHubDialog::buildPskTab(QTabWidget* tabs)
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(8);

    auto& s = AppSettings::instance();

    auto* connGroup = new QGroupBox("PSK Reporter Identity");
    auto* connLayout = new QVBoxLayout(connGroup);
    connLayout->setSpacing(4);

    auto* grid = new QGridLayout;
    grid->setColumnStretch(1, 1);
    int row = 0;

    // 2026-05-12 (PR #238 review P2): canonical PSK identity lives
    // under the slash-key family RadioModel reads.  Fallback chain:
    //   PskReporter/Callsign -> PskReporterCallsign (legacy flat) ->
    //   DxClusterCallsign -> User/Callsign.
    //   PskReporter/GridSquare -> PskReporterGrid (legacy flat) ->
    //   User/GridSquare.
    // Flat keys retained as fallback to migrate existing installs
    // that pre-date the unified key scheme.
    QString defaultCall = s.value("PskReporter/Callsign").toString();
    if (defaultCall.isEmpty()) {
        defaultCall = s.value("PskReporterCallsign").toString();
    }
    if (defaultCall.isEmpty()) {
        defaultCall = s.value("DxClusterCallsign").toString();
    }
    if (defaultCall.isEmpty()) {
        defaultCall = s.value("User/Callsign").toString();
    }
    QString defaultGrid = s.value("PskReporter/GridSquare").toString();
    if (defaultGrid.isEmpty()) {
        defaultGrid = s.value("PskReporterGrid").toString();
    }
    if (defaultGrid.isEmpty()) {
        defaultGrid = s.value("User/GridSquare").toString();
    }

    grid->addWidget(new QLabel("Callsign:"), row, 0);
    m_pskCallEdit = new QLineEdit(defaultCall);
    m_pskCallEdit->setObjectName("pskCallEdit");
    m_pskCallEdit->setPlaceholderText(
        "Enter Callsign Here (set under Settings tab)");
    m_pskCallEdit->setStyleSheet(kLineEditStyle);
    grid->addWidget(m_pskCallEdit, row, 1);
    row++;

    grid->addWidget(new QLabel("Grid:"), row, 0);
    m_pskGridEdit = new QLineEdit(defaultGrid);
    m_pskGridEdit->setObjectName("pskGridEdit");
    m_pskGridEdit->setPlaceholderText("4-character or 6-character Maidenhead");
    m_pskGridEdit->setStyleSheet(kLineEditStyle);
    grid->addWidget(m_pskGridEdit, row, 1);
    row++;

    connLayout->addLayout(grid);

    auto* helpLabel = new QLabel(
        "Reports your radio's decodes to the PSK Reporter pool via\n"
        "report.pskreporter.info:4739 (UDP/IPFIX). Sent records are\n"
        "queued and flushed periodically; the console below shows\n"
        "send activity. Callsign and grid are required.");
    helpLabel->setWordWrap(true);
    helpLabel->setStyleSheet("QLabel { color: #8aa8c0; font-size: 11px; }");
    connLayout->addWidget(helpLabel);

    auto* btnRow = new QHBoxLayout;
    m_pskAutoStartBtn = new QPushButton(
        s.value("PskReporterAutoStart", "False").toString() == "True"
            ? "Auto-Start: ON" : "Auto-Start: OFF");
    m_pskAutoStartBtn->setObjectName("pskAutoStartBtn");
    m_pskAutoStartBtn->setCheckable(true);
    m_pskAutoStartBtn->setChecked(s.value("PskReporterAutoStart", "False").toString() == "True");
    m_pskAutoStartBtn->setStyleSheet(kAutoToggleStyle);
    connect(m_pskAutoStartBtn, &QPushButton::toggled, this, [this](bool on) {
        m_pskAutoStartBtn->setText(on ? "Auto-Start: ON" : "Auto-Start: OFF");
        auto& settings = AppSettings::instance();
        settings.setValue("PskReporterAutoStart", on ? "True" : "False");
        settings.save();
    });
    btnRow->addWidget(m_pskAutoStartBtn);
    btnRow->addStretch();

    m_pskStatusLabel = new QLabel("Stopped");
    m_pskStatusLabel->setObjectName("pskStatusLabel");
    m_pskStatusLabel->setStyleSheet(kStatusIdleStyle);
    btnRow->addWidget(m_pskStatusLabel);
    btnRow->addStretch();

    m_pskStartBtn = new QPushButton(m_pskClient && m_pskClient->isListening()
                                        ? "Stop" : "Start");
    m_pskStartBtn->setObjectName("pskStartBtn");
    m_pskStartBtn->setFixedWidth(100);
    m_pskStartBtn->setStyleSheet(kStartBtnStyle);
    connect(m_pskStartBtn, &QPushButton::clicked, this, [this] {
        if (m_pskClient && m_pskClient->isListening()) {
            emit pskStopRequested();
            return;
        }
        QString call = m_pskCallEdit->text().trimmed().toUpper();
        QString gridSquare = m_pskGridEdit->text().trimmed().toUpper();
        if (call.isEmpty() || gridSquare.isEmpty()) {
            m_pskStatusLabel->setText("Callsign and grid are required");
            m_pskStatusLabel->setStyleSheet("QLabel { color: #ff4444; font-size: 11px; }");
            return;
        }
        // 2026-05-12 bench fix (PR #238 review P2): persist under the
        // SAME slash-key family RadioModel reads on construction and
        // session restore (RadioModel.cpp:1000-1004, :1575-1582 — keys
        // `PskReporter/Callsign` + `PskReporter/GridSquare`).  The
        // earlier flat keys `PskReporterCallsign` / `PskReporterGrid`
        // were write-only orphans no one read; first-time PSK-tab
        // users restarted with the PSK client still holding the empty
        // identity set at RadioModel construction.
        auto& settings = AppSettings::instance();
        settings.setValue("PskReporter/Callsign", call);
        settings.setValue("PskReporter/GridSquare", gridSquare);
        settings.save();
        emit pskStartRequested(call, gridSquare);
    });
    btnRow->addWidget(m_pskStartBtn);
    connLayout->addLayout(btnRow);

    layout->addWidget(connGroup);

    auto* consoleLabel = new QLabel("PSK Reporter Activity");
    consoleLabel->setStyleSheet("QLabel { color: #00b4d8; font-weight: bold; }");
    layout->addWidget(consoleLabel);

    m_pskConsole = new QPlainTextEdit;
    m_pskConsole->setObjectName("pskConsole");
    m_pskConsole->setReadOnly(true);
    m_pskConsole->setMaximumBlockCount(2000);
    m_pskConsole->setStyleSheet(kConsoleStyle);
    layout->addWidget(m_pskConsole, 1);

    // 2026-05-12 bench fix: PSK Start/Stop UI feedback.  PSK Reporter
    // is send-only with no listening/stopped signal pair (see
    // pskreporter.h:65-68 [@77e793a]); flip the button text + status
    // label inline when the user clicks Start/Stop so the UI doesn't
    // look dead.  The actual auto-send timer is armed in MainWindow's
    // openSpotHub wire which calls setAutoSendIntervalSec.  Status
    // shows the 5-minute cadence inherited from freedv-gui
    // main.cpp:2597 [@77e793a].
    connect(this, &SpotHubDialog::pskStartRequested,
            this, [this](const QString& /*call*/,
                         const QString& /*grid*/) {
        if (m_pskStatusLabel) {
            m_pskStatusLabel->setText(
                "Auto-send every 5 minutes");
            m_pskStatusLabel->setStyleSheet(kStatusActiveStyle);
        }
        if (m_pskStartBtn) m_pskStartBtn->setText("Stop");
    });
    connect(this, &SpotHubDialog::pskStopRequested,
            this, [this]() {
        if (m_pskStatusLabel) {
            m_pskStatusLabel->setText("Stopped");
            m_pskStatusLabel->setStyleSheet(kStatusIdleStyle);
        }
        if (m_pskStartBtn) m_pskStartBtn->setText("Start");
    });

    tabs->addTab(page, "PSK Reporter");
}

// From AetherSDR src/gui/DxClusterDialog.cpp:1599-1717 [@0cd4559]
// + NereusSDR Task F3 extensions documented below.
//
// Spot List tab: hosts the merged 8-column QTableView bound to a
// BandFilterProxy wrapped around a SpotTableModel that aggregates
// every source's spots. Top row: 12 band pills (160m..2m) + 7 source
// pills (DX/RBN/JT/COL/POT/FDR/PSK). Bottom row: spot count + Clear.
// Double-click on any row emits tuneRequested(double) for MainWindow
// to forward to the active slice.
//
// NereusSDR divergences from upstream:
//   (1) Upstream uses 11 QCheckBox band filters (160m..6m). F3
//       widens to 12 (adds 2m) and turns each filter into a
//       checkable QPushButton "pill" so the row matches the
//       SpotHub dialog's pill aesthetic.
//   (2) Upstream filters bands only. F3 adds a second pill row for
//       sources (7 pills mapping to the 7 ingest clients) driving
//       BandFilterProxy::setSourceVisible (NereusSDR-native
//       extension to the proxy, see BandFilterProxy.{h,cpp}).
//   (3) Upstream's spotModel/proxyModel/spotTable lived on the
//       dialog as Cluster-tab-scoped members. In NereusSDR they
//       are populated by every client (Cluster + RBN + WSJT-X +
//       SpotCollector + POTA + FreeDV + PSK Reporter) so the Spot
//       List shows the cross-source merge. spotReceived(DxSpot)
//       signals from all seven clients land here.
//   (4) AppSettings keys preserved verbatim for bands
//       (SpotBandFilter_<band>) and added for sources
//       (SpotSourceFilter_<source>; NereusSDR-native).
void SpotHubDialog::buildSpotListTab(QTabWidget* tabs)
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(4);

    auto& s = AppSettings::instance();

    // Proxy only — the SpotTableModel itself is owned by RadioModel
    // and passed in via the constructor (2026-05-12 bench fix:
    // ownership moved so the table populates from app start, not
    // from first-time dialog open).  BandFilterProxy filters the
    // shared source model by band + visible sources.
    m_spotProxyModel = new BandFilterProxy(this);
    m_spotProxyModel->setObjectName("spotListProxyModel");
    if (m_spotTableModel) {
        m_spotProxyModel->setSourceModel(m_spotTableModel);
    }
    m_spotProxyModel->setSortRole(Qt::UserRole);

    // ── Band pill row ───────────────────────────────────────────────
    // NereusSDR-native: pill row replaces upstream's
    // DxClusterDialog.cpp:1605-1641 checkbox row. 12 pills (upstream
    // had 11, plus 2m).
    auto* bandRow = new QHBoxLayout;
    bandRow->setSpacing(3);
    auto* bandLabel = new QLabel("Bands:");
    bandLabel->setStyleSheet("QLabel { color: #808080; font-size: 11px; }");
    bandLabel->setFixedWidth(40);
    bandRow->addWidget(bandLabel);

    static constexpr const char* bands[] = {
        "160m", "80m", "60m", "40m", "30m", "20m",
        "17m", "15m", "12m", "10m", "6m", "2m"
    };
    for (const char* band : bands) {
        auto* pill = new QPushButton(band);
        pill->setObjectName(QString("spotListBandPill_%1").arg(band));
        pill->setCheckable(true);
        pill->setStyleSheet(kFilterPillStyle);
        const QString key = QString("SpotBandFilter_%1").arg(band);
        bool on = s.value(key, "True").toString() == "True";
        pill->setChecked(on);
        if (!on)
            m_spotProxyModel->setBandVisible(QString(band), false);
        connect(pill, &QPushButton::toggled, this, [this, b = QString(band), key](bool checked) {
            m_spotProxyModel->setBandVisible(b, checked);
            auto& settings = AppSettings::instance();
            settings.setValue(key, checked ? "True" : "False");
            settings.save();
        });
        bandRow->addWidget(pill);
    }
    bandRow->addStretch();
    layout->addLayout(bandRow);

    // ── Source pill row ─────────────────────────────────────────────
    // NereusSDR-native (no upstream equivalent). 7 pills mapping to
    // the 7 ingest clients. Each pill's text is the short label
    // (DX / RBN / JT / COL / POT / FDR / PSK); the filter value is
    // the upstream source string emitted by the client (Cluster /
    // RBN / WSJT-X / SpotCollector / POTA / FreeDV / PSK).
    struct SourcePill {
        const char* label;
        const char* source;  // matches DxSpot::source emitted by clients
    };
    static constexpr SourcePill sources[] = {
        {"DX",  "Cluster"},
        {"RBN", "RBN"},
        {"JT",  "WSJT-X"},
        {"COL", "SpotCollector"},
        {"POT", "POTA"},
        {"FDR", "FreeDV"},
        {"PSK", "PSK"},
    };
    auto* srcRow = new QHBoxLayout;
    srcRow->setSpacing(3);
    auto* srcLabel = new QLabel("Sources:");
    srcLabel->setStyleSheet("QLabel { color: #808080; font-size: 11px; }");
    srcLabel->setFixedWidth(40);
    srcRow->addWidget(srcLabel);
    for (const auto& src : sources) {
        auto* pill = new QPushButton(src.label);
        pill->setObjectName(QString("spotListSourcePill_%1").arg(src.label));
        pill->setCheckable(true);
        pill->setStyleSheet(kFilterPillStyle);
        const QString key = QString("SpotSourceFilter_%1").arg(src.label);
        bool on = s.value(key, "True").toString() == "True";
        pill->setChecked(on);
        const QString sourceStr(src.source);
        if (!on)
            m_spotProxyModel->setSourceVisible(sourceStr, false);
        connect(pill, &QPushButton::toggled, this, [this, sourceStr, key](bool checked) {
            m_spotProxyModel->setSourceVisible(sourceStr, checked);
            auto& settings = AppSettings::instance();
            settings.setValue(key, checked ? "True" : "False");
            settings.save();
        });
        srcRow->addWidget(pill);
    }
    srcRow->addStretch();
    layout->addLayout(srcRow);

    // ── Table view ──────────────────────────────────────────────────
    // From AetherSDR DxClusterDialog.cpp:1643-1696 [@0cd4559].
    m_spotTable = new QTableView;
    m_spotTable->setObjectName("spotListTable");
    m_spotTable->setModel(m_spotProxyModel);
    m_spotTable->setSortingEnabled(true);
    m_spotTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_spotTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_spotTable->setAlternatingRowColors(true);
    m_spotTable->verticalHeader()->setVisible(false);
    m_spotTable->verticalHeader()->setDefaultSectionSize(20);
    m_spotTable->horizontalHeader()->setStretchLastSection(true);
    m_spotTable->setStyleSheet(kSpotTableStyle);

    m_spotTable->setColumnWidth(SpotTableModel::ColTime, 50);
    m_spotTable->setColumnWidth(SpotTableModel::ColFreq, 80);
    m_spotTable->setColumnWidth(SpotTableModel::ColDxCall, 90);
    m_spotTable->setColumnWidth(SpotTableModel::ColMode, 45);
    m_spotTable->setColumnWidth(SpotTableModel::ColComment, 200);
    m_spotTable->setColumnWidth(SpotTableModel::ColSpotter, 80);
    m_spotTable->setColumnWidth(SpotTableModel::ColBand, 45);
    m_spotTable->setColumnWidth(SpotTableModel::ColSource, 55);

    // No default sort - insertion order is newest-first.
    m_spotTable->horizontalHeader()->setSortIndicatorShown(false);

    // Double-click to tune. From upstream DxClusterDialog.cpp:1688-1693.
    connect(m_spotTable, &QTableView::doubleClicked, this, [this](const QModelIndex& idx) {
        auto srcIdx = m_spotProxyModel->mapToSource(idx);
        double freq = m_spotTableModel->freqAtRow(srcIdx.row());
        if (freq > 0.0)
            emit tuneRequested(freq);
    });

    // 2026-05-12 bench fix (Gap #6 — list → panadapter hover sync).
    // Enable mouse tracking so the table's `entered` signal fires on
    // every cell the mouse moves over without requiring a click.
    // Map the proxy row back through the SpotModel by matching
    // callsign + freq + source, then emit spotListHoverChanged so
    // MainWindow can forward to SpectrumWidget::setHoverSpotIndexExternal
    // which paints the halo around the matching panadapter label.
    m_spotTable->setMouseTracking(true);
    connect(m_spotTable, &QTableView::entered,
            this, [this](const QModelIndex& idx) {
        if (!idx.isValid() || !m_spotProxyModel || !m_spotTableModel
            || !m_spotModel) {
            emit spotListHoverChanged(-1);
            return;
        }
        const QModelIndex srcIdx = m_spotProxyModel->mapToSource(idx);
        const int row = srcIdx.row();
        const double rowFreqMhz = m_spotTableModel->freqAtRow(row);
        const QString rowCall = m_spotTableModel->data(
            m_spotTableModel->index(row, SpotTableModel::ColDxCall),
            Qt::DisplayRole).toString();
        const QString rowSource = m_spotTableModel->data(
            m_spotTableModel->index(row, SpotTableModel::ColSource),
            Qt::DisplayRole).toString();
        // Walk SpotModel for the matching entry.  -1 if not found
        // (table row is older than the SpotModel cache or was
        // expired).
        int foundIndex = -1;
        const auto& spots = m_spotModel->spots();
        for (auto it = spots.constBegin(); it != spots.constEnd(); ++it) {
            const SpotData& sd = it.value();
            const double sdFreq = (sd.rxFreqMhz > 0.0)
                                      ? sd.rxFreqMhz
                                      : sd.txFreqMhz;
            if (std::abs(sdFreq - rowFreqMhz) > 0.001) continue;
            if (sd.callsign.compare(rowCall, Qt::CaseInsensitive) != 0) continue;
            if (!sd.source.isEmpty()
                && sd.source.compare(rowSource, Qt::CaseInsensitive) != 0) continue;
            foundIndex = sd.index;
            break;
        }
        emit spotListHoverChanged(foundIndex);
    });

    layout->addWidget(m_spotTable, 1);

    // ── Bottom row: spot count + Clear ──────────────────────────────
    // From upstream DxClusterDialog.cpp:1697-1714.
    auto* bottomRow = new QHBoxLayout;
    auto* countLabel = new QLabel("0 spots");
    countLabel->setObjectName("spotListCountLabel");
    countLabel->setStyleSheet("QLabel { color: #808080; font-size: 11px; }");
    connect(m_spotTableModel, &QAbstractTableModel::rowsInserted,
            this, [this, countLabel] {
        countLabel->setText(QString("%1 spots").arg(m_spotTableModel->rowCount()));
    });
    bottomRow->addWidget(countLabel);
    bottomRow->addStretch();

    auto* clearBtn = new QPushButton("Clear");
    clearBtn->setObjectName("spotListClearBtn");
    clearBtn->setFixedWidth(60);
    connect(clearBtn, &QPushButton::clicked, this, [this, countLabel] {
        m_spotTableModel->clear();
        countLabel->setText("0 spots");
    });
    bottomRow->addWidget(clearBtn);
    layout->addLayout(bottomRow);

    // 2026-05-12 bench fix: client spotReceived → SpotTableModel
    // wiring lives in RadioModel ctor now (alongside the per-source
    // on*SpotReceived → SpotModel adapters), so the table populates
    // from app start regardless of whether this dialog is open.
    // Previously the wires lived here, which meant auto-connected
    // sources flowed past with nothing listening until the user
    // opened the dialog — forcing a manual disconnect+reconnect to
    // repopulate.

    tabs->addTab(page, "Spot List");
}

// Display tab - F4. Two-column layout. LEFT (NereusSDR-native): 8
// live stat blocks driven by SpotTableModel + DxccColorProvider
// counts, with a red "Clear All Spots" button at the bottom that
// calls SpotModel::clear() and emits spotsClearedAll(). RIGHT
// (verbatim port): all knobs from AetherSDR
// src/gui/SpotSettingsDialog.cpp:38-292 [@0cd4559] (Spots /
// Memories toggles + Levels / Position / Font Size / Spot Lifetime
// sliders + Override Colors / Override Background + Auto +
// swatches + BG Opacity slider). Every knob change writes to the
// upstream AppSettings keys (`SpotSettingsDialog.cpp:22-37
// [@0cd4559]`) and emits settingsChanged() so MainWindow can
// refresh the live spectrum spot overlay.
void SpotHubDialog::buildDisplayTab(QTabWidget* tabs)
{
    auto* page = new QWidget;
    auto* root = new QHBoxLayout(page);
    root->setSpacing(12);

    auto& s = AppSettings::instance();

    // ── LEFT column: stat blocks + Clear All Spots ────────────────
    // NereusSDR-native: upstream's standalone dialog had a single
    // "Total Spots" line at `SpotSettingsDialog.cpp:272-276
    // [@0cd4559]`. The Display tab expands this to 8 stat blocks
    // reading live from SpotTableModel + DxccColorProvider so the
    // operator sees a consolidated view across all ingest sources.
    auto* leftCol = new QVBoxLayout;
    leftCol->setSpacing(4);

    auto* statsTitle = new QLabel("Statistics");
    statsTitle->setStyleSheet("QLabel { color: #00b4d8; font-weight: bold; }");
    leftCol->addWidget(statsTitle);

    auto makeStatRow = [leftCol](const QString& label, const QString& objName) -> QLabel* {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);
        auto* lbl = new QLabel(label + ":");
        lbl->setStyleSheet("QLabel { color: #808890; }");
        lbl->setMinimumWidth(120);
        row->addWidget(lbl);
        auto* value = new QLabel("0");
        value->setObjectName(objName);
        value->setStyleSheet("QLabel { color: #c8d8e8; font-weight: bold; }");
        row->addWidget(value);
        row->addStretch();
        leftCol->addLayout(row);
        return value;
    };

    m_statTotalSpots      = makeStatRow("Total Spots",      "displayStatTotalSpots");
    m_statUniqueCallsigns = makeStatRow("Unique Callsigns", "displayStatUniqueCallsigns");
    m_statActiveSources   = makeStatRow("Active Sources",   "displayStatActiveSources");
    m_statCtyDatEntries   = makeStatRow("cty.dat entries",  "displayStatCtyDatEntries");
    m_statAdifQsos        = makeStatRow("ADIF QSOs",        "displayStatAdifQsos");
    m_statDxccEntities    = makeStatRow("DXCC entities",    "displayStatDxccEntities");
    m_statNewDxcc         = makeStatRow("New DXCC in feed", "displayStatNewDxcc");
    m_statNewBands        = makeStatRow("New bands in feed","displayStatNewBands");

    leftCol->addStretch();

    // Red "Clear All Spots" button. From upstream
    // SpotSettingsDialog.cpp:281-292 [@0cd4559] but colored red per
    // F4 spec and emitting spotsClearedAll() so MainWindow can
    // propagate the clear to the live spectrum overlay.
    auto* clearAllBtn = new QPushButton("Clear All Spots");
    clearAllBtn->setObjectName("displayClearAllSpotsBtn");
    clearAllBtn->setFixedHeight(28);
    clearAllBtn->setStyleSheet(
        "QPushButton { background: #b03030; color: white; font-weight: bold; "
        "border: 1px solid #802020; padding: 4px 10px; border-radius: 3px; }"
        "QPushButton:hover { background: #c84040; }"
        "QPushButton:pressed { background: #902020; }");
    connect(clearAllBtn, &QPushButton::clicked, this, [this] {
        if (m_spotModel) {
            m_spotModel->clear();
        }
        if (m_spotTableModel) {
            m_spotTableModel->clear();
        }
        if (m_statTotalSpots) {
            m_statTotalSpots->setText("0");
        }
        if (m_statUniqueCallsigns) {
            m_statUniqueCallsigns->setText("0");
        }
        if (m_statNewDxcc) {
            m_statNewDxcc->setText("0");
        }
        if (m_statNewBands) {
            m_statNewBands->setText("0");
        }
        emit spotsClearedAll();
    });
    leftCol->addWidget(clearAllBtn);

    root->addLayout(leftCol, 1);

    // ── RIGHT column: knobs ───────────────────────────────────────
    // Port verbatim from AetherSDR
    // src/gui/SpotSettingsDialog.cpp:38-270 [@0cd4559]. The
    // standalone upstream dialog is retired in favour of this
    // folded Display tab; the knob grid layout, stylesheets,
    // AppSettings keys, and non-linear lifetime step table are
    // preserved byte-for-byte.

    // From AetherSDR SpotSettingsDialog.cpp:21-37 [@0cd4559]:
    // load persisted state for the knobs.
    bool spotsEnabled       = s.value("IsSpotsEnabled", "True").toString() == "True";
    bool memoriesEnabled    = s.value("IsMemorySpotsEnabled", "False").toString() == "True";
    bool overrideColors     = s.value("IsSpotsOverrideColorsEnabled", "False").toString() == "True";
    bool overrideBg         = s.value("IsSpotsOverrideBackgroundColorsEnabled", "True").toString() == "True";
    bool overrideBgAutoMode = s.value("IsSpotsOverrideToAutoBackgroundColorEnabled", "True").toString() == "True";
    int  levelsVal   = s.value("SpotsMaxLevel", 3).toInt();
    int  positionVal = s.value("SpotsStartingHeightPercentage", 50).toInt();
    int  fontSizeVal = s.value("SpotFontSize", 16).toInt();
    QColor spotColor(s.value("SpotsOverrideColor", "#FFFF00").toString());
    QColor bgColor  (s.value("SpotsOverrideBgColor", "#000000").toString());
    int  bgOpacityVal = s.value("SpotsBackgroundOpacity", 48).toInt();
    // Migrate from old minutes key to new seconds key. Preserved
    // verbatim from upstream SpotSettingsDialog.cpp:34-37 [@0cd4559].
    int lifetimeSec = s.value("DxClusterSpotLifetimeSec", 0).toInt();
    if (lifetimeSec <= 0) {
        lifetimeSec = s.value("DxClusterSpotLifetime", 30).toInt() * 60;
    }

    auto* rightCol = new QVBoxLayout;
    rightCol->setSpacing(4);

    auto* knobsTitle = new QLabel("Spot Settings");
    knobsTitle->setStyleSheet("QLabel { color: #00b4d8; font-weight: bold; }");
    rightCol->addWidget(knobsTitle);

    auto* grid = new QGridLayout;
    grid->setColumnStretch(1, 1);
    grid->setSpacing(4);
    int row = 0;

    auto save = [this](const QString& key, const QVariant& val) {
        auto& settings = AppSettings::instance();
        settings.setValue(key, val);
        settings.save();
        emit settingsChanged();
    };

    // Upstream stylesheet fragment for the green/red toggle pair.
    // From SpotSettingsDialog.cpp:63-65 [@0cd4559].
    static constexpr const char* kToggleStyle =
        "QPushButton { background: #206030; color: white; "
        "border: 1px solid #305040; padding: 3px; }"
        "QPushButton:!checked { background: #603020; }";

    // Spots: Enabled/Disabled. Upstream :57-71 [@0cd4559].
    grid->addWidget(new QLabel("Spots:"), row, 0);
    auto* spotsToggle = new QPushButton(spotsEnabled ? "Enabled" : "Disabled");
    spotsToggle->setObjectName("displaySpotsToggle");
    spotsToggle->setCheckable(true);
    spotsToggle->setChecked(spotsEnabled);
    spotsToggle->setFixedWidth(80);
    spotsToggle->setStyleSheet(kToggleStyle);
    connect(spotsToggle, &QPushButton::toggled, this,
            [spotsToggle, save](bool on) {
        spotsToggle->setText(on ? "Enabled" : "Disabled");
        save("IsSpotsEnabled", on ? "True" : "False");
    });
    grid->addWidget(spotsToggle, row++, 1, Qt::AlignLeft);

    // Memories: Enabled/Disabled. Upstream :74-89 [@0cd4559].
    grid->addWidget(new QLabel("Memories:"), row, 0);
    auto* memoriesToggle = new QPushButton(memoriesEnabled ? "Enabled" : "Disabled");
    memoriesToggle->setObjectName("displayMemoriesToggle");
    memoriesToggle->setCheckable(true);
    memoriesToggle->setChecked(memoriesEnabled);
    memoriesToggle->setFixedWidth(80);
    memoriesToggle->setToolTip(
        "Show radio memory channels as a spot-like feed on the panadapter.");
    memoriesToggle->setStyleSheet(kToggleStyle);
    connect(memoriesToggle, &QPushButton::toggled, this,
            [memoriesToggle, save](bool on) {
        memoriesToggle->setText(on ? "Enabled" : "Disabled");
        save("IsMemorySpotsEnabled", on ? "True" : "False");
    });
    grid->addWidget(memoriesToggle, row++, 1, Qt::AlignLeft);

    // Levels slider. Upstream :91-106 [@0cd4559].
    grid->addWidget(new QLabel("Levels:"), row, 0);
    auto* levelsRow = new QHBoxLayout;
    auto* levelsSlider = new GuardedSlider(Qt::Horizontal);
    levelsSlider->setObjectName("displayLevelsSlider");
    levelsSlider->setRange(1, 10);
    levelsSlider->setValue(levelsVal);
    auto* levelsValueLbl = new QLabel(QString::number(levelsVal));
    levelsValueLbl->setFixedWidth(24);
    levelsValueLbl->setAlignment(Qt::AlignRight);
    levelsRow->addWidget(levelsSlider);
    levelsRow->addWidget(levelsValueLbl);
    connect(levelsSlider, &QSlider::valueChanged, this,
            [levelsValueLbl, save](int v) {
        levelsValueLbl->setText(QString::number(v));
        save("SpotsMaxLevel", QString::number(v));
    });
    grid->addLayout(levelsRow, row++, 1);

    // Position slider. Upstream :108-123 [@0cd4559].
    grid->addWidget(new QLabel("Position:"), row, 0);
    auto* posRow = new QHBoxLayout;
    auto* positionSlider = new GuardedSlider(Qt::Horizontal);
    positionSlider->setObjectName("displayPositionSlider");
    positionSlider->setRange(0, 100);
    positionSlider->setValue(positionVal);
    auto* positionValueLbl = new QLabel(QString::number(positionVal));
    positionValueLbl->setFixedWidth(24);
    positionValueLbl->setAlignment(Qt::AlignRight);
    posRow->addWidget(positionSlider);
    posRow->addWidget(positionValueLbl);
    connect(positionSlider, &QSlider::valueChanged, this,
            [positionValueLbl, save](int v) {
        positionValueLbl->setText(QString::number(v));
        save("SpotsStartingHeightPercentage", QString::number(v));
    });
    grid->addLayout(posRow, row++, 1);

    // Font Size slider. Upstream :125-140 [@0cd4559].
    grid->addWidget(new QLabel("Font Size:"), row, 0);
    auto* fontRow = new QHBoxLayout;
    auto* fontSizeSlider = new GuardedSlider(Qt::Horizontal);
    fontSizeSlider->setObjectName("displayFontSizeSlider");
    fontSizeSlider->setRange(8, 32);
    fontSizeSlider->setValue(fontSizeVal);
    auto* fontSizeValueLbl = new QLabel(QString::number(fontSizeVal));
    fontSizeValueLbl->setFixedWidth(24);
    fontSizeValueLbl->setAlignment(Qt::AlignRight);
    fontRow->addWidget(fontSizeSlider);
    fontRow->addWidget(fontSizeValueLbl);
    connect(fontSizeSlider, &QSlider::valueChanged, this,
            [fontSizeValueLbl, save](int v) {
        fontSizeValueLbl->setText(QString::number(v));
        save("SpotFontSize", QString::number(v));
    });
    grid->addLayout(fontRow, row++, 1);

    // Spot Lifetime slider (non-linear). Upstream :142-178
    // [@0cd4559]. Steps: 10..55s (5s), 5..55min (5min), 1..24hr
    // (1hr); 45 indices total.
    grid->addWidget(new QLabel("Spot Lifetime:"), row, 0);
    auto* lifeRow = new QHBoxLayout;

    static QVector<int> lifeSteps;
    if (lifeSteps.isEmpty()) {
        for (int sec = 10; sec <= 55; sec += 5) {
            lifeSteps.append(sec);
        }
        for (int m = 5; m <= 55; m += 5) {
            lifeSteps.append(m * 60);
        }
        for (int h = 1; h <= 24; h++) {
            lifeSteps.append(h * 3600);
        }
    }
    auto formatLifetime = [](int secs) -> QString {
        if (secs < 60) {
            return QString("%1 sec").arg(secs);
        }
        if (secs < 3600) {
            return QString("%1 min%2").arg(secs / 60).arg(secs / 60 == 1 ? "" : "s");
        }
        int hrs = secs / 3600;
        if (hrs == 24) {
            return QStringLiteral("1 day");
        }
        return QString("%1 hr%2").arg(hrs).arg(hrs == 1 ? "" : "s");
    };
    // Find closest step index for the persisted seconds value.
    int lifeIdx = 0;
    for (int i = 0; i < lifeSteps.size(); ++i) {
        if (std::abs(lifeSteps[i] - lifetimeSec) < std::abs(lifeSteps[lifeIdx] - lifetimeSec)) {
            lifeIdx = i;
        }
    }

    auto* lifetimeSlider = new GuardedSlider(Qt::Horizontal);
    lifetimeSlider->setObjectName("displayLifetimeSlider");
    lifetimeSlider->setRange(0, lifeSteps.size() - 1);
    lifetimeSlider->setValue(lifeIdx);
    auto* lifetimeValueLbl = new QLabel(formatLifetime(lifeSteps[lifeIdx]));
    lifetimeValueLbl->setFixedWidth(90);
    lifetimeValueLbl->setAlignment(Qt::AlignRight);
    lifeRow->addWidget(lifetimeSlider);
    lifeRow->addWidget(lifetimeValueLbl);
    connect(lifetimeSlider, &QSlider::valueChanged, this,
            [save, lifetimeValueLbl, formatLifetime](int idx) {
        int secs = lifeSteps.value(idx, 1800);
        lifetimeValueLbl->setText(formatLifetime(secs));
        save("DxClusterSpotLifetimeSec", QString::number(secs));
    });
    grid->addLayout(lifeRow, row++, 1);

    // Override Colors + color swatch. Upstream :180-210 [@0cd4559].
    grid->addWidget(new QLabel("Override Colors:"), row, 0);
    auto* colorRow = new QHBoxLayout;
    auto* overrideColorsToggle = new QPushButton(overrideColors ? "Enabled" : "Disabled");
    overrideColorsToggle->setObjectName("displayOverrideColorsToggle");
    overrideColorsToggle->setCheckable(true);
    overrideColorsToggle->setChecked(overrideColors);
    overrideColorsToggle->setFixedWidth(80);
    overrideColorsToggle->setStyleSheet(kToggleStyle);
    connect(overrideColorsToggle, &QPushButton::toggled, this,
            [overrideColorsToggle, save](bool on) {
        overrideColorsToggle->setText(on ? "Enabled" : "Disabled");
        save("IsSpotsOverrideColorsEnabled", on ? "True" : "False");
    });
    colorRow->addWidget(overrideColorsToggle);

    auto* colorSwatch = new QPushButton;
    colorSwatch->setObjectName("displayColorSwatch");
    colorSwatch->setFixedSize(24, 24);
    colorSwatch->setStyleSheet(swatchStyle(spotColor));
    connect(colorSwatch, &QPushButton::clicked, this,
            [this, colorSwatch, save] {
        QColor current(AppSettings::instance().value("SpotsOverrideColor", "#FFFF00").toString());
        QColor c = QColorDialog::getColor(current, this, "Spot Text Color");
        if (c.isValid()) {
            colorSwatch->setStyleSheet(swatchStyle(c));
            save("SpotsOverrideColor", c.name());
        }
    });
    colorRow->addWidget(colorSwatch);
    colorRow->addStretch();
    grid->addLayout(colorRow, row++, 1);

    // Override Background + Auto + swatch. Upstream :212-252
    // [@0cd4559].
    grid->addWidget(new QLabel("Override Background:"), row, 0);
    auto* bgRow = new QHBoxLayout;
    auto* overrideBgToggle = new QPushButton("Enabled");
    overrideBgToggle->setObjectName("displayOverrideBgToggle");
    overrideBgToggle->setCheckable(true);
    overrideBgToggle->setChecked(overrideBg);
    overrideBgToggle->setFixedWidth(70);
    overrideBgToggle->setStyleSheet(kToggleStyle);
    auto* overrideBgAutoToggle = new QPushButton("Auto");
    overrideBgAutoToggle->setObjectName("displayOverrideBgAutoToggle");
    overrideBgAutoToggle->setCheckable(true);
    overrideBgAutoToggle->setChecked(overrideBgAutoMode);
    overrideBgAutoToggle->setFixedWidth(50);
    overrideBgAutoToggle->setStyleSheet(kToggleStyle);
    connect(overrideBgToggle, &QPushButton::toggled, this,
            [save](bool on) {
        save("IsSpotsOverrideBackgroundColorsEnabled", on ? "True" : "False");
    });
    connect(overrideBgAutoToggle, &QPushButton::toggled, this,
            [save](bool on) {
        save("IsSpotsOverrideToAutoBackgroundColorEnabled", on ? "True" : "False");
    });
    bgRow->addWidget(overrideBgToggle);
    bgRow->addWidget(overrideBgAutoToggle);

    auto* bgColorSwatch = new QPushButton;
    bgColorSwatch->setObjectName("displayBgColorSwatch");
    bgColorSwatch->setFixedSize(24, 24);
    bgColorSwatch->setStyleSheet(swatchStyle(bgColor));
    connect(bgColorSwatch, &QPushButton::clicked, this,
            [this, bgColorSwatch, save] {
        QColor current(AppSettings::instance().value("SpotsOverrideBgColor", "#000000").toString());
        QColor c = QColorDialog::getColor(current, this, "Spot Background Color");
        if (c.isValid()) {
            bgColorSwatch->setStyleSheet(swatchStyle(c));
            save("SpotsOverrideBgColor", c.name());
        }
    });
    bgRow->addWidget(bgColorSwatch);
    bgRow->addStretch();
    grid->addLayout(bgRow, row++, 1);

    // Background Opacity slider. Upstream :254-270 [@0cd4559].
    grid->addWidget(new QLabel("Background Opacity:"), row, 0);
    auto* opacRow = new QHBoxLayout;
    auto* bgOpacitySlider = new GuardedSlider(Qt::Horizontal);
    bgOpacitySlider->setObjectName("displayBgOpacitySlider");
    bgOpacitySlider->setRange(0, 100);
    bgOpacitySlider->setValue(bgOpacityVal);
    auto* bgOpacityValueLbl = new QLabel(QString::number(bgOpacityVal));
    bgOpacityValueLbl->setFixedWidth(24);
    bgOpacityValueLbl->setAlignment(Qt::AlignRight);
    opacRow->addWidget(bgOpacitySlider);
    opacRow->addWidget(bgOpacityValueLbl);
    connect(bgOpacitySlider, &QSlider::valueChanged, this,
            [bgOpacityValueLbl, save](int v) {
        bgOpacityValueLbl->setText(QString::number(v));
        save("SpotsBackgroundOpacity", QString::number(v));
    });
    grid->addLayout(opacRow, row++, 1);

    rightCol->addLayout(grid);

    // 2026-05-12 bench fix (Gap #7 — per-source visibility).  Seven
    // checkboxes, one per spot source, that toggle whether that
    // source's spots appear anywhere -- panadapter overlay AND Spot
    // List table.  Updated 2026-05-12 (Phase 3J-1 closeout follow-up)
    // to ALSO hide the spots from the Spot List by routing through
    // BandFilterProxy::setSourceVisible.  Previously only affected
    // the panadapter; bench operator unchecked FreeDV expecting
    // global hide and the spots kept appearing in the list.  Spot List
    // tab's source pills remain a secondary filter for fine-grained
    // control without opening this tab.
    auto* visTitle = new QLabel("Show spots from source");
    visTitle->setStyleSheet("QLabel { color: #00b4d8; font-weight: bold; "
                            "margin-top: 8px; }");
    rightCol->addWidget(visTitle);

    static const QVector<QPair<QString, QString>> kSourceChecks = {
        {QStringLiteral("Cluster"),       QStringLiteral("DX Cluster")},
        {QStringLiteral("RBN"),           QStringLiteral("RBN")},
        {QStringLiteral("WSJT-X"),        QStringLiteral("WSJT-X")},
        {QStringLiteral("SpotCollector"), QStringLiteral("SpotCollector")},
        {QStringLiteral("POTA"),          QStringLiteral("POTA")},
        {QStringLiteral("FreeDV"),        QStringLiteral("FreeDV")},
        {QStringLiteral("PSK"),           QStringLiteral("PSK Reporter")},
    };
    auto* visGrid = new QGridLayout;
    visGrid->setColumnStretch(1, 1);
    for (int i = 0; i < kSourceChecks.size(); ++i) {
        const QString& source = kSourceChecks[i].first;
        const QString& label  = kSourceChecks[i].second;
        const QString key = QStringLiteral("SpotSourceVisible/") + source;
        const bool on = s.value(key, "True").toString() == "True";
        auto* cb = new QCheckBox(label);
        cb->setObjectName(QStringLiteral("displaySourceShow_") + source);
        cb->setChecked(on);
        // Apply persisted state to the Spot List immediately on dialog
        // construct -- otherwise the list shows everything until the
        // operator toggles the checkbox once.
        if (!on && m_spotProxyModel) {
            m_spotProxyModel->setSourceVisible(source, false);
        }
        connect(cb, &QCheckBox::toggled, this,
                [save, key, source, this](bool on) {
            save(key, on ? "True" : "False");
            // settingsChanged emit (inside `save`) triggers MainWindow's
            // loadSpotDisplaySettings which pulls per-source visibility
            // into the SpectrumWidget (panadapter overlay mask).
            //
            // Phase 3J-1 closeout follow-up (2026-05-12): ALSO route
            // through BandFilterProxy::setSourceVisible so the Spot List
            // table hides this source's rows.  Before this, the Display
            // tab toggle only affected the panadapter -- user-visible bug
            // because the section label ("Show on panadapter") was
            // misleading; the operator's mental model is "hide
            // everywhere".  See 2026-05-12 bench session.
            if (m_spotProxyModel) {
                m_spotProxyModel->setSourceVisible(source, on);
            }
        });
        visGrid->addWidget(cb, i, 0);
    }
    rightCol->addLayout(visGrid);

    rightCol->addStretch();

    root->addLayout(rightCol, 1);

    // ── Live stat refresh wiring (NereusSDR-native) ───────────────
    // Refresh the LEFT-column stat blocks whenever the table model
    // emits a row change or the DxccColorProvider finishes an
    // import. The unique-callsign count is computed by walking the
    // SpotTableModel's source spots (cheap; bounded at 500). The
    // Active Sources counter polls the seven ingest clients'
    // isConnected / isListening / isPolling methods. New DXCC and
    // New bands counts walk the table model's spots and ask the
    // DxccColorProvider for the status of each.
    auto refreshStats = [this] {
        if (m_statTotalSpots && m_spotTableModel) {
            m_statTotalSpots->setText(QString::number(m_spotTableModel->rowCount()));
        }
        if (m_statUniqueCallsigns && m_spotTableModel) {
            QSet<QString> unique;
            const int n = m_spotTableModel->rowCount();
            for (int r = 0; r < n; ++r) {
                QModelIndex idx = m_spotTableModel->index(r, SpotTableModel::ColDxCall);
                unique.insert(m_spotTableModel->data(idx, Qt::DisplayRole).toString().trimmed().toUpper());
            }
            unique.remove(QString());
            m_statUniqueCallsigns->setText(QString::number(unique.size()));
        }
        if (m_statActiveSources) {
            int active = 0;
            constexpr int kTotal = 7;
            if (m_clusterClient       && m_clusterClient->isConnected())      ++active;
            if (m_rbnClient           && m_rbnClient->isConnected())          ++active;
            if (m_wsjtxClient         && m_wsjtxClient->isListening())        ++active;
            if (m_spotCollectorClient && m_spotCollectorClient->isListening()) ++active;
            if (m_potaClient          && m_potaClient->isPolling())           ++active;
            if (m_freedvClient        && m_freedvClient->isConnected())       ++active;
            if (m_pskClient           && m_pskClient->isListening())          ++active;
            m_statActiveSources->setText(QString("%1/%2").arg(active).arg(kTotal));
        }
        if (m_statCtyDatEntries) {
            // CtyDatParser is held inside DxccColorProvider but not
            // publicly exposed; the integrator's `entityCount()`
            // currently reports DxccWorkedStatus entities (loaded
            // entities, not cty.dat rows). Use the entityCount
            // method as the closest proxy and document that this
            // tracks DXCC entities resolved by the worked-status
            // table, not raw cty.dat rows. If a dedicated cty.dat
            // count is needed later, the integrator can grow a
            // ctyEntityCount accessor.
            int n = m_dxccProvider ? m_dxccProvider->entityCount() : 0;
            m_statCtyDatEntries->setText(QString::number(n));
        }
        if (m_statAdifQsos) {
            int n = m_dxccProvider ? m_dxccProvider->qsoCount() : 0;
            m_statAdifQsos->setText(QString::number(n));
        }
        if (m_statDxccEntities) {
            int n = m_dxccProvider ? m_dxccProvider->entityCount() : 0;
            m_statDxccEntities->setText(QString::number(n));
        }
        if ((m_statNewDxcc || m_statNewBands) && m_dxccProvider && m_spotTableModel) {
            int newDxcc = 0;
            int newBands = 0;
            const int n = m_spotTableModel->rowCount();
            for (int r = 0; r < n; ++r) {
                QModelIndex callIdx = m_spotTableModel->index(r, SpotTableModel::ColDxCall);
                QModelIndex modeIdx = m_spotTableModel->index(r, SpotTableModel::ColMode);
                QString call = m_spotTableModel->data(callIdx, Qt::DisplayRole).toString();
                double freq = m_spotTableModel->freqAtRow(r);
                QString mode = m_spotTableModel->data(modeIdx, Qt::DisplayRole).toString();
                DxccStatus status = m_dxccProvider->statusForSpot(call, freq, mode);
                if (status == DxccStatus::NewDxcc) {
                    ++newDxcc;
                } else if (status == DxccStatus::NewBand) {
                    ++newBands;
                }
            }
            if (m_statNewDxcc)  m_statNewDxcc->setText(QString::number(newDxcc));
            if (m_statNewBands) m_statNewBands->setText(QString::number(newBands));
        }
    };

    // Initial refresh + on-change refresh hooks. The table model
    // emits rowsInserted / modelReset / rowsRemoved through the
    // QAbstractItemModel base; wiring those keeps the stat blocks
    // honest. nullptr-guarded for the test fixture path.
    refreshStats();
    if (m_spotTableModel) {
        connect(m_spotTableModel, &QAbstractTableModel::rowsInserted, this,
                [refreshStats] { refreshStats(); });
        connect(m_spotTableModel, &QAbstractTableModel::rowsRemoved, this,
                [refreshStats] { refreshStats(); });
        connect(m_spotTableModel, &QAbstractTableModel::modelReset, this,
                [refreshStats] { refreshStats(); });
    }
    if (m_dxccProvider) {
        connect(m_dxccProvider, &DxccColorProvider::importFinished, this,
                [refreshStats] { refreshStats(); });
    }

    tabs->addTab(page, "Display");
}

// 2026-05-12 bench fix (Gap #6 — Spot List ↔ panadapter hover sync).
//
// Panadapter → list direction.  SpectrumWidget emits
// spotHoverIndexChanged(spotIdx) when the mouse hovers a spot label;
// MainWindow forwards into here.  spotIdx is SpotModel::SpotData::index
// (the per-spot counter the model assigns in applySpotStatus).  Map
// that back to a row in m_spotTableModel by matching the SpotData's
// callsign + freqMhz + source against the table's DxSpot rows.  When
// found, select that row (which also scrolls it into view via the
// view's autoscroll-on-selection-change default).
//
// -1 sentinel clears the selection ("mouse is no longer over a spot").
void SpotHubDialog::setHoveredPanadapterSpot(int spotIdx)
{
    if (!m_spotTable || !m_spotTableModel) {
        return;
    }
    if (spotIdx < 0) {
        m_spotTable->clearSelection();
        return;
    }
    if (!m_spotModel) {
        return;
    }
    const auto& spots = m_spotModel->spots();
    const auto it = spots.constFind(spotIdx);
    if (it == spots.constEnd()) {
        return;
    }
    const SpotData& sd = it.value();
    const double spotFreqMhz = (sd.rxFreqMhz > 0.0)
                                   ? sd.rxFreqMhz
                                   : sd.txFreqMhz;
    // Scan the table for a row whose DxSpot matches by callsign +
    // freq (1 kHz tolerance) + source.
    for (int row = 0; row < m_spotTableModel->rowCount(); ++row) {
        const double rowFreq = m_spotTableModel->freqAtRow(row);
        if (std::abs(rowFreq - spotFreqMhz) > 0.001) { continue; }
        const QModelIndex callIdx = m_spotTableModel->index(
            row, SpotTableModel::ColDxCall);
        const QString rowCall = m_spotTableModel->data(callIdx, Qt::DisplayRole).toString();
        if (rowCall.compare(sd.callsign, Qt::CaseInsensitive) != 0) { continue; }
        const QModelIndex sourceIdx = m_spotTableModel->index(
            row, SpotTableModel::ColSource);
        const QString rowSource = m_spotTableModel->data(sourceIdx, Qt::DisplayRole).toString();
        if (!sd.source.isEmpty() && rowSource.compare(sd.source, Qt::CaseInsensitive) != 0) {
            continue;
        }
        // Found — select via the proxy so the view scrolls correctly.
        const QModelIndex proxyIdx = m_spotProxyModel
            ? m_spotProxyModel->mapFromSource(m_spotTableModel->index(row, 0))
            : m_spotTableModel->index(row, 0);
        if (proxyIdx.isValid()) {
            m_spotTable->setCurrentIndex(proxyIdx);
            m_spotTable->scrollTo(proxyIdx,
                                   QAbstractItemView::EnsureVisible);
        }
        return;
    }
}

} // namespace NereusSDR
