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
//   2026-09-23 - R-R3-46: the TX buffer latency and PTT hang follow the
//                transmit permission. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-24 - R-R3-49: the second I2C bus choice (bus 0) is hidden until
//                 it is built (UnbuiltFeatures).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (remote-window parity Task 13, plan C6): the TX
//                 buffer latency and PTT hang rows are hidden until built
//                 (UnbuiltFeatures). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-26 - R-R3-46 (remote-window parity Task 14): Read, Write and
//                 Pin Control go through RadioModel (requestIoBoardI2c,
//                 setIoBoardOutput), so a remote window reaches the Core's
//                 radio; the output strip shows the output register read
//                 back; a write and Pin Control close while the radio is on
//                 the air. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-26 - Parity Task 14 follow-up (R-R3-46): the four read-response
//                 boxes follow mi0bot's txtI2CByte3..txtI2CByte0 (C1 at the
//                 register + 3 on the left, C4 at the register on the right)
//                 with its per-box tooltips. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-29 - Swap audio channels, Enable CL2, CL2 frequency and
//                 External 10 MHz are stored but not sent to the radio, so
//                 they show disabled with a plain reason; the "wire
//                 emission" warnings are gone. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 1: the TX buffer latency and PTT hang
//                 reach the radio (bank 17), so their rows are shown again.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 1: the power supply sync box says what a
//                 tick does, "Disable power supply sync" (mi0bot's "Disable
//                 PS Sync"), not "PureSignal sync". J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 2: the input strip shows the input pins
//                 register (6) as each poll reads it, lit pins red while
//                 on the air, in a local window and a remote one alike.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - The Hermes Lite Options boxes carry the ids of the rows
//                 the Setup description gives them (version 16), so a
//                 test holds the two alike. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 clock options: Enable CL2, CL2 frequency and External
//                 10 MHz reach the radio (mi0bot setup.cs:21694-21756
//                 [@c26a8a4]), so they are enabled with mi0bot's tooltips;
//                 the frequency box follows Enable CL2, and a remote window
//                 needs a Core that sends them (setClockControlAvailable).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Radio codec lane: Swap audio channels reaches the radio
//                 (P1RadioConnection::setHl2SwapAudioChannels), so it is
//                 enabled with mi0bot's tooltip; a remote window needs a
//                 Core at radioHardwareVersion 13 (setSwapAudioAvailable).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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
#include "HardwareTransmitGate.h"
#include "gui/UnbuiltFeatures.h"

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
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cmath>

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

    // ── Output register → output strip ────────────────────────────────────
    // R-R3-46 (remote-window parity Task 14): the strip shows the I/O
    // board's output register (169) as last read back, as mi0bot's
    // ucOutPinsLedStripHF shows read_data[3] of that read
    // (setup.cs:30006-30036 [@c26a8a4]). In a remote window the Core's
    // `ioBoard` outputs are written into this model's board
    // (IoBoardHl2Facade). The HL2 I/O tab's OC strip still shows the band
    // output byte the radio is sent.
    if (m_ioBoard) {
        connect(m_ioBoard, &IoBoardHl2::registerChanged, this,
                [this](IoBoardHl2::Register reg, quint8) {
                    if (reg == IoBoardHl2::Register::REG_OUT_PINS) {
                        onOutputsChanged();
                    } else if (reg == IoBoardHl2::Register::REG_INPUT_PINS) {
                        // From mi0bot console.cs:25887 [@c26a8a4]: each read of
                        // REG_INPUT_PINS calls UpdateIOLedStrip(MOX, registers[6]).
                        m_inputStrip->setBits(
                            m_ioBoard->registerValue(IoBoardHl2::Register::REG_INPUT_PINS));
                    }
                });
        onOutputsChanged();
        m_inputStrip->setBits(m_ioBoard->registerValue(IoBoardHl2::Register::REG_INPUT_PINS));
    }
    // A write and Pin Control close while the radio is on the air, in a
    // local window and a remote one alike. The input strip lights its pins
    // in the transmit color on the air: from mi0bot setup.cs:22606-22610
    // UpdateIOLedStrip [@c26a8a4], ucIOPinsLedStripHF.TX = tx.
    if (m_model) {
        connect(m_model, &RadioModel::coreOnAirChanged, this, [this](bool onAir) {
            m_inputStrip->setTx(onAir);
            applyIoGates();
        });
        m_inputStrip->setTx(m_model->isCoreOnAir());
    }
    applyIoGates();
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
    m_udTxLatency->setObjectName(QStringLiteral("hl2TxBufferLatency"));
    m_udTxLatency->setProperty("nereusSetupId", "hardware.hl2Io.txLatency");
    grid->addWidget(m_udTxLatency, row, 1);
    ++row;

    // From mi0bot setup.designer.cs:11235-11258 udPTTHang (PTT hang,
    // ms) [v2.10.3.13-beta2] — range 0..30, default 12.
    grid->addWidget(new QLabel(tr("PTT hang:"), parent), row, 0);
    m_udPttHang = new QSpinBox(parent);
    m_udPttHang->setRange(Hl2OptionsModel::kPttHangMinMs,
                          Hl2OptionsModel::kPttHangMaxMs);
    m_udPttHang->setSuffix(tr(" ms"));
    m_udPttHang->setObjectName(QStringLiteral("hl2PttHang"));
    m_udPttHang->setProperty("nereusSetupId", "hardware.hl2Io.pttHang");
    grid->addWidget(m_udPttHang, row, 1);
    ++row;

    // From mi0bot setup.designer.cs:11166-11176 chkCl2Enable +
    // :11133-11163 udCl2Freq [@c26a8a4] - range 1..200 MHz, three decimal
    // places, step 0.1, default 116. The box applies on commit (Enter or
    // leaving it), as a NumericUpDown does, so typing does not reprogram
    // the CL2 output at every keystroke.
    //
    // The clock options reach the radio's clock chip over I2C
    // (P1RadioConnection::setHl2Clock, mi0bot setup.cs:21694-21756
    // [@c26a8a4]). In a remote window they wait for a Core that sends them
    // (setClockControlAvailable).
    // From mi0bot setup.designer.cs:11174 [@c26a8a4]:
    //   this.toolTip1.SetToolTip(this.chkCl2Enable, "Enable frequency output on CL2");
    m_chkCl2Enable = new QCheckBox(tr("Enable CL2"), parent);
    m_chkCl2Enable->setObjectName(QStringLiteral("hl2Cl2Enable"));
    m_chkCl2Enable->setProperty("nereusSetupId", "hardware.hl2Io.cl2Enable");
    m_chkCl2Enable->setToolTip(tr("Enable frequency output on CL2"));
    grid->addWidget(m_chkCl2Enable, row, 0);
    m_udCl2Freq = new QDoubleSpinBox(parent);
    m_udCl2Freq->setDecimals(3);
    m_udCl2Freq->setSingleStep(0.1);
    m_udCl2Freq->setRange(Hl2OptionsModel::kCl2FreqMinKHz / 1000.0,
                          Hl2OptionsModel::kCl2FreqMaxKHz / 1000.0);
    m_udCl2Freq->setKeyboardTracking(false);
    m_udCl2Freq->setSuffix(tr(" MHz"));
    m_udCl2Freq->setObjectName(QStringLiteral("hl2Cl2Freq"));
    m_udCl2Freq->setProperty("nereusSetupId", "hardware.hl2Io.cl2Freq");
    // The row label the Setup description gives the box beside Enable CL2.
    m_udCl2Freq->setAccessibleName(tr("CL2 frequency"));
    // From mi0bot setup.designer.cs:11158 [@c26a8a4]:
    //   this.toolTip1.SetToolTip(this.udCl2Freq, "Output frequency on CL2 output");
    m_udCl2Freq->setToolTip(tr("Output frequency on CL2 output"));
    grid->addWidget(m_udCl2Freq, row, 1);
    ++row;

    // From mi0bot setup.designer.cs:11178-11189 chkExt10MHz [@c26a8a4]:
    //   this.chkExt10MHz.Text = "Ext 10MHz (CL1 Input)";
    //   this.toolTip1.SetToolTip(this.chkExt10MHz, "Enable external 10MHz input on CL1");
    m_chkExt10MHz = new QCheckBox(tr("External 10 MHz reference"), parent);
    m_chkExt10MHz->setObjectName(QStringLiteral("hl2Ext10MHz"));
    m_chkExt10MHz->setProperty("nereusSetupId", "hardware.hl2Io.ext10MHz");
    m_chkExt10MHz->setToolTip(tr("Enable external 10 MHz input on CL1"));
    grid->addWidget(m_chkExt10MHz, row, 0, 1, 2);
    ++row;

    // From mi0bot setup.designer.cs:11258 chkDisconnectReset
    m_chkDisconnectReset = new QCheckBox(tr("Reset on Ethernet disconnect"), parent);
    m_chkDisconnectReset->setObjectName(QStringLiteral("hl2DisconnectReset"));
    m_chkDisconnectReset->setProperty("nereusSetupId", "hardware.hl2Io.disconnectReset");
    grid->addWidget(m_chkDisconnectReset, row, 0, 1, 2);
    ++row;

    // From mi0bot setup.designer.cs:11293-11301 chkHL2PsSync [@c26a8a4]:
    //   this.chkHL2PsSync.Text = "Disable PS Sync";
    //   this.toolTip1.SetToolTip(this.chkHL2PsSync, "Disables the FPGA synchronisation of the power supply clock");
    // PS is the power supply, not PureSignal: a tick disables the power
    // supply clock sync (setup.cs:13384-13390, // MI0BOT: Control power
    // supply sync for the HL2). Spelled out so it cannot read as PureSignal.
    m_chkPsSync = new QCheckBox(tr("Disable power supply sync"), parent);
    m_chkPsSync->setObjectName(QStringLiteral("hl2DisablePsSync"));
    m_chkPsSync->setProperty("nereusSetupId", "hardware.hl2Io.psSync");
    m_chkPsSync->setToolTip(tr("Stops the radio synchronizing its power supply clock."));
    grid->addWidget(m_chkPsSync, row, 0, 1, 2);
    ++row;

    // From mi0bot setup.designer.cs:11305-11313 chkHL2BandVolts
    m_chkBandVolts = new QCheckBox(tr("Band Volts (PWM out 0–3.3 V)"), parent);
    m_chkBandVolts->setObjectName(QStringLiteral("hl2BandVolts"));
    m_chkBandVolts->setProperty("nereusSetupId", "hardware.hl2Io.bandVolts");
    grid->addWidget(m_chkBandVolts, row, 0, 1, 2);
    ++row;

    // From mi0bot setup.designer.cs:11343 chkSwapAudioChannels
    // mi0bot swaps the left and right audio it sends the radio over P1
    // (networkproto1.c:1231-1239 [@c26a8a4]). NereusSDR now sends the
    // receive audio in each TX frame's L/R bytes, and RadioModel's
    // applyHl2Options hands this option to P1RadioConnection, so it is live.
    // Tooltip from mi0bot setup.designer.cs:11119 [@c26a8a4], with its
    // "ot" typo read as "to".
    m_chkSwapAudio = new QCheckBox(tr("Swap audio channels"), parent);
    m_chkSwapAudio->setObjectName(QStringLiteral("hl2SwapAudioChannels"));
    m_chkSwapAudio->setProperty("nereusSetupId", "hardware.hl2Io.swapAudioChannels");
    m_chkSwapAudio->setToolTip(tr("Swap the audio channels sent to the HL2"));
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
                    }
                });
    };
    auto bindInt = [this](QSpinBox* sp, auto setter) {
        connect(sp, qOverload<int>(&QSpinBox::valueChanged), this,
                [this, setter](int v) {
                    if (m_syncing) { return; }
                    if (m_options) {
                        (m_options->*setter)(v);
                    }
                });
    };

    bindBool(m_chkSwapAudio,        &Hl2OptionsModel::setSwapAudioChannels);
    bindBool(m_chkCl2Enable,        &Hl2OptionsModel::setCl2Enabled);
    connect(m_udCl2Freq, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double mhz) {
                if (m_syncing) { return; }
                if (m_options) {
                    m_options->setCl2FreqKHz(static_cast<int>(std::lround(mhz * 1000.0)));
                }
            });
    bindBool(m_chkExt10MHz,         &Hl2OptionsModel::setExt10MHz);
    bindBool(m_chkDisconnectReset,  &Hl2OptionsModel::setDisconnectReset);
    bindInt (m_udPttHang,           &Hl2OptionsModel::setPttHangMs);
    bindInt (m_udTxLatency,         &Hl2OptionsModel::setTxLatencyMs);
    bindBool(m_chkPsSync,           &Hl2OptionsModel::setPsSync);
    bindBool(m_chkBandVolts,        &Hl2OptionsModel::setBandVolts);

    // From mi0bot setup.cs:21694-21729 ControlCl2 [@c26a8a4]:
    //   // MI0BOT: Support for HL2 Cl2 clock output
    //   udCl2Freq.Enabled = enable;
    // The frequency box follows Enable CL2.
    connect(m_chkCl2Enable, &QCheckBox::toggled, this, [this](bool) { applyClockGates(); });
    applyClockGates();
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
    auto* bus0 = new QCheckBox(tr("0"), parent);
    bus0->setEnabled(false);
    bus0->setToolTip(tr("I2C bus 0 on the HL2. NereusSDR reads and writes bus 1 only."));
    grid->addWidget(bus0, row, 1);
    // R-R3-49: the second bus is hidden until it is built; bus 1 stays.
    bus0->setObjectName(QStringLiteral("hl2I2cBus0"));
    UnbuiltFeatures::hideUnlessBuilt(bus0, UnbuiltFeature::Hl2SecondI2cBus);
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

    // Read response display: 4 hex bytes, laid out as mi0bot lays them out.
    // From mi0bot setup.designer.cs [@c26a8a4] (txtI2CByte0..3 Location and
    // toolTip1.SetToolTip): left to right txtI2CByte3 (x 153, "Data at
    // address+3"), txtI2CByte2 (x 184, +2), txtI2CByte1 (x 215, +1) and
    // txtI2CByte0 (x 246, "Data at address"). btnI2CRead_MouseDown
    // (setup.cs:21486-21489) fills byte0 from read_data[3] (C4) through
    // byte3 from read_data[0] (C1), so the boxes read C1..C4 left to right,
    // the register itself on the right. "address" in mi0bot's tooltips is
    // the register chosen in Reg/Ctrl, so the tooltips name that control.
    grid->addWidget(new QLabel(tr("Read response:"), parent), row, 0);
    auto* respRow = new QHBoxLayout();
    auto makeByteLbl = [parent](const QString& tip) {
        auto* lbl = new QLabel(QStringLiteral("--"), parent);
        lbl->setFixedWidth(28);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { background: white; color: black; "
            "font-family: monospace; border: 1px solid #555; padding: 2px; }"));
        lbl->setToolTip(tip);
        return lbl;
    };
    m_byte3Label = makeByteLbl(tr("Data at Reg/Ctrl + 3"));
    m_byte2Label = makeByteLbl(tr("Data at Reg/Ctrl + 2"));
    m_byte1Label = makeByteLbl(tr("Data at Reg/Ctrl + 1"));
    m_byte0Label = makeByteLbl(tr("Data at Reg/Ctrl"));
    respRow->addWidget(m_byte3Label);
    respRow->addWidget(m_byte2Label);
    respRow->addWidget(m_byte1Label);
    respRow->addWidget(m_byte0Label);
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

    // R-R3-46 (parity Task 14): why the last request was not done.
    m_i2cStatusLabel = new QLabel(parent);
    m_i2cStatusLabel->setObjectName(QStringLiteral("hl2I2cStatus"));
    m_i2cStatusLabel->setWordWrap(true);
    m_i2cStatusLabel->hide();
    grid->addWidget(m_i2cStatusLabel, row, 0, 1, 4);
    ++row;

    grid->setRowStretch(row, 1);

    // ── Wire ──────────────────────────────────────────────────────────────
    connect(m_chkI2cEnable, &QCheckBox::toggled,
            this, &Hl2OptionsTab::onI2cEnableToggled);
    connect(m_btnRead,  &QPushButton::clicked,
            this, &Hl2OptionsTab::onI2cReadClicked);
    connect(m_btnWrite, &QPushButton::clicked,
            this, &Hl2OptionsTab::onI2cWriteClicked);

    // Belt-and-braces Write gate: m_btnWrite only enables when BOTH
    // chkI2cEnable and chkI2cWriteEnable are checked. Wired exactly once
    // here in the build path — previously this was wired inside
    // onI2cEnableToggled with Qt::UniqueConnection + lambda, which is the
    // shape Qt warns about ("unique connections require a pointer to member
    // function") because lambdas can't be deduped. #272.
    connect(m_chkI2cWriteEnable, &QCheckBox::toggled,
            this, &Hl2OptionsTab::syncI2cWriteButtonEnabled);

    // The byte labels show this tool's own read's answer (onI2cReadClicked),
    // from this window's radio or, in a remote window, the Core's.

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
        "Read-only. Lit pins show red while transmitting."));
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

    connect(m_chkPinControl, &QCheckBox::toggled, this, [this](bool) { applyIoGates(); });
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
        m_udCl2Freq->setValue(m_options->cl2FreqKHz() / 1000.0); }
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
    applyClockGates();
}

// ── I2C Control slots ──────────────────────────────────────────────────────

void Hl2OptionsTab::onI2cEnableToggled(bool /*on*/)
{
    applyIoGates();
}

void Hl2OptionsTab::syncI2cWriteButtonEnabled()
{
    applyIoGates();
}

void Hl2OptionsTab::setIoBoardControlAvailable(bool available, const QString& reason)
{
    m_ioAvailable = available;
    m_ioUnavailableReason = available ? QString() : reason;
    applyIoGates();
}

void Hl2OptionsTab::setClockControlAvailable(bool available, const QString& reason)
{
    m_clockAvailable = available;
    m_clockUnavailableReason = available ? QString() : reason;
    applyClockGates();
}

void Hl2OptionsTab::setSwapAudioAvailable(bool available, const QString& reason)
{
    m_swapAudioAvailable = available;
    m_swapAudioUnavailableReason = available ? QString() : reason;
    // No on-air rule: mi0bot's chkSwapAudioChannels_CheckedChanged sets
    // NetworkIO.SwapAudioChannels with no MOX check (setup.cs:38065
    // [@c26a8a4]).
    HardwareTransmitGate::apply(m_chkSwapAudio, m_swapAudioAvailable,
                                m_swapAudioUnavailableReason);
}

void Hl2OptionsTab::applyClockGates()
{
    // Enable CL2 and External 10 MHz follow the Core's offer (a remote
    // window); the frequency box also follows Enable CL2, as mi0bot's
    // ControlCl2 sets udCl2Freq.Enabled. No on-air rule: mi0bot writes the
    // clock chip with no MOX check.
    HardwareTransmitGate::apply(m_chkCl2Enable, m_clockAvailable, m_clockUnavailableReason);
    HardwareTransmitGate::apply(m_chkExt10MHz, m_clockAvailable, m_clockUnavailableReason);
    HardwareTransmitGate::apply(m_udCl2Freq, m_clockAvailable, m_clockUnavailableReason);
    if (m_udCl2Freq) {
        m_udCl2Freq->setEnabled(m_clockAvailable && m_chkCl2Enable
                                && m_chkCl2Enable->isChecked());
    }
}

void Hl2OptionsTab::applyIoGates()
{
    // The tool's own gates: I2C Enable opens the group, Write enable the
    // Write button. On top: the Core's offer (a remote window), then the
    // on-air rule for what writes to the board (parity Task 14).
    const bool i2cOn = m_chkI2cEnable && m_chkI2cEnable->isChecked();
    const bool writeOn = m_chkI2cWriteEnable && m_chkI2cWriteEnable->isChecked();
    const bool onAir = m_model != nullptr && m_model->isCoreOnAir();
    const QString writeReason = !m_ioAvailable ? m_ioUnavailableReason
                                               : RadioModel::onAirReason();
    const bool writesOpen = m_ioAvailable && !onAir;
    for (QWidget* w : std::initializer_list<QWidget*>{
             m_udI2cAddress, m_udI2cRegister, m_udI2cWriteData, m_chkI2cWriteEnable}) {
        if (w) { w->setEnabled(i2cOn && m_ioAvailable); }
    }
    if (m_btnRead) {
        HardwareTransmitGate::apply(m_btnRead, m_ioAvailable, m_ioUnavailableReason);
        m_btnRead->setEnabled(i2cOn && m_ioAvailable);
    }
    if (m_btnWrite) {
        HardwareTransmitGate::apply(m_btnWrite, writesOpen, writeReason);
        m_btnWrite->setEnabled(i2cOn && writeOn && writesOpen);
    }
    HardwareTransmitGate::apply(m_chkPinControl, writesOpen, writeReason);
    if (m_outputStrip) {
        m_outputStrip->setInteractive(writesOpen && m_chkPinControl
                                      && m_chkPinControl->isChecked());
    }
}

void Hl2OptionsTab::showI2cStatus(const QString& text)
{
    if (!m_i2cStatusLabel) { return; }
    m_i2cStatusLabel->setText(text);
    m_i2cStatusLabel->setVisible(!text.isEmpty());
}

void Hl2OptionsTab::onI2cReadClicked()
{
    if (!m_model) { return; }
    RadioModel::IoBoardI2cRequest request;
    request.bus = IoBoardHl2::kI2cBusIndex;
    request.address = m_udI2cAddress->value();
    request.reg = m_udI2cRegister->value();
    request.write = false;
    showI2cStatus({});
    const QPointer<Hl2OptionsTab> self(this);
    m_model->requestIoBoardI2c(request, [self](bool ok, qint64 value, const QString& reason) {
        if (!self) { return; }
        if (!ok) {
            self->showI2cStatus(reason);
            return;
        }
        // From mi0bot setup.cs:21486-21489 [@c26a8a4]: byte0 = read_data[3]
        // (C4, the register itself) .. byte3 = read_data[0] (C1). `value`
        // packs C1 in its top byte and C4 in its low byte.
        auto fmt = [value](int shift) {
            return QStringLiteral("%1").arg((value >> shift) & 0xFF, 2, 16, QLatin1Char('0'))
                                       .toUpper();
        };
        if (self->m_byte0Label) { self->m_byte0Label->setText(fmt(0)); }
        if (self->m_byte1Label) { self->m_byte1Label->setText(fmt(8)); }
        if (self->m_byte2Label) { self->m_byte2Label->setText(fmt(16)); }
        if (self->m_byte3Label) { self->m_byte3Label->setText(fmt(24)); }
    });
}

void Hl2OptionsTab::onI2cWriteClicked()
{
    if (!m_model) { return; }
    if (!m_chkI2cWriteEnable || !m_chkI2cWriteEnable->isChecked()) {
        qCWarning(lcHl2Options) << "Write blocked — write-enable not set";
        return;
    }
    RadioModel::IoBoardI2cRequest request;
    request.bus = IoBoardHl2::kI2cBusIndex;
    request.address = m_udI2cAddress->value();
    request.reg = m_udI2cRegister->value();
    request.write = true;
    request.value = m_udI2cWriteData->value();
    showI2cStatus({});
    const QPointer<Hl2OptionsTab> self(this);
    m_model->requestIoBoardI2c(request, [self](bool ok, qint64, const QString& reason) {
        if (self && !ok) { self->showI2cStatus(reason); }
    });
}

void Hl2OptionsTab::onOutputPinClicked(int idx)
{
    if (!m_model || !m_outputStrip) { return; }
    if (idx < 0 || idx > 7) { return; }
    // From mi0bot setup.cs:30039-30056 [@c26a8a4] ucOutPinsLedStripHF_MouseDown:
    // the clicked pin toggled against the strip, then the register read
    // back (RadioModel::setIoBoardOutput). The strip changes only when the
    // read-back arrives.
    const bool on = (m_outputStrip->bits() & (1u << idx)) == 0;
    showI2cStatus({});
    const QPointer<Hl2OptionsTab> self(this);
    m_model->setIoBoardOutput(idx, on, [self](bool ok, qint64, const QString& reason) {
        if (self && !ok) { self->showI2cStatus(reason); }
    });
}

void Hl2OptionsTab::onOutputsChanged()
{
    if (!m_outputStrip || !m_ioBoard) { return; }
    m_outputStrip->setBits(m_ioBoard->registerValue(IoBoardHl2::Register::REG_OUT_PINS));
}

void Hl2OptionsTab::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_model) {
        m_model->refreshIoBoardOutputs();
    }
}

void Hl2OptionsTab::setTransmitPermitted(bool permitted, const QString& reason)
{
    HardwareTransmitGate::apply(m_udTxLatency, permitted, reason);
    HardwareTransmitGate::apply(m_udPttHang, permitted, reason);
}

#ifdef NEREUS_BUILD_TESTS
bool Hl2OptionsTab::swapAudioChannelsCheckedForTest() const
{
    return m_chkSwapAudio && m_chkSwapAudio->isChecked();
}
bool Hl2OptionsTab::transmitTimingsEnabledForTest() const
{
    return m_udPttHang && m_udTxLatency && m_udPttHang->isEnabled()
        && m_udTxLatency->isEnabled();
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
bool Hl2OptionsTab::inputStripTxForTest() const
{
    return m_inputStrip && m_inputStrip->tx();
}
bool Hl2OptionsTab::isI2cWriteEnabledForTest() const
{
    return m_btnWrite && m_btnWrite->isEnabled();
}
bool Hl2OptionsTab::isI2cReadEnabledForTest() const
{
    return m_btnRead && m_btnRead->isEnabled();
}
bool Hl2OptionsTab::isPinControlEnabledForTest() const
{
    return m_chkPinControl && m_chkPinControl->isEnabled();
}
QString Hl2OptionsTab::i2cWriteToolTipForTest() const
{
    return m_btnWrite ? m_btnWrite->toolTip() : QString();
}
QString Hl2OptionsTab::pinControlToolTipForTest() const
{
    return m_chkPinControl ? m_chkPinControl->toolTip() : QString();
}
QString Hl2OptionsTab::i2cResponseTextForTest() const
{
    QStringList bytes;
    for (const QLabel* label : {m_byte3Label, m_byte2Label, m_byte1Label, m_byte0Label}) {
        bytes << (label ? label->text() : QString());
    }
    return bytes.join(QLatin1Char(' '));
}
QStringList Hl2OptionsTab::i2cByteToolTipsForTest() const
{
    QStringList tips;
    for (const QLabel* label : {m_byte3Label, m_byte2Label, m_byte1Label, m_byte0Label}) {
        tips << (label ? label->toolTip() : QString());
    }
    return tips;
}
QString Hl2OptionsTab::i2cStatusTextForTest() const
{
    return m_i2cStatusLabel && !m_i2cStatusLabel->isHidden() ? m_i2cStatusLabel->text()
                                                             : QString();
}
void Hl2OptionsTab::readI2cForTest(int address, int reg)
{
    m_chkI2cEnable->setChecked(true);
    m_udI2cAddress->setValue(address);
    m_udI2cRegister->setValue(reg);
    if (m_btnRead->isEnabled()) { m_btnRead->click(); }
}
void Hl2OptionsTab::writeI2cForTest(int address, int reg, int data)
{
    m_chkI2cEnable->setChecked(true);
    m_chkI2cWriteEnable->setChecked(true);
    m_udI2cAddress->setValue(address);
    m_udI2cRegister->setValue(reg);
    m_udI2cWriteData->setValue(data);
    if (m_btnWrite->isEnabled()) { m_btnWrite->click(); }
}
void Hl2OptionsTab::clickOutputPinForTest(int pin)
{
    m_chkPinControl->setChecked(true);
    if (m_outputStrip->isInteractive()) { onOutputPinClicked(pin); }
}
#endif

} // namespace NereusSDR
