// no-port-check: NereusSDR-original.
// Fifth-device admission over an in-process Core; no radio or RF.
#include <QtTest>

#include "MultiDeviceHarness.h"

using namespace NereusSDR;
using namespace NereusSDR::Test;

class TstStationFifthDevice : public QObject {
    Q_OBJECT
private slots:
    void heldPeerIsIsolatedAndCanCancel()
    {
        Core core;
        Device devices[5];
        for (Device& device : devices) core.pair(device);
        for (int i = 0; i < 4; ++i) QVERIFY(admitted(core.signIn(devices[i])));
        LoopbackTransport* fifth = core.signIn(devices[4]);
        QJsonObject held;
        verifyHeld(fifth, &held);
        // Even a well-formed command is a protocol violation while held.
        fifth->sendText(SessionMessages::encode(
            SessionMessages::commandInvoke("session.leave", 7, {})));
        const QJsonObject end = endOf(fifth);
        QCOMPARE(end.value(QStringLiteral("code")).toString(), QStringLiteral("protocolError"));
        QCOMPARE(core.sessions().placesTaken(), 4);

        LoopbackTransport* again = core.signIn(devices[4]);
        QJsonObject question;
        verifyHeld(again, &question);
        again->sendText(SessionMessages::encode(SessionMessages::sessionTakeover(
            QString(), static_cast<quint32>(question.value(QStringLiteral("revision")).toInteger()))));
        QCOMPARE(endOf(again).value(QStringLiteral("code")).toString(), QStringLiteral("coreFull"));
        QCOMPARE(core.sessions().placesTaken(), 4);
        QVERIFY(held.value(QStringLiteral("revision")).isDouble());
    }

    void staleAnswerRetainsOriginalDeadline()
    {
        Core core;
        Device devices[5];
        for (Device& device : devices) core.pair(device);
        for (int i = 0; i < 4; ++i) QVERIFY(admitted(core.signIn(devices[i])));
        LoopbackTransport* fifth = core.signIn(devices[4]);
        QJsonObject held;
        verifyHeld(fifth, &held);
        const QString target = held.value(QStringLiteral("devices")).toArray().first()
                                   .toObject().value(QStringLiteral("deviceId")).toString();
        fifth->sendText(SessionMessages::encode(
            SessionMessages::sessionTakeover(target, 0)));
        QTRY_VERIFY(ofType(fifth->received(), QStringLiteral("session.held")).size() >= 2);
        QCOMPARE(core.sessions().placesTaken(), 4);
        core.now = 60000;
        fifth->sendText(SessionMessages::encode(SessionMessages::sessionTakeover(target,
            static_cast<quint32>(held.value(QStringLiteral("revision")).toInteger()))));
        QCOMPARE(endOf(fifth).value(QStringLiteral("code")).toString(), QStringLiteral("coreFull"));
    }

    void unheldIncumbentIsReplacedBeforeTakerAdmission()
    {
        Core core;
        Device devices[5];
        for (Device& device : devices) core.pair(device);
        QList<LoopbackTransport*> first;
        for (int i = 0; i < 4; ++i) {
            first.append(core.signIn(devices[i]));
            QVERIFY(admitted(first.last()));
        }
        LoopbackTransport* fifth = core.signIn(devices[4]);
        QJsonObject held;
        verifyHeld(fifth, &held);
        const QJsonObject target = held.value(QStringLiteral("devices")).toArray().first().toObject();
        const QString id = target.value(QStringLiteral("deviceId")).toString();
        const quint32 revision = static_cast<quint32>(held.value(QStringLiteral("revision")).toInteger());
        fifth->sendText(SessionMessages::encode(SessionMessages::sessionTakeover(id, revision)));
        QTRY_VERIFY(admitted(fifth));
        QCOMPARE(core.sessions().placesTaken(), 4);
        QVERIFY(!core.sessions().entry(StationIdentity::fromBase64Url(id)));
        QVERIFY(core.sessions().entry(devices[4].key.fingerprint()));
        const QJsonObject end = endOf(first.first());
        QCOMPARE(end.value(QStringLiteral("code")).toString(), QStringLiteral("takenOver"));
        QCOMPARE(end.value(QStringLiteral("takenOverById")).toString(), devices[4].id());
        QCOMPARE(end.value(QStringLiteral("secondsAgo")).toInteger(), 0);
    }

    void awayIncumbentIsOfferedFirstAndReplaced()
    {
        Core core;
        Device devices[5];
        for (Device& device : devices) core.pair(device);
        QList<LoopbackTransport*> first;
        for (int i = 0; i < 4; ++i) {
            first.append(core.signIn(devices[i]));
            QVERIFY(admitted(first.last()));
        }
        first.at(1)->closeLink(QStringLiteral("went away"));
        QTRY_COMPARE(core.sessions().entry(devices[1].key.fingerprint())->state,
                     DeviceSessionRegistry::State::Away);
        LoopbackTransport* fifth = core.signIn(devices[4]);
        QJsonObject held;
        verifyHeld(fifth, &held);
        const QJsonObject offered = held.value(QStringLiteral("devices")).toArray().first().toObject();
        QCOMPARE(offered.value(QStringLiteral("deviceId")).toString(), devices[1].id());
        fifth->sendText(SessionMessages::encode(SessionMessages::sessionTakeover(
            devices[1].id(), static_cast<quint32>(held.value(QStringLiteral("revision")).toInteger()))));
        QTRY_VERIFY(admitted(fifth));
        QVERIFY(!core.sessions().entry(devices[1].key.fingerprint()));
        QCOMPARE(core.sessions().placesTaken(), 4);
    }

    void expiredPlaceIsReportedToItsReturningDeviceAtFullCapacity()
    {
        Core core;
        Device devices[5];
        for (Device& device : devices) core.pair(device);
        QList<LoopbackTransport*> first;
        for (int i = 0; i < 4; ++i) {
            first.append(core.signIn(devices[i]));
            QVERIFY(admitted(first.last()));
        }
        first.first()->closeLink(QStringLiteral("went away"));
        QTRY_COMPARE(core.sessions().entry(devices[0].key.fingerprint())->state,
                     DeviceSessionRegistry::State::Away);
        core.now = 180000;
        QCOMPARE(core.sessions().expireAway().size(), 1);
        QVERIFY(admitted(core.signIn(devices[4])));
        LoopbackTransport* returning = core.signIn(devices[0]);
        QJsonObject held;
        verifyHeld(returning, &held);
        QCOMPARE(held.value(QStringLiteral("placeFreed")).toObject()
                     .value(QStringLiteral("secondsAgo")).toInteger(), 0);
        QCOMPARE(core.sessions().placesTaken(), 4);
    }

    void noAnswerExpiresAtOriginalSixtySecondDeadline()
    {
        Core core;
        Device devices[5];
        for (Device& device : devices) core.pair(device);
        for (int i = 0; i < 4; ++i) QVERIFY(admitted(core.signIn(devices[i])));
        LoopbackTransport* fifth = core.signIn(devices[4]);
        verifyHeld(fifth);
        QTimer* answerTimer = nullptr;
        for (QTimer* timer : core.server->findChildren<QTimer*>()) {
            if (timer->isSingleShot() && timer->interval() == 60000) {
                answerTimer = timer;
                break;
            }
        }
        QVERIFY(answerTimer);
        core.now = 59999;
        QVERIFY(fifth->isOpen());
        core.now = 60000;
        QVERIFY(QMetaObject::invokeMethod(answerTimer, "timeout", Qt::DirectConnection));
        QCOMPARE(endOf(fifth).value(QStringLiteral("code")).toString(), QStringLiteral("coreFull"));
        QCOMPARE(core.sessions().placesTaken(), 4);
    }

    void directSignalsCannotKeyDuringCutover()
    {
        Core core;
        Device devices[5];
        for (Device& device : devices) core.pair(device);
        for (int i = 0; i < 4; ++i) QVERIFY(admitted(core.signIn(devices[i])));
        LoopbackTransport* fifth = core.signIn(devices[4]);
        QJsonObject held;
        verifyHeld(fifth, &held);
        const QString id = held.value(QStringLiteral("devices")).toArray().first()
                               .toObject().value(QStringLiteral("deviceId")).toString();
        const QByteArray targetId = StationIdentity::fromBase64Url(id);
        int sliceAttempts = 0;
        int registryAttempts = 0;
        auto* holder = core.server->transmitHolder();
        auto tryKey = [&]() {
            const KeyingAnswer answer = holder->askKey(
                {QByteArrayLiteral("reentrant"), TransmitHolder::Source::Device, false, false});
            QCOMPARE(answer.verdict, KeyingVerdict::Refuse);
            bool transferred = true;
            holder->transferTo(TransmitHolder::Holder{QByteArrayLiteral("reentrant")},
                               QStringLiteral("reentrant"), [&](bool ok) { transferred = ok; });
            QVERIFY(!transferred);
        };
        const auto sliceConnection = QObject::connect(core.model.get(), &RadioModel::sliceRemoved,
                         core.server.get(), [&](int) {
            ++sliceAttempts;
            tryKey();
        }, Qt::DirectConnection);
        const auto registryConnection = QObject::connect(core.server->deviceSessions(), &DeviceSessionRegistry::changed,
                         core.server.get(), [&]() {
            if (!core.sessions().entry(targetId)) { ++registryAttempts; tryKey(); }
        }, Qt::DirectConnection);
        auto disconnectSignals = qScopeGuard([&]() {
            QObject::disconnect(registryConnection);
            QObject::disconnect(sliceConnection);
        });
        fifth->sendText(SessionMessages::encode(SessionMessages::sessionTakeover(
            id, static_cast<quint32>(held.value(QStringLiteral("revision")).toInteger()))));
        QTRY_VERIFY(admitted(fifth));
        QVERIFY(sliceAttempts > 0);
        QVERIFY(registryAttempts > 0);
        QCOMPARE(core.sessions().placesTaken(), 4);
    }

    void aHolderChangedCallbackCannotCauseUnsafeReplacement()
    {
        Core core;
        Device devices[5];
        for (Device& device : devices) core.pair(device);
        for (int i = 0; i < 4; ++i) QVERIFY(admitted(core.signIn(devices[i])));
        auto* holder = core.server->transmitHolder();
        QCOMPARE(holder->askKey({devices[0].key.fingerprint(),
                                 TransmitHolder::Source::Device, false, false}).verdict,
                 KeyingVerdict::Admit);
        LoopbackTransport* fifth = core.signIn(devices[4]);
        QJsonObject held;
        verifyHeld(fifth, &held);
        bool reentered = false;
        const auto holderConnection = QObject::connect(holder, &TransmitHolder::changed, core.server.get(), [&]() {
            if (!reentered && holder->state() == TransmitHolder::State::Unheld) {
                reentered = true;
                QCOMPARE(holder->askKey({devices[0].key.fingerprint(),
                                         TransmitHolder::Source::Device, false, false}).verdict,
                         KeyingVerdict::Admit);
            }
        }, Qt::DirectConnection);
        auto disconnectHolder = qScopeGuard([&]() { QObject::disconnect(holderConnection); });
        fifth->sendText(SessionMessages::encode(SessionMessages::sessionTakeover(
            devices[0].id(), static_cast<quint32>(held.value(QStringLiteral("revision")).toInteger()))));
        QTRY_VERIFY(reentered);
        QVERIFY(core.sessions().entry(devices[0].key.fingerprint()));
        QVERIFY(!admitted(fifth));
        QCOMPARE(core.sessions().placesTaken(), 4);
    }

    void reentrantSignInWaitsForReservedCutover()
    {
        Core core;
        Device devices[6];
        for (Device& device : devices) core.pair(device);
        for (int i = 0; i < 4; ++i) QVERIFY(admitted(core.signIn(devices[i])));
        LoopbackTransport* fifth = core.signIn(devices[4]);
        QJsonObject held;
        verifyHeld(fifth, &held);
        const QString target = held.value(QStringLiteral("devices")).toArray().first()
                                   .toObject().value(QStringLiteral("deviceId")).toString();
        const QByteArray targetId = StationIdentity::fromBase64Url(target);
        LoopbackTransport* sixth = nullptr;
        const auto connection = QObject::connect(core.server->deviceSessions(),
            &DeviceSessionRegistry::changed, core.server.get(), [&]() {
                if (sixth || core.sessions().entry(targetId)) return;
                sixth = core.signIn(devices[5]);
                QVERIFY(ofType(sixth->received(), QStringLiteral("auth.result")).isEmpty());
                QVERIFY(!admitted(sixth));
            }, Qt::DirectConnection);
        auto disconnect = qScopeGuard([&]() { QObject::disconnect(connection); });
        fifth->sendText(SessionMessages::encode(SessionMessages::sessionTakeover(
            target, static_cast<quint32>(held.value(QStringLiteral("revision")).toInteger()))));
        QTRY_VERIFY(admitted(fifth));
        QVERIFY(sixth);
        verifyHeld(sixth);
        QCOMPARE(core.sessions().placesTaken(), 4);
    }

    void takerDisconnectAtFirstSliceRemovalStillSettlesIncumbent()
    {
        Core core;
        Device devices[5];
        for (Device& device : devices) core.pair(device);
        QList<LoopbackTransport*> first;
        for (int i = 0; i < 4; ++i) {
            first.append(core.signIn(devices[i]));
            QVERIFY(admitted(first.last()));
        }
        LoopbackTransport* fifth = core.signIn(devices[4]);
        QJsonObject held;
        verifyHeld(fifth, &held);
        const QString target = held.value(QStringLiteral("devices")).toArray().first()
                                   .toObject().value(QStringLiteral("deviceId")).toString();
        bool dropped = false;
        const auto connection = QObject::connect(core.model.get(), &RadioModel::sliceRemoved,
            core.server.get(), [&](int) {
                if (dropped) return;
                dropped = true;
                fifth->peerForTest()->closeLink(QStringLiteral("test taker left"));
            }, Qt::DirectConnection);
        auto disconnect = qScopeGuard([&]() { QObject::disconnect(connection); });
        fifth->sendText(SessionMessages::encode(SessionMessages::sessionTakeover(
            target, static_cast<quint32>(held.value(QStringLiteral("revision")).toInteger()))));
        QTRY_VERIFY(dropped);
        QVERIFY(!core.sessions().entry(StationIdentity::fromBase64Url(target)));
        QCOMPARE(core.sessions().placesTaken(), 3);
        QVERIFY(!admitted(fifth));
        QCOMPARE(endOf(first.first()).value(QStringLiteral("code")).toString(),
                 QStringLiteral("takenOver"));
    }

    void takerDisconnectAtIncumbentEndStillSettlesIncumbent()
    {
        Core core;
        Device devices[5];
        for (Device& device : devices) core.pair(device);
        QList<LoopbackTransport*> first;
        for (int i = 0; i < 4; ++i) {
            first.append(core.signIn(devices[i]));
            QVERIFY(admitted(first.last()));
        }
        LoopbackTransport* fifth = core.signIn(devices[4]);
        QJsonObject held;
        verifyHeld(fifth, &held);
        const QString target = held.value(QStringLiteral("devices")).toArray().first()
                                   .toObject().value(QStringLiteral("deviceId")).toString();
        bool dropped = false;
        const auto connection = QObject::connect(first.first()->peerForTest(),
            &LoopbackTransport::outboundText, core.server.get(), [&](const QByteArray& wire) {
                const QJsonObject message = QJsonDocument::fromJson(wire).object();
                if (message.value(QStringLiteral("type")) != QStringLiteral("session.end")
                    || message.value(QStringLiteral("code")) != QStringLiteral("takenOver")) return;
                dropped = true;
                fifth->peerForTest()->closeLink(QStringLiteral("test taker left"));
            }, Qt::DirectConnection);
        auto disconnect = qScopeGuard([&]() { QObject::disconnect(connection); });
        fifth->sendText(SessionMessages::encode(SessionMessages::sessionTakeover(
            target, static_cast<quint32>(held.value(QStringLiteral("revision")).toInteger()))));
        QTRY_VERIFY(dropped);
        QVERIFY(!core.sessions().entry(StationIdentity::fromBase64Url(target)));
        QCOMPARE(core.sessions().placesTaken(), 3);
        QVERIFY(!admitted(fifth));
        QCOMPARE(endOf(first.first()).value(QStringLiteral("code")).toString(),
                 QStringLiteral("takenOver"));
    }

    void serverDestroyedDuringAdmissionDoesNotResumeRefresh()
    {
        Core core;
        Device device;
        core.pair(device);
        QObject::connect(core.server->deviceSessions(), &DeviceSessionRegistry::changed,
                         core.model.get(), [&]() { core.server.reset(); }, Qt::DirectConnection);
        core.signIn(device);
        QTRY_VERIFY(!core.server);
    }

    void serverDestroyedByIncumbentEndDoesNotResumeDrop()
    {
        Core core;
        Device devices[5];
        for (Device& device : devices) core.pair(device);
        QList<LoopbackTransport*> first;
        for (int i = 0; i < 4; ++i) {
            first.append(core.signIn(devices[i]));
            QVERIFY(admitted(first.last()));
        }
        LoopbackTransport* fifth = core.signIn(devices[4]);
        QJsonObject held;
        verifyHeld(fifth, &held);
        const QString target = held.value(QStringLiteral("devices")).toArray().first()
                                   .toObject().value(QStringLiteral("deviceId")).toString();
        QObject::connect(first.first()->peerForTest(), &LoopbackTransport::outboundText,
                         core.model.get(), [&](const QByteArray& wire) {
            const QJsonObject message = QJsonDocument::fromJson(wire).object();
            if (message.value(QStringLiteral("code")).toString() == QStringLiteral("takenOver"))
                core.server.reset();
        }, Qt::DirectConnection);
        fifth->sendText(SessionMessages::encode(SessionMessages::sessionTakeover(
            target, static_cast<quint32>(held.value(QStringLiteral("revision")).toInteger()))));
        QTRY_VERIFY(!core.server);
    }

    void serverDestroyedDuringSliceRemovalDoesNotResumeRelease()
    {
        Core core;
        Device devices[5];
        for (Device& device : devices) core.pair(device);
        QList<LoopbackTransport*> first;
        for (int i = 0; i < 4; ++i) {
            first.append(core.signIn(devices[i]));
            QVERIFY(admitted(first.last()));
        }
        LoopbackTransport* fifth = core.signIn(devices[4]);
        QJsonObject held;
        verifyHeld(fifth, &held);
        const QString target = held.value(QStringLiteral("devices")).toArray().first()
                                   .toObject().value(QStringLiteral("deviceId")).toString();
        QObject::connect(core.model.get(), &RadioModel::sliceRemoved, core.model.get(),
                         [&](int) { core.server.reset(); }, Qt::DirectConnection);
        fifth->sendText(SessionMessages::encode(SessionMessages::sessionTakeover(
            target, static_cast<quint32>(held.value(QStringLiteral("revision")).toInteger()))));
        QTRY_VERIFY(!core.server);
    }

    void serverDestroyedByCutoverSignalDoesNotResumeItsCleanup()
    {
        Core core;
        Device devices[5];
        for (Device& device : devices) core.pair(device);
        for (int i = 0; i < 4; ++i) QVERIFY(admitted(core.signIn(devices[i])));
        LoopbackTransport* fifth = core.signIn(devices[4]);
        QJsonObject held;
        verifyHeld(fifth, &held);
        const QString target = held.value(QStringLiteral("devices")).toArray().first()
                                   .toObject().value(QStringLiteral("deviceId")).toString();
        const QByteArray targetId = StationIdentity::fromBase64Url(target);
        bool destroyed = false;
        QObject::connect(core.server->deviceSessions(), &DeviceSessionRegistry::changed,
                         core.model.get(), [&]() {
            if (destroyed || core.sessions().entry(targetId)) return;
            destroyed = true;
            core.server.reset();
        }, Qt::DirectConnection);
        fifth->sendText(SessionMessages::encode(SessionMessages::sessionTakeover(
            target, static_cast<quint32>(held.value(QStringLiteral("revision")).toInteger()))));
        QTRY_VERIFY(destroyed);
        QTRY_VERIFY(!core.server);
    }
};

QTEST_MAIN(TstStationFifthDevice)
#include "tst_station_fifth_device.moc"
