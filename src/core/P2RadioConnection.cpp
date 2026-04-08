#include "P2RadioConnection.h"
#include "LogCategories.h"

#include <QNetworkDatagram>
#include <QtEndian>

namespace NereusSDR {

P2RadioConnection::P2RadioConnection(QObject* parent)
    : RadioConnection(parent)
{
    m_rxFrequencies.fill(14225000);
    m_firstIqPacket.fill(true);
    m_lastIqSeq.fill(0);

    for (auto& buf : m_iqBuffers) {
        buf.resize(kSamplesPerPacket * 2);  // I + Q per sample
    }
}

P2RadioConnection::~P2RadioConnection()
{
    if (m_running) {
        disconnect();
    }
}

// --- Thread Lifecycle ---

void P2RadioConnection::init()
{
    // Create sockets on the worker thread.
    // P2 protocol uses fixed port numbers — both PC and radio use the same ports.
    // The PC must bind to the standard ports so the radio's responses arrive correctly.

    // Command socket — ephemeral port, sends to radio on ports 1024-1027
    m_cmdSocket = new QUdpSocket(this);
    if (!m_cmdSocket->bind(QHostAddress::Any, 0)) {
        qCWarning(lcConnection) << "P2: Failed to bind command socket";
    }

    // High-priority status socket — bind to port 1025 (radio sends status here)
    m_highPriStatusSocket = new QUdpSocket(this);
    if (!m_highPriStatusSocket->bind(QHostAddress::Any, kPortHighPriFrom)) {
        qCWarning(lcConnection) << "P2: Failed to bind status socket on port"
                                << kPortHighPriFrom << "- trying ephemeral";
        if (!m_highPriStatusSocket->bind(QHostAddress::Any, 0)) {
            qCWarning(lcConnection) << "P2: Failed to bind status socket at all";
        }
    }
    m_localHighPriStatusPort = m_highPriStatusSocket->localPort();
    connect(m_highPriStatusSocket, &QUdpSocket::readyRead,
            this, &P2RadioConnection::onHighPriorityStatusReady);

    // Data socket — bind to port 1035 (radio sends DDC0 I/Q data here)
    m_dataSocket = new QUdpSocket(this);
    if (!m_dataSocket->bind(QHostAddress::Any, kPortDdcBase)) {
        qCWarning(lcConnection) << "P2: Failed to bind data socket on port"
                                << kPortDdcBase << "- trying ephemeral";
        if (!m_dataSocket->bind(QHostAddress::Any, 0)) {
            qCWarning(lcConnection) << "P2: Failed to bind data socket at all";
        }
    }
    m_localDataPort = m_dataSocket->localPort();
    connect(m_dataSocket, &QUdpSocket::readyRead, this, &P2RadioConnection::onDataReady);

    // High-priority timer — periodic resend of frequencies/run state
    m_highPriorityTimer = new QTimer(this);
    m_highPriorityTimer->setInterval(kHighPriIntervalMs);
    connect(m_highPriorityTimer, &QTimer::timeout, this, &P2RadioConnection::onHighPriorityTick);

    // Reconnect timer
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setInterval(3000);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &P2RadioConnection::onReconnectTimeout);

    qCDebug(lcConnection) << "P2: init() on worker thread, data port:" << m_localDataPort
                          << "status port:" << m_localHighPriStatusPort;
}

// --- Connection Lifecycle ---

void P2RadioConnection::connectToRadio(const RadioInfo& info)
{
    if (m_running) {
        disconnect();
    }

    m_radioInfo = info;
    m_intentionalDisconnect = false;
    m_totalIqPackets = 0;

    // Reset sequence counters
    m_seqGeneral = 0;
    m_seqRx = 0;
    m_seqTx = 0;
    m_seqHighPri = 0;
    m_firstIqPacket.fill(true);

    setState(ConnectionState::Connecting);

    qCDebug(lcConnection) << "P2: Connecting to" << info.displayName()
                          << "at" << info.address.toString();

    // Send configuration commands then start streaming
    sendAllCommands();

    // Set running flag and send the start command
    m_running = true;
    QByteArray highPri = buildCmdHighPriority();
    m_cmdSocket->writeDatagram(highPri, m_radioInfo.address, kPortHighPri);

    // Start periodic high-priority resend
    m_highPriorityTimer->start();

    setState(ConnectionState::Connected);

    qCDebug(lcConnection) << "P2: Connected, streaming started";
}

void P2RadioConnection::disconnect()
{
    m_intentionalDisconnect = true;

    if (m_highPriorityTimer) {
        m_highPriorityTimer->stop();
    }
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
    }

    if (m_running && m_cmdSocket && !m_radioInfo.address.isNull()) {
        // Send stop command
        m_running = false;
        QByteArray highPri = buildCmdHighPriority();
        m_cmdSocket->writeDatagram(highPri, m_radioInfo.address, kPortHighPri);
        qCDebug(lcConnection) << "P2: Stop command sent";
    }

    m_running = false;

    // Close sockets so the event loop has nothing left to process
    if (m_dataSocket) {
        m_dataSocket->close();
    }
    if (m_highPriStatusSocket) {
        m_highPriStatusSocket->close();
    }
    if (m_cmdSocket) {
        m_cmdSocket->close();
    }

    setState(ConnectionState::Disconnected);

    qCDebug(lcConnection) << "P2: Disconnected. Total I/Q packets received:" << m_totalIqPackets;
}

// --- Hardware Control Slots ---

void P2RadioConnection::setReceiverFrequency(int receiverIndex, quint64 frequencyHz)
{
    if (receiverIndex < 0 || receiverIndex >= kMaxReceivers) {
        return;
    }
    m_rxFrequencies[receiverIndex] = frequencyHz;

    // Immediately resend high-priority command with new frequency
    if (m_running) {
        QByteArray pkt = buildCmdHighPriority();
        m_cmdSocket->writeDatagram(pkt, m_radioInfo.address, kPortHighPri);
    }
}

void P2RadioConnection::setTxFrequency(quint64 frequencyHz)
{
    m_txFrequency = frequencyHz;

    if (m_running) {
        QByteArray pkt = buildCmdHighPriority();
        m_cmdSocket->writeDatagram(pkt, m_radioInfo.address, kPortHighPri);
    }
}

void P2RadioConnection::setActiveReceiverCount(int count)
{
    m_activeReceiverCount = qBound(1, count, kMaxReceivers);

    if (m_running) {
        QByteArray pkt = buildCmdRx();
        m_cmdSocket->writeDatagram(pkt, m_radioInfo.address, kPortRxConfig);
    }
}

void P2RadioConnection::setSampleRate(int sampleRate)
{
    m_sampleRate = sampleRate;

    if (m_running) {
        QByteArray pkt = buildCmdRx();
        m_cmdSocket->writeDatagram(pkt, m_radioInfo.address, kPortRxConfig);
    }
}

void P2RadioConnection::setAttenuator(int dB)
{
    m_attenuatorDb = qBound(0, dB, 31);

    if (m_running) {
        QByteArray pkt = buildCmdHighPriority();
        m_cmdSocket->writeDatagram(pkt, m_radioInfo.address, kPortHighPri);
    }
}

void P2RadioConnection::setPreamp(bool enabled)
{
    m_preampOn = enabled;

    if (m_running) {
        QByteArray pkt = buildCmdHighPriority();
        m_cmdSocket->writeDatagram(pkt, m_radioInfo.address, kPortHighPri);
    }
}

void P2RadioConnection::setTxDrive(int level)
{
    m_txDriveLevel = qBound(0, level, 255);

    if (m_running) {
        QByteArray pkt = buildCmdHighPriority();
        m_cmdSocket->writeDatagram(pkt, m_radioInfo.address, kPortHighPri);
    }
}

void P2RadioConnection::setMox(bool enabled)
{
    m_mox = enabled;

    if (m_running) {
        QByteArray pkt = buildCmdHighPriority();
        m_cmdSocket->writeDatagram(pkt, m_radioInfo.address, kPortHighPri);
    }
}

void P2RadioConnection::setAntenna(int antennaIndex)
{
    m_antennaIndex = antennaIndex;

    if (m_running) {
        QByteArray pkt = buildCmdHighPriority();
        m_cmdSocket->writeDatagram(pkt, m_radioInfo.address, kPortHighPri);
    }
}

// --- Private Slots ---

void P2RadioConnection::onDataReady()
{
    while (m_dataSocket && m_dataSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_dataSocket->receiveDatagram();
        QByteArray data = datagram.data();

        // On the DDC data socket (bound to port 1035), all packets are DDC0 I/Q.
        // For multiple DDCs, we'd bind additional sockets on ports 1036-1041.
        // For now, treat everything on this socket as DDC0.
        if (data.size() >= kIqDataOffset + (kSamplesPerPacket * kIqBytesPerSample)) {
            processIqPacket(data, 0);
        } else if (data.size() > 4) {
            qCDebug(lcProtocol) << "P2: Data packet, size:" << data.size()
                                << "from port" << datagram.senderPort();
        }
    }
}

void P2RadioConnection::onHighPriorityStatusReady()
{
    while (m_highPriStatusSocket && m_highPriStatusSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_highPriStatusSocket->receiveDatagram();
        processHighPriorityStatus(datagram.data());
    }
}

void P2RadioConnection::onHighPriorityTick()
{
    if (m_running && !m_radioInfo.address.isNull()) {
        QByteArray pkt = buildCmdHighPriority();
        m_cmdSocket->writeDatagram(pkt, m_radioInfo.address, kPortHighPri);
    }
}

void P2RadioConnection::onReconnectTimeout()
{
    if (!m_intentionalDisconnect && !m_radioInfo.address.isNull()) {
        qCDebug(lcConnection) << "P2: Attempting reconnect to" << m_radioInfo.displayName();
        connectToRadio(m_radioInfo);
    }
}

// --- Command Builders ---

void P2RadioConnection::sendAllCommands()
{
    QByteArray general = buildCmdGeneral();
    m_cmdSocket->writeDatagram(general, m_radioInfo.address, kPortGeneral);

    QByteArray rx = buildCmdRx();
    m_cmdSocket->writeDatagram(rx, m_radioInfo.address, kPortRxConfig);

    QByteArray tx = buildCmdTx();
    m_cmdSocket->writeDatagram(tx, m_radioInfo.address, kPortTxConfig);

    qCDebug(lcProtocol) << "P2: Sent General+Rx+Tx config commands";
}

QByteArray P2RadioConnection::buildCmdGeneral()
{
    // 60-byte General command → port 1024
    // Port assignments follow Thetis ChannelMaster convention:
    //   Bytes 5-10: ports the radio receives data FROM the PC (PC source ports)
    //   Bytes 11-22: ports the radio sends data OUT to the PC (radio source ports)
    // Both sides use the same standard port numbers (base 1025).
    QByteArray pkt(60, 0);
    writeBE32(pkt, 0, m_seqGeneral++);

    // Byte 4: Command type = 0x00 (general)
    pkt[4] = 0x00;

    // Ports the radio RECEIVES from PC (PC outbound source ports)
    quint16 base = kBasePort + 1;  // 1025
    writeBE16(pkt, 5, base);          // Bytes 5-6: RX Specific #1025
    writeBE16(pkt, 7, base + 1);      // Bytes 7-8: TX Specific #1026
    writeBE16(pkt, 9, base + 2);      // Bytes 9-10: High Priority from PC #1027
    writeBE16(pkt, 13, base + 3);     // Bytes 13-14: RX Audio #1028
    writeBE16(pkt, 15, base + 4);     // Bytes 15-16: TX0 IQ #1029

    // Ports the radio SENDS to PC (radio outbound source ports)
    writeBE16(pkt, 11, base);         // Bytes 11-12: High Priority to PC #1025
    writeBE16(pkt, 17, base + 10);    // Bytes 17-18: RX0 DDC IQ #1035
    writeBE16(pkt, 19, base + 1);     // Bytes 19-20: Mic Samples #1026
    writeBE16(pkt, 21, base + 2);     // Bytes 21-22: Wideband ADC0 #1027

    return pkt;
}

QByteArray P2RadioConnection::buildCmdRx()
{
    // 1444-byte RX configuration → port 1025
    QByteArray pkt(1444, 0);
    writeBE32(pkt, 0, m_seqRx++);

    // Byte 4: Number of ADCs
    pkt[4] = static_cast<char>(m_radioInfo.adcCount);

    // Byte 7: RX enable bitmask (bits 0-6 for DDC0-DDC6)
    quint8 rxEnable = 0;
    for (int i = 0; i < m_activeReceiverCount && i < kMaxReceivers; ++i) {
        rxEnable |= (1 << i);
    }
    pkt[7] = static_cast<char>(rxEnable);

    // Sample rate encoding: 48=48kHz, 96=96kHz, 192=192kHz, 384=384kHz
    // Stored as the rate / 1000
    quint16 rateCode = static_cast<quint16>(m_sampleRate / 1000);

    // Per-RX configuration (6 bytes each, starting at byte 17)
    for (int i = 0; i < m_activeReceiverCount && i < kMaxReceivers; ++i) {
        int offset = 17 + (i * 6);
        pkt[offset] = 0;                          // ADC selection (0 = ADC0)
        writeBE16(pkt, offset + 1, rateCode);     // Sampling rate
        pkt[offset + 3] = 0;                      // Reserved
        pkt[offset + 4] = 24;                     // Bit depth (24-bit)
        pkt[offset + 5] = 0;                      // Reserved
    }

    return pkt;
}

QByteArray P2RadioConnection::buildCmdTx()
{
    // 60-byte TX configuration → port 1026
    QByteArray pkt(60, 0);
    writeBE32(pkt, 0, m_seqTx++);

    // Byte 4: Number of DACs
    pkt[4] = 1;

    // Byte 5: CW mode control (all zeros for now — CW support in later phase)
    // Byte 6: Sidetone level
    // Bytes 7-8: Sidetone frequency
    // Byte 9: Keyer speed
    // Bytes 14-15: TX0 sampling rate
    quint16 txRate = static_cast<quint16>(m_sampleRate / 1000);
    writeBE16(pkt, 14, txRate);

    return pkt;
}

QByteArray P2RadioConnection::buildCmdHighPriority()
{
    // 1444-byte High Priority command → port 1027
    // Contains: run/stop flag, PTT, frequencies, drive level, filters, attenuators
    QByteArray pkt(1444, 0);
    writeBE32(pkt, 0, m_seqHighPri++);

    // Byte 4: Run/PTT control
    quint8 runPtt = 0;
    if (m_running) {
        runPtt |= 0x01;  // bit 0: run
    }
    if (m_mox) {
        runPtt |= 0x02;  // bit 1: PTT
    }
    pkt[4] = static_cast<char>(runPtt);

    // RX Frequencies: 32-bit big-endian Hz values
    // Bytes 9-12: RX0, 13-16: RX1, 17-20: RX2, etc. (4 bytes each, up to 12 receivers)
    for (int i = 0; i < kMaxReceivers; ++i) {
        int offset = 9 + (i * 4);
        writeBE32(pkt, offset, static_cast<quint32>(m_rxFrequencies[i]));
    }

    // TX0 Frequency: bytes 329-332
    writeBE32(pkt, 329, static_cast<quint32>(m_txFrequency));

    // TX0 Drive Level: byte 345
    pkt[345] = static_cast<char>(m_txDriveLevel);

    // Step attenuators: bytes 1441-1443 (ADC2, ADC1, ADC0)
    pkt[1443] = static_cast<char>(m_attenuatorDb);  // ADC0

    return pkt;
}

// --- Data Parsing ---

void P2RadioConnection::processIqPacket(const QByteArray& data, int ddcIndex)
{
    if (ddcIndex < 0 || ddcIndex >= kMaxDdc) {
        return;
    }

    // P2 I/Q packet: 1444 bytes
    // Bytes 0-3: sequence number
    // Bytes 4-15: reserved/metadata
    // Bytes 16-1443: I/Q data (238 samples × 6 bytes = 1428 bytes)
    if (data.size() < kIqDataOffset + (kSamplesPerPacket * kIqBytesPerSample)) {
        qCDebug(lcProtocol) << "P2: Short I/Q packet for DDC" << ddcIndex
                            << "size:" << data.size();
        return;
    }

    const auto* raw = reinterpret_cast<const unsigned char*>(data.constData());

    // Sequence tracking
    quint32 seq = qFromBigEndian<quint32>(raw);
    if (m_firstIqPacket[ddcIndex]) {
        m_firstIqPacket[ddcIndex] = false;
    } else if (seq != m_lastIqSeq[ddcIndex] + 1) {
        int dropped = static_cast<int>(seq - m_lastIqSeq[ddcIndex] - 1);
        if (dropped > 0 && dropped < 1000) {
            qCDebug(lcProtocol) << "P2: DDC" << ddcIndex
                                << "dropped" << dropped << "packets"
                                << "(expected" << m_lastIqSeq[ddcIndex] + 1
                                << "got" << seq << ")";
        }
    }
    m_lastIqSeq[ddcIndex] = seq;

    // Parse I/Q samples
    QVector<float>& buf = m_iqBuffers[ddcIndex];
    if (buf.size() != kSamplesPerPacket * 2) {
        buf.resize(kSamplesPerPacket * 2);
    }

    const unsigned char* iqStart = raw + kIqDataOffset;
    for (int i = 0; i < kSamplesPerPacket; ++i) {
        const unsigned char* samp = iqStart + (i * kIqBytesPerSample);
        buf[i * 2]     = decodeP2Sample(samp);       // I
        buf[i * 2 + 1] = decodeP2Sample(samp + 3);   // Q
    }

    ++m_totalIqPackets;

    // Log periodically for diagnostics
    if (m_totalIqPackets == 1) {
        qCDebug(lcConnection) << "P2: First I/Q packet received, DDC" << ddcIndex
                              << "seq:" << seq;
    } else if (m_totalIqPackets % 10000 == 0) {
        qCDebug(lcProtocol) << "P2: I/Q packets received:" << m_totalIqPackets;
    }

    emit iqDataReceived(ddcIndex, buf);
}

void P2RadioConnection::processHighPriorityStatus(const QByteArray& data)
{
    if (data.size() < 60) {
        return;
    }

    const auto* raw = reinterpret_cast<const unsigned char*>(data.constData());

    // Bytes 0-3: sequence number (skip)

    // Bytes 49-50: forward power (big-endian 16-bit ADC value)
    quint16 fwdRaw = qFromBigEndian<quint16>(raw + 49);
    // Bytes 51-52: reverse power
    quint16 revRaw = qFromBigEndian<quint16>(raw + 51);
    // Bytes 57-58: supply voltage
    quint16 voltRaw = qFromBigEndian<quint16>(raw + 57);
    // Bytes 59-60: PA current
    quint16 paRaw = (data.size() > 60) ? qFromBigEndian<quint16>(raw + 59) : 0;

    // Convert to approximate real values
    // These scaling factors are board-dependent; use approximate values for now
    float fwdPower = static_cast<float>(fwdRaw) / 4095.0f;
    float revPower = static_cast<float>(revRaw) / 4095.0f;
    float supplyVoltage = static_cast<float>(voltRaw) * 3.3f / 4095.0f * 11.0f;  // Voltage divider
    float paCurrent = static_cast<float>(paRaw) * 3.3f / 4095.0f / 0.04f;         // Current sense

    emit meterDataReceived(fwdPower, revPower, supplyVoltage, paCurrent);

    // ADC overflow: byte 0 of status area
    if (data.size() > 5 && (raw[5] & 0x01)) {
        emit adcOverflow(0);
    }
    if (data.size() > 5 && (raw[5] & 0x02)) {
        emit adcOverflow(1);
    }
}

// --- Utility ---

float P2RadioConnection::decodeP2Sample(const unsigned char* p)
{
    // 24-bit big-endian signed → float
    // Shift into top 24 bits of 32-bit int for automatic sign extension
    qint32 val = (static_cast<qint32>(p[0]) << 24)
               | (static_cast<qint32>(p[1]) << 16)
               | (static_cast<qint32>(p[2]) << 8);
    return static_cast<float>(val) / 2147483648.0f;
}

void P2RadioConnection::writeBE32(QByteArray& buf, int offset, quint32 value)
{
    buf[offset]     = static_cast<char>((value >> 24) & 0xFF);
    buf[offset + 1] = static_cast<char>((value >> 16) & 0xFF);
    buf[offset + 2] = static_cast<char>((value >> 8)  & 0xFF);
    buf[offset + 3] = static_cast<char>( value        & 0xFF);
}

void P2RadioConnection::writeBE16(QByteArray& buf, int offset, quint16 value)
{
    buf[offset]     = static_cast<char>((value >> 8) & 0xFF);
    buf[offset + 1] = static_cast<char>( value       & 0xFF);
}

} // namespace NereusSDR
