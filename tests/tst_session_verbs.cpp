// =================================================================
// tests/tst_session_verbs.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Remote Daemon R2 Task 11: command verbs and results. Exercises the
// SessionMessages CommandInvoke/CommandResult codec plus a real
// SessionCommandDispatcher round trip over fakes/LoopbackStationLink.h.
// There is no transport until Task 18, so "over the wire" here means the
// codec and dispatch, not a socket -- LoopbackStationLink is a plain
// in-process byte relay standing in for it.
//
// Central case (R2 plan Task 11 step 1): with A(0) B(1) C(2), removing B
// -- dispatched as a real removeSlice command over the loopback -- leaves
// ids and positions diverged (C sits at id 2, position 1).
// RadioModel::setActiveSliceById(2) must still select slice C, not
// whatever (if anything) sits at position 2. A test that never makes ids
// and positions diverge would pass whether resolution used one or the
// other; this one would not, because position 2 does not exist any more.
//
// Also covers: addSlice / removeSlice / addSliceOnPan accept and reject
// paths, relaying RadioModel's own rejection reason rather than inventing
// one; requestSliceSampleRate deferred to a LATER RadioModel event-loop
// turn (this is the one verb that can reach RadioModel::setSampleRateLive's
// >= 40 ms of QThread::msleep, so dispatch() must never block on it); the
// CommandResult's affectedKeys reporting the ACTUAL scope of a rate
// change rather than the requested one, for slices co-hosting one DDC
// stream (no live connection needed) and for a Protocol 1 board's
// radio-wide RadioModel::setSampleRateLive escalation (a real HermesLite
// connection via fakes/ConnectableRadioModel.h); the new
// RadioModel::activeSliceIdChanged(int) signal firing the resolved slice
// id beside the existing positional activeSliceChanged, without disturbing
// it; and StateMirror::applyInbound()'s m_applying guard surviving a
// genuinely NESTED apply -- the Task 8 defect Task 11's command dispatch
// makes reachable (StateMirror.cpp's ApplyingGuard).
// =================================================================

#include <QtTest/QtTest>
#include <QByteArray>
#include <QList>
#include <QSignalSpy>
#include <QVariant>

#include <memory>

#include "core/session/MirrorSchema.h"
#include "core/session/ObjectRegistry.h"
#include "core/session/SessionCommandDispatcher.h"
#include "core/session/SessionMessages.h"
#include "core/session/StateMirror.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/ConnectableRadioModel.h"
#include "fakes/LoopbackStationLink.h"

using namespace NereusSDR;
using NereusSDR::Test::ConnectableRadioModel;
using NereusSDR::Test::LoopbackStationLink;

namespace {

MirrorUpdate intArg(const QByteArray& name, qint64 value)
{
    return MirrorUpdate{ 0, name, MirrorWireKind::Int64, QVariant(value) };
}

MirrorUpdate strArg(const QByteArray& name, const QString& value)
{
    return MirrorUpdate{ 0, name, MirrorWireKind::Utf8, QVariant(value) };
}

MirrorUpdate boolArg(const QByteArray& name, bool value)
{
    return MirrorUpdate{ 0, name, MirrorWireKind::Bool, QVariant(value) };
}

// Flattens every batch StateMirror emits, same shape as
// tst_mirror_forwarder.cpp / tst_mirror_inbound.cpp's own Collector
// (separate translation unit, so its own copy -- see either file's header
// comment for why this is not shared).
class Collector : public QObject {
public:
    explicit Collector(StateMirror* mirror)
    {
        connect(mirror, &StateMirror::propertiesChanged, this,
                [this](const QByteArray& key, const QList<MirrorUpdate>& ups) {
                    for (const MirrorUpdate& u : ups) {
                        flat.append(qMakePair(key, u));
                    }
                });
    }

    bool sawKey(const QByteArray& key) const
    {
        for (const auto& e : flat) {
            if (e.first == key) { return true; }
        }
        return false;
    }

    QList<QPair<QByteArray, MirrorUpdate>> flat;
};

// Wires a SessionCommandDispatcher to both ends of a LoopbackStationLink,
// exactly the shape Task 18's real session will have on the daemon side
// (decode inbound bytes, dispatch, encode+send whatever
// commandResultReady() produces) plus a client-side decode step so a test
// can inspect results without hand-rolling the codec calls at every call
// site.
class DispatchHarness : public QObject {
public:
    explicit DispatchHarness(RadioModel* radioModel, QObject* parent = nullptr)
        : QObject(parent)
        , dispatcher(radioModel)
    {
        connect(&link, &LoopbackStationLink::receivedByDaemon, this,
                [this](const QByteArray& wire) {
                    SessionMessage msg;
                    if (SessionMessages::decode(wire, &msg)) {
                        dispatcher.dispatch(msg);
                    }
                });
        connect(&dispatcher, &SessionCommandDispatcher::commandResultReady, this,
                [this](const SessionMessage& result) {
                    link.sendFromDaemon(SessionMessages::encode(result));
                });
        connect(&link, &LoopbackStationLink::receivedByClient, this,
                [this](const QByteArray& wire) {
                    SessionMessage msg;
                    if (SessionMessages::decode(wire, &msg)) {
                        results.append(msg);
                    }
                });
    }

    void invoke(const QByteArray& verb, quint32 commandId, const QList<MirrorUpdate>& arguments)
    {
        link.sendFromClient(
            SessionMessages::encode(SessionMessages::commandInvoke(verb, commandId, arguments)));
    }

    LoopbackStationLink link;
    SessionCommandDispatcher dispatcher;
    QList<SessionMessage> results;
};

} // namespace

class TestSessionVerbs : public QObject {
    Q_OBJECT

private slots:

    // ── Codec ────────────────────────────────────────────────────────────

    void commandMessagesRoundTripThroughJson()
    {
        {
            const SessionMessage m = SessionMessages::commandInvoke(
                "requestSliceSampleRate", 42,
                { intArg("sliceId", 2), intArg("rateHz", 384000) });
            SessionMessage back;
            QVERIFY(SessionMessages::decode(SessionMessages::encode(m), &back));
            QCOMPARE(back.kind, SessionMessageKind::CommandInvoke);
            QCOMPARE(back.commandVerb, QByteArray("requestSliceSampleRate"));
            QCOMPARE(back.commandId, quint32(42));
            QCOMPARE(back.arguments.size(), 2);
            QCOMPARE(back.arguments.at(0).name, QByteArray("sliceId"));
            QCOMPARE(back.arguments.at(0).value.toLongLong(), qint64(2));
            QCOMPARE(back.arguments.at(1).name, QByteArray("rateHz"));
            QCOMPARE(back.arguments.at(1).value.toLongLong(), qint64(384000));
        }
        {
            const SessionMessage m = SessionMessages::commandInvoke(
                "addSliceOnPan", 7, { strArg("panId", QStringLiteral("pan-b")) });
            SessionMessage back;
            QVERIFY(SessionMessages::decode(SessionMessages::encode(m), &back));
            QCOMPARE(back.arguments.first().value.toString(), QStringLiteral("pan-b"));
        }
        {
            const SessionMessage m = SessionMessages::commandResult(
                "requestSliceSampleRate", 42, true, QString(),
                { QByteArray("slice:0"), QByteArray("slice:1") });
            SessionMessage back;
            QVERIFY(SessionMessages::decode(SessionMessages::encode(m), &back));
            QCOMPARE(back.kind, SessionMessageKind::CommandResult);
            QCOMPARE(back.commandVerb, QByteArray("requestSliceSampleRate"));
            QCOMPARE(back.commandId, quint32(42));
            QVERIFY(back.accepted);
            QVERIFY(back.reason.isEmpty());
            QCOMPARE(back.affectedKeys, (QList<QByteArray>{ "slice:0", "slice:1" }));
        }
        {
            const SessionMessage m = SessionMessages::commandResult(
                "removeSlice", 9, false, QStringLiteral("no such slice"), {});
            SessionMessage back;
            QVERIFY(SessionMessages::decode(SessionMessages::encode(m), &back));
            QVERIFY(!back.accepted);
            QCOMPARE(back.reason, QStringLiteral("no such slice"));
            QVERIFY(back.affectedKeys.isEmpty());
        }
    }

    void decodeRejectsMalformedCommandMessages()
    {
        SessionMessage out;

        // CommandInvoke: missing "verb", missing "id", missing "args".
        QVERIFY2(!SessionMessages::decode(QByteArray(R"({"type":"command.invoke"})"), &out),
                 "command.invoke with no verb at all must be rejected");
        QVERIFY2(!SessionMessages::decode(
                     QByteArray(R"({"type":"command.invoke","verb":"addSlice"})"), &out),
                 "command.invoke with no id must be rejected");
        QVERIFY2(!SessionMessages::decode(
                     QByteArray(R"({"type":"command.invoke","verb":"addSlice","id":1})"), &out),
                 "command.invoke with no args array at all must be rejected");
        QVERIFY2(!SessionMessages::decode(
                     QByteArray(R"({"type":"command.invoke","verb":"addSlice","id":"not a number","args":[]})"),
                     &out),
                 "command.invoke with a non-numeric id must be rejected");

        // Fix round 1 review finding (Important 2): a NUMERIC id that
        // cannot be represented as a quint32 must also be rejected, before
        // decode() ever reaches the narrowing static_cast<quint32> --
        // otherwise this is a floating-to-unsigned conversion of an
        // unrepresentable value, undefined behaviour on untrusted input.
        // Mirrors the ordinal range check this same file already applies
        // to MirrorUpdate/SessionSchemaField (updateFromJson/fieldFromJson,
        // above).
        QVERIFY2(!SessionMessages::decode(
                     QByteArray(R"({"type":"command.invoke","verb":"addSlice","id":-1,"args":[]})"),
                     &out),
                 "a negative id must be rejected");
        QVERIFY2(!SessionMessages::decode(
                     QByteArray(R"({"type":"command.invoke","verb":"addSlice","id":1e30,"args":[]})"),
                     &out),
                 "an id past quint32's range must be rejected");

        // CommandResult: missing "verb"/"id"/"accepted"/"reason"/"affected".
        QVERIFY2(!SessionMessages::decode(QByteArray(R"({"type":"command.result"})"), &out),
                 "command.result with no verb at all must be rejected");
        QVERIFY2(!SessionMessages::decode(
                     QByteArray(R"({"type":"command.result","verb":"addSlice","id":1})"), &out),
                 "command.result with no accepted flag must be rejected");
        QVERIFY2(!SessionMessages::decode(
                     QByteArray(
                         R"({"type":"command.result","verb":"addSlice","id":1,"accepted":true})"),
                     &out),
                 "command.result with no reason string at all must be rejected");
        QVERIFY2(!SessionMessages::decode(
                     QByteArray(
                         R"({"type":"command.result","verb":"addSlice","id":1,"accepted":true,"reason":""})"),
                     &out),
                 "command.result with no affected array at all must be rejected");
        QVERIFY2(!SessionMessages::decode(
                     QByteArray(
                         R"({"type":"command.result","verb":"addSlice","id":1,"accepted":"yes","reason":"","affected":[]})"),
                     &out),
                 "a non-bool accepted value must be rejected");

        // Well-formed shell, still rejected: a non-string entry in
        // "affected" -- same reject-rather-than-coerce discipline as every
        // other array element this codec decodes.
        QVERIFY2(!SessionMessages::decode(
                     QByteArray(
                         R"({"type":"command.result","verb":"addSlice","id":1,"accepted":true,"reason":"","affected":[42]})"),
                     &out),
                 "a non-string affected-key entry must be rejected");
    }

    // ── TGXL station accessory commands ─────────────────────────────────

    void tgxlCommandsRejectMalformedWireShapesAtTheirOwnBoundary()
    {
        RadioModel model;
        DispatchHarness harness(&model);

        // These must be recognised as TGXL commands and rejected by the
        // TGXL argument boundary.  A generic "unrecognised command verb"
        // response would leave malformed requests indistinguishable from a
        // peer using an entirely unsupported verb.
        harness.invoke("configureTgxl", 1, {});
        harness.invoke("configureTgxl", 2,
                       { strArg("host", QStringLiteral("192.0.2.10")) });
        harness.invoke("configureTgxl", 3,
                       { strArg("host", QStringLiteral("192.0.2.10")),
                         intArg("port", 0) });
        harness.invoke("configureTgxl", 4,
                       { intArg("host", 1), intArg("port", 9010) });
        harness.invoke("configureTgxl", 5,
                       { strArg("host", QStringLiteral("192.0.2.10")),
                         intArg("port", 65536) });
        harness.invoke("configureTgxl", 6,
                       { strArg("host", QStringLiteral("192.0.2.10")),
                         MirrorUpdate{ 0, "port", MirrorWireKind::Enum, QVariant(qint64(9010)) } });
        harness.invoke("configureTgxl", 7,
                       { strArg("host", QStringLiteral("192.0.2.10")),
                         intArg("port", 9010),
                         intArg("port", 9011) });
        harness.invoke("disconnectTgxl", 8,
                       { strArg("host", QStringLiteral("192.0.2.10")) });

        QCOMPARE(harness.results.size(), 8);
        for (const SessionMessage& result : harness.results) {
            QVERIFY(!result.accepted);
            QVERIFY2(result.reason != QStringLiteral("unrecognised command verb"),
                     qPrintable(result.reason));
            QVERIFY(result.affectedKeys.isEmpty());
        }
    }

    void fourO3ACommandRejectsMalformedAndDuplicateEnabledArguments()
    {
        RadioModel model;
        DispatchHarness harness(&model);

        harness.invoke("setFourO3AEnabled", 1, {});
        harness.invoke("setFourO3AEnabled", 2, { intArg("enabled", 1) });
        harness.invoke("setFourO3AEnabled", 3,
                       { boolArg("enabled", true), boolArg("enabled", false) });

        QCOMPARE(harness.results.size(), 3);
        for (const SessionMessage& result : harness.results) {
            QVERIFY(!result.accepted);
            QVERIFY(result.reason != QStringLiteral("unrecognised command verb"));
            QVERIFY(result.affectedKeys.isEmpty());
        }
    }

    // ── Step 1: the central id-vs-position divergence test ──────────────

    void removingASliceOverTheWireDivergesIdsFromPositionsAndSetActiveSliceByIdPicksTheId()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        DispatchHarness harness(&model);

        // A(0) B(1) C(2).
        const int a = model.addSlice();
        const int b = model.addSlice();
        const int c = model.addSlice();
        QCOMPARE(a, 0);
        QCOMPARE(b, 1);
        QCOMPARE(c, 2);

        // Remove B THROUGH THE WIRE: encode a command.invoke, hand it to
        // the daemon side of the loopback, and confirm a command.result
        // comes back out the client side.
        harness.invoke("removeSlice", 1, { intArg("sliceId", b) });
        QCOMPARE(harness.results.size(), 1);
        QVERIFY2(harness.results.first().accepted, qPrintable(harness.results.first().reason));
        QCOMPARE(harness.results.first().affectedKeys,
                 (QList<QByteArray>{ ObjectRegistry::keyForSlice(b) }));

        // Ids and positions have now diverged: C sits at id 2, position 1.
        QCOMPARE(model.slices().size(), 2);
        QCOMPARE(model.slices().indexOf(model.sliceById(c)), 1);
        QVERIFY(model.sliceById(b) == nullptr);

        // The actual discriminator: ask for id 2 (C), not position 2
        // (which does not exist -- the list only has 2 entries now). A
        // test that never makes ids and positions diverge would pass
        // whether this resolved by id or by position; this one would not.
        QVERIFY(model.setActiveSliceById(c));
        QVERIFY2(model.activeSlice() == model.sliceById(c),
                 "setActiveSliceById(2) must select slice C (the object "
                 "whose id is 2), not whatever sits at position 2 (nothing) "
                 "or position 0 (A)");
    }

    // ── addSlice ─────────────────────────────────────────────────────────

    void addSliceCommandCreatesAndReportsKeyOrRelaysRejectionReason()
    {
        RadioModel model;
        model.configureStreamPool(1, 5, 192000); // exactly one DDC
        DispatchHarness harness(&model);

        harness.invoke("addSlice", 1, { strArg("initialPanId", QStringLiteral("pan-a")) });
        QCOMPARE(harness.results.size(), 1);
        QVERIFY2(harness.results.at(0).accepted, qPrintable(harness.results.at(0).reason));
        QCOMPARE(harness.results.at(0).commandVerb, QByteArray("addSlice"));
        QCOMPARE(harness.results.at(0).commandId, quint32(1));
        QCOMPARE(harness.results.at(0).affectedKeys,
                 (QList<QByteArray>{ ObjectRegistry::keyForSlice(0) }));
        QVERIFY(model.sliceById(0) != nullptr);

        // A second, differently-panned add: the allocator has no room left
        // (RadioModel.cpp's bindSliceToStream rejection, the same path
        // tst_mirror_lifecycle.cpp's bindPathRejectionProducesNoObjectCreate
        // exercises).
        harness.invoke("addSlice", 2, { strArg("initialPanId", QStringLiteral("pan-b")) });
        QCOMPARE(harness.results.size(), 2);
        QVERIFY(!harness.results.at(1).accepted);
        QVERIFY2(!harness.results.at(1).reason.isEmpty(),
                 "must relay the real allocator rejection reason, not a generic one");
        QVERIFY(harness.results.at(1).affectedKeys.isEmpty());
        QVERIFY(model.sliceById(1) == nullptr); // nothing was created
    }

    // ── removeSlice ──────────────────────────────────────────────────────

    void removeSliceCommandRemovesReportsKeyAndRejectsInvalidRequests()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        DispatchHarness harness(&model);

        const int a = model.addSlice();
        const int b = model.addSlice();

        harness.invoke("removeSlice", 1, { intArg("sliceId", 999) }); // unknown id
        QCOMPARE(harness.results.size(), 1);
        QVERIFY(!harness.results.at(0).accepted);
        QVERIFY(harness.results.at(0).affectedKeys.isEmpty());

        harness.invoke("removeSlice", 2, { intArg("sliceId", b) });
        QCOMPARE(harness.results.size(), 2);
        QVERIFY2(harness.results.at(1).accepted, qPrintable(harness.results.at(1).reason));
        QCOMPARE(harness.results.at(1).affectedKeys,
                 (QList<QByteArray>{ ObjectRegistry::keyForSlice(b) }));
        QVERIFY(model.sliceById(b) == nullptr);

        // RadioModel::removeSlice() (RadioModel.cpp) silently no-ops
        // rather than take the last remaining slice; the dispatcher must
        // catch this itself and report a rejection, not a false accept.
        harness.invoke("removeSlice", 3, { intArg("sliceId", a) });
        QCOMPARE(harness.results.size(), 3);
        QVERIFY(!harness.results.at(2).accepted);
        QVERIFY(harness.results.at(2).affectedKeys.isEmpty());
        QVERIFY(model.sliceById(a) != nullptr);
        QCOMPARE(model.slices().size(), 1);
    }

    // ── addSliceOnPan ────────────────────────────────────────────────────

    void addSliceOnPanCommandCreatesOrRelaysCapRejection()
    {
        RadioModel model;
        // A stream pool sized for one slice: the cap addSliceOnPan holds
        // to (sliceChannelLimit()).
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 1, 192000);
        DispatchHarness harness(&model);

        harness.invoke("addSliceOnPan", 1, { strArg("panId", QStringLiteral("pan-a")) });
        QCOMPARE(harness.results.size(), 1);
        QVERIFY2(harness.results.at(0).accepted, qPrintable(harness.results.at(0).reason));
        QCOMPARE(harness.results.at(0).affectedKeys,
                 (QList<QByteArray>{ ObjectRegistry::keyForSlice(0) }));

        // addSliceOnPan()'s own cap check, refusing before addSlice() is
        // even entered -- the same path tst_mirror_lifecycle.cpp's
        // addSliceOnPanCapRejectionProducesNoObjectCreate exercises.
        harness.invoke("addSliceOnPan", 2, { strArg("panId", QStringLiteral("pan-b")) });
        QCOMPARE(harness.results.size(), 2);
        QVERIFY(!harness.results.at(1).accepted);
        QVERIFY2(!harness.results.at(1).reason.isEmpty(),
                 "must relay the real cap-rejection reason");
        QVERIFY(harness.results.at(1).affectedKeys.isEmpty());
    }

    // ── requestSliceSampleRate ───────────────────────────────────────────

    void requestSliceSampleRateRejectsUnknownSliceIdSynchronously()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        DispatchHarness harness(&model);

        harness.invoke("requestSliceSampleRate", 1,
                       { intArg("sliceId", 999), intArg("rateHz", 384000) });

        // Unlike a well-formed request (see below), an unknown slice id is
        // rejected BEFORE the deferred RadioModel call is even queued, so
        // this must complete synchronously.
        QCOMPARE(harness.results.size(), 1);
        QVERIFY(!harness.results.at(0).accepted);
        QVERIFY(harness.results.at(0).affectedKeys.isEmpty());
    }

    void requestSliceSampleRateIsDeferredAndReportsActualScopeAcrossCoHostedSlices()
    {
        RadioModel model;
        model.configureStreamPool(1, 5, 192000); // exactly one DDC -- forces co-hosting
        DispatchHarness harness(&model);

        // Three unpanned adds all share the SAME stream by default
        // (tst_stream_pool_binding.cpp's own fixture, "no pan id" ==
        // "join whatever the empty-pan slot already has").
        const int a = model.addSlice();
        const int b = model.addSlice();
        const int c = model.addSlice();
        QCOMPARE(model.sliceById(b)->streamIndex(), model.sliceById(a)->streamIndex());
        QCOMPARE(model.sliceById(c)->streamIndex(), model.sliceById(a)->streamIndex());
        QCOMPARE(model.sliceById(a)->sampleRateHz(), 192000); // configureStreamPool's default

        harness.invoke("requestSliceSampleRate", 7,
                       { intArg("sliceId", a), intArg("rateHz", 768000) });

        // Must NOT complete synchronously: this is the one verb Task 11
        // defers to a LATER turn of RadioModel's own event loop. Fix round
        // 1 review finding (Minor 3): this does not make the ~40 ms of
        // QThread::msleep inside RadioModel::setSampleRateLive disappear --
        // the queued call is posted to the SAME thread the session read
        // loop lives on (SessionCommandDispatcher.h's THREADING section),
        // so that thread still stalls for the full duration, one event-loop
        // turn later than it otherwise would have. What deferring buys is
        // narrower: THIS dispatch() call returns immediately rather than
        // blocking inline, so whatever is already queued ahead of the
        // deferred call (including other already-dispatched commands) gets
        // a chance to run first instead of queueing up behind an inline
        // 40 ms block.
        QCOMPARE(harness.results.size(), 0);

        QTRY_COMPARE(harness.results.size(), 1);
        const SessionMessage result = harness.results.first();
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(result.commandVerb, QByteArray("requestSliceSampleRate"));
        QCOMPARE(result.commandId, quint32(7));

        // Actual scope: all THREE co-hosted slices moved, not only the one
        // named in the request.
        QCOMPARE(model.sliceById(a)->sampleRateHz(), 768000);
        QCOMPARE(model.sliceById(b)->sampleRateHz(), 768000);
        QCOMPARE(model.sliceById(c)->sampleRateHz(), 768000);
        QCOMPARE(result.affectedKeys.size(), 3);
        QVERIFY(result.affectedKeys.contains(ObjectRegistry::keyForSlice(a)));
        QVERIFY(result.affectedKeys.contains(ObjectRegistry::keyForSlice(b)));
        QVERIFY(result.affectedKeys.contains(ObjectRegistry::keyForSlice(c)));
    }

    // Heavier variant of the above: a REAL Protocol 1 connection
    // (fakes/ConnectableRadioModel.h, a HermesLite over a loopback
    // P1FakeRadio), where requestSliceSampleRate can escalate all the way
    // to RadioModel::setSampleRateLive's radio-wide 12-step sequence
    // (RadioModel.cpp: requestSliceSampleRate -> setStreamSampleRate ->
    // sampleRateIsRadioWide() -> setSampleRateLive). Two slices on
    // DIFFERENT streams -- so co-hosting (the test above) cannot explain
    // the result -- both moving proves this is genuinely the radio-wide
    // path, not the stream-sharing one.
    void requestSliceSampleRateReportsStationWideScopeOnAProtocol1Board()
    {
        std::unique_ptr<ConnectableRadioModel> harness = ConnectableRadioModel::create();
        QVERIFY(harness != nullptr);
        RadioModel& model = harness->model();
        QVERIFY2(model.sampleRateIsRadioWide(),
                 "test setup: this scenario only proves anything on a "
                 "Protocol 1 (radio-wide sample rate) connection");

        QVERIFY(model.sliceById(0) != nullptr); // Slice A, seeded by connectToRadio()
        const int b = model.addSlice(QStringLiteral("pan-b")); // its OWN, DIFFERENT stream
        QVERIFY(b >= 0);
        QVERIFY2(model.sliceById(b)->streamIndex() != model.sliceById(0)->streamIndex(),
                 "test setup: slice B must be on a stream slice A is NOT on, "
                 "or a co-hosted-stream move could explain the result too");

        const int before = model.sliceById(0)->sampleRateHz();
        const int target = (before == 384000) ? 192000 : 384000; // both valid on HermesLite/P1

        DispatchHarness dispatch(&model);
        dispatch.invoke("requestSliceSampleRate", 11,
                        { intArg("sliceId", b), intArg("rateHz", target) });
        QCOMPARE(dispatch.results.size(), 0); // still deferred

        QTRY_COMPARE_WITH_TIMEOUT(dispatch.results.size(), 1, 10000);
        const SessionMessage result = dispatch.results.first();
        QVERIFY2(result.accepted, qPrintable(result.reason));

        // Station-wide: slice A, on a stream the request never named, must
        // ALSO have moved.
        QCOMPARE(model.sliceById(0)->sampleRateHz(), target);
        QCOMPARE(model.sliceById(b)->sampleRateHz(), target);
        QVERIFY(result.affectedKeys.contains(ObjectRegistry::keyForSlice(0)));
        QVERIFY(result.affectedKeys.contains(ObjectRegistry::keyForSlice(b)));
        QCOMPARE(result.affectedKeys.size(), 2);

        harness.reset();
    }

    void commandsRejectMissingArgumentsAndUnrecognisedVerbs()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        DispatchHarness harness(&model);

        harness.invoke("addSlice", 1, {});                                    // no initialPanId
        harness.invoke("removeSlice", 2, {});                                 // no sliceId
        harness.invoke("requestSliceSampleRate", 3, { intArg("sliceId", 0) }); // no rateHz
        harness.invoke("addSliceOnPan", 4, {});                               // no panId
        harness.invoke("setActiveSliceById", 5, {});                         // no sliceId
        harness.invoke("bogusVerb", 6, {});                                   // unrecognised

        QCOMPARE(harness.results.size(), 6);
        for (const SessionMessage& r : harness.results) {
            QVERIFY2(!r.accepted, qPrintable(r.commandVerb));
            QVERIFY2(!r.reason.isEmpty(), qPrintable(r.commandVerb));
            QVERIFY(r.affectedKeys.isEmpty());
        }
    }

    // ── Out-of-range integer arguments (fix round 5, review Important 1) ──

    // Every id argument used to reach RadioModel through a bare
    // QVariant::toInt() with the `ok` flag discarded. Measured on this
    // tree's Qt: QVariant(qlonglong 4294967296).toInt() returns 0 with
    // ok == true, and 4294967297 returns 1. So an authenticated peer
    // asking to remove slice 4294967296 removed slice 0 instead, and the
    // command.result claimed accepted with affected ["slice:0"] -- the
    // request named an object that does not exist and a DIFFERENT,
    // existing object was destroyed.
    //
    // Driven from RAW WIRE BYTES rather than a hand-built MirrorUpdate.
    // The whole defect lives in what a decoded, attacker-controlled frame
    // narrows to, so building the argument in C++ (where the literal is
    // already an int) would test the wrong thing.
    void outOfRangeIntegerArgumentsAreRefusedRatherThanTruncated()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        DispatchHarness harness(&model);

        const int a = model.addSlice();
        const int b = model.addSlice();
        QCOMPARE(a, 0);
        QCOMPARE(b, 1);
        QCOMPARE(model.activeSlice(), model.sliceById(a));

        // 2^32: exactly representable as a double, so it survives JSON
        // intact and arrives as a qlonglong whose low 32 bits are zero.
        harness.link.sendFromClient(
            QByteArray(R"({"type":"command.invoke","verb":"removeSlice","id":1,)"
                       R"("args":[{"ordinal":0,"name":"sliceId","kind":"i64",)"
                       R"("value":4294967296}]})"));
        QCOMPARE(harness.results.size(), 1);
        QVERIFY2(!harness.results.at(0).accepted,
                 "sliceId 4294967296 names no slice and must be refused, not "
                 "truncated to 0");
        QVERIFY(harness.results.at(0).affectedKeys.isEmpty());
        QVERIFY2(model.sliceById(a) != nullptr, "slice 0 must not have been removed");
        QVERIFY(model.sliceById(b) != nullptr);

        // 2^32 + 1: low 32 bits are 1, so the same truncation activates
        // slice 1 instead of refusing.
        harness.link.sendFromClient(
            QByteArray(R"({"type":"command.invoke","verb":"setActiveSliceById","id":2,)"
                       R"("args":[{"ordinal":0,"name":"sliceId","kind":"i64",)"
                       R"("value":4294967297}]})"));
        QCOMPARE(harness.results.size(), 2);
        QVERIFY2(!harness.results.at(1).accepted,
                 "sliceId 4294967297 names no slice and must be refused, not "
                 "truncated to 1");
        QCOMPARE(model.activeSlice(), model.sliceById(a)); // unmoved

        // rateHz is narrowed by the same bare toInt(). 2^32 + 192000
        // truncates to a perfectly plausible 192 kHz.
        harness.link.sendFromClient(
            QByteArray(R"({"type":"command.invoke","verb":"requestSliceSampleRate","id":3,)"
                       R"("args":[{"ordinal":0,"name":"sliceId","kind":"i64","value":0},)"
                       R"({"ordinal":0,"name":"rateHz","kind":"i64","value":4295159296}]})"));
        // Refused at argument-parse time, so the answer is synchronous --
        // this verb's own deferral (see the class comment) is never
        // reached at all.
        QCOMPARE(harness.results.size(), 3);
        QVERIFY2(!harness.results.at(2).accepted,
                 "rateHz 4295159296 must be refused, not truncated to 192000");
        QVERIFY(harness.results.at(2).affectedKeys.isEmpty());

        // Every rejection carries a reason, and none of them is the
        // pre-existing "missing argument" one: the argument was present,
        // it was the VALUE that could not be represented.
        for (const SessionMessage& r : harness.results) {
            QVERIFY(!r.reason.isEmpty());
            QVERIFY2(!r.reason.contains(QStringLiteral("missing")),
                     qPrintable(r.reason));
        }

        // A well-formed id still works, over the same raw-bytes path, so
        // the check above is a range test and not a blanket refusal.
        harness.link.sendFromClient(
            QByteArray(R"({"type":"command.invoke","verb":"setActiveSliceById","id":4,)"
                       R"("args":[{"ordinal":0,"name":"sliceId","kind":"i64","value":1}]})"));
        QCOMPARE(harness.results.size(), 4);
        QVERIFY2(harness.results.at(3).accepted, qPrintable(harness.results.at(3).reason));
        QCOMPARE(model.activeSlice(), model.sliceById(b));
    }

    // ── setActiveSliceById (fix round 1, review Important 1) ────────────

    // Before this verb existed, a remote operator's active-slice click had
    // no path to the daemon at all: SliceModel::active carries no WRITE, so
    // StateMirror::applyInbound() always fell through to
    // applyMirroredValue(), which refused outright. This test dispatches
    // the new verb over the SAME LoopbackStationLink + SessionCommandDispatcher
    // round trip every other verb in this file uses, and asserts the
    // DAEMON-SIDE RadioModel's active slice actually moved -- not merely
    // that a command.result came back accepted.
    void setActiveSliceByIdCommandMovesTheDaemonSideActiveSliceAndRejectsUnknownIds()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        DispatchHarness harness(&model);

        const int a = model.addSlice();
        const int b = model.addSlice();
        QCOMPARE(model.activeSlice(), model.sliceById(a)); // first slice is active by default

        harness.invoke("setActiveSliceById", 1, { intArg("sliceId", b) });
        QCOMPARE(harness.results.size(), 1);
        QVERIFY2(harness.results.at(0).accepted, qPrintable(harness.results.at(0).reason));
        QCOMPARE(harness.results.at(0).commandVerb, QByteArray("setActiveSliceById"));
        QCOMPARE(harness.results.at(0).commandId, quint32(1));

        // The actual assertion this test exists for.
        QCOMPARE(model.activeSlice(), model.sliceById(b));

        // affectedKeys: the newly-active key plus the previously-active one.
        QCOMPARE(harness.results.at(0).affectedKeys.size(), 2);
        QVERIFY(harness.results.at(0).affectedKeys.contains(ObjectRegistry::keyForSlice(b)));
        QVERIFY(harness.results.at(0).affectedKeys.contains(ObjectRegistry::keyForSlice(a)));

        // Requesting the slice that is ALREADY active is a legitimate
        // no-op accept (RadioModel::setActiveSlice()'s own change-guard),
        // reported once, not as two copies of the same key.
        harness.invoke("setActiveSliceById", 2, { intArg("sliceId", b) });
        QCOMPARE(harness.results.size(), 2);
        QVERIFY2(harness.results.at(1).accepted, qPrintable(harness.results.at(1).reason));
        QCOMPARE(harness.results.at(1).affectedKeys,
                 (QList<QByteArray>{ ObjectRegistry::keyForSlice(b) }));

        // Unknown id: rejected with a reason, model unchanged.
        harness.invoke("setActiveSliceById", 3, { intArg("sliceId", 999) });
        QCOMPARE(harness.results.size(), 3);
        QVERIFY(!harness.results.at(2).accepted);
        QVERIFY2(!harness.results.at(2).reason.isEmpty(), "must carry a reason");
        QVERIFY(harness.results.at(2).affectedKeys.isEmpty());
        QCOMPARE(model.activeSlice(), model.sliceById(b)); // unchanged by the rejection
    }

    // ── activeSliceIdChanged ──────────────────────────────────────────────

    void activeSliceIdChangedFiresTheResolvedIdBesideThePositionalSignal()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        QSignalSpy positional(&model, &RadioModel::activeSliceChanged);
        QSignalSpy byId(&model, &RadioModel::activeSliceIdChanged);

        const int a = model.addSlice();
        const int b = model.addSlice();
        model.addSlice();
        QVERIFY(!positional.isEmpty());
        QVERIFY(!byId.isEmpty());
        positional.clear();
        byId.clear();

        // Remove A (currently active): the fallback becomes whichever now
        // sits at position 0 -- B, carrying id 1. The positional signal
        // reports 0 (a raw list position); the id-based one must report 1
        // (an object identity), proving it resolves through the slice
        // rather than re-emitting the same number on both channels.
        model.removeSlice(a);

        QVERIFY(!positional.isEmpty());
        QVERIFY(!byId.isEmpty());
        QCOMPARE(positional.last().at(0).toInt(), 0);
        QCOMPARE(byId.last().at(0).toInt(), b);
        QVERIFY2(b != 0,
                 "the discriminator only works if the resolved id and the "
                 "raw position differ -- b must not be 0");

        // The positional signal is unaffected: existing GUI code binding
        // to it still gets exactly the position it always did.
        QCOMPARE(model.activeSlice(), model.sliceById(b));
    }

    // ── StateMirror nested-apply guard (the inherited defect) ────────────

    // Task 8's review flagged StateMirror's m_applying guard as Minor
    // because nothing could re-enter applyInbound() while one was already
    // in flight. Task 11's command dispatch is what makes that reachable
    // (a command handler running a RadioModel call whose synchronous side
    // effects lead back into the mirror), so this test constructs the
    // reentrancy directly against StateMirror rather than through the full
    // dispatcher stack -- what matters is that SOME same-thread,
    // synchronous path re-enters applyInbound() while an outer one is
    // still on the call stack, and StateMirror has no way to tell that
    // apart from any other cause.
    void nestedApplyInboundDoesNotLeakTheOuterCallsRemainingDeltas()
    {
        SliceModel sliceA(0);
        SliceModel sliceB(1);
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &sliceA));
        QVERIFY(mirror.watch("slice:1", &sliceB));
        Collector c(&mirror);

        // Baselines, confirmed different from what this test writes below
        // -- SliceModel's setters are uniformly change-guarded, so a value
        // that does not genuinely differ from what is already there
        // produces no NOTIFY at all, which would make every assertion
        // below pass whether or not the guard works (tst_mirror_inbound.
        // cpp's own discipline).
        QVERIFY(sliceA.frequency() != 21050000.0);
        QVERIFY(sliceA.afGain() != 77);
        QVERIFY(sliceB.rfGain() != 5);
        QVERIFY(sliceA.rfGain() != 41);

        // The reentrancy trigger: reacting to slice A's OWN frequency
        // notify -- fired synchronously, from inside the OUTER
        // applyInbound() call's own watch.schema->write() -- by (1)
        // running a SECOND, NESTED applyInbound() call against a
        // DIFFERENT watched object (slice B), simulating a command
        // handler whose RadioModel call leads back into the mirror, then
        // (2), still synchronously inside the same handler, writing
        // ANOTHER property on the SAME object (slice A) the outer call is
        // still in the middle of applying. That second write is the
        // discriminator: it happens strictly AFTER the nested call has
        // returned but strictly BEFORE the outer applyInbound() call
        // itself does.
        bool nestedReturned = false;
        connect(&sliceA, &SliceModel::frequencyChanged, &mirror, [&](double) {
            const MirrorApplyResult inner = mirror.applyInbound("slice:1", "rfGain", QVariant(5));
            QVERIFY(inner.accepted);
            nestedReturned = true;
            sliceA.setAfGain(77);
        });

        const MirrorApplyResult outer =
            mirror.applyInbound("slice:0", "frequency", QVariant(21050000.0));

        QVERIFY2(outer.accepted, qPrintable(outer.reason));
        QVERIFY(nestedReturned);
        QCOMPARE(sliceA.frequency(), 21050000.0);
        QCOMPARE(sliceB.rfGain(), 5);
        QCOMPARE(sliceA.afGain(), 77);

        // The actual assertion this test exists for. Pre-fix, m_applying
        // was set true then unconditionally cleared to false around each
        // write -- the nested call's own cleanup would clear it while the
        // OUTER call was still unwinding (still inside sliceA's own
        // frequencyChanged handler, itself inside the outer write()), so
        // sliceA.setAfGain(77)'s afGainChanged notify would stop being
        // suppressed and leak out as an ordinary delta. A saved/restored
        // guard keeps m_applying true for the whole nested cascade, so
        // NOTHING here may have reached the wire.
        QVERIFY2(c.flat.isEmpty(), "no delta may leak while a nested apply is still unwinding");

        // And the guard must not stick afterward, matching tst_mirror_
        // inbound.cpp's own guardDoesNotOutliveTheApplyInboundCallThatSetIt:
        // a genuinely local change now (nothing nested, no apply in
        // flight) must forward normally.
        sliceA.setRfGain(41);
        QVERIFY2(c.sawKey("slice:0"), "forwarding must resume once the OUTER apply has returned");
    }
};

QTEST_MAIN(TestSessionVerbs)
#include "tst_session_verbs.moc"
