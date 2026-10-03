// =================================================================
// tests/tst_notch_channel_sync.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Thetis and WDSP
// file names appear in comments to document what each push forwards to; no
// upstream logic is ported into this file.
//
// Tunable Notch Filter, Task 4: the RadioModel notch fan-out and
// syncNotchesToAllChannels().
//
// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md
//   section 6.3  fan-out and the openRxChannelPool-tail reconcile
//   section 5.5  restore order (model populated before any channel exists)
//   section 8.1  RadioModel::notchModel() accessor
//   section 11   tst_notch_channel_sync
// R-R3-21 / R-R3-09: a remote window's notch.* commands, dispatched on the
// Core, change the Core's list and every bound receiver's notches.
//
// Uses the WdspEngine NEREUS_BUILD_TESTS friend seam exactly as
// tests/tst_stream_pool_binding.cpp does: priming m_initialized lets
// openRxChannelPool run createRxChannel's real OpenChannel, so every
// RXANBP* wrapper here talks to a genuinely opened WDSP channel. Design
// section 11.1: an unopened channel is not merely inert, it is an
// out-of-bounds read on rxa[] (third_party/wdsp/src/comm.h sizes rxa at
// MAX_CHANNELS = 32, and every RXANBP* entry point dereferences
// rxa[channel] before it range-checks anything).
// =================================================================
#include <QtTest/QtTest>

#include "core/DspControlThread.h"
#include "core/AppSettings.h"
#include "core/RxChannel.h"
#include "core/SampleRateCatalog.h"
#include "core/WdspEngine.h"
#include "core/dsp/Notch.h"
#include "core/session/SessionCommandDispatcher.h"
#include "core/session/SessionMessages.h"
#include "models/NotchModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {
// R-R3-39: a RadioModel runs its receive WDSP calls on the receive lane;
// wait for the lane before reading WDSP back (no events are processed, so
// a coalescing timer is not fired by the wait).
bool laneIdle(RadioModel& model)
{
    return model.receiveLane() == nullptr || model.receiveLane()->waitIdleForTest(600000);
}
} // namespace

namespace {

// Stream geometry shared by every slot, matching
// tests/tst_stream_pool_binding.cpp. 192 kHz gives a +/- 96 kHz window, so
// the 14.0745 / 14.076 MHz slice pair below shares one stream.
constexpr int    kRateHz       = 192000;
constexpr double kSliceAFreqHz = 14074500.0;
constexpr double kSliceBFreqHz = 14076000.0;

} // namespace

class TestNotchChannelSync : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void init()         { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    // -- section 8.1: the accessor, alongside spotModel() -----------------
    void radio_model_owns_one_notch_model()
    {
        RadioModel model;
        QVERIFY(model.notchModel() != nullptr);
        // A non-owning view onto a unique_ptr member, not a factory.
        QCOMPARE(model.notchModel(), model.notchModel());
    }

    // -- section 5.5 / 8.1: restoreFromSettings() runs in the ctor, before
    // any channel exists, so the openRxChannelPool-tail reconcile always has
    // the full list to install.
    void notch_model_is_restored_at_construction()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("NotchGlobalEnabled"), QStringLiteral("True"));
        s.setValue(QStringLiteral("NotchCount"),         1);
        s.setValue(QStringLiteral("Notch0Center"),       14074000.0);
        s.setValue(QStringLiteral("Notch0Width"),        250.0);
        s.setValue(QStringLiteral("Notch0Active"),       QStringLiteral("True"));

        RadioModel model;
        const NotchModel* nm = model.notchModel();
        QVERIFY(nm != nullptr);
        QCOMPARE(nm->notches().size(), 1);
        QCOMPARE(nm->notches().at(0).centerHz, 14074000.0);
        QCOMPARE(nm->notches().at(0).widthHz,  250.0);
        QVERIFY(nm->notches().at(0).active);
        QVERIFY(nm->globalEnabled());
    }

    // -- section 6.2 + 11: the readbacks the fan-out slots assert through --
    //
    // notchesRun() / notchAutoIncrease() are C++ carries (the section 4.6
    // pattern: written outside #ifdef HAVE_WDSP), because WDSP exposes no
    // getter for NOTCHDB::master_run or NBP::autoincr. notchAt() is the real
    // thing: RXANBPGetNotch reads WDSP's own per-channel database back
    // (third_party/wdsp/src/nbp.c:393), so it proves a push landed rather
    // than echoing a carry.
    void rx_channel_reports_back_the_notch_state_it_was_handed()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        model.configureStreamPool(5, 5, kRateHz);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(kSliceAFreqHz);
        model.openRxChannelPool(5, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));

        RxChannel* ch = engine->rxChannel(a);
        QVERIFY(ch != nullptr);

        ch->setNotchesRun(true);
        QVERIFY(ch->notchesRun());
        ch->setNotchesRun(false);
        QVERIFY(!ch->notchesRun());

        ch->setNotchAutoIncrease(true);
        QVERIFY(ch->notchAutoIncrease());
        ch->setNotchAutoIncrease(false);
        QVERIFY(!ch->notchAutoIncrease());

        Notch n;
        n.centerHz = 14074000.0;
        n.widthHz  = 250.0;
        n.active   = true;
        // addNotch() calls WDSP on the caller's thread (the product only
        // calls it from a receive-lane job, addNotchReconciled). The
        // setters above are still queued on the lane: calling it now ran
        // RXANBPAddNotch on this thread while RXANBPSetNotchesRun ran on
        // the lane, both rebuilding and freeing nbp0's impulse (a double
        // free under ThreadSanitizer, R-R3-49). Let the lane finish first.
        QVERIFY(laneIdle(model));
        QVERIFY(ch->addNotch(0, n));
        QVERIFY(laneIdle(model));
        QCOMPARE(ch->notchCount(), 1);

        Notch got;
        QVERIFY(laneIdle(model));
        QVERIFY(ch->notchAt(0, got));
        QCOMPARE(got.centerHz, 14074000.0);
        QCOMPARE(got.widthHz,  250.0);
        QVERIFY(got.active);

        // Past the end: RXANBPGetNotch returns -1 and writes its sentinels
        // (nbp.c:406-411), so the wrapper must report failure, not garbage.
        QVERIFY(laneIdle(model));
        QVERIFY(!ch->notchAt(1, got));
    }

    // -- section 6.3: the whole point of the task -------------------------
    //
    // connectToRadio's WDSP-init lambda activates channel 0 BEFORE it opens
    // the pool, and activateSliceChannel early-returns on an already-active
    // channel. Slice A therefore never passes through that hook, so the
    // reconcile has to live at the openRxChannelPool tail.
    void slice_a_gets_notches_run_autoincrease_and_tunefreq_on_connect()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        NotchModel* nm = model.notchModel();
        nm->setGlobalEnabled(true);
        nm->setAutoIncrease(true);
        QVERIFY(nm->addNotch(14074000.0, 200.0) >= 0);
        QVERIFY(nm->addNotch(14100000.0, 500.0) >= 0);

        model.configureStreamPool(5, 5, kRateHz);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(kSliceAFreqHz);
        QVERIFY(model.sliceById(a)->streamIndex() >= 0);

        // Everything above happened with zero WDSP channels open, so nothing
        // pushed anything. This call is the only writer.
        model.openRxChannelPool(5, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));

        RxChannel* ch = engine->rxChannel(a);
        QVERIFY(ch != nullptr);
        QVERIFY(laneIdle(model));
        QCOMPARE(ch->notchCount(), 2);
        QVERIFY(ch->notchesRun());
        QVERIFY(ch->notchAutoIncrease());

        // Section 4.1: tunefreq is the hosting stream's centre, not the slice
        // frequency. WDSP sums it with the shift (offset = tunefreq + shift,
        // nbp.c:192), so both halves are asserted, not just the sum.
        const int st = model.sliceById(a)->streamIndex();
        QCOMPARE(ch->notchTuneFrequencyHz(), model.streamCentreHzForTest(st));
        QCOMPARE(ch->notchTuneFrequencyHz() + model.sliceById(a)->shiftOffsetHz(),
                 model.sliceById(a)->frequency());

        // List order is the WDSP index (section 5.2).
        Notch got;
        QVERIFY(laneIdle(model));
        QVERIFY(ch->notchAt(0, got));
        QCOMPARE(got.centerHz, 14074000.0);
        QVERIFY(laneIdle(model));
        QVERIFY(ch->notchAt(1, got));
        QCOMPARE(got.centerHz, 14100000.0);
    }

    // -- section 6.3: reconnect reopens the hole ---------------------------
    //
    // teardownConnection calls WdspEngine::shutdown, which destroys every RX
    // channel; connectToRadio then re-opens the pool. Simulated here by
    // destroying the pool directly, which is precisely the half of shutdown()
    // that matters.
    void notches_come_back_on_slice_a_after_a_reconnect()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        NotchModel* nm = model.notchModel();
        nm->setGlobalEnabled(true);
        QVERIFY(nm->addNotch(7040000.0, 200.0) >= 0);

        model.configureStreamPool(5, 5, kRateHz);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(7040500.0);
        model.openRxChannelPool(5, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));
        QCOMPARE(engine->rxChannel(a)->notchCount(), 1);

        for (int ch = 0; ch < WdspEngine::kMaxSliceChannels; ++ch) {
            engine->destroyRxChannel(ch);
        }
        QVERIFY(engine->rxChannel(a) == nullptr);

        model.openRxChannelPool(5, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));

        RxChannel* ch = engine->rxChannel(a);
        QVERIFY(ch != nullptr);
        QVERIFY(laneIdle(model));
        QCOMPARE(ch->notchCount(), 1);
        QVERIFY(ch->notchesRun());
        QCOMPARE(ch->notchTuneFrequencyHz(),
                 model.streamCentreHzForTest(model.sliceById(a)->streamIndex()));
    }

    // -- section 6.3: "keep the activateSliceChannel hook for the
    // later-added-slice case". The discriminating sequence is a notch added
    // AFTER the pool reconcile: the live fan-out walks slices(), and slice B
    // does not exist yet, so channel 1 is left open, bound to nothing and
    // empty. Binding B is the only remaining chance to seed it.
    void a_slice_added_after_a_live_notch_add_inherits_the_set()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, kRateHz);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(kSliceAFreqHz);
        model.openRxChannelPool(5, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));

        NotchModel* nm = model.notchModel();
        nm->setGlobalEnabled(true);
        nm->setAutoIncrease(true);
        QVERIFY(nm->addNotch(14074000.0, 200.0) >= 0);

        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(kSliceBFreqHz);
        QVERIFY(model.sliceById(b)->streamIndex() >= 0);

        RxChannel* ch = engine->rxChannel(b);
        QVERIFY(ch != nullptr);
        QVERIFY(laneIdle(model));
        QCOMPARE(ch->notchCount(), 1);
        QVERIFY(ch->notchesRun());
        QVERIFY(ch->notchAutoIncrease());
        QCOMPARE(ch->notchTuneFrequencyHz(),
                 model.streamCentreHzForTest(model.sliceById(b)->streamIndex()));
    }

    // -- section 6.3 live fan-out: add ------------------------------------
    void a_live_add_reaches_every_bound_channel()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, kRateHz);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(kSliceAFreqHz);
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(kSliceBFreqHz);
        model.openRxChannelPool(5, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));

        QVERIFY(model.notchModel()->addNotch(14074000.0, 200.0) >= 0);

        QVERIFY(laneIdle(model));
        QCOMPARE(engine->rxChannel(a)->notchCount(), 1);
        QVERIFY(laneIdle(model));
        QCOMPARE(engine->rxChannel(b)->notchCount(), 1);

        Notch got;
        QVERIFY(laneIdle(model));
        QVERIFY(engine->rxChannel(b)->notchAt(0, got));
        QCOMPARE(got.centerHz, 14074000.0);
        QCOMPARE(got.widthHz,  200.0);
        QVERIFY(got.active);
    }

    // -- section 6.2: an edit is incremental (one UpdateNBPFilters), not a
    // resync.
    void a_live_width_edit_reaches_the_channel()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, kRateHz);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(kSliceAFreqHz);
        model.openRxChannelPool(5, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));

        const int id = model.notchModel()->addNotch(14074000.0, 200.0);
        QVERIFY(id >= 0);
        QVERIFY(model.notchModel()->setWidth(id, 400.0));

        RxChannel* ch = engine->rxChannel(a);
        QVERIFY(laneIdle(model));
        QCOMPARE(ch->notchCount(), 1);
        Notch got;
        QVERIFY(laneIdle(model));
        QVERIFY(ch->notchAt(0, got));
        QCOMPARE(got.widthHz,  400.0);
        QCOMPARE(got.centerHz, 14074000.0);
    }

    // -- section 5.2 + 6.3: delete uses the FORMER index, and WDSP shifts its
    // own array down internally (nbp.c:418-441), so positions stay aligned.
    void a_live_remove_reaches_the_channel_and_keeps_the_order()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, kRateHz);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(kSliceAFreqHz);
        model.openRxChannelPool(5, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));

        NotchModel* nm = model.notchModel();
        const int first  = nm->addNotch(14074000.0, 200.0);
        const int second = nm->addNotch(14100000.0, 500.0);
        QVERIFY(first >= 0);
        QVERIFY(second >= 0);
        QVERIFY(laneIdle(model));
        QCOMPARE(engine->rxChannel(a)->notchCount(), 2);

        QVERIFY(nm->removeNotch(first));

        RxChannel* ch = engine->rxChannel(a);
        QVERIFY(laneIdle(model));
        QCOMPARE(ch->notchCount(), 1);
        Notch got;
        QVERIFY(laneIdle(model));
        QVERIFY(ch->notchAt(0, got));
        QCOMPARE(got.centerHz, 14100000.0);
        QCOMPARE(got.widthHz,  500.0);
    }

    // -- section 5.3 clear() contract: a clear that emitted nothing would
    // leave the channels notched while the model showed none.
    void clearing_the_model_empties_every_bound_channel()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, kRateHz);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(kSliceAFreqHz);
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(kSliceBFreqHz);
        model.openRxChannelPool(5, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));

        NotchModel* nm = model.notchModel();
        QVERIFY(nm->addNotch(14074000.0, 200.0) >= 0);
        QVERIFY(nm->addNotch(14100000.0, 500.0) >= 0);
        QVERIFY(laneIdle(model));
        QCOMPARE(engine->rxChannel(a)->notchCount(), 2);

        nm->clear();

        QVERIFY(laneIdle(model));
        QCOMPARE(engine->rxChannel(a)->notchCount(), 0);
        QVERIFY(laneIdle(model));
        QCOMPARE(engine->rxChannel(b)->notchCount(), 0);
    }

    // -- section 6.3: master TNF toggle reaches every channel --------------
    void master_enable_flips_the_run_flag_on_every_bound_channel()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, kRateHz);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(kSliceAFreqHz);
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(kSliceBFreqHz);
        model.openRxChannelPool(5, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));

        // Both directions are load-bearing here: the model ships OFF
        // (maintainer decision D-a), so the pool reconcile pushed false and
        // each flip below is a real change the fan-out has to carry.
        model.notchModel()->setGlobalEnabled(true);
        QVERIFY(engine->rxChannel(a)->notchesRun());
        QVERIFY(engine->rxChannel(b)->notchesRun());

        model.notchModel()->setGlobalEnabled(false);
        QVERIFY(!engine->rxChannel(a)->notchesRun());
        QVERIFY(!engine->rxChannel(b)->notchesRun());
    }

    // -- section 6.3: auto-increase is the one that goes missing -----------
    //
    // OFF first, deliberately. NotchModel ships autoIncrease true (WDSP
    // creates nbp0 with autoincr = 1, RXA.c:105) and RxChannel carries the
    // same default, so a setAutoIncrease(true) opener would assert a value
    // that was already there and pass with the fan-out unwired.
    void auto_increase_flips_on_every_bound_channel()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, kRateHz);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(kSliceAFreqHz);
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(kSliceBFreqHz);
        model.openRxChannelPool(5, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));

        model.notchModel()->setAutoIncrease(false);
        QVERIFY(!engine->rxChannel(a)->notchAutoIncrease());
        QVERIFY(!engine->rxChannel(b)->notchAutoIncrease());

        model.notchModel()->setAutoIncrease(true);
        QVERIFY(engine->rxChannel(a)->notchAutoIncrease());
        QVERIFY(engine->rxChannel(b)->notchAutoIncrease());
    }

    // 2026-08-02 bench regression (JJ, ANAN-G2E): "pan 1 works, pan 2 needs a
    // tune joggle".
    //
    // openRxChannelPool opens every pool channel, but at connect only slice 0
    // exists. syncNotchesToChannel installed the notch list into ALL of them
    // while only channel 0 had a resolvable RF origin, so channels 1..N held
    // notches anchored to tunefreq 0. Opening a second pan later bound its
    // slice onto one of those pre-poisoned channels: the marker drew over the
    // carrier, WDSP genuinely held the notch, master_run and fnfrun both read
    // 1, and the audio was untouched until a retune reached
    // bindSliceToStream -> pushNotchOrigin.
    //
    // Notch data and notch origin are all-or-nothing. An unbound pool channel
    // carries no notches at all.
    void unbound_pool_channels_hold_no_notches()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        NotchModel* nm = model.notchModel();
        QVERIFY(nm != nullptr);
        nm->setGlobalEnabled(true);
        QVERIFY(nm->addNotch(7245000.0) >= 0);

        // Deliberately more channels than slices, which is the real connect
        // shape: the pool opens maxSlices channels and only slice 0 exists.
        model.configureStreamPool(/*userDdcCount*/ 2, /*maxSlices*/ 4, kRateHz);
        model.openRxChannelPool(4, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));

        const int aId = model.addSlice();
        SliceModel* a = model.sliceById(aId);
        QVERIFY(a != nullptr);
        a->setFrequency(kSliceAFreqHz);

        RxChannel* bound = engine->rxChannel(aId);
        QVERIFY(bound != nullptr);
        QVERIFY(laneIdle(model));
        QCOMPARE(bound->notchCount(), 1);          // the owner gets it

        for (int ch = 0; ch < 4; ++ch) {
            if (ch == aId) { continue; }
            if (RxChannel* idle = engine->rxChannel(ch)) {
                QVERIFY(laneIdle(model));
                QVERIFY2(idle->notchCount() == 0,
                         qPrintable(QStringLiteral(
                             "unbound pool channel %1 was handed %2 notch(es) "
                             "with no RF origin to map them from")
                             .arg(ch).arg(idle->notchCount())));
            }
        }

        // And a slice claiming one of those channels later gets BOTH the
        // notches and a resolved origin, with no retune needed.
        const int bId = model.addSlice();
        SliceModel* b = model.sliceById(bId);
        QVERIFY(b != nullptr);
        b->setFrequency(kSliceAFreqHz + 15000.0);

        RxChannel* claimed = engine->rxChannel(bId);
        QVERIFY(claimed != nullptr);
        QVERIFY(laneIdle(model));
        QCOMPARE(claimed->notchCount(), 1);
        QVERIFY2(claimed->notchTuneFrequencyHz() > 0.0,
                 "a slice claiming a pool channel must get a resolved notch "
                 "origin without needing a retune first");
    }

    // 2026-08-02 bench (JJ): a notch drag logged ~50 mutations per second per
    // channel, each a full UpdateNBPFilters (an FFT per partition at nc=4096
    // plus a bpsnba recalculation) swapped under the DSP lock from the GUI
    // thread. Thetis pushes per mouse-move (console.cs:49967 [v2.10.3.15])
    // but for one notch on one channel; multi-pan multiplies it.
    //
    // A burst of edits inside one coalescing window must reach WDSP once, not
    // once per edit, and the final value must still be exact.
    void a_burst_of_notch_edits_coalesces_into_one_push()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        NotchModel* nm = model.notchModel();
        QVERIFY(nm != nullptr);
        nm->setGlobalEnabled(true);

        model.configureStreamPool(/*userDdcCount*/ 2, /*maxSlices*/ 2, kRateHz);
        model.openRxChannelPool(2, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));
        const int aId = model.addSlice();
        SliceModel* a = model.sliceById(aId);
        QVERIFY(a != nullptr);
        a->setFrequency(kSliceAFreqHz);

        const int id = nm->addNotch(kSliceAFreqHz + 1000.0);
        QVERIFY(id >= 0);
        RxChannel* ch = engine->rxChannel(aId);
        QVERIFY(ch != nullptr);

        // Simulate a drag: 20 width edits with no event-loop turn between
        // them, which is how mouse-moves arrive relative to the 50 ms window.
        for (int i = 0; i < 20; ++i) {
            nm->setWidth(id, 200.0 + i);
        }

        // The first edit of the gesture lands immediately (throttle, not
        // debounce), the other 19 are still pending.
        Notch back;
        QVERIFY(laneIdle(model));
        QVERIFY(ch->notchAt(0, back));
        QVERIFY2(back.widthHz < 219.0,
                 "every edit reached WDSP synchronously; the drag was not "
                 "coalesced at all");

        // Ending the drag must commit the exact final value.
        model.commitPendingNotchEdits();
        QVERIFY(laneIdle(model));
        QVERIFY(ch->notchAt(0, back));
        QCOMPARE(back.widthHz, 219.0);
    }

    // Codex review of PR #313, P2 x2. Every notch creation route must clamp to
    // the minimum width THAT SLICE's filter can realise, and must resolve it
    // from the slice acted on rather than from activeSlice().
    //
    // min_notch_width is 1600 / (nc / 256) * (rate / 48000) (nbp.c:88), so a
    // channel at nc 1024 cannot realise anything below 400 Hz. WDSP's
    // auto-increase is on by default (RXA.c:105) and widens silently, so an
    // unclamped add stores and draws a width the DSP is not applying.
    void add_clamps_to_the_target_slices_minimum_not_the_active_slices()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        NotchModel* nm = model.notchModel();
        QVERIFY(nm != nullptr);
        nm->setGlobalEnabled(true);

        model.configureStreamPool(/*userDdcCount*/ 2, /*maxSlices*/ 2, kRateHz);
        model.openRxChannelPool(2, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));

        const int aId = model.addSlice();
        SliceModel* a = model.sliceById(aId);
        QVERIFY(a != nullptr);
        a->setFrequency(kSliceAFreqHz);

        const int bId = model.addSlice();
        SliceModel* b = model.sliceById(bId);
        QVERIFY(b != nullptr);
        b->setFrequency(kSliceAFreqHz + 40000.0);

        RxChannel* chA = engine->rxChannel(aId);
        RxChannel* chB = engine->rxChannel(bId);
        QVERIFY(chA != nullptr);
        QVERIFY(chB != nullptr);

        // Give the two slices different filter sizes, so clamping against the
        // wrong one is observable. nc 1024 -> 400 Hz minimum at 48 kHz.
        chB->setFilterSizeSamples(1024);
        QVERIFY(laneIdle(model));
        const double minB = chB->minNotchWidthHz();
        QVERIFY2(minB > NotchModel::kDefaultNotchWidthHz,
                 qPrintable(QStringLiteral("fixture needs slice B's minimum "
                                           "above the default; got %1").arg(minB)));

        // Add on B while A is whatever activeSlice() happens to be. The stored
        // width must follow B, not A.
        const int id = model.addNotchForSlice(b, b->frequency() + 1000.0,
                                              NotchModel::kDefaultNotchWidthHz);
        QVERIFY(id >= 0);
        const Notch* n = nm->notchById(id);
        QVERIFY(n != nullptr);
        QCOMPARE(n->widthHz, minB);
    }

    // -- R-R3-21 / R-R3-09: the Core's notch commands ----------------------
    //
    // A remote window changes the Core's list one notch at a time through
    // notch.add / notch.move / notch.setActive / notch.delete. Each goes
    // through the Core's NotchModel, so every bound receiver follows, and
    // the add is clamped to the named receiver's real minimum width.
    void remote_notch_commands_reach_every_bound_channel()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        model.configureStreamPool(5, 5, kRateHz);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(kSliceAFreqHz);
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(kSliceBFreqHz);
        model.openRxChannelPool(5, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));
        RxChannel* chA = engine->rxChannel(a);
        RxChannel* chB = engine->rxChannel(b);
        QVERIFY(chA && chB);
        chB->setFilterSizeSamples(1024);
        QVERIFY(laneIdle(model));
        const double minB = chB->minNotchWidthHz();
        QVERIFY(minB > 250.0);

        SessionCommandDispatcher dispatcher(&model);
        QList<SessionMessage> results;
        connect(&dispatcher, &SessionCommandDispatcher::commandResultReady, this,
                [&results](const SessionMessage& m) { results.append(m); });
        quint32 nextId = 1;
        const auto invoke = [&](const QByteArray& verb, const QList<MirrorUpdate>& args) {
            dispatcher.dispatch(SessionMessages::commandInvoke(verb, nextId++, args));
            return results.isEmpty() ? SessionMessage{} : results.last();
        };
        const auto i64 = [](const QByteArray& name, qlonglong v) {
            return MirrorUpdate{0, name, MirrorWireKind::Int64, v};
        };
        const auto f64 = [](const QByteArray& name, double v) {
            return MirrorUpdate{0, name, MirrorWireKind::Float64, v};
        };
        const auto value = [](const SessionMessage& m, const QByteArray& name) {
            for (const MirrorUpdate& u : m.updates) {
                if (u.name == name) { return u.value; }
            }
            return QVariant();
        };

        SessionMessage r = invoke("notch.add",
            {i64("sliceId", b), f64("centreHz", 14076500.0), f64("widthHz", 250.0)});
        QVERIFY2(r.accepted, qPrintable(r.reason));
        const int id = value(r, "id").toInt();
        QVERIFY(id > 0);
        QCOMPARE(value(r, "revision").toUInt(), model.notchModel()->revision());
        QVERIFY(laneIdle(model));
        QCOMPARE(chA->notchCount(), 1);
        QVERIFY(laneIdle(model));
        QCOMPARE(chB->notchCount(), 1);
        QCOMPARE(model.notchModel()->notchById(id)->widthHz, minB);

        r = invoke("notch.move",
            {i64("id", id), f64("centreHz", 14076600.0), f64("widthHz", 600.0)});
        QVERIFY2(r.accepted, qPrintable(r.reason));
        r = invoke("notch.setActive",
            {i64("id", id), MirrorUpdate{0, "active", MirrorWireKind::Bool, false}});
        QVERIFY2(r.accepted, qPrintable(r.reason));
        model.commitPendingNotchEdits();
        for (RxChannel* ch : {chA, chB}) {
            Notch got;
            QVERIFY(laneIdle(model));
            QVERIFY(ch->notchAt(0, got));
            QCOMPARE(got.centerHz, 14076600.0);
            QCOMPARE(got.widthHz, 600.0);
            QVERIFY(!got.active);
        }

        // Refusals: a notch no longer on the Core, an unknown receiver, a
        // move off the radio's range, and a wrong shape. None changes
        // anything.
        r = invoke("notch.move", {i64("id", id + 100), f64("centreHz", 1.4e7), f64("widthHz", 200.0)});
        QVERIFY(!r.accepted);
        QCOMPARE(r.reason, QStringLiteral("That notch is no longer on this Core."));
        r = invoke("notch.add", {i64("sliceId", 99), f64("centreHz", 1.4e7), f64("widthHz", 200.0)});
        QVERIFY(!r.accepted);
        QCOMPARE(r.reason, QStringLiteral("That receiver is not on this Core"));
        r = invoke("notch.move", {i64("id", id), f64("centreHz", 90000.0), f64("widthHz", 200.0)});
        QVERIFY(!r.accepted);
        QCOMPARE(r.reason,
                 QStringLiteral("That notch would be outside the radio's tuning range."));
        r = invoke("notch.delete", {i64("id", id), i64("extra", 1)});
        QVERIFY(!r.accepted);
        QCOMPARE(r.reason,
                 QStringLiteral("This notch change is not one this Core understands."));
        QCOMPARE(model.notchModel()->notchById(id)->centerHz, 14076600.0);

        r = invoke("notch.delete", {i64("id", id)});
        QVERIFY2(r.accepted, qPrintable(r.reason));
        QVERIFY(laneIdle(model));
        QCOMPARE(chA->notchCount(), 0);
        QVERIFY(laneIdle(model));
        QCOMPARE(chB->notchCount(), 0);
        QVERIFY(model.notchModel()->notches().isEmpty());
    }
};

QTEST_MAIN(TestNotchChannelSync)
#include "tst_notch_channel_sync.moc"
