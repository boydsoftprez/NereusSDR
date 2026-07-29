// =================================================================
// tests/tst_pan_status_overlay.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Nothing here
// is a port; the surfaces under test are fixed by
// docs/architecture/2026-05-26-phase3f-sub-epic-e-ui-atlas-plan.md
// Task 2, which is a NereusSDR design document.
//
// Phase 3F: the per-pan status overlay must paint the state of the
// slice that pan is actually showing.
//
// PanadapterApplet::updateStatusOverlay shipped with zero production
// callers (Sub-Epic E Task 2 Step 4 was written in the plan and never
// executed), so on a live radio every pan painted the widget's
// construction-time placeholders -- slice "A", "0.000", "USB", "CH 0" --
// no matter what it was tuned to. The WIDE pill next to them went live
// in 00ab9522, so a correctly lit WIDE badge sat beside a stale mode and
// a zero frequency.
//
// Two resolutions are under test, and the point is that they are ONE
// resolution shared with the WIDE badge rather than two that can drift:
//
//   which slice  ->  the pan's own activeSliceIndex()
//   which chain  ->  slice -> its stream -> that stream's ADC
//                    (RadioModel::sliceChainIndex, which
//                     RadioModel::panBypassState also calls)
// =================================================================

#include <QtTest/QtTest>

#include <QMetaProperty>

#include "core/AppSettings.h"
#include "core/DdcAssignment.h"
#include "core/ReceiverManager.h"
#include "gui/MainWindow.h"
#include "gui/PanadapterApplet.h"
#include "gui/widgets/SpectrumStatusOverlay.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// ReceiverManager only knows about receivers that were explicitly created,
// and an unknown index silently reports the default ADC0
// (ReceiverManager.cpp:219-222, 315-322). Production creates one receiver
// per stream at connect; these cases have no connection, so they stand the
// pool up themselves or every slice lands on chain 0 and the per-chain
// assertions below would pass for the wrong reason.
// Same helper contract as tst_pan_wide_badge.cpp.
// Stand up the two things a per-chain case needs and a bare RadioModel has
// not got.
//
// 1. A BOARD WITH TWO FILTER CHAINS. RadioModel::chainForStream folds any ADC
//    at or above BoardCapabilities::rxFilterChainCount onto chain 0, because
//    on a one-chain board both ADCs really do sit behind the one preselector
//    (defect D4). A model with no board reports the Unknown row, which is one
//    chain, so without this every pin below would fold to chain 0 and these
//    cases would assert nothing. Saturn is the ANAN-G2, the SKU the per-chain
//    routing exists for.
// 2. A RECEIVER PER STREAM. ReceiverManager only knows about receivers that
//    were explicitly created, and an unknown index silently reports the
//    default (ReceiverManager.cpp:219-222). Production creates one per stream
//    at connect (RadioModel.cpp:5334-5338); these cases have no connection.
void seedTwoChainRadio(RadioModel& model, int streamCount)
{
    model.setBoardForTest(HPSDRHW::Saturn);

    ReceiverManager* rm = model.receiverManager();
    QVERIFY(rm);
    for (int st = 0; st < streamCount; ++st) {
        if (rm->receiverConfig(st).receiverIndex < 0) {
            rm->createReceiver();
        }
    }
}

SliceModel* addSliceAt(RadioModel& model, double hz, DSPMode mode)
{
    const int id = model.addSlice();
    SliceModel* slice = model.sliceById(id);
    if (slice) {
        slice->setFrequency(hz);
        slice->setDspMode(mode);
    }
    return slice;
}

// Put each slice's stream on a given ADC, the way production does it.
//
// These cases used to call ReceiverManager::setAdcForReceiver. That stopped
// being the source of truth with defect D1: the per-stream ADC now reaches the
// model through exactly one door, a codec composing a DdcAssignment whose
// adcCtrl bytes RadioModel::publishDdcAssignment decodes. Seeding the old
// field leaves every slice on chain 0 while the per-chain assertions below
// pass for the wrong reason, which is the failure mode the preconditions in
// these cases were written to catch.
//
// One call carrying the whole map rather than one per slice, because that is
// the shape of an assignment: the codec places every stream at once.
//
// DDC numbers are the Saturn / OrionMkII user table {2, 3, 4, 5, 6}
// (P2CodecOrionMkII::applyDdcAssignment), so a five-stream pool spans BOTH
// control bytes: DDC2 / DDC3 sit in adcCtrl1, DDC4-6 in adcCtrl2. Encoding is
// two bits per DDC, from Thetis setup.cs:16928-16942 [v2.10.3.15].
void pinToAdcs(RadioModel& model, const QList<QPair<SliceModel*, int>>& pins)
{
    static constexpr int kStreamToDdc[5] = {2, 3, 4, 5, 6};

    DdcAssignment a{};
    for (int st = 0; st < 5; ++st) {
        a.streamDdc[st] = kStreamToDdc[st];
        a.ddcEnable |= (1 << kStreamToDdc[st]);
    }

    for (const QPair<SliceModel*, int>& pin : pins) {
        QVERIFY(pin.first);
        const int st = pin.first->streamIndex();
        QVERIFY2(st >= 0 && st < 5,
                 "slice never bound a stream; the ADC pin has nothing to attach to");
        const int ddc = kStreamToDdc[st];
        if (ddc < 4) {
            a.adcCtrl1 = (a.adcCtrl1 & ~(3 << (ddc * 2)))
                       | (pin.second << (ddc * 2));
        } else {
            const int sh = (ddc - 4) * 2;
            a.adcCtrl2 = (a.adcCtrl2 & ~(3 << sh)) | (pin.second << sh);
        }
    }

    model.publishDdcAssignmentForTest(a);
}

// The fan-out exactly as MainWindow::refreshPanStatusOverlays drives it:
// for each pan, resolve its active slice and that slice's chain from the
// model, then push both at the pan. Mirrors the shape tst_pan_wide_badge.cpp
// uses for the WIDE badge so the two stay comparable.
void refreshOverlays(RadioModel& model,
                     const QList<PanadapterApplet*>& pans)
{
    for (PanadapterApplet* pan : pans) {
        if (!pan) { continue; }
        SliceModel* slice = model.sliceById(pan->activeSliceIndex());
        if (!slice) { continue; }
        pan->updateStatusOverlay(slice,
                                 model.sliceChainIndex(pan->activeSliceIndex()));
    }
}

} // namespace

class TestPanStatusOverlay : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    // ── The defect: a pan painted placeholders, not its slice ─────────────

    // Everything the overlay shows is a construction-time constant until
    // something calls updateStatusOverlay. This pins the placeholder set so
    // the case below is demonstrably asserting a change, not a coincidence.
    void untouched_overlay_still_reads_the_placeholders()
    {
        PanadapterApplet pan(QStringLiteral("pan-0"));
        QCOMPARE(pan.statusSliceLetter(), QChar('A'));
        QCOMPARE(pan.statusFrequencyHz(), Q_INT64_C(0));
        QCOMPARE(pan.statusMode(), QStringLiteral("USB"));
        QCOMPARE(pan.statusChainIndex(), 0);
    }

    // One slice, one pan: the single-slice case an operator actually runs
    // today. Tuned deliberately off every placeholder (40 m, LSB) so a
    // regression to the hardcoded values cannot pass.
    void pan_shows_its_own_slice_and_not_the_placeholder()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        seedTwoChainRadio(model, 5);

        SliceModel* a = addSliceAt(model, 7150000.0, DSPMode::LSB);
        QVERIFY(a);

        PanadapterApplet pan(QStringLiteral("pan-0"));
        pan.addSlice(a->sliceIndex());
        refreshOverlays(model, {&pan});

        QCOMPARE(pan.statusFrequencyHz(), Q_INT64_C(7150000));
        QCOMPARE(pan.statusMode(), QStringLiteral("LSB"));
        QCOMPARE(pan.statusSliceLetter(), QChar('A'));
    }

    // Two pans, two slices. The whole point of a PER-pan overlay: if the
    // fan-out were global (or read the globally-active slice) both pans
    // would report the same thing and this is the case that catches it.
    void two_pans_show_their_own_slices()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        seedTwoChainRadio(model, 5);

        SliceModel* a = addSliceAt(model, 14200000.0, DSPMode::USB);
        SliceModel* b = addSliceAt(model, 21200000.0, DSPMode::CWU);
        QVERIFY(a && b);

        PanadapterApplet panA(QStringLiteral("pan-0"));
        panA.addSlice(a->sliceIndex());
        PanadapterApplet panB(QStringLiteral("pan-1"));
        panB.addSlice(b->sliceIndex());
        refreshOverlays(model, {&panA, &panB});

        QCOMPARE(panA.statusFrequencyHz(), Q_INT64_C(14200000));
        QCOMPARE(panA.statusMode(), QStringLiteral("USB"));
        QCOMPARE(panA.statusSliceLetter(), QChar('A'));

        QCOMPARE(panB.statusFrequencyHz(), Q_INT64_C(21200000));
        QCOMPARE(panB.statusMode(), QStringLiteral("CWU"));
        // Slice letter is derived from the slice id, the same way every
        // other display site derives it (RadioModel.cpp:12796,
        // RxApplet.cpp:1298). SliceModel::sliceLetter() is never written in
        // production, so reading it would report 'A' for every slice and
        // both pans would claim to be slice A.
        QCOMPARE(panB.statusSliceLetter(), QChar('B'));

        QVERIFY2(panA.statusFrequencyHz() != panB.statusFrequencyHz(),
                 "both pans report one frequency; the overlay is global, not per-pan");
    }

    // A pan hosting several slices shows its ACTIVE one, per Sub-Epic E
    // plan Task 2 Step 4. Switching the active slice moves the overlay.
    void multi_slice_pan_follows_its_active_slice()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        seedTwoChainRadio(model, 5);

        SliceModel* a = addSliceAt(model, 14200000.0, DSPMode::USB);
        SliceModel* b = addSliceAt(model,  7150000.0, DSPMode::LSB);
        QVERIFY(a && b);

        PanadapterApplet pan(QStringLiteral("pan-0"));
        pan.addSlice(a->sliceIndex());   // first slice added becomes active
        pan.addSlice(b->sliceIndex());
        refreshOverlays(model, {&pan});
        QCOMPARE(pan.statusFrequencyHz(), Q_INT64_C(14200000));

        pan.setActiveSliceIndex(b->sliceIndex());
        refreshOverlays(model, {&pan});
        QCOMPARE(pan.statusFrequencyHz(), Q_INT64_C(7150000));
        QCOMPARE(pan.statusMode(), QStringLiteral("LSB"));
    }

    // Retune follows. This is the trigger an operator exercises constantly
    // (every VFO detent) and the one most obviously broken while the
    // overlay read a hardcoded 0.000.
    void retuning_a_slice_updates_its_pan()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        seedTwoChainRadio(model, 5);

        SliceModel* a = addSliceAt(model, 14200000.0, DSPMode::USB);
        QVERIFY(a);

        PanadapterApplet pan(QStringLiteral("pan-0"));
        pan.addSlice(a->sliceIndex());
        refreshOverlays(model, {&pan});
        QCOMPARE(pan.statusFrequencyHz(), Q_INT64_C(14200000));

        a->setFrequency(14250000.0);
        refreshOverlays(model, {&pan});
        QCOMPARE(pan.statusFrequencyHz(), Q_INT64_C(14250000));

        a->setDspMode(DSPMode::CWL);
        refreshOverlays(model, {&pan});
        QCOMPARE(pan.statusMode(), QStringLiteral("CWL"));
    }

    // ── The CH tag resolves the way the WIDE badge resolves ───────────────

    // CH N is the ADC chain, and the badge sitting beside it already
    // resolves that as slice -> stream -> ADC. Reading
    // SliceModel::chainIndex() instead (as updateStatusOverlay originally
    // did) is a second resolution that reports 0 for everything, because
    // setChainIndex has no production caller.
    void chain_tag_reflects_the_slices_actual_adc()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        seedTwoChainRadio(model, 5);

        SliceModel* a = addSliceAt(model, 14200000.0, DSPMode::USB);
        SliceModel* b = addSliceAt(model, 21200000.0, DSPMode::USB);
        QVERIFY(a && b);
        pinToAdcs(model, {{a, 0}, {b, 1}});

        PanadapterApplet panA(QStringLiteral("pan-0"));
        panA.addSlice(a->sliceIndex());
        PanadapterApplet panB(QStringLiteral("pan-1"));
        panB.addSlice(b->sliceIndex());
        refreshOverlays(model, {&panA, &panB});

        QCOMPARE(panA.statusChainIndex(), 0);
        QCOMPARE(panB.statusChainIndex(), 1);
        QVERIFY2(panB.statusChainIndex() != 0,
                 "CH tag stuck on chain 0; it is reading SliceModel::chainIndex(), "
                 "which nothing in production ever writes");
    }

    // The single resolver, asserted directly: the number the CH tag paints
    // is the number panBypassState groups by. If these ever diverge, a pan
    // can say CH 0 while its WIDE pill reports chain 1's bypass state.
    void chain_resolution_is_shared_with_the_wide_badge()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        seedTwoChainRadio(model, 5);

        SliceModel* a = addSliceAt(model, 14200000.0, DSPMode::USB);
        QVERIFY(a);
        pinToAdcs(model, {{a, 1}});

        QCOMPARE(model.sliceChainIndex(a->sliceIndex()), 1);

        // An unbound slice is on no chain at all -- the same "feeds nothing"
        // case panBypassState skips rather than defaulting to chain 0.
        a->setStreamIndex(-1);
        QCOMPARE(model.sliceChainIndex(a->sliceIndex()), -1);
        QVERIFY2(!model.panBypassState({a->sliceIndex()}).bypassed,
                 "an unbound slice's pan claimed a chain state");

        // An id that was never issued resolves to no chain, not chain 0.
        QCOMPARE(model.sliceChainIndex(999), -1);
    }

    // Slice ids are stable and are NOT list positions: addSlice hands out the
    // lowest free id and removeSlice does not renumber survivors
    // (RadioModel.h:359-365). Resolving one by indexing slices() positionally
    // silently reads a different slice after any mid-list removal -- and
    // PanadapterApplet::associatedSlices() is keyed by id, so that is exactly
    // what the chain lookup is handed.
    void chain_resolution_survives_a_mid_list_removal()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        seedTwoChainRadio(model, 5);

        SliceModel* a = addSliceAt(model, 14200000.0, DSPMode::USB);
        SliceModel* b = addSliceAt(model,  7150000.0, DSPMode::LSB);
        SliceModel* c = addSliceAt(model, 21200000.0, DSPMode::CWU);
        QVERIFY(a && b && c);
        const int idC = c->sliceIndex();

        // Drop the middle slice. c keeps id 2 but slides to position 1.
        model.removeSlice(b->sliceIndex());
        QVERIFY(model.sliceById(idC));
        QCOMPARE(model.slices().indexOf(model.sliceById(idC)), 1);
        QCOMPARE(model.sliceById(idC)->sliceIndex(), idC);

        pinToAdcs(model, {{model.sliceById(idC), 1}});
        QCOMPARE(model.sliceChainIndex(idC), 1);

        PanadapterApplet pan(QStringLiteral("pan-0"));
        pan.addSlice(idC);
        refreshOverlays(model, {&pan});

        QCOMPARE(pan.statusFrequencyHz(), Q_INT64_C(21200000));
        QCOMPARE(pan.statusChainIndex(), 1);
    }

    // ── Trigger completeness ──────────────────────────────────────────────

    // The overlay is rebuilt wholesale from the model, so it stays correct
    // only as long as every SliceModel field it paints has a trigger.
    // PanadapterApplet::statusOverlaySliceProperties() is that list, and
    // MainWindow connects the notify signal of each entry to the refresh.
    // A property rename upstream would otherwise silently drop a trigger and
    // strand the overlay again; here it fails the build's test run instead.
    void every_declared_trigger_property_exists_and_notifies()
    {
        SliceModel probe;
        const QMetaObject* mo = probe.metaObject();
        const QByteArrayList names = PanadapterApplet::statusOverlaySliceProperties();
        QVERIFY2(!names.isEmpty(), "no overlay trigger properties declared");

        for (const QByteArray& name : names) {
            const int idx = mo->indexOfProperty(name.constData());
            QVERIFY2(idx >= 0,
                     qPrintable(QStringLiteral("SliceModel has no property '%1'; "
                                               "the overlay trigger for it is dead")
                                    .arg(QString::fromUtf8(name))));
            QVERIFY2(mo->property(idx).hasNotifySignal(),
                     qPrintable(QStringLiteral("SliceModel property '%1' has no NOTIFY; "
                                               "the overlay cannot follow it")
                                    .arg(QString::fromUtf8(name))));
        }
    }

    // Pairs the declared trigger list against the fields updateStatusOverlay
    // actually reads. Named explicitly so adding a field to the setter and
    // forgetting the trigger shows up here as a one-line diff.
    void trigger_list_covers_every_painted_slice_field()
    {
        const QByteArrayList names = PanadapterApplet::statusOverlaySliceProperties();
        for (const char* required : {"frequency", "dspMode", "txSlice",
                                     "diversityEnabled", "psPaused",
                                     "streamIndex"}) {
            QVERIFY2(names.contains(QByteArray(required)),
                     qPrintable(QStringLiteral("overlay paints '%1' but nothing "
                                               "triggers a refresh on it")
                                    .arg(QLatin1String(required))));
        }
    }

    // MainWindow connects each trigger through QMetaMethod, and resolves its
    // own refresh slot by name. A rename that missed the string would leave
    // every connect silently unmade and strand the overlay exactly the way it
    // was stranded before -- so the lookup is pinned here rather than trusted.
    void mainwindow_exposes_the_refresh_slot_the_triggers_resolve()
    {
        QVERIFY2(MainWindow::staticMetaObject
                     .indexOfSlot("refreshPanStatusOverlays()") >= 0,
                 "MainWindow::refreshPanStatusOverlays() is not an invokable "
                 "slot; wireSliceStatusOverlayTriggers resolves it by name and "
                 "would connect nothing");
    }

    // ── Read-back seam ────────────────────────────────────────────────────

    // The wide-badge commit added wideBpf()/wideReason() because a write-only
    // widget cannot be asserted. Same reasoning, same shape, for the fields
    // this change starts driving.
    void overlay_reads_back_every_field_it_paints()
    {
        SpectrumStatusOverlay overlay;
        overlay.setSliceLetter(QChar('C'));
        overlay.setFrequencyHz(Q_INT64_C(28400000));
        overlay.setMode(QStringLiteral("DIGU"));
        overlay.setChainIndex(1);
        overlay.setTxBound(true);
        overlay.setDiversityActive(true);
        overlay.setPsPaused(true);

        QCOMPARE(overlay.sliceLetter(), QChar('C'));
        QCOMPARE(overlay.frequencyHz(), Q_INT64_C(28400000));
        QCOMPARE(overlay.mode(), QStringLiteral("DIGU"));
        QCOMPARE(overlay.chainIndex(), 1);
        QVERIFY(overlay.txBound());
        QVERIFY(overlay.diversityActive());
        QVERIFY(overlay.psPaused());
    }
};

QTEST_MAIN(TestPanStatusOverlay)
#include "tst_pan_status_overlay.moc"
