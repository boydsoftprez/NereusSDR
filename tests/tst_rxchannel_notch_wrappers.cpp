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
};

QTEST_MAIN(TestRxChannelNotchWrappers)
#include "tst_rxchannel_notch_wrappers.moc"
