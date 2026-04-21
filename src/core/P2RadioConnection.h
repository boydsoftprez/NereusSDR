#pragma once

// =================================================================
// src/core/P2RadioConnection.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/ChannelMaster/network.c, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/network.h, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/netInterface.c, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

/*
 * network.c
 * Copyright (C) 2015-2020 Doug Wigley (W5WC)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*  network.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2015-2020 Doug Wigley, W5WC

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

/*
 * netinterface.c
 * Copyright (C) 2006,2007  Bill Tracey (bill@ejwt.com) (KD5TFD)
 * Copyright (C) 2010-2020 Doug Wigley (W5WC)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

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

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#include "RadioConnection.h"
#include "BoardCapabilities.h"

#include <QUdpSocket>
#include <QTimer>
#include <QVector>

#include <array>
#include <memory>

#include "codec/IP2Codec.h"
#include "codec/CodecContext.h"

namespace NereusSDR {

// Protocol 2 connection for Orion MkII / Saturn (ANAN-G2) radios.
//
// Faithfully ported from Thetis ChannelMaster/network.c and network.h.
// Uses a single UDP socket matching Thetis listenSock.
// State structs mirror Thetis _radionet (network.h:53).
class P2RadioConnection : public RadioConnection {
    Q_OBJECT

public:
    explicit P2RadioConnection(QObject* parent = nullptr);
    ~P2RadioConnection() override;

    int getAdcForDdc(int ddc) const override;

public slots:
    void init() override;
    void connectToRadio(const NereusSDR::RadioInfo& info) override;
    void disconnect() override;

    void setReceiverFrequency(int receiverIndex, quint64 frequencyHz) override;
    void setTxFrequency(quint64 frequencyHz) override;
    void setActiveReceiverCount(int count) override;
    void setSampleRate(int sampleRate) override;
    void setAttenuator(int dB) override;
    void setPreamp(bool enabled) override;
    void setTxDrive(int level) override;
    void setMox(bool enabled) override;
    void setAntenna(int antennaIndex) override;

    // Phase 3P-B Task 10: per-ADC RX1 preamp control for OrionMKII family.
    // Routes to m_rx[1].preamp → CodecContext.p2Rx1Preamp →
    // P2CodecOrionMkII::composeCmdHighPriority byte 1403 bit 1.
    // ADC0 preamp uses the existing setPreamp(bool) (byte 1403 bit 0).
    void setRx1Preamp(bool enabled);

private slots:
    void onReadyRead();
    void onKeepAliveTick();
    void onReconnectTimeout();

private:
    // --- Phase 3P-B: per-board codec chosen at connectToRadio() time ---
    std::unique_ptr<IP2Codec> m_codec;
    bool m_useLegacyP2Codec{false};

    void selectCodec();
    CodecContext buildCodecContext() const;

    // Legacy compose paths — preserved for the NEREUS_USE_LEGACY_P2_CODEC rollback flag.
    void composeCmdGeneralLegacy     (char buf[60])   const;
    void composeCmdHighPriorityLegacy(char buf[1444]) const;
    void composeCmdRxLegacy          (char buf[1444]) const;
    void composeCmdTxLegacy          (char buf[60])   const;

    // --- Command composers (extract buffer-fill logic; sendCmd* calls these then UDP-sends) ---
    void composeCmdGeneral(char buf[60]) const;               // network.c:821
    void composeCmdHighPriority(char buf[1444]) const;        // network.c:913  (1444 == kBufLen)
    void composeCmdRx(char buf[1444]) const;                  // network.c:1066 (1444 == kBufLen)
    void composeCmdTx(char buf[60]) const;                    // network.c:1181

    // --- Command senders (compose + UDP dispatch) ---
    void sendCmdGeneral();       // network.c:821 → port 1024
    void sendCmdHighPriority();  // network.c:913 → port 1027
    void sendCmdRx();            // network.c:1066 → port 1025
    void sendCmdTx();            // network.c:1181 → port 1026

    void processIqPacket(const QByteArray& data, int ddcIndex);
    void processHighPriorityStatus(const QByteArray& data);

    static void writeBE32(char* buf, int offset, quint32 value);

    // From pcap analysis: phase_word = freq_hz * 2^32 / 122880000
    // General cmd byte 37 bit 3 = 1 means radio expects phase words, not Hz.
    static quint32 hzToPhaseWord(quint64 freqHz);

    // --- Constants from Thetis network.h ---
    static constexpr int kMaxRxStreams = 12;  // network.h:34 MAX_RX_STREAMS
    static constexpr int kMaxTxStreams = 3;   // network.h:35 MAX_TX_STREAMS
    static constexpr int kMaxAdc = 3;         // network.h:33 MAX_ADC
    static constexpr int kMaxDdc = 7;         // DDC0-DDC6
    static constexpr int kBufLen = 1444;      // Thetis BUFLEN
    static constexpr int kKeepAliveIntervalMs = 500; // network.c:1428

    // --- Board capabilities (set in connectToRadio, used for clamp/dispatch) ---
    const BoardCapabilities* m_caps{nullptr};

    // --- Single socket (Thetis listenSock, network.c:67) ---
    QUdpSocket* m_socket{nullptr};
    QTimer* m_keepAliveTimer{nullptr};
    QTimer* m_reconnectTimer{nullptr};
    QTimer* m_txIqTimer{nullptr};

    // --- Port configuration (from Thetis _radionet, network.h:55-56) ---
    int m_p2CustomPortBase{1025};    // prn->p2_custom_port_base
    int m_baseOutboundPort{1024};    // prn->base_outbound_port

    // --- Run state (from Thetis _radionet, network.h:65-66) ---
    bool m_running{false};           // prn->run
    int m_wdt{0};                    // prn->wdt (watchdog timer, 0=disabled)
    bool m_intentionalDisconnect{false};

    // --- Hardware config (from Thetis _radionet) ---
    int m_numAdc{1};                 // prn->num_adc
    int m_numDac{1};                 // prn->num_dac

    // --- Sequence counters ---
    quint32 m_seqGeneral{0};
    quint32 m_seqRx{0};
    quint32 m_seqTx{0};
    quint32 m_seqTxIq{0};
    quint32 m_seqHighPri{0};
    quint32 m_ccSeqNo{0};            // prn->cc_seq_no

    // --- RX state (from Thetis _radionet._rx, network.h:191-213) ---
    struct RxState {
        int id{0};
        int rxAdc{0};                // prn->rx[i].rx_adc
        int frequency{0};            // prn->rx[i].frequency (Hz)
        int enable{0};               // prn->rx[i].enable
        int sync{0};                 // prn->rx[i].sync
        int samplingRate{48};        // prn->rx[i].sampling_rate (kHz value)
        int bitDepth{24};            // prn->rx[i].bit_depth
        int preamp{0};               // prn->rx[i].preamp
        int spp{238};                // prn->rx[i].spp (IQ-samples per packet)
        quint32 rxInSeqNo{0};        // prn->rx[i].rx_in_seq_no
        quint32 rxInSeqErr{0};       // prn->rx[i].rx_in_seq_err
    };
    std::array<RxState, kMaxRxStreams> m_rx;

    // --- TX state (from Thetis _radionet._tx, network.h:215-236) ---
    struct TxState {
        int id{0};
        int frequency{0};            // prn->tx[i].frequency (Hz)
        int samplingRate{192};       // From Thetis create_rnet (netInterface.c:1513) — P2 always 192
        int cwx{0};                  // prn->tx[i].cwx
        int dash{0};
        int dot{0};
        int pttOut{0};               // prn->tx[i].ptt_out
        int driveLevel{0};           // prn->tx[i].drive_level
        int phaseShift{0};           // prn->tx[i].phase_shift
        int pa{1};                   // prn->tx[i].pa
        int epwmMax{0};
        int epwmMin{0};
    };
    std::array<TxState, kMaxTxStreams> m_tx;

    // --- ADC state (from Thetis _radionet._adc, network.h:125-140) ---
    struct AdcState {
        int rxStepAttn{0};           // prn->adc[i].rx_step_attn
        int txStepAttn{31};          // prn->adc[i].tx_step_attn (default 31 from create_rnet:1472)
        int dither{0};
        int random{0};
    };
    std::array<AdcState, kMaxAdc> m_adc;

    // --- CW state (from Thetis _radionet._cw, network.h:142-167) ---
    struct CwState {
        int sidetoneLevel{0};
        int sidetoneFreq{0};
        int keyerSpeed{0};
        int keyerWeight{0};
        int hangDelay{0};
        int rfDelay{0};
        int edgeLength{7};           // From create_rnet:1454
        unsigned char modeControl{0};
    };
    CwState m_cw;

    // --- Mic state (from Thetis _radionet._mic, network.h:169-189) ---
    // mic_control bit-field (from Thetis network.c:1227-1233):
    //   Bit 0: Line In (0=off, 1=on)
    //   Bit 1: Mic Boost (0=off, 1=on)
    //   Bit 2: Orion Mic PTT (0=enabled, 1=disabled)
    //   Bit 3: Tip/Ring (0=ptt-ring/mic-tip, 1=ptt-tip/mic-ring)
    //   Bit 4: Mic Bias (0=disabled, 1=enabled)
    //   Bit 5: Balanced Input (0=disabled, 1=enabled, Saturn only)
    struct MicState {
        unsigned char micControl{0};
        int lineInGain{0};
    };
    MicState m_mic;

    // --- Wideband settings (from Thetis create_rnet:1461-1466) ---
    int m_wbSamplesPerPacket{512};
    int m_wbSampleSize{16};
    int m_wbUpdateRate{70};
    int m_wbPacketsPerFrame{32};

    // --- Alex filter/antenna state ---
    // From Thetis ChannelMaster/network.h bpfilter struct
    // Each Alex register is a 32-bit value written to CmdHighPriority bytes 1428-1435.
    // Alex0 (bytes 1432-1435): RX antenna, HPF bits, LPF bits
    // Alex1 (bytes 1428-1431): TX antenna, HPF/LPF bits
    struct AlexState {
        // Antenna selection — from Thetis netInterface.c:459-499 SetAntBits
        int rxAnt{1};       // 1=ANT1, 2=ANT2, 3=ANT3
        int txAnt{1};       // 1=ANT1, 2=ANT2, 3=ANT3
        int hpfBits{0x20};  // HPF filter bits (default: bypass = 0x20)
        int lpfBits{0x10};  // LPF filter bits (default: 6m LPF = 0x10)
    };
    AlexState m_alex;

    // Build Alex0 and Alex1 32-bit register values from current state.
    quint32 buildAlex0() const;
    quint32 buildAlex1() const;

    // --- DDC→ADC mapping register (from Thetis network.c rx_adc_ctrl1) ---
    quint32 m_rxAdcCtrl1{0};

    // --- I/Q buffers and packet counters ---
    std::array<QVector<float>, kMaxDdc> m_iqBuffers;
    int m_totalIqPackets{0};

#ifdef NEREUS_BUILD_TESTS
public:
    // Test-only helpers — allow unit tests to inject board state without a live radio.
    void setBoardForTest(HPSDRHW board) {
        m_caps = &BoardCapsTable::forBoard(board);
        switch (board) {
            case HPSDRHW::OrionMKII:  m_hardwareProfile.model = HPSDRModel::ORIONMKII; break;
            case HPSDRHW::Saturn:     m_hardwareProfile.model = HPSDRModel::ANAN_G2;   break;
            case HPSDRHW::SaturnMKII: m_hardwareProfile.model = HPSDRModel::ANAN_G2;   break;
            default:                  m_hardwareProfile.model = HPSDRModel::ORIONMKII; break;
        }
        m_hardwareProfile.effectiveBoard = board;
        selectCodec();
    }
    // Expose compose methods for regression-freeze capture (Task 1) and gate test (Task 7).
    void composeCmdGeneralForTest(quint8 buf[60]) const {
        char tmp[60];
        memset(tmp, 0, sizeof(tmp));
        composeCmdGeneral(tmp);
        memcpy(buf, tmp, 60);
    }
    void composeCmdHighPriorityForTest(quint8 buf[kBufLen]) const {
        char tmp[kBufLen];
        memset(tmp, 0, sizeof(tmp));
        composeCmdHighPriority(tmp);
        memcpy(buf, tmp, kBufLen);
    }
    void composeCmdRxForTest(quint8 buf[kBufLen]) const {
        char tmp[kBufLen];
        memset(tmp, 0, sizeof(tmp));
        composeCmdRx(tmp);
        memcpy(buf, tmp, kBufLen);
    }
    void composeCmdTxForTest(quint8 buf[60]) const {
        char tmp[60];
        memset(tmp, 0, sizeof(tmp));
        composeCmdTx(tmp);
        memcpy(buf, tmp, 60);
    }
#endif
};

} // namespace NereusSDR
