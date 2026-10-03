// tests/fakes/P1FakeRadio.h
//
// Loopback test fake for Protocol 1 radio hardware.
// Binds a UDP socket on an ephemeral loopback port, responds to discovery
// probes, metis-start/stop commands, and can stream canned ep6 frames.
//
// Used by tst_p1_loopback_connection.cpp (Phase 3I Task 9) and
// tst_reconnect_on_silence.cpp (Phase 3I Task 10).
//
// R-R3-49 load round: the fake runs on a thread of its own, as a radio is a
// device of its own. On the test's thread, a caller busy for two seconds
// (ConnectableRadioModel's synchronous WDSP start, a loaded machine) kept it
// from reading the metis-start and streaming, so P1RadioConnection's real
// 2 s connect watchdog fired on the test, not on the radio. Every call below
// runs on the fake's thread and returns once done, as before (frames sent
// have been written when sendEp6Frames() returns); the readers take a lock.

#pragma once
#include <QByteArray>
#include <QHostAddress>
#include <QList>
#include <QMutex>
#include <QObject>

#include <atomic>
#include <functional>
#include <memory>

class QThread;
class QTimer;
class QUdpSocket;

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
    quint16      localPort()    const { return m_port.load(); }

    // Build `count` ep6 frames with I=0.5, Q=0.0 (DC tone) and send them
    // to the last client that sent a metis-start command.
    void sendEp6Frames(int count);

    // Temporarily ignore incoming packets AND stop auto-streaming ep6
    // (without unbinding). Simulates a radio that goes silent.
    void goSilent();
    // Resume handling packets and restart auto-streaming.
    void resume();

    // Disable (or re-enable) the 10 ms auto-stream timer so a test has full
    // control over when ep6 frames arrive at the receiver.  Must be called
    // BEFORE start() to suppress the timer entirely; called after start() it
    // stops the timer in place.  Tests that need to burst-inject N ep6
    // frames into a single readyRead batch (e.g. the issue #258 first-
    // connect log-spam regression) call this before start(), then drive
    // frames manually with sendEp6Frames(N).  Default is enabled.
    void setAutoStreamEnabled(bool enabled);

    // Skip `count` ep6 sequence numbers, as a radio whose frames were lost
    // on the way would appear: the next frame sent carries a number `count`
    // past the one it would have had.
    void skipEp6Sequence(quint32 count);

    // The port the fake streams ep6 to: the sender port of the last
    // metis-start, as a radio answers the host that started it. 0 before
    // any start.
    quint16 clientPort() const;

    int  ep2FramesReceived() const;
    // The C&C bytes of every ep2 frame received, oldest first: ten bytes per
    // frame, C0..C4 of subframe 0 then C0..C4 of subframe 1 (frame offsets
    // 11-15 and 523-527). Frames dropped while silent are not recorded, and
    // the log is cleared at each metis-stop received.
    QList<QByteArray> ep2CcReceived() const;
    void clearEp2CcLog();
    bool isRunning()         const;
    int  metisStopCount()    const;
    // Every start/stop (EF FE 04 xx) datagram received, as it arrived on
    // the socket, oldest first (R-R3-49: tests assert on the wire).
    QList<QByteArray> metisCommandsReceived() const;

    // Override the firmware version reported in discovery replies (default: 72).
    void setFirmwareVersion(int fw);

private:
    // Runs `work` on the fake's thread and returns once it has run.
    void onRadioThread(const std::function<void()>& work);

    // The fake's thread only.
    void onReadyRead();
    void onAutoStreamTick();
    void handleDiscoveryProbe(const QHostAddress& from, quint16 port);
    void handleMetisCommand(const QByteArray& pkt, const QHostAddress& from, quint16 port);
    void handleEp2Frame(const QByteArray& pkt);
    void writeEp6Frames(int count);

    // Build one 1032-byte ep6 frame with a fixed DC tone (I=0.5, Q=0.0).
    // Puts `seq` in the sequence field.
    QByteArray buildEp6Frame(quint32 seq, int numRx = 1);

    std::unique_ptr<QThread> m_thread;
    // Lives on m_thread; the socket and the timer are its children.
    QObject*     m_radio{nullptr};
    QUdpSocket*  m_socket{nullptr};
    QTimer*      m_streamTimer{nullptr};
    std::atomic<quint16> m_port{0};

    // Everything below is shared between the fake's thread and callers.
    mutable QMutex m_mutex;
    QHostAddress m_clientAddress;
    quint16      m_clientPort{0};
    bool         m_running{false};
    bool         m_silent{false};
    bool         m_autoStreamEnabled{true};
    int          m_ep2Count{0};
    QList<QByteArray> m_ep2Cc;
    int          m_stopCount{0};
    QList<QByteArray> m_metisCommands;
    quint32      m_ep6Seq{0};
    int          m_firmwareVersion{72};  // arbitrary default; any value is now valid
};

} // namespace NereusSDR::Test
