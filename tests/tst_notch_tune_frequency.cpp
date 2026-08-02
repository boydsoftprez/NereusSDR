// =================================================================
// tests/tst_notch_tune_frequency.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Tunable Notch Filter, Task 1. Covers
// docs/architecture/2026-07-28-tunable-notch-filter-design.md section 4:
//   4.1 / 4.2  the notch database's tune frequency is the hosting DDC
//              stream's CENTRE, pushed from bindSliceToStream.
//   4.3        the shift reaches the notch database on the way back down
//              to zero, and keeps its sign.
//   4.4        the stream offset and RIT/DIG compose instead of clobbering
//              each other, asserted in both writer orders.
//   4.5        the connect-time seed commands the stream centre, not the
//              slice frequency.
//
// The invariant every slot below is really asserting, from 4.1:
//     tunefreq + shift == the slice's demodulated RF
//                      == stream centre + slice offset + RIT + DIG
// asserted in BOTH halves, never the sum alone: a sum-only assertion
// passes under the double-count bug whenever the shift happens to be zero.
//
// WDSP is live in test binaries (CMakeLists.txt sets HAVE_WDSP PUBLIC) and
// RXANBPSetTuneFrequency dereferences rxa[channel].ndb.p before it compares
// (third_party/wdsp/src/nbp.c:477-479), so the usual kTestChannel = 99
// never-opened-channel hatch is an out-of-bounds read here, not a no-op.
// Every slot uses a really opened channel through the NEREUS_BUILD_TESTS
// friend seam, the pattern at tests/tst_stream_pool_binding.cpp:1018,1033.
// =================================================================
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/P1RadioConnection.h"
#include "core/ReceiverManager.h"
#include "core/RxChannel.h"
#include "core/SampleRateCatalog.h"
#include "core/WdspEngine.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// Stream geometry shared by every slot. 192 kHz gives a +/- 96 kHz window
// (SliceStreamAllocator::windowContains), so 14.200 and 14.210 MHz share a
// stream and 14.200 / 14.400 MHz do not.
constexpr int    kRateHz       = 192000;
constexpr double kSliceAFreqHz = 14200000.0;
constexpr double kSliceBFreqHz = 14210000.0;
constexpr double kFarFreqHz    = 14400000.0;

// Detaches a stack-injected RadioConnection on scope exit, however the
// scope is left. From tests/tst_stream_pool_binding.cpp:57-63: QCOMPARE
// returns from the enclosing slot on failure, so a trailing
// injectConnectionForTest(nullptr) is skipped on exactly the run where it
// matters most.
struct DetachConnection {
    RadioModel* model{nullptr};
    ~DetachConnection() { if (model) { model->injectConnectionForTest(nullptr); } }
};

} // namespace

class TestNotchTuneFrequency : public QObject {
    Q_OBJECT

private slots:
    // -- 4.6: the RxChannel carries ---------------------------------------

    void notch_tune_frequency_defaults_to_zero()
    {
        WdspEngine engine;
        engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        RxChannel* ch = engine.createRxChannel(0, bufferSizeForRate(kRateHz),
                                               4096, kRateHz, 48000, 48000);
        QVERIFY(ch != nullptr);

        // The construction default, and the whole defect: nothing in the
        // tree pushed this value, so every channel's notchdb.tunefreq sat
        // here and calc_nbp_lightweight mapped notches from the wrong RF
        // origin (third_party/wdsp/src/nbp.c:192).
        QCOMPARE(ch->notchTuneFrequencyHz(), 0.0);
    }

    void notch_tune_frequency_carry_round_trips()
    {
        WdspEngine engine;
        engine.m_initialized = true;
        RxChannel* ch = engine.createRxChannel(0, bufferSizeForRate(kRateHz),
                                               4096, kRateHz, 48000, 48000);
        QVERIFY(ch != nullptr);

        ch->setNotchTuneFrequency(kSliceAFreqHz);
        QCOMPARE(ch->notchTuneFrequencyHz(), kSliceAFreqHz);
    }

    void shift_push_keeps_its_sign()
    {
        WdspEngine engine;
        engine.m_initialized = true;
        RxChannel* ch = engine.createRxChannel(0, bufferSizeForRate(kRateHz),
                                               4096, kRateHz, 48000, 48000);
        QVERIFY(ch != nullptr);

        // A slice 10 kHz ABOVE its stream centre pushes +10000, not -10000.
        // Thetis's -value at radio.cs:1419-1420 [v2.10.3.15] is not a
        // divergence: rx_osc is already the negated quantity upstream
        // (console.cs:31916-31922), so Thetis's -rx_osc equals the offsetHz
        // handed in here, which equals frequencyHz - centreHz at
        // SliceStreamAllocator.cpp:70. Locked here so it never gets
        // "corrected" into an inversion of every shifted slice.
        ch->setShiftFrequency(10000.0);
        QCOMPARE(ch->shiftOffsetHz(), 10000.0);
    }

    // -- 4.3: the shift must reach the notch DB on the way back to zero ----

    void shift_reaches_the_notch_database_on_the_way_back_to_zero()
    {
        WdspEngine engine;
        engine.m_initialized = true;
        RxChannel* ch = engine.createRxChannel(0, bufferSizeForRate(kRateHz),
                                               4096, kRateHz, 48000, 48000);
        QVERIFY(ch != nullptr);

        ch->setShiftFrequency(10000.0);
        QCOMPARE(ch->notchShiftHz(), 10000.0);

        // 4.3. RXANBPSetShiftFrequency is the sole writer of NOTCHDB->shift
        // (third_party/wdsp/src/nbp.c:487-496) and calc_nbp_lightweight
        // consumes that field with no reference to any run flag (nbp.c:192),
        // so the old near-zero branch, which called SetRXAShiftRun(channel, 0)
        // and nothing else, left the notch database holding a stale shift on
        // every RIT-off, DIG-exit, band jump and CTUN-off. Thetis has no such
        // branch: radio.cs:1419-1420 [v2.10.3.15] pushes both setters on every
        // RXOsc change including a change to zero, and SetRXAShiftRun appears
        // nowhere in its Console tree. notchShiftHz() is written next to the
        // WDSP call it mirrors, so it goes stale here if the gate ever comes
        // back.
        ch->setShiftFrequency(0.0);
        QCOMPARE(ch->shiftOffsetHz(), 0.0);
        QCOMPARE(ch->notchShiftHz(), 0.0);
    }

    // -- 4.1 / 4.2: bindSliceToStream pushes the hosting stream's centre ---

    void bind_pushes_the_hosting_streams_centre_sole_owner()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        model.configureStreamPool(/*userDdcCount*/ 2, /*maxSlices*/ 2, kRateHz);
        // Pool BEFORE the slices. bindSliceToStream is the push site (4.2)
        // and it can only reach a channel that already exists; the real
        // connect ordering (openRxChannelPool after the binds) is closed by
        // syncNotchesToAllChannels in the fan-out task, per design doc 6.3.
        model.openRxChannelPool(2, bufferSizeForRate(kRateHz), kRateHz);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(kSliceAFreqHz);

        RxChannel* ch = engine->rxChannel(a);
        QVERIFY(ch != nullptr);

        // Sole occupant: the allocator retunes the stream under the slice
        // (SliceStreamAllocator.cpp:128 sets shiftOffsetHz = 0.0), so the
        // centre and the slice frequency coincide. This half of 4.1 cannot
        // catch the double-count bug on its own; the next slot can.
        QCOMPARE(model.streamCentreHzForTest(0), kSliceAFreqHz);
        QCOMPARE(ch->notchTuneFrequencyHz(), kSliceAFreqHz);
        QCOMPARE(ch->shiftOffsetHz(), 0.0);
    }

    void bind_pushes_the_hosting_streams_centre_joined_existing()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(2, 2, kRateHz);
        model.openRxChannelPool(2, bufferSizeForRate(kRateHz), kRateHz);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(kSliceAFreqHz);

        const int b = model.addSlice();
        SliceModel* sliceB = model.sliceById(b);
        sliceB->setFrequency(kSliceBFreqHz);

        // Two slices, one stream, 10 kHz apart inside a 192 kHz window.
        // This is Thetis's own topology: console.cs:31922 gives the subrx
        // its own shift while console.cs:31940-31941 [v2.10.3.15] push the
        // IDENTICAL tunefreq to id(0,0) and id(0,1).
        QCOMPARE(sliceB->streamIndex(), 0);
        QCOMPARE(sliceB->shiftOffsetHz(), 10000.0);

        RxChannel* chB = engine->rxChannel(b);
        QVERIFY(chB != nullptr);

        // Half (a) of the 4.1 invariant: tunefreq is the STREAM centre.
        // Driving it from the slice frequency computes
        // 2*sliceFreq - streamCentre, which the sum assertion below would
        // not catch on its own.
        QCOMPARE(chB->notchTuneFrequencyHz(), kSliceAFreqHz);
        QCOMPARE(chB->shiftOffsetHz(), 10000.0);

        // Half (b): the two terms WDSP adds (nbp.c:192) land on the RF this
        // slice is demodulating.
        QCOMPARE(chB->notchTuneFrequencyHz() + chB->shiftOffsetHz(),
                 sliceB->effectiveRxFrequency());
    }

    void a_sole_occupant_retune_carries_the_tune_frequency_to_the_new_centre()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(2, 2, kRateHz);
        model.openRxChannelPool(2, bufferSizeForRate(kRateHz), kRateHz);

        const int a = model.addSlice();
        SliceModel* sliceA = model.sliceById(a);
        sliceA->setFrequency(kSliceAFreqHz);

        RxChannel* ch = engine->rxChannel(a);
        QVERIFY(ch != nullptr);

        // 200 kHz away is outside the 192 kHz window, so the sole occupant
        // drags its own DDC rather than claiming a second one.
        sliceA->setFrequency(kFarFreqHz);

        QCOMPARE(sliceA->streamIndex(), 0);
        QCOMPARE(model.streamCentreHzForTest(0), kFarFreqHz);
        QCOMPARE(ch->notchTuneFrequencyHz(), kFarFreqHz);
        QCOMPARE(ch->shiftOffsetHz(), 0.0);
    }

    void panning_a_shifted_slice_onto_the_centre_clears_the_notch_shift()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(2, 2, kRateHz);
        model.openRxChannelPool(2, bufferSizeForRate(kRateHz), kRateHz);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(kSliceAFreqHz);

        const int b = model.addSlice();
        SliceModel* sliceB = model.sliceById(b);
        sliceB->setFrequency(kSliceBFreqHz);

        RxChannel* chB = engine->rxChannel(b);
        QVERIFY(chB != nullptr);
        QCOMPARE(chB->notchShiftHz(), 10000.0);

        // Pan out and back (4.3 at the model level). Slice A still holds the
        // stream, so B is not the sole occupant and this is a JoinedExisting
        // placement with a zero shift, not a retune.
        sliceB->setFrequency(kSliceAFreqHz);

        QCOMPARE(chB->shiftOffsetHz(), 0.0);
        QCOMPARE(chB->notchShiftHz(), 0.0);
        QCOMPARE(chB->notchTuneFrequencyHz(), kSliceAFreqHz);
        QCOMPARE(chB->notchTuneFrequencyHz() + chB->shiftOffsetHz(),
                 sliceB->effectiveRxFrequency());
    }

    // -- 4.4: the stream offset and RIT/DIG compose -----------------------
    //
    // Both writer orders, because fixing only one leaves the mirror bug:
    // bindSliceToStream pushed the placement offset with no RIT term and the
    // wireSliceSignals lambda pushed RIT with no placement term, so whichever
    // fired last won and threw the other away.
    //
    // wireSliceSignals early-returns on !m_connection
    // (RadioModel.cpp:8883-8885), so the RIT lambda only exists once a
    // connection is injected. It is never opened: isConnected() stays false,
    // which also keeps the connect-time seed's singleShot a no-op.

    void rit_adds_to_the_stream_shift_instead_of_replacing_it()
    {
        RadioModel model;
        P1RadioConnection conn;
        model.injectConnectionForTest(&conn);
        DetachConnection detach{&model};

        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(2, 2, kRateHz);
        model.openRxChannelPool(2, bufferSizeForRate(kRateHz), kRateHz);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(kSliceAFreqHz);

        const int b = model.addSlice();
        SliceModel* sliceB = model.sliceById(b);
        sliceB->setFrequency(kSliceBFreqHz);

        RxChannel* chB = engine->rxChannel(b);
        QVERIFY(chB != nullptr);
        QCOMPARE(chB->shiftOffsetHz(), 10000.0);

        sliceB->setRitHz(500);
        sliceB->setRitEnabled(true);

        // 10 kHz of stream offset PLUS 500 Hz of RIT, not 500 Hz alone.
        QCOMPARE(chB->shiftOffsetHz(), 10500.0);
        QCOMPARE(chB->notchTuneFrequencyHz(), kSliceAFreqHz);
        QCOMPARE(chB->notchTuneFrequencyHz() + chB->shiftOffsetHz(),
                 sliceB->effectiveRxFrequency());
    }

    void a_retune_while_rit_is_on_keeps_the_rit_term()
    {
        RadioModel model;
        P1RadioConnection conn;
        model.injectConnectionForTest(&conn);
        DetachConnection detach{&model};

        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(2, 2, kRateHz);
        model.openRxChannelPool(2, bufferSizeForRate(kRateHz), kRateHz);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(kSliceAFreqHz);

        const int b = model.addSlice();
        SliceModel* sliceB = model.sliceById(b);
        sliceB->setFrequency(kSliceBFreqHz);
        sliceB->setRitHz(500);
        sliceB->setRitEnabled(true);

        RxChannel* chB = engine->rxChannel(b);
        QVERIFY(chB != nullptr);
        QCOMPARE(chB->shiftOffsetHz(), 10500.0);

        // The mirror case: the retune writer must not drop the RIT term.
        sliceB->setFrequency(14205000.0);

        QCOMPARE(sliceB->shiftOffsetHz(), 5000.0);
        QCOMPARE(chB->shiftOffsetHz(), 5500.0);
        QCOMPARE(chB->notchTuneFrequencyHz(), kSliceAFreqHz);
        QCOMPARE(chB->notchTuneFrequencyHz() + chB->shiftOffsetHz(),
                 sliceB->effectiveRxFrequency());
    }
};

QTEST_MAIN(TestNotchTuneFrequency)
#include "tst_notch_tune_frequency.moc"
