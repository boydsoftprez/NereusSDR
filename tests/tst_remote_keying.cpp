// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_remote_keying.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 35 (R-IOS-13, R-IOS-02; D51, D58, D63; the
// several-devices design, section 2.2 and rulings 8.3, 8.5 and 8.14):
// keying from a remote device, over the in-process loopback against a real
// StationServer and RadioModel (no radio behind it; nothing reaches a
// transmitter). Every refusal, holder and gate rule first:
//
//   - tx.key from a device with no completed session is refused notReady;
//   - a write of transmit's mox or tune is refused "Use the transmit
//     button." and keys nothing;
//   - another device's key while transmit is held is refused
//     otherDeviceHolds, keyed or not; its tx.unkey is refused and the
//     transmission continues;
//   - a program's key never takes transmit (programNeedsTransmit, nobody
//     the holder), and keys once its device holds transmit;
//   - a copy of a key after a safety stop never keys again (keyEnded); an
//     unkey carrying an older epoch is ignored; each key and unkey sent
//     three times acts once;
//   - a lost link unkeys, and after the reconnect nothing keys until a new
//     tx.key;
//
// then the keys themselves: a person's key on unheld transmit takes it and
// keys the transmit slice, tx.unkey unkeys, tx.tune tunes at the tune
// power, VOX the holder armed is the holder's, and keyedBy names the holder.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 35 (R-IOS-13), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 37 (R-IOS-13): tx.keepalive's gate and
//               readability, keepalives holding a key, an older epoch
//               holding nothing, the watchdog's stop and the delayed copy.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include "core/PttSource.h"
#include "core/RadioStatus.h"
#include "core/TwoToneController.h"
#include "core/session/RemoteKeying.h"
#include "core/safety/RemoteTxWatchdog.h"

namespace {

const QString kUseTheButton = QStringLiteral("Use the transmit button.");

MirrorUpdate boolean(const char* name, bool value)
{
    return MirrorUpdate{0, QByteArray(name), MirrorWireKind::Bool, QVariant(value)};
}

// Sends `verb` with command id `id` and waits for the `nth` result carrying
// that id (a copy is answered too).
QJsonObject invokeAs(LoopbackTransport* app, const QByteArray& verb, quint32 id,
                     const QList<MirrorUpdate>& arguments, int nth = 1)
{
    app->sendText(SessionMessages::encode(SessionMessages::commandInvoke(verb, id, arguments)));
    const auto find = [app, id, nth]() {
        int seen = 0;
        for (const QJsonObject& o : ofType(app->received(), QStringLiteral("command.result"))) {
            if (o.value(QStringLiteral("id")).toInteger() == id && ++seen == nth) {
                return o;
            }
        }
        return QJsonObject{};
    };
    const bool answered = QTest::qWaitFor([&find]() { return !find().isEmpty(); }, 5000);
    Q_UNUSED(answered);
    return find();
}

QJsonValue valueOf(const QJsonObject& result, const QString& name)
{
    for (const QJsonValue& v : result.value(QStringLiteral("values")).toArray()) {
        if (v.toObject().value(QStringLiteral("name")).toString() == name) {
            return v.toObject().value(QStringLiteral("value"));
        }
    }
    return {};
}

qint64 epochOf(const QJsonObject& result)
{
    return valueOf(result, QStringLiteral("epoch")).toInteger(0);
}

bool refusedWith(const QJsonObject& result, const TxRefusal& refusal)
{
    return !result.value(QStringLiteral("accepted")).toBool(true)
        && result.value(QStringLiteral("reason")).toString() == refusal.text
        && valueOf(result, QStringLiteral("refusalCode")).toString() == QString::fromUtf8(refusal.code)
        && valueOf(result, QStringLiteral("refusalFix")).toString() == QString::fromUtf8(refusal.fix);
}

QString describe(const QJsonObject& result)
{
    return QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact));
}

bool accepted(const QJsonObject& result)
{
    return result.value(QStringLiteral("accepted")).toBool(false);
}

QList<MirrorUpdate> trigger(const char* name)
{
    return {utf8("trigger", QString::fromLatin1(name))};
}

// Two devices on one Core that allows remote transmit.
struct Pair {
    Core core;
    Device a{QStringLiteral("Grant's iPhone"), QStringLiteral("phone"), QStringLiteral("iPhone")};
    Device b{QStringLiteral("iPad"), QStringLiteral("tablet")};
    LoopbackTransport* appA{nullptr};
    LoopbackTransport* appB{nullptr};
    MoxController* mox{nullptr};
    TransmitHolder* holder{nullptr};
    quint32 nextId{700};

    Pair()
    {
        allowTransmit(core);
        core.pair(a);
        core.pair(b);
        appA = core.signIn(a, kTransmitter);
        appB = core.signIn(b, kTransmitter);
        mox = core.model->moxController();
        holder = core.server->transmitHolder();
    }

    QJsonObject send(LoopbackTransport* app, const QByteArray& verb,
                     const QList<MirrorUpdate>& arguments)
    {
        return invokeAs(app, verb, nextId++, arguments);
    }
    // Station VOX (whole-branch review, TX path): A, holding transmit,
    // arms VOX as a device does (its write, its microphone line open, VOX
    // listening to it as DaemonMediaController marks it). VOX keys only
    // for the device that armed it.
    bool armVoxForA()
    {
        const QByteArray id = a.key.fingerprint();
        core.model->openRemoteMicLine(id);
        const quint32 writeId = nextId++;
        appA->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "transmit", {MirrorUpdate{0, "voxEnabled", MirrorWireKind::Bool, QVariant(true)}},
            writeId)));
        if (!QTest::qWaitFor([&]() { return !propertyResult(appA, writeId).isEmpty(); }, 5000)) {
            return false;
        }
        const QJsonObject result = propertyResult(appA, writeId)
                                       .value(QStringLiteral("results")).toArray().first().toObject();
        if (!accepted(result)) {
            qWarning() << "VOX arm refused:" << describe(result);
            return false;
        }
        core.model->setRemoteMicVoxArmed(id, true);
        return core.server->voxArmedBy() == id && core.model->remoteVoxDevice() == id;
    }
};

} // namespace

class TstRemoteKeying : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { qRegisterMetaType<NereusSDR::TxRefusal>(); }

    // ---- Refusals, holders and gates first --------------------------------------

    void aKeyBeforeTheDeviceHasFinishedConnectingIsRefusedNotReady()
    {
        // A device with no completed session (it has not reached
        // snapshot.complete) is refused by the session's own gate. The
        // gate's snapshot rule itself is tst_station_tx_gate's.
        Core core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        RemoteKeying::Command command;
        command.verb = RemoteKeying::Verb::Key;
        command.deviceId = a.key.fingerprint();
        command.session = QStringLiteral("station:none");
        command.commandId = 1;
        command.trigger = "screen";
        const RemoteKeying::Result result = core.server->remoteKeying()->handle(command);
        QVERIFY(!result.accepted);
        QCOMPARE(result.refusal.code, QByteArray(TxRefusals::kNotReady));
        QVERIFY(!core.model->moxController()->isMox());
        QCOMPARE(core.server->transmitHolder()->state(), TransmitHolder::State::Unheld);
    }

    void theKeyingVerbsComeOnlyWithRemoteTx()
    {
        // A device whose hello did not declare remoteTx is refused before
        // anything is looked at, and nothing keys.
        Core core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* app = core.signIn(a, kHolder);
        QVERIFY(admitted(app));
        for (const QByteArray& verb : {QByteArrayLiteral("tx.key"), QByteArrayLiteral("tx.unkey"),
                                        QByteArrayLiteral("tx.tune"), QByteArrayLiteral("tx.twoTone"),
                                        QByteArrayLiteral("tx.keepalive")}) {
            const QJsonObject r = core.invoke(
                app, verb,
                verb == "tx.key"         ? trigger("screen")
                : verb == "tx.unkey"     ? QList<MirrorUpdate>{int64("epoch", 1)}
                : verb == "tx.keepalive" ? QList<MirrorUpdate>{int64("sequence", 1), int64("epoch", 1)}
                                         : QList<MirrorUpdate>{boolean("on", true)});
            QVERIFY2(!accepted(r), verb.constData());
            QCOMPARE(r.value(QStringLiteral("reason")).toString(),
                     TxRefusals::appCannotTransmit().text);
        }
        QVERIFY(!core.model->moxController()->isMox());
    }

    void aKeyTheCoreCannotReadIsRefused()
    {
        Pair p;
        QVERIFY(admitted(p.appA));
        const QString unreadable = QStringLiteral("The Core could not read this request.");
        for (const auto& [verb, args] : QList<QPair<QByteArray, QList<MirrorUpdate>>>{
                 {"tx.key", trigger("elbow")},
                 {"tx.key", {}},
                 {"tx.unkey", {int64("epoch", 0)}},
                 {"tx.unkey", {utf8("epoch", QStringLiteral("1"))}},
                 {"tx.tune", {int64("on", 1)}},
                 {"tx.twoTone", {}},
                 // Task 37: tx.keepalive {sequence >= 1, epoch 0..4294967295}.
                 {"tx.keepalive", {int64("sequence", 0), int64("epoch", 1)}},
                 {"tx.keepalive", {int64("sequence", 1), int64("epoch", -1)}},
                 {"tx.keepalive", {int64("sequence", 1), int64("epoch", 4294967296LL)}},
                 {"tx.keepalive", {int64("sequence", 1)}},
                 {"tx.keepalive", {int64("sequence", 1), utf8("epoch", QStringLiteral("1"))}}}) {
            const QJsonObject r = p.send(p.appA, verb, args);
            QVERIFY2(!accepted(r), verb.constData());
            QCOMPARE(r.value(QStringLiteral("reason")).toString(), unreadable);
        }
        QVERIFY(!p.mox->isMox());
        QCOMPARE(p.holder->state(), TransmitHolder::State::Unheld);
    }

    void aPropertyWriteOfMoxOrTuneIsRefusedAndKeysNothing()
    {
        Pair p;
        QVERIFY(admitted(p.appA));
        qint64 writeId = 50;
        for (const char* name : {"mox", "tune"}) {
            const qint64 id = ++writeId;
            p.appA->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
                "transmit", {boolean(name, true)}, static_cast<quint32>(id))));
            QTRY_VERIFY(!propertyResult(p.appA, id).isEmpty());
            const QJsonObject result =
                propertyResult(p.appA, id).value(QStringLiteral("results")).toArray().first().toObject();
            QVERIFY(!result.value(QStringLiteral("accepted")).toBool(true));
            QCOMPARE(result.value(QStringLiteral("reason")).toString(), kUseTheButton);
        }
        QVERIFY(!p.mox->isMox());
        QVERIFY(!p.core.model->isTune());
        QCOMPARE(p.holder->state(), TransmitHolder::State::Unheld);
    }

    void anotherDevicesKeyIsRefusedWhileTransmitIsHeldKeyedOrNot()
    {
        Pair p;
        QVERIFY(admitted(p.appA) && admitted(p.appB));
        // A person's key on unheld transmit: A takes it and keys, asked of
        // nobody.
        const QJsonObject keyA = p.send(p.appA, "tx.key", trigger("screen"));
        QVERIFY2(accepted(keyA), qPrintable(describe(keyA)));
        QVERIFY(p.holder->isHeldBy(p.a.key.fingerprint()));
        QVERIFY(p.mox->isMox());

        const TxRefusal heldByA = TxRefusals::otherDeviceHolds(QStringLiteral("Grant's iPhone"));
        // B's key while A is keyed.
        QVERIFY(refusedWith(p.send(p.appB, "tx.key", trigger("screen")), heldByA));
        QVERIFY(refusedWith(p.send(p.appB, "tx.tune", {boolean("on", true)}), heldByA));
        QVERIFY(refusedWith(p.send(p.appB, "tx.twoTone", {boolean("on", true)}), heldByA));
        // B's release is refused and A stays keyed.
        const TxRefusal stopRefused = TxRefusals::otherDeviceHoldsStop(QStringLiteral("Grant's iPhone"));
        QCOMPARE(stopRefused.text,
                 QStringLiteral("Grant's iPhone has the transmitter. Take it to stop the transmission."));
        QVERIFY(refusedWith(p.send(p.appB, "tx.unkey", {int64("epoch", epochOf(keyA))}), stopRefused));
        QVERIFY(refusedWith(p.send(p.appB, "tx.tune", {boolean("on", false)}), stopRefused));
        QVERIFY(refusedWith(p.send(p.appB, "tx.twoTone", {boolean("on", false)}), stopRefused));
        QVERIFY(p.mox->isMox());
        QCOMPARE(p.mox->currentKeyer().deviceId, p.a.key.fingerprint());

        // A's release unkeys.
        QVERIFY(accepted(p.send(p.appA, "tx.unkey", {int64("epoch", epochOf(keyA))})));
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);

        // A still holds transmit, unkeyed: B is still refused.
        QVERIFY(p.holder->isHeldBy(p.a.key.fingerprint()));
        QVERIFY(refusedWith(p.send(p.appB, "tx.key", trigger("headset")), heldByA));
        QVERIFY(!p.mox->isMox());
    }

    void aProgramNeverTakesTransmit()
    {
        Pair p;
        QVERIFY(admitted(p.appA) && admitted(p.appB));
        // Unheld: refused programNeedsTransmit, and nobody holds it.
        QVERIFY(refusedWith(p.send(p.appA, "tx.key", trigger("tci")),
                            TxRefusals::programNeedsTransmit()));
        QCOMPARE(p.holder->state(), TransmitHolder::State::Unheld);
        QVERIFY(!p.mox->isMox());

        // A takes transmit with a person's key and lets go; the program's
        // key then keys, as A's.
        const QJsonObject person = p.send(p.appA, "tx.key", trigger("screen"));
        QVERIFY(accepted(person));
        // A refused key spends no epoch: this is the Core's first key.
        QCOMPARE(epochOf(person), 1);
        QVERIFY(accepted(p.send(p.appA, "tx.unkey", {int64("epoch", epochOf(person))})));
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
        const QJsonObject program = p.send(p.appA, "tx.key", trigger("tci"));
        QVERIFY2(accepted(program), qPrintable(describe(program)));
        QVERIFY(epochOf(program) > epochOf(person));
        QCOMPARE(p.mox->currentKeyer().deviceId, p.a.key.fingerprint());
        QCOMPARE(p.core.model->keyedBy().trigger, QByteArray("tci"));
        QCOMPARE(p.core.model->keyedBy().deviceId, p.a.key.fingerprint());

        // Another device's program while A holds: otherDeviceHolds.
        QVERIFY(refusedWith(p.send(p.appB, "tx.key", trigger("tci")),
                            TxRefusals::otherDeviceHolds(QStringLiteral("Grant's iPhone"))));
        QVERIFY(accepted(p.send(p.appA, "tx.unkey", {int64("epoch", epochOf(program))})));
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
    }

    void eachKeyAndUnkeySentThreeTimesActsOnce()
    {
        Pair p;
        QVERIFY(admitted(p.appA));
        QSignalSpy keyUps(p.mox, &MoxController::txAboutToBegin);
        QSignalSpy keyDowns(p.mox, &MoxController::txAboutToEnd);
        const quint32 keyId = 900;
        QList<qint64> epochs;
        for (int copy = 1; copy <= 3; ++copy) {
            const QJsonObject r = invokeAs(p.appA, "tx.key", keyId, trigger("screen"), copy);
            QVERIFY2(accepted(r), qPrintable(describe(r)));
            epochs.append(epochOf(r));
        }
        QCOMPARE(keyUps.count(), 1);
        QVERIFY(epochs.at(0) > 0);
        QCOMPARE(epochs.at(1), epochs.at(0));
        QCOMPARE(epochs.at(2), epochs.at(0));
        // A key with a new id while this device's key is on changes nothing.
        const QJsonObject again = p.send(p.appA, "tx.key", trigger("screen"));
        QCOMPARE(epochOf(again), epochs.at(0));
        QCOMPARE(keyUps.count(), 1);

        const quint32 unkeyId = 901;
        for (int copy = 1; copy <= 3; ++copy) {
            QVERIFY(accepted(invokeAs(p.appA, "tx.unkey", unkeyId, {int64("epoch", epochs.at(0))}, copy)));
        }
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
        QCOMPARE(keyDowns.count(), 1);
        QCOMPARE(keyUps.count(), 1);
    }

    void anUnkeyCarryingAnOlderEpochIsIgnored()
    {
        Pair p;
        QVERIFY(admitted(p.appA));
        const QJsonObject first = p.send(p.appA, "tx.key", trigger("screen"));
        QVERIFY(accepted(p.send(p.appA, "tx.unkey", {int64("epoch", epochOf(first))})));
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
        const QJsonObject second = p.send(p.appA, "tx.key", trigger("screen"));
        QVERIFY(epochOf(second) > epochOf(first));
        QTRY_COMPARE(p.mox->state(), MoxState::Tx);
        // The first key's unkey, arriving late: ignored, still keyed.
        QVERIFY(accepted(p.send(p.appA, "tx.unkey", {int64("epoch", epochOf(first))})));
        QVERIFY(p.mox->isMox());
        QCOMPARE(p.core.model->keyedBy().epoch, static_cast<quint32>(epochOf(second)));
        QVERIFY(accepted(p.send(p.appA, "tx.unkey", {int64("epoch", epochOf(second))})));
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
    }

    // Task 37 (remote design section 12.1): keepalives hold a remote key;
    // keepalives naming an older epoch do not, and the Core stops within
    // 500 ms of the last one that counted; a delayed copy of the key from
    // before the stop never keys again.
    void keepalivesHoldAKeyAndTheWatchdogStopsItWithoutThem()
    {
        Pair p;
        QVERIFY(admitted(p.appA));
        QSignalSpy tripped(p.core.server->txWatchdog(), &RemoteTxWatchdog::tripped);
        const quint32 keyId = p.nextId++;
        const QJsonObject key = invokeAs(p.appA, "tx.key", keyId, trigger("screen"));
        QVERIFY2(accepted(key), qPrintable(describe(key)));
        const qint64 epoch = epochOf(key);
        QTRY_COMPARE(p.mox->state(), MoxState::Tx);
        // The Core's clock is the harness's; it moves with real time here,
        // 50 ms a step, so the watchdog's real timer finds it moved.
        const auto step = [&p]() {
            p.core.now += 50;
            QTest::qWait(50);
        };
        qint64 sequence = 0;
        for (int i = 0; i < 20; ++i) {
            if (i % 2 == 0) {
                QVERIFY(accepted(p.send(p.appA, "tx.keepalive",
                                        {int64("sequence", ++sequence), int64("epoch", epoch)})));
            }
            step();
        }
        QVERIFY(p.mox->isMox());
        QCOMPARE(tripped.count(), 0);
        // From an earlier key: answered, but they hold nothing.
        for (int i = 0; i < 30 && tripped.isEmpty(); ++i) {
            if (i % 2 == 0) {
                QVERIFY(accepted(p.send(p.appA, "tx.keepalive",
                                        {int64("sequence", ++sequence), int64("epoch", epoch - 1)})));
            }
            step();
        }
        QCOMPARE(tripped.count(), 1);
        const qint64 silent = tripped.first().at(2).toLongLong();
        QVERIFY2(silent > RemoteTxWatchdog::kLinkLossDeadlineMs && silent <= 500,
                 qPrintable(QString::number(silent)));
        QVERIFY(!p.mox->isMox());
        // The key's delayed copy: refused, nothing keys.
        const QJsonObject copy = invokeAs(p.appA, "tx.key", keyId, trigger("screen"), 2);
        QVERIFY2(refusedWith(copy, TxRefusals::keyEnded()), qPrintable(describe(copy)));
        QTest::qWait(100);
        QVERIFY(!p.mox->isMox());
    }

    void theEmergencyStopBeatsARemoteKeyAndADelayedCopyNeverKeysAgain()
    {
        Pair p;
        QVERIFY(admitted(p.appA));
        const quint32 keyId = 950;
        const QJsonObject key = invokeAs(p.appA, "tx.key", keyId, trigger("screen"));
        QVERIFY(accepted(key));
        QTRY_COMPARE(p.mox->state(), MoxState::Tx);

        // The Core's safety stop (as the watchdog, starvation, the time-out
        // and the interlock call it): whoever is keyed is unkeyed.
        p.core.model->stopAllTx(QStringLiteral("The link to Grant's iPhone went quiet, so the "
                                               "Core stopped transmitting."));
        QVERIFY(!p.mox->isMox());
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
        QVERIFY(p.core.model->keyedBy().isEmpty());

        // The key's delayed copies (the same command) are refused and key
        // nothing.
        QSignalSpy keyUps(p.mox, &MoxController::txAboutToBegin);
        for (int copy = 2; copy <= 3; ++copy) {
            const QJsonObject r = invokeAs(p.appA, "tx.key", keyId, trigger("screen"), copy);
            QVERIFY2(refusedWith(r, TxRefusals::keyEnded()), qPrintable(describe(r)));
        }
        QCOMPARE(keyUps.count(), 0);
        QVERIFY(!p.mox->isMox());
    }

    void aLostLinkUnkeysAndTheReconnectNeverKeysByItself()
    {
        Pair p;
        QVERIFY(admitted(p.appA) && admitted(p.appB));
        const quint32 keyId = 960;
        QVERIFY(accepted(invokeAs(p.appA, "tx.key", keyId, trigger("screen"))));
        QTRY_COMPARE(p.mox->state(), MoxState::Tx);

        p.appA->closeLink(QStringLiteral("lost"));
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
        QTRY_VERIFY(!p.holder->isFenced());
        QVERIFY(p.holder->holder()->away);

        // Back within its 180 s: still the holder, unkeyed, and nothing
        // keys (no replay, no resume), not even the old command's id.
        p.core.now += 30000;
        QSignalSpy keyUps(p.mox, &MoxController::txAboutToBegin);
        LoopbackTransport* back = p.core.signIn(p.a, kTransmitter);
        QVERIFY(admitted(back));
        QVERIFY(p.holder->isHeldBy(p.a.key.fingerprint()));
        QTest::qWait(50);
        QVERIFY(!p.mox->isMox());
        QCOMPARE(keyUps.count(), 0);

        // A new tx.key keys (the new session's own command).
        QVERIFY(accepted(invokeAs(back, "tx.key", keyId, trigger("screen"))));
        QVERIFY(p.mox->isMox());
        QCOMPARE(keyUps.count(), 1);
        p.mox->setMox(false);
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
    }

    // ---- The keys -------------------------------------------------------------------

    void aPersonsKeyOnUnheldTransmitTakesItKeysTheTxSliceAndUnkeys()
    {
        Pair p;
        QVERIFY(admitted(p.appA));
        QCOMPARE(p.holder->state(), TransmitHolder::State::Unheld);
        const QJsonObject key = p.send(p.appA, "tx.key", trigger("headset"));
        QVERIFY2(accepted(key), qPrintable(describe(key)));
        QVERIFY(epochOf(key) > 0);
        QVERIFY(p.holder->isHeldBy(p.a.key.fingerprint()));
        QTRY_COMPARE(p.mox->state(), MoxState::Tx);
        // The transmit slice keys: the arbiter's TX slice is the one on air.
        QVERIFY(p.core.model->txBoundSlice() != nullptr);

        // keyedBy names the holder, how it keyed and the key's epoch.
        const RadioModel::KeyedBy keyedBy = p.core.model->keyedBy();
        QCOMPARE(keyedBy.deviceId, p.a.key.fingerprint());
        QCOMPARE(keyedBy.deviceName, QStringLiteral("Grant's iPhone"));
        QCOMPARE(keyedBy.deviceKind, QStringLiteral("phone"));
        QCOMPARE(keyedBy.trigger, QByteArray("headset"));
        QCOMPARE(keyedBy.epoch, static_cast<quint32>(epochOf(key)));
        QCOMPARE(p.core.model->radioStatus().activePttSource(), PttSource::Remote);

        QVERIFY(accepted(p.send(p.appA, "tx.unkey", {int64("epoch", epochOf(key))})));
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
        QVERIFY(p.core.model->keyedBy().isEmpty());
        QCOMPARE(p.core.model->radioStatus().activePttSource(), PttSource::None);
    }

    void tuneTunesAtTheTunePowerForTheDevice()
    {
        Pair p;
        QVERIFY(admitted(p.appA));
        const QJsonObject tune = p.send(p.appA, "tx.tune", {boolean("on", true)});
        QVERIFY2(accepted(tune), qPrintable(describe(tune)));
        QVERIFY(epochOf(tune) > 0);
        QVERIFY(p.core.model->isTune());
        // TransmitModel's tune follows TUNE, which selects the tune power
        // as the drive source (TransmitModel::setPowerUsingTargetDbm).
        QVERIFY(p.core.model->transmitModel().isTune());
        QCOMPARE(p.mox->currentKeyer().deviceId, p.a.key.fingerprint());
        QVERIFY(p.holder->isHeldBy(p.a.key.fingerprint()));
        QCOMPARE(p.core.model->keyedBy().trigger, QByteArray("tune"));

        QVERIFY(accepted(p.send(p.appA, "tx.tune", {boolean("on", false)})));
        QTRY_VERIFY(!p.core.model->isTune());
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
    }

    void twoToneIsTheHoldersAndRefusedAsAnyKey()
    {
        Pair p;
        QVERIFY(admitted(p.appA) && admitted(p.appB));
        QVERIFY(accepted(p.send(p.appA, "tx.key", trigger("screen"))));
        // Held by A: B's two-tone is refused naming A before anything starts.
        QVERIFY(refusedWith(p.send(p.appB, "tx.twoTone", {boolean("on", true)}),
                            TxRefusals::otherDeviceHolds(QStringLiteral("Grant's iPhone"))));
        QVERIFY(p.core.model->twoToneController() == nullptr
                || !p.core.model->twoToneController()->isActivationInFlight());
        p.mox->setMox(false);
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
    }

    void voxIsTheHoldersKey()
    {
        Pair p;
        QVERIFY(admitted(p.appA));
        const QJsonObject key = p.send(p.appA, "tx.key", trigger("screen"));
        QVERIFY(accepted(p.send(p.appA, "tx.unkey", {int64("epoch", epochOf(key))})));
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
        // A holds transmit and armed VOX; a VOX key is admitted as A's.
        QVERIFY(p.armVoxForA());
        p.mox->onVoxActive(true);
        QVERIFY(p.mox->isMox());
        QCOMPARE(p.core.model->keyedBy().deviceId, p.a.key.fingerprint());
        QCOMPARE(p.core.model->keyedBy().trigger, QByteArray("vox"));
        QVERIFY(p.holder->holder()->keyed);
        // The holder's release stops its VOX key too.
        QVERIFY(accepted(p.send(p.appA, "tx.unkey",
                                {int64("epoch", p.core.model->keyedBy().epoch)})));
        QVERIFY(!p.mox->isMox());
        p.mox->onVoxActive(false);
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
    }

    // Fix wave M5: the holder's own press while its VOX key is on is that
    // key, answered with its epoch (never "changing hands"), and its
    // release ends the VOX key.
    void theHoldersPressDuringItsVoxKeyIsItsKey()
    {
        Pair p;
        QVERIFY(admitted(p.appA));
        const QJsonObject key = p.send(p.appA, "tx.key", trigger("screen"));
        QVERIFY(accepted(p.send(p.appA, "tx.unkey", {int64("epoch", epochOf(key))})));
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
        QVERIFY(p.armVoxForA());
        p.mox->onVoxActive(true);
        QVERIFY(p.mox->isMox());
        const quint32 voxEpoch = p.core.model->keyedBy().epoch;
        const QJsonObject press = p.send(p.appA, "tx.key", trigger("screen"));
        QVERIFY2(accepted(press), QJsonDocument(press).toJson().constData());
        QCOMPARE(quint32(epochOf(press)), voxEpoch);
        QVERIFY(accepted(p.send(p.appA, "tx.unkey", {int64("epoch", epochOf(press))})));
        QVERIFY(!p.mox->isMox());
        p.mox->onVoxActive(false);
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
    }

    void keyedByNamesTheRadioForTheRadiosOwnPtt()
    {
        Pair p;
        QVERIFY(admitted(p.appA));
        p.mox->onMicPttFromRadio(true);
        QVERIFY(p.mox->isMox());
        const RadioModel::KeyedBy keyedBy = p.core.model->keyedBy();
        QCOMPARE(keyedBy.deviceId, QByteArray(KeyerIdentity::kStationDeviceId));
        QCOMPARE(keyedBy.deviceName, QStringLiteral("Radio"));
        QCOMPARE(keyedBy.deviceKind, QStringLiteral("station"));
        QCOMPARE(keyedBy.trigger, QByteArray("radioPtt"));
        QVERIFY(keyedBy.epoch > 0);
        QVERIFY(p.core.model->radioStatus().activePttSource() != PttSource::Remote);
        // A's key while the radio holds transmit: refused naming the radio.
        QVERIFY(refusedWith(p.send(p.appA, "tx.key", trigger("screen")),
                            TxRefusals::otherDeviceHolds(QStringLiteral("Radio"))));
        p.mox->onMicPttFromRadio(false);
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
    }

    void theKeyingRefusalsArePlainWords()
    {
        for (const TxRefusal& refusal :
             {TxRefusals::keyEnded(), TxRefusals::otherDeviceHoldsStop(QStringLiteral("Jo's iPhone"))}) {
            QVERIFY2(OperatorWording::isPlain(refusal.text), qPrintable(refusal.text));
        }
        QVERIFY(OperatorWording::isPlain(kUseTheButton));
    }
};

QTEST_MAIN(TstRemoteKeying)
#include "tst_remote_keying.moc"
