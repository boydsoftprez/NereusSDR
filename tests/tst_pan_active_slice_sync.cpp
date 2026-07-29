// =================================================================
// tests/tst_pan_active_slice_sync.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Nothing here
// is a port; the surfaces under test are fixed by
// docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md
// §3 (Slice / Pan binding), which is a NereusSDR design document.
//
// Bench report, two slices on one pan, 2026-07-28: "when I click to tune
// it always tunes flag A, not the last selected."
//
// There are two independent notions of "active slice" and nothing kept
// them in step:
//
//   1. RadioModel::activeSlice() -- global, one per application. Every
//      route the operator has for selecting a slice (a VfoWidget flag
//      press, an RxApplet slice tab, a band button) lands here.
//
//   2. PanadapterApplet::activeSliceIndex() -- per pan. This is what
//      MainWindow::sliceForPan resolves, so it is what click-to-tune,
//      the filter-edge drag, the CH tag and the pan TX pill all act on.
//
// PanadapterApplet::addSlice seeds (2) once, when the pan has no active
// slice yet, and nothing else ever wrote it. So a pan latched onto the
// first slice added to it and stayed there for the session: selecting
// flag B moved (1) and left (2) pointing at A, and the next click on the
// spectrum retuned A.
//
// Two more defects sit on the same path and are pinned here too:
//
//   * The activation wires hand a slice ID to RadioModel::setActiveSlice,
//     which takes a LIST POSITION. Ids and positions diverge after any
//     mid-list removal (removeSlice does not renumber survivors), so
//     clicking flag C once B has been closed selected the wrong slice or,
//     when the id ran off the end of the list, silently nothing.
//
//   * A slice migrating between pans left its old pan still listing it in
//     associatedSlices(), and therefore able to keep it as that pan's
//     active slice -- a pan acting on a slice it no longer hosts.
// =================================================================

#include <QtTest/QtTest>

#include <QSignalSpy>

#include "core/AppSettings.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanadapterStack.h"
#include "gui/MainWindow.h"
#include "gui/SpectrumWidget.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// The two lines MainWindow puts on RadioModel::activeSliceChanged. Mirrored
// here because MainWindow is not constructible in this harness (see
// tst_mainwindow_status_bar_safety.cpp), so the composition of "operator
// selects a slice" -> "the hosting pan follows" cannot be driven through the
// production object. The mirror is deliberately one statement long: all the
// logic worth testing lives in PanadapterStack::setActiveSliceOnHostingPan,
// which the cases above it drive directly.
//
// activeSliceChanged carries a LIST POSITION, so the id has to be resolved
// through activeSlice()->sliceIndex() rather than used as it arrives.
void mirrorMainWindowSyncWire(RadioModel& model, PanadapterStack& stack)
{
    QObject::connect(&model, &RadioModel::activeSliceChanged,
                     &stack, [&model, &stack](int) {
        if (SliceModel* active = model.activeSlice()) {
            stack.setActiveSliceOnHostingPan(active->sliceIndex());
        }
    });
}

} // namespace

class TestPanActiveSliceSync : public QObject {
    Q_OBJECT
private slots:

    // ── RadioModel::setActiveSliceById: the id/position conversion ─────────

    // A(0) B(1) C(2), close B, and the list is [A(0), C(2)]: C sits at
    // position 1 while its id stays 2. Every per-slice UI surface carries the
    // stable id -- VfoWidget::sliceActivationRequested emits the value
    // createSliceFlag stamped from SliceModel::sliceIndex(), and
    // RxApplet::updateSliceButtons keys its button group the same way -- so
    // handing that id to the positional setter asked for position 2 of a
    // two-element list and selected nothing at all.
    void activationByIdSurvivesAMidListRemoval()
    {
        RadioModel model;
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);

        const int a = model.addSlice(QStringLiteral("pan-0"));
        const int b = model.addSlice(QStringLiteral("pan-0"));
        const int c = model.addSlice(QStringLiteral("pan-0"));
        QCOMPARE(a, 0);
        QCOMPARE(b, 1);
        QCOMPARE(c, 2);

        model.removeSlice(b);
        QCOMPARE(model.slices().size(), 2);
        // The divergence this test exists for: id 2 now sits at position 1.
        QCOMPARE(model.slices().at(1)->sliceIndex(), c);

        QVERIFY(model.setActiveSliceById(c));
        QVERIFY(model.activeSlice() != nullptr);
        QCOMPARE(model.activeSlice()->sliceIndex(), c);
    }

    // An id no slice carries must leave the active slice where it was rather
    // than clearing it, so a stale click cannot strand every active-slice
    // surface on nullptr.
    void activationByUnknownIdChangesNothing()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice(QStringLiteral("pan-0"));
        const int b = model.addSlice(QStringLiteral("pan-0"));
        QVERIFY(model.setActiveSliceById(b));

        QVERIFY(!model.setActiveSliceById(97));
        QVERIFY(model.activeSlice() != nullptr);
        QCOMPARE(model.activeSlice()->sliceIndex(), b);
        Q_UNUSED(a);
    }

    void antenna_selection_for_c_changes_c_after_b_is_removed()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        SliceModel* a = model.sliceById(model.addSlice());
        const int b = model.addSlice();
        SliceModel* c = model.sliceById(model.addSlice());
        QVERIFY(a);
        QVERIFY(c);
        model.removeSlice(b);
        QCOMPARE(model.slices().indexOf(c), 1);

        a->setRxAntenna(QStringLiteral("ANT1"));
        c->setRxAntenna(QStringLiteral("ANT1"));
        MainWindow::applyAntennaChangeForTest(&model, c->sliceIndex(),
                                               QStringLiteral("ANT3"));

        QCOMPARE(c->rxAntenna(), QStringLiteral("ANT3"));
        QCOMPARE(a->rxAntenna(), QStringLiteral("ANT1"));
    }

    void slice_added_subscriber_resolves_c_by_stable_id_after_b_is_removed()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        model.addSlice();
        const int b = model.addSlice();
        SliceModel* c = model.sliceById(model.addSlice());
        QVERIFY(c);
        model.removeSlice(b);
        QCOMPARE(model.slices().indexOf(c), 1);

        QCOMPARE(MainWindow::sliceForAddedIdForTest(&model, c->sliceIndex()), c);
    }

    void rade_updates_are_scoped_to_the_matching_flag_id()
    {
        RadioModel model;
        VfoWidget flagA;
        VfoWidget flagC;
        flagA.setSliceIndex(0);
        flagC.setSliceIndex(2);
        flagA.setRadeActive(true);
        flagA.setRadeSnrLabel(-1.0f);
        flagC.setRadeActive(true);
        flagC.setRadeSnrLabel(-7.0f);

        MainWindow::wireRadeFlagForTest(&model, &flagA, 0);
        MainWindow::wireRadeFlagForTest(&model, &flagC, 2);

        emit model.radeSyncChanged(0, false);
        QVERIFY(flagA.snrLabelForTest()->text().contains(QStringLiteral("RADE")));
        QVERIFY(flagA.snrLabelForTest()->text().contains(QStringLiteral("#505050")));
        QVERIFY(flagA.snrLabelForTest()->text().contains(QStringLiteral("---")));
        QVERIFY(flagC.snrLabelForTest()->text().contains(QStringLiteral("-7")));

        emit model.radeFreqOffsetChanged(2, 125.0f);
        QVERIFY(flagC.snrLabelForTest()->text().contains(QStringLiteral("+125Hz")));
        QVERIFY(!flagA.snrLabelForTest()->text().contains(QStringLiteral("+125Hz")));
    }

    void wideband_bins_are_fanned_to_every_pan()
    {
        PanadapterStack stack;
        PanadapterApplet* pan0 = stack.panadapter(QStringLiteral("pan-0"));
        PanadapterApplet* pan1 = stack.addPanadapter(QStringLiteral("pan-1"));
        QVERIFY(pan0);
        QVERIFY(pan1);

        const QVector<float> adc0{-101.0f, -99.0f};
        const QVector<float> adc1{-88.0f, -87.0f, -86.0f};
        MainWindow::fanWidebandBinsForTest(&stack, 0, adc0);
        MainWindow::fanWidebandBinsForTest(&stack, 1, adc1);

        QCOMPARE(pan0->spectrumWidget()->widebandBinsForTest(0), adc0);
        QCOMPARE(pan0->spectrumWidget()->widebandBinsForTest(1), adc1);
        QCOMPARE(pan1->spectrumWidget()->widebandBinsForTest(0), adc0);
        QCOMPARE(pan1->spectrumWidget()->widebandBinsForTest(1), adc1);
    }

    // ── PanadapterStack::setActiveSliceOnHostingPan ────────────────────────

    // The reported defect, at the layer that fixes it. addSlice seeds the pan
    // with A and refuses to move afterwards; selecting B has to move it.
    void hostingPanFollowsTheSelectedSlice()
    {
        PanadapterStack stack;
        PanadapterApplet* pan0 = stack.panadapter(QStringLiteral("pan-0"));
        QVERIFY(pan0);

        pan0->addSlice(0);
        pan0->addSlice(1);
        QCOMPARE(pan0->activeSliceIndex(), 0);   // latched on the first added

        stack.setActiveSliceOnHostingPan(1);
        QCOMPARE(pan0->activeSliceIndex(), 1);
    }

    // Fourth bench report on this same wiring, Sub-Epic J: "with Slice A
    // selected, Slice B's flag covered A's, clipping A's frequency readout."
    // hostingPanFollowsTheSelectedSlice above proves setActiveSliceOnHostingPan
    // retargets the right pan's activeSliceIndex; this proves the SAME call,
    // through the SAME production path (PanadapterStack::
    // setActiveSliceOnHostingPan -> PanadapterApplet::setActiveSliceIndex ->
    // SpectrumWidget::setFrontSliceIndex), also reorders that slice's REAL
    // flag on the pan's REAL, owned SpectrumWidget -- the exact object
    // MainWindow's activeSliceChanged lambda drives, nothing mirrored or
    // stood in for it.
    //
    // Two earlier fixes on this branch shipped with passing tests and did
    // not work on the bench because the real behaviour lived in an
    // untestable MainWindow lambda. This pins the chain one layer below that
    // lambda; the lambda itself is one line
    // (m_panStack->setActiveSliceOnHostingPan(...)), covered by
    // mirrorMainWindowSyncWire below.
    void selectingASliceRaisesItsRealFlagOnTheHostingPan()
    {
        PanadapterStack stack;
        PanadapterApplet* pan0 = stack.panadapter(QStringLiteral("pan-0"));
        QVERIFY(pan0);

        pan0->addSlice(0);
        pan0->addSlice(1);

        SpectrumWidget* spectrum = pan0->spectrumWidget();
        QVERIFY(spectrum);
        VfoWidget* flagA = spectrum->addVfoWidget(0);
        VfoWidget* flagB = spectrum->addVfoWidget(1);
        QVERIFY(flagA);
        QVERIFY(flagB);
        flagA->setFrequency(14'150'000.0);
        flagB->setFrequency(14'250'000.0);

        QWidget* parent = flagA->parentWidget();
        QVERIFY(parent);

        // Select B through the real production call.
        stack.setActiveSliceOnHostingPan(1);
        spectrum->updateVfoPositions();  // the render-frame pass that follows
        QVERIFY2(parent->children().indexOf(flagB) > parent->children().indexOf(flagA),
                 "selecting B must bring its real flag to the front");

        // The operator's reported repro, one step further: select A back.
        // A one-shot raise that does not survive updateVfoPositions() (which
        // ran once already above, and runs again here) would leave B on top
        // -- B has the higher slice index, which is what the loop falls back
        // to on every pass with no pin held.
        stack.setActiveSliceOnHostingPan(0);
        spectrum->updateVfoPositions();
        QVERIFY2(parent->children().indexOf(flagA) > parent->children().indexOf(flagB),
                 "selecting A afterward must bring ITS real flag back to the "
                 "front, even though B has the higher slice index");
    }

    // The standing project rule is that a control drawn on a pan acts on that
    // pan. Selecting a slice must therefore retarget the pan that hosts it and
    // no other: pan-1's click-to-tune keeps tuning pan-1's own slice.
    void aPanThatDoesNotHostTheSliceIsLeftAlone()
    {
        PanadapterStack stack;
        PanadapterApplet* pan0 = stack.panadapter(QStringLiteral("pan-0"));
        PanadapterApplet* pan1 = stack.addPanadapter(QStringLiteral("pan-1"));
        QVERIFY(pan0);
        QVERIFY(pan1);

        pan0->addSlice(0);
        pan0->addSlice(1);
        pan1->addSlice(2);
        QCOMPARE(pan0->activeSliceIndex(), 0);
        QCOMPARE(pan1->activeSliceIndex(), 2);

        stack.setActiveSliceOnHostingPan(1);
        QCOMPARE(pan0->activeSliceIndex(), 1);
        QCOMPARE(pan1->activeSliceIndex(), 2);   // untouched
    }

    // A slice no pan hosts (created before its pan exists, or mid-migration)
    // must not push any pan onto a slice it cannot show.
    void syncForASliceNoPanHostsChangesNothing()
    {
        PanadapterStack stack;
        PanadapterApplet* pan0 = stack.panadapter(QStringLiteral("pan-0"));
        QVERIFY(pan0);
        pan0->addSlice(0);

        stack.setActiveSliceOnHostingPan(4);
        QCOMPARE(pan0->activeSliceIndex(), 0);
    }

    // ── Composition: selecting a flag retargets that pan's click-to-tune ───

    // End to end from the signal the operator's click raises, with the
    // MainWindow connect mirrored (see mirrorMainWindowSyncWire above).
    // pan0->activeSliceIndex() is exactly what MainWindow::sliceForPan
    // resolves before calling SliceModel::setFrequency, so asserting on it is
    // asserting on which slice a click on the spectrum retunes.
    void flagSelectionRetargetsClickToTuneOnThatPan()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        PanadapterStack stack;
        mirrorMainWindowSyncWire(model, stack);

        PanadapterApplet* pan0 = stack.panadapter(QStringLiteral("pan-0"));
        QVERIFY(pan0);

        const int a = model.addSlice(QStringLiteral("pan-0"));
        const int b = model.addSlice(QStringLiteral("pan-0"));
        pan0->addSlice(a);
        pan0->addSlice(b);
        QCOMPARE(pan0->activeSliceIndex(), a);

        // The operator presses flag B. VfoWidget emits its slice ID.
        QVERIFY(model.setActiveSliceById(b));

        QCOMPARE(pan0->activeSliceIndex(), b);
    }

    // ── Migration: a pan must never point at a slice it no longer hosts ────

    void migratingASliceMovesItsPanAssociation()
    {
        PanadapterStack stack;
        PanadapterApplet* pan0 = stack.panadapter(QStringLiteral("pan-0"));
        PanadapterApplet* pan1 = stack.addPanadapter(QStringLiteral("pan-1"));
        QVERIFY(pan0);
        QVERIFY(pan1);

        pan0->addSlice(0);
        pan0->addSlice(1);

        stack.moveSliceToPan(1, QStringLiteral("pan-1"));

        QVERIFY(!pan0->associatedSlices().contains(1));
        QVERIFY(pan1->associatedSlices().contains(1));
        QCOMPARE(pan0->activeSliceIndex(), 0);
        QCOMPARE(pan1->activeSliceIndex(), 1);
    }

    // The pan the slice left had it as its active one. Leaving the pointer
    // behind is the failure this guards: the pan would keep tuning, and keep
    // painting the CH tag for, a slice that now belongs to another pan.
    void migratingThePansOnlySliceClearsItsActivePointer()
    {
        PanadapterStack stack;
        PanadapterApplet* pan0 = stack.panadapter(QStringLiteral("pan-0"));
        PanadapterApplet* pan1 = stack.addPanadapter(QStringLiteral("pan-1"));
        QVERIFY(pan0);
        QVERIFY(pan1);

        pan0->addSlice(3);
        QCOMPARE(pan0->activeSliceIndex(), 3);

        stack.moveSliceToPan(3, QStringLiteral("pan-1"));

        QVERIFY(pan0->associatedSlices().isEmpty());
        QCOMPARE(pan0->activeSliceIndex(), -1);
        QCOMPARE(pan1->activeSliceIndex(), 3);
    }

    // Migrating to a pan that does not exist must not silently drop the slice
    // off every pan; the operator would be left with a flag no pan hosts and
    // click-to-tune dead on the pan it came from.
    void migratingToAnUnknownPanLeavesTheAssociationAlone()
    {
        PanadapterStack stack;
        PanadapterApplet* pan0 = stack.panadapter(QStringLiteral("pan-0"));
        QVERIFY(pan0);
        pan0->addSlice(0);

        stack.moveSliceToPan(0, QStringLiteral("pan-9"));

        QVERIFY(pan0->associatedSlices().contains(0));
        QCOMPARE(pan0->activeSliceIndex(), 0);
    }
};

QTEST_MAIN(TestPanActiveSliceSync)
#include "tst_pan_active_slice_sync.moc"
