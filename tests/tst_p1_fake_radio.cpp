// no-port-check: NereusSDR-original test of a test fake.
//
// P1FakeRadio streams like a radio while the thread that made it is busy
// (R-R3-49 load round). ConnectableRadioModel's synchronous WDSP start and
// the other connect work run on the test's thread; with the fake's socket
// and stream timer on that thread too, a busy two seconds there (a loaded
// machine) kept the fake from reading the metis-start at all, and
// P1RadioConnection's real 2 s connect watchdog fired on the test, not on
// the radio (tst_daemon_audio_transitions, tst_slice_meter_pump_readings).
//
// Modification history (NereusSDR):
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (R-R3-49).

#include <QtTest>
#include <QNetworkDatagram>
#include <QUdpSocket>

#include <chrono>

#include "fakes/P1FakeRadio.h"

using NereusSDR::Test::P1FakeRadio;

namespace {

QByteArray metis(quint8 command)
{
    QByteArray pkt(64, '\0');
    pkt[0] = char(0xEF);
    pkt[1] = char(0xFE);
    pkt[2] = char(0x04);
    pkt[3] = char(command);
    return pkt;
}

} // namespace

class TstP1FakeRadio : public QObject {
    Q_OBJECT

private slots:
    // The calling thread runs no event loop after it sends the start, as a
    // Core opening its channels does not: the fake still reads the start
    // and streams. The wait below is only for the fake's own thread to be
    // scheduled; a fake on the calling thread never answers here at all.
    void streamsWhileTheCallingThreadIsBusy()
    {
        P1FakeRadio fake;
        fake.start();
        QUdpSocket client;
        QVERIFY(client.bind(QHostAddress::LocalHost, 0));
        QCOMPARE(client.writeDatagram(metis(0x01), fake.localAddress(), fake.localPort()),
                 qint64(64));

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        int ep6 = 0;
        while (ep6 < 3 && std::chrono::steady_clock::now() < deadline) {
            if (!client.waitForReadyRead(50)) {
                continue;
            }
            while (client.hasPendingDatagrams()) {
                const QByteArray frame = client.receiveDatagram().data();
                if (frame.size() == 1032 && quint8(frame[0]) == 0xEF
                    && quint8(frame[1]) == 0xFE && frame[2] == 0x01 && frame[3] == 0x06) {
                    ++ep6;
                }
            }
        }
        QCOMPARE(ep6, 3);
        QVERIFY(fake.isRunning());
        QCOMPARE(fake.metisCommandsReceived().size(), 1);
    }

    // What a caller asks for has happened by the time the call returns.
    void callsTakeEffectBeforeTheyReturn()
    {
        P1FakeRadio fake;
        fake.setAutoStreamEnabled(false);
        fake.start();
        QUdpSocket client;
        QVERIFY(client.bind(QHostAddress::LocalHost, 0));
        client.writeDatagram(metis(0x01), fake.localAddress(), fake.localPort());
        QTRY_VERIFY(fake.isRunning());

        fake.sendEp6Frames(4);
        int received = 0;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (received < 4 && std::chrono::steady_clock::now() < deadline) {
            if (client.waitForReadyRead(50)) {
                while (client.hasPendingDatagrams()) {
                    client.receiveDatagram();
                    ++received;
                }
            }
        }
        QCOMPARE(received, 4);

        client.writeDatagram(metis(0x00), fake.localAddress(), fake.localPort());
        QTRY_COMPARE(fake.metisStopCount(), 1);
        QVERIFY(!fake.isRunning());
        fake.stop();
        QCOMPARE(fake.localPort(), quint16(0));
    }
};

QTEST_GUILESS_MAIN(TstP1FakeRadio)
#include "tst_p1_fake_radio.moc"
