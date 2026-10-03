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
#include "core/TxChannel.h"
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

QList<QJsonObject> resultsWithId(LoopbackTransport* app, quint32 id)
{
    QList<QJsonObject> results;
    for (const QJsonObject& result : ofType(app->received(), QStringLiteral("command.result"))) {
        if (result.value(QStringLiteral("id")).toInteger() == id) { results.append(result); }
    }
    return results;
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

    explicit Pair(bool radioSource = false)
    {
        allowTransmit(core);
        core.pair(a);
        core.pair(b);
        auto features = kTransmitter;
        if (radioSource) { features.insert("radioMic", 2); }
        appA = core.signIn(a, features);
        appB = core.signIn(b, features);
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

    // Radio input belongs to this authenticated session. A source selection
    // acknowledges its accepted value without changing the Core's own input,
    // taking transmit, or pressing any transmit button.
    void radioMicSelectionIsAcknowledgedWithoutChangingCoreInputOrKeying()
    {
        Core core;
        allowTransmit(core);
        core.model->setPcCaptureAllowed(false);
        core.model->transmitModel().setMicSource(MicSource::Pc);
        Device a;
        core.pair(a);
        auto features = kTransmitter;
        features.insert(QByteArrayLiteral("radioMic"), 2);
        LoopbackTransport* app = core.signIn(a, features);
        QVERIFY(admitted(app));

        const QJsonObject radio = core.invoke(
            app, "tx.setMicSource", {utf8("source", QStringLiteral("RadioMic"))});
        QVERIFY2(accepted(radio), qPrintable(describe(radio)));
        QCOMPARE(radio.value(QStringLiteral("verb")).toString(),
                 QStringLiteral("tx.setMicSource"));
        const QJsonArray wantedRadio{
            QJsonObject{{QStringLiteral("ordinal"), 0},
                        {QStringLiteral("name"), QStringLiteral("source")},
                        {QStringLiteral("kind"), QStringLiteral("utf8")},
                        {QStringLiteral("value"), QStringLiteral("RadioMic")}}};
        QCOMPARE(radio.value(QStringLiteral("values")).toArray(), wantedRadio);
        const auto version = latestCapabilityIf(app->received(), QStringLiteral("radioMicVersion"));
        QVERIFY2(version.has_value(), "Radio source selection needs a negotiated capability.");
        QCOMPARE(version->toInteger(), qint64(2));
        QCOMPARE(core.model->transmitModel().micSource(), MicSource::Pc);
        QVERIFY(!core.model->moxController()->isMox());
        QCOMPARE(core.server->transmitHolder()->state(), TransmitHolder::State::Unheld);

        const QJsonObject client = core.invoke(
            app, "tx.setMicSource", {utf8("source", QStringLiteral("ClientAudio"))});
        QVERIFY2(accepted(client), qPrintable(describe(client)));
        const QJsonArray wantedClient{
            QJsonObject{{QStringLiteral("ordinal"), 0},
                        {QStringLiteral("name"), QStringLiteral("source")},
                        {QStringLiteral("kind"), QStringLiteral("utf8")},
                        {QStringLiteral("value"), QStringLiteral("ClientAudio")}}};
        QCOMPARE(client.value(QStringLiteral("values")).toArray(), wantedClient);
        QCOMPARE(core.model->transmitModel().micSource(), MicSource::Pc);
        QVERIFY(!core.model->moxController()->isMox());
        QCOMPARE(core.server->transmitHolder()->state(), TransmitHolder::State::Unheld);
    }

    // A selected radio input must not need the desktop's microphone line or
    // wait for its RTP buffer. Returning to ClientAudio restores that gate.
    void radioMicPttKeysWithoutClientMicrophoneButClientAudioStillNeedsIt()
    {
        Core core;
        allowTransmit(core);
        int primeCalls = 0;
        RemoteKeying::MicUplink closedLine;
        closedLine.carriesMic = [](const QByteArray&) { return false; };
        closedLine.prime = [&primeCalls](const QByteArray&, std::function<void(bool)> done) {
            ++primeCalls;
            done(false);
        };
        closedLine.endPriming = [](const QByteArray&) {};
        core.server->remoteKeying()->setMicUplink(closedLine);
        Device a;
        core.pair(a);
        auto features = kTransmitter;
        features.insert(QByteArrayLiteral("radioMic"), 2);
        LoopbackTransport* app = core.signIn(a, features);
        QVERIFY(admitted(app));
        QVERIFY(!core.model->remoteMicLineOpen(a.key.fingerprint()));
        QVERIFY(core.model->sliceById(0) != nullptr);
        QCOMPARE(core.model->sliceById(0)->dspMode(), DSPMode::USB);

        // Send both commands before checking source acceptance: on the old
        // Core this test diagnoses the voice-key refusal, independently of
        // the preceding source command's missing implementation.
        const QJsonObject source = core.invoke(
            app, "tx.setMicSource", {utf8("source", QStringLiteral("RadioMic"))});
        const QJsonObject key = core.invoke(app, "tx.key", trigger("screen"));
        const QString diagnostic = QStringLiteral("source=%1; key=%2")
                                       .arg(describe(source), describe(key));
        QVERIFY2(accepted(key), qPrintable(diagnostic));
        QVERIFY2(accepted(source), qPrintable(describe(source)));
        QCOMPARE(valueOf(source, QStringLiteral("source")).toString(), QStringLiteral("RadioMic"));
        QVERIFY(epochOf(key) > 0);
        QVERIFY(core.model->moxController()->isMox());
        QVERIFY(core.server->transmitHolder()->isHeldBy(a.key.fingerprint()));
        QCOMPARE(core.model->moxController()->currentKeyer().deviceId, a.key.fingerprint());
        QCOMPARE(core.model->keyedBy().deviceId, a.key.fingerprint());
        QCOMPARE(core.model->keyedBy().epoch, static_cast<quint32>(epochOf(key)));
        QCOMPARE(primeCalls, 0);
        QVERIFY(!core.server->remoteKeying()->keyPending());
        QVERIFY(!core.model->remoteMicInUse());
        QVERIFY(core.model->remoteMicWriter().isEmpty());
        QVERIFY(!core.model->remoteMicLineOpen(a.key.fingerprint()));

        const QJsonObject unkey = core.invoke(app, "tx.unkey", {int64("epoch", epochOf(key))});
        QVERIFY2(accepted(unkey), qPrintable(describe(unkey)));
        QTRY_VERIFY(!core.model->moxController()->isMox());
        const QJsonObject client = core.invoke(
            app, "tx.setMicSource", {utf8("source", QStringLiteral("ClientAudio"))});
        QVERIFY2(accepted(client), qPrintable(describe(client)));
        QCOMPARE(valueOf(client, QStringLiteral("source")).toString(), QStringLiteral("ClientAudio"));
        const QJsonObject blocked = core.invoke(app, "tx.key", trigger("screen"));
        QVERIFY2(refusedWith(blocked, TxRefusals::micNotConnected()), qPrintable(describe(blocked)));
        QVERIFY(!core.model->moxController()->isMox());
        QCOMPARE(primeCalls, 0);
        QVERIFY(!core.server->remoteKeying()->keyPending());
    }

    void anotherSessionOfSameDeviceCannotInheritRadioKeyEpoch()
    {
        Pair p(true);
        QVERIFY(accepted(p.send(p.appA, "tx.setMicSource", {utf8("source", QStringLiteral("RadioMic"))})));
        const auto key = p.send(p.appA, "tx.key", trigger("screen"));
        QVERIFY(accepted(key));
        RemoteKeying::Command other;
        other.verb = RemoteKeying::Verb::Key;
        other.deviceId = p.a.key.fingerprint();
        other.session = QStringLiteral("station:different-owner");
        other.commandId = 7001;
        other.trigger = "screen";
        const auto result = p.core.server->remoteKeying()->handle(other);
        QVERIFY(!result.accepted);
        QCOMPARE(p.core.model->keyedBy().epoch, static_cast<quint32>(epochOf(key)));
        QVERIFY(p.core.model->remoteRadioMicKeyActive(p.a.key.fingerprint()));
    }

    void sourceDisarmingVoxRechecksReentrantKeyBeforeCommit()
    {
        Pair p(true);
        const auto first = p.send(p.appA, "tx.key", trigger("screen"));
        QVERIFY(accepted(first));
        QVERIFY(accepted(p.send(p.appA, "tx.unkey", {int64("epoch", epochOf(first))})));
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
        QVERIFY(p.armVoxForA());
        bool called = false;
        QJsonObject nested;
        const auto observation = connect(p.core.server.get(), &StationServer::voxArmedByChanged, this,
                [&](const QByteArray& device) {
            if (device.isEmpty() && !called) {
                called = true;
                nested = p.send(p.appA, "tx.key", trigger("screen"));
            }
        });
        const auto source = p.send(p.appA, "tx.setMicSource", {utf8("source", QStringLiteral("RadioMic"))});
        QVERIFY(called);
        QVERIFY2(accepted(nested), qPrintable(describe(nested)));
        QVERIFY(!accepted(source));
        QCOMPARE(valueOf(source, QStringLiteral("source")).toString(), QStringLiteral("ClientAudio"));
        QVERIFY(p.mox->isMox());
        QVERIFY(!p.core.model->remoteRadioMicKeyActive(p.a.key.fingerprint()));
        disconnect(observation);
    }

    void nestedRadioKeyCannotReplaceOuterSourceOrPendingEpoch()
    {
        Pair p(true);
        p.core.model->transmitModel().setMicSource(MicSource::Pc);
        QVERIFY(accepted(p.send(p.appA, "tx.setMicSource", {utf8("source", QStringLiteral("RadioMic"))})));
        QVERIFY(accepted(p.send(p.appB, "tx.setMicSource", {utf8("source", QStringLiteral("RadioMic"))})));
        RemoteKeying::MicUplink noMic;
        noMic.carriesMic = [](const QByteArray&) { return false; };
        p.core.server->remoteKeying()->setMicUplink(noMic);
        bool nested = false;
        bool outerExempt = false;
        QJsonObject nestedReply;
        p.mox->setMoxCheck([&]() {
            if (!nested) {
                nested = true;
                nestedReply = p.send(p.appB, "tx.key", trigger("screen"));
                outerExempt = !p.core.model->pcCaptureGatesKeyingForTest();
            }
            safety::BandPlanGuard::MoxCheckResult result;
            result.ok = !p.core.model->pcCaptureGatesKeyingForTest();
            result.reason = QStringLiteral("Computer capture required");
            return result;
        });
        const auto key = p.send(p.appA, "tx.key", trigger("screen"));
        QVERIFY2(accepted(key), qPrintable(describe(key)));
        QVERIFY(!accepted(nestedReply));
        QVERIFY(outerExempt);
        QVERIFY(p.core.model->remoteRadioMicKeyActive(p.a.key.fingerprint()));
        QVERIFY(!p.core.model->remoteRadioMicKeyActive(p.b.key.fingerprint()));
        QCOMPARE(p.core.model->keyedBy().deviceId, p.a.key.fingerprint());
        QVERIFY(!p.core.server->remoteKeying()->keyPending());
    }

    void refusedSynchronousAdmissionEndsItsPendingRetryWindow_data()
    {
        QTest::addColumn<QByteArray>("verb");
        QTest::newRow("radio-voice") << QByteArray("tx.key");
        QTest::newRow("generated-tune") << QByteArray("tx.tune");
    }

    void refusedSynchronousAdmissionEndsItsPendingRetryWindow()
    {
        QFETCH(QByteArray, verb);
        Pair p(true);
        QVERIFY(accepted(p.send(p.appA, "tx.setMicSource", {utf8("source", QStringLiteral("RadioMic"))})));
        auto* keying = p.core.server->remoteKeying();
        bool pendingAtPrecheck = false;
        bool endedWithoutPending = false;
        QSignalSpy ended(keying, &RemoteKeying::pendingKeyEnded);
        const auto observation = connect(keying, &RemoteKeying::pendingKeyEnded, this, [&]() {
            endedWithoutPending = !keying->keyPending();
        });
        p.mox->setMoxCheck([&]() {
            pendingAtPrecheck = pendingAtPrecheck || keying->keyPending();
            safety::BandPlanGuard::MoxCheckResult result;
            result.ok = false;
            result.reason = QStringLiteral("Refused synchronous admission");
            return result;
        });
        const auto cleanup = qScopeGuard([&]() { disconnect(observation); p.mox->setMoxCheck({}); });
        const auto result = p.send(p.appA, verb, verb == "tx.key" ? trigger("screen")
                                  : QList<MirrorUpdate>{boolean("on", true)});
        QVERIFY(!accepted(result));
        QVERIFY(pendingAtPrecheck);
        QVERIFY(!keying->keyPending());
        // StationServer's owed amplifier restore waits on this existing
        // completion signal. A refused synchronous attempt never produces
        // a MOX-off event, so its pending window must notify the retry path.
        QCOMPARE(ended.count(), 1);
        QVERIFY(endedWithoutPending);
        QVERIFY(!p.mox->isMox());
        p.mox->setMoxCheck({});
        QVERIFY(accepted(p.send(p.appA, "tx.setMicSource", {utf8("source", QStringLiteral("ClientAudio"))})));
    }

    void twoToneRefusalDefersItsPendingEndUntilAdmissionRetires()
    {
        Pair p(true);
        TxChannel channel(1);
        auto* tt = p.core.model->twoToneController();
        tt->setTxChannel(&channel);
        tt->setSliceModel(p.core.model->sliceById(0));
        tt->setSettleDelaysMs(0, 0);
        auto* keying = p.core.server->remoteKeying();
        int checks = 0;
        bool pendingDuringRefusal = false;
        bool endedWithoutPending = false;
        QSignalSpy ended(keying, &RemoteKeying::pendingKeyEnded);
        const auto observation = connect(keying, &RemoteKeying::pendingKeyEnded, this, [&]() {
            endedWithoutPending = !keying->keyPending();
        });
        p.mox->setMoxCheck([&]() {
            safety::BandPlanGuard::MoxCheckResult result;
            result.ok = ++checks == 1; // admit the generator; refuse its own later MOX attempt
            if (!result.ok) {
                pendingDuringRefusal = keying->keyPending();
                result.reason = QStringLiteral("Two-tone key refused after generator admission");
            }
            return result;
        });
        const auto cleanup = qScopeGuard([&]() {
            disconnect(observation);
            p.mox->setMoxCheck({});
            tt->setActive(false);
            tt->setTxChannel(nullptr);
            p.mox->setMox(false);
        });
        const auto start = p.send(p.appA, "tx.twoTone", {boolean("on", true)});
        QVERIFY(!accepted(start));
        QVERIFY(checks > 1);
        QVERIFY(pendingDuringRefusal);
        QVERIFY(!tt->isActive());
        QVERIFY(!tt->isActivationInFlight());
        QVERIFY(!p.mox->isMox());
        QVERIFY(!keying->keyPending());
        QCOMPARE(ended.count(), 1); // synchronous generator cleanup and scope exit coalesce
        QVERIFY(endedWithoutPending);
        p.mox->setMoxCheck({});
        QVERIFY(accepted(p.send(p.appA, "tx.setMicSource", {utf8("source", QStringLiteral("RadioMic"))})));
    }

    void nestedExplicitRefusalPreservesOuterRadioAdmissionAndNextLocalIsolation()
    {
        Pair p(true);
        QVERIFY(accepted(p.send(p.appA, "tx.setMicSource", {utf8("source", QStringLiteral("RadioMic"))})));
        int outerChecks = 0;
        bool nestedRefused = false;
        bool restoredAttempt = false;
        bool sourceBeforeHardware = false;
        QString outerSession;
        KeyerIdentity other;
        other.deviceId = QByteArray("other-explicit-keyer");
        other.session = QStringLiteral("station:other-explicit");
        p.mox->setMoxCheck([&]() {
            const auto attempt = p.mox->keyAttemptIdentity();
            safety::BandPlanGuard::MoxCheckResult result;
            if (attempt && attempt->deviceId == other.deviceId) {
                nestedRefused = true;
                result.ok = false;
                result.reason = QStringLiteral("Nested explicit request refused");
                return result;
            }
            if (attempt && attempt->deviceId == p.a.key.fingerprint()) { outerSession = attempt->session; }
            if (++outerChecks == 2) {
                p.mox->setMox(true, other);
                const auto restored = p.mox->keyAttemptIdentity();
                restoredAttempt = restored && restored->deviceId == p.a.key.fingerprint()
                    && restored->session == attempt->session;
            }
            result.ok = true;
            return result;
        });
        const auto observation = connect(p.mox, &MoxController::hardwareFlipped, this, [&](bool on) {
            if (on) { sourceBeforeHardware = p.core.model->remoteRadioMicKeyActive(p.a.key.fingerprint()); }
        });
        const auto cleanup = qScopeGuard([&]() {
            disconnect(observation);
            p.mox->setMoxCheck({});
            p.mox->setMox(false);
        });
        const quint32 before = p.core.model->keyingEpoch();
        const auto key = p.send(p.appA, "tx.key", trigger("screen"));
        QVERIFY(nestedRefused);
        QVERIFY(restoredAttempt);
        QVERIFY2(accepted(key), qPrintable(describe(key)));
        QCOMPARE(p.mox->currentKeyer().deviceId, p.a.key.fingerprint());
        QVERIFY(!outerSession.isEmpty());
        QCOMPARE(p.mox->currentKeyer().session, outerSession);
        QVERIFY(sourceBeforeHardware);
        QCOMPARE(p.core.model->keyedBy().epoch, before+1);
        QCOMPARE(epochOf(key), qint64(before+1));
        QVERIFY(!p.mox->keyAttemptIdentity());
        QVERIFY(accepted(p.send(p.appA, "tx.unkey", {int64("epoch", epochOf(key))})));
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
        disconnect(observation);
        p.mox->setMoxCheck({});
        p.holder->release(p.a.key.fingerprint(), QStringLiteral("following local key"));
        p.core.model->transmitModel().setMicSource(MicSource::Pc);
        p.mox->setMoxCheck([&]() {
            safety::BandPlanGuard::MoxCheckResult result;
            result.ok = !p.core.model->pcCaptureGatesKeyingForTest();
            result.reason = QStringLiteral("Local PC capture is unavailable");
            return result;
        });
        p.mox->setMox(true);
        QVERIFY(!p.mox->isMox());
        QVERIFY(!p.core.model->remoteRadioMicKeyActive(p.a.key.fingerprint()));
    }

    void nestedGeneratedStartCannotReplaceOuterRadioAdmission_data()
    {
        QTest::addColumn<QByteArray>("verb");
        QTest::addColumn<bool>("otherSession");
        for (const QByteArray& verb : {QByteArray("tx.tune"), QByteArray("tx.twoTone")}) {
            QTest::newRow((verb+"/same-session").constData()) << verb << false;
            QTest::newRow((verb+"/other-session").constData()) << verb << true;
        }
    }

    void nestedGeneratedStartCannotReplaceOuterRadioAdmission()
    {
        QFETCH(QByteArray, verb);
        QFETCH(bool, otherSession);
        Pair p(true);
        TxChannel channel(1); // no WDSP channel/radio; enough to enable the genuine two-tone controller
        auto* tt = p.core.model->twoToneController();
        QVERIFY(tt);
        tt->setTxChannel(&channel);
        tt->setSliceModel(p.core.model->sliceById(0));
        tt->setSettleDelaysMs(0, 0);
        QVERIFY(accepted(p.send(p.appA, "tx.setMicSource", {utf8("source", QStringLiteral("RadioMic"))})));
        bool entered = false;
        QJsonObject nested;
        QSignalSpy tuneChanges(&p.core.model->transmitModel(), &TransmitModel::tuneChanged);
        QSignalSpy tones(tt, &TwoToneController::twoToneActiveChanged);
        p.mox->setMoxCheck([&]() {
            if (!entered) {
                entered = true;
                nested = p.send(otherSession ? p.appB : p.appA, verb, {boolean("on", true)});
            }
            safety::BandPlanGuard::MoxCheckResult result;
            result.ok = true;
            return result;
        });
        const auto cleanup = qScopeGuard([&]() {
            p.mox->setMoxCheck({});
            tt->setActive(false);
            tt->setTxChannel(nullptr);
            p.mox->setMox(false);
        });
        const quint32 before = p.core.model->keyingEpoch();
        const auto outer = p.send(p.appA, "tx.key", trigger("screen"));
        QVERIFY(entered);
        QVERIFY2(!accepted(nested), qPrintable(describe(nested)));
        QVERIFY(!p.core.model->isTune());
        QVERIFY(!tt->isActivationInFlight());
        QVERIFY(!tt->isActive());
        QCOMPARE(tuneChanges.count(), 0);
        QCOMPARE(tones.count(), 0);
        QVERIFY2(accepted(outer), qPrintable(describe(outer)));
        QCOMPARE(p.core.model->keyedBy().trigger, QByteArray("screen"));
        QCOMPARE(p.core.model->keyedBy().epoch, before+1);
        QCOMPARE(p.mox->currentKeyer().deviceId, p.a.key.fingerprint());
        QVERIFY(p.core.model->remoteRadioMicKeyActive(p.a.key.fingerprint()));
        QVERIFY(!p.core.server->remoteKeying()->keyPending());
        p.mox->setMoxCheck({});
        QVERIFY(accepted(p.send(p.appA, "tx.unkey", {int64("epoch", epochOf(outer))})));
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
        // The same generated request works when it is an ordinary start,
        // and its own off still works; the guard must cover admission only.
        const auto normal = p.send(p.appA, verb, {boolean("on", true)});
        QVERIFY2(accepted(normal), qPrintable(describe(normal)));
        QTRY_VERIFY(p.mox->isMox());
        QVERIFY(accepted(p.send(p.appA, verb, {boolean("on", false)})));
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
        tt->setTxChannel(nullptr);
    }

    // Outer requests/replays use the authenticated wire. Only callback
    // copies enter RemoteKeying directly with that exact session identity:
    // the server's pre-existing nested-dispatch routing is a separate seam.
    void primingCompletionCopyReceivesTheWaitingKeysAuthoritativeEpoch()
    {
        Pair p;
        constexpr quint32 id = 6500;
        std::function<void(bool)> finish;
        int primes = 0;
        int primingEnds = 0;
        RemoteKeying::MicUplink mic;
        mic.carriesMic = [](const QByteArray&) { return true; };
        mic.prime = [&](const QByteArray&, std::function<void(bool)> done) {
            ++primes;
            finish = std::move(done);
        };
        mic.endPriming = [&](const QByteArray&) { ++primingEnds; };
        auto* keying = p.core.server->remoteKeying();
        keying->setMicUplink(mic);
        const auto request = SessionMessages::encode(SessionMessages::commandInvoke("tx.key", id, trigger("screen")));
        p.appA->sendText(request);
        QTRY_VERIFY(finish);
        QVERIFY(resultsWithId(p.appA, id).isEmpty());
        QSignalSpy starts(p.mox, &MoxController::txAboutToBegin);
        int checks = 0;
        bool copied = false;
        bool enteredWhilePending = false;
        std::optional<RemoteKeying::Result> duplicate;
        p.mox->setMoxCheck([&]() {
            ++checks;
            if (!copied) {
                copied = true;
                const auto identity = p.mox->keyAttemptIdentity();
                if (identity) {
                    RemoteKeying::Command copy;
                    copy.verb = RemoteKeying::Verb::Key;
                    copy.deviceId = identity->deviceId;
                    copy.session = identity->session;
                    copy.commandId = id;
                    copy.trigger = "screen";
                    enteredWhilePending = keying->keyPending();
                    keying->handle(copy, [&](const RemoteKeying::Result& result) { duplicate = result; });
                }
            }
            safety::BandPlanGuard::MoxCheckResult result;
            result.ok = true;
            return result;
        });
        const auto cleanup = qScopeGuard([&]() { p.mox->setMoxCheck({}); p.mox->setMox(false); });
        finish(true);
        QTRY_COMPARE(resultsWithId(p.appA, id).size(), 1);
        const auto original = resultsWithId(p.appA, id).first();
        QVERIFY(enteredWhilePending);
        QVERIFY2(accepted(original), qPrintable(describe(original)));
        QVERIFY(duplicate.has_value());
        QVERIFY(duplicate->accepted);
        QCOMPARE(qint64(duplicate->epoch), epochOf(original));
        QVERIFY(duplicate->refusal.isEmpty());
        QVERIFY(duplicate->reason.isEmpty());
        QCOMPARE(epochOf(original), qint64(p.core.model->keyedBy().epoch));
        QCOMPARE(primes, 1);
        QCOMPARE(primingEnds, 1);
        QCOMPARE(starts.count(), 1);
        QCOMPARE(checks, 2); // ordinary initial and commit checks; no duplicate admission
        QCOMPARE(invokeAs(p.appA, "tx.key", id, trigger("screen"), 2), original);
        QCOMPARE(starts.count(), 1);
    }

    void activeTwoToneCopyReceivesTheGeneratedCommandsAuthoritativeEpoch()
    {
        Pair p;
        constexpr quint32 id = 6501;
        TxChannel channel(1);
        auto* tt = p.core.model->twoToneController();
        QVERIFY(tt);
        tt->setTxChannel(&channel);
        tt->setSliceModel(p.core.model->sliceById(0));
        tt->setSettleDelaysMs(0, 0);
        auto* keying = p.core.server->remoteKeying();
        int activeSignals = 0;
        int checks = 0;
        bool enteredWhilePending = false;
        bool copyChangedAdmission = false;
        std::optional<RemoteKeying::Result> duplicate;
        p.mox->setMoxCheck([&]() {
            ++checks;
            safety::BandPlanGuard::MoxCheckResult result;
            result.ok = true;
            return result;
        });
        const auto observation = connect(tt, &TwoToneController::twoToneActiveChanged, this, [&](bool active) {
            if (!active) { return; }
            ++activeSignals;
            RemoteKeying::Command copy;
            copy.verb = RemoteKeying::Verb::TwoTone;
            copy.deviceId = tt->keyer().deviceId;
            copy.session = tt->keyer().session;
            copy.commandId = id;
            const int beforeChecks = checks;
            enteredWhilePending = keying->keyPending();
            keying->handle(copy, [&](const RemoteKeying::Result& result) { duplicate = result; });
            copyChangedAdmission = checks != beforeChecks;
        });
        QSignalSpy starts(p.mox, &MoxController::txAboutToBegin);
        const auto cleanup = qScopeGuard([&]() {
            disconnect(observation);
            p.mox->setMoxCheck({});
            tt->setActive(false);
            tt->setTxChannel(nullptr);
            p.mox->setMox(false);
        });
        const auto original = invokeAs(p.appA, "tx.twoTone", id, {boolean("on", true)});
        QCOMPARE(activeSignals, 1);
        QVERIFY(enteredWhilePending);
        QVERIFY(!copyChangedAdmission);
        QVERIFY2(accepted(original), qPrintable(describe(original)));
        QVERIFY(duplicate.has_value());
        QVERIFY(duplicate->accepted);
        QCOMPARE(qint64(duplicate->epoch), epochOf(original));
        QVERIFY(duplicate->refusal.isEmpty());
        QVERIFY(duplicate->reason.isEmpty());
        QCOMPARE(epochOf(original), qint64(p.core.model->keyedBy().epoch));
        QCOMPARE(starts.count(), 1);
        QCOMPARE(invokeAs(p.appA, "tx.twoTone", id, {boolean("on", true)}, 2), original);
        QCOMPARE(activeSignals, 1);
        QCOMPARE(starts.count(), 1);
    }

    void rememberedLiveKeyCopySurvivesAnotherGeneratedAdmission()
    {
        Pair p;
        constexpr quint32 id = 6502;
        const auto original = invokeAs(p.appA, "tx.key", id, trigger("screen"));
        QVERIFY2(accepted(original), qPrintable(describe(original)));
        TxChannel channel(1);
        auto* tt = p.core.model->twoToneController();
        QVERIFY(tt);
        tt->setTxChannel(&channel);
        tt->setSliceModel(p.core.model->sliceById(0));
        tt->setSettleDelaysMs(0, 0);
        bool replayed = false;
        bool unchangedLiveKey = false;
        int checks = 0;
        bool copyChangedAdmission = false;
        bool enteredWhilePending = false;
        QList<RemoteKeying::Result> duplicates;
        p.mox->setMoxCheck([&]() {
            ++checks;
            safety::BandPlanGuard::MoxCheckResult result;
            result.ok = true;
            return result;
        });
        auto* keying = p.core.server->remoteKeying();
        // This existing gate precedes the generator's legitimate release.
        // The ordinary outer wire still authenticates its session/device.
        keying->setSessionGate([&](const RemoteKeying::Command& command) {
            if (command.verb == RemoteKeying::Verb::TwoTone && command.on && !replayed) {
                replayed = true;
                const auto before = p.core.model->keyedBy();
                const int beforeChecks = checks;
                RemoteKeying::Command copy = command;
                copy.verb = RemoteKeying::Verb::Key;
                copy.commandId = id;
                copy.trigger = "screen";
                enteredWhilePending = keying->keyPending();
                for (int i = 0; i < 2; ++i) {
                    keying->handle(copy, [&](const RemoteKeying::Result& result) { duplicates.append(result); });
                }
                const auto after = p.core.model->keyedBy();
                unchangedLiveKey = p.mox->isMox() && after.deviceId == before.deviceId
                    && after.epoch == before.epoch && after.trigger == before.trigger;
                copyChangedAdmission = checks != beforeChecks;
            }
            return TxRefusal{};
        });
        const auto cleanup = qScopeGuard([&]() {
            keying->setSessionGate({});
            p.mox->setMoxCheck({});
            tt->setActive(false);
            tt->setTxChannel(nullptr);
            p.mox->setMox(false);
        });
        const auto generated = p.send(p.appA, "tx.twoTone", {boolean("on", true)});
        QVERIFY(replayed);
        QVERIFY(enteredWhilePending);
        QVERIFY(unchangedLiveKey);
        QVERIFY(!copyChangedAdmission);
        QCOMPARE(duplicates.size(), 2);
        for (const auto& duplicate : duplicates) {
            QVERIFY(duplicate.accepted);
            QCOMPARE(qint64(duplicate.epoch), epochOf(original));
            QVERIFY(duplicate.refusal.isEmpty());
            QVERIFY(duplicate.reason.isEmpty());
        }
        QVERIFY2(accepted(generated), qPrintable(describe(generated)));
        QVERIFY(epochOf(generated) > epochOf(original));
        QCOMPARE(p.core.model->keyedBy().trigger, QByteArray("twoTone"));
        // The old epoch has now legitimately ended; its later wire replay
        // follows the existing keyEnded contract and starts nothing.
        QVERIFY(refusedWith(invokeAs(p.appA, "tx.key", id, trigger("screen"), 2), TxRefusals::keyEnded()));
    }

    void generatedReplyTeardownDropsRemainingCopiesAndForgetsItsCache_data()
    {
        QTest::addColumn<bool>("forgetInOriginal");
        QTest::newRow("original-reply") << true;
        QTest::newRow("first-joined-reply") << false;
    }

    void generatedReplyTeardownDropsRemainingCopiesAndForgetsItsCache()
    {
        QFETCH(bool, forgetInOriginal);
        Pair p;
        const auto voice = p.send(p.appA, "tx.key", trigger("screen"));
        QVERIFY(accepted(voice));
        const auto identity = p.mox->currentKeyer(); // exact authenticated owner established by the wire
        QVERIFY(accepted(p.send(p.appA, "tx.unkey", {int64("epoch", epochOf(voice))})));
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
        TxChannel channel(1);
        auto* tt = p.core.model->twoToneController();
        QVERIFY(tt);
        tt->setTxChannel(&channel);
        tt->setSliceModel(p.core.model->sliceById(0));
        tt->setSettleDelaysMs(0, 0);
        auto* keying = p.core.server->remoteKeying();
        RemoteKeying::Command command;
        command.verb = RemoteKeying::Verb::TwoTone;
        command.deviceId = identity.deviceId;
        command.session = identity.session;
        command.commandId = p.nextId++;
        int originals = 0;
        int firstCopies = 0;
        int remainingCopies = 0;
        std::optional<RemoteKeying::Result> original;
        bool firstAccepted = false;
        const auto observation = connect(tt, &TwoToneController::twoToneActiveChanged, this, [&](bool active) {
            if (!active) { return; }
            keying->handle(command, [&](const RemoteKeying::Result& result) {
                ++firstCopies;
                firstAccepted = result.accepted;
                if (!forgetInOriginal) { keying->forgetSession(command.session); }
            });
            keying->handle(command, [&](const RemoteKeying::Result&) { ++remainingCopies; });
        });
        const auto cleanup = qScopeGuard([&]() {
            disconnect(observation);
            tt->setActive(false);
            tt->setTxChannel(nullptr);
            p.mox->setMox(false);
        });
        keying->handle(command, [&](const RemoteKeying::Result& result) {
            ++originals;
            original = result;
            if (forgetInOriginal) { keying->forgetSession(command.session); }
        });
        QCOMPARE(originals, 1);
        QVERIFY(original.has_value());
        QVERIFY(original->accepted);
        QCOMPARE(firstCopies, forgetInOriginal ? 0 : 1);
        if (!forgetInOriginal) { QVERIFY(firstAccepted); }
        QCOMPARE(remainingCopies, 0);
        disconnect(observation);
        tt->setActive(false);
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
        // A forgotten result cannot be reinserted by scope cleanup. Once
        // the old generated key ends this id is fresh, not cached keyEnded.
        const auto fresh = keying->handle(command);
        QVERIFY(fresh.accepted);
        QVERIFY(fresh.epoch > original->epoch);
    }

    void malformedSourceReservesItsIdUntilThatAuthenticatedSessionEnds()
    {
        Pair p(true);
        constexpr quint32 id = 6100;
        const auto badArgs = QList<MirrorUpdate>{utf8("source", QStringLiteral("radio"))};
        const quint32 before = p.core.model->keyingEpoch();
        const auto original = invokeAs(p.appA, "tx.setMicSource", id, badArgs);
        QVERIFY(!accepted(original));
        QCOMPARE(invokeAs(p.appA, "tx.setMicSource", id, badArgs, 2), original);
        const auto corrected = invokeAs(p.appA, "tx.setMicSource", id,
                                        {utf8("source", QStringLiteral("RadioMic"))}, 3);
        QVERIFY(!accepted(corrected));
        const auto otherSource = invokeAs(p.appA, "tx.setMicSource", id,
                                          {utf8("source", QStringLiteral("ClientAudio"))}, 4);
        QVERIFY(!accepted(otherSource));
        const auto key = invokeAs(p.appA, "tx.key", id, trigger("screen"), 5);
        QVERIFY(!accepted(key));
        QVERIFY(!p.mox->isMox());
        QCOMPARE(p.core.model->keyingEpoch(), before);
        RemoteKeying::MicUplink closedMic;
        closedMic.carriesMic = [](const QByteArray&) { return false; };
        p.core.server->remoteKeying()->setMicUplink(closedMic);
        // A different request id proves malformed/reused invocations did not
        // quietly change the session's default ClientAudio selection.
        const auto unchanged = invokeAs(p.appA, "tx.key", id+1, trigger("screen"));
        QVERIFY2(refusedWith(unchanged, TxRefusals::micNotConnected()), qPrintable(describe(unchanged)));
        QVERIFY(!p.mox->isMox());
        QCOMPARE(p.core.model->keyingEpoch(), before);
        p.appA->closeLink(QStringLiteral("fresh authenticated session"));
        auto features = kTransmitter;
        features.insert("radioMic", 2);
        auto* fresh = p.core.signIn(p.a, features);
        QVERIFY(admitted(fresh));
        const auto newSource = invokeAs(fresh, "tx.setMicSource", id,
                                        {utf8("source", QStringLiteral("RadioMic"))});
        QVERIFY2(accepted(newSource), qPrintable(describe(newSource)));
        QVERIFY(!p.mox->isMox());
    }

    void sourceCommandRequiresV2AndExactArguments()
    {
        for (const int version : {0, 1, 2}) {
            Core core;
            allowTransmit(core);
            Device a;
            core.pair(a);
            auto features = kTransmitter;
            if (version) { features.insert("radioMic", version); }
            auto* app = core.signIn(a, features);
            QVERIFY(admitted(app));
            const auto offered = latestCapabilityIf(app->received(), QStringLiteral("radioMicVersion"));
            QCOMPARE(offered.has_value(), version > 0);
            if (offered) { QCOMPARE(offered->toInteger(), qint64(version)); }
            const auto result = core.invoke(app, "tx.setMicSource", {utf8("source", QStringLiteral("RadioMic"))});
            QCOMPARE(accepted(result), version == 2);
            if (version == 2) {
                for (const QList<MirrorUpdate>& args : QList<QList<MirrorUpdate>>{
                    {}, {utf8("source", QStringLiteral("radio"))}, {boolean("source", true)},
                    {utf8("source", QStringLiteral("RadioMic")), utf8("extra", QStringLiteral("x"))}}) {
                    const auto invalid = core.invoke(app, "tx.setMicSource", args);
                    QVERIFY(!accepted(invalid));
                }
            }
            QVERIFY(!core.model->moxController()->isMox());
        }
    }

    void sourceRefusedWhileKeyedAndOldCopyCannotOverwriteNewChoice()
    {
        Pair p(true);
        const auto radio = invokeAs(p.appA, "tx.setMicSource", 6000,
                                    {utf8("source", QStringLiteral("RadioMic"))});
        QVERIFY2(accepted(radio), qPrintable(describe(radio)));
        const auto key = p.send(p.appA, "tx.key", trigger("screen"));
        QVERIFY2(accepted(key), qPrintable(describe(key)));
        const auto blocked = p.send(p.appA, "tx.setMicSource",
                                    {utf8("source", QStringLiteral("ClientAudio"))});
        QVERIFY(!accepted(blocked));
        QCOMPARE(valueOf(blocked, QStringLiteral("source")).toString(), QStringLiteral("RadioMic"));
        QVERIFY(p.mox->isMox());
        QVERIFY(accepted(p.send(p.appA, "tx.unkey", {int64("epoch", epochOf(key))})));
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
        const auto client = p.send(p.appA, "tx.setMicSource", {utf8("source", QStringLiteral("ClientAudio"))});
        QVERIFY(accepted(client));
        const auto copy = invokeAs(p.appA, "tx.setMicSource", 6000,
                                  {utf8("source", QStringLiteral("RadioMic"))}, 2);
        QCOMPARE(copy, radio);
        const auto reused = invokeAs(p.appA, "tx.key", 6000, trigger("screen"), 3);
        QVERIFY(!accepted(reused));
        QVERIFY(!p.mox->isMox());
        RemoteKeying::MicUplink noMic;
        noMic.carriesMic = [](const QByteArray&) { return false; };
        p.core.server->remoteKeying()->setMicUplink(noMic);
        const auto retry = p.send(p.appA, "tx.key", trigger("screen"));
        QVERIFY2(refusedWith(retry, TxRefusals::micNotConnected()), qPrintable(describe(retry)));
        QVERIFY(!p.mox->isMox());
    }

    void sourceCannotReplaceClientAudioKeyWaitingForPriming()
    {
        Pair p(true);
        std::function<void(bool)> finish;
        RemoteKeying::MicUplink mic;
        mic.carriesMic = [](const QByteArray&) { return true; };
        mic.prime = [&](const QByteArray&, std::function<void(bool)> done) { finish = std::move(done); };
        p.core.server->remoteKeying()->setMicUplink(mic);
        const quint32 id = p.nextId++;
        p.appA->sendText(SessionMessages::encode(SessionMessages::commandInvoke("tx.key", id, trigger("screen"))));
        QTRY_VERIFY(p.core.server->remoteKeying()->keyPending());
        QVERIFY(!p.mox->isMox());
        const auto selection = p.send(p.appA, "tx.setMicSource", {utf8("source", QStringLiteral("RadioMic"))});
        QVERIFY(!accepted(selection));
        QCOMPARE(valueOf(selection, QStringLiteral("source")).toString(), QStringLiteral("ClientAudio"));
        QVERIFY(!p.mox->isMox());
        QVERIFY(finish);
        finish(false);
        QTRY_VERIFY(!p.core.server->remoteKeying()->keyPending());
        QVERIFY(accepted(p.send(p.appA, "tx.setMicSource", {utf8("source", QStringLiteral("RadioMic"))})));
    }

    void radioInputRejectsRemoteVoiceProgramAndVoxButLinkLossStillStops()
    {
        Pair p(true);
        QVERIFY(accepted(p.send(p.appA, "tx.setMicSource", {utf8("source", QStringLiteral("RadioMic"))})));
        const auto key = p.send(p.appA, "tx.key", trigger("screen"));
        QVERIFY(accepted(key));
        p.core.server->remoteMicStarved(p.a.key.fingerprint(), true);
        QVERIFY(p.mox->isMox());
        p.core.server->remoteMicStarved(p.a.key.fingerprint(), false);
        QVERIFY(accepted(p.send(p.appA, "tx.unkey", {int64("epoch", epochOf(key))})));
        QTRY_COMPARE(p.mox->state(), MoxState::Rx);
        const auto program = p.send(p.appA, "tx.key", trigger("tci"));
        QVERIFY(!accepted(program));
        QCOMPARE(program.value(QStringLiteral("reason")).toString(), remoteRadioProgramReason());
        p.core.model->openRemoteMicLine(p.a.key.fingerprint());
        const quint32 id = p.nextId++;
        p.appA->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "transmit", {boolean("voxEnabled", true)}, id)));
        QTRY_VERIFY(!propertyResult(p.appA, id).isEmpty());
        const auto vox = propertyResult(p.appA, id).value(QStringLiteral("results")).toArray().first().toObject();
        QVERIFY(!accepted(vox));
        QCOMPARE(vox.value(QStringLiteral("reason")).toString(), remoteRadioVoxReason());
        const auto second = p.send(p.appA, "tx.key", trigger("screen"));
        QVERIFY(accepted(second));
        p.appA->closeLink(QStringLiteral("Radio source link lost"));
        QTRY_VERIFY(!p.mox->isMox());
    }

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
