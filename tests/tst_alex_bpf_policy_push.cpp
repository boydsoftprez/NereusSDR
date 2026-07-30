// =================================================================
// tests/tst_alex_bpf_policy_push.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Expected wire
// values are cited to Thetis in comments, but nothing here is a port.
//
// Phase 3F: an effective-BPF change must reach the wire on its OWN
// trigger, not on the next unrelated VFO tick.
//
// RadioModel::republishAlexAdcSlices is the only producer of AlexRxBpf,
// and it used to have exactly two call sites: requestDdcAssignment and
// ConnectionState::Connected. Neither of the two state changes that flip
// AlexAdcState::effective went through either of them:
//
//   1. Wideband. SliceModel::widebandExtensionRequestedChanged ->
//      AlexController::setWidebandActive -> recomputeBpf sets
//      effective = WidebandLocked.
//   2. Operator override. FilterPolicyDialog Apply ->
//      AlexController::setBpfMode -> recomputeBpf.
//
// Both repainted the bottom-bar CH label (the only consumer of
// bpfStateChanged was MainWindow) and left the preselector where it was
// until the operator happened to nudge the VFO. The UI asserted a state
// the hardware was not in, for an unbounded time.
//
// Upstream pushes from the setter, with no deferral:
//   From Thetis console.cs:18793-18806 [v2.10.3.15]: AlexHPFBypass,
//   the operator's manual bypass, calls setAlex1HPF(freq) inside its own
//   setter, and setAlexHPF issues NetworkIO.SetAlexHPFBits(0x20) on the
//   spot (console.cs:6838-6855 [v2.10.3.15]).
//   From Thetis console.cs:43552-43566 [v2.10.3.15]: the wideband
//   window drives that same immediate-push setter when it opens, and
//   wbClosing() restores it. Thetis has no deferred-until-next-tick
//   path anywhere in this chain.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/RadioConnection.h"
#include "core/accessories/AlexController.h"
#include "core/codec/AlexFilterMap.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

// Records every per-ADC BPF decision RadioModel pushes at the connection.
// Deliberately does NOT de-duplicate: P2RadioConnection::setAlexRxBpf does
// that (P2RadioConnection.cpp:1096-1099), and these cases need to count the
// raw calls so a redundant republish is visible rather than absorbed.
//
// File scope, not an anonymous namespace: moc cannot generate a
// staticMetaObject for a Q_OBJECT class with internal linkage.
class BpfMockConnection : public RadioConnection {
    Q_OBJECT
public:
    QList<AlexRxBpf> bpfCalls;

    explicit BpfMockConnection(QObject* parent = nullptr)
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
    void setAntennaRouting(AntennaRouting) override {}
    void setAlexRxBpf(AlexRxBpf b) override { bpfCalls.append(b); }
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

namespace {

// Detaches a stack-injected connection however the scope is left: QCOMPARE
// returns from the enclosing slot on failure, which would otherwise skip the
// trailing detach on exactly the run where it matters.
struct DetachConnection {
    RadioModel* model{nullptr};
    ~DetachConnection() { if (model) { model->injectConnectionForTest(nullptr); } }
};

// 0x20 is the bypass encoding on both chains.
// From Thetis ChannelMaster/netInterface.c:604-651 [v2.10.3.15]:
//   prbpfilter->_Bypass  = (bits & 0x20) != 0;
//   prbpfilter2->_Bypass = (bits & 0x20) != 0;
constexpr int kBypassBits = 0x20;

} // namespace

class TestAlexBpfPolicyPush : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    // ── Trigger 1: operator override (FilterPolicyDialog Apply) ──────────

    // The operator picks "Force bypass, always wideband" and hits Apply.
    // The preselector must go wide right then, not when the VFO next moves.
    //
    // Upstream shape: Thetis's AlexHPFBypass setter calls setAlex1HPF(freq)
    // itself (console.cs:18793-18806 [v2.10.3.15]).
    void operator_force_bypass_reaches_the_wire_without_a_retune()
    {
        RadioModel model;
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);
        auto* mock = new BpfMockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);   // 20 m

        // Baseline: the chain is filtered at 20 m.
        QVERIFY(!mock->bpfCalls.isEmpty());
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0,
                 int(codec::alex::computeHpf(14.2)));
        const int before = int(mock->bpfCalls.size());

        // Exactly what FilterPolicyDialog's Apply button does
        // (FilterPolicyDialog.cpp:112-117). No VFO change of any kind.
        model.alexControllerMutable().setBpfMode(
            0, AlexController::BpfMode::ForceBypass);

        QVERIFY2(int(mock->bpfCalls.size()) > before,
                 "operator override repainted the UI but never reached the wire");
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, kBypassBits);
    }

    // ... and "Force filter" must put the band back on the wire the same way.
    void operator_force_band_reaches_the_wire_without_a_retune()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new BpfMockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);

        model.alexControllerMutable().setBpfMode(
            0, AlexController::BpfMode::ForceBypass);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, kBypassBits);
        const int before = int(mock->bpfCalls.size());

        model.alexControllerMutable().setBpfMode(
            0, AlexController::BpfMode::ForceBand);

        QVERIFY2(int(mock->bpfCalls.size()) > before,
                 "force-filter repainted the UI but never reached the wire");
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0,
                 int(codec::alex::computeHpf(14.2)));
    }

    // ── Trigger 2: wideband ──────────────────────────────────────────────

    // Flipping a slice's wideband-extension flag drives
    // AlexController::setWidebandActive (RadioModel.cpp:3799-3806), which
    // recomputes to WidebandLocked. That has to bypass the preselector now,
    // because the wideband stream is already running.
    //
    // Upstream shape: Thetis's wideband window sets AlexHPFBypass = true on
    // open, routing through the same immediate-push setter, and wbClosing()
    // clears it (console.cs:43545-43566 [v2.10.3.15]).
    // Upstream inline attribution preserved verbatim (console.cs:43545):
    //   private bool _wb_caused_alex_hpf_bypass = false; //[2.10.3.7]MW0LGE fixes #529
    void wideband_toggle_reaches_the_wire_without_a_retune()
    {
        RadioModel model;
        // Codex review, PR #293: wideband now honours
        // BoardCapabilities::widebandAdcs, which a boardless model reports as
        // 0. This test is about the push path, not the capability, so declare
        // the wideband-capable board it always implicitly assumed.
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new BpfMockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        SliceModel* slice = model.slices().at(a);
        slice->setFrequency(14200000.0);

        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0,
                 int(codec::alex::computeHpf(14.2)));
        const int before = int(mock->bpfCalls.size());

        slice->setWidebandExtensionRequested(true);

        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::WidebandLocked);
        QVERIFY2(int(mock->bpfCalls.size()) > before,
                 "wideband locked the BPF in the model but never reached the wire");
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, kBypassBits);
    }

    // Releasing wideband restores the filtered selection, again on its own
    // trigger. Mirrors Thetis wbClosing() (console.cs:43546-43550
    // [v2.10.3.15]).
    void wideband_release_restores_the_filtered_bits()
    {
        RadioModel model;
        // Codex review, PR #293: wideband now honours
        // BoardCapabilities::widebandAdcs, which a boardless model reports as
        // 0. This test is about the push path, not the capability, so declare
        // the wideband-capable board it always implicitly assumed.
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new BpfMockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        SliceModel* slice = model.slices().at(a);
        slice->setFrequency(14200000.0);
        slice->setWidebandExtensionRequested(true);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, kBypassBits);
        const int before = int(mock->bpfCalls.size());

        slice->setWidebandExtensionRequested(false);

        QVERIFY2(int(mock->bpfCalls.size()) > before,
                 "wideband release never reached the wire");
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0,
                 int(codec::alex::computeHpf(14.2)));
    }

    // ── The push must not spam the wire ──────────────────────────────────

    // recomputeBpf runs on every slice band change, and republishAlexAdcSlices
    // calls it (via notifySlicesOnAdc) before composing. Driving the republish
    // from bpfStateChanged therefore re-enters: the emit fires from inside the
    // republish, between the ADC0 and ADC1 notifications.
    //
    // The change-only emit in recomputeBpf (AlexController.cpp:152-154) is NOT
    // enough on its own to stop this. reasonText is part of the change test and
    // it carries the band label, so in Auto mode every band crossing re-emits.
    //
    // One band-crossing retune must therefore still cost exactly one push. Two
    // means the re-entrancy guard is gone; the real connection would absorb the
    // duplicate (P2RadioConnection.cpp:1096-1099) but the inner pass composes
    // ADC1 from state the outer pass has not refreshed yet, so the ordering
    // hazard is real even when the byte count is not.
    void band_crossing_retune_pushes_exactly_once()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new BpfMockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        SliceModel* slice = model.slices().at(a);
        slice->setFrequency(14200000.0);          // 20 m
        const int before = int(mock->bpfCalls.size());

        slice->setFrequency(7150000.0);           // 40 m, crosses a band edge

        QCOMPARE(int(mock->bpfCalls.size()) - before, 1);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0,
                 int(codec::alex::computeHpf(7.15)));
    }

    // An in-band retune leaves effective, currentBpfBand and reasonText all
    // untouched, so recomputeBpf must not emit and the new trigger must add
    // nothing: one push from requestDdcAssignment's own call, carrying the
    // value it already had.
    void in_band_retune_produces_no_extra_push()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new BpfMockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        SliceModel* slice = model.slices().at(a);
        slice->setFrequency(14200000.0);
        const int before = int(mock->bpfCalls.size());
        const int bitsBefore = mock->bpfCalls.last().hpfBitsAdc0;

        slice->setFrequency(14250000.0);          // still 20 m

        QCOMPARE(int(mock->bpfCalls.size()) - before, 1);
        QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0, bitsBefore);
    }

    // Re-applying the mode the chain is already in changes nothing, so it must
    // cost nothing. setBpfMode early-returns (AlexController.cpp:80), no emit,
    // no republish.
    void reapplying_the_same_mode_produces_no_push()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new BpfMockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);

        model.alexControllerMutable().setBpfMode(
            0, AlexController::BpfMode::ForceBypass);
        const int before = int(mock->bpfCalls.size());

        model.alexControllerMutable().setBpfMode(
            0, AlexController::BpfMode::ForceBypass);

        QCOMPARE(int(mock->bpfCalls.size()), before);
    }

    // ── Regression: the no-override single-slice case is byte-identical ───

    // With nobody overriding anything, one slice on one chain must still push
    // exactly the frequency-derived bits, across the whole ladder. This is the
    // "do not regress single-slice operation" lock.
    void single_slice_no_override_bits_unchanged()
    {
        struct Row { double hz; double mhz; };
        const QVector<Row> rows {
            {  1900000.0,  1.9 },   // 160 m
            {  3700000.0,  3.7 },   //  80 m
            {  7150000.0,  7.15},   //  40 m
            { 10120000.0, 10.12},   //  30 m
            { 14200000.0, 14.2 },   //  20 m
            { 21200000.0, 21.2 },   //  15 m
            { 28400000.0, 28.4 },   //  10 m
            { 50150000.0, 50.15},   //   6 m
        };

        for (const Row& r : rows) {
            RadioModel model;
            model.configureStreamPool(5, 5, 192000);
            auto* mock = new BpfMockConnection();
            model.injectConnectionForTest(mock);

            const int a = model.addSlice();
            model.slices().at(a)->setFrequency(r.hz);

            QVERIFY2(!mock->bpfCalls.isEmpty(),
                     qPrintable(QStringLiteral("no push at %1 MHz").arg(r.mhz)));
            QCOMPARE(mock->bpfCalls.last().hpfBitsAdc0,
                     int(codec::alex::computeHpf(r.mhz)));
            // Nothing ever asked for a bypass, so none may appear.
            QCOMPARE(model.alexController().adcState(0).effective,
                     AlexController::BpfEffective::Filtered);

            model.injectConnectionForTest(nullptr);
            delete mock;
        }
    }
};

QTEST_MAIN(TestAlexBpfPolicyPush)
#include "tst_alex_bpf_policy_push.moc"
