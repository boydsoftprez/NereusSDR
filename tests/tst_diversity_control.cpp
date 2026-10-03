// no-port-check: NereusSDR-original regression infrastructure.
// 2026-10-01 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "MultiDeviceHarness.h"
#include "core/ReceiverManager.h"
#include "core/ReceiveLayoutStore.h"
#include "core/codec/P1CodecStandard.h"
#include "core/codec/P2CodecOrionMkII.h"
#include "core/WdspEngine.h"
#include "models/RxDspWorker.h"

namespace {
const QHash<QByteArray, int> kDiversity{{"deviceAuth", 1}, {"sessionHolder", 1},
    {"sliceAccess", 1}, {"diversityControl", 1}};

QJsonObject state(const RadioModel& model)
{
    return QJsonDocument::fromJson(model.diversityState().toUtf8()).object();
}

QList<MirrorUpdate> move(const RadioModel& model, int source, int target)
{
    const SliceOwnership* own = model.sliceOwnership();
    const auto ref = [own](int id, bool incarnation) -> qint64 {
        return id < 0 ? 0 : static_cast<qint64>(incarnation ? own->incarnation(id)
                                                         : own->controlRevision(id));
    };
    return {{0, "enabled", MirrorWireKind::Bool, target >= 0},
        int64("stateRevision", state(model).value("revision").toInteger()),
        int64("sourceSliceId", source), int64("sourceIncarnation", ref(source, true)),
        int64("sourceControlRevision", ref(source, false)), int64("targetSliceId", target),
        int64("targetIncarnation", ref(target, true)), int64("targetControlRevision", ref(target, false))};
}

void prepare(Core& core, P2CodecOrionMkII& codec)
{
    core.model->setBoardForTest(HPSDRHW::OrionMKII);
    core.model->configureStreamPool(5, 5, 192000);
    core.model->receiverManager()->setMaxReceivers(5);
    core.model->bindUnboundSlices();
    core.model->receiverManager()->setP2Codec(&codec);
}
}

class TestDiversityControl : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(QStringLiteral("diversity-control-test-%1")
                                       .arg(QCoreApplication::applicationPid()));
    }
    void init() { AppSettings::instance().clear(); }
    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    void active_diversity_pauses_for_link_loss_and_recovers_without_retune()
    {
        RxDspWorker worker;
        P2CodecOrionMkII codec;
        Core core;
        prepare(core, codec);
        Device owner;
        core.pair(owner);
        auto* app = core.signIn(owner, kDiversity);
        QVERIFY(admitted(app));
        core.model->attachDspWorkerForTest(&worker);
        worker.setEngines(core.model->wdspEngine(), nullptr);
        worker.setBufferSizes(64, 64);
        // Real model/lane/worker route lifecycle with an offline WDSP API.
        // No radio socket, DSP engine/channel admission or sample processing.
        core.model->wdspEngine()->setExternalDiversityApiForTest({
            +[](int, int, int, int) {}, +[](int) {},
            +[](int, int, double**, double*) {}, +[](int, int) {},
            +[](int, int) {}, +[](int, int) {}, +[](int, int, double*, double*) {}});
        auto* slice = core.model->sliceById(0);
        slice->setDiversityPhaseDeg(73);
        slice->setDiversityGainDb(-4);
        QVERIFY(core.invoke(app, "diversity.setTarget", move(*core.model, -1, 0)).value("accepted").toBool());
        QVERIFY(core.model->waitForReceiveLaneForTest());
        const auto before = state(*core.model);
        QVERIFY(before.value("running").toBool());
        QVERIFY(!before.value("paused").toBool());
        auto liveBefore = before.value("live").toObject();
        liveBefore.remove("pattern"); // Existing pattern publication is coalesced.
        const auto checkOwnerAndBlend = [&liveBefore](const QJsonObject& summary) {
            auto live = summary.value("live").toObject();
            live.remove("pattern");
            return summary.value("requested").toBool() && live == liveBefore;
        };
        QSignalSpy published(core.model.get(), &RadioModel::diversityStateChanged);
        core.model->onConnectionStateChangedForTest(ConnectionState::LinkLost);
        const auto lost = state(*core.model);
        QVERIFY(checkOwnerAndBlend(lost));
        QVERIFY(!lost.value("running").toBool());
        QVERIFY(lost.value("paused").toBool());
        QCOMPARE(lost.value("reasonCode").toString(), QStringLiteral("radioDisconnected"));
        QCOMPARE(lost.value("reason").toString(), QStringLiteral("Connect the radio to use Diversity."));
        QVERIFY(lost.value("revision").toInteger() > before.value("revision").toInteger());
        QVERIFY(!published.isEmpty());
        for (const auto& target : lost.value("targets").toArray()) {
            QVERIFY(!target.toObject().value("eligible").toBool());
            QCOMPARE(target.toObject().value("reasonCode").toString(), QStringLiteral("radioDisconnected"));
        }
        for (const auto& signal : published) {
            const auto summary = QJsonDocument::fromJson(signal[0].toString().toUtf8()).object();
            QVERIFY(checkOwnerAndBlend(summary));
            QVERIFY(!summary.value("running").toBool());
            QVERIFY(summary.value("paused").toBool());
            QCOMPARE(summary.value("reason").toString(), lost.value("reason").toString());
        }
        core.model->onConnectionStateChangedForTest(ConnectionState::Connecting);
        const auto connecting = state(*core.model);
        QVERIFY(checkOwnerAndBlend(connecting));
        QVERIFY(!connecting.value("running").toBool());
        QVERIFY(connecting.value("paused").toBool());
        QCOMPARE(connecting.value("reason").toString(), lost.value("reason").toString());
        QCOMPARE(connecting.value("targets"), lost.value("targets"));
        QCOMPARE(connecting.value("revision").toInteger(), lost.value("revision").toInteger());
        core.model->onConnectionStateChangedForTest(ConnectionState::Connected);
        QVERIFY(core.model->waitForReceiveLaneForTest());
        const auto recovered = state(*core.model);
        QVERIFY(checkOwnerAndBlend(recovered));
        QVERIFY(recovered.value("running").toBool());
        QVERIFY(!recovered.value("paused").toBool());
        QVERIFY(recovered.value("reason").toString().isEmpty());
        QVERIFY(recovered.value("targets").toArray().first().toObject().value("eligible").toBool());
        QVERIFY(recovered.value("revision").toInteger() > connecting.value("revision").toInteger());
        QVERIFY(core.invoke(app, "diversity.setTarget", move(*core.model, 0, -1)).value("accepted").toBool());
        QVERIFY(core.model->waitForReceiveLaneForTest());
    }

    void replaced_session_cannot_commit_a_request_with_current_participants()
    {
        QObject replacement;
        P2CodecOrionMkII codec;
        Core core;
        prepare(core, codec);
        Device owner;
        core.pair(owner);
        auto* app = core.signIn(owner, kDiversity);
        QVERIFY(admitted(app));
        const int b = core.model->addSlice("pan-0");
        core.model->sliceOwnership()->setOwner(b, owner.key.fingerprint());
        QVERIFY(core.invoke(app, "diversity.setTarget", move(*core.model, -1, 0)).value("accepted").toBool());
        const auto request = move(*core.model, 0, b);
        const auto before = state(*core.model);
        auto* sessions = core.server->deviceSessions();
        const auto current = sessions->entry(owner.key.fingerprint());
        QVERIFY(current.has_value());
        // The registry replaces the session before StationServer retires
        // the older transport. A queued request must recheck admission at
        // execution even though source/target control and revisions match.
        const auto admission = sessions->admit(*current, &replacement);
        QCOMPARE(admission.admission, DeviceSessionRegistry::Admission::SameDevice);
        QCOMPARE(admission.replacedSession, current->session);
        QCOMPARE(state(*core.model), before);
        const auto refused = core.invoke(app, "diversity.setTarget", request);
        QVERIFY(!refused.value("accepted").toBool());
        QCOMPARE(refused.value("reason").toString(), QStringLiteral("This connection is no longer current on the Core."));
        QCOMPARE(state(*core.model), before);
        sessions->admit(*current, current->session);
        QVERIFY(core.invoke(app, "diversity.setTarget", request).value("accepted").toBool());
        QCOMPARE(state(*core.model).value("live").toObject().value("sliceId").toInt(), b);
    }

    void receiver_slot_swap_replays_only_the_complete_map()
    {
        ReceiverManager receivers;
        receivers.setMaxReceivers(3);
        for (int st = 0; st < 3; ++st) { QCOMPARE(receivers.createReceiver(), st); }
        receivers.setReceiverFrequency(0, 14200000);
        receivers.setReceiverFrequency(1, 7100000);
        receivers.setReceiverFrequency(2, 144100000);
        receivers.applyDdcMapping({0, 2, 4, -1, -1}, 7);
        QSignalSpy tuning(&receivers, &ReceiverManager::hardwareFrequencyChanged);
        QSignalSpy slotChanges(&receivers, &ReceiverManager::hardwareSlotsChanged);
        bool everyReplaySawFinalMap = true;
        connect(&receivers, &ReceiverManager::hardwareFrequencyChanged, &receivers,
                [&receivers, &everyReplaySawFinalMap](int, quint64) {
            everyReplaySawFinalMap &= receivers.receiverConfig(0).hardwareRx == 2
                && receivers.receiverConfig(1).hardwareRx == 0
                && receivers.receiverConfig(2).hardwareRx == 4;
        });
        receivers.applyDdcMapping({2, 0, 4, -1, -1}, 7);
        QVERIFY(everyReplaySawFinalMap);
        QCOMPARE(slotChanges.size(), 1);
        QCOMPARE(tuning.size(), 3);
        for (const auto& event : tuning) {
            const int ddc = event[0].toInt();
            const quint64 hz = event[1].toULongLong();
            QCOMPARE(hz, ddc == 2 ? quint64(14200000)
                        : ddc == 0 ? quint64(7100000) : quint64(144100000));
        }
        QCOMPARE(receivers.receiverConfig(2).hardwareRx, 4);
        receivers.applyDdcMapping({2, 0, 4, -1, -1}, 7);
        QCOMPARE(tuning.size(), 3);
    }

    void restart_manifest_restores_only_the_named_slice()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath("settings.xml"));
        const QList<ReceiveSliceState> slices{{0, "pan-0", 14200000, DSPMode::USB},
                                              {2, "pan-1", 7100000, DSPMode::LSB}};
        const QString mac = QStringLiteral("AA:BB:CC:DD:EE:01");
        QVERIFY(ReceiveLayoutStore::stage(settings, mac, slices, nullptr, std::nullopt, 2));
        const auto restored = ReceiveLayoutStore::load(settings, mac);
        QCOMPARE(restored.diversityOwnerId.value_or(-1), 2);
        QVERIFY(ReceiveLayoutStore::stage(settings, mac, slices.mid(0, 1), nullptr, std::nullopt, 2));
        QVERIFY(!ReceiveLayoutStore::load(settings, mac).diversityOwnerId.has_value());
        QVERIFY(ReceiveLayoutStore::stage(settings, mac, slices));
        QVERIFY(!ReceiveLayoutStore::load(settings, mac).diversityOwnerId.has_value());
    }

    void model_restart_restores_same_identity_and_saved_blend()
    {
        auto& settings = AppSettings::instance();
        settings.setValue("Slice2/Band40m/DiversityPhaseDeg", 123.0);
        settings.setValue("Slice2/Band40m/DiversityGainDb", -7.0);
        ReceiveLayoutStore::LoadResult layout;
        layout.state = ReceiveLayoutStore::LoadState::Loaded;
        layout.slices = {{0, "pan-0", 14200000, DSPMode::USB},
                         {2, "pan-1", 7100000, DSPMode::LSB}};
        layout.diversityOwnerId = 2;
        RadioModel restored;
        QString error;
        QVERIFY2(restored.hydrateReceiveLayout("AA:BB:CC:DD:EE:01", layout, &error), qPrintable(error));
        QCOMPARE(state(restored).value("live").toObject().value("sliceId").toInt(), 2);
        QCOMPARE(restored.sliceById(2)->diversityPhaseDeg(), 123.0);
        QCOMPARE(restored.sliceById(2)->diversityGainDb(), -7.0);
        QVERIFY(!restored.sliceById(0)->diversityEnabled());
        layout.slices.removeLast();
        // Retain the saved id deliberately: no matching restored slice
        // must mean off, including callers of hydration without the store.
        RadioModel absent;
        QVERIFY2(absent.hydrateReceiveLayout("AA:BB:CC:DD:EE:01", layout, &error), qPrintable(error));
        QVERIFY(state(absent).value("live").isNull());
    }

    void p1_full_receive_roster_refuses_to_take_an_unrelated_receiver()
    {
        P1CodecStandard codec;
        Core core;
        core.model->setBoardForTest(HPSDRHW::OrionMKII);
        core.model->configureStreamPool(5, 5, 192000);
        core.model->receiverManager()->setMaxReceivers(5);
        core.model->bindUnboundSlices();
        core.model->receiverManager()->setP1Codec(&codec);
        Device owner;
        core.pair(owner);
        auto* app = core.signIn(owner, kDiversity);
        for (int id = 1; id < 5; ++id) {
            QCOMPARE(core.model->addSlice(QStringLiteral("pan-%1").arg(id)), id);
            core.model->sliceOwnership()->setOwner(id, owner.key.fingerprint());
        }
        QString code;
        QVERIFY(!core.model->diversityEligibility(0, &code).isEmpty());
        QCOMPARE(code, QStringLiteral("resourcesUnavailable"));
        const auto result = core.invoke(app, "diversity.setTarget", move(*core.model, -1, 0));
        QVERIFY(!result.value("accepted").toBool());
        QVERIFY(!state(*core.model).value("requested").toBool());
        for (int id = 0; id < 5; ++id) {
            QCOMPARE(core.model->sliceById(id)->streamIndex(), id);
            QCOMPARE(core.model->sliceOwnership()->mark(id).owner, owner.key.fingerprint());
        }
    }

    void source_control_and_recycled_target_are_revalidated_without_mutation()
    {
        P2CodecOrionMkII codec;
        Core core;
        prepare(core, codec);
        Device owner, other;
        core.pair(owner);
        core.pair(other);
        auto* app = core.signIn(owner, kDiversity);
        QVERIFY(admitted(app));
        const int b = core.model->addSlice("pan-0");
        auto* own = core.model->sliceOwnership();
        own->setOwner(b, owner.key.fingerprint());
        QVERIFY(core.invoke(app, "diversity.setTarget", move(*core.model, -1, 0)).value("accepted").toBool());
        own->setOwner(0, other.key.fingerprint());
        const auto withoutSourceControl = move(*core.model, 0, b);
        QVERIFY(!core.invoke(app, "diversity.setTarget", withoutSourceControl).value("accepted").toBool());
        QVERIFY(core.model->sliceById(0)->diversityEnabled());
        QVERIFY(!core.model->sliceById(b)->diversityEnabled());
        own->setOwner(0, owner.key.fingerprint());
        auto recycled = move(*core.model, 0, b);
        core.model->removeSlice(b);
        QCOMPARE(core.model->addSlice("pan-0"), b);
        own->setOwner(b, owner.key.fingerprint());
        // Use the current station revision but deliberately retain the old
        // target incarnation: the participant check must independently fail.
        recycled[1] = int64("stateRevision", state(*core.model).value("revision").toInteger());
        QVERIFY(!core.invoke(app, "diversity.setTarget", recycled).value("accepted").toBool());
        QCOMPARE(core.model->diversityTargetSlice(), core.model->sliceById(0));
    }

    void p1_ps_keeps_the_receive_pair_when_separate_feedback_slots_fit()
    {
        P1CodecStandard codec;
        CodecContext ctx;
        ctx.model = HPSDRModel::ANAN7000D;
        ctx.diversity = true;
        ctx.diversityStream = 1;
        ctx.mox = true;
        ctx.puresignalRun = true;
        std::array<SliceConfig, 5> streams{};
        streams[0].live = streams[1].live = true;
        const auto a = codec.applyDdcAssignment(ctx, streams);
        QCOMPARE(a.streamDdc[1], 0);
        QCOMPARE(a.streamDdc[0], 2);
        QCOMPARE(a.psFwdDdc, 3);
        QCOMPARE(a.psRevDdc, 4);
        QCOMPARE(a.p1Diversity, 1);
        QCOMPARE(a.p1RxCount, 5);
        QCOMPARE(a.rate[0], a.rate[1]);
    }

    void p2_moves_pair_to_target_stream_without_retuning_other_streams()
    {
        P2CodecOrionMkII codec;
        CodecContext ctx;
        ctx.diversity = true;
        ctx.diversityStream = 2;
        std::array<SliceConfig, 5> streams{};
        for (int st = 0; st < 5; ++st) {
            streams[st].live = true;
            streams[st].sampleRateHz = st == 2 ? 96000 : 192000;
        }
        const auto a = codec.applyDdcAssignment(ctx, streams);
        QCOMPARE(a.streamDdc[2], 0);
        QCOMPARE(a.rate[0], 96000);
        QCOMPARE(a.rate[1], 96000);
        QCOMPARE(a.syncEnable, 2);
        QCOMPARE(a.streamDdc[0], 2);
        QCOMPARE(a.streamDdc[1], 3);
        QCOMPARE(a.streamDdc[3], 5);
        QCOMPARE(a.streamDdc[4], 6);
        QCOMPARE(a.rate[4], 0);
    }

    void p1_preserves_fixed_frame_and_allocates_four_streams_with_pair()
    {
        P1CodecStandard codec;
        CodecContext ctx;
        ctx.model = HPSDRModel::ANAN7000D;
        ctx.diversity = true;
        ctx.diversityStream = 2;
        std::array<SliceConfig, 5> streams{};
        for (int st = 0; st < 4; ++st) {
            streams[st].live = true;
            streams[st].sampleRateHz = 192000;
        }
        const auto a = codec.applyDdcAssignment(ctx, streams);
        QCOMPARE(a.p1RxCount, 5);
        QCOMPARE(a.p1Diversity, 1);
        QCOMPARE(a.streamDdc[2], 0);
        QCOMPARE(a.streamDdc[0], 2);
        QCOMPARE(a.streamDdc[1], 3);
        QCOMPARE(a.streamDdc[3], 4);
        QCOMPARE(a.rate[0], a.rate[1]);
        QCOMPARE((a.adcCtrl1 >> 2) & 3, 1);
    }

    void move_retains_each_blend_and_listener_summary_follows_unjoined_owner()
    {
        P2CodecOrionMkII codec;
        Core core;
        prepare(core, codec);
        Device owner, observer;
        core.pair(owner);
        core.pair(observer);
        auto* app = core.signIn(owner, kDiversity);
        QVERIFY(admitted(app));
        const int b = core.model->addSlice("pan-0");
        core.model->sliceOwnership()->setOwner(b, owner.key.fingerprint());
        auto* first = core.model->sliceById(0);
        auto* second = core.model->sliceById(b);
        first->setDiversityPhaseDeg(25);
        second->setDiversityPhaseDeg(120);
        second->setDiversityGainDb(-3);
        QVERIFY(core.invoke(app, "diversity.setTarget", move(*core.model, -1, 0)).value("accepted").toBool());
        auto stale = move(*core.model, 0, b);
        core.model->sliceOwnership()->setOwner(b, observer.key.fingerprint());
        QVERIFY(!core.invoke(app, "diversity.setTarget", stale).value("accepted").toBool());
        QCOMPARE(core.model->diversityTargetSlice(), first);
        core.model->sliceOwnership()->setOwner(b, owner.key.fingerprint());
        QVERIFY(core.model->sliceOwnership()->leave(observer.key.fingerprint(), b));
        QVERIFY(core.invoke(app, "diversity.setTarget", move(*core.model, 0, b)).value("accepted").toBool());
        QCOMPARE(core.model->diversityTargetSlice(), second);
        QCOMPARE(first->diversityPhaseDeg(), 25.0);
        QCOMPARE(second->diversityPhaseDeg(), 120.0);
        QCOMPARE(second->diversityGainDb(), -3.0);
        QVERIFY(!first->diversityEnabled());
        QVERIFY(second->diversityEnabled());
        auto* viewer = core.signIn(observer, {{"deviceAuth", 1}, {"diversityControl", 1}});
        QVERIFY(admitted(viewer));
        QCOMPARE(capability(viewer->received(), "diversityControlVersion").value_or(0), 1);
        QVERIFY(!core.model->sliceOwnership()->isListening(observer.key.fingerprint(), b));
        const auto summary = QJsonDocument::fromJson(latest(viewer->received(), "radio", "diversityState").toString().toUtf8()).object();
        QCOMPARE(summary.value("live").toObject().value("sliceId").toInt(), b);
        QCOMPARE(summary.value("live").toObject().value("phaseDeg").toDouble(), 120.0);
        QVERIFY(summary.value("live").toObject().value("pattern").isNull());
        QVERIFY(core.invoke(app, "diversity.setTarget", move(*core.model, b, -1)).value("accepted").toBool());
        QVERIFY(state(*core.model).value("live").isNull());
    }

    void legacy_write_never_transfers_and_exact_argument_shape_is_required()
    {
        P2CodecOrionMkII codec;
        Core core;
        prepare(core, codec);
        Device owner;
        core.pair(owner);
        auto* app = core.signIn(owner, kDiversity);
        QVERIFY(admitted(app));
        const int b = core.model->addSlice("pan-0");
        core.model->sliceOwnership()->setOwner(b, owner.key.fingerprint());
        core.model->sliceById(0)->setDiversityEnabled(true);
        QCOMPARE(core.model->diversityTargetSlice(), core.model->sliceById(0));
        app->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            QByteArray("slice:") + QByteArray::number(b), {{29, "diversityEnabled", MirrorWireKind::Bool, true}}, 41)));
        QTRY_VERIFY(!propertyResult(app, 41).isEmpty());
        QVERIFY(!core.model->sliceById(b)->diversityEnabled());
        QVERIFY(core.model->sliceById(0)->diversityEnabled());
        auto malformed = move(*core.model, 0, b);
        malformed.append(int64("targetSliceId", b));
        QVERIFY(!core.invoke(app, "diversity.setTarget", malformed).value("accepted").toBool());
        QCOMPARE(core.model->diversityTargetSlice(), core.model->sliceById(0));
    }

    void legacy_off_cannot_stop_another_owner_and_closing_live_slice_has_no_promotion()
    {
        P2CodecOrionMkII codec;
        Core core;
        prepare(core, codec);
        Device owner, other;
        core.pair(owner);
        core.pair(other);
        auto* app = core.signIn(owner, kDiversity);
        auto* visitor = core.signIn(other, kDiversity);
        const int b = core.model->addSlice("pan-0");
        core.model->sliceOwnership()->setOwner(b, owner.key.fingerprint());
        core.model->sliceById(b)->setDiversityPhaseDeg(87);
        QVERIFY(core.invoke(app, "diversity.setTarget", move(*core.model, -1, b)).value("accepted").toBool());
        visitor->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            QByteArray("slice:") + QByteArray::number(b), {{29, "diversityEnabled", MirrorWireKind::Bool, false}}, 42)));
        QTRY_VERIFY(!propertyResult(visitor, 42).isEmpty());
        QVERIFY(core.model->sliceById(b)->diversityEnabled());
        // Directly controlled live B can still turn itself off with the old
        // property; turning it on again requires the guarded new action.
        app->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            QByteArray("slice:") + QByteArray::number(b), {{29, "diversityEnabled", MirrorWireKind::Bool, false}}, 43)));
        QTRY_VERIFY(!propertyResult(app, 43).isEmpty());
        QVERIFY(state(*core.model).value("requested").toBool());
        QJsonObject question;
        QTRY_VERIFY(([&]() {
            question = firstOfType(app->received(), QStringLiteral("confirm.request"));
            return !question.isEmpty();
        })());
        QCOMPARE(question.value("forWriteId").toInteger(), qint64(43));
        QVERIFY(core.invoke(app, "confirm.proceed",
            {int64("id", question.value("id").toInteger()), int64("choice", -1)})
                .value("accepted").toBool());
        QTRY_VERIFY(!state(*core.model).value("requested").toBool());
        QVERIFY(core.invoke(app, "diversity.setTarget", move(*core.model, -1, b)).value("accepted").toBool());
        core.model->removeSlice(b);
        QVERIFY(!state(*core.model).value("requested").toBool());
        QVERIFY(!core.model->sliceById(0)->diversityEnabled());
        QCOMPARE(core.model->addSlice("pan-0"), b);
        QVERIFY(!core.model->sliceById(b)->diversityEnabled());
    }

    void summary_preserves_2m_frequency_and_old_peer_gets_no_new_surface()
    {
        P2CodecOrionMkII codec;
        Core core;
        prepare(core, codec);
        Device owner, oldDevice;
        core.pair(owner);
        core.pair(oldDevice);
        auto* app = core.signIn(owner, kDiversity);
        core.model->sliceById(0)->setFrequency(144100000.0);
        QVERIFY(core.invoke(app, "diversity.setTarget", move(*core.model, -1, 0)).value("accepted").toBool());
        const auto live = state(*core.model).value("live").toObject();
        QCOMPARE(live.value("frequencyHz").toDouble(), 144100000.0);
        QVERIFY(live.value("band").toInt() >= 0 && live.value("band").toInt() < 28);
        auto* old = core.signIn(oldDevice, {{"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 1}});
        QVERIFY(admitted(old));
        QVERIFY(!capability(old->received(), "diversityControlVersion").has_value());
        for (const auto& wire : old->received()) {
            const auto message = QJsonDocument::fromJson(wire).object();
            if (message.value("key").toString() != "radio") { continue; }
            for (const auto& property : message.value("properties").toArray()) {
                QVERIFY(property.toObject().value("name").toString() != "diversityState");
            }
        }
        const auto result = core.invoke(old, "diversity.setTarget", move(*core.model, 0, -1));
        QVERIFY(!result.value("accepted").toBool());
        QVERIFY(core.model->sliceById(0)->diversityEnabled());
    }

    void p2_ps_pause_preserves_requested_owner_and_normal_blend_writes()
    {
        P2CodecOrionMkII codec;
        Core core;
        prepare(core, codec);
        Device owner;
        core.pair(owner);
        auto* app = core.signIn(owner, kDiversity);
        QVERIFY(core.invoke(app, "diversity.setTarget", move(*core.model, -1, 0)).value("accepted").toBool());
        core.model->setDdcContextForTest(true, true, true);
        core.model->requestDdcAssignment();
        auto* slice = core.model->sliceById(0);
        slice->setDiversityPhaseDeg(54);
        slice->setDiversityGainDb(-8);
        const auto summary = state(*core.model);
        QVERIFY(summary.value("requested").toBool());
        QVERIFY(summary.value("paused").toBool());
        QCOMPARE(summary.value("reason").toString(),
                 QStringLiteral("Diversity pauses while PureSignal transmits on this radio."));
        QCOMPARE(summary.value("live").toObject().value("sliceId").toInt(), 0);
        QCOMPARE(summary.value("live").toObject().value("phaseDeg").toDouble(), 54.0);
        QCOMPARE(summary.value("live").toObject().value("gainDb").toDouble(), -8.0);
        core.model->setDdcContextForTest(false, true, true);
        core.model->requestDdcAssignment();
        QVERIFY(slice->diversityEnabled());
        QVERIFY(state(*core.model).value("reason").toString()
                    != QStringLiteral("Diversity pauses while PureSignal transmits on this radio."));
    }
};

QTEST_MAIN(TestDiversityControl)
#include "tst_diversity_control.moc"
