// no-port-check: piHPSDR-only port; exact notices and source mapping are in
// docs/attribution/PIHPSDR-PROVENANCE.md. Shared contributor callsigns are
// not evidence of a Thetis port. This does not exempt piHPSDR attribution.
// src/core/saturn/SaturnXdma.cpp (NereusSDR)
// Ported from piHPSDR sources at @4aa95c5; verbatim source notices follow.
// Modification history (NereusSDR):
//   2026-10-28 - Recover notices for Martinus Stroomer's (CT1IQI) C++
//                 adaptation using Codeconvert.ai. J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex. Unfinished backend retained
//                 outside the build; no register-write or DMA use is enabled.
//
// --- From piHPSDR src/saturndrivers.c [@4aa95c5] ---
/* Copyright (C)
*  2021 - Laurence Barker G8NJJ
*  2025 - Christoph van Wüllen, DL1YCF
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*
*/

// --- From piHPSDR src/saturnregisters.c [@4aa95c5] ---
/* Copyright (C)
*  2021 - Laurence Barker G8NJJ
*  2025 - Christoph van Wüllen, DL1YCF
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*
*/

/* 2026 -  Martinus Stroomer, CT1IQI
 * This source code supports use of the xdma interface on a Saturn board,
 * as used in the Apache-Labs 'Anan G2' Software Defined Radio (SDR).
 * The documentation provided by the designers of Saturn was used.
 * Code written in C for the G2 in the applications P2app and piHPSDR has
 * been extensively re-used, aranged to fit the Nereus-SDR application.   
 * Main authors of Saturn's SDR design, documentation, and code support in
 * p2app and piHPSDR applications are:   
 * Laurence Barker, G8NJJ
 * Rick Koch, N1GP
 * John Melton, G0ORX
 * Christoph van Wüllen, DL1YCF.
 *
 * Written as extension to NereusSDR, J.J. Boyd , KG4VCF
 * Copyright references made elsewhere in NereusSDR apply here as well.
 *
 * Software architectural comments:
 * Where all basis and examples have the starting point that ethernet UDP IP packets 
 * are used for communication between control program and G2 SDR, using a convention
 * called Protocol 2, the code here is used to support as direct a communication via
 * the xdma interface as possible. The code only comes into action in case Nereus-SDR
 * is running on the Anan G2 itself as host computer with its PCIe XDMA interface.
 * The steps to create or analyze ethernet IP packets are skipped.
 * Local copies are kept of the FPGA status and settings and changes are only sent 
 * via xdma when indeed a change needs to be made to one or more settings. 
 */

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <string>
#include <cerrno>
#include <cmath>
#include <fcntl.h>
#include "SaturnXdma.h"
#include "../LogCategories.h"

#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QtGlobal>

namespace NereusSDR {

//
// Mutexes to protect register updates:
// In the following list, you find at the end variable names. They are used
// to store the value of a FPGA register to suppress unnecessary DMA writes.
// Between reading and updating this variables we need exclusive access
// ensured by a mutex.
// The code always looks like this:
//
// pthread_mutex_lock(&Mutex);
// if (new_value != stored_value) {
//   RegisterWrite(register, new_value);
//   stored_value = new_value;
// }
// pthread_mutex_unlock(&Mutex);
//
//
static QMutex codecMutex;     // GCodecPath, GCodecGain
static QMutex gpioMutex;      // GPIORegValue
static QMutex ddcInSelMutex;  // DDCInSelReg
static QMutex txConfigMutex;  // TXConfigRegValue
static QMutex ddcRateMutex;   // DDCRateReg, GDDCEnable
static QMutex iambicMutex;    // GIambicConfigReg
static QMutex keyerMutex;     // GCWKeyerSetup, GCWKeyerRampLength
static QMutex ddcRegMutex;    // DDCDeltaPhase[]
static QMutex ducRegMutex;    // DUCDeltaPhase
static QMutex alexRxMutex;    // GAlexRXRegister
static QMutex alexTxMutex;    // GAlexTXAntRegister, GAlexTXFiltRegister
static QMutex txDriveMutex;   // GTXDACCtrl
static QMutex sideToneMutex;  // GSideToneReg
static QMutex attenMutex;     // GRXADCCtrl

//
// 8 bit Codec register write over the AXILite bus via SPI
// using simple SPI writer IP
// given 7 bit register address and 9 bit data
//
// Note: "protection codec" is in the caller
//
static void codecRegisterWrite(unsigned int address, unsigned int data) {
    quint32 writeData;
    writeData = (address << 9) | (data & 0x01FF);
    regWrite(VADDRCODECSPIWRITEREG, writeData);
    usleep(5);
}

//
// ROMs for DAC Current Setting and 0.5dB step digital attenuator
//
static unsigned int dacCurrentRom[256];          // used for residual attenuation
static unsigned int dacStepAttenRom[256];        // provides most atten setting

//
// local copies of values written to registers
//
static quint32 ddcDeltaPhase[VNUMDDC];           // DDC frequency settings                 (ddcRegMutex)
static quint32 ducDeltaPhase;                    // DUC frequency setting                  (ducRegMutex)
static quint32 gStatusRegister;                  // most recent status register setting    (no mutex necessary)
static quint32 gpioRegValue;                     // value stored into GPIO                 (gpioMutex)
static quint32 txConfigRegValue;                 // value written into TX config register  (txConfigMutex)
static quint32 ddcInSelReg;                      // value written into DDC config register (ddcInSelMutex)
static quint32 ddcRateReg;                       // value written into DDC rate register   (ddcRateMutex)
static quint32 gDdcEnabled;                      // 1 bit per DDC                          (ddcRateMutex)
static quint32 gTxDacCtrl;                       // TX DAC current setting & atten         (txDriveMutex)
static quint32 gRxAdcCtrl;                       // RX1 & 2 attenuations                   (attenMutex)
static quint32 gAlexTxFiltRegister;              // 16 bit used of 32                      (alexTxMutex)
static quint32 gAlexTxAntRegister;               // 16 bit used of 32                      (alexTxMutex)
static quint32 gAlexRxRegister;                  // 32 bit RX register                     (alexRxMutex)
static quint32 gIambicConfigReg;                 // copy of iambic comfig register         (iambicMutex)
static quint32 gCwKeyerSetup;                    // keyer control register                 (keyerMutex)
static quint32 gSideToneReg;                     // side tone configuration                (sideToneMutex)
static bool gCwEnabled;                          // true if CW mode                        (no mutex necessary)
static bool gBreakinEnabled;                     // true if break-in is enabled            (no mutex necessary)
static unsigned int gCwKeyerRampLength = 0;      // ramp length for keyer, in samples      (keyerMutex)

//
// local copies of Codec registers
//
static unsigned int gCodecGain;                  // Codec gain register                    (codecMutex)
static unsigned int gCodecPath;                  // Codec path register                    (codecMutex)

//
// Saturn PCB Version, needed for codec ID
// (PCB version 3 onwards will have a TLV320AIC3204)
static ECodecType installedCodec;                // Codec type on the Saturn board

//
// addresses of the DDC frequency registers
//
static quint32 ddcRegisters[VNUMDDC] = {
    VADDRDDC0REG,
    VADDRDDC1REG,
    VADDRDDC2REG,
    VADDRDDC3REG,
    VADDRDDC4REG,
    VADDRDDC5REG,
    VADDRDDC6REG,
    VADDRDDC7REG,
    VADDRDDC8REG,
    VADDRDDC9REG
};


static int register_fd = -1;                     // device identifier, -1 means "not open"

// Saturn project: Artix7 FPGA + Raspberry Pi4 Compute Module [original inline comment from piHPSDR src/saturndrivers.c:22]
// PCI Express interface from linux on Raspberry pi [original inline comment from piHPSDR src/saturndrivers.c:23]
// this application uses C code to emulate HPSDR protocol 1 [original inline comment from piHPSDR src/saturndrivers.c:24]
// Contribution of interfacing to PiHPSDR from N1GP (Rick Koch) [original inline comment from piHPSDR src/saturndrivers.c:26]
// saturndrivers.c: [original inline comment from piHPSDR src/saturndrivers.c:28]
// Drivers for minor IP cores [original inline comment from piHPSDR src/saturndrivers.c:29]
// for function min() [original inline comment from piHPSDR src/saturndrivers.c:35]
// Global "version" variables, determined upon startup [original inline comment from piHPSDR src/saturndrivers.c:56]
// - open/close connection to the XDMA device driver for register and DMA access [original inline comment from piHPSDR src/saturndrivers.c:63]
// - DMA read and write operations from/to FPGA [original inline comment from piHPSDR src/saturndrivers.c:64]
// - register read and write [original inline comment from piHPSDR src/saturndrivers.c:65]
// - monitoring FIFOs [original inline comment from piHPSDR src/saturndrivers.c:66]
// - Analyse DDC headers [original inline comment from piHPSDR src/saturndrivers.c:67]
// If hitting "discover" several times on the startup screen, [original inline comment from piHPSDR src/saturndrivers.c:75]
// XDMA is opened repeatedly, so close it first [original inline comment from piHPSDR src/saturndrivers.c:76]
int openXDMADriver() {
  int Result = 0;

  //
  // Close the xdma interface first, if open due to earlier contact attempts
  //
  if (register_fd >= 0) { close(register_fd); }

  //
  // Note this fd is used for both pread() and pwrite() so use read-write mode
  //
  if ((register_fd = open("/dev/xdma0_user", O_RDWR)) == -1) {
    qCDebug(NereusSDR::lcProtocol) << "register R/W address space not available";
  } else {
    qCDebug(NereusSDR::lcProtocol) << "register access connected to /dev/xdma0_user";
    Result = 1;
  }
  return Result;
}

//
// close connection to the XDMA device driver for register and DMA access
//
void closeXDMADriver() {
  if (register_fd >= 0) {
    close(register_fd);
    register_fd = -1;
  }
}

//
// 32 bit register read over the AXILite bus
//
quint32 regRead(quint32 Address) {
  quint32 result = 0;

  if (register_fd < 0) {
    return result;
  }

  if (pread(register_fd, &result, sizeof(result), (off_t) Address) != sizeof(result)) {
    qCDebug(NereusSDR::lcProtocol) << "ERROR: register read: addr=" << std::to_string(Address)
                                   << "Error: no. " << strerror(errno);
  }

  return result;
}

//
// 32 bit register write over the AXILite bus
//
void regWrite(quint32 Address, quint32 Data) {
  if (register_fd < 0) {
    return;
  }

  if (pwrite(register_fd, &Data, sizeof(Data), (off_t) Address) != sizeof(Data)) {
      qCDebug(NereusSDR::lcProtocol) << "ERROR: register write: addr=" << std::to_string(Address)
                                     << "Error: no. " << strerror(errno);
  }
}

//
// initiate a DMA to the FPGA with specified parameters
// returns 0 if success, else -EIO
// fd: file device (an open file)
// srcData: pointer to memory block to transfer
// length: number of bytes to copy
// axiAddr: offset address in the FPGA window
//
// SrcData: pointer to memory block to transfer [original inline comment from piHPSDR src/saturndrivers.c:105]
// Length: number of bytes to copy [original inline comment from piHPSDR src/saturndrivers.c:106]
// AXIAddr: offset address in the FPGA window [original inline comment from piHPSDR src/saturndrivers.c:107]
int dmaWriteToFpga(int fd, unsigned char *srcData, quint32 length, quint32 axiAddr) {
    ssize_t rc;                 // response code
    // write data to FPGA from memory buffer
    rc = pwrite(fd, srcData, length, (off_t)axiAddr);

    if (rc < 0) {
        qCDebug(NereusSDR::lcProtocol, "write 0x%x @ 0x%lx failed %ld.", (int)length, (long)axiAddr, (long)rc);
        perror("DMA write");
        return -EIO;
    }

    return 0;
}

//
// initiate a DMA from the FPGA with specified parameters
// returns 0 if success, else -EIO
// fd: file device (an open file)
// destData: pointer to memory block to transfer
// length: number of bytes to copy
// axiAddr: offset address in the FPGA window
//
// DestData: pointer to memory block to transfer [original inline comment from piHPSDR src/saturndrivers.c:125]
// Length: number of bytes to copy [original inline comment from piHPSDR src/saturndrivers.c:126]
// AXIAddr: offset address in the FPGA window [original inline comment from piHPSDR src/saturndrivers.c:127]
int dmaReadFromFpga(int fd, unsigned char *destData, quint32 length, quint32 axiAddr) {
    ssize_t rc;                 // response code
    // read data from FPGA to memory buffer
    rc = pread(fd, destData, length, (off_t)axiAddr);

    if (rc < 0) {
        qCDebug(NereusSDR::lcProtocol, "read 0x%x @ 0x%lX failed %ld.", (int)length, (long)axiAddr, (long)rc);
        perror("DMA read");
        return -EIO;
    }

    return 0;
}

//
// DMA FIFO depths
// this is the number of *64 bit* FIFO locations
// this is now version dependent, and updated by InitialiseFIFOSizes()
//
static quint32 dmaFifoDepths[VNUMDMAFIFO] = {
    8192,             //  eRXDDCDMA,    selects RX
    1024,             //  eTXDUCDMA,    selects TX
    256,              //  eMicCodecDMA, selects mic samples
    256               //  eSpkCodecDMA  selects speaker samples
};

////////////////////////////////////////////////////////////////////////////////
//
// from common/saturndrivers.c
//
////////////////////////////////////////////////////////////////////////////////

//
// void setupFifoMonitorChannel(EDMAStreamSelect channel, bool enableInterrupt);
//
// Setup a single FIFO monitor channel.
//   channel:     IP channel number (enum)
//   enableInterrupt: true if interrupt generation enabled for overflows
// modified 28/9/2023 to remove "write FIFO": FPGA now detects overflow AND underflow
//
// void SetupFIFOMonitorChannel(EDMAStreamSelect Channel, bool EnableInterrupt); [original inline comment from piHPSDR src/saturndrivers.c:192]
//   Channel:     IP channel number (enum) [original inline comment from piHPSDR src/saturndrivers.c:195]
//   EnableInterrupt: true if interrupt generation enabled for overflows [original inline comment from piHPSDR src/saturndrivers.c:196]
void setupFifoMonitorChannel(EDMAStreamSelect channel, bool enableInterrupt, RadioInfo rinfo) {
    quint32 address;             // register address
    quint32 data;                // register content
    static bool gFifoSizesInitialised = false;

    if (!gFifoSizesInitialised) {
        if (rinfo.firmwareVersion < 10) {
            qCDebug(NereusSDR::lcProtocol) << "loading new FIFO sizes for 0.x firmware";
            dmaFifoDepths[0] = 8192;        //  eRXDDCDMA (RX)
            dmaFifoDepths[1] = 1024;        //  eTXDUCDMA (TX)
            dmaFifoDepths[2] = 256;         //  eMicCodecDMA (Mic)
            dmaFifoDepths[3] = 256;         //  eSpkCodecDMA (Headphone)
        } else if (rinfo.firmwareVersion <= 12) {
            qCDebug(NereusSDR::lcProtocol) << "loading new FIFO sizes for 1.0, 1.1, 1.2 firmware";
            dmaFifoDepths[0] = 16384;       //  eRXDDCDMA (RX)
            dmaFifoDepths[1] = 2048;        //  eTXDUCDMA (TX)
            dmaFifoDepths[2] = 256;         //  eMicCodecDMA (Mic)
            dmaFifoDepths[3] = 1024;        //  eSpkCodecDMA (Headphone)
        } else {
            qCDebug(NereusSDR::lcProtocol) << "loading new FIFO sizes for firmware version 1.3 and newer";
            dmaFifoDepths[0] = 16384;       //  eRXDDCDMA (RX)
            dmaFifoDepths[1] = 4096;        //  eTXDUCDMA (TX)
            dmaFifoDepths[2] = 256;         //  eMicCodecDMA (Mic)
            dmaFifoDepths[3] = 1024;        //  eSpkCodecDMA (Headphone)
        }

        gFifoSizesInitialised = true;
    }

    address = VADDRFIFOMONBASE + 4 * channel + 0x10;      // config register address
    data = dmaFifoDepths[(int)channel];                   // memory depth

    if (enableInterrupt) {
        data += 0x80000000;  // bit 31
    }

    regWrite(address, data);
}

//
// quint32 readFifoMonitorChannel(EDMAStreamSelect channel, bool* overflowed);
//
// Read number of locations in a FIFO
// for a read FIFO: returns the number of occupied locations available to read
// for a write FIFO: returns the number of free locations available to write
//   channel:     IP core channel number (enum)
//   overflowed:    true if an overflow has occurred. Reading clears the overflow bit.
//   overThreshold:   true if overflow occurred  measures by threshold. Cleared by read.
//   underflowed:       true if underflow has occurred. Cleared by read.
//   current:           number of locations occupied (in either FIFO type)
//
// uint32_t ReadFIFOMonitorChannel(EDMAStreamSelect Channel, bool* Overflowed); [original inline comment from piHPSDR src/saturndrivers.c:234]
//   Channel:     IP core channel number (enum) [original inline comment from piHPSDR src/saturndrivers.c:239]
//   Overflowed:    true if an overflow has occurred. Reading clears the overflow bit. [original inline comment from piHPSDR src/saturndrivers.c:240]
//   OverThreshold:   true if overflow occurred  measures by threshold. Cleared by read. [original inline comment from piHPSDR src/saturndrivers.c:241]
//   Underflowed:       true if underflow has occurred. Cleared by read. [original inline comment from piHPSDR src/saturndrivers.c:242]
//   Current:           number of locations occupied (in either FIFO type) [original inline comment from piHPSDR src/saturndrivers.c:243]
quint32 readFifoMonitorChannel(EDMAStreamSelect channel, bool *overflowed, bool *overThreshold, bool *underflowed,
                               unsigned int *current) {
    quint32 address;             // register address
    quint32 data = 0;            // register content
    bool overflow = false;
    bool overThresh = false;
    bool underflow = false;
    address = VADDRFIFOMONBASE + 4 * (quint32)channel;     // status register address
    data = regRead(address);

    if (data & 0x80000000) {                  // if top bit set, declare overflow
        overflow = true;
    }

    if (data & 0x40000000) {                  // if bit 30 set, declare over threshold
        overThresh = true;
    }

    if (data & 0x20000000) {                  // if bit 29 set, declare underflow
        underflow = true;
    }

    data = data & 0xFFFF;                   // strip to 16 bits
    *current = data;
    *overflowed = overflow;                 // send out overflow result
    *overThreshold = overThresh;            // send out over threshold result
    *underflowed = underflow;               // send out underflow result

    //
    // If it is a "write" channel, return number of free locations instead
    // of the currentfilling
    //
    if ((channel == eTXDUCDMA) || (channel == eSpkCodecDMA)) {
        data = dmaFifoDepths[channel] - data;
    }

    return data;                        // return 16 bit FIFO count
}

//
// reset a stream FIFO
//
void resetDmaStreamFifo(EDMAStreamSelect ddcNum) {
    quint32 data;                    // DDC register content
    quint32 dataBit = 0;
    static QMutex resetMutex;

    switch (ddcNum) {
    case eRXDDCDMA:             // selects RX
        dataBit = (1 << VBITDDCFIFORESET);
        break;

    case eTXDUCDMA:             // selects TX
        dataBit = (1 << VBITDUCFIFORESET);
        break;

    case eMicCodecDMA:            // selects mic samples
        dataBit = (1 << VBITCODECMICFIFORESET);
        break;

    case eSpkCodecDMA:            // selects speaker samples
        dataBit = (1 << VBITCODECSPKFIFORESET);
        break;
    }

    // The Mutex takes care we get a clean "data pulse"
    QMutexLocker locker(&resetMutex);
    data = regRead(VADDRFIFORESET);        // read current content
    data = data & ~dataBit;
    regWrite(VADDRFIFORESET, data);        // set reset bit to zero
    data = data | dataBit;
    regWrite(VADDRFIFORESET, data);        // set reset bit to 1
}

//
// number of samples to read for each DDC setting
// these settings must match behaviour of the FPGA IP!
// a value of "7" indicates an interleaved DDC
// and the rate value is stored for *next* DDC
//
static const quint32 ddcSampleCounts[] = {
    0,            // set to zero so no samples transferred
    1,
    2,
    4,
    8,
    16,
    32,
    0           // when set to 7, use next value & double it
};

//
// quint32 analyseDdcHeader(quint32 header, quint32* ddcCounts)
// parameters are the header read from the DDC stream, and
// a pointer to an array [DDC count] of ints
// the array of ints is populated with the number of samples to read for each DDC
// returns the number of words per frame, which helps set the DMA transfer size
// This limits the total number of DDCs to 10, since each DDC is described
// by 3 bits in the Header word.
//
// uint32_t AnalyseDDCHeader(unit32_t Header, unit32_t* DDCCounts) [original inline comment from piHPSDR src/saturndrivers.c:327]
quint32 analyseDdcHeader(quint32 header, quint32 *ddcCounts) {
    quint32 ddc;               // DDC counter
    quint32 count;
    quint32 total = 0;

    for (ddc = 0; ddc < VNUMDDC; ddc++) {
        // 3 bit value for this DDC
        quint32 rate = header & 7;

        if (rate != 7) {
            count = ddcSampleCounts[rate];
            ddcCounts[ddc] = count;
            total += count;
        } else {
            // This and the next DDC channel form a "synchronised pair"
            // which share the sample rate and where all samples are
            // delivered pair-wise in the DDC stream of the first member
            // of the pair.
            header = header >> 3;
            rate = header & 7;
            count = 2 * ddcSampleCounts[rate];
            ddcCounts[ddc] = count;   // This one gets all the samples for the pair
            total += count;
            ddcCounts[ddc + 1] = 0;   // This one gets no samples
            ddc += 1;
        }

        header = header >> 3;         // ready for next DDC rate
    }

    return total;
}

////////////////////////////////////////////////////////////////////////////////////
//
// initialise the DAC Atten ROMs.
// The "drive level" of the HPSDR protocol has a voltage
// amplitude in mind, with values 0-255.
//
// The Saturn hardware controls its drive output through
// a step attenuator (0...63 --> 0.0...31.5 dB in  0.5 dB steps)
// and a fine-tuning through a "DACdrive" which is an amplitude
// 0..255.
// This way, most of the control goes to the Attenuator, while
// the DACdrive assumes values between 240 and 255, doing the
// interpolation between two adjacent 0.5-db-steps.
// Some data from the table, with a TX power that is 100 Watts
// at full scale:
//
// Level     Step   DACdrive  Watt
// --------------------------------
//    0       63        0       0
//   26       63      245       1
//   57       26      254       5
//   81       19      241      10
//  128       11      241      25
//  180       26      254      50
//  221        2      247      75
//  255        0      255     100
////////////////////////////////////////////////////////////////////////////////////

// ALEX SPI registers [original inline comment from piHPSDR src/saturnregisters.c:146]
// offset addr in IP core: TX filt, RX ant [original inline comment from piHPSDR src/saturnregisters.c:148]
// offset addr in IP core [original inline comment from piHPSDR src/saturnregisters.c:149]
// offset addr in IP core: TX filt, TX ant [original inline comment from piHPSDR src/saturnregisters.c:150]
// GPIO output bits [original inline comment from piHPSDR src/saturnregisters.c:153]
// bits 16-22 [original inline comment from piHPSDR src/saturnregisters.c:167]
// not used by this hardware [original inline comment from piHPSDR src/saturnregisters.c:172]
// GPIO input buts [original inline comment from piHPSDR src/saturnregisters.c:177]
// dot key [original inline comment from piHPSDR src/saturnregisters.c:179]
// dash key [original inline comment from piHPSDR src/saturnregisters.c:180]
// keyer output [original inline comment from piHPSDR src/saturnregisters.c:188]
// keyer request for TX active [original inline comment from piHPSDR src/saturnregisters.c:189]
// Keyer setup register defines [original inline comment from piHPSDR src/saturnregisters.c:192]
// enable bit [original inline comment from piHPSDR src/saturnregisters.c:194]
// delay bits 7:0 [original inline comment from piHPSDR src/saturnregisters.c:195]
// hang time is 17:8 [original inline comment from piHPSDR src/saturnregisters.c:196]
// ramp time [original inline comment from piHPSDR src/saturnregisters.c:197]
// max ramp length in words [original inline comment from piHPSDR src/saturnregisters.c:198]
// Iambic config register defines [original inline comment from piHPSDR src/saturnregisters.c:201]
// speed bits 7:0 [original inline comment from piHPSDR src/saturnregisters.c:203]
// weight bits 15:8 [original inline comment from piHPSDR src/saturnregisters.c:204]
// keys reversed bit 16 [original inline comment from piHPSDR src/saturnregisters.c:205]
// keyer enabled bit 17 [original inline comment from piHPSDR src/saturnregisters.c:206]
// mode bit 18 [original inline comment from piHPSDR src/saturnregisters.c:207]
// strict spacing bit 19 [original inline comment from piHPSDR src/saturnregisters.c:208]
// CWX enable bit 20 [original inline comment from piHPSDR src/saturnregisters.c:209]
// CWX dox bit 21 [original inline comment from piHPSDR src/saturnregisters.c:210]
// CWX dash bit 22 [original inline comment from piHPSDR src/saturnregisters.c:211]
// breakin bit (CW not Iambic strictly!) [original inline comment from piHPSDR src/saturnregisters.c:212]
// all CWX bits [original inline comment from piHPSDR src/saturnregisters.c:213]
// all non CWX bits [original inline comment from piHPSDR src/saturnregisters.c:214]
// TX config register defines [original inline comment from piHPSDR src/saturnregisters.c:217]
// do the max atten values separately; then calculate point by point [original inline comment from piHPSDR src/saturnregisters.c:260]
// this is the atten value we want after the high speed DAC [original inline comment from piHPSDR src/saturnregisters.c:265]
// convert to integer and clip to 6 bits [original inline comment from piHPSDR src/saturnregisters.c:267]
// atten to go in the current setting DAC [original inline comment from piHPSDR src/saturnregisters.c:270]
// this needs to be achieved through the current setting drive [original inline comment from piHPSDR src/saturnregisters.c:271]
// convert to integer [original inline comment from piHPSDR src/saturnregisters.c:273]
// write data pair to the ROM [original inline comment from piHPSDR src/saturnregisters.c:275]
void initialiseDacAttenRoms(void) {
    // min level [original inline comment from piHPSDR src/saturnregisters.c:262]
    dacCurrentRom[0] = 0;
    // max atten [original inline comment from piHPSDR src/saturnregisters.c:263]
    dacStepAttenRom[0] = 63;

    for (unsigned int level = 1; level < 256; level++) {
        double desiredAtten = 20.0 * log10(255.0 / (double)level);
        unsigned int stepValue = (int)(2.0 * desiredAtten);

        if (stepValue > 63) { stepValue = 63; }

        double residualAtten = desiredAtten - ((double)stepValue * 0.5);
        unsigned int dacDrive = (unsigned int)(255.0 / pow(10.0, (residualAtten / 20.0)));
        dacCurrentRom[level] = dacDrive;
        dacStepAttenRom[level] = stepValue;
    }
}

//
// SetByteSwapping(bool)
// set whether byte swapping is enabled. True if yes, to get data in network byte order.
//
void setByteSwapping(bool isSwapped) {
    quint32 reg;
    QMutexLocker locker(&gpioMutex);
    reg = gpioRegValue;

    if (isSwapped) {
        // set bit for swapped to network order [original inline comment from piHPSDR src/saturnregisters.c:290]
        reg |= (1 << VDATAENDIAN);
    } else {
        // clear bit for raspberry pi local order [original inline comment from piHPSDR src/saturnregisters.c:292]
        reg &= ~(1 << VDATAENDIAN);
    }

    if (reg != gpioRegValue) {
        gpioRegValue = reg;
        regWrite(VADDRRFGPIOREG, reg);
    }
}

//
// internal function to set the keyer on or off
// needed because keyer setting can change by message, of by TX operation
//
static void activateCwKeyer(bool keyer) {
    QMutexLocker locker(&keyerMutex);
    quint32 reg = gCwKeyerSetup;

    if (keyer) {
        reg |= (1U << VCWKEYERENABLE);
    } else {
        reg &= ~(1U << VCWKEYERENABLE);
    }

    if (reg != gCwKeyerSetup) {
        gCwKeyerSetup = reg;
        regWrite(VADDRKEYERCONFIGREG, reg);
    }
}

//
// SetMOX(bool Mox)
// sets or clears TX state
// set or clear the relevant bit in GPIO
// and enable keyer if CW
//
// now set CW keyer if required [original inline comment from piHPSDR src/saturnregisters.c:341]
void setMox(bool mox) {
    quint32 reg;
    QMutexLocker locker(&gpioMutex);
    reg = gpioRegValue;

    if (mox) {
        reg |= (1 << VMOXBIT);
    } else {
        reg &= ~(1 << VMOXBIT);
    }

    if (reg != gpioRegValue) {
        gpioRegValue = reg;
        regWrite(VADDRRFGPIOREG, reg);
    }

    locker.unlock();

    if (mox) {
        activateCwKeyer(gCwEnabled);
    // disable keyer unless CW & breakin [original inline comment from piHPSDR src/saturnregisters.c:345]
    } else {
        activateCwKeyer(gCwEnabled && gBreakinEnabled);
    }
}

//
// SetTXEnable(bool Enabled)
// sets or clears TX enable bit
// set or clear the relevant bit in GPIO
//
void setTxEnable(bool enabled) {
    quint32 reg;
    QMutexLocker locker(&gpioMutex);
    reg = gpioRegValue;

    if (enabled) {
        reg |= (1 << VTXENABLEBIT);
    } else {
        reg &= ~(1 << VTXENABLEBIT);
    }

    if (reg != gpioRegValue) {
        gpioRegValue = reg;
        regWrite(VADDRRFGPIOREG, reg);
    }
}

// Although we do not (yet) write DDCRateReg to the FPGA, we need [original inline comment from piHPSDR src/saturnregisters.c:377]
// mutex here to protect the  read-modify-write cycles for GDDCEnabled and DDCRateReg [original inline comment from piHPSDR src/saturnregisters.c:378]
// look up enum value [original inline comment from piHPSDR src/saturnregisters.c:389]
void setP2SampleRate(unsigned int ddc, bool enabled, unsigned int sampleRate, bool interleaveWithNext) {
    quint32 registerValue;
    quint32 mask;
    ESampleRate rate;
    // 3 bits in correct position [original inline comment from piHPSDR src/saturnregisters.c:375]
    mask = 7 << (ddc * 3);

    QMutexLocker locker(&ddcRateMutex);

    // if not enabled, clear sample rate value & enabled flag [original inline comment from piHPSDR src/saturnregisters.c:381]
    if (!enabled) {
        // clear enable bit [original inline comment from piHPSDR src/saturnregisters.c:382]
        gDdcEnabled &= ~(1 << ddc);
        rate = eDisabled;
    } else {
        // set enable bit [original inline comment from piHPSDR src/saturnregisters.c:385]
        gDdcEnabled |= (1 << ddc);

        if (interleaveWithNext) {
            rate = eInterleaveWithNext;
        } else {
            // assume 48KHz; then check other rates [original inline comment from piHPSDR src/saturnregisters.c:390]
            rate = e48KHz;

            if (sampleRate == 96) {
                rate = e96KHz;
            } else if (sampleRate == 192) {
                rate = e192KHz;
            } else if (sampleRate == 384) {
                rate = e384KHz;
            } else if (sampleRate == 768) {
                rate = e768KHz;
            } else if (sampleRate == 1536) {
                rate = e1536KHz;
            }
        }
    }

    // get current register setting [original inline comment from piHPSDR src/saturnregisters.c:404]
    registerValue = ddcRateReg;
    // strip current bits [original inline comment from piHPSDR src/saturnregisters.c:405]
    registerValue &= ~mask;
    // new bits [original inline comment from piHPSDR src/saturnregisters.c:406]
    mask = (quint32)rate;
    // get new bits to right bit position [original inline comment from piHPSDR src/saturnregisters.c:407]
    mask = mask << (ddc * 3);
    registerValue |= mask;
    // don't save to hardware [original inline comment from piHPSDR src/saturnregisters.c:409]
    ddcRateReg = registerValue;
}

//
// void WriteP2DDCRateRegister(void)
// writes the DDCRateRegister, once all settings have been made
// this is done so the number of changes to the DDC rates are minimised
// and the information all comes form one P2 message anyway.
//
void writeP2DdcRateRegister(void) {
    static quint32 oldValue = 0;
    QMutexLocker locker(&ddcRateMutex);

    if (ddcRateReg != oldValue) {
        oldValue = ddcRateReg;
        regWrite(VADDRDDCRATES, ddcRateReg);
    }
}

//
// SetOpenCollectorOutputs(unsigned int bits)
// sets the 7 open collector output bits
// data must be provided in bits 6:0
//
void setOpenCollectorOutputs(unsigned int bits) {
    // FPGA register content [original inline comment from piHPSDR src/saturnregisters.c:435]
    quint32 reg;
    // bitmask for 7 OC bits [original inline comment from piHPSDR src/saturnregisters.c:436]
    quint32 bitMask;
    QMutexLocker locker(&gpioMutex);
    // get current settings [original inline comment from piHPSDR src/saturnregisters.c:438]
    reg = gpioRegValue;
    bitMask = (0b1111111) << VOPENCOLLECTORBITS;
    // strip old bits, add new [original inline comment from piHPSDR src/saturnregisters.c:440]
    reg = reg & ~bitMask;
    // OC bits are in bits (6:0) [original inline comment from piHPSDR src/saturnregisters.c:441]
    reg |= (bits << VOPENCOLLECTORBITS);

    if (reg != gpioRegValue) {
        // store it back [original inline comment from piHPSDR src/saturnregisters.c:443]
        gpioRegValue = reg;
        // and write to it [original inline comment from piHPSDR src/saturnregisters.c:444]
        regWrite(VADDRRFGPIOREG, reg);
    }
}

//
// SetADCOptions(bool PGA1, bool Dither1, bool Random1, bool PGA2, bool Dither2, bool Random2);
// sets the ADC contol bits for both ADCs
//
void setAdcOptions(bool pga1, bool dither1, bool random1, bool pga2, bool dither2, bool random2) {
    // FPGA register content [original inline comment from piHPSDR src/saturnregisters.c:454]
    quint32 reg;
    QMutexLocker locker(&gpioMutex);
    // get current settings [original inline comment from piHPSDR src/saturnregisters.c:456]
    reg = gpioRegValue;
    // strip old bits [original inline comment from piHPSDR src/saturnregisters.c:457]
    reg &= ~(1 << VADC1RANDBIT);
    reg &= ~(1 << VADC1PGABIT);
    reg &= ~(1 << VADC1DITHERBIT);
    // strip old bits [original inline comment from piHPSDR src/saturnregisters.c:460]
    reg &= ~(1 << VADC2RANDBIT);
    reg &= ~(1 << VADC2PGABIT);
    reg &= ~(1 << VADC2DITHERBIT);

    if (pga1)    { reg |= (1 << VADC1PGABIT); }
    if (pga2)    { reg |= (1 << VADC2PGABIT); }
    if (dither1) { reg |= (1 << VADC1DITHERBIT); }
    if (dither2) { reg |= (1 << VADC2DITHERBIT); }
    if (random1) { reg |= (1 << VADC1RANDBIT); }
    if (random2) { reg |= (1 << VADC2RANDBIT); }

    if (reg != gpioRegValue) {
        // store it back [original inline comment from piHPSDR src/saturnregisters.c:470]
        gpioRegValue = reg;
        // and write to it [original inline comment from piHPSDR src/saturnregisters.c:471]
        regWrite(VADDRRFGPIOREG, reg);
    }
}

//
// SetDDCFrequency(uint32_t DDC, uint32_t Value, bool IsDeltaPhase)
// sets a DDC frequency.
// DDC: DDC number (0-9)
// Value: 32 bit phase word or frequency word (1Hz resolution)
// IsDeltaPhase: true if a delta phase value, false if a frequency value (P1)
// calculate delta phase if required. Delta=2^32 * (F/Fs)
// store delta phase; write to FPGA register.
//
// We never arrive here, since this conversion is done in new_protocol.c [original inline comment from piHPSDR src/saturnregisters.c:492]
// the "obscure" constant 34.95233...  is 2^32 / 12288000 (sample rate) [original inline comment from piHPSDR src/saturnregisters.c:493]
// only write back if changed [original inline comment from piHPSDR src/saturnregisters.c:500]
void setDdcFrequency(uint32_t ddc, uint32_t value, bool isDeltaPhase) {
    // calculated deltaphase value [original inline comment from piHPSDR src/saturnregisters.c:486]
    quint32 deltaPhase;

    // limit the DDC count to actual regs! [original inline comment from piHPSDR src/saturnregisters.c:487]
    if (ddc >= VNUMDDC) {
        ddc = VNUMDDC - 1;
    }

    if (!isDeltaPhase) {
        deltaPhase = (quint32)((double)value * 34.952533333333333333333333333333);
    } else {
        deltaPhase = value;
    }

    QMutexLocker locker(&ddcRegMutex);

    if (ddcDeltaPhase[ddc] != deltaPhase) {
        ddcDeltaPhase[ddc] = deltaPhase;
        quint32 regAddress = ddcRegisters[ddc];
        regWrite(regAddress, deltaPhase);
    }
}

#define DELTAPHIHPFCUTIN 1712674133L

//
// SetDUCFrequency(unsigned int Value, bool IsDeltaPhase)
// sets a DUC frequency. (Currently only 1 DUC, therefore DUC must be 0)
// Value: 32 bit phase word or frequency word (1Hz resolution)
// IsDeltaPhase: true if a delta phase value, false if a frequency value (P1)
//
// delta phi for 49MHz [original inline comment from piHPSDR src/saturnregisters.c:511]
// only accepts DUC=0 [original inline comment from piHPSDR src/saturnregisters.c:518]
// We never arrive here, since this conversion is done in new_protocol.c [original inline comment from piHPSDR src/saturnregisters.c:522]
// the "obscure" constant 34.95233...  is 2^32 / 12288000 (sample rate) [original inline comment from piHPSDR src/saturnregisters.c:523]
// PCB V3+: now enable high pass filter if above 49MHz [original inline comment from piHPSDR src/saturnregisters.c:536]
void setDucFrequency(unsigned int value, bool isDeltaPhase, RadioInfo rinfo) {
    // calculated deltaphase value [original inline comment from piHPSDR src/saturnregisters.c:519]
    quint32 deltaPhase;
    RadioInfo info = rinfo;

    if (!isDeltaPhase) {
        deltaPhase = (quint32)((double)value * 34.952533333333333333333333333333);
    } else {
        deltaPhase = (quint32)value;
    }

    {
        QMutexLocker locker(&ducRegMutex);

        if (deltaPhase != ducDeltaPhase) {
            // store this delta phase [original inline comment from piHPSDR src/saturnregisters.c:531]
            ducDeltaPhase = deltaPhase;
            // and write to it [original inline comment from piHPSDR src/saturnregisters.c:532]
            regWrite(VADDRTXDUCREG, deltaPhase);
        }
    }

    if (info.pcbVersion >= 3) {
        bool needsHpf = false;

        if (deltaPhase > DELTAPHIHPFCUTIN) { needsHpf = true; }

        QMutexLocker locker(&txConfigMutex);
        // get current settings [original inline comment from piHPSDR src/saturnregisters.c:542]
        quint32 reg = txConfigRegValue;
        // remove old HPF bit [original inline comment from piHPSDR src/saturnregisters.c:543]
        reg &= ~(1 << VTXCONFIGHPFENABLE);

        // add new bit if HPF to be enabled [original inline comment from piHPSDR src/saturnregisters.c:544]
        if (needsHpf) { reg |= (1 << VTXCONFIGHPFENABLE); }

        if (reg != txConfigRegValue) {
            // store it back [original inline comment from piHPSDR src/saturnregisters.c:546]
            txConfigRegValue = reg;
            // and write to it [original inline comment from piHPSDR src/saturnregisters.c:547]
            regWrite(VADDRTXCONFIGREG, reg);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////
//
// Alex layout is relevant for P1 only. P2 does this is new-protocol.c
// We keep the list here just for information
//
//////////////////////////////////////////////////////////////////////////////////
//  data to send to Alex Tx filters is in the following format:
//  Bit  0 - NC               U3 - D0       0
//  Bit  1 - NC               U3 - D1       0
//  Bit  2 - txrx_status      U3 - D2       TXRX_Relay strobe
//  Bit  3 - Yellow Led       U3 - D3       RX2_GROUND: from C0=0x24: C1[7]
//  Bit  4 - 30/20m LPF       U3 - D4       LPF[0] : from C0=0x12: C4[0]
//  Bit  5 - 60/40m LPF       U3 - D5       LPF[1] : from C0=0x12: C4[1]
//  Bit  6 - 80m LPF          U3 - D6       LPF[2] : from C0=0x12: C4[2]
//  Bit  7 - 160m LPF         U3 - D7       LPF[3] : from C0=0x12: C4[3]
//  Bit  8 - Ant #1           U5 - D0       Gate from C0=0:C4[1:0]=00
//  Bit  9 - Ant #2           U5 - D1       Gate from C0=0:C4[1:0]=01
//  Bit 10 - Ant #3           U5 - D2       Gate from C0=0:C4[1:0]=10
//  Bit 11 - T/R relay        U5 - D3       T/R relay. 1=TX TXRX_Relay strobe
//  Bit 12 - Red Led          U5 - D4       TXRX_Relay strobe
//  Bit 13 - 6m LPF           U5 - D5       LPF[4] : from C0=0x12: C4[4]
//  Bit 14 - 12/10m LPF       U5 - D6       LPF[5] : from C0=0x12: C4[5]
//  Bit 15 - 17/15m LPF       U5 - D7       LPF[6] : from C0=0x12: C4[6]
//
// bit 4 (or bit 11 as sent by AXI) replaced by TX strobe
//
//  data to send to Alex Rx filters is in the folowing format:
//  bits 15:0 - RX1; bits 31:16 - RX1
// (IC designators and functions for 7000DLE RF board)
//
//  Bit  0 - Yellow LED       U6 - QA       0
//  Bit  1 - 10-22 MHz BPF    U6 - QB       BPF[0]: from C0=0x12: C3[0]
//  Bit  2 - 22-35 MHz BPF    U6 - QC       BPF[1]: from C0=0x12: C3[1]
//  Bit  3 - 6M Preamp        U6 - QD       10/6M LNA: from C0=0x12: C3[6]
//  Bit  4 - 6-10MHz BPF      U6 - QE       BPF[2]: from C0=0x12: C3[2]
//  Bit  5 - 2.5-6 MHz BPF    U6 - QF       BPF[3]: from C0=0x12: C3[3]
//  Bit  6 - 1-2.5 MHz BPF    U6 - QG       BPF[4]: from C0=0x12: C3[4]
//  Bit  7 - N/A              U6 - QH       0
//  Bit  8 - Transverter      U10 - QA      Gated C122_Transverter. True if C0=0: C3[6:5]=11
//  Bit  9 - Ext1 In          U10 - QB      Gated C122_Rx_2_in. True if C0=0: C3[6:5]=10
//  Bit 10 - N/A              U10 - QC      0
//  Bit 11 - PS sample select U10 - QD      Selects main or RX_BYPASS_OUT Gated C122_Rx_1_in True if C0=0: C3[6:5]=01
//  Bit 12 - RX1 Filt bypass  U10 - QE      BPF[5]: from C0=0x12: C3[5]
//  Bit 13 - N/A              U10 - QF      0
//  Bit 14 - RX1 master in    U10 - QG      (selects main, or transverter/ext1) Gated. True if C0=0: C3[6:5]=11 or C0=0: C3[6:5]=10
//  Bit 15 - RED LED          U10 - QH      0
//  Bit 16 - Yellow LED       U7 - QA       0
//  Bit 17 - 10-22 MHz BPF    U7 - QB       BPF2[0]: from C0=0x24: C1[0]
//  Bit 18 - 22-35 MHz BPF    U7 - QC       BPF2[1]: from C0=0x24: C1[1]
//  Bit 19 - 6M Preamp        U7 - QD       10/6M LNA2: from C0=0x24: C1[6]
//  Bit 20 - 6-10MHz BPF      U7 - QE       BPF2[2]: from C0=0x24: C1[2]
//  Bit 21 - 2.5-6 MHz BPF    U7 - QF       BPF2[3]: from C0=0x24: C1[3]
//  Bit 22 - 1-2.5 MHz BPF    U7 - QG       BPF2[4]: from C0=0x24: C1[4]
//  Bit 23 - N/A              U7 - QH       0
//  Bit 24 - RX2_GROUND       U13 - QA      RX2_GROUND: from C0=0x24: C1[7]
//  Bit 25 - N/A              U13 - QB      0
//  Bit 26 - N/A              U13 - QC      0
//  Bit 27 - N/A              U13 - QD      0
//  Bit 28 - HPF_BYPASS 2     U13 - QE      BPF2[5]: from C0=0x24: C1[5]
//  Bit 29 - N/A              U13 - QF      0
//  Bit 30 - N/A              U13 - QG      0
//  Bit 31 - RED LED 2        U13 - QH      0
//
//
//////////////////////////////////////////////////////////////////////////////////

//
// AlexManualRXFilters(unsigned int Bits, int RX)
// P2: provides a 16 bit word with all of the Alex settings for a single RX
// must be formatted according to the Alex specification
// RX=0 or 1: RX1; RX=2: RX2
//
void alexManualRxFilters(unsigned int bits, int rx) {
    QMutexLocker locker(&alexRxMutex);
    // copy original register [original inline comment from piHPSDR src/saturnregisters.c:627]
    quint32 reg = gAlexRxRegister;

    if (rx != 2) {
        // turn off all affected bits [original inline comment from piHPSDR src/saturnregisters.c:629]
        reg &= 0xFFFF0000;
        // add back all new bits [original inline comment from piHPSDR src/saturnregisters.c:630]
        reg |= bits;
    } else {
        // turn off all affected bits [original inline comment from piHPSDR src/saturnregisters.c:632]
        reg &= 0x0000FFFF;
        // add back all new bits [original inline comment from piHPSDR src/saturnregisters.c:633]
        reg |= (bits << 16);
    }

    // write back if changed [original inline comment from piHPSDR src/saturnregisters.c:635]
    if (reg != gAlexRxRegister) {
        gAlexRxRegister = reg;
        // and write to it [original inline comment from piHPSDR src/saturnregisters.c:637]
        regWrite(VADDRALEXSPIREG + VOFFSETALEXRXREG, reg);
    }
}

//
// AlexManualTXFilters(unsigned int Bits)
// P2: provides a 16 bit word with all of the Alex settings for TX
// must be formatted according to the Alex specification
// FPGA V12 onwards: uses an additional register with TX ant settings
// HasTXAntExplicitly true if data is for the new TXfilter, TX ant register
//
void alexManualTxFilters(unsigned int bits, bool hasTxAntExplicitly) {
    // new setting [original inline comment from piHPSDR src/saturnregisters.c:650]
    quint32 reg = bits;
    QMutexLocker locker(&alexTxMutex);

    if (hasTxAntExplicitly && (reg != gAlexTxAntRegister)) {
        gAlexTxAntRegister = reg;
        // and write to it [original inline comment from piHPSDR src/saturnregisters.c:654]
        regWrite(VADDRALEXSPIREG + VOFFSETALEXTXANTREG, reg);
    } else if (!hasTxAntExplicitly && (reg != gAlexTxFiltRegister)) {
        gAlexTxFiltRegister = reg;
        // and write to it [original inline comment from piHPSDR src/saturnregisters.c:657]
        regWrite(VADDRALEXSPIREG + VOFFSETALEXTXFILTREG, reg);
    }
}

//
// SetTXDriveLevel(unsigned int Level)
// sets the TX DAC current via a PWM DAC output
// level: 0 to 255 drive level value (255 = max current)
// sets both step attenuator drive and PWM DAC drive for high speed DAC current,
// using ROMs calculated at initialise.
//
void setTxDriveLevel(unsigned int level) {
    quint32 registerValue = 0;
    quint32 dacDrive, attenDrive;
    // make sure 8 bits only [original inline comment from piHPSDR src/saturnregisters.c:672]
    level &= 0xFF;
    // get PWM [original inline comment from piHPSDR src/saturnregisters.c:673]
    dacDrive = dacCurrentRom[level];
    // get step atten [original inline comment from piHPSDR src/saturnregisters.c:674]
    attenDrive = dacStepAttenRom[level];
    // set drive level when RX [original inline comment from piHPSDR src/saturnregisters.c:675]
    registerValue = dacDrive;
    // set drive level when TX [original inline comment from piHPSDR src/saturnregisters.c:676]
    registerValue |= (dacDrive << 8);
    // set step atten when RX [original inline comment from piHPSDR src/saturnregisters.c:677]
    registerValue |= (attenDrive << 16);
    // set step atten when TX [original inline comment from piHPSDR src/saturnregisters.c:678]
    registerValue |= (attenDrive << 24);
    QMutexLocker locker(&txDriveMutex);

    if (registerValue != gTxDacCtrl) {
        gTxDacCtrl = registerValue;
        // and write to it [original inline comment from piHPSDR src/saturnregisters.c:682]
        regWrite(VADDRDACCTRLREG, registerValue);
    }
}

//
// EnableLine: true: enable Line input, false: enable Mic input
// MicBoost:   true: use 20dB mic boost, false: no boost
// LineInGain: LineIn gain vaule
//
// MicBoost has no effect if EnableLine is true
// LineInGain has no effect if MicLine is true
//
// Path = Register 4, Gain = Register 0 [original inline comment from piHPSDR src/saturnregisters.c:701]
// Path = Registers 52/55, Gain = Registers 59/60 [original inline comment from piHPSDR src/saturnregisters.c:721]
// Route LineIn, and set gain [original inline comment from piHPSDR src/saturnregisters.c:723]
// Route MicIn, set gain to 23 or 3 dB [original inline comment from piHPSDR src/saturnregisters.c:727]
// Select Page 1, update registers 52 and 55 [original inline comment from piHPSDR src/saturnregisters.c:732]
// Legal values for Gain at this place are 0 - 95, we use 0 - 93 [original inline comment from piHPSDR src/saturnregisters.c:738]
// Select Page 1, update registers 59 and 60 [original inline comment from piHPSDR src/saturnregisters.c:740]
void setCodecInputParams(bool enableLine, bool enableBoost, int lineInGain) {
    unsigned int path, gain;
    QMutexLocker locker(&codecMutex);

    switch (installedCodec) {
    case e23b:
        // Clear InSel, MicMute, MicBoost [original inline comment from piHPSDR src/saturnregisters.c:702]
        path = gCodecPath & 0xFFF8;
        gain = gCodecGain & 0xFFE0;

        if (enableLine) {
            // Line-In, Mic muted, no Boost [original inline comment from piHPSDR src/saturnregisters.c:705]
            path |= 0x02;
            // 5-bit value [original inline comment from piHPSDR src/saturnregisters.c:706]
            gain |= (lineInGain & 0x001F);
        } else {
            // Mic-In, Mic normal (unmuted) [original inline comment from piHPSDR src/saturnregisters.c:708]
            path |= 0x04;

            // Set MicBoost bit [original inline comment from piHPSDR src/saturnregisters.c:709]
            if (enableBoost) { path |= 0x0001; }
        }

        if (path != gCodecPath) {
            gCodecPath = path;
            codecRegisterWrite(4, path);
        }

        if (gain != gCodecGain) {
            gCodecGain = gain;
            codecRegisterWrite(0, gain);
        }

        break;

    case e3204:
        if (enableLine) {
            // IN1 routed to MICPGA with 40k resistance [original inline comment from piHPSDR src/saturnregisters.c:724]
            path = 0xC0;
            // in 0.5 dB steps, from 0 to 46.5 dB [original inline comment from piHPSDR src/saturnregisters.c:725]
            gain = 3 * (lineInGain & 0x001F);
        } else {
            // IN3 routed to PGA with 10k resistance [original inline comment from piHPSDR src/saturnregisters.c:728]
            path = 0x04;
            gain = enableBoost ? 46 : 6;
        }

        if (path != gCodecPath) {
            gCodecPath = path;
            codecRegisterWrite(0x00, 0x01);
            codecRegisterWrite(52, path);
            codecRegisterWrite(55, path);
        }

        if (gain != gCodecGain) {
            gCodecGain = gain;
            codecRegisterWrite(0x00, 0x01);
            codecRegisterWrite(59, gain);
            codecRegisterWrite(60, gain);
        }

        break;

    default:
        qCDebug(NereusSDR::lcProtocol) << "setCodecInputParams: Invalid Installed Codec";
        break;
    }
}

//
// SetOrionMicOptions(bool MicRing, bool EnableBias, bool EnablePTT)
// sets the microphone control inputs
// write the bits to GPIO. Note the register bits aren't directly the protocol input bits.
// note also that EnablePTT is actually a DISABLE signal (enabled = 0)
//
void setOrionMicOptions(bool micRing, bool enableBias, bool enablePtt) {
    // FPGA register content [original inline comment from piHPSDR src/saturnregisters.c:761]
    quint32 reg;
    QMutexLocker locker(&gpioMutex);
    // get current settings [original inline comment from piHPSDR src/saturnregisters.c:763]
    reg = gpioRegValue;
    // strip old bits [original inline comment from piHPSDR src/saturnregisters.c:764]
    reg &= ~(1 << VMICBIASENABLEBIT);
    reg &= ~(1 << VMICPTTSELECTBIT);
    reg &= ~(1 << VMICSIGNALSELECTBIT);
    reg &= ~(1 << VMICBIASSELECTBIT);

    // add new bits where set [original inline comment from piHPSDR src/saturnregisters.c:768]
    if (!micRing) {
        // mic on tip [original inline comment from piHPSDR src/saturnregisters.c:769]
        reg &= ~(1 << VMICSIGNALSELECTBIT);
        // and hence mic bias on tip [original inline comment from piHPSDR src/saturnregisters.c:770]
        reg |= (1 << VMICBIASSELECTBIT);
        // strip old bits [original inline comment from piHPSDR src/saturnregisters.c:765]
        // PTT on ring [original inline comment from piHPSDR src/saturnregisters.c:771]
        reg &= ~(1 << VMICPTTSELECTBIT);
    } else {
        // mic on ring [original inline comment from piHPSDR src/saturnregisters.c:773]
        reg |= (1 << VMICSIGNALSELECTBIT);
        // bias on ring [original inline comment from piHPSDR src/saturnregisters.c:774]
        reg &= ~(1 << VMICBIASSELECTBIT);
        // PTT on tip [original inline comment from piHPSDR src/saturnregisters.c:775]
        reg |= (1 << VMICPTTSELECTBIT);
    }

    if (enableBias) {
        reg |= (1 << VMICBIASENABLEBIT);
    }

    if (reg != gpioRegValue) {
        // store it back [original inline comment from piHPSDR src/saturnregisters.c:781]
        gpioRegValue = reg;
        // and write to it [original inline comment from piHPSDR src/saturnregisters.c:782]
        regWrite(VADDRRFGPIOREG, reg);
    }
}

//
// SetBalancedMicInput(bool Balanced)
// selects the balanced microphone input, not supported by current protocol code.
// just set the bit into GPIO
//
void setBalancedMicInput(bool balanced) {
    // FPGA register content [original inline comment from piHPSDR src/saturnregisters.c:793]
    quint32 reg;
    QMutexLocker locker(&gpioMutex);
    // get current settings [original inline comment from piHPSDR src/saturnregisters.c:795]
    reg = gpioRegValue;
    // strip old bit [original inline comment from piHPSDR src/saturnregisters.c:796]
    reg &= ~(1 << VBALANCEDMICSELECT);

    if (balanced) {
        // set new bit [original inline comment from piHPSDR src/saturnregisters.c:798]
        reg |= (1 << VBALANCEDMICSELECT);
    }

    if (reg != gpioRegValue) {
        // store it back [original inline comment from piHPSDR src/saturnregisters.c:801]
        gpioRegValue = reg;
        // and write to it [original inline comment from piHPSDR src/saturnregisters.c:802]
        regWrite(VADDRRFGPIOREG, reg);
    }
}

//
// SetADCAttenuator(unsigned int Atten1, bool RXAtten1, bool TXAtten1, unsigned int Atten2, bool RXAtten2, bool TXAtten2)
// sets the  stepped attenuator on the ADC input
// Atten provides a 5 bit atten value
// RXAtten: if true, sets atten to be used during RX
// TXAtten: if true, sets atten to be used during TX
//
// In P2, the RX attenuators are set from the HighPrio handler, while te
// TX attenuators are set from the DUCspecific handler.
//
void setAdcAttenuator(unsigned int atten1, bool rxAtten1, bool txAtten1, unsigned int atten2, bool rxAtten2,
                      bool txAtten2) {
    QMutexLocker locker(&attenMutex);
    // get existing settings [original inline comment from piHPSDR src/saturnregisters.c:820]
    quint32 reg = gRxAdcCtrl;
    const quint32 rxMask1 = 0b00000000000000011111;
    const quint32 txMask1 = 0b00000000001111100000;
    const quint32 rxMask2 = 0b00000111110000000000;
    const quint32 txMask2 = 0b11111000000000000000;

    if (rxAtten1) {
        reg &= ~rxMask1;
        reg |= (atten1 & 0x1F);
    }

    if (txAtten1) {
        reg &= ~txMask1;
        reg |= (atten1 & 0x1F) << 5;
    }

    if (rxAtten2) {
        reg &= ~rxMask2;
        reg |= (atten2 & 0x1F) << 10;
    }

    if (txAtten2) {
        reg &= ~txMask2;
        reg |= (atten2 & 0x1F) << 15;
    }

    if (reg != gRxAdcCtrl) {
        gRxAdcCtrl = reg;
        // and write to it [original inline comment from piHPSDR src/saturnregisters.c:843]
        regWrite(VADDRADCCTRLREG, reg);
    }
}

//
//void SetCWIambicKeyer(...)
// setup CW iambic keyer parameters
// Speed: keyer speed in WPM
// weight: typically 50
// ReverseKeys: swaps dot and dash
// mode: true if mode B
// strictSpacing: true if it enforces character spacing
// IambicEnabled: if false, reverts to straight CW key
//
// Need Mutex since this is called from DUC spec, but SetCWXBits from HighPrio [original inline comment from piHPSDR src/saturnregisters.c:862]
// set new data [original inline comment from piHPSDR src/saturnregisters.c:867]
void setCwIambicKeyer(uint8_t speed, uint8_t weight, bool reverseKeys, bool mode,
                      bool strictSpacing, bool iambicEnabled, bool breakin) {
    quint32 reg;
    QMutexLocker locker(&iambicMutex);
    // copy of H/W register [original inline comment from piHPSDR src/saturnregisters.c:865]
    reg = gIambicConfigReg;
    // strip off old iambic bits [original inline comment from piHPSDR src/saturnregisters.c:866]
    reg &= ~VIAMBICBITS;
    reg |= speed;
    reg |= (weight << VIAMBICWEIGHT);

    if (reverseKeys) { reg |= (1 << VIAMBICREVERSED); }
    if (mode) { reg |= (1 << VIAMBICMODE); }
    if (strictSpacing) { reg |= (1 << VIAMBICSTRICT); }
    if (iambicEnabled) { reg |= (1 << VIAMBICENABLE); }
    if (breakin) { reg |= (1 << VCWBREAKIN); }

    if (reg != gIambicConfigReg) {
        gIambicConfigReg = reg;
        regWrite(VADDRIAMBICCONFIG, reg);
    }
}

//
// SetDDCADC(int DDC, EADCSelect ADC)
// sets the ADC to be used for each DDC
// DDC = 0 to 9
//
void setDdcAdc(int ddc, EADCSelect adc) {
    quint32 reg;
    quint32 adcSetting;
    quint32 mask;
    // 2 bits with ADC setting [original inline comment from piHPSDR src/saturnregisters.c:891]
    adcSetting = ((quint32)adc & 0x3) << (ddc * 2);
    // 0,2,4,6,8,10,12,14,16,18 bit positions [original inline comment from piHPSDR src/saturnregisters.c:892]
    mask = 0x3 << (ddc * 2);
    // get protected access [original inline comment from piHPSDR src/saturnregisters.c:893]
    // get protected access [original inline comment from piHPSDR src/saturnregisters.c:901]
    QMutexLocker locker(&ddcInSelMutex);
    // get current register setting [original inline comment from piHPSDR src/saturnregisters.c:894]
    reg = ddcInSelReg;
    // strip ADC bits [original inline comment from piHPSDR src/saturnregisters.c:895]
    reg &= ~mask;
    reg |= adcSetting;

    if (reg != ddcInSelReg) {
        // write back [original inline comment from piHPSDR src/saturnregisters.c:898]
        ddcInSelReg = reg;
        // and write to it [original inline comment from piHPSDR src/saturnregisters.c:899]
        regWrite(VADDRDDCINSEL, reg);
    }
}

//
// void SetRXDDCEnabled(bool IsEnabled);
// sets enable bit so DDC operates normally. Resets input FIFO when starting.
//
void setRxDdcEnabled(bool isEnabled) {
    quint32 address;
    quint32 data;
    // DDC config register address [original inline comment from piHPSDR src/saturnregisters.c:911]
    address = VADDRDDCINSEL;
    // get protected access [original inline comment from piHPSDR src/saturnregisters.c:912]
    // get protected access [original inline comment from piHPSDR src/saturnregisters.c:923]
    QMutexLocker locker(&ddcInSelMutex);
    // get current register setting [original inline comment from piHPSDR src/saturnregisters.c:913]
    data = ddcInSelReg;

    if (isEnabled) {
        // set new bit [original inline comment from piHPSDR src/saturnregisters.c:915]
        data |= (1 << 30);
    } else {
        // clear new bit [original inline comment from piHPSDR src/saturnregisters.c:917]
        data &= ~(1 << 30);
    }

    if (data != ddcInSelReg) {
        // write back [original inline comment from piHPSDR src/saturnregisters.c:920]
        ddcInSelReg = data;
        // write back [original inline comment from piHPSDR src/saturnregisters.c:921]
        regWrite(address, data);
    }
}

#define VMINCWRAMPDURATION         5
#define VMAXCWRAMPDURATION        10
#define VMAXCWRAMPDURATIONV14PLUS 20

//
// InitialiseCWKeyerRamp(uint32_t Length)
// calculates an "S" shape ramp curve and loads into RAM
// needs to be called before keyer enabled!
// parameter is length in milliseconds; typically 9
// setup ramp memory and rampl length fields
// only calculate if parameters have changed!
//
// NOTE: only in-lined into SetKeyerParams() where it is protected
//       by the KeyerMutex
//
// 5ms min [original inline comment from piHPSDR src/saturnregisters.c:927]
// 10ms max [original inline comment from piHPSDR src/saturnregisters.c:928]
// 20ms max, for firmware V1.4 and later [original inline comment from piHPSDR src/saturnregisters.c:929]
// now apply that ramp length [original inline comment from piHPSDR src/saturnregisters.c:950]
// ======================================================================== [original inline comment from piHPSDR src/saturnregisters.c:952]
// Calculate a "DL1YCF" ramp [original inline comment from piHPSDR src/saturnregisters.c:954]
// The "black magic" in the coefficients comes from optimizing them [original inline comment from piHPSDR src/saturnregisters.c:957]
// against the spectral pollution of a string of dots, [original inline comment from piHPSDR src/saturnregisters.c:958]
// namely with ramp width 7 msec for CW speed  5 - 15 wpm [original inline comment from piHPSDR src/saturnregisters.c:959]
//        and  ramp width 8 msec for CW speed 16 - 32 wpm [original inline comment from piHPSDR src/saturnregisters.c:960]
//        and  ramp width 9 msec for CW speed 33 - 40 wpm [original inline comment from piHPSDR src/saturnregisters.c:961]
// such that the spectra meet ARRL's "clean signal initiative" requirement [original inline comment from piHPSDR src/saturnregisters.c:963]
// for the maximum peak strength at frequencies with a distance to the [original inline comment from piHPSDR src/saturnregisters.c:964]
// carrier that is larger than an offset, namely [original inline comment from piHPSDR src/saturnregisters.c:965]
//  -20 dBc for offsets >   90 Hz [original inline comment from piHPSDR src/saturnregisters.c:967]
//  -40 dBc for offsets >  150 Hz [original inline comment from piHPSDR src/saturnregisters.c:968]
//  -60 dBc for offsets >  338 Hz [original inline comment from piHPSDR src/saturnregisters.c:969]
// and is also meets the extended DL1YCF criteria which restrict spectral [original inline comment from piHPSDR src/saturnregisters.c:971]
// pollution at larger offsets, namely [original inline comment from piHPSDR src/saturnregisters.c:972]
//  -80 dBc for offsets >  600 Hz [original inline comment from piHPSDR src/saturnregisters.c:974]
// -100 dBc for offsets >  900 Hz [original inline comment from piHPSDR src/saturnregisters.c:975]
// -120 dBc for offsets > 1200 Hz [original inline comment from piHPSDR src/saturnregisters.c:976]
// ======================================================================== [original inline comment from piHPSDR src/saturnregisters.c:978]
static inline void initialiseCwKeyerRamp(uint8_t length, RadioInfo rinfo) {
    if (rinfo.firmwareVersion >= 14) {
        if (length > VMAXCWRAMPDURATIONV14PLUS) { length = VMAXCWRAMPDURATIONV14PLUS; }
    } else {
        if (length > VMAXCWRAMPDURATION) { length = VMAXCWRAMPDURATION; }
    }

    if (length < VMINCWRAMPDURATION) { length = VMINCWRAMPDURATION; }

    quint32 rampLength = length * 192;

    if (rampLength != gCwKeyerRampLength) {
        for (unsigned int cntr = 0; cntr < rampLength; cntr++) {
            quint32 sample;
            // between 0 and 1 [original inline comment from piHPSDR src/saturnregisters.c:981]
            double y = (double) cntr / (double) rampLength;
            //  2 Pi y [original inline comment from piHPSDR src/saturnregisters.c:982]
            double y2  = y * 6.2831853071795864769252867665590;
            //  4 Pi y [original inline comment from piHPSDR src/saturnregisters.c:983]
            double y4  = y2 + y2;
            //  6 Pi y [original inline comment from piHPSDR src/saturnregisters.c:984]
            double y6  = y4 + y2;
            //  8 Pi y [original inline comment from piHPSDR src/saturnregisters.c:985]
            double y8  = y4 + y4;
            // 10 Pi y [original inline comment from piHPSDR src/saturnregisters.c:986]
            double y10 = y4 + y6;
            double rampsample = y - 0.12182865361171612    * sin(y2)
                                - 0.018557469249199286   * sin(y4)
                                - 0.0009378783245428506  * sin(y6)
                                + 0.0008567571519403228  * sin(y8)
                                + 0.00018706912431472442 * sin(y10);
            sample = (quint32) (rampsample * 8388607.0);
            regWrite(VADDRCWKEYERRAM + 4 * cntr, sample);
        }

        // fill remainder of RAM [original inline comment from piHPSDR src/saturnregisters.c:995]
        for (unsigned int cntr = rampLength; cntr < VRAMPSIZE; cntr++) {
            regWrite(VADDRCWKEYERRAM + 4 * cntr, 8388607);
        }

        gCwKeyerRampLength = rampLength;
    }
}

//
// SetTXModulationSource(ETXModulationSource Source)
// selects the modulation source for the TX chain.
// this will need to be called operationally to change over between CW & I/Q
//
static inline void setTxModulationSource(ETXModulationSource source) {
    quint32 reg;
    QMutexLocker locker(&txConfigMutex);
    // get current settings [original inline comment from piHPSDR src/saturnregisters.c:1010]
    reg = txConfigRegValue;
    // remove old bits [original inline comment from piHPSDR src/saturnregisters.c:1011]
    reg &= 0xFFFFFFFC;
    // add new bits [original inline comment from piHPSDR src/saturnregisters.c:1012]
    reg |= ((unsigned int)source);

    if (reg != txConfigRegValue) {
        // store it back [original inline comment from piHPSDR src/saturnregisters.c:1014]
        txConfigRegValue = reg;
        // and write to it [original inline comment from piHPSDR src/saturnregisters.c:1015]
        regWrite(VADDRTXCONFIGREG, reg);
    }
}

//
// EnableCW (bool Enabled, bool Breakin)
// enables or disables CW mode; selects CW as modulation source.
// If Breakin enabled, the key input engages TX automatically
// and generates sidetone.
//
// set I/Q modulation source if CW selected [original inline comment from piHPSDR src/saturnregisters.c:1028]
// now set keyer enable if CW and break-in [original inline comment from piHPSDR src/saturnregisters.c:1036]
void enableCw (bool enabled, bool breakin) {
    gCwEnabled = enabled;

    if (enabled) {
        // CW source [original inline comment from piHPSDR src/saturnregisters.c:1032]
        setTxModulationSource(eCWKeyer);
    } else {
        // else IQ source [original inline comment from piHPSDR src/saturnregisters.c:1034]
        setTxModulationSource(eIQData);
    }

    gBreakinEnabled = breakin;
    activateCwKeyer(gBreakinEnabled && gCwEnabled);
}

// This is for 48000 kHz [original inline comment from piHPSDR src/saturnregisters.c:1043]
void setCwSideTone(bool enabled, uint8_t volume, uint16_t frequency) {
    quint32 reg;
    reg = (512 * frequency) / 375;

    if (enabled) {
        // add back new bits; resize to 16 bits [original inline comment from piHPSDR src/saturnregisters.c:1046]
        reg |= (volume & 0xFF) << 24;
    }

    QMutexLocker locker(&sideToneMutex);

    if (reg != gSideToneReg) {
        // store it back [original inline comment from piHPSDR src/saturnregisters.c:1050]
        gSideToneReg = reg;
        // and write to it [original inline comment from piHPSDR src/saturnregisters.c:1051]
        regWrite(VADDRSIDETONECONFIGREG, reg);
    }
}

void setKeyerParams(uint8_t delay, uint16_t hangTime, uint8_t ramp, RadioInfo rinfo) {
    quint32 reg;
    QMutexLocker locker(&keyerMutex);
    // get current settings [original inline comment from piHPSDR src/saturnregisters.c:1060]
    reg = gCwKeyerSetup;
    // remove old bits [original inline comment from piHPSDR src/saturnregisters.c:1061]
    reg &= 0xFFFC0000;
    reg |= (delay & 0xFF);
    reg |= (hangTime & 0x3FF) << VCWKEYERHANG;

    if (ramp > 0) {
        initialiseCwKeyerRamp(ramp, rinfo);
        // strip out ramp bits [original inline comment from piHPSDR src/saturnregisters.c:1066]
        reg &= 0x8003FFFF;

        if (rinfo.firmwareVersion >= 14) {
            // word end address [original inline comment from piHPSDR src/saturnregisters.c:1068]
            reg |= (gCwKeyerRampLength << VCWKEYERRAMP);
        } else {
            // byte end address [original inline comment from piHPSDR src/saturnregisters.c:1070]
            reg |= ((gCwKeyerRampLength << 2) << VCWKEYERRAMP);
        }
    }

    // write back if different [original inline comment from piHPSDR src/saturnregisters.c:1073]
    if (reg != gCwKeyerSetup) {
        // store it back [original inline comment from piHPSDR src/saturnregisters.c:1074]
        gCwKeyerSetup = reg;
        // and write to it [original inline comment from piHPSDR src/saturnregisters.c:1075]
        regWrite(VADDRKEYERCONFIGREG, reg);
    }
}

//
// SetXvtrEnable(bool Enabled)
// enables or disables transverter. If enabled, the PA is not keyed.
//
void setXvtrEnable(bool enabled) {
    quint32 reg;
    QMutexLocker locker(&gpioMutex);
    // get current settings [original inline comment from piHPSDR src/saturnregisters.c:1087]
    reg = gpioRegValue;

    if (enabled) {
        reg |= (1 << VXVTRENABLEBIT);
    } else {
        reg &= ~(1 << VXVTRENABLEBIT);
    }

    if (reg != gpioRegValue) {
        // store it back [original inline comment from piHPSDR src/saturnregisters.c:1094]
        gpioRegValue = reg;
        // and write to it [original inline comment from piHPSDR src/saturnregisters.c:1095]
        regWrite(VADDRRFGPIOREG, reg);
    }
}

//
// SetPAEnabled(bool Enabled)
// true if PA is enabled.
//
void setPaEnabled(bool enabled) {
    quint32 reg;
    QMutexLocker locker(&gpioMutex);
    // get current settings [original inline comment from piHPSDR src/saturnregisters.c:1107]
    reg = gpioRegValue;

    if (!enabled) {
        reg |= (1 << VTXRELAYDISABLEBIT);
    } else {
        reg &= ~(1 << VTXRELAYDISABLEBIT);
    }

    if (reg != gpioRegValue) {
        // store it back [original inline comment from piHPSDR src/saturnregisters.c:1114]
        gpioRegValue = reg;
        // and write to it [original inline comment from piHPSDR src/saturnregisters.c:1115]
        regWrite(VADDRRFGPIOREG, reg);
    }
}

//
// SetSpkrMute(bool IsMuted)
// enables or disables the Codec speaker output
//
void setSpkrMute(bool isMuted) {
    quint32 reg;
    QMutexLocker locker(&gpioMutex);
    // get current settings [original inline comment from piHPSDR src/saturnregisters.c:1127]
    reg = gpioRegValue;

    if (isMuted) {
        reg |= (1 << VSPKRMUTEBIT);
    } else {
        reg &= ~(1 << VSPKRMUTEBIT);
    }

    if (reg != gpioRegValue) {
        // store it back [original inline comment from piHPSDR src/saturnregisters.c:1134]
        gpioRegValue = reg;
        // and write to it [original inline comment from piHPSDR src/saturnregisters.c:1135]
        regWrite(VADDRRFGPIOREG, reg);
    }
}

//
// ReadStatusRegister(void)
// this is a precursor to getting any of the data itself; simply reads the register to a local variable
// probably call every time an outgoig packet is put together initially
// but possibly do this one a timed basis.
//
void readStatusRegister(void) {
    quint32 statusRegisterValue = 0;
    statusRegisterValue = regRead(VADDRSTATUSREG);
    // save to global [original inline comment from piHPSDR src/saturnregisters.c:1149]
    gStatusRegister = statusRegisterValue;
}

//
// GetP2PTTKeyInputs(void)
// return several bits from Saturn status register:
// bit 0 - true if PTT active or CW keyer active
// bit 1 - true if CW dot input active
// bit 2 - true if CW dash input active or IO8 active
// bit 4 - true if 10MHz to 122MHz PLL is locked
// note that PTT declared if PTT pressed, or CW key is pressed.
//
// ReadStatusRegister(); [original inline comment from piHPSDR src/saturnregisters.c:1163]
unsigned int getP2PttKeyInputs(void) {
    unsigned int result = 0;

    if (gStatusRegister & 1) {
        result |= 1;
    }

    if ((gStatusRegister >> VCWKEYDOWN) & 1) {
        result |= 1;
    }

    if ((gStatusRegister >> VKEYINA) & 1) {
        // set dot output bit [original inline comment from piHPSDR src/saturnregisters.c:1171]
        result |= 2;
    }

    if ((gStatusRegister >> VKEYINB) & 1) {
        result |= 4;
    }

    if (!((gStatusRegister >> VUSERIO8) & 1)) {
        // set dash output bit [original inline comment from piHPSDR src/saturnregisters.c:1174]
        // set dash output bit if IO8 active [original inline comment from piHPSDR src/saturnregisters.c:1177]
        result |= 4;
    }

    if ((gStatusRegister >> VPLLLOCKED) & 1) {
        // set PLL output bit [original inline comment from piHPSDR src/saturnregisters.c:1180]
        result |= 16;
    }

    if ((gStatusRegister >> VCWKEYDOWN) & 1) {
        // set PTT output bit [original inline comment from piHPSDR src/saturnregisters.c:1165]
        // set PTT output bit if keyer PTT active [original inline comment from piHPSDR src/saturnregisters.c:1168]
        // set PTT if keyer asserted TX [original inline comment from piHPSDR src/saturnregisters.c:1183]
        result |= 1;
    }

    return result;
}

//
// return true if ADC amplitude overflow has occurred since last read.
// the overflow stored state is reset when this is read.
// returns bit0: 1 if ADC1 overflow; bit1: 1 if ARC2 overflow
// for FPGA version >27, returns the unsigned max amplitude from each ADC as parameters
//
// Note the "#if 0": since piHPDR currently does not use the value, [original inline comment from piHPSDR src/saturnregisters.c:1194]
// save the two register reads. [original inline comment from piHPSDR src/saturnregisters.c:1195]
// for FPGAs with code, read the ADC1 & 2 max amplitude [original inline comment from piHPSDR src/saturnregisters.c:1204]
unsigned int getAdcOverflow(uint16_t *adc1Max, uint16_t *adc2Max, RadioInfo rinfo) {
    unsigned int result = 0;
    result = regRead(VADDRADCOVERFLOWBASE);

#if 0
    if(rinfo.firmwareVersion >= 27) {
        *adc1Max = regRead(VADDRADCOVERFLOWBASE+4);
        *adc2Max = regRead(VADDRADCOVERFLOWBASE+8);
    } else {
        *adc1Max = 0;
        *adc2Max = 0;
    }
#endif

    return (result & 0x3);
}

//
// GetUserIOBits(void)
// return the user input bits
// returns IO4 in LSB, IO5 in bit 1, ATU bit in bit 2 & IO8 in bit 3
//
unsigned int getUserIoBits(void) {
    unsigned int result = 0;
    // get user input 4/5/-/8 [original inline comment from piHPSDR src/saturnregisters.c:1219]
    result = ((gStatusRegister >> VUSERIO4) & 0b1011);
    // invert IO8 (should be active low) [original inline comment from piHPSDR src/saturnregisters.c:1220]
    result = result ^ 0x8;
    // get ATU bit into IO6 location [original inline comment from piHPSDR src/saturnregisters.c:1221]
    result |= ((gStatusRegister >> 7) & 0b0100);
    return result;
}

//
// unsigned int GetAnalogueIn(unsigned int AnalogueSelect)
// return one of 6 ADC values from the RF board analogue values
// the paramter selects which input is read.
// AnalogueSelect=0: AIN1 .... AnalogueSepect=5: AIN6
unsigned int getAnalogueIn(unsigned int analogueSelect) {
    unsigned int result = 0;
    // limit to 3 bits [original inline comment from piHPSDR src/saturnregisters.c:1232]
    analogueSelect &= 7;
    result = regRead(VADDRALEXADCBASE + 4 * analogueSelect);
    return result;
}

//
// Initialise TLV320AIC3204 codec.
// separate function because there are many operations needed!
// High Performance Stereo Playback and record
// ---------------------------------------------
// PowerTune mode PTM_P3 is used for high
// performance 16-bit audio. For PTM_P4,
// an external audio interface that provides
// 20-bit audio is required.
//
// For normal USB Audio, no hardware change is required.
//
// If using an external interface, SW2.4 and
// SW2.5 of the USB-ModEVM must be set to
// HI and clocks can be connected to J14 of
// the USB-ModEVM.
//
// Audio is routed to both headphone and
// line outputs.
//
// Software reset [original inline comment from piHPSDR src/saturnregisters.c:1262]
// pg0, r1: Initialize the device through software reset; takes 1ms [original inline comment from piHPSDR src/saturnregisters.c:1263]
// Select Page 0 [original inline comment from piHPSDR src/saturnregisters.c:1264]
// Clock Settings [original inline comment from piHPSDR src/saturnregisters.c:1269]
// The codec receives: MCLK = 12.288 MHz, target sample rate is 48 kHz [original inline comment from piHPSDR src/saturnregisters.c:1270]
// The default (after reset) oversampling rate is 128, so we (only) need [original inline comment from piHPSDR src/saturnregisters.c:1271]
// and additional divider by 2 (NDAC = 1, MDAC = 2) [original inline comment from piHPSDR src/saturnregisters.c:1272]
// Need MDAC at least 2 since the resource class of the processing block is 8 [original inline comment from piHPSDR src/saturnregisters.c:1273]
// pg0, r11&12: NDAC = 1, MDAC = 2 [original inline comment from piHPSDR src/saturnregisters.c:1276]
//pg0 r18, 19: set ADC clock = DAC clock [original inline comment from piHPSDR src/saturnregisters.c:1280]
// Signal Processing Settings [original inline comment from piHPSDR src/saturnregisters.c:1283]
// pg0 r60: set the DAC Mode to PRB_P1 (LVB) [original inline comment from piHPSDR src/saturnregisters.c:1285]
// pg0 r61: set the ADC Mode to PRB_P1 (LVB) [original inline comment from piHPSDR src/saturnregisters.c:1286]
// PRB_P1 is the default processing block (resource class = 8) with 3 biquads [original inline comment from piHPSDR src/saturnregisters.c:1288]
// Initialize Codec [original inline comment from piHPSDR src/saturnregisters.c:1293]
// Select Page 1 [original inline comment from piHPSDR src/saturnregisters.c:1295]
// pg1 r1: Disable weak AVDD in presence of external AVDD supply [original inline comment from piHPSDR src/saturnregisters.c:1298]
// pg1 r2: Enable Master Analog Power Control (LVB) [original inline comment from piHPSDR src/saturnregisters.c:1301]
// pg1 r123: Set the REF charging time to slow (LVB) [original inline comment from piHPSDR src/saturnregisters.c:1304]
// pg1 r1: disable weak AVDD in presence of external AVDD supply [original inline comment from piHPSDR src/saturnregisters.c:1307]
// DUPLICATE? [original inline comment from piHPSDR src/saturnregisters.c:1308]
// pg1 r2: Enable Master Analog Power Control [original inline comment from piHPSDR src/saturnregisters.c:1311]
// pg1 r61: Select ADC PTM_R4 (this is the default) [original inline comment from piHPSDR src/saturnregisters.c:1314]
// pg1 r71: Set the input powerup time [original inline comment from piHPSDR src/saturnregisters.c:1316]
// Recording Setup [original inline comment from piHPSDR src/saturnregisters.c:1319]
// Select Page 1 [original inline comment from piHPSDR src/saturnregisters.c:1321]
// pg1 r58: enable analogue inputs (IN1 and IN3) [original inline comment from piHPSDR src/saturnregisters.c:1324]
// pg1 r52, r55: route IN1+ or IN3+ to PGA [original inline comment from piHPSDR src/saturnregisters.c:1328]
// pg1 r54: Route Common Mode to PGA with 10k resistance [original inline comment from piHPSDR src/saturnregisters.c:1332]
// pg1 r57: Route Common Mode to PGA with 10k resistance [original inline comment from piHPSDR src/saturnregisters.c:1333]
// pg1 r71: input powerup time [original inline comment from piHPSDR src/saturnregisters.c:1337]
// pg1 r59: Unmute Left MICPGA, Gain selection of 23dB [original inline comment from piHPSDR src/saturnregisters.c:1340]
// pg1 r60: Unmute Right MICPGA, Gain selection of 23dB [original inline comment from piHPSDR src/saturnregisters.c:1341]
// pg1 r51: mic bias [original inline comment from piHPSDR src/saturnregisters.c:1345]
// Select Page 0 [original inline comment from piHPSDR src/saturnregisters.c:1348]
// pg0 r81: Power up LADC/RADC [original inline comment from piHPSDR src/saturnregisters.c:1351]
// pg0 r82: Unmute LADC/RADC [original inline comment from piHPSDR src/saturnregisters.c:1354]
// Playback Setup [original inline comment from piHPSDR src/saturnregisters.c:1357]
// Select Page 1 [original inline comment from piHPSDR src/saturnregisters.c:1360]
// Anti-thump step 1. [original inline comment from piHPSDR src/saturnregisters.c:1363]
// pg1 r20: De-pop. 6K, 5 time constants -> 300ms; add 100ms soft routing. [original inline comment from piHPSDR src/saturnregisters.c:1364]
// anti-thump step 2. [original inline comment from piHPSDR src/saturnregisters.c:1367]
// pg1 r10: common mode [original inline comment from piHPSDR src/saturnregisters.c:1368]
// anti-thump step 3. [original inline comment from piHPSDR src/saturnregisters.c:1371]
// pg1 r12, 13: Route LDAC/RDAC to HPL/HPR [original inline comment from piHPSDR src/saturnregisters.c:1372]
// pg1 r14, 15: Route LDAC/RDAC to LOL/LOR [original inline comment from piHPSDR src/saturnregisters.c:1376]
// before anti-thump step 4: [original inline comment from piHPSDR src/saturnregisters.c:1379]
// pg1 r22, 23: in1 to headphone bypass: MUTE [original inline comment from piHPSDR src/saturnregisters.c:1380]
// anti-thump step 4: [original inline comment from piHPSDR src/saturnregisters.c:1384]
// pg0 r63: Power up LDAC/RDAC [original inline comment from piHPSDR src/saturnregisters.c:1385]
// select Page 1 [original inline comment from piHPSDR src/saturnregisters.c:1389]
// anti-thump step 5: [original inline comment from piHPSDR src/saturnregisters.c:1392]
// pg1 r16, 17: Unmute HPL/HPR driver, 0dB Gain [original inline comment from piHPSDR src/saturnregisters.c:1393]
// anti-thump step 6: [original inline comment from piHPSDR src/saturnregisters.c:1397]
// pg1 r9: Power up HPL/HPR and LOL/LOR drivers (LVB) [original inline comment from piHPSDR src/saturnregisters.c:1398]
// pg1 r18, 19: Unmute LOL/LOR driver, 0dB Gain [original inline comment from piHPSDR src/saturnregisters.c:1401]
// Select Page 0 [original inline comment from piHPSDR src/saturnregisters.c:1405]
// pg0 r65, 66: DAC => 0dB [original inline comment from piHPSDR src/saturnregisters.c:1408]
// anti-thump step 7: AFTER 300ms DELAY for ramp-up [original inline comment from piHPSDR src/saturnregisters.c:1412]
// pg0 r64: Unmute LDAC/RDAC [original inline comment from piHPSDR src/saturnregisters.c:1414]
static void initialiseTlv320Aic3204(void) {
    QMutexLocker locker(&codecMutex);
    // Mic Preamp set to 23 dB [original inline comment from piHPSDR src/saturnregisters.c:1260]
    gCodecGain = 46;
    // IN3 routed to PGA with 10k resistance [original inline comment from piHPSDR src/saturnregisters.c:1261]
    gCodecPath = 0x04;
    codecRegisterWrite(   0, 0x00);
    codecRegisterWrite(   1, 0x01);
    // 2ms wait for reset to complete [original inline comment from piHPSDR src/saturnregisters.c:1267]
    usleep (2000);
    // enable NDAC = 1 [original inline comment from piHPSDR src/saturnregisters.c:1277]
    codecRegisterWrite(  11, 0x81);
    // enable MDAC = 1 [original inline comment from piHPSDR src/saturnregisters.c:1278]
    codecRegisterWrite(  12, 0x82);
    // bit7 clear --> ADC_CLK = DAC_CLK [original inline comment from piHPSDR src/saturnregisters.c:1281]
    codecRegisterWrite(  18, 0x01);
    // bit7 clear --> ADC_MOD_CLK = DAC_MOD_CLK [original inline comment from piHPSDR src/saturnregisters.c:1282]
    codecRegisterWrite(  19, 0x02);
    codecRegisterWrite(  60, 0x01);
    codecRegisterWrite(  61, 0x01);
    codecRegisterWrite(   0, 0x01);
    codecRegisterWrite(   1, 0x08);
    // DVDD = AVDD = 1.75 Volt, analog blocks disabled [original inline comment from piHPSDR src/saturnregisters.c:1302]
    codecRegisterWrite(   2, 0x09);
    codecRegisterWrite( 123, 0x00);
    codecRegisterWrite(   1, 0x08);
    // DVDD = AVDD = 1.75 Volt, analog blocks enabled [original inline comment from piHPSDR src/saturnregisters.c:1312]
    codecRegisterWrite(   2, 0x01);
    codecRegisterWrite(  61, 0x00);
    codecRegisterWrite(  71, 0x32);
    codecRegisterWrite(0x00, 0x01);
    codecRegisterWrite(  58, 0x30);
    codecRegisterWrite(  52, gCodecPath);
    codecRegisterWrite(  55, gCodecPath);
    codecRegisterWrite(  54, 0x40);
    codecRegisterWrite(  57, 0x40);
    // 6.4 msec [original inline comment from piHPSDR src/saturnregisters.c:1317]
    // 6.4 msec [original inline comment from piHPSDR src/saturnregisters.c:1338]
    codecRegisterWrite(  71, 0x32);
    codecRegisterWrite(  59, gCodecGain);
    codecRegisterWrite(  60, gCodecGain);
    // Bias powered up (from LDOIN), max. Voltage [original inline comment from piHPSDR src/saturnregisters.c:1346]
    codecRegisterWrite(  51, 0x68);
    codecRegisterWrite(0x00, 0x00);
    codecRegisterWrite(  81, 0xC0);
    codecRegisterWrite(  82, 0x00);
    codecRegisterWrite(   0, 0x01);
    // step time 50 msec, 6 kOhm resistance, 5 time constants, 50 msec soft routing [original inline comment from piHPSDR src/saturnregisters.c:1365]
    codecRegisterWrite(  20, 0x65);
    // HP common mode 1.65 V from LDOIN [original inline comment from piHPSDR src/saturnregisters.c:1369]
    codecRegisterWrite(  10, 0x3B);
    codecRegisterWrite(  12, 0x08);
    codecRegisterWrite(  13, 0x08);
    codecRegisterWrite(  14, 0x08);
    codecRegisterWrite(  15, 0x08);
    codecRegisterWrite(  22, 0x72);
    codecRegisterWrite(  23, 0x72);
    codecRegisterWrite(0x00, 0x00);
    codecRegisterWrite(0x3F, 0xD6);
    codecRegisterWrite(0x00, 0x01);
    codecRegisterWrite(  16, 0x00);
    codecRegisterWrite(  17, 0x00);
    codecRegisterWrite(   9, 0x3F);
    codecRegisterWrite(  18, 0x00);
    codecRegisterWrite(  19, 0x00);
    // select page 0 [original inline comment from piHPSDR src/saturnregisters.c:1386]
    codecRegisterWrite(0x00, 0x00);
    codecRegisterWrite(  65, 0x00);
    codecRegisterWrite(  66, 0x00);
    usleep(300000);
    codecRegisterWrite(  64, 0x00);
}

static void initialiseTlv320Aic23b(void) {
    QMutexLocker locker(&codecMutex);
    // Codec left line in gain register [original inline comment from piHPSDR src/saturnregisters.c:1421]
    gCodecGain = 0;
    // mic input, no boost [original inline comment from piHPSDR src/saturnregisters.c:1422]
    gCodecPath = 0x14;
    // reset register: reset deveice [original inline comment from piHPSDR src/saturnregisters.c:1423]
    codecRegisterWrite(15, 0x0);
    usleep(100);
    // digital activation set to ACTIVE [original inline comment from piHPSDR src/saturnregisters.c:1425]
    codecRegisterWrite(9, 0x1);
    usleep(100);
    codecRegisterWrite(4, gCodecPath);
    usleep(100);
    // all elements powered on [original inline comment from piHPSDR src/saturnregisters.c:1429]
    codecRegisterWrite(6, 0x0);
    usleep(100);
    // slave; no swap; right when LRC high; 16 bit, I2S [original inline comment from piHPSDR src/saturnregisters.c:1431]
    codecRegisterWrite(7, 0x2);
    usleep(100);
    // no clock divide; rate ctrl=0; normal mode, oversample 256Fs [original inline comment from piHPSDR src/saturnregisters.c:1433]
    codecRegisterWrite(8, 0x0);
    usleep(100);
    // no soft mute; no deemphasis; ADC high pass filter enabled [original inline comment from piHPSDR src/saturnregisters.c:1435]
    codecRegisterWrite(5, 0x0);
    usleep(100);
    // line in gain=0 [original inline comment from piHPSDR src/saturnregisters.c:1437]
    codecRegisterWrite(0, gCodecGain);
    usleep(100);
}

//
// CodecInitialise()
// initialise the CODEC, with the register values that don't normally change
// these are the values used by existing HPSDR FPGA firmware
//
void codecInitialise(RadioInfo info) {
    if (info.pcbVersion >= 3) {
        qCDebug(NereusSDR::lcProtocol) << "Initialising TLV320AIC3204 codec";
        installedCodec = e3204;
        initialiseTlv320Aic3204();
    } else {
        qCDebug(NereusSDR::lcProtocol) << "Initialising TLV320AIC23B codec";
        installedCodec = e23b;
        initialiseTlv320Aic23b();
    }
}

//
// SetTXAmplitudeScaling (unsigned int Amplitude)
// sets the overall TX amplitude. This must match the FPGA firmware
// and is set once on program start.
//
void setTxAmplitudeScaling (unsigned int amplitude) {
    quint32 reg;
    QMutexLocker locker(&txConfigMutex);
    // get current settings [original inline comment from piHPSDR src/saturnregisters.c:1467]
    reg = txConfigRegValue;
    // remove old bits [original inline comment from piHPSDR src/saturnregisters.c:1468]
    reg &= 0xFFC0000F;
    // add new bits [original inline comment from piHPSDR src/saturnregisters.c:1469]
    reg |= ((amplitude & 0x3FFFF) << VTXCONFIGSCALEBIT);

    if (reg != txConfigRegValue) {
        // store it back [original inline comment from piHPSDR src/saturnregisters.c:1471]
        txConfigRegValue = reg;
        // and write to it [original inline comment from piHPSDR src/saturnregisters.c:1472]
        regWrite(VADDRTXCONFIGREG, reg);
    }
}

//
// SetTXProtocol2 (void)
// config TX for P2. This is called ONCE at startup
void setTxProtocol2 () {
    quint32 reg;
    QMutexLocker locker(&txConfigMutex);
    // get current settings [original inline comment from piHPSDR src/saturnregisters.c:1483]
    reg = txConfigRegValue;
    // remove old bit [original inline comment from piHPSDR src/saturnregisters.c:1484]
    reg &= 0xFFFFFF7;
    // Set P2 (192 kHz) [original inline comment from piHPSDR src/saturnregisters.c:1485]
    reg |= 1 << VTXCONFIGPROTOCOLBIT;

    if (reg != txConfigRegValue) {
        // store it back [original inline comment from piHPSDR src/saturnregisters.c:1487]
        txConfigRegValue = reg;
        // and write to it [original inline comment from piHPSDR src/saturnregisters.c:1488]
        regWrite(VADDRTXCONFIGREG, reg);
    }
}

//
// void ResetDUCMux(void)
// resets to 64 to 48 bit multiplexer to initial state, expecting 1st 64 bit word
// also causes any input data to be discarded, so don't set it for long!
//
void resetDucMux(void) {
    quint32 reg;
    quint32 bitMask;
    bitMask = (1 << 29);
    QMutexLocker locker(&txConfigMutex);
    // get current settings [original inline comment from piHPSDR src/saturnregisters.c:1503]
    reg = txConfigRegValue;
    reg |= bitMask;
    regWrite(VADDRTXCONFIGREG, reg);
    // remove old bit [original inline comment from piHPSDR src/saturnregisters.c:1506]
    reg &= ~bitMask;
    // and write to it [original inline comment from piHPSDR src/saturnregisters.c:1505]
    // and write to it [original inline comment from piHPSDR src/saturnregisters.c:1507]
    regWrite(VADDRTXCONFIGREG, reg);
    txConfigRegValue = reg;
}

//
// void SetTXIQDeinterleave(bool Interleaved)
// if true, put DUC hardware in EER mode. Alternate IQ samples go:
// even samples to I/Q modulation; odd samples to EER.
// ensure FIFO empty & reset multiplexer when changing this bit!
// shgould be called by the TX I/Q data handler only to be sure
// of meeting that constraint
//
void setTxIqDeinterleaved(bool interleaved) {
    quint32 reg;
    quint32 bitMask;
    bitMask = (1 << 30);
    QMutexLocker locker(&txConfigMutex);
    // get current settings [original inline comment from piHPSDR src/saturnregisters.c:1525]
    reg = txConfigRegValue;

    if (interleaved) {
        // set bit if true [original inline comment from piHPSDR src/saturnregisters.c:1527]
        reg |= bitMask;
    } else {
        // clear bit if false [original inline comment from piHPSDR src/saturnregisters.c:1529]
        reg &= ~bitMask;
    }

    if (reg != txConfigRegValue) {
        // store it back [original inline comment from piHPSDR src/saturnregisters.c:1532]
        txConfigRegValue = reg;
        // and write to it [original inline comment from piHPSDR src/saturnregisters.c:1533]
        regWrite(VADDRTXCONFIGREG, reg);
    }
}

//
// void EnableDUCMux(bool Enabled)
// enabled the multiplexer to take samples from FIFO and hand on to DUC
// // needs to be stoppable if there is an error condition
//
void enableDucMux(bool enabled) {
    quint32 reg;
    quint32 bitMask;
    bitMask = 0x80000000;
    QMutexLocker locker(&txConfigMutex);
    // get current settings [original inline comment from piHPSDR src/saturnregisters.c:1548]
    reg = txConfigRegValue;

    if (enabled) {
        // set bit if true [original inline comment from piHPSDR src/saturnregisters.c:1550]
        reg |= bitMask;
    } else {
        // clear bit if false [original inline comment from piHPSDR src/saturnregisters.c:1552]
        reg &= ~bitMask;
    }

    if (reg != txConfigRegValue) {
        // store it back [original inline comment from piHPSDR src/saturnregisters.c:1555]
        txConfigRegValue = reg;
        // and write to it [original inline comment from piHPSDR src/saturnregisters.c:1556]
        regWrite(VADDRTXCONFIGREG, reg);
    }
}
} // namespace NereusSDR
