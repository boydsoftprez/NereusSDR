#pragma once

#include "RadioConnection.h"
#include "BoardCapabilities.h"

#include <QUdpSocket>
#include <QTimer>
#include <QVector>

#include <array>

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

private slots:
    void onReadyRead();
    void onKeepAliveTick();
    void onReconnectTimeout();

private:
    // --- Command senders (ported from Thetis network.c) ---
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

    // Compute Alex HPF bits based on frequency.
    // Ported from Thetis console.cs:6830-6942 setAlexHPF
    static int computeAlexHpf(double freqMhz);

    // Compute Alex LPF bits based on frequency.
    // Ported from Thetis console.cs:7168-7234 setAlexLPF
    static int computeAlexLpf(double freqMhz);

    // Build Alex0 and Alex1 32-bit register values from current state.
    quint32 buildAlex0() const;
    quint32 buildAlex1() const;

    // --- DDC→ADC mapping register (from Thetis network.c rx_adc_ctrl1) ---
    quint32 m_rxAdcCtrl1{0};

    // --- I/Q buffers and packet counters ---
    std::array<QVector<float>, kMaxDdc> m_iqBuffers;
    int m_totalIqPackets{0};
};

} // namespace NereusSDR
