// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_slice_access_verbs.cpp  (NereusSDR)
// =================================================================
//
// Slice control and shared listening plan Task 4: the sliceAccess feature
// and its two-key gate, the SliceAccess objects (`access:<id>`), the slice
// and marker forms a sharing device receives, and slice.listen,
// slice.stopListening, slice.takeControl and slice.release against a real
// Core over loopback links. Devices A, B and C share slices; D is an older
// app (sessionHolder only). No radio: the transmit cases key the fake MOX,
// never hardware.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), slice control and shared listening plan Task 4,
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: slice.setListenLevel and the Q4 seeding, slice control
//               plan Task 6. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-29: the keying refusal on a slice taken and not chosen, and
//               receive selection leaving transmit alone, slice control
//               plan Task 11. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-29: slice control fix wave: a keyed slice is never closed by
//               stopListening or release (deferred or refused), the
//               station freeze covers both verbs, and the local transmit
//               hand-off obeys the same access rule as the remote verb.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: take-over parity: Take it back on controlTaken
//               (sliceAccessVersion 2), transmit left where it was, and an
//               older peer offered none. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: take-over fix wave: a key never lands on the slice a
//               device lost (I-2), nor another device's slice for a keyer
//               that shares slices (N-1); an older peer's controlTaken entry
//               (M-1); exact refusal words and a take-back of a slice made
//               again (M-2). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: TX rulings (item 3): the Core refuses a listener's
//               attenuator and preamp writes while its shown slice is one
//               it listens to. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: core-slice take-over: a peer that declared sliceAccess 3
//               reads 3. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: desktop listening lane: Take it back of a slice taken
//               again or released since the notice answers "That can no
//               longer be taken back." J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: JJ's wider ruling: control passes from a session that
//               cannot stay listening (it loses the slice; an older
//               window left with none ends). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: take-over review: listening to an older window's slice
//               and its own close releasing it to a listener (ruling Q6)
//               are back in their own tests. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include "core/session/SliceAccessController.h"
#include "core/session/SliceAccessPolicy.h"
#include "core/StepAttenuatorController.h"
#include "core/StepAttenuatorFacade.h"

#include <QSignalSpy>

#include <limits>

namespace {

// A device that shares slices, and one that also transmits.
const QHash<QByteArray, int> kShares{{"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 1}};
const QHash<QByteArray, int> kSharesTx{
    {"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 1}, {"remoteTx", 1}};
// Take-over parity: a device at sliceAccess 2 (Take it back on
// controlTaken), and one that also transmits.
const QHash<QByteArray, int> kSharesBack{
    {"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 2}};
const QHash<QByteArray, int> kSharesBackTx{
    {"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 2}, {"remoteTx", 1}};

QString accessKey(int sliceId)
{
    return QStringLiteral("access:%1").arg(sliceId);
}

QJsonValue accessOf(const LoopbackTransport* app, int sliceId, const char* property)
{
    return latest(app->received(), accessKey(sliceId), QString::fromLatin1(property));
}

QStringList idList(const QJsonValue& json)
{
    QStringList ids;
    for (const QJsonValue& v : QJsonDocument::fromJson(json.toString().toUtf8()).array()) {
        ids.append(v.toString());
    }
    return ids;
}

qint64 resultValue(const QJsonObject& result, const QString& name)
{
    for (const QJsonValue& v : result.value(QStringLiteral("values")).toArray()) {
        if (v.toObject().value(QStringLiteral("name")).toString() == name) {
            return v.toObject().value(QStringLiteral("value")).toInteger();
        }
    }
    return -1;
}

bool accepted(const QJsonObject& result)
{
    return result.value(QStringLiteral("accepted")).toBool(false);
}

QString reasonOf(const QJsonObject& result)
{
    return result.value(QStringLiteral("reason")).toString();
}

// The index in `app`'s wire of the first message of `type` on `key` from
// `from` on, or -1.
int indexOf(const LoopbackTransport* app, int from, const QString& type, const QString& key)
{
    for (int i = std::max(0, from); i < app->received().size(); ++i) {
        const QJsonObject o = QJsonDocument::fromJson(app->received().at(i)).object();
        if (o.value(QStringLiteral("type")).toString() == type
            && o.value(QStringLiteral("key")).toString() == key) {
            return i;
        }
    }
    return -1;
}

QString listenerWords(const QString& letter, const QString& controller)
{
    return QStringLiteral("Slice %1 is controlled by %2. Take control to change it.")
        .arg(letter, controller);
}

// Every active receiver stream, as a bit per stream.
quint32 activeStreamMask(const RadioModel& model)
{
    quint32 mask = 0;
    const SliceStreamAllocator& allocator = model.streamAllocator();
    for (int i = 0; i < allocator.streamCount() && i < 32; ++i) {
        if (allocator.isStreamActive(i)) {
            mask |= 1u << i;
        }
    }
    return mask;
}

// The slice's reference and revision as `app` last read them.
struct Seen {
    qint64 sliceId = -1;
    qint64 incarnation = 0;
    qint64 revision = 0;
};

Seen seenBy(const LoopbackTransport* app, int sliceId)
{
    return Seen{sliceId, accessOf(app, sliceId, "incarnation").toInteger(),
                accessOf(app, sliceId, "controlRevision").toInteger()};
}

QList<MirrorUpdate> refArgs(const Seen& seen)
{
    return {int64("sliceId", seen.sliceId), int64("incarnation", seen.incarnation)};
}

QList<MirrorUpdate> levelArgs(const Seen& seen, double level, bool muted)
{
    return {int64("sliceId", seen.sliceId), int64("incarnation", seen.incarnation),
            f64("level", level),
            MirrorUpdate{0, QByteArrayLiteral("muted"), MirrorWireKind::Bool, QVariant(muted)}};
}

QList<MirrorUpdate> revisionArgs(const Seen& seen)
{
    return {int64("sliceId", seen.sliceId), int64("incarnation", seen.incarnation),
            int64("controlRevision", seen.revision)};
}

} // namespace

class TstSliceAccessVerbs : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        const QString profile =
            QStringLiteral("slice-access-verbs-%1").arg(QCoreApplication::applicationPid());
        AppSettings::setProfileOverride(profile);
        AppSettings::instance().clear();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // ── Negotiation ──────────────────────────────────────────────────────

    void onlyADeviceThatDeclaresSliceAccessIsOfferedIt()
    {
        Core core;
        Device a;
        Device d(QStringLiteral("Mac"), QStringLiteral("computer"));
        core.pair(a);
        core.pair(d);
        LoopbackTransport* appA = core.signIn(a, kShares);
        LoopbackTransport* appD = core.signIn(d, kHolder);
        QVERIFY(admitted(appA));
        QVERIFY(admitted(appD));
        QCOMPARE(capability(appA->received(), QStringLiteral("sliceAccessVersion")), 1);
        QVERIFY(!capability(appD->received(), QStringLiteral("sliceAccessVersion")).has_value());
        // The capability is the last before coreBuildInfo.
        const QJsonArray caps =
            firstOfType(appA->received(), QStringLiteral("capabilities"))
                .value(QStringLiteral("properties")).toArray();
        QString last;
        for (const QJsonValue& p : caps) {
            const QString name = p.toObject().value(QStringLiteral("name")).toString();
            if (name != QStringLiteral("coreBuildInfo")) {
                last = name;
            }
        }
        QCOMPARE(last, QStringLiteral("sliceAccessVersion"));

        // A: the class and an object per slice. D: neither.
        QTRY_VERIFY(holds(appA, accessKey(0)));
        QTRY_VERIFY(holds(appA, accessKey(1)));
        QCOMPARE(accessOf(appA, 0, "sliceId").toInt(), 0);
        QCOMPARE(static_cast<quint64>(accessOf(appA, 0, "incarnation").toInteger()),
                 core.model->sliceOwnership()->incarnation(0));
        QCOMPARE(accessOf(appA, 0, "controllerDeviceId").toString(), a.id());
        QCOMPARE(accessOf(appA, 0, "controlRevision").toInteger(),
                 static_cast<qint64>(core.model->sliceOwnership()->controlRevision(0)));
        QCOMPARE(idList(accessOf(appA, 0, "listenerDeviceIds")), QStringList{a.id()});
        QCOMPARE(idList(accessOf(appA, 0, "activeRxDeviceIds")), QStringList{a.id()});
        QCOMPARE(accessOf(appA, 1, "controllerDeviceId").toString(), d.id());
        for (const QByteArray& wire : appD->received()) {
            const QJsonObject o = QJsonDocument::fromJson(wire).object();
            QVERIFY(!o.value(QStringLiteral("key")).toString().startsWith(QStringLiteral("access:")));
            QVERIFY(o.value(QStringLiteral("class")).toString() != QStringLiteral("SliceAccess"));
        }
        // D keeps today's forms: its own slice, a marker for A's.
        QCOMPARE(heldKeys(appD, QStringLiteral("slice:")), QStringList{QStringLiteral("slice:1")});
        QCOMPARE(heldKeys(appD, QStringLiteral("marker:")), QStringList{QStringLiteral("marker:0")});

        // The verbs are refused to D by the gate, and change nothing.
        const QString update =
            QStringLiteral("Update this app to listen to and take slices on this Core.");
        QVERIFY(OperatorWording::isPlain(update));
        const Seen seen = seenBy(appA, 0);
        for (const QByteArray verb :
             {QByteArrayLiteral("slice.listen"), QByteArrayLiteral("slice.stopListening")}) {
            const QJsonObject r = core.invoke(appD, verb, refArgs(seen));
            QVERIFY(!accepted(r));
            QCOMPARE(reasonOf(r), update);
        }
        for (const QByteArray verb :
             {QByteArrayLiteral("slice.takeControl"), QByteArrayLiteral("slice.release")}) {
            const QJsonObject r = core.invoke(appD, verb, revisionArgs(seen));
            QVERIFY(!accepted(r));
            QCOMPARE(reasonOf(r), update);
        }
        QCOMPARE(core.model->sliceOwnership()->listenersOf(0),
                 QList<QByteArray>{a.key.fingerprint()});
        QCOMPARE(core.model->sliceOwnership()->mark(0).owner, a.key.fingerprint());
    }

    // ── Listen in ────────────────────────────────────────────────────────

    void listeningAtFullCapacityAllocatesNothingAndSwapsTheForm()
    {
        Core core;
        core.model->configureStreamPool(2, 5, 192000);
        core.model->sliceById(0)->setFrequency(14200000.0);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kShares);
        QVERIFY(admitted(appA));
        // A fills the slice cap, on both receivers.
        for (int i = 0; i < 8; ++i) {
            if (!accepted(core.invoke(appA, "addSlice", {utf8("initialPanId", QString())}))) {
                break;
            }
            if (i == 0) {
                core.model->sliceById(1)->setFrequency(7074000.0);
            }
        }
        const int full = core.model->slices().size();
        QVERIFY(full > 2);
        QCOMPARE(core.model->streamAllocator().activeStreamCount(),
                 core.model->streamAllocator().streamCount());
        QVERIFY(!accepted(core.invoke(appA, "addSlice", {utf8("initialPanId", QString())})));
        const quint32 streams = activeStreamMask(*core.model);

        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appB));
        QCOMPARE(core.model->slices().size(), full);
        QTRY_VERIFY(holds(appB, QStringLiteral("marker:0")));
        QVERIFY(!holds(appB, QStringLiteral("slice:0")));
        QTRY_VERIFY(holds(appB, accessKey(0)));

        const int from = appB->received().size();
        const Seen seen = seenBy(appB, 0);
        QVERIFY(seen.incarnation > 0);
        const QJsonObject r = core.invoke(appB, "slice.listen", refArgs(seen));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        QCOMPARE(resultValue(r, QStringLiteral("controlRevision")), seen.revision);
        // Nothing allocated.
        QCOMPARE(core.model->slices().size(), full);
        QCOMPARE(core.model->streamAllocator().activeStreamCount(),
                 core.model->streamAllocator().streamCount());
        QCOMPARE(activeStreamMask(*core.model), streams);
        // B's view: the marker goes, then the slice comes.
        QTRY_VERIFY(holds(appB, QStringLiteral("slice:0")));
        const int destroyed = indexOf(appB, from, QStringLiteral("object.destroy"),
                                      QStringLiteral("marker:0"));
        const int created = indexOf(appB, from, QStringLiteral("object.create"),
                                    QStringLiteral("slice:0"));
        QVERIFY(destroyed >= 0);
        QVERIFY(destroyed < created);
        QVERIFY(!holds(appB, QStringLiteral("marker:0")));
        const QStringList both{a.id(), b.id()};
        QTRY_COMPARE(idList(accessOf(appB, 0, "listenerDeviceIds")), both);
        QTRY_COMPARE(idList(accessOf(appA, 0, "listenerDeviceIds")), both);
        // A's own form is unchanged.
        QVERIFY(holds(appA, QStringLiteral("slice:0")));
        QVERIFY(!everSaw(appA, QStringLiteral("marker:0")));

        // Listening again is accepted and changes nothing.
        const int again = appB->received().size();
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refArgs(seen))));
        QTest::qWait(3 * StationServer::kDefaultDeltaFlushMs);
        QCOMPARE(indexOf(appB, again, QStringLiteral("object.create"), QStringLiteral("slice:0")),
                 -1);
        QCOMPARE(core.model->sliceOwnership()->listenersOf(0).size(), 2);
    }

    void aListenerIsRefusedAChangeAndHearsTheControllers()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kShares);
        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appA) && admitted(appB));
        QTRY_VERIFY(holds(appB, accessKey(0)));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refArgs(seenBy(appB, 0)))));
        QTRY_VERIFY(holds(appB, QStringLiteral("slice:0")));
        SliceModel* slice = core.model->sliceById(0);
        const double frequency = slice->frequency();

        const QString words = listenerWords(QStringLiteral("A"), QStringLiteral("iPhone"));
        QVERIFY(OperatorWording::isPlain(words));
        appB->sendText(SessionMessages::encode(
            SessionMessages::propertyWrite("slice:0", {f64("frequency", 7074000.0)}, 601)));
        QTRY_VERIFY(!propertyResult(appB, 601).isEmpty());
        const QJsonObject refused =
            propertyResult(appB, 601).value(QStringLiteral("results")).toArray().first().toObject();
        QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(refused.value(QStringLiteral("reason")).toString(), words);
        QCOMPARE(slice->frequency(), frequency);
        // A verb that names the slice is refused in the same words.
        const QJsonObject verb = core.invoke(appB, "nnr.resetTuning", {int64("sliceId", 0)});
        QVERIFY(!accepted(verb));
        QCOMPARE(reasonOf(verb), words);

        // A's change is applied, and B receives it.
        const int from = appB->received().size();
        appA->sendText(SessionMessages::encode(
            SessionMessages::propertyWrite("slice:0", {f64("frequency", 14100000.0)}, 602)));
        QTRY_COMPARE(slice->frequency(), 14100000.0);
        QVERIFY(QTest::qWaitFor(
            [&]() {
                const QList<QJsonValue> v =
                    deltaValues(appB->received(), from, QStringLiteral("slice:0"),
                                QStringLiteral("frequency"));
                return !v.isEmpty() && v.last().toDouble() == 14100000.0;
            },
            5000));

        // B makes it its active receive slice; its own and the Core's
        // active slice stay where they were.
        const int stationSlice = core.model->sliceOwnership()->stationActiveSlice();
        const int ownActive = core.model->sliceOwnership()->activeFor(b.key.fingerprint());
        const QJsonObject select = core.invoke(appB, "setActiveSliceById", {int64("sliceId", 0)});
        QVERIFY2(accepted(select), qPrintable(reasonOf(select)));
        QCOMPARE(core.model->sliceOwnership()->activeRxFor(b.key.fingerprint()), 0);
        QCOMPARE(core.model->sliceOwnership()->activeFor(b.key.fingerprint()), ownActive);
        QCOMPARE(core.model->sliceOwnership()->stationActiveSlice(), stationSlice);
        QTRY_COMPARE(idList(accessOf(appB, 0, "activeRxDeviceIds")),
                     (QStringList{a.id(), b.id()}));
    }

    // TX rulings (item 3, JJ): the attenuator and preamp act on the slice a
    // device is shown. While that is a slice it only listens to, the Core
    // refuses its writes in the listener's words; on its own slice they
    // apply, and the controller's always do.
    void aListenerIsRefusedTheAttenuatorAndPreamp()
    {
        Core core;
        StepAttenuatorController stepAtt;
        stepAtt.setTickTimerEnabled(false);
        core.model->setStepAttController(&stepAtt);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kShares);
        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appA) && admitted(appB));
        QTRY_VERIFY(holds(appB, accessKey(0)));
        QTRY_VERIFY(holds(appB, QStringLiteral("stepAtt")));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refArgs(seenBy(appB, 0)))));
        QTRY_VERIFY(holds(appB, QStringLiteral("slice:0")));
        const QByteArray bId = b.key.fingerprint();
        const int bOwn = core.model->sliceOwnership()->activeFor(bId);
        QVERIFY(bOwn >= 0 && bOwn != 0);
        QVERIFY(accepted(core.invoke(appB, "setActiveSliceById", {int64("sliceId", 0)})));
        QCOMPARE(core.model->sliceOwnership()->activeRxFor(bId), 0);

        StepAttenuatorFacade* facade = core.model->stepAttFacade();
        const int dB = facade->attenuationDb();
        const int mode = facade->preampMode();
        const QString words = listenerWords(QStringLiteral("A"), QStringLiteral("iPhone"));
        const auto flag = [](const char* name, bool on) {
            return MirrorUpdate{0, QByteArray(name), MirrorWireKind::Bool, QVariant(on)};
        };
        // Every receive-level setting, both ADCs (review I-2).
        const bool enabled = facade->enabled();
        const bool autoOn = facade->autoAttEnabled();
        const QList<MirrorUpdate> writes{
            flag("enabled", !enabled), int64("attenuationDb", dB + 5),
            int64("preampMode", mode == 0 ? 1 : 0), flag("rx1Preamp", true),
            flag("autoAttEnabled", !autoOn), int64("autoAttMode", 1),
            flag("autoAttUndo", true), int64("autoAttUndoDelayMs", 3000),
            int64("autoAttHoldMs", 4000), flag("rx2StepAttEnabled", true),
            int64("rx2AttenuationDb", 7), int64("rx2PreampMode", 1),
            flag("rx2AutoAttEnabled", true), flag("rx2AutoAttUndo", true),
            int64("rx2AutoAttUndoDelayMs", 3000)};
        appB->sendText(SessionMessages::encode(
            SessionMessages::propertyWrite("stepAtt", writes, 701)));
        QTRY_VERIFY(!propertyResult(appB, 701).isEmpty());
        const QJsonArray refused =
            propertyResult(appB, 701).value(QStringLiteral("results")).toArray();
        QCOMPARE(refused.size(), writes.size());
        for (const QJsonValue& r : refused) {
            const QString what = r.toObject().value(QStringLiteral("name")).toString();
            QVERIFY2(!r.toObject().value(QStringLiteral("accepted")).toBool(true),
                     qPrintable(what));
            QCOMPARE(r.toObject().value(QStringLiteral("reason")).toString(), words);
        }
        QCOMPARE(facade->attenuationDb(), dB);
        QCOMPARE(facade->preampMode(), mode);
        QCOMPARE(facade->enabled(), enabled);
        QCOMPARE(facade->autoAttEnabled(), autoOn);

        // The transmit settings are not the slice's: ATT on TX applies.
        appB->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "stepAtt", {flag("attOnTxEnabled", !facade->attOnTxEnabled())}, 704)));
        QTRY_VERIFY(!propertyResult(appB, 704).isEmpty());
        const QJsonObject tx = propertyResult(appB, 704)
            .value(QStringLiteral("results")).toArray().first().toObject();
        QVERIFY(tx.value(QStringLiteral("reason")).toString() != words);
        QVERIFY2(tx.value(QStringLiteral("accepted")).toBool(false),
                 qPrintable(tx.value(QStringLiteral("reason")).toString()));

        // The controller of slice A changes it.
        appA->sendText(SessionMessages::encode(
            SessionMessages::propertyWrite("stepAtt", {int64("attenuationDb", dB + 5)}, 702)));
        QTRY_COMPARE(facade->attenuationDb(), dB + 5);

        // Shown its own slice again, the listener changes it too.
        QVERIFY(accepted(core.invoke(appB, "setActiveSliceById", {int64("sliceId", bOwn)})));
        QCOMPARE(core.model->sliceOwnership()->activeRxFor(bId), bOwn);
        appB->sendText(SessionMessages::encode(
            SessionMessages::propertyWrite("stepAtt", {int64("attenuationDb", dB + 7)}, 703)));
        QTRY_COMPARE(facade->attenuationDb(), dB + 7);
    }

    // ── Stop listening ───────────────────────────────────────────────────

    void stoppingListeningSwapsBackAndTheControllerIsSentToRelease()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kShares);
        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appA) && admitted(appB));
        QTRY_VERIFY(holds(appB, accessKey(0)));
        const Seen seen = seenBy(appB, 0);
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refArgs(seen))));
        QVERIFY(accepted(core.invoke(appB, "setActiveSliceById", {int64("sliceId", 0)})));
        QCOMPARE(core.model->sliceOwnership()->activeRxFor(b.key.fingerprint()), 0);

        // The controller cannot stop listening (ruling Q5).
        const QJsonObject refused = core.invoke(appA, "slice.stopListening", refArgs(seen));
        QVERIFY(!accepted(refused));
        const QString release = QStringLiteral("You control slice A. Use Release to leave it.");
        QVERIFY(OperatorWording::isPlain(release));
        QCOMPARE(reasonOf(refused), release);

        const int from = appB->received().size();
        QVERIFY(accepted(core.invoke(appB, "slice.stopListening", refArgs(seen))));
        QVERIFY(!core.model->sliceOwnership()->isListening(b.key.fingerprint(), 0));
        // Its receive choice moves to its next joined slice, its own.
        QCOMPARE(core.model->sliceOwnership()->activeRxFor(b.key.fingerprint()), 1);
        QTRY_VERIFY(holds(appB, QStringLiteral("marker:0")));
        QVERIFY(!holds(appB, QStringLiteral("slice:0")));
        QVERIFY(indexOf(appB, from, QStringLiteral("object.destroy"), QStringLiteral("slice:0"))
                < indexOf(appB, from, QStringLiteral("object.create"), QStringLiteral("marker:0")));
        // The slice stays, its controller's.
        QVERIFY(core.model->sliceById(0) != nullptr);
        QCOMPARE(core.model->sliceOwnership()->mark(0).owner, a.key.fingerprint());
        // Not joined any more: accepted, nothing to leave.
        QVERIFY(accepted(core.invoke(appB, "slice.stopListening", refArgs(seen))));
    }

    // ── Take control ─────────────────────────────────────────────────────

    void takingControlKeepsTheSliceAndTellsTheFormerController()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kShares);
        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appA) && admitted(appB));
        QTRY_VERIFY(holds(appB, accessKey(0)));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refArgs(seenBy(appB, 0)))));
        QTRY_VERIFY(holds(appB, QStringLiteral("slice:0")));

        SliceModel* slice = core.model->sliceById(0);
        const QPointer<SliceModel> same(slice);
        const double frequency = slice->frequency();
        const DSPMode mode = slice->dspMode();
        const int filterLow = slice->filterLow();
        const int filterHigh = slice->filterHigh();
        const quint64 incarnation = core.model->sliceOwnership()->incarnation(0);
        RxChannel* const channel = core.model->rxChannelForSlice(0);
        QSignalSpy added(core.model.get(), &RadioModel::sliceAdded);
        QSignalSpy removed(core.model.get(), &RadioModel::sliceRemoved);
        const int fromA = appA->received().size();
        const int fromB = appB->received().size();

        const Seen seen = seenBy(appB, 0);
        const QJsonObject r = core.invoke(appB, "slice.takeControl", revisionArgs(seen));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        QCOMPARE(resultValue(r, QStringLiteral("controlRevision")), seen.revision + 1);
        const SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownership->mark(0).owner, b.key.fingerprint());
        QCOMPARE(ownership->listenersOf(0),
                 (QList<QByteArray>{b.key.fingerprint(), a.key.fingerprint()}));
        // The same slice, untouched.
        QCOMPARE(core.model->sliceById(0), same.data());
        QCOMPARE(ownership->incarnation(0), incarnation);
        QCOMPARE(slice->sliceLetter(), QChar(QLatin1Char('A')));
        QCOMPARE(slice->frequency(), frequency);
        QCOMPARE(slice->dspMode(), mode);
        QCOMPARE(slice->filterLow(), filterLow);
        QCOMPARE(slice->filterHigh(), filterHigh);
        QCOMPARE(core.model->rxChannelForSlice(0), channel);
        QCOMPARE(added.count(), 0);
        QCOMPARE(removed.count(), 0);

        // Nobody's form changes: both already had slice:0.
        QTest::qWait(3 * StationServer::kDefaultDeltaFlushMs);
        QVERIFY(holds(appA, QStringLiteral("slice:0")));
        QVERIFY(holds(appB, QStringLiteral("slice:0")));
        QCOMPARE(indexOf(appA, fromA, QStringLiteral("object.destroy"), QStringLiteral("slice:0")),
                 -1);
        QCOMPARE(indexOf(appB, fromB, QStringLiteral("object.create"), QStringLiteral("slice:0")),
                 -1);
        QTRY_COMPARE(accessOf(appA, 0, "controllerDeviceId").toString(), b.id());
        QTRY_COMPARE(idList(accessOf(appA, 0, "listenerDeviceIds")), (QStringList{b.id(), a.id()}));

        // A is told.
        QTRY_VERIFY(!firstOfType(appA->received(), QStringLiteral("notice")).isEmpty());
        const QJsonObject notice = firstOfType(appA->received(), QStringLiteral("notice"));
        QCOMPARE(notice.value(QStringLiteral("kind")).toString(), QStringLiteral("controlTaken"));
        const QString told = QStringLiteral("iPad took control of slice A. You are still listening.");
        QVERIFY(OperatorWording::isPlain(told));
        QCOMPARE(notice.value(QStringLiteral("reason")).toString(), told);
        QCOMPARE(notice.value(QStringLiteral("takeBack")).toBool(true), false);
        QCOMPARE(notice.value(QStringLiteral("byDeviceId")).toString(), b.id());
        QCOMPARE(notice.value(QStringLiteral("byName")).toString(), QStringLiteral("iPad"));
        const QJsonObject entry =
            notice.value(QStringLiteral("slices")).toArray().first().toObject();
        QCOMPARE(entry.value(QStringLiteral("sliceId")).toInt(), 0);
        QCOMPARE(entry.value(QStringLiteral("letter")).toString(), QStringLiteral("A"));
        QCOMPARE(entry.value(QStringLiteral("frequencyHz")).toDouble(), frequency);
        QVERIFY(firstOfType(appB->received(), QStringLiteral("notice")).isEmpty());

        // A's next change is refused, in the listener's words.
        appA->sendText(SessionMessages::encode(
            SessionMessages::propertyWrite("slice:0", {f64("frequency", 7074000.0)}, 701)));
        QTRY_VERIFY(!propertyResult(appA, 701).isEmpty());
        QCOMPARE(propertyResult(appA, 701).value(QStringLiteral("results")).toArray().first()
                     .toObject().value(QStringLiteral("reason")).toString(),
                 listenerWords(QStringLiteral("A"), QStringLiteral("iPad")));
        QCOMPARE(slice->frequency(), frequency);

        // Taking it again with the revision seen is accepted, no change.
        const Seen now = seenBy(appB, 0);
        QTRY_COMPARE(seenBy(appB, 0).revision, seen.revision + 1);
        QVERIFY(accepted(core.invoke(appB, "slice.takeControl", revisionArgs(seenBy(appB, 0)))));
        Q_UNUSED(now);
        QCOMPARE(ownership->mark(0).owner, b.key.fingerprint());
    }

    // ── Take it back (take-over parity, sliceAccessVersion 2) ───────────

    void aPeerIsSentTheLowerSliceAccessVersion()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kShares);
        LoopbackTransport* appB = core.signIn(b, kSharesBack);
        QVERIFY(admitted(appA) && admitted(appB));
        QCOMPARE(capability(appA->received(), QStringLiteral("sliceAccessVersion")), 1);
        QCOMPARE(capability(appB->received(), QStringLiteral("sliceAccessVersion")), 2);
        // Core-slice take-over: a peer that declared 3 reads 3, the Core's
        // own version.
        Device c(QStringLiteral("Pad"), QStringLiteral("tablet"));
        core.pair(c);
        LoopbackTransport* appC = core.signIn(
            c, {{"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 3}});
        QVERIFY(admitted(appC));
        QCOMPARE(capability(appC->received(), QStringLiteral("sliceAccessVersion")), 3);
    }

    void takeItBackReturnsControlAndLeavesTransmitAlone()
    {
        Core core;
        allowTransmit(core);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kSharesBackTx);
        LoopbackTransport* appB = core.signIn(b, kSharesBackTx);
        QVERIFY(admitted(appA) && admitted(appB));
        const SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownership->mark(0).owner, a.key.fingerprint());
        for (int id : ownership->ownedBy(b.key.fingerprint())) {
            QVERIFY(accepted(core.invoke(appB, "removeSlice", {int64("sliceId", id)})));
        }

        QTRY_VERIFY(holds(appB, accessKey(0)));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refArgs(seenBy(appB, 0)))));
        const QJsonObject taken =
            core.invoke(appB, "slice.takeControl", revisionArgs(seenBy(appB, 0)));
        QVERIFY2(accepted(taken), qPrintable(reasonOf(taken)));
        const qint64 afterTake = resultValue(taken, QStringLiteral("controlRevision"));

        // A is told, with Take it back and the slice as it is now.
        QTRY_VERIFY(!firstOfType(appA->received(), QStringLiteral("notice")).isEmpty());
        const QJsonObject notice = firstOfType(appA->received(), QStringLiteral("notice"));
        QCOMPARE(notice.value(QStringLiteral("kind")).toString(), QStringLiteral("controlTaken"));
        QCOMPARE(notice.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("iPad took control of slice A. You are still listening."));
        QCOMPARE(notice.value(QStringLiteral("takeBack")).toBool(false), true);
        const QJsonObject entry =
            notice.value(QStringLiteral("slices")).toArray().first().toObject();
        QCOMPARE(entry.value(QStringLiteral("sliceId")).toInt(-1), 0);
        QCOMPARE(static_cast<quint64>(entry.value(QStringLiteral("incarnation")).toInteger()),
                 ownership->incarnation(0));
        QCOMPARE(entry.value(QStringLiteral("controlRevision")).toInteger(), afterTake);

        // B chooses the slice to transmit on, and stays unkeyed.
        MoxController* mox = core.model->moxController();
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        QVERIFY(accepted(core.invoke(appB, "tx.take")));
        QTRY_VERIFY(core.server->transmitHolder()->isHeldBy(b.key.fingerprint()));
        QVERIFY(accepted(core.invoke(appB, "tx.setTxSlice", {int64("sliceId", 0)})));
        QCOMPARE(arbiter->txBoundSliceId(), 0);
        QSignalSpy moxChanges(mox, &MoxController::moxChanged);

        // One tap: notice.takeBack with the notice's id.
        const qint64 id = notice.value(QStringLiteral("id")).toInteger();
        const QJsonObject back = core.invoke(appA, "notice.takeBack", {int64("id", id)});
        QVERIFY2(accepted(back), qPrintable(reasonOf(back)));
        QCOMPARE(resultValue(back, QStringLiteral("controlRevision")), afterTake + 1);
        QCOMPARE(ownership->mark(0).owner, a.key.fingerprint());
        QVERIFY(ownership->listenersOf(0).contains(b.key.fingerprint()));

        // Transmit does not come with it (ruling Q8): B's choice of the
        // slice goes, A holds nothing, and A's key is refused until A
        // chooses the slice with tx.setTxSlice.
        QTRY_VERIFY(!core.server->transmitHolder()->isHeldBy(b.key.fingerprint()));
        QVERIFY(!core.server->transmitHolder()->isHeldBy(a.key.fingerprint()));
        QCOMPARE(moxChanges.count(), 0);
        QVERIFY(!mox->isMox());
        mox->setMox(true, keyerFor(a));
        QVERIFY(!mox->isMox());
        QCOMPARE(QString::fromLatin1(mox->lastRefusal().code),
                 QString::fromLatin1(TxRefusals::kChooseTransmitSlice));

        // B is told in turn, with a Take it back of its own.
        QTRY_VERIFY(!firstOfType(appB->received(), QStringLiteral("notice")).isEmpty());
        const QJsonObject toldB = firstOfType(appB->received(), QStringLiteral("notice"));
        QCOMPARE(toldB.value(QStringLiteral("kind")).toString(), QStringLiteral("controlTaken"));
        QCOMPARE(toldB.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("iPhone took control of slice A. You are still listening."));
        QCOMPARE(toldB.value(QStringLiteral("takeBack")).toBool(false), true);

        // The same tap again: nothing left to take back.
        const QJsonObject again = core.invoke(appA, "notice.takeBack", {int64("id", id)});
        QVERIFY(!accepted(again));
        QCOMPARE(reasonOf(again), QStringLiteral("That can no longer be taken back."));
        QCOMPARE(ownership->mark(0).owner, a.key.fingerprint());
    }

    // Desktop listening lane (JJ, 2026-09-30): the slice was taken again
    // since the notice, so the take-back record is void and the first tap
    // says so. It used to answer with the stale-revision words, which the
    // Core keeps for a real race: see
    // ofTwoTakesWithTheSameRevisionExactlyOneIsApplied, and the race check
    // at the end of this test.
    void aTakeItBackAfterControlMovedOnIsRefused()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        Device c(QStringLiteral("Mac"), QStringLiteral("computer"));
        for (Device* x : {&a, &b, &c}) {
            core.pair(*x);
        }
        LoopbackTransport* appA = core.signIn(a, kSharesBack);
        LoopbackTransport* appB = core.signIn(b, kSharesBack);
        LoopbackTransport* appC = core.signIn(c, kSharesBack);
        QVERIFY(admitted(appA) && admitted(appB) && admitted(appC));
        QTRY_VERIFY(holds(appB, accessKey(0)));
        QVERIFY(accepted(core.invoke(appB, "slice.takeControl", revisionArgs(seenBy(appB, 0)))));
        QTRY_VERIFY(!firstOfType(appA->received(), QStringLiteral("notice")).isEmpty());
        const qint64 id =
            firstOfType(appA->received(), QStringLiteral("notice")).value(QStringLiteral("id"))
                .toInteger();
        // C takes it from B before A taps.
        QTRY_VERIFY(holds(appC, accessKey(0)));
        QTRY_COMPARE(accessOf(appC, 0, "controllerDeviceId").toString(), b.id());
        QVERIFY(accepted(core.invoke(appC, "slice.takeControl", revisionArgs(seenBy(appC, 0)))));
        QCOMPARE(core.model->sliceOwnership()->mark(0).owner, c.key.fingerprint());

        const QJsonObject late = core.invoke(appA, "notice.takeBack", {int64("id", id)});
        QVERIFY(!accepted(late));
        QCOMPARE(reasonOf(late), QStringLiteral("That can no longer be taken back."));
        QVERIFY(OperatorWording::isPlain(reasonOf(late)));
        QCOMPARE(core.model->sliceOwnership()->mark(0).owner, c.key.fingerprint());
        // It never can be now, so the record is gone.
        const QJsonObject again = core.invoke(appA, "notice.takeBack", {int64("id", id)});
        QCOMPARE(reasonOf(again), QStringLiteral("That can no longer be taken back."));
        QCOMPARE(core.model->sliceOwnership()->mark(0).owner, c.key.fingerprint());
        // A real race keeps the stale-revision words: B's take at the
        // revision it saw before C took the slice.
        const QJsonObject raced =
            core.invoke(appB, "slice.takeControl",
                        {int64("sliceId", 0),
                         int64("incarnation",
                               static_cast<qint64>(core.model->sliceOwnership()->incarnation(0))),
                         int64("controlRevision",
                               static_cast<qint64>(
                                   core.model->sliceOwnership()->controlRevision(0) - 1))});
        QVERIFY(!accepted(raced));
        QCOMPARE(reasonOf(raced), QStringLiteral("Someone else changed who controls slice A. "
                                                 "Look again and try once more."));
    }

    // Desktop listening lane (JJ, 2026-09-30): the device that took the
    // slice released it before A tapped Take it back. The first tap
    // answers that it can no longer be taken back, not the stale-revision
    // words, and nothing changes.
    void aTakeItBackAfterTheTakerReleasedIsRefusedPlainly()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kSharesBack);
        LoopbackTransport* appB = core.signIn(b, kSharesBack);
        QVERIFY(admitted(appA) && admitted(appB));
        const SliceOwnership* ownership = core.model->sliceOwnership();
        QTRY_VERIFY(holds(appB, accessKey(0)));
        QVERIFY(accepted(core.invoke(appB, "slice.takeControl", revisionArgs(seenBy(appB, 0)))));
        QCOMPARE(ownership->mark(0).owner, b.key.fingerprint());
        QTRY_VERIFY(!firstOfType(appA->received(), QStringLiteral("notice")).isEmpty());
        const qint64 id =
            firstOfType(appA->received(), QStringLiteral("notice")).value(QStringLiteral("id"))
                .toInteger();
        // B releases; A still listens, so the slice stays with nobody in
        // control.
        QTRY_COMPARE(accessOf(appB, 0, "controllerDeviceId").toString(), b.id());
        const QJsonObject released =
            core.invoke(appB, "slice.release", revisionArgs(seenBy(appB, 0)));
        QVERIFY2(accepted(released), qPrintable(reasonOf(released)));
        QVERIFY(ownership->mark(0).owner.isEmpty());
        const quint64 revision = ownership->controlRevision(0);

        const QJsonObject first = core.invoke(appA, "notice.takeBack", {int64("id", id)});
        QVERIFY(!accepted(first));
        QCOMPARE(reasonOf(first), QStringLiteral("That can no longer be taken back."));
        QVERIFY(ownership->mark(0).owner.isEmpty());
        QCOMPARE(ownership->controlRevision(0), revision);
        const QJsonObject again = core.invoke(appA, "notice.takeBack", {int64("id", id)});
        QCOMPARE(reasonOf(again), QStringLiteral("That can no longer be taken back."));
    }

    // Take-over fix wave (M-2): the slice closes and is made again under
    // the same letter before A taps. The notice's incarnation is stale:
    // refused as slice.takeControl refuses it, and the record is gone.
    void aTakeItBackOfASliceClosedAndMadeAgainIsRefused()
    {
        Core core;
        core.model->configureStreamPool(5, 5, 192000);
        core.model->sliceById(0)->setFrequency(14200000.0);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kSharesBack);
        QVERIFY(admitted(appA));
        QVERIFY(accepted(core.invoke(appA, "addSlice", {utf8("initialPanId", QString())})));
        LoopbackTransport* appB = core.signIn(b, kSharesBack);
        QVERIFY(admitted(appB));
        QTRY_VERIFY(holds(appB, accessKey(0)));
        QVERIFY(accepted(core.invoke(appB, "slice.takeControl", revisionArgs(seenBy(appB, 0)))));
        QTRY_VERIFY(!firstOfType(appA->received(), QStringLiteral("notice")).isEmpty());
        const qint64 id =
            firstOfType(appA->received(), QStringLiteral("notice")).value(QStringLiteral("id"))
                .toInteger();
        SliceOwnership* ownership = core.model->sliceOwnership();
        const quint64 before = ownership->incarnation(0);

        core.model->removeSlice(0);
        QVERIFY(core.model->sliceById(0) == nullptr);
        int remade = -1;
        {
            const SliceOwnership::CreatorScope creator(ownership, b.key.fingerprint());
            remade = core.model->addSlice(QStringLiteral("pan-0"));
        }
        QCOMPARE(remade, 0);
        QVERIFY(ownership->incarnation(0) != before);

        const QJsonObject late = core.invoke(appA, "notice.takeBack", {int64("id", id)});
        QVERIFY(!accepted(late));
        QCOMPARE(reasonOf(late),
                 QStringLiteral("That slice has closed. Choose it again from the list."));
        QCOMPARE(ownership->mark(0).owner, b.key.fingerprint());
        const QJsonObject again = core.invoke(appA, "notice.takeBack", {int64("id", id)});
        QCOMPARE(reasonOf(again), QStringLiteral("That can no longer be taken back."));
    }

    void anOlderPeerIsOfferedNoTakeItBack()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kShares);
        LoopbackTransport* appB = core.signIn(b, kSharesBack);
        QVERIFY(admitted(appA) && admitted(appB));
        QTRY_VERIFY(holds(appB, accessKey(0)));
        QVERIFY(accepted(core.invoke(appB, "slice.takeControl", revisionArgs(seenBy(appB, 0)))));
        QTRY_VERIFY(!firstOfType(appA->received(), QStringLiteral("notice")).isEmpty());
        const QJsonObject notice = firstOfType(appA->received(), QStringLiteral("notice"));
        QCOMPARE(notice.value(QStringLiteral("kind")).toString(), QStringLiteral("controlTaken"));
        QCOMPARE(notice.value(QStringLiteral("takeBack")).toBool(true), false);
        // Take-over fix wave (M-1): the slice entry is the one it had,
        // without the incarnation and revision only Take it back reads.
        const QJsonObject entry =
            notice.value(QStringLiteral("slices")).toArray().first().toObject();
        QCOMPARE(entry.value(QStringLiteral("sliceId")).toInt(-1), 0);
        QVERIFY(!entry.contains(QStringLiteral("incarnation")));
        QVERIFY(!entry.contains(QStringLiteral("controlRevision")));
        // Its id sent anyway is refused, and control stays.
        const QJsonObject r = core.invoke(
            appA, "notice.takeBack", {int64("id", notice.value(QStringLiteral("id")).toInteger())});
        QVERIFY(!accepted(r));
        QCOMPARE(reasonOf(r), QStringLiteral("That can no longer be taken back."));
        QCOMPARE(core.model->sliceOwnership()->mark(0).owner, b.key.fingerprint());
    }

    void ofTwoTakesWithTheSameRevisionExactlyOneIsApplied()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        Device c(QStringLiteral("Mac"), QStringLiteral("computer"));
        for (Device* x : {&a, &b, &c}) {
            core.pair(*x);
        }
        LoopbackTransport* appA = core.signIn(a, kShares);
        LoopbackTransport* appB = core.signIn(b, kShares);
        LoopbackTransport* appC = core.signIn(c, kShares);
        QVERIFY(admitted(appA) && admitted(appB) && admitted(appC));
        QTRY_VERIFY(holds(appB, accessKey(0)));
        QTRY_VERIFY(holds(appC, accessKey(0)));
        const Seen seen = seenBy(appB, 0);
        QCOMPARE(seenBy(appC, 0).revision, seen.revision);
        const int slices = core.model->slices().size();

        const QJsonObject first = core.invoke(appB, "slice.takeControl", revisionArgs(seen));
        const QJsonObject second = core.invoke(appC, "slice.takeControl", revisionArgs(seen));
        QVERIFY(accepted(first));
        QVERIFY(!accepted(second));
        const QString changed =
            QStringLiteral("Someone else changed who controls slice A. Look again and try once "
                           "more.");
        QVERIFY(OperatorWording::isPlain(changed));
        QCOMPARE(reasonOf(second), changed);
        const SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownership->mark(0).owner, b.key.fingerprint());
        QCOMPARE(ownership->controlRevision(0), static_cast<quint64>(seen.revision + 1));
        QCOMPARE(ownership->listenersOf(0),
                 (QList<QByteArray>{b.key.fingerprint(), a.key.fingerprint()}));
        QCOMPARE(core.model->slices().size(), slices);
    }

    void anOldIncarnationReachesNothing()
    {
        Core core;
        core.model->configureStreamPool(5, 5, 192000);
        core.model->sliceById(0)->setFrequency(14200000.0);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kShares);
        QVERIFY(admitted(appA));
        const QJsonObject added = core.invoke(appA, "addSlice", {utf8("initialPanId", QString())});
        QVERIFY(accepted(added));
        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appB));
        QTRY_VERIFY(holds(appB, accessKey(0)));
        const Seen old = seenBy(appB, 0);

        // A closes A0 and makes a new slice on the same letter.
        QVERIFY(accepted(core.invoke(appA, "removeSlice", {int64("sliceId", 0)})));
        QVERIFY(core.model->sliceById(0) == nullptr);
        int remade = -1;
        {
            const SliceOwnership::CreatorScope creator(core.model->sliceOwnership(),
                                                       a.key.fingerprint());
            remade = core.model->addSlice(QStringLiteral("pan-0"));
        }
        QCOMPARE(remade, 0);
        const SliceOwnership* ownership = core.model->sliceOwnership();
        QVERIFY(ownership->incarnation(0) != static_cast<quint64>(old.incarnation));
        const quint64 revision = ownership->controlRevision(0);

        const QString closed = QStringLiteral("That slice has closed. Choose it again from the list.");
        QVERIFY(OperatorWording::isPlain(closed));
        QJsonObject r = core.invoke(appB, "slice.listen", refArgs(old));
        QVERIFY(!accepted(r));
        QCOMPARE(reasonOf(r), closed);
        r = core.invoke(appB, "slice.takeControl",
                        {int64("sliceId", 0), int64("incarnation", old.incarnation),
                         int64("controlRevision", static_cast<qint64>(revision))});
        QVERIFY(!accepted(r));
        QCOMPARE(reasonOf(r), closed);
        QCOMPARE(ownership->mark(0).owner, a.key.fingerprint());
        QCOMPARE(ownership->listenersOf(0), QList<QByteArray>{a.key.fingerprint()});
        QCOMPARE(ownership->controlRevision(0), revision);
    }

    // JJ's wider ruling (2026-09-30): every slice can be taken, the slice of
    // a session that cannot stay on as a listener included (the refusal
    // "<name> needs an update before control ..." is gone). That session
    // loses the slice and is told nothing it cannot read.
    void controlPassesFromADeviceThatCannotStayListening()
    {
        Core core;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        Device d(QStringLiteral("Mac"), QStringLiteral("computer"));
        core.pair(d);
        core.pair(b);
        LoopbackTransport* appD = core.signIn(d, kHolder);
        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appD) && admitted(appB));
        const SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownership->mark(0).owner, d.key.fingerprint());
        QTRY_VERIFY(holds(appB, accessKey(0)));
        QTRY_VERIFY(holds(appD, QStringLiteral("slice:0")));
        const QJsonObject r = core.invoke(appB, "slice.takeControl", revisionArgs(seenBy(appB, 0)));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        QCOMPARE(ownership->mark(0).owner, b.key.fingerprint());
        // The older session left the slice; the iPad controls it alone.
        QCOMPARE(ownership->listenersOf(0), QList<QByteArray>{b.key.fingerprint()});
        QTRY_VERIFY(!holds(appD, QStringLiteral("slice:0")));
        QTest::qWait(50);
        for (const QJsonObject& notice : ofType(appD->received(), QStringLiteral("notice"))) {
            QVERIFY(notice.value(QStringLiteral("kind")).toString()
                    != QStringLiteral("controlTaken"));
        }
        // A session that holds sessions stays connected without a slice.
        QVERIFY(appD->isOpen());
    }

    // JJ's wider ruling: an older window (no sessionHolder) whose only
    // slice is taken is left with none, and ends as ruling 6.10 says.
    void anOlderWindowWhoseOnlySliceIsTakenEnds()
    {
        Core core;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        Device d(QStringLiteral("Mac"), QStringLiteral("computer"));
        core.pair(d);
        core.pair(b);
        LoopbackTransport* appD = core.signIn(d, {{"deviceAuth", 1}});
        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appD) && admitted(appB));
        const SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownership->ownedBy(d.key.fingerprint()), QList<int>{0});
        QTRY_VERIFY(holds(appB, accessKey(0)));
        const QJsonObject r = core.invoke(appB, "slice.takeControl", revisionArgs(seenBy(appB, 0)));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        QCOMPARE(ownership->mark(0).owner, b.key.fingerprint());
        QVERIFY(!ownership->listenersOf(0).contains(d.key.fingerprint()));
        QVERIFY(!endOf(appD).isEmpty());
        QVERIFY(core.model->sliceById(0) != nullptr);
    }

    // Review fix (Job B, a): a current device may listen to an older
    // window's slice.
    void aDeviceMayListenToAnOlderWindowsSlice()
    {
        Core core;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        Device d(QStringLiteral("Mac"), QStringLiteral("computer"));
        core.pair(d);
        core.pair(b);
        LoopbackTransport* appD = core.signIn(d, kHolder);
        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appD) && admitted(appB));
        const SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownership->mark(0).owner, d.key.fingerprint());
        QTRY_VERIFY(holds(appB, accessKey(0)));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refArgs(seenBy(appB, 0)))));
        QVERIFY(ownership->listenersOf(0).contains(b.key.fingerprint()));
        QCOMPARE(ownership->mark(0).owner, d.key.fingerprint());
    }

    // Review fix (Job B, b): the older window's own close of its slice
    // releases it while a remote listener is there (ruling Q6): the slice
    // stays for B, controlled by nobody.
    void anOlderWindowsCloseReleasesItsSliceToAListener()
    {
        Core core;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        Device d(QStringLiteral("Mac"), QStringLiteral("computer"));
        core.pair(d);
        core.pair(b);
        LoopbackTransport* appD = core.signIn(d, kHolder);
        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appD) && admitted(appB));
        QCOMPARE(core.model->sliceOwnership()->mark(0).owner, d.key.fingerprint());
        QTRY_VERIFY(holds(appB, accessKey(0)));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refArgs(seenBy(appB, 0)))));
        QVERIFY(accepted(core.invoke(appD, "removeSlice", {int64("sliceId", 0)})));
        QVERIFY(core.model->sliceById(0) != nullptr);
        QVERIFY(core.model->sliceOwnership()->mark(0).owner.isEmpty());
        QCOMPARE(core.model->sliceOwnership()->listenersOf(0),
                 QList<QByteArray>{b.key.fingerprint()});
        QTRY_VERIFY(!holds(appD, QStringLiteral("slice:0")));
    }

    // ── Transmit (ruling Q8) ─────────────────────────────────────────────

    void takingTheIdleTransmitSliceMovesTheFlagToTheFormerControllersOther()
    {
        Core core;
        allowTransmit(core);
        const int second = addCoHostedSlice(*core.model);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kSharesTx);
        LoopbackTransport* appB = core.signIn(b, kSharesTx);
        QVERIFY(admitted(appA) && admitted(appB));
        const SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownership->mark(0).owner, a.key.fingerprint());
        QCOMPARE(ownership->mark(second).owner, a.key.fingerprint());
        MoxController* mox = core.model->moxController();
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        // A holds transmit, unkeyed, on A0.
        mox->setMox(true, keyerFor(a));
        mox->setMox(false, keyerFor(a));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QVERIFY(accepted(core.invoke(appA, "tx.setTxSlice", {int64("sliceId", 0)})));
        QCOMPARE(arbiter->txBoundSliceId(), 0);
        QVERIFY(core.server->transmitHolder()->isHeldBy(a.key.fingerprint()));
        QSignalSpy moxChanges(mox, &MoxController::moxChanged);

        QTRY_VERIFY(holds(appB, accessKey(0)));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refArgs(seenBy(appB, 0)))));
        const QJsonObject r = core.invoke(appB, "slice.takeControl", revisionArgs(seenBy(appB, 0)));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        QCOMPARE(arbiter->txBoundSliceId(), second);
        QCOMPARE(core.server->chosenTxSliceForTest(a.key.fingerprint()), second);
        QVERIFY(core.server->transmitHolder()->isHeldBy(a.key.fingerprint()));
        QVERIFY(!core.server->transmitHolder()->isHeldBy(b.key.fingerprint()));
        QCOMPARE(moxChanges.count(), 0);
        QVERIFY(!mox->isMox());
        QTRY_COMPARE(accessOf(appA, 0, "txSelected").toBool(true), false);
    }

    void takingTheOnlyIdleTransmitSliceReleasesTransmit()
    {
        Core core;
        allowTransmit(core);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kSharesTx);
        QVERIFY(admitted(appA));
        LoopbackTransport* appB = core.signIn(b, kSharesTx);
        QVERIFY(admitted(appB));
        QCOMPARE(core.model->sliceOwnership()->ownedBy(a.key.fingerprint()), QList<int>{0});
        MoxController* mox = core.model->moxController();
        mox->setMox(true, keyerFor(a));
        mox->setMox(false, keyerFor(a));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QVERIFY(core.server->transmitHolder()->isHeldBy(a.key.fingerprint()));
        QCOMPARE(core.model->txSliceArbiter()->txBoundSliceId(), 0);
        QSignalSpy moxChanges(mox, &MoxController::moxChanged);

        QTRY_VERIFY(holds(appB, accessKey(0)));
        const QJsonObject r = core.invoke(appB, "slice.takeControl", revisionArgs(seenBy(appB, 0)));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        QTRY_VERIFY(!core.server->transmitHolder()->holder().has_value());
        QCOMPARE(moxChanges.count(), 0);
        QVERIFY(!mox->isMox());
    }

    // TX safety (take-over review, I-2): a device that does not hold
    // transmit loses its only slice, the idle transmit slice. Its key must
    // never land on the slice another device now controls (ruling Q8).
    void aKeyNeverLandsOnTheSliceADeviceLostWhileNotHoldingTransmit()
    {
        Core core;
        allowTransmit(core);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kSharesTx);
        LoopbackTransport* appB = core.signIn(b, kSharesTx);
        QVERIFY(admitted(appA) && admitted(appB));
        const SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownership->ownedBy(a.key.fingerprint()), QList<int>{0});
        MoxController* mox = core.model->moxController();
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        QCOMPARE(arbiter->txBoundSliceId(), 0);
        QVERIFY(!core.server->transmitHolder()->holder().has_value());

        QTRY_VERIFY(holds(appB, accessKey(0)));
        const QJsonObject r = core.invoke(appB, "slice.takeControl", revisionArgs(seenBy(appB, 0)));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        QCOMPARE(ownership->mark(0).owner, b.key.fingerprint());
        QVERIFY(ownership->ownedBy(a.key.fingerprint()).isEmpty());

        QSignalSpy moxChanges(mox, &MoxController::moxChanged);
        mox->setMox(true, keyerFor(a));
        QTest::qWait(50);
        QVERIFY(!mox->isMox());
        QCOMPARE(mox->state(), MoxState::Rx);
        QCOMPARE(moxChanges.count(), 0);
        QCOMPARE(QString::fromLatin1(mox->lastRefusal().code),
                 QString::fromLatin1(TxRefusals::kNoTransmitSlice));
        QVERIFY(!core.server->transmitHolder()->isHeldBy(a.key.fingerprint()));
        QCOMPARE(ownership->mark(0).owner, b.key.fingerprint());
    }

    // TX safety (take-over re-review, N-1): the remote variant. A device
    // with no slice of its own keys while the flag sits on another device's
    // slice (remembered there under 8.10 after that device let go). Its
    // key never lands there.
    void aKeyWithNoSliceOfItsOwnNeverLandsOnAnotherDevicesSlice()
    {
        Core core;
        allowTransmit(core);
        core.model->configureStreamPool(5, 5, 192000);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        Device c(QStringLiteral("iPhone"), QStringLiteral("phone"));
        core.pair(a);
        core.pair(b);
        core.pair(c);
        LoopbackTransport* appA = core.signIn(a, kSharesTx);
        QVERIFY(admitted(appA));
        SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownership->ownedBy(a.key.fingerprint()), QList<int>{0});
        LoopbackTransport* appC = core.signIn(c, kSharesTx);
        QVERIFY(admitted(appC));
        if (ownership->ownedBy(c.key.fingerprint()).isEmpty()) {
            QVERIFY(accepted(core.invoke(appC, "addSlice", {utf8("initialPanId", QString())})));
        }
        const int p = ownership->ownedBy(c.key.fingerprint()).first();
        QVERIFY(p != 0);
        MoxController* mox = core.model->moxController();
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        QVERIFY(accepted(core.invoke(appC, "tx.take")));
        QTRY_VERIFY(core.server->transmitHolder()->isHeldBy(c.key.fingerprint()));
        QVERIFY(accepted(core.invoke(appC, "tx.setTxSlice", {int64("sliceId", p)})));
        QTRY_COMPARE(arbiter->txBoundSliceId(), p);
        core.server->releaseTransmitFor(c.key.fingerprint(), QStringLiteral("The test let go."));
        QTRY_VERIFY(!core.server->transmitHolder()->holder().has_value());
        QCOMPARE(arbiter->txBoundSliceId(), p);

        LoopbackTransport* appB = core.signIn(b, kSharesTx);
        QVERIFY(admitted(appB));
        QTRY_VERIFY(holds(appB, accessKey(0)));
        const QJsonObject r = core.invoke(appB, "slice.takeControl", revisionArgs(seenBy(appB, 0)));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        QVERIFY(ownership->ownedBy(a.key.fingerprint()).isEmpty());
        QCOMPARE(arbiter->txBoundSliceId(), p);

        QSignalSpy moxChanges(mox, &MoxController::moxChanged);
        mox->setMox(true, keyerFor(a));
        QTest::qWait(50);
        QVERIFY(!mox->isMox());
        QCOMPARE(mox->state(), MoxState::Rx);
        QCOMPARE(moxChanges.count(), 0);
        QCOMPARE(QString::fromLatin1(mox->lastRefusal().code),
                 QString::fromLatin1(TxRefusals::kNoTransmitSlice));
        QVERIFY(!core.server->transmitHolder()->isHeldBy(a.key.fingerprint()));
        QCOMPARE(arbiter->txBoundSliceId(), p);
    }

    // The same device with a second slice of its own keys there instead.
    void aKeyAfterLosingTheTransmitSliceGoesToTheDevicesOtherSlice()
    {
        Core core;
        allowTransmit(core);
        const int second = addCoHostedSlice(*core.model);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kSharesTx);
        LoopbackTransport* appB = core.signIn(b, kSharesTx);
        QVERIFY(admitted(appA) && admitted(appB));
        const SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownership->mark(second).owner, a.key.fingerprint());
        core.model->sliceById(second)->setDspMode(DSPMode::USB);
        core.model->sliceById(second)->setFrequency(14250000.0);
        MoxController* mox = core.model->moxController();
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        QCOMPARE(arbiter->txBoundSliceId(), 0);
        QVERIFY(!core.server->transmitHolder()->holder().has_value());

        QTRY_VERIFY(holds(appB, accessKey(0)));
        const QJsonObject r = core.invoke(appB, "slice.takeControl", revisionArgs(seenBy(appB, 0)));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));

        mox->setMox(true, keyerFor(a));
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        QCOMPARE(arbiter->txBoundSliceId(), second);
        mox->setMox(false, keyerFor(a));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    void aTransmittingSliceCannotChangeHands()
    {
        Core core;
        allowTransmit(core);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kSharesTx);
        LoopbackTransport* appB = core.signIn(b, kSharesTx);
        QVERIFY(admitted(appA) && admitted(appB));
        QTRY_VERIFY(holds(appB, accessKey(0)));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refArgs(seenBy(appB, 0)))));
        MoxController* mox = core.model->moxController();
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        mox->setMox(true, keyerFor(a));
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        QCOMPARE(arbiter->txBoundSliceId(), 0);
        QTRY_COMPARE(accessOf(appB, 0, "onAir").toBool(false), true);

        const SliceOwnership* ownership = core.model->sliceOwnership();
        const Seen seen = seenBy(appB, 0);
        QJsonObject r = core.invoke(appB, "slice.takeControl", revisionArgs(seen));
        QVERIFY(!accepted(r));
        const QString words = QStringLiteral("Slice A is transmitting. Take control once it stops.");
        QVERIFY(OperatorWording::isPlain(words));
        QCOMPARE(reasonOf(r), words);
        QCOMPARE(ownership->mark(0).owner, a.key.fingerprint());
        QCOMPARE(ownership->listenersOf(0),
                 (QList<QByteArray>{a.key.fingerprint(), b.key.fingerprint()}));
        QCOMPARE(arbiter->txBoundSliceId(), 0);
        QVERIFY(mox->isMox());

        // A release with a listener is a hand-off too.
        r = core.invoke(appA, "slice.release", revisionArgs(seen));
        QVERIFY(!accepted(r));
        QCOMPARE(reasonOf(r), QStringLiteral("Slice A is transmitting. Release it once it stops."));
        QCOMPARE(ownership->mark(0).owner, a.key.fingerprint());

        mox->setMox(false, keyerFor(a));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        r = core.invoke(appB, "slice.takeControl", revisionArgs(seen));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        QCOMPARE(ownership->mark(0).owner, b.key.fingerprint());
    }

    // Slice control fix wave (Critical 1): a transmit move waiting for the
    // unkey counts as transmitting on the slice it lands on, so control of
    // that slice cannot pass before the flag lands, and the former
    // controller's transmit never ends up on another device's slice.
    void aTransmitMoveWaitingForTheUnkeyHoldsItsSlice()
    {
        Core core;
        allowTransmit(core);
        const int second = addCoHostedSlice(*core.model);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kSharesTx);
        LoopbackTransport* appB = core.signIn(b, kSharesTx);
        QVERIFY(admitted(appA) && admitted(appB));
        const SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownership->mark(0).owner, a.key.fingerprint());
        QCOMPARE(ownership->mark(second).owner, a.key.fingerprint());
        QTRY_VERIFY(holds(appB, accessKey(0)));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refArgs(seenBy(appB, 0)))));
        MoxController* mox = core.model->moxController();
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        // A holds transmit on its second slice and keys there.
        mox->setMox(true, keyerFor(a));
        mox->setMox(false, keyerFor(a));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QVERIFY(accepted(core.invoke(appA, "tx.setTxSlice", {int64("sliceId", second)})));
        QCOMPARE(arbiter->txBoundSliceId(), second);
        mox->setMox(true, keyerFor(a));
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        // The key's end is held (its walk stalls, the gate's bound never
        // fires), so the move below waits.
        mox->setTimerIntervals(0, 0, 0, 0, 600000, 0);
        core.model->unkeyGate()->setScheduler([](int, QObject*, std::function<void()>) {});

        // A moves its transmit to A0 while keyed: the move waits.
        QVERIFY(accepted(core.invoke(appA, "tx.setTxSlice", {int64("sliceId", 0)})));
        QCOMPARE(arbiter->pendingHandoffSliceId(), 0);
        QCOMPARE(arbiter->txBoundSliceId(), second);
        // Fix wave (minor): the listener reads A0 as on the air while the
        // move waits, with no change of holder to announce it.
        QTRY_COMPARE(accessOf(appB, 0, "onAir").toBool(false), true);

        // B cannot take A0 meanwhile, and A cannot release it to B.
        const Seen seen = seenBy(appB, 0);
        QJsonObject r = core.invoke(appB, "slice.takeControl", revisionArgs(seen));
        QVERIFY(!accepted(r));
        QCOMPARE(reasonOf(r), QStringLiteral("Slice A is transmitting. Take control once it stops."));
        r = core.invoke(appA, "slice.release", revisionArgs(seen));
        QVERIFY(!accepted(r));
        QCOMPARE(reasonOf(r), QStringLiteral("Slice A is transmitting. Release it once it stops."));
        QCOMPARE(ownership->mark(0).owner, a.key.fingerprint());

        // The key ends; the flag lands on A0, still A's.
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QTRY_COMPARE(arbiter->txBoundSliceId(), 0);
        QCOMPARE(arbiter->pendingHandoffSliceId(), -1);
        QTRY_COMPARE(accessOf(appB, 0, "onAir").toBool(true), false);

        // Now B takes it, idle: the flag leaves the slice B took.
        r = core.invoke(appB, "slice.takeControl", revisionArgs(seenBy(appB, 0)));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        QCOMPARE(ownership->mark(0).owner, b.key.fingerprint());
        QCOMPARE(arbiter->txBoundSliceId(), second);
        QVERIFY(core.server->transmitHolder()->isHeldBy(a.key.fingerprint()));
        QVERIFY(!mox->isMox());
    }

    // Slice control fix wave (Important 4): the record of an explicit
    // transmit choice is written only by tx.setTxSlice (and the hosting
    // desktop's own selection), never by the binding a new holder gets by
    // itself, and it goes when control of the slice passes.
    // Slice control fix wave, round 2: the explicit transmit choice goes
    // whenever the slice leaves the device's control, however it leaves
    // (here a plain change of owner, as a layout restore, a token release
    // or a revoke makes), not only by take and release.
    void anExplicitTransmitChoiceGoesWhenTheSliceLeavesTheDevicesControl()
    {
        Core core;
        allowTransmit(core);
        core.model->configureStreamPool(5, 5, 192000);
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kSharesTx);
        QVERIFY(admitted(appB));
        SliceOwnership* ownership = core.model->sliceOwnership();
        if (ownership->ownedBy(b.key.fingerprint()).isEmpty()) {
            QVERIFY(accepted(core.invoke(appB, "addSlice", {utf8("initialPanId", QString())})));
        }
        const int mine = ownership->ownedBy(b.key.fingerprint()).first();
        MoxController* mox = core.model->moxController();
        mox->setMox(true, keyerFor(b));
        mox->setMox(false, keyerFor(b));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QVERIFY(accepted(core.invoke(appB, "tx.setTxSlice", {int64("sliceId", mine)})));
        QCOMPARE(core.server->explicitTxSliceFor(b.key.fingerprint()), mine);

        ownership->setOwner(mine, {});
        QCOMPARE(core.server->explicitTxSliceFor(b.key.fingerprint()), -1);
    }

    // Slice control plan Task 8b: two windows of one computer, one run with
    // a profile, are two devices; a refusal names the other plainly.
    void aRefusalNamesTheOtherProfilePlainly()
    {
        Core core;
        Device plain(QStringLiteral("MacBook-Pro"), QStringLiteral("computer"));
        Device profiled(QStringLiteral("MacBook-Pro (radxa_5c_r3)"), QStringLiteral("computer"));
        core.pair(plain);
        core.pair(profiled);
        LoopbackTransport* appPlain = core.signIn(plain, kShares);
        LoopbackTransport* appProfiled = core.signIn(profiled, kShares);
        QVERIFY(admitted(appPlain) && admitted(appProfiled));
        const SliceOwnership* ownership = core.model->sliceOwnership();
        const int profiledSlice = ownership->ownedBy(profiled.key.fingerprint()).first();
        const QJsonObject r =
            core.invoke(appPlain, "nnr.resetTuning", {int64("sliceId", profiledSlice)});
        QVERIFY(!accepted(r));
        QVERIFY2(reasonOf(r).contains(QStringLiteral("MacBook-Pro (radxa_5c_r3)")),
                 qPrintable(reasonOf(r)));
        const int plainSlice = ownership->ownedBy(plain.key.fingerprint()).first();
        const QJsonObject back =
            core.invoke(appProfiled, "nnr.resetTuning", {int64("sliceId", plainSlice)});
        QVERIFY(!accepted(back));
        QVERIFY(reasonOf(back).contains(QStringLiteral("MacBook-Pro")));
        QVERIFY2(!reasonOf(back).contains(QStringLiteral("radxa")), qPrintable(reasonOf(back)));
    }

    void onlyAnExplicitSelectionMarksATransmitSliceChosen()
    {
        Core core;
        allowTransmit(core);
        core.model->configureStreamPool(5, 5, 192000);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kSharesTx);
        LoopbackTransport* appB = core.signIn(b, kSharesTx);
        QVERIFY(admitted(appA) && admitted(appB));
        SliceOwnership* ownership = core.model->sliceOwnership();
        if (ownership->ownedBy(b.key.fingerprint()).isEmpty()) {
            QVERIFY(accepted(core.invoke(appB, "addSlice", {utf8("initialPanId", QString())})));
        }
        const int mine = ownership->ownedBy(b.key.fingerprint()).first();
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        QVERIFY(arbiter->txBoundSliceId() != mine);

        // B keys: its hold binds its own slice by itself.
        MoxController* mox = core.model->moxController();
        mox->setMox(true, keyerFor(b));
        mox->setMox(false, keyerFor(b));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QVERIFY(core.server->transmitHolder()->isHeldBy(b.key.fingerprint()));
        QTRY_COMPARE(arbiter->txBoundSliceId(), mine);
        QCOMPARE(core.server->explicitTxSliceFor(b.key.fingerprint()), -1);

        // Its own selection is the record.
        QVERIFY(accepted(core.invoke(appB, "tx.setTxSlice", {int64("sliceId", mine)})));
        QCOMPARE(core.server->explicitTxSliceFor(b.key.fingerprint()), mine);

        // Control of the slice passes: the record goes with it.
        QTRY_VERIFY(holds(appA, accessKey(mine)));
        const QJsonObject r =
            core.invoke(appA, "slice.takeControl", revisionArgs(seenBy(appA, mine)));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        QCOMPARE(core.server->explicitTxSliceFor(b.key.fingerprint()), -1);
        QCOMPARE(core.server->explicitTxSliceFor(a.key.fingerprint()), -1);

        // The hosting desktop's own selection is the station device's.
        // Fix wave (Minor 3): a slice nobody is on, since the window may
        // not move transmit onto another device's.
        QCOMPARE(core.server->explicitTxSliceFor(SliceOwnership::stationDevice()), -1);
        const int free = core.model->addSlice();
        QVERIFY(free >= 0);
        QVERIFY(core.model->requestTxHandoffToSlice(free));
        QCOMPARE(core.server->explicitTxSliceFor(SliceOwnership::stationDevice()), free);
    }

    // ── Keying on a slice taken from another device (Task 11, ruling Q8) ──
    // A key is refused only when it would land on a slice the device took
    // control of from another device and has not chosen for transmit, and
    // it has no other slice it may transmit on. No radio: the fake MOX.

    void aLoneDeviceKeysOnItsOwnSliceWithoutChoosingIt()
    {
        Core core;
        allowTransmit(core);
        core.model->configureStreamPool(5, 5, 192000);
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kSharesTx);
        QVERIFY(admitted(appB));
        SliceOwnership* ownership = core.model->sliceOwnership();
        if (ownership->ownedBy(b.key.fingerprint()).isEmpty()) {
            QVERIFY(accepted(core.invoke(appB, "addSlice", {utf8("initialPanId", QString())})));
        }
        const int mine = ownership->ownedBy(b.key.fingerprint()).first();
        MoxController* mox = core.model->moxController();
        mox->setMox(true, keyerFor(b));
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        QCOMPARE(core.model->txSliceArbiter()->txBoundSliceId(), mine);
        QCOMPARE(core.server->explicitTxSliceFor(b.key.fingerprint()), -1);
        mox->setMox(false, keyerFor(b));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    void aKeyOnATakenSliceIsRefusedUntilTheDeviceChoosesIt()
    {
        Core core;
        allowTransmit(core);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kSharesTx);
        LoopbackTransport* appB = core.signIn(b, kSharesTx);
        QVERIFY(admitted(appA) && admitted(appB));
        SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownership->mark(0).owner, a.key.fingerprint());
        for (int id : ownership->ownedBy(b.key.fingerprint())) {
            QVERIFY(accepted(core.invoke(appB, "removeSlice", {int64("sliceId", id)})));
        }
        QTRY_VERIFY(holds(appB, accessKey(0)));
        QJsonObject r = core.invoke(appB, "slice.takeControl", revisionArgs(seenBy(appB, 0)));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        QCOMPARE(ownership->ownedBy(b.key.fingerprint()), QList<int>{0});
        MoxController* mox = core.model->moxController();
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();

        // B keys on the slice it took and never chose: refused, plainly,
        // and it takes nothing.
        mox->setMox(true, keyerFor(b));
        QVERIFY(!mox->isMox());
        QCOMPARE(QString::fromLatin1(mox->lastRefusal().code),
                 QString::fromLatin1(TxRefusals::kChooseTransmitSlice));
        const QString words = TxRefusals::chooseTransmitSlice().text;
        QVERIFY2(OperatorWording::isPlain(words), qPrintable(words));
        QVERIFY(words.contains(QStringLiteral("TX button")));
        QVERIFY(!core.server->transmitHolder()->isHeldBy(b.key.fingerprint()));

        // Holding transmit binds it there by itself, which is not a choice.
        QVERIFY(accepted(core.invoke(appB, "tx.take")));
        QTRY_VERIFY(core.server->transmitHolder()->isHeldBy(b.key.fingerprint()));
        QTRY_COMPARE(arbiter->txBoundSliceId(), 0);
        mox->setMox(true, keyerFor(b));
        QVERIFY(!mox->isMox());
        QCOMPARE(QString::fromLatin1(mox->lastRefusal().code),
                 QString::fromLatin1(TxRefusals::kChooseTransmitSlice));

        // Its own choice: the same key now transmits.
        QVERIFY(accepted(core.invoke(appB, "tx.setTxSlice", {int64("sliceId", 0)})));
        mox->setMox(true, keyerFor(b));
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        QCOMPARE(arbiter->txBoundSliceId(), 0);
        mox->setMox(false, keyerFor(b));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        Q_UNUSED(appA);
    }

    void aDeviceWithAnotherSliceItMayTransmitOnKeysThere()
    {
        Core core;
        allowTransmit(core);
        core.model->configureStreamPool(5, 5, 192000);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kSharesTx);
        LoopbackTransport* appB = core.signIn(b, kSharesTx);
        QVERIFY(admitted(appA) && admitted(appB));
        SliceOwnership* ownership = core.model->sliceOwnership();
        for (int id : ownership->ownedBy(b.key.fingerprint())) {
            QVERIFY(accepted(core.invoke(appB, "removeSlice", {int64("sliceId", id)})));
        }
        QTRY_VERIFY(holds(appB, accessKey(0)));
        QJsonObject r = core.invoke(appB, "slice.takeControl", revisionArgs(seenBy(appB, 0)));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        MoxController* mox = core.model->moxController();
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        // B holds transmit with only the taken slice: bound there.
        QVERIFY(accepted(core.invoke(appB, "tx.take")));
        QTRY_VERIFY(core.server->transmitHolder()->isHeldBy(b.key.fingerprint()));
        QTRY_COMPARE(arbiter->txBoundSliceId(), 0);

        // It adds a slice of its own: its key goes there, not to the one
        // it took.
        QVERIFY(accepted(core.invoke(appB, "addSlice", {utf8("initialPanId", QString())})));
        int own = -1;
        for (int id : ownership->ownedBy(b.key.fingerprint())) {
            if (id != 0) {
                own = id;
            }
        }
        QVERIFY(own >= 0);
        core.model->sliceById(own)->setDspMode(DSPMode::USB);
        core.model->sliceById(own)->setFrequency(14250000.0);
        mox->setMox(true, keyerFor(b));
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        QCOMPARE(arbiter->txBoundSliceId(), own);
        mox->setMox(false, keyerFor(b));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        Q_UNUSED(appA);
    }

    // Task 11: selecting a listened slice for receive never moves transmit.
    void selectingAListenedSliceForReceiveLeavesTransmitAlone()
    {
        Core core;
        allowTransmit(core);
        core.model->configureStreamPool(5, 5, 192000);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kSharesTx);
        LoopbackTransport* appB = core.signIn(b, kSharesTx);
        QVERIFY(admitted(appA) && admitted(appB));
        SliceOwnership* ownership = core.model->sliceOwnership();
        if (ownership->ownedBy(b.key.fingerprint()).isEmpty()) {
            QVERIFY(accepted(core.invoke(appB, "addSlice", {utf8("initialPanId", QString())})));
        }
        const int mine = ownership->ownedBy(b.key.fingerprint()).first();
        MoxController* mox = core.model->moxController();
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        mox->setMox(true, keyerFor(b));
        mox->setMox(false, keyerFor(b));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QVERIFY(accepted(core.invoke(appB, "tx.setTxSlice", {int64("sliceId", mine)})));
        const quint64 epoch = core.server->transmitHolder()->epoch();

        QTRY_VERIFY(holds(appB, accessKey(0)));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refArgs(seenBy(appB, 0)))));
        QVERIFY(accepted(core.invoke(appB, "setActiveSliceById", {int64("sliceId", 0)})));
        QCOMPARE(arbiter->txBoundSliceId(), mine);
        QCOMPARE(core.server->chosenTxSliceForTest(b.key.fingerprint()), mine);
        QCOMPARE(core.server->explicitTxSliceFor(b.key.fingerprint()), mine);
        QVERIFY(core.server->transmitHolder()->isHeldBy(b.key.fingerprint()));
        QCOMPARE(core.server->transmitHolder()->epoch(), epoch);
        QVERIFY(!mox->isMox());
        Q_UNUSED(appA);
    }

    // The review's third-holder case: a device holding transmit with the
    // flag parked on a slice it does not control loses that selection when
    // another device takes the slice.
    void takingASliceClearsAnotherHoldersSelectionOfIt()
    {
        Core core;
        allowTransmit(core);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        Device c(QStringLiteral("Mac"), QStringLiteral("computer"));
        core.pair(a);
        core.pair(b);
        core.pair(c);
        LoopbackTransport* appA = core.signIn(a, kSharesTx);
        LoopbackTransport* appB = core.signIn(b, kSharesTx);
        LoopbackTransport* appC = core.signIn(c, kSharesTx);
        QVERIFY(admitted(appA) && admitted(appB) && admitted(appC));
        SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownership->mark(0).owner, a.key.fingerprint());
        // C gives up its own slices, so its transmit binds nowhere of its
        // own and the flag stays parked on A0.
        for (int id : ownership->ownedBy(c.key.fingerprint())) {
            QVERIFY(accepted(core.invoke(appC, "removeSlice", {int64("sliceId", id)})));
        }
        QVERIFY(ownership->ownedBy(c.key.fingerprint()).isEmpty());
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        QCOMPARE(arbiter->txBoundSliceId(), 0);
        MoxController* mox = core.model->moxController();
        // Take-over re-review (N-1): C shares slices, so its key on A's
        // slice is refused; it holds transmit unkeyed (tx.take) instead.
        mox->setMox(true, keyerFor(c));
        QVERIFY(!mox->isMox());
        QCOMPARE(QString::fromLatin1(mox->lastRefusal().code),
                 QString::fromLatin1(TxRefusals::kNoTransmitSlice));
        QVERIFY(accepted(core.invoke(appC, "tx.take")));
        QTRY_VERIFY(core.server->transmitHolder()->isHeldBy(c.key.fingerprint()));
        QCOMPARE(arbiter->txBoundSliceId(), 0);

        QTRY_VERIFY(holds(appB, accessKey(0)));
        const QJsonObject r =
            core.invoke(appB, "slice.takeControl", revisionArgs(seenBy(appB, 0)));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        QCOMPARE(ownership->mark(0).owner, b.key.fingerprint());
        // C controls no slice to move to: its transmit is released.
        QTRY_VERIFY(!core.server->transmitHolder()->isHeldBy(c.key.fingerprint()));
        QVERIFY(!mox->isMox());
    }

    // ── Leaving the Core (fix wave, Important 2) ────────────────────────
    // A device that leaves, is revoked or stays away past its 180 s stops
    // listening too. A slice kept only for it closes, with its receiver.

    // A and B on the Core: A controls A0 and A1, B listens to both, then A
    // releases A0, which stays for B alone. Returns A0's receiver stream.
    int listenerKeepsAReleasedSlice(Core& core, const Device& a, const Device& b,
                                    LoopbackTransport** appA, LoopbackTransport** appB)
    {
        core.model->configureStreamPool(5, 5, 192000);
        core.model->sliceById(0)->setFrequency(14200000.0);
        *appA = core.signIn(a, kShares);
        if (!admitted(*appA)) {
            return -1;
        }
        const QJsonObject added = core.invoke(*appA, "addSlice", {utf8("initialPanId", QString())});
        if (!accepted(added)) {
            return -1;
        }
        core.model->sliceById(0)->setFrequency(7074000.0);
        *appB = core.signIn(b, kShares);
        if (!admitted(*appB)) {
            return -1;
        }
        const bool seen = QTest::qWaitFor(
            [&]() { return holds(*appB, accessKey(0)) && holds(*appB, accessKey(1)); }, 5000);
        if (!seen
            || !accepted(core.invoke(*appB, "slice.listen", refArgs(seenBy(*appB, 0))))
            || !accepted(core.invoke(*appB, "slice.listen", refArgs(seenBy(*appB, 1))))
            || !accepted(core.invoke(*appA, "slice.release", revisionArgs(seenBy(*appA, 0))))) {
            return -1;
        }
        return core.model->sliceById(0)->streamIndex();
    }

    void aDeviceThatLeavesStopsListening()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = nullptr;
        LoopbackTransport* appB = nullptr;
        const int stream = listenerKeepsAReleasedSlice(core, a, b, &appA, &appB);
        QVERIFY(stream >= 0);
        const SliceOwnership* ownership = core.model->sliceOwnership();
        QVERIFY(ownership->mark(0).owner.isEmpty());
        QCOMPARE(ownership->listenersOf(0), QList<QByteArray>{b.key.fingerprint()});
        QCOMPARE(ownership->listenersOf(1),
                 (QList<QByteArray>{a.key.fingerprint(), b.key.fingerprint()}));

        QVERIFY(accepted(core.invoke(appB, "session.leave")));
        QTRY_VERIFY(!appB->isOpen());
        // A0 was kept only for B: it closes, and its receiver is free.
        QTRY_VERIFY(core.model->sliceById(0) == nullptr);
        QVERIFY(!core.model->streamAllocator().isStreamActive(stream));
        // A1 stays A's, with B gone from it.
        QCOMPARE(ownership->listenersOf(1), QList<QByteArray>{a.key.fingerprint()});
        QVERIFY(ownership->joinedBy(b.key.fingerprint()).isEmpty());
    }

    void aDeviceAwayPastItsGraceStopsListening()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = nullptr;
        LoopbackTransport* appB = nullptr;
        const int stream = listenerKeepsAReleasedSlice(core, a, b, &appA, &appB);
        QVERIFY(stream >= 0);
        const SliceOwnership* ownership = core.model->sliceOwnership();

        appB->closeLink(QStringLiteral("lost"));
        QTRY_VERIFY(core.sessions().entry(b.key.fingerprint()).has_value()
                    && core.sessions().entry(b.key.fingerprint())->state
                        == DeviceSessionRegistry::State::Away);
        // Away within its 180 s: still listening.
        QCOMPARE(ownership->listenersOf(0), QList<QByteArray>{b.key.fingerprint()});
        QVERIFY(core.model->sliceById(0) != nullptr);

        core.now += DeviceSessionRegistry::kGraceMs + 1;
        core.sessions().expireAway();
        QTRY_VERIFY(core.model->sliceById(0) == nullptr);
        QVERIFY(!core.model->streamAllocator().isStreamActive(stream));
        QCOMPARE(ownership->listenersOf(1), QList<QByteArray>{a.key.fingerprint()});
        QVERIFY(ownership->joinedBy(b.key.fingerprint()).isEmpty());
    }

    void aRevokedDeviceStopsListening()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = nullptr;
        LoopbackTransport* appB = nullptr;
        const int stream = listenerKeepsAReleasedSlice(core, a, b, &appA, &appB);
        QVERIFY(stream >= 0);
        const SliceOwnership* ownership = core.model->sliceOwnership();

        const QJsonObject revoked = core.invoke(appA, "devices.revoke", {utf8("id", b.id())});
        QVERIFY2(accepted(revoked), qPrintable(reasonOf(revoked)));
        QTRY_VERIFY(core.model->sliceById(0) == nullptr);
        QVERIFY(!core.model->streamAllocator().isStreamActive(stream));
        QCOMPARE(ownership->listenersOf(1), QList<QByteArray>{a.key.fingerprint()});
        QVERIFY(ownership->joinedBy(b.key.fingerprint()).isEmpty());
    }

    // ── Release ──────────────────────────────────────────────────────────

    void aReleasedSliceStaysForItsListenerWhoIsNotGivenControl()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kShares);
        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appA) && admitted(appB));
        QTRY_VERIFY(holds(appB, accessKey(0)));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refArgs(seenBy(appB, 0)))));
        const SliceOwnership* ownership = core.model->sliceOwnership();
        QVERIFY(SliceAccessPolicy::mayHear(*ownership, b.key.fingerprint(), 0));

        // Only the controller releases.
        const Seen seen = seenBy(appA, 0);
        QJsonObject r = core.invoke(appB, "slice.release", revisionArgs(seen));
        QVERIFY(!accepted(r));
        QCOMPARE(reasonOf(r), QStringLiteral("Only the device that controls slice A can release it."));

        r = core.invoke(appA, "slice.release", revisionArgs(seen));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        QVERIFY(core.model->sliceById(0) != nullptr);
        QVERIFY(ownership->mark(0).owner.isEmpty());
        QCOMPARE(ownership->listenersOf(0), QList<QByteArray>{b.key.fingerprint()});
        QVERIFY(SliceAccessPolicy::mayHear(*ownership, b.key.fingerprint(), 0));
        QTRY_VERIFY(!holds(appA, QStringLiteral("slice:0")));
        QTRY_VERIFY(holds(appA, QStringLiteral("marker:0")));
        QVERIFY(holds(appB, QStringLiteral("slice:0")));
        QTRY_COMPARE(accessOf(appB, 0, "controllerDeviceId").toString(), QString());
        QTRY_COMPARE(idList(accessOf(appB, 0, "listenerDeviceIds")), QStringList{b.id()});

        // B alone on the Core: a new slice nobody owns is adopted, the
        // released one is not (ruling Q9).
        QVERIFY(accepted(core.invoke(appA, "session.leave")));
        QTRY_COMPARE(core.sessions().entries().size(), 1);
        const int fresh = core.model->addSlice(QStringLiteral("pan-0"));
        QVERIFY(fresh >= 0);
        QTRY_COMPARE(ownership->mark(fresh).owner, b.key.fingerprint());
        QVERIFY(ownership->mark(0).owner.isEmpty());
        QCOMPARE(ownership->listenersOf(0), QList<QByteArray>{b.key.fingerprint()});
        // Its changes are refused while nobody controls it, in words that
        // name the slice as every other refusal does.
        const QJsonObject change = core.invoke(appB, "nnr.resetTuning", {int64("sliceId", 0)});
        QVERIFY(!accepted(change));
        const QString nobody =
            QStringLiteral("Nobody controls slice A. Take control to change it.");
        QVERIFY(OperatorWording::isPlain(nobody));
        QCOMPARE(reasonOf(change), nobody);

        // It can take it, with the revision it saw.
        QTRY_VERIFY(seenBy(appB, 0).revision == static_cast<qint64>(ownership->controlRevision(0)));
        r = core.invoke(appB, "slice.takeControl", revisionArgs(seenBy(appB, 0)));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        QCOMPARE(ownership->mark(0).owner, b.key.fingerprint());
    }

    void aReleasedSliceNobodyElseIsOnCloses()
    {
        Core core;
        core.model->configureStreamPool(5, 5, 192000);
        core.model->sliceById(0)->setFrequency(14200000.0);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kShares);
        QVERIFY(admitted(appA));
        QVERIFY(accepted(core.invoke(appA, "addSlice", {utf8("initialPanId", QString())})));
        core.model->sliceById(0)->setFrequency(7074000.0);
        const int streamOfA0 = core.model->sliceById(0)->streamIndex();
        QVERIFY(streamOfA0 >= 0);
        QVERIFY(core.model->sliceById(1)->streamIndex() != streamOfA0);
        QTRY_VERIFY(holds(appA, accessKey(0)));

        const QJsonObject r = core.invoke(appA, "slice.release", revisionArgs(seenBy(appA, 0)));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        QVERIFY(core.model->sliceById(0) == nullptr);
        QVERIFY(!core.model->streamAllocator().isStreamActive(streamOfA0));
        QTRY_VERIFY(!holds(appA, accessKey(0)));
        QVERIFY(!holds(appA, QStringLiteral("slice:0")));
    }

    // Slice control fix wave (Important 1): the Core's last slice stays when
    // released, so a release of it is a hand-off to nobody and is refused
    // while it transmits, as a release kept for listeners is.
    void releasingTheLastSliceIsRefusedWhileItTransmits()
    {
        Core core;
        allowTransmit(core);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kSharesTx);
        QVERIFY(admitted(appA));
        QCOMPARE(core.model->slices().size(), 1);
        const SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownership->mark(0).owner, a.key.fingerprint());
        MoxController* mox = core.model->moxController();
        mox->setMox(true, keyerFor(a));
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        QTRY_VERIFY(holds(appA, accessKey(0)));

        const Seen seen = seenBy(appA, 0);
        QJsonObject r = core.invoke(appA, "slice.release", revisionArgs(seen));
        QVERIFY(!accepted(r));
        QCOMPARE(reasonOf(r), QStringLiteral("Slice A is transmitting. Release it once it stops."));
        QCOMPARE(ownership->mark(0).owner, a.key.fingerprint());
        QVERIFY(core.server->transmitHolder()->isHeldBy(a.key.fingerprint()));
        QVERIFY(mox->isMox());

        mox->setMox(false, keyerFor(a));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        r = core.invoke(appA, "slice.release", revisionArgs(seen));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        // Slice control plan Task 7: nobody is on it, so it closes, the
        // Core's last slice included.
        QVERIFY(core.model->sliceById(0) == nullptr);
        QVERIFY(core.model->slices().isEmpty());
    }

    // Slice control fix wave (whole-branch review, Critical 1): while the
    // radio's own PTT keys a slice (ruling 8.11), a device can neither
    // release it nor stop listening to it. Before, the release closed the
    // keyed slice and MOX dropped by list repair.
    void theRadiosKeyedSliceCannotBeReleasedOrLeft()
    {
        Core core;
        allowTransmit(core);
        core.model->configureStreamPool(5, 5, 192000);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kSharesTx);
        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appA) && admitted(appB));
        SliceOwnership* ownership = core.model->sliceOwnership();
        QVERIFY(accepted(core.invoke(appA, "addSlice", {utf8("initialPanId", QString())})));
        QCOMPARE(ownership->ownedBy(a.key.fingerprint()).size(), 2);
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        const int keyed = arbiter->txBoundSliceId();
        QVERIFY(ownership->ownedBy(a.key.fingerprint()).contains(keyed));
        core.model->sliceById(keyed)->setDspMode(DSPMode::USB);
        core.model->sliceById(keyed)->setFrequency(14200000.0);
        QTRY_VERIFY(holds(appB, accessKey(keyed)));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refArgs(seenBy(appB, keyed)))));

        MoxController* mox = core.model->moxController();
        mox->onMicPttFromRadio(true);
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        QTRY_VERIFY(holds(appA, accessKey(keyed)));

        const QString onAir = QStringLiteral("The radio is on the air. Try again when it stops.");
        QJsonObject r = core.invoke(appA, "slice.release", revisionArgs(seenBy(appA, keyed)));
        QVERIFY(!accepted(r));
        QCOMPARE(reasonOf(r), onAir);
        r = core.invoke(appB, "slice.stopListening", refArgs(seenBy(appB, keyed)));
        QVERIFY(!accepted(r));
        QCOMPARE(reasonOf(r), onAir);
        QVERIFY(core.model->sliceById(keyed) != nullptr);
        QCOMPARE(ownership->mark(keyed).owner, a.key.fingerprint());
        QVERIFY(ownership->isListening(b.key.fingerprint(), keyed));
        QVERIFY(mox->isMox());
        QCOMPARE(arbiter->txBoundSliceId(), keyed);

        mox->onMicPttFromRadio(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        r = core.invoke(appA, "slice.release", revisionArgs(seenBy(appA, keyed)));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
    }

    // Critical 1: the last one on a keyed slice leaves it. The leave stands;
    // the slice closes only once the radio is unkeyed, never under the key.
    void aKeyedSliceLeftByItsLastListenerClosesOnceUnkeyed()
    {
        Core core;
        allowTransmit(core);
        core.model->configureStreamPool(5, 5, 192000);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kSharesTx);
        QVERIFY(admitted(appA));
        QCOMPARE(core.model->slices().size(), 1);
        SliceOwnership* ownership = core.model->sliceOwnership();
        const int only = ownership->ownedBy(a.key.fingerprint()).first();
        core.model->sliceById(only)->setDspMode(DSPMode::USB);
        core.model->sliceById(only)->setFrequency(14200000.0);
        SliceAccessController* access = core.server->sliceAccessController();
        const QByteArray& station = SliceOwnership::stationDevice();
        QVERIFY(access->listen(station, ownership->refOf(only)).accepted);
        QTRY_VERIFY(holds(appA, accessKey(only)));
        QJsonObject r = core.invoke(appA, "slice.release", revisionArgs(seenBy(appA, only)));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        QVERIFY(ownership->mark(only).owner.isEmpty());
        QCOMPARE(ownership->listenersOf(only), QList<QByteArray>{station});
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        QCOMPARE(arbiter->txBoundSliceId(), only);
        QList<bool> keyedAtUnbind;
        MoxController* mox = core.model->moxController();
        connect(arbiter, &TxSliceArbiter::txBoundSliceChanged, arbiter,
                [&keyedAtUnbind, mox](int, int now) {
                    if (now == -1) {
                        keyedAtUnbind.append(mox->isMox() || mox->state() != MoxState::Rx);
                    }
                });

        mox->onMicPttFromRadio(true);
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        const SliceAccessController::Result left =
            access->stopListening(station, ownership->refOf(only));
        QVERIFY2(left.accepted, qPrintable(left.reason));
        QVERIFY(!ownership->isListening(station, only));
        QTest::qWait(50);
        QVERIFY(core.model->sliceById(only) != nullptr);
        QVERIFY(mox->isMox());
        QCOMPARE(arbiter->txBoundSliceId(), only);

        mox->onMicPttFromRadio(false);
        QTRY_VERIFY(core.model->slices().isEmpty());
        QCOMPARE(mox->state(), MoxState::Rx);
        QCOMPARE(keyedAtUnbind, QList<bool>({false}));
    }

    // Critical 1: a slice someone joins again before the key ends stays.
    void aDeferredCloseIsDroppedWhenSomeoneComesBack()
    {
        Core core;
        allowTransmit(core);
        core.model->configureStreamPool(5, 5, 192000);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kSharesTx);
        QVERIFY(admitted(appA));
        SliceOwnership* ownership = core.model->sliceOwnership();
        const int only = ownership->ownedBy(a.key.fingerprint()).first();
        core.model->sliceById(only)->setDspMode(DSPMode::USB);
        core.model->sliceById(only)->setFrequency(14200000.0);
        SliceAccessController* access = core.server->sliceAccessController();
        const QByteArray& station = SliceOwnership::stationDevice();
        QVERIFY(access->listen(station, ownership->refOf(only)).accepted);
        QTRY_VERIFY(holds(appA, accessKey(only)));
        QVERIFY(accepted(core.invoke(appA, "slice.release", revisionArgs(seenBy(appA, only)))));
        MoxController* mox = core.model->moxController();
        mox->onMicPttFromRadio(true);
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        QVERIFY(access->stopListening(station, ownership->refOf(only)).accepted);
        QVERIFY(access->listen(station, ownership->refOf(only)).accepted);

        mox->onMicPttFromRadio(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QTest::qWait(50);
        QVERIFY(core.model->sliceById(only) != nullptr);
        QVERIFY(ownership->isListening(station, only));
    }

    // Minor 3: the Core's own window moves transmit only onto a slice it
    // controls or one nobody is on, the check the remote tx.setTxSlice
    // makes. Another device's slice is refused and nothing moves.
    void theLocalWindowCannotMoveTransmitOntoAnotherDevicesSlice()
    {
        Core core;
        allowTransmit(core);
        core.model->configureStreamPool(5, 5, 192000);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kSharesTx);
        QVERIFY(admitted(appA));
        SliceOwnership* ownership = core.model->sliceOwnership();
        QVERIFY(accepted(core.invoke(appA, "addSlice", {utf8("initialPanId", QString())})));
        const QList<int> owned = ownership->ownedBy(a.key.fingerprint());
        QCOMPARE(owned.size(), 2);
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        const int bound = arbiter->txBoundSliceId();
        const int other = owned.first() == bound ? owned.last() : owned.first();
        QSignalSpy selected(core.model.get(), &RadioModel::txSliceSelected);

        QVERIFY(!core.model->requestTxHandoffToSlice(other));
        QCOMPARE(arbiter->txBoundSliceId(), bound);
        QCOMPARE(selected.count(), 0);

        // A slice nobody is on is the window's to choose.
        const int free = core.model->addSlice();
        QVERIFY(free >= 0);
        QVERIFY(ownership->unclaimed().contains(free));
        QVERIFY(core.model->requestTxHandoffToSlice(free));
        QCOMPARE(arbiter->txBoundSliceId(), free);
    }

    // Slice control plan Task 7: zero slices is a valid idle Core. The only
    // slice released with nobody listening closes; nothing is active, bound
    // for transmit or streaming, every key is refused in plain words and
    // nothing keys; a new slice from zero works and reaches the window.
    void releasingTheOnlySliceWithNobodyOnItLeavesAnIdleCore()
    {
        Core core;
        allowTransmit(core);
        core.model->configureStreamPool(5, 5, 192000);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kSharesTx);
        QVERIFY(admitted(appA));
        QCOMPARE(core.model->slices().size(), 1);
        const int only = core.model->slices().first()->sliceIndex();
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        QCOMPARE(arbiter->txBoundSliceId(), only);
        QTRY_VERIFY(holds(appA, accessKey(only)));
        const int from = appA->received().size();

        const QJsonObject r =
            core.invoke(appA, "slice.release", revisionArgs(seenBy(appA, only)));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        QVERIFY(core.model->slices().isEmpty());
        QVERIFY(core.model->activeSlice() == nullptr);
        QCOMPARE(arbiter->txBoundSliceId(), -1);
        QVERIFY(!core.model->hasTransmitSlice());
        QCOMPARE(activeStreamMask(*core.model), 0u);
        QTRY_VERIFY(indexOf(appA, from, QStringLiteral("object.destroy"),
                            QStringLiteral("slice:%1").arg(only)) >= 0);

        MoxController* mox = core.model->moxController();
        QSignalSpy refused(mox, &MoxController::moxRefused);
        QSignalSpy moxChanged(mox, &MoxController::moxChanged);
        mox->setMox(true, keyerFor(a));
        QTRY_COMPARE(refused.count(), 1);
        const TxRefusal refusal = refused.first().at(0).value<TxRefusal>();
        QCOMPARE(refusal.code, QByteArray(TxRefusals::kNoTransmitSlice));
        QCOMPARE(refusal.text, QStringLiteral("There is no slice to transmit on. Add a slice first."));
        QVERIFY(OperatorWording::isPlain(refusal.text));
        QVERIFY(!mox->isMox());
        QCOMPARE(mox->state(), MoxState::Rx);
        QCOMPARE(moxChanged.count(), 0);

        const int before = appA->received().size();
        QVERIFY(accepted(core.invoke(appA, "addSlice", {utf8("initialPanId", QString())})));
        QCOMPARE(core.model->slices().size(), 1);
        SliceModel* fresh = core.model->slices().first();
        QVERIFY(fresh->streamIndex() >= 0);
        QVERIFY(activeStreamMask(*core.model) != 0u);
        QCOMPARE(arbiter->txBoundSliceId(), fresh->sliceIndex());
        QVERIFY(core.model->hasTransmitSlice());
        QCOMPARE(core.model->sliceOwnership()->mark(fresh->sliceIndex()).owner,
                 a.key.fingerprint());
        QTRY_VERIFY(indexOf(appA, before, QStringLiteral("object.create"),
                            QStringLiteral("slice:%1").arg(fresh->sliceIndex())) >= 0);
    }

    // Slice control plan Task 7: a device's removeSlice of the Core's last
    // slice with nobody else on it closes it (a release that leaves nobody).
    void removingTheCoresLastSliceClosesIt()
    {
        Core core;
        core.model->configureStreamPool(5, 5, 192000);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kShares);
        QVERIFY(admitted(appA));
        QCOMPARE(core.model->slices().size(), 1);
        const int only = core.model->slices().first()->sliceIndex();
        const QJsonObject r = core.invoke(appA, "removeSlice", {int64("sliceId", only)});
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        QVERIFY(core.model->slices().isEmpty());
    }

    void aControllersCloseOfASharedSliceReleasesIt()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kShares);
        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appA) && admitted(appB));
        QTRY_VERIFY(holds(appB, accessKey(0)));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refArgs(seenBy(appB, 0)))));
        const int slices = core.model->slices().size();
        const QJsonObject r = core.invoke(appA, "removeSlice", {int64("sliceId", 0)});
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        QCOMPARE(core.model->slices().size(), slices);
        QVERIFY(core.model->sliceOwnership()->mark(0).owner.isEmpty());
        QCOMPARE(core.model->sliceOwnership()->listenersOf(0),
                 QList<QByteArray>{b.key.fingerprint()});
        // A listener's close is refused as any change is.
        const QJsonObject refused = core.invoke(appB, "removeSlice", {int64("sliceId", 0)});
        QVERIFY(!accepted(refused));
        QCOMPARE(core.model->slices().size(), slices);
    }

    // Slice control plan Task 6 (rulings Q3, Q4, and JJ's AF ruling): a
    // listener's level starts at the slice's AF and is its own after that;
    // the controller's AF and mute stay as they were. A device that does
    // not listen, an old incarnation and an unreadable level are refused
    // and change nothing.
    void aListenerSetsItsOwnLevelAndTheControllersAfStays()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        Device c(QStringLiteral("Laptop"), QStringLiteral("computer"));
        core.pair(a);
        core.pair(b);
        core.pair(c);
        LoopbackTransport* appA = core.signIn(a, kShares);
        LoopbackTransport* appB = core.signIn(b, kShares);
        LoopbackTransport* appC = core.signIn(c, kShares);
        QVERIFY(admitted(appA) && admitted(appB) && admitted(appC));
        QTRY_VERIFY(holds(appB, accessKey(0)));
        QTRY_VERIFY(holds(appC, accessKey(0)));
        SliceModel* const slice = core.model->sliceById(0);
        slice->setAfGain(40);
        SliceAccessController* const access = core.server->sliceAccessController();
        QVERIFY(access != nullptr);

        QVERIFY(accepted(core.invoke(appB, "slice.listen", refArgs(seenBy(appB, 0)))));
        QCOMPARE(access->listenLevel(b.key.fingerprint(), 0).level, 0.4);
        QCOMPARE(access->listenLevel(b.key.fingerprint(), 0).muted, false);

        const Seen seen = seenBy(appB, 0);
        QJsonObject r = core.invoke(appB, "slice.setListenLevel", levelArgs(seen, 0.25, true));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        QCOMPARE(access->listenLevel(b.key.fingerprint(), 0).level, 0.25);
        QCOMPARE(access->listenLevel(b.key.fingerprint(), 0).muted, true);
        QCOMPARE(slice->afGain(), 40);
        QCOMPARE(slice->muted(), false);
        // The controller's AF moves; the listener's level does not.
        slice->setAfGain(70);
        QCOMPARE(access->listenLevel(b.key.fingerprint(), 0).level, 0.25);

        // Not listening.
        r = core.invoke(appC, "slice.setListenLevel", levelArgs(seenBy(appC, 0), 0.5, false));
        QVERIFY(!accepted(r));
        const QString notListening =
            QStringLiteral("You are not listening to slice A. Listen in first.");
        QVERIFY(OperatorWording::isPlain(notListening));
        QCOMPARE(reasonOf(r), notListening);
        // An old incarnation.
        Seen stale = seen;
        stale.incarnation += 1;
        r = core.invoke(appB, "slice.setListenLevel", levelArgs(stale, 0.5, false));
        QVERIFY(!accepted(r));
        QCOMPARE(reasonOf(r),
                 QStringLiteral("That slice has closed. Choose it again from the list."));
        // A level outside 0..1.
        const QString unread = QStringLiteral("The Core could not read this request.");
        for (double bad : {1.5, -0.1, std::numeric_limits<double>::quiet_NaN()}) {
            r = core.invoke(appB, "slice.setListenLevel", levelArgs(seen, bad, false));
            QVERIFY(!accepted(r));
            QCOMPARE(reasonOf(r), unread);
        }
        // A level sent as an integer.
        r = core.invoke(appB, "slice.setListenLevel",
                        {int64("sliceId", 0), int64("incarnation", seen.incarnation),
                         int64("level", 1),
                         MirrorUpdate{0, QByteArrayLiteral("muted"), MirrorWireKind::Bool,
                                      QVariant(false)}});
        QVERIFY(!accepted(r));
        QCOMPARE(reasonOf(r), unread);
        QCOMPARE(access->listenLevel(b.key.fingerprint(), 0).level, 0.25);
        QCOMPARE(access->listenLevel(b.key.fingerprint(), 0).muted, true);
        QCOMPARE(slice->afGain(), 70);
    }

    // Ruling Q4: a controller that hands the slice over keeps hearing it at
    // the AF level it had.
    void theFormerControllerListensAtTheAfItHad()
    {
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kShares);
        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appA) && admitted(appB));
        QTRY_VERIFY(holds(appB, accessKey(0)));
        SliceModel* const slice = core.model->sliceById(0);
        slice->setAfGain(60);
        SliceAccessController* const access = core.server->sliceAccessController();
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refArgs(seenBy(appB, 0)))));
        QVERIFY(accepted(core.invoke(appB, "slice.setListenLevel",
                                     levelArgs(seenBy(appB, 0), 0.1, false))));
        const QJsonObject r =
            core.invoke(appB, "slice.takeControl", revisionArgs(seenBy(appB, 0)));
        QVERIFY2(accepted(r), qPrintable(reasonOf(r)));
        QCOMPARE(access->listenLevel(a.key.fingerprint(), 0).level, 0.6);
        QCOMPARE(access->listenLevel(a.key.fingerprint(), 0).muted, false);
        // The former controller sets its own level now.
        QTRY_VERIFY(seenBy(appA, 0).revision == seenBy(appB, 0).revision);
        QVERIFY(accepted(core.invoke(appA, "slice.setListenLevel",
                                     levelArgs(seenBy(appA, 0), 0.3, false))));
        QCOMPARE(access->listenLevel(a.key.fingerprint(), 0).level, 0.3);
        QCOMPARE(slice->afGain(), 60);
    }
};

QTEST_MAIN(TstSliceAccessVerbs)
#include "tst_slice_access_verbs.moc"
