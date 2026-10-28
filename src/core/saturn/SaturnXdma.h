// no-port-check: piHPSDR-only port; exact notices and source mapping are in
// docs/attribution/PIHPSDR-PROVENANCE.md. Shared contributor callsigns are
// not evidence of a Thetis port. This does not exempt piHPSDR attribution.
// src/core/saturn/SaturnXdma.h (NereusSDR)
// Ported from piHPSDR sources at @4aa95c5; verbatim source notices follow.
// Modification history (NereusSDR):
//   2026-10-28 - Recover notices for Martinus Stroomer's (CT1IQI) C++
//                 adaptation using Codeconvert.ai. J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex. Unfinished backend retained
//                 outside the build; no register-write or DMA use is enabled.
//
// --- From piHPSDR src/saturndrivers.h [@4aa95c5] ---
/* Copyright (C)
* 2021 - Laurence Barker G8NJJ
* 2025 - Christoph van Wüllen, DL1YCF
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

// --- From piHPSDR src/saturnregisters.h [@4aa95c5] ---
/* Copyright (C)
* 2021 - Laurence Barker G8NJJ
* 2025 - Christoph van Wüllen, DL1YCF
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

#ifndef SATURNXDMA_H
#define SATURNXDMA_H
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
 * Local copies are kept of the FPGA status and settings; changes are only sent 
 * via xdma when indeed a change needs to be made to one or more settings. 
 */

#include <QMetaType>
#include <QtCore/QtGlobal>
#include "../RadioDiscovery.h"

namespace NereusSDR {

#define VNUMDDC 10                      // # DDCs (downconverters) available

//
// DMA channel allocations
//
#define VMICDMADEVICE "/dev/xdma0_c2h_1"
#define VDDCDMADEVICE "/dev/xdma0_c2h_0"
#define VSPKDMADEVICE "/dev/xdma0_h2c_1"
#define VDUCDMADEVICE "/dev/xdma0_h2c_0"

//
// FPGA register map
//
#define VADDRDDC0REG             0x00000
#define VADDRDDC1REG             0x00004
#define VADDRDDC2REG             0x00008
#define VADDRDDC3REG             0x0000C
#define VADDRDDC4REG             0x00010
#define VADDRDDC5REG             0x00014
#define VADDRDDC6REG             0x00018
#define VADDRDDC7REG             0x0001C
#define VADDRDDC8REG             0x01000
#define VADDRDDC9REG             0x01004
#define VADDRRXTESTDDSREG        0x01008
#define VADDRDDCRATES            0x0100C
#define VADDRDDCINSEL            0x01010
#define VADDRKEYERCONFIGREG      0x02000
#define VADDRSIDETONECONFIGREG   0x02004
#define VADDRTXCONFIGREG         0x02008
#define VADDRTXDUCREG            0x0200C
#define VADDRTXMODTESTREG        0x02010
#define VADDRRFGPIOREG           0x02014
#define VADDRADCCTRLREG          0x02018
#define VADDRDACCTRLREG          0x0201C
#define VADDRDEBUGLEDREG         0x03000
#define VADDRSTATUSREG           0x04000
#define VADDRDATECODE            0x04004
#define VADDRADCOVERFLOWBASE     0x05000
#define VADDRFIFOOVERFLOWBASE    0x06000
#define VADDRFIFORESET           0x07000
#define VADDRIAMBICCONFIG        0X07004
#define VADDRFIFOMONBASE         0x09000
#define VADDRALEXADCBASE         0x0A000
#define VADDRALEXSPIREG          0x0B000
#define VADDRBOARDID1            0x0C000
#define VADDRBOARDID2            0x0C004
#define VADDRCONFIGSPIREG        0x10000
#define VADDRCODECSPIWRITEREG    0x14000            // Write to Codec
#define VADDRCODECSPIREADREG     0x14004            // Read from Codec
#define VADDRXADCREG             0x18000            // on-chip XADC (temp, VCC...)
#define VADDRCWKEYERRAM          0x1C000            // keyer RAM mapped here

#define VNUMDMAFIFO           4                  // DMA streams available
#define VADDRDDCSTREAMREAD    0x0L               // stream reader/writer on AXI-4 bus
#define VADDRDUCSTREAMWRITE   0x0L               // stream reader/writer on AXI-4 bus
#define VADDRMICSTREAMREAD    0x40000L           // stream reader/writer on AXI-4 bus
#define VADDRSPKRSTREAMWRITE  0x40000L           // stream reader/writer on AXI-4 bus

#define VBITCODECMICFIFORESET 0         // reset bit in register
#define VBITCODECSPKFIFORESET 1         // reset bit in register
#define VBITDDCFIFORESET      2         // reset bit in register
#define VBITDUCFIFORESET      3         // reset bit in register

//
// ALEX SPI registers
//
#define VOFFSETALEXTXFILTREG       0
#define VOFFSETALEXRXREG           4
#define VOFFSETALEXTXANTREG        8

//
// GPIO output bits
//
#define VMICBIASENABLEBIT           0
#define VMICPTTSELECTBIT            1
#define VMICSIGNALSELECTBIT         2
#define VMICBIASSELECTBIT           3
#define VSPKRMUTEBIT                4
#define VBALANCEDMICSELECT          5
#define VADC1RANDBIT                8
#define VADC1PGABIT                 9
#define VADC1DITHERBIT             10
#define VADC2RANDBIT               11
#define VADC2PGABIT                12
#define VADC2DITHERBIT             13
#define VOPENCOLLECTORBITS         16
#define VMOXBIT                    24
#define VTXENABLEBIT               25
#define VDATAENDIAN                26
#define VTXRELAYDISABLEBIT         27
#define VPURESIGNALENABLE          28
#define VATUTUNEBIT                29
#define VXVTRENABLEBIT             30

//
// GPIO input buts
//
#define VKEYINA                     2
#define VKEYINB                     3
#define VUSERIO4                    4
#define VUSERIO5                    5
#define VUSERIO6                    6
#define VUSERIO8                    7
#define V13_8VDETECTBIT             8
#define VATUTUNECOMPLETEBIT         9
#define VPLLLOCKED                 10
#define VCWKEYDOWN                 11
#define VCWKEYPRESSED              12

//
// Keyer setup register defines
//
#define VCWKEYERENABLE             31
#define VCWKEYERDELAY               0
#define VCWKEYERHANG                8
#define VCWKEYERRAMP               18
#define VRAMPSIZE                4096

//
// Iambic config register defines
//
#define VIAMBICSPEED                0
#define VIAMBICWEIGHT               8
#define VIAMBICREVERSED            16
#define VIAMBICENABLE              17
#define VIAMBICMODE                18
#define VIAMBICSTRICT              19
#define VIAMBICCWX                 20
#define VIAMBICCWXDOT              21
#define VIAMBICCWXDASH             22
#define VCWBREAKIN                 23
#define VIAMBICCWXBITS     0x00700000
#define VIAMBICBITS        0x000FFFFF

//
// TX config register defines
//
#define VTXCONFIGDATASOURCEBIT      0
#define VTXCONFIGSAMPLEGATINGBIT    2
#define VTXCONFIGPROTOCOLBIT        3
#define VTXCONFIGSCALEBIT           4
#define VTXCONFIGHPFENABLE         27
#define VTXCONFIGWATCHDOGOVERRIDE  28
#define VTXCONFIGMUXRESETBIT       29
#define VTXCONFIGIQDEINTERLEAVEBIT 30
#define VTXCONFIGIQSTREAMENABLED   31

//
// enum type for ADC selection
//
typedef enum {
  eADC1,                        // selects ADC1
  eADC2,                        // selects ADC2
  eTestSource,                  // selects internal test source (not for operational use)
  eTXSamples                    // (for Puresignal)
} EADCSelect;

//
// enum type for FIFO monitor and DMA channel selection
//
typedef enum {
    eRXDDCDMA,              // selects RX
    eTXDUCDMA,              // selects TX
    eMicCodecDMA,           // selects mic samples
    eSpkCodecDMA            // selects speaker samples
} EDMAStreamSelect;

//
// enum for the different types of codecs we can have on the G2 board
//
typedef enum {
    e23b,                   // TLV320AIC23B (now end-of-life)
    e3204                   // TLV320AIC3204 (replacement in newer boards)
} ECodecType;

//
// enum type for sample rate. only 48-384KHz allowed for protocol 1
//
typedef enum {
    eDisabled = 0,
    e48KHz,
    e96KHz,
    e192KHz,
    e384KHz,
    e768KHz,
    e1536KHz,
    eInterleaveWithNext
} ESampleRate;

//
// enum for TX modulation source
//
typedef enum {
  eIQData,
  eFixed0Hz,
  eTXDDS,
  eCWKeyer
} ETXModulationSource;

///////////////////////////////////////////////////////////// [original inline comment from piHPSDR src/saturndrivers.h:20]
// Saturn project: Artix7 FPGA + Raspberry Pi4 Compute Module [original inline comment from piHPSDR src/saturndrivers.h:22]
// PCI Express interface from linux on Raspberry pi [original inline comment from piHPSDR src/saturndrivers.h:23]
// this application uses C code to emulate HPSDR protocol 1 [original inline comment from piHPSDR src/saturndrivers.h:24]
// Contribution of interfacing to PiHPSDR from N1GP (Rick Koch) [original inline comment from piHPSDR src/saturndrivers.h:26]
// saturndrivers.h: [original inline comment from piHPSDR src/saturndrivers.h:28]
// header file. Drivers for minor IP cores [original inline comment from piHPSDR src/saturndrivers.h:29]
////////////////////////////////////////////////////////////// [original inline comment from piHPSDR src/saturndrivers.h:31]
// void SetupFIFOMonitorChannel(EDMAStreamSelect Channel, bool EnableInterrupt); [original inline comment from piHPSDR src/saturndrivers.h:72]
// Setup a single FIFO monitor channel. [original inline comment from piHPSDR src/saturndrivers.h:74]
//   Channel:     IP channel number (enum) [original inline comment from piHPSDR src/saturndrivers.h:75]
//   EnableInterrupt: true if interrupt generation enabled for overflows [original inline comment from piHPSDR src/saturndrivers.h:76]
// uint32_t ReadFIFOMonitorChannel(EDMAStreamSelect Channel, bool* Overflowed, bool* OverThreshold, bool* Underflowed, unsigned int* Current); [original inline comment from piHPSDR src/saturndrivers.h:81]
// Read number of locations in a FIFO [original inline comment from piHPSDR src/saturndrivers.h:83]
// for a read FIFO: returns the number of occupied locations available to read [original inline comment from piHPSDR src/saturndrivers.h:84]
// for a write FIFO: returns the number of free locations available to write [original inline comment from piHPSDR src/saturndrivers.h:85]
//   Channel:     IP core channel number (enum) [original inline comment from piHPSDR src/saturndrivers.h:86]
//   Overflowed:    true if an overflow has occurred. Reading clears the overflow bit. [original inline comment from piHPSDR src/saturndrivers.h:87]
//   OverThreshold:   true if overflow occurred  measures by threshold. Cleared by read. [original inline comment from piHPSDR src/saturndrivers.h:88]
//   Underflowed:       true if underflow has occurred. Cleared by read. [original inline comment from piHPSDR src/saturndrivers.h:89]
//   Current:           number of locations occupied (in either FIFO type) [original inline comment from piHPSDR src/saturndrivers.h:90]
// reset a stream FIFO [original inline comment from piHPSDR src/saturndrivers.h:96]
// clears the FIFOs directly read ori written by the FPGA [original inline comment from piHPSDR src/saturndrivers.h:97]
// SetTXAmplitudeEER (bool EEREnabled) [original inline comment from piHPSDR src/saturndrivers.h:102]
// enables amplitude restoratino mode. Generates envelope output alongside I/Q samples. [original inline comment from piHPSDR src/saturndrivers.h:103]
// NOTE hardware does not properly support this yet! [original inline comment from piHPSDR src/saturndrivers.h:104]
// uint32_t AnalyseDDCHeader(unit32_t Header, unit32_t** DDCCounts) [original inline comment from piHPSDR src/saturndrivers.h:109]
// Global "version" variables, determined upon startup [original inline comment from piHPSDR src/saturndrivers.h:118]
///////////////////////////////////////////////////////////// [original inline comment from piHPSDR src/saturnregisters.h:20]
// Saturn project: Artix7 FPGA + Raspberry Pi4 Compute Module [original inline comment from piHPSDR src/saturnregisters.h:22]
// PCI Express interface from linux on Raspberry pi [original inline comment from piHPSDR src/saturnregisters.h:23]
// this application uses C code to emulate HPSDR protocol 1 [original inline comment from piHPSDR src/saturnregisters.h:24]
// Contribution of interfacing to PiHPSDR from N1GP (Rick Koch) [original inline comment from piHPSDR src/saturnregisters.h:26]
// saturnregisters.h: [original inline comment from piHPSDR src/saturnregisters.h:28]
// Hardware access to FPGA registers in the Saturn FPGA [original inline comment from piHPSDR src/saturnregisters.h:29]
//  at the level of "set TX frequency" or set DDC frequency" [original inline comment from piHPSDR src/saturnregisters.h:30]
////////////////////////////////////////////////////////////// [original inline comment from piHPSDR src/saturnregisters.h:32]
// DDC I/Q packet length (in Bytes) [original inline comment from piHPSDR src/saturnregisters.h:41]
// open/close connection to the XDMA device driver for register and DMA access [original inline comment from piHPSDR src/saturnregisters.h:44]
// initiate a DMA to the FPGA with specified parameters [original inline comment from piHPSDR src/saturnregisters.h:50]
// returns 1 if success, else 0 [original inline comment from piHPSDR src/saturnregisters.h:51]
// fd: file device (an open file) [original inline comment from piHPSDR src/saturnregisters.h:52]
// SrcData: pointer to memory block to transfer [original inline comment from piHPSDR src/saturnregisters.h:53]
// Length: number of bytes to copy [original inline comment from piHPSDR src/saturnregisters.h:54]
// AXIAddr: offset address in the FPGA window [original inline comment from piHPSDR src/saturnregisters.h:55]
// initiate a DMA from the FPGA with specified parameters [original inline comment from piHPSDR src/saturnregisters.h:60]
// returns 1 if success, else 0 [original inline comment from piHPSDR src/saturnregisters.h:61]
// fd: file device (an open file) [original inline comment from piHPSDR src/saturnregisters.h:62]
// DestData: pointer to memory block to transfer [original inline comment from piHPSDR src/saturnregisters.h:63]
// Length: number of bytes to copy [original inline comment from piHPSDR src/saturnregisters.h:64]
// AXIAddr: offset address in the FPGA window [original inline comment from piHPSDR src/saturnregisters.h:65]
// single 32 bit register read/write, from/to AXI-Lite bus [original inline comment from piHPSDR src/saturnregisters.h:70]
// initialise the DAC Atten ROMs [original inline comment from piHPSDR src/saturnregisters.h:157]
// these set the step attenuator and DAC drive level [original inline comment from piHPSDR src/saturnregisters.h:158]
// for "attenuation intent" values from 0 to 255 [original inline comment from piHPSDR src/saturnregisters.h:159]
// SetByteSwap(bool) [original inline comment from piHPSDR src/saturnregisters.h:164]
// set whether byte swapping is enabled. True if yes, to get data in network byte order. [original inline comment from piHPSDR src/saturnregisters.h:165]
// SetMOX(bool Mox) [original inline comment from piHPSDR src/saturnregisters.h:170]
// sets or clears TX state [original inline comment from piHPSDR src/saturnregisters.h:171]
// SetTXEnable(bool Enabled) [original inline comment from piHPSDR src/saturnregisters.h:176]
// sets or clears TX enable bit [original inline comment from piHPSDR src/saturnregisters.h:177]
// set or clear the relevant bit in GPIO [original inline comment from piHPSDR src/saturnregisters.h:178]
// SetP2SampleRate(unsigned int DDC, bool Enabled, unsigned int SampleRate, bool InterleaveWithNext) [original inline comment from piHPSDR src/saturnregisters.h:183]
// sets the sample rate for a single DDC (used in protocol 2) [original inline comment from piHPSDR src/saturnregisters.h:184]
// allowed rates are 48KHz to 1536KHz. [original inline comment from piHPSDR src/saturnregisters.h:185]
// This sets the DDCRateReg variable and does NOT write to hardware [original inline comment from piHPSDR src/saturnregisters.h:186]
// The WriteP2DDCRateRegister() call must be made after setting values for all DDCs [original inline comment from piHPSDR src/saturnregisters.h:187]
// void WriteP2DDCRateRegister(void) [original inline comment from piHPSDR src/saturnregisters.h:192]
// writes the DDCRateRegister, once all settings have been made [original inline comment from piHPSDR src/saturnregisters.h:193]
// this is done so the number of changes to the DDC rates are minimised [original inline comment from piHPSDR src/saturnregisters.h:194]
// and the information all comes form one P2 message anyway. [original inline comment from piHPSDR src/saturnregisters.h:195]
// SetOpenCollectorOutputs(unsigned int bits) [original inline comment from piHPSDR src/saturnregisters.h:200]
// sets the 7 open collector output bits [original inline comment from piHPSDR src/saturnregisters.h:201]
// data must be provided in bits 0-6 [original inline comment from piHPSDR src/saturnregisters.h:202]
// SetADCOptions [original inline comment from piHPSDR src/saturnregisters.h:207]
// sets the ADC contol bits for one ADC [original inline comment from piHPSDR src/saturnregisters.h:208]
// SetDDCFrequency(unsigned int DDC, unsigned int Value, bool IsDeltaPhase) [original inline comment from piHPSDR src/saturnregisters.h:213]
// sets a DDC frequency. [original inline comment from piHPSDR src/saturnregisters.h:214]
// DDC: DDC number (0-9) [original inline comment from piHPSDR src/saturnregisters.h:215]
// Value: 32 bit phase word or frequency word (1Hz resolution) [original inline comment from piHPSDR src/saturnregisters.h:216]
// IsDeltaPhase: true if a delta phase value, false if a frequency value (P1) [original inline comment from piHPSDR src/saturnregisters.h:217]
// SetDUCFrequency(unsigned int Value, bool IsDeltaPhase) [original inline comment from piHPSDR src/saturnregisters.h:222]
// sets a DUC frequency. (Currently only 1 DUC, therefore DUC must be 0) [original inline comment from piHPSDR src/saturnregisters.h:223]
// Value: 32 bit phase word or frequency word (1Hz resolution) [original inline comment from piHPSDR src/saturnregisters.h:224]
// IsDeltaPhase: true if a delta phase value, false if a frequency value (P1) [original inline comment from piHPSDR src/saturnregisters.h:225]
// only accepts DUC=0 [original inline comment from piHPSDR src/saturnregisters.h:227]
// AlexManualRXFilters(unsigned int Bits, int RX) [original inline comment from piHPSDR src/saturnregisters.h:230]
// P2: provides a 16 bit word with all of the Alex settings for a single RX [original inline comment from piHPSDR src/saturnregisters.h:231]
// must be formatted according to the Alex specification [original inline comment from piHPSDR src/saturnregisters.h:232]
// RX=0 or 1: RX1; RX=2: RX2 [original inline comment from piHPSDR src/saturnregisters.h:233]
// AlexManualTXFilters(unsigned int Bits) [original inline comment from piHPSDR src/saturnregisters.h:238]
// P2: provides a 16 bit word with all of the Alex settings for TX [original inline comment from piHPSDR src/saturnregisters.h:239]
// must be formatted according to the Alex specification [original inline comment from piHPSDR src/saturnregisters.h:240]
// FPGA V12 onwards: uses an additional regoster with TX ant settings [original inline comment from piHPSDR src/saturnregisters.h:241]
// HasTXAntExplicitly true if data is for the new TXfilter, TX ant register [original inline comment from piHPSDR src/saturnregisters.h:242]
// SetTXDriveLevel(unsigned int Level) [original inline comment from piHPSDR src/saturnregisters.h:247]
// sets the TX DAC current via a PWM DAC output [original inline comment from piHPSDR src/saturnregisters.h:248]
// level: 0 to 255 drive level value (255 = max current) [original inline comment from piHPSDR src/saturnregisters.h:249]
// EnableLine: true: enable Line input, false: enable Mic input [original inline comment from piHPSDR src/saturnregisters.h:254]
// MicBoost:   true: use 20dB mic boost, false: no boost [original inline comment from piHPSDR src/saturnregisters.h:255]
// LineInGain: LineIn gain vaule [original inline comment from piHPSDR src/saturnregisters.h:256]
// MicBoost has no effect if EnableLine is true [original inline comment from piHPSDR src/saturnregisters.h:258]
// LineInGain has no effect if MicLine is true [original inline comment from piHPSDR src/saturnregisters.h:259]
// SetOrionMicOptions(bool MicRing, bool EnableBias, bool EnablePTT) [original inline comment from piHPSDR src/saturnregisters.h:265]
// sets the microphone control inputs [original inline comment from piHPSDR src/saturnregisters.h:266]
// SetBalancedMicInput(bool Balanced) [original inline comment from piHPSDR src/saturnregisters.h:271]
// selects the balanced microphone input, not supported by current protocol code. [original inline comment from piHPSDR src/saturnregisters.h:272]
// sets the  stepped attenuator on the ADC input of ADC1 and ADC2 [original inline comment from piHPSDR src/saturnregisters.h:277]
// Atten1/2:   a 5 bit atten value [original inline comment from piHPSDR src/saturnregisters.h:278]
// RXAtten1/2: if true, sets atten to be used during RX [original inline comment from piHPSDR src/saturnregisters.h:279]
// TXAtten1/2: if true, sets atten to be used during TX [original inline comment from piHPSDR src/saturnregisters.h:280]
// (it can be both!) [original inline comment from piHPSDR src/saturnregisters.h:281]
//void SetCWIambicKeyer(...) [original inline comment from piHPSDR src/saturnregisters.h:287]
// setup CW iambic keyer parameters [original inline comment from piHPSDR src/saturnregisters.h:288]
// Speed: keyer speed in WPM [original inline comment from piHPSDR src/saturnregisters.h:289]
// weight: typically 50 [original inline comment from piHPSDR src/saturnregisters.h:290]
// ReverseKeys: swaps dot and dash [original inline comment from piHPSDR src/saturnregisters.h:291]
// mode: true if mode B [original inline comment from piHPSDR src/saturnregisters.h:292]
// strictSpacing: true if it enforces character spacing [original inline comment from piHPSDR src/saturnregisters.h:293]
// IambicEnabled: if false, reverts to straight CW key [original inline comment from piHPSDR src/saturnregisters.h:294]
// void SetCWXBits(bool CWXEnabled, bool CWXDash, bool CWXDot) [original inline comment from piHPSDR src/saturnregisters.h:300]
// setup CWX (host generated dot and dash) [original inline comment from piHPSDR src/saturnregisters.h:301]
// SetDDCADC(int DDC, EADCSelect ADC) [original inline comment from piHPSDR src/saturnregisters.h:306]
// sets the ADC to be used for each DDC [original inline comment from piHPSDR src/saturnregisters.h:307]
// DDC = 0 to 9 [original inline comment from piHPSDR src/saturnregisters.h:308]
// void SetRXDDCEnabled(bool IsEnabled); [original inline comment from piHPSDR src/saturnregisters.h:313]
// sets enable bit so DDC operates normally. Resets input FIFO when starting. [original inline comment from piHPSDR src/saturnregisters.h:314]
// EnableCW (bool Enabled, bool Breakin) [original inline comment from piHPSDR src/saturnregisters.h:319]
// enables or disables CW mode; selects CW as modulation source. [original inline comment from piHPSDR src/saturnregisters.h:320]
// If Breakin enabled, the key input engages TX automatically [original inline comment from piHPSDR src/saturnregisters.h:321]
// and generates sidetone. [original inline comment from piHPSDR src/saturnregisters.h:322]
// void SetCWSideTone(bool Enabled, uint8_t Volume, uint16_t Frequency) [original inline comment from piHPSDR src/saturnregisters.h:328]
// enables/disables the side tone and sets volume and frequency [original inline comment from piHPSDR src/saturnregisters.h:329]
// void  SetKeyerParams(uint8_t Delay, uint16_t HangTime, uint8_t Ramp) [original inline comment from piHPSDR src/saturnregisters.h:334]
// sets RF delay, Hang time, and CW pulse ramp length [original inline comment from piHPSDR src/saturnregisters.h:335]
// SetXvtrEnable(bool Enabled) [original inline comment from piHPSDR src/saturnregisters.h:340]
// enables or disables transverter. If enabled, the PA is not keyed. [original inline comment from piHPSDR src/saturnregisters.h:341]
// SetPAEnabled(bool Enabled) [original inline comment from piHPSDR src/saturnregisters.h:346]
// true if PA is enabled. [original inline comment from piHPSDR src/saturnregisters.h:347]
// SetSpkrMute(bool IsMuted) [original inline comment from piHPSDR src/saturnregisters.h:352]
// enables or disables the Codec speaker output [original inline comment from piHPSDR src/saturnregisters.h:353]
// ReadStatusRegister(void) [original inline comment from piHPSDR src/saturnregisters.h:358]
// this is a precursor to getting any of the data itself; simply reads the register to a local variable [original inline comment from piHPSDR src/saturnregisters.h:359]
// probably call every time an outgoig packet is put together initially [original inline comment from piHPSDR src/saturnregisters.h:360]
// but possibly do this one a timed basis. [original inline comment from piHPSDR src/saturnregisters.h:361]
// GetP2PTTKeyInputs(void) [original inline comment from piHPSDR src/saturnregisters.h:366]
// return several bits from Saturn status register: [original inline comment from piHPSDR src/saturnregisters.h:367]
// bit 0 - true if PTT active [original inline comment from piHPSDR src/saturnregisters.h:368]
// bit 1 - true if CW dot input active [original inline comment from piHPSDR src/saturnregisters.h:369]
// bit 2 - true if CW dash input active [original inline comment from piHPSDR src/saturnregisters.h:370]
// bit 4 - true if 10MHz to 122MHz PLL is locked [original inline comment from piHPSDR src/saturnregisters.h:371]
// return true if ADC overflow has occurred since last read. [original inline comment from piHPSDR src/saturnregisters.h:376]
// the overflow stored state is reset when this is read. [original inline comment from piHPSDR src/saturnregisters.h:377]
// returns bit0: 1 if ADC1 overflow; bit1: 1 if ADC2 overflow [original inline comment from piHPSDR src/saturnregisters.h:378]
// In addition, record peak values [original inline comment from piHPSDR src/saturnregisters.h:379]
// GetUserIOBits(void) [original inline comment from piHPSDR src/saturnregisters.h:384]
// return the user input bits [original inline comment from piHPSDR src/saturnregisters.h:385]
// unsigned int GetAnalogueIn(unsigned int AnalogueSelect) [original inline comment from piHPSDR src/saturnregisters.h:390]
// return one of 6 ADC values from the RF board analogue values [original inline comment from piHPSDR src/saturnregisters.h:391]
// the paramter selects which input is read. [original inline comment from piHPSDR src/saturnregisters.h:392]
// CodecInitialise() [original inline comment from piHPSDR src/saturnregisters.h:397]
// initialise the CODEC, with the register values that don't normally change [original inline comment from piHPSDR src/saturnregisters.h:398]
// SetTXAmplitudeScaling (unsigned int Amplitude) [original inline comment from piHPSDR src/saturnregisters.h:403]
// sets the overall TX amplitude. This is normally set to a constant determined during development. [original inline comment from piHPSDR src/saturnregisters.h:404]
// SetTXProtocol2 (void) [original inline comment from piHPSDR src/saturnregisters.h:409]
// configure TX for P2 (192 kHz) [original inline comment from piHPSDR src/saturnregisters.h:410]
// void ResetDUCMux(void) [original inline comment from piHPSDR src/saturnregisters.h:415]
// void SetTXIQDeinterleaved(bool Interleaved) [original inline comment from piHPSDR src/saturnregisters.h:420]
// if true, put DUC hardware in EER mode [original inline comment from piHPSDR src/saturnregisters.h:421]
// this must only be called by the TX datas handler [original inline comment from piHPSDR src/saturnregisters.h:422]
// as it needs ot reset the DUC mux at the same time [original inline comment from piHPSDR src/saturnregisters.h:423]
// void EnableDUCMux(bool Enabled) [original inline comment from piHPSDR src/saturnregisters.h:428]
// enabled the multiplexer to take samples from FIFO and hand on to DUC [original inline comment from piHPSDR src/saturnregisters.h:429]
// // needs to be stoppable if there is an error condition [original inline comment from piHPSDR src/saturnregisters.h:430]
// Function Prototypes
int     openXDMADriver();
void    closeXDMADriver();
quint32 regRead(quint32 Address);
void    regWrite(quint32 Address, quint32 Data);

/*
 * DMA Operations
 * Returns 0 if success, else -EIO
 */
int dmaWriteToFpga(int fd, unsigned char *srcData, quint32 length, quint32 axiAddr);
int dmaReadFromFpga(int fd, unsigned char *destData, quint32 length, quint32 axiAddr);

/*
 * FIFO Monitoring and Control
 */

/**
 * @brief Setup a single FIFO monitor channel.
 * @param channel IP channel number
 * @param enableInterrupt True if interrupt generation enabled for overflows
 */
void setupFifoMonitorChannel(EDMAStreamSelect channel, bool enableInterrupt, RadioInfo rinfo);

/**
 * @brief Read number of locations in a FIFO.
 * @param channel IP core channel number
 * @param overflowed Output: true if an overflow has occurred
 * @param overThreshold Output: true if overflow occurred measured by threshold
 * @param underflowed Output: true if underflow has occurred
 * @param current Output: number of locations occupied
 * @return 16-bit FIFO count (free locations for write channels, occupied for read channels)
 */
quint32 readFifoMonitorChannel(EDMAStreamSelect channel, 
                               bool *overflowed, 
                               bool *overThreshold, 
                               bool *underflowed,
                               unsigned int *current);

/**
 * @brief Reset a stream FIFO.
 * @param ddcNum The DMA stream to reset
 */
void resetDmaStreamFifo(EDMAStreamSelect ddcNum);

//
// quint32 AnalyseDDCHeader(quint32 Header, quint32* DDCCounts)
// parameters are the header read from the DDC stream, and
// a pointer to an array [DDC count] of ints
// the array of ints is populated with the number of samples to read for each DDC
// returns the number of words per frame, which helps set the DMA transfer size
//
quint32 analyseDDCHeader(quint32 Header, quint32* DDCCounts);

/**
 * @brief Initialise the DAC Atten ROMs for power/drive level calculations.
 */
void initialiseDACAttenROMs();

/**
 * @brief Set whether byte swapping is enabled for network byte order.
 */
void setByteSwapping(bool IsSwapped);

/**
 * @brief Sets or clears the TX state and manages the CW keyer state.
 */
void setMOX(bool Mox);

/**
 * @brief Sets or clears the hardware TX enable bit.
 */
void setTXEnable(bool Enabled);

/**
 * @brief Configures sample rate and interleaving for a specific DDC.
 */
void setP2SampleRate(unsigned int DDC, bool Enabled, unsigned int SampleRate, bool InterleaveWithNext);

/**
 * @brief Commits the DDC rate register changes to hardware.
 */
void writeP2DDCRateRegister();

/**
 * @brief Sets the 7 open collector output bits (bits 6:0).
 */
void setOpenCollectorOutputs(unsigned int bits);

/**
 * @brief Configures ADC options (PGA, Dither, Random) for both ADC units.
 */
void setADCOptions(bool PGA1, bool Dither1, bool Random1, bool PGA2, bool Dither2, bool Random2);

/**
 * @brief Sets the frequency for a specific DDC.
 */
void setDDCFrequency(quint32 DDC, quint32 Value, bool IsDeltaPhase);

/**
 * @brief Sets the frequency for the DUC.
 */
void setDUCFrequency(unsigned int Value, bool IsDeltaPhase, RadioInfo rinfo);

/**
 * @brief Provides Alex RX filter settings for a single RX path.
 */
void alexManualRXFilters(unsigned int Bits, int RX);

/**
 * @brief Provides Alex TX filter settings.
 */
void alexManualTXFilters(unsigned int Bits, bool HasTXAntExplicitly);

/**
 * @brief Sets the TX DAC current and step attenuator based on drive level (0-255).
 */
void setTXDriveLevel(unsigned int Level);

/**
 * @brief Configures Codec input parameters (Line vs Mic, Gain, Boost).
 */
void setCodecInputParams(bool EnableLine, bool EnableBoost, int LineInGain);

/**
 * @brief Configures Orion-specific microphone options.
 */
void setOrionMicOptions(bool MicRing, bool EnableBias, bool EnablePTT);

/**
 * @brief Selects the balanced microphone input.
 */
void setBalancedMicInput(bool Balanced);

/**
 * @brief Sets the stepped attenuators on the ADC inputs.
 */
void setADCAttenuator(unsigned int Atten1, bool RXAtten1, bool TXAtten1, 
                      unsigned int Atten2, bool RXAtten2, bool TXAtten2);

/**
 * @brief Sets up CW iambic keyer parameters.
 */
void setCWIambicKeyer(quint8 Speed, quint8 Weight, bool ReverseKeys, bool Mode,
                      bool StrictSpacing, bool IambicEnabled, bool Breakin);

/**
 * @brief Assigns an ADC to a specific DDC.
 */
void setDDCADC(int DDC, EADCSelect ADC);

/**
 * @brief Enables the DDC and resets input FIFOs.
 */
void setRXDDCEnabled(bool IsEnabled);

/**
 * @brief Enables CW mode and selects the modulation source.
 */
void enableCW(bool Enabled, bool Breakin);

/**
 * @brief Configures the CW sidetone (Volume and Frequency).
 */
void setCWSideTone(bool Enabled, quint8 Volume, quint16 Frequency);

/**
 * @brief Sets CW keyer timing parameters (Delay, Hang, Ramp).
 */
void setKeyerParams(quint8 Delay, quint16 HangTime, quint8 Ramp, RadioInfo rinfo);

/**
 * @brief Enables or disables the transverter.
 */
void setXvtrEnable(bool Enabled);

/**
 * @brief Enables or disables the Power Amplifier.
 */
void setPAEnabled(bool Enabled);

/**
 * @brief Controls the Codec speaker mute state.
 */
void setSpkrMute(bool IsMuted);

/**
 * @brief Reads the hardware status register into a local cache.
 */
void readStatusRegister();

/**
 * @brief Returns PTT and Key input states from the status register.
 */
unsigned int getP2PTTKeyInputs();

/**
 * @brief Returns ADC overflow status and max amplitudes.
 */
unsigned int getADCOverflow(quint16 *ADC1Max, quint16 *ADC2Max, RadioInfo rinfo);

/**
 * @brief Returns the user-defined IO input bits.
 */
unsigned int getUserIOBits();

/**
 * @brief Reads one of the analogue input values from the RF board.
 */
unsigned int getAnalogueIn(unsigned int AnalogueSelect);

/**
 * @brief Detects and initialises the onboard audio Codec.
 */
void codecInitialise(RadioInfo info);

/**
 * @brief Sets overall TX amplitude scaling.
 */
void setTXAmplitudeScaling(unsigned int Amplitude);

/**
 * @brief Configures the TX chain for Protocol 2.
 */
void setTXProtocol2();

/**
 * @brief Resets the DUC multiplexer.
 */
void resetDUCMux();

/**
 * @brief Sets the DUC hardware into EER (deinterleaved) mode.
 */
void setTXIQDeinterleaved(bool Interleaved);

/**
 * @brief Enables the multiplexer to feed the DUC from the FIFO.
 */
void enableDUCMux(bool Enabled);

} // namespace NereusSDR
#endif // SATURNXDMA_H
