// tests/fakes/P1FakeRadio.cpp
//
// no-port-check: NereusSDR-original test fake.  The `networkproto1.c`
// filename references below are wire-format spec citations (byte
// layouts the fake emits and consumes), not ported logic; the
// implementation here is independently written against the OpenHPSDR
// Protocol 1 wire format.  See HOW-TO-PORT.md §Inline cite versioning
// for the difference between port-attribution-bearing cites and
// documentation-only filename references in tests.
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

#include <QMutexLocker>
#include <QNetworkDatagram>
#include <QThread>
#include <QTimer>
#include <QUdpSocket>
#include <cstring>

namespace NereusSDR::Test {

P1FakeRadio::P1FakeRadio(QObject* parent)
    : QObject(parent)
{
}

P1FakeRadio::~P1FakeRadio()
{
    stop();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_radio;
    }
}

void P1FakeRadio::onRadioThread(const std::function<void()>& work)
{
    Q_ASSERT_X(QThread::currentThread() != m_thread.get(), "P1FakeRadio",
               "called on the fake's own thread");
    QMetaObject::invokeMethod(m_radio, work, Qt::BlockingQueuedConnection);
}

void P1FakeRadio::start()
{
    if (!m_thread) {
        m_thread = std::make_unique<QThread>();
        m_thread->setObjectName(QStringLiteral("P1FakeRadio"));
        m_radio = new QObject;
        m_radio->moveToThread(m_thread.get());
        m_thread->start();
    }
    onRadioThread([this] {
        if (m_socket) {
            return;  // already started
        }
        m_socket = new QUdpSocket(m_radio);
        bool ok = m_socket->bind(QHostAddress::LocalHost, 0);
        Q_ASSERT_X(ok, "P1FakeRadio::start", "Failed to bind loopback UDP socket");
        m_port.store(m_socket->localPort());

        QObject::connect(m_socket, &QUdpSocket::readyRead, m_radio, [this] { onReadyRead(); });

        // Auto-stream timer — fires every 10ms to push ep6 frames while running.
        // This simulates the continuous ep6 stream a real HPSDR radio sends after
        // receiving metis-start (networkproto1.c WriteMainLoop cadence).
        m_streamTimer = new QTimer(m_radio);
        m_streamTimer->setInterval(10);
        QObject::connect(m_streamTimer, &QTimer::timeout, m_radio, [this] { onAutoStreamTick(); });
        bool autoStream = false;
        {
            QMutexLocker lock(&m_mutex);
            autoStream = m_autoStreamEnabled;
        }
        if (autoStream) {
            m_streamTimer->start();
        }
    });
}

void P1FakeRadio::stop()
{
    if (m_thread) {
        onRadioThread([this] {
            if (m_streamTimer) {
                m_streamTimer->stop();
                delete m_streamTimer;
                m_streamTimer = nullptr;
            }
            if (m_socket) {
                m_socket->close();
                delete m_socket;
                m_socket = nullptr;
            }
            m_port.store(0);
        });
    }
    QMutexLocker lock(&m_mutex);
    m_running = false;
}

void P1FakeRadio::goSilent()
{
    QMutexLocker lock(&m_mutex);
    m_silent = true;
    // Also clear client tracking so auto-stream stops sending.
    // The client address is restored when the next metis-start arrives.
    m_clientPort = 0;
}

void P1FakeRadio::resume()
{
    QMutexLocker lock(&m_mutex);
    m_silent = false;
    // m_clientAddress/m_clientPort will be repopulated when the reconnect
    // attempt sends a fresh metis-start.
}

void P1FakeRadio::setAutoStreamEnabled(bool enabled)
{
    {
        QMutexLocker lock(&m_mutex);
        m_autoStreamEnabled = enabled;
    }
    if (!m_thread) { return; }  // start() not called yet
    onRadioThread([this, enabled] {
        if (!m_streamTimer) { return; }
        if (enabled) {
            if (!m_streamTimer->isActive()) { m_streamTimer->start(); }
        } else {
            if (m_streamTimer->isActive()) { m_streamTimer->stop(); }
        }
    });
}

void P1FakeRadio::skipEp6Sequence(quint32 count)
{
    QMutexLocker lock(&m_mutex);
    m_ep6Seq += count;
}

quint16 P1FakeRadio::clientPort() const
{
    QMutexLocker lock(&m_mutex);
    return m_clientPort;
}

int P1FakeRadio::ep2FramesReceived() const
{
    QMutexLocker lock(&m_mutex);
    return m_ep2Count;
}

QList<QByteArray> P1FakeRadio::ep2CcReceived() const
{
    QMutexLocker lock(&m_mutex);
    return m_ep2Cc;
}

void P1FakeRadio::clearEp2CcLog()
{
    QMutexLocker lock(&m_mutex);
    m_ep2Cc.clear();
}

bool P1FakeRadio::isRunning() const
{
    QMutexLocker lock(&m_mutex);
    return m_running;
}

int P1FakeRadio::metisStopCount() const
{
    QMutexLocker lock(&m_mutex);
    return m_stopCount;
}

QList<QByteArray> P1FakeRadio::metisCommandsReceived() const
{
    QMutexLocker lock(&m_mutex);
    return m_metisCommands;
}

void P1FakeRadio::setFirmwareVersion(int fw)
{
    QMutexLocker lock(&m_mutex);
    m_firmwareVersion = fw;
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

        {
            QMutexLocker lock(&m_mutex);
            if (m_silent) { continue; }
        }

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
    int firmware = 0;
    {
        QMutexLocker lock(&m_mutex);
        firmware = m_firmwareVersion;
    }
    reply[9]  = static_cast<char>(firmware & 0xFF);  // configurable fw version (default 72)
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
    QMutexLocker lock(&m_mutex);
    m_metisCommands.append(pkt);
    const quint8 cmd = static_cast<quint8>(pkt[3]);
    if (cmd == 0x01 || cmd == 0x02 || cmd == 0x03) {
        // Start streaming — remember client address
        m_running       = true;
        m_clientAddress = from;
        m_clientPort    = port;
    } else if (cmd == 0x00) {
        m_running = false;
        ++m_stopCount;
        // The C&C log starts again at each stop, so a test reads what a
        // (re)start sent, not frames that were still in flight before it.
        m_ep2Cc.clear();
    }
}

// ---------------------------------------------------------------------------
// handleEp2Frame — count ep2 command frames from the client and keep their
// C&C bytes (subframe 0 at offset 11, subframe 1 at offset 523)
// ---------------------------------------------------------------------------
void P1FakeRadio::handleEp2Frame(const QByteArray& pkt)
{
    if (pkt.size() == 1032) {
        QMutexLocker lock(&m_mutex);
        ++m_ep2Count;
        m_ep2Cc.append(pkt.mid(11, 5) + pkt.mid(523, 5));
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
    if (!m_thread) { return; }
    onRadioThread([this, count] { writeEp6Frames(count); });
}

void P1FakeRadio::writeEp6Frames(int count)
{
    QHostAddress address;
    quint16 port = 0;
    quint32 first = 0;
    {
        QMutexLocker lock(&m_mutex);
        if (m_clientPort == 0) { return; }
        address = m_clientAddress;
        port = m_clientPort;
        first = m_ep6Seq;
        m_ep6Seq += static_cast<quint32>(count);
    }
    if (!m_socket) { return; }

    for (int i = 0; i < count; ++i) {
        QByteArray frame = buildEp6Frame(first + static_cast<quint32>(i), 1);
        m_socket->writeDatagram(frame, address, port);
    }
}

// ---------------------------------------------------------------------------
// onAutoStreamTick — fires every 10ms; sends one ep6 frame if running and
// not silent. Simulates the continuous ep6 cadence a real HPSDR radio sends
// after receiving metis-start.
// ---------------------------------------------------------------------------
void P1FakeRadio::onAutoStreamTick()
{
    {
        QMutexLocker lock(&m_mutex);
        if (!m_running || m_silent) { return; }
    }
    writeEp6Frames(1);
}

} // namespace NereusSDR::Test
