// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_slice_ownership.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 73 (R-IOS-02; the several-devices design, sections
// 5.1, 5.2 and 5.7, rulings 5.1, 5.2, 5.10 and 5.11): SliceOwnership, the
// Core's record of whose each slice is and of each owner's active slice.
//
// Ownership first (who may change which slice rests on these marks): a new
// slice belongs to the open creator scope's owner, or to none; adoption takes
// only slices with no owner, never one held for another device; held slices
// return to the device they are held for and to nobody else; a slice being
// removed is nobody's, but its mark is kept until its removal is announced.
// Then the active slice: each owner's own, one per owner, A's choice never
// moving B's; a removed active slice passing to its owner's next; the
// station-level active slice following the most recent choice, or the
// transmit holder's while one holds transmit.
//
// Slice control plan Task 1: each slice's incarnation, distinct from its
// reusable id, never shared by two slices of one run and not repeated by a
// Core started again with another boot nonce; each slice's control
// revision, 1 when made and one higher for each change of owner.
//
// Slice control plan Task 3: listening membership (join order kept, the
// controller always joined, the former controller kept on a change of
// owner, leaving only when asked or on claims removal) and each device's
// active receive slice, which never moves a slice's `active`, its owner's
// active slice or the station-level one, so a listener's choice never
// moves Alex band routing.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 73 (R-IOS-02), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-28: slice control and shared listening plan Task 1: the
//               incarnation and control revision cases. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-28: slice control and shared listening plan Task 3: the
//               membership, claims removal and active receive slice
//               cases. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QFile>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/RadioConnection.h"
#include "core/SliceOwnership.h"
#include "core/accessories/AlexController.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

const QByteArray kA = QByteArrayLiteral("device-a");
const QByteArray kB = QByteArrayLiteral("device-b");
const QByteArray kC = QByteArrayLiteral("device-c");
const QByteArray kToken = QByteArrayLiteral("token:1");

void add(SliceOwnership& own, int id, const QByteArray& creator = {})
{
    SliceOwnership::CreatorScope scope(&own, creator);
    own.noteSliceAdded(id);
}

// Captures the antenna routing the model sends (tst_antenna_kept's fake):
// connected, so applyAlexAntennaForBand reaches it.
class RoutingConnection : public RadioConnection {
    Q_OBJECT
public:
    QList<AntennaRouting> calls;

    explicit RoutingConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setMox(bool) override {}
    void setAntennaRouting(AntennaRouting r) override { calls.append(r); }
    void setWatchdogEnabled(bool enabled) override { m_watchdogEnabled = enabled; }
    void sendTxIq(const float*, int) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};

} // namespace

class TstSliceOwnership : public QObject {
    Q_OBJECT

private slots:
    // 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
    void adoptionNotificationMayRetireItsModel_data()
    {
        QTest::addColumn<int>("notification");
        QTest::newRow("mark") << 0;
        QTest::newRow("revision") << 1;
        QTest::newRow("listeners") << 2;
        QTest::newRow("active") << 3;
        QTest::newRow("activeRx") << 4;
    }
    void adoptionNotificationMayRetireItsModel()
    {
        QFETCH(int, notification);
        auto model = std::make_unique<RadioModel>();
        const int a = model->addSlice(QStringLiteral("pan-0"));
        const int b = model->addSlice(QStringLiteral("pan-0"));
        QVERIFY(a >= 0 && b >= 0);
        QPointer<RadioModel> observed(model.get());
        QPointer<SliceOwnership> ownership(model->sliceOwnership());
        bool retired = false;
        const auto retire = [&] { if (!retired) { retired = true; model.reset(); } };
        switch (notification) {
        case 0: connect(ownership, &SliceOwnership::markChanged, this, retire); break;
        case 1: connect(ownership, &SliceOwnership::controlRevisionChanged, this, retire); break;
        case 2: connect(ownership, &SliceOwnership::listenersChanged, this, retire); break;
        case 3: connect(ownership, &SliceOwnership::activeChanged, this, retire); break;
        case 4: connect(ownership, &SliceOwnership::activeRxChanged, this, retire); break;
        }
        ownership->adoptUnowned(SliceOwnership::stationDevice());
        QVERIFY(retired);
        QVERIFY(!observed);
        QVERIFY(!ownership);
    }

    void defaultAdoptionDoesNotOverwriteAReentrantlyHeldRemainingSlice()
    {
        SliceOwnership own;
        own.noteSliceAdded(0);
        own.noteSliceAdded(1);
        connect(&own, &SliceOwnership::markChanged, this,
                [&](int id, const QByteArray&, const QByteArray&) {
            if (id == 0) { own.hold(1, kB); }
        });
        const auto adopted = own.adoptUnowned(SliceOwnership::stationDevice());
        QCOMPARE(adopted, QList<int>{0});
        QCOMPARE(own.mark(0).owner, SliceOwnership::stationDevice());
        QCOMPARE(own.mark(1).heldFor, kB);
        QCOMPARE(own.controlRevision(0), quint64(2));
        QCOMPARE(own.controlRevision(1), quint64(2));
        QCOMPARE(own.activeFor(SliceOwnership::stationDevice()), 0);
    }

    void initTestCase()
    {
        AppSettings::setProfileOverride(
            QStringLiteral("slice-ownership-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // ── Owners ───────────────────────────────────────────────────────────

    void aNewSliceBelongsToTheOpenCreatorOrToNone()
    {
        SliceOwnership own;
        own.noteSliceAdded(0);
        add(own, 1, kA);
        {
            SliceOwnership::CreatorScope outer(&own, kB);
            {
                SliceOwnership::CreatorScope inner(&own, kToken);
                own.noteSliceAdded(2);
            }
            own.noteSliceAdded(3);
        }
        own.noteSliceAdded(4);
        QVERIFY(own.mark(0).owner.isEmpty());
        QCOMPARE(own.mark(1).owner, kA);
        QCOMPARE(own.mark(2).owner, kToken);
        QCOMPARE(own.mark(3).owner, kB);
        QVERIFY(own.mark(4).owner.isEmpty());
        QVERIFY(own.creator().isEmpty());
        QCOMPARE(own.liveSlices(), (QList<int>{0, 1, 2, 3, 4}));
    }

    void adoptionTakesOnlySlicesWithNoOwner()
    {
        SliceOwnership own;
        own.noteSliceAdded(0);
        add(own, 1, kB);
        own.noteSliceAdded(2);
        own.noteSliceAdded(3);
        own.hold(3, kB);
        QCOMPARE(own.adoptUnowned(kA), (QList<int>{0, 2}));
        QCOMPARE(own.ownedBy(kA), (QList<int>{0, 2}));
        QCOMPARE(own.mark(1).owner, kB);
        // Held for B: never adopted, still the station device's.
        QCOMPARE(own.mark(3).owner, SliceOwnership::stationDevice());
        QCOMPARE(own.mark(3).heldFor, kB);
        QVERIFY(own.unowned().isEmpty());
        // Nothing left to adopt.
        QVERIFY(own.adoptUnowned(kB).isEmpty());
    }

    void heldSlicesReturnToTheirOwnerAndToNobodyElse()
    {
        SliceOwnership own;
        add(own, 0, kA);
        add(own, 1, kA);
        add(own, 2, kB);
        own.hold(0, kA);
        own.hold(1, kA);
        QCOMPARE(own.heldFor(kA), (QList<int>{0, 1}));
        QVERIFY(own.ownedBy(kA).isEmpty());
        QCOMPARE(own.ownedBy(SliceOwnership::stationDevice()), (QList<int>{0, 1}));
        QVERIFY(own.mark(0).isHeld());
        QCOMPARE(own.mark(0).subject(), kA);

        QVERIFY(own.returnHeld(kB).isEmpty());
        QCOMPARE(own.heldFor(kA), (QList<int>{0, 1}));

        QCOMPARE(own.returnHeld(kA), (QList<int>{0, 1}));
        QCOMPARE(own.ownedBy(kA), (QList<int>{0, 1}));
        QVERIFY(!own.mark(0).isHeld());
        QVERIFY(own.heldFor(kA).isEmpty());
    }

    void aHeldMarkIsAlwaysTheStationDevices()
    {
        SliceOwnership own;
        add(own, 0, kA);
        own.setMark(0, SliceOwnership::Mark{kB, kA});
        QCOMPARE(own.mark(0).owner, SliceOwnership::stationDevice());
        QCOMPARE(own.mark(0).heldFor, kA);
    }

    void aMarkChangeIsSignalledWithWhatItWas()
    {
        SliceOwnership own;
        own.noteSliceAdded(0);
        QSignalSpy spy(&own, &SliceOwnership::markChanged);
        own.adoptUnowned(kA);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);
        QVERIFY(spy.at(0).at(1).toByteArray().isEmpty());
        own.hold(0, kA);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(1).toByteArray(), kA);
        QVERIFY(spy.at(1).at(2).toByteArray().isEmpty());
        // The same mark again is no change.
        own.hold(0, kA);
        QCOMPARE(spy.count(), 2);
        // A slice that is not live has no mark to change.
        own.setOwner(7, kB);
        QCOMPARE(spy.count(), 2);
        QVERIFY(own.mark(7).owner.isEmpty());
    }

    void aSliceBeingRemovedIsNobodysButKeepsItsMarkUntilAnnounced()
    {
        SliceOwnership own;
        add(own, 0, kA);
        add(own, 1, kA);
        own.beginRemove(1);
        QVERIFY(!own.isLive(1));
        QCOMPARE(own.ownedBy(kA), (QList<int>{0}));
        // What is sent about its removal still knows whose it was.
        QCOMPARE(own.mark(1).owner, kA);
        own.endRemove(1);
        QVERIFY(own.mark(1).owner.isEmpty());
        QCOMPARE(own.liveSlices(), (QList<int>{0}));
    }

    void theManifestsOrderIsTheCreationOrder()
    {
        SliceOwnership own;
        add(own, 0, kA);
        add(own, 2, kA);
        add(own, 1, kA);
        own.setOrder({2, 1, 0});
        QCOMPARE(own.liveSlices(), (QList<int>{2, 1, 0}));
        QCOMPARE(own.activeFor(kA), 2);
        // Ids it does not know are left out; live ids it was not given stay,
        // after the ones it was.
        own.setOrder({1, 9});
        QCOMPARE(own.liveSlices(), (QList<int>{1, 2, 0}));
    }

    // ── The active slice ─────────────────────────────────────────────────

    void eachOwnerHasOneActiveSliceOfItsOwn()
    {
        SliceOwnership own;
        add(own, 0, kA);
        add(own, 1, kA);
        add(own, 2, kB);
        add(own, 3, kB);
        QCOMPARE(own.activeFor(kA), 0);
        QCOMPARE(own.activeFor(kB), 2);
        QCOMPARE(own.activeFor(QByteArrayLiteral("nobody")), -1);
        int activeA = 0;
        int activeB = 0;
        for (int id : own.liveSlices()) {
            if (own.isActive(id)) {
                (own.mark(id).owner == kA ? activeA : activeB) += 1;
            }
        }
        QCOMPARE(activeA, 1);
        QCOMPARE(activeB, 1);
    }

    void aChoiceByAMovesAsActiveSliceAndNeverBs()
    {
        SliceOwnership own;
        add(own, 0, kA);
        add(own, 1, kA);
        add(own, 2, kB);
        add(own, 3, kB);
        own.setActive(kB, 3);
        QSignalSpy spy(&own, &SliceOwnership::activeChanged);
        own.setActive(kA, 1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(own.activeFor(kA), 1);
        QCOMPARE(own.activeFor(kB), 3);
        QVERIFY(own.isActive(1));
        QVERIFY(!own.isActive(0));
        QVERIFY(own.isActive(3));
        // A choice of another owner's slice is not a choice.
        own.setActive(kA, 2);
        QCOMPARE(own.activeFor(kA), 1);
        QCOMPARE(own.activeFor(kB), 3);
    }

    void aRemovedActiveSlicePassesToItsOwnersNext()
    {
        SliceOwnership own;
        add(own, 0, kA);
        add(own, 1, kA);
        add(own, 2, kA);
        own.setActive(kA, 1);
        own.beginRemove(1);
        QCOMPARE(own.activeFor(kA), 0);
        own.endRemove(1);
        QCOMPARE(own.activeFor(kA), 0);
        // A choice survives while the slice is away from its owner, and is
        // its active slice again when it comes back.
        own.setActive(kA, 2);
        own.hold(2, kA);
        QCOMPARE(own.activeFor(kA), 0);
        own.returnHeld(kA);
        QCOMPARE(own.activeFor(kA), 2);
    }

    void adoptionCarriesTheActiveSliceOver()
    {
        SliceOwnership own;
        own.noteSliceAdded(0);
        own.noteSliceAdded(1);
        own.setActive(QByteArray(), 1);
        own.adoptUnowned(kA);
        QCOMPARE(own.activeFor(kA), 1);
    }

    void theStationLevelActiveSliceFollowsTheMostRecentChoice()
    {
        SliceOwnership own;
        QCOMPARE(own.stationActiveSlice(), -1);
        add(own, 0, kA);
        add(own, 1, kB);
        own.setActive(kA, 0);
        QCOMPARE(own.stationActiveSlice(), 0);
        own.setActive(kB, 1);
        QCOMPARE(own.stationActiveSlice(), 1);
        // Removing it moves the station-level slice to its owner's next, or
        // to none when that owner has no other.
        add(own, 2, kB);
        own.beginRemove(1);
        QCOMPARE(own.stationActiveSlice(), 2);
        own.endRemove(1);
        own.beginRemove(2);
        QCOMPARE(own.stationActiveSlice(), -1);
    }

    void theTransmitHoldersActiveSliceWinsWhileItHoldsTransmit()
    {
        SliceOwnership own;
        add(own, 0, kA);
        add(own, 1, kB);
        own.setActive(kA, 0);
        own.setActive(kB, 1);
        QSignalSpy spy(&own, &SliceOwnership::activeChanged);
        own.setTransmitHolder(kA);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(own.stationActiveSlice(), 0);
        own.setActive(kB, 1);
        QCOMPARE(own.stationActiveSlice(), 0);
        own.setTransmitHolder(QByteArray());
        QCOMPARE(own.stationActiveSlice(), 1);
        // A holder that owns no slice leaves the most recent choice.
        own.setTransmitHolder(QByteArrayLiteral("device-c"));
        QCOMPARE(own.stationActiveSlice(), 1);
    }

    // ── Incarnation and control revision (slice control plan Task 1) ──────

    void aClosedIdMadeAgainForTheSameOwnerIsANewIncarnation()
    {
        SliceOwnership own;
        add(own, 0, kA);
        const SliceOwnership::SliceRef first = own.refOf(0);
        QCOMPARE(first.sliceId, 0);
        QVERIFY(first.incarnation != 0);
        QCOMPARE(own.incarnation(0), first.incarnation);
        QVERIFY(own.matches(first));

        // Being removed, the slice is not live: no incarnation, no match.
        own.beginRemove(0);
        QCOMPARE(own.incarnation(0), quint64{0});
        QCOMPARE(own.controlRevision(0), quint64{0});
        QVERIFY(!own.matches(first));
        own.endRemove(0);
        QCOMPARE(own.refOf(0).incarnation, quint64{0});

        // The same letter again, for the same owner.
        add(own, 0, kA);
        QCOMPARE(own.mark(0).owner, kA);
        QVERIFY(own.incarnation(0) != 0);
        QVERIFY(own.incarnation(0) != first.incarnation);
        QVERIFY(!own.matches(first));
        QVERIFY(own.matches(own.refOf(0)));
        // A ref with no incarnation or for no slice never matches.
        QVERIFY(!own.matches(SliceOwnership::SliceRef{}));
        QVERIFY(!own.matches(SliceOwnership::SliceRef{0, 0}));
        QVERIFY(!own.matches(SliceOwnership::SliceRef{5, own.incarnation(0)}));
    }

    void twoSlicesOfOneRunNeverShareAnIncarnation()
    {
        SliceOwnership own;
        QSet<quint64> seen;
        for (int round = 0; round < 200; ++round) {
            for (int id = 0; id < 4; ++id) {
                add(own, id, round % 2 == 0 ? kA : kB);
                const quint64 value = own.incarnation(id);
                QVERIFY(value != 0);
                // Below 2^53: exact as a JSON number.
                QVERIFY(value < (quint64{1} << 53));
                QVERIFY2(!seen.contains(value), qPrintable(QString::number(value)));
                seen.insert(value);
            }
            for (int id = 0; id < 4; ++id) {
                own.beginRemove(id);
                own.endRemove(id);
            }
        }
        QCOMPARE(seen.size(), 800);
    }

    void aCoreStartedAgainWithANewNonceDoesNotRepeatTheLastRunsValues()
    {
        quint64 lastRunFirst = 0;
        {
            SliceOwnership lastRun(0x12345u, nullptr);
            add(lastRun, 0, kA);
            lastRunFirst = lastRun.incarnation(0);
        }
        SliceOwnership thisRun(0x54321u, nullptr);
        add(thisRun, 0, kA);
        QVERIFY(thisRun.incarnation(0) != 0);
        QVERIFY(thisRun.incarnation(0) != lastRunFirst);
        QCOMPARE(thisRun.incarnation(0) >> 32, quint64{0x54321u});
        // Only the nonce's low 20 bits are used, so every value stays
        // below 2^52.
        SliceOwnership wide(0xFFFFFFFFu, nullptr);
        add(wide, 0, kA);
        QCOMPARE(wide.incarnation(0) >> 32, quint64{0xFFFFFu});
        QVERIFY(wide.incarnation(0) < (quint64{1} << 52));
        // A nonce of 0 still gives no incarnation of 0.
        SliceOwnership zero(0u, nullptr);
        add(zero, 0, kA);
        QVERIFY(zero.incarnation(0) != 0);
    }

    void theControlRevisionRisesByOneForEachChangeOfOwner()
    {
        SliceOwnership own;
        own.noteSliceAdded(0);
        QCOMPARE(own.controlRevision(0), quint64{1});
        QCOMPARE(own.controlRevision(3), quint64{0});
        QSignalSpy spy(&own, &SliceOwnership::controlRevisionChanged);

        // Adoption.
        own.adoptUnowned(kA);
        QCOMPARE(own.controlRevision(0), quint64{2});
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);
        QCOMPARE(spy.at(0).at(1).value<quint64>(), quint64{2});

        // The same owner again is no change.
        own.setOwner(0, kA);
        own.setMark(0, SliceOwnership::Mark{kA, QByteArray()});
        QCOMPARE(own.controlRevision(0), quint64{2});
        QCOMPARE(spy.count(), 1);

        // Hold, then return: twice.
        own.hold(0, kA);
        QCOMPARE(own.controlRevision(0), quint64{3});
        QCOMPARE(own.returnHeld(kA), QList<int>{0});
        QCOMPARE(own.controlRevision(0), quint64{4});
        QCOMPARE(spy.count(), 3);

        // Take (another owner) and release (no owner).
        own.setOwner(0, kB);
        QCOMPARE(own.controlRevision(0), quint64{5});
        own.setOwner(0, QByteArray());
        QCOMPARE(own.controlRevision(0), quint64{6});
        QCOMPARE(spy.count(), 5);
        QCOMPARE(spy.last().at(1).value<quint64>(), quint64{6});

        // Held for one absent device, then for another: the station device
        // owns it throughout, so only the first is a change of owner.
        own.hold(0, kA);
        QCOMPARE(own.controlRevision(0), quint64{7});
        own.hold(0, kB);
        QCOMPARE(own.mark(0).heldFor, kB);
        QCOMPARE(own.controlRevision(0), quint64{7});
        QCOMPARE(spy.count(), 6);

        // Each slice counts on its own; a new incarnation starts at 1.
        add(own, 1, kA);
        QCOMPARE(own.controlRevision(1), quint64{1});
        own.beginRemove(0);
        own.endRemove(0);
        add(own, 0, kA);
        QCOMPARE(own.controlRevision(0), quint64{1});
        QCOMPARE(spy.count(), 6);
    }

    // ── Membership and the active receive slice (slice control plan Task 3)

    void aJoinedListenerComesAfterTheControllerAndReceivesItsSlice()
    {
        SliceOwnership own;
        add(own, 0, kA);
        add(own, 1, kC);
        own.setActive(kC, 1);
        // The controller is joined from the start.
        QCOMPARE(own.listenersOf(0), QList<QByteArray>{kA});
        QCOMPARE(own.joinedBy(kA), QList<int>{0});
        QCOMPARE(own.activeRxFor(kA), 0);
        QCOMPARE(own.stationActiveSlice(), 1);

        QSignalSpy listeners(&own, &SliceOwnership::listenersChanged);
        QSignalSpy rx(&own, &SliceOwnership::activeRxChanged);
        QSignalSpy active(&own, &SliceOwnership::activeChanged);
        QVERIFY(own.join(kB, 0));
        QCOMPARE(own.listenersOf(0), (QList<QByteArray>{kA, kB}));
        QCOMPARE(listeners.count(), 1);
        QCOMPARE(listeners.at(0).at(0).toInt(), 0);
        QCOMPARE(own.activeRxFor(kB), 0);
        QCOMPARE(rx.count(), 1);
        QCOMPARE(rx.at(0).at(0).toByteArray(), kB);
        QCOMPARE(own.activeFor(kA), 0);
        QCOMPARE(own.activeFor(kB), -1);
        // B's choice of it moves nothing of A's and nothing station-wide.
        QVERIFY(own.setActiveRx(kB, 0));
        QCOMPARE(own.activeFor(kA), 0);
        QCOMPARE(own.stationActiveSlice(), 1);
        QCOMPARE(active.count(), 0);
        // Joined again: no change. Nobody, and a slice that is not live,
        // cannot be joined.
        QVERIFY(own.join(kB, 0));
        QVERIFY(own.join(kA, 0));
        QVERIFY(!own.join(QByteArray(), 0));
        QVERIFY(!own.join(kB, 7));
        QCOMPARE(own.listenersOf(0), (QList<QByteArray>{kA, kB}));
        QCOMPARE(listeners.count(), 1);
        QCOMPARE(rx.count(), 1);
    }

    void joinedSlicesAreListedInCreationOrderNotJoinOrder()
    {
        SliceOwnership own;
        add(own, 2, kA);
        add(own, 0, kA);
        add(own, 1, kC);
        QVERIFY(own.join(kB, 1));
        QVERIFY(own.join(kB, 0));
        QVERIFY(own.join(kB, 2));
        QCOMPARE(own.joinedBy(kB), (QList<int>{2, 0, 1}));
        QCOMPARE(own.joinedBy(kA), (QList<int>{2, 0}));
        QVERIFY(own.joinedBy(QByteArray()).isEmpty());
        // Its first joined slice until it chooses one.
        QCOMPARE(own.activeRxFor(kB), 2);
        QCOMPARE(own.activeRxFor(QByteArray()), -1);
        QCOMPARE(own.activeRxFor(QByteArrayLiteral("device-d")), -1);
    }

    void aListenersChoiceNeverMovesItsOwnActiveSliceOrTheStations()
    {
        SliceOwnership own;
        add(own, 0, kA);
        add(own, 1, kB);
        own.setActive(kA, 0);
        own.setActive(kB, 1);
        QCOMPARE(own.stationActiveSlice(), 1);
        QVERIFY(own.join(kB, 0));
        QCOMPARE(own.activeRxFor(kB), 1);   // its own choice so far
        QSignalSpy active(&own, &SliceOwnership::activeChanged);
        QVERIFY(own.setActiveRx(kB, 0));
        QCOMPARE(own.activeRxFor(kB), 0);
        QCOMPARE(own.activeFor(kB), 1);
        QVERIFY(own.isActive(1));
        QVERIFY(own.isActive(0));   // A's own
        QCOMPARE(own.stationActiveSlice(), 1);
        QCOMPARE(active.count(), 0);
        // A controller's own choice is its receive choice too.
        QVERIFY(own.setActiveRx(kB, 1));
        QCOMPARE(own.activeRxFor(kB), 1);
        QVERIFY(own.setActiveRx(kB, 0));
        own.setActive(kB, 1);
        QCOMPARE(own.activeRxFor(kB), 1);
    }

    void aSliceNotJoinedCannotBeChosenAndNothingChanges()
    {
        SliceOwnership own;
        add(own, 0, kA);
        add(own, 1, kB);
        add(own, 2, kC);
        own.setActive(kB, 1);
        QVERIFY(own.join(kB, 2));
        QVERIFY(own.setActiveRx(kB, 2));
        QSignalSpy rx(&own, &SliceOwnership::activeRxChanged);
        QSignalSpy active(&own, &SliceOwnership::activeChanged);
        QVERIFY(!own.setActiveRx(kB, 0));   // A's, not joined
        QVERIFY(!own.setActiveRx(kB, 5));   // no such slice
        QVERIFY(!own.setActiveRx(QByteArray(), 0));
        QCOMPARE(own.activeRxFor(kB), 2);
        QCOMPARE(own.activeFor(kB), 1);
        QCOMPARE(own.activeFor(kA), 0);
        QCOMPARE(own.stationActiveSlice(), 1);
        QCOMPARE(rx.count(), 0);
        QCOMPARE(active.count(), 0);
        // A slice being removed cannot be chosen either.
        own.beginRemove(0);
        QVERIFY(own.join(kA, 2));
        QVERIFY(!own.setActiveRx(kA, 0));
    }

    void theReceiveChoiceFallsBackWhenItsSliceIsLeftOrClosed()
    {
        SliceOwnership own;
        add(own, 0, kA);
        add(own, 1, kB);
        add(own, 2, kB);
        add(own, 3, kC);
        own.setActive(kB, 2);
        QVERIFY(own.join(kB, 0));
        QVERIFY(own.join(kB, 3));
        QVERIFY(own.setActiveRx(kB, 3));
        QSignalSpy rx(&own, &SliceOwnership::activeRxChanged);
        // Leaving it: back to its controller-active slice.
        QVERIFY(own.leave(kB, 3));
        QCOMPARE(own.activeRxFor(kB), 2);
        QCOMPARE(rx.count(), 1);
        QCOMPARE(rx.at(0).at(0).toByteArray(), kB);
        QCOMPARE(own.joinedBy(kB), (QList<int>{0, 1, 2}));
        // Joined again, the old choice does not come back.
        QVERIFY(own.join(kB, 3));
        QCOMPARE(own.activeRxFor(kB), 2);
        // Closing the chosen slice: the same.
        QVERIFY(own.setActiveRx(kB, 0));
        own.beginRemove(0);
        QCOMPARE(own.activeRxFor(kB), 2);
        own.endRemove(0);
        // The id made again for B does not carry the old choice.
        add(own, 0, kB);
        QCOMPARE(own.activeRxFor(kB), 2);
        // With no slice of its own left, its first joined slice.
        own.beginRemove(0);
        own.endRemove(0);
        own.beginRemove(1);
        own.endRemove(1);
        own.beginRemove(2);
        own.endRemove(2);
        QCOMPARE(own.activeFor(kB), -1);
        QCOMPARE(own.activeRxFor(kB), 3);
        QVERIFY(own.leave(kB, 3));
        QCOMPARE(own.activeRxFor(kB), -1);
    }

    void onlyAListenerLeavesAndNeverTheController()
    {
        SliceOwnership own;
        add(own, 0, kA);
        QVERIFY(own.join(kB, 0));
        QSignalSpy listeners(&own, &SliceOwnership::listenersChanged);
        QVERIFY(!own.leave(kA, 0));   // release first
        QVERIFY(!own.leave(kC, 0));   // not joined
        QVERIFY(!own.leave(kB, 9));
        QVERIFY(!own.leave(QByteArray(), 0));
        QCOMPARE(listeners.count(), 0);
        QVERIFY(own.leave(kB, 0));
        QCOMPARE(own.listenersOf(0), QList<QByteArray>{kA});
        QCOMPARE(listeners.count(), 1);
        // A release: control cleared, then the releaser leaves.
        QVERIFY(own.join(kB, 0));
        own.setOwner(0, QByteArray());
        QCOMPARE(own.listenersOf(0), (QList<QByteArray>{kA, kB}));
        QVERIFY(own.leave(kA, 0));
        QCOMPARE(own.listenersOf(0), QList<QByteArray>{kB});
        QVERIFY(own.unclaimed().isEmpty());
        QVERIFY(own.leave(kB, 0));
        QCOMPARE(own.unclaimed(), QList<int>{0});
    }

    void aChangeOfControllerKeepsTheFormerOneListening()
    {
        SliceOwnership own;
        add(own, 0, kA);
        QVERIFY(own.join(kC, 0));
        const quint64 revision = own.controlRevision(0);
        QSignalSpy listeners(&own, &SliceOwnership::listenersChanged);
        QList<QList<QByteArray>> seenAtMarkChange;
        connect(&own, &SliceOwnership::markChanged, this,
                [&own, &seenAtMarkChange](int id) { seenAtMarkChange.append(own.listenersOf(id)); });
        own.setOwner(0, kB);
        QCOMPARE(own.controlRevision(0), revision + 1);
        QCOMPARE(own.listenersOf(0), (QList<QByteArray>{kB, kA, kC}));
        QVERIFY(own.isListening(kA, 0));
        QCOMPARE(listeners.count(), 1);
        // Whoever hears of the change already sees the new membership.
        QCOMPARE(seenAtMarkChange,
                 (QList<QList<QByteArray>>{QList<QByteArray>{kB, kA, kC}}));
        // A listener made controller keeps its place in the join order.
        own.setOwner(0, kC);
        QCOMPARE(own.listenersOf(0), (QList<QByteArray>{kC, kA, kB}));
        // The former controllers leave only when asked.
        QVERIFY(own.leave(kA, 0));
        QVERIFY(own.leave(kB, 0));
        QCOMPARE(own.listenersOf(0), QList<QByteArray>{kC});
    }

    void aHoldNeverLeavesTheStationDeviceListening()
    {
        SliceOwnership own;
        add(own, 0, kA);
        own.hold(0, kA);
        // The station device runs it (it controls it) without joining.
        QCOMPARE(own.listenersOf(0), (QList<QByteArray>{SliceOwnership::stationDevice(), kA}));
        QVERIFY(own.returnHeld(kA) == QList<int>{0});
        QCOMPARE(own.listenersOf(0), QList<QByteArray>{kA});
        QVERIFY(!own.isListening(SliceOwnership::stationDevice(), 0));
        // A hold restored from a manifest (nobody joined before it).
        add(own, 1);
        own.hold(1, kB);
        QCOMPARE(own.listenersOf(1), QList<QByteArray>{SliceOwnership::stationDevice()});
        own.returnHeld(kB);
        QCOMPARE(own.listenersOf(1), QList<QByteArray>{kB});
    }

    void removingADevicesClaimsKeepsItsSlicesForTheOthers()
    {
        SliceOwnership own;
        add(own, 0, kA);    // B listens
        add(own, 1, kB);    // B controls, C listens
        add(own, 2, kB);    // B controls alone
        add(own, 3, kB);    // held for B
        add(own, 4, kC);    // nothing of B's
        own.hold(3, kB);
        QVERIFY(own.join(kB, 0));
        QVERIFY(own.join(kC, 1));
        own.setActive(kB, 2);
        QVERIFY(own.setActiveRx(kB, 0));
        const quint64 revision1 = own.controlRevision(1);
        QList<bool> bJoinedAtMarkChange;
        connect(&own, &SliceOwnership::markChanged, this,
                [&own, &bJoinedAtMarkChange](int id) {
                    bJoinedAtMarkChange.append(own.isListening(kB, id));
                });
        QSignalSpy listeners(&own, &SliceOwnership::listenersChanged);
        QSignalSpy rx(&own, &SliceOwnership::activeRxChanged);

        const SliceOwnership::ClaimsRemoved removed = own.removeClaims(kB);
        QCOMPARE(removed.releasedControl, (QList<int>{1, 2, 3}));
        QCOMPARE(removed.leftListening, QList<int>{0});
        QVERIFY(own.mark(1).owner.isEmpty());
        QCOMPARE(own.listenersOf(1), QList<QByteArray>{kC});
        QCOMPARE(own.controlRevision(1), revision1 + 1);
        QVERIFY(own.mark(3).owner.isEmpty());
        QVERIFY(!own.mark(3).isHeld());
        QCOMPARE(own.listenersOf(0), QList<QByteArray>{kA});
        QCOMPARE(own.listenersOf(4), QList<QByteArray>{kC});
        QCOMPARE(own.unclaimed(), (QList<int>{2, 3}));
        QVERIFY(own.joinedBy(kB).isEmpty());
        QCOMPARE(own.activeRxFor(kB), -1);
        QCOMPARE(own.activeFor(kB), -1);
        // B was gone from each slice before anyone heard of the change.
        QCOMPARE(bJoinedAtMarkChange, (QList<bool>{false, false, false}));
        QCOMPARE(listeners.count(), 4);
        // B's receive slice is gone, and so is the station device's, which
        // ran slice 3 for B.
        QCOMPARE(rx.count(), 2);
        QCOMPARE(rx.at(0).at(0).toByteArray(), kB);
        QCOMPARE(rx.at(1).at(0).toByteArray(), SliceOwnership::stationDevice());
        // Every slice is kept.
        QCOMPARE(own.liveSlices(), (QList<int>{0, 1, 2, 3, 4}));
        // Nothing left to remove.
        const SliceOwnership::ClaimsRemoved again = own.removeClaims(kB);
        QVERIFY(again.releasedControl.isEmpty());
        QVERIFY(again.leftListening.isEmpty());
        QVERIFY(own.removeClaims(QByteArray()).releasedControl.isEmpty());
    }

    void aListenersChoiceNeverMovesTheAlexBandOrTheStationSlice()
    {
        // A model with an Alex (ANT1 on 20 m, ANT2 on 40 m, receive and
        // transmit, as tst_antenna_kept's bench): A's slice 0 on 20 m, B's
        // slice 1 on 40 m, B's the station-level one.
        RadioModel model;
        auto* connection = new RoutingConnection();
        model.setCapsForTest(true);
        model.injectConnectionForTest(connection);
        model.configureStreamPool(2, 5, 192000);
        AlexController& alex = model.alexControllerMutable();
        alex.setRxAnt(Band::Band20m, 1);
        alex.setRxAnt(Band::Band40m, 2);
        alex.setTxAnt(Band::Band40m, 2);
        QCOMPARE(model.addSlice(QStringLiteral("pan-0")), 0);
        QCOMPARE(model.addSlice(QStringLiteral("pan-1")), 1);
        // Tuned before either has an owner, so no receive antenna is kept
        // for another device on the way (tst_antenna_kept).
        model.sliceById(0)->setFrequency(14074000.0);
        model.sliceById(1)->setFrequency(7074000.0);
        SliceOwnership* own = model.sliceOwnership();
        own->setOwner(0, kA);
        own->setOwner(1, kB);
        QVERIFY(model.setActiveSliceByIdFor(kA, 0));
        QVERIFY(model.setActiveSliceByIdFor(kB, 1));
        QCOMPARE(model.activeSlice(), model.sliceById(1));
        QVERIFY(own->join(kB, 0));
        const quint64 freedvBefore = model.freedvWantedFrequencyHzForTest();
        connection->calls.clear();
        QSignalSpy stationMoved(&model, &RadioModel::activeSliceIdChanged);

        // B selects A's slice, which it only listens to.
        QVERIFY(model.setActiveRxFor(kB, 0));
        QCOMPARE(own->activeRxFor(kB), 0);
        QCOMPARE(own->activeFor(kB), 1);
        QVERIFY(model.sliceById(1)->isActive());
        QVERIFY(model.sliceById(0)->isActive());   // A's own, as before
        QCOMPARE(model.activeSlice(), model.sliceById(1));
        QCOMPARE(stationMoved.count(), 0);
        QCOMPARE(model.freedvWantedFrequencyHzForTest(), freedvBefore);
        QVERIFY(connection->calls.isEmpty());
        // The next routing still follows the station-level slice's band
        // (40 m, ANT2), never A's 20 m because of B.
        alex.setRxOutOnTx(!alex.rxOutOnTx());
        QVERIFY(!connection->calls.isEmpty());
        QCOMPARE(connection->calls.last().trxAnt, 2);
        QCOMPARE(connection->calls.last().txAnt, 2);

        // A slice B has not joined: refused, nothing moves.
        QVERIFY(own->leave(kB, 0));
        QVERIFY(!model.setActiveRxFor(kB, 0));
        QVERIFY(!model.setActiveRxFor(kB, 7));
        QCOMPARE(own->activeRxFor(kB), 1);
        QCOMPARE(model.activeSlice(), model.sliceById(1));

        // A's choice of its own slice is its active slice as well, and the
        // most recent choice, so the station-level slice follows it.
        QVERIFY(model.setActiveRxFor(kA, 0));
        QCOMPARE(model.activeSlice(), model.sliceById(0));
        QCOMPARE(stationMoved.count(), 1);

        model.injectConnectionForTest(nullptr);
        delete connection;
    }
};

QTEST_GUILESS_MAIN(TstSliceOwnership)
#include "tst_slice_ownership.moc"
