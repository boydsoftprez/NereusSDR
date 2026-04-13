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

        // Verify sample format — hwReceiverIndex=0, interleaved I/Q floats
        auto args    = dataSpy.last();
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

    // Test 2: firmware below minimum refuses connect, emits FirmwareTooOld
    void firmwareBelowMinimumRefusesConnect() {
        P1FakeRadio fake;
        fake.setFirmwareVersion(50);  // HL2 minFirmwareVersion is 70 (see BoardCapabilities.cpp)
        fake.start();

        RadioInfo info;
        info.address         = fake.localAddress();
        info.port            = fake.localPort();
        info.boardType       = HPSDRHW::HermesLite;
        info.protocol        = ProtocolVersion::Protocol1;
        info.firmwareVersion = 50;   // below HL2 minimum
        info.macAddress      = QStringLiteral("aa:bb:cc:11:22:33");

        P1RadioConnection conn;
        conn.init();
        QSignalSpy errSpy(&conn, &RadioConnection::errorOccurred);
        conn.connectToRadio(info);

        // Should be in Error state synchronously — no async path needed for refusal.
        QTRY_VERIFY_WITH_TIMEOUT(conn.state() == ConnectionState::Error, 2000);
        QVERIFY(errSpy.count() >= 1);
        auto args = errSpy.first();
        auto code = args.at(0).value<RadioConnectionError>();
        QCOMPARE(code, RadioConnectionError::FirmwareTooOld);

        fake.stop();
    }

    // Test 3: disconnect() stops the radio stream
    void disconnectStopsData() {
        P1FakeRadio fake;
        fake.start();

        P1RadioConnection conn;
        conn.init();

        conn.connectToRadio(makeInfo(fake));
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);

        conn.disconnect();
        QCOMPARE(conn.state(), ConnectionState::Disconnected);

        // The fake should have received a metis-stop and cleared m_running
        QVERIFY(!fake.isRunning());

        fake.stop();
    }
};

QTEST_MAIN(TestP1LoopbackConnection)
#include "tst_p1_loopback_connection.moc"
