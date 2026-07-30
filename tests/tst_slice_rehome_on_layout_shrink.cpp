// no-port-check: NereusSDR-original. AetherSDR is a thin FlexRadio API client
// whose RadioModel::removePanadapter just sends "display pan remove" and lets
// the radio decide what happens to the slices; NereusSDR owns slice and DDC
// allocation locally, so the workflow shape ports but the mechanics do not.
//
// Codex review, PR #293, P1. Shrinking the pan layout deleted the omitted
// PanadapterApplets, but the slice-side loop in MainWindow only ever ADDED:
//
//     for (int i = existing; i < target; ++i) { addSliceOnPan(...); }
//
// With existing > target that body never runs, so slices whose panKey named a
// deleted pane were left pointing at nothing. Their VFO widgets went with the
// pane, re-expanding the layout did not re-associate them because no
// panKeyChanged was emitted, and meanwhile they still held their DDC, their
// stream and their audio.
//
// Rehomed rather than removed, deliberately. A slice carries the operator's
// frequency, mode, filter and DSP state; discarding that as a side effect of
// choosing a smaller layout is destructive and was never asked for. Moving it
// to a surviving pan keeps every one of those and keeps the slice on screen.
//
// Lives on RadioModel, not in the MainWindow lambda where the defect is,
// because MainWindow is not constructible in this harness. Logic that cannot
// be tested there has already shipped green and failed on the bench twice on
// this branch (see docs/architecture/2026-07-28-phase3f-session-state.md §6).

#include <QtTest/QtTest>

#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceRehomeOnLayoutShrink : public QObject {
    Q_OBJECT
private slots:

    // Four pans down to one: every orphan lands on the survivor.
    void shrinking_the_layout_rehomes_orphaned_slices()
    {
        RadioModel model;
        // addSlice(panId) is what addSliceOnPan delegates to once its
        // maxSlices() gate passes. Used directly here because the gate reads
        // BoardCapabilities, which a bare model leaves at one slice, and the
        // cap is not what this test is about.
        model.configureStreamPool(/*userDdcCount*/ 4, /*maxSlices*/ 4, 192000);
        for (int i = 0; i < 4; ++i) {
            model.addSlice(QStringLiteral("pan-%1").arg(i));
        }
        QCOMPARE(model.slices().size(), 4);

        const int moved = model.rehomeSlicesToPans({QStringLiteral("pan-0")});
        QCOMPARE(moved, 3);

        for (SliceModel* s : model.slices()) {
            QVERIFY(s);
            QVERIFY2(s->panKey() == QStringLiteral("pan-0"),
                qPrintable(QStringLiteral("slice %1 still points at %2, a pane "
                    "that no longer exists").arg(s->sliceIndex()).arg(s->panKey())));
        }
    }

    // Slices already on surviving pans must not be disturbed, or a layout
    // change would silently collapse everything onto one pane.
    void slices_on_surviving_pans_are_left_alone()
    {
        RadioModel model;
        // addSlice(panId) is what addSliceOnPan delegates to once its
        // maxSlices() gate passes. Used directly here because the gate reads
        // BoardCapabilities, which a bare model leaves at one slice, and the
        // cap is not what this test is about.
        model.configureStreamPool(/*userDdcCount*/ 4, /*maxSlices*/ 4, 192000);
        for (int i = 0; i < 4; ++i) {
            model.addSlice(QStringLiteral("pan-%1").arg(i));
        }

        const int moved = model.rehomeSlicesToPans(
            {QStringLiteral("pan-0"), QStringLiteral("pan-1")});
        QCOMPARE(moved, 2);   // pan-2 and pan-3 only

        QCOMPARE(model.slices().at(0)->panKey(), QStringLiteral("pan-0"));
        QCOMPARE(model.slices().at(1)->panKey(), QStringLiteral("pan-1"));
        QCOMPARE(model.slices().at(2)->panKey(), QStringLiteral("pan-0"));
        QCOMPARE(model.slices().at(3)->panKey(), QStringLiteral("pan-0"));
    }

    // The signal is what MainWindow needs in order to move the VFO widget, and
    // its absence is half of why re-expanding the layout left panes blank.
    void rehoming_emits_pan_key_changed()
    {
        RadioModel model;
        model.configureStreamPool(/*userDdcCount*/ 4, /*maxSlices*/ 4, 192000);
        model.addSlice(QStringLiteral("pan-0"));
        model.addSlice(QStringLiteral("pan-1"));
        QCOMPARE(model.slices().size(), 2);
        SliceModel* sliceB = model.slices().at(1);
        QVERIFY(sliceB);
        QCOMPARE(sliceB->panKey(), QStringLiteral("pan-1"));

        QSignalSpy spy(sliceB, &SliceModel::panKeyChanged);
        QVERIFY(spy.isValid());

        model.rehomeSlicesToPans({QStringLiteral("pan-0")});

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("pan-0"));
    }

    // A no-op must stay a no-op: nothing moves, nothing is emitted.
    void no_orphans_moves_nothing()
    {
        RadioModel model;
        model.addSlice(QStringLiteral("pan-0"));
        SliceModel* slice = model.slices().at(0);
        QSignalSpy spy(slice, &SliceModel::panKeyChanged);

        const int moved = model.rehomeSlicesToPans({QStringLiteral("pan-0")});
        QCOMPARE(moved, 0);
        QCOMPARE(spy.count(), 0);
    }

    // Defensive: an empty survivor list means there is nowhere to move to.
    // Leaving the slices pointing at their old panes is strictly better than
    // pointing them at an empty string, which no pane will ever match.
    void an_empty_pan_list_leaves_slices_untouched()
    {
        RadioModel model;
        model.addSlice(QStringLiteral("pan-0"));

        const int moved = model.rehomeSlicesToPans({});
        QCOMPARE(moved, 0);
        QCOMPARE(model.slices().at(0)->panKey(), QStringLiteral("pan-0"));
    }
};

QTEST_MAIN(TestSliceRehomeOnLayoutShrink)
#include "tst_slice_rehome_on_layout_shrink.moc"
