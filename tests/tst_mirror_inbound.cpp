// =================================================================
// tests/tst_mirror_inbound.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Remote Daemon R2 Task 8, inbound half: StateMirror::applyInbound()
// applies one write a remote peer sent for a watched object's property.
//
// Three things this file pins that nothing else does:
//
//   1. A property with a real Q_PROPERTY WRITE goes through
//      MirrorPolicy::inboundAllowed() then MirrorSchema::write(). The
//      SliceModel properties that carry WRITE but are Outbound
//      (MirrorPolicy.cpp: the R2 plan's chainIndex, ddcIndex,
//      streamIndex, shiftOffsetHz, sampleRateHz,
//      widebandExtensionRequested and psPaused, plus snrDb and
//      lastRadeRxCallsign, which joined the same category later) are
//      refused with a reason; sampleRateHz is the one the R2 plan names a
//      client verb for (requestSliceSampleRate), so its rejection must
//      name that verb literally.
//
//   2. A property with NO Q_PROPERTY WRITE at all (the ATU surface on
//      TunerModel, plus active/txSlice/band on SliceModel and
//      name/model/version on RadioModel) routes to that model's own
//      applyMirroredValue(name, value) hook instead -- a SEPARATE channel
//      from MirrorPolicy, because MirrorPolicy always classifies these
//      Outbound (there is nothing standard for it to write through) and
//      that guard is pinned for good in tst_mirror_schema's
//      everyReadOnlyPropertyIsDeniedInbound(). sliceIndex (CONSTANT) is
//      the one no-WRITE property that must NOT reach the hook; it is the
//      mirror's object identity and travels only in the snapshot.
//
//   3. Applying a write that genuinely changes a property's value must
//      produce NO outbound delta, for the object written AND for any
//      other watched object a same-thread, synchronous side effect of
//      that write touches -- RadioModel::addSlice()'s co-hosted-slice
//      NB-mode/NB-tuning peer mirrors are wired unconditionally
//      (RadioModel.cpp, inside addSlice(), same reasoning as the
//      frequencyChanged TX rollback handler beside them) and are exactly
//      this case: peer->setNbMode() fires nbModeChanged on the PEER
//      object, which a naive forwarder would send outbound as a delta the
//      remote peer never asked for. Task 7's own forwarder tests already
//      proved that applying an ALREADY-current value produces no NOTIFY
//      at all (SliceModel's setters are uniformly change-guarded), which
//      would make every test below pass whether or not the guard here
//      works; every value applied in this file is checked to differ from
//      what was there before.
//
//   4. iPhone app Task 72 (R-IOS-02, ruling 5.7): that suppression is per
//      WRITER. With device views attached, the write and its side effect
//      on the co-hosted slice are withheld from the writer's view only and
//      reach every other device's view.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: iPhone app plan Task 72 (R-IOS-02): echo per writer. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QVariant>

#include "core/session/MirrorPolicy.h"
#include "core/session/MirrorSchema.h"
#include "core/session/MirrorView.h"
#include "core/session/SessionMessages.h"
#include "core/session/StateMirror.h"
#include "core/StepAttenuatorController.h"
#include "core/StepAttenuatorFacade.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TunerModel.h"

using namespace NereusSDR;

namespace {

// Flattens every batch StateMirror emits, same shape as
// tst_mirror_forwarder.cpp's Collector (separate translation unit, so its
// own copy).
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

} // namespace

class TestMirrorInbound : public QObject {
    Q_OBJECT

private slots:

    // ── Bidirectional properties: the standard write path ──────────────────

    void bidirectionalWriteLandsThroughTheRealSetter()
    {
        SliceModel slice(0);
        QVERIFY(slice.frequency() != 7100000.0); // genuinely different, not vacuous
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &slice));

        const MirrorApplyResult result =
            mirror.applyInbound("slice:0", "frequency", QVariant(7100000.0));

        QVERIFY2(result.accepted, qPrintable(result.reason));
        QVERIFY(result.reason.isEmpty());
        QCOMPARE(slice.frequency(), 7100000.0);
    }

    void bidirectionalEnumWriteDecodesThroughTheUnderlyingInteger()
    {
        SliceModel slice(0);
        QCOMPARE(slice.nbMode(), NbMode::Off); // ctor default; genuinely changing it below
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &slice));

        const MirrorApplyResult result = mirror.applyInbound(
            "slice:0", "nbMode", QVariant(static_cast<int>(NbMode::NB)));

        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(slice.nbMode(), NbMode::NB);
    }

    // ── R-R3-46: the Core's step attenuator (`stepAtt`) ─────────────────────

    // An operator setting lands through the Core's controller, clamped to
    // its range; the facade says why when it kept another value.
    void stepAttWriteLandsThroughTheCoresController()
    {
        StepAttenuatorController controller;
        controller.setTickTimerEnabled(false);
        controller.setMaxAttenuation(61);
        StepAttenuatorFacade facade(nullptr);
        facade.bindController(&controller);
        StateMirror mirror;
        QVERIFY(mirror.watch("stepAtt", &facade));

        MirrorApplyResult result =
            mirror.applyInbound("stepAtt", "attenuationDb", QVariant(qlonglong(45)));
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(controller.attenuatorDb(), 45);
        QVERIFY(facade.settleReason("attenuationDb").isEmpty());

        result = mirror.applyInbound("stepAtt", "attenuationDb", QVariant(qlonglong(70)));
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(controller.attenuatorDb(), 61);
        QCOMPARE(facade.attenuationDb(), 61);
        QCOMPARE(facade.settleReason("attenuationDb"),
                 QStringLiteral("This radio's attenuator goes from 0 to 61 dB."));

        result = mirror.applyInbound("stepAtt", "autoAttEnabled", QVariant(true));
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QVERIFY(controller.autoAttEnabled());
        result = mirror.applyInbound("stepAtt", "autoAttUndoDelayMs", QVariant(qlonglong(2500)));
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(controller.autoUndoDelaySec(), 3);
        QCOMPARE(facade.autoAttUndoDelayMs(), 3000);
        QVERIFY(!facade.settleReason("autoAttUndoDelayMs").isEmpty());
    }

    // What the Core reports (its range, auto-attenuate's own state, the
    // overload readings, ADC sharing) is never written from outside.
    void stepAttCoreReportedPropertiesAreRefused()
    {
        StepAttenuatorController controller;
        controller.setTickTimerEnabled(false);
        StepAttenuatorFacade facade(nullptr);
        facade.bindController(&controller);
        StateMirror mirror;
        QVERIFY(mirror.watch("stepAtt", &facade));

        for (const char* name : { "minDb", "maxDb", "overloadAdc0", "overloadAdc1" }) {
            const MirrorApplyResult result =
                mirror.applyInbound("stepAtt", name, QVariant(qlonglong(99)));
            QVERIFY2(!result.accepted, name);
        }
        for (const char* name : { "autoAttApplied", "adcLinked" }) {
            const MirrorApplyResult result = mirror.applyInbound("stepAtt", name, QVariant(true));
            QVERIFY2(!result.accepted, name);
        }
        QCOMPARE(controller.maxAttenuation(), 31);
        QCOMPARE(facade.maxDb(), 31);
        QCOMPARE(facade.overloadAdc0(), 0);
        QVERIFY(!facade.autoAttApplied());
        // Bound, the facade takes no reported value from outside either.
        QVERIFY(!facade.applyRemoteProperty("maxDb", QVariant(99)));
    }

    // ── The seven writable-but-Outbound properties ──────────────────────────

    void sampleRateHzIsRejectedNamingItsControlAndLeavesThePropertyUnchanged()
    {
        SliceModel slice(0);
        const int before = slice.sampleRateHz();
        QVERIFY(before != 768000); // genuinely different value being attempted
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &slice));

        const MirrorApplyResult result =
            mirror.applyInbound("slice:0", "sampleRateHz", QVariant(768000));

        QVERIFY(!result.accepted);
        // R-IOS-01: in operator words, naming the control that sends
        // requestSliceSampleRate rather than the verb.
        QVERIFY2(result.reason.contains("sample rate with its own control"),
                 qPrintable(result.reason));
        QCOMPARE(slice.sampleRateHz(), before);
    }

    // The other six carry WRITE and are Outbound too, but the R2 plan names
    // no client verb for them yet (only sampleRateHz). The rejection must
    // still refuse, still leave the property untouched, and must NOT
    // fabricate a verb name nobody has assigned.
    void otherWritableOutboundPropertiesAreRejectedWithoutInventingAVerb()
    {
        SliceModel slice(0);
        const bool before = slice.psPaused();
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &slice));

        const MirrorApplyResult result =
            mirror.applyInbound("slice:0", "psPaused", QVariant(!before));

        QVERIFY(!result.accepted);
        QVERIFY(!result.reason.isEmpty());
        QVERIFY2(!result.reason.contains("requestSliceSampleRate"),
                 "psPaused has no assigned verb; must not borrow sampleRateHz's");
        QCOMPARE(slice.psPaused(), before);
    }

    // ── Structural rejections ────────────────────────────────────────────

    void constantPropertyIsRejectedAndNeverReachesTheHook()
    {
        SliceModel slice(7);
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:7", &slice));

        const MirrorApplyResult result =
            mirror.applyInbound("slice:7", "sliceIndex", QVariant(99));

        QVERIFY(!result.accepted);
        QCOMPARE(slice.sliceIndex(), 7); // untouched -- the object identity
    }

    void unknownPropertyIsRejected()
    {
        SliceModel slice(0);
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &slice));

        const MirrorApplyResult result =
            mirror.applyInbound("slice:0", "noSuchPropertyEver", QVariant(1));

        QVERIFY(!result.accepted);
        QVERIFY(!result.reason.isEmpty());
    }

    void unknownObjectKeyIsRejected()
    {
        StateMirror mirror;
        const MirrorApplyResult result =
            mirror.applyInbound("slice:99", "frequency", QVariant(7100000.0));
        QVERIFY(!result.accepted);
        QVERIFY(!result.reason.isEmpty());
    }

    void ordinalOverloadResolvesTheSamePropertyAsTheNameOverload()
    {
        SliceModel slice(0);
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &slice));

        const MirrorSchema& schema = MirrorSchema::forObject(&slice);
        const MirrorProperty* prop = schema.byName("frequency");
        QVERIFY(prop != nullptr);

        const MirrorApplyResult result =
            mirror.applyInbound("slice:0", prop->ordinal, QVariant(7100000.0));

        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(result.property, QByteArray("frequency"));
        QCOMPARE(slice.frequency(), 7100000.0);
    }

    // ── Whole-branch review, Important 4: the NAME is load-bearing ─────────
    //
    // MirrorSchema.h used to call the ordinal "the wire identity" and the
    // name something "carried for logging and tests only", and
    // StateMirror.h used to describe the ordinal overload above as keyed
    // by "what an actual wire frame carries". Both were false as shipped:
    // everything cross-process routes by NAME. StationServer::
    // handlePropertyWrite applies by name, StationClient::applyUpdates and
    // onWriteFlushTick both resolve schema.byName(update.name),
    // StationCapabilities::fromUpdates dispatches on name with a
    // hardcoded ordinal of 0, and StationClient::handleSchema builds a
    // name set and discards every ordinal it is sent. The ordinal overload
    // above has no production caller at all; the ordinal's real job is
    // in-process, as MirrorCoalescer's key.
    //
    // The comments are now corrected, and this is the thing that keeps
    // them true. The optimisation they used to invite is dropping `name`
    // from updateToJson/updateFromJson to shrink a 146-property snapshot,
    // and it would make every property.write refuse with "no such
    // mirrored property" and every inbound delta land in the
    // schema-only-on-station bucket -- both log-only, both silent to the
    // operator. Built from a LIVE model through the REAL codec rather
    // than from a hand-written MirrorUpdate literal, which is exactly how
    // the task-18 NaN defect survived four reviews.
    void thePropertyNameIsWhatTheWireRoutesOnNotTheOrdinal()
    {
        SliceModel slice(0);
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &slice));
        slice.setFrequency(7100000.0);

        // A real snapshot of a real object, encoded and decoded by the
        // real message codec.
        const QList<MirrorUpdate> live = mirror.snapshot("slice:0");
        MirrorUpdate frequency;
        bool found = false;
        for (const MirrorUpdate& u : live) {
            if (u.name == "frequency") {
                frequency = u;
                found = true;
                break;
            }
        }
        QVERIFY2(found, "the live snapshot carried no frequency property");

        const QByteArray wire =
            SessionMessages::encode(SessionMessages::delta("slice:0", { frequency }));
        SessionMessage decoded;
        QVERIFY(SessionMessages::decode(wire, &decoded));
        QCOMPARE(decoded.updates.size(), 1);
        const MirrorUpdate roundTripped = decoded.updates.first();

        // 1. The encoded frame carries the name, and the decoder keeps it.
        //    This assertion is what a "drop the name to save bytes" change
        //    breaks first, before any behaviour test would notice.
        QVERIFY2(wire.contains("\"name\":\"frequency\""),
                 "the encoded frame does not carry the property NAME; every "
                 "cross-process apply path resolves by name, so this is not a "
                 "byte to save");
        QCOMPARE(roundTripped.name, QByteArray("frequency"));

        // 2. The daemon's apply path is BY NAME. This is literally
        //    StationServer::handlePropertyWrite's call.
        const MirrorApplyResult byName =
            mirror.applyInbound("slice:0", roundTripped.name, QVariant(14200000.0));
        QVERIFY2(byName.accepted, qPrintable(byName.reason));
        QCOMPARE(slice.frequency(), 14200000.0);

        // 3. ...and the CLIENT's is too: StationClient::applyUpdates does
        //    exactly this lookup before it can apply anything.
        QVERIFY(MirrorSchema::forObject(&slice).byName(roundTripped.name) != nullptr);

        // 4. The same update with its name stripped and its ordinal still
        //    perfectly correct is REFUSED, which is the state every frame
        //    would be in if the name were dropped from the wire.
        MirrorUpdate nameless = roundTripped;
        nameless.name.clear();
        const MirrorApplyResult refused =
            mirror.applyInbound("slice:0", nameless.name, QVariant(21050000.0));
        QVERIFY2(!refused.accepted,
                 "a nameless update was applied, so this test cannot detect the "
                 "name being dropped from the wire");
        QCOMPARE(slice.frequency(), 14200000.0);

        // 5. The ordinal it still carries really is correct -- so the
        //    refusal above is about the NAME being gone, not about a
        //    damaged update. This is also the whole reason ordinal skew is
        //    harmless today: no cross-process consumer reads it.
        QCOMPARE(nameless.ordinal,
                 MirrorSchema::forObject(&slice).byName("frequency")->ordinal);
        QVERIFY(mirror.applyInbound("slice:0", nameless.ordinal, QVariant(21050000.0)).accepted);
        QCOMPARE(slice.frequency(), 21050000.0);
    }

    // ── The per-model applyMirroredValue hook ───────────────────────────────

    // TunerModel's whole ATU surface carries no WRITE. Without the hook,
    // nothing on the tuner is remotely operable at all -- not even the
    // controls (isOperate/isBypass/antennaA) that DO have a real command
    // slot behind them.
    void tunerOperateAndBypassRouteThroughTheHookToTheRealCommandSlots()
    {
        TunerModel tuner;
        // Direct unit check of the hook itself, independent of StateMirror's
        // reflection-based dispatch (proven separately below): must return
        // accepted (empty reason)...
        QVERIFY(tuner.applyMirroredValue("isOperate", QVariant(true)).isEmpty());
        // ...and must NOT have taken a shortcut straight to the property.
        // setOperate() requires a live, connected TgxlConnection (none is
        // bound here) and is a no-op without one; if isOperate() changed
        // anyway, the hook bypassed the command path and wrote m_operate
        // directly, which is the one thing it must not do (a local GUI's
        // TunerApplet drives the SAME setOperate() slot, and the mirror
        // must not diverge from that contract).
        QCOMPARE(tuner.isOperate(), false);

        QVERIFY(tuner.applyMirroredValue("isBypass", QVariant(true)).isEmpty());
        QCOMPARE(tuner.isBypass(), false);
    }

    void tunerAntennaARejectsOutOfRangeWithoutInventingASilentSuccess()
    {
        TunerModel tuner;
        const QString reason = tuner.applyMirroredValue("antennaA", QVariant(7));
        QVERIFY2(!reason.isEmpty(), "antennaA=7 is out of TunerModel::setAntennaA's "
                                    "own 1..3 range and must be refused, not silently "
                                    "swallowed and reported as applied");
    }

    void tunerTelemetryPropertiesAreRejectedByTheHook()
    {
        TunerModel tuner;
        for (const char* name : { "relayC1", "relayL", "relayC2", "isTuning",
                                  "hasAntennaSwitch", "isPresent",
                                  "hasDirectConnection", "tgxlIp", "fwdPower",
                                  "swr" }) {
            const QString reason = tuner.applyMirroredValue(name, QVariant(1));
            QVERIFY2(!reason.isEmpty(), name);
        }
    }

    // Proves StateMirror's dispatch genuinely reaches the hook via
    // QMetaObject::invokeMethod (as opposed to the direct-call unit checks
    // above, which exercise the hook but not the routing to it).
    void stateMirrorRoutesANoWritePropertyToTheTunerHook()
    {
        TunerModel tuner;
        StateMirror mirror;
        QVERIFY(mirror.watch("tuner:0", &tuner));

        const MirrorApplyResult result =
            mirror.applyInbound("tuner:0", "isOperate", QVariant(true));

        QVERIFY2(result.accepted, qPrintable(result.reason));
    }

    void radioModelIdentityPropertiesAreRejectedByTheHook()
    {
        RadioModel radio;
        for (const char* name : { "name", "model", "version" }) {
            const QString reason =
                radio.applyMirroredValue(name, QVariant(QStringLiteral("bogus")));
            QVERIFY2(!reason.isEmpty(), name);
        }
    }

    // Task 3 re-points `connected` at m_connectionState and Task 18 drives
    // it through setConnectionState; applyMirroredValue must not build a
    // second path into it, so it falls through to the same generic refusal
    // as name/model/version rather than doing anything special.
    void radioModelConnectedIsRejectedByTheHookLikeAnyOtherUnhandledProperty()
    {
        RadioModel radio;
        const QString reason = radio.applyMirroredValue("connected", QVariant(true));
        QVERIFY(!reason.isEmpty());
    }

    // SliceModel's no-WRITE surface is active/txSlice/band -- THREE, not
    // the brief's stale two (Task 6 added band). sliceIndex is the fourth
    // no-WRITE property but is CONSTANT and is proven separately never to
    // reach the hook at all.
    void sliceModelNoWritePropertiesAreRejectedByTheHookWithDistinctReasons()
    {
        SliceModel slice(0);
        const QString activeReason = slice.applyMirroredValue("active", QVariant(true));
        const QString txReason = slice.applyMirroredValue("txSlice", QVariant(true));
        const QString bandReason = slice.applyMirroredValue(
            "band", QVariant(static_cast<int>(Band::Band40m)));

        QVERIFY(!activeReason.isEmpty());
        QVERIFY(!txReason.isEmpty());
        QVERIFY(!bandReason.isEmpty());
        // Distinct, not a single copy-pasted message for all three -- each
        // names why (arbitrated elsewhere, or derived from frequency).
        QVERIFY(activeReason != txReason);
        QVERIFY(activeReason != bandReason);
        QVERIFY(txReason != bandReason);

        QCOMPARE(slice.isActive(), false);
        QCOMPARE(slice.isTxSlice(), false);
        QCOMPARE(slice.band(), Band::Band20m); // ctor default (14.225 MHz); untouched
    }

    // ── No outbound delta ────────────────────────────────────────────────

    // The trap named in the task notes: SliceModel's setters are uniformly
    // change-guarded, so re-applying the CURRENT value produces no NOTIFY
    // at all and this assertion would hold whether or not the guard works.
    // nbMode's ctor default is Off; NB is genuinely different.
    void inboundApplyOfAGenuinelyDifferentValueProducesNoOutboundDelta()
    {
        SliceModel slice(0);
        QCOMPARE(slice.nbMode(), NbMode::Off);
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &slice));
        Collector c(&mirror);

        const MirrorApplyResult result = mirror.applyInbound(
            "slice:0", "nbMode", QVariant(static_cast<int>(NbMode::NB)));

        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(slice.nbMode(), NbMode::NB); // the write genuinely landed...
        QVERIFY2(c.flat.isEmpty(),
                 "...yet must not have produced an outbound delta");
    }

    // The guard must not stick: once applyInbound() returns, this same
    // object's notifies must forward normally again.
    void guardDoesNotOutliveTheApplyInboundCallThatSetIt()
    {
        SliceModel slice(0);
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &slice));
        Collector c(&mirror);

        mirror.applyInbound("slice:0", "nbMode",
                            QVariant(static_cast<int>(NbMode::NB)));
        QVERIFY(c.flat.isEmpty());

        slice.setFrequency(7100000.0); // an ordinary LOCAL change, not mirrored
        QVERIFY2(c.sawKey("slice:0"),
                 "forwarding must resume once applyInbound() has returned");
    }

    // ── The RadioModel peer-mirror sites (the guard extension) ──────────────
    //
    // RadioModel::addSlice() wires two peer-mirror connects UNCONDITIONALLY
    // (not gated on wireSliceSignals / m_connection, by design -- see the
    // comment beside them): the nbModeChanged peer mirror, and the
    // mirrorNbTuning helper shared by nb1Threshold/nb1TransitionMs/
    // nb1LeadMs/nb1LagMs/nb2Mode. Both exist so co-hosted slices (multiple
    // demod slices sharing one DDC stream, hence one physical WDSP
    // blanker) never disagree on NB state. An inbound write to ONE
    // co-hosted slice's nbMode/nb-tuning therefore changes ANOTHER watched
    // SliceModel too, purely as a same-thread, synchronous side effect of
    // the write StateMirror is already applying.
    //
    // The peer's own change must still happen locally -- skipping it would
    // leave the peer's WDSP channel disagreeing with the one that just
    // changed, exactly the desync these connects exist to prevent -- but
    // it must not be forwarded outbound as a delta the remote peer never
    // asked for. StateMirror::onWatchedPropertyChanged() achieves both by
    // checking m_applying before looking at WHICH object fired the notify,
    // so the suppression covers this same-thread cascade without
    // RadioModel needing to know a mirror exists.

    void peerNbModeMirrorAppliesLocallyButIsNotForwarded()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        const int b = model.addSlice();
        // Matches tst_stream_pool_binding.cpp's own co-hosting fixture:
        // 10 kHz apart lands both inside one stream's window.
        model.sliceById(a)->setFrequency(14200000.0);
        model.sliceById(b)->setFrequency(14210000.0);
        QCOMPARE(model.sliceById(b)->streamIndex(), model.sliceById(a)->streamIndex());

        SliceModel* sliceA = model.sliceById(a);
        SliceModel* sliceB = model.sliceById(b);
        QCOMPARE(sliceA->nbMode(), NbMode::Off);
        QCOMPARE(sliceB->nbMode(), NbMode::Off);

        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", sliceA));
        QVERIFY(mirror.watch("slice:1", sliceB));
        Collector c(&mirror);

        const MirrorApplyResult result = mirror.applyInbound(
            "slice:0", "nbMode", QVariant(static_cast<int>(NbMode::NB)));

        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(sliceA->nbMode(), NbMode::NB);
        // The peer mirror in addSlice() really ran (hardware correctness:
        // the co-hosted stream's single blanker must agree)...
        QCOMPARE(sliceB->nbMode(), NbMode::NB);
        // ...but neither slice's change reached the wire.
        QVERIFY2(c.flat.isEmpty(),
                 "the peer's own nbModeChanged must not have been forwarded");
    }

    void peerNbTuningMirrorAppliesLocallyButIsNotForwarded()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        const int b = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        model.sliceById(b)->setFrequency(14210000.0);
        QCOMPARE(model.sliceById(b)->streamIndex(), model.sliceById(a)->streamIndex());

        SliceModel* sliceA = model.sliceById(a);
        SliceModel* sliceB = model.sliceById(b);
        const int before = sliceA->nb1Threshold();
        QCOMPARE(sliceB->nb1Threshold(), before);
        const int genuinelyDifferent = before + 5;

        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", sliceA));
        QVERIFY(mirror.watch("slice:1", sliceB));
        Collector c(&mirror);

        const MirrorApplyResult result = mirror.applyInbound(
            "slice:0", "nb1Threshold", QVariant(genuinelyDifferent));

        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(sliceA->nb1Threshold(), genuinelyDifferent);
        QCOMPARE(sliceB->nb1Threshold(), genuinelyDifferent); // mirrorNbTuning ran
        QVERIFY(c.flat.isEmpty());
    }

    // Proves the guard extension changed nothing about RadioModel.cpp's own
    // behaviour: with NO StateMirror involved at all -- the exact situation
    // every existing local-mode user is in today -- a plain local write
    // still mirrors to the co-hosted peer exactly as it always has.
    void peerMirrorStillRunsForAPurelyLocalChangeNoMirrorInvolved()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        const int b = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        model.sliceById(b)->setFrequency(14210000.0);
        QCOMPARE(model.sliceById(b)->streamIndex(), model.sliceById(a)->streamIndex());

        SliceModel* sliceA = model.sliceById(a);
        SliceModel* sliceB = model.sliceById(b);
        QCOMPARE(sliceB->nbMode(), NbMode::Off);

        sliceA->setNbMode(NbMode::NB); // local operator action, no mirror anywhere

        QCOMPARE(sliceB->nbMode(), NbMode::NB);
    }

    // iPhone app Task 72 (ruling 5.7): the co-hosted NB tuning mirror, with
    // two devices' views. A's write reaches B's view on both slices and
    // A's view on neither; propertiesChanged() stays silent as before.
    void peerNbTuningMirrorReachesEveryViewButTheWriters()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        const int b = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        model.sliceById(b)->setFrequency(14210000.0);
        QCOMPARE(model.sliceById(b)->streamIndex(), model.sliceById(a)->streamIndex());
        SliceModel* sliceA = model.sliceById(a);
        const int genuinelyDifferent = sliceA->nb1Threshold() + 5;

        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", sliceA));
        QVERIFY(mirror.watch("slice:1", model.sliceById(b)));
        Collector c(&mirror);
        QList<SessionMessage> toA;
        QList<SessionMessage> toB;
        MirrorView viewA(&mirror, [&toA](const SessionMessage& m) { toA.append(m); });
        MirrorView viewB(&mirror, [&toB](const SessionMessage& m) { toB.append(m); });
        viewA.attach();
        viewB.attach();
        toA.clear();
        toB.clear();

        const MirrorApplyResult result = mirror.applyInbound(
            "slice:0", "nb1Threshold", QVariant(genuinelyDifferent), &viewA);
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(model.sliceById(b)->nb1Threshold(), genuinelyDifferent);
        viewA.flush();
        viewB.flush();

        QVERIFY(c.flat.isEmpty());
        QVERIFY(toA.isEmpty());
        QSet<QByteArray> reachedB;
        for (const SessionMessage& m : toB) {
            for (const MirrorUpdate& u : m.updates) {
                if (u.name == "nb1Threshold" && u.value.toInt() == genuinelyDifferent) {
                    reachedB.insert(m.objectKey);
                }
            }
        }
        QCOMPARE(reachedB, (QSet<QByteArray>{"slice:0", "slice:1"}));
    }
};

QTEST_MAIN(TestMirrorInbound)
#include "tst_mirror_inbound.moc"
