// Porting from Thetis ChannelMaster/network.c
// Functions: nativeInitMetis, SendStart, SendStop, CmdGeneral, CmdRx, CmdTx,
//            CmdHighPriority, ReadUDPFrame, ReadThreadMainLoop, sendPacket,
//            KeepAliveLoop
// Struct: _radionet (network.h:53)
// Init: create_rnet (netInterface.c:1416)

#include "P2RadioConnection.h"
#include "LogCategories.h"

#include <QNetworkDatagram>
#include <QVariant>
#include <QtEndian>

namespace NereusSDR {

P2RadioConnection::P2RadioConnection(QObject* parent)
    : RadioConnection(parent)
{
    // From Thetis create_rnet() netInterface.c:1416
    // Initialize rx state with Thetis defaults
    for (int i = 0; i < kMaxRxStreams; ++i) {
        m_rx[i].id = i;
        m_rx[i].rxAdc = 0;
        m_rx[i].frequency = 0;
        m_rx[i].enable = 0;
        m_rx[i].sync = 0;
        m_rx[i].samplingRate = 48;     // From Thetis create_rnet:1488
        m_rx[i].bitDepth = 24;         // From Thetis create_rnet:1489
        m_rx[i].preamp = 0;
        m_rx[i].spp = 238;             // From Thetis create_rnet:1496
        m_rx[i].rxInSeqNo = 0;
        m_rx[i].rxInSeqErr = 0;
    }

    // From Thetis create_rnet() netInterface.c:1504-1514
    for (int i = 0; i < kMaxTxStreams; ++i) {
        m_tx[i].id = i;
        m_tx[i].frequency = 0;
        m_tx[i].samplingRate = 48;
        m_tx[i].cwx = 0;
        m_tx[i].dash = 0;
        m_tx[i].dot = 0;
        m_tx[i].pttOut = 0;
        m_tx[i].driveLevel = 0;
        m_tx[i].phaseShift = 0;
        m_tx[i].pa = 1;
        m_tx[i].epwmMax = 0;
        m_tx[i].epwmMin = 0;
    }
}

P2RadioConnection::~P2RadioConnection()
{
    if (m_running) {
        disconnect();
    }
}

// --- Thread Lifecycle ---
// Porting from Thetis nativeInitMetis() network.c:84
// Creates a single UDP socket, matching Thetis listenSock

void P2RadioConnection::init()
{
    m_socket = new QUdpSocket(this);

    // From Thetis nativeInitMetis:203 — bind to any available port
    if (!m_socket->bind(QHostAddress::Any, 0)) {
        qCWarning(lcConnection) << "P2: Failed to bind UDP socket";
        return;
    }

    // From Thetis nativeInitMetis:163-194 — socket buffer sizing
    // const int sndbuf_bytes = 0xfa000; const int rcvbuf_bytes = 0xfa000;
    m_socket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption,
                              QVariant(0xfa000));
    m_socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,
                              QVariant(0xfa000));

    connect(m_socket, &QUdpSocket::readyRead, this, &P2RadioConnection::onReadyRead);

    // From Thetis KeepAliveLoop network.c:1428 — timer fires every 500ms
    m_keepAliveTimer = new QTimer(this);
    m_keepAliveTimer->setInterval(kKeepAliveIntervalMs);
    connect(m_keepAliveTimer, &QTimer::timeout, this, &P2RadioConnection::onKeepAliveTick);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setInterval(3000);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &P2RadioConnection::onReconnectTimeout);

    // TX I/Q silence stream — radio expects continuous TX data on port 1029
    // even in RX-only mode. From pcap: ~800 packets/sec at 192 kHz / 240 spp.
    m_txIqTimer = new QTimer(this);
    m_txIqTimer->setInterval(5);  // ~200 packets/sec (close enough, radio buffers)
    connect(m_txIqTimer, &QTimer::timeout, this, [this]() {
        if (!m_running || !m_socket) {
            return;
        }
        // 4-byte seq + 240 I/Q pairs × 6 bytes (24-bit I + 24-bit Q) = 1444 bytes
        static constexpr int kTxPktLen = 4 + 240 * 6;
        char buf[kTxPktLen];
        memset(buf, 0, sizeof(buf));
        writeBE32(buf, 0, m_seqTxIq++);
        QByteArray pkt(buf, sizeof(buf));
        m_socket->writeDatagram(pkt, m_radioInfo.address, m_baseOutboundPort + 4);
    });

    qCDebug(lcConnection) << "P2: init() socket port:" << m_socket->localPort();
}

// --- Connection Lifecycle ---
// Porting from Thetis SendStart() network.c:362
// prn->run = 1; CmdGeneral(); CmdRx(); CmdTx(); CmdHighPriority();

void P2RadioConnection::connectToRadio(const RadioInfo& info)
{
    if (m_running) {
        disconnect();
    }

    m_radioInfo = info;
    m_intentionalDisconnect = false;
    m_totalIqPackets = 0;

    // Cache board capabilities — drives attenuator clamp, ADC count, etc.
    m_caps = &BoardCapsTable::forBoard(info.boardType);

    // Reset sequence counters
    m_seqGeneral = 0;
    m_seqRx = 0;
    m_seqTx = 0;
    m_seqHighPri = 0;
    m_ccSeqNo = 0;

    // From Thetis create_rnet defaults + Thetis pcap analysis
    // Use m_caps->adcCount instead of info.adcCount for capability-metadata consistency.
    // Both carry the same value (info.adcCount is populated from adcCountForBoard() which
    // reads the same BoardCapsTable). m_caps is authoritative.
    m_numAdc = m_caps->adcCount;
    m_numDac = 1;
    m_wdt = 1;  // Watchdog timer MUST be enabled — radio requires it for streaming

    // From Thetis console.cs:8216 UpdateDDCs() for ANAN-G2 (OrionMkII/Saturn)
    // In non-diversity, non-PureSignal RX mode:
    //   DDCEnable = DDC2 (bit 2), Rate[2] = rx1_rate
    // This means DDC2 is the primary receiver, not DDC0!
    // From Thetis console.cs:8234-8241
    m_rx[2].enable = 1;
    m_rx[2].frequency = 3865000;   // 80m LSB — matching Phase 3D verified config
    m_rx[2].samplingRate = 768;
    // Note: pcap shows Thetis uses 768 kHz for wider spectrum. To enable that,
    // WDSP must also be configured for 768 kHz input (future work).

    // From pcap: Thetis enables dither and random on all ADCs
    m_adc[0].dither = 1;
    m_adc[1].dither = 1;
    m_adc[2].dither = 1;
    m_adc[0].random = 1;
    m_adc[1].random = 1;
    m_adc[2].random = 1;

    // TX frequency — overridden by SliceModel via setTxFrequency (simplex: TX=RX)
    m_tx[0].frequency = 3865000;

    setState(ConnectionState::Connecting);

    qCDebug(lcConnection) << "P2: Connecting to" << info.displayName()
                          << "at" << info.address.toString()
                          << "from port" << m_socket->localPort();

    // From Thetis SendStart() network.c:362-369
    m_running = true;         // prn->run = 1;
    sendCmdGeneral();         // CmdGeneral(); //1024
    sendCmdRx();              // CmdRx(); //1025
    sendCmdTx();              // CmdTx(); //1026
    sendCmdHighPriority();    // CmdHighPriority(); //1027

    qCDebug(lcConnection) << "P2: SendStart complete (run=1)";

    // From Thetis StartAudioNative netInterface.c:83
    // prn->hKeepAliveThread = _beginthreadex(NULL, 0, KeepAliveMain, 0, 0, NULL);
    m_keepAliveTimer->start();
    // m_txIqTimer->start();  // DISABLED: testing if TX silence stream causes weak signals

    setState(ConnectionState::Connected);
}

// Porting from Thetis SendStop() network.c:372
// prn->run = 0; CmdHighPriority();

void P2RadioConnection::disconnect()
{
    m_intentionalDisconnect = true;

    if (m_keepAliveTimer) {
        m_keepAliveTimer->stop();
    }
    if (m_txIqTimer) {
        m_txIqTimer->stop();
    }
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
    }

    if (m_running && m_socket && !m_radioInfo.address.isNull()) {
        // From Thetis SendStop() network.c:372-376
        m_running = false;       // prn->run = 0;
        sendCmdHighPriority();   // CmdHighPriority();
        qCDebug(lcConnection) << "P2: SendStop complete (run=0)";
    }

    m_running = false;

    if (m_socket) {
        m_socket->close();
    }

    setState(ConnectionState::Disconnected);
    qCDebug(lcConnection) << "P2: Disconnected. I/Q packets:" << m_totalIqPackets;
}

// --- Hardware Control Slots ---

void P2RadioConnection::setReceiverFrequency(int receiverIndex, quint64 frequencyHz)
{
    if (receiverIndex < 0 || receiverIndex >= kMaxRxStreams) {
        return;
    }
    m_rx[receiverIndex].frequency = static_cast<int>(frequencyHz);

    // Update Alex HPF/LPF based on new frequency
    // From Thetis console.cs:6830-7234 — auto-select band filters
    double freqMhz = frequencyHz / 1e6;
    m_alex.hpfBits = computeAlexHpf(freqMhz);
    m_alex.lpfBits = computeAlexLpf(freqMhz);

    if (m_running) {
        sendCmdHighPriority();
    }
}

void P2RadioConnection::setTxFrequency(quint64 frequencyHz)
{
    m_tx[0].frequency = static_cast<int>(frequencyHz);
    if (m_running) {
        sendCmdHighPriority();
    }
}

void P2RadioConnection::setActiveReceiverCount(int count)
{
    // Clamp to board-reported maximum if caps are available.
    // kMaxRxStreams (12) is the wire-protocol ceiling; board caps may be lower.
    const int maxRx = m_caps ? m_caps->maxReceivers : kMaxRxStreams;
    const int clamped = qBound(1, count, maxRx);
    for (int i = 0; i < kMaxRxStreams; ++i) {
        m_rx[i].enable = (i < clamped) ? 1 : 0;
    }
    if (m_running) {
        sendCmdRx();
    }
}

void P2RadioConnection::setSampleRate(int sampleRate)
{
    // From Thetis: sampling_rate stored as kHz value (48, 96, 192, 384)
    int rateKhz = sampleRate / 1000;
    for (int i = 0; i < kMaxRxStreams; ++i) {
        m_rx[i].samplingRate = rateKhz;
    }
    m_tx[0].samplingRate = rateKhz;
    if (m_running) {
        sendCmdRx();
        sendCmdTx();
    }
}

void P2RadioConnection::setAttenuator(int dB)
{
    // From Thetis: prn->adc[0].rx_step_attn
    // Clamp to board-specific attenuator range from BoardCapabilities.
    // Saturn/SaturnMKII: minDb=0, maxDb=31, stepDb=1 (kSaturn in BoardCapabilities.cpp).
    // Fallback to [0, 31] if m_caps is not yet set (should not occur in normal flow).
    const int minDb = m_caps ? m_caps->attenuator.minDb : 0;
    const int maxDb = m_caps ? m_caps->attenuator.maxDb : 31;
    m_adc[0].rxStepAttn = qBound(minDb, dB, maxDb);
    if (m_running) {
        sendCmdHighPriority();
    }
}

void P2RadioConnection::setPreamp(bool enabled)
{
    // From Thetis: prn->rx[0].preamp
    m_rx[0].preamp = enabled ? 1 : 0;
    if (m_running) {
        sendCmdHighPriority();
    }
}

void P2RadioConnection::setTxDrive(int level)
{
    // From Thetis: prn->tx[0].drive_level
    m_tx[0].driveLevel = qBound(0, level, 255);
    if (m_running) {
        sendCmdHighPriority();
    }
}

void P2RadioConnection::setMox(bool enabled)
{
    // From Thetis: prn->tx[0].ptt_out
    m_tx[0].pttOut = enabled ? 1 : 0;
    if (m_running) {
        sendCmdHighPriority();
    }
}

void P2RadioConnection::setAntenna(int antennaIndex)
{
    // antennaIndex: 0=ANT1, 1=ANT2, 2=ANT3
    m_alex.rxAnt = antennaIndex + 1;  // 1-based for Alex register encoding
    m_alex.txAnt = antennaIndex + 1;
    if (m_running) {
        sendCmdHighPriority();
    }
}

// --- UDP Reception ---
// Porting from Thetis ReadUDPFrame() network.c:481
// Single socket, dispatch by source port: inport = ntohs(fromaddr.sin_port)

void P2RadioConnection::onReadyRead()
{
    while (m_socket && m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        QByteArray data = datagram.data();
        quint16 sourcePort = datagram.senderPort();

        // From Thetis ReadUDPFrame:514-515
        // inport = ntohs(fromaddr.sin_port);
        // int portIdx = inport - prn->p2_custom_port_base;
        int portIdx = sourcePort - m_p2CustomPortBase;

        // Filter out spurious empty datagrams (Windows loopback from our own sends)
        if (data.isEmpty()) {
            continue;
        }

        // Debug: log first 5 real packets
        static int debugCount = 0;
        if (debugCount < 5) {
            qCDebug(lcConnection) << "P2: UDP packet: port" << sourcePort
                                  << "idx" << portIdx << "size" << data.size()
                                  << "from" << datagram.senderAddress().toString();
            ++debugCount;
        }

        // From Thetis ReadUDPFrame:517-637 switch(portIdx)
        switch (portIdx) {
        case 0:  // 1025: 60 bytes - High Priority C&C data
            // From Thetis ReadUDPFrame:519-532
            if (data.size() == 60) {
                processHighPriorityStatus(data);
            }
            break;

        case 1:  // 1026: 132 bytes - Mic samples
            // From Thetis ReadUDPFrame:534-548 (not used yet)
            break;

        case 2:  // 1027: wideband ADC data
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
            // From Thetis ReadUDPFrame:550-603 (wideband, not used yet)
            break;

        case 10: // 1035: DDC0 I/Q
        case 11: // 1036: DDC1
        case 12: // 1037: DDC2
        case 13: // 1038: DDC3
        case 14: // 1039: DDC4
        case 15: // 1040: DDC5
        case 16: // 1041: DDC6
        {
            // From Thetis ReadUDPFrame:605-631
            if (data.size() != 1444) {
                break;  // check for malformed packet
            }
            int ddc = portIdx - 10;
            processIqPacket(data, ddc);
            break;
        }

        default:
            // From Thetis ReadUDPFrame:633-635
            qCDebug(lcConnection) << "P2: Data on port" << sourcePort
                                  << "portIdx" << portIdx
                                  << "size" << data.size()
                                  << "from" << datagram.senderAddress().toString();
            break;
        }
    }
}

// From Thetis KeepAliveLoop network.c:1417-1440
// Fires every 500ms, sends CmdGeneral when running
void P2RadioConnection::onKeepAliveTick()
{
    // From Thetis network.c:1436
    // if (prn->run && prn->wdt) CmdGeneral();
    // Note: we send CmdGeneral unconditionally when running (wdt=0 means no watchdog,
    // but keepalive still runs per Thetis behavior)
    if (m_running && !m_radioInfo.address.isNull()) {
        sendCmdGeneral();
    }
}

void P2RadioConnection::onReconnectTimeout()
{
    if (!m_intentionalDisconnect && !m_radioInfo.address.isNull()) {
        qCDebug(lcConnection) << "P2: Reconnecting to" << m_radioInfo.displayName();
        connectToRadio(m_radioInfo);
    }
}

// --- Command Senders ---
// Each ported byte-for-byte from Thetis CmdGeneral/CmdRx/CmdTx/CmdHighPriority

// Porting from Thetis CmdGeneral() network.c:821-911
void P2RadioConnection::sendCmdGeneral()
{
    char buf[60];
    memset(buf, 0, sizeof(buf));

    // From Thetis network.c:826
    buf[4] = 0x00;  // Command

    // From Thetis network.c:831-876 — PORT assignments
    int tmp;

    // PC outbound source ports (radio receives FROM these)
    // From Thetis network.c:839-857
    tmp = m_p2CustomPortBase + 0;  // Rx Specific #1025
    buf[5] = tmp >> 8; buf[6] = tmp & 0xff;
    tmp = m_p2CustomPortBase + 1;  // Tx Specific #1026
    buf[7] = tmp >> 8; buf[8] = tmp & 0xff;
    tmp = m_p2CustomPortBase + 2;  // High Priority from PC #1027
    buf[9] = tmp >> 8; buf[10] = tmp & 0xff;
    tmp = m_p2CustomPortBase + 3;  // Rx Audio #1028
    buf[13] = tmp >> 8; buf[14] = tmp & 0xff;
    tmp = m_p2CustomPortBase + 4;  // Tx0 IQ #1029
    buf[15] = tmp >> 8; buf[16] = tmp & 0xff;

    // Radio outbound source ports (radio sends FROM these)
    // From Thetis network.c:860-875
    tmp = m_p2CustomPortBase + 0;  // High Priority to PC #1025
    buf[11] = tmp >> 8; buf[12] = tmp & 0xff;
    tmp = m_p2CustomPortBase + 10; // Rx0 DDC IQ #1035
    buf[17] = tmp >> 8; buf[18] = tmp & 0xff;
    tmp = m_p2CustomPortBase + 1;  // Mic Samples #1026
    buf[19] = tmp >> 8; buf[20] = tmp & 0xff;
    tmp = m_p2CustomPortBase + 2;  // Wideband ADC0 #1027
    buf[21] = tmp >> 8; buf[22] = tmp & 0xff;

    // From Thetis network.c:878-888 — Wideband settings
    buf[23] = 0;    // wb_enable
    buf[24] = (m_wbSamplesPerPacket >> 8) & 0xff;
    buf[25] = m_wbSamplesPerPacket & 0xff;
    buf[26] = m_wbSampleSize;      // 16 bits
    buf[27] = m_wbUpdateRate;      // 70ms
    buf[28] = m_wbPacketsPerFrame; // 32

    // From Thetis network.c:896 — 0x08 = bit[3] "freq or phase word"
    // Thetis sends 0x08 but stores frequencies as Hz in prn->rx[].frequency
    // Keep this matching Thetis exactly
    buf[37] = 0x08;

    // From Thetis network.c:898
    buf[38] = m_wdt;  // Watchdog timer (0 = disabled)

    // From Thetis network.c:904
    buf[58] = (!m_tx[0].pa) & 0x01;  // PA enable

    // From Thetis network.c:906 — Alex enable (BPF board)
    // prbpfilter->enable | prbpfilter2->enable
    buf[59] = 0x03;  // Enable both Alex0 and Alex1

    // Sequence number (bytes 0-3 big-endian)
    // From Thetis sendPacket — seq is in the packet buffer
    writeBE32(buf, 0, m_seqGeneral++);

    // From Thetis network.c:910
    // sendPacket(listenSock, packetbuf, sizeof(packetbuf), prn->base_outbound_port);
    QByteArray pkt(buf, sizeof(buf));
    m_socket->writeDatagram(pkt, m_radioInfo.address, m_baseOutboundPort);
}

// Porting from Thetis CmdHighPriority() network.c:913-1063
void P2RadioConnection::sendCmdHighPriority()
{
    char buf[kBufLen];
    memset(buf, 0, sizeof(buf));

    // Sequence number
    writeBE32(buf, 0, m_seqHighPri++);

    // From Thetis network.c:924-925
    // packetbuf[4] = (prn->tx[0].ptt_out << 1 | prn->run) & 0xff;
    buf[4] = (m_tx[0].pttOut << 1 | (m_running ? 1 : 0)) & 0xff;

    // From Thetis network.c:931-933
    buf[5] = (m_tx[0].dash << 2 | m_tx[0].dot << 1 | m_tx[0].cwx) & 0x7;

    // From Thetis network.c:936-1005
    // RX frequencies — 4 bytes each, big-endian phase words.
    // General cmd byte 37 = 0x08 (bit 3) means frequencies are NCO phase words.
    // From pcap analysis: phase_word = freq_hz * 2^32 / 122880000
    // RX0-RX1 have PureSignal override logic; for now use straight frequency
    for (int i = 0; i < kMaxRxStreams; ++i) {
        int offset = 9 + (i * 4);
        if (offset + 3 < kBufLen) {
            quint32 phaseWord = hzToPhaseWord(m_rx[i].frequency);
            writeBE32(buf, offset, phaseWord);
        }
    }

    // From Thetis network.c:1008-1011 — TX0 frequency (also phase word)
    writeBE32(buf, 329, hzToPhaseWord(m_tx[0].frequency));

    // From Thetis network.c:1014
    buf[345] = m_tx[0].driveLevel;

    // From Thetis network.c:1037-1038 — Mercury Attenuator
    buf[1403] = m_rx[1].preamp << 1 | m_rx[0].preamp;

    // From Thetis network.c:1055-1057 — Step Attenuators
    buf[1442] = m_adc[1].rxStepAttn;
    buf[1443] = m_adc[0].rxStepAttn;

    // Alex filter/antenna registers (bytes 1428-1435)
    // From Thetis ChannelMaster/network.c:1040-1050
    // Alex0 (bytes 1432-1435): RX antenna + HPF + LPF
    // Alex1 (bytes 1428-1431): TX antenna + HPF + LPF
    writeBE32(buf, 1432, buildAlex0());
    writeBE32(buf, 1428, buildAlex1());

    // From Thetis network.c:1062
    // sendPacket(listenSock, packetbuf, BUFLEN, prn->base_outbound_port + 3);
    QByteArray pkt(buf, sizeof(buf));
    m_socket->writeDatagram(pkt, m_radioInfo.address, m_baseOutboundPort + 3);
}

// Porting from Thetis CmdRx() network.c:1066-1179
void P2RadioConnection::sendCmdRx()
{
    char buf[kBufLen];
    memset(buf, 0, sizeof(buf));

    writeBE32(buf, 0, m_seqRx++);

    // From Thetis network.c:1074
    buf[4] = m_numAdc;

    // From Thetis network.c:1080-1082 — Dither
    buf[5] = (m_adc[2].dither << 2 | m_adc[1].dither << 1 | m_adc[0].dither) & 0x7;

    // From Thetis network.c:1088-1090 — Random
    buf[6] = (m_adc[2].random << 2 | m_adc[1].random << 1 | m_adc[0].random) & 0x7;

    // From Thetis network.c:1097-1103 — Enable bitmask
    buf[7] = (m_rx[6].enable << 6 | m_rx[5].enable << 5 |
              m_rx[4].enable << 4 | m_rx[3].enable << 3 |
              m_rx[2].enable << 2 | m_rx[1].enable << 1 |
              m_rx[0].enable) & 0xff;

    // From Thetis network.c:1106-1169 — Per-RX config
    // Layout: each RX is 6 bytes apart, starting at byte 17
    // byte+0: ADC, byte+1-2: sampling rate, byte+5: bit depth
    for (int i = 0; i < 7; ++i) {
        int base = 17 + (i * 6);
        buf[base] = m_rx[i].rxAdc;
        buf[base + 1] = (m_rx[i].samplingRate >> 8) & 0xff;
        buf[base + 2] = m_rx[i].samplingRate & 0xff;
        buf[base + 5] = m_rx[i].bitDepth;
    }

    // From Thetis network.c:1172
    buf[1363] = m_rx[0].sync;

    // From Thetis network.c:1178
    QByteArray pkt(buf, sizeof(buf));
    m_socket->writeDatagram(pkt, m_radioInfo.address, m_baseOutboundPort + 1);
}

// Porting from Thetis CmdTx() network.c:1181-1248
void P2RadioConnection::sendCmdTx()
{
    char buf[60];
    memset(buf, 0, sizeof(buf));

    writeBE32(buf, 0, m_seqTx++);

    // From Thetis network.c:1188
    buf[4] = m_numDac;

    // From Thetis network.c:1199 — CW mode control
    buf[5] = m_cw.modeControl;

    // From Thetis network.c:1202-1216
    buf[6] = m_cw.sidetoneLevel;
    buf[7] = (m_cw.sidetoneFreq >> 8) & 0xff;
    buf[8] = m_cw.sidetoneFreq & 0xff;
    buf[9] = m_cw.keyerSpeed;
    buf[10] = m_cw.keyerWeight;
    buf[11] = (m_cw.hangDelay >> 8) & 0xff;
    buf[12] = m_cw.hangDelay & 0xff;
    buf[13] = m_cw.rfDelay;

    // From Thetis network.c:1218-1220 — TX0 sampling rate
    buf[14] = (m_tx[0].samplingRate >> 8) & 0xff;
    buf[15] = m_tx[0].samplingRate & 0xff;

    // From Thetis network.c:1222
    buf[17] = m_cw.edgeLength & 0xff;

    // From Thetis network.c:1224-1226 — TX0 phase shift
    buf[26] = (m_tx[0].phaseShift >> 8) & 0xff;
    buf[27] = m_tx[0].phaseShift & 0xff;

    // From Thetis network.c:1234 — Mic control
    buf[50] = m_mic.micControl;

    // From Thetis network.c:1236
    buf[51] = m_mic.lineInGain;

    // From Thetis network.c:1238-1242 — Step attenuators on TX
    buf[57] = m_adc[2].txStepAttn;
    buf[58] = m_adc[1].txStepAttn;
    buf[59] = m_adc[0].txStepAttn;

    // From Thetis network.c:1247
    QByteArray pkt(buf, sizeof(buf));
    m_socket->writeDatagram(pkt, m_radioInfo.address, m_baseOutboundPort + 2);
}

// --- Data Parsing ---
// Porting from Thetis ReadUDPFrame:605-631 and ReadThreadMainLoop:790-808

void P2RadioConnection::processIqPacket(const QByteArray& data, int ddcIndex)
{
    if (ddcIndex < 0 || ddcIndex >= kMaxDdc) {
        return;
    }

    const auto* raw = reinterpret_cast<const unsigned char*>(data.constData());

    // From Thetis ReadUDPFrame:509-512 — sequence number extraction
    quint32 seq = (static_cast<quint32>(raw[0]) << 24)
               | (static_cast<quint32>(raw[1]) << 16)
               | (static_cast<quint32>(raw[2]) << 8)
               | (static_cast<quint32>(raw[3]));

    // From Thetis ReadUDPFrame:619-626 — sequence error detection
    if (seq != (1 + m_rx[ddcIndex].rxInSeqNo) && seq != 0
        && m_rx[ddcIndex].rxInSeqNo != 0) {
        m_rx[ddcIndex].rxInSeqErr += 1;
        qCDebug(lcProtocol) << "P2: DDC" << ddcIndex
                            << "seq error this:" << seq
                            << "last:" << m_rx[ddcIndex].rxInSeqNo;
    }
    m_rx[ddcIndex].rxInSeqNo = seq;

    // From Thetis ReadUDPFrame:629 — copy I/Q data (skip 16-byte header)
    // memcpy(bufp, readbuf + 16, 1428);
    // Then ReadThreadMainLoop:790-806 — convert 24-bit to float
    int spp = m_rx[ddcIndex].spp;  // 238 samples per packet
    QVector<float>& buf = m_iqBuffers[ddcIndex];
    if (buf.size() != spp * 2) {
        buf.resize(spp * 2);
    }

    // From Thetis ReadThreadMainLoop:790-806
    // for (i = 0, k = 0; i < prn->rx[0].spp; i++, k += 6)
    //   prn->RxReadBufp[2*i+0] = const_1_div_2147483648_ *
    //     (double)(prn->ReadBufp[k+0]<<24 | prn->ReadBufp[k+1]<<16 | prn->ReadBufp[k+2]<<8);
    const unsigned char* iqData = raw + 16;
    for (int i = 0, k = 0; i < spp; ++i, k += 6) {
        // I sample
        qint32 iVal = (static_cast<qint32>(iqData[k + 0]) << 24)
                    | (static_cast<qint32>(iqData[k + 1]) << 16)
                    | (static_cast<qint32>(iqData[k + 2]) << 8);
        buf[2 * i + 0] = static_cast<float>(iVal) / 2147483648.0f;

        // Q sample
        qint32 qVal = (static_cast<qint32>(iqData[k + 3]) << 24)
                    | (static_cast<qint32>(iqData[k + 4]) << 16)
                    | (static_cast<qint32>(iqData[k + 5]) << 8);
        buf[2 * i + 1] = static_cast<float>(qVal) / 2147483648.0f;
    }

    ++m_totalIqPackets;

    if (m_totalIqPackets == 1) {
        qCDebug(lcConnection) << "P2: First I/Q packet! DDC" << ddcIndex
                              << "seq:" << seq << "spp:" << spp;
    } else if (m_totalIqPackets % 10000 == 0) {
        qCDebug(lcProtocol) << "P2: I/Q packets:" << m_totalIqPackets;
    }

    emit iqDataReceived(ddcIndex, buf);
}

// Porting from Thetis ReadUDPFrame:519-532 — High Priority C&C status
void P2RadioConnection::processHighPriorityStatus(const QByteArray& data)
{
    const auto* raw = reinterpret_cast<const unsigned char*>(data.constData());

    // From Thetis ReadUDPFrame:522-530
    quint32 seq = (static_cast<quint32>(raw[0]) << 24)
               | (static_cast<quint32>(raw[1]) << 16)
               | (static_cast<quint32>(raw[2]) << 8)
               | (static_cast<quint32>(raw[3]));

    if (seq != (1 + m_ccSeqNo) && seq != 0 && m_ccSeqNo != 0) {
        qCDebug(lcProtocol) << "P2: CC seq error this:" << seq << "last:" << m_ccSeqNo;
    }
    m_ccSeqNo = seq;

    // Status data starts at byte 4 (Thetis copies readbuf+4, 56 bytes)
    // Extract key fields for meter data
    // These offsets are from the Thetis high-priority status parsing
    // (varies by firmware; basic fields for now)

    // From Thetis network.c:getAndResetADC_Overload — bit 0=ADC0, bit 1=ADC1, bit 2=ADC2.
    const quint8 adcOverloadBits = raw[4];
    for (int i = 0; i < 3; ++i) {
        if (adcOverloadBits & (1 << i)) {
            emit adcOverflow(i);
        }
    }

    emit meterDataReceived(0.0f, 0.0f, 0.0f, 0.0f);
}

// ---------------------------------------------------------------------------
// getAdcForDdc
// ---------------------------------------------------------------------------
int P2RadioConnection::getAdcForDdc(int ddc) const
{
    if (ddc < 0 || ddc > 6) { return 0; }
    return static_cast<int>((m_rxAdcCtrl1 >> (ddc * 2)) & 0x3);
}

// --- Utility ---

void P2RadioConnection::writeBE32(char* buf, int offset, quint32 value)
{
    buf[offset]     = static_cast<char>((value >> 24) & 0xFF);
    buf[offset + 1] = static_cast<char>((value >> 16) & 0xFF);
    buf[offset + 2] = static_cast<char>((value >> 8)  & 0xFF);
    buf[offset + 3] = static_cast<char>( value        & 0xFF);
}

// From pcap analysis: phase_word = freq_hz * 2^32 / 122880000
// The ANAN-G2 clock is 122.88 MHz. Phase word mode (General byte 37 bit 3)
// tells the radio to interpret frequency fields as NCO phase increments.
quint32 P2RadioConnection::hzToPhaseWord(quint64 freqHz)
{
    // Use 64-bit math to avoid overflow: freq * 2^32 / 122880000
    return static_cast<quint32>((freqHz * 4294967296ULL) / 122880000ULL);
}

// ============================================================================
// Alex Filter/Antenna Register Computation
// ============================================================================

// Porting from Thetis console.cs:6830-6942 — setAlexHPF
// Selects the appropriate high-pass filter based on RX frequency.
// Returns HPF bits for the Alex bpfilter register.
int P2RadioConnection::computeAlexHpf(double freqMhz)
{
    // From Thetis console.cs:6836-6939 — frequency breakpoints with default ranges
    // Each HPF has a start/end range; these are the Thetis defaults.
    if (freqMhz < 1.5) {
        return 0x20;   // Bypass HPF (below 1.5 MHz)
    }
    if (freqMhz < 6.5) {
        return 0x10;   // 1.5 MHz HPF
    }
    if (freqMhz < 9.5) {
        return 0x08;   // 6.5 MHz HPF
    }
    if (freqMhz < 13.0) {
        return 0x04;   // 9.5 MHz HPF
    }
    if (freqMhz < 20.0) {
        return 0x01;   // 13 MHz HPF
    }
    if (freqMhz < 50.0) {
        return 0x02;   // 20 MHz HPF
    }
    // 6m band: use 6M BPF/preamp
    return 0x40;       // 6M preamp/BPF
}

// Porting from Thetis console.cs:7168-7234 — setAlexLPF
// Selects the appropriate low-pass filter based on TX frequency.
// Returns LPF bits for the Alex bpfilter register.
int P2RadioConnection::computeAlexLpf(double freqMhz)
{
    // From Thetis console.cs:7172-7230 — frequency breakpoints with default ranges
    if (freqMhz < 2.0) {
        return 0x08;   // 160m LPF
    }
    if (freqMhz < 4.0) {
        return 0x04;   // 80m LPF
    }
    if (freqMhz < 7.3) {
        return 0x02;   // 60/40m LPF
    }
    if (freqMhz < 14.35) {
        return 0x01;   // 30/20m LPF
    }
    if (freqMhz < 21.45) {
        return 0x40;   // 17/15m LPF
    }
    if (freqMhz < 29.7) {
        return 0x20;   // 12/10m LPF
    }
    // 6m and above
    return 0x10;       // 6m LPF
}

// Build Alex0 32-bit register (bytes 1432-1435 in CmdHighPriority).
// Contains: RX antenna (bits 24-26), LPF (bits 20-31), HPF (bits 0-6),
//           RX relay bits (bits 8-15).
// From Thetis ChannelMaster/network.h:263-358 bpfilter struct.
quint32 P2RadioConnection::buildAlex0() const
{
    quint32 reg = 0;

    // RX antenna selection — from Thetis netInterface.c:479-485
    // ANT1=0x01, ANT2=0x02, ANT3=0x03 → bits 24-26
    int antBits = m_alex.rxAnt & 0x03;
    if (antBits == 0x01) {
        reg |= (1 << 24);  // _ANT_1
    } else if (antBits == 0x02) {
        reg |= (1 << 25);  // _ANT_2
    } else if (antBits == 0x03) {
        reg |= (1 << 26);  // _ANT_3
    }

    // LPF bits — from Thetis netInterface.c:682-726
    // Bits map: 30_20[20], 60_40[21], 80[22], 160[23], 6[29], 12_10[30], 17_15[31]
    if (m_alex.lpfBits & 0x01) { reg |= (1 << 20); }  // 30/20m
    if (m_alex.lpfBits & 0x02) { reg |= (1 << 21); }  // 60/40m
    if (m_alex.lpfBits & 0x04) { reg |= (1 << 22); }  // 80m
    if (m_alex.lpfBits & 0x08) { reg |= (1 << 23); }  // 160m
    if (m_alex.lpfBits & 0x10) { reg |= (1 << 29); }  // 6m
    if (m_alex.lpfBits & 0x20) { reg |= (1 << 30); }  // 12/10m
    if (m_alex.lpfBits & 0x40) { reg |= (1 << 31); }  // 17/15m

    // HPF bits — from Thetis netInterface.c:605-621
    // Bits map: 13MHz[1], 20MHz[2], 6M_preamp[3], 9.5MHz[4], 6.5MHz[5], 1.5MHz[6]
    if (m_alex.hpfBits & 0x01) { reg |= (1 << 1); }   // 13 MHz
    if (m_alex.hpfBits & 0x02) { reg |= (1 << 2); }   // 20 MHz
    if (m_alex.hpfBits & 0x04) { reg |= (1 << 4); }   // 9.5 MHz
    if (m_alex.hpfBits & 0x08) { reg |= (1 << 5); }   // 6.5 MHz
    if (m_alex.hpfBits & 0x10) { reg |= (1 << 6); }   // 1.5 MHz
    if (m_alex.hpfBits & 0x20) { reg |= (1 << 12); }  // Bypass
    if (m_alex.hpfBits & 0x40) { reg |= (1 << 3); }   // 6M preamp

    return reg;
}

// Build Alex1 32-bit register (bytes 1428-1431 in CmdHighPriority).
// Contains: TX antenna (bits 24-26), same LPF/HPF layout as Alex0.
// From Thetis ChannelMaster/network.h bpfilter2 struct.
quint32 P2RadioConnection::buildAlex1() const
{
    quint32 reg = 0;

    // TX antenna selection — same encoding as RX but in Alex1
    int antBits = m_alex.txAnt & 0x03;
    if (antBits == 0x01) {
        reg |= (1 << 24);  // _TXANT_1
    } else if (antBits == 0x02) {
        reg |= (1 << 25);  // _TXANT_2
    } else if (antBits == 0x03) {
        reg |= (1 << 26);  // _TXANT_3
    }

    // Same LPF bits as Alex0 (TX uses same LPF selection)
    if (m_alex.lpfBits & 0x01) { reg |= (1 << 20); }
    if (m_alex.lpfBits & 0x02) { reg |= (1 << 21); }
    if (m_alex.lpfBits & 0x04) { reg |= (1 << 22); }
    if (m_alex.lpfBits & 0x08) { reg |= (1 << 23); }
    if (m_alex.lpfBits & 0x10) { reg |= (1 << 29); }
    if (m_alex.lpfBits & 0x20) { reg |= (1 << 30); }
    if (m_alex.lpfBits & 0x40) { reg |= (1 << 31); }

    // Same HPF bits
    if (m_alex.hpfBits & 0x01) { reg |= (1 << 1); }
    if (m_alex.hpfBits & 0x02) { reg |= (1 << 2); }
    if (m_alex.hpfBits & 0x04) { reg |= (1 << 4); }
    if (m_alex.hpfBits & 0x08) { reg |= (1 << 5); }
    if (m_alex.hpfBits & 0x10) { reg |= (1 << 6); }
    if (m_alex.hpfBits & 0x20) { reg |= (1 << 12); }
    if (m_alex.hpfBits & 0x40) { reg |= (1 << 3); }

    return reg;
}

} // namespace NereusSDR
