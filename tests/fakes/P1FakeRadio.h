// tests/fakes/P1FakeRadio.h
//
// Loopback test fake for Protocol 1 radio hardware.
// Binds a UDP socket on an ephemeral loopback port, responds to discovery
// probes, metis-start/stop commands, and can stream canned ep6 frames.
//
// Used by tst_p1_loopback_connection.cpp (Phase 3I Task 9) and
// tst_reconnect_on_silence.cpp (Phase 3I Task 10).

#pragma once
#include <QObject>
#include <QTimer>
#include <QUdpSocket>
#include <QHostAddress>

namespace NereusSDR::Test {

class P1FakeRadio : public QObject {
    Q_OBJECT
public:
    explicit P1FakeRadio(QObject* parent = nullptr);
    ~P1FakeRadio() override;

    // Bind the socket and start listening. Call before using.
    void start();

    // Close the socket cleanly.
    void stop();

    QHostAddress localAddress() const { return QHostAddress(QHostAddress::LocalHost); }
    quint16      localPort()    const;

    // Build `count` ep6 frames with I=0.5, Q=0.0 (DC tone) and send them
    // to the last client that sent a metis-start command.
    void sendEp6Frames(int count);

    // Temporarily ignore incoming packets AND stop auto-streaming ep6
    // (without unbinding). Simulates a radio that goes silent.
    void goSilent();
    // Resume handling packets and restart auto-streaming.
    void resume();

    int  ep2FramesReceived() const { return m_ep2Count; }
    bool isRunning()         const { return m_running; }

    // Override the firmware version reported in discovery replies (default: 72).
    void setFirmwareVersion(int fw) { m_firmwareVersion = fw; }

private slots:
    void onReadyRead();
    void onAutoStreamTick();

private:
    void handleDiscoveryProbe(const QHostAddress& from, quint16 port);
    void handleMetisCommand(const QByteArray& pkt, const QHostAddress& from, quint16 port);
    void handleEp2Frame(const QByteArray& pkt);

    // Build one 1032-byte ep6 frame with a fixed DC tone (I=0.5, Q=0.0).
    // Puts `seq` in the sequence field.
    QByteArray buildEp6Frame(quint32 seq, int numRx = 1);

    QUdpSocket*  m_socket{nullptr};
    QTimer*      m_streamTimer{nullptr};
    QHostAddress m_clientAddress;
    quint16      m_clientPort{0};
    bool         m_running{false};
    bool         m_silent{false};
    int          m_ep2Count{0};
    quint32      m_ep6Seq{0};
    int          m_firmwareVersion{72};  // default matches HL2 kHermesLite knownGoodFirmware
};

} // namespace NereusSDR::Test
