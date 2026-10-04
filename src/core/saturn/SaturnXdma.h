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
