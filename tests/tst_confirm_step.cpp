// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_confirm_step.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 74 (R-IOS-02, R-IOS-30; the several-devices design,
// sections 6.1 to 6.4, 7.3, 7.4 and 10.7, rulings 6.2 to 6.10, design
// ruling 6.5a, rulings 7.4a and 10.2): receivers shared by several devices,
// asked before, told after.
//
// Authorisation first (who may move or take what), over one Core and the
// in-process loopback, several devices signed in by keys made at run time:
//   - a device that does not anchor its receiver has its C-Tune pin
//     request refused with the plain reason naming the anchor;
//   - the anchor's C-Tune move that would leave another device's slice
//     outside the window changes nothing and asks (confirm.request
//     panMove), naming the device and each slice's effect (moves, or closes
//     with no receiver free); proceed moves or closes it and tells its
//     device; cancel changes nothing;
//   - the anchor's band change on a shared receiver is the same question,
//     with the bands in `change`; alone, today's retune; with the anchor's
//     own other slice left outside, today's retune (design ruling 6.5a),
//     and with no receiver free the chooser lists the shared receiver
//     takeable false, naming that slice;
//   - a proceed whose set grew asks again and applies nothing;
//   - a device that does not anchor moves its pan to a free receiver,
//     nobody asked; with none free, the chooser;
//   - every receiver held: the refusal names who holds them, the chooser
//     lists each receiver's slices and devices; proceed closes every other
//     device's slice on the chosen one, tells each owner with Take it back,
//     and Take it back asks the same question the other way;
//   - the slice cap full: the slice chooser, and the same notice;
//   - an older window is never asked: the refusal names who it would
//     affect; a take of its last slice ends it (takenOver); with no slice
//     for it at admission it is refused, retryable.
// Then the readback (ruling 7.4a) for a command and a property write, and
// notices: waiting for an away device and arriving after its
// snapshot.complete with secondsAgo from when they happened; graceEnded for
// a device back after its 180 s.
//
// Slice control plan Task 1: a take whose slice was closed and whose id a
// new slice of the same device took since is refused as changed and closes
// nothing, whether the slice was offered (takeSlice) or shown closing with
// a receiver (takeReceiver).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 74 (R-IOS-02, R-IOS-30),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: checkpoint join (R-IOS-30, R-R3-49): the parity lane's
//               accessory verbs, the Alex transmit antennas and relays and
//               setAlexTxAntenna on the D53 list. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: merge of the trunk into the transmit lane: the Protocol 1
//               rate change runs on the receive lane, so its slice closes
//               when it finishes (QTRY). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: merge of the trunk into the transmit lane: Task 34's holder
//               joins ruling 6.8 (a keyed holder's transmit receiver not
//               takeable) and the transmitter's Core settings (asked while
//               a device holds transmit). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: transmit group fix wave 2 (R-IOS-02): ruling 8.11's freeze
//               on every path (a pan move by the anchor or by the slice's
//               owner, XIT, a stored change confirmed during the radio's
//               PTT). J.J. Boyd (KG4VCF), with AI-assisted implementation
//               via Anthropic Claude Code.
//   2026-09-26: trunk merge of remote transmit (R-IOS-02, R-R3-46): the
//               Alex tab's transmit high-pass switches follow the TX
//               antennas' rule; the OC transmit pins wait on the air
//               whoever holds transmit, and the OC pin actions are taken on
//               it. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
//   2026-09-26: Task 77 fix wave: a chooser's txSlice is the TX mark (M1);
//               the radio's PTT's frozen transmit receiver is not takeable
//               (M5). J.J. Boyd (KG4VCF), with AI-assisted implementation
//               via Anthropic Claude Code.
//   2026-09-28: addendum G-42: the Core's Extended transmit setting needs
//               transmit permission and waits for receive. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: Prevent TX'ing on a different band is a Core setting like
//               Extended. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-28: slice control and shared listening plan Task 1: takes
//               whose slice id was reused by the same device's new slice.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-29: slice control plan Task 8: the held slice is made
//               directly, as a restored layout makes it. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 9: a refused Add lists the slices
//               to listen to; take questions name listeners, ask again when
//               one joins, and every listener is told of the close. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-30: take-over fix wave (M-2): forgetTakeBacks on its own. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include "core/P1RadioConnection.h"
#include "core/StepAttenuatorController.h"
#include "core/WdspEngine.h"
#include "core/StepAttenuatorFacade.h"
#include "core/session/ConfirmStep.h"
#include "core/safety/TransmitHolder.h"
#include "models/RfKitModel.h"
#include "models/TunerModel.h"

namespace {

const QString kWaiting = QStringLiteral("Waiting for you to confirm.");

QList<QJsonObject> messagesOfType(const LoopbackTransport* app, const QString& type)
{
    return ofType(app->received(), type);
}

QJsonObject lastOf(const LoopbackTransport* app, const QString& type)
{
    const QList<QJsonObject> all = messagesOfType(app, type);
    return all.isEmpty() ? QJsonObject{} : all.last();
}

int countOf(const LoopbackTransport* app, const QString& type)
{
    return static_cast<int>(messagesOfType(app, type).size());
}

// The value of `name` in a command.result's `values`, or undefined.
QJsonValue valueOf(const QJsonObject& result, const QString& name)
{
    for (const QJsonValue& v : result.value(QStringLiteral("values")).toArray()) {
        if (v.toObject().value(QStringLiteral("name")).toString() == name) {
            return v.toObject().value(QStringLiteral("value"));
        }
    }
    return {};
}

// A slice's receiver, by slice id.
int streamOf(const Core& core, int sliceId)
{
    const SliceModel* slice = core.model->sliceById(sliceId);
    return slice == nullptr ? -2 : slice->streamIndex();
}

// A property.write of one slice's frequency; waits for its property.result.
QJsonObject writeFrequency(LoopbackTransport* app, int sliceId, double hz, quint32 writeId)
{
    app->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
        ObjectRegistry::keyForSlice(sliceId), {f64("frequency", hz)}, writeId)));
    QJsonObject result;
    const bool answered = QTest::qWaitFor([&]() {
        result = propertyResult(app, writeId);
        return !result.isEmpty();
    }, 5000);
    Q_UNUSED(answered);
    return result;
}

QJsonObject firstResultEntry(const QJsonObject& propertyResultMessage)
{
    return propertyResultMessage.value(QStringLiteral("results")).toArray().first().toObject();
}

// Two devices on one receiver: A (the iPhone) anchors it with slice 0 on
// 7.074 MHz, B (the iPad) has slice 1 on 7.150 MHz, inside A's window. A
// second receiver is free. Receivers are 192 kHz wide (+/- 96 kHz).
struct Shared {
    Core core;
    Device a;
    Device b{QStringLiteral("iPad"), QStringLiteral("tablet")};
    Device c{QStringLiteral("Mac"), QStringLiteral("computer")};
    LoopbackTransport* appA = nullptr;
    LoopbackTransport* appB = nullptr;

    explicit Shared(int receivers = 2, int sliceCap = 5,
                    const QHash<QByteArray, int>& bFeatures = kHolder)
    {
        core.model->configureStreamPool(receivers, sliceCap, 192000);
        core.model->sliceById(0)->setFrequency(7074000.0);
        core.pair(a);
        core.pair(b);
        core.pair(c);
        appA = core.signIn(a);
        appB = core.signIn(b, bFeatures);
        core.model->sliceById(1)->setFrequency(7150000.0);
    }

    int receiver() const { return streamOf(core, 0); }

    // C signs in and takes the other receiver with its slice on 20 m.
    LoopbackTransport* cOnTheOtherReceiver(const QHash<QByteArray, int>& features = kHolder)
    {
        LoopbackTransport* appC = core.signIn(c, features);
        const QList<int> own = core.model->sliceOwnership()->ownedBy(c.key.fingerprint());
        if (!own.isEmpty()) {
            core.model->sliceById(own.first())->setFrequency(14074000.0);
        }
        return appC;
    }

    QJsonObject proceed(LoopbackTransport* app, qint64 id, qint64 choice = -1)
    {
        return core.invoke(app, "confirm.proceed", {int64("id", id), int64("choice", choice)});
    }
};

// Slice control plan Task 9: the feature set of a device that shares
// slices (slice.listen and the rest).
const QHash<QByteArray, int> kShares{{"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 1}};

// A device's id as the link names it.
QString wireOf(const Device& device)
{
    return StationIdentity::toBase64Url(device.key.fingerprint());
}

// The ids in a slice entry's listenerDeviceIds, or {"absent"} without one.
QStringList listenersIn(const QJsonObject& slice)
{
    if (!slice.contains(QStringLiteral("listenerDeviceIds"))) {
        return {QStringLiteral("absent")};
    }
    QStringList ids;
    for (const QJsonValue& v : slice.value(QStringLiteral("listenerDeviceIds")).toArray()) {
        ids.append(v.toString());
    }
    return ids;
}

// The takeReceiver choice for `stream` in `ask`, or an empty object.
QJsonObject choiceForStream(const QJsonObject& ask, int stream)
{
    for (const QJsonValue& v : ask.value(QStringLiteral("choices")).toArray()) {
        if (v.toObject().value(QStringLiteral("streamIndex")).toInt(-1) == stream) {
            return v.toObject();
        }
    }
    return {};
}

// The entry for `sliceId` in a choice's slices, or an empty object.
QJsonObject sliceIn(const QJsonObject& choice, int sliceId)
{
    for (const QJsonValue& v : choice.value(QStringLiteral("slices")).toArray()) {
        if (v.toObject().value(QStringLiteral("sliceId")).toInt(-1) == sliceId) {
            return v.toObject();
        }
    }
    return {};
}

const QString kRadioOnAir = QStringLiteral("The radio is on the air. Try again when it stops.");

// The radio's own PTT keys at once and with no PC microphone (fix wave 2):
// MOX walks with no delays and the radio's microphone carries the audio.
// The slices stay where Shared put them.
void preparePtt(Core& core)
{
    core.server->setRemoteTransmitAllowed(true);
    core.model->moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
    core.model->transmitModel().setMicSourceLocked(false);
    core.model->transmitModel().setMicSource(MicSource::Radio);
}

QJsonObject waitForLast(const LoopbackTransport* app, const QString& type, int after)
{
    const bool arrived = QTest::qWaitFor([&]() { return countOf(app, type) > after; }, 5000);
    Q_UNUSED(arrived);
    return lastOf(app, type);
}

// The desktop, with every receiver in use, asks for a new slice and takes
// `receiver` (the phone's); returns the proceed's result.
QJsonObject takePhoneReceiver(Core& core, LoopbackTransport* appDesk, int receiver)
{
    const int before = countOf(appDesk, QStringLiteral("confirm.request"));
    const QJsonObject refused =
        core.invoke(appDesk, "addSliceOnPan", {utf8("panId", QStringLiteral("pan-d2"))});
    if (refused.value(QStringLiteral("accepted")).toBool(true)) {
        return {};
    }
    const QJsonObject ask = waitForLast(appDesk, QStringLiteral("confirm.request"), before);
    int choice = -1;
    for (const QJsonValue& v : ask.value(QStringLiteral("choices")).toArray()) {
        if (v.toObject().value(QStringLiteral("streamIndex")).toInt() == receiver) {
            choice = v.toObject().value(QStringLiteral("choice")).toInt();
        }
    }
    return core.invoke(appDesk, "confirm.proceed",
                       {int64("id", ask.value(QStringLiteral("id")).toInteger()),
                        int64("choice", choice)});
}

// The index of the first message of `type` in what `app` received, or -1.
int indexOfType(const LoopbackTransport* app, const QString& type)
{
    const QList<QByteArray> all = app->received();
    for (int i = 0; i < all.size(); ++i) {
        if (QJsonDocument::fromJson(all.at(i)).object().value(QStringLiteral("type")).toString()
            == type) {
            return i;
        }
    }
    return -1;
}

// iPhone app Task 75: two devices each on a receiver of their own, both fed
// from the HL2's one ADC. A (the iPhone) on 7.074 MHz, B (the iPad) on
// 14.074 MHz; the Core's attenuator bound.
struct SharedAdc {
    Core core;
    StepAttenuatorController stepAtt;
    Device a;
    Device b{QStringLiteral("iPad"), QStringLiteral("tablet"), QStringLiteral("iPad")};
    LoopbackTransport* appA = nullptr;
    LoopbackTransport* appB = nullptr;
    quint32 nextWriteId = 900;

    explicit SharedAdc(bool withB = true, bool permittedTransmitter = false)
    {
        stepAtt.setTickTimerEnabled(false);
        core.model->setStepAttController(&stepAtt);
        core.model->configureStreamPool(3, 5, 192000);
        core.model->sliceById(0)->setFrequency(7074000.0);
        core.pair(a);
        core.pair(b);
        if (permittedTransmitter) {
            core.server->setRemoteTransmitAllowed(true);
        }
        appA = core.signIn(a, permittedTransmitter ? kTransmitter : kHolder);
        if (withB) {
            appB = core.signIn(b);
            core.model->sliceById(bSlice())->setFrequency(14074000.0);
        }
    }

    int bSlice() const { return core.model->sliceOwnership()->ownedBy(b.key.fingerprint()).first(); }

    // A property.write of stepAtt; waits for its property.result.
    QJsonObject writeStepAtt(LoopbackTransport* app, const char* name, qint64 value,
                             MirrorWireKind kind = MirrorWireKind::Int64)
    {
        const quint32 id = nextWriteId++;
        app->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "stepAtt", {MirrorUpdate{0, QByteArray(name), kind, QVariant(value)}}, id)));
        QJsonObject result;
        const bool answered = QTest::qWaitFor([&]() {
            result = propertyResult(app, id);
            return !result.isEmpty();
        }, 5000);
        Q_UNUSED(answered);
        return result;
    }

    QJsonObject proceed(LoopbackTransport* app, qint64 id)
    {
        return core.invoke(app, "confirm.proceed", {int64("id", id), int64("choice", -1)});
    }

    int attenuation() const { return core.model->stepAttFacade()->attenuationDb(); }

    // Ruling 7.1a (JJ, 2026-09-28): an attenuator change now applies at
    // once and tells; diversity asks first. A property.write of A's own
    // slice (slice 0), whose diversity reaches every receiver on the HL2's
    // one ADC; waits for its property.result.
    QJsonObject writeOwnSlice(LoopbackTransport* app, const MirrorUpdate& update)
    {
        const quint32 id = nextWriteId++;
        app->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            ObjectRegistry::keyForSlice(0), {update}, id)));
        QJsonObject result;
        const bool answered = QTest::qWaitFor([&]() {
            result = propertyResult(app, id);
            return !result.isEmpty();
        }, 5000);
        Q_UNUSED(answered);
        return result;
    }

    QJsonObject writeDiversityGain(LoopbackTransport* app, double db)
    {
        return writeOwnSlice(app, f64("diversityGainDb", db));
    }

    double diversityGain() const { return core.model->sliceById(0)->diversityGainDb(); }
};

// Slice control plan Task 9: the HL2's two receivers held. A (the iPhone)
// has slice 0 on 7.074 MHz; C (the Mac) has its slice on 14.074 MHz on the
// other receiver, and listens to slice 0; B (the iPad) and D (the second
// iPad) each have a slice beside C's. Every device shares slices.
struct Listened {
    Core core;
    Device a;
    Device b{QStringLiteral("iPad"), QStringLiteral("tablet")};
    Device c{QStringLiteral("Mac"), QStringLiteral("computer")};
    Device d{QStringLiteral("Second iPad"), QStringLiteral("tablet"), QStringLiteral("iPad 2")};
    LoopbackTransport* appA = nullptr;
    LoopbackTransport* appB = nullptr;
    LoopbackTransport* appC = nullptr;
    LoopbackTransport* appD = nullptr;

    Listened()
    {
        core.model->configureStreamPool(2, 5, 192000);
        core.model->sliceById(0)->setFrequency(7074000.0);
        core.pair(a);
        core.pair(b);
        core.pair(c);
        core.pair(d);
        appA = core.signIn(a, kShares);
        appC = core.signIn(c, kShares);
        besideC(c, 14074000.0);
        appB = core.signIn(b, kShares);
        besideC(b, 14080000.0);
        appD = core.signIn(d, kShares);
        besideC(d, 14090000.0);
    }

    void besideC(const Device& device, double hz)
    {
        for (int id : core.model->sliceOwnership()->ownedBy(device.key.fingerprint())) {
            core.model->sliceById(id)->setFrequency(hz);
        }
    }

    QList<MirrorUpdate> refOf(int sliceId) const
    {
        return {int64("sliceId", sliceId),
                int64("incarnation",
                      static_cast<qint64>(core.model->sliceOwnership()->incarnation(sliceId)))};
    }

    int aStream() const { return streamOf(core, 0); }

    // B asks for a slice on a new pan and is asked takeReceiver.
    QJsonObject bAsks()
    {
        const int before = countOf(appB, QStringLiteral("confirm.request"));
        const QJsonObject refused =
            core.invoke(appB, "addSliceOnPan", {utf8("panId", QStringLiteral("b-new-pan"))});
        if (refused.value(QStringLiteral("accepted")).toBool(true)) {
            return {};
        }
        return waitForLast(appB, QStringLiteral("confirm.request"), before);
    }

    QJsonObject proceed(const QJsonObject& ask)
    {
        return core.invoke(appB, "confirm.proceed",
                           {int64("id", ask.value(QStringLiteral("id")).toInteger()),
                            int64("choice", choiceForStream(ask, aStream())
                                                .value(QStringLiteral("choice"))
                                                .toInt(-1))});
    }
};

} // namespace

class TstConfirmStep : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(
            QStringLiteral("confirm-step-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    void init() { AppSettings::instance().clear(); }

    // ── Authorisation: the pin and the anchor ────────────────────────────

    void theSetupSharesOneReceiverAnchoredByA()
    {
        Shared s;
        QVERIFY(admitted(s.appA));
        QVERIFY(admitted(s.appB));
        QVERIFY(s.receiver() >= 0);
        QCOMPARE(streamOf(s.core, 1), s.receiver());
        QCOMPARE(s.core.model->sliceOwnership()->anchorOf(s.receiver()), s.a.key.fingerprint());
    }

    void aNonAnchorsPinRequestIsRefusedWithThePlainReason()
    {
        Shared s;
        const QJsonObject refused = s.core.invoke(
            s.appB, "requestStreamCtunPinned",
            {int64("sliceId", 1), MirrorUpdate{0, "pinned", MirrorWireKind::Bool, true}});
        QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
        const QString reason = refused.value(QStringLiteral("reason")).toString();
        QCOMPARE(reason, QStringLiteral("This panadapter shows iPhone's receiver. Its C-Tune "
                                        "setting is iPhone's."));
        QVERIFY(OperatorWording::isPlain(reason));
        QVERIFY(!s.core.model->sliceById(0)->streamCtunPinned());
        // The anchor's own pin goes through.
        const QJsonObject pinned = s.core.invoke(
            s.appA, "requestStreamCtunPinned",
            {int64("sliceId", 0), MirrorUpdate{0, "pinned", MirrorWireKind::Bool, true}});
        QCOMPARE(pinned.value(QStringLiteral("accepted")).toBool(false), true);
    }

    void theAnchorsCtuneMoveAsksNamingBThenMovesItsSlice()
    {
        Shared s;
        const int receiver = s.receiver();
        const double centreBefore = s.core.model->streamAllocator().streamCentreHz(receiver);
        const QJsonObject held = s.core.invoke(
            s.appA, "requestStreamCentre", {int64("sliceId", 0), f64("centreHz", 7000000.0)});
        QCOMPARE(held.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(held.value(QStringLiteral("reason")).toString(), kWaiting);
        QCOMPARE(valueOf(held, QStringLiteral("phase")).toString(),
                 QStringLiteral("needsConfirmation"));
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("panMove"));
        QCOMPARE(ask.value(QStringLiteral("reason")).toString(), kWaiting);
        QCOMPARE(ask.value(QStringLiteral("expiresInMs")).toInteger(), 60000);
        QCOMPARE(ask.value(QStringLiteral("forCommandId")).toInteger(),
                 held.value(QStringLiteral("id")).toInteger());
        const QJsonArray affected = ask.value(QStringLiteral("affected")).toArray();
        QCOMPARE(affected.size(), 1);
        const QJsonObject who = affected.first().toObject();
        QCOMPARE(who.value(QStringLiteral("deviceId")).toString(), s.b.id());
        QCOMPARE(who.value(QStringLiteral("deviceName")).toString(), QStringLiteral("iPad"));
        QCOMPARE(who.value(QStringLiteral("state")).toString(), QStringLiteral("listening"));
        const QJsonObject bSlice = who.value(QStringLiteral("slices")).toArray().first().toObject();
        QCOMPARE(bSlice.value(QStringLiteral("sliceId")).toInt(), 1);
        QCOMPARE(bSlice.value(QStringLiteral("letter")).toString(), QStringLiteral("B"));
        QCOMPARE(bSlice.value(QStringLiteral("effect")).toString(), QStringLiteral("moves"));
        QCOMPARE(bSlice.value(QStringLiteral("streamIndex")).toInt(), receiver);
        QCOMPARE(bSlice.value(QStringLiteral("adc")).toInt(), 0);
        QCOMPARE(bSlice.value(QStringLiteral("mode")).toInt(),
                 static_cast<int>(s.core.model->sliceById(1)->dspMode()));
        // Nothing changed before the answer.
        QCOMPARE(s.core.model->streamAllocator().streamCentreHz(receiver), centreBefore);
        QCOMPARE(streamOf(s.core, 1), receiver);
        QCOMPARE(countOf(s.appB, QStringLiteral("notice")), 0);

        const QJsonObject done = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(done.value(QStringLiteral("accepted")).toBool(false), true);
        // Ruling 7.4a: the command's own answer, as if it had applied at
        // once (requestStreamCentre names the slices on the receiver).
        QCOMPARE(done.value(QStringLiteral("affected")).toArray(),
                 QJsonArray{QStringLiteral("slice:0")});
        QCOMPARE(s.core.model->streamAllocator().streamCentreHz(receiver), 7000000.0);
        QVERIFY(streamOf(s.core, 1) >= 0);
        QVERIFY(streamOf(s.core, 1) != receiver);
        QCOMPARE(s.core.model->sliceById(1)->frequency(), 7150000.0);
        const QJsonObject told = waitForLast(s.appB, QStringLiteral("notice"), 0);
        QCOMPARE(told.value(QStringLiteral("kind")).toString(), QStringLiteral("sliceMoved"));
        QCOMPARE(told.value(QStringLiteral("takeBack")).toBool(true), false);
        QCOMPARE(told.value(QStringLiteral("byName")).toString(), QStringLiteral("iPhone"));
        QCOMPARE(told.value(QStringLiteral("byDeviceId")).toString(), s.a.id());
        QCOMPARE(told.value(QStringLiteral("bySource")).toString(), QStringLiteral("device"));
        QCOMPARE(told.value(QStringLiteral("secondsAgo")).toInteger(), 0);
        QCOMPARE(told.value(QStringLiteral("slices")).toArray().first().toObject()
                     .value(QStringLiteral("letter")).toString(),
                 QStringLiteral("B"));
        QVERIFY(OperatorWording::isPlain(told.value(QStringLiteral("reason")).toString()));
        // A was not told about itself.
        QCOMPARE(countOf(s.appA, QStringLiteral("notice")), 0);
    }

    void withNoReceiverFreeTheOtherSliceClosesAndItsDeviceIsTold()
    {
        Shared s;
        LoopbackTransport* appC = s.cOnTheOtherReceiver();
        QVERIFY(admitted(appC));
        const int receiver = s.receiver();
        QVERIFY(streamOf(s.core, 2) >= 0 && streamOf(s.core, 2) != receiver);
        s.core.invoke(s.appA, "requestStreamCentre",
                      {int64("sliceId", 0), f64("centreHz", 7000000.0)});
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("affected")).toArray().first().toObject()
                     .value(QStringLiteral("slices")).toArray().first().toObject()
                     .value(QStringLiteral("effect")).toString(),
                 QStringLiteral("closes"));
        QVERIFY(s.core.model->sliceById(1) != nullptr);
        const QJsonObject done = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(done.value(QStringLiteral("accepted")).toBool(false), true);
        QVERIFY(s.core.model->sliceById(1) == nullptr);
        const QJsonObject told = waitForLast(s.appB, QStringLiteral("notice"), 0);
        QCOMPARE(told.value(QStringLiteral("kind")).toString(), QStringLiteral("sliceClosed"));
        QCOMPARE(told.value(QStringLiteral("takeBack")).toBool(true), false);
        QCOMPARE(told.value(QStringLiteral("slices")).toArray().first().toObject()
                     .value(QStringLiteral("frequencyHz")).toDouble(),
                 7150000.0);
        // C's receiver was never touched.
        QCOMPARE(s.core.model->sliceById(2)->frequency(), 14074000.0);
    }

    void theAnchorsOwnSliceOutsideStaysRefusedAsToday()
    {
        Shared s;
        const QJsonObject refused = s.core.invoke(
            s.appA, "requestStreamCentre", {int64("sliceId", 0), f64("centreHz", 7300000.0)});
        QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
        QVERIFY(refused.value(QStringLiteral("reason")).toString() != kWaiting);
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), 0);
    }

    void aProceedWhoseSetGrewAsksAgainAndAppliesNothing()
    {
        Shared s;
        const int receiver = s.receiver();
        s.core.invoke(s.appA, "requestStreamCentre",
                      {int64("sliceId", 0), f64("centreHz", 7000000.0)});
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        // C arrives on the same receiver, outside the new window, before
        // the answer.
        LoopbackTransport* appC = s.core.signIn(s.c);
        QVERIFY(admitted(appC));
        const int cSlice = s.core.model->sliceOwnership()->ownedBy(s.c.key.fingerprint()).first();
        s.core.model->sliceById(cSlice)->setFrequency(7160000.0);
        QCOMPARE(streamOf(s.core, cSlice), receiver);

        const QJsonObject again = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(again.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(again.value(QStringLiteral("reason")).toString(), kWaiting);
        const QJsonObject asked = waitForLast(s.appA, QStringLiteral("confirm.request"), 1);
        QVERIFY(asked.value(QStringLiteral("id")).toInteger()
                != ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(asked.value(QStringLiteral("affected")).toArray().size(), 2);
        // Nothing was applied.
        QVERIFY(s.core.model->streamAllocator().streamCentreHz(receiver) != 7000000.0);
        QCOMPARE(streamOf(s.core, 1), receiver);
        QCOMPARE(countOf(s.appB, QStringLiteral("notice")), 0);
        // The old question is gone.
        const QJsonObject stale = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(stale.value(QStringLiteral("accepted")).toBool(true), false);
    }

    // ── The anchor's band change (ruling 6.5, design ruling 6.5a) ────────

    void theAnchorsBandChangeOnASharedReceiverIsAPanMoveWithBands()
    {
        Shared s;
        const int receiver = s.receiver();
        const QJsonObject held = writeFrequency(s.appA, 0, 14074000.0, 501);
        QCOMPARE(firstResultEntry(held).value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(firstResultEntry(held).value(QStringLiteral("reason")).toString(), kWaiting);
        QCOMPARE(s.core.model->sliceById(0)->frequency(), 7074000.0);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("panMove"));
        QCOMPARE(ask.value(QStringLiteral("forWriteId")).toInteger(), 501);
        const QJsonObject change = ask.value(QStringLiteral("change")).toObject();
        QCOMPARE(change.value(QStringLiteral("label")).toString(),
                 QStringLiteral("Receiver %1").arg(receiver + 1));
        QCOMPARE(change.value(QStringLiteral("from")).toString(), QStringLiteral("40 m"));
        QCOMPARE(change.value(QStringLiteral("to")).toString(), QStringLiteral("20 m"));
        QCOMPARE(ask.value(QStringLiteral("affected")).toArray().first().toObject()
                     .value(QStringLiteral("slices")).toArray().first().toObject()
                     .value(QStringLiteral("effect")).toString(),
                 QStringLiteral("moves"));

        // Proceed: the receiver follows A, re-centred on its new frequency;
        // B's slice moves; the answer carries the readback.
        const QJsonObject done = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(done.value(QStringLiteral("accepted")).toBool(false), true);
        QCOMPARE(valueOf(done, QStringLiteral("objectKey")).toString(), QStringLiteral("slice:0"));
        QCOMPARE(valueOf(done, QStringLiteral("frequency")).toDouble(), 14074000.0);
        QCOMPARE(s.core.model->sliceById(0)->frequency(), 14074000.0);
        QCOMPARE(streamOf(s.core, 0), receiver);
        QCOMPARE(s.core.model->streamAllocator().streamCentreHz(receiver), 14074000.0);
        QVERIFY(streamOf(s.core, 1) != receiver);
        QCOMPARE(s.core.model->sliceById(1)->frequency(), 7150000.0);
        QCOMPARE(waitForLast(s.appB, QStringLiteral("notice"), 0).value(QStringLiteral("kind"))
                     .toString(),
                 QStringLiteral("sliceMoved"));
    }

    void cancelChangesNothing()
    {
        Shared s;
        const int receiver = s.receiver();
        writeFrequency(s.appA, 0, 14074000.0, 502);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        const QJsonObject cancelled = s.core.invoke(
            s.appA, "confirm.cancel", {int64("id", ask.value(QStringLiteral("id")).toInteger())});
        QCOMPARE(cancelled.value(QStringLiteral("accepted")).toBool(false), true);
        QCOMPARE(s.core.model->sliceById(0)->frequency(), 7074000.0);
        QCOMPARE(streamOf(s.core, 1), receiver);
        QCOMPARE(countOf(s.appB, QStringLiteral("notice")), 0);
        const QJsonObject after = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(after.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(s.core.model->sliceById(0)->frequency(), 7074000.0);
    }

    void aloneTheBandChangeIsTodaysRetune()
    {
        Shared s;
        // B's slice goes to 20 m first: A is alone on its receiver.
        s.core.model->sliceById(1)->setFrequency(14074000.0);
        QVERIFY(streamOf(s.core, 1) != s.receiver());
        const int receiver = s.receiver();
        const QJsonObject result = writeFrequency(s.appA, 0, 21074000.0, 503);
        QCOMPARE(firstResultEntry(result).value(QStringLiteral("accepted")).toBool(false), true);
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), 0);
        QCOMPARE(streamOf(s.core, 0), receiver);
        QCOMPARE(s.core.model->streamAllocator().streamCentreHz(receiver), 21074000.0);
    }

    void theAnchorsOwnOtherSliceOutsideMakesItTodaysRetune()
    {
        Shared s;
        const int receiver = s.receiver();
        // A's second slice on the shared receiver.
        const QJsonObject added = s.core.invoke(s.appA, "addSlice", {utf8("initialPanId", QString())});
        QVERIFY(added.value(QStringLiteral("accepted")).toBool());
        const int second = s.core.model->sliceOwnership()->ownedBy(s.a.key.fingerprint()).last();
        s.core.model->sliceById(second)->setFrequency(7080000.0);
        QCOMPARE(streamOf(s.core, second), receiver);
        // A free receiver: slice 0 leaves for it, nobody asked.
        const QJsonObject moved = writeFrequency(s.appA, 0, 14074000.0, 504);
        QCOMPARE(firstResultEntry(moved).value(QStringLiteral("accepted")).toBool(false), true);
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), 0);
        QVERIFY(streamOf(s.core, 0) != receiver);
        QCOMPARE(streamOf(s.core, 1), receiver);
        QCOMPARE(streamOf(s.core, second), receiver);
    }

    void withNoneFreeTheChooserListsTheSharedReceiverNotTakeable()
    {
        Shared s;
        const int receiver = s.receiver();
        const QJsonObject added = s.core.invoke(s.appA, "addSlice", {utf8("initialPanId", QString())});
        QVERIFY(added.value(QStringLiteral("accepted")).toBool());
        const int second = s.core.model->sliceOwnership()->ownedBy(s.a.key.fingerprint()).last();
        s.core.model->sliceById(second)->setFrequency(7080000.0);
        LoopbackTransport* appC = s.cOnTheOtherReceiver();
        QVERIFY(admitted(appC));
        const QJsonObject refused = writeFrequency(s.appA, 0, 21074000.0, 505);
        const QJsonObject entry = firstResultEntry(refused);
        QCOMPARE(entry.value(QStringLiteral("accepted")).toBool(true), false);
        const QString reason = entry.value(QStringLiteral("reason")).toString();
        QVERIFY2(reason.endsWith(QStringLiteral("The radio's receivers are in use by iPad and Mac.")),
                 qPrintable(reason));
        QVERIFY(OperatorWording::isPlain(reason));
        QCOMPARE(s.core.model->sliceById(0)->frequency(), 7074000.0);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("takeReceiver"));
        QVERIFY(!ask.contains(QStringLiteral("change")));
        const QJsonArray choices = ask.value(QStringLiteral("choices")).toArray();
        QCOMPARE(choices.size(), 2);
        for (const QJsonValue& v : choices) {
            const QJsonObject choice = v.toObject();
            if (choice.value(QStringLiteral("streamIndex")).toInt() == receiver) {
                QCOMPARE(choice.value(QStringLiteral("takeable")).toBool(true), false);
                QCOMPARE(choice.value(QStringLiteral("why")).toString(),
                         QStringLiteral("Your slice %1 would close.")
                             .arg(QChar(QLatin1Char('A').unicode() + second)));
            } else {
                QCOMPARE(choice.value(QStringLiteral("takeable")).toBool(false), true);
                QCOMPARE(choice.value(QStringLiteral("anchorName")).toString(),
                         QStringLiteral("Mac"));
            }
        }
    }

    // ── A device that does not anchor moves its pan (ruling 6.6) ────────

    void aNonAnchorMovesItsPanToAFreeReceiverNobodyAsked()
    {
        Shared s;
        const int receiver = s.receiver();
        const double centre = s.core.model->streamAllocator().streamCentreHz(receiver);
        const QJsonObject moved = s.core.invoke(
            s.appB, "requestStreamCentre", {int64("sliceId", 1), f64("centreHz", 7120000.0)});
        QCOMPARE(moved.value(QStringLiteral("accepted")).toBool(false), true);
        QVERIFY(streamOf(s.core, 1) != receiver);
        QCOMPARE(s.core.model->streamAllocator().streamCentreHz(streamOf(s.core, 1)), 7120000.0);
        // A's receiver did not move, and nobody was asked or told.
        QCOMPARE(s.core.model->streamAllocator().streamCentreHz(receiver), centre);
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), 0);
        QCOMPARE(countOf(s.appB, QStringLiteral("confirm.request")), 0);
        QCOMPARE(countOf(s.appA, QStringLiteral("notice")), 0);
    }

    void aNonAnchorWithNoReceiverFreeGetsTheChooser()
    {
        Shared s;
        LoopbackTransport* appC = s.cOnTheOtherReceiver();
        QVERIFY(admitted(appC));
        const QJsonObject refused = s.core.invoke(
            s.appB, "requestStreamCentre", {int64("sliceId", 1), f64("centreHz", 7120000.0)});
        QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
        QVERIFY(OperatorWording::isPlain(refused.value(QStringLiteral("reason")).toString()));
        const QJsonObject ask = waitForLast(s.appB, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("takeReceiver"));
        QCOMPARE(ask.value(QStringLiteral("choices")).toArray().size(), 2);
    }

    void aPanMoveThatNoLongerFitsPreservesTheChosenReceiver()
    {
        Shared s;
        LoopbackTransport* appC = s.cOnTheOtherReceiver();
        QVERIFY(admitted(appC));
        const int cSlice = s.core.model->sliceOwnership()->ownedBy(s.c.key.fingerprint()).first();
        const int cStream = streamOf(s.core, cSlice);
        QCOMPARE(s.core.invoke(s.appB, "requestStreamCentre",
                               {int64("sliceId", 1), f64("centreHz", 7120000.0)})
                     .value(QStringLiteral("accepted")).toBool(true), false);
        const QJsonObject ask = waitForLast(s.appB, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("takeReceiver"));
        int choice = -1;
        for (const QJsonValue& value : ask.value(QStringLiteral("choices")).toArray()) {
            const QJsonObject candidate = value.toObject();
            if (candidate.value(QStringLiteral("streamIndex")).toInt() == cStream) {
                choice = candidate.value(QStringLiteral("choice")).toInt();
            }
        }
        QVERIFY(choice >= 0);
        // 7.000 MHz remains in B's current 7.074 MHz window, but lies
        // outside the requested 7.120 MHz window.
        s.core.model->sliceById(1)->setFrequency(7000000.0);
        QCOMPARE(s.core.model->sliceById(1)->frequency(), 7000000.0);
        const int notices = countOf(appC, QStringLiteral("notice"));
        const QJsonObject done = s.proceed(s.appB, ask.value(QStringLiteral("id")).toInteger(),
                                           choice);
        QCOMPARE(done.value(QStringLiteral("accepted")).toBool(true), false);
        QVERIFY(s.core.model->sliceById(cSlice) != nullptr);
        QCOMPARE(s.core.model->sliceOwnership()->mark(cSlice).owner, s.c.key.fingerprint());
        QCOMPARE(streamOf(s.core, cSlice), cStream);
        QCOMPARE(countOf(appC, QStringLiteral("notice")), notices);
    }

    // ── Taking a receiver (section 6.4, rulings 6.7 to 6.9) ──────────────

    void takingAReceiverClosesTheOthersSlicesAndTakeItBackAsksTheOtherWay()
    {
        Shared s;
        // B's slice on 20 m: each device holds one receiver.
        s.core.model->sliceById(1)->setFrequency(14074000.0);
        const int bReceiver = streamOf(s.core, 1);
        QVERIFY(bReceiver >= 0 && bReceiver != s.receiver());
        const QJsonObject refused = s.core.invoke(s.appA, "addSliceOnPan",
                                                  {utf8("panId", QStringLiteral("pan-a2"))});
        QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
        const QString reason = refused.value(QStringLiteral("reason")).toString();
        QVERIFY2(reason.endsWith(QStringLiteral("The radio's receivers are in use by iPad.")),
                 qPrintable(reason));
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("takeReceiver"));
        QCOMPARE(ask.value(QStringLiteral("affected")).toArray().size(), 0);
        int choice = -1;
        const QJsonArray choices = ask.value(QStringLiteral("choices")).toArray();
        for (const QJsonValue& v : choices) {
            const QJsonObject c = v.toObject();
            if (c.value(QStringLiteral("streamIndex")).toInt() == bReceiver) {
                choice = c.value(QStringLiteral("choice")).toInt();
                QCOMPARE(c.value(QStringLiteral("takeable")).toBool(false), true);
                const QJsonObject slice = c.value(QStringLiteral("slices")).toArray().first().toObject();
                QCOMPARE(slice.value(QStringLiteral("letter")).toString(), QStringLiteral("B"));
                QCOMPARE(slice.value(QStringLiteral("deviceName")).toString(), QStringLiteral("iPad"));
                QCOMPARE(slice.value(QStringLiteral("frequencyHz")).toDouble(), 14074000.0);
                const QJsonObject device = c.value(QStringLiteral("devices")).toArray().first().toObject();
                QCOMPARE(device.value(QStringLiteral("deviceId")).toString(), s.b.id());
                QCOMPARE(device.value(QStringLiteral("state")).toString(), QStringLiteral("listening"));
                QCOMPARE(c.value(QStringLiteral("anchorName")).toString(), QStringLiteral("iPad"));
            } else {
                // A's own panadapter's receiver cannot give it a new one.
                QCOMPARE(c.value(QStringLiteral("takeable")).toBool(true), false);
            }
        }
        QVERIFY(choice >= 0);
        QVERIFY(s.core.model->sliceById(1) != nullptr);

        const QJsonObject done = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger(), choice);
        QVERIFY2(done.value(QStringLiteral("accepted")).toBool(false),
                 QJsonDocument(done).toJson().constData());
        // B's slice closed (its letter may be A's new slice's now).
        QVERIFY(s.core.model->sliceOwnership()->ownedBy(s.b.key.fingerprint()).isEmpty());
        const QString newKey = done.value(QStringLiteral("affected")).toArray().first().toString();
        QVERIFY(newKey.startsWith(QStringLiteral("slice:")));
        const int newSlice = newKey.mid(6).toInt();
        QCOMPARE(streamOf(s.core, newSlice), bReceiver);
        QCOMPARE(s.core.model->sliceOwnership()->mark(newSlice).owner, s.a.key.fingerprint());
        const QJsonObject told = waitForLast(s.appB, QStringLiteral("notice"), 0);
        QCOMPARE(told.value(QStringLiteral("kind")).toString(), QStringLiteral("receiverTaken"));
        QCOMPARE(told.value(QStringLiteral("takeBack")).toBool(false), true);
        QCOMPARE(told.value(QStringLiteral("byName")).toString(), QStringLiteral("iPhone"));
        QCOMPARE(told.value(QStringLiteral("slices")).toArray().first().toObject()
                     .value(QStringLiteral("frequencyHz")).toDouble(),
                 14074000.0);
        QVERIFY(OperatorWording::isPlain(told.value(QStringLiteral("reason")).toString()));

        // Take it back: the same question the other way.
        const QJsonObject back = s.core.invoke(
            s.appB, "notice.takeBack", {int64("id", told.value(QStringLiteral("id")).toInteger())});
        QCOMPARE(back.value(QStringLiteral("reason")).toString(), kWaiting);
        const QJsonObject askBack = waitForLast(s.appB, QStringLiteral("confirm.request"), 0);
        QCOMPARE(askBack.value(QStringLiteral("kind")).toString(), QStringLiteral("takeReceiver"));
        const QJsonObject first = askBack.value(QStringLiteral("choices")).toArray().first().toObject();
        QCOMPARE(first.value(QStringLiteral("streamIndex")).toInt(), bReceiver);
        QCOMPARE(first.value(QStringLiteral("slices")).toArray().first().toObject()
                     .value(QStringLiteral("sliceId")).toInt(),
                 newSlice);
        const QJsonObject returned =
            s.proceed(s.appB, askBack.value(QStringLiteral("id")).toInteger(), 0);
        QCOMPARE(returned.value(QStringLiteral("accepted")).toBool(false), true);
        QVERIFY(s.core.model->sliceById(newSlice) == nullptr
                || s.core.model->sliceOwnership()->mark(newSlice).owner != s.a.key.fingerprint());
        const QList<int> bOwns = s.core.model->sliceOwnership()->ownedBy(s.b.key.fingerprint());
        QCOMPARE(bOwns.size(), 1);
        QCOMPARE(s.core.model->sliceById(bOwns.first())->frequency(), 14074000.0);
        const QJsonObject aTold = waitForLast(s.appA, QStringLiteral("notice"), 0);
        QCOMPARE(aTold.value(QStringLiteral("kind")).toString(), QStringLiteral("receiverTaken"));
        QCOMPARE(aTold.value(QStringLiteral("byName")).toString(), QStringLiteral("iPad"));
        // Taken back once: the record is gone.
        const QJsonObject twice = s.core.invoke(
            s.appB, "notice.takeBack", {int64("id", told.value(QStringLiteral("id")).toInteger())});
        QCOMPARE(twice.value(QStringLiteral("accepted")).toBool(true), false);
        QVERIFY(twice.value(QStringLiteral("reason")).toString() != kWaiting);
    }

    void aReceiverChoiceWhoseVictimChangedOwnerAsksAgainWithoutClosingIt()
    {
        Shared s;
        s.core.model->sliceById(1)->setFrequency(14074000.0);
        const int bStream = streamOf(s.core, 1);
        QCOMPARE(s.core.invoke(s.appA, "addSliceOnPan",
                               {utf8("panId", QStringLiteral("pan-a2"))})
                     .value(QStringLiteral("accepted")).toBool(true), false);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        int choice = -1;
        for (const QJsonValue& value : ask.value(QStringLiteral("choices")).toArray()) {
            const QJsonObject candidate = value.toObject();
            if (candidate.value(QStringLiteral("streamIndex")).toInt() == bStream) {
                choice = candidate.value(QStringLiteral("choice")).toInt();
            }
        }
        QVERIFY(choice >= 0);
        s.core.model->sliceOwnership()->setOwner(1, s.c.key.fingerprint());
        const QJsonObject done = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger(),
                                           choice);
        QCOMPARE(done.value(QStringLiteral("reason")).toString(), kWaiting);
        QCOMPARE(streamOf(s.core, 1), bStream);
        QCOMPARE(s.core.model->sliceOwnership()->mark(1).owner, s.c.key.fingerprint());
        QCOMPARE(countOf(s.appB, QStringLiteral("notice")), 0);
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), 2);
    }

    void aTakeBackRestoresTwoSavedSlicesInsideAnIdleReceiversDefaultWindow()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.model->configureStreamPool(2, 5, 192000);
        core.model->sliceById(0)->setFrequency(7074000.0);
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a);
        QVERIFY(admitted(appA));
        QVERIFY(core.invoke(appA, "addSliceOnPan", {utf8("panId", QStringLiteral("pan-0"))})
                    .value(QStringLiteral("accepted")).toBool());
        const QList<int> aBefore = core.model->sliceOwnership()->ownedBy(a.key.fingerprint());
        QCOMPARE(aBefore.size(), 2);
        core.model->sliceById(aBefore.last())->setFrequency(7150000.0);
        core.model->sliceById(aBefore.last())->setLocked(true);
        const int aStream = streamOf(core, aBefore.first());
        QCOMPARE(streamOf(core, aBefore.last()), aStream);

        LoopbackTransport* appB = core.signIn(b, kHolder);
        QVERIFY(admitted(appB));
        const int bFirst = core.model->sliceOwnership()->ownedBy(b.key.fingerprint()).first();
        core.model->sliceById(bFirst)->setFrequency(14074000.0);
        QCOMPARE(core.invoke(appB, "addSliceOnPan", {utf8("panId", QStringLiteral("pan-b2"))})
                     .value(QStringLiteral("accepted")).toBool(true), false);
        const QJsonObject take = waitForLast(appB, QStringLiteral("confirm.request"), 0);
        int choice = -1;
        for (const QJsonValue& value : take.value(QStringLiteral("choices")).toArray()) {
            const QJsonObject candidate = value.toObject();
            if (candidate.value(QStringLiteral("streamIndex")).toInt() == aStream) {
                choice = candidate.value(QStringLiteral("choice")).toInt();
            }
        }
        QVERIFY(choice >= 0);
        const QJsonObject taken = core.invoke(
            appB, "confirm.proceed",
            {int64("id", take.value(QStringLiteral("id")).toInteger()), int64("choice", choice)});
        QVERIFY2(taken.value(QStringLiteral("accepted")).toBool(false),
                 QJsonDocument(taken).toJson().constData());
        const QJsonObject notice = waitForLast(appA, QStringLiteral("notice"), 0);
        QCOMPARE(notice.value(QStringLiteral("slices")).toArray().size(), 2);

        const QJsonObject back = core.invoke(
            appA, "notice.takeBack", {int64("id", notice.value(QStringLiteral("id")).toInteger())});
        QCOMPARE(back.value(QStringLiteral("reason")).toString(), kWaiting);
        const QJsonObject ask = waitForLast(appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("choices")).toArray().first().toObject()
                     .value(QStringLiteral("takeable")).toBool(false), true);
        const QJsonObject restored = core.invoke(
            appA, "confirm.proceed",
            {int64("id", ask.value(QStringLiteral("id")).toInteger()), int64("choice", 0)});
        QVERIFY2(restored.value(QStringLiteral("accepted")).toBool(false),
                 QJsonDocument(restored).toJson().constData());
        const QList<int> aAfter = core.model->sliceOwnership()->ownedBy(a.key.fingerprint());
        QCOMPARE(aAfter.size(), 2);
        QList<double> frequencies;
        bool lockedOn7150 = false;
        for (int id : aAfter) {
            frequencies.append(core.model->sliceById(id)->frequency());
            lockedOn7150 = lockedOn7150
                || (core.model->sliceById(id)->frequency() == 7150000.0
                    && core.model->sliceById(id)->locked());
        }
        QVERIFY(frequencies.contains(7074000.0));
        QVERIFY(frequencies.contains(7150000.0));
        QVERIFY(lockedOn7150);
    }

    void aTakeBackWithoutRoomForEverySavedSlicePreservesTheVictimAndClaim()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.model->configureStreamPool(2, 5, 192000);
        core.model->sliceById(0)->setFrequency(7074000.0);
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a);
        QVERIFY(admitted(appA));
        QVERIFY(core.invoke(appA, "addSliceOnPan", {utf8("panId", QStringLiteral("pan-0"))})
                    .value(QStringLiteral("accepted")).toBool());
        const QList<int> aBefore = core.model->sliceOwnership()->ownedBy(a.key.fingerprint());
        QCOMPARE(aBefore.size(), 2);
        const int aStream = streamOf(core, aBefore.first());
        QCOMPARE(streamOf(core, aBefore.last()), aStream);
        core.model->sliceById(aBefore.last())->setFrequency(7150000.0);
        QCOMPARE(streamOf(core, aBefore.last()), aStream);

        LoopbackTransport* appB = core.signIn(b, kHolder);
        QVERIFY(admitted(appB));
        const int bFirst = core.model->sliceOwnership()->ownedBy(b.key.fingerprint()).first();
        core.model->sliceById(bFirst)->setFrequency(14074000.0);
        QVERIFY(streamOf(core, bFirst) != aStream);
        QCOMPARE(core.invoke(appB, "addSliceOnPan", {utf8("panId", QStringLiteral("pan-b2"))})
                     .value(QStringLiteral("accepted")).toBool(true), false);
        const QJsonObject take = waitForLast(appB, QStringLiteral("confirm.request"), 0);
        int takeChoice = -1;
        for (const QJsonValue& value : take.value(QStringLiteral("choices")).toArray()) {
            const QJsonObject candidate = value.toObject();
            if (candidate.value(QStringLiteral("streamIndex")).toInt() == aStream) {
                takeChoice = candidate.value(QStringLiteral("choice")).toInt();
            }
        }
        QVERIFY(takeChoice >= 0);
        const QJsonObject taken = core.invoke(
            appB, "confirm.proceed",
            {int64("id", take.value(QStringLiteral("id")).toInteger()), int64("choice", takeChoice)});
        QVERIFY2(taken.value(QStringLiteral("accepted")).toBool(false),
                 QJsonDocument(taken).toJson().constData());
        QCOMPARE(core.model->sliceOwnership()->ownedBy(a.key.fingerprint()).size(), 0);
        const QJsonObject notice = waitForLast(appA, QStringLiteral("notice"), 0);
        QCOMPARE(notice.value(QStringLiteral("kind")).toString(), QStringLiteral("receiverTaken"));
        QCOMPARE(notice.value(QStringLiteral("slices")).toArray().size(), 2);

        const int bVictim = taken.value(QStringLiteral("affected")).toArray().first()
                                .toString().mid(6).toInt();
        QCOMPARE(streamOf(core, bVictim), aStream);
        core.model->sliceById(bVictim)->setFrequency(7074000.0);
        core.model->sliceOwnership()->setActive(b.key.fingerprint(), bFirst);
        QList<int> bExtras;
        for (int i = 0; i < 3; ++i) {
            const QJsonObject added = core.invoke(
                appB, "addSlice", {utf8("initialPanId", QString())});
            QVERIFY2(added.value(QStringLiteral("accepted")).toBool(false),
                     QJsonDocument(added).toJson().constData());
            bExtras.append(added.value(QStringLiteral("affected")).toArray().first()
                               .toString().mid(6).toInt());
        }
        QCOMPARE(core.model->slices().size(), 5);
        QCOMPARE(core.model->slicesOnStream(aStream), QList<int>{bVictim});

        const QJsonObject back = core.invoke(
            appA, "notice.takeBack", {int64("id", notice.value(QStringLiteral("id")).toInteger())});
        QCOMPARE(back.value(QStringLiteral("reason")).toString(), kWaiting);
        const QJsonObject ask = waitForLast(appA, QStringLiteral("confirm.request"), 0);
        const int bNotices = countOf(appB, QStringLiteral("notice"));
        const QJsonObject refused = core.invoke(
            appA, "confirm.proceed",
            {int64("id", ask.value(QStringLiteral("id")).toInteger()), int64("choice", 0)});
        QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(core.model->sliceOwnership()->ownedBy(a.key.fingerprint()).size(), 0);
        QCOMPARE(core.model->slicesOnStream(aStream), QList<int>{bVictim});
        QCOMPARE(core.model->sliceOwnership()->mark(bVictim).owner, b.key.fingerprint());
        QCOMPARE(countOf(appB, QStringLiteral("notice")), bNotices);
        // Refusal leaves the saved claim available for another attempt.
        const QJsonObject again = core.invoke(
            appA, "notice.takeBack", {int64("id", notice.value(QStringLiteral("id")).toInteger())});
        QCOMPARE(again.value(QStringLiteral("reason")).toString(), kWaiting);

        // Clear the slice-cap pressure. A slice already owned by A keeps
        // the narrow target receiver active after B's victim would close.
        const QJsonObject secondAsk = waitForLast(appA, QStringLiteral("confirm.request"), 1);
        QVERIFY(core.invoke(appA, "confirm.cancel",
                            {int64("id", secondAsk.value(QStringLiteral("id")).toInteger())})
                    .value(QStringLiteral("accepted")).toBool());
        for (int id : bExtras) {
            QVERIFY(core.invoke(appB, "removeSlice", {int64("sliceId", id)})
                        .value(QStringLiteral("accepted")).toBool());
        }
        const QJsonObject addedA = core.invoke(appA, "addSlice", {utf8("initialPanId", QString())});
        QVERIFY2(addedA.value(QStringLiteral("accepted")).toBool(false),
                 QJsonDocument(addedA).toJson().constData());
        const int aExtra = addedA.value(QStringLiteral("affected")).toArray().first()
                               .toString().mid(6).toInt();
        core.model->sliceById(aExtra)->setFrequency(7074000.0);
        QCOMPARE(streamOf(core, aExtra), aStream);
        QVERIFY(core.model->setStreamSampleRate(aStream, 96000));
        QCOMPARE(core.model->streamAllocator().streamSampleRateHz(aStream), 96000);

        const QJsonObject backWithNarrowWindow = core.invoke(
            appA, "notice.takeBack", {int64("id", notice.value(QStringLiteral("id")).toInteger())});
        QCOMPARE(backWithNarrowWindow.value(QStringLiteral("reason")).toString(), kWaiting);
        const QJsonObject narrowAsk = waitForLast(appA, QStringLiteral("confirm.request"), 2);
        const int noticesBeforeNarrow = countOf(appB, QStringLiteral("notice"));
        const QJsonObject narrowRefusal = core.invoke(
            appA, "confirm.proceed",
            {int64("id", narrowAsk.value(QStringLiteral("id")).toInteger()), int64("choice", 0)});
        QCOMPARE(narrowRefusal.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(core.model->sliceOwnership()->ownedBy(a.key.fingerprint()), QList<int>{aExtra});
        QCOMPARE(core.model->sliceOwnership()->mark(bVictim).owner, b.key.fingerprint());
        QCOMPARE(countOf(appB, QStringLiteral("notice")), noticesBeforeNarrow);
        QCOMPARE(core.invoke(appA, "notice.takeBack",
                             {int64("id", notice.value(QStringLiteral("id")).toInteger())})
                     .value(QStringLiteral("reason")).toString(), kWaiting);
    }

    // Fix wave C2 (ruling 5.2, its last paragraph): the phone leaves last,
    // so its slice is held for it; the desktop then takes that receiver.
    // Nobody is there to ask or tell, so the closed slice is saved in the
    // phone's layout store, and its next sign-in restores it with its
    // settings.
    void aHeldSliceATakeClosesIsSavedForItsOwnerAndRestoredAtItsReturn()
    {
        Core core;
        Device phone;
        Device desktop(QStringLiteral("Mac"), QStringLiteral("computer"));
        core.model->configureStreamPool(2, 5, 192000);
        core.model->sliceById(0)->setFrequency(7074000.0);
        core.pair(phone);
        core.pair(desktop);
        // Slice control plan Task 8: a device that leaves no longer has its
        // slice held for it (Q12); a hold comes from a layout restored after
        // a restart (ruling 5.3), made here directly.
        core.model->sliceById(0)->setAfGain(17);
        core.model->sliceOwnership()->hold(0, phone.key.fingerprint());
        QCOMPARE(core.model->sliceOwnership()->mark(0).heldFor, phone.key.fingerprint());
        const int heldReceiver = streamOf(core, 0);
        QVERIFY(heldReceiver >= 0);

        LoopbackTransport* appDesk = core.signIn(desktop);
        QVERIFY(admitted(appDesk));
        const int deskSlice =
            core.model->sliceOwnership()->ownedBy(desktop.key.fingerprint()).first();
        core.model->sliceById(deskSlice)->setFrequency(14074000.0);
        QVERIFY(streamOf(core, deskSlice) >= 0 && streamOf(core, deskSlice) != heldReceiver);

        // Every receiver is in use: the desktop takes the held slice's.
        QCOMPARE(core.invoke(appDesk, "addSliceOnPan", {utf8("panId", QStringLiteral("pan-d2"))})
                     .value(QStringLiteral("accepted")).toBool(true),
                 false);
        const QJsonObject ask = waitForLast(appDesk, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("takeReceiver"));
        int choice = -1;
        for (const QJsonValue& v : ask.value(QStringLiteral("choices")).toArray()) {
            if (v.toObject().value(QStringLiteral("streamIndex")).toInt() == heldReceiver) {
                choice = v.toObject().value(QStringLiteral("choice")).toInt();
            }
        }
        QVERIFY(choice >= 0);
        const QJsonObject done = core.invoke(
            appDesk, "confirm.proceed",
            {int64("id", ask.value(QStringLiteral("id")).toInteger()), int64("choice", choice)});
        QVERIFY2(done.value(QStringLiteral("accepted")).toBool(false),
                 QJsonDocument(done).toJson().constData());
        QVERIFY(core.model->sliceOwnership()->heldFor(phone.key.fingerprint()).isEmpty());

        // Saved for the phone, with its settings.
        const QString mac = core.model->currentRadioMac();
        const QList<SavedSlice> saved =
            DeviceLayoutStore::load(AppSettings::instance(), mac, phone.key.fingerprint());
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved.first().id, 0);
        QCOMPARE(saved.first().frequencyHz, 7074000.0);
        QCOMPARE(saved.first().settings.value(QStringLiteral("Slice/AfGain")), QStringLiteral("17"));

        // The desktop closes its new slice, freeing the receiver.
        const QString newKey = done.value(QStringLiteral("affected")).toArray().first().toString();
        QVERIFY(newKey.startsWith(QStringLiteral("slice:")));
        QVERIFY(core.invoke(appDesk, "removeSlice", {int64("sliceId", newKey.mid(6).toInt())})
                    .value(QStringLiteral("accepted")).toBool());

        // The phone signs in again and has its slice back.
        LoopbackTransport* back = core.signIn(phone);
        QVERIFY(admitted(back));
        const QList<int> own = core.model->sliceOwnership()->ownedBy(phone.key.fingerprint());
        QCOMPARE(own.size(), 1);
        QCOMPARE(core.model->sliceById(own.first())->frequency(), 7074000.0);
        QCOMPARE(core.model->sliceById(own.first())->afGain(), 17);
        QVERIFY(core.server->slicesNotRestored(phone.key.fingerprint()).isEmpty());
        QVERIFY(DeviceLayoutStore::load(AppSettings::instance(), mac, phone.key.fingerprint())
                    .isEmpty());
    }

    // Fix wave 2 (the re-review's second out-of-scope item): the phone is
    // away (dropped, inside its 180 s) when the desktop takes its receiver.
    // Its slice lives in the waiting Take it back notice; when the 180 s
    // end first, the slice is saved in the phone's layout store, and its
    // next sign-in restores it, with the notice arriving without Take it
    // back.
    void aSliceTakenFromAnAwayDeviceIsSavedWhenIts180SecondsEnd()
    {
        Core core;
        Device phone;
        Device desktop(QStringLiteral("Mac"), QStringLiteral("computer"));
        core.model->configureStreamPool(2, 5, 192000);
        core.model->sliceById(0)->setFrequency(7074000.0);
        core.pair(phone);
        core.pair(desktop);
        LoopbackTransport* appPhone = core.signIn(phone);
        QVERIFY(admitted(appPhone));
        core.model->sliceById(0)->setAfGain(19);
        LoopbackTransport* appDesk = core.signIn(desktop);
        QVERIFY(admitted(appDesk));
        const int deskSlice =
            core.model->sliceOwnership()->ownedBy(desktop.key.fingerprint()).first();
        core.model->sliceById(deskSlice)->setFrequency(14074000.0);
        const int phoneReceiver = streamOf(core, 0);
        QVERIFY(phoneReceiver >= 0 && streamOf(core, deskSlice) != phoneReceiver);

        core.now = 1000;
        appPhone->closeLink(QStringLiteral("lost"));
        QTRY_COMPARE(core.sessions().entry(phone.key.fingerprint())->state,
                     DeviceSessionRegistry::State::Away);
        QCOMPARE(core.model->sliceOwnership()->mark(0).owner, phone.key.fingerprint());

        // Every receiver is in use: the desktop takes the away phone's.
        QCOMPARE(core.invoke(appDesk, "addSliceOnPan", {utf8("panId", QStringLiteral("pan-d2"))})
                     .value(QStringLiteral("accepted")).toBool(true),
                 false);
        const QJsonObject ask = waitForLast(appDesk, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("takeReceiver"));
        int choice = -1;
        for (const QJsonValue& v : ask.value(QStringLiteral("choices")).toArray()) {
            if (v.toObject().value(QStringLiteral("streamIndex")).toInt() == phoneReceiver) {
                choice = v.toObject().value(QStringLiteral("choice")).toInt();
            }
        }
        QVERIFY(choice >= 0);
        const QJsonObject done = core.invoke(
            appDesk, "confirm.proceed",
            {int64("id", ask.value(QStringLiteral("id")).toInteger()), int64("choice", choice)});
        QVERIFY2(done.value(QStringLiteral("accepted")).toBool(false),
                 QJsonDocument(done).toJson().constData());
        QVERIFY(core.model->sliceOwnership()->ownedBy(phone.key.fingerprint()).isEmpty());
        // Inside the 180 s the slice is the notice's, for Take it back.
        const QString mac = core.model->currentRadioMac();
        QVERIFY(DeviceLayoutStore::load(AppSettings::instance(), mac, phone.key.fingerprint())
                    .isEmpty());

        // The 180 s end: saved, with its settings.
        core.now = 1000 + DeviceSessionRegistry::kGraceMs;
        QCOMPARE(core.sessions().expireAway().size(), 1);
        const QList<SavedSlice> saved =
            DeviceLayoutStore::load(AppSettings::instance(), mac, phone.key.fingerprint());
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved.first().frequencyHz, 7074000.0);
        QCOMPARE(saved.first().settings.value(QStringLiteral("Slice/AfGain")), QStringLiteral("19"));

        // The desktop closes its new slice, freeing the receiver; the phone
        // signs in again and has its slice back.
        const QString newKey = done.value(QStringLiteral("affected")).toArray().first().toString();
        QVERIFY(newKey.startsWith(QStringLiteral("slice:")));
        QVERIFY(core.invoke(appDesk, "removeSlice", {int64("sliceId", newKey.mid(6).toInt())})
                    .value(QStringLiteral("accepted")).toBool());
        core.now = 1000 + DeviceSessionRegistry::kGraceMs + 10000;
        LoopbackTransport* back = core.signIn(phone);
        QVERIFY(admitted(back));
        const QList<int> own = core.model->sliceOwnership()->ownedBy(phone.key.fingerprint());
        QCOMPARE(own.size(), 1);
        QCOMPARE(core.model->sliceById(own.first())->frequency(), 7074000.0);
        QCOMPARE(core.model->sliceById(own.first())->afGain(), 19);
        QVERIFY(DeviceLayoutStore::load(AppSettings::instance(), mac, phone.key.fingerprint())
                    .isEmpty());
        // The taken notice still arrives, without Take it back.
        QTRY_VERIFY(countOf(back, QStringLiteral("notice")) >= 2);
        for (const QJsonObject& notice : ofType(back->received(), QStringLiteral("notice"))) {
            QCOMPARE(notice.value(QStringLiteral("takeBack")).toBool(true), false);
        }
    }

    // Fix wave 3 (the re-review's second out-of-scope item, ruling 7.4):
    // the desktop takes the phone's receiver while the phone is here, so
    // the phone's notice, with Take it back, is delivered. The phone then
    // drops; when that away period's 180 s end, Take it back ends and the
    // slice is saved, and the phone's next sign-in restores it.
    void aDeliveredTakeBackEndsAndIsSavedWhenTheNextAwayPeriodEnds()
    {
        Core core;
        Device phone;
        Device desktop(QStringLiteral("Mac"), QStringLiteral("computer"));
        core.model->configureStreamPool(2, 5, 192000);
        core.model->sliceById(0)->setFrequency(7074000.0);
        core.pair(phone);
        core.pair(desktop);
        LoopbackTransport* appPhone = core.signIn(phone);
        QVERIFY(admitted(appPhone));
        core.model->sliceById(0)->setAfGain(23);
        LoopbackTransport* appDesk = core.signIn(desktop);
        QVERIFY(admitted(appDesk));
        const int deskSlice =
            core.model->sliceOwnership()->ownedBy(desktop.key.fingerprint()).first();
        core.model->sliceById(deskSlice)->setFrequency(14074000.0);
        const int phoneReceiver = streamOf(core, 0);
        QVERIFY(phoneReceiver >= 0 && streamOf(core, deskSlice) != phoneReceiver);

        const QJsonObject done = takePhoneReceiver(core, appDesk, phoneReceiver);
        QVERIFY2(done.value(QStringLiteral("accepted")).toBool(false),
                 QJsonDocument(done).toJson().constData());
        const QJsonObject told = waitForLast(appPhone, QStringLiteral("notice"), 0);
        QCOMPARE(told.value(QStringLiteral("takeBack")).toBool(false), true);
        const qint64 noticeId = told.value(QStringLiteral("id")).toInteger();

        core.now = 1000;
        appPhone->closeLink(QStringLiteral("lost"));
        QTRY_COMPARE(core.sessions().entry(phone.key.fingerprint())->state,
                     DeviceSessionRegistry::State::Away);
        const QString mac = core.model->currentRadioMac();
        QVERIFY(DeviceLayoutStore::load(AppSettings::instance(), mac, phone.key.fingerprint())
                    .isEmpty());
        core.now = 1000 + DeviceSessionRegistry::kGraceMs;
        QCOMPARE(core.sessions().expireAway().size(), 1);
        const QList<SavedSlice> saved =
            DeviceLayoutStore::load(AppSettings::instance(), mac, phone.key.fingerprint());
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved.first().frequencyHz, 7074000.0);
        QCOMPARE(saved.first().settings.value(QStringLiteral("Slice/AfGain")), QStringLiteral("23"));

        const QString newKey = done.value(QStringLiteral("affected")).toArray().first().toString();
        QVERIFY(core.invoke(appDesk, "removeSlice", {int64("sliceId", newKey.mid(6).toInt())})
                    .value(QStringLiteral("accepted")).toBool());
        core.now = 1000 + DeviceSessionRegistry::kGraceMs + 10000;
        LoopbackTransport* back = core.signIn(phone);
        QVERIFY(admitted(back));
        const QList<int> own = core.model->sliceOwnership()->ownedBy(phone.key.fingerprint());
        QCOMPARE(own.size(), 1);
        QCOMPARE(core.model->sliceById(own.first())->frequency(), 7074000.0);
        QCOMPARE(core.model->sliceById(own.first())->afGain(), 23);
        // Take it back ended with the away period.
        const QJsonObject late = core.invoke(back, "notice.takeBack", {int64("id", noticeId)});
        QCOMPARE(late.value(QStringLiteral("accepted")).toBool(true), false);
        QVERIFY(late.value(QStringLiteral("reason")).toString() != kWaiting);
    }

    // Fix wave 3 (the re-review's Minor 2): the 180 s end while no radio is
    // connected. Nothing can be saved, so Take it back is kept: the phone's
    // notice arrives with it after the radio is back. When its next away
    // period ends with the radio connected, the slice is saved.
    void aTakeBackIsKeptWhenThe180SecondsEndWithNoRadioToSaveIn()
    {
        Core core;
        Device phone;
        Device desktop(QStringLiteral("Mac"), QStringLiteral("computer"));
        core.model->configureStreamPool(2, 5, 192000);
        core.model->sliceById(0)->setFrequency(7074000.0);
        core.pair(phone);
        core.pair(desktop);
        LoopbackTransport* appPhone = core.signIn(phone);
        QVERIFY(admitted(appPhone));
        LoopbackTransport* appDesk = core.signIn(desktop);
        QVERIFY(admitted(appDesk));
        const int deskSlice =
            core.model->sliceOwnership()->ownedBy(desktop.key.fingerprint()).first();
        core.model->sliceById(deskSlice)->setFrequency(14074000.0);
        const int phoneReceiver = streamOf(core, 0);

        core.now = 1000;
        appPhone->closeLink(QStringLiteral("lost"));
        QTRY_COMPARE(core.sessions().entry(phone.key.fingerprint())->state,
                     DeviceSessionRegistry::State::Away);
        QVERIFY(takePhoneReceiver(core, appDesk, phoneReceiver)
                    .value(QStringLiteral("accepted")).toBool(false));
        const QString mac = core.model->currentRadioMac();
        QVERIFY(!mac.isEmpty());

        core.model->setConnectionStateForTest(ConnectionState::Disconnected);
        core.now = 1000 + DeviceSessionRegistry::kGraceMs;
        QCOMPARE(core.sessions().expireAway().size(), 1);
        core.model->setConnectionStateForTest(ConnectionState::Connected);
        QVERIFY(DeviceLayoutStore::load(AppSettings::instance(), mac, phone.key.fingerprint())
                    .isEmpty());

        core.now = 1000 + DeviceSessionRegistry::kGraceMs + 10000;
        LoopbackTransport* back = core.signIn(phone);
        QVERIFY(admitted(back));
        QJsonObject taken;
        QTRY_VERIFY([&]() {
            for (const QJsonObject& n : ofType(back->received(), QStringLiteral("notice"))) {
                if (n.value(QStringLiteral("kind")).toString() == QStringLiteral("receiverTaken")) {
                    taken = n;
                }
            }
            return !taken.isEmpty();
        }());
        QCOMPARE(taken.value(QStringLiteral("takeBack")).toBool(false), true);

        // Its next away period ends with the radio connected: saved.
        core.now += 1000;
        const qint64 leftAt = core.now;
        back->closeLink(QStringLiteral("lost"));
        QTRY_COMPARE(core.sessions().entry(phone.key.fingerprint())->state,
                     DeviceSessionRegistry::State::Away);
        core.now = leftAt + DeviceSessionRegistry::kGraceMs;
        QCOMPARE(core.sessions().expireAway().size(), 1);
        // Saved beside the slice the phone was given at its return (which
        // its own release saves as before).
        const QList<SavedSlice> saved =
            DeviceLayoutStore::load(AppSettings::instance(), mac, phone.key.fingerprint());
        const auto takenSaved = std::count_if(saved.cbegin(), saved.cend(), [](const SavedSlice& x) {
            return x.frequencyHz == 7074000.0;
        });
        QCOMPARE(takenSaved, 1);
        // And Take it back is over.
        QCOMPARE(core.invoke(core.signIn(phone), "notice.takeBack",
                             {int64("id", taken.value(QStringLiteral("id")).toInteger())})
                     .value(QStringLiteral("accepted")).toBool(true),
                 false);
    }

    // Fix wave I2: A's question names A's slice 0. B takes that slice
    // (the slice cap is full) and B's new slice gets the lowest free id,
    // 0. A's proceed is refused as changed, and B's slice is untouched.
    void aProceedWhoseSliceWasTakenAndItsIdReusedIsRefused()
    {
        Shared s(2, 2);
        QCOMPARE(s.core.model->slices().size(), 2);
        QCOMPARE(s.core.model->sliceOwnership()->mark(0).owner, s.a.key.fingerprint());
        const quint32 writeId = 1901;
        s.appA->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "slice:0",
            // Ruling 7.1a: diversity asks first (a blanker now applies at
            // once and tells).
            {MirrorUpdate{0, "diversityEnabled", MirrorWireKind::Bool, QVariant(true)}},
            writeId)));
        const QJsonObject asked = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(asked.value(QStringLiteral("kind")).toString(), QStringLiteral("sharedSetting"));
        QCOMPARE(s.core.model->sliceById(0)->diversityEnabled(), false);

        // B takes A's slice 0; B's new slice takes id 0.
        QCOMPARE(s.core.invoke(s.appB, "addSlice", {utf8("initialPanId", QString())})
                     .value(QStringLiteral("accepted")).toBool(true),
                 false);
        const QJsonObject take = waitForLast(s.appB, QStringLiteral("confirm.request"), 0);
        QCOMPARE(take.value(QStringLiteral("kind")).toString(), QStringLiteral("takeSlice"));
        QCOMPARE(take.value(QStringLiteral("choices")).toArray().first().toObject()
                     .value(QStringLiteral("sliceId")).toInt(),
                 0);
        QVERIFY(s.proceed(s.appB, take.value(QStringLiteral("id")).toInteger(), 0)
                    .value(QStringLiteral("accepted")).toBool(false));
        QCOMPARE(s.core.model->sliceOwnership()->mark(0).owner, s.b.key.fingerprint());
        QCOMPARE(s.core.model->sliceById(0)->diversityEnabled(), false);

        // A proceeds: refused, and B's slice 0 keeps diversity off.
        const QJsonObject done = s.proceed(s.appA, asked.value(QStringLiteral("id")).toInteger());
        QCOMPARE(done.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(done.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("That setting changed since you asked. Make the change again."));
        QCOMPARE(s.core.model->sliceById(0)->diversityEnabled(), false);
        QCOMPARE(s.core.model->sliceOwnership()->mark(0).owner, s.b.key.fingerprint());
    }

    // Fix wave I2: a slice a question names that passes to another owner
    // drops the question, even when it is still live, and ownership is
    // checked again at proceed.
    void aProceedWhoseSliceChangedOwnerIsRefused()
    {
        Shared s;
        const quint32 writeId = 1902;
        s.appA->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "slice:0",
            // Ruling 7.1a: diversity asks first (a blanker now applies at
            // once and tells).
            {MirrorUpdate{0, "diversityEnabled", MirrorWireKind::Bool, QVariant(true)}},
            writeId)));
        const QJsonObject asked = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(asked.value(QStringLiteral("kind")).toString(), QStringLiteral("sharedSetting"));
        // Slice 0 passes to B (as a take-back or adoption would move it).
        s.core.model->sliceOwnership()->setOwner(0, s.b.key.fingerprint());
        const QJsonObject done = s.proceed(s.appA, asked.value(QStringLiteral("id")).toInteger());
        QCOMPARE(done.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(done.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("That setting changed since you asked. Make the change again."));
        QCOMPARE(s.core.model->sliceById(0)->diversityEnabled(), false);
    }

    // Slice control plan Task 1: A is offered B's slice 1; B closes it and
    // makes a new slice, which takes id 1 in B's own name. The same letter
    // and the same owner, but not the slice A was shown: refused as
    // changed, and nothing closes.
    void aSliceTakeWhoseSliceWasClosedAndMadeAgainForTheSameDeviceIsRefused()
    {
        Shared s(2, 2);
        const QByteArray bKey = s.b.key.fingerprint();
        QCOMPARE(s.core.model->sliceOwnership()->ownedBy(bKey), QList<int>{1});
        const QJsonObject refused =
            s.core.invoke(s.appA, "addSlice", {utf8("initialPanId", QString())});
        QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("takeSlice"));
        QCOMPARE(ask.value(QStringLiteral("choices")).toArray().first().toObject()
                     .value(QStringLiteral("sliceId")).toInt(),
                 1);
        const quint64 asked = s.core.model->sliceOwnership()->incarnation(1);

        RadioModel* model = s.core.model.get();
        const QString pan = model->sliceById(1)->panKey();
        model->removeSlice(1);
        int reused = -1;
        {
            const SliceOwnership::CreatorScope creator(model->sliceOwnership(), bKey);
            reused = model->addSlice(pan);
        }
        QCOMPARE(reused, 1);
        QCOMPARE(model->sliceOwnership()->mark(1).subject(), bKey);
        const quint64 now = model->sliceOwnership()->incarnation(1);
        QVERIFY(now != 0);
        QVERIFY(now != asked);

        const QJsonObject done = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger(), 0);
        QCOMPARE(done.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(done.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("What this change reaches has changed. Make the change again."));
        QCOMPARE(model->slices().size(), 2);
        QCOMPARE(model->sliceOwnership()->ownedBy(bKey), QList<int>{1});
        QCOMPARE(model->sliceOwnership()->incarnation(1), now);
        QCOMPARE(model->sliceOwnership()->ownedBy(s.a.key.fingerprint()), QList<int>{0});
        QTest::qWait(2 * StationServer::kDefaultDeltaFlushMs);
        QCOMPARE(countOf(s.appB, QStringLiteral("notice")), 0);
    }

    // Slice control fix wave (minor): A is offered B's slice 1; control of
    // it passes to C and back to B. The same letter, incarnation and
    // controller, but C now listens to it and was never shown: its control
    // revision moved, so nothing closes and A is asked again.
    void aSliceTakeWhoseSliceChangedHandsAndBackIsNotApplied()
    {
        Shared s(2, 2);
        const QByteArray bKey = s.b.key.fingerprint();
        const QByteArray cKey = s.c.key.fingerprint();
        QCOMPARE(s.core.model->sliceOwnership()->ownedBy(bKey), QList<int>{1});
        const QJsonObject refused =
            s.core.invoke(s.appA, "addSlice", {utf8("initialPanId", QString())});
        QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("takeSlice"));
        QCOMPARE(ask.value(QStringLiteral("choices")).toArray().first().toObject()
                     .value(QStringLiteral("sliceId")).toInt(),
                 1);
        SliceOwnership* ownership = s.core.model->sliceOwnership();
        const quint64 incarnation = ownership->incarnation(1);
        const quint64 revision = ownership->controlRevision(1);

        ownership->setOwner(1, cKey);
        ownership->setOwner(1, bKey);
        QCOMPARE(ownership->mark(1).subject(), bKey);
        QCOMPARE(ownership->incarnation(1), incarnation);
        QVERIFY(ownership->controlRevision(1) != revision);
        QVERIFY(ownership->listenersOf(1).contains(cKey));

        const QJsonObject done = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger(), 0);
        QCOMPARE(done.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(s.core.model->slices().size(), 2);
        QVERIFY(s.core.model->sliceById(1) != nullptr);
        QCOMPARE(ownership->ownedBy(bKey), QList<int>{1});
        QVERIFY(ownership->listenersOf(1).contains(cKey));
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), 2);
    }

    // Slice control plan Task 1: the receiver choice showed B's slice 1
    // closing; B closes it and makes a new slice on the same receiver,
    // which takes id 1 in B's own name. Refused as changed; nothing closes.
    void aReceiverTakeWhoseVictimWasClosedAndMadeAgainForTheSameDeviceIsRefused()
    {
        Shared s;
        s.core.model->sliceById(1)->setFrequency(14074000.0);
        const int bStream = streamOf(s.core, 1);
        const QByteArray bKey = s.b.key.fingerprint();
        QCOMPARE(s.core.invoke(s.appA, "addSliceOnPan",
                               {utf8("panId", QStringLiteral("pan-a2"))})
                     .value(QStringLiteral("accepted")).toBool(true), false);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("takeReceiver"));
        int choice = -1;
        for (const QJsonValue& value : ask.value(QStringLiteral("choices")).toArray()) {
            const QJsonObject candidate = value.toObject();
            if (candidate.value(QStringLiteral("streamIndex")).toInt() == bStream) {
                choice = candidate.value(QStringLiteral("choice")).toInt();
            }
        }
        QVERIFY(choice >= 0);
        const quint64 asked = s.core.model->sliceOwnership()->incarnation(1);

        RadioModel* model = s.core.model.get();
        const QString pan = model->sliceById(1)->panKey();
        model->removeSlice(1);
        int reused = -1;
        {
            const SliceOwnership::CreatorScope creator(model->sliceOwnership(), bKey);
            reused = model->addSlice(pan);
        }
        QCOMPARE(reused, 1);
        model->sliceById(1)->setFrequency(14074000.0);
        QCOMPARE(streamOf(s.core, 1), bStream);
        const quint64 now = model->sliceOwnership()->incarnation(1);
        QVERIFY(now != asked);

        const QJsonObject done =
            s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger(), choice);
        QCOMPARE(done.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(done.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("What this change reaches has changed. Make the change again."));
        QCOMPARE(streamOf(s.core, 1), bStream);
        QCOMPARE(model->sliceOwnership()->ownedBy(bKey), QList<int>{1});
        QCOMPARE(model->sliceOwnership()->incarnation(1), now);
        QTest::qWait(2 * StationServer::kDefaultDeltaFlushMs);
        QCOMPARE(countOf(s.appB, QStringLiteral("notice")), 0);
    }

    void theSliceCapFullOffersTheSliceChooser()
    {
        Shared s(2, 2);
        QCOMPARE(s.core.model->slices().size(), 2);
        const QJsonObject refused =
            s.core.invoke(s.appA, "addSlice", {utf8("initialPanId", QString())});
        QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("takeSlice"));
        const QJsonArray choices = ask.value(QStringLiteral("choices")).toArray();
        QCOMPARE(choices.size(), 1);
        QCOMPARE(choices.first().toObject().value(QStringLiteral("sliceId")).toInt(), 1);
        QCOMPARE(choices.first().toObject().value(QStringLiteral("deviceName")).toString(),
                 QStringLiteral("iPad"));
        const QJsonObject done = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger(), 0);
        QCOMPARE(done.value(QStringLiteral("accepted")).toBool(false), true);
        QCOMPARE(s.core.model->sliceOwnership()->ownedBy(s.b.key.fingerprint()).size(), 0);
        QCOMPARE(s.core.model->sliceOwnership()->ownedBy(s.a.key.fingerprint()).size(), 2);
        const QJsonObject told = waitForLast(s.appB, QStringLiteral("notice"), 0);
        QCOMPARE(told.value(QStringLiteral("kind")).toString(), QStringLiteral("sliceTaken"));
        QCOMPARE(told.value(QStringLiteral("takeBack")).toBool(false), true);
    }

    // A stored single-slice choice must be reconsidered if another device
    // claims the free receiver before it is confirmed.
    void aSliceTakeThatCannotPlaceTheNewPanPreservesTheVictim()
    {
        Core core;
        Device a(QStringLiteral("Mac A"), QStringLiteral("computer"));
        Device c(QStringLiteral("Mac C"), QStringLiteral("computer"));
        Device b(QStringLiteral("iPhone B"), QStringLiteral("phone"));
        core.model->configureStreamPool(2, 5, 192000);
        core.model->sliceById(0)->setFrequency(7074000.0);
        core.pair(a);
        core.pair(c);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a);
        QVERIFY(admitted(appA));
        QCOMPARE(core.model->sliceOwnership()->ownedBy(a.key.fingerprint()), QList<int>{0});
        const int aStream = streamOf(core, 0);
        QVERIFY(aStream >= 0);

        for (int expected = 2; expected <= 4; ++expected) {
            const QJsonObject added =
                core.invoke(appA, "addSliceOnPan", {utf8("panId", QStringLiteral("pan-0"))});
            QVERIFY2(added.value(QStringLiteral("accepted")).toBool(false),
                     QJsonDocument(added).toJson().constData());
            QCOMPARE(core.model->sliceOwnership()->ownedBy(a.key.fingerprint()).size(), expected);
        }
        const QList<int> aIds = core.model->sliceOwnership()->ownedBy(a.key.fingerprint());
        for (int id : aIds) {
            QCOMPARE(streamOf(core, id), aStream);
        }

        LoopbackTransport* appC = core.signIn(c);
        QVERIFY(admitted(appC));
        const QList<int> cIds = core.model->sliceOwnership()->ownedBy(c.key.fingerprint());
        QCOMPARE(cIds.size(), 1);
        const int cId = cIds.first();
        const int cStream = streamOf(core, cId);
        QCOMPARE(cStream, aStream);
        QCOMPARE(core.model->slices().size(), 5);

        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appB));
        QVERIFY(core.model->sliceOwnership()->ownedBy(b.key.fingerprint()).isEmpty());
        const int noticesBefore = countOf(appA, QStringLiteral("notice"));
        const int destroysBefore = countOf(appA, QStringLiteral("object.destroy"));
        const int questionsBefore = countOf(appB, QStringLiteral("confirm.request"));
        const QJsonObject refused =
            core.invoke(appB, "addSliceOnPan", {utf8("panId", QStringLiteral("b-new-pan"))});
        QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
        const QJsonObject asked = waitForLast(appB, QStringLiteral("confirm.request"), questionsBefore);
        QCOMPARE(asked.value(QStringLiteral("kind")).toString(), QStringLiteral("takeSlice"));
        int choice = -1;
        for (const QJsonValue& value : asked.value(QStringLiteral("choices")).toArray()) {
            const QJsonObject candidate = value.toObject();
            if (candidate.value(QStringLiteral("sliceId")).toInt(-1) == aIds.first()) {
                choice = candidate.value(QStringLiteral("choice")).toInt(-1);
            }
        }
        QVERIFY(choice >= 0);
        core.model->sliceById(cId)->setFrequency(14074000.0);
        QVERIFY(streamOf(core, cId) != aStream);
        const QJsonObject done = core.invoke(
            appB, "confirm.proceed",
            {int64("id", asked.value(QStringLiteral("id")).toInteger()), int64("choice", choice)});

        const QList<int> afterA = core.model->sliceOwnership()->ownedBy(a.key.fingerprint());
        const QList<int> afterB = core.model->sliceOwnership()->ownedBy(b.key.fingerprint());
        const QList<int> afterC = core.model->sliceOwnership()->ownedBy(c.key.fingerprint());
        qInfo().noquote() << "take atomicity: before A=" << aIds << "C=" << cIds
                          << "DDCs=" << aStream << cStream
                          << "; after A=" << afterA << "B=" << afterB << "C=" << afterC
                          << "DDCs=" << streamOf(core, aIds.first()) << streamOf(core, cId)
                          << "; result=" << QJsonDocument(done).toJson(QJsonDocument::Compact)
                          << "; notices=" << countOf(appA, QStringLiteral("notice")) - noticesBefore
                          << "; destroys=" << countOf(appA, QStringLiteral("object.destroy")) - destroysBefore
                          << "; lastNotice=" << QJsonDocument(lastOf(appA, QStringLiteral("notice")))
                                                   .toJson(QJsonDocument::Compact)
                          << "; lastDestroy=" << QJsonDocument(lastOf(appA, QStringLiteral("object.destroy")))
                                                    .toJson(QJsonDocument::Compact);
        QCOMPARE(done.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(done.value(QStringLiteral("reason")).toString(), kWaiting);
        QCOMPARE(valueOf(done, QStringLiteral("phase")).toString(), QStringLiteral("needsConfirmation"));
        const QJsonObject replacement = waitForLast(appB, QStringLiteral("confirm.request"), questionsBefore + 1);
        QCOMPARE(replacement.value(QStringLiteral("kind")).toString(), QStringLiteral("takeReceiver"));
        bool namesC = false;
        for (const QJsonValue& value : replacement.value(QStringLiteral("choices")).toArray()) {
            const QJsonObject receiver = value.toObject();
            if (receiver.value(QStringLiteral("streamIndex")).toInt(-1) == streamOf(core, cId)) {
                const QJsonArray slices = receiver.value(QStringLiteral("slices")).toArray();
                namesC = slices.size() == 1
                    && slices.first().toObject().value(QStringLiteral("sliceId")).toInt() == cId
                    && slices.first().toObject().value(QStringLiteral("deviceName")).toString()
                           == QStringLiteral("Mac C");
            }
        }
        QVERIFY(namesC);
        QCOMPARE(afterA, aIds);
        QCOMPARE(afterB.size(), 0);
        QCOMPARE(afterC, cIds);
        QCOMPARE(countOf(appA, QStringLiteral("notice")), noticesBefore);
        QCOMPARE(countOf(appA, QStringLiteral("object.destroy")), destroysBefore);
    }

    // Slice control plan Task 9: with every receiver held, a sharing
    // device's Add is refused naming the receivers and lists every live
    // slice it could listen to instead; listening to one closes nothing.
    // A device without the feature is sent no list.
    void aRefusedAddListsTheSlicesToListenTo()
    {
        Listened s;
        QVERIFY(s.aStream() >= 0);
        QVERIFY(streamOf(s.core, s.core.model->sliceOwnership()
                                    ->ownedBy(s.c.key.fingerprint()).first())
                != s.aStream());
        const int slicesBefore = static_cast<int>(s.core.model->slices().size());
        QVERIFY(slicesBefore < 5);
        const QJsonObject refused = s.core.invoke(
            s.appB, "addSliceOnPan", {utf8("panId", QStringLiteral("b-new-pan"))});
        QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
        QVERIFY2(refused.value(QStringLiteral("reason")).toString().contains(
                     QStringLiteral("receivers are in use by")),
                 qPrintable(refused.value(QStringLiteral("reason")).toString()));
        const QJsonValue listed = valueOf(refused, QStringLiteral("usableSlices"));
        QVERIFY2(listed.isString(), QJsonDocument(refused).toJson().constData());
        const QJsonArray usable = QJsonDocument::fromJson(listed.toString().toUtf8()).array();
        QCOMPARE(usable.size(), slicesBefore);
        const SliceOwnership* ownership = s.core.model->sliceOwnership();
        QSet<int> ids;
        for (const QJsonValue& v : usable) {
            const QJsonObject entry = v.toObject();
            const int id = entry.value(QStringLiteral("sliceId")).toInt(-1);
            ids.insert(id);
            QVERIFY(ownership->isLive(id));
            QCOMPARE(static_cast<quint64>(entry.value(QStringLiteral("incarnation")).toInteger()),
                     ownership->incarnation(id));
            QCOMPARE(entry.value(QStringLiteral("letter")).toString(),
                     ReceiverPlanner::letterOf(id));
        }
        QCOMPARE(ids.size(), slicesBefore);
        QJsonObject zero;
        for (const QJsonValue& v : usable) {
            if (v.toObject().value(QStringLiteral("sliceId")).toInt(-1) == 0) {
                zero = v.toObject();
            }
        }
        QCOMPARE(zero.value(QStringLiteral("controllerDeviceId")).toString(), wireOf(s.a));

        const QJsonObject listened = s.core.invoke(
            s.appB, "slice.listen",
            {int64("sliceId", 0),
             int64("incarnation", zero.value(QStringLiteral("incarnation")).toInteger())});
        QVERIFY2(listened.value(QStringLiteral("accepted")).toBool(false),
                 QJsonDocument(listened).toJson().constData());
        QCOMPARE(static_cast<int>(s.core.model->slices().size()), slicesBefore);
        QVERIFY(ownership->isListening(s.b.key.fingerprint(), 0));
        // An older device is refused with no list:
        // aTakeQuestionToAnOlderDeviceNamesNoListeners.
    }

    // Slice control plan Task 9: a take question to a sharing device names
    // each slice's listeners, the controller first. A listener who leaves
    // before the answer changes nothing the operator must see: the take
    // proceeds.
    void aTakeQuestionNamesListenersAndALeaverDoesNotAskAgain()
    {
        Listened s;
        QVERIFY(s.core.invoke(s.appC, "slice.listen", s.refOf(0))
                    .value(QStringLiteral("accepted")).toBool(false));
        const quint64 incarnation = s.core.model->sliceOwnership()->incarnation(0);
        const QJsonObject ask = s.bAsks();
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("takeReceiver"));
        const QJsonObject choice = choiceForStream(ask, s.aStream());
        QVERIFY2(choice.value(QStringLiteral("takeable")).toBool(false),
                 QJsonDocument(choice).toJson().constData());
        QCOMPARE(listenersIn(sliceIn(choice, 0)), (QStringList{wireOf(s.a), wireOf(s.c)}));

        QVERIFY(s.core.invoke(s.appC, "slice.stopListening", s.refOf(0))
                    .value(QStringLiteral("accepted")).toBool(false));
        const QJsonObject done = s.proceed(ask);
        QVERIFY2(done.value(QStringLiteral("accepted")).toBool(false),
                 QJsonDocument(done).toJson().constData());
        QVERIFY(!s.core.model->sliceOwnership()->matches(SliceOwnership::SliceRef{0, incarnation}));
    }

    // Slice control plan Task 9: a listener who joins a slice the question
    // closes before the answer: asked again, nothing closed. On the new
    // question's answer the slice closes and each listener is told, naming
    // it, besides its controller's own notice.
    void aListenerWhoJoinsAsksAgainAndEveryListenerIsTold()
    {
        Listened s;
        QVERIFY(s.core.invoke(s.appC, "slice.listen", s.refOf(0))
                    .value(QStringLiteral("accepted")).toBool(false));
        const quint64 incarnation = s.core.model->sliceOwnership()->incarnation(0);
        const QJsonObject ask = s.bAsks();
        QCOMPARE(listenersIn(sliceIn(choiceForStream(ask, s.aStream()), 0)),
                 (QStringList{wireOf(s.a), wireOf(s.c)}));
        QVERIFY(s.core.invoke(s.appD, "slice.listen", s.refOf(0))
                    .value(QStringLiteral("accepted")).toBool(false));
        const int questions = countOf(s.appB, QStringLiteral("confirm.request"));
        const QJsonObject again = s.proceed(ask);
        QCOMPARE(again.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(again.value(QStringLiteral("reason")).toString(), kWaiting);
        QVERIFY(s.core.model->sliceOwnership()->matches(SliceOwnership::SliceRef{0, incarnation}));
        const QJsonObject asked = waitForLast(s.appB, QStringLiteral("confirm.request"), questions);
        QCOMPARE(listenersIn(sliceIn(choiceForStream(asked, s.aStream()), 0)),
                 (QStringList{wireOf(s.a), wireOf(s.c), wireOf(s.d)}));

        const int cNotices = countOf(s.appC, QStringLiteral("notice"));
        const int dNotices = countOf(s.appD, QStringLiteral("notice"));
        const QJsonObject done = s.proceed(asked);
        QVERIFY2(done.value(QStringLiteral("accepted")).toBool(false),
                 QJsonDocument(done).toJson().constData());
        QVERIFY(!s.core.model->sliceOwnership()->matches(SliceOwnership::SliceRef{0, incarnation}));
        const QJsonObject owner = waitForLast(s.appA, QStringLiteral("notice"), 0);
        QCOMPARE(owner.value(QStringLiteral("kind")).toString(), QStringLiteral("receiverTaken"));
        for (const auto& [app, before] :
             {qMakePair(s.appC, cNotices), qMakePair(s.appD, dNotices)}) {
            const QJsonObject told = waitForLast(app, QStringLiteral("notice"), before);
            QCOMPARE(told.value(QStringLiteral("kind")).toString(), QStringLiteral("sliceClosed"));
            QCOMPARE(told.value(QStringLiteral("takeBack")).toBool(true), false);
            QCOMPARE(told.value(QStringLiteral("reason")).toString(),
                     QStringLiteral("iPad took the receiver slice A was on. You were listening "
                                    "to it."));
            const QJsonArray slices = told.value(QStringLiteral("slices")).toArray();
            QCOMPARE(slices.size(), 1);
            QCOMPARE(slices.first().toObject().value(QStringLiteral("letter")).toString(),
                     QStringLiteral("A"));
        }
        // B, who took it, is told nothing; A's own notice is not doubled.
        QCOMPARE(countOf(s.appA, QStringLiteral("notice")), 1);
    }

    // Slice control plan Task 9: a device without the feature is asked as
    // before, with no listener lists.
    void aTakeQuestionToAnOlderDeviceNamesNoListeners()
    {
        Shared s(2, 5);
        s.cOnTheOtherReceiver();
        const QJsonObject refused =
            s.core.invoke(s.appB, "addSliceOnPan", {utf8("panId", QStringLiteral("b-new-pan"))});
        QVERIFY2(!refused.value(QStringLiteral("accepted")).toBool(true),
                 QJsonDocument(refused).toJson().constData());
        QVERIFY2(!valueOf(refused, QStringLiteral("usableSlices")).isString(),
                 QJsonDocument(refused).toJson().constData());
        const QJsonObject ask = waitForLast(s.appB, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("takeReceiver"));
        for (const QJsonValue& c : ask.value(QStringLiteral("choices")).toArray()) {
            for (const QJsonValue& slice : c.toObject().value(QStringLiteral("slices")).toArray()) {
                QVERIFY(!slice.toObject().contains(QStringLiteral("listenerDeviceIds")));
            }
        }
    }

    void fullSliceAndReceiverPoolOffersTheNamedReceiverGroup()
    {
        Shared s(2, 3);
        QCOMPARE(s.core.invoke(s.appA, "addSliceOnPan", {utf8("panId", QStringLiteral("pan-0"))})
                     .value(QStringLiteral("accepted")).toBool(false), true);
        s.core.model->sliceById(1)->setFrequency(14074000.0);
        QCOMPARE(s.core.model->slices().size(), 3);
        const int notices = countOf(s.appB, QStringLiteral("notice"));
        const int destroys = countOf(s.appB, QStringLiteral("object.destroy"));
        const QJsonObject refused = s.core.invoke(
            s.appA, "addSliceOnPan", {utf8("panId", QStringLiteral("new-own-pan"))});
        QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("takeReceiver"));
        bool found = false;
        for (const QJsonValue& item : ask.value(QStringLiteral("choices")).toArray()) {
            const QJsonObject receiver = item.toObject();
            if (receiver.value(QStringLiteral("streamIndex")).toInt(-1) == streamOf(s.core, 1)) {
                found = true;
                QCOMPARE(receiver.value(QStringLiteral("takeable")).toBool(false), true);
                const QJsonArray named = receiver.value(QStringLiteral("slices")).toArray();
                QCOMPARE(named.size(), 1);
                QCOMPARE(named.first().toObject().value(QStringLiteral("sliceId")).toInt(), 1);
                QCOMPARE(named.first().toObject().value(QStringLiteral("deviceName")).toString(),
                         QStringLiteral("iPad"));
            }
        }
        QVERIFY(found);
        QCOMPARE(s.core.model->slices().size(), 3);
        QCOMPARE(countOf(s.appB, QStringLiteral("notice")), notices);
        QCOMPARE(countOf(s.appB, QStringLiteral("object.destroy")), destroys);
    }

    void aReceiverTakeAtTheSliceCapFreesBothResources()
    {
        Shared s(2, 3);
        QCOMPARE(s.core.invoke(s.appA, "addSliceOnPan", {utf8("panId", QStringLiteral("pan-0"))})
                     .value(QStringLiteral("accepted")).toBool(false), true);
        s.core.model->sliceById(1)->setFrequency(14074000.0);
        const int bStream = streamOf(s.core, 1);
        s.core.invoke(s.appA, "addSliceOnPan", {utf8("panId", QStringLiteral("new-own-pan"))});
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("takeReceiver"));
        int choice = -1;
        for (const QJsonValue& item : ask.value(QStringLiteral("choices")).toArray()) {
            const QJsonObject receiver = item.toObject();
            if (receiver.value(QStringLiteral("streamIndex")).toInt(-1) == bStream) {
                choice = receiver.value(QStringLiteral("choice")).toInt(-1);
            }
        }
        QVERIFY(choice >= 0);
        const QJsonObject done = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger(), choice);
        QCOMPARE(done.value(QStringLiteral("accepted")).toBool(false), true);
        QVERIFY(s.core.model->sliceOwnership()->ownedBy(s.b.key.fingerprint()).isEmpty());
        QCOMPARE(s.core.model->slices().size(), 3);
        QCOMPARE(lastOf(s.appB, QStringLiteral("notice")).value(QStringLiteral("kind")).toString(),
                 QStringLiteral("receiverTaken"));
    }

    void malformedAddAtCapacityNeverAsksToTakeOrRemoves()
    {
        Shared s(2, 2);
        QCOMPARE(s.core.model->slices().size(), 2);
        for (const QByteArray& verb : {QByteArrayLiteral("addSlice"), QByteArrayLiteral("addSliceOnPan")}) {
            const int before = countOf(s.appA, QStringLiteral("confirm.request"));
            const int destroys = countOf(s.appB, QStringLiteral("object.destroy"));
            const QJsonObject refused = s.core.invoke(s.appA, verb, {});
            QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
            QCOMPARE(refused.value(QStringLiteral("reason")).toString(),
                     QStringLiteral("The Core could not read this request."));
            QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), before);
            QCOMPARE(countOf(s.appB, QStringLiteral("object.destroy")), destroys);
            QCOMPARE(s.core.model->slices().size(), 2);
        }
    }

    void sharedOwnPanAtSliceCapStillSucceedsAfterSliceTake()
    {
        Shared s(2, 2);
        QCOMPARE(streamOf(s.core, 0), streamOf(s.core, 1));
        s.core.invoke(s.appA, "addSliceOnPan", {utf8("panId", QStringLiteral("pan-0"))});
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("takeSlice"));
        const QJsonObject done = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger(), 0);
        QCOMPARE(done.value(QStringLiteral("accepted")).toBool(false), true);
        QCOMPARE(s.core.model->sliceOwnership()->ownedBy(s.a.key.fingerprint()).size(), 2);
        QCOMPARE(streamOf(s.core, 0), streamOf(s.core, 1));
    }

    void lastPhysicalCoreSliceCannotBeOfferedAsAnAddVictim()
    {
        Core core;
        Device a(QStringLiteral("Mac A"), QStringLiteral("computer"));
        Device b(QStringLiteral("iPhone B"), QStringLiteral("phone"));
        core.model->configureStreamPool(1, 1, 192000);
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appA));
        QVERIFY(admitted(appB));
        const int before = countOf(appB, QStringLiteral("confirm.request"));
        const int destroys = countOf(appA, QStringLiteral("object.destroy"));
        const QJsonObject refused = core.invoke(
            appB, "addSliceOnPan", {utf8("panId", QStringLiteral("new-pan"))});
        QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(countOf(appB, QStringLiteral("confirm.request")), before);
        QCOMPARE(countOf(appA, QStringLiteral("object.destroy")), destroys);
        QCOMPARE(core.model->sliceOwnership()->ownedBy(a.key.fingerprint()), QList<int>{0});
        QCOMPARE(core.model->slices().size(), 1);
    }

    // ── Notices for an away device (7.4) ─────────────────────────────────

    void aNoticeWaitsForAnAwayDeviceAndArrivesAfterItsSnapshotComplete()
    {
        Shared s;
        s.core.model->sliceById(1)->setFrequency(14074000.0);
        const int bReceiver = streamOf(s.core, 1);
        s.core.now = 1000;
        s.appB->closeLink(QStringLiteral("lost"));
        QTRY_COMPARE(s.core.sessions().entry(s.b.key.fingerprint())->state,
                     DeviceSessionRegistry::State::Away);
        s.core.now = 5000;
        s.core.invoke(s.appA, "addSliceOnPan", {utf8("panId", QStringLiteral("pan-a2"))});
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        int choice = -1;
        for (const QJsonValue& v : ask.value(QStringLiteral("choices")).toArray()) {
            if (v.toObject().value(QStringLiteral("streamIndex")).toInt() == bReceiver) {
                choice = v.toObject().value(QStringLiteral("choice")).toInt();
                QCOMPARE(v.toObject().value(QStringLiteral("devices")).toArray().first().toObject()
                             .value(QStringLiteral("state")).toString(),
                         QStringLiteral("away"));
            }
        }
        QCOMPARE(s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger(), choice)
                     .value(QStringLiteral("accepted")).toBool(false),
                 true);
        QVERIFY(s.core.model->sliceOwnership()->ownedBy(s.b.key.fingerprint()).isEmpty());

        s.core.now = 35000;
        LoopbackTransport* back = s.core.signIn(s.b);
        QVERIFY(admitted(back));
        const QJsonObject told = waitForLast(back, QStringLiteral("notice"), 0);
        QCOMPARE(told.value(QStringLiteral("kind")).toString(), QStringLiteral("receiverTaken"));
        QCOMPARE(told.value(QStringLiteral("secondsAgo")).toInteger(), 30);
        QCOMPARE(told.value(QStringLiteral("takeBack")).toBool(false), true);
        QVERIFY(indexOfType(back, QStringLiteral("notice"))
                > indexOfType(back, QStringLiteral("snapshot.complete")));
    }

    void aDeviceBackAfterIts180SecondsGetsGraceEnded()
    {
        Shared s;
        s.core.now = 1000;
        s.appB->closeLink(QStringLiteral("lost"));
        QTRY_COMPARE(s.core.sessions().entry(s.b.key.fingerprint())->state,
                     DeviceSessionRegistry::State::Away);
        s.core.now = 1000 + DeviceSessionRegistry::kGraceMs;
        QCOMPARE(s.core.sessions().expireAway().size(), 1);
        s.core.now = 1000 + DeviceSessionRegistry::kGraceMs + 10000;
        LoopbackTransport* back = s.core.signIn(s.b);
        QVERIFY(admitted(back));
        const QJsonObject told = waitForLast(back, QStringLiteral("notice"), 0);
        QCOMPARE(told.value(QStringLiteral("kind")).toString(), QStringLiteral("graceEnded"));
        QCOMPARE(told.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("You were away for more than 3 minutes. Your slices are back."));
        QCOMPARE(told.value(QStringLiteral("secondsAgo")).toInteger(), 10);
        QCOMPARE(told.value(QStringLiteral("takeBack")).toBool(true), false);
        QVERIFY(!told.contains(QStringLiteral("byName")));
        QVERIFY(indexOfType(back, QStringLiteral("notice"))
                > indexOfType(back, QStringLiteral("snapshot.complete")));
    }

    // Fix wave (Minor): back after its 180 s with a saved slice that no
    // longer fits, graceEnded names what was not restored, as the
    // slicesNotRestored notice does, and never says the slices are back.
    void aGraceEndedWithASliceNotRestoredSaysSo()
    {
        Shared s(2, 2);
        s.core.now = 1000;
        s.appB->closeLink(QStringLiteral("lost"));
        QTRY_COMPARE(s.core.sessions().entry(s.b.key.fingerprint())->state,
                     DeviceSessionRegistry::State::Away);
        s.core.now = 1000 + DeviceSessionRegistry::kGraceMs;
        QCOMPARE(s.core.sessions().expireAway().size(), 1);
        QVERIFY(s.core.model->sliceById(1) == nullptr);
        // A takes the freed slice place: the cap is full again.
        QVERIFY(s.core.invoke(s.appA, "addSlice", {utf8("initialPanId", QString())})
                    .value(QStringLiteral("accepted")).toBool());
        s.core.now = 1000 + DeviceSessionRegistry::kGraceMs + 10000;
        LoopbackTransport* back = s.core.signIn(s.b);
        QVERIFY(admitted(back));
        const QJsonObject told = waitForLast(back, QStringLiteral("notice"), 0);
        QCOMPARE(told.value(QStringLiteral("kind")).toString(), QStringLiteral("graceEnded"));
        QCOMPARE(told.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("You were away for more than 3 minutes. 1 of your slices could "
                                "not be restored: all the radio's receivers are in use."));
        QVERIFY(OperatorWording::isPlain(told.value(QStringLiteral("reason")).toString()));
        QCOMPARE(told.value(QStringLiteral("slices")).toArray().size(), 1);
    }

    // ── An older window (10.7, rulings 6.10 and 10.2) ────────────────────

    void anOlderWindowIsRefusedNamingWhoItWouldAffectNeverAsked()
    {
        Core core;
        core.model->configureStreamPool(2, 5, 192000);
        core.model->sliceById(0)->setFrequency(7074000.0);
        Device older(QStringLiteral("Shack PC"), QStringLiteral("computer"));
        Device b(QStringLiteral("Grant's iPhone"), QStringLiteral("phone"));
        core.pair(older);
        core.pair(b);
        LoopbackTransport* appOld = core.signIn(older, {{"deviceAuth", 1}});
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appOld));
        QVERIFY(admitted(appB));
        core.model->sliceById(1)->setFrequency(7150000.0);
        const QJsonObject refused = core.invoke(
            appOld, "requestStreamCentre", {int64("sliceId", 0), f64("centreHz", 7000000.0)});
        QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
        const QString reason = refused.value(QStringLiteral("reason")).toString();
        QCOMPARE(reason, QStringLiteral("This change would affect Grant's iPhone. Update NereusSDR "
                                        "to confirm changes that affect other devices."));
        // A name is the operator's own words: checked with it set aside.
        QVERIFY(OperatorWording::isPlain(QString(reason).remove(QStringLiteral("Grant's iPhone"))));
        QCOMPARE(countOf(appOld, QStringLiteral("confirm.request")), 0);
        QCOMPARE(streamOf(core, 1), streamOf(core, 0));
    }

    void takingAnOlderWindowsLastSliceEndsItTakenOver()
    {
        Core core;
        core.model->configureStreamPool(2, 5, 192000);
        core.model->sliceById(0)->setFrequency(7074000.0);
        Device a;
        Device older(QStringLiteral("Shack PC"), QStringLiteral("computer"));
        core.pair(a);
        core.pair(older);
        LoopbackTransport* appA = core.signIn(a);
        LoopbackTransport* appOld = core.signIn(older, {{"deviceAuth", 1}});
        QVERIFY(admitted(appA));
        QVERIFY(admitted(appOld));
        const int oldSlice = core.model->sliceOwnership()->ownedBy(older.key.fingerprint()).first();
        core.model->sliceById(oldSlice)->setFrequency(14074000.0);
        const int oldReceiver = streamOf(core, oldSlice);
        core.invoke(appA, "addSliceOnPan", {utf8("panId", QStringLiteral("pan-a2"))});
        const QJsonObject ask = waitForLast(appA, QStringLiteral("confirm.request"), 0);
        int choice = -1;
        for (const QJsonValue& v : ask.value(QStringLiteral("choices")).toArray()) {
            if (v.toObject().value(QStringLiteral("streamIndex")).toInt() == oldReceiver) {
                choice = v.toObject().value(QStringLiteral("choice")).toInt();
            }
        }
        const QJsonObject done = core.invoke(
            appA, "confirm.proceed",
            {int64("id", ask.value(QStringLiteral("id")).toInteger()), int64("choice", choice)});
        QCOMPARE(done.value(QStringLiteral("accepted")).toBool(false), true);
        const QJsonObject end = endOf(appOld);
        QCOMPARE(end.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("iPhone took the receiver this app was using. Update NereusSDR to "
                                "share the Core."));
        QCOMPARE(end.value(QStringLiteral("retryable")).toBool(true), false);
        QCOMPARE(end.value(QStringLiteral("code")).toString(), QStringLiteral("takenOver"));
        QVERIFY(!core.sessions().entry(older.key.fingerprint()));
        // The older window was never told by a notice.
        QCOMPARE(countOf(appOld, QStringLiteral("notice")), 0);
    }

    void anOlderWindowWithNoSliceAtAdmissionIsRefusedRetryable()
    {
        Shared s(2, 2);
        Device older(QStringLiteral("Shack PC"), QStringLiteral("computer"));
        s.core.pair(older);
        LoopbackTransport* appOld = s.core.signIn(older, {{"deviceAuth", 1}});
        const QJsonObject end = endOf(appOld);
        QCOMPARE(end.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("All the radio's slices are in use. Try again when another device "
                                "closes one."));
        QCOMPARE(end.value(QStringLiteral("retryable")).toBool(false), true);
        QVERIFY(!end.contains(QStringLiteral("code")));
        QVERIFY(!s.core.sessions().entry(older.key.fingerprint()));
        QCOMPARE(s.core.model->slices().size(), 2);
    }

    // ── iPhone app Task 75: settings that affect every device ────────────

    void aloneAnAttenuatorChangeAppliesAtOnce()
    {
        SharedAdc s(false);
        QVERIFY(admitted(s.appA));
        const QJsonObject result = s.writeStepAtt(s.appA, "attenuationDb", 20);
        QCOMPARE(firstResultEntry(result).value(QStringLiteral("accepted")).toBool(false), true);
        QCOMPARE(s.attenuation(), 20);
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), 0);
    }

    // Ruling 7.1a (JJ, 2026-09-28): the attenuator is a small adjustment:
    // it applies at once, nobody is asked, and the device it reaches is
    // told.
    void anAttenuatorChangeWithAnotherDeviceOnTheAdcAppliesAtOnceAndTellsIt()
    {
        SharedAdc s;
        QVERIFY(admitted(s.appB));
        const int bSlice = s.bSlice();
        const QJsonObject result = s.writeStepAtt(s.appA, "attenuationDb", 20);
        QCOMPARE(firstResultEntry(result).value(QStringLiteral("accepted")).toBool(false), true);
        QCOMPARE(s.attenuation(), 20);
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), 0);

        const QJsonObject told = waitForLast(s.appB, QStringLiteral("notice"), 0);
        QCOMPARE(told.value(QStringLiteral("kind")).toString(), QStringLiteral("settingChanged"));
        QCOMPARE(told.value(QStringLiteral("byName")).toString(), QStringLiteral("iPhone"));
        QCOMPARE(told.value(QStringLiteral("takeBack")).toBool(true), false);
        QCOMPARE(told.value(QStringLiteral("change")).toObject(),
                 (QJsonObject{{QStringLiteral("label"), QStringLiteral("Attenuator, ADC 1")},
                              {QStringLiteral("from"), QStringLiteral("0 dB")},
                              {QStringLiteral("to"), QStringLiteral("20 dB")}}));
        QCOMPARE(told.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("iPhone changed Attenuator, ADC 1 from 0 dB to 20 dB."));
        QCOMPARE(told.value(QStringLiteral("slices")).toArray().first().toObject()
                     .value(QStringLiteral("sliceId")).toInt(),
                 bSlice);
        QCOMPARE(countOf(s.appA, QStringLiteral("notice")), 0);

        // A write that changes nothing tells nobody.
        s.writeStepAtt(s.appA, "attenuationDb", 20);
        QTest::qWait(50);
        QCOMPARE(countOf(s.appB, QStringLiteral("notice")), 1);
    }

    // Ruling 7.1a: an away device is never asked about. A change on an Ask
    // row that disturbs only an away device applies at once, and the away
    // device is told when it comes back.
    void aChangeDisturbingOnlyAnAwayDeviceAppliesAndItIsToldOnItsReturn()
    {
        SharedAdc s;
        QVERIFY(admitted(s.appB));
        const int bSlice = s.bSlice();
        s.appB->closeLink(QStringLiteral("lost"));
        QTRY_COMPARE(s.core.sessions().entry(s.b.key.fingerprint())->state,
                     DeviceSessionRegistry::State::Away);
        QVERIFY(s.core.model->sliceById(bSlice) != nullptr);

        const QJsonObject result = s.writeDiversityGain(s.appA, 6.0);
        QCOMPARE(firstResultEntry(result).value(QStringLiteral("accepted")).toBool(false), true);
        QCOMPARE(s.diversityGain(), 6.0);
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), 0);

        LoopbackTransport* back = s.core.signIn(s.b);
        QVERIFY(admitted(back));
        const QJsonObject told = waitForLast(back, QStringLiteral("notice"), 0);
        QCOMPARE(told.value(QStringLiteral("kind")).toString(), QStringLiteral("settingChanged"));
        QCOMPARE(told.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("iPhone changed Diversity gain from 0 dB to 6 dB."));
    }

    // Ruling 7.1a: the receive DSP options apply at once and tell too.
    void aReceiveOptionsWriteAppliesAtOnceAndTells()
    {
        SharedAdc s;
        const QString key = QStringLiteral("DspOptionsBufferSizePhoneRx");
        s.appA->sendText(SessionMessages::encode(
            SessionMessages::settingsWrite(key, QStringLiteral("1024"), QStringLiteral("a-1"))));
        QTRY_COMPARE(s.core.settings->value(key).toString(), QStringLiteral("1024"));
        QCOMPARE(countOf(s.appA, QStringLiteral("settings.reject")), 0);
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), 0);
        const QJsonObject told = waitForLast(s.appB, QStringLiteral("notice"), 0);
        QCOMPARE(told.value(QStringLiteral("change")).toObject()
                     .value(QStringLiteral("label")).toString(),
                 QStringLiteral("Receive buffer size, voice modes"));
    }

    void aDiversityChangeWithAnotherDeviceListeningAsksThenTellsIt()
    {
        SharedAdc s;
        QVERIFY(admitted(s.appB));
        const int bSlice = s.bSlice();
        const int bReceiver = streamOf(s.core, bSlice);
        QVERIFY(bReceiver >= 0);
        QVERIFY(bReceiver != streamOf(s.core, 0));
        const QJsonObject held = s.writeDiversityGain(s.appA, 6.0);
        QCOMPARE(firstResultEntry(held).value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(firstResultEntry(held).value(QStringLiteral("reason")).toString(), kWaiting);
        QCOMPARE(s.diversityGain(), 0.0);

        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("sharedSetting"));
        QCOMPARE(ask.value(QStringLiteral("reason")).toString(), kWaiting);
        QCOMPARE(ask.value(QStringLiteral("expiresInMs")).toInteger(), 60000);
        QVERIFY(ask.contains(QStringLiteral("forWriteId")));
        const QJsonObject change = ask.value(QStringLiteral("change")).toObject();
        QCOMPARE(change.value(QStringLiteral("label")).toString(), QStringLiteral("Diversity gain"));
        QCOMPARE(change.value(QStringLiteral("from")).toString(), QStringLiteral("0 dB"));
        QCOMPARE(change.value(QStringLiteral("to")).toString(), QStringLiteral("6 dB"));
        const QJsonArray affected = ask.value(QStringLiteral("affected")).toArray();
        QCOMPARE(affected.size(), 1);
        const QJsonObject who = affected.first().toObject();
        QCOMPARE(who.value(QStringLiteral("deviceName")).toString(), QStringLiteral("iPad"));
        QCOMPARE(who.value(QStringLiteral("deviceShortName")).toString(), QStringLiteral("iPad"));
        QCOMPARE(who.value(QStringLiteral("state")).toString(), QStringLiteral("listening"));
        QCOMPARE(who.value(QStringLiteral("holdsTransmit")).toBool(true), false);
        const QJsonObject slice = who.value(QStringLiteral("slices")).toArray().first().toObject();
        QCOMPARE(slice.value(QStringLiteral("sliceId")).toInt(), bSlice);
        QCOMPARE(slice.value(QStringLiteral("mode")).toInt(),
                 static_cast<int>(s.core.model->sliceById(bSlice)->dspMode()));
        QCOMPARE(slice.value(QStringLiteral("adc")).toInt(), 0);
        QCOMPARE(slice.value(QStringLiteral("streamIndex")).toInt(), bReceiver);
        QCOMPARE(slice.value(QStringLiteral("effect")).toString(), QStringLiteral("changes"));
        QCOMPARE(countOf(s.appB, QStringLiteral("notice")), 0);

        s.core.now = 7000;
        const QJsonObject done = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(done.value(QStringLiteral("accepted")).toBool(false), true);
        QCOMPARE(valueOf(done, QStringLiteral("objectKey")).toString(), QStringLiteral("slice:0"));
        QCOMPARE(valueOf(done, QStringLiteral("diversityGainDb")).toDouble(), 6.0);
        QCOMPARE(s.diversityGain(), 6.0);

        const QJsonObject told = waitForLast(s.appB, QStringLiteral("notice"), 0);
        QCOMPARE(told.value(QStringLiteral("kind")).toString(), QStringLiteral("settingChanged"));
        QCOMPARE(told.value(QStringLiteral("byName")).toString(), QStringLiteral("iPhone"));
        QCOMPARE(told.value(QStringLiteral("bySource")).toString(), QStringLiteral("device"));
        QCOMPARE(told.value(QStringLiteral("secondsAgo")).toInteger(), 0);
        QCOMPARE(told.value(QStringLiteral("takeBack")).toBool(true), false);
        QCOMPARE(told.value(QStringLiteral("change")).toObject(), change);
        QCOMPARE(told.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("iPhone changed Diversity gain from 0 dB to 6 dB."));
        QCOMPARE(told.value(QStringLiteral("slices")).toArray().first().toObject()
                     .value(QStringLiteral("sliceId")).toInt(),
                 bSlice);
        // The asker is not told.
        QCOMPARE(countOf(s.appA, QStringLiteral("notice")), 0);
    }

    void aSharedQuestionExpiresSixtySecondsAfterItWasSent()
    {
        SharedAdc s;
        s.core.now = 1000;
        s.writeDiversityGain(s.appA, 6.0);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        s.core.now = 1000 + ConfirmStep::kExpiryMs;
        const QJsonObject late = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(late.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(late.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("That question has expired. Make the change again."));
        QCOMPARE(s.diversityGain(), 0.0);
        QCOMPARE(countOf(s.appB, QStringLiteral("notice")), 0);

        // One millisecond short of it, the question is still open.
        s.writeDiversityGain(s.appA, 6.0);
        const QJsonObject again = waitForLast(s.appA, QStringLiteral("confirm.request"), 1);
        s.core.now += ConfirmStep::kExpiryMs - 1;
        QCOMPARE(s.proceed(s.appA, again.value(QStringLiteral("id")).toInteger())
                     .value(QStringLiteral("accepted")).toBool(false),
                 true);
        QCOMPARE(s.diversityGain(), 6.0);
    }

    void aPanMoveQuestionExpiresToo()
    {
        Shared s;
        s.core.now = 1000;
        s.core.invoke(s.appA, "requestStreamCentre", {int64("sliceId", 0), f64("centreHz", 7000000.0)});
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("panMove"));
        s.core.now = 1000 + ConfirmStep::kExpiryMs;
        const QJsonObject late = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(late.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("That question has expired. Make the change again."));
        QCOMPARE(streamOf(s.core, 1), s.receiver());
    }

    void aSecondDisturbingChangeReplacesTheFirst()
    {
        SharedAdc s;
        s.writeDiversityGain(s.appA, 6.0);
        const QJsonObject first = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        s.writeOwnSlice(s.appA, MirrorUpdate{0, QByteArrayLiteral("diversityEnabled"),
                                             MirrorWireKind::Bool, QVariant(true)});
        const QJsonObject second = waitForLast(s.appA, QStringLiteral("confirm.request"), 1);
        QVERIFY(second.value(QStringLiteral("id")).toInteger()
                != first.value(QStringLiteral("id")).toInteger());
        const QJsonObject stale = s.proceed(s.appA, first.value(QStringLiteral("id")).toInteger());
        QCOMPARE(stale.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(stale.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("That question is no longer open. Make the change again."));
        QCOMPARE(s.diversityGain(), 0.0);
    }

    void aNewWriteToTheSameTargetCancelsTheQuestion()
    {
        SharedAdc s;
        s.writeDiversityGain(s.appA, 6.0);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        // Back to the value it already has: no change, nobody asked, and
        // the open question goes (ruling 7.6).
        const QJsonObject same = s.writeDiversityGain(s.appA, 0.0);
        QCOMPARE(firstResultEntry(same).value(QStringLiteral("accepted")).toBool(false), true);
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), 1);
        const QJsonObject stale = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(stale.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("That question is no longer open. Make the change again."));
        QCOMPARE(s.diversityGain(), 0.0);
    }

    void aProceedAfterTheTargetChangedIsRefused()
    {
        SharedAdc s;
        s.writeDiversityGain(s.appA, 6.0);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        // The Core's own change moves the target (ruling 7.6: whoever
        // changed it).
        s.core.model->sliceById(0)->setDiversityGainDb(3.0);
        QCOMPARE(s.diversityGain(), 3.0);
        const QJsonObject refused = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(refused.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("That setting changed since you asked. Make the change again."));
        QVERIFY(refused.value(QStringLiteral("values")).toArray().isEmpty());
        QCOMPARE(s.diversityGain(), 3.0);
        QCOMPARE(countOf(s.appB, QStringLiteral("notice")), 0);
    }

    void aSharedProceedWhoseSetGrewAsksAgainAndAppliesNothing()
    {
        SharedAdc s;
        s.writeDiversityGain(s.appA, 6.0);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        // B opens a second slice before A answers.
        QCOMPARE(s.core.invoke(s.appB, "addSliceOnPan", {utf8("panId", QStringLiteral("pan-b2"))})
                     .value(QStringLiteral("accepted")).toBool(false),
                 true);
        const QJsonObject again = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(again.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(again.value(QStringLiteral("reason")).toString(), kWaiting);
        QCOMPARE(valueOf(again, QStringLiteral("phase")).toString(),
                 QStringLiteral("needsConfirmation"));
        const QJsonObject asked = waitForLast(s.appA, QStringLiteral("confirm.request"), 1);
        QCOMPARE(asked.value(QStringLiteral("affected")).toArray().first().toObject()
                     .value(QStringLiteral("slices")).toArray().size(),
                 2);
        QCOMPARE(s.diversityGain(), 0.0);
        // The original write is not answered twice.
        QCOMPARE(countOf(s.appA, QStringLiteral("property.result")), 1);
        QCOMPARE(s.proceed(s.appA, asked.value(QStringLiteral("id")).toInteger())
                     .value(QStringLiteral("accepted")).toBool(false),
                 true);
        QCOMPARE(s.diversityGain(), 6.0);
    }

    void aSharedQuestionIsDroppedWhenTheSessionEnds()
    {
        SharedAdc s;
        s.writeDiversityGain(s.appA, 6.0);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        s.appA->closeLink(QStringLiteral("lost"));
        QTRY_COMPARE(s.core.sessions().entry(s.a.key.fingerprint())->state,
                     DeviceSessionRegistry::State::Away);
        LoopbackTransport* back = s.core.signIn(s.a);
        QVERIFY(admitted(back));
        const QJsonObject stale = s.proceed(back, ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(stale.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("That question is no longer open. Make the change again."));
        QCOMPARE(s.diversityGain(), 0.0);
    }

    void aSettingsWriteIsHeldAndItsProceedCarriesTheReadback()
    {
        SharedAdc s;
        // Ruling 7.1a: a Tuner Genius setting asks (the receive options
        // now apply at once and tell).
        const QString key = QStringLiteral("TGXL_AntLabel_1");
        const QString before = s.core.settings->value(key).toString();
        s.appA->sendText(SessionMessages::encode(
            SessionMessages::settingsWrite(key, QStringLiteral("Beam"), QStringLiteral("a-1"))));
        const QJsonObject reject = waitForLast(s.appA, QStringLiteral("settings.reject"), 0);
        QCOMPARE(reject.value(QStringLiteral("key")).toString(), key);
        QCOMPARE(reject.value(QStringLiteral("reason")).toString(), kWaiting);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("forSettingsKey")).toString(), key);
        QCOMPARE(ask.value(QStringLiteral("change")).toObject().value(QStringLiteral("label")).toString(),
                 QStringLiteral("Tuner setting"));
        QCOMPARE(s.core.settings->value(key).toString(), before);

        const QJsonObject done = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(done.value(QStringLiteral("accepted")).toBool(false), true);
        QCOMPARE(valueOf(done, QStringLiteral("settingsKey")).toString(), key);
        QCOMPARE(valueOf(done, QStringLiteral("value")).toString(), QStringLiteral("Beam"));
        QCOMPARE(s.core.settings->value(key).toString(), QStringLiteral("Beam"));
        QCOMPARE(waitForLast(s.appB, QStringLiteral("notice"), 0).value(QStringLiteral("kind")).toString(),
                 QStringLiteral("settingChanged"));
    }

    // Fix wave I3: removing a setting returns it to its default, live, so
    // it is a write of the default: asked when it reaches another device,
    // applied only on proceed, and the other device told.
    void aSettingsRemoveIsAskedAsAWriteOfItsDefault()
    {
        SharedAdc s;
        // Ruling 7.1a: a Tuner Genius setting asks (the receive options
        // now apply at once and tell).
        const QString key = QStringLiteral("TGXL_AntLabel_1");
        s.core.settings->setValue(key, QStringLiteral("Beam"));
        s.appA->sendText(SessionMessages::encode(SessionMessages::settingsRemove(key)));
        const QJsonObject reject = waitForLast(s.appA, QStringLiteral("settings.reject"), 0);
        QCOMPARE(reject.value(QStringLiteral("key")).toString(), key);
        QCOMPARE(reject.value(QStringLiteral("reason")).toString(), kWaiting);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("sharedSetting"));
        QCOMPARE(ask.value(QStringLiteral("forSettingsKey")).toString(), key);
        const QJsonObject change = ask.value(QStringLiteral("change")).toObject();
        QCOMPARE(change.value(QStringLiteral("label")).toString(),
                 QStringLiteral("Tuner setting"));
        QCOMPARE(change.value(QStringLiteral("from")).toString(), QStringLiteral("Beam"));
        QCOMPARE(change.value(QStringLiteral("to")).toString(), QStringLiteral("Default"));
        QVERIFY(OperatorWording::isPlain(change.value(QStringLiteral("to")).toString()));
        QCOMPARE(s.core.settings->value(key).toString(), QStringLiteral("Beam"));

        const QJsonObject done = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger());
        QVERIFY2(done.value(QStringLiteral("accepted")).toBool(false),
                 QJsonDocument(done).toJson().constData());
        QCOMPARE(valueOf(done, QStringLiteral("settingsKey")).toString(), key);
        QVERIFY(!s.core.settings->contains(key));
        QCOMPARE(waitForLast(s.appB, QStringLiteral("notice"), 0).value(QStringLiteral("kind")).toString(),
                 QStringLiteral("settingChanged"));
    }

    // Alone on the Core, the same removal applies at once.
    void aloneASettingsRemoveAppliesAtOnce()
    {
        SharedAdc s(/*withB=*/false);
        const QString key = QStringLiteral("DspOptionsBufferSizePhoneRx");
        s.core.settings->setValue(key, QStringLiteral("1024"));
        s.appA->sendText(SessionMessages::encode(SessionMessages::settingsRemove(key)));
        QTRY_VERIFY(!s.core.settings->contains(key));
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), 0);
        QCOMPARE(countOf(s.appA, QStringLiteral("settings.reject")), 0);
    }

    // Fix wave (the D53 list): diversity's phase, gain and fine null, and
    // turning 4O3A on or off, reach every receiver on the HL2's one ADC, so
    // each asks while another device listens.
    void diversityKnobsAndFourO3AAreOnTheList()
    {
        SharedAdc s;
        QVERIFY(admitted(s.appB));
        const QByteArray own = ObjectRegistry::keyForSlice(0);
        const auto askFor = [&](const QList<MirrorUpdate>& updates, quint32 writeId) {
            const int before = countOf(s.appA, QStringLiteral("confirm.request"));
            s.appA->sendText(SessionMessages::encode(
                SessionMessages::propertyWrite(own, updates, writeId)));
            return waitForLast(s.appA, QStringLiteral("confirm.request"), before);
        };
        const struct {
            const char* name;
            MirrorUpdate update;
            const char* label;
        } knobs[] = {
            {"phase", f64("diversityPhaseDeg", 45.0), "Diversity phase"},
            {"gain", f64("diversityGainDb", 6.0), "Diversity gain"},
            {"fine null",
             MirrorUpdate{0, QByteArrayLiteral("diversityFineNullEnabled"), MirrorWireKind::Bool,
                          QVariant(true)},
             "Diversity fine null"},
        };
        quint32 writeId = 700;
        for (const auto& knob : knobs) {
            const QJsonObject ask = askFor({knob.update}, writeId++);
            QVERIFY2(!ask.isEmpty(), knob.name);
            QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("sharedSetting"));
            QCOMPARE(ask.value(QStringLiteral("change")).toObject()
                         .value(QStringLiteral("label")).toString(),
                     QString::fromLatin1(knob.label));
            QCOMPARE(ask.value(QStringLiteral("affected")).toArray().first().toObject()
                         .value(QStringLiteral("deviceName")).toString(),
                     QStringLiteral("iPad"));
        }
        QCOMPARE(s.core.model->sliceById(0)->diversityPhaseDeg(), 0.0);

        const int before = countOf(s.appA, QStringLiteral("confirm.request"));
        const bool was = s.core.model->fourO3AEnabled();
        s.appA->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
            "setFourO3AEnabled", 4242,
            {MirrorUpdate{0, QByteArrayLiteral("enabled"), MirrorWireKind::Bool, QVariant(!was)}})));
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), before);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("sharedSetting"));
        const QJsonObject change = ask.value(QStringLiteral("change")).toObject();
        QCOMPARE(change.value(QStringLiteral("label")).toString(),
                 QStringLiteral("4O3A amplifier and tuner"));
        QCOMPARE(change.value(QStringLiteral("to")).toString(),
                 was ? QStringLiteral("Off") : QStringLiteral("On"));
        QCOMPARE(s.core.model->fourO3AEnabled(), was);
    }

    // Checkpoint join: the parity lane's verbs and the two-way Alex
    // transmit side. The tuner's relays and the RF-Kit's antenna reach
    // ADC0 (ruling 7.2), every receiver on the HL2, so each asks while B
    // listens; Disable RX Bypass relay moves the receive side's relay, so
    // it asks too. A transmit antenna, a transmit relay and the amp's
    // operate touch only the transmitter, which nobody holds yet, so they
    // go straight to the Core's own answer.
    void theParityVerbsAndTheAlexTransmitSideAreOnTheList()
    {
        SharedAdc s(/*withB=*/true, /*permittedTransmitter=*/true);
        QVERIFY(admitted(s.appB));
        QVERIFY(txPermitted(s.appA));
        s.core.model->enableStationAccessoryIdentity();
        const auto asks = [&](const QByteArray& verb, const QList<MirrorUpdate>& args,
                              quint32 id) {
            const int before = countOf(s.appA, QStringLiteral("confirm.request"));
            s.appA->sendText(SessionMessages::encode(SessionMessages::commandInvoke(verb, id, args)));
            return waitForLast(s.appA, QStringLiteral("confirm.request"), before);
        };
        const QJsonObject relay = asks("moveTgxlRelay", {int64("relay", 1), int64("direction", 1)}, 4301);
        QCOMPARE(relay.value(QStringLiteral("kind")).toString(), QStringLiteral("sharedSetting"));
        QCOMPARE(relay.value(QStringLiteral("change")).toObject()
                     .value(QStringLiteral("label")).toString(),
                 QStringLiteral("Tuner relays"));
        QCOMPARE(relay.value(QStringLiteral("affected")).toArray().first().toObject()
                     .value(QStringLiteral("deviceName")).toString(),
                 QStringLiteral("iPad"));
        const QJsonObject antenna = asks("setRfKitAntenna", {int64("port", 2)}, 4302);
        QCOMPARE(antenna.value(QStringLiteral("change")).toObject()
                     .value(QStringLiteral("label")).toString(),
                 QStringLiteral("RF-Kit antenna"));

        // The transmitter alone: no question, the Core's own answer.
        const int asked = countOf(s.appA, QStringLiteral("confirm.request"));
        const QJsonObject operate = s.core.invoke(
            s.appA, "setPgxlOperate",
            {MirrorUpdate{0, QByteArrayLiteral("on"), MirrorWireKind::Bool, QVariant(true)}});
        QVERIFY(!operate.isEmpty());
        QVERIFY(operate.value(QStringLiteral("reason")).toString() != kWaiting);
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), asked);

        // The Alex facade: Disable RX Bypass relay asks; Block TX on Ant 2
        // does not.
        const int beforeOverride = countOf(s.appA, QStringLiteral("confirm.request"));
        s.appA->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "alexAntennas",
            {MirrorUpdate{0, QByteArrayLiteral("rxOutOverride"), MirrorWireKind::Bool,
                          QVariant(!s.core.model->alexController().rxOutOverride())}},
            4303)));
        const QJsonObject bypass =
            waitForLast(s.appA, QStringLiteral("confirm.request"), beforeOverride);
        QCOMPARE(bypass.value(QStringLiteral("change")).toObject()
                     .value(QStringLiteral("label")).toString(),
                 QStringLiteral("Disable RX Bypass relay"));
        const int beforeBlock = countOf(s.appA, QStringLiteral("confirm.request"));
        const bool blocked = s.core.model->alexController().blockTxAnt2();
        s.appA->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "alexAntennas",
            {MirrorUpdate{0, QByteArrayLiteral("blockTxAnt2"), MirrorWireKind::Bool,
                          QVariant(!blocked)}},
            4304)));
        QTRY_VERIFY(!propertyResult(s.appA, 4304).isEmpty());
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), beforeBlock);
    }

    // R-IOS-30 / R-R3-49: the tuner reports its antenna 0-based (antA=0 is
    // ANT 1, TunerModel::antennaA and the mirror's antennaA carry it as
    // sent) while setTgxlAntenna's port is 1-based (activate ant=N). The
    // question names both in the operator's numbering, a tap on the antenna
    // already in use asks nothing, and a tap on any other antenna asks.
    void theTunerAntennaQuestionNumbersAntennasAsTheButtonsDo()
    {
        SharedAdc s(/*withB=*/true, /*permittedTransmitter=*/true);
        QVERIFY(admitted(s.appB));
        QVERIFY(txPermitted(s.appA));
        s.core.model->enableStationAccessoryIdentity();
        TunerModel* tuner = s.core.model->tunerModel();
        QVERIFY(tuner != nullptr);
        tuner->applyStatus({{QStringLiteral("antA"), QStringLiteral("1")}});   // ANT 2
        QCOMPARE(tuner->antennaA(), 1);
        const auto tap = [&](int port) {
            const int before = countOf(s.appA, QStringLiteral("confirm.request"));
            const QJsonObject answer =
                s.core.invoke(s.appA, "setTgxlAntenna", {int64("port", port)});
            Q_UNUSED(answer);
            return countOf(s.appA, QStringLiteral("confirm.request")) > before
                ? waitForLast(s.appA, QStringLiteral("confirm.request"), before)
                : QJsonObject();
        };

        // Another antenna: asked, in the buttons' numbers.
        const QJsonObject up = tap(3);
        QVERIFY(!up.isEmpty());
        const QJsonObject change = up.value(QStringLiteral("change")).toObject();
        QCOMPARE(change.value(QStringLiteral("label")).toString(), QStringLiteral("Tuner antenna"));
        QCOMPARE(change.value(QStringLiteral("from")).toString(), QStringLiteral("ANT2"));
        QCOMPARE(change.value(QStringLiteral("to")).toString(), QStringLiteral("ANT3"));

        // The antenna already in use: nothing changes, nothing is asked.
        QVERIFY(tap(2).isEmpty());

        // The antenna whose button number equals the raw report (ANT 1
        // while antA=1): a real change, so it asks.
        const QJsonObject down = tap(1);
        QVERIFY(!down.isEmpty());
        QCOMPARE(down.value(QStringLiteral("change")).toObject()
                     .value(QStringLiteral("from")).toString(),
                 QStringLiteral("ANT2"));
        QCOMPARE(down.value(QStringLiteral("change")).toObject()
                     .value(QStringLiteral("to")).toString(),
                 QStringLiteral("ANT1"));
    }

    // R-IOS-30 / R-R3-49: the RF-Kit amp numbers its antennas from 1 on
    // both sides (activeAntennaNumber and setRfKitAntenna's port, 0 for
    // none), but it numbers its external antennas from 1 as well; a tap on
    // internal ANT 2 while external antenna 2 is active is a change and
    // asks, and the question does not call the external antenna ANT2.
    void theRfKitAntennaQuestionNumbersAntennasAsTheButtonsDo()
    {
        SharedAdc s(/*withB=*/true, /*permittedTransmitter=*/true);
        QVERIFY(admitted(s.appB));
        QVERIFY(txPermitted(s.appA));
        s.core.model->enableStationAccessoryIdentity();
        RfKitModel* rfKit = s.core.model->rfKitModel();
        QVERIFY(rfKit != nullptr);
        const auto active = [&](RfKitAntenna::Type type, int number) {
            RfKitAntenna a;
            a.type = type;
            a.number = number;
            a.state = RfKitAntenna::State::Active;
            rfKit->applyActiveAntenna(a);
        };
        const auto tap = [&](int port) {
            const int before = countOf(s.appA, QStringLiteral("confirm.request"));
            const QJsonObject answer =
                s.core.invoke(s.appA, "setRfKitAntenna", {int64("port", port)});
            Q_UNUSED(answer);
            return countOf(s.appA, QStringLiteral("confirm.request")) > before
                ? waitForLast(s.appA, QStringLiteral("confirm.request"), before)
                    .value(QStringLiteral("change")).toObject()
                : QJsonObject();
        };

        active(RfKitAntenna::Type::Internal, 2);
        const QJsonObject up = tap(3);
        QCOMPARE(up.value(QStringLiteral("label")).toString(), QStringLiteral("RF-Kit antenna"));
        QCOMPARE(up.value(QStringLiteral("from")).toString(), QStringLiteral("ANT2"));
        QCOMPARE(up.value(QStringLiteral("to")).toString(), QStringLiteral("ANT3"));
        QVERIFY(tap(2).isEmpty());
        const QJsonObject down = tap(1);
        QCOMPARE(down.value(QStringLiteral("from")).toString(), QStringLiteral("ANT2"));
        QCOMPARE(down.value(QStringLiteral("to")).toString(), QStringLiteral("ANT1"));

        // External antenna 2 in use: internal ANT 2 is a change.
        active(RfKitAntenna::Type::External, 2);
        const QJsonObject fromExternal = tap(2);
        QVERIFY(!fromExternal.isEmpty());
        QCOMPARE(fromExternal.value(QStringLiteral("from")).toString(),
                 QStringLiteral("External antenna 2"));
        QCOMPARE(fromExternal.value(QStringLiteral("to")).toString(), QStringLiteral("ANT2"));

        // No antenna reported yet: asked, from None.
        active(RfKitAntenna::Type::Internal, 0);
        QCOMPARE(tap(1).value(QStringLiteral("from")).toString(), QStringLiteral("None"));
    }

    // Ruling 7.1a: a notch applies at once; inside another device's
    // passband that device is told, outside it nobody is.
    void aNotchInsideAnotherDevicesPassbandTellsItOneOutsideDoesNot()
    {
        SharedAdc s;
        const int notches = static_cast<int>(s.core.model->notchModel()->notches().size());
        // Outside B's passband (14.0741 to 14.077 MHz in USB): nobody told.
        QCOMPARE(s.core.invoke(s.appA, "notch.add",
                               {int64("sliceId", 0), f64("centreHz", 7075000.0), f64("widthHz", 100.0)})
                     .value(QStringLiteral("accepted")).toBool(false),
                 true);
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), 0);
        QTRY_COMPARE(static_cast<int>(s.core.model->notchModel()->notches().size()), notches + 1);
        QCOMPARE(countOf(s.appB, QStringLiteral("notice")), 0);
        // Inside it: applied at once too, and B told.
        QCOMPARE(s.core.invoke(s.appA, "notch.add",
                               {int64("sliceId", 0), f64("centreHz", 14075000.0), f64("widthHz", 100.0)})
                     .value(QStringLiteral("accepted")).toBool(false),
                 true);
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), 0);
        QTRY_COMPARE(static_cast<int>(s.core.model->notchModel()->notches().size()), notches + 2);
        const QJsonObject told = waitForLast(s.appB, QStringLiteral("notice"), 0);
        QCOMPARE(told.value(QStringLiteral("kind")).toString(), QStringLiteral("settingChanged"));
        QCOMPARE(told.value(QStringLiteral("change")).toObject()
                     .value(QStringLiteral("label")).toString(),
                 QStringLiteral("Notch"));
    }

    void anOlderWindowsSharedChangeIsRefusedNamingWhoItWouldAffect()
    {
        SharedAdc s(false);
        Device older(QStringLiteral("Shack PC"), QStringLiteral("computer"));
        s.core.pair(older);
        LoopbackTransport* appOld = s.core.signIn(older, {{"deviceAuth", 1}});
        QVERIFY(admitted(appOld));
        // Ruling 7.1a: a change on an Ask row (its own slice's diversity).
        const int oldSlice =
            s.core.model->sliceOwnership()->ownedBy(older.key.fingerprint()).first();
        const quint32 writeId = 4711;
        appOld->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            ObjectRegistry::keyForSlice(oldSlice), {f64("diversityGainDb", 6.0)}, writeId)));
        QJsonObject refused;
        QVERIFY(QTest::qWaitFor([&]() {
            refused = propertyResult(appOld, writeId);
            return !refused.isEmpty();
        }, 5000));
        const QString reason = firstResultEntry(refused).value(QStringLiteral("reason")).toString();
        QCOMPARE(reason, QStringLiteral("This change would affect iPhone. Update NereusSDR to "
                                        "confirm changes that affect other devices."));
        QCOMPARE(firstResultEntry(refused).value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(s.core.model->sliceById(oldSlice)->diversityGainDb(), 0.0);
        QCOMPARE(countOf(appOld, QStringLiteral("confirm.request")), 0);

        // A change on a Notify row needs no question, so the older window
        // makes it, and the iPhone is told.
        const QJsonObject taken = s.writeStepAtt(appOld, "attenuationDb", 20);
        QCOMPARE(firstResultEntry(taken).value(QStringLiteral("accepted")).toBool(false), true);
        QCOMPARE(s.attenuation(), 20);
        QCOMPARE(waitForLast(s.appA, QStringLiteral("notice"), 0).value(QStringLiteral("kind"))
                     .toString(),
                 QStringLiteral("settingChanged"));
    }

    void aNarrowerRateOnASharedReceiverMovesTheOtherDevicesSlice()
    {
        // No connection: the rate is per receiver (Protocol 2's rule). B's
        // slice, 76 kHz from A's, no longer fits a 96 kHz window: the plan
        // moves it to the free receiver (ruling 7.3).
        Shared s;
        const int receiver = s.receiver();
        const QJsonObject held = s.core.invoke(
            s.appA, "requestSliceSampleRate", {int64("sliceId", 0), int64("rateHz", 96000)});
        QCOMPARE(held.value(QStringLiteral("reason")).toString(), kWaiting);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        const QJsonObject change = ask.value(QStringLiteral("change")).toObject();
        QCOMPARE(change.value(QStringLiteral("label")).toString(),
                 QStringLiteral("Sample rate, Receiver %1").arg(receiver + 1));
        QCOMPARE(change.value(QStringLiteral("from")).toString(), QStringLiteral("192 kHz"));
        QCOMPARE(change.value(QStringLiteral("to")).toString(), QStringLiteral("96 kHz"));
        const QJsonObject slice = ask.value(QStringLiteral("affected")).toArray().first().toObject()
                                      .value(QStringLiteral("slices")).toArray().first().toObject();
        QCOMPARE(slice.value(QStringLiteral("sliceId")).toInt(), 1);
        QCOMPARE(slice.value(QStringLiteral("effect")).toString(), QStringLiteral("moves"));
        QCOMPARE(ask.value(QStringLiteral("forCommandId")).toInteger(),
                 held.value(QStringLiteral("id")).toInteger());
        QCOMPARE(streamOf(s.core, 1), receiver);

        const QJsonObject done = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(done.value(QStringLiteral("accepted")).toBool(false), true);
        QCOMPARE(s.core.model->streamAllocator().streamSampleRateHz(receiver), 96000);
        QVERIFY(streamOf(s.core, 1) != receiver);
        QVERIFY(streamOf(s.core, 1) >= 0);
        const QJsonObject told = waitForLast(s.appB, QStringLiteral("notice"), 0);
        QCOMPARE(told.value(QStringLiteral("kind")).toString(), QStringLiteral("settingChanged"));
        QCOMPARE(told.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("iPhone changed Sample rate, Receiver %1 from 192 kHz to 96 kHz. "
                                "Your slice B moved to another receiver.")
                     .arg(receiver + 1));
    }

    // Fix wave I1: a proceed answered when its held rate change's result
    // arrives is keyed by its session. B's own rate change, sent with the
    // same command id as A's held one, is B's result, never A's proceed
    // answer, and A's result never reaches B.
    void aDeferredProceedIsNotFinishedByAnotherDevicesResultWithTheSameId()
    {
        Shared s(3);
        QVERIFY(s.core.invoke(s.appB, "addSliceOnPan", {utf8("panId", QStringLiteral("pan-b2"))})
                    .value(QStringLiteral("accepted")).toBool());
        const int bOwn = s.core.model->sliceOwnership()->ownedBy(s.b.key.fingerprint()).last();
        s.core.model->sliceById(bOwn)->setFrequency(14074000.0);
        QVERIFY(streamOf(s.core, bOwn) != s.receiver());
        const quint32 sameId = 7777;
        const auto resultsFor = [](const LoopbackTransport* app, quint32 id) {
            QList<QJsonObject> out;
            for (const QJsonObject& o : ofType(app->received(), QStringLiteral("command.result"))) {
                if (o.value(QStringLiteral("id")).toInteger() == id) {
                    out.append(o);
                }
            }
            return out;
        };
        s.appA->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
            "requestSliceSampleRate", sameId, {int64("sliceId", 0), int64("rateHz", 96000)})));
        QTRY_VERIFY(!resultsFor(s.appA, sameId).isEmpty());
        QCOMPARE(resultsFor(s.appA, sameId).first().value(QStringLiteral("reason")).toString(),
                 kWaiting);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("sharedSetting"));
        // B's own rate change first, then A's proceed, before the Core's
        // event loop runs either change.
        const quint32 proceedId = 7778;
        s.appB->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
            "requestSliceSampleRate", sameId, {int64("sliceId", bOwn), int64("rateHz", 96000)})));
        s.appA->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
            "confirm.proceed", proceedId,
            {int64("id", ask.value(QStringLiteral("id")).toInteger()), int64("choice", -1)})));
        QTRY_VERIFY(!resultsFor(s.appA, proceedId).isEmpty() && !resultsFor(s.appB, sameId).isEmpty());
        QTest::qWait(2 * StationServer::kDefaultDeltaFlushMs);
        const QList<QJsonObject> proceeds = resultsFor(s.appA, proceedId);
        QCOMPARE(proceeds.size(), 1);
        QVERIFY2(proceeds.first().value(QStringLiteral("accepted")).toBool(false),
                 QJsonDocument(proceeds.first()).toJson().constData());
        const QStringList proceedAffected = [&]() {
            QStringList keys;
            for (const QJsonValue& v : proceeds.first().value(QStringLiteral("affected")).toArray()) {
                keys.append(v.toString());
            }
            return keys;
        }();
        QVERIFY2(proceedAffected.contains(QStringLiteral("slice:0")),
                 qPrintable(proceedAffected.join(QLatin1Char(','))));
        QVERIFY(!proceedAffected.contains(QStringLiteral("slice:%1").arg(bOwn)));
        // B hears its own result, once, and A's rate change is not it.
        const QList<QJsonObject> bResults = resultsFor(s.appB, sameId);
        QCOMPARE(bResults.size(), 1);
        QVERIFY(bResults.first().value(QStringLiteral("accepted")).toBool(false));
        for (const QJsonValue& v : bResults.first().value(QStringLiteral("affected")).toArray()) {
            QVERIFY2(v.toString() != QStringLiteral("slice:0"), "A's result reached B");
        }
        // A heard no second answer to its held rate change.
        QCOMPARE(resultsFor(s.appA, sameId).size(), 1);
        QCOMPARE(s.core.model->streamAllocator().streamSampleRateHz(s.receiver()), 96000);
    }

    void aNarrowerRateWithNoReceiverFreeClosesTheOtherDevicesSlice()
    {
        Shared s(1);
        const QJsonObject held = s.core.invoke(
            s.appA, "requestSliceSampleRate", {int64("sliceId", 0), int64("rateHz", 96000)});
        QCOMPARE(held.value(QStringLiteral("reason")).toString(), kWaiting);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("affected")).toArray().first().toObject()
                     .value(QStringLiteral("slices")).toArray().first().toObject()
                     .value(QStringLiteral("effect")).toString(),
                 QStringLiteral("closes"));
        QCOMPARE(s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger())
                     .value(QStringLiteral("accepted")).toBool(false),
                 true);
        QVERIFY(s.core.model->sliceById(1) == nullptr);
        QCOMPARE(s.core.model->streamAllocator().streamSampleRateHz(s.receiver()), 96000);
        QVERIFY(waitForLast(s.appB, QStringLiteral("notice"), 0)
                    .value(QStringLiteral("reason")).toString()
                    .endsWith(QStringLiteral("Your slice B closed: no receiver was free.")));
    }

    // Fix wave: a rate proceed closes another device's slice only once the
    // change succeeds. Here the change is refused on its later turn (a
    // slice nobody owns arrives, outside the narrower window, before it
    // runs), so B's slice stays, B is told nothing, and the rate stays.
    void aRefusedRateProceedClosesNothing()
    {
        Shared s(1);
        const QJsonObject held = s.core.invoke(
            s.appA, "requestSliceSampleRate", {int64("sliceId", 0), int64("rateHz", 96000)});
        QCOMPARE(held.value(QStringLiteral("reason")).toString(), kWaiting);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        const quint32 proceedId = 8801;
        s.appA->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
            "confirm.proceed", proceedId,
            {int64("id", ask.value(QStringLiteral("id")).toInteger()), int64("choice", -1)})));
        // Queued behind the proceed's delivery, so it runs after the
        // proceed and before the rate change the proceed queues.
        RadioModel* model = s.core.model.get();
        QMetaObject::invokeMethod(model, [model]() {
            const int id = model->addSlice(QStringLiteral("pan-0"));
            model->sliceById(id)->setFrequency(7160000.0);
        }, Qt::QueuedConnection);
        QJsonObject done;
        QTRY_VERIFY([&]() {
            for (const QJsonObject& o : ofType(s.appA->received(), QStringLiteral("command.result"))) {
                if (o.value(QStringLiteral("id")).toInteger() == proceedId) {
                    done = o;
                }
            }
            return !done.isEmpty();
        }());
        QVERIFY2(!done.value(QStringLiteral("accepted")).toBool(true),
                 QJsonDocument(done).toJson().constData());
        // Refused by the rate change itself, not by the proceed's checks.
        QCOMPARE(done.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("Sample-rate change to 96 kHz was rejected; all slices "
                                "stayed on their existing DDC windows."));
        QTest::qWait(2 * StationServer::kDefaultDeltaFlushMs);
        // B still owns its slice 1 (a closed id would be reused by the new
        // slice, so the owner is what shows it stayed) at 7.150 MHz.
        QCOMPARE(s.core.model->sliceOwnership()->ownedBy(s.b.key.fingerprint()), QList<int>{1});
        QCOMPARE(s.core.model->sliceById(1)->frequency(), 7150000.0);
        QCOMPARE(s.core.model->streamAllocator().streamSampleRateHz(s.receiver()), 192000);
        QCOMPARE(countOf(s.appB, QStringLiteral("notice")), 0);
    }

    // Fix wave 2 (re-review Minor 2): on Protocol 1 the rate is the radio's
    // own, and a confirmed change closes the other device's slice between
    // setSampleRateLive taking the rate and the commit. B's slice closes,
    // B is told, the radio's wire carries the new rate and the receiver
    // runs at it.
    void aProtocol1RateProceedClosesTheOtherDevicesSlice()
    {
        Shared s(1);
        WdspEngine* wdsp = s.core.model->wdspEngine();
        // Initialized, with no receive channel opened: setSampleRateLive
        // needs the engine up, and opening channels would plan their FFTs.
        wdsp->m_initialized = true;  // friend access (NEREUS_BUILD_TESTS)
        P1RadioConnection conn;
        conn.restartStreamWithRate(192000);
        s.core.model->injectConnectionForTest(&conn);
        const auto detach = qScopeGuard([&s] { s.core.model->injectConnectionForTest(nullptr); });
        QVERIFY(s.core.model->sampleRateIsRadioWide());
        QCOMPARE(static_cast<quint8>(conn.captureBank0ForTest().at(1)), quint8(2));

        const QJsonObject held = s.core.invoke(
            s.appA, "requestSliceSampleRate", {int64("sliceId", 0), int64("rateHz", 96000)});
        QCOMPARE(held.value(QStringLiteral("reason")).toString(), kWaiting);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        const QJsonObject slice = ask.value(QStringLiteral("affected")).toArray().first().toObject()
                                      .value(QStringLiteral("slices")).toArray().first().toObject();
        QCOMPARE(slice.value(QStringLiteral("sliceId")).toInt(), 1);
        QCOMPARE(slice.value(QStringLiteral("effect")).toString(), QStringLiteral("closes"));
        QCOMPARE(s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger())
                     .value(QStringLiteral("accepted")).toBool(false),
                 true);
        // Merge with the transmit lane (R-R3-39): the change runs on the
        // receive lane, and the slice closes when it has finished.
        QTRY_VERIFY(s.core.model->sliceById(1) == nullptr);
        QVERIFY(s.core.model->sliceOwnership()->ownedBy(s.b.key.fingerprint()).isEmpty());
        QCOMPARE(s.core.model->streamAllocator().streamSampleRateHz(s.receiver()), 96000);
        QTRY_COMPARE(static_cast<quint8>(conn.captureBank0ForTest().at(1)), quint8(1));
        QVERIFY(waitForLast(s.appB, QStringLiteral("notice"), 0)
                    .value(QStringLiteral("reason")).toString()
                    .endsWith(QStringLiteral("Your slice B closed: no receiver was free.")));
    }

    // Parity ruling C4: setRadioSampleRate (radioHardwareVersion 9) is a
    // remote window's Radio Info sample rate, the change a local window
    // makes (RadioModel::setSampleRateLive's path): every receiver and the
    // radio's own rate, which new receivers take. Alone on the Core it
    // applies at once.
    void aRadioSampleRateChangeMovesEveryReceiverAndTheRadiosRate()
    {
        Core core;
        core.model->configureStreamPool(2, 5, 192000);
        core.model->sliceById(0)->setFrequency(7074000.0);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a);
        QVERIFY(admitted(appA));
        const int second = core.model->addSlice(QStringLiteral("pan-a2"));
        core.model->sliceById(second)->setFrequency(14074000.0);
        const int first = streamOf(core, 0);
        const int other = streamOf(core, second);
        QVERIFY(first >= 0 && other >= 0 && first != other);
        WdspEngine* wdsp = core.model->wdspEngine();
        wdsp->m_initialized = true;  // friend access (NEREUS_BUILD_TESTS)
        P1RadioConnection conn;
        conn.restartStreamWithRate(192000);
        core.model->injectConnectionForTest(&conn);
        const auto detach = qScopeGuard([&core] { core.model->injectConnectionForTest(nullptr); });
        // The radio's rate as a connect leaves it.
        QVERIFY(core.model->setSampleRateLive(192000) >= 0);
        QCOMPARE(core.model->connectionSampleRateHz(), 192000);

        const QJsonObject done =
            core.invoke(appA, "setRadioSampleRate", {int64("rateHz", 96000)});
        QVERIFY2(done.value(QStringLiteral("accepted")).toBool(false),
                 QJsonDocument(done).toJson().constData());
        QTRY_COMPARE(core.model->connectionSampleRateHz(), 96000);
        QCOMPARE(core.model->streamAllocator().streamSampleRateHz(first), 96000);
        QCOMPARE(core.model->streamAllocator().streamSampleRateHz(other), 96000);
        QTRY_COMPARE(static_cast<quint8>(conn.captureBank0ForTest().at(1)), quint8(1));

        // The rate it is at: accepted, nothing moves.
        const QJsonObject same =
            core.invoke(appA, "setRadioSampleRate", {int64("rateHz", 96000)});
        QVERIFY(same.value(QStringLiteral("accepted")).toBool(false));
        QVERIFY(same.value(QStringLiteral("affected")).toArray().isEmpty());

        // A rate this radio cannot run, and a request it cannot read.
        const QJsonObject odd =
            core.invoke(appA, "setRadioSampleRate", {int64("rateHz", 12345)});
        QVERIFY(!odd.value(QStringLiteral("accepted")).toBool(true));
        QCOMPARE(odd.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("This radio cannot run at that sample rate."));
        const QJsonObject unread =
            core.invoke(appA, "setRadioSampleRate", {int64("rate", 96000)});
        QCOMPARE(unread.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("The Core could not read this request."));
        QCOMPARE(core.model->connectionSampleRateHz(), 96000);
    }

    // Parity ruling C4 and ruling 7.1: a radio-wide rate disturbs every
    // other device's slices, so it is asked of them first, then applies to
    // every receiver and they are told.
    void aRadioSampleRateChangeIsAskedOfTheOtherDevices()
    {
        Shared s(1);
        WdspEngine* wdsp = s.core.model->wdspEngine();
        wdsp->m_initialized = true;  // friend access (NEREUS_BUILD_TESTS)
        P1RadioConnection conn;
        conn.restartStreamWithRate(192000);
        s.core.model->injectConnectionForTest(&conn);
        const auto detach = qScopeGuard([&s] { s.core.model->injectConnectionForTest(nullptr); });
        QVERIFY(s.core.model->setSampleRateLive(192000) >= 0);

        const QJsonObject held =
            s.core.invoke(s.appA, "setRadioSampleRate", {int64("rateHz", 96000)});
        QCOMPARE(held.value(QStringLiteral("reason")).toString(), kWaiting);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        const QJsonObject change = ask.value(QStringLiteral("change")).toObject();
        QCOMPARE(change.value(QStringLiteral("label")).toString(), QStringLiteral("Sample rate"));
        QCOMPARE(change.value(QStringLiteral("from")).toString(), QStringLiteral("192 kHz"));
        QCOMPARE(change.value(QStringLiteral("to")).toString(), QStringLiteral("96 kHz"));
        const QJsonObject slice = ask.value(QStringLiteral("affected")).toArray().first().toObject()
                                      .value(QStringLiteral("slices")).toArray().first().toObject();
        QCOMPARE(slice.value(QStringLiteral("sliceId")).toInt(), 1);
        QCOMPARE(s.core.model->connectionSampleRateHz(), 192000);

        const QJsonObject done = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger());
        QVERIFY2(done.value(QStringLiteral("accepted")).toBool(false),
                 QJsonDocument(done).toJson().constData());
        QTRY_COMPARE(s.core.model->connectionSampleRateHz(), 96000);
        QCOMPARE(s.core.model->streamAllocator().streamSampleRateHz(s.receiver()), 96000);
        const QJsonObject told = waitForLast(s.appB, QStringLiteral("notice"), 0);
        QCOMPARE(told.value(QStringLiteral("kind")).toString(), QStringLiteral("settingChanged"));
        QVERIFY(told.value(QStringLiteral("reason")).toString().startsWith(
            QStringLiteral("iPhone changed Sample rate from 192 kHz to 96 kHz.")));
    }

    // Parity ruling C4: like the Core's other radio-wide changes, the rate
    // waits while the radio is on the air.
    void aRadioSampleRateChangeWaitsOffTheAir()
    {
        Shared s(2, 5, kTransmitter);
        allowTransmit(s.core);
        s.core.model->sliceById(1)->setDspMode(DSPMode::USB);
        s.core.model->sliceById(1)->setFrequency(14200000.0);
        QVERIFY(s.core.model->txSliceArbiter()->requestHandoff(1));
        MoxController* mox = s.core.model->moxController();
        mox->setMox(true, keyerFor(s.b));
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        const QJsonObject refused =
            s.core.invoke(s.appA, "setRadioSampleRate", {int64("rateHz", 96000)});
        QVERIFY(!refused.value(QStringLiteral("accepted")).toBool(true));
        QCOMPARE(refused.value(QStringLiteral("reason")).toString(), kRadioOnAir);
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), 0);
        mox->setMox(false, keyerFor(s.b));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    // Fix wave 2 (Important 4): the proceed confirms closing B's slice 1.
    // Before the change runs on its later turn, B closes slice 1 and A's
    // new slice takes id 1. The change is refused as changed and closes
    // nothing: A's new slice stays and the rate stays.
    void aRateProceedWhoseClosingSliceIdWasReusedIsRefused()
    {
        Shared s(1);
        const QJsonObject held = s.core.invoke(
            s.appA, "requestSliceSampleRate", {int64("sliceId", 0), int64("rateHz", 96000)});
        QCOMPARE(held.value(QStringLiteral("reason")).toString(), kWaiting);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        const quint32 proceedId = 8811;
        s.appA->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
            "confirm.proceed", proceedId,
            {int64("id", ask.value(QStringLiteral("id")).toInteger()), int64("choice", -1)})));
        // Queued behind the proceed's delivery: B removes its slice 1, and
        // A's new slice takes the free id 1.
        RadioModel* model = s.core.model.get();
        const QByteArray aKey = s.a.key.fingerprint();
        int reused = -1;
        QMetaObject::invokeMethod(model, [model, aKey, &reused]() {
            model->removeSlice(1);
            const SliceOwnership::CreatorScope creator(model->sliceOwnership(), aKey);
            reused = model->addSlice(QStringLiteral("pan-0"));
        }, Qt::QueuedConnection);
        QJsonObject done;
        QTRY_VERIFY([&]() {
            for (const QJsonObject& o : ofType(s.appA->received(), QStringLiteral("command.result"))) {
                if (o.value(QStringLiteral("id")).toInteger() == proceedId) {
                    done = o;
                }
            }
            return !done.isEmpty();
        }());
        QCOMPARE(reused, 1);
        QVERIFY2(!done.value(QStringLiteral("accepted")).toBool(true),
                 QJsonDocument(done).toJson().constData());
        QCOMPARE(done.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("That setting changed since you asked. Make the change again."));
        QTest::qWait(2 * StationServer::kDefaultDeltaFlushMs);
        QVERIFY(s.core.model->sliceById(1) != nullptr);
        QCOMPARE(s.core.model->sliceOwnership()->mark(1).owner, aKey);
        QCOMPARE(s.core.model->streamAllocator().streamSampleRateHz(s.receiver()), 192000);
    }

    // Fix wave 2 (Important 4, the re-review's first out-of-scope item): a
    // rate change A asks for on its own slice runs on a later turn. Before
    // it does, A's slice closes and B's new slice takes its id. The change
    // is refused as B's slice, and B's receiver keeps its rate.
    void aRateChangeWhoseSliceIdWasReusedBeforeItRanIsRefused()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.model->configureStreamPool(5, 5, 192000);
        core.model->sliceById(0)->setFrequency(7074000.0);
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appA) && admitted(appB));
        const int bSlice = core.model->sliceOwnership()->ownedBy(b.key.fingerprint()).first();
        core.model->sliceById(bSlice)->setFrequency(14074000.0);
        QCOMPARE(core.model->sliceOwnership()->mark(0).owner, a.key.fingerprint());
        QVERIFY(streamOf(core, 0) != streamOf(core, bSlice));

        const quint32 rateId = 8821;
        appA->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
            "requestSliceSampleRate", rateId, {int64("sliceId", 0), int64("rateHz", 96000)})));
        RadioModel* model = core.model.get();
        const QByteArray bKey = b.key.fingerprint();
        int reused = -1;
        // Queued behind A's request, ahead of the change it queues.
        QMetaObject::invokeMethod(model, [model, bKey, &reused]() {
            model->removeSlice(0);
            const SliceOwnership::CreatorScope creator(model->sliceOwnership(), bKey);
            reused = model->addSlice(QStringLiteral("pan-b2"));
            model->sliceById(reused)->setFrequency(3573000.0);
        }, Qt::QueuedConnection);
        QJsonObject done;
        QTRY_VERIFY([&]() {
            for (const QJsonObject& o : ofType(appA->received(), QStringLiteral("command.result"))) {
                if (o.value(QStringLiteral("id")).toInteger() == rateId) {
                    done = o;
                }
            }
            return !done.isEmpty();
        }());
        QCOMPARE(reused, 0);
        QCOMPARE(core.model->sliceOwnership()->mark(0).owner, bKey);
        QVERIFY2(!done.value(QStringLiteral("accepted")).toBool(true),
                 QJsonDocument(done).toJson().constData());
        QVERIFY2(done.value(QStringLiteral("reason")).toString()
                     .startsWith(QStringLiteral("That slice belongs to iPad.")),
                 qPrintable(done.value(QStringLiteral("reason")).toString()));
        QTest::qWait(50);
        QCOMPARE(core.model->sliceById(0)->sampleRateHz(), 192000);
    }

    // Fix wave 3 (Important 1): the re-review's case. The proceed confirms
    // closing B's slice 1; before the change runs, B closes slice 1 and
    // makes a new slice in its own name, which takes id 1 with the same
    // owner. The change is refused as changed, and B's new slice stays.
    void aRateProceedWhoseClosingSliceIdWasReusedBySameOwnerIsRefused()
    {
        Shared s(1);
        const QJsonObject held = s.core.invoke(
            s.appA, "requestSliceSampleRate", {int64("sliceId", 0), int64("rateHz", 96000)});
        QCOMPARE(held.value(QStringLiteral("reason")).toString(), kWaiting);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        const quint32 proceedId = 8831;
        s.appA->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
            "confirm.proceed", proceedId,
            {int64("id", ask.value(QStringLiteral("id")).toInteger()), int64("choice", -1)})));
        RadioModel* model = s.core.model.get();
        const QByteArray bKey = s.b.key.fingerprint();
        int reused = -1;
        QMetaObject::invokeMethod(model, [model, bKey, &reused]() {
            // B's slice's own pan (a new slice for B on A's pan would need
            // a receiver of its own).
            const QString pan = model->sliceById(1)->panKey();
            model->removeSlice(1);
            const SliceOwnership::CreatorScope creator(model->sliceOwnership(), bKey);
            reused = model->addSlice(pan);
        }, Qt::QueuedConnection);
        QJsonObject done;
        QTRY_VERIFY([&]() {
            for (const QJsonObject& o : ofType(s.appA->received(), QStringLiteral("command.result"))) {
                if (o.value(QStringLiteral("id")).toInteger() == proceedId) {
                    done = o;
                }
            }
            return !done.isEmpty();
        }());
        QCOMPARE(reused, 1);
        QCOMPARE(s.core.model->sliceOwnership()->mark(1).owner, bKey);
        QVERIFY2(!done.value(QStringLiteral("accepted")).toBool(true),
                 QJsonDocument(done).toJson().constData());
        QCOMPARE(done.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("That setting changed since you asked. Make the change again."));
        QTest::qWait(2 * StationServer::kDefaultDeltaFlushMs);
        QVERIFY(s.core.model->sliceById(1) != nullptr);
        QCOMPARE(s.core.model->sliceOwnership()->ownedBy(bKey), QList<int>{1});
        QCOMPARE(s.core.model->streamAllocator().streamSampleRateHz(s.receiver()), 192000);
    }

    // Fix wave 3 (Important 1), the asynchronous path: A asks for a rate on
    // its own slice 0; before the change runs, A closes slice 0 and makes a
    // new slice in its own name, which takes id 0. The change is refused
    // (the slice asked about is gone) and the new slice keeps its rate.
    void aRateChangeWhoseSliceIdWasReusedBySameOwnerBeforeItRanIsRefused()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.model->configureStreamPool(5, 5, 192000);
        core.model->sliceById(0)->setFrequency(7074000.0);
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appA) && admitted(appB));
        const int bSlice = core.model->sliceOwnership()->ownedBy(b.key.fingerprint()).first();
        core.model->sliceById(bSlice)->setFrequency(14074000.0);
        QCOMPARE(core.model->sliceOwnership()->mark(0).owner, a.key.fingerprint());
        QVERIFY(streamOf(core, 0) != streamOf(core, bSlice));

        const quint32 rateId = 8841;
        appA->sendText(SessionMessages::encode(SessionMessages::commandInvoke(
            "requestSliceSampleRate", rateId, {int64("sliceId", 0), int64("rateHz", 96000)})));
        RadioModel* model = core.model.get();
        const QByteArray aKey = a.key.fingerprint();
        int reused = -1;
        QMetaObject::invokeMethod(model, [model, aKey, &reused]() {
            model->removeSlice(0);
            const SliceOwnership::CreatorScope creator(model->sliceOwnership(), aKey);
            reused = model->addSlice(QStringLiteral("pan-a2"));
            model->sliceById(reused)->setFrequency(3573000.0);
        }, Qt::QueuedConnection);
        QJsonObject done;
        QTRY_VERIFY([&]() {
            for (const QJsonObject& o : ofType(appA->received(), QStringLiteral("command.result"))) {
                if (o.value(QStringLiteral("id")).toInteger() == rateId) {
                    done = o;
                }
            }
            return !done.isEmpty();
        }());
        QCOMPARE(reused, 0);
        QCOMPARE(core.model->sliceOwnership()->mark(0).owner, aKey);
        QVERIFY2(!done.value(QStringLiteral("accepted")).toBool(true),
                 QJsonDocument(done).toJson().constData());
        QCOMPARE(done.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("That receiver is no longer on the Core."));
        QTest::qWait(50);
        QCOMPARE(core.model->sliceById(0)->sampleRateHz(), 192000);
    }

    // ── Merge of the trunk into the transmit lane: Task 34's holder ──────

    // Ruling 6.8 (D64): the receiver of a holder's transmit slice is not
    // takeable while the holder is on the air, and is again once it stops.
    void aKeyedHoldersTransmitReceiverIsNotTakeable()
    {
        Shared s(2, 5, kTransmitter);
        allowTransmit(s.core);
        // B's slice on 20 m, its own receiver, and the transmit slice.
        s.core.model->sliceById(1)->setDspMode(DSPMode::USB);
        s.core.model->sliceById(1)->setFrequency(14200000.0);
        const int bReceiver = streamOf(s.core, 1);
        QVERIFY(bReceiver >= 0 && bReceiver != s.receiver());
        QVERIFY(s.core.model->txSliceArbiter()->requestHandoff(1));
        MoxController* mox = s.core.model->moxController();
        mox->setMox(true, keyerFor(s.b));
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        QVERIFY(s.core.server->transmitHolder()->isHeldBy(s.b.key.fingerprint()));

        const auto choiceFor = [&](int before) {
            const QJsonObject refused = s.core.invoke(
                s.appA, "addSliceOnPan", {utf8("panId", QStringLiteral("pan-a2"))});
            QJsonObject found;
            if (refused.value(QStringLiteral("accepted")).toBool(true)) {
                return found;
            }
            const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), before);
            for (const QJsonValue& v : ask.value(QStringLiteral("choices")).toArray()) {
                if (v.toObject().value(QStringLiteral("streamIndex")).toInt() == bReceiver) {
                    found = v.toObject();
                }
            }
            s.core.invoke(s.appA, "confirm.cancel",
                          {int64("id", ask.value(QStringLiteral("id")).toInteger())});
            return found;
        };
        const QJsonObject onAir = choiceFor(0);
        QVERIFY(!onAir.isEmpty());
        QCOMPARE(onAir.value(QStringLiteral("takeable")).toBool(true), false);
        QCOMPARE(onAir.value(QStringLiteral("why")).toString(),
                 QStringLiteral("Tablet is on the air. Try again when they stop."));

        mox->setMox(false, keyerFor(s.b));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        const QJsonObject offAir = choiceFor(countOf(s.appA, QStringLiteral("confirm.request")));
        QVERIFY(!offAir.isEmpty());
        QCOMPARE(offAir.value(QStringLiteral("takeable")).toBool(false), true);
    }

    // Task 77 fix wave, M5 (rulings 6.8 and 8.9): after the radio's PTT
    // takes transmit from a keyed device, it keys on the frozen transmit
    // slice, whose receiver is not takeable while the radio is on the air.
    void theRadioPttsFrozenTransmitReceiverIsNotTakeable()
    {
        Shared s(2, 5, kTransmitter);
        allowTransmit(s.core);
        s.core.model->sliceById(1)->setDspMode(DSPMode::USB);
        s.core.model->sliceById(1)->setFrequency(14200000.0);
        const int bReceiver = streamOf(s.core, 1);
        QVERIFY(bReceiver >= 0 && bReceiver != s.receiver());
        QVERIFY(s.core.model->txSliceArbiter()->requestHandoff(1));
        MoxController* mox = s.core.model->moxController();
        mox->setMox(true, keyerFor(s.b));
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        mox->onMicPttFromRadio(true);
        QTRY_VERIFY(s.core.server->transmitHolder()->isHeldBy(
            QByteArray(KeyerIdentity::kStationDeviceId)));
        QTRY_VERIFY(mox->isMox() && mox->currentKeyer().isStation());
        QCOMPARE(s.core.model->txBoundSlice()->sliceIndex(), 1);

        const QJsonObject refused = s.core.invoke(
            s.appA, "addSliceOnPan", {utf8("panId", QStringLiteral("pan-a2"))});
        QVERIFY(!refused.value(QStringLiteral("accepted")).toBool(true));
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QJsonObject found;
        for (const QJsonValue& v : ask.value(QStringLiteral("choices")).toArray()) {
            if (v.toObject().value(QStringLiteral("streamIndex")).toInt() == bReceiver) {
                found = v.toObject();
            }
        }
        QVERIFY(!found.isEmpty());
        QCOMPARE(found.value(QStringLiteral("takeable")).toBool(true), false);
        QCOMPARE(found.value(QStringLiteral("why")).toString(), kRadioOnAir);
        s.core.invoke(s.appA, "confirm.cancel",
                      {int64("id", ask.value(QStringLiteral("id")).toInteger())});
        mox->onMicPttFromRadio(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    // Task 77 fix wave, M1 (ruling 5.4a): a take chooser's txSlice is the TX
    // mark, as on slice: and marker:: with transmit unheld no slice shows
    // it, the bound transmit slice included; once its owner holds transmit
    // it does.
    void aChoosersTxSliceIsTheTxMark()
    {
        Shared s(2, 5, kTransmitter);
        allowTransmit(s.core);
        s.core.model->sliceById(1)->setDspMode(DSPMode::USB);
        s.core.model->sliceById(1)->setFrequency(14200000.0);
        const int bReceiver = streamOf(s.core, 1);
        QVERIFY(bReceiver >= 0 && bReceiver != s.receiver());
        QVERIFY(s.core.model->txSliceArbiter()->requestHandoff(1));
        QVERIFY(s.core.model->sliceById(1)->isTxSlice());
        const auto txSliceShown = [&](int before) {
            const QJsonObject refused = s.core.invoke(
                s.appA, "addSliceOnPan", {utf8("panId", QStringLiteral("pan-a2"))});
            std::optional<bool> shown;
            if (refused.value(QStringLiteral("accepted")).toBool(true)) {
                return shown;
            }
            const QJsonObject ask =
                waitForLast(s.appA, QStringLiteral("confirm.request"), before);
            for (const QJsonValue& v : ask.value(QStringLiteral("choices")).toArray()) {
                for (const QJsonValue& slice :
                     v.toObject().value(QStringLiteral("slices")).toArray()) {
                    if (slice.toObject().value(QStringLiteral("sliceId")).toInt() == 1) {
                        shown = slice.toObject().value(QStringLiteral("txSlice")).toBool();
                    }
                }
            }
            s.core.invoke(s.appA, "confirm.cancel",
                          {int64("id", ask.value(QStringLiteral("id")).toInteger())});
            return shown;
        };
        QCOMPARE(txSliceShown(0), std::optional<bool>(false));
        QVERIFY(s.core.invoke(s.appB, "tx.take", {}).value(QStringLiteral("accepted")).toBool());
        QTRY_VERIFY(s.core.model->sliceById(1)->txSliceMarked());
        QCOMPARE(txSliceShown(countOf(s.appA, QStringLiteral("confirm.request"))),
                 std::optional<bool>(true));
    }

    // The transmitter's own Core settings (Receive Only, External TX
    // Inhibit) disturb the holder of transmit: another device's change is
    // asked while the holder is unkeyed, refused with the on-air words while
    // it transmits, and applied at once with transmit unheld.
    void transmitRegionProceedRechecksOnAir_data()
    {
        QTest::addColumn<bool>("remove");
        QTest::addColumn<bool>("onAir");
        QTest::newRow("write-off-air") << false << false;
        QTest::newRow("write-now-on-air") << false << true;
        QTest::newRow("remove-off-air") << true << false;
        QTest::newRow("remove-now-on-air") << true << true;
    }

    void transmitRegionProceedRechecksOnAir()
    {
        QFETCH(bool, remove);
        QFETCH(bool, onAir);
        Shared s(2, 5, kTransmitter);
        allowTransmit(s.core);
        const QString key = QStringLiteral("BandPlanRegion");
        s.core.settings->setValue(key, QStringLiteral("8"));
        TransmitHolder::KeyRequest take;
        take.deviceId = s.b.key.fingerprint();
        QCOMPARE(s.core.server->transmitHolder()->askKey(take).verdict, KeyingVerdict::Admit);
        s.appA->sendText(SessionMessages::encode(remove
            ? SessionMessages::settingsRemove(key)
            : SessionMessages::settingsWrite(key, QStringLiteral("3"), QStringLiteral("region"))));
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("forSettingsKey")).toString(), key);
        QCOMPARE(ask.value(QStringLiteral("change")).toObject().value(QStringLiteral("label")).toString(),
                 QStringLiteral("Transmit region"));
        QCOMPARE(s.core.settings->value(key).toString(), QStringLiteral("8"));
        // State-only simulation; no actual keying or RF.
        s.core.model->transmitModel().setMox(onAir);
        const QJsonObject answer = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(answer.value(QStringLiteral("accepted")).toBool(), !onAir);
        if (onAir) {
            QCOMPARE(answer.value(QStringLiteral("reason")).toString(), RadioModel::onAirReason());
            QCOMPARE(s.core.settings->value(key).toString(), QStringLiteral("8"));
        } else if (remove) {
            QVERIFY(!s.core.settings->contains(key));
        } else {
            QCOMPARE(s.core.settings->value(key).toString(), QStringLiteral("3"));
        }
        s.core.model->transmitModel().setMox(false);
    }

    // Addendum G-42 (JJ's ruling 2026-09-28): Extended transmit is one
    // Core setting. A device changes it only with transmit permission (the
    // station transmit gate: remote transmit allowed, an app that
    // transmits, nobody else holding transmit), never while the radio is
    // on the air, and only to True or False. Every device hears the Core's
    // value. No RF: MOX here is the transmit model's latch only.
    // Prevent TX'ing on a different band (PreventTxOnDifferentBandToRx,
    // 2026-09-29) is the same kind of Core setting.
    void transmitGateSettingNeedsTransmitPermissionAndWaitsForReceive_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QString>("onOrOff");
        QTest::newRow("extended") << QStringLiteral("ExtendedTransmit")
                                  << QStringLiteral("Extended transmit is either on or off.");
        QTest::newRow("prevent-different-band")
            << QStringLiteral("PreventTxOnDifferentBandToRx")
            << QStringLiteral("Prevent transmitting on a different band is either on or off.");
    }

    void transmitGateSettingNeedsTransmitPermissionAndWaitsForReceive()
    {
        QFETCH(QString, key);
        QFETCH(QString, onOrOff);
        Shared s(2, 5, kTransmitter);
        const auto refusedWith = [&](LoopbackTransport* app, const QString& value) {
            const int before = countOf(app, QStringLiteral("settings.reject"));
            app->sendText(SessionMessages::encode(
                value.isNull() ? SessionMessages::settingsRemove(key)
                               : SessionMessages::settingsWrite(key, value, QStringLiteral("ext"))));
            const QJsonObject reject = waitForLast(app, QStringLiteral("settings.reject"), before);
            return reject.value(QStringLiteral("key")).toString() == key
                ? reject.value(QStringLiteral("reason")).toString() : QString();
        };
        const auto heardValue = [&](const LoopbackTransport* app) {
            QString last;
            for (const QJsonObject& m : messagesOfType(app, QStringLiteral("settings.value"))) {
                if (m.value(QStringLiteral("key")).toString() == key) {
                    const QJsonArray props = m.value(QStringLiteral("properties")).toArray();
                    last = props.isEmpty() ? QStringLiteral("<absent>")
                        : props.first().toObject().value(QStringLiteral("value")).toString();
                }
            }
            return last;
        };

        // A receive-only Core (remote transmit denied) refuses it.
        QCOMPARE(refusedWith(s.appB, QStringLiteral("True")),
                 QStringLiteral("This Core is set to receive only."));
        QVERIFY(!s.core.settings->contains(key));

        allowTransmit(s.core);
        // An app that does not transmit is refused.
        QCOMPARE(refusedWith(s.appA, QStringLiteral("True")),
                 QStringLiteral("Update this app to transmit through this Core."));
        // Only on or off.
        QCOMPARE(refusedWith(s.appB, QStringLiteral("yes")), onOrOff);
        QVERIFY(!s.core.settings->contains(key));

        // A device with transmit permission, off the air: taken, and every
        // device hears it.
        s.appB->sendText(SessionMessages::encode(
            SessionMessages::settingsWrite(key, QStringLiteral("True"), QStringLiteral("ext-on"))));
        QTRY_COMPARE(s.core.settings->value(key).toString(), QStringLiteral("True"));
        QTRY_COMPARE(heardValue(s.appA), QStringLiteral("True"));
        QTRY_COMPARE(heardValue(s.appB), QStringLiteral("True"));

        // On the air: neither a change nor a removal.
        s.core.model->transmitModel().setMox(true);
        QCOMPARE(refusedWith(s.appB, QStringLiteral("False")), kRadioOnAir);
        QCOMPARE(refusedWith(s.appB, QString()), kRadioOnAir);
        QCOMPARE(s.core.settings->value(key).toString(), QStringLiteral("True"));
        s.core.model->transmitModel().setMox(false);

        // Another device holds transmit: this one may not change it.
        TransmitHolder* holder = s.core.server->transmitHolder();
        TransmitHolder::KeyRequest take;
        take.deviceId = s.a.key.fingerprint();
        QCOMPARE(holder->askKey(take).verdict, KeyingVerdict::Admit);
        const QString heldReason = refusedWith(s.appB, QStringLiteral("False"));
        QVERIFY(!heldReason.isEmpty());
        QVERIFY(heldReason != onOrOff);
        QCOMPARE(s.core.settings->value(key).toString(), QStringLiteral("True"));
        holder->release(s.a.key.fingerprint(), QStringLiteral("test"));

        // Removing it turns it off again.
        s.appB->sendText(SessionMessages::encode(SessionMessages::settingsRemove(key)));
        QTRY_VERIFY(!s.core.settings->contains(key));
        QTRY_COMPARE(heardValue(s.appA), QStringLiteral("<absent>"));
    }

    void theTransmittersCoreSettingsAskTheHolder()
    {
        Shared s(2, 5, kTransmitter);
        allowTransmit(s.core);
        TransmitHolder* holder = s.core.server->transmitHolder();
        const QString rxOnly = QStringLiteral("RxOnly");
        const QString inhibit = QStringLiteral("TxInhibitMonitorEnabled");

        // Unheld: applies at once.
        s.appA->sendText(SessionMessages::encode(
            SessionMessages::settingsWrite(inhibit, QStringLiteral("True"), QStringLiteral("a-0"))));
        QTRY_COMPARE(s.core.settings->value(inhibit).toString(), QStringLiteral("True"));
        s.core.settings->setValue(inhibit, QStringLiteral("False"));

        // B holds transmit, unkeyed: A's change is asked, naming B.
        TransmitHolder::KeyRequest take;
        take.deviceId = s.b.key.fingerprint();
        QCOMPARE(holder->askKey(take).verdict, KeyingVerdict::Admit);
        const QString before = s.core.settings->value(rxOnly).toString();
        s.appA->sendText(SessionMessages::encode(
            SessionMessages::settingsWrite(rxOnly, QStringLiteral("True"), QStringLiteral("a-1"))));
        const QJsonObject reject = waitForLast(s.appA, QStringLiteral("settings.reject"), 0);
        QCOMPARE(reject.value(QStringLiteral("key")).toString(), rxOnly);
        QCOMPARE(reject.value(QStringLiteral("reason")).toString(), kWaiting);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("forSettingsKey")).toString(), rxOnly);
        QCOMPARE(ask.value(QStringLiteral("change")).toObject().value(QStringLiteral("label")).toString(),
                 QStringLiteral("Receive Only"));
        const QJsonObject device = ask.value(QStringLiteral("affected")).toArray().first().toObject();
        QCOMPARE(device.value(QStringLiteral("deviceId")).toString(), s.b.id());
        QCOMPARE(device.value(QStringLiteral("holdsTransmit")).toBool(false), true);
        QCOMPARE(s.core.settings->value(rxOnly).toString(), before);
        s.core.invoke(s.appA, "confirm.cancel", {int64("id", ask.value(QStringLiteral("id")).toInteger())});

        // B on the air: refused with the on-air words, never asked.
        s.core.model->sliceById(1)->setDspMode(DSPMode::USB);
        s.core.model->sliceById(1)->setFrequency(14200000.0);
        QVERIFY(s.core.model->txSliceArbiter()->requestHandoff(1));
        MoxController* mox = s.core.model->moxController();
        mox->setMox(true, keyerFor(s.b));
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        const int rejects = countOf(s.appA, QStringLiteral("settings.reject"));
        const int asks = countOf(s.appA, QStringLiteral("confirm.request"));
        s.appA->sendText(SessionMessages::encode(
            SessionMessages::settingsWrite(inhibit, QStringLiteral("True"), QStringLiteral("a-2"))));
        const QJsonObject onAir = waitForLast(s.appA, QStringLiteral("settings.reject"), rejects);
        QCOMPARE(onAir.value(QStringLiteral("key")).toString(), inhibit);
        QCOMPARE(onAir.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("Tablet is on the air. Try again when they stop."));
        QTest::qWait(50);
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), asks);
        QCOMPARE(s.core.settings->value(inhibit).toString(), QStringLiteral("False"));
        mox->setMox(false, keyerFor(s.b));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    // Trunk merge of remote transmit (join d): the Alex tab's three
    // transmit high-pass switches (radioHardwareVersion 7) follow the TX
    // antennas' rule: the device holding transmit changes them on the air,
    // as Thetis sets them with no MOX check for the operator who is
    // transmitting, and another device's change waits. With remote transmit
    // allowed they are transmit settings, so the station transmit gate
    // answers another device first ("<holder> has the transmitter.").
    // (A receive-only Core, whose own key is the holder, holds a window's
    // change until the key ends: tst_remote_hl2_io.)
    void theHighPassSwitchesFollowTheTransmitAntennasRule()
    {
        Shared s(3, 5, kTransmitter);
        allowTransmit(s.core);
        // C, a device that may transmit and does not hold transmit.
        LoopbackTransport* appC = s.cOnTheOtherReceiver(kTransmitter);
        QVERIFY(admitted(appC));
        TransmitHolder* holder = s.core.server->transmitHolder();
        const QString key = QStringLiteral("hardware/AA:BB:CC:DD:EE:01/alex/master/hpfBypassOnTx");
        const QString hasTransmitter = QStringLiteral("iPad has the transmitter.");

        // Unheld: applies at once.
        appC->sendText(SessionMessages::encode(
            SessionMessages::settingsWrite(key, QStringLiteral("True"), QStringLiteral("c-0"))));
        QTRY_COMPARE(s.core.settings->value(key).toString(), QStringLiteral("True"));

        // B holds transmit: C's change waits.
        TransmitHolder::KeyRequest take;
        take.deviceId = s.b.key.fingerprint();
        QCOMPARE(holder->askKey(take).verdict, KeyingVerdict::Admit);
        appC->sendText(SessionMessages::encode(
            SessionMessages::settingsWrite(key, QStringLiteral("False"), QStringLiteral("c-1"))));
        QJsonObject reject = waitForLast(appC, QStringLiteral("settings.reject"), 0);
        QCOMPARE(reject.value(QStringLiteral("key")).toString(), key);
        QCOMPARE(reject.value(QStringLiteral("reason")).toString(), hasTransmitter);
        QCOMPARE(s.core.settings->value(key).toString(), QStringLiteral("True"));

        // B on the air: C's change still waits, B's own goes ahead.
        s.core.model->sliceById(1)->setDspMode(DSPMode::USB);
        s.core.model->sliceById(1)->setFrequency(14200000.0);
        QVERIFY(s.core.model->txSliceArbiter()->requestHandoff(1));
        MoxController* mox = s.core.model->moxController();
        mox->setMox(true, keyerFor(s.b));
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        const int rejects = countOf(appC, QStringLiteral("settings.reject"));
        appC->sendText(SessionMessages::encode(
            SessionMessages::settingsWrite(key, QStringLiteral("False"), QStringLiteral("c-2"))));
        reject = waitForLast(appC, QStringLiteral("settings.reject"), rejects);
        QCOMPARE(reject.value(QStringLiteral("key")).toString(), key);
        QVERIFY(!reject.value(QStringLiteral("reason")).toString().isEmpty());
        QCOMPARE(s.core.settings->value(key).toString(), QStringLiteral("True"));
        s.appB->sendText(SessionMessages::encode(
            SessionMessages::settingsWrite(key, QStringLiteral("False"), QStringLiteral("b-0"))));
        QTRY_COMPARE(s.core.settings->value(key).toString(), QStringLiteral("False"));
        QVERIFY(mox->isMox());
        mox->setMox(false, keyerFor(s.b));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    // Trunk merge of remote transmit (join c): with remote transmit allowed
    // the parity lane's OC transmit pins keep their on-air rule, by change
    // and not by holder: a pin waits while the radio is on the air, from
    // the device holding transmit too, as Thetis greys the pins while MOX
    // is on; an OC pin action is taken on the air.
    void theOcTransmitPinsWaitOnTheAirWhoeverHolds()
    {
        Shared s(2, 5, kTransmitter);
        allowTransmit(s.core);
        const QString pin = QStringLiteral("hardware/AA:BB:CC:DD:EE:01/oc/tx/40m/pin2");
        const QString action = QStringLiteral("hardware/AA:BB:CC:DD:EE:01/oc/actions/pin1/action");

        s.core.model->sliceById(1)->setDspMode(DSPMode::USB);
        s.core.model->sliceById(1)->setFrequency(14200000.0);
        QVERIFY(s.core.model->txSliceArbiter()->requestHandoff(1));
        MoxController* mox = s.core.model->moxController();
        mox->setMox(true, keyerFor(s.b));
        QTRY_COMPARE(mox->state(), MoxState::Tx);

        const int rejects = countOf(s.appB, QStringLiteral("settings.reject"));
        s.appB->sendText(SessionMessages::encode(
            SessionMessages::settingsWrite(pin, QStringLiteral("True"), QStringLiteral("b-0"))));
        const QJsonObject reject = waitForLast(s.appB, QStringLiteral("settings.reject"), rejects);
        QCOMPARE(reject.value(QStringLiteral("key")).toString(), pin);
        QCOMPARE(reject.value(QStringLiteral("reason")).toString(), kRadioOnAir);
        QVERIFY(!s.core.settings->contains(pin));
        s.appB->sendText(SessionMessages::encode(
            SessionMessages::settingsWrite(action, QStringLiteral("tune"), QStringLiteral("b-1"))));
        QTRY_COMPARE(s.core.settings->value(action).toString(), QStringLiteral("tune"));

        mox->setMox(false, keyerFor(s.b));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        s.appB->sendText(SessionMessages::encode(
            SessionMessages::settingsWrite(pin, QStringLiteral("True"), QStringLiteral("b-2"))));
        QTRY_COMPARE(s.core.settings->value(pin).toString(), QStringLiteral("True"));
    }

    // ---- Transmit group fix wave 2, Important 3 (ruling 8.11) ----------

    // While the radio's own PTT keys on B's slice (inside A's receiver),
    // that slice is frozen on every path: B moving its pan (ruling 6.6)
    // would carry it, A moving the receiver (the anchor, ruling 6.4) would
    // move it, and its XIT retunes what is transmitted. The press ends and
    // each goes ahead again.
    void theRadiosPttFreezesTheSliceOnEveryPath()
    {
        Shared s;
        preparePtt(s.core);
        s.core.model->sliceById(1)->setDspMode(DSPMode::LSB);
        QVERIFY(s.core.model->txSliceArbiter()->requestHandoff(1));
        const int receiver = s.receiver();
        QCOMPARE(streamOf(s.core, 1), receiver);
        const double centre = s.core.model->streamAllocator().streamCentreHz(receiver);
        MoxController* mox = s.core.model->moxController();
        mox->onMicPttFromRadio(true);
        QTRY_COMPARE(mox->state(), MoxState::Tx);

        // B, which does not anchor, moving its pan to a free receiver.
        QJsonObject r = s.core.invoke(s.appB, "requestStreamCentre",
                                      {int64("sliceId", 1), f64("centreHz", 7120000.0)});
        QCOMPARE(r.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(r.value(QStringLiteral("reason")).toString(), kRadioOnAir);
        QCOMPARE(streamOf(s.core, 1), receiver);

        // A, the anchor, moving the receiver so B's slice would leave it:
        // refused, nobody asked.
        const int asks = countOf(s.appA, QStringLiteral("confirm.request"));
        r = s.core.invoke(s.appA, "requestStreamCentre",
                          {int64("sliceId", 0), f64("centreHz", 6990000.0)});
        QCOMPARE(r.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(r.value(QStringLiteral("reason")).toString(), kRadioOnAir);
        QTest::qWait(50);
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), asks);
        QCOMPARE(s.core.model->streamAllocator().streamCentreHz(receiver), centre);

        // B's XIT on the frozen slice.
        quint32 writeId = 4100;
        for (const MirrorUpdate& update :
             {MirrorUpdate{0, "xitEnabled", MirrorWireKind::Bool, QVariant(true)},
              MirrorUpdate{0, "xitHz", MirrorWireKind::Int64, QVariant(qlonglong(250))}}) {
            const quint32 id = ++writeId;
            s.appB->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
                ObjectRegistry::keyForSlice(1), {update}, id)));
            QTRY_VERIFY(!propertyResult(s.appB, id).isEmpty());
            const QJsonObject result = firstResultEntry(propertyResult(s.appB, id));
            QCOMPARE(result.value(QStringLiteral("accepted")).toBool(true), false);
            QCOMPARE(result.value(QStringLiteral("reason")).toString(), kRadioOnAir);
        }
        QVERIFY(!s.core.model->sliceById(1)->xitEnabled());
        QCOMPARE(s.core.model->sliceById(1)->xitHz(), 0);

        mox->onMicPttFromRadio(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        // Task 77 (ruling 8.1): the radio keeps transmit, unkeyed; the
        // freeze ends with the press.
        QTRY_VERIFY(s.core.server->transmitHolder()->holder().has_value()
                    && !s.core.server->transmitHolder()->holder()->keyed);
        r = s.core.invoke(s.appB, "requestStreamCentre",
                          {int64("sliceId", 1), f64("centreHz", 7120000.0)});
        QVERIFY2(r.value(QStringLiteral("accepted")).toBool(false),
                 qPrintable(r.value(QStringLiteral("reason")).toString()));
        QVERIFY(streamOf(s.core, 1) != receiver);
    }

    // A change asked before the press and confirmed during it asks the
    // freeze again: B's retune of its slice, stored behind the receiver
    // chooser, is refused on proceed while the radio's PTT keys on that
    // slice, and nothing of it applies (C keeps its receiver and slice).
    void aStoredChangeConfirmedDuringTheRadiosPttWaits()
    {
        Shared s;
        preparePtt(s.core);
        LoopbackTransport* appC = s.cOnTheOtherReceiver();
        QVERIFY(admitted(appC));
        const QList<int> cSlices = s.core.model->sliceOwnership()->ownedBy(s.c.key.fingerprint());
        QCOMPARE(cSlices.size(), 1);
        s.core.model->sliceById(1)->setDspMode(DSPMode::LSB);
        QVERIFY(s.core.model->txSliceArbiter()->requestHandoff(1));

        const QJsonObject refused = writeFrequency(s.appB, 1, 7250000.0, 4200);
        QCOMPARE(firstResultEntry(refused).value(QStringLiteral("accepted")).toBool(true), false);
        const QJsonObject ask = waitForLast(s.appB, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("takeReceiver"));
        const int cReceiver = streamOf(s.core, cSlices.first());
        int choice = -1;
        for (const QJsonValue& v : ask.value(QStringLiteral("choices")).toArray()) {
            if (v.toObject().value(QStringLiteral("streamIndex")).toInt() == cReceiver) {
                QCOMPARE(v.toObject().value(QStringLiteral("takeable")).toBool(false), true);
                choice = v.toObject().value(QStringLiteral("choice")).toInt();
            }
        }
        QVERIFY(choice >= 0);

        MoxController* mox = s.core.model->moxController();
        mox->onMicPttFromRadio(true);
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        const QJsonObject done = s.proceed(s.appB, ask.value(QStringLiteral("id")).toInteger(), choice);
        QCOMPARE(done.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(done.value(QStringLiteral("reason")).toString(), kRadioOnAir);
        QCOMPARE(s.core.model->sliceById(1)->frequency(), 7150000.0);
        QVERIFY(s.core.model->sliceById(cSlices.first()) != nullptr);
        QCOMPARE(s.core.model->sliceOwnership()->mark(cSlices.first()).owner, s.c.key.fingerprint());
        mox->onMicPttFromRadio(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    // Take-over fix wave (M-2): forgetTakeBacks drops only the records of
    // the one device that the predicate picks.
    void forgetTakeBacksDropsOnlyThePickedRecordsOfOneDevice()
    {
        const auto record = [](const QByteArray& device, qint64 id, int sliceId) {
            ConfirmStep::Notice notice;
            notice.id = id;
            notice.device = device;
            notice.prompt.kind = QStringLiteral("controlTaken");
            notice.prompt.slices = QJsonArray{QJsonObject{{QStringLiteral("sliceId"), sliceId}}};
            return notice;
        };
        const auto onSlice = [](int sliceId) {
            return [sliceId](const ConfirmStep::Notice& kept) {
                return kept.prompt.slices->first().toObject().value(QStringLiteral("sliceId"))
                           .toInt(-1)
                    == sliceId;
            };
        };
        ConfirmStep step;
        step.keepTakeBack(record("x", 1, 0));
        step.keepTakeBack(record("x", 2, 1));
        step.keepTakeBack(record("y", 3, 0));

        step.forgetTakeBacks("x", onSlice(0));
        QVERIFY(!step.takeBackRecord("x", 1).has_value());
        QVERIFY(step.takeBackRecord("x", 2).has_value());
        QVERIFY(step.takeBackRecord("y", 3).has_value());

        // No predicate, or a device with none: nothing goes.
        step.forgetTakeBacks("x", {});
        step.forgetTakeBacks("z", onSlice(1));
        QVERIFY(step.takeBackRecord("x", 2).has_value());
        QVERIFY(step.takeBackRecord("y", 3).has_value());

        step.forgetTakeBacks("x", onSlice(1));
        QVERIFY(!step.takeBackRecord("x", 2).has_value());
        QVERIFY(step.takeBackRecord("y", 3).has_value());
    }
};

QTEST_GUILESS_MAIN(TstConfirmStep)
#include "tst_confirm_step.moc"
