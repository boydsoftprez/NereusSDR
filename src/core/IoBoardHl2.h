// =================================================================
// src/core/IoBoardHl2.h  (NereusSDR)
// =================================================================
//
// Ported from mi0bot/OpenHPSDR-Thetis sources:
//   Project Files/Source/Console/HPSDR/IoBoardHl2.cs (full)
//     — IOBoard class: Registers enum, HardwareVersion enum,
//       readRequest / readResponse / writeRequest / setFrequency,
//       128-byte register array, hardware-version detection
//   Project Files/Source/ChannelMaster/network.h:112-148
//     — _i2c struct: i2c_control bitfield (ctrl_read/ctrl_stop/
//       ctrl_request/ctrl_error/ctrl_read_available), i2c_queue[]
//       array layout, MAX_I2C_QUEUE = 32
//   Project Files/Source/Console/console.cs:25781-25945
//     — UpdateIOBoard() 12-step async loop: HardwareVersion probe,
//       state machine switch(state++) for REG_OP_MODE/REG_INPUT_PINS/
//       REG_FREQUENCY/REG_RF_INPUTS/REG_ANTENNA writes + reads
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Closes Phase 3I-T12 deferred work
//                (P1RadioConnection.cpp:892, 939, 1416-1462). Pure
//                model layer — Phase 3P-E Task 2 wires I2C intercept
//                into P1CodecHl2; Task 4 builds the Setup → Hardware →
//                HL2 I/O page UI.
// =================================================================
//
// --- From Console/HPSDR/IoBoardHl2.cs ---
/*
*
* Copyright (C) 2025 Reid Campbell, MI0BOT, mi0bot@trom.uk
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
// This module contains code to support the I/O Board used in the Hermes Lite 2
//
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

#pragma once

#include <QObject>
#include <QString>
#include <array>

namespace NereusSDR {

// Per-HL2 I/O board state — ports mi0bot IOBoard class and UpdateIOBoard()
// state machine.
//
// Sources:
//   mi0bot IoBoardHl2.cs (full) [@c26a8a4]
//   mi0bot network.h:112-148 [@c26a8a4]
//   mi0bot console.cs:25781-25945 [@c26a8a4]
class IoBoardHl2 : public QObject {
    Q_OBJECT

public:
    // Register map per mi0bot IoBoardHl2.cs:39-80 [@c26a8a4]
    // Integer values are verbatim from the C# enum.
    enum class Register : int {
        HardwareVersion   = -1,  // Special: read-only, different I2C address (0x41)
        REG_TX_FREQ_BYTE4 = 0,
        REG_TX_FREQ_BYTE3 = 1,
        REG_TX_FREQ_BYTE2 = 2,
        REG_TX_FREQ_BYTE1 = 3,
        REG_TX_FREQ_BYTE0 = 4,
        REG_CONTROL       = 5,
        REG_INPUT_PINS    = 6,
        REG_ANTENNA_TUNER = 7,
        REG_FAULT         = 8,
        REG_FIRMWARE_MAJOR = 9,
        REG_FIRMWARE_MINOR = 10,
        REG_RF_INPUTS     = 11,
        REG_FAN_SPEED     = 12,
        REG_FCODE_RX1     = 13,
        REG_FCODE_RX2     = 14,
        REG_FCODE_RX3     = 15,
        REG_FCODE_RX4     = 16,
        REG_FCODE_RX5     = 17,
        REG_FCODE_RX6     = 18,
        REG_FCODE_RX7     = 19,
        REG_FCODE_RX8     = 20,
        REG_FCODE_RX9     = 21,
        REG_FCODE_RX10    = 22,
        REG_FCODE_RX11    = 23,
        REG_FCODE_RX12    = 24,
        REG_ADC0_MSB      = 25,
        REG_ADC0_LSB      = 26,
        REG_ADC1_MSB      = 27,
        REG_ADC1_LSB      = 28,
        REG_ADC2_MSB      = 29,
        REG_ADC2_LSB      = 30,
        REG_ANTENNA       = 31,
        REG_OP_MODE       = 32,
        REG_STATUS        = 167,
        REG_IN_PINS       = 168,
        REG_OUT_PINS      = 169,
        GPIO_DIRECT_BASE  = 170,
    };

    // Hardware version constants per mi0bot IoBoardHl2.cs:82-85 [@c26a8a4]
    static constexpr quint8 kHardwareVersion1 = 0xF1;  // HardwareVersion::Version_1

    // I2C device addresses per mi0bot IoBoardHl2.cs:130-140 [@c26a8a4]
    static constexpr quint8 kI2cBusIndex       = 1;     // bus 2 (1-indexed)
    static constexpr quint8 kI2cAddrGeneral    = 0x1D;  // general registers at 0x1d
    static constexpr quint8 kI2cAddrHwVersion  = 0x41;  // hardware version at 0x41 reg 0

    // I2C TLV control byte bits per mi0bot network.h:120-124 [@c26a8a4]
    static constexpr quint8 CtrlRead          = 0x01;  // bit 00: 1=read, 0=write
    static constexpr quint8 CtrlStop          = 0x02;  // bit 01: stop after txn
    static constexpr quint8 CtrlRequest       = 0x04;  // bit 02: issue request
    static constexpr quint8 CtrlError         = 0x08;  // bit 03: firmware error
    static constexpr quint8 CtrlReadAvailable = 0x10;  // bit 04: read result ready
    static constexpr quint8 CtrlWrite         = 0x00;  // alias for clarity (bit 0 clear)

    // I2C queue entry — POD matching mi0bot network.h:i2c_queue[] struct layout
    // per network.h:133-138 [@c26a8a4].
    struct I2cTxn {
        quint8 bus{0};
        quint8 address{0};
        quint8 control{0};
        quint8 writeData{0};
        std::array<quint8, 4> readData{};
    };

    // MAX_I2C_QUEUE = 32 per mi0bot network.h:41 [@c26a8a4]
    static constexpr int kMaxI2cQueue = 32;

    // 12-step UpdateIOBoard cycle per mi0bot console.cs:25844-25928 [@c26a8a4]
    static constexpr int kStateMachineSteps = 12;

    // Size of the internal register mirror array.
    // Upstream uses byte[256] per IoBoardHl2.cs:91 [@c26a8a4].
    static constexpr int kRegisterArraySize = 256;

    explicit IoBoardHl2(QObject* parent = nullptr);

    // ── I2C queue ──
    // Circular FIFO buffer, kMaxI2cQueue slots, enqueue/dequeue/depth/clear.
    bool   enqueueI2c(const I2cTxn& txn);
    bool   dequeueI2c(I2cTxn& out);
    int    i2cQueueDepth() const;
    bool   i2cQueueIsEmpty() const;
    bool   i2cQueueIsFull() const;
    void   clearI2cQueue();

    // ── 12-step state machine ──
    // Mirrors the switch(state++) in mi0bot console.cs:25844-25928 [@c26a8a4].
    int     currentStep() const;
    void    advanceStep();                    // increments; wraps 11 → 0
    QString stepDescriptor(int step) const;  // human-readable label for logging/UI

    // ── Register mirror ──
    // Mirrors IoBoardHl2.cs:91 `byte[] registers = new byte[256]` [@c26a8a4].
    // HardwareVersion (Register value = -1) is stored in m_hardwareVersion,
    // not in the register array (it lives at a different I2C address upstream).
    quint8 registerValue(Register reg) const;
    void   setRegisterValue(Register reg, quint8 value);

    // Convenience accessor for the hardware version byte.
    quint8 hardwareVersion() const;
    void   setHardwareVersion(quint8 v);

    // ── Detection ──
    bool isDetected() const;
    void setDetected(bool on);

signals:
    void i2cQueueChanged();
    void stepAdvanced(int newStep);
    void registerChanged(NereusSDR::IoBoardHl2::Register reg, quint8 value);
    void hardwareVersionChanged(quint8 version);
    void detectedChanged(bool detected);

private:
    // Circular FIFO for I2C queue (oldest entry at head, newest at tail-1).
    std::array<I2cTxn, kMaxI2cQueue> m_i2cQueue{};
    int  m_i2cHead{0};
    int  m_i2cTail{0};
    int  m_i2cCount{0};

    int  m_currentStep{0};

    // Register mirror — index is the enum integer value (0–255).
    // Matches upstream `byte[] registers = new byte[256]` [@c26a8a4].
    std::array<quint8, kRegisterArraySize> m_registers{};

    // Hardware version byte — stored separately because upstream reads it from
    // a different I2C address (0x41) and stores it in a dedicated field.
    // Per IoBoardHl2.cs:89 `public byte hardwareVersion` [@c26a8a4].
    quint8 m_hardwareVersion{0};

    bool m_detected{false};
};

} // namespace NereusSDR
