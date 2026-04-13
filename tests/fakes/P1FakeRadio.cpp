// tests/fakes/P1FakeRadio.cpp
//
// Protocol 1 fake radio for loopback integration tests.
// Implements just enough of the P1 wire protocol to exercise
// P1RadioConnection's socket path.
//
// Wire-format references:
//   Discovery reply:    p1_hermeslite_reply.hex (60 bytes, EF FE 02 ...)
//   Metis start:        networkproto1.c:49  (EF FE 04 01)
//   Metis stop:         networkproto1.c:84  (EF FE 04 00)
//   ep2 command:        networkproto1.c:223 (EF FE 01 02)
//   ep6 IQ reply:       networkproto1.c:319 (EF FE 01 06)

#include "P1FakeRadio.h"

#include <QNetworkDatagram>
#include <cstring>

namespace NereusSDR::Test {

P1FakeRadio::P1FakeRadio(QObject* parent)
    : QObject(parent)
{
}

P1FakeRadio::~P1FakeRadio()
{
    stop();
}

void P1FakeRadio::start()
{
    if (m_socket) {
        return;  // already started
    }
    m_socket = new QUdpSocket(this);
    bool ok = m_socket->bind(QHostAddress::LocalHost, 0);
    Q_ASSERT_X(ok, "P1FakeRadio::start", "Failed to bind loopback UDP socket");

    connect(m_socket, &QUdpSocket::readyRead, this, &P1FakeRadio::onReadyRead);

    // Auto-stream timer — fires every 10ms to push ep6 frames while running.
    // This simulates the continuous ep6 stream a real HPSDR radio sends after
    // receiving metis-start (networkproto1.c WriteMainLoop cadence).
    m_streamTimer = new QTimer(this);
    m_streamTimer->setInterval(10);
    connect(m_streamTimer, &QTimer::timeout, this, &P1FakeRadio::onAutoStreamTick);
    m_streamTimer->start();
}

void P1FakeRadio::stop()
{
    if (m_streamTimer) {
        m_streamTimer->stop();
        m_streamTimer->deleteLater();
        m_streamTimer = nullptr;
    }
    if (m_socket) {
        m_socket->close();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_running = false;
}

quint16 P1FakeRadio::localPort() const
{
    if (!m_socket) { return 0; }
    return m_socket->localPort();
}

void P1FakeRadio::goSilent()
{
    m_silent = true;
    // Also clear client tracking so auto-stream stops sending.
    // The client address is restored when the next metis-start arrives.
    m_clientPort = 0;
}

void P1FakeRadio::resume()
{
    m_silent = false;
    // m_clientAddress/m_clientPort will be repopulated when the reconnect
    // attempt sends a fresh metis-start.
}

// ---------------------------------------------------------------------------
// onReadyRead — dispatch incoming datagrams
// ---------------------------------------------------------------------------
void P1FakeRadio::onReadyRead()
{
    if (!m_socket) { return; }

    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram dg = m_socket->receiveDatagram();
        if (dg.data().isEmpty()) { continue; }

        if (m_silent) { continue; }

        const QByteArray& pkt  = dg.data();
        const QHostAddress from = dg.senderAddress();
        const quint16      port = static_cast<quint16>(dg.senderPort());

        if (pkt.size() < 4) { continue; }

        const quint8 b0 = static_cast<quint8>(pkt[0]);
        const quint8 b1 = static_cast<quint8>(pkt[1]);
        const quint8 b2 = static_cast<quint8>(pkt[2]);
        const quint8 b3 = static_cast<quint8>(pkt[3]);

        if (b0 == 0xEF && b1 == 0xFE) {
            if (b2 == 0x02) {
                // Discovery probe — EF FE 02 ...
                handleDiscoveryProbe(from, port);
            } else if (b2 == 0x04) {
                // Metis start/stop — EF FE 04 <cmd>
                handleMetisCommand(pkt, from, port);
            } else if (b2 == 0x01 && b3 == 0x02) {
                // ep2 command frame — EF FE 01 02
                handleEp2Frame(pkt);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// handleDiscoveryProbe — reply with a 60-byte P1 discovery reply
//
// Reply format mirrors p1_hermeslite_reply.hex:
//   [0-1]  EF FE
//   [2]    02 — available
//   [3-8]  AA BB CC 11 22 33 — MAC
//   [9]    48 — firmware 72
//   [10]   06 — board: HermesLite
//   [11-59] 00 — pad
// ---------------------------------------------------------------------------
void P1FakeRadio::handleDiscoveryProbe(const QHostAddress& from, quint16 port)
{
    QByteArray reply(60, '\0');
    reply[0] = static_cast<char>(0xEF);
    reply[1] = static_cast<char>(0xFE);
    reply[2] = static_cast<char>(0x02);  // available
    reply[3] = static_cast<char>(0xAA);
    reply[4] = static_cast<char>(0xBB);
    reply[5] = static_cast<char>(0xCC);
    reply[6] = static_cast<char>(0x11);
    reply[7] = static_cast<char>(0x22);
    reply[8] = static_cast<char>(0x33);
    reply[9]  = static_cast<char>(m_firmwareVersion & 0xFF);  // configurable fw version (default 72)
    reply[10] = static_cast<char>(0x06);  // HL2
    m_socket->writeDatagram(reply, from, port);
}

// ---------------------------------------------------------------------------
// handleMetisCommand — EF FE 04 <cmd>
//   cmd=0x01: start IQ stream   (networkproto1.c:49  SendStartToMetis)
//   cmd=0x00: stop IQ stream    (networkproto1.c:84  SendStopToMetis)
// ---------------------------------------------------------------------------
void P1FakeRadio::handleMetisCommand(const QByteArray& pkt,
                                      const QHostAddress& from,
                                      quint16 port)
{
    if (pkt.size() < 4) { return; }
    const quint8 cmd = static_cast<quint8>(pkt[3]);
    if (cmd == 0x01 || cmd == 0x02 || cmd == 0x03) {
        // Start streaming — remember client address
        m_running       = true;
        m_clientAddress = from;
        m_clientPort    = port;
    } else if (cmd == 0x00) {
        m_running = false;
    }
}

// ---------------------------------------------------------------------------
// handleEp2Frame — count ep2 command frames from the client
// ---------------------------------------------------------------------------
void P1FakeRadio::handleEp2Frame(const QByteArray& pkt)
{
    if (pkt.size() == 1032) {
        ++m_ep2Count;
    }
}

// ---------------------------------------------------------------------------
// buildEp6Frame
//
// Builds a 1032-byte ep6 datagram.  Layout mirrors parseEp6Frame expectations:
//   [0-3]  EF FE 01 06
//   [4-7]  sequence number big-endian
//   [8-10] 7F 7F 7F  — subframe 0 sync
//   [11-15] C0..C4 (zeros)
//   [16..]  sample slots: numRx * 6 bytes (I24+Q24) + 2 mic bytes
//   [520-522] 7F 7F 7F — subframe 1 sync
//   [523-527] C0..C4 (zeros)
//   [528..] sample slots
//
// Each I sample encodes 0.5 in 24-bit big-endian two's complement.
// 0.5 * 2^23 = 4194304 = 0x400000
// Q sample = 0.
// ---------------------------------------------------------------------------
QByteArray P1FakeRadio::buildEp6Frame(quint32 seq, int numRx)
{
    QByteArray frame(1032, '\0');
    auto* b = reinterpret_cast<quint8*>(frame.data());

    // Metis ep6 header — networkproto1.c:319
    b[0] = 0xEF;
    b[1] = 0xFE;
    b[2] = 0x01;
    b[3] = 0x06;
    b[4] = static_cast<quint8>((seq >> 24) & 0xFF);
    b[5] = static_cast<quint8>((seq >> 16) & 0xFF);
    b[6] = static_cast<quint8>((seq >>  8) & 0xFF);
    b[7] = static_cast<quint8>( seq        & 0xFF);

    // Subframe sync bytes (networkproto1.c:327)
    b[8]   = 0x7F; b[9]   = 0x7F; b[10]  = 0x7F;
    b[520] = 0x7F; b[521] = 0x7F; b[522] = 0x7F;

    // Encode samples: I=0.5, Q=0.0
    // 0.5 * 2^23 = 4194304 = 0x400000 (big-endian 24-bit)
    const quint8 iHigh = 0x40;
    const quint8 iMid  = 0x00;
    const quint8 iLow  = 0x00;

    // slotBytes = 6*numRx + 2 (networkproto1.c:361)
    const int slotBytes        = 6 * numRx + 2;
    const int samplesPerSubframe = 504 / slotBytes;

    auto fillSubframe = [&](int sampleStart) {
        for (int s = 0; s < samplesPerSubframe; ++s) {
            for (int r = 0; r < numRx; ++r) {
                int off = sampleStart + s * slotBytes + r * 6;
                // I = 0.5
                b[off + 0] = iHigh;
                b[off + 1] = iMid;
                b[off + 2] = iLow;
                // Q = 0.0
                b[off + 3] = 0;
                b[off + 4] = 0;
                b[off + 5] = 0;
            }
        }
    };

    // Subframe 0: samples start at offset 16 (8-byte header + 5-byte C&C + 3-byte sync)
    fillSubframe(16);
    // Subframe 1: samples start at offset 528
    fillSubframe(528);

    return frame;
}

// ---------------------------------------------------------------------------
// sendEp6Frames — stream `count` ep6 frames to the connected client
// ---------------------------------------------------------------------------
void P1FakeRadio::sendEp6Frames(int count)
{
    if (!m_socket || m_clientPort == 0) { return; }

    for (int i = 0; i < count; ++i) {
        QByteArray frame = buildEp6Frame(m_ep6Seq++, 1);
        m_socket->writeDatagram(frame, m_clientAddress, m_clientPort);
    }
}

// ---------------------------------------------------------------------------
// onAutoStreamTick — fires every 10ms; sends one ep6 frame if running and
// not silent. Simulates the continuous ep6 cadence a real HPSDR radio sends
// after receiving metis-start.
// ---------------------------------------------------------------------------
void P1FakeRadio::onAutoStreamTick()
{
    if (!m_socket || !m_running || m_silent || m_clientPort == 0) { return; }

    QByteArray frame = buildEp6Frame(m_ep6Seq++, 1);
    m_socket->writeDatagram(frame, m_clientAddress, m_clientPort);
}

} // namespace NereusSDR::Test
