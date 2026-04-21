// =================================================================
// src/gui/setup/hardware/Hl2IoBoardTab.cpp  (NereusSDR)
// =================================================================
//
// Ported from mi0bot-Thetis sources:
//   Project Files/Source/Console/setup.cs (~lines 20234-20238 chkHERCULES
//     N2ADR Filter toggle + surrounding HL2 I/O UI)
//   Project Files/Source/Console/console.cs:25781-25945 (UpdateIOBoard
//     state machine driving register state; subscribed via IoBoardHl2 model)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Phase 3I placeholder (GPIO combos only).
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Replaces Phase 3I empty placeholder; surfaces
//                IoBoardHl2 + HermesLiteBandwidthMonitor state for HL2
//                diagnostics. NereusSDR spin: register state table + I2C
//                transaction log + state-machine viz + bandwidth mini are
//                pure NereusSDR diagnostic surfaces (mi0bot doesn't expose
//                them in the Thetis UI).
// =================================================================
//
// --- From Console/setup.cs ---
//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
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
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
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
// =================================================================
//
// --- From Console/console.cs ---
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
// =================================================================

#include "Hl2IoBoardTab.h"

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/HermesLiteBandwidthMonitor.h"
#include "core/HpsdrModel.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QDateTime>
#include <QFont>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace NereusSDR {

// ── Register display table rows ───────────────────────────────────────────────
// 8 principal registers per spec §10 mockup.
// I2C addresses: general registers at 0x1D (IoBoardHl2.cs:139 [@c26a8a4]),
// hardware version at 0x41 (IoBoardHl2.cs:140 [@c26a8a4]).
const Hl2IoBoardTab::RegRow Hl2IoBoardTab::kRegRows[Hl2IoBoardTab::kRegRowCount] = {
    { IoBoardHl2::Register::HardwareVersion,   "HardwareVersion",  0x41 },
    { IoBoardHl2::Register::REG_CONTROL,       "REG_CONTROL",      0x1D },
    { IoBoardHl2::Register::REG_OP_MODE,       "REG_OP_MODE",      0x1D },
    { IoBoardHl2::Register::REG_ANTENNA,       "REG_ANTENNA",      0x1D },
    { IoBoardHl2::Register::REG_RF_INPUTS,     "REG_RF_INPUTS",    0x1D },
    { IoBoardHl2::Register::REG_INPUT_PINS,    "REG_INPUT_PINS",   0x1D },
    { IoBoardHl2::Register::REG_ANTENNA_TUNER, "REG_ANTENNA_TUNER",0x1D },
    { IoBoardHl2::Register::REG_FAULT,         "REG_FAULT",        0x1D },
};

// ── LED helper ────────────────────────────────────────────────────────────────
static QFrame* makeLed(QWidget* parent)
{
    auto* led = new QFrame(parent);
    led->setFixedSize(12, 12);
    led->setFrameShape(QFrame::NoFrame);
    led->setStyleSheet(QStringLiteral(
        "QFrame { background: #606060; border-radius: 6px; }"));
    return led;
}

static void setLedColor(QFrame* led, bool active)
{
    if (active) {
        led->setStyleSheet(QStringLiteral(
            "QFrame { background: #22cc44; border-radius: 6px; }"));
    } else {
        led->setStyleSheet(QStringLiteral(
            "QFrame { background: #606060; border-radius: 6px; }"));
    }
}

// ── Constructor ───────────────────────────────────────────────────────────────

Hl2IoBoardTab::Hl2IoBoardTab(RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_ioBoard(&model->ioBoardMutable())
    , m_bwMonitor(&model->bwMonitorMutable())
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 8, 8, 8);
    outerLayout->setSpacing(6);

    buildStatusBar(outerLayout);
    buildConfigAndRegisterRow(outerLayout);
    buildStateMachineRow(outerLayout);
    buildI2cAndBandwidthRow(outerLayout);
    outerLayout->addStretch();

    // ── Signal wiring ─────────────────────────────────────────────────────────
    connect(m_ioBoard, &IoBoardHl2::detectedChanged,
            this, &Hl2IoBoardTab::onDetectedChanged);
    connect(m_ioBoard, &IoBoardHl2::registerChanged,
            this, &Hl2IoBoardTab::onRegisterChanged);
    connect(m_ioBoard, &IoBoardHl2::stepAdvanced,
            this, &Hl2IoBoardTab::onStepAdvanced);
    connect(m_ioBoard, &IoBoardHl2::i2cQueueChanged,
            this, &Hl2IoBoardTab::onI2cQueueChanged);
    connect(m_bwMonitor, &HermesLiteBandwidthMonitor::throttledChanged,
            this, &Hl2IoBoardTab::onThrottledChanged);

    // 250 ms timer for live bandwidth rate readouts
    m_bwTimer = new QTimer(this);
    m_bwTimer->setInterval(250);
    connect(m_bwTimer, &QTimer::timeout, this, &Hl2IoBoardTab::onBwTimerTick);
    m_bwTimer->start();

    // ── Auto-hide for non-HL2 boards ──────────────────────────────────────────
    // From mi0bot setup.cs:20234 chkHERCULES — only visible for HermesLite [@c26a8a4]
    const bool isHl2 =
        (m_model->hardwareProfile().model == HPSDRModel::HERMESLITE);
    setVisible(isHl2);

    // Populate initial state
    updateStatusBar(m_ioBoard->isDetected());
    refreshAllRegisters();
    highlightStep(m_ioBoard->currentStep());
    updateBwDisplay();
}

Hl2IoBoardTab::~Hl2IoBoardTab() = default;

// ── buildStatusBar ────────────────────────────────────────────────────────────

void Hl2IoBoardTab::buildStatusBar(QVBoxLayout* outer)
{
    m_statusFrame = new QFrame(this);
    m_statusFrame->setFrameShape(QFrame::StyledPanel);
    m_statusFrame->setStyleSheet(QStringLiteral(
        "QFrame { background: #2a2a2a; border: 1px solid #444; border-radius: 4px; }"));

    auto* row = new QHBoxLayout(m_statusFrame);
    row->setContentsMargins(8, 4, 8, 4);
    row->setSpacing(8);

    m_statusLed = makeLed(m_statusFrame);
    row->addWidget(m_statusLed);

    m_statusLabel = new QLabel(tr("HL2 I/O board: Not detected"), m_statusFrame);
    QFont bold = m_statusLabel->font();
    bold.setBold(true);
    m_statusLabel->setFont(bold);
    row->addWidget(m_statusLabel);

    row->addStretch();

    // I2C address (static — 0x1D per mi0bot IoBoardHl2.cs:139 [@c26a8a4])
    m_i2cAddrLabel = new QLabel(
        QStringLiteral("I2C addr: 0x%1")
            .arg(IoBoardHl2::kI2cAddrGeneral, 2, 16, QLatin1Char('0')).toUpper(),
        m_statusFrame);
    m_i2cAddrLabel->setStyleSheet(QStringLiteral("color: #aaa; font-size: 10px;"));
    row->addWidget(m_i2cAddrLabel);

    m_lastProbeLabel = new QLabel(tr("Last probe: —"), m_statusFrame);
    m_lastProbeLabel->setStyleSheet(QStringLiteral("color: #aaa; font-size: 10px;"));
    row->addWidget(m_lastProbeLabel);

    outer->addWidget(m_statusFrame);
}

// ── buildConfigAndRegisterRow ─────────────────────────────────────────────────

void Hl2IoBoardTab::buildConfigAndRegisterRow(QVBoxLayout* outer)
{
    auto* row = new QHBoxLayout;
    row->setSpacing(8);

    // ── Left: Configuration ───────────────────────────────────────────────────
    auto* configGroup = new QGroupBox(tr("Configuration"), this);
    auto* configLayout = new QVBoxLayout(configGroup);
    configLayout->setSpacing(6);

    // N2ADR Filter enable — ports mi0bot setup.cs chkHERCULES [@c26a8a4]
    // chkHERCULES: "Enable N2ADR Filter board" toggle in setup.cs:20234-20238
    m_n2adrFilter = new QCheckBox(tr("Enable N2ADR Filter board"), configGroup);
    configLayout->addWidget(m_n2adrFilter);

    auto* noteLabel = new QLabel(
        tr("<i>Auto-switch LPFs based on TX frequency.\n"
           "Disable for manual OC pin control.</i>"),
        configGroup);
    noteLabel->setWordWrap(true);
    noteLabel->setStyleSheet(QStringLiteral("color: #aaa; font-size: 10px;"));
    configLayout->addWidget(noteLabel);

    // Decoded register quick-view (read-only form rows)
    auto* formWidget = new QWidget(configGroup);
    auto* form = new QVBoxLayout(formWidget);
    form->setSpacing(2);
    form->setContentsMargins(0, 4, 0, 0);

    auto makeRow = [&](const QString& label, QLabel*& valueLabel) {
        auto* rowW = new QWidget(formWidget);
        auto* rowL = new QHBoxLayout(rowW);
        rowL->setContentsMargins(0, 0, 0, 0);
        rowL->setSpacing(4);
        auto* lbl = new QLabel(label, rowW);
        lbl->setStyleSheet(QStringLiteral("color: #aaa; font-size: 10px;"));
        lbl->setFixedWidth(160);
        valueLabel = new QLabel(QStringLiteral("—"), rowW);
        valueLabel->setStyleSheet(QStringLiteral("font-size: 10px; font-family: monospace;"));
        rowL->addWidget(lbl);
        rowL->addWidget(valueLabel);
        rowL->addStretch();
        form->addWidget(rowW);
    };

    makeRow(tr("Op mode (REG_OP_MODE):"),    m_opModeValue);
    makeRow(tr("Antenna (REG_ANTENNA):"),    m_antennaValue);
    makeRow(tr("RX2 input (REG_RF_INPUTS):"),m_rfInputsValue);
    makeRow(tr("Antenna tuner:"),            m_antTunerValue);
    configLayout->addWidget(formWidget);

    configLayout->addStretch();

    // Buttons
    auto* btnRow = new QHBoxLayout;
    m_probeButton = new QPushButton(tr("Probe board"), configGroup);
    m_resetButton = new QPushButton(tr("Reset I/O board"), configGroup);
    m_resetButton->setStyleSheet(QStringLiteral(
        "QPushButton { color: #ff6666; border: 1px solid #ff6666; "
        "border-radius: 3px; padding: 2px 6px; }"));
    btnRow->addWidget(m_probeButton);
    btnRow->addWidget(m_resetButton);
    configLayout->addLayout(btnRow);

    row->addWidget(configGroup, 1);

    // ── Right: Register state table ───────────────────────────────────────────
    auto* regGroup = new QGroupBox(tr("Register state  (polled @ 40 ms)"), this);
    auto* regLayout = new QVBoxLayout(regGroup);

    m_registerTable = new QTableWidget(kRegRowCount, 4, regGroup);
    m_registerTable->setHorizontalHeaderLabels(
        { tr("Register"), tr("Addr"), tr("Value (hex)"), tr("Decoded") });
    m_registerTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_registerTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_registerTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_registerTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_registerTable->verticalHeader()->setVisible(false);
    m_registerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_registerTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_registerTable->setAlternatingRowColors(true);

    // Populate static columns (name + addr); value + decoded filled by refreshAllRegisters()
    for (int i = 0; i < kRegRowCount; ++i) {
        m_registerTable->setItem(i, 0,
            new QTableWidgetItem(QString::fromLatin1(kRegRows[i].name)));
        m_registerTable->setItem(i, 1,
            new QTableWidgetItem(
                QStringLiteral("0x%1").arg(kRegRows[i].displayAddr, 2, 16,
                                            QLatin1Char('0')).toUpper()));
        m_registerTable->setItem(i, 2, new QTableWidgetItem(QStringLiteral("0x00")));
        m_registerTable->setItem(i, 3, new QTableWidgetItem(QStringLiteral("—")));
    }

    regLayout->addWidget(m_registerTable);
    row->addWidget(regGroup, 2);

    outer->addLayout(row);

    // ── Connections ───────────────────────────────────────────────────────────
    // N2ADR Filter checkbox — saved to AppSettings
    // From mi0bot setup.cs:20234-20238 chkHERCULES [@c26a8a4]
    connect(m_n2adrFilter, &QCheckBox::toggled,
            this, &Hl2IoBoardTab::onN2adrToggled);

    connect(m_probeButton, &QPushButton::clicked,
            this, &Hl2IoBoardTab::onProbeClicked);
    connect(m_resetButton, &QPushButton::clicked,
            this, &Hl2IoBoardTab::onResetClicked);
}

// ── buildStateMachineRow ──────────────────────────────────────────────────────

void Hl2IoBoardTab::buildStateMachineRow(QVBoxLayout* outer)
{
    // 12-step UpdateIOBoard state machine per mi0bot console.cs:25844-25928 [@c26a8a4]
    auto* smGroup = new QGroupBox(tr("State machine  (UpdateIOBoard — 12 steps)"), this);
    auto* smLayout = new QHBoxLayout(smGroup);
    smLayout->setSpacing(4);

    for (int i = 0; i < kSteps; ++i) {
        // Each cell: vertical layout — numbered circle + descriptor
        auto* cell = new QFrame(smGroup);
        cell->setFrameShape(QFrame::StyledPanel);
        cell->setStyleSheet(QStringLiteral(
            "QFrame { border: 1px solid #444; border-radius: 4px; "
            "background: #222; padding: 2px; }"));
        auto* cellLayout = new QVBoxLayout(cell);
        cellLayout->setSpacing(2);
        cellLayout->setContentsMargins(4, 2, 4, 2);

        // Step number circle (label styled as circle)
        auto* numLabel = new QLabel(QString::number(i), cell);
        numLabel->setAlignment(Qt::AlignCenter);
        numLabel->setStyleSheet(QStringLiteral(
            "QLabel { background: #444; border-radius: 8px; "
            "font-weight: bold; font-size: 10px; min-width: 16px; "
            "max-width: 16px; min-height: 16px; max-height: 16px; }"));
        cellLayout->addWidget(numLabel, 0, Qt::AlignHCenter);

        // Step descriptor
        auto* descLabel = new QLabel(m_ioBoard->stepDescriptor(i), cell);
        descLabel->setAlignment(Qt::AlignCenter);
        descLabel->setWordWrap(true);
        descLabel->setStyleSheet(QStringLiteral(
            "QLabel { font-size: 9px; color: #aaa; }"));
        cellLayout->addWidget(descLabel);

        m_stepFrames[i]     = cell;
        m_stepNumLabels[i]  = numLabel;
        m_stepDescLabels[i] = descLabel;

        smLayout->addWidget(cell, 1);
    }

    outer->addWidget(smGroup);
}

// ── buildI2cAndBandwidthRow ───────────────────────────────────────────────────

void Hl2IoBoardTab::buildI2cAndBandwidthRow(QVBoxLayout* outer)
{
    auto* row = new QHBoxLayout;
    row->setSpacing(8);

    // ── Left: I2C transaction log ─────────────────────────────────────────────
    auto* i2cGroup = new QGroupBox(
        QStringLiteral("I2C transaction log  (queue depth: 0 / %1)")
            .arg(IoBoardHl2::kMaxI2cQueue),
        this);
    auto* i2cLayout = new QVBoxLayout(i2cGroup);

    m_i2cLog = new QListWidget(i2cGroup);
    QFont mono;
    mono.setFamily(QStringLiteral("Monospace"));
    mono.setStyleHint(QFont::TypeWriter);
    mono.setPointSize(9);
    m_i2cLog->setFont(mono);
    m_i2cLog->setAlternatingRowColors(true);
    m_i2cLog->setSelectionMode(QAbstractItemView::NoSelection);
    m_i2cLog->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Store pointer to group box for title updates in onI2cQueueChanged
    m_queueDepthLabel = new QLabel(i2cGroup);  // hidden, used for group title update
    m_queueDepthLabel->hide();

    i2cLayout->addWidget(m_i2cLog);
    row->addWidget(i2cGroup, 3);

    // ── Right: Bandwidth monitor mini ─────────────────────────────────────────
    auto* bwGroup = new QGroupBox(tr("Bandwidth monitor"), this);
    auto* bwLayout = new QVBoxLayout(bwGroup);
    bwLayout->setSpacing(4);

    // EP6 ingress bar
    auto* ep6Row = new QHBoxLayout;
    auto* ep6Lbl = new QLabel(tr("EP6 ingress (RX I/Q):"), bwGroup);
    ep6Lbl->setFixedWidth(180);
    ep6Lbl->setStyleSheet(QStringLiteral("font-size: 10px;"));
    m_ep6Bar = new QProgressBar(bwGroup);
    m_ep6Bar->setRange(0, 100);
    m_ep6Bar->setValue(0);
    m_ep6Bar->setTextVisible(false);
    m_ep6Bar->setFixedHeight(14);
    m_ep6RateLabel = new QLabel(QStringLiteral("0.0 Mbps"), bwGroup);
    m_ep6RateLabel->setStyleSheet(QStringLiteral("font-size: 10px; font-family: monospace;"));
    m_ep6RateLabel->setFixedWidth(70);
    ep6Row->addWidget(ep6Lbl);
    ep6Row->addWidget(m_ep6Bar, 1);
    ep6Row->addWidget(m_ep6RateLabel);
    bwLayout->addLayout(ep6Row);

    // EP2 egress bar
    auto* ep2Row = new QHBoxLayout;
    auto* ep2Lbl = new QLabel(tr("EP2 egress (audio + C&C):"), bwGroup);
    ep2Lbl->setFixedWidth(180);
    ep2Lbl->setStyleSheet(QStringLiteral("font-size: 10px;"));
    m_ep2Bar = new QProgressBar(bwGroup);
    m_ep2Bar->setRange(0, 100);
    m_ep2Bar->setValue(0);
    m_ep2Bar->setTextVisible(false);
    m_ep2Bar->setFixedHeight(14);
    m_ep2RateLabel = new QLabel(QStringLiteral("0.0 Mbps"), bwGroup);
    m_ep2RateLabel->setStyleSheet(QStringLiteral("font-size: 10px; font-family: monospace;"));
    m_ep2RateLabel->setFixedWidth(70);
    ep2Row->addWidget(ep2Lbl);
    ep2Row->addWidget(m_ep2Bar, 1);
    ep2Row->addWidget(m_ep2RateLabel);
    bwLayout->addLayout(ep2Row);

    bwLayout->addSpacing(6);

    // Throttle status
    auto* throttleRow = new QHBoxLayout;
    auto* throttleLbl = new QLabel(tr("LAN PHY throttle:"), bwGroup);
    throttleLbl->setStyleSheet(QStringLiteral("font-size: 10px;"));
    m_throttleStatusLabel = new QLabel(tr("○ not throttled"), bwGroup);
    m_throttleStatusLabel->setStyleSheet(
        QStringLiteral("font-size: 10px; color: #22cc44;"));
    throttleRow->addWidget(throttleLbl);
    throttleRow->addWidget(m_throttleStatusLabel);
    throttleRow->addStretch();
    bwLayout->addLayout(throttleRow);

    // Dropped frames (throttle event count proxy)
    auto* droppedRow = new QHBoxLayout;
    auto* droppedLbl = new QLabel(tr("Dropped frames:"), bwGroup);
    droppedLbl->setStyleSheet(QStringLiteral("font-size: 10px;"));
    m_throttleEventLabel = new QLabel(QStringLiteral("0"), bwGroup);
    m_throttleEventLabel->setStyleSheet(
        QStringLiteral("font-size: 10px; font-family: monospace;"));
    droppedRow->addWidget(droppedLbl);
    droppedRow->addWidget(m_throttleEventLabel);
    droppedRow->addStretch();
    bwLayout->addLayout(droppedRow);

    bwLayout->addStretch();
    row->addWidget(bwGroup, 2);

    outer->addLayout(row);
}

// ── updateStatusBar ───────────────────────────────────────────────────────────

void Hl2IoBoardTab::updateStatusBar(bool detected)
{
    setLedColor(m_statusLed, detected);

    if (detected) {
        m_statusLabel->setText(tr("HL2 I/O board: Active"));
        m_statusFrame->setStyleSheet(QStringLiteral(
            "QFrame { background: #1a2f1a; border: 1px solid #2a5a2a; border-radius: 4px; }"));
        m_lastProbeLabel->setText(
            QStringLiteral("Last probe: %1")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"))));
    } else {
        m_statusLabel->setText(tr("HL2 I/O board: Not detected"));
        m_statusFrame->setStyleSheet(QStringLiteral(
            "QFrame { background: #2a2a2a; border: 1px solid #444; border-radius: 4px; }"));
    }
}

// ── refreshAllRegisters ───────────────────────────────────────────────────────

void Hl2IoBoardTab::refreshAllRegisters()
{
    for (int i = 0; i < kRegRowCount; ++i) {
        const quint8 val = m_ioBoard->registerValue(kRegRows[i].reg);

        if (auto* item = m_registerTable->item(i, 2)) {
            item->setText(QStringLiteral("0x%1").arg(val, 2, 16, QLatin1Char('0')).toUpper());
        }
        if (auto* item = m_registerTable->item(i, 3)) {
            item->setText(decodeRegister(kRegRows[i].reg, val));
        }
    }

    // Refresh quick-view labels
    m_opModeValue->setText(
        QStringLiteral("0x%1")
            .arg(m_ioBoard->registerValue(IoBoardHl2::Register::REG_OP_MODE),
                 2, 16, QLatin1Char('0')).toUpper());
    m_antennaValue->setText(
        QStringLiteral("0x%1")
            .arg(m_ioBoard->registerValue(IoBoardHl2::Register::REG_ANTENNA),
                 2, 16, QLatin1Char('0')).toUpper());
    m_rfInputsValue->setText(
        QStringLiteral("0x%1")
            .arg(m_ioBoard->registerValue(IoBoardHl2::Register::REG_RF_INPUTS),
                 2, 16, QLatin1Char('0')).toUpper());
    m_antTunerValue->setText(
        QStringLiteral("0x%1")
            .arg(m_ioBoard->registerValue(IoBoardHl2::Register::REG_ANTENNA_TUNER),
                 2, 16, QLatin1Char('0')).toUpper());
}

// ── highlightStep ─────────────────────────────────────────────────────────────

void Hl2IoBoardTab::highlightStep(int step)
{
    // Clear previous highlight
    if (m_currentHighlightedStep >= 0 && m_currentHighlightedStep < kSteps) {
        m_stepFrames[m_currentHighlightedStep]->setStyleSheet(QStringLiteral(
            "QFrame { border: 1px solid #444; border-radius: 4px; "
            "background: #222; padding: 2px; }"));
        m_stepNumLabels[m_currentHighlightedStep]->setStyleSheet(QStringLiteral(
            "QLabel { background: #444; border-radius: 8px; "
            "font-weight: bold; font-size: 10px; min-width: 16px; "
            "max-width: 16px; min-height: 16px; max-height: 16px; }"));
    }

    m_currentHighlightedStep = step;

    if (step >= 0 && step < kSteps) {
        m_stepFrames[step]->setStyleSheet(QStringLiteral(
            "QFrame { border: 2px solid #22cc44; border-radius: 4px; "
            "background: #1a2f1a; padding: 2px; }"));
        m_stepNumLabels[step]->setStyleSheet(QStringLiteral(
            "QLabel { background: #22cc44; border-radius: 8px; "
            "font-weight: bold; font-size: 10px; color: #000; min-width: 16px; "
            "max-width: 16px; min-height: 16px; max-height: 16px; }"));
    }
}

// ── appendI2cLogEntry ─────────────────────────────────────────────────────────

void Hl2IoBoardTab::appendI2cLogEntry(const QString& text)
{
    m_i2cLog->addItem(text);
    // Trim to max lines
    while (m_i2cLog->count() > kMaxLogLines) {
        delete m_i2cLog->takeItem(0);
    }
    m_i2cLog->scrollToBottom();
}

// ── updateBwDisplay ───────────────────────────────────────────────────────────

void Hl2IoBoardTab::updateBwDisplay()
{
    // EP6 ingress: scale to 100% at 10 Mbps
    // 192k×24bit×2ch = ~9.2 Mbps; round to 10 Mbps as display ceiling.
    static constexpr double kMaxBps = 10.0e6;

    const double ep6Bps = m_bwMonitor->ep6IngressBytesPerSec();
    const double ep2Bps = m_bwMonitor->ep2EgressBytesPerSec();

    const int ep6Pct = qBound(0, static_cast<int>(ep6Bps / kMaxBps * 100.0), 100);
    const int ep2Pct = qBound(0, static_cast<int>(ep2Bps / kMaxBps * 100.0), 100);

    m_ep6Bar->setValue(ep6Pct);
    m_ep2Bar->setValue(ep2Pct);

    m_ep6RateLabel->setText(
        QStringLiteral("%1 Mbps").arg(ep6Bps / 1.0e6, 0, 'f', 2));
    m_ep2RateLabel->setText(
        QStringLiteral("%1 Mbps").arg(ep2Bps / 1.0e6, 0, 'f', 2));

    m_throttleEventLabel->setText(
        QString::number(m_bwMonitor->throttleEventCount()));
}

// ── decodeRegister ────────────────────────────────────────────────────────────

QString Hl2IoBoardTab::decodeRegister(IoBoardHl2::Register reg, quint8 value) const
{
    // Human-readable decoding for the principal registers.
    // NereusSDR diagnostic surface — no upstream UI equivalent.
    switch (reg) {
    case IoBoardHl2::Register::HardwareVersion:
        if (value == IoBoardHl2::kHardwareVersion1) {
            return QStringLiteral("Version 1 (0xF1)");
        }
        return value == 0 ? QStringLiteral("Not read") : QStringLiteral("Unknown");

    case IoBoardHl2::Register::REG_CONTROL:
        return QStringLiteral("ctrl=0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();

    case IoBoardHl2::Register::REG_OP_MODE:
        return QStringLiteral("mode=%1").arg(value);

    case IoBoardHl2::Register::REG_ANTENNA:
        return QStringLiteral("ant=%1").arg(value);

    case IoBoardHl2::Register::REG_RF_INPUTS:
        return QStringLiteral("rf_in=0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();

    case IoBoardHl2::Register::REG_INPUT_PINS:
        return QStringLiteral("pins=0b%1").arg(value, 8, 2, QLatin1Char('0'));

    case IoBoardHl2::Register::REG_ANTENNA_TUNER:
        return value ? QStringLiteral("enabled") : QStringLiteral("disabled");

    case IoBoardHl2::Register::REG_FAULT:
        return value == 0 ? QStringLiteral("no fault")
                          : QStringLiteral("FAULT 0x%1")
                                .arg(value, 2, 16, QLatin1Char('0')).toUpper();

    default:
        return QStringLiteral("—");
    }
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void Hl2IoBoardTab::onDetectedChanged(bool detected)
{
    updateStatusBar(detected);
    if (detected) {
        refreshAllRegisters();
        appendI2cLogEntry(
            QStringLiteral("[%1] Board detected — hardware version 0x%2")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz")))
                .arg(m_ioBoard->hardwareVersion(), 2, 16, QLatin1Char('0')).toUpper());
    }
}

void Hl2IoBoardTab::onRegisterChanged(IoBoardHl2::Register reg, quint8 value)
{
    // Update table row for the changed register
    for (int i = 0; i < kRegRowCount; ++i) {
        if (kRegRows[i].reg == reg) {
            if (auto* item = m_registerTable->item(i, 2)) {
                item->setText(
                    QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper());
            }
            if (auto* item = m_registerTable->item(i, 3)) {
                item->setText(decodeRegister(reg, value));
            }
            break;
        }
    }

    // Update quick-view labels for the four registers shown there
    switch (reg) {
    case IoBoardHl2::Register::REG_OP_MODE:
        m_opModeValue->setText(
            QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper());
        break;
    case IoBoardHl2::Register::REG_ANTENNA:
        m_antennaValue->setText(
            QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper());
        break;
    case IoBoardHl2::Register::REG_RF_INPUTS:
        m_rfInputsValue->setText(
            QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper());
        break;
    case IoBoardHl2::Register::REG_ANTENNA_TUNER:
        m_antTunerValue->setText(
            QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper());
        break;
    default:
        break;
    }
}

void Hl2IoBoardTab::onStepAdvanced(int newStep)
{
    highlightStep(newStep);
}

void Hl2IoBoardTab::onI2cQueueChanged()
{
    const int depth = m_ioBoard->i2cQueueDepth();
    const QString title =
        QStringLiteral("I2C transaction log  (queue depth: %1 / %2)")
            .arg(depth).arg(IoBoardHl2::kMaxI2cQueue);

    // Update the group box title to show current depth
    if (auto* gb = qobject_cast<QGroupBox*>(m_i2cLog->parentWidget())) {
        gb->setTitle(title);
    }

    // Log the queue change
    appendI2cLogEntry(
        QStringLiteral("[%1] I2C queue depth: %2 / %3")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz")))
            .arg(depth)
            .arg(IoBoardHl2::kMaxI2cQueue));
}

void Hl2IoBoardTab::onThrottledChanged(bool throttled)
{
    if (throttled) {
        m_throttleStatusLabel->setText(tr("● throttled"));
        m_throttleStatusLabel->setStyleSheet(
            QStringLiteral("font-size: 10px; color: #ff4444;"));
    } else {
        m_throttleStatusLabel->setText(tr("○ not throttled"));
        m_throttleStatusLabel->setStyleSheet(
            QStringLiteral("font-size: 10px; color: #22cc44;"));
    }
    m_throttleEventLabel->setText(
        QString::number(m_bwMonitor->throttleEventCount()));

    appendI2cLogEntry(
        QStringLiteral("[%1] LAN PHY throttle: %2")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz")))
            .arg(throttled ? QStringLiteral("ASSERTED") : QStringLiteral("cleared")));
}

void Hl2IoBoardTab::onBwTimerTick()
{
    updateBwDisplay();
}

// From mi0bot setup.cs:20234-20238 — chkHERCULES_CheckedChanged [@c26a8a4]
// N2ADR Filter board enable persisted to AppSettings.
void Hl2IoBoardTab::onN2adrToggled(bool checked)
{
    AppSettings::instance().setValue(
        QStringLiteral("hl2/n2adrFilter"),
        checked ? QStringLiteral("True") : QStringLiteral("False"));
}

void Hl2IoBoardTab::onProbeClicked()
{
    // Mark a probe attempt in the log.
    // Actual probing is driven by P1CodecHl2 via the I2C queue (Task 2).
    // This button provides a user-visible timestamp so users can correlate
    // with the state machine activity.
    appendI2cLogEntry(
        QStringLiteral("[%1] *** User-initiated probe ***")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"))));
    m_lastProbeLabel->setText(
        QStringLiteral("Last probe: %1")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"))));
}

void Hl2IoBoardTab::onResetClicked()
{
    // Reset local UI state and I2C log.
    m_i2cLog->clear();
    m_ioBoard->setDetected(false);
    m_ioBoard->clearI2cQueue();
    updateStatusBar(false);
    refreshAllRegisters();
    highlightStep(0);
    appendI2cLogEntry(
        QStringLiteral("[%1] *** I/O board reset (UI) ***")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"))));
}

// ── HardwarePage compatibility stubs ─────────────────────────────────────────
// This tab is self-populating via RadioModel signals; populate() and
// restoreSettings() are no-ops.  The generic wire() lambda in HardwarePage
// connects settingChanged() — emitted by onN2adrToggled() via AppSettings
// directly (the wire() path is therefore redundant but harmless).

void Hl2IoBoardTab::populate(const RadioInfo& /*info*/, const BoardCapabilities& /*caps*/)
{
    // No-op: board detection and register state come from IoBoardHl2 signals.
    // Auto-hide is already set in the constructor based on hardwareProfile().
}

void Hl2IoBoardTab::restoreSettings(const QMap<QString, QVariant>& settings)
{
    // Restore N2ADR filter checkbox from persisted settings if present.
    const auto it = settings.constFind(QStringLiteral("n2adrFilter"));
    if (it != settings.constEnd()) {
        const bool checked = (it.value().toString() == QStringLiteral("True"));
        QSignalBlocker blocker(m_n2adrFilter);
        m_n2adrFilter->setChecked(checked);
    }
}

} // namespace NereusSDR
