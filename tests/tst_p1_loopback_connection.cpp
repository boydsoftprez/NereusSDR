// tests/tst_p1_loopback_connection.cpp
//
// Phase 3I Task 9 — end-to-end integration test for P1RadioConnection.
// Uses P1FakeRadio (loopback UDP) to verify:
//   1. State transitions: Disconnected → Connecting → Connected
//   2. iqDataReceived emits correctly-shaped interleaved I/Q QVector<float>
//   3. disconnect() stops the fake's "running" state
//
// Uses QTEST_MAIN (not APPLESS_MAIN) — requires QCoreApplication for socket I/O.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/P1RadioConnection.h"
#include "core/RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/HpsdrModel.h"
#include "fakes/P1FakeRadio.h"

using namespace NereusSDR;
using NereusSDR::Test::P1FakeRadio;

class TestP1LoopbackConnection : public QObject {
    Q_OBJECT

private:
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
