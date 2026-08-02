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

#include "core/AppSettings.h"
#include "core/RxChannel.h"
#include "core/SampleRateCatalog.h"
#include "core/WdspEngine.h"
#include "core/dsp/Notch.h"
#include "models/NotchModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

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
        QVERIFY(ch->addNotch(0, n));
        QCOMPARE(ch->notchCount(), 1);

        Notch got;
        QVERIFY(ch->notchAt(0, got));
        QCOMPARE(got.centerHz, 14074000.0);
        QCOMPARE(got.widthHz,  250.0);
        QVERIFY(got.active);

        // Past the end: RXANBPGetNotch returns -1 and writes its sentinels
        // (nbp.c:406-411), so the wrapper must report failure, not garbage.
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

        RxChannel* ch = engine->rxChannel(a);
        QVERIFY(ch != nullptr);
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
        QVERIFY(ch->notchAt(0, got));
        QCOMPARE(got.centerHz, 14074000.0);
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
        QCOMPARE(engine->rxChannel(a)->notchCount(), 1);

        for (int ch = 0; ch < WdspEngine::kMaxSliceChannels; ++ch) {
            engine->destroyRxChannel(ch);
        }
        QVERIFY(engine->rxChannel(a) == nullptr);

        model.openRxChannelPool(5, bufferSizeForRate(kRateHz), kRateHz);

        RxChannel* ch = engine->rxChannel(a);
        QVERIFY(ch != nullptr);
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

        NotchModel* nm = model.notchModel();
        nm->setGlobalEnabled(true);
        nm->setAutoIncrease(true);
        QVERIFY(nm->addNotch(14074000.0, 200.0) >= 0);

        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(kSliceBFreqHz);
        QVERIFY(model.sliceById(b)->streamIndex() >= 0);

        RxChannel* ch = engine->rxChannel(b);
        QVERIFY(ch != nullptr);
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

        QVERIFY(model.notchModel()->addNotch(14074000.0, 200.0) >= 0);

        QCOMPARE(engine->rxChannel(a)->notchCount(), 1);
        QCOMPARE(engine->rxChannel(b)->notchCount(), 1);

        Notch got;
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

        const int id = model.notchModel()->addNotch(14074000.0, 200.0);
        QVERIFY(id >= 0);
        QVERIFY(model.notchModel()->setWidth(id, 400.0));

        RxChannel* ch = engine->rxChannel(a);
        QCOMPARE(ch->notchCount(), 1);
        Notch got;
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

        NotchModel* nm = model.notchModel();
        const int first  = nm->addNotch(14074000.0, 200.0);
        const int second = nm->addNotch(14100000.0, 500.0);
        QVERIFY(first >= 0);
        QVERIFY(second >= 0);
        QCOMPARE(engine->rxChannel(a)->notchCount(), 2);

        QVERIFY(nm->removeNotch(first));

        RxChannel* ch = engine->rxChannel(a);
        QCOMPARE(ch->notchCount(), 1);
        Notch got;
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

        NotchModel* nm = model.notchModel();
        QVERIFY(nm->addNotch(14074000.0, 200.0) >= 0);
        QVERIFY(nm->addNotch(14100000.0, 500.0) >= 0);
        QCOMPARE(engine->rxChannel(a)->notchCount(), 2);

        nm->clear();

        QCOMPARE(engine->rxChannel(a)->notchCount(), 0);
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

        model.notchModel()->setAutoIncrease(false);
        QVERIFY(!engine->rxChannel(a)->notchAutoIncrease());
        QVERIFY(!engine->rxChannel(b)->notchAutoIncrease());

        model.notchModel()->setAutoIncrease(true);
        QVERIFY(engine->rxChannel(a)->notchAutoIncrease());
        QVERIFY(engine->rxChannel(b)->notchAutoIncrease());
    }
};

QTEST_MAIN(TestNotchChannelSync)
#include "tst_notch_channel_sync.moc"
