// =================================================================
// src/gui/setup/hardware/Hl2OptionsTab.cpp  (NereusSDR)
// =================================================================
//
// Ported from mi0bot-Thetis source:
//   Project Files/Source/Console/setup.designer.cs:tpHL2Options
//   (groupBoxHL2RXOptions + groupBoxI2CControl + grpIOPinState)
//   (mi0bot v2.10.3.13-beta2 / @c26a8a4)
//
// See Hl2OptionsTab.h for the full design + scope rationale.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-30 — New for Phase 3L HL2 Filter visibility brainstorm.
//                Phase 3L commit #9.  Three group boxes:
//                Hermes Lite Options + I2C Control + I/O Pin State.
//                J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Anthropic Claude Code.
// =================================================================
//
//=================================================================
// setup.cs (mi0bot fork)
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
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves his       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "Hl2OptionsTab.h"

#include "core/BoardCapabilities.h"
#include "core/Hl2OptionsModel.h"
#include "core/IoBoardHl2.h"
#include "core/RadioDiscovery.h"
#include "gui/widgets/OcLedStripWidget.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

namespace NereusSDR {

namespace {
Q_LOGGING_CATEGORY(lcHl2Options, "nereus.hl2.options")

// Small helper for hex-displayed integer spinboxes.
QSpinBox* makeHexSpin(QWidget* parent, int min, int max, int initial, int width = 56)
{
    auto* sp = new QSpinBox(parent);
    sp->setDisplayIntegerBase(16);
    sp->setPrefix(QStringLiteral("0x"));
    sp->setRange(min, max);
    sp->setValue(initial);
    sp->setFixedWidth(width);
    return sp;
}
} // namespace

Hl2OptionsTab::Hl2OptionsTab(RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_options(model ? &model->hl2OptionsMutable() : nullptr)
    , m_ioBoard(model ? &model->ioBoardMutable() : nullptr)
{
    auto* outer = new QVBoxLayout(this);
    outer->setSpacing(8);
    outer->setContentsMargins(8, 8, 8, 8);

    // ── Top row: Hermes Lite Options (left) + I/O Pin State (right) ───────
    // From mi0bot setup.designer.cs:11074-11084 tpHL2Options layout
    // [v2.10.3.13-beta2] — groupBoxHL2RXOptions @ (12, 15), grpIOPinState @
    // (12, 229).  We render side-by-side instead of stacked because Qt's
    // tab body is narrower in our SetupDialog than mi0bot's WinForms tab.
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(8);

    auto* hermesGroup = new QGroupBox(tr("Hermes Lite Options"), this);
    buildHermesLiteOptions(hermesGroup);
    topRow->addWidget(hermesGroup, /*stretch=*/1);

    auto* pinStateGroup = new QGroupBox(tr("I/O Board Pin States"), this);
    buildIoPinState(pinStateGroup);
    topRow->addWidget(pinStateGroup, /*stretch=*/1);

    outer->addLayout(topRow);

    // ── Bottom row: I2C Control (full width) ──────────────────────────────
    // From mi0bot setup.designer.cs:11315-11645 groupBoxI2CControl
    // @ (419, 252).
    auto* i2cGroup = new QGroupBox(tr("I2C Control"), this);
    buildI2cControl(i2cGroup);
    outer->addWidget(i2cGroup);

    outer->addStretch();

    // ── Wire model → UI sync ──────────────────────────────────────────────
    if (m_options) {
        connect(m_options, &Hl2OptionsModel::changed,
                this, &Hl2OptionsTab::syncFromModel);
        syncFromModel();
    }

    // ── Wire I/O Board live OC byte → output strip indicator ──────────────
    // The output strip on this tab mirrors the same OC bank-0 byte the
    // status-bar strip on Hl2IoBoardTab shows.  No new signal — reuse
    // IoBoardHl2::currentOcByteChanged (added during Phase 3L carry-forward).
    if (m_ioBoard) {
        connect(m_ioBoard, &IoBoardHl2::currentOcByteChanged,
                this, &Hl2OptionsTab::onIoBoardOcByteChanged);
    }
}

Hl2OptionsTab::~Hl2OptionsTab() = default;

// ── populate / restoreSettings — HardwarePage contract ──────────────────────

void Hl2OptionsTab::populate(const RadioInfo& info, const BoardCapabilities& caps)
{
    // Visibility is gated by HardwarePage on caps.hasIoBoardHl2; this is a
    // belt-and-braces hide for non-HL2 just in case the page is ever
    // attached without the gate.
    setVisible(caps.hasIoBoardHl2);

    if (m_options) {
        m_options->setMacAddress(info.macAddress);
        m_options->load();   // fires changed() → syncFromModel
    }
}

void Hl2OptionsTab::restoreSettings(const QMap<QString, QVariant>& /*settings*/)
{
    // State lives in Hl2OptionsModel under hardware/<mac>/hl2/...; populate()
    // already called load(), so nothing to do here.  This stub satisfies
    // the HardwarePage API contract.
}

// ── buildHermesLiteOptions ──────────────────────────────────────────────────

void Hl2OptionsTab::buildHermesLiteOptions(QWidget* parent)
{
    auto* grid = new QGridLayout(parent);
    grid->setSpacing(6);
    grid->setContentsMargins(8, 16, 8, 8);
    int row = 0;

    // From mi0bot setup.designer.cs:11267-11290 udTxBufferLat (TX buffer
    // latency, ms) [v2.10.3.13-beta2] — range 0..70, default 20.
    grid->addWidget(new QLabel(tr("TX buffer latency:"), parent), row, 0);
    m_udTxLatency = new QSpinBox(parent);
    m_udTxLatency->setRange(Hl2OptionsModel::kTxLatencyMinMs,
                            Hl2OptionsModel::kTxLatencyMaxMs);
    m_udTxLatency->setSuffix(tr(" ms"));
    grid->addWidget(m_udTxLatency, row, 1);
    ++row;

    // From mi0bot setup.designer.cs:11235-11258 udPTTHang (PTT hang,
    // ms) [v2.10.3.13-beta2] — range 0..30, default 12.
    grid->addWidget(new QLabel(tr("PTT hang:"), parent), row, 0);
    m_udPttHang = new QSpinBox(parent);
    m_udPttHang->setRange(Hl2OptionsModel::kPttHangMinMs,
                          Hl2OptionsModel::kPttHangMaxMs);
    m_udPttHang->setSuffix(tr(" ms"));
    grid->addWidget(m_udPttHang, row, 1);
    ++row;

    // From mi0bot setup.designer.cs:11166-11176 chkCl2Enable +
    // :11142-11164 udCl2Freq [v2.10.3.13-beta2] — range 1..200 MHz,
    // default 116.
    m_chkCl2Enable = new QCheckBox(tr("Enable CL2"), parent);
    grid->addWidget(m_chkCl2Enable, row, 0);
    m_udCl2Freq = new QSpinBox(parent);
    m_udCl2Freq->setRange(Hl2OptionsModel::kCl2FreqMinMHz,
                          Hl2OptionsModel::kCl2FreqMaxMHz);
    m_udCl2Freq->setSuffix(tr(" MHz"));
    grid->addWidget(m_udCl2Freq, row, 1);
    ++row;

    // From mi0bot setup.designer.cs:11178+ chkExt10MHz [v2.10.3.13-beta2]
    m_chkExt10MHz = new QCheckBox(tr("External 10 MHz reference"), parent);
    grid->addWidget(m_chkExt10MHz, row, 0, 1, 2);
    ++row;

    // From mi0bot setup.designer.cs:11258 chkDisconnectReset
    m_chkDisconnectReset = new QCheckBox(tr("Reset on Ethernet disconnect"), parent);
    grid->addWidget(m_chkDisconnectReset, row, 0, 1, 2);
    ++row;

    // From mi0bot setup.designer.cs:11293+ chkHL2PsSync
    m_chkPsSync = new QCheckBox(tr("PureSignal sync"), parent);
    grid->addWidget(m_chkPsSync, row, 0, 1, 2);
    ++row;

    // From mi0bot setup.designer.cs:11305-11313 chkHL2BandVolts
    m_chkBandVolts = new QCheckBox(tr("Band Volts (PWM out 0–3.3 V)"), parent);
    grid->addWidget(m_chkBandVolts, row, 0, 1, 2);
    ++row;

    // From mi0bot setup.designer.cs:11343 chkSwapAudioChannels
    m_chkSwapAudio = new QCheckBox(tr("Swap audio channels"), parent);
    grid->addWidget(m_chkSwapAudio, row, 0, 1, 2);
    ++row;

    grid->setRowStretch(row, 1);

    // ── UI → model wiring (with re-entrancy guard) ────────────────────────
    auto bindBool = [this](QCheckBox* cb, auto setter) {
        connect(cb, &QCheckBox::toggled, this,
                [this, setter](bool on) {
                    if (m_syncing) { return; }
                    if (m_options) {
                        (m_options->*setter)(on);
                        qCWarning(lcHl2Options).nospace()
                            << "wire emission TBD Phase 3L follow-up "
                               "(toggle " << (on ? "ON" : "OFF") << ")";
                    }
                });
    };
    auto bindInt = [this](QSpinBox* sp, auto setter) {
        connect(sp, qOverload<int>(&QSpinBox::valueChanged), this,
                [this, setter](int v) {
                    if (m_syncing) { return; }
                    if (m_options) {
                        (m_options->*setter)(v);
                        qCWarning(lcHl2Options).nospace()
                            << "wire emission TBD Phase 3L follow-up (value=" << v << ")";
                    }
                });
    };

    bindBool(m_chkSwapAudio,        &Hl2OptionsModel::setSwapAudioChannels);
    bindBool(m_chkCl2Enable,        &Hl2OptionsModel::setCl2Enabled);
    bindInt (m_udCl2Freq,           &Hl2OptionsModel::setCl2FreqMHz);
    bindBool(m_chkExt10MHz,         &Hl2OptionsModel::setExt10MHz);
    bindBool(m_chkDisconnectReset,  &Hl2OptionsModel::setDisconnectReset);
    bindInt (m_udPttHang,           &Hl2OptionsModel::setPttHangMs);
    bindInt (m_udTxLatency,         &Hl2OptionsModel::setTxLatencyMs);
    bindBool(m_chkPsSync,           &Hl2OptionsModel::setPsSync);
    bindBool(m_chkBandVolts,        &Hl2OptionsModel::setBandVolts);
}

// ── buildI2cControl ─────────────────────────────────────────────────────────

void Hl2OptionsTab::buildI2cControl(QWidget* parent)
{
    auto* grid = new QGridLayout(parent);
    grid->setSpacing(6);
    grid->setContentsMargins(8, 16, 8, 8);
    int row = 0;

    // chkI2CEnable (mi0bot setup.designer.cs:11335 — group enabled when checked).
    m_chkI2cEnable = new QCheckBox(tr("Enable I2C R/W tool"), parent);
    m_chkI2cEnable->setToolTip(tr(
        "Manual I2C read/write tool.  Disabled by default to avoid "
        "accidental writes; enable to read or write registers on the "
        "HL2 daughterboard at I2C address 0x1D."));
    grid->addWidget(m_chkI2cEnable, row, 0, 1, 4);
    ++row;

    // Bus radio — bus 0 deferred per design §4 (no NereusSDR I2cTxn path
    // for bus 0 today), so render as disabled with explanatory tooltip.
    grid->addWidget(new QLabel(tr("Bus:"), parent), row, 0);
    auto* bus0 = new QCheckBox(tr("0 (deferred)"), parent);
    bus0->setEnabled(false);
    bus0->setToolTip(tr(
        "Bus 0 surface deferred to a Phase 3L follow-up — NereusSDR's "
        "I2cTxn pipeline currently emits bus 1 only.  See "
        "docs/architecture/phase3l-hl2-visibility-design.md §4."));
    grid->addWidget(bus0, row, 1);
    auto* bus1 = new QCheckBox(tr("1 (HL2 daughterboard)"), parent);
    bus1->setChecked(true);
    bus1->setEnabled(false); // single supported value
    grid->addWidget(bus1, row, 2, 1, 2);
    ++row;

    // I2C address (default 0x1D — IoBoardHl2.cs:139 [@c26a8a4]).
    grid->addWidget(new QLabel(tr("Address:"), parent), row, 0);
    m_udI2cAddress = makeHexSpin(parent, 0x00, 0x7F,
                                  IoBoardHl2::kI2cAddrGeneral);
    m_udI2cAddress->setToolTip(tr(
        "7-bit I2C address.  0x1D = HL2 general registers; 0x41 = HL2 "
        "hardware version register."));
    grid->addWidget(m_udI2cAddress, row, 1);

    grid->addWidget(new QLabel(tr("Reg/Ctrl:"), parent), row, 2);
    m_udI2cRegister = makeHexSpin(parent, 0x00, 0xFF, 0x00);
    m_udI2cRegister->setToolTip(tr("Register / sub-address byte (C3 wire byte)."));
    grid->addWidget(m_udI2cRegister, row, 3);
    ++row;

    grid->addWidget(new QLabel(tr("Write data:"), parent), row, 0);
    m_udI2cWriteData = makeHexSpin(parent, 0x00, 0xFF, 0x00);
    m_udI2cWriteData->setToolTip(tr("Data byte to send (C4 wire byte) on Write."));
    grid->addWidget(m_udI2cWriteData, row, 1);

    m_chkI2cWriteEnable = new QCheckBox(tr("Write enable"), parent);
    m_chkI2cWriteEnable->setToolTip(tr(
        "Belt-and-braces guard: must be checked before the Write button "
        "actually enqueues an I2C write transaction."));
    grid->addWidget(m_chkI2cWriteEnable, row, 2, 1, 2);
    ++row;

    // Read response display — 4 hex bytes.
    grid->addWidget(new QLabel(tr("Read response:"), parent), row, 0);
    auto* respRow = new QHBoxLayout();
    auto makeByteLbl = [parent]() {
        auto* lbl = new QLabel(QStringLiteral("--"), parent);
        lbl->setFixedWidth(28);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { background: white; color: black; "
            "font-family: monospace; border: 1px solid #555; padding: 2px; }"));
        return lbl;
    };
    m_byte0Label = makeByteLbl();
    m_byte1Label = makeByteLbl();
    m_byte2Label = makeByteLbl();
    m_byte3Label = makeByteLbl();
    respRow->addWidget(m_byte0Label);
    respRow->addWidget(m_byte1Label);
    respRow->addWidget(m_byte2Label);
    respRow->addWidget(m_byte3Label);
    respRow->addStretch();
    auto* respWrap = new QWidget(parent);
    respWrap->setLayout(respRow);
    grid->addWidget(respWrap, row, 1, 1, 3);
    ++row;

    // Read / Write buttons.
    m_btnRead  = new QPushButton(tr("Read"),  parent);
    m_btnWrite = new QPushButton(tr("Write"), parent);
    grid->addWidget(m_btnRead,  row, 1);
    grid->addWidget(m_btnWrite, row, 2);
    ++row;

    grid->setRowStretch(row, 1);

    // ── Wire ──────────────────────────────────────────────────────────────
    connect(m_chkI2cEnable, &QCheckBox::toggled,
            this, &Hl2OptionsTab::onI2cEnableToggled);
    connect(m_btnRead,  &QPushButton::clicked,
            this, &Hl2OptionsTab::onI2cReadClicked);
    connect(m_btnWrite, &QPushButton::clicked,
            this, &Hl2OptionsTab::onI2cWriteClicked);

    // Push read responses into the byte labels as they arrive.
    if (m_ioBoard) {
        connect(m_ioBoard, &IoBoardHl2::i2cReadResponseReceived, this,
                [this](quint8 /*retAddr*/, quint8 b0, quint8 b1, quint8 b2, quint8 b3) {
                    auto fmt = [](quint8 v) {
                        return QStringLiteral("%1").arg(v, 2, 16, QLatin1Char('0'))
                                                   .toUpper();
                    };
                    if (m_byte0Label) { m_byte0Label->setText(fmt(b0)); }
                    if (m_byte1Label) { m_byte1Label->setText(fmt(b1)); }
                    if (m_byte2Label) { m_byte2Label->setText(fmt(b2)); }
                    if (m_byte3Label) { m_byte3Label->setText(fmt(b3)); }
                });
    }

    onI2cEnableToggled(false);  // start disabled
}

// ── buildIoPinState ─────────────────────────────────────────────────────────

void Hl2OptionsTab::buildIoPinState(QWidget* parent)
{
    auto* col = new QVBoxLayout(parent);
    col->setSpacing(6);
    col->setContentsMargins(8, 16, 8, 8);

    // From mi0bot setup.designer.cs:11697-11716 ucIOPinsLedStripHF.DisplayBits = 6
    // [v2.10.3.13-beta2] — HL2 has 6 input pins (vs Hermes's 8).
    col->addWidget(new QLabel(tr("Input pins (Rx/i1..i5):"), parent));
    m_inputStrip = new OcLedStripWidget(parent);
    m_inputStrip->setDisplayBits(6);
    m_inputStrip->setInteractive(false);   // input port is read-only
    m_inputStrip->setToolTip(tr(
        "HL2 I/O Board input pins (6 bits, polled while board detected).  "
        "Read-only."));
    col->addWidget(m_inputStrip);

    // From mi0bot setup.designer.cs:11684-11695 ucOutPinsLedStripHF
    // .DisplayBits = 8 — HL2 output port is 8 LEDs.
    col->addWidget(new QLabel(tr("Output pins (o0..o7):"), parent));
    m_outputStrip = new OcLedStripWidget(parent);
    m_outputStrip->setDisplayBits(8);
    m_outputStrip->setToolTip(tr(
        "HL2 I/O Board output port (8 bits).  Click a LED to toggle when "
        "Pin Control is enabled."));
    col->addWidget(m_outputStrip);

    // From mi0bot setup.designer.cs:11662-11671 chkIOPinControl —
    // gates the click-to-toggle behavior on the output strip.
    m_chkPinControl = new QCheckBox(tr("Pin Control (click to toggle output)"), parent);
    m_chkPinControl->setToolTip(tr(
        "When enabled, clicking an output LED writes the new mask via "
        "I2C (bus 1, address 0x1D, register 169 = OC output register)."));
    col->addWidget(m_chkPinControl);

    col->addStretch();

    connect(m_chkPinControl, &QCheckBox::toggled, this, [this](bool on) {
        if (m_outputStrip) { m_outputStrip->setInteractive(on); }
    });
    connect(m_outputStrip, &OcLedStripWidget::pinClicked,
            this, &Hl2OptionsTab::onOutputPinClicked);
}

// ── syncFromModel ───────────────────────────────────────────────────────────

void Hl2OptionsTab::syncFromModel()
{
    if (!m_options) { return; }
    m_syncing = true;

    if (m_chkSwapAudio)       { QSignalBlocker b(m_chkSwapAudio);
        m_chkSwapAudio->setChecked(m_options->swapAudioChannels()); }
    if (m_chkCl2Enable)       { QSignalBlocker b(m_chkCl2Enable);
        m_chkCl2Enable->setChecked(m_options->cl2Enabled()); }
    if (m_udCl2Freq)          { QSignalBlocker b(m_udCl2Freq);
        m_udCl2Freq->setValue(m_options->cl2FreqMHz()); }
    if (m_chkExt10MHz)        { QSignalBlocker b(m_chkExt10MHz);
        m_chkExt10MHz->setChecked(m_options->ext10MHz()); }
    if (m_chkDisconnectReset) { QSignalBlocker b(m_chkDisconnectReset);
        m_chkDisconnectReset->setChecked(m_options->disconnectReset()); }
    if (m_udPttHang)          { QSignalBlocker b(m_udPttHang);
        m_udPttHang->setValue(m_options->pttHangMs()); }
    if (m_udTxLatency)        { QSignalBlocker b(m_udTxLatency);
        m_udTxLatency->setValue(m_options->txLatencyMs()); }
    if (m_chkPsSync)          { QSignalBlocker b(m_chkPsSync);
        m_chkPsSync->setChecked(m_options->psSync()); }
    if (m_chkBandVolts)       { QSignalBlocker b(m_chkBandVolts);
        m_chkBandVolts->setChecked(m_options->bandVolts()); }

    m_syncing = false;
}

// ── I2C Control slots ──────────────────────────────────────────────────────

void Hl2OptionsTab::onI2cEnableToggled(bool on)
{
    if (m_udI2cAddress)    { m_udI2cAddress->setEnabled(on); }
    if (m_udI2cRegister)   { m_udI2cRegister->setEnabled(on); }
    if (m_udI2cWriteData)  { m_udI2cWriteData->setEnabled(on); }
    if (m_chkI2cWriteEnable) { m_chkI2cWriteEnable->setEnabled(on); }
    if (m_btnRead)         { m_btnRead->setEnabled(on); }
    // Write button needs both gates.
    if (m_btnWrite) {
        m_btnWrite->setEnabled(on && m_chkI2cWriteEnable
                               && m_chkI2cWriteEnable->isChecked());
    }
    if (m_chkI2cWriteEnable) {
        connect(m_chkI2cWriteEnable, &QCheckBox::toggled, m_btnWrite,
                [this](bool we) {
                    if (m_btnWrite) {
                        m_btnWrite->setEnabled(
                            m_chkI2cEnable && m_chkI2cEnable->isChecked() && we);
                    }
                }, Qt::UniqueConnection);
    }
}

void Hl2OptionsTab::onI2cReadClicked()
{
    if (!m_ioBoard) { return; }
    IoBoardHl2::I2cTxn txn{};
    txn.bus           = 1;
    txn.address       = static_cast<quint8>(m_udI2cAddress->value());
    txn.control       = static_cast<quint8>(m_udI2cRegister->value());
    txn.writeData     = 0;
    txn.isRead        = true;
    txn.needsResponse = true;
    if (!m_ioBoard->enqueueI2c(txn)) {
        qCWarning(lcHl2Options) << "I2C read enqueue failed (queue full)";
    }
}

void Hl2OptionsTab::onI2cWriteClicked()
{
    if (!m_ioBoard) { return; }
    if (!m_chkI2cWriteEnable || !m_chkI2cWriteEnable->isChecked()) {
        qCWarning(lcHl2Options) << "Write blocked — write-enable not set";
        return;
    }
    IoBoardHl2::I2cTxn txn{};
    txn.bus           = 1;
    txn.address       = static_cast<quint8>(m_udI2cAddress->value());
    txn.control       = static_cast<quint8>(m_udI2cRegister->value());
    txn.writeData     = static_cast<quint8>(m_udI2cWriteData->value());
    txn.isRead        = false;
    txn.needsResponse = false;
    if (!m_ioBoard->enqueueI2c(txn)) {
        qCWarning(lcHl2Options) << "I2C write enqueue failed (queue full)";
    }
}

void Hl2OptionsTab::onOutputPinClicked(int idx)
{
    if (!m_ioBoard || !m_outputStrip) { return; }
    if (idx < 0 || idx > 7) { return; }
    const quint8 newMask = m_outputStrip->bits() ^ static_cast<quint8>(1u << idx);
    // Local visual feedback first; wire write follows.
    m_outputStrip->setBits(newMask);

    // Compose the I2C write that sets the OC output register.  Per the
    // design doc §3.2 row 3 the target is bus 1, addr 0x1D, register 169
    // (OC output).  No-op if no IoBoard or no HL2 connected — the txn
    // queues but won't drain until probe/init completes.
    IoBoardHl2::I2cTxn txn{};
    txn.bus           = 1;
    txn.address       = IoBoardHl2::kI2cAddrGeneral;  // 0x1D
    txn.control       = 169;                          // OC output register
    txn.writeData     = newMask;
    txn.isRead        = false;
    txn.needsResponse = false;
    if (!m_ioBoard->enqueueI2c(txn)) {
        qCWarning(lcHl2Options) << "Output pin toggle enqueue failed (queue full)";
    }
}

void Hl2OptionsTab::onIoBoardOcByteChanged(quint8 ocByte, int /*bandIdx*/, bool /*mox*/)
{
    if (m_outputStrip) { m_outputStrip->setBits(ocByte); }
}

#ifdef NEREUS_BUILD_TESTS
bool Hl2OptionsTab::swapAudioChannelsCheckedForTest() const
{
    return m_chkSwapAudio && m_chkSwapAudio->isChecked();
}
int Hl2OptionsTab::pttHangMsForTest() const
{
    return m_udPttHang ? m_udPttHang->value() : -1;
}
int Hl2OptionsTab::txLatencyMsForTest() const
{
    return m_udTxLatency ? m_udTxLatency->value() : -1;
}
quint8 Hl2OptionsTab::outputBitsForTest() const
{
    return m_outputStrip ? m_outputStrip->bits() : 0;
}
quint8 Hl2OptionsTab::inputBitsForTest() const
{
    return m_inputStrip ? m_inputStrip->bits() : 0;
}
bool Hl2OptionsTab::isI2cWriteEnabledForTest() const
{
    return m_btnWrite && m_btnWrite->isEnabled();
}
#endif

} // namespace NereusSDR
