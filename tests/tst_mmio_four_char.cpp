// no-port-check: NereusSDR-original tests for the MMIO 4-character
// endpoint ID, direction, terminator and outbound queue.
// Modification history (NereusSDR):
//   2026-10-25: first version. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
#include <QtTest>
#include <QHostAddress>
#include <QNetworkDatagram>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>

#include "core/AppSettings.h"
#include "core/mmio/ExternalVariableEngine.h"
#include "core/mmio/FourChar.h"
#include "core/mmio/MmioEndpoint.h"
#include "core/mmio/TcpClientEndpointWorker.h"
#include "core/mmio/UdpEndpointWorker.h"

using namespace NereusSDR;

namespace {

const QUuid kGuid(QStringLiteral("3f2504e0-4f89-11d3-9a0c-0305e82c3301"));
const QUuid kLegacyGuid(QStringLiteral("8c0c8a52-5f8b-4b8e-9d0e-1a2b3c4d5e6f"));

QString legacyKey(const QString& prop)
{
    return QStringLiteral("MmioEndpoints/%1/%2")
        .arg(kLegacyGuid.toString(QUuid::WithoutBraces), prop);
}

// Writes an endpoint the way a build from before direction, terminator
// and IDs existed saved it.
void writeLegacyEndpoint()
{
    auto& s = AppSettings::instance();
    s.setValue(legacyKey(QStringLiteral("Name")), QStringLiteral("Legacy"));
    s.setValue(legacyKey(QStringLiteral("Transport")), QStringLiteral("UdpListener"));
    s.setValue(legacyKey(QStringLiteral("Format")), QStringLiteral("Json"));
    s.setValue(legacyKey(QStringLiteral("Host")), QStringLiteral("127.0.0.1"));
    // Port 0: the listener takes any free loopback port.
    s.setValue(legacyKey(QStringLiteral("Port")), QStringLiteral("0"));
    s.setValue(legacyKey(QStringLiteral("Device")), QString());
    s.setValue(legacyKey(QStringLiteral("Baud")), QStringLiteral("9600"));
}

} // namespace

class TestMmioFourChar : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        ExternalVariableEngine::instance().shutdown();
        AppSettings::instance().clear();
    }

    void cleanup()
    {
        ExternalVariableEngine::instance().shutdown();
        AppSettings::instance().clear();
    }

    void fourCharMatchesThetis_data()
    {
        QTest::addColumn<QString>("data1");
        QTest::addColumn<int>("data2");
        QTest::addColumn<QString>("expected");
        QTest::newRow("webimage") << QStringLiteral("webimage") << 0 << QStringLiteral("web4");
        QTest::newRow("ledindicator") << QStringLiteral("ledindicator") << 0 << QStringLiteral("tCyp");
        QTest::newRow("network") << QStringLiteral("127.0.0.1") << 9000 << QStringLiteral("pcWQ");
    }

    void fourCharMatchesThetis()
    {
        QFETCH(QString, data1);
        QFETCH(int, data2);
        QFETCH(QString, expected);
        QCOMPARE(fourChar(data1, data2, kGuid), expected);
    }

    void newEndpointDefaults()
    {
        MmioEndpoint ep;
        QCOMPARE(ep.direction(), MmioEndpoint::Direction::In);
        QCOMPARE(ep.terminator(), MmioEndpoint::Terminator::None);
        QCOMPARE(ep.host(), QStringLiteral("127.0.0.1"));
        QCOMPARE(ep.port(), quint16(9000));
        ep.setGuid(kGuid);
        QCOMPARE(ep.fourChar(), QStringLiteral("pcWQ"));
        // The ID stays once worked out; a host or port edit keeps it.
        ep.setPort(9001);
        QCOMPARE(ep.fourChar(), QStringLiteral("pcWQ"));
        // A new guid gives a new ID.
        ep.setGuid(kLegacyGuid);
        QCOMPARE(ep.fourChar(), fourChar(QStringLiteral("127.0.0.1"), 9001, kLegacyGuid));
    }

    void serialEndpointIdUsesDeviceAndBaud()
    {
        MmioEndpoint ep;
        ep.setGuid(kGuid);
        ep.setTransport(MmioEndpoint::Transport::Serial);
        ep.setSerialDevice(QStringLiteral("/dev/tty.usbserial"));
        ep.setSerialBaud(9600);
        QCOMPARE(ep.fourChar(), fourChar(QStringLiteral("/dev/tty.usbserial"), 9608, kGuid));
    }

    void legacyEndpointLoadsAsInWithStableId()
    {
        writeLegacyEndpoint();
        auto& engine = ExternalVariableEngine::instance();
        engine.init();

        MmioEndpoint* ep = engine.endpoint(kLegacyGuid);
        QVERIFY(ep);
        QCOMPARE(ep->direction(), MmioEndpoint::Direction::In);
        QCOMPARE(ep->terminator(), MmioEndpoint::Terminator::None);
        const QString id = ep->fourChar();
        QCOMPARE(id, fourChar(QStringLiteral("127.0.0.1"), 0, kLegacyGuid));
        QCOMPARE(AppSettings::instance().value(legacyKey(QStringLiteral("FourChar"))).toString(), id);

        engine.shutdown();
        engine.init();
        ep = engine.endpoint(kLegacyGuid);
        QVERIFY(ep);
        QCOMPARE(ep->fourChar(), id);
        QCOMPARE(ep->direction(), MmioEndpoint::Direction::In);
    }

    void settingsRoundTrip()
    {
        writeLegacyEndpoint();
        auto& engine = ExternalVariableEngine::instance();
        engine.init();
        MmioEndpoint* ep = engine.endpoint(kLegacyGuid);
        QVERIFY(ep);
        const QString id = ep->fourChar();
        ep->setDirection(MmioEndpoint::Direction::Both);
        ep->setTerminator(MmioEndpoint::Terminator::Custom);
        ep->setCustomTerminator(QStringLiteral(";\\r"));
        ep->setUdpTargetHost(QStringLiteral("127.0.0.1"));
        ep->setUdpTargetPort(10001);
        engine.updateEndpoint(ep);

        engine.shutdown();
        engine.init();
        ep = engine.endpoint(kLegacyGuid);
        QVERIFY(ep);
        QCOMPARE(ep->direction(), MmioEndpoint::Direction::Both);
        QCOMPARE(ep->terminator(), MmioEndpoint::Terminator::Custom);
        QCOMPARE(ep->customTerminator(), QStringLiteral(";\\r"));
        QCOMPARE(ep->outboundTerminatorString(), QStringLiteral(";\r"));
        QCOMPARE(ep->udpTargetPort(), quint16(10001));
        QCOMPARE(ep->fourChar(), id);
    }

    void enqueueOnInEndpointLeavesQueueEmpty()
    {
        writeLegacyEndpoint();
        auto& engine = ExternalVariableEngine::instance();
        engine.init();
        MmioEndpoint* ep = engine.endpoint(kLegacyGuid);
        QVERIFY(ep);
        engine.enqueueOutbound(kLegacyGuid, QStringLiteral("hello"));
        QCOMPARE(ep->outboundQueueSize(), 0);

        MmioEndpoint standalone;
        QVERIFY(!standalone.enqueueOutbound(QStringLiteral("hello")));
        QCOMPARE(standalone.outboundQueueSize(), 0);
    }

    void queueKeepsAtMostMaxOutbound()
    {
        MmioEndpoint ep;
        ep.setDirection(MmioEndpoint::Direction::Out);
        for (int i = 0; i < 1005; ++i) {
            ep.enqueueOutbound(QString::number(i));
        }
        QCOMPARE(ep.outboundQueueSize(), 1000);
        QCOMPARE(kMaxOutbound, 1000);
        // The oldest messages were dropped.
        QCOMPARE(ep.takeOutboundBatch(1), QStringList{QStringLiteral("5")});
    }

    void batchIsAtMostTwentyAndTerminated()
    {
        MmioEndpoint ep;
        ep.setDirection(MmioEndpoint::Direction::Out);
        for (int i = 0; i < 25; ++i) {
            ep.enqueueOutbound(QString::number(i));
        }
        QCOMPARE(kSendBatch, 20);
        const QStringList batch = ep.takeOutboundBatch();
        QCOMPARE(batch.size(), 20);
        QCOMPARE(batch.first(), QStringLiteral("0"));
        QCOMPARE(ep.takeOutboundBatch().size(), 5);

        QCOMPARE(MmioEndpoint::composeOutbound({QStringLiteral("a"), QStringLiteral("b")},
                                               QStringLiteral("\r\n")),
                 QByteArray("a\r\nb\r\n"));
        // .NET Encoding.ASCII writes '?' for anything above 0x7F.
        QCOMPARE(MmioEndpoint::composeOutbound({QStringLiteral("é")}, QString()),
                 QByteArray("?"));

        ep.setTerminator(MmioEndpoint::Terminator::Custom);
        QCOMPARE(ep.outboundTerminatorString(), QString(QChar(u'\0')));
    }

    void udpWorkerSendsBatchesJoinedWithCrLf()
    {
        QUdpSocket receiver;
        QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));

        MmioEndpoint ep;
        ep.setGuid(kGuid);
        ep.setDirection(MmioEndpoint::Direction::Out);
        ep.setTerminator(MmioEndpoint::Terminator::CrLf);
        ep.setHost(QStringLiteral("127.0.0.1"));
        ep.setPort(0);
        ep.setUdpTargetHost(QStringLiteral("127.0.0.1"));
        ep.setUdpTargetPort(receiver.localPort());
        QStringList expected;
        for (int i = 0; i < 25; ++i) {
            ep.enqueueOutbound(QStringLiteral("m%1").arg(i));
            expected << QStringLiteral("m%1").arg(i);
        }

        UdpEndpointWorker worker(&ep);
        worker.start(); // sends what is queued

        QList<QByteArray> datagrams;
        const auto drain = [&receiver, &datagrams] {
            while (receiver.hasPendingDatagrams()) {
                datagrams.append(receiver.receiveDatagram().data());
            }
            return datagrams.size() >= 2;
        };
        QTRY_VERIFY_WITH_TIMEOUT(drain(), 5000);
        QCOMPARE(datagrams.size(), 2);
        QCOMPARE(datagrams.at(0), (expected.mid(0, 20).join(QStringLiteral("\r\n"))
                                   + QStringLiteral("\r\n")).toLatin1());
        QCOMPARE(datagrams.at(1), (expected.mid(20).join(QStringLiteral("\r\n"))
                                   + QStringLiteral("\r\n")).toLatin1());
        QCOMPARE(ep.outboundQueueSize(), 0);
        worker.stop();
    }

    void tcpClientWorkerSendsOnConnect()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        MmioEndpoint ep;
        ep.setGuid(kGuid);
        ep.setTransport(MmioEndpoint::Transport::TcpClient);
        ep.setDirection(MmioEndpoint::Direction::Both);
        ep.setTerminator(MmioEndpoint::Terminator::Lf);
        ep.setHost(QStringLiteral("127.0.0.1"));
        ep.setPort(server.serverPort());
        ep.enqueueOutbound(QStringLiteral("one"));
        ep.enqueueOutbound(QStringLiteral("two"));

        TcpClientEndpointWorker worker(&ep);
        worker.start();
        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 5000);
        QTcpSocket* peer = server.nextPendingConnection();
        QVERIFY(peer);
        QByteArray received;
        const auto drain = [peer, &received] {
            received.append(peer->readAll());
            return received.size() >= 8;
        };
        QTRY_VERIFY_WITH_TIMEOUT(drain(), 5000);
        QCOMPARE(received, QByteArray("one\ntwo\n"));
        worker.stop();
    }

    void lookupByFourChar()
    {
        writeLegacyEndpoint();
        auto& engine = ExternalVariableEngine::instance();
        engine.init();
        MmioEndpoint* ep = engine.endpoint(kLegacyGuid);
        QVERIFY(ep);
        QCOMPARE(engine.endpointByFourChar(ep->fourChar()), ep);
        const QString unknown = ep->fourChar() == QStringLiteral("zzzz")
            ? QStringLiteral("yyyy") : QStringLiteral("zzzz");
        QCOMPARE(engine.endpointByFourChar(unknown), nullptr);
        QCOMPARE(engine.endpointByFourChar(QString()), nullptr);
    }
};

QTEST_MAIN(TestMmioFourChar)
#include "tst_mmio_four_char.moc"
