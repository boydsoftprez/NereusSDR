// =================================================================
// tests/tst_pan_badge_click_wiring.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Nothing here
// is a port; the surfaces under test are fixed by
// docs/architecture/2026-05-26-phase3f-sub-epic-e-ui-atlas-plan.md
// Task 3, which is a NereusSDR design document.
//
// Phase 3F: the per-pan status-overlay badges must be clickable on
// EVERY pan, and each click must act on the pan that was clicked.
//
// Two commits made the defect visible. 00ab9522 lit the WIDE badge on
// every pan whose chain is bypassed; 0896b4f3 drove the whole overlay
// from live slice state on every pan. After both, a multi-pan layout
// paints correct badges everywhere and responded to clicks nowhere
// except pan-0, because MainWindow connected the three signals for the
// single applet it could name at startup.
//
// Three defects are pinned here, in the order an operator meets them:
//
//   1. Reachability. Only pan-0 was connected, so a WIDE pill on pan-1
//      was inert. Pans created later by a layout switch were never
//      wired at all, which is every pan except pan-0.
//
//   2. Target. The pan-0 WIDE handler resolved its chain as
//      slices.first()->chainIndex() -- slice 0 rather than the pan's
//      own active slice, AND through SliceModel::chainIndex(), which
//      has no production writer (see PanadapterApplet.h:88-94). Both
//      errors land on the same answer, 0, so the dialog opened on
//      chain 0 no matter which pan or chain was clicked. The overlay
//      beside it resolves the chain through
//      RadioModel::sliceChainIndex, so the dialog could contradict the
//      CH tag the operator clicked.
//
//   3. TX handoff. PanadapterApplet::activeSliceIndex() is a slice ID
//      (0896b4f3 resolves it through RadioModel::sliceById), but
//      TxSliceArbiter::requestHandoff takes a LIST POSITION. Feeding
//      one to the other picks the wrong slice -- or none -- as soon as
//      a mid-list removal makes ids and positions diverge, and the
//      thing it would pick wrong is which slice transmits.
// =================================================================

#include <QtTest/QtTest>

#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/DdcAssignment.h"
#include "core/ReceiverManager.h"
#include "core/TxSliceArbiter.h"
#include "gui/MainWindow.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanadapterStack.h"
#include "gui/widgets/SpectrumStatusOverlay.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// Same helper contract as tst_pan_status_overlay.cpp / tst_pan_wide_badge.cpp:
// ReceiverManager reports the default ADC0 for receivers it was never told
// about, so without an explicit pool every slice lands on chain 0 and the
// per-chain assertions below would pass for the wrong reason.
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

// The overlay is a private child of the applet. Reaching it through findChild
// keeps the click path the production one (applet re-emits what the overlay
// emits) without widening PanadapterApplet's API just for a test.
SpectrumStatusOverlay* overlayOf(PanadapterApplet* pan)
{
    return pan ? pan->findChild<SpectrumStatusOverlay*>() : nullptr;
}

// Click the centre of a named badge. badgeRect() is the same geometry
// mousePressEvent hit-tests, so a layout change moves the test's click with
// the pill rather than leaving it silently clicking empty background and
// reporting a pass.
void clickBadge(SpectrumStatusOverlay* overlay, SpectrumStatusOverlay::Badge badge)
{
    QVERIFY(overlay);
    overlay->resize(600, overlay->height());
    const QRect r = overlay->badgeRect(badge);
    QVERIFY2(r.isValid(), "badge is not currently hit-testable");
    QTest::mousePress(overlay, Qt::LeftButton, Qt::NoModifier, r.center());
    QTest::mouseRelease(overlay, Qt::LeftButton, Qt::NoModifier, r.center());
}

// The production fan-out, exactly as MainWindow::refreshPanStatusOverlays
// drives it. Mirrors tst_pan_status_overlay.cpp so the two stay comparable.
void refreshOverlays(RadioModel& model, const QList<PanadapterApplet*>& pans)
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

class TestPanBadgeClickWiring : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    // ── 1. Reachability: a click on a non-zero pan carries that pan's id ──

    // The WIDE pill on pan-1 must reach a handler, and must say "pan-1".
    // Before this change the applet re-emitted correctly and nothing was
    // listening, so this is the first assertion that the signal is worth
    // anything on a pan other than the first.
    void wide_badge_on_a_non_zero_pan_reports_that_pan()
    {
        PanadapterApplet pan(QStringLiteral("pan-1"));
        pan.setWideBpf(true, QStringLiteral("bypassed"));

        QSignalSpy spy(&pan, &PanadapterApplet::wideBadgeClicked);
        clickBadge(overlayOf(&pan), SpectrumStatusOverlay::Badge::Wide);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("pan-1"));
    }

    // The CH tag is always painted, so it is always clickable. It must name
    // the pan as well as the chain: the handler resolves the chain from the
    // pan (see chain_for_a_pan_* below), and a chain-only signal leaves it
    // unable to tell which pan asked.
    void chain_tag_on_a_non_zero_pan_reports_that_pan_and_its_chain()
    {
        PanadapterApplet pan(QStringLiteral("pan-2"));
        SpectrumStatusOverlay* overlay = overlayOf(&pan);
        QVERIFY(overlay);
        overlay->setChainIndex(1);

        QSignalSpy spy(&pan, &PanadapterApplet::chainTagClicked);
        clickBadge(overlay, SpectrumStatusOverlay::Badge::ChainTag);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("pan-2"));
        QCOMPARE(spy.at(0).at(1).toInt(), 1);
    }

    // The TX pill is hit-testable only while the pan's active slice already
    // holds TX (SpectrumStatusOverlay::mousePressEvent gates the region on
    // m_txBound, which updateStatusOverlay drives from
    // SliceModel::isTxSlice). Pinned so the narrow reachability of this
    // surface is a recorded fact rather than something a later reader has to
    // rediscover from the paint order.
    void tx_badge_is_hit_testable_only_while_the_pan_holds_tx()
    {
        PanadapterApplet pan(QStringLiteral("pan-1"));
        SpectrumStatusOverlay* overlay = overlayOf(&pan);
        QVERIFY(overlay);

        QVERIFY2(!overlay->badgeRect(SpectrumStatusOverlay::Badge::Tx).isValid(),
                 "TX pill claims a hit region while unlit; mousePressEvent "
                 "does not paint or hit-test it in that state");

        overlay->setTxBound(true);
        QVERIFY(overlay->badgeRect(SpectrumStatusOverlay::Badge::Tx).isValid());

        QSignalSpy spy(&pan, &PanadapterApplet::txBadgeClicked);
        clickBadge(overlay, SpectrumStatusOverlay::Badge::Tx);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("pan-1"));
    }

    // ── 2. Target: the handler acts on the clicked pan's chain ────────────

    // The defect, stated as an assertion. Pan-1 hosts a slice on ADC 1; the
    // expression the pan-0 handler used answers 0 for it. Both halves are
    // asserted together so the case cannot silently degrade into checking
    // that 1 == 1.
    void chain_for_a_pan_is_its_own_slices_chain_not_slice_zeros()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        seedTwoChainRadio(model, 5);

        SliceModel* a = addSliceAt(model, 7150000.0, DSPMode::LSB);
        SliceModel* b = addSliceAt(model, 14250000.0, DSPMode::USB);
        QVERIFY(a);
        QVERIFY(b);
        pinToAdcs(model, {{a, 0}, {b, 1}});

        PanadapterApplet panA(QStringLiteral("pan-0"));
        PanadapterApplet panB(QStringLiteral("pan-1"));
        panA.addSlice(a->sliceIndex());
        panB.addSlice(b->sliceIndex());

        // What the pan actually shows, and what the dialog must open on.
        QCOMPARE(model.sliceChainIndex(panB.activeSliceIndex()), 1);
        QCOMPARE(model.sliceChainIndex(panA.activeSliceIndex()), 0);

        // What the shipped pan-0 handler computed for BOTH pans. Two
        // independent errors landing on the same wrong answer: slice 0
        // instead of the pan's slice, through a property with no production
        // writer instead of RadioModel::sliceChainIndex.
        QCOMPARE(model.slices().first()->chainIndex(), 0);
    }

    // The CH tag the operator clicked and the chain the dialog opens on have
    // to be one resolution. updateStatusOverlay pushes
    // RadioModel::sliceChainIndex into the tag, so the handler reading the
    // same call is reading what is painted.
    void chain_tag_shown_matches_the_chain_the_handler_resolves()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        seedTwoChainRadio(model, 5);

        SliceModel* a = addSliceAt(model, 7150000.0, DSPMode::LSB);
        SliceModel* b = addSliceAt(model, 14250000.0, DSPMode::USB);
        QVERIFY(a);
        QVERIFY(b);
        pinToAdcs(model, {{a, 0}, {b, 1}});

        PanadapterApplet panB(QStringLiteral("pan-1"));
        panB.addSlice(b->sliceIndex());
        refreshOverlays(model, {&panB});

        QCOMPARE(panB.statusChainIndex(), 1);
        QCOMPARE(panB.statusChainIndex(),
                 model.sliceChainIndex(panB.activeSliceIndex()));
    }

    // ── 3. TX handoff: id in, position out ───────────────────────────────

    // The pan carries a slice ID; the arbiter indexes positionally. After a
    // mid-list removal the two diverge, and feeding the id straight through
    // either misses or picks the wrong transmitter. RadioModel::
    // requestTxHandoffToSlice owns the conversion so the pan surface does
    // not have to know the difference.
    void tx_handoff_from_a_pan_resolves_the_slice_by_id_not_position()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        seedTwoChainRadio(model, 5);

        SliceModel* a = addSliceAt(model, 7150000.0, DSPMode::LSB);
        SliceModel* b = addSliceAt(model, 14250000.0, DSPMode::USB);
        SliceModel* c = addSliceAt(model, 21300000.0, DSPMode::USB);
        QVERIFY(a);
        QVERIFY(b);
        QVERIFY(c);
        const int idC = c->sliceIndex();
        QCOMPARE(idC, 2);

        // Remove the middle slice. Survivors keep their ids (removeSlice does
        // not renumber), so C is id 2 at position 1.
        model.removeSlice(b->sliceIndex());
        QCOMPARE(model.slices().size(), 2);
        QCOMPARE(model.slices().indexOf(model.sliceById(idC)), 1);

        PanadapterApplet pan(QStringLiteral("pan-1"));
        pan.addSlice(idC);
        QCOMPARE(pan.activeSliceIndex(), idC);

        QVERIFY(model.requestTxHandoffToSlice(pan.activeSliceIndex()));

        QVERIFY2(model.sliceById(idC)->isTxSlice(),
                 "TX did not land on the pan's active slice");
        QVERIFY(!model.sliceById(a->sliceIndex())->isTxSlice());
        QCOMPARE(model.txSliceArbiter()->txBoundSlice(), model.sliceById(idC));

        // The unconverted call the naive wiring would have made: id 2 is out
        // of range as a position in a two-slice list, so the handoff is
        // rejected outright and the transmitter never moves.
        QVERIFY2(!model.txSliceArbiter()->requestHandoff(idC),
                 "id-as-position happened to be in range; the case no longer "
                 "demonstrates why the conversion exists");
    }

    // The conversion must not quietly bind something when handed an id that
    // resolves to no slice: a rejected handoff leaves the transmitter where
    // it was rather than moving it somewhere the operator did not ask for.
    void tx_handoff_for_an_unknown_slice_is_rejected_and_moves_nothing()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        seedTwoChainRadio(model, 5);

        SliceModel* a = addSliceAt(model, 7150000.0, DSPMode::LSB);
        QVERIFY(a);
        SliceModel* before = model.txSliceArbiter()->txBoundSlice();

        QVERIFY(!model.requestTxHandoffToSlice(97));
        QVERIFY(!model.requestTxHandoffToSlice(-1));
        QCOMPARE(model.txSliceArbiter()->txBoundSlice(), before);
    }

    // ── 4. Every pan, including ones created after startup ────────────────

    // MainWindow arms the badge connects from PanadapterStack::countChanged,
    // the same hook 0896b4f3 used for the overlay refresh. countChanged has
    // to actually fire for pans that appear after startup, or the arming
    // never runs for the pans that need it most.
    void a_pan_created_after_startup_raises_the_arming_trigger()
    {
        PanadapterStack stack;
        QSignalSpy spy(&stack, &PanadapterStack::countChanged);

        QVERIFY(stack.addPanadapter(QStringLiteral("pan-1")));
        QVERIFY2(spy.count() >= 1,
                 "addPanadapter did not raise countChanged; MainWindow would "
                 "never arm the badge connects for the new pan");
        QVERIFY(stack.allApplets().contains(
            stack.panadapter(QStringLiteral("pan-1"))));

        // A layout switch is the other way pans come into existence.
        const int beforeLayout = spy.count();
        stack.applyLayout(QStringLiteral("2x2"),
                          {QStringLiteral("pan-0"), QStringLiteral("pan-1"),
                           QStringLiteral("pan-2"), QStringLiteral("pan-3")});
        QVERIFY(spy.count() > beforeLayout);
        QCOMPARE(stack.allApplets().size(), 4);
        for (const QString& id : {QStringLiteral("pan-2"),
                                  QStringLiteral("pan-3")}) {
            QVERIFY2(stack.panadapter(id) != nullptr,
                     qPrintable(QStringLiteral("layout switch did not create %1")
                                    .arg(id)));
        }
    }

    // The arming entry point and the three handlers are resolved by name
    // here for the same reason tst_pan_status_overlay pins
    // refreshPanStatusOverlays(): MainWindow cannot be stood up in a test, so
    // a rename that stranded every badge would otherwise reach a release
    // silently. This is the seam that says "wired", the cases above say
    // "wired to the right thing".
    void mainwindow_exposes_the_badge_wiring_slots()
    {
        const QMetaObject& mo = MainWindow::staticMetaObject;
        for (const char* sig : {"wirePanBadgeHandlers()",
                                "onPanWideBadgeClicked(QString)",
                                "onPanChainTagClicked(QString,int)",
                                "onPanTxBadgeClicked(QString)"}) {
            QVERIFY2(mo.indexOfSlot(sig) >= 0,
                     qPrintable(QStringLiteral("MainWindow::%1 is not an "
                                               "invokable slot; the badge "
                                               "connects would be unmade or "
                                               "would silently duplicate")
                                    .arg(QLatin1String(sig))));
        }
    }

    // ── 5. Pan-0 keeps working ───────────────────────────────────────────

    // The pan that worked before this change must still work, and must still
    // answer "pan-0" rather than inheriting a neighbour's id from the shared
    // wiring pass.
    void pan_zero_still_reports_its_own_id_and_chain()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        seedTwoChainRadio(model, 5);

        SliceModel* a = addSliceAt(model, 7150000.0, DSPMode::LSB);
        SliceModel* b = addSliceAt(model, 14250000.0, DSPMode::USB);
        QVERIFY(a);
        QVERIFY(b);
        pinToAdcs(model, {{a, 0}, {b, 1}});

        PanadapterApplet panA(QStringLiteral("pan-0"));
        PanadapterApplet panB(QStringLiteral("pan-1"));
        panA.addSlice(a->sliceIndex());
        panB.addSlice(b->sliceIndex());
        panA.setWideBpf(true, QStringLiteral("bypassed"));
        refreshOverlays(model, {&panA, &panB});

        QSignalSpy spy(&panA, &PanadapterApplet::wideBadgeClicked);
        clickBadge(overlayOf(&panA), SpectrumStatusOverlay::Badge::Wide);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("pan-0"));
        QCOMPARE(model.sliceChainIndex(panA.activeSliceIndex()), 0);
        QCOMPARE(panA.statusChainIndex(), 0);
    }
};

QTEST_MAIN(TestPanBadgeClickWiring)
#include "tst_pan_badge_click_wiring.moc"
