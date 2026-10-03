// no-port-check: NereusSDR-original. Task 42 accessory transmit commands.
#include <QtTest>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <algorithm>

#include "MultiDeviceHarness.h"
#include "core/LanDiscovery.h"
#include "core/PgxlConnection.h"
#include "core/StationPgxlController.h"
#include "core/TxInterlockPolicy.h"
#include "core/session/SessionCommandDispatcher.h"

using namespace NereusSDR;

class AccessoryTxCommandsTest : public QObject {
    Q_OBJECT
private slots:
    void everyVerbHasTheTask42Capability()
    {
        const QList<QByteArray> verbs{"amp.operate", "amp.standby", "tuner.tune",
                                      "tuner.operate", "tuner.bypass", "tuner.antenna",
                                      "rfkit.operate", "rfkit.standby", "rfkit.antenna"};
        for (const QByteArray& verb : verbs) {
            const auto it = std::find_if(SessionCommandDispatcher::verbSpecs().cbegin(),
                                         SessionCommandDispatcher::verbSpecs().cend(),
                                         [&verb](const CommandVerbSpec& spec) {
                                             return spec.verb == verb;
                                         });
            QVERIFY2(it != SessionCommandDispatcher::verbSpecs().cend(), verb.constData());
            QCOMPARE(it->capability, QByteArray("accessoryTxVersion"));
            QCOMPARE(it->capabilityVersion, 1);
        }
    }

    void missingSessionPermissionRefusesBeforeAnyAccessoryAction()
    {
        RadioModel model;
        SessionCommandDispatcher dispatcher(&model);
        QList<SessionMessage> answers;
        QObject::connect(&dispatcher, &SessionCommandDispatcher::commandResultReady,
                         &dispatcher, [&answers](const SessionMessage& message) {
                             answers.append(message);
                         });
        dispatcher.setRequester("phone");
        // The gate is absent here, so no accessory operation is authorized.
        dispatcher.dispatch(SessionMessages::commandInvoke("amp.operate", 1, {}));
        QCOMPARE(answers.size(), 1);
        QVERIFY(!answers.first().accepted);
        QVERIFY(!answers.first().reason.isEmpty());
        QVERIFY(std::any_of(answers.first().updates.cbegin(), answers.first().updates.cend(),
                            [](const MirrorUpdate& update) {
                                return update.name == "refusalCode";
                            }));
    }

    void idleHolderIsAskedBeforeAnotherDeviceSwitchesTheAmp()
    {
        Core core(true);
        core.model->enableStationAccessoryIdentity();
        Device a(QStringLiteral("iPhone"), QStringLiteral("phone"));
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        allowTransmit(core);
        LoopbackTransport* appA = core.signIn(a, kTransmitter);
        LoopbackTransport* appB = core.signIn(b, kTransmitter);
        QVERIFY(admitted(appA));
        QVERIFY(admitted(appB));
        QVERIFY(capability(appB->received(), QStringLiteral("accessoryTxVersion")).has_value());
        QCOMPARE(*capability(appB->received(), QStringLiteral("accessoryTxVersion")), qint64(1));
        const QJsonObject take = core.invoke(appA, "tx.take");
        QVERIFY2(take.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(take.value(QStringLiteral("reason")).toString()));
        const QJsonObject change = core.invoke(appB, "amp.operate");
        QVERIFY(!change.value(QStringLiteral("accepted")).toBool());
        QCOMPARE(change.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("Waiting for you to confirm."));
        const QJsonObject question = firstOfType(appB->received(), QStringLiteral("confirm.request"));
        QVERIFY(!question.isEmpty());
    }

    void oldAndNewAccessoryVerbsRequireRemoteTransmitPermission()
    {
        Core core(true);
        core.model->enableStationAccessoryIdentity();
        Device app(QStringLiteral("iPhone"), QStringLiteral("phone"));
        core.pair(app);
        LoopbackTransport* peer = core.signIn(app, kTransmitter);
        QVERIFY(admitted(peer));
        const QList<MirrorUpdate> on{{0, "on", MirrorWireKind::Bool, true}};
        const QList<MirrorUpdate> port{{0, "port", MirrorWireKind::Int64, qint64(1)}};
        const QList<QPair<QByteArray, QList<MirrorUpdate>>> commands{
            {"amp.operate", {}}, {"setPgxlOperate", on},
            {"tuner.operate", on}, {"setTgxlOperate", on},
            {"tuner.bypass", on}, {"setTgxlBypass", on},
            {"tuner.antenna", port}, {"setTgxlAntenna", port},
            {"rfkit.operate", {}}, {"setRfKitOperate", on},
            {"rfkit.antenna", port}, {"setRfKitAntenna", port}};
        for (const auto& [verb, args] : commands) {
            const QJsonObject result = core.invoke(peer, verb, args);
            QVERIFY2(!result.value(QStringLiteral("accepted")).toBool(), verb.constData());
            QCOMPARE(result.value(QStringLiteral("reason")).toString(),
                     QStringLiteral("This Core is set to receive only."));
            if (!verb.contains('.')) {
                QVERIFY(result.value(QStringLiteral("values")).toArray().isEmpty());
            }
        }
    }

    void legacyAliasesReachAccessoryChecksWhenRemoteTransmitIsAllowed()
    {
        Core core(true);
        core.model->enableStationAccessoryIdentity();
        Device app(QStringLiteral("iPhone"), QStringLiteral("phone"));
        core.pair(app);
        allowTransmit(core);
        LoopbackTransport* peer = core.signIn(app, kTransmitter);
        QVERIFY(admitted(peer));
        const QList<MirrorUpdate> on{{0, "on", MirrorWireKind::Bool, true}};
        const QList<MirrorUpdate> port{{0, "port", MirrorWireKind::Int64, qint64(1)}};
        const QList<QPair<QByteArray, QList<MirrorUpdate>>> commands{
            {"setPgxlOperate", on}, {"setTgxlOperate", on},
            {"setTgxlBypass", on}, {"setTgxlAntenna", port},
            {"setRfKitOperate", on}, {"setRfKitAntenna", port}};
        for (const auto& [verb, args] : commands) {
            const QJsonObject result = core.invoke(peer, verb, args);
            QVERIFY2(!result.value(QStringLiteral("accepted")).toBool(), verb.constData());
            QVERIFY2(result.value(QStringLiteral("reason")).toString()
                         != QStringLiteral("This Core is set to receive only."), verb.constData());
        }
    }

    void anOnAirHolderRefusesAnotherDevicesSwitch()
    {
        Core core(true);
        core.model->enableStationAccessoryIdentity();
        Device a(QStringLiteral("iPhone"), QStringLiteral("phone"));
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        allowTransmit(core);
        LoopbackTransport* holder = core.signIn(a, kTransmitter);
        LoopbackTransport* other = core.signIn(b, kTransmitter);
        QVERIFY(admitted(holder) && admitted(other));
        const QJsonObject key = core.invoke(holder, "tx.key",
                                            {MirrorUpdate{0, "trigger", MirrorWireKind::Utf8,
                                                          QStringLiteral("screen")}});
        QVERIFY2(key.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(key.value(QStringLiteral("reason")).toString()));
        const QJsonObject refused = core.invoke(other, "amp.operate");
        QVERIFY(!refused.value(QStringLiteral("accepted")).toBool());
        QVERIFY(refused.value(QStringLiteral("reason")).toString().contains(
            QStringLiteral("on the air")));
        const QJsonObject oldRefused = core.invoke(
            other, "setPgxlOperate", {{0, "on", MirrorWireKind::Bool, true}});
        QVERIFY(!oldRefused.value(QStringLiteral("accepted")).toBool());
        QVERIFY(oldRefused.value(QStringLiteral("reason")).toString().contains(
            QStringLiteral("on the air")));
        QVERIFY(ofType(other->received(), QStringLiteral("confirm.request")).isEmpty());
    }

    void receiveOnlyEnabledBeforeProceedRefusesHeldSwitch()
    {
        Core core(true);
        core.model->enableStationAccessoryIdentity();
        Device a(QStringLiteral("iPhone"), QStringLiteral("phone"));
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        allowTransmit(core);
        LoopbackTransport* holder = core.signIn(a, kTransmitter);
        LoopbackTransport* other = core.signIn(b, kTransmitter);
        QVERIFY(admitted(holder) && admitted(other));
        QVERIFY(core.invoke(holder, "tx.take").value(QStringLiteral("accepted")).toBool());
        QCOMPARE(core.invoke(other, "amp.operate").value(QStringLiteral("reason")).toString(),
                 QStringLiteral("Waiting for you to confirm."));
        const QJsonObject question = firstOfType(other->received(), QStringLiteral("confirm.request"));
        QVERIFY(!question.isEmpty());
        core.server->setRemoteTransmitAllowed(false);
        const QJsonObject refused = core.invoke(
            other, "confirm.proceed",
            {MirrorUpdate{0, "id", MirrorWireKind::Int64,
                          question.value(QStringLiteral("id")).toInteger()},
             MirrorUpdate{0, "choice", MirrorWireKind::Int64, qint64(-1)}});
        QVERIFY(!refused.value(QStringLiteral("accepted")).toBool());
        QCOMPARE(refused.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("This Core is set to receive only."));
    }

    void tunerTuneWithoutAConnectedTunerDoesNotTakeOrKeyTransmit()
    {
        Core core(true);
        core.model->enableStationAccessoryIdentity();
        Device app(QStringLiteral("iPhone"), QStringLiteral("phone"));
        core.pair(app);
        allowTransmit(core);
        LoopbackTransport* peer = core.signIn(app, kTransmitter);
        QVERIFY(admitted(peer));
        QVERIFY(!core.server->transmitHolder()->holder().has_value());
        const QJsonObject refused = core.invoke(peer, "tuner.tune");
        QVERIFY(!refused.value(QStringLiteral("accepted")).toBool());
        QCOMPARE(refused.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("No Tuner Genius is connected to the Core."));
        QVERIFY(!core.server->transmitHolder()->holder().has_value());
        QVERIFY(!core.model->moxController()->isMox());
    }

    void holderKeyingAfterQuestionRefusesProceed()
    {
        Core core(true);
        core.model->enableStationAccessoryIdentity();
        Device a(QStringLiteral("iPhone"), QStringLiteral("phone"));
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        allowTransmit(core);
        LoopbackTransport* holder = core.signIn(a, kTransmitter);
        LoopbackTransport* other = core.signIn(b, kTransmitter);
        QVERIFY(admitted(holder) && admitted(other));
        QVERIFY(core.invoke(holder, "tx.take").value(QStringLiteral("accepted")).toBool());
        QCOMPARE(core.invoke(other, "amp.operate").value(QStringLiteral("reason")).toString(),
                 QStringLiteral("Waiting for you to confirm."));
        const QJsonObject question = firstOfType(other->received(), QStringLiteral("confirm.request"));
        QVERIFY(!question.isEmpty());
        QVERIFY(core.invoke(holder, "tx.key",
                            {{0, "trigger", MirrorWireKind::Utf8, QStringLiteral("screen")}})
                    .value(QStringLiteral("accepted")).toBool());
        const QJsonObject refused = core.invoke(
            other, "confirm.proceed",
            {{0, "id", MirrorWireKind::Int64, question.value(QStringLiteral("id")).toInteger()},
             {0, "choice", MirrorWireKind::Int64, qint64(-1)}});
        QVERIFY(!refused.value(QStringLiteral("accepted")).toBool());
        QVERIFY(refused.value(QStringLiteral("reason")).toString().contains(
            QStringLiteral("on the air")));
    }

    void remoteOperateRepairsAnAmpStandbyBlockWithoutKeying()
    {
        // All accessory traffic stays on a localhost TCP fake. The policy
        // check is the same one MoxController uses, asked without a key.
        QTcpServer fakeAmp;
        QVERIFY(fakeAmp.listen(QHostAddress::LocalHost, 0));
        Core core(true);
        core.model->enableStationAccessoryIdentity();
        core.model->setPeripheralValue(QStringLiteral("FourO3A_Enabled"),
                                       QStringLiteral("True"));
        core.model->setPgxlLanScanWindowMsForTest(150);
        QSignalSpy frames(core.model->pgxlConnection(),
                          &PgxlConnection::testFrameWrittenForTesting);
        QString reason;
        QVERIFY2(core.model->configurePgxlForStation(QStringLiteral("127.0.0.1"),
                                                     fakeAmp.serverPort(), &reason),
                 qPrintable(reason));
        QTRY_VERIFY_WITH_TIMEOUT(fakeAmp.hasPendingConnections(), 2000);
        QTcpSocket* peer = fakeAmp.nextPendingConnection();
        QVERIFY(peer);
        peer->write("V3.8.9\n");
        peer->flush();
        const auto infoSequence = [&frames]() -> quint32 {
            const QRegularExpression info(QStringLiteral("^C(\\d+)\\|info$"));
            for (const auto& row : frames) {
                const QRegularExpressionMatch match = info.match(row.first().toString());
                if (match.hasMatch()) { return match.captured(1).toUInt(); }
            }
            return 0;
        };
        QTRY_VERIFY_WITH_TIMEOUT(infoSequence() != 0, 2000);
        peer->write(QStringLiteral("R%1|0|serial=10-200/24-0046  version=3.8.9 "
                                   "protocol=1.0 mains=240\n")
                        .arg(infoSequence()).toUtf8());
        peer->flush();
        StationPgxlController* controller = core.model->findChild<StationPgxlController*>();
        QVERIFY(controller);
        LanDiscovery* discovery = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT((discovery = controller->findChild<LanDiscovery*>()) != nullptr,
                                 2000);
        discovery->injectDatagramForTesting(
            QStringLiteral("PowerGeniusXL ip=127.0.0.1 v=3.8.9 "
                           "serial=10-200/24-0046 nickname=PowerGeniusXL"),
            fakeAmp.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(core.model->pgxlConnection()->isConnected(), 2000);
        peer->write("S0|status state=STANDBY\n");
        peer->flush();
        QTRY_VERIFY(core.model->hasAmplifier());
        QVERIFY(!core.model->ampOperate());

        allowTransmit(core);
        TxInterlockPolicy* policy = core.model->txInterlockPolicy();
        policy->setMode(TxInterlockPolicy::Block);
        QVERIFY(!policy->evaluateTxRequest(core.model->hasAmplifier(),
                                           core.model->ampOperate(), 1.0f));
        QCOMPARE(policy->lastDenial(), TxInterlockPolicy::Denial::AmpStandby);

        Device app(QStringLiteral("iPhone"), QStringLiteral("phone"));
        core.pair(app);
        LoopbackTransport* session = core.signIn(app, kTransmitter);
        QVERIFY(admitted(session));
        const QJsonObject switched = core.invoke(session, "amp.operate");
        QVERIFY2(switched.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(switched.value(QStringLiteral("reason")).toString()));
        QByteArray received;
        QTRY_VERIFY_WITH_TIMEOUT((received += peer->readAll()).contains("operate=1\n"), 2000);
        QVERIFY(!core.model->ampOperate()); // The request is not the readback.
        peer->write("S0|status state=OPERATE\n");
        peer->flush();
        QTRY_VERIFY(core.model->ampOperate());
        QVERIFY(policy->evaluateTxRequest(core.model->hasAmplifier(),
                                          core.model->ampOperate(), 1.0f));
        QCOMPARE(policy->lastDenial(), TxInterlockPolicy::Denial::None);
        QVERIFY(!core.model->moxController()->isMox());
        QVERIFY(!core.server->transmitHolder()->holder().has_value());
    }
};

QTEST_GUILESS_MAIN(AccessoryTxCommandsTest)
#include "tst_accessory_tx_commands.moc"
