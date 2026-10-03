// tests/tst_p1_loopback_connection.cpp
//
// Phase 3I Task 9 — end-to-end integration test for P1RadioConnection.
// Uses P1FakeRadio (loopback UDP) to verify:
//   1. State transitions: Disconnected → Connecting → Connected
//   2. iqDataReceived emits correctly-shaped interleaved I/Q QVector<float>
//   3. disconnect() stops the fake's "running" state
//   4. the radio's frames reach this connection's socket, and no other
//      socket on the machine bound to the address the radio answers can
//      take them (tst_slice_cap_every_path's lost reconnect)
//
// Uses QTEST_MAIN (not APPLESS_MAIN) — requires QCoreApplication for socket I/O.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QUdpSocket>
#include <atomic>
#include "core/P1RadioConnection.h"
#include "core/RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/HpsdrModel.h"
#include "core/HermesLiteBandwidthMonitor.h"
#include "fakes/P1FakeRadio.h"

using namespace NereusSDR;
using NereusSDR::Test::P1FakeRadio;

namespace {
// Issue #258 regression — message handler that counts "ep6 stream established"
// log lines emitted during the test.  Installed once per test method (init())
// and uninstalled (cleanup()) so the count is fresh between cases.
static std::atomic<int> g_ep6EstablishedLogCount{0};
static QtMessageHandler g_previousMsgHandler = nullptr;

static void countEp6EstablishedLogs(QtMsgType type,
                                    const QMessageLogContext& ctx,
                                    const QString& msg)
{
    if (msg.contains(QStringLiteral("Connected, ep6 stream established"))) {
        g_ep6EstablishedLogCount.fetch_add(1);
    }
    if (g_previousMsgHandler) {
        g_previousMsgHandler(type, ctx, msg);
    }
}
} // namespace

class TestP1LoopbackConnection : public QObject {
    Q_OBJECT

private:
    // Every address: dual-stack Any, or IPv4 Any when Qt keeps the IPv4
    // socket a failed bind to an IPv4 address left behind.
    static bool listensEverywhere(const QHostAddress& a) {
        return a == QHostAddress(QHostAddress::Any)
            || a == QHostAddress(QHostAddress::AnyIPv4);
    }

    // The receive buffer the kernel grants a socket asking what
    // P1RadioConnection asks (0x400000); the kernel may cap it.
    static int expectedReceiveBuffer() {
        QUdpSocket probe;
        if (!probe.bind(QHostAddress::Any, 0)) { return -2; }
        probe.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,
                              QVariant(0x400000));
        return probe.socketOption(QAbstractSocket::ReceiveBufferSizeSocketOption).toInt();
    }

    RadioInfo makeInfo(P1FakeRadio& fake) const {
        RadioInfo info;
        info.address         = fake.localAddress();
        info.port            = fake.localPort();
        info.boardType       = HPSDRHW::HermesLite;
        info.protocol        = ProtocolVersion::Protocol1;
        info.macAddress      = QStringLiteral("aa:bb:cc:11:22:33");
        info.firmwareVersion = 72;
        info.name            = QStringLiteral("FakeHL2");
        return info;
    }

private slots:
    // Test 1: Full RX path end-to-end
    void rxPathEndToEnd() {
        P1FakeRadio fake;
        fake.start();

        P1RadioConnection conn;
        conn.init();

        QSignalSpy stateSpy(&conn, &RadioConnection::connectionStateChanged);
        QSignalSpy dataSpy(&conn,  &RadioConnection::iqDataReceived);

        conn.connectToRadio(makeInfo(fake));

        // Wait for Connected state (timeout 3s)
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);

        // Wait for the fake to process the metis-start datagram.
        // Both sockets are in the same thread — event loop must tick for the
        // fake's readyRead to fire after connectToRadio() sends the start packet.
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);

        // Fake sends 10 ep6 frames
        fake.sendEp6Frames(10);

        // Wait for first iqDataReceived emission
        QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

        // Verify sample format — hwReceiverIndex=0, interleaved I/Q floats.
        // Use first() rather than last(): the fake streams 10 ep6 frames and
        // dataSpy may have captured additional emissions between QTRY_VERIFY
        // returning and this line running (e.g. rx=1 from a subsequent frame
        // if the connection ever fans out to multiple DDCs). The invariant
        // the test checks is "the first iqDataReceived emission is rx=0 with
        // the I=0.5 / Q=0 fake payload," which first() encodes unambiguously.
        auto args    = dataSpy.first();
        int  rxIdx   = args.at(0).toInt();
        QCOMPARE(rxIdx, 0);

        auto samples = args.at(1).value<QVector<float>>();
        QVERIFY(samples.size() > 0);
        QVERIFY((samples.size() % 2) == 0);  // interleaved I/Q pairs

        // The fake emits I=0.5, Q=0.0. Verify at least one I sample is ~0.5.
        // samples[0] = I0, samples[1] = Q0
        QVERIFY(qAbs(samples[0] - 0.5f) < 0.001f);
        QVERIFY(qAbs(samples[1] - 0.0f) < 0.001f);

        conn.disconnect();
        fake.stop();
    }

    // R-R3-21: Diagnostics > Connection Quality's "EP6 sequence gaps".
    // Each ep6 frame whose sequence number is not one past the last counts
    // one error, as P1RadioConnection::onReadyRead documents.
    void ep6SequenceGapsAreCounted() {
        P1FakeRadio fake;
        fake.setAutoStreamEnabled(false);
        fake.start();

        HermesLiteBandwidthMonitor bw;
        P1RadioConnection conn;
        conn.init();
        conn.setBandwidthMonitor(&bw);
        QSignalSpy frames(&conn, &RadioConnection::frameReceived);

        conn.connectToRadio(makeInfo(fake));
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);

        fake.sendEp6Frames(5);
        QTRY_COMPARE_WITH_TIMEOUT(frames.count(), 5, 3000);
        const int before = bw.ep6SequenceErrorCount();

        // In-order frames add nothing.
        fake.sendEp6Frames(3);
        QTRY_COMPARE_WITH_TIMEOUT(frames.count(), 8, 3000);
        QCOMPARE(bw.ep6SequenceErrorCount(), before);

        // A jump in the sequence counts one gap.
        fake.skipEp6Sequence(4);
        fake.sendEp6Frames(2);
        QTRY_COMPARE_WITH_TIMEOUT(frames.count(), 10, 3000);
        QCOMPARE(bw.ep6SequenceErrorCount(), before + 1);

        conn.disconnect();
        fake.stop();
    }

    // (Removed 2026-04-13) firmwareBelowMinimumRefusesConnect — the firmware
    // refusal path was removed from P1RadioConnection because Thetis enforces
    // no equivalent floor for the boards the previous test covered. See
    // BoardCapabilities.cpp file-header comment for the audit details.

    // Test 3: disconnect() stops the radio stream
    void disconnectStopsData() {
        P1FakeRadio fake;
        fake.start();

        P1RadioConnection conn;
        conn.init();

        conn.connectToRadio(makeInfo(fake));
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);
        // Wait for the fake to register the metis-start before issuing
        // metis-stop — otherwise the stop can race ahead of the start in
        // the fake's event loop and m_running ends up true.
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);

        conn.disconnect();
        QCOMPARE(conn.state(), ConnectionState::Disconnected);

        // The fake should have received a metis-stop and cleared m_running.
        // Use QTRY_VERIFY because the stop is processed asynchronously by
        // the fake's readyRead slot one event-loop turn after disconnect().
        QTRY_VERIFY_WITH_TIMEOUT(!fake.isRunning(), 1000);

        fake.stop();
    }

    // tst_slice_cap_every_path once lost a reconnect: the fake streamed, and
    // P1RadioConnection's 2 s connect watchdog still saw no ep6. The
    // connection bound its socket to every address (Any) on a port the OS
    // chose. On macOS the OS can hand such a socket a port that another
    // socket already holds on one address (127.0.0.1, which every test fake
    // and some of the app's own helpers bind), and a datagram to that
    // address and port goes to the socket bound to the address, not to the
    // connection. Another program took the radio's frames. The connection
    // now binds the address itself, as Thetis does (the cites are at
    // P1RadioConnection::bindToRadioFacingAddress), where no other socket
    // can share the port.
    //
    // The OS will not hand the connection a chosen port, so this test binds
    // the other socket second, the same sharing in the other order: asked
    // with ShareAddress (SO_REUSEADDR), macOS lets it share a port bound to
    // Any, and refuses it a port already bound to the same address.
    // The collision reproduces only on macOS: a pass on Linux or Windows
    // does not cover the fix.
    void anotherSocketOnTheRadiosAddressCannotTakeItsFrames() {
        P1FakeRadio fake;
        fake.start();

        P1RadioConnection conn;
        conn.init();
        QSignalSpy frames(&conn, &RadioConnection::frameReceived);

        conn.connectToRadio(makeInfo(fake));
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(fake.clientPort() != 0, 3000);
        const quint16 hostPort = fake.clientPort();

        QUdpSocket other;
        const bool otherBound =
            other.bind(fake.localAddress(), hostPort, QAbstractSocket::ShareAddress);

        frames.clear();
        QTRY_VERIFY_WITH_TIMEOUT(frames.count() >= 5, 2000);
        QVERIFY2(!otherBound || !other.hasPendingDatagrams(),
                 "the radio's frames went to another socket");

        conn.disconnect();
        fake.stop();
    }

    // With no route to the radio, connectToRadio warns and listens on every
    // address. disconnect() closes the socket, so on a reconnect the
    // fallback must bind it again, with the buffer sizes: before, it left
    // the socket closed and Qt bound it on the first send without them.
    void noRouteOnAReconnectBindsEveryAddressWithTheBuffers() {
        P1FakeRadio fake;
        fake.start();

        P1RadioConnection conn;
        conn.init();
        conn.connectToRadio(makeInfo(fake));
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);
        conn.disconnect();
        QTRY_VERIFY_WITH_TIMEOUT(!fake.isRunning(), 1000);

        conn.setRadioFacingAddressForTest(QHostAddress());
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("^P1: no local address reaches")));
        conn.connectToRadio(makeInfo(fake));

        QCOMPARE(conn.socketStateForTest(), QAbstractSocket::BoundState);
        QVERIFY(listensEverywhere(conn.socketAddressForTest()));
        QCOMPARE(conn.socketReceiveBufferForTest(), expectedReceiveBuffer());
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);

        conn.disconnect();
        fake.stop();
    }

    // An address this host cannot bind (TEST-NET-1, RFC 5737) takes the
    // same fallback, with its own warning.
    void anUnbindableAddressBindsEveryAddressWithTheBuffers() {
        P1FakeRadio fake;
        fake.start();

        P1RadioConnection conn;
        conn.init();
        conn.setRadioFacingAddressForTest(QHostAddress(QStringLiteral("192.0.2.1")));
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("^P1: could not listen on")));
        conn.connectToRadio(makeInfo(fake));

        QCOMPARE(conn.socketStateForTest(), QAbstractSocket::BoundState);
        QVERIFY(listensEverywhere(conn.socketAddressForTest()));
        QCOMPARE(conn.socketReceiveBufferForTest(), expectedReceiveBuffer());
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);

        conn.disconnect();
        fake.stop();
    }

    // Issue #258 regression — when a burst of N 1032-byte ep6 datagrams
    // lands in a single readyRead batch while state is still Connecting,
    // the "P1: Connected, ep6 stream established" log line must fire
    // EXACTLY ONCE, not once per datagram.  Field repro: first connect
    // emitted 181 spurious lines in <10 ms because cs was a const local
    // snapshot read outside the drain loop.
    void firstConnectLogFiresExactlyOncePerBatch() {
        P1FakeRadio fake;
        // Disable the 10 ms auto-stream timer so we have exclusive control
        // over when ep6 frames arrive.  Otherwise the first auto-stream
        // tick would promote state to Connected before our burst lands,
        // masking the bug (state==Connected -> log branch not taken).
        fake.setAutoStreamEnabled(false);
        fake.start();

        // Install the counting message handler AFTER fake.start() so the
        // fake's own diagnostics aren't counted (they don't match the
        // substring, but belt-and-braces).
        g_ep6EstablishedLogCount.store(0);
        g_previousMsgHandler = qInstallMessageHandler(countEp6EstablishedLogs);

        P1RadioConnection conn;
        conn.init();

        RadioInfo info;
        info.address         = fake.localAddress();
        info.port            = fake.localPort();
        info.boardType       = HPSDRHW::HermesLite;
        info.protocol        = ProtocolVersion::Protocol1;
        info.macAddress      = QStringLiteral("aa:bb:cc:11:22:33");
        info.firmwareVersion = 72;
        info.name            = QStringLiteral("FakeHL2");
        conn.connectToRadio(info);

        // Wait for the fake to receive metis-start (m_running flips true)
        // — that means conn is now in Connecting waiting for first ep6.
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);
        QCOMPARE(conn.state(), ConnectionState::Connecting);

        // Burst-send 100 ep6 frames in a tight loop.  All 100 land in
        // the OS UDP receive queue before conn's event loop ticks; when
        // onReadyRead fires it drains all 100 in one while-loop pass.
        fake.sendEp6Frames(100);

        // Pump the event loop until the conn transitions to Connected
        // (i.e. at least the first of the 100 frames has been drained
        // through onReadyRead).
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);

        // Bug A: pre-fix this was 100 (once per datagram in the batch).
        // Post-fix: 1.
        QCOMPARE(g_ep6EstablishedLogCount.load(), 1);

        qInstallMessageHandler(g_previousMsgHandler);
        g_previousMsgHandler = nullptr;

        conn.disconnect();
        fake.stop();
    }

    // Verifies EP2 (host→radio) cadence matches the spec's 48 kHz / 126
    // samples-per-packet rate of ~381 pps. Before the pacer fix the rate
    // was ~40 pps (once per 25 ms watchdog tick), starving the radio's
    // audio DAC. See issue #38 pcap analysis.
    void ep2PaceRateMatchesAudioClock() {
        P1FakeRadio fake;
        fake.start();

        P1RadioConnection conn;
        conn.init();
        conn.connectToRadio(makeInfo(fake));
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);

        // Baseline count after connection is up — discard discovery/start framing.
        const int baseline = fake.ep2FramesReceived();

        // Sample for 500 ms.
        QTest::qWait(500);

        const int delta = fake.ep2FramesReceived() - baseline;
        // At 380.95 pps ideal we expect ~190 packets in 500 ms. Windows QTimer
        // jitter + catch-up loop settle the observed rate around 300-400 pps,
        // so assert a floor of 100 packets (200 pps) which is still 5x the
        // broken 40 pps watchdog cadence.
        QVERIFY2(delta >= 100,
            qPrintable(QString("EP2 rate too slow: %1 packets in 500 ms (=%2 pps)")
                .arg(delta).arg(delta * 2)));

        conn.disconnect();
        fake.stop();
    }
};

QTEST_MAIN(TestP1LoopbackConnection)
#include "tst_p1_loopback_connection.moc"
