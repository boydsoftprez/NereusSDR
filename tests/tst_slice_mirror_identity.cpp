// =================================================================
// tests/tst_slice_mirror_identity.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Remote Daemon R2 Task 6: SliceModel gains two Q_PROPERTYs the future
// StateMirror (Task 7) needs and currently has no way to carry -- a
// stable per-slice id (sliceIndex) and the slice's current ham/SWL band
// (band).
//
// The interesting case for sliceIndex is a slice created AFTER a
// mid-list removal. RadioModel::addSlice() hands out the lowest free id
// (not m_slices.size()) and removeSlice() never renumbers survivors
// (RadioModel.cpp addSlice() / removeSlice() / sliceById(), defect C3
// comment), so a slice's id and its position in RadioModel::slices() are
// the SAME thing only until the first mid-list removal -- after that
// they diverge, and a mirror keyed by list position would address the
// wrong slice on the remote side.
//
// SliceModel.{h,cpp} already carry Thetis provenance (see
// docs/attribution/THETIS-PROVENANCE.md); the sliceIndex/band exposure
// added by this task is NereusSDR-original wiring for the remote-daemon
// feature, with no Thetis equivalent, matching how the Phase 3F
// sliceLetter/chainIndex/ddcIndex additions to this same class were
// treated (see tst_slice_model_phase3f_properties.cpp).
// =================================================================

#include <QtTest/QtTest>
#include <QMetaProperty>
#include <QSignalSpy>
#include <QVariant>

#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceMirrorIdentity : public QObject {
    Q_OBJECT

private slots:
    // ── sliceIndex ───────────────────────────────────────────────────────

    // Step 2's shape: no WRITE, no NOTIFY. A mirror strategy that only
    // enumerates NOTIFY-bearing properties needs to know this one exists
    // and must be snapshotted a different way -- the CONSTANT-snapshot
    // MirrorPolicy entry recorded next to the Q_PROPERTY declaration in
    // SliceModel.h for Task 7.
    void sliceIndexPropertyExistsAndIsAConstantSnapshot()
    {
        SliceModel slice;
        const QMetaObject* mo = slice.metaObject();

        const int idx = mo->indexOfProperty("sliceIndex");
        QVERIFY2(idx >= 0, "SliceModel has no 'sliceIndex' Q_PROPERTY");

        const QMetaProperty prop = mo->property(idx);
        QVERIFY2(prop.isConstant(),
                 "sliceIndex must be CONSTANT -- it is a stable id assigned "
                 "once at slice creation and never reassigned");
        QVERIFY2(!prop.hasNotifySignal(),
                 "sliceIndex unexpectedly grew a NOTIFY signal; the "
                 "CONSTANT-snapshot MirrorPolicy comment needs updating");
    }

    // The regression this task exists to prevent: a slice created after a
    // mid-list removal must be found by its stable id, not by where it
    // happens to land in RadioModel::slices().
    //
    // Three slices A(0) B(1) C(2); remove the true middle one (B); then
    // add a fourth slice D. RadioModel::addSlice() hands D the lowest
    // free id (1, B's freed slot), but D lands at the END of the
    // surviving list: [A, C, D] at positions [0, 1, 2]. D's sliceIndex
    // (1) and its list position (2) disagree -- that disagreement is the
    // whole point. A mirror keyed by position would resolve D's updates
    // onto C instead.
    void sliceIndexReadOffASliceCreatedAfterAMidListRemovalDivergesFromListPosition()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);

        const int a = radio.addSlice();
        const int b = radio.addSlice();
        const int c = radio.addSlice();
        QCOMPARE(a, 0);
        QCOMPARE(b, 1);
        QCOMPARE(c, 2);

        radio.removeSlice(b);  // the true middle of [A, B, C]
        QCOMPARE(radio.slices().size(), 2);

        const int d = radio.addSlice();  // created AFTER the removal
        QCOMPARE(d, b);                  // reuses the freed id, not size()

        SliceModel* sliceD = radio.sliceById(d);
        QVERIFY(sliceD != nullptr);

        const int listPosition = radio.slices().indexOf(sliceD);
        QCOMPARE(listPosition, 2);  // [A, C, D] -- D is last

        // Read the id through the Q_PROPERTY -- the mechanism StateMirror
        // will actually use -- not just the C++ accessor.
        const QVariant mirrored = sliceD->property("sliceIndex");
        QVERIFY(mirrored.isValid());
        QCOMPARE(mirrored.toInt(), sliceD->sliceIndex());
        QCOMPARE(mirrored.toInt(), d);

        // The assertion that would be trivially true (and prove nothing)
        // if D's id happened to equal its position:
        QVERIFY(mirrored.toInt() != listPosition);
    }

    // ── band ─────────────────────────────────────────────────────────────

    void bandPropertyExistsAndHasANotifySignal()
    {
        SliceModel slice;
        const QMetaObject* mo = slice.metaObject();

        const int idx = mo->indexOfProperty("band");
        QVERIFY2(idx >= 0, "SliceModel has no 'band' Q_PROPERTY");

        const QMetaProperty prop = mo->property(idx);
        QVERIFY2(prop.hasNotifySignal(),
                 "band has no NOTIFY signal; StateMirror cannot follow it "
                 "Outbound");
    }

    // setFrequency already maintains m_currentBand and emits bandChanged on
    // a boundary crossing (SliceModel.cpp:211-214); this proves the new
    // Q_PROPERTY reads the same value the signal just announced, through
    // the meta-object property system a remote mirror will actually use.
    void bandPropertyNotifiesOnABandCrossingSetFrequency()
    {
        SliceModel slice;

        // Ctor defaults: 14.225 MHz, Band20m (SliceModel.h m_frequency /
        // m_currentBand). Confirm the starting point through the property
        // before changing anything.
        QCOMPARE(slice.property("band").value<Band>(), Band::Band20m);

        QSignalSpy bandSpy(&slice, &SliceModel::bandChanged);

        // 7.225 MHz sits inside the 40m ham band range (7.000-7.300 MHz,
        // Band.cpp kHamBandRanges) -- crosses out of 20m.
        slice.setFrequency(7'225'000.0);

        QCOMPARE(bandSpy.count(), 1);
        QCOMPARE(bandSpy.first().at(0).value<Band>(), Band::Band40m);
        QCOMPARE(slice.property("band").value<Band>(), Band::Band40m);
        QCOMPARE(slice.band(), Band::Band40m);
    }

    // Regression guard alongside the notify test above: a frequency move
    // that stays inside the same band must not re-announce it. Protects
    // the Task 7 Outbound mirror from resending the same band on every
    // VFO tick.
    void bandPropertyDoesNotNotifyWithinTheSameBand()
    {
        SliceModel slice;
        QSignalSpy bandSpy(&slice, &SliceModel::bandChanged);

        // Still inside 20m (14.000-14.350 MHz).
        slice.setFrequency(14'250'000.0);

        QCOMPARE(bandSpy.count(), 0);
        QCOMPARE(slice.property("band").value<Band>(), Band::Band20m);
    }
};

QTEST_MAIN(TestSliceMirrorIdentity)
#include "tst_slice_mirror_identity.moc"
