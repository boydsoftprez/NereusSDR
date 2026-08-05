// =================================================================
// tests/tst_rxchannel_notch_wrappers.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Thetis and WDSP
// file names appear in comments to document what each wrapper forwards to;
// no upstream logic is ported into this file.
//
// Tunable Notch Filter, Task 2: the Notch value type plus the RxChannel
// manual-notch wrappers that carry it into the per-channel WDSP notch
// database.
//
// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md
//         section 5.1 (Notch), 6.1 (wdsp_api.h declarations),
//         6.2 (RxChannel wrappers), 11.1 (why these need a real channel).
// =================================================================
#include <QtTest/QtTest>

#include <QList>

#include "core/RxChannel.h"
#include "core/WdspEngine.h"
#include "core/dsp/Notch.h"
#include "core/wdsp_api.h"

using namespace NereusSDR;

namespace {

// A real WDSP channel id, not the usual kTestChannel = 99 sentinel. Every
// RXANBP* entry point dereferences rxa[channel] before it range-checks
// anything (third_party/wdsp/src/nbp.c:367, :398, :423, :448, :469), and rxa
// is sized MAX_CHANNELS = 32 (comm.h:110), so 99 is an out-of-bounds read
// rather than a harmless miss. Design doc section 11.1.
constexpr int kNotchTestChannel = 0;
// Outside WDSP's [0, MAX_CHANNELS) range (comm.h:110), matching the
// kTestChannel = 99 convention the rest of the RxChannel suites use.
constexpr int kOutOfRangeChannel = 99;

// Geometry the fixture opens the channel with. Both values are load-bearing
// for the min-notch-width expectation in the last cycle: WDSP derives the
// filter's coefficient count as max(2048, dsp_size) (RXA.c:96) and reads the
// rate straight off the channel (RXA.c:102).
constexpr int kDspBufferSize   = 4096;
constexpr int kDspSampleRateHz = 48000;

// Closes the opened WDSP channel however the scope is left. QCOMPARE /
// QVERIFY return from the enclosing slot on failure, so a trailing
// destroyRxChannel() would be skipped on exactly the run where a leaked
// rxa[0] slot would poison every later slot.
struct ChannelCloser {
    WdspEngine* engine{nullptr};
    ~ChannelCloser() {
        if (engine) { engine->destroyRxChannel(kNotchTestChannel); }
    }
};

#ifdef HAVE_WDSP
// One notch read straight out of the WDSP database, bypassing the wrapper
// under test. nbp.c:393 returns -1 and writes fcenter = -1.0 past the end.
struct RawNotch {
    double centerHz{0.0};
    double widthHz{0.0};
    int    active{-1};
    int    rval{-1};
};

RawNotch readRawNotch(int channelId, int index)
{
    RawNotch n;
    n.rval = RXANBPGetNotch(channelId, index, &n.centerHz, &n.widthHz, &n.active);
    return n;
}
#endif

} // namespace

class TestRxChannelNotchWrappers : public QObject {
    Q_OBJECT

private:
    // Primes the engine past its async wisdom load (the NEREUS_BUILD_TESTS
    // friend seam on WdspEngine) and opens one real RX channel, so
    // rxa[kNotchTestChannel].ndb exists. Same pattern as
    // tests/tst_ps_feedback_channel.cpp:72,78.
    RxChannel* openNotchChannel(WdspEngine& engine)
    {
        engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        return engine.createRxChannel(kNotchTestChannel,
                                      /*inputBufferSize*/ 238,
                                      /*dspBufferSize*/ kDspBufferSize,
                                      /*inputSampleRate*/ kDspSampleRateHz,
                                      /*dspSampleRate*/ kDspSampleRateHz,
                                      /*outputSampleRate*/ kDspSampleRateHz);
    }

private slots:
    // -- 5.1: the Notch value type ----------------------------------------

    void notch_defaults_to_panadapter_width()
    {
        Notch n;
        QCOMPARE(n.widthHz, 200.0);
    }

    void notch_defaults_to_active()
    {
        Notch n;
        QVERIFY(n.active);
    }

    void notch_defaults_to_unset_id_and_centre()
    {
        Notch n;
        QCOMPARE(n.id, 0);
        QCOMPARE(n.centerHz, 0.0);
    }

    // -- 6.2: notch capacity and count readback ---------------------------

    void max_notches_matches_the_wdsp_database_size()
    {
        // create_notchdb is called with maxnotches = 1024 for every RXA
        // channel (third_party/wdsp/src/RXA.c:88); RXANBPAddNotch refuses
        // past it (nbp.c:368).
        QCOMPARE(RxChannel::kMaxNotches, 1024);
    }

    void fresh_channel_reports_zero_notches()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        QCOMPARE(ch->notchCount(), 0);
#endif
    }

    // -- 6.2: add ---------------------------------------------------------

    void add_notch_appends_in_list_order()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        QVERIFY(ch->addNotch(0, Notch{1, 14074000.0, 200.0, true}));
        QVERIFY(ch->addNotch(1, Notch{2, 14100000.0, 100.0, true}));
        QVERIFY(ch->addNotch(2, Notch{3, 14200000.0, 500.0, false}));

        QCOMPARE(ch->notchCount(), 3);

        const RawNotch first = readRawNotch(kNotchTestChannel, 0);
        QCOMPARE(first.rval, 0);
        QCOMPARE(first.centerHz, 14074000.0);
        QCOMPARE(first.widthHz, 200.0);
        QCOMPARE(first.active, 1);

        const RawNotch third = readRawNotch(kNotchTestChannel, 2);
        QCOMPARE(third.rval, 0);
        QCOMPARE(third.centerHz, 14200000.0);
        QCOMPARE(third.widthHz, 500.0);
        QCOMPARE(third.active, 0);
#endif
    }

    void add_notch_past_the_end_is_rejected_without_mutating()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        QVERIFY(ch->addNotch(0, Notch{1, 7040000.0, 200.0, true}));

        // nbp.c:368 guards on `notch <= b->nn`, so index 1 is legal (append)
        // and index 2 is not. The -1 must reach the caller, not be swallowed.
        QVERIFY(!ch->addNotch(2, Notch{2, 7050000.0, 200.0, true}));
        QCOMPARE(ch->notchCount(), 1);
#endif
    }

    // -- 6.2: edit --------------------------------------------------------

    void edit_notch_rewrites_only_that_index()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        QVERIFY(ch->addNotch(0, Notch{1, 14074000.0, 200.0, true}));
        QVERIFY(ch->addNotch(1, Notch{2, 14100000.0, 200.0, true}));

        QVERIFY(ch->editNotch(1, Notch{2, 14101234.0, 350.0, false}));

        const RawNotch edited = readRawNotch(kNotchTestChannel, 1);
        QCOMPARE(edited.centerHz, 14101234.0);
        QCOMPARE(edited.widthHz, 350.0);
        QCOMPARE(edited.active, 0);

        const RawNotch untouched = readRawNotch(kNotchTestChannel, 0);
        QCOMPARE(untouched.centerHz, 14074000.0);
        QCOMPARE(untouched.widthHz, 200.0);
        QCOMPARE(untouched.active, 1);
        QCOMPARE(ch->notchCount(), 2);
#endif
    }

    void edit_notch_past_the_end_is_rejected()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        QVERIFY(ch->addNotch(0, Notch{1, 3573000.0, 200.0, true}));
        QVERIFY(!ch->editNotch(1, Notch{2, 3574000.0, 200.0, true}));
        QCOMPARE(ch->notchCount(), 1);
#endif
    }

    // -- 6.2 / 5.2: delete keeps position == WDSP index -------------------

    void delete_notch_shifts_later_notches_down()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        QVERIFY(ch->addNotch(0, Notch{1, 14074000.0, 200.0, true}));
        QVERIFY(ch->addNotch(1, Notch{2, 14100000.0, 200.0, true}));
        QVERIFY(ch->addNotch(2, Notch{3, 14200000.0, 200.0, true}));

        // Delete from the middle: WDSP shifts its array down (nbp.c:426-434),
        // so index 1 must now be what used to be index 2.
        QVERIFY(ch->deleteNotch(1));

        QCOMPARE(ch->notchCount(), 2);
        QCOMPARE(readRawNotch(kNotchTestChannel, 0).centerHz, 14074000.0);
        QCOMPARE(readRawNotch(kNotchTestChannel, 1).centerHz, 14200000.0);
        QCOMPARE(readRawNotch(kNotchTestChannel, 2).rval, -1);
#endif
    }

    void delete_notch_past_the_end_is_rejected()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        QVERIFY(!ch->deleteNotch(0));
        QCOMPARE(ch->notchCount(), 0);
#endif
    }

    // -- 6.2: full rebuild ------------------------------------------------

    void sync_notches_replaces_the_whole_set_in_list_order()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        // Stale set the channel is already carrying.
        QVERIFY(ch->addNotch(0, Notch{9, 1810000.0, 200.0, true}));
        QVERIFY(ch->addNotch(1, Notch{8, 1820000.0, 200.0, true}));

        const QList<Notch> wanted = {
            Notch{1, 7040000.0,  200.0, true},
            Notch{2, 7074000.0,  100.0, false},
            Notch{3, 7100000.0, 1000.0, true},
        };
        ch->syncNotches(wanted);

        QCOMPARE(ch->notchCount(), static_cast<int>(wanted.size()));
        for (int i = 0; i < wanted.size(); ++i) {
            const RawNotch got = readRawNotch(kNotchTestChannel, i);
            QCOMPARE(got.rval, 0);
            QCOMPARE(got.centerHz, wanted.at(i).centerHz);
            QCOMPARE(got.widthHz, wanted.at(i).widthHz);
            QCOMPARE(got.active, wanted.at(i).active ? 1 : 0);
        }
#endif
    }

    void sync_notches_with_an_empty_list_clears_the_channel()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        QVERIFY(ch->addNotch(0, Notch{1, 10120000.0, 200.0, true}));
        QVERIFY(ch->addNotch(1, Notch{2, 10130000.0, 200.0, true}));

        // NotchModel::clear() lands here via notchesReset(); a sync that did
        // not erase would leave the channel notching while the model shows
        // nothing (design doc section 5.3, clear() contract).
        ch->syncNotches({});

        QCOMPARE(ch->notchCount(), 0);
        QCOMPARE(readRawNotch(kNotchTestChannel, 0).rval, -1);
#endif
    }

    // -- 6.3: run flag and auto-increase carries --------------------------

    void notches_run_defaults_off_and_round_trips()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        // create_notchdb is called with master_run = 0
        // (third_party/wdsp/src/RXA.c:87), so a freshly opened channel is
        // notch-inert until told otherwise.
        QVERIFY(!ch->notchesRun());

        ch->setNotchesRun(true);
        QVERIFY(ch->notchesRun());

        ch->setNotchesRun(false);
        QVERIFY(!ch->notchesRun());
#endif
    }

    void auto_increase_defaults_to_the_wdsp_construction_value()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        // create_nbp is called with autoincr = 1
        // (third_party/wdsp/src/RXA.c:105), and Thetis ships
        // chkMNFAutoIncrease.Checked = true, so a false carry would lie
        // about a freshly opened channel and would silently disable the
        // feature on every channel the fan-out reconciles.
        QVERIFY(ch->notchAutoIncrease());
#endif
    }

    void auto_increase_round_trips()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        ch->setNotchAutoIncrease(false);
        QVERIFY(!ch->notchAutoIncrease());

        ch->setNotchAutoIncrease(true);
        QVERIFY(ch->notchAutoIncrease());
#endif
    }

    // -- 9: minimum realisable notch width --------------------------------

    void min_notch_width_matches_the_wdsp_formula()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        // min_notch_width (third_party/wdsp/src/nbp.c:82-95) for wintype 0 is
        //   1600.0 / (nc / 256) * (rate / 48000)
        // create_nbp gives this channel wintype 0 (RXA.c:103),
        // nc = max(2048, dsp_size) = 4096 (RXA.c:96) and rate = dsp_rate
        // = 48000 (RXA.c:102), so 1600.0 / 16 * 1 = 100.0 Hz.
        QCOMPARE(ch->minNotchWidthHz(), 100.0);
#endif
    }

    // Regression: the three notch READBACKS must be inert on a channel id
    // outside WDSP's [0, MAX_CHANNELS) range, because each dereferences a
    // nested pointer inside rxa[] (RXANBPGetNumNotches and RXANBPGetNotch
    // reach ndb.p, RXANBPGetMinNotchWidth reaches nbp0.p) and a plain WDSP
    // setter only scribbles where a read segfaults.
    //
    // This is not hypothetical. minNotchWidthHz() was called unguarded from
    // the tail of setSampleRate() and setFilterSizeSamples(), which
    // tst_set_sample_rate_live drives on kTestChannel = 99, and the whole
    // suite went from green to SIGSEGV. Same guard NbFamily already carries
    // (NbFamily.h:269-275, after Linux CI #238).
    void notch_readbacks_are_inert_outside_the_wdsp_channel_range()
    {
        RxChannel ch(kOutOfRangeChannel, 2048, 48000);  // never opened
        QVERIFY(kOutOfRangeChannel >= 32);              // outside rxa[]

        QCOMPARE(ch.notchCount(), 0);
        QCOMPARE(ch.minNotchWidthHz(), 0.0);

        Notch out;
        QVERIFY(!ch.notchAt(0, out));
    }

    // setSampleRate carries one of those readbacks, so it must survive an
    // out-of-range channel. This is the exact path that segfaulted:
    // tst_set_sample_rate_live drives it on kTestChannel = 99.
    //
    // setFilterSizeSamples carries the same emit but is deliberately NOT
    // exercised here. It also calls RXASetNC and SetDSPBuffsize, which write
    // into rxa[99] and crash independently of anything TNF added, so it has
    // never been safe on an unopened channel and no existing suite drives it
    // that way. Guarding those two would be a separate change with its own
    // justification, not something to smuggle in under a notch fix.
    void rate_changes_survive_an_unopened_channel()
    {
        RxChannel ch(kOutOfRangeChannel, 2048, 48000);
        ch.setSampleRate(96000);
        QCOMPARE(ch.sampleRate(), 96000);
    }
};

QTEST_MAIN(TestRxChannelNotchWrappers)
#include "tst_rxchannel_notch_wrappers.moc"
