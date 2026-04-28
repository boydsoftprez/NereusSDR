// =================================================================
// src/gui/ConnectionPanel.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/ucRadioList.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/clsDiscoveredRadioPicker.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
// =================================================================

/*  ucRadioList.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
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

/*  clsDiscoveredRadioPicker.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
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

#include "ConnectionPanel.h"
#include "AddCustomRadioDialog.h"
#include "models/RadioModel.h"
#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"

#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFont>
#include <QColor>
#include <QTimer>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Color constants — from ucRadioList.cs ~:1115 (adapted to dark theme)
// ucRadioList uses light background; NereusSDR uses dark background.
// ---------------------------------------------------------------------------
// From ucRadioList.cs:1118 — connectedFill = Color.FromArgb(235, 248, 235)
static constexpr QColor kColorOnlineFree  { 20, 80, 30};    // dark green — online + free
// From ucRadioList.cs:1117 — selectedFill = Color.FromArgb(225, 240, 255)
static constexpr QColor kColorOnlineInUse { 80, 60, 10};    // dark amber — online + in-use
static constexpr QColor kColorOffline     { 30, 30, 40};    // dark grey — offline

// ---------------------------------------------------------------------------
// State pill colors — design §7.3 (Phase 3Q Task 5)
// ---------------------------------------------------------------------------
static const char* kPillOnlineColor    = "#39c167";   // green  — last seen < 60 s
static const char* kPillStaleColor     = "#d39c2a";   // amber  — last seen 60 s – 5 min
static const char* kPillOfflineColor   = "#c14848";   // red    — last seen > 5 min / never
static const char* kPillConnectedColor = "#39c167";   // green  — currently connected

// ---------------------------------------------------------------------------
// StatePill — pure static threshold function (Phase 3Q Task 5 Step 3)
// ---------------------------------------------------------------------------

ConnectionPanel::StatePill ConnectionPanel::statePillForLastSeen(qint64 lastSeenMs, qint64 nowMs)
{
    if (lastSeenMs <= 0) {
        return StatePill::Offline;
    }
    const qint64 ageMs = nowMs - lastSeenMs;
    if (ageMs < 60 * 1000) {
        return StatePill::Online;
    }
    if (ageMs < 5 * 60 * 1000) {
        return StatePill::Stale;
    }
    return StatePill::Offline;
}

// ---------------------------------------------------------------------------
// Relative time helper (Phase 3Q Task 5 Step 6)
// ---------------------------------------------------------------------------

QString ConnectionPanel::relativeTime(qint64 lastSeenMs, qint64 nowMs)
{
    if (lastSeenMs <= 0) {
        return QStringLiteral("never seen");
    }
    const qint64 ageMs = nowMs - lastSeenMs;
    if (ageMs < 60 * 1000) {
        return QStringLiteral("just now");
    }
    if (ageMs < 60 * 60 * 1000) {
        return QStringLiteral("%1 m ago").arg(ageMs / 60000);
    }
    if (ageMs < 24 * 60 * 60 * 1000LL) {
        return QStringLiteral("%1 h ago").arg(ageMs / 3600000);
    }
    return QStringLiteral("%1 d ago").arg(ageMs / 86400000LL);
}

ConnectionPanel::ConnectionPanel(RadioModel* model, QWidget* parent)
    : QDialog(parent)
    , m_radioModel(model)
{
    setWindowTitle(QStringLiteral("Connect to Radio"));
    setMinimumSize(800, 460);
    resize(900, 520);

    buildUI();

    // Wire discovery signals
    RadioDiscovery* disc = m_radioModel->discovery();
    connect(disc, &RadioDiscovery::radioDiscovered, this, &ConnectionPanel::onRadioDiscovered);
    connect(disc, &RadioDiscovery::radioUpdated,    this, &ConnectionPanel::onRadioUpdated);
    connect(disc, &RadioDiscovery::radioLost,       this, &ConnectionPanel::onRadioLost);
    connect(disc, &RadioDiscovery::discoveryStarted,  this, [this]() {
        setStatusText(QStringLiteral("Searching for radios..."));
        m_scanBtn->setEnabled(false);
    });
    connect(disc, &RadioDiscovery::discoveryFinished, this, [this]() {
        int n = m_radioTable->rowCount();
        setStatusText(n > 0
            ? QStringLiteral("Found %1 radio(s)").arg(n)
            : QStringLiteral("No radios found — try again"));
        m_scanBtn->setEnabled(true);
    });

    // Wire connection state
    connect(m_radioModel, &RadioModel::connectionStateChanged,
            this, &ConnectionPanel::onConnectionStateChanged);
    connect(m_radioModel, &RadioModel::connectionStateChanged,
            this, &ConnectionPanel::updateStatusStrip);

    // Periodic refresh of Last Seen relative times — every 15 s is adequate.
    connect(&m_lastSeenRefreshTimer, &QTimer::timeout,
            this, &ConnectionPanel::refreshLastSeenColumn);
    m_lastSeenRefreshTimer.start(15000);

    // Design §7.4: feed all currently-saved MACs to RadioDiscovery so it knows
    // which entries to exempt from the stale-removal sweep.
    {
        const QList<SavedRadio> saved = AppSettings::instance().savedRadios();
        QStringList savedMacs;
        savedMacs.reserve(saved.size());
        for (const SavedRadio& sr : saved) {
            if (!sr.info.macAddress.isEmpty()) {
                savedMacs.append(sr.info.macAddress);
            }
        }
        disc->setSavedMacs(savedMacs);
    }

    // Populate with any radios already discovered before the panel opened
    const QList<RadioInfo> existing = disc->discoveredRadios();
    for (const RadioInfo& info : existing) {
        onRadioDiscovered(info);
    }

    // Start discovery automatically when panel opens
    disc->startDiscovery();

    if (m_radioTable->rowCount() == 0) {
        setStatusText(QStringLiteral("Searching for radios..."));
    }

    // Initial status strip state
    updateStatusStrip();
}

ConnectionPanel::~ConnectionPanel()
{
    // Don't stop discovery here — RadioModel owns it and it keeps running
}

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------

void ConnectionPanel::buildUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // --- Phase 3Q Task 5: Top connection-status strip ---
    mainLayout->addWidget(buildStatusStrip());

    // --- Status label ---
    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #8090a0; padding: 4px; }"));
    mainLayout->addWidget(m_statusLabel);

    // --- Radio table (QTableWidget — simpler than QTreeView for this use case) ---
    // Source: clsDiscoveredRadioPicker.cs:81 — DataGridView with SelectionMode FullRowSelect
    auto* radioGroup = new QGroupBox(QStringLiteral("Discovered Radios"), this);
    auto* radioLayout = new QVBoxLayout(radioGroup);

    m_radioTable = new QTableWidget(0, ColCount, this);

    // Column headers — mapping clsDiscoveredRadioPicker.cs columns (~:99) to 8-col layout.
    // Phase 3Q Task 5: ColMac replaced by ColLastSeen; MAC moves to detail panel.
    // clsDiscoveredRadioPicker.cs columns: Hardware(:111), IP(:118), Base Port(:126),
    //   Mac Address(:134), Protocol(:142), Version(:150)
    // NereusSDR adds: status dot (col 0), Board type (col 2), In-Use (col 7)
    QStringList headers;
    headers << QStringLiteral("●")           // Col 0 — state pill dot
            << QStringLiteral("Name")        // Col 1 — clsDiscoveredRadioPicker.cs:112 "Hardware"
            << QStringLiteral("Board")       // Col 2 — HPSDRHW board type name
            << QStringLiteral("Protocol")    // Col 3 — clsDiscoveredRadioPicker.cs:143 "Protocol"
            << QStringLiteral("IP")          // Col 4 — clsDiscoveredRadioPicker.cs:119 "IP"
            << QStringLiteral("Last Seen")   // Col 5 — relative last-seen time (was MAC)
            << QStringLiteral("Firmware")    // Col 6 — clsDiscoveredRadioPicker.cs:151 "Version"
            << QStringLiteral("In-Use");     // Col 7 — ucRadioList.cs:101 RadioIsBusy
    m_radioTable->setHorizontalHeaderLabels(headers);

    // Source: clsDiscoveredRadioPicker.cs:87 — FullRowSelect
    m_radioTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_radioTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_radioTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_radioTable->setAlternatingRowColors(false);
    m_radioTable->setShowGrid(false);
    m_radioTable->verticalHeader()->setVisible(false);
    m_radioTable->horizontalHeader()->setStretchLastSection(true);
    m_radioTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);

    // Fixed width for narrow columns
    m_radioTable->setColumnWidth(ColStatus,    28);
    m_radioTable->setColumnWidth(ColName,     180);
    m_radioTable->setColumnWidth(ColBoard,    100);
    m_radioTable->setColumnWidth(ColProtocol,  60);
    m_radioTable->setColumnWidth(ColIp,       130);
    m_radioTable->setColumnWidth(ColLastSeen, 100);
    m_radioTable->setColumnWidth(ColFirmware,  70);
    // ColInUse stretches

    m_radioTable->setContextMenuPolicy(Qt::CustomContextMenu);
    m_radioTable->setStyleSheet(QStringLiteral(
        "QTableWidget {"
        "  background: #0a0a15;"
        "  color: #c8d8e8;"
        "  border: 1px solid #203040;"
        "  font-family: Consolas, 'Courier New', monospace;"
        "  font-size: 12px;"
        "  gridline-color: #1a2a3a;"
        "}"
        "QTableWidget::item {"
        "  padding: 4px 6px;"
        "  border: none;"
        "}"
        "QTableWidget::item:selected {"
        "  background: #005578;"
        "  color: #ffffff;"
        "}"
        "QHeaderView::section {"
        "  background: #12202e;"
        "  color: #8090a0;"
        "  border: none;"
        "  border-right: 1px solid #203040;"
        "  border-bottom: 1px solid #203040;"
        "  padding: 4px 6px;"
        "  font-size: 11px;"
        "}"));

    connect(m_radioTable, &QTableWidget::itemSelectionChanged,
            this, &ConnectionPanel::onRadioSelectionChanged);
    connect(m_radioTable, &QTableWidget::cellDoubleClicked,
            this, &ConnectionPanel::onTableDoubleClicked);
    connect(m_radioTable, &QTableWidget::customContextMenuRequested,
            this, &ConnectionPanel::onContextMenuRequested);

    radioLayout->addWidget(m_radioTable);
    mainLayout->addWidget(radioGroup, /*stretch=*/1);

    // --- Selected Radio detail panel (Phase 3I-RP) ---
    m_detailGroup = new QGroupBox(QStringLiteral("Selected Radio"), this);
    m_detailGroup->setVisible(false);
    auto* detailLayout = new QVBoxLayout(m_detailGroup);
    detailLayout->setSpacing(4);

    // Info row 1: Board + Protocol + Firmware
    auto* infoRow1 = new QHBoxLayout();
    m_detailBoardLabel = new QLabel(m_detailGroup);
    m_detailProtoLabel = new QLabel(m_detailGroup);
    m_detailFwLabel    = new QLabel(m_detailGroup);
    for (auto* lbl : {m_detailBoardLabel, m_detailProtoLabel, m_detailFwLabel}) {
        lbl->setStyleSheet(QStringLiteral("QLabel { color: #8090a0; font-size: 12px; }"));
    }
    infoRow1->addWidget(m_detailBoardLabel);
    infoRow1->addWidget(m_detailProtoLabel);
    infoRow1->addWidget(m_detailFwLabel);
    infoRow1->addStretch();
    detailLayout->addLayout(infoRow1);

    // Info row 2: IP + MAC
    auto* infoRow2 = new QHBoxLayout();
    m_detailIpLabel  = new QLabel(m_detailGroup);
    m_detailMacLabel = new QLabel(m_detailGroup);
    for (auto* lbl : {m_detailIpLabel, m_detailMacLabel}) {
        lbl->setStyleSheet(QStringLiteral("QLabel { color: #8090a0; font-size: 12px; }"));
    }
    infoRow2->addWidget(m_detailIpLabel);
    infoRow2->addWidget(m_detailMacLabel);
    infoRow2->addStretch();
    detailLayout->addLayout(infoRow2);

    // Model selector row
    auto* modelRow = new QHBoxLayout();
    auto* modelLabel = new QLabel(QStringLiteral("Radio Model:"), m_detailGroup);
    modelLabel->setStyleSheet(QStringLiteral("QLabel { color: #c8d8e8; font-weight: bold; }"));
    m_modelCombo = new QComboBox(m_detailGroup);
    m_modelCombo->setMinimumWidth(200);
    m_modelCombo->setStyleSheet(QStringLiteral(
        "QComboBox { background: #1a2a3a; color: #c8d8e8; border: 1px solid #304050;"
        "  border-radius: 3px; padding: 4px 8px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: #12202e; color: #c8d8e8;"
        "  selection-background-color: #005578; }"));
    connect(m_modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConnectionPanel::onModelComboChanged);
    modelRow->addWidget(modelLabel);
    modelRow->addWidget(m_modelCombo);
    modelRow->addStretch();
    detailLayout->addLayout(modelRow);

    // Hint label
    m_modelHintLabel = new QLabel(m_detailGroup);
    m_modelHintLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #b08020; font-size: 11px; font-style: italic; }"));
    m_modelHintLabel->setVisible(false);
    detailLayout->addWidget(m_modelHintLabel);

    // Phase 3Q Task 5 — Auto-connect-on-launch checkbox
    // Reads from AppSettings when a row is selected, writes on toggle.
    m_autoConnectCheck = new QCheckBox(
        QStringLiteral("Auto-connect to this radio on launch"), m_detailGroup);
    m_autoConnectCheck->setStyleSheet(QStringLiteral(
        "QCheckBox { color: #c8d8e8; font-size: 12px; }"
        "QCheckBox::indicator { width: 14px; height: 14px; }"));
    connect(m_autoConnectCheck, &QCheckBox::toggled, this, [this](bool checked) {
        RadioInfo info = selectedRadio();
        if (info.macAddress.isEmpty()) {
            return;
        }
        // Use QSignalBlocker to prevent echo if we're populating from settings
        AppSettings& s = AppSettings::instance();
        bool pinToMac = false;
        if (auto existing = s.savedRadio(info.macAddress)) {
            pinToMac = existing->pinToMac;
        }
        s.saveRadio(info, pinToMac, checked);
        s.save();
    });
    detailLayout->addWidget(m_autoConnectCheck);

    mainLayout->addWidget(m_detailGroup);

    // --- Bottom strip buttons (plan §5.2 + Thetis layout) ---
    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(6);

    // Helper lambda for button styling
    auto makeBtn = [this](const QString& label,
                          const QString& style = {}) -> QPushButton* {
        auto* btn = new QPushButton(label, this);
        btn->setAutoDefault(false);
        btn->setMinimumWidth(90);
        if (!style.isEmpty()) {
            btn->setStyleSheet(style);
        }
        return btn;
    };

    static const QString kPrimaryStyle = QStringLiteral(
        "QPushButton {"
        "  background: #00b4d8; color: #fff;"
        "  border: none; border-radius: 4px; padding: 6px 12px; font-weight: bold;"
        "}"
        "QPushButton:hover { background: #0096b7; }"
        "QPushButton:disabled { background: #303850; color: #606878; }");

    static const QString kSecondaryStyle = QStringLiteral(
        "QPushButton {"
        "  background: #304050; color: #c8d8e8;"
        "  border: 1px solid #405060; border-radius: 4px; padding: 6px 12px;"
        "}"
        "QPushButton:hover { background: #405060; }"
        "QPushButton:disabled { background: #1a2a3a; color: #404858; border-color: #203040; }");

    static const QString kDestructiveStyle = QStringLiteral(
        "QPushButton {"
        "  background: #4a2020; color: #c8a8a8;"
        "  border: 1px solid #6a3030; border-radius: 4px; padding: 6px 12px;"
        "}"
        "QPushButton:hover { background: #602020; }"
        "QPushButton:disabled { background: #1a2a3a; color: #404858; border-color: #203040; }");

    // Phase 3Q Task 5 — single ↻ Scan button replaces Start + Stop Discovery.
    // One action, one-shot: triggers the same NIC broadcast scan as before.
    m_scanBtn = makeBtn(QStringLiteral("↻ Scan"), kSecondaryStyle);
    m_scanBtn->setToolTip(QStringLiteral("Broadcast-scan all network interfaces for OpenHPSDR radios"));
    connect(m_scanBtn, &QPushButton::clicked, this, &ConnectionPanel::onScanClicked);
    btnLayout->addWidget(m_scanBtn);

    btnLayout->addStretch();

    // Add Manually — Phase 3I Task 16
    // Source: frmAddCustomRadio.cs — port of the Thetis "Add Custom Radio" dialog
    m_addManuallyBtn = makeBtn(QStringLiteral("Add Manually..."), kSecondaryStyle);
    m_addManuallyBtn->setToolTip(QStringLiteral("Add a radio that is not on the local subnet"));
    connect(m_addManuallyBtn, &QPushButton::clicked, this, &ConnectionPanel::onAddManuallyClicked);
    btnLayout->addWidget(m_addManuallyBtn);

    // Forget — Phase 3I Task 15
    m_forgetBtn = makeBtn(QStringLiteral("Forget"), kDestructiveStyle);
    m_forgetBtn->setEnabled(false);
    m_forgetBtn->setToolTip(QStringLiteral("Remove this radio from the saved list"));
    connect(m_forgetBtn, &QPushButton::clicked, this, &ConnectionPanel::onForgetClicked);
    btnLayout->addWidget(m_forgetBtn);

    btnLayout->addStretch();

    // Connect
    m_connectBtn = makeBtn(QStringLiteral("Connect"), kPrimaryStyle);
    m_connectBtn->setEnabled(false);
    connect(m_connectBtn, &QPushButton::clicked, this, &ConnectionPanel::onConnectClicked);
    btnLayout->addWidget(m_connectBtn);

    // Phase 3Q Task 5: Disconnect removed from bottom strip.
    // It now lives in the top status strip (buildStatusStrip()).
    // Bottom strip: [↻ Scan] [Add Manually…] [Forget] (stretch) [Connect] [Close]

    // Close
    m_closeBtn = makeBtn(QStringLiteral("Close"), kSecondaryStyle);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(m_closeBtn);

    mainLayout->addLayout(btnLayout);

    // Apply dark theme to dialog
    setStyleSheet(QStringLiteral(
        "QDialog { background: #0f0f1a; }"
        "QGroupBox {"
        "  color: #8090a0;"
        "  border: 1px solid #203040;"
        "  border-radius: 4px;"
        "  margin-top: 8px;"
        "  padding-top: 12px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 10px;"
        "  padding: 0 3px;"
        "}"));

    updateButtonStates();
}

// ---------------------------------------------------------------------------
// buildStatusStrip — Phase 3Q Task 5 Step 7
// Top strip above the table: shows current connection state with pill +
// info text + Disconnect button (when connected) or Reconnect hint (when not).
// ---------------------------------------------------------------------------

QWidget* ConnectionPanel::buildStatusStrip()
{
    m_statusStrip = new QWidget(this);
    m_statusStrip->setObjectName(QStringLiteral("statusStrip"));
    m_statusStrip->setFixedHeight(40);
    m_statusStrip->setStyleSheet(QStringLiteral(
        "QWidget#statusStrip {"
        "  background: #0d1b28;"
        "  border: 1px solid #203040;"
        "  border-radius: 4px;"
        "}"));

    auto* stripLayout = new QHBoxLayout(m_statusStrip);
    stripLayout->setContentsMargins(10, 4, 10, 4);
    stripLayout->setSpacing(8);

    // Pill label — coloured circle text
    m_stripPillLabel = new QLabel(QStringLiteral("●"), m_statusStrip);
    m_stripPillLabel->setStyleSheet(QStringLiteral("QLabel { color: #c14848; font-size: 16px; }"));

    // Info text label
    m_stripInfoLabel = new QLabel(QStringLiteral("Disconnected"), m_statusStrip);
    m_stripInfoLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #8090a0; font-size: 12px; }"));

    // Reconnect hint — shown when disconnected and a last-connected radio is known
    m_stripReconnectLabel = new QLabel(m_statusStrip);
    m_stripReconnectLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #00b4d8; font-size: 11px; }"));
    m_stripReconnectLabel->setVisible(false);

    stripLayout->addWidget(m_stripPillLabel);
    stripLayout->addWidget(m_stripInfoLabel);
    stripLayout->addWidget(m_stripReconnectLabel);
    stripLayout->addStretch();

    // Disconnect button — only visible when connected
    static const QString kDisconnectStyle = QStringLiteral(
        "QPushButton {"
        "  background: #4a2020; color: #c8a8a8;"
        "  border: 1px solid #6a3030; border-radius: 4px; padding: 4px 10px;"
        "}"
        "QPushButton:hover { background: #602020; }");

    m_stripDisconnectBtn = new QPushButton(QStringLiteral("Disconnect"), m_statusStrip);
    m_stripDisconnectBtn->setAutoDefault(false);
    m_stripDisconnectBtn->setStyleSheet(kDisconnectStyle);
    m_stripDisconnectBtn->setVisible(false);
    connect(m_stripDisconnectBtn, &QPushButton::clicked,
            this, &ConnectionPanel::onDisconnectClicked);
    stripLayout->addWidget(m_stripDisconnectBtn);

    return m_statusStrip;
}

// ---------------------------------------------------------------------------
// updateStatusStrip — Phase 3Q Task 5 Step 7
// Updates the top strip to reflect the current RadioModel connection state.
// ---------------------------------------------------------------------------

void ConnectionPanel::updateStatusStrip()
{
    if (!m_statusStrip) {
        return;
    }

    const ConnectionState state = m_radioModel->connectionState();
    const bool connected = (state == ConnectionState::Connected);

    if (connected) {
        // Show green pill + radio name + IP
        m_stripPillLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 16px; }").arg(QLatin1String(kPillConnectedColor)));

        RadioConnection* conn = m_radioModel->connection();
        if (conn) {
            const RadioInfo& ri = conn->radioInfo();
            m_stripInfoLabel->setText(QStringLiteral("Connected — %1  (%2)")
                .arg(ri.displayName(), ri.address.toString()));
        } else {
            m_stripInfoLabel->setText(QStringLiteral("Connected — %1")
                .arg(m_radioModel->name()));
        }
        m_stripInfoLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #c8d8e8; font-size: 12px; font-weight: bold; }"));

        m_stripDisconnectBtn->setVisible(true);
        m_stripReconnectLabel->setVisible(false);
    } else {
        // Show red pill + Disconnected + optional Reconnect hint
        m_stripPillLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 16px; }").arg(QLatin1String(kPillOfflineColor)));

        m_stripInfoLabel->setText(QStringLiteral("Disconnected"));
        m_stripInfoLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #8090a0; font-size: 12px; }"));

        m_stripDisconnectBtn->setVisible(false);

        // Reconnect hint if there's a last-connected radio
        const QString lastMac = AppSettings::instance().lastConnected();
        if (!lastMac.isEmpty()) {
            if (auto saved = AppSettings::instance().savedRadio(lastMac)) {
                m_stripReconnectLabel->setText(
                    QStringLiteral("Last: %1 — scan to reconnect").arg(saved->info.name));
                m_stripReconnectLabel->setVisible(true);
            } else {
                m_stripReconnectLabel->setVisible(false);
            }
        } else {
            m_stripReconnectLabel->setVisible(false);
        }
    }
}

// ---------------------------------------------------------------------------
// refreshLastSeenColumn — Phase 3Q Task 5 (periodic timer slot)
// Rewrites every cell in the Last Seen column using current relative times.
// ---------------------------------------------------------------------------

void ConnectionPanel::refreshLastSeenColumn()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QString connectedMac = m_radioModel->isConnected()
        ? (m_radioModel->connection()
           ? m_radioModel->connection()->radioInfo().macAddress
           : QString())
        : QString();

    for (int row = 0; row < m_radioTable->rowCount(); ++row) {
        QTableWidgetItem* statusCell = m_radioTable->item(row, ColStatus);
        if (!statusCell) {
            continue;
        }
        const QString mac = statusCell->data(kMacRole).toString();

        // Refresh Last Seen column
        QTableWidgetItem* lsCell = m_radioTable->item(row, ColLastSeen);
        if (lsCell) {
            const qint64 lastSeen = m_lastSeenMs.value(mac, 0LL);
            lsCell->setText(relativeTime(lastSeen, now));
        }

        // Refresh state pill on the status column
        if (statusCell) {
            const bool isConnected = (!connectedMac.isEmpty() && mac == connectedMac);
            StatePill pill = isConnected
                ? StatePill::Connected
                : statePillForLastSeen(m_lastSeenMs.value(mac, 0LL), now);
            const char* pillColor = kPillOfflineColor;
            switch (pill) {
                case StatePill::Online:    pillColor = kPillOnlineColor;    break;
                case StatePill::Stale:     pillColor = kPillStaleColor;     break;
                case StatePill::Offline:   pillColor = kPillOfflineColor;   break;
                case StatePill::Connected: pillColor = kPillConnectedColor; break;
            }
            statusCell->setForeground(QColor(QString::fromLatin1(pillColor)));
        }
    }
}

// ---------------------------------------------------------------------------
// Status text
// ---------------------------------------------------------------------------

void ConnectionPanel::setStatusText(const QString& text)
{
    if (m_statusLabel) {
        m_statusLabel->setText(text);
    }
}

// ---------------------------------------------------------------------------
// Row helpers
// ---------------------------------------------------------------------------

// Apply background color to a table row based on radio state.
// Color coding from ucRadioList.cs:1115-1126:
//   connectedFill  = Color.FromArgb(235, 248, 235)  → kColorOnlineFree
//   inUseFill      = amber (not in Thetis, NereusSDR addition)
//   offline/error  = grey/red
//
// Phase 3Q Task 5: state dot in ColStatus is given the StatePill color.
// The pill color is derived from the radio's last-seen timestamp (m_lastSeenMs).
void ConnectionPanel::applyRowColor(int row, const RadioInfo& info)
{
    QColor bg;
    QColor fg{0xc8, 0xd8, 0xe8};

    if (info.inUse) {
        bg = kColorOnlineInUse;
        fg = QColor{0xe0, 0xc0, 0x80};
    } else {
        // Radios in m_discoveredRadios are always "online" — seen in last scan.
        bg = kColorOnlineFree;
        fg = QColor{0x80, 0xe0, 0x80};
    }

    for (int col = 0; col < ColCount; ++col) {
        QTableWidgetItem* cell = m_radioTable->item(row, col);
        if (cell) {
            cell->setBackground(bg);
            cell->setForeground(fg);
        }
    }

    // Override the state pill dot with a StatePill-appropriate color
    if (QTableWidgetItem* dotCell = m_radioTable->item(row, ColStatus)) {
        const bool isConnected = m_radioModel->isConnected()
            && m_radioModel->connection()
            && m_radioModel->connection()->radioInfo().macAddress == info.macAddress;
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const StatePill pill = isConnected
            ? StatePill::Connected
            : statePillForLastSeen(m_lastSeenMs.value(info.macAddress, 0LL), now);
        const char* pillColor = kPillOfflineColor;
        switch (pill) {
            case StatePill::Online:    pillColor = kPillOnlineColor;    break;
            case StatePill::Stale:     pillColor = kPillStaleColor;     break;
            case StatePill::Offline:   pillColor = kPillOfflineColor;   break;
            case StatePill::Connected: pillColor = kPillConnectedColor; break;
        }
        dotCell->setForeground(QColor(QString::fromLatin1(pillColor)));
    }
}

// Populate a table row from a RadioInfo.
// Column mapping per plan §5.2 and clsDiscoveredRadioPicker.cs (Phase 3Q Task 5):
//   Col 0 ●  : state pill dot — "●" (U+25CF), colored by StatePill
//   Col 1 Name: displayName() or BoardCapsTable fallback
//             (clsDiscoveredRadioPicker.cs:112 "Hardware")
//   Col 2 Board: HPSDRHW enum name
//   Col 3 Protocol: "P1" / "P2"  (clsDiscoveredRadioPicker.cs:143 "Protocol")
//   Col 4 IP: address.toString() (clsDiscoveredRadioPicker.cs:119 "IP")
//   Col 5 Last Seen: relative time — "just now", "4 m ago", etc. (was MAC)
//   Col 6 Firmware: firmwareVersion (clsDiscoveredRadioPicker.cs:151 "Version")
//   Col 7 In-Use: "free" / "in use" (ucRadioList.cs:101 RadioIsBusy)
//
// MAC moves to the detail panel (already shown as "MAC: ..." in m_detailMacLabel).
void ConnectionPanel::populateRow(int row, const RadioInfo& info)
{
    // Derive display strings
    const QString& capDisplayName =
        QString::fromUtf8(BoardCapsTable::forBoard(info.boardType).displayName);
    const QString name = capDisplayName.isEmpty() ? info.displayName() : capDisplayName;

    // Board string — HPSDRHW enum to friendly name
    QString boardStr;
    switch (info.boardType) {
        case HPSDRHW::Atlas:      boardStr = QStringLiteral("Atlas");       break;
        case HPSDRHW::Hermes:     boardStr = QStringLiteral("Hermes");      break;
        case HPSDRHW::HermesII:   boardStr = QStringLiteral("Hermes II");   break;
        case HPSDRHW::Angelia:    boardStr = QStringLiteral("Angelia");     break;
        case HPSDRHW::Orion:      boardStr = QStringLiteral("Orion");       break;
        case HPSDRHW::OrionMKII:  boardStr = QStringLiteral("Orion MkII"); break;
        case HPSDRHW::HermesLite: boardStr = QStringLiteral("HL2");        break;
        case HPSDRHW::Saturn:     boardStr = QStringLiteral("Saturn");      break;
        case HPSDRHW::SaturnMKII: boardStr = QStringLiteral("Saturn MkII"); break;
        default:                  boardStr = QStringLiteral("Unknown");     break;
    }

    // Protocol string — ucRadioList.cs:1342 "Protocol-1" / "Protocol-2"
    const QString protoStr = (info.protocol == ProtocolVersion::Protocol2)
                             ? QStringLiteral("P2") : QStringLiteral("P1");

    // In-use string — ucRadioList.cs:101 RadioIsBusy
    const QString inUseStr = info.inUse
                             ? QStringLiteral("in use") : QStringLiteral("free");

    // Phase 3Q Task 5: Last Seen column (relative time from m_lastSeenMs).
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QString lastSeenStr = relativeTime(m_lastSeenMs.value(info.macAddress, 0LL), now);

    auto makeItem = [&](const QString& text) {
        auto* item = new QTableWidgetItem(text);
        item->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        // Store MAC in every cell of the row for easy retrieval
        item->setData(kMacRole, info.macAddress);
        return item;
    };

    m_radioTable->setItem(row, ColStatus,   makeItem(QStringLiteral("●")));
    m_radioTable->setItem(row, ColName,     makeItem(name));
    m_radioTable->setItem(row, ColBoard,    makeItem(boardStr));
    m_radioTable->setItem(row, ColProtocol, makeItem(protoStr));
    m_radioTable->setItem(row, ColIp,       makeItem(info.address.toString()));
    m_radioTable->setItem(row, ColLastSeen, makeItem(lastSeenStr));
    m_radioTable->setItem(row, ColFirmware, makeItem(QString::number(info.firmwareVersion)));
    m_radioTable->setItem(row, ColInUse,    makeItem(inUseStr));

    // Center the status dot
    if (QTableWidgetItem* dot = m_radioTable->item(row, ColStatus)) {
        dot->setTextAlignment(Qt::AlignCenter);
    }

    applyRowColor(row, info);
}

// Insert or update a row for `info`. If online=false the row uses offline colour.
// Used by onAddManuallyClicked (Task 16) to show the entry immediately after saving.
void ConnectionPanel::upsertRowForInfo(const RadioInfo& info, bool online)
{
    int row = rowForMac(info.macAddress);
    if (row < 0) {
        row = m_radioTable->rowCount();
        m_radioTable->insertRow(row);
    }
    populateRow(row, info);

    if (!online) {
        // Paint the row offline-grey — manually added radios haven't been seen yet
        for (int col = 0; col < ColCount; ++col) {
            QTableWidgetItem* cell = m_radioTable->item(row, col);
            if (cell) {
                cell->setBackground(kColorOffline);
                cell->setForeground(QColor{0x60, 0x60, 0x70});
            }
        }
        if (QTableWidgetItem* inUse = m_radioTable->item(row, ColInUse)) {
            inUse->setText(QStringLiteral("saved"));
        }
    }

    // Auto-select if this is the only row
    if (m_radioTable->rowCount() == 1) {
        m_radioTable->selectRow(0);
    }

    updateButtonStates();
}

int ConnectionPanel::rowForMac(const QString& mac) const
{
    for (int r = 0; r < m_radioTable->rowCount(); ++r) {
        QTableWidgetItem* cell = m_radioTable->item(r, 0);
        if (cell && cell->data(kMacRole).toString() == mac) {
            return r;
        }
    }
    return -1;
}

// Phase 3Q Task 10 — select the table row for `mac` so the user immediately
// sees which radio failed to auto-connect. Defensive: if the row doesn't
// exist yet (the auto-open panel hasn't been populated by discovery), do
// nothing — the user can still see the status-bar message.
void ConnectionPanel::highlightMac(const QString& mac)
{
    const int row = rowForMac(mac);
    if (row >= 0) {
        m_radioTable->selectRow(row);
    }
}

// ---------------------------------------------------------------------------
// Discovery slots
// ---------------------------------------------------------------------------

void ConnectionPanel::onRadioDiscovered(const RadioInfo& info)
{
    // Don't add duplicates
    if (rowForMac(info.macAddress) >= 0) {
        onRadioUpdated(info);
        return;
    }

    m_discoveredRadios.insert(info.macAddress, info);
    // Phase 3Q Task 5: record last-seen timestamp for this MAC
    m_lastSeenMs.insert(info.macAddress, QDateTime::currentMSecsSinceEpoch());

    int row = m_radioTable->rowCount();
    m_radioTable->insertRow(row);
    populateRow(row, info);

    // Auto-select the first radio
    if (m_radioTable->rowCount() == 1) {
        m_radioTable->selectRow(0);
    }

    setStatusText(QStringLiteral("Found %1 radio(s)").arg(m_radioTable->rowCount()));
    updateButtonStates();

    qCDebug(lcDiscovery) << "Panel: radio discovered" << info.displayName()
                         << info.address.toString();
}

void ConnectionPanel::onRadioUpdated(const RadioInfo& info)
{
    m_discoveredRadios.insert(info.macAddress, info);
    // Phase 3Q Task 5: refresh last-seen timestamp on update
    m_lastSeenMs.insert(info.macAddress, QDateTime::currentMSecsSinceEpoch());

    int row = rowForMac(info.macAddress);
    if (row < 0) {
        onRadioDiscovered(info);
        return;
    }

    populateRow(row, info);
    updateButtonStates();
}

void ConnectionPanel::onRadioLost(const QString& macAddress)
{
    m_discoveredRadios.remove(macAddress);

    int row = rowForMac(macAddress);
    if (row >= 0) {
        // Grey out the row rather than removing it (offline state)
        // ucRadioList.cs has no "offline" concept — it removes them.
        // NereusSDR keeps them greyed for user context.
        for (int col = 0; col < ColCount; ++col) {
            QTableWidgetItem* cell = m_radioTable->item(row, col);
            if (cell) {
                cell->setBackground(kColorOffline);
                cell->setForeground(QColor{0x60, 0x60, 0x70});
            }
        }
        if (QTableWidgetItem* inUse = m_radioTable->item(row, ColInUse)) {
            inUse->setText(QStringLiteral("offline"));
        }
    }

    int n = m_radioTable->rowCount();
    setStatusText(n > 0
        ? QStringLiteral("Found %1 radio(s)").arg(m_discoveredRadios.size())
        : QStringLiteral("Searching for radios..."));

    updateButtonStates();
}

// ---------------------------------------------------------------------------
// Connection state
// ---------------------------------------------------------------------------

void ConnectionPanel::onConnectionStateChanged()
{
    if (m_radioModel->isConnected()) {
        setStatusText(QStringLiteral("Connected to %1").arg(m_radioModel->name()));
    } else {
        int n = m_discoveredRadios.size();
        setStatusText(n > 0
            ? QStringLiteral("Found %1 radio(s)").arg(n)
            : QStringLiteral("Disconnected"));
    }
    updateButtonStates();
}

// ---------------------------------------------------------------------------
// Button handlers
// ---------------------------------------------------------------------------

// Phase 3Q Task 5 — single ↻ Scan button replaces Start/Stop Discovery.
// Same broadcast scan; one user-facing action.
// Source: clsDiscoveredRadioPicker.cs — "Start Discovery" drives NIC scan
void ConnectionPanel::onScanClicked()
{
    m_radioModel->discovery()->startDiscovery();
    setStatusText(QStringLiteral("Searching for radios..."));
    m_scanBtn->setEnabled(false);  // re-enabled by discoveryFinished slot
}

void ConnectionPanel::onConnectClicked()
{
    RadioInfo info = selectedRadio();
    if (info.macAddress.isEmpty()) {
        return;
    }

    // Apply model override from dropdown (Phase 3I-RP)
    if (m_modelCombo && m_modelCombo->currentIndex() >= 0) {
        int modelInt = m_modelCombo->itemData(m_modelCombo->currentIndex()).toInt();
        info.modelOverride = static_cast<HPSDRModel>(modelInt);
    }

    if (info.inUse) {
        setStatusText(QStringLiteral("Warning: radio reports in use — attempting connection anyway"));
    } else {
        setStatusText(QStringLiteral("Connecting to %1...").arg(info.displayName()));
    }
    m_connectBtn->setEnabled(false);

    // Phase 3I Task 17 — persist as auto-reconnect target.
    // Compute the same macKey saveRadio uses (MAC if present, else "manual-ip-port").
    // saveRadio updates the autoConnect flag to true for this entry, then
    // setLastConnected records which entry to reconnect to on next launch.
    const QString macKey = info.macAddress.isEmpty()
        ? QStringLiteral("manual-%1-%2").arg(info.address.toString()).arg(info.port)
        : info.macAddress;
    AppSettings& s = AppSettings::instance();
    // Preserve existing pinToMac flag if the radio is already saved; default false.
    bool pinToMac = false;
    if (auto existing = s.savedRadio(macKey)) {
        pinToMac = existing->pinToMac;
    }
    s.saveRadio(info, pinToMac, /*autoConnect=*/true);
    s.setLastConnected(macKey);
    s.save();

    m_radioModel->connectToRadio(info);
}

void ConnectionPanel::onDisconnectClicked()
{
    setStatusText(QStringLiteral("Disconnecting..."));
    m_radioModel->disconnectFromRadio();
}

// Phase 3I Task 16 — Add Manually wired.
// Opens AddCustomRadioDialog (port of Thetis frmAddCustomRadio.cs).
// On OK: saves to AppSettings, adds a row to the table immediately.
void ConnectionPanel::onAddManuallyClicked()
{
    AddCustomRadioDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const RadioInfo info = dlg.result();
    AppSettings::instance().saveRadio(info, dlg.pinToMac(), dlg.autoConnect());
    AppSettings::instance().save();

    // Design §7.4: tell RadioDiscovery this MAC is now saved so it is
    // permanently exempt from the stale-removal sweep.
    if (!info.macAddress.isEmpty()) {
        m_radioModel->discovery()->addSavedMac(info.macAddress);
    }

    // Add the row to the table immediately so the user sees their entry.
    // onRadioDiscovered handles de-duplication via rowForMac.
    upsertRowForInfo(info, /*online=*/false);
    setStatusText(QStringLiteral("Added: %1").arg(info.name));

    qCDebug(lcDiscovery) << "ConnectionPanel: manually added radio"
                         << info.name << info.address.toString();

    // Phase 3Q-4 design §6.3 — "Probe and connect now" path: dialog accept()
    // means the probe succeeded and the user wants to connect immediately.
    // Save-offline path leaves savedOffline()=true; in that case we just save
    // the entry (above) and stop, per design §6.4 / §6.5.
    if (!dlg.savedOffline()) {
        m_radioModel->connectToRadio(info);
        setStatusText(QStringLiteral("Connecting to %1…").arg(info.displayName()));
    }
}

// Phase 3I Task 15 — Forget wired.
// Removes the radio from the persistent saved-radio list and from the table.
void ConnectionPanel::onForgetClicked()
{
    int row = m_radioTable->currentRow();
    if (row < 0) {
        return;
    }
    // Phase 3Q Task 5: MAC is now retrieved via kMacRole (ColMac column removed).
    QTableWidgetItem* cell = m_radioTable->item(row, ColStatus);
    const QString mac = cell ? cell->data(kMacRole).toString() : QString();
    if (mac.isEmpty()) {
        return;
    }

    AppSettings::instance().forgetRadio(mac);
    AppSettings::instance().save();

    // Design §7.4: the user has explicitly forgotten this radio, so remove
    // the MAC from the saved-exempt set — it can now age out of discovery.
    m_radioModel->discovery()->removeSavedMac(mac);

    m_discoveredRadios.remove(mac);
    m_lastSeenMs.remove(mac);
    m_radioTable->removeRow(row);

    int n = m_radioTable->rowCount();
    setStatusText(n > 0
        ? QStringLiteral("Found %1 radio(s)").arg(n)
        : QStringLiteral("No radios — start discovery"));
    updateButtonStates();
}

// ---------------------------------------------------------------------------
// Selection + double-click
// ---------------------------------------------------------------------------

void ConnectionPanel::onRadioSelectionChanged()
{
    updateButtonStates();
    updateDetailPanel();
}

// Source: ucRadioList.cs double-click action — select and connect
// From clsDiscoveredRadioPicker.cs flow: double-click calls Accept on the form
void ConnectionPanel::onTableDoubleClicked(int /*row*/, int /*column*/)
{
    if (!m_connectBtn->isEnabled()) {
        return;
    }
    onConnectClicked();
}

// ---------------------------------------------------------------------------
// Context menu — right-click
// Source: ucRadioList.cs context (trash icon = remove), clsDiscoveredRadioPicker.cs
// NereusSDR provides: Connect, Disconnect, Copy MAC
// Deferred stubs: Forget (Task 15), Edit IP (Task 16)
// ---------------------------------------------------------------------------

void ConnectionPanel::onContextMenuRequested(const QPoint& pos)
{
    QTableWidgetItem* item = m_radioTable->itemAt(pos);
    if (!item) {
        return;
    }

    int row = item->row();
    m_radioTable->selectRow(row);

    RadioInfo info = selectedRadio();
    bool connected = m_radioModel->isConnected();

    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background: #12202e; color: #c8d8e8; border: 1px solid #203040; }"
        "QMenu::item:selected { background: #005578; }"
        "QMenu::item:disabled { color: #404858; }"));

    QAction* actConnect    = menu.addAction(QStringLiteral("Connect"));
    QAction* actDisconnect = menu.addAction(QStringLiteral("Disconnect"));
    menu.addSeparator();
    QAction* actCopyMac    = menu.addAction(QStringLiteral("Copy MAC"));
    menu.addSeparator();

    // Phase 3I Task 15 — Forget wired
    QAction* actForget     = menu.addAction(QStringLiteral("Forget"));
    actForget->setEnabled(!info.macAddress.isEmpty());

    actConnect->setEnabled(!info.macAddress.isEmpty() && !connected);
    actDisconnect->setEnabled(connected);
    actCopyMac->setEnabled(!info.macAddress.isEmpty());

    QAction* triggered = menu.exec(m_radioTable->viewport()->mapToGlobal(pos));
    if (!triggered) {
        return;
    }

    if (triggered == actConnect) {
        onConnectClicked();
    } else if (triggered == actDisconnect) {
        onDisconnectClicked();
    } else if (triggered == actCopyMac) {
        QApplication::clipboard()->setText(info.macAddress);
        setStatusText(QStringLiteral("Copied MAC: %1").arg(info.macAddress));
    } else if (triggered == actForget) {
        onForgetClicked();
    }
}

// ---------------------------------------------------------------------------
// Button state management
// ---------------------------------------------------------------------------

void ConnectionPanel::updateButtonStates()
{
    bool hasSelection = (m_radioTable->currentRow() >= 0);
    bool connected    = m_radioModel->isConnected();

    // Selected row must not be an "offline" row to allow connect
    bool canConnect = false;
    if (hasSelection) {
        QTableWidgetItem* inUseCell = m_radioTable->item(
            m_radioTable->currentRow(), ColInUse);
        if (inUseCell && inUseCell->text() != QStringLiteral("offline")) {
            canConnect = true;
        }
    }

    m_connectBtn->setEnabled(canConnect && !connected);
    // Phase 3Q Task 5: m_disconnectBtn removed from bottom strip.
    // Disconnect lives in the status strip (m_stripDisconnectBtn).
    // Forget: enabled when a row with a MAC is selected (saved or discovered)
    m_forgetBtn->setEnabled(hasSelection && !selectedRadio().macAddress.isEmpty());
}

// ---------------------------------------------------------------------------
// Selected radio lookup
// ---------------------------------------------------------------------------

RadioInfo ConnectionPanel::selectedRadio() const
{
    int row = m_radioTable->currentRow();
    if (row < 0) {
        return {};
    }
    QTableWidgetItem* cell = m_radioTable->item(row, 0);
    if (!cell) {
        return {};
    }
    QString mac = cell->data(kMacRole).toString();
    return m_discoveredRadios.value(mac);
}

// ---------------------------------------------------------------------------
// Detail panel — Phase 3I-RP
// ---------------------------------------------------------------------------

void ConnectionPanel::updateDetailPanel()
{
    RadioInfo info = selectedRadio();
    if (info.macAddress.isEmpty()) {
        m_detailGroup->setVisible(false);
        return;
    }

    m_detailGroup->setVisible(true);

    QString boardName = QString::fromLatin1(
        BoardCapsTable::forBoard(info.boardType).displayName);
    m_detailBoardLabel->setText(QStringLiteral("Board: %1 (0x%2)")
        .arg(boardName)
        .arg(static_cast<int>(info.boardType), 2, 16, QLatin1Char('0')));
    m_detailProtoLabel->setText(QStringLiteral("Protocol: P%1")
        .arg(info.protocol == ProtocolVersion::Protocol2 ? 2 : 1));
    m_detailFwLabel->setText(QStringLiteral("Firmware: %1").arg(info.firmwareVersion));
    m_detailIpLabel->setText(QStringLiteral("IP: %1").arg(info.address.toString()));
    m_detailMacLabel->setText(QStringLiteral("MAC: %1").arg(info.macAddress));

    // Populate model combo
    m_modelCombo->blockSignals(true);
    m_modelCombo->clear();
    QList<HPSDRModel> models = compatibleModels(info.boardType);
    HPSDRModel defaultModel = defaultModelForBoard(info.boardType);

    HPSDRModel persisted = AppSettings::instance().modelOverride(info.macAddress);
    HPSDRModel selected = (persisted != HPSDRModel::FIRST) ? persisted : defaultModel;

    int selectIdx = 0;
    for (int i = 0; i < models.size(); ++i) {
        m_modelCombo->addItem(
            QString::fromLatin1(displayName(models[i])),
            static_cast<int>(models[i]));
        if (models[i] == selected) {
            selectIdx = i;
        }
    }
    m_modelCombo->setCurrentIndex(selectIdx);
    m_modelCombo->blockSignals(false);

    if (selected != defaultModel) {
        m_modelHintLabel->setText(QStringLiteral("Board reports \"%1\" -- model override applied")
            .arg(boardName));
        m_modelHintLabel->setVisible(true);
    } else {
        m_modelHintLabel->setVisible(false);
    }

    // Phase 3Q Task 5: populate auto-connect checkbox from AppSettings.
    // Use QSignalBlocker to prevent the toggled() handler from echoing back.
    if (m_autoConnectCheck) {
        QSignalBlocker blocker(m_autoConnectCheck);
        bool autoConn = false;
        if (auto saved = AppSettings::instance().savedRadio(info.macAddress)) {
            autoConn = saved->autoConnect;
        }
        m_autoConnectCheck->setChecked(autoConn);
    }
}

void ConnectionPanel::onModelComboChanged(int index)
{
    if (index < 0) { return; }
    RadioInfo info = selectedRadio();
    if (info.macAddress.isEmpty()) { return; }

    int modelInt = m_modelCombo->itemData(index).toInt();
    auto model = static_cast<HPSDRModel>(modelInt);

    AppSettings::instance().setModelOverride(info.macAddress, model);
    AppSettings::instance().save();

    HPSDRModel defaultModel = defaultModelForBoard(info.boardType);
    QString boardName = QString::fromLatin1(
        BoardCapsTable::forBoard(info.boardType).displayName);
    if (model != defaultModel) {
        m_modelHintLabel->setText(QStringLiteral("Board reports \"%1\" -- model override applied")
            .arg(boardName));
        m_modelHintLabel->setVisible(true);
    } else {
        m_modelHintLabel->setVisible(false);
    }

    setStatusText(QStringLiteral("Model set to %1 for %2")
        .arg(QString::fromLatin1(displayName(model)), info.macAddress));
}

} // namespace NereusSDR
