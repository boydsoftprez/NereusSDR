// no-port-check: NereusSDR-original local/hosted/remote admission regression.
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-03: reentrant close/commit/hydration regression coverage, same author/assistant.
#include "MultiDeviceHarness.h"
#include "core/MoxController.h"
#include "core/TxSliceArbiter.h"
#include "core/ReceiverManager.h"
#include "core/ReceiveLayoutStore.h"
#include "core/codec/P2CodecOrionMkII.h"
#include "core/session/SessionCommandDispatcher.h"
#include "core/session/SliceAccessSet.h"
#include "core/session/StateMirror.h"
#include "core/safety/TxRefusal.h"
#include <QMetaMethod>
#include <thread>

namespace {
const QHash<QByteArray, int> kControl{{"deviceAuth", 1}, {"sessionHolder", 1},
    {"sliceAccess", 1}, {"diversityControl", 1}};
QJsonObject summary(const RadioModel& model)
{
    return QJsonDocument::fromJson(model.diversityState().toUtf8()).object();
}
QVariant value(const SessionMessage& message, const QByteArray& name)
{
    for (const auto& field : message.updates) { if (field.name == name) { return field.value; } }
    return {};
}
SessionMessage request(const RadioModel& model, int source, int target, quint32 id = 701)
{
    const auto* own = model.sliceOwnership();
    const auto identity = [own](int slice, bool incarnation) -> qint64 {
        return slice < 0 ? 0 : qint64(incarnation ? own->incarnation(slice) : own->controlRevision(slice));
    };
    return SessionMessages::commandInvoke("diversity.setTarget", id,
        {{0, "enabled", MirrorWireKind::Bool, target >= 0},
         int64("stateRevision", summary(model).value("revision").toInteger()),
         int64("sourceSliceId", source), int64("sourceIncarnation", identity(source, true)),
         int64("sourceControlRevision", identity(source, false)), int64("targetSliceId", target),
         int64("targetIncarnation", identity(target, true)), int64("targetControlRevision", identity(target, false))});
}
void currentRevision(SessionMessage& invoke, const RadioModel& model)
{
    for (auto& field : invoke.arguments) {
        if (field.name == "stateRevision") { field.value = qint64(model.diversityStateRevision()); }
    }
}
void prepareModel(RadioModel& model, P2CodecOrionMkII& codec)
{
    model.setBoardForTest(HPSDRHW::OrionMKII);
    model.setConnectionStateForTest(ConnectionState::Connected);
    model.configureStreamPool(5, 5, 192000);
    model.receiverManager()->setMaxReceivers(5);
    model.bindUnboundSlices();
    model.receiverManager()->setP2Codec(&codec);
}
struct Local {
    P2CodecOrionMkII codec; // Outlives the model's codec observer.
    RadioModel model;
    Local()
    {
        prepareModel(model, codec);
        // Existing, explicit station controller ownership, not an implicit
        // claim by the new action. Actual unclaimed policy is tested below.
        SliceOwnership::CreatorScope station(model.sliceOwnership(), SliceOwnership::stationDevice());
        model.addSlice("pan-0");
    }
    int add()
    {
        SliceOwnership::CreatorScope station(model.sliceOwnership(), SliceOwnership::stationDevice());
        return model.addSlice("pan-0");
    }
};
SessionMessage hosted(StationServer& server, const SessionMessage& invoke)
{
    std::optional<SessionMessage> result;
    server.invokeAsStationDevice(invoke, [&result](const SessionMessage& message) { result = message; }, {});
    if (!result) { return {}; }
    return *result;
}
}

class TestDiversityLocalAdmission : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(QStringLiteral("diversity-local-test-%1")
                                       .arg(QCoreApplication::applicationPid()));
    }
    void init() { AppSettings::instance().clear(); }
    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // Real current producer, no stub or removed earlier fix. A synchronous
    // summary observer retires the model just as the prepared desktop row does.
    void deleting_model_during_active_summary_returns_safely_without_state()
    {
        P2CodecOrionMkII codec;
        Core core;
        prepareModel(*core.model, codec);
        auto* model = core.model.get();
        model->sliceOwnership()->setOwner(0, SliceOwnership::stationDevice());
        QCOMPARE(model->sliceOwnership()->mark(0).owner, SliceOwnership::stationDevice());
        QVERIFY(model->diversityEligibility(0).isEmpty());
        QVERIFY(!summary(*model).value("requested").toBool());
        const auto invoke = request(*model, -1, 0, 709);
        QPointer<RadioModel> observed(model);
        QSignalSpy destroyed(model, &QObject::destroyed);
        bool deletedFromActiveSummary = false;
        QJsonObject published;
        connect(model, &RadioModel::diversityStateChanged, this, [&](const QString& state) {
            const auto current = QJsonDocument::fromJson(state.toUtf8()).object();
            if (deletedFromActiveSummary || !current.value("requested").toBool()
                || current.value("live").toObject().value("sliceId").toInt(-1) != 0) { return; }
            published = current; // Keep value bytes before destroying the producer.
            deletedFromActiveSummary = true;
            core.model.reset();
        });
        const auto result = model->invokeDiversityAsStationDevice(invoke);
        QVERIFY(deletedFromActiveSummary);
        QVERIFY(!observed);
        QCOMPARE(destroyed.size(), 1);
        QVERIFY(published.value("requested").toBool());
        QCOMPARE(published.value("live").toObject().value("sliceId").toInt(-1), 0);
        QCOMPARE(result.kind, SessionMessageKind::CommandResult);
        QCOMPARE(result.commandVerb, invoke.commandVerb);
        QCOMPARE(result.commandId, invoke.commandId);
        QVERIFY(!result.accepted);
        QCOMPARE(result.reason, QStringLiteral("The Core has no radio ready."));
        QVERIFY(result.updates.isEmpty()); // No authoritative state read from a dead model.
        QVERIFY(result.affectedKeys.isEmpty());
    }

    void deleting_model_during_slice_enable_returns_safely_without_state()
    {
        P2CodecOrionMkII codec;
        Core core;
        prepareModel(*core.model, codec);
        auto* model = core.model.get();
        model->sliceOwnership()->setOwner(0, SliceOwnership::stationDevice());
        QVERIFY(model->diversityEligibility(0).isEmpty());
        const auto invoke = request(*model, -1, 0, 710);
        QPointer<RadioModel> observed(model);
        QSignalSpy destroyed(model, &QObject::destroyed);
        bool deletedFromEnable = false;
        connect(model->sliceById(0), &SliceModel::diversityEnabledChanged, this, [&](bool on) {
            if (!on || deletedFromEnable) { return; }
            deletedFromEnable = true;
            core.model.reset();
        });
        const auto result = model->invokeDiversityAsStationDevice(invoke);
        QVERIFY(deletedFromEnable);
        QVERIFY(!observed);
        QCOMPARE(destroyed.size(), 1);
        QCOMPARE(result.kind, SessionMessageKind::CommandResult);
        QCOMPARE(result.commandVerb, invoke.commandVerb);
        QCOMPARE(result.commandId, invoke.commandId);
        QVERIFY(!result.accepted);
        QCOMPARE(result.reason, QStringLiteral("The Core has no radio ready."));
        QVERIFY(result.updates.isEmpty());
        QVERIFY(result.affectedKeys.isEmpty());
    }

    void closing_target_observer_may_retire_model_data()
    {
        QTest::addColumn<bool>("claimsClose");
        QTest::newRow("ordinary") << false;
        QTest::newRow("unclaimed") << true;
    }

    void closing_target_observer_may_retire_model()
    {
        QFETCH(bool, claimsClose);
        P2CodecOrionMkII codec;
        Core core;
        prepareModel(*core.model, codec);
        auto* model = core.model.get();
        model->sliceOwnership()->setOwner(0, SliceOwnership::stationDevice());
        {
            SliceOwnership::CreatorScope station(model->sliceOwnership(), SliceOwnership::stationDevice());
            QVERIFY(model->addSlice("pan-0") >= 0);
        }
        QVERIFY(model->invokeDiversityAsStationDevice(request(*model, -1, 0)).accepted);
        if (claimsClose) {
            model->sliceOwnership()->setOwner(0, {});
            QVERIFY(model->sliceOwnership()->leave(SliceOwnership::stationDevice(), 0));
            QVERIFY(model->sliceOwnership()->unclaimed().contains(0));
        }
        QPointer<RadioModel> alive(model);
        bool retired = false;
        connect(model->sliceById(0), &SliceModel::diversityEnabledChanged, this, [&](bool on) {
            if (on || retired) { return; }
            retired = true;
            core.model.reset();
        });
        if (claimsClose) { QVERIFY(!model->closeUnclaimedSlice(0)); }
        else { model->removeSlice(0); }
        QVERIFY(retired);
        QVERIFY(!alive);
    }

    void nested_same_target_close_preserves_other_normal_slice()
    {
        Local local;
        const int b = local.add();
        auto& model = local.model;
        QPointer<SliceModel> originalB(model.sliceById(b));
        const auto bIncarnation = model.sliceOwnership()->incarnation(b);
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, -1, 0)).accepted);
        bool nested = false;
        connect(model.sliceById(0), &SliceModel::diversityEnabledChanged, this, [&](bool on) {
            if (on || nested) { return; }
            nested = true;
            model.removeSlice(0);
        });
        model.removeSlice(0);
        QVERIFY(nested);
        QVERIFY(!model.sliceById(0));
        QCOMPARE(model.slices().size(), 1);
        QCOMPARE(model.sliceById(b), originalB.data());
        QCOMPARE(model.sliceOwnership()->incarnation(b), bIncarnation);
        QVERIFY(!originalB->diversityEnabled());
        QVERIFY(!summary(model).value("requested").toBool());
    }

    void target_close_revalidates_changed_topology_data()
    {
        QTest::addColumn<bool>("removeOther");
        QTest::newRow("position-shifts") << true;
        QTest::newRow("last-normal-slice-gate") << false;
    }

    void target_close_revalidates_changed_topology()
    {
        QFETCH(bool, removeOther);
        Local local;
        const int b = local.add();
        const int c = removeOther ? local.add() : -1;
        auto& model = local.model;
        const int target = removeOther ? b : 0;
        const int other = removeOther ? 0 : b;
        QPointer<SliceModel> survivor(model.sliceById(removeOther ? c : target));
        const auto incarnation = model.sliceOwnership()->incarnation(survivor->sliceIndex());
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, -1, target)).accepted);
        bool nested = false;
        connect(model.sliceById(target), &SliceModel::diversityEnabledChanged, this, [&](bool on) {
            if (on || nested) { return; }
            nested = true;
            model.removeSlice(other);
        });
        model.removeSlice(target);
        QVERIFY(nested);
        QVERIFY(!model.sliceById(other));
        QCOMPARE(model.slices().size(), 1);
        QCOMPARE(model.sliceById(survivor->sliceIndex()), survivor.data());
        QCOMPARE(model.sliceOwnership()->incarnation(survivor->sliceIndex()), incarnation);
        QVERIFY(!survivor->diversityEnabled());
        if (removeOther) { QVERIFY(!model.sliceById(target)); }
    }

    void valid_nested_station_move_is_refused_during_flag_commit()
    {
        Local local;
        const int b = local.add();
        auto& model = local.model;
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, -1, 0)).accepted);
        const quint64 revision = model.diversityStateRevision();
        SessionMessage nestedResult;
        bool nested = false;
        connect(model.sliceById(0), &SliceModel::diversityEnabledChanged, this, [&](bool on) {
            if (on || nested) { return; }
            nested = true;
            // The outer move has disabled A but not enabled B: the actual
            // authoritative snapshot is temporarily off. Use CURRENT tuples.
            QVERIFY(summary(model).value("live").isNull());
            nestedResult = model.invokeDiversityAsStationDevice(request(model, -1, 0, 812));
        });
        const auto outer = model.invokeDiversityAsStationDevice(request(model, 0, b, 811));
        QVERIFY(nested);
        qInfo() << "commit outcome" << summary(model) << "A/B flags"
                << model.sliceById(0)->diversityEnabled() << model.sliceById(b)->diversityEnabled()
                << "outer/nested accepted" << outer.accepted << nestedResult.accepted;
        QVERIFY(outer.accepted);
        QVERIFY(!nestedResult.accepted);
        QCOMPARE(nestedResult.commandId, quint32(812));
        QCOMPARE(value(nestedResult, "reasonCode").toString(), QStringLiteral("stateChanged"));
        QCOMPARE(summary(model).value("live").toObject().value("sliceId").toInt(-1), b);
        QVERIFY(!model.sliceById(0)->diversityEnabled());
        QVERIFY(model.sliceById(b)->diversityEnabled());
        QCOMPARE(model.diversityStateRevision(), revision + 1);
        QCOMPARE(value(outer, "diversityState").toString(), model.diversityState());
        QVERIFY(QJsonDocument::fromJson(value(nestedResult, "diversityState").toString().toUtf8())
                .object().value("live").isNull());
    }

    void mid_commit_internal_close_cannot_remove_requested_target()
    {
        Local local;
        const int b = local.add();
        auto& model = local.model;
        QPointer<SliceModel> target(model.sliceById(b));
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, -1, 0)).accepted);
        bool nested = false;
        connect(target, &SliceModel::diversityEnabledChanged, this, [&](bool on) {
            if (!on || nested) { return; }
            nested = true;
            model.removeSlice(b);
        });
        const auto outer = model.invokeDiversityAsStationDevice(request(model, 0, b));
        QVERIFY(nested);
        QVERIFY(outer.accepted);
        QCOMPARE(model.sliceById(b), target.data());
        QCOMPARE(model.slices().size(), 2);
        QVERIFY(!model.sliceById(0)->diversityEnabled());
        QVERIFY(target->diversityEnabled());
        QCOMPARE(summary(model).value("live").toObject().value("sliceId").toInt(-1), b);
    }

    void legacy_flag_observer_cannot_reenable_previous_target_mid_commit()
    {
        Local local;
        const int b = local.add();
        auto& model = local.model;
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, -1, 0)).accepted);
        bool nested = false;
        connect(model.sliceById(0), &SliceModel::diversityEnabledChanged, this, [&](bool on) {
            if (on || nested) { return; }
            nested = true;
            model.sliceById(0)->setDiversityEnabled(true);
        });
        const auto outer = model.invokeDiversityAsStationDevice(request(model, 0, b));
        QVERIFY(nested);
        QVERIFY(outer.accepted);
        QVERIFY(!model.sliceById(0)->diversityEnabled());
        QVERIFY(model.sliceById(b)->diversityEnabled());
        QCOMPARE(summary(model).value("live").toObject().value("sliceId").toInt(-1), b);
    }

    void claims_close_rechecks_new_controller_listener_or_hold_data()
    {
        QTest::addColumn<int>("change");
        QTest::newRow("controller") << 0;
        QTest::newRow("listener") << 1;
        QTest::newRow("hold") << 2;
    }

    void claims_close_rechecks_new_controller_listener_or_hold()
    {
        QFETCH(int, change);
        Local local;
        auto& model = local.model;
        auto* own = model.sliceOwnership();
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, -1, 0)).accepted);
        own->setOwner(0, {});
        QVERIFY(own->leave(SliceOwnership::stationDevice(), 0));
        QVERIFY(own->unclaimed().contains(0));
        QPointer<SliceModel> original(model.sliceById(0));
        const auto incarnation = own->incarnation(0);
        const QByteArray other("other-device");
        bool changed = false;
        connect(original, &SliceModel::diversityEnabledChanged, this, [&](bool on) {
            if (on || changed) { return; }
            changed = true;
            if (change == 0) { own->setOwner(0, other); }
            else if (change == 1) { QVERIFY(own->join(other, 0)); }
            else { own->hold(0, other); }
        });
        QVERIFY(!model.closeUnclaimedSlice(0));
        QVERIFY(changed);
        QCOMPARE(model.sliceById(0), original.data());
        QCOMPARE(own->incarnation(0), incarnation);
        QVERIFY(!own->unclaimed().contains(0));
        QVERIFY(!original->diversityEnabled());
        if (change == 0) { QCOMPARE(own->mark(0).owner, other); }
        else if (change == 1) { QVERIFY(own->isListening(other, 0)); }
        else { QCOMPARE(own->mark(0).heldFor, other); }
    }

    void same_id_recreated_during_close_keeps_new_incarnation()
    {
        Local local;
        const int b = local.add();
        auto& model = local.model;
        const auto oldIncarnation = model.sliceOwnership()->incarnation(0);
        QPointer<SliceModel> old(model.sliceById(0));
        QPointer<SliceModel> replacement;
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, -1, 0)).accepted);
        bool nested = false;
        connect(old, &SliceModel::diversityEnabledChanged, this, [&](bool on) {
            if (on || nested) { return; }
            nested = true;
            model.removeSlice(0);
            const int id = local.add();
            QCOMPARE(id, 0);
            replacement = model.sliceById(id);
        });
        model.removeSlice(0);
        QVERIFY(nested);
        QVERIFY(replacement);
        QVERIFY(replacement != old);
        QCOMPARE(model.sliceById(0), replacement.data());
        QVERIFY(model.sliceOwnership()->incarnation(0) != oldIncarnation);
        QVERIFY(model.sliceById(b));
        QCOMPARE(model.slices().size(), 2);
        QVERIFY(!replacement->diversityEnabled());
    }

    void hydration_observer_retirement_data()
    {
        QTest::addColumn<bool>("retireOldTarget");
        QTest::newRow("remove-old-requested-owner") << true;
        QTest::newRow("restore-requested-owner") << false;
    }

    void hydration_observer_retirement()
    {
        QFETCH(bool, retireOldTarget);
        P2CodecOrionMkII codec;
        Core core;
        auto* model = core.model.get();
        if (retireOldTarget) {
            prepareModel(*model, codec);
            model->sliceOwnership()->setOwner(0, SliceOwnership::stationDevice());
            QVERIFY(model->invokeDiversityAsStationDevice(request(*model, -1, 0)).accepted);
            model->setConnectionStateForTest(ConnectionState::Disconnected);
            model->sliceById(0)->setStreamIndex(-1);
        }
        ReceiveLayoutStore::LoadResult layout;
        layout.state = ReceiveLayoutStore::LoadState::Loaded;
        layout.slices = {{retireOldTarget ? 1 : 0, QStringLiteral("pan-0"), 14293200.0, DSPMode::USB}};
        if (!retireOldTarget) { layout.diversityOwnerId = 0; }
        bool retired = false;
        QPointer<RadioModel> alive(model);
        connect(model->sliceById(0), &SliceModel::diversityEnabledChanged, this, [&](bool on) {
            if (retired || on == retireOldTarget) { return; }
            retired = true;
            core.model.reset();
        });
        QString error;
        const bool restored = model->hydrateReceiveLayout("AA:BB:CC:DD:EE:01", layout, &error);
        QVERIFY(retired);
        QVERIFY(!alive);
        QVERIFY(!restored);
    }

    void hydration_close_observer_cannot_publish_a_removed_replacement()
    {
        Local local;
        auto& model = local.model;
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, -1, 0)).accepted);
        model.setConnectionStateForTest(ConnectionState::Disconnected);
        model.sliceById(0)->setStreamIndex(-1);
        ReceiveLayoutStore::LoadResult layout;
        layout.state = ReceiveLayoutStore::LoadState::Loaded;
        layout.slices = {{1, QStringLiteral("pan-1"), 7200000.0, DSPMode::USB}};
        bool nested = false;
        connect(model.sliceById(0), &SliceModel::diversityEnabledChanged, this, [&](bool on) {
            if (on || nested) { return; }
            nested = true;
            QVERIFY(model.sliceById(1));
            model.removeSlice(1);
        });
        QString error;
        QVERIFY(!model.hydrateReceiveLayout("AA:BB:CC:DD:EE:01", layout, &error));
        QVERIFY(nested);
        QVERIFY(!error.isEmpty());
        QCOMPARE(model.slices().size(), 1);
        QVERIFY(model.sliceById(0));
        QVERIFY(!model.sliceById(1));
        QVERIFY(!model.sliceById(0)->diversityEnabled());
    }

    void prepare_layout_stops_after_hydration_retires_model()
    {
        Core core;
        auto* model = core.model.get();
        const QString mac = QStringLiteral("AA:BB:CC:DD:EE:01");
        const QList<ReceiveSliceState> slices{{0, QStringLiteral("pan-0"), 14293200.0, DSPMode::USB}};
        QVERIFY(ReceiveLayoutStore::stage(AppSettings::instance(), mac, slices,
                                          nullptr, std::nullopt, 0));
        QVERIFY(AppSettings::instance().save());
        QCOMPARE(ReceiveLayoutStore::load(AppSettings::instance(), mac).state,
                 ReceiveLayoutStore::LoadState::Loaded);
        QPointer<RadioModel> alive(model);
        bool retired = false;
        connect(model->sliceById(0), &SliceModel::diversityEnabledChanged, this, [&](bool on) {
            if (!on || retired) { return; }
            retired = true;
            core.model.reset();
        });
        model->prepareReceiveLayout(mac);
        QVERIFY(retired);
        QVERIFY(!alive);
    }

    void binding_stops_after_channel_close_retires_model_data()
    {
        QTest::addColumn<bool>("managedRecovery");
        QTest::newRow("ordinary-bind") << false;
        QTest::newRow("managed-recovery-bind") << true;
    }

    void binding_stops_after_channel_close_retires_model()
    {
        QFETCH(bool, managedRecovery);
        P2CodecOrionMkII codec;
        Core core;
        auto* model = core.model.get();
        prepareModel(*model, codec);
        model->sliceOwnership()->setOwner(0, SliceOwnership::stationDevice());
        int b;
        {
            SliceOwnership::CreatorScope station(model->sliceOwnership(), SliceOwnership::stationDevice());
            b = model->addSlice("pan-0");
        }
        QCOMPARE(b, 1);
        QVERIFY(model->invokeDiversityAsStationDevice(request(*model, -1, b)).accepted);
        if (managedRecovery) {
            const QString mac = QStringLiteral("AA:BB:CC:DD:EE:02");
            QCOMPARE(ReceiveLayoutStore::load(AppSettings::instance(), mac).state,
                     ReceiveLayoutStore::LoadState::Missing);
            model->prepareReceiveLayout(mac);
            model->completeReceiveLayoutStartup();
            QVERIFY(!model->receiveLayoutPendingAdmission());
        }
        // Simulated recovery to a radio with only channel0. The real channel
        // admission closes requested B, before any sockets or DSP startup.
        model->configureStreamPool(5, 1, 192000);
        QPointer<RadioModel> alive(model);
        bool retired = false;
        connect(model->sliceById(b), &SliceModel::diversityEnabledChanged, this, [&](bool on) {
            if (on || retired) { return; }
            retired = true;
            core.model.reset();
        });
        model->bindUnboundSlices();
        QVERIFY(retired);
        QVERIFY(!alive);
    }

    void fallback_creation_observer_may_retire_model_data()
    {
        QTest::addColumn<bool>("savedLayoutAdmission");
        QTest::newRow("all-closed") << false;
        QTest::newRow("all-refused") << true;
    }

    void fallback_creation_observer_may_retire_model()
    {
        QFETCH(bool, savedLayoutAdmission);
        Core core;
        auto* model = core.model.get();
        // Restore only E offline, then discover a software Metis resource
        // set supporting A-C. Both admission paths must first create A.
        ReceiveLayoutStore::LoadResult layout;
        layout.state = ReceiveLayoutStore::LoadState::Loaded;
        layout.slices = {{4, QStringLiteral("pan-0"), 14293200.0, DSPMode::USB}};
        QString error;
        QVERIFY2(model->hydrateReceiveLayout("AA:BB:CC:DD:EE:31", layout, &error), qPrintable(error));
        QVERIFY(model->sliceById(4));
        QVERIFY(!model->sliceById(0));
        if (savedLayoutAdmission) {
            model->prepareReceiveLayout("AA:BB:CC:DD:EE:31");
            QVERIFY(model->receiveLayoutPendingAdmission());
        }
        model->setBoardForTest(HPSDRHW::Atlas);
        model->configureStreamPool(3, 3, 192000);
        bool retired = false;
        QPointer<RadioModel> alive(model);
        connect(model, &RadioModel::sliceAdded, this, [&](int id) {
            if (id != 0 || retired) { return; }
            QVERIFY(model->sliceById(4));
            QVERIFY(model->sliceById(0));
            retired = true;
            core.model.reset();
        });
        model->bindUnboundSlices();
        QVERIFY(retired);
        QVERIFY(!alive);
    }

    void post_roster_summary_observer_may_retire_model()
    {
        P2CodecOrionMkII codec;
        Core core;
        auto* model = core.model.get();
        prepareModel(*model, codec);
        model->sliceOwnership()->setOwner(0, SliceOwnership::stationDevice());
        int b;
        {
            SliceOwnership::CreatorScope station(model->sliceOwnership(), SliceOwnership::stationDevice());
            b = model->addSlice("pan-0");
        }
        QCOMPARE(b, 1);
        QVERIFY(model->invokeDiversityAsStationDevice(request(*model, -1, b)).accepted);
        bool retired = false;
        int preRemovalSummaries = 0;
        QPointer<RadioModel> alive(model);
        connect(model, &RadioModel::diversityStateChanged, this, [&] {
            if (retired) { return; }
            if (model->sliceById(b)) { ++preRemovalSummaries; return; }
            QVERIFY(model->sliceById(0));
            QVERIFY(!summary(*model).value("requested").toBool());
            retired = true;
            core.model.reset();
        });
        model->removeSlice(b);
        QVERIFY(preRemovalSummaries > 0); // Initial off is deliberately not the witness.
        QVERIFY(retired);                // Structural publication after takeAt is.
        QVERIFY(!alive);
    }

    void same_id_recreation_keeps_registry_and_access_generation_data()
    {
        QTest::addColumn<bool>("preannounce");
        QTest::addColumn<bool>("beforeConsumers");
        QTest::addColumn<bool>("checkAccess");
        for (bool pre : {true, false}) {
            for (bool before : {true, false}) {
                for (bool access : {false, true}) {
                    const QByteArray name = QByteArray(pre ? "post-detach" : "inside-removed")
                        + (before ? "-before-consumers" : "-after-consumers")
                        + (access ? "-access" : "-lifecycle");
                    QTest::newRow(name.constData()) << pre << before << access;
                }
            }
        }
    }

    void same_id_recreation_keeps_registry_and_access_generation()
    {
        QFETCH(bool, preannounce);
        QFETCH(bool, beforeConsumers);
        QFETCH(bool, checkAccess);
        Local local;
        auto* model = &local.model;
        const int b = local.add();
        QCOMPARE(b, 1);
        QVERIFY(model->sliceById(b));
        QVERIFY(model->invokeDiversityAsStationDevice(request(*model, -1, b)).accepted);
        const quint64 originalIncarnation = model->sliceOwnership()->incarnation(b);
        const QPointer<SliceModel> original(model->sliceById(b));
        const QByteArray nextOwner("replacement-controller");
        const QString nextOwnerWire = QString::fromLatin1(nextOwner.toHex());
        StateMirror mirror;
        mirror.attachSession(); // Real delta path, no transport or peer.
        QStringList events;
        bool attempted = false;
        bool oldRemovedSeen = false;
        bool mixedOldAccess = false;
        int returned = -1;
        quint64 newIncarnation = 0;
        QPointer<SliceModel> replacement;
        QPointer<SliceAccess> oldAccess;
        const auto recreate = [&] {
            if (attempted) { return; }
            attempted = true;
            events.append("replacement-call");
            SliceOwnership::CreatorScope creator(model->sliceOwnership(), nextOwner);
            returned = model->addSlice("pan-0");
            replacement = model->sliceById(b);
            newIncarnation = model->sliceOwnership()->incarnation(b);
            events.append(QString("replacement-return:%1:%2").arg(returned).arg(newIncarnation));
        };
        const auto connectRecreator = [&] {
            if (preannounce) {
                connect(model, &RadioModel::diversityStateChanged, this, [&](const QString&) {
                    if (!oldRemovedSeen && !model->sliceById(b)) { recreate(); }
                });
            } else {
                connect(model, &RadioModel::sliceRemoved, this, [&](int id) {
                    if (id == b) { recreate(); }
                });
            }
        };
        connect(model, &RadioModel::sliceAdded, this, [&](int id) {
            if (id == b) { events.append("slice-added"); }
        });
        connect(model, &RadioModel::sliceRemoved, this, [&](int id) {
            if (id == b) { oldRemovedSeen = true; events.append("slice-removed"); }
        });
        if (beforeConsumers) { connectRecreator(); }
        ObjectRegistry registry(model, &mirror);
        SliceAccessSet access(model, &mirror, [model](int id) {
            // Production ownership-backed fields, not a stubbed access policy.
            const auto* ownership = model->sliceOwnership();
            SliceAccess::Fields fields;
            fields.controllerDeviceId = QString::fromLatin1(ownership->mark(id).owner.toHex());
            fields.controlRevision = qint64(ownership->controlRevision(id));
            QJsonArray listeners;
            QJsonArray receiving;
            for (const QByteArray& owner : ownership->listenersOf(id)) {
                const auto wire = QString::fromLatin1(owner.toHex());
                listeners.append(wire);
                if (ownership->activeRxFor(owner) == id) { receiving.append(wire); }
            }
            fields.listenerDeviceIds = QString::fromUtf8(QJsonDocument(listeners).toJson(QJsonDocument::Compact));
            fields.activeRxDeviceIds = QString::fromUtf8(QJsonDocument(receiving).toJson(QJsonDocument::Compact));
            const auto* slice = model->sliceById(id);
            fields.txSelected = slice && slice->txSliceMarked();
            return fields; // No on-air activity in this software fixture.
        });
        connect(&registry, &ObjectRegistry::objectCreated, this,
                [&](const QByteArray& key, const QByteArray&, int id, const QList<MirrorUpdate>&) {
            if (id == b) { events.append("create:" + QString::fromLatin1(key)); }
        });
        connect(&registry, &ObjectRegistry::objectDestroyed, this,
                [&](const QByteArray& key, const QByteArray&, int id) {
            if (id == b) { events.append("destroy:" + QString::fromLatin1(key)); }
        });
        connect(&access, &SliceAccessSet::accessCreated, this,
                [&](const QByteArray& key, const QByteArray&, const QList<MirrorUpdate>&) {
            if (key == SliceAccessSet::keyFor(b)) { events.append("create:" + QString::fromLatin1(key)); }
        });
        connect(&access, &SliceAccessSet::accessDestroyed, this,
                [&](const QByteArray& key, const QByteArray&) {
            if (key == SliceAccessSet::keyFor(b)) { events.append("destroy:" + QString::fromLatin1(key)); }
        });
        registry.backfillExistingSlices();
        access.backfill();
        oldAccess = access.access(b);
        QVERIFY(oldAccess);
        QCOMPARE(oldAccess->incarnation(), qint64(originalIncarnation));
        connect(oldAccess, &SliceAccess::controllerDeviceIdChanged, this, [&] {
            if (oldAccess && oldAccess->controllerDeviceId() == nextOwnerWire
                && oldAccess->incarnation() == qint64(originalIncarnation)) {
                mixedOldAccess = true;
                events.append(QString("mixed-access:old=%1:current=%2")
                    .arg(originalIncarnation).arg(model->sliceOwnership()->incarnation(b)));
            }
        });
        if (!beforeConsumers) { connectRecreator(); }
        events.clear();
        model->removeSlice(b);
        qInfo().noquote() << "LIFECYCLE_TRACE" << events.join(" -> ")
                         << "registry-live" << registry.isLive(b)
                         << "mirror-new" << (mirror.watchedObject(ObjectRegistry::keyForSlice(b)) == replacement.data())
                         << "access-present" << bool(access.access(b)) << "mixed-old-access" << mixedOldAccess;
        QVERIFY(attempted);
        QCOMPARE(returned, b); // Supported synchronous same-ID creation API.
        QVERIFY(replacement && replacement != original);
        QCOMPARE(model->sliceById(b), replacement.data());
        QVERIFY(newIncarnation != 0 && newIncarnation != originalIncarnation);
        QCOMPARE(model->sliceOwnership()->incarnation(b), newIncarnation);
        QCOMPARE(model->sliceOwnership()->mark(b).owner, nextOwner);
        if (checkAccess) {
            QVERIFY2(!mixedOldAccess, qPrintable(events.join(" -> ")));
            QVERIFY(access.access(b));
            QCOMPARE(access.access(b)->incarnation(), qint64(newIncarnation));
            QCOMPARE(access.access(b)->controllerDeviceId(), nextOwnerWire);
        } else {
            QVERIFY2(registry.isLive(b), qPrintable(events.join(" -> ")));
            QCOMPARE(mirror.watchedObject(ObjectRegistry::keyForSlice(b)), replacement.data());
            QCOMPARE(events.count("destroy:slice:1"), 1);
            QCOMPARE(events.count("create:slice:1"), 1);
            QCOMPARE(events.count("destroy:access:1"), 1);
            QCOMPARE(events.count("create:access:1"), 1);
            QVERIFY(events.indexOf("destroy:slice:1") < events.indexOf("create:slice:1"));
            QVERIFY(events.indexOf("destroy:access:1") < events.indexOf("create:access:1"));
        }
    }

    void unpublished_replacements_coalesce_after_entire_old_removal_dispatch()
    {
        Local local;
        auto& model = local.model;
        const int b = local.add();
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, -1, b)).accepted);
        QStringList events;
        bool attempted = false, oldTailReturned = false;
        quint64 intermediate = 0, surviving = 0;
        connect(&model, &RadioModel::diversityStateChanged, this, [&] {
            if (attempted || model.sliceById(b)) { return; }
            attempted = true;
            QCOMPARE(local.add(), b); // Synchronous ID and ownership, no announcement yet.
            intermediate = model.sliceOwnership()->incarnation(b);
            model.removeSlice(b);
            QCOMPARE(local.add(), b);
            surviving = model.sliceOwnership()->incarnation(b);
        });
        connect(&model, &RadioModel::sliceRemoved, this, [&](int id) {
            if (id == b) { events.append("old-removed-first"); }
        });
        connect(&model, &RadioModel::sliceAdded, this, [&](int id) {
            if (id == b) {
                QVERIFY(oldTailReturned);
                QCOMPARE(model.sliceOwnership()->incarnation(b), surviving);
                events.append("surviving-added");
            }
        });
        connect(&model, &RadioModel::sliceRemoved, this, [&](int id) {
            if (id == b) { events.append("old-removed-last"); oldTailReturned = true; }
        });
        model.removeSlice(b);
        QVERIFY(attempted);
        QVERIFY(intermediate != 0 && surviving != 0 && intermediate != surviving);
        QCOMPARE(events, QStringList({"old-removed-first", "old-removed-last", "surviving-added"}));
        QCOMPARE(model.sliceOwnership()->incarnation(b), surviving);
        QCOMPARE(model.sliceOwnership()->mark(b).owner, SliceOwnership::stationDevice());
    }

    void disposal_before_pending_flush_and_during_surviving_announcement_data()
    {
        QTest::addColumn<bool>("duringAnnouncement");
        QTest::newRow("before-flush") << false;
        QTest::newRow("during-publish") << true;
    }
    void disposal_before_pending_flush_and_during_surviving_announcement()
    {
        QFETCH(bool, duringAnnouncement);
        P2CodecOrionMkII codec;
        QPointer<RadioModel> model(new RadioModel);
        prepareModel(*model, codec);
        model->addSlice("pan-0");
        const int b = model->addSlice("pan-0");
        // Explicit existing station ownership only for the initial admitted
        // route. Replacement is an ordinary unclaimed software receiver.
        model->sliceOwnership()->setOwner(b, SliceOwnership::stationDevice());
        QVERIFY(model->invokeDiversityAsStationDevice(request(*model, -1, b)).accepted);
        bool attempted = false;
        connect(model, &RadioModel::diversityStateChanged, this, [&] {
            if (attempted || !model || model->sliceById(b)) { return; }
            attempted = true;
            QCOMPARE(model->addSlice("pan-0"), b);
        });
        connect(model, &RadioModel::sliceRemoved, this, [&](int id) {
            if (!duringAnnouncement && id == b) { delete model.data(); }
        });
        connect(model, &RadioModel::sliceAdded, this, [&](int id) {
            if (duringAnnouncement && id == b) { delete model.data(); }
        });
        model->removeSlice(b);
        QVERIFY(attempted);
        QVERIFY(!model);
    }

    void begin_remove_nested_close_preserves_new_last_slice()
    {
        P2CodecOrionMkII codec;
        Core core;
        auto* model = core.model.get();
        prepareModel(*model, codec);
        model->sliceOwnership()->setOwner(0, SliceOwnership::stationDevice());
        int b;
        {
            SliceOwnership::CreatorScope station(model->sliceOwnership(), SliceOwnership::stationDevice());
            b = model->addSlice("pan-0");
        }
        QCOMPARE(b, 1);
        const quint64 originalIncarnation = model->sliceOwnership()->incarnation(0);
        const auto originalMark = model->sliceOwnership()->mark(0);
        const auto originalListeners = model->sliceOwnership()->listenersOf(0);
        const quint64 originalRevision = model->sliceOwnership()->controlRevision(0);
        bool nested = false;
        connect(model->sliceOwnership(), &SliceOwnership::activeChanged, this, [&] {
            if (nested || model->sliceOwnership()->incarnation(0) != 0) { return; }
            nested = true; // A is now in removal; B's normal close still has two rows.
            model->removeSlice(b);
        });
        model->removeSlice(0);
        QVERIFY(nested);
        QCOMPARE(model->slices().size(), 1);
        QVERIFY(model->sliceById(0));
        QVERIFY(!model->sliceById(b));
        QCOMPARE(model->sliceOwnership()->incarnation(0), originalIncarnation);
        QCOMPARE(model->sliceOwnership()->mark(0).owner, originalMark.owner);
        QCOMPARE(model->sliceOwnership()->listenersOf(0), originalListeners);
        QCOMPARE(model->sliceOwnership()->controlRevision(0), originalRevision);
        QVERIFY(model->sliceOwnership()->isLive(0));
    }

    void slice_removed_recreation_keeps_new_ownership_record()
    {
        P2CodecOrionMkII codec;
        Core core;
        auto* model = core.model.get();
        prepareModel(*model, codec);
        model->sliceOwnership()->setOwner(0, SliceOwnership::stationDevice());
        int b;
        {
            SliceOwnership::CreatorScope station(model->sliceOwnership(), SliceOwnership::stationDevice());
            b = model->addSlice("pan-0");
        }
        QCOMPARE(b, 1);
        const quint64 oldIncarnation = model->sliceOwnership()->incarnation(b);
        const QPointer<SliceModel> oldVictim(model->sliceById(b));
        const QByteArray replacementOwner("replacement-controller");
        quint64 replacementIncarnation = 0;
        QPointer<SliceModel> replacement;
        connect(model, &RadioModel::sliceRemoved, this, [&](int id) {
            if (id != b || replacement) { return; }
            SliceOwnership::CreatorScope creator(model->sliceOwnership(), replacementOwner);
            QCOMPARE(model->addSlice("pan-0"), b);
            replacement = model->sliceById(b);
            replacementIncarnation = model->sliceOwnership()->incarnation(b);
            QVERIFY(replacementIncarnation != 0 && replacementIncarnation != oldIncarnation);
        });
        model->removeSlice(b);
        QVERIFY(replacement);
        QVERIFY(replacement != oldVictim);
        QCOMPARE(model->sliceById(b), replacement.data());
        QCOMPARE(model->sliceOwnership()->incarnation(b), replacementIncarnation);
        QCOMPARE(model->sliceOwnership()->mark(b).owner, replacementOwner);
        QCOMPARE(model->sliceOwnership()->listenersOf(b), QList<QByteArray>{replacementOwner});
        QVERIFY(model->sliceOwnership()->isLive(b));
    }

    void removal_notification_observer_may_retire_model_data()
    {
        QTest::addColumn<QByteArray>("boundary");
        for (const QByteArray name : {QByteArray("stream-index"), QByteArray("stream-epoch"),
             QByteArray("stream-pin"), QByteArray("stream-bindings"), QByteArray("ddc-request"),
             QByteArray("active-flag"), QByteArray("begin-remove"), QByteArray("tx-flag")}) {
            QTest::newRow(name.constData()) << name;
        }
    }

    void removal_notification_observer_may_retire_model()
    {
        QFETCH(QByteArray, boundary);
        P2CodecOrionMkII codec;
        Core core;
        auto* model = core.model.get();
        prepareModel(*model, codec);
        model->sliceOwnership()->setOwner(0, SliceOwnership::stationDevice());
        int b;
        {
            SliceOwnership::CreatorScope station(model->sliceOwnership(), SliceOwnership::stationDevice());
            b = model->addSlice("pan-0");
        }
        QCOMPARE(b, 1);
        const int victimId = boundary == "tx-flag" || boundary == "active-flag" ? 0 : b;
        auto* victim = model->sliceById(victimId);
        QVERIFY(model->invokeDiversityAsStationDevice(request(*model, -1, victimId)).accepted);
        QVERIFY(victim->streamIndex() >= 0);
        QVERIFY(victim->streamEpoch() != 0);
        if (boundary == "stream-pin") { victim->setStreamCtunPinned(true); }
        bool retired = false;
        QPointer<RadioModel> alive(model);
        const auto retire = [&] {
            if (retired) { return; }
            retired = true;
            core.model.reset();
        };
        if (boundary == "stream-index") {
            connect(victim, &SliceModel::streamIndexChanged, this, [&](int stream) {
                if (stream < 0) { QVERIFY(!model->sliceById(victimId)); retire(); }
            });
        } else if (boundary == "stream-epoch") {
            connect(victim, &SliceModel::streamEpochChanged, this, [&](quint64 epoch) {
                if (epoch == 0) { QVERIFY(!model->sliceById(victimId)); retire(); }
            });
        } else if (boundary == "stream-pin") {
            connect(victim, &SliceModel::streamCtunPinnedChanged, this, [&](bool pinned) {
                if (!pinned) { QVERIFY(!model->sliceById(victimId)); retire(); }
            });
        } else if (boundary == "stream-bindings") {
            connect(model, &RadioModel::streamBindingsChanged, this, [&](int, const QVector<int>&) {
                if (!model->sliceById(victimId)) { retire(); }
            });
        } else if (boundary == "ddc-request") {
            connect(model, &RadioModel::ddcAssignmentRequested, this, [&] {
                if (!model->sliceById(victimId)) { retire(); }
            });
        } else if (boundary == "begin-remove") {
            connect(model->sliceOwnership(), &SliceOwnership::activeChanged, this, [&] {
                if (model->sliceById(victimId) && !model->sliceOwnership()->isLive(victimId)) { retire(); }
            });
        } else if (boundary == "tx-flag") {
            QVERIFY(victim->isTxSlice());
            connect(victim, &SliceModel::txSliceChanged, this, [&](bool bound) { if (!bound) { retire(); } });
        } else {
            QVERIFY(victim->isActiveSlice());
            connect(victim, &SliceModel::activeChanged, this, [&](bool active) { if (!active) { retire(); } });
        }
        model->removeSlice(victimId);
        QVERIFY(retired);
        QVERIFY(!alive);
    }

    void binding_notification_observer_may_retire_model_data()
    {
        QTest::addColumn<QByteArray>("boundary");
        for (const QByteArray name : {QByteArray("receiver-created"), QByteArray("receiver-activated"),
             QByteArray("hardware-mapping"), QByteArray("stream-centre"), QByteArray("stream-index")}) {
            QTest::newRow(name.constData()) << name;
        }
    }

    void binding_notification_observer_may_retire_model()
    {
        QFETCH(QByteArray, boundary);
        Core core;
        auto* model = core.model.get();
        model->setBoardForTest(HPSDRHW::OrionMKII);
        model->configureStreamPool(5, 5, 192000);
        model->receiverManager()->setMaxReceivers(5);
        QVERIFY(model->sliceById(0));
        QCOMPARE(model->sliceById(0)->streamIndex(), -1);
        bool retired = false;
        QPointer<RadioModel> alive(model);
        const auto retire = [&] {
            if (retired) { return; }
            retired = true;
            core.model.reset();
        };
        if (boundary == "receiver-created") {
            connect(model->receiverManager(), &ReceiverManager::receiverCreated, this, [&](int) { retire(); });
        } else if (boundary == "receiver-activated") {
            connect(model->receiverManager(), &ReceiverManager::receiverActivated, this, [&](int) { retire(); });
        } else if (boundary == "hardware-mapping") {
            connect(model->receiverManager(), &ReceiverManager::hardwareSlotsChanged, this, [&](quint32) { retire(); });
        } else if (boundary == "stream-centre") {
            connect(model, &RadioModel::streamCentreChanged, this, [&](int, double, int) { retire(); });
        } else {
            connect(model->sliceById(0), &SliceModel::streamIndexChanged, this, [&](int stream) {
                if (stream >= 0) { retire(); }
            });
        }
        model->bindUnboundSlices();
        QVERIFY(retired);
        QVERIFY(!alive);
    }

    void binding_snapshot_does_not_activate_removed_participant()
    {
        Core core;
        auto* model = core.model.get();
        const int second = model->addSlice("pan-1");
        QVERIFY(second > 0);
        QPointer<SliceModel> removed(model->sliceById(second));
        removed->setFrequency(7100000.0);
        model->setBoardForTest(HPSDRHW::OrionMKII);
        model->configureStreamPool(5, 5, 192000);
        model->receiverManager()->setMaxReceivers(5);
        bool closed = false;
        connect(model->sliceById(0), &SliceModel::streamIndexChanged, this, [&](int stream) {
            if (stream < 0 || closed) { return; }
            closed = true;
            model->removeSlice(second);
        });
        model->bindUnboundSlices();
        QVERIFY(closed);
        QVERIFY(removed); // deleteLater: a weak snapshot alone is still live.
        QVERIFY(!model->sliceById(second));
        QCOMPARE(model->sliceById(0)->streamIndex(), 0);
        QCOMPARE(removed->streamIndex(), -1);
        QCOMPARE(model->activeStreamCount(), 1);
    }

    void admitted_assignment_mapping_observer_may_retire_model()
    {
        P2CodecOrionMkII codec;
        Core core;
        auto* model = core.model.get();
        prepareModel(*model, codec);
        model->sliceOwnership()->setOwner(0, SliceOwnership::stationDevice());
        const auto invoke = request(*model, -1, 0);
        bool retired = false;
        QPointer<RadioModel> alive(model);
        connect(model->receiverManager(), &ReceiverManager::hardwareSlotsChanged, this, [&](quint32) {
            if (retired) { return; }
            retired = true;
            core.model.reset();
        });
        const auto result = model->invokeDiversityAsStationDevice(invoke);
        QVERIFY(retired);
        QVERIFY(!alive);
        QVERIFY(!result.accepted);
        QVERIFY(result.updates.isEmpty());
    }

    void arbiter_sync_notification_observer_may_retire_model_data()
    {
        QTest::addColumn<bool>("normalizing");
        QTest::newRow("initial-binding") << false;
        QTest::newRow("multiple-flags") << true;
    }

    void arbiter_sync_notification_observer_may_retire_model()
    {
        QFETCH(bool, normalizing);
        P2CodecOrionMkII codec;
        Core core;
        auto* model = core.model.get();
        prepareModel(*model, codec);
        int b = model->addSlice("pan-0");
        QCOMPARE(b, 1);
        model->sliceById(0)->setTxSlice(normalizing);
        model->sliceById(b)->setTxSlice(normalizing);
        const int observedId = normalizing ? b : 0;
        bool retired = false;
        QPointer<RadioModel> alive(model);
        connect(model->sliceById(observedId), &SliceModel::txSliceChanged, this, [&](bool bound) {
            if (retired || bound == normalizing) { return; }
            retired = true;
            core.model.reset();
        });
        model->txSliceArbiter()->syncToSliceList();
        QVERIFY(retired);
        QVERIFY(!alive);
    }

    void remote_summary_retiring_server_does_not_deliver_a_late_result()
    {
        P2CodecOrionMkII codec;
        Core core;
        prepareModel(*core.model, codec);
        Device owner;
        core.pair(owner);
        auto* app = core.signIn(owner, kControl);
        QVERIFY(admitted(app));
        QCOMPARE(core.model->sliceOwnership()->mark(0).owner, owner.key.fingerprint());
        const auto invoke = request(*core.model, -1, 0, 711);
        const QPointer<StationServer> server(core.server.get());
        bool retired = false;
        connect(core.model.get(), &RadioModel::diversityStateChanged, this,
                [&](const QString& state) {
            if (retired || !QJsonDocument::fromJson(state.toUtf8()).object()
                                .value("requested").toBool()) { return; }
            retired = true;
            core.server.reset();
        }, Qt::DirectConnection);
        app->sendText(SessionMessages::encode(invoke));
        QTRY_VERIFY(retired);
        QVERIFY(!server);
        QVERIFY(core.model);
        for (const auto& reply : ofType(app->received(), QStringLiteral("command.result"))) {
            QVERIFY(reply.value("id").toInteger() != invoke.commandId);
        }
    }

    void nested_station_refusal_keeps_remote_diversity_result_and_requester()
    {
        P2CodecOrionMkII codec;
        Core core;
        prepareModel(*core.model, codec);
        Device owner;
        core.pair(owner);
        auto* app = core.signIn(owner, kControl);
        QVERIFY(admitted(app));
        const quint32 id = core.nextCommandId;
        bool nested = false;
        SessionMessage nestedResult;
        connect(core.model.get(), &RadioModel::diversityStateChanged, this,
                [&](const QString& state) {
            if (nested || !QJsonDocument::fromJson(state.toUtf8()).object()
                               .value("requested").toBool()) { return; }
            nested = true;
            nestedResult = hosted(*core.server, request(*core.model, 0, -1, id));
        }, Qt::DirectConnection);
        const auto result = core.invoke(app, "diversity.setTarget",
                                        request(*core.model, -1, 0).arguments);
        QVERIFY(nested);
        QCOMPARE(nestedResult.kind, SessionMessageKind::CommandResult);
        QCOMPARE(nestedResult.commandId, id);
        QVERIFY(!nestedResult.accepted);
        QCOMPARE(value(nestedResult, "reasonCode").toString(),
                 QStringLiteral("sourceControlRequired"));
        QVERIFY(result.value("accepted").toBool());
        QCOMPARE(result.value("id").toInteger(), qint64(id));
        int replies = 0;
        for (const auto& reply : ofType(app->received(), QStringLiteral("command.result"))) {
            if (reply.value("id").toInteger() == id) { ++replies; }
        }
        QCOMPARE(replies, 1);
        QVERIFY(summary(*core.model).value("requested").toBool());
        QVERIFY(core.invoke(app, "diversity.setTarget",
                            request(*core.model, 0, -1).arguments).value("accepted").toBool());
        QVERIFY(!summary(*core.model).value("requested").toBool());
    }

    void actual_fresh_nonhosting_unclaimed_A_is_not_implicitly_adopted()
    {
        P2CodecOrionMkII codec;
        RadioModel model;
        prepareModel(model, codec);
        // Actual nonhosting addSlice has no CreatorScope. The entry must not
        // paper over that ownership gap with an automatic claim.
        QCOMPARE(model.addSlice("pan-0"), 0);
        QVERIFY(model.sliceOwnership()->mark(0).owner.isEmpty());
        QVERIFY(model.sliceOwnership()->listenersOf(0).isEmpty());
        const auto before = summary(model);
        const auto result = model.invokeDiversityAsStationDevice(request(model, -1, 0));
        QVERIFY(!result.accepted);
        QCOMPARE(value(result, "reasonCode").toString(), QStringLiteral("targetControlRequired"));
        QCOMPARE(summary(model), before);
        QVERIFY(model.sliceOwnership()->mark(0).owner.isEmpty());
        QVERIFY(model.sliceOwnership()->listenersOf(0).isEmpty());
        QVERIFY(model.stationLink() == nullptr);
        QVERIFY(model.findChildren<StationServer*>().isEmpty());
    }

    // Genuine new-path causal case: the retained signature-only baseline
    // safely refuses at the first accepted assertion, with all setup valid.
    void nonhosting_station_owned_A_enable_reaffirm_off_and_B_C_move()
    {
        Local local;
        auto& model = local.model;
        QVERIFY(model.stationLink() == nullptr);
        QVERIFY(model.findChildren<StationServer*>().isEmpty());
        QCOMPARE(model.sliceOwnership()->mark(0).owner, SliceOwnership::stationDevice());
        const int b = local.add(), c = local.add();
        QCOMPARE(b, 1); QCOMPARE(c, 2);
        model.sliceById(0)->setDiversityPhaseDeg(25);
        model.sliceById(b)->setDiversityPhaseDeg(120);
        model.sliceById(c)->setDiversityGainDb(-7);
        const auto listeners = model.sliceOwnership()->listenersOf(b);
        const auto enabled = model.invokeDiversityAsStationDevice(request(model, -1, 0));
        QVERIFY2(enabled.accepted, qPrintable(enabled.reason));
        QCOMPARE(enabled.kind, SessionMessageKind::CommandResult);
        QCOMPARE(enabled.commandVerb, QByteArray("diversity.setTarget"));
        QCOMPARE(enabled.commandId, quint32(701));
        QCOMPARE(enabled.affectedKeys, QList<QByteArray>({"radio", "slice:0"}));
        QCOMPARE(value(enabled, "diversityState").toString(), model.diversityState());
        const auto beforeReaffirm = summary(model);
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, 0, 0)).accepted);
        QCOMPARE(summary(model), beforeReaffirm);
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, 0, -1)).accepted);
        QVERIFY(summary(model).value("live").isNull());
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, -1, b)).accepted);
        const auto moved = model.invokeDiversityAsStationDevice(request(model, b, c));
        QVERIFY(moved.accepted);
        QCOMPARE(moved.reason, QStringLiteral("Diversity moved from B to C. Both slices paused briefly."));
        QCOMPARE(moved.affectedKeys, QList<QByteArray>({"radio", "slice:1", "slice:2"}));
        QCOMPARE(model.diversityTargetSlice(), model.sliceById(c));
        QCOMPARE(model.sliceById(0)->diversityPhaseDeg(), 25.0);
        QCOMPARE(model.sliceById(b)->diversityPhaseDeg(), 120.0);
        QCOMPARE(model.sliceById(c)->diversityGainDb(), -7.0);
        QVERIFY(!model.sliceById(b)->diversityEnabled());
        QCOMPARE(model.sliceOwnership()->listenersOf(b), listeners);
        QVERIFY(model.stationLink() == nullptr);
        QVERIFY(model.findChildren<StationServer*>().isEmpty());
    }

    void hosted_and_detached_station_results_match_exactly()
    {
        P2CodecOrionMkII codec;
        Core core;
        prepareModel(*core.model, codec);
        core.model->sliceOwnership()->setOwner(0, SliceOwnership::stationDevice());
        QVERIFY(!core.server->isListening());
        const auto on = request(*core.model, -1, 0);
        QVERIFY(hosted(*core.server, on).accepted);
        const auto reaffirm = request(*core.model, 0, 0, 702);
        const auto throughServer = hosted(*core.server, reaffirm);
        const auto localHosted = core.model->invokeDiversityAsStationDevice(reaffirm);
        QCOMPARE(SessionMessages::encode(localHosted), SessionMessages::encode(throughServer));
        core.server.reset();
        QVERIFY(!core.model->moxController()->hasKeyingGate());
        const auto detached = core.model->invokeDiversityAsStationDevice(reaffirm);
        QVERIFY(detached.accepted);
        QCOMPARE(SessionMessages::encode(detached), SessionMessages::encode(throughServer));
    }

    void remote_uses_same_participant_body_and_negotiated_pattern()
    {
        P2CodecOrionMkII codec;
        Core core;
        prepareModel(*core.model, codec);
        Device owner;
        core.pair(owner);
        auto* app = core.signIn(owner, kControl);
        QVERIFY(admitted(app));
        auto invoke = request(*core.model, -1, 0);
        const auto accepted = core.invoke(app, invoke.commandVerb, invoke.arguments);
        QVERIFY(accepted.value("accepted").toBool());
        QCOMPARE(core.model->diversityTargetSlice(), core.model->sliceById(0));
        SessionMessage wireResult;
        QVERIFY(SessionMessages::decode(QJsonDocument(accepted).toJson(QJsonDocument::Compact), &wireResult));
        const auto state = QJsonDocument::fromJson(value(wireResult, "diversityState").toString().toUtf8()).object();
        QCOMPARE(state, QJsonDocument::fromJson(core.model->diversityStateForPeer(false).toUtf8()).object());
        QTRY_COMPARE(latest(app->received(), "radio", "diversityState").toString(),
                     core.model->diversityStateForPeer(false));
        QVERIFY(state.value("live").toObject().value("pattern").isNull());
        const auto before = summary(*core.model);
        const auto station = core.model->invokeDiversityAsStationDevice(request(*core.model, 0, -1));
        QVERIFY(!station.accepted);
        QCOMPARE(value(station, "reasonCode").toString(), QStringLiteral("sourceControlRequired"));
        QCOMPARE(summary(*core.model), before);
    }

    void unclaimed_foreign_listener_and_held_for_are_not_promoted()
    {
        Local local;
        auto& model = local.model;
        const int b = local.add();
        auto* own = model.sliceOwnership();
        own->setOwner(b, {});
        QVERIFY(own->leave(SliceOwnership::stationDevice(), b));
        const auto unclaimed = model.invokeDiversityAsStationDevice(request(model, -1, b));
        QVERIFY(!unclaimed.accepted);
        QCOMPARE(value(unclaimed, "reasonCode").toString(), QStringLiteral("targetControlRequired"));
        QCOMPARE(own->mark(b).owner, QByteArray());
        QVERIFY(own->listenersOf(b).isEmpty());
        const QByteArray foreign("foreign-controller");
        own->setOwner(b, foreign);
        const auto before = summary(model);
        const auto refused = model.invokeDiversityAsStationDevice(request(model, -1, b));
        QVERIFY(!refused.accepted);
        QCOMPARE(summary(model), before);
        QVERIFY(own->join(SliceOwnership::stationDevice(), b));
        const auto listener = model.invokeDiversityAsStationDevice(request(model, -1, b));
        QVERIFY(!listener.accepted);
        QCOMPARE(listener.reason, QStringLiteral("Slice B is controlled by another device. Take control to change it."));
        own->hold(b, foreign);
        // The actual controller remains station while held for an absent
        // device; heldFor does not give that remote device control.
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, -1, b)).accepted);
        P2CodecOrionMkII codec;
        Core core;
        prepareModel(*core.model, codec);
        Device away;
        core.pair(away);
        auto* app = core.signIn(away, kControl);
        QVERIFY(admitted(app));
        core.model->sliceOwnership()->hold(0, away.key.fingerprint());
        const auto remote = request(*core.model, -1, 0);
        const auto heldFor = core.invoke(app, remote.commandVerb, remote.arguments);
        QVERIFY(!heldFor.value("accepted").toBool());
        QCOMPARE(core.model->sliceOwnership()->mark(0).heldFor, away.key.fingerprint());
        core.model->sliceOwnership()->setOwner(0, {});
        core.model->sliceOwnership()->leave(SliceOwnership::stationDevice(), 0);
        core.model->sliceOwnership()->leave(away.key.fingerprint(), 0);
        const auto hostedUnclaimed = hosted(*core.server, request(*core.model, -1, 0));
        QCOMPARE(hostedUnclaimed.reason, unclaimed.reason);
        QCOMPARE(value(hostedUnclaimed, "reasonCode"), value(unclaimed, "reasonCode"));
        QVERIFY(!hostedUnclaimed.accepted);
    }

    void current_state_both_incarnations_and_control_revisions_are_required()
    {
        Local local;
        auto& model = local.model;
        int b = local.add();
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, -1, 0)).accepted);
        auto wrongSource = request(model, 0, b);
        for (auto& field : wrongSource.arguments) {
            if (field.name == "sourceIncarnation") { field.value = field.value.toLongLong() + 1; }
        }
        const auto before = summary(model);
        const auto source = model.invokeDiversityAsStationDevice(wrongSource);
        QVERIFY(!source.accepted);
        QCOMPARE(value(source, "reasonCode").toString(), QStringLiteral("sourceChanged"));
        QCOMPARE(summary(model), before);
        auto staleSourceControl = request(model, 0, b);
        model.sliceOwnership()->setOwner(0, "other-source");
        model.sliceOwnership()->setOwner(0, SliceOwnership::stationDevice());
        currentRevision(staleSourceControl, model);
        const auto sourceControl = model.invokeDiversityAsStationDevice(staleSourceControl);
        QVERIFY(!sourceControl.accepted);
        QCOMPARE(value(sourceControl, "reasonCode").toString(), QStringLiteral("sourceChanged"));
        auto staleTarget = request(model, 0, b);
        model.removeSlice(b);
        QCOMPARE(local.add(), b);
        currentRevision(staleTarget, model);
        const auto target = model.invokeDiversityAsStationDevice(staleTarget);
        QVERIFY(!target.accepted);
        QCOMPARE(value(target, "reasonCode").toString(), QStringLiteral("targetChanged"));
        auto staleControl = request(model, 0, b);
        model.sliceOwnership()->setOwner(b, "other");
        model.sliceOwnership()->setOwner(b, SliceOwnership::stationDevice());
        currentRevision(staleControl, model);
        const auto control = model.invokeDiversityAsStationDevice(staleControl);
        QVERIFY(!control.accepted);
        QCOMPARE(value(control, "reasonCode").toString(), QStringLiteral("targetChanged"));
        auto staleState = request(model, 0, b);
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, 0, -1)).accepted);
        const auto off = summary(model);
        QVERIFY(!model.invokeDiversityAsStationDevice(staleState).accepted);
        QCOMPARE(summary(model), off);
    }

    void station_key_freezes_only_its_bound_participants_data()
    {
        QTest::addColumn<bool>("withServer");
        QTest::newRow("nonhosting") << false;
        QTest::newRow("hosted") << true;
    }
    void station_key_freezes_only_its_bound_participants()
    {
        QFETCH(bool, withServer);
        Local local;
        auto& model = local.model;
        QTemporaryDir settingsDir, securityDir;
        AppSettings settings(settingsDir.filePath("test.settings"));
        std::unique_ptr<StationServer> server;
        if (withServer) {
            server = std::make_unique<StationServer>(&model, settings,
                NereusSDR::Test::withSharedTlsIdentity(NereusSDR::Test::seedCoreIdentity(securityDir.path())));
            server->setHeartbeatIntervalMs(0);
            server->setRemoteTransmitAllowed(true);
        }
        const int b = local.add(), c = local.add();
        QVERIFY(model.requestTxHandoffToSlice(0));
        QCOMPARE(model.txBoundSlice(), model.sliceById(0));
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, -1, 0)).accepted);
        model.transmitModel().setMicSourceLocked(false);
        model.transmitModel().setMicSource(MicSource::Radio);
        auto* mox = model.moxController();
        mox->setRxOnly(false); mox->setRadioLinkDown(false); mox->setInterlockPolicy(nullptr);
        mox->setMox(true);
        QTRY_VERIFY(mox->isMox());
        QVERIFY(mox->currentKeyer().isStation());
        const auto before = summary(model);
        const auto frozenSource = model.invokeDiversityAsStationDevice(request(model, 0, b));
        QVERIFY(!frozenSource.accepted);
        QCOMPARE(value(frozenSource, "reasonCode").toString(), QStringLiteral("onAir"));
        QCOMPARE(frozenSource.reason, TxRefusals::holderOnAir(QString(), true).text);
        QCOMPARE(summary(model), before);
        QVERIFY(!model.invokeDiversityAsStationDevice(request(model, 0, -1)).accepted);
        mox->setMox(false); QTRY_COMPARE(mox->state(), MoxState::Rx);
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, 0, -1)).accepted);
        mox->setMox(true); QTRY_VERIFY(mox->isMox());
        const auto frozenTarget = model.invokeDiversityAsStationDevice(request(model, -1, 0));
        QVERIFY(!frozenTarget.accepted);
        QCOMPARE(value(frozenTarget, "reasonCode").toString(), QStringLiteral("onAir"));
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, -1, b)).accepted);
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, b, c)).accepted);
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, c, -1)).accepted);
        mox->setMox(false); QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    void replacement_server_cannot_be_cleared_or_authorized_by_retired_server()
    {
        Local local;
        auto& model = local.model;
        const int b = local.add();
        QTemporaryDir settingsDir, firstSecurity, replacementSecurity;
        AppSettings settings(settingsDir.filePath("test.settings"));
        auto first = std::make_unique<StationServer>(&model, settings,
            NereusSDR::Test::withSharedTlsIdentity(NereusSDR::Test::seedCoreIdentity(firstSecurity.path())));
        auto replacement = std::make_unique<StationServer>(&model, settings,
            NereusSDR::Test::withSharedTlsIdentity(NereusSDR::Test::seedCoreIdentity(replacementSecurity.path())));
        first->setHeartbeatIntervalMs(0); replacement->setHeartbeatIntervalMs(0);
        replacement->setRemoteTransmitAllowed(true);
        replacement->setStationDeviceWords("New desktop", "New desktop");
        const auto before = summary(model);
        const auto stale = hosted(*first, request(model, -1, 0));
        QVERIFY(!stale.accepted);
        QCOMPARE(value(stale, "reasonCode").toString(), QStringLiteral("sessionChanged"));
        QCOMPARE(summary(model), before);
        QVERIFY(hosted(*replacement, request(model, -1, b)).accepted);
        QVERIFY(model.setActiveSliceById(b));
        QVERIFY(model.requestTxHandoffToSlice(b));
        model.transmitModel().setMicSourceLocked(false);
        model.transmitModel().setMicSource(MicSource::Radio);
        auto* mox = model.moxController();
        mox->setRxOnly(false); mox->setRadioLinkDown(false); mox->setInterlockPolicy(nullptr);
        mox->setMox(true); QTRY_VERIFY(mox->isMox());
        QCOMPARE(model.txBoundSlice(), model.sliceById(b));
        const auto namedFreeze = model.invokeDiversityAsStationDevice(request(model, b, 0));
        QVERIFY(!namedFreeze.accepted);
        QCOMPARE(namedFreeze.reason, TxRefusals::holderOnAir("New desktop", false).text);
        first.reset();
        QVERIFY(mox->hasKeyingGate());
        const auto retainedFreeze = model.invokeDiversityAsStationDevice(request(model, b, 0));
        QVERIFY(!retainedFreeze.accepted);
        QCOMPARE(retainedFreeze.reason, namedFreeze.reason);
        mox->setMox(false); QTRY_COMPARE(mox->state(), MoxState::Rx);
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, b, b)).accepted);
        QCOMPARE(model.sliceOwnership()->mark(b).owner, SliceOwnership::stationDevice());
        replacement.reset();
        QVERIFY(!model.moxController()->hasKeyingGate());
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, b, 0)).accepted);
    }

    void role_thread_message_shape_and_standalone_dispatcher_are_safe()
    {
        Local local;
        auto& model = local.model;
        const auto invoke = request(model, -1, 0);
        const auto before = summary(model);
        RadioModel remote(RadioModel::Role::Remote);
        QVERIFY(!remote.invokeDiversityAsStationDevice(invoke).accepted);
        SessionMessage offThread;
        std::thread thread([&]() { offThread = model.invokeDiversityAsStationDevice(invoke); });
        thread.join();
        QVERIFY(!offThread.accepted);
        QCOMPARE(summary(model), before);
        for (int i = 0; i < model.metaObject()->methodCount(); ++i) {
            QVERIFY(model.metaObject()->method(i).name() != "invokeDiversityAsStationDevice");
        }
        auto wrong = invoke; wrong.commandVerb = "removeSlice";
        QVERIFY(!model.invokeDiversityAsStationDevice(wrong).accepted);
        wrong = invoke; wrong.kind = SessionMessageKind::PropertyWrite;
        QVERIFY(!model.invokeDiversityAsStationDevice(wrong).accepted);
        wrong = invoke; wrong.arguments.append(wrong.arguments.first());
        const auto malformed = model.invokeDiversityAsStationDevice(wrong);
        QVERIFY(!malformed.accepted);
        QCOMPARE(value(malformed, "reasonCode").toString(), QStringLiteral("invalidRequest"));
        SessionCommandDispatcher dispatcher(&model);
        QSignalSpy results(&dispatcher, &SessionCommandDispatcher::commandResultReady);
        dispatcher.dispatch(invoke);
        QCOMPARE(results.size(), 1);
        QVERIFY(!qvariant_cast<SessionMessage>(results.first().first()).accepted);
        QCOMPARE(summary(model), before);
    }

    void unrelated_foreign_receiver_and_cohosts_survive_local_move()
    {
        Local local;
        auto& model = local.model;
        const int b = local.add(), c = local.add(), d = local.add();
        auto* other = model.sliceById(c);
        auto* cohost = model.sliceById(d);
        other->setFrequency(7100000); cohost->setFrequency(7100000);
        model.sliceOwnership()->setOwner(c, "foreign-one");
        model.sliceOwnership()->setOwner(d, "foreign-two");
        QCOMPARE(other->streamIndex(), cohost->streamIndex());
        const int stream = other->streamIndex();
        const int rate = other->sampleRateHz();
        const auto otherMark = model.sliceOwnership()->mark(c);
        const auto cohostMark = model.sliceOwnership()->mark(d);
        const auto incarnation = model.sliceOwnership()->incarnation(c);
        const auto revision = model.sliceOwnership()->controlRevision(c);
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, -1, 0)).accepted);
        QVERIFY(model.invokeDiversityAsStationDevice(request(model, 0, b)).accepted);
        QCOMPARE(model.sliceById(c), other); QCOMPARE(model.sliceById(d), cohost);
        QCOMPARE(other->streamIndex(), stream); QCOMPARE(cohost->streamIndex(), stream);
        QCOMPARE(other->frequency(), 7100000.0); QCOMPARE(cohost->frequency(), 7100000.0);
        QCOMPARE(other->sampleRateHz(), rate);
        QCOMPARE(model.sliceOwnership()->mark(c), otherMark);
        QCOMPARE(model.sliceOwnership()->mark(d), cohostMark);
        QCOMPARE(model.sliceOwnership()->incarnation(c), incarnation);
        QCOMPARE(model.sliceOwnership()->controlRevision(c), revision);
    }
};
QTEST_MAIN(TestDiversityLocalAdmission)
#include "tst_diversity_local_admission.moc"
