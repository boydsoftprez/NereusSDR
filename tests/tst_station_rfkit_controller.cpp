// no-port-check: NereusSDR-original. R-R3-47 / R-R3-48 / R-R3-22 / R-R3-25
// Core-owned RF-Kit RF2K-S: identity from /info before admission, the
// Core's configure, disconnect and switch rules, cancellation, and the
// Core switching the amp into TCI mode (once per connection) while the
// station's TCI server is on.
//
// An in-process HTTP server stands in for the amp's REST interface; its
// /info body is the one tst_rf2ks_connection_parse uses (the design doc's
// swagger and live probe, firmware G200C267). No real amplifier, radio or
// Core is contacted.
// J.J. Boyd (KG4VCF), September 2026; AI-assisted via Anthropic Claude Code.
#include <QtTest/QtTest>
#include <QTcpServer>
#include <QTcpSocket>

#include "core/AppSettings.h"
#include "core/Rf2ksConnection.h"
#include "core/StationRfKitController.h"
#include "models/RadioModel.h"
#include "models/RfKitModel.h"

using namespace NereusSDR;
using Phase = RfKitModel::ConnectionPhase;

namespace {

constexpr const char* kRf2ksInfo =
    R"({"device":"RF2K-S","software_version":{"GUI":200,"controller":267},"custom_device_name":"KG4VCF"})";

// The amp's REST interface, one request per connection (the connection
// asks for Connection: close).
class FakeAmp : public QTcpServer {
    Q_OBJECT
public:
    FakeAmp()
    {
        listen(QHostAddress::LocalHost, 0);
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* sock = nextPendingConnection()) {
                connect(sock, &QTcpSocket::readyRead, this, [this, sock] { serve(sock); });
            }
        });
    }

    QByteArray info = kRf2ksInfo;
    QByteArray operationalInterface = R"({"operational_interface":"UDP","error":""})";
    bool answering = true;
    QStringList requests;   // "GET /info", "PUT /operational-interface {...}"

    int count(const QString& request) const { return int(requests.count(request)); }

private:
    void serve(QTcpSocket* sock)
    {
        QByteArray& buffer = m_buffers[sock];
        buffer += sock->readAll();
        const int headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0) { return; }
        const QByteArray head = buffer.left(headerEnd);
        int length = 0;
        for (const QByteArray& line : head.split('\n')) {
            if (line.toLower().startsWith("content-length:")) {
                length = line.mid(15).trimmed().toInt();
            }
        }
        if (buffer.size() < headerEnd + 4 + length) { return; }
        const QByteArray body = buffer.mid(headerEnd + 4, length);
        const QList<QByteArray> first = head.left(head.indexOf('\r')).split(' ');
        const QString verb = QString::fromLatin1(first.value(0));
        const QString path = QString::fromLatin1(first.value(1));
        m_buffers.remove(sock);
        requests.append(body.isEmpty() ? verb + QLatin1Char(' ') + path
                                       : verb + QLatin1Char(' ') + path + QLatin1Char(' ')
                                             + QString::fromUtf8(body));
        if (!answering) {
            sock->abort();
            sock->deleteLater();
            return;
        }
        QByteArray reply;
        if (verb == QLatin1String("GET")) {
            if (path == QLatin1String("/info")) { reply = info; }
            else if (path == QLatin1String("/operational-interface")) { reply = operationalInterface; }
            else if (path == QLatin1String("/operate-mode")) { reply = R"({"operate_mode":"STANDBY"})"; }
            else { reply = "{}"; }
        }
        QByteArray response = "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n";
        response += "Content-Length: " + QByteArray::number(reply.size()) + "\r\n\r\n" + reply;
        sock->write(response);
        sock->flush();
        sock->disconnectFromHost();
    }

    QHash<QTcpSocket*, QByteArray> m_buffers;
};

} // namespace

class StationRfKitControllerTest : public QObject {
    Q_OBJECT

    static void prepare(RadioModel& model, const QString& mac = QStringLiteral("aa:bb:cc:dd:ee:81"))
    {
        model.enableStationAccessoryIdentity();
        model.setReceiveOnlyStationPolicy(true);
        RadioInfo radio;
        radio.macAddress = mac;
        model.setLastRadioInfoForTest(radio);
        model.setConnectionStateForTest(ConnectionState::Connected);
    }

private slots:
    void init()
    {
        AppSettings::instance().clear();
        AppSettings::instance().setValue(QStringLiteral("PeripheralsMigrationDone"),
                                         QStringLiteral("True"));
        // Poll slowly so the probe and the one control write are what the
        // fake amp sees.
        AppSettings::instance().setValue(QStringLiteral("RfKit_PollIntervalMs"),
                                         QStringLiteral("5000"));
    }
    void cleanup() { AppSettings::instance().clear(); }

    // An RF2K-S naming itself in /info is admitted, and only then counted
    // as connected; its identity is what the Core reports.
    void admitsAnRf2ksOnlyAfterItsInfo()
    {
        FakeAmp amp;
        Rf2ksConnection connection;
        RfKitModel model;
        model.bindConnection(&connection);
        StationRfKitController controller(&connection, &model);
        QVERIFY(connection.identityAdmissionRequired());
        QSignalSpy connected(&connection, &Rf2ksConnection::connected);

        controller.start(QStringLiteral("127.0.0.1"), amp.serverPort());
        QCOMPARE(model.connectionPhase(), Phase::Connecting);
        QVERIFY(!connection.isConnected());
        QTRY_COMPARE_WITH_TIMEOUT(model.connectionPhase(), Phase::Connected, 3000);
        QCOMPARE(connected.count(), 1);
        QCOMPARE(model.deviceModel(), QStringLiteral("RF2K-S"));
        QCOMPARE(model.deviceNickname(), QStringLiteral("KG4VCF"));
        QCOMPARE(model.deviceVersion(), QStringLiteral("G200C267"));
        QCOMPARE(model.configuredHost(), QStringLiteral("127.0.0.1"));
        QCOMPARE(model.configuredPort(), int(amp.serverPort()));
        QVERIFY(model.connectionError().isEmpty());
    }

    // Anything else at the address is refused, reported as a fault, and
    // never retried or polled.
    void refusesAnotherDeviceAndNeverRetries()
    {
        FakeAmp amp;
        amp.info = R"({"device":"PowerGeniusXL","software_version":{"GUI":1,"controller":2}})";
        Rf2ksConnection connection;
        RfKitModel model;
        model.bindConnection(&connection);
        StationRfKitController controller(&connection, &model);
        QSignalSpy connected(&connection, &Rf2ksConnection::connected);
        QSignalSpy faults(&connection, &Rf2ksConnection::faultObserved);

        controller.start(QStringLiteral("127.0.0.1"), amp.serverPort());
        QTRY_COMPARE_WITH_TIMEOUT(model.connectionPhase(), Phase::Error, 3000);
        QCOMPARE(connected.count(), 0);
        QVERIFY(!connection.isConnected());
        QVERIFY(!connection.reconnectPending());
        QVERIFY(model.connectionError().contains(QStringLiteral("PowerGeniusXL")));
        QVERIFY(model.connectionError().contains(QStringLiteral("not an RF-Kit RF2K-S")));
        QCOMPARE(faults.count(), 1);
        QCOMPARE(faults.first().at(0).toString(), QStringLiteral("identity"));
        const int seen = amp.requests.size();
        QTest::qWait(300);
        QCOMPARE(amp.requests.size(), seen);   // nothing more is asked
        QCOMPARE(amp.count(QStringLiteral("GET /info")), 1);
    }

    // M2 (R-R3-47): a reply with no device is a failed answer, retried; it
    // never drops an admitted amp for good. Only a reply that names another
    // product is refused.
    void answerThatNamesNoDeviceIsRetried()
    {
        FakeAmp amp;
        amp.info = R"({"hello":"world"})";
        Rf2ksConnection connection;
        connection.setPollIntervalMs(250);
        RfKitModel model;
        model.bindConnection(&connection);
        StationRfKitController controller(&connection, &model);
        QSignalSpy faults(&connection, &Rf2ksConnection::faultObserved);
        controller.start(QStringLiteral("127.0.0.1"), amp.serverPort());
        QTRY_COMPARE_WITH_TIMEOUT(model.connectionPhase(), Phase::Retrying, 3000);
        QVERIFY(connection.reconnectPending());
        QVERIFY(!connection.isConnected());
        QCOMPARE(faults.count(), 0);

        // The amp answers properly on a later try: admitted.
        amp.info = kRf2ksInfo;
        QTRY_COMPARE_WITH_TIMEOUT(model.connectionPhase(), Phase::Connected, 5000);
        QCOMPARE(model.deviceModel(), QStringLiteral("RF2K-S"));

        // One /info re-poll without a device: the amp stays admitted.
        amp.info = R"({"hello":"world"})";
        const int infos = amp.count(QStringLiteral("GET /info"));
        QTRY_VERIFY_WITH_TIMEOUT(amp.count(QStringLiteral("GET /info")) > infos, 6000);
        amp.info = kRf2ksInfo;
        QTest::qWait(200);
        QVERIFY(connection.isConnected());
        QCOMPARE(model.connectionPhase(), Phase::Connected);
        QCOMPARE(model.deviceModel(), QStringLiteral("RF2K-S"));
        for (const auto& fault : faults) {
            QVERIFY(fault.at(0).toString() != QStringLiteral("identity"));
        }
        controller.cancel();
    }

    // Nothing answering: the Core keeps trying (Retrying) unless automatic
    // retry is off, when it says why it stopped.
    void unansweredAddressRetriesOrSaysWhy()
    {
        FakeAmp amp;
        amp.answering = false;
        {
            Rf2ksConnection connection;
            RfKitModel model;
            model.bindConnection(&connection);
            StationRfKitController controller(&connection, &model);
            controller.start(QStringLiteral("127.0.0.1"), amp.serverPort());
            QTRY_COMPARE_WITH_TIMEOUT(model.connectionPhase(), Phase::Retrying, 3000);
            QVERIFY(connection.reconnectPending());
            controller.cancel();
            QCOMPARE(model.connectionPhase(), Phase::Disconnected);
            QVERIFY(!connection.reconnectPending());
        }
        {
            Rf2ksConnection connection;
            connection.setAutoReconnect(false);
            RfKitModel model;
            model.bindConnection(&connection);
            StationRfKitController controller(&connection, &model);
            controller.start(QStringLiteral("127.0.0.1"), amp.serverPort());
            QTRY_COMPARE_WITH_TIMEOUT(model.connectionPhase(), Phase::Error, 3000);
            QCOMPARE(model.connectionError(),
                     QStringLiteral("The RF-Kit amplifier did not answer at this address."));
            QVERIFY(!connection.reconnectPending());
        }
    }

    // Cancelled while the /info probe is on the wire: the late answer does
    // not bring the amp back.
    void cancelWhileIdentifyingNeverAdmits()
    {
        FakeAmp amp;
        Rf2ksConnection connection;
        RfKitModel model;
        model.bindConnection(&connection);
        StationRfKitController controller(&connection, &model);
        controller.start(QStringLiteral("127.0.0.1"), amp.serverPort());
        controller.cancel(/*disabled=*/true);
        QCOMPARE(model.connectionPhase(), Phase::Disabled);
        QTest::qWait(300);
        QVERIFY(!connection.isConnected());
        QCOMPARE(model.connectionPhase(), Phase::Disabled);
    }

    // R-R3-48: with the station's TCI server on, the admitted amp is put in
    // TCI mode through its web interface, once per connection; not while
    // the server is off, and not when it already is in TCI mode.
    void switchesTheAmpToTciOnceWhileBandFollowIsWanted()
    {
        FakeAmp amp;
        Rf2ksConnection connection;
        RfKitModel model;
        model.bindConnection(&connection);
        StationRfKitController controller(&connection, &model);
        const QString put = QStringLiteral(
            R"(PUT /operational-interface {"operational_interface":"TCI"})");

        controller.start(QStringLiteral("127.0.0.1"), amp.serverPort());
        QTRY_COMPARE_WITH_TIMEOUT(model.connectionPhase(), Phase::Connected, 3000);
        // The amp's interface is read by the poller; feed it directly.
        connection.injectJsonForTesting(QStringLiteral("/operational-interface"),
                                        amp.operationalInterface);
        QCOMPARE(model.operationalInterface(), QStringLiteral("UDP"));
        QTest::qWait(100);
        QCOMPARE(amp.count(put), 0);   // TCI server off: the amp is left alone

        controller.setBandFollowWanted(true);
        QTRY_COMPARE_WITH_TIMEOUT(amp.count(put), 1, 3000);
        QVERIFY(controller.tciModeRequestedForTesting());
        // A second report (still UDP, the operator switched it back) is not
        // fought on the same connection.
        connection.injectJsonForTesting(QStringLiteral("/operational-interface"),
                                        amp.operationalInterface);
        QTest::qWait(100);
        QCOMPARE(amp.count(put), 1);

        // A fresh connection to an amp already in TCI mode: nothing sent.
        amp.operationalInterface = R"({"operational_interface":"TCI","error":""})";
        controller.start(QStringLiteral("127.0.0.1"), amp.serverPort());
        QTRY_COMPARE_WITH_TIMEOUT(model.connectionPhase(), Phase::Connected, 3000);
        connection.injectJsonForTesting(QStringLiteral("/operational-interface"),
                                        amp.operationalInterface);
        QTest::qWait(100);
        QCOMPARE(amp.count(put), 1);
    }

    // I4 (R-R3-47): a window's Reset amp error reaches the admitted amp as
    // the request the local page's button sends (POST /error/reset);
    // refused, with nothing sent, while no amp is admitted.
    void resetErrorReachesTheAdmittedAmp()
    {
        FakeAmp amp;
        RadioModel model;
        prepare(model);
        QString reason;
        QVERIFY(model.setRfKitEnabledForStation(true, &reason));
        QVERIFY(!model.resetRfKitErrorForStation(&reason));
        QCOMPARE(reason, QStringLiteral("The Core is not connected to the RF-Kit amplifier."));
        QVERIFY(model.configureRfKitForStation(QStringLiteral("127.0.0.1"), amp.serverPort(),
                                               &reason));
        QTRY_COMPARE_WITH_TIMEOUT(model.rfKitModel()->connectionPhase(), Phase::Connected, 3000);
        QCOMPARE(amp.count(QStringLiteral("POST /error/reset")), 0);
        QVERIFY(model.resetRfKitErrorForStation(&reason));
        QTRY_COMPARE_WITH_TIMEOUT(amp.count(QStringLiteral("POST /error/reset")), 1, 3000);
        // Nothing else changed the amp's state.
        for (const QString& request : amp.requests) {
            QVERIFY2(request.startsWith(QStringLiteral("GET "))
                         || request == QStringLiteral("POST /error/reset"),
                     qPrintable(request));
        }
    }

    // M3 (R-R3-48): the Core switches the amp to TCI once when band follow
    // starts, not again after every link blip (an operator who chose
    // another interface on the amp's front panel keeps it). Turning the
    // station's TCI switch on again is band follow starting again.
    void tciModeIsNotForcedAgainAfterALinkBlip()
    {
        FakeAmp amp;
        Rf2ksConnection connection;
        RfKitModel model;
        model.bindConnection(&connection);
        StationRfKitController controller(&connection, &model);
        const QString put = QStringLiteral(
            R"(PUT /operational-interface {"operational_interface":"TCI"})");
        controller.start(QStringLiteral("127.0.0.1"), amp.serverPort());
        QTRY_COMPARE_WITH_TIMEOUT(model.connectionPhase(), Phase::Connected, 3000);
        connection.injectJsonForTesting(QStringLiteral("/operational-interface"),
                                        amp.operationalInterface);
        controller.setBandFollowWanted(true);
        QTRY_COMPARE_WITH_TIMEOUT(amp.count(put), 1, 3000);

        // The link blips: three failed polls, then the amp answers again.
        // The operator has set it back to UDP on its front panel.
        QSignalSpy reconnected(&connection, &Rf2ksConnection::connected);
        connection.testMarkPollFailure();
        connection.testMarkPollFailure();
        connection.testMarkPollFailure();
        QVERIFY(!connection.isConnected());
        QTRY_VERIFY_WITH_TIMEOUT(reconnected.count() >= 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(model.connectionPhase(), Phase::Connected, 3000);
        connection.injectJsonForTesting(QStringLiteral("/operational-interface"),
                                        amp.operationalInterface);
        QTest::qWait(200);
        QCOMPARE(amp.count(put), 1);

        // The station's TCI switch off and on again: band follow starts
        // again, so the amp is switched once more.
        controller.setBandFollowWanted(false);
        controller.setBandFollowWanted(true);
        QTRY_COMPARE_WITH_TIMEOUT(amp.count(put), 2, 3000);
        controller.cancel();
    }

    // The amp's own interface errors and a lost link are faults.
    void faultsAreEmitted()
    {
        Rf2ksConnection connection;
        QSignalSpy faults(&connection, &Rf2ksConnection::faultObserved);
        connection.injectJsonForTesting(QStringLiteral("/operational-interface"),
            R"({"operational_interface":"TCI","error":"TCI server unreachable"})");
        connection.injectJsonForTesting(QStringLiteral("/operational-interface"),
            R"({"operational_interface":"TCI","error":"TCI server unreachable"})");
        QCOMPARE(faults.count(), 1);
        QCOMPARE(faults.first().at(0).toString(), QStringLiteral("interface"));
        QCOMPARE(faults.first().at(1).toString(), QStringLiteral("TCI server unreachable"));

        connection.setAutoReconnect(false);
        connection.testForceConnectedForTesting();
        connection.testMarkPollFailure();
        connection.testMarkPollFailure();
        connection.testMarkPollFailure();
        QCOMPARE(faults.count(), 2);
        QCOMPARE(faults.last().at(0).toString(), QStringLiteral("link"));
    }

    // RadioModel's Core rules: a radio first, the switch on, a valid
    // address; then both fields saved together and the amp identified.
    void coreConfigureRulesAndSwitch()
    {
        FakeAmp amp;
        RadioModel model;
        QString reason;
        model.enableStationAccessoryIdentity();
        QVERIFY(model.stationRfKitController() != nullptr);
        QVERIFY(!model.configureRfKitForStation(QStringLiteral("127.0.0.1"), amp.serverPort(),
                                                &reason));
        QCOMPARE(reason, QStringLiteral("Connect the Core to a radio before setting up its RF-Kit "
                                        "amplifier."));
        QVERIFY(!model.setRfKitEnabledForStation(true, &reason));

        RadioInfo radio;
        radio.macAddress = QStringLiteral("aa:bb:cc:dd:ee:82");
        model.setLastRadioInfoForTest(radio);
        model.setConnectionStateForTest(ConnectionState::Connected);

        QVERIFY(!model.configureRfKitForStation(QStringLiteral("127.0.0.1"), amp.serverPort(),
                                                &reason));
        QCOMPARE(reason, QStringLiteral("Turn on the RF-Kit amplifier on the Core before "
                                        "connecting it."));

        QSignalSpy switched(&model, &RadioModel::rfKitEnabledChanged);
        QVERIFY(model.setRfKitEnabledForStation(true, &reason));
        QVERIFY(model.rfKitEnabled());
        QCOMPARE(switched.count(), 1);

        QVERIFY(!model.configureRfKitForStation(QStringLiteral("bad host!"), amp.serverPort(),
                                                &reason));
        QCOMPARE(reason, QStringLiteral("Enter the RF-Kit amplifier's IP address or host name, "
                                        "and a port from 1 to 65535."));
        QVERIFY(model.peripheralValue(QStringLiteral("RfKit_ManualIp")).isEmpty());

        QVERIFY(model.configureRfKitForStation(QStringLiteral(" 127.0.0.1 "), amp.serverPort(),
                                               &reason));
        QVERIFY(reason.isEmpty());
        QCOMPARE(model.peripheralValue(QStringLiteral("RfKit_ManualIp")),
                 QStringLiteral("127.0.0.1"));
        QCOMPARE(model.peripheralValue(QStringLiteral("RfKit_ManualPort")),
                 QString::number(amp.serverPort()));
        QTRY_COMPARE_WITH_TIMEOUT(model.rfKitModel()->connectionPhase(), Phase::Connected, 3000);

        QVERIFY(model.disconnectRfKitForStation(&reason));
        QCOMPARE(model.rfKitModel()->connectionPhase(), Phase::Disconnected);
        QVERIFY(!model.rfKitConnection()->isConnected());

        QVERIFY(model.setRfKitEnabledForStation(false, &reason));
        QVERIFY(!model.rfKitEnabled());
        QCOMPARE(model.rfKitModel()->connectionPhase(), Phase::Disabled);
    }

    // Turning the switch on dials the saved address through the identity
    // check; the radio's scope publishes that address first.
    void switchOnDialsTheSavedAddress()
    {
        FakeAmp amp;
        RadioModel model;
        prepare(model);
        model.setPeripheralValue(QStringLiteral("RfKit_ManualIp"), QStringLiteral("127.0.0.1"));
        model.setPeripheralValue(QStringLiteral("RfKit_ManualPort"),
                                 QString::number(amp.serverPort()));
        model.applyPeripheralsForTest();
        QCOMPARE(model.rfKitModel()->connectionPhase(), Phase::Disabled);
        QCOMPARE(model.rfKitModel()->configuredHost(), QStringLiteral("127.0.0.1"));
        QVERIFY(amp.requests.isEmpty());

        QString reason;
        QVERIFY(model.setRfKitEnabledForStation(true, &reason));
        QTRY_COMPARE_WITH_TIMEOUT(model.rfKitModel()->connectionPhase(), Phase::Connected, 3000);
        QCOMPARE(amp.count(QStringLiteral("GET /info")), 1);
    }
};

QTEST_MAIN(StationRfKitControllerTest)
#include "tst_station_rfkit_controller.moc"
