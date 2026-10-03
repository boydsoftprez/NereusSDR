// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_receiver_anchor.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 74 (R-IOS-02, R-IOS-30; the several-devices design,
// sections 6.1 and 6.2, ruling 6.2): each receiver's anchor.
//
// The anchor decides who may move a shared receiver's window and whose its
// C-Tune pin is, so these are authorisation tests:
//   - the device whose slice claimed a receiver anchors it; a slice that
//     joins does not take the anchor;
//   - when the anchor's last slice leaves, the anchor passes to the device
//     whose slice has been on the receiver longest; nobody else's slice
//     moving changes it;
//   - when the last slice leaves, the receiver is free (no anchor);
//   - a slice held for a device anchors for that device; adoption and a
//     returned slice carry the anchor to the new owner;
//   - through RadioModel's own placement (the allocator's NewStream and
//     JoinedExisting outcomes), the same rules hold.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 74 (R-IOS-02, R-IOS-30),
//               with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include "core/AppSettings.h"
#include "core/SliceOwnership.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

const QByteArray kA = QByteArrayLiteral("device-a");
const QByteArray kB = QByteArrayLiteral("device-b");
const QByteArray kC = QByteArrayLiteral("device-c");

void add(SliceOwnership& own, int id, const QByteArray& creator, int stream)
{
    SliceOwnership::CreatorScope scope(&own, creator);
    own.noteStream(id, stream);
    own.noteSliceAdded(id);
}

} // namespace

class TstReceiverAnchor : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(
            QStringLiteral("receiver-anchor-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    void theDeviceWhoseSliceClaimedAReceiverAnchorsIt()
    {
        SliceOwnership own;
        add(own, 0, kA, 0);
        QCOMPARE(own.anchorOf(0), kA);
        // B's slice joins: A still anchors.
        add(own, 1, kB, 0);
        QCOMPARE(own.anchorOf(0), kA);
        QCOMPARE(own.slicesOnStreamInJoinOrder(0), (QList<int>{0, 1}));
        // A receiver nobody is on has no anchor.
        QVERIFY(own.anchorOf(1).isEmpty());
    }

    void theAnchorPassesToTheLongestStayingDeviceWhenItsLastSliceLeaves()
    {
        SliceOwnership own;
        add(own, 0, kA, 0);
        add(own, 1, kA, 0);
        add(own, 2, kB, 0);
        add(own, 3, kC, 0);
        // One of A's two slices leaves: A still anchors.
        own.noteStream(0, 1);
        QCOMPARE(own.anchorOf(0), kA);
        // C's slice leaving changes nothing.
        own.noteStream(3, -1);
        QCOMPARE(own.anchorOf(0), kA);
        // C's slice comes back after B's: B has been on it longest.
        own.noteStream(3, 0);
        own.noteStream(1, 2);
        QCOMPARE(own.anchorOf(0), kB);
        // A's slice claimed receiver 1 alone, and a slice of A on 2.
        QCOMPARE(own.anchorOf(1), kA);
        QCOMPARE(own.anchorOf(2), kA);
    }

    void aReceiverWhoseLastSliceLeftIsFree()
    {
        SliceOwnership own;
        add(own, 0, kA, 0);
        add(own, 1, kB, 0);
        own.noteStream(0, -1);
        QCOMPARE(own.anchorOf(0), kB);
        own.beginRemove(1);
        own.noteStream(1, -1);
        own.endRemove(1);
        QVERIFY(own.anchorOf(0).isEmpty());
        QVERIFY(own.slicesOnStreamInJoinOrder(0).isEmpty());
        // The next slice there claims it afresh.
        add(own, 2, kC, 0);
        QCOMPARE(own.anchorOf(0), kC);
    }

    void aHeldSliceAnchorsForItsDeviceAndAnOwnerChangeCarriesTheAnchor()
    {
        SliceOwnership own;
        // A slice with no owner claims receiver 0; A adopts it.
        add(own, 0, QByteArray(), 0);
        QVERIFY(own.anchorOf(0).isEmpty());
        add(own, 1, kB, 0);
        QVERIFY(own.anchorOf(0).isEmpty());
        own.adoptUnowned(kA);
        QCOMPARE(own.anchorOf(0), kA);
        // Held for A while A is away: it still anchors for A.
        own.hold(0, kA);
        QCOMPARE(own.anchorOf(0), kA);
        own.returnHeld(kA);
        QCOMPARE(own.anchorOf(0), kA);
        // Given to C (as a take-back would recreate it): C anchors.
        own.setOwner(0, kC);
        QCOMPARE(own.anchorOf(0), kC);
    }

    void aSliceMadeBeforeItsMarkAnchorsOnceItIsNoted()
    {
        // RadioModel binds a new slice before it notes its owner.
        SliceOwnership own;
        own.noteStream(4, 1);
        QVERIFY(own.anchorOf(1).isEmpty());
        {
            SliceOwnership::CreatorScope scope(&own, kB);
            own.noteSliceAdded(4);
        }
        QCOMPARE(own.anchorOf(1), kB);
    }

    // Through RadioModel's placement: NewStream claims, JoinedExisting joins.
    void radioModelPlacementClaimsAndJoins()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLite);
        model.configureStreamPool(2, 5, 192000);
        SliceOwnership* own = model.sliceOwnership();
        int a = -1;
        {
            SliceOwnership::CreatorScope scope(own, kA);
            a = model.addSlice(QStringLiteral("pan-a"));
        }
        QVERIFY(a >= 0);
        model.sliceById(a)->setFrequency(7074000.0);
        const int stream = model.sliceById(a)->streamIndex();
        QVERIFY(stream >= 0);
        QCOMPARE(own->anchorOf(stream), kA);
        int b = -1;
        {
            SliceOwnership::CreatorScope scope(own, kB);
            b = model.addSlice(QString());
        }
        QVERIFY(b >= 0);
        model.sliceById(b)->setFrequency(7080000.0);
        QCOMPARE(model.sliceById(b)->streamIndex(), stream);
        QCOMPARE(own->anchorOf(stream), kA);
        // A's slice leaves for another band (a receiver of its own): B's is
        // the only one left on the first receiver and anchors it.
        model.sliceById(a)->setFrequency(14074000.0);
        QVERIFY(model.sliceById(a)->streamIndex() != stream);
        QCOMPARE(own->anchorOf(stream), kB);
        QCOMPARE(own->anchorOf(model.sliceById(a)->streamIndex()), kA);
    }
};

QTEST_GUILESS_MAIN(TstReceiverAnchor)
#include "tst_receiver_anchor.moc"
