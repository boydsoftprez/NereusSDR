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

#include "core/BoardCapabilities.h"
#include "core/HermesLiteBandwidthMonitor.h"
#include "core/HpsdrModel.h"
#include "core/OcMatrix.h"
#include "core/P1RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/accessories/N2adrPreset.h"
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
    // Raw wire-byte taps — log every composed (OUT) and every parsed (IN) frame
    // so a wire-format mismatch is visible side-by-side.
    connect(m_ioBoard, &IoBoardHl2::i2cTxComposed,
            this, [this](quint8 c0, quint8 c1, quint8 c2, quint8 c3, quint8 c4) {
                appendI2cLogEntry(
                    QStringLiteral("[%1] OUT C0=%2 C1=%3 C2=%4 C3=%5 C4=%6")
                        .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz")))
                        .arg(c0, 2, 16, QLatin1Char('0'))
                        .arg(c1, 2, 16, QLatin1Char('0'))
                        .arg(c2, 2, 16, QLatin1Char('0'))
                        .arg(c3, 2, 16, QLatin1Char('0'))
                        .arg(c4, 2, 16, QLatin1Char('0'))
                        .toUpper());
            });
    connect(m_ioBoard, &IoBoardHl2::i2cReadResponseReceived,
            this, [this](quint8 retAddr, quint8 retSubAddr,
                         quint8 b0, quint8 b1, quint8 b2, quint8 b3) {
                appendI2cLogEntry(
                    QStringLiteral("[%1]  IN ret=%2 sub=%3 C1=%4 C2=%5 C3=%6 C4=%7")
                        .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz")))
                        .arg(retAddr,    2, 16, QLatin1Char('0'))
                        .arg(retSubAddr, 2, 16, QLatin1Char('0'))
                        .arg(b0, 2, 16, QLatin1Char('0'))
                        .arg(b1, 2, 16, QLatin1Char('0'))
                        .arg(b2, 2, 16, QLatin1Char('0'))
                        .arg(b3, 2, 16, QLatin1Char('0'))
                        .toUpper());
            });
    connect(m_ioBoard, &IoBoardHl2::currentOcByteChanged,
            this, &Hl2IoBoardTab::updateOcIndicator);
    connect(m_bwMonitor, &HermesLiteBandwidthMonitor::throttledChanged,
            this, &Hl2IoBoardTab::onThrottledChanged);

    // 250 ms timer for live bandwidth rate readouts
    m_bwTimer = new QTimer(this);
    m_bwTimer->setInterval(250);
    connect(m_bwTimer, &QTimer::timeout, this, &Hl2IoBoardTab::onBwTimerTick);
    m_bwTimer->start();

    // Phase 3P-H Task 5c — 40 ms register-table poller.
    // Matches the group-box title "Register state  (polled @ 40 ms)" and
    // supplements push-based IoBoardHl2::registerChanged so a coalesced
    // write still surfaces within one frame.
    m_regPollTimer = new QTimer(this);
    m_regPollTimer->setInterval(kRegisterPollMs);
    connect(m_regPollTimer, &QTimer::timeout,
            this, &Hl2IoBoardTab::onRegisterPollTick);
    m_regPollTimer->start();

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

    m_statusLabel = new QLabel(
        tr("mi0bot custom I/O board (0x41): Not detected"), m_statusFrame);
    m_statusLabel->setToolTip(tr(
        "Detection of mi0bot's custom HL2 daughterboard at I2C address 0x41.\n"
        "If you only have the N2ADR Filter and/or smallio companion boards,\n"
        "this will (correctly) say Not detected — those boards don't speak\n"
        "this I2C protocol.  The N2ADR is driven by OC pins (see Live OC pin\n"
        "state below + Setup → Hardware → OC Outputs → HF for the matrix)."));
    QFont bold = m_statusLabel->font();
    bold.setBold(true);
    m_statusLabel->setFont(bold);
    row->addWidget(m_statusLabel);

    // Live OC byte indicator — band, hex, MOX, and 7 pin LEDs.
    row->addSpacing(16);
    row->addWidget(new QLabel(tr("OC bank 0:"), m_statusFrame));
    m_ocBandLabel = new QLabel(QStringLiteral("—"), m_statusFrame);
    m_ocBandLabel->setStyleSheet(QStringLiteral("color: #aaa; font-size: 10px;"));
    row->addWidget(m_ocBandLabel);
    m_ocByteLabel = new QLabel(QStringLiteral("0x00"), m_statusFrame);
    m_ocByteLabel->setStyleSheet(QStringLiteral(
        "color: #ddd; font-family: monospace; font-weight: bold;"));
    row->addWidget(m_ocByteLabel);
    m_ocMoxLabel = new QLabel(QStringLiteral("RX"), m_statusFrame);
    m_ocMoxLabel->setStyleSheet(QStringLiteral(
        "color: #6699ff; font-weight: bold;"));
    row->addWidget(m_ocMoxLabel);
    for (int i = 0; i < 7; ++i) {
        auto* led = new QFrame(m_statusFrame);
        led->setFixedSize(10, 10);
        led->setFrameShape(QFrame::StyledPanel);
        led->setStyleSheet(QStringLiteral(
            "QFrame { background: #222; border: 1px solid #555; border-radius: 5px; }"));
        led->setToolTip(tr("OC pin %1").arg(i + 1));
        m_ocPinLeds[i] = led;
        row->addWidget(led);
    }

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
        m_statusLabel->setText(tr("mi0bot custom I/O board (0x41): Active"));
        m_statusFrame->setStyleSheet(QStringLiteral(
            "QFrame { background: #1a2f1a; border: 1px solid #2a5a2a; border-radius: 4px; }"));
        m_lastProbeLabel->setText(
            QStringLiteral("Last probe: %1")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"))));
    } else {
        m_statusLabel->setText(tr("mi0bot custom I/O board (0x41): Not detected"));
        m_statusFrame->setStyleSheet(QStringLiteral(
            "QFrame { background: #2a2a2a; border: 1px solid #444; border-radius: 4px; }"));
    }
}

// ── updateOcIndicator ─────────────────────────────────────────────────────────

void Hl2IoBoardTab::updateOcIndicator(quint8 ocByte, int bandIdx, bool mox)
{
    static constexpr const char* kBandLabels[] = {
        "160m", "80m", "60m", "40m", "30m", "20m", "17m",
        "15m", "12m", "10m", "6m", "GEN", "WWV", "XVTR"
    };
    if (m_ocBandLabel) {
        const QString name = (bandIdx >= 0 && bandIdx < int(sizeof(kBandLabels)/sizeof(*kBandLabels)))
                             ? QString::fromLatin1(kBandLabels[bandIdx])
                             : QString::number(bandIdx);
        m_ocBandLabel->setText(QStringLiteral("band=%1").arg(name));
    }
    if (m_ocByteLabel) {
        m_ocByteLabel->setText(
            QStringLiteral("0x%1").arg(ocByte, 2, 16, QLatin1Char('0')).toUpper());
    }
    if (m_ocMoxLabel) {
        m_ocMoxLabel->setText(mox ? QStringLiteral("TX") : QStringLiteral("RX"));
        m_ocMoxLabel->setStyleSheet(
            mox ? QStringLiteral("color: #ff6655; font-weight: bold;")
                : QStringLiteral("color: #6699ff; font-weight: bold;"));
    }
    for (int i = 0; i < 7; ++i) {
        if (!m_ocPinLeds[i]) { continue; }
        const bool on = (ocByte & (1u << i)) != 0;
        m_ocPinLeds[i]->setStyleSheet(
            on ? QStringLiteral("QFrame { background: #44ff44; border: 1px solid #88ff88; border-radius: 5px; }")
               : QStringLiteral("QFrame { background: #222; border: 1px solid #555; border-radius: 5px; }"));
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

// Phase 3P-H Task 5c — 40 ms register-table poll.
// Reads every principal register via IoBoardHl2::registerValue(reg) and
// writes the hex + decoded strings into the table cells. Cheap enough
// to run every 40 ms (8 register reads + 8 cell updates per tick).
void Hl2IoBoardTab::onRegisterPollTick()
{
    refreshAllRegisters();
}

// N2ADR Filter board toggle — sole control surface for N2ADR on HL2.
//
// Source: mi0bot setup.cs:14311-14424 chkHERCULES_CheckedChanged [@c26a8a4]
// (HERMESLITE branch lines 14324-14368).  mi0bot's chkHERCULES is BOTH the
// enable AND the preset trigger:
//   case true:  clear all chkPenOC* checkboxes, then set the per-band cells
//               that match the N2ADR filter wiring (10 ham + 13 SWL).
//   case false: clear all chkPenOC* checkboxes.
//
// NereusSDR mirrors that exactly: this checkbox is the single source of
// truth for N2ADR.  Toggling on populates the shared OcMatrix with the
// per-band pattern; toggling off wipes it.  No separate "apply preset"
// step.  The per-band write table lives in N2adrPreset so that this
// handler and RadioModel's app-launch reconcile share one source of
// truth (Phase 3L extraction — was duplicated until 2026-04-30).
//
// hermes-filter-debug Bug 2: emit settingChanged() instead of writing
// AppSettings directly.  HardwarePage's wire() lambda routes the signal
// through onTabSettingChanged() → setHardwareValue(mac, ...) so the value
// lands at hardware/<mac>/hl2IoBoard/n2adrFilter.  Mirrors Thetis's
// effective per-radio semantic (each Thetis DB file ≈ one radio,
// database.cs:64 [@c26a8a4]) within NereusSDR's multi-radio settings model.
void Hl2IoBoardTab::onN2adrToggled(bool checked)
{
    emit settingChanged(QStringLiteral("n2adrFilter"),
                        checked ? QStringLiteral("True")
                                : QStringLiteral("False"));
    applyN2adrMatrix(checked);
}

// Matrix-only side of the N2ADR toggle.  Extracted from onN2adrToggled so
// restoreSettings() can reapply persisted state without echoing a redundant
// write back through the wire() persistence path.
void Hl2IoBoardTab::applyN2adrMatrix(bool checked)
{
    if (!m_model) { return; }
    OcMatrix& oc = m_model->ocMatrixMutable();
    applyN2adrPreset(oc, checked);
    // Persist whichever state we just composed (cleared or populated).
    oc.save();
}

void Hl2IoBoardTab::onProbeClicked()
{
    // Trigger a real probe — enqueues 3 I2C reads (HW version + FW major +
    // FW minor) on the IoBoardHl2 queue.  Wire encoder drains them on the
    // next ep2 frames; responses populate IoBoardHl2 register state and
    // setDetected.  Noop on non-HL2 boards or before connect.
    bool issued = false;
    if (auto* p1 = qobject_cast<P1RadioConnection*>(m_model->connection())) {
        p1->requestIoBoardProbe();
        issued = true;
    }

    appendI2cLogEntry(
        QStringLiteral("[%1] *** User-initiated probe %2 ***")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz")))
            .arg(issued ? QStringLiteral("(3 reads enqueued)")
                        : QStringLiteral("(no P1 connection — skipped)")));
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

// ── HardwarePage compatibility hooks ─────────────────────────────────────────
// populate() is a no-op here — board detection and register state arrive via
// RadioModel/IoBoardHl2 signals.  restoreSettings() reapplies the persisted
// N2ADR filter state from the per-MAC store HardwarePage feeds in.
// settingChanged() emissions originate from onN2adrToggled() and are routed
// through HardwarePage::wire() → onTabSettingChanged() →
// setHardwareValue(mac, ...).

void Hl2IoBoardTab::populate(const RadioInfo& /*info*/, const BoardCapabilities& /*caps*/)
{
    // No-op: board detection and register state come from IoBoardHl2 signals.
    // Auto-hide is already set in the constructor based on hardwareProfile().
}

void Hl2IoBoardTab::restoreSettings(const QMap<QString, QVariant>& settings)
{
    // Restore N2ADR filter checkbox + RECONCILE the OcMatrix to match.
    //
    // Without the reconcile call, an OcMatrix populated from a prior session
    // can survive a wrong-key persistence bug — leaving the matrix populated
    // even while the checkbox shows unchecked.  The MCP23008 on the HL2's
    // smallio companion is then driven on every band change and the user
    // hears relay clicks they can't disable.
    //
    // hermes-filter-debug Bug 2: only reconcile the OcMatrix when the
    // persisted key is ACTUALLY present.  A missing key means "this radio
    // has never had N2ADR explicitly set" — leave the matrix alone rather
    // than wiping back to False (which previously turned every Setup-tab
    // open on a fresh launch into a destructive reset).  Apply via
    // applyN2adrMatrix (matrix-only) so we don't echo settingChanged back
    // through HardwarePage::onTabSettingChanged immediately after reading.
    //
    // Codex P2 (PR #160 review): always reset the CHECKBOX state too —
    // this Hl2IoBoardTab instance is reused across currentRadioChanged
    // events, so leaving m_n2adrFilter at its previous value would show
    // stale state from the previously selected radio (e.g. checked from
    // HL2-A while connected to HL2-B that never had N2ADR set).  We
    // touch only the checkbox here, not the matrix — connect-time
    // (RadioModel::connectToRadio) has already reconciled the matrix to
    // the per-MAC value before this hook runs.
    //
    // Issue #174: default to True when the key is absent — strict
    // mi0bot port from setup.designer.cs:17466-17467 [v2.10.3.13-beta2]
    // (chkHERCULES.Checked = true).  Must match the connect-time default
    // in RadioModel.cpp; otherwise the checkbox would render unchecked
    // while the OcMatrix is already populated with the N2ADR preset.
    const auto it = settings.constFind(QStringLiteral("n2adrFilter"));
    const bool keyPresent = (it != settings.constEnd());
    const bool checked = keyPresent
                       ? it.value().toString() == QStringLiteral("True")
                       : true;
    {
        QSignalBlocker blocker(m_n2adrFilter);
        m_n2adrFilter->setChecked(checked);
    }
    if (!keyPresent) {
        return;  // matrix already reconciled at connect-time; do not wipe.
    }
    applyN2adrMatrix(checked);
}

// ── Phase 3P-H Task 5c test seams ────────────────────────────────────────────

int Hl2IoBoardTab::registerPollIntervalMsForTest() const
{
    return m_regPollTimer ? m_regPollTimer->interval() : -1;
}

void Hl2IoBoardTab::pollRegistersNowForTest()
{
    refreshAllRegisters();
}

QString Hl2IoBoardTab::registerCellTextForTest(int row) const
{
    if (!m_registerTable || row < 0 || row >= kRegRowCount) { return {}; }
    auto* item = m_registerTable->item(row, 2);
    return item ? item->text() : QString();
}

int Hl2IoBoardTab::bandwidthPollIntervalMsForTest() const
{
    return m_bwTimer ? m_bwTimer->interval() : -1;
}

QString Hl2IoBoardTab::ep6RateTextForTest() const
{
    return m_ep6RateLabel ? m_ep6RateLabel->text() : QString();
}

QString Hl2IoBoardTab::ep2RateTextForTest() const
{
    return m_ep2RateLabel ? m_ep2RateLabel->text() : QString();
}

QString Hl2IoBoardTab::throttleStatusTextForTest() const
{
    return m_throttleStatusLabel ? m_throttleStatusLabel->text() : QString();
}

QString Hl2IoBoardTab::throttleEventTextForTest() const
{
    return m_throttleEventLabel ? m_throttleEventLabel->text() : QString();
}

void Hl2IoBoardTab::pollBandwidthNowForTest()
{
    updateBwDisplay();
}

// PR #160 Codex P2 test seams (hermes-filter-debug): expose checkbox state
// so tests can verify restoreSettings resets the box when the key is absent.

bool Hl2IoBoardTab::n2adrCheckboxStateForTest() const
{
    return m_n2adrFilter && m_n2adrFilter->isChecked();
}

void Hl2IoBoardTab::setN2adrCheckboxStateForTest(bool checked)
{
    if (m_n2adrFilter) {
        QSignalBlocker blocker(m_n2adrFilter);
        m_n2adrFilter->setChecked(checked);
    }
}

} // namespace NereusSDR
