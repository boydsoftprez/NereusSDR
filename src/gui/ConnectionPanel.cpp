// src/gui/ConnectionPanel.cpp
//
// Thetis ucRadioList port — radio list UI for NereusSDR.
//
// Porting notes:
//   ucRadioList.cs (ramdor/Thetis) — custom-painted card list, 4 info lines per row:
//     Line 1 (bold):  RadioModel  IP  MAC              (buildLine1, ~:1276)
//     Line 2:         Protocol     Version  [PLL]      (buildLine2, ~:1324)
//     Line 3:         NIC description                  (buildNicLine3, ~:1383)
//     Line 4:         NIC IP/mask  type                (buildNicLine4, ~:1406)
//   Color coding: connectedFill green, selectedFill blue, hoverFill light-blue (~:1115)
//
//   clsDiscoveredRadioPicker.cs — DataGridView columns (~:99):
//     Pick(checkbox) | Hardware | IP | Base Port | Mac Address | Protocol | Version
//
//   NereusSDR maps both sources to QTableWidget with 8 columns per plan §5.2.

#include "ConnectionPanel.h"
#include "AddCustomRadioDialog.h"
#include "models/RadioModel.h"
#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QColor>

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
static constexpr QColor kColorError       { 80, 20, 20};    // dark red — error

ConnectionPanel::ConnectionPanel(RadioModel* model, QWidget* parent)
    : QDialog(parent)
    , m_radioModel(model)
{
    setWindowTitle(QStringLiteral("Connect to Radio"));
    setMinimumSize(750, 420);
    resize(860, 480);

    buildUI();

    // Wire discovery signals
    RadioDiscovery* disc = m_radioModel->discovery();
    connect(disc, &RadioDiscovery::radioDiscovered, this, &ConnectionPanel::onRadioDiscovered);
    connect(disc, &RadioDiscovery::radioUpdated,    this, &ConnectionPanel::onRadioUpdated);
    connect(disc, &RadioDiscovery::radioLost,       this, &ConnectionPanel::onRadioLost);
    connect(disc, &RadioDiscovery::discoveryStarted,  this, [this]() {
        setStatusText(QStringLiteral("Searching for radios..."));
        m_startDiscoveryBtn->setEnabled(false);
        m_stopDiscoveryBtn->setEnabled(true);
    });
    connect(disc, &RadioDiscovery::discoveryFinished, this, [this]() {
        int n = m_radioTable->rowCount();
        setStatusText(n > 0
            ? QStringLiteral("Found %1 radio(s)").arg(n)
            : QStringLiteral("No radios found — try again"));
        m_startDiscoveryBtn->setEnabled(true);
        m_stopDiscoveryBtn->setEnabled(false);
    });

    // Wire connection state
    connect(m_radioModel, &RadioModel::connectionStateChanged,
            this, &ConnectionPanel::onConnectionStateChanged);

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

    // Column headers — mapping clsDiscoveredRadioPicker.cs columns (~:99) to 8-col layout
    // clsDiscoveredRadioPicker.cs columns: Hardware(:111), IP(:118), Base Port(:126),
    //   Mac Address(:134), Protocol(:142), Version(:150)
    // NereusSDR adds: status dot (col 0), Board type (col 2), In-Use (col 7)
    QStringList headers;
    headers << QStringLiteral("●")           // Col 0 — status dot
            << QStringLiteral("Name")        // Col 1 — clsDiscoveredRadioPicker.cs:112 "Hardware"
            << QStringLiteral("Board")       // Col 2 — HPSDRHW board type name
            << QStringLiteral("Protocol")    // Col 3 — clsDiscoveredRadioPicker.cs:143 "Protocol"
            << QStringLiteral("IP")          // Col 4 — clsDiscoveredRadioPicker.cs:119 "IP"
            << QStringLiteral("MAC")         // Col 5 — clsDiscoveredRadioPicker.cs:135 "Mac Address"
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
    m_radioTable->setColumnWidth(ColStatus,   28);
    m_radioTable->setColumnWidth(ColName,    180);
    m_radioTable->setColumnWidth(ColBoard,   100);
    m_radioTable->setColumnWidth(ColProtocol, 60);
    m_radioTable->setColumnWidth(ColIp,      130);
    m_radioTable->setColumnWidth(ColMac,     140);
    m_radioTable->setColumnWidth(ColFirmware, 70);
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

    // Start Discovery button
    m_startDiscoveryBtn = makeBtn(QStringLiteral("Start Discovery"), kSecondaryStyle);
    connect(m_startDiscoveryBtn, &QPushButton::clicked,
            this, &ConnectionPanel::onStartDiscoveryClicked);
    btnLayout->addWidget(m_startDiscoveryBtn);

    // Stop Discovery button
    m_stopDiscoveryBtn = makeBtn(QStringLiteral("Stop Discovery"), kSecondaryStyle);
    m_stopDiscoveryBtn->setEnabled(false);
    connect(m_stopDiscoveryBtn, &QPushButton::clicked,
            this, &ConnectionPanel::onStopDiscoveryClicked);
    btnLayout->addWidget(m_stopDiscoveryBtn);

    btnLayout->addStretch();

    // Add Manually — Phase 3I Task 16
    // Source: frmAddCustomRadio.cs — port of the Thetis "Add Custom Radio" dialog
    m_addManuallyBtn = makeBtn(QStringLiteral("Add Manually"), kSecondaryStyle);
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

    // Disconnect
    m_disconnectBtn = makeBtn(QStringLiteral("Disconnect"), kSecondaryStyle);
    m_disconnectBtn->setEnabled(false);
    connect(m_disconnectBtn, &QPushButton::clicked,
            this, &ConnectionPanel::onDisconnectClicked);
    btnLayout->addWidget(m_disconnectBtn);

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
void ConnectionPanel::applyRowColor(int row, const RadioInfo& info)
{
    QColor bg;
    QColor fg{0xc8, 0xd8, 0xe8};

    if (info.inUse) {
        bg = kColorOnlineInUse;
        fg = QColor{0xe0, 0xc0, 0x80};
    } else {
        // Radios in m_discoveredRadios are always "online" — seen in last scan.
        // Offline tracking would require a separate set; for now all discovered = online+free.
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
}

// Populate a table row from a RadioInfo.
// Column mapping per plan §5.2 and clsDiscoveredRadioPicker.cs:
//   Col 0 ●  : status dot — "●" (U+25CF)
//   Col 1 Name: displayName() or BoardCapsTable fallback
//             (clsDiscoveredRadioPicker.cs:112 "Hardware")
//   Col 2 Board: HPSDRHW enum name
//   Col 3 Protocol: "P1" / "P2"  (clsDiscoveredRadioPicker.cs:143 "Protocol")
//   Col 4 IP: address.toString() (clsDiscoveredRadioPicker.cs:119 "IP")
//   Col 5 MAC: macAddress        (clsDiscoveredRadioPicker.cs:135 "Mac Address")
//   Col 6 Firmware: firmwareVersion (clsDiscoveredRadioPicker.cs:151 "Version")
//   Col 7 In-Use: "free" / "in use" (ucRadioList.cs:101 RadioIsBusy)
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
    m_radioTable->setItem(row, ColMac,      makeItem(info.macAddress));
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

// Source: clsDiscoveredRadioPicker.cs — "Start Discovery" drives NIC scan
void ConnectionPanel::onStartDiscoveryClicked()
{
    // Clear offline rows from previous scan
    for (int r = m_radioTable->rowCount() - 1; r >= 0; --r) {
        QTableWidgetItem* cell = m_radioTable->item(r, ColInUse);
        if (cell && cell->text() == QStringLiteral("offline")) {
            m_radioTable->removeRow(r);
        }
    }
    m_radioModel->discovery()->startDiscovery();
    setStatusText(QStringLiteral("Searching for radios..."));
    m_startDiscoveryBtn->setEnabled(false);
    m_stopDiscoveryBtn->setEnabled(true);
}

void ConnectionPanel::onStopDiscoveryClicked()
{
    m_radioModel->discovery()->stopDiscovery();
    m_startDiscoveryBtn->setEnabled(true);
    m_stopDiscoveryBtn->setEnabled(false);
    int n = m_discoveredRadios.size();
    setStatusText(n > 0
        ? QStringLiteral("Found %1 radio(s)").arg(n)
        : QStringLiteral("Discovery stopped"));
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

    // Add the row to the table immediately so the user sees their entry.
    // onRadioDiscovered handles de-duplication via rowForMac.
    upsertRowForInfo(info, /*online=*/false);
    setStatusText(QStringLiteral("Added: %1").arg(info.name));

    qCDebug(lcDiscovery) << "ConnectionPanel: manually added radio"
                         << info.name << info.address.toString();
}

// Phase 3I Task 15 — Forget wired.
// Removes the radio from the persistent saved-radio list and from the table.
void ConnectionPanel::onForgetClicked()
{
    int row = m_radioTable->currentRow();
    if (row < 0) {
        return;
    }
    QTableWidgetItem* cell = m_radioTable->item(row, ColMac);
    const QString mac = cell ? cell->text() : QString();
    if (mac.isEmpty()) {
        return;
    }

    AppSettings::instance().forgetRadio(mac);
    AppSettings::instance().save();

    m_discoveredRadios.remove(mac);
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
    m_disconnectBtn->setEnabled(connected);
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
