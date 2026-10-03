// no-port-check: NereusSDR-original. R-R3-22/25 Core accessory admission.
// Loopback TCP peers and captured discovery/info grammar; no RF or hardware.
// 2026-09-24: R-R3-47 / R-R3-22: a window's requests for the tuner's own
// settings reach the (fake) tuner as the local page's own commands. J.J.
// Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-49 / R-R3-47: a window's antenna, operate and bypass
// requests reach the (fake) tuner as the local applet's lines, and are
// refused while the radio is on the air, with no tuner, with no antenna
// switch or a port outside 1 to 3. J.J. Boyd (KG4VCF), AI-assisted via
// Anthropic Claude Code.
// 2026-09-24: R-R3-49 fix wave: also refused while the Core's MoxController
// is keyed by a hardware PTT or the two-tone test, and through its TX to RX
// handover. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-49 fix wave: OPERATE on sends bypass=0 then operate=1
// from one command (remoteTgxlControlVersion 3). J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// 2026-09-25: R-R3-49 (parity Task 8): a window's relay nudge reaches the
// (fake) tuner as the local applet's line; the Core's LAN scan answers with
// the Tuner Genius announcements it heard; a typed address is saved without
// dialling. Each refused while the radio is on the air, and the nudge with
// no tuner. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-10-01: RD-I11: the tuner's name is one word (Shack_Tuner); a name
// with a space is refused on the Core and nothing reaches the tuner. J.J.
// Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#include <QtTest/QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include "core/AppSettings.h"
#include "core/LanDiscovery.h"
#include "core/MoxController.h"
#include "core/StationTgxlController.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "models/AccessorySettingsModel.h"
#include "models/RadioModel.h"
#include "models/TunerModel.h"

using namespace NereusSDR;

class TgxlStationIdentityTest : public QObject {
    Q_OBJECT
    static void prepare(RadioModel& model)
    {
        model.enableStationAccessoryIdentity();
        model.setReceiveOnlyStationPolicy(true);
        RadioInfo radio;
        radio.macAddress = QStringLiteral("aa:bb:cc:dd:ee:44");
        model.setLastRadioInfoForTest(radio);
        model.setConnectionStateForTest(ConnectionState::Connected);
        model.setPeripheralValue(QStringLiteral("FourO3A_Enabled"), QStringLiteral("True"));
    }
    // The Core's identity listener, a child of its StationTgxlController.
    // Looked up through the controller, never the model, since a window's
    // Scan LAN adds a LanDiscovery (tgxlLanScan) to the model itself
    // (group B fix wave, M3).
    static LanDiscovery* identityDiscovery(RadioModel& model)
    {
        auto* controller = model.findChild<StationTgxlController*>();
        return controller ? controller->findChild<LanDiscovery*>() : nullptr;
    }
    static void announce(LanDiscovery* discovery, quint16 port,
                         const QString& product, const QString& serial)
    {
        announceAt(discovery, QStringLiteral("127.0.0.1"), port, product, serial);
    }
    static void announceAt(LanDiscovery* discovery, const QString& ip,
                           quint16 port, const QString& product,
                           const QString& serial)
    {
        discovery->injectDatagramForTesting(
            QStringLiteral("%1 ip=%2 v=1.2.17 serial=%3 nickname=Tuner_Genius_XL")
                .arg(product, ip, serial), port);
    }
    static quint32 infoSequence(const QSignalSpy& frames)
    {
        const QRegularExpression rx(QStringLiteral("^C(\\d+)\\|info$"));
        for (const auto& args : frames) {
            const auto match = rx.match(args.first().toString());
            if (match.hasMatch()) { return match.captured(1).toUInt(); }
        }
        return 0;
    }
    static void sendInfo(QTcpSocket* peer, quint32 sequence, const QString& serial)
    {
        peer->write(QStringLiteral("R%1|0|info serial=%2 version=1.2.17 nickname=Tuner_Genius_XL 3way=1\n")
                        .arg(sequence).arg(serial).toUtf8());
        peer->flush();
    }
    static void seedScope(const QString& mac, bool enabled,
                          const QString& host, quint16 port = 9010)
    {
        auto& settings = AppSettings::instance();
        settings.setValue(QStringLiteral("PeripheralsMigrationDone"),
                          QStringLiteral("True"));
        settings.setHardwareValue(mac,
            QStringLiteral("peripherals/FourO3A_Enabled"),
            enabled ? QStringLiteral("True") : QStringLiteral("False"));
        settings.setHardwareValue(mac,
            QStringLiteral("peripherals/TGXL_ManualIp"), host);
        settings.setHardwareValue(mac,
            QStringLiteral("peripherals/TGXL_ManualPort"), QString::number(port));
    }
private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void validatesWholeEndpointBeforeChangingAnything()
    {
        RadioModel model;
        prepare(model);
        model.setPeripheralValue(QStringLiteral("TGXL_ManualIp"), QStringLiteral("saved.example"));
        model.setPeripheralValue(QStringLiteral("TGXL_ManualPort"), QStringLiteral("9010"));
        const auto token = model.tgxlConnection()->socketAttemptToken();
        QString reason;
        for (const auto& host : {QString(), QStringLiteral("bad host"),
                                QStringLiteral("http://127.0.0.1"),
                                QStringLiteral("127.0.0.1\nstatus"), QStringLiteral("-invalid.example")}) {
            QVERIFY(!model.configureTgxlForStation(host, 9010, &reason));
            QVERIFY(!reason.isEmpty());
            QCOMPARE(model.peripheralValue(QStringLiteral("TGXL_ManualIp")), QStringLiteral("saved.example"));
            QCOMPARE(model.peripheralValue(QStringLiteral("TGXL_ManualPort")), QStringLiteral("9010"));
            QCOMPARE(model.tgxlConnection()->socketAttemptToken(), token);
        }
        QVERIFY(!model.configureTgxlForStation(QStringLiteral("127.0.0.1"), 0, &reason));
        model.setPeripheralValue(QStringLiteral("FourO3A_Enabled"), QStringLiteral("False"));
        QVERIFY(!model.configureTgxlForStation(QStringLiteral("127.0.0.1"), 9010, &reason));
        model.setConnectionStateForTest(ConnectionState::Disconnected);
        QVERIFY(!model.configureTgxlForStation(QStringLiteral("127.0.0.1"), 9010, &reason));
        QVERIFY(model.disconnectTgxlForStation(&reason));
    }

    void coldDisabledScopePublishesSavedEndpoint()
    {
        const QString mac = QStringLiteral("aa:bb:cc:dd:ee:51");
        seedScope(mac, false, QStringLiteral("saved-tuner.example"), 9010);
        RadioModel model;
        model.enableStationAccessoryIdentity();
        model.setReceiveOnlyStationPolicy(true);
        RadioInfo radio;
        radio.macAddress = mac;
        model.setLastRadioInfoForTest(radio);
        model.setConnectionStateForTest(ConnectionState::Connected);
        model.applyPeripheralsForTest();

        QCOMPARE(model.tunerModel()->configuredHost(),
                 QStringLiteral("saved-tuner.example"));
        QCOMPARE(model.tunerModel()->configuredPort(), 9010);
        QCOMPARE(model.tunerModel()->connectionPhase(),
                 TunerModel::ConnectionPhase::Disabled);
        QVERIFY(!model.tgxlConnection()->isConnected());
        QCOMPARE(model.tgxlConnection()->socketAttemptToken(), quint64(0));
    }

    void changingMacScopeClearsRetiredEndpointBeforeDisabledScope()
    {
        QTcpServer oldServer;
        QVERIFY(oldServer.listen(QHostAddress::LocalHost, 0));
        const QString oldMac = QStringLiteral("aa:bb:cc:dd:ee:52");
        const QString newMac = QStringLiteral("aa:bb:cc:dd:ee:53");
        seedScope(oldMac, true, QStringLiteral("127.0.0.1"),
                  oldServer.serverPort());
        seedScope(newMac, false, QString{}, 9010);

        RadioModel model;
        model.enableStationAccessoryIdentity();
        model.setReceiveOnlyStationPolicy(true);
        RadioInfo oldRadio;
        oldRadio.macAddress = oldMac;
        model.setLastRadioInfoForTest(oldRadio);
        model.setConnectionStateForTest(ConnectionState::Connected);
        model.applyPeripheralsForTest();
        QTRY_VERIFY_WITH_TIMEOUT(oldServer.hasPendingConnections(), 1500);
        auto* oldPeer = oldServer.nextPendingConnection();
        QVERIFY(oldPeer);
        const quint64 retiredAttempt = model.tgxlConnection()->socketAttemptToken();

        model.setConnectionStateForTest(ConnectionState::Disconnected);
        model.teardownPeripheralsForTest();
        QVERIFY(!model.tgxlConnection()->testReconnectPending());
        RadioInfo newRadio;
        newRadio.macAddress = newMac;
        model.setLastRadioInfoForTest(newRadio);
        model.setConnectionStateForTest(ConnectionState::Connected);
        model.applyPeripheralsForTest();

        QCOMPARE(model.tunerModel()->configuredHost(), QString{});
        QCOMPARE(model.tunerModel()->configuredPort(), 9010);
        QCOMPARE(model.tunerModel()->connectionPhase(),
                 TunerModel::ConnectionPhase::Disabled);
        model.tgxlConnection()->testInjectFailureForSocketAttempt(retiredAttempt);
        QTest::qWait(50);
        QVERIFY(!model.tgxlConnection()->testReconnectPending());
        QVERIFY(!oldServer.hasPendingConnections());
    }

    void admittedOnlyByMatchingDiscoveryAndNativeInfo_data()
    {
        QTest::addColumn<QString>("product");
        QTest::newRow("observed-alias") << QStringLiteral("TunerGenius");
        QTest::newRow("canonical-alias") << QStringLiteral("TunerGeniusXL");
    }
    void admittedOnlyByMatchingDiscoveryAndNativeInfo()
    {
        QFETCH(QString, product);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        RadioModel model;
        prepare(model);
        QSignalSpy frames(model.tgxlConnection(), &TgxlConnection::testFrameWrittenForTesting);
        QString reason;
        QVERIFY2(model.configureTgxlForStation(QStringLiteral("127.0.0.1"), server.serverPort(), &reason), qPrintable(reason));
        QCOMPARE(model.peripheralValue(QStringLiteral("TGXL_ManualPort")), QString::number(server.serverPort()));
        QVERIFY(!model.tunerModel()->hasDirectConnection());
        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 1500);
        auto* peer = server.nextPendingConnection();
        peer->write("V1.2.17\n"); peer->flush();
        QTRY_VERIFY_WITH_TIMEOUT(infoSequence(frames) != 0, 1500);
        auto* discovery = identityDiscovery(model);
        QVERIFY(discovery);
        sendInfo(peer, infoSequence(frames), QStringLiteral("241288-1"));
        QTRY_COMPARE(model.tgxlConnection()->identityInfo().serial, QStringLiteral("241288-1"));
        QVERIFY(!model.tunerModel()->hasDirectConnection());
        QVERIFY(!model.tunerModel()->isPresent());
        announce(discovery, server.serverPort(), product, QStringLiteral("241288-1"));
        QTRY_VERIFY(model.tunerModel()->hasDirectConnection());
        QVERIFY(model.tunerModel()->isPresent());
        QVERIFY(model.tunerModel()->hasAntennaSwitch());
        QCOMPARE(model.tunerModel()->connectionPhase(), TunerModel::ConnectionPhase::Connected);
        QCOMPARE(model.tunerModel()->deviceModel(), product);
        QCOMPARE(model.tunerModel()->deviceSerial(), QStringLiteral("241288-1"));
        QCOMPARE(model.tunerModel()->tgxlIp(), QStringLiteral("127.0.0.1"));
        QVERIFY(model.disconnectTgxlForStation(&reason));
        QVERIFY(!model.tunerModel()->isPresent());
        QVERIFY(!model.tunerModel()->hasDirectConnection());
        QCOMPARE(model.tunerModel()->antennaA(), 0);
    }

    void rejectsWrongIdentity_data()
    {
        QTest::addColumn<QString>("product");
        QTest::addColumn<QString>("serial");
        QTest::newRow("amplifier") << QStringLiteral("PowerGeniusXL") << QStringLiteral("241288-1");
        QTest::newRow("different-serial") << QStringLiteral("TunerGenius") << QStringLiteral("different-device");
    }
    void rejectsWrongIdentity()
    {
        QFETCH(QString, product); QFETCH(QString, serial);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        RadioModel model; prepare(model);
        QSignalSpy frames(model.tgxlConnection(), &TgxlConnection::testFrameWrittenForTesting);
        QString reason;
        QVERIFY(model.configureTgxlForStation(QStringLiteral("127.0.0.1"), server.serverPort(), &reason));
        QTRY_VERIFY(server.hasPendingConnections());
        auto* peer = server.nextPendingConnection();
        peer->write("V1.2.17\n"); peer->flush();
        QTRY_VERIFY(infoSequence(frames) != 0);
        auto* discovery = identityDiscovery(model); QVERIFY(discovery);
        sendInfo(peer, infoSequence(frames), QStringLiteral("241288-1"));
        QTRY_VERIFY(!model.tgxlConnection()->identityInfo().serial.isEmpty());
        announce(discovery, server.serverPort(), product, serial);
        QTRY_VERIFY(!model.tunerModel()->connectionError().isEmpty());
        QVERIFY(!model.tgxlConnection()->isConnected());
        QVERIFY(!model.tunerModel()->isPresent());
        for (const auto& args : frames) { QVERIFY(args.first().toString().endsWith(QStringLiteral("|info"))); }
        QVERIFY(model.disconnectTgxlForStation(&reason));
    }

    void pendingLifecycleCancellation_data()
    {
        QTest::addColumn<int>("action");
        QTest::newRow("disconnect") << 0;
        QTest::newRow("master-disable") << 1;
        QTest::newRow("radio-teardown") << 2;
    }

    void discoveryMustMatchActualPeerPortAndHasBoundedFailure()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        RadioModel model; prepare(model);
        QSignalSpy frames(model.tgxlConnection(), &TgxlConnection::testFrameWrittenForTesting);
        QString reason;
        QVERIFY(model.configureTgxlForStation(QStringLiteral("localhost"), server.serverPort(), &reason));
        QTRY_VERIFY(server.hasPendingConnections());
        auto* peer = server.nextPendingConnection();
        peer->write("V1.2.17\n"); peer->flush();
        QTRY_VERIFY(infoSequence(frames) != 0);
        auto* discovery = identityDiscovery(model); QVERIFY(discovery);
        sendInfo(peer, infoSequence(frames), QStringLiteral("241288-1"));
        QTRY_VERIFY(!model.tgxlConnection()->identityInfo().serial.isEmpty());
        announce(discovery, server.serverPort() == 9010 ? 9008 : 9010,
                 QStringLiteral("TunerGenius"), QStringLiteral("241288-1"));
        QVERIFY(!model.tunerModel()->hasDirectConnection());
        QVERIFY(QMetaObject::invokeMethod(discovery, "onTimeout", Qt::DirectConnection));
        QCOMPARE(model.tunerModel()->connectionError(),
                 QStringLiteral("The Core did not find a Tuner Genius at this address on its "
                                "network. Check the tuner's address and port."));
        QVERIFY(!model.tunerModel()->isPresent());
        QVERIFY(model.disconnectTgxlForStation(&reason));
    }

    void sameSerialWrongEndpointCannotSuppressMatchingDiscovery_data()
    {
        QTest::addColumn<QString>("wrongIp");
        QTest::addColumn<bool>("wrongPort");
        QTest::newRow("wrong IP first") << QStringLiteral("127.0.0.2") << false;
        QTest::newRow("wrong port first") << QStringLiteral("127.0.0.1") << true;
    }

    void sameSerialWrongEndpointCannotSuppressMatchingDiscovery()
    {
        QFETCH(QString, wrongIp);
        QFETCH(bool, wrongPort);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        RadioModel model; prepare(model);
        QSignalSpy frames(model.tgxlConnection(),
                          &TgxlConnection::testFrameWrittenForTesting);
        QString reason;
        QVERIFY(model.configureTgxlForStation(QStringLiteral("127.0.0.1"),
                                              server.serverPort(), &reason));
        QTRY_VERIFY(server.hasPendingConnections());
        auto* peer = server.nextPendingConnection();
        peer->write("V1.2.17\n"); peer->flush();
        QTRY_VERIFY(infoSequence(frames) != 0);
        auto* discovery = identityDiscovery(model);
        QVERIFY(discovery);
        sendInfo(peer, infoSequence(frames), QStringLiteral("241288-1"));
        QTRY_COMPARE(model.tgxlConnection()->identityInfo().serial,
                     QStringLiteral("241288-1"));

        const quint16 firstPort = wrongPort
            ? quint16(server.serverPort() == 9010 ? 9008 : 9010)
            : server.serverPort();
        announceAt(discovery, wrongIp, firstPort,
                   QStringLiteral("TunerGenius"), QStringLiteral("241288-1"));
        QVERIFY(!model.tunerModel()->hasDirectConnection());

        // Identity-sensitive discovery must not let the same-serial record at
        // another endpoint consume the valid current-peer announcement.
        announce(discovery, server.serverPort(), QStringLiteral("TunerGenius"),
                 QStringLiteral("241288-1"));
        QTRY_VERIFY_WITH_TIMEOUT(model.tunerModel()->hasDirectConnection(), 1000);
        QCOMPARE(model.tunerModel()->connectionPhase(),
                 TunerModel::ConnectionPhase::Connected);
        QVERIFY(model.disconnectTgxlForStation(&reason));
    }

    void provisionalPeerDropRemainsRetryingUntilCancelled()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        RadioModel model; prepare(model);
        model.tgxlConnection()->testSetReconnectBackoffUnitMs(500);
        QString reason;
        QVERIFY(model.configureTgxlForStation(QStringLiteral("127.0.0.1"),
                                              server.serverPort(), &reason));
        QTRY_VERIFY(server.hasPendingConnections());
        auto* peer = server.nextPendingConnection();
        peer->write("V1.2.17\n"); peer->flush();
        QTRY_COMPARE(model.tunerModel()->connectionPhase(),
                     TunerModel::ConnectionPhase::Identifying);

        // Closing before native info produces Qt's paired error/disconnected
        // callbacks. The owned retry is authoritative even if disconnected is
        // delivered after connectionFailed/reconnectAttempt.
        peer->disconnectFromHost();
        peer->deleteLater();
        QTRY_VERIFY_WITH_TIMEOUT(model.tgxlConnection()->testReconnectPending(),
                                 1500);
        QCOMPARE(model.tunerModel()->connectionPhase(),
                 TunerModel::ConnectionPhase::Retrying);

        QVERIFY(model.disconnectTgxlForStation(&reason));
        QVERIFY(!model.tgxlConnection()->testReconnectPending());
        QCOMPARE(model.tunerModel()->connectionPhase(),
                 TunerModel::ConnectionPhase::Disconnected);
        QTest::qWait(550);
        QVERIFY(!server.hasPendingConnections());
    }

    // I2 (R-R3-47): a Tuner Genius that is switched off records one fault
    // for the outage, not one per retry. A tuner that was admitted and then
    // drops records one (the drop), and the retries after it none. The next
    // admission starts a new outage.
    void oneFaultPerOutage()
    {
        QTcpServer reservation;
        QVERIFY(reservation.listen(QHostAddress::LocalHost, 0));
        const quint16 port = reservation.serverPort();
        reservation.close();   // nothing answers: connection refused

        RadioModel model; prepare(model);
        model.tgxlConnection()->testSetReconnectBackoffUnitMs(20);
        QSignalSpy retries(model.tgxlConnection(), &TgxlConnection::reconnectAttempt);
        QString reason;
        QVERIFY(model.configureTgxlForStation(QStringLiteral("127.0.0.1"), port, &reason));
        QTRY_VERIFY_WITH_TIMEOUT(retries.count() >= 4, 3000);
        QCOMPARE(model.tgxlFaultLog()->events().size(), 1);
        QCOMPARE(model.tgxlFaultLog()->events().first().state, QStringLiteral("connection"));

        // The tuner comes back and is admitted; then it drops again.
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, port));
        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 3000);
        QSignalSpy frames(model.tgxlConnection(), &TgxlConnection::testFrameWrittenForTesting);
        auto* peer = server.nextPendingConnection();
        peer->write("V1.2.17\n"); peer->flush();
        QTRY_VERIFY_WITH_TIMEOUT(infoSequence(frames) != 0, 3000);
        auto* discovery = identityDiscovery(model);
        QVERIFY(discovery);
        sendInfo(peer, infoSequence(frames), QStringLiteral("241288-1"));
        QTRY_VERIFY(!model.tgxlConnection()->identityInfo().serial.isEmpty());
        announce(discovery, port, QStringLiteral("TunerGeniusXL"), QStringLiteral("241288-1"));
        QTRY_COMPARE(model.tunerModel()->connectionPhase(),
                     TunerModel::ConnectionPhase::Connected);
        QCOMPARE(model.tgxlFaultLog()->events().size(), 1);

        server.close();
        retries.clear();
        peer->disconnectFromHost();
        QTRY_VERIFY_WITH_TIMEOUT(retries.count() >= 4, 3000);
        QCOMPARE(model.tgxlFaultLog()->events().size(), 2);
        QCOMPARE(model.tgxlFaultLog()->events().first().state, QStringLiteral("link"));   // newest first
        QVERIFY(model.disconnectTgxlForStation(&reason));
    }

    void endpointReplacementRejectsStaleDiscoveryAndInfo()
    {
        QTcpServer oldServer, newServer;
        QVERIFY(oldServer.listen(QHostAddress::LocalHost, 0));
        QVERIFY(newServer.listen(QHostAddress::LocalHost, 0));
        RadioModel model; prepare(model);
        QSignalSpy frames(model.tgxlConnection(), &TgxlConnection::testFrameWrittenForTesting);
        QString reason;
        QVERIFY(model.configureTgxlForStation(QStringLiteral("127.0.0.1"), oldServer.serverPort(), &reason));
        QTRY_VERIFY(oldServer.hasPendingConnections());
        auto* oldPeer = oldServer.nextPendingConnection();
        oldPeer->write("V1.2.17\n"); oldPeer->flush();
        QTRY_VERIFY(infoSequence(frames) != 0);
        QPointer<LanDiscovery> oldDiscovery = identityDiscovery(model); QVERIFY(oldDiscovery);
        const auto oldToken = model.tgxlConnection()->socketAttemptToken();
        const auto oldSequence = infoSequence(frames);
        QVERIFY(model.configureTgxlForStation(QStringLiteral("127.0.0.1"), newServer.serverPort(), &reason));
        // A callback already in flight from the old scan cannot admit B.
        if (oldDiscovery) {
            announce(oldDiscovery, newServer.serverPort(), QStringLiteral("TunerGenius"), QStringLiteral("241288-1"));
        }
        model.tgxlConnection()->testInjectLineForSocketAttempt(
            QStringLiteral("R%1|0|info serial=241288-1 version=1.2.17 nickname=Tuner_Genius_XL")
                .arg(oldSequence), oldToken);
        QVERIFY(!model.tunerModel()->isPresent());
        frames.clear();
        QTRY_VERIFY(newServer.hasPendingConnections());
        auto* peer = newServer.nextPendingConnection();
        peer->write("V1.2.17\n"); peer->flush();
        QTRY_VERIFY(infoSequence(frames) != 0);
        QTRY_VERIFY(oldDiscovery.isNull());
        auto* discovery = identityDiscovery(model); QVERIFY(discovery);
        sendInfo(peer, infoSequence(frames), QStringLiteral("241288-1"));
        QTRY_VERIFY(!model.tgxlConnection()->identityInfo().serial.isEmpty());
        QVERIFY(!model.tunerModel()->hasDirectConnection());
        announce(discovery, newServer.serverPort(), QStringLiteral("TunerGenius"), QStringLiteral("241288-1"));
        QTRY_VERIFY(model.tunerModel()->hasDirectConnection());
        QCOMPARE(model.tunerModel()->configuredPort(), int(newServer.serverPort()));
        QVERIFY(model.disconnectTgxlForStation(&reason));
    }
    // R-R3-47 / R-R3-22: a window's requests for the tuner's own settings
    // (name, network, Save & Reboot, Revert). The Core sends each as the
    // local Advanced page's own command (the fake tuner records the
    // bytes), matches the answer and publishes it in plain words; the
    // tuner's name read back is saved as the local page saves it.
    void deviceSettingsReachTheTunerAsTheLocalPageSendsThem()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        RadioModel model;
        prepare(model);
        const AccessorySettingsModel* settings = model.accessorySettingsModel();
        QSignalSpy frames(model.tgxlConnection(), &TgxlConnection::testFrameWrittenForTesting);
        QString reason;
        QVERIFY(!model.setTgxlNameForStation(QStringLiteral("Tuner"), &reason));
        QCOMPARE(reason, QStringLiteral("The Core is not connected to the Tuner Genius."));
        QVERIFY(model.configureTgxlForStation(QStringLiteral("127.0.0.1"), server.serverPort(),
                                              &reason));
        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 1500);
        auto* peer = server.nextPendingConnection();
        peer->write("V1.2.17\n"); peer->flush();
        QTRY_VERIFY_WITH_TIMEOUT(infoSequence(frames) != 0, 1500);
        auto* discovery = identityDiscovery(model);
        QVERIFY(discovery);
        sendInfo(peer, infoSequence(frames), QStringLiteral("241288-1"));
        QTRY_COMPARE(model.tgxlConnection()->identityInfo().serial, QStringLiteral("241288-1"));
        QVERIFY(!model.saveTgxlSettingsForStation(&reason));   // not admitted yet
        announce(discovery, server.serverPort(), QStringLiteral("TunerGeniusXL"),
                 QStringLiteral("241288-1"));
        QTRY_VERIFY(model.tgxlConnection()->isConnected());

        QByteArray pending;
        QStringList lines;
        const auto waitFor = [&](const QString& command) {
            quint32 seq = 0;
            (void)QTest::qWaitFor([&] {   // 0 when it never arrives
                pending += peer->readAll();
                qsizetype nl = 0;
                while ((nl = pending.indexOf('\n')) >= 0) {
                    lines.append(QString::fromUtf8(pending.left(nl)));
                    pending.remove(0, nl + 1);
                }
                for (const QString& line : lines) {
                    const qsizetype bar = line.indexOf(QLatin1Char('|'));
                    if (bar > 1 && line.mid(bar + 1) == command) {
                        seq = line.mid(1, bar - 1).toUInt();
                        lines.removeOne(line);
                        return true;
                    }
                }
                return false;
            }, 3000);
            return seq;
        };
        const auto answer = [&](quint32 seq, const QString& codeAndBody) {
            peer->write(QStringLiteral("R%1|%2\n").arg(seq).arg(codeAndBody).toUtf8());
            peer->flush();
        };

        // RD-I11: the name is one word of the `setup` line (the local
        // page's name box takes no space or '='), so a name with a space
        // is refused on the Core and nothing reaches the tuner.
        const qsizetype framesBeforeName = frames.count();
        QVERIFY(!model.setTgxlNameForStation(QStringLiteral("Shack Tuner"), &reason));
        QCOMPARE(reason, QStringLiteral("Enter a name without spaces or equals signs."));
        QCOMPARE(frames.count(), framesBeforeName);
        QVERIFY(model.setTgxlNameForStation(QStringLiteral("Shack_Tuner"), &reason));
        quint32 seq = waitFor(QStringLiteral("setup nickname=Shack_Tuner"));
        QVERIFY(seq != 0);
        answer(seq, QStringLiteral("0|"));
        QTRY_COMPARE(settings->tgxlNickname(), QStringLiteral("Shack_Tuner"));
        QCOMPARE(settings->tgxlAnswer(), QStringLiteral("The Tuner Genius took the new name."));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("TGXL_Nickname")).toString(),
                 QStringLiteral("Shack_Tuner"));

        // I5: DHCP off with no address or netmask is refused on the Core
        // and nothing reaches the tuner.
        const qsizetype sentBefore = frames.count();
        QVERIFY(!model.setTgxlNetworkForStation(false, QString(), QString(), QString(), &reason));
        QCOMPARE(reason, QStringLiteral("Without DHCP, enter an address and a netmask."));
        QCOMPARE(frames.count(), sentBefore);
        QVERIFY(model.setTgxlNetworkForStation(true, QString(), QString(), QString(), &reason));
        seq = waitFor(QStringLiteral("ifconf address= netmask= gateway= dhcp=true"));
        QVERIFY(seq != 0);
        // M9: an unobserved non-zero code (the design doc's section 6.1
        // says only that non-zero is a failure; none has been captured).
        answer(seq, QStringLiteral("1|"));
        QTRY_COMPARE(settings->tgxlAnswer(),
                     QStringLiteral("The Tuner Genius did not take the new network settings."));
        QVERIFY(!settings->tgxlNetworkKnown());

        // Revert: the tuner's name read back is saved (TgxlAdvancedPage's
        // onSetupResponse does the same).
        QVERIFY(model.readTgxlSettingsForStation(&reason));
        const quint32 setupSeq = waitFor(QStringLiteral("setup read"));
        const quint32 ifconfSeq = waitFor(QStringLiteral("ifconf read"));
        QVERIFY(setupSeq != 0 && ifconfSeq != 0);
        // M9: shapes from the local page's parsers
        // (TgxlAdvancedPage::onSetupResponse / onIfconfResponse); the
        // capture (captures/flex-tgxl-direct-NOTES.md) shows the `ifconf
        // read` request but not its reply, so both are unobserved, pending
        // hardware.
        answer(setupSeq, QStringLiteral("0|nickname=Tuner_Genius_XL"));
        answer(ifconfSeq, QStringLiteral("0|dhcp=0 ip=192.168.1.60 netmask=255.255.255.0 "
                                         "gateway=192.168.1.1"));
        QTRY_COMPARE(settings->tgxlAddress(), QStringLiteral("192.168.1.60"));
        QCOMPARE(settings->tgxlNickname(), QStringLiteral("Tuner_Genius_XL"));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("TGXL_Nickname")).toString(),
                 QStringLiteral("Tuner_Genius_XL"));
        QVERIFY(!settings->tgxlDhcp());

        QVERIFY(model.saveTgxlSettingsForStation(&reason));
        seq = waitFor(QStringLiteral("save"));
        QVERIFY(seq != 0);
        answer(seq, QStringLiteral("0|saving"));
        QTRY_COMPARE(settings->tgxlAnswer(),
                     QStringLiteral("The Tuner Genius is saving its settings and restarting."));
        // The Power Genius's record is untouched.
        QCOMPARE(settings->pgxlAnswerCount(), qint64(0));
        // Nothing operated the tuner or moved its antenna.
        for (const auto& row : frames) {
            const QString frame = row.first().toString();
            QVERIFY2(!frame.contains(QStringLiteral("operate"))
                         && !frame.contains(QStringLiteral("bypass"))
                         && !frame.contains(QStringLiteral("antenna")), qPrintable(frame));
        }
        QVERIFY(model.disconnectTgxlForStation(&reason));
    }

    // R-R3-49 / R-R3-47 (remoteTgxlControlVersion 2): a window switches the
    // Core's tuner antenna, operate and bypass whenever the radio is not on
    // the air, on a receive-only Core too (they key nothing). Each refusal
    // is in plain words and sends nothing to the tuner.
    void windowSwitchesTheTunerOnlyWhenItMay()
    {
        QString reason;
        {   // A Core that does not own its accessories.
            RadioModel plain;
            QVERIFY(!plain.setTgxlAntennaForStation(2, &reason));
            QCOMPARE(reason, QStringLiteral("This Core cannot change its amplifier and tuner settings."));
            QVERIFY(!plain.setTgxlOperateForStation(true, &reason));
            QCOMPARE(reason, QStringLiteral("This Core cannot change its amplifier and tuner settings."));
        }
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        RadioModel model;
        prepare(model);
        QSignalSpy frames(model.tgxlConnection(), &TgxlConnection::testFrameWrittenForTesting);
        const QString notConnected = QStringLiteral("The Core is not connected to the Tuner Genius.");
        QVERIFY(!model.setTgxlAntennaForStation(2, &reason));
        QCOMPARE(reason, notConnected);
        QVERIFY(!model.setTgxlOperateForStation(true, &reason));
        QCOMPARE(reason, notConnected);
        QVERIFY(!model.setTgxlBypassForStation(true, &reason));
        QCOMPARE(reason, notConnected);

        QVERIFY(model.configureTgxlForStation(QStringLiteral("127.0.0.1"), server.serverPort(),
                                              &reason));
        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 1500);
        auto* peer = server.nextPendingConnection();
        peer->write("V1.2.17\n"); peer->flush();
        QTRY_VERIFY_WITH_TIMEOUT(infoSequence(frames) != 0, 1500);
        auto* discovery = identityDiscovery(model);
        QVERIFY(discovery);
        sendInfo(peer, infoSequence(frames), QStringLiteral("241288-1"));
        QTRY_COMPARE(model.tgxlConnection()->identityInfo().serial, QStringLiteral("241288-1"));
        QVERIFY(!model.setTgxlOperateForStation(true, &reason));   // not admitted yet
        QCOMPARE(reason, notConnected);
        announce(discovery, server.serverPort(), QStringLiteral("TunerGeniusXL"),
                 QStringLiteral("241288-1"));
        QTRY_VERIFY(model.tgxlConnection()->isConnected());

        const auto sentLine = [&](const QString& command) {
            for (const auto& row : frames) {
                if (row.first().toString().endsWith(QLatin1Char('|') + command)) { return true; }
            }
            return false;
        };
        const auto switchingSent = [&] {
            for (const auto& row : frames) {
                const QString frame = row.first().toString();
                if (frame.contains(QStringLiteral("|operate=")) || frame.contains(QStringLiteral("|bypass="))
                    || frame.contains(QStringLiteral("|activate ant="))) {
                    return true;
                }
            }
            return false;
        };

        // A port outside 1 to 3.
        for (int port : {0, 4, -1}) {
            QVERIFY(!model.setTgxlAntennaForStation(port, &reason));
            QCOMPARE(reason, QStringLiteral("Choose Tuner Genius antenna 1, 2 or 3."));
        }
        // A tuner with no antenna switch.
        peer->write("S0|state one_by_three=0\n"); peer->flush();
        QTRY_VERIFY(!model.tunerModel()->hasAntennaSwitch());
        QVERIFY(!model.setTgxlAntennaForStation(2, &reason));
        QCOMPARE(reason, QStringLiteral("This Tuner Genius has no antenna switch."));
        peer->write("S0|state one_by_three=1 antA=1 operate=0 bypass=0\n"); peer->flush();
        QTRY_VERIFY(model.tunerModel()->hasAntennaSwitch());

        // The radio on the air: MOX, then TUNE.
        const QString onAir = QStringLiteral("The radio is on the air. Try again when it stops.");
        for (int keyed = 0; keyed < 2; ++keyed) {
            if (keyed == 0) { model.transmitModel().setMox(true); }
            else { model.transmitModel().setTune(true); }
            QVERIFY(!model.setTgxlAntennaForStation(2, &reason));
            QCOMPARE(reason, onAir);
            QVERIFY(!model.setTgxlOperateForStation(true, &reason));
            QCOMPARE(reason, onAir);
            QVERIFY(!model.setTgxlBypassForStation(true, &reason));
            QCOMPARE(reason, onAir);
            model.transmitModel().setMox(false);
            model.transmitModel().setTune(false);
        }

        // The Core's own MoxController: a hardware PTT press, the two-tone
        // test, and the TX to RX handover. Today's Core is receive-only and
        // its MOX pre-check refuses every key; lifting it stands in for a
        // Core that can transmit, and the keys go through the real paths.
        MoxController* const mox = model.moxController();
        QVERIFY(mox);
        mox->setMoxCheck({});
        const auto expectRefused = [&] {
            QVERIFY(!model.setTgxlAntennaForStation(2, &reason));
            QCOMPARE(reason, onAir);
            QVERIFY(!model.setTgxlOperateForStation(true, &reason));
            QCOMPARE(reason, onAir);
            QVERIFY(!model.setTgxlBypassForStation(true, &reason));
            QCOMPARE(reason, onAir);
        };
        // Hardware PTT (the radio's own PTT input).
        mox->onMicPttFromRadio(true);
        QVERIFY(mox->isMox());
        QVERIFY(!model.transmitModel().isMox());
        expectRefused();
        if (QTest::currentTestFailed()) { return; }
        mox->onMicPttFromRadio(false);
        QTRY_VERIFY(mox->state() == MoxState::Rx);
        // Two-tone, keyed by its controller through the same MoxController.
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
        // The TX to RX handover: MOX is already off but the controller is
        // still walking back to receive, so a switch still waits.
        mox->setTimerIntervals(0, 0, 0, /*keyUpMs=*/300, /*pttOutMs=*/300, 0);
        mox->setMox(true);
        QTRY_VERIFY(mox->state() == MoxState::Tx);
        mox->setMox(false);
        QVERIFY(!mox->isMox());
        QVERIFY(mox->state() != MoxState::Rx);
        QVERIFY(model.isTransmitting());
        expectRefused();
        if (QTest::currentTestFailed()) { return; }
        QTRY_VERIFY_WITH_TIMEOUT(mox->state() == MoxState::Rx, 2000);
        QVERIFY(!model.isTransmitting());
        QTest::qWait(50);
        QVERIFY(!switchingSent());

        // Off the air, on a receive-only Core: each reaches the tuner as the
        // local applet's own line.
        QVERIFY(model.receiveOnlyStationPolicy());
        QVERIFY(model.setTgxlAntennaForStation(2, &reason));
        QTRY_VERIFY(sentLine(QStringLiteral("activate ant=2")));
        // OPERATE whole (remoteTgxlControlVersion 3): bypass off, then
        // operate on, both from the one command.
        const int operateMark = frames.count();
        QVERIFY(model.setTgxlOperateForStation(true, &reason));
        QTRY_VERIFY(sentLine(QStringLiteral("operate=1")));
        {
            QStringList sinceMark;
            for (int i = operateMark; i < frames.count(); ++i) {
                const QString frame = frames.at(i).first().toString();
                sinceMark.append(frame.mid(frame.indexOf(QLatin1Char('|')) + 1));
            }
            QCOMPARE(sinceMark, (QStringList{QStringLiteral("bypass=0"),
                                             QStringLiteral("operate=1")}));
        }
        QVERIFY(model.setTgxlBypassForStation(true, &reason));
        QTRY_VERIFY(sentLine(QStringLiteral("bypass=1")));
        QVERIFY(model.setTgxlOperateForStation(false, &reason));
        QTRY_VERIFY(sentLine(QStringLiteral("operate=0")));
        // The model reports what the tuner says, not the request.
        QCOMPARE(model.tunerModel()->antennaA(), 1);
        peer->write("S0|state antA=2 operate=1 bypass=1\n"); peer->flush();
        QTRY_COMPARE(model.tunerModel()->antennaA(), 2);
        QVERIFY(model.tunerModel()->isOperate());
        QVERIFY(model.tunerModel()->isBypass());
        QVERIFY(model.disconnectTgxlForStation(&reason));
    }

    // R-R3-49 (parity Task 8, remoteTgxlControlVersion 4): a window moves
    // one relay, scans the Core's network and saves an address, whenever
    // the radio is not on the air, on a receive-only Core too. Every
    // refusal is plain words and sends nothing to the tuner.
    void windowMovesRelaysScansAndSavesAddressOnlyWhenItMay()
    {
        QString reason;
        const QString notOwning =
            QStringLiteral("This Core cannot change its amplifier and tuner settings.");
        {   // A Core that does not own its accessories.
            RadioModel plain;
            QVERIFY(!plain.moveTgxlRelayForStation(0, 1, &reason));
            QCOMPARE(reason, notOwning);
            bool called = false;
            QVERIFY(!plain.scanTgxlLanForStation([&called](const QString&) { called = true; },
                                                 &reason));
            QCOMPARE(reason, notOwning);
            QVERIFY(!plain.setTgxlAddressForStation(QStringLiteral("192.0.2.9"), 9010, &reason));
            QCOMPARE(reason, notOwning);
            QTest::qWait(20);
            QVERIFY(!called);
        }
        {   // No radio: the address has nowhere to be kept.
            RadioModel noRadio;
            noRadio.enableStationAccessoryIdentity();
            QVERIFY(!noRadio.setTgxlAddressForStation(QStringLiteral("192.0.2.9"), 9010, &reason));
            QCOMPARE(reason, QStringLiteral("Connect the Core to a radio before setting up its "
                                            "Tuner Genius XL."));
        }
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        RadioModel model;
        prepare(model);
        model.setTgxlLanScanWindowMsForTest(150);
        QSignalSpy frames(model.tgxlConnection(), &TgxlConnection::testFrameWrittenForTesting);
        const auto relaySent = [&] {
            for (const auto& row : frames) {
                if (row.first().toString().contains(QStringLiteral("|tune relay="))) { return true; }
            }
            return false;
        };
        const QString notUnderstood =
            QStringLiteral("The request to move a Tuner Genius relay was not understood.");
        for (const auto& [relay, direction] : QList<QPair<int, int>>{
                 {3, 1}, {-1, 1}, {0, 0}, {0, 2}, {1, -2}}) {
            QVERIFY(!model.moveTgxlRelayForStation(relay, direction, &reason));
            QCOMPARE(reason, notUnderstood);
        }
        const QString notConnected = QStringLiteral("The Core is not connected to the Tuner Genius.");
        QVERIFY(!model.moveTgxlRelayForStation(0, 1, &reason));
        QCOMPARE(reason, notConnected);
        // A bad address is refused with configureTgxl's words, before anything is saved.
        const QString badAddress = QStringLiteral("Enter the Tuner Genius XL's IP address or host "
                                                  "name, and a port from 1 to 65535.");
        for (const auto& [host, port] : QList<QPair<QString, int>>{
                 {QString(), 0}, {QStringLiteral("bad host!"), 9010},
                 {QStringLiteral("192.0.2.9"), 0}, {QStringLiteral("192.0.2.9"), 65536}}) {
            QVERIFY(!model.setTgxlAddressForStation(host, port, &reason));
            QCOMPARE(reason, badAddress);
        }
        QVERIFY(model.peripheralValue(QStringLiteral("TGXL_ManualIp")).isEmpty());

        QVERIFY(model.configureTgxlForStation(QStringLiteral("127.0.0.1"), server.serverPort(),
                                              &reason));
        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 1500);
        auto* peer = server.nextPendingConnection();
        peer->write("V1.2.17\n"); peer->flush();
        QTRY_VERIFY_WITH_TIMEOUT(infoSequence(frames) != 0, 1500);
        auto* discovery = identityDiscovery(model);
        QVERIFY(discovery);
        sendInfo(peer, infoSequence(frames), QStringLiteral("241288-1"));
        announce(discovery, server.serverPort(), QStringLiteral("TunerGeniusXL"),
                 QStringLiteral("241288-1"));
        QTRY_VERIFY(model.tgxlConnection()->isConnected());

        // On the air: the MOX latch, TUNE, then the Core's MoxController
        // keyed by the radio's own PTT input (its receive-only pre-check
        // lifted, standing in for a Core that can transmit).
        const QString onAir = QStringLiteral("The radio is on the air. Try again when it stops.");
        const auto expectRefused = [&] {
            QVERIFY(!model.moveTgxlRelayForStation(0, 1, &reason));
            QCOMPARE(reason, onAir);
        };
        // Parity mini-round (rulings a and b): Scan LAN only listens and the
        // address is only saved, so both go ahead on the air, as a local
        // window's do; neither switches the tuner nor sends it anything.
        const auto expectListenAndSaveTaken = [&](const QString& host) {
            bool scanAnswered = false;
            QVERIFY(model.scanTgxlLanForStation(
                [&scanAnswered](const QString&) { scanAnswered = true; }, &reason));
            QVERIFY(reason.isEmpty());
            QVERIFY(model.setTgxlAddressForStation(host, 9010, &reason));
            QVERIFY(reason.isEmpty());
            QCOMPARE(model.peripheralValue(QStringLiteral("TGXL_ManualIp")), host);
            QTRY_VERIFY(scanAnswered);
            QTRY_VERIFY(model.findChild<LanDiscovery*>(QStringLiteral("tgxlLanScan")) == nullptr);
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
        MoxController* const mox = model.moxController();
        QVERIFY(mox);
        mox->setMoxCheck({});
        mox->onMicPttFromRadio(true);
        QVERIFY(mox->isMox());
        expectRefused();
        if (QTest::currentTestFailed()) { return; }
        expectListenAndSaveTaken(QStringLiteral("192.0.2.8"));
        if (QTest::currentTestFailed()) { return; }
        mox->onMicPttFromRadio(false);
        QTRY_VERIFY(mox->state() == MoxState::Rx);
        QTest::qWait(200);
        QVERIFY(!relaySent());

        // Off the air, on a receive-only Core: each relay nudge reaches the
        // tuner as the local applet's own line, and keys nothing.
        QVERIFY(model.receiveOnlyStationPolicy());
        const auto sentLine = [&](const QString& command) {
            for (const auto& row : frames) {
                if (row.first().toString().endsWith(QLatin1Char('|') + command)) { return true; }
            }
            return false;
        };
        QVERIFY(model.moveTgxlRelayForStation(0, 1, &reason));
        QTRY_VERIFY(sentLine(QStringLiteral("tune relay=0 move=1")));
        QVERIFY(model.moveTgxlRelayForStation(1, -1, &reason));
        QTRY_VERIFY(sentLine(QStringLiteral("tune relay=1 move=-1")));
        QVERIFY(model.moveTgxlRelayForStation(2, 1, &reason));
        QTRY_VERIFY(sentLine(QStringLiteral("tune relay=2 move=1")));
        // The model reports what the tuner says, not the request.
        QCOMPARE(model.tunerModel()->relayC1(), 0);
        peer->write("S0|state relayC1=42 relayL=17 relayC2=3\n"); peer->flush();
        QTRY_COMPARE(model.tunerModel()->relayC1(), 42);
        QVERIFY(!mox->isMox());
        QVERIFY(!model.transmitModel().isTune());
        QVERIFY(!model.isTransmitting());

        // The Core's Scan LAN: the Tuner Genius announcements it heard, in
        // the scan's window, not the Power Genius's.
        QString devicesJson;
        bool answered = false;
        QVERIFY(model.scanTgxlLanForStation(
            [&](const QString& json) { devicesJson = json; answered = true; }, &reason));
        // Group B fix wave (M2): one scan per device at a time. A second
        // request while it listens joins it: no second listener, and both
        // get the same answer when the window ends.
        QString secondJson;
        bool secondAnswered = false;
        QVERIFY(model.scanTgxlLanForStation(
            [&](const QString& json) { secondJson = json; secondAnswered = true; }, &reason));
        QCOMPARE(model.findChildren<LanDiscovery*>(QStringLiteral("tgxlLanScan")).size(), 1);
        auto* scan = model.findChild<LanDiscovery*>(QStringLiteral("tgxlLanScan"));
        QVERIFY(scan);
        scan->injectDatagramForTesting(
            QStringLiteral("TunerGeniusXL ip=192.0.2.44 v=1.2.17 serial=9911-2 nickname=Shack_TGXL"),
            9010);
        scan->injectDatagramForTesting(
            QStringLiteral("PowerGeniusXL ip=192.0.2.45 v=3.8.9 serial=5501-7 nickname=Amp"), 9008);
        QTRY_VERIFY(answered);
        QVERIFY(secondAnswered);
        QCOMPARE(secondJson, devicesJson);
        const QJsonArray devices = QJsonDocument::fromJson(devicesJson.toUtf8()).array();
        bool heard = false;
        for (const QJsonValue& value : devices) {
            const QJsonObject device = value.toObject();
            QVERIFY(device.value(QStringLiteral("model")).toString().startsWith(
                QStringLiteral("TunerGenius")));
            if (device.value(QStringLiteral("serial")).toString() == QStringLiteral("9911-2")) {
                heard = true;
                QCOMPARE(device.value(QStringLiteral("address")).toString(),
                         QStringLiteral("192.0.2.44"));
                QCOMPARE(device.value(QStringLiteral("port")).toInt(), 9010);
                QCOMPARE(device.value(QStringLiteral("model")).toString(),
                         QStringLiteral("TunerGeniusXL"));
                QCOMPARE(device.value(QStringLiteral("nickname")).toString(),
                         QStringLiteral("Shack_TGXL"));
                QCOMPARE(device.keys().size(), 5);
            }
        }
        QVERIFY(heard);
        QTRY_VERIFY(model.findChild<LanDiscovery*>(QStringLiteral("tgxlLanScan")) == nullptr);

        // A typed address: saved for the Core's radio. While the Core is
        // connected its own address stays on `tuner`; once it is not, the
        // saved one shows there, and nothing is dialled.
        QVERIFY(model.setTgxlAddressForStation(QStringLiteral(" 192.0.2.9 "), 9011, &reason));
        QCOMPARE(model.peripheralValue(QStringLiteral("TGXL_ManualIp")), QStringLiteral("192.0.2.9"));
        QCOMPARE(model.peripheralValue(QStringLiteral("TGXL_ManualPort")), QStringLiteral("9011"));
        QCOMPARE(model.tunerModel()->configuredHost(), QStringLiteral("127.0.0.1"));
        QVERIFY(model.disconnectTgxlForStation(&reason));
        QTRY_COMPARE(model.tunerModel()->connectionPhase(),
                     TunerModel::ConnectionPhase::Disconnected);
        QVERIFY(model.setTgxlAddressForStation(QStringLiteral("192.0.2.10"), 9012, &reason));
        QCOMPARE(model.tunerModel()->configuredHost(), QStringLiteral("192.0.2.10"));
        QCOMPARE(model.tunerModel()->configuredPort(), 9012);
        QCOMPARE(model.tunerModel()->connectionPhase(),
                 TunerModel::ConnectionPhase::Disconnected);
        QTest::qWait(100);
        QVERIFY(!model.tgxlConnection()->isConnected());
        QCOMPARE(model.tunerModel()->connectionPhase(),
                 TunerModel::ConnectionPhase::Disconnected);

        // Group B fix wave (I1): a blank Host is kept, as a local window's
        // blank Host is, and stops auto-connect. The Core shows the blank
        // and its next start dials nothing, where a saved address dials.
        QVERIFY(model.setTgxlAddressForStation(QStringLiteral("127.0.0.1"),
                                               server.serverPort(), &reason));
        QVERIFY(model.setTgxlAddressForStation(QStringLiteral("  "), 9013, &reason));
        QVERIFY(reason.isEmpty());
        QVERIFY(model.peripheralValue(QStringLiteral("TGXL_ManualIp")).isEmpty());
        QCOMPARE(model.peripheralValue(QStringLiteral("TGXL_ManualPort")), QStringLiteral("9013"));
        QVERIFY(model.tunerModel()->configuredHost().isEmpty());
        QCOMPARE(model.tunerModel()->configuredPort(), 9013);
        const quint64 blankToken = model.tgxlConnection()->socketAttemptToken();
        model.applyPeripheralsForTest();
        QTest::qWait(150);
        QVERIFY(!server.hasPendingConnections());
        QCOMPARE(model.tgxlConnection()->socketAttemptToken(), blankToken);
        QVERIFY(model.tunerModel()->configuredHost().isEmpty());
        QCOMPARE(model.tunerModel()->connectionPhase(),
                 TunerModel::ConnectionPhase::Disconnected);
    }

    void pendingLifecycleCancellation()
    {
        QFETCH(int, action);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        RadioModel model; prepare(model);
        model.tgxlConnection()->testSetReconnectBackoffUnitMs(10);
        QString reason;
        QVERIFY(model.configureTgxlForStation(QStringLiteral("127.0.0.1"), server.serverPort(), &reason));
        QTRY_VERIFY(server.hasPendingConnections());
        auto* peer = server.nextPendingConnection();
        peer->write("V1.2.17\n"); peer->flush();
        QTRY_VERIFY(identityDiscovery(model));
        const auto token = model.tgxlConnection()->socketAttemptToken();
        if (action == 0) { QVERIFY(model.disconnectTgxlForStation(&reason)); }
        if (action == 1) { model.setFourO3AEnabled(false); }
        if (action == 2) { model.teardownPeripheralsForTest(); }
        model.tgxlConnection()->testInjectFailureForSocketAttempt(token);
        QTest::qWait(80);
        QVERIFY(!server.hasPendingConnections());
        QVERIFY(!model.tgxlConnection()->testReconnectPending());
        QVERIFY(!model.tunerModel()->hasDirectConnection());
        QVERIFY(!model.tunerModel()->isPresent());
    }
};
QTEST_GUILESS_MAIN(TgxlStationIdentityTest)
#include "tst_tgxl_station_identity.moc"
