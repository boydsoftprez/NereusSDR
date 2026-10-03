// no-port-check: NereusSDR-original. R-R3-49 (parity Task 9): a window's
// Power Genius OPERATE and STANDBY, the Core's LAN scan for it and its
// saved address, on a Core that owns its accessories.
//
// A loopback TCP peer stands in for the amp and records every line it
// receives; discovery is injected. The announcement and `info` reply are
// the real amp's (StationPgxlController.h names the captures). No real
// accessory is contacted, nothing keys a radio, and a scan only listens.
// J.J. Boyd (KG4VCF), September 2026; AI-assisted via Anthropic Claude Code.
#include <QtTest/QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/LanDiscovery.h"
#include "core/MoxController.h"
#include "core/PgxlConnection.h"
#include "core/SmartSdrApiListener.h"
#include "core/StationPgxlController.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "models/AmplifierModel.h"
#include "models/RadioModel.h"

using namespace NereusSDR;
using Phase = AmplifierModel::ConnectionPhase;

namespace {

constexpr const char* kSerial = "10-200/24-0046";
constexpr const char* kInfoReply = "serial=10-200/24-0046  version=3.8.9 protocol=1.0 mains=240";

quint32 sequenceOf(const QSignalSpy& frames, const QString& commandPattern)
{
    const QRegularExpression rx(QStringLiteral("^C(\\d+)\\|") + commandPattern);
    for (const auto& row : frames) {
        const auto match = rx.match(row.first().toString());
        if (match.hasMatch()) { return match.captured(1).toUInt(); }
    }
    return 0;
}

// What the fake amp received, line by line: the bytes on the wire, with
// the "C<seq>|" prefix removed.
QStringList receivedCommands(QTcpSocket* peer, QByteArray& pending, QStringList& lines)
{
    pending += peer->readAll();
    qsizetype nl = 0;
    while ((nl = pending.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(pending.left(nl));
        pending.remove(0, nl + 1);
        const qsizetype bar = line.indexOf(QLatin1Char('|'));
        lines.append(bar >= 0 ? line.mid(bar + 1) : line);
    }
    return lines;
}

} // namespace

class PgxlStationControlTest : public QObject {
    Q_OBJECT

    static void prepare(RadioModel& model)
    {
        model.enableStationAccessoryIdentity();
        model.setReceiveOnlyStationPolicy(true);
        RadioInfo radio;
        radio.macAddress = QStringLiteral("aa:bb:cc:dd:ee:91");
        model.setLastRadioInfoForTest(radio);
        model.setConnectionStateForTest(ConnectionState::Connected);
        model.smartSdrListener()->setListenEndpointForTesting(QHostAddress::LocalHost, 0);
        model.setPeripheralValue(QStringLiteral("FourO3A_Enabled"), QStringLiteral("True"));
    }

    // Dial through the Core, answer the banner and `info`, and announce the
    // amp so the Core admits it.
    static QTcpSocket* admit(RadioModel& model, QTcpServer& server, const QSignalSpy& frames)
    {
        QString reason;
        if (!model.configurePgxlForStation(QStringLiteral("127.0.0.1"), server.serverPort(),
                                           &reason)) {
            return nullptr;
        }
        if (!QTest::qWaitFor([&] { return server.hasPendingConnections(); }, 2000)) {
            return nullptr;
        }
        QTcpSocket* peer = server.nextPendingConnection();
        peer->write("V3.8.9\n");
        peer->flush();
        if (!QTest::qWaitFor([&] { return sequenceOf(frames, QStringLiteral("info$")) != 0; },
                             2000)) {
            return nullptr;
        }
        peer->write(QStringLiteral("R%1|0|%2\n")
                        .arg(sequenceOf(frames, QStringLiteral("info$")))
                        .arg(QString::fromLatin1(kInfoReply)).toUtf8());
        peer->flush();
        auto* controller = model.findChild<StationPgxlController*>();
        LanDiscovery* discovery = nullptr;
        if (!QTest::qWaitFor([&] {
                discovery = controller ? controller->findChild<LanDiscovery*>() : nullptr;
                return discovery != nullptr;
            }, 2000)) {
            return nullptr;
        }
        discovery->injectDatagramForTesting(
            QStringLiteral("PowerGeniusXL ip=127.0.0.1 v=3.8.9 serial=%1 nickname=PowerGeniusXL")
                .arg(QString::fromLatin1(kSerial)), server.serverPort());
        if (!QTest::qWaitFor([&] { return model.pgxlConnection()->isConnected(); }, 2000)) {
            return nullptr;
        }
        return peer;
    }

private slots:
    void init()
    {
        AppSettings::instance().clear();
        AppSettings::instance().setValue(QStringLiteral("PeripheralsMigrationDone"),
                                         QStringLiteral("True"));
    }
    void cleanup() { AppSettings::instance().clear(); }

    void refusedWithNothingSentWhereTheCoreMayNot()
    {
        QString reason;
        const QString notOwning =
            QStringLiteral("This Core cannot change its amplifier and tuner settings.");
        {   // A Core that does not own its accessories.
            RadioModel plain;
            QVERIFY(!plain.setPgxlOperateForStation(true, &reason));
            QCOMPARE(reason, notOwning);
            bool called = false;
            QVERIFY(!plain.scanPgxlLanForStation([&called](const QString&) { called = true; },
                                                 &reason));
            QCOMPARE(reason, notOwning);
            QVERIFY(!plain.setPgxlAddressForStation(QStringLiteral("192.0.2.9"), 9008, &reason));
            QCOMPARE(reason, notOwning);
            QTest::qWait(20);
            QVERIFY(!called);
        }
        {   // No radio: the address has nowhere to be kept.
            RadioModel noRadio;
            noRadio.enableStationAccessoryIdentity();
            QVERIFY(!noRadio.setPgxlAddressForStation(QStringLiteral("192.0.2.9"), 9008, &reason));
            QCOMPARE(reason, QStringLiteral("Connect the Core to a radio before setting up its "
                                            "Power Genius."));
        }
        RadioModel model;
        prepare(model);
        QSignalSpy frames(model.pgxlConnection(), &PgxlConnection::testFrameWrittenForTesting);
        // Not connected to the amp: nothing to send it to.
        QVERIFY(!model.setPgxlOperateForStation(true, &reason));
        QCOMPARE(reason, QStringLiteral("The Core is not connected to the Power Genius."));
        // A bad address is refused with configurePgxl's words, nothing saved.
        const QString badAddress = QStringLiteral("Enter the Power Genius's IP address or host "
                                                  "name, and a port from 1 to 65535.");
        for (const auto& [host, port] : QList<QPair<QString, int>>{
                 {QString(), 0}, {QStringLiteral("bad host!"), 9008},
                 {QStringLiteral("192.0.2.9"), 0}, {QStringLiteral("192.0.2.9"), 65536}}) {
            QVERIFY(!model.setPgxlAddressForStation(host, port, &reason));
            QCOMPARE(reason, badAddress);
        }
        QVERIFY(model.peripheralValue(QStringLiteral("PGXL_ManualIp")).isEmpty());
        QCOMPARE(frames.count(), 0);
        for (const QString& text : {notOwning, badAddress,
                                    QStringLiteral("The Core is not connected to the Power Genius.")}) {
            QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
        }
    }

    // The exact line, off the air on a receive-only Core; on the air (the
    // MOX latch, TUNE, a hardware PTT, the two-tone test and the hand-back
    // to receive) nothing reaches the amp.
    void operateSendsTheLocalLineOnlyOffTheAir()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        RadioModel model;
        prepare(model);
        model.setPgxlLanScanWindowMsForTest(150);
        QSignalSpy frames(model.pgxlConnection(), &PgxlConnection::testFrameWrittenForTesting);
        QTcpSocket* peer = admit(model, server, frames);
        QVERIFY(peer);
        QByteArray pending;
        QStringList lines;
        const auto operateLines = [&] {
            return receivedCommands(peer, pending, lines).filter(
                QRegularExpression(QStringLiteral("^operate")));
        };

        const QString onAir = QStringLiteral("The radio is on the air. Try again when it stops.");
        QString reason;
        const auto expectRefused = [&] {
            QVERIFY(!model.setPgxlOperateForStation(true, &reason));
            QCOMPARE(reason, onAir);
            QVERIFY(!model.setPgxlOperateForStation(false, &reason));
            QCOMPARE(reason, onAir);
        };
        // Parity mini-round (rulings a and b): Scan LAN only listens and the
        // address is only saved, so both go ahead on the air, as a local
        // window's do; neither switches the amp nor sends it anything.
        const auto expectListenAndSaveTaken = [&](const QString& host) {
            bool scanAnswered = false;
            QVERIFY(model.scanPgxlLanForStation(
                [&scanAnswered](const QString&) { scanAnswered = true; }, &reason));
            QVERIFY(reason.isEmpty());
            QVERIFY(model.setPgxlAddressForStation(host, 9008, &reason));
            QVERIFY(reason.isEmpty());
            QCOMPARE(model.peripheralValue(QStringLiteral("PGXL_ManualIp")), host);
            QTRY_VERIFY(scanAnswered);
            QTRY_VERIFY(model.findChild<LanDiscovery*>(QStringLiteral("pgxlLanScan")) == nullptr);
        };
        model.transmitModel().setMox(true);
        expectRefused();
        if (QTest::currentTestFailed()) { return; }
        expectListenAndSaveTaken(QStringLiteral("192.0.2.7"));
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
        QVERIFY(operateLines().isEmpty());

        // Off the air, on a receive-only Core: the local applet's line.
        QVERIFY(model.receiveOnlyStationPolicy());
        QVERIFY(model.setPgxlOperateForStation(true, &reason));
        QTRY_COMPARE(operateLines(), QStringList{QStringLiteral("operate=1")});
        // The model follows the amp's report, not the request.
        QVERIFY(!model.amplifierModel()->operate());
        peer->write("S0|status state=OPERATE\n");
        peer->flush();
        QTRY_VERIFY(model.amplifierModel()->operate());
        QCOMPARE(model.amplifierModel()->deviceState(), QStringLiteral("OPERATE"));
        QVERIFY(model.setPgxlOperateForStation(false, &reason));
        QTRY_COMPARE(operateLines(),
                     (QStringList{QStringLiteral("operate=1"), QStringLiteral("operate=0")}));
        peer->write("S0|status state=STANDBY\n");
        peer->flush();
        QTRY_VERIFY(!model.amplifierModel()->operate());
        // Nothing was keyed.
        QVERIFY(!mox->isMox());
        QVERIFY(!model.transmitModel().isTune());
        QVERIFY(!model.isTransmitting());

        // The Core's Scan LAN: the Power Genius announcements it heard in
        // the scan's window, not the Tuner Genius's.
        QString devicesJson;
        bool answered = false;
        QVERIFY(model.scanPgxlLanForStation(
            [&](const QString& json) { devicesJson = json; answered = true; }, &reason));
        auto* scan = model.findChild<LanDiscovery*>(QStringLiteral("pgxlLanScan"));
        QVERIFY(scan);
        scan->injectDatagramForTesting(
            QStringLiteral("PowerGeniusXL ip=192.0.2.45 v=3.8.9 serial=5501-7 nickname=Shack_Amp"),
            9008);
        scan->injectDatagramForTesting(
            QStringLiteral("TunerGeniusXL ip=192.0.2.44 v=1.2.17 serial=9911-2 nickname=Tuner"),
            9010);
        QTRY_VERIFY(answered);
        const QJsonArray devices = QJsonDocument::fromJson(devicesJson.toUtf8()).array();
        bool heard = false;
        for (const QJsonValue& value : devices) {
            const QJsonObject device = value.toObject();
            // The operator's real amp on this LAN may answer too.
            QCOMPARE(device.value(QStringLiteral("model")).toString(),
                     QStringLiteral("PowerGeniusXL"));
            if (device.value(QStringLiteral("serial")).toString() == QStringLiteral("5501-7")) {
                heard = true;
                QCOMPARE(device.value(QStringLiteral("address")).toString(),
                         QStringLiteral("192.0.2.45"));
                QCOMPARE(device.value(QStringLiteral("port")).toInt(), 9008);
                QCOMPARE(device.value(QStringLiteral("nickname")).toString(),
                         QStringLiteral("Shack_Amp"));
                QCOMPARE(device.keys().size(), 5);
            }
        }
        QVERIFY(heard);
        QTRY_VERIFY(model.findChild<LanDiscovery*>(QStringLiteral("pgxlLanScan")) == nullptr);

        // A typed address: saved for the Core's radio. While connected the
        // amp's own address stays on `amplifier`; once not, the saved one
        // shows there, and nothing is dialled.
        QVERIFY(model.setPgxlAddressForStation(QStringLiteral(" 192.0.2.9 "), 9011, &reason));
        QCOMPARE(model.peripheralValue(QStringLiteral("PGXL_ManualIp")), QStringLiteral("192.0.2.9"));
        QCOMPARE(model.peripheralValue(QStringLiteral("PGXL_ManualPort")), QStringLiteral("9011"));
        QCOMPARE(model.amplifierModel()->configuredHost(), QStringLiteral("127.0.0.1"));
        QVERIFY(model.disconnectPgxlForStation(&reason));
        QCOMPARE(model.amplifierModel()->connectionPhase(), Phase::Disconnected);
        const quint64 token = model.pgxlConnection()->socketAttemptToken();
        QVERIFY(model.setPgxlAddressForStation(QStringLiteral("192.0.2.10"), 9012, &reason));
        QCOMPARE(model.amplifierModel()->configuredHost(), QStringLiteral("192.0.2.10"));
        QCOMPARE(model.amplifierModel()->configuredPort(), 9012);
        QCOMPARE(model.amplifierModel()->connectionPhase(), Phase::Disconnected);
        QTest::qWait(100);
        QVERIFY(!model.pgxlConnection()->isConnected());
        QCOMPARE(model.pgxlConnection()->socketAttemptToken(), token);
        QVERIFY(!server.hasPendingConnections());

        // Group B fix wave (I1): a blank Host is kept, as a local window's
        // blank Host is, and stops auto-connect: the Core shows the blank
        // and its next start dials nothing.
        QVERIFY(model.setPgxlAddressForStation(QStringLiteral("127.0.0.1"),
                                               server.serverPort(), &reason));
        QVERIFY(model.setPgxlAddressForStation(QStringLiteral("  "), 9013, &reason));
        QVERIFY(reason.isEmpty());
        QVERIFY(model.peripheralValue(QStringLiteral("PGXL_ManualIp")).isEmpty());
        QCOMPARE(model.peripheralValue(QStringLiteral("PGXL_ManualPort")), QStringLiteral("9013"));
        QVERIFY(model.amplifierModel()->configuredHost().isEmpty());
        QCOMPARE(model.amplifierModel()->configuredPort(), 9013);
        model.applyPeripheralsForTest();
        QTest::qWait(150);
        QVERIFY(!server.hasPendingConnections());
        QCOMPARE(model.pgxlConnection()->socketAttemptToken(), token);
        QVERIFY(model.amplifierModel()->configuredHost().isEmpty());
        QCOMPARE(model.amplifierModel()->connectionPhase(), Phase::Disconnected);
    }
};

QTEST_MAIN(PgxlStationControlTest)
#include "tst_pgxl_station_control.moc"
