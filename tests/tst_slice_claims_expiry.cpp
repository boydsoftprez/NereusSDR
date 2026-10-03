// =================================================================
// tests/tst_slice_claims_expiry.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. A device's claims at the end of its
// 180 s, at an explicit leave and on its return (slice control and shared
// listening plan Task 8, approved policy 6), and Amendment 8a: a slice of a
// device that is not here never counts in the preselector choice or in a
// question asked of the devices a change would disturb.
//
// Modification history (NereusSDR):
//   2026-09-29: created for NereusSDR by J.J. Boyd (KG4VCF), slice control
//               and shared listening plan Task 8, with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-29: slice control fix wave: a device's last slice expiring
//               while radio PTT keys it closes only after the radio
//               unkeys. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================
//
// A real Core over loopback links, with the session registry's clock
// injected (the harness's `now`). No radio, no RF, nothing keys.

#include "MultiDeviceHarness.h"

#include "core/DdcAssignment.h"
#include "core/ReceiverManager.h"
#include "core/session/ReceiverPlanner.h"
#include "core/TxSliceArbiter.h"
#include "core/MoxController.h"

#include <QSignalSpy>

namespace {

const QHash<QByteArray, int> kShares{{"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 1}};
const QHash<QByteArray, int> kSharesTx{
    {"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 1}, {"remoteTx", 1}};

QString accessKey(int sliceId)
{
    return QStringLiteral("access:%1").arg(sliceId);
}

QList<MirrorUpdate> refArgs(const LoopbackTransport* app, int sliceId)
{
    return {int64("sliceId", sliceId),
            int64("incarnation",
                  latest(app->received(), accessKey(sliceId), QStringLiteral("incarnation"))
                      .toInteger())};
}

QList<MirrorUpdate> revisionArgs(const LoopbackTransport* app, int sliceId)
{
    QList<MirrorUpdate> args = refArgs(app, sliceId);
    args.append(int64("controlRevision",
                      latest(app->received(), accessKey(sliceId),
                             QStringLiteral("controlRevision"))
                          .toInteger()));
    return args;
}

MirrorUpdate flag(const char* name, bool value)
{
    return MirrorUpdate{0, name, MirrorWireKind::Bool, QVariant(value)};
}

bool accepted(const QJsonObject& result)
{
    return result.value(QStringLiteral("accepted")).toBool(false);
}

QString reasonOf(const QJsonObject& result)
{
    return result.value(QStringLiteral("reason")).toString();
}

void drop(Core& core, LoopbackTransport* app, const Device& device)
{
    app->closeLink(QStringLiteral("lost"));
    QTRY_COMPARE(core.sessions().entry(device.key.fingerprint())->state,
                 DeviceSessionRegistry::State::Away);
}

int onlySliceOf(const Core& core, const Device& device)
{
    const QList<int> owned = core.model->sliceOwnership()->ownedBy(device.key.fingerprint());
    return owned.size() == 1 ? owned.first() : -1;
}

// ---- A Saturn-class model with its two chains, for the preselector ------

void seedTwoChainRadio(RadioModel& model)
{
    model.setBoardForTest(HPSDRHW::Saturn);
    RadioInfo info;
    info.protocol = ProtocolVersion::Protocol2;
    model.setLastRadioInfoForTest(info);
    for (int st = 0; st < 5; ++st) {
        if (model.receiverManager()->receiverConfig(st).receiverIndex < 0) {
            model.receiverManager()->createReceiver();
        }
    }
}

void pinToAdc0(RadioModel& model, const QList<SliceModel*>& slices)
{
    static constexpr int kStreamToDdc[5] = {2, 3, 4, 5, 6};
    DdcAssignment a{};
    for (int st = 0; st < 5; ++st) {
        a.streamDdc[st] = kStreamToDdc[st];
        a.ddcEnable |= (1 << kStreamToDdc[st]);
    }
    for (SliceModel* slice : slices) {
        const int ddc = kStreamToDdc[slice->streamIndex()];
        if (ddc < 4) {
            a.adcCtrl1 &= ~(3 << (ddc * 2));
        } else {
            a.adcCtrl2 &= ~(3 << ((ddc - 4) * 2));
        }
    }
    model.publishDdcAssignmentForTest(a);
}

} // namespace

class TstSliceClaimsExpiry : public QObject {
    Q_OBJECT

private slots:
    // A controls A0 and B listens; A drops; at 180 s A0 keeps running with
    // no controller for B, and A's transmit goes as today.
    void aControllersEndOf180SecondsKeepsTheSliceForItsListener()
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
        const int a0 = onlySliceOf(core, a);
        QVERIFY(a0 >= 0);
        QTRY_VERIFY(holds(appB, accessKey(a0)));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refArgs(appB, a0))));
        MoxController* mox = core.model->moxController();
        mox->setMox(true, keyerFor(a));
        mox->setMox(false, keyerFor(a));
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        TransmitHolder* holder = core.server->transmitHolder();
        QVERIFY(holder->isHeldBy(a.key.fingerprint()));
        const int streamBefore = core.model->sliceById(a0)->streamIndex();

        core.now = 0;
        drop(core, appA, a);
        // Within the 180 s nothing changes.
        core.now = DeviceSessionRegistry::kGraceMs - 1;
        QVERIFY(core.sessions().expireAway().isEmpty());
        SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownership->mark(a0).owner, a.key.fingerprint());

        core.now = DeviceSessionRegistry::kGraceMs;
        QCOMPARE(core.sessions().expireAway().size(), 1);
        QVERIFY(core.model->sliceById(a0) != nullptr);
        QVERIFY(ownership->mark(a0).owner.isEmpty());
        QVERIFY(!ownership->mark(a0).isHeld());
        QCOMPARE(ownership->listenersOf(a0), QList<QByteArray>{b.key.fingerprint()});
        // Its receiver, and so B's audio, never stopped.
        QCOMPARE(core.model->sliceById(a0)->streamIndex(), streamBefore);
        QTRY_COMPARE(holder->state(), TransmitHolder::State::Unheld);
    }

    // A controls A0 alone and drops: at 180 s A0 closes, the Core has no
    // slice, and A's layout is saved and restored when A comes back.
    void aLoneControllersEndOf180SecondsClosesAndSavesItsSlice()
    {
        Core core;
        core.model->configureStreamPool(5, 5, 192000);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kShares);
        QVERIFY(admitted(appA));
        QCOMPARE(core.model->slices().size(), 1);
        const int a0 = onlySliceOf(core, a);
        core.model->sliceById(a0)->setFrequency(7074000.0);

        core.now = 0;
        drop(core, appA, a);
        core.now = DeviceSessionRegistry::kGraceMs;
        QCOMPARE(core.sessions().expireAway().size(), 1);
        QVERIFY(core.model->slices().isEmpty());
        QVERIFY(core.model->sliceOwnership()->heldFor(a.key.fingerprint()).isEmpty());
        const QString mac = core.model->currentRadioMac();
        const QList<SavedSlice> saved =
            DeviceLayoutStore::load(AppSettings::instance(), mac, a.key.fingerprint());
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved.first().frequencyHz, 7074000.0);

        LoopbackTransport* back = core.signIn(a, kShares);
        QVERIFY(admitted(back));
        QCOMPARE(core.model->slices().size(), 1);
        const int restored = onlySliceOf(core, a);
        QVERIFY(restored >= 0);
        QCOMPARE(core.model->sliceById(restored)->frequency(), 7074000.0);
    }

    // Slice control fix wave (whole-branch review, Critical 1): the radio's
    // own PTT keys A's only slice and A's 180 s end. The slice stays while
    // the radio is keyed; once it unkeys, the slice closes and A's layout
    // is saved. The transmit binding ends only with the radio unkeyed.
    void aKeyedLoneSliceClosesOnlyOnceTheRadioUnkeys()
    {
        Core core;
        allowTransmit(core);
        core.model->configureStreamPool(5, 5, 192000);
        Device a;
        core.pair(a);
        LoopbackTransport* appA = core.signIn(a, kSharesTx);
        QVERIFY(admitted(appA));
        QCOMPARE(core.model->slices().size(), 1);
        const int a0 = onlySliceOf(core, a);
        SliceModel* slice = core.model->sliceById(a0);
        slice->setDspMode(DSPMode::USB);
        slice->setFrequency(14200000.0);
        TxSliceArbiter* arbiter = core.model->txSliceArbiter();
        QCOMPARE(arbiter->txBoundSliceId(), a0);
        MoxController* mox = core.model->moxController();
        QList<bool> keyedAtUnbind;
        connect(arbiter, &TxSliceArbiter::txBoundSliceChanged, arbiter,
                [&keyedAtUnbind, mox](int, int now) {
                    if (now == -1) {
                        keyedAtUnbind.append(mox->isMox() || mox->state() != MoxState::Rx);
                    }
                });
        mox->onMicPttFromRadio(true);
        QTRY_COMPARE(mox->state(), MoxState::Tx);

        core.now = 0;
        drop(core, appA, a);
        core.now = DeviceSessionRegistry::kGraceMs;
        QCOMPARE(core.sessions().expireAway().size(), 1);
        QVERIFY(core.model->sliceOwnership()->heldFor(a.key.fingerprint()).isEmpty());
        QTest::qWait(50);
        QVERIFY(core.model->sliceById(a0) != nullptr);
        QVERIFY(mox->isMox());
        QCOMPARE(arbiter->txBoundSliceId(), a0);
        QVERIFY(keyedAtUnbind.isEmpty());

        mox->onMicPttFromRadio(false);
        QTRY_VERIFY(core.model->slices().isEmpty());
        QCOMPARE(mox->state(), MoxState::Rx);
        QCOMPARE(keyedAtUnbind, QList<bool>({false}));
        const QList<SavedSlice> saved = DeviceLayoutStore::load(
            AppSettings::instance(), core.model->currentRadioMac(), a.key.fingerprint());
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved.first().frequencyHz, 14200000.0);
    }

    // A drops at t0, returns at t0+170 s, drops again at t0+175 s: the
    // first absence's end does nothing; the second ends at t0+355 s.
    void anOldAbsenceNeverEndsANewOne()
    {
        Core core;
        core.model->configureStreamPool(5, 5, 192000);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kShares);
        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appA) && admitted(appB));
        const int a0 = onlySliceOf(core, a);
        const QByteArray id = a.key.fingerprint();
        QSignalSpy ended(&core.sessions(), &DeviceSessionRegistry::graceEnded);

        core.now = 0;
        drop(core, appA, a);
        const quint64 first = core.sessions().entry(id)->awayGeneration;
        QVERIFY(first != 0);
        core.now = 170000;
        LoopbackTransport* again = core.signIn(a, kShares);
        QVERIFY(admitted(again));
        QCOMPARE(core.sessions().entry(id)->awayGeneration, quint64(0));
        QVERIFY(!core.sessions().isCurrentAbsence(id, first));
        core.now = 175000;
        drop(core, again, a);
        const quint64 second = core.sessions().entry(id)->awayGeneration;
        QVERIFY(second != first);
        QVERIFY(!core.sessions().isCurrentAbsence(id, first));
        QVERIFY(core.sessions().isCurrentAbsence(id, second));

        // The first absence's end does nothing.
        core.now = 180000;
        QVERIFY(core.sessions().expireAway().isEmpty());
        QCOMPARE(ended.count(), 0);
        QCOMPARE(core.model->sliceOwnership()->mark(a0).owner, id);

        core.now = 175000 + DeviceSessionRegistry::kGraceMs - 1;
        QVERIFY(core.sessions().expireAway().isEmpty());
        QCOMPARE(core.model->sliceOwnership()->mark(a0).owner, id);
        core.now = 175000 + DeviceSessionRegistry::kGraceMs;
        QCOMPARE(core.sessions().expireAway(), QList<QByteArray>{id});
        QCOMPARE(ended.count(), 1);
        QCOMPARE(ended.first().at(1).value<quint64>(), second);
        QVERIFY(core.model->sliceById(a0) == nullptr
                || core.model->sliceOwnership()->mark(a0).owner != id);
    }

    // A drops; B takes A0 at 60 s; A returns at 120 s: A listens to A0, B
    // controls it, and A's change is refused.
    void aSliceTakenWhileAwayIsOnlyListenedToOnReturn()
    {
        Core core;
        core.model->configureStreamPool(5, 5, 192000);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kShares);
        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appA) && admitted(appB));
        const int a0 = onlySliceOf(core, a);
        QTRY_VERIFY(holds(appB, accessKey(a0)));

        core.now = 0;
        drop(core, appA, a);
        core.now = 60000;
        const QJsonObject took = core.invoke(appB, "slice.takeControl", revisionArgs(appB, a0));
        QVERIFY2(accepted(took), qPrintable(reasonOf(took)));
        SliceOwnership* ownership = core.model->sliceOwnership();
        QCOMPARE(ownership->mark(a0).owner, b.key.fingerprint());
        QVERIFY(ownership->isListening(a.key.fingerprint(), a0));

        core.now = 120000;
        LoopbackTransport* back = core.signIn(a, kShares);
        QVERIFY(admitted(back));
        QCOMPARE(ownership->mark(a0).owner, b.key.fingerprint());
        QVERIFY(ownership->isListening(a.key.fingerprint(), a0));
        const QJsonObject change =
            core.invoke(back, "nnr.resetTuning", {int64("sliceId", a0)});
        QVERIFY(!accepted(change));
        QVERIFY(!reasonOf(change).isEmpty());
    }

    // An explicit leave applies the same rule at once.
    void anExplicitLeaveReleasesAtOnce()
    {
        Core core;
        core.model->configureStreamPool(5, 5, 192000);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        Device c(QStringLiteral("Shack Mac"), QStringLiteral("computer"));
        core.pair(a);
        core.pair(b);
        core.pair(c);
        LoopbackTransport* appA = core.signIn(a, kShares);
        LoopbackTransport* appB = core.signIn(b, kShares);
        LoopbackTransport* appC = core.signIn(c, kShares);
        QVERIFY(admitted(appA) && admitted(appB) && admitted(appC));
        const int a0 = onlySliceOf(core, a);
        const int c0 = onlySliceOf(core, c);
        QTRY_VERIFY(holds(appB, accessKey(a0)));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refArgs(appB, a0))));

        QVERIFY(accepted(core.invoke(appA, "session.leave")));
        SliceOwnership* ownership = core.model->sliceOwnership();
        QTRY_VERIFY(ownership->mark(a0).owner.isEmpty());
        QVERIFY(core.model->sliceById(a0) != nullptr);
        QCOMPARE(ownership->listenersOf(a0), QList<QByteArray>{b.key.fingerprint()});

        // C's slice, with nobody else on it, closes and is saved.
        core.model->sliceById(c0)->setFrequency(3573000.0);
        QVERIFY(accepted(core.invoke(appC, "session.leave")));
        QTRY_VERIFY(core.model->sliceById(c0) == nullptr);
        const QList<SavedSlice> saved = DeviceLayoutStore::load(
            AppSettings::instance(), core.model->currentRadioMac(), c.key.fingerprint());
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved.first().frequencyHz, 3573000.0);
    }

    // A reconnect after the deadline, before the timer fired: the expiry
    // runs first, inside admit, and acts once; then A is admitted afresh
    // and its saved slice restored.
    void aLateReconnectExpiresFirstAndActsOnce()
    {
        Core core;
        core.model->configureStreamPool(5, 5, 192000);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kShares);
        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appA) && admitted(appB));
        const int a0 = onlySliceOf(core, a);
        core.model->sliceById(a0)->setFrequency(10136000.0);
        QSignalSpy ended(&core.sessions(), &DeviceSessionRegistry::graceEnded);
        QSignalSpy removed(core.model.get(), &RadioModel::sliceRemoved);

        core.now = 0;
        drop(core, appA, a);
        core.now = DeviceSessionRegistry::kGraceMs + 5;
        LoopbackTransport* back = core.signIn(a, kShares);
        QVERIFY(admitted(back));
        QCOMPARE(ended.count(), 1);
        QCOMPARE(removed.count(), 1);
        const int restored = onlySliceOf(core, a);
        QVERIFY(restored >= 0);
        QCOMPARE(core.model->sliceById(restored)->frequency(), 10136000.0);
        QCOMPARE(core.sessions().entry(a.key.fingerprint())->state,
                 DeviceSessionRegistry::State::Listening);
    }

    // A returning device's saved slices never displace a current controller
    // or listener, and never make it the controller of a slice that still
    // exists.
    void aReturnNeverTakesBackASliceStillOnTheCore()
    {
        Core core;
        core.model->configureStreamPool(5, 5, 192000);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a, kShares);
        LoopbackTransport* appB = core.signIn(b, kShares);
        QVERIFY(admitted(appA) && admitted(appB));
        const int a0 = onlySliceOf(core, a);
        QTRY_VERIFY(holds(appB, accessKey(a0)));
        QVERIFY(accepted(core.invoke(appB, "slice.listen", refArgs(appB, a0))));

        core.now = 0;
        drop(core, appA, a);
        core.now = DeviceSessionRegistry::kGraceMs;
        QCOMPARE(core.sessions().expireAway().size(), 1);
        SliceOwnership* ownership = core.model->sliceOwnership();
        QVERIFY(ownership->mark(a0).owner.isEmpty());

        LoopbackTransport* back = core.signIn(a, kShares);
        QVERIFY(admitted(back));
        QVERIFY(ownership->mark(a0).owner.isEmpty());
        QCOMPARE(ownership->listenersOf(a0), QList<QByteArray>{b.key.fingerprint()});
        QVERIFY(!ownership->ownedBy(a.key.fingerprint()).contains(a0));
    }

    // JJ's bench: a second key for the same computer gets C-Tune on the
    // receiver the first key's slice anchored, once the first key's 180 s
    // are over; before that the refusal says that device is not connected.
    void aNewKeyGetsCTuneOnceTheOldKeysTimeIsUp()
    {
        Core core;
        core.model->configureStreamPool(5, 5, 192000);
        // The Core's first slice, on a receiver.
        core.model->sliceById(0)->setFrequency(14200000.0);
        Device oldKey(QStringLiteral("MacBook-Pro"), QStringLiteral("computer"));
        Device newKey(QStringLiteral("MacBook-Pro"), QStringLiteral("computer"));
        core.pair(oldKey);
        core.pair(newKey);
        LoopbackTransport* appOld = core.signIn(oldKey, kShares);
        QVERIFY(admitted(appOld));
        const int oldSlice = onlySliceOf(core, oldKey);
        const int stream = core.model->sliceById(oldSlice)->streamIndex();
        QVERIFY(stream >= 0);

        core.now = 0;
        drop(core, appOld, oldKey);
        LoopbackTransport* appNew = core.signIn(newKey, kShares);
        QVERIFY(admitted(appNew));
        // A slice of the new key's on the old key's receiver.
        const int newSlice = onlySliceOf(core, newKey);
        QVERIFY(newSlice >= 0);
        core.model->sliceById(newSlice)->setFrequency(
            core.model->sliceById(oldSlice)->frequency() + 5000.0);
        QTRY_COMPARE(core.model->sliceById(newSlice)->streamIndex(), stream);
        QCOMPARE(core.model->sliceOwnership()->anchorOf(stream), oldKey.key.fingerprint());

        QJsonObject pin = core.invoke(appNew, "requestStreamCtunPinned",
                                      {int64("sliceId", newSlice), flag("pinned", true)});
        QVERIFY(!accepted(pin));
        QVERIFY2(reasonOf(pin).contains(QStringLiteral("which is not connected now")),
                 qPrintable(reasonOf(pin)));
        QVERIFY(OperatorWording::isPlain(reasonOf(pin)));

        core.now = DeviceSessionRegistry::kGraceMs;
        QCOMPARE(core.sessions().expireAway().size(), 1);
        QCOMPARE(core.model->sliceOwnership()->anchorOf(stream), newKey.key.fingerprint());
        pin = core.invoke(appNew, "requestStreamCtunPinned",
                          {int64("sliceId", newSlice), flag("pinned", true)});
        QVERIFY2(accepted(pin), qPrintable(reasonOf(pin)));
    }

    // ── Amendment 8a: the preselector ────────────────────────────────────

    // A's 20 m slice and B's 40 m slice on one chain. With A away (in its
    // 180 s, or kept for it) the chain filters for B alone; with A here it
    // is bypassed as today.
    void anAwayDevicesSliceNeverWidensTheFilter()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        seedTwoChainRadio(model);
        const int a0 = model.addSlice();
        model.sliceById(a0)->setFrequency(14200000.0);
        const int b0 = model.addSlice();
        model.sliceById(b0)->setFrequency(7150000.0);
        pinToAdc0(model, {model.sliceById(a0), model.sliceById(b0)});
        SliceOwnership* ownership = model.sliceOwnership();
        const QByteArray devA = QByteArrayLiteral("device-a");
        ownership->setOwner(a0, devA);
        ownership->setOwner(b0, QByteArrayLiteral("device-b"));
        model.requestDdcAssignment();
        QVERIFY(model.panBypassState({b0}).bypassed);

        ownership->setAwayDevices({devA});
        QVERIFY(ownership->isAwaySlice(a0));
        QVERIFY(!ownership->isAwaySlice(b0));
        QVERIFY(!model.panBypassState({b0}).bypassed);

        // Back: A0 counts again at once.
        ownership->setAwayDevices({});
        QVERIFY(model.panBypassState({b0}).bypassed);

        // Kept for A (a hold): the same; its return counts at once.
        ownership->hold(a0, devA);
        QVERIFY(!model.panBypassState({b0}).bypassed);
        ownership->returnHeld(devA);
        QVERIFY(model.panBypassState({b0}).bypassed);
    }

    // ── Amendment 8a: the several-devices questions ──────────────────────

    // A window move reaching only an away device's slice names nobody; a
    // slice of an away device is not offered to close.
    void theReceiverPlannerNamesNoAwayDevice()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int a0 = model.addSlice(QStringLiteral("pan-0"));
        model.sliceById(a0)->setFrequency(14200000.0);
        const int b0 = model.addSlice(QStringLiteral("pan-0"));
        model.sliceById(b0)->setFrequency(14210000.0);
        const int stream = model.sliceById(b0)->streamIndex();
        QVERIFY(stream >= 0);
        QCOMPARE(model.sliceById(a0)->streamIndex(), stream);
        SliceOwnership* ownership = model.sliceOwnership();
        const QByteArray devA = QByteArrayLiteral("device-a");
        const QByteArray devB = QByteArrayLiteral("device-b");
        ownership->setOwner(a0, devA);
        ownership->setOwner(b0, devB);
        const ReceiverPlanner planner(model, [](const QByteArray&) {
            return ReceiverPlanner::DeviceInfo{};
        });
        // B's move far enough that A0 leaves the window.
        const double farCentre = 14210000.0 + 150000.0;
        QCOMPARE(planner.planWindowMove(stream, farCentre, devB, b0).disturbed.size(), 1);
        QCOMPARE(planner.sliceChoices(devB).size(), 1);

        ownership->setAwayDevices({devA});
        QVERIFY(planner.planWindowMove(stream, farCentre, devB, b0).disturbed.isEmpty());
        QVERIFY(planner.sliceChoices(devB).isEmpty());
    }

    // A shared change that reaches only an away device's slice applies at
    // once; with that device here it is asked, as today.
    void aSharedChangeReachingOnlyAnAwayDeviceIsNotAsked()
    {
        Core core;
        addCoHostedSlice(*core.model);
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appA) && admitted(appB));
        QVERIFY(!heldKeys(appB, QStringLiteral("slice:")).isEmpty());

        // B here: A's shared diversity change is asked. (Ruling 7.1a, joined
        // at the merge into the trunk: a shared blanker now applies at once
        // and tells, so the asking example is diversity, as in
        // tst_confirm_step.)
        appA->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "slice:0",
            {f64("diversityGainDb", 6.0)},
            77)));
        QVERIFY(QTest::qWaitFor(
            [appA]() {
                return !firstOfType(appA->received(), QStringLiteral("confirm.request")).isEmpty();
            },
            5000));
        QCOMPARE(core.model->sliceById(0)->diversityGainDb(), 0.0);

        // B away: the same change applies at once, naming nobody.
        core.now = 0;
        drop(core, appB, b);
        const int asked = static_cast<int>(
            ofType(appA->received(), QStringLiteral("confirm.request")).size());
        appA->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "slice:0",
            {f64("diversityGainDb", 6.0)},
            78)));
        QTRY_COMPARE(core.model->sliceById(0)->diversityGainDb(), 6.0);
        QCOMPARE(static_cast<int>(ofType(appA->received(), QStringLiteral("confirm.request")).size()),
                 asked);
    }
};

QTEST_MAIN(TstSliceClaimsExpiry)
#include "tst_slice_claims_expiry.moc"
