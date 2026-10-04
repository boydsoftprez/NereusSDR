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
//   2026-09-26 - Remote-window parity Task 14 (R-R3-46): i2cReadAnswered,
//                the answered read's device address and register. J.J.
//                Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 2: the polling pause (mi0bot
//                SetI2CPollingPause), held while the manual I2C tool waits
//                on an answer. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
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

#include "core/NereusCoreExport.h"
#include <QObject>
#include <QString>
#include <QElapsedTimer>
#include <array>
#include <atomic>
#include <functional>

namespace NereusSDR {

// Per-HL2 I/O board state — ports mi0bot IOBoard class and UpdateIOBoard()
// state machine.
//
// Sources:
//   mi0bot IoBoardHl2.cs (full) [@c26a8a4]
//   mi0bot network.h:112-148 [@c26a8a4]
//   mi0bot console.cs:25781-25945 [@c26a8a4]
class NEREUS_CORE_EXPORT IoBoardHl2 : public QObject {
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

    // Upstream i2c_control bitfield per mi0bot network.h:120-124 [@c26a8a4].
    // These describe the GLOBAL bits mi0bot keeps on prn->i2c (ctrl_read,
    // ctrl_request, ctrl_error, ctrl_read_available). NereusSDR moves
    // ctrl_read / ctrl_request to per-txn flags (I2cTxn::isRead /
    // needsResponse) and ctrl_read_available to I2cReadResponse::available.
    // Kept here as wire-format reference values; not used for queue-entry
    // composition.
    static constexpr quint8 CtrlRead          = 0x01;  // bit 00: 1=read, 0=write
    static constexpr quint8 CtrlStop          = 0x02;  // bit 01: stop after txn
    static constexpr quint8 CtrlRequest       = 0x04;  // bit 02: issue request
    static constexpr quint8 CtrlError         = 0x08;  // bit 03: firmware error
    static constexpr quint8 CtrlReadAvailable = 0x10;  // bit 04: read result ready
    static constexpr quint8 CtrlWrite         = 0x00;  // alias for clarity (bit 0 clear)

    // I2C queue entry.
    //
    // mi0bot's queue entry (network.h:133-138 [@c26a8a4]) carries only bus /
    // address / control / write_data; read/request flags live on the global
    // prn->i2c struct (ctrl_read / ctrl_request) and are set by I2CReadInitiate
    // / I2CWriteInitiate.  The wire encoder reads the globals at compose time.
    //
    // NereusSDR moves the flags onto the queue entry so multiple back-to-back
    // transactions (e.g. the HW-version probe + firmware-version reads) can
    // each carry their own intent without stomping global state.  The wire
    // encoder still emits exactly the same bytes — it just sources the flags
    // per-txn instead of from globals.
    //
    // `control` is the C3 wire byte (= I2C sub-address / register index for
    // register-based devices), preserving mi0bot's queue-entry semantics.
    struct I2cTxn {
        quint8 bus{0};
        quint8 address{0};
        quint8 control{0};        // C3 wire byte — I2C sub-address / register
        quint8 writeData{0};      // C4 wire byte
        bool   isRead{false};     // → C1 = 0x07 (read) vs 0x06 (write)
        bool   needsResponse{false}; // → C0 bit 7 (ctrl_request)
        std::array<quint8, 4> readData{};
    };

    // Last I2C read response from EP6 — mirrors the upstream fields
    //   prn->i2c.read_data[0..3] + prn->i2c.ctrl_read_available
    //   per mi0bot network.h:112-148 [@c26a8a4].
    // `returnedAddress` is the low 7 bits of the response C0 byte, which
    // identifies which outstanding read the payload belongs to (set by
    // upstream firmware before framing the response).
    struct I2cReadResponse {
        quint8 returnedAddress{0};
        std::array<quint8, 4> data{};  // [0]=C1, [1]=C2, [2]=C3, [3]=C4
        bool   available{false};       // mirrors ctrl_read_available
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
    // Bounded FIFO, kMaxI2cQueue slots, safe from any thread without a lock.
    bool   enqueueI2c(const I2cTxn& txn);
    bool   dequeueI2c(I2cTxn& out);
    int    i2cQueueDepth() const;
    bool   i2cQueueIsEmpty() const;
    bool   i2cQueueIsFull() const;
    void   clearI2cQueue();
    // How many transactions have left the queue since it was made. A
    // writer that finds the queue full reads it to tell a queue that is
    // draining from one that has stopped.
    quint64 i2cDequeuedCount() const;
    // How many transactions have gone into the queue since it was made. A
    // writer reads it after its last write: once i2cDequeuedCount() reaches
    // it, that write has left the queue.
    quint64 i2cEnqueuedCount() const;
    // Goes up by one as clearI2cQueue() starts and again as it ends. A
    // writer that sees it change knows writes it queued may have been
    // dropped rather than sent. Read it after i2cDequeuedCount(), with an
    // acquire fence between the two: a count raised by the clear's own
    // dequeues then always comes with the change.
    quint64 i2cClearCount() const;

    // ── Pending-read FIFO ──
    // Mi0bot tracks one in-flight read at a time (IoBoardHl2.cs:142-143
    // `lastReadRequest`) because I2CReadInitiate refuses new reads while one
    // is pending. NereusSDR allows multiple reads to be queued back-to-back
    // (the HL2 firmware processes I2C in queue order and responses arrive in
    // the same order), so we mirror the queue with a parallel FIFO of
    // pending reads — each codec dequeue of a read txn pushes a record;
    // each EP6 I2C response pops the oldest record and routes the bytes to
    // the right destination via applyI2cReadResponse().
    // Source: mi0bot IoBoardHl2.cs:129-170 [@c26a8a4]
    struct PendingRead {
        quint8 deviceAddress{0};
        quint8 subAddress{0};
    };
    bool pushPendingRead(const PendingRead& r);   // codec calls on dequeue
    bool popPendingRead(PendingRead& out);        // dispatcher pops oldest
    bool hasPendingRead() const;
    int  pendingReadDepth() const;
    void clearPendingReads();

    // A read the radio does not answer is given up, as mi0bot's poll gives
    // up on it (console.cs:25866-25872 [@c26a8a4]):
    //   do { await Task.Delay(1); status = ioBoard.readResponse();
    //        if (timeout++ >= 20) break; } while (1 == status);
    // and the next read then takes the answer slot (netInterface.c:1471-1497
    // I2CReadInitiate clears i2c_control). Here a read that went out
    // kReadAnswerMs or more before the next one is dropped from the
    // pending-read FIFO when that next read goes out, so its missing answer
    // cannot take the next read's answer.
    static constexpr qint64 kReadAnswerMs = 21;
    // Tests drive the clock the FIFO ages reads by (milliseconds, any
    // origin, never going back). Null restores the monotonic clock.
    void setClockForTest(std::function<qint64()> nowMs);

    // ── 12-step state machine ──
    // Mirrors the switch(state++) in mi0bot console.cs:25844-25928 [@c26a8a4].
    int     currentStep() const;
    void    advanceStep();                    // increments; wraps 11 → 0
    QString stepDescriptor(int step) const;  // human-readable label for logging/UI

    // ── Polling pause ──
    // From mi0bot console.cs:25640-25646 [@c26a8a4] I2CPollingPause /
    // SetI2CPollingPause: while held, the poll waits on its current step
    // (console.cs:25931-25935, the Task.Delay loop) so a manual I2C
    // transaction has the bus to itself. Set on the main thread, read on
    // the connection thread, hence atomic.
    void setPollingPause(bool pause);
    bool isPollingPaused() const;

    // ── Register mirror ──
    // Mirrors IoBoardHl2.cs:91 `byte[] registers = new byte[256]` [@c26a8a4].
    // HardwareVersion (Register value = -1) is stored in m_hardwareVersion,
    // not in the register array (it lives at a different I2C address upstream).
    quint8 registerValue(Register reg) const;
    void   setRegisterValue(Register reg, quint8 value);

    // ── I2C read-response mirror ──
    // Stores the most recent EP6 I2C read response. Phase 3P-E Task 2:
    // byte persistence + consumer signal; full register-slot dispatch from
    // returnedAddress lands in Task 3 with the 12-step state machine.
    // Source: mi0bot network.h:112-148 prn->i2c.read_data / ctrl_read_available
    //   [@c26a8a4]
    // Upstream inline attribution preserved verbatim:
    //   network.h:109  int reset_on_disconnect;       // MI0BOT: Reset on software disconnect
    //   network.h:110  int swap_audio_channels;       // MI0BOT: Control to swap the left and right audio channels send over P1
    //   network.h:113  struct _i2c    // MI0BOT: I2C data structure for HL2
    I2cReadResponse lastI2cRead() const;
    void applyI2cReadResponse(quint8 c0, quint8 c1, quint8 c2, quint8 c3, quint8 c4);
    void clearI2cReadAvailable();

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
    // Emitted when applyI2cReadResponse() stores a new EP6 response.
    // Consumer: the HL2 I/O diagnostics page's raw IN log.
    //
    // `returnedAddress` is C0's low 7 bits as the radio sent them; mi0bot
    // never reads them (networkproto1.c:478-493 [@c26a8a4]), so no
    // consumer may use them to tell which read was answered: listen to
    // i2cReadAnswered for that (the HL2 probe does, parity Task 14
    // follow-up).  `returnedSubAddress` is the register of the outstanding
    // read popped from the pending-read FIFO at emit time.
    void i2cReadResponseReceived(quint8 returnedAddress,
                                 quint8 returnedSubAddress,
                                 quint8 b0, quint8 b1, quint8 b2, quint8 b3);

    // Remote-window parity Task 14 (R-R3-46): the same answer as
    // i2cReadResponseReceived, named by the (device address, register) of
    // the read it answers, both taken from the pending-read FIFO when the
    // answer popped it. Emitted only when an outstanding read was popped.
    // The firmware's returned C0 names the I2C controller, not the device,
    // so a consumer that must tell one device's answer from another's
    // (RadioModel's I2C tool request, P1RadioConnection's HL2 probe)
    // listens here.
    void i2cReadAnswered(quint8 deviceAddress, quint8 subAddress,
                         quint8 c1, quint8 c2, quint8 c3, quint8 c4);

    // Diagnostic tap: emitted from the HL2 codec just after it composes the
    // outbound I2C wire bytes for the next ep2 frame.  The diagnostics page
    // logs both the OUT bytes (this signal) and the IN bytes
    // (i2cReadResponseReceived) so a wire-format mismatch is visible side-by-side.
    void i2cTxComposed(quint8 c0, quint8 c1, quint8 c2, quint8 c3, quint8 c4);

    // Diagnostic tap: emitted from buildCodecContext() whenever the resolved
    // bank-0 OC byte changes (band switch or MOX flip).  Drives the live OC
    // pin grid + hex label on the HL2 I/O Board diagnostic page so the user
    // can see exactly which OC pins are active for the current band/MOX state.
    // Plan Task 14 fix wave: the displays now read RadioModel::bandOutputsByte
    // (every connection reports it, and a remote window gets the Core's);
    // this signal is still emitted beside the Core's "HL2 ocByte=" log line.
    void currentOcByteChanged(quint8 ocByte, int bandIdx, bool mox);

private:
    // The I2C queue is filled from the main thread (the I2C tool, the
    // output pins) and from the connection thread (the poll, the probe),
    // and emptied by the codec on the connection thread as it builds each
    // C&C frame. It is a bounded lock-free queue (each slot carries a
    // sequence number; producers and consumers claim positions with a
    // compare-exchange), so no thread ever waits on a lock and the send
    // path never blocks. FIFO order holds across all producers.
    static_assert((kMaxI2cQueue & (kMaxI2cQueue - 1)) == 0,
                  "the I2C queue indexes by mask");
    struct I2cSlot {
        std::atomic<quint64> sequence{0};
        I2cTxn txn;
    };
    std::array<I2cSlot, kMaxI2cQueue> m_i2cSlots{};
    alignas(64) std::atomic<quint64> m_i2cEnqueuePos{0};
    alignas(64) std::atomic<quint64> m_i2cDequeuePos{0};
    std::atomic<quint64> m_i2cClearCount{0};

    // When each pending read went out (nowMs()), parallel to m_pendingReads.
    std::array<qint64, kMaxI2cQueue> m_pendingSentMs{};
    QElapsedTimer m_clock;
    std::function<qint64()> m_nowForTest;
    qint64 nowMs() const;

    int  m_currentStep{0};
    std::atomic<bool> m_pollingPaused{false};

    // Register mirror — index is the enum integer value (0–255).
    // Matches upstream `byte[] registers = new byte[256]` [@c26a8a4].
    std::array<quint8, kRegisterArraySize> m_registers{};

    // Hardware version byte — stored separately because upstream reads it from
    // a different I2C address (0x41) and stores it in a dedicated field.
    // Per IoBoardHl2.cs:89 `public byte hardwareVersion` [@c26a8a4].
    quint8 m_hardwareVersion{0};

    bool m_detected{false};

    // Last EP6 I2C read response — mirrors prn->i2c.read_data[] + flag.
    I2cReadResponse m_lastI2cRead{};

    // Pending-read FIFO — parallel to the I2C queue.  Each codec dequeue of a
    // read txn pushes a record; each EP6 response pops the oldest.  Both run
    // on the connection thread, so this FIFO needs no guard.
    std::array<PendingRead, kMaxI2cQueue> m_pendingReads{};
    int m_pendingHead{0};
    int m_pendingTail{0};
    int m_pendingCount{0};
};

} // namespace NereusSDR
