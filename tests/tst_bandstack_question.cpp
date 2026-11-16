// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_bandstack_question.cpp  (NereusSDR)
// =================================================================
//
// A band stack recall on a shared receiver (design sections 5.6 and 6.3): the
// anchor's press is held and asked through the panMove question with nothing
// saved, cancel and expiry change nothing, proceed works the target out again,
// a device that does not anchor takes a free receiver, every receiver in use
// goes to the take question, and a frozen transmit slice refuses at invoke and
// at proceed.
//
// Loopback link with fake peers, no RF and no hardware. "On the air" is the
// radio's own PTT in MoxController; nothing reaches a radio.
//
//   cmake --build build --target tst_bandstack_question
//   ctest --test-dir build -R '^tst_bandstack_question$' --output-on-failure
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include <QLoggingCategory>

#include "core/session/BandStackFacade.h"
#include "models/Band.h"
#include "models/bandstack/BandStackController.h"

namespace {

const QString kWaiting = QStringLiteral("Waiting for you to confirm.");
const QString kChanged = QStringLiteral("What this change reaches has changed. Make the change again.");
const QString kExpired = QStringLiteral("That question has expired. Make the change again.");
const QString kRadioOnAir = QStringLiteral("The radio is on the air. Try again when it stops.");
const QString kNotUnderstood = QStringLiteral("The request for the band stack was not understood.");

const QHash<QByteArray, int> kStack{{"deviceAuth", 1}, {"sessionHolder", 1}, {"bandStack", 1}};
// A window from before the confirm step that still knows the band stack.
const QHash<QByteArray, int> kOlderStack{{"deviceAuth", 1}, {"bandStack", 1}};

constexpr double kReceiverHalfHz = 96000.0;

int countOf(const LoopbackTransport* app, const QString& type)
{
    return static_cast<int>(ofType(app->received(), type).size());
}

QJsonObject waitForLast(const LoopbackTransport* app, const QString& type, int after)
{
    const bool arrived = QTest::qWaitFor([&]() { return countOf(app, type) > after; }, 5000);
    Q_UNUSED(arrived);
    const QList<QJsonObject> all = ofType(app->received(), type);
    return all.isEmpty() ? QJsonObject{} : all.last();
}

// The value of `name` in a command.result's `values`, or null.
QJsonValue valueOf(const QJsonObject& result, const QString& name)
{
    for (const QJsonValue& v : result.value(QStringLiteral("values")).toArray()) {
        if (v.toObject().value(QStringLiteral("name")).toString() == name) {
            return v.toObject().value(QStringLiteral("value"));
        }
    }
    return {};
}

QString reasonOf(const QJsonObject& result)
{
    return result.value(QStringLiteral("reason")).toString(QStringLiteral("(no answer)"));
}

// Empty when accepted, otherwise why not.
QString refusalOf(const QJsonObject& result)
{
    return result.value(QStringLiteral("accepted")).toBool(false) ? QString() : reasonOf(result);
}

int streamOf(const Core& core, int sliceId)
{
    const SliceModel* slice = core.model->sliceById(sliceId);
    return slice == nullptr ? -2 : slice->streamIndex();
}

QJsonObject coreDocument(const Core& core)
{
    return QJsonDocument::fromJson(core.model->bandStackFacade()->json().toUtf8()).object();
}

QStringList orderOn(const QJsonObject& document, const QString& band)
{
    QStringList ids;
    const QJsonArray order = document.value(QStringLiteral("bands")).toObject().value(band).toObject()
                                 .value(QStringLiteral("order")).toArray();
    for (const QJsonValue& id : order) {
        ids.append(id.toString());
    }
    return ids;
}

qint64 hzOf(const QJsonObject& document, const QString& id)
{
    for (const QJsonValue& value : document.value(QStringLiteral("entries")).toArray()) {
        if (value.toObject().value(QStringLiteral("id")).toString() == id) {
            return value.toObject().value(QStringLiteral("frequencyHz")).toInteger();
        }
    }
    return -1;
}

QString placeOf(const QJsonObject& document, const QString& letter, const QString& band)
{
    return document.value(QStringLiteral("places")).toObject().value(letter).toObject()
        .value(band).toString();
}

QList<MirrorUpdate> pressArguments(int sliceId, const QString& band)
{
    return {int64("sliceId", sliceId), utf8("band", band), utf8("direction", QStringLiteral("next"))};
}

// A (the iPhone) anchors a receiver with slice 0 on 7.074 MHz, placed on the
// first 40m entry. B (the iPad) has slice 1 on 7.150 MHz in A's window. A second
// receiver is free. Receivers are 192 kHz wide. All on a running band stack.
struct Shared {
    Core core;
    Device a;
    Device b{QStringLiteral("iPad"), QStringLiteral("tablet")};
    Device c{QStringLiteral("Mac"), QStringLiteral("computer")};
    LoopbackTransport* appA = nullptr;
    LoopbackTransport* appB = nullptr;
    QString firstEntry;

    explicit Shared(const QHash<QByteArray, int>& aFeatures = kStack)
    {
        RadioInfo info;
        info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:01");
        info.name = QStringLiteral("Bench HL2");
        info.boardType = HPSDRHW::HermesLite;
        core.model->announceRadioForTest(info);
        core.model->configureStreamPool(2, 5, 192000);

        firstEntry = orderOn(coreDocument(core), QStringLiteral("40m")).first();
        BandStackRecallRequest goTo;
        goTo.kind = BandStackRecallRequest::GoTo;
        goTo.sliceId = 0;
        goTo.entryId = firstEntry;
        QString refusal;
        const bool placed = controller()->performRecall(goTo, &refusal);
        Q_ASSERT(placed);
        Q_UNUSED(placed);
        core.model->sliceById(0)->setFrequency(7074000.0);

        core.pair(a);
        core.pair(b);
        core.pair(c);
        appA = core.signIn(a, aFeatures);
        appB = core.signIn(b, kStack);
        core.model->sliceById(1)->setFrequency(7150000.0);
    }

    BandStackController* controller() const { return core.model->bandStackController(); }
    int receiver() const { return streamOf(core, 0); }
    double centre() const { return core.model->streamAllocator().streamCentreHz(receiver()); }
    QString json() const { return core.model->bandStackFacade()->json(); }

    QString entryAt(const QString& band, qsizetype index) const
    {
        return orderOn(coreDocument(core), band).value(index);
    }

    // Where a press of `band` on `sliceId` would go now.
    qint64 pressTarget(int sliceId, const QString& band) const
    {
        BandStackRecallRequest request;
        request.sliceId = sliceId;
        request.band = *BandStackStore::bandFromKey(band);
        const BandStackRecallPlan plan = controller()->planRecall(request);
        return plan.valid ? plan.frequencyHz : -1;
    }

    bool insideWindow(qint64 hz) const { return qAbs(double(hz) - 7074000.0) < kReceiverHalfHz; }

    // Reorders 40m so `next` follows the first entry.
    void put40mNext(const QString& next) const
    {
        QStringList order = orderOn(coreDocument(core), QStringLiteral("40m"));
        order.removeOne(next);
        order.insert(1, next);
        QString refusal;
        const bool done = controller()->reorder(*BandStackStore::bandFromKey(QStringLiteral("40m")),
                                                order, &refusal);
        Q_ASSERT(done);
        Q_UNUSED(done);
    }

    // A 40m entry inside A's window, other than the first.
    QString near40m() const
    {
        const QJsonObject document = coreDocument(core);
        for (const QString& id : orderOn(document, QStringLiteral("40m"))) {
            if (id != firstEntry && insideWindow(hzOf(document, id))) {
                return id;
            }
        }
        return {};
    }

    // A 40m entry outside A's window whose own window would not cover B's slice,
    // so the receiver following it leaves B behind.
    QString far40m() const
    {
        const QJsonObject document = coreDocument(core);
        for (const QString& id : orderOn(document, QStringLiteral("40m"))) {
            const qint64 hz = hzOf(document, id);
            if (!insideWindow(hz) && qAbs(double(hz) - 7150000.0) >= kReceiverHalfHz) {
                return id;
            }
        }
        return {};
    }

    // C signs in and takes the other receiver with its slice on 20 m.
    LoopbackTransport* cOnTheOtherReceiver()
    {
        LoopbackTransport* appC = core.signIn(c, kStack);
        const QList<int> own = core.model->sliceOwnership()->ownedBy(c.key.fingerprint());
        if (!own.isEmpty()) {
            core.model->sliceById(own.first())->setFrequency(14074000.0);
        }
        return appC;
    }

    QJsonObject press(LoopbackTransport* app, int sliceId, const QString& band)
    {
        return core.invoke(app, "bandStack.press", pressArguments(sliceId, band));
    }

    QJsonObject proceed(LoopbackTransport* app, qint64 id, qint64 choice = -1)
    {
        return core.invoke(app, "confirm.proceed", {int64("id", id), int64("choice", choice)});
    }

    // The radio's own PTT on `sliceId`, keyed at once with no PC microphone.
    void prepareKey(int sliceId)
    {
        core.server->setRemoteTransmitAllowed(true);
        core.model->moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        core.model->transmitModel().setMicSourceLocked(false);
        core.model->transmitModel().setMicSource(MicSource::Radio);
        core.model->sliceById(sliceId)->setDspMode(DSPMode::LSB);
        QVERIFY(core.model->txSliceArbiter()->requestHandoff(sliceId));
    }
};

} // namespace

class TstBandStackQuestion : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(
            QStringLiteral("bandstack-question-%1").arg(QCoreApplication::applicationPid()));
        // As tst_bandstack_link: a tune on a model with no radio logs per message,
        // and the harness turns its heartbeat off on purpose.
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false\n"
                                                        "nereus.*.info=false\n"
                                                        "nereussdr.*.info=false\n"
                                                        "nereus.connection.warning=false\n"
                                                        "nereus.dsp.warning=false\n"
                                                        "nereus.station.warning=false\n"
                                                        "nereus.stationclient.warning=false"));
    }

    void init()
    {
        // Each case starts with no saved stack and no band memory.
        AppSettings::instance().clear();
        AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // ── Held and asked ───────────────────────────────────────────────────

    void theAnchorsPressIsHeldAndChangesNothing()
    {
        Shared s;
        const int receiver = s.receiver();
        const double centre = s.centre();
        const QString before = s.json();
        const qint64 target = s.pressTarget(0, QStringLiteral("20m"));
        QVERIFY(!s.insideWindow(target));

        const QJsonObject held = s.press(s.appA, 0, QStringLiteral("20m"));
        QCOMPARE(held.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(reasonOf(held), kWaiting);
        QCOMPARE(valueOf(held, QStringLiteral("phase")).toString(),
                 QStringLiteral("needsConfirmation"));

        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("panMove"));
        QCOMPARE(ask.value(QStringLiteral("forCommandId")).toInteger(),
                 held.value(QStringLiteral("id")).toInteger());
        QCOMPARE(ask.value(QStringLiteral("expiresInMs")).toInteger(), 60000);
        const QJsonObject change = ask.value(QStringLiteral("change")).toObject();
        QCOMPARE(change.value(QStringLiteral("label")).toString(),
                 QStringLiteral("Receiver %1").arg(receiver + 1));
        QCOMPARE(change.value(QStringLiteral("from")).toString(), QStringLiteral("40 m"));
        QCOMPARE(change.value(QStringLiteral("to")).toString(), QStringLiteral("20 m"));
        const QJsonObject who = ask.value(QStringLiteral("affected")).toArray().first().toObject();
        QCOMPARE(who.value(QStringLiteral("deviceId")).toString(), s.b.id());
        QCOMPARE(who.value(QStringLiteral("slices")).toArray().first().toObject()
                     .value(QStringLiteral("effect")).toString(),
                 QStringLiteral("moves"));

        // The store and the slices, the spot left included.
        QCOMPARE(s.json(), before);
        QCOMPARE(s.core.model->sliceById(0)->frequency(), 7074000.0);
        QVERIFY(hzOf(coreDocument(s.core), s.firstEntry) != 7074000);
        QCOMPARE(s.centre(), centre);
        QCOMPARE(streamOf(s.core, 1), receiver);
        QCOMPARE(countOf(s.appB, QStringLiteral("notice")), 0);
    }

    void stepAndGoToAreHeldToo()
    {
        Shared s;
        const QString before = s.json();
        const QString far = s.far40m();
        QVERIFY(!far.isEmpty());
        s.put40mNext(far);
        const QString reordered = s.json();
        QVERIFY(reordered != before);

        const QJsonObject step = s.core.invoke(
            s.appA, "bandStack.step", {int64("sliceId", 0), utf8("direction", QStringLiteral("next"))});
        QCOMPARE(reasonOf(step), kWaiting);
        QCOMPARE(waitForLast(s.appA, QStringLiteral("confirm.request"), 0)
                     .value(QStringLiteral("kind")).toString(),
                 QStringLiteral("panMove"));

        const QString entry = s.entryAt(QStringLiteral("20m"), 0);
        const QJsonObject goTo = s.core.invoke(s.appA, "bandStack.goTo",
                                               {int64("sliceId", 0), utf8("entryId", entry)});
        QCOMPARE(reasonOf(goTo), kWaiting);
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 1);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("panMove"));
        QCOMPARE(s.json(), reordered);

        const QJsonObject done = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(refusalOf(done), QString());
        QCOMPARE(qRound64(s.core.model->sliceById(0)->frequency()),
                 hzOf(coreDocument(s.core), entry));
    }

    void cancelChangesNothing()
    {
        Shared s;
        const double centre = s.centre();
        const QString before = s.json();
        s.press(s.appA, 0, QStringLiteral("20m"));
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        const qint64 id = ask.value(QStringLiteral("id")).toInteger();
        QCOMPARE(refusalOf(s.core.invoke(s.appA, "confirm.cancel", {int64("id", id)})), QString());
        QCOMPARE(s.json(), before);
        QCOMPARE(s.core.model->sliceById(0)->frequency(), 7074000.0);
        QCOMPARE(s.centre(), centre);
        QCOMPARE(streamOf(s.core, 1), s.receiver());
        QCOMPARE(countOf(s.appB, QStringLiteral("notice")), 0);
        // And it cannot be proceeded later.
        QVERIFY(!refusalOf(s.proceed(s.appA, id)).isEmpty());
        QCOMPARE(s.json(), before);
        QCOMPARE(s.core.model->sliceById(0)->frequency(), 7074000.0);
    }

    void expiryChangesNothing()
    {
        Shared s;
        const double centre = s.centre();
        const QString before = s.json();
        s.core.now = 1000;
        s.press(s.appA, 0, QStringLiteral("20m"));
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        s.core.now = 1000 + ConfirmStep::kExpiryMs;
        const QJsonObject late = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(reasonOf(late), kExpired);
        QCOMPARE(s.json(), before);
        QCOMPARE(s.core.model->sliceById(0)->frequency(), 7074000.0);
        QCOMPARE(s.centre(), centre);
        QCOMPARE(streamOf(s.core, 1), s.receiver());
    }

    void proceedRunsThePressMovesTheOtherAndSaves()
    {
        Shared s;
        const int receiver = s.receiver();
        const qint64 target = s.pressTarget(0, QStringLiteral("20m"));
        s.press(s.appA, 0, QStringLiteral("20m"));
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        const QJsonObject done = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(refusalOf(done), QString());
        const QJsonArray affected = done.value(QStringLiteral("affected")).toArray();
        QVERIFY(affected.contains(QStringLiteral("bandStack")));
        QVERIFY(affected.contains(QStringLiteral("slice:0")));

        // The press ran, and the receiver followed it.
        QCOMPARE(qRound64(s.core.model->sliceById(0)->frequency()), target);
        QCOMPARE(streamOf(s.core, 0), receiver);
        QCOMPARE(s.centre(), double(target));
        // B moved, and was told as any pan move tells it.
        QVERIFY(streamOf(s.core, 1) != receiver);
        QCOMPARE(s.core.model->sliceById(1)->frequency(), 7150000.0);
        const QJsonObject told = waitForLast(s.appB, QStringLiteral("notice"), 0);
        QCOMPARE(told.value(QStringLiteral("kind")).toString(), QStringLiteral("sliceMoved"));
        // It saved as normal: the spot left, and the new place.
        const QJsonObject document = coreDocument(s.core);
        QCOMPARE(hzOf(document, s.firstEntry), qint64(7074000));
        const QString place = placeOf(document, QStringLiteral("A"), QStringLiteral("20m"));
        QVERIFY(!place.isEmpty());
        QCOMPARE(hzOf(document, place), target);
    }

    // ── The target is worked out again at proceed ────────────────────────

    void anEntryChangedDuringTheQuestionUsesTheNewTarget()
    {
        Shared s;
        const qint64 target = s.pressTarget(0, QStringLiteral("20m"));
        s.press(s.appA, 0, QStringLiteral("20m"));
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);

        // Another entry on 20m becomes where the band returns.
        const QJsonObject document = coreDocument(s.core);
        QString other;
        for (const QString& id : orderOn(document, QStringLiteral("20m"))) {
            if (hzOf(document, id) != target) {
                other = id;
                break;
            }
        }
        QVERIFY(!other.isEmpty());
        QString refusal;
        QVERIFY2(s.controller()->setReturn(*BandStackStore::bandFromKey(QStringLiteral("20m")),
                                           BandStackReturnMode::Specific, other, &refusal),
                 qPrintable(refusal));
        const qint64 moved = hzOf(document, other);
        QCOMPARE(s.pressTarget(0, QStringLiteral("20m")), moved);

        QCOMPARE(refusalOf(s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger())), QString());
        QCOMPARE(qRound64(s.core.model->sliceById(0)->frequency()), moved);
        QCOMPARE(s.centre(), double(moved));
    }

    void aTargetNoLongerValidIsRefusedAsChanged()
    {
        Shared s;
        const double centre = s.centre();
        const QString entry = s.entryAt(QStringLiteral("20m"), 0);
        s.core.invoke(s.appA, "bandStack.goTo", {int64("sliceId", 0), utf8("entryId", entry)});
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QString refusal;
        QVERIFY2(s.controller()->remove(entry, &refusal), qPrintable(refusal));
        const QString before = s.json();

        QCOMPARE(reasonOf(s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger())), kChanged);
        QCOMPARE(s.json(), before);
        QCOMPARE(s.core.model->sliceById(0)->frequency(), 7074000.0);
        QCOMPARE(s.centre(), centre);
        QCOMPARE(streamOf(s.core, 1), s.receiver());
    }

    void aNewTargetInsideTheWindowRunsAndMovesNobody()
    {
        Shared s;
        const QString far = s.far40m();
        const QString near = s.near40m();
        QVERIFY(!far.isEmpty());
        QVERIFY(!near.isEmpty());
        s.put40mNext(far);
        const int receiver = s.receiver();
        const double centre = s.centre();
        s.press(s.appA, 0, QStringLiteral("40m"));
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("panMove"));

        s.put40mNext(near);
        QCOMPARE(refusalOf(s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger())), QString());
        QCOMPARE(qRound64(s.core.model->sliceById(0)->frequency()),
                 hzOf(coreDocument(s.core), near));
        QCOMPARE(s.centre(), centre);
        QCOMPARE(streamOf(s.core, 1), receiver);
        QCOMPARE(countOf(s.appB, QStringLiteral("notice")), 0);
    }

    void aQuestionThatGrewAsksAgainAndAppliesNothing()
    {
        Shared s;
        const qint64 target = s.pressTarget(0, QStringLiteral("20m"));
        const QJsonObject held = s.press(s.appA, 0, QStringLiteral("20m"));
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("affected")).toArray().first().toObject()
                     .value(QStringLiteral("slices")).toArray().first().toObject()
                     .value(QStringLiteral("effect")).toString(),
                 QStringLiteral("moves"));

        // C takes the free receiver, so B's slice would now close.
        LoopbackTransport* appC = s.cOnTheOtherReceiver();
        QVERIFY(admitted(appC));
        const QString before = s.json();
        const QJsonObject again = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(again.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(reasonOf(again), kWaiting);
        QCOMPARE(valueOf(again, QStringLiteral("phase")).toString(),
                 QStringLiteral("needsConfirmation"));
        QCOMPARE(s.json(), before);
        QCOMPARE(s.core.model->sliceById(0)->frequency(), 7074000.0);
        QVERIFY(s.core.model->sliceById(1) != nullptr);

        const QJsonObject second = waitForLast(s.appA, QStringLiteral("confirm.request"), 1);
        QVERIFY(second.value(QStringLiteral("id")).toInteger()
                != ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(second.value(QStringLiteral("forCommandId")).toInteger(),
                 held.value(QStringLiteral("id")).toInteger());
        const QJsonObject change = second.value(QStringLiteral("change")).toObject();
        QCOMPARE(change.value(QStringLiteral("from")).toString(), QStringLiteral("40 m"));
        QCOMPARE(change.value(QStringLiteral("to")).toString(), QStringLiteral("20 m"));
        QCOMPARE(second.value(QStringLiteral("affected")).toArray().first().toObject()
                     .value(QStringLiteral("slices")).toArray().first().toObject()
                     .value(QStringLiteral("effect")).toString(),
                 QStringLiteral("closes"));

        // Confirmed again, the press runs and B's slice closes, and it is told.
        QCOMPARE(refusalOf(s.proceed(s.appA, second.value(QStringLiteral("id")).toInteger())),
                 QString());
        QCOMPARE(qRound64(s.core.model->sliceById(0)->frequency()), target);
        QVERIFY(s.core.model->sliceOwnership()->ownedBy(s.b.key.fingerprint()).isEmpty());
        QCOMPARE(waitForLast(s.appB, QStringLiteral("notice"), 0)
                     .value(QStringLiteral("kind")).toString(),
                 QStringLiteral("sliceClosed"));
    }

    // ── Nobody to ask ────────────────────────────────────────────────────

    void aSliceNobodyOwnsMovesWithoutAQuestion()
    {
        Shared s;
        const int unowned = s.core.model->addSlice(QStringLiteral("pan-0"));
        QVERIFY(unowned >= 0);
        s.core.model->sliceById(unowned)->setFrequency(7100000.0);
        QVERIFY(s.core.model->sliceOwnership()->mark(unowned).subject().isEmpty());
        // B is gone from the receiver, so the unowned slice is the only other.
        s.core.model->sliceById(1)->setFrequency(21074000.0);
        QVERIFY(streamOf(s.core, 1) != s.receiver());
        QCOMPARE(streamOf(s.core, unowned), s.receiver());

        const qint64 target = s.pressTarget(0, QStringLiteral("20m"));
        const QJsonObject done = s.press(s.appA, 0, QStringLiteral("20m"));
        QCOMPARE(refusalOf(done), QString());
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), 0);
        QCOMPARE(qRound64(s.core.model->sliceById(0)->frequency()), target);
        QCOMPARE(s.centre(), double(target));
        QVERIFY(streamOf(s.core, unowned) != s.receiver());
        QCOMPARE(hzOf(coreDocument(s.core), s.firstEntry), qint64(7074000));
    }

    void aDeviceThatDoesNotAnchorTakesAFreeReceiver()
    {
        Shared s;
        const int receiver = s.receiver();
        const double centre = s.centre();
        const qint64 target = s.pressTarget(1, QStringLiteral("20m"));
        const QJsonObject done = s.press(s.appB, 1, QStringLiteral("20m"));
        QCOMPARE(refusalOf(done), QString());
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), 0);
        QCOMPARE(countOf(s.appB, QStringLiteral("confirm.request")), 0);
        QCOMPARE(qRound64(s.core.model->sliceById(1)->frequency()), target);
        QVERIFY(streamOf(s.core, 1) != receiver);
        // The anchor is left alone.
        QCOMPARE(streamOf(s.core, 0), receiver);
        QCOMPARE(s.centre(), centre);
        QCOMPARE(s.core.model->sliceById(0)->frequency(), 7074000.0);
        QCOMPARE(countOf(s.appA, QStringLiteral("notice")), 0);
    }

    void aPressThatStaysInTheWindowAsksNobody()
    {
        Shared s;
        const QString near = s.near40m();
        QVERIFY(!near.isEmpty());
        s.put40mNext(near);
        const double centre = s.centre();
        QCOMPARE(refusalOf(s.press(s.appA, 0, QStringLiteral("40m"))), QString());
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), 0);
        QCOMPARE(qRound64(s.core.model->sliceById(0)->frequency()),
                 hzOf(coreDocument(s.core), near));
        QCOMPARE(s.centre(), centre);
    }

    // ── Every receiver in use ────────────────────────────────────────────

    void everyReceiverInUseRefusesNamingHoldersThenAsksToTake()
    {
        Shared s;
        LoopbackTransport* appC = s.cOnTheOtherReceiver();
        QVERIFY(admitted(appC));
        const int cSlice = s.core.model->sliceOwnership()->ownedBy(s.c.key.fingerprint()).first();
        const int cStream = streamOf(s.core, cSlice);
        const QString before = s.json();
        const qint64 target = s.pressTarget(1, QStringLiteral("15m"));

        const QJsonObject refused = s.press(s.appB, 1, QStringLiteral("15m"));
        QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
        const QString reason = reasonOf(refused);
        QVERIFY2(reason.startsWith(QStringLiteral("Slice B stayed on")), qPrintable(reason));
        QVERIFY2(reason.contains(QStringLiteral("iPhone")) && reason.contains(QStringLiteral("Mac")),
                 qPrintable(reason));
        QCOMPARE(valueOf(refused, QStringLiteral("phase")).toString(), QString());
        QCOMPARE(s.json(), before);

        const QJsonObject ask = waitForLast(s.appB, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("takeReceiver"));
        int choice = -1;
        for (const QJsonValue& v : ask.value(QStringLiteral("choices")).toArray()) {
            if (v.toObject().value(QStringLiteral("streamIndex")).toInt() == cStream) {
                choice = v.toObject().value(QStringLiteral("choice")).toInt();
                QCOMPARE(v.toObject().value(QStringLiteral("takeable")).toBool(false), true);
            }
        }
        QVERIFY(choice >= 0);

        QCOMPARE(refusalOf(s.proceed(s.appB, ask.value(QStringLiteral("id")).toInteger(), choice)),
                 QString());
        QCOMPARE(qRound64(s.core.model->sliceById(1)->frequency()), target);
        QVERIFY(s.core.model->sliceOwnership()->ownedBy(s.c.key.fingerprint()).isEmpty());
        QCOMPARE(waitForLast(appC, QStringLiteral("notice"), 0)
                     .value(QStringLiteral("kind")).toString(),
                 QStringLiteral("receiverTaken"));
        const QString place = placeOf(coreDocument(s.core), QStringLiteral("B"), QStringLiteral("15m"));
        QCOMPARE(hzOf(coreDocument(s.core), place), target);
    }

    void aTakeUsesTheTargetAtProceedAndRefusesOneNoLongerValid()
    {
        // The entry changes while the chooser is up: a new target is used.
        {
            Shared s;
            LoopbackTransport* appC = s.cOnTheOtherReceiver();
            QVERIFY(admitted(appC));
            const int cStream =
                streamOf(s.core, s.core.model->sliceOwnership()->ownedBy(s.c.key.fingerprint()).first());
            const qint64 first = s.pressTarget(1, QStringLiteral("15m"));
            s.press(s.appB, 1, QStringLiteral("15m"));
            const QJsonObject ask = waitForLast(s.appB, QStringLiteral("confirm.request"), 0);
            const QJsonObject document = coreDocument(s.core);
            QString other;
            for (const QString& id : orderOn(document, QStringLiteral("15m"))) {
                if (hzOf(document, id) != first) {
                    other = id;
                    break;
                }
            }
            QVERIFY(!other.isEmpty());
            QString refusal;
            QVERIFY2(s.controller()->setReturn(*BandStackStore::bandFromKey(QStringLiteral("15m")),
                                               BandStackReturnMode::Specific, other, &refusal),
                     qPrintable(refusal));
            int choice = -1;
            for (const QJsonValue& v : ask.value(QStringLiteral("choices")).toArray()) {
                if (v.toObject().value(QStringLiteral("streamIndex")).toInt() == cStream) {
                    choice = v.toObject().value(QStringLiteral("choice")).toInt();
                }
            }
            QCOMPARE(refusalOf(s.proceed(s.appB, ask.value(QStringLiteral("id")).toInteger(), choice)),
                     QString());
            QCOMPARE(qRound64(s.core.model->sliceById(1)->frequency()), hzOf(document, other));
        }
        // The entry goes while the chooser is up: nothing closes.
        {
            Shared s;
            LoopbackTransport* appC = s.cOnTheOtherReceiver();
            QVERIFY(admitted(appC));
            const int cSlice = s.core.model->sliceOwnership()->ownedBy(s.c.key.fingerprint()).first();
            const QString entry = s.entryAt(QStringLiteral("15m"), 0);
            s.core.invoke(s.appB, "bandStack.goTo", {int64("sliceId", 1), utf8("entryId", entry)});
            const QJsonObject ask = waitForLast(s.appB, QStringLiteral("confirm.request"), 0);
            QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("takeReceiver"));
            QString refusal;
            QVERIFY2(s.controller()->remove(entry, &refusal), qPrintable(refusal));
            int choice = -1;
            for (const QJsonValue& v : ask.value(QStringLiteral("choices")).toArray()) {
                if (v.toObject().value(QStringLiteral("streamIndex")).toInt() == streamOf(s.core, cSlice)) {
                    choice = v.toObject().value(QStringLiteral("choice")).toInt();
                }
            }
            QCOMPARE(reasonOf(s.proceed(s.appB, ask.value(QStringLiteral("id")).toInteger(), choice)),
                     kChanged);
            QVERIFY(s.core.model->sliceById(cSlice) != nullptr);
            QCOMPARE(s.core.model->sliceById(1)->frequency(), 7150000.0);
        }
    }

    // ── On the air ───────────────────────────────────────────────────────

    void aFrozenTransmitSliceRefusesAtInvoke()
    {
        Shared s;
        s.prepareKey(1);
        MoxController* mox = s.core.model->moxController();
        mox->onMicPttFromRadio(true);
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        const QString before = s.json();
        const double centre = s.centre();

        // B on its own frozen slice, by each verb.
        QCOMPARE(reasonOf(s.press(s.appB, 1, QStringLiteral("20m"))), kRadioOnAir);
        QCOMPARE(reasonOf(s.core.invoke(s.appB, "bandStack.step",
                                        {int64("sliceId", 1),
                                         utf8("direction", QStringLiteral("next"))})),
                 kRadioOnAir);
        QCOMPARE(reasonOf(s.core.invoke(s.appB, "bandStack.goTo",
                                        {int64("sliceId", 1),
                                         utf8("entryId", s.entryAt(QStringLiteral("20m"), 0))})),
                 kRadioOnAir);
        QCOMPARE(s.core.model->sliceById(1)->frequency(), 7150000.0);

        // A, the anchor, whose press would carry B's frozen slice away.
        const QJsonObject onAir = s.press(s.appA, 0, QStringLiteral("20m"));
        QCOMPARE(reasonOf(onAir), kRadioOnAir);
        QVERIFY(!valueOf(onAir, QStringLiteral("refusalCode")).toString().isEmpty());
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), 0);
        QCOMPARE(s.centre(), centre);
        QCOMPARE(s.json(), before);

        mox->onMicPttFromRadio(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    void aFrozenTransmitSliceRefusesAtProceed()
    {
        Shared s;
        s.prepareKey(0);
        const QString before = s.json();
        const double centre = s.centre();
        s.press(s.appA, 0, QStringLiteral("20m"));
        const QJsonObject ask = waitForLast(s.appA, QStringLiteral("confirm.request"), 0);
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("panMove"));

        MoxController* mox = s.core.model->moxController();
        mox->onMicPttFromRadio(true);
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        const QJsonObject refused = s.proceed(s.appA, ask.value(QStringLiteral("id")).toInteger());
        QCOMPARE(reasonOf(refused), kRadioOnAir);
        QVERIFY(!valueOf(refused, QStringLiteral("refusalCode")).toString().isEmpty());
        QCOMPARE(s.core.model->sliceById(0)->frequency(), 7074000.0);
        QCOMPARE(s.centre(), centre);
        QCOMPARE(streamOf(s.core, 1), s.receiver());
        QCOMPARE(s.json(), before);

        mox->onMicPttFromRadio(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    // ── Refused without asking ───────────────────────────────────────────

    void aWindowWithoutTheHolderVersionIsToldNotAsked()
    {
        Shared s(kOlderStack);
        const QString before = s.json();
        const QJsonObject refused = s.press(s.appA, 0, QStringLiteral("20m"));
        QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(reasonOf(refused),
                 QStringLiteral("This change would affect iPad. Update NereusSDR to confirm changes "
                                "that affect other devices."));
        QCOMPARE(valueOf(refused, QStringLiteral("phase")).toString(), QString());
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), 0);
        QCOMPARE(s.json(), before);
        QCOMPARE(s.core.model->sliceById(0)->frequency(), 7074000.0);
        QCOMPARE(streamOf(s.core, 1), s.receiver());
    }

    void whatTheDispatcherRefusesIsNeverAsked()
    {
        Shared s;
        // Another device's slice.
        QCOMPARE(reasonOf(s.press(s.appA, 1, QStringLiteral("20m"))),
                 ownedElsewhere(QStringLiteral("iPad")));
        // An argument the verb does not take.
        QCOMPARE(reasonOf(s.core.invoke(s.appA, "bandStack.step",
                                        {int64("sliceId", 0),
                                         utf8("direction", QStringLiteral("next")),
                                         utf8("band", QStringLiteral("20m"))})),
                 kNotUnderstood);
        // A view that could not be real.
        QCOMPARE(reasonOf(s.core.invoke(s.appA, "bandStack.press",
                                        {int64("sliceId", 0), utf8("band", QStringLiteral("20m")),
                                         utf8("direction", QStringLiteral("next")),
                                         int64("viewSpanHz", -1)})),
                 kNotUnderstood);
        // An unknown entry, and a locked slice, in the dispatcher's words.
        QCOMPARE(reasonOf(s.core.invoke(s.appA, "bandStack.goTo",
                                        {int64("sliceId", 0),
                                         utf8("entryId", QStringLiteral("no-such-entry"))})),
                 QStringLiteral("That band stack entry no longer exists."));
        s.core.model->sliceById(0)->setLocked(true);
        const QString locked = reasonOf(s.press(s.appA, 0, QStringLiteral("20m")));
        QVERIFY2(locked.contains(QStringLiteral("the slice is locked")), qPrintable(locked));
        QCOMPARE(countOf(s.appA, QStringLiteral("confirm.request")), 0);
    }
};

QTEST_MAIN(TstBandStackQuestion)
#include "tst_bandstack_question.moc"
