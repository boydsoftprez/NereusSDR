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
//   2026-09-26 - Remote-window parity Task 14 (R-R3-46): i2cReadAnswered.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 2: setPollingPause / isPollingPaused. J.J.
//                Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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
    for (int i = 0; i < kMaxI2cQueue; ++i) {
        m_i2cSlots[i].sequence.store(static_cast<quint64>(i), std::memory_order_relaxed);
    }
    m_clock.start();
}

qint64 IoBoardHl2::nowMs() const
{
    return m_nowForTest ? m_nowForTest() : m_clock.elapsed();
}

void IoBoardHl2::setClockForTest(std::function<qint64()> nowMs)
{
    m_nowForTest = std::move(nowMs);
}

// ─────────────────────────────────────────────────────────────────────────────
// I2C queue
//
// Bounded FIFO, safe across threads. Ports the i2c_queue[] array and MAX_I2C_QUEUE=32
// from mi0bot network.h:41,133-138 [@c26a8a4]. The model holds the queue
// that Task 2 (P1CodecHl2 wiring) will drain into ep2 TLV frames.
// ─────────────────────────────────────────────────────────────────────────────

bool IoBoardHl2::enqueueI2c(const I2cTxn& txn)
{
    quint64 pos = m_i2cEnqueuePos.load(std::memory_order_relaxed);
    I2cSlot* slot = nullptr;
    for (;;) {
        slot = &m_i2cSlots[pos & (kMaxI2cQueue - 1)];
        const quint64 seq = slot->sequence.load(std::memory_order_acquire);
        const qint64 diff = static_cast<qint64>(seq - pos);
        if (diff == 0) {
            if (m_i2cEnqueuePos.compare_exchange_weak(pos, pos + 1,
                                                      std::memory_order_relaxed)) {
                break;
            }
        } else if (diff < 0) {
            return false;   // full (MAX_I2C_QUEUE)
        } else {
            pos = m_i2cEnqueuePos.load(std::memory_order_relaxed);
        }
    }
    slot->txn = txn;
    slot->sequence.store(pos + 1, std::memory_order_release);
    emit i2cQueueChanged();
    return true;
}

bool IoBoardHl2::dequeueI2c(I2cTxn& out)
{
    quint64 pos = m_i2cDequeuePos.load(std::memory_order_relaxed);
    I2cSlot* slot = nullptr;
    for (;;) {
        slot = &m_i2cSlots[pos & (kMaxI2cQueue - 1)];
        const quint64 seq = slot->sequence.load(std::memory_order_acquire);
        const qint64 diff = static_cast<qint64>(seq - (pos + 1));
        if (diff == 0) {
            if (m_i2cDequeuePos.compare_exchange_weak(pos, pos + 1,
                                                      std::memory_order_relaxed)) {
                break;
            }
        } else if (diff < 0) {
            return false;   // empty
        } else {
            pos = m_i2cDequeuePos.load(std::memory_order_relaxed);
        }
    }
    out = slot->txn;
    slot->sequence.store(pos + kMaxI2cQueue, std::memory_order_release);
    emit i2cQueueChanged();
    return true;
}

int IoBoardHl2::i2cQueueDepth() const
{
    // Read the consumer position first so a concurrent dequeue can only
    // make the result smaller than the truth, never negative.
    const quint64 out = m_i2cDequeuePos.load(std::memory_order_acquire);
    const quint64 in  = m_i2cEnqueuePos.load(std::memory_order_acquire);
    const qint64 depth = static_cast<qint64>(in - out);
    if (depth <= 0) { return 0; }
    return depth > kMaxI2cQueue ? kMaxI2cQueue : static_cast<int>(depth);
}

quint64 IoBoardHl2::i2cDequeuedCount() const
{
    return m_i2cDequeuePos.load(std::memory_order_acquire);
}

quint64 IoBoardHl2::i2cEnqueuedCount() const
{
    return m_i2cEnqueuePos.load(std::memory_order_acquire);
}

quint64 IoBoardHl2::i2cClearCount() const
{
    return m_i2cClearCount.load(std::memory_order_acquire);
}

bool IoBoardHl2::i2cQueueIsEmpty() const { return i2cQueueDepth() == 0; }
bool IoBoardHl2::i2cQueueIsFull() const  { return i2cQueueDepth() >= kMaxI2cQueue; }

void IoBoardHl2::clearI2cQueue()
{
    // Taken like the codec takes them, so a clear from the main thread is
    // safe while the connection thread fills and empties the queue. The
    // clear count goes up before the first dequeue (the fence orders it
    // ahead of them) and again after the last, so a writer that read it
    // part way through still sees it change.
    m_i2cClearCount.fetch_add(1, std::memory_order_seq_cst);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    I2cTxn discarded;
    while (dequeueI2c(discarded)) {
    }
    m_i2cClearCount.fetch_add(1, std::memory_order_seq_cst);
    emit i2cQueueChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// Pending-read FIFO
//
// Mi0bot (IoBoardHl2.cs:142-143) records one in-flight read at a time. We
// allow back-to-back reads via a parallel FIFO matching the txn queue order.
// Each codec dequeue of a read txn pushes a record; each EP6 response pops
// the oldest record so applyI2cReadResponse() can steer bytes to the right
// destination (hardwareVersion for 0x41, registers[] slot for 0x1D).
// ─────────────────────────────────────────────────────────────────────────────

bool IoBoardHl2::pushPendingRead(const PendingRead& r)
{
    // A read that went out kReadAnswerMs or more ago has been given up, as
    // mi0bot's poll gives it up after its 20 one-millisecond waits
    // (console.cs:25866-25872 [@c26a8a4]); this newer read takes the answer
    // slot, as mi0bot's I2CReadInitiate does (netInterface.c:1471-1497).
    //
    // An answer that is late rather than lost then lands on this newer read.
    // mi0bot does the same: I2CReadInitiate clears ctrl_read_available with
    // the rest of i2c_control (netInterface.c:1488 [@c26a8a4]), a late answer
    // sets it again (networkproto1.c:478-493 [@c26a8a4]), and I2CResponse
    // hands it to whoever asks next (netInterface.c:1567-1587 [@c26a8a4]).
    // The answer cannot be matched to its read by address: the low bits of
    // the returned C0 are not one mi0bot uses, as returned_address is never
    // set from the wire (network.h:145 [@c26a8a4], only cleared at
    // netInterface.c:1623 [@c26a8a4], under "// MI0BOT: HL2 I2C variables").
    const qint64 now = nowMs();
    while (m_pendingCount > 0 && now - m_pendingSentMs[m_pendingHead] >= kReadAnswerMs) {
        qDebug("HL2 I2C: read of %02X/%02X unanswered, given up",
               m_pendingReads[m_pendingHead].deviceAddress,
               m_pendingReads[m_pendingHead].subAddress);
        m_pendingHead = (m_pendingHead + 1) % kMaxI2cQueue;
        --m_pendingCount;
    }
    if (m_pendingCount >= kMaxI2cQueue) { return false; }
    m_pendingReads[m_pendingTail] = r;
    m_pendingSentMs[m_pendingTail] = now;
    m_pendingTail = (m_pendingTail + 1) % kMaxI2cQueue;
    ++m_pendingCount;
    return true;
}

bool IoBoardHl2::popPendingRead(PendingRead& out)
{
    if (m_pendingCount <= 0) { return false; }
    out = m_pendingReads[m_pendingHead];
    m_pendingHead = (m_pendingHead + 1) % kMaxI2cQueue;
    --m_pendingCount;
    return true;
}

bool IoBoardHl2::hasPendingRead()  const { return m_pendingCount > 0; }
int  IoBoardHl2::pendingReadDepth() const { return m_pendingCount; }

void IoBoardHl2::clearPendingReads()
{
    m_pendingHead = m_pendingTail = m_pendingCount = 0;
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

// From mi0bot console.cs:25642-25646 [@c26a8a4]:
//   public void SetI2CPollingPause( bool pause )
//   {
//       I2CPollingPause = pause;
//       if (pause) Thread.Sleep(45);
//   }
// NereusSDR difference: no 45 ms sleep. mi0bot sleeps so a poll read
// already in flight is answered before the manual one goes; here the queue
// is a FIFO the codec sends one per C&C frame, so a read the poll queued
// before the pause still goes, and is answered, ahead of the manual one.
void IoBoardHl2::setPollingPause(bool pause)
{
    m_pollingPaused.store(pause, std::memory_order_release);
}

bool IoBoardHl2::isPollingPaused() const
{
    return m_pollingPaused.load(std::memory_order_acquire);
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

IoBoardHl2::I2cReadResponse IoBoardHl2::lastI2cRead() const
{
    return m_lastI2cRead;
}

void IoBoardHl2::applyI2cReadResponse(quint8 c0, quint8 c1, quint8 c2,
                                      quint8 c3, quint8 c4)
{
    // Bit 7 of C0 is the response marker and has already been checked by
    // the caller. The low 7 bits are kept for the diagnostics log only:
    // mi0bot never reads them (networkproto1.c:478-493 [@c26a8a4]); the
    // answer belongs to the oldest outstanding read, popped below.
    m_lastI2cRead.returnedAddress = static_cast<quint8>(c0 & 0x7F);
    m_lastI2cRead.data = {c1, c2, c3, c4};
    m_lastI2cRead.available = true;
    qDebug("HL2 I2C IN:  C0=%02X C1=%02X C2=%02X C3=%02X C4=%02X (ret=%02X pendingDepth=%d)",
           c0, c1, c2, c3, c4, m_lastI2cRead.returnedAddress, m_pendingCount);

    // Pop the matching pending-read FIRST and route bytes to their
    // destination BEFORE emitting the signal.  Consumers that read back
    // IoBoardHl2 state from the response signal (e.g. the HL2 probe FSM
    // checking isDetected()) depend on the routing being complete by the
    // time their slot fires.  Pre-3L the emit ran first and the FSM
    // raced against setHardwareVersion — silently broken for tests but
    // worked on real hardware because of frame timing.
    //
    // From mi0bot IoBoardHl2.cs:148-170 readResponse() [@c26a8a4]:
    //   if (lastReadRequest == (int)Registers.HardwareVersion)
    //       hardwareVersion = read_data[3];
    //   else {
    //       registers[lastReadRequest+0] = read_data[3];
    //       registers[lastReadRequest+1] = read_data[2];
    //       registers[lastReadRequest+2] = read_data[1];
    //       registers[lastReadRequest+3] = read_data[0];
    //   }
    //
    // Mi0bot's read_data[0..3] map to wire bytes (C1..C4). The byte order
    // upstream stores into the register slots is REVERSED relative to wire
    // order (read_data[3]=C4 → registers[lastReadRequest+0]).
    PendingRead pr;
    const bool havePending = popPendingRead(pr);

    if (havePending) {
        if (pr.deviceAddress == kI2cAddrHwVersion) {
            // Special device: HW version sits at I2C addr 0x41 reg 0.
            // Mi0bot stores read_data[3] (= C4) into the dedicated
            // hardwareVersion field.
            setHardwareVersion(c4);
            // setDetected if version matches the known good ID (0xF1).
            // Source: mi0bot IoBoardHl2.cs:82-85 HardwareVersion.Version_1 [@c26a8a4]
            setDetected(c4 == kHardwareVersion1);
        } else {
            // General register read — fan four bytes into the 4-byte
            // sub-register block starting at the requested sub-address.
            const int base = static_cast<int>(pr.subAddress);
            if (base >= 0 && base + 3 < kRegisterArraySize) {
                m_registers[base + 0] = c4;  // read_data[3]
                m_registers[base + 1] = c3;  // read_data[2]
                m_registers[base + 2] = c2;  // read_data[1]
                m_registers[base + 3] = c1;  // read_data[0]
                emit registerChanged(static_cast<Register>(base + 0), c4);
            }
        }
    }

    // Phase 3L Codex P2: signal carries (returnedAddress, returnedSubAddress)
    // so consumers can gate on (deviceAddr, register) when multiple
    // outstanding reads share a device address (probe FW major vs FW
    // minor at 0x1d).  Emit AFTER routing so slot handlers see a
    // consistent IoBoardHl2 state.
    emit i2cReadResponseReceived(m_lastI2cRead.returnedAddress,
                                 havePending ? pr.subAddress : quint8(0),
                                 c1, c2, c3, c4);
    if (havePending) {
        emit i2cReadAnswered(pr.deviceAddress, pr.subAddress, c1, c2, c3, c4);
    }
}

void IoBoardHl2::clearI2cReadAvailable()
{
    m_lastI2cRead.available = false;
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
