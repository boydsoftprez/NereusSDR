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

void initialiseDacAttenRoms(void) {
    dacCurrentRom[0] = 0;
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
        reg |= (1 << VDATAENDIAN);
    } else {
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

void setP2SampleRate(unsigned int ddc, bool enabled, unsigned int sampleRate, bool interleaveWithNext) {
    quint32 registerValue;
    quint32 mask;
    ESampleRate rate;
    mask = 7 << (ddc * 3);

    QMutexLocker locker(&ddcRateMutex);

    if (!enabled) {
        gDdcEnabled &= ~(1 << ddc);
        rate = eDisabled;
    } else {
        gDdcEnabled |= (1 << ddc);

        if (interleaveWithNext) {
            rate = eInterleaveWithNext;
        } else {
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

    registerValue = ddcRateReg;
    registerValue &= ~mask;
    mask = (quint32)rate;
    mask = mask << (ddc * 3);
    registerValue |= mask;
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
    quint32 reg;
    quint32 bitMask;
    QMutexLocker locker(&gpioMutex);
    reg = gpioRegValue;
    bitMask = (0b1111111) << VOPENCOLLECTORBITS;
    reg = reg & ~bitMask;
    reg |= (bits << VOPENCOLLECTORBITS);

    if (reg != gpioRegValue) {
        gpioRegValue = reg;
        regWrite(VADDRRFGPIOREG, reg);
    }
}

//
// SetADCOptions(bool PGA1, bool Dither1, bool Random1, bool PGA2, bool Dither2, bool Random2);
// sets the ADC contol bits for both ADCs
//
void setAdcOptions(bool pga1, bool dither1, bool random1, bool pga2, bool dither2, bool random2) {
    quint32 reg;
    QMutexLocker locker(&gpioMutex);
    reg = gpioRegValue;
    reg &= ~(1 << VADC1RANDBIT);
    reg &= ~(1 << VADC1PGABIT);
    reg &= ~(1 << VADC1DITHERBIT);
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
        gpioRegValue = reg;
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
void setDdcFrequency(uint32_t ddc, uint32_t value, bool isDeltaPhase) {
    quint32 deltaPhase;

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
void setDucFrequency(unsigned int value, bool isDeltaPhase, RadioInfo rinfo) {
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
            ducDeltaPhase = deltaPhase;
            regWrite(VADDRTXDUCREG, deltaPhase);
        }
    }

    if (info.pcbVersion >= 3) {
        bool needsHpf = false;

        if (deltaPhase > DELTAPHIHPFCUTIN) { needsHpf = true; }

        QMutexLocker locker(&txConfigMutex);
        quint32 reg = txConfigRegValue;
        reg &= ~(1 << VTXCONFIGHPFENABLE);

        if (needsHpf) { reg |= (1 << VTXCONFIGHPFENABLE); }

        if (reg != txConfigRegValue) {
            txConfigRegValue = reg;
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
    quint32 reg = gAlexRxRegister;

    if (rx != 2) {
        reg &= 0xFFFF0000;
        reg |= bits;
    } else {
        reg &= 0x0000FFFF;
        reg |= (bits << 16);
    }

    if (reg != gAlexRxRegister) {
        gAlexRxRegister = reg;
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
    quint32 reg = bits;
    QMutexLocker locker(&alexTxMutex);

    if (hasTxAntExplicitly && (reg != gAlexTxAntRegister)) {
        gAlexTxAntRegister = reg;
        regWrite(VADDRALEXSPIREG + VOFFSETALEXTXANTREG, reg);
    } else if (!hasTxAntExplicitly && (reg != gAlexTxFiltRegister)) {
        gAlexTxFiltRegister = reg;
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
    level &= 0xFF;
    dacDrive = dacCurrentRom[level];
    attenDrive = dacStepAttenRom[level];
    registerValue = dacDrive;
    registerValue |= (dacDrive << 8);
    registerValue |= (attenDrive << 16);
    registerValue |= (attenDrive << 24);
    QMutexLocker locker(&txDriveMutex);

    if (registerValue != gTxDacCtrl) {
        gTxDacCtrl = registerValue;
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
void setCodecInputParams(bool enableLine, bool enableBoost, int lineInGain) {
    unsigned int path, gain;
    QMutexLocker locker(&codecMutex);

    switch (installedCodec) {
    case e23b:
        path = gCodecPath & 0xFFF8;
        gain = gCodecGain & 0xFFE0;

        if (enableLine) {
            path |= 0x02;
            gain |= (lineInGain & 0x001F);
        } else {
            path |= 0x04;

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
            path = 0xC0;
            gain = 3 * (lineInGain & 0x001F);
        } else {
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
    quint32 reg;
    QMutexLocker locker(&gpioMutex);
    reg = gpioRegValue;
    reg &= ~(1 << VMICBIASENABLEBIT);
    reg &= ~(1 << VMICPTTSELECTBIT);
    reg &= ~(1 << VMICSIGNALSELECTBIT);
    reg &= ~(1 << VMICBIASSELECTBIT);

    if (!micRing) {
        reg &= ~(1 << VMICSIGNALSELECTBIT);
        reg |= (1 << VMICBIASSELECTBIT);
        reg &= ~(1 << VMICPTTSELECTBIT);
    } else {
        reg |= (1 << VMICSIGNALSELECTBIT);
        reg &= ~(1 << VMICBIASSELECTBIT);
        reg |= (1 << VMICPTTSELECTBIT);
    }

    if (enableBias) {
        reg |= (1 << VMICBIASENABLEBIT);
    }

    if (reg != gpioRegValue) {
        gpioRegValue = reg;
        regWrite(VADDRRFGPIOREG, reg);
    }
}

//
// SetBalancedMicInput(bool Balanced)
// selects the balanced microphone input, not supported by current protocol code.
// just set the bit into GPIO
//
void setBalancedMicInput(bool balanced) {
    quint32 reg;
    QMutexLocker locker(&gpioMutex);
    reg = gpioRegValue;
    reg &= ~(1 << VBALANCEDMICSELECT);

    if (balanced) {
        reg |= (1 << VBALANCEDMICSELECT);
    }

    if (reg != gpioRegValue) {
        gpioRegValue = reg;
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
void setCwIambicKeyer(uint8_t speed, uint8_t weight, bool reverseKeys, bool mode,
                      bool strictSpacing, bool iambicEnabled, bool breakin) {
    quint32 reg;
    QMutexLocker locker(&iambicMutex);
    reg = gIambicConfigReg;
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
    adcSetting = ((quint32)adc & 0x3) << (ddc * 2);
    mask = 0x3 << (ddc * 2);
    QMutexLocker locker(&ddcInSelMutex);
    reg = ddcInSelReg;
    reg &= ~mask;
    reg |= adcSetting;

    if (reg != ddcInSelReg) {
        ddcInSelReg = reg;
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
    address = VADDRDDCINSEL;
    QMutexLocker locker(&ddcInSelMutex);
    data = ddcInSelReg;

    if (isEnabled) {
        data |= (1 << 30);
    } else {
        data &= ~(1 << 30);
    }

    if (data != ddcInSelReg) {
        ddcInSelReg = data;
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
            double y = (double) cntr / (double) rampLength;
            double y2  = y * 6.2831853071795864769252867665590;
            double y4  = y2 + y2;
            double y6  = y4 + y2;
            double y8  = y4 + y4;
            double y10 = y4 + y6;
            double rampsample = y - 0.12182865361171612    * sin(y2)
                                - 0.018557469249199286   * sin(y4)
                                - 0.0009378783245428506  * sin(y6)
                                + 0.0008567571519403228  * sin(y8)
                                + 0.00018706912431472442 * sin(y10);
            sample = (quint32) (rampsample * 8388607.0);
            regWrite(VADDRCWKEYERRAM + 4 * cntr, sample);
        }

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
    reg = txConfigRegValue;
    reg &= 0xFFFFFFFC;
    reg |= ((unsigned int)source);

    if (reg != txConfigRegValue) {
        txConfigRegValue = reg;
        regWrite(VADDRTXCONFIGREG, reg);
    }
}

//
// EnableCW (bool Enabled, bool Breakin)
// enables or disables CW mode; selects CW as modulation source.
// If Breakin enabled, the key input engages TX automatically
// and generates sidetone.
//
void enableCw (bool enabled, bool breakin) {
    gCwEnabled = enabled;

    if (enabled) {
        setTxModulationSource(eCWKeyer);
    } else {
        setTxModulationSource(eIQData);
    }

    gBreakinEnabled = breakin;
    activateCwKeyer(gBreakinEnabled && gCwEnabled);
}

void setCwSideTone(bool enabled, uint8_t volume, uint16_t frequency) {
    quint32 reg;
    reg = (512 * frequency) / 375;

    if (enabled) {
        reg |= (volume & 0xFF) << 24;
    }

    QMutexLocker locker(&sideToneMutex);

    if (reg != gSideToneReg) {
        gSideToneReg = reg;
        regWrite(VADDRSIDETONECONFIGREG, reg);
    }
}

void setKeyerParams(uint8_t delay, uint16_t hangTime, uint8_t ramp, RadioInfo rinfo) {
    quint32 reg;
    QMutexLocker locker(&keyerMutex);
    reg = gCwKeyerSetup;
    reg &= 0xFFFC0000;
    reg |= (delay & 0xFF);
    reg |= (hangTime & 0x3FF) << VCWKEYERHANG;

    if (ramp > 0) {
        initialiseCwKeyerRamp(ramp, rinfo);
        reg &= 0x8003FFFF;

        if (rinfo.firmwareVersion >= 14) {
            reg |= (gCwKeyerRampLength << VCWKEYERRAMP);
        } else {
            reg |= ((gCwKeyerRampLength << 2) << VCWKEYERRAMP);
        }
    }

    if (reg != gCwKeyerSetup) {
        gCwKeyerSetup = reg;
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
    reg = gpioRegValue;

    if (enabled) {
        reg |= (1 << VXVTRENABLEBIT);
    } else {
        reg &= ~(1 << VXVTRENABLEBIT);
    }

    if (reg != gpioRegValue) {
        gpioRegValue = reg;
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
    reg = gpioRegValue;

    if (!enabled) {
        reg |= (1 << VTXRELAYDISABLEBIT);
    } else {
        reg &= ~(1 << VTXRELAYDISABLEBIT);
    }

    if (reg != gpioRegValue) {
        gpioRegValue = reg;
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
    reg = gpioRegValue;

    if (isMuted) {
        reg |= (1 << VSPKRMUTEBIT);
    } else {
        reg &= ~(1 << VSPKRMUTEBIT);
    }

    if (reg != gpioRegValue) {
        gpioRegValue = reg;
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
unsigned int getP2PttKeyInputs(void) {
    unsigned int result = 0;

    if (gStatusRegister & 1) {
        result |= 1;
    }

    if ((gStatusRegister >> VCWKEYDOWN) & 1) {
        result |= 1;
    }

    if ((gStatusRegister >> VKEYINA) & 1) {
        result |= 2;
    }

    if ((gStatusRegister >> VKEYINB) & 1) {
        result |= 4;
    }

    if (!((gStatusRegister >> VUSERIO8) & 1)) {
        result |= 4;
    }

    if ((gStatusRegister >> VPLLLOCKED) & 1) {
        result |= 16;
    }

    if ((gStatusRegister >> VCWKEYDOWN) & 1) {
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
    result = ((gStatusRegister >> VUSERIO4) & 0b1011);
    result = result ^ 0x8;
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
static void initialiseTlv320Aic3204(void) {
    QMutexLocker locker(&codecMutex);
    gCodecGain = 46;
    gCodecPath = 0x04;
    codecRegisterWrite(   0, 0x00);
    codecRegisterWrite(   1, 0x01);
    usleep (2000);
    codecRegisterWrite(  11, 0x81);
    codecRegisterWrite(  12, 0x82);
    codecRegisterWrite(  18, 0x01);
    codecRegisterWrite(  19, 0x02);
    codecRegisterWrite(  60, 0x01);
    codecRegisterWrite(  61, 0x01);
    codecRegisterWrite(   0, 0x01);
    codecRegisterWrite(   1, 0x08);
    codecRegisterWrite(   2, 0x09);
    codecRegisterWrite( 123, 0x00);
    codecRegisterWrite(   1, 0x08);
    codecRegisterWrite(   2, 0x01);
    codecRegisterWrite(  61, 0x00);
    codecRegisterWrite(  71, 0x32);
    codecRegisterWrite(0x00, 0x01);
    codecRegisterWrite(  58, 0x30);
    codecRegisterWrite(  52, gCodecPath);
    codecRegisterWrite(  55, gCodecPath);
    codecRegisterWrite(  54, 0x40);
    codecRegisterWrite(  57, 0x40);
    codecRegisterWrite(  71, 0x32);
    codecRegisterWrite(  59, gCodecGain);
    codecRegisterWrite(  60, gCodecGain);
    codecRegisterWrite(  51, 0x68);
    codecRegisterWrite(0x00, 0x00);
    codecRegisterWrite(  81, 0xC0);
    codecRegisterWrite(  82, 0x00);
    codecRegisterWrite(   0, 0x01);
    codecRegisterWrite(  20, 0x65);
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
    codecRegisterWrite(0x00, 0x00);
    codecRegisterWrite(  65, 0x00);
    codecRegisterWrite(  66, 0x00);
    usleep(300000);
    codecRegisterWrite(  64, 0x00);
}

static void initialiseTlv320Aic23b(void) {
    QMutexLocker locker(&codecMutex);
    gCodecGain = 0;
    gCodecPath = 0x14;
    codecRegisterWrite(15, 0x0);
    usleep(100);
    codecRegisterWrite(9, 0x1);
    usleep(100);
    codecRegisterWrite(4, gCodecPath);
    usleep(100);
    codecRegisterWrite(6, 0x0);
    usleep(100);
    codecRegisterWrite(7, 0x2);
    usleep(100);
    codecRegisterWrite(8, 0x0);
    usleep(100);
    codecRegisterWrite(5, 0x0);
    usleep(100);
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
    reg = txConfigRegValue;
    reg &= 0xFFC0000F;
    reg |= ((amplitude & 0x3FFFF) << VTXCONFIGSCALEBIT);

    if (reg != txConfigRegValue) {
        txConfigRegValue = reg;
        regWrite(VADDRTXCONFIGREG, reg);
    }
}

//
// SetTXProtocol2 (void)
// config TX for P2. This is called ONCE at startup
void setTxProtocol2 () {
    quint32 reg;
    QMutexLocker locker(&txConfigMutex);
    reg = txConfigRegValue;
    reg &= 0xFFFFFF7;
    reg |= 1 << VTXCONFIGPROTOCOLBIT;

    if (reg != txConfigRegValue) {
        txConfigRegValue = reg;
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
    reg = txConfigRegValue;
    reg |= bitMask;
    regWrite(VADDRTXCONFIGREG, reg);
    reg &= ~bitMask;
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
    reg = txConfigRegValue;

    if (interleaved) {
        reg |= bitMask;
    } else {
        reg &= ~bitMask;
    }

    if (reg != txConfigRegValue) {
        txConfigRegValue = reg;
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
    reg = txConfigRegValue;

    if (enabled) {
        reg |= bitMask;
    } else {
        reg &= ~bitMask;
    }

    if (reg != txConfigRegValue) {
        txConfigRegValue = reg;
        regWrite(VADDRTXCONFIGREG, reg);
    }
}
} // namespace NereusSDR
