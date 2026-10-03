// no-port-check: NereusSDR-original. R-R3-49 (parity Task 10): a window's
// RF-Kit RF2K-S OPERATE and STANDBY, antenna, TCI mode and saved address,
// on a Core that owns its accessories, and the Core's RF-Kit connection
// counts on `accessoryData`.
//
// An in-process HTTP server stands in for the amp's REST interface and
// records every request it receives; its /info body is the one
// tst_rf2ks_connection_parse uses (the design doc's swagger and live probe,
// firmware G200C267). It keeps what a PUT sets and reports it on the next
// GET, as the amp does. No real accessory is contacted and nothing keys a
// radio. J.J. Boyd (KG4VCF), September 2026; AI-assisted via Anthropic
// Claude Code.
#include <QtTest/QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/Rf2ksConnection.h"
#include "core/StationAccessoryData.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "models/AccessoryDataModel.h"
#include "models/RadioModel.h"
#include "models/RfKitModel.h"

using namespace NereusSDR;
using Phase = RfKitModel::ConnectionPhase;

namespace {

constexpr const char* kRf2ksInfo =
    R"({"device":"RF2K-S","software_version":{"GUI":200,"controller":267},"custom_device_name":"KG4VCF"})";

// The amp's REST interface, one request per connection (the connection
// asks for Connection: close). A PUT changes what the next GET reports.
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

    QString operateMode{QStringLiteral("STANDBY")};
    int activeAntenna{1};
    QString operationalInterface{QStringLiteral("UDP")};
    QStringList requests;   // "GET /info", "PUT /operate-mode {...}"

    // The requests that change the amp (everything but reads).
    QStringList writes() const
    {
        QStringList out;
        for (const QString& r : requests) {
            if (!r.startsWith(QLatin1String("GET "))) { out.append(r); }
        }
        return out;
    }

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
        QByteArray reply;
        if (verb == QLatin1String("PUT")) {
            const QJsonObject o = QJsonDocument::fromJson(body).object();
            if (path == QLatin1String("/operate-mode")) {
                operateMode = o.value(QStringLiteral("operate_mode")).toString();
            } else if (path == QLatin1String("/antennas/active")) {
                activeAntenna = o.value(QStringLiteral("number")).toInt();
            } else if (path == QLatin1String("/operational-interface")) {
                operationalInterface = o.value(QStringLiteral("operational_interface")).toString();
            }
        } else if (verb == QLatin1String("GET")) {
            if (path == QLatin1String("/info")) {
                reply = kRf2ksInfo;
            } else if (path == QLatin1String("/operate-mode")) {
                reply = QJsonDocument(QJsonObject{{QStringLiteral("operate_mode"), operateMode}})
                            .toJson(QJsonDocument::Compact);
            } else if (path == QLatin1String("/antennas/active")) {
                reply = QJsonDocument(QJsonObject{{QStringLiteral("type"), QStringLiteral("INTERNAL")},
                                                  {QStringLiteral("number"), activeAntenna}})
                            .toJson(QJsonDocument::Compact);
            } else if (path == QLatin1String("/operational-interface")) {
                reply = QJsonDocument(QJsonObject{
                            {QStringLiteral("operational_interface"), operationalInterface},
                            {QStringLiteral("error"), QString()}})
                            .toJson(QJsonDocument::Compact);
            } else {
                reply = "{}";
            }
        }
        QByteArray response = "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n";
        response += "Content-Length: " + QByteArray::number(reply.size()) + "\r\n\r\n" + reply;
        sock->write(response);
        sock->flush();
        sock->disconnectFromHost();
    }

    QHash<QTcpSocket*, QByteArray> m_buffers;
};

// Internal antennas 1 and 2 usable, 3 listed as disabled, 4 not fitted.
constexpr const char* kAntennas =
    R"({"antennas":[{"type":"INTERNAL","number":1,"state":"ACTIVE"},)"
    R"({"type":"INTERNAL","number":2,"state":"AVAILABLE"},)"
    R"({"type":"INTERNAL","number":3,"state":"DISABLED"},)"
    R"({"type":"EXTERNAL","number":1,"state":"AVAILABLE"}]})";

} // namespace

class RfKitStationControlTest : public QObject {
    Q_OBJECT

    static void prepare(RadioModel& model)
    {
        model.enableStationAccessoryIdentity();
        model.setReceiveOnlyStationPolicy(true);
        RadioInfo radio;
        radio.macAddress = QStringLiteral("aa:bb:cc:dd:ee:a1");
        model.setLastRadioInfoForTest(radio);
        model.setConnectionStateForTest(ConnectionState::Connected);
    }

    // The Core's switch on and its amp admitted.
    static bool admit(RadioModel& model, const FakeAmp& amp)
    {
        QString reason;
        if (!model.setRfKitEnabledForStation(true, &reason)
            || !model.configureRfKitForStation(QStringLiteral("127.0.0.1"), amp.serverPort(),
                                               &reason)) {
            return false;
        }
        return QTest::qWaitFor([&] { return model.rfKitConnection()->isConnected(); }, 5000);
    }

private slots:
    void init()
    {
        AppSettings::instance().clear();
        AppSettings::instance().setValue(QStringLiteral("PeripheralsMigrationDone"),
                                         QStringLiteral("True"));
        // Poll quickly so the amp's report of a change arrives in the test.
        AppSettings::instance().setValue(QStringLiteral("RfKit_PollIntervalMs"),
                                         QStringLiteral("250"));
    }
    void cleanup() { AppSettings::instance().clear(); }

    void refusedWithNothingSentWhereTheCoreMayNot()
    {
        QString reason;
        const QString notOwning =
            QStringLiteral("This Core cannot change its amplifier and tuner settings.");
        {   // A Core that does not own its accessories.
            RadioModel plain;
            QVERIFY(!plain.setRfKitOperateForStation(true, &reason));
            QCOMPARE(reason, notOwning);
            QVERIFY(!plain.setRfKitAntennaForStation(1, &reason));
            QCOMPARE(reason, notOwning);
            QVERIFY(!plain.setRfKitTciModeForStation(&reason));
            QCOMPARE(reason, notOwning);
            QVERIFY(!plain.setRfKitAddressForStation(QStringLiteral("192.0.2.9"), 8080, &reason));
            QCOMPARE(reason, notOwning);
        }
        {   // No radio: the address has nowhere to be kept.
            RadioModel noRadio;
            noRadio.enableStationAccessoryIdentity();
            QVERIFY(!noRadio.setRfKitAddressForStation(QStringLiteral("192.0.2.9"), 8080, &reason));
            QCOMPARE(reason, QStringLiteral("Connect the Core to a radio before setting up its "
                                            "RF-Kit amplifier."));
        }
        FakeAmp amp;
        RadioModel model;
        prepare(model);
        // No amp admitted: nothing to send to.
        const QString notConnected =
            QStringLiteral("The Core is not connected to the RF-Kit amplifier.");
        QVERIFY(!model.setRfKitOperateForStation(true, &reason));
        QCOMPARE(reason, notConnected);
        QVERIFY(!model.setRfKitAntennaForStation(2, &reason));
        QCOMPARE(reason, notConnected);
        QVERIFY(!model.setRfKitTciModeForStation(&reason));
        QCOMPARE(reason, notConnected);
        // An antenna the amp cannot have.
        const QString badAntenna = QStringLiteral("Choose RF-Kit amplifier antenna 1, 2, 3 or 4.");
        for (int port : {0, 5, -1}) {
            QVERIFY(!model.setRfKitAntennaForStation(port, &reason));
            QCOMPARE(reason, badAntenna);
        }
        // A bad address is refused with configureRfKit's words, nothing saved.
        const QString badAddress = QStringLiteral("Enter the RF-Kit amplifier's IP address or "
                                                  "host name, and a port from 1 to 65535.");
        for (const auto& [host, port] : QList<QPair<QString, int>>{
                 {QString(), 0}, {QStringLiteral("bad host!"), 8080},
                 {QStringLiteral("192.0.2.9"), 0}, {QStringLiteral("192.0.2.9"), 65536}}) {
            QVERIFY(!model.setRfKitAddressForStation(host, port, &reason));
            QCOMPARE(reason, badAddress);
        }
        QVERIFY(model.peripheralValue(QStringLiteral("RfKit_ManualIp")).isEmpty());
        QTest::qWait(50);
        QVERIFY(amp.requests.isEmpty());
        for (const QString& text : {notOwning, notConnected, badAntenna, badAddress}) {
            QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
        }
    }

    // The exact REST requests, off the air on a receive-only Core; on the
    // air (the MOX latch, TUNE, a hardware PTT, the two-tone test and the
    // hand-back to receive) nothing reaches the amp.
    void commandsReachTheAmpOnlyOffTheAir()
    {
        FakeAmp amp;
        RadioModel model;
        prepare(model);
        QVERIFY(admit(model, amp));
        Rf2ksConnection* const conn = model.rfKitConnection();
        conn->injectJsonForTesting(QStringLiteral("/antennas"), kAntennas);
        RfKitModel* const rfKit = model.rfKitModel();
        QTRY_VERIFY(rfKit->present());

        const QString onAir = QStringLiteral("The radio is on the air. Try again when it stops.");
        QString reason;
        const auto expectRefused = [&] {
            QVERIFY(!model.setRfKitOperateForStation(true, &reason));
            QCOMPARE(reason, onAir);
            QVERIFY(!model.setRfKitAntennaForStation(2, &reason));
            QCOMPARE(reason, onAir);
            QVERIFY(!model.setRfKitTciModeForStation(&reason));
            QCOMPARE(reason, onAir);
        };
        // Parity mini-round (rulings a and b): the address is only saved, so
        // it goes ahead on the air, as a local window's Save does; it
        // switches nothing and sends the amp nothing.
        const auto expectSaveTaken = [&](const QString& host) {
            QVERIFY(model.setRfKitAddressForStation(host, 8080, &reason));
            QVERIFY(reason.isEmpty());
            QCOMPARE(model.peripheralValue(QStringLiteral("RfKit_ManualIp")), host);
        };
        model.transmitModel().setMox(true);
        expectRefused();
        if (QTest::currentTestFailed()) { return; }
        expectSaveTaken(QStringLiteral("192.0.2.7"));
        if (QTest::currentTestFailed()) { return; }
        model.transmitModel().setMox(false);
        model.transmitModel().setTune(true);
        expectRefused();
        if (QTest::currentTestFailed()) { return; }
        model.transmitModel().setTune(false);

        // The Core's own MoxController, its receive-only pre-check lifted to
        // stand in for a Core that can transmit.
        MoxController* const mox = model.moxController();
        QVERIFY(mox);
        mox->setMoxCheck({});
        mox->onMicPttFromRadio(true);
        QVERIFY(mox->isMox());
        expectRefused();
        if (QTest::currentTestFailed()) { return; }
        mox->onMicPttFromRadio(false);
        QTRY_VERIFY(mox->state() == MoxState::Rx);
        {
            TxChannel tx(/*channelId=*/1);
            TwoToneController* const twoTone = model.twoToneController();
            QVERIFY(twoTone);
            twoTone->setTxChannel(&tx);
            twoTone->setSettleDelaysMs(0, 0);
            twoTone->setActive(true);
            QTRY_VERIFY(twoTone->isActive());
            expectRefused();
            if (QTest::currentTestFailed()) { return; }
            twoTone->setActive(false);
            QTRY_VERIFY(!twoTone->isActive());
            QTRY_VERIFY(mox->state() == MoxState::Rx);
            twoTone->setTxChannel(nullptr);
        }
        mox->setTimerIntervals(0, 0, 0, /*keyUpMs=*/300, /*pttOutMs=*/300, 0);
        mox->setMox(true);
        QTRY_VERIFY(mox->state() == MoxState::Tx);
        mox->setMox(false);
        QVERIFY(mox->state() != MoxState::Rx);
        expectRefused();
        if (QTest::currentTestFailed()) { return; }
        QTRY_VERIFY(mox->state() == MoxState::Rx);
        QTest::qWait(100);
        QVERIFY2(amp.writes().isEmpty(), qPrintable(amp.writes().join(QLatin1Char('\n'))));

        // Off the air, on a receive-only Core: the local applet's requests.
        QVERIFY(model.receiveOnlyStationPolicy());
        QVERIFY(!rfKit->operate());
        QVERIFY(model.setRfKitOperateForStation(true, &reason));
        QTRY_COMPARE(amp.writes(),
                     QStringList{QStringLiteral(R"(PUT /operate-mode {"operate_mode":"OPERATE"})")});
        // The model follows the amp's report.
        QTRY_VERIFY(rfKit->operate());
        QVERIFY(model.setRfKitOperateForStation(false, &reason));
        QTRY_COMPARE(amp.writes().size(), 2);
        QCOMPARE(amp.writes().at(1),
                 QStringLiteral(R"(PUT /operate-mode {"operate_mode":"STANDBY"})"));
        QTRY_VERIFY(!rfKit->operate());

        // Antennas: one the amp lists as usable switches; one it lists as
        // disabled, or does not list, is refused with nothing sent.
        const QString unavailable =
            QStringLiteral("This antenna is not available on the RF-Kit amplifier.");
        QVERIFY(OperatorWording::isPlain(unavailable));
        QVERIFY(!model.setRfKitAntennaForStation(3, &reason));
        QCOMPARE(reason, unavailable);
        QVERIFY(!model.setRfKitAntennaForStation(4, &reason));
        QCOMPARE(reason, unavailable);
        QVERIFY(model.setRfKitAntennaForStation(2, &reason));
        QTRY_COMPARE(amp.writes().size(), 3);
        QCOMPARE(amp.writes().at(2),
                 QStringLiteral(R"(PUT /antennas/active {"number":2,"type":"INTERNAL"})"));
        QTRY_COMPARE(rfKit->activeAntennaNumber(), 2);
        QVERIFY(!rfKit->activeAntennaExternal());

        // TCI mode, as the local page's "Set amp to TCI mode".
        QTRY_COMPARE(rfKit->operationalInterface(), QStringLiteral("UDP"));
        QVERIFY(model.setRfKitTciModeForStation(&reason));
        QTRY_COMPARE(amp.writes().size(), 4);
        QCOMPARE(amp.writes().at(3), QStringLiteral(
            R"(PUT /operational-interface {"operational_interface":"TCI"})"));
        QTRY_COMPARE(rfKit->operationalInterface(), QStringLiteral("TCI"));

        // Nothing was keyed.
        QVERIFY(!mox->isMox());
        QVERIFY(!model.transmitModel().isTune());
        QVERIFY(!model.isTransmitting());

        // A typed address: saved for the Core's radio. While connected the
        // amp's own address stays on `rfkit`; once not, the saved one shows
        // there, and nothing is dialled.
        QVERIFY(model.setRfKitAddressForStation(QStringLiteral(" 192.0.2.9 "), 8081, &reason));
        QCOMPARE(model.peripheralValue(QStringLiteral("RfKit_ManualIp")), QStringLiteral("192.0.2.9"));
        QCOMPARE(model.peripheralValue(QStringLiteral("RfKit_ManualPort")), QStringLiteral("8081"));
        QCOMPARE(rfKit->configuredHost(), QStringLiteral("127.0.0.1"));
        QVERIFY(model.disconnectRfKitForStation(&reason));
        QCOMPARE(rfKit->connectionPhase(), Phase::Disconnected);
        QTest::qWait(100);
        const int seen = amp.requests.size();
        QVERIFY(model.setRfKitAddressForStation(QStringLiteral("192.0.2.10"), 8082, &reason));
        QCOMPARE(rfKit->configuredHost(), QStringLiteral("192.0.2.10"));
        QCOMPARE(rfKit->configuredPort(), 8082);
        QCOMPARE(rfKit->connectionPhase(), Phase::Disconnected);
        QTest::qWait(300);
        QVERIFY(!conn->isConnected());
        QVERIFY(conn->peerAddress() != QStringLiteral("192.0.2.10"));
        QCOMPARE(amp.requests.size(), seen);
        // The switch off: still saved, shown as off.
        QVERIFY(model.setRfKitEnabledForStation(false, &reason));
        QVERIFY(model.setRfKitAddressForStation(QStringLiteral("192.0.2.11"), 8083, &reason));
        QCOMPARE(model.peripheralValue(QStringLiteral("RfKit_ManualIp")), QStringLiteral("192.0.2.11"));
        QCOMPARE(rfKit->configuredHost(), QStringLiteral("192.0.2.11"));
        QCOMPARE(rfKit->connectionPhase(), Phase::Disabled);

        // Group B fix wave (I1): a blank Host is kept, as a local window's
        // blank Host is, and stops auto-connect: with the switch on again
        // the Core shows the blank and dials nothing.
        QVERIFY(model.setRfKitAddressForStation(QStringLiteral("  "), 8084, &reason));
        QVERIFY(reason.isEmpty());
        QVERIFY(model.peripheralValue(QStringLiteral("RfKit_ManualIp")).isEmpty());
        QCOMPARE(model.peripheralValue(QStringLiteral("RfKit_ManualPort")), QStringLiteral("8084"));
        QVERIFY(rfKit->configuredHost().isEmpty());
        const int seenBlank = amp.requests.size();
        QVERIFY(model.setRfKitEnabledForStation(true, &reason));
        model.applyPeripheralsForTest();
        QTest::qWait(300);
        QVERIFY(!conn->isConnected());
        QCOMPARE(amp.requests.size(), seenBlank);
        QVERIFY(rfKit->configuredHost().isEmpty());
        QCOMPARE(rfKit->connectionPhase(), Phase::Disconnected);
    }

    // B1.12: the Core's RF-Kit connection counts reach `accessoryData`, as
    // the local page reads them from its own connection.
    void countersReachAccessoryData()
    {
        FakeAmp amp;
        RadioModel model;
        prepare(model);
        AccessoryDataModel* const data = model.accessoryDataModel();
        QVERIFY(data);
        QCOMPARE(data->rfkitPollsOk(), 0);
        QCOMPARE(data->rfkitConnectedSinceMs(), qint64(0));
        QSignalSpy changed(data, &AccessoryDataModel::rfkitDiagnosticsChanged);
        QVERIFY(admit(model, amp));
        Rf2ksConnection* const conn = model.rfKitConnection();
        // Published on their own, about once a second.
        QTRY_VERIFY_WITH_TIMEOUT(data->rfkitPollsOk() > 0, 3000);
        QVERIFY(changed.count() >= 1);
        QVERIFY(data->rfkitConnectedSinceMs() > 0);
        QCOMPARE(data->rfkitConnectedSinceMs(), conn->connectedSinceMs());
        QVERIFY(data->rfkitLastPollMs() > 0);
        // publishAll reads the connection as it is now.
        model.stationAccessoryData()->publishAll();
        QCOMPARE(data->rfkitPollsOk(), conn->pollsSucceeded());
        QCOMPARE(data->rfkitPollsFailed(), conn->pollsFailed());
        QCOMPARE(data->rfkitReconnectCount(), conn->reconnectAttempts());
        QCOMPARE(data->rfkitLastPollMs(), conn->lastPollMs());
        // Group B fix wave (M7): the response time the local page shows
        // (the average over the last ten polls).
        for (int i = 0; i < 5; ++i) { conn->testMarkPollSuccess(120); }
        QVERIFY(conn->rttAvgLast10Ms() > 0);
        model.stationAccessoryData()->publishAll();
        QCOMPARE(data->rfkitRttAvgMs(), conn->rttAvgLast10Ms());
        // A remote window's copy takes each one.
        AccessoryDataModel window;
        QVERIFY(window.applyStationValue("rfkitPollsOk", QVariant(qint64(41))));
        QVERIFY(window.applyStationValue("rfkitPollsFailed", QVariant(qint64(3))));
        QVERIFY(window.applyStationValue("rfkitReconnectCount", QVariant(qint64(2))));
        QVERIFY(window.applyStationValue("rfkitConnectedSinceMs", QVariant(qint64(1790000000000))));
        QVERIFY(window.applyStationValue("rfkitLastPollMs", QVariant(qint64(1790000001000))));
        QVERIFY(window.applyStationValue("rfkitRttAvgMs", QVariant(qint64(37))));
        QVERIFY(!window.applyStationValue("rfkitNoSuchCounter", QVariant(qint64(1))));
        QCOMPARE(window.rfkitRttAvgMs(), 37);
        QCOMPARE(window.rfkitPollsOk(), 41);
        QCOMPARE(window.rfkitPollsFailed(), 3);
        QCOMPARE(window.rfkitReconnectCount(), 2);
        QCOMPARE(window.rfkitConnectedSinceMs(), qint64(1790000000000));
        QCOMPARE(window.rfkitLastPollMs(), qint64(1790000001000));
        QString reason;
        QVERIFY(model.disconnectRfKitForStation(&reason));
        model.stationAccessoryData()->publishAll();
        QCOMPARE(data->rfkitConnectedSinceMs(), qint64(0));
    }
};

QTEST_MAIN(RfKitStationControlTest)
#include "tst_rfkit_station_control.moc"
