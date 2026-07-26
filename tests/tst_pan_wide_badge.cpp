// =================================================================
// tests/tst_pan_wide_badge.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Nothing here
// is a port; the operator-facing strings under test are fixed by
// docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md
// §16.4.4, which is a NereusSDR design document.
//
// Phase 3F: the WIDE badge must light on every panadapter fed by a
// bypassed RX preselector chain, and on no others.
//
// SpectrumStatusOverlay::setWideBpf was painted but had zero callers, so
// an operator whose preselector had gone wide (multi-band auto-bypass,
// extended view, or their own Filter Policy override) had no per-pan
// indication of it at all. The bottom-bar CH label was the only surface,
// and on a multi-pan layout it cannot say WHICH pan is exposed.
//
// The routing under test is per chain, not global:
//
//   pan -> its slices -> their stream -> that stream's ADC -> BpfEffective
//
// setAdcForReceiver is called exactly once in production today (receiver 0
// -> ADC 0, RadioModel.cpp), so every stream currently reports ADC0. These
// cases drive setAdcForReceiver directly so the per-chain contract is
// locked now and stays correct when ADC distribution lands.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/ReceiverManager.h"
#include "core/accessories/AlexController.h"
#include "gui/PanadapterApplet.h"
#include "gui/widgets/SpectrumStatusOverlay.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// ReceiverManager only knows about receivers that were explicitly created:
// setAdcForReceiver and receiverConfig both key off that map, and an unknown
// index silently reports the default ADC0 (ReceiverManager.cpp:219-222,
// 315-322). Production creates one receiver per stream at connect
// (RadioModel.cpp:4946 + 5024-5028). These cases have no connection, so they
// have to stand that pool up themselves or every slice lands on chain 0 and
// the per-chain assertions below would pass for the wrong reason.
void seedReceiverPool(RadioModel& model, int streamCount)
{
    ReceiverManager* rm = model.receiverManager();
    QVERIFY(rm);
    for (int st = 0; st < streamCount; ++st) {
        if (rm->receiverConfig(st).receiverIndex < 0) {
            rm->createReceiver();
        }
    }
}

// Add a slice and tune it. Stream binding is the allocator's business and it
// can move slices between streams as the set grows, so nothing here assumes
// which stream a slice ends up on; pinToAdc below reads it once the set is
// final.
SliceModel* addSliceAt(RadioModel& model, double hz)
{
    const int idx = model.addSlice();
    SliceModel* slice = model.slices().value(idx);
    if (slice) { slice->setFrequency(hz); }
    return slice;
}

// Put a slice's stream on a given ADC. Call after every slice exists.
void pinToAdc(RadioModel& model, SliceModel* slice, int adc)
{
    QVERIFY(slice);
    QVERIFY(model.receiverManager());
    QVERIFY2(slice->streamIndex() >= 0,
             "slice never bound a stream; the ADC pin has nothing to attach to");
    model.receiverManager()->setAdcForReceiver(slice->streamIndex(), adc);
}

// Re-run the per-chain BPF analysis after the ADC pins moved.
//
// requestDdcAssignment is the production coalescing point for exactly this:
// it ends in republishAlexAdcSlices, which is what groups slices by adcIndex
// and recomputes AlexController's per-chain state (RadioModel.cpp:3340-3348).
// Slice retunes reach it on their own; a bare setAdcForReceiver does not,
// because today nothing in production moves a stream between ADCs after
// connect.
void recomputeChains(RadioModel& model)
{
    model.requestDdcAssignment();
}

} // namespace

class TestPanWideBadge : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    // ── Layer 1: the routing decision (RadioModel::panBypassState) ────────

    // Two slices on ONE chain in two different filter ranges. The band-pass
    // can only pass one of them, so the Auto policy bypasses to keep both
    // slices hearing (AlexController::recomputeBpf). Every pan showing
    // either slice is now fed by an unfiltered chain and must say so.
    void two_slices_one_chain_different_bands_report_wide()
    {
        RadioModel model;
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);
        seedReceiverPool(model, 5);

        SliceModel* a = addSliceAt(model, 14200000.0);  // 20 m
        SliceModel* b = addSliceAt(model,  7150000.0);  // 40 m
        QVERIFY(a && b);
        pinToAdc(model, a, 0);
        pinToAdc(model, b, 0);
        recomputeChains(model);

        // Precondition: the chain really is bypassed.
        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::Bypass);

        // A pan hosting both slices.
        const RadioModel::PanBypassState both =
            model.panBypassState({a->sliceIndex(), b->sliceIndex()});
        QVERIFY2(both.bypassed, "pan on a bypassed chain did not report WIDE");

        // ... and a pan hosting only one of them is fed by the same chain,
        // so it is exposed in exactly the same way.
        const RadioModel::PanBypassState justA =
            model.panBypassState({a->sliceIndex()});
        QVERIFY2(justA.bypassed,
                 "a pan showing one slice of a bypassed chain is still wide");
    }

    // The badge is per chain. With chain 0 bypassed and chain 1 filtered, a
    // pan showing only chain 1's slice must stay clear. If this fails the
    // wiring is global and the operator cannot tell which receiver is exposed.
    void pan_on_a_filtered_chain_stays_clear_while_another_chain_is_wide()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        seedReceiverPool(model, 5);

        SliceModel* a = addSliceAt(model, 14200000.0);  // 20 m -> chain 0
        SliceModel* b = addSliceAt(model,  7150000.0);  // 40 m -> chain 0
        SliceModel* c = addSliceAt(model, 21200000.0);  // 15 m -> chain 1
        QVERIFY(a && b && c);
        pinToAdc(model, a, 0);
        pinToAdc(model, b, 0);
        pinToAdc(model, c, 1);
        recomputeChains(model);

        // Precondition: the pins actually took. An empty chain also reports
        // Filtered, so without this the case below would pass on a build
        // where every slice silently landed on chain 0.
        ReceiverManager* rm = model.receiverManager();
        QCOMPARE(rm->receiverConfig(a->streamIndex()).adcIndex, 0);
        QCOMPARE(rm->receiverConfig(b->streamIndex()).adcIndex, 0);
        QCOMPARE(rm->receiverConfig(c->streamIndex()).adcIndex, 1);

        // Precondition: the two chains genuinely disagree.
        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::Bypass);
        QCOMPARE(model.alexController().adcState(1).effective,
                 AlexController::BpfEffective::Filtered);

        QVERIFY(model.panBypassState({a->sliceIndex(), b->sliceIndex()}).bypassed);

        const RadioModel::PanBypassState onChain1 =
            model.panBypassState({c->sliceIndex()});
        QVERIFY2(!onChain1.bypassed,
                 "a pan on the filtered chain lit WIDE: the badge is global, "
                 "not per chain");
        QVERIFY(onChain1.reason.isEmpty());
    }

    // Do not regress single-slice operation: one slice, one band, no badge.
    void single_slice_on_one_band_reports_no_wide()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        seedReceiverPool(model, 5);

        SliceModel* a = addSliceAt(model, 14200000.0);
        QVERIFY(a);
        pinToAdc(model, a, 0);
        recomputeChains(model);
        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::Filtered);

        const RadioModel::PanBypassState st =
            model.panBypassState({a->sliceIndex()});
        QVERIFY2(!st.bypassed, "single slice on one band raised a WIDE badge");
        QVERIFY(st.reason.isEmpty());
    }

    // A pan with no slices, and a pan whose slices never bound a stream,
    // both feed off nothing. Neither may claim the RF path is wide.
    void pan_with_no_bound_slices_reports_no_wide()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        seedReceiverPool(model, 5);
        addSliceAt(model, 14200000.0);
        addSliceAt(model,  7150000.0);   // chain 0 is bypassed

        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::Bypass);

        QVERIFY2(!model.panBypassState({}).bypassed,
                 "an empty pan inherited the chain's bypass");
        QVERIFY2(!model.panBypassState({999}).bypassed,
                 "an unknown slice index resolved to a chain");
    }

    // ── Layer 2: the operator-facing reason ───────────────────────────────

    // §16.4.4 fixes one sentence per cause. The multi-range sentence has to
    // name the ranges actually in conflict, otherwise the operator is told
    // their receiver is wide with no way to work out why or what to change.
    void reason_names_the_conflicting_ranges_and_says_what_to_do()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        seedReceiverPool(model, 5);

        SliceModel* a = addSliceAt(model, 14200000.0);  // 20 m
        SliceModel* b = addSliceAt(model,  7150000.0);  // 40 m
        QVERIFY(a && b);
        pinToAdc(model, a, 0);
        pinToAdc(model, b, 0);
        recomputeChains(model);

        const RadioModel::PanBypassState st =
            model.panBypassState({a->sliceIndex(), b->sliceIndex()});
        QVERIFY(st.bypassed);
        QVERIFY2(!st.reason.isEmpty(), "WIDE badge carried no reason at all");

        QVERIFY2(st.reason.contains(bandLabel(Band::Band20m)),
                 qPrintable(QStringLiteral("reason did not name 20m: %1")
                                .arg(st.reason)));
        QVERIFY2(st.reason.contains(bandLabel(Band::Band40m)),
                 qPrintable(QStringLiteral("reason did not name 40m: %1")
                                .arg(st.reason)));

        // Tells the operator what to DO, not only what happened.
        QVERIFY2(st.reason.contains(QStringLiteral("Retune")),
                 qPrintable(QStringLiteral("reason offered no remedy: %1")
                                .arg(st.reason)));

        // Project rule: no source citations in user-visible strings.
        QVERIFY(!st.reason.contains(QStringLiteral(".cs:")));
        QVERIFY(!st.reason.contains(QStringLiteral("Thetis")));
    }

    // Extended view and operator override are DIFFERENT causes of the same
    // RF fact, so the badge is identical and the reason is not. Without this
    // the two are silently conflated and the operator cannot tell a zoom
    // they can undo from a policy they chose.
    void extended_view_and_operator_override_give_different_reasons()
    {
        RadioModel wideband;
        wideband.configureStreamPool(5, 5, 192000);
        seedReceiverPool(wideband, 5);
        SliceModel* w = addSliceAt(wideband, 14200000.0);
        QVERIFY(w);
        w->setWidebandExtensionRequested(true);
        QCOMPARE(wideband.alexController().adcState(0).effective,
                 AlexController::BpfEffective::WidebandLocked);
        const RadioModel::PanBypassState ext =
            wideband.panBypassState({w->sliceIndex()});
        QVERIFY(ext.bypassed);
        QVERIFY2(ext.reason.contains(QStringLiteral("extended view")),
                 qPrintable(QStringLiteral("extended-view reason not used: %1")
                                .arg(ext.reason)));

        RadioModel forced;
        forced.configureStreamPool(5, 5, 192000);
        seedReceiverPool(forced, 5);
        SliceModel* f = addSliceAt(forced, 14200000.0);
        QVERIFY(f);
        forced.alexControllerMutable().setBpfMode(
            0, AlexController::BpfMode::ForceBypass);
        const RadioModel::PanBypassState ovr =
            forced.panBypassState({f->sliceIndex()});
        QVERIFY(ovr.bypassed);
        QVERIFY2(ovr.reason.contains(QStringLiteral("Filter Policy")),
                 qPrintable(QStringLiteral("override reason not used: %1")
                                .arg(ovr.reason)));

        QVERIFY2(ext.reason != ovr.reason,
                 "extended view and operator override share one reason string; "
                 "the causes are conflated");
    }

    // ── Layer 3: the widget actually lights ───────────────────────────────

    // setWideBpf was inspection-only: no getter, so nothing could assert the
    // pill state and the badge shipped unverified. These accessors are the
    // narrow test seam that closes that.
    void overlay_records_the_wide_state_and_reason()
    {
        SpectrumStatusOverlay overlay;
        QVERIFY(!overlay.wideBpf());
        QVERIFY(overlay.wideReason().isEmpty());

        overlay.setWideBpf(true, QStringLiteral("Preselector bypassed."));
        QVERIFY(overlay.wideBpf());
        QCOMPARE(overlay.wideReason(), QStringLiteral("Preselector bypassed."));
        // The reason is the badge's tooltip; it was stored dead before.
        QCOMPARE(overlay.toolTip(), QStringLiteral("Preselector bypassed."));

        overlay.setWideBpf(false, QString());
        QVERIFY(!overlay.wideBpf());
        QVERIFY(overlay.wideReason().isEmpty());
        QVERIFY(overlay.toolTip().isEmpty());
    }

    // The pan container forwards to its own overlay, so MainWindow drives
    // one call per pan and never reaches through into the widget tree.
    void applet_forwards_wide_state_to_its_overlay()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        QVERIFY(!applet.wideBpf());

        applet.setWideBpf(true, QStringLiteral("Preselector bypassed."));
        QVERIFY(applet.wideBpf());
        QCOMPARE(applet.wideReason(), QStringLiteral("Preselector bypassed."));

        applet.setWideBpf(false, QString());
        QVERIFY(!applet.wideBpf());
    }

    // ── End to end: model state -> pan badge ──────────────────────────────

    // The whole chain in one case, driven the way MainWindow drives it:
    // ask the model for each pan's state, push it at that pan.
    void pans_light_per_chain_end_to_end()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        seedReceiverPool(model, 5);

        SliceModel* a = addSliceAt(model, 14200000.0);  // 20 m -> chain 0
        SliceModel* b = addSliceAt(model,  7150000.0);  // 40 m -> chain 0
        SliceModel* c = addSliceAt(model, 21200000.0);  // 15 m -> chain 1
        QVERIFY(a && b && c);
        pinToAdc(model, a, 0);
        pinToAdc(model, b, 0);
        pinToAdc(model, c, 1);
        recomputeChains(model);

        PanadapterApplet panWide(QStringLiteral("pan-0"));
        panWide.addSlice(a->sliceIndex());
        panWide.addSlice(b->sliceIndex());

        PanadapterApplet panClear(QStringLiteral("pan-1"));
        panClear.addSlice(c->sliceIndex());

        for (PanadapterApplet* pan : {&panWide, &panClear}) {
            const RadioModel::PanBypassState st =
                model.panBypassState(pan->associatedSlices());
            pan->setWideBpf(st.bypassed, st.reason);
        }

        QVERIFY2(panWide.wideBpf(), "pan on the bypassed chain stayed dark");
        QVERIFY2(!panClear.wideBpf(),
                 "pan on the filtered chain lit anyway");
        QVERIFY(!panWide.wideReason().isEmpty());
    }
};

QTEST_MAIN(TestPanWideBadge)
#include "tst_pan_wide_badge.moc"
