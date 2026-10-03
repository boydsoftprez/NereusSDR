// no-port-check: NereusSDR-original. R-R3-47 / R-R3-22 / R-R3-25 Core-owned
// Power Genius XL: identity before admission, pairing only after it, band
// follow, and cancellation in every phase. Station network filter on the
// identity announcement (R-R3-22, 2026-09-24).
//
// Loopback TCP peers stand in for the amp; discovery is injected. The
// discovery announcement and the `info` reply are the real amp's, captured
// on the bench (StationPgxlController.h names the capture files). No real
// accessory is contacted and nothing is sent to hardware.
// J.J. Boyd (KG4VCF), September 2026; AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-47 / R-R3-22: a window's requests for the amp's own
// settings reach the (fake) amp as the local page's own commands, with the
// amp's answers and values published on AccessorySettingsModel. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: Lane B takes integration (R-IOS-01, R-R3-21): another amp off
// the network leaves this one's plain not-found reason. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
#include <QtTest/QtTest>
#include "OperatorWording.h"
#include <QPointer>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>

#include "core/AppSettings.h"
#include "core/LanDiscovery.h"
#include "core/PgxlConnection.h"
#include "core/SmartSdrApiListener.h"
#include "core/StationNetwork.h"
#include "core/StationDeviceSettings.h"
#include "core/StationPgxlController.h"
#include "models/AccessorySettingsModel.h"
#include "models/AmplifierModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;
using Phase = AmplifierModel::ConnectionPhase;

namespace {

// Captured from the real amp (192.168.109.235), with the address and port
// replaced by the loopback fixture's.
constexpr const char* kSerial = "10-200/24-0046";
constexpr const char* kInfoReply = "serial=10-200/24-0046  version=3.8.9 protocol=1.0 mains=240";
// The real Tuner Genius's replies (captures/flex-tgxl-direct-NOTES.md and
// the discovery capture): what answers when the amp's address points at it.
constexpr const char* kTunerInfoReply =
    "info serial=241288-1 version=1.2.17 nickname=Tuner_Genius_XL";

quint16 closedLoopbackPort()
{
    QTcpServer reservation;
    if (!reservation.listen(QHostAddress::LocalHost, 0)) { return 0; }
    const quint16 port = reservation.serverPort();
    reservation.close();
    return port;
}

quint32 sequenceOf(const QSignalSpy& frames, const QString& commandPattern)
{
    const QRegularExpression rx(QStringLiteral("^C(\\d+)\\|") + commandPattern);
    for (const auto& row : frames) {
        const auto match = rx.match(row.first().toString());
        if (match.hasMatch()) { return match.captured(1).toUInt(); }
    }
    return 0;
}

QStringList commandsOf(const QSignalSpy& frames)
{
    QStringList commands;
    for (const auto& row : frames) {
        const QString frame = row.first().toString();
        commands.append(frame.mid(frame.indexOf(QLatin1Char('|')) + 1));
    }
    return commands;
}

// What the fake amp received, line by line: the bytes on the wire.
struct Received {
    QByteArray pending;
    QStringList lines;
    void take(QTcpSocket* peer)
    {
        pending += peer->readAll();
        qsizetype nl = 0;
        while ((nl = pending.indexOf('\n')) >= 0) {
            lines.append(QString::fromUtf8(pending.left(nl)));
            pending.remove(0, nl + 1);
        }
    }
};

// Wait for the amp to receive `command` (after `from` lines); its sequence.
quint32 waitForCommand(QTcpSocket* peer, Received& rx, const QString& command,
                       qsizetype* from)
{
    quint32 seq = 0;
    // The sequence stays 0 when the command never arrives.
    (void)QTest::qWaitFor([&] {
        rx.take(peer);
        for (qsizetype i = *from; i < rx.lines.size(); ++i) {
            const QString& line = rx.lines.at(i);
            const qsizetype bar = line.indexOf(QLatin1Char('|'));
            if (line.startsWith(QLatin1Char('C')) && bar > 1 && line.mid(bar + 1) == command) {
                seq = line.mid(1, bar - 1).toUInt();
                *from = i + 1;
                return true;
            }
        }
        return false;
    }, 3000);
    return seq;
}

void reply(QTcpSocket* peer, quint32 seq, const QString& codeAndBody)
{
    peer->write(QStringLiteral("R%1|%2\n").arg(seq).arg(codeAndBody).toUtf8());
    peer->flush();
}

} // namespace

class StationPgxlControllerTest : public QObject {
    Q_OBJECT

    static void prepare(RadioModel& model, const QString& mac = QStringLiteral("aa:bb:cc:dd:ee:71"))
    {
        model.enableStationAccessoryIdentity();
        model.setReceiveOnlyStationPolicy(true);
        RadioInfo radio;
        radio.macAddress = mac;
        model.setLastRadioInfoForTest(radio);
        model.setConnectionStateForTest(ConnectionState::Connected);
        model.smartSdrListener()->setListenEndpointForTesting(QHostAddress::LocalHost, 0);
        model.setPeripheralValue(QStringLiteral("FourO3A_Enabled"), QStringLiteral("True"));
    }

    static LanDiscovery* discoveryOf(RadioModel& model)
    {
        auto* controller = model.findChild<StationPgxlController*>();
        return controller ? controller->findChild<LanDiscovery*>() : nullptr;
    }

    static void announce(RadioModel& model, quint16 port, const QString& product,
                         const QString& serial, const QString& nickname = QStringLiteral("PowerGeniusXL"))
    {
        discoveryOf(model)->injectDatagramForTesting(
            QStringLiteral("%1 ip=127.0.0.1 v=3.8.9 serial=%2 nickname=%3")
                .arg(product, serial, nickname), port);
    }

    // Dial through the Core, answer the V banner and the `info` request.
    static QTcpSocket* answerUpToInfo(RadioModel& model, QTcpServer& server,
                                      const QSignalSpy& frames, const char* banner,
                                      const char* infoBody)
    {
        QString reason;
        if (!model.configurePgxlForStation(QStringLiteral("127.0.0.1"), server.serverPort(),
                                           &reason)) {
            qWarning() << "configure refused:" << reason;
            return nullptr;
        }
        if (!QTest::qWaitFor([&] { return server.hasPendingConnections(); }, 2000)) {
            return nullptr;
        }
        QTcpSocket* peer = server.nextPendingConnection();
        peer->write(QByteArray(banner) + '\n');
        peer->flush();
        if (!QTest::qWaitFor([&] { return sequenceOf(frames, QStringLiteral("info$")) != 0; },
                             2000)) {
            return nullptr;
        }
        peer->write(QStringLiteral("R%1|0|%2\n")
                        .arg(sequenceOf(frames, QStringLiteral("info$")))
                        .arg(QString::fromLatin1(infoBody)).toUtf8());
        peer->flush();
        if (!QTest::qWaitFor([&] { return discoveryOf(model) != nullptr; }, 2000)) {
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

    // The whole address is checked before anything is saved or dialled.
    void validatesAddressBeforeChangingAnything()
    {
        RadioModel model;
        prepare(model);
        model.setPeripheralValue(QStringLiteral("PGXL_ManualIp"), QStringLiteral("saved.example"));
        model.setPeripheralValue(QStringLiteral("PGXL_ManualPort"), QStringLiteral("9008"));
        const quint64 token = model.pgxlConnection()->socketAttemptToken();
        QString reason;
        for (const auto& host : {QString(), QStringLiteral("bad host"),
                                QStringLiteral("http://127.0.0.1"),
                                QStringLiteral("127.0.0.1\nstatus")}) {
            QVERIFY(!model.configurePgxlForStation(host, 9008, &reason));
            QVERIFY(!reason.isEmpty());
            QCOMPARE(model.peripheralValue(QStringLiteral("PGXL_ManualIp")),
                     QStringLiteral("saved.example"));
            QCOMPARE(model.pgxlConnection()->socketAttemptToken(), token);
        }
        QVERIFY(!model.configurePgxlForStation(QStringLiteral("127.0.0.1"), 0, &reason));
        model.setPeripheralValue(QStringLiteral("FourO3A_Enabled"), QStringLiteral("False"));
        QVERIFY(!model.configurePgxlForStation(QStringLiteral("127.0.0.1"), 9008, &reason));
        QCOMPARE(reason, QStringLiteral("Turn on 4O3A on the Core before connecting the Power Genius."));
        model.setConnectionStateForTest(ConnectionState::Disconnected);
        QVERIFY(!model.configurePgxlForStation(QStringLiteral("127.0.0.1"), 9008, &reason));
        QVERIFY(model.disconnectPgxlForStation(&reason));

        // A window with no Core controller has nothing to configure.
        RadioModel local;
        QVERIFY(!local.configurePgxlForStation(QStringLiteral("127.0.0.1"), 9008, &reason));
        QCOMPARE(reason, QStringLiteral("This Core cannot change its amplifier and tuner settings."));
    }

    // A real Power Genius (captured discovery plus the same serial in its own
    // info reply) is admitted, then paired, then follows the band.
    void realPowerGeniusIsAdmittedThenPairedThenFollowsBand()
    {
        QTcpServer amp;
        QVERIFY(amp.listen(QHostAddress::LocalHost, 0));
        RadioModel model;
        prepare(model);
        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        QVERIFY(a >= 0);
        model.sliceById(a)->setFrequency(14200000);
        auto* pgxl = model.pgxlConnection();
        auto* ampModel = model.amplifierModel();
        QSignalSpy frames(pgxl, &PgxlConnection::testFrameWrittenForTesting);
        QSignalSpy paired(pgxl, &PgxlConnection::pairingResult);

        QTcpSocket* peer = answerUpToInfo(model, amp, frames, "V3.8.9", kInfoReply);
        QVERIFY(peer);
        QCOMPARE(ampModel->connectionPhase(), Phase::Identifying);
        QVERIFY(!pgxl->isConnected());
        // Before admission the amp was asked who it is, and nothing else.
        QCOMPARE(commandsOf(frames), QStringList{QStringLiteral("info")});

        announce(model, amp.serverPort(), QStringLiteral("PowerGeniusXL"),
                 QString::fromLatin1(kSerial));
        QTRY_VERIFY(pgxl->isConnected());
        QCOMPARE(ampModel->connectionPhase(), Phase::Connected);
        QCOMPARE(ampModel->deviceModel(), QStringLiteral("PowerGeniusXL"));
        QCOMPARE(ampModel->deviceSerial(), QString::fromLatin1(kSerial));
        QCOMPARE(ampModel->deviceVersion(), QStringLiteral("3.8.9"));
        QCOMPARE(ampModel->deviceNickname(), QStringLiteral("PowerGeniusXL"));
        QCOMPARE(ampModel->configuredHost(), QStringLiteral("127.0.0.1"));
        QCOMPARE(ampModel->configuredPort(), int(amp.serverPort()));
        QCOMPARE(model.peripheralValue(QStringLiteral("PGXL_ManualPort")),
                 QString::number(amp.serverPort()));

        // Admitted: now paired automatically.
        const QStringList sent = commandsOf(frames);
        QVERIFY2(sent.filter(QStringLiteral("amplifier create ")).size() == 1, qPrintable(sent.join('\n')));
        QVERIFY2(sent.filter(QStringLiteral("flexradio ampslice=A serial=")).size() == 1,
                 qPrintable(sent.join('\n')));
        QVERIFY(sent.contains(QStringLiteral("keepalive enable")));
        // QStringList::indexOf(QRegularExpression) matches the whole string.
        const qsizetype createAt = sent.indexOf(QRegularExpression(
            QStringLiteral("amplifier create .*")));
        QVERIFY(createAt > sent.indexOf(QStringLiteral("info")));
        QCOMPARE(sent.indexOf(QStringLiteral("info")), 0);

        // Band follow after the amp accepts the pairing.
        const quint32 pairSeq = sequenceOf(frames, QStringLiteral("flexradio ampslice=A serial=\\S+ txant"));
        QVERIFY(pairSeq != 0);
        peer->write(QStringLiteral("R%1|0|\n").arg(pairSeq).toUtf8());
        peer->flush();
        QTRY_COMPARE(paired.count(), 1);
        QVERIFY(paired.first().first().toBool());
        QTRY_VERIFY(!commandsOf(frames).filter(QStringLiteral(" band=14200000")).isEmpty());

        QString reason;
        QVERIFY(model.disconnectPgxlForStation(&reason));
        QCOMPARE(ampModel->connectionPhase(), Phase::Disconnected);
        QVERIFY(!pgxl->isConnected());
    }

    // R-R3-47 / R-R3-22: a window's requests for the amp's own settings. The
    // Core sends each as exactly the command the local Advanced page sends
    // (the fake amp records the bytes), matches the amp's answer by its
    // sequence and publishes the values the amp took or reported, and its
    // answer in plain words. Nothing reaches an amp the Core has not
    // admitted, a bad value is refused before anything is sent, and nothing
    // operates the amp.
    void deviceSettingsReachTheAmpAsTheLocalPageSendsThem()
    {
        QTcpServer amp;
        QVERIFY(amp.listen(QHostAddress::LocalHost, 0));
        RadioModel model;
        prepare(model);
        auto* pgxl = model.pgxlConnection();
        const AccessorySettingsModel* settings = model.accessorySettingsModel();
        QSignalSpy frames(pgxl, &PgxlConnection::testFrameWrittenForTesting);
        QString reason;

        // No amp yet: refused, nothing sent.
        QVERIFY(!model.setPgxlNameForStation(QStringLiteral("Shack"), &reason));
        QCOMPARE(reason, QStringLiteral("The Core is not connected to the Power Genius."));
        QVERIFY(!model.savePgxlSettingsForStation(&reason));
        QVERIFY(!model.readPgxlSettingsForStation(&reason));
        QCOMPARE(frames.count(), 0);

        // An amp that has not been admitted gets nothing but `info`.
        QTcpSocket* peer = answerUpToInfo(model, amp, frames, "V3.8.9", kInfoReply);
        QVERIFY(peer);
        QVERIFY(!model.setPgxlNetworkForStation(true, QString(), QString(), QString(), &reason));
        QCOMPARE(commandsOf(frames), QStringList{QStringLiteral("info")});
        announce(model, amp.serverPort(), QStringLiteral("PowerGeniusXL"),
                 QString::fromLatin1(kSerial));
        QTRY_VERIFY(pgxl->isConnected());

        Received rx;
        qsizetype from = 0;

        // Name: `setup nickname=`, saved on the Core beside it.
        QVERIFY2(model.setPgxlNameForStation(QStringLiteral(" Shack_PGXL "), &reason),
                 qPrintable(reason));
        QCOMPARE(settings->pgxlAnswer(),
                 QStringLiteral("Sent to the Power Genius. Waiting for its answer."));
        quint32 seq = waitForCommand(peer, rx, QStringLiteral("setup nickname=Shack_PGXL"), &from);
        QVERIFY(seq != 0);
        const qint64 before = settings->pgxlAnswerCount();
        reply(peer, seq, QStringLiteral("0|"));
        QTRY_COMPARE(settings->pgxlNickname(), QStringLiteral("Shack_PGXL"));
        QCOMPARE(settings->pgxlAnswer(), QStringLiteral("The Power Genius took the new name."));
        QVERIFY(settings->pgxlAnswerAccepted());
        QCOMPARE(settings->pgxlAnswerCount(), before + 1);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("PGXL_Nickname")).toString(),
                 QStringLiteral("Shack_PGXL"));

        // Bias, refused by the amp: the value is not taken; the answer says so.
        QVERIFY(model.setPgxlHardwareForStation(QStringLiteral("biasMode"),
                                                QStringLiteral("ClassA"), &reason));
        seq = waitForCommand(peer, rx, QStringLiteral("setup bias=a"), &from);
        QVERIFY(seq != 0);
        // M9: a refusal. 50000015 is the code a real Power Genius sent when it
        // refused `amplifier create` (bench note of 2026-05-21 in
        // RadioModel.cpp, above the PGXL_PairModel read); the design doc
        // (2026-05-18-pgxl-tgxl-and-analog-smeter-design.md section 6.1, from
        // the FlexRadio wiki) says only that non-zero is a failure. The amp's
        // refusal of a `setup` command itself has not been observed.
        reply(peer, seq, QStringLiteral("50000015|"));
        QTRY_COMPARE(settings->pgxlAnswer(),
                     QStringLiteral("The Power Genius did not take the new setting."));
        QVERIFY(!settings->pgxlAnswerAccepted());
        QVERIFY(settings->pgxlBiasMode().isEmpty());

        // Fan and LED.
        QVERIFY(model.setPgxlHardwareForStation(QStringLiteral("fanMode"),
                                                QStringLiteral("Quiet"), &reason));
        seq = waitForCommand(peer, rx, QStringLiteral("setup fan=quiet"), &from);
        QVERIFY(seq != 0);
        reply(peer, seq, QStringLiteral("0|"));
        QTRY_COMPARE(settings->pgxlFanMode(), QStringLiteral("Quiet"));
        QVERIFY(model.setPgxlHardwareForStation(QStringLiteral("ledIntensity"), 40, &reason));
        seq = waitForCommand(peer, rx, QStringLiteral("setup led=40"), &from);
        QVERIFY(seq != 0);
        reply(peer, seq, QStringLiteral("0|"));
        QTRY_COMPARE(settings->pgxlLedIntensity(), 40);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("PGXL_LedIntensity")).toInt(), 40);

        // Values the page could never send are refused before anything leaves.
        const qsizetype sentBefore = frames.count();
        QVERIFY(!model.setPgxlHardwareForStation(QStringLiteral("ledIntensity"), 101, &reason));
        QCOMPARE(reason, QStringLiteral("Choose an LED brightness from 0 to 100."));
        QVERIFY(!model.setPgxlHardwareForStation(QStringLiteral("fanMode"),
                                                 QStringLiteral("Loud"), &reason));
        QVERIFY(!model.setPgxlHardwareForStation(QStringLiteral("biasMode"),
                                                 QStringLiteral("B"), &reason));
        QVERIFY(!model.setPgxlHardwareForStation(QStringLiteral("operate"), 1, &reason));
        QVERIFY(!model.setPgxlNetworkForStation(false, QStringLiteral("192.168.1.256"),
                                                QStringLiteral("255.255.255.0"),
                                                QStringLiteral("192.168.1.1"), &reason));
        QCOMPARE(reason, QStringLiteral("Enter each address as four numbers from 0 to 255 "
                                        "separated by dots."));
        QVERIFY(!model.setPgxlNameForStation(QStringLiteral("Amp\nstatus"), &reason));
        QCOMPARE(reason, QStringLiteral("Enter a name without line breaks or tabs."));
        // I5: DHCP off needs an address and a netmask; a gateway, when
        // given, on the same network. The Core is the gate for every app.
        QVERIFY(!model.setPgxlNetworkForStation(false, QString(), QString(), QString(), &reason));
        QCOMPARE(reason, QStringLiteral("Without DHCP, enter an address and a netmask."));
        QVERIFY(!model.setPgxlNetworkForStation(false, QStringLiteral("192.168.1.50"), QString(),
                                                QString(), &reason));
        QCOMPARE(reason, QStringLiteral("Without DHCP, enter an address and a netmask."));
        QVERIFY(!model.setPgxlNetworkForStation(false, QStringLiteral("192.168.1.50"),
                                                QStringLiteral("255.0.255.0"), QString(),
                                                &reason));
        QCOMPARE(reason, QStringLiteral("Enter a netmask such as 255.255.255.0."));
        QVERIFY(!model.setPgxlNetworkForStation(false, QStringLiteral("0.0.0.0"),
                                                QStringLiteral("255.255.255.0"), QString(),
                                                &reason));
        QCOMPARE(reason, QStringLiteral("Enter an address the device can use on your network."));
        QVERIFY(!model.setPgxlNetworkForStation(false, QStringLiteral("192.168.1.50"),
                                                QStringLiteral("255.255.255.0"),
                                                QStringLiteral("10.0.0.1"), &reason));
        QCOMPARE(reason, QStringLiteral("Enter a gateway on the same network as the address, "
                                        "or leave it empty."));
        QVERIFY(OperatorWording::isPlain(reason));
        // Follow-up 7: a /32 netmask, and the subnet's network and broadcast
        // addresses, cannot be a device's fixed setting.
        QVERIFY(!model.setPgxlNetworkForStation(false, QStringLiteral("192.168.1.50"),
                                                QStringLiteral("255.255.255.255"), QString(),
                                                &reason));
        QCOMPARE(reason, QStringLiteral("Enter a netmask such as 255.255.255.0."));
        for (const char* address : {"192.168.1.0", "192.168.1.255"}) {
            QVERIFY(!model.setPgxlNetworkForStation(false, QString::fromLatin1(address),
                                                    QStringLiteral("255.255.255.0"), QString(),
                                                    &reason));
            QCOMPARE(reason,
                     QStringLiteral("Enter an address the device can use on your network."));
        }
        // Rework part 6: a gateway at the subnet's broadcast (or network)
        // address is refused too.
        for (const char* gateway : {"192.168.1.255", "192.168.1.0"}) {
            QCOMPARE(StationDeviceSettings::networkProblem(
                         false, QStringLiteral("192.168.1.50"), QStringLiteral("255.255.255.0"),
                         QString::fromLatin1(gateway)),
                     QStringLiteral("Enter a gateway on the same network as the address, "
                                    "or leave it empty."));
        }
        QVERIFY(StationDeviceSettings::networkProblem(false, QStringLiteral("10.0.0.0"),
                                                      QStringLiteral("255.255.255.254"),
                                                      QString()).isEmpty());   // /31 link
        QCOMPARE(frames.count(), sentBefore);

        // Network: `ifconf address= netmask= gateway= dhcp=`.
        QVERIFY(model.setPgxlNetworkForStation(false, QStringLiteral("192.168.1.50"),
                                               QStringLiteral("255.255.255.0"),
                                               QStringLiteral("192.168.1.1"), &reason));
        seq = waitForCommand(peer, rx,
                             QStringLiteral("ifconf address=192.168.1.50 netmask=255.255.255.0 "
                                            "gateway=192.168.1.1 dhcp=false"),
                             &from);
        QVERIFY(seq != 0);
        reply(peer, seq, QStringLiteral("0|"));
        QTRY_VERIFY(settings->pgxlNetworkKnown());
        QCOMPARE(settings->pgxlAddress(), QStringLiteral("192.168.1.50"));
        QVERIFY(!settings->pgxlDhcp());
        QCOMPARE(settings->pgxlAnswer(),
                 QStringLiteral("The Power Genius took the new network settings."));

        // Revert: `setup read` then `ifconf read`; the amp's values replace ours.
        QVERIFY(model.readPgxlSettingsForStation(&reason));
        const quint32 setupSeq = waitForCommand(peer, rx, QStringLiteral("setup read"), &from);
        const quint32 ifconfSeq = waitForCommand(peer, rx, QStringLiteral("ifconf read"), &from);
        QVERIFY(setupSeq != 0 && ifconfSeq != 0);
        // M9: reply shapes, none captured from a real amp (pending hardware
        // evidence). `setup read`: the design doc's section 6.4 (verbatim
        // from the FlexRadio wiki) documents `nickname= fan= meffa= led=`;
        // `bias=` is not in that reply and is unobserved (the Core reads it
        // only if the amp sends it). `ifconf read`: `dhcp=` 0/1 and `ip=`
        // are the keys TgxlAdvancedPage::onIfconfResponse (the local page's
        // parser, the only one in the tree) reads; the design doc's
        // section 6.4 documents `address=` and `dhcp=false` instead.
        reply(peer, setupSeq, QStringLiteral("0|nickname=Amp2 bias=classa fan=continuous led=90"));
        reply(peer, ifconfSeq, QStringLiteral("0|dhcp=1 ip=10.0.0.5 netmask=255.0.0.0 "
                                              "gateway=10.0.0.1"));
        QTRY_COMPARE(settings->pgxlAddress(), QStringLiteral("10.0.0.5"));
        QCOMPARE(settings->pgxlNickname(), QStringLiteral("Amp2"));
        QCOMPARE(settings->pgxlBiasMode(), QStringLiteral("ClassA"));
        QCOMPARE(settings->pgxlFanMode(), QStringLiteral("Continuous"));
        QCOMPARE(settings->pgxlLedIntensity(), 90);
        QVERIFY(settings->pgxlDhcp());
        QCOMPARE(settings->pgxlNetmask(), QStringLiteral("255.0.0.0"));
        QCOMPARE(settings->pgxlGateway(), QStringLiteral("10.0.0.1"));
        QCOMPARE(settings->pgxlAnswer(),
                 QStringLiteral("The Power Genius sent its network settings."));

        // Save & Reboot: `save`, acknowledged with "saving".
        QVERIFY(model.savePgxlSettingsForStation(&reason));
        seq = waitForCommand(peer, rx, QStringLiteral("save"), &from);
        QVERIFY(seq != 0);
        reply(peer, seq, QStringLiteral("0|saving"));
        QTRY_COMPARE(settings->pgxlAnswer(),
                     QStringLiteral("The Power Genius is saving its settings and restarting."));

        // The bytes on the wire are the connection's own framing.
        rx.take(peer);
        QVERIFY(rx.lines.contains(QStringLiteral("C%1|save").arg(seq)));
        // Nothing asked the amp to operate.
        for (const QString& line : rx.lines) {
            QVERIFY2(!line.contains(QStringLiteral("operate")), qPrintable(line));
        }
        QVERIFY(!model.amplifierModel()->operate());

        // A request still waiting when the amp goes away: said plainly.
        QVERIFY(model.setPgxlNameForStation(QStringLiteral("Late"), &reason));
        QVERIFY(waitForCommand(peer, rx, QStringLiteral("setup nickname=Late"), &from) != 0);
        peer->abort();
        QTRY_COMPARE(settings->pgxlAnswer(),
                     QStringLiteral("The Power Genius went offline before it answered."));
        QVERIFY(!settings->pgxlAnswerAccepted());
        QVERIFY(model.disconnectPgxlForStation(&reason));
        // A new scope forgets the old amp's values, keeps the last answer.
        QVERIFY(settings->pgxlNickname().isEmpty());
        QVERIFY(!settings->pgxlNetworkKnown());
        QCOMPARE(settings->pgxlAnswer(),
                 QStringLiteral("The Power Genius went offline before it answered."));
    }

    // A Tuner Genius answering at the amp's address: never admitted, never
    // paired, asked for nothing but `info`.
    // M5 (R-R3-47): a request the amp never answers is given up after
    // kAnswerTimeoutMs with a plain answer, on the Core's clock (injected
    // here); a late answer to it changes nothing.
    void unansweredDeviceSettingTimesOut()
    {
        AccessorySettingsModel model;
        StationDeviceSettings::Wire wire;
        QStringList sentCommands;
        wire.connected = [] { return true; };
        wire.writeSetup = [&](const QMap<QString, QString>& fields) {
            sentCommands.append(QStringLiteral("setup ") + fields.firstKey());
            return quint32(41);
        };
        StationDeviceSettings settings(StationDeviceSettings::Device::Pgxl, std::move(wire));
        settings.setModel(&model);
        qint64 now = 1'000'000;
        settings.setClockForTesting([&now] { return now; });
        QString reason;
        QVERIFY(settings.setName(QStringLiteral("Shack_PGXL"), &reason));
        QCOMPARE(sentCommands, QStringList{QStringLiteral("setup nickname")});
        QCOMPARE(model.pgxlAnswer(),
                 QStringLiteral("Sent to the Power Genius. Waiting for its answer."));

        now += StationDeviceSettings::kAnswerTimeoutMs - 1;
        settings.checkTimeouts();
        QCOMPARE(model.pgxlAnswer(),
                 QStringLiteral("Sent to the Power Genius. Waiting for its answer."));

        now += 1;
        settings.checkTimeouts();
        // Follow-up 4: the Core's own clock is monotonic (a Pi or Rock has no
        // real-time clock and its wall clock steps at boot): it counts from
        // the object's start, not from 1970.
        StationDeviceSettings fresh(StationDeviceSettings::Device::Tgxl, {});
        QVERIFY(fresh.clockNowForTesting() >= 0);
        QVERIFY(fresh.clockNowForTesting() < 60'000);
        QCOMPARE(model.pgxlAnswer(),
                 QStringLiteral("The Power Genius did not answer. Try again."));
        QVERIFY(!model.pgxlAnswerAccepted());
        QVERIFY(OperatorWording::isPlain(model.pgxlAnswer()));

        // The late answer is ignored: the name was never confirmed.
        settings.onReply(41, true, QString());
        QCOMPARE(model.pgxlAnswer(),
                 QStringLiteral("The Power Genius did not answer. Try again."));
        QVERIFY(model.pgxlNickname().isEmpty());
    }

    // RD-I11: the name is one word of the amp's `setup` line. A name with a
    // space or '=' would add fields ("Shack bias=a" sets the bias); it is
    // refused with a plain reason and nothing goes to the amp.
    void aNameThatWouldAddFieldsIsRefused()
    {
        for (const auto device : {StationDeviceSettings::Device::Pgxl,
                                  StationDeviceSettings::Device::Tgxl}) {
            AccessorySettingsModel model;
            StationDeviceSettings::Wire wire;
            QStringList sent;
            wire.connected = [] { return true; };
            wire.writeSetup = [&](const QMap<QString, QString>& fields) {
                sent.append(fields.firstKey() + QLatin1Char('=') + fields.first());
                return quint32(7);
            };
            StationDeviceSettings settings(device, std::move(wire));
            settings.setModel(&model);
            for (const QString& bad : {QStringLiteral("Shack bias=a"),
                                       QStringLiteral("Shack PGXL"),
                                       QStringLiteral("led=100")}) {
                QString reason;
                QVERIFY(!settings.setName(bad, &reason));
                QCOMPARE(reason, QStringLiteral("Enter a name without spaces or equals signs."));
                QVERIFY(OperatorWording::isPlain(reason));
            }
            QVERIFY(sent.isEmpty());
            QString reason;
            QVERIFY2(settings.setName(QStringLiteral(" Shack_Amp "), &reason), qPrintable(reason));
            QCOMPARE(sent, QStringList{QStringLiteral("nickname=Shack_Amp")});
        }
    }

    void tunerGeniusAtAmpAddressIsNeverAdmitted()
    {
        AppSettings::instance().setValue(QStringLiteral("PGXL_AutoReconnect"), QStringLiteral("False"));
        QTcpServer tuner;
        QVERIFY(tuner.listen(QHostAddress::LocalHost, 0));
        RadioModel model;
        prepare(model);
        auto* pgxl = model.pgxlConnection();
        QSignalSpy frames(pgxl, &PgxlConnection::testFrameWrittenForTesting);
        QSignalSpy connected(pgxl, &PgxlConnection::connected);
        QTcpSocket* peer = answerUpToInfo(model, tuner, frames, "V1.2.17", kTunerInfoReply);
        QVERIFY(peer);
        announce(model, tuner.serverPort(), QStringLiteral("TunerGenius"),
                 QStringLiteral("241288-1"), QStringLiteral("Tuner_Genius_XL"));
        QTRY_COMPARE(model.amplifierModel()->connectionPhase(), Phase::Error);
        QVERIFY(model.amplifierModel()->connectionError().contains(QStringLiteral("TunerGenius")));
        QCOMPARE(model.amplifierModel()->deviceModel(), QStringLiteral("TunerGenius"));
        QTest::qWait(200);
        QCOMPARE(connected.count(), 0);
        QVERIFY(!pgxl->isConnected());
        QVERIFY(!model.amplifierModel()->present());
        QCOMPARE(commandsOf(frames), QStringList{QStringLiteral("info")});
    }

    // A Power Genius announcement with a different serial: not admitted.
    void serialMismatchIsNeverAdmitted()
    {
        AppSettings::instance().setValue(QStringLiteral("PGXL_AutoReconnect"), QStringLiteral("False"));
        QTcpServer amp;
        QVERIFY(amp.listen(QHostAddress::LocalHost, 0));
        RadioModel model;
        prepare(model);
        auto* pgxl = model.pgxlConnection();
        QSignalSpy frames(pgxl, &PgxlConnection::testFrameWrittenForTesting);
        QVERIFY(answerUpToInfo(model, amp, frames, "V3.8.9", kInfoReply));
        announce(model, amp.serverPort(), QStringLiteral("PowerGeniusXL"),
                 QStringLiteral("10-200/24-0047"));
        QTRY_COMPARE(model.amplifierModel()->connectionPhase(), Phase::Error);
        QCOMPARE(model.amplifierModel()->connectionError(),
                 QStringLiteral("The Power Genius at this address is not the one the Core found "
                                "on its network. Check the amplifier's address and port."));
        QVERIFY(!pgxl->isConnected());
        QCOMPARE(commandsOf(frames), QStringList{QStringLiteral("info")});
    }

    // R-R3-22 / R-R3-47: with the Core's station rule set, the identity
    // announcement is heard only from the station network (here this
    // computer, station_bind = 127.0.0.1): the same announcement from
    // another network admits nothing.
    void coreHearsIdentityOnlyFromTheStationNetwork()
    {
        QTcpServer amp;
        QVERIFY(amp.listen(QHostAddress::LocalHost, 0));
        RadioModel model;
        prepare(model);
        model.setStationBind(QStringLiteral("127.0.0.1"));
        auto* pgxl = model.pgxlConnection();
        QSignalSpy frames(pgxl, &PgxlConnection::testFrameWrittenForTesting);
        QVERIFY(answerUpToInfo(model, amp, frames, "V3.8.9", kInfoReply));
        const QString line = QStringLiteral("PowerGeniusXL ip=127.0.0.1 v=3.8.9 serial=%1 nickname=PowerGeniusXL")
                                 .arg(QString::fromLatin1(kSerial));
        discoveryOf(model)->injectDatagramForTesting(line, amp.serverPort(),
                                                     QHostAddress(QStringLiteral("192.168.1.43")));
        QTest::qWait(100);
        QVERIFY(!pgxl->isConnected());
        QCOMPARE(model.amplifierModel()->connectionPhase(), Phase::Identifying);
        discoveryOf(model)->injectDatagramForTesting(line, amp.serverPort(),
                                                     QHostAddress(QHostAddress::LocalHost));
        QTRY_VERIFY(pgxl->isConnected());
        QString reason;
        QVERIFY(model.disconnectPgxlForStation(&reason));
    }

    // M7 (R-R3-47): an amp heard only from another network than the
    // radio's is refused, and the reason says so and which Core setting
    // allows it.
    void ampOnAnotherNetworkSaysHowToAllowIt()
    {
        AppSettings::instance().setValue(QStringLiteral("PGXL_AutoReconnect"), QStringLiteral("False"));
        QTcpServer amp;
        QVERIFY(amp.listen(QHostAddress::LocalHost, 0));
        RadioModel model;
        prepare(model);
        model.setStationBind(QStringLiteral("127.0.0.1"));
        auto* pgxl = model.pgxlConnection();
        QSignalSpy frames(pgxl, &PgxlConnection::testFrameWrittenForTesting);
        // A serial no real amp has, so a Power Genius on the bench LAN
        // (heard by broadcast) is never taken for this one.
        QVERIFY(answerUpToInfo(model, amp, frames, "V3.8.9",
                               "serial=77-777/77-7777  version=3.8.9 protocol=1.0 mains=240"));
        const QString line = QStringLiteral(
            "PowerGeniusXL ip=192.168.1.43 v=3.8.9 serial=77-777/77-7777 nickname=PowerGeniusXL");
        discoveryOf(model)->injectDatagramForTesting(line, amp.serverPort(),
                                                     QHostAddress(QStringLiteral("192.168.1.43")));
        QTRY_COMPARE_WITH_TIMEOUT(model.amplifierModel()->connectionPhase(), Phase::Error, 6000);
        const QString error = model.amplifierModel()->connectionError();
        QCOMPARE(error, StationNetwork::offNetworkReason(QStringLiteral("Power Genius"),
                                                         QStringLiteral("192.168.1.43")));
        QVERIFY(error.contains(QStringLiteral("station_bind")));
        QVERIFY(OperatorWording::isPlain(error));
        QCOMPARE(OperatorReasonText::forDisplay(error), error);   // a window shows it as sent
        QVERIFY(!pgxl->isConnected());
        QString reason;
        QVERIFY(model.disconnectPgxlForStation(&reason));
    }

    // Follow-up 8: another Power Genius announcing from another network
    // (not this amp's serial, not its address) is not a reason to say this
    // amp is on another network.
    void anotherAmpOffTheNetworkIsNotThisAmp()
    {
        AppSettings::instance().setValue(QStringLiteral("PGXL_AutoReconnect"), QStringLiteral("False"));
        QTcpServer amp;
        QVERIFY(amp.listen(QHostAddress::LocalHost, 0));
        RadioModel model;
        prepare(model);
        model.setStationBind(QStringLiteral("127.0.0.1"));
        auto* pgxl = model.pgxlConnection();
        QSignalSpy frames(pgxl, &PgxlConnection::testFrameWrittenForTesting);
        // This amp's serial is one no real amp has: a Power Genius on the
        // bench LAN announcing by broadcast (the discovery sockets hear every
        // network) is then not this amp either.
        QVERIFY(answerUpToInfo(model, amp, frames, "V3.8.9",
                               "serial=77-777/77-7777  version=3.8.9 protocol=1.0 mains=240"));
        discoveryOf(model)->injectDatagramForTesting(
            QStringLiteral("PowerGeniusXL ip=192.168.1.77 v=3.8.9 serial=99-999/99-9999 nickname=Other"),
            amp.serverPort(), QHostAddress(QStringLiteral("192.168.1.77")));
        QTRY_COMPARE_WITH_TIMEOUT(model.amplifierModel()->connectionPhase(), Phase::Error, 6000);
        const QString error = model.amplifierModel()->connectionError();
        QVERIFY2(!error.contains(QStringLiteral("different network")), qPrintable(error));
        QVERIFY2(error.startsWith(QStringLiteral("The Core did not find a Power Genius at this "
                                                  "address")),
                 qPrintable(error));
        QString reason;
        QVERIFY(model.disconnectPgxlForStation(&reason));
    }

    // Anything that answers V but never says who it is times out unpaired.
    void silentPeerTimesOutUnpaired()
    {
        AppSettings::instance().setValue(QStringLiteral("PGXL_AutoReconnect"), QStringLiteral("False"));
        QTcpServer amp;
        QVERIFY(amp.listen(QHostAddress::LocalHost, 0));
        RadioModel model;
        prepare(model);
        auto* pgxl = model.pgxlConnection();
        pgxl->testSetIdentityTimeoutMs(200);
        QSignalSpy frames(pgxl, &PgxlConnection::testFrameWrittenForTesting);
        QString reason;
        QVERIFY(model.configurePgxlForStation(QStringLiteral("127.0.0.1"), amp.serverPort(), &reason));
        QTRY_VERIFY(amp.hasPendingConnections());
        QTcpSocket* peer = amp.nextPendingConnection();
        peer->write("V3.8.9\n");
        peer->flush();
        QTRY_COMPARE(model.amplifierModel()->connectionPhase(), Phase::Error);
        QCOMPARE(model.amplifierModel()->connectionError(),
                 QStringLiteral("The device at this address did not answer as a Power Genius "
                                "in time."));
        QVERIFY(!pgxl->isConnected());
        QCOMPARE(commandsOf(frames), QStringList{QStringLiteral("info")});
    }

    // Disabling or disconnecting in any phase never redials the old address.
    void cancelInEveryPhaseNeverRedials_data()
    {
        QTest::addColumn<QString>("phase");
        QTest::addColumn<bool>("switchOff");
        for (const char* phase : {"connecting", "identifying", "connected", "retrying"}) {
            QTest::newRow(QByteArray(phase).append("-disconnect").constData())
                << QString::fromLatin1(phase) << false;
            QTest::newRow(QByteArray(phase).append("-switch-off").constData())
                << QString::fromLatin1(phase) << true;
        }
    }
    void cancelInEveryPhaseNeverRedials()
    {
        QFETCH(QString, phase);
        QFETCH(bool, switchOff);
        RadioModel model;
        prepare(model);
        auto* pgxl = model.pgxlConnection();
        pgxl->testSetReconnectBackoffUnitMs(100);
        QSignalSpy frames(pgxl, &PgxlConnection::testFrameWrittenForTesting);
        QString reason;
        QTcpServer amp;
        quint16 port = 0;
        if (phase == QStringLiteral("retrying")) {
            port = closedLoopbackPort();
            QVERIFY(port != 0);
            QVERIFY(model.configurePgxlForStation(QStringLiteral("127.0.0.1"), port, &reason));
            QTRY_COMPARE(model.amplifierModel()->connectionPhase(), Phase::Retrying);
            QVERIFY(pgxl->testReconnectPending());
        } else {
            QVERIFY(amp.listen(QHostAddress::LocalHost, 0));
            port = amp.serverPort();
            if (phase == QStringLiteral("connecting")) {
                QVERIFY(model.configurePgxlForStation(QStringLiteral("127.0.0.1"), port, &reason));
                QCOMPARE(model.amplifierModel()->connectionPhase(), Phase::Connecting);
            } else {
                QTcpSocket* peer = answerUpToInfo(model, amp, frames, "V3.8.9", kInfoReply);
                QVERIFY(peer);
                if (phase == QStringLiteral("connected")) {
                    announce(model, port, QStringLiteral("PowerGeniusXL"),
                             QString::fromLatin1(kSerial));
                    QTRY_VERIFY(pgxl->isConnected());
                } else {
                    QCOMPARE(model.amplifierModel()->connectionPhase(), Phase::Identifying);
                }
            }
        }

        if (switchOff) {
            QVERIFY(model.setFourO3AEnabledForStation(false, &reason));
            QCOMPARE(model.amplifierModel()->connectionPhase(), Phase::Disabled);
        } else {
            QVERIFY(model.disconnectPgxlForStation(&reason));
            QCOMPARE(model.amplifierModel()->connectionPhase(), Phase::Disconnected);
        }
        QVERIFY(!pgxl->testReconnectPending());
        QVERIFY(!pgxl->isConnected());

        // Whatever the old address was, it is never dialled again.
        amp.close();
        QTcpServer old;
        QVERIFY(old.listen(QHostAddress::LocalHost, port));
        QSignalSpy redials(&old, &QTcpServer::newConnection);
        QTest::qWait(700);
        QCOMPARE(redials.count(), 0);
        QVERIFY(!pgxl->testReconnectPending());
        QVERIFY(model.amplifierModel()->connectionPhase() == Phase::Disabled
                || model.amplifierModel()->connectionPhase() == Phase::Disconnected);
    }

    // Replacing A with B while A is being identified leaves only B: A's late
    // reply cannot act, A is not redialled, B is admitted.
    void replacingPendingAWithBLeavesOnlyB()
    {
        QTcpServer ampA;
        QTcpServer ampB;
        QVERIFY(ampA.listen(QHostAddress::LocalHost, 0));
        QVERIFY(ampB.listen(QHostAddress::LocalHost, 0));
        RadioModel model;
        prepare(model);
        auto* pgxl = model.pgxlConnection();
        QSignalSpy frames(pgxl, &PgxlConnection::testFrameWrittenForTesting);
        QSignalSpy connectionsA(&ampA, &QTcpServer::newConnection);

        // A: V answered, its info reply held back.
        QString reason;
        QVERIFY(model.configurePgxlForStation(QStringLiteral("127.0.0.1"), ampA.serverPort(), &reason));
        QTRY_VERIFY(ampA.hasPendingConnections());
        QPointer<QTcpSocket> peerA = ampA.nextPendingConnection();
        peerA->write("V3.8.9\n");
        peerA->flush();
        QTRY_VERIFY(sequenceOf(frames, QStringLiteral("info$")) != 0);
        const quint32 infoA = sequenceOf(frames, QStringLiteral("info$"));
        QCOMPARE(model.amplifierModel()->connectionPhase(), Phase::Identifying);

        // B replaces A.
        frames.clear();
        QTcpSocket* peerB = answerUpToInfo(model, ampB, frames, "V3.8.9", kInfoReply);
        QVERIFY(peerB);
        if (peerA) {
            peerA->write(QStringLiteral("R%1|0|%2\n").arg(infoA)
                             .arg(QString::fromLatin1(kInfoReply)).toUtf8());
            peerA->flush();
        }
        announce(model, ampA.serverPort(), QStringLiteral("PowerGeniusXL"),
                 QString::fromLatin1(kSerial));
        QTest::qWait(100);
        QVERIFY(!pgxl->isConnected());
        announce(model, ampB.serverPort(), QStringLiteral("PowerGeniusXL"),
                 QString::fromLatin1(kSerial));
        QTRY_VERIFY(pgxl->isConnected());
        QCOMPARE(pgxl->peerPort(), ampB.serverPort());
        QCOMPARE(model.amplifierModel()->configuredPort(), int(ampB.serverPort()));
        QCOMPARE(model.peripheralValue(QStringLiteral("PGXL_ManualPort")),
                 QString::number(ampB.serverPort()));
        QTest::qWait(300);
        QCOMPARE(connectionsA.count(), 1);
        QVERIFY(pgxl->isConnected());
        QVERIFY(model.disconnectPgxlForStation(&reason));
    }

    // Connection settings are saved on the Core and applied to the running
    // connection; a bad value changes nothing.
    void connectionSettingsAreSavedAndApplied()
    {
        const quint16 deadPort = closedLoopbackPort();
        QVERIFY(deadPort != 0);
        RadioModel model;
        prepare(model);
        auto* pgxl = model.pgxlConnection();
        pgxl->testSetReconnectBackoffUnitMs(100);
        QString reason;
        QVERIFY(!model.setPgxlConnectionSettingsForStation(true, 0, 10, &reason));
        QVERIFY(!model.setPgxlConnectionSettingsForStation(true, 30, -1, &reason));
        QVERIFY(!model.setPgxlConnectionSettingsForStation(true, 30, 3601, &reason));
        QVERIFY(AppSettings::instance().value(QStringLiteral("PGXL_KeepaliveSec")).isNull());

        QVERIFY(model.configurePgxlForStation(QStringLiteral("127.0.0.1"), deadPort, &reason));
        QTRY_COMPARE(model.amplifierModel()->connectionPhase(), Phase::Retrying);
        QVERIFY(model.setPgxlConnectionSettingsForStation(false, 45, 20, &reason));
        QVERIFY(reason.isEmpty());
        auto& s = AppSettings::instance();
        QCOMPARE(s.value(QStringLiteral("PGXL_AutoReconnect")).toString(), QStringLiteral("False"));
        QCOMPARE(s.value(QStringLiteral("PGXL_KeepaliveSec")).toString(), QStringLiteral("45"));
        QCOMPARE(s.value(QStringLiteral("PGXL_PingSec")).toString(), QStringLiteral("20"));
        QCOMPARE(pgxl->autoPingIntervalSec(), 20);
        // Automatic retry off: the pending retry is gone and the phase says so.
        QVERIFY(!pgxl->testReconnectPending());
        QCOMPARE(model.amplifierModel()->connectionPhase(), Phase::Disconnected);
        QVERIFY(model.disconnectPgxlForStation(&reason));
    }

    // A radio's saved address on the Core: dialled through the identity
    // check at radio connect, and a new radio's scope retires the old one.
    void radioScopeDialsThroughIdentityCheck()
    {
        QTcpServer amp;
        QVERIFY(amp.listen(QHostAddress::LocalHost, 0));
        const QString mac = QStringLiteral("aa:bb:cc:dd:ee:72");
        auto& settings = AppSettings::instance();
        settings.setHardwareValue(mac, QStringLiteral("peripherals/FourO3A_Enabled"),
                                  QStringLiteral("True"));
        settings.setHardwareValue(mac, QStringLiteral("peripherals/PGXL_ManualIp"),
                                  QStringLiteral("127.0.0.1"));
        settings.setHardwareValue(mac, QStringLiteral("peripherals/PGXL_ManualPort"),
                                  QString::number(amp.serverPort()));
        RadioModel model;
        model.enableStationAccessoryIdentity();
        model.setReceiveOnlyStationPolicy(true);
        model.smartSdrListener()->setListenEndpointForTesting(QHostAddress::LocalHost, 0);
        RadioInfo radio;
        radio.macAddress = mac;
        model.setLastRadioInfoForTest(radio);
        model.setConnectionStateForTest(ConnectionState::Connected);
        auto* pgxl = model.pgxlConnection();
        QSignalSpy frames(pgxl, &PgxlConnection::testFrameWrittenForTesting);
        model.applyPeripheralsForTest();
        QCOMPARE(model.amplifierModel()->configuredHost(), QStringLiteral("127.0.0.1"));
        QTRY_VERIFY(amp.hasPendingConnections());
        QTcpSocket* peer = amp.nextPendingConnection();
        peer->write("V3.8.9\n");
        peer->flush();
        QTRY_COMPARE(model.amplifierModel()->connectionPhase(), Phase::Identifying);
        QVERIFY(!pgxl->isConnected());
        QCOMPARE(commandsOf(frames), QStringList{QStringLiteral("info")});

        model.setConnectionStateForTest(ConnectionState::Disconnected);
        model.teardownPeripheralsForTest();
        QCOMPARE(model.amplifierModel()->connectionPhase(), Phase::Disconnected);
        QVERIFY(!pgxl->testReconnectPending());
    }
};

QTEST_GUILESS_MAIN(StationPgxlControllerTest)
#include "tst_station_pgxl_controller.moc"
