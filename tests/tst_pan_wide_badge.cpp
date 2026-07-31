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
// Streams reach the second ADC through the codec: a slice on an RX-only
// antenna is routed there by applyDdcAssignment, and RadioModel decodes that
// placement out of the resulting DdcAssignment. These cases publish an
// assignment directly (pinToAdcs below) so they can pin any slice to any
// chain without going through antenna policy, which is a different subject
// and is covered by tst_alex_per_adc_bpf_wire.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/DdcAssignment.h"
#include "core/ReceiverManager.h"
#include "core/accessories/AlexController.h"
#include "gui/PanadapterApplet.h"
#include "gui/widgets/SpectrumStatusOverlay.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

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

    // A Saturn is a Protocol 2 radio, so say so. Codex review round 6,
    // PR #293: wideband gates on the protocol of the radio actually
    // connected, not on the board row's nominal one, because ANVELINAPRO3
    // and REDPITAYA resolve to a Protocol2 row while having real Protocol 1
    // codecs. An undeclared RadioInfo reports Protocol1, which would leave
    // this helper describing a Saturn reached over P1: a radio that does not
    // exist. Declared here rather than in the one test that noticed, because
    // this helper is where "what radio is this" is decided.
    RadioInfo info;
    info.protocol = ProtocolVersion::Protocol2;
    model.setLastRadioInfoForTest(info);

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

// Re-run the per-chain BPF analysis after the ADC pins moved.
//
// requestDdcAssignment is the production coalescing point for exactly this:
// it ends in republishAlexAdcSlices, which groups slices by chain and
// recomputes AlexController's per-chain state. Slice retunes and antenna
// changes reach it on their own; publishDdcAssignment does not, because it is
// the client-side half and deliberately writes no wire frame.
//
// Safe to call after pinToAdcs even with no codec injected: computeDdcAssignment
// then returns std::nullopt, so publishDdcAssignment does not run at all and
// cannot overwrite the pins. republishAlexAdcSlices is called by
// requestDdcAssignment itself, not by the publish, so it still runs.
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
        seedTwoChainRadio(model, 5);

        SliceModel* a = addSliceAt(model, 14200000.0);  // 20 m
        SliceModel* b = addSliceAt(model,  7150000.0);  // 40 m
        QVERIFY(a && b);
        pinToAdcs(model, {{a, 0}, {b, 0}});
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
        seedTwoChainRadio(model, 5);

        SliceModel* a = addSliceAt(model, 14200000.0);  // 20 m -> chain 0
        SliceModel* b = addSliceAt(model,  7150000.0);  // 40 m -> chain 0
        SliceModel* c = addSliceAt(model, 21200000.0);  // 15 m -> chain 1
        QVERIFY(a && b && c);
        pinToAdcs(model, {{a, 0}, {b, 0}, {c, 1}});
        recomputeChains(model);

        // Precondition: the pins actually took, on BOTH destinations
        // publishDdcAssignment writes. An empty chain also reports Filtered,
        // so without this the case below would pass on a build where every
        // slice silently landed on chain 0. chainForStream is the one the
        // badge reads; ReceiverManager's copy is the mirror.
        QCOMPARE(model.chainForStream(a->streamIndex()), 0);
        QCOMPARE(model.chainForStream(b->streamIndex()), 0);
        QCOMPARE(model.chainForStream(c->streamIndex()), 1);
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
        seedTwoChainRadio(model, 5);

        SliceModel* a = addSliceAt(model, 14200000.0);
        QVERIFY(a);
        pinToAdcs(model, {{a, 0}});
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
        seedTwoChainRadio(model, 5);
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
        seedTwoChainRadio(model, 5);

        SliceModel* a = addSliceAt(model, 14200000.0);  // 20 m
        SliceModel* b = addSliceAt(model,  7150000.0);  // 40 m
        QVERIFY(a && b);
        pinToAdcs(model, {{a, 0}, {b, 0}});
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
        seedTwoChainRadio(wideband, 5);
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
        seedTwoChainRadio(forced, 5);
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
        seedTwoChainRadio(model, 5);

        SliceModel* a = addSliceAt(model, 14200000.0);  // 20 m -> chain 0
        SliceModel* b = addSliceAt(model,  7150000.0);  // 40 m -> chain 0
        SliceModel* c = addSliceAt(model, 21200000.0);  // 15 m -> chain 1
        QVERIFY(a && b && c);
        pinToAdcs(model, {{a, 0}, {b, 0}, {c, 1}});
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
