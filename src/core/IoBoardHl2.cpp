// =================================================================
// src/core/IoBoardHl2.cpp  (NereusSDR)
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

#include "IoBoardHl2.h"

namespace NereusSDR {

IoBoardHl2::IoBoardHl2(QObject* parent) : QObject(parent)
{
    // Upstream initialises all 256 register bytes to 0.
    // Per mi0bot IoBoardHl2.cs:121-125 [@c26a8a4]:
    //   for (int i = 0; i < 256; i++) { registers[i] = 0; }
    m_registers.fill(0);
}

// ─────────────────────────────────────────────────────────────────────────────
// I2C queue
//
// Circular FIFO buffer. Ports the i2c_queue[] array and MAX_I2C_QUEUE=32
// from mi0bot network.h:41,133-138 [@c26a8a4]. The model holds the queue
// that Task 2 (P1CodecHl2 wiring) will drain into ep2 TLV frames.
// ─────────────────────────────────────────────────────────────────────────────

bool IoBoardHl2::enqueueI2c(const I2cTxn& txn)
{
    if (m_i2cCount >= kMaxI2cQueue) { return false; }
    m_i2cQueue[m_i2cTail] = txn;
    m_i2cTail = (m_i2cTail + 1) % kMaxI2cQueue;
    ++m_i2cCount;
    emit i2cQueueChanged();
    return true;
}

bool IoBoardHl2::dequeueI2c(I2cTxn& out)
{
    if (m_i2cCount <= 0) { return false; }
    out = m_i2cQueue[m_i2cHead];
    m_i2cHead = (m_i2cHead + 1) % kMaxI2cQueue;
    --m_i2cCount;
    emit i2cQueueChanged();
    return true;
}

int  IoBoardHl2::i2cQueueDepth() const   { return m_i2cCount; }
bool IoBoardHl2::i2cQueueIsEmpty() const { return m_i2cCount == 0; }
bool IoBoardHl2::i2cQueueIsFull() const  { return m_i2cCount >= kMaxI2cQueue; }

void IoBoardHl2::clearI2cQueue()
{
    m_i2cHead = m_i2cTail = m_i2cCount = 0;
    emit i2cQueueChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// 12-step state machine
//
// Mirrors the UpdateIOBoard() switch(state++) per mi0bot console.cs:25844-25928
// [@c26a8a4]. The C# implementation is a C# async-loop with await Task.Delay(40ms)
// between steps; here we expose the pure step counter + descriptor so the
// P1CodecHl2 Task-2 layer can drive it from the ep2 send cadence.
//
// switch(state++) case mapping (verbatim from upstream):
//   case 0:            write REG_OP_MODE (if mode changed)
//   case 1, 4, 7, 10:  read REG_INPUT_PINS (blocking read/response)
//   case 2, 8:         write REG_FREQUENCY (setFrequency)
//   case 3, 6:         write REG_RF_INPUTS (if IOBoardAerialMode changed)
//   case 5, 9:         write REG_ANTENNA   (if IOBoardAerialPorts changed)
//   case 11 / default: state = 0           (reset cycle)
// ─────────────────────────────────────────────────────────────────────────────

int IoBoardHl2::currentStep() const { return m_currentStep; }

void IoBoardHl2::advanceStep()
{
    m_currentStep = (m_currentStep + 1) % kStateMachineSteps;
    emit stepAdvanced(m_currentStep);
}

QString IoBoardHl2::stepDescriptor(int step) const
{
    // From mi0bot console.cs UpdateIOBoard switch(state++) [@c26a8a4]
    switch (step) {
        case 0:  return QStringLiteral("WR REG_OP_MODE");     // mode selection
        case 1:  return QStringLiteral("RD REG_INPUT_PINS");  // read input pins
        case 2:  return QStringLiteral("WR REG_FREQUENCY");   // write TX frequency
        case 3:  return QStringLiteral("WR REG_RF_INPUTS");   // secondary RX selection
        case 4:  return QStringLiteral("RD REG_INPUT_PINS");  // read input pins
        case 5:  return QStringLiteral("WR REG_ANTENNA");     // aerial selection
        case 6:  return QStringLiteral("WR REG_RF_INPUTS");   // secondary RX selection
        case 7:  return QStringLiteral("RD REG_INPUT_PINS");  // read input pins
        case 8:  return QStringLiteral("WR REG_FREQUENCY");   // write TX frequency
        case 9:  return QStringLiteral("WR REG_ANTENNA");     // aerial selection
        case 10: return QStringLiteral("RD REG_INPUT_PINS");  // read input pins
        case 11: return QStringLiteral("CYCLE");              // state = 0 (reset)
        default: return QStringLiteral("?");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Register mirror
//
// Mirrors upstream `byte[] registers = new byte[256]` from IoBoardHl2.cs:91
// [@c26a8a4]. The register index is the integer value of the Register enum.
// HardwareVersion (enum value -1) is stored in m_hardwareVersion separately,
// matching the upstream design where hardwareVersion is a public property,
// not part of the registers[] array.
// ─────────────────────────────────────────────────────────────────────────────

quint8 IoBoardHl2::registerValue(Register reg) const
{
    if (reg == Register::HardwareVersion) { return m_hardwareVersion; }
    const int idx = static_cast<int>(reg);
    if (idx < 0 || idx >= kRegisterArraySize) { return 0; }
    return m_registers[idx];
}

void IoBoardHl2::setRegisterValue(Register reg, quint8 value)
{
    if (reg == Register::HardwareVersion) {
        setHardwareVersion(value);
        return;
    }
    const int idx = static_cast<int>(reg);
    if (idx < 0 || idx >= kRegisterArraySize) { return; }
    if (m_registers[idx] == value) { return; }
    m_registers[idx] = value;
    emit registerChanged(reg, value);
}

quint8 IoBoardHl2::hardwareVersion() const { return m_hardwareVersion; }

void IoBoardHl2::setHardwareVersion(quint8 v)
{
    if (m_hardwareVersion == v) { return; }
    m_hardwareVersion = v;
    emit hardwareVersionChanged(v);
    emit registerChanged(Register::HardwareVersion, v);
}

// ─────────────────────────────────────────────────────────────────────────────
// Detection
//
// Tracks whether the HL2 I/O board was found at startup (hardware version
// probe succeeded and version == kHardwareVersion1). Mirrors the
// SetupForm.HL2IOBoardPresent path in mi0bot console.cs:25810-25818 [@c26a8a4].
// ─────────────────────────────────────────────────────────────────────────────

bool IoBoardHl2::isDetected() const { return m_detected; }

void IoBoardHl2::setDetected(bool on)
{
    if (m_detected == on) { return; }
    m_detected = on;
    emit detectedChanged(on);
}

} // namespace NereusSDR
